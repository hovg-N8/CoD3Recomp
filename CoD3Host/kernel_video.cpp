// Xbox 360 kernel imports: the video driver.
//
// On the console these calls talk to the Xenos GPU. The title hands over a
// command ring buffer, writes packets into it, advances a write pointer, and
// waits for the GPU to catch up and for a vertical blank interrupt before
// starting the next frame.
//
// There is no GPU here. What this file provides is the part of that contract
// the title can observe from the CPU: the ring buffer is accepted, a thread
// advances the read pointer to wherever the write pointer has reached, and the
// graphics interrupt callback is invoked at 60 Hz. That is enough for the
// title to believe frames are completing and to keep running its main loop.
//
// Nothing in the ring buffer is read. No packet is decoded, no shader is
// translated, nothing is drawn. Producing a picture means writing a real
// command processor and running XenosRecomp over the title's shaders, which
// is the largest single piece of work left in this project.

#include "kernel.h"
#include "input.h"
#include "pc_prompts.h"
#include "sampler.h"
#include "audio_out.h"
#include "scheduler.h"
#include "gpu.h"
#include "render.h"
#include "settings.h"
#include "window.h"
#include "pool_trace.h"
#include "timeline.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <vector>
#include <condition_variable>
#include <thread>

#include <Windows.h>

#ifdef COD3_MP
void MpFixesAskVehicle(const std::string& kind);   // mp_fixes.cpp
void BotsAsk(int count);
void BotsTactics(int enabled);
void BotsCamera(int enabled);
void BotsPose(const std::string& args);       // bots.cpp, 0.5.206: bot_pose
void BotsPoseTest(const std::string& args);   // bot_pose_test
void BotsSetup(int bits);                           // bots.cpp
void BotsRemote(int enabled);                       // bots.cpp (0.5.135)
void BotsLocal(int enabled);                        // bots.cpp (0.5.137)
void BotsTankFire(int seconds, int crew);           // bots.cpp (0.5.796): tank_fire
void MpFixesMortarTest(int rounds);                 // mp_fixes.cpp (0.5.801): mortar_test
void MpFixesVehiclesEverywhere(bool on);           // mp_fixes.cpp
void MpFixesKangaroo(bool on);                     // mp_fixes.cpp
void MpFixesKangarooType(int type);
void MpFixesQueueTankBlast(const float* at, const char* event);   // mp_fixes.cpp (0.5.780; console 0.5.791)
void MpFixesKangarooTread(int mode);                // mp_fixes.cpp
void MpFixesKangarooEntry(int enabled);
void MpFixesKangarooExit(int mode);
void MpFixesKangarooDrop(float amount);
void MpFixesKangarooBuild(int mode);                // mp_fixes.cpp (0.5.99)
void MpFixesVehicleOrder(int what, const std::string& kind);   // mp_fixes.cpp
void MpFixesGiveStreak(const std::string& kind);   // mp_fixes.cpp
void MpFixesPlaneSoundTest(const std::string& args);   // mp_fixes.cpp (0.5.264)
void MpFixesPlaneModelTest(const std::string& args);   // mp_fixes.cpp (0.5.267)
void MpFixesPlaneEngine(const std::string& args);   // mp_fixes.cpp (0.5.272)
void MpFixesMortarYaw(const std::string& args);     // mp_fixes.cpp (0.5.303)
void MpFixesMortarView(const std::string& args);    // mp_fixes.cpp (0.5.309)
#endif

namespace
{
    struct VideoState
    {
        std::atomic<bool> running{ false };

        // Where the command thread waits for work and what wakes it.
        std::mutex submitMutex;
        std::condition_variable submitted;
        std::atomic<uint64_t> submissions{ 0 };

        std::atomic<uint32_t> ringBuffer{ 0 };
        std::atomic<uint32_t> ringBufferSize{ 0 };
        std::atomic<uint32_t> readPointerWriteBack{ 0 };
        std::atomic<uint32_t> readPointer{ 0 };
        std::atomic<uint64_t> swaps{ 0 };           // VdSwap calls: frames
        std::atomic<uint32_t> gpuIdentifier{ 0 };
        std::atomic<uint32_t> interruptCallback{ 0 };
        std::atomic<uint64_t> interruptsRaised{ 0 };
        std::atomic<uint32_t> interruptContext{ 0 };
        std::atomic<uint64_t> frames{ 0 };
        std::thread thread;
        std::thread commandThread;

        // Held while the ring is walked and while it is replaced, so the two
        // never overlap.
        std::mutex ringMutex;
    };

    VideoState& Video()
    {
        static VideoState state;
        return state;
    }

    // The pretend GPU. It never looks at what is in the ring buffer; it only
    // keeps the bookkeeping the title polls consistent.
    // The command processor.
    //
    // This used to share the video thread, and that only worked while there was
    // nothing to draw. Once draws were really carried out, one busy buffer took
    // longer than a frame and the vertical blank stopped happening, which the
    // title reads as a display that has stopped. Giving the two their own
    // threads is also what the hardware does: the command processor and the
    // scanout run independently and neither waits for the other.
    // The command thread's guest context, for the interrupt hook above.
    thread_local PPCContext* t_commandContext = nullptr;
    thread_local uint32_t t_commandStackTop = 0;

    // Whether the command thread is inside the title's interrupt handler,
    // and since when. The handler is guest code run in the middle of the
    // command stream, and if it blocks, on a lock a render thread holds
    // while that thread waits for the GPU, the stream stops with it and the
    // title reports the GPU hung. The ring report below says so.
    std::atomic<bool> g_inHandler{ false };
    std::atomic<int64_t> g_handlerSince{ 0 };
    std::atomic<uint32_t> g_commandThreadId{ 0 };

