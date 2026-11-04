#include "blood_diag.h"
#include "blood.h"
// The Direct3D 11 renderer: the device, the draw, the resolve and the
// present. See render.h for where it sits.
//
// This is the executor: it runs the commands the PC render layer
// (render_commands.cpp) makes, and knows nothing of the console. A
// DrawCommand names what to bind by handle and view; each is compared with
// what the context already has and set only when it differs, so that a
// run of draws sharing their programs, targets and textures costs a few
// compares and a DrawIndexed each. Nothing here allocates, locks or
// creates a Direct3D object on a draw.

#include "render.h"
#include "blood_render.h"
#include "render_state.h"
#include "render_commands.h"
#include "render_internal.h"
#include "render_pipeline.h"
#include "render_shaders.h"
#include "render_resources.h"
#include "render_stats.h"
#include "overlay.h"
#include "settings.h"
#include "kernel.h"
#include "gpu.h"
#include "debug_probe.h"
#include "parallel.h"
#include "tuning.h"
#include "verify.h"
#include "hdr_shaders.h"
#include "render_queue.h"

#include <algorithm>
#include <atomic>
#include <climits>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <condition_variable>
#include <cstddef>
#include <type_traits>
#include <intrin.h>
#include <smmintrin.h>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <Windows.h>
#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>

#pragma comment(lib, "Synchronization.lib")   // WaitOnAddress, for the drawing thread

using Microsoft::WRL::ComPtr;
using RenderState::Handle;

namespace
{
    // --- the device ---------------------------------------------------------------------------

    // The command thread draws and presents; the window thread only reports
    // its size and, when the title is idle, asks for the last frame again.
    std::recursive_mutex g_mutex;
    ComPtr<ID3D11Device> g_device;
    ComPtr<ID3D11DeviceContext> g_context;
    ComPtr<ID3D11DeviceContext1> g_context1;
    ComPtr<IDXGISwapChain1> g_swapChain;
    ComPtr<ID3D11RenderTargetView> g_backView;
    ComPtr<ID3D11Texture2D> g_backTexture;
    HWND g_swapChainWindow = nullptr;
    int g_swapChainWidth = 0, g_swapChainHeight = 0;
    bool g_tearing = false;
    bool g_failed = false;
    bool g_started = false;

    std::atomic<HWND> g_window{ nullptr };
    std::atomic<int> g_clientWidth{ 0 }, g_clientHeight{ 0 };
    std::atomic<uint64_t> g_lastPresent{ 0 };   // steady clock milliseconds

    typedef HRESULT (WINAPI* CompileFunction)(LPCVOID, SIZE_T, LPCSTR, const D3D_SHADER_MACRO*,
        ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**);
    CompileFunction g_compile = nullptr;

    uint64_t g_frame = 1;          // the title's frames, counted at the swap
    std::atomic<uint64_t> g_swaps{ 0 };
    uint32_t g_frontBuffer = 0;

    // --- what the context has bound ------------------------------------------------------------

    struct Bound
    {
        Handle vertexShader = 0, pixelShader = 0;
        bool rectangles = false;
        bool flat = false;
        Handle blend = 0, depth = 0, rasterizer = 0;
        float blendFactor[4] = { -1, -1, -1, -1 };
        uint32_t stencilReference = 0xFFFFFFFFu;
        ID3D11RenderTargetView* targets[4] = {};
        ID3D11DepthStencilView* depthView = nullptr;
        uint32_t targetCount = 0;
        D3D11_RECT scissor{ -1, -1, -1, -1 };
        uint32_t viewportWidth = 0, viewportHeight = 0;
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        ID3D11ShaderResourceView* textures[2][32] = {};    // pixel, vertex
        ID3D11SamplerState* samplers[2][32] = {};
        ID3D11ShaderResourceView* vertexBuffers[96] = {};
        bool indices32 = false;
        bool indexBufferSet = false;
        ID3D11Buffer* indexBuffer = nullptr;
        uint32_t floatAt[2] = { 0xFFFFFFFFu, 0xFFFFFFFFu };   // the constant ring offsets bound
        uint32_t floatCount[2] = { 0, 0 };
        uint32_t drawAt = 0xFFFFFFFFu;
        bool constantsBound = false;
    };
    Bound g_bound;

    void ForgetBindings()
    {
        g_bound = Bound();
    }

    // The constant buffers: the booleans, and the float files and the
    // draw block for a device without constant buffer offsetting.
    ComPtr<ID3D11Buffer> g_boolConstants;
    ComPtr<ID3D11Buffer> g_floatConstants[2];   // when the ring is not available
    ComPtr<ID3D11Buffer> g_drawConstants;

    int AntiAliasing(const Settings::Values& settings);
    void PicturesAtDraw(const RenderState::DrawCommand& command);

    // --- the built in programs -------------------------------------------------------------------

    ComPtr<ID3D11GeometryShader> g_rectangleShader;
    ComPtr<ID3D11VertexShader> g_quadVertexShader;   // a triangle over the target, from the vertex id
    ComPtr<ID3D11PixelShader> g_copyPixelShader;     // a rectangle of a texture, point sampled
    ComPtr<ID3D11PixelShader> g_copyColourMS[4];     // the same of a multisampled colour target, the samples averaged: 2, 4, 8
    ComPtr<ID3D11PixelShader> g_copyDepthMS[4];      // and of a depth target, its first sample
    ComPtr<ID3D11PixelShader> g_presentPixelShader;  // the frame, linear sampled
    ComPtr<ID3D11PixelShader> g_fxaaPixelShader;
    ComPtr<ID3D11PixelShader> g_flatPixelShader;
    ComPtr<ID3D11PixelShader> g_depthOnlyPixelShader;   // the depth only mode's draws (COD3_DEPTH24)
    ComPtr<ID3D11SamplerState> g_pointSampler, g_linearSampler;
    ComPtr<ID3D11Buffer> g_quadConstants;
    ComPtr<ID3D11Buffer> g_hdrConstants;              // the HDR mode's settings for its programs (b1)
    ComPtr<ID3D11Texture1D> g_gammaTexture;           // the display's colour table (Gpu::DisplayGamma)
    ComPtr<ID3D11ShaderResourceView> g_gammaView;
    ComPtr<ID3D11Texture1D> g_identityGammaTexture;   // a table that changes nothing, for the films
    ComPtr<ID3D11ShaderResourceView> g_identityGammaView;
    std::atomic<bool> g_filmVideoLevels{ true };       // the setting, taken at each swap
    uint64_t g_gammaVersion = ~0ull;

    // The console's rectangle list, three corners with the fourth implied,
    // as two triangles. The three can come in any order: the diagonal is
    // the longest edge on the screen, the corner off it is mirrored across
    // the diagonal for the fourth, and the strip starts from that corner so
    // the first triangle keeps the winding it came with.
    const char* const RectangleSource =
        "struct V { float4 position : SV_Position; float4 o0 : TEXCOORD0; float4 o1 : TEXCOORD1; float4 o2 : TEXCOORD2; float4 o3 : TEXCOORD3;"
        " float4 o4 : TEXCOORD4; float4 o5 : TEXCOORD5; float4 o6 : TEXCOORD6; float4 o7 : TEXCOORD7; float4 o8 : TEXCOORD8; float4 o9 : TEXCOORD9;"
        " float4 o10 : TEXCOORD10; float4 o11 : TEXCOORD11; float4 o12 : TEXCOORD12; float4 o13 : TEXCOORD13; float4 o14 : TEXCOORD14; float4 o15 : TEXCOORD15; };\n"
        "V fourth(V a, V b, V c) { V d; d.position = a.position + c.position - b.position;"
        " d.o0 = a.o0 + c.o0 - b.o0; d.o1 = a.o1 + c.o1 - b.o1; d.o2 = a.o2 + c.o2 - b.o2; d.o3 = a.o3 + c.o3 - b.o3;"
        " d.o4 = a.o4 + c.o4 - b.o4; d.o5 = a.o5 + c.o5 - b.o5; d.o6 = a.o6 + c.o6 - b.o6; d.o7 = a.o7 + c.o7 - b.o7;"
        " d.o8 = a.o8 + c.o8 - b.o8; d.o9 = a.o9 + c.o9 - b.o9; d.o10 = a.o10 + c.o10 - b.o10; d.o11 = a.o11 + c.o11 - b.o11;"
        " d.o12 = a.o12 + c.o12 - b.o12; d.o13 = a.o13 + c.o13 - b.o13; d.o14 = a.o14 + c.o14 - b.o14; d.o15 = a.o15 + c.o15 - b.o15; return d; }\n"
        "[maxvertexcount(4)]\n"
        "void main(triangle V input[3], inout TriangleStream<V> stream) {\n"
        "    float2 p0 = input[0].position.xy / input[0].position.w, p1 = input[1].position.xy / input[1].position.w, p2 = input[2].position.xy / input[2].position.w;\n"
        "    float d01 = dot(p1 - p0, p1 - p0), d12 = dot(p2 - p1, p2 - p1), d02 = dot(p2 - p0, p2 - p0);\n"
        "    uint c = (d02 >= d01 && d02 >= d12) ? 1 : (d01 >= d12) ? 2 : 0;\n"
        "    uint a = (c + 1) % 3, b = (c + 2) % 3;\n"
        "    stream.Append(input[c]); stream.Append(input[a]); stream.Append(input[b]); stream.Append(fourth(input[a], input[c], input[b]));\n"
        "    stream.RestartStrip(); }\n";

    // FXAA 3.11, the quality path, on the presented picture: the edges the
    // title's own picture has, softened by the luma along them. The colour
    // at uv, before the display's table: the SDR present puts it through
    // that, the HDR one (hdr/cod3_hdr.hlsl, which includes this as
    // cod3_fxaa.hlsl) through RenoDX's output. Wants source, smp and texel.
#define COD3_FXAA_SOURCE \
        "float luma(float3 c) { return dot(c, float3(0.299, 0.587, 0.114)); }\n" \
        "float3 fxaaColor(float2 uv) {\n" \
        "    float2 px = texel.xy;\n" \
        "    float3 rgbM = source.Sample(smp, uv).rgb; float lM = luma(rgbM);\n" \
        "    float lN = luma(source.Sample(smp, uv + float2(0, -px.y)).rgb), lS = luma(source.Sample(smp, uv + float2(0, px.y)).rgb);\n" \
        "    float lW = luma(source.Sample(smp, uv + float2(-px.x, 0)).rgb), lE = luma(source.Sample(smp, uv + float2(px.x, 0)).rgb);\n" \
        "    float lMin = min(lM, min(min(lN, lS), min(lW, lE))), lMax = max(lM, max(max(lN, lS), max(lW, lE)));\n" \
        "    float range = lMax - lMin;\n" \
        "    if (range < max(0.0312, lMax * 0.125)) return rgbM;\n" \
        "    float lNW = luma(source.Sample(smp, uv + float2(-px.x, -px.y)).rgb), lNE = luma(source.Sample(smp, uv + float2(px.x, -px.y)).rgb);\n" \
        "    float lSW = luma(source.Sample(smp, uv + float2(-px.x, px.y)).rgb), lSE = luma(source.Sample(smp, uv + float2(px.x, px.y)).rgb);\n" \
        "    float lNS = lN + lS, lWE = lW + lE, lWc = lNW + lSW, lEc = lNE + lSE, lNc = lNW + lNE, lSc = lSW + lSE;\n" \
        "    float edgeH = abs(-2.0 * lW + lWc) + abs(-2.0 * lM + lNS) * 2.0 + abs(-2.0 * lE + lEc);\n" \
        "    float edgeV = abs(-2.0 * lN + lNc) + abs(-2.0 * lM + lWE) * 2.0 + abs(-2.0 * lS + lSc);\n" \
        "    bool horizontal = edgeH >= edgeV;\n" \
        "    float l1 = horizontal ? lS : lE, l2 = horizontal ? lN : lW;\n" \
        "    float g1 = l1 - lM, g2 = l2 - lM; bool steep1 = abs(g1) >= abs(g2);\n" \
        "    float gradScaled = 0.25 * max(abs(g1), abs(g2));\n" \
        "    float step = horizontal ? px.y : px.x; float lAvg = 0.0;\n" \
        "    if (steep1) { lAvg = 0.5 * (l1 + lM); } else { step = -step; lAvg = 0.5 * (l2 + lM); }\n" \
        "    float2 cur = uv; if (horizontal) cur.y += step * 0.5; else cur.x += step * 0.5;\n" \
        "    float2 off = horizontal ? float2(px.x, 0) : float2(0, px.y);\n" \
        "    float2 uv1 = cur - off, uv2 = cur + off;\n" \
        "    float e1 = luma(source.Sample(smp, uv1).rgb) - lAvg, e2 = luma(source.Sample(smp, uv2).rgb) - lAvg;\n" \
        "    bool r1 = abs(e1) >= gradScaled, r2 = abs(e2) >= gradScaled; bool done = r1 && r2;\n" \
        "    if (!r1) uv1 -= off; if (!r2) uv2 += off;\n" \
        "    if (!done) { [loop] for (int i = 2; i < 12; i++) {\n" \
        "        if (!r1) e1 = luma(source.Sample(smp, uv1).rgb) - lAvg; if (!r2) e2 = luma(source.Sample(smp, uv2).rgb) - lAvg;\n" \
        "        r1 = abs(e1) >= gradScaled; r2 = abs(e2) >= gradScaled; done = r1 && r2;\n" \
        "        float q = i < 5 ? 1.5 : i < 10 ? 2.0 : 4.0;\n" \
        "        if (!r1) uv1 -= off * q; if (!r2) uv2 += off * q; if (done) break; } }\n" \
        "    float d1 = horizontal ? (uv.x - uv1.x) : (uv.y - uv1.y), d2 = horizontal ? (uv2.x - uv.x) : (uv2.y - uv.y);\n" \
        "    bool dir1 = d1 < d2; float dist = min(d1, d2), edgeLen = d1 + d2;\n" \
        "    float pixelOffset = -dist / edgeLen + 0.5;\n" \
        "    bool lessThanAvg = lM < lAvg;\n" \
        "    bool correct = ((dir1 ? e1 : e2) < 0.0) != lessThanAvg;\n" \
        "    float finalOffset = correct ? pixelOffset : 0.0;\n" \
        "    float lAvg12 = (1.0 / 12.0) * (2.0 * (lNS + lWE) + lWc + lEc);\n" \
        "    float sub = clamp(abs(lAvg12 - lM) / range, 0.0, 1.0);\n" \
        "    float subFinal = (-2.0 * sub + 3.0) * sub * sub; subFinal = subFinal * subFinal * 0.75;\n" \
        "    finalOffset = max(finalOffset, subFinal);\n" \
        "    float2 finalUv = uv; if (horizontal) finalUv.y += finalOffset * step; else finalUv.x += finalOffset * step;\n" \
        "    return source.Sample(smp, finalUv).rgb; }\n"
    const char* const FxaaSource = COD3_FXAA_SOURCE;

    // One triangle over the target; the rectangle of the source it shows
    // and the source's texel size come in the constants.
    const char* const QuadSource =
        "Texture2D source : register(t0); SamplerState smp : register(s0);\n"
        // The display's colour table (Gpu::DisplayGamma), 256 entries, which
        // the console puts every channel of the picture through on its way out.
        "Texture1D gammaTable : register(t1);\n"
        "float3 gamma(float3 c) { float3 at = saturate(c) * (255.0 / 256.0) + (0.5 / 256.0);"
        " return float3(gammaTable.SampleLevel(smp, at.r, 0).r, gammaTable.SampleLevel(smp, at.g, 0).g, gammaTable.SampleLevel(smp, at.b, 0).b); }\n"
        "cbuffer Quad : register(b0) { float4 uvRect; float4 texel; };\n"
        "struct V { float4 position : SV_Position; float2 uv : TEXCOORD0; };\n"
        "V vsmain(uint id : SV_VertexID) { V v; float2 t = float2((id << 1) & 2, id & 2);"
        " v.position = float4(t * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0); v.uv = uvRect.xy + t * uvRect.zw; return v; }\n"
        "float4 copy(V v) : SV_Target { return source.Sample(smp, v.uv); }\n"
        "float4 present(V v) : SV_Target { return float4(gamma(source.Sample(smp, v.uv).rgb), 1.0); }\n"
        COD3_FXAA_SOURCE
        "float4 fxaa(V v) : SV_Target { return float4(gamma(fxaaColor(v.uv)), 1.0); }\n";

    // The copy of a multisampled target, which has no sampler: the texel
    // by position, a colour as the average of its samples, a depth as its
    // first, as the console's own resolve took one. SAMPLES is defined
    // when this is compiled, once for each count.
    const char* const CopyMultisampledSource =
        "cbuffer Quad : register(b0) { float4 uvRect; float4 texel; };\n"
        "struct V { float4 position : SV_Position; float2 uv : TEXCOORD0; };\n"
        "Texture2DMS<float4, SAMPLES> sourceMS : register(t0);\n"
        "float4 copyColourMS(V v) : SV_Target { int2 p = int2(v.uv / texel.xy); float4 sum = 0; [unroll] for (int i = 0; i < SAMPLES; i++) sum += sourceMS.Load(p, i); return sum / SAMPLES; }\n"
        "float4 copyDepthMS(V v) : SV_Target { int2 p = int2(v.uv / texel.xy); return sourceMS.Load(p, 0); }\n";

    // COD3_D3DFLAT=1: every opaque draw in a flat colour made from its pixel
    // program's hash, so a frame shows which program painted what.
    const char* const FlatSource =
        "cbuffer DrawConstants : register(b2) { float4 viewportScale; float4 viewportOffset; float4 targetSize; uint4 flags; };\n"
        "float4 main() : SV_Target { uint h = flags.w; return float4(float(h & 255u) / 255.0, float((h >> 8) & 255u) / 255.0, float((h >> 16) & 255u) / 255.0, 1.0); }\n";

    // The depth only mode's draws, which run no pixel program on the console:
    // with COD3_DEPTHONLYPS=1 their depth is cut to the console's precision
    // as the title's own pixel programs cut theirs (xenos_hlsl.cpp). Off by
    // default: with MSAA the cut gives a pixel's samples one depth, and the
    // colour pass then lost patches of ground to the Z pass where the two
    // cover a pixel's samples differently; the Z pass is put a little behind
    // instead (render_commands.cpp). It also took 0.3 ms a frame at 200%
    // with MSAA 4x.
    std::string DepthOnlySource()
    {
        return std::string(XenosHlsl::Depth20e4Source) +
            "struct DepthIn { noperspective centroid float4 position : SV_Position; };\n"
            "float main(DepthIn input) : SV_DepthLessEqual { return Depth20e4(input.position.z); }\n";
    }

