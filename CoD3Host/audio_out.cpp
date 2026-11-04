#include "audio_out.h"
#include "kernel.h"
#include "settings.h"
#include "hitmarkers.h"

#include <algorithm>
#include <cmath>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <thread>
#include <chrono>

#include <Windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

namespace
{
    // The shape of a frame, fixed by the console: 256 samples a channel, six
    // channels, 48 kHz.
    constexpr uint32_t SamplesPerChannel = 256;
    constexpr uint32_t Channels = 6;
    constexpr uint32_t SampleRate = 48000;

    // Eight frames in flight is about forty three milliseconds of sound. Fewer
    // and the device runs dry whenever the title misses a frame; more and the
    // sound lags visibly behind what is on screen.
    constexpr int Buffers = 8;

    std::mutex g_mutex;
    HWAVEOUT g_device = nullptr;
    bool g_tried = false;
    uint32_t g_outChannels = 2;     // what the device was opened with: 2, or 6 for 5.1
    int g_openedFor = -1;           // the speakers setting it was opened for

    struct Block
    {
        WAVEHDR header{};
        int16_t samples[SamplesPerChannel * Channels]{};   // interleaved, g_outChannels a sample
    };
    Block g_blocks[Buffers];
    int g_next = 0;

    std::atomic<uint64_t> g_played{ 0 };
    std::atomic<uint64_t> g_dropped{ 0 };

    // The loudest sample that has gone out, so a run that plays perfect silence
    // can be told apart from one that plays sound nobody can hear. If the title
    // mixes nothing but its XMA voices, and that decoder is not here, every
    // sample leaving this file is a zero and the speakers are correctly silent.
    std::atomic<int> g_peak{ 0 };
    std::atomic<int> g_recentPeak{ 0 };            // the loudest since the last report: whether there is sound now
    std::atomic<uint32_t> g_nonzeroBytes{ 0 };   // the most of a frame's bytes ever seen nonzero
    // The limiter (SubmitFrame): its gain now, the frames it brought down,
    // and the loudest the mix would have been without it since the last
    // report (past full scale is what it is for).
    float g_limiterGain = 1.0f;
    std::atomic<uint64_t> g_limited{ 0 };
    std::atomic<int> g_recentPreLimit{ 0 };

    // COD3_AUDIODUMP=path: everything that goes to the device, as a stereo
    // 48 kHz WAV, for listening to a run after it or looking at the wave.
    // The header's sizes are brought up to date every so often, so a run
    // cut short still leaves a file that plays.
    FILE* g_dump = nullptr;
    uint32_t g_dumpBytes = 0;
    uint32_t g_dumpFrames = 0;

    void DumpHeader()
    {
        uint8_t header[44] = {};
        auto put32 = [&](int at, uint32_t v) { header[at] = uint8_t(v); header[at + 1] = uint8_t(v >> 8); header[at + 2] = uint8_t(v >> 16); header[at + 3] = uint8_t(v >> 24); };
        auto put16 = [&](int at, uint32_t v) { header[at] = uint8_t(v); header[at + 1] = uint8_t(v >> 8); };
        memcpy(header, "RIFF", 4); put32(4, 36 + g_dumpBytes); memcpy(header + 8, "WAVEfmt ", 8);
        put32(16, 16); put16(20, 1); put16(22, g_outChannels); put32(24, SampleRate); put32(28, SampleRate * 2 * g_outChannels); put16(32, 2 * g_outChannels); put16(34, 16);
        memcpy(header + 36, "data", 4); put32(40, g_dumpBytes);
        fseek(g_dump, 0, SEEK_SET);
        fwrite(header, 1, sizeof(header), g_dump);
        fseek(g_dump, 0, SEEK_END);
    }

    void Dump(const int16_t* samples, size_t bytes)
    {
        static const char* const path = getenv("COD3_AUDIODUMP");
        if (path == nullptr) return;
        if (g_dump == nullptr)
        {
            g_dump = fopen(path, "wb");
            if (g_dump == nullptr) return;
            DumpHeader();
        }
        fwrite(samples, 1, bytes, g_dump);
        g_dumpBytes += uint32_t(bytes);
        if (++g_dumpFrames % 64 == 0) { DumpHeader(); fflush(g_dump); }
    }

    // A big endian float out of guest memory.
    float GuestFloat(uint32_t address)
    {
        const uint32_t bits = Guest::Read32(Guest::Base, address);
        float value;
        memcpy(&value, &bits, sizeof(value));
        return value;
    }