    int64_t NowMilliseconds()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }

    // The handler itself, on the command thread's context, as the given CPU.
    void RunInterruptHandler(PPCFunc* routine, uint32_t cpu)
    {
        VideoState& video = Video();
        PPCContext& context = *t_commandContext;

        context.r1.u32 = t_commandStackTop - 0x100;
        context.r3.u32 = 1;   // source: the command processor
        context.r4.u32 = video.interruptContext.load();

        // The handler decides what to do from the mirrored scratch block:
        // register four is the callback it armed, or 0x0BADF00D when it armed
        // nothing, and an interrupt that finds the latter is one it did not
        // ask for. The first word is the mask of CPUs with an interrupt
        // pending, which the handler clears its own bit from.
        const uint32_t block = Gpu::ReadRegister(Gpu::ApertureBase + 0x1DD * 4) & ~3u;
        const uint32_t pending = block
            ? Guest::Read32(Guest::Base, Guest::PhysicalAlias(block)) : 0;
        const uint32_t armed = block
            ? Guest::Read32(Guest::Base, Guest::PhysicalAlias(block + 16)) : 0;
        const uint64_t raised = video.interruptsRaised.fetch_add(1, std::memory_order_relaxed);
        // The first forty, and forty more once a level is loading: the
        // interrupts that complete the level's recorded command buffers.
        static std::atomic<int> inLevel{ 0 };
        const bool level = Kernel::Stats().filesOpened.load(std::memory_order_relaxed) >= 40;
        const bool usual = armed == 0x82130098u || armed == 0x821300C8u ||
                           armed == 0x822F4A10u || armed == 0x0BADF00Du;
        // An unusual callback is said too, but not every time: the
        // multiplayer title arms its own on every frame, and a line each
        // was hundreds of writes a second on the thread that delivers them.
        static std::atomic<int> unusual{ 0 };
        if (raised < 40 || (level && (usual ? (inLevel.load(std::memory_order_relaxed) < 40 && inLevel.fetch_add(1) < 40) : (unusual.load(std::memory_order_relaxed) < 200 && unusual.fetch_add(1) < 200))))
        {
            printf("gpu: interrupt %llu on cpu %u, pending mask 0x%02X, callback "
                   "0x%08X%s\n", (unsigned long long)raised, cpu, pending, armed,
                armed == 0x0BADF00Du ? " (none armed)" : "");
            fflush(stdout);
        }

        g_handlerSince.store(NowMilliseconds());
        g_inHandler.store(true);
        Timeline::Mark("interrupt", pending, armed);
        routine(context, Guest::Base);
        Timeline::Mark("handled", pending, armed);
        g_inHandler.store(false);
    }

    // The console commands waiting for the vertical blank.
    std::mutex g_consoleMutex;
    std::deque<std::string> g_consoleQueue;

    // The next of them, when the title's command buffer has room for it.
    // Cbuf_AddText (sub_824644D0) drops what does not fit, and until the
    // title has set the buffer up (cmd_text at 0x829F1BB8: the text, its
    // size, how much is used) nothing fits: a command asked for that early
    // waits here instead of being lost.
    std::string TakeConsoleCommand()
    {
#ifdef COD3_MP
        // Multiplayer's cmd_text is at 0x82B93444 (the text, its size, how
        // much is used), its Cbuf_AddText sub_8247FA38.
        const uint32_t size = Guest::Read32(Guest::Base, 0x82B93448u);
        const uint32_t used = Guest::Read32(Guest::Base, 0x82B9344Cu);
#else
        const uint32_t size = Guest::Read32(Guest::Base, 0x829F1BBCu);
        const uint32_t used = Guest::Read32(Guest::Base, 0x829F1BC0u);
#endif
        std::lock_guard<std::mutex> lock(g_consoleMutex);
        if (g_consoleQueue.empty()) return {};
        if (size <= used || g_consoleQueue.front().size() + 2 > size - used) return {};
        std::string command = std::move(g_consoleQueue.front());
        g_consoleQueue.pop_front();
        return command;
    }

    // The command processor thread's own accounts (cp: in the log).
    uint64_t g_cpIdleNs = 0, g_cpHandlerNs = 0;

    void CommandThread()
    {
        VideoState& video = Video();

        // The handler this thread raises is guest code, so it needs a context
        // and a stack of its own, exactly like a thread the title made itself.
        const uint32_t stackTop = Guest::AllocateStack(128u << 10);
        alignas(0x40) PPCContext ctx{};
        ctx.r13.u32 = Guest::CreateThreadPointer(GetCurrentThreadId());
        ctx.fpscr.loadFromHost();

        // Reachable from the command processor, so an interrupt packet can
        // run the handler the moment it is decoded.
        t_commandContext = &ctx;
        Kernel::SetCurrentContext(&ctx);
        t_commandStackTop = stackTop;
        g_commandThreadId.store(GetCurrentThreadId());
        Gpu::SetSubmitHook([]() {
            VideoState& v = Video();
            v.submissions.fetch_add(1, std::memory_order_release);
            std::lock_guard<std::mutex> lock(v.submitMutex);
            v.submitted.notify_one();
        });
        uint64_t seenSubmissions = 0;
        Gpu::SetInterruptRaiser([](uint32_t cpuMask) {
            VideoState& video = Video();
            const uint32_t callback = video.interruptCallback.load();
            if (callback == 0 || t_commandContext == nullptr || t_commandStackTop == 0)
            {
                printf("gpu: interrupt with no handler: callback 0x%08X, context %s, stack %s\n",
                    callback, t_commandContext ? "set" : "null", t_commandStackTop ? "set" : "none");
                return;
            }
            PPCFunc* routine = Guest::Lookup(callback);
            if (routine == nullptr)
            {
                printf("gpu: interrupt handler 0x%08X is not recompiled code\n", callback);
                return;
            }

            Gpu::WriteRegister(Gpu::RegisterInterruptStatus,
                Gpu::ReadRegister(Gpu::RegisterInterruptStatus) | 1);

            // On the CPU the packet names.
            //
            // The handler ends by clearing its own CPU's bit out of the first
            // word of the scratch block, in memory, under a spin lock: that
            // word is the mask of CPUs the driver has interrupts pending on,
            // and the wait that follows in the stream is for it to reach zero.
            // The driver puts the mask into SCRATCH_REG0 and the same mask
            // into the packet, so the handler has to run as that CPU, or it
            // clears the wrong bit and the wait never ends. The thread this
            // runs on is not a guest thread and its processor number is
            // whatever it was given at creation, so it is set here, per
            // interrupt, to each CPU in the mask in turn.
            //
            // COD3_IRQSLOT=1 runs the handler holding that CPU's hardware
            // thread, as the console never runs an interrupt beside the thread
            // it interrupts. It was tried against the hung GPU waits, before
            // the spin locks were made real, and made them more frequent, so
            // it stays off.
            static const bool holdSlot = []() { const char* t = getenv("COD3_IRQSLOT"); return t != nullptr && t[0] == '1'; }();
            if (cpuMask == 0) cpuMask = 1;
            for (uint32_t cpu = 0; cpu < 6; cpu++)
            {
                if (((cpuMask >> cpu) & 1) == 0) continue;
                Guest::SetProcessor(*t_commandContext, int(cpu));
                if (holdSlot) Scheduler::Attach(int(cpu));
                RunInterruptHandler(routine, cpu);
                if (holdSlot) Scheduler::Detach();
            }
        });

        while (video.running.load(std::memory_order_acquire))
        {
            const uint32_t ring = video.ringBuffer.load();
            const uint32_t ringDwords = video.ringBufferSize.load() / 4;
            bool worked = false;

            if (ring != 0 && ringDwords != 0)
            {
                std::lock_guard<std::mutex> lock(video.ringMutex);
                // Read again under the lock: the ring may have been replaced
                // between the reads above and here.
                const uint32_t lockedRing = video.ringBuffer.load();
                const uint32_t lockedDwords = video.ringBufferSize.load() / 4;
                const uint32_t before = video.readPointer.load();
                const uint32_t consumed = Gpu::ProcessRing(
                    lockedRing, lockedDwords, before, Gpu::WritePointer());
                video.readPointer.store(consumed);
                Gpu::WriteRegister(Gpu::ApertureBase + 0x710, consumed);
                worked = consumed != before;

                // The read pointer, published where the driver asked for it
                // (VdEnableRingBufferRPtrWriteBack: word fifteen of the
                // write-back block). The ring writer checks it before every
                // packet it writes, and it is the only thing the writer
                // checks at the wrap: with nothing ever published there the
                // wrap looked like the GPU still reading from the start of
                // the ring, and the title sat out its five second "GPU is
                // hung" timeout on every lap of the ring.
                if (const uint32_t at = video.readPointerWriteBack.load())
                    Guest::WritePhysical32(at & ~3u, consumed);

                // Packets waiting that the walk did not take. Once is a
                // deadline; a thousand times in a row is the walk refusing
                // them, and the header it stopped at says why.
                static int refused = 0;
                const uint32_t write = Gpu::WritePointer();
                if (!worked && write != consumed && lockedDwords != 0)
                {
                    if (++refused == 1000)
                    {
                        const uint32_t at = Guest::PhysicalAlias(lockedRing) + (consumed % lockedDwords) * 4;
                        printf("cp: %u dwords waiting at ring dword %u were not taken a thousand "
                               "times running; header there 0x%08X, next 0x%08X 0x%08X\n",
                            (write + lockedDwords - consumed) % lockedDwords, consumed,
                            Guest::Read32(Guest::Base, at), Guest::Read32(Guest::Base, at + 4),
                            Guest::Read32(Guest::Base, at + 8));
                        fflush(stdout);
                    }
                }
                else refused = 0;
            }

            // The driver's write-back block, checked for the fill pattern.
            //
            // The block the driver registered for the GPU's progress was found
            // holding 0xCDCDCDCD, the pattern the title's allocator writes into
            // memory it has just handed out, and the ring buffer 0x55555555.
            // Something the title allocated landed on top of the driver's
            // state. A hardware watchpoint slows the run enough that it never
            // happens, so instead this looks every time round and, the first
            // time the pattern is there, prints what every guest thread is in
            // the middle of: one of them is the memset.
            {
                static bool reported = false;
                const uint32_t writeBack = video.readPointerWriteBack.load();
                if (!reported && writeBack != 0)
                {
                    const uint32_t block = Guest::PhysicalAlias(writeBack & ~0xFFFFu);
                    const uint32_t word = Guest::Read32(Guest::Base, block);
                    if (word == 0xCDCDCDCDu || word == 0x55555555u ||
                        word == 0xDDDDDDDDu || word == 0xFEEEFEEEu)
                    {
                        reported = true;
                        printf("gpu: the write back block at 0x%08X now holds 0x%08X, "
                               "a fill pattern: something allocated over it. Every "
                               "guest thread, right now:\n", block, word);
                        fflush(stdout);
                        Sampler::SampleNow();
                        Kernel::ReportRecentCalls();
                    }
                }
            }

            // Interrupts the stream asked for, raised now that everything
            // queued ahead of them has been carried out. Source one is the
            // command processor; the vertical blank on the other thread is
            // source zero.
            if (const uint32_t raised = Gpu::TakePendingInterrupts())
            {
                const uint32_t callback = video.interruptCallback.load();
                if (callback != 0 && stackTop != 0)
                {
                    if (PPCFunc* routine = Guest::Lookup(callback))
                    {
                        Gpu::WriteRegister(Gpu::RegisterInterruptStatus,
                            Gpu::ReadRegister(Gpu::RegisterInterruptStatus) | 1);

                        // Once, however many were queued: the handler asks the
                        // hardware what happened rather than being told, so
                        // raising it repeatedly only costs time.
                        (void)raised;
                        ctx.r1.u32 = stackTop - 0x100;
                        ctx.r3.u32 = 1;   // source: the command processor
                        ctx.r4.u32 = video.interruptContext.load();
                        const auto handlerStarted = std::chrono::steady_clock::now();
                        routine(ctx, Guest::Base);
                        g_cpHandlerNs += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - handlerStarted).count());
                        worked = true;
                    }
                }
            }

            // The word the title's render thread spins on is the first of the
            // block its device points at (0x2A10), against the number it keeps
            // at 0x2A1C. Before the fences were carried out this loop wrote
            // that number there itself, so the wait would end. The word is the
            // title's fence counter: every indirect buffer ends with a fence
            // that writes its sequence number there, and 0x2A1C is the last
            // one submitted. Writing the submitted number there from here,
            // every millisecond, told the title the GPU had finished
            // everything the moment it was submitted, and the title, trusting
            // it, wrote the next frame's vertices and constants over memory
            // this thread had not read yet: every twelfth frame or so, a
            // triangle across the whole screen, and once in a while a soldier
            // in pieces. The fences say it themselves now, in order, and this
            // stays only behind COD3_FENCELIE=1 for comparison.
            static const bool fenceLie = getenv("COD3_FENCELIE") != nullptr;
            const uint32_t context = fenceLie ? video.interruptContext.load() : 0;
            if (context != 0)
            {
                const uint32_t readPointerAt =
                    Guest::Read32(Guest::Base, context + 0x2A10);
                const uint32_t titleWritePointer =
                    Guest::Read32(Guest::Base, context + 0x2A1C);
                if (readPointerAt != 0)
                {
                    Guest::WritePhysical32(readPointerAt, titleWritePointer);
                    Guest::WritePhysical32(readPointerAt & ~0x3Fu, titleWritePointer);
                }
            }

            // Nothing done: wait for the title to move the write pointer,
            // or a millisecond, whichever is first. The write pointer is
            // read again before waiting because a submission between the
            // walk and here would otherwise be sat on for that millisecond.
            if (!worked)
            {
                const auto idleStarted = std::chrono::steady_clock::now();
                std::unique_lock<std::mutex> lock(video.submitMutex);
                if (video.submissions.load(std::memory_order_acquire) == seenSubmissions)
                    video.submitted.wait_for(lock, std::chrono::milliseconds(1));
                seenSubmissions = video.submissions.load(std::memory_order_acquire);
                g_cpIdleNs += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - idleStarted).count());
            }
            // Every five seconds: how much of the command processor's time
            // went waiting for the title to give it something, and how much
            // to the title's own interrupt handler run on it. A command
            // processor that never waits is the frame rate's limit.
            {
                static auto reportedAt = std::chrono::steady_clock::now();
                const auto now = std::chrono::steady_clock::now();
                const double seconds = std::chrono::duration<double>(now - reportedAt).count();
                if (seconds >= 5.0)
                {
                    printf("cp: %.0f%% of the time waiting for the title, %.0f%% in its interrupt handler\n",
                        double(g_cpIdleNs) / 1e9 / seconds * 100.0, double(g_cpHandlerNs) / 1e9 / seconds * 100.0);
                    fflush(stdout);
                    g_cpIdleNs = 0;
                    g_cpHandlerNs = 0;
                    reportedAt = now;
                }
            }
        }
    }

    // A level being played: the title's server runs (sv_running, the
    // console variable whose pointer is at 0x829BAB18, its integer at +32)
    // and no film is open (the film player at 0x82A2A24C). The main menu,
    // the films before it and a mission's briefing, which plays while the
    // level loads and after, are not.
    bool TitlePlaying()
    {
#ifdef COD3_MP
        // Multiplayer has no films to keep at their speed, a player who
        // joins another's game runs no server of their own, and the single
        // player's test never once said "playing" here, not even for the
        // host: the frame rate is the machine's from the menu on.
        return true;
#else
        // 0.5.221: Justin: the campaign at 144 or more, as multiplayer. Its
        // levels already were (0.5.215's log: 144 fps in a level, with
        // frame_cap 144); its menus stayed at the console's rate. Now only
        // a film playing (the film player at 0x82A2A24C: the intros, the
        // briefings while a level loads, a menu's moving background) keeps
        // the console's rate, so films keep their speed; the menus and the
        // levels run at the machine's (capped by frame_cap). COD3_SPMENUFPS=0
        // puts the menus back to the console's rate.
        static const bool menusToo = []() { const char* t = getenv("COD3_SPMENUFPS"); return !(t && t[0] == '0'); }();
        if (Guest::Read32(Guest::Base, 0x82A2A24Cu) != 0) return false;
        if (menusToo) return true;
        const uint32_t running = Guest::Read32(Guest::Base, 0x829BAB18u);
        return running != 0 && Guest::Read32(Guest::Base, running + 32) != 0;
#endif
    }

    // Whether the frame rate is unlocked now: the settings say so (the
    // default; COD3_UNLOCKFPS=0|1 over them) and a level is being played.
    // Looked at by the video thread once a blank, read by the command
    // processor at the wait that follows each swap.
    std::atomic<bool> g_frameRateUnlocked{ false };
    void UpdateFrameRateUnlocked(uint64_t frame)
    {
        static const int unlockOverride = []() { const char* t = getenv("COD3_UNLOCKFPS"); return t ? (t[0] == '0' ? 0 : 1) : -1; }();
        static bool unlocked = true;
        if ((frame & 15) == 0) unlocked = unlockOverride >= 0 ? unlockOverride != 0 : Settings::Get().unlockFrameRate;
        const bool now = unlocked && TitlePlaying();
        if (now != g_frameRateUnlocked.load(std::memory_order_relaxed))
        {
            g_frameRateUnlocked.store(now, std::memory_order_relaxed);
            printf("video: the frame rate is %s\n", now ? "unlocked (the machine's, up to frame_cap)" : "the console's (a film, or unlocking is off)");
            fflush(stdout);
        }
    }

    // How long until the next vertical blank: the console's sixty a second,
    // always. The title draws a frame and its stream then waits for a flag
    // its vertical blank handler clears, which is the frame rate's ceiling;
    // an unlocked frame rate lets that wait through (gpu.cpp) rather than
    // making the blanks come faster. Faster blanks were tried, one every
    // millisecond: the frame rate came unlocked, but the title counts the
    // blanks as well, and its soldiers' animation jumped back and forth
    // every frame. COD3_VBLANK=N keeps N a second, for a test (30 .. 1000).
    std::chrono::nanoseconds VblankPeriod()
    {
        static const long fixed = []() { const char* t = getenv("COD3_VBLANK"); const long hz = t ? strtol(t, nullptr, 10) : 0; return hz >= 30 && hz <= 1000 ? hz : 0L; }();
        return fixed != 0 ? std::chrono::nanoseconds(1000000000ll / fixed) : std::chrono::nanoseconds(16666667);
    }

    void VideoThread()
    {
        VideoState& video = Video();
        // The seconds below are the wall clock's, whatever rate the blanks
        // come at (COD3_VBLANK).
        const auto started = std::chrono::steady_clock::now();
        uint64_t lastSecond = 0;

        // Guest code runs on this thread when the interrupt callback fires, so
        // it needs a processor context and a stack of its own, exactly like a
        // thread the title created itself.
        const uint32_t stackTop = Guest::AllocateStack(256u << 10);
        if (stackTop == 0)
        {
            fprintf(stderr, "video: no room for the interrupt stack\n");
            return;
        }

        alignas(0x40) PPCContext ctx{};
        ctx.r13.u32 = Guest::CreateThreadPointer(GetCurrentThreadId());
        ctx.fpscr.loadFromHost();
        Kernel::SetCurrentContext(&ctx);

        auto nextFrame = std::chrono::steady_clock::now();
        while (video.running.load(std::memory_order_acquire))
        {
            UpdateFrameRateUnlocked(video.frames.load(std::memory_order_relaxed));
            nextFrame += VblankPeriod();
            // After a stall (a debugger, a long interrupt), carry on from now
            // rather than firing the missed blanks back to back.
            if (std::chrono::steady_clock::now() - nextFrame > std::chrono::milliseconds(100))
                nextFrame = std::chrono::steady_clock::now();

            // Two separate things, which looked like one and cost a while to
            // untangle. The command ring is the buffer VdInitializeRingBuffer
            // named, and the register at 0x714 says how far the title has
            // filled it. The field at 0x2A04 in the title's own structure is
            // the system command buffer, a different allocation that stays
            // empty here.
            // The word at offset 4 of the write back block is how far the GPU
            // has got: an address in the upper thirty bits and a two bit tag
            // below it. The render thread waits until that address reaches the
            // one it cares about.
            //
            // This used to be cycled through all four tag values from here, as
            // a stand in for not knowing what wrote it. The command stream
            // writes it: that is what an EVENT_WRITE_SHD packet is for. Now
            // those packets are carried out, forcing a tag from here would
            // overwrite the real answer with a rotating guess.

            const uint32_t callback = video.interruptCallback.load();
            if (callback != 0)
            {
                if (PPCFunc* routine = Guest::Lookup(callback))
                {
                    // The handler checks the interrupt status register before
                    // it does anything, so raising the interrupt means setting
                    // the pending bit as well as making the call.
                    Gpu::WriteRegister(Gpu::RegisterInterruptStatus,
                        Gpu::ReadRegister(Gpu::RegisterInterruptStatus) | 1);

                    ctx.r1.u32 = stackTop - 0x100;
                    ctx.r3.u32 = 0;   // source: vertical blank
                    ctx.r4.u32 = video.interruptContext.load();

                    // What the vertical blank does to the swap flag.
                    //
                    // The handler's vertical blank path counts down a word in
                    // its context and, when that reaches zero, clears the
                    // second word of the scratch block: the flag the stream
                    // waits on after every swap. Printing the countdown and the
                    // flag around each call says whether the countdown is ever
                    // armed, and if so whether the clear happens.
                    const uint32_t context = ctx.r4.u32;
                    const uint32_t block = Gpu::ReadRegister(Gpu::ApertureBase + 0x1DD * 4) & ~3u;
                    const uint32_t countdownBefore = context ? Guest::Read32(Guest::Base, context + 15132) : 0;
                    const uint32_t flagBefore = block ? Guest::Read32(Guest::Base, Guest::PhysicalAlias(block + 4)) : 0;

                    routine(ctx, Guest::Base);

                    static std::atomic<int> announced{ 0 };
                    if (block != 0 && (flagBefore != 0 || countdownBefore != 0) &&
                        (announced.load(std::memory_order_relaxed) < 24 && announced.fetch_add(1) < 24))
                    {
                        printf("vblank: countdown %u -> %u, swap flag %u -> %u, status 0x%08X\n",
                            countdownBefore, Guest::Read32(Guest::Base, context + 15132),
                            flagBefore, Guest::Read32(Guest::Base, Guest::PhysicalAlias(block + 4)),
                            Gpu::ReadRegister(Gpu::RegisterInterruptStatus));
                        fflush(stdout);
                    }
                }
            }

            const uint64_t frame = video.frames.fetch_add(1, std::memory_order_relaxed) + 1;
            Kernel::Stats().frames.store(frame, std::memory_order_relaxed);
            const uint64_t clockSecond = uint64_t(std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - started).count());
            const bool newSecond = clockSecond != lastSecond;
            lastSecond = clockSecond;

            // Without a picture, this line is the only evidence the title is
            // doing anything. Once a second is often enough to see movement
            // and rare enough not to bury the rest of the output.
            if (newSecond)
            {
                const auto& stats = Kernel::Stats();
                // The ring pointers say whether the command processor is
                // keeping up, and whether the size the driver asked for is
                // being interpreted the way the driver meant it.
                printf("ring: write pointer %u, read pointer %u, size %u dwords\n",
                    Gpu::WritePointer(), video.readPointer.load(),
                    video.ringBufferSize.load() / 4);

                // What sits at the read pointer when it is behind the write
                // pointer: the packet the command processor has not taken.
                {
                    const uint32_t ringAt = video.ringBuffer.load();
                    const uint32_t dwords = video.ringBufferSize.load() / 4;
                    const uint32_t read = video.readPointer.load();
                    const uint32_t write = Gpu::WritePointer();
                    if (ringAt != 0 && dwords != 0 && read != write)
                    {
                        printf("  at the read pointer:");
                        for (uint32_t i = 0; i < 12; i++)
                        {
                            const uint32_t at = (read + i) % dwords;
                            printf(" %08X", Guest::Read32(Guest::Base,
                                Guest::PhysicalAlias(ringAt) + at * 4));
                        }
                        printf("\n");
                    }
                    else if (ringAt != 0 && dwords != 0 && read + 1 >= dwords)
                    {
                        // The pointers at the very end of the ring: what the
                        // last packets before the wrap were, and what sits at
                        // the start.
                        printf("  behind the pointer:");
                        for (uint32_t i = 8; i >= 1; i--)
                            printf(" %08X", Guest::Read32(Guest::Base,
                                Guest::PhysicalAlias(ringAt) + ((read + dwords - i) % dwords) * 4));
                        printf("  | at the pointer and after:");
                        for (uint32_t i = 0; i < 6; i++)
                            printf(" %08X", Guest::Read32(Guest::Base,
                                Guest::PhysicalAlias(ringAt) + ((read + i) % dwords) * 4));
                        printf("\n");

                    }
                }
                // The title's indirect buffer pool and the fence it
                // waits on before reusing part of it: the device's
                // fields at 0x34AC.. and the write-back block's words.
                const uint32_t device = video.interruptContext.load();
                if (device != 0)
                {
                    const uint32_t blockAt = Guest::Read32(Guest::Base, device + 0x2A10);
                    printf("  ib pool: base %08X end %08X min %08X at %08X lap %u start %08X limit %08X | gpu %08X lap %u\n",
                        Guest::Read32(Guest::Base, device + 0x34AC), Guest::Read32(Guest::Base, device + 0x34B0),
                        Guest::Read32(Guest::Base, device + 0x34B4), Guest::Read32(Guest::Base, device + 0x34B8),
                        Guest::Read32(Guest::Base, device + 0x34BC), Guest::Read32(Guest::Base, device + 0x34C0),
                        Guest::Read32(Guest::Base, device + 0x34C4), Guest::Read32(Guest::Base, device + 0x3290),
                        Guest::Read32(Guest::Base, device + 0x3294));
                    if (blockAt != 0)
                    {
                        printf("  write back block %08X:", blockAt);
                        for (uint32_t i = 0; i < 8; i++)
                            printf(" %08X", Guest::Read32(Guest::Base, Guest::PhysicalAlias(blockAt) + i * 4));
                        printf("\n");
                    }
                }
                if (g_inHandler.load())
                {
                    const int64_t held = NowMilliseconds() - g_handlerSince.load();
                    if (held > 500)
                    {
                        printf("cp: inside the title's graphics interrupt handler for %.1f s; "
                               "the stream is stopped behind it\n", held / 1000.0);
                        Kernel::ReportRecentCalls(g_commandThreadId.load());
                    }
                }

                // The write back block and the fields around the ring
                // description. Which field the render thread compares against
                // the progress pointer is not known yet, so the whole
                // neighbourhood is printed and the one that moves is the one
                // that matters.
                const uint32_t identifier = video.gpuIdentifier.load();
                if (identifier >= 8)
                {
                    const uint32_t blockAt = identifier - 8;
                    printf("block:");
                    for (int i = 0; i < 6; i++)
                        printf(" %08X", Guest::Read32(Guest::Base, blockAt + i * 4));
                    printf("\n");
                }

                // Strings the title keeps near the code that is stuck. Its own
                // words about what it is waiting for are worth more than any
                // guess made from the outside.
                {
                    static bool once = false;
                    if (!once)
                    {
                        once = true;
                        for (uint32_t at = 0x82A8DE00; at < 0x82A8E100; at++)
                        {
                            const char* text =
                                reinterpret_cast<const char*>(Guest::Ptr(at));
                            size_t length = 0;
                            while (length < 90 && text[length] >= 0x20 &&
                                   text[length] < 0x7F)
                                length++;
                            if (length >= 12 && text[length] == 0)
                            {
                                printf("string 0x%08X: %.*s\n", at, int(length), text);
                                at += uint32_t(length);
                            }
                        }
                        fflush(stdout);
                    }
                }

                // The render thread waits on the word four bytes into whatever
                // this field points at. Printing the pointer and what is around
                // it is the only way to tell whether the fences the command
                // stream writes are landing in the block it is watching.
                const uint32_t deviceContext = video.interruptContext.load();
                if (deviceContext != 0)
                {
                    const uint32_t watched =
                        Guest::Read32(Guest::Base, deviceContext + 0x2A10);
                    printf("watched: 0x%08X ->", watched);
                    if (watched != 0)
                    {
                        const uint32_t at = Guest::PhysicalAlias(watched);
                        for (int i = 0; i < 4; i++)
                            printf(" %08X", Guest::Read32(Guest::Base, at + i * 4));
                    }
                    printf("\n");
                }

                const uint32_t context = video.interruptContext.load();
                if (context != 0)
                {
                    printf("ctx:  ");
                    for (uint32_t offset = 0x2A00; offset <= 0x2A3C; offset += 4)
                        printf(" %08X", Guest::Read32(Guest::Base, context + offset));
                    printf("\n");
                }

                Gpu::ReportPacketMix();
                Render::Report();
                Kernel::ReportHeaps();
                PoolTrace::Report();
                if (!Timeline::OnStallOnly()) Timeline::Report();
                Kernel::ReportImports();
                Kernel::ReportApcs();
                Scheduler::Report();
                Audio::Report();
                Kernel::ReportXma();
                Gpu::ReportFences();
                Kernel::ReportFileReads();
                Gpu::ReportPolling();
                if (clockSecond >= 10) Kernel::ReportWaitTraffic();
                // COD3_FOVSCAN=second[,fov]: at that second, where the view's
                // field is kept: floats in the title's memory that are the
                // tangents (or angles) of half the field of cg_fov fov (65)
                // across at 16:9 and up and down, two within 64 bytes.
                if (const char* scan = getenv("COD3_FOVSCAN"); scan && clockSecond == uint64_t(atoi(scan)))
                {
                    const char* comma = strchr(scan, ',');
                    const double fov = comma ? atof(comma + 1) : 65.0;
                    const double ty = std::tan(fov * 0.5 * 3.14159265358979 / 180.0) * 0.75, tx = ty * 16.0 / 9.0;
                    const float wanted[4] = { float(tx), float(ty), float(2.0 * std::atan(tx) * 180.0 / 3.14159265358979), float(2.0 * std::atan(ty) * 180.0 / 3.14159265358979) };
                    printf("fovscan: looking for %.4f %.4f %.3f %.3f\n", wanted[0], wanted[1], wanted[2], wanted[3]);
                    int found = 0;
                    for (uint64_t at = 0x40000000; at < 0xA0000000 && found < 60; )
                    {
                        MEMORY_BASIC_INFORMATION info{};
                        uint8_t* host = reinterpret_cast<uint8_t*>(Guest::Base) + at;
                        if (VirtualQuery(host, &info, sizeof(info)) == 0) break;
                        const uint64_t end = at + uint64_t(reinterpret_cast<uint8_t*>(info.BaseAddress) + info.RegionSize - host);
                        if (info.State == MEM_COMMIT && (info.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READWRITE)) && !(info.Protect & PAGE_GUARD))
                            for (uint64_t a = at; a + 4 <= end && found < 60; a += 4)
                            {
                                uint32_t raw; memcpy(&raw, reinterpret_cast<uint8_t*>(Guest::Base) + a, 4);
                                raw = _byteswap_ulong(raw); float v; memcpy(&v, &raw, 4);
                                int kind = -1;
                                for (int k = 0; k < 4; k++) if (std::fabs(v - wanted[k]) < wanted[k] * 2e-4f) kind = k;
                                if (kind < 0) continue;
                                for (uint64_t b = a + 4; b < a + 64 && b + 4 <= end; b += 4)
                                {
                                    uint32_t r2; memcpy(&r2, reinterpret_cast<uint8_t*>(Guest::Base) + b, 4);
                                    r2 = _byteswap_ulong(r2); float w; memcpy(&w, &r2, 4);
                                    for (int k = 0; k < 4; k++)
                                        if (k != kind && std::fabs(w - wanted[k]) < wanted[k] * 2e-4f)
                                        {
                                            printf("fovscan: %08llX = %.4f (%d), %08llX = %.4f (%d)\n", (unsigned long long)a, v, kind, (unsigned long long)b, w, k);
                                            found++;
                                        }
                                }
                            }
                        at = end;
                    }
                    printf("fovscan: %d pairs\n", found);
                    fflush(stdout);
                }
                // COD3_FOVWATCH=second,hexaddress[,r]: the watchpoint armed then.
                if (const char* fw = getenv("COD3_FOVWATCH"); fw && clockSecond == uint64_t(atoi(fw)))
                    if (const char* comma = strchr(fw, ','))
                    {
                        Kernel::WatchWrite(uint32_t(strtoul(comma + 1, nullptr, 16)), strstr(comma + 1, ",r") != nullptr);
                        Kernel::ArmWatchpoints();
                    }
                if (Sampler::ThreadTimesWanted() && clockSecond % 5 == 0)
                    Sampler::ReportThreadTimes({
                        { GetThreadId(video.commandThread.native_handle()), "command processor" },
                        { GetThreadId(video.thread.native_handle()), "video" },
                        { GetCurrentThreadId(), "video" } });
                const Gpu::Statistics gpu = Gpu::Stats();
                static uint64_t swapsBefore = 0;
                const uint64_t swaps = video.swaps.load();
                printf("[%4llus] fps %llu  frames %llu  files %llu (%.1f MB read)  threads %llu  "
                       "allocated %.1f MB  audio %llu  packets %llu  draws %llu\n",
                    (unsigned long long)clockSecond,
                    (unsigned long long)(swaps - swapsBefore),
                    (unsigned long long)frame,
                    (unsigned long long)stats.filesOpened.load(),
                    stats.fileBytesRead.load() / 1048576.0,
                    (unsigned long long)stats.threadsCreated.load(),
                    stats.bytesAllocated.load() / 1048576.0,
                    (unsigned long long)stats.audioFrames.load(),
                    (unsigned long long)gpu.packets,
                    (unsigned long long)gpu.draws);
                swapsBefore = swaps;
                // The processor's clock now, against its most: a machine
                // that slows itself down (heat, or a power limit that lets it
                // run fast for half a minute) shows here.
                {
                    struct PowerInfo { ULONG number, maxMhz, currentMhz, limitMhz, maxIdle, currentIdle; };
                    using CallPower = LONG(WINAPI*)(int, void*, ULONG, void*, ULONG);
                    static CallPower callPower = reinterpret_cast<CallPower>(
                        GetProcAddress(LoadLibraryA("powrprof.dll"), "CallNtPowerInformation"));
                    SYSTEM_INFO system{};
                    GetSystemInfo(&system);
                    std::vector<PowerInfo> cores(system.dwNumberOfProcessors ? system.dwNumberOfProcessors : 1);
                    if (callPower && callPower(11 /* ProcessorInformation */, nullptr, 0, cores.data(),
                                               ULONG(cores.size() * sizeof(PowerInfo))) == 0)
                    {
                        unsigned long long current = 0, most = 0, limit = 0;
                        for (const PowerInfo& core : cores) { current += core.currentMhz; most += core.maxMhz; limit += core.limitMhz; }
                        printf("cpu clock: %llu of %llu MHz (limit %llu), %zu threads\n", current / cores.size(), most / cores.size(),
                               limit / cores.size(), cores.size());
                    }
                }
                fflush(stdout);

                // COD3_SCANMEM=text: every ten seconds, where that text is
                // in the guest's memory, with what is around it: for finding
                // the title's own data by a string it holds.
                static const char* const scanFor = getenv("COD3_SCANMEM");
                if (scanFor != nullptr && scanFor[0] != 0 && newSecond && clockSecond % 10 == 5)
                {
                    const size_t needle = strlen(scanFor);
                    int found = 0;
                    uint64_t address = 0;
                    while (address < 0x100000000ull && found < 20)
                    {
                        MEMORY_BASIC_INFORMATION info{};
                        if (VirtualQuery(Guest::Base + address, &info, sizeof(info)) == 0) break;
                        const uint64_t regionEnd = uint64_t(static_cast<uint8_t*>(info.BaseAddress) - Guest::Base) + info.RegionSize;
                        if ((info.State & MEM_COMMIT) != 0 && (info.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE)) != 0)
                        {
                            const uint8_t* at = static_cast<const uint8_t*>(info.BaseAddress);
                            for (size_t i = 0; i + needle <= info.RegionSize && found < 20; i++)
                            {
                                if (at[i] != uint8_t(scanFor[0]) || memcmp(at + i, scanFor, needle) != 0) continue;
                                const uint64_t guest = uint64_t(at + i - Guest::Base);
                                std::string around;
                                for (size_t k = (i >= 48 ? i - 48 : 0); k < i + needle + 80 && k < info.RegionSize; k++)
                                    around += (at[k] >= 32 && at[k] < 127) ? char(at[k]) : (at[k] == 0 ? '|' : '.');
                                printf("scan: \"%s\" at 0x%08llX: %s\n", scanFor, (unsigned long long)guest, around.c_str());
                                found++;
                            }
                        }
                        address = regionEnd;
                    }
                    if (found == 0) printf("scan: \"%s\" is nowhere in guest memory\n", scanFor);
                    fflush(stdout);
                }
                // COD3_MAP=name[,second]: the level started from the front
                // end at that second (twelve by default; the main menu is
                // up by then with COD3_PAD="5:start 8:a") by the console
                // command the chapter select itself uses, "spmap name",
                // through the title's va (sub_824534B0, a format and its
                // arguments, the text kept in the title's own pool) and
                // Cbuf_AddText (sub_824644D0). This thread has a guest
                // context and a stack of its own, so it can call them; the
                // command runs on the title's own thread on its next pass
                // over the buffer.
                static const char* const mapWanted = getenv("COD3_MAP");
                static bool mapStarted = false;
                if (mapWanted != nullptr && !mapStarted)
                {
                    std::string name(mapWanted);
                    long second = 12;
                    if (const size_t comma = name.find(','); comma != std::string::npos) { second = strtol(name.c_str() + comma + 1, nullptr, 10); name.resize(comma); }
                    if (long(clockSecond) == second && !name.empty() && name.size() < 64)
                    {
                        mapStarted = true;
                        Kernel::QueueConsoleCommand("spmap " + name);
                    }
                }
                // COD3_CMD="second:command;second:command": console commands
                // at those seconds, for a scripted run (cg_fov 90, r_...).
                {
                    static const char* const cmdWanted = getenv("COD3_CMD");
                    static std::vector<std::pair<long, std::string>> pending = []() {
                        std::vector<std::pair<long, std::string>> list;
                        if (cmdWanted == nullptr) return list;
                        std::string all(cmdWanted);
                        size_t at = 0;
                        while (at < all.size())
                        {
                            size_t semi = all.find(';', at); if (semi == std::string::npos) semi = all.size();
                            const std::string item = all.substr(at, semi - at);
                            const size_t colon = item.find(':');
                            if (colon != std::string::npos) list.emplace_back(strtol(item.c_str(), nullptr, 10), item.substr(colon + 1));
                            at = semi + 1;
                        }
                        return list;
                    }();
                    for (size_t i = 0; i < pending.size(); i++)
                        if (long(clockSecond) == pending[i].first) { Kernel::QueueConsoleCommand(pending[i].second); pending.erase(pending.begin() + i); break; }
                }
                // COD3_PICTURES=second: what the screenshot key does, at that
                // second: the next frames logged in full and the next one's
                // draws pictured one by one (screenshots beside the exe).
                {
                    static const char* const picturesAt = getenv("COD3_PICTURES");
                    static bool pictured = false;
                    if (picturesAt != nullptr && !pictured && long(clockSecond) == strtol(picturesAt, nullptr, 10))
                    {
                        pictured = true;
                        Render::LogNextFrame();
                    }
                }
                // COD3_WIN="second:level": the mission won at that second, as
                // the level script's missionsuccess (table slot 0x4A4,
                // sub_824A4F38) marks it in the game state at 0x82A4E790:
                // +2760 success, +2764 failed cleared, +2752 the end asked
                // for, +2780 the next level's name. For testing the way
                // from one mission to the next.
                {
                    static const char* const winWanted = getenv("COD3_WIN");
                    static bool won = false;
                    if (winWanted != nullptr && !won && long(clockSecond) == strtol(winWanted, nullptr, 10))
                    {
                        won = true;
                        const char* colon = strchr(winWanted, ':');
                        const std::string next = colon != nullptr ? std::string(colon + 1) : std::string();
                        constexpr uint32_t state = 0x82A4E790u;
                        for (size_t i = 0; i < 255; i++)
                            Guest::Write8(Guest::Base, state + 2780 + uint32_t(i), i < next.size() ? uint8_t(next[i]) : 0);
                        Guest::Write32(Guest::Base, state + 2772, 0);
                        Guest::Write32(Guest::Base, state + 2776, 0);
                        Guest::Write32(Guest::Base, state + 2760, 1);
                        Guest::Write32(Guest::Base, state + 2764, 0);
                        Guest::Write32(Guest::Base, state + 2752, 1);
                        printf("video: the mission marked won, next \"%s\"\n", next.c_str());
                        fflush(stdout);
                    }
                }
                // COD3_STRINGS="prefix,prefix[,second]": the strings in the
                // title's image that start so, listed once, that many
                // seconds in (twenty by default): what its console knows.
                {
                    static const char* const stringsWanted = getenv("COD3_STRINGS");
                    static bool listed = false;
                    if (stringsWanted != nullptr && !listed)
                    {
                        std::vector<std::string> prefixes;
                        long second = 20;
                        std::string all(stringsWanted);
                        size_t at = 0;
                        while (at <= all.size())
                        {
                            size_t comma = all.find(',', at); if (comma == std::string::npos) comma = all.size();
                            const std::string item = all.substr(at, comma - at);
                            if (!item.empty() && item.find_first_not_of("0123456789") == std::string::npos) second = strtol(item.c_str(), nullptr, 10);
                            else if (!item.empty()) prefixes.push_back(item);
                            at = comma + 1;
                        }
                        if (long(clockSecond) == second)
                        {
                            listed = true;
                            const uint32_t first = 0x82000000u, last = 0x82000000u + (16u << 20);
                            std::string found;
                            uint32_t count = 0;
                            for (uint32_t a = first; a + 4 < last; a++)
                            {
                                const uint8_t* p = Guest::Base + a;
                                if (a > first && p[-1] != 0) continue;   // the start of a string
                                for (const std::string& prefix : prefixes)
                                {
                                    if (memcmp(p, prefix.data(), prefix.size()) != 0) continue;
                                    size_t n = 0;
                                    while (n < 64 && p[n] >= 0x20 && p[n] < 0x7F) n++;
                                    if (n < prefix.size() + 1 || p[n] != 0) break;
                                    found += std::string(reinterpret_cast<const char*>(p), n) + ' ';
                                    count++;
                                    break;
                                }
                            }
                            printf("strings: %u in the image starting with %s: %s\n", count, stringsWanted, found.c_str());
                            fflush(stdout);
                        }
                    }
                }
                // The settings that are the title's own console variables:
                // at first what differs from the title's own, then when they
                // change; once a second is often enough. COD3_EXEC="command;
                // command" adds commands of its own at the start, for a run.
                if (newSecond)
                {
                    const std::string changed = Settings::TitleCommandsChanged();
                    if (!changed.empty()) Kernel::QueueConsoleCommand(changed);
                    static bool execQueued = false;
                    if (!execQueued)
                    {
                        execQueued = true;
                        if (const char* exec = getenv("COD3_EXEC"))
                        {
                            std::string lines(exec);
                            for (char& c : lines) if (c == ';') c = '\n';
                            Kernel::QueueConsoleCommand(lines);
                        }
                    }
                }
                // The host console's commands, and COD3_MAP's, onto the
                // title's command buffer: the text through the title's va
                // (sub_824534B0), which keeps it in its own pool, and
                // Cbuf_AddText (sub_824644D0). A percent sign is doubled
                // on the way, since va takes a format.
                for (std::string command = TakeConsoleCommand(); !command.empty(); command = TakeConsoleCommand())
                {
                    if (command.rfind("noclip", 0) == 0) Input::ArmNoclipTap();   // 0.5.454: a double tap of A from now on
#ifdef COD3_MP
                    // Multiplayer: the text straight to Cbuf_AddText
                    // (sub_8247FA38, which copies it), from a block of
                    // our own that lives as long as the game.
                    PPCFunc* format = nullptr;
                    PPCFunc* addText = Guest::Lookup(0x8247FA38);
#else
                    PPCFunc* format = Guest::Lookup(0x824534B0);
                    PPCFunc* addText = Guest::Lookup(0x824644D0);
#endif
#ifdef COD3_MP
                    // spawnvehicle <kind>: done by the host's code (mp_fixes.cpp).
                    // kangaroo 0/1: the Kangaroo APC set up on the maps after
                    // (mp_fixes.cpp).
                    // kangaroo_type 1/2: the Kangaroo driven as four wheel or tank.
                    // kangaroo_tread 0-6 (0.5.88): how the Kangaroo's right tread
                    // is moved while it drives, for finding the right way.
                    if (command.compare(0, 15, "kangaroo_tread ") == 0)
                    {
                        MpFixesKangarooTread(atoi(command.c_str() + 15));
                        continue;
                    }
                    // kangaroo_build 0/1/2 (0.5.99): how the Kangaroo is made at
                    // the map's start: 1 (the default) made a Kangaroo from the
                    // start (its model, kind and the tanks' physics before its
                    // physics is set up), 2 the same with the Horch's physics,
                    // 0 the Horch made over afterwards as before.
                    if (command.compare(0, 14, "kangaroo_drop ") == 0)
                    {
                        MpFixesKangarooDrop(float(atof(command.c_str() + 14)));
                        continue;
                    }
                    if (command.compare(0, 14, "kangaroo_exit ") == 0)
                    {
                        MpFixesKangarooExit(atoi(command.c_str() + 14));
                        continue;
                    }
                    if (command.compare(0, 15, "kangaroo_entry ") == 0)
                    {
                        MpFixesKangarooEntry(atoi(command.c_str() + 15));
                        continue;
                    }
                    if (command.compare(0, 15, "kangaroo_build ") == 0)
                    {
                        MpFixesKangarooBuild(atoi(command.c_str() + 15));
                        continue;
                    }
                    if (command.compare(0, 14, "kangaroo_type ") == 0)
                    {
                        MpFixesKangarooType(atoi(command.c_str() + 14));
                        continue;
                    }
                    // 0.5.791: "tankblast x y z event": a tank shell's explosion
                    // (the named event on one of our origins, as a shell's hit
                    // makes it) where asked, for a test to look at (tests cannot
                    // fire a tank's gun)
                    if (command.compare(0, 10, "tankblast ") == 0)
                    {
                        float at[3] = {};
                        char event[96] = {};
                        if (sscanf(command.c_str() + 10, "%f %f %f %95s", &at[0], &at[1], &at[2], event) == 4) MpFixesQueueTankBlast(at, event);
                        printf("video: tankblast %s at %.0f %.0f %.0f\n", event, at[0], at[1], at[2]);
                        fflush(stdout);
                        continue;
                    }
                    // 0.5.796: "tank_fire [seconds]": a bot gets into the tank
                    // nearest the host and fires its gun (bots.cpp), for tests
                    if (command.compare(0, 9, "tank_fire") == 0)
                    {
                        int seconds = 60, crew = 2;
                        if (command.size() > 10) sscanf(command.c_str() + 10, "%d %d", &seconds, &crew);
                        BotsTankFire(seconds, crew);
                        continue;
                    }
                    if (command.compare(0, 9, "kangaroo ") == 0 || command == "kangaroo")
                    {
                        const bool on = command.size() <= 9 || atoi(command.c_str() + 9) != 0;
                        MpFixesKangaroo(on);
                        printf("video: kangaroo %d (from the next map loaded)\n", on ? 1 : 0);
                        fflush(stdout);
                        continue;
                    }
                    // vehicles_everywhere 0/1: Merville's vehicles loaded on the
                    // maps after (mp_fixes.cpp).
                    if (command.compare(0, 19, "vehicles_everywhere") == 0)
                    {
                        const bool on = atoi(command.c_str() + 19) != 0;
                        MpFixesVehiclesEverywhere(on);
                        printf("video: vehicles_everywhere %d (from the next map loaded)\n", on ? 1 : 0);
                        fflush(stdout);
                        continue;
                    }
                    // vehicle_mark <kind>, vehicle_clear, vehicle_list: the map's
                    // placed vehicles (vehicles.txt, mp_fixes.cpp).
                    if (command.compare(0, 12, "vehicle_mark") == 0 || command.compare(0, 13, "vehicle_clear") == 0 ||
                        command.compare(0, 12, "vehicle_list") == 0)
                    {
                        const size_t space = command.find(' ');
                        std::string kind = space == std::string::npos ? std::string() : command.substr(space + 1);
                        while (!kind.empty() && (kind.back() == ' ' || kind.back() == '\r' || kind.back() == '\n')) kind.pop_back();
                        const int what = command.compare(0, 12, "vehicle_mark") == 0 ? 2 : command.compare(0, 13, "vehicle_clear") == 0 ? 3 : 4;
                        if (what == 2 && kind.empty()) kind = "sherman";
                        MpFixesVehicleOrder(what, kind);
                        printf("video: %s asked for\n", command.c_str());
                        fflush(stdout);
                        continue;
                    }
                    // streak_give <uav|artillery|typhoon>: a killstreak for the host's
                    // own player, for testing (mp_fixes.cpp).
                    // 0.5.801: mortar_test [rounds]: the host's mortar set up and fired (mp_fixes.cpp)
                    if (command.compare(0, 11, "mortar_test") == 0)
                    {
                        MpFixesMortarTest(command.size() > 12 ? atoi(command.c_str() + 12) : 10);
                        continue;
                    }
                    if (command.compare(0, 11, "streak_give") == 0)
                    {
                        std::string kind = command.size() > 12 ? command.substr(12) : std::string();
                        while (!kind.empty() && (kind.back() == ' ' || kind.back() == '\r' || kind.back() == '\n')) kind.pop_back();
                        MpFixesGiveStreak(kind);
                        printf("video: streak_give %s asked for\n", kind.c_str());
                        fflush(stdout);
                        continue;
                    }
                    // plane_sound <plane|origin|me|bot> <flyby|engine|flag> (0.5.264):
                    // where the UAV plane's sound is played and which, played once now.
                    if (command.compare(0, 11, "plane_sound") == 0)
                    {
                        std::string args = command.size() > 12 ? command.substr(12) : std::string();
                        while (!args.empty() && (args.back() == ' ' || args.back() == '\r' || args.back() == '\n')) args.pop_back();
                        MpFixesPlaneSoundTest(args);
                        printf("video: plane_sound %s asked for\n", args.c_str());
                        fflush(stdout);
                        continue;
                    }
                    // mortar_view <forward> <right> <up> (0.5.309): the camera at the mortar, live.
                    if (command.compare(0, 11, "mortar_view") == 0)
                    {
                        std::string args = command.size() > 12 ? command.substr(12) : std::string();
                        MpFixesMortarView(args);
                        printf("video: mortar_view %s asked for\n", args.c_str());
                        fflush(stdout);
                        continue;
                    }
                    // mortar_yaw <deg> (0.5.303): the mortar tube's facing, live.
                    if (command.compare(0, 10, "mortar_yaw") == 0)
                    {
                        std::string args = command.size() > 11 ? command.substr(11) : std::string("0");
                        MpFixesMortarYaw(args);
                        printf("video: mortar_yaw %s asked for\n", args.c_str());
                        fflush(stdout);
                        continue;
                    }
                    // plane_engine <cue> <ms> (0.5.272): the UAV plane's engine sound.
                    if (command.compare(0, 12, "plane_engine") == 0)
                    {
                        std::string args = command.size() > 13 ? command.substr(13) : std::string();
                        while (!args.empty() && (args.back() == ' ' || args.back() == '\r' || args.back() == '\n')) args.pop_back();
                        MpFixesPlaneEngine(args);
                        printf("video: plane_engine %s asked for\n", args.c_str());
                        fflush(stdout);
                        continue;
                    }
                    // plane_model <name> [clear] (0.5.267): a model in front of you.
                    if (command.compare(0, 11, "plane_model") == 0)
                    {
                        std::string args = command.size() > 12 ? command.substr(12) : std::string();
                        while (!args.empty() && (args.back() == ' ' || args.back() == '\r' || args.back() == '\n')) args.pop_back();
                        MpFixesPlaneModelTest(args);
                        printf("video: plane_model %s asked for\n", args.c_str());
                        fflush(stdout);
                        continue;
                    }
                    if (command.compare(0, 13, "scope_breath ") == 0)
                    {
                        Input::SetScopedBreath(atoi(command.c_str() + 13) != 0);
                        continue;
                    }
                    if (command.compare(0, 12, "bot_tactics ") == 0)
                    {
                        BotsTactics(atoi(command.c_str() + 12));
                        continue;
                    }
                    // 0.5.206: bot_pose <guns> [distance] [facing] / bot_pose off,
                    // bot_pose_test [distance] (bots.cpp: how guns sit in hands).
                    if (command.compare(0, 13, "bot_pose_test") == 0)
                    {
                        BotsPoseTest(command.substr(13));
                        continue;
                    }
                    if (command.compare(0, 9, "bot_pose ") == 0)
                    {
                        BotsPose(command.substr(9));
                        continue;
                    }
                    if (command.compare(0, 11, "bot_camera ") == 0)
                    {
                        BotsCamera(atoi(command.c_str() + 11));
                        continue;
                    }
                    // bot_setup <bits> (0.5.92): 1 the bots' kit left out, 2 their side switch.
                    // bot_remote 0/1 (0.5.135): new bots get their own machine object.
                    if (command.compare(0, 10, "bot_local ") == 0)
                    {
                        BotsLocal(atoi(command.c_str() + 10));
                        continue;
                    }
                    if (command.compare(0, 11, "bot_remote ") == 0)
                    {
                        BotsRemote(atoi(command.c_str() + 11));
                        continue;
                    }
                    if (command.compare(0, 10, "bot_setup ") == 0)
                    {
                        BotsSetup(atoi(command.c_str() + 10));
                        continue;
                    }
                    // bot_add <n>: bots on the host until there are n (bots.cpp).
                    if (command.compare(0, 7, "bot_add") == 0)
                    {
                        const int count = atoi(command.c_str() + 7);
                        BotsAsk(count);
                        printf("video: bot_add %d asked for\n", count);
                        fflush(stdout);
                        continue;
                    }
                    if (command.compare(0, 12, "spawnvehicle") == 0 || command.compare(0, 12, "vehiclespawn") == 0 ||
                        command.compare(0, 13, "vechiclespawn") == 0 || command.compare(0, 8, "vehicle ") == 0)
                    {
                        // (The kind after the first space; vehiclespawn and
                        // vehicle are the same command.)
                        const size_t space = command.find(' ');
                        std::string kind = space == std::string::npos ? std::string() : command.substr(space);
                        while (!kind.empty() && (kind.front() == ' ' || kind.front() == '\t')) kind.erase(0, 1);
                        while (!kind.empty() && (kind.back() == ' ' || kind.back() == '\r' || kind.back() == '\n')) kind.pop_back();
                        MpFixesAskVehicle(kind);
                        printf("video: spawnvehicle %s asked for\n", kind.c_str());
                        fflush(stdout);
                        continue;
                    }
                    // g_gametype (the PC Call of Duty name) is mp_gametype here
                    // (dm, tdm, ctf, hq, scf, dom, war, sd); taken either way.
                    if (command.compare(0, 11, "g_gametype ") == 0) command = "mp_gametype " + command.substr(11);
                    if (addText != nullptr)
                    {
                        static uint32_t block = 0;
                        if (!block) block = Guest::AllocatePhysical(0x1000);
                        std::string text = command + "\n";
                        if (text.size() > 0xF00) text.resize(0xF00);
                        for (size_t i = 0; i <= text.size(); i++) Guest::Base[block + i] = uint8_t(i < text.size() ? text[i] : 0);
                        ctx.r1.u32 = stackTop - 0x1100;
                        ctx.r3.u32 = block;
                        addText(ctx, Guest::Base);
                        printf("video: \"%s\" put on the title's command buffer\n", command.c_str());
                        fflush(stdout);
                        continue;
                    }
#endif
                    if (format == nullptr || addText == nullptr)
                    {
                        printf("video: the title's Cbuf_AddText is not recompiled code; \"%s\" not run\n", command.c_str());
                        continue;
                    }
                    std::string text;
                    for (char c : command) { if (c == '%') text += '%'; text += c; }
                    text += '\n';
                    if (text.size() > 0xF00) text.resize(0xF00);
                    const uint32_t at = stackTop - 0x1000;
                    for (size_t i = 0; i <= text.size(); i++) Guest::Base[at + i] = uint8_t(i < text.size() ? text[i] : 0);
                    ctx.r1.u32 = stackTop - 0x1100;
                    ctx.r3.u32 = at;
                    format(ctx, Guest::Base);
                    ctx.r1.u32 = stackTop - 0x1100;
                    addText(ctx, Guest::Base);
                    printf("video: \"%s\" put on the title's command buffer\n", command.c_str());
                    fflush(stdout);
                }
                // COD3_WATCH_SECOND=N with COD3_WATCH: the watch armed now.
                {
                    static const long watchSecond = []() { const char* t = getenv("COD3_WATCH_SECOND"); return t ? strtol(t, nullptr, 10) : -1; }();
                    static bool watchArmed = false;
                    if (watchSecond >= 0 && !watchArmed && long(clockSecond) == watchSecond)
                    {
                        watchArmed = true;
                        const char* watch = getenv("COD3_WATCH");
                        const uint32_t address = watch ? uint32_t(strtoul(watch, nullptr, 16)) : 0;
                        if (address != 0)
                        {
                            Kernel::WatchWrite(address, strchr(watch, ',') != nullptr && strchr(watch, 'r') != nullptr);
                            Kernel::ArmWatchpoints();
                        }
                    }
                }
