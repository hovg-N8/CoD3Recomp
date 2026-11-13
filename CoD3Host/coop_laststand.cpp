// Co-op v40: World at War's rules, kept by the host (see coop.h and
// docs/COOP-V40-DESIGN.md "World at War's co-op rules"):
// - a player brought to nothing by the enemy (a soldier's shot, a grenade,
//   a missile, a vehicle) goes down instead of dying: last stand. He keeps
//   health 1 and takes no more damage while down, and bleeds out in 30 s
//   (COD3_COOP_BLEEDOUT);
// - the other player brings him back by holding Use beside him for 3 s
//   (COD3_COOP_REVIVE): full health again;
// - the mission fails (back to the checkpoint) when the other player is down
//   too, when a downed player bleeds out, or when a player dies of anything
//   last stand does not take (a fall, a trigger, crushing, suicide);
// - on the joining copy no damage is ever made: the host's is the only one.
// Alone (no friend, or no second player yet) the campaign is as ever.
#ifndef COD3_MP

#include "coop.h"
#include "overlay.h"
#include "input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

extern "C" PPC_FUNC(__imp__sub_82564888);   // G_Damage
extern "C" PPC_FUNC(__imp__sub_8255F320);   // a player's die

void Probe40Damage(const uint8_t* base, uint32_t target, uint32_t attacker, int32_t damage, int32_t means, uint32_t flags, int32_t before);   // coop_probe.cpp

