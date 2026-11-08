// The watchdog: what every host thread is doing when the runtime stops.
//
// A run that hangs says nothing, and the diagnostics that would say
// something are printed by the very threads that have stopped. This thread
// prints nothing until the vertical blank counter has not moved for a
// while, and then walks the native stack of every thread in the process
// with dbghelp, symbolised from the executable's own debug information.
// The lock one thread holds and another waits for is on those stacks.

#include "kernel.h"
#include "sampler.h"
#include "timeline.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <algorithm>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <psapi.h>

#ifdef COD3_MP
void MpFixesHeapDump(uint8_t* base);   // mp_fixes.cpp, 0.5.556: the game heaps at a stall
#endif
#include <winternl.h>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

namespace
{
    std::atomic<bool> g_running{ false };
    std::thread g_thread;

    void PrintFrame(HANDLE process, DWORD64 pc);
    void WalkFrames(HANDLE process, HANDLE thread, CONTEXT& context);

    void PrintThreadStack(HANDLE process, DWORD threadId)
    {
        HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT |
                                   THREAD_QUERY_INFORMATION, FALSE, threadId);
        if (thread == nullptr) return;

        CONTEXT context{};
        context.ContextFlags = CONTEXT_FULL;
        bool captured = false;
        if (SuspendThread(thread) != DWORD(-1))
        {
            captured = GetThreadContext(thread, &context) != FALSE;
            ResumeThread(thread);
        }
        if (!captured) { CloseHandle(thread); return; }

        printf("  thread %5lu:", threadId);
        Kernel::SetUnwinding(true);
        WalkFrames(process, thread, context);
        Kernel::SetUnwinding(false);
        printf("\n");
        CloseHandle(thread);
    }

    void PrintFrame(HANDLE process, DWORD64 pc)
    {
        char buffer[sizeof(SYMBOL_INFO) + 256] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, pc, &displacement, symbol))
        {
            printf(" %s+0x%llx", symbol->Name, (unsigned long long)displacement);
            // The line, for the runtime's own code: a profile by symbol says
            // which function, and a lambda the size of the pixel loop needs
            // more than that.
            IMAGEHLP_LINE64 line{};
            line.SizeOfStruct = sizeof(line);
            DWORD lineDisplacement = 0;
            if (SymGetLineFromAddr64(process, pc, &lineDisplacement, &line))
            {
                const char* file = strrchr(line.FileName, '\\');
                printf("@%s:%lu", file != nullptr ? file + 1 : line.FileName, line.LineNumber);
            }
            // And the address within the module, for llvm-symbolizer, which
            // knows what was inlined where and this does not.
            if (const DWORD64 module = SymGetModuleBase64(process, pc))
                printf("@+%llx", (unsigned long long)(pc - module));
            return;
        }
        const DWORD64 module = SymGetModuleBase64(process, pc);
        char name[64] = "?";
        if (module != 0)
        {
            IMAGEHLP_MODULE64 info{};
            info.SizeOfStruct = sizeof(info);
            if (SymGetModuleInfo64(process, module, &info))
                snprintf(name, sizeof(name), "%s", info.ModuleName);
        }
        printf(" %s!%llx", name, (unsigned long long)(pc - module));
    }

    void WalkFrames(HANDLE process, HANDLE thread, CONTEXT& context)
    {
        STACKFRAME64 frame{};
        frame.AddrPC.Offset = context.Rip;
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Offset = context.Rbp;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Offset = context.Rsp;
        frame.AddrStack.Mode = AddrModeFlat;

        __try
        {
            for (int depth = 0; depth < 24; depth++)
            {
                if (!StackWalk64(IMAGE_FILE_MACHINE_AMD64, process, thread, &frame, &context,
                        nullptr, SymFunctionTableAccess64, SymGetModuleBase64, nullptr))
                    break;
                if (frame.AddrPC.Offset == 0) break;
                PrintFrame(process, frame.AddrPC.Offset);
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            printf(" (walk faulted)");
        }
    }

    // DbgHelp is not made for more than one thread at a time.
    std::recursive_mutex g_symbolsLock;

    void LoadSymbols(HANDLE process)
    {
        std::lock_guard<std::recursive_mutex> lock(g_symbolsLock);
        static bool symbols = false;
        if (!symbols)
        {
            SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES);
            symbols = SymInitialize(process, nullptr, TRUE) != FALSE;
        }
    }

    void DumpAllThreads()
    {
        const HANDLE process = GetCurrentProcess();
        LoadSymbols(process);

        printf("\nwatchdog: host stacks of every thread:\n");
        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (snapshot == INVALID_HANDLE_VALUE) return;
        THREADENTRY32 entry{};
        entry.dwSize = sizeof(entry);
        const DWORD self = GetCurrentThreadId();
        const DWORD pid = GetCurrentProcessId();
        for (BOOL ok = Thread32First(snapshot, &entry); ok; ok = Thread32Next(snapshot, &entry))
        {
            if (entry.th32OwnerProcessID != pid || entry.th32ThreadID == self) continue;
            PrintThreadStack(process, entry.th32ThreadID);
        }
        CloseHandle(snapshot);
        fflush(stdout);
    }

    void WatchdogThread()
    {
        uint64_t lastFrames = 0;
        auto lastChange = std::chrono::steady_clock::now();
        bool reported = false;
        uint64_t lastSwaps = 0;
        auto lastSwap = std::chrono::steady_clock::now();
        bool swapReported = false;
        while (g_running.load(std::memory_order_acquire))
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            const uint64_t frames = Kernel::Stats().frames.load(std::memory_order_relaxed);
            const auto now = std::chrono::steady_clock::now();

            // The frame counter above is the command thread's own loop, which
            // keeps turning when the title has stopped. The title's swaps are
            // the title: five seconds without one, once it has made any, is
            // a stall, and the report is the guest side of it, the locks and
            // where every guest thread stands, since that is where the title
            // is stuck.
            const uint64_t swaps = Kernel::Stats().swaps.load(std::memory_order_relaxed);
            if (swaps != lastSwaps)
            {
                lastSwaps = swaps;
                lastSwap = now;
                swapReported = false;
            }
            else if (!swapReported && swaps != 0 && now - lastSwap > std::chrono::seconds(5))
            {
                swapReported = true;
                printf("\nwatchdog: no swap for five seconds, after %llu\n",
                    (unsigned long long)swaps);
                Kernel::ReportLocks();
                Sampler::SampleNow();
                DumpAllThreads();
                Timeline::Report();
#ifdef COD3_MP
                ::MpFixesHeapDump(Guest::Base);   // 0.5.556
#endif
                fflush(stdout);
            }

            // COD3_WATCHDOG=N dumps every N seconds whether or not anything
            // has stopped: what every host thread is doing, on demand.
            static const int every = []() {
                const char* text = getenv("COD3_WATCHDOG");
                return text != nullptr ? int(strtol(text, nullptr, 10)) : 0;
            }();
            static auto lastDump = now;
            if (every > 0 && now - lastDump > std::chrono::seconds(every))
            {
                lastDump = now;
                DumpAllThreads();
            }

            if (frames != lastFrames)
            {
                lastFrames = frames;
                lastChange = now;
                reported = false;
                continue;
            }
            if (!reported && frames != 0 && now - lastChange > std::chrono::seconds(8))
            {
                reported = true;
                DumpAllThreads();
            }
        }
    }
}