#ifdef COD3_MP
                // 0.5.556: the renderer list 82C89A60's count (+8) watched in
                // every multiplayer game from 15 s, to name whatever writes
                // float data over the list (count 0x3F800000, 1.0; the 0.5.185
                // guard repairs the list, the writer never found). 555's
                // freeze followed five of these, the last a moment before the
                // game's malloc (sub_820DF428) looped on a broken heap. Off with
                // COD3_LISTWATCH=0; not when COD3_WATCH names another word.
                {
                    static bool listWatchArmed = false;
                    static const bool listWatch = []() { const char* t = getenv("COD3_LISTWATCH"); return !(t && t[0] == '0') && getenv("COD3_WATCH") == nullptr; }();
                    if (listWatch && !listWatchArmed && clockSecond >= 15)
                    {
                        listWatchArmed = true;
                        Kernel::WatchWrite(0x82C89A68u, false);
                        Kernel::WatchSuspiciousAbove(100000u);
                        Kernel::ArmWatchpoints();
                    }
                }
#endif
                // COD3_DUMPMEM=hexaddress,bytes,second: that much of guest
                // memory, once, that many seconds in (five by default), as
                // words and text.
                static const char* const dumpMem = getenv("COD3_DUMPMEM");
                static bool dumped = false;
                static const long dumpSecond = []() {
                    const char* t = getenv("COD3_DUMPMEM"); if (t == nullptr) return 5L;
                    const char* comma = strchr(t, ','); if (comma == nullptr) return 5L;
                    comma = strchr(comma + 1, ','); return comma != nullptr ? strtol(comma + 1, nullptr, 10) : 5L; }();
                // COD3_HEAPWALK=hexmstate,second: the title's malloc bins
                // walked from its malloc state at that second, each list
                // followed by fd until it returns to its head or repeats:
                // where a list has a cycle or a chunk whose size is not
                // the bin's, the corruption a hung malloc loops over.
                {
                    static const char* const walkWanted = getenv("COD3_HEAPWALK");
                    static bool walked = false;
                    if (walkWanted != nullptr && !walked)
                    {
                        char* end = nullptr;
                        const uint32_t mstate = uint32_t(strtoul(walkWanted, &end, 16));
                        const long second = (end != nullptr && *end == ',') ? strtol(end + 1, nullptr, 10) : 60L;
                        if (long(clockSecond) == second)
                        {
                            walked = true;
                            auto readable = [&](uint32_t a) {
                                MEMORY_BASIC_INFORMATION info{};
                                return a >= 0x10000 && a < 0xC0000000u && VirtualQuery(Guest::Base + a, &info, sizeof(info)) != 0 && (info.State & MEM_COMMIT) != 0 && info.Protect != PAGE_NOACCESS;
                            };
                            auto word = [&](uint32_t a) { return readable(a) ? Guest::Read32(Guest::Base, a) : 0xDEADBEEFu; };
                            printf("heapwalk: malloc state at %08X, top %08X, unsorted %08X/%08X\n", mstate, word(mstate + 24), word(mstate + 52 + 8), word(mstate + 52 + 12));
                            for (uint32_t bin = 1; bin < 128; bin++)
                            {
                                const uint32_t head = mstate + 44 + (bin << 3);
                                uint32_t at = word(head + 8);
                                if (at == head || at == 0xDEADBEEFu) continue;
                                printf("heapwalk: bin %u at %08X:", bin, head);
                                std::vector<uint32_t> seen;
                                int steps = 0;
                                bool broken = false;
                                while (at != head && steps++ < 4000)
                                {
                                    if (!readable(at + 12)) { printf(" [%08X unreadable]", at); broken = true; break; }
                                    const uint32_t size = word(at + 4), fd = word(at + 8), bk = word(at + 12);
                                    if (steps <= 6) printf(" %08X(size %X fd %08X bk %08X)", at, size, fd, bk);
                                    if (std::find(seen.begin(), seen.end(), at) != seen.end()) { printf(" [CYCLE back to %08X after %d]", at, steps); broken = true; break; }
                                    seen.push_back(at);
                                    at = fd;
                                }
                                if (steps >= 4000) { printf(" [no end after 4000]"); broken = true; }
                                printf(" : %d chunks%s\n", steps, broken ? " BROKEN" : "");
                            }
                            fflush(stdout);
                        }
                    }
                }
                // More ranges after the second, each "hexaddress,bytes",
                // separated by semicolons: COD3_DUMPMEM=A,64,66;B,64;C,256; *D,48
                // dumps where the word at D points (a console variable, say).
                if (dumpMem != nullptr && !dumped && long(clockSecond) == dumpSecond)
                {
                    dumped = true;
                    const char* cursor = dumpMem;
                    bool first = true;
                    while (cursor != nullptr && *cursor != 0)
                    {
                        char* end = nullptr;
                        // A leading * dumps where the word at that address points.
                        const bool indirect = *cursor == '*';
                        uint32_t at = uint32_t(strtoul(cursor + (indirect ? 1 : 0), &end, 16));
                        if (indirect) at = Guest::Read32(Guest::Base, at);
                        const uint32_t bytes = (end != nullptr && *end == ',') ? uint32_t(strtoul(end + 1, &end, 0)) : 256u;
                        if (first && end != nullptr && *end == ',') strtoul(end + 1, &end, 10);   // the second
                        first = false;
                        for (uint32_t offset = 0; offset < bytes; offset += 16)
                        {
                            MEMORY_BASIC_INFORMATION info{};
                            if (VirtualQuery(Guest::Base + at + offset, &info, sizeof(info)) == 0 || (info.State & MEM_COMMIT) == 0 || info.Protect == PAGE_NOACCESS) { printf("dump: %08X is not committed\n", at + offset); break; }
                            const uint8_t* row = Guest::Base + at + offset;
                            printf("dump: %08X:", at + offset);
                            for (uint32_t i = 0; i < 16; i += 4) printf(" %02X%02X%02X%02X", row[i], row[i + 1], row[i + 2], row[i + 3]);
                            printf("  ");
                            for (uint32_t i = 0; i < 16; i++) putchar((row[i] >= 32 && row[i] < 127) ? row[i] : '.');
                            printf("\n");
                        }
                        cursor = (end != nullptr && *end == ';') ? end + 1 : nullptr;
                    }
                    fflush(stdout);
                }
            }

            std::this_thread::sleep_until(nextFrame);
        }
    }

    void StartVideoThread()
    {
        VideoState& video = Video();
        bool expected = false;
        if (!video.running.compare_exchange_strong(expected, true))
            return;
        video.thread = std::thread(VideoThread);
        video.commandThread = std::thread(CommandThread);
        Kernel::ProfileHostThread(GetThreadId(video.commandThread.native_handle()), "command processor", "cp");
    }
}