    ComPtr<ID3DBlob> CompileBuiltIn(const char* source, const char* entry, const char* profile, const char* what, const D3D_SHADER_MACRO* defines = nullptr)
    {
        ComPtr<ID3DBlob> code, errors;
        const HRESULT hr = g_compile(source, strlen(source), what, defines, nullptr, entry, profile, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
        if (FAILED(hr))
        {
            printf("render: the %s did not compile: %s\n", what, errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no message");
            return nullptr;
        }
        return code;
    }

    bool BuiltInPrograms()
    {
        if (g_quadVertexShader) return true;
        if (ComPtr<ID3DBlob> code = CompileBuiltIn(RectangleSource, "main", "gs_5_0", "rectangle list program"))
            g_device->CreateGeometryShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_rectangleShader);
        if (ComPtr<ID3DBlob> code = CompileBuiltIn(QuadSource, "vsmain", "vs_5_0", "quad program"))
            g_device->CreateVertexShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_quadVertexShader);
        if (ComPtr<ID3DBlob> code = CompileBuiltIn(QuadSource, "copy", "ps_5_0", "copy program"))
            g_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_copyPixelShader);
        for (int i = 1; i <= 3; i++)
        {
            const char* const counts[4] = { "1", "2", "4", "8" };
            const D3D_SHADER_MACRO defines[2] = { { "SAMPLES", counts[i] }, { nullptr, nullptr } };
            if (ComPtr<ID3DBlob> code = CompileBuiltIn(CopyMultisampledSource, "copyColourMS", "ps_5_0", "multisampled copy program", defines))
                g_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_copyColourMS[i]);
            if (ComPtr<ID3DBlob> code = CompileBuiltIn(CopyMultisampledSource, "copyDepthMS", "ps_5_0", "multisampled depth copy program", defines))
                g_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_copyDepthMS[i]);
        }
        if (ComPtr<ID3DBlob> code = CompileBuiltIn(QuadSource, "present", "ps_5_0", "present program"))
            g_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_presentPixelShader);
        if (ComPtr<ID3DBlob> code = CompileBuiltIn(QuadSource, "fxaa", "ps_5_0", "anti aliasing program"))
            g_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_fxaaPixelShader);
        if (getenv("COD3_D3DFLAT") != nullptr)
            if (ComPtr<ID3DBlob> code = CompileBuiltIn(FlatSource, "main", "ps_5_0", "flat program"))
                g_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_flatPixelShader);
        if (XenosHlsl::Depth24() && getenv("COD3_DEPTHONLYPS") != nullptr)
            if (ComPtr<ID3DBlob> code = CompileBuiltIn(DepthOnlySource().c_str(), "main", "ps_5_0", "depth only program"))
                g_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &g_depthOnlyPixelShader);
        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        sampler.AddressU = sampler.AddressV = sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        g_device->CreateSamplerState(&sampler, &g_pointSampler);
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        g_device->CreateSamplerState(&sampler, &g_linearSampler);
        D3D11_BUFFER_DESC desc{};
        desc.ByteWidth = 32;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        g_device->CreateBuffer(&desc, nullptr, &g_quadConstants);
        return g_quadVertexShader && g_copyPixelShader && g_presentPixelShader;
    }

    // Draws `source`'s rectangle (in texels of the source) over the whole
    // of the target view of the given size, through the pixel program.
    void DrawQuad(ID3D11ShaderResourceView* source, uint32_t sourceWidth, uint32_t sourceHeight,
                  float x0, float y0, float width, float height,
                  ID3D11RenderTargetView* target, const D3D11_VIEWPORT& viewport,
                  ID3D11PixelShader* program, ID3D11SamplerState* sampler, ID3D11ShaderResourceView* second = nullptr,
                  ID3D11BlendState* blend = nullptr)
    {
        if (!BuiltInPrograms()) return;
        const float constants[8] = { x0 / sourceWidth, y0 / sourceHeight, width / sourceWidth, height / sourceHeight,
                                     1.0f / sourceWidth, 1.0f / sourceHeight, 0.0f, 0.0f };
        g_context->UpdateSubresource(g_quadConstants.Get(), 0, nullptr, constants, 0, 0);
        // Every texture the draws had bound goes, so the target is not
        // among them and the bindings forgotten below are truly gone.
        ID3D11ShaderResourceView* none[32] = {};
        g_context->PSSetShaderResources(0, 32, none);
        g_context->VSSetShaderResources(0, 32, none);
        ID3D11RenderTargetView* targets[1] = { target };
        g_context->OMSetRenderTargets(1, targets, nullptr);
        g_context->RSSetViewports(1, &viewport);
        { const float none[4] = {}; g_context->RSSetState(RenderPipeline::RasterizerObject(RenderPipeline::Rasterizer(0, false, true, none))); }
        g_context->OMSetBlendState(blend, nullptr, 0xFFFFFFFF);
        g_context->OMSetDepthStencilState(nullptr, 0);
        g_context->VSSetShader(g_quadVertexShader.Get(), nullptr, 0);
        g_context->GSSetShader(nullptr, nullptr, 0);
        g_context->PSSetShader(program, nullptr, 0);
        ID3D11Buffer* buffers[1] = { g_quadConstants.Get() };
        g_context->VSSetConstantBuffers(0, 1, buffers);
        g_context->PSSetConstantBuffers(0, 1, buffers);
        // The HDR programs' settings (hdr/cod3_hdr.hlsl), where the title's
        // draws have their booleans: those are bound again after this.
        if (g_hdrConstants) { ID3D11Buffer* hdr[1] = { g_hdrConstants.Get() }; g_context->PSSetConstantBuffers(1, 1, hdr); }
        ID3D11ShaderResourceView* sources[2] = { source, second };
        g_context->PSSetShaderResources(0, 2, sources);
        ID3D11SamplerState* samplers[1] = { sampler };
        g_context->PSSetSamplers(0, 1, samplers);
        g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_context->IASetInputLayout(nullptr);
        g_context->Draw(3, 0);
        g_context->PSSetShaderResources(0, 32, none);
        ID3D11RenderTargetView* noTargets[1] = { nullptr };
        g_context->OMSetRenderTargets(1, noTargets, nullptr);
        // Everything the draws had bound is gone.
        ForgetBindings();
    }

    // --- starting --------------------------------------------------------------------------------

    void StartDrawingThread();   // the drawing thread (0.5.391), below

    bool Start()
    {
        if (g_started) return !g_failed;
        g_started = true;

        HMODULE compiler = LoadLibraryW(L"d3dcompiler_47.dll");
        if (compiler != nullptr) g_compile = reinterpret_cast<CompileFunction>(GetProcAddress(compiler, "D3DCompile"));
        if (g_compile == nullptr)
        {
            printf("render: d3dcompiler_47.dll is not available; nothing can be drawn\n");
            g_failed = true;
            return false;
        }

        UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        if (getenv("COD3_D3DDEBUG") != nullptr) flags |= D3D11_CREATE_DEVICE_DEBUG;
        const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0 };
        D3D_FEATURE_LEVEL level;
        HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
            levels, 1, D3D11_SDK_VERSION, &g_device, &level, &g_context);
        if (FAILED(hr))
        {
            printf("render: no Direct3D 11 device (0x%08lX); nothing can be drawn\n", hr);
            g_failed = true;
            return false;
        }
        g_context.As(&g_context1);
        {
            ComPtr<IDXGIDevice> dxgiDevice;
            ComPtr<IDXGIAdapter> adapter;
            ComPtr<IDXGIFactory5> factory;
            DXGI_ADAPTER_DESC description{};
            if (SUCCEEDED(g_device.As(&dxgiDevice)) && SUCCEEDED(dxgiDevice->GetAdapter(&adapter)))
            {
                adapter->GetDesc(&description);
                if (SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory))))
                {
                    BOOL allowed = FALSE;
                    if (SUCCEEDED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowed, sizeof(allowed))))
                        g_tearing = allowed != FALSE;
                }
            }
            printf("render: Direct3D 11 on %ls%s%s\n", description.Description,
                (flags & D3D11_CREATE_DEVICE_DEBUG) ? ", with the debug layer" : "", g_tearing ? ", tearing allowed" : "");
        }
        RenderPipeline::Initialize(g_device.Get());
        RenderShaders::Initialize(g_device.Get());
        RenderResources::Initialize(g_device.Get(), g_context.Get());
        RenderStats::Initialize(g_device.Get(), g_context.Get());
        {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = 256;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            g_device->CreateBuffer(&desc, nullptr, &g_boolConstants);
            desc.ByteWidth = 4096;
            g_device->CreateBuffer(&desc, nullptr, &g_floatConstants[0]);
            g_device->CreateBuffer(&desc, nullptr, &g_floatConstants[1]);
            desc.ByteWidth = (sizeof(RenderState::DrawConstants) + 15) & ~15u;
            g_device->CreateBuffer(&desc, nullptr, &g_drawConstants);
        }
        RenderShaders::Precompile();
        RenderStats::BeginFrame();
        (void)RenderResources::BeginFrame(g_frame);
        StartDrawingThread();
        fflush(stdout);
        return true;
    }

    // With COD3_D3DDEBUG, what the debug layer has to say, printed as it comes.
    void DrainDebugMessages()
    {
        static ComPtr<ID3D11InfoQueue> queue;
        static bool tried = false;
        if (!tried) { tried = true; g_device.As(&queue); }
        if (!queue) return;
        static int printed = 0;
        const UINT64 count = queue->GetNumStoredMessages();
        for (UINT64 i = 0; i < count && printed < 60; i++)
        {
            SIZE_T length = 0;
            queue->GetMessage(i, nullptr, &length);
            std::vector<uint8_t> storage(length);
            D3D11_MESSAGE* message = reinterpret_cast<D3D11_MESSAGE*>(storage.data());
            if (SUCCEEDED(queue->GetMessage(i, message, &length)))
            {
                printf("d3d11 debug: %.*s\n", int(message->DescriptionByteLength), message->pDescription);
                printed++;
            }
        }
        queue->ClearStoredMessages();
        fflush(stdout);
    }

    // --- the frame log -------------------------------------------------------------------------------

    // COD3_D3DFRAME=N: the draws, resolves and presents from the Nth swap
    // to the one after next are printed. COD3_D3DFRAME=auto picks the
    // frame itself: the one after the first swap with three hundred draws
    // or more, ten seconds into a level. COD3_D3DFRAME=loading: the first
    // frame of the loading screen with anything on it.
    std::atomic<int64_t> g_frameWanted{ -1 };
    std::atomic<int64_t> g_frameUntil{ 0 };   // the first swap not logged, when the screenshot key set it
    long g_autoSeconds = 10;
    uint64_t g_drawsAtSwap = 0, g_drawsTotal = 0;
    int g_dumpNumber = 0;

    bool FrameLogged()
    {
        static const bool parsed = []() {
            const char* text = getenv("COD3_D3DFRAME");
            // COD3_RENDER_DEBUG=1 alone: the auto frame.
            if (text == nullptr && RenderStats::Debug()) text = "auto";
            if (text == nullptr) g_frameWanted.store(-1);
            else if (strncmp(text, "auto", 4) == 0)
            {
                g_frameWanted.store(-2);
                if (text[4] != 0) g_autoSeconds = strtol(text + 4, nullptr, 10);
            }
            else if (strcmp(text, "loading") == 0) g_frameWanted.store(-3);
            else g_frameWanted.store(strtol(text, nullptr, 10));
            return true;
        }();
        (void)parsed;
        const int64_t wanted = g_frameWanted.load(std::memory_order_relaxed);
        if (wanted < 0) return false;
        const int64_t swap = int64_t(g_swaps.load(std::memory_order_relaxed));
        // COD3_D3DFRAME_COUNT=N logs N frames instead of two, for a fault
        // that comes and goes from one frame to the next.
        static const int64_t count = []() { const char* t = getenv("COD3_D3DFRAME_COUNT"); const long v = t ? strtol(t, nullptr, 10) : 2; return int64_t(v > 0 ? v : 2); }();
        const int64_t until = g_frameUntil.load(std::memory_order_relaxed);
        return swap >= wanted && swap < (until > 0 ? until : wanted + count);
    }

    void FrameSwapped()
    {
        const uint64_t inFrame = g_drawsTotal - g_drawsAtSwap;
        g_drawsAtSwap = g_drawsTotal;
        const uint64_t swap = g_swaps.load(std::memory_order_relaxed);
        static int64_t levelSince = 0;
        if (levelSince == 0 && Kernel::Stats().filesOpened.load(std::memory_order_relaxed) >= 40)
            levelSince = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
        const bool inLevel = levelSince != 0 &&
            std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now().time_since_epoch()).count() - levelSince >= g_autoSeconds;
        static const uint64_t minDraws = []() { const char* t = getenv("COD3_D3DFRAME_MINDRAWS"); return t ? uint64_t(strtoull(t, nullptr, 10)) : 300ull; }();
        if (g_frameWanted.load(std::memory_order_relaxed) == -2 && inLevel && inFrame >= minDraws)
        {
            g_frameWanted.store(int64_t(swap) + 1);
            printf("render: frame %llu had %llu draws, the next two are logged\n", (unsigned long long)swap, (unsigned long long)inFrame);
        }
        if (g_frameWanted.load(std::memory_order_relaxed) == -3 && Kernel::Stats().filesOpened.load(std::memory_order_relaxed) >= 22 && inFrame >= 3)
        {
            g_frameWanted.store(int64_t(swap) + 1);
            printf("render: loading frame %llu had %llu draws, the next two are logged\n", (unsigned long long)swap, (unsigned long long)inFrame);
        }
    }

    void WriteBmp(const char* name, const std::vector<uint8_t>& pixels, uint32_t w, uint32_t h)
    {
        BITMAPFILEHEADER file{};
        BITMAPINFOHEADER info{};
        info.biSize = sizeof(info);
        info.biWidth = int(w);
        info.biHeight = -int(h);
        info.biPlanes = 1;
        info.biBitCount = 32;
        info.biCompression = BI_RGB;
        info.biSizeImage = DWORD(pixels.size());
        file.bfType = 0x4D42;
        file.bfOffBits = sizeof(file) + sizeof(info);
        file.bfSize = file.bfOffBits + info.biSizeImage;
        if (FILE* out = fopen(name, "wb"))
        {
            fwrite(&file, sizeof(file), 1, out);
            fwrite(&info, sizeof(info), 1, out);
            fwrite(pixels.data(), 1, pixels.size(), out);
            fclose(out);
        }
    }

    // A half float as a float.
    float HalfToFloat(uint16_t h)
    {
        const uint32_t sign = uint32_t(h & 0x8000u) << 16, exponent = (h >> 10) & 0x1Fu, mantissa = h & 0x3FFu;
        uint32_t bits;
        if (exponent == 0) { const float f = std::ldexp(float(mantissa), -24); return sign ? -f : f; }
        if (exponent == 31) bits = sign | 0x7F800000u | (mantissa << 13);
        else bits = sign | ((exponent + 112u) << 23) | (mantissa << 13);
        float f;
        memcpy(&f, &bits, 4);
        return f;
    }

    // A channel of a 16 bit float picture as a byte: the HDR mode's targets
    // hold the title's colour as an 8 bit one would, past one clipped; the
    // scRGB back buffer (scRgbWhite nonzero) is light, one being 80 nits,
    // shown with the game's white at the top and the sRGB curve.
    uint8_t HalfToByte(uint16_t h, float scRgbWhite)
    {
        float v = HalfToFloat(h);
        if (!(v > 0.0f)) return 0;
        if (scRgbWhite > 0.0f)
        {
            v /= scRgbWhite;
            v = v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(std::min(v, 1.0f), 1.0f / 2.4f) - 0.055f;
        }
        return uint8_t(std::min(v, 1.0f) * 255.0f + 0.5f);
    }

    std::atomic<float> g_screenshotWhite{ 0.0f };   // the HDR back buffer's white for pictures of it, scRGB

    // A texture read back at a quarter size, as B G R A rows; empty when it
    // could not be.
    std::vector<uint8_t> ReadBack(ID3D11Texture2D* texture, uint32_t& w, uint32_t& h, uint32_t shrink, float scRgbWhite = 0.0f)
    {
        std::vector<uint8_t> pixels;
        if (texture == nullptr) return pixels;
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        const bool half = desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT;
        if (desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM && !half) return pixels;
        // A multisampled target is resolved to one sample first: a staging
        // copy cannot be made of it directly.
        ComPtr<ID3D11Texture2D> resolved;
        if (desc.SampleDesc.Count > 1)
        {
            D3D11_TEXTURE2D_DESC single = desc;
            single.SampleDesc.Count = 1;
            single.SampleDesc.Quality = 0;
            single.BindFlags = D3D11_BIND_RENDER_TARGET;
            single.MiscFlags = 0;
            if (FAILED(g_device->CreateTexture2D(&single, nullptr, &resolved))) return pixels;
            g_context->ResolveSubresource(resolved.Get(), 0, texture, 0, desc.Format);
            texture = resolved.Get();
            desc.SampleDesc.Count = 1;
            desc.SampleDesc.Quality = 0;
        }
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> staging;
        if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &staging))) return pixels;
        g_context->CopyResource(staging.Get(), texture);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(g_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return pixels;
        w = desc.Width / shrink;
        h = desc.Height / shrink;
        pixels.resize(size_t(w) * h * 4);
        const bool rgba = desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM;
        for (uint32_t y = 0; y < h; y++)
        {
            const uint8_t* row = static_cast<const uint8_t*>(mapped.pData) + size_t(y) * shrink * mapped.RowPitch;
            for (uint32_t x = 0; x < w; x++)
            {
                uint8_t* to = pixels.data() + (size_t(y) * w + x) * 4;
                if (half)
                {
                    uint16_t c[4];
                    memcpy(c, row + size_t(x) * shrink * 8, 8);
                    to[0] = HalfToByte(c[2], scRgbWhite); to[1] = HalfToByte(c[1], scRgbWhite); to[2] = HalfToByte(c[0], scRgbWhite); to[3] = 255;
                    continue;
                }
                const uint8_t* from = row + size_t(x) * shrink * 4;
                to[0] = rgba ? from[2] : from[0]; to[1] = from[1]; to[2] = rgba ? from[0] : from[2]; to[3] = 255;
            }
        }
        g_context->Unmap(staging.Get(), 0);
        return pixels;
    }


    struct BloodPixelAudit {
        ComPtr<ID3D11Texture2D> scene;
        std::vector<uint8_t> before,after;
        uint32_t w=0,h=0;uint64_t frame=0,at=0;int captures=0;
        bool pending=false;
    } g_bloodPixels;
    std::string BloodPixelPath(const char* stage){
        const char* dir=getenv("COD3_BLOOD_CAPTURE_DIR");if(!dir)return {};
        std::filesystem::create_directories(dir);char name[256];snprintf(name,sizeof(name),"/frame_%llu_%s.bmp",g_bloodPixels.frame,stage);return std::string(dir)+name;
    }
    void BloodPixelsBefore(ID3D11Texture2D* scene,uint64_t frame){
        auto& a=g_bloodPixels;if(!BloodDiag::On()||!getenv("COD3_BLOOD_CAPTURE_DIR")||a.pending||a.captures>=(getenv("COD3_BLOOD_CONTROLLED")?36:12)||GetTickCount64()-a.at<(getenv("COD3_BLOOD_CONTROLLED")?5000:2000)||Blood::Status().decals==0||(getenv("COD3_BLOOD_CONTROLLED")&&Blood::Status().particles>0))return;
        a.at=GetTickCount64();a.scene=scene;a.frame=frame;a.before=ReadBack(scene,a.w,a.h,1);a.pending=!a.before.empty();
    }
    void BloodPixelsAfter(){
        auto& a=g_bloodPixels;if(!a.pending)return;
        uint32_t w=0,h=0;a.after=ReadBack(a.scene.Get(),w,h,1);
        if(a.after.size()!=a.before.size()){a.pending=false;return;}
        size_t changed=0;for(size_t i=0;i<a.before.size();i+=4)if(memcmp(&a.before[i],&a.after[i],3))++changed;
        BloodDiag::Log("pixel_write frame=%llu target=%p width=%u height=%u changed_rgb_pixels=%zu",a.frame,a.scene.Get(),a.w,a.h,changed);
        WriteBmp(BloodPixelPath("before_blood").c_str(),a.before,a.w,a.h);WriteBmp(BloodPixelPath("after_blood").c_str(),a.after,a.w,a.h);
    }
    void BloodPixelsFinal(){
        auto& a=g_bloodPixels;if(!a.pending||a.after.empty())return;
        uint32_t w=0,h=0;auto end=ReadBack(a.scene.Get(),w,h,1);size_t mask=0,sameAfter=0,backBefore=0,other=0;
        if(end.size()==a.after.size())for(size_t i=0;i<end.size();i+=4)if(memcmp(&a.before[i],&a.after[i],3)){
          ++mask;if(!memcmp(&end[i],&a.after[i],3))++sameAfter;else if(!memcmp(&end[i],&a.before[i],3))++backBefore;else ++other;
        }
        BloodDiag::Log("pixel_survival frame=%llu source_scene=%p mask=%zu unchanged_after_blood=%zu restored_before_blood=%zu modified_downstream=%zu final_backbuffer=%p",a.frame,a.scene.Get(),mask,sameAfter,backBefore,other,g_backTexture.Get());
        if(!end.empty())WriteBmp(BloodPixelPath("scene_at_present").c_str(),end,w,h);
        auto final=ReadBack(g_backTexture.Get(),w,h,1);if(!final.empty())WriteBmp(BloodPixelPath("final_backbuffer").c_str(),final,w,h);
        a.pending=false;a.scene.Reset();a.before.clear();a.after.clear();++a.captures;
    }

    // The depth alongside, as grey on a log scale, when COD3_D3DDRAWDUMPZ is set.
    void DumpDepth(const char* prefix, int number, ID3D11Texture2D* depth)
    {
        if (depth == nullptr) return;
        D3D11_TEXTURE2D_DESC desc{};
        depth->GetDesc(&desc);
        if (desc.Format != DXGI_FORMAT_R32G8X24_TYPELESS) return;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> staging;
        if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &staging))) return;
        g_context->CopyResource(staging.Get(), depth);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(g_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return;
        const uint32_t w = desc.Width / 4, h = desc.Height / 4;
        std::vector<uint8_t> pixels(size_t(w) * h * 4);
        for (uint32_t y = 0; y < h; y++)
        {
            const uint8_t* row = static_cast<const uint8_t*>(mapped.pData) + size_t(y) * 4 * mapped.RowPitch;
            for (uint32_t x = 0; x < w; x++)
            {
                float z;
                memcpy(&z, row + size_t(x) * 32, 4);
                const float shade = z <= 0.0f ? 0.0f : z >= 1.0f ? 1.0f : 1.0f + std::log10(z) / 6.0f;
                const uint8_t grey = uint8_t(std::min(std::max(shade, 0.0f), 1.0f) * 255.0f);
                uint8_t* to = pixels.data() + (size_t(y) * w + x) * 4;
                to[0] = grey; to[1] = grey; to[2] = z < 0.0f ? 255 : z > 1.0f ? 0 : grey; to[3] = 255;
            }
        }
        g_context->Unmap(staging.Get(), 0);
        char name[512];
        snprintf(name, sizeof(name), "%s-%04d-z.bmp", prefix, number);
        WriteBmp(name, pixels, w, h);
    }

    // COD3_D3DDRAWDUMP=prefix, with COD3_D3DFRAME: the colour target after
    // every draw of the logged frame, at a quarter size, as prefix-NNNN.bmp,
    // so a frame can be watched being built. COD3_D3DDRAWDUMPFROM=N skips
    // the first N draws of the frame.
    void DumpTargetAfterDraw(ID3D11Texture2D* texture, ID3D11Texture2D* depth)
    {
        static const char* const prefix = getenv("COD3_D3DDRAWDUMP");
        static const bool withDepth = getenv("COD3_D3DDRAWDUMPZ") != nullptr;
        if (prefix == nullptr || (texture == nullptr && !(withDepth && depth != nullptr)) || !FrameLogged()) return;
        static const uint32_t from = []() { const char* t = getenv("COD3_D3DDRAWDUMPFROM"); return t ? uint32_t(strtoul(t, nullptr, 10)) : 0u; }();
        int& number = g_dumpNumber;
        if (number < int(from)) { number++; return; }
        if (number >= int(from) + 2000) return;
        // A draw that writes only depth: the depth alone.
        if (texture == nullptr) { DumpDepth(prefix, number++, depth); return; }
        uint32_t w = 0, h = 0;
        const std::vector<uint8_t> pixels = ReadBack(texture, w, h, 4);
        if (pixels.empty()) return;
        if (withDepth) DumpDepth(prefix, number, depth);
        char name[512];
        snprintf(name, sizeof(name), "%s-%04d.bmp", prefix, number++);
        WriteBmp(name, pixels, w, h);
    }

    // COD3_FRAMEDUMP=path: the window's picture as a bitmap every
    // COD3_FRAMEDUMP_EVERY presents, cycling through COD3_FRAMEDUMP_KEEP files.
    void DumpFrame(ID3D11Texture2D* back)
    {
        static const char* const path = getenv("COD3_FRAMEDUMP");
        if (path == nullptr) return;
        static const int every = []() { const char* t = getenv("COD3_FRAMEDUMP_EVERY"); const int v = t ? int(strtol(t, nullptr, 10)) : 120; return v > 0 ? v : 120; }();
        static const int keep = []() { const char* t = getenv("COD3_FRAMEDUMP_KEEP"); const int v = t ? int(strtol(t, nullptr, 10)) : 40; return v > 0 ? v : 40; }();
        // COD3_FRAMEDUMP_FROM=N: nothing before the Nth present, so a run of
        // consecutive frames (EVERY=1) can be kept from late in a level
        // without writing every frame before it.
        static const int from = []() { const char* t = getenv("COD3_FRAMEDUMP_FROM"); return t ? int(strtol(t, nullptr, 10)) : 0; }();
        // COD3_FRAMEDUMP_UNTIL=N: nothing from the Nth present on.
        static const int until = []() { const char* t = getenv("COD3_FRAMEDUMP_UNTIL"); return t ? int(strtol(t, nullptr, 10)) : INT_MAX; }();
        static int presents = 0;
        if (presents++ < from || presents > until) return;
        static int counter = 0;
        if ((counter++ % every) != 0) return;
        // COD3_FRAMEDUMP_SHRINK=N: each side divided by N, for long runs.
        static const uint32_t shrink = []() { const char* t = getenv("COD3_FRAMEDUMP_SHRINK"); const long v = t ? strtol(t, nullptr, 10) : 1; return uint32_t(v >= 1 && v <= 16 ? v : 1); }();
        uint32_t w = 0, h = 0;
        const std::vector<uint8_t> pixels = ReadBack(back, w, h, shrink, g_screenshotWhite.load(std::memory_order_relaxed));
        if (pixels.empty()) return;
        char name[512];
        snprintf(name, sizeof(name), "%s-%03d.bmp", path, (counter / every) % keep);
        printf("render: frame dump %03d at present %d, swap %llu\n", (counter / every) % keep, counter - 1, (unsigned long long)g_swaps.load(std::memory_order_relaxed));
        WriteBmp(name, pixels, w, h);
    }

    // --- the draw ------------------------------------------------------------------------------------

    // 0.5.394: by the processor's time stamp counter, not the system clock:
    // two clock reads a draw were 2.7% of the command thread. The counter's
    // rate is measured against the clock as the game runs (TscNanoseconds).
    std::atomic<double> g_nanosecondsPerTick{ 0.3 };
    void CalibrateTsc()
    {
        static uint64_t tsc0 = 0;
        static std::chrono::steady_clock::time_point clock0;
        const uint64_t tsc = __rdtsc();
        const auto clock = std::chrono::steady_clock::now();
        if (tsc0 == 0) { tsc0 = tsc; clock0 = clock; return; }
        const double ns = double(std::chrono::duration_cast<std::chrono::nanoseconds>(clock - clock0).count());
        if (ns > 2e8 && tsc > tsc0) g_nanosecondsPerTick.store(ns / double(tsc - tsc0), std::memory_order_relaxed);
    }
    struct Timed
    {
        uint64_t& into;
        uint64_t started = __rdtsc();
        explicit Timed(uint64_t& i) : into(i) {}
        ~Timed() { into += uint64_t(double(__rdtsc() - started) * g_nanosecondsPerTick.load(std::memory_order_relaxed)); }
    };

    void BindTexture(int stage, uint32_t slot, ID3D11ShaderResourceView* view)
    {
        if (g_bound.textures[stage][slot] == view) return;
        g_bound.textures[stage][slot] = view;
        RenderStats::Frame().textureSwitches++;
        if (stage == 0) g_context->PSSetShaderResources(slot, 1, &view);
        else g_context->VSSetShaderResources(slot, 1, &view);
    }

    void BindSampler(int stage, uint32_t slot, ID3D11SamplerState* sampler)
    {
        if (g_bound.samplers[stage][slot] == sampler) return;
        g_bound.samplers[stage][slot] = sampler;
        if (stage == 0) g_context->PSSetSamplers(slot, 1, &sampler);
        else g_context->VSSetSamplers(slot, 1, &sampler);
    }

    // A view about to be a target cannot stay bound as a texture: the
    // host refuses and binds nothing. Only the slots holding it are cleared.
    void UnbindResource(ID3D11ShaderResourceView* view)
    {
        if (view == nullptr) return;
        for (int stage = 0; stage < 2; stage++)
            for (uint32_t slot = 0; slot < 32; slot++)
                if (g_bound.textures[stage][slot] == view) BindTexture(stage, slot, nullptr);
    }

    // The command on the context: each thing it names compared with what
    // is bound and set only when it differs, then the one draw call.
    // Verify mode: the layers over a surface (the decals, the bullet holes)
    // counted on the GPU. Each such draw is wrapped in an occlusion query,
    // read back without waiting at a later swap, and the first few hundred
    // logged with their frame, pixel program, vertex pool and index count:
    // a layer drawn every frame whose samples come and go is failing its
    // depth or alpha test on the GPU; one whose samples stay is being
    // covered or blended away after it.
    // Each is drawn a second time, with no depth test and no colour
    // written, into a second query: the samples it would have without the
    // depth test. A layer whose first count drops to nought while its
    // second stays is failing its depth test; one whose second drops too is
    // being thrown away by its own program (its alpha test, its texture).
    struct LayerQuery { ComPtr<ID3D11Query> query, open; uint64_t frame = 0, ps = 0; uint32_t vertices = 0, indices = 0; bool pending = false; };
    std::vector<LayerQuery> g_layerQueries;
    LayerQuery* LayerQueryBegin()
    {
        LayerQuery* free = nullptr;
        for (LayerQuery& q : g_layerQueries) if (!q.pending) { free = &q; break; }
        if (free == nullptr)
        {
            if (g_layerQueries.size() >= 256) return nullptr;
            g_layerQueries.emplace_back();
            free = &g_layerQueries.back();
            D3D11_QUERY_DESC desc{ D3D11_QUERY_OCCLUSION, 0 };
            if (FAILED(g_device->CreateQuery(&desc, &free->query)) || FAILED(g_device->CreateQuery(&desc, &free->open))) { g_layerQueries.pop_back(); return nullptr; }
        }
        g_context->Begin(free->query.Get());
        return free;
    }
    void LayerQueryEnd(LayerQuery* q, const RenderState::DrawCommand& command)
    {
        g_context->End(q->query.Get());
        // Again, open: no depth test, nothing written.
        static ComPtr<ID3D11DepthStencilState> noDepth;
        static ComPtr<ID3D11BlendState> noWrite;
        if (!noDepth)
        {
            D3D11_DEPTH_STENCIL_DESC d{};
            d.DepthEnable = FALSE;
            d.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
            d.DepthFunc = D3D11_COMPARISON_ALWAYS;
            g_device->CreateDepthStencilState(&d, &noDepth);
            D3D11_BLEND_DESC bd{};
            bd.RenderTarget[0].RenderTargetWriteMask = 0;
            g_device->CreateBlendState(&bd, &noWrite);
        }
        if (noDepth && noWrite)
        {
            g_context->OMSetDepthStencilState(noDepth.Get(), 0);
            g_context->OMSetBlendState(noWrite.Get(), nullptr, 0xFFFFFFFF);
            g_context->Begin(q->open.Get());
            if (command.indexed) g_context->DrawIndexed(command.indexCount, command.indexRingOffset / (command.indices32 ? 4 : 2), command.baseVertex);
            else g_context->Draw(command.indexCount, UINT(command.baseVertex));
            g_context->End(q->open.Get());
            // Bound again by the next draw.
            g_bound.depth = Handle(0xFFFFFFFFu);
            g_bound.blend = Handle(0xFFFFFFFFu);
            g_bound.stencilReference = 0xFFFFFFFFu;
            for (float& f : g_bound.blendFactor) f = -1.0f;
        }
        q->pending = true;
        q->frame = g_swaps.load(std::memory_order_relaxed);
        q->ps = command.pixelShader != 0 ? RenderShaders::HashOf(command.pixelShader) : 0;
        q->vertices = command.layerVertices;
        q->indices = command.indexCount;
    }
    void PollLayerQueries()
    {
        static int told = 0;
        for (LayerQuery& q : g_layerQueries)
        {
            if (!q.pending) continue;
            UINT64 samples = 0, open = 0;
            if (g_context->GetData(q.query.Get(), &samples, sizeof(samples), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) continue;
            if (g_context->GetData(q.open.Get(), &open, sizeof(open), D3D11_ASYNC_GETDATA_DONOTFLUSH) != S_OK) continue;
            q.pending = false;
            // Every layer program, by the pool its vertices came from: the
            // draws, and the samples with and without the depth test, put
            // in the log every five hundred frames (the tracers and the
            // other effects beside the bullet holes).
            {
                struct Sum { uint64_t draws = 0, samples = 0, open = 0, zero = 0; };
                static std::map<std::pair<uint64_t, uint32_t>, Sum> sums;
                static uint64_t since = 0;
                Sum& sum = sums[{ q.ps, q.vertices }];
                sum.draws++; sum.samples += samples; sum.open += open; if (samples == 0 && open != 0) sum.zero++;
                if (since == 0) since = q.frame;
                if (q.frame >= since + 500)
                {
                    printf("verify: layers by program and pool, frames %llu to %llu (draws; samples passed; without the depth test; draws that failed it all):\n",
                        (unsigned long long)since, (unsigned long long)q.frame);
                    for (const auto& [key, v] : sums)
                        if (v.draws >= 20)
                            printf("   ps %016llx pool %08X: %llu; %llu; %llu; %llu\n", (unsigned long long)key.first, key.second,
                                (unsigned long long)v.draws, (unsigned long long)v.samples, (unsigned long long)v.open, (unsigned long long)v.zero);
                    fflush(stdout);
                    sums.clear();
                    since = q.frame;
                }
            }
            // The bullet holes' program by default; COD3_LAYERPS=hash for another.
            static const uint64_t wanted = []() { const char* t = getenv("COD3_LAYERPS"); return t ? uint64_t(strtoull(t, nullptr, 16)) : 0xd6d768427524ea61ull; }();
            if (q.ps == wanted && told < 600)
            {
                told++;
                printf("verify: layer frame %llu ps %016llx vertices %08X, %u indices: %llu samples passed, %llu without the depth test\n",
                    (unsigned long long)q.frame, (unsigned long long)q.ps, q.vertices, q.indices, (unsigned long long)samples, (unsigned long long)open);
                fflush(stdout);
            }
        }
    }

    bool HdrScenePass(const RenderState::DrawCommand& command);   // the HDR section, below
    ID3D11PixelShader* HdrProgramFor(const char* pass, int toneMapper, uint32_t samples, bool fxaa);

    void ExecuteDraw(const RenderState::DrawCommand& command)
    {
        // The HDR mode: before the bloom's composite, the frame tone mapped
        // (its passes leave nothing bound); the composite then adds the
        // bloom over the tone mapped picture through hdr/cod3_hdr.hlsl's
        // bloom program, once the scene pass ran and the program is ready.
        ID3D11PixelShader* bloomProgram = nullptr;
        if (command.hdrScene && HdrScenePass(command)) bloomProgram = HdrProgramFor("bloom", 0, 1, false);
        RenderStats::Enter(RenderStats::SectionBind);
        if (command.bindingsLost) ForgetBindings();

        // Insert one batch after the opaque world, before foreground,
        // native transparent effects, post processing and menus. No particle
        // draw is required. The matrix belongs to this queued render frame.
        static uint64_t bloodFrame=~uint64_t(0);
        static bool bloodPending=false,bloodDrawn=false;static int opaqueAfter=0;
        static BloodRender::Projection bloodProjection{};
        static ID3D11RenderTargetView* bloodTarget=nullptr;
        static ID3D11DepthStencilView* bloodDepth=nullptr;
        if(bloodFrame!=command.bloodFrame){static int report=0;if(BloodDiag::On()&&report++%30==0){auto s=Blood::Status();BloodDiag::Log("execute frame=%llu submitted=%d pending=%d alive_decals=%d particles=%d opaque_world_draws_after_blood=%d",bloodFrame,bloodDrawn,bloodPending,s.decals,s.particles,opaqueAfter);}bloodFrame=command.bloodFrame;bloodPending=bloodDrawn=false;opaqueAfter=0;}
        if(command.bloodWorldStage&&bloodDrawn){++opaqueAfter;bloodPending=true;}
        if(command.bloodProjectionValid&&!bloodDrawn){
            bloodProjection=command.bloodProjection;bloodTarget=command.targets[0];bloodDepth=command.depthView;bloodPending=true;
        }
        if(bloodPending&&!command.bloodWorldStage&&command.bloodScene&&command.targets[0]==bloodTarget&&command.depthView==bloodDepth){
            bloodPending=false;bloodDrawn=true;
            UnbindResource(command.targetResources[0]);UnbindResource(command.depthResource);
            g_context->OMSetRenderTargets(1,&bloodTarget,bloodDepth);
            const D3D11_VIEWPORT viewport{0,0,float(command.targetWidth),float(command.targetHeight),0,1};
            g_context->RSSetViewports(1,&viewport);
            if(BloodDiag::On()){static uint64_t last=0;auto now=GetTickCount64();if(now-last>=1000){last=now;BloodDiag::Log("selected frame=%llu source_vs=%016llX source_pass=%s source_depth=%08X source_mask=%08X near=%.6f far=%.6f target=%p depth_target=%p trigger_is_world=%d",bloodProjection.diagFrame,bloodProjection.diagShader,bloodProjection.diagPass?"depth_only":"opaque_color_depth_write",bloodProjection.diagDepth,bloodProjection.diagMask,bloodProjection.diagNear,bloodProjection.diagFar,bloodTarget,bloodDepth,command.bloodWorldStage);for(int row=0;row<4;row++)BloodDiag::Log("selected_matrix frame=%llu row=%d values=%.9g,%.9g,%.9g,%.9g",bloodProjection.diagFrame,row,bloodProjection.matrix[row*4],bloodProjection.matrix[row*4+1],bloodProjection.matrix[row*4+2],bloodProjection.matrix[row*4+3]);}}
            BloodPixelsBefore(command.colorTexture,bloodProjection.diagFrame);
            BloodRender::Draw(g_device.Get(),g_context.Get(),bloodProjection);
            BloodPixelsAfter();
            // Target binding also changes when the batch is empty.
            ForgetBindings();
        }

        // The targets are set before the textures so a surface about to be
        // drawn into is not still bound as one.
        {
            bool same = command.targetCount == g_bound.targetCount && command.depthView == g_bound.depthView;
            for (uint32_t i = 0; same && i < 4; i++) same = command.targets[i] == g_bound.targets[i];
            if (!same)
            {
                for (uint32_t i = 0; i < 4; i++) if (command.targets[i] != nullptr) UnbindResource(command.targetResources[i]);
                if (command.depthView != nullptr) UnbindResource(command.depthResource);
                g_context->OMSetRenderTargets(4, command.targets, command.depthView);
                memcpy(g_bound.targets, command.targets, sizeof(g_bound.targets));
                g_bound.depthView = command.depthView;
                g_bound.targetCount = command.targetCount;
                RenderStats::Frame().targetSwitches++;
            }
            if (g_bound.viewportWidth != command.targetWidth || g_bound.viewportHeight != command.targetHeight)
            {
                const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, float(command.targetWidth), float(command.targetHeight), 0.0f, 1.0f };
                g_context->RSSetViewports(1, &viewport);
                g_bound.viewportWidth = command.targetWidth;
                g_bound.viewportHeight = command.targetHeight;
            }
            const D3D11_RECT scissor{ command.scissor[0], command.scissor[1], command.scissor[2], command.scissor[3] };
            if (memcmp(&scissor, &g_bound.scissor, sizeof(scissor)) != 0)
            {
                g_context->RSSetScissorRects(1, &scissor);
                g_bound.scissor = scissor;
            }
        }

        // The textures, samplers and vertex buffers, by slot.
        for (uint32_t i = 0; i < command.textureCount; i++)
        {
            const RenderState::DrawCommand::Texture& texture = command.textures[i];
            BindTexture(texture.stage, texture.slot, texture.view);
            BindSampler(texture.stage, texture.samplerSlot, texture.sampler);
        }
        for (uint32_t i = 0; i < command.bufferCount; i++)
        {
            const RenderState::DrawCommand::Buffer& buffer = command.buffers[i];
            if (g_bound.vertexBuffers[buffer.slot] == buffer.view) continue;
            g_bound.vertexBuffers[buffer.slot] = buffer.view;
            g_context->VSSetShaderResources(32 + buffer.slot, 1, &buffer.view);
        }

        // The constants: the booleans when they changed, the float files
        // and the draw block where the ring has them, bound again whenever
        // an offset moved (the binding carries it); or, without the ring,
        // uploaded in place and bound once.
        if (command.boolsChanged) g_context->UpdateSubresource(g_boolConstants.Get(), 0, nullptr, command.bools, 0, 0);
        if (command.ring)
        {
            if (!g_bound.constantsBound || g_bound.floatAt[0] != command.floatAt[0] || g_bound.floatAt[1] != command.floatAt[1] ||
                g_bound.drawAt != command.drawAt || g_bound.floatCount[0] != command.floatCount[0] || g_bound.floatCount[1] != command.floatCount[1])
            {
                ID3D11Buffer* ringBuffer = RenderResources::ConstantRing();
                ID3D11Buffer* bools = g_boolConstants.Get();
                for (int stage = 0; stage < 2; stage++)
                {
                    ID3D11Buffer* buffers[3] = { ringBuffer, bools, ringBuffer };
                    const UINT firsts[3] = { command.floatAt[stage] / 16, 0, command.drawAt / 16 };
                    const UINT counts[3] = { command.floatCount[stage], 16, command.drawBytes / 16 };
                    if (stage == 0) g_context1->VSSetConstantBuffers1(0, 3, buffers, firsts, counts);
                    else g_context1->PSSetConstantBuffers1(0, 3, buffers, firsts, counts);
                }
                g_bound.floatAt[0] = command.floatAt[0]; g_bound.floatAt[1] = command.floatAt[1];
                g_bound.floatCount[0] = command.floatCount[0]; g_bound.floatCount[1] = command.floatCount[1];
                g_bound.drawAt = command.drawAt;
                g_bound.constantsBound = true;
            }
        }
        else
        {
            for (int stage = 0; stage < 2; stage++)
                if (command.floatChanged[stage]) g_context->UpdateSubresource(g_floatConstants[stage].Get(), 0, nullptr, command.floatFile[stage], 0, 0);
            if (command.drawChanged) g_context->UpdateSubresource(g_drawConstants.Get(), 0, nullptr, command.drawConstants, 0, 0);
            if (!g_bound.constantsBound)
            {
                ID3D11Buffer* buffers[3] = { g_floatConstants[0].Get(), g_boolConstants.Get(), g_drawConstants.Get() };
                g_context->VSSetConstantBuffers(0, 3, buffers);
                ID3D11Buffer* pixelBuffers[3] = { g_floatConstants[1].Get(), g_boolConstants.Get(), g_drawConstants.Get() };
                g_context->PSSetConstantBuffers(0, 3, pixelBuffers);
                g_bound.constantsBound = true;
            }
        }
        // The HDR bloom program reads the HDR settings at b1, where the
        // game's programs have their booleans: bound for it, and the
        // booleans bound again by the next draw.
        if (bloomProgram != nullptr && g_hdrConstants)
        {
            ID3D11Buffer* hdr[1] = { g_hdrConstants.Get() };
            g_context->PSSetConstantBuffers(1, 1, hdr);
            g_bound.constantsBound = false;
        }

        // The programs and the states, when they changed.
        if (g_bound.vertexShader != command.vertexShader)
        {
            g_context->VSSetShader(command.vs, nullptr, 0);
            g_bound.vertexShader = command.vertexShader;
            RenderStats::Frame().shaderSwitches++;
        }
        {
            const bool flat = g_flatPixelShader && command.opaque;
            // The HDR bloom program has a handle of its own, so the binding
            // is made again on either side of it.
            const Handle pixelShader = bloomProgram != nullptr ? Handle(0xFFFFFFF0u) : command.pixelShader;
            if (g_bound.pixelShader != pixelShader || g_bound.flat != flat)
            {
                ID3D11PixelShader* program = flat ? g_flatPixelShader.Get() : bloomProgram != nullptr ? bloomProgram : command.ps;
                // No pixel program: the depth cutting one, except for a shadow
                // map's casters (render_commands.cpp's NoPixelProgramFast).
                if (program == nullptr && command.pixelShader != 0xFFFFFFFEu) program = g_depthOnlyPixelShader.Get();
                g_context->PSSetShader(program, nullptr, 0);
                g_bound.pixelShader = pixelShader;
                g_bound.flat = flat;
                RenderStats::Frame().shaderSwitches++;
            }
        }
        if (g_bound.rectangles != command.rectangles)
        {
            if (!BuiltInPrograms()) return;
            g_context->GSSetShader(command.rectangles ? g_rectangleShader.Get() : nullptr, nullptr, 0);
            g_bound.rectangles = command.rectangles;
        }
        if (g_bound.rasterizer != command.rasterizer)
        {
            g_context->RSSetState(command.rasterizerObject);
            g_bound.rasterizer = command.rasterizer;
            RenderStats::Frame().pipelineSwitches++;
        }
        if (g_bound.blend != command.blend || memcmp(g_bound.blendFactor, command.blendFactor, sizeof(command.blendFactor)) != 0)
        {
            g_context->OMSetBlendState(command.blendObject, command.blendFactor, 0xFFFFFFFF);
            g_bound.blend = command.blend;
            memcpy(g_bound.blendFactor, command.blendFactor, sizeof(command.blendFactor));
            RenderStats::Frame().pipelineSwitches++;
        }
        if (g_bound.depth != command.depth || g_bound.stencilReference != command.stencilReference)
        {
            g_context->OMSetDepthStencilState(command.depthObject, command.stencilReference);
            g_bound.depth = command.depth;
            g_bound.stencilReference = command.stencilReference;
            RenderStats::Frame().pipelineSwitches++;
        }
        const D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY(command.topology);
        if (g_bound.topology != topology)
        {
            g_context->IASetPrimitiveTopology(topology);
            g_bound.topology = topology;
        }
        // The ring, or a buffer of the draw's indices kept converted.
        ID3D11Buffer* const indexBuffer = command.indexBuffer != nullptr ? command.indexBuffer : RenderResources::IndexRing();
        if (command.indexed && (!g_bound.indexBufferSet || g_bound.indices32 != command.indices32 || g_bound.indexBuffer != indexBuffer))
        {
            g_context->IASetIndexBuffer(indexBuffer, command.indices32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT, 0);
            g_bound.indexBufferSet = true;
            g_bound.indices32 = command.indices32;
            g_bound.indexBuffer = indexBuffer;
        }

        RenderStats::Enter(RenderStats::SectionDraw);
        RenderStats::Counters& counters = RenderStats::Frame();
        counters.draws++;
        counters.triangles += command.triangles;
        g_drawsTotal++;
        // VGT_INDX_OFFSET is added to every vertex index, which is how the
        // terrain draws its patches out of one buffer with one set of
        // indices; the host adds it as the base vertex.
        LayerQuery* layerQuery = command.layerQuery ? LayerQueryBegin() : nullptr;
        if (command.indexed) g_context->DrawIndexed(command.indexCount, command.indexRingOffset / (command.indices32 ? 4 : 2), command.baseVertex);
        else g_context->Draw(command.indexCount, UINT(command.baseVertex));
        if (layerQuery != nullptr) LayerQueryEnd(layerQuery, command);
        // The key: the pixel program's hash (3 for none), and the target's
        // width in the top 16 bits, to see which targets the time goes to.
        if (RenderStats::DrawsTimed())
            RenderStats::DrawTimed(((command.ps != nullptr ? RenderShaders::HashOf(command.pixelShader) : 3) & 0x0000FFFFFFFFFFFFull) |
                                   (uint64_t(command.targetWidth & 0xFFFF) << 48));

        // COD3_D3DDRAWDUMPPS=hex: only after the draws whose pixel program's
        // hash starts so, for watching one kind of draw over many frames.
        static const char* const dumpPixel = getenv("COD3_D3DDRAWDUMPPS");
        bool dumpThis = command.logged && (command.colorTexture != nullptr || command.depthTexture != nullptr);
        if (dumpThis && dumpPixel != nullptr)
        {
            char name[24];
            snprintf(name, sizeof(name), "%016llx", (unsigned long long)RenderShaders::HashOf(command.pixelShader));
            dumpThis = strncmp(name, dumpPixel, strlen(dumpPixel)) == 0;
        }
        if (dumpThis) DumpTargetAfterDraw(command.colorTexture, command.depthTexture);
        PicturesAtDraw(command);
    }

    // The screenshot key's per draw pictures: after every draw of the first
    // frame it logs, the colour target made small on the GPU (its mip chain,
    // so thin things leave a trace) and read back, kept when it differs from
    // the one before, all in one file beside the screenshot
    // (screenshots/CoD3-<moment>-draws.bin): per picture the word 'DRAW',
    // the draw's number in the frame (the order of the frame log's draw
    // lines), the width and the height, then its RGBA bytes. Which draw
    // brought a fault in is then a look through the pictures.
    struct DrawPictures
    {
        FILE* file = nullptr;
        uint32_t number = 0, kept = 0;
        std::vector<uint8_t> last;
        ComPtr<ID3D11Texture2D> copy, thumb, staging;
        ComPtr<ID3D11ShaderResourceView> copyView;
        ComPtr<ID3D11RenderTargetView> thumbView;
        uint32_t copyWidth = 0, copyHeight = 0, thumbHeight = 0;
        DXGI_FORMAT copyFormat = DXGI_FORMAT_UNKNOWN;
    };
    DrawPictures g_pictures;
    std::atomic<int64_t> g_picturesSwap{ -1 };   // the swap whose draws are pictured
    std::mutex g_picturesMutex;
    std::wstring g_picturesPath;

    void PictureAfterDraw(ID3D11Texture2D* texture)
    {
        DrawPictures& p = g_pictures;
        const uint32_t number = p.number++;
        if (texture == nullptr || p.file == nullptr) return;
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        if (desc.Format != DXGI_FORMAT_R8G8B8A8_UNORM && desc.Format != DXGI_FORMAT_B8G8R8A8_UNORM && desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT) return;
        constexpr uint32_t Width = 160;
        if (!p.copy || p.copyWidth != desc.Width || p.copyHeight != desc.Height || p.copyFormat != desc.Format)
        {
            p.copy.Reset(); p.copyView.Reset(); p.thumb.Reset(); p.thumbView.Reset(); p.staging.Reset();
            D3D11_TEXTURE2D_DESC c{};
            c.Width = desc.Width; c.Height = desc.Height; c.MipLevels = 0; c.ArraySize = 1; c.Format = desc.Format;
            c.SampleDesc.Count = 1; c.Usage = D3D11_USAGE_DEFAULT;
            c.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET; c.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
            if (FAILED(g_device->CreateTexture2D(&c, nullptr, &p.copy))) return;
            g_device->CreateShaderResourceView(p.copy.Get(), nullptr, &p.copyView);
            p.thumbHeight = std::max(1u, uint32_t(uint64_t(Width) * desc.Height / desc.Width));
            D3D11_TEXTURE2D_DESC s = c;
            s.Width = Width; s.Height = p.thumbHeight; s.MipLevels = 1; s.MiscFlags = 0; s.BindFlags = D3D11_BIND_RENDER_TARGET;
            // The picture 8 bit whatever the target (the HDR mode's are
            // 16 bit float): the copy program's output clips it at one.
            if (desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) s.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            if (FAILED(g_device->CreateTexture2D(&s, nullptr, &p.thumb))) return;
            g_device->CreateRenderTargetView(p.thumb.Get(), nullptr, &p.thumbView);
            s.Usage = D3D11_USAGE_STAGING; s.BindFlags = 0; s.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            if (FAILED(g_device->CreateTexture2D(&s, nullptr, &p.staging))) return;
            p.copyWidth = desc.Width; p.copyHeight = desc.Height; p.copyFormat = desc.Format;
            p.last.clear();
        }
        if (desc.SampleDesc.Count > 1) g_context->ResolveSubresource(p.copy.Get(), 0, texture, 0, desc.Format);
        else g_context->CopySubresourceRegion(p.copy.Get(), 0, 0, 0, 0, texture, 0, nullptr);
        g_context->GenerateMips(p.copyView.Get());
        const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, float(Width), float(p.thumbHeight), 0.0f, 1.0f };
        DrawQuad(p.copyView.Get(), desc.Width, desc.Height, 0, 0, float(desc.Width), float(desc.Height),
                 p.thumbView.Get(), viewport, g_copyPixelShader.Get(), g_linearSampler.Get());
        ID3D11RenderTargetView* noTargets[1] = { nullptr };
        g_context->OMSetRenderTargets(1, noTargets, nullptr);
        ForgetBindings();
        g_context->CopyResource(p.staging.Get(), p.thumb.Get());
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(g_context->Map(p.staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return;
        std::vector<uint8_t> pixels(size_t(Width) * p.thumbHeight * 4);
        const bool bgra = desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM;
        for (uint32_t y = 0; y < p.thumbHeight; y++)
        {
            const uint8_t* row = static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch;
            uint8_t* to = pixels.data() + size_t(y) * Width * 4;
            for (uint32_t x = 0; x < Width; x++)
            {
                to[x * 4 + 0] = row[x * 4 + (bgra ? 2 : 0)];
                to[x * 4 + 1] = row[x * 4 + 1];
                to[x * 4 + 2] = row[x * 4 + (bgra ? 0 : 2)];
                to[x * 4 + 3] = row[x * 4 + 3];
            }
        }
        g_context->Unmap(p.staging.Get(), 0);
        if (pixels == p.last) return;
        const uint32_t header[4] = { 0x57415244u, number, Width, p.thumbHeight };   // 'DRAW'
        fwrite(header, sizeof(header), 1, p.file);
        fwrite(pixels.data(), 1, pixels.size(), p.file);
        p.last.swap(pixels);
        p.kept++;
    }

    // At each draw: the pictures of the swap asked for, opening and closing
    // their file at its start and end.
    void PicturesAtDraw(const RenderState::DrawCommand& command)
    {
        const int64_t wanted = g_picturesSwap.load(std::memory_order_relaxed);
        DrawPictures& p = g_pictures;
        const int64_t swap = int64_t(g_swaps.load(std::memory_order_relaxed));
        if (p.file != nullptr && swap != wanted)
        {
            fclose(p.file);
            p.file = nullptr;
            printf("render: the screenshot key: %u of %u draws pictured\n", p.kept, p.number);
            fflush(stdout);
            g_picturesSwap.store(-1, std::memory_order_relaxed);
        }
        if (wanted < 0 || swap != wanted) return;
        if (p.file == nullptr)
        {
            std::wstring path;
            { std::lock_guard<std::mutex> lock(g_picturesMutex); path = g_picturesPath; }
            if (path.empty() || _wfopen_s(&p.file, path.c_str(), L"wb") != 0) { p.file = nullptr; g_picturesSwap.store(-1); return; }
            p.number = 0; p.kept = 0; p.last.clear();
        }
        PictureAfterDraw(command.colorTexture);
    }

    // --- the resolve ----------------------------------------------------------------------------------

    // The console's 24 bit float depth, 20e4: the host's float cut to four
    // bits of exponent and twenty of mantissa, as Xenia does it.
    uint32_t Float32To20e4(float value)
    {
        if (!(value > 0.0f)) return 0;
        uint32_t bits;
        memcpy(&bits, &value, 4);
        bits = std::min(bits, 0x3FFFFFF8u);
        if (bits < 0x38800000u)
        {
            const uint32_t shift = std::min(113u - (bits >> 23), 24u);
            bits = (0x800000u | (bits & 0x7FFFFFu)) >> shift;
        }
        else bits += 0xC8000000u;
        bits += 3u + ((bits >> 3) & 1u);
        return (bits >> 3) & 0xFFFFFFu;
    }

    // A texel's place in the console's 2D tiled surface, in bytes (as
    // render_resources.cpp's, from Xenia).
    uint32_t TiledByteOffset(uint32_t x, uint32_t y, uint32_t pitch, uint32_t bytesPerTexelLog2)
    {
        const uint32_t alignedPitch = (pitch + 31u) & ~31u;
        const uint32_t macro = ((x >> 5) + (y >> 5) * (alignedPitch >> 5)) << (bytesPerTexelLog2 + 7);
        const uint32_t micro = ((x & 7u) + ((y & 0xEu) << 2)) << bytesPerTexelLog2;
        const uint32_t offset = macro + ((micro & ~0xFu) << 1) + (micro & 0xFu) + ((y & 1u) << 4);
        return ((offset & ~0x1FFu) << 3) + ((y & 16u) << 7) + ((offset & 0x1C0u) << 2)
             + (((((y & 8u) >> 2) + (x >> 3)) & 3u) << 6) + (offset & 0x3Fu);
    }

    // The depth copies written back into the title's memory, where the
    // console's resolve leaves them. The title reads the depth there itself:
    // it decides on the CPU whether each light's glow is hidden (the glows
    // are drawn in front of everything, at depth 1, and left out when
    // covered), and with nothing written there every glow on the map shone
    // through the walls. The copy goes to a staging texture and is read
    // a frame or two later, when the GPU is done with it, so the frame does
    // not wait; the title sees the depth that late, as glows fading in and
    // out a frame behind, which is not to be seen.
    // debug.txt depth_writeback 0 leaves it out, to compare.
    //
    // The title reads it at its own size (1040 by 624 for the frame, 512 by
    // 512 for a shadow cascade), but the copy read back was the host's: at
    // 4K the frame's depth alone is 31 MB across the bus to the CPU, and
    // with the shadow maps over a hundred a frame, which measured 5 to 6 ms
    // of the GPU's frame (the "resolve depth copy" lines of COD3_GPU_DRAWS,
    // each the size of its copy over the bus's speed). So the copy is first
    // brought down to the title's size on the GPU, a point sampled pass into
    // a small target, and only that is read back: 2.6 MB for the frame.
    // COD3_FULLDEPTHREADBACK=1 reads the host's whole copy as before.
    struct DepthWriteback
    {
        ComPtr<ID3D11Texture2D> staging;
        ComPtr<ID3D11Texture2D> shrunk;             // the copy at the title's size (not "small": a Windows macro)
        ComPtr<ID3D11RenderTargetView> shrunkView;
        D3D11_TEXTURE2D_DESC desc{};
        bool pending = false;
        uint64_t order = 0;
        uint32_t hostWidth = 0, hostHeight = 0;
        uint32_t guestBase = 0, guestPitch = 0, guestWidth = 0, guestHeight = 0, guestEndian = 0;
        bool guestFloatDepth = false;
        bool mapped = false;               // read back and mapped, the depth thread on it or done with it
        bool converting = false;           // the depth thread not yet done (under g_depthJobMutex)
        std::vector<uint32_t> columns;
    };
    // Enough slots for two frames' depth copies (the frame's, and its
    // shadow maps', three to five a frame) to be in flight: the device
    // runs up to two frames behind when the GPU is the limit, and with six
    // a new copy found its slot still on the GPU and waited for it there,
    // the command thread stopped until the GPU caught up.
    constexpr int DepthWritebackSlots = 12;
    DepthWriteback g_depthWritebacks[DepthWritebackSlots];
    uint64_t g_depthWritebackOrder = 0;

    // The conversion of a depth copy into the title's memory, from the
    // texels read back. A 32 by 32 tile of four byte texels is one 4 KB
    // block of the surface, wherever it is, and where a texel lands inside
    // its tile depends only on its place in the tile: so the tiled address
    // is the tile's block plus an entry of this table, not the twenty
    // operations of TiledByteOffset for every texel. (The two agree for any
    // pitch: checked against TiledByteOffset for every texel of every pitch
    // the title uses.)
    struct TileTable
    {
        uint16_t offset[32][32];
        TileTable()
        {
            for (uint32_t y = 0; y < 32; y++)
                for (uint32_t x = 0; x < 32; x++)
                    offset[y][x] = uint16_t(TiledByteOffset(x, y, 32, 2));
        }
    };
    const TileTable& Tiles() { static const TileTable table; return table; }

    // One texel's depth as the console keeps it: 24 bits, fixed or 20e4,
    // above eight of stencil, in the surface's byte order. Made once for
    // each of the eight kinds, so the loop has no branch on either.
    template <bool FloatDepth, uint32_t Endian>
    inline uint32_t EncodeDepth(float z)
    {
        uint32_t depth;
        if constexpr (FloatDepth) depth = Float32To20e4(z);
        else depth = uint32_t((z > 0.0f ? (z < 1.0f ? z : 1.0f) : 0.0f) * 16777215.0f + 0.5f);
        uint32_t value = depth << 8;
        if constexpr (Endian == 1) value = ((value & 0xFF00FF00u) >> 8) | ((value & 0x00FF00FFu) << 8);
        else if constexpr (Endian == 2) value = __builtin_bswap32(value);
        else if constexpr (Endian == 3) value = (value >> 16) | (value << 16);
        return value;
    }

    struct DepthRows
    {
        const uint8_t* texels; uint32_t rowPitch;
        uint8_t* destination;
        const uint32_t* column;   // the host column of each of the title's, or null when they are the same
        uint32_t hostWidth, hostHeight, guestPitch, guestWidth, guestHeight;
    };

    template <bool FloatDepth, uint32_t Endian>
    void ConvertDepthRows(const DepthRows& d, uint32_t first, uint32_t last)
    {
        const TileTable& tiles = Tiles();
        const size_t tilesAcross = ((d.guestPitch + 31u) & ~31u) >> 5;
        for (uint32_t y = first; y < last; y++)
        {
            const uint32_t hy = d.guestHeight == d.hostHeight ? y
                : std::min(d.hostHeight - 1, uint32_t(uint64_t(y) * d.hostHeight / d.guestHeight));
            const float* row = reinterpret_cast<const float*>(d.texels + size_t(hy) * d.rowPitch);
            const uint16_t* inTile = tiles.offset[y & 31u];
            uint8_t* tileRow = d.destination + size_t(y >> 5) * tilesAcross * 4096u;
            for (uint32_t x0 = 0; x0 < d.guestWidth; x0 += 32)
            {
                uint8_t* block = tileRow + size_t(x0 >> 5) * 4096u;
                const uint32_t n = std::min(32u, d.guestWidth - x0);
                if (d.column == nullptr)
                {
                    const float* source = row + x0;
                    uint32_t i = 0;
                    // 0.5.406: the float (20e4) depth four texels at a time
                    // (SSE4.1), the common case: every texel above zero and
                    // none below the 20e4 normal range, else the four go
                    // one by one as before.
                    if constexpr (FloatDepth)
                        for (; i + 4 <= n; i += 4)
                        {
                            const __m128 z = _mm_loadu_ps(source + i);
                            __m128i bits = _mm_castps_si128(z);
                            const int positive = _mm_movemask_ps(_mm_cmpgt_ps(z, _mm_setzero_ps()));
                            bits = _mm_min_epu32(bits, _mm_set1_epi32(0x3FFFFFF8));
                            const __m128i low = _mm_cmplt_epi32(bits, _mm_set1_epi32(0x38800000));   // bits <= 0x3FFFFFF8: signed compare is safe
                            if (positive != 15 || _mm_movemask_epi8(low) != 0)
                            {
                                for (uint32_t k = i; k < i + 4; k++)
                                {
                                    const uint32_t value = EncodeDepth<FloatDepth, Endian>(source[k]);
                                    memcpy(block + inTile[k], &value, 4);
                                }
                                continue;
                            }
                            bits = _mm_add_epi32(bits, _mm_set1_epi32(int(0xC8000000u)));
                            bits = _mm_add_epi32(bits, _mm_add_epi32(_mm_set1_epi32(3), _mm_and_si128(_mm_srli_epi32(bits, 3), _mm_set1_epi32(1))));
                            __m128i value = _mm_slli_epi32(_mm_and_si128(_mm_srli_epi32(bits, 3), _mm_set1_epi32(0xFFFFFF)), 8);
                            if constexpr (Endian == 1) value = _mm_or_si128(_mm_srli_epi32(_mm_and_si128(value, _mm_set1_epi32(int(0xFF00FF00u))), 8), _mm_slli_epi32(_mm_and_si128(value, _mm_set1_epi32(0x00FF00FF)), 8));
                            else if constexpr (Endian == 2) value = _mm_shuffle_epi8(value, _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
                            else if constexpr (Endian == 3) value = _mm_or_si128(_mm_srli_epi32(value, 16), _mm_slli_epi32(value, 16));
                            alignas(16) uint32_t out[4];
                            _mm_store_si128(reinterpret_cast<__m128i*>(out), value);
                            memcpy(block + inTile[i], &out[0], 4);
                            memcpy(block + inTile[i + 1], &out[1], 4);
                            memcpy(block + inTile[i + 2], &out[2], 4);
                            memcpy(block + inTile[i + 3], &out[3], 4);
                        }
                    for (; i < n; i++)
                    {
                        const uint32_t value = EncodeDepth<FloatDepth, Endian>(source[i]);
                        memcpy(block + inTile[i], &value, 4);
                    }
                }
                else
                {
                    const uint32_t* columns = d.column + x0;
                    for (uint32_t i = 0; i < n; i++)
                    {
                        const uint32_t value = EncodeDepth<FloatDepth, Endian>(row[columns[i]]);
                        memcpy(block + inTile[i], &value, 4);
                    }
                }
            }
        }
    }

    using ConvertRows = void (*)(const DepthRows&, uint32_t, uint32_t);
    ConvertRows ConverterFor(bool floatDepth, uint32_t endian)
    {
        static const ConvertRows table[2][4] = {
            { ConvertDepthRows<false, 0>, ConvertDepthRows<false, 1>, ConvertDepthRows<false, 2>, ConvertDepthRows<false, 3> },
            { ConvertDepthRows<true, 0>,  ConvertDepthRows<true, 1>,  ConvertDepthRows<true, 2>,  ConvertDepthRows<true, 3> },
        };
        return table[floatDepth ? 1 : 0][endian & 3u];
    }

    // The conversion is off the command processor: it ran there, a depth
    // copy or more a frame (the frame's and three to five shadow maps',
    // over a million texels), and was the largest single part of the
    // command thread's time with the native command lists on. The title
    // reads these a frame or two late anyway, so a moment more on a thread
    // of its own changes nothing it sees. The staging copy stays mapped
    // while the thread works on it and is unmapped by the command thread
    // when it is done; one thread, so two copies to the same place land in
    // order. COD3_SYNCDEPTHWRITEBACK=1 converts on the command thread.
    struct DepthJob { DepthWriteback* slot; DepthRows rows; ConvertRows convert; };
    std::mutex g_depthJobMutex;
    std::condition_variable g_depthJobWake, g_depthJobDone;
    std::deque<DepthJob> g_depthJobs;

    void DepthWorker()
    {
        for (;;)
        {
            DepthJob job;
            {
                std::unique_lock<std::mutex> lock(g_depthJobMutex);
                g_depthJobWake.wait(lock, [] { return !g_depthJobs.empty(); });
                job = g_depthJobs.front();
                g_depthJobs.pop_front();
            }
            job.convert(job.rows, 0, job.rows.guestHeight);
            {
                std::lock_guard<std::mutex> lock(g_depthJobMutex);
                job.slot->converting = false;
            }
            g_depthJobDone.notify_all();
        }
    }

    bool DepthWorkerRunning()
    {
        static const bool running = [] {
            if (getenv("COD3_SYNCDEPTHWRITEBACK") != nullptr) return false;
            try { std::thread(DepthWorker).detach(); return true; }
            catch (...) { return false; }
        }();
        return running;
    }

    // A slot the depth thread has finished with goes back: unmapped, free.
    // With `wait`, it waits for the thread first.
    void ReleaseDepthWriteback(DepthWriteback& slot, bool wait)
    {
        if (!slot.mapped) return;
        {
            std::unique_lock<std::mutex> lock(g_depthJobMutex);
            if (slot.converting)
            {
                if (!wait) return;
                g_depthJobDone.wait(lock, [&] { return !slot.converting; });
            }
        }
        g_context->Unmap(slot.staging.Get(), 0);
        slot.mapped = false;
    }

    void FinishDepthWriteback(DepthWriteback& slot, bool wait)
    {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        const HRESULT hr = g_context->Map(slot.staging.Get(), 0, D3D11_MAP_READ, wait ? 0 : D3D11_MAP_FLAG_DO_NOT_WAIT, &mapped);
        if (hr == DXGI_ERROR_WAS_STILL_DRAWING) return;
        slot.pending = false;
        if (FAILED(hr)) return;
        const DepthWriteback& w = slot;
        DepthRows rows{};
        rows.texels = static_cast<const uint8_t*>(mapped.pData);
        rows.rowPitch = mapped.RowPitch;
        rows.destination = Guest::Base + Guest::PhysicalAlias(w.guestBase);
        rows.hostWidth = w.hostWidth; rows.hostHeight = w.hostHeight;
        rows.guestPitch = w.guestPitch; rows.guestWidth = w.guestWidth; rows.guestHeight = w.guestHeight;
        // Which host column each of the title's columns reads, when the
        // two differ (the whole host copy read back), worked out once
        // rather than with a division for every texel.
        if (w.hostWidth != w.guestWidth)
        {
            slot.columns.resize(w.guestWidth);
            for (uint32_t x = 0; x < w.guestWidth; x++)
                slot.columns[x] = std::min(w.hostWidth - 1, uint32_t(uint64_t(x) * w.hostWidth / w.guestWidth));
            rows.column = slot.columns.data();
        }
        const ConvertRows convert = ConverterFor(w.guestFloatDepth, w.guestEndian);
        if (Tuning::On(Tuning::depthThread) && DepthWorkerRunning())
        {
            slot.mapped = true;
            {
                std::lock_guard<std::mutex> lock(g_depthJobMutex);
                slot.converting = true;
                g_depthJobs.push_back(DepthJob{ &slot, rows, convert });
            }
            g_depthJobWake.notify_one();
            return;
        }
        Parallel::For(0, int(w.guestHeight), 32, [&](int first, int last) { convert(rows, uint32_t(first), uint32_t(last)); });
        g_context->Unmap(slot.staging.Get(), 0);
    }

    // The copies the GPU has finished, oldest first, into the title's
    // memory; and the slots the depth thread is done with, back.
    void PollDepthWritebacks()
    {
        for (DepthWriteback& slot : g_depthWritebacks)
            if (slot.mapped) ReleaseDepthWriteback(slot, false);
        for (;;)
        {
            DepthWriteback* oldest = nullptr;
            for (DepthWriteback& slot : g_depthWritebacks)
                if (slot.pending && (oldest == nullptr || slot.order < oldest->order)) oldest = &slot;
            if (oldest == nullptr) return;
            FinishDepthWriteback(*oldest, false);
            if (oldest->pending) return;   // still drawing: the later ones are too
        }
    }

    void WriteDepthBack(const RenderState::ResolveCommand& command)
    {
        if (command.destinationResource == nullptr || command.guestWidth == 0 || command.guestHeight == 0) return;
        ComPtr<ID3D11Resource> resource;
        command.destinationResource->GetResource(&resource);
        ComPtr<ID3D11Texture2D> texture;
        if (FAILED(resource.As(&texture))) return;
        D3D11_TEXTURE2D_DESC desc{};
        texture->GetDesc(&desc);
        if (desc.Format != DXGI_FORMAT_R32_FLOAT && desc.Format != DXGI_FORMAT_R32_TYPELESS)
        {
            static bool said = false;
            if (!said) { said = true; printf("render: a depth copy of format %u is not written back to the title's memory\n", unsigned(desc.Format)); }
            return;
        }
        static const bool fullReadback = getenv("COD3_FULLDEPTHREADBACK") != nullptr;
        const uint32_t copiedWidth = std::max(1u, std::min<uint32_t>(command.width ? command.width : desc.Width, desc.Width));
        const uint32_t copiedHeight = std::max(1u, std::min<uint32_t>(command.height ? command.height : desc.Height, desc.Height));
        const bool shrink = !fullReadback && (command.guestWidth < copiedWidth || command.guestHeight < copiedHeight);
        PollDepthWritebacks();
        // The slot used longest ago; if the GPU or the depth thread still
        // has it, this waits.
        DepthWriteback* slot = &g_depthWritebacks[0];
        auto busy = [](const DepthWriteback& s) { return s.pending || s.mapped; };
        for (DepthWriteback& candidate : g_depthWritebacks)
        {
            if (!busy(candidate)) { if (busy(*slot) || candidate.order < slot->order) slot = &candidate; }
            else if (busy(*slot) && candidate.order < slot->order) slot = &candidate;
        }
        if (slot->pending) FinishDepthWriteback(*slot, true);
        if (slot->mapped) ReleaseDepthWriteback(*slot, true);
        // What is read back: the host's copy, or the title's size of it.
        const uint32_t readWidth = shrink ? command.guestWidth : desc.Width;
        const uint32_t readHeight = shrink ? command.guestHeight : desc.Height;
        if (!slot->staging || slot->desc.Width != readWidth || slot->desc.Height != readHeight || slot->desc.Format != desc.Format)
        {
            slot->staging.Reset();
            slot->shrunk.Reset();
            slot->shrunkView.Reset();
            D3D11_TEXTURE2D_DESC s = desc;
            s.Width = readWidth; s.Height = readHeight;
            s.MipLevels = 1; s.ArraySize = 1; s.SampleDesc.Count = 1; s.SampleDesc.Quality = 0;
            s.Usage = D3D11_USAGE_STAGING; s.BindFlags = 0; s.CPUAccessFlags = D3D11_CPU_ACCESS_READ; s.MiscFlags = 0;
            if (FAILED(g_device->CreateTexture2D(&s, nullptr, &slot->staging))) return;
            slot->desc = s;
        }
        if (shrink)
        {
            if (!slot->shrunk)
            {
                D3D11_TEXTURE2D_DESC t{};
                t.Width = readWidth; t.Height = readHeight; t.MipLevels = 1; t.ArraySize = 1;
                t.Format = DXGI_FORMAT_R32_FLOAT; t.SampleDesc.Count = 1;
                t.Usage = D3D11_USAGE_DEFAULT; t.BindFlags = D3D11_BIND_RENDER_TARGET;
                if (FAILED(g_device->CreateTexture2D(&t, nullptr, &slot->shrunk))) return;
                if (FAILED(g_device->CreateRenderTargetView(slot->shrunk.Get(), nullptr, &slot->shrunkView))) { slot->shrunk.Reset(); return; }
            }
            // Each of the title's pixels from the host pixel at its centre,
            // as the CPU's conversion took one host pixel for each before.
            const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, float(readWidth), float(readHeight), 0.0f, 1.0f };
            DrawQuad(command.destinationResource, desc.Width, desc.Height, 0.0f, 0.0f, float(copiedWidth), float(copiedHeight),
                     slot->shrunkView.Get(), viewport, g_copyPixelShader.Get(), g_pointSampler.Get());
            g_context->CopySubresourceRegion(slot->staging.Get(), 0, 0, 0, 0, slot->shrunk.Get(), 0, nullptr);
        }
        else g_context->CopySubresourceRegion(slot->staging.Get(), 0, 0, 0, 0, texture.Get(), 0, nullptr);
        slot->pending = true;
        slot->order = ++g_depthWritebackOrder;
        slot->hostWidth = shrink ? readWidth : copiedWidth;
        slot->hostHeight = shrink ? readHeight : copiedHeight;
        slot->guestBase = command.guestBase;
        slot->guestPitch = command.guestPitch;
        slot->guestWidth = command.guestWidth;
        slot->guestHeight = command.guestHeight;
        slot->guestEndian = command.guestEndian;
        slot->guestFloatDepth = command.guestFloatDepth;
    }

    void ExecuteResolve(const RenderState::ResolveCommand& command)
    {
        // COD3_GPU_DRAWS: a resolve timed in parts, so the report says which
        // resolves the time goes to. A stamp first, with nothing issued since
        // the last one: what it measures is the GPU idle before the resolve,
        // not the resolve. Then the copy, by kind and host size, and then
        // the clears. (Keys: the top four bits D, E and F, never a draw's.)
        const bool timed = RenderStats::DrawsTimed();
        if (timed) RenderStats::DrawTimed(0xDull << 60);
        if (command.bindingsLost) ForgetBindings();
        if (command.copy)
        {
            // The source may be bound as a texture from the draws before,
            // and the destination too: both go.
            UnbindResource(command.destinationResource);
            const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, float(command.width), float(command.height), 0.0f, 1.0f };
            ID3D11PixelShader* copy = g_copyPixelShader.Get();
            if (command.sourceSamples > 1)
            {
                const int slot = command.sourceSamples >= 8 ? 3 : command.sourceSamples >= 4 ? 2 : 1;
                copy = command.fromDepth ? g_copyDepthMS[slot].Get() : g_copyColourMS[slot].Get();
            }
            if (copy != nullptr)
                DrawQuad(command.source, command.sourceWidth, command.sourceHeight, command.x0, command.y0, float(command.width), float(command.height),
                         command.destination, viewport, copy, g_pointSampler.Get());
            if (command.writeBack) WriteDepthBack(command);
            if (timed)
                RenderStats::DrawTimed((0xEull << 60) | (command.fromDepth ? (1ull << 32) : 0ull) |
                                       (uint64_t(command.width & 0xFFFF) << 16) | uint64_t(command.height & 0xFFFF));
        }
        if (command.colorClear)
        {
            if (g_context1)
            {
                const D3D11_RECT rect{ command.clearRect[0], command.clearRect[1], command.clearRect[2], command.clearRect[3] };
                g_context1->ClearView(command.clearTarget, command.clearColor, &rect, 1);
            }
            else g_context->ClearRenderTargetView(command.clearTarget, command.clearColor);
        }
        if (command.depthClear)
            g_context->ClearDepthStencilView(command.clearDepth, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, command.clearDepthValue, command.clearStencilValue);
        if (timed && (command.colorClear || command.depthClear))
        {
            const uint32_t width = command.colorClear ? uint32_t(command.clearRect[2] - command.clearRect[0]) : 0u;
            const uint32_t height = command.colorClear ? uint32_t(command.clearRect[3] - command.clearRect[1]) : 0u;
            RenderStats::DrawTimed((0xFull << 60) | (command.colorClear ? (1ull << 33) : 0ull) | (command.depthClear ? (1ull << 34) : 0ull) |
                                   (uint64_t(width & 0xFFFF) << 16) | uint64_t(height & 0xFFFF));
        }
    }

    // --- HDR -------------------------------------------------------------------------------------------
    //
    // The HDR mode (F11's HDR tab), on RenoDX (thirdparty/renodx, MIT,
    // Carlos Lopez Jr. and contributors) and the Prism tone mapper
    // (thirdparty/prism, MIT, Musa Haji and OopyDoopy / KickFister / Jon):
    //
    //   - the swap chain scRGB: 16 bit float, linear, one being 80 nits
    //   - the title's 8 bit colour targets 16 bit float (render_resources.cpp),
    //     its pixel programs' output kept from nought up (xenos_hlsl.cpp)
    //   - right before the bloom's composite the frame is tone mapped where
    //     it lies (HdrScenePass), and the composite then adds the bloom over
    //     the tone mapped picture, past white where it adds up so (the bloom
    //     program), before the HUD is drawn over it
    //   - the present is RenoDX's swap chain pass, and Dear ImGui draws into a
    //     layer of its own, laid over at the UI's brightness
    //
    // The programs are hdr/cod3_hdr.hlsl on RenoDX's library, built into the
    // executable (hdr_shaders.h), compiled the first time each is needed on
    // a thread of their own; until one is ready the pass waits a frame.

    std::atomic<bool> g_hdrWanted{ false };       // the setting, at the frame's edge
    std::atomic<bool> g_hdrSwapChain{ false };     // the swap chain is scRGB now
    DXGI_FORMAT g_swapChainFormat = DXGI_FORMAT_UNKNOWN;
    int g_hdrToneMapper = 3;
    float g_hdrValues[20] = {};                    // hdr/cod3_hdr.hlsl's HdrSettings
    std::atomic<float> g_displayPeak{ 0.0f };      // what Windows says of the display, for the menu
    std::atomic<bool> g_displayHdr{ false };
    std::atomic<int> g_hdrCompiling{ 0 };

    // This frame's scene passes (one a view: a split screen has two), and
    // whether the frame being shown had one.
    std::vector<D3D11_RECT> g_hdrGraded;
    bool g_hdrPresentGraded = false;

    ComPtr<ID3D11Texture2D> g_hdrSceneCopy;
    ComPtr<ID3D11ShaderResourceView> g_hdrSceneCopyView;
    D3D11_TEXTURE2D_DESC g_hdrSceneCopyDesc{};

    ComPtr<ID3D11Texture2D> g_uiTexture;           // Dear ImGui's layer
    ComPtr<ID3D11RenderTargetView> g_uiView;
    ComPtr<ID3D11ShaderResourceView> g_uiResource;
    int g_uiWidth = 0, g_uiHeight = 0;
    ComPtr<ID3D11BlendState> g_premultipliedBlend;

    ID3D11ShaderResourceView* GammaTable();         // the present's, below

    // The embedded files by path, any case: RenoDX's own includes name
    // tonemap/dice.hlsl for DICE.hlsl, which only Windows' file system
    // forgives.
    std::string HdrNormalise(const std::string& path)
    {
        std::vector<std::string> parts;
        std::string part;
        auto flush = [&]() {
            if (part.empty() || part == ".") { part.clear(); return; }
            if (part == "..") { if (!parts.empty()) parts.pop_back(); part.clear(); return; }
            parts.push_back(part);
            part.clear();
        };
        for (char c : path)
        {
            if (c == '/' || c == '\\') flush();
            else part += char(tolower(static_cast<unsigned char>(c)));
        }
        flush();
        std::string out;
        for (const std::string& p : parts) { if (!out.empty()) out += '/'; out += p; }
        return out;
    }

    const HdrShaders::File* HdrFile(const std::string& normalised)
    {
        static const std::map<std::string, const HdrShaders::File*> files = []() {
            std::map<std::string, const HdrShaders::File*> made;
            for (size_t i = 0; i < HdrShaders::FileCount; i++) made[HdrNormalise(HdrShaders::Files[i].name)] = &HdrShaders::Files[i];
            return made;
        }();
        const auto found = files.find(normalised);
        return found != files.end() ? found->second : nullptr;
    }

    // An #include by the file that names it: relative to that file's
    // folder, then from the top. The data handed out is the embedded text,
    // which lives as long as the program, so nothing is freed.
    class HdrInclude : public ID3DInclude
    {
    public:
        HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR name, LPCVOID parent, LPCVOID* data, UINT* bytes) override
        {
            std::string folder;
            if (const auto known = m_folders.find(parent); known != m_folders.end()) folder = known->second;
            std::string path = HdrNormalise(folder + name);
            const char* text = nullptr;
            size_t size = 0;
            if (path == "cod3_fxaa.hlsl") { text = FxaaSource; size = strlen(FxaaSource); }
            else
            {
                const HdrShaders::File* file = HdrFile(path);
                if (file == nullptr) { path = HdrNormalise(name); file = HdrFile(path); }
                if (file != nullptr) { text = reinterpret_cast<const char*>(file->data); size = file->size; }
            }
            if (text == nullptr)
            {
                printf("render: the HDR programs include %s, which is not among them\n", name);
                return E_FAIL;
            }
            const size_t slash = path.rfind('/');
            m_folders[text] = slash == std::string::npos ? std::string() : path.substr(0, slash + 1);
            *data = text;
            *bytes = UINT(size);
            return S_OK;
        }
        HRESULT STDMETHODCALLTYPE Close(LPCVOID) override { return S_OK; }

    private:
        std::map<const void*, std::string> m_folders;
    };

    struct HdrProgram
    {
        std::atomic<int> state{ 0 };   // 0 compiling, 1 ready, 2 failed
        ComPtr<ID3D11PixelShader> shader;
    };
    std::mutex g_hdrProgramsMutex;
    std::map<std::string, std::shared_ptr<HdrProgram>> g_hdrPrograms;

    void CompileHdrProgram(std::shared_ptr<HdrProgram> program, std::string pass, int toneMapper, uint32_t samples, bool fxaa)
    {
        const HdrShaders::File* main = HdrFile("cod3_hdr.hlsl");
        if (main == nullptr || g_compile == nullptr) { program->state.store(2, std::memory_order_release); return; }
        char toneMapperText[8], samplesText[8];
        snprintf(toneMapperText, sizeof(toneMapperText), "%d", toneMapper);
        snprintf(samplesText, sizeof(samplesText), "%u", samples);
        const char* passMacro = pass == "scene" ? "HDR_SCENE" : pass == "bloom" ? "HDR_BLOOM" : pass == "present" ? "HDR_PRESENT" : "HDR_UI";
        const D3D_SHADER_MACRO defines[] = {
            { passMacro, "1" }, { "HDR_TONEMAPPER", toneMapperText }, { "HDR_SAMPLES", samplesText },
            { "HDR_FXAA", fxaa ? "1" : "0" }, { nullptr, nullptr } };
        HdrInclude include;
        ComPtr<ID3DBlob> code, errors;
        const auto started = std::chrono::steady_clock::now();
        const HRESULT hr = g_compile(main->data, main->size, "cod3_hdr.hlsl", defines, &include, pass.c_str(), "ps_5_0",
                                     D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        if (FAILED(hr) || !code || FAILED(g_device->CreatePixelShader(code->GetBufferPointer(), code->GetBufferSize(), nullptr, &program->shader)))
        {
            printf("render: the HDR %s program (tone mapper %d, %u samples) did not compile: %s\n", pass.c_str(), toneMapper, samples,
                errors ? static_cast<const char*>(errors->GetBufferPointer()) : "no message");
            fflush(stdout);
            program->state.store(2, std::memory_order_release);
            return;
        }
        printf("render: the HDR %s program (tone mapper %d, %u sample%s%s) compiled in %.0f ms\n", pass.c_str(), toneMapper, samples,
            samples == 1 ? "" : "s", fxaa ? ", FXAA" : "", ms);
        fflush(stdout);
        program->state.store(1, std::memory_order_release);
    }

    // The program for a pass, or nothing while it is being compiled.
    ID3D11PixelShader* HdrProgramFor(const char* pass, int toneMapper, uint32_t samples, bool fxaa)
    {
        char key[64];
        snprintf(key, sizeof(key), "%s %d %u %d", pass, toneMapper, samples, fxaa ? 1 : 0);
        std::shared_ptr<HdrProgram> program;
        bool start = false;
        {
            std::lock_guard<std::mutex> lock(g_hdrProgramsMutex);
            std::shared_ptr<HdrProgram>& slot = g_hdrPrograms[key];
            if (!slot) { slot = std::make_shared<HdrProgram>(); start = true; }
            program = slot;
        }
        if (start)
        {
            g_hdrCompiling.fetch_add(1);
            std::thread([program, pass = std::string(pass), toneMapper, samples, fxaa]() {
                CompileHdrProgram(program, pass, toneMapper, samples, fxaa);
                g_hdrCompiling.fetch_sub(1);
            }).detach();
        }
        return program->state.load(std::memory_order_acquire) == 1 ? program->shader.Get() : nullptr;
    }

    // The settings in hdr/cod3_hdr.hlsl's units (RenoDX's: the sliders'
    // 50 is one). The peak left at nought is the display's own.
    void HdrSettingsFrom(const Settings::Values& s)
    {
        const float reported = g_displayPeak.load(std::memory_order_relaxed);
        float peak = s.hdrPeakNits > 0.0f ? s.hdrPeakNits : reported > 0.0f ? reported : 1000.0f;
        const float game = std::max(s.hdrGameNits, 48.0f), ui = std::max(s.hdrUiNits, 48.0f);
        peak = std::max(peak, game);
        float* v = g_hdrValues;
        v[0] = peak; v[1] = game; v[2] = ui; v[3] = float(std::clamp(s.hdrGammaCorrection, 0, 2));
        v[4] = s.hdrExposure;
        v[5] = s.hdrHighlights * 0.02f; v[6] = s.hdrShadows * 0.02f; v[7] = s.hdrContrast * 0.02f; v[8] = s.hdrSaturation * 0.02f;
        v[9] = s.hdrHighlightSaturation * 0.02f; v[10] = s.hdrBlowout * 0.01f; v[11] = s.hdrFlare * 0.02f;
        v[12] = s.hdrHueCorrection * 0.01f; v[13] = 0.5f + s.hdrConeResponse * 0.01f;
        v[14] = std::clamp(s.hdrTvRamp, 0.0f, 100.0f) * 0.01f; v[15] = s.hdrFilmUpgrade ? 1.0f : 0.0f;
        g_hdrToneMapper = std::clamp(s.hdrToneMapper, 0, 7);
        g_screenshotWhite.store(g_hdrSwapChain ? game / 80.0f : 0.0f, std::memory_order_relaxed);
    }

    // The settings onto the GPU, with what this pass is about.
    void HdrUpload(bool graded, bool film)
    {
        if (!g_hdrConstants)
        {
            D3D11_BUFFER_DESC desc{};
            desc.ByteWidth = sizeof(g_hdrValues);
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            if (FAILED(g_device->CreateBuffer(&desc, nullptr, &g_hdrConstants))) return;
        }
        g_hdrValues[16] = graded ? 1.0f : 0.0f;
        g_hdrValues[17] = film ? 1.0f : 0.0f;
        g_context->UpdateSubresource(g_hdrConstants.Get(), 0, nullptr, g_hdrValues, 0, 0);
    }

    // What Windows says of the display the window is on: HDR on or not, and
    // its peak. Every few seconds: the window may have moved.
    void QueryDisplay()
    {
        static uint32_t presents = 0;
        if ((presents++ % 240) != 0 || !g_swapChain) return;
        ComPtr<IDXGIOutput> output;
        ComPtr<IDXGIOutput6> output6;
        DXGI_OUTPUT_DESC1 desc{};
        if (FAILED(g_swapChain->GetContainingOutput(&output)) || FAILED(output.As(&output6)) || FAILED(output6->GetDesc1(&desc))) return;
        const bool hdr = desc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020;
        if (hdr != g_displayHdr.load() || desc.MaxLuminance != g_displayPeak.load())
        {
            printf("render: the display is %s, its peak %.0f nits\n", hdr ? "in HDR" : "in SDR (HDR is off in Windows' display settings)", desc.MaxLuminance);
            fflush(stdout);
        }
        g_displayHdr.store(hdr);
        g_displayPeak.store(desc.MaxLuminance);
    }

    // The scene pass: the frame's target, as the bloom left it, tone mapped
    // over the composite's rectangle, the programs' input a copy of it.
    // With MSAA the copy is the target resolved, and each pixel's samples
    // all get its tone mapped colour: the edges keep their blend, the HUD
    // drawn next still has every sample to cover, and the copy is one
    // sample's size rather than four or eight. Once a view a frame.
    bool HdrScenePass(const RenderState::DrawCommand& command)
    {
        ID3D11Texture2D* target = command.colorTexture;
        if (!g_hdrWanted.load(std::memory_order_relaxed) || target == nullptr || command.targets[0] == nullptr) return false;
        D3D11_TEXTURE2D_DESC desc{};
        target->GetDesc(&desc);
        if (desc.Format != DXGI_FORMAT_R16G16B16A16_FLOAT) return false;
        const D3D11_RECT rect{ command.scissor[0], command.scissor[1],
                               std::min<LONG>(command.scissor[2], LONG(desc.Width)), std::min<LONG>(command.scissor[3], LONG(desc.Height)) };
        if (rect.right <= rect.left || rect.bottom <= rect.top) return false;
        for (const D3D11_RECT& done : g_hdrGraded)
            if (rect.left < done.right && done.left < rect.right && rect.top < done.bottom && done.top < rect.bottom) return false;
        const uint32_t samples = desc.SampleDesc.Count;
        ID3D11PixelShader* program = HdrProgramFor("scene", g_hdrToneMapper, 1, false);
        if (program == nullptr || !BuiltInPrograms()) return false;

        if (!g_hdrSceneCopy || g_hdrSceneCopyDesc.Width != desc.Width || g_hdrSceneCopyDesc.Height != desc.Height)
        {
            g_hdrSceneCopy.Reset();
            g_hdrSceneCopyView.Reset();
            D3D11_TEXTURE2D_DESC copy = desc;
            copy.SampleDesc.Count = 1;
            copy.SampleDesc.Quality = 0;
            copy.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            copy.MiscFlags = 0;
            copy.CPUAccessFlags = 0;
            copy.Usage = D3D11_USAGE_DEFAULT;
            if (FAILED(g_device->CreateTexture2D(&copy, nullptr, &g_hdrSceneCopy)) ||
                FAILED(g_device->CreateShaderResourceView(g_hdrSceneCopy.Get(), nullptr, &g_hdrSceneCopyView)))
            {
                g_hdrSceneCopy.Reset();
                g_hdrSceneCopyView.Reset();
                return false;
            }
            g_hdrSceneCopyDesc = desc;
        }
        // The copy: the rectangle alone, or a multisampled target resolved.
        if (samples > 1) g_context->ResolveSubresource(g_hdrSceneCopy.Get(), 0, target, 0, DXGI_FORMAT_R16G16B16A16_FLOAT);
        else
        {
            const D3D11_BOX box{ UINT(rect.left), UINT(rect.top), 0, UINT(rect.right), UINT(rect.bottom), 1 };
            g_context->CopySubresourceRegion(g_hdrSceneCopy.Get(), 0, UINT(rect.left), UINT(rect.top), 0, target, 0, &box);
        }

        HdrUpload(false, false);
        ID3D11ShaderResourceView* none[32] = {};
        g_context->PSSetShaderResources(0, 32, none);
        g_context->VSSetShaderResources(0, 32, none);
        ID3D11RenderTargetView* targets[1] = { command.targets[0] };
        g_context->OMSetRenderTargets(1, targets, nullptr);
        const D3D11_VIEWPORT viewport{ 0.0f, 0.0f, float(desc.Width), float(desc.Height), 0.0f, 1.0f };
        g_context->RSSetViewports(1, &viewport);
        g_context->RSSetScissorRects(1, &rect);
        { const float noOffset[4] = {}; g_context->RSSetState(RenderPipeline::RasterizerObject(RenderPipeline::Rasterizer(0, true, true, noOffset))); }
        g_context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        g_context->OMSetDepthStencilState(nullptr, 0);
        g_context->VSSetShader(g_quadVertexShader.Get(), nullptr, 0);
        g_context->GSSetShader(nullptr, nullptr, 0);
        g_context->PSSetShader(program, nullptr, 0);
        ID3D11Buffer* quad[1] = { g_quadConstants.Get() };
        g_context->VSSetConstantBuffers(0, 1, quad);
        ID3D11Buffer* hdr[1] = { g_hdrConstants.Get() };
        g_context->PSSetConstantBuffers(1, 1, hdr);
        ID3D11ShaderResourceView* sources[2] = { g_hdrSceneCopyView.Get(), GammaTable() };
        g_context->PSSetShaderResources(0, 2, sources);
        ID3D11SamplerState* samplers[1] = { g_linearSampler.Get() };
        g_context->PSSetSamplers(0, 1, samplers);
        g_context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        g_context->IASetInputLayout(nullptr);
        g_context->Draw(3, 0);
        g_context->PSSetShaderResources(0, 2, none);
        // Everything the draws had bound is gone; the next draw binds its own.
        ForgetBindings();
        g_hdrGraded.push_back(rect);
        return true;
    }

    // Dear ImGui's layer, the swap chain's size.
    bool UiLayerReady(int width, int height)
    {
        if (g_uiTexture && g_uiWidth == width && g_uiHeight == height) return true;
        g_uiTexture.Reset(); g_uiView.Reset(); g_uiResource.Reset();
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = UINT(width);
        desc.Height = UINT(height);
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &g_uiTexture)) ||
            FAILED(g_device->CreateRenderTargetView(g_uiTexture.Get(), nullptr, &g_uiView)) ||
            FAILED(g_device->CreateShaderResourceView(g_uiTexture.Get(), nullptr, &g_uiResource)))
        {
            g_uiTexture.Reset(); g_uiView.Reset(); g_uiResource.Reset();
            return false;
        }
        g_uiWidth = width;
        g_uiHeight = height;
        if (!g_premultipliedBlend)
        {
            D3D11_BLEND_DESC blend{};
            blend.RenderTarget[0].BlendEnable = TRUE;
            blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
            blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
            blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
            blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
            blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
            blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
            blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
            g_device->CreateBlendState(&blend, &g_premultipliedBlend);
        }
        return true;
    }

    // --- the present ------------------------------------------------------------------------------------

    // The colour space the swap chain is shown in: scRGB for the HDR mode
    // (16 bit float, linear, BT.709 primaries, one at 80 nits), sRGB else.
    void SetSwapChainColourSpace(bool hdr)
    {
        ComPtr<IDXGISwapChain3> chain3;
        if (FAILED(g_swapChain.As(&chain3))) return;
        const DXGI_COLOR_SPACE_TYPE space = hdr ? DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709 : DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;
        UINT support = 0;
        if (SUCCEEDED(chain3->CheckColorSpaceSupport(space, &support)) && (support & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) != 0)
            chain3->SetColorSpace1(space);
        else if (hdr)
        {
            static bool said = false;
            if (!said) { said = true; printf("render: the swap chain cannot be shown as scRGB here; the HDR mode's picture goes out as it is\n"); fflush(stdout); }
        }
    }

    bool SwapChainReady()
    {
        const HWND hwnd = g_window.load();
        const int width = g_clientWidth.load(), height = g_clientHeight.load();
        if (hwnd == nullptr || width <= 0 || height <= 0) return false;
        // The HDR mode's swap chain is 16 bit float (scRGB); changing it is a
        // resize to the other format.
        const bool hdr = g_hdrWanted.load(std::memory_order_relaxed);
        const DXGI_FORMAT format = hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_B8G8R8A8_UNORM;
        if (!g_swapChain || g_swapChainWindow != hwnd)
        {
            g_backView.Reset();
            g_backTexture.Reset();
            g_swapChain.Reset();
            ComPtr<IDXGIDevice> dxgiDevice;
            ComPtr<IDXGIAdapter> adapter;
            ComPtr<IDXGIFactory2> factory;
            if (FAILED(g_device.As(&dxgiDevice)) || FAILED(dxgiDevice->GetAdapter(&adapter)) ||
                FAILED(adapter->GetParent(IID_PPV_ARGS(&factory)))) return false;
            DXGI_SWAP_CHAIN_DESC1 desc{};
            desc.Width = width;
            desc.Height = height;
            desc.Format = format;
            desc.SampleDesc.Count = 1;
            desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            desc.BufferCount = 3;
            desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            desc.Flags = g_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
            if (FAILED(factory->CreateSwapChainForHwnd(g_device.Get(), hwnd, &desc, nullptr, nullptr, &g_swapChain))) return false;
            // Alt+Enter is the window's own, not DXGI's exclusive mode.
            factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);
            ComPtr<IDXGISwapChain2> chain2;
            if (SUCCEEDED(g_swapChain.As(&chain2))) chain2->SetMaximumFrameLatency(2);
            g_swapChainWindow = hwnd;
            g_swapChainWidth = width;
            g_swapChainHeight = height;
            g_swapChainFormat = format;
            SetSwapChainColourSpace(hdr);
            g_hdrSwapChain = hdr;
        }
        else if (g_swapChainWidth != width || g_swapChainHeight != height || g_swapChainFormat != format)
        {
            g_backView.Reset();
            g_backTexture.Reset();
            g_context->OMSetRenderTargets(0, nullptr, nullptr);
            ForgetBindings();
            const HRESULT hr = g_swapChain->ResizeBuffers(0, width, height, format, g_tearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0);
            g_swapChainWidth = width;
            g_swapChainHeight = height;
            if (SUCCEEDED(hr) && g_swapChainFormat != format)
            {
                g_swapChainFormat = format;
                SetSwapChainColourSpace(hdr);
                g_hdrSwapChain = hdr;
                printf("render: the swap chain is %s\n", hdr ? "scRGB, 16 bit float (HDR)" : "8 bit sRGB");
                fflush(stdout);
            }
        }
        if (!g_backView)
        {
            if (FAILED(g_swapChain->GetBuffer(0, IID_PPV_ARGS(&g_backTexture)))) return false;
            g_device->CreateRenderTargetView(g_backTexture.Get(), nullptr, &g_backView);
        }
        return g_backView != nullptr;
    }

    // The display's colour table as a texture for the output programs: the
    // identity until the title writes one, then the title's, again each
    // time it changes. COD3_NOGAMMA=1 keeps the identity, to compare.
    //
    // While a film plays the identity is used, unless "Films as video" is
    // off. The title's table is the console's ramp for a television: the
    // picture taken as sRGB and re-encoded with BT.709's camera curve
    // (32 of 255 out as 66 of 1023, the dark end at a third), which the
    // game's own drawing was made to be seen through. The films are video
    // already, encoded for the screen as they are; put through it again
    // their dark scenes sank into black. A video player shows them as
    // decoded, and so does this.
    ID3D11ShaderResourceView* GammaTable()
    {
        if (g_filmVideoLevels.load(std::memory_order_relaxed) && Kernel::FilmPlaying())
        {
            if (!g_identityGammaTexture)
            {
                D3D11_TEXTURE1D_DESC desc{};
                desc.Width = 256;
                desc.MipLevels = 1;
                desc.ArraySize = 1;
                desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
                desc.Usage = D3D11_USAGE_IMMUTABLE;
                desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                uint32_t identity[256];
                for (uint32_t i = 0; i < 256; i++) { const uint32_t v = (i << 2) | (i >> 6); identity[i] = v | (v << 10) | (v << 20) | (3u << 30); }
                const D3D11_SUBRESOURCE_DATA initial{ identity, 0, 0 };
                if (SUCCEEDED(g_device->CreateTexture1D(&desc, &initial, &g_identityGammaTexture)))
                    g_device->CreateShaderResourceView(g_identityGammaTexture.Get(), nullptr, &g_identityGammaView);
            }
            if (g_identityGammaView) return g_identityGammaView.Get();
        }
        if (!g_gammaTexture)
        {
            D3D11_TEXTURE1D_DESC desc{};
            desc.Width = 256;
            desc.MipLevels = 1;
            desc.ArraySize = 1;
            desc.Format = DXGI_FORMAT_R10G10B10A2_UNORM;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            uint32_t identity[256];
            for (uint32_t i = 0; i < 256; i++) { const uint32_t v = (i << 2) | (i >> 6); identity[i] = v | (v << 10) | (v << 20) | (3u << 30); }
            const D3D11_SUBRESOURCE_DATA initial{ identity, 0, 0 };
            if (FAILED(g_device->CreateTexture1D(&desc, &initial, &g_gammaTexture))) return nullptr;
            g_device->CreateShaderResourceView(g_gammaTexture.Get(), nullptr, &g_gammaView);
        }
        static const bool off = getenv("COD3_NOGAMMA") != nullptr;
        uint32_t table[256];
        uint64_t version = 0;
        // 0.5.249: the Brightness setting (F11, 50..150 %) bends the table:
        // each entry raised to the power 100 / brightness, so black stays
        // black, white white, and the middle comes up or down (as the
        // campaign's own brightness slider does with the title's table).
        static int brightnessWas = 100;
        const int brightness = std::clamp(Settings::Get().brightness, 50, 150);
        bool have = !off && Gpu::DisplayGamma(table, version);
        if (!have && (brightness != 100 || brightness != brightnessWas))
        {
            for (uint32_t i = 0; i < 256; i++) { const uint32_t v = (i << 2) | (i >> 6); table[i] = v | (v << 10) | (v << 20); }
            have = true;
        }
        if (have && (version != g_gammaVersion || brightness != brightnessWas))
        {
            g_gammaVersion = version;
            if (brightness != brightnessWas) { printf("render: brightness %d%%\n", brightness); fflush(stdout); }
            brightnessWas = brightness;
            if (brightness != 100)
            {
                const double power = 100.0 / brightness;
                for (uint32_t i = 0; i < 256; i++)
                {
                    uint32_t e = 0;
                    for (int c = 0; c < 3; c++)
                    {
                        const double x = double((table[i] >> (10 * c)) & 0x3FF) / 1023.0;
                        e |= uint32_t(std::lround(std::pow(x, power) * 1023.0)) << (10 * c);
                    }
                    table[i] = e;
                }
            }
            // The console's entries have blue in the low bits and red in the
            // high; the host's format the other way round.
            uint32_t texels[256];
            for (uint32_t i = 0; i < 256; i++)
            {
                const uint32_t e = table[i];
                texels[i] = ((e >> 20) & 0x3FF) | (((e >> 10) & 0x3FF) << 10) | ((e & 0x3FF) << 20) | (3u << 30);
            }
            g_context->UpdateSubresource(g_gammaTexture.Get(), 0, nullptr, texels, 0, 0);
        }
        return g_gammaView.Get();
    }

    // The last swapped frame onto the window, the overlay over it, and the
    // present with or without vertical sync.
    void PresentLocked(const Settings::Values& settings)
    {
        Timed timed(RenderStats::Frame().presentNanoseconds);
        if (!SwapChainReady() || !BuiltInPrograms()) return;
        const int clientWidth = g_swapChainWidth, clientHeight = g_swapChainHeight;
        const float black[4] = { 0, 0, 0, 1 };
        g_context->ClearRenderTargetView(g_backView.Get(), black);
        const bool hdr = g_hdrSwapChain;
        QueryDisplay();

        const RenderResources::Resolved* frame = RenderResources::ResolvedAt(g_frontBuffer);
        const bool fxaa = (AntiAliasing(settings) == 1 || AntiAliasing(settings) == 5) && g_fxaaPixelShader;
        if (frame != nullptr && frame->resource != nullptr)
        {
            // Sixteen by nine, as the console sends it, centred.
            const float aspect = 16.0f / 9.0f;
            float width = float(clientWidth), height = width / aspect;
            if (height > clientHeight) { height = float(clientHeight); width = height * aspect; }
            const D3D11_VIEWPORT viewport{ std::floor((clientWidth - width) * 0.5f), std::floor((clientHeight - height) * 0.5f), std::floor(width), std::floor(height), 0.0f, 1.0f };
            // The anti aliasing reads the frame itself, in its own pixels,
            // and writes into the picture's rectangle: the frame is the
            // chosen resolution, so its rows are the window's, and it has
            // no black either side to bleed a dark rim into the edges.
            if (!hdr)
                DrawQuad(frame->resource, frame->width, frame->height, 0, 0, float(frame->width), float(frame->height),
                         g_backView.Get(), viewport, fxaa ? g_fxaaPixelShader.Get() : g_presentPixelShader.Get(), g_linearSampler.Get(), GammaTable());
            // The HDR mode: RenoDX's swap chain pass (hdr/cod3_hdr.hlsl),
            // the frame already tone mapped when its scene pass ran.
            else if (ID3D11PixelShader* program = HdrProgramFor("present", 0, 1, fxaa))
            {
                HdrUpload(g_hdrPresentGraded, Kernel::FilmPlaying());
                DrawQuad(frame->resource, frame->width, frame->height, 0, 0, float(frame->width), float(frame->height),
                         g_backView.Get(), viewport, program, g_linearSampler.Get(), GammaTable());
            }
        }
        // The menu, the counters and the screenshot key, over the picture.
        Overlay::Initialize(g_swapChainWindow, g_device.Get(), g_context.Get());
        if (!hdr) Overlay::Render(g_backView.Get(), g_backTexture.Get(), clientWidth, clientHeight);
        else
        {
            // The screenshot of the picture alone, made 8 bit (the game's
            // white at the top); then Dear ImGui into its layer, laid over
            // at the UI's brightness.
            if (Overlay::TakeScreenshotRequest()) Overlay::SaveScreenshot(g_backTexture.Get(), clientWidth, clientHeight, g_hdrValues[1] / 80.0f);
            if (UiLayerReady(clientWidth, clientHeight))
            {
                const float clear[4] = { 0, 0, 0, 0 };
                g_context->ClearRenderTargetView(g_uiView.Get(), clear);
                if (Overlay::Render(g_uiView.Get(), nullptr, clientWidth, clientHeight))
                    if (ID3D11PixelShader* program = HdrProgramFor("ui", 0, 1, false))
                    {
                        HdrUpload(false, false);
                        const D3D11_VIEWPORT whole{ 0.0f, 0.0f, float(clientWidth), float(clientHeight), 0.0f, 1.0f };
                        DrawQuad(g_uiResource.Get(), UINT(clientWidth), UINT(clientHeight), 0, 0, float(clientWidth), float(clientHeight),
                                 g_backView.Get(), whole, program, g_pointSampler.Get(), nullptr, g_premultipliedBlend.Get());
                    }
            }
        }
        ID3D11RenderTargetView* noTargets[1] = { nullptr };
        g_context->OMSetRenderTargets(1, noTargets, nullptr);
        ForgetBindings();
        BloodPixelsFinal();
        DumpFrame(g_backTexture.Get());

        RenderStats::EndGpuFrame();
        // COD3_VSYNC=0|1 over the settings.
        static const int vsyncOverride = []() { const char* t = getenv("COD3_VSYNC"); return t ? (t[0] == '0' ? 0 : 1) : -1; }();
        const bool vsync = vsyncOverride >= 0 ? vsyncOverride != 0 : settings.vsync;
        // The frame rate cap (F11, "Frame rate cap"): each frame shown no
        // sooner than its turn on an even clock, so the pace stays the same
        // whatever the scene costs below the cap. The wait is here, at the
        // swap, which the title's next frame waits for anyway. A sleep for
        // most of it and a spin for the last bit: Windows' sleep is late by
        // up to a millisecond even at its finest.
        if (settings.frameCap > 0)
        {
            using clock = std::chrono::steady_clock;
            static clock::time_point due = clock::now();
            const auto period = std::chrono::nanoseconds(1000000000ll / settings.frameCap);
            auto now = clock::now();
            const auto waitStarted = now;
            if (now - due > period * 2) due = now;   // behind by more than a frame: start the clock again
            // A high resolution waitable timer for all but the last 0.3 ms,
            // then a spin: Sleep(1) came back up to two milliseconds late,
            // which at a cap of 144 or more is a third of a frame of jitter.
            static const HANDLE timer = []() {
                HANDLE made = CreateWaitableTimerExW(nullptr, nullptr, 0x00000002 /* CREATE_WAITABLE_TIMER_HIGH_RESOLUTION */, TIMER_ALL_ACCESS);
                return made;
            }();
            if (timer != nullptr && due - now > std::chrono::microseconds(600))
            {
                LARGE_INTEGER wait;
                wait.QuadPart = -int64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(due - now - std::chrono::microseconds(300)).count() / 100);
                if (SetWaitableTimerEx(timer, &wait, 0, nullptr, nullptr, nullptr, 0)) WaitForSingleObject(timer, INFINITE);
                now = clock::now();
            }
            while (now < due)
            {
                if (timer == nullptr && due - now > std::chrono::microseconds(1500)) Sleep(1);
                else YieldProcessor();
                now = clock::now();
            }
            due += period;
            // The wait is idle time, not work: left out of the "cpu" the
            // stats report, which otherwise read 14 ms at a 60 cap however
            // little the frame cost. (Timed adds the whole present after.)
            RenderStats::Frame().presentNanoseconds -= uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(now - waitStarted).count());
        }
        const HRESULT hr = g_swapChain->Present(vsync ? 1 : 0, (!vsync && g_tearing) ? DXGI_PRESENT_ALLOW_TEARING : 0);
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            static bool announced = false;
            if (!announced) { announced = true; printf("render: the device was lost (0x%08lX); nothing more can be drawn\n", hr); }
            g_failed = true;
        }
        g_lastPresent.store(uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()));
        if (getenv("COD3_D3DDEBUG") != nullptr) DrainDebugMessages();
    }

    // The anti aliasing asked for: 0 none, 1 FXAA, 2 MSAA 2x, 3 MSAA 4x,
    // 4 MSAA 8x, 5 MSAA 4x with FXAA over it. COD3_AA=0|fxaa|msaa2|msaa4|
    // msaa8|msaa4fxaa over the settings.
    int AntiAliasing(const Settings::Values& settings)
    {
        static const int override = []() {
            const char* t = getenv("COD3_AA");
            if (t == nullptr) return -1;
            if (strcmp(t, "fxaa") == 0) return 1;
            if (strcmp(t, "msaa2") == 0) return 2;
            if (strcmp(t, "msaa4") == 0) return 3;
            if (strcmp(t, "msaa8") == 0) return 4;
            if (strcmp(t, "msaa4fxaa") == 0) return 5;
            return 0;
        }();
        return override >= 0 ? override : settings.antialiasing;
    }

    // What the settings ask of the renderer, applied at the frame's edge.
    void ApplySettings(const Settings::Values& settings)
    {
        g_filmVideoLevels.store(settings.filmVideoLevels, std::memory_order_relaxed);
        // The HDR mode: the swap chain at the next present, the targets at
        // the next frame's start, the scene pass and the bloom from the
        // next draw; and the programs it will want, started now.
        g_hdrWanted.store(settings.hdr, std::memory_order_relaxed);
        RenderResources::RequestHdrTargets(settings.hdr);
        // The HUD's white, measured with the RenoDX devkit in a level: about
        // 0.86 of the intermediate's one (143 nits of 203 at gamma 2.2).
        RenderCommands::SetHdr(settings.hdr, std::clamp(settings.hdrBloom, 0.0f, 100.0f) * 0.02f, settings.hdrHudMatch ? 1.0f / 0.86f : 0.0f);
        HdrSettingsFrom(settings);
        if (settings.hdr)
        {
            const int aa = AntiAliasing(settings);
            HdrProgramFor("present", 0, 1, aa == 1 || aa == 5);
            HdrProgramFor("ui", 0, 1, false);
            HdrProgramFor("scene", g_hdrToneMapper, 1, false);
            HdrProgramFor("bloom", 0, 1, false);
        }
        // The optimizations' switches (tuning.h), for the next frame.
        Tuning::depthThread.store(settings.depthThread, std::memory_order_relaxed);
        Tuning::fastConstants.store(settings.fastConstants, std::memory_order_relaxed);
        Tuning::shaderLoadCache.store(settings.shaderLoadCache, std::memory_order_relaxed);
        Tuning::strongChangeCheck.store(settings.strongChangeCheck, std::memory_order_relaxed);
        Tuning::rangeLooks.store(settings.rangeLooks, std::memory_order_relaxed);
        Tuning::opaqueShaders.store(settings.opaqueShaders, std::memory_order_relaxed);
        Tuning::stagingUploads.store(settings.stagingUploads, std::memory_order_relaxed);
        Tuning::texturePool.store(settings.texturePool, std::memory_order_relaxed);
        const int antialiasing = AntiAliasing(settings);
        RenderResources::RequestMultisample(antialiasing == 2 ? 2 : antialiasing == 3 || antialiasing == 5 ? 4 : antialiasing == 4 ? 8 : 1);
        // The render scale: the chosen resolution's height, times the
        // resolution scale in per cent, over the title's 624 rows (the
        // desktop's height by default). COD3_SCALE=N pins it, fractions
        // allowed; COD3_RESSCALE=percent pins the per cent alone.
        static const float pinned = []() { const char* t = getenv("COD3_SCALE"); return t ? float(atof(t)) : 0.0f; }();
        static const int percentPinned = []() { const char* t = getenv("COD3_RESSCALE"); return t ? int(strtol(t, nullptr, 10)) : 0; }();
        float scale;
        if (pinned > 0.0f) scale = pinned;
        else
        {
            Settings::Values sized = settings;
            if (percentPinned > 0) sized.resolutionScale = percentPinned;
            int width, height;
            Settings::DrawnSize(sized, width, height);
            scale = float(height) / 624.0f;
            if (scale < 0.25f) scale = 0.25f;
        }
        RenderResources::RequestScale(scale);
        // The blur while aiming: COD3_AIMBLUR=0|1 over the settings.
        static const int aimBlurOverride = []() { const char* t = getenv("COD3_AIMBLUR"); return t ? (t[0] == '0' ? 0 : 1) : -1; }();
        RenderCommands::SetAimBlur(aimBlurOverride >= 0 ? aimBlurOverride != 0 : settings.aimBlur);
        RenderCommands::SetShellShockBlur(settings.shellShockBlur);
        RenderCommands::SetRendering(settings.sharpShadows, settings.fastShadowMaps, settings.foliageCoverage);
        RenderCommands::SetSteadyDecals(settings.steadyDecals);
        RenderResources::RequestEffectsScale(settings.effectsResolution >= 2 ? 2.0 : settings.effectsResolution == 1 ? 1.5 : 1.0);
        // The texture quality: COD3_TEXQUALITY=0|1|2 over the settings.
        static const int qualityOverride = []() { const char* t = getenv("COD3_TEXQUALITY"); return t ? int(strtol(t, nullptr, 10)) : -1; }();
        const int quality = qualityOverride >= 0 ? qualityOverride : settings.textureQuality;
        RenderResources::RequestMipSkip(quality >= 2 ? 0 : quality == 1 ? 1 : 2);

        // The texture filtering, unless the environment said.
        static const bool filterPinned = getenv("COD3_TEXTURE_FILTER") != nullptr || getenv("COD3_ANISO") != nullptr;
        if (!filterPinned)
        {
            const RenderPipeline::Filter filter = settings.textureFilter == 0 ? RenderPipeline::Filter::Native
                : settings.textureFilter == 1 ? RenderPipeline::Filter::Bilinear
                : settings.textureFilter == 2 ? RenderPipeline::Filter::Trilinear : RenderPipeline::Filter::Anisotropic;
            if (filter != RenderPipeline::Filtering() || uint32_t(settings.anisotropy) != RenderPipeline::Anisotropy())
            {
                RenderPipeline::SetFiltering(filter, uint32_t(settings.anisotropy));
                // The samplers were dropped: nothing bound is valid.
                memset(g_bound.samplers, 0, sizeof(g_bound.samplers));
            }
        }
    }
}

