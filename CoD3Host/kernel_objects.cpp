// Xbox 360 kernel imports: handles, synchronisation objects, and networking.
//
// Two different kinds of object live here. Nt* calls work on handles from a
// table this file owns. Ke* calls work on dispatcher objects the title keeps
// in its own memory, so those read and write a guest structure directly.
//
// Guest threads are real host threads, so all of this is shared state. One
// lock covers the handle table and every signal state; waiters block on one
// condition variable and re-check their own condition when anything changes.

#include "kernel.h"
#include "scheduler.h"
#include "timeline.h"
#include "debug_probe.h"
#include <atomic>
#include "gpu.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <string>
#include <algorithm>
#include <unordered_map>
#include <vector>

#include <Windows.h>
#include <immintrin.h>

namespace
{
    constexpr uint32_t X_STATUS_SUCCESS            = 0x00000000;
    constexpr uint32_t X_STATUS_TIMEOUT            = 0x00000102;
    constexpr uint32_t X_STATUS_INVALID_HANDLE     = 0xC0000008;
    constexpr uint32_t X_STATUS_INVALID_PARAMETER  = 0xC000000D;

    // Offsets inside the dispatcher header the title embeds in its own events,
    // semaphores and mutants. The type is the high byte of the first word,
    // because the guest is big endian.
    constexpr uint32_t DISPATCH_TYPE         = 0x00;
    constexpr uint32_t DISPATCH_SIGNAL_STATE = 0x04;

    constexpr uint32_t DispatchNotificationEvent   = 0;
    constexpr uint32_t DispatchSynchronizationEvent = 1;
    constexpr uint32_t DispatchMutant              = 2;
    constexpr uint32_t DispatchSemaphore           = 5;

    uint32_t DispatchType(const uint8_t* base, uint32_t object)
    {
        return (Guest::Read32(base, object + DISPATCH_TYPE) >> 24) & 0xFF;
    }

    // A wait consumes the signal of a synchronisation event, a mutant or a
    // semaphore. A notification event stays set until it is explicitly reset,
    // which is the whole point of it: every thread waiting on one is released,
    // not just the first. Clearing it here would strand the rest forever.
    bool WaitConsumes(uint32_t type)
    {
        return type == DispatchSynchronizationEvent ||
               type == DispatchMutant ||
               type == DispatchSemaphore;
    }

    enum class ObjectType { Event, Semaphore, Mutant, File, Thread, Other };

    struct Object
    {
        ObjectType type = ObjectType::Other;
        bool manualReset = false;
        int32_t signalled = 0;
        int32_t limit = 0;         // semaphore maximum
        std::string name;

        // A mutant is recursive: the thread that holds it can acquire it again
        // without blocking, and only the matching number of releases frees it.
        // Treating one as a plain count deadlocks the first thread that takes
        // it twice, which is what happened here.
        uint32_t owner = 0;
        int32_t recursion = 0;

        // How many times this object has been pulsed.
        //
        // A pulse releases every thread waiting at that moment and then leaves
        // the object clear. A signal state cannot say that: setting it to one
        // releases a single waiter, and setting it and clearing it releases
        // none. This runtime did the second, which is why four worker threads
        // sat on their job queue with no timeout while the two pulses meant to
        // start them went nowhere. A waiter records this counter when it starts
        // and is released as soon as it changes.
        uint32_t pulses = 0;

        // Where this object lives in guest memory.
        //
        // The title can turn a handle into a pointer with
        // ObReferenceObjectByHandle and then wait on the pointer with the Ke
        // family, which reads the signal state straight out of guest memory.
        // Handing back the handle value as if it were a pointer, which is what
        // this used to do, sent those waits to read address 0x104 and similar:
        // always zero, never signalled, and the thread waiting for its data to
        // load never woke up.
        uint32_t guestAddress = 0;
    };

    // The dispatcher header is sixteen bytes: a type and size, then the signal
    // state, then a list head this runtime does not model.
    constexpr uint32_t ObjectBodySize = 32;

    int32_t SignalState(Object& object)
    {
        if (object.guestAddress != 0)
            object.signalled = int32_t(
                Guest::Read32(Guest::Base, object.guestAddress + DISPATCH_SIGNAL_STATE));
        return object.signalled;
    }

    void SetSignalState(Object& object, int32_t value)
    {
        object.signalled = value;
        if (object.guestAddress != 0)
            Guest::Write32(Guest::Base, object.guestAddress + DISPATCH_SIGNAL_STATE,
                uint32_t(value));
    }

    void GiveBody(Object& object)
    {
        object.guestAddress = Guest::AllocateKernelObject(ObjectBodySize);
        if (object.guestAddress == 0) return;
        // Type in the low byte, so guest code that inspects it sees something
        // consistent. Event, mutant and semaphore use the values the console
        // does.
        const uint32_t type = object.type == ObjectType::Mutant ? 2u
                            : object.type == ObjectType::Semaphore ? 5u
                            : (object.manualReset ? 0u : 1u);
        Guest::Write32(Guest::Base, object.guestAddress, type << 24);
        Guest::Write32(Guest::Base, object.guestAddress + DISPATCH_SIGNAL_STATE,
            uint32_t(object.signalled));
    }

    std::map<uint32_t, Object>& Handles()
    {
        static std::map<uint32_t, Object> handles;
        return handles;
    }

    uint32_t g_nextHandle = 0x00000100;

    // Which handle is which matters when one of them is never signalled: the
    // kind of object and where it was created point straight at the code that
    // was supposed to signal it.
    uint32_t CreateHandle(Object object, uint32_t from)
    {
        const uint32_t handle = g_nextHandle;
        g_nextHandle += 4;

        const char* kind = "object";
        switch (object.type)
        {
        case ObjectType::Event:     kind = object.manualReset ? "notification event"
                                                             : "synchronisation event"; break;
        case ObjectType::Semaphore: kind = "semaphore"; break;
        case ObjectType::Mutant:    kind = "mutant"; break;
        default: break;
        }

        printf("object: handle 0x%08X is a %s, initially %d, created from 0x%08X\n",
            handle, kind, object.signalled, from);
        fflush(stdout);

        Handles()[handle] = std::move(object);
        return handle;
    }

    // The handles last found, in front of the map (under DispatcherLock):
    // the job lock's handle is looked up two million times a second. The
    // map's entries never move; an entry erased is dropped here too.
    struct Found { uint32_t handle = 0; Object* object = nullptr; };
    Found g_found[256];
    Found& FoundSlot(uint32_t handle) { return g_found[(handle >> 2) & 255]; }

    Object* Find(uint32_t handle)
    {
        Found& slot = FoundSlot(handle);
        if (slot.handle == handle && slot.object != nullptr) return slot.object;
        auto found = Handles().find(handle);
        if (found == Handles().end()) return nullptr;
        slot = Found{ handle, &found->second };
        return slot.object;
    }

    // An object that is waited on and never signalled is what a stalled boot
    // looks like from here, so both sides are counted and compared.
    std::mutex g_trafficMutex;
    std::map<uint32_t, uint64_t> g_waits;
    std::map<uint32_t, uint64_t> g_signals;

    // Totals say what is never signalled; the order says why. The first few
    // dozen events are kept so the sequence that leads into the deadlock can be
    // read back rather than guessed at.
    struct Event
    {
        uint32_t thread;
        const char* action;
        uint32_t object;
        uint32_t from;
    };
    constexpr size_t EventLimit = 64;
    Event g_events[EventLimit];

    // A ring, not a prefix: the interesting events are the last ones before
    // everything stopped, not the first ones after it started. Written
    // without a lock (a slot each, by a counter): an event read while it is
    // written is a report's detail, and a lock here was taken two million
    // times a second.
    std::atomic<uint64_t> g_eventNext{ 0 };

