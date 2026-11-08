// 0.5.490: spectating (spectator.h says what and why).

#ifdef COD3_MP

#include "spectator.h"
#include "kernel.h"
#include "overlay.h"
#include "menu_mouse.h"   // 0.5.608: the game's menus taken down for the spectator's seat
#include "render.h"       // 0.5.614: the seat test's frame of draws
#include "render_commands.h"   // 0.5.616: the blur behind the team choice left out in the seat

#include "imgui.h"

#include <windows.h>
#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int BotsNumberOfClient(int client);   // bots.cpp: "Bot N", 0 for a human
bool BotsThisMachineHostsBots(uint8_t* base);   // bots.cpp: this machine runs the match (0.5.604: LobbyIsHost was false in a match not started from the lobby)
bool KillcamReplayingNow();           // bots.cpp

namespace
{
    constexpr uint32_t EntityList = 0x82BCC7FCu;    // the players' entities, by client number + 1
    constexpr uint32_t LocalClient = 0x82BCBE9Cu;
    constexpr uint32_t Cg = 0x82C7D490u, CgSize = 6368u;
    constexpr uint32_t WeaponList = 0x82BCBF9Cu;
    constexpr uint32_t PsSize = 1500, RecSize = 2808;   // as the killcam keeps them (bots.cpp)
    constexpr int MaxClients = 32;
    constexpr uint16_t PadA = 0x1000, PadB = 0x2000, PadX = 0x4000, PadY = 0x8000, PadBack = 0x0020;
    constexpr uint16_t PadLeftThumb = 0x0040, PadRightThumb = 0x0080, PadLeftShoulder = 0x0100, PadRightShoulder = 0x0200;

    std::atomic<int> g_mode{0}, g_lastMode{int(Spectator::Mode::FreeCam)};
    std::atomic<int> g_target{-1};         // the followed client
    std::atomic<int> g_steps{0};           // next / previous player asked, not yet done
    std::atomic<bool> g_targetAlive{false}, g_ownAlive{true};
    // the free camera's sticks and buttons, from the pad reads
    std::atomic<float> g_moveX{0.0f}, g_moveY{0.0f}, g_moveZ{0.0f};
    // 0.5.614: the free camera's own aim: the right stick (-1..1) and the
    // mouse's turn not yet used (degrees), from the input's threads
    std::atomic<float> g_lookX{0.0f}, g_lookY{0.0f}, g_mouseYaw{0.0f}, g_mousePitch{0.0f};
    bool g_freeAimInit = false;
    float g_freeYaw = 0.0f, g_freePitch = 0.0f;
    void AddTo(std::atomic<float>& a, float d) { float v = a.load(); while (!a.compare_exchange_weak(v, v + d)) {} }
    std::atomic<bool> g_fast{false};
    float g_speed = 500.0f;
    std::atomic<bool> g_smooth{true};      // 0.5.593: his eyes smoothed between server frames
    std::atomic<bool> g_pure{false};       // 0.5.608: in the spectator's seat (no player in the match)

