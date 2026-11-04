#include "blood_diag.h"
#include "blood_projection.h"
extern void BloodDiagnosticEye(float* eye);
// The PC render layer: registers in, commands out. See render_commands.h.
//
// A draw is prepared in the order the profile names its sections: the
// registers into a snapshot and the programs looked up, the targets and
// the scissor, the pipeline handles, the textures, the vertex buffers, the
// constants gathered into the ring, the indices turned round into theirs.
// Nothing here calls the Direct3D context except through the resource
// layer's rings and caches, and nothing allocates on a draw that has been
// seen before.

#include "render_commands.h"
#include "render_internal.h"
#include "tuning.h"
#include "verify.h"
#include "render_pipeline.h"
#include "render_shaders.h"
#include "render_resources.h"
#include "render_stats.h"
#include "debug_probe.h"
#include "settings.h"
#include "gpu.h"
#include "kernel.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <tuple>
#include <vector>

#include <d3d11_1.h>
#include <emmintrin.h>
#include <tmmintrin.h>   // 0.5.424: pshufb (SSSE3, in x86-64-v2)

using RenderState::Handle;
using RenderState::DrawCommand;
using RenderState::ResolveCommand;

namespace
{
    // --- what a draw was not made for ---------------------------------------------------------------

    enum SkipReason { SkipBackendOff, SkipPrimitive, SkipProgram, SkipProgramBuilding, SkipProgramOnRequest, SkipPitch, SkipTarget, SkipScissor, SkipBuffer, SkipAimBlur, SkipCount };
    const char* const SkipNames[SkipCount] = { "render backend off", "primitive type", "program", "program building", "program skipped on request", "no surface pitch", "no target", "empty scissor", "vertex buffer", "aim blur off" };

    // The title's depth of field while aiming: the frame downsampled to a
    // quarter (10e0411b), blurred (0925661f), and blended back over the
    // frame by depth (56ccd878), the weapon nearer than the plane staying
    // sharp. When the settings say, the last of the three is left out and
    // the frame stays as it was before it: the scene and the weapon,
    // sharp. The two before it still run, so the surface they make is
    // there for anything else that reads it.
    bool g_aimBlur = true;
    bool g_hdrScene = false;    // the HDR mode: the frame tone mapped at the bloom's composite
    bool g_steadyDecals = true; // the layers over a surface put a little nearer
    float g_bloom = 1.0f;       // the bloom's strength, the game's at one
    float g_hudGain = 0.0f;     // the HDR mode's HUD colour scale, nought for none
    uint64_t g_sceneSwap = ~0ull;   // the frame the scene pass last ran on
    // The shell shock pass (below): taken from the settings once a frame.
    // It was a Settings::Get() on every draw, a lock and a copy of the
    // whole settings block a thousand times a frame.
    bool g_shellShockBlur = false;
    // F11's shadow and foliage options, taken once a frame (SetRendering).
    bool g_sharpShadows = true, g_fastShadowMaps = true, g_foliageCoverage = true;
    bool IsAimBlurProgram(uint64_t hash)
    {
        return hash == 0x56ccd878c09e55bbull;
    }
    // 0.5.234: the death flash. Justin's first death on Marseille (0.5.233,
    // frame 7 logged, screenshots 155906): after the depth of field's
    // downsample (10e0411b) and blur (0925661f) the title draws 56ccd878,
    // which lays the blurred quarter size frame back over the whole view,
    // then cc16ad46 reads that frame twice at an offset: the "double
    // vision". With the blur while aiming off, 56ccd878 was left out
    // ("aim blur off x71" in the skips), so the two copies were the sharp
    // frame: two of every star icon and two bodies, the "split screen"
    // glimpse. While the player is down nothing is in his hands, so the
    // ghost outline the setting is there for cannot happen: drawn then.
    std::atomic<bool> g_playerDown{ false };
    // 0.5.248: the binoculars at the eyes (the host's player). Justin: the
    // sides of the binocular view are black at times, not at others, with
    // the HD textures off too. While they are up the depth of field pass is
    // drawn (it may be what lays the black round the lenses) and the draws
    // left out are counted by reason, said when they come down.
    std::atomic<bool> g_binocularsUp{ false };
    uint64_t g_upSkips[SkipCount] = {};
    uint64_t g_upFrames = 0;
    // 0.5.249: a list of the next draws, one line each, for the binocular
    // sides (Justin: at the x2 zoom he sees through them): DumpDraws.
    std::atomic<int> g_dumpLeft{ 0 };
    int g_dumpIndex = 0;
    RenderState::Snapshot g_dumpState;
    uint64_t g_dumpVs = 0, g_dumpPs = 0;
    // 0.5.252: a negative count lists only the flat (depth test off) draws
    // of the next 8000: the HUD and overlays, the binoculars' mask among
    // them (0.5.249's lists were the first 700 draws: all world).
    bool g_dumpHud = false;
    int g_dumpScan = 0;
    uint64_t g_skips[SkipCount] = {};

    void Skip(SkipReason reason)
    {
        g_skips[reason]++;
        if (g_binocularsUp.load(std::memory_order_relaxed)) g_upSkips[reason]++;
        RenderStats::Frame().skipped++;
    }

    // COD3_SKIPLOG=1: each frame whose skips, reason by reason, are not the
    // frame before's, for finding a draw that goes missing for one frame.
    void SkipFrameCheck()
    {
        static const bool log = getenv("COD3_SKIPLOG") != nullptr;
        if (!log) return;
        static uint64_t lastSwap = 0, atStart[SkipCount] = {}, previous[SkipCount] = {};
        const uint64_t swap = RenderInternal::Swaps();
        if (swap == lastSwap) return;
        uint64_t frame[SkipCount];
        bool differs = false;
        for (int i = 0; i < SkipCount; i++) { frame[i] = g_skips[i] - atStart[i]; if (frame[i] != previous[i]) differs = true; }
        if (differs)
        {
            printf("render: skips in frame %llu:", (unsigned long long)lastSwap);
            for (int i = 0; i < SkipCount; i++) if (frame[i] != 0) printf(" %s x%llu;", SkipNames[i], (unsigned long long)frame[i]);
            printf("\n");
        }
        memcpy(previous, frame, sizeof(frame));
        memcpy(atStart, g_skips, sizeof(atStart));
        lastSwap = swap;
    }

    // --- the constants ----------------------------------------------------------------------------------

    // The draw's own block, kept from one draw to the next so it goes up
    // only when it changed.
    RenderState::DrawConstants g_constants{};
    RenderState::DrawConstants g_lastConstants{};

    // What has been written to the float files and not yet uploaded, in
    // float4s, per stage.
    uint32_t g_pendingFirst[2] = { 0, 0 }, g_pendingEnd[2] = { 256, 256 };

    // What is in the ring: where each stage's file went last, packed for
    // which program, and where the draw block went. Invalid after the ring
    // wrapped, since a discard threw the old contents away.
    struct Uploaded
    {
        uint32_t floatAt[2] = { 0xFFFFFFFFu, 0xFFFFFFFFu };
        uint32_t floatCount[2] = { 0, 0 };
        Handle floatProgram[2] = { 0, 0 };
        uint32_t drawAt = 0xFFFFFFFFu;
    };
    Uploaded g_uploaded;

    // Without the ring, the files go whole: the scratch the command points at.
    uint32_t g_floatFile[2][1024];

    // From a draw's record: the file as it was when it last went up, by
    // stage, and whether a program's constants differ from it now.
    uint32_t g_sentFile[2][1024];
    // 0.5.386: whether g_sentFile holds what went up for the stage. Only
    // the draws' records (the native command lists) read it, so with them
    // off the copy was a second memcpy of every upload for nothing; it is
    // kept only while records are in use, and a stage whose copy was not
    // kept goes up whole at the first recorded draw. COD3_ALWAYSSENTFILE=1:
    // kept always, as before.
    bool g_sentValid[2] = { false, false };
    // COD3_CONSTCHECK: what each stage's last upload packed.
    std::vector<uint32_t> g_packedShadow[2];
    bool RecordChanged(uint32_t stage, const std::vector<uint16_t>& map, const uint32_t* file)
    {
        const uint32_t* now = file + (stage == 0 ? 0x4000 : 0x4400);
        if (map.empty()) return memcmp(g_sentFile[stage], now, 4096) != 0;
        size_t i = 0;
        while (i < map.size())
        {
            size_t run = 1;
            while (i + run < map.size() && map[i + run] == map[i] + run) run++;
            if (memcmp(g_sentFile[stage] + map[i] * 4u, now + map[i] * 4u, run * 16) != 0) return true;
            i += run;
        }
        return false;
    }
    uint32_t g_bools[40];
    bool g_boolsUploaded = false;

    // --- scratch for the indices ------------------------------------------------------------------------

    // Kept between draws: no allocation once it has grown to the largest draw.
    std::vector<uint32_t> g_indices32;

    inline uint32_t BSwap16(uint16_t v) { return uint16_t((v >> 8) | (v << 8)); }

