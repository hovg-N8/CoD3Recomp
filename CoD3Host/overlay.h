// The settings menu (F11), the frame counter and the screenshot key (F12),
// drawn over the picture with Dear ImGui on the window's thread.
#pragma once

#include <cstdint>
#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11RenderTargetView;
struct ID3D11Texture2D;

namespace Overlay
{
    // On the window's thread, once the swap chain exists.
    void Initialize(void* hwnd, ID3D11Device* device, ID3D11DeviceContext* context);

    // Before the window's Present: the menu and the counter into the back
    // buffer, and the screenshot of it when one was asked for (with a
    // backTexture). True when anything was drawn. The HDR mode draws into a
    // layer of its own (render_d3d11.cpp) and takes the screenshot itself.
    bool Render(ID3D11RenderTargetView* back, ID3D11Texture2D* backTexture, int width, int height);

    // The HDR mode's screenshot: whether F12 asked for one (the ask is
    // taken), and a PNG of the scRGB back buffer, scRgbWhite (the game's
    // white over 80 nits) at the top.
    bool TakeScreenshotRequest();
    void SaveScreenshot(ID3D11Texture2D* back, int width, int height, float scRgbWhite);

    // The window procedure hands every message here first; true when it
    // was the menu's.
    bool HandleMessage(void* hwnd, uint32_t message, uint64_t wParam, int64_t lParam);

    // The settings menu or the console is up: the keys and the mouse are
    // theirs, not the title's.
    bool IsOpen();
    void Toggle();
    void ToggleConsole();

    // A line for the console's log, from anywhere.
    void ConsolePrint(const std::string& line);
    // A message across the top of the picture for a while (a killstreak's).
    void Notice(const std::string& text, int milliseconds = 3000);
    void RequestScreenshot();

    // Frames the title finished, for the counter; called at every swap.
    void NoteFrame();

    // What the front end drew this frame (title_trace.cpp): the main
    // menu, or its options page. The settings menu opens itself when the
    // options page follows the main menu, which is the OPTIONS entry.
    enum class FrontEndText { MainMenu = 1, OptionsMenu = 2 };
    void NoteFrontEndText(FrontEndText text);
    // The main menu was drawn within the last second (co-op's lobby: back from a mission).
    bool MainMenuShown();

    // A game's own heads-up display drawn over the picture (Domination's
    // flags, dom.cpp): while active() says so, draw(width, height) each frame
    // between ImGui's new frame and its render.
    void SetHud(bool (*active)(), void (*draw)(int width, int height));
    // A second one, over the first (the custom class editor, class_editor.cpp).
    void SetMenuHud(bool (*active)(), void (*draw)(int width, int height));
    // A third, the killstreaks' box (mp_fixes.cpp, 0.5.82).
    void SetStreakHud(bool (*active)(), void (*draw)(int width, int height));
    // A fourth, the spectator's seat on the team choice (spectator.cpp, 0.5.608).
    void SetSeatHud(bool (*active)(), void (*draw)(int width, int height));
    // A fifth, the pre-match countdown (bots.cpp, 0.5.641).
    void SetMatchHud(bool (*active)(), void (*draw)(int width, int height));
    // A sixth, drawn first (under the others): the weapon's own picture over
    // the view (the flamethrower's flame, flamethrower.cpp, 0.5.681).
    void SetWeaponHud(bool (*active)(), void (*draw)(int width, int height));
    // 0.5.739: a seventh, drawn just after it: World at War's signal flare's
    // glow where it burns and its blinding over the view (flare.cpp)
    void SetFlareHud(bool (*active)(), void (*draw)(int width, int height));
    // 0.5.718: the weapon's view mesh (its vertices as drawn: xyz, the mesh's
    // own space; triangles): the weapon picture above is drawn only where this
    // mesh is not, put on the screen by its own draw's transforms
    // (Render::TrackedTransform): the gun over its flame
    void SetWeaponMaskMesh(const float* xyz, size_t vertices, const uint16_t* indices, size_t count);
    // 0.5.681: a PNG file as a texture for ImGui's draws (an
    // ID3D11ShaderResourceView*, kept for good; null if it cannot be read).
    void* ImageTexture(const std::string& pngPath, int* width = nullptr, int* height = nullptr);
    // Co-op v40's lobby (coop_lobby.cpp, the campaign; merged 0.5.622): while
    // available() says so, F2 opens and closes it, and while it is open it has
    // the keys and the mouse as the settings do; draw(width, height, open) each
    // frame (closed, it may draw a hint).
    void SetLobby(bool (*available)(), void (*draw)(int width, int height, bool open));
    bool LobbyOpen();
    void SetLobbyOpen(bool open);
}