    void Record(const char* action, uint32_t object, uint32_t from)
    {
        const uint64_t at = g_eventNext.fetch_add(1, std::memory_order_relaxed);
        g_events[at % EventLimit] = { GetCurrentThreadId(), action, object, from };
    }

    // What each thread is blocked on right now, as opposed to what it has ever
    // waited on. This is the state that matters when everything has stopped.
    struct Blocked { uint32_t object; uint32_t from; bool infinite; };
    std::map<uint32_t, Blocked> g_blocked;

    void EnterWait(uint32_t object, uint32_t from, bool infinite)
    {
        std::lock_guard<std::mutex> lock(g_trafficMutex);
        g_blocked[GetCurrentThreadId()] = { object, from, infinite };
    }

    void LeaveWait()
    {
        std::lock_guard<std::mutex> lock(g_trafficMutex);
        g_blocked.erase(GetCurrentThreadId());
    }

    void CountWait(uint32_t object, uint32_t from)
    {
        {
            std::lock_guard<std::mutex> lock(g_trafficMutex);
            g_waits[object]++;
        }
        Record("wait  ", object, from);
    }

    // Signals are counted by each thread and added to the totals now and
    // then: at once for an object the thread has not signalled before (so
    // the totals know every object signalled), else every few thousand.
    void CountSignal(uint32_t object, uint32_t from)
    {
        struct Counts { std::unordered_map<uint32_t, uint64_t> signals; uint32_t pending = 0; };
        thread_local Counts counts;
        auto [entry, first] = counts.signals.try_emplace(object, 0);
        entry->second++;
        if (first || ++counts.pending >= 4096)
        {
            std::lock_guard<std::mutex> lock(g_trafficMutex);
            for (auto& [signalled, count] : counts.signals)
            {
                g_signals[signalled] += count;
                count = 0;
            }
            counts.pending = 0;
        }
        Record("signal", object, from);
    }

    // A wait that never completes is the usual way a boot stalls, and it is
    // invisible from outside. Reporting one after a few seconds names what the
    // title is blocked on.
    void ReportStalledWait(const char* what, uint32_t object, uint32_t from, int64_t state = -1)
    {
        static std::mutex reported;
        static std::map<uint32_t, bool> seen;
        std::lock_guard<std::mutex> lock(reported);
        if (seen[from]) return;
        seen[from] = true;
        printf("stalled: waiting on %s 0x%08X, called from 0x%08X%s\n", what, object, from,
            state < 0 ? "" : state > 0 ? " (signalled now: a wake was lost)" : " (not signalled)");
        fflush(stdout);
    }

    // Guest timeouts are 100 nanosecond units; negative means relative.
    // Returns false when the caller asked not to wait at all.
    bool TimeoutToDeadline(const uint8_t* base, uint32_t timeoutPtr,
                           std::chrono::steady_clock::time_point& deadline,
                           bool& infinite)
    {
        infinite = (timeoutPtr == 0);
        if (infinite) return true;

        const int64_t timeout = static_cast<int64_t>(Guest::Read64(base, timeoutPtr));
        if (timeout == 0) return false;                  // poll

        const int64_t hundredNanoseconds = timeout < 0 ? -timeout : timeout;
        deadline = std::chrono::steady_clock::now() +
                   std::chrono::nanoseconds(hundredNanoseconds * 100);
        return true;
    }
}

std::mutex& Kernel::DispatcherLock()
{
    static std::mutex lock;
    return lock;
}

// The threads asleep on the dispatcher, each on a condition of its own,
// with what it waits for (under DispatcherLock). A change wakes only those
// waiting for what changed: the title releases one mutant fifty thousand
// times a second, and when every release woke every sleeping thread in the
// process the title's main thread spent a third of its time waking them,
// and they theirs waking up to sleep again.
namespace
{
    struct Sleeper
    {
        std::condition_variable changed;
        const uint64_t* keys = nullptr;   // none: woken by anything
        uint32_t count = 0;
    };
    std::vector<Sleeper*> g_sleepers;
    std::atomic<int> g_dispatcherSleepers{ 0 };

    std::cv_status Sleep(std::unique_lock<std::mutex>& lock, const uint64_t* keys, uint32_t count,
                         std::chrono::steady_clock::time_point deadline)
    {
        thread_local Sleeper self;
        self.keys = keys;
        self.count = count;
        g_sleepers.push_back(&self);
        g_dispatcherSleepers.fetch_add(1, std::memory_order_acq_rel);
        const std::cv_status status = self.changed.wait_until(lock, deadline);
        g_dispatcherSleepers.fetch_sub(1, std::memory_order_acq_rel);
        for (size_t i = 0; i < g_sleepers.size(); i++)
            if (g_sleepers[i] == &self) { g_sleepers[i] = g_sleepers.back(); g_sleepers.pop_back(); break; }
        return status;
    }

    // A handle's keys: the handle, and its body in guest memory if it has one
    // (a Ke wait on the body waits for the same object).
    uint32_t HandleKeys(uint32_t handle, uint32_t guestAddress, uint64_t keys[2])
    {
        keys[0] = (1ull << 32) | handle;
        if (guestAddress == 0) return 1;
        keys[1] = guestAddress;
        return 2;
    }
}

std::cv_status Kernel::WaitDispatcher(std::unique_lock<std::mutex>& lock, std::chrono::milliseconds limit)
{
    return Sleep(lock, nullptr, 0, std::chrono::steady_clock::now() + limit);
}

std::cv_status Kernel::WaitDispatcherUntil(std::unique_lock<std::mutex>& lock, std::chrono::steady_clock::time_point deadline)
{
    return Sleep(lock, nullptr, 0, deadline);
}

std::cv_status Kernel::WaitDispatcherOn(std::unique_lock<std::mutex>& lock, const uint64_t* keys, uint32_t count,
                                        std::chrono::steady_clock::time_point deadline)
{
    return Sleep(lock, keys, count, deadline);
}

void Kernel::WakeDispatcher()
{
    for (Sleeper* sleeper : g_sleepers) sleeper->changed.notify_one();
}

void Kernel::WakeDispatcherFor(const uint64_t* keys, uint32_t count)
{
    for (Sleeper* sleeper : g_sleepers)
    {
        bool wanted = sleeper->count == 0;
        for (uint32_t i = 0; i < sleeper->count && !wanted; i++)
            for (uint32_t j = 0; j < count && !wanted; j++)
                wanted = sleeper->keys[i] == keys[j];
        if (wanted) sleeper->changed.notify_one();
    }
}

void Kernel::WakeDispatcherForUnlocked(uint64_t key)
{
    if (g_dispatcherSleepers.load(std::memory_order_acquire) == 0) return;
    std::lock_guard<std::mutex> lock(DispatcherLock());
    WakeDispatcherFor(&key, 1);
}

namespace
{
    // At the end of a change to a handle's object: its waiters woken.
    struct WakeHandle
    {
        uint32_t handle;
        ~WakeHandle();
    };
}

// --- Handles ---------------------------------------------------------------

WakeHandle::~WakeHandle()
{
    uint64_t keys[2];
    const Object* object = Find(handle);
    Kernel::WakeDispatcherFor(keys, HandleKeys(handle, object != nullptr ? object->guestAddress : 0, keys));
}

// NTSTATUS NtClose(HANDLE handle)
PPC_FUNC(__imp__NtClose)
{
    Kernel::CountImport("NtClose");
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { ~Wake() { Kernel::WakeDispatcher(); } } wake;

    auto found = Handles().find(ctx.r3.u32);
    if (found != Handles().end())
    {
        if (FoundSlot(found->first).handle == found->first) FoundSlot(found->first) = Found{};
        Handles().erase(found);
        ctx.r3.u32 = X_STATUS_SUCCESS;
        return;
    }

    // File handles come from a separate table.
    if (Kernel::CloseFileHandle(ctx.r3.u32))
    {
        ctx.r3.u32 = X_STATUS_SUCCESS;
        return;
    }

    ctx.r3.u32 = X_STATUS_INVALID_HANDLE;
}