    // Sixteen bit words turned round, eight at a time.
    void Swap16Into(uint16_t* to, const uint16_t* from, uint32_t count)
    {
        uint32_t i = 0;
        for (; i + 8 <= count; i += 8)
        {
            __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(from + i));
            v = _mm_shuffle_epi8(v, _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14));   // 0.5.424: one shuffle
            _mm_storeu_si128(reinterpret_cast<__m128i*>(to + i), v);
        }
        for (; i < count; i++) to[i] = uint16_t(BSwap16(from[i]));
    }

    // Thirty two bit words turned round, four at a time.
    void Swap32Into(uint32_t* to, const uint32_t* from, uint32_t count)
    {
        uint32_t i = 0;
        for (; i + 4 <= count; i += 4)
        {
            __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(from + i));
            v = _mm_shuffle_epi8(v, _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));   // 0.5.424: one shuffle
            _mm_storeu_si128(reinterpret_cast<__m128i*>(to + i), v);
        }
        for (; i < count; i++) to[i] = _byteswap_ulong(from[i]);
    }

    // --- the pieces of a draw -----------------------------------------------------------------------------

    // The programs: the stage's current ones, ready or not. False when the
    // draw cannot be made with them, the skip counted.
    bool Programs(DrawCommand& out, const RenderShaders::Program*& vs, const RenderShaders::Program*& ps)
    {
        out.vertexShader = RenderShaders::Current(false);
        out.pixelShader = RenderShaders::Current(true);
        vs = RenderShaders::Get(out.vertexShader);
        ps = RenderShaders::Get(out.pixelShader);
        if (g_dumpLeft.load(std::memory_order_relaxed) > 0) { g_dumpVs = vs ? vs->hash : 0; g_dumpPs = ps ? ps->hash : 0; }
        if (vs == nullptr || ps == nullptr) { Skip(SkipProgram); return false; }
        const RenderShaders::State vsState = vs->state.load(std::memory_order_acquire);
        const RenderShaders::State psState = ps->state.load(std::memory_order_acquire);
        if (vsState != RenderShaders::State::Ready || psState != RenderShaders::State::Ready)
        {
            // A program that failed is said the first time a draw wants it:
            // that is what a missing thing on screen comes from.
            for (const RenderShaders::Program* program : { vs, ps })
            {
                if (program->state.load(std::memory_order_acquire) != RenderShaders::State::Failed || program->announced) continue;
                const_cast<RenderShaders::Program*>(program)->announced = true;
                printf("render: a draw wants %s program %016llx, which could not be built: %s\n",
                    program->pixel ? "pixel" : "vertex", (unsigned long long)program->hash, program->problem.empty() ? "the compile failed" : program->problem.c_str());
                fflush(stdout);
            }
            Skip(vsState == RenderShaders::State::Failed || psState == RenderShaders::State::Failed ? SkipProgram : SkipProgramBuilding);
            return false;
        }
        if (vs->skipped) { Skip(SkipProgramOnRequest); return false; }
        if (!g_aimBlur && IsAimBlurProgram(ps->hash) && !g_playerDown.load(std::memory_order_relaxed) && !g_binocularsUp.load(std::memory_order_relaxed)) { Skip(SkipAimBlur); return false; }
        out.vs = static_cast<ID3D11VertexShader*>(vs->shader);
        out.ps = static_cast<ID3D11PixelShader*>(ps->shader);
        return true;
    }

    // The targets the pixel program writes and the surface's scissor, in
    // host pixels. False when there is nowhere to draw.
    // The EDRAM's depth only mode, RB_MODECONTROL 5: the hardware writes
    // depth and runs no pixel program at all, so the title does not load
    // one for such a draw and leaves the last one in place. The shadow maps
    // are drawn so: the soldiers' casters followed a clear in one cascade
    // and inherited its program (nothing discarded, a whole shadow), and
    // followed the alpha tested casters in another and inherited theirs,
    // which discards by a texture the soldiers' draw never meant for it,
    // and which draws came before them changed from frame to frame with
    // what each cascade held: the soldiers' shadows came and went. The
    // depth pre-pass (select 0x80000001) uses the mode too, with the colour
    // mask left open. Here such a draw gets no pixel program and no colour
    // target. COD3_DEPTHMODEPS=1 runs the program as before.
    bool DepthOnlyMode(const RenderState::Snapshot& state)
    {
        static const bool runProgram = getenv("COD3_DEPTHMODEPS") != nullptr;
        return state.modeControl == 5 && !runProgram;
    }
    constexpr RenderState::Handle NoPixelProgram = 0xFFFFFFFFu;
    // A depth only draw with its colour mask closed: a shadow map's caster.
    // It gets no pixel program at all, not even the one that cuts depth to
    // the console's precision (which the shadow maps do not need: the cut
    // keeps the frame's own passes agreeing with each other), so the GPU's
    // depth only path draws it: a pixel program that writes depth turns
    // off the fast depth only rate and part of the early depth test, and
    // the shadow passes were most of the frame's GPU time in a battle.
    // COD3_SHADOWDEPTHCUT=1 gives them the cutting program again.
    constexpr RenderState::Handle NoPixelProgramFast = 0xFFFFFFFEu;

    bool Targets(const RenderState::Snapshot& state, const RenderShaders::Program* vs, const RenderShaders::Program* ps, DrawCommand& out, float& scaleY)
    {
        // The rows the scissor reaches, so the surfaces are tall enough
        // before they are looked up.
        {
            const uint32_t tl = state.scissorTopLeft, br = state.scissorBottomRight;
            const int32_t offsetY = (tl & 0x80000000u) ? 0 : (int32_t(state.windowOffset << 1) >> 17);
            const int32_t bottom = int32_t((br >> 16) & 0x7FFF) + offsetY;
            if (bottom > 0 && RenderResources::EnsureRows(state.pitch, uint32_t(bottom))) out.bindingsLost = true;
        }

        // Only the targets the pixel program writes, each once: the title
        // leaves the other target registers pointing at the same tiles, and
        // the host refuses one surface bound twice.
        const bool depthOnly = DepthOnlyMode(state);
        for (uint32_t i = 0; i < 4 && !depthOnly; i++)
        {
            if (((ps->colourTargets >> i) & 1) == 0) continue;
            if (((state.colorMask >> (i * 4)) & 0xF) == 0) continue;
            const RenderResources::ColorTarget* target = RenderResources::ColorTargetOf(RenderResources::ColorTargetFor(state.colorInfo[i], state.pitch));
            if (target == nullptr) continue;
            bool duplicate = false;
            for (uint32_t j = 0; j < i; j++) if (out.targets[j] == target->view) duplicate = true;
            if (duplicate) continue;
            out.targets[i] = target->view;
            out.targetResources[i] = target->resource;
            if (target->upgraded) out.upgradedTargets |= 1u << i;
            if (i == 0) out.colorTexture = target->texture;
            out.targetCount = i + 1;
        }
        if ((state.depthControl & 3) != 0)
        {
            if (const RenderResources::DepthTarget* depth = RenderResources::DepthTargetOf(RenderResources::DepthTargetFor(state.depthInfo, state.pitch)))
            {
                out.depthView = depth->view;
                out.depthResource = depth->resource;
                out.depthTexture = depth->texture;
            }
        }
        if (out.targetCount == 0 && out.depthView == nullptr)
        {
            // The first few, with their state: what a draw with nowhere to
            // draw is asking for.
            static int announced = 0;
            if ((announced < 6 && announced++ < 6))
            {
                printf("render: draw with no target: primitive %u, %u indices, mode %u, colour mask %08X, depth control %08X, targets written %X, colour info %08X, depth info %08X, vs %016llx ps %016llx\n",
                    state.primitive, state.indexCount, state.modeControl, state.colorMask, state.depthControl, ps->colourTargets, state.colorInfo[0], state.depthInfo,
                    (unsigned long long)vs->hash, (unsigned long long)ps->hash);
                fflush(stdout);
            }
            Skip(SkipTarget);
            return false;
        }

        // The target's host size, and the scissor in it. Bit 31 of the
        // scissor says the window offset does not apply.
        float scaleX;
        RenderResources::TargetSize(state.pitch, 4, out.targetWidth, out.targetHeight, scaleX, scaleY);
        {
            const uint32_t tl = state.scissorTopLeft, br = state.scissorBottomRight;
            const int32_t offsetX = (tl & 0x80000000u) ? 0 : (int32_t(state.windowOffset << 17) >> 17);
            const int32_t offsetY = (tl & 0x80000000u) ? 0 : (int32_t(state.windowOffset << 1) >> 17);
            out.scissor[0] = std::max(0, int(std::lround((int(tl & 0x7FFF) + offsetX) * scaleX)));
            out.scissor[1] = std::max(0, int(std::lround((int((tl >> 16) & 0x7FFF) + offsetY) * scaleY)));
            out.scissor[2] = std::min(int(out.targetWidth), int(std::lround((int(br & 0x7FFF) + offsetX) * scaleX)));
            out.scissor[3] = std::min(int(out.targetHeight), int(std::lround((int((br >> 16) & 0x7FFF) + offsetY) * scaleY)));
            if (out.scissor[2] <= out.scissor[0] || out.scissor[3] <= out.scissor[1]) { Skip(SkipScissor); return false; }
        }
        return true;
    }

    // A view that is not of the fetch's kind, said once each: a compressed
    // or 8 bit texture that reads as a surface a resolve made (a depth copy,
    // a shadow map, a copy of the frame) draws as a flat red or black
    // shape, which is how foliage was once seen to go.
    void CheckView(ID3D11ShaderResourceView* view, const uint32_t words[6], bool resolved, Handle texture, uint32_t slot, int stage)
    {
        if (view == nullptr || (!resolved && texture == RenderResources::WhiteTexture())) return;
        const uint32_t format = words[1] & 0x3F;
        DXGI_FORMAT expected;
        switch (format)
        {
        case 2: expected = DXGI_FORMAT_R8_UNORM; break;
        case 6: if (resolved) return; expected = DXGI_FORMAT_R8G8B8A8_UNORM; break;
        case 18: expected = DXGI_FORMAT_BC1_UNORM; break;
        case 19: expected = DXGI_FORMAT_BC2_UNORM; break;
        case 20: expected = DXGI_FORMAT_BC3_UNORM; break;
        default: return;
        }
        // 0.5.412: each view asked once. This check is only for the log, and
        // GetDesc is a call into the Direct3D runtime: some 12000 a frame in
        // Marseille's busy views, every texture of every draw, all on the
        // command thread. A view already found to be of the fetch's kind is
        // noted (by its pointer and the kind expected) and not asked again,
        // and so is one of another kind once looked at (the texture pack's
        // replacements are RGBA whatever the title's format, so every draw
        // of one asked again only to be told the same). A pointer reused for
        // a new view can at worst skip one log line. Resolved surfaces are
        // not noted (each read says so once anyway).
        // COD3_VIEWCHECKALL=1: every draw asks, as before.
        static const bool checkAll = getenv("COD3_VIEWCHECKALL") != nullptr;
        static constexpr uint32_t CheckedSize = 4096;
        static uint64_t checked[CheckedSize] = {};
        const uint64_t viewKey = (uint64_t(uintptr_t(view)) << 8) ^ uint64_t(expected) ^ 1u;
        const uint32_t checkedSlot = uint32_t((viewKey >> 4) ^ (viewKey >> 17) ^ (viewKey >> 31)) & (CheckedSize - 1);
        if (!checkAll && !resolved && checked[checkedSlot] == viewKey) return;
        D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
        view->GetDesc(&desc);
        if (!resolved) checked[checkedSlot] = viewKey;
        if (desc.Format == expected && !resolved) return;
        static std::atomic<int> said{ 0 };
        static uint64_t last = 0;
        const uint64_t what = (uint64_t(words[1]) << 32) ^ (uint64_t(slot) << 8) ^ uint64_t(desc.Format);
        if (what == last || (said.load(std::memory_order_relaxed) >= 100 || said.fetch_add(1) >= 100)) return;
        last = what;
        printf("render: frame %llu, %s texture %u, format %u at %08X (%ux%u), is bound to a view of DXGI format %u%s\n",
            (unsigned long long)RenderInternal::Swaps(), stage == 0 ? "pixel" : "vertex", slot, format, words[1] & 0xFFFFF000u,
            (words[2] & 0x1FFF) + 1, ((words[2] >> 13) & 0x1FFF) + 1, unsigned(desc.Format), resolved ? ", a resolved surface" : "");
        fflush(stdout);
    }

    // The textures and samplers the programs sample: the pixel program's,
    // and the vertex program's (the terrain's height map).
    void Textures(const RenderShaders::Program* vs, const RenderShaders::Program* ps, DrawCommand& out, RenderState::DrawConstants& constants)
    {
        for (int stage = 0; stage < 2; stage++)
        {
            const RenderShaders::Program* program = stage == 0 ? ps : vs;
            for (const XenosHlsl::TextureFetch& fetch : program->textureFetches)
            {
                if (out.textureCount == DrawCommand::MaxTextures) return;
                uint32_t words[6];
                RenderState::ReadTextureFetch(fetch.slot, words);
                uint32_t width = 1, height = 1;
                bool resolved = false;
                const Handle texture = RenderResources::TextureFor(words, width, height, resolved);
                DrawCommand::Texture& binding = out.textures[out.textureCount++];
                binding.stage = uint8_t(stage);
                binding.slot = uint8_t(fetch.slot);
                binding.samplerSlot = uint8_t(fetch.sampler);
                if (resolved) binding.view = RenderResources::ResolvedView(words[1] & 0xFFFFF000u, width, height);
                else binding.view = RenderResources::TextureView(texture, fetch.dimension);
                CheckView(binding.view, words, resolved, texture, fetch.slot, stage);
                // A shadow map: a depth surface the program filters itself,
                // four taps at a texel's corners blended by getWeights. Those
                // go by the size given here, and with the title's size they
                // filtered on the console's 512 texel grid however finely the
                // map was drawn: shadows in blocks of the console's texels,
                // and the texture filter setting blurring the depths besides.
                // So the map's own size, and point sampling: four exact texels
                // of the map as drawn. COD3_OLDSHADOWS=1 for the old way.
                static const bool oldShadowsForced = getenv("COD3_OLDSHADOWS") != nullptr;
                const bool oldShadows = oldShadowsForced || !g_sharpShadows;
                const RenderResources::Resolved* shadow = (resolved && fetch.weights != 0 && !oldShadows) ? RenderResources::ResolvedAt(words[1] & 0xFFFFF000u) : nullptr;
                if (shadow != nullptr && shadow->depth && shadow->guestWidth != 0 && shadow->guestHeight != 0)
                {
                    width = std::max(1u, uint32_t(std::lround(double(width) * shadow->width / shadow->guestWidth)));
                    height = std::max(1u, uint32_t(std::lround(double(height) * shadow->height / shadow->guestHeight)));
                    binding.sampler = RenderPipeline::SamplerObject(RenderPipeline::SamplerPoint(words));
                }
                else binding.sampler = RenderPipeline::SamplerObject(RenderPipeline::Sampler(words));
                constants.textureSize[fetch.slot][0] = float(width);
                constants.textureSize[fetch.slot][1] = float(height);
                constants.textureSize[fetch.slot][2] = 1.0f / float(width);
                constants.textureSize[fetch.slot][3] = 1.0f / float(height);
                // A surface the resolve made is sampled by the title with red
                // and blue swapped: the resolve wrote the console's memory in
                // one byte order and the fetch reads it in another, and the
                // swizzle in the fetch constant undoes that. The surfaces
                // here never go through memory, so the swap is left out.
                // Not only the 8_8_8_8 fetch with alpha as the fourth (0x60A):
                // the damage flash (shot or hit by a melee attack) lays a
                // resolved copy of an earlier frame over the picture, read
                // with red and blue swapped the same way, and a brown scene
                // with its red and blue swapped went purple. Any resolved
                // surface read with red from the third channel and blue from
                // the first gets them back in place, whatever its format and
                // its other two channels. Each new pair of format and
                // swizzle a resolved surface is read with goes in the log.
                uint32_t swizzle = (words[3] >> 1) & 0xFFF;
                if (resolved)
                {
                    const uint32_t format = words[1] & 0x3F;
                    const uint32_t original = swizzle;
                    if ((swizzle & 7) == 2 && ((swizzle >> 6) & 7) == 0) swizzle = (swizzle & ~0x1C7u) | (2u << 6);
                    static std::atomic<uint32_t> seen[64];
                    static std::atomic<uint32_t> seenCount{ 0 };
                    const uint32_t key = ((format << 12) | original) + 1;
                    const uint32_t count = std::min<uint32_t>(seenCount.load(std::memory_order_relaxed), 64);
                    bool known = false;
                    for (uint32_t i = 0; i < count && !known; i++) known = seen[i].load(std::memory_order_relaxed) == key;
                    const uint32_t slot = (known || count >= 64) ? 64 : seenCount.fetch_add(1, std::memory_order_relaxed);
                    if (slot < 64)
                    {
                        seen[slot].store(key, std::memory_order_relaxed);
                        printf("render: a resolved surface at %08X (%ux%u) read as format %u with swizzle %03X%s\n",
                            words[1] & 0xFFFFF000u, width, height, format, original,
                            swizzle != original ? ", red and blue put back" : "");
                        fflush(stdout);
                    }
                }
                constants.textureAdjustment[fetch.slot][0] = swizzle;
                constants.textureAdjustment[fetch.slot][1] = (words[0] >> 2) & 0xFF;
            }
        }
    }

    // The draw's buffers that want the part it reads looked at again
    // (RenderResources::NeedsRangeLook), for LookRanges once its indices
    // are known.
    struct RangeLook { Handle buffer; uint32_t physical; uint32_t stride; bool wholeIfUnknown = false; };
    std::vector<RangeLook> g_rangeLooks;
    std::vector<RangeLook> g_verifyBuffers;   // verify mode: every buffer the draw reads

    // The vertex buffers the vertex program fetches. False when one could
    // not be had.
    bool Buffers(const RenderShaders::Program* vs, DrawCommand& out)
    {
        g_rangeLooks.clear();
        g_verifyBuffers.clear();
        const bool verify = Verify::On(Verify::Buffers);
        for (const XenosHlsl::VertexFetch& fetch : vs->vertexFetches)
        {
            uint32_t word0, word1;
            RenderState::ReadVertexFetch(fetch.slot, word0, word1);
            // The programs turn every word round on the way in, which is the
            // console's usual byte order for a buffer; another would need a
            // conversion on upload, and is noted if it ever comes.
            if ((word1 & 3) != 2)
            {
                static int announced = 0;
                if ((announced < 4 && announced++ < 4)) printf("render: a vertex buffer in byte order %u, drawn as order 2\n", word1 & 3);
            }
            const Handle buffer = RenderResources::VertexBufferFor(word0 & ~3u, ((word1 >> 2) & 0xFFFFFF) * 4, 2);
            ID3D11ShaderResourceView* view = RenderResources::VertexBufferView(buffer);
            if (view == nullptr || out.bufferCount == DrawCommand::MaxBuffers) { Skip(SkipBuffer); return false; }
            DrawCommand::Buffer& binding = out.buffers[out.bufferCount++];
            binding.slot = uint8_t(fetch.slot);
            binding.view = view;
            if (RenderResources::NeedsRangeLook(buffer)) g_rangeLooks.push_back(RangeLook{ buffer, word0 & ~3u, fetch.stride, RenderResources::MidFrameRangeLook(buffer) });
            if (verify) g_verifyBuffers.push_back(RangeLook{ buffer, word0 & ~3u, fetch.stride });
        }
        return true;
    }

    // With "Watch memory writes" off: the vertices this draw reads, of a
    // big buffer the frame's first draw looked at and the title has been
    // changing, compared again and sent where they differ. The title
    // writes its dynamic geometry (marks, effects) into one big pool a
    // piece at a time through the frame, each piece just before the draw
    // that uses it; looked at only by the frame's first draw, a later draw
    // got what its piece held before, and its vertices were flung across
    // the screen as lines. That took the command thread running alongside
    // the title's writes, rather than a frame behind them, to show. Which
    // vertices: the indices' lowest and highest, with the index offset.
    // The lowest and highest vertex the draw's indices name, without the
    // index offset. False for a draw with none.
    bool DrawVertexRange(const RenderState::Snapshot& state, const DrawCommand& out, uint32_t& lowest, uint32_t& highest)
    {
        const uint32_t count = state.indexCount;
        if (count == 0) return false;
        lowest = 0xFFFFFFFFu; highest = 0;
        if (state.sourceSelect == 0)
        {
            const bool strips = out.topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP || out.topology == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
            const bool resetEnabled = strips && (state.suScModeControl & (1u << 21)) != 0;
            const uint8_t* data = Guest::Base + Guest::PhysicalAlias(state.indexBase);
            if (state.wideIndices)
            {
                const uint32_t reset = resetEnabled ? state.resetIndex & 0xFFFFFFu : 0xFFFFFFFFu;
                const uint32_t* from = reinterpret_cast<const uint32_t*>(data);
                for (uint32_t i = 0; i < count; i++)
                {
                    const uint32_t index = _byteswap_ulong(from[i]);
                    if (resetEnabled && (index & 0xFFFFFFu) == reset) continue;
                    lowest = std::min(lowest, index); highest = std::max(highest, index);
                }
            }
            else
            {
                const uint32_t reset = resetEnabled ? state.resetIndex & 0xFFFFu : 0xFFFFFFFFu;
                const uint16_t* from = reinterpret_cast<const uint16_t*>(data);
                for (uint32_t i = 0; i < count; i++)
                {
                    const uint32_t index = BSwap16(from[i]);
                    if (index == reset) continue;
                    lowest = std::min(lowest, index); highest = std::max(highest, index);
                }
            }
        }
        else { lowest = 0; highest = count - 1; }
        return lowest <= highest;
    }

    void LookRanges(const RenderState::Snapshot& state, const DrawCommand& out)
    {
        uint32_t lowest, highest;
        if (!DrawVertexRange(state, out, lowest, highest))
        {
            // 0.5.374: a mid-frame buffer kept by its copy is compared whole
            // when the draw's vertices can't be told.
            for (const RangeLook& look : g_rangeLooks)
                if (look.wholeIfUnknown) RenderResources::LookRange(look.buffer, look.physical, 0, 0xFFFFFFFFu);
            return;
        }
        const int64_t offset = state.indexOffset;
        for (const RangeLook& look : g_rangeLooks)
        {
            if (look.stride == 0)
            {
                if (look.wholeIfUnknown) RenderResources::LookRange(look.buffer, look.physical, 0, 0xFFFFFFFFu);
                continue;
            }
            const int64_t strideBytes = int64_t(look.stride) * 4;
            auto send = [&](int64_t first, int64_t last)
            {
                if (last < 0) return;
                first = std::max<int64_t>(first, 0);
                const int64_t from = std::min<int64_t>(first * strideBytes, 0xFFFFFFFFll);
                const int64_t to = std::min<int64_t>((last + 1) * strideBytes, 0xFFFFFFFFll);
                RenderResources::LookRange(look.buffer, look.physical, uint32_t(from), uint32_t(to));
            };
            // With the offset, as the program sees the vertex; and without,
            // should the host's vertex id not carry it.
            send(int64_t(lowest) + offset, int64_t(highest) + offset);
            if (offset != 0) send(lowest, highest);
        }
    }

    // Verify mode, late writes: what each draw read of its vertices and
    // indices, kept with a hash of it, and hashed again when the frame is
    // swapped. The title's dynamic geometry (the bullet holes among it) is
    // in pools it fills a frame at a time, two of them in turn; one that
    // changed between the draw and the frame's end was still being written
    // when the draw read it (the console's GPU reads it later than the
    // command thread here does). Only in verify mode.
    struct LateCheck { uint32_t physical, bytes; uint64_t hash; bool indices; uint64_t ps; };
    std::vector<LateCheck> g_lateChecks;
    uint64_t HashGuest(uint32_t physical, uint32_t bytes)
    {
        const uint8_t* data = Guest::Base + Guest::PhysicalAlias(physical);
        uint64_t h = 1469598103934665603ull;
        for (uint32_t i = 0; i < bytes; i++) { h ^= data[i]; h *= 1099511628211ull; }
        return h;
    }
    void NoteForLateCheck(uint32_t physical, uint32_t bytes, bool indices, uint64_t ps)
    {
        if (bytes == 0 || bytes > (64u << 10) || g_lateChecks.size() >= 20000) return;
        g_lateChecks.push_back(LateCheck{ physical, bytes, HashGuest(physical, bytes), indices, ps });
    }

    // Verify mode: the vertices the draw reads, with the index offset, held
    // against what was sent of each of its buffers (verify.h).
    void VerifyDrawBuffers(const RenderState::Snapshot& state, const DrawCommand& out, uint64_t ps)
    {
        uint32_t lowest, highest;
        if (!DrawVertexRange(state, out, lowest, highest)) return;
        const int64_t first = std::max<int64_t>(int64_t(lowest) + state.indexOffset, 0);
        const int64_t last = int64_t(highest) + state.indexOffset;
        if (last < first) return;
        if (state.sourceSelect == 0) NoteForLateCheck(state.indexBase, state.indexCount * (state.wideIndices ? 4u : 2u), true, ps);
        for (const RangeLook& look : g_verifyBuffers)
        {
            if (look.stride == 0) continue;
            const int64_t strideBytes = int64_t(look.stride) * 4;
            const uint32_t from = uint32_t(std::min<int64_t>(first * strideBytes, 0xFFFFFFFFll));
            const uint32_t to = uint32_t(std::min<int64_t>((last + 1) * strideBytes, 0xFFFFFFFFll));
            RenderResources::VerifyRange(look.buffer, look.physical, from, to);
            if (to > from) NoteForLateCheck(look.physical + from, to - from, false, ps);
        }
    }

    // The float files, packed to what the programs read, and the draw's own
    // block, into the ring; the booleans when they changed. A file goes up
    // when something the program reads was written since it last went up,
    // and only as much of it as the program reads: the title writes a few
    // constants before most draws, and the whole four kilobytes a draw was
    // most of what a draw cost. The blocks go in one map, bound at offsets.
    // False when the ring could not be mapped.
    bool Constants(const RenderShaders::Program* vs, const RenderShaders::Program* ps, DrawCommand& out, const RenderState::DrawConstants& constants)
    {
        const uint32_t* file = RenderState::Registers();
        // From the register file, what was written since is what the stream
        // says; from a draw's record, what differs from what went up.
        const bool record = RenderState::UsingRecord();
        for (uint32_t range = 0; range < 2; range++)
        {
            uint32_t spanFirst, spanEnd;
            if (!Gpu::TakeConstantSpan(range, spanFirst, spanEnd)) continue;
            g_pendingFirst[range] = std::min(g_pendingFirst[range], spanFirst / 4);
            g_pendingEnd[range] = std::max(g_pendingEnd[range], (spanEnd + 3) / 4);
        }
        {
            uint32_t spanFirst, spanEnd;
            const bool written = Gpu::TakeConstantSpan(2, spanFirst, spanEnd);
            if (record ? memcmp(g_bools, file + 0x4900, sizeof(g_bools)) != 0 : written)
                g_boolsUploaded = false;
            if (!g_boolsUploaded)
            {
                memcpy(g_bools, file + 0x4900, sizeof(g_bools));
                out.bools = g_bools;
                out.boolsChanged = true;
                g_boolsUploaded = true;
            }
        }
        const bool drawChanged = memcmp(&g_lastConstants, &constants, sizeof(constants)) != 0;
        const RenderShaders::Program* programs[2] = { vs, ps };
        const Handle handles[2] = { out.vertexShader, out.pixelShader & ~RenderShaders::OpaqueVariantBit };
        out.ring = RenderResources::ConstantOffsetsAvailable();
        if (!out.ring)
        {
            // No offsets on this device: the whole files, in place.
            for (uint32_t stage = 0; stage < 2; stage++)
            {
                if (!record && g_pendingFirst[stage] >= 256 && g_uploaded.floatAt[stage] != 0xFFFFFFFFu) continue;
                const uint32_t first = stage == 0 ? 0x4000 : 0x4400;
                if (record && g_uploaded.floatAt[stage] != 0xFFFFFFFFu && memcmp(g_floatFile[stage], file + first, 4096) == 0) continue;
                memcpy(g_floatFile[stage], file + first, 4096);
                out.floatFile[stage] = g_floatFile[stage];
                out.floatChanged[stage] = true;
                g_uploaded.floatAt[stage] = 0;
                g_pendingFirst[stage] = 256; g_pendingEnd[stage] = 0;
                RenderStats::Frame().streamBytes += 4096;
            }
            if (drawChanged || g_uploaded.drawAt == 0xFFFFFFFFu)
            {
                g_lastConstants = constants;
                g_uploaded.drawAt = 0;
                out.drawConstants = &g_lastConstants;
                out.drawChanged = true;
            }
            return true;
        }

        constexpr uint32_t DrawBytes = (sizeof(constants) + 255) & ~255u;
        uint32_t need[2], bytes = 0;
        bool upload[2];
        for (uint32_t stage = 0; stage < 2; stage++)
        {
            const std::vector<uint16_t>& map = programs[stage]->constantMap;
            const uint32_t count = map.empty() ? 256 : uint32_t(map.size());
            need[stage] = std::min(256u, std::max(16u, (count + 15) & ~15u));
            // The file's span the program reads, for the dirty check.
            const uint32_t readFirst = map.empty() ? 0 : map.front(), readEnd = map.empty() ? 256 : map.back() + 1u;
            const bool dirty = record ? (!g_sentValid[stage] || RecordChanged(stage, map, file)) : g_pendingFirst[stage] < readEnd && g_pendingEnd[stage] > readFirst;
            upload[stage] = g_uploaded.floatAt[stage] == 0xFFFFFFFFu || g_uploaded.floatProgram[stage] != handles[stage] || dirty;
            // COD3_CONSTCHECK=1: a stage whose upload is skipped held against
            // what the upload before it packed: a constant that differs is
            // one the skip got wrong.
            static const bool constCheckEnv = getenv("COD3_CONSTCHECK") != nullptr;
            const bool constCheck = constCheckEnv || Verify::On(Verify::Constants);
            if (constCheck && !upload[stage] && g_packedShadow[stage].size() == size_t(need[stage]) * 4)
            {
                Verify::Checked(Verify::Constants);
                const uint32_t* from = file + (stage == 0 ? 0x4000 : 0x4400);
                const size_t count = map.empty() ? 256 : map.size();
                for (size_t i = 0; i < count; i++)
                {
                    const uint32_t constant = map.empty() ? uint32_t(i) : map[i];
                    if (memcmp(&g_packedShadow[stage][i * 4], from + constant * 4, 16) == 0) continue;
                    Verify::Mismatch(Verify::Constants, "%s %016llx c%u was uploaded as %08X %08X %08X %08X, is %08X %08X %08X %08X now; skipped as unchanged (record %d)",
                        stage == 0 ? "vs" : "ps", (unsigned long long)programs[stage]->hash, constant,
                        g_packedShadow[stage][i * 4], g_packedShadow[stage][i * 4 + 1], g_packedShadow[stage][i * 4 + 2], g_packedShadow[stage][i * 4 + 3],
                        from[constant * 4], from[constant * 4 + 1], from[constant * 4 + 2], from[constant * 4 + 3], record ? 1 : 0);
                    // Uploaded after all, so the draw is right and the miss
                    // is said once rather than by every draw after it.
                    upload[stage] = true;
                    break;
                }
            }
            if (upload[stage]) bytes += need[stage] * 16;
        }
        bool drawUpload = drawChanged || g_uploaded.drawAt == 0xFFFFFFFFu;
        if (drawUpload) bytes += DrawBytes;
        if (bytes != 0)
        {
            if (RenderResources::ConstantReserve(need[0] * 16 + need[1] * 16 + DrawBytes))
            {
                // A wrap: everything remembered is in the old buffer.
                g_uploaded = Uploaded();
                upload[0] = upload[1] = drawUpload = true;
                bytes = need[0] * 16 + need[1] * 16 + DrawBytes;
            }
            uint32_t base;
            uint8_t* to = RenderResources::ConstantMap(bytes, base);
            if (to == nullptr) { Skip(SkipBuffer); return false; }
            uint32_t at = 0;
            for (uint32_t stage = 0; stage < 2; stage++)
            {
                if (!upload[stage]) continue;
                const uint32_t first = stage == 0 ? 0x4000 : 0x4400;
                uint32_t* words = reinterpret_cast<uint32_t*>(to + at);
                const std::vector<uint16_t>& map = programs[stage]->constantMap;
                // The file's words are written by this thread: plain copies.
                const uint32_t* from = reinterpret_cast<const uint32_t*>(file + first);
                // What was sent is kept for RecordChanged, which compares
                // only the constants this program reads: a change of program
                // uploads anyway, so the rest of the file need not be kept
                // (a whole 4 KB a draw was most of the command processor's
                // copying).
                static const bool alwaysSent = getenv("COD3_ALWAYSSENTFILE") != nullptr;
                static const bool constantLibraryCopy = getenv("COD3_CONSTLIBCOPY") != nullptr;
                const bool keepSent = record || alwaysSent;
                g_sentValid[stage] = keepSent;
                if (map.empty())
                {
                    memcpy(words, from, 4096);
                    if (keepSent) memcpy(g_sentFile[stage], from, 4096);
                }
                else if (!constantLibraryCopy)
                {
                    // 0.5.414: a constant (16 bytes) at a time, straight in
                    // order, without the library's memcpy. Each run of
                    // consecutive constants was a call to it, a short copy
                    // of unknown length each time: 3.2% of the command
                    // thread in 0.5.410's busy views was memcpy from here.
                    // The ring's memory is written front to back as before.
                    // COD3_CONSTLIBCOPY=1: the runs through memcpy again.
                    const uint16_t* constant = map.data();
                    const size_t count = map.size();
                    __m128i* toWords = reinterpret_cast<__m128i*>(words);
                    const uint32_t* fromFile = from;
                    for (size_t i = 0; i < count; i++)
                        _mm_storeu_si128(toWords + i, _mm_loadu_si128(reinterpret_cast<const __m128i*>(fromFile + uint32_t(constant[i]) * 4u)));
                    // What was sent kept from the file, not read back from
                    // the ring (mapped for writing: slow to read).
                    if (keepSent)
                        for (size_t i = 0; i < count; i++)
                        {
                            const uint32_t at = uint32_t(constant[i]) * 4u;
                            _mm_storeu_si128(reinterpret_cast<__m128i*>(g_sentFile[stage] + at), _mm_loadu_si128(reinterpret_cast<const __m128i*>(fromFile + at)));
                        }
                }
                else
                {
                    // Runs of consecutive constants go as one copy.
                    size_t i = 0;
                    while (i < map.size())
                    {
                        size_t run = 1;
                        while (i + run < map.size() && map[i + run] == map[i] + run) run++;
                        memcpy(words + i * 4, from + map[i] * 4u, run * 16);
                        if (keepSent) memcpy(g_sentFile[stage] + map[i] * 4u, from + map[i] * 4u, run * 16);
                        i += run;
                    }
                }
                // COD3_DUMPCONST=hash: what a program's constants hold,
                // once a second, to see whether the file that reaches it is
                // the file the title wrote.
                static const char* const dumpConst = getenv("COD3_DUMPCONST");
                char hashText[24] = {};
                if (dumpConst != nullptr) snprintf(hashText, sizeof(hashText), "%016llx", (unsigned long long)programs[stage]->hash);
                // Several hashes may be named, with commas; in a logged
                // frame every upload is printed, not one a second.
                if (dumpConst != nullptr && strstr(dumpConst, hashText) != nullptr)
                {
                    static uint64_t last = 0;
                    const uint64_t now = GetTickCount64() / 1000;
                    if (now != last || RenderInternal::FrameLogged())
                    {
                        last = now;
                        const float* values = reinterpret_cast<const float*>(words);
                        printf("const %016llx %u read: bools", (unsigned long long)programs[stage]->hash, unsigned(map.size()));
                        for (uint32_t i = 0; i < 8; i++) printf(" %08X", file[0x4900 + i]);
                        printf(" loops");
                        for (uint32_t i = 0; i < 32; i++) printf(" %08X", file[0x4908 + i]);
                        printf(" ::");
                        for (size_t i = 0; i < map.size(); i++)
                            printf(" c%u=(%.4g %.4g %.4g %.4g)", unsigned(map[i]),
                                values[i * 4], values[i * 4 + 1], values[i * 4 + 2], values[i * 4 + 3]);
                        printf("\n");
                        fflush(stdout);
                    }
                }
                static const bool constCheckEnv = getenv("COD3_CONSTCHECK") != nullptr;
                if (constCheckEnv || Verify::On(Verify::Constants)) g_packedShadow[stage].assign(words, words + need[stage] * 4);
                g_uploaded.floatAt[stage] = base + at;
                g_uploaded.floatCount[stage] = need[stage];
                g_uploaded.floatProgram[stage] = handles[stage];
                at += need[stage] * 16;
                // Nothing is pending after an upload: a change of program
                // uploads anyway, and the same program only needs what is
                // written from here on.
                g_pendingFirst[stage] = 256; g_pendingEnd[stage] = 0;
            }
            if (drawUpload)
            {
                memcpy(to + at, &constants, sizeof(constants));
                g_lastConstants = constants;
                g_uploaded.drawAt = base + at;
                at += DrawBytes;
            }
            RenderResources::ConstantUnmap();
            RenderStats::Frame().streamBytes += at;
            RenderStats::Frame().constantUploads += (upload[0] ? 1 : 0) + (upload[1] ? 1 : 0) + (drawUpload ? 1 : 0);
        }
        out.floatAt[0] = g_uploaded.floatAt[0]; out.floatAt[1] = g_uploaded.floatAt[1];
        out.floatCount[0] = g_uploaded.floatCount[0]; out.floatCount[1] = g_uploaded.floatCount[1];
        out.drawAt = g_uploaded.drawAt;
        out.drawBytes = DrawBytes;
        return true;
    }

    // COD3_INDEXCHECK=1: an indexed draw whose indices reach outside the
    // hardware's clamp (VGT_MIN/MAX_VTX_INDX) or past the end of a vertex
    // buffer its program reads, said for each vertex program.
    void IndexCheck(const RenderState::Snapshot& state, const RenderShaders::Program* vs, const DrawCommand& out)
    {
        static const bool on = getenv("COD3_INDEXCHECK") != nullptr;
        if (!on || !out.indexed || state.sourceSelect != 0 || state.indexCount == 0) return;
        const uint8_t* data = Guest::Base + Guest::PhysicalAlias(state.indexBase);
        const bool strips = out.topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP || out.topology == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        const bool resetEnabled = strips && (state.suScModeControl & (1u << 21)) != 0;
        const uint32_t reset = state.resetIndex & (state.wideIndices ? 0xFFFFFFu : 0xFFFFu);
        uint32_t lowest = 0xFFFFFFFFu, highest = 0, resets = 0;
        for (uint32_t i = 0; i < state.indexCount; i++)
        {
            uint32_t index;
            if (state.wideIndices) { uint32_t w; memcpy(&w, data + i * 4, 4); index = _byteswap_ulong(w) & 0xFFFFFFu; }
            else { uint16_t h; memcpy(&h, data + i * 2, 2); index = BSwap16(h); }
            if (resetEnabled && index == reset) { resets++; continue; }
            lowest = std::min(lowest, index);
            highest = std::max(highest, index);
        }
        if (lowest > highest) return;
        uint32_t vertices = 0xFFFFFFFFu;
        for (const XenosHlsl::VertexFetch& fetch : vs->vertexFetches)
        {
            uint32_t word0, word1;
            RenderState::ReadVertexFetch(fetch.slot, word0, word1);
            if (fetch.stride != 0) vertices = std::min(vertices, ((word1 >> 2) & 0xFFFFFFu) / fetch.stride);
        }
        const int64_t first = int64_t(lowest) + state.indexOffset, last = int64_t(highest) + state.indexOffset;
        const uint32_t clampLow = state.minVertexIndex & 0xFFFFFFu, clampHigh = state.maxVertexIndex & 0xFFFFFFu;
        const bool past = last >= int64_t(vertices) || first < 0 || highest > clampHigh || lowest < clampLow;
        if (!past) return;
        static std::atomic<int> said{ 0 };
        static uint64_t lastHash = 0;
        if (vs->hash == lastHash || (said.load(std::memory_order_relaxed) >= 60 || said.fetch_add(1) >= 60)) return;
        lastHash = vs->hash;
        printf("render: frame %llu, vs %016llx: %u indices (%u resets) from %u to %u, offset %d, clamp %u..%u, the buffer has %u vertices\n",
            (unsigned long long)RenderInternal::Swaps(), (unsigned long long)vs->hash, state.indexCount, resets, lowest, highest,
            state.indexOffset, clampLow, clampHigh, vertices);
        fflush(stdout);
    }

    // The indices: the title's, byte swapped into the index ring, or none.
    // Quads and fans become triangle lists. The index that ends a strip and
    // starts the next, when the title has turned it on (PA_SU_SC_MODE_CNTL
    // bit 21), is the host's own cut index when it is all ones, and is
    // rewritten to it when it is not. False when the ring is full.
    bool Indices(const RenderState::Snapshot& state, bool convertQuads, bool convertFan, DrawCommand& out)
    {
        uint32_t indexCount = state.indexCount;
        const bool strips = out.topology == D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP || out.topology == D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
        const bool resetEnabled = strips && (state.suScModeControl & (1u << 21)) != 0;
        const bool convert = convertQuads || convertFan;
        if (state.sourceSelect == 0 && !convert)
        {
            // Kept converted, while their pages say they are the same
            // (COD3_WRITEWATCH): nothing to turn round or stream.
            {
                const uint32_t reset = resetEnabled ? state.resetIndex & (state.wideIndices ? 0xFFFFFFu : 0xFFFFu) : ~0u;
                bool indices32 = false;
                if (ID3D11Buffer* kept = RenderResources::IndexBufferFor(state.indexBase, indexCount, state.wideIndices, reset, indices32))
                {
                    out.indexBuffer = kept;
                    out.indexRingOffset = 0;
                    out.indices32 = indices32;
                    out.indexed = true;
                    out.indexCount = indexCount;
                    return true;
                }
            }
            // The common case: turned round straight into the ring.
            const uint8_t* data = Guest::Base + Guest::PhysicalAlias(state.indexBase);
            if (state.wideIndices)
            {
                out.indices32 = true;
                uint32_t* to = reinterpret_cast<uint32_t*>(RenderResources::IndexMap(indexCount * 4, out.indexRingOffset));
                if (to == nullptr) { Skip(SkipBuffer); return false; }
                Swap32Into(to, reinterpret_cast<const uint32_t*>(data), indexCount);
                if (resetEnabled)
                {
                    const uint32_t reset = state.resetIndex & 0xFFFFFFu;
                    for (uint32_t i = 0; i < indexCount; i++) if ((to[i] & 0xFFFFFFu) == reset) to[i] = 0xFFFFFFFFu;
                }
                RenderResources::IndexUnmap();
                RenderStats::Frame().streamBytes += indexCount * 4;
            }
            else
            {
                const uint16_t* from = reinterpret_cast<const uint16_t*>(data);
                const uint32_t reset = state.resetIndex & 0xFFFFu;
                if (resetEnabled && reset != 0xFFFFu)
                {
                    // Widened, so the cut index can be the host's.
                    out.indices32 = true;
                    uint32_t* to = reinterpret_cast<uint32_t*>(RenderResources::IndexMap(indexCount * 4, out.indexRingOffset));
                    if (to == nullptr) { Skip(SkipBuffer); return false; }
                    for (uint32_t i = 0; i < indexCount; i++)
                    {
                        const uint32_t index = BSwap16(from[i]);
                        to[i] = index == reset ? 0xFFFFFFFFu : index;
                    }
                    RenderResources::IndexUnmap();
                    RenderStats::Frame().streamBytes += indexCount * 4;
                }
                else
                {
                    uint16_t* to = reinterpret_cast<uint16_t*>(RenderResources::IndexMap(indexCount * 2, out.indexRingOffset));
                    if (to == nullptr) { Skip(SkipBuffer); return false; }
                    Swap16Into(to, from, indexCount);
                    RenderResources::IndexUnmap();
                    RenderStats::Frame().streamBytes += indexCount * 2;
                }
            }
            out.indexed = true;
            out.indexCount = indexCount;
            return true;
        }
        if (!convert)
        {
            // A plain draw: the vertex ids are the indices.
            out.indexed = false;
            out.indexCount = indexCount;
            return true;
        }

        // Quads or a fan: the indices, or the vertex ids, as a triangle list.
        g_indices32.resize(indexCount);
        if (state.sourceSelect == 0)
        {
            const uint8_t* data = Guest::Base + Guest::PhysicalAlias(state.indexBase);
            if (state.wideIndices) Swap32Into(g_indices32.data(), reinterpret_cast<const uint32_t*>(data), indexCount);
            else
            {
                const uint16_t* from = reinterpret_cast<const uint16_t*>(data);
                for (uint32_t i = 0; i < indexCount; i++) g_indices32[i] = BSwap16(from[i]);
            }
        }
        else
            for (uint32_t i = 0; i < indexCount; i++) g_indices32[i] = i;
        static std::vector<uint32_t> list;
        list.clear();
        if (convertQuads)
            for (uint32_t i = 0; i + 3 < indexCount; i += 4)
            {
                const uint32_t q[6] = { g_indices32[i], g_indices32[i + 1], g_indices32[i + 2], g_indices32[i], g_indices32[i + 2], g_indices32[i + 3] };
                list.insert(list.end(), q, q + 6);
            }
        else
            for (uint32_t i = 1; i + 1 < indexCount; i++)
            {
                const uint32_t t[3] = { g_indices32[0], g_indices32[i], g_indices32[i + 1] };
                list.insert(list.end(), t, t + 3);
            }
        if (list.empty()) return false;
        const uint32_t bytes = uint32_t(list.size()) * 4;
        out.indexRingOffset = RenderResources::IndexAppend(list.data(), bytes);
        if (out.indexRingOffset == 0xFFFFFFFFu) { Skip(SkipBuffer); return false; }
        RenderStats::Frame().streamBytes += bytes;
        out.indexed = true;
        out.indices32 = true;
        out.indexCount = uint32_t(list.size());
        return true;
    }

    // The frame log's line for the draw, with its textures and buffers as
    // the registers name them.
    void Log(const RenderState::Snapshot& state, const RenderShaders::Program* vs, const RenderShaders::Program* ps, const DrawCommand& out)
    {
        printf("frame %llu (dump %d): draw %u of %u indices (%s), vte %03X viewport %g %g %g %g z %g %g, target %u %ux%u, scissor %d,%d-%d,%d, "
               "depth %08X blend %08X factor %g %g %g %g mask %X, mode %u, cull %08X, alpha %08X, vs %016llx ps %016llx\n",
            (unsigned long long)RenderInternal::Swaps(), RenderInternal::DumpNumber(), state.primitive, state.indexCount, out.indexed ? "indexed" : "plain",
            state.vteControl, state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3], state.viewport[4], state.viewport[5],
            out.targetCount, out.targetWidth, out.targetHeight, out.scissor[0], out.scissor[1], out.scissor[2], out.scissor[3],
            state.depthControl, state.blendControl[0], state.blendFactor[0], state.blendFactor[1], state.blendFactor[2], state.blendFactor[3], state.colorMask, state.modeControl, state.suScModeControl, state.colorControl,
            (unsigned long long)vs->hash, (unsigned long long)ps->hash);
        for (int stage = 0; stage < 2; stage++)
        {
            const RenderShaders::Program* program = stage == 0 ? ps : vs;
            for (const XenosHlsl::TextureFetch& fetch : program->textureFetches)
            {
                uint32_t w[6];
                RenderState::ReadTextureFetch(fetch.slot, w);
                uint32_t tw, th; bool tr;
                const bool white = RenderResources::TextureFor(w, tw, th, tr) == 1 && !tr;
                printf("   %stexture %u (read as %uD)%s%s: format %u %ux%u at %08X%s swizzle %03X signs %02X, words %08X %08X %08X %08X %08X %08X\n", stage == 0 ? "" : "vs ", fetch.slot, fetch.dimension == 3 ? 6 : fetch.dimension + 1, white ? " WHITE: " : "", white ? RenderResources::WhiteReason() : "", w[1] & 0x3F,
                    (w[2] & 0x1FFF) + 1, ((w[2] >> 13) & 0x1FFF) + 1, w[1] & 0xFFFFF000u, RenderResources::ResolvedAt(w[1] & 0xFFFFF000u) ? " (resolved)" : "",
                    (w[3] >> 1) & 0xFFF, (w[0] >> 2) & 0xFF, w[0], w[1], w[2], w[3], w[4], w[5]);
            }
        }
        for (const XenosHlsl::VertexFetch& fetch : vs->vertexFetches)
        {
            uint32_t word0, word1;
            RenderState::ReadVertexFetch(fetch.slot, word0, word1);
            printf("   vertex slot %u at %08X, %u dwords, endian %u, stride %u\n", fetch.slot, word0 & ~3u, (word1 >> 2) & 0xFFFFFF, word1 & 3, fetch.stride);
        }
        fflush(stdout);
    }
}