    bool Valid(uint32_t p) { return p >= 0x40000000u && p < 0xA0000000u; }
    float ReadF(uint8_t* base, uint32_t at) { const uint32_t w = Guest::Read32(base, at); float f; memcpy(&f, &w, 4); return f; }
    void WriteF(uint8_t* base, uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); }
    float LerpAngle(float x, float y, float f) { const float d = fmodf(y - x + 540.0f, 360.0f) - 180.0f; return x + d * f; }

    uint32_t Local(uint8_t* base) { return Guest::Read32(base, LocalClient); }
    // client c's record whatever his state (0.5.602: ours while it is in the
    // game's spectator state too)
    uint32_t RawRecordOf(uint8_t* base, int c)
    {
        if (c < 0 || c >= MaxClients) return 0;
        const uint32_t entities = Guest::Read32(base, EntityList);
        if (!Valid(entities)) return 0;
        const uint32_t e = Guest::Read32(base, entities + 4 * uint32_t(c + 1));
        if (!Valid(e)) return 0;
        const uint32_t r = Guest::Read32(base, e + 512);
        return Valid(r) ? r : 0;
    }
    // client c's record (his playerstate and the rest), 0 when he is not in the game
    uint32_t RecordOf(uint8_t* base, int c)
    {
        if (c < 0 || c >= MaxClients) return 0;
        const uint32_t entities = Guest::Read32(base, EntityList);
        if (!Valid(entities)) return 0;
        const uint32_t e = Guest::Read32(base, entities + 4 * uint32_t(c + 1));
        if (!Valid(e)) return 0;
        const uint32_t r = Guest::Read32(base, e + 512);
        if (!Valid(r)) return 0;
        const uint32_t state = Guest::Read32(base, r + 1924);   // 3 playing, 4 dead
        return (state == 3 || state == 4) ? r : 0;
    }
    bool Alive(uint8_t* base, uint32_t r) { return r && int32_t(Guest::Read32(base, r + 304)) > 0 && Guest::Read32(base, r + 1924) == 3; }

    // The next player to follow from 'from' in direction step: an alive one
    // first, else anyone in the game; -1 for nobody.
    int Pick(uint8_t* base, int from, int step)
    {
        const int me = int(Local(base));
        int anyone = -1;
        for (int k = 1; k <= MaxClients; k++)
        {
            const int c = ((from < 0 ? (step > 0 ? -1 : 0) : from) + step * k + MaxClients * 4) % MaxClients;
            if (c == me) continue;
            const uint32_t r = RecordOf(base, c);
            if (!r) continue;
            if (Alive(base, r)) return c;
            if (anyone < 0) anyone = c;
        }
        return anyone;
    }

    std::string NameOf(int c)
    {
        char t[48];
        const int n = c >= 0 ? BotsNumberOfClient(c) : 0;
        if (n > 0) snprintf(t, sizeof t, "Bot %d", n); else snprintf(t, sizeof t, "Player %d", c + 1);
        return t;
    }

    // The followed player's place and aim, between his last two server
    // frames (20 a second), so the view moves smoothly at the picture's rate.
    struct Sample { float o[3]; float pitch, yaw; uint64_t at; };
    Sample g_prev{}, g_cur{};
    int g_sampled = -1;
    void Note(uint8_t* base, int c, uint32_t r)
    {
        Sample s{ { ReadF(base, r), ReadF(base, r + 4), ReadF(base, r + 8) }, ReadF(base, r + 204), ReadF(base, r + 208), GetTickCount64() };
        if (c != g_sampled) { g_sampled = c; g_prev = g_cur = s; return; }
        if (s.o[0] != g_cur.o[0] || s.o[1] != g_cur.o[1] || s.o[2] != g_cur.o[2] || s.pitch != g_cur.pitch || s.yaw != g_cur.yaw)
        {
            const float jump = fabsf(s.o[0] - g_cur.o[0]) + fabsf(s.o[1] - g_cur.o[1]) + fabsf(s.o[2] - g_cur.o[2]);
            g_prev = jump > 300.0f ? s : g_cur;   // a respawn: no glide across the map
            g_cur = s;
        }
    }
    Sample Now()
    {
        const uint64_t t = GetTickCount64();
        const float span = float(std::max<uint64_t>(g_cur.at - g_prev.at, 16));
        const float f = std::clamp(float(t - g_cur.at) / span, 0.0f, 1.0f);
        Sample s{};
        for (int i = 0; i < 3; i++) s.o[i] = g_prev.o[i] + (g_cur.o[i] - g_prev.o[i]) * f;
        s.pitch = LerpAngle(g_prev.pitch, g_cur.pitch, f);
        s.yaw = LerpAngle(g_prev.yaw, g_cur.yaw, f);
        return s;
    }

    // the swap (FollowEyes): our record holds his for cgame's scopes
    bool g_swapped = false;
    uint32_t g_ownRec = 0;
    uint8_t g_saved[RecSize];
    // the cameras' own state
    bool g_freeInit = false;
    float g_free[3] = {};
    bool g_behindInit = false;
    float g_behind[3] = {};
    LARGE_INTEGER g_lastTick{};
    uint64_t g_deadSince = 0;

    void PatchHeld(uint8_t* base);

    // 0.5.602: THE GAME'S OWN SPECTATOR (F11, "Leave the match while
    // spectating", host only, off by default). A player's session state is
    // the record's +1924: 3 playing, 4 dead, 1 SPECTATOR (the multiplayer's
    // follow cycle, sub_82560928, runs only for 1 and steps through clients
    // that are 3; a frame's listener, sub_8252E2E0, takes a state 1 player's
    // place from the client his +1344 names), set by the scripts through the
    // field setter sub_824C05E0 (table 0x825FF09C). +1344 is the client a
    // player's view belongs to: his own number, or the followed one's. While
    // spectating with this on, your player is put in state 1 (out of the
    // match: not drawn, not hit, the bots look only for state 3), +1344 the
    // player you follow (your own in the free camera); when you stop, state
    // 3 again, at the place, aim and +1344 you left. If the game changes the
    // state meanwhile (a round's end, the scripts), it is left to the game.
    std::atomic<bool> g_leaveMatch{false};
    bool g_inGameSpectator = false;
    uint32_t g_gsRecord = 0, g_gsClientNum = 0;
    float g_gsPlace[5] = {};
    void GameSpectator(uint8_t* base, uint32_t own, int me)
    {
        using Spectator::Mode;
        const int mode = g_mode.load();
        const bool follow = mode == int(Mode::FollowEyes) || mode == int(Mode::FollowBehind);
        // 0.5.605: 604's self-test: the game puts the state back to 3 in the
        // next frame (6860 times in 90 s as this went back in), and the first
        // change moved the player across the map (1991 557 to 1042 -1196, a
        // respawn). The state is the scripts' (their sessionstate), not to be
        // written under them. So one try a spectating (refused: given up
        // until spectating stops), and the switch is only offered with
        // COD3_SPECTATE_LEAVE=1, for the work on the scripts' side.
        static bool refused = false;
        if (mode == int(Mode::Off)) refused = false;
        const bool want = g_leaveMatch.load() && !refused && mode != int(Mode::Off) && own != 0 && BotsThisMachineHostsBots(base) && !KillcamReplayingNow();
        if (g_inGameSpectator && own != g_gsRecord)
        {
            printf("spectator: game's spectator: your record changed (a new map): left to the game\n"); fflush(stdout);
            g_inGameSpectator = false;
        }
        if (g_inGameSpectator && Guest::Read32(base, own + 1924) != 1)
        {
            printf("spectator: game's spectator: the game set your state to %u: left to it, not tried again while spectating\n", Guest::Read32(base, own + 1924)); fflush(stdout);
            g_inGameSpectator = false;
            refused = true;
        }
        if (want && !g_inGameSpectator)
        {
            if (!Alive(base, own)) return;   // only from playing, alive
            g_gsRecord = own;
            g_gsClientNum = Guest::Read32(base, own + 1344);
            g_gsPlace[0] = ReadF(base, own); g_gsPlace[1] = ReadF(base, own + 4); g_gsPlace[2] = ReadF(base, own + 8);
            g_gsPlace[3] = ReadF(base, own + 204); g_gsPlace[4] = ReadF(base, own + 208);
            Guest::Write32(base, own + 1924, 1);
            g_inGameSpectator = true;
            printf("spectator: game's spectator: you are out of the match (state 3 -> 1, +1344 was %d, pm_type %d, at %.0f %.0f %.0f)\n",
                   int32_t(g_gsClientNum), int32_t(Guest::Read32(base, own + 36)), g_gsPlace[0], g_gsPlace[1], g_gsPlace[2]);
            fflush(stdout);
        }
        if (g_inGameSpectator && want)
        {
            const int target = g_target.load();
            const uint32_t view = follow && target >= 0 && target != me ? uint32_t(target) : g_gsClientNum;
            if (Guest::Read32(base, own + 1344) != view) Guest::Write32(base, own + 1344, view);
            static uint64_t toldAt = 0;
            static int told = 0;
            const uint64_t now = GetTickCount64();
            if (told < 60 && now - toldAt >= 2000)
            {
                toldAt = now; told++;
                printf("spectator: game's spectator: state %u, +1344 %d, pm_type %d, health %d, at %.0f %.0f %.0f\n", Guest::Read32(base, own + 1924),
                       int32_t(Guest::Read32(base, own + 1344)), int32_t(Guest::Read32(base, own + 36)), int32_t(Guest::Read32(base, own + 304)),
                       ReadF(base, own), ReadF(base, own + 4), ReadF(base, own + 8));
                fflush(stdout);
            }
        }
        if (!want && g_inGameSpectator)
        {
            Guest::Write32(base, own + 1924, 3);
            Guest::Write32(base, own + 1344, g_gsClientNum);
            WriteF(base, own, g_gsPlace[0]); WriteF(base, own + 4, g_gsPlace[1]); WriteF(base, own + 8, g_gsPlace[2]);
            WriteF(base, own + 16, 0.0f); WriteF(base, own + 20, 0.0f); WriteF(base, own + 24, 0.0f);
            WriteF(base, own + 204, g_gsPlace[3]); WriteF(base, own + 208, g_gsPlace[4]);
            g_inGameSpectator = false;
            printf("spectator: game's spectator: back in the match (state 1 -> 3, at %.0f %.0f %.0f)\n", g_gsPlace[0], g_gsPlace[1], g_gsPlace[2]);
            fflush(stdout);
        }
    }

    // Once a frame on the game's thread: what was asked from the keys and the menu.
    void Tick(uint8_t* base)
    {
        using Spectator::Mode;
        static int applied = 0;
        const uint64_t now = GetTickCount64();
        const int me = int(Local(base));
        const uint32_t own = RawRecordOf(base, me);
        // 0.5.603: COD3_SPECTATE_TEST=from,to[,mode] (a test nobody plays): the
        // game's spectator on (Leave the match) and spectating in mode (2 his
        // eyes by default) from 'from' to 'to' seconds after the first frame
        // of a level, off after.
        {
            static int phase = -1;   // -1 not asked, 0 waiting, 1 on, 2 done
            static double from = 0, to = 0;
            static int testMode = 2;
            static uint64_t firstAt = 0;
            if (phase < 0)
            {
                phase = 2;
                if (const char* t = getenv("COD3_SPECTATE_TEST"); t && sscanf(t, "%lf,%lf,%d", &from, &to, &testMode) >= 2) phase = 0;
            }
            if (phase < 2 && own)
            {
                if (!firstAt) firstAt = now;
                const double seconds = double(now - firstAt) / 1000.0;
                if (phase == 0 && seconds >= from)
                {
                    phase = 1;
                    printf("spectator: test: on at %.0f s (mode %d, leave the match)\n", seconds, testMode); fflush(stdout);
                    g_leaveMatch.store(true);
                    Spectator::Set(Mode(std::clamp(testMode, 1, 3)));
                }
                else if (phase == 1 && seconds >= to)
                {
                    phase = 2;
                    printf("spectator: test: off at %.0f s\n", seconds); fflush(stdout);
                    Spectator::Set(Mode::Off);
                }
                if (phase == 1)
                {
                    static uint64_t steppedAt = 0;
                    if (!steppedAt) steppedAt = now;
                    else if (now - steppedAt >= 15000) { steppedAt = now; g_steps.fetch_add(1); }   // the next player every 15 s
                }
            }
        }
        GameSpectator(base, own, me);
        g_ownAlive.store(own && (Alive(base, own) || g_inGameSpectator || g_pure.load()));
        int mode = g_mode.load();
        const bool follow = mode == int(Mode::FollowEyes) || mode == int(Mode::FollowBehind);
        int target = g_target.load();
        const int steps = g_steps.exchange(0);
        bool changed = false;
        if (follow)
        {
            if (steps) { const int t = Pick(base, target, steps > 0 ? 1 : -1); if (t >= 0 && t != target) { target = t; changed = true; } }
            const uint32_t r = RecordOf(base, target);
            if (!r || target == me) { target = Pick(base, target, 1); changed = true; }
            else if (!Alive(base, r))
            {
                // 3 s on his body, then the next one alive
                if (!g_deadSince) g_deadSince = now;
                else if (now - g_deadSince > 3000) { const int t = Pick(base, target, 1); if (t >= 0 && t != target) { target = t; changed = true; } }
            }
            if (Alive(base, RecordOf(base, target))) g_deadSince = 0;
            if (target < 0)
            {
                Overlay::Notice("SPECTATING  -  nobody to follow: free camera", 2500);
                mode = int(Mode::FreeCam);
                g_mode.store(mode);
            }
            g_target.store(target);
            g_targetAlive.store(Alive(base, RecordOf(base, target)));
        }
        if (mode != applied)
        {
            const bool gunWas = applied == int(Mode::Off) || applied == int(Mode::FollowEyes);
            const bool gunNow = mode == int(Mode::Off) || mode == int(Mode::FollowEyes);
            if (gunWas != gunNow) Kernel::QueueConsoleCommand(gunNow ? "cg_drawGun 1" : "cg_drawGun 0");
            if (mode == int(Mode::Off)) Overlay::Notice("SPECTATING OFF", 1500);
            else if (mode == int(Mode::FreeCam)) Overlay::Notice("FREE CAMERA  -  move, look, jump up, crouch down, sprint fast  |  Y: follow  |  F6: off", 4000);
            g_freeInit = false;
            g_freeAimInit = false;
            g_behindInit = false;
            printf("spectator: mode %d -> %d\n", applied, mode); fflush(stdout);
            if (mode == int(Mode::FollowEyes) || mode == int(Mode::FollowBehind)) changed = true;
            applied = mode;
        }
        if (changed && (mode == int(Mode::FollowEyes) || mode == int(Mode::FollowBehind)) && target >= 0)
        {
            const std::string text = std::string(mode == int(Mode::FollowEyes) ? "FOLLOWING  -  " : "FOLLOWING (BEHIND)  -  ") + NameOf(target) +
                                     "  |  fire: next, aim: previous, Y: mode, F6: off";
            Overlay::Notice(text, 3000);
            printf("spectator: following client %d (%s)\n", target, NameOf(target).c_str()); fflush(stdout);
            g_behindInit = false;
            g_deadSince = 0;
            if (mode == int(Mode::FollowEyes)) PatchHeld(base);
        }
    }

    bool EyesOn(uint8_t* base, int& target, uint32_t& r)
    {
        if (g_mode.load() != int(Spectator::Mode::FollowEyes) || KillcamReplayingNow()) return false;
        target = g_target.load();
        if (target < 0 || target == int(Local(base))) return false;
        r = RecordOf(base, target);
        return r != 0;
    }

    void PatchHeld(uint8_t* base)
    {
        const uint32_t lc = Local(base);
        if (lc > 3) return;
        const uint32_t cg = Cg + CgSize * lc;
        const uint32_t a = Guest::Read32(base, cg + 32), b = Guest::Read32(base, cg + 36);
        Spectator::PatchSnapshot(base, a);
        if (b != a) Spectator::PatchSnapshot(base, b);
    }

    void PutCamera(uint8_t* base, uint32_t camera, const float o[3], float pitch, float yaw, bool turn)
    {
        WriteF(base, camera + 48, o[0]); WriteF(base, camera + 52, o[1]); WriteF(base, camera + 56, o[2]);
        const uint32_t view = Cg + CgSize * Guest::Read32(base, camera + 480);
        WriteF(base, view + 4960, o[0]); WriteF(base, view + 4964, o[1]); WriteF(base, view + 4968, o[2]);
        if (!turn) return;
        const float p = pitch * 0.0174533f, y = yaw * 0.0174533f;
        WriteF(base, camera + 64, pitch); WriteF(base, camera + 68, yaw);
        WriteF(base, camera + 80, cosf(p) * cosf(y)); WriteF(base, camera + 84, cosf(p) * sinf(y)); WriteF(base, camera + 88, -sinf(p));
        WriteF(base, view + 5024, pitch); WriteF(base, view + 5028, yaw); WriteF(base, view + 5032, 0.0f);
    }
}

