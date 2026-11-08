#include "log.h"
#include "kernel.h"
#include "net.h"   // 0.5.428: PublicShutdown
#include "write_watch.h"
#include <unordered_map>
#include "scheduler.h"
#include <atomic>
#include "sampler.h"
#include <utility>
#include <mutex>
#include <shared_mutex>
#include <map>
#include <algorithm>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <Windows.h>

#include <file.h>
#include <image.h>
#include "modules.h"
#include <xex.h>

namespace Guest
{
    uint8_t* Base = nullptr;
    std::filesystem::path GameRoot;
}

namespace
{
    void* g_reservation = nullptr;
    HANDLE g_physicalSection = nullptr;   // the 512 MB behind the three physical windows
    uint64_t g_demandCommitted = 0;

    // Guest memory is reserved in one block but committed lazily. The title
    // allocates through kernel calls that do not exist yet, so there is no way
    // to know in advance which pages it will touch. Faulting a page in on
    // first touch stands in until the Nt and Mm imports do it properly. The
    // limit matches the console's 512 MB, so a runaway pointer stops instead
    // of quietly filling memory with zeroed pages.
    constexpr uint64_t DemandCommitGranularity = 64u << 10;
    // Two gigabytes. Half a gigabyte was the console's own memory and looked
    // like the right ceiling, but physical memory is written through four
    // windows here and each one costs its own pages, so the real figure is
    // several times what the title thinks it is using.
    constexpr uint64_t DemandCommitLimit = 2048ull << 20;

    const char* AccessKind(ULONG_PTR type)
    {
        switch (type)
        {
        case 0:  return "read";
        case 1:  return "write";
        case 8:  return "execute";
        default: return "access";
        }
    }

    thread_local bool t_unwinding = false;

    // What nothing else handled: the exception that is about to end the
    // process, said out loud with its host frames, on the unbuffered stream.
    LONG CALLBACK LastChanceHandler(EXCEPTION_POINTERS* info)
    {
        const auto* record = info->ExceptionRecord;
        const uintptr_t exe = uintptr_t(GetModuleHandleW(nullptr));
        _fflush_nolock(stdout);   // 0.5.370: stdout is buffered now (log.cpp): what came before the crash first (no lock: the crash may hold it)
        fprintf(stderr, "unhandled: code %08lX at %p (exe+%llx) info %llx %p thread %lu\n",
            record->ExceptionCode, record->ExceptionAddress,
            (unsigned long long)(uintptr_t(record->ExceptionAddress) - exe),
            (unsigned long long)record->ExceptionInformation[0],
            (void*)record->ExceptionInformation[1], GetCurrentThreadId());
        CONTEXT walk = *info->ContextRecord;
        fprintf(stderr, "  frames:");
        for (int depth = 0; depth < 24; depth++)
        {
            fprintf(stderr, " %llx", (unsigned long long)(walk.Rip - exe));
            DWORD64 imageBase = 0;
            PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(walk.Rip, &imageBase, nullptr);
            if (entry == nullptr) break;
            void* handlerData = nullptr;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, walk.Rip, entry,
                &walk, &handlerData, &establisher, nullptr);
            if (walk.Rip == 0) break;
        }
        fprintf(stderr, "\n");
        fflush(stdout);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    LONG CALLBACK GuestFaultHandler(EXCEPTION_POINTERS* info)
    {
        const auto* record = info->ExceptionRecord;

        // 0.5.240: the multiplayer program's own first look (the roster trap).
        if (Kernel::FaultHook != nullptr && Kernel::FaultHook(info))
            return EXCEPTION_CONTINUE_EXECUTION;

        // A write to a page the renderer watches (write_watch.cpp): noted,
        // the page made writable, and the write made again.
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->ExceptionInformation[0] == 1 &&
            WriteWatch::HandleFault(reinterpret_cast<const void*>(record->ExceptionInformation[1])))
            return EXCEPTION_CONTINUE_EXECUTION;

