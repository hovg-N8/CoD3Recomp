// The button prompts for the keyboard and mouse (0.5.478, a PC addition,
// from FrankHUN88453's CoD3Recomp ac3b2a52, "the prompts as keys").
//
// The title writes its prompts with tokens (~cross, ~menu_back, ~square,
// ~updown ...) that its front end's text turns into a character of the
// button icon font (i_button_icons_xe), drawn in that font. The characters
// are '"' A, '%' B, '#' X, '$' Y, '!' RT, '&' LT, '(' RB, '\'' LB, ')' ','
// '+' '*' the cross up, down, left and right, '-' Start, '1' Back, '/' up
// and down, '.' left and right, '0' all four, '2' and '3' the sticks.
//
// The front end's manager (0x82A8DF2C) holds its fonts, the icon font at +8:
// +4 its image (+20 the texture, whose fetch constant's second word, +32,
// holds the address), +36 the first character, +40 how many, +44 each
// one's texture rectangle (u0 v0 u1 v1). While the keyboard and mouse are
// what the player used last (Input::PadPrompts), the renderer draws that
// texture as key caps naming the keys that press each button here
// (render_prompts.cpp): every prompt then names keys, at once, even in a
// text already laid out; a press on the pad puts the pad's icons back.
//
// The labels follow input.cpp's own mapping (the keys the settings give each
// button; in a menu the mouse's buttons, which take and go back there). Left
// out from his: the campaign's sentences named by hash, and the main menu's
// Marketplace as Quit (his mouse menus).

#include "pc_prompts.h"
#include "input.h"
#include "kernel.h"
#include "render.h"
#include "settings.h"

#include <Windows.h>

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
    constexpr uint32_t FrontEndManager = 0x82A8DF2C;
    constexpr uint32_t ManagerIconFont = 8;
    constexpr uint32_t FontImage = 4, FontFirst = 36, FontCount = 40, FontRectangles = 44;
    constexpr uint32_t ImageTexture = 20, TextureFetchAddress = 32;

    float ReadFloat(uint8_t* base, uint32_t address)
    {
        const uint32_t v = Guest::Read32(base, address);
        float f;
        memcpy(&f, &v, 4);
        return f;
    }

    // A key as a prompt names it: short and in capitals.
    std::string KeyLabel(int key)
    {
        switch (key)
        {
        case 0: return "";
        case VK_LBUTTON: return "LMB";
        case VK_RBUTTON: return "RMB";
        case VK_MBUTTON: return "MMB";
        case VK_XBUTTON1: return "M4";
        case VK_XBUTTON2: return "M5";
        case VK_ESCAPE: return "ESC";
        case VK_RETURN: return "ENTER";
        case VK_SPACE: return "SPACE";
        case VK_TAB: return "TAB";
        case VK_LSHIFT: case VK_RSHIFT: case VK_SHIFT: return "SHIFT";
        case VK_LCONTROL: case VK_RCONTROL: case VK_CONTROL: return "CTRL";
        case VK_LMENU: case VK_RMENU: case VK_MENU: return "ALT";
        case VK_BACK: return "BKSP";
        case VK_CAPITAL: return "CAPS";
        case VK_UP: return "\xE2\x86\x91";
        case VK_DOWN: return "\xE2\x86\x93";
        case VK_LEFT: return "\xE2\x86\x90";
        case VK_RIGHT: return "\xE2\x86\x92";
        default: break;
        }
        std::string name = Settings::KeyName(key);
        for (char& c : name) c = char(std::toupper(static_cast<unsigned char>(c)));
        return name;
    }

    // A bound key's label, or the fallback where the action has none.
    std::string Bound(const Settings::Values& s, Settings::Action action, int fallback)
    {
        return KeyLabel(s.keys[action] != 0 ? s.keys[action] : fallback);
    }

    // What each icon of the font is on the keyboard and mouse, as input.cpp
    // presses it: jump A, crouch B, use X, weapon Y, fire and aim the
    // triggers, grenade and smoke the bumpers, the arrows the cross, Enter
    // Start; in a menu the left button takes (A), the right goes back.
    std::string Label(char icon, bool menu, const Settings::Values& s)
    {
        switch (icon)
        {
        case '"': return menu ? "LMB" : Bound(s, Settings::Jump, VK_SPACE);
        case '%': return menu ? "RMB" : Bound(s, Settings::Crouch, 'C');
        case '#': return Bound(s, Settings::Use, 'E');
        case '$': return Bound(s, Settings::Weapon, 'F');
        case '!': return Bound(s, Settings::Fire, VK_LBUTTON);
        case '&': return Bound(s, Settings::Aim, VK_RBUTTON);
        case '(': return Bound(s, Settings::Grenade, 'G');
        case '\'': return Bound(s, Settings::Smoke, '4');
        case ')': return KeyLabel(VK_UP);
        case ',': return KeyLabel(VK_DOWN);
        case '+': return KeyLabel(VK_LEFT);
        case '*': return KeyLabel(VK_RIGHT);
        case '-': return menu ? "ENTER" : Bound(s, Settings::Pause, VK_ESCAPE);
        case '1': return menu ? "RMB" : Bound(s, Settings::Objectives, VK_TAB);
        case '/': return "\xE2\x86\x91\xE2\x86\x93";
        case '.': return "\xE2\x86\x90\xE2\x86\x92";
        case '0': return "\xE2\x86\x91\xE2\x86\x93\xE2\x86\x90\xE2\x86\x92";
        case '2':
        {
            const std::string f = KeyLabel(s.keys[Settings::Forward]), l = KeyLabel(s.keys[Settings::Left]);
            const std::string b = KeyLabel(s.keys[Settings::Backward]), r = KeyLabel(s.keys[Settings::Right]);
            if (f.size() == 1 && l.size() == 1 && b.size() == 1 && r.size() == 1) return f + l + b + r;
            return "MOVE";
        }
        case '3': return "MOUSE";
        default: return "";
        }
    }
}