// VOID VdInitializeEngines(ULONG unk, VOID* callback, ULONG unk, ULONG* unk, ULONG* unk)
PPC_FUNC(__imp__VdInitializeEngines)
{
    Kernel::CountImport("VdInitializeEngines");
    printf("video: engines initialised (no GPU behind them, nothing will be drawn)\n");
    StartVideoThread();
}

// VOID VdShutdownEngines(void)
PPC_FUNC(__imp__VdShutdownEngines)
{
    Kernel::CountImport("VdShutdownEngines");
    VideoState& video = Video();
    if (video.running.exchange(false) && video.thread.joinable())
        video.thread.join();
        if (video.commandThread.joinable()) video.commandThread.join();
    printf("video: engines shut down after %llu frames\n",
        (unsigned long long)video.frames.load());
}

// VOID VdInitializeRingBuffer(ULONG address, ULONG sizeLog2)
PPC_FUNC(__imp__VdInitializeRingBuffer)
{
    Kernel::CountImport("VdInitializeRingBuffer");
    // The second argument is the log2 of the size in quadwords, which is how
    // CP_RB_CNTL encodes it, so the ring is 8 << log2 bytes. Reading it as a
    // plain byte count makes the ring eight times too small, and the read
    // pointer then wraps before it can ever reach the value the title is
    // spinning for.
    VideoState& video = Video();

    // A new ring starts empty: the command processor is reset with it, so
    // both pointers are zero. This used to keep the old read pointer and the
    // old write pointer register, and the title initialises the ring twice at
    // start up; if the command thread ran between the second initialisation
    // and the title's first write pointer, it walked the new ring from the
    // old read position to the old write position, through whatever the
    // allocator had left there. Executing 0xCDCDCDCD as packets was the
    // start of every "GPU is hung". The lock keeps the command thread out
    // while the three values change together.
    std::lock_guard<std::mutex> lock(video.ringMutex);

    // Whatever the old ring still holds is carried out first. The console
    // is idle by the time a title re-initialises its ring, because the title
    // waited for it to be; here the command thread may simply not have got
    // to the tail yet, and resetting the pointers over it dropped those
    // packets, fences included, and the title then waited for them forever.
    {
        const uint32_t oldRing = video.ringBuffer.load();
        const uint32_t oldDwords = video.ringBufferSize.load() / 4;
        const uint32_t oldRead = video.readPointer.load();
        const uint32_t oldWrite = Gpu::WritePointer();
        if (oldRing != 0 && oldDwords != 0 && oldRead != oldWrite && oldWrite < oldDwords)
        {
            const uint32_t consumed = Gpu::ProcessRing(oldRing, oldDwords, oldRead, oldWrite);
            printf("video: the previous ring was drained from dword %u to %u before "
                   "being replaced\n", oldRead, consumed);
        }
    }

    Gpu::WriteRegister(Gpu::RegisterWritePointer, 0);
    video.readPointer.store(0);
    video.ringBuffer.store(ctx.r3.u32);
    video.ringBufferSize.store(8u << (ctx.r4.u32 & 0x1F));
    printf("video: ring buffer at 0x%08X, %u bytes (%u dwords), pointers reset\n",
        video.ringBuffer.load(), video.ringBufferSize.load(),
        video.ringBufferSize.load() / 4);
    fflush(stdout);
}

