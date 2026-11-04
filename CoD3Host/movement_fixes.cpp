// Movement as the later games of the engine have it (a PC addition).
//
// Bounces. Call of Duty 4 and World at War let a player who jumps onto a
// slope at the right angle bounce off it. That comes from one line of their
// PM_StepSlideMove: when a jump's step up leaves the player above a slope
// and the trace down finds it, the velocity is projected onto the slope
// with its speed kept (PM_ProjectVelocity), so speed into a steep slope
// becomes speed up it. Call of Duty 3 has the same function, from the same
// source, but clips the velocity there instead (PM_ClipVelocity: the speed
// into the slope is thrown away), and the jump dies on the slope: close to
// a bounce, never one.
//
// The clip at that one call is made the projection, as Call of Duty 4 has
// it (from the reconstructed source, kisakcod's bg_pmove.cpp). A slope
// steeper than Call of Duty 4 lets bounce (a normal under 0.3 up) keeps
// the clip. Every other caller of the clip is left as it is.
//
// Movement is predicted on each player's machine and decided on the host,
// so in multiplayer everyone in the match wants the same setting, or a
// bounce is taken back as it happens. The setting: "CoD4 bounces" in the
// F11 menu (bounces in the ini).

#include "kernel.h"
#include "settings.h"
#include "input.h"

#include <cmath>
#include <cstring>

