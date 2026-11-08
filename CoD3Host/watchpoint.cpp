// Catching whoever writes a particular word of guest memory.
//
// When a value that was right becomes wrong, the useful question is not what it
// is now but who changed it, and no amount of polling answers that: by the time
// a poll notices, the writer is long gone.
//
// The processor can answer it directly. A debug register holds an address and a
// length, and the processor raises a single step exception on the instruction
// that writes there. The vectored handler this runtime already has for guest
// memory then names the guest function, exactly as it does for a fault.
//
// Debug registers are per thread, so the watch has to be armed on every thread
// that could do the writing, including ones that start later.

#include "kernel.h"
#include "sampler.h"

#include <atomic>
#include <cstdio>
#include <mutex>
#include <set>
#include <vector>

#include <Windows.h>
#include <TlHelp32.h>

namespace
{
    std::mutex g_mutex;
    uint32_t g_watched = 0;          // guest address, zero when nothing is watched
    bool g_reads = false;            // reads too, not only writes
    std::set<uint32_t> g_armed;      // os thread ids already carrying the watch
    std::atomic<int> g_reports{ 0 };
    uint32_t g_above = 0;            // 0.5.556: nonzero: a value above this is the suspicious one (a count, not a pointer)

    bool ArmThread(HANDLE thread, uintptr_t address)
    {
        CONTEXT context{};
        context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        if (!GetThreadContext(thread, &context)) return false;

        context.Dr0 = address;

        // Enable the first slot locally, and describe it: write only, four
        // bytes. The two bit fields for slot zero sit at bits 16 and 18.
        context.Dr7 &= ~uint64_t(0xF << 16);
        context.Dr7 |= uint64_t(1) << 0;          // local enable, slot zero
        context.Dr7 |= uint64_t(g_reads ? 0b11 : 0b01) << 16;   // break on write, or on read and write
        context.Dr7 |= uint64_t(0b11) << 18;      // four bytes

        context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
        return SetThreadContext(thread, &context) != FALSE;
    }
}

void Kernel::WatchWrite(uint32_t guestAddress, bool reads)
{
    std::lock_guard<std::mutex> lock(g_mutex);

    // The first address asked for is the one kept. Moving the watch to a
    // second one leaves the first unwatched, which is exactly where the write
    // then happened.
    if (g_watched != 0) return;

    g_watched = guestAddress;
    g_reads = reads;
    g_armed.clear();
    printf("watchpoint: watching guest address 0x%08X for %s\n", guestAddress, reads ? "reads and writes" : "writes");
    fflush(stdout);
}

void Kernel::ArmWatchpoints()
{
    uint32_t address = 0;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        address = g_watched;
    }
    if (address == 0) return;

    const uintptr_t host = reinterpret_cast<uintptr_t>(Guest::Base) + address;

    // Every thread in the process, not only the guest ones: a value can just as
    // easily be cleared by this runtime as by the title, and arming half of
    // them would only prove which half.
    const DWORD self = GetCurrentThreadId();
    const DWORD process = GetCurrentProcessId();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return;

    THREADENTRY32 entry{};
    entry.dwSize = sizeof(entry);
    if (Thread32First(snapshot, &entry))
    {
        do
        {
            if (entry.th32OwnerProcessID != process) continue;
            if (entry.th32ThreadID == self) continue;
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                if (g_armed.find(entry.th32ThreadID) != g_armed.end()) continue;
            }

            HANDLE thread = OpenThread(
                THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME,
                FALSE, entry.th32ThreadID);
            if (thread == nullptr) continue;

            bool ok = false;
            if (SuspendThread(thread) != DWORD(-1))
            {
                ok = ArmThread(thread, host);
                ResumeThread(thread);
            }
            CloseHandle(thread);

            if (!ok) continue;
            std::lock_guard<std::mutex> lock(g_mutex);
            g_armed.insert(entry.th32ThreadID);
        }
        while (Thread32Next(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

bool Kernel::SelfTestWatchpoint()
{
    // A watch that never fires and a value that never changes look the same
    // from outside, so the mechanism is checked against itself: arm the
    // current thread on a scratch word, write it, and see whether the report
    // came out. A tool that is not known to work is worse than no tool.
    static bool done = false;
    if (done) return true;
    done = true;

    const uint32_t scratch = 0x3F7FF000;   // inside the runtime's own region
    const uintptr_t host = reinterpret_cast<uintptr_t>(Guest::Base) + scratch;
    VirtualAlloc(Guest::Ptr(scratch), 0x1000, MEM_COMMIT, PAGE_READWRITE);

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_watched = scratch;
        g_armed.clear();
    }

    // Arming the current thread needs no suspend: the context is written back
    // when the handler returns to it.
    CONTEXT context{};
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    HANDLE self = GetCurrentThread();
    if (!GetThreadContext(self, &context)) return false;
    context.Dr0 = host;
    context.Dr7 &= ~uint64_t(0xF << 16);
    context.Dr7 |= uint64_t(1) << 0;
    context.Dr7 |= uint64_t(0b01) << 16;
    context.Dr7 |= uint64_t(0b11) << 18;
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    if (!SetThreadContext(self, &context)) return false;

    const int before = g_reports.load();
    *reinterpret_cast<volatile uint32_t*>(Guest::Ptr(scratch)) = 0x12345678;
    const bool fired = g_reports.load() != before;

    // Put it back the way it was.
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_watched = 0;
        g_armed.clear();
    }
    context.Dr7 = 0;
    context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
    SetThreadContext(self, &context);

    printf("watchpoint: self test %s\n", fired ? "fired, the mechanism works"
                                               : "DID NOT FIRE, the mechanism is not working");
    fflush(stdout);
    return fired;
}

