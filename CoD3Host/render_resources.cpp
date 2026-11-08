#include "render_resources.h"
#include "parallel.h"
#include "render_table.h"
#include "render_stats.h"
#include "kernel.h"
#include "write_watch.h"
#include "tuning.h"
#include "verify.h"
#include "texture_pack.h"
#include "render_queue.h"
#include "render.h"   // 0.5.677: Render::SetTextureImage

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <thread>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include <d3d11_1.h>
#include <wrl/client.h>

namespace
{
    // 0.5.413: the textures', vertex buffers' and index buffers' entries,
    // by handle, in blocks of 256 that never move. They were std::deques,
    // which Microsoft's library builds of one entry a block for anything
    // over 16 bytes: each look up by handle (some 25000 a frame in
    // Marseille's busy views, on the command thread) went through the
    // deque's map to an entry of its own somewhere on the heap. Here it is
    // one small table of blocks, then the entry, its neighbours beside it.
    // Like the deque, an entry stays where it is while more are added.
    // Only what the three tables use: emplace_back, size, back, [].
    template <typename T>
    class StableArray
    {
    public:
        static constexpr size_t BlockShift = 8, BlockSize = size_t(1) << BlockShift;
        T& operator[](size_t i) { return m_blocks[i >> BlockShift][i & (BlockSize - 1)]; }
        const T& operator[](size_t i) const { return m_blocks[i >> BlockShift][i & (BlockSize - 1)]; }
        size_t size() const { return m_size; }
        T& back() { return (*this)[m_size - 1]; }
        // A new entry as T() makes it (the blocks' entries are made so).
        T& emplace_back()
        {
            if ((m_size >> BlockShift) == m_blocks.size()) m_blocks.emplace_back(new T[BlockSize]());
            return (*this)[m_size++];
        }
    private:
        std::vector<std::unique_ptr<T[]>> m_blocks;
        size_t m_size = 0;
    };
}

using Microsoft::WRL::ComPtr;

namespace
{
    using namespace RenderResources;

    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_context = nullptr;
    uint64_t g_frame = 0;
    // 0.5.432: the map being loaded or played (TexturePack::CurrentMapKey),
    // looked at once a frame, and a count that moves on when it changes
    std::string g_mapKey;
    uint32_t g_mapGeneration = 0;

    // Released when unused this long: half a minute of frames, so a level
    // change lets the last level's textures go.
    constexpr uint64_t UnusedFrames = 1800;

    std::atomic<uint64_t> g_textureUploads{ 0 }, g_textureBytes{ 0 }, g_bufferUploads{ 0 }, g_bufferBytes{ 0 };
    std::atomic<uint64_t> g_indexBytes{ 0 }, g_constantBytes{ 0 };
    uint64_t g_resourceBytes = 0;
    uint32_t g_released = 0;

    // --- the scale ----------------------------------------------------------------------------

    float g_scale = 1.0f;
    float g_requestedScale = 1.0f;
    float g_widen = 1.0f, g_requestedWiden = 1.0f;   // the width's own factor over the scale (RequestWiden)
    uint32_t g_shadowDetail = 1, g_requestedShadowDetail = 1;   // the shadow maps' own factor (RequestShadowDetail)
    double g_effectsScale = 1.0, g_requestedEffectsScale = 1.0;   // the 560 pitch surfaces' scale over the frame's (PitchScale)
    uint32_t g_samples = 1;
    uint32_t g_requestedSamples = 1;
    bool g_hdrTargets = false, g_requestedHdrTargets = false;   // the HDR mode's 16 bit float targets
    uint32_t g_mipSkip = 0, g_requestedMipSkip = 0;   // the texture quality: top levels left out

    // The count the device can do, at or under the one asked for.
    uint32_t SupportedSamples(uint32_t wanted)
    {
        for (uint32_t samples = wanted; samples > 1; samples /= 2)
        {
            UINT colour = 0, depth = 0;
            UINT half = 0;
            if (SUCCEEDED(g_device->CheckMultisampleQualityLevels(DXGI_FORMAT_R8G8B8A8_UNORM, samples, &colour)) && colour > 0 &&
                SUCCEEDED(g_device->CheckMultisampleQualityLevels(DXGI_FORMAT_R16G16B16A16_FLOAT, samples, &half)) && half > 0 &&
                SUCCEEDED(g_device->CheckMultisampleQualityLevels(DXGI_FORMAT_D32_FLOAT_S8X24_UINT, samples, &depth)) && depth > 0)
                return samples;
        }
        return 1;
    }

    // Rows a surface of a pitch gets. The console's EDRAM would hold more,
    // but the title draws into 624 rows of its 1040 wide surfaces most of
    // the time, 512 of the 560 wide ones and 256 of the 320 wide, and a
    // surface as tall as EDRAM allows at four times the size is a lot of
    // memory for nothing: 720 rows for the wide ones and the pitch for the
    // rest to start with, and more when a level asks (EnsureRows: the
    // crossroads level's shadow map is 1024 rows of the 1040 wide surface).
    // COD3_TARGETROWS=N pins it.
    uint32_t g_rowsAsked[16384 / 32 + 1] = {};   // by pitch in 32s, the most a draw or resolve has used

    uint32_t TargetRows(uint32_t pitch, uint32_t bytesPerSample)
    {
        if (pitch == 0) return 0;
        static const uint32_t pinned = []() { const char* t = getenv("COD3_TARGETROWS"); return t ? uint32_t(strtoul(t, nullptr, 10)) : 0u; }();
        const uint32_t edram = (10u << 20) / (pitch * bytesPerSample);
        uint32_t rows = pinned != 0 ? pinned : pitch >= 1024 ? 720 : std::max(pitch, 256u);
        if (pinned == 0 && pitch / 32 < sizeof(g_rowsAsked) / sizeof(g_rowsAsked[0])) rows = std::max(rows, g_rowsAsked[pitch / 32]);
        return std::min(std::min(rows, edram), 1440u);
    }

    // --- the console's texture tiling ---------------------------------------------------------

    // Where a texel lies in a tiled surface, in bytes: the console's 2D
    // tiling, coordinates split into a 32 by 32 macro tile and a position
    // inside it with the bits of the two interleaved. From Xenia; checked
    // one to one against the resolve when this runtime still resolved on
    // the CPU.
    inline uint32_t TiledOffset(uint32_t x, uint32_t y, uint32_t pitch, uint32_t bytesPerTexelLog2)
    {
        const uint32_t alignedPitch = (pitch + 31u) & ~31u;
        const uint32_t macro = ((x >> 5) + (y >> 5) * (alignedPitch >> 5)) << (bytesPerTexelLog2 + 7);
        const uint32_t micro = ((x & 7u) + ((y & 0xEu) << 2)) << bytesPerTexelLog2;
        const uint32_t offset = macro + ((micro & ~0xFu) << 1) + (micro & 0xFu) + ((y & 1u) << 4);
        return ((offset & ~0x1FFu) << 3) + ((y & 16u) << 7) + ((offset & 0x1C0u) << 2)
             + (((((y & 8u) >> 2) + (x >> 3)) & 3u) << 6) + (offset & 0x3Fu);
    }