namespace
{
    // COD3_DRAWLOG=path: one line a draw, every frame, to that file: the
    // swap, the draw's number in it, the pass (bin select), the programs,
    // the primitive and count, the first vertex buffer with a sampled look
    // at its memory, a look at the indices and at the vertex program's
    // 256 constants, and the depth and blend state. Frames
    // are then compared draw by draw, to find what one frame did that its
    // neighbours did not.
    uint64_t SampleHash(uint32_t physical, uint32_t bytes)
    {
        if (physical == 0 || bytes == 0 || bytes > (64u << 20)) return 0;
        const uint8_t* data = Guest::Base + Guest::PhysicalAlias(physical);
        uint64_t hash = 1469598103934665603ull ^ bytes;
        const uint32_t step = std::max<uint32_t>(bytes / 64, 4) & ~3u;
        for (uint32_t at = 0; at + 4 <= bytes; at += step)
        {
            uint32_t word;
            memcpy(&word, data + at, 4);
            hash = (hash ^ word) * 1099511628211ull;
        }
        return hash;
    }

    void DumpVertices(const RenderState::Snapshot& state, const RenderShaders::Program* vs, const DrawCommand& out)
    {
        printf("debug: vs %016llx, primitive %u, %u indices, depth control %08X info %08X, view %p, vte %03X, viewport z %g %g, poly offset %08X\n",
            (unsigned long long)vs->hash, state.primitive, state.indexCount, state.depthControl, state.depthInfo, (void*)out.depthView,
            state.vteControl, state.viewport[4], state.viewport[5], state.suScModeControl);
        for (const XenosHlsl::VertexFetch& fetch : vs->vertexFetches)
        {
            uint32_t word0, word1;
            RenderState::ReadVertexFetch(fetch.slot, word0, word1);
            const uint32_t at = word0 & ~3u, bytes = ((word1 >> 2) & 0xFFFFFF) * 4;
            printf("  slot %u at %08X, %u bytes:", fetch.slot, at, bytes);
            for (uint32_t i = 0; i < bytes / 4 && i < 48; i++)
            {
                const uint32_t word = Guest::Read32(Guest::Base, Guest::PhysicalAlias(at + i * 4));
                float f;
                memcpy(&f, &word, 4);
                printf(" %08X(%g)", word, f);
            }
            printf("\n");
            break;
        }
        fflush(stdout);
    }