    // The speakers Windows is set up for: the default output's mix format,
    // its channel count and which speakers they are. Zero when it cannot
    // be asked.
    uint32_t SpeakerChannels(DWORD& mask)
    {
        mask = 0;
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        uint32_t channels = 0;
        IMMDeviceEnumerator* enumerator = nullptr;
        IMMDevice* device = nullptr;
        IAudioClient* client = nullptr;
        WAVEFORMATEX* mix = nullptr;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(&enumerator))) &&
            SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device)) &&
            SUCCEEDED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(&client))) &&
            SUCCEEDED(client->GetMixFormat(&mix)) && mix != nullptr)
        {
            channels = mix->nChannels;
            if (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mix->cbSize >= 22)
                mask = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix)->dwChannelMask;
        }
        if (mix) CoTaskMemFree(mix);
        if (client) client->Release();
        if (device) device->Release();
        if (enumerator) enumerator->Release();
        if (SUCCEEDED(com)) CoUninitialize();
        return channels;
    }

    // The 5.1 layouts (ksmedia.h's KSAUDIO_SPEAKER_5POINT1 and
    // _5POINT1_SURROUND) and the PCM subformat, spelled out here rather
    // than taken from the kernel streaming headers.
    constexpr DWORD Speakers51Back = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
                                     SPEAKER_LOW_FREQUENCY | SPEAKER_BACK_LEFT | SPEAKER_BACK_RIGHT;
    constexpr DWORD Speakers51Side = SPEAKER_FRONT_LEFT | SPEAKER_FRONT_RIGHT | SPEAKER_FRONT_CENTER |
                                     SPEAKER_LOW_FREQUENCY | SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT;
    const GUID SubtypePcm = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71 } };

    // 5.1 or stereo, by the setting (F11, Controls, "Speakers") and the
    // speakers: COD3_AUDIOCHANNELS=2|6 over both.
    bool WantSurround(int setting, uint32_t speakers)
    {
        static const int forced = []() { const char* t = getenv("COD3_AUDIOCHANNELS"); return t ? atoi(t) : 0; }();
        if (forced == 2) return false;
        if (forced == 6) return true;
        if (setting == 1) return false;
        if (setting == 2) return true;
        return speakers >= 6;
    }

    int16_t ToPcm(float value, float volume)
    {
        // A little headroom, because six channels folded into two can add up to
        // more than one and clipping is far more audible than being quiet.
        // The settings menu's master volume on top, on a curve so the slider's
        // middle sounds like half as loud rather than nearly full.
        const float scaled = value * 0.8f * volume * 32767.0f;
        return int16_t(std::clamp(scaled, -32768.0f, 32767.0f));
    }
}

bool Audio::Open()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_device != nullptr) return true;
    if (g_tried) return false;
    g_tried = true;

    // The console mixes six channels: front left, front right, centre, low
    // frequency, and the two surrounds, the order Windows' 5.1 has them in
    // (Xenia and UnleashedRecomp hand the frame over in the same order).
    // Given to Windows as they are when the speakers have them, rather than
    // folded into two. The surrounds go to the side speakers when the
    // speakers have sides (7.1, and 5.1 set up as "surround"), else the
    // back ones, so a 7.1 set does not leave its sides silent.
    const int setting = Settings::Get().audioChannels;
    g_openedFor = setting;
    DWORD speakerMask = 0;
    const uint32_t speakers = SpeakerChannels(speakerMask);
    MMRESULT opened = MMSYSERR_ERROR;
    g_outChannels = 2;
    if (WantSurround(setting, speakers))
    {
        WAVEFORMATEXTENSIBLE surround{};
        surround.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
        surround.Format.nChannels = 6;
        surround.Format.nSamplesPerSec = SampleRate;
        surround.Format.wBitsPerSample = 16;
        surround.Format.nBlockAlign = surround.Format.nChannels * surround.Format.wBitsPerSample / 8;
        surround.Format.nAvgBytesPerSec = surround.Format.nSamplesPerSec * surround.Format.nBlockAlign;
        surround.Format.cbSize = sizeof(WAVEFORMATEXTENSIBLE) - sizeof(WAVEFORMATEX);
        surround.Samples.wValidBitsPerSample = 16;
        const bool sides = (speakerMask & (SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT)) != 0;
        surround.dwChannelMask = sides ? Speakers51Side : Speakers51Back;
        surround.SubFormat = SubtypePcm;
        opened = waveOutOpen(&g_device, WAVE_MAPPER, &surround.Format, 0, 0, CALLBACK_NULL);
        if (opened == MMSYSERR_NOERROR) g_outChannels = 6;
        else
        {
            g_device = nullptr;
            printf("audio: 5.1 could not be opened (error %u, the speakers have %u channels); stereo instead\n", unsigned(opened), speakers);
        }
    }
    if (g_outChannels == 2)
    {
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = 2;
        format.nSamplesPerSec = SampleRate;
        format.wBitsPerSample = 16;
        format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        opened = waveOutOpen(&g_device, WAVE_MAPPER, &format, 0, 0, CALLBACK_NULL);
    }
    if (opened != MMSYSERR_NOERROR)
    {
        g_device = nullptr;
        printf("audio: no output device, the run will be silent (error %u)\n",
            unsigned(opened));
        fflush(stdout);
        return false;
    }

    for (Block& block : g_blocks)
    {
        block.header = WAVEHDR{};
        block.header.lpData = reinterpret_cast<LPSTR>(block.samples);
        block.header.dwBufferLength = SamplesPerChannel * g_outChannels * sizeof(int16_t);
        waveOutPrepareHeader(g_device, &block.header, sizeof(WAVEHDR));
        // Marked done so the first pass through finds them all free.
        block.header.dwFlags |= WHDR_DONE;
    }

    printf("audio: playing the title's own mix, 48 kHz %s (the speakers have %u channels)\n",
        g_outChannels == 6 ? ((speakerMask & (SPEAKER_SIDE_LEFT | SPEAKER_SIDE_RIGHT)) ? "5.1, surrounds to the sides" : "5.1") : "stereo", speakers);
    fflush(stdout);
    return true;
}