    // What a resource's memory looks like, for telling whether the title
    // has written it since the upload. Dense: every byte, in four lanes,
    // for a resource that changed lately (the title's dynamic buffers,
    // a texture it draws into), so no change of theirs is missed. Sampled:
    // sixty four words across the range plus the ends, for one that has
    // been still, where a change is a rare and large thing (the memory
    // reused for something else).
    // COD3_FPAUDIT=1: what the dense looks are spent on, every five
    // seconds: by memory, how many bytes and looks, and which kind.
    thread_local const char* t_fingerprintKind = "other";
    void AuditFingerprint(const uint8_t* data, size_t bytes)
    {
        struct Use { uint64_t bytes = 0, looks = 0; size_t size = 0; const char* kind = ""; };
        static std::unordered_map<const uint8_t*, Use> uses;
        static auto reported = std::chrono::steady_clock::now();
        Use& use = uses[data];
        use.bytes += bytes; use.looks++; use.size = bytes; use.kind = t_fingerprintKind;
        const auto now = std::chrono::steady_clock::now();
        if (now - reported < std::chrono::seconds(5)) return;
        reported = now;
        std::vector<std::pair<const uint8_t*, Use>> ordered(uses.begin(), uses.end());
        std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) { return a.second.bytes > b.second.bytes; });
        std::map<std::string, std::pair<uint64_t, uint64_t>> byKind;   // kind and size class: bytes, resources
        for (const auto& [at, u] : ordered)
        {
            auto& k = byKind[std::string(u.kind) + (u.size <= (256u << 10) ? " small" : " large")];
            k.first += u.bytes; k.second++;
        }
        printf("fingerprints, five seconds:");
        for (const auto& [kind, k] : byKind) printf(" %s %.1f MB over %llu,", kind.c_str(), k.first / 1048576.0, (unsigned long long)k.second);
        printf("\n");
        for (size_t i = 0; i < ordered.size() && i < 12; i++)
            printf("  %s %08llx: %u bytes, %llu looks, %.1f MB\n", ordered[i].second.kind, (unsigned long long)(ordered[i].first - Guest::Base),
                unsigned(ordered[i].second.size), (unsigned long long)ordered[i].second.looks, ordered[i].second.bytes / 1048576.0);
        fflush(stdout);
        uses.clear();
    }

    uint64_t Fingerprint(const uint8_t* data, size_t bytes, bool dense)
    {
        static const bool audit = getenv("COD3_FPAUDIT") != nullptr;
        if (audit && dense) AuditFingerprint(data, bytes);
        RenderStats::Frame().fingerprints++;
        RenderStats::Frame().fingerprintBytes += dense ? bytes : std::min<size_t>(bytes, 65 * 64);
        constexpr uint64_t K = 1099511628211ull;
        uint64_t hash = 1469598103934665603ull ^ bytes;
        if (dense && !Tuning::On(Tuning::strongChangeCheck))
        {
            // The four lane fingerprint of 0.5.13 and before, for comparing
            // (F11, Graphics, Optimizations): a change of only the top bit
            // of two words in a lane cancels in it.
            uint64_t b = 0x9E3779B97F4A7C15ull, c = 0xBF58476D1CE4E5B9ull, d = 0x94D049BB133111EBull;
            size_t at = 0;
            for (; at + 32 <= bytes; at += 32)
            {
                uint64_t w[4];
                memcpy(w, data + at, 32);
                hash = (hash ^ w[0]) * K; b = (b ^ w[1]) * K; c = (c ^ w[2]) * K; d = (d ^ w[3]) * K;
            }
            for (; at + 8 <= bytes; at += 8)
            {
                uint64_t word;
                memcpy(&word, data + at, 8);
                hash = (hash ^ word) * K;
            }
            if (at < bytes)
            {
                uint64_t word = 0;
                memcpy(&word, data + at, bytes - at);
                hash = (hash ^ word) * K;
            }
            return ((hash ^ b) * K) ^ ((c ^ d) * K);
        }
        if (dense)
        {
            // Eight lanes, each a multiply and a turn of the word: the turn
            // brings a word's top bits down into the next step's low ones.
            // Without it a lane's top bit never reached any other, and two
            // changes of it cancelled: a vertex buffer whose only change was
            // the sign of the same coordinate in two vertices a stride apart
            // (a model mirrored, a normal turned over) looked the same and
            // kept the old vertices. Eight lanes rather than four keep the
            // multiplier busy through the turn's extra step: as fast as before.
            auto turn = [](uint64_t x) { return (x << 31) | (x >> 33); };
            uint64_t b = 0x9E3779B97F4A7C15ull, c = 0xBF58476D1CE4E5B9ull, d = 0x94D049BB133111EBull;
            uint64_t e = 0xD6E8FEB86659FD93ull, f = 0xA0761D6478BD642Full, g = 0xE7037ED1A0B428DBull, h = 0x8EBC6AF09C88C6E3ull;
            size_t at = 0;
            for (; at + 64 <= bytes; at += 64)
            {
                uint64_t w[8];
                memcpy(w, data + at, 64);
                hash = turn((hash ^ w[0]) * K); b = turn((b ^ w[1]) * K); c = turn((c ^ w[2]) * K); d = turn((d ^ w[3]) * K);
                e = turn((e ^ w[4]) * K); f = turn((f ^ w[5]) * K); g = turn((g ^ w[6]) * K); h = turn((h ^ w[7]) * K);
            }
            for (; at + 8 <= bytes; at += 8)
            {
                uint64_t word;
                memcpy(&word, data + at, 8);
                hash = turn((hash ^ word) * K);
            }
            if (at < bytes)
            {
                uint64_t word = 0;
                memcpy(&word, data + at, bytes - at);
                hash = turn((hash ^ word) * K);
            }
            return ((hash ^ b) * K) ^ ((c ^ d) * K) ^ turn(((e ^ f) * K) ^ ((g ^ h) * K));
        }
        const size_t step = std::max<size_t>(bytes / 64, 64);
        for (size_t at = 0; at + 8 <= bytes; at += step)
        {
            uint64_t word;
            memcpy(&word, data + at, 8);
            hash = (hash ^ word) * K;
        }
        if (bytes >= 8)
        {
            uint64_t word;
            memcpy(&word, data + bytes - 8, 8);
            hash = (hash ^ word) * K;
        }
        return hash;
    }

    // --- render targets -----------------------------------------------------------------------

    struct ColorEntry { ColorTarget target; ComPtr<ID3D11Texture2D> texture; ComPtr<ID3D11RenderTargetView> view; ComPtr<ID3D11ShaderResourceView> resource; uint32_t key[2] = {}; };
    struct DepthEntry { DepthTarget target; ComPtr<ID3D11Texture2D> texture; ComPtr<ID3D11DepthStencilView> view; ComPtr<ID3D11ShaderResourceView> resource; uint32_t key[2] = {}; };
    std::deque<ColorEntry> g_colorTargets;   // by handle less one
    std::deque<DepthEntry> g_depthTargets;
    RenderTable::KeyTable<2> g_colorKeys;
    RenderTable::KeyTable<2> g_depthKeys;

    DXGI_FORMAT ColorFormat(uint32_t format, uint32_t& bytesPerSample)
    {
        bytesPerSample = 4;
        switch (format)
        {
        // The HDR mode's upgrade: the same guest format and the same EDRAM
        // bytes (the rows a pitch holds stay the console's), a float target
        // on the host so the drawing is not clipped at one.
        case 0: case 1: return g_hdrTargets ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
        case 2: case 10: return DXGI_FORMAT_R10G10B10A2_UNORM;
        case 3: case 12: bytesPerSample = 8; return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case 4: return DXGI_FORMAT_R16G16_UNORM;
        case 5: bytesPerSample = 8; return DXGI_FORMAT_R16G16B16A16_UNORM;
        case 6: return DXGI_FORMAT_R16G16_FLOAT;
        case 7: bytesPerSample = 8; return DXGI_FORMAT_R16G16B16A16_FLOAT;
        case 14: return DXGI_FORMAT_R32_FLOAT;
        case 15: bytesPerSample = 8; return DXGI_FORMAT_R32G32_FLOAT;
        default: return DXGI_FORMAT_R8G8B8A8_UNORM;
        }
    }

    // The samples a target is drawn with: the frame's own (a pitch of 640
    // and more: 1040 in a level, 1280 in the menus) with the chosen MSAA,
    // the smaller targets with one. Those are the shadow maps (512 in a
    // 560 pitch), the half size smoke (520 by 312, the same pitch) and the
    // bloom's steps (256 in a 320 pitch): depth only or soft, MSAA shows
    // nothing in them and at 200% it cost a shadow map 2585 by 2585 by 4
    // samples of depth. A draw's colour and depth targets share its pitch,
    // so they always agree. COD3_SMALLMSAA=1 multisamples them as before.
    uint32_t SamplesFor(uint32_t pitch)
    {
        static const bool all = getenv("COD3_SMALLMSAA") != nullptr;
        return all || pitch >= 640 ? g_samples : 1;
    }

    // The surfaces of a 560 pitch are the title's half size ones: the
    // smoke and the soft particles (520 by 312) with the half size depth
    // they test against, and the shadow cascades (512 by 512). Drawn at
    // half the frame's size, the smoke was put back over the frame from a
    // picture of half its pixels, and whatever stood in front of it had a
    // stepped rim of the smoke's coarser pixels. Here they are drawn twice
    // as large again: the smoke at the frame's own resolution, the shadow
    // cascades twice as sharp. COD3_HALFSIZE=1 keeps them at half size.
    // F11's "Full resolution smoke and shadows"; COD3_HALFSIZE=1 forces half.
    double PitchScale(uint32_t pitch)
    {
        static const bool halfSize = getenv("COD3_HALFSIZE") != nullptr;
        return (!halfSize && pitch == 560) ? g_effectsScale : 1.0;
    }

    // A target's size on the host. The frame's (and those of its shape:
    // the half size smoke's) widened when filling the screen; the shadow
    // maps (a power of two across, below the frame's: the sun's 512, the
    // 256 ones) left square and made finer by the shadow detail, their
    // edges sharper where they fall near. (Team Friend's look, merged on
    // 0.5.546 with this tree's effects scale, PitchScale.)
    void HostSize(uint32_t pitch, uint32_t rows, uint32_t& width, uint32_t& height)
    {
        const double scale = double(g_scale) * PitchScale(pitch);
        const bool shadow = pitch < 640 && (pitch & (pitch - 1)) == 0;
        const double across = shadow ? scale * g_shadowDetail : scale * g_widen, down = shadow ? scale * g_shadowDetail : scale;
        width = std::max(1u, uint32_t(std::lround(double(pitch) * across)));
        height = std::max(1u, uint32_t(std::lround(double(rows) * down)));
    }

    // --- resolved surfaces -----------------------------------------------------------------------

    // `crop`: the top left corner of the surface, for a fetch that samples
    // less than the resolve wrote. The resolve rounds its rectangle up (the
    // aim blur's quarter frame is resolved 264x160 and sampled 260x156), and
    // the console's fetch reads its own width and height from the corner;
    // the whole surface here would put the fetch's 0..1 across the padding
    // as well, the blurred picture stretched off the sharp one and the
    // padding's black along the edges. Copied again after each resolve.
    struct ResolvedEntry
    {
        Resolved surface; ComPtr<ID3D11Texture2D> texture; ComPtr<ID3D11ShaderResourceView> resource; ComPtr<ID3D11RenderTargetView> view; uint32_t physical = 0;
        ComPtr<ID3D11Texture2D> crop; ComPtr<ID3D11ShaderResourceView> cropView;
        uint32_t cropGuestWidth = 0, cropGuestHeight = 0, cropWidth = 0, cropHeight = 0;
        uint64_t cropSerial = 0;
        void DropCrop()
        {
            if (crop) g_resourceBytes -= uint64_t(cropWidth) * cropHeight * 4;
            crop.Reset(); cropView.Reset();
            cropGuestWidth = cropGuestHeight = cropWidth = cropHeight = 0;
            cropSerial = 0;
        }
    };
    std::deque<ResolvedEntry> g_resolved;
    RenderTable::KeyTable<1> g_resolvedKeys;
    uint64_t g_resolveSerial = 0;
    // 0.5.411: moved on whenever ResolvedAt's answer could change (a
    // resolve's surface added, made again or let go), so the texture
    // lookup's front cache (TextureForUnchecked) never keeps a "not a
    // resolve" answer past it.
    uint64_t g_resolvedVersion = 1;

    // --- textures --------------------------------------------------------------------------------

    constexpr uint32_t StableLooks = 8;   // looks without a change before a resource counts as still

    // Whether a resource's whole memory is hashed rather than sampled.
    // Everything small is, always: the HUD's fonts, icons and compass are
    // a few tens of kilobytes and change a handful of texels at a time,
    // which sixty four samples miss: the HUD then shows the picture it
    // had before, and flickers between the two as the misses come and go.
    // Everything under a quarter of a megabyte is hashed whole every
    // frame (1.65 microseconds a draw in the forest level against 1.13,
    // and 60 frames a second either way); the level's big textures fall
    // back to sampling once they have been still for a while, where
    // hashing them whole would be tens of megabytes a frame.
    // COD3_DENSEALL=1 hashes everything whole, whatever it costs.
    constexpr size_t AlwaysDenseBytes = 256u << 10;
    // 0.5.374: mid-frame buffers looked at by each draw's part (below).
    bool MidFrameRanges()
    {
        static const bool on = getenv("COD3_MIDFRAMEHASH") == nullptr && getenv("COD3_NOMIDFRAME") == nullptr;
        return on;
    }
    // COD3_MISSAUDIT=1: a resource looked at by sampling is also hashed
    // whole, and a change the samples did not see is named: which memory,
    // how much, a texture or a buffer.
    void MissAudit(const char* kind, uint32_t physical, const uint8_t* data, size_t bytes, bool dense, uint64_t& auditDense)
    {
        static const bool auditEnv = getenv("COD3_MISSAUDIT") != nullptr;
        if ((!auditEnv && !Verify::On(Verify::Watched)) || dense) return;
        Verify::Checked(Verify::Watched);
        const uint64_t whole = Fingerprint(data, bytes, true);
        if (whole == auditDense) return;
        if (auditDense != 0)
            Verify::Mismatch(Verify::Watched, "%s at %08X (%zu bytes) changed and its sampled look missed it", kind, physical, bytes);
        auditDense = whole;
    }

    // COD3_WATCHAUDIT=1: a resource found the same by its pages
    // (COD3_WRITEWATCH) is hashed anyway, and a change the pages did not
    // tell is named.
    void WatchAudit(const char* kind, uint32_t physical, const uint8_t* data, size_t bytes, bool dense, uint64_t fingerprint)
    {
        static const bool auditEnv = getenv("COD3_WATCHAUDIT") != nullptr;
        if (!auditEnv && !Verify::On(Verify::Watched)) return;
        Verify::Checked(Verify::Watched);
        static std::atomic<uint64_t> looks{ 0 }, missed{ 0 };
        const uint64_t look = looks.fetch_add(1) + 1;
        if (Fingerprint(data, bytes, dense) != fingerprint)
        {
            missed.fetch_add(1);
            Verify::Mismatch(Verify::Watched, "the %s at %08X (%zu bytes) changed with no write seen by its pages", kind, physical, bytes);
        }
        if ((look & 0xFFFF) == 0)
        {
            printf("render: watch audit: %llu looks by the pages, %llu missed\n", (unsigned long long)look,
                (unsigned long long)missed.load());
            fflush(stdout);
        }
    }

    bool WantDense(uint32_t stable, size_t bytes)
    {
        // COD3_DENSEALL=1: everything whole, whatever it costs, for
        // telling a missed change from another kind of flicker.
        static const bool all = getenv("COD3_DENSEALL") != nullptr;
        return all || stable < StableLooks || bytes <= AlwaysDenseBytes;
    }

    struct TextureEntry
    {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> resource;
        ComPtr<ID3D11ShaderResourceView> face;   // a cube's first face as a flat texture
        bool cube = false;
        uint32_t key[4] = {};
        uint64_t fingerprint = 0;
        uint64_t auditDense = 0;        // COD3_MISSAUDIT: every byte's fingerprint when the sampling began
        uint64_t denseFingerprint = 0;  // every byte's fingerprint when the sampling began, for a watched write
        uint64_t checkedFrame = 0;      // the frame the fingerprint was last compared in
        uint64_t usedFrame = 0;
        uint32_t stable = StableLooks;  // checks in a row that found no change; a texture starts as still
        bool dense = false;             // the fingerprint is of every byte
        uint64_t watchedSince = 0;      // COD3_WRITEWATCH: its pages watched from this write sequence on, 0 not
        uint32_t width = 0, height = 0, levels = 0;
        uint64_t bytes = 0;
        bool live = false;
        std::string replacedFor;        // 0.5.234: the map whose own replacement this is ("" none)
        std::string packKey;            // 0.5.432: its texture pack key (when the pack was looked at)
        uint32_t mapGeneration = 0;     // 0.5.432: g_mapGeneration when it was made or last checked
        std::vector<uint8_t> sourceMirror;   // verify mode: the memory it was last made from
        uint32_t mirrorEpoch = 0;
        uint64_t verifiedFrame = 0;
    };
    StableArray<TextureEntry> g_textures;   // by handle less one; handle 1 is the white texture (0.5.413: was a std::deque)
    // 0.5.677: pictures given for textures by their place (Render::SetTextureImage), by GPU offset
    std::mutex g_placedLock;
    std::map<uint32_t, std::string> g_placedImages;
    std::atomic<uint32_t> g_placedLast{ 0 };   // 0.5.695: the last place given (a fetch of it is logged)

    // --- the texture pool ------------------------------------------------------------------
    //
    // Making a texture was the frame time's spikes: a hitch line of 0.5.11
    // took apart the slow frames, and nearly all were frames that met new
    // textures (the level streaming them in as the player moves), each
    // CreateTexture2D with its texels about a millisecond on the command
    // thread at a high frame rate, eight or ten of them in one frame. The
    // texels' layout was a tenth of that. What costs is the driver finding
    // and initialising memory for each new texture.
    //
    // So textures are kept for reuse rather than made each time, as
    // UnleashedRecomp keeps its streaming off the drawing thread: a texture
    // let go (unused for a while, its memory freed by the title, replaced)
    // goes into a pool by its size, levels and format; a texture changed in
    // place is written in place; and a worker thread keeps two spare
    // textures of each shape lately asked for made ahead, the device being
    // free threaded for making things. Only the texels are then sent, with
    // UpdateSubresource, on the command thread. A shape never seen before
    // is still made there, once. COD3_NOTEXPOOL=1: made each time, as before.
    struct PooledTexture
    {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> view;
    };
    struct PoolWant
    {
        D3D11_TEXTURE2D_DESC desc{};
        uint64_t lastUse = 0;
    };
    std::mutex g_poolMutex;
    std::condition_variable g_poolWake;
    std::unordered_map<uint64_t, std::vector<PooledTexture>> g_pool;
    std::unordered_map<uint64_t, PoolWant> g_poolWants;   // shapes to keep spares of
    uint64_t g_poolBytes = 0;
    bool g_poolAhead = false;                             // the worker runs
    std::atomic<uint64_t> g_poolHits{ 0 }, g_poolMisses{ 0 }, g_poolInPlace{ 0 }, g_poolMadeAhead{ 0 };
    constexpr uint64_t PoolBytesLimit = 256ull << 20;     // spares and textures let go, together
    constexpr size_t PoolSpares = 2;
    constexpr uint64_t PoolSpareBytesLimit = 24ull << 20; // nothing bigger is made ahead
    constexpr size_t PoolWantsLimit = 64;

    bool TexturePoolOn()
    {
        static const bool off = getenv("COD3_NOTEXPOOL") != nullptr;
        return !off && Tuning::On(Tuning::texturePool);
    }

    uint64_t PoolKey(const D3D11_TEXTURE2D_DESC& desc)
    {
        return uint64_t(desc.Width) | (uint64_t(desc.Height) << 16) | (uint64_t(desc.MipLevels) << 32) | (uint64_t(desc.Format) << 40);
    }

    // What a texture of the shape takes, its levels together.
    uint64_t PoolBytes(const D3D11_TEXTURE2D_DESC& desc)
    {
        const bool bc1 = desc.Format == DXGI_FORMAT_BC1_UNORM;
        const bool bc = bc1 || desc.Format == DXGI_FORMAT_BC2_UNORM || desc.Format == DXGI_FORMAT_BC3_UNORM;
        const uint32_t texel = desc.Format == DXGI_FORMAT_R8_UNORM ? 1 : 4;
        uint64_t bytes = 0;
        for (uint32_t level = 0; level < std::max(desc.MipLevels, 1u); level++)
        {
            const uint32_t w = std::max(desc.Width >> level, 1u), h = std::max(desc.Height >> level, 1u);
            bytes += bc ? uint64_t((w + 3) / 4) * ((h + 3) / 4) * (bc1 ? 8 : 16) : uint64_t(w) * h * texel;
        }
        return bytes;
    }

    // A pooled texture's shape: flat, one array slice, sampled only, and
    // written with UpdateSubresource (not immutable).
    bool Poolable(const D3D11_TEXTURE2D_DESC& desc)
    {
        return desc.Usage == D3D11_USAGE_DEFAULT && desc.ArraySize == 1 && desc.MiscFlags == 0 &&
               desc.BindFlags == D3D11_BIND_SHADER_RESOURCE && desc.SampleDesc.Count == 1;
    }

    bool MakePooled(const D3D11_TEXTURE2D_DESC& desc, PooledTexture& made)
    {
        if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &made.texture))) return false;
        if (FAILED(g_device->CreateShaderResourceView(made.texture.Get(), nullptr, &made.view))) { made.texture.Reset(); return false; }
        return true;
    }

    // The worker: makes the spares of the shapes asked for lately, one at a
    // time, and sleeps when every shape has its two.
    void PoolWorker()
    {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        std::unique_lock<std::mutex> lock(g_poolMutex);
        for (;;)
        {
            uint64_t key = 0;
            D3D11_TEXTURE2D_DESC desc{};
            bool short_ = false;
            for (const auto& [wantKey, want] : g_poolWants)
            {
                auto found = g_pool.find(wantKey);
                const size_t have = found == g_pool.end() ? 0 : found->second.size();
                if (have < PoolSpares && g_poolBytes + PoolBytes(want.desc) <= PoolBytesLimit)
                {
                    key = wantKey; desc = want.desc; short_ = true;
                    break;
                }
            }
            if (!short_) { g_poolWake.wait(lock); continue; }
            lock.unlock();
            PooledTexture made;
            const bool ok = MakePooled(desc, made);
            lock.lock();
            if (!ok) { g_poolWants.erase(key); continue; }
            g_poolBytes += PoolBytes(desc);
            g_pool[key].push_back(std::move(made));
            g_poolMadeAhead.fetch_add(1, std::memory_order_relaxed);
        }
    }

    void StartTexturePool()
    {
        if (getenv("COD3_NOTEXPOOL") != nullptr || getenv("COD3_NOTEXAHEAD") != nullptr) return;
        // Only where the driver makes things concurrently with the drawing
        // (every current one does); else the worker would take the lock the
        // command thread's own calls wait on.
        D3D11_FEATURE_DATA_THREADING threading{};
        if (FAILED(g_device->CheckFeatureSupport(D3D11_FEATURE_THREADING, &threading, sizeof(threading))) || !threading.DriverConcurrentCreates)
        {
            printf("render: the driver does not make resources concurrently; textures are not made ahead\n");
            return;
        }
        g_poolAhead = true;
        std::thread(PoolWorker).detach();
    }

    // A texture of the shape: a spare, or made now. Asking for a shape also
    // has the worker keep spares of it.
    bool AcquirePooled(const D3D11_TEXTURE2D_DESC& desc, PooledTexture& out)
    {
        const uint64_t key = PoolKey(desc), bytes = PoolBytes(desc);
        bool hit = false;
        {
            std::lock_guard<std::mutex> lock(g_poolMutex);
            auto found = g_pool.find(key);
            if (found != g_pool.end() && !found->second.empty())
            {
                out = std::move(found->second.back());
                found->second.pop_back();
                g_poolBytes -= bytes;
                hit = true;
            }
            if (g_poolAhead && bytes <= PoolSpareBytesLimit)
            {
                PoolWant& want = g_poolWants[key];
                want.desc = desc;
                want.lastUse = g_frame;
                if (g_poolWants.size() > PoolWantsLimit)
                {
                    // The shape used longest ago is no longer kept spares of.
                    auto oldest = g_poolWants.begin();
                    for (auto it = g_poolWants.begin(); it != g_poolWants.end(); ++it)
                        if (it->second.lastUse < oldest->second.lastUse) oldest = it;
                    g_poolWants.erase(oldest);
                }
            }
        }
        if (g_poolAhead) g_poolWake.notify_one();
        if (hit) { g_poolHits.fetch_add(1, std::memory_order_relaxed); return true; }
        g_poolMisses.fetch_add(1, std::memory_order_relaxed);
        return MakePooled(desc, out);
    }

    // A texture let go: into the pool when it is of a pooled shape and
    // there is room, else released.
    void ReleasePooled(ComPtr<ID3D11Texture2D>& texture, ComPtr<ID3D11ShaderResourceView>& view)
    {
        RenderQueue::Sync(1);   // 0.5.391: no queued draw still names it
        if (TexturePoolOn() && texture && view)
        {
            D3D11_TEXTURE2D_DESC desc{};
            texture->GetDesc(&desc);
            if (Poolable(desc))
            {
                const uint64_t bytes = PoolBytes(desc);
                std::lock_guard<std::mutex> lock(g_poolMutex);
                if (g_poolBytes + bytes <= PoolBytesLimit)
                {
                    g_pool[PoolKey(desc)].push_back(PooledTexture{ std::move(texture), std::move(view) });
                    g_poolBytes += bytes;
                }
            }
        }
        texture.Reset();
        view.Reset();
    }


    // Every resource is looked at once a frame, as it always was: a look
    // put off is a texture that changes late on screen, which is a flicker
    // in the menu and the HUD. What the look costs is what changes: every
    // byte of a resource that has changed lately, sixty four samples of one
    // that has been still (Fingerprint). One not used for a second counts
    // as changing again, since the title may have put something else there.
    bool LookThisFrame(uint32_t& stable, uint64_t usedFrame, uint32_t)
    {
        if (g_frame - usedFrame > 60) stable = 0;
        return true;
    }
    ComPtr<ID3D11Texture2D> g_whiteCube;
    ComPtr<ID3D11ShaderResourceView> g_whiteCubeView;
    ComPtr<ID3D11Texture3D> g_whiteVolume;
    ComPtr<ID3D11ShaderResourceView> g_whiteVolumeView;
    RenderTable::KeyTable<4> g_textureKeys;
    std::vector<uint32_t> g_freeTextures;

    struct BufferEntry
    {
        ComPtr<ID3D11Buffer> buffer;
        ComPtr<ID3D11ShaderResourceView> resource;
        uint32_t key[2] = {};
        uint64_t fingerprint = 0;
        uint64_t auditDense = 0;        // COD3_MISSAUDIT: every byte's fingerprint when the sampling began
        uint64_t checkedFrame = 0;
        uint64_t usedFrame = 0;
        uint32_t stable = 0;            // checks in a row that found no change; a buffer starts as changing
        bool dense = true;              // the fingerprint is of every byte
        uint64_t sampled = 0;           // the sampled fingerprint, as the frame's first look found it
        bool midFrame = false;          // the title rewrites it between the draws of one frame
        bool rangeShadow = false;       // 0.5.374: a mid-frame one with its shadow kept as what went up, each draw's part compared
        uint32_t bytes = 0;             // the buffer's size
        uint32_t seen = 0;              // the largest size a fetch asked for
        uint64_t watchedSince = 0;      // COD3_WRITEWATCH: its pages watched from this write sequence on, 0 not
        uint64_t liveSince = 0;         // 0.5.99: a big buffer's pages watched within the frame from this sequence on
        uint8_t falseWrites = 0;        // looks its pages said written that found it the same: past a few, not watched
        bool live = false;
        std::vector<uint8_t> shadow;    // a big buffer: what went up, to send only what changed
        uint64_t changedFrame = 0;      // the frame a look last found it changed
        uint64_t lookedCall = 0;        // the VertexBufferFor call that last looked at it
        std::vector<uint8_t> mirror;    // verify mode: what was sent to the GPU
        uint32_t mirrorEpoch = 0;       // the verify epoch the mirror was last whole in
        const char* lastSent = "";      // how it was last sent, for verify mode's report
    };

    // Verify mode (verify.h): what was sent of a buffer kept beside it, so
    // a draw's vertices can be held against it. `data` is the buffer's start.
    void Sent(BufferEntry& entry, uint32_t first, uint32_t end, const uint8_t* data, const char* how)
    {
        entry.lastSent = how;
        if (!Verify::On(Verify::Buffers) || entry.bytes == 0) return;
        if (entry.mirror.size() < entry.bytes) entry.mirror.resize(entry.bytes);
        end = std::min(end, entry.bytes);
        if (first < end) memcpy(entry.mirror.data() + first, data + first, end - first);
        if (first == 0 && end >= entry.seen) entry.mirrorEpoch = Verify::epoch.load(std::memory_order_relaxed);
    }
    StableArray<BufferEntry> g_buffers;   // 0.5.413: was a std::deque
    uint64_t g_vertexCalls = 0;         // VertexBufferFor's calls, counted

    // A vertex buffer this big is compared with a copy of what went up, a
    // piece at a time, and only the pieces that changed go up. The title
    // keeps a twelve megabyte pool its few dynamic models draw from, a few
    // kilobytes of it rewritten each frame, and it went up whole, all of
    // it looked at first, every frame. COD3_NOBUFFERDIFF=1: whole again.
    constexpr uint32_t BigBufferBytes = 1u << 20;
    constexpr uint32_t BufferPieceBytes = 16u << 10;
    RenderTable::KeyTable<2> g_bufferKeys;
    std::vector<uint32_t> g_freeBuffers;

    // A draw's indices kept converted, by where they are, how many, and how
    // (COD3_WRITEWATCH: kept only while the pages say they are the same).
    // A range is a candidate first, its fingerprint noted but nothing
    // made: one seen again unchanged gets its buffer, so the title's
    // dynamic indices, at new places every frame, make none.
    struct IndexEntry
    {
        ComPtr<ID3D11Buffer> buffer;     // none while a candidate
        uint32_t key[4] = {};            // physical, count, bit 0 32 bit, the reset index (~0 none)
        uint64_t fingerprint = 0;
        uint64_t watchedSince = 0;       // its pages watched from this write sequence on, 0 not
        uint64_t usedFrame = 0;
        uint32_t stable = 0;             // looks in a row that found it the same
        uint32_t bytes = 0;              // the host buffer's
        bool live = false;
    };
    StableArray<IndexEntry> g_indexEntries;   // by handle less one (0.5.413: was a std::deque)
    RenderTable::KeyTable<4> g_indexKeys;
    std::vector<uint32_t> g_freeIndexEntries;
    std::vector<uint8_t> g_indexScratch;

    void DropIndexEntry(uint32_t i)
    {
        RenderQueue::Sync(2);
        IndexEntry& entry = g_indexEntries[i];
        g_indexKeys.Erase(entry.key);
        g_resourceBytes -= entry.bytes;
        entry.buffer.Reset();
        entry.live = false;
        g_freeIndexEntries.push_back(i + 1);
    }

    // A level's rows, split across the cores when there are enough bytes
    // in it to be worth waking them (a quarter of a megabyte: a 512 by 512
    // DXT5 and up). A level is laid out on the command thread when the
    // title first draws with it, and the textures a level streams in were
    // the frame time's spikes: each a millisecond or more of untiling on
    // that one thread, several in a frame as the player moved on.
    template <class Rows>
    void ForRows(uint32_t rows, size_t bytes, const Rows& rowsWork)
    {
        static const bool serial = getenv("COD3_SERIALUNTILE") != nullptr;
        if (serial || bytes < (256u << 10) || rows < 16) { rowsWork(0u, rows); return; }
        const int grain = std::max(4, int(rows) / (Parallel::Width() * 4));
        Parallel::For(0, int(rows), grain, [&](int first, int last) { rowsWork(uint32_t(first), uint32_t(last)); });
    }

    // Eight bytes with each pair swapped: the console's sixteen bit order.
    inline uint64_t SwapPairs(uint64_t v)
    {
        return ((v & 0x00FF00FF00FF00FFull) << 8) | ((v >> 8) & 0x00FF00FF00FF00FFull);
    }

    // The bytes of a texture level as the host wants them: untiled, and
    // the console's byte order undone. `x0`, `y0` are where the level lies
    // in its tile when the levels are packed.
    void LinearLevel(const uint8_t* data, uint32_t format, uint32_t endian, uint32_t width, uint32_t height,
                     uint32_t pitchTexels, bool tiled, uint32_t x0, uint32_t y0,
                     std::vector<uint8_t>& out, uint32_t& rowBytes)
    {
        if (format == 18 || format == 19 || format == 20)
        {
            const uint32_t blockBytes = format == 18 ? 8 : 16;
            const uint32_t blockShift = blockBytes == 8 ? 3 : 4;
            const uint32_t blocksAcross = std::max(pitchTexels / 4, 1u);
            const uint32_t blocksWide = (width + 3) / 4, blocksHigh = (height + 3) / 4;
            rowBytes = blocksWide * blockBytes;
            out.resize(size_t(rowBytes) * blocksHigh);
            uint8_t* const base = out.data();
            const uint32_t stride = rowBytes;
            ForRows(blocksHigh, out.size(), [&](uint32_t firstRow, uint32_t lastRow)
            {
                for (uint32_t by = firstRow; by < lastRow; by++)
                    for (uint32_t bx = 0; bx < blocksWide; bx++)
                    {
                        const uint32_t sx = bx + x0 / 4, sy = by + y0 / 4;
                        const uint32_t offset = tiled ? TiledOffset(sx, sy, blocksAcross, blockShift)
                                                      : (sy * blocksAcross + sx) * blockBytes;
                        uint8_t* to = base + size_t(by) * stride + size_t(bx) * blockBytes;
                        // The blocks' sixteen bit fields are stored the console's
                        // way round: each pair of bytes swapped, eight at a time.
                        uint64_t word;
                        memcpy(&word, data + offset, 8);
                        word = SwapPairs(word);
                        memcpy(to, &word, 8);
                        if (blockBytes == 16)
                        {
                            memcpy(&word, data + offset + 8, 8);
                            word = SwapPairs(word);
                            memcpy(to + 8, &word, 8);
                        }
                    }
            });
        }
        else if (format == 6)
        {
            rowBytes = width * 4;
            out.resize(size_t(rowBytes) * height);
            uint8_t* const base = out.data();
            const uint32_t stride = rowBytes;
            ForRows(height, out.size(), [&](uint32_t firstRow, uint32_t lastRow)
            {
                for (uint32_t y = firstRow; y < lastRow; y++)
                {
                    uint8_t* const row = base + size_t(y) * stride;
                    for (uint32_t x = 0; x < width; x++)
                    {
                        const uint32_t offset = tiled ? TiledOffset(x + x0, y + y0, pitchTexels, 2)
                                                      : ((y + y0) * pitchTexels + x + x0) * 4;
                        uint32_t word;
                        memcpy(&word, data + offset, 4);
                        switch (endian)
                        {
                        case 1: word = ((word & 0xFF00FF00u) >> 8) | ((word & 0x00FF00FFu) << 8); break;
                        case 2: word = _byteswap_ulong(word); break;
                        case 3: word = (word >> 16) | (word << 16); break;
                        default: break;
                        }
                        // The word is A R G B from the top, and the console's
                        // component x is its lowest byte: blue. The title's fetch
                        // swizzle puts it right, so the bytes stay as they are.
                        memcpy(row + size_t(x) * 4, &word, 4);
                    }
                }
            });
        }
        else if (format == 3 || format == 4 || format == 10 || format == 15)
        {
            // Sixteen bits a texel (1_5_5_5, 5_6_5, 8_8, 4_4_4_4: some HUD
            // icons, the campaign's among them), widened to eight bits a
            // component: x the lowest bits into the first byte, as format
            // 6's are, the fetch swizzle putting them right.
            rowBytes = width * 4;
            out.resize(size_t(rowBytes) * height);
            uint8_t* const base = out.data();
            const uint32_t stride = rowBytes;
            ForRows(height, out.size(), [&](uint32_t firstRow, uint32_t lastRow)
            {
                for (uint32_t y = firstRow; y < lastRow; y++)
                    for (uint32_t x = 0; x < width; x++)
                    {
                        const uint32_t offset = tiled ? TiledOffset(x + x0, y + y0, pitchTexels, 1)
                                                      : ((y + y0) * pitchTexels + x + x0) * 2;
                        uint16_t v;
                        memcpy(&v, data + offset, 2);
                        if (endian == 1 || endian == 2) v = uint16_t((v >> 8) | (v << 8));
                        auto wide = [](uint32_t bits, uint32_t count) { return uint8_t(count >= 8 ? bits : (bits * 255 + ((1u << count) - 1) / 2) / ((1u << count) - 1)); };
                        uint8_t* to = base + size_t(y) * stride + size_t(x) * 4;
                        switch (format)
                        {
                        case 3: to[0] = wide(v & 31, 5); to[1] = wide((v >> 5) & 31, 5); to[2] = wide((v >> 10) & 31, 5); to[3] = (v & 0x8000) ? 255 : 0; break;
                        case 4: to[0] = wide(v & 31, 5); to[1] = wide((v >> 5) & 63, 6); to[2] = wide((v >> 11) & 31, 5); to[3] = 255; break;
                        case 10: to[0] = uint8_t(v & 0xFF); to[1] = uint8_t(v >> 8); to[2] = 0; to[3] = 255; break;
                        default: to[0] = wide(v & 15, 4); to[1] = wide((v >> 4) & 15, 4); to[2] = wide((v >> 8) & 15, 4); to[3] = wide((v >> 12) & 15, 4); break;
                        }
                    }
            });
        }
        else   // 2: one byte a texel
        {
            rowBytes = width;
            out.resize(size_t(rowBytes) * height);
            uint8_t* const base = out.data();
            const uint32_t stride = rowBytes;
            ForRows(height, out.size(), [&](uint32_t firstRow, uint32_t lastRow)
            {
                for (uint32_t y = firstRow; y < lastRow; y++)
                    for (uint32_t x = 0; x < width; x++)
                    {
                        const uint32_t offset = tiled ? TiledOffset(x + x0, y + y0, pitchTexels, 0)
                                                      : ((y + y0) * pitchTexels + x + x0);
                        base[size_t(y) * stride + x] = data[offset];
                    }
            });
        }
    }

    // Where a texture's first level lies when its levels are packed into
    // one tile: a texture of sixteen texels or fewer on a side shares the
    // tile with its own mips, and the first level is not at the corner but
    // sixteen texels along, the smaller levels in the corner before it.
    // From Xenia's texture_info.cc, which had it from graph paper.
    void PackedBaseOffset(bool packed, uint32_t width, uint32_t height, uint32_t& x, uint32_t& y)
    {
        x = 0; y = 0;
        if (!packed) return;
        uint32_t log2Width = 0, log2Height = 0;
        while ((1u << log2Width) < width) log2Width++;
        while ((1u << log2Height) < height) log2Height++;
        if (std::min(log2Width, log2Height) > 4) return;
        if (log2Width > log2Height) y = 16; else x = 16;
    }

    bool IsPowerOfTwo(uint32_t v) { return v != 0 && (v & (v - 1)) == 0; }

    // --- texels sent through a staging texture ------------------------------------------
    //
    // With the pool, making textures stopped costing, and the 0.5.12 hitch
    // lines showed what was left: the texels' own trip. A frame that met
    // one big texture (a few megabytes of levels) spent about nine
    // milliseconds in UpdateSubresource, the same as the immutable texture
    // made with its texels before: under a gigabyte a second, the driver
    // rearranging the texels into the GPU's own layout on the command
    // thread. So the texels go into a staging texture instead (plain
    // memory, a memcpy) and the GPU's copy engine moves them into the
    // texture and lays them out (CopyResource), off the CPU. The staging
    // textures are kept by shape and used again three frames on, when the
    // GPU is done with them. COD3_NOSTAGING=1: UpdateSubresource as before.
    struct StagingTexture
    {
        ComPtr<ID3D11Texture2D> texture;
        uint64_t freeFrame = 0;
    };
    std::unordered_map<uint64_t, std::vector<StagingTexture>> g_staging;   // command thread only
    uint64_t g_stagingBytes = 0;
    constexpr uint64_t StagingBytesLimit = 192ull << 20;
    std::atomic<uint64_t> g_stagedUploads{ 0 }, g_directUploads{ 0 };

    bool StagingOn()
    {
        static const bool off = getenv("COD3_NOSTAGING") != nullptr;
        return !off && Tuning::On(Tuning::stagingUploads);
    }

    bool UploadByStaging(ID3D11Texture2D* target, const D3D11_TEXTURE2D_DESC& desc,
                         const std::vector<D3D11_SUBRESOURCE_DATA>& initial, const std::vector<std::vector<uint8_t>>& pixels, uint32_t levels)
    {
        if (!StagingOn() || target == nullptr) return false;
        std::vector<StagingTexture>& list = g_staging[PoolKey(desc)];
        StagingTexture* slot = nullptr;
        for (StagingTexture& candidate : list)
            if (candidate.freeFrame <= g_frame) { slot = &candidate; break; }
        if (slot == nullptr)
        {
            const uint64_t bytes = PoolBytes(desc);
            if (g_stagingBytes + bytes > StagingBytesLimit) return false;
            D3D11_TEXTURE2D_DESC staging = desc;
            staging.Usage = D3D11_USAGE_STAGING;
            staging.BindFlags = 0;
            staging.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            staging.MiscFlags = 0;
            StagingTexture made;
            if (FAILED(g_device->CreateTexture2D(&staging, nullptr, &made.texture))) return false;
            list.push_back(std::move(made));
            slot = &list.back();
            g_stagingBytes += bytes;
        }
        for (uint32_t level = 0; level < levels; level++)
        {
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(g_context->Map(slot->texture.Get(), level, D3D11_MAP_WRITE, D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped)))
            {
                // Still the GPU's (it ran more than three frames behind):
                // tried again later; sent the old way now.
                slot->freeFrame = g_frame + 1;
                return false;
            }
            const uint32_t pitch = initial[level].SysMemPitch;
            const uint32_t rows = pitch != 0 ? uint32_t(pixels[level].size() / pitch) : 0;
            uint8_t* const to = static_cast<uint8_t*>(mapped.pData);
            const uint8_t* const from = pixels[level].data();
            const uint32_t toPitch = mapped.RowPitch;
            ForRows(rows, pixels[level].size(), [&](uint32_t first, uint32_t last)
            {
                if (toPitch == pitch) memcpy(to + size_t(first) * pitch, from + size_t(first) * pitch, size_t(last - first) * pitch);
                else for (uint32_t row = first; row < last; row++) memcpy(to + size_t(row) * toPitch, from + size_t(row) * pitch, pitch);
            });
            g_context->Unmap(slot->texture.Get(), level);
        }
        g_context->CopyResource(target, slot->texture.Get());
        slot->freeFrame = g_frame + 3;
        g_stagedUploads.fetch_add(1, std::memory_order_relaxed);
        return true;
    }



    // The texture's levels from guest memory into a new texture. The
    // levels past the first lie one after another from the mip address,
    // each with its pitch and height rounded up to whole 32 block tiles;
    // once a level is sixteen texels or fewer on a side the rest are
    // packed into one tile, and those are left out: the sampler clamps to
    // the last level there is, thirty two texels, which is close enough
    // to a texel on the screen for nobody to see the difference.
    bool UploadTexture(TextureEntry& entry, const uint32_t fetch[6], const uint8_t* data)
    {
        // 0.5.391: textures go up on this thread, the drawing thread idle
        // (rare once a map is in; the staging copies map the context).
        RenderQueue::Sync(3);
        entry.replacedFor.clear();   // 0.5.234: set again by a map's own replacement
        entry.packKey.clear();
        entry.mapGeneration = g_mapGeneration;
        // 0.5.677: a picture given for this texture's place (Render::
        // SetTextureImage: the Webley's kill feed icon, CoD2's hud_webley)
        {
            std::string png;
            {
                std::lock_guard<std::mutex> lock(g_placedLock);
                const auto hit = g_placedImages.find(Guest::GpuOffset(fetch[1] & 0xFFFFF000u));
                if (hit != g_placedImages.end()) png = hit->second;
            }
            uint32_t w = 0, h = 0;
            std::vector<uint8_t> rgba;
            if (!png.empty() && TexturePack::ReadPngFile(std::filesystem::path(png), w, h, rgba) && w && h)
            {
                D3D11_TEXTURE2D_DESC desc{};
                desc.Width = w; desc.Height = h; desc.MipLevels = 1; desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
                desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                const D3D11_SUBRESOURCE_DATA initial{ rgba.data(), w * 4, 0 };
                ComPtr<ID3D11Texture2D> texture;
                ComPtr<ID3D11ShaderResourceView> view;
                if (SUCCEEDED(g_device->CreateTexture2D(&desc, &initial, &texture)) && SUCCEEDED(g_device->CreateShaderResourceView(texture.Get(), nullptr, &view)))
                {
                    entry.texture = texture; entry.resource = view;
                    entry.cube = false; entry.face.Reset();
                    entry.width = (fetch[2] & 0x1FFF) + 1; entry.height = ((fetch[2] >> 13) & 0x1FFF) + 1; entry.levels = 1;
                    entry.bytes = rgba.size();
                    printf("render: texture at %08X given the picture %s (%ux%u)\n", fetch[1] & 0xFFFFF000u, png.c_str(), w, h);
                    fflush(stdout);
                    g_textureUploads.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
            }
        }
        // The upload's two parts, timed for the hitch lines: laying the
        // texels out, then making the texture on the device.
        const auto uploadStarted = std::chrono::steady_clock::now();
        const uint32_t format = fetch[1] & 0x3F;
        const uint32_t endian = (fetch[1] >> 6) & 3;
        const uint32_t pitch = ((fetch[0] >> 22) & 0x1FF) * 32;
        const bool tiled = ((fetch[0] >> 31) & 1) != 0;
        const uint32_t width = (fetch[2] & 0x1FFF) + 1;
        const uint32_t height = ((fetch[2] >> 13) & 0x1FFF) + 1;
        const bool packedMips = ((fetch[5] >> 11) & 1) != 0;
        const uint32_t mipAddress = fetch[5] & 0xFFFFF000u;
        const uint32_t mipMax = (fetch[4] >> 6) & 0xF;
        const bool compressed = format == 18 || format == 19 || format == 20;
        const uint32_t blockBytes = format == 18 ? 8 : format == 19 || format == 20 ? 16 : format == 6 ? 4 : (format == 3 || format == 4 || format == 10 || format == 15) ? 2 : 1;
        const uint32_t blockTexels = compressed ? 4 : 1;

        DXGI_FORMAT hostFormat;
        switch (format)
        {
        case 2: hostFormat = DXGI_FORMAT_R8_UNORM; break;
        case 6: hostFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
        case 3: case 4: case 10: case 15: hostFormat = DXGI_FORMAT_R8G8B8A8_UNORM; break;
        case 18: hostFormat = DXGI_FORMAT_BC1_UNORM; break;
        case 19: hostFormat = DXGI_FORMAT_BC2_UNORM; break;
        case 20: hostFormat = DXGI_FORMAT_BC3_UNORM; break;
        default: return false;
        }

        // How many levels can be had: down to thirty two texels, as many as
        // the fetch says exist, and only for a power of two size, whose
        // levels are where the arithmetic below says.
        uint32_t levels = 1;
        if (mipAddress != 0 && mipMax > 0 && IsPowerOfTwo(width) && IsPowerOfTwo(height) && std::min(width, height) >= 64)
        {
            while (levels <= mipMax && std::min(width >> levels, height >> levels) >= 32) levels++;
        }
        static const bool noMips = getenv("COD3_NOMIPS") != nullptr;
        if (noMips) levels = 1;
        // The texture quality: the top levels left out, when there are
        // levels below them to keep. The title's arithmetic sees the size
        // it named; only the texels are fewer.
        const uint32_t skip = std::min(g_mipSkip, levels - 1);

        // A cube map: six faces, each a whole tiled picture of the size,
        // one after another, each taking its rows rounded up to whole
        // tiles. Its first level only: the faces' mips are not laid out
        // where the arithmetic below would look.
        const bool cube = ((fetch[5] >> 9) & 3) == 3;
        const uint32_t faces = cube ? 6 : 1;
        if (cube) levels = 1;
        const uint32_t kept = cube ? 1 : levels - skip;   // the levels uploaded, from `skip` down
        std::vector<std::vector<uint8_t>> pixels(kept * faces);
        std::vector<D3D11_SUBRESOURCE_DATA> initial(kept * faces);
        uint32_t packedX = 0, packedY = 0;
        PackedBaseOffset(packedMips, width, height, packedX, packedY);
        uint32_t rowBytes = 0;
        entry.bytes = 0;
        if (cube || skip == 0)
            for (uint32_t face = 0; face < faces; face++)
            {
                const uint32_t blocksHigh = (height + blockTexels - 1) / blockTexels;
                const uint32_t faceBytes = std::max(pitch / blockTexels, 1u) * ((blocksHigh + 31) & ~31u) * blockBytes;
                LinearLevel(data + size_t(face) * faceBytes, format, endian, width, height, pitch, tiled, packedX, packedY, pixels[face * kept], rowBytes);
                initial[face * kept] = { pixels[face * kept].data(), rowBytes, 0 };
                entry.bytes += pixels[face * kept].size();
            }

        uint32_t offset = 0;
        for (uint32_t level = 1; level < levels; level++)
        {
            const uint32_t w = std::max(width >> level, 1u), h = std::max(height >> level, 1u);
            const uint32_t blocksWide = (w + blockTexels - 1) / blockTexels, blocksHigh = (h + blockTexels - 1) / blockTexels;
            const uint32_t pitchBlocks = (blocksWide + 31) & ~31u, rowsBlocks = (blocksHigh + 31) & ~31u;
            if (level >= skip)
            {
                const uint8_t* levelData = Guest::Base + Guest::PhysicalAlias(mipAddress + offset);
                LinearLevel(levelData, format, endian, w, h, pitchBlocks * blockTexels, tiled, 0, 0, pixels[level - skip], rowBytes);
                initial[level - skip] = { pixels[level - skip].data(), rowBytes, 0 };
                entry.bytes += pixels[level - skip].size();
            }
            offset += pitchBlocks * rowsBlocks * blockBytes;
        }

        // A texture pack's replacement (texture_pack.h), by the first
        // level's texels; or the texture saved for one.
        if (!cube && TexturePack::Active())
        {
            std::vector<uint8_t> top;
            uint32_t topRow = 0;
            if (skip == 0) top = pixels[0];
            else LinearLevel(data, format, endian, width, height, pitch, tiled, packedX, packedY, top, topRow);
            const std::string key = TexturePack::Key(format, width, height, top);
            entry.packKey = key;   // 0.5.432
            TexturePack::NoteMap(key);   // 0.5.197: textures\maps\<map>.txt (texture_dump only)
            ID3D11Texture2D* replaced = nullptr;
            ID3D11ShaderResourceView* replacedView = nullptr;
            if (TexturePack::Load(key, g_device, g_context, &replaced, &replacedView, &entry.replacedFor))
            {
                entry.texture.Attach(replaced);
                entry.resource.Attach(replacedView);
                entry.cube = false;
                entry.face.Reset();
                entry.width = width;
                entry.height = height;
                entry.levels = 1;
                g_textureUploads.fetch_add(1, std::memory_order_relaxed);
                return true;
            }
            TexturePack::Dump(key, format, width, height, top);
        }

        const uint32_t topWidth = std::max(width >> skip, 1u), topHeight = std::max(height >> skip, 1u);
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = compressed ? ((topWidth + 3) & ~3u) : topWidth;
        desc.Height = compressed ? ((topHeight + 3) & ~3u) : topHeight;
        desc.MipLevels = kept;
        desc.ArraySize = faces;
        desc.Format = hostFormat;
        desc.SampleDesc.Count = 1;
        const bool pooled = !cube && TexturePoolOn();
        desc.Usage = pooled ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = cube ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;
        const auto createStarted = std::chrono::steady_clock::now();
        RenderStats::Frame().uploadLayoutNanoseconds += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(createStarted - uploadStarted).count());
        struct CreateTimed { std::chrono::steady_clock::time_point started; ~CreateTimed() {
            RenderStats::Frame().uploadCreateNanoseconds += uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()); } } createTimed{ createStarted };
        if (pooled)
        {
            // The texture it has, written in place when it is the same
            // shape (a texture the title changed); else one from the pool,
            // the old one going back to it.
            bool inPlace = false;
            if (entry.texture && entry.resource && !entry.cube)
            {
                D3D11_TEXTURE2D_DESC had{};
                entry.texture->GetDesc(&had);
                inPlace = Poolable(had) && PoolKey(had) == PoolKey(desc);
            }
            if (inPlace) g_poolInPlace.fetch_add(1, std::memory_order_relaxed);
            else
            {
                ReleasePooled(entry.texture, entry.resource);
                PooledTexture got;
                if (!AcquirePooled(desc, got)) return false;
                entry.texture = std::move(got.texture);
                entry.resource = std::move(got.view);
            }
            if (!UploadByStaging(entry.texture.Get(), desc, initial, pixels, kept))
            {
                g_directUploads.fetch_add(1, std::memory_order_relaxed);
                for (uint32_t level = 0; level < kept; level++)
                    g_context->UpdateSubresource(entry.texture.Get(), level, nullptr, initial[level].pSysMem, initial[level].SysMemPitch, 0);
            }
        }
        else
        {
            entry.texture.Reset();
            entry.resource.Reset();
            if (FAILED(g_device->CreateTexture2D(&desc, initial.data(), &entry.texture))) return false;
            if (FAILED(g_device->CreateShaderResourceView(entry.texture.Get(), nullptr, &entry.resource))) return false;
        }
        entry.cube = cube;
        entry.face.Reset();
        if (cube)
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC view{};
            view.Format = hostFormat;
            view.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            view.Texture2DArray.MipLevels = 1;
            view.Texture2DArray.ArraySize = 1;
            g_device->CreateShaderResourceView(entry.texture.Get(), &view, &entry.face);
        }
        entry.width = width;
        entry.height = height;
        entry.levels = kept;
        g_textureUploads.fetch_add(1, std::memory_order_relaxed);
        g_textureBytes.fetch_add(entry.bytes, std::memory_order_relaxed);
        RenderStats::Frame().textureUploads++;
        RenderStats::Frame().uploadBytes += entry.bytes;

        // COD3_D3DTEXDUMP=directory keeps every texture uploaded, the block
        // compressed ones as DDS and the rest as BMP, named by address.
        static const char* const dumpTextures = getenv("COD3_D3DTEXDUMP");
        if (dumpTextures != nullptr)
        {
            char name[512];
            snprintf(name, sizeof(name), "%s/tex-%08X-%ux%u-f%u.%s", dumpTextures, fetch[1] & 0xFFFFF000u, width, height, format, compressed ? "dds" : "bmp");
            if (FILE* out = fopen(name, "wb"))
            {
                const std::vector<uint8_t>& linear = pixels[0];
                if (compressed)
                {
                    uint32_t header[32] = {};
                    header[0] = 0x20534444; header[1] = 124; header[2] = 0x1 | 0x2 | 0x4 | 0x1000 | 0x80000;
                    header[3] = height; header[4] = width; header[5] = uint32_t(linear.size());
                    header[19] = 32; header[20] = 0x4;
                    header[21] = format == 18 ? 0x31545844 : format == 19 ? 0x33545844 : 0x35545844;
                    header[27] = 0x1000;
                    fwrite(header, 4, 32, out);
                    fwrite(linear.data(), 1, linear.size(), out);
                }
                else
                {
                    const uint32_t bytesPerTexel = (format == 6 || format == 3 || format == 4 || format == 10 || format == 15) ? 4 : 1;
                    std::vector<uint8_t> bgra(size_t(width) * height * 4);
                    for (uint32_t y = 0; y < height; y++)
                        for (uint32_t x = 0; x < width; x++)
                        {
                            const uint8_t* from = linear.data() + size_t(y) * width * bytesPerTexel + size_t(x) * bytesPerTexel;
                            uint8_t* to = bgra.data() + (size_t(y) * width + x) * 4;
                            if (bytesPerTexel == 1) { to[0] = to[1] = to[2] = from[0]; to[3] = 255; }
                            else { to[0] = from[2]; to[1] = from[1]; to[2] = from[0]; to[3] = from[3]; }
                        }
                    BITMAPFILEHEADER file{};
                    BITMAPINFOHEADER info{};
                    info.biSize = sizeof(info); info.biWidth = int(width); info.biHeight = -int(height); info.biPlanes = 1;
                    info.biBitCount = 32; info.biCompression = BI_RGB; info.biSizeImage = DWORD(bgra.size());
                    file.bfType = 0x4D42; file.bfOffBits = sizeof(file) + sizeof(info); file.bfSize = file.bfOffBits + info.biSizeImage;
                    fwrite(&file, sizeof(file), 1, out); fwrite(&info, sizeof(info), 1, out); fwrite(bgra.data(), 1, bgra.size(), out);
                }
                fclose(out);
            }
        }
        return true;
    }

    // How many bytes of guest memory the first level takes: what the
    // fingerprint covers.
    size_t SourceBytes(const uint32_t fetch[6])
    {
        const uint32_t format = fetch[1] & 0x3F;
        const uint32_t pitch = ((fetch[0] >> 22) & 0x1FF) * 32;
        const uint32_t height = ((fetch[2] >> 13) & 0x1FFF) + 1;
        if (format == 18 || format == 19 || format == 20)
            return size_t(std::max(pitch / 4, 1u)) * ((height + 3) / 4) * (format == 18 ? 8 : 16) + 4096;
        if (format == 6) return size_t(pitch) * height * 4;
        return size_t(pitch) * height;
    }

    // --- the rings --------------------------------------------------------------------------------

    struct Ring
    {
        ComPtr<ID3D11Buffer> buffer;
        uint32_t bytes = 0;
        uint32_t offset = 0;
        bool mappedHere = false;   // the last Map was on this thread's context, not in the queue

        bool Make(uint32_t size, UINT bind)
        {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = size;
            desc.Usage = D3D11_USAGE_DYNAMIC;
            desc.BindFlags = bind;
            desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            if (FAILED(g_device->CreateBuffer(&desc, nullptr, &buffer))) return false;
            bytes = size;
            return true;
        }

        // Room for `count` bytes, aligned, mapped for writing: where to write
        // and the offset it will have; null when the map failed. The ring is
        // discarded and started over when it is full.
        uint8_t* Map(uint32_t count, uint32_t alignment, uint32_t& at)
        {
            if (!buffer) return nullptr;
            const uint32_t aligned = (count + alignment - 1) & ~(alignment - 1);
            D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
            if (offset + aligned > bytes || offset == 0) { offset = 0; mode = D3D11_MAP_WRITE_DISCARD; }
            // 0.5.391: with the drawing thread, the bytes go in its queue
            // and it maps the ring at the same offset, in the same way.
            // (0.5.393: a write too big for the queue is mapped here, the
            // drawing thread caught up first.)
            mappedHere = !RenderQueue::Deferring() || count > RenderQueue::MaxRingWrite;
            if (!mappedHere)
            {
                at = offset;
                offset += aligned;
                return RenderQueue::RingWrite(buffer.Get(), mode == D3D11_MAP_WRITE_DISCARD, at, count);
            }
            RenderQueue::Sync(16);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (FAILED(g_context->Map(buffer.Get(), 0, mode, 0, &mapped))) return nullptr;
            at = offset;
            offset += aligned;
            return static_cast<uint8_t*>(mapped.pData) + at;
        }

        void Unmap() { if (mappedHere) g_context->Unmap(buffer.Get(), 0); }

        uint32_t Append(const void* data, uint32_t count, uint32_t alignment)
        {
            uint32_t at;
            uint8_t* to = Map(count, alignment, at);
            if (to == nullptr) return 0xFFFFFFFFu;
            memcpy(to, data, count);
            Unmap();
            return at;
        }
    };
    Ring g_indexRing, g_constantRing;
    bool g_constantOffsets = false;
    constexpr uint32_t IndexRingBytes = 16u << 20;
    constexpr uint32_t ConstantRingBytes = 16u << 20;
}