// VOID VdEnableRingBufferRPtrWriteBack(ULONG address, ULONG blockSize)
PPC_FUNC(__imp__VdEnableRingBufferRPtrWriteBack)
{
    Kernel::CountImport("VdEnableRingBufferRPtrWriteBack");
    printf("video: read pointer write back at 0x%08X, block size %u\n",
        ctx.r3.u32, ctx.r4.u32);
    // The address where the GPU is expected to publish how far it has read,
    // in dwords. The command thread writes its read pointer there after
    // every walk of the ring.
    Video().readPointerWriteBack.store(ctx.r3.u32);
    Guest::WritePhysical32(ctx.r3.u32 & ~3u, Video().readPointer.load());
}

// VOID VdSetSystemCommandBufferGpuIdentifierAddress(ULONG address)
PPC_FUNC(__imp__VdSetSystemCommandBufferGpuIdentifierAddress)
{
    Kernel::CountImport("VdSetSystemCommandBufferGpuIdentifierAddress");
    printf("video: gpu identifier address 0x%08X\n", ctx.r3.u32);
    fflush(stdout);
    Video().gpuIdentifier.store(ctx.r3.u32);
    if (ctx.r3.u32 != 0)
        Guest::Write32(base, ctx.r3.u32, 0);
}

// VOID VdSetGraphicsInterruptCallback(VOID* callback, ULONG context)
PPC_FUNC(__imp__VdSetGraphicsInterruptCallback)
{
    Kernel::CountImport("VdSetGraphicsInterruptCallback");
    Video().interruptCallback.store(ctx.r3.u32);
    Video().interruptContext.store(ctx.r4.u32);
    printf("video: interrupt callback at 0x%08X\n", ctx.r3.u32);
}