Spectator::Mode Spectator::Current() { return Mode(g_mode.load()); }
bool Spectator::Active() { return g_mode.load() != int(Mode::Off); }
float& Spectator::FreeCamSpeed() { return g_speed; }
bool Spectator::SmoothEyes() { return g_smooth.load(); }
bool Spectator::LeaveMatch() { return g_leaveMatch.load(); }
void Spectator::SetLeaveMatch(bool on) { g_leaveMatch.store(on); printf("spectator: leave the match while spectating %s\n", on ? "on" : "off"); fflush(stdout); }
void Spectator::SetSmoothEyes(bool on) { g_smooth.store(on); printf("spectator: smoothing of his eyes %s\n", on ? "on" : "off"); fflush(stdout); }

void Spectator::Set(Mode mode)
{
    if (mode != Mode::Off) g_lastMode.store(int(mode));
    g_mode.store(int(mode));
}
void Spectator::Toggle() { Set(Active() ? Mode::Off : Mode(g_lastMode.load())); }

// 0.5.608: THE SPECTATOR'S SEAT (user: "as its own option for when we spawn
// into a game similar to how you pick allies axis or auto assign ... no map,
// no hud no nothing just pure camera"). Every player who joins is first the
// game's own spectator: CallbackPlayerJoin / CallbackPlayerEnter / StartRound
// (_mp_common.bro) call SpawnSpectator (sub_82214B30: through the engine's
// builtins, the session state 1, followed client -1, and a spawn at a
// spectator's point), the local player's thread SpawnLocalSpectator
// (sub_82214EB8) with it, until a side and a kit are picked. Choosing the
// seat keeps him so: the game's menus are taken down (menu_mouse.cpp: their
// system's active menu -1, the team choice put back when leaving), the HUD
// is off (cg_draw2D 0), and the camera is ours: free, his eyes or behind
// him. Chosen in a match already (F11, or from the in-game menus), the
// game's SpawnSpectator is called for him first, as its join does.
namespace
{
    std::atomic<int> g_pureWant{ 0 };   // 1 take the seat, -1 leave it
    uint32_t g_handleScratch = 0;
    int32_t g_seatSide = -1, g_seatKit = -1;   // 0.5.611: his side and kit before the seat in a match
    std::atomic<int> g_chosenRow{ -1 };   // 0.5.648: the team choice's row A was pressed on (0 auto, 1 allies, 2 axis)
    bool g_awaitSide = false;             // the seat left by SWITCH SIDE: a side to join once the team choice is done
    uint64_t g_awaitAt = 0;