void PcPrompts::Frame(uint8_t* base)
{
    static uint64_t last = 0;
    const uint64_t now = GetTickCount64();
    if (now - last < 100) return;
    last = now;

    const uint32_t font = Guest::Read32(base, FrontEndManager + ManagerIconFont);
    const uint32_t first = font != 0 ? Guest::Read32(base, font + FontFirst) : 0;
    const uint32_t count = font != 0 ? Guest::Read32(base, font + FontCount) : 0;
    const uint32_t rectangles = font != 0 ? Guest::Read32(base, font + FontRectangles) : 0;
    const uint32_t image = font != 0 ? Guest::Read32(base, font + FontImage) : 0;
    const uint32_t texture = image != 0 ? Guest::Read32(base, image + ImageTexture) : 0;
    const uint32_t address = texture != 0 ? Guest::Read32(base, texture + TextureFetchAddress) & 0xFFFFF000u : 0;
    // The icon font: a few dozen characters from the space on.
    const bool fontOk = address != 0 && rectangles != 0 && first == 0x20 && count >= 20 && count <= 64;
    static bool toldFont = false;
    if (!toldFont && font != 0)
    {
        toldFont = true;
        printf("prompts: the button icon font %08X: first %u, %u characters, texture %08X%s\n", font, first, count, address, fontOk ? "" : " (not as expected: the pad's icons stay)");
        fflush(stdout);
    }
    // 0.5.635: which the prompts are, each time it changes (multiplayer: no
    // log ever said "drawn as keys"; this says whether keys were asked)
    {
        static int lastState = -1, told = 0;
        const int state = !fontOk ? 0 : Input::PadPrompts() ? 1 : 2;
        if (state != lastState && told < 20)
        {
            lastState = state;
            told++;
            printf("prompts: now %s (setting %d, texture %08X)\n", state == 0 ? "the pad's icons: no icon font" : state == 1 ? "the pad's icons: the pad used last or chosen" : "keys asked for",
                   Settings::Get().buttonPrompts, address);
            fflush(stdout);
        }
    }
    if (Input::PadPrompts() || !fontOk)
    {
        Render::SetPromptKeys(0, nullptr, 0);
        return;
    }

    const Settings::Values settings = Settings::Get();
    const bool menu = Input::MenuMode();
    static const char icons[] = "\"%#$!&('),+*-1/.023";
    Render::PromptCell cells[sizeof(icons)] = {};
    uint32_t n = 0;
    for (const char* icon = icons; *icon != 0; icon++)
    {
        const uint32_t index = uint32_t(uint8_t(*icon)) - first;
        if (index >= count) continue;
        Render::PromptCell& cell = cells[n];
        cell.u0 = ReadFloat(base, rectangles + index * 16);
        cell.v0 = ReadFloat(base, rectangles + index * 16 + 4);
        cell.u1 = ReadFloat(base, rectangles + index * 16 + 8);
        cell.v1 = ReadFloat(base, rectangles + index * 16 + 12);
        if (!(cell.u1 > cell.u0 && cell.v1 > cell.v0 && cell.u0 >= 0.0f && cell.v1 <= 1.0f)) continue;
        const std::string label = Label(*icon, menu, settings);
        strncpy(cell.label, label.c_str(), sizeof(cell.label) - 1);
        n++;
    }
    Render::SetPromptKeys(address, cells, n);
}