// VOID VdCallGraphicsNotificationRoutines(ULONG value)
PPC_FUNC(__imp__VdCallGraphicsNotificationRoutines) {}

// VOID VdGetSystemCommandBuffer(ULONG* bufferOut, ULONG* valueOut)
PPC_FUNC(__imp__VdGetSystemCommandBuffer)
{
    Kernel::CountImport("VdGetSystemCommandBuffer");
    // Two addresses in physical memory: the system command buffer, and the
    // block of GPU state the driver keeps beside it. The kernel hands back
    // two pointers into the area it reserves for the GPU near the top of RAM,
    // and the driver works relative to both. This used to answer the second
    // one with zero, and the driver then wrote its fences and state relative
    // to zero: a fence to physical 0x100, which is where the title had put
    // its job queue, and a corrupt job descriptor from then on. The two are
    // laid out the way the kernel lays them out, sixteen bytes apart, in a
    // block nothing else is given.
    static uint32_t block = 0;
    if (block == 0)
    {
        block = Guest::AllocatePhysical(64u << 10);
        printf("video: system command buffer at 0x%08X\n", block);
        fflush(stdout);
    }

    if (ctx.r3.u32 != 0) Guest::Write32(base, ctx.r3.u32, block + 0x900);
    if (ctx.r4.u32 != 0) Guest::Write32(base, ctx.r4.u32, block + 0x910);
}