    void CallGuest(PPCContext& ctx, uint8_t* base, uint32_t fn, uint64_t a3, uint64_t a4 = 0, uint64_t a5 = 0, uint64_t a6 = 0, uint64_t a7 = 0)
    {
        PPCContext call = ctx;
        call.r3.u64 = a3; call.r4.u64 = a4; call.r5.u64 = a5; call.r6.u64 = a6; call.r7.u64 = a7;
        if (PPCFunc* f = Guest::Lookup(fn)) f(call, base);
    }

    // 0.5.611: joining again after the seat in a match, as the host's own
    // team and kit menus join a player (bots.cpp, PickSide and PickKit)
    void Rejoin(PPCContext& ctx, uint8_t* base, int me, uint32_t own)
    {
        constexpr uint32_t Callbacks = 0x82BCBF64, ModeObject = 0x82BCCAC0, PickSideOther = 0x823EA4E0;
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t e = Valid(entities) ? Guest::Read32(base, entities + 4 * uint32_t(me + 1)) : 0;
        if (!e) return;
        if (!g_handleScratch) g_handleScratch = Guest::AllocatePhysical(0x20);
        if (!g_handleScratch) return;
        Guest::Write32(base, g_handleScratch, Guest::Read32(base, e + 484));
        const uint32_t mode = Guest::Read32(base, ModeObject);
        const uint32_t object = Guest::Read32(base, Callbacks);
        if (Valid(mode) && Guest::Read32(base, mode) != 0)
            CallGuest(ctx, base, PickSideOther, mode, e, uint32_t(g_seatSide), 0, 0);
        else if (Valid(object))
        {
            const uint32_t team = Guest::Read32(base, e + 520);
            if (Valid(team)) Guest::Write32(base, team + 4, uint32_t(g_seatSide));
            if (const uint32_t side = Guest::Read32(base, object + 3320)) CallGuest(ctx, base, side, g_handleScratch, 1, 1);
        }
        if (Valid(object) && g_seatKit >= 0 && g_seatKit < 64)
            if (const uint32_t kit = Guest::Read32(base, object + 3328)) CallGuest(ctx, base, kit, g_handleScratch, uint32_t(g_seatKit));
        // 0.5.612: 611's self-test: the session's way left him in state 1 for
        // the rest of the match. Still a spectator, he is spawned as the bots
        // are (bots.cpp, Respawn): the game's spawn at one of his side's
        // points, sub_822029F0, given his entity number and the side's name
        // as one of the game's strings (sub_821E0E48 makes it; 0x8205FCC8
        // "allies", 0x8205FCD0 "axis"; side 1 is the allies).
        if (Guest::Read32(base, own + 1924) == 1)
        {
            constexpr uint32_t SpawnAtPoint = 0x822029F0, MakeString = 0x821E0E48, AlliesName = 0x8205FCC8, AxisName = 0x8205FCD0;
            Guest::Write32(base, g_handleScratch + 8, 0);
            CallGuest(ctx, base, MakeString, g_handleScratch + 8, g_seatSide == 1 ? AlliesName : AxisName);
            CallGuest(ctx, base, SpawnAtPoint, g_handleScratch, g_handleScratch + 8);
            printf("spectator: the spectator's seat left in a match: still a spectator, spawned at a point of the %s: state %u, health %d, at %.0f %.0f %.0f\n",
                   g_seatSide == 1 ? "allies" : "axis", Guest::Read32(base, own + 1924), int32_t(Guest::Read32(base, own + 304)),
                   ReadF(base, own), ReadF(base, own + 4), ReadF(base, own + 8));
            fflush(stdout);
        }
        printf("spectator: the spectator's seat left in a match: joined again as side %d, kit %d (%s); state now %u\n", g_seatSide, g_seatKit,
               Valid(mode) && Guest::Read32(base, mode) != 0 ? "the session's way" : "the menus' callbacks", Guest::Read32(base, own + 1924));
        fflush(stdout);
    }

    uint32_t HandleOf(uint8_t* base, int client)
    {
        const uint32_t entities = Guest::Read32(base, EntityList);
        const uint32_t e = Valid(entities) ? Guest::Read32(base, entities + 4 * uint32_t(client + 1)) : 0;
        const uint32_t index = uint32_t(client + 1), entry = 0x82BD1FC0u + index * 8;
        return e && Guest::Read32(base, entry) == e ? ((Guest::Read32(base, entry + 4) << 14) | index) : 0u;
    }