// --- freed guest memory ---------------------------------------------------------------------------

namespace
{
    // Ranges the title freed, waiting for the command thread.
    std::mutex g_freedMutex;
    std::vector<std::pair<uint32_t, uint32_t>> g_freed;
    std::atomic<bool> g_freedPending{ false };

    void ForgetFreed()
    {
        if (!g_freedPending.load(std::memory_order_acquire)) return;
        std::vector<std::pair<uint32_t, uint32_t>> freed;
        {
            std::lock_guard<std::mutex> lock(g_freedMutex);
            freed.swap(g_freed);
            g_freedPending.store(false, std::memory_order_release);
        }
        bool dropped = false;
        for (const auto& range : freed) dropped |= RenderResources::Forget(range.first, range.second);
        if (dropped) ForgetBindings();
    }
}

// --- the drawing thread (0.5.391, render_queue.h) ---------------------------------------------
//
// One queue, one writer (the command thread, holding g_mutex) and one
// reader (the drawing thread). Records are 16-byte aligned in a ring of
// QueueBytes: the writer's cursor runs ahead and is handed over (published)
// at the end of each draw; the reader runs what is published and says how
// far it got (consumed). Sync waits until consumed reaches the writer's
// cursor: the drawing thread is then idle and the context is the caller's.

namespace
{
    // 0.5.393: 4 MB, not 64: the drawing thread is a draw or two behind,
    // and a queue that stays in the processor's cache costs the command
    // thread far less to write than one that runs through 64 MB.
    constexpr uint64_t QueueBytes = 4ull << 20;
    // Handed over every PublishDraws draws (or sooner at a Sync), not every
    // draw: each hand over moves cache lines between the two cores.
    constexpr uint32_t PublishDraws = 8;
    uint32_t g_unpublishedDraws = 0;
    uint8_t* g_queue = nullptr;
    bool g_queueOn = false;
    bool g_queueActive = false;   // this frame's draws go to the drawing thread (set at each swap)
    uint64_t g_queueWritten = 0;                     // the writer's cursor (under g_mutex)
    std::atomic<uint64_t> g_queuePublished{ 0 };     // what the reader may run
    std::atomic<uint64_t> g_queueConsumed{ 0 };      // what it has run
    std::atomic<bool> g_drawerSleeping{ false };
    DWORD g_drawerThreadId = 0;
    thread_local bool t_deferring = false;
    uint64_t g_queuedDraws = 0, g_queueSyncs = 0, g_queueSyncWaits = 0;
    // The once a second "queue:" line (render profile runs): where the
    // command thread waited for the drawing thread, and how busy that was.
    constexpr int SyncSites = 16;
    uint64_t g_qSyncBySite[SyncSites] = {}, g_qSyncCycles = 0, g_qSyncLag = 0;
    std::atomic<uint64_t> g_qExecuted{ 0 }, g_qBusyCycles{ 0 }, g_qDrawCycles{ 0 }, g_qWriteCycles{ 0 };