        // COD3_RAWFAULTS=1: every access violation, first thing and to the
        // unbuffered stream, with the host frames exe relative for
        // llvm-symbolizer, before anything below can go wrong. Demand
        // commits are access violations too, so this is for a run that is
        // dying without a word, not for reading in general.
        static const bool rawFaults = []() {
            const char* text = getenv("COD3_RAWFAULTS");
            return text != nullptr && text[0] != 0 && text[0] != '0';
        }();
        if (rawFaults && record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION)
        {
            static std::atomic<int> raw{ 0 };
            if ((raw.load(std::memory_order_relaxed) < 400 && raw.fetch_add(1) < 400))
                fprintf(stderr, "fault: code %08lX kind %llu at %p (exe+%llx) address %p thread %lu unwinding %d guest ctx %p\n",
                    record->ExceptionCode, (unsigned long long)record->ExceptionInformation[0],
                    record->ExceptionAddress,
                    (unsigned long long)(uintptr_t(record->ExceptionAddress) - uintptr_t(GetModuleHandleW(nullptr))),
                    (void*)record->ExceptionInformation[1],
                    GetCurrentThreadId(), t_unwinding ? 1 : 0, (void*)Kernel::CurrentContext());
                // The host frames, exe relative, unwound without dbghelp.
                CONTEXT walk = *info->ContextRecord;
                const uintptr_t exe = uintptr_t(GetModuleHandleW(nullptr));
                fprintf(stderr, "  frames:");
                for (int depth = 0; depth < 24; depth++)
                {
                    fprintf(stderr, " %llx", (unsigned long long)(walk.Rip - exe));
                    DWORD64 imageBase = 0;
                    PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(walk.Rip, &imageBase, nullptr);
                    if (entry == nullptr) break;
                    void* handlerData = nullptr;
                    DWORD64 establisher = 0;
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, walk.Rip, entry,
                        &walk, &handlerData, &establisher, nullptr);
                    if (walk.Rip == 0) break;
                }
                fprintf(stderr, "  rsp %p guest base %p\n", (void*)info->ContextRecord->Rsp, (void*)Guest::Base);
        }

        // A sampler or the watchdog walking a running thread's stack from a
        // snapshot of its registers can read through a stale pointer. That
        // is theirs to catch, not a guest fault.
        if (t_unwinding && record->ExceptionCode != EXCEPTION_SINGLE_STEP)
            return EXCEPTION_CONTINUE_SEARCH;

        // A hardware watch fires as a single step. The processor has already
        // done the write, so reporting and continuing is all that is wanted.
        if (record->ExceptionCode == EXCEPTION_SINGLE_STEP)
        {
            if (Kernel::ReportWatchpoint(info->ContextRecord))
                return EXCEPTION_CONTINUE_EXECUTION;
            return EXCEPTION_CONTINUE_SEARCH;
        }

        // Anything else that reaches here kills the process without a word,
        // which is the worst way for a run to end: no message, no stack, no
        // idea. Saying what it was and where costs nothing and is often the
        // whole answer.
        if (record->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || Guest::Base == nullptr)
        {
            static std::atomic<int> announced{ 0 };
            if ((record->ExceptionCode & 0x20000000) == 0 &&
                (announced.load(std::memory_order_relaxed) < 4 && announced.fetch_add(1) < 4))
            {
                printf("\n");
                uint32_t functions[12] = {};
                const int count =
                    Sampler::WalkGuestStack(info->ContextRecord, functions, 12);
                if (count > 0)
                {
                    printf("  guest call chain, innermost first:");
                    for (int i = 0; i < count; i++)
                        printf("%ssub_%08X", i == 0 ? " " : " <- ", functions[i]);
                    printf("\n");
                }

                const char* name = "an exception";
                switch (record->ExceptionCode)
                {
                case EXCEPTION_STACK_OVERFLOW:    name = "a stack overflow"; break;
                case EXCEPTION_ILLEGAL_INSTRUCTION: name = "an illegal instruction"; break;
                case EXCEPTION_INT_DIVIDE_BY_ZERO: name = "a divide by zero"; break;
                case EXCEPTION_PRIV_INSTRUCTION:  name = "a privileged instruction"; break;
                case EXCEPTION_IN_PAGE_ERROR:     name = "an in page error"; break;
                case 0xE06D7363:                  name = "a C++ throw"; break;
                default: break;
                }
                printf("Unhandled: %s, code 0x%08lX at %p\n", name,
                    record->ExceptionCode, record->ExceptionAddress);

                // What was thrown.
                //
                // A C++ throw carries a description of the type with it, and
                // that description ends in the type's own name. Following it
                // turns "the game raised an exception" into the sentence the
                // programmer wrote, which is the difference between guessing
                // at a dirty disc and knowing what was actually wrong.
                if (record->ExceptionCode == 0xE06D7363 &&
                    record->NumberParameters >= 3)
                {
                    const uint32_t throwInfo =
                        uint32_t(record->ExceptionInformation[2]);
                    if (throwInfo >= 0x82000000u && throwInfo < 0x82C30000u)
                    {
                        const uint32_t array =
                            Guest::Read32(Guest::Base, throwInfo + 12);
                        if (array >= 0x82000000u && array < 0x82C30000u)
                        {
                            const uint32_t count =
                                Guest::Read32(Guest::Base, array + 0);
                            for (uint32_t i = 0; i < count && i < 4; i++)
                            {
                                const uint32_t catchable =
                                    Guest::Read32(Guest::Base, array + 4 + i * 4);
                                if (catchable < 0x82000000u ||
                                    catchable >= 0x82C30000u) continue;
                                const uint32_t descriptor =
                                    Guest::Read32(Guest::Base, catchable + 4);
                                if (descriptor < 0x82000000u ||
                                    descriptor >= 0x82C30000u) continue;

                                char text[128];
                                uint32_t at = 0;
                                for (; at + 1 < sizeof(text); at++)
                                {
                                    const uint8_t byte = Guest::Read8(
                                        Guest::Base, descriptor + 8 + at);
                                    if (byte == 0) break;
                                    text[at] = char(byte);
                                }
                                text[at] = 0;
                                if (at > 0)
                                    printf("  what was thrown: %s\n", text);
                            }
                        }
                    }
                }
                fflush(stdout);
            }
            return EXCEPTION_CONTINUE_SEARCH;
        }

        const auto faulting = static_cast<uint8_t*>(
            reinterpret_cast<void*>(record->ExceptionInformation[1]));

        if (faulting < Guest::Base || faulting >= Guest::Base + Guest::TotalSize)
        {
            // Outside guest memory entirely. The usual cause is an indirect
            // call through a table entry that was never filled in, which lands
            // on a null pointer.
            printf("\n");
            {
                uint32_t functions[12] = {};
                const int count =
                    Sampler::WalkGuestStack(info->ContextRecord, functions, 12);
                if (count > 0)
                {
                    printf("\n  guest call chain, innermost first:");
                    for (int i = 0; i < count; i++)
                        printf("%ssub_%08X", i == 0 ? " " : " <- ", functions[i]);
                    printf("\n");
                }
            }
            printf("Host fault: %s at %p, outside the guest address space.\n"
                   "  guest memory runs from %p to %p\n",
                AccessKind(record->ExceptionInformation[0]), (void*)faulting,
                (void*)Guest::Base, (void*)(Guest::Base + Guest::TotalSize));

            Kernel::DumpRequested();

            // The guest registers at the fault: whichever one is the small
            // negative number is the one that was handed an error code.
            if (const PPCContext* guest = Kernel::CurrentContext())
            {
                printf("  guest registers r0 to r31, lr 0x%08X:", uint32_t(guest->lr));
                for (int i = 0; i < 32; i++)
                {
                    if ((i % 8) == 0) printf("\n   r%-2d", i);
                    printf(" %08X", Kernel::Register(*guest, i));
                }
                printf("\n");
            }
            // An indirect call reads the function table, which sits at a fixed
            // offset from the base, so the address it read says which guest
            // address it was looking for. A fault below the base is that
            // arithmetic going negative, which only happens for an address
            // below the start of the code: nearly always zero.
            const int64_t offset = int64_t(faulting - Guest::Base);

            // Just past the end of guest memory is a guest address near
            // 0xFFFFFFFF, which is a small negative number being used as a
            // pointer: an error code returned where an object was expected.
            if (offset >= int64_t(Guest::TotalSize) &&
                offset < int64_t(Guest::TotalSize) + 0x1000)
            {
                printf("  That is the very top of the address space, reached by an "
                       "access that started just below 0xFFFFFFFF: a small\n"
                       "  negative number used as a pointer, so something handed back\n"
                       "  an error code where the caller expected an object.\n");
            }

            const int64_t tableStart = int64_t(Guest::FuncTableBase);
            const int64_t implied =
                int64_t(PPC_CODE_BASE) + ((offset - tableStart) / 2);
            if (implied >= -0x1000 && implied < int64_t(PPC_CODE_BASE) + 0x1000000)
            {
                printf("  This is the function table being read for guest address "
                       "0x%08X.\n", uint32_t(implied));
                if (implied == 0)
                {
                    printf("  The guest called through a null function pointer: an "
                           "object it\n  expected to be set up was not. The call "
                           "chain above says which one.\n");
                }
                else
                {
                    printf("  No recompiled function stands behind that address.\n");
                }
            }
            else if (reinterpret_cast<uintptr_t>(faulting) < 0x10000)
            {
                printf("  A near null address on an %s means an indirect call through\n",
                    AccessKind(record->ExceptionInformation[0]));
                printf("  an empty slot in the function table: the guest asked for a\n");
                printf("  code address that has no recompiled function behind it.\n");
            }
            printf("  %.1f MB of guest memory had been committed on demand.\n",
                g_demandCommitted / 1048576.0);
            Kernel::PrintHostStack(info->ContextRecord);
            fflush(stdout);
            return EXCEPTION_CONTINUE_SEARCH;
        }

        const uint64_t offset = static_cast<uint64_t>(faulting - Guest::Base);
        const uint64_t block = offset & ~(DemandCommitGranularity - 1);

        // Freed memory touched: the title using what it gave back. Said
        // with the reader's call chain, the first dozen times, and let
        // through, since with the memory's contents kept it went unnoticed.
        if (Kernel::FreedMemoryTouched(uint32_t(offset)))
        {
            static std::atomic<int> announced{ 0 };
            if ((announced.load(std::memory_order_relaxed) < 12 && announced.fetch_add(1) < 12))
            {
                uint32_t functions[10] = {};
                const int count = Sampler::WalkGuestStack(info->ContextRecord, functions, 10);
                printf("memory: freed memory %s at 0x%08X;", AccessKind(record->ExceptionInformation[0]), uint32_t(offset));
                if (count > 0)
                {
                    printf(" from");
                    for (int i = 0; i < count; i++) printf(" sub_%08X", functions[i]);
                }
                printf("\n");
                fflush(stdout);
            }
            return EXCEPTION_CONTINUE_EXECUTION;
        }
        // In one of the heaps, outside everything they handed out, and never
        // committed: a pointer the title kept from somewhere, said with its
        // reader (the dozen commits below are used up by a level's load).
        {
            const uint32_t guest = uint32_t(offset);
            const bool inHeap = (guest >= Guest::PhysicalHeapBase && guest < Guest::PhysicalHeapLimit) ||
                                (guest >= Guest::VirtualHeapBase && guest < Guest::VirtualHeapLimit);
            static std::atomic<int> strays{ 0 };
            if (inHeap && (strays.load(std::memory_order_relaxed) < 16 && strays.fetch_add(1) < 16))
            {
                uint32_t functions[10] = {};
                const int count = Sampler::WalkGuestStack(info->ContextRecord, functions, 10);
                printf("memory: %s of heap memory never handed out, at 0x%08X;", AccessKind(record->ExceptionInformation[0]), guest);
                if (count > 0)
                {
                    printf(" from");
                    for (int i = 0; i < count; i++) printf(" sub_%08X", functions[i]);
                }
                printf("\n");
                fflush(stdout);
            }
        }

        // The first page of the address space is not memory on this console,
        // and an access there is a null pointer being followed. Committing it
        // on demand hands back zeros and lets the guest carry on with a null
        // object, which it then does for a long time before dying somewhere
        // that says nothing about the cause. Refusing it puts the report at
        // the first mistake instead of the last symptom.
        // Where the guest touches memory nothing handed it. Everything the
        // allocators give out is committed as it is given, so a commit here is
        // the guest reaching somewhere else: the console's memory windows,
        // where the same physical page has several addresses, or plain
        // garbage. Which one is the question, and the region and the caller
        // answer it.
        {
            static std::atomic<int> announced{ 0 };
            const uint32_t guest = uint32_t(offset);
            const bool inImage = guest >= 0x82000000u && guest < uint32_t(Guest::FuncTableEnd);
            const bool inHeaps = (guest >= 0x3F000000u && guest < 0x70000000u);
            // A run through memory: hundreds of megabytes committed on demand
            // is a pointer gone wild, and the thread it is on is named.
            static std::atomic<int> runaway{ 0 };
            const bool running = g_demandCommitted > (256ull << 20) && (runaway.load(std::memory_order_relaxed) < 12 && runaway.fetch_add(1) < 12);
            if ((!inImage && !inHeaps && (announced.load(std::memory_order_relaxed) < 12 && announced.fetch_add(1) < 12)) || running)
            {
                uint32_t functions[8] = {};
                const int count = Sampler::WalkGuestStack(info->ContextRecord, functions, 8);
                printf("commit: guest %s at 0x%08X, %s;",
                    AccessKind(record->ExceptionInformation[0]), guest,
                    running ? "with hundreds of megabytes already committed on demand" : "outside every allocation");
                if (count > 0)
                {
                    printf(" from");
                    for (int i = 0; i < count; i++) printf(" sub_%08X", functions[i]);
                }
                printf("\n");
                fflush(stdout);
            }
        }

        if (g_demandCommitted < DemandCommitLimit)
        {
            // Sixty four kilobytes at a time, because that is the granularity
            // the console hands memory out in and it keeps the number of
            // commits down. A block that straddles something already mapped
            // differently is refused whole, so the page on its own is tried
            // next: the aim is to keep the guest running, not to be tidy.
            if (VirtualAlloc(Guest::Base + block, DemandCommitGranularity,
                             MEM_COMMIT, PAGE_READWRITE) != nullptr)
            {
                g_demandCommitted += DemandCommitGranularity;
                return EXCEPTION_CONTINUE_EXECUTION;
            }

            const uint64_t page = offset & ~uint64_t(0xFFF);
            if (VirtualAlloc(Guest::Base + page, 0x1000,
                             MEM_COMMIT, PAGE_READWRITE) != nullptr)
            {
                g_demandCommitted += 0x1000;
                return EXCEPTION_CONTINUE_EXECUTION;
            }
        }

        printf("\n");
        printf("Guest fault: %s at guest address 0x%08llX, which could not be "
               "committed (error %lu)\n",
            AccessKind(record->ExceptionInformation[0]),
            (unsigned long long)offset, GetLastError());
        printf("  %.1f MB had already been committed on demand.\n",
            g_demandCommitted / 1048576.0);
        fflush(stdout);
        return EXCEPTION_CONTINUE_SEARCH;
    }

    // The first pages are physical memory on this console, and the graphics
    // driver puts fences there: one of the first is written to physical
    // address 0x100. Committing them up front keeps that off the fault path.
    void CommitLowMemory()
    {
        VirtualAlloc(Guest::Base, 0x10000, MEM_COMMIT, PAGE_READWRITE);
    }

    bool Commit(uint64_t offset, uint64_t size, const char* what)
    {
        // Round out to page boundaries so adjacent commits cannot leave a hole.
        const uint64_t begin = offset & ~uint64_t(0xFFF);
        const uint64_t end = (offset + size + 0xFFF) & ~uint64_t(0xFFF);

        if (VirtualAlloc(Guest::Base + begin, end - begin, MEM_COMMIT, PAGE_READWRITE) == nullptr)
        {
            fprintf(stderr, "  failed to commit %s at 0x%llX (%llu bytes): error %lu\n",
                what, (unsigned long long)begin, (unsigned long long)(end - begin), GetLastError());
            return false;
        }
        return true;
    }
}

