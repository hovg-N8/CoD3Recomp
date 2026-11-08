#pragma once

// Shared declarations for the host side of the recompilation.
//
// ppc_recomp_shared.h brings in ppc_config.h and ppc_context.h, which is what
// gives PPC_FUNC_IMPL, PPCContext and the guest memory macros. Including it
// here means the kernel stubs are declared exactly the way the recompiled code
// calls them, so the linker matches them up.
#include "ppc_recomp_shared.h"

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <string>
#include <filesystem>
#include <mutex>
#include <vector>

namespace Kernel
{
    // The multiplayer map whose archive was opened last (kernel_file.cpp),
    // lower case: "poissons", "rouen"...
    inline std::string& CurrentMap() { static std::string map; return map; }

    // One kernel import: the name the XEX records for it, and the host
    // function that stands in for it.
    struct Import
    {
        const char* name;
        PPCFunc*    host;
    };

    // The host stub behind a kernel import, by name, or null.
    PPCFunc* FindImport(const char* name);

    // Where a guest path (game:\sp\x.dll, d:\..., or bare) is in the installed
    // game, for reading; empty when it is not there.
    std::filesystem::path ResolveGuestPath(const std::string& guestPath);

    // Called by every unimplemented import. Reports which one the game reached
    // and how it got there, then stops. It never returns: continuing past a
    // missing kernel call would corrupt guest state and produce a crash far
    // from the real cause.
    [[noreturn]] void Unimplemented(const char* name, PPCContext& ctx, uint8_t* base);

    // Ends the process. When the console belongs to this program, which is the
    // case when it was started from Explorer rather than from a shell, it waits
    // for a key first: otherwise the window closes the instant the process ends
    // and everything printed goes with it.
    [[noreturn]] void Exit(int code);

    // Prepares the saves folder. Reads resolve against Guest::GameRoot, which
    // the installer has already located.
    void InitializeFileSystem(const std::filesystem::path& exeDirectory);

    // A content package the title has mounted under a root name ("save:")
    // is a folder here; paths under the name resolve into it, for reading
    // and writing alike, until it is unmounted.
    std::filesystem::path SavesRoot();
    void MountContent(const std::string& rootName, const std::filesystem::path& folder);
    void UnmountContent(const std::string& rootName);

    // NtClose serves both the object table and the file table. This lets the
    // object implementation hand a handle it does not recognise to the file
    // layer before reporting it invalid.
    // Every mutant a thread still owned when it ended, handed back. The
    // console does this; without it the next waiter waits for a thread that
    // has gone.
    void AbandonMutants(uint32_t osThreadId);

    bool CloseFileHandle(uint32_t handle);

    // Whether a handle is a file. A wait on one is a wait for its I/O to
    // finish, and here that has always already happened.
    bool IsFileHandle(uint32_t handle);

    // Bytes actually read, per file, and a report of the busiest ones.
    void CountFileRead(const std::string& guestPath, uint32_t bytes);
    void ReportFileReads();

    // One lock and one condition variable cover every kernel object's signal
    // state, whether it lives in the handle table or in guest memory. A single
    // pair is coarse, but it makes the ordering between a signal and a wait
    // impossible to get wrong, which matters far more here than contention.
    // A guest thread, for the stack sampler.
    struct ThreadSample
    {
        void* handle = nullptr;
        uint32_t id = 0;
        uint32_t osId = 0;   // so a stack can be matched to a lock owner
        uint32_t startAddress = 0;
    };
    std::vector<ThreadSample> SampleGuestThreads();

    // The thread that entered the guest is not one ExCreateThread made, so it
    // has to be registered by hand or the sampler cannot see the one thread
    // that has been running longest.
    void RegisterEntryThread();

    // Reports an object guest threads keep waiting on that nothing signals.
    void ReportWaitTraffic();

    // The mutants and who holds them, what each thread is blocked on, and
    // every thread's recent kernel calls: the second half of the above, on
    // its own, for a lock that is not coming free.
    void ReportLocks();

    // A thread about to block on an object, for the report above, and the
    // same thread no longer blocked. Critical sections use this too: their
    // wait is a loop of this runtime's own, not a dispatcher wait.
    void EnterWait(uint32_t object, uint32_t from, bool infinite);
    void LeaveWait();

    // The title's own waits for the GPU to pass a point in its indirect
    // buffer pool, those in progress: the point, the lap, and the GPU's
    // last word on where it is.
    void ReportPoolWaits();

