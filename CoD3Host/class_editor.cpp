// The custom class editor (a PC addition, multiplayer only), on the kit
// screen: R1 there opens it, drawn on the overlay and run from the pad (or
// the keys that stand in for it). Up and down pick a row, left and right
// change it, A uses the class and closes, B closes.
//
// What a custom class gives at one's own spawns (mp_fixes.cpp's kit hooks):
// the base class (whose ability, items and grenade count the kit follows),
// the main weapon, the sidearm, the grenades' kind and how many, and the
// special grenade (0.5.739: the smoke or World at War's signal flare). The game's
// own kit menu still picks a class too; with a base class set, the custom
// one's wins.

#ifdef COD3_MP

#include "class_editor.h"
#include "input.h"
#include "kernel.h"
#include "overlay.h"
#include "settings.h"

#include "imgui.h"

#include <windows.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{
    std::atomic<bool> g_open{ false };
    std::atomic<uint32_t> g_openedAt{ 0 };
    int g_row = 0;
    uint16_t g_lastButtons = 0;

    // XInput's buttons, as Input::Buttons() reports them.
    constexpr uint16_t Up = 0x0001, Down = 0x0002, Left = 0x0004, Right = 0x0008, A = 0x1000, B = 0x2000;

    const char* const Weapons[] = { "CLASS WEAPON", "M1 GARAND", "THOMPSON", "BAR", "SPRINGFIELD (SCOPED)", ".30 CAL MG", "BAZOOKA",
                                    "TRENCH GUN", "KAR98K", "MP40", "STG 44", "KAR98K (SCOPED)", "MG34", "PANZERSCHRECK",
                                    "LEE-ENFIELD", "BREN", "STEN", "FG42", "FG42 (SCOPED)", "GEWEHR 43", "MG42", "LEE-ENFIELD (SCOPED)", "WEBLEY", "M2 FLAMETHROWER", "MOSIN-NAGANT (SCOPED)", "COLT .45 DUAL" };   // 0.5.653, 0.5.681, 0.5.726
    const char* const Bases[] = { "AS PICKED", "HEAVY ASSAULT", "LIGHT ASSAULT", "RIFLEMAN", "MEDIC", "SUPPORT", "ANTI-ARMOR", "SCOUT" };
    const char* const Sidearms[] = { "SIDE'S OWN", "COLT .45", "P38", "M1 GARAND", "THOMPSON", "BAR", "SPRINGFIELD (SCOPED)",
                                     ".30 CAL MG", "BAZOOKA", "TRENCH GUN", "KAR98K", "MP40", "STG 44", "KAR98K (SCOPED)", "MG34",
                                     "PANZERSCHRECK", "LEE-ENFIELD", "BREN", "STEN", "FG42", "FG42 (SCOPED)", "GEWEHR 43", "MG42", "LEE-ENFIELD (SCOPED)", "WEBLEY", "M2 FLAMETHROWER", "MOSIN-NAGANT (SCOPED)", "COLT .45 DUAL" };   // 0.5.653, 0.5.681, 0.5.726
    const char* const GrenadeTypes[] = { "SIDE'S OWN", "FRAG (US)", "STICK (GERMAN)", "MILLS BOMB (BRITISH)", "STICKY BOMB (BRITISH)" };
    const char* const GrenadeCounts[] = { "USUAL", "1", "2", "3", "4" };
    const char* const Specials[] = { "SMOKE", "SIGNAL FLARE (WAW)" };   // 0.5.739

    enum Row { RowClass, RowBase, RowPrimary, RowSidearm, RowGrenadeType, RowGrenades, RowSpecial, RowCount };

    int Wrap(int value, int count) { return (value % count + count) % count; }

    void Change(int step)
    {
        Settings::Values v = Settings::Get();
        const int slot = v.activeClass - 1;
        if (g_row == RowClass) v.activeClass = Wrap(v.activeClass + step, Settings::Values::CustomClasses + 1);
        else if (slot >= 0)
        {
            if (g_row == RowBase) v.classBase[slot] = Wrap(v.classBase[slot] + step, 8);
            else if (g_row == RowPrimary) v.classPrimary[slot] = Wrap(v.classPrimary[slot] + step, int(sizeof(Weapons) / sizeof(Weapons[0])));
            else if (g_row == RowSidearm) v.classSidearm[slot] = Wrap(v.classSidearm[slot] + step, int(sizeof(Sidearms) / sizeof(Sidearms[0])));
            else if (g_row == RowGrenadeType) v.classGrenadeType[slot] = Wrap(v.classGrenadeType[slot] + step, 5);
            else if (g_row == RowGrenades) v.classGrenades[slot] = Wrap(v.classGrenades[slot] + step, 5);
            else if (g_row == RowSpecial) v.classSpecial[slot] = Wrap(v.classSpecial[slot] + step, 2);
        }
        Settings::Set(v);
    }

    bool Active() { return g_open.load(); }

    void Draw(int width, int height)
    {
        // The pad, a press at a time.
        const uint16_t buttons = Input::Buttons();
        const uint16_t pressed = buttons & ~g_lastButtons;
        g_lastButtons = buttons;
        // The press that opened it is not also a choice.
        if (GetTickCount() - g_openedAt.load() > 250)
        {
            if (pressed & Up) g_row = Wrap(g_row - 1, RowCount);
            if (pressed & Down) g_row = Wrap(g_row + 1, RowCount);
            if (pressed & Left) Change(-1);
            if (pressed & Right) Change(1);
            if (pressed & (A | B)) { g_open = false; return; }
        }

        const Settings::Values v = Settings::Get();
        const int slot = v.activeClass - 1;
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        ImFont* font = ImGui::GetFont();
        const float scale = height / 1080.0f;
        const float size = 30.0f * scale, rowHeight = 46.0f * scale;
        const float panelW = 760.0f * scale, panelH = rowHeight * (RowCount + 2.6f);
        const ImVec2 at((width - panelW) * 0.5f, (height - panelH) * 0.5f);
        draw->AddRectFilled(at, ImVec2(at.x + panelW, at.y + panelH), IM_COL32(12, 12, 10, 230), 8 * scale);
        draw->AddRect(at, ImVec2(at.x + panelW, at.y + panelH), IM_COL32(190, 160, 90, 255), 8 * scale, 0, 2 * scale);
        draw->AddText(font, size * 1.1f, ImVec2(at.x + 24 * scale, at.y + 16 * scale), IM_COL32(230, 200, 120, 255), "CREATE A CLASS");

        std::string values[RowCount];
        values[RowClass] = slot < 0 ? std::string("NONE (THE GAME'S OWN KIT)") : std::string(v.className[slot]);
        if (slot >= 0)
        {
            values[RowBase] = Bases[v.classBase[slot]];
            values[RowPrimary] = Weapons[v.classPrimary[slot]];
            values[RowSidearm] = Sidearms[v.classSidearm[slot] >= 0 && v.classSidearm[slot] < int(sizeof(Sidearms) / sizeof(Sidearms[0])) ? v.classSidearm[slot] : 0];
            values[RowGrenadeType] = GrenadeTypes[v.classGrenadeType[slot]];
            values[RowGrenades] = GrenadeCounts[v.classGrenades[slot]];
            values[RowSpecial] = Specials[v.classSpecial[slot] == 1 ? 1 : 0];
        }
        static const char* const labels[RowCount] = { "CLASS", "BASE CLASS", "MAIN WEAPON", "SECOND WEAPON", "GRENADE", "GRENADES", "SPECIAL" };
        for (int row = 0; row < RowCount; row++)
        {
            const float y = at.y + rowHeight * (row + 1.3f);
            const bool on = row == g_row;
            if (on)
                draw->AddRectFilled(ImVec2(at.x + 12 * scale, y - 6 * scale), ImVec2(at.x + panelW - 12 * scale, y + rowHeight - 10 * scale),
                                    IM_COL32(90, 75, 40, 200), 4 * scale);
            const ImU32 colour = (row > RowClass && slot < 0) ? IM_COL32(110, 110, 110, 255) : IM_COL32(240, 235, 220, 255);
            draw->AddText(font, size, ImVec2(at.x + 28 * scale, y), colour, labels[row]);
            const std::string shown = row > RowClass && slot < 0 ? std::string("-") : "<  " + values[row] + "  >";
            draw->AddText(font, size, ImVec2(at.x + 300 * scale, y), colour, shown.c_str());
        }
        draw->AddText(font, size * 0.7f, ImVec2(at.x + 28 * scale, at.y + panelH - 40 * scale), IM_COL32(200, 190, 160, 255),
                      "UP/DOWN: ROW    LEFT/RIGHT: CHANGE    A: USE    B: CLOSE    (NAMES: F11)");
    }
}

void ClassEditor::Open()
{
    static bool registered = false;
    if (!registered) { registered = true; Overlay::SetMenuHud(Active, Draw); }
    g_lastButtons = Input::Buttons();
    g_openedAt = GetTickCount();
    g_open = true;
    printf("class editor: open\n");
    fflush(stdout);
}

bool ClassEditor::IsOpen() { return g_open.load(); }

#endif
