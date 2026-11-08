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
#include "render.h"   // 0.5.714: Render::ProbeVertexBuffer
#include <string>
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
#include <mutex>
#include <set>
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
    // 0.5.837: Seelow Night's light on the arms (Render::SetNightLight)
    std::atomic<bool> g_nightLight{ false };
    float g_nightScale[3] = { 1.0f, 1.0f, 1.0f };

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
    uint32_t g_surfaceLight = 0;   // the world's solid surfaces brightened by this much (0 .. 127; xenos_hlsl.cpp)
    bool g_depthReversed = false;    // the solid world's depth test is greater (render_post.cpp's occlusion)
    float g_fillSqueeze = 0.0f;      // the HUD's squeeze when filling the screen (SetFill), 0 none
    bool g_worldThisFrame = false, g_worldLastFrame = false;   // the scene's whole depth resolved: a level's world drawn
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
    std::atomic<bool> g_spectatorSeat{ false };   // 0.5.616: the team choice's blur left out
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
        out.samples = 1;
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
            out.samples = target->samples;
            out.targetCount = i + 1;
        }
        if ((state.depthControl & 3) != 0)
        {
            if (const RenderResources::DepthTarget* depth = RenderResources::DepthTargetOf(RenderResources::DepthTargetFor(state.depthInfo, state.pitch)))
            {
                out.depthView = depth->view;
                out.depthResource = depth->resource;
                out.depthTexture = depth->texture;
                out.samples = depth->samples;
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
    uint32_t g_drawTexture0 = 0;   // 0.5.821: this draw's first pixel texture (diagnostic)
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
                if (stage == 0 && out.textureCount == 0) g_drawTexture0 = words[1] & 0xFFFFF000u;   // 0.5.821
                uint32_t width = 1, height = 1;
                bool resolved = false;
                const Handle texture = RenderResources::TextureFor(words, width, height, resolved);
                DrawCommand::Texture& binding = out.textures[out.textureCount++];
                binding.stage = uint8_t(stage);
                binding.slot = uint8_t(fetch.slot);
                binding.samplerSlot = uint8_t(fetch.sampler);
                if (resolved) binding.view = RenderResources::ResolvedView(words[1] & 0xFFFFF000u, width, height);
                else binding.view = RenderResources::TextureView(texture, fetch.dimension);
                // 0.5.478: the button icons drawn as keys (render_prompts.cpp).
                if (ID3D11ShaderResourceView* keys = RenderInternal::PromptTexture(words[1] & 0xFFFFF000u, width, height)) binding.view = keys;
                CheckView(binding.view, words, resolved, texture, fetch.slot, stage);
                // 0.5.698: DIAGNOSTIC (Carentan's interiors): the programs that
                // sample a 512x512 8888 texture (our converted lightmaps; Rouen's
                // are DXT5), once a program pair, with every texture they fetch
                if (stage == 0 && (words[1] & 0x3F) == 6 && (words[2] & 0x1FFF) == 511 && ((words[2] >> 13) & 0x1FFF) == 511)
                {
                    static std::mutex saidLock;
                    static std::set<uint64_t> said;
                    std::lock_guard<std::mutex> lock(saidLock);
                    if (said.size() < 24 && said.insert(ps->hash ^ ((vs ? vs->hash : 0) * 31)).second)
                    {
                        printf("render: lightmap-like texture (8888 512x512) in pixel slot %u: ps %016llx vs %016llx; fetches:", fetch.slot,
                            (unsigned long long)ps->hash, (unsigned long long)(vs ? vs->hash : 0));
                        for (const XenosHlsl::TextureFetch& other : ps->textureFetches)
                        {
                            uint32_t w[6];
                            RenderState::ReadTextureFetch(other.slot, w);
                            printf(" t%u=fmt%u %ux%u@%08X", other.slot, w[1] & 0x3F, (w[2] & 0x1FFF) + 1, ((w[2] >> 13) & 0x1FFF) + 1, w[1] & 0xFFFFF000u);
                        }
                        // 0.5.699: the program's c[i] is the file's constant map[i] (packed)
                        printf("\nrender: its pixel constants (packed c = file c: values):");
                        const size_t count = ps->constantMap.empty() ? 16 : std::min<size_t>(ps->constantMap.size(), 24);
                        for (size_t r = 0; r < count; r++)
                        {
                            const uint32_t file = ps->constantMap.empty() ? uint32_t(r) : ps->constantMap[r];
                            printf(" | c%zu=f%u:", r, file);
                            for (uint32_t k = 0; k < 4; k++)
                            {
                                const uint32_t bits = RenderState::Register(RenderState::PixelConstants + file * 4 + k);
                                float value; memcpy(&value, &bits, 4);
                                printf(" %g", value);
                            }
                        }
                        printf("\n");
                        fflush(stdout);
                    }
                }
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
        // 0.5.616: the spectator's seat (spectator.cpp) without the team
        // choice's blur (user, 613: "it stay blurry in the background").
        // 615's frames: behind the team choice, and in the seat taken from
        // it, the game's last full screen pass is 2a90db64, which mixes the
        // frame (texture 0, 1040x624) with a quarter size blurred copy
        // (texture 1, 260x156); in play the same pass is 09fa43c5, the frame
        // alone. The game keeps the blurred one while it has the team choice
        // up, so in the seat the frame goes in for the blurred copy too.
        if (g_spectatorSeat.load(std::memory_order_relaxed) && ps->hash == 0x2a90db64a855e6baull)
        {
            ID3D11ShaderResourceView* frame = nullptr;
            for (uint32_t i = 0; i < out.textureCount; i++) if (out.textures[i].stage == 0 && out.textures[i].slot == 0) frame = out.textures[i].view;
            for (uint32_t i = 0; frame && i < out.textureCount; i++) if (out.textures[i].stage == 0 && out.textures[i].slot == 1) out.textures[i].view = frame;
            static int told = 0;
            if (told < 3) { told++; printf("render: the spectator's seat: the blur behind the team choice taken out (%s)\n", frame ? "its frame for the blurred copy" : "no frame found"); fflush(stdout); }
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
    // 0.5.714: Render::ProbeVertexBuffer's buffer and folder
    std::atomic<uint32_t> g_probeBuffer{ 0 };
    std::string g_probeDir;

    // 0.5.717: Render::TrackMeshPoint's buffer (its GPU address), point and result
    std::atomic<uint32_t> g_trackBuffer{ 0 };
    float g_trackPoint[3] = {};
    std::mutex g_trackLock;
    float g_trackScreen[2] = {};
    uint64_t g_trackAt = 0;
    float g_trackBone[12] = {}, g_trackViewProjection[16] = {};   // 0.5.718

    // 0.5.809: Render::SetMirrorBuffers' (GPU addresses), and this draw's
    // match and what PrepareMirror needs of it
    std::mutex g_mirrorLock;
    std::vector<uint32_t> g_mirrorBuffers;
    std::atomic<bool> g_mirrorOn{ false };
    bool g_mirrorThisDraw = false;
    uint32_t g_mirrorVb = 0;   // 0.5.811: the matched buffer
    const RenderShaders::Program* g_mirrorVs = nullptr;
    uint32_t g_mirrorScMode = 0;
    float g_mirrorOffset[4] = {};
    bool g_mirrorBehind = false, g_mirrorForward = false;
    // 0.5.821: Render::LogDrawsFor's window, and this draw's first pixel texture
    std::atomic<uint64_t> g_logDrawsUntil{ 0 }, g_logDraws2From{ 0 }, g_logDraws2Until{ 0 };   // 0.5.826: and a second window 400 ms later (no shot), to compare
    // 0.5.822: the view model's clip x and w rows this frame (c12, c15 of a
    // mirrored draw), and this draw's first vertex buffer and stride
    float g_frameX[4] = {}, g_frameW[4] = {};
    std::atomic<uint64_t> g_flashUntil{ 0 };   // 0.5.824: Render::DualFlashFor
    uint32_t g_drawBlend = 0;                  // 0.5.824: this draw's blend control
    uint32_t g_drawFirstVertex = 0;            // 0.5.825: its lowest vertex's place (physical), 0: none
    uint64_t g_frameXFrame = ~0ull;            // the frame g_frameX/W are of
    uint32_t g_drawVb0 = 0, g_drawStride0 = 0;
    // (g_drawTexture0: declared before Textures)
    void MirrorCheck(uint32_t vertexBuffer)
    {
        if (!g_mirrorOn.load(std::memory_order_relaxed) || g_mirrorThisDraw) return;
        std::lock_guard<std::mutex> hold(g_mirrorLock);
        for (uint32_t b : g_mirrorBuffers) if (b == vertexBuffer)
        {
            g_mirrorThisDraw = true; g_mirrorVb = b;
            g_frameXFrame = RenderInternal::Swaps();
            for (int k = 0; k < 4; k++)
            {
                const uint32_t a = RenderState::Register(RenderState::VertexConstants + 48 + k), w = RenderState::Register(RenderState::VertexConstants + 60 + k);
                memcpy(&g_frameX[k], &a, 4); memcpy(&g_frameW[k], &w, 4);
            }
            return;
        }
    }

    uint32_t GpuAddress(uint32_t a)   // (render_prompts.cpp's MeshTexKey)
    {
        a &= ~3u;
        if (a >= 0x48000000u && a < 0x6F000000u) a -= 0x28000000u;
        return a;
    }

    void TrackPoint(uint32_t vertexBuffer)
    {
        const uint32_t want = g_trackBuffer.load();
        if (!want || (vertexBuffer & ~3u) != want) return;
        float c[24];
        for (int i = 0; i < 24; i++) { const uint32_t w = RenderState::Register(RenderState::VertexConstants + 48 + i); memcpy(&c[i], &w, 4); }   // c12..c17
        float b[12];
        for (int i = 0; i < 12; i++) { const uint32_t w = RenderState::Register(RenderState::VertexConstants + 64 + i); memcpy(&b[i], &w, 4); }    // c16..c18: the bone
        float p[3], world[3], clip[4];
        { std::lock_guard<std::mutex> hold(g_trackLock); for (int k = 0; k < 3; k++) p[k] = g_trackPoint[k]; }
        for (int r = 0; r < 3; r++) world[r] = b[4 * r] * p[0] + b[4 * r + 1] * p[1] + b[4 * r + 2] * p[2] + b[4 * r + 3];
        for (int r = 0; r < 4; r++) clip[r] = c[4 * r] * world[0] + c[4 * r + 1] * world[1] + c[4 * r + 2] * world[2] + c[4 * r + 3];
        if (!(clip[3] > 0.01f) || !std::isfinite(clip[0]) || !std::isfinite(clip[1])) return;
        std::lock_guard<std::mutex> hold(g_trackLock);
        memcpy(g_trackBone, b, sizeof g_trackBone);
        memcpy(g_trackViewProjection, c, sizeof g_trackViewProjection);
        g_trackScreen[0] = 0.5f + 0.5f * clip[0] / clip[3];
        g_trackScreen[1] = 0.5f - 0.5f * clip[1] / clip[3];
        g_trackAt = GetTickCount64();
    }

    void ProbeConstants(uint32_t vertexBuffer)
    {
        // (the draw reads it 0x28000000 lower in the extended window, as
        // render_prompts.cpp's MeshTexKey: 46190000 for 6E190000)
        uint32_t want = g_probeBuffer.load() & ~3u;
        if (want >= 0x48000000u && want < 0x6F000000u) want -= 0x28000000u;
        if (!vertexBuffer || !want || (vertexBuffer & ~3u) != want) return;
        static int written = 0;
        static uint64_t lastAt = 0;
        const uint64_t now = GetTickCount64();
        if (written >= 3 || now - lastAt < 8000) return;
        lastAt = now;
        written++;
        uint32_t words[1024];
        for (uint32_t i = 0; i < 1024; i++) words[i] = RenderState::Register(RenderState::VertexConstants + i);
        const std::string path = g_probeDir + "vsconst_" + std::to_string(written) + ".bin";
        if (FILE* f = fopen(path.c_str(), "wb")) { fwrite(words, 4, 1024, f); fclose(f); }
        printf("render: the probed mesh's vertex constants written to %s (%llu ms)\n", path.c_str(), (unsigned long long)now);
        fflush(stdout);
    }

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
            // 0.5.653: a mesh with a texture of its own (Render::SetMeshTexture):
            // the pixel program's texture in COD3_MESHTEX_SLOT (0) is it
            int meshSlot = -1;
            ProbeConstants(word0 & ~3u);   // 0.5.714
            TrackPoint(word0 & ~3u);       // 0.5.717
            MirrorCheck(word0 & ~3u);      // 0.5.809
            if (!g_drawVb0) { g_drawVb0 = word0 & ~3u; g_drawStride0 = fetch.stride; }   // 0.5.822
            if (ID3D11ShaderResourceView* own = RenderInternal::MeshTexture(word0 & ~3u, meshSlot))
            {
                static const int defaultSlot = [] { const char* t = getenv("COD3_MESHTEX_SLOT"); return t ? atoi(t) : 5; }();   // 0.5.657: 5, the colour map (656's screenshots)
                // 0.5.661: the mesh's own slot; -2: the draw's lowest
                int wantedSlot = meshSlot >= 0 ? meshSlot : defaultSlot;
                if (meshSlot == -2)
                {
                    wantedSlot = 1 << 30;
                    for (uint32_t i = 0; i < out.textureCount; i++) if (out.textures[i].stage == 0) wantedSlot = std::min(wantedSlot, int(out.textures[i].slot));
                }
                static std::map<uint32_t, int> toldFor;   // 0.5.666: the first two draws of each mesh
                int& told = toldFor[word0 & ~3u];
                bool put = false;
                for (uint32_t i = 0; i < out.textureCount; i++)
                {
                    DrawCommand::Texture& t = out.textures[i];
                    if (told < 2)
                    {
                        // 0.5.664: what each slot holds (its format and size: a normal
                        // map, a specular map, a cube), to know what to give it
                        D3D11_SHADER_RESOURCE_VIEW_DESC vd{};
                        D3D11_TEXTURE2D_DESC td{};
                        if (t.view)
                        {
                            t.view->GetDesc(&vd);
                            ID3D11Resource* res = nullptr;
                            t.view->GetResource(&res);
                            ID3D11Texture2D* tex = nullptr;
                            if (res && SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex)))) { tex->GetDesc(&td); tex->Release(); }
                            if (res) res->Release();
                        }
                        printf("render: mesh texture draw: texture %u stage %u slot %u sampler %u: format %u, dimension %u, %ux%u\n", i, t.stage, t.slot, t.samplerSlot,
                               unsigned(vd.Format), unsigned(vd.ViewDimension), td.Width, td.Height);
                    }
                    if (t.stage == 0 && int(t.slot) == wantedSlot && !put) { t.view = own; put = true; }
                    else if (t.stage == 0)
                        if (ID3D11ShaderResourceView* extra = RenderInternal::MeshSlotTexture(word0 & ~3u, int(t.slot))) t.view = extra;
                }
                if (told < 2) { told++; printf("render: mesh texture draw (vertex buffer %08X): %u textures, put in slot %d: %s\n", word0 & ~3u, out.textureCount, wantedSlot, put ? "yes" : "no"); fflush(stdout); }
            }
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
                // 0.5.837: Seelow Night: the arms' and gun's light (pixel program
                // 9a14f83313a256a4, 9 rgb terms c18-c26 from the zone's light grid,
                // Rouen's afternoon: ~0.20 at night where Longyard has ~0.03 bluish)
                // scaled to moonlight (Render::SetNightLight)
                if (stage == 1 && g_nightLight.load(std::memory_order_relaxed) && programs[1]->hash == 0x9a14f83313a256a4ull)
                {
                    const size_t count = map.empty() ? 256 : map.size();
                    for (size_t i = 0; i < count; i++)
                    {
                        const uint32_t c = map.empty() ? uint32_t(i) : uint32_t(map[i]);
                        if (c < 18 || c > 26) continue;
                        float v[4];
                        memcpy(v, from + c * 4u, 16);
                        for (int k = 0; k < 3; k++) v[k] *= g_nightScale[k];
                        memcpy(words + i * 4, v, 16);
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
        // program adds its 256 pixel constants, every word, to the log, and
        // (0.5.732) the same draw's 256 vertex constants as a VSDUMP line
        // (Seelow Night's haze: no fog colour among the pixel constants).
        static const uint64_t psDump = []() { const char* t = getenv("COD3_DRAWLOG_PSDUMP"); return t ? uint64_t(strtoull(t, nullptr, 16)) : 0ull; }();
        static uint64_t dumpedSwap = ~0ull;
        if (psDump != 0 && ps->hash == psDump && dumpedSwap != swap)
        {
            dumpedSwap = swap;
            char word[24];
            lines += "PSDUMP " + std::to_string(swap);
            for (uint32_t i = 0; i < 1024; i++) { snprintf(word, sizeof(word), " %08X", registers[0x4400 + i].load(std::memory_order_relaxed)); lines += word; }
            lines += "\n";
            snprintf(word, sizeof(word), " %016llx", (unsigned long long)vs->hash);
            lines += "VSDUMP " + std::to_string(swap) + word;
            for (uint32_t i = 0; i < 1024; i++) { snprintf(word, sizeof(word), " %08X", registers[0x4000 + i].load(std::memory_order_relaxed)); lines += word; }
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

namespace { bool FlashRows(const RenderShaders::Program* vs, int32_t& packedX, int32_t& packedW, float x[4], float w[4]); }   // 0.5.827 (below)
bool RenderCommands::PrepareDrawInner(uint32_t initiator, uint32_t indexBase, DrawCommand& out)
{
    g_mirrorThisDraw = false; g_mirrorVs = nullptr;   // 0.5.809
    g_drawTexture0 = 0;   // 0.5.821
    g_drawVb0 = 0; g_drawStride0 = 0;   // 0.5.822
    g_drawBlend = 0;   // 0.5.824
    g_drawFirstVertex = 0;   // 0.5.825
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
    g_drawBlend = state.blendControl[0];   // 0.5.824
    // 0.5.809: for a mirrored copy (PrepareMirror): the same, without culling
    g_mirrorScMode = state.suScModeControl;
    memcpy(g_mirrorOffset, oldOffset ? state.polyOffset : slopeOnly, sizeof g_mirrorOffset);
    g_mirrorBehind = behind; g_mirrorForward = forward;
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
    // Surface light (render_post.cpp, xenos_hlsl.cpp): its strength in bits
    // 24 to 30, on the world's solid surfaces only: a depth tested and
    // written, no blending, colour written. The sky, smoke, flashes, decals
    // and the HUD fail one of those and are left as they are.
    const bool solid = out.opaque && out.depthView != nullptr && (state.depthControl & 6u) == 6u && (state.colorMask & 7u) == 7u;
    if (g_surfaceLight != 0 && solid)
        constants.flags[0] |= (g_surfaceLight & 0x7Fu) << 24;
    // The solid world's depth test (bits 4 to 6): greater or greater or
    // equal is a depth that is 1 near, for the ambient occlusion.
    if (solid)
    {
        const uint32_t test = (state.depthControl >> 4) & 7;
        if (test != 7 && test != 0) g_depthReversed = test == 4 || test == 6;
    }
    // (Team Friend's alpha coverage bits 20-21 left out on merging: this
    // tree has its own alpha to coverage, constants.coverage[0] above.)
    // Filling the screen: a frame sized target's flat draws (the HUD, a
    // menu) squeezed into the 16:9 middle (xenos_hlsl.cpp), full screen
    // ones too (bit 22) when the last frame drew no world: a film, a
    // loading picture, the menus' backgrounds.
    constants.viewportScale[3] = 0.0f;
    if (g_fillSqueeze != 0.0f && state.pitch >= 640)
    {
        constants.viewportScale[3] = g_fillSqueeze;
        if (!g_worldLastFrame) constants.flags[0] |= 0x400000u;
    }
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
    g_mirrorVs = vs;   // 0.5.809

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
    // 0.5.825: in the dual Colts' flash window, a blended draw's own lowest
    // vertex (the particles share one big buffer: its start is another's)
    if (GetTickCount64() < g_flashUntil.load() && g_drawVb0 && g_drawStride0 && state.blendControl[0] != 0x00010001u)
    {
        uint32_t lowest = 0, highest = 0;
        if (DrawVertexRange(state, out, lowest, highest))
        {
            const int64_t vertex = int64_t(lowest) + state.indexOffset;
            if (vertex >= 0 && vertex < 0x100000) g_drawFirstVertex = g_drawVb0 + uint32_t(vertex) * g_drawStride0 * 4u;
        }
    }
    // 0.5.821: DIAGNOSTIC (Render::LogDrawsFor)
    // 0.5.827: the blended draws only, each with its own lowest vertex's distance
    // from the eye and side (by its program's clip rows), both windows
    if (const uint64_t now = GetTickCount64(); state.blendControl[0] != 0x00010001u &&
        (now < g_logDrawsUntil.load() || (now >= g_logDraws2From.load() && now < g_logDraws2Until.load())))
    {
        static int told2 = 0;
        if (told2 < 4000)
        {
            told2++;
            int32_t px = -1, pw = -1;
            float x[4] = {}, w[4] = {};
            const bool rows = FlashRows(vs, px, pw, x, w);
            float p[3] = {}, cw = 0.0f, cx = 0.0f;
            uint32_t lowest = 0, highest = 0;
            if (rows && g_drawVb0 && g_drawStride0 && DrawVertexRange(state, out, lowest, highest))
            {
                const int64_t vertex = int64_t(lowest) + state.indexOffset;
                if (vertex >= 0 && vertex < 0x100000)
                {
                    const uint8_t* at = Guest::Base + Guest::PhysicalAlias(g_drawVb0 + uint32_t(vertex) * g_drawStride0 * 4u);
                    for (int k = 0; k < 3; k++) { const uint32_t v = _byteswap_ulong(reinterpret_cast<const uint32_t*>(at)[k]); memcpy(&p[k], &v, 4); }
                    cw = w[0] * p[0] + w[1] * p[1] + w[2] * p[2] + w[3];
                    cx = x[0] * p[0] + x[1] * p[1] + x[2] * p[2] + x[3];
                }
            }
            // 0.5.831: the particle program's draws: where all their vertices land on the screen (c0, c1, c3)
            float bx0 = 9, bx1 = -9, by0 = 9, by1 = -9, ew0 = 1e9f, ew1 = -1e9f;
            if (vs->hash == 0x1c2487958f971104ull && g_drawVb0 && g_drawStride0 && DrawVertexRange(state, out, lowest, highest) && highest - lowest < 4096)
            {
                float r[3][4];
                const int rowOf[3] = { 0, 1, 3 };
                for (int j = 0; j < 3; j++) for (int k = 0; k < 4; k++) { const uint32_t v = RenderState::Register(RenderState::VertexConstants + 4 * rowOf[j] + k); memcpy(&r[j][k], &v, 4); }
                for (uint32_t i = lowest; i <= highest; i++)
                {
                    const int64_t vertex = int64_t(i) + state.indexOffset;
                    if (vertex < 0 || vertex >= 0x100000) break;
                    const uint8_t* at = Guest::Base + Guest::PhysicalAlias(g_drawVb0 + uint32_t(vertex) * g_drawStride0 * 4u);
                    float q[3];
                    for (int k = 0; k < 3; k++) { const uint32_t v = _byteswap_ulong(reinterpret_cast<const uint32_t*>(at)[k]); memcpy(&q[k], &v, 4); }
                    const float qx = r[0][0] * q[0] + r[0][1] * q[1] + r[0][2] * q[2] + r[0][3];
                    const float qy = r[1][0] * q[0] + r[1][1] * q[1] + r[1][2] * q[2] + r[1][3];
                    const float qw = r[2][0] * q[0] + r[2][1] * q[1] + r[2][2] * q[2] + r[2][3];
                    if (qw <= 0.01f) continue;
                    bx0 = (std::min)(bx0, qx / qw); bx1 = (std::max)(bx1, qx / qw); by0 = (std::min)(by0, qy / qw); by1 = (std::max)(by1, qy / qw);
                    ew0 = (std::min)(ew0, qw); ew1 = (std::max)(ew1, qw);
                }
            }
            printf("blend: %llu %s vs %016llx ps %016llx tex0 %08X blend %08X n %u rows %d/%d vertex %.0f %.0f %.0f eye %.0f side %.2f screen x %.2f..%.2f y %.2f..%.2f w %.0f..%.0f\n",
                   (unsigned long long)RenderInternal::Swaps(), now < g_logDrawsUntil.load() ? "SHOT" : "quiet", (unsigned long long)vs->hash,
                   ps ? (unsigned long long)ps->hash : 0ull, g_drawTexture0, state.blendControl[0], state.indexCount, px, pw, p[0], p[1], p[2], cw, cw != 0.0f ? cx / cw : 0.0f,
                   bx0, bx1, by0, by1, ew0, ew1);
        }
    }
    if (false)
    {
        static int told = 0;
        if (told < 1500)
        {
            told++;
            float c12[4], c15[4];
            for (int k = 0; k < 4; k++)
            {
                const uint32_t a = RenderState::Register(RenderState::VertexConstants + 48 + k), b = RenderState::Register(RenderState::VertexConstants + 60 + k);
                memcpy(&c12[k], &a, 4); memcpy(&c15[k], &b, 4);
            }
            printf("draws: %llu vs %016llx ps %016llx tex0 %08X blend %08X depth %08X n %u%s c12 %.3f %.3f %.3f %.1f c15 %.3f %.3f %.3f %.1f\n",
                   (unsigned long long)RenderInternal::Swaps(), (unsigned long long)vs->hash, ps ? (unsigned long long)ps->hash : 0ull, g_drawTexture0,
                   state.blendControl[0], state.depthControl, state.indexCount, g_mirrorThisDraw ? " MIRRORED" : "",
                   c12[0], c12[1], c12[2], c12[3], c15[0], c15[1], c15[2], c15[3]);
            // 0.5.822: a blended draw: which constants hold the view's x and w rows,
            // and its first vertex's words
            if (state.blendControl[0] != 0x00010001u)
            {
                std::string rows;
                for (int c = 0; c < 64; c++)
                {
                    float v[4];
                    for (int k = 0; k < 4; k++) { const uint32_t w = RenderState::Register(RenderState::VertexConstants + 4 * c + k); memcpy(&v[k], &w, 4); }
                    bool x = true, w = true;
                    for (int k = 0; k < 4; k++) { x = x && fabsf(v[k] - g_frameX[k]) < 1e-3f; w = w && fabsf(v[k] - g_frameW[k]) < 1e-3f; }
                    if (x) rows += " x=c" + std::to_string(c);
                    if (w) rows += " w=c" + std::to_string(c);
                }
                std::string vtx;
                if (g_drawVb0)
                {
                    const uint8_t* at = Guest::Base + Guest::PhysicalAlias(g_drawFirstVertex);
                    char t[16];
                    for (int k = 0; k < 8; k++) { uint32_t w = _byteswap_ulong(reinterpret_cast<const uint32_t*>(at)[k]); float f; memcpy(&f, &w, 4); snprintf(t, sizeof t, " %.2f", f); vtx += t; }
                }
                printf("draws:   rows%s; vb %08X stride %u, first words%s\n", rows.empty() ? " none" : rows.c_str(), g_drawVb0, g_drawStride0, vtx.c_str());
                // 0.5.823: the particle program's constants c0..c19, beside the view's rows
                static int dumped = 0;
                if (vs->hash == 0x1c2487958f971104ull && dumped < 2)
                {
                    dumped++;
                    printf("draws:   view x %.4f %.4f %.4f %.2f w %.4f %.4f %.4f %.2f\n", g_frameX[0], g_frameX[1], g_frameX[2], g_frameX[3], g_frameW[0], g_frameW[1], g_frameW[2], g_frameW[3]);
                    for (int c = 0; c < 20; c++)
                    {
                        float v[4];
                        for (int k = 0; k < 4; k++) { const uint32_t w = RenderState::Register(RenderState::VertexConstants + 4 * c + k); memcpy(&v[k], &w, 4); }
                        printf("draws:   c%d %.4f %.4f %.4f %.2f\n", c, v[0], v[1], v[2], v[3]);
                    }
                    std::string map;
                    for (size_t i = 0; i < vs->constantMap.size() && i < 40; i++) map += " " + std::to_string(vs->constantMap[i]);
                    printf("draws:   the program reads c:%s\n", map.c_str());
                }
            }
        }
    }
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
        // 0.5.477: and, always, the one resolve the title reads on the CPU
        // (the coronas' depth test, cpu_depth.cpp)
        const uint32_t cpuDepthAt = RenderInternal::CpuDepthAt();
        const bool cpuReads = cpuDepthAt != 0 && (state.destBase & 0x1FFFF000u) == cpuDepthAt;
        if (fromDepth && (DebugProbe::DepthWriteback() != 0 || cpuReads) && (state.destPitch & 0x3FFF) != 0)
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
            // The scene's whole depth (the width of its surface, wider than
            // tall, not a square shadow map), which the title resolves once
            // a frame for its soft smoke after the solid world: the ambient
            // occlusion goes into the colour beside it then.
            if (width * 10 >= state.pitch * 9 && width > height + height / 4 && height >= 400) g_worldThisFrame = true;
            if (width * 10 >= state.pitch * 9 && width > height + height / 4 && height >= 400)
                if (const RenderResources::ColorTarget* color = RenderResources::ColorTargetOf(RenderResources::ColorTargetFor(state.colorInfo[0], state.pitch)))
                    if (color->samples == depth->samples && color->width == depth->width)
                    {
                        out.occlusion = true;
                        out.occlusionTarget = color->view;
                        out.depthReversed = g_depthReversed;
                    }
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
void RenderCommands::SetSurfaceLight(uint32_t level) { g_surfaceLight = std::min(level, 127u); }
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

void RenderCommands::SetSpectatorSeat(bool seated) { g_spectatorSeat.store(seated, std::memory_order_relaxed); }

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

void RenderCommands::SetFill(float squeeze)
{
    g_fillSqueeze = squeeze;
    g_worldLastFrame = g_worldThisFrame;
    g_worldThisFrame = false;
}

// 0.5.714: DIAGNOSTIC (render.h)
void Render::ProbeVertexBuffer(uint32_t vertexBuffer, const char* dir)
{
    g_probeDir = dir ? dir : "";
    g_probeBuffer.store(vertexBuffer);
}

// 0.5.717 (render.h)
void Render::TrackMeshPoint(uint32_t vertexBuffer, float x, float y, float z)
{
    {
        std::lock_guard<std::mutex> hold(g_trackLock);
        g_trackPoint[0] = x; g_trackPoint[1] = y; g_trackPoint[2] = z;
    }
    g_trackBuffer.store(vertexBuffer ? GpuAddress(vertexBuffer) : 0);
}

bool Render::TrackedPoint(float& x, float& y)
{
    std::lock_guard<std::mutex> hold(g_trackLock);
    if (!g_trackAt || GetTickCount64() - g_trackAt > 150) return false;
    x = g_trackScreen[0]; y = g_trackScreen[1];
    return true;
}

bool Render::TrackedTransform(float bone[12], float viewProjection[16])
{
    std::lock_guard<std::mutex> hold(g_trackLock);
    if (!g_trackAt || GetTickCount64() - g_trackAt > 150) return false;
    memcpy(bone, g_trackBone, sizeof g_trackBone);
    memcpy(viewProjection, g_trackViewProjection, sizeof g_trackViewProjection);
    return true;
}

// 0.5.809: the dual pistols' left gun and arm (render_commands.h)
// 0.5.811: both guns set apart (the original moved right, its mirror left, by
// g_dualSpread of the screen's half width: the user, "they are really close
// together almost touching"), and the reload one pistol at a time: for
// g_reloadMs from Render::SetMirrorReload the left gun and arm are not drawn
// (the left hand, free, reloads the right Colt: the Colt's own reload) and the
// right arm's draws are recorded each frame; for the same time after it the
// right gun and arm are not drawn and the left plays that recording, mirrored
// (the right hand reloads the left Colt).
namespace
{
    struct DualRecord { uint32_t vb, n; uint64_t hash; std::vector<uint32_t> words; };
    struct DualFrame { uint64_t t; std::vector<DualRecord> draws; };
    std::vector<DualFrame> g_dualFrames;
    std::atomic<uint64_t> g_reloadStart{ 0 };
    std::atomic<uint32_t> g_reloadMs{ 0 };
    std::atomic<float> g_dualSpread{ 0.10f };
    uint64_t g_reloadSeen = 0;
    uint64_t g_mirrorFrame = ~0ull;
    std::map<uint32_t, uint32_t> g_mirrorCounts;

    // The vertex program's constants as it reads them (packed by its map).
    uint32_t PackVs(const RenderShaders::Program* vs, std::vector<uint32_t>& words, int32_t& row12, int32_t& row15)
    {
        const std::vector<uint16_t>& map = vs->constantMap;
        const uint32_t count = map.empty() ? 256u : uint32_t(map.size());
        const uint32_t need = std::min(256u, std::max(16u, (count + 15) & ~15u));
        words.assign(size_t(need) * 4, 0u);
        const uint32_t* from = RenderState::Registers() + 0x4000;
        row12 = row15 = -1;
        for (uint32_t i = 0; i < count; i++)
        {
            const uint32_t constant = map.empty() ? i : uint32_t(map[i]);
            memcpy(&words[size_t(i) * 4], from + constant * 4, 16);
            if (constant == 12) row12 = int32_t(i);
            if (constant == 15) row15 = int32_t(i);
        }
        return need;
    }
    // c12 (the clip's x) made sign * c12 + shift * c15 (w): mirrored and/or moved
    void PatchX(std::vector<uint32_t>& words, int32_t row12, int32_t row15, float sign, float shift)
    {
        float x[4], w[4];
        memcpy(x, &words[size_t(row12) * 4], 16);
        memcpy(w, &words[size_t(row15) * 4], 16);
        for (int k = 0; k < 4; k++) x[k] = sign * x[k] + shift * w[k];
        memcpy(&words[size_t(row12) * 4], x, 16);
    }
    // Into the ring; false on a wrap (as Constants: what was remembered is gone)
    bool UploadVs(const std::vector<uint32_t>& words, uint32_t need, uint32_t& at)
    {
        if (RenderResources::ConstantReserve(need * 16)) { g_uploaded = Uploaded(); return false; }
        uint32_t base;
        uint8_t* to = RenderResources::ConstantMap(need * 16, base);
        if (to == nullptr) return false;
        memcpy(to, words.data(), size_t(need) * 16);
        RenderResources::ConstantUnmap();
        RenderStats::Frame().streamBytes += need * 16;
        at = base;
        return true;
    }
}

// 0.5.824: the dual Colts' muzzle flash: a blended draw in the moments after
// their shot whose first vertex is near the eye on the right (the Colt's own
// view flash, at its real muzzle): its clip x row (found by value: the row
// three before the one equal to the view model's w row this frame, as c12/c15
// for the arms and guns, c0/c3 for the particles) moved as the right gun is,
// and drawn again mirrored as the left gun is.
namespace
{
    bool FlashRows(const RenderShaders::Program* vs, int32_t& packedX, int32_t& packedW, float x[4], float w[4])
    {
        const std::vector<uint16_t>& map = vs->constantMap;
        const uint32_t count = map.empty() ? 256u : uint32_t(map.size());
        auto packedOf = [&](uint32_t constant) -> int32_t {
            if (map.empty()) return constant < 256 ? int32_t(constant) : -1;
            for (uint32_t i = 0; i < count; i++) if (map[i] == constant) return int32_t(i);
            return -1;
        };
        for (uint32_t c = 3; c < 64; c++)
        {
            float v[4];
            for (int k = 0; k < 4; k++) { const uint32_t r = RenderState::Register(RenderState::VertexConstants + 4 * c + k); memcpy(&v[k], &r, 4); }
            bool same = true;
            for (int k = 0; k < 4; k++) same = same && fabsf(v[k] - g_frameW[k]) < 1e-3f;
            if (!same) continue;
            const int32_t pw = packedOf(c), px = packedOf(c - 3);
            if (pw < 0 || px < 0) continue;
            for (int k = 0; k < 4; k++) { const uint32_t r = RenderState::Register(RenderState::VertexConstants + 4 * (c - 3) + k); memcpy(&x[k], &r, 4); w[k] = v[k]; }
            bool isW = true;   // (c6 and c3 are both the w row in the particles': the lowest whose x is not w)
            for (int k = 0; k < 4; k++) isW = isW && fabsf(x[k] - w[k]) < 1e-3f;
            if (isW) continue;
            packedX = px; packedW = pw;
            return true;
        }
        // 0.5.828: the flash's additive core (tex 271E2000) is drawn when no row
        // equals the view's w row (another frame's): the particles' own c0/c3
        // (the caller's near-eye test keeps far effects out)
        if (vs->hash == 0x1c2487958f971104ull)
        {
            const int32_t pw = packedOf(3), px = packedOf(0);
            if (pw < 0 || px < 0) return false;
            for (int k = 0; k < 4; k++)
            {
                const uint32_t a = RenderState::Register(RenderState::VertexConstants + k), b = RenderState::Register(RenderState::VertexConstants + 12 + k);
                memcpy(&x[k], &a, 4); memcpy(&w[k], &b, 4);
            }
            packedX = px; packedW = pw;
            return true;
        }
        return false;
    }
}

bool RenderCommands::PrepareMirror(DrawCommand& in, DrawCommand& out, bool& skipOriginal)
{
    skipOriginal = false;
    // 0.5.824: the muzzle flash
    if (!g_mirrorThisDraw && g_mirrorVs != nullptr && in.ring && g_drawBlend != 0x00010001u && g_drawBlend != 0 &&
        GetTickCount64() < g_flashUntil.load() && RenderInternal::Swaps() - g_frameXFrame <= 2 && g_drawFirstVertex)
    {
        int32_t px, pw;
        float x[4], w[4];
        if (!FlashRows(g_mirrorVs, px, pw, x, w)) return false;
        const uint8_t* at = Guest::Base + Guest::PhysicalAlias(g_drawFirstVertex);   // 0.5.828: this draw's own (was the shared buffer's start)
        float p[3];
        for (int k = 0; k < 3; k++) { const uint32_t v = _byteswap_ulong(reinterpret_cast<const uint32_t*>(at)[k]); memcpy(&p[k], &v, 4); }
        const float cw = w[0] * p[0] + w[1] * p[1] + w[2] * p[2] + w[3];
        const float cx = x[0] * p[0] + x[1] * p[1] + x[2] * p[2] + x[3];
        static int told = 0;
        // (0.5.830-832's screen-place "core" test is gone: the far glow was the bullet's impact spark,
        // 3200 from the eye; the flash's own smoke and core are both ~10 from it and pass below)
        if (!(cw > 1.0f && cw < 150.0f && cx / cw > -0.3f)) return false;   // not near the eye on the right: another effect
        const uint64_t start = g_reloadStart.load(), length = g_reloadMs.load(), now = GetTickCount64();
        if (start && now >= start && now < start + 2 * length) return false;   // (no shots in a reload)
        std::vector<uint32_t> current;
        int32_t r12, r15;
        const uint32_t need = PackVs(g_mirrorVs, current, r12, r15);
        const float spread = g_dualSpread.load();
        std::vector<uint32_t> moved = current;
        PatchX(moved, px, pw, 1.0f, spread);
        uint32_t movedAt;
        if (UploadVs(moved, need, movedAt)) { in.floatAt[0] = movedAt; in.floatCount[0] = need; }
        std::vector<uint32_t> mirrored = current;
        PatchX(mirrored, px, pw, -1.0f, -spread);
        uint32_t mirrorAt;
        if (!UploadVs(mirrored, need, mirrorAt)) return false;
        out = in;
        out.floatAt[0] = mirrorAt;
        out.floatCount[0] = need;
        out.rasterizer = RenderPipeline::Rasterizer(g_mirrorScMode, true, true, g_mirrorOffset, g_mirrorBehind, g_mirrorForward);
        out.logged = false;
        out.layerQuery = false;
        if (told < 4) { told++; printf("render: dual flash: program %016llx tex0 %08X blend %08X (x row %d, w row %d), %.0f from the eye: moved right and mirrored left\n", (unsigned long long)g_mirrorVs->hash, g_drawTexture0, g_drawBlend, px, pw, cw); fflush(stdout); }
        return true;
    }
    if (!g_mirrorThisDraw || g_mirrorVs == nullptr || !in.ring) return false;
    g_mirrorThisDraw = false;
    std::vector<uint32_t> current;
    int32_t row12, row15;
    const uint32_t need = PackVs(g_mirrorVs, current, row12, row15);
    static int told = 0;
    if (row12 < 0 || row15 < 0)
    {
        if (told < 8) { told++; printf("render: mirror: vertex program %016llx reads no c12/c15: not mirrored\n", (unsigned long long)g_mirrorVs->hash); fflush(stdout); }
        return false;
    }
    // the reload's two parts
    const uint64_t now = GetTickCount64();
    const uint64_t start = g_reloadStart.load();
    const uint64_t length = g_reloadMs.load();
    if (start != g_reloadSeen) { g_reloadSeen = start; g_dualFrames.clear(); g_mirrorFrame = ~0ull; }
    const bool partA = start && now >= start && now < start + length;
    const bool partB = start && now >= start + length && now < start + 2 * length;
    const uint64_t frame = RenderInternal::Swaps();
    if (frame != g_mirrorFrame)
    {
        g_mirrorFrame = frame;
        g_mirrorCounts.clear();
        if (partA && g_dualFrames.size() < 2000) g_dualFrames.push_back(DualFrame{ now - start, {} });
    }
    const uint32_t n = g_mirrorCounts[g_mirrorVb]++;
    if (partA && !g_dualFrames.empty() && g_dualFrames.back().draws.size() < 64)
        g_dualFrames.back().draws.push_back(DualRecord{ g_mirrorVb, n, g_mirrorVs->hash, current });
    const float spread = g_dualSpread.load();
    // the right gun moved right (not in part B: not drawn)
    if (partB) skipOriginal = true;
    else if (spread != 0.0f)
    {
        std::vector<uint32_t> moved = current;
        PatchX(moved, row12, row15, 1.0f, spread);
        uint32_t at;
        if (UploadVs(moved, need, at)) { in.floatAt[0] = at; in.floatCount[0] = need; }
    }
    if (partA) return false;   // the left gun and arm down while the right is reloaded
    std::vector<uint32_t> words;
    if (partB)
    {
        // the recorded right arm at the same time into its reload, mirrored
        const uint64_t into = now - (start + length);
        const DualFrame* use = nullptr;
        for (const DualFrame& f : g_dualFrames) { if (f.t <= into) use = &f; else break; }
        if (!use && !g_dualFrames.empty()) use = &g_dualFrames.front();
        if (use)
            for (const DualRecord& r : use->draws)
                if (r.vb == g_mirrorVb && r.n == n && r.hash == g_mirrorVs->hash && r.words.size() == current.size()) { words = r.words; break; }
        if (words.empty()) return false;
    }
    else words = current;
    PatchX(words, row12, row15, -1.0f, -spread);
    uint32_t at;
    if (!UploadVs(words, need, at)) return false;
    out = in;
    out.floatAt[0] = at;
    out.floatCount[0] = need;
    out.rasterizer = RenderPipeline::Rasterizer(g_mirrorScMode, true, true, g_mirrorOffset, g_mirrorBehind, g_mirrorForward);
    out.logged = false;
    out.layerQuery = false;
    if (told < 8)
    {
        told++;
        printf("render: mirror: vertex program %016llx mirrored (c12 packed %d of %u, spread %.2f)%s\n", (unsigned long long)g_mirrorVs->hash, row12, need, spread,
               partB ? " from the recorded reload" : "");
        fflush(stdout);
    }
    return true;
}

void Render::SetMirrorReload(uint32_t milliseconds)
{
    g_reloadMs.store(milliseconds);
    g_reloadStart.store(milliseconds ? GetTickCount64() : 0);
}

void Render::SetMirrorSpread(float spread) { g_dualSpread.store(spread); }

void Render::LogDrawsFor(uint32_t milliseconds) { const uint64_t now = GetTickCount64(); g_logDrawsUntil.store(now + milliseconds); g_logDraws2From.store(now + 400); g_logDraws2Until.store(now + 400 + milliseconds); }

void Render::DualFlashFor(uint32_t milliseconds) { g_flashUntil.store(GetTickCount64() + milliseconds); }
void Render::SetNightLight(bool on, const float scale[3])
{
    for (int k = 0; k < 3; k++) g_nightScale[k] = scale[k];
    g_nightLight.store(on);
}

void Render::SetMirrorBuffers(const uint32_t* buffers, int count)
{
    std::lock_guard<std::mutex> hold(g_mirrorLock);
    g_mirrorBuffers.clear();
    for (int i = 0; buffers && i < count; i++) if (buffers[i]) g_mirrorBuffers.push_back(GpuAddress(buffers[i]));
    g_mirrorOn.store(!g_mirrorBuffers.empty());
}