namespace Coop
{
    namespace
    {
        struct Player { bool down = false; double downAt = -1, bleedUntil = -1, reviveFrom = -1, safeUntil = -1; float revive = 0; uint32_t means = 0; };
        bool g_allDown = false;       // (0.5.869) everyone is down: back to the checkpoint, done in the frame
        Player g_p[2];
        bool g_injecting = false;     // a hit given on purpose (not dropped on the joining copy)
        // competitive co-op's scores (values ours): 10 a kill, 50 a revive, 25 lost going down;
        // kept through restarts from a checkpoint, new for a new mission
        struct Score { int16_t kills = 0, revives = 0, downs = 0; };
        Score g_score[2];
        std::string g_scoreMap;
        int ScoreOf(const Score& c) { return c.kills * 10 + c.revives * 50 - c.downs * 25; }
        // (0.5.869, the user's rule) a player brought to nothing is down for 10 s (0.5.875; COD3_COOP_RESPAWN), then comes back
        // beside the other player with full health; with everyone down, both go back to the checkpoint. (0.5.875) The other player holding Use beside him for 3 s revives him where he lies.
        double Bleedout() { static const double v = [] { const char* t = getenv("COD3_COOP_RESPAWN"); return t ? std::clamp(atof(t), 1.0, 300.0) : 10.0; }(); return v; }
        float ReviveRadius() { static const float v = [] { const char* t = getenv("COD3_COOP_REVIVERADIUS"); return t ? std::clamp(float(atof(t)), 30.0f, 400.0f) : 80.0f; }(); return v; }
        double ReviveTime() { static const double v = [] { const char* t = getenv("COD3_COOP_REVIVE"); return t ? std::clamp(atof(t), 0.5, 20.0) : 3.0; }(); return v; }
        int Who(const uint8_t* base, uint32_t e) { if (!e) return -1; if (e == Me(base)) return 0; if (e == SecondPlayer()) return 1; return -1; }
        uint32_t PlayerEnt(const uint8_t* base, int who) { return who == 0 ? Me(base) : SecondPlayer(); }
        bool TwoPlayers(const uint8_t* base)
        {
            const uint32_t s = SecondPlayer();
            return IsHost() && PeerConnected() && s && IndexOf(base, s) >= 0 && base[s] == 1;
        }
        // the state, to the joining copy (and kept there for its HUD)
        struct State { uint8_t down[2]; uint8_t pad[2]; float bleed[2]; float revive[2]; int16_t kills[2], revives[2], downs[2]; uint8_t pad2[4]; };
        State g_shown{};
        void SendState(const double now)
        {
            State s{};
            for (int k = 0; k < 2; k++)
            {
                s.down[k] = g_p[k].down ? 1 : 0;
                s.bleed[k] = g_p[k].down ? float(std::max(0.0, g_p[k].bleedUntil - now)) : 0.0f;
                s.revive[k] = g_p[k].revive;
                s.kills[k] = g_score[k].kills; s.revives[k] = g_score[k].revives; s.downs[k] = g_score[k].downs;
            }
            SendReliable(R_LASTSTAND, &s, sizeof s);
        }
        void GoDown(uint8_t* base, int who, uint32_t means)
        {
            Player& p = g_p[who];
            p.down = true; p.downAt = Now(); p.bleedUntil = Now() + Bleedout(); p.revive = 0; p.reviveFrom = -1; p.means = means;
            g_score[who].downs++;
            // the AI no longer aims at him (WaW's ignoreme): the entity's FL_NOTARGET (+632 bit 2, the notarget cheat's)
            if (const uint32_t e = PlayerEnt(base, who)) Guest::Write32(base, e + 632, Guest::Read32(base, e + 632) | 2u);
            printf("coop: last stand: %s is down (means %u): back in %.0f s\n", who == 0 ? "the host" : "the joining player", means, Bleedout()); fflush(stdout);
            if (who == 0) Notice("You are down! Back in a few seconds.", 4000);
            else Notice("Your friend is down! Stay alive: they are back in a few seconds.", 4000);
            SendState(Now());
            (void)base;
        }
        void MissionFailed(PPCContext& ctx, uint8_t* base, int who, const char* why)
        {
            printf("coop: last stand: mission failed: %s\n", why); fflush(stdout);
            for (Player& p : g_p) p = Player{};
            uint8_t m[64] = { 1 };
            strncpy(reinterpret_cast<char*>(m + 1), why, 62);
            SendReliable(R_MISSION, m, sizeof m);
            // the game's own death of the host's player: its mission failed screen and the checkpoint
            const uint32_t me = Me(base);
            if (!me) return;
            Guest::Write32(base, me + 776, 0);
            PPCContext c = CallCtx(ctx);
            c.r3.u64 = me; c.r4.u64 = me; c.r5.u64 = me; c.r6.u64 = 100000; c.r7.u64 = 25; c.r8.u64 = 0; c.r9.u64 = 0; c.r10.u64 = 0;
            __imp__sub_8255F320(c, base);
            (void)who;
        }
        // which way a player's Use is held: the host's from his record, the joining player's from his command
        bool UseHeld(const uint8_t* base, int who);
    }
    bool PlayerDown(int who) { return who >= 0 && who < 2 && (IsJoin() ? g_shown.down[who] != 0 : g_p[who].down); }
    // a downed player lies down (WaW's last stand: prone, crawling): standing and crouching not allowed
    // (the player's +44 flags 0x100000 and 0x200000, the scripts' allowstand / allowcrouch: set, not allowed)
    void DownStance(uint8_t* base, uint32_t player, bool down)
    {
        const uint32_t rec = Record(base, player);
        if (!rec) return;
        constexpr uint32_t Flags = 0x00300000u;
        static uint32_t ours[2] = {};   // the bits we set (not the scripts'), by player slot (0 this copy's)
        const int slot = player == Me(base) ? 0 : 1;
        const uint32_t now = Guest::Read32(base, rec + 44);
        if (down) { const uint32_t add = Flags & ~now; if (add) { ours[slot] |= add; Guest::Write32(base, rec + 44, now | add); } }
        else if (ours[slot]) { Guest::Write32(base, rec + 44, now & ~ours[slot]); ours[slot] = 0; }
    }
    void InjectDamage(PPCContext& ctx, uint8_t* base, uint32_t target, int32_t damage, int32_t means)
    {
        PPCContext c = CallCtx(ctx);
        c.r3.u64 = target; c.r4.u64 = 0; c.r5.u64 = 0; c.r6.u64 = 0; c.r7.u64 = 0; c.r8.u64 = uint32_t(damage); c.r9.u64 = 0; c.r10.u64 = uint32_t(means);
        g_injecting = true;
        CallGuest(c, base, 0x82564888);
        g_injecting = false;
    }
    double MissionTime() { static double inAt = -1; static uint32_t gen = 0; const bool in = InMission(Guest::Base); if (!in) { inAt = -1; return -1; } if (inAt < 0 || gen != MissionCount()) { inAt = Now(); gen = MissionCount(); } return Now() - inAt; }
    double TestTime(const char* env) { const char* t = getenv(env); return t ? atof(t) : -1.0; }
    void LastStandMissionStart()
    {
        for (Player& p : g_p) p = Player{};
        const State keep = g_shown;
        g_shown = State{};
        memcpy(g_shown.kills, keep.kills, sizeof keep.kills); memcpy(g_shown.revives, keep.revives, sizeof keep.revives); memcpy(g_shown.downs, keep.downs, sizeof keep.downs);
        if (MyMap() != g_scoreMap) { g_scoreMap = MyMap(); for (Score& c : g_score) c = Score{}; g_shown = State{}; }
    }