void RenderResources::Initialize(ID3D11Device* device, ID3D11DeviceContext* context)
{
    g_device = device;
    g_context = context;
    StartTexturePool();
    g_indexRing.Make(IndexRingBytes, D3D11_BIND_INDEX_BUFFER);
    D3D11_FEATURE_DATA_D3D11_OPTIONS options{};
    if (SUCCEEDED(device->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS, &options, sizeof(options))) && options.ConstantBufferOffsetting)
        g_constantOffsets = g_constantRing.Make(ConstantRingBytes, D3D11_BIND_CONSTANT_BUFFER);
    if (!g_constantOffsets) printf("render: constant buffer offsetting is not available on this device\n");

    // Handle 1: the white texture, for what cannot be uploaded.
    g_textures.emplace_back();
    TextureEntry& white = g_textures.back();
    const uint32_t pixel = 0xFFFFFFFFu;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = desc.Height = 1;
    desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{ &pixel, 4, 4 };
    if (SUCCEEDED(device->CreateTexture2D(&desc, &initial, &white.texture)))
        device->CreateShaderResourceView(white.texture.Get(), nullptr, &white.resource);
    white.live = true;
    white.width = white.height = white.levels = 1;
    // And a white cube for a program that reads a cube where the fetch
    // constant names none.
    desc.ArraySize = 6;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;
    D3D11_SUBRESOURCE_DATA faces[6];
    for (D3D11_SUBRESOURCE_DATA& f : faces) f = initial;
    if (SUCCEEDED(device->CreateTexture2D(&desc, faces, &g_whiteCube)))
        device->CreateShaderResourceView(g_whiteCube.Get(), nullptr, &g_whiteCubeView);
    // And a white volume: the title's volume textures are not uploaded
    // (their tiling is another matter), and a program reading one reads
    // white, as it did before.
    D3D11_TEXTURE3D_DESC volume{};
    volume.Width = volume.Height = volume.Depth = 1;
    volume.MipLevels = 1;
    volume.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    volume.Usage = D3D11_USAGE_IMMUTABLE;
    volume.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    if (SUCCEEDED(device->CreateTexture3D(&volume, &initial, &g_whiteVolume)))
        device->CreateShaderResourceView(g_whiteVolume.Get(), nullptr, &g_whiteVolumeView);
}

