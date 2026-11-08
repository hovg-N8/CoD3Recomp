// 0.5.595: pointing at the campaign's own menus with the mouse
// (menu_mouse.cpp; ported from FrankHUN88453's CoD3Recomp, ac3b2a52).
// Campaign only: the multiplayer program's menus are another system.
#pragma once

#include <cstdint>

struct PPCContext;

namespace MenuMouse
{
    // A menu of the title's is up (its system updated it lately).
    bool MenuUp();

    // What the left button does where the pointer is: the cross's left
    // (-1) or right (1) pressed steps times (an arrow, an option's value,
    // a chapter of the strip), or A (takes).
    struct Click
    {
        int direction = 0;
        int steps = 0;
        bool takes = false;
    };
    Click ClickUnderPointer();

    // The menu up is the title screen ("PRESS START"), which wants Start.
    bool TitleScreen();

    // 0.5.608 (multiplayer): the team choice (Allies / Axis / Auto-assign) is
    // the menu up.
    bool TeamMenuUp();
    // The spectator's seat drawn under it (spectator.cpp): its box in the
    // menus' 16:9 picture (0..1 across and down), and the pointer on it.
    bool SeatBox(float& x0, float& y0, float& x1, float& y1);
    bool PointerOnSeat();
    // 0.5.610: the team choice put up again (its system and place as seen when
    // the player joined), to join a side after the seat taken in a match;
    // on the game's thread. False when it was never seen.
    bool OpenTeamChoice(uint8_t* base);
    // 0.5.623: SPECTATOR, the team choice's own fourth entry, is the one
    // highlighted (A on it is the spectator's seat, never the game's)
    bool TeamSeatSelected();
    // 0.5.640: the team choice's highlighted row (-1 when it is not up), and
    // a row asked for (2 AXIS, 3 SPECTATOR: the cross's walk past the game's three)
    int TeamSelected();
    void AskTeamRow(int row);
    // each frame of play too (spectator.cpp): the team choice's own entries put
    // back once it is not up
    void Tick(uint8_t* base);
    // 0.5.614: the menu up has entries, arrows or buttons the pointer finds;
    // false for a screen of no entries of its own (the lobby's), which the
    // mouse drives as before (moving steps the highlight, a click is A).
    bool Pointable();
    // 0.5.618: on a lobby list, the cross's up (-1) or down (1) that moves its
    // highlight toward the row under the pointer, 0 when it is there.
    int ListStep();
    // 0.5.637: the menu up now (its address), 0 when none: whether a back
    // press took (input.cpp's right button tries B, then Back).
    uint32_t CurrentMenu();
    // 0.5.643: the menu up has SWITCH TEAM highlighted (the pause menu's)
    bool SwitchTeamSelected();
    // 0.5.644: SWITCH SIDE taken: the pause menu goes, the team choice comes up
    void AskTeamChoiceFromPause();
    // 0.5.646: the menu up takes only presses held as long as a key's (the team choice)
    bool SlowSteps();
    // 0.5.646: the pause menu put up (in the spectator's seat, where the game does not)
    void AskPauseMenu();
    // Every menu the game puts up is taken down (its system's active menu
    // -1, the one it was kept) while hidden; false puts back the ones taken.
    void SetHidden(bool hidden);
    bool Hidden();
    // The game's menu systems' update, each frame: for the spectator, which
    // calls the game's script (spectator.cpp) from it.
    void SetFrameHook(void (*hook)(PPCContext& ctx, uint8_t* base));
}