    void LastStandHud(HudState& h)
    {
        h.bleedTotal = float(Bleedout());
        const double now = Now();
        for (int k = 0; k < 2; k++)
        {
            if (IsHost())
            {
                h.down[k] = g_p[k].down; h.bleed[k] = g_p[k].down ? float(std::max(0.0, g_p[k].bleedUntil - now)) : 0.0f; h.revive[k] = g_p[k].revive;
                h.kills[k] = g_score[k].kills; h.revives[k] = g_score[k].revives; h.downs[k] = g_score[k].downs;
            }
            else
            {
                h.down[k] = g_shown.down[k] != 0; h.bleed[k] = g_shown.bleed[k]; h.revive[k] = g_shown.revive[k];
                h.kills[k] = g_shown.kills[k]; h.revives[k] = g_shown.revives[k]; h.downs[k] = g_shown.downs[k];
            }
            h.score[k] = h.kills[k] * 10 + h.revives[k] * 50 - h.downs[k] * 25;
        }
        // the points as they come: this copy's player's score as it changes
        static int was = 0; static int popup = 0; static double until = 0; static std::string map;
        if (map != MyMap()) { map = MyMap(); was = h.score[h.me]; }
        if (h.score[h.me] != was) { popup = (now < until ? popup : 0) + (h.score[h.me] - was); until = now + 1.5; was = h.score[h.me]; }
        h.points[h.me] = popup; h.pointsUntil = until;
    }

    bool LastStandTakes(PPCContext& ctx, uint8_t* base, uint32_t player, int32_t damage, int32_t means, uint32_t attacker)
    {
        (void)ctx; (void)damage;
        const int who = Who(base, player);
        if (who < 0 || !TwoPlayers(base)) return false;
        if (g_p[who].down) return true;   // (already down: no second death)
        // (0.5.869) whatever killed him (the enemy, a fall, a trigger, his friend): down, and back in 5 s, while
        // the other player is up. The other down too: everyone is down, back to the checkpoint for both (done in
        // the frame, by the host's player's own death: the game's mission failed and its checkpoint).
        (void)attacker;
        const int other = 1 - who;
        if (g_p[other].down)
        {
            printf("coop: last stand: both players down (%s by means %d): back to the checkpoint\n", who == 0 ? "the host" : "the joining player", means); fflush(stdout);
            g_allDown = true;
            return true;
        }
        GoDown(base, who, uint32_t(means));
        return true;
    }

    void LastStandReceive(PPCContext& ctx, uint8_t* base, RelKind kind, const uint8_t* data, size_t size)
    {
        (void)ctx; (void)base;
        if (IsJoin() && kind == R_LASTSTAND && size >= sizeof(State))
        {
            State s; memcpy(&s, data, sizeof s);
            if (s.down[1] && !g_shown.down[1]) Notice("You are down! Back in a few seconds.", 4000);
            if (!s.down[1] && g_shown.down[1]) Notice("You are back in.", 3000);
            if (s.down[0] && !g_shown.down[0]) Notice("Your friend is down! Stay alive: they are back in a few seconds.", 4000);
            if (!s.down[0] && g_shown.down[0]) Notice("Your friend is back up.", 3000);
            g_shown = s;
        }
        if (IsJoin() && kind == R_MISSION && size >= 2 && data[0] == 1)
        {
            char why[64] = {}; memcpy(why, data + 1, std::min<size_t>(size - 1, 62));
            printf("coop: last stand: the host says: mission failed: %s\n", why); fflush(stdout);
            Notice(std::string("Mission failed: ") + why, 6000);
            g_shown = State{};
        }
    }

