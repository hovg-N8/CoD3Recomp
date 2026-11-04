#include "version.h"
#include "overlay.h"
#include "kernel.h"
#include "verify.h"
#include "settings.h"
#include "hitmarkers.h"
#include "input.h"
#include "net.h"
#include "render_stats.h"
#include "render.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <deque>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

#include <Windows.h>
#include <d3d11.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "imgui.h"
#include "backends/imgui_impl_dx11.h"
#include "backends/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

using Microsoft::WRL::ComPtr;

namespace
{
    // The window thread hands messages in and the command thread draws:
    // Dear ImGui is for one thread at a time, so both take this. Recursive,
    // because the render sends the window a message of its own (the input
    // method's window, placed for the console's text field), and the
    // window procedure runs on the rendering thread, inside the lock.
    std::recursive_mutex g_mutex;

    std::atomic<bool> g_open{ false };
    std::atomic<bool> g_console{ false };
    std::atomic<bool> g_screenshotWanted{ false };

    // The console: what was typed and what was said back, under the
    // overlay's lock. The title's own console was stripped from this build
    // (its "toggleconsole" and its Com_Printf are gone), so this is the
    // host's: the commands go to the title's command buffer, which still
    // runs them, and the answers are only what this side can say.
    std::deque<std::string> g_consoleLines;
    std::vector<std::string> g_consoleHistory;
    int g_consoleHistoryAt = -1;
    char g_consoleInput[1024] = {};
    bool g_consoleFocus = false;
    bool g_consoleScroll = false;

    void ConsoleLine(const std::string& line)
    {
        g_consoleLines.push_back(line);
        if (g_consoleLines.size() > 400) g_consoleLines.pop_front();
        g_consoleScroll = true;
    }

    int ConsoleInputCallback(ImGuiInputTextCallbackData* data)
    {
        // Up and down through the history.
        if (data->EventFlag != ImGuiInputTextFlags_CallbackHistory || g_consoleHistory.empty()) return 0;
        const int count = int(g_consoleHistory.size());
        if (data->EventKey == ImGuiKey_UpArrow) g_consoleHistoryAt = g_consoleHistoryAt < 0 ? count - 1 : std::max(0, g_consoleHistoryAt - 1);
        else if (data->EventKey == ImGuiKey_DownArrow) g_consoleHistoryAt = g_consoleHistoryAt < 0 ? -1 : (g_consoleHistoryAt + 1 >= count ? -1 : g_consoleHistoryAt + 1);
        data->DeleteChars(0, data->BufTextLen);
        if (g_consoleHistoryAt >= 0) data->InsertChars(0, g_consoleHistory[size_t(g_consoleHistoryAt)].c_str());
        return 0;
    }

    void RunConsoleLine(const std::string& typed)
    {
        std::string line = typed;
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
        size_t first = 0;
        while (first < line.size() && (line[first] == ' ' || line[first] == '\t')) first++;
        line = line.substr(first);
        if (line.empty()) return;
        ConsoleLine("] " + line);
        if (g_consoleHistory.empty() || g_consoleHistory.back() != line) g_consoleHistory.push_back(line);
        if (g_consoleHistory.size() > 100) g_consoleHistory.erase(g_consoleHistory.begin());
        g_consoleHistoryAt = -1;
        if (line == "help" || line == "?")
        {
            ConsoleLine("Commands go on the game's own command buffer, the way the chapter select runs \"spmap\".");
#ifdef COD3_MP
            ConsoleLine("Examples: devmap mp_merv   (cheats on)   god   noclip   ufo   notarget   give all   where   cg_thirdPerson 1   map_restart");
            ConsoleLine("Maps: mp_merv mp_fuel mp_hosta mpmaynne ederdam forest poissons rouen verdun. Cheats need a map started with devmap.");
#else
            ConsoleLine("Examples: spmap forest   map_restart   cg_fov 80   timescale 0.5   god   noclip   give all   seta com_maxfps 60");
#endif
            ConsoleLine("The game's replies (Com_Printf) were compiled out of this build, so only this side's lines appear here.");
            ConsoleLine("clear: empties the log.   ` or ~ or Esc: closes the console.");
            return;
        }
        if (line == "clear") { g_consoleLines.clear(); return; }
        Kernel::QueueConsoleCommand(line);
    }

