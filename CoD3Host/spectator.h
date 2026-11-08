// 0.5.490: spectating (a PC addition, multiplayer): a free camera that flies
// around the map while the match runs, and following another player (bot or
// human) from his own eyes or from behind him, cycling between players.
//
// The game has no spectator team to put a player on in a running match, and
// its noclip needs a devmap. So this is done on this machine's picture only:
// your player stands where he is (his moves and shots held back), and
// - the free camera is put over cgame's view each frame (the killcam's first,
//   camera-only way, 0.5.323);
// - following from his eyes hands cgame the followed player's live state, as
//   the killcam replay does with a recorded one (0.5.327: his gun, aim and
//   shots drawn by the game itself, his body hidden as a followed player's);
// - following from behind puts the camera behind and above his eyes.
// F6 on and off (F11 > Multiplayer > Spectator too); weapon switch (Y) the
// next mode; fire / aim (RT / LT) the next / previous player.
#pragma once

#include "input.h"

#include <cstdint>
#include <string>

struct PPCContext;

namespace Spectator
{
    enum class Mode { Off = 0, FreeCam = 1, FollowEyes = 2, FollowBehind = 3 };

    Mode Current();
    void Set(Mode mode);
    void Toggle();          // F6: off, or back to the last mode used
    void NextMode();        // free camera -> his eyes -> behind him -> free camera
    void NextPlayer(int step);   // +1 / -1
    bool Active();
    // What the menu shows: the mode and whom it follows.
    std::string Status();
    float& FreeCamSpeed();  // units a second
    // 0.5.593: his eyes smoothed between server frames (on), or his record's own
    bool SmoothEyes();
    void SetSmoothEyes(bool on);
    // 0.5.602: while spectating, your player put in the game's own spectator
    // state (out of the match: not drawn, not hit, not hunted), back where he
    // was after (the host only; experimental, off by default)
    bool LeaveMatch();
    void SetLeaveMatch(bool on);

    // 0.5.608: the spectator's seat: chosen on the team choice (beside
    // Allies / Axis / Auto-assign), the player stays the game's own
    // spectator (no body in the match), the game's menus and HUD go, and the
    // camera is ours; leaving it puts the team choice back.
    void TakeSeat();
    void LeaveSeat();
    bool PureActive();
    // F6 / Back + Y: leave the seat, take it (the team choice up), or the
    // spectating of before (your player stays in the match)
    void Key();

    // 0.5.614: the mouse's turn (degrees) for the free camera's own aim;
    // true when the free camera took it (the game's view is left alone).
    bool TakeMouseLook(float yawDegrees, float pitchDegrees);

    // input.cpp, on every pad read: the spectator's own buttons taken, the
    // ones that would move or fire your player held back.
    void FilterPad(Input::Pad& pad);

    // bots.cpp's cgame hooks (only outside the killcam's own swap):
    // CG_DrawActiveFrame / the scene / CG_AddPacketEntities, around the call
    bool SwapIn(uint8_t* base);
    void SwapOut(uint8_t* base);
    // a snapshot cgame has just read (82451ED0's result)
    void PatchSnapshot(uint8_t* base, uint32_t snap);
    // after cgame's player state (82464D38): the view weapon
    void PatchWeapon(uint8_t* base);
    // before the view's axes are made (82463150): the free and behind cameras
    void ApplyCamera(uint8_t* base, uint32_t camera);
}