    void LastStandFrame(PPCContext& ctx, uint8_t* base)
    {
        // (0.5.886) DIAGNOSTIC: the host's player's health as it changes (the entity's +776, +780, the record's +272
        // and the record's first words), to see how a grenade kills him: t885b, no G_Damage and no die were called
        if (IsHost())
        {
            static int32_t was[3] = { -99999, -99999, -99999 }; static uint32_t wasType = 99, wasW0 = 0; static int told = 0;
            const uint32_t me = Me(base), rec = Record(base, me);
            if (me && rec)
            {
                const int32_t now[3] = { int32_t(Guest::Read32(base, me + 776)), int32_t(Guest::Read32(base, me + 780)), int32_t(Guest::Read32(base, rec + 272)) };
                const uint32_t w0 = Guest::Read32(base, rec + 4 * 3), flags = Guest::Read32(base, me + 632);
                if ((now[0] != was[0] || now[2] != was[2] || base[me] != wasType) && (now[0] <= 0 || now[2] <= 0 || base[me] != wasType) && told < 100)
                {
                    told++;
                    printf("coop: health: the host's entity %d of %d (record %d), type %u, record +12 %08X, +44 %08X, flags %08X, takedamage %u, at %.1f s (level %d ms)\n", now[0], now[1], now[2], base[me], w0, Guest::Read32(base, rec + 44), flags, Guest::Read32(base, me + 624), Now(), int32_t(Guest::Read32(base, Level + 156)));
                    fflush(stdout);
                }
                memcpy(was, now, sizeof was); wasType = base[me]; wasW0 = w0;
            }
        }
        // DIAGNOSTIC: the buttons the game keeps in the player's record (+1352), when they change (to find Use)
        {
            static uint32_t was = 0; static int told = 0;
            const uint32_t rec = Record(base, Me(base));
            const uint32_t b = rec ? Guest::Read32(base, rec + 1352) : 0;
            if (b != was && told < 60) { told++; printf("coop: buttons %08X -> %08X at %.1f s in the mission\n", was, b, MissionTime()); fflush(stdout); }
            // (0.5.882) the record's words that change in the 2.5 s after a button changes (not the ones that change
            // all the time): to map the weapon's state, the ammunition, on the ground or in the air
            {
                static uint32_t prev[556], first[556]; static uint16_t changes[556]; static double until = -1; static uint32_t why = 0; static int reports = 0;
                if (rec && b != was && until < 0 && reports < 40 && ((b | was) & 0x17u)) { for (int i = 0; i < 556; i++) { prev[i] = first[i] = Guest::Read32(base, rec + 4u * i); changes[i] = 0; } until = Now() + 2.5; why = b ? b : was; }
                else if (rec && until >= 0)
                {
                    for (int i = 0; i < 556; i++) { const uint32_t w = Guest::Read32(base, rec + 4u * i); if (w != prev[i]) { if (changes[i] < 60000) changes[i]++; prev[i] = w; } }
                    if (Now() >= until)
                    {
                        until = -1; reports++;
                        std::string line; int n = 0;
                        for (int i = 0; i < 556 && n < 40; i++) if (changes[i] >= 1 && changes[i] <= 8) { char t[64]; snprintf(t, sizeof t, " +%d:%08X>%08X(x%u)", 4 * i, first[i], prev[i], changes[i]); line += t; n++; }
                        printf("coop: record after buttons %08X:%s\n", why, line.c_str()); fflush(stdout);
                    }
                }
            }
            // (0.5.883) the weapon's state (+136) as it changes, with the clip (+668) and the weapon's animation (+1280)
            {
                static uint32_t wsWas = 0xFFFFFFFFu; static int toldWs = 0;
                const uint32_t ws = rec ? Guest::Read32(base, rec + 136) : 0;
                if (rec && ws != wsWas && toldWs < 300) { toldWs++; printf("coop: weapon state %u -> %u (clip %u, weapon animation %03X, buttons %08X, velocity up %.0f)\n", wsWas, ws, Guest::Read32(base, rec + 668), Guest::Read32(base, rec + 1280), b, F(base, rec + 24)); fflush(stdout); }
                wsWas = ws;
            }
            was = b;
        }
        if (!IsHost()) return;
        const double now = Now();
        // COD3_COOP_TESTDOWN=t: the joining player goes down t s into the mission (a test of last stand)
        {
            static uint32_t done = 0;
            const double t = TestTime("COD3_COOP_TESTDOWN");
            if (t >= 0 && done != MissionCount() + 1 && MissionTime() >= t && TwoPlayers(base) && !g_p[1].down) { done = MissionCount() + 1; printf("coop: test: the joining player goes down now\n"); fflush(stdout); GoDown(base, 1, 2);
                // COD3_COOP_TESTNEAR=1: the host's player put beside him (a test of revive)
                if (getenv("COD3_COOP_TESTNEAR"))
                {
                    const uint32_t me = Me(base), rec = Record(base, me), two = SecondPlayer();
                    if (rec && two) { SetF(base, rec + 0, F(base, two + 336) + 40.0f); SetF(base, rec + 4, F(base, two + 340)); SetF(base, rec + 8, F(base, two + 344) + 4.0f); printf("coop: test: the host put beside him\n"); fflush(stdout); }
                }
            }
        }
        // (0.5.869) COD3_COOP_TESTDOWN0=t: the host goes down t s into the mission (tests of the respawn; with
        // COD3_COOP_TESTDOWN at the same time: everyone down)
        {
            static uint32_t done = 0;
            const double t = TestTime("COD3_COOP_TESTDOWN0");
            if (t >= 0 && done != MissionCount() + 1 && MissionTime() >= t && TwoPlayers(base))
            {
                done = MissionCount() + 1; printf("coop: test: the host goes down now\n"); fflush(stdout);
                if (g_p[1].down) g_allDown = true; else if (!g_p[0].down) GoDown(base, 0, 2);
            }
        }
        static double sentAt = 0;
        if (!TwoPlayers(base)) { for (Player& p : g_p) p = Player{}; return; }
        if (g_allDown) { g_allDown = false; MissionFailed(ctx, base, 0, "everyone is down"); return; }
        bool changed = false;
        DownStance(base, Me(base), g_p[0].down);
        if (const uint32_t two = SecondPlayer()) DownStance(base, two, g_p[1].down);
        for (int who = 0; who < 2; who++)
        {
            Player& p = g_p[who];
            const uint32_t e = PlayerEnt(base, who);
            if (!p.down || !e) continue;
            // kept at 1 while down
            if (int32_t(Guest::Read32(base, e + 776)) < 1) Guest::Write32(base, e + 776, 1);
            if (now >= p.bleedUntil && p.reviveFrom < 0)   // (0.5.875: a revive under way holds the clock)
            {
                // (0.5.869) back in beside the other player, whole, and safe for 3 s
                const int32_t full = int32_t(Guest::Read32(base, e + 780));
                Guest::Write32(base, e + 776, uint32_t(full > 0 ? full : 100));
                Guest::Write32(base, e + 632, Guest::Read32(base, e + 632) & ~2u);
                p = Player{}; p.safeUntil = now + 3.0;
                HostRespawn(ctx, base, who);
                printf("coop: last stand: %s is back in beside %s (health %d)\n", who == 0 ? "the host" : "the joining player", who == 0 ? "the joining player" : "the host", full); fflush(stdout);
                if (who == 0) Notice("You are back in.", 3000); else Notice("Your friend is back in.", 3000);
                changed = true;
                continue;
            }
            // the other player holding Use beside him
            const int other = 1 - who;
            const uint32_t o = PlayerEnt(base, other);
            bool near = false;
            if (o && !g_p[other].down)
            {
                const float dx = F(base, o + 336) - F(base, e + 336), dy = F(base, o + 340) - F(base, e + 340), dz = F(base, o + 344) - F(base, e + 344);
                const float flat = sqrtf(dx * dx + dy * dy);
                near = flat < ReviveRadius() && fabsf(dz) < 72.0f;
                static double toldAt = 0;
                if (now - toldAt > 2.0) { toldAt = now; printf("coop: last stand: %s down at %.0f %.0f %.0f, %s at %.0f %.0f %.0f: %.0f apart (%.0f up), use %d, %.0f s left\n", who == 0 ? "the host" : "the joining player", F(base, e + 336), F(base, e + 340), F(base, e + 344), other == 0 ? "the host" : "the joining player", F(base, o + 336), F(base, o + 340), F(base, o + 344), flat, dz, UseHeld(base, other) ? 1 : 0, p.bleedUntil - now); fflush(stdout); }
            }
            if (near && UseHeld(base, other))
            {
                if (p.reviveFrom < 0) { p.reviveFrom = now; printf("coop: last stand: %s is reviving %s\n", other == 0 ? "the host" : "the joining player", who == 0 ? "the host" : "the joining player"); fflush(stdout); changed = true; }
                p.revive = float(std::min(1.0, (now - p.reviveFrom) / ReviveTime()));
                if (p.revive >= 1.0f)
                {
                    const int32_t full = int32_t(Guest::Read32(base, e + 780));
                    Guest::Write32(base, e + 776, uint32_t(full > 0 ? full : 100));
                    p = Player{}; p.safeUntil = now + 3.0;
                    g_score[other].revives++;
                    Guest::Write32(base, e + 632, Guest::Read32(base, e + 632) & ~2u);
                    printf("coop: last stand: %s revived\n", who == 0 ? "the host" : "the joining player"); fflush(stdout);
                    if (who == 0) Notice("You have been revived.", 3000); else Notice("You revived your friend.", 3000);
                    changed = true;
                    continue;
                }
            }
            else if (p.reviveFrom >= 0) { p.reviveFrom = -1; p.revive = 0; changed = true; }
            // (the HUD shows the rest: coop_hud.cpp)
        }
        if (changed || now - sentAt > 0.5) { sentAt = now; SendState(now); }
    }
}