    enum : uint32_t { RecordWrap = 1, RecordDraw, RecordRing, RecordUpload };
    struct RecordHeader { uint32_t type, bytes, pad0, pad1; };
    struct RingRecord { ID3D11Buffer* buffer; uint32_t offset, size, discard, pad; };
    struct UploadRecord { ID3D11Resource* resource; uint32_t subresource, hasBox, size, pad; D3D11_BOX box; uint32_t pad2; };
    struct DrawRecord { uint32_t textureCount, bufferCount, hasBools, pad; };
    static_assert(sizeof(RecordHeader) == 16, "record header");
    static_assert(std::is_trivially_copyable<RenderState::DrawCommand>::value, "the draw is copied as bytes");

    constexpr size_t DrawTexturesAt = offsetof(RenderState::DrawCommand, textures);
    constexpr size_t DrawTextureCountAt = offsetof(RenderState::DrawCommand, textureCount);
    constexpr size_t DrawBuffersAt = offsetof(RenderState::DrawCommand, buffers);
    constexpr size_t DrawBufferCountAt = offsetof(RenderState::DrawCommand, bufferCount);
    constexpr uint32_t BoolBytes = 256;   // the booleans' constant buffer, as UpdateSubresource reads it

    void QueuePublish()
    {
        g_queuePublished.store(g_queueWritten, std::memory_order_seq_cst);
        if (g_drawerSleeping.load(std::memory_order_seq_cst)) WakeByAddressSingle(&g_queuePublished);
    }