// NTSTATUS ObReferenceObjectByHandle(HANDLE handle, void* type, void** out)
PPC_FUNC(__imp__ObReferenceObjectByHandle)
{
    Kernel::CountImport("ObReferenceObjectByHandle");
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { ~Wake() { Kernel::WakeDispatcher(); } } wake;

    const uint32_t handle = ctx.r3.u32;
    if (Find(handle) == nullptr)
    {
        // Thread handles live in their own table, and a title that wants to
        // resume or wait on a thread asks for its object this way.
        const uint32_t threadObject = Kernel::ThreadObjectFor(handle);
        if (threadObject != 0)
        {
            if (ctx.r5.u32 != 0) Guest::Write32(base, ctx.r5.u32, threadObject);
            ctx.r3.u32 = X_STATUS_SUCCESS;
            return;
        }

        printf("ObReferenceObjectByHandle: 0x%08X is not an object this runtime "
               "knows, from 0x%08X\n", handle, uint32_t(ctx.lr));
        fflush(stdout);
        ctx.r3.u32 = X_STATUS_INVALID_HANDLE;
        return;
    }
    Object* object = Find(handle);
    if (ctx.r5.u32 != 0)
        Guest::Write32(base, ctx.r5.u32,
            object->guestAddress != 0 ? object->guestAddress : handle);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// VOID ObDereferenceObject(void* object)
PPC_FUNC(__imp__ObDereferenceObject)
{
    Kernel::CountImport("ObDereferenceObject");
    // Reference counting is not modelled: handles live until NtClose.
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS ObCreateSymbolicLink(STRING* path, STRING* target)
PPC_FUNC(__imp__ObCreateSymbolicLink)
{
    Kernel::CountImport("ObCreateSymbolicLink");
    // Device links such as game: are resolved by the file layer instead.
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS ObDeleteSymbolicLink(STRING* path)
PPC_FUNC(__imp__ObDeleteSymbolicLink) { ctx.r3.u32 = X_STATUS_SUCCESS; }

// --- Events, by handle -----------------------------------------------------

// NTSTATUS NtCreateEvent(HANDLE* out, OBJECT_ATTRIBUTES* attr,
//                        ULONG type, BOOLEAN initialState)
PPC_FUNC(__imp__NtCreateEvent)
{
    Kernel::CountImport("NtCreateEvent");
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { ~Wake() { Kernel::WakeDispatcher(); } } wake;

    Object object;
    object.type = ObjectType::Event;
    object.manualReset = (ctx.r5.u32 == 0);   // 0 notification, 1 synchronisation
    object.signalled = ctx.r6.u32 != 0 ? 1 : 0;
    GiveBody(object);

    const uint32_t handle = CreateHandle(std::move(object), uint32_t(ctx.lr));
    if (ctx.r3.u32 != 0)
        Guest::Write32(base, ctx.r3.u32, handle);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtSetEvent(HANDLE handle, LONG* previousState)
PPC_FUNC(__imp__NtSetEvent)
{
    Kernel::CountImportOn("NtSetEvent", ctx.r3.u32);
    CountSignal(ctx.r3.u32, uint32_t(ctx.lr));
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    WakeHandle wake{ ctx.r3.u32 };

    Object* object = Find(ctx.r3.u32);
    if (object == nullptr) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }

    if (ctx.r4.u32 != 0)
        Guest::Write32(base, ctx.r4.u32, uint32_t(SignalState(*object)));
    SetSignalState(*object, 1);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtClearEvent(HANDLE handle)
PPC_FUNC(__imp__NtClearEvent)
{
    Kernel::CountImportOn("NtClearEvent", ctx.r3.u32);
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    WakeHandle wake{ ctx.r3.u32 };

    Object* object = Find(ctx.r3.u32);
    if (object == nullptr) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }
    SetSignalState(*object, 0);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtPulseEvent(HANDLE handle, LONG* previousState)
PPC_FUNC(__imp__NtPulseEvent)
{
    Kernel::CountImportOn("NtPulseEvent", ctx.r3.u32);
    CountSignal(ctx.r3.u32, uint32_t(ctx.lr));
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    WakeHandle wake{ ctx.r3.u32 };

    Object* object = Find(ctx.r3.u32);
    if (object == nullptr) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }

    if (ctx.r4.u32 != 0)
        Guest::Write32(base, ctx.r4.u32, uint32_t(SignalState(*object)));

    // Everyone waiting right now goes; the event is left clear for anyone who
    // arrives afterwards.
    object->pulses++;
    SetSignalState(*object, 0);

    // How many that was. A pulse that finds nobody waiting is lost, on the
    // console as here, and a job system that relies on one being caught is
    // exactly where a run goes quiet with its workers asleep.
    {
        int waiting = 0;
        {
            // Under its own lock: the waiters add and remove themselves
            // from other threads, and a walk over a map being changed
            // ended in a read of nowhere.
            std::lock_guard<std::mutex> traffic(g_trafficMutex);
            for (const auto& entry : g_blocked)
                if (entry.second.object == ctx.r3.u32) waiting++;
        }
        static std::atomic<int> announced{ 0 };
        if ((announced.load(std::memory_order_relaxed) < 120 && announced.fetch_add(1) < 120))
        {
            printf("pulse: 0x%08X from 0x%08X released %d waiter%s\n",
                ctx.r3.u32, uint32_t(ctx.lr), waiting, waiting == 1 ? "" : "s");
            fflush(stdout);
        }
    }
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// --- Semaphores and mutants, by handle -------------------------------------

// NTSTATUS NtCreateSemaphore(HANDLE* out, OBJECT_ATTRIBUTES*, LONG count, LONG limit)
PPC_FUNC(__imp__NtCreateSemaphore)
{
    Kernel::CountImport("NtCreateSemaphore");
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { ~Wake() { Kernel::WakeDispatcher(); } } wake;

    Object object;
    object.type = ObjectType::Semaphore;
    object.signalled = int32_t(ctx.r5.u32);
    object.limit = int32_t(ctx.r6.u32);
    GiveBody(object);

    const uint32_t handle = CreateHandle(std::move(object), uint32_t(ctx.lr));
    if (ctx.r3.u32 != 0)
        Guest::Write32(base, ctx.r3.u32, handle);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtReleaseSemaphore(HANDLE handle, LONG count, LONG* previous)
PPC_FUNC(__imp__NtReleaseSemaphore)
{
    Kernel::CountImportOn("NtReleaseSemaphore", ctx.r3.u32);
    CountSignal(ctx.r3.u32, uint32_t(ctx.lr));
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    WakeHandle wake{ ctx.r3.u32 };

    Object* object = Find(ctx.r3.u32);
    if (object == nullptr) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }

    if (ctx.r5.u32 != 0)
        Guest::Write32(base, ctx.r5.u32, uint32_t(SignalState(*object)));
    int32_t released = SignalState(*object) + int32_t(ctx.r4.u32);
    if (object->limit > 0 && released > object->limit) released = object->limit;
    SetSignalState(*object, released);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtCreateMutant(HANDLE* out, OBJECT_ATTRIBUTES*, BOOLEAN owned)
PPC_FUNC(__imp__NtCreateMutant)
{
    Kernel::CountImport("NtCreateMutant");
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { ~Wake() { Kernel::WakeDispatcher(); } } wake;

    Object object;
    object.type = ObjectType::Mutant;
    object.signalled = ctx.r5.u32 != 0 ? 0 : 1;   // owned means not available
    GiveBody(object);

    const uint32_t handle = CreateHandle(std::move(object), uint32_t(ctx.lr));
    if (ctx.r3.u32 != 0)
        Guest::Write32(base, ctx.r3.u32, handle);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// The job lock's poll, paced. The title's main thread waits for its workers
// by taking the job lock, looking, and giving it back, over and over: on
// the console each of those kernel calls costs a microsecond or so, here a
// tenth of that, and a run measured 2.09 billion takes and as many
// releases in five minutes, seven million a second each. At that rate the
// lock is free for a few nanoseconds at a time: a worker that wants it to
// take its next job, or to say it finished one, mostly finds it taken
// again, and every release wakes a sleeping worker to find just that. So a
// thread that takes back the mutant it gave up less than a microsecond ago,
// with nothing done in between, for the 64th time running, waits until a
// microsecond has passed since the release first: the window the console's
// own call gave everyone else. A take after real work (a worker's job) is
// never held back. COD3_POLLPACE=microseconds (0: off), 1 by default.
namespace
{
    thread_local uint32_t t_releasedMutant = 0;
    thread_local uint64_t t_releasedAt = 0;      // __rdtsc at the release
    thread_local uint32_t t_pollStreak = 0;

    uint64_t TicksPerMicrosecond()
    {
        static const uint64_t ticks = []() {
            const auto start = std::chrono::steady_clock::now();
            const uint64_t first = __rdtsc();
            while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2)) _mm_pause();
            const uint64_t elapsed = __rdtsc() - first;
            const double micros = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
            const uint64_t perMicro = micros > 0 ? uint64_t(double(elapsed) / micros) : 3000;
            return perMicro != 0 ? perMicro : uint64_t(3000);
        }();
        return ticks;
    }

    uint64_t PollPaceTicks()
    {
        static const uint64_t ticks = []() {
            const char* t = getenv("COD3_POLLPACE");
            const long micros = t != nullptr ? strtol(t, nullptr, 10) : 1;
            if (micros <= 0) return uint64_t(0);
            return uint64_t(micros > 50 ? 50 : micros) * TicksPerMicrosecond();
        }();
        return ticks;
    }

    void PacePoll(uint32_t handle)
    {
        const uint64_t pace = PollPaceTicks();
        if (pace == 0 || handle != t_releasedMutant) { t_pollStreak = 0; return; }
        const uint64_t now = __rdtsc();
        if (now - t_releasedAt >= pace) { t_pollStreak = 0; return; }
        if (++t_pollStreak < 64) return;
        while (__rdtsc() - t_releasedAt < pace) _mm_pause();
    }
}

// NTSTATUS NtReleaseMutant(HANDLE handle, LONG* previous)
PPC_FUNC(__imp__NtReleaseMutant)
{
    Kernel::CountImportOn("NtReleaseMutant", ctx.r3.u32);
    CountSignal(ctx.r3.u32, uint32_t(ctx.lr));
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    WakeHandle wake{ ctx.r3.u32 };

    Object* object = Find(ctx.r3.u32);
    if (object == nullptr) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }

    if (ctx.r4.u32 != 0)
        Guest::Write32(base, ctx.r4.u32, uint32_t(SignalState(*object)));

    // Only the matching release frees it, and only for the thread that holds it.
    if (object->recursion > 0) object->recursion--;
    if (object->recursion == 0)
    {
        object->owner = 0;
        SetSignalState(*object, 1);
        t_releasedMutant = ctx.r3.u32;
        t_releasedAt = __rdtsc();
    }
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtWaitForSingleObjectEx(HANDLE handle, mode, alertable, timeout*)
PPC_FUNC(__imp__NtWaitForSingleObjectEx)
{
    Kernel::CountImportOn("NtWaitForSingleObjectEx", ctx.r3.u32);
    Kernel::DeliverApcs(ctx, ctx.r5.u32 != 0);
    const uint32_t handle = ctx.r3.u32;
    PacePoll(handle);
    const uint32_t timeoutPtr = ctx.r6.u32;

    std::chrono::steady_clock::time_point deadline;
    bool infinite = false;
    const bool mayWait = TimeoutToDeadline(base, timeoutPtr, deadline, infinite);

    // The hardware thread is handed over only when this actually blocks:
    // the title's workers poll their job lock a million times a second
    // between them, and a wait that is over at once is not a place another
    // thread needs to run. Blocking is noted then too, for the reports of
    // what everything is waiting on. The order matters at the end: the
    // dispatcher lock goes before the hardware thread is taken back.
    struct Blocking
    {
        uint32_t handle, from; bool infinite; bool began = false;
        void Begin()
        {
            if (began) return;
            began = true;
            Scheduler::Release();
            CountWait(handle, from);
            EnterWait(handle, from, infinite);
        }
        ~Blocking() { if (began) { LeaveWait(); Scheduler::Acquire(); Timeline::Mark("unblocked", handle, from); } }
    } blocking{ handle, uint32_t(ctx.lr), infinite };

    // The object table first: nearly every wait is on one of its objects,
    // most of them the job lock, taken and given back a million times a
    // second, and this is one look under one lock for them.
    std::unique_lock<std::mutex> lock(Kernel::DispatcherLock());
    if (Find(handle) == nullptr)
    {
        // A file handle is signalled when its last I/O has finished, which
        // every I/O here has by the time the call returns.
        lock.unlock();
        if (Kernel::IsFileHandle(handle)) { ctx.r3.u32 = X_STATUS_SUCCESS; return; }
        lock.lock();

        // A thread handle is not in the object table: threads keep their
        // own. A wait on one is a wait for that thread to end, and it
        // completes when the thread's object in guest memory is signalled.
        {
            const uint32_t threadObject = Kernel::ThreadObjectFor(handle);
            if (threadObject != 0)
            {
                for (;;)
                {
                    if (Guest::Read32(base, threadObject + DISPATCH_SIGNAL_STATE) > 0)
                    {
                        ctx.r3.u32 = X_STATUS_SUCCESS;
                        return;
                    }
                    if (!mayWait) { ctx.r3.u32 = X_STATUS_TIMEOUT; return; }
                    if (!blocking.began) { lock.unlock(); blocking.Begin(); lock.lock(); continue; }
                    const uint64_t key = threadObject;
                    if (infinite)
                    {
                        Kernel::WaitDispatcherOn(lock, &key, 1, std::chrono::steady_clock::now() + std::chrono::milliseconds(20));
                    }
                    else if (Kernel::WaitDispatcherOn(lock, &key, 1, deadline) == std::cv_status::timeout)
                    {
                        ctx.r3.u32 = X_STATUS_TIMEOUT;
                        return;
                    }
                }
            }
        }
    }

    // A pulse that happens while this wait is in progress releases it.
    uint32_t pulsesAtStart = 0;
    if (Object* start = Find(handle)) pulsesAtStart = start->pulses;

    // A wait that would block looks again for a little while first. The
    // title's job system hands work to four worker threads through a
    // semaphore and a lock, and its main thread then waits, polling, for
    // them to finish; a worker that had gone to sleep took the host's wake
    // (and a hardware thread back) before it started, every job, and the
    // main thread's frame was spent polling for it. The console wakes one
    // in a moment. COD3_SPINWAIT=microseconds (0: never), 60 by default.
    // debug.txt's spin_wait over both, to compare while the game runs.
    static const long spinDefault = []() { const char* t = getenv("COD3_SPINWAIT"); const long v = t ? strtol(t, nullptr, 10) : 60; return v >= 0 && v <= 5000 ? v : 60L; }();
    const long spinGiven = DebugProbe::SpinWaitMicros();
    const long spinMicros = spinGiven >= 0 && spinGiven <= 5000 ? spinGiven : spinDefault;
    bool spun = spinMicros == 0;

    for (;;)
    {
        Object* object = Find(handle);
        if (object == nullptr) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }

        // The thread that already holds a mutant takes it again without
        // waiting. Anything else would deadlock a lock taken recursively,
        // which is normal and which the console allows.
        if (object->type == ObjectType::Mutant &&
            object->owner == GetCurrentThreadId())
        {
            object->recursion++;
            ctx.r3.u32 = X_STATUS_SUCCESS;
            return;
        }

        if (object->pulses != pulsesAtStart)
        {
            ctx.r3.u32 = X_STATUS_SUCCESS;
            return;
        }

        if (SignalState(*object) > 0)
        {
            if (object->type == ObjectType::Mutant)
            {
                SetSignalState(*object, 0);
                object->owner = GetCurrentThreadId();
                object->recursion = 1;
            }
            else if (object->type == ObjectType::Semaphore ||
                     (object->type == ObjectType::Event && !object->manualReset))
            {
                // A semaphore and a synchronisation event each consume the
                // signal they satisfy. A notification event does not.
                SetSignalState(*object, SignalState(*object) - 1);
            }
            ctx.r3.u32 = X_STATUS_SUCCESS;
            return;
        }

        if (!mayWait) { ctx.r3.u32 = X_STATUS_TIMEOUT; return; }

        if (!spun && (object->type == ObjectType::Mutant || object->type == ObjectType::Semaphore || object->type == ObjectType::Event))
        {
            spun = true;
            const auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(spinMicros);
            for (;;)
            {
                lock.unlock();
                // Anything else on this hardware thread runs meanwhile.
                Scheduler::Checkpoint();
                for (int i = 0; i < 48; i++) _mm_pause();
                lock.lock();
                Object* again = Find(handle);
                if (again == nullptr || SignalState(*again) > 0 || again->pulses != pulsesAtStart ||
                    (again->type == ObjectType::Mutant && again->owner == GetCurrentThreadId())) break;
                if (!infinite && std::chrono::steady_clock::now() >= deadline) break;
                if (std::chrono::steady_clock::now() >= until) break;
            }
            continue;   // looked at again from the top, under the lock
        }

        // Blocking after all: the hardware thread goes, then the state is
        // looked at again, since it may have changed meanwhile.
        if (!blocking.began) { lock.unlock(); blocking.Begin(); lock.lock(); continue; }

        uint64_t keys[2];
        const uint32_t keyCount = HandleKeys(handle, object->guestAddress, keys);
        if (infinite)
        {
            if (Kernel::WaitDispatcherOn(lock, keys, keyCount, std::chrono::steady_clock::now() + std::chrono::seconds(3)) == std::cv_status::timeout)
            {
                ReportStalledWait("handle", handle, uint32_t(ctx.lr), SignalState(*object));
                // 0.5.470: a mutant with no owner that does not read as free (its
                // state is read from its body in guest memory, its owner kept
                // here) can never be taken: 0.5.469's CTF on Rouen froze for good
                // loading, the main thread and a worker on mutant 0x104, "free"
                // (no owner) and not signalled. A mutant is free exactly when it
                // has no owner: put right, and the waiter takes it from the top.
                Object* again = Find(handle);
                if (again != nullptr && again->type == ObjectType::Mutant && again->owner == 0 && SignalState(*again) <= 0)
                {
                    SetSignalState(*again, 1);
                    again->recursion = 0;
                    static int told = 0;
                    if (told++ < 40) { printf("mutant repaired: 0x%08X had no owner and did not read as free (waited 3 s, from 0x%08X)\n", handle, uint32_t(ctx.lr)); fflush(stdout); }
                }
            }
        }
        else if (Kernel::WaitDispatcherOn(lock, keys, keyCount, deadline) == std::cv_status::timeout)
        {
            ctx.r3.u32 = X_STATUS_TIMEOUT;
            return;
        }
    }
}

// --- Dispatcher objects in guest memory ------------------------------------

// LONG KeSetEvent(KEVENT* event, LONG increment, BOOLEAN wait)
PPC_FUNC(__imp__KeSetEvent)
{
    Kernel::CountImportOn("KeSetEvent", ctx.r3.u32);
    CountSignal(ctx.r3.u32, uint32_t(ctx.lr));
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { uint64_t key; ~Wake() { Kernel::WakeDispatcherFor(&key, 1); } } wake{ ctx.r3.u32 };

    const uint32_t event = ctx.r3.u32;
    if (event == 0) { ctx.r3.u32 = 0; return; }

    const uint32_t previous = Guest::Read32(base, event + DISPATCH_SIGNAL_STATE);
    Guest::Write32(base, event + DISPATCH_SIGNAL_STATE, 1);
    Timeline::Mark("set event", event, uint32_t(ctx.lr));
    ctx.r3.u32 = previous;
    // COD3_XMATRACE: the mixer's two events, the first few times.
    static const bool audioTrace = getenv("COD3_XMATRACE") != nullptr;
    static int shown = 0;
    if (audioTrace && (event == 0x8291EB7Cu || event == 0x8291EB6Cu) && (shown < 40 && shown++ < 40))
    {
        printf("title: set event %08X (was %u) from %08X on thread %lu\n", event, previous, uint32_t(ctx.lr), GetCurrentThreadId());
        fflush(stdout);
    }
}

// LONG KeResetEvent(KEVENT* event)
PPC_FUNC(__imp__KeResetEvent)
{
    Kernel::CountImportOn("KeResetEvent", ctx.r3.u32);
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { uint64_t key; ~Wake() { Kernel::WakeDispatcherFor(&key, 1); } } wake{ ctx.r3.u32 };

    const uint32_t event = ctx.r3.u32;
    if (event == 0) { ctx.r3.u32 = 0; return; }

    const uint32_t previous = Guest::Read32(base, event + DISPATCH_SIGNAL_STATE);
    Guest::Write32(base, event + DISPATCH_SIGNAL_STATE, 0);
    ctx.r3.u32 = previous;
}

// VOID KeInitializeSemaphore(KSEMAPHORE* semaphore, LONG count, LONG limit)
PPC_FUNC(__imp__KeInitializeSemaphore)
{
    Kernel::CountImport("KeInitializeSemaphore");
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { ~Wake() { Kernel::WakeDispatcher(); } } wake;

    const uint32_t semaphore = ctx.r3.u32;
    if (semaphore == 0) return;

    // A KSEMAPHORE is the sixteen byte dispatcher header and the limit:
    // twenty bytes. This used to clear twenty four and put the limit at
    // twenty, which is the first word of whatever the title keeps next.
    // The audio mixer keeps its "mixed" event there: the limit of six
    // landed on the event's type byte, a synchronisation event became a
    // notification event that no wait ever cleared, and the callback that
    // waits for the mixer stopped waiting after the first frame, read
    // silence, and the mixer thread took its cleared request for a
    // shutdown and left.
    memset(Guest::Ptr(semaphore), 0, 0x14);
    Guest::Write32(base, semaphore + DISPATCH_TYPE, 5u << 24);   // SemaphoreObject
    Guest::Write32(base, semaphore + DISPATCH_SIGNAL_STATE, ctx.r4.u32);
    Guest::Write32(base, semaphore + 0x10, ctx.r5.u32);          // limit
}

// LONG KeReleaseSemaphore(KSEMAPHORE* semaphore, LONG increment, LONG adjust, BOOLEAN wait)
PPC_FUNC(__imp__KeReleaseSemaphore)
{
    Kernel::CountImportOn("KeReleaseSemaphore", ctx.r3.u32);
    CountSignal(ctx.r3.u32, uint32_t(ctx.lr));
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { uint64_t key; ~Wake() { Kernel::WakeDispatcherFor(&key, 1); } } wake{ ctx.r3.u32 };

    const uint32_t semaphore = ctx.r3.u32;
    if (semaphore == 0) { ctx.r3.u32 = 0; return; }

    const uint32_t previous = Guest::Read32(base, semaphore + DISPATCH_SIGNAL_STATE);
    const uint32_t limit = Guest::Read32(base, semaphore + 0x10);
    uint32_t count = previous + ctx.r5.u32;
    if (limit > 0 && count > limit) count = limit;

    Guest::Write32(base, semaphore + DISPATCH_SIGNAL_STATE, count);
    ctx.r3.u32 = previous;
}

// NTSTATUS KeWaitForSingleObject(void* object, reason, mode, alertable, timeout*)
PPC_FUNC(__imp__KeWaitForSingleObject)
{
    Kernel::CountImportOn("KeWaitForSingleObject", ctx.r3.u32);
    Scheduler::Release();
    struct Resume { ~Resume() { Scheduler::Acquire(); } } resume;
    Kernel::DeliverApcs(ctx, ctx.r6.u32 != 0);
    const uint32_t object = ctx.r3.u32;
    CountWait(object, uint32_t(ctx.lr));
    const uint32_t timeoutPtr = ctx.r7.u32;
    if (object == 0) { ctx.r3.u32 = X_STATUS_INVALID_PARAMETER; return; }

    std::chrono::steady_clock::time_point deadline;
    bool infinite = false;
    const bool mayWait = TimeoutToDeadline(base, timeoutPtr, deadline, infinite);

    EnterWait(object, uint32_t(ctx.lr), infinite);
    struct Leave { uint32_t object, from; ~Leave() { LeaveWait(); Timeline::Mark("ke waited", object, from); } } leave{ object, uint32_t(ctx.lr) };
    Timeline::Mark("ke wait", object, uint32_t(ctx.lr));

    static const bool audioTrace = getenv("COD3_XMATRACE") != nullptr;
    static int shown = 0;
    const bool traced = audioTrace && (object == 0x8291EB7Cu || object == 0x8291EB6Cu) && shown < 40;
    if (traced) { shown++; printf("title: wait on %08X (state %u, type %u) from %08X on thread %lu\n", object, Guest::Read32(base, object + 4), DispatchType(base, object), uint32_t(ctx.lr), GetCurrentThreadId()); fflush(stdout); }
    std::unique_lock<std::mutex> lock(Kernel::DispatcherLock());
    for (;;)
    {
        const uint32_t state = Guest::Read32(base, object + DISPATCH_SIGNAL_STATE);
        if (state > 0)
        {
            if (WaitConsumes(DispatchType(base, object)))
                Guest::Write32(base, object + DISPATCH_SIGNAL_STATE, state - 1);
            ctx.r3.u32 = X_STATUS_SUCCESS;
            if (traced) { printf("title:   wait on %08X done on thread %lu\n", object, GetCurrentThreadId()); fflush(stdout); }
            return;
        }

        if (!mayWait) { ctx.r3.u32 = X_STATUS_TIMEOUT; return; }

        const uint64_t key = object;
        if (infinite)
        {
            if (Kernel::WaitDispatcherOn(lock, &key, 1, std::chrono::steady_clock::now() + std::chrono::seconds(3)) == std::cv_status::timeout)
                ReportStalledWait("dispatcher object", object, uint32_t(ctx.lr), Guest::Read32(base, object + DISPATCH_SIGNAL_STATE));
        }
        else if (Kernel::WaitDispatcherOn(lock, &key, 1, deadline) == std::cv_status::timeout)
        {
            ctx.r3.u32 = X_STATUS_TIMEOUT;
            return;
        }
    }
}

// NTSTATUS KeWaitForMultipleObjects(count, objects[], type, ...)
PPC_FUNC(__imp__KeWaitForMultipleObjects)
{
    Kernel::CountImport("KeWaitForMultipleObjects");
    Scheduler::Release();
    struct Resume { ~Resume() { Scheduler::Acquire(); } } resume;
    const uint32_t count = ctx.r3.u32;
    const uint32_t objects = ctx.r4.u32;
    const uint32_t waitType = ctx.r5.u32;   // 0 all, 1 any
    const uint32_t timeoutPtr = ctx.r9.u32;

    if (count == 0 || objects == 0) { ctx.r3.u32 = X_STATUS_INVALID_PARAMETER; return; }

    std::chrono::steady_clock::time_point deadline;
    bool infinite = false;
    const bool mayWait = TimeoutToDeadline(base, timeoutPtr, deadline, infinite);

    // COD3_XMATRACE: the audio callback's wait for the mixer thread, with
    // its result and how long it took, a few times.
    static const bool audioTrace = getenv("COD3_XMATRACE") != nullptr;
    const bool traced = audioTrace && count >= 1 && Guest::Read32(base, objects) == 0x8291EB6Cu;
    const auto started = std::chrono::steady_clock::now();
    struct Report
    {
        bool traced; uint32_t& result; std::chrono::steady_clock::time_point started; const uint8_t* base; uint32_t objects; uint32_t count;
        ~Report()
        {
            static int shown = 0;
            if (!traced || shown++ >= 12) return;
            printf("title: callback waited %lld us for the mixer, result %u, states now", (long long)std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count(), result);
            for (uint32_t i = 0; i < count; i++) { const uint32_t o = Guest::Read32(base, objects + i * 4); printf(" %08X:%u (word0 %08X)", o, Guest::Read32(base, o + 4), Guest::Read32(base, o)); }
            printf("\n");
            fflush(stdout);
        }
    } report{ traced, ctx.r3.u32, started, base, objects, count };

    std::unique_lock<std::mutex> lock(Kernel::DispatcherLock());
    for (;;)
    {
        uint32_t signalled = 0;
        uint32_t firstSignalled = 0xFFFFFFFF;
        for (uint32_t i = 0; i < count; i++)
        {
            const uint32_t object = Guest::Read32(base, objects + i * 4);
            if (object == 0) continue;
            if (Guest::Read32(base, object + DISPATCH_SIGNAL_STATE) > 0)
            {
                signalled++;
                if (firstSignalled == 0xFFFFFFFF) firstSignalled = i;
            }
        }

        if ((waitType == 1 && signalled > 0) || (waitType == 0 && signalled == count))
        {
            // Consume only what a wait is supposed to consume.
            for (uint32_t i = 0; i < count; i++)
            {
                if (waitType == 1 && i != firstSignalled) continue;
                const uint32_t object = Guest::Read32(base, objects + i * 4);
                if (object == 0) continue;
                if (!WaitConsumes(DispatchType(base, object))) continue;
                const uint32_t state = Guest::Read32(base, object + DISPATCH_SIGNAL_STATE);
                if (state > 0)
                    Guest::Write32(base, object + DISPATCH_SIGNAL_STATE, state - 1);
            }
            ctx.r3.u32 = (waitType == 1) ? firstSignalled : 0;
            return;
        }

        if (!mayWait) { ctx.r3.u32 = X_STATUS_TIMEOUT; return; }

        uint64_t keys[64];
        const uint32_t keyCount = std::min(count, 64u);
        for (uint32_t i = 0; i < keyCount; i++) keys[i] = Guest::Read32(base, objects + i * 4);
        // More than the keys hold: woken by anything.
        const uint32_t named = count <= 64 ? keyCount : 0;
        if (infinite)
        {
            Kernel::WaitDispatcherOn(lock, keys, named, std::chrono::steady_clock::now() + std::chrono::seconds(3));
        }
        else if (Kernel::WaitDispatcherOn(lock, keys, named, deadline) == std::cv_status::timeout)
        {
            ctx.r3.u32 = X_STATUS_TIMEOUT;
            return;
        }
    }
}

// --- Networking ------------------------------------------------------------

// INT NetDll_WSAStartup(ULONG caller, WORD version, WSADATA* data)
PPC_FUNC(__imp__NetDll_WSAStartup)
{
    Kernel::CountImport("NetDll_WSAStartup");
    // Reported as available so the title's network layer initialises. Nothing
    // below this does anything: no socket call is implemented, and there are
    // no socket imports in this executable beyond startup and cleanup.
    const uint32_t data = ctx.r5.u32;
    if (data != 0)
        memset(Guest::Ptr(data), 0, 0x190);
    ctx.r3.u32 = 0;
}

// INT NetDll_WSACleanup(ULONG caller)
PPC_FUNC(__imp__NetDll_WSACleanup) { ctx.r3.u32 = 0; }

void Kernel::ReportWaitTraffic()
{
    // Printed once. A thread that blocks calls its wait exactly once, so
    // counting repeats would miss precisely the case that matters.
    static bool reported = false;
    if (reported) return;

    // The lock is a unique_lock so it can be given up before the lock
    // report at the end, which takes it again for its own copy.
    std::unique_lock<std::mutex> lock(g_trafficMutex);
    if (g_waits.empty()) return;
    reported = true;

    printf("\n");
    const uint64_t eventsMade = g_eventNext.load();
    const size_t eventsKept = size_t(std::min<uint64_t>(eventsMade, EventLimit));
    printf("the last %zu synchronisation events, oldest first:\n", eventsKept);
    for (size_t i = 0; i < eventsKept; i++)
    {
        const Event& event = g_events[(eventsMade - eventsKept + i) % EventLimit];
        printf("  thread %-5u %s 0x%08X   from 0x%08X\n",
            event.thread, event.action, event.object, event.from);
    }

    // The worker pool's own globals. sub_82129778 only wakes the workers when
    // the word at 0x8290E928 is non zero, so its value decides whether the
    // event the workers wait on is ever set at all.
    printf("\n");
    printf("worker pool globals:\n");
    static const struct { uint32_t address; const char* what; } globals[] = {
        { 0x8290E920, "lock handle" },
        { 0x8290E928, "wake gate" },
        { 0x8290E930, "job array" },
        { 0x8290E934, "job count" },
        { 0x8290E938, "queued" },
        { 0x8290E98C, "worker wake event" },
        { 0x8290F900, "main thread lock" },
        { 0x8290F934, "lock depth" },
    };
    for (const auto& global : globals)
        printf("  0x%08X %-16s = 0x%08X\n",
            global.address, global.what,
            Guest::Read32(Guest::Base, global.address));

    // The title keeps its own copy of the ring pointers in a structure whose
    // address is not known from outside. It is findable: the structure holds
    // the address the GPU publishes the read pointer to, so searching guest
    // memory for that address locates the structure, and the fields around it
    // say what the title actually believes about the ring.
    printf("\n");
    printf("the title's own view of the ring:\n");
    {
        const uint32_t context = Gpu::InterruptContext();
        if (context == 0)
        {
            printf("  no interrupt context yet\n");
        }
        else
        {
            // The exact meaning of these fields is not documented, so the
            // block around the two the spin loop reads is dumped whole and the
            // pattern is read off it.
            printf("  context 0x%08X, fields 0x2A00 to 0x2A28:\n", context);
            for (uint32_t offset = 0x2A00; offset <= 0x2A28; offset += 4)
            {
                const uint32_t value = Guest::Read32(Guest::Base, context + offset);
                printf("    +0x%04X = 0x%08X (%u)%s\n", offset, value, value,
                    offset == 0x2A10 ? "   read pointer location" :
                    offset == 0x2A1C ? "   the value the spin compares against" : "");
            }

            const uint32_t location = Guest::Read32(Guest::Base, context + 0x2A10);
            if (location != 0)
                printf("  read pointer there = %u\n",
                    Guest::Read32(Guest::Base, location));
            printf("  write pointer register = %u\n", Gpu::WritePointer());

            // The ring address the structure holds is a physical address, and
            // physical memory is reachable through several windows. Which one
            // actually holds the command data has to be looked at rather than
            // assumed, because walking the wrong one turns noise into packets.
            const uint32_t ringAddress = Guest::Read32(Guest::Base, context + 0x2A04);
            if (ringAddress != 0)
            {
                printf("  ring address 0x%08X, first words at each alias:\n",
                    ringAddress);
                // Only the window the command processor reads through: the
                // others are not mapped, and reading them ended the process
                // from inside a report.
                const uint32_t aliases[] = { Guest::PhysicalAlias(ringAddress) };
                for (uint32_t alias : aliases)
                {
                    printf("    0x%08X:", alias);
                    for (int i = 0; i < 6; i++)
                        printf(" %08X", Guest::Read32(Guest::Base, alias + i * 4));
                    printf("\n");
                }
            }
        }
    }

    lock.unlock();
    Kernel::ReportLocks();
}

void Kernel::EnterWait(uint32_t object, uint32_t from, bool infinite)
{
    ::EnterWait(object, from, infinite);
}

void Kernel::LeaveWait()
{
    ::LeaveWait();
}

void Kernel::ReportLocks()
{
    // Who holds each mutant. A thread blocked on one is waiting for whoever
    // owns it, so this turns "blocked on 0x108" into a thread to go and look at.
    printf("\n");
    printf("mutant ownership:\n");
    {
        std::lock_guard<std::mutex> objects(Kernel::DispatcherLock());
        for (const auto& entry : Handles())
        {
            if (entry.second.type != ObjectType::Mutant) continue;
            if (entry.second.owner == 0)
                printf("  0x%08X free\n", entry.first);
            else
                printf("  0x%08X held by thread %u, depth %d\n",
                    entry.first, entry.second.owner, entry.second.recursion);
        }
    }

    printf("\n");
    printf("what each thread is blocked on right now:\n");
    // A copy: the threads keep entering and leaving waits while this prints.
    std::map<uint32_t, Blocked> blocked;
    {
        std::lock_guard<std::mutex> lock(g_trafficMutex);
        blocked = g_blocked;
    }
    if (blocked.empty()) printf("  nothing is blocked\n");
    for (const auto& entry : blocked)
    {
        printf("  thread %-5u on 0x%08X from 0x%08X, %s",
            entry.first, entry.second.object, entry.second.from,
            entry.second.infinite ? "no timeout" : "with a timeout");

        // A critical section is an address in the title's memory rather than
        // a handle; its holder is the thread object written into it.
        if (entry.second.object >= 0x80000000u)
        {
            const uint32_t owner = Guest::Read32(Guest::Base, entry.second.object + 0x18);
            printf("   a critical section held by thread object 0x%08X (os %u)",
                owner, owner ? Guest::Read32(Guest::Base, owner + 0x14C) : 0);
        }

        // A thread handle says nothing on its own. Which thread, and where it
        // started, is what makes a stalled wait readable.
        const std::string who = Kernel::DescribeThreadHandle(entry.second.object);
        if (!who.empty()) printf("   waiting for a thread: %s", who.c_str());
        printf("\n");
    }

    Kernel::ReportPoolWaits();

    // And what each of them was doing, since a thread that appears to be
    // waiting on a lock it holds is either a report out of date or a wait
    // that took a path this runtime does not expect.
    Kernel::ReportRecentCalls();

    printf("\n");
    printf("synchronisation traffic so far:\n");
    for (const auto& entry : g_waits)
    {
        const auto signalled = g_signals.find(entry.first);
        printf("  object 0x%08X  waited %llu, signalled %llu%s\n",
            entry.first,
            (unsigned long long)entry.second,
            signalled == g_signals.end() ? 0ull : (unsigned long long)signalled->second,
            signalled == g_signals.end() ? "   <- nothing ever signals this" : "");
    }
    for (const auto& entry : g_signals)
    {
        if (g_waits.find(entry.first) != g_waits.end()) continue;
        printf("  object 0x%08X  signalled %llu, never waited on\n",
            entry.first, (unsigned long long)entry.second);
    }
    fflush(stdout);
}

bool Kernel::SignalHandle(uint32_t handle)
{
    if (handle == 0) return false;

    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { ~Wake() { Kernel::WakeDispatcher(); } } wake;

    Object* object = Find(handle);
    if (object == nullptr) return false;
    SetSignalState(*object, 1);
    return true;
}

void Kernel::AbandonMutants(uint32_t osThreadId)
{
    // A thread that ends while holding a mutant.
    //
    // The console frees them: the kernel hands the next waiter the lock and
    // tells it the previous owner died. Leaving them held instead means every
    // thread behind that lock waits for a thread that no longer exists, which
    // is a deadlock that only shows up as the title going quiet. One run in
    // three stopped this way, always with a mutant still owned by a thread that
    // was no longer blocked on anything.
    if (osThreadId == 0) return;

    int released = 0;
    {
        std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
        for (auto& entry : Handles())
        {
            Object& object = entry.second;
            if (object.type != ObjectType::Mutant) continue;
            if (object.owner != osThreadId) continue;

            object.owner = 0;
            object.recursion = 0;
            SetSignalState(object, 1);
            released++;
        }
    }

    if (released > 0)
    {
        printf("thread: %d mutant%s released because the thread holding "
               "%s ended\n",
            released, released == 1 ? "" : "s", released == 1 ? "it" : "them");
        fflush(stdout);
        std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
        Kernel::WakeDispatcher();
    }
}

// --- Timers ------------------------------------------------------------------
//
// Only the multiplayer executable makes timers (its network threads pace
// themselves with them). A timer is an event the runtime signals itself
// when it falls due: a notification timer (type 0) stays signalled until it
// is set again, a synchronisation timer (type 1) releases one waiter. A
// thread of the host's sleeps until the due time; setting or cancelling the
// timer again makes any earlier one's wake-up a no-op, by its generation.

namespace
{
    std::mutex g_timerMutex;
    std::map<uint32_t, uint64_t> g_timerGeneration;   // handle -> generation

    // The current system time in the console's units: 100 ns since 1601.
    uint64_t SystemTime100ns()
    {
        FILETIME now;
        GetSystemTimePreciseAsFileTime(&now);
        return (uint64_t(now.dwHighDateTime) << 32) | uint64_t(now.dwLowDateTime);
    }

    void FireTimer(uint32_t handle, uint64_t generation, uint64_t firstDelay100ns, uint32_t periodMs)
    {
        std::thread([handle, generation, firstDelay100ns, periodMs] {
            std::this_thread::sleep_for(std::chrono::nanoseconds(firstDelay100ns * 100));
            for (;;)
            {
                {
                    std::lock_guard<std::mutex> lock(g_timerMutex);
                    const auto found = g_timerGeneration.find(handle);
                    if (found == g_timerGeneration.end() || found->second != generation) return;
                }
                {
                    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
                    WakeHandle wake{ handle };
                    Object* object = Find(handle);
                    if (object == nullptr) return;
                    SetSignalState(*object, 1);
                }
                if (periodMs == 0) return;
                std::this_thread::sleep_for(std::chrono::milliseconds(periodMs));
            }
        }).detach();
    }
}

// NTSTATUS NtCreateTimer(HANDLE* handle, OBJECT_ATTRIBUTES* attributes, TIMER_TYPE type)
PPC_FUNC(__imp__NtCreateTimer)
{
    Kernel::CountImport("NtCreateTimer");
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    struct Wake { ~Wake() { Kernel::WakeDispatcher(); } } wake;

    Object object;
    object.type = ObjectType::Event;
    object.manualReset = (ctx.r5.u32 == 0);   // 0 notification, 1 synchronisation
    object.signalled = 0;
    GiveBody(object);
    const uint32_t handle = CreateHandle(std::move(object), uint32_t(ctx.lr));
    if (ctx.r3.u32 != 0) Guest::Write32(base, ctx.r3.u32, handle);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtSetTimerEx(HANDLE handle, LARGE_INTEGER* dueTime, PTIMERAPCROUTINE routine,
//                       KPROCESSOR_MODE mode, void* context, BOOLEAN resume, LONG period, BOOLEAN* previous)
PPC_FUNC(__imp__NtSetTimerEx)
{
    Kernel::CountImport("NtSetTimerEx");
    const uint32_t handle = ctx.r3.u32;
    const int64_t due = ctx.r4.u32 != 0 ? int64_t(Guest::Read64(base, ctx.r4.u32)) : 0;
    const uint32_t period = ctx.r9.u32;
    if (ctx.r5.u32 != 0)
    {
        static std::atomic<int> shown{ 0 };
        if ((shown.load(std::memory_order_relaxed) < 4 && shown.fetch_add(1) < 4))
        {
            printf("object: timer 0x%08X has a completion routine 0x%08X, which this runtime does not call\n", handle, ctx.r5.u32);
            fflush(stdout);
        }
    }
    {
        std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
        WakeHandle wake{ handle };
        Object* object = Find(handle);
        if (object == nullptr) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }
        if (ctx.r10.u32 != 0) Guest::Write8(base, ctx.r10.u32, uint8_t(SignalState(*object) != 0));
        SetSignalState(*object, 0);
    }
    // Negative: relative, in 100 ns. Positive: an absolute system time.
    uint64_t delay = 0;
    if (due < 0) delay = uint64_t(-due);
    else if (due > 0) { const uint64_t now = SystemTime100ns(); delay = uint64_t(due) > now ? uint64_t(due) - now : 0; }
    uint64_t generation;
    {
        std::lock_guard<std::mutex> lock(g_timerMutex);
        generation = ++g_timerGeneration[handle];
    }
    FireTimer(handle, generation, delay, period);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtCancelTimer(HANDLE handle, BOOLEAN* currentState)
PPC_FUNC(__imp__NtCancelTimer)
{
    Kernel::CountImport("NtCancelTimer");
    {
        std::lock_guard<std::mutex> lock(g_timerMutex);
        ++g_timerGeneration[ctx.r3.u32];
    }
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    Object* object = Find(ctx.r3.u32);
    if (object == nullptr) { ctx.r3.u32 = X_STATUS_INVALID_HANDLE; return; }
    if (ctx.r4.u32 != 0) Guest::Write8(base, ctx.r4.u32, uint8_t(SignalState(*object) != 0));
    ctx.r3.u32 = X_STATUS_SUCCESS;
}

// NTSTATUS NtDuplicateObject(HANDLE source, HANDLE* target, DWORD options)
//
// A second handle to the same object. The copy shares the object's body in
// guest memory, which is where the signal state is read from, so a signal
// through either is seen through both. A handle this table does not hold (a
// file's, a thread's) is handed back as it is.
PPC_FUNC(__imp__NtDuplicateObject)
{
    Kernel::CountImport("NtDuplicateObject");
    std::lock_guard<std::mutex> lock(Kernel::DispatcherLock());
    uint32_t duplicate = ctx.r3.u32;
    if (Object* object = Find(ctx.r3.u32))
    {
        Object copy = *object;
        duplicate = CreateHandle(std::move(copy), uint32_t(ctx.lr));
    }
    if (ctx.r4.u32 != 0) Guest::Write32(base, ctx.r4.u32, duplicate);
    ctx.r3.u32 = X_STATUS_SUCCESS;
}