namespace
{
    // The step down's clip in PM_StepSlideMove: its return address.
    constexpr uint32_t StepDownClipCall = 0x8248FB70;

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

// PM_ClipVelocity(const float* in, const float* normal, float* out)
extern "C" PPC_FUNC(__imp__sub_8246B138);
PPC_FUNC(sub_8246B138)
{
    if (uint32_t(ctx.lr) == StepDownClipCall && Settings::Get().bounces)
    {
        const uint32_t in = ctx.r3.u32, normalAt = ctx.r4.u32, out = ctx.r5.u32;
        const float v[3] = { ReadFloat(base, in), ReadFloat(base, in + 4), ReadFloat(base, in + 8) };
        const float n[3] = { ReadFloat(base, normalAt), ReadFloat(base, normalAt + 4), ReadFloat(base, normalAt + 8) };
        if (n[2] >= 0.3f)
        {
            // PM_ProjectVelocity, as Call of Duty 4 has it.
            const float lengthSq2D = v[0] * v[0] + v[1] * v[1];
            float result[3] = { v[0], v[1], v[2] };
            if (std::fabs(n[2]) >= 0.001f && lengthSq2D != 0.0f)
            {
                const float newZ = -(n[0] * v[0] + n[1] * v[1]) / n[2];
                const float lengthScale = std::sqrt((v[2] * v[2] + lengthSq2D) / (newZ * newZ + lengthSq2D));
                if (lengthScale < 1.0f || newZ < 0.0f || v[2] > 0.0f)
                {
                    result[0] = v[0] * lengthScale;
                    result[1] = v[1] * lengthScale;
                    result[2] = newZ * lengthScale;
                }
            }
            WriteFloat(base, out, result[0]);
            WriteFloat(base, out + 4, result[1]);
            WriteFloat(base, out + 8, result[2]);
            return;
        }
    }
    __imp__sub_8246B138(ctx, base);
}

#ifndef COD3_MP
// The campaign's binoculars coming up while running. The +binoculars
// command (sub_8250CDB0: the player's +1340 bit 1, or 0x82A578F4's bit 2
// to raise them) was reaching the game in the middle of a run (the PC's
// tap-to-sprint keeps the sprint button held for the title, input.cpp),
// and the binoculars then stopped the run. They are not taken out while
// running; standing or walking, they come up as before: the left stick
// clicked and held standing still (the game's own control), or the
// Binoculars out key held (B). (Before, "running" counted the left stick
// held in on its own, which is the binoculars' own button, and tap to
// sprint took a press standing still away from the game: they could not
// be raised at all. Then a click that pushed the stick a third of the way
// forward, as clicking it does, still counted as a run: a run is now the
// stick past half way, sprint_input.h's RunStart.)
extern "C" PPC_FUNC(__imp__sub_8250CDB0);
PPC_FUNC(sub_8250CDB0)
{
    const Input::Pad pad = Input::State();
    static int said = 0;
    if ((said < 40 && said++ < 40))
    {
        printf("binoculars: +binoculars with buttons 0x%04X, left stick %d,%d%s\n", pad.buttons, pad.thumbLX, pad.thumbLY,
               Input::Running() ? ": running, not taken out" : "");
        fflush(stdout);
    }
    if (Input::Running()) return;
    __imp__sub_8250CDB0(ctx, base);
}

// The campaign's binoculars, watched (they did not come up). What the
// title does with the button, read from its code:
//   +binoculars (above) sets 0x82A578F4's bit 2, or holds the breath when the
//   sights are fully up (the player's +0x8c, the aim's fraction, at one).
//   Each frame sub_82488B88 then: with bit 2 held and no weapon being
//   raised (the player's +0x88 not 1), counts the frame's time at
//   0x82A2A2E8 up to 0.4 s and sets bit 0x80; next frame, with 0x80, raises
//   them (sub_82486858: the weapon in slot 7 taken out by sub_82439AB0,
//   bit 4 set). Released, it lowers them if they are up and at the eyes.
// Every change of that word, or of the weapon in hand, is put in the log
// (the first eighty) with what the steps read, to see which one stops.
extern "C" PPC_FUNC(__imp__sub_82488B88);
PPC_FUNC(sub_82488B88)
{
    const uint32_t before = Guest::Read32(base, 0x82A578F4);
    __imp__sub_82488B88(ctx, base);
    static int said = 0;
    static uint32_t lastWord = 0xFFFFFFFFu, lastWeapon = 0xFFFFFFFFu;
    if (said >= 80) return;
    const auto valid = [](uint32_t at) { return at >= 0x40000000u && at < 0x90000000u; };
    const uint32_t word = Guest::Read32(base, 0x82A578F4);
    const uint32_t client = Guest::Read32(base, 0x82A2A1C8);
    const uint32_t entities = Guest::Read32(base, 0x82A2AB40);
    const uint32_t entity = valid(entities) && client < 64 ? Guest::Read32(base, entities + 4 * (client + 1)) : 0;
    const uint32_t ps = valid(entity) ? Guest::Read32(base, entity + 0x210) : 0;
    if (!valid(ps)) return;
    const uint32_t weapon = Guest::Read32(base, ps + 0x84);
    if (word == lastWord && weapon == lastWeapon && before == word) return;
    lastWord = word;
    lastWeapon = weapon;
    const uint32_t list = Guest::Read32(base, 0x82A2A2E0);
    const uint32_t held = valid(list) && weapon < 256 ? Guest::Read32(base, list + 4 * weapon) : 0;
    const uint8_t slot7 = Guest::Read8(base, ps + 0x408 + 7);
    const uint32_t binocular = valid(list) && slot7 != 0 ? Guest::Read32(base, list + 4 * slot7) : 0;
    said++;
    printf("binoculars: word %02X (was %02X), held for %.2f s, weapon %u (kind %u), raising %u, aim %.2f, slot 7 weapon %u (kind %u), flags %08X %08X, stick in %d\n",
        word, before, ReadFloat(base, 0x82A2A2E8), weapon, valid(held) ? Guest::Read32(base, held + 0xb4) : 0xFFFFFFFFu,
        Guest::Read32(base, ps + 0x88), ReadFloat(base, ps + 0x8c), slot7, valid(binocular) ? Guest::Read32(base, binocular + 0xb4) : 0xFFFFFFFFu,
        Guest::Read32(base, ps + 0x2c), Guest::Read32(base, ps + 0xd0), (Input::State().buttons & 0x0040) != 0 ? 1 : 0);
    fflush(stdout);
}
#endif

#ifndef COD3_MP
// The SP reload eligibility handler honors WeaponDef::noPartialReload.
// Enable the Garand's existing partial reload path, retaining native ammo checks.
extern "C" PPC_FUNC(__imp__sub_8246FC78);
PPC_FUNC(sub_8246FC78)
{
    const uint32_t weapon=Guest::Read32(base,0x82A922B0+176);
    if(weapon>=0x10000u && weapon<0xC0000000u-2048){
        const uint32_t name=Guest::Read32(base,weapon+8);
        if(name>=0x10000u && name<0xC0000000u-16 &&
           memcmp(base+name,"m1garand",9)==0 && Guest::Read32(base,weapon+1864)){
            Guest::Write32(base,weapon+1864,0);
            printf("weapons: M1 Garand partial reload enabled\n");
        }
    }
    __imp__sub_8246FC78(ctx,base);
}
#endif
