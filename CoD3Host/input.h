#pragma once

// The controller.
//
// The title polls for one about five hundred thousand times a second while it
// waits for the player, so this has to be cheap to ask and has to give the same
// answer to every caller within a frame.
//
// A real pad is used when one is plugged in. When there is not, the keyboard
// stands in, so the title can be driven without hardware.

#include <cstdint>
#include <string>

namespace Input
{
    // A pad, in the shape the console reports one: a button mask, two analogue
    // triggers and two sticks. The keyboard and mouse fill the same structure,
    // so nothing downstream needs to know which was used.
    struct Pad
    {
        uint16_t buttons;
        uint8_t leftTrigger;
        uint8_t rightTrigger;
        int16_t thumbLX;
        int16_t thumbLY;
        int16_t thumbRX;
        int16_t thumbRY;
    };

    Pad State();
    void MainMenuSeen();
    bool ObjectivesVisible(); // campaign keyboard objectives toggle
    // Experimental stationary L3 save/R3 return, sampled before sprint filtering.
    void SetTacticalPointInput(bool active, bool canReturn);
    bool TacticalPointSaveHeld();
    bool TacticalPointReturnHeld();

    // The Xbox button mask the pad is currently reporting.
    uint16_t Buttons();
    // 0.5.231: a pad button pressed from code (the photo tour's way through
    // the team and class menus): "a", "b", "start", ... for two of the
    // title's reads of the pad, as COD3_PAD's presses.
    void PressButton(const char* name);

    // Whether the mouse is being held by the window and turned into the right
    // stick. Clicking in the window takes it; Escape gives it back.
    bool MouseHeld();
    // Window management temporarily owns the pointer; no clip/recenter or
    // automatic capture until the move, resize or mode change has finished.
    void SuspendMouseCapture(bool suspended);

    // Changes only when the buttons do. Titles use it to tell a held button
    // from a new press without comparing states themselves.
    uint32_t PacketNumber();

    // Which keys stand in for which buttons, for the status view to show.
    const char* KeyboardHelp();

    // Which controllers Windows reports, for the settings menu: "none found"
    // or the XInput slots in use and what they are pressing.
    std::string ControllerStatus();

    // The title's vibration, sent to every connected pad (0 .. 65535 each
    // motor, the low frequency one on the left).
    void SetRumble(uint16_t left, uint16_t right);

    // Direct aim: the mouse's movement since the last call, in its counts,
    // for the title's frame to turn the view by; false when the setting is
    // off. While it is called the mouse no longer moves the right stick.
    bool TakeLook(long& dx, long& dy);
    // The title is turning by the stick alone for now (a mounted gun's
    // limits): the mouse goes back to the stick until TakeLook is next
    // called.
    void LookByStick();

    // Multiplayer's binoculars out (mp_fixes.cpp): aim held for the title
    // the whole time (at the eyes), the left trigger's presses counted for
    // the zoom instead (TakeAimPresses: how many since the last call), and
    // the right trigger let through only when mayFire (an artillery strike
    // ready).
    void SetBinoculars(bool out, bool mayFire);
    int TakeAimPresses();
    // Presses of Start, d-pad up and d-pad down since the last call, counted
    // as the pad is read (so a short one between two game frames counts).
    void TakePresses(int& start, int& up, int& down);
    // Presses of d-pad right since the last call (the killstreaks' call, 0.5.82).
    int TakeRightPresses();
    // 0.5.285: the mortar killstreak set up (mp_fixes.cpp): the player held
    // in place for the title (left stick, A, B, X, Y, the sticks' clicks,
    // the shoulders and both triggers kept from it), the right trigger's
    // presses counted instead (TakeMortarFires: how many since the last call).
    void SetMortar(bool on);
    // 0.5.300: the right trigger passed on to the title too while at the
    // mortar (the campaign mortar interaction reads it to load a round).
    void SetMortarPassFire(bool pass);
    void PressCrouch();   // 0.5.307: one tap of crouch (B) for a few reads
    int TakeMortarFires();
    // MP gameplay consumes each Y/wheel edge once through the native weapon
    // command; native menu/vehicle Y remains available when the bridge is off.
    unsigned TakeWeaponCyclePresses();
    void SetWeaponCycleEnabled(bool enabled);
    uint16_t NativeButtons(uint16_t buttons,bool paused);


    // Whether the player is running: the left stick (or the keys) pushed most
    // of the way, or sprint held (the tap-to-sprint's hold too).
    bool Running();
    // the sprint button held (or latched by tap-to-sprint) while going forward, not aiming (0.5.237)
    bool SprintHeld();
    // Multiplayer scoped L3: native hold breath, never a latched sprint.
    void SetScopedBreath(bool enabled);
}