    void DrawConsole(int width, int height)
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(float(width), float(height) * 0.45f));
        ImGui::SetNextWindowBgAlpha(0.85f);
        if (!ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::End();
            return;
        }
        const float inputHeight = ImGui::GetFrameHeightWithSpacing() + 4.0f;
        if (ImGui::BeginChild("log", ImVec2(0, -inputHeight), false, ImGuiWindowFlags_HorizontalScrollbar))
        {
            if (g_consoleLines.empty()) ImGui::TextDisabled("Console. Type \"help\" for examples.");
            for (const std::string& line : g_consoleLines) ImGui::TextUnformatted(line.c_str());
            if (g_consoleScroll) { ImGui::SetScrollHereY(1.0f); g_consoleScroll = false; }
        }
        ImGui::EndChild();
        ImGui::Separator();
        ImGui::SetNextItemWidth(-1.0f);
        if (g_consoleFocus) { ImGui::SetKeyboardFocusHere(); g_consoleFocus = false; }
        const ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CallbackHistory;
        if (ImGui::InputText("##input", g_consoleInput, sizeof(g_consoleInput), flags, ConsoleInputCallback))
        {
            RunConsoleLine(g_consoleInput);
            g_consoleInput[0] = 0;
            g_consoleFocus = true;
        }
        ImGui::End();
    }

    std::atomic<bool> g_ready{ false };
    HWND g_hwnd = nullptr;
    ID3D11Device* g_device = nullptr;
    ID3D11DeviceContext* g_context = nullptr;
    int g_capturing = -1;   // the action whose key is being pressed next, or -1
    Settings::Values g_edit;   // the values the menu shows and changes
    bool g_editLoaded = false;
    float g_fontScale = 1.0f;

    // The frame counter: swaps in the last second.
    std::atomic<uint64_t> g_frames{ 0 };
    uint64_t g_framesAtTick = 0;
    double g_fps = 0.0;
    std::chrono::steady_clock::time_point g_tick;

    // A key the player pressed while a binding waited for one.
    void Capture(int key)
    {
        if (g_capturing < 0 || g_capturing >= Settings::ActionCount) return;
        if (key == VK_ESCAPE) { g_capturing = -1; return; }
        g_edit.keys[g_capturing] = key;
        g_capturing = -1;
    }

    // A half float as a float, for the HDR mode's scRGB back buffer.
    float HalfToFloat(uint16_t h)
    {
        const uint32_t sign = uint32_t(h & 0x8000u) << 16, exponent = (h >> 10) & 0x1Fu, mantissa = h & 0x3FFu;
        if (exponent == 0) { const float f = std::ldexp(float(mantissa), -24); return sign ? -f : f; }
        const uint32_t bits = exponent == 31 ? (sign | 0x7F800000u | (mantissa << 13)) : (sign | ((exponent + 112u) << 23) | (mantissa << 13));
        float f;
        memcpy(&f, &bits, 4);
        return f;
    }

    // scRGB light (one is 80 nits) as an sRGB byte, the given white at the top.
    uint8_t ScRgbToByte(uint16_t h, float white)
    {
        float v = HalfToFloat(h) / white;
        if (!(v > 0.0f)) return 0;
        v = std::min(v, 1.0f);
        v = v <= 0.0031308f ? v * 12.92f : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
        return uint8_t(v * 255.0f + 0.5f);
    }

    void SaveScreenshotOf(ID3D11Texture2D* back, int width, int height, float scRgbWhite)
    {
        D3D11_TEXTURE2D_DESC desc{};
        back->GetDesc(&desc);
        const bool half = desc.Format == DXGI_FORMAT_R16G16B16A16_FLOAT;
        if (scRgbWhite <= 0.0f) scRgbWhite = 203.0f / 80.0f;
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        desc.MiscFlags = 0;
        ComPtr<ID3D11Texture2D> staging;
        if (FAILED(g_device->CreateTexture2D(&desc, nullptr, &staging))) return;
        g_context->CopyResource(staging.Get(), back);
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (FAILED(g_context->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped))) return;

        // Beside the executable, named by the moment.
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        std::filesystem::path folder = std::filesystem::path(exe).parent_path() / "screenshots";
        std::error_code ec;
        std::filesystem::create_directories(folder, ec);
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        char name[64];
        strftime(name, sizeof(name), "CoD3-%Y%m%d-%H%M%S.png", &local);
        std::filesystem::path file = folder / name;
        // several in one second (test runs): -2, -3, ... after the first
        for (int n = 2; std::filesystem::exists(file, ec) && n < 100; n++)
        {
            char more[80];
            snprintf(more, sizeof(more), "%.*s-%d.png", int(strlen(name)) - 4, name, n);
            file = folder / more;
        }

        ComPtr<IWICImagingFactory> factory;
        ComPtr<IWICBitmapEncoder> encoder;
        ComPtr<IWICBitmapFrameEncode> frame;
        ComPtr<IWICStream> stream;
        bool ok = false;
        if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
            SUCCEEDED(factory->CreateStream(&stream)) &&
            SUCCEEDED(stream->InitializeFromFilename(file.c_str(), GENERIC_WRITE)) &&
            SUCCEEDED(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) &&
            SUCCEEDED(encoder->Initialize(stream.Get(), WICBitmapEncoderNoCache)) &&
            SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) &&
            SUCCEEDED(frame->Initialize(nullptr)) &&
            SUCCEEDED(frame->SetSize(UINT(width), UINT(height))))
        {
            WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
            if (SUCCEEDED(frame->SetPixelFormat(&format)))
            {
                // The back buffer is B8G8R8A8; its alpha is whatever the
                // present left, so it is made opaque on the way out. The
                // HDR mode's is scRGB: an SDR picture of it, the game's
                // white at the top, highlights past it clipped.
                std::vector<uint8_t> rows(size_t(width) * height * 4);
                for (int y = 0; y < height; y++)
                {
                    const uint8_t* from = static_cast<const uint8_t*>(mapped.pData) + size_t(y) * mapped.RowPitch;
                    uint8_t* to = rows.data() + size_t(y) * width * 4;
                    if (half)
                        for (int x = 0; x < width; x++)
                        {
                            uint16_t c[4];
                            memcpy(c, from + size_t(x) * 8, 8);
                            to[x * 4 + 0] = ScRgbToByte(c[2], scRgbWhite);
                            to[x * 4 + 1] = ScRgbToByte(c[1], scRgbWhite);
                            to[x * 4 + 2] = ScRgbToByte(c[0], scRgbWhite);
                        }
                    else memcpy(to, from, size_t(width) * 4);
                    for (int x = 0; x < width; x++) to[x * 4 + 3] = 255;
                }
                ok = SUCCEEDED(frame->WritePixels(UINT(height), UINT(width) * 4, UINT(rows.size()), rows.data())) &&
                     SUCCEEDED(frame->Commit()) && SUCCEEDED(encoder->Commit());
            }
        }
        g_context->Unmap(staging.Get(), 0);
        printf("screenshot: %s%s\n", file.string().c_str(), ok ? "" : " could not be written");
        fflush(stdout);
    }

    // The HDR tabs' values from one set into another; true when any differed.
    bool CopyHdrSettings(const Settings::Values& from, Settings::Values& to)
    {
        bool differed = false;
        auto copy = [&](auto& into, const auto& value) { if (!(into == value)) { into = value; differed = true; } };
        copy(to.hdr, from.hdr);
        copy(to.hdrToneMapper, from.hdrToneMapper);
        copy(to.hdrPeakNits, from.hdrPeakNits);
        copy(to.hdrGameNits, from.hdrGameNits);
        copy(to.hdrUiNits, from.hdrUiNits);
        copy(to.hdrGammaCorrection, from.hdrGammaCorrection);
        copy(to.hdrExposure, from.hdrExposure);
        copy(to.hdrHighlights, from.hdrHighlights);
        copy(to.hdrShadows, from.hdrShadows);
        copy(to.hdrContrast, from.hdrContrast);
        copy(to.hdrSaturation, from.hdrSaturation);
        copy(to.hdrHighlightSaturation, from.hdrHighlightSaturation);
        copy(to.hdrBlowout, from.hdrBlowout);
        copy(to.hdrFlare, from.hdrFlare);
        copy(to.hdrHueCorrection, from.hdrHueCorrection);
        copy(to.hdrConeResponse, from.hdrConeResponse);
        copy(to.hdrBloom, from.hdrBloom);
        copy(to.hdrTvRamp, from.hdrTvRamp);
        copy(to.hdrFilmUpgrade, from.hdrFilmUpgrade);
        copy(to.hdrHudMatch, from.hdrHudMatch);
        return differed;
    }

    void DrawMenu(int width, int height)
    {
        if (!g_editLoaded) { g_edit = Settings::Get(); g_editLoaded = true; }
        bool changed = false;   // something differs from what is applied
        const Settings::Values applied = Settings::Get();
        ImGui::SetNextWindowSize(ImVec2(620 * g_fontScale, 0), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(width * 0.5f, height * 0.5f), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
        bool open = true;
        if (ImGui::Begin("Settings (F11) - version " COD3_VERSION "###settings", &open, ImGuiWindowFlags_NoCollapse))
        {
            if (ImGui::BeginTabBar("tabs"))
            {
                if (ImGui::BeginTabItem("Graphics"))
                {
                    const char* renderers[] = { "DirectX 11", "DirectX 12 (not yet)", "Vulkan (not yet)" };
                    int renderer = g_edit.renderer;
                    if (ImGui::Combo("Renderer", &renderer, renderers, 3))
                    {
                        // Only the first exists; the others stay names for now.
                        g_edit.renderer = 0;
                    }
                    if (renderer != 0) ImGui::TextDisabled("Only DirectX 11 is available for now.");
                    const char* modes[] = { "Windowed", "Borderless full screen" };
                    ImGui::Combo("Window mode", &g_edit.windowMode, modes, 2);
                    ImGui::TextDisabled("Alt+Enter switches it too.");
                    {
                        // The display's sizes, and "Desktop" for whatever it is now.
                        const std::vector<Settings::Mode>& modes = Settings::DisplayModes();
                        std::vector<std::string> names;
                        names.push_back("Desktop");
                        int current = 0;
                        for (size_t i = 0; i < modes.size(); i++)
                        {
                            char name[32];
                            snprintf(name, sizeof(name), "%dx%d", modes[i].width, modes[i].height);
                            names.push_back(name);
                            if (g_edit.resolutionWidth == modes[i].width && g_edit.resolutionHeight == modes[i].height) current = int(i) + 1;
                        }
                        if (g_edit.resolutionWidth > 0 && current == 0)
                        {
                            char name[32];
                            snprintf(name, sizeof(name), "%dx%d", g_edit.resolutionWidth, g_edit.resolutionHeight);
                            names.push_back(name);
                            current = int(names.size()) - 1;
                        }
                        std::vector<const char*> items;
                        for (const std::string& name : names) items.push_back(name.c_str());
                        if (ImGui::Combo("Resolution", &current, items.data(), int(items.size())))
                        {
                            if (current == 0) { g_edit.resolutionWidth = 0; g_edit.resolutionHeight = 0; }
                            else if (current <= int(modes.size())) { g_edit.resolutionWidth = modes[size_t(current) - 1].width; g_edit.resolutionHeight = modes[size_t(current) - 1].height; }
                        }
                        ImGui::SliderInt("Resolution scale", &g_edit.resolutionScale, 25, 200, "%d%%");
                        int width, height, drawnWidth, drawnHeight;
                        Settings::Resolution(g_edit, width, height);
                        Settings::DrawnSize(g_edit, drawnWidth, drawnHeight);
                        if (g_edit.resolutionScale == 100)
                            ImGui::TextDisabled("The game is drawn at %dx%d; the window is that size, full screen scales it.", width, height);
                        else
                            ImGui::TextDisabled("The game is drawn at %dx%d and scaled to %dx%d; the window is that size, full screen scales it.", drawnWidth, drawnHeight, width, height);
                    }
                    const char* filters[] = { "The game's own", "Bilinear", "Trilinear", "Anisotropic" };
                    ImGui::Combo("Texture filtering", &g_edit.textureFilter, filters, 4);
                    if (g_edit.textureFilter == 3)
                    {
                        const char* levels[] = { "2x", "4x", "8x", "16x" };
                        int level = g_edit.anisotropy >= 16 ? 3 : g_edit.anisotropy >= 8 ? 2 : g_edit.anisotropy >= 4 ? 1 : 0;
                        if (ImGui::Combo("Anisotropy", &level, levels, 4)) g_edit.anisotropy = 2 << level;
                    }
                    const char* aa[] = { "None", "FXAA", "MSAA 2x", "MSAA 4x", "MSAA 8x", "MSAA 4x + FXAA" };
                    ImGui::Combo("Anti-aliasing", &g_edit.antialiasing, aa, 6);
                    const char* qualities[] = { "Low", "Medium", "High" };
                    ImGui::Combo("Texture quality", &g_edit.textureQuality, qualities, 3);
                    ImGui::SliderInt("Field of view", &g_edit.fov, 65, 100, g_edit.fov == 65 ? "65 (the game's own)" : "%d");
                    ImGui::SliderInt("Brightness", &g_edit.brightness, 50, 150, g_edit.brightness == 100 ? "100%% (the game's own)" : "%d%%");
                    const char* details[] = { "The game's own", "Maximum" };
                    ImGui::Combo("Model detail", &g_edit.modelDetail, details, 2);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(Maximum: the finest version of every model at any distance)");
                    ImGui::Checkbox("Fog", &g_edit.fog);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(the maps' haze in the distance)");
                    const char* distances[] = { "The game's own", "Far", "Medium", "Near" };
                    ImGui::Combo("Draw distance", &g_edit.drawDistance, distances, 4);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(nearer draws less: faster on a slow machine)");
                    const char* effects[] = { "All", "Fewer far away", "None" };
                    ImGui::Combo("Effects", &g_edit.effects, effects, 3);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(smoke, dust, fire and sparks)");
                    ImGui::Checkbox("Dynamic lights", &g_edit.dynamicLights);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(muzzle flashes and explosions lighting what is near)");
                    ImGui::Checkbox("Blur while aiming", &g_edit.aimBlur);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(the console's depth of field down the sights: soft, with an outline round the gun at PC resolutions)");
                    ImGui::Checkbox("Console depth precision", &g_edit.consoleDepth);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(no flickering surfaces; costs frame rate with MSAA; after a restart)");
                    ImGui::Checkbox("Sharp shadows", &g_edit.sharpShadows);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(shadow edges filtered at the shadow map's real resolution; off: the console's blocky ones)");
                    const char* effectSizes[] = { "Console (half)", "High (3/4)", "Full" };
                    ImGui::Combo("Smoke and shadow resolution", &g_edit.effectsResolution, effectSizes, 3);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(higher: smoother edges in front of smoke, sharper shadow cascades; Full cost a third of the frame rate at 4K)");
                    ImGui::Checkbox("Smooth foliage edges", &g_edit.foliageCoverage);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(grass, leaves and fences antialiased; needs an MSAA anti-aliasing mode)");
                    ImGui::Checkbox("Fast shadow maps", &g_edit.fastShadowMaps);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(shadow maps drawn on the GPU's fast depth only path; turn off if shadows flicker)");
                    ImGui::Checkbox("Steady decals", &g_edit.steadyDecals);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(bullet holes, blood and scorch marks held a hair in front of their wall: no flicker)");
                    ImGui::Checkbox("Films as video", &g_edit.filmVideoLevels);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(the cutscene films without the console's TV gamma curve: no crushed blacks)");
                    ImGui::Checkbox("Save textures for texture packs", &g_edit.textureDump);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(each texture the game uses saved once to textures\\dump, up to 3000 a run)");
                    ImGui::Checkbox("Use replacement textures", &g_edit.textureReplace);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(textures\\replace\\<name>.png or .dds in the game's place)");
                    ImGui::Checkbox("Native command lists (experimental)", &g_edit.nativeCommands);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(the draws' state straight from the game's own calls: faster; after a restart)");
                    ImGui::Checkbox("Watch memory writes (experimental)", &g_edit.watchWrites);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(still textures and buffers not looked at again until written: faster; after a restart)");
                    // Each of the renderer's speed ups on its own switch, for
                    // finding the one behind something that looks wrong.
                    // They take effect at the next frame (tuning.h).
                    if (ImGui::CollapsingHeader("Optimizations (turn one off if something looks wrong)"))
                    {
                        ImGui::TextDisabled("All on is fastest. A change takes effect at Apply, no restart needed.");
                        ImGui::Checkbox("Depth copy on its own thread", &g_edit.depthThread);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(the depth the game reads back for light glows, prepared off the render thread)");
                        ImGui::Checkbox("Fast constant writes", &g_edit.fastConstants);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(the game's shader constants stored in blocks rather than one at a time)");
                        ImGui::Checkbox("Shader load cache", &g_edit.shaderLoadCache);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(shaders the game loads again recognised without hashing them again)");
                        ImGui::Checkbox("Thorough change detection", &g_edit.strongChangeCheck);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(off: the older check for whether a buffer or texture changed)");
                        ImGui::Checkbox("Recheck effect geometry each draw", &g_edit.rangeLooks);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(with Watch memory writes off: the vertices of effects written mid frame looked at again)");
                        ImGui::Checkbox("Opaque shader variants", &g_edit.opaqueShaders);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(shaders without the alpha test for surfaces that do not use it)");
                        ImGui::Checkbox("Staged texture uploads", &g_edit.stagingUploads);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(new textures sent to the GPU through a copy, fewer hitches)");
                        ImGui::Checkbox("Texture pool", &g_edit.texturePool);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(textures of the same size kept and used again, fewer hitches)");
                        if (ImGui::Button("All on"))
                        {
                            g_edit.depthThread = g_edit.fastConstants = g_edit.shaderLoadCache = g_edit.strongChangeCheck = true;
                            g_edit.rangeLooks = g_edit.opaqueShaders = g_edit.stagingUploads = g_edit.texturePool = true;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("All off"))
                        {
                            g_edit.depthThread = g_edit.fastConstants = g_edit.shaderLoadCache = g_edit.strongChangeCheck = false;
                            g_edit.rangeLooks = g_edit.opaqueShaders = g_edit.stagingUploads = g_edit.texturePool = false;
                        }
                    }
                    // Verify mode (verify.h): each shortcut checked against the
                    // slow, exact answer. Takes effect at once, is never saved.
                    if (ImGui::CollapsingHeader("Verify the shortcuts (diagnostics, slow)"))
                    {
                        ImGui::TextDisabled("Checks each shortcut against the exact answer and logs every disagreement to CoD3.log.");
                        ImGui::TextDisabled("Stale vertex buffers, textures and constants it finds are also sent again, so the picture is corrected.");
                        static const char* const hints[Verify::KindCount] = {
                            "(the vertices each draw reads against what the GPU was sent)",
                            "(each texture's memory against what it was made from, once a frame)",
                            "(constant uploads skipped as unchanged against what they would have been)",
                            "(buffers and textures judged unchanged by their pages or by samples, hashed whole)",
                            "(with Native command lists on: each draw's state against its record)" };
                        for (int k = 0; k < Verify::KindCount; k++)
                        {
                            const Verify::Kind kind = Verify::Kind(k);
                            bool value = Verify::On(kind);
                            char label[64];
                            snprintf(label, sizeof(label), "Verify %s", Verify::Name(kind));
                            if (ImGui::Checkbox(label, &value)) Verify::Set(kind, value);
                            ImGui::SameLine();
                            ImGui::TextDisabled("%s", hints[k]);
                            const uint64_t checked = Verify::checks[k].load(), wrong = Verify::mismatches[k].load();
                            if (checked != 0 || wrong != 0)
                            {
                                ImGui::Indent();
                                if (wrong != 0) ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%llu checked, %llu wrong", (unsigned long long)checked, (unsigned long long)wrong);
                                else ImGui::Text("%llu checked, none wrong", (unsigned long long)checked);
                                const std::string last = Verify::Last(kind);
                                if (!last.empty()) { ImGui::PushTextWrapPos(0.0f); ImGui::TextDisabled("Last: %s", last.c_str()); ImGui::PopTextWrapPos(); }
                                ImGui::Unindent();
                            }
                        }
                        if (ImGui::Button("Verify all")) for (int k = 0; k < Verify::KindCount; k++) Verify::Set(Verify::Kind(k), true);
                        ImGui::SameLine();
                        if (ImGui::Button("Verify none")) for (int k = 0; k < Verify::KindCount; k++) Verify::Set(Verify::Kind(k), false);
                        ImGui::SameLine();
                        if (ImGui::Button("Reset counts")) Verify::Reset();
                    }
                    ImGui::Checkbox("Vertical sync (VSync)", &g_edit.vsync);
                    ImGui::Checkbox("Unlimited frame rate in levels", &g_edit.unlockFrameRate);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(the menus and films keep the console's 60)");
                    {
                        // A cap holds the frames to an even pace: a steady 60
                        // feels smoother than a rate swinging between 50 and 140.
                        static const int caps[] = { 0, 60, 90, 120, 144, 165, 240 };
                        static const char* const names[] = { "Off", "60", "90", "120", "144", "165", "240" };
                        int current = 0;
                        for (int i = 0; i < 7; i++) if (caps[i] == g_edit.frameCap) current = i;
                        ImGui::SetNextItemWidth(120);
                        if (ImGui::Combo("Frame rate cap", &current, names, 7)) g_edit.frameCap = caps[current];
                        ImGui::SameLine();
                        ImGui::TextDisabled("(an even pace; 60 for the steadiest picture)");
                    }
                    ImGui::Checkbox("Show FPS", &g_edit.fpsOverlay);
                    ImGui::Checkbox("Renderer statistics", &g_edit.statsOverlay);
                    ImGui::EndTabItem();
                }
                // The HDR mode (render_d3d11.cpp, hdr/cod3_hdr.hlsl), on
                // RenoDX: the mode and its brightnesses here, RenoDX's
                // sliders on the next tab. Both apply as they are changed,
                // as RenoDX's own settings do.
                if (ImGui::BeginTabItem("HDR"))
                {
                    bool displayHdr = false;
                    float displayPeak = 0.0f;
                    Render::DisplayHdr(displayHdr, displayPeak);
                    ImGui::Checkbox("HDR", &g_edit.hdr);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(an scRGB picture tone mapped by RenoDX; the game's colour targets 16 bit float)");
                    if (Render::HdrActive() && !displayHdr)
                        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Windows has this display in SDR: turn on \"Use HDR\" in Windows' display settings.");
                    else if (displayHdr)
                        ImGui::TextDisabled("This display is in HDR; Windows reports a peak of %.0f nits.", displayPeak);
                    if (g_edit.hdr && Render::HdrCompiling() > 0) ImGui::TextDisabled("Compiling the HDR shaders...");
                    ImGui::BeginDisabled(!g_edit.hdr);
                    static const char* const toneMappers[] = { "Vanilla (clipped, as the SDR picture)", "None (untonemapped)", "ACES", "RenoDRT",
                                                               "PsychoV 17", "PsychoV 22", "PsychoV 30", "Prism" };
                    ImGui::Combo("Tone mapper", &g_edit.hdrToneMapper, toneMappers, 8);
                    {
                        char peakText[64];
                        if (g_edit.hdrPeakNits <= 0.0f) snprintf(peakText, sizeof(peakText), "The display's (%.0f nits)", displayPeak > 0.0f ? displayPeak : 1000.0f);
                        else snprintf(peakText, sizeof(peakText), "%%.0f nits");
                        ImGui::SliderFloat("Peak brightness", &g_edit.hdrPeakNits, 0.0f, 4000.0f, peakText);
                        ImGui::SameLine();
                        if (ImGui::SmallButton("Display's")) g_edit.hdrPeakNits = 0.0f;
                    }
                    ImGui::SliderFloat("Game brightness", &g_edit.hdrGameNits, 80.0f, 500.0f, "%.0f nits");
                    ImGui::SliderFloat("UI brightness", &g_edit.hdrUiNits, 80.0f, 500.0f, "%.0f nits");
                    ImGui::Checkbox("HUD white at the UI brightness", &g_edit.hdrHudMatch);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(the game draws its HUD's white at about 86%%: off, it peaks at 70%% of the UI brightness)");
                    static const char* const gammas[] = { "sRGB", "2.2", "2.4" };
                    ImGui::Combo("Gamma correction", &g_edit.hdrGammaCorrection, gammas, 3);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(how the SDR picture is decoded; 2.2 as a PC monitor shows it)");
                    ImGui::SliderFloat("Console TV gamma ramp", &g_edit.hdrTvRamp, 0.0f, 100.0f, "%.0f%%");
                    ImGui::SameLine();
                    ImGui::TextDisabled("(the game's own display curve, held to Game brightness: white and mid grey stay put, the shadows go deeper as on the console; lower, lighter shadows)");
                    ImGui::Checkbox("Upgrade films to HDR", &g_edit.hdrFilmUpgrade);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(RenoDX's video upscale for the cutscene films)");
                    ImGui::EndDisabled();
                    ImGui::SliderFloat("Bloom", &g_edit.hdrBloom, 0.0f, 100.0f, "%.0f");
                    ImGui::SameLine();
                    ImGui::TextDisabled("(50: the game's own; in SDR too)");
                    if (ImGui::Button("Reset HDR"))
                    {
                        const Settings::Values defaults = Settings::Defaults();
                        g_edit.hdrToneMapper = defaults.hdrToneMapper;
                        g_edit.hdrPeakNits = defaults.hdrPeakNits;
                        g_edit.hdrGameNits = defaults.hdrGameNits;
                        g_edit.hdrUiNits = defaults.hdrUiNits;
                        g_edit.hdrGammaCorrection = defaults.hdrGammaCorrection;
                        g_edit.hdrTvRamp = defaults.hdrTvRamp;
                        g_edit.hdrFilmUpgrade = defaults.hdrFilmUpgrade;
                        g_edit.hdrHudMatch = defaults.hdrHudMatch;
                        g_edit.hdrBloom = defaults.hdrBloom;
                    }
                    ImGui::Separator();
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextDisabled("The HDR mode is built on RenoDX by Carlos Lopez Jr. and its contributors "
                                        "(github.com/clshortfuse/renodx, MIT): its tone mappers, its sliders and its scRGB output. "
                                        "Prism tone mapper by Musa Haji and OopyDoopy / KickFister / Jon (MIT).");
                    ImGui::PopTextWrapPos();
                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("RenoDX"))
                {
                    const int toneMapper = g_edit.hdrToneMapper;
                    const bool renoDrt = toneMapper == 3, psychoV = toneMapper >= 4 && toneMapper <= 6, prism = toneMapper == 7;
                    if (!g_edit.hdr) ImGui::TextDisabled("These are the HDR mode's: turn HDR on in the HDR tab.");
                    ImGui::BeginDisabled(!g_edit.hdr);
                    ImGui::TextDisabled("RenoDX's sliders. 50 is the game's own look; they apply as they move.");
                    ImGui::SliderFloat("Exposure", &g_edit.hdrExposure, 0.0f, 4.0f, "%.2f");
                    ImGui::SliderFloat("Highlights", &g_edit.hdrHighlights, 0.0f, 100.0f, "%.0f");
                    ImGui::SliderFloat("Shadows", &g_edit.hdrShadows, 0.0f, 100.0f, "%.0f");
                    ImGui::SliderFloat("Contrast", &g_edit.hdrContrast, 0.0f, 100.0f, "%.0f");
                    ImGui::SliderFloat("Saturation", &g_edit.hdrSaturation, 0.0f, 100.0f, "%.0f");
                    ImGui::BeginDisabled(!renoDrt);
                    ImGui::SliderFloat("Highlight saturation", &g_edit.hdrHighlightSaturation, 0.0f, 100.0f, "%.0f");
                    ImGui::SliderFloat("Blowout", &g_edit.hdrBlowout, 0.0f, 100.0f, "%.0f");
                    ImGui::SliderFloat("Flare", &g_edit.hdrFlare, 0.0f, 100.0f, "%.0f");
                    ImGui::EndDisabled();
                    if (!renoDrt) { ImGui::SameLine(); ImGui::TextDisabled("(RenoDRT's)"); }
                    ImGui::BeginDisabled(psychoV || prism);
                    ImGui::SliderFloat("Hue correction", &g_edit.hdrHueCorrection, 0.0f, 100.0f, "%.0f");
                    ImGui::EndDisabled();
                    ImGui::BeginDisabled(!psychoV);
                    ImGui::SliderFloat("Cone response", &g_edit.hdrConeResponse, 0.0f, 100.0f, "%.0f");
                    ImGui::EndDisabled();
                    if (!psychoV) { ImGui::SameLine(); ImGui::TextDisabled("(PsychoV's)"); }
                    if (ImGui::Button("Reset sliders"))
                    {
                        const Settings::Values defaults = Settings::Defaults();
                        g_edit.hdrExposure = defaults.hdrExposure;
                        g_edit.hdrHighlights = defaults.hdrHighlights;
                        g_edit.hdrShadows = defaults.hdrShadows;
                        g_edit.hdrContrast = defaults.hdrContrast;
                        g_edit.hdrSaturation = defaults.hdrSaturation;
                        g_edit.hdrHighlightSaturation = defaults.hdrHighlightSaturation;
                        g_edit.hdrBlowout = defaults.hdrBlowout;
                        g_edit.hdrFlare = defaults.hdrFlare;
                        g_edit.hdrHueCorrection = defaults.hdrHueCorrection;
                        g_edit.hdrConeResponse = defaults.hdrConeResponse;
                    }
                    ImGui::EndDisabled();
                    ImGui::Separator();
                    ImGui::PushTextWrapPos(0.0f);
                    ImGui::TextDisabled("RenoDX by Carlos Lopez Jr. and contributors, github.com/clshortfuse/renodx (MIT): "
                                        "RenoDRT, ACES and the PsychoV tone mappers, and these sliders.");
                    ImGui::PopTextWrapPos();
                    ImGui::EndTabItem();
                }
                // The HDR tabs apply as they change: their values go into
                // what is in use at once, and to the file when no slider is
                // being dragged.
                {
                    static bool previewed = false;
                    Settings::Values live = Settings::Get();
                    if (CopyHdrSettings(g_edit, live)) { Settings::Preview(live); previewed = true; }
                    if (previewed && !ImGui::IsAnyItemActive()) { Settings::Save(); previewed = false; }
                }
                if (ImGui::BeginTabItem("Controls"))
                {
                    ImGui::SliderInt("Master volume", &g_edit.masterVolume, 0, 100, "%d%%");
                    {
                        static const char* const outputs[] = { "Automatic", "Stereo", "5.1 surround" };
                        ImGui::SetNextItemWidth(160);
                        ImGui::Combo("Speakers", &g_edit.audioChannels, outputs, 3);
                        ImGui::SameLine();
                        ImGui::TextDisabled("(the console's own six channel mix; Automatic picks 5.1 when Windows' speakers are set up for it)");
                    }
                    ImGui::SliderFloat("Mouse sensitivity", &g_edit.mouseSensitivity, 0.1f, 3.0f, "%.2f");
                    ImGui::TextDisabled("Both axes alike.");
                    ImGui::Checkbox("Raw mouse input", &g_edit.rawMouse);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(the mouse's own movement, without Windows' pointer speed and acceleration)");
                    ImGui::Checkbox("Direct mouse aim (PC style)", &g_edit.directMouse);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("The mouse turns the view one to one, as in a PC shooter. Off: the mouse\n"
                                          "stands in for the right stick, with the console's turn speed-up and limits.");
                    ImGui::Checkbox("Tap to sprint (L3 / Shift once, not held)", &g_edit.toggleSprint);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("One press of sprint keeps running until you stop pushing forward, aim, fire\n"
                                          "or press sprint again. Off: sprint lasts only while the button is held.");
                    ImGui::Checkbox("Aim assist", &g_edit.aimAssist);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(the game's own; takes effect after a restart)");
                    ImGui::Checkbox("Controller support", &g_edit.controller);
                    ImGui::SameLine();
                    ImGui::TextDisabled("(%s)", Input::ControllerStatus().c_str());
                    ImGui::Checkbox("Controller vibration", &g_edit.rumble);
                    ImGui::SliderInt("Stick deadzone", &g_edit.stickDeadzone, 0, 50, "%d%%");
                    ImGui::SliderInt("Vertical look speed", &g_edit.stickVerticalScale, 50, 300, g_edit.stickVerticalScale == 200 ? "200%% (matches turning)" : g_edit.stickVerticalScale == 100 ? "100%% (the game's own)" : "%d%%");
                    ImGui::TextDisabled("The game looks up and down at half the speed it turns; 200%% makes them match.");
                    ImGui::TextDisabled("Raise it if the view or the soldier drifts with the sticks untouched.");
                    ImGui::Separator();
                    ImGui::TextUnformatted("Keys");
                    if (ImGui::BeginTable("keys", 2, ImGuiTableFlags_SizingStretchProp))
                    {
                        for (int i = 0; i < Settings::ActionCount; i++)
                        {
                            ImGui::TableNextRow();
                            ImGui::TableNextColumn();
                            ImGui::TextUnformatted(Settings::ActionName(Settings::Action(i)));
#ifndef COD3_MP
                            if (i == Settings::BinocularsOut && ImGui::IsItemHovered())
                                ImGui::SetTooltip("Campaign: hold to look through the binoculars, standing still\n"
                                                  "(as holding the left stick in does on the controller).");
#endif
                            ImGui::TableNextColumn();
                            ImGui::PushID(i);
                            const bool waiting = g_capturing == i;
                            if (ImGui::Button(waiting ? "Press a key... (Esc: cancel)" : Settings::KeyName(g_edit.keys[i]), ImVec2(-1, 0)))
                                g_capturing = waiting ? -1 : i;
                            ImGui::PopID();
                        }
                        ImGui::EndTable();
                    }
                    if (ImGui::Button("Default keys"))
                    {
                        const Settings::Values defaults = Settings::Defaults();
                        for (int i = 0; i < Settings::ActionCount; i++) g_edit.keys[i] = defaults.keys[i];
                    }
                    ImGui::EndTabItem();
                }
#ifndef COD3_MP
                if (ImGui::BeginTabItem("Gameplay"))
                {
                    ImGui::TextUnformatted("Hitmarkers");
                    ImGui::Checkbox("Enable hitmarkers", &g_edit.hitmarkers);
                    ImGui::BeginDisabled(!g_edit.hitmarkers);
                    ImGui::SliderInt("Size (at 1080p)", &g_edit.hitmarkerSize, 16, 160, "%d px");
                    ImGui::SliderInt("Opacity", &g_edit.hitmarkerOpacity, 0, 100, "%d%%");
                    ImGui::SliderInt("Display duration", &g_edit.hitmarkerDuration, 50, 1000, "%d ms");
                    ImGui::Checkbox("Red hitmarker on confirmed kill", &g_edit.hitmarkerRedKill);
                    ImGui::Checkbox("Hitmarker sound", &g_edit.hitmarkerSound);
                    ImGui::BeginDisabled(!g_edit.hitmarkerSound);
                    ImGui::SliderInt("Hitmarker volume", &g_edit.hitmarkerVolume, 0, 100, "%d%%");
                    ImGui::EndDisabled();
                    ImGui::TextDisabled("Apply changes to save. Sound also follows master volume.");
                    if (ImGui::Button("Test hit")) Hitmarkers::Preview(false);
                    ImGui::SameLine();
                    if (ImGui::Button("Test kill")) Hitmarkers::Preview(true);
                    ImGui::EndDisabled();
                    ImGui::Separator();
                    if (ImGui::CollapsingHeader("Blood & Gore", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        bool changed=false;
                        auto& b=g_edit.blood;
                        changed |= ImGui::Checkbox("Enable Blood", &b.enabled);
                        ImGui::BeginDisabled(!b.enabled);
                        changed |= ImGui::SliderInt("Blood Amount", &b.amount,0,200,"%d%%");
                        const char* densities[]={"Low","Medium","High","Ultra"};
                        changed |= ImGui::Combo("Blood Particle Density", &b.density,densities,4);
                        changed |= ImGui::SliderFloat("Blood Splatter Size", &b.size,0.25f,3.0f,"%.2fx");
                        changed |= ImGui::SliderFloat("Blood Cloud Size", &b.cloudSize,.25f,4.f,"%.2fx");
                        changed |= ImGui::SliderFloat("Blood Cloud Opacity", &b.cloudOpacity,0.f,3.f,"%.2fx");
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Multiplier for COD3 native blood clouds. 1x retains the original appearance.");
                        changed |= ImGui::Checkbox("Blood Decals", &b.decals);
                        changed |= ImGui::Checkbox("Blood Physics", &b.physics);
                        if(ImGui::IsItemHovered())ImGui::SetTooltip("Gravity and drag. Surface collisions remain active when gravity is off.");
                        bool permanent=b.persistence==0;
                        if(ImGui::Checkbox("Permanent blood decals", &permanent)){b.persistence=permanent?0:30;changed=true;}
                        ImGui::BeginDisabled(permanent);
                        changed |= ImGui::SliderInt("Blood Persistence", &b.persistence,1,600,"%d seconds");
                        ImGui::EndDisabled();
                        changed |= ImGui::SliderInt("Maximum Blood Decals", &b.maxDecals,8,Blood::MaxDecals);
                        changed |= ImGui::ColorEdit3("Blood Color",b.color);
                        ImGui::EndDisabled();
                        if(ImGui::Button("Reset Blood to Defaults")){b=Blood::Options{};changed=true;}
                        static bool pendingSave=false;
                        if(changed){auto live=Settings::Get();live.blood=b;Settings::Preview(live);pendingSave=true;}
                        if(pendingSave&&!ImGui::IsAnyItemActive()){Settings::Save();pendingSave=false;}
                        ImGui::TextDisabled("Blood changes apply immediately and save automatically.");
                        ImGui::TextDisabled("Vertical slice: impacts and static-surface splatters. Pooling and trails follow after testing.");
                    }

                    ImGui::EndTabItem();
                }
#endif
                if (ImGui::BeginTabItem("Multiplayer"))
                {
#ifndef COD3_MP
                    ImGui::TextDisabled("These are for CoD3MP.exe, the multiplayer program (Multiplayer in the main menu starts it).");
#endif
                    ImGui::InputText("Player name", g_edit.playerName, sizeof(g_edit.playerName));
                    ImGui::SameLine();
                    ImGui::TextDisabled("(what others see; empty is your Windows name)");
                    ImGui::InputText("System Link address", g_edit.lanAddress, sizeof(g_edit.lanAddress));
                    ImGui::SameLine();
                    ImGui::TextDisabled("(empty: automatic; after a restart)");
                    ImGui::InputText("System Link peers", g_edit.systemLinkPeers, sizeof(g_edit.systemLinkPeers));
                    ImGui::SameLine();
                    ImGui::TextDisabled("(friends' IPs, comma separated; after a restart)");
                    ImGui::Separator();
                    ImGui::TextUnformatted("Network");
                    const Net::Summary net = Net::Status();
                    ImGui::Text("This PC on System Link: %s", net.address.c_str());
                    for (const std::string& line : net.adapters) ImGui::BulletText("%s", line.c_str());
                    if (net.peers.empty()) ImGui::TextDisabled("No peers: games are found by broadcast only.");
                    else
                    {
                        std::string list;
                        for (const std::string& peer : net.peers) list += (list.empty() ? "" : ", ") + peer;
                        ImGui::Text("Peers: %s", list.c_str());
                    }
#ifdef COD3_MP
                    ImGui::Text("Sockets open: %u   Packets sent: %llu   received: %llu%s%s", net.sockets, net.sent, net.received,
                        net.lastFrom.empty() ? "" : "   last from ", net.lastFrom.c_str());
#endif
                    ImGui::Separator();
                    ImGui::Checkbox("CoD4 bounces", &g_edit.bounces);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Jumping onto a slope at the right angle bounces you off it, as in Call of Duty 4 and\n"
                                          "World at War. Everyone in a match should have the same setting, or a bounce gets\n"
                                          "taken back by the host as it happens.");
                    ImGui::Checkbox("Quick landing", &g_edit.quickLanding);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("After a heavy fall you are slowed for a moment only (at most 0.15 seconds),\n"
                                          "not the game's own half a second to two seconds.");
                    ImGui::Checkbox("Quick sprint recovery", &g_edit.quickSprint);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Stamina starts coming back a quarter second after a sprint and is full\n"
                                          "in a second and a half, so you can sprint again sooner.");
                    if (ImGui::CollapsingHeader("Custom classes (multiplayer)", ImGuiTreeNodeFlags_DefaultOpen))
                    {
                        const char* weapons[] = { "The class's own", "M1 Garand", "Thompson", "BAR", "Springfield (scoped)",
                                                  ".30 cal machine gun", "Bazooka", "Trench gun", "Kar98k", "MP40", "StG 44",
                                                  "Kar98k (scoped)", "MG34", "Panzerschreck", "Lee-Enfield", "Bren", "Sten", "FG42", "FG42 (scoped)", "Gewehr 43", "MG42", "Lee-Enfield (scoped)" };
                        const char* sidearms[] = { "The side's own", "Colt .45", "P38", "M1 Garand", "Thompson", "BAR", "Springfield (scoped)",
                                                   ".30 cal machine gun", "Bazooka", "Trench gun", "Kar98k", "MP40", "StG 44",
                                                   "Kar98k (scoped)", "MG34", "Panzerschreck", "Lee-Enfield", "Bren", "Sten", "FG42", "FG42 (scoped)", "Gewehr 43", "MG42", "Lee-Enfield (scoped)" };
                        ImGui::TextDisabled("Pick one on the class screen with L1 / R1 (or here). The class you pick in the game's\n"
                                            "menu still gives its grenades and ability (medic healing, scout artillery and so on).");
                        ImGui::RadioButton("None: the class's own kit", &g_edit.activeClass, 0);
                        for (int i = 0; i < Settings::Values::CustomClasses; i++)
                        {
                            ImGui::PushID(i);
                            ImGui::RadioButton("##use", &g_edit.activeClass, i + 1);
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(140);
                            ImGui::InputText("##name", g_edit.className[i], sizeof(g_edit.className[i]), ImGuiInputTextFlags_CharsUppercase);
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(200);
                            ImGui::Combo("##primary", &g_edit.classPrimary[i], weapons, 19);
                            ImGui::SameLine();
                            ImGui::SetNextItemWidth(140);
                            ImGui::Combo("##sidearm", &g_edit.classSidearm[i], sidearms, 21);
                            ImGui::PopID();
                        }
                    }
                    ImGui::Checkbox("Faster aiming walk", &g_edit.fasterAds);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Walking while aiming down the sights is quicker (never quicker than from the hip).");
                    if (g_edit.fasterAds)
                        ImGui::SliderInt("Aiming walk speed", &g_edit.aimWalk, 40, 100, "%d%% of the hip walk");
                    // 0.5.197: scoped rifles their own walk and sway.
                    ImGui::SliderInt("Scoped walk speed", &g_edit.sniperWalk, 15, 100, "%d%% of the hip walk");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Walking with a sniper scope up (the game's own is 40%%; other guns use Aiming walk speed).");
                    ImGui::SliderInt("Scoped sway", &g_edit.sniperSway, 0, 100, "%d%% of the game's");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("How much a sniper scope drifts while you aim (hold breath still steadies it).");
                    ImGui::Checkbox("Bots use vehicles and mounted guns", &g_edit.botVehicles);
                    ImGui::Checkbox("Rocket launcher on d-pad left", &g_edit.launcher);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Every spawn also gets your side's launcher (bazooka or Panzerschreck) with a few rockets:\n"
                                          "d-pad left takes it out, press again for your main weapon.");
                    ImGui::Checkbox("Binoculars for everyone", &g_edit.binoculars);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Every class also gets binoculars: d-pad up (or the Binoculars out key, B) puts them to\n"
                                          "your eyes, d-pad down (Binoculars away, N) puts them away. The left trigger (aim) zooms\n"
                                          "x2 and back. The right trigger calls artillery when a killstreak has earned one.");
                    ImGui::Checkbox("Killstreaks", &g_edit.killstreakArtillery);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Hosting: kills in a row without dying earn rewards. The UAV shows every enemy on the\n"
                                          "compass; the artillery barrage and the Typhoon are called in with the binoculars\n"
                                          "(d-pad up, then the right trigger at the spot).");
                    if (g_edit.killstreakArtillery)
                    {
                        ImGui::SliderInt("Kills for the UAV", &g_edit.killstreakUav, 1, 15, "%d in a row");
                        ImGui::SliderInt("UAV time", &g_edit.uavSeconds, 10, 90, "%d seconds");
                        ImGui::SliderInt("Kills for artillery", &g_edit.killstreakKills, 1, 20, "%d in a row");
                        ImGui::SliderInt("Kills for the Typhoon", &g_edit.killstreakTyphoon, 1, 25, "%d in a row");
                    }
                    ImGui::Checkbox("Four of the second grenade and mines", &g_edit.fourSecondary);
                    {
                        // 0.5.247: 32 players (a test): read when the game starts.
                        bool thirtyTwo = g_edit.maxPlayers >= 32;
                        if (ImGui::Checkbox("32 players (test, restart the game)", &thirtyTwo)) g_edit.maxPlayers = thirtyTwo ? 32 : 24;
                        if (ImGui::IsItemHovered())
                            ImGui::SetTooltip("Up to 32 players in a match instead of 24 (you and up to 28 bots). Takes effect\n"
                                              "the next time the game starts. Both PCs of a System Link game must use the\n"
                                              "same setting. Still a test: the scoreboard lists only 24 players.");
                    }
                    ImGui::SliderInt("Ability recharge", &g_edit.abilityRecharge, 10, 100, g_edit.abilityRecharge >= 100 ? "the game's own" : "%d%% of the wait");
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Multiplayer: the scout's artillery (three minutes normally), the medic's health packs\n"
                                          "and the support's ammo come back in this share of the usual time.");
                    ImGui::Checkbox("Two grenades", &g_edit.twoGrenades);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Multiplayer: you start with at least two grenades (more as you rank up).");
                    ImGui::Checkbox("Melee lunge", &g_edit.meleeLunge);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("A melee with an enemy close in front pulls you at him, as World at War's bayonet\n"
                                          "and Modern Warfare 2's commando knife do. Everyone in a match wants the same setting.");
                    ImGui::Checkbox("Shell shock screen blur", &g_edit.shellShockBlur);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("The blurred, dark screen after an explosion close by (a bazooka at your feet), which\n"
                                          "hides the HUD for a moment. Off: the screen stays clear; the shock's sound and kick stay.");
                    ImGui::Separator();
                    ImGui::TextWrapped("Playing with a friend: both of you on the same LAN, or in the same Hamachi, ZeroTier or "
                                       "Radmin network; one hosts under System Link, the other searches. On Tailscale, which "
                                       "carries no broadcasts, each of you puts the other's Tailscale address in the peers.");
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            ImGui::Separator();
            // Apply writes and uses the values; Cancel drops the edits.
            const bool dirty = memcmp(&g_edit, &applied, sizeof(Settings::Values)) != 0;
            ImGui::BeginDisabled(!dirty);
            if (ImGui::Button("Apply", ImVec2(140 * g_fontScale, 0)))
            {
                Settings::Set(g_edit);
                g_capturing = -1;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(140 * g_fontScale, 0)))
            {
                g_edit = applied;
                g_capturing = -1;
            }
            ImGui::SameLine();
            if (ImGui::Button("Close", ImVec2(140 * g_fontScale, 0))) open = false;
            if (dirty) { ImGui::SameLine(); ImGui::TextDisabled("(unapplied changes)"); }
            ImGui::TextDisabled("F10: photo mode (no HUD or gun)   F11: this menu   F12: screenshot   ` (left of 1): console   Esc: back to the game");
        }
        ImGui::End();
        (void)changed;
        if (!open) { g_open.store(false); g_capturing = -1; }
    }
}

void Overlay::Initialize(void* hwnd, ID3D11Device* device, ID3D11DeviceContext* context)
{
    if (g_ready.load()) return;
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    g_hwnd = static_cast<HWND>(hwnd);
    g_device = device;
    g_context = context;
    Hitmarkers::Initialize(device);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    // A readable size on any screen: the font grows with the resolution
    // the picture is drawn at, which is the window's size or the screen's.
    int width = 0, height = 0;
    Settings::Resolution(Settings::Get(), width, height);
    g_fontScale = std::min(2.0f, std::max(1.0f, float(height) / 900.0f));
    static const ImWchar ranges[] = { 0x0020, 0x00FF, 0x0100, 0x017F, 0 };   // Latin with the Hungarian letters
    ImFontConfig config;
    config.OversampleH = 2;
    if (io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 20.0f * g_fontScale, &config, ranges) == nullptr)
        io.Fonts->AddFontDefault();
    ImGui::StyleColorsDark();
    {
        // The title's own front end: dark sepia panels, tan text, the
        // chosen entry in a lighter gold, thin brown rules.
        ImGuiStyle& style = ImGui::GetStyle();
        ImVec4* c = style.Colors;
        const ImVec4 tan(0.86f, 0.80f, 0.64f, 1.00f), gold(0.95f, 0.84f, 0.52f, 1.00f), dim(0.58f, 0.52f, 0.40f, 1.00f);
        const ImVec4 panel(0.10f, 0.085f, 0.065f, 0.94f), panelLight(0.16f, 0.13f, 0.095f, 1.00f);
        const ImVec4 frame(0.22f, 0.18f, 0.13f, 0.95f), frameHover(0.33f, 0.27f, 0.18f, 1.00f), frameActive(0.44f, 0.36f, 0.22f, 1.00f);
        const ImVec4 rule(0.45f, 0.38f, 0.24f, 0.70f);
        c[ImGuiCol_Text] = tan;
        c[ImGuiCol_TextDisabled] = dim;
        c[ImGuiCol_WindowBg] = panel;
        c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.10f, 0.075f, 0.97f);
        c[ImGuiCol_Border] = rule;
        c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_FrameBg] = frame;
        c[ImGuiCol_FrameBgHovered] = frameHover;
        c[ImGuiCol_FrameBgActive] = frameActive;
        c[ImGuiCol_TitleBg] = panelLight;
        c[ImGuiCol_TitleBgActive] = ImVec4(0.22f, 0.18f, 0.12f, 1.00f);
        c[ImGuiCol_TitleBgCollapsed] = panel;
        c[ImGuiCol_MenuBarBg] = panelLight;
        c[ImGuiCol_ScrollbarBg] = ImVec4(0.08f, 0.07f, 0.05f, 0.8f);
        c[ImGuiCol_ScrollbarGrab] = frameHover;
        c[ImGuiCol_ScrollbarGrabHovered] = frameActive;
        c[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.60f, 0.50f, 0.30f, 1.00f);
        c[ImGuiCol_CheckMark] = gold;
        c[ImGuiCol_SliderGrab] = ImVec4(0.75f, 0.63f, 0.38f, 1.00f);
        c[ImGuiCol_SliderGrabActive] = gold;
        c[ImGuiCol_Button] = ImVec4(0.30f, 0.25f, 0.16f, 1.00f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.46f, 0.38f, 0.23f, 1.00f);
        c[ImGuiCol_ButtonActive] = ImVec4(0.62f, 0.50f, 0.28f, 1.00f);
        c[ImGuiCol_Header] = ImVec4(0.30f, 0.25f, 0.16f, 1.00f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.46f, 0.38f, 0.23f, 1.00f);
        c[ImGuiCol_HeaderActive] = ImVec4(0.62f, 0.50f, 0.28f, 1.00f);
        c[ImGuiCol_Separator] = rule;
        c[ImGuiCol_SeparatorHovered] = gold;
        c[ImGuiCol_SeparatorActive] = gold;
        c[ImGuiCol_ResizeGrip] = rule;
        c[ImGuiCol_ResizeGripHovered] = gold;
        c[ImGuiCol_ResizeGripActive] = gold;
        c[ImGuiCol_Tab] = ImVec4(0.20f, 0.16f, 0.11f, 1.00f);
        c[ImGuiCol_TabHovered] = ImVec4(0.46f, 0.38f, 0.23f, 1.00f);
        c[ImGuiCol_TabActive] = ImVec4(0.36f, 0.29f, 0.18f, 1.00f);
        c[ImGuiCol_TabUnfocused] = panelLight;
        c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.28f, 0.23f, 0.15f, 1.00f);
        c[ImGuiCol_TableHeaderBg] = panelLight;
        c[ImGuiCol_TableBorderStrong] = rule;
        c[ImGuiCol_TableBorderLight] = ImVec4(0.30f, 0.25f, 0.16f, 0.5f);
        c[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
        c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 0.9f, 0.7f, 0.04f);
        c[ImGuiCol_TextSelectedBg] = ImVec4(0.62f, 0.50f, 0.28f, 0.45f);
        c[ImGuiCol_NavHighlight] = gold;
        c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.05f, 0.04f, 0.03f, 0.6f);
        style.WindowRounding = 2.0f;
        style.FrameRounding = 2.0f;
        style.GrabRounding = 2.0f;
        style.TabRounding = 2.0f;
        style.WindowBorderSize = 1.0f;
        style.FrameBorderSize = 1.0f;
    }
    ImGui::GetStyle().ScaleAllSizes(g_fontScale);
    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(device, context);
    g_tick = std::chrono::steady_clock::now();
    g_ready.store(true);
}

namespace
{
    // The front end's pages seen this frame and the last, as bits.
    std::atomic<uint32_t> g_frontEndNow{ 0 };
    uint32_t g_frontEndLast = 0;
}

void Overlay::NoteFrame()
{
    g_frames.fetch_add(1, std::memory_order_relaxed);
    const uint32_t now = g_frontEndNow.exchange(0);
    // The options page arriving from the main menu: the player pressed
    // OPTIONS, and the settings come up over it. Not when it comes back
    // from one of its own sub pages, and not while the menu is up already.
    const bool wasMain = (g_frontEndLast & uint32_t(FrontEndText::MainMenu)) != 0;
    const bool isOptions = (now & uint32_t(FrontEndText::OptionsMenu)) != 0;
    const bool wasOptions = (g_frontEndLast & uint32_t(FrontEndText::OptionsMenu)) != 0;
    if (isOptions && !wasOptions && wasMain && !g_open.load() && !g_console.load())
    {
        g_open.store(true);
        g_capturing = -1;
        g_editLoaded = false;
    }
    g_frontEndLast = now;
}

namespace
{
    std::atomic<bool (*)()> g_hudActive{ nullptr };
    std::atomic<void (*)(int, int)> g_hudDraw{ nullptr };
    std::atomic<bool (*)()> g_menuHudActive{ nullptr };
    std::atomic<void (*)(int, int)> g_menuHudDraw{ nullptr };
    std::atomic<bool (*)()> g_streakHudActive{ nullptr };
    std::atomic<void (*)(int, int)> g_streakHudDraw{ nullptr };
}

// A short message across the top of the picture for a few seconds (the
// killstreaks' "UAV online" and such, mp_fixes.cpp).
namespace
{
    std::mutex g_noticeLock;
    std::string g_notice;
    uint64_t g_noticeUntil = 0;

    bool NoticeShown()
    {
        std::lock_guard<std::mutex> lock(g_noticeLock);
        return !g_notice.empty() && GetTickCount64() < g_noticeUntil;
    }

    void DrawNotice(int width, int height)
    {
        std::string text;
        uint64_t until = 0;
        {
            std::lock_guard<std::mutex> lock(g_noticeLock);
            text = g_notice;
            until = g_noticeUntil;
        }
        if (text.empty() || GetTickCount64() >= until) return;
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float size = 34.0f * height / 1080.0f;
        const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text.c_str());
        // Fading out over its last half second.
        const float left = float(until - GetTickCount64());
        const int alpha = left < 500.0f ? int(255.0f * left / 500.0f) : 255;
        const ImVec2 at((width - extent.x) * 0.5f, height * 0.22f);
        draw->AddText(font, size, ImVec2(at.x + 2, at.y + 2), IM_COL32(0, 0, 0, alpha * 3 / 4), text.c_str());
        draw->AddText(font, size, at, IM_COL32(255, 214, 90, alpha), text.c_str());
    }
}

void Overlay::Notice(const std::string& text, int milliseconds)
{
    std::lock_guard<std::mutex> lock(g_noticeLock);
    g_notice = text;
    g_noticeUntil = GetTickCount64() + uint64_t(milliseconds);
}

void Overlay::SetMenuHud(bool (*active)(), void (*draw)(int width, int height))
{
    g_menuHudDraw.store(draw);
    g_menuHudActive.store(active);
}

void Overlay::SetStreakHud(bool (*active)(), void (*draw)(int width, int height))
{
    g_streakHudDraw.store(draw);
    g_streakHudActive.store(active);
}

void Overlay::SetHud(bool (*active)(), void (*draw)(int width, int height))
{
    g_hudDraw.store(draw);
    g_hudActive.store(active);
}

void Overlay::NoteFrontEndText(FrontEndText text)
{
    if(text==FrontEndText::MainMenu) Input::MainMenuSeen();
    g_frontEndNow.fetch_or(uint32_t(text), std::memory_order_relaxed);
}

bool Overlay::IsOpen() { return g_open.load() || g_console.load(); }

void Overlay::Toggle()
{
    g_open.store(!g_open.load());
    g_capturing = -1;
    g_editLoaded = false;
}

void Overlay::ToggleConsole()
{
    const bool open = !g_console.load();
    g_console.store(open);
    if (open) { g_consoleFocus = true; g_consoleInput[0] = 0; }
}

void Overlay::ConsolePrint(const std::string& line)
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    ConsoleLine(line);
}