    // cg_draw2D, sent only when it changes
    int g_seatHud = -1;
    void SeatHud(bool on)
    {
        if (g_seatHud == int(on)) return;
        g_seatHud = int(on);
        Kernel::QueueConsoleCommand(on ? "cg_draw2D 1" : "cg_draw2D 0");
    }
    void GameFrame(PPCContext& ctx, uint8_t* base)
    {
        static uint64_t lastAt = 0;
        const uint64_t now = GetTickCount64();
        if (now - lastAt < 15) return;   // the systems' updates come several a frame
        lastAt = now;
        const int me = int(Local(base));
        const uint32_t own = me >= 0 && me < MaxClients ? RawRecordOf(base, me) : 0;
        // the state as the game has it, each change (at most 200 a run)
        {
            static uint32_t lastState = 0xFFFFFFFFu, lastOwn = 0;
            static int told = 0;
            const uint32_t state = own ? Guest::Read32(base, own + 1924) : 0xFFFFFFFEu;
            if ((state != lastState || own != lastOwn) && told < 200)
            {
                told++;
                printf("spectator: your session state %d (+1344 %d, pm_type %d)%s\n", int32_t(state), own ? int32_t(Guest::Read32(base, own + 1344)) : 0,
                       own ? int32_t(Guest::Read32(base, own + 36)) : 0, g_pure.load() ? " [the spectator's seat]" : "");
                fflush(stdout);
            }
            lastState = state; lastOwn = own;
        }
        // COD3_SPECTATE_SEATTEST="team=40;mid=60,60" (a test nobody plays): the
        // seat taken as the team choice first comes up and left 40 s later;
        // taken in the match 60 s after its first frame with a player, the
        // cameras in turn (free, his eyes, behind him; the next player every
        // 10 s), and left 60 s later.
        {
            static int parsed = 0, teamFor = 0, midFrom = 0, midFor = 0;
            static uint64_t teamAt = 0, firstAt = 0, midAt = 0;
            static int teamPhase = 0, midPhase = 0;
            if (!parsed)
            {
                parsed = 1;
                if (const char* t = getenv("COD3_SPECTATE_SEATTEST"))
                {
                    if (const char* p = strstr(t, "team=")) teamFor = atoi(p + 5);
                    if (const char* p = strstr(t, "mid=")) sscanf(p + 4, "%d,%d", &midFrom, &midFor);
                    printf("spectator: seat test: team %d s, mid from %d s for %d s\n", teamFor, midFrom, midFor); fflush(stdout);
                }
            }
            if (teamFor > 0)
            {
                // 0.5.614: a picture and a frame's draws of the team choice (3 s up),
                // then the seat; again 5 s into the seat (the blur the user saw)
                static uint64_t seenAt = 0;
                if (teamPhase == 0 && MenuMouse::TeamMenuUp() && !seenAt) seenAt = now;
                if (teamPhase == 0 && seenAt && now - seenAt >= 3000)
                {
                    printf("spectator: seat test: the team choice: a picture and the next frame's draws\n"); fflush(stdout);
                    Overlay::RequestScreenshot(); Render::LogNextFrame();
                    teamPhase = 3; seenAt = now;
                }
                else if (teamPhase == 3 && now - seenAt >= 1500) { teamPhase = 1; teamAt = now; printf("spectator: seat test: the team choice is up: the seat taken\n"); fflush(stdout); g_pureWant.store(1); }
                else if (teamPhase == 1 && now - teamAt >= 5000 && now - teamAt < 5100)
                {
                    printf("spectator: seat test: in the seat: a picture and the next frame's draws\n"); fflush(stdout);
                    Overlay::RequestScreenshot(); Render::LogNextFrame();
                    teamAt -= 100;
                }
                else if (teamPhase == 1 && now - teamAt >= uint64_t(teamFor) * 1000u) { teamPhase = 2; printf("spectator: seat test: the seat left (team)\n"); fflush(stdout); g_pureWant.store(-1); }
            }
            if (midFor > 0 && own && Alive(base, own) && !firstAt) firstAt = now;
            if (midFor > 0 && firstAt)
            {
                const uint64_t since = now - firstAt;
                // 0.5.615: a frame's draws in play (the blur's comparison), 10 s before the seat
                static bool playLogged = false;
                if (!playLogged && midFrom > 12 && since >= uint64_t(midFrom - 10) * 1000u)
                {
                    playLogged = true;
                    printf("spectator: seat test: in play: a picture and the next frame's draws\n"); fflush(stdout);
                    Overlay::RequestScreenshot(); Render::LogNextFrame();
                }
                if (midPhase == 0 && since >= uint64_t(midFrom) * 1000u) { midPhase = 1; midAt = now; printf("spectator: seat test: the seat taken in the match\n"); fflush(stdout); g_pureWant.store(1); }
                else if (midPhase == 1)
                {
                    const uint64_t in = now - midAt, len = uint64_t(midFor) * 1000u;
                    if (in >= len) { midPhase = 2; printf("spectator: seat test: the seat left (mid)\n"); fflush(stdout); g_pureWant.store(-1); }
                    else if (g_pure.load())
                    {
                        static bool seatLogged = false;   // 0.5.615: the seat in a match pictured too (the free camera, 5 s in)
                        if (!seatLogged && in >= 5000) { seatLogged = true; printf("spectator: seat test: in the seat in the match: a picture and the next frame's draws\n"); fflush(stdout); Overlay::RequestScreenshot(); Render::LogNextFrame(); }
                        const Spectator::Mode m = in < len / 3 ? Spectator::Mode::FreeCam : in < 2 * len / 3 ? Spectator::Mode::FollowEyes : Spectator::Mode::FollowBehind;
                        if (Spectator::Current() != m) Spectator::Set(m);
                        static uint64_t steppedAt = 0;
                        if (m != Spectator::Mode::FreeCam && now - steppedAt >= 10000) { steppedAt = now; g_steps.fetch_add(1); }
                    }
                }
            }
        }
        const int want = g_pureWant.exchange(0);
        if (want > 0 && !g_pure.load())
        {
            if (!own) { printf("spectator: the spectator's seat: no player of yours yet\n"); fflush(stdout); return; }
            MenuMouse::SetHidden(true);
            const uint32_t state = Guest::Read32(base, own + 1924);
            g_seatSide = -1;
            if (state != 1)
            {
                // 0.5.611: his side and kit, to join again as he was
                {
                    const uint32_t entities = Guest::Read32(base, EntityList);
                    const uint32_t e = Valid(entities) ? Guest::Read32(base, entities + 4 * uint32_t(me + 1)) : 0;
                    const uint32_t team = e ? Guest::Read32(base, e + 520) : 0;
                    g_seatSide = Valid(team) ? int32_t(Guest::Read32(base, team + 4)) : -1;
                    g_seatKit = int32_t(int16_t(Guest::Read16(base, own + 1920)));
                    printf("spectator: the spectator's seat: side %d and kit %d kept\n", g_seatSide, g_seatKit); fflush(stdout);
                }
                // in the match already: the game's own SpawnSpectator, as its join calls it
                const uint32_t handle = HandleOf(base, me);
                if (!g_handleScratch) g_handleScratch = Guest::AllocatePhysical(0x20);
                if (handle && g_handleScratch)
                {
                    Guest::Write32(base, g_handleScratch, handle);
                    PPCContext call = ctx;
                    call.r3.u64 = g_handleScratch;
                    if (PPCFunc* f = Guest::Lookup(0x82214B30)) f(call, base);
                    printf("spectator: the spectator's seat: the game's SpawnSpectator called (handle %08X): state %u -> %u, at %.0f %.0f %.0f\n", handle, state,
                           Guest::Read32(base, own + 1924), ReadF(base, own), ReadF(base, own + 4), ReadF(base, own + 8));
                    fflush(stdout);
                }
            }
            g_pure.store(true);
            RenderCommands::SetSpectatorSeat(true);   // 0.5.616: without the team choice's blur
            SeatHud(false);
            Spectator::Set(Spectator::Mode::FreeCam);
            Overlay::Notice("SPECTATOR  -  Y: free / his eyes / behind him  |  fire / aim: next / previous player  |  F6 (Back + Y): join a side", 6000);
            printf("spectator: the spectator's seat taken (state %u)\n", Guest::Read32(base, own + 1924)); fflush(stdout);
        }
        else if (want < 0 && g_pure.load())
        {
            // 0.5.643: left by the pause menu's SWITCH TEAM: no joining the side
            // of before; the game's team choice comes up instead (the game's own
            // for a spectator, or put up again below)
            if (want == -2)
            {
                g_seatSide = -1; g_chosenRow.store(-1); g_awaitSide = true; g_awaitAt = GetTickCount64();   // 0.5.648
                printf("spectator: SWITCH TEAM in the seat: the seat left for the team choice\n"); fflush(stdout);
            }
            g_pure.store(false);
            RenderCommands::SetSpectatorSeat(false);
            Spectator::Set(Spectator::Mode::Off);
            SeatHud(true);
            MenuMouse::SetHidden(false);
            printf("spectator: the spectator's seat left: the game's menus back (state %u)\n", own ? Guest::Read32(base, own + 1924) : 0); fflush(stdout);
            // 0.5.610: 609's self-test: left in a match, he stayed the game's
            // spectator with no menu to join from (none had been taken down).
            // 0.5.611: 610's: the team choice set up again was never updated nor
            // drawn in play. So he joins again as the host's own team and kit
            // menus join him (bots.cpp's PickSide / PickKit): his side and kit
            // of before the seat through the game's callbacks (Callbacks
            // 0x82BCBF64 +3320 side, +3328 kit, given his entity number; in the
            // session's mode, sub_823EA4E0). The team choice only when no side
            // was kept.
            if (own && Guest::Read32(base, own + 1924) == 1)
            {
                if (g_seatSide >= 1 && g_seatSide <= 2) Rejoin(ctx, base, me, own);
                else MenuMouse::OpenTeamChoice(base);
            }
            g_seatSide = -1;
        }
        // 0.5.625 (user, 624: "if i spectate a player i should be able to see hud,
        // but if i go freecam absolutely no hud"): in the seat the HUD follows the
        // camera: off for the free camera, his HUD on his eyes and behind him
        if (g_pure.load()) SeatHud(Spectator::Current() != Spectator::Mode::FreeCam || MenuMouse::MenuUp());   // 0.5.643: and while a menu (the pause menu) is up
        // 0.5.648 (user, 647: "when in spectator if i click to change teams to allies
        // or axis, it never spawns you in ... it just leaves you in a frozen spectator
        // cam"). 647's log: the seat left by SWITCH SIDE, a side taken on the team
        // choice, the menus gone, and you still the game's spectator (state 1): its
        // join goes on only from the match's start. So once the menus are gone with a
        // side taken there (A on AUTO SELECT, ALLIES or AXIS: FilterPad), you join it
        // as the seat's leaving does (Rejoin: the side, your kit, a spawn at its point).
        if (g_awaitSide && !g_pure.load())
        {
            const uint64_t t = GetTickCount64();
            const int row = g_chosenRow.load();
            if (!own || Guest::Read32(base, own + 1924) != 1) { g_awaitSide = false; printf("spectator: after SWITCH SIDE the game joined you itself (state %u)\n", own ? Guest::Read32(base, own + 1924) : 0); fflush(stdout); }
            else if (row >= 0 && !MenuMouse::MenuUp() && t - g_awaitAt > 300)
            {
                int side = row == 1 ? 1 : row == 2 ? 2 : -1;
                if (side < 0)
                {
                    // AUTO SELECT: the side the game gave you, else the allies
                    const uint32_t entities = Guest::Read32(base, EntityList);
                    const uint32_t e = Valid(entities) ? Guest::Read32(base, entities + 4 * uint32_t(me + 1)) : 0;
                    const uint32_t team = e ? Guest::Read32(base, e + 520) : 0;
                    const uint32_t read = Valid(team) ? Guest::Read32(base, team + 4) : 0;
                    side = read == 1 || read == 2 ? int(read) : 1;
                }
                const int16_t kit = int16_t(Guest::Read16(base, own + 1920));
                if (kit >= 0 && kit < 64) g_seatKit = kit;
                g_seatSide = side;
                printf("spectator: after SWITCH SIDE row %d taken in the seat: joining side %d, kit %d\n", row, side, g_seatKit); fflush(stdout);
                Rejoin(ctx, base, me, own);
                g_seatSide = -1;
                g_awaitSide = false;
            }
            else if (t - g_awaitAt > 120000) g_awaitSide = false;
        }
        // spawned by the game meanwhile (a round's start): the seat is left
        if (g_pure.load() && own && Guest::Read32(base, own + 1924) == 3)
        {
            printf("spectator: the game spawned you: the spectator's seat left\n"); fflush(stdout);
            g_pure.store(false);
            RenderCommands::SetSpectatorSeat(false);
            Spectator::Set(Spectator::Mode::Off);
            SeatHud(true);
            MenuMouse::SetHidden(false);
        }
    }
    // The seat's button, a row under the team choice's entries (menu_mouse.cpp
    // finds the place), drawn over the picture: the menus' 2D is the largest
    // 16:9 in the middle of the window, as the pointer is found in it.
    // 0.5.623: no longer drawn over the menu: SPECTATOR is the team choice's own
    // entry (menu_mouse.cpp); COD3_NOSEATENTRY=1 brings this drawn row back
    bool SeatShown() { static const bool drawn = getenv("COD3_NOSEATENTRY") != nullptr; return drawn && !g_pure.load() && MenuMouse::TeamMenuUp(); }
    void DrawSeat(int width, int height)
    {
        float u0, v0, u1, v1;
        if (!MenuMouse::SeatBox(u0, v0, u1, v1)) return;
        float w = float(width), h = w * 9.0f / 16.0f;
        if (h > float(height)) { h = float(height); w = h * 16.0f / 9.0f; }
        const float x0 = std::floor((float(width) - w) * 0.5f), y0 = std::floor((float(height) - h) * 0.5f);
        const ImVec2 a(x0 + u0 * w, y0 + v0 * h), b(x0 + u1 * w, y0 + v1 * h);
        const bool on = MenuMouse::PointerOnSeat();
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        // 0.5.615 (user: "spectator being its own box like how allies or axis
        // is"): a row as the team choice's own (614's picture: AUTO SELECT,
        // ALLIES, AXIS, light text left aligned, each over a thin gold line),
        // its text the rows' height, gold while the pointer is on it
        ImFont* font = ImGui::GetFont();
        const float rowHeight = b.y - a.y;
        const float size = std::max(12.0f, rowHeight * 0.62f);
        const char* label = "SPECTATOR";
        const char* hint = "F6 / Back + Y";
        const float ty = a.y + (rowHeight - size) * 0.5f;
        const ImU32 textColour = on ? IM_COL32(222, 178, 96, 255) : IM_COL32(226, 219, 201, 255);
        draw->AddText(font, size, ImVec2(a.x + 1.5f, ty + 1.5f), IM_COL32(0, 0, 0, 170), label);   // the game's text has a soft shadow
        draw->AddText(font, size, ImVec2(a.x, ty), textColour, label);
        const ImVec2 labelSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, label);
        const float hintSize = size * 0.55f;
        draw->AddText(font, hintSize, ImVec2(a.x + labelSize.x + size * 0.8f, ty + (size - hintSize) * 0.6f), IM_COL32(170, 160, 140, 200), hint);
        const float lineY = b.y - rowHeight * 0.08f, lineEnd = x0 + w * 0.84f;
        draw->AddLine(ImVec2(a.x - w * 0.17f, lineY), ImVec2(lineEnd, lineY), IM_COL32(120, 98, 52, on ? 230 : 150), std::max(1.0f, h / 700.0f));
    }
    struct HookOnce { HookOnce() { MenuMouse::SetFrameHook(GameFrame); Overlay::SetSeatHud(SeatShown, DrawSeat); } } g_hookOnce;
}