void Kernel::WatchSuspiciousAbove(uint32_t threshold)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    g_above = threshold;
}

bool Kernel::ReportWatchpoint(void* winContext)
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        if (g_watched == 0) return false;
    }
    // Every hit is counted, because the self test below is a host side
    // write that expects to be counted. Only the printing is filtered.
    const int report = g_reports.fetch_add(1);

    uint32_t functions[12] = {};
    const int count = Sampler::WalkGuestStack(winContext, functions, 12);

    // Writes made by this runtime on the title's behalf, fences and the
    // like, come from host threads whose only guest frames are the import
    // thunks at 0x82578000 and the thread start trampoline behind them.
    // Those are known and uninteresting; the writes worth seeing are the
    // title's own, which have real code on the stack.
    bool guestCode = false;
    for (int i = 0; i < count; i++)
        if (functions[i] < 0x82578000u) guestCode = true;

    // A word that is written every frame fills the forty reports in a
    // second with the writes that are right. The ones that matter are
    // those that leave something that is not a pointer, and they are
    // printed whoever made them and however many came before.
    // Not read here when reads are watched: the handler's own read would
    // trip the watch again, inside itself, without end.
    const uint32_t now = g_reads ? 0 : Guest::Read32(Guest::Base, g_watched);
    const bool suspicious = !g_reads && (g_above != 0 ? now > g_above : (now < 0x10000u || now >= 0xC0000000u));
    // 0.5.556: how often the word is written, once a minute (the watch's cost)
    {
        static std::atomic<uint32_t> perMinute{ 0 }; static std::atomic<uint64_t> since{ 0 };
        const uint64_t ms = GetTickCount64();
        perMinute++;
        uint64_t was = since.load();
        if (was == 0) since.store(ms);
        else if (ms - was >= 60000 && since.compare_exchange_strong(was, ms))
        { printf("watchpoint: 0x%08X written %u times in the last minute\n", g_watched, perMinute.exchange(0)); fflush(stdout); }
    }
    if (!suspicious && (!guestCode || report >= 40)) return true;

    printf("\nwatchpoint: guest address 0x%08X was %s\n", g_watched, g_reads ? "touched" : "written");
    if (count > 0)
    {
        printf("  guest call chain, innermost first:");
        for (int i = 0; i < count; i++)
            printf("%ssub_%08X", i == 0 ? " " : " <- ", functions[i]);
        printf("\n");
    }
    if (g_above != 0)   // 0.5.556: the list it belongs to (buffer, capacity, count)
        printf("  it now holds 0x%08X (%u)%s; the words before it: %08X %08X\n", now, now, suspicious ? ", far above any real count" : "",
               Guest::Read32(Guest::Base, g_watched - 8), Guest::Read32(Guest::Base, g_watched - 4));
    else
        printf("  it now holds 0x%08X%s\n", now, suspicious ? ", which is not a pointer" : "");
    // The instruction itself, and the thread's link register: the host
    // walk above goes astray through host frames, these two do not.
    {
        const CONTEXT* context = static_cast<const CONTEXT*>(winContext);
        const uint32_t function = Sampler::FunctionAt(context->Rip);
        const PPCContext* guest = Kernel::ContextOf(GetCurrentThreadId());
        printf("  written by sub_%08X, os thread %lu, lr 0x%08X\n", function, GetCurrentThreadId(),
            guest ? uint32_t(guest->lr) : 0);
    }
    fflush(stdout);
    return true;
}
