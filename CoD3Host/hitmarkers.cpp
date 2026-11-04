#include "hitmarkers.h"
#include "blood.h"
#include "hitmarker_rules.h"
#include "settings.h"
#include "kernel.h"
#include "imgui.h"
#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <atomic>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>
using Microsoft::WRL::ComPtr;
namespace {
std::atomic<uint64_t> hitAt{0}, killAt{0};
ComPtr<ID3D11ShaderResourceView> texture;
std::atomic<const int16_t*> pcm{nullptr};
uint32_t pcmCount = 0;
bool Resource(int id, const void*& data, uint32_t& size) {
    const HMODULE module = GetModuleHandleW(nullptr);
    const HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), MAKEINTRESOURCEW(10));
    if (!resource) return false;
    size = SizeofResource(module, resource);
    data = LockResource(LoadResource(module, resource));
    return data && size;
}
void Publish(bool kill) {
    const uint64_t now = GetTickCount64();
    if (kill) killAt.store(now, std::memory_order_relaxed);
    hitAt.store(now, std::memory_order_release);
}
bool Pointer(uint32_t p) { return p >= 0x40000000u && p < 0xC0000000u - 1024; }
}
void Hitmarkers::Initialize(ID3D11Device* device) {
    // Both assets are embedded. Decode/upload once, before gameplay starts.
    const HRESULT initialized = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    struct Apartment { bool owned; ~Apartment() { if (owned) CoUninitialize(); } } apartment{SUCCEEDED(initialized)};
    const void* data = nullptr; uint32_t bytes = 0;
    if (Resource(202, data, bytes) && !(bytes & 1)) {
        pcmCount = bytes / 2;
        pcm.store(static_cast<const int16_t*>(data), std::memory_order_release);
    }
    ComPtr<IWICImagingFactory> factory;
    ComPtr<IWICStream> stream;
    ComPtr<IWICBitmapDecoder> decoder;
    ComPtr<IWICBitmapFrameDecode> frame;
    ComPtr<IWICFormatConverter> converter;
    if (!Resource(201, data, bytes) ||
        FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) ||
        FAILED(factory->CreateStream(&stream)) ||
        FAILED(stream->InitializeFromMemory((BYTE*)data, bytes)) ||
        FAILED(factory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
        FAILED(decoder->GetFrame(0, &frame)) || FAILED(factory->CreateFormatConverter(&converter)) ||
        FAILED(converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) {
        printf("hitmarkers: could not decode image resource\n"); return;
    }
    UINT width = 0, height = 0; converter->GetSize(&width, &height);
    if (!width || !height || width > 2048 || height > 2048) return;
    std::vector<uint8_t> pixels(width * height * 4);
    if (FAILED(converter->CopyPixels(nullptr, width * 4, UINT(pixels.size()), pixels.data()))) return;
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = width; desc.Height = height; desc.MipLevels = desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_IMMUTABLE; desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA initial{pixels.data(), width * 4, 0};
    ComPtr<ID3D11Texture2D> image;
    if (FAILED(device->CreateTexture2D(&desc, &initial, &image)) ||
        FAILED(device->CreateShaderResourceView(image.Get(), nullptr, &texture))) return;
    printf("hitmarkers: loaded supplied PNG (%ux%u) and sound (%u samples)\n", width, height, pcmCount);
}
bool Hitmarkers::Active(const Settings::Values& v) {
    const uint64_t at = hitAt.load(std::memory_order_acquire);
    return v.hitmarkers && at && GetTickCount64() - at < uint64_t(v.hitmarkerDuration);
}
void Hitmarkers::Draw(int width, int height, const Settings::Values& v) {
    if (!texture || !Active(v)) return;
    const uint64_t at = hitAt.load(std::memory_order_acquire), now = GetTickCount64();
    const uint64_t killed = killAt.load(std::memory_order_relaxed);
    const bool red = v.hitmarkerRedKill && killed && now - killed < uint64_t(v.hitmarkerDuration);
    const int alpha = int(255 * HitmarkerRules::Alpha(now - at, v.hitmarkerDuration, v.hitmarkerOpacity));
    // Asset has transparent padding: size is the full supplied image at 1080p.
    const float size = v.hitmarkerSize * height / 1080.0f;
    const ImVec2 a((width - size) * 0.5f, (height - size) * 0.5f);
    ImGui::GetForegroundDrawList()->AddImage((ImTextureID)texture.Get(), a,
        ImVec2(a.x + size, a.y + size), ImVec2(0, 0), ImVec2(1, 1),
        red ? IM_COL32(255, 45, 45, alpha) : IM_COL32(255, 255, 255, alpha));
}
void Hitmarkers::Mix(float* samples, uint32_t frames, uint32_t channels, const Settings::Values& v) {
    // Called only under Audio's existing mutex. One reusable voice, no allocation.
    static uint64_t seen = 0, playedAt = 0;
    static uint32_t position = 0;
    static bool playing = false;
    const int16_t* sound = pcm.load(std::memory_order_acquire);
    const uint64_t at = hitAt.load(std::memory_order_acquire);
    if (!sound || !v.hitmarkers || !v.hitmarkerSound || !v.hitmarkerVolume) {
        playing = false; seen = at; return;
    }
    if (at != seen) {
        seen = at;
        if (GetTickCount64() - at < 200 && at - playedAt >= 80) {
            position = 0; playing = true; playedAt = at;
        }
    }
    const float gain = v.hitmarkerVolume / 100.0f / 32768.0f;
    for (uint32_t i = 0; playing && i < frames; ++i) {
        const float sample = sound[position++] * gain;
        samples[i * channels] += sample;
        samples[i * channels + 1] += sample;
        if (position >= pcmCount) playing = false;
    }
}
void Hitmarkers::Preview(bool kill) { Publish(kill); }

#ifndef COD3_MP
// Final SP damage application: r3 target, r5 attacker, r8 applied damage.
// Wrap the original; never reproduce its health, script, or death handling.
extern "C" PPC_FUNC(__imp__sub_82562DD8);
PPC_FUNC(sub_82562DD8) {
    Blood::Impact blood;
    Blood::Before(ctx,base,blood);
    const uint32_t target = ctx.r3.u32, attacker = ctx.r5.u32;
    bool eligible = false; int before = 0;
    if (Pointer(target) && Pointer(attacker)) {
        const uint32_t ts = Guest::Read32(base, target + 536), as = Guest::Read32(base, attacker + 536);
        if (Pointer(ts) && Pointer(as)) {
            before = int32_t(Guest::Read32(base, target + 776));
            eligible = HitmarkerRules::Eligible(Guest::Read32(base, attacker + 528) != 0,
                Guest::Read32(base, target + 532) != 0, target == attacker,
                Guest::Read32(base, as + 4), Guest::Read32(base, ts + 4), before, int32_t(ctx.r8.u32));
        }
    }
    __imp__sub_82562DD8(ctx, base);
    Blood::After(base,blood);
    if (!eligible) return;
    const int after = int32_t(Guest::Read32(base, target + 776));
    if (HitmarkerRules::Confirmed(before, after) && Settings::Get().hitmarkers) {
        Publish(after <= 0);
        static const bool trace = getenv("COD3_HITMARKER_TRACE") != nullptr;
        if (trace) { printf("hitmarkers: player hit %08X health %d -> %d%s\n", target, before, after, after <= 0 ? " KILL" : ""); fflush(stdout); }
    }
}
#endif
