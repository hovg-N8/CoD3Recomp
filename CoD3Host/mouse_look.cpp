// Direct aim with the mouse (a PC addition).
//
// The mouse used to stand in for the right stick (input.cpp), and the
// title's stick look is built for thumbs: the turn speeds up the longer the
// stick is held over, it tops out at the stick's full speed, and anything
// small falls into its dead zone. A mouse driven through that feels heavy,
// a quick flick is capped, and a fine adjustment does nothing.
//
// The title's own pad look (CL_GamepadMove, here sub_8250FF48) adds the
// stick's turn to the local client's view angles, scaled by a sensitivity
// the game sets (lower while aiming down the sights). After it has run, the
// mouse's movement since the frame before is added to the same angles the
// same way, a fixed angle per count: what a PC shooter does. The stick and
// a pad still go through the title's own look.
//
// Where the title turns nobody (the flags it checks itself), the mouse's
// movement is dropped; where it limits the turn (a mounted gun), the mouse
// goes back through the stick, which those limits apply to. "Direct mouse
// aim" in the F11 menu's controls.

#include "input.h"
#include "kernel.h"
#include "settings.h"
#ifdef COD3_MP
#include "spectator.h"   // 0.5.614: the free camera's own aim
#endif

#include <cmath>
#include <cstring>

namespace
{
    // The local clients' state (clientActive_t, one a local player), and
    // which of them is playing here. The offsets inside differ between the
    // two executables.
    constexpr uint32_t ClientBase = 0x82A57900;
    constexpr uint32_t LocalClientIndex = 0x82A2A1C8;
    constexpr uint32_t TurnBlocked = 0x82A90A6A;      // a byte per local client
#ifdef COD3_MP
    constexpr uint32_t ClientSize = 6352;
    constexpr uint32_t StateOffset = 1336;
    constexpr uint32_t SensitivityOffset = 1632;
    constexpr uint32_t PitchCapOffset = 1636, YawCapOffset = 1640;
    constexpr uint32_t PitchOffset = 1668, YawOffset = 1672;
#else
    constexpr uint32_t ClientSize = 7280;
    constexpr uint32_t StateOffset = 1304;
    constexpr uint32_t SensitivityOffset = 5696;
    constexpr uint32_t PitchCapOffset = 5700, YawCapOffset = 5704;
    constexpr uint32_t PitchOffset = 5732, YawOffset = 5736;
#endif
    constexpr uint32_t FlagsOffset = 76;

    // Degrees a count at sensitivity 1: Quake's m_yaw of 0.022 at a
    // sensitivity of five, near what the stick gave a slow sweep before.
    constexpr float DegreesPerCount = 0.11f;

    float ReadFloat(uint8_t* base, uint32_t address)
    {
        const uint32_t word = Guest::Read32(base, address);
        float value;
        memcpy(&value, &word, 4);
        return value;
    }
    void WriteFloat(uint8_t* base, uint32_t address, float value)
    {
        uint32_t word;
        memcpy(&word, &value, 4);
        Guest::Write32(base, address, word);
    }
}

#ifdef COD3_MP
void NativeOptionsTracePad(PPCContext& ctx,uint8_t* base);
#endif
extern "C" PPC_FUNC(__imp__sub_8250FF48);
PPC_FUNC(sub_8250FF48)
{
#ifdef COD3_MP
    NativeOptionsTracePad(ctx,base);
#endif
    // The pad's up and down look. The title halves the stick's vertical
    // deflection before anything else (CL_GamepadMove multiplies it by 0.5
    // where the turn's is not), then treats both axes alike: the same
    // speeds (m_pitch and m_yaw, equal), the same sensitivity. So up and
    // down went at half the turn's speed. The angle the title added this
    // frame is scaled by F11's "Vertical look speed" (200%: the turn's
    // speed). Not where a limit on the turn applies (a mounted gun), which
    // the title has already held the change to.
    const uint32_t padIndex = Guest::Read32(base, LocalClientIndex);
    const uint32_t padClient = ClientBase + (padIndex <= 3 ? padIndex : 0) * ClientSize;
    const float pitchBefore = padIndex <= 3 ? ReadFloat(base, padClient + PitchOffset) : 0.0f;

    __imp__sub_8250FF48(ctx, base);

    // The campaign's executable, where the halving was read; multiplayer's
    // pad look is its own build and is left as it is.
#ifndef COD3_MP
    if (padIndex <= 3 && ReadFloat(base, padClient + PitchCapOffset) == 0.0f)
    {
        const float scale = float(Settings::Get().stickVerticalScale) / 100.0f;
        const float after = ReadFloat(base, padClient + PitchOffset);
        const float added = after - pitchBefore;
        if (added != 0.0f && scale != 1.0f && std::fabs(added) < 90.0f)
            WriteFloat(base, padClient + PitchOffset, pitchBefore + added * scale);
    }
#endif

    long dx = 0, dy = 0;
    const uint32_t index = Guest::Read32(base, LocalClientIndex);
    if (index > 3) return;
    const uint32_t client = ClientBase + index * ClientSize;

    // A limit on the turn: the stick's way, which honours it.
    if (ReadFloat(base, client + YawCapOffset) != 0.0f || ReadFloat(base, client + PitchCapOffset) != 0.0f)
    {
        Input::LookByStick();
        return;
    }
    if (!Input::TakeLook(dx, dy) || (dx == 0 && dy == 0)) return;
#ifdef COD3_MP
    // 0.5.614: the free camera turns by its own aim (spectator.cpp): in the
    // spectator's seat the game's spectator script holds the view still
    if (Spectator::TakeMouseLook(float(dx) * DegreesPerCount * Settings::Get().mouseSensitivity,
                                 float(dy) * DegreesPerCount * Settings::Get().mouseSensitivity)) return;
#endif

    // Nobody turns: the title's own reasons, as its pad look has them.
    if ((Guest::Read32(base, client + FlagsOffset) & 0x4000u) != 0) return;
    if (Guest::Read32(base, client + StateOffset) == 5) return;
    if (Guest::Read8(base, TurnBlocked + index) != 0) return;

    const float sensitivity = ReadFloat(base, client + SensitivityOffset);
    if (!(sensitivity > 0.0f) || sensitivity > 10.0f) return;
    const float scale = DegreesPerCount * Settings::Get().mouseSensitivity * sensitivity;
    WriteFloat(base, client + YawOffset, ReadFloat(base, client + YawOffset) - float(dx) * scale);
    WriteFloat(base, client + PitchOffset, ReadFloat(base, client + PitchOffset) + float(dy) * scale);
}
