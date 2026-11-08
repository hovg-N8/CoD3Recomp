// Xbox 360 kernel imports: audio.
//
// Two layers. XMA is the console's hardware codec: the title hands it
// compressed blocks through a context structure and reads decoded samples back
// out. XAudio is the mixer the decoded voices are submitted to.
//
// XMA is decoded through FFmpeg's xma2 decoder when its libraries are on
// the machine (xma.cpp); without them the contexts go through the same
// motions and the sound that goes through them is silence.
//
// XAudio: the frames the title mixes are handed to the machine's sound
// device.

#include "kernel.h"
#include "audio_out.h"
#include "xma.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <map>
#include <mutex>
#include <vector>

#include <Windows.h>

namespace
{
    constexpr uint32_t X_ERROR_SUCCESS = 0x00000000;

    // Each context is a block of guest memory the title also reads directly.
    constexpr uint32_t ContextSize = 64;

    std::mutex g_audioMutex;

    // COD3_XMATRACE prints every call on the codec with its context and
    // arguments, which is the only way to see how a title paces a stream.
    bool XmaTrace()
    {
        static const bool on = getenv("COD3_XMATRACE") != nullptr;
        return on;
    }
    void Trace(const char* name, const PPCContext& ctx)
    {
        if (!XmaTrace()) return;
        static std::atomic<int> count{ 0 };
        if ((count.load(std::memory_order_relaxed) >= 2000 || count.fetch_add(1) >= 2000)) return;
        printf("xma: %s(0x%08X, 0x%08X, 0x%08X) from 0x%08X\n",
            name, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, uint32_t(ctx.lr));
        fflush(stdout);
    }

    uint32_t g_registeredClients = 0;
    uint64_t g_submittedFrames = 0;

    // For the sound cutting out: how many XMA contexts the title has going
    // in one audio frame at most, counted between the pump's ticks (a title
    // out of voices stops sounds to make room for new ones).
    std::atomic<uint32_t> g_enablesThisFrame{ 0 }, g_mostEnablesInAFrame{ 0 };

    // The audio pump.
    //
    // Registering a render driver client hands the console a callback, and the
    // audio hardware calls it every time it has finished a frame of samples so
    // the title can produce the next one. Registering it and never calling it
    // leaves the title's audio thread waiting for a frame boundary that never
    // arrives, and a title that waits for its own sound to start never gets
    // past the screen it is on.
    //
    // A frame is 256 samples at 48 kHz, so the callback is due every 5333
    // microseconds. Nothing is played: what matters here is that the clock
    // runs.
    std::atomic<uint32_t> g_callback{ 0 };
    std::atomic<uint32_t> g_callbackContext{ 0 };
    std::atomic<bool> g_pumpRunning{ false };
    std::atomic<uint64_t> g_callbacks{ 0 };
    std::thread g_pumpThread;

    void AudioPump()
    {
        // Guest code runs on this thread, so it needs a context and a stack of
        // its own, exactly like a thread the title created itself.
        const uint32_t stackTop = Guest::AllocateStack(128u << 10);
        if (stackTop == 0)
        {
            fprintf(stderr, "audio: no room for the callback stack\n");
            return;
        }

        alignas(0x40) PPCContext ctx{};
        ctx.r13.u32 = Guest::CreateThreadPointer(GetCurrentThreadId());
        ctx.fpscr.loadFromHost();

        auto next = std::chrono::steady_clock::now();
        while (g_pumpRunning.load(std::memory_order_acquire))
        {
            next += std::chrono::microseconds(5333);

            const uint32_t callback = g_callback.load();
            if (callback != 0)
            {
                if (PPCFunc* routine = Guest::Lookup(callback))
                {
                    ctx.r1.u32 = stackTop - 0x100;
                    ctx.r3.u32 = g_callbackContext.load();
                    routine(ctx, Guest::Base);
                    g_callbacks.fetch_add(1, std::memory_order_relaxed);
                    const uint32_t enabled = g_enablesThisFrame.exchange(0, std::memory_order_relaxed);
                    if (enabled > g_mostEnablesInAFrame.load(std::memory_order_relaxed)) g_mostEnablesInAFrame.store(enabled, std::memory_order_relaxed);
                }
                else
                {
                    printf("audio: the render callback at 0x%08X was not "
                           "recompiled, the pump cannot run\n", callback);
                    fflush(stdout);
                    g_callback.store(0);
                }
            }

            std::this_thread::sleep_until(next);
        }
    }