void Audio::Close()
{
    std::lock_guard<std::mutex> lock(g_mutex);
    if (g_device == nullptr) return;

    waveOutReset(g_device);
    for (Block& block : g_blocks)
        waveOutUnprepareHeader(g_device, &block.header, sizeof(WAVEHDR));
    waveOutClose(g_device);
    g_device = nullptr;
    g_next = 0;
}

bool Audio::SubmitFrame(uint32_t guestAddress)
{
    if (guestAddress == 0) return false;
    // The speakers setting changed: the device opened again for it.
    if (g_device != nullptr && Settings::Get().audioChannels != g_openedFor)
    {
        Close();
        std::lock_guard<std::mutex> lock(g_mutex);
        g_tried = false;
    }
    if (!Open()) return false;

    std::unique_lock<std::mutex> lock(g_mutex);
    if (g_device == nullptr) return false;

    // The next buffer in the ring, once the device has finished with it.
    // The frames come from this runtime's own pump thread, so a short wait
    // here is how the pump keeps the device's time rather than its own:
    // a frame a second was dropped with a click when the two drifted
    // apart. Longer than a few frames' worth and the frame is dropped
    // rather than the pump held.
    Block& block = g_blocks[g_next];
    for (int waited = 0; (block.header.dwFlags & WHDR_DONE) == 0; waited++)
    {
        if (waited >= 20)
        {
            g_dropped.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        lock.unlock();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        lock.lock();
    }
    g_next = (g_next + 1) % Buffers;

    // Six channels, one after another in memory, in the console's order: left,
    // right, centre, low frequency, left surround, right surround. Folded into
    // two the usual way, with the centre and the surrounds shared between them
    // at about seven tenths, which keeps the total energy the same.
    const uint32_t channelBytes = SamplesPerChannel * 4;
    {
        // How much of the frame is anything at all, for the report: a mixer
        // that writes nothing and one whose layout this misreads look the
        // same at the speakers and different here.
        const uint8_t* bytes = Guest::Ptr(guestAddress);
        uint32_t nonzero = 0;
        for (uint32_t i = 0; i < SamplesPerChannel * Channels * 4; i++) nonzero += bytes[i] != 0;
        if (nonzero > g_nonzeroBytes.load(std::memory_order_relaxed)) g_nonzeroBytes.store(nonzero, std::memory_order_relaxed);
    }
    const auto settings = Settings::Get();
    const float percent = float(settings.masterVolume) / 100.0f;
    const float volume = percent * percent;
    const uint32_t out = g_outChannels;
    // The frame as it will go out, before the limiter: each channel to its
    // own speaker in the order they came, or folded into two.
    float mixed[SamplesPerChannel * Channels];
    float framePeak = 0.0f;
    for (uint32_t i = 0; i < SamplesPerChannel; i++)
    {
        const uint32_t offset = guestAddress + i * 4;
        if (out == 6)
        {
            for (uint32_t c = 0; c < 6; c++) mixed[i * 6 + c] = GuestFloat(offset + c * channelBytes);
        }
        else
        {
            const float l  = GuestFloat(offset + 0 * channelBytes);
            const float r  = GuestFloat(offset + 1 * channelBytes);
            const float c  = GuestFloat(offset + 2 * channelBytes);
            const float ls = GuestFloat(offset + 4 * channelBytes);
            const float rs = GuestFloat(offset + 5 * channelBytes);
            mixed[i * 2 + 0] = l + 0.707f * c + 0.707f * ls;
            mixed[i * 2 + 1] = r + 0.707f * c + 0.707f * rs;
        }
    }
    Hitmarkers::Mix(mixed, SamplesPerChannel, out, settings);
    for (uint32_t i = 0; i < SamplesPerChannel; i++)
    {
        for (uint32_t c = 0; c < out; c++)
        {
            float& v = mixed[i * out + c];
            if (!(v == v)) v = 0.0f;   // not a number: silence, not a click
            framePeak = std::max(framePeak, std::fabs(v));
        }
    }

    // A limiter ahead of the sixteen bits. The title's float mix goes past
    // full scale in the heaviest fighting (a campaign level's log had the
    // loudest sample at 100% one second in thirty), and samples clipped
    // there crackle. A frame whose loudest sample would pass the ceiling
    // has the gain brought down at once to what it needs, all the channels
    // together so nothing moves between the speakers; afterwards the gain
    // comes back up over about a fifth of a second. Below the ceiling
    // nothing is touched. COD3_NOLIMITER=1 clips as before.
    static const bool noLimiter = getenv("COD3_NOLIMITER") != nullptr;
    constexpr float Ceiling = 0.97f;
    constexpr float Release = 0.026f;   // a 256 sample frame's share of a 200 ms recovery
    const float scale = 0.8f * volume;
    const float loudestIn = framePeak * scale;
    const float before = g_limiterGain;
    float after = before + (1.0f - before) * Release;
    if (!noLimiter && loudestIn * after > Ceiling) after = Ceiling / loudestIn;
    if (noLimiter) after = 1.0f;
    if (after < before) g_limited.fetch_add(1, std::memory_order_relaxed);
    g_limiterGain = after;
    const int loudestPre = int(std::min(loudestIn, 100.0f) * 32767.0f);
    if (loudestPre > g_recentPreLimit.load(std::memory_order_relaxed)) g_recentPreLimit.store(loudestPre, std::memory_order_relaxed);
    for (uint32_t i = 0; i < SamplesPerChannel; i++)
    {
        // Going down: the whole frame at the new gain, so its loudest
        // sample is under the ceiling. Coming back up: a ramp across it.
        const float gain = after < before ? after : before + (after - before) * (float(i + 1) / float(SamplesPerChannel));
        int loudest = 0;
        for (uint32_t c = 0; c < out; c++)
        {
            const int16_t sample = ToPcm(mixed[i * out + c] * gain, volume);
            block.samples[i * out + c] = sample;
            loudest = std::max(loudest, std::abs(int(sample)));
        }
        if (loudest > g_peak.load(std::memory_order_relaxed))
            g_peak.store(loudest, std::memory_order_relaxed);
        if (loudest > g_recentPeak.load(std::memory_order_relaxed))
            g_recentPeak.store(loudest, std::memory_order_relaxed);
    }

    const uint32_t bytes = SamplesPerChannel * out * sizeof(int16_t);
    Dump(block.samples, bytes);
    block.header.dwFlags &= ~WHDR_DONE;
    block.header.dwBufferLength = bytes;
    if (waveOutWrite(g_device, &block.header, sizeof(WAVEHDR)) != MMSYSERR_NOERROR)
    {
        block.header.dwFlags |= WHDR_DONE;
        g_dropped.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    g_played.fetch_add(1, std::memory_order_relaxed);
    return true;
}

void Audio::Report()
{
    const uint64_t played = g_played.load();
    if (played == 0) return;

    printf("audio: at most %u of %u frame bytes nonzero; ", g_nonzeroBytes.load(), SamplesPerChannel * Channels * 4);
    printf("%llu frames played, %llu dropped, loudest sample %.1f%% of "
           "full scale (%.1f%% since the last report, %.1f%% before the limiter, %llu frames limited)\n",
        (unsigned long long)played, (unsigned long long)g_dropped.load(),
        g_peak.load() * 100.0 / 32767.0, g_recentPeak.exchange(0) * 100.0 / 32767.0,
        g_recentPreLimit.exchange(0) * 100.0 / 32767.0, (unsigned long long)g_limited.load());
    fflush(stdout);
}