extern std::unordered_map<size_t, const char*> XboxKernelExports;
extern std::unordered_map<size_t, const char*> XamExports;

namespace
{
    // The kernel's exported variables.
    //
    // A XEX imports two kinds of thing from the kernel: functions, which the
    // recompiler turns into calls to this runtime, and variables, which are
    // addresses of data the kernel owns. XenonUtils binds the first kind and
    // leaves the second untouched, so every slot that should have held the
    // address of a kernel variable held whatever the file had there, and the
    // title read those as pointers: the C runtime's start up read from
    // 0x93010000, and the graphics driver's initialisation dereferenced a
    // word in the import area and found nothing behind it.
    //
    // Each variable gets a block of guest memory of its own here, and the
    // ones whose shape is known get the value the console would hold. The
    // time stamp bundle is kept current by the clock thread.
    // The kind of each export, function or variable, read off the same tables
    // XenonUtils builds its names from. XenonUtils keeps the names and drops
    // the kind; the kind is what decides what goes into the slot.
    enum ExportKind { kFunction, kVariable };
    struct ExportEntry { uint32_t ordinal; ExportKind kind; };
#define XE_EXPORT(module, ordinal, name, kind) { uint32_t(ordinal), kind }
    const ExportEntry kKernelExportKinds[] = {
#include <xbox/xboxkrnl_table.inc>
    };
    const ExportEntry kXamExportKinds[] = {
#include <xbox/xam_table.inc>
    };
#undef XE_EXPORT