    void StartPump()
    {
        bool expected = false;
        if (!g_pumpRunning.compare_exchange_strong(expected, true)) return;
        g_pumpThread = std::thread(AudioPump);
    }
}

void Kernel::StopAudioPump()
{
    if (g_pumpRunning.exchange(false) && g_pumpThread.joinable())
        g_pumpThread.join();
}

uint64_t Kernel::AudioCallbacks() { return g_callbacks.load(); }

// --- XMA contexts ----------------------------------------------------------
//
// The context is sixty four bytes of physical memory the console's codec
// reads, laid out as Xenia has it (xma_context.h): bit fields in the first
// five words, the buffer addresses in the next four, the output read
// offset and the output valid bit in the tenth. The title here never
// touches the block itself, it goes through the kernel calls below, but
// the block is kept true to the hardware's all the same, and it is what the
// decoding reads.
//
// Decoding follows the hardware's shape: two input buffers of whole 2048
// byte packets, taken in turn and marked consumed; an output ring of 256
// byte blocks the decoded samples go into while the title says the ring is
// open (output valid) and there is room; and when enabled the context
// decodes its subframe count, 128 samples a channel each, and then stops,
// which is what the title counts on when it reads the ring back.

namespace
{
    // Bit fields of the context block. The words are big endian in memory.
    uint32_t Field(uint8_t* base, uint32_t context, uint32_t word, uint32_t shift, uint32_t bits)
    {
        return (Guest::Read32(base, context + word * 4) >> shift) & ((1u << bits) - 1);
    }
    void SetField(uint8_t* base, uint32_t context, uint32_t word, uint32_t shift, uint32_t bits, uint32_t value)
    {
        const uint32_t mask = ((1u << bits) - 1) << shift;
        const uint32_t old = Guest::Read32(base, context + word * 4);
        Guest::Write32(base, context + word * 4, (old & ~mask) | ((value << shift) & mask));
    }
    // Word 0.
    uint32_t Input0Packets(uint8_t* b, uint32_t c) { return Field(b, c, 0, 0, 12); }
    uint32_t Input0Valid(uint8_t* b, uint32_t c) { return Field(b, c, 0, 20, 1); }
    uint32_t Input1Valid(uint8_t* b, uint32_t c) { return Field(b, c, 0, 21, 1); }
    uint32_t OutputBlocks(uint8_t* b, uint32_t c) { return Field(b, c, 0, 22, 5); }
    uint32_t WriteOffset(uint8_t* b, uint32_t c) { return Field(b, c, 0, 27, 5); }
    // Word 1.
    uint32_t Input1Packets(uint8_t* b, uint32_t c) { return Field(b, c, 1, 0, 12); }
    uint32_t SubframeCount(uint8_t* b, uint32_t c) { return Field(b, c, 1, 20, 4); }
    uint32_t IsStereo(uint8_t* b, uint32_t c) { return Field(b, c, 1, 30, 1); }
    // Word 2: the read offset, in bits, within the current input buffer.
    // Word 4 bit 31: which input buffer is current.
    uint32_t CurrentBuffer(uint8_t* b, uint32_t c) { return Field(b, c, 4, 31, 1); }
    // Words 5 to 7: the buffers.
    uint32_t InputPtr(uint8_t* b, uint32_t c, uint32_t which) { return Guest::Read32(b, c + (5 + which) * 4); }
    uint32_t OutputPtr(uint8_t* b, uint32_t c) { return Guest::Read32(b, c + 7 * 4); }
    // Word 9: the read offset and the valid bit of the output ring.
    uint32_t ReadOffset(uint8_t* b, uint32_t c) { return Field(b, c, 9, 0, 5); }
    uint32_t OutputValid(uint8_t* b, uint32_t c) { return Field(b, c, 9, 30, 1); }