    void DrawLog(const RenderState::Snapshot& state, const RenderShaders::Program* vs, const RenderShaders::Program* ps, const DrawCommand& out)
    {
        static const char* const path = getenv("COD3_DRAWLOG");
        if (path == nullptr) return;
        // COD3_DRAWLOG_FROM / _UNTIL: only the swaps in that range. The lines
        // are kept in memory and written when the range is over, so the log
        // slows the frames it covers as little as it can.
        static const uint64_t from = []() { const char* t = getenv("COD3_DRAWLOG_FROM"); return t ? uint64_t(strtoull(t, nullptr, 10)) : 0ull; }();
        static const uint64_t until = []() { const char* t = getenv("COD3_DRAWLOG_UNTIL"); return t ? uint64_t(strtoull(t, nullptr, 10)) : ~0ull; }();
        static std::string lines;
        static bool written = false;
        const uint64_t swap = RenderInternal::Swaps();
        if (written) return;
        if (swap >= until || lines.size() > (512u << 20))
        {
            if (FILE* f = fopen(path, "wb")) { fwrite(lines.data(), 1, lines.size(), f); fclose(f); }
            printf("render: the draw log went to %s, %zu bytes\n", path, lines.size());
            fflush(stdout);
            lines.clear(); lines.shrink_to_fit();
            written = true;
            return;
        }
        if (swap < from) return;
        if (lines.empty()) lines.reserve(64u << 20);
        static uint64_t lastSwap = ~0ull;
        static uint32_t number = 0;
        if (swap != lastSwap) { lastSwap = swap; number = 0; }
        uint32_t vb = 0, vbBytes = 0;
        if (!vs->vertexFetches.empty())
        {
            uint32_t word0, word1;
            RenderState::ReadVertexFetch(vs->vertexFetches[0].slot, word0, word1);
            vb = word0 & ~3u;
            vbBytes = ((word1 >> 2) & 0xFFFFFF) * 4;
        }
        const std::atomic<uint32_t>* registers = Gpu::RegisterFile();
        uint64_t vsConstants = 1469598103934665603ull, psConstants = 1469598103934665603ull, textures = 1469598103934665603ull;
        for (uint32_t i = 0; i < 1024; i++) vsConstants = (vsConstants ^ registers[0x4000 + i].load(std::memory_order_relaxed)) * 1099511628211ull;
        for (uint32_t i = 0; i < 1024; i++) psConstants = (psConstants ^ registers[0x4400 + i].load(std::memory_order_relaxed)) * 1099511628211ull;
        for (const XenosHlsl::TextureFetch& fetch : ps->textureFetches)
        {
            uint32_t w[6];
            RenderState::ReadTextureFetch(fetch.slot, w);
            const uint32_t base = w[1] & 0xFFFFF000u;
            textures = (textures ^ (base | (RenderResources::ResolvedAt(base) ? 1u : 0u))) * 1099511628211ull;
        }
        // Every vertex stream the program reads, each hashed whole when it
        // is small: a second stream (a model's lighting, say) is as likely
        // to be what changes as the first.
        uint64_t streams = 1469598103934665603ull;
        for (const XenosHlsl::VertexFetch& fetch : vs->vertexFetches)
        {
            uint32_t word0, word1;
            RenderState::ReadVertexFetch(fetch.slot, word0, word1);
            const uint32_t at = word0 & ~3u, bytes = ((word1 >> 2) & 0xFFFFFF) * 4;
            uint64_t h = (at ^ 0x9E3779B97F4A7C15ull) * 1099511628211ull;
            if (at != 0 && bytes != 0 && bytes <= (256u << 10))
            {
                const uint8_t* data = Guest::Base + Guest::PhysicalAlias(at);
                for (uint32_t i = 0; i + 8 <= bytes; i += 8) { uint64_t w; memcpy(&w, data + i, 8); h = (h ^ w) * 1099511628211ull; }
            }
            else h ^= SampleHash(at, bytes);
            streams = (streams ^ h) * 1099511628211ull;
        }
        const uint32_t indexBytes = out.indexed ? state.indexCount * 4 : 0;
        char line[512];
        const int n = snprintf(line, sizeof(line), "%llu %u %llx %016llx %016llx p%u n%u %c vb %08X %u %016llx ib %016llx c %016llx d %08X b %08X m %X t %u pc %016llx tx %016llx vs %016llx %u\n",
            (unsigned long long)swap, number++, (unsigned long long)Gpu::BinSelect(), (unsigned long long)vs->hash, (unsigned long long)ps->hash,
            state.primitive, state.indexCount, out.indexed ? 'i' : 'p', vb, vbBytes, (unsigned long long)SampleHash(vb, vbBytes),
            (unsigned long long)(out.indexed ? SampleHash(state.indexBase, indexBytes) : 0), (unsigned long long)vsConstants,
            state.depthControl, state.blendControl[0], state.colorMask, out.targetCount, (unsigned long long)psConstants, (unsigned long long)textures,
            (unsigned long long)streams, unsigned(vs->vertexFetches.size()));
        if (n > 0) lines.append(line, size_t(n));
        // COD3_DRAWLOG_PSDUMP=hash: the first draw of a frame with that pixel
        // program adds its 256 pixel constants, every word, to the log.
        static const uint64_t psDump = []() { const char* t = getenv("COD3_DRAWLOG_PSDUMP"); return t ? uint64_t(strtoull(t, nullptr, 16)) : 0ull; }();
        static uint64_t dumpedSwap = ~0ull;
        if (psDump != 0 && ps->hash == psDump && dumpedSwap != swap)
        {
            dumpedSwap = swap;
            char word[16];
            lines += "PSDUMP " + std::to_string(swap);
            for (uint32_t i = 0; i < 1024; i++) { snprintf(word, sizeof(word), " %08X", registers[0x4400 + i].load(std::memory_order_relaxed)); lines += word; }
            lines += "\n";
        }
    }
}