    void QueueWaitConsumed(uint64_t until)
    {
        uint32_t spins = 0;
        while (g_queueConsumed.load(std::memory_order_acquire) < until)
        {
            if (++spins < 20000) _mm_pause();
            else SwitchToThread();
        }
    }

    // Room for a record of `payload` bytes after its header: where its
    // payload goes. Everything given before it is complete, so it is
    // published first if the writer has to wait for room.
    uint8_t* QueueReserve(uint32_t type, uint32_t payload)
    {
        const uint32_t bytes = uint32_t((sizeof(RecordHeader) + payload + 15) & ~size_t(15));
        uint64_t at = g_queueWritten % QueueBytes;
        const uint64_t wrap = at + bytes > QueueBytes ? QueueBytes - at : 0;
        if (g_queueWritten + wrap + bytes - g_queueConsumed.load(std::memory_order_acquire) > QueueBytes)
        {
            QueuePublish();
            QueueWaitConsumed(g_queueWritten + wrap + bytes - QueueBytes);
        }
        if (wrap != 0)
        {
            RecordHeader* skip = reinterpret_cast<RecordHeader*>(g_queue + at);
            skip->type = RecordWrap;
            skip->bytes = uint32_t(wrap);
            g_queueWritten += wrap;
            at = 0;
        }
        RecordHeader* header = reinterpret_cast<RecordHeader*>(g_queue + at);
        header->type = type;
        header->bytes = bytes;
        g_queueWritten += bytes;
        return g_queue + at + sizeof(RecordHeader);
    }