bool RenderResources::BeginFrame(uint64_t frame)
{
    RenderQueue::Sync(4);
    g_frame = frame;
    {
        // 0.5.432: the map, once a frame (not at each of the frame's texture looks)
        std::string map = TexturePack::CurrentMapKey();
        if (map != g_mapKey) { g_mapKey.swap(map); g_mapGeneration++; }
    }
    bool remade = false;
    if (g_requestedMipSkip != g_mipSkip)
    {
        // Every texture goes, to come back with the levels the quality asks.
        remade = true;
        g_mipSkip = g_requestedMipSkip;
        for (uint32_t i = 0; i < g_textures.size(); i++)
        {
            TextureEntry& entry = g_textures[i];
            if (!entry.live) continue;
            g_textureKeys.Erase(entry.key);
            g_resourceBytes -= entry.bytes;
            ReleasePooled(entry.texture, entry.resource);
            entry.face.Reset();
            entry.live = false;
            g_freeTextures.push_back(i + 1);
        }
        printf("render: textures are uploaded from mip level %u\n", g_mipSkip);
    }
    if (g_requestedSamples != g_samples)
    {
        remade = true;
        g_samples = g_requestedSamples;
        g_colorTargets.clear();
        g_depthTargets.clear();
        g_colorKeys.Clear();
        g_depthKeys.Clear();
        printf("render: the frame is drawn with %u sample%s a pixel\n", g_samples, g_samples == 1 ? "" : "s");
    }
    if (g_requestedHdrTargets != g_hdrTargets)
    {
        remade = true;
        g_hdrTargets = g_requestedHdrTargets;
        g_colorTargets.clear();
        g_depthTargets.clear();
        g_colorKeys.Clear();
        g_depthKeys.Clear();
        printf("render: the frame's colour targets are %s\n", g_hdrTargets ? "16 bit float (HDR)" : "8 bit");
    }
    if (g_requestedEffectsScale != g_effectsScale)
    {
        remade = true;
        g_effectsScale = g_requestedEffectsScale;
        g_colorTargets.clear();
        g_depthTargets.clear();
        g_colorKeys.Clear();
        g_depthKeys.Clear();
        printf("render: the half size surfaces are drawn at %.2f times their size\n", g_effectsScale);
    }
    if (g_requestedScale != g_scale || g_requestedWiden != g_widen || g_requestedShadowDetail != g_shadowDetail)
    {
        remade = true;
        g_scale = g_requestedScale;
        g_widen = g_requestedWiden;
        g_shadowDetail = g_requestedShadowDetail;
        // The targets are made again at the new size as they are asked for.
        g_colorTargets.clear();
        g_depthTargets.clear();
        g_colorKeys.Clear();
        g_depthKeys.Clear();
        printf("render: the frame is drawn at %.3f times the title's size, its width %.3f times more, the shadow maps %u times finer\n", g_scale, g_widen, g_shadowDetail);
    }
    // The release of what has not been used: a walk every few seconds.
    if (frame % 300 != 0) return remade;
    for (uint32_t i = 1; i < g_textures.size(); i++)
    {
        TextureEntry& entry = g_textures[i];
        if (!entry.live || frame - entry.usedFrame < UnusedFrames) continue;
        g_textureKeys.Erase(entry.key);
        g_resourceBytes -= entry.bytes;
        ReleasePooled(entry.texture, entry.resource);
        entry.face.Reset();
        entry.live = false;
        g_freeTextures.push_back(i + 1);
        g_released++;
    }
    for (uint32_t i = 0; i < g_buffers.size(); i++)
    {
        BufferEntry& entry = g_buffers[i];
        if (!entry.live || frame - entry.usedFrame < UnusedFrames) continue;
        g_bufferKeys.Erase(entry.key);
        g_resourceBytes -= entry.bytes;
        entry.buffer.Reset();
        entry.resource.Reset();
        entry.live = false;
        g_freeBuffers.push_back(i + 1);
        g_released++;
    }
    for (uint32_t i = 0; i < g_indexEntries.size(); i++)
    {
        IndexEntry& entry = g_indexEntries[i];
        if (!entry.live || frame - entry.usedFrame < UnusedFrames) continue;
        if (entry.buffer) g_released++;
        DropIndexEntry(i);
    }
    if (RenderStats::Wanted() && TexturePoolOn())
    {
        uint64_t spare = 0;
        {
            std::lock_guard<std::mutex> lock(g_poolMutex);
            spare = g_poolBytes;
        }
        printf("render: texture pool: %llu new textures from the pool, %llu made on the spot, %llu written in place, %llu made ahead; %.1f MB kept; "
               "%llu sent by staging (%.1f MB of staging textures), %llu directly\n",
            (unsigned long long)g_poolHits.load(), (unsigned long long)g_poolMisses.load(), (unsigned long long)g_poolInPlace.load(),
            (unsigned long long)g_poolMadeAhead.load(), spare / 1048576.0,
            (unsigned long long)g_stagedUploads.load(), g_stagingBytes / 1048576.0, (unsigned long long)g_directUploads.load());
    }
    return remade;
}