void Kernel::PrintHostStack(void* context)
{
    const HANDLE process = GetCurrentProcess();
    LoadSymbols(process);
    CONTEXT copy = *static_cast<CONTEXT*>(context);
    printf("  host stack:");
    WalkFrames(process, GetCurrentThread(), copy);
    printf("\n");
    fflush(stdout);
}

namespace
{
    // 0.5.396: the modules' unwind tables, found once, so a stack can be
    // walked while its thread is stopped, without RtlLookupFunctionEntry
    // (whose lock the stopped thread may hold) and without reading outside
    // the stopped thread's own stack (so nothing faults: a fault's handlers
    // can wait on locks the stopped thread holds). The self only profile of
    // 0.5.371 could not say who called NtFreeVirtualMemory, 4% of the main
    // thread in Marseille's heavy views.
    struct UnwindModule { DWORD64 base, end; const RUNTIME_FUNCTION* table; DWORD count; };
    std::vector<UnwindModule> g_unwindModules;

    void LoadUnwindModules()
    {
        std::vector<UnwindModule> found;
        HMODULE modules[1024];
        DWORD needed = 0;
        if (!EnumProcessModules(GetCurrentProcess(), modules, sizeof(modules), &needed)) return;
        for (DWORD i = 0; i < std::min<DWORD>(needed / sizeof(HMODULE), 1024); i++)
        {
            const auto* base = reinterpret_cast<const uint8_t*>(modules[i]);
            const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) continue;
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) continue;
            const IMAGE_DATA_DIRECTORY& directory = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
            if (directory.VirtualAddress == 0 || directory.Size < sizeof(RUNTIME_FUNCTION)) continue;
            found.push_back({ DWORD64(base), DWORD64(base) + nt->OptionalHeader.SizeOfImage,
                reinterpret_cast<const RUNTIME_FUNCTION*>(base + directory.VirtualAddress), DWORD(directory.Size / sizeof(RUNTIME_FUNCTION)) });
        }
        std::sort(found.begin(), found.end(), [](const UnwindModule& a, const UnwindModule& b) { return a.base < b.base; });
        g_unwindModules.swap(found);
    }

    // RtlLookupFunctionEntry's answer from the tables above: no lock, no
    // allocation. `known` false: the address is in no module.
    PRUNTIME_FUNCTION LookupEntry(DWORD64 pc, DWORD64& imageBase, bool& known)
    {
        known = false;
        size_t lo = 0, hi = g_unwindModules.size();
        while (lo < hi) { const size_t mid = (lo + hi) / 2; if (g_unwindModules[mid].base <= pc) lo = mid + 1; else hi = mid; }
        if (lo == 0) return nullptr;
        const UnwindModule& m = g_unwindModules[lo - 1];
        if (pc >= m.end) return nullptr;
        known = true;
        imageBase = m.base;
        const DWORD rva = DWORD(pc - m.base);
        size_t a = 0, b = m.count;
        while (a < b)
        {
            const size_t mid = (a + b) / 2;
            if (m.table[mid].EndAddress <= rva) a = mid + 1;
            else if (m.table[mid].BeginAddress > rva) b = mid;
            else return const_cast<PRUNTIME_FUNCTION>(&m.table[mid]);
        }
        return nullptr;
    }

    // A stopped thread's stack, walked by the tables above, stopping at
    // anything outside the stack [low, high) or outside every module.
    void FunctionsOfStopped(CONTEXT* context, DWORD64 low, DWORD64 high, DWORD64* starts, int& count, int limit)
    {
        __try
        {
            for (int depth = 0; depth < 64 && count < limit; depth++)
            {
                DWORD64 imageBase = 0;
                bool known = false;
                PRUNTIME_FUNCTION entry = LookupEntry(context->Rip, imageBase, known);
                if (!known) { if (count == 0) starts[count++] = context->Rip; break; }
                // The unwind reads the frame above the stack pointer: the
                // whole of it must be in the stack (a frame is far smaller).
                if (context->Rsp < low || context->Rsp + 0x1000 > high) { starts[count++] = entry ? imageBase + entry->BeginAddress : context->Rip; break; }
                if (entry == nullptr)
                {
                    starts[count++] = context->Rip;
                    context->Rip = *reinterpret_cast<DWORD64*>(context->Rsp);
                    context->Rsp += 8;
                }
                else
                {
                    starts[count++] = imageBase + entry->BeginAddress;
                    void* handlerData = nullptr;
                    DWORD64 establisherFrame = 0;
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context->Rip, entry,
                        context, &handlerData, &establisherFrame, nullptr);
                }
                if (context->Rip == 0) break;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    // The functions on a stack, innermost first, by their start addresses:
    // a copy of the registers taken while the thread was stopped, unwound
    // after it runs on (a stale stack costs one wrong sample at worst).
    void FunctionsOf(CONTEXT* context, DWORD64* starts, int& count, int limit)
    {
        __try
        {
            for (int depth = 0; depth < 64 && count < limit; depth++)
            {
                DWORD64 imageBase = 0;
                PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(context->Rip, &imageBase, nullptr);
                if (entry == nullptr)
                {
                    // A leaf without unwind data: its return address is on top.
                    starts[count++] = context->Rip;
                    context->Rip = *reinterpret_cast<DWORD64*>(context->Rsp);
                    context->Rsp += 8;
                }
                else
                {
                    starts[count++] = imageBase + entry->BeginAddress;
                    void* handlerData = nullptr;
                    DWORD64 establisherFrame = 0;
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, context->Rip, entry,
                        context, &handlerData, &establisherFrame, nullptr);
                }
                if (context->Rip == 0) break;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
        }
    }

    std::string FunctionName(HANDLE process, DWORD64 start)
    {
        std::lock_guard<std::recursive_mutex> lock(g_symbolsLock);
        char buffer[sizeof(SYMBOL_INFO) + 256] = {};
        auto* symbol = reinterpret_cast<SYMBOL_INFO*>(buffer);
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = 255;
        DWORD64 displacement = 0;
        if (SymFromAddr(process, start, &displacement, symbol)) return symbol->Name;
        char text[64];
        const DWORD64 module = SymGetModuleBase64(process, start);
        snprintf(text, sizeof(text), "%llx (+%llx)", (unsigned long long)start, (unsigned long long)(start - module));
        return text;
    }

    // The source line of an address: "file.cpp:123", or "" when unknown.
    std::string LineOf(HANDLE process, DWORD64 address)
    {
        std::lock_guard<std::recursive_mutex> lock(g_symbolsLock);
        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(line);
        DWORD displacement = 0;
        if (!SymGetLineFromAddr64(process, address, &displacement, &line) || line.FileName == nullptr) return "";
        const char* file = line.FileName;
        for (const char* p = line.FileName; *p != 0; p++)
            if (*p == '\\' || *p == '/') file = p + 1;
        return std::string(file) + ":" + std::to_string(line.LineNumber);
    }
}

void Kernel::ProfileHostThread(uint32_t osId, const char* name, const char* tag)
{
    const char* wanted = getenv("COD3_HOSTPROFILE");
    if (wanted == nullptr || !(strstr(wanted, tag) != nullptr || (strcmp(wanted, "1") == 0 && strcmp(tag, "cp") == 0))) return;
    std::thread([osId, name]() {
        const HANDLE process = GetCurrentProcess();
        LoadSymbols(process);
        const HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, osId);
        if (thread == nullptr) return;
        // 0.5.396: COD3_HOSTPROFILE_SELF=stopped (the default now) walks the
        // stack while the thread is stopped (above); =1 where it was only;
        // =0 the old walk after it runs on.
        static const int mode = []() { const char* t = getenv("COD3_HOSTPROFILE_SELF"); return !t ? 2 : t[0] == '0' ? 0 : t[0] == '1' ? 1 : 2; }();
        // The stack's bounds, from the thread's TEB (NT_TIB: StackBase +8, StackLimit +16).
        const uint8_t* teb = nullptr;
        if (mode == 2)
        {
            static std::mutex modulesLock;
            { std::lock_guard<std::mutex> lock(modulesLock); if (g_unwindModules.empty()) LoadUnwindModules(); }
            using Query = LONG(NTAPI*)(HANDLE, int, PVOID, ULONG, PULONG);
            struct BasicInformation { LONG status; PVOID teb; CLIENT_ID client; ULONG_PTR affinity; LONG priority, basePriority; } info{};
            if (auto query = reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread")))
                if (query(thread, 0, &info, sizeof(info), nullptr) == 0) teb = static_cast<const uint8_t*>(info.teb);
        }
        std::unordered_map<DWORD64, uint32_t> self, inclusive;
        // For the busiest functions, who called them: (function, caller).
        std::map<std::pair<DWORD64, DWORD64>, uint32_t> callers;
        // COD3_HOSTPROFILE_LINES=1: where in a function the samples fell,
        // by the instruction they were at.
        static const bool byLine = getenv("COD3_HOSTPROFILE_LINES") != nullptr;
        std::unordered_map<DWORD64, std::unordered_map<DWORD64, uint32_t>> at;
        uint32_t samples = 0;
        auto reported = std::chrono::steady_clock::now();
        for (;;)
        {
            CONTEXT context{};
            context.ContextFlags = CONTEXT_FULL;
            DWORD64 starts[64];
            int count = 0;
            if (SuspendThread(thread) == DWORD(-1)) break;
            const BOOL captured = GetThreadContext(thread, &context);
            const DWORD64 rip = context.Rip;   // the unwind moves the context on
            if (captured && mode == 2 && teb != nullptr)
            {
                const DWORD64 high = *reinterpret_cast<const DWORD64*>(teb + 8), low = *reinterpret_cast<const DWORD64*>(teb + 16);
                FunctionsOfStopped(&context, low, high, starts, count, 24);
            }
            ResumeThread(thread);
            if (captured)
            {
                // 0.5.371: COD3_HOSTPROFILE_SELF=1 (on by default): only where the
                // thread WAS, not its callers. The full stack walk reads a
                // running thread's stack and froze the game twice (0.5.236 at
                // 307 s, 0.5.370 at 95 s in the match); "in the function itself"
                // is what the fixes need anyway. COD3_HOSTPROFILE_SELF=0: walk.
                const bool selfOnly = mode == 1 || (mode == 2 && count == 0);
                if (count > 0) {}
                else if (selfOnly)
                {
                    DWORD64 imageBase = 0;
                    PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(rip, &imageBase, nullptr);
                    starts[count++] = entry ? imageBase + entry->BeginAddress : rip;
                }
                else
                {
                    Kernel::SetUnwinding(true);
                    FunctionsOf(&context, starts, count, 64);
                    Kernel::SetUnwinding(false);
                }
                if (count > 0)
                {
                    samples++;
                    self[starts[0]]++;
                    if (byLine) at[starts[0]][rip]++;
                    // The caller: for a function outside the executable (the
                    // system's, the driver's), the first frame back in it.
                    if (count > 1)
                    {
                        static const DWORD64 exeLow = DWORD64(GetModuleHandleW(nullptr));
                        auto inExe = [](DWORD64 a) { return a >= exeLow && a < exeLow + (1ull << 31); };
                        int i = 1;
                        if (!inExe(starts[0])) while (i + 1 < count && !inExe(starts[i])) i++;
                        callers[{ starts[0], starts[i] }]++;
                    }
                    for (int i = 0; i < count; i++)
                    {
                        bool seen = false;
                        for (int j = 0; j < i && !seen; j++) seen = starts[j] == starts[i];
                        if (!seen) inclusive[starts[i]]++;
                    }
                }
            }
            Sleep(1);
            const auto now = std::chrono::steady_clock::now();
            if (now - reported < std::chrono::seconds(5) || samples == 0) continue;
            reported = now;
            // The report made whole, then written at once: several threads
            // may be profiled, and their lines would interleave.
            std::string report;
            auto add = [&report](const char* format, auto... values) {
                char line[512];
                snprintf(line, sizeof(line), format, values...);
                report += line;
            };
            for (int kind = 0; kind < 2; kind++)
            {
                std::vector<std::pair<uint32_t, DWORD64>> ordered;
                for (const auto& [start, hits] : kind == 0 ? self : inclusive) ordered.push_back({ hits, start });
                std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
                add("profile of the %s, %u samples, %s:\n", name, samples, kind == 0 ? "in the function itself" : "with what it calls");
                for (size_t i = 0; i < ordered.size() && i < (kind == 0 ? 30u : 50u); i++)
                    add("  %5.1f%% %s\n", 100.0 * ordered[i].first / samples, FunctionName(process, ordered[i].second).c_str());
            }
            {
                std::vector<std::pair<uint32_t, DWORD64>> busiest;
                for (const auto& [start, hits] : self) busiest.push_back({ hits, start });
                std::sort(busiest.begin(), busiest.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
                add("profile of the %s, who called the busiest:\n", name);
                for (size_t i = 0; i < busiest.size() && i < 8; i++)
                {
                    std::vector<std::pair<uint32_t, DWORD64>> from;
                    for (const auto& [pair, hits] : callers)
                        if (pair.first == busiest[i].second) from.push_back({ hits, pair.second });
                    std::sort(from.begin(), from.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
                    add("  %s:", FunctionName(process, busiest[i].second).c_str());
                    for (size_t j = 0; j < from.size() && j < 4; j++)
                        add(" %s %.1f%%%s", FunctionName(process, from[j].second).c_str(), 100.0 * from[j].first / samples, j + 1 < from.size() && j < 3 ? "," : "");
                    add("\n");
                }
            }
            if (byLine)
            {
                // The five busiest functions, by source line.
                std::vector<std::pair<uint32_t, DWORD64>> busiest;
                for (const auto& [start, hits] : self) busiest.push_back({ hits, start });
                std::sort(busiest.begin(), busiest.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
                for (size_t i = 0; i < busiest.size() && i < 5; i++)
                {
                    std::map<std::string, uint32_t> lines;
                    for (const auto& [rip, hits] : at[busiest[i].second]) lines[LineOf(process, rip)] += hits;
                    std::vector<std::pair<uint32_t, std::string>> ordered;
                    for (const auto& [line, hits] : lines) ordered.push_back({ hits, line });
                    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
                    add("profile of the %s, %s by line:", name, FunctionName(process, busiest[i].second).c_str());
                    for (size_t j = 0; j < ordered.size() && j < 12; j++)
                        add(" %s %.1f%%", ordered[j].second.empty() ? "?" : ordered[j].second.c_str(), 100.0 * ordered[j].first / samples);
                    add("\n");
                }
            }
            {
                static std::mutex printing;
                std::lock_guard<std::mutex> lock(printing);
                fwrite(report.data(), 1, report.size(), stdout);
                fflush(stdout);
            }
            self.clear();
            inclusive.clear();
            at.clear();
            callers.clear();
            samples = 0;
        }
        CloseHandle(thread);
    }).detach();
}

void Kernel::StartWatchdog()
{
    bool expected = false;
    if (!g_running.compare_exchange_strong(expected, true)) return;
    g_thread = std::thread(WatchdogThread);
    g_thread.detach();
}