void Overlay::RequestScreenshot() { g_screenshotWanted.store(true); }

bool Overlay::TakeScreenshotRequest() { return g_screenshotWanted.exchange(false); }

void Overlay::SaveScreenshot(ID3D11Texture2D* back, int width, int height, float scRgbWhite)
{
    if (!g_ready.load() || back == nullptr) return;
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    SaveScreenshotOf(back, width, height, scRgbWhite);
}

bool Overlay::HandleMessage(void* hwnd, uint32_t message, uint64_t wParam, int64_t lParam)
{
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)
    {
        if (wParam == VK_F11) { Toggle(); return true; }
        // F12: the picture, and the next frame's draws to the log, so a
        // fault seen while playing can be read afterwards.
        // 0.5.228: F10, the photo mode: the HUD and the gun off (cg_draw2D,
        // cg_drawGun) for clean pictures, and F12 then takes the picture
        // alone (no draw log). F10 again puts them back.
        static bool photo = false;
        if (wParam == VK_F10)
        {
            photo = !photo;
            Kernel::QueueConsoleCommand(photo ? "cg_draw2D 0\ncg_drawGun 0" : "cg_draw2D 1\ncg_drawGun 1");
            printf("photo mode %s\n", photo ? "on (HUD and gun hidden)" : "off"); fflush(stdout);
            return true;
        }
        if (wParam == VK_F12) { RequestScreenshot(); if (!photo) Render::LogNextFrame(); return true; }
        // The key left of 1 (scan code 0x29: ` and ~ on a US keyboard, 0
        // on a Hungarian one): the console, as the title's own configs
        // bind it. By scan code, so it is that key on every layout.
        if (((lParam >> 16) & 0xFF) == 0x29 && !g_open.load()) { ToggleConsole(); return true; }
        if (wParam == VK_ESCAPE && g_console.load() && !g_open.load()) { g_console.store(false); return true; }
    }
    // The character that key makes must not land in the console's input.
    if (message == WM_CHAR && ((lParam >> 16) & 0xFF) == 0x29 && g_console.load()) return true;
    if (!g_ready.load() || (!g_open.load() && !g_console.load())) return false;
    // A binding waiting for a key takes the next key or mouse button.
    if (g_capturing >= 0)
    {
        if (message == WM_KEYDOWN || message == WM_SYSKEYDOWN) { Capture(int(wParam)); return true; }
        if (message == WM_LBUTTONDOWN) { Capture(VK_LBUTTON); return true; }
        if (message == WM_RBUTTONDOWN) { Capture(VK_RBUTTON); return true; }
        if (message == WM_MBUTTONDOWN) { Capture(VK_MBUTTON); return true; }
        if (message == WM_XBUTTONDOWN) { Capture(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2); return true; }
    }
    else if ((message == WM_KEYDOWN || message == WM_SYSKEYDOWN) && wParam == VK_ESCAPE)
    {
        g_open.store(false);
        return true;
    }
    ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(hwnd), message, WPARAM(wParam), LPARAM(lParam));
    // While the menu is up the keys and the mouse are its.
    return message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP ||
           message == WM_CHAR || message == WM_MOUSEWHEEL || message == WM_LBUTTONDOWN || message == WM_LBUTTONUP ||
           message == WM_RBUTTONDOWN || message == WM_RBUTTONUP || message == WM_MBUTTONDOWN || message == WM_MBUTTONUP;
}