bool RenderResources::Forget(uint32_t address, uint32_t size)
{
    RenderQueue::Sync(5);
    const uint32_t first = Guest::GpuOffset(address), end = first + size;
    auto inside = [&](uint32_t at) { return at >= first && at < end; };
    bool dropped = false;
    for (uint32_t i = 0; i < g_textures.size(); i++)
    {
        TextureEntry& entry = g_textures[i];
        if (!entry.live || !inside(entry.key[0])) continue;
        g_textureKeys.Erase(entry.key);
        g_resourceBytes -= entry.bytes;
        ReleasePooled(entry.texture, entry.resource);
        entry.face.Reset();
        entry.live = false;
        g_freeTextures.push_back(i + 1);
        g_released++;
        dropped = true;
    }
    for (uint32_t i = 0; i < g_buffers.size(); i++)
    {
        BufferEntry& entry = g_buffers[i];
        if (!entry.live || !inside(entry.key[0])) continue;
        g_bufferKeys.Erase(entry.key);
        g_resourceBytes -= entry.bytes;
        entry.buffer.Reset(); entry.resource.Reset();
        entry.live = false;
        g_freeBuffers.push_back(i + 1);
        g_released++;
        dropped = true;
    }
    for (uint32_t i = 0; i < g_indexEntries.size(); i++)
    {
        IndexEntry& entry = g_indexEntries[i];
        if (!entry.live || !inside(entry.key[0])) continue;
        if (entry.buffer) { g_released++; dropped = true; }
        DropIndexEntry(i);
    }
    for (uint32_t i = 0; i < g_resolved.size(); i++)
    {
        ResolvedEntry& entry = g_resolved[i];
        if (!entry.texture || !inside(entry.physical)) continue;
        if (entry.texture) g_resourceBytes -= uint64_t(entry.surface.width) * entry.surface.height * 4;
        entry.texture.Reset(); entry.resource.Reset(); entry.view.Reset();
        entry.DropCrop();
        entry.surface.texture = nullptr; entry.surface.resource = nullptr; entry.surface.view = nullptr;
        entry.surface.width = entry.surface.height = 0;
        g_resolvedVersion++;
        dropped = true;
    }
    return dropped;
}

void RenderResources::RequestEffectsScale(double scale) { g_requestedEffectsScale = scale < 1.0 ? 1.0 : scale > 2.0 ? 2.0 : scale; }

void RenderResources::RequestScale(float scale)
{
    g_requestedScale = scale < 0.25f ? 0.25f : scale > 8.0f ? 8.0f : scale;
}

float RenderResources::Scale() { return g_scale; }

uint32_t RenderResources::TargetSamples(uint32_t pitch) { return SamplesFor(pitch); }

void RenderResources::RequestShadowDetail(uint32_t times)
{
    g_requestedShadowDetail = times < 1 ? 1 : times > 4 ? 4 : times;
}

void RenderResources::RequestWiden(float widen)
{
    g_requestedWiden = widen < 0.5f ? 0.5f : widen > 3.0f ? 3.0f : std::round(widen * 1000.0f) / 1000.0f;
}

void RenderResources::RequestMipSkip(uint32_t levels) { g_requestedMipSkip = levels > 4 ? 4 : levels; }

uint32_t RenderResources::MipSkip() { return g_mipSkip; }

void RenderResources::RequestMultisample(uint32_t samples)
{
    samples = samples >= 8 ? 8 : samples >= 4 ? 4 : samples >= 2 ? 2 : 1;
    g_requestedSamples = SupportedSamples(samples);
}

uint32_t RenderResources::Multisample() { return g_samples; }

void RenderResources::RequestHdrTargets(bool upgraded) { g_requestedHdrTargets = upgraded; }

bool RenderResources::HdrTargets() { return g_hdrTargets; }

void RenderResources::TargetSize(uint32_t pitch, uint32_t bytesPerSample, uint32_t& width, uint32_t& height, float& scaleX, float& scaleY)
{
    const uint32_t rows = TargetRows(pitch, bytesPerSample);
    HostSize(pitch, rows, width, height);
    scaleX = pitch != 0 ? float(width) / float(pitch) : 1.0f;
    scaleY = rows != 0 ? float(height) / float(rows) : 1.0f;
}

RenderState::Handle RenderResources::ColorTargetFor(uint32_t colorInfo, uint32_t pitch)
{
    const uint32_t key[2] = { colorInfo & 0x000F0FFFu, pitch };   // base tile and format
    if (const Handle found = g_colorKeys.Find(key)) return found;

    g_colorTargets.emplace_back();
    ColorEntry& entry = g_colorTargets.back();
    const Handle handle = Handle(g_colorTargets.size());
    uint32_t bytesPerSample = 4;
    const uint32_t format = (colorInfo >> 16) & 0xF;
    entry.target.format = ColorFormat(format, bytesPerSample);
    entry.target.upgraded = g_hdrTargets && (format == 0 || format == 1);
    entry.target.pitch = pitch;
    entry.target.rows = TargetRows(pitch, bytesPerSample);
    HostSize(pitch, entry.target.rows, entry.target.width, entry.target.height);
    memcpy(entry.key, key, sizeof(entry.key));
    g_colorKeys.Insert(key, handle);
    if (pitch == 0 || entry.target.rows == 0) return handle;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = entry.target.width;
    desc.Height = entry.target.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = entry.target.format;
    const uint32_t samples = SamplesFor(pitch);
    desc.SampleDesc.Count = samples;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &entry.texture))) return handle;
    g_device->CreateRenderTargetView(entry.texture.Get(), nullptr, &entry.view);
    g_device->CreateShaderResourceView(entry.texture.Get(), nullptr, &entry.resource);
    entry.target.texture = entry.texture.Get();
    entry.target.view = entry.view.Get();
    entry.target.resource = entry.resource.Get();
    entry.target.samples = samples;
    g_resourceBytes += uint64_t(entry.target.width) * entry.target.height * bytesPerSample * samples;
    printf("render: colour target at tile %u, pitch %u, format %u: %ux%u, %u sample%s%s\n",
        colorInfo & 0xFFF, pitch, format, entry.target.width, entry.target.height, samples, samples == 1 ? "" : "s",
        entry.target.upgraded ? ", 16 bit float (HDR)" : "");
    fflush(stdout);
    return handle;
}

RenderState::Handle RenderResources::DepthTargetFor(uint32_t depthInfo, uint32_t pitch)
{
    const uint32_t key[2] = { depthInfo & 0xFFF, pitch };
    if (const Handle found = g_depthKeys.Find(key)) return found;

    g_depthTargets.emplace_back();
    DepthEntry& entry = g_depthTargets.back();
    const Handle handle = Handle(g_depthTargets.size());
    entry.target.pitch = pitch;
    entry.target.rows = TargetRows(pitch, 4);
    HostSize(pitch, entry.target.rows, entry.target.width, entry.target.height);
    memcpy(entry.key, key, sizeof(entry.key));
    g_depthKeys.Insert(key, handle);
    if (pitch == 0 || entry.target.rows == 0) return handle;

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = entry.target.width;
    desc.Height = entry.target.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
    const uint32_t samples = SamplesFor(pitch);
    desc.SampleDesc.Count = samples;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &entry.texture))) return handle;
    D3D11_DEPTH_STENCIL_VIEW_DESC viewDesc{};
    viewDesc.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
    viewDesc.ViewDimension = samples > 1 ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
    g_device->CreateDepthStencilView(entry.texture.Get(), &viewDesc, &entry.view);
    D3D11_SHADER_RESOURCE_VIEW_DESC resourceDesc{};
    resourceDesc.Format = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
    resourceDesc.ViewDimension = samples > 1 ? D3D11_SRV_DIMENSION_TEXTURE2DMS : D3D11_SRV_DIMENSION_TEXTURE2D;
    resourceDesc.Texture2D.MipLevels = 1;
    g_device->CreateShaderResourceView(entry.texture.Get(), &resourceDesc, &entry.resource);
    entry.target.texture = entry.texture.Get();
    entry.target.view = entry.view.Get();
    entry.target.resource = entry.resource.Get();
    entry.target.samples = samples;
    g_resourceBytes += uint64_t(entry.target.width) * entry.target.height * 8 * samples;
    printf("render: depth target at tile %u, pitch %u: %ux%u, %u sample%s\n", depthInfo & 0xFFF, pitch, entry.target.width, entry.target.height, samples, samples == 1 ? "" : "s");
    fflush(stdout);
    return handle;
}

bool RenderResources::EnsureRows(uint32_t pitch, uint32_t rows)
{
    if (pitch == 0 || pitch / 32 >= sizeof(g_rowsAsked) / sizeof(g_rowsAsked[0])) return false;
    rows = std::min(rows, 1440u);
    if (rows <= g_rowsAsked[pitch / 32] || rows <= TargetRows(pitch, 4)) return false;
    // Whole 64s, so a scissor that grows a row at a time does not make the
    // targets again a row at a time.
    g_rowsAsked[pitch / 32] = (rows + 63) & ~63u;
    // The targets of this pitch go: they come back taller at the next ask.
    RenderQueue::Sync(6);
    bool dropped = false;
    for (uint32_t i = 0; i < g_colorTargets.size(); i++)
    {
        ColorEntry& entry = g_colorTargets[i];
        if (entry.target.pitch != pitch || entry.texture == nullptr) continue;
        g_colorKeys.Erase(entry.key);
        g_resourceBytes -= uint64_t(entry.target.width) * entry.target.height * 4 * entry.target.samples;
        entry.texture.Reset(); entry.view.Reset(); entry.resource.Reset();
        entry.target = ColorTarget();
        dropped = true;
    }
    for (uint32_t i = 0; i < g_depthTargets.size(); i++)
    {
        DepthEntry& entry = g_depthTargets[i];
        if (entry.target.pitch != pitch || entry.texture == nullptr) continue;
        g_depthKeys.Erase(entry.key);
        g_resourceBytes -= uint64_t(entry.target.width) * entry.target.height * 8 * entry.target.samples;
        entry.texture.Reset(); entry.view.Reset(); entry.resource.Reset();
        entry.target = DepthTarget();
        dropped = true;
    }
    printf("render: the %u wide surfaces get %u rows\n", pitch, g_rowsAsked[pitch / 32]);
    fflush(stdout);
    return dropped;
}

const RenderResources::ColorTarget* RenderResources::ColorTargetOf(Handle handle)
{
    if (handle == 0 || handle > g_colorTargets.size()) return nullptr;
    const ColorTarget& target = g_colorTargets[handle - 1].target;
    return target.view != nullptr ? &target : nullptr;
}

const RenderResources::DepthTarget* RenderResources::DepthTargetOf(Handle handle)
{
    if (handle == 0 || handle > g_depthTargets.size()) return nullptr;
    const DepthTarget& target = g_depthTargets[handle - 1].target;
    return target.view != nullptr ? &target : nullptr;
}

RenderResources::Resolved* RenderResources::ResolvedFor(uint32_t physical, uint32_t width, uint32_t height, DXGI_FORMAT format, bool depth)
{
    const uint32_t key[1] = { Guest::GpuOffset(physical) };
    Handle handle = g_resolvedKeys.Find(key);
    if (handle == 0)
    {
        g_resolved.emplace_back();
        handle = Handle(g_resolved.size());
        g_resolved.back().physical = key[0];
        g_resolvedKeys.Insert(key, handle);
        g_resolvedVersion++;
    }
    ResolvedEntry& entry = g_resolved[handle - 1];
    Resolved& out = entry.surface;
    if (!entry.texture || out.width != width || out.height != height || out.format != format)
    {
        g_resolvedVersion++;
        RenderQueue::Sync(7);
        if (entry.texture) g_resourceBytes -= uint64_t(out.width) * out.height * 4;
        entry.texture.Reset(); entry.resource.Reset(); entry.view.Reset();
        entry.DropCrop();
        out.texture = nullptr; out.resource = nullptr; out.view = nullptr;
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &entry.texture))) return nullptr;
        g_device->CreateShaderResourceView(entry.texture.Get(), nullptr, &entry.resource);
        g_device->CreateRenderTargetView(entry.texture.Get(), nullptr, &entry.view);
        out.texture = entry.texture.Get();
        out.resource = entry.resource.Get();
        out.view = entry.view.Get();
        out.width = width;
        out.height = height;
        out.format = format;
        g_resourceBytes += uint64_t(width) * height * 4;
    }
    out.depth = depth;
    out.serial = ++g_resolveSerial;
    return &out;
}

const RenderResources::Resolved* RenderResources::ResolvedAt(uint32_t physical)
{
    const uint32_t key[1] = { Guest::GpuOffset(physical) };
    const Handle handle = g_resolvedKeys.Find(key);
    if (handle == 0) return nullptr;
    const Resolved& out = g_resolved[handle - 1].surface;
    return out.resource != nullptr ? &out : nullptr;
}

ID3D11ShaderResourceView* RenderResources::ResolvedView(uint32_t physical, uint32_t guestWidth, uint32_t guestHeight)
{
    const uint32_t key[1] = { Guest::GpuOffset(physical) };
    const Handle handle = g_resolvedKeys.Find(key);
    if (handle == 0) return nullptr;
    ResolvedEntry& entry = g_resolved[handle - 1];
    const Resolved& surface = entry.surface;
    if (surface.resource == nullptr) return nullptr;
    // The whole surface, unless the fetch reads a smaller corner of it.
    const bool corner = guestWidth <= surface.guestWidth && guestHeight <= surface.guestHeight &&
                        (guestWidth != surface.guestWidth || guestHeight != surface.guestHeight) &&
                        surface.guestWidth != 0 && surface.guestHeight != 0;
    if (!corner) return surface.resource;

    const uint32_t width = std::min(surface.width, std::max(1u, uint32_t(std::lround(double(guestWidth) * surface.width / surface.guestWidth))));
    const uint32_t height = std::min(surface.height, std::max(1u, uint32_t(std::lround(double(guestHeight) * surface.height / surface.guestHeight))));
    if (entry.cropSerial != surface.serial || !entry.crop) RenderQueue::Sync(8);   // 0.5.391: copied on this thread
    if (!entry.crop || entry.cropGuestWidth != guestWidth || entry.cropGuestHeight != guestHeight || entry.cropWidth != width || entry.cropHeight != height)
    {
        RenderQueue::Sync(9);
        entry.DropCrop();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = width;
        desc.Height = height;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = surface.format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &entry.crop))) { entry.crop.Reset(); return surface.resource; }
        if (FAILED(g_device->CreateShaderResourceView(entry.crop.Get(), nullptr, &entry.cropView))) { entry.crop.Reset(); return surface.resource; }
        entry.cropGuestWidth = guestWidth;
        entry.cropGuestHeight = guestHeight;
        entry.cropWidth = width;
        entry.cropHeight = height;
        g_resourceBytes += uint64_t(width) * height * 4;
    }
    // Once per resolve: the draws after it that sample the corner share it.
    if (entry.cropSerial != surface.serial)
    {
        const D3D11_BOX box{ 0, 0, 0, width, height, 1 };
        g_context->CopySubresourceRegion(entry.crop.Get(), 0, 0, 0, 0, surface.texture, 0, &box);
        entry.cropSerial = surface.serial;
    }
    return entry.cropView.Get();
}

RenderState::Handle RenderResources::WhiteTexture() { return 1; }

namespace { const char* g_whiteReason = ""; }
const char* RenderResources::WhiteReason() { return g_whiteReason; }

namespace
{
    // Verify mode (verify.h): the memory a texture was made from, kept.
    void TextureSent(TextureEntry& entry, const uint32_t fetch[6], const uint8_t* data)
    {
        if (!Verify::On(Verify::Textures)) return;
        const size_t span = std::min(SourceBytes(fetch), size_t(64u << 20));
        entry.sourceMirror.assign(data, data + span);
        entry.mirrorEpoch = Verify::epoch.load(std::memory_order_relaxed);
    }