    void QueueDraw(const RenderState::DrawCommand& command)
    {
        using Command = RenderState::DrawCommand;
        const size_t textures = size_t(command.textureCount) * sizeof(Command::Texture);
        const size_t buffers = size_t(command.bufferCount) * sizeof(Command::Buffer);
        const bool bools = command.boolsChanged && command.bools != nullptr;
        const size_t payload = sizeof(DrawRecord) + DrawTexturesAt + textures + (DrawBuffersAt - DrawTextureCountAt) + buffers +
                               (sizeof(Command) - DrawBufferCountAt) + (bools ? BoolBytes : 0);
        uint8_t* to = QueueReserve(RecordDraw, uint32_t(payload));
        DrawRecord* record = reinterpret_cast<DrawRecord*>(to);
        record->textureCount = command.textureCount;
        record->bufferCount = command.bufferCount;
        record->hasBools = bools ? 1 : 0;
        to += sizeof(DrawRecord);
        const uint8_t* from = reinterpret_cast<const uint8_t*>(&command);
        memcpy(to, from, DrawTexturesAt); to += DrawTexturesAt;
        memcpy(to, command.textures, textures); to += textures;
        memcpy(to, from + DrawTextureCountAt, DrawBuffersAt - DrawTextureCountAt); to += DrawBuffersAt - DrawTextureCountAt;
        memcpy(to, command.buffers, buffers); to += buffers;
        memcpy(to, from + DrawBufferCountAt, sizeof(Command) - DrawBufferCountAt); to += sizeof(Command) - DrawBufferCountAt;
        if (bools) memcpy(to, command.bools, BoolBytes);
        g_queuedDraws++;
    }