bool Overlay::Render(ID3D11RenderTargetView* back, ID3D11Texture2D* backTexture, int width, int height)
{
    if (!g_ready.load()) return false;
    std::lock_guard<std::recursive_mutex> lock(g_mutex);
    // The counter, once a second.
    {
        const auto now = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(now - g_tick).count();
        if (seconds >= 1.0)
        {
            const uint64_t frames = g_frames.load(std::memory_order_relaxed);
            g_fps = double(frames - g_framesAtTick) / seconds;
            g_framesAtTick = frames;
            g_tick = now;
        }
    }
    // The screenshot first, of the picture alone.
    if (backTexture != nullptr && g_screenshotWanted.exchange(false)) SaveScreenshotOf(backTexture, width, height, 0.0f);

    const Settings::Values values = Settings::Get();
    const bool open = g_open.load();
    const bool console = g_console.load();
    bool (*hudActive)() = g_hudActive.load();
    void (*hudDraw)(int, int) = g_hudDraw.load();
    const bool hud = hudActive && hudDraw && hudActive();
    bool (*menuHudActive)() = g_menuHudActive.load();
    void (*menuHudDraw)(int, int) = g_menuHudDraw.load();
    const bool menuHud = menuHudActive && menuHudDraw && menuHudActive();
    bool (*streakHudActive)() = g_streakHudActive.load();
    void (*streakHudDraw)(int, int) = g_streakHudDraw.load();
    const bool streakHud = streakHudActive && streakHudDraw && streakHudActive();
    const bool marker = Hitmarkers::Active(values);
    const bool notice = NoticeShown();
    if (!open && !console && !hud && !menuHud && !streakHud && !marker && !notice && !values.fpsOverlay && !values.statsOverlay) return false;

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    if (values.fpsOverlay || values.statsOverlay)
    {
        ImGui::SetNextWindowPos(ImVec2(8, 8));
        ImGui::SetNextWindowBgAlpha(0.45f);
        if (ImGui::Begin("fps", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing))
        {
            ImGui::Text("%.0f fps", g_fps);
            if (values.statsOverlay)
            {
                // The renderer's own counters, averaged over the last second.
                const RenderStats::Snapshot s = RenderStats::Current();
                ImGui::Text("frame %.2f ms (worst %.1f)  cpu %.2f ms  gpu %s", s.frameMilliseconds, s.worstMilliseconds, s.cpuMilliseconds,
                    s.gpuProfiled ? "" : "(COD3_GPU_PROFILE=1)");
                if (s.gpuProfiled) { ImGui::SameLine(); ImGui::Text("%.2f ms", s.gpuMilliseconds); }
                ImGui::Text("%.0f draws  %.0f k triangles  %.0f resolves  %.0f skipped", s.draws, s.triangles / 1000.0f, s.resolves, s.skipped);
                ImGui::Text("switches: %.0f programs  %.0f textures  %.0f states  %.0f targets", s.shaderSwitches, s.textureSwitches, s.pipelineSwitches, s.targetSwitches);
                ImGui::Text("uploads: %.0f textures  %.0f buffers  %.0f KB;  streamed %.0f KB", s.textureUploads, s.bufferUploads, s.uploadKilobytes, s.streamKilobytes);
            }
        }
        ImGui::End();
    }
    if (marker && !console) Hitmarkers::Draw(width, height, values);
    if (hud) hudDraw(width, height);
    if (streakHud) streakHudDraw(width, height);
    if (menuHud) menuHudDraw(width, height);
    if (notice) DrawNotice(width, height);
    if (open) DrawMenu(width, height);
    else if (console) DrawConsole(width, height);
    ImGui::Render();
    ID3D11RenderTargetView* targets[1] = { back };
    g_context->OMSetRenderTargets(1, targets, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    return true;
}