    std::mutex& DispatcherLock();
    // Sleeping on the dispatcher (DispatcherLock held), and waking the
    // sleepers. A sleep may say what it waits for, by keys: a guest address
    // (the object's body, a critical section), or a handle with bit 32 set.
    // A change to those wakes it and nothing else does; a sleep that names
    // nothing is woken by every change. WakeDispatcher wakes everyone.
    std::cv_status WaitDispatcher(std::unique_lock<std::mutex>& lock, std::chrono::milliseconds limit);
    std::cv_status WaitDispatcherUntil(std::unique_lock<std::mutex>& lock, std::chrono::steady_clock::time_point deadline);
    std::cv_status WaitDispatcherOn(std::unique_lock<std::mutex>& lock, const uint64_t* keys, uint32_t count,
                                    std::chrono::steady_clock::time_point deadline);
    void WakeDispatcher();
    void WakeDispatcherFor(const uint64_t* keys, uint32_t count);
    // The same without the lock held: it is taken only when someone sleeps.
    void WakeDispatcherForUnlocked(uint64_t key);

    // A heartbeat, so a run that neither stops nor draws anything still shows
    // whether the title is doing work. Without a picture there is no other way
    // to tell progress from a spin.
    struct Counters
    {
        std::atomic<uint64_t> frames{ 0 };
        std::atomic<uint64_t> filesOpened{ 0 };
        std::atomic<uint64_t> fileBytesRead{ 0 };
        std::atomic<uint64_t> threadsCreated{ 0 };
        std::atomic<uint64_t> bytesAllocated{ 0 };
        std::atomic<uint64_t> audioFrames{ 0 };
        std::atomic<uint64_t> swaps{ 0 };         // VdSwap calls: the frames the title finished
        std::atomic<uint64_t> swapsReached{ 0 };  // swap packets the GPU has carried out
    };
    Counters& Stats();

    // Which kernel calls the title is making, and how often.
    //
    // A title that has stopped making progress is almost always waiting on
    // something, and what it calls while it waits names what it is waiting for.
    // There is no central place imports are dispatched, so each one counts
    // itself on the way in.
    void CountImport(const char* name);

    // The same, with the call's first argument. A loop of waits says nothing
    // until you know which object each wait was on.
    void CountImportOn(const char* name, uint32_t subject);
    void ReportImports();

    // Begins the kernel call trace COD3_TRACEKERNEL asks for, if it does.
    void StartKernelTrace();

    // A thread that prints every host thread's stack once the vertical
    // blank counter has stopped moving: what a hang looks like from inside.
    void StartWatchdog();

    // COD3_HOSTPROFILE=tags: a thread's time by function, from samples of
    // where it is a thousand times a second, printed every five seconds:
    // the functions it is in itself, and those on its stack (inclusive).
    // The thread is profiled when its tag is named ("cp" the command
    // processor, "main" the title's first thread; "1" means "cp").
    void ProfileHostThread(uint32_t osId, const char* name, const char* tag);

    // The host stack behind a set of registers, symbolised: for the fault
    // handler, which has the registers of the thread that faulted.
    void PrintHostStack(void* context);

    // Set by a thread while it unwinds another thread's stack from a copy of
    // its registers. A fault during that is a stale register, not a guest
    // error, and the fault handler leaves it to the unwinder's own handler.
    void SetUnwinding(bool unwinding);
    bool IsUnwinding();

    // Text the title formatted for itself, which is the closest thing it has
    // to telling this runtime what it is doing.
    // A guest printf's text: the format at formatAddress, and its arguments
    // from register firstRegister (3 to 10) on, then the stack.
    std::string FormatGuestCall(const PPCContext& ctx, const uint8_t* base, uint32_t formatAddress, uint32_t firstRegister);
    void ReportGuestText(const std::string& text);

    // The last few kernel calls each thread made, in order. A thread that has
    // stopped making progress is repeating something, and the repetition names
    // what it is waiting for far better than a count does.
    void ReportRecentCalls(uint32_t onlyThread = 0);

    // The processor context of the guest thread this host thread is running,
    // so a fault report can print the registers at the fault. Each thread
    // that runs guest code registers its context on the way in.
    void SetCurrentContext(PPCContext* context);

    // Another thread's context, by host thread id, for reading its guest
    // registers in a report; null for a thread that runs no guest code.
    PPCContext* ContextOf(uint32_t hostThreadId);
    PPCContext* CurrentContext();
    // 0.5.240: a handler the multiplayer program puts first (mp_fixes.cpp's
    // roster trap): given the EXCEPTION_POINTERS, true when it dealt with it.
    extern bool (*FaultHook)(void* exceptionPointers);

    // COD3_DUMP names guest addresses, in hexadecimal separated by commas,
    // to print whenever something goes wrong: a bad call, a host fault. Each
    // gets eight words, and whatever its first word points at gets eight
    // more, which is one hop along a list or into an object.
    void DumpRequested();