namespace RenderCommands { bool PrepareDrawInner(uint32_t initiator, uint32_t indexBase, RenderState::DrawCommand& out); }

void RenderCommands::DumpDraws(const char* why, int count)
{
    if (g_dumpLeft.load(std::memory_order_relaxed) > 0) return;
    g_dumpHud = count < 0;
    if (g_dumpHud) count = -count;
    g_dumpScan = 8000;
    printf("render: draw list (%s), the next %d %sdraws: index, vs, ps, primitive, indices, colour mask, depth control, stencil, blend 0, scissor, result\n",
           why, count, g_dumpHud ? "flat (no depth test) " : "");
    fflush(stdout);
    g_dumpIndex = 0;
    g_dumpLeft.store(count, std::memory_order_relaxed);
}

bool RenderCommands::PrepareDraw(uint32_t initiator, uint32_t indexBase, DrawCommand& out)
{
    if (g_dumpLeft.load(std::memory_order_relaxed) <= 0) return PrepareDrawInner(initiator, indexBase, out);
    uint64_t before[SkipCount];
    memcpy(before, g_skips, sizeof before);
    g_dumpVs = g_dumpPs = 0;
    g_dumpState = RenderState::Snapshot();
    const bool drawn = PrepareDrawInner(initiator, indexBase, out);
    const char* result = drawn ? "drawn" : "nothing";
    for (int i = 0; i < SkipCount; i++) if (g_skips[i] != before[i]) result = SkipNames[i];
    const RenderState::Snapshot& s = g_dumpState;
    if (g_dumpHud && (s.depthControl & 2u))
    {
        g_dumpIndex++;
        if (--g_dumpScan <= 0) { g_dumpLeft.store(0, std::memory_order_relaxed); printf("render: draw list done (8000 draws looked at)\n"); fflush(stdout); }
        return drawn;
    }
    printf("  draw %d: vs %016llx ps %016llx prim %u n %u mask %08X depth %08X stencil %08X blend %08X scissor %08X %08X: %s\n", g_dumpIndex++,
        (unsigned long long)g_dumpVs, (unsigned long long)g_dumpPs, s.primitive, s.indexCount, s.colorMask, s.depthControl, s.stencilRefMask,
        s.blendControl[0], s.scissorTopLeft, s.scissorBottomRight, result);
    if (g_dumpLeft.fetch_sub(1, std::memory_order_relaxed) == 1) { printf("render: draw list done\n"); fflush(stdout); }
    return drawn;
}