    // What the block does not hold: the decoder behind the context and the
    // samples it has produced that the ring had no room for yet.
    struct Context
    {
        Xma::Stream* stream = nullptr;
        int channels = 1;
        int sampleRate = 48000;
        bool enabled = false;
        bool announcedNoDecoder = false;
        std::vector<int16_t> fifo;
        size_t fifoRead = 0;
        uint32_t packetInBuffer = 0;     // the next packet of the current input buffer
        uint32_t blocksThisEnable = 0;   // written since the last enable
        uint32_t output = 0;             // the output ring the last initialize gave
        bool starved = false;            // ran out of input part way through an enable
        bool played = false;             // has written a block since it was initialized: its start is not a gap
    };
    std::map<uint32_t, Context> g_xma;   // by context address

    // COD3_XMAMODE=full: decode until the ring is full or the input runs out
    // rather than stopping at the subframe count, for comparison.
    bool StopAtSubframeCount()
    {
        static const bool full = []() { const char* t = getenv("COD3_XMAMODE"); return t != nullptr && strcmp(t, "full") == 0; }();
        return !full;
    }

    std::atomic<uint64_t> g_packetsDecoded{ 0 }, g_blocksWritten{ 0 }, g_packetsRefused{ 0 }, g_enables{ 0 };
    // For the sound cutting out: how many sounds the title starts, and how
    // often a context ran dry with the title then giving it more: a gap in
    // a sound still playing.
    std::atomic<uint64_t> g_initializes{ 0 }, g_underruns{ 0 };