    // The drawing thread's copy of the draw it is running.
    RenderState::DrawCommand g_drawerCommand;

    void RunRecord(const RecordHeader* header)
    {
        const uint8_t* body = reinterpret_cast<const uint8_t*>(header + 1);
        switch (header->type)
        {
        case RecordRing:
        {
            const uint64_t started = __rdtsc();
            const RingRecord* ring = reinterpret_cast<const RingRecord*>(body);
            D3D11_MAPPED_SUBRESOURCE mapped{};
            if (SUCCEEDED(g_context->Map(ring->buffer, 0, ring->discard ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped)))
            {
                memcpy(static_cast<uint8_t*>(mapped.pData) + ring->offset, ring + 1, ring->size);
                g_context->Unmap(ring->buffer, 0);
            }
            g_qWriteCycles.fetch_add(__rdtsc() - started, std::memory_order_relaxed);
            break;
        }
        case RecordUpload:
        {
            const UploadRecord* upload = reinterpret_cast<const UploadRecord*>(body);
            g_context->UpdateSubresource(upload->resource, upload->subresource, upload->hasBox ? &upload->box : nullptr, upload + 1, 0, 0);
            break;
        }
        case RecordDraw:
        {
            using Command = RenderState::DrawCommand;
            const DrawRecord* record = reinterpret_cast<const DrawRecord*>(body);
            const uint8_t* from = body + sizeof(DrawRecord);
            uint8_t* to = reinterpret_cast<uint8_t*>(&g_drawerCommand);
            const size_t textures = size_t(record->textureCount) * sizeof(Command::Texture);
            const size_t buffers = size_t(record->bufferCount) * sizeof(Command::Buffer);
            memcpy(to, from, DrawTexturesAt); from += DrawTexturesAt;
            memcpy(g_drawerCommand.textures, from, textures); from += textures;
            memcpy(to + DrawTextureCountAt, from, DrawBuffersAt - DrawTextureCountAt); from += DrawBuffersAt - DrawTextureCountAt;
            memcpy(g_drawerCommand.buffers, from, buffers); from += buffers;
            memcpy(to + DrawBufferCountAt, from, sizeof(Command) - DrawBufferCountAt); from += sizeof(Command) - DrawBufferCountAt;
            g_drawerCommand.bools = record->hasBools ? reinterpret_cast<const uint32_t*>(from) : nullptr;
            g_drawerCommand.boolsChanged = record->hasBools != 0;
            const uint64_t started = __rdtsc();
            ExecuteDraw(g_drawerCommand);
            RenderStats::Leave();
            g_qDrawCycles.fetch_add(__rdtsc() - started, std::memory_order_relaxed);
            g_qExecuted.fetch_add(1, std::memory_order_relaxed);
            break;
        }
        default: break;
        }
    }