    bool SkipVariableValue(const char* name)
    {
        const char* list = getenv("COD3_NOVAR");
        if (list == nullptr) return false;
        const size_t length = strlen(name);
        for (const char* at = list; *at != 0;)
        {
            const char* end = strchr(at, ',');
            if (end == nullptr) end = at + strlen(at);
            if (size_t(end - at) == length && strncmp(at, name, length) == 0) return true;
            at = *end == ',' ? end + 1 : end;
        }
        return false;
    }

    bool IsVariableExport(const std::string& library, uint32_t ordinal)
    {
        const ExportEntry* table = nullptr;
        size_t count = 0;
        if (library == "xboxkrnl.exe") { table = kKernelExportKinds; count = sizeof(kKernelExportKinds) / sizeof(kKernelExportKinds[0]); }
        else if (library == "xam.xex") { table = kXamExportKinds; count = sizeof(kXamExportKinds) / sizeof(kXamExportKinds[0]); }
        for (size_t i = 0; i < count; i++)
            if (table[i].ordinal == ordinal) return table[i].kind == kVariable;
        return false;
    }

    struct KnownVariable
    {
        const char* name;
        uint32_t size;
    };

    const KnownVariable kKnownVariables[] = {
        { "XboxHardwareInfo",          0x40 },
        { "KeTimeStampBundle",         0x20 },
        { "ExLoadedCommandLine",       0x400 },
        { "XexExecutableModuleHandle", 0x100 },
        { "KeDebugMonitorData",        0x40 },
        { "VdGlobalDevice",            0x10 },
        { "VdGlobalXamDevice",         0x10 },
        { "VdGpuClockInMHz",           0x10 },
        { "VdHSIOCalibrationLock",     0x40 },
        { "ExConsoleGameRegion",       0x10 },
        { "XboxKrnlVersion",           0x10 },
        { "KeCertMonitorData",         0x40 },
    };