    // Made again at its next look: its fingerprint forgotten.
    void LookAgain(TextureEntry& entry)
    {
        entry.fingerprint = 0;
        entry.dense = true;
        entry.denseFingerprint = 0;
        entry.watchedSince = 0;
        entry.checkedFrame = 0;
    }

    // A texture at its first use in a frame, after that frame's look, held
    // against what it was made from: a difference is a change the look
    // missed. It is made again, and said.
    void VerifyTexture(RenderState::Handle handle, const uint32_t fetch[6])
    {
        if (handle <= 1 || handle > g_textures.size()) return;
        TextureEntry& entry = g_textures[handle - 1];
        if (!entry.live) return;
        if (entry.mirrorEpoch != Verify::epoch.load(std::memory_order_relaxed)) { LookAgain(entry); return; }
        if (entry.verifiedFrame == g_frame) return;
        entry.verifiedFrame = g_frame;
        Verify::Checked(Verify::Textures);
        const uint32_t base = fetch[1] & 0xFFFFF000u;
        const uint8_t* data = Guest::Base + Guest::PhysicalAlias(base);
        const size_t span = std::min(std::min(SourceBytes(fetch), size_t(64u << 20)), entry.sourceMirror.size());
        if (span == 0 || memcmp(data, entry.sourceMirror.data(), span) == 0) return;
        size_t differ = 0;
        for (size_t i = 0; i < span; i++) differ += data[i] != entry.sourceMirror[i];
        Verify::Mismatch(Verify::Textures, "texture %08X, format %u, %ux%u: %zu of %zu bytes changed and its look missed it",
            base, fetch[1] & 0x3F, (fetch[2] & 0x1FFF) + 1, ((fetch[2] >> 13) & 0x1FFF) + 1, differ, span);
        LookAgain(entry);
    }
}

namespace RenderResources
{
    Handle TextureForUnchecked(const uint32_t fetch[6], uint32_t& width, uint32_t& height, bool& resolved);
}

RenderState::Handle RenderResources::TextureFor(const uint32_t fetch[6], uint32_t& width, uint32_t& height, bool& resolved)
{
    const Handle handle = TextureForUnchecked(fetch, width, height, resolved);
    if (Verify::On(Verify::Textures) && !resolved) VerifyTexture(handle, fetch);
    return handle;
}

RenderState::Handle RenderResources::TextureForUnchecked(const uint32_t fetch[6], uint32_t& width, uint32_t& height, bool& resolved)
{
    t_fingerprintKind = "texture";
    const uint32_t format = fetch[1] & 0x3F;
    const uint32_t base = fetch[1] & 0xFFFFF000u;
    width = (fetch[2] & 0x1FFF) + 1;
    height = ((fetch[2] >> 13) & 0x1FFF) + 1;
    resolved = false;
    if ((fetch[0] & 3) != 2 || base == 0 || width > 8192 || height > 8192) { g_whiteReason = "no texture in the fetch constant"; return 1; }
    // 0.5.695: a fetch of a place given a picture (SetTextureImage), or of
    // that place less its top bits (an address cut to 29 bits), said a few times
    if (const uint32_t last = g_placedLast.load(std::memory_order_relaxed))
    {
        static std::atomic<int> said{ 0 };
        const bool placed = last == Guest::GpuOffset(base) || (last & 0x1FFFFFFFu) == (base & 0x1FFFFFFFu);
        if (placed && said.fetch_add(1) < 6)
        {
            printf("render: a fetch of the placed picture's texture %08X: %08X %08X %08X %08X %08X %08X\n", base, fetch[0], fetch[1], fetch[2], fetch[3], fetch[4], fetch[5]);
            fflush(stdout);
        }
    }

    // 0.5.411: a small front cache of the lookups that found a texture.
    // Every texture of every draw asked the resolves' table (ResolvedAt)
    // and then the textures' (g_textureKeys), two hash probes each, some
    // 24000 a frame in Marseille's busy views. A hit here needs the same
    // four-word key as before, the resolves unchanged since it was noted
    // (g_resolvedVersion), and the entry it names still live under that
    // very key; then it is exactly what the two probes would have found.
    // Everything after the lookup (the page watch, the fingerprint, the
    // upload) runs as before. COD3_TEXTURELOOKUPCACHE=0: the probes always.
    static const bool textureFront = []() {
        const char* value = getenv("COD3_TEXTURELOOKUPCACHE");
        return value == nullptr || value[0] != '0';
    }();
    struct TextureFront { uint32_t key[4]; Handle handle; uint64_t resolvedVersion; };
    static constexpr uint32_t TextureFrontSize = 2048;
    static TextureFront textureFrontTable[TextureFrontSize] = {};
    const uint32_t lookDimension = (fetch[5] >> 9) & 3;
    const uint32_t lookKey[4] = { Guest::GpuOffset(base), format | (lookDimension << 8), width, height };
    TextureFront* front = nullptr;
    Handle frontHandle = 0;
    if (textureFront)
    {
        const uint32_t mixed = (lookKey[0] >> 12) ^ (lookKey[0] >> 23) ^ (lookKey[1] * 0x9E3779B9u) ^ (lookKey[2] * 0x85EBCA6Bu) ^ (lookKey[3] * 0xC2B2AE35u);
        front = &textureFrontTable[(mixed ^ (mixed >> 15)) & (TextureFrontSize - 1)];
        if (front->handle != 0 && front->resolvedVersion == g_resolvedVersion && memcmp(front->key, lookKey, sizeof(lookKey)) == 0 &&
            front->handle <= g_textures.size())
        {
            const TextureEntry& cached = g_textures[front->handle - 1];
            if (cached.live && memcmp(cached.key, lookKey, sizeof(lookKey)) == 0) frontHandle = front->handle;
        }
    }

    // A surface a resolve made, at the size the frame was drawn: those are
    // the shadow maps and the post processing's copies of the frame.
    if (const Resolved* surface = frontHandle != 0 ? nullptr : ResolvedAt(base))
    {
        // A fetch that does not look like the resolve: said once each. A
        // shadow map or depth copy read as 24 bit depth, or a copy of the
        // frame read as 8888, at its own size, is what the title does.
        const bool kindMatches = surface->depth ? (format == 22 || format == 23) : format == 6;
        if (!kindMatches || width != surface->guestWidth || height != surface->guestHeight)
        {
            static std::set<uint64_t> said;
            const uint64_t what = (uint64_t(base) << 32) ^ (uint64_t(format) << 26) ^ (uint64_t(width) << 13) ^ height;
            if (said.size() < 200 && said.insert(what).second)
            {
                printf("render: a texture at %08X, format %u, %ux%u, is sampled as the %ux%u %s surface a resolve left there (resolve %llu of %llu)\n",
                    base, format, width, height, surface->guestWidth, surface->guestHeight, surface->depth ? "depth" : "colour",
                    (unsigned long long)surface->serial, (unsigned long long)g_resolveSerial);
                fflush(stdout);
            }
        }
        // A fetch inside the resolve keeps its own size: ResolvedView gives
        // it that corner, and its texel offsets and unnormalised
        // coordinates go by its own width and height, as on the console.
        if (width > surface->guestWidth || height > surface->guestHeight)
        {
            width = surface->guestWidth;
            height = surface->guestHeight;
        }
        resolved = true;
        // Handle 0 with `resolved` set: the caller binds the surface itself.
        return 0;
    }
    const bool uploadable = format == 2 || format == 3 || format == 4 || format == 6 || format == 10 || format == 15 || format == 18 || format == 19 || format == 20;
    const uint32_t dimension = (fetch[5] >> 9) & 3;
    // What is not uploaded is said once each, so a level that needs a
    // format this does not have is a level that says so (said before the
    // way out, which until 0.5.36 came first and kept it quiet).
    static uint64_t announced = 0;
    if ((dimension == 2 || !uploadable) && (announced & (1ull << (format & 63))) == 0)
    {
        announced |= 1ull << (format & 63);
        printf("render: texture format %u%s at %08X (%ux%u) is not uploaded; it reads white\n", format, dimension == 2 ? " (a volume)" : dimension == 3 ? " (a cube)" : "", base, width, height);
        fflush(stdout);
    }
    if (!uploadable) { g_whiteReason = "format not uploaded"; return 1; }
    if (dimension == 2) return 1;   // a volume: not uploaded, white
    const uint32_t key[4] = { Guest::GpuOffset(base), format | (dimension << 8), width, height };
    Handle handle = frontHandle != 0 ? frontHandle : g_textureKeys.Find(key);
    if (front != nullptr && handle != 0 && handle != frontHandle)
    {
        memcpy(front->key, key, sizeof(key));
        front->handle = handle;
        front->resolvedVersion = g_resolvedVersion;
    }
    const uint8_t* data = Guest::Base + Guest::PhysicalAlias(base);
    if (handle != 0)
    {
        TextureEntry& entry = g_textures[handle - 1];
        // 0.5.234: one map's own replacement (textures\replace\maps\<map>)
        // taken up again once another map is loading: the DLC maps' shared
        // loading picture can sit at the same place with the same texels
        // from one map to the next, and would keep the first map's copy.
        // 0.5.432: and a texture made with no replacement (the loading picture
        // first drawn while no map was known yet: right after choosing a map
        // in a lobby) taken up again once a map that has its own copy of it
        // is known. Justin: the photo tour's DLC loading pictures no longer
        // showed; only the benchmark, which loads the map first, still got one.
        const bool otherMap = !entry.replacedFor.empty() && entry.replacedFor != g_mapKey;
        bool mapPicture = false;
        if (entry.replacedFor.empty() && entry.mapGeneration != g_mapGeneration)
        {
            mapPicture = TexturePack::HasMapReplacement(entry.packKey, g_mapKey);
            if (!mapPicture) entry.mapGeneration = g_mapGeneration;   // nothing of this map's for it
        }
        if (otherMap || mapPicture)
        {
            entry.checkedFrame = entry.usedFrame = g_frame;
            entry.stable = 0;
            entry.watchedSince = 0;
            entry.dense = true;
            entry.fingerprint = Fingerprint(data, std::min(SourceBytes(fetch), size_t(64u << 20)), true);
            g_resourceBytes -= entry.bytes;
            if (!UploadTexture(entry, fetch, data)) { g_textureKeys.Erase(entry.key); entry.live = false; g_freeTextures.push_back(handle); g_whiteReason = "upload failed"; return 1; }
            g_resourceBytes += entry.bytes;
            return handle;
        }
        if (entry.checkedFrame == g_frame) return handle;
        const bool look = LookThisFrame(entry.stable, entry.usedFrame, handle);
        entry.usedFrame = g_frame;
        if (!look) return handle;
        entry.checkedFrame = g_frame;
        const size_t span = std::min(SourceBytes(fetch), size_t(64u << 20));
        // COD3_WRITEWATCH=1: as for a vertex buffer, a texture still for
        // long enough has its pages watched, and none written since is the
        // same texture without a byte of it read. The small ones (the HUD's,
        // hashed whole every frame) are most of what the looks cost.
        bool written = false;
        if (entry.watchedSince != 0)
        {
            if (WriteWatch::Clean(base, uint32_t(span), entry.watchedSince))
            {
                RenderStats::Frame().watchedLooks++;
                WatchAudit("texture", base, data, span, entry.dense, entry.fingerprint);
                return handle;
            }
            entry.watchedSince = 0;
            written = true;
        }
        // Compared in the kind the stored fingerprint is; the kind changes
        // only once the memory is known to be the same, so no change is
        // lost in the switch. But a big still texture whose pages were
        // written is compared whole: sixty four samples of it are what a
        // streamed texture put in the same place passes unseen, the old
        // picture staying on screen, or coming and going as the samples
        // catch it and miss it again.
        const bool wholeLook = written && !entry.dense && entry.denseFingerprint != 0;
        const uint64_t fingerprint = Fingerprint(data, span, entry.dense || wholeLook);
        if (wholeLook ? fingerprint == entry.denseFingerprint : fingerprint == entry.fingerprint)
        {
            MissAudit("texture", fetch[1] & 0xFFFFF000u, data, span, entry.dense, entry.auditDense);
            if (entry.stable < StableLooks) entry.stable++;
            const bool wantDense = WantDense(entry.stable, span);
            if (wantDense != entry.dense) { if (!wantDense) { entry.auditDense = entry.fingerprint; entry.denseFingerprint = entry.fingerprint; } entry.dense = wantDense; entry.fingerprint = Fingerprint(data, span, wantDense); }
            if (WriteWatch::Enabled() && entry.stable >= StableLooks)
            {
                const uint64_t since = WriteWatch::Sequence();
                if (WriteWatch::Arm(base, uint32_t(span)) && Fingerprint(data, span, entry.dense) == entry.fingerprint)
                    entry.watchedSince = since;
            }
            return handle;
        }
        entry.stable = 0;
        entry.fingerprint = (entry.dense || wholeLook) ? fingerprint : Fingerprint(data, span, true);
        entry.dense = true;
        entry.denseFingerprint = 0;
        g_resourceBytes -= entry.bytes;
        if (!UploadTexture(entry, fetch, data)) { g_textureKeys.Erase(entry.key); entry.live = false; g_freeTextures.push_back(handle); g_whiteReason = "upload failed"; return 1; }
        TextureSent(entry, fetch, data);
        g_resourceBytes += entry.bytes;
        return handle;
    }

    if (!g_freeTextures.empty()) { handle = g_freeTextures.back(); g_freeTextures.pop_back(); }
    else { g_textures.emplace_back(); handle = Handle(g_textures.size()); }
    TextureEntry& entry = g_textures[handle - 1];
    entry = TextureEntry();
    memcpy(entry.key, key, sizeof(entry.key));
    entry.dense = true;
    entry.fingerprint = Fingerprint(data, std::min(SourceBytes(fetch), size_t(64u << 20)), entry.dense);
    entry.checkedFrame = entry.usedFrame = g_frame;
    if (!UploadTexture(entry, fetch, data)) { g_freeTextures.push_back(handle); g_whiteReason = "upload failed"; return 1; }
    TextureSent(entry, fetch, data);
    entry.live = true;
    g_textureKeys.Insert(key, handle);
    g_resourceBytes += entry.bytes;
    return handle;
}

ID3D11ShaderResourceView* RenderResources::TextureView(Handle handle, uint32_t dimension)
{
    if (handle == 0 || handle > g_textures.size()) return nullptr;
    const TextureEntry& entry = g_textures[handle - 1];
    if (dimension == 3) return entry.cube ? entry.resource.Get() : g_whiteCubeView.Get();
    // A volume fetch reads a flat texture (xenos_hlsl.cpp says why).
    return entry.cube ? entry.face.Get() : entry.resource.Get();
}