    // The decoder, moved along as far as the buffers let it.
    void Work(uint8_t* base, uint32_t context)
    {
        // COD3_NOXMA=1: nothing is decoded and nothing is written into the
        // title's ring buffers, for telling a fault the decoder's writes
        // cause from one it does not.
        static const bool none = getenv("COD3_NOXMA") != nullptr;
        if (none) return;
        auto found = g_xma.find(context);
        if (found == g_xma.end()) return;
        Context& state = found->second;
        if (!state.enabled) return;
        const uint32_t blocks = OutputBlocks(base, context);
        if (blocks == 0) return;
        const uint32_t subframes = SubframeCount(base, context);
        const uint32_t channels = uint32_t(state.channels);

        for (int guard = 0; guard < 4096; guard++)
        {
            // A block of the ring: 128 samples, mono or interleaved stereo.
            if (state.fifo.size() - state.fifoRead >= 128)
            {
                if (OutputValid(base, context) == 0) return;   // the ring is the title's until it says otherwise
                uint32_t write = WriteOffset(base, context);
                const uint32_t read = ReadOffset(base, context);
                if (OutputPtr(base, context) != state.output)
                {
                    static std::atomic<int> told{ 0 };
                    if ((told.load(std::memory_order_relaxed) < 20 && told.fetch_add(1) < 20))
                    {
                        printf("xma: context 0x%08X would write to 0x%08X, but was initialised with 0x%08X\n",
                            context, OutputPtr(base, context), state.output);
                        fflush(stdout);
                    }
                }
                uint8_t* block = Guest::Ptr(Guest::PhysicalAlias(OutputPtr(base, context))) + write * 256;
                // COD3_XMANOWRITE=1: the ring is moved along as ever but the
                // samples are not put into the title's memory, for telling a
                // fault the decoded samples cause from one they do not.
                static const bool noWrite = getenv("COD3_XMANOWRITE") != nullptr;
                for (uint32_t i = 0; i < 128 && !noWrite; i++)
                {
                    const uint16_t sample = uint16_t(state.fifo[state.fifoRead + i]);
                    block[i * 2] = uint8_t(sample >> 8);
                    block[i * 2 + 1] = uint8_t(sample);
                }
                state.fifoRead += 128;
                if (state.fifoRead >= 65536) { state.fifo.erase(state.fifo.begin(), state.fifo.begin() + state.fifoRead); state.fifoRead = 0; }
                write = (write + 1) % blocks;
                SetField(base, context, 0, 27, 5, write);
                if (write == read) SetField(base, context, 9, 30, 1, 0);   // full: the title reopens it
                state.blocksThisEnable++;
                state.played = true;
                g_blocksWritten.fetch_add(1, std::memory_order_relaxed);
                // COD3_XMASUBFRAME=channel: a subframe is 128 samples a channel
                // (two blocks for stereo); by default it is one block of the
                // ring, which is what this title's ring and its mixer frame of
                // 256 samples add up to.
                static const bool perChannel = []() { const char* t = getenv("COD3_XMASUBFRAME"); return t != nullptr && strcmp(t, "channel") == 0; }();
                const uint32_t perSubframe = perChannel ? channels : 1;
                if (StopAtSubframeCount() && subframes != 0 && state.blocksThisEnable / perSubframe >= subframes)
                {
                    if (XmaTrace())
                    {
                        static std::atomic<int> count{ 0 };
                        if ((count.load(std::memory_order_relaxed) < 40 && count.fetch_add(1) < 40))
                        {
                            const uint8_t* ring = Guest::Ptr(Guest::PhysicalAlias(OutputPtr(base, context)));
                            printf("xma:   decoded %u blocks this enable; ring words %02X%02X%02X%02X %02X%02X%02X%02X ... write %u read %u valid %u%s",
                                state.blocksThisEnable, ring[0], ring[1], ring[2], ring[3], ring[4], ring[5], ring[6], ring[7],
                                WriteOffset(base, context), ReadOffset(base, context), OutputValid(base, context), "\n");
                        }
                    }
                    // Its subframes done, the context stops until enabled again.
                    state.enabled = false;
                    return;
                }
                continue;
            }

            // More samples: the next packet of the current input buffer.
            const uint32_t which = CurrentBuffer(base, context);
            const uint32_t valid = which == 0 ? Input0Valid(base, context) : Input1Valid(base, context);
            if (valid == 0)
            {
                // Starved: the title has not filled it yet.
                if (state.played && state.blocksThisEnable < (subframes != 0 ? subframes : 1)) state.starved = true;
                return;
            }
            const uint32_t packets = which == 0 ? Input0Packets(base, context) : Input1Packets(base, context);
            if (state.packetInBuffer >= packets)
            {
                // This buffer is consumed: back to the title, and on to the other.
                SetField(base, context, 0, 20 + which, 1, 0);
                SetField(base, context, 4, 31, 1, which ^ 1);
                state.packetInBuffer = 0;
                Guest::Write32(base, context + 2 * 4, 32);   // past the next packet's header
                continue;
            }
            const uint8_t* packet = Guest::Ptr(Guest::PhysicalAlias(InputPtr(base, context, which))) + state.packetInBuffer * 2048;
            bool decoded = false;
            if (state.stream != nullptr)
                decoded = Xma::Decode(state.stream, packet, state.fifo);
            if (!decoded)
            {
                // No decoder, or a packet it would not take: silence of a
                // packet's likely length, so the title's clock keeps time.
                if (state.stream != nullptr) g_packetsRefused.fetch_add(1, std::memory_order_relaxed);
                state.fifo.resize(state.fifo.size() + 2048 * channels, 0);
            }
            else g_packetsDecoded.fetch_add(1, std::memory_order_relaxed);
            state.packetInBuffer++;
            Guest::Write32(base, context + 2 * 4, (state.packetInBuffer * 2048 * 8) & 0x03FFFFFF);
        }
    }

    void Announce()
    {
        static bool announced = false;
        if (announced) return;
        announced = true;
        if (Xma::Available()) printf("xma: decoding through FFmpeg%s", "\n");
        else printf("xma: no decoder (%s); the title's XMA sound is silent%s", Xma::Unavailable(), "\n");
        fflush(stdout);
    }
}

void Kernel::ReportXma()
{
    std::lock_guard<std::mutex> lock(g_audioMutex);
    printf("xma: %zu contexts, %llu enables, %llu packets decoded, %llu refused, %llu blocks written%s", g_xma.size(),
        (unsigned long long)g_enables.load(), (unsigned long long)g_packetsDecoded.load(), (unsigned long long)g_packetsRefused.load(),
        (unsigned long long)g_blocksWritten.load(), "\n");
    printf("xma: %llu sounds started, at most %u contexts going in one audio frame (since the last report), %llu run dry and then fed%s",
        (unsigned long long)g_initializes.load(), g_mostEnablesInAFrame.exchange(0), (unsigned long long)g_underruns.load(), "\n");
}