    void BindVariableImports(const uint8_t* file)
    {
        const auto* header = reinterpret_cast<const Xex2ImportHeader*>(
            getOptHeaderPtr(file, XEX_HEADER_IMPORT_LIBRARIES));
        if (header == nullptr) return;

        std::vector<std::string> libraries;
        const char* strings = reinterpret_cast<const char*>(header + 1);
        size_t at = 0;
        for (uint32_t i = 0; i < header->numImports; i++)
        {
            libraries.emplace_back(strings + at);
            at += ((libraries.back().size() + 1) + 3) & ~size_t(3);
        }

        // Sixty four kilobytes for all of them, carved in order.
        const uint32_t block = Guest::AllocatePhysical(64u << 10);
        if (block == 0) return;
        uint32_t next = block;
        auto Carve = [&](uint32_t size) {
            const uint32_t address = next;
            next += (size + 15) & ~15u;
            return address;
        };

        size_t bound = 0;
        size_t functionSlots = 0;
        const auto* library = reinterpret_cast<const Xex2ImportLibrary*>(
            reinterpret_cast<const char*>(header) + sizeof(Xex2ImportHeader) + header->sizeOfStringTable);
        for (size_t i = 0; i < libraries.size(); i++)
        {
            const auto* descriptors = reinterpret_cast<const Xex2ImportDescriptor*>(library + 1);
            const std::unordered_map<size_t, const char*>* names = nullptr;
            if (libraries[i] == "xboxkrnl.exe") names = &XboxKernelExports;
            else if (libraries[i] == "xam.xex") names = &XamExports;

            // ParseImage replaced the code of every thunk with three nops and
            // a blr, and left the word in each slot swapped to host order. The
            // thunks are told apart by that replacement, because what they
            // used to hold is gone: read as a slot word, a thunk's first nop
            // says type 0, ordinal 96, and passed for an import of its own.
            auto IsThunk = [](uint32_t address) {
                static const uint32_t pattern[4] = { 0x00000060, 0x00000060, 0x00000060, 0x2000804E };
                return memcmp(Guest::Base + address, pattern, sizeof(pattern)) == 0;
            };

            for (uint32_t im = 0; im < library->numberOfImports; im++)
            {
                const uint32_t slot = descriptors[im].firstThunk;
                if (slot < PPC_IMAGE_BASE || slot >= PPC_IMAGE_BASE + PPC_IMAGE_SIZE) continue;
                if (IsThunk(slot)) continue;

                uint32_t raw;
                memcpy(&raw, Guest::Base + slot, sizeof(raw));
                const uint32_t type = raw >> 24;
                const uint32_t ordinal = raw & 0xFFFF;
                if (type != 0) continue;

                const char* name = nullptr;
                if (names != nullptr)
                {
                    auto found = names->find(ordinal);
                    if (found != names->end()) name = found->second;
                }
                // The tables name them as import symbols, prefix and all.
                if (name != nullptr && strncmp(name, "__imp__", 7) == 0) name += 7;

                // Every import has one of these slots, functions included: it
                // is the word the title reads to call through a pointer. For
                // a function the slot gets the address of its thunk, which is
                // the next entry in the table and which the function binding
                // below points at this runtime. Only a variable gets memory.
                if (!IsVariableExport(libraries[i], ordinal))
                {
                    // Its thunk is the entry after it, and only if that entry
                    // really is one: a function with no thunk keeps its slot
                    // as it was rather than pointing at somebody else's.
                    if (im + 1 < library->numberOfImports)
                    {
                        const uint32_t thunk = descriptors[im + 1].firstThunk;
                        if (thunk >= PPC_CODE_BASE && thunk < PPC_CODE_BASE + PPC_CODE_SIZE && IsThunk(thunk))
                        {
                            Guest::Write32(Guest::Base, slot, thunk);
                            functionSlots++;
                        }
                        else
                        {
                            printf("  import %s (%s ordinal %u) at 0x%08X has no thunk after it\n",
                                name ? name : "?", libraries[i].c_str(), ordinal, slot);
                        }
                    }
                    continue;
                }

                uint32_t size = 0x10;
                for (const KnownVariable& known : kKnownVariables)
                    if (name != nullptr && strcmp(name, known.name) == 0) size = known.size;

                const uint32_t variable = Carve(size);
                Guest::Write32(Guest::Base, slot, variable);
                bound++;

                // COD3_NOVAR=name,name leaves those variables allocated but
                // zero, to tell which value changes the title's behaviour.
                if (name != nullptr && !SkipVariableValue(name))
                {
                    if (strcmp(name, "XboxHardwareInfo") == 0)
                    {
                        Guest::Write32(Guest::Base, variable + 0, 0);      // flags: retail
                        *(Guest::Base + variable + 4) = 6;                  // hardware threads
                    }
                    else if (strcmp(name, "ExLoadedCommandLine") == 0)
                    {
#ifdef COD3_MP
                        const char* line = "codmp_xenonf.xex";
#else
                        const char* line = "default.xex";
#endif
                        memcpy(Guest::Base + variable, line, strlen(line) + 1);
                    }
                    else if (strcmp(name, "VdGpuClockInMHz") == 0)
                    {
                        Guest::Write32(Guest::Base, variable, 500);
                    }
                    else if (strcmp(name, "XboxKrnlVersion") == 0)
                    {
                        // Major, minor, build and QFE, sixteen bits each:
                        // 2.0.17559.0, the last retail kernel.
                        Guest::Write16(Guest::Base, variable + 0, 2);
                        Guest::Write16(Guest::Base, variable + 2, 0);
                        Guest::Write16(Guest::Base, variable + 4, 17559);
                        Guest::Write16(Guest::Base, variable + 6, 0);
                    }
                    else if (strcmp(name, "ExConsoleGameRegion") == 0)
                    {
                        Guest::Write32(Guest::Base, variable, 0xFFFFFFFFu);
                    }
                    else if (strcmp(name, "KeTimeStampBundle") == 0)
                    {
                        Guest::SetTimeStampBundle(variable);
                    }
                    else if (strcmp(name, "XexExecutableModuleHandle") == 0)
                    {
                        // Points at a module record; the record's first word
                        // is the image base, which is what is usually wanted.
                        const uint32_t record = Carve(0x100);
                        Guest::Write32(Guest::Base, variable, record);
                        Guest::Write32(Guest::Base, record + 0, uint32_t(PPC_IMAGE_BASE));
                    }
                }

                static int announced = 0;
                if ((announced < 24 && announced++ < 24))
                    printf("  variable import %s (%s ordinal %u) at 0x%08X -> 0x%08X\n",
                        name ? name : "?", libraries[i].c_str(), ordinal, slot, variable);
            }
            library = reinterpret_cast<const Xex2ImportLibrary*>(
                reinterpret_cast<const char*>(library + 1) +
                library->numberOfImports * sizeof(Xex2ImportDescriptor));
        }
        printf("  %zu variable imports bound to guest memory, %zu function slots "
               "point at their thunks\n", bound, functionSlots);
    }
}

namespace
{
    const Kernel::Import* g_imports = nullptr;
    size_t g_importCount = 0;
}

PPCFunc* Kernel::FindImport(const char* name)
{
    for (size_t i = 0; i < g_importCount; i++)
        if (strcmp(g_imports[i].name, name) == 0) return g_imports[i].host;
    return nullptr;
}

PPCFunc* Guest::LookupModule(uint32_t guestAddress)
{
    return Modules::Lookup(guestAddress);
}