    // General register number n of a context. They are not laid out in order
    // (r3 comes first), so this is the only way to index them.
    inline uint32_t Register(const PPCContext& context, int n)
    {
        const PPCRegister* const registers[32] = {
            &context.r0,  &context.r1,  &context.r2,  &context.r3,
            &context.r4,  &context.r5,  &context.r6,  &context.r7,
            &context.r8,  &context.r9,  &context.r10, &context.r11,
            &context.r12, &context.r13, &context.r14, &context.r15,
            &context.r16, &context.r17, &context.r18, &context.r19,
            &context.r20, &context.r21, &context.r22, &context.r23,
            &context.r24, &context.r25, &context.r26, &context.r27,
            &context.r28, &context.r29, &context.r30, &context.r31,
        };
        return registers[n & 31]->u32;
    }

    // A hardware watch on one word of guest memory. The processor raises an
    // exception on the instruction that writes it, which is the only way to
    // find out who changed a value that used to be right.
    void WatchWrite(uint32_t guestAddress, bool reads = false);
    void ArmWatchpoints();
    bool ReportWatchpoint(void* winContext);
    // 0.5.556: the watched word is a count, not a pointer: a value above
    // this is the one to report whoever wrote it (0: the pointer test).
    void WatchSuspiciousAbove(uint32_t threshold);
    bool SelfTestWatchpoint();

    // Signals an event by handle, for a kernel call that completes work the
    // title asked to be told about. Returns false when the handle is not one.
    bool SignalHandle(uint32_t handle);

    // The guest memory a thread handle refers to, and the signal that a thread
    // has ended so a wait on it completes.
    uint32_t ThreadObjectFor(uint32_t handle);

    // "os 1234, started at 0x82527B70" for a thread handle, empty otherwise.
    std::string DescribeThreadHandle(uint32_t handle);
    void SignalThreadExit(uint32_t osId);
    void RecordThreadPointer(uint32_t osId, uint32_t block);

    // Completion routines for asynchronous work: queued against the thread
    // that asked, run when that thread next waits.
    void QueueApc(uint32_t routine, uint32_t context, uint32_t statusBlock,
                  uint32_t status, uint32_t information);
    // Completion routines run when the thread waits and says it may be
    // interrupted. A thread that waits without saying so is not expecting its
    // own code to run underneath it, and running it there turns a sequence the
    // title wrote into a reentrant one. They are delivered anyway once they
    // have waited long enough that something is clearly wrong.
    void DeliverApcs(PPCContext& ctx, bool alertable);
    void ReportApcs();

    // The audio render callback is driven from a thread of its own.
    void StopAudioPump();
    uint64_t AudioCallbacks();

    // A console command for the title, from the host's console (overlay.cpp):
    // put on the title's own command buffer at the next vertical blank,
    // the way COD3_MAP starts a level. Any thread may queue one.
    void QueueConsoleCommand(const std::string& text);
    // The frame rate is unlocked (the settings) and a level is played:
    // the wait after each swap is let through.
    bool FrameRateUnlocked();
    // One of the title's films (the WMV player) is open: the render layer
    // shows it without the display's gamma ramp (render_d3d11.cpp).
    bool FilmPlaying();

    // The heaps: how much is handed out and how much of that is free again.
    void ReportHeaps();

    // A fault at an address inside the heaps that no live region holds:
    // freed memory being read or written. True when it was, and the page
    // has been made accessible so the guest goes on.
    bool FreedMemoryTouched(uint32_t address);
    void ReportXma();   // the codec contexts, in the periodic report
}

namespace Guest
{
    // Guest memory is big endian. Every read and write of a guest structure
    // goes through these, so the byte order is never left to chance.
    inline uint32_t Read32(const uint8_t* base, uint32_t address)
    {
        return __builtin_bswap32(*reinterpret_cast<const volatile uint32_t*>(base + address));
    }
    // The same compare and exchange the recompiled code uses, so a lock the
    // title takes inline and a lock it takes through the kernel are the same
    // lock.
    inline bool CompareExchange32(uint8_t* base, uint32_t address,
                                  uint32_t expected, uint32_t desired)
    {
        return __sync_bool_compare_and_swap(
            reinterpret_cast<uint32_t*>(base + address),
            __builtin_bswap32(expected), __builtin_bswap32(desired));
    }