bool RenderCommands::PrepareDrawInner(uint32_t initiator, uint32_t indexBase, DrawCommand& out)
{
    SkipFrameCheck();
    RenderStats::Enter(RenderStats::SectionState);
    RenderState::Snapshot state;
    RenderState::Read(initiator, indexBase, state);
    if (g_dumpLeft.load(std::memory_order_relaxed) > 0) g_dumpState = state;
    if (state.indexCount == 0) return false;
    if (state.modeControl == 0) { Skip(SkipBackendOff); return false; }
    // EDRAM mode 5 is drawn as colour and depth like mode 4: Xenia calls
    // it "depth only", but this title draws its ground, tents and jeeps
    // in it with programs that fetch the shadow maps.

    bool convertQuads = false, convertFan = false;
    switch (state.primitive)
    {
    case RenderState::Points:
    {
        // The host draws a point as one pixel; a wider one is noted.
        out.topology = D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
        static int announced = 0;
        if (((state.pointSize & 0xFFFF) > 8 || (state.pointSize >> 16) > 8) && (announced < 4 && announced++ < 4))
            printf("render: a point list of %u with a point size of %u by %u eighths of a pixel, drawn as pixels\n", state.indexCount, state.pointSize & 0xFFFF, state.pointSize >> 16);
        break;
    }
    case RenderState::Lines: out.topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST; break;
    case RenderState::LineStrip: out.topology = D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP; break;
    case RenderState::Triangles: out.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; break;
    case RenderState::TriangleFan: out.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; convertFan = true; break;
    case RenderState::TriangleStrip: out.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP; break;
    case RenderState::Rectangles: out.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; out.rectangles = true; break;
    case RenderState::Quads: out.topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST; convertQuads = true; break;
    default: Skip(SkipPrimitive); return false;
    }

    const RenderShaders::Program* vs;
    const RenderShaders::Program* ps;
    if (!Programs(out, vs, ps)) return false;
    if (DepthOnlyMode(state))
    {
        static const bool shadowCutForced = getenv("COD3_SHADOWDEPTHCUT") != nullptr;
        const bool shadowCut = shadowCutForced || !g_fastShadowMaps;
        out.ps = nullptr;
        // Only a shadow map's caster goes without the depth cut: its
        // viewport is square (512 or 1024 a side). A depth only draw into
        // the frame's own surface (a depth pass ahead of the colour pass)
        // keeps the cut, or its depth and the colour pass's would not
        // agree and the surfaces drawn over it would flicker.
        const float vx = std::fabs(state.viewport[0]), vy = std::fabs(state.viewport[2]);
        const bool squareViewport = vx >= 64.0f && std::fabs(vx - vy) < 1.0f;
        out.pixelShader = (!shadowCut && state.colorMask == 0 && squareViewport) ? NoPixelProgramFast : NoPixelProgram;
    }
    if (state.pitch == 0) { Skip(SkipPitch); return false; }

    RenderStats::Enter(RenderStats::SectionTargets);
    float scaleY = 1.0f;
    if (!Targets(state, vs, ps, out, scaleY)) return false;

    RenderStats::Enter(RenderStats::SectionPipeline);
    // The polygon offset. Its constant part (PA_SU_POLY_OFFSET_*_OFFSET) is
    // in depth units: the title writes 0.0001 for the layers it lays over a
    // surface (the mud, the ivy, the decals, drawn with a greater or equal
    // test and no depth write) and nought for the surfaces themselves, and
    // the console adds it to the depth as it is. The host's bias counts in
    // steps of the depth's own precision, which on a float depth buffer
    // shrinks with the depth: with this title's depth running from one near
    // to nought far, a layer fifty units away was lifted an eighth of what
    // it should, lost to its surface in patches, and drew as ragged holes.
    // So the constant part goes into the vertex program's depth (the w of
    // the viewport offset constant) and the rasterizer keeps the slope's.
    // COD3_OLDPOLYOFFSET=1 gives the host the constant part again.
    static const bool oldOffset = getenv("COD3_OLDPOLYOFFSET") != nullptr;
    static const bool noOffset = getenv("COD3_NOPOLYOFFSET") != nullptr;
    float depthOffset = 0.0f;
    float slopeOnly[4] = { state.polyOffset[0], 0.0f, state.polyOffset[2], 0.0f };
    if (!oldOffset && !noOffset)
    {
        if ((state.suScModeControl >> 11) & 1) depthOffset = state.polyOffset[1];
        else if ((state.suScModeControl >> 12) & 1) depthOffset = state.polyOffset[3];
    }
    // The Z pass (the depth only mode with its colour mask left open; the
    // shadow maps close theirs) is put a little behind. The colour pass
    // tests against it (greater or equal) and finds its own surfaces there
    // again, but with MSAA a translated pixel program gives one depth, cut
    // to the console's steps at the covered samples' centroid, to all of a
    // pixel's samples, where the Z pass has each sample's own: on steep
    // ground half a pixel's samples failed and kept an old frame's colour
    // (dotted lines along the triangles' edges; white speckles everywhere
    // without MSAA, where the cut alone loses to the Z pass's last bits),
    // and cutting the Z pass's depth the same way instead left whole
    // patches of a stale colour where the two passes' triangles cover a
    // pixel's samples differently. Behind by two pixels of slope and a few
    // steps, the Z pass still hides what is truly behind and never its own
    // surface. COD3_ZPASSBEHIND=0 leaves it where it is.
    static const bool behindOff = []() { const char* t = getenv("COD3_ZPASSBEHIND"); return t != nullptr && t[0] == '0'; }();
    const bool behind = !behindOff && DepthOnlyMode(state) && state.colorMask != 0;
    // The layers laid over a surface (the decals: pixel program
    // d6d768427524ea61 and the like): tested greater or equal, writing no
    // depth, blended. Put a little nearer (render_pipeline.cpp), so they no
    // longer flicker against the surface under them. F11's "Steady decals";
    // COD3_NODECALNUDGE=1 leaves them, to compare.
    static const bool nudgeOff = getenv("COD3_NODECALNUDGE") != nullptr;
    const uint32_t depthFunction = (state.depthControl >> 4) & 7;
    const bool forward = !nudgeOff && g_steadyDecals && out.depthView != nullptr && !DepthOnlyMode(state) &&
                         ((state.depthControl >> 1) & 1) != 0 && ((state.depthControl >> 2) & 1) == 0 && depthFunction == 6 &&
                         state.blendControl[0] != 0x00010001;
    out.rasterizer = RenderPipeline::Rasterizer(state.suScModeControl, true, out.rectangles, oldOffset ? state.polyOffset : slopeOnly, behind, forward);
    // Verify mode: every such layer (whether or not it is nudged) counted on
    // the GPU, how many of its samples passed (render_d3d11.cpp).
    out.layerQuery = Verify::On(Verify::Buffers) && out.depthView != nullptr && !DepthOnlyMode(state) &&
                     ((state.depthControl >> 1) & 1) != 0 && ((state.depthControl >> 2) & 1) == 0 && depthFunction == 6 &&
                     state.blendControl[0] != 0x00010001;
    out.blend = RenderPipeline::Blend(state.blendControl, state.colorMask);
    out.depth = RenderPipeline::Depth(out.depthView ? state.depthControl : 0, state.stencilRefMask);
    memcpy(out.blendFactor, state.blendFactor, sizeof(out.blendFactor));
    // A factor that is the constant colour's alpha: Direct3D multiplies the
    // colour channels by the factor's rgb, so the alpha goes there. (The
    // title's bloom and its exposure pass blend by the constant alpha, and
    // with the rgb of a factor that is (0,0,0,1) they came out as nothing
    // at all.)
    if (RenderPipeline::BlendTakesConstantAlpha(state.blendControl))
        out.blendFactor[0] = out.blendFactor[1] = out.blendFactor[2] = state.blendFactor[3];
    out.stencilReference = state.stencilRefMask & 0xFF;
    // The bloom's composite: one full screen draw of this pixel program,
    // adding the blurred bright parts (an 886 pixel square at 4K) to the
    // frame by the constant colour, right before the HUD (seen with the
    // RenoDX devkit: draw 2148 of 2180, then the HUD's from 2149). Its
    // strength is the HDR tab's bloom slider, in SDR too; and in the HDR
    // mode the frame is tone mapped right before it (render_d3d11.cpp's
    // HdrScenePass) and the bloom added over the tone mapped picture, so the
    // halos stay the game's and the HUD is drawn over the finished picture.
    // COD3_HDRSCENEPS=hash names another program for the place.
    {
        static const uint64_t composite = []() { const char* t = getenv("COD3_HDRSCENEPS"); return t ? uint64_t(strtoull(t, nullptr, 16)) : 0xfe6578445de135eaull; }();
        const bool adds = ((state.blendControl[0] >> 8) & 0x1F) == 1;   // the destination's factor one
        if (ps != nullptr && ps->hash == composite && adds && out.targetCount > 0 && out.targets[0] != nullptr)
        {
            for (int i = 0; i < 3; i++) out.blendFactor[i] *= g_bloom;
            out.hdrScene = g_hdrScene && (out.upgradedTargets & 1) != 0;
            if (out.hdrScene) g_sceneSwap = RenderInternal::Swaps();
        }
    }
    // Only the opaque draws may go flat: a blended one in a flat colour
    // with an alpha of one would cover the frame.
    out.opaque = state.blendControl[0] == 0x00010001;

    // The draw's constants: the viewport the vertex program folds in, the
    // alpha test, and what the textures below fill in.
    RenderState::DrawConstants& constants = g_constants;
    constants.viewportScale[0] = state.viewport[0]; constants.viewportOffset[0] = state.viewport[1];
    constants.viewportScale[1] = state.viewport[2]; constants.viewportOffset[1] = state.viewport[3];
    constants.viewportScale[2] = state.viewport[4]; constants.viewportOffset[2] = state.viewport[5];
    constants.viewportOffset[3] = depthOffset;
    constants.targetSize[0] = float(state.pitch);
    constants.targetSize[1] = float(out.targetHeight) / scaleY;
    constants.targetSize[2] = 1.0f / float(state.pitch);
    constants.targetSize[3] = scaleY / float(out.targetHeight);
    // The viewport control, and in bit 16 where the console's pixel centres
    // are: at whole coordinates (PA_SU_VTX_CNTL bit 0 clear, as this title
    // has it), half a pixel up and left of the host's, so the vertex program
    // moves the geometry half a pixel the other way. COD3_NOHALFPIXEL=1 for
    // the host's centres, to compare.
    static const bool noHalfPixel = getenv("COD3_NOHALFPIXEL") != nullptr;
    constants.flags[0] = state.vteControl | (((state.vertexControl & 1) != 0 || noHalfPixel) ? 0x10000u : 0u);
    constants.flags[1] = ((state.colorControl >> 3) & 1) ? (state.colorControl & 7) : 7;   // the alpha test, or always
    constants.flags[2] = state.alphaReference;
    constants.flags[3] = uint32_t(ps->hash);
    // Alpha to coverage for an alpha tested draw into a multisampled target
    // (xenos_hlsl.cpp's alpha test). COD3_NOCOVERAGE=1 leaves it out.
    {
        static const bool noCoverageForced = getenv("COD3_NOCOVERAGE") != nullptr;
        const bool noCoverage = noCoverageForced || !g_foliageCoverage;
        const uint32_t samples = RenderResources::TargetSamples(state.pitch);
        constants.coverage[0] = (!noCoverage && samples > 1 && out.targetCount > 0 && constants.flags[1] != 7) ? samples : 0u;
        constants.coverage[1] = out.upgradedTargets;
        constants.coverage[2] = constants.coverage[3] = 0;
    }
    // The index offset (VGT_INDX_OFFSET), added to each index by the
    // vertex program itself (xenos_hlsl.cpp: coverage.z), not by the draw
    // call's base vertex. Whether Direct3D 11's SV_VertexID carries the base
    // vertex is not something every driver agrees on, and where it did not
    // the vertices were fetched from the start of the buffer. The title's
    // dynamic pools (the bullet holes, the tracers) are drawn at a growing
    // offset into two pools taken in turn, and each frame drew whatever
    // stale geometry lay at the pool's start: the holes blinked (one pool's
    // start covered 29 pixels of the screen, the other's 3526, for the same
    // draw), and old tracers stayed hanging where they had been.
    constants.coverage[2] = uint32_t(state.indexOffset);
    // The HDR mode's HUD: what is drawn into the frame after the scene pass
    // has tone mapped it (the HUD, the menus over a level) has its colour
    // scaled (coverage.w, a float; nought for none), so the HUD's white,
    // which the title draws at about 86%, comes out at the UI brightness.
    constants.coverage[3] = 0;
    if (g_hudGain > 0.0f && g_sceneSwap == RenderInternal::Swaps() && !out.hdrScene && (out.upgradedTargets & 1) != 0)
        memcpy(&constants.coverage[3], &g_hudGain, 4);
    // A draw with the alpha test off takes its pixel program's variant
    // without it (render_shaders.cpp), once built: the same picture, and
    // the GPU's early depth test whole for it.
    if (constants.flags[1] == 7 && out.ps != nullptr && ps != nullptr && out.pixelShader < RenderShaders::OpaqueVariantBit && Tuning::On(Tuning::opaqueShaders))
    {
        if (ID3D11DeviceChild* opaque = ps->opaque.load(std::memory_order_acquire))
        {
            out.ps = static_cast<ID3D11PixelShader*>(opaque);
            out.pixelShader |= RenderShaders::OpaqueVariantBit;
        }
    }
    // Snapshot the main world pass, independent of particle visibility.
    // Mesh programs transform local positions to world first; c12..15 is
    // their world-view-projection, c16..18 is the separate model transform.
    out.bloodFrame=RenderInternal::Swaps();
    const bool mainView=std::fabs(state.viewport[0])>std::fabs(state.viewport[2])*1.2f &&
        std::fabs(state.viewport[0])>=320 && !(state.vteControl&0x100);
    out.bloodScene=mainView && out.depthView && out.targets[0] && (state.depthControl&2);
    out.bloodWorldStage=out.bloodScene && (state.colorMask&15)==15 && out.opaque && (state.depthControl&4);
    const auto hasRow=[&](uint16_t row){return vs && (vs->constantMap.empty() || std::find(vs->constantMap.begin(),vs->constantMap.end(),row)!=vs->constantMap.end());};
    out.bloodProjectionValid=out.bloodWorldStage && hasRow(12)&&hasRow(13)&&hasRow(14)&&hasRow(15)&&hasRow(16)&&hasRow(17)&&hasRow(18);
    if(BloodDiag::On()){
 static uint64_t frame=~uint64_t(0);static int world=0,valid=0,full=0;static uint64_t lastVs=0;static float eyeW=0,wLength=0;
 if(frame!=out.bloodFrame){if(world)BloodDiag::Log("prepare frame=%llu world_draws=%d valid_projections=%d full_constant_file_draws=%d selected_vs=%016llX eye_w=%.4f w_axis_length=%.4f",frame,world,valid,full,lastVs,eyeW,wLength);frame=out.bloodFrame;world=valid=full=0;}
 if(out.bloodWorldStage){++world;valid+=out.bloodProjectionValid;full+=vs&&vs->constantMap.empty();}
 if(out.bloodProjectionValid){lastVs=vs->hash;float row[4];memcpy(row,RenderState::Registers()+0x4000+15*4,sizeof(row));float eye[3]{};BloodDiagnosticEye(eye);eyeW=row[0]*eye[0]+row[1]*eye[1]+row[2]*eye[2]+row[3];wLength=sqrtf(row[0]*row[0]+row[1]*row[1]+row[2]*row[2]);}


 }
 if(out.bloodProjectionValid){
        auto& p=out.bloodProjection;
        memcpy(p.matrix,RenderState::Registers()+0x4000+12*4,sizeof(p.matrix));
        const float nearPlane=BloodRender::NearPlane(p.matrix);
        // Short-range matrices also expose c12..18, but clip world decals.
        out.bloodProjectionValid=nearPlane>=1.0f;
        float ww=p.matrix[12]*p.matrix[12]+p.matrix[13]*p.matrix[13]+p.matrix[14]*p.matrix[14];
        float a=ww>0?(p.matrix[8]*p.matrix[12]+p.matrix[9]*p.matrix[13]+p.matrix[10]*p.matrix[14])/ww:0;
        float b=p.matrix[11]-a*p.matrix[15];
        p.diagShader=vs->hash;p.diagFrame=out.bloodFrame;p.diagDepth=state.depthControl;p.diagMask=state.colorMask;p.diagPass=DepthOnlyMode(state)?1:0;p.diagNear=nearPlane;p.diagFar=a!=0?-b/a:INFINITY;
        if(BloodDiag::On()){
          static uint64_t frame=~uint64_t(0);static int accepted=0,rejected=0;static uint64_t badVs=0;static float badNear=0;
          if(frame!=out.bloodFrame){BloodDiag::Log("selection_summary frame=%llu accepted=%d rejected_short_range=%d rejected_vs=%016llX rejected_near=%.6f reason=near_below_1_or_invalid_perspective",frame,accepted,rejected,badVs,badNear);frame=out.bloodFrame;accepted=rejected=0;}
          if(out.bloodProjectionValid)++accepted;else{++rejected;badVs=vs->hash;badNear=nearPlane;}
        }
        memcpy(p.scale,constants.viewportScale,sizeof(p.scale));memcpy(p.offset,constants.viewportOffset,sizeof(p.offset));
        memcpy(p.target,constants.targetSize,sizeof(p.target));memcpy(p.flags,constants.flags,sizeof(p.flags));
    }

    // The pixel's own position, which the console writes into a register
    // of the pixel program when the program control asks (bit 18), the
    // register named by the context's bits 8 to 15. The soft particles
    // read the depth under them there; without it they read the corner
    // of the depth, and the smoke came and went with the tree's leaves.
    {
        uint32_t hostWidth, hostHeight;
        float scaleX, scaleYAgain;
        RenderResources::TargetSize(state.pitch, 4, hostWidth, hostHeight, scaleX, scaleYAgain);
        constants.pixelGen[0] = scaleX > 0.0f ? 1.0f / scaleX : 1.0f;
        constants.pixelGen[1] = scaleYAgain > 0.0f ? 1.0f / scaleYAgain : 1.0f;
        constants.pixelGen[2] = float((state.contextMisc >> 8) & 0xFF);
        constants.pixelGen[3] = ((state.programControl >> 18) & 1) ? 1.0f : 0.0f;
    }

    // COD3_SKIPPS=hash: the draws of that pixel program are left out, to
    // see what a pass (the fog, say) puts over the picture.
    static const uint64_t skipPixel = []() { const char* t = getenv("COD3_SKIPPS"); return t ? uint64_t(strtoull(t, nullptr, 16)) : 0ull; }();
    if (skipPixel != 0 && ps != nullptr && ps->hash == skipPixel) return false;
    // The shell shock's black flash: right after an explosion close by (a
    // bazooka at one's feet) the title draws one full screen pass with this
    // pixel program over the whole frame, before the HUD, for its first
    // frames. It should show a blurred copy of the scene; here the copy is
    // black, and the pass wipes the picture to black for a moment. Left out
    // unless "Shell shock screen blur" is ticked in F11 (found from F12
    // captures of the shock: present at its first and eighth frame, gone by
    // its twentieth, and the frame black from that draw on).
    if (!g_shellShockBlur && ps != nullptr && ps->hash == 0x5fb07e1b8502496aull) return false;
    // debug.txt's skip_vs and skip_ps, and dump_vs: what a vertex program's
    // draws are given, the vertices as the title wrote them.
    {
        const uint64_t skipVs = DebugProbe::SkipVs(), skipPs = DebugProbe::SkipPs();
        if ((skipVs != 0 && vs->hash == skipVs) || (skipPs != 0 && ps != nullptr && ps->hash == skipPs)) return false;
        if (DebugProbe::DumpVs() == vs->hash && DebugProbe::TakeDump()) DumpVertices(state, vs, out);
    }

    RenderStats::Enter(RenderStats::SectionTextures);
    Textures(vs, ps, out, constants);

    RenderStats::Enter(RenderStats::SectionBuffers);
    if (!Buffers(vs, out)) return false;

    RenderStats::Enter(RenderStats::SectionConstants);
    if (!Constants(vs, ps, out, constants)) return false;

    RenderStats::Enter(RenderStats::SectionIndices);
    if (!Indices(state, convertQuads, convertFan, out)) return false;
    out.baseVertex = 0;   // added by the vertex program (coverage.z, above)
    if (!g_rangeLooks.empty() || !g_verifyBuffers.empty())
    {
        RenderStats::Enter(RenderStats::SectionBuffers);
        if (!g_rangeLooks.empty()) LookRanges(state, out);
        if (!g_verifyBuffers.empty()) VerifyDrawBuffers(state, out, ps != nullptr ? ps->hash : 0);
        if (!g_verifyBuffers.empty()) out.layerVertices = g_verifyBuffers.front().physical;
        // Verify mode: the bullet holes' draws (or COD3_LAYERPS's) written out
        // whole, the first 120: what of their depth could differ from one
        // frame to the next (their two pools give different sample counts).
        static const uint64_t layerPs = []() { const char* t = getenv("COD3_LAYERPS"); return t ? uint64_t(strtoull(t, nullptr, 16)) : 0xd6d768427524ea61ull; }();
        static int layerTold = 0;
        if (out.layerQuery && ps != nullptr && ps->hash == layerPs && layerTold < 120 && !g_verifyBuffers.empty() && state.sourceSelect == 0)
        {
            layerTold++;
            const RangeLook& look = g_verifyBuffers.front();
            float vc[16];
            for (int i = 0; i < 16; i++) { const uint32_t w = RenderState::Register(RenderState::VertexConstants + i); memcpy(&vc[i], &w, 4); }
            printf("verify: layer draw, frame %llu, pool %08X, indices at %08X x%u (wide %d), offset %d, min %u max %u, sc %08X, poly %g %g %g %g, vte %03X, depth %08X, vp %g %g %g %g %g %g\n",
                (unsigned long long)RenderInternal::Swaps(), look.physical, state.indexBase, state.indexCount, state.wideIndices ? 1 : 0, state.indexOffset,
                state.minVertexIndex, state.maxVertexIndex, state.suScModeControl, state.polyOffset[0], state.polyOffset[1], state.polyOffset[2], state.polyOffset[3],
                state.vteControl, state.depthControl, state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3], state.viewport[4], state.viewport[5]);
            printf("   c0-3: %g %g %g %g | %g %g %g %g | %g %g %g %g | %g %g %g %g\n", vc[0], vc[1], vc[2], vc[3], vc[4], vc[5], vc[6], vc[7],
                vc[8], vc[9], vc[10], vc[11], vc[12], vc[13], vc[14], vc[15]);
            const uint8_t* indexData = Guest::Base + Guest::PhysicalAlias(state.indexBase);
            printf("   vertices:");
            for (uint32_t i = 0; i < state.indexCount && i < 12; i++)
            {
                const uint32_t index = state.wideIndices ? _byteswap_ulong(reinterpret_cast<const uint32_t*>(indexData)[i])
                                                         : BSwap16(reinterpret_cast<const uint16_t*>(indexData)[i]);
                const int64_t vertex = int64_t(index) + state.indexOffset;
                if (vertex < 0 || vertex >= 0x8000) { printf(" [%u ?]", index); continue; }
                const uint8_t* at = Guest::Base + Guest::PhysicalAlias(look.physical + uint32_t(vertex) * look.stride * 4);
                float xyz[3];
                for (int k = 0; k < 3; k++) { const uint32_t w = _byteswap_ulong(reinterpret_cast<const uint32_t*>(at)[k]); memcpy(&xyz[k], &w, 4); }
                const uint32_t colour = _byteswap_ulong(reinterpret_cast<const uint32_t*>(at)[5]);
                printf(" [%u: %.3f %.3f %.3f %08X]", index, xyz[0], xyz[1], xyz[2], colour);
            }
            printf("\n");
            fflush(stdout);
        }
        RenderStats::Enter(RenderStats::SectionIndices);
    }
    IndexCheck(state, vs, out);
    switch (out.topology)
    {
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST: out.triangles = out.rectangles ? out.indexCount / 3 * 2 : out.indexCount / 3; break;
    case D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP: out.triangles = out.indexCount >= 2 ? out.indexCount - 2 : 0; break;
    default: break;
    }

    if (RenderInternal::FrameLogged())
    {
        RenderStats::Enter(RenderStats::SectionLog);
        out.logged = true;
        Log(state, vs, ps, out);
    }
    DrawLog(state, vs, ps, out);
    return true;
}