namespace Coop
{
    namespace
    {
        bool UseHeld(const uint8_t* base, int who)
        {
            const uint32_t e = PlayerEnt(base, who);
            const uint32_t rec = Record(base, e);
            if (!rec) return false;
            // the buttons the game keeps in the record (+1352; bit 0 attack): Use is found in the first test
            // COD3_COOP_TESTUSE="a b": the host holds Use from a to b s into the mission (a test of revive)
            if (who == 0)
            {
                static double a = -1, b = -1; static bool parsed = false;
                if (!parsed) { parsed = true; if (const char* t = getenv("COD3_COOP_TESTUSE")) sscanf(t, "%lf %lf", &a, &b); }
                const double mt = MissionTime();
                if (a >= 0 && mt >= a && mt <= b) return true;
            }
            // (0.5.875) Use is the pad's X (the keyboard's Use and Reload are X too): read from each player's own pad,
            // the joining player's sent in his commands (the record's +1352 bits never showed a Use)
            return who == 0 ? (Input::Buttons() & 0x4000) != 0 : JoinerUseHeld();
        }
    }
}

using namespace Coop;

// G_Damage(target, inflictor, attacker, dir, point, damage, flags, means, ...)
PPC_FUNC(sub_82564888)
{
    const uint32_t target = ctx.r3.u32, attacker = ctx.r5.u32;
    const int32_t damage = int32_t(ctx.r8.u32), means = int32_t(ctx.r10.u32);
    const int32_t before = Valid(target) ? int32_t(Guest::Read32(base, target + 776)) : 0;
    if (IsJoin() && JoinFrozen() && !g_injecting)
    {
        // the host's damage is the only damage; but what our own game does to me (a fall, a trigger)
        // goes to the host, where it is given to me
        const uint32_t me = Me(base);
        if (target == me && me && (!attacker || !Valid(attacker) || attacker == me || base[attacker] == 0) && damage > 0)
        {
            int32_t d[2] = { damage, means };
            SendReliable(R_SELFHIT, d, sizeof d);
        }
        return;
    }
    if (IsHost())
    {
        const int who = Who(base, target);
        // (0.5.924) NO FRIENDLY FIRE between the two players (World at War's co-op default): since 0.5.915 the joining
        // player's shots and grenades are real in the host's world, and t923a he shot the host (150, 150, 176). A
        // player's own grenade still hurts himself. COD3_COOP_FF=1: friendly fire on.
        {
            static const bool ff = []() { const char* t = getenv("COD3_COOP_FF"); return t && t[0] == '1'; }();
            const int by = Who(base, attacker);
            if (!ff && who >= 0 && by >= 0 && by != who && TwoPlayers(base))
            {
                static int told = 0;
                if (told++ < 20) { printf("coop: host: %s hit by %s for %d (means %d): no friendly fire\n", who == 0 ? "the host" : "the joining player", by == 0 ? "the host" : "the joining player", damage, means); fflush(stdout); }
                return;
            }
        }
        if (who >= 0 && !g_p[who].down && Now() < g_p[who].safeUntil && TwoPlayers(base)) return;   // (0.5.869: just back in)
        if (who >= 0 && g_p[who].down && TwoPlayers(base))
        {
            // (down: no more damage; how often the AI still shoots at him, for the log)
            static int shots = 0; static double toldAt = 0;
            shots++;
            if (Now() - toldAt > 5) { toldAt = Now(); printf("coop: last stand: %d hits on a downed player so far\n", shots); fflush(stdout); }
            return;
        }
        // (v44e) a player crushed by a vehicle of the scripts (the landing craft, a truck he is set down
        // beside): not hurt. Two players where the level was made for one: the scripts' vehicles do not know
        // of the second, and a crush is outside last stand: it failed the mission at once, again and again
        if (who >= 0 && TwoPlayers(base) && means == 32 && attacker && Valid(attacker) && attacker != target && base[attacker] != 1) return;
        // client 1 hurt by the world here (his falls are his own game's: R_SELFHIT brings them)
        if (who == 1 && !g_injecting && (!attacker || !Valid(attacker) || attacker == target || base[attacker] == 0)) return;
    }
    // (0.5.907) THE JOINING PLAYER'S HITS ON THE LEVEL'S THINGS. The level's scripts are compiled code that acts for
    // the host's player only (as with touches, 0.5.897): a damage trigger or a target that the joining player shoots
    // or blows up is told as the host's player's doing, so the mission hears it. Soldiers and players keep their real
    // attacker (who to shoot back at, the scores, team damage). COD3_COOP_DMGASHOST=0: as before.
    if (IsHost() && attacker && attacker == SecondPlayer() && Valid(target) && Who(base, target) < 0 && base[target] != 11 && Me(base))
    {
        static const bool off = []() { const char* t = getenv("COD3_COOP_DMGASHOST"); return t && t[0] == '0'; }();
        static int told = 0;
        if (told++ < 80) { printf("coop: host: the joining player hit %s (entity %d, type %u) for %d, means %d%s\n", BrocText(base, target + 564).c_str(), IndexOf(base, target), base[target], damage, means, off ? "" : ": told as the host's player's"); fflush(stdout); }
        if (!off) ctx.r5.u64 = Me(base);
    }
    // (0.5.887) With two players a player never dies INSIDE the game's damage: he takes the hit behind a shield
    // of health, what it took is measured, and a hit that would have taken him to nothing is the rule's (down,
    // revive, back in). t885b: a grenade at the host's feet failed the mission with neither this hook's line
    // after the call nor the die hook's: the game's own death left this call without coming back, by a way
    // that is not sub_8255F320. Now there is no death in there to leave by.
    int shielded = -1;
    constexpr int32_t Shield = 1000000;
    if (IsHost() && TwoPlayers(base) && before > 0 && before < Shield && damage > 0)
    {
        shielded = Who(base, target);
        if (shielded >= 0) Guest::Write32(base, target + 776, uint32_t(before + Shield));
    }
    __imp__sub_82564888(ctx, base);
    if (shielded >= 0)
    {
        const int32_t held = int32_t(Guest::Read32(base, target + 776));
        const int32_t took = std::clamp(before + Shield - held, 0, Shield);
        int32_t left = before - took;
        // (0.5.888) every hit a player takes behind the shield, for the log (t887a: five grenades at the host's
        // feet, no death and no line: was he hit at all?)
        { static int told = 0; if (told++ < 120) { printf("coop: shield: %s hit %d (means %d, flags %08X, by %s type %u, inflictor %s): took %d of %d, %d left\n", shielded == 0 ? "the host" : "the joining player", damage, means, ctx.r9.u32,
              attacker && Valid(attacker) ? BrocText(base, attacker + 564).c_str() : "nothing", attacker && Valid(attacker) ? base[attacker] : 0u, ctx.r4.u32 && Valid(ctx.r4.u32) ? BrocText(base, ctx.r4.u32 + 564).c_str() : "nothing", took, before, left); fflush(stdout); } }
        if (left <= 0)
        {
            printf("coop: last stand: %s would have died here (hit %d, took %d of %d, means %d, by %s)\n", shielded == 0 ? "the host" : "the joining player", damage, took, before, means, attacker && Valid(attacker) ? BrocText(base, attacker + 564).c_str() : "nothing"); fflush(stdout);
            LastStandTakes(ctx, base, target, damage, means, attacker);
            left = 1;
        }
        Guest::Write32(base, target + 776, uint32_t(left));
    }
    Probe40Damage(base, target, attacker, damage, means, ctx.r9.u32, before);
    if (IsHost())
    {
        const int who = Who(base, target);
        // (0.5.885) every hit on a player that takes him to nothing, and big ones: which way a death comes
        // (t884a: the host "killed by a grenade", the mission failed, and no line of the rule in the log)
        if (who >= 0)
        {
            const int32_t after = int32_t(Guest::Read32(base, target + 776));
            static int told = 0;
            if ((after <= 0 || damage >= 100) && told++ < 60)
            {
                printf("coop: damage: %s hit for %d (means %d, by %s type %u), health %d -> %d, down %d, two players %d\n", who == 0 ? "the host" : "the joining player", damage, means,
                       attacker && Valid(attacker) ? BrocText(base, attacker + 564).c_str() : "nothing", attacker && Valid(attacker) ? base[attacker] : 0u, before, after, g_p[who].down ? 1 : 0, TwoPlayers(base) ? 1 : 0);
                fflush(stdout);
            }
        }
        if (who >= 0 && g_p[who].down && int32_t(Guest::Read32(base, target + 776)) < 1) Guest::Write32(base, target + 776, 1);
        // a kill for the scores: an enemy soldier (axis) brought to nothing by one of the two players
        const int by = Who(base, attacker);
        if (by >= 0 && Valid(target) && base[target] == 11 && before > 0 && int32_t(Guest::Read32(base, target + 776)) <= 0 && TwoPlayers(base)
            && BrocText(base, target + 564).find("axis") != std::string::npos)
        {
            g_score[by].kills++;
            static int told = 0;
            if (told++ < 40) { printf("coop: score: %s killed %s (%d kills)\n", by == 0 ? "the host" : "the joining player", BrocText(base, target + 564).c_str(), g_score[by].kills); fflush(stdout); }
        }
        if (who == 1 && PeerConnected())
        {
            // the joining player was hit: his copy shows it (later: its own hit effects)
            struct { int32_t damage, means, health; float from[3]; } m{ damage, means, int32_t(Guest::Read32(base, target + 776)), {} };
            if (attacker && Valid(attacker)) for (int k = 0; k < 3; k++) m.from[k] = F(base, attacker + 336 + 4 * k);
            SendReliable(R_HIT, &m, sizeof m);
        }
    }
}