    // Adds to a guest word atomically and returns the new value, in the byte
    // order the guest sees.
    inline int32_t AtomicAdd32(uint8_t* base, uint32_t address, int32_t delta)
    {
        uint32_t* word = reinterpret_cast<uint32_t*>(base + address);
        for (;;)
        {
            const uint32_t raw = __atomic_load_n(word, __ATOMIC_ACQUIRE);
            const int32_t next = int32_t(__builtin_bswap32(raw)) + delta;
            if (__sync_bool_compare_and_swap(word, raw,
                    __builtin_bswap32(uint32_t(next))))
                return next;
        }
    }

    inline void Write32(uint8_t* base, uint32_t address, uint32_t value)
    {
        *reinterpret_cast<volatile uint32_t*>(base + address) = __builtin_bswap32(value);
    }
    // A single byte has no order to get wrong, so it goes straight through.
    inline uint8_t Read8(const uint8_t* base, uint32_t address)
    {
        return *reinterpret_cast<const volatile uint8_t*>(base + address);
    }
    inline void Write8(uint8_t* base, uint32_t address, uint8_t value)
    {
        *reinterpret_cast<volatile uint8_t*>(base + address) = value;
    }
    inline uint16_t Read16(const uint8_t* base, uint32_t address)
    {
        return __builtin_bswap16(*reinterpret_cast<const volatile uint16_t*>(base + address));
    }
    inline void Write16(uint8_t* base, uint32_t address, uint16_t value)
    {
        *reinterpret_cast<volatile uint16_t*>(base + address) = __builtin_bswap16(value);
    }
    inline uint64_t Read64(const uint8_t* base, uint32_t address)
    {
        return __builtin_bswap64(*reinterpret_cast<const volatile uint64_t*>(base + address));
    }
    inline void Write64(uint8_t* base, uint32_t address, uint64_t value)
    {
        *reinterpret_cast<volatile uint64_t*>(base + address) = __builtin_bswap64(value);
    }

    // Guest pointers are 32 bit, so the whole 4 GB has to be addressable: the
    // title maps its heap and physical memory well above the image. Reserved
    // in one block, committed a page at a time as it is touched.
    inline constexpr uint64_t TotalSize = 0x100000000ull;

    // PPC_LOOKUP_FUNC hard codes where the indirect call table lives: directly
    // after the image, inside the guest address space rather than beyond it.
    // That region is therefore not available to the guest.
    inline constexpr uint64_t FuncTableBase = PPC_IMAGE_BASE + PPC_IMAGE_SIZE;
    inline constexpr uint64_t FuncTableSize = PPC_CODE_SIZE * 2;
    inline constexpr uint64_t FuncTableEnd  = FuncTableBase + FuncTableSize;

    // Guest stack, placed at the top of the virtual allocation region.
    inline constexpr uint32_t StackBase = 0x70000000;
    inline constexpr uint32_t StackSize = 1u << 20;

    // Where kernel allocations come from. The title asks for virtual memory
    // through Nt* and for physical memory through Mm*, and on the console
    // those live in different parts of the map. Handing out addresses from
    // separate ranges keeps that distinction visible when debugging.
    inline constexpr uint32_t VirtualHeapBase  = 0x40000000;
    inline constexpr uint32_t VirtualHeapLimit = 0x6F000000;   // stops below the stack
    // Physical allocations begin a megabyte in. The console never hands a
    // title physical page zero, and this runtime did: the first thing the
    // title asked for landed at physical offset 0x80, which is where it then
    // kept its job queue. A fence the GPU driver writes to physical 0x100
    // at start up went straight into the second job's descriptor, and the job
    // that ran from it called through whatever it found there.
    inline constexpr uint32_t PhysicalHeapBase  = 0xA0100000;
    inline constexpr uint32_t PhysicalHeapLimit = 0xC0000000;

    extern uint8_t* Base;

    // Where the installed game lives on the host. Guest paths such as
    // game:\sp\global.cod resolve underneath this.
    extern std::filesystem::path GameRoot;

    bool Initialize(const char* xexPath);
    void Shutdown();

    inline uint8_t* Ptr(uint32_t guestAddress) { return Base + guestAddress; }