bool RenderCommands::PrepareResolve(ResolveCommand& out)
{
    RenderState::ResolveSnapshot state;
    RenderState::ReadResolve(state);
    if (state.pitch == 0) return false;

    const uint32_t x0 = state.scissorTopLeft & 0x7FFF, y0 = (state.scissorTopLeft >> 16) & 0x7FFF;
    const uint32_t x1 = state.scissorBottomRight & 0x7FFF, y1 = (state.scissorBottomRight >> 16) & 0x7FFF;
    if (x1 <= x0 || y1 <= y0 || x1 - x0 > 4096 || y1 - y0 > 4096) return false;
    const uint32_t width = x1 - x0, height = y1 - y0;

    const uint32_t sourceSelect = state.control & 7;
    const uint32_t command = (state.control >> 20) & 3;
    const bool colorClear = ((state.control >> 8) & 1) != 0;
    const bool depthClear = ((state.control >> 9) & 1) != 0;
    RenderStats::Frame().resolves++;
    if (RenderResources::EnsureRows(state.pitch, y1)) out.bindingsLost = true;

    uint32_t targetWidth, targetHeight;
    float scaleX, scaleY;
    RenderResources::TargetSize(state.pitch, 4, targetWidth, targetHeight, scaleX, scaleY);
    const float hostX0 = x0 * scaleX, hostY0 = y0 * scaleY;
    const uint32_t hostWidth = std::max(1u, uint32_t(std::lround(width * scaleX)));
    const uint32_t hostHeight = std::max(1u, uint32_t(std::lround(height * scaleY)));

    if (RenderInternal::FrameLogged())
        printf("frame: resolve control %08X to %08X, %u,%u-%u,%u, pitch %u, clears %d%d, colour %08X depth %08X\n",
            state.control, state.destBase, x0, y0, x1, y1, state.pitch, colorClear ? 1 : 0, depthClear ? 1 : 0, state.colorInfo[0], state.depthInfo);
    // COD3_RESOLVELOG=1: every five seconds, how many resolves went where.
    static const bool resolveLog = getenv("COD3_RESOLVELOG") != nullptr;
    if (resolveLog)
    {
        static std::map<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>, uint64_t> where;
        static auto told = std::chrono::steady_clock::now();
        where[{ state.control, state.destBase, width, height }]++;
        if (std::chrono::steady_clock::now() - told > std::chrono::seconds(5))
        {
            told = std::chrono::steady_clock::now();
            printf("resolves, five seconds:");
            for (const auto& [key, count] : where)
                printf(" %08X>%08X %ux%u x%llu;", std::get<0>(key), std::get<1>(key), std::get<2>(key), std::get<3>(key), (unsigned long long)count);
            printf("\n");
            fflush(stdout);
            where.clear();
        }
    }

    if (DebugProbe::TakeResolveLog())
    {
        printf("debug: resolve control %08X to %08X, %u,%u-%u,%u, dest info %08X pitch %08X, surface pitch %u, colour %08X depth %08X\n",
            state.control, state.destBase, x0, y0, x1, y1, state.destInfo, state.destPitch, state.pitch, state.colorInfo[0], state.depthInfo);
        fflush(stdout);
    }

    if (command != 3 && state.destBase != 0)
    {
        const bool fromDepth = sourceSelect == 4;
        if (fromDepth && DebugProbe::DepthWriteback() != 0 && (state.destPitch & 0x3FFF) != 0)
        {
            out.writeBack = true;
            out.guestBase = state.destBase;
            out.guestPitch = state.destPitch & 0x3FFF;
            out.guestWidth = width;
            out.guestHeight = height;
            out.guestEndian = state.destInfo & 7;
            out.guestFloatDepth = ((state.depthInfo >> 16) & 1) != 0;
        }
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
        if (fromDepth)
        {
            const RenderResources::DepthTarget* depth = RenderResources::DepthTargetOf(RenderResources::DepthTargetFor(state.depthInfo, state.pitch));
            if (depth == nullptr) return false;
            out.source = depth->resource;
            out.sourceWidth = depth->width;
            out.sourceHeight = depth->height;
            out.sourceSamples = depth->samples;
            format = DXGI_FORMAT_R32_FLOAT;
        }
        else
        {
            const uint32_t info = state.colorInfo[sourceSelect < 4 ? sourceSelect : 0];
            const RenderResources::ColorTarget* color = RenderResources::ColorTargetOf(RenderResources::ColorTargetFor(info, state.pitch));
            if (color == nullptr) return false;
            out.source = color->resource;
            out.sourceWidth = color->width;
            out.sourceHeight = color->height;
            out.sourceSamples = color->samples;
            format = color->format;
        }
        RenderResources::Resolved* surface = RenderResources::ResolvedFor(state.destBase, hostWidth, hostHeight, format, fromDepth);
        if (surface == nullptr) return false;
        surface->guestWidth = width;
        surface->guestHeight = height;
        out.copy = true;
        out.fromDepth = fromDepth;
        out.destination = surface->view;
        out.destinationResource = surface->resource;
        out.x0 = hostX0;
        out.y0 = hostY0;
        out.width = hostWidth;
        out.height = hostHeight;
    }

    if (colorClear)
    {
        if (const RenderResources::ColorTarget* color = RenderResources::ColorTargetOf(RenderResources::ColorTargetFor(state.colorInfo[0], state.pitch)))
        {
            const uint32_t value = state.colorClearValue;
            out.colorClear = true;
            out.clearTarget = color->view;
            out.clearColor[0] = ((value >> 16) & 0xFF) / 255.0f;
            out.clearColor[1] = ((value >> 8) & 0xFF) / 255.0f;
            out.clearColor[2] = (value & 0xFF) / 255.0f;
            out.clearColor[3] = (value >> 24) / 255.0f;
            out.clearRect[0] = int(std::lround(hostX0));
            out.clearRect[1] = int(std::lround(hostY0));
            out.clearRect[2] = int(std::lround(x1 * scaleX));
            out.clearRect[3] = int(std::lround(y1 * scaleY));
        }
    }
    if (depthClear)
    {
        if (const RenderResources::DepthTarget* depth = RenderResources::DepthTargetOf(RenderResources::DepthTargetFor(state.depthInfo, state.pitch)))
        {
            const uint32_t value = state.depthClearValue;
            out.depthClear = true;
            out.clearDepth = depth->view;
            out.clearDepthValue = float(value >> 8) / 16777215.0f;
            out.clearStencilValue = uint8_t(value & 0xFF);
        }
    }
    return out.copy || out.colorClear || out.depthClear;
}