// 0.5.609: during play too (608's self-test: the menu systems' update, the
// only caller before, runs only while a menu is up, so the seat was never
// taken in the match): mp_fixes.cpp's frame (MpFixesLauncher) calls this.
void SpectatorGameFrame(PPCContext& ctx, uint8_t* base) { GameFrame(ctx, base); MenuMouse::Tick(base); }

void Spectator::TakeSeat() { g_pureWant.store(1); }
void Spectator::LeaveSeat() { g_pureWant.store(-1); }
bool Spectator::PureActive() { return g_pure.load(); }
bool Spectator::TakeMouseLook(float yawDegrees, float pitchDegrees)
{
    if (Current() != Mode::FreeCam) return false;
    AddTo(g_mouseYaw, yawDegrees);
    AddTo(g_mousePitch, pitchDegrees);
    return true;
}
void Spectator::Key()
{
    // 0.5.613: the seat everywhere (612's self-test: in a match too, and you
    // join again after); the spectating with your soldier left in the match
    // is F11's buttons now, and F6 ends it
    if (PureActive()) LeaveSeat();
    else if (Active()) Set(Mode::Off);
    else TakeSeat();
}
void Spectator::NextMode()
{
    switch (Current())
    {
    case Mode::FreeCam: Set(Mode::FollowEyes); break;
    case Mode::FollowEyes: Set(Mode::FollowBehind); break;
    default: Set(Mode::FreeCam); break;
    }
}
void Spectator::NextPlayer(int step)
{
    if (Current() == Mode::FreeCam || Current() == Mode::Off) Set(Mode::FollowEyes);
    g_steps.fetch_add(step);
}

std::string Spectator::Status()
{
    switch (Current())
    {
    case Mode::Off: return "Off";
    case Mode::FreeCam: return "Free camera";
    default: break;
    }
    const int t = g_target.load();
    std::string s = Current() == Mode::FollowEyes ? "Following from his eyes: " : "Following from behind: ";
    if (t < 0) return s + "(looking for a player)";
    return s + NameOf(t) + (g_targetAlive.load() ? "" : " (dead)");
}