// VOID VdInitializeScalerCommandBuffer(...)
PPC_FUNC(__imp__VdInitializeScalerCommandBuffer)
{
    Kernel::CountImport("VdInitializeScalerCommandBuffer");
    // Returns the number of words written into the caller's buffer.
    ctx.r3.u32 = 0;
}

// VOID VdSwap(...)
// The title's wait for the GPU to pass a point in its indirect buffer pool,
// sub_822F16A0(device, end, lap): it waits while the GPU's last fence, the
// second word of the write-back block, is on an earlier lap and behind
// `end`. The arguments are kept while the wait lasts, for the stall report:
// a wait that never ends says exactly which fence the GPU never wrote.
namespace
{
    struct PoolWait { uint32_t device, end, lap, guestThread; int64_t since; };
    std::mutex g_poolWaitMutex;
    std::map<uint32_t, PoolWait> g_poolWaits;
}

extern "C" PPC_FUNC(__imp__sub_822F16A0);
// With the decompiled code (COD3_HAVE_DECOMP) the wait is its decompiled
// body, decomp_sub_822F16A0.
#ifdef COD3_HAVE_DECOMP
PPC_FUNC(decomp_sub_822F16A0);
#define COD3_POOL_WAIT decomp_sub_822F16A0
#else
#define COD3_POOL_WAIT __imp__sub_822F16A0
#endif
PPC_FUNC(sub_822F16A0)
{
    const uint32_t osId = GetCurrentThreadId();
    {
        std::lock_guard<std::mutex> lock(g_poolWaitMutex);
        // The waiter's own thread id as the title keeps it (r13+0x100, then
        // +0x14C): the wait's timeout is held off while the device names
        // this thread as the one with submissions pending.
        const uint32_t threadObject = ctx.r13.u32 ? Guest::Read32(base, ctx.r13.u32 + 0x100) : 0;
        const uint32_t guestThread = threadObject ? Guest::Read32(base, threadObject + 0x14C) : 0;
        g_poolWaits[osId] = { ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, guestThread, NowMilliseconds() };
    }
    const int64_t started = NowMilliseconds();
    Timeline::Mark("gpu wait", ctx.r4.u32, ctx.r5.u32);
    COD3_POOL_WAIT(ctx, base);
    Timeline::Mark("gpu waited", ctx.r4.u32, ctx.r5.u32);
    PoolTrace::Waited(uint64_t(NowMilliseconds() - started) * 1000000ull);
    std::lock_guard<std::mutex> lock(g_poolWaitMutex);
    g_poolWaits.erase(osId);
}