void RenderCommands::SetAimBlur(bool drawn) { g_aimBlur = drawn; }
void RenderCommands::SetSteadyDecals(bool on) { g_steadyDecals = on; }

void RenderCommands::CheckLateWrites()
{
    if (g_lateChecks.empty()) return;
    for (const LateCheck& check : g_lateChecks)
    {
        Verify::Checked(Verify::Buffers);
        if (HashGuest(check.physical, check.bytes) != check.hash)
            Verify::Mismatch(Verify::Buffers, "the %s at %08X (%u bytes) changed after a draw read them, before the frame ended: written late by the title (ps %016llx)",
                check.indices ? "indices" : "vertices", check.physical, check.bytes, (unsigned long long)check.ps);
    }
    g_lateChecks.clear();
}

void RenderCommands::SetHdr(bool scenePass, float bloom, float hudGain)
{
    g_hdrScene = scenePass;
    g_bloom = bloom;
    g_hudGain = scenePass ? hudGain : 0.0f;
}
void RenderCommands::SetShellShockBlur(bool drawn) { g_shellShockBlur = drawn; }
void RenderCommands::SetRendering(bool sharpShadows, bool fastShadowMaps, bool foliageCoverage)
{
    g_sharpShadows = sharpShadows;
    g_fastShadowMaps = fastShadowMaps;
    g_foliageCoverage = foliageCoverage;
}

void RenderCommands::SetPlayerDown(bool down)
{
    if (g_playerDown.exchange(down, std::memory_order_relaxed) == down) return;
    printf("render: the host's player is %s: the depth of field pass %s\n", down ? "down" : "up again",
           down ? "is drawn for the death view's blur" : "follows the blur while aiming setting again");
    fflush(stdout);
}

void RenderCommands::SetBinocularsUp(bool up)
{
    if (up) g_upFrames++;
    if (g_binocularsUp.exchange(up, std::memory_order_relaxed) == up) return;
    static int said = 0;
    if (up)
    {
        for (uint64_t& n : g_upSkips) n = 0;
        g_upFrames = 1;
        if (said < 40) { printf("render: binoculars up: the depth of field pass is drawn while they are\n"); fflush(stdout); }
        return;
    }
    if (said++ >= 40) return;
    printf("render: binoculars down after %llu frames; draws left out meanwhile:", (unsigned long long)g_upFrames);
    for (int i = 0; i < SkipCount; i++) if (g_upSkips[i] != 0) printf(" %s x%llu;", SkipNames[i], (unsigned long long)g_upSkips[i]);
    printf("\n");
    fflush(stdout);
}

void RenderCommands::ReportSkips()
{
    bool any = false;
    for (int i = 0; i < SkipCount; i++) if (g_skips[i] != 0) any = true;
    if (!any) return;
    printf("        skipped:");
    for (int i = 0; i < SkipCount; i++) if (g_skips[i] != 0) printf(" %s x%llu;", SkipNames[i], (unsigned long long)g_skips[i]);
    printf("\n");
}