void Spectator::FilterPad(Input::Pad& pad)
{
    static uint16_t was = 0;
    static bool rtWas = false, ltWas = false;
    const uint16_t now = pad.buttons;
    const bool yPressed = (now & PadY) && !(was & PadY);
    was = now;
    const bool rt = pad.rightTrigger > 60, lt = pad.leftTrigger > 60;
    const bool rtPressed = rt && !rtWas, ltPressed = lt && !ltWas;
    rtWas = rt; ltWas = lt;
    if (Overlay::IsOpen()) return;
    // Back + Y: on and off from the pad (F6 on the keyboard); on the team
    // choice, the spectator's seat (0.5.608)
    if (yPressed && (now & PadBack)) { Key(); pad.buttons &= uint16_t(~PadY); return; }
    // 0.5.640 (user: "you cannot actually scroll down to spectator"): the
    // game's walk of the team choice knows its three rows only; down from AXIS
    // is SPECTATOR, up from it AXIS, and the game sees neither press
    {
        constexpr uint16_t PadUp = 0x0001, PadDown = 0x0002;
        static uint16_t crossWas = 0;
        const uint16_t cross = now & (PadUp | PadDown);
        const uint16_t pressedNow = cross & uint16_t(~crossWas);
        crossWas = cross;
        static bool swallowing = false;   // the press that asked, until let go
        if (!cross) swallowing = false;
        const int row = MenuMouse::TeamSelected();
        if (row == 2 && (pressedNow & PadDown)) { MenuMouse::AskTeamRow(3); swallowing = true; }
        else if (row == 3 && (pressedNow & PadUp)) { MenuMouse::AskTeamRow(2); swallowing = true; }
        if (swallowing || row == 3) pad.buttons &= uint16_t(~(PadUp | PadDown));
    }
    // 0.5.623: A (or a click, or Enter) on SPECTATOR, the team choice's own fourth
    // entry (menu_mouse.cpp): the spectator's seat; the game never sees that A
    {
        static bool aWas = false;
        const bool a = (now & PadA) != 0;
        // 0.5.648: the row a side was taken on (for the join after SWITCH SIDE in the seat)
        if (a && !aWas) { const int row = MenuMouse::TeamSelected(); if (row >= 0 && row <= 2) g_chosenRow.store(row); }
        if (MenuMouse::TeamSeatSelected() && (a || aWas))
        {
            if (a && !aWas) { printf("spectator: SPECTATOR taken on the team choice\n"); fflush(stdout); TakeSeat(); }
            aWas = a;
            pad.buttons &= uint16_t(~PadA);
            return;
        }
        aWas = a;
    }
    // 0.5.643 (user: "we need to make a menu that is included in the mp in game
    // pause menus that says change team, but this menu need to open into a sub
    // menu that shows both allies axis or spectator"): the pause menu's own
    // SWITCH TEAM opens the team choice (with SPECTATOR); taken in the seat, the
    // seat is left first (its hiding would take the team choice down)
    // 0.5.646: Start (or Escape) in the seat with no menu up: the pause menu,
    // which the game does not put up for a spectator (menu_mouse.cpp puts it up)
    {
        constexpr uint16_t PadStart = 0x0010;
        static bool startWas = false;
        const bool start = (now & PadStart) != 0;
        if (start && !startWas && PureActive() && !MenuMouse::MenuUp()) { MenuMouse::AskPauseMenu(); printf("spectator: Start in the seat: the pause menu asked for\n"); fflush(stdout); }
        startWas = start;
    }
    // 0.5.644: it is SWITCH SIDE (643's log), which only asks YES / NO: its A
    // never reaches the game; the team choice is put up in the pause menu's
    // place (menu_mouse.cpp)
    {
        static bool aWasSwitch = false, swallowing = false;
        const bool a = (now & PadA) != 0;
        if (a && !aWasSwitch && MenuMouse::SwitchTeamSelected())
        {
            swallowing = true;
            if (PureActive()) g_pureWant.store(-2);
            MenuMouse::AskTeamChoiceFromPause();
        }
        if (!a) swallowing = false;
        aWasSwitch = a;
        if (swallowing) { pad.buttons &= uint16_t(~PadA); return; }
    }
    if (Input::MenuMode() && (!PureActive() || MenuMouse::MenuUp())) return;   // 0.5.643: in the seat a menu up (the pause menu) has the buttons
    if (!Active() || !g_ownAlive.load()) return;   // dead: the kit menu and the respawn are the game's
    if (yPressed) NextMode();
    if (Current() != Mode::FreeCam)
    {
        if (rtPressed) NextPlayer(1);
        if (ltPressed) NextPlayer(-1);
    }
    g_moveX.store(float(pad.thumbLX) / 32767.0f);
    {
        // 0.5.614: the right stick for the free camera's aim, past a dead zone
        auto stick = [](int16_t v) { const float f = float(v) / 32767.0f; return std::fabs(f) < 0.2f ? 0.0f : f; };
        g_lookX.store(stick(pad.thumbRX));
        g_lookY.store(stick(pad.thumbRY));
    }
    g_moveY.store(float(pad.thumbLY) / 32767.0f);
    g_moveZ.store(((now & PadA) ? 1.0f : 0.0f) - ((now & PadB) ? 1.0f : 0.0f));
    g_fast.store((now & PadLeftThumb) != 0);
    // your player stands still: no moving, firing, using or throwing (the look stays)
    pad.thumbLX = 0; pad.thumbLY = 0;
    pad.leftTrigger = 0; pad.rightTrigger = 0;
    pad.buttons &= uint16_t(~(PadA | PadB | PadX | PadY | PadLeftThumb | PadRightThumb | PadLeftShoulder | PadRightShoulder));
}

bool Spectator::SwapIn(uint8_t* base)
{
    if (g_swapped) return false;
    Tick(base);
    int target; uint32_t r;
    if (!EyesOn(base, target, r)) return false;
    const uint32_t own = RawRecordOf(base, int(Local(base)));
    if (!own || own == r) return false;
    Note(base, target, r);
    memcpy(g_saved, base + own, RecSize);
    memcpy(base + own, base + r, RecSize);
    // 0.5.593: the smoothing is a switch (F11): off, the view is his record's
    // own place and aim, as the killcam replay's (no blend since 0.5.346)
    const Sample s = g_smooth.load() ? Now() : g_cur;
    if (g_smooth.load())
    {
        WriteF(base, own, s.o[0]); WriteF(base, own + 4, s.o[1]); WriteF(base, own + 8, s.o[2]);
        WriteF(base, own + 204, s.pitch); WriteF(base, own + 208, s.yaw);
    }
    g_ownRec = own;
    g_swapped = true;
    static int told = 0;
    if (told < 3) { told++; printf("spectator: cgame's frame drawn as client %d (origin %.0f %.0f %.0f)\n", target, s.o[0], s.o[1], s.o[2]); fflush(stdout); }
    return true;
}

void Spectator::SwapOut(uint8_t* base)
{
    if (!g_swapped) return;
    memcpy(base + g_ownRec, g_saved, RecSize);
    g_swapped = false;
}

void Spectator::PatchSnapshot(uint8_t* base, uint32_t snap)
{
    if (!Valid(snap)) return;
    int target; uint32_t r;
    if (!EyesOn(base, target, r)) return;
    memcpy(base + snap + 16, base + r, PsSize);
    const uint32_t flags = snap + 16 + 44;
    Guest::Write32(base, flags, Guest::Read32(base, flags) | 0x00100000u);   // followed: interpolated, not predicted
    Guest::Write32(base, snap + 1360, uint32_t(target));                     // the followed client: his body hidden (CG_Player)
}