RenderState::Handle RenderResources::VertexBufferFor(uint32_t physical, uint32_t bytes, uint32_t endian)
{
    t_fingerprintKind = "vertex";
    if (bytes == 0 || bytes > (128u << 20)) return 0;
    // A look that takes long, named (with COD3_RENDER_STATS): the frames of
    // 0.5.12 that spent 30 to 40 ms in the buffers were few and unexplained.
    // Timed only when the stats are on: two clock reads a look, a few
    // thousand looks a frame, are not free.
    const char* path = "look";
    const bool lookTimed = RenderStats::Wanted();
    struct SlowLook
    {
        bool timed;
        std::chrono::steady_clock::time_point started;
        const char*& path; uint32_t physical; uint32_t& bytes;
        ~SlowLook()
        {
            if (!timed) return;
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            static int told = 0;
            if (ms > 4.0 && told < 40)
            {
                told++;
                printf("render: a vertex buffer look took %.2f ms: %08X, %u bytes, %s\n", ms, physical, bytes, path);
            }
        }
    } slowLook{ lookTimed, lookTimed ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{}, path, physical, bytes };
    const uint32_t key[2] = { Guest::GpuOffset(physical), endian };
    Handle handle = g_bufferKeys.Find(key);
    const uint8_t* data = Guest::Base + Guest::PhysicalAlias(physical);
    bytes = (bytes + 3) & ~3u;
    uint32_t wanted = bytes;
    const uint64_t call = ++g_vertexCalls;

    if (handle != 0)
    {
        BufferEntry& entry = g_buffers[handle - 1];
        // Verify mode, just switched on: sent whole once, so what each draw
        // is held against is known to be what the GPU has.
        if (Verify::On(Verify::Buffers) && entry.mirrorEpoch != Verify::epoch.load(std::memory_order_relaxed)
            && entry.buffer && entry.seen != 0 && entry.bytes >= entry.seen)
        {
            D3D11_BOX box{ 0, 0, 0, entry.seen, 1, 1 };
            RenderQueue::Upload(entry.buffer.Get(), 0, &box, data, box.right - box.left);
            if (entry.shadow.size() >= entry.seen) memcpy(entry.shadow.data(), data, entry.seen);
            entry.fingerprint = Fingerprint(data, entry.seen, entry.dense);
            Sent(entry, 0, entry.seen, data, "sent whole as verify mode began");
        }
        // Already looked at this frame. A buffer is looked at once a frame,
        // like a texture, but the title rewrites some of its small ones
        // between the draws of a frame: a tank's treads are strips built
        // each frame into 16 KB buffers, one tread after another into the
        // same memory once the GPU has drawn the one before (the console's
        // GPU reads the memory at the draw), and a few 128 KB buffers go the
        // same way. Looked at once a frame, the second tread was drawn with
        // the first one's vertices, and its strip, longer by a few, ran on
        // into whatever lay beyond them: a dark textured triangle across the
        // ground for a frame, on every level with tanks. So every later draw
        // of a frame takes a sampled look at a small buffer (sixty four
        // words), and one found changing is looked at whole by every draw
        // from then on. COD3_NOMIDFRAME=1 goes back to once a frame.
        if (entry.checkedFrame == g_frame && entry.seen >= bytes)
        {
            if (entry.watchedSince != 0 && WriteWatch::Clean(physical, entry.seen, entry.watchedSince)) return handle;
            static const bool once = getenv("COD3_NOMIDFRAME") != nullptr;
            // COD3_VBAUDIT=1 (with COD3_DENSEALL=1): every buffer, whatever
            // its size, looked at by every draw, and the ones found changing
            // within a frame named with their size.
            static const bool audit = getenv("COD3_VBAUDIT") != nullptr;
            // 0.5.99: the bullet holes' flicker. A buffer over a quarter
            // megabyte was looked at only by the frame's first draw. The
            // title writes its dynamic geometry (the marks among it) into a
            // big pool a piece at a time through the frame, each piece just
            // before the draw that uses it, so a later draw of the frame got
            // what that place held a frame or more before: a mark drawn with
            // last frame's vertices there (another mark, or nothing), in and
            // out until the pool's layout settled. Now such a buffer's pages
            // are watched from the frame's first look (the page protection
            // of COD3_WRITEWATCH), and each later draw sends up just the
            // pieces written since: no byte compared, only what changed sent.
            // COD3_NOLIVEBUFFERS=1: as before.
            static const bool noLive = getenv("COD3_NOLIVEBUFFERS") != nullptr;
            const uint64_t since = entry.liveSince != 0 ? entry.liveSince : entry.watchedSince;
            if (!once && !noLive && since != 0 && entry.seen > AlwaysDenseBytes && entry.buffer)
            {
                path = "written within the frame (pieces)";
                const uint32_t span = std::min(entry.seen, entry.bytes);
                if (WriteWatch::Clean(physical, span, since)) return handle;
                const uint64_t now = WriteWatch::Sequence();
                // Which pieces were written is found first, then the pages
                // are watched again, and only then are the pieces read and
                // sent. They were read first and watched after: a page
                // already written once is no longer protected, so the title
                // carrying on writing it (the next mark, the next effect,
                // into the same page) between the read and the watch went
                // unseen, and a later draw took the piece as it was half
                // written: its vertices flung across the screen as lines.
                // It took the command thread running alongside the title's
                // writes, rather than a frame behind them, to show.
                static thread_local std::vector<std::pair<uint32_t, uint32_t>> runs;
                runs.clear();
                uint32_t runStart = 0xFFFFFFFFu;
                for (uint32_t at = 0; at < span; at += BufferPieceBytes)
                {
                    const uint32_t length = std::min(BufferPieceBytes, span - at);
                    const bool written = !WriteWatch::Clean(physical + at, length, since);
                    if (written && runStart == 0xFFFFFFFFu) runStart = at;
                    const bool last = at + length >= span;
                    if (runStart != 0xFFFFFFFFu && (!written || last))
                    {
                        runs.emplace_back(runStart, written ? at + length : at);
                        runStart = 0xFFFFFFFFu;
                    }
                }
                entry.watchedSince = 0;
                entry.stable = 0;
                entry.liveSince = WriteWatch::Arm(physical, span) ? now : 0;
                uint64_t sent = 0;
                for (const auto& [first, end] : runs)
                {
                    D3D11_BOX box{ first, 0, 0, end, 1, 1 };
                    RenderQueue::Upload(entry.buffer.Get(), 0, &box, data + first, box.right - box.left);
                    Sent(entry, first, end, data, "written within the frame (pieces)");
                    if (entry.shadow.size() >= end) memcpy(entry.shadow.data() + first, data + first, end - first);
                    sent += end - first;
                }
                if (entry.dense) entry.fingerprint = 0;   // the next frame's look sends it whole if it is not the same
                static int told = 0;
                if ((told < 8 && told++ < 8))
                    printf("render: the vertex buffer at %08X (%u bytes) was written within a frame: %llu bytes of it sent again for a later draw\n",
                        physical, span, (unsigned long long)sent);
                if (sent != 0)
                {
                    g_bufferUploads.fetch_add(1, std::memory_order_relaxed);
                    g_bufferBytes.fetch_add(sent, std::memory_order_relaxed);
                    RenderStats::Frame().bufferUploads++;
                    RenderStats::Frame().uploadBytes += sent;
                }
                return handle;
            }
            if (once || (entry.seen > AlwaysDenseBytes && !audit) || !entry.dense) return handle;
            // Watched since the frame's first look (a still buffer of 32 KB
            // up, below): none of its pages written since, the same; one
            // written is looked at whole.
            const bool liveWritten = entry.liveSince != 0 && entry.seen <= AlwaysDenseBytes;
            if (liveWritten && WriteWatch::Clean(physical, entry.seen, entry.liveSince)) return handle;
            if (audit && entry.seen > AlwaysDenseBytes && !entry.midFrame)
            {
                if (Fingerprint(data, entry.seen, true) == entry.fingerprint) return handle;
                entry.midFrame = true;
                printf("render: audit: the vertex buffer at %08X (%u bytes) changed within a frame\n", physical, entry.seen);
            }
            if (entry.midFrame)
            {
                // 0.5.374: a small buffer the title rewrites within the
                // frame (two of 128 KB on every map, the effects' and the
                // treads' vertices) was hashed whole by every draw that
                // used it: on Marseille most of the 16 MB the command
                // thread hashed a frame. Now a copy of what went up is kept
                // and each draw compares only the vertices it reads
                // (LookRanges: its indices' lowest to highest), sending the
                // pieces that differ; a draw whose vertices can't be told
                // compares the whole buffer. COD3_MIDFRAMEHASH=1: as before.
                if (MidFrameRanges() && entry.seen <= AlwaysDenseBytes && entry.buffer && entry.bytes >= entry.seen)
                {
                    if (entry.rangeShadow && entry.shadow.size() >= entry.seen) return handle;
                    entry.shadow.assign(data, data + entry.seen);
                    D3D11_BOX box{ 0, 0, 0, entry.seen, 1, 1 };
                    RenderQueue::Upload(entry.buffer.Get(), 0, &box, data, box.right - box.left);
                    Sent(entry, 0, entry.seen, data, "mid-frame, copy begun");
                    entry.rangeShadow = true;
                    entry.fingerprint = 0;   // looked at by its copy from now on
                    entry.changedFrame = g_frame;
                    g_bufferUploads.fetch_add(1, std::memory_order_relaxed);
                    g_bufferBytes.fetch_add(entry.seen, std::memory_order_relaxed);
                    RenderStats::Frame().bufferUploads++;
                    RenderStats::Frame().uploadBytes += entry.seen;
                    static int told = 0;
                    if (told < 8 && told++ < 8)
                        printf("render: the vertex buffer at %08X (%u bytes) changes within a frame: compared by each draw's part from now on\n", physical, entry.seen);
                    return handle;
                }
                if (Fingerprint(data, entry.seen, true) == entry.fingerprint) return handle;
            }
            else
            {
                // A buffer that changed in the last few looks (a decal or
                // bullet mark pool gaining a mark, a model rebuilt) is looked
                // at whole by the frame's later draws, not by sixty four
                // samples: a mark is a few vertices among thousands, which
                // the samples pass over, and the draw then took the pool as
                // it was before the mark, the mark coming and going from one
                // frame to the next. One still for a while keeps the sampled
                // look. COD3_SAMPLEDMIDFRAME=1: sampled for every buffer.
                static const bool sampledOnly = getenv("COD3_SAMPLEDMIDFRAME") != nullptr;
                if (!sampledOnly && (entry.stable < StableLooks || liveWritten))
                {
                    if (Fingerprint(data, entry.seen, true) == entry.fingerprint) return handle;
                }
                else if (Fingerprint(data, entry.seen, false) == entry.sampled) return handle;
                entry.midFrame = true;
                static int announced = 0;
                if ((announced < 16 && announced++ < 16))
                    printf("render: the vertex buffer at %08X (%u bytes) changes within a frame; every draw looks at it now\n",
                        physical, entry.seen);
            }
            entry.checkedFrame = 0;   // changed: taken up again below
        }
        const bool look = LookThisFrame(entry.stable, entry.usedFrame, handle) || entry.seen < bytes;
        entry.usedFrame = g_frame;
        if (!look) return handle;
        entry.checkedFrame = g_frame;
        entry.lookedCall = call;
        wanted = std::max(bytes, entry.seen);
        // 0.5.374: a mid-frame buffer with its copy kept: compared with the
        // copy, only what differs sent (the hash and the whole send of
        // before not needed).
        if (entry.midFrame && entry.rangeShadow && MidFrameRanges() && wanted <= entry.seen && entry.shadow.size() >= wanted
            && entry.bytes >= wanted && entry.buffer)
        {
            RenderResources::LookRange(handle, physical, 0, wanted);
            return handle;
        }
        // Its pages watched from its last look and none written since: the
        // same as it was sent, without a byte of it hashed. Most of what
        // the command thread hashed was this: a few hundred buffers of the
        // level's models, every byte every frame, ten megabytes a frame,
        // unchanged since they were loaded. COD3_HASHLIVE=1: hashed again.
        static const bool noLiveLooks = getenv("COD3_HASHLIVE") != nullptr || getenv("COD3_NOLIVEBUFFERS") != nullptr;
        const bool wasWatched = entry.liveSince != 0;
        const uint64_t previousLive = entry.liveSince;   // 0.5.378: for the big compare below
        if (!noLiveLooks && wasWatched && wanted <= entry.seen && entry.bytes >= wanted && entry.buffer
            && WriteWatch::Clean(physical, wanted, entry.liveSince))
        {
            RenderStats::Frame().watchedLooks++;
            if (entry.stable < StableLooks) entry.stable++;
            return handle;
        }
        // 0.5.99: a big buffer's pages watched from before this look, so a
        // write later in the frame is seen by the draws after it (above).
        // And a still one of 32 KB up, so its next look can go by its pages
        // (unless its pages keep saying written when it is not: memory it
        // shares with something the title writes every frame).
        entry.liveSince = 0;
        // 0.5.377: from 4 KB up (was 32 KB). Marseille's busy views (the
        // fingerprint audit, 0.5.376): 7500 still vertex buffers of 23 to
        // 32 KB, the soldiers' and props' models, hashed whole every frame,
        // 300 to 400 MB a second, just under the old size. Their pages say
        // instead now. COD3_WATCHSTILL_KB=32: as before.
        // 0.5.381: the 0.5.380 Marseille audit still found hundreds of MB
        // per five seconds in 3-4 KB models, below the 4 KB cutoff. Watch
        // stable models from 1 KB too; COD3_WATCHSTILL_KB=4 restores 380.
        static const uint32_t watchStillBytes = []() {
            const char* t = getenv("COD3_WATCHSTILL_KB");
            const long kb = t != nullptr ? strtol(t, nullptr, 10) : 1;
            return uint32_t(std::max(1L, kb)) << 10;
        }();
        const bool watchStill = !noLiveLooks && wanted >= watchStillBytes && entry.stable >= StableLooks && !entry.midFrame && entry.falseWrites < 4;
        if (WriteWatch::Enabled() && (wanted > AlwaysDenseBytes || watchStill) && entry.bytes >= wanted && getenv("COD3_NOLIVEBUFFERS") == nullptr)
        {
            const uint64_t since = WriteWatch::Sequence();
            if (WriteWatch::Arm(physical, wanted)) entry.liveSince = since;
        }
        static const bool noDiff = getenv("COD3_NOBUFFERDIFF") != nullptr;
        if (!noDiff && wanted >= BigBufferBytes && entry.bytes >= wanted)
        {
            // Watched since it was last found the same (COD3_WRITEWATCH,
            // "Watch memory writes"), and none of its pages written since:
            // the same still, without comparing a byte. A big buffer
            // compared whole each time it was looked at was a tenth of the
            // command processor's time on a big map.
            if (entry.watchedSince != 0 && entry.shadow.size() >= wanted && wanted <= entry.seen
                && WriteWatch::Clean(physical, wanted, entry.watchedSince))
            {
                RenderStats::Frame().watchedLooks++;
                return handle;
            }
            entry.watchedSince = 0;
            path = "big, compared by pieces";
            uint64_t sent = 0;
            if (entry.shadow.size() < wanted)
            {
                path = "big, first sent whole";
                entry.shadow.assign(data, data + wanted);
                D3D11_BOX box{ 0, 0, 0, wanted, 1, 1 };
                RenderQueue::Upload(entry.buffer.Get(), 0, &box, data, box.right - box.left);
                Sent(entry, 0, wanted, data, "big, first sent whole");
                sent = wanted;
            }
            else
            {
                uint8_t* shadow = entry.shadow.data();
                uint32_t runStart = 0xFFFFFFFFu;
                // 0.5.378: watched since its last look (its pages armed
                // then, and every piece written since sent and kept in the
                // copy by the frame's later draws): a piece none of whose
                // pages was written since is the same as its copy, and is
                // not compared. The twelve megabyte pool of the title's
                // dynamic geometry was compared whole every frame, a few
                // pieces of it written: most of the command thread's
                // memcmp on Marseille. COD3_BIGCOMPAREALL=1: all compared.
                static const bool compareAll = getenv("COD3_BIGCOMPAREALL") != nullptr;
                const uint64_t pagesSince = compareAll ? 0 : previousLive;
                for (uint32_t at = 0; at < wanted; at += BufferPieceBytes)
                {
                    const uint32_t length = std::min(BufferPieceBytes, wanted - at);
                    const bool changed = !(pagesSince != 0 && WriteWatch::Clean(physical + at, length, pagesSince))
                        && memcmp(data + at, shadow + at, length) != 0;
                    if (changed)
                    {
                        if (runStart == 0xFFFFFFFFu) runStart = at;
                        memcpy(shadow + at, data + at, length);
                    }
                    const bool last = at + length >= wanted;
                    if (runStart != 0xFFFFFFFFu && (!changed || last))
                    {
                        const uint32_t end = changed ? at + length : at;
                        D3D11_BOX box{ runStart, 0, 0, end, 1, 1 };
                        RenderQueue::Upload(entry.buffer.Get(), 0, &box, data + runStart, box.right - box.left);
                        Sent(entry, runStart, end, data, "big, compared by pieces");
                        sent += end - runStart;
                        runStart = 0xFFFFFFFFu;
                    }
                }
            }
            entry.seen = wanted;
            if (sent != 0)
            {
                entry.stable = 0;
                entry.changedFrame = g_frame;
                g_bufferUploads.fetch_add(1, std::memory_order_relaxed);
                g_bufferBytes.fetch_add(sent, std::memory_order_relaxed);
                RenderStats::Frame().bufferUploads++;
                RenderStats::Frame().uploadBytes += sent;
            }
            else
            {
                if (entry.stable < StableLooks) entry.stable++;
                // Still for long enough: its pages watched from now on, and
                // compared once more after they are, for a write in between.
                if (WriteWatch::Enabled() && entry.stable >= StableLooks && !entry.midFrame)
                {
                    const uint64_t since = WriteWatch::Sequence();
                    if (WriteWatch::Arm(physical, wanted) && memcmp(data, entry.shadow.data(), wanted) == 0)
                        entry.watchedSince = since;
                }
            }
            return handle;
        }
        // COD3_WRITEWATCH=1: a buffer watched since it was last found the
        // same, and none of its pages written since, is the same still.
        if (entry.watchedSince != 0)
        {
            if (wanted <= entry.seen && WriteWatch::Clean(physical, wanted, entry.watchedSince))
            {
                RenderStats::Frame().watchedLooks++;
                WatchAudit("vertex buffer", physical, data, wanted, entry.dense, entry.fingerprint);
                return handle;
            }
            entry.watchedSince = 0;
        }
        // As for a texture: compared in the stored kind, the kind changed
        // only when the memory is known to be the same.
        const uint64_t fingerprint = Fingerprint(data, wanted, entry.dense);
        if (fingerprint == entry.fingerprint && entry.bytes >= wanted)
        {
            if (wasWatched && entry.falseWrites < 255) entry.falseWrites++;
            MissAudit("vertex buffer", physical, data, wanted, entry.dense, entry.auditDense);
            if (entry.stable < StableLooks) entry.stable++;
            // 0.5.94: a vertex or index buffer under the big-buffer size is
            // always hashed whole and never left to the page watch. The
            // bullet holes: the game keeps every mark's few vertices in one
            // pool of a few hundred kilobytes, and a new mark is a handful of
            // them. Once the pool had been still eight looks it was sampled
            // (sixty four words) or left to its pages, and a new mark's
            // vertices were seen only when a sample happened on them: marks
            // showed late, and flickered between the old pool and the new.
            const bool wantDense = WantDense(entry.stable, wanted) || wanted < BigBufferBytes;
            if (wantDense != entry.dense) { if (!wantDense) entry.auditDense = entry.fingerprint; entry.dense = wantDense; entry.fingerprint = Fingerprint(data, wanted, wantDense); }
            if (!entry.midFrame && wanted <= AlwaysDenseBytes) entry.sampled = Fingerprint(data, wanted, false);
            // Still for long enough: its pages watched from now on, and
            // looked at once more after they are, for a write in between.
            if (WriteWatch::Enabled() && entry.stable >= StableLooks && !entry.midFrame && wanted > AlwaysDenseBytes && wanted >= BigBufferBytes)
            {
                const uint64_t since = WriteWatch::Sequence();
                if (WriteWatch::Arm(physical, wanted) && Fingerprint(data, wanted, entry.dense) == entry.fingerprint)
                    entry.watchedSince = since;
            }
            return handle;
        }
        entry.stable = 0;
        entry.changedFrame = g_frame;
        entry.fingerprint = entry.dense ? fingerprint : Fingerprint(data, wanted, true);
        entry.dense = true;
        entry.seen = wanted;
        path = "changed, sent whole";
        if (entry.bytes < wanted)
        {
            path = "grown, made again";
            RenderQueue::Sync(10);
            // Grown: made again below.
            g_resourceBytes -= entry.bytes;
            g_bufferKeys.Erase(entry.key);
            entry.buffer.Reset();
            entry.resource.Reset();
            entry.live = false;
            g_freeBuffers.push_back(handle);
            handle = 0;
        }
        else
        {
            D3D11_BOX box{ 0, 0, 0, wanted, 1, 1 };
            RenderQueue::Upload(entry.buffer.Get(), 0, &box, data, box.right - box.left);
            Sent(entry, 0, wanted, data, "changed, sent whole");
            if (entry.rangeShadow) entry.shadow.assign(data, data + wanted);
            if (!entry.midFrame && wanted <= AlwaysDenseBytes) entry.sampled = Fingerprint(data, wanted, false);
            g_bufferUploads.fetch_add(1, std::memory_order_relaxed);
            g_bufferBytes.fetch_add(wanted, std::memory_order_relaxed);
            RenderStats::Frame().bufferUploads++;
            RenderStats::Frame().uploadBytes += wanted;
            return handle;
        }
    }

    if (path[0] != 'g') path = "new";
    if (!g_freeBuffers.empty()) { handle = g_freeBuffers.back(); g_freeBuffers.pop_back(); }
    else { g_buffers.emplace_back(); handle = Handle(g_buffers.size()); }
    BufferEntry& entry = g_buffers[handle - 1];
    entry = BufferEntry();
    memcpy(entry.key, key, sizeof(entry.key));
    entry.fingerprint = Fingerprint(data, wanted, entry.dense);
    if (wanted <= AlwaysDenseBytes) entry.sampled = Fingerprint(data, wanted, false);
    entry.checkedFrame = entry.usedFrame = g_frame;
    entry.changedFrame = g_frame;
    entry.lookedCall = call;
    entry.seen = wanted;
    // A little room to grow: the title's dynamic buffers are asked for at
    // whatever size the frame needs, and a buffer made again every time
    // that grows a little is a buffer made again every frame.
    entry.bytes = (wanted + wanted / 8 + 4095) & ~4095u;
    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = entry.bytes;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
    if (FAILED(g_device->CreateBuffer(&desc, nullptr, &entry.buffer))) { g_freeBuffers.push_back(handle); return 0; }
    D3D11_SHADER_RESOURCE_VIEW_DESC view{};
    view.Format = DXGI_FORMAT_R32_TYPELESS;
    view.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    view.BufferEx.FirstElement = 0;
    view.BufferEx.NumElements = entry.bytes / 4;
    view.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
    if (FAILED(g_device->CreateShaderResourceView(entry.buffer.Get(), &view, &entry.resource))) { entry.buffer.Reset(); g_freeBuffers.push_back(handle); return 0; }
    D3D11_BOX box{ 0, 0, 0, wanted, 1, 1 };
    RenderQueue::Upload(entry.buffer.Get(), 0, &box, data, box.right - box.left);
    Sent(entry, 0, wanted, data, "new");
    entry.live = true;
    g_bufferKeys.Insert(key, handle);
    g_resourceBytes += entry.bytes;
    g_bufferUploads.fetch_add(1, std::memory_order_relaxed);
    g_bufferBytes.fetch_add(wanted, std::memory_order_relaxed);
    RenderStats::Frame().bufferUploads++;
    RenderStats::Frame().uploadBytes += wanted;
    return handle;
}