    // Physical memory is visible through three windows on this hardware, at
    // 0xA0000000, 0xC0000000 and 0xE0000000 plus the physical address, and
    // they are the same bytes: here, three views of one section
    // (Initialize). Where a physical address is readable in this runtime's
    // flat memory.
    // Graphics memory past 512 MB (gpu_address.cpp). The processor's
    // addresses 0x48000000 to 0x6F000000 (above the virtual heap the title
    // allocates from) are graphics memory too, seen by the GPU at 0x20000000
    // to 0x47000000: addresses the Xbox never had, which neither the 512 MB of
    // physical memory nor its windows can mean.
    // 0.5.287: 624 MB (was 0x50000000, 496 MB, all of it the map block with
    // big_memory: 285's mortar archive ran it out on Eder Dam). The title's
    // virtual heap keeps 0x40000000 to 0x48000000 (128 MB; it uses ~16).
    // The GPU's side now reaches past 0x40000000 (bit 30): every place that
    // keeps GPU address bits keeps it (GpuMaskBits, the stream pages, the
    // write watch's pages).
    inline constexpr uint32_t ExtendedCpuBase = 0x48000000, ExtendedCpuLimit = 0x6F000000;
    inline constexpr uint32_t ExtendedShift = 0x28000000;   // processor address minus GPU address
    inline constexpr bool IsExtendedCpu(uint32_t address) { return address >= ExtendedCpuBase && address < ExtendedCpuLimit; }
    inline constexpr bool IsExtendedGpu(uint32_t address) { return address >= ExtendedCpuBase - ExtendedShift && address < ExtendedCpuLimit - ExtendedShift; }

    // An address as the GPU sees it, from a processor's or a GPU's: what the
    // console's `& 0x1FFFFFFF` did, now keeping the extended window.
    inline constexpr uint32_t GpuOffset(uint32_t address)
    {
        if (IsExtendedCpu(address)) return address - ExtendedShift;
        if (IsExtendedGpu(address)) return address;
        return address & 0x1FFFFFFF;
    }

    inline uint32_t PhysicalAlias(uint32_t physicalAddress)
    {
        if (IsExtendedCpu(physicalAddress)) return physicalAddress;
        if (IsExtendedGpu(physicalAddress)) return physicalAddress + ExtendedShift;
        return 0xA0000000u | (physicalAddress & 0x1FFFFFFF);
    }

    // A word the GPU writes at a physical address. The low copy is for the
    // fence the driver puts at physical 0x100 and reads back there.
    inline void WritePhysical32(uint32_t physicalAddress, uint32_t value)
    {
        if (IsExtendedCpu(physicalAddress) || IsExtendedGpu(physicalAddress)) { Write32(Base, PhysicalAlias(physicalAddress), value); return; }
        const uint32_t offset = physicalAddress & 0x1FFFFFFF;
        if (offset < 0x10000) Write32(Base, offset, value);
        Write32(Base, 0xA0000000u | offset, value);
    }

    // A function outside the title's own code: in a level's DLL, when one
    // is loaded (modules.cpp).
    PPCFunc* LookupModule(uint32_t guestAddress);

    // Turns a guest code address into the recompiled function behind it, the
    // same way PPC_LOOKUP_FUNC does inside the translated code. Returns null
    // when nothing was recompiled at that address.
    inline PPCFunc* Lookup(uint32_t guestAddress)
    {
        if (guestAddress < PPC_CODE_BASE || guestAddress >= PPC_CODE_BASE + PPC_CODE_SIZE)
            return LookupModule(guestAddress);
        return *reinterpret_cast<PPCFunc**>(
            Base + FuncTableBase + (uint64_t(guestAddress - PPC_CODE_BASE) * 2));
    }

    // Reserves a stack for a new guest thread and returns the address to put
    // in r1. Zero means the address space is exhausted.
    uint32_t AllocateStack(uint32_t size);

    // A block of physical memory, from the same pool the title is given.
    uint32_t AllocatePhysical(uint32_t size);
    // Graphics memory in the extended window (0 when it is full).
    uint32_t AllocateExtended(uint32_t size);

    // Where KeTimeStampBundle lives, for the clock thread to keep current.
    void SetTimeStampBundle(uint32_t address);

    // The block r13 points at. Guest code reads its thread id, its last error
    // and a millisecond timestamp out of this rather than calling the kernel,
    // so every thread that runs guest code needs one.
    // The processor argument is which of the console's six hardware threads
    // this one runs on. It is not cosmetic: guest code reads it out of the
    // block and uses it as an index.
    uint32_t CreateThreadPointer(uint32_t threadId, int processor = -1);

    // Which hardware thread the caller is on, read out of its own thread block.
    int CurrentProcessor(const PPCContext& ctx);

    // Changes which of the six hardware threads a context says it is on.
    void SetProcessor(const PPCContext& ctx, int processor);

    // A small block of guest memory for a kernel object's dispatcher header.
    // Objects the title reaches through a handle also have to exist as memory,
    // because it can turn a handle into a pointer and wait on that instead.
    uint32_t AllocateKernelObject(uint32_t size);
    void StopThreadClock();

    // Hands a thread pointer block back when its thread has ended.
    void ReleaseThreadPointer(uint32_t block);
}