// X_RESULT XMACreateContext(ULONG* contextOut, ...)
PPC_FUNC(__imp__XMACreateContext)
{ Trace("XMACreateContext", ctx);
    Kernel::CountImport("XMACreateContext");
    std::lock_guard<std::mutex> lock(g_audioMutex);
    Announce();

    // The context lives in physical memory, as the console's do: the codec
    // reads it there, and a title that pokes its fields through one of the
    // physical windows (0xA0000000, 0xC0000000, 0xE0000000) lands on the
    // same bytes this runtime reads.
    static uint32_t pool = 0, poolUsed = 0;
    if (pool == 0 || poolUsed + ContextSize > (64u << 10)) { pool = Guest::AllocatePhysical(64u << 10); poolUsed = 0; }
    if (pool == 0) { ctx.r3.u32 = 0x8007000E; return; }   // E_OUTOFMEMORY
    const uint32_t address = Guest::PhysicalAlias(pool) + poolUsed;
    poolUsed += ContextSize;
    memset(Guest::Ptr(address), 0, ContextSize);
    g_xma[address] = Context{};

    if (ctx.r3.u32 != 0)
        Guest::Write32(base, ctx.r3.u32, address);
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XMAReleaseContext(ULONG context)
PPC_FUNC(__imp__XMAReleaseContext)
{ Trace("XMAReleaseContext", ctx);
    Kernel::CountImport("XMAReleaseContext");
    std::lock_guard<std::mutex> lock(g_audioMutex);
    auto found = g_xma.find(ctx.r3.u32);
    const bool known = found != g_xma.end();
    if (known)
    {
        Xma::Destroy(found->second.stream);
        g_xma.erase(found);
    }
    {
        static std::atomic<int> told{ 0 };
        if ((told.load(std::memory_order_relaxed) < 40 && told.fetch_add(1) < 40)) { printf("xma: context 0x%08X released%s\n", ctx.r3.u32, known ? "" : " (not known)"); fflush(stdout); }
    }
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XMAInitializeContext(ULONG context, XMA_CONTEXT_INIT* parameters)
//
// The parameters: input buffer 0 and its packet count, input buffer 1 and
// its, the read offset in bits, the output buffer and its block count, the
// work buffer, the subframe count, the channel count, the sample rate as an
// index (24, 32, 44.1, 48 kHz), and the loop data.
PPC_FUNC(__imp__XMAInitializeContext)
{ Trace("XMAInitializeContext", ctx);
    Kernel::CountImport("XMAInitializeContext");

    const uint32_t context = ctx.r3.u32;
    const uint32_t parameters = ctx.r4.u32;
    if (context == 0) { ctx.r3.u32 = X_ERROR_SUCCESS; return; }

    std::lock_guard<std::mutex> lock(g_audioMutex);
    g_initializes.fetch_add(1, std::memory_order_relaxed);
    memset(Guest::Ptr(context), 0, ContextSize);
    Context& state = g_xma[context];
    state.enabled = false;
    state.starved = false;
    state.played = false;
    state.fifo.clear();
    state.fifoRead = 0;
    state.packetInBuffer = 0;
    state.blocksThisEnable = 0;
    if (parameters == 0) { ctx.r3.u32 = X_ERROR_SUCCESS; return; }

    auto word = [&](uint32_t index) { return Guest::Read32(base, parameters + index * 4); };
    const uint32_t input0 = word(0), input0Packets = word(1), input1 = word(2), input1Packets = word(3);
    const uint32_t readOffset = word(4), output = word(5), outputBlocks = word(6), work = word(7);
    const uint32_t subframes = word(8), channels = word(9), rateIndex = word(10);
    const uint32_t loopStart = word(11), loopEnd = word(12), loopCount = word(13);
    static const int rates[4] = { 24000, 32000, 44100, 48000 };

    {
        static std::atomic<int> announced{ 0 };
        if ((announced.load(std::memory_order_relaxed) < 3 && announced.fetch_add(1) < 3) || XmaTrace())
        {
            printf("xma: context 0x%08X: inputs 0x%08X x%u and 0x%08X x%u, read offset %u bits, output 0x%08X x%u blocks, "
                   "%u subframes, stereo %u, %d Hz, loop %u..%u x%u%s",
                context, input0, input0Packets, input1, input1Packets, readOffset, output, outputBlocks, subframes,
                channels, rates[rateIndex & 3], loopStart, loopEnd, loopCount, "\n");
            fflush(stdout);
        }
    }

    // Word 0: packet count, loop count, valid bits clear, block count, write offset 0.
    Guest::Write32(base, context + 0, (input0Packets & 0xFFF) | ((loopCount & 0xFF) << 12) | ((outputBlocks & 0x1F) << 22));
    // Word 1: packet count, subframe count, stereo.
    Guest::Write32(base, context + 4, (input1Packets & 0xFFF) | ((subframes & 0xF) << 20) | ((channels != 0 ? 1u : 0u) << 30));
    Guest::Write32(base, context + 8, readOffset & 0x03FFFFFF);
    Guest::Write32(base, context + 12, loopStart & 0x03FFFFFF);
    Guest::Write32(base, context + 16, loopEnd & 0x03FFFFFF);
    Guest::Write32(base, context + 20, input0);
    Guest::Write32(base, context + 24, input1);
    Guest::Write32(base, context + 28, output);
    state.output = output;
    Guest::Write32(base, context + 32, work);
    Guest::Write32(base, context + 36, 0);

    // A decoder of this shape, made once per shape. The channel word is a
    // stereo flag rather than a count: the menu music comes with it set to
    // one and decodes only as two channels.
    const int wantChannels = channels != 0 ? 2 : 1;
    const int wantRate = rates[rateIndex & 3];
    if (state.stream != nullptr && (state.channels != wantChannels || state.sampleRate != wantRate))
    {
        Xma::Destroy(state.stream);
        state.stream = nullptr;
    }
    if (state.stream == nullptr && Xma::Available())
        state.stream = Xma::Create(wantChannels, wantRate);
    else if (state.stream != nullptr)
        Xma::Reset(state.stream);
    state.channels = wantChannels;
    state.sampleRate = wantRate;
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XMAEnableContext(ULONG context)
PPC_FUNC(__imp__XMAEnableContext)
{ Trace("XMAEnableContext", ctx);
    Kernel::CountImport("XMAEnableContext");
    std::lock_guard<std::mutex> lock(g_audioMutex);
    auto found = g_xma.find(ctx.r3.u32);
    if (found != g_xma.end())
    {
        found->second.enabled = true;
        found->second.blocksThisEnable = 0;
        // 0.5.468: at most 128 decoded a frame (Justin, 0.5.467 on Rouen: "the
        // game got very slow and sounded like an alien running at 8fps"; its
        // log: 100 to 260 contexts going in each audio frame for the last 45 s,
        // peaks of 440 to 516, where 40 to 60 is usual): past that a context is
        // enabled but not decoded this frame (it is decoded again on its next)
        const uint32_t going = g_enablesThisFrame.fetch_add(1, std::memory_order_relaxed) + 1;
        if (going <= 128) Work(base, ctx.r3.u32);
        else
        {
            static std::atomic<uint64_t> skipped{ 0 };
            const uint64_t n = skipped.fetch_add(1, std::memory_order_relaxed) + 1;
            if (n == 1 || n % 5000 == 0) { printf("xma: more than 128 sounds going in one audio frame: %llu decodes put off so far\n", (unsigned long long)n); fflush(stdout); }
        }
        g_enables.fetch_add(1, std::memory_order_relaxed);
        if (XmaTrace())
        {
            static auto first = std::chrono::steady_clock::now();
            static std::atomic<int> count{ 0 };
            if ((count.load(std::memory_order_relaxed) < 200 && count.fetch_add(1) < 200))
                printf("xma:   enable at %.1f ms on thread %lu%s", std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - first).count(), GetCurrentThreadId(), "\n");
        }
    }
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XMADisableContext(ULONG context, BOOL wait)
PPC_FUNC(__imp__XMADisableContext)
{ Trace("XMADisableContext", ctx);
    Kernel::CountImport("XMADisableContext");
    std::lock_guard<std::mutex> lock(g_audioMutex);
    auto found = g_xma.find(ctx.r3.u32);
    if (found != g_xma.end()) found->second.enabled = false;
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// --- Buffer state ----------------------------------------------------------
// Each answer is read off the context block after the decoder has had its
// turn, and each flag the title sets is followed by one.

namespace
{
    uint32_t Advanced(uint8_t* base, uint32_t context, uint32_t word, uint32_t shift, uint32_t bits)
    {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        Work(base, context);
        const uint32_t value = Field(base, context, word, shift, bits);
        if (XmaTrace())
        {
            static std::atomic<int> count{ 0 };
            if ((count.load(std::memory_order_relaxed) < 400 && count.fetch_add(1) < 400))
                printf("xma:   -> word %u bits %u+%u = %u (write %u read %u valid %u in0 %u in1 %u)%s", word, shift, bits, value,
                    WriteOffset(base, context), ReadOffset(base, context), OutputValid(base, context), Input0Valid(base, context), Input1Valid(base, context), "\n");
        }
        return value;
    }
    void Set(uint8_t* base, uint32_t context, uint32_t word, uint32_t shift, uint32_t bits, uint32_t value)
    {
        std::lock_guard<std::mutex> lock(g_audioMutex);
        SetField(base, context, word, shift, bits, value);
        // An input buffer given to a context that ran dry part way through
        // an enable: the sound had a gap.
        if (word == 0 && (shift == 20 || shift == 21))
        {
            auto found = g_xma.find(context);
            if (found != g_xma.end() && found->second.starved)
            {
                found->second.starved = false;
                g_underruns.fetch_add(1, std::memory_order_relaxed);
            }
        }
        Work(base, context);
    }
}

PPC_FUNC(__imp__XMAIsInputBuffer0Valid)  { Trace("XMAIsInputBuffer0Valid", ctx); ctx.r3.u32 = Advanced(base, ctx.r3.u32, 0, 20, 1); }
PPC_FUNC(__imp__XMAIsInputBuffer1Valid)  { Trace("XMAIsInputBuffer1Valid", ctx); ctx.r3.u32 = Advanced(base, ctx.r3.u32, 0, 21, 1); }
PPC_FUNC(__imp__XMAIsOutputBufferValid)  { Trace("XMAIsOutputBufferValid", ctx); ctx.r3.u32 = Advanced(base, ctx.r3.u32, 9, 30, 1); }

PPC_FUNC(__imp__XMASetInputBuffer0Valid) { Trace("XMASetInputBuffer0Valid", ctx); Set(base, ctx.r3.u32, 0, 20, 1, 1); ctx.r3.u32 = X_ERROR_SUCCESS; }
PPC_FUNC(__imp__XMASetInputBuffer1Valid) { Trace("XMASetInputBuffer1Valid", ctx); Set(base, ctx.r3.u32, 0, 21, 1, 1); ctx.r3.u32 = X_ERROR_SUCCESS; }
PPC_FUNC(__imp__XMASetOutputBufferValid) { Trace("XMASetOutputBufferValid", ctx); Set(base, ctx.r3.u32, 9, 30, 1, 1); ctx.r3.u32 = X_ERROR_SUCCESS; }

PPC_FUNC(__imp__XMAGetInputBufferReadOffset)
{ Trace("XMAGetInputBufferReadOffset", ctx);
    std::lock_guard<std::mutex> lock(g_audioMutex);
    Work(base, ctx.r3.u32);
    ctx.r3.u32 = Guest::Read32(base, ctx.r3.u32 + 2 * 4) & 0x03FFFFFF;
}
PPC_FUNC(__imp__XMAGetOutputBufferReadOffset) { Trace("XMAGetOutputBufferReadOffset", ctx); ctx.r3.u32 = Advanced(base, ctx.r3.u32, 9, 0, 5); }
PPC_FUNC(__imp__XMAGetOutputBufferWriteOffset) { Trace("XMAGetOutputBufferWriteOffset", ctx); ctx.r3.u32 = Advanced(base, ctx.r3.u32, 0, 27, 5); }

PPC_FUNC(__imp__XMASetInputBufferReadOffset)
{ Trace("XMASetInputBufferReadOffset", ctx);
    std::lock_guard<std::mutex> lock(g_audioMutex);
    Guest::Write32(base, ctx.r3.u32 + 2 * 4, ctx.r4.u32 & 0x03FFFFFF);
    auto found = g_xma.find(ctx.r3.u32);
    if (found != g_xma.end()) found->second.packetInBuffer = (ctx.r4.u32 & 0x03FFFFFF) / (2048 * 8);
    ctx.r3.u32 = X_ERROR_SUCCESS;
}
PPC_FUNC(__imp__XMASetOutputBufferReadOffset)
{ Trace("XMASetOutputBufferReadOffset", ctx);
    Set(base, ctx.r3.u32, 9, 0, 5, ctx.r4.u32);
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// --- XAudio ----------------------------------------------------------------

// X_RESULT XAudioRegisterRenderDriverClient(ULONG* callback, ULONG* clientOut)
PPC_FUNC(__imp__XAudioRegisterRenderDriverClient)
{
    Kernel::CountImport("XAudioRegisterRenderDriverClient");

    // The first argument points at two words: the function to call, and the
    // value to call it with.
    if (ctx.r3.u32 != 0)
    {
        const uint32_t callback = Guest::Read32(base, ctx.r3.u32);
        const uint32_t context = Guest::Read32(base, ctx.r3.u32 + 4);
        g_callback.store(callback);
        g_callbackContext.store(context);
        printf("audio: render callback at 0x%08X, context 0x%08X, "
               "pumping at 187 Hz\n", callback, context);
        fflush(stdout);
        StartPump();
    }

    std::lock_guard<std::mutex> lock(g_audioMutex);
    if (ctx.r4.u32 != 0)
        Guest::Write32(base, ctx.r4.u32, ++g_registeredClients);
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XAudioUnregisterRenderDriverClient(ULONG client)
PPC_FUNC(__imp__XAudioUnregisterRenderDriverClient)
{
    Kernel::CountImport("XAudioUnregisterRenderDriverClient");
    g_callback.store(0);
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XAudioSubmitRenderDriverFrame(ULONG client, VOID* samples)
PPC_FUNC(__imp__XAudioSubmitRenderDriverFrame)
{
    Kernel::CountImport("XAudioSubmitRenderDriverFrame");

    {
        static std::atomic<int> announced{ 0 };
        const int n = announced.fetch_add(1);
        if (XmaTrace() && (n < 3 || n == 600 || n == 1200))
        {
            printf("audio: frame %d: client 0x%08X, frame at 0x%08X, words", n, ctx.r3.u32, ctx.r4.u32);
            for (uint32_t i = 0; i < 8; i++) printf(" %08X", Guest::Read32(base, ctx.r4.u32 + i * 4));
            printf(", words at +1024:");
            for (uint32_t i = 0; i < 4; i++) printf(" %08X", Guest::Read32(base, ctx.r4.u32 + 1024 + i * 4));
            printf("\n");
            fflush(stdout);
        }
    }
    // The frame the title just finished mixing, played. Success is reported
    // whatever happens to it: a title that waits for its own audio to drain
    // stops dead, so a frame the device could not take is dropped quietly.
    Audio::SubmitFrame(ctx.r4.u32);

    std::lock_guard<std::mutex> lock(g_audioMutex);
    g_submittedFrames++;
    Kernel::Stats().audioFrames.fetch_add(1, std::memory_order_relaxed);
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// X_RESULT XAudioGetVoiceCategoryVolume(ULONG category, float* volume)
PPC_FUNC(__imp__XAudioGetVoiceCategoryVolume)
{
    Kernel::CountImport("XAudioGetVoiceCategoryVolume");
    if (ctx.r4.u32 != 0)
        Guest::Write32(base, ctx.r4.u32, 0x3F800000);   // 1.0f
    ctx.r3.u32 = X_ERROR_SUCCESS;
}

// ULONG XAudioGetVoiceCategoryVolumeChangeMask(ULONG driver, ULONG* mask)
PPC_FUNC(__imp__XAudioGetVoiceCategoryVolumeChangeMask)
{
    Kernel::CountImport("XAudioGetVoiceCategoryVolumeChangeMask");
    if (ctx.r4.u32 != 0)
        Guest::Write32(base, ctx.r4.u32, 0);   // nothing changed
    ctx.r3.u32 = X_ERROR_SUCCESS;
}