bool RenderResources::MidFrameRangeLook(Handle handle)
{
    if (handle == 0 || handle > g_buffers.size() || !MidFrameRanges()) return false;
    const BufferEntry& entry = g_buffers[handle - 1];
    return entry.midFrame && entry.rangeShadow && entry.buffer && entry.checkedFrame == g_frame && entry.seen <= AlwaysDenseBytes
        && entry.shadow.size() >= entry.seen;
}

bool RenderResources::NeedsRangeLook(Handle handle)
{
    // 0.5.374: a mid-frame buffer kept by its copy: always (above).
    if (MidFrameRangeLook(handle)) return true;
    // COD3_NORANGELOOKS=1: once a frame, as before.
    static const bool off = getenv("COD3_NORANGELOOKS") != nullptr;
    if (off || !Tuning::On(Tuning::rangeLooks) || handle == 0 || handle > g_buffers.size()) return false;
    const BufferEntry& entry = g_buffers[handle - 1];
    // Watched (the pages say what was written within the frame): left to
    // the watch, unless its pages could not be watched.
    if (WriteWatch::Enabled() && (entry.liveSince != 0 || entry.watchedSince != 0)) return false;
    // Changing in the last few seconds: the title's pools of dynamic
    // geometry change most frames; the level's own models stop changing
    // once they are loaded, and are not looked at again.
    return entry.live && entry.buffer && entry.seen > AlwaysDenseBytes && entry.checkedFrame == g_frame
        && entry.lookedCall != g_vertexCalls && entry.changedFrame != 0 && g_frame - entry.changedFrame <= 600;
}

void RenderResources::LookRange(Handle handle, uint32_t physical, uint32_t firstByte, uint32_t endByte)
{
    if (handle == 0 || handle > g_buffers.size()) return;
    BufferEntry& entry = g_buffers[handle - 1];
    endByte = std::min(endByte, std::min(entry.seen, entry.bytes));
    if (!entry.buffer || firstByte >= endByte) return;
    const uint8_t* data = Guest::Base + Guest::PhysicalAlias(physical);
    uint64_t sent = 0;
    if (entry.shadow.size() >= endByte)
    {
        // A page at a time against what went up: only what differs goes.
        constexpr uint32_t Piece = 4096;
        uint8_t* shadow = entry.shadow.data();
        uint32_t runStart = 0xFFFFFFFFu;
        for (uint32_t at = firstByte & ~(Piece - 1); at < endByte; at += Piece)
        {
            const uint32_t length = std::min(Piece, endByte - at);
            const bool changed = memcmp(data + at, shadow + at, length) != 0;
            if (changed)
            {
                if (runStart == 0xFFFFFFFFu) runStart = at;
                memcpy(shadow + at, data + at, length);
            }
            const bool last = at + length >= endByte;
            if (runStart != 0xFFFFFFFFu && (!changed || last))
            {
                const uint32_t end = changed ? at + length : at;
                D3D11_BOX box{ runStart, 0, 0, end, 1, 1 };
                RenderQueue::Upload(entry.buffer.Get(), 0, &box, data + runStart, box.right - box.left);
                Sent(entry, runStart, end, data, "range looked at again by a draw");
                sent += end - runStart;
                runStart = 0xFFFFFFFFu;
            }
        }
    }
    else
    {
        // No copy kept (under a megabyte): the part is sent as it is, and
        // the next frame's look, finding the memory changed, sends it whole.
        D3D11_BOX box{ firstByte, 0, 0, endByte, 1, 1 };
        RenderQueue::Upload(entry.buffer.Get(), 0, &box, data + firstByte, box.right - box.left);
        Sent(entry, firstByte, endByte, data, "range sent again by a draw");
        sent = endByte - firstByte;
        // Not a change found, so not counted as one: whether it keeps
        // being looked at is the frame's first look's to say.
    }
    if (sent != 0)
    {
        if (entry.shadow.size() >= endByte) entry.changedFrame = g_frame;
        g_bufferUploads.fetch_add(1, std::memory_order_relaxed);
        g_bufferBytes.fetch_add(sent, std::memory_order_relaxed);
        RenderStats::Frame().bufferUploads++;
        RenderStats::Frame().uploadBytes += sent;
    }
}

void RenderResources::VerifyRange(Handle handle, uint32_t physical, uint32_t firstByte, uint32_t endByte)
{
    if (handle == 0 || handle > g_buffers.size()) return;
    BufferEntry& entry = g_buffers[handle - 1];
    if (!entry.buffer || entry.mirrorEpoch != Verify::epoch.load(std::memory_order_relaxed)) return;
    endByte = std::min(std::min(endByte, entry.seen), std::min(entry.bytes, uint32_t(entry.mirror.size())));
    if (firstByte >= endByte) return;
    Verify::Checked(Verify::Buffers);
    const uint8_t* data = Guest::Base + Guest::PhysicalAlias(physical);
    if (memcmp(data + firstByte, entry.mirror.data() + firstByte, endByte - firstByte) == 0) return;
    uint32_t firstDiff = 0, lastDiff = 0, differ = 0;
    for (uint32_t at = firstByte; at < endByte; at++)
        if (data[at] != entry.mirror[at]) { if (differ++ == 0) firstDiff = at; lastDiff = at; }
    Verify::Mismatch(Verify::Buffers, "vertex buffer %08X (%u bytes): a draw reads %u..%u, %u bytes there are not what the GPU has (%u..%u); last sent: %s",
        physical, entry.seen, firstByte, endByte, differ, firstDiff, lastDiff + 1, entry.lastSent);
    // Sent again, so the picture is right and one miss is not said by
    // every draw after it.
    D3D11_BOX box{ firstDiff, 0, 0, lastDiff + 1, 1, 1 };
    RenderQueue::Upload(entry.buffer.Get(), 0, &box, data + firstDiff, box.right - box.left);
    memcpy(entry.mirror.data() + firstDiff, data + firstDiff, lastDiff + 1 - firstDiff);
    if (entry.shadow.size() >= lastDiff + 1) memcpy(entry.shadow.data() + firstDiff, data + firstDiff, lastDiff + 1 - firstDiff);
}

ID3D11ShaderResourceView* RenderResources::VertexBufferView(Handle handle)
{
    if (handle == 0 || handle > g_buffers.size()) return nullptr;
    return g_buffers[handle - 1].resource.Get();
}

uint32_t RenderResources::IndexAppend(const void* data, uint32_t bytes)
{
    g_indexBytes.fetch_add(bytes, std::memory_order_relaxed);
    return g_indexRing.Append(data, bytes, 4);
}

uint8_t* RenderResources::IndexMap(uint32_t bytes, uint32_t& offset)
{
    g_indexBytes.fetch_add(bytes, std::memory_order_relaxed);
    return g_indexRing.Map(bytes, 4, offset);
}

void RenderResources::IndexUnmap() { g_indexRing.Unmap(); }

ID3D11Buffer* RenderResources::IndexRing() { return g_indexRing.buffer.Get(); }

namespace
{
    // The guest's indices as the host draws them (as RenderCommands'
    // Indices turns them round into the ring), into g_indexScratch.
    const uint8_t* ConvertIndices(const uint8_t* data, uint32_t count, bool wide, uint32_t reset, bool indices32)
    {
        g_indexScratch.resize(size_t(count) * (indices32 ? 4 : 2));
        const bool resetting = reset != ~0u;
        if (wide)
        {
            uint32_t* to = reinterpret_cast<uint32_t*>(g_indexScratch.data());
            for (uint32_t i = 0; i < count; i++)
            {
                uint32_t w;
                memcpy(&w, data + i * 4, 4);
                w = _byteswap_ulong(w);
                to[i] = resetting && (w & 0xFFFFFFu) == reset ? 0xFFFFFFFFu : w;
            }
        }
        else if (indices32)
        {
            // Widened, so the cut index can be the host's.
            uint32_t* to = reinterpret_cast<uint32_t*>(g_indexScratch.data());
            for (uint32_t i = 0; i < count; i++)
            {
                uint16_t h;
                memcpy(&h, data + i * 2, 2);
                const uint32_t index = _byteswap_ushort(h);
                to[i] = index == reset ? 0xFFFFFFFFu : index;
            }
        }
        else
        {
            uint16_t* to = reinterpret_cast<uint16_t*>(g_indexScratch.data());
            for (uint32_t i = 0; i < count; i++)
            {
                uint16_t h;
                memcpy(&h, data + i * 2, 2);
                to[i] = _byteswap_ushort(h);
            }
        }
        return g_indexScratch.data();
    }
}

ID3D11Buffer* RenderResources::IndexBufferFor(uint32_t physical, uint32_t count, bool wide, uint32_t reset, bool& indices32)
{
    if (!WriteWatch::Enabled() || count == 0 || count > (1u << 22)) return nullptr;
    t_fingerprintKind = "index";
    // Sixteen bit indices with a reset index that is not all ones are
    // widened, so the cut can be the host's.
    indices32 = wide || (reset != ~0u && reset != 0xFFFFu);
    const uint32_t guestBytes = count * (wide ? 4 : 2);
    const uint32_t key[4] = { Guest::GpuOffset(physical), count, wide ? 1u : 0u, reset };
    const uint8_t* data = Guest::Base + Guest::PhysicalAlias(physical);
    // 0.5.383: a small front cache avoids the full four-word key-table
    // hash/probe on repeated index ranges. Handles only: validate the live
    // entry's ENTIRE key, including count/width/reset, after handle reuse or
    // resource eviction. This never bypasses page/content validation below.
    // COD3_INDEXLOOKUPCACHE=0 restores the ordinary table lookup.
    static const bool fastLookup = []() {
        const char* value = getenv("COD3_INDEXLOOKUPCACHE");
        return value == nullptr || value[0] != '0';
    }();
    static constexpr uint32_t FrontSize = 2048;
    static Handle front[FrontSize] = {};
    const uint32_t frontSlot = ((key[0] >> 4) ^ (key[0] >> 16)
        ^ (count * 0x9E3779B9u) ^ reset ^ (wide ? 0x85EBCA6Bu : 0)) & (FrontSize - 1);
    Handle handle = fastLookup ? front[frontSlot] : 0;
    if (handle != 0)
    {
        const IndexEntry& cached = g_indexEntries[handle - 1];
        if (!cached.live || memcmp(cached.key, key, sizeof(cached.key)) != 0) handle = 0;
    }
    if (handle == 0) handle = g_indexKeys.Find(key);
    if (handle == 0)
    {
        // A candidate: noted, streamed this time.
        if (!g_freeIndexEntries.empty()) { handle = g_freeIndexEntries.back(); g_freeIndexEntries.pop_back(); }
        else { g_indexEntries.emplace_back(); handle = Handle(g_indexEntries.size()); }
        IndexEntry& entry = g_indexEntries[handle - 1];
        entry = IndexEntry();
        memcpy(entry.key, key, sizeof(entry.key));
        entry.fingerprint = Fingerprint(data, guestBytes, true);
        entry.usedFrame = g_frame;
        entry.live = true;
        g_indexKeys.Insert(key, handle);
        if (fastLookup) front[frontSlot] = handle;
        return nullptr;
    }
    if (fastLookup) front[frontSlot] = handle;
    IndexEntry& entry = g_indexEntries[handle - 1];
    entry.usedFrame = g_frame;
    if (entry.watchedSince != 0)
    {
        if (WriteWatch::Clean(physical, guestBytes, entry.watchedSince))
        {
            RenderStats::Frame().watchedLooks++;
            WatchAudit("index buffer", physical, data, guestBytes, true, entry.fingerprint);
            return entry.buffer.Get();
        }
        entry.watchedSince = 0;
    }
    // Not watched (yet): looked at by every draw, so a range the title
    // writes again within a frame is never drawn with what it held before.
    const uint64_t fingerprint = Fingerprint(data, guestBytes, true);
    if (fingerprint != entry.fingerprint)
    {
        entry.fingerprint = fingerprint;
        entry.stable = 0;
        if (!entry.buffer) return nullptr;
        RenderQueue::Upload(entry.buffer.Get(), 0, nullptr, ConvertIndices(data, count, wide, reset, indices32), 0);
        RenderStats::Frame().bufferUploads++;
        RenderStats::Frame().uploadBytes += entry.bytes;
        return entry.buffer.Get();
    }
    if (entry.stable < StableLooks) entry.stable++;
    if (!entry.buffer)
    {
        // Seen again the same: made.
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = count * (indices32 ? 4 : 2);
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
        D3D11_SUBRESOURCE_DATA initial{ ConvertIndices(data, count, wide, reset, indices32), 0, 0 };
        if (FAILED(g_device->CreateBuffer(&desc, &initial, &entry.buffer))) return nullptr;
        entry.bytes = desc.ByteWidth;
        g_resourceBytes += entry.bytes;
        RenderStats::Frame().bufferUploads++;
        RenderStats::Frame().uploadBytes += entry.bytes;
    }
    if (entry.stable >= StableLooks)
    {
        // Still for long enough: its pages watched from now on, and looked
        // at once more after they are, for a write in between.
        const uint64_t since = WriteWatch::Sequence();
        if (WriteWatch::Arm(physical, guestBytes) && Fingerprint(data, guestBytes, true) == entry.fingerprint)
            entry.watchedSince = since;
    }
    return entry.buffer.Get();
}

bool RenderResources::ConstantReserve(uint32_t bytes)
{
    const uint32_t aligned = (bytes + 255) & ~255u;
    if (g_constantRing.offset != 0 && g_constantRing.offset + aligned <= g_constantRing.bytes) return false;
    g_constantRing.offset = 0;
    return true;
}

uint32_t RenderResources::ConstantAppend(const void* data, uint32_t bytes)
{
    g_constantBytes.fetch_add(bytes, std::memory_order_relaxed);
    return g_constantRing.Append(data, bytes, 256);
}

uint8_t* RenderResources::ConstantMap(uint32_t bytes, uint32_t& offset)
{
    g_constantBytes.fetch_add(bytes, std::memory_order_relaxed);
    return g_constantRing.Map(bytes, 256, offset);
}

void RenderResources::ConstantUnmap() { g_constantRing.Unmap(); }

ID3D11Buffer* RenderResources::ConstantRing() { return g_constantRing.buffer.Get(); }
bool RenderResources::ConstantOffsetsAvailable() { return g_constantOffsets; }

RenderResources::Statistics RenderResources::Stats()
{
    Statistics s;
    s.textureUploads = g_textureUploads.load(std::memory_order_relaxed);
    s.textureBytes = g_textureBytes.load(std::memory_order_relaxed);
    s.bufferUploads = g_bufferUploads.load(std::memory_order_relaxed);
    s.bufferBytes = g_bufferBytes.load(std::memory_order_relaxed);
    s.indexBytes = g_indexBytes.load(std::memory_order_relaxed);
    s.constantBytes = g_constantBytes.load(std::memory_order_relaxed);
    s.resourceBytes = g_resourceBytes;
    s.textures = g_textureKeys.Size();
    s.buffers = g_bufferKeys.Size();
    s.colorTargets = g_colorKeys.Size();
    s.depthTargets = g_depthKeys.Size();
    s.resolved = g_resolvedKeys.Size();
    s.released = g_released;
    return s;
}

// 0.5.677: a picture for the texture at a guest place (its base address, as
// the texture's fetch constant has it), used whenever a texture there is
// made (UploadTexture); an empty path takes it away. The Webley's kill feed
// icon is a copy of the Colt's image pointing at a place of its own, given
// CoD2's hud_webley this way (mp_fixes.cpp).
void Render::SetTextureImage(uint32_t address, const char* pngPath)
{
    std::lock_guard<std::mutex> lock(g_placedLock);
    const uint32_t key = Guest::GpuOffset(address & 0xFFFFF000u);
    if (!pngPath || !*pngPath) g_placedImages.erase(key);
    else { g_placedImages[key] = pngPath; g_placedLast = key; }
}