bool Guest::Initialize(const char* xexPath)
{
    printf("Guest address space\n");
    printf("  image        0x%08llX .. 0x%08llX\n",
        (unsigned long long)PPC_IMAGE_BASE,
        (unsigned long long)(PPC_IMAGE_BASE + PPC_IMAGE_SIZE));
    printf("  code         0x%08llX .. 0x%08llX\n",
        (unsigned long long)PPC_CODE_BASE,
        (unsigned long long)(PPC_CODE_BASE + PPC_CODE_SIZE));
    printf("  call table   0x%08llX .. 0x%08llX\n",
        (unsigned long long)FuncTableBase, (unsigned long long)FuncTableEnd);
    printf("  reserving    %.0f GB of address space\n", TotalSize / 1073741824.0);

    // One contiguous reservation covering the whole 32 bit guest range, so any
    // guest pointer resolves to base + address. Reserve everything, commit
    // only what is actually touched: committing 4 GB up front would cost real
    // memory for pages the guest never uses.
    //
    // Physical memory is reachable through three windows on this console, at
    // 0xA0000000, 0xC0000000 and 0xE0000000 plus the physical address, and a
    // write through one is visible through all of them. The title relies on
    // that: its render threads write command lists through the first window
    // and the thread that replays them reads them through the second, and
    // with the windows as separate storage the replay read zeros, called a
    // null handler for every entry and never fed the GPU a thing, which is
    // where every level stopped. So the three windows are three views of one
    // section. The reservation is made as a placeholder, which can be split
    // and have views put into it without ever giving the address range back:
    // releasing and re-reserving was tried before and lost the range.
    static const auto virtualAlloc2 = reinterpret_cast<PVOID (WINAPI*)(
        HANDLE, PVOID, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG)>(
        GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "VirtualAlloc2"));
    static const auto mapViewOfFile3 = reinterpret_cast<PVOID (WINAPI*)(
        HANDLE, HANDLE, PVOID, ULONG64, SIZE_T, ULONG, ULONG, MEM_EXTENDED_PARAMETER*, ULONG)>(
        GetProcAddress(GetModuleHandleW(L"kernelbase.dll"), "MapViewOfFile3"));
    if (virtualAlloc2 == nullptr || mapViewOfFile3 == nullptr)
    {
        fprintf(stderr, "  this Windows has no VirtualAlloc2 or MapViewOfFile3\n");
        return false;
    }
    g_reservation = virtualAlloc2(nullptr, nullptr, TotalSize,
        MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
    if (g_reservation == nullptr)
    {
        fprintf(stderr, "  reservation failed: error %lu\n", GetLastError());
        return false;
    }
    Base = static_cast<uint8_t*>(g_reservation);

    constexpr uint64_t WindowSize = 0x20000000;   // 512 MB of physical memory
    constexpr uint64_t Windows[] = { 0xA0000000, 0xC0000000, 0xE0000000 };
    g_physicalSection = CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr,
        PAGE_READWRITE | SEC_RESERVE, DWORD(WindowSize >> 32), DWORD(WindowSize), nullptr);
    if (g_physicalSection == nullptr)
    {
        fprintf(stderr, "  the physical memory section could not be made: error %lu\n",
            GetLastError());
        return false;
    }
    // Split the placeholder at each window, so that each is a placeholder
    // of its own and can be replaced by a view. Splitting off the tail first
    // keeps every piece a single range.
    for (uint64_t window : Windows)
    {
        // The last window reaches the end of the range: once the one before
        // it is split off it is a placeholder of its own already.
        if (window + WindowSize == TotalSize) continue;
        if (!VirtualFree(Base + window, WindowSize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER))
        {
            fprintf(stderr, "  the placeholder could not be split at 0x%llX: error %lu\n",
                (unsigned long long)window, GetLastError());
            return false;
        }
    }
    for (uint64_t window : Windows)
    {
        if (mapViewOfFile3(g_physicalSection, GetCurrentProcess(), Base + window, 0, WindowSize,
                           MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr, 0) == nullptr)
        {
            fprintf(stderr, "  the physical window at 0x%llX could not be mapped: error %lu\n",
                (unsigned long long)window, GetLastError());
            return false;
        }
    }
    // The rest of the range, below the windows, becomes an ordinary
    // reservation that pages are committed into as they are touched.
    if (virtualAlloc2(GetCurrentProcess(), Base, Windows[0],
                      MEM_RESERVE | MEM_REPLACE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0) == nullptr)
    {
        fprintf(stderr, "  the reservation below the windows failed: error %lu\n", GetLastError());
        return false;
    }

    // PPC_FUNC_PROLOGUE asserts this, and the vector loads assume it.
    if ((reinterpret_cast<uintptr_t>(Base) & 0x1F) != 0)
    {
        fprintf(stderr, "  base %p is not 32 byte aligned\n", Base);
        return false;
    }

    AddVectoredExceptionHandler(1, GuestFaultHandler);
    SetUnhandledExceptionFilter(LastChanceHandler);
    // An abort (a C++ exception nobody caught, a runtime check) says where
    // it was, which the fast fail that follows it does not.
    signal(SIGABRT, [](int) {
        CONTEXT context{};
        RtlCaptureContext(&context);
        printf("\nabort() called on thread %lu\n", GetCurrentThreadId());
        Kernel::PrintHostStack(&context);
        fflush(stdout);
    });
    CommitLowMemory();

    if (!Commit(PPC_IMAGE_BASE, PPC_IMAGE_SIZE, "image")) return false;
    if (!Commit(FuncTableBase, FuncTableSize, "function table")) return false;
    if (!Commit(StackBase - StackSize, StackSize, "stack")) return false;

    // --- Load the executable ---------------------------------------------
    printf("\nLoading %s\n", xexPath);
    const auto file = LoadFile(xexPath);
    if (file.empty())
    {
        fprintf(stderr, "  cannot read the file\n");
        return false;
    }

    auto image = Image::ParseImage(file.data(), file.size());
    printf("  entry point 0x%llX\n", (unsigned long long)image.entry_point);

    // The recompiled code reads guest data (string literals, tables, vtables)
    // straight out of this memory, so every section has to land at its own
    // virtual address, big-endian bytes untouched.
    size_t mapped = 0;
    size_t skipped = 0;
    for (const auto& section : image.sections)
    {
        // The indirect call table sits immediately after the image, and
        // .reloc runs into it. Skipping .reloc is safe: base relocations are
        // applied only by a loader placing an image somewhere other than its
        // preferred base, and this one is at its preferred base by
        // construction. Any other section landing there is a real problem and
        // must not be dropped quietly.
        if (section.base < FuncTableEnd && section.base + section.size > FuncTableBase)
        {
            if (section.name == ".reloc")
            {
                skipped++;
                continue;
            }
            fprintf(stderr, "  section %s (0x%zX .. 0x%zX) overlaps the call table "
                            "at 0x%llX\n",
                section.name.c_str(), section.base, section.base + section.size,
                (unsigned long long)FuncTableBase);
            return false;
        }
        if (section.base + section.size > TotalSize)
        {
            fprintf(stderr, "  section %s ends at 0x%zX, past the guest address space\n",
                section.name.c_str(), section.base + section.size);
            return false;
        }
        memcpy(Base + section.base, section.data, section.size);
        mapped += section.size;
    }
    printf("  mapped %zu sections, %.2f MB", image.sections.size() - skipped, mapped / 1048576.0);
    if (skipped > 0) printf(" (.reloc not needed, skipped)");
    printf("\n");

    // --- Indirect call table ----------------------------------------------
    // PPC_LOOKUP_FUNC turns a guest code address into a host function pointer
    // by indexing this table at (address - PPC_CODE_BASE) * 2.
    BindVariableImports(file.data());

    size_t entries = 0;
    for (size_t i = 0; PPCFuncMappings[i].guest != 0; i++)
    {
        const size_t guest = PPCFuncMappings[i].guest;
        if (guest < PPC_CODE_BASE || guest >= PPC_CODE_BASE + PPC_CODE_SIZE)
        {
            fprintf(stderr, "  function 0x%zX is outside the code range\n", guest);
            return false;
        }
        auto** slot = reinterpret_cast<PPCFunc**>(
            Base + FuncTableBase + (uint64_t(uint32_t(guest) - PPC_CODE_BASE) * 2));
        *slot = PPCFuncMappings[i].host;
        entries++;
    }
    printf("  %zu functions in the indirect call table\n", entries);

    // Import thunks are not recompiled functions, so they are absent from
    // PPCFuncMappings. A direct branch to one is turned into a call to the
    // host stub at recompile time, but a call through a function pointer goes
    // through the table at run time and would find a null entry. Fill those in
    // by name, using the symbols the XEX itself carries.
    static const Kernel::Import kImports[] = {
#include "kernel_import_table.inc"
    };
    g_imports = kImports;
    g_importCount = sizeof(kImports) / sizeof(kImports[0]);

    size_t importsBound = 0;
    size_t importsUnbound = 0;
    for (const auto& symbol : image.symbols)
    {
        if (symbol.name.empty()) continue;

        std::string name = symbol.name;
        const std::string prefix = "__imp__";
        if (name.rfind(prefix, 0) == 0) name = name.substr(prefix.size());

        PPCFunc* host = nullptr;
        for (const auto& import : kImports)
        {
            if (name == import.name) { host = import.host; break; }
        }

        if (host == nullptr)
        {
            fprintf(stderr, "  no host stub for import %s\n", name.c_str());
            importsUnbound++;
            continue;
        }
        if (symbol.address < PPC_CODE_BASE || symbol.address >= PPC_CODE_BASE + PPC_CODE_SIZE)
        {
            fprintf(stderr, "  import %s at 0x%zX is outside the code range\n",
                name.c_str(), symbol.address);
            importsUnbound++;
            continue;
        }

        auto** slot = reinterpret_cast<PPCFunc**>(
            Base + FuncTableBase + (uint64_t(uint32_t(symbol.address) - PPC_CODE_BASE) * 2));
        *slot = host;
        importsBound++;
    }
    printf("  %zu kernel imports bound", importsBound);
    if (importsUnbound > 0) printf(", %zu could not be bound", importsUnbound);
    printf("\n");

    return true;
}