    void DrawerMain()
    {
        SetThreadDescription(GetCurrentThread(), L"CoD3 drawing");
        uint64_t read = 0;
        for (;;)
        {
            uint64_t end = g_queuePublished.load(std::memory_order_acquire);
            if (end == read)
            {
                // A short spin (the next draw is usually microseconds away),
                // then asleep until the writer publishes.
                // 0.5.394: about a third of a millisecond by the time stamp
                // counter, whatever a pause costs on this processor: waking
                // it was a system call for the command thread.
                const uint64_t spinFrom = __rdtsc();
                while (end == read && __rdtsc() - spinFrom < 1000000)
                {
                    _mm_pause();
                    end = g_queuePublished.load(std::memory_order_acquire);
                }
                if (end == read)
                {
                    g_drawerSleeping.store(true, std::memory_order_seq_cst);
                    end = g_queuePublished.load(std::memory_order_seq_cst);
                    if (end == read) WaitOnAddress(&g_queuePublished, &read, sizeof(read), 2);
                    g_drawerSleeping.store(false, std::memory_order_seq_cst);
                    continue;
                }
            }
            const uint64_t busyFrom = __rdtsc();
            while (read < end)
            {
                const RecordHeader* header = reinterpret_cast<const RecordHeader*>(g_queue + read % QueueBytes);
                RunRecord(header);
                read += header->bytes;
            }
            g_qBusyCycles.fetch_add(__rdtsc() - busyFrom, std::memory_order_relaxed);
            g_queueConsumed.store(read, std::memory_order_release);
        }
    }

    void StartDrawingThread()
    {
        const char* setting = getenv("COD3_DRAWTHREAD");
        if (setting != nullptr && setting[0] == '0') { printf("render: the drawing thread is off (COD3_DRAWTHREAD=0)\n"); return; }
        g_queue = static_cast<uint8_t*>(VirtualAlloc(nullptr, SIZE_T(QueueBytes), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        if (g_queue == nullptr) return;
        DWORD id = 0;
        HANDLE thread = CreateThread(nullptr, 0, [](LPVOID) -> DWORD { DrawerMain(); return 0; }, nullptr, 0, &id);
        if (thread == nullptr) { VirtualFree(g_queue, 0, MEM_RELEASE); g_queue = nullptr; return; }
        SetThreadPriority(thread, THREAD_PRIORITY_ABOVE_NORMAL);
        g_drawerThreadId = id;
        RenderStats::SetDrawThread(thread);
        g_queueOn = true;
        printf("render: the drawing thread is on (Direct3D calls on their own thread; COD3_DRAWTHREAD=0 turns it off)\n");
    }

    // A draw the drawing thread can run: not one the logs, the checks or the
    // HDR mode's scene pass look at from this thread.
    bool Queueable(const RenderState::DrawCommand& command)
    {
        return command.ring && !command.logged && !command.layerQuery && !command.hdrScene &&
               !RenderStats::DrawsTimed() && !FrameLogged() && g_picturesSwap.load(std::memory_order_relaxed) < 0;
    }
}

bool RenderQueue::Deferring() { return t_deferring; }

void RenderQueue::Sync(int site)
{
    if (!g_queueOn || GetCurrentThreadId() == g_drawerThreadId) return;
    if (g_queueConsumed.load(std::memory_order_acquire) == g_queueWritten) return;
    g_queueSyncs++;
    g_qSyncBySite[unsigned(site) < SyncSites ? site : 0]++;
    g_qSyncLag += g_queuedDraws - g_qExecuted.load(std::memory_order_relaxed);
    const uint64_t started = __rdtsc();
    if (g_queuePublished.load(std::memory_order_relaxed) != g_queueWritten) QueuePublish();
    if (g_queueConsumed.load(std::memory_order_acquire) != g_queueWritten) g_queueSyncWaits++;
    QueueWaitConsumed(g_queueWritten);
    g_qSyncCycles += __rdtsc() - started;
}

namespace
{
    // Once a second, in render profile runs.
    void QueueReport()
    {
        if (!g_queueOn || !RenderStats::Profiled()) return;
        static auto lastTime = std::chrono::steady_clock::now();
        static uint64_t lastTsc = __rdtsc();
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - lastTime).count();
        if (seconds < 1.0) return;
        const uint64_t tsc = __rdtsc();
        const double perMs = double(tsc - lastTsc) / (seconds * 1000.0);
        lastTime = now; lastTsc = tsc;
        static uint64_t lastSyncs = 0;
        const uint64_t syncs = g_queueSyncs - lastSyncs;
        lastSyncs = g_queueSyncs;
        printf("queue: %.0f syncs/s, command thread waited %.1f ms/s (avg %.0f draws behind); drawing thread busy %.1f ms/s (draws %.1f, ring writes %.1f); by site:",
            syncs / seconds, g_qSyncCycles / perMs / seconds, syncs ? double(g_qSyncLag) / syncs : 0.0,
            g_qBusyCycles.exchange(0) / perMs / seconds, g_qDrawCycles.exchange(0) / perMs / seconds, g_qWriteCycles.exchange(0) / perMs / seconds);
        for (int i = 0; i < SyncSites; i++) if (g_qSyncBySite[i]) printf(" %d:%llu", i, (unsigned long long)g_qSyncBySite[i]);
        printf("\n");
        memset(g_qSyncBySite, 0, sizeof(g_qSyncBySite));
        g_qSyncCycles = 0; g_qSyncLag = 0;
    }
}

uint8_t* RenderQueue::RingWrite(ID3D11Buffer* buffer, bool discard, uint32_t offset, uint32_t bytes)
{
    RingRecord* record = reinterpret_cast<RingRecord*>(QueueReserve(RecordRing, uint32_t(sizeof(RingRecord) + bytes)));
    record->buffer = buffer;
    record->offset = offset;
    record->size = bytes;
    record->discard = discard ? 1 : 0;
    return reinterpret_cast<uint8_t*>(record + 1);
}

void RenderQueue::Upload(ID3D11Resource* resource, unsigned subresource, const D3D11_BOX* box, const void* data, uint32_t bytes)
{
    if (box == nullptr && bytes == 0)
    {
        D3D11_RESOURCE_DIMENSION dimension{};
        resource->GetType(&dimension);
        if (dimension == D3D11_RESOURCE_DIMENSION_BUFFER)
        {
            D3D11_BUFFER_DESC desc{};
            static_cast<ID3D11Buffer*>(resource)->GetDesc(&desc);
            bytes = desc.ByteWidth;
        }
    }
    // At once: not deferring, a size not worked out, or too big for the queue.
    if (!t_deferring || bytes == 0 || bytes > RenderQueue::MaxRingWrite)
    {
        Sync(15);
        g_context->UpdateSubresource(resource, subresource, box, data, 0, 0);
        return;
    }
    UploadRecord* record = reinterpret_cast<UploadRecord*>(QueueReserve(RecordUpload, uint32_t(sizeof(UploadRecord) + bytes)));
    record->resource = resource;
    record->subresource = subresource;
    record->hasBox = box != nullptr ? 1 : 0;
    if (box != nullptr) record->box = *box;
    record->size = bytes;
    memcpy(record + 1, data, bytes);
}

void Render::MemoryFreed(uint32_t address, uint32_t size)
{
    if (size == 0) return;
    std::lock_guard<std::mutex> lock(g_freedMutex);
    g_freed.emplace_back(Guest::GpuOffset(address), size);
    g_freedPending.store(true, std::memory_order_release);
}

bool Render::Enabled()
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    return Start();
}

void Render::Draw(uint32_t initiator, uint32_t indexBase, uint32_t indexWord)
{
    (void)indexWord;
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    if (!Start()) return;
    Timed timed(RenderStats::Frame().drawNanoseconds);
    ForgetFreed();
    RenderState::DrawCommand command;
    // 0.5.391: worked out here, run on the drawing thread when it is on.
    const bool queue = g_queueOn && g_queueActive;
    t_deferring = queue;
    const bool prepared = RenderCommands::PrepareDraw(initiator, indexBase, command);
    t_deferring = false;
    if (prepared)
    {
        command.blendObject = RenderPipeline::BlendObject(command.blend);
        command.depthObject = RenderPipeline::DepthObject(command.depth);
        command.rasterizerObject = RenderPipeline::RasterizerObject(command.rasterizer);
        if (queue && Queueable(command)) QueueDraw(command);
        else
        {
            RenderQueue::Sync(14);
            ExecuteDraw(command);
        }
    }
    if (g_queueOn && g_queuePublished.load(std::memory_order_relaxed) != g_queueWritten && ++g_unpublishedDraws >= PublishDraws)
    {
        g_unpublishedDraws = 0;
        QueuePublish();
    }
    RenderStats::Leave();
}

void Render::Resolve()
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    if (!Start()) return;
    Timed timed(RenderStats::Frame().resolveNanoseconds);
    RenderQueue::Sync(11);
    ForgetFreed();
    RenderState::ResolveCommand command;
    if (RenderCommands::PrepareResolve(command)) ExecuteResolve(command);
}

void Render::ShaderLoaded(bool pixel, uint32_t guestAddress, uint32_t sizeDwords, uint32_t start)
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    if (!Start()) return;
    RenderShaders::Loaded(pixel, Guest::Base + guestAddress, sizeDwords);
    if (FrameLogged()) printf("frame: load %s %016llx, %u dwords from %08X at %u\n", pixel ? "ps" : "vs",
        (unsigned long long)RenderShaders::HashOf(RenderShaders::Current(pixel)), sizeDwords, guestAddress, start);
}

void Render::UseProgram(bool pixel, uint32_t guestAddress, uint32_t sizeDwords, uint64_t hash)
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    if (!Start()) return;
    RenderShaders::Loaded(pixel, Guest::Base + guestAddress, sizeDwords, hash);
}

uint64_t Render::CurrentProgramHash(bool pixel)
{
    return RenderShaders::HashOf(RenderShaders::Current(pixel));
}

void Render::LogNextFrame()
{
    // 0.5.312: only with COD3_FRAME_LOGS=1. The screenshot key logged the
    // next frames in full and pictured every draw (a 35 MB -draws.bin each
    // time): Justin's game lagged badly at every screenshot.
    static const bool wanted = []() { const char* t = getenv("COD3_FRAME_LOGS"); return t && t[0] == '1'; }();
    if (!wanted) return;
    // A press while frames are being logged makes the run longer: starting
    // it again would cut the frame being logged short. (The first call also
    // reads the environment's choice, so it does not undo this.)
    const int64_t swap = int64_t(g_swaps.load(std::memory_order_relaxed));
    if (!::FrameLogged()) g_frameWanted.store(swap + 1, std::memory_order_relaxed);
    g_frameUntil.store(swap + 3, std::memory_order_relaxed);
    // And the next frame's draws pictured one by one, unless a press before
    // this one is still at it.
    if (g_picturesSwap.load(std::memory_order_relaxed) < 0)
    {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        const std::filesystem::path folder = std::filesystem::path(exe).parent_path() / "screenshots";
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        wchar_t name[64];
        wcsftime(name, 64, L"CoD3-%Y%m%d-%H%M%S-draws.bin", &local);
        { std::lock_guard<std::mutex> lock(g_picturesMutex); g_picturesPath = (folder / name).wstring(); }
        g_picturesSwap.store(swap + 1, std::memory_order_relaxed);
    }
    printf("render: the screenshot key: frames %lld to %lld go to the log in full\n",
        (long long)g_frameWanted.load(std::memory_order_relaxed), (long long)swap + 2);
    fflush(stdout);
}

void Render::Swap(uint32_t frontBufferPhysical, uint32_t width, uint32_t height)
{
    (void)width; (void)height;
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    if (!Start()) return;
    RenderQueue::Sync(12);
    CalibrateTsc();
    QueueReport();
    // COD3_DRAWTHREAD_TOGGLE=seconds: the drawing thread allowed and not in
    // turns, logged as the benchmark's settings A and B.
    // 0.5.395: and used only in frames heavy enough for the command thread
    // to be what holds the frame (COD3_DRAWTHREAD_MIN draws, 4000; on from
    // there, off again under three quarters of it; 0 = every frame). In
    // lighter frames the game's own threads are what hold it, and the
    // drawing thread's core is better left to them.
    {
        static bool wanted = true;
        static const int toggle = []() { const char* t = getenv("COD3_DRAWTHREAD_TOGGLE"); return t ? atoi(t) : 0; }();
        static auto since = std::chrono::steady_clock::now();
        if (g_queueOn && toggle > 0 && std::chrono::steady_clock::now() - since >= std::chrono::seconds(toggle))
        {
            since = std::chrono::steady_clock::now();
            wanted = !wanted;
            printf("bench: setting %s (drawthread %s)\n", wanted ? "A" : "B", wanted ? "on when heavy" : "off");
        }
        static const uint32_t heavy = []() { const char* t = getenv("COD3_DRAWTHREAD_MIN"); return t ? uint32_t(atoi(t)) : 4000u; }();
        static bool busy = false;
        const uint32_t draws = uint32_t(RenderStats::Frame().draws);
        if (draws >= heavy) busy = true;
        else if (draws < heavy - heavy / 4) busy = false;
        g_queueActive = wanted && busy;
    }
    Overlay::NoteFrame();
    DebugProbe::Poll();
    PollDepthWritebacks();
    RenderCommands::CheckLateWrites();
    PollLayerQueries();
    g_frontBuffer = Guest::GpuOffset(frontBufferPhysical);
    if (FrameLogged()) printf("frame: swap %llu of %08X\n", (unsigned long long)g_swaps.load(std::memory_order_relaxed), frontBufferPhysical);
    // Whether this frame had the HDR mode's scene pass: it is shown so,
    // and again by an idle present until the next swap.
    g_hdrPresentGraded = !g_hdrGraded.empty();
    g_hdrGraded.clear();

    // The frame's end: onto the window, the counters closed, the next
    // frame's settings taken.
    const Settings::Values settings = Settings::Get();
    PresentLocked(settings);
    RenderStats::EndFrame();
    // Verify mode (verify.h): what each check has looked at, every ten
    // seconds while any is on, so the log says it ran even when it found
    // nothing wrong.
    {
        static auto told = std::chrono::steady_clock::now();
        bool any = false;
        for (int k = 0; k < Verify::KindCount; k++) any = any || Verify::On(Verify::Kind(k));
        if (any && std::chrono::steady_clock::now() - told > std::chrono::seconds(10))
        {
            told = std::chrono::steady_clock::now();
            printf("verify:");
            for (int k = 0; k < Verify::KindCount; k++)
                if (Verify::On(Verify::Kind(k)))
                    printf(" %s %llu checked, %llu wrong;", Verify::Name(Verify::Kind(k)),
                        (unsigned long long)Verify::checks[k].load(), (unsigned long long)Verify::mismatches[k].load());
            printf("\n");
            fflush(stdout);
        }
    }
    FrameSwapped();
    g_swaps.fetch_add(1, std::memory_order_relaxed);
    g_frame++;
    ApplySettings(settings);
    if (RenderResources::BeginFrame(g_frame)) ForgetBindings();   // the targets were made again
    RenderStats::BeginFrame();
}

void Render::SetWindow(void* hwnd, int clientWidth, int clientHeight)
{
    g_window.store(static_cast<HWND>(hwnd));
    g_clientWidth.store(clientWidth);
    g_clientHeight.store(clientHeight);
}

void Render::PresentIdle()
{
    // Only when the title has not swapped for a while: a resize, or the
    // menu, while it is loading or paused.
    const uint64_t now = uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
    if (now - g_lastPresent.load() < 250) return;
    std::unique_lock<std::recursive_mutex> lock(g_mutex, std::try_to_lock);
    if (!lock.owns_lock()) return;
    if (!g_started || g_failed) return;
    RenderQueue::Sync(13);
    PresentLocked(Settings::Get());
}

void Render::Report()
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    if (!g_started || g_failed) return;
    const RenderStats::Snapshot s = RenderStats::Current();
    const RenderResources::Statistics r = RenderResources::Stats();
    const RenderPipeline::Statistics p = RenderPipeline::Stats();
    printf("render: %.1f fps, %.2f ms a frame (%.2f ms cpu%s%.2f ms gpu), %.0f draws, %.0f k triangles, %.0f resolves, %.0f skipped a frame; "
           "switches a frame: %.0f programs, %.0f textures, %.0f states, %.0f targets\n",
        s.fps, s.frameMilliseconds, s.cpuMilliseconds, s.gpuProfiled ? ", " : ", no ", s.gpuMilliseconds,
        s.draws, s.triangles / 1000.0f, s.resolves, s.skipped, s.shaderSwitches, s.textureSwitches, s.pipelineSwitches, s.targetSwitches);
    printf("        %u textures, %u vertex buffers, %u colour and %u depth targets, %u resolved surfaces: %.1f MB; %u released; "
           "%llu texture uploads (%.1f MB), %llu buffer uploads (%.1f MB), %.1f MB of indices, %.1f MB of constants streamed\n",
        r.textures, r.buffers, r.colorTargets, r.depthTargets, r.resolved, r.resourceBytes / 1048576.0, r.released,
        (unsigned long long)r.textureUploads, r.textureBytes / 1048576.0, (unsigned long long)r.bufferUploads, r.bufferBytes / 1048576.0,
        r.indexBytes / 1048576.0, r.constantBytes / 1048576.0);
    printf("        %u blend, %u depth, %u rasteriser states, %u samplers; scale %.3f\n", p.blendStates, p.depthStates, p.rasterizerStates, p.samplers, RenderResources::Scale());
    if (g_queueOn)
        printf("        drawing thread: %llu draws queued, %llu syncs (%llu waited)\n",
            (unsigned long long)g_queuedDraws, (unsigned long long)g_queueSyncs, (unsigned long long)g_queueSyncWaits);
    RenderShaders::Report();
    RenderCommands::ReportSkips();
    fflush(stdout);
}

void Render::DisplayHdr(bool& hdr, float& peakNits)
{
    hdr = g_displayHdr.load(std::memory_order_relaxed);
    peakNits = g_displayPeak.load(std::memory_order_relaxed);
}

bool Render::HdrActive() { return g_hdrSwapChain; }

int Render::HdrCompiling() { return g_hdrCompiling.load(std::memory_order_relaxed); }

bool RenderInternal::FrameLogged() { return ::FrameLogged(); }
uint64_t RenderInternal::Swaps() { return g_swaps.load(std::memory_order_relaxed); }
int RenderInternal::DumpNumber() { return g_dumpNumber; }