void Kernel::ReportPoolWaits()
{
    std::lock_guard<std::mutex> lock(g_poolWaitMutex);
    if (g_poolWaits.empty()) return;
    printf("\n");
    printf("waits for the GPU to pass a point in the indirect buffer pool:\n");
    for (const auto& entry : g_poolWaits)
    {
        const PoolWait& wait = entry.second;
        const uint32_t block = Guest::Read32(Guest::Base, wait.device + 10768);
        const uint32_t word = block ? Guest::Read32(Guest::Base, block + 4) : 0;
        printf("  thread %-5u wants the GPU past 0x%08X on lap %u (%u); the GPU's word is "
               "0x%08X (lap %u), %llu ms so far\n",
            entry.first, wait.end, wait.lap, wait.lap & 3, word, word & 3,
            (unsigned long long)(NowMilliseconds() - wait.since));
        // What holds the title's own five second timeout off: the device's
        // owner of pending submissions and their count, against the waiter.
        printf("    the waiter is guest thread %08X; the device's owner %08X, pending %u/%u, flags %02X%02X, counter %u, the GPU's counter word %u\n",
            wait.guestThread, Guest::Read32(Guest::Base, wait.device + 10760),
            Guest::Read32(Guest::Base, wait.device + 10864), Guest::Read32(Guest::Base, wait.device + 10868),
            Guest::Read8(Guest::Base, wait.device + 10808), Guest::Read8(Guest::Base, wait.device + 10809),
            Guest::Read32(Guest::Base, wait.device + 10780), block ? Guest::Read32(Guest::Base, block) : 0);
    }
}

PPC_FUNC(__imp__VdSwap)
{
    Kernel::CountImport("VdSwap");
    // VdSwap is where the title hands over a finished frame. Its arguments
    // name the buffer, so the first few are reported in full: that address is
    // what the window would present if anything had rendered into it.
    const uint64_t count = Video().swaps.fetch_add(1) + 1;
    Kernel::Stats().swaps.store(count, std::memory_order_relaxed);
    // The fourth argument points at the texture fetch constant that describes
    // the front buffer: its address, its size and its format. That is the one
    // piece of information the window needs to show a real frame, so it is
    // decoded here and handed over.
    const uint32_t fetchConstant = ctx.r4.u32;
    // 0.5.478: the key caps the button prompts are drawn as, for what the
    // player used last (pc_prompts.cpp).
    PcPrompts::Frame(base);
#ifdef COD3_MP
    // 0.5.479: the game heap's fill every 30 s (mp_fixes.cpp)
    { extern void MpFixesHeapReport(uint8_t*); MpFixesHeapReport(base); }
#endif

    // Every argument, the first few times. Only the fetch constant was ever
    // read, and a swap the console performs is more than a picture: the first
    // argument is where in the ring buffer the driver expects a swap packet to
    // be written, and one of the others may well be where it expects the swap
    // to be acknowledged, which is the address a wait has been abandoned on
    // after every first frame of every run.
    if (count <= 3)
    {
        printf("video: VdSwap arguments r3..r10:");
        for (int i = 3; i <= 10; i++)
            printf(" %08X", Kernel::Register(ctx, i));
        printf("\n");
        const uint32_t buffer = ctx.r3.u32;
        if (buffer != 0)
        {
            printf("video:   at r3, the buffer holds:");
            for (int i = 0; i < 8; i++)
                printf(" %08X", Guest::Read32(base, buffer + i * 4));
            printf("\n");
        }
        printf("video:   ring write pointer register reads %u\n",
            Gpu::WritePointer());

        // What the stack arguments point at. On the console VdSwap is the
        // kernel's, and a kernel call that takes four pointers into the
        // caller's stack is writing results into them or reading a request
        // out of them; either way the words there say what the driver expects
        // the swap to do beyond changing the picture.
        for (int reg = 5; reg <= 9; reg++)
        {
            const uint32_t at = Kernel::Register(ctx, reg);
            if (at < 0x10000 || at >= 0xC0000000) continue;
            printf("video:   r%d 0x%08X ->", reg, at);
            for (int i = 0; i < 8; i++) printf(" %08X", Guest::Read32(base, at + i * 4));
            printf("\n");
        }
        fflush(stdout);
    }

    if (fetchConstant != 0)
    {
        const uint32_t word0 = Guest::Read32(base, fetchConstant + 0);
        const uint32_t word1 = Guest::Read32(base, fetchConstant + 4);
        const uint32_t word2 = Guest::Read32(base, fetchConstant + 8);

        // The address is in the upper bits of the second word, in 4 KB units.
        const uint32_t surface = (word1 & 0xFFFFF000);
        const uint32_t width  = ((word2 & 0x00001FFF) + 1);
        const uint32_t height = (((word2 >> 13) & 0x00001FFF) + 1);

        if (count <= 3)
        {
            printf("video: VdSwap %llu fetch constant %08X %08X %08X\n",
                (unsigned long long)count, word0, word1, word2);
            printf("video:   surface 0x%08X, %ux%u\n", surface, width, height);

            bool anything = false;
            for (uint32_t i = 0; i < 4096 && !anything; i += 4)
                if (Guest::Read32(Guest::Base, Guest::PhysicalAlias(surface) + i) != 0)
                    anything = true;
            printf("video:   the surface is %s\n",
                anything ? "not empty" : "entirely zero, nothing was rendered into it");
            fflush(stdout);
        }

        // The swap goes into the stream, not on the screen here.
        //
        // The driver reserves sixty four words of its command buffer for
        // this call, hands it the first of them in r3 and moves its cursor
        // to the last of them when it returns (its cursor is the last word
        // written, and every writer stores ahead of it): on the console the
        // kernel fills them with the packets that change the picture. Nothing was written
        // into them here, and the command processor read whatever the
        // memory held: zeros at first, which parse as writes of register
        // zero and leave the stream misaligned by a word at the end of the
        // run, and later the packets of whatever the memory was used for
        // before. Draws of one point, interrupts nobody had asked for,
        // waits on the display scaler's registers with a mask the reference
        // could never satisfy: those came from here, and a wait like that
        // was every stall of five seconds.
        //
        // So: one packet the command processor knows, and the rest filler,
        // and the picture changes when the GPU reaches it, after the frame's
        // draws, rather than when the CPU finished recording them.
        if (surface != 0 && width > 1 && height > 1 && width <= 4096 && height <= 4096)
        {
            const uint32_t buffer = ctx.r3.u32;
            if (buffer != 0)
            {
                constexpr uint32_t Words = 64;
                uint32_t offset = 0;
                auto put = [&](uint32_t value) { Guest::Write32(base, buffer + offset * 4, value); offset++; };
                put(0xC0000000u | (3u << 16) | (0x64u << 8));   // SWAP, four words
                put(0x50415753u);                   // 'SWAP'
                put(surface);
                put(width);
                put(height);
                while (offset < Words) put(0x80000000u);
            }
            else
            {
                Window::SetFrontBuffer(Guest::PhysicalAlias(surface), width, height);
                Render::Swap(surface, width, height);
            }
        }
    }

    // One frame of latency, as the console's driver allows. The title's
    // vertical blank runs at sixty hertz here whether or not the GPU has
    // kept up, so nothing else stops the title from running two frames
    // ahead of the GPU; and two frames ahead is the title writing the
    // dynamic data of frame N+2 over the buffer frame N is still being
    // drawn from - the sky's pieces went missing that way in every other
    // frame of a cutscene. This waits until the GPU has reached the swap
    // before this one. COD3_SWAPLATENCY=N allows N frames instead.
    {
        static const uint64_t allowed = []() {
            const char* text = getenv("COD3_SWAPLATENCY");
            const long value = text != nullptr ? strtol(text, nullptr, 10) : 1;
            return uint64_t(value < 0 ? 0 : value);
        }();
        if (count > allowed)
        {
            const uint64_t needed = count - allowed;
            if (Kernel::Stats().swapsReached.load(std::memory_order_acquire) < needed)
            {
                Scheduler::Release();
                struct Resume { ~Resume() { Scheduler::Acquire(); } } resume;
                // Woken by the swap itself (gpu.cpp), up to half a second.
                Gpu::WaitForSwapsReached(needed, std::chrono::milliseconds(500));
            }
        }
    }
    ctx.r3.u32 = 0;
}

// --- Display queries -------------------------------------------------------

// VOID VdQueryVideoMode(X_VIDEO_MODE* mode)
PPC_FUNC(__imp__VdQueryVideoMode)
{
    Kernel::CountImport("VdQueryVideoMode");
    const uint32_t mode = ctx.r3.u32;
    if (mode == 0) return;

    // The same mode XGetVideoMode reports, so the title never sees the two
    // disagree.
    memset(Guest::Ptr(mode), 0, 48);
    Guest::Write32(base, mode + 0x00, 1280);
    Guest::Write32(base, mode + 0x04, 720);
    Guest::Write32(base, mode + 0x08, 0);
    Guest::Write32(base, mode + 0x0C, 1);
    Guest::Write32(base, mode + 0x10, 1);
    Guest::Write32(base, mode + 0x14, 0x42700000);   // 60.0f
    Guest::Write32(base, mode + 0x18, 1);
}

// ULONG VdQueryVideoFlags(void)
PPC_FUNC(__imp__VdQueryVideoFlags)
{
    Kernel::CountImport("VdQueryVideoFlags");
    // Widescreen and high definition.
    ctx.r3.u32 = 0x00000003;
}

// VOID VdGetCurrentDisplayInformation(void* information)
PPC_FUNC(__imp__VdGetCurrentDisplayInformation)
{
    Kernel::CountImport("VdGetCurrentDisplayInformation");
    const uint32_t information = ctx.r3.u32;
    if (information == 0) return;

    memset(Guest::Ptr(information), 0, 0x58);
    Guest::Write32(base, information + 0x00, 1280);
    Guest::Write32(base, information + 0x04, 720);
}

// VOID VdGetCurrentDisplayGamma(ULONG* unk, float* gamma)
PPC_FUNC(__imp__VdGetCurrentDisplayGamma)
{
    Kernel::CountImport("VdGetCurrentDisplayGamma");
    if (ctx.r3.u32 != 0) Guest::Write32(base, ctx.r3.u32, 2);
    if (ctx.r4.u32 != 0) Guest::Write32(base, ctx.r4.u32, 0x40133333);   // 2.3f
}

// VOID VdSetDisplayMode(ULONG mode)
PPC_FUNC(__imp__VdSetDisplayMode) { ctx.r3.u32 = 0; }

// BOOL VdPersistDisplay(ULONG unk, ULONG* out)
PPC_FUNC(__imp__VdPersistDisplay)
{
    Kernel::CountImport("VdPersistDisplay");
    if (ctx.r4.u32 != 0) Guest::Write32(base, ctx.r4.u32, 0);
    ctx.r3.u32 = 1;
}

// BOOL VdIsHSIOTrainingSucceeded(void)
PPC_FUNC(__imp__VdIsHSIOTrainingSucceeded) { ctx.r3.u32 = 1; }

// VOID VdEnableDisableClockGating(ULONG enable)
PPC_FUNC(__imp__VdEnableDisableClockGating) {}

// ULONG VdRetrainEDRAM(...)
PPC_FUNC(__imp__VdRetrainEDRAM) { ctx.r3.u32 = 0; }

// ULONG VdRetrainEDRAMWorker(...)
PPC_FUNC(__imp__VdRetrainEDRAMWorker) { ctx.r3.u32 = 0; }

uint32_t Gpu::InterruptContext()
{
    return Video().interruptContext.load();
}

bool Kernel::FrameRateUnlocked() { return g_frameRateUnlocked.load(std::memory_order_relaxed); }

bool Kernel::FilmPlaying()
{
#ifdef COD3_MP
    return false;   // multiplayer has no films
#else
    return Guest::Read32(Guest::Base, 0x82A2A24Cu) != 0;   // the film player (TitlePlaying above)
#endif
}

void Kernel::QueueConsoleCommand(const std::string& text)
{
    if (text.empty() || text.size() > 4000) return;
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    if (g_consoleQueue.size() < 64) g_consoleQueue.push_back(text);
}