// Gives the address space back.
//
// Only safe when nothing else is running in it. A thread that fails does not
// get to do this: every other thread would then fault on memory that is no
// longer there, each fault reported as one that could not be committed, and the
// message that actually mattered scrolls away. Report and stop instead.
void Guest::Shutdown()
{
    if (g_reservation != nullptr)
    {
        for (uint64_t window : { 0xA0000000ull, 0xC0000000ull, 0xE0000000ull })
            UnmapViewOfFile(Base + window);
        VirtualFree(g_reservation, 0, MEM_RELEASE);
        g_reservation = nullptr;
        if (g_physicalSection != nullptr) CloseHandle(g_physicalSection);
        g_physicalSection = nullptr;
        Base = nullptr;
    }
}

Kernel::Counters& Kernel::Stats()
{
    static Counters counters;
    return counters;
}

void Kernel::Exit(int code)
{
    fprintf(stderr, "exit: code %d (0x%X) on thread %lu\n", code, unsigned(code), GetCurrentThreadId());
    // GetConsoleProcessList reports how many processes share this console.
    // One means the console was created for us, so nothing else will be left
    // to read the output after the process ends.
    DWORD processes[2] = {};
    const DWORD attached = GetConsoleProcessList(processes, 2);
    // 0.5.215: with no console at all (the multiplayer program's normal
    // start now) there is no window to keep open and nothing to wait for.
    if (attached <= 1 && GetConsoleWindow() != nullptr)
    {
        printf("\nPress Enter to close this window.\n");
        fflush(stdout);
        (void)getchar();
    }
    Net::PublicShutdown();   // 0.5.428: off the public list first
    Log::Finish();
    ExitProcess(static_cast<UINT>(code));
}

void Kernel::Unimplemented(const char* name, PPCContext& ctx, uint8_t* base)
{
    (void)base;

    printf("\n");
    printf("=====================================================================\n");
    printf(" The game called an Xbox 360 kernel function that does not exist yet\n");
    printf("=====================================================================\n");
    printf("\n");
    printf("  import   %s\n", name);
    printf("  link     0x%08X   (guest address it will return to)\n", uint32_t(ctx.lr));
    printf("\n");
    printf("  arguments, r3 first:\n");
    printf("    r3 = 0x%08X   r4 = 0x%08X   r5 = 0x%08X\n",
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32);
    printf("    r6 = 0x%08X   r7 = 0x%08X   r8 = 0x%08X\n",
        ctx.r6.u32, ctx.r7.u32, ctx.r8.u32);
    printf("\n");
    printf("  This is the expected result today. The recompiled game code is\n");
    printf("  complete, but the runtime beneath it is not: 179 kernel imports\n");
    printf("  are still stubs. Implement this one in CoD3Host/kernel.cpp,\n");
    printf("  remove its line from kernel_stubs.cpp, and run again to find the\n");
    printf("  next. See STATUS.md.\n");
    printf("\n");

    fflush(stdout);
    Guest::Shutdown();
    Kernel::Exit(1);
}

namespace
{
    // The counts are found under a shared lock and bumped atomically: three
    // worker threads polling a job lock make a million kernel calls a second
    // between them, and a plain mutex here was what every other thread's
    // calls queued behind. A name is a string literal, so its address is
    // its identity.
    std::shared_mutex g_importMutex;
    std::map<const char*, std::atomic<uint64_t>> g_importCounts;
}

namespace
{
    // A short history per thread, kept in a fixed ring so it costs one store.
    constexpr size_t HistoryLength = 24;

    struct History
    {
        const char* names[HistoryLength] = {};
        uint32_t subjects[HistoryLength] = {};
        size_t next = 0;
    };

    // Each thread writes its own entry without the lock, which only guards
    // the map itself; the entries never move or go away. A report reads
    // a ring that may be mid-write, which is fine for what it is for.
    std::mutex g_historyMutex;
    std::map<uint32_t, History> g_histories;
    thread_local History* t_history = nullptr;
}

namespace
{
    // COD3_TRACEKERNEL=N prints the next N kernel calls, every thread, with
    // the return address, from the moment Kernel::StartKernelTrace is called:
    // the whole conversation between the title and the kernel over a short
    // stretch, which is how a player that gives up in a few milliseconds
    // says why.
    std::atomic<int> g_kernelTraceLeft{ 0 };
}