void Spectator::PatchWeapon(uint8_t* base)
{
    int target; uint32_t r;
    if (!EyesOn(base, target, r)) return;
    const uint32_t lc = Local(base);
    if (lc > 3) return;
    const uint32_t cg = Cg + CgSize * lc;
    const uint32_t weapon = Guest::Read32(base, cg + 3136 + 164), list = Guest::Read32(base, WeaponList);
    if (weapon && weapon < 128 && Valid(list)) Guest::Write32(base, cg + 4732, Guest::Read32(base, list + 4 * weapon));
}

void Spectator::ApplyCamera(uint8_t* base, uint32_t camera)
{
    Tick(base);
    LARGE_INTEGER t, freq;
    QueryPerformanceCounter(&t); QueryPerformanceFrequency(&freq);
    const float dt = g_lastTick.QuadPart ? std::clamp(float(double(t.QuadPart - g_lastTick.QuadPart) / double(freq.QuadPart)), 0.0f, 0.1f) : 0.0f;
    g_lastTick = t;
    if (KillcamReplayingNow()) return;
    const Mode mode = Current();
    // 0.5.593 (user, 592: "some bots ... there gun is not in frame properly"):
    // twice a second while following from his eyes, the view the game made
    // (camera) against the record it was made from (ours, holding his),
    // cgame's player state (cg+3136: the gun is placed from it) and his own
    // live record, with what he holds and does, to see which disagree.
    if (mode == Mode::FollowEyes)
    {
        static uint64_t toldAt = 0;
        static int told = 0;
        const uint64_t now = GetTickCount64();
        const int target = g_target.load();
        const uint32_t live = RecordOf(base, target), lc = Local(base);
        if (live && lc <= 3 && told < 400 && now - toldAt >= 500)
        {
            toldAt = now; told++;
            const uint32_t own = g_swapped ? g_ownRec : RawRecordOf(base, int(lc)), ps = Cg + CgSize * lc + 3136;
            printf("spectator: eyes %s (client %d, smooth %d, swapped %d): camera %.0f %.0f %.0f view %.1f %.1f | record %.0f %.0f %.0f view %.1f %.1f | cg ps %.0f %.0f %.0f view %.1f %.1f weapon %u | "
                   "his %.0f %.0f %.0f view %.1f %.1f weapon %u ads %.2f flags %08X pm_type %d +44 %08X\n",
                   NameOf(target).c_str(), target, g_smooth.load() ? 1 : 0, g_swapped ? 1 : 0,
                   ReadF(base, camera + 48), ReadF(base, camera + 52), ReadF(base, camera + 56), ReadF(base, camera + 64), ReadF(base, camera + 68),
                   ReadF(base, own), ReadF(base, own + 4), ReadF(base, own + 8), ReadF(base, own + 204), ReadF(base, own + 208),
                   ReadF(base, ps), ReadF(base, ps + 4), ReadF(base, ps + 8), ReadF(base, ps + 204), ReadF(base, ps + 208), Guest::Read32(base, ps + 164),
                   ReadF(base, live), ReadF(base, live + 4), ReadF(base, live + 8), ReadF(base, live + 204), ReadF(base, live + 208), Guest::Read32(base, live + 164),
                   ReadF(base, live + 172), Guest::Read32(base, live + 240), int32_t(Guest::Read32(base, live + 36)), Guest::Read32(base, live + 44));
            fflush(stdout);
        }
        // 0.5.604: 603's self-test showed why the gun leaves the frame: in a
        // bot's quick turn the camera (137.3), the record it came from (145.3)
        // and cgame's player state the gun is placed from (153.3) were a step
        // of his turn apart each (8 degrees a server frame), so the gun
        // pointed up to 16 degrees off the view and swayed with the jumps. The
        // camera's angles, once made, go into cgame's player state, so the
        // gun is placed and sways as the view turns. COD3_SPECTATE_GUNSYNC=0: off.
        static const bool gunSync = [] { const char* t = getenv("COD3_SPECTATE_GUNSYNC"); return !(t && t[0] == '0'); }();
        if (gunSync && g_swapped && lc <= 3)
        {
            const uint32_t ps = Cg + CgSize * lc + 3136;
            WriteF(base, ps + 204, ReadF(base, camera + 64));
            WriteF(base, ps + 208, ReadF(base, camera + 68));
        }
    }
    if (mode == Mode::FreeCam)
    {
        if (!g_freeInit)
        {
            g_freeInit = true;
            g_free[0] = ReadF(base, camera + 48); g_free[1] = ReadF(base, camera + 52); g_free[2] = ReadF(base, camera + 56);
        }
        // 0.5.614: its own aim (user, 613: in the seat "you cannot fly left or
        // right only up and down": the game's spectator script holds the view
        // still there, so the camera's own angles never turned): from the
        // view when it starts, turned by the mouse (mouse_look.cpp hands its
        // movement here) and the right stick; the sticks / keys move it
        if (!g_freeAimInit)
        {
            g_freeAimInit = true;
            g_freePitch = ReadF(base, camera + 64); g_freeYaw = ReadF(base, camera + 68);
            g_mouseYaw.store(0.0f); g_mousePitch.store(0.0f);
        }
        g_freeYaw -= g_mouseYaw.exchange(0.0f) + g_lookX.load() * 150.0f * dt;
        g_freePitch += g_mousePitch.exchange(0.0f) - g_lookY.load() * 110.0f * dt;
        g_freePitch = std::clamp(g_freePitch, -85.0f, 85.0f);
        g_freeYaw = fmodf(g_freeYaw + 540.0f, 360.0f) - 180.0f;
        const float pitch = g_freePitch, yaw = g_freeYaw;
        const float p = pitch * 0.0174533f, y = yaw * 0.0174533f;
        const float fwd[3] = { cosf(p) * cosf(y), cosf(p) * sinf(y), -sinf(p) };
        const float right[3] = { sinf(y), -cosf(y), 0.0f };
        const float mx = g_moveX.load(), my = g_moveY.load(), mz = g_moveZ.load();
        const float speed = g_speed * (g_fast.load() ? 3.0f : 1.0f) * dt;
        for (int i = 0; i < 3; i++) g_free[i] += (fwd[i] * my + right[i] * mx) * speed;
        g_free[2] += mz * speed;
        PutCamera(base, camera, g_free, pitch, yaw, true);
        return;
    }
    if (mode != Mode::FollowBehind) return;
    const int target = g_target.load();
    const uint32_t r = RecordOf(base, target);
    if (!r) return;
    Note(base, target, r);
    const Sample s = Now();
    // behind and a little above his eyes, looking where he looks
    const float pitch = std::clamp(s.pitch, -60.0f, 60.0f) * 0.5f;
    const float p = pitch * 0.0174533f, y = s.yaw * 0.0174533f;
    const float eye[3] = { s.o[0], s.o[1], s.o[2] + (Alive(base, r) ? 60.0f : 20.0f) };
    const float goal[3] = { eye[0] - cosf(p) * cosf(y) * 120.0f, eye[1] - cosf(p) * sinf(y) * 120.0f, eye[2] + sinf(p) * 120.0f + 16.0f };
    if (!g_behindInit) { g_behindInit = true; memcpy(g_behind, goal, sizeof goal); }
    const float k = 1.0f - expf(-14.0f * dt);
    for (int i = 0; i < 3; i++) g_behind[i] += (goal[i] - g_behind[i]) * k;
    PutCamera(base, camera, g_behind, pitch, s.yaw, true);
}

#endif