// a player's die (self, inflictor, attacker, damage, means, ...): last stand first
PPC_FUNC(sub_8255F320)
{
    // (v44k) the joining copy never kills its own player: only the host decides deaths and mission
    // failures (World at War: the client has no say in either). Its own death here reloaded its own
    // checkpoint under the frozen world: everything of the level spawned again, and it froze
    if (IsJoin() && (JoinFrozen() || PeerConnected()))
    {
        static int told = 0;
        if (told++ < 5) { printf("coop: join: this copy's own death of its player not run (the host's game decides)\n"); fflush(stdout); }
        if (ctx.r3.u32 && Valid(ctx.r3.u32)) Guest::Write32(base, ctx.r3.u32 + 776, 1);
        return;
    }
    // (v44i) crushed by a thing of the scripts (Island's landing craft: means 32, by a script_vehicle,
    // which is not entity type 10 here): no death and no last stand, his health back
    if (IsHost() && int32_t(ctx.r7.u32) == 32 && TwoPlayers(base) && Who(base, ctx.r3.u32) >= 0 && ctx.r5.u32 && Valid(ctx.r5.u32) && ctx.r5.u32 != ctx.r3.u32 && base[ctx.r5.u32] != 1)
    {
        static int told = 0;
        if (told++ < 5) { printf("coop: last stand: %s crushed by %s (type %u): not killed\n", Who(base, ctx.r3.u32) == 0 ? "the host" : "the joining player", BrocText(base, ctx.r5.u32 + 564).c_str(), base[ctx.r5.u32]); fflush(stdout); }
        const int32_t mx = int32_t(Guest::Read32(base, ctx.r3.u32 + 780));
        Guest::Write32(base, ctx.r3.u32 + 776, uint32_t(mx > 0 ? mx : 100));
        return;
    }
    if (IsHost())
    {
        // (0.5.885) every death of a player that comes here, and what the rule is asked with
        static int told = 0;
        if (told++ < 40)
        {
            const uint32_t s = SecondPlayer();
            printf("coop: die: entity %08X (the host's %08X, client 1's %08X) means %d damage %d by %08X, from %08X; peer %d, client 1 type %u\n", ctx.r3.u32, Me(base), s, int32_t(ctx.r7.u32), int32_t(ctx.r6.u32), ctx.r5.u32, uint32_t(ctx.lr), PeerConnected() ? 1 : 0, s && Valid(s) ? base[s] : 255u);
            fflush(stdout);
        }
    }
    if (IsHost() && LastStandTakes(ctx, base, ctx.r3.u32, int32_t(ctx.r6.u32), int32_t(ctx.r7.u32), ctx.r5.u32))
    {
        Guest::Write32(base, ctx.r3.u32 + 776, 1);
        return;
    }
    __imp__sub_8255F320(ctx, base);
}

// (0.5.886) DIAGNOSTIC: the two other ways into a death, logged with who called
extern "C" PPC_FUNC(__imp__sub_8255EF30);
PPC_FUNC(sub_8255EF30)
{
    if (IsHost()) { static int told = 0; if (told++ < 20) { printf("coop: death routine sub_8255EF30: r3 %08X r4 %08X r5 %08X, from %08X\n", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, uint32_t(ctx.lr)); fflush(stdout); } }
    __imp__sub_8255EF30(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_8255F488);
PPC_FUNC(sub_8255F488)
{
    if (IsHost()) { static int told = 0; if (told++ < 20) { printf("coop: sub_8255F488: r3 %08X r4 %08X r5 %08X r6 %08X, from %08X\n", ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, uint32_t(ctx.lr)); fflush(stdout); } }
    __imp__sub_8255F488(ctx, base);
}

#endif