void Kernel::StartKernelTrace()
{
    const char* text = getenv("COD3_TRACEKERNEL");
    if (text == nullptr) return;
    int expected = 0;
    const int count = int(strtol(text, nullptr, 10));
    if (count > 0 && g_kernelTraceLeft.compare_exchange_strong(expected, count))
    {
        printf("k: tracing the next %d kernel calls\n", count);
        fflush(stdout);
    }
}

void Kernel::CountImportOn(const char* name, uint32_t subject)
{
    if (g_kernelTraceLeft.load(std::memory_order_relaxed) > 0 &&
        strstr(name, "CriticalSection") == nullptr &&   // thousands a second, all alike
        !(subject == 0x100 && (name[2] == 'W' || name[2] == 'R')) &&   // the job lock, polled
        g_kernelTraceLeft.fetch_sub(1) > 0)
    {
        const PPCContext* context = CurrentContext();
        printf("k: %5u %s(0x%08X) from 0x%08X", GetCurrentThreadId(), name, subject,
            context != nullptr ? uint32_t(context->lr) : 0u);
        // A dispatcher object in guest memory: its header says its type and
        // its signal state, which is what a wait that never ends is about.
        if (name[0] == 'K' && name[1] == 'e' && subject >= 0x10000u && subject < 0xC0000000u)
            printf("  [%08X %08X]", Guest::Read32(Guest::Base, subject),
                Guest::Read32(Guest::Base, subject + 4));
        uint32_t functions[14] = {};
        const int count = Sampler::FunctionsOnStack(functions, 14);
        for (int i = 1; i < count; i++) printf("%s%08X", i == 1 ? "  <- " : " ", functions[i]);
        printf("\n");
    }

    // Every kernel call is a place a thread can hand its hardware thread over.
    // Putting the check here rather than in a handful of chosen imports is what
    // makes the sharing actually happen: a thread that only ever calls one
    // unremarkable function would otherwise keep the slot to itself.
    Scheduler::Checkpoint();

    {
        if (t_history == nullptr)
        {
            std::lock_guard<std::mutex> lock(g_historyMutex);
            t_history = &g_histories[GetCurrentThreadId()];
        }
        History& history = *t_history;
        history.names[history.next % HistoryLength] = name;
        history.subjects[history.next % HistoryLength] = subject;
        history.next++;
    }

    // Each thread keeps where the counters of the names it has called are
    // (the map's entries never move): the title makes a million of these
    // calls a second, and a look in the map under its lock was a cost the
    // size of the call.
    struct Known { const char* name = nullptr; std::atomic<uint64_t>* counter = nullptr; };
    thread_local Known known[64];
    Known& slot = known[(reinterpret_cast<uintptr_t>(name) >> 3) & 63];
    if (slot.name != name)
    {
        std::unique_lock<std::shared_mutex> lock(g_importMutex);
        slot.counter = &g_importCounts[name];
        slot.name = name;
    }
    slot.counter->fetch_add(1, std::memory_order_relaxed);
}

void Kernel::CountImport(const char* name) { CountImportOn(name, 0); }

namespace
{
    thread_local PPCContext* t_currentContext = nullptr;
}

void Kernel::DumpRequested()
{
    const char* list = getenv("COD3_DUMP");
    if (list == nullptr) return;
    auto Words = [](const char* label, uint32_t address) {
        printf("  %s0x%08X ->", label, address);
        for (int w = 0; w < 8; w++)
            printf(" %08X", Guest::Read32(Guest::Base, address + w * 4));
        printf("\n");
    };
    for (const char* at = list; *at != 0;)
    {
        char* end = nullptr;
        const uint32_t address = uint32_t(strtoul(at, &end, 16));
        if (end == at) break;
        Words("dump ", address);
        const uint32_t first = Guest::Read32(Guest::Base, address);
        if (first >= 0x10000u && first < 0xC0000000u) Words("  -> ", first);
        at = *end == ',' ? end + 1 : end;
    }
    fflush(stdout);
}

void Kernel::SetUnwinding(bool unwinding) { t_unwinding = unwinding; }
bool Kernel::IsUnwinding() { return t_unwinding; }

namespace
{
    std::mutex g_contextsMutex;
    std::map<uint32_t, PPCContext*> g_contexts;   // by host thread id
}

void Kernel::SetCurrentContext(PPCContext* context)
{
    t_currentContext = context;
    std::lock_guard<std::mutex> lock(g_contextsMutex);
    if (context != nullptr) g_contexts[GetCurrentThreadId()] = context;
    else g_contexts.erase(GetCurrentThreadId());
}

bool (*Kernel::FaultHook)(void* exceptionPointers) = nullptr;

PPCContext* Kernel::ContextOf(uint32_t hostThreadId)
{
    std::lock_guard<std::mutex> lock(g_contextsMutex);
    auto found = g_contexts.find(hostThreadId);
    return found != g_contexts.end() ? found->second : nullptr;
}

PPCContext* Kernel::CurrentContext()
{
    return t_currentContext;
}

void Kernel::ReportRecentCalls(uint32_t onlyThread)
{
    std::lock_guard<std::mutex> lock(g_historyMutex);
    printf("recent kernel calls %s, oldest first:\n",
        onlyThread ? "on the thread that threw" : "per thread");
    for (const auto& entry : g_histories)
    {
        const History& history = entry.second;
        if (history.next == 0) continue;
        if (onlyThread != 0 && entry.first != onlyThread) continue;

        printf("  os %-6u:", entry.first);
        const size_t count = history.next < HistoryLength ? history.next : HistoryLength;
        const size_t first = history.next - count;
        for (size_t i = 0; i < count; i++)
        {
            const size_t slot = (first + i) % HistoryLength;
            const char* name = history.names[slot];
            printf(" %s", name ? name : "?");
            if (history.subjects[slot] != 0) printf("(%08X)", history.subjects[slot]);
        }
        printf("\n");
    }
    fflush(stdout);
}

void Kernel::ReportImports()
{
    std::vector<std::pair<uint64_t, const char*>> ordered;
    {
        std::shared_lock<std::shared_mutex> lock(g_importMutex);
        for (const auto& entry : g_importCounts)
            ordered.push_back({ entry.second.load(std::memory_order_relaxed), entry.first });
    }
    if (ordered.empty()) return;

    std::sort(ordered.begin(), ordered.end(),
        [](const auto& a, const auto& b) { return a.first > b.first; });

    printf("kernel calls, busiest first:");
    for (size_t i = 0; i < ordered.size() && i < 80; i++)
        printf(" %s x%llu", ordered[i].second, (unsigned long long)ordered[i].first);
    printf("\n");
    fflush(stdout);
}
