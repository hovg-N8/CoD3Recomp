// Co-op v40, the host's side (see coop.h, docs/COOP-V40-DESIGN.md):
// - client 1, the joining player as a second real player of this world
//   (COOP-SECOND-SLOT.md's steps 1 and 2: his own client, entity, record and
//   sentient, so the AI aims at him; his own commands and view; his place
//   taken from his copy, which moves him without lag);
// - the snapshots that the joining copy draws this world from;
// - the config strings and server commands the host's player gets, given to
//   the joining copy too; the HUD elements;
// - the two players' bodies (script_models dressed as soldiers).
#ifndef COD3_MP

#include "coop.h"
#include "overlay.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" PPC_FUNC(__imp__sub_82513C98);   // SV_SetConfigstring(index, text)
extern "C" PPC_FUNC(__imp__sub_82512970);   // a reliable command into a client's buffer (client, index, text)

namespace Coop
{
    namespace
    {
        constexpr uint32_t Svs = 0x829C39E0;
        constexpr uint32_t ClientSize = 4832;
        constexpr uint32_t LocalClients = 0x82A57900;   // the client side's per local player array (7280 bytes)
        uint32_t CommandSlot(const uint8_t* base) { return LocalClients + 5752 + (Guest::Read32(base, LocalClients + 5848) & 1) * 48; }

        // ---- the joining player's input (K_CMD) ----
        struct CmdIn { uint32_t seq; uint8_t cmd[48]; float view[3]; float origin[3]; float velocity[3]; uint32_t stance; uint32_t flags; char map[32]; };
        CmdIn g_cmd{};
        double g_cmdAt = -1;
        bool g_hold = false; float g_holdPos[3] = {}; double g_holdAt = -1;
        uint64_t g_cmds = 0;

        // ---- client 1 ----
        uint32_t g_client1 = 0, g_ent = 0, g_rec = 0, g_index = 0;
        uint32_t g_gen = 0;          // the mission count he was made in
        double g_inSince = -1;
        uint32_t g_clientBlock = 0;  // our two-client block (svs +8)

        // where the host's player has stood lately (twice a second), for room beside him
        float g_trail[24][3] = {}; int g_trailCount = 0, g_trailNext = 0; double g_trailAt = -10, g_sceneEndAt = -1;
        void Trail(const uint8_t* base)
        {
            const uint32_t me = Me(base);
            if (!me || Now() - g_trailAt < 0.5) return;
            g_trailAt = Now();
            // (0.5.960) no room taken while a scripted scene plays (a ride: the scripts carry him) or while he moves up or down
            const uint32_t rec = Record(base, me);
            // (0.5.993) a scripted scene is a break in the trail: what he stood on before it is no room after it.
            // t991a: Saint-Lo's wall carried him 185 over it (under the 600 below); the room made for the joining
            // player afterwards was a place from before the wall, on its far side.
            static bool sceneSince = false;
            if (Guest::Read32(base, 0x825F51A0) != 0) { sceneSince = true; g_sceneEndAt = -1; return; }
            if (sceneSince) { sceneSince = false; g_trailCount = 0; g_sceneEndAt = Now(); }
            if (rec && fabsf(F(base, rec + 24)) >= 4.0f) return;
            if (g_trailCount)
            {
                // (v45u) moved far at once (a checkpoint, a mission started again): what was before is no room now
                const float* q = g_trail[(g_trailNext + 23) % 24];
                const float dx = F(base, me + 336) - q[0], dy = F(base, me + 340) - q[1], dz = F(base, me + 344) - q[2];
                if (dx * dx + dy * dy + dz * dz > 600.0f * 600.0f) g_trailCount = 0;
            }
            for (int k = 0; k < 3; k++) g_trail[g_trailNext][k] = F(base, me + 336 + 4 * k);
            g_trailNext = (g_trailNext + 1) % 24; g_trailCount = std::min(g_trailCount + 1, 24);
        }
        bool Holding();   // (v44e, below)
        void RoomBeside(const uint8_t* base, uint32_t me, float out[3])
        {
            const float h[3] = { F(base, me + 336), F(base, me + 340), F(base, me + 344) };
            for (int n = 1; n <= g_trailCount; n++)
            {
                const float* p = g_trail[(g_trailNext - n + 48) % 24];
                const float dx = p[0] - h[0], dy = p[1] - h[1];
                if (fabsf(p[2] - h[2]) > 20.0f) continue;   // (v45u: another floor, or a stair: Mayenne, he came in under the floor and fell out of the world)
                if (dx * dx + dy * dy >= 56.0f * 56.0f) { out[0] = p[0]; out[1] = p[1]; out[2] = p[2] + 24.0f; return; }
            }
            const float yaw = F(base, me + 356) * 0.0174533f;
            // (0.5.993) just after a scripted scene, IN FRONT of him: the scene ends with him facing the way on, and
            // behind him is what it carried him over (Saint-Lo's wall)
            if (g_sceneEndAt >= 0 && Now() - g_sceneEndAt < 30.0) { out[0] = h[0] + 56.0f * cosf(yaw); out[1] = h[1] + 56.0f * sinf(yaw); out[2] = h[2] + 24.0f; return; }
            out[0] = h[0] - 56.0f * cosf(yaw); out[1] = h[1] - 56.0f * sinf(yaw); out[2] = h[2] + 24.0f;   // (v45u: a little above: a short drop, never inside the floor)
        }
        void MakeSecondPlayer(PPCContext& ctx, uint8_t* base)
        {
            const uint32_t me = Me(base);
            const uint32_t old = Guest::Read32(base, Svs + 8);
            if (!Valid(old) || !me) return;
            printf("coop: host: making the second player (the host's player %08X, record %08X)\n", me, Record(base, me)); fflush(stdout);
            // 1. two clients: a new block, client 0 copied in (or ours, kept, after a restart)
            uint32_t block = old;
            if (old != g_clientBlock)
            {
                block = Guest::AllocatePhysical(2 * ClientSize);
                memset(base + block, 0, 2 * ClientSize);
                memmove(base + block, base + old, ClientSize);
                Guest::Write32(base, Svs + 8, block);
                g_clientBlock = block;
            }
            else memset(base + block + ClientSize, 0, ClientSize);
            const uint32_t cl1 = block + ClientSize;
            // 2. his entity and record
            PPCContext c = CallCtx(ctx);
            c.r3.u64 = uint64_t(-1);
            CallGuest(c, base, 0x82543568);   // G_Spawn
            const uint32_t ent = c.r3.u32;
            if (!Valid(ent)) { printf("coop: host: G_Spawn gave %08X: no second player\n", ent); fflush(stdout); return; }
            const uint32_t rec = GuestAlloc(2224);
            const uint32_t rec0 = Record(base, me), sentient0 = Guest::Read32(base, me + 536);
            memmove(base + rec, base + rec0, 2224);
            // the copy's words that are the host's own (his entity, his record, his client, his sentient): his
            auto fixOwn = [&](const char* when, uint32_t sentient1) { (void)when;
                for (uint32_t at = 0; at + 4 <= 2224; at += 4)
                {
                    const uint32_t w = Guest::Read32(base, rec + at);
                    uint32_t to = 0; const char* what = nullptr;
                    if (w == me) { to = ent; what = "entity"; }
                    else if (w == rec0) { to = rec; what = "record"; }
                    else if (w == old) { to = block + ClientSize; what = "client"; }
                    else if (w == sentient0 && Valid(sentient0)) { to = sentient1; what = "sentient"; }
                    if (!what) continue;
                    printf("coop: host: client 1's record +%u was the host's %s (%08X)%s\n", at, what, w, to ? "" : ": left"); fflush(stdout);
                    if (to) Guest::Write32(base, rec + at, to);
                }
            };
            fixOwn("copied", 0);
            base[ent] = base[me];   // type 1, a player
            Guest::Write32(base, ent + 564, Guest::Read32(base, me + 564));   // its class ('player')
            Guest::Write32(base, ent + 568, Guest::Read32(base, me + 568));
            if (const uint32_t b = Guest::Read32(base, me + 564); Valid(b)) Guest::Write16(base, b + 8, uint16_t(Guest::Read16(base, b + 8) + 1));   // (the class string shared: counted)
            const float yaw = F(base, me + 356) * 0.0174533f;
            const float place[3] = { F(base, me + 336) - 48.0f * sinf(yaw), F(base, me + 340) + 48.0f * cosf(yaw), F(base, me + 344) + 4.0f };
            // where he comes in: where his copy has him, unless that is in the host's player (their intros
            // end in one place): then where the host stood a moment ago, and his copy is told so
            float at[3] = { g_cmdAt >= 0 ? g_cmd.origin[0] : place[0], g_cmdAt >= 0 ? g_cmd.origin[1] : place[1], g_cmdAt >= 0 ? g_cmd.origin[2] : place[2] };
            if (Holding())
            {
                memcpy(at, g_holdPos, 12);
                uint8_t m[16] = { 2, 1 };   // (his copy put there: "room made")
                memcpy(m + 4, at, 12);
                SendReliable(R_MISSION, m, sizeof m);
            }
            {
                const float dx = at[0] - F(base, me + 336), dy = at[1] - F(base, me + 340), dz = at[2] - F(base, me + 344);
                if (dx * dx + dy * dy < 48.0f * 48.0f && fabsf(dz) < 80.0f)
                {
                    float room[3]; RoomBeside(base, me, room);
                    memcpy(at, room, 12);
                    uint8_t m[16] = { 2, 1 };
                    memcpy(m + 4, at, 12);
                    SendReliable(R_MISSION, m, sizeof m);
                    g_cmdAt = -1;   // (his next command says where he is now)
                    printf("coop: host: the joining player is where the host stands: he comes in at %.0f %.0f %.0f\n", at[0], at[1], at[2]); fflush(stdout);
                }
            }
            for (int k = 0; k < 3; k++) SetF(base, ent + 336 + 4 * k, at[k]);
            Guest::Write32(base, ent + 528, rec);
            const uint32_t table = Guest::Read32(base, 0x82A2AB40);
            // (the table's [2] is the world's entity, not a client's: his only while he connects, as was
            // here before 0.5.9xx: kept, the world was him, and the host's player slid off the ground and the
            // level's notifies to the world went astray - the intro never ended)
            const uint32_t world = Guest::Read32(base, table + 8);
            Guest::Write32(base, table + 8, ent);
            // 3. connected and in, as SV_SpawnServer does client 0
            const uint32_t was = Guest::Read32(base, 0x82A2A1C8);
            Guest::Write32(base, 0x82A2A1C8, 1);
            c = CallCtx(ctx);
            c.r3.u64 = uint64_t(2) << 32;   // the loopback address (type 2)
            c.r4.u64 = 0; c.r5.u64 = 0;
            CallGuest(c, base, 0x82514440);   // SVC_DirectConnect
            static const bool noBegin = getenv("COD3_COOP_C1NOBEGIN") != nullptr;   // (a test)
            c = CallCtx(ctx);
            c.r3.u64 = cl1; c.r4.u64 = 1; c.r5.u64 = 0;
            if (!noBegin) CallGuest(c, base, 0x82517458);   // SV_ClientEnterWorld (ClientBegin: his sentient, his health)
            Guest::Write32(base, 0x82A2A1C8, was);
            Guest::Write32(base, table + 8, world);
            fixOwn("in", Guest::Read32(base, ent + 536));
            g_client1 = cl1; g_ent = ent; g_rec = rec; g_index = uint32_t(IndexOf(base, ent));
            g_gen = MissionCount();
            const uint32_t sentient = Guest::Read32(base, ent + 536);
            printf("coop: host: the second player is entity %u (%08X), record %08X, client %08X (state %u), sentient %08X (team %u), health %d of %d, at %.0f %.0f %.0f\n",
                   g_index, ent, rec, cl1, Guest::Read32(base, cl1), sentient, Valid(sentient) ? Guest::Read32(base, sentient + 4) : 0,
                   int32_t(Guest::Read32(base, ent + 776)), int32_t(Guest::Read32(base, ent + 780)), F(base, ent + 336), F(base, ent + 340), F(base, ent + 344));
            fflush(stdout);
        }
        bool SecondPlayerAlive(const uint8_t* base)
        {
            return g_ent && EntityAt(base, g_index) == g_ent && base[g_ent] == 1 && Guest::Read32(base, g_ent + 528) == g_rec && Guest::Read32(base, Svs + 8) == g_clientBlock;
        }
        // (v44e) after the host's mission starts again, the joining player is moved beside the host and his
        // copy's old place is not used until his copy says it is there: else client 1 came back where he was
        // (on Island inside the landing craft) and was crushed at once, mission failed again, over and over
        bool Holding()
        {
            if (!g_hold) return false;
            const float dx = g_cmd.origin[0] - g_holdPos[0], dy = g_cmd.origin[1] - g_holdPos[1];
            // (v45) the joining copy loads the level again: held until it is frozen and beside the host (90 s at most)
            if ((g_cmdAt > g_holdAt && (g_cmd.flags & 2) && dx * dx + dy * dy < 96.0f * 96.0f) || Now() - g_holdAt > 90.0) { g_hold = false; printf("coop: host: the joining copy has its player beside the host again\n"); fflush(stdout); }
            return g_hold;
        }
        // where the joining player is, onto client 1 (record +0, entity +32, +336)
        void PlaceSecond(uint8_t* base)
        {
            if (!SecondPlayerAlive(base) || g_cmdAt < 0 || Now() - g_cmdAt > 1.0 || Holding()) return;
            for (int k = 0; k < 3; k++) { SetF(base, g_rec + 4 * k, g_cmd.origin[k]); SetF(base, g_rec + 16 + 4 * k, g_cmd.velocity[k]); }   // (+16 his velocity: no fall gathered between)
            for (const uint32_t at : { 32u, 336u }) for (int k = 0; k < 3; k++) SetF(base, g_ent + at + 4 * k, g_cmd.origin[k]);
            SetF(base, g_ent + 352, 0.0f); SetF(base, g_ent + 356, g_cmd.view[1]); SetF(base, g_ent + 360, 0.0f);
            static uint32_t toldEnt = 0;
            if (toldEnt != g_ent) { toldEnt = g_ent; printf("coop: host: client 1's contents %08X, flags %08X\n", Guest::Read32(base, g_ent + 324), Guest::Read32(base, g_ent + 16)); fflush(stdout); }
            // client 1 in the host's player (the joining copy's own intro ends where the host's does):
            // not solid until they are apart, or the host's player is pushed off (and bounces away)
            static uint32_t solidContents = 0x02000000u;
            const uint32_t now = Guest::Read32(base, g_ent + 324);
            if (now) solidContents = now;
            bool inside = false;
            if (const uint32_t me = Me(base))
            {
                const float dx = F(base, me + 336) - g_cmd.origin[0], dy = F(base, me + 340) - g_cmd.origin[1], dz = F(base, me + 344) - g_cmd.origin[2];
                inside = dx * dx + dy * dy < 34.0f * 34.0f && fabsf(dz) < 72.0f;
            }
            static const bool notSolid = getenv("COD3_COOP_C1NOTSOLID") != nullptr;   // (a test: never solid)
            const uint32_t want = notSolid || inside ? 0u : solidContents;
            // (v45u) fallen out of the world (far below the host, near him across, not moving for 4 s): room beside the host
            if (const uint32_t me = Me(base)) {
                static double lowSince = -1, toldAt = -30; static float lowZ = 0;
                const float dx = F(base, me + 336) - g_cmd.origin[0], dy = F(base, me + 340) - g_cmd.origin[1], dz = g_cmd.origin[2] - F(base, me + 344);
                const bool low = g_cmdAt >= 0 && Now() - g_cmdAt < 1.0 && dz < -250.0f && dx * dx + dy * dy < 400.0f * 400.0f;
                if (!low || lowSince < 0 || fabsf(g_cmd.origin[2] - lowZ) > 2.0f) { lowSince = low ? Now() : -1; lowZ = g_cmd.origin[2]; }
                else if (Now() - lowSince > 4.0 && Now() - toldAt > 10.0)
                {
                    toldAt = Now(); lowSince = -1;
                    float room[3]; RoomBeside(base, Me(base), room);
                    uint8_t m[16] = { 2, 1 };
                    memcpy(m + 4, room, 12);
                    SendReliable(R_MISSION, m, sizeof m);
                    printf("coop: host: the joining player is stuck %.0f below the host: room made at %.0f %.0f %.0f\n", -dz, room[0], room[1], room[2]); fflush(stdout);
                    // (v45u) what the host stands on there (what the joining copy may not have): things within 120 across, below his eye
                    std::string ex;
                    for (uint32_t i = 0; i < 4096 && ex.size() < 900; i++)
                    {
                        const uint32_t e = EntityAt(base, i);
                        if (!e || e == me || base[e] == 0) continue;
                        const float ex0 = F(base, e + 336) - F(base, me + 336), ey0 = F(base, e + 340) - F(base, me + 340), ez0 = F(base, e + 344) - F(base, me + 344);
                        if (ex0 * ex0 + ey0 * ey0 > 300.0f * 300.0f || ez0 > 60.0f || ez0 < -400.0f) continue;
                        char b[200]; snprintf(b, sizeof b, " %u(type %u %s %s at %.0f %.0f %.0f contents %08X)", i, base[e], BrocText(base, e + 564).c_str(), ResName(base, ModelSlot(base, e, 0)).c_str(), ex0, ey0, ez0, Guest::Read32(base, e + 324));
                        ex += b;
                    }
                    printf("coop: host: around the host there:%s\n", ex.c_str()); fflush(stdout);
                }
            }
            // in each other for a second (his own intro ended where the host's did): room made beside the host
            {
                static double insideSince = -1, toldAt = -10;
                if (!inside) insideSince = -1;
                else if (insideSince < 0) insideSince = Now();
                else if (Now() - insideSince > 1.0 && Now() - toldAt > 5.0)
                {
                    toldAt = Now(); insideSince = -1;
                    float room[3]; RoomBeside(base, Me(base), room);
                    uint8_t m[16] = { 2, 1 };
                    memcpy(m + 4, room, 12);
                    SendReliable(R_MISSION, m, sizeof m);
                    printf("coop: host: the joining player is in the host's player: room made at %.0f %.0f %.0f\n", room[0], room[1], room[2]); fflush(stdout);
                }
            }
            if (now != want)
            {
                Guest::Write32(base, g_ent + 324, want);
                static int told = 0;
                if (told++ < 20) { printf("coop: host: client 1 %s\n", want ? "solid again" : "in the host's player: not solid until apart"); fflush(stdout); }
            }
        }

        // ---- names: the joining copy is told each the first time it is used ----
        std::map<std::string, uint16_t> g_names[5];
        uint16_t NameId(uint8_t what, const std::string& text)
        {
            auto& m = g_names[what];
            const auto it = m.find(text);
            if (it != m.end()) return it->second;
            uint16_t id = 0;
            for (auto& mm : g_names) id = uint16_t(id + mm.size());
            id = uint16_t(id + 1);
            m[text] = id;
            std::vector<uint8_t> msg(3 + text.size());
            memcpy(msg.data(), &id, 2); msg[2] = what; memcpy(msg.data() + 3, text.data(), text.size());
            SendReliable(R_NAME, msg.data(), msg.size());
            return id;
        }
        void ResendNames()
        {
            for (uint8_t what = 0; what < 5; what++)
                for (const auto& [text, id] : g_names[what])
                {
                    std::vector<uint8_t> msg(3 + text.size());
                    memcpy(msg.data(), &id, 2); msg[2] = what; memcpy(msg.data() + 3, text.data(), text.size());
                    SendReliable(R_NAME, msg.data(), msg.size());
                }
        }

        // ---- the snapshots ----
        // What the joining copy needs of an entity to draw it as a puppet:
        // models, attachments, animation tree and what it plays, place,
        // angles and flags. Sent when changed, and all of it once a second.
        struct SentState
        {
            uint16_t gen = 0; uint8_t type = 0;
            float o[3] = {}; int16_t a[3] = {};
            uint16_t models[4] = {}, cls = 0;
            std::vector<uint16_t> attach;
            uint32_t flags = 0; uint8_t extra = 0;
            bool seenOpaque = false;   // (0.5.926) its model instance was drawn (opacity over a half) at some time
            std::vector<uint8_t> anim;
            double fullAt = -10;
        };
        std::unordered_map<uint16_t, SentState> g_sent;
        std::map<uint16_t, double> g_removed;   // removals told again for a second
        uint32_t g_snapId = 0;
        double g_snapAt = -1;
        uint16_t F16(float v, float scale) { return uint16_t(std::clamp(int32_t(lrintf(v * scale)), 0, 65535)); }
        uint32_t g_hostBody = 0, g_secondBody = 0, g_bodyGen = 0;   // the bodies drawn for the two players
        // (0.5.906) the bodies put aside at a change of gun. t905a: "the replaced body 231 is STILL HERE 10.1 s after
        // its delete": what coop.cpp calls the scripts' delete (sub_824A7280 -> sub_82540D78) only takes an entity
        // off its parent (unlink); a body was never deleted. Each change of gun left one standing: frozen soldiers
        // on the host (the user's clip of t903u), and on the joining copy the old body of his OWN player, a white
        // headless soldier on its side round his eye (t902u). Now a body put aside is hidden, never sent, and
        // KEPT for that gun: changing back takes it again, so a player has one body a gun and no more.
        std::vector<uint32_t> g_asideBodies;
        // (0.5.916) THE JOINING PLAYER'S OWN MISSILES. Since 0.5.915 his grenade is thrown in the host's world too (the
        // real one: its blast hurts here). His own copy throws its own at the same moment, so his are not sent to it,
        // nor their blasts: he sees his own once, the host sees the host's. A missile is his when it is first seen
        // within 48 units of him and nearer him than the host's player. Where one ended is kept a second (FxForward).
        struct HisMissile { uint32_t gen; float pos[3]; double goneAt; };
        std::map<uint32_t, HisMissile> g_hisMissiles;
        bool IsHisMissile(const uint8_t* base, uint32_t e, uint32_t i)
        {
            auto it = g_hisMissiles.find(i);
            return base[e] == 3 && it != g_hisMissiles.end() && it->second.goneAt < 0;
        }
        bool Replicated(const uint8_t* base, uint32_t e, uint32_t i)
        {
            const uint8_t type = base[e];
            if (type == 1 || type == 12) return false;           // players (bodies stand for them), spawners
            if (type == 3 && IsHisMissile(base, e, i)) return false;   // (0.5.916)
            // (0.5.919) what his grenade leaves when it blows (t918a: the same slot as a type 0 "grenade" for a moment)
            if (const auto it = g_hisMissiles.find(i); it != g_hisMissiles.end() && it->second.goneAt >= 0 && BrocText(base, e + 564) == "grenade") return false;   // (0.5.922: as long as it stays, t921a: 4 s)
            if (e == g_ent) return false;
            // (the joining player's own body: his copy is first person, his body there was drawn round his eye)
            if (e && e == g_secondBody) return false;
            // (0.5.906) a body put aside at a change of gun: hidden here, never the joining copy's
            for (const uint32_t a : g_asideBodies) if (a == e) return false;
            if (ModelSlot(base, e, 0) || Valid(Guest::Read32(base, e + 552)) || Valid(Guest::Read32(base, e + 928))) return true;
            (void)i;
            return false;
        }
        int g_animMost = 0, g_animCut = 0;   // (v45u) the most animations playing on one tree, trees cut short
        std::vector<uint8_t> AnimBytes(const uint8_t* base, uint32_t tree)
        {
            std::vector<uint8_t> out;
            if (!Valid(tree)) return out;
            const uint32_t list = Guest::Read32(base, tree + 8);
            if (!Valid(list)) return out;
            const uint16_t listId = NameId(2, Text(base, Guest::Read32(base, list)));
            const uint32_t count = std::min<uint32_t>(Guest::Read32(base, list + 4), 4096);
            out.resize(3);
            memcpy(out.data(), &listId, 2);
            uint8_t n = 0;
            int total = 0;
            for (uint32_t i = 0; i < count; i++)
            {
                const uint16_t r = Guest::Read16(base, tree + 24 + 2 * i);
                if (!r) continue;
                const uint32_t rec = InfoPool + 52u * r;
                const float goal = F(base, rec + 32), weight = F(base, rec + 36);
                if (goal <= 0.0f && weight <= 0.001f) continue;   // (finished: the engine frees it)
                total++;
                if (n >= 100) continue;   // (v45u: 48 before; a soldier's tree cut short lost the moves past it)
                uint8_t node[10];
                const uint16_t anim = uint16_t(i);
                const uint16_t time = F16(F(base, rec + 16), 65535.0f);
                const uint16_t rate = F16(F(base, rec + 40), 1000.0f);
                const uint16_t blend = F16(F(base, rec + 28), 1000.0f);
                memcpy(node, &anim, 2);
                node[2] = uint8_t(std::clamp(int32_t(lrintf(goal * 255.0f)), 0, 255));
                node[3] = uint8_t(std::clamp(int32_t(lrintf(weight * 255.0f)), 0, 255));
                memcpy(node + 4, &time, 2); memcpy(node + 6, &rate, 2); memcpy(node + 8, &blend, 2);
                out.insert(out.end(), node, node + 10);
                n++;
            }
            out[2] = n;
            if (total > g_animMost) g_animMost = total;
            if (total > n) g_animCut++;
            return out;
        }
        enum : uint8_t { M_ORIGIN = 1, M_ANGLES = 2, M_MODELS = 4, M_ATTACH = 8, M_ANIM = 16, M_FLAGS = 32, M_REMOVED = 64, M_FULL = 128 };
        struct Piece { std::vector<uint8_t> bytes; uint16_t count = 0; };
        void SendPieces(std::vector<Piece>& pieces, double levelTime)
        {
            g_snapId++;
            for (size_t k = 0; k < pieces.size(); k++)
            {
                if (pieces[k].count == 0) continue;
                std::vector<uint8_t> m(16);
                const uint16_t piece = uint16_t(k), total = uint16_t(pieces.size());
                memcpy(m.data(), &g_snapId, 4); memcpy(m.data() + 4, &piece, 2); memcpy(m.data() + 6, &total, 2);
                memcpy(m.data() + 8, &levelTime, 8);
                m.insert(m.end(), pieces[k].bytes.begin(), pieces[k].bytes.end());
                SendUnreliable(K_SNAP, m.data(), m.size());
            }
        }
        int g_diagDead = 0, g_diagDeadEmpty = 0, g_diagAliveEmpty = 0; double g_censusAt = 0;
        void Snapshot(uint8_t* base)
        {
            const double now = Now();
            if (now - g_snapAt < 0.05) return;
            g_snapAt = now;
            std::vector<Piece> pieces(1);
            auto add = [&](const std::vector<uint8_t>& rec) {
                if (pieces.back().bytes.size() + rec.size() > 1100) pieces.emplace_back();
                pieces.back().bytes.insert(pieces.back().bytes.end(), rec.begin(), rec.end());
                pieces.back().count++;
            };
            std::set<uint16_t> present;
            int fulls = 0;
            const uint32_t hostBody = HostBody(), secondBody = SecondBody();
            for (uint32_t i = 0; i < 4096; i++)
            {
                const uint32_t e = EntityAt(base, i);
                if (!e || !Replicated(base, e, i)) continue;
                const uint16_t idx = uint16_t(i), gen = uint16_t(GenAt(base, i));
                present.insert(idx);
                SentState& s = g_sent[idx];
                const bool fresh = s.gen != gen || s.type != base[e] || s.fullAt < 0;
                if (fresh) { s = SentState{}; s.gen = gen; s.type = base[e]; }
                // a full one a second (a few each snapshot, so no snapshot is all of them)
                bool full = fresh || (now - s.fullAt > 1.0 && fulls < 12);
                if (full) { fulls++; s.fullAt = now; }
                uint8_t mask = full ? M_FULL : 0;
                float o[3]; int16_t a[3];
                for (int k = 0; k < 3; k++) { o[k] = F(base, e + 336 + 4 * k); a[k] = int16_t(lrintf(F(base, e + 352 + 4 * k) * (32768.0f / 180.0f))); }
                if (e == g_hostBody) o[2] += 6000.0f;   // (kept below the world here: DriveBody)
                // (0.5.895) the host's pitch, for his body's aim there: in the body's first angle (the body itself stands upright)
                if (e == g_hostBody) { const uint32_t hr = Record(base, Me(base)); if (hr) a[0] = int16_t(lrintf(Pitch180(F(base, hr + 172)) * (32768.0f / 180.0f))); }
                if (full || fabsf(o[0] - s.o[0]) + fabsf(o[1] - s.o[1]) + fabsf(o[2] - s.o[2]) > 0.05f) mask |= M_ORIGIN;
                if (full || a[0] != s.a[0] || a[1] != s.a[1] || a[2] != s.a[2]) mask |= M_ANGLES;
                uint16_t models[4]; uint16_t cls = NameId(4, BrocText(base, e + 564));
                for (int sl = 0; sl < 4; sl++) { const uint32_t m = ModelSlot(base, e, sl); models[sl] = Valid(m) ? NameId(1, ResName(base, m)) : 0; }
                if (full || memcmp(models, s.models, sizeof models) != 0 || cls != s.cls) mask |= M_MODELS;
                std::vector<uint16_t> attach;
                for (int k = 0; k < 7; k++)
                {
                    const uint32_t m = Guest::Read32(base, e + 928 + 8 * k);
                    if (!Valid(m)) continue;
                    attach.push_back(NameId(1, ResName(base, m)));
                    attach.push_back(NameId(3, BrocText(base, e + 932 + 8 * k)));
                }
                if (full || attach != s.attach) mask |= M_ATTACH;
                uint32_t flags = Guest::Read32(base, e + 632);
                // (0.5.920) the scripts' hide (built-in 0x5E0, sub_824A4A60 -> sub_820B23F8) is the model instance's
                // (+548) opacity at +236, not +632: t919a, the Saint-Lo Garand taken here stayed on the barrel there
                // (0.5.926) only a thing the scripts HID: seen drawn (opacity over a half) and then at 0, never a soldier.
                // t925a: 0.5.920 hid every soldier on the joining copy (theirs is 0 while they are drawn)
                if (const uint32_t inst = Guest::Read32(base, e + 548); Valid(inst) && e != hostBody && base[e] != 11)
                {
                    const float op = F(base, inst + 236);
                    if (op > 0.5f) s.seenOpaque = true;
                    else if (op <= 0.001f && s.seenOpaque) flags |= 0x400u;
                }
                uint8_t extra = 0;
                if (e == hostBody) flags &= ~0x400u;          // hidden here, drawn there
                if (e == hostBody)
                {
                    // (v43g) the host's stance for his body there: 4 crouched, 8 prone (the joining copy animates it itself)
                    const uint32_t hr = Record(base, Me(base));
                    if (hr) { const int32_t hh = int32_t(Guest::Read32(base, hr + 184)); if (hh <= 20) extra |= 8; else if ((Guest::Read32(base, hr + 208) & 0x20) || hh <= 45) extra |= 4; }   // (0.5.869: crouched by the view height too)
                }
                if (e == hostBody) extra |= uint8_t(HostActBits(base, 0) << 4);   // (0.5.884) the high four bits: what he does (1 aim, 2 fire, 3 throw, 4 reload, 5 melee)
                if (e == secondBody) extra |= 2;              // the joining player's own body: not drawn there
                if (Guest::Read32(base, e + 324) & 1) extra |= 1;   // solid
                if (full || flags != s.flags || extra != s.extra) mask |= M_FLAGS;
                const std::vector<uint8_t> anim = AnimBytes(base, Guest::Read32(base, e + 552));
                if (base[e] == 11 || (base[e] == 7 && anim.size() >= 3))
                {
                    // (v45u) what the dead are drawn with here: a soldier lying dead with nothing playing is posed
                    // by something else (his body's physics), and there stood in his bind pose
                    if (int32_t(Guest::Read32(base, e + 776)) <= 0) { g_diagDead++; if (anim.size() < 3 || anim[2] == 0) g_diagDeadEmpty++; }
                    else if (anim.size() >= 3 && anim[2] == 0) g_diagAliveEmpty++;
                }
                // (the animations: when what plays changes, else each 0.25 s for the times)
                bool animChanged = anim.size() != s.anim.size();
                if (!animChanged && anim.size() >= 3)
                    for (size_t k = 3; k + 10 <= anim.size() && !animChanged; k += 10)
                        if (memcmp(anim.data() + k, s.anim.data() + k, 4) != 0) animChanged = true;   // (which animation, goal, weight)
                static std::unordered_map<uint16_t, double> animAt;
                if (!anim.empty() && (full || animChanged || now - animAt[idx] > 0.25)) { mask |= M_ANIM; animAt[idx] = now; }
                if (!(mask & ~M_FULL)) continue;
                std::vector<uint8_t> r(6);
                memcpy(r.data(), &idx, 2); memcpy(r.data() + 2, &gen, 2); r[4] = base[e]; r[5] = mask;
                auto put = [&](const void* p, size_t n) { const uint8_t* b = static_cast<const uint8_t*>(p); r.insert(r.end(), b, b + n); };
                if (mask & M_ORIGIN) { put(o, 12); memcpy(s.o, o, 12); }
                if (mask & M_ANGLES) { put(a, 6); memcpy(s.a, a, 6); }
                if (mask & M_MODELS) { put(models, 8); put(&cls, 2); memcpy(s.models, models, 8); s.cls = cls; }
                if (mask & M_ATTACH) { const uint8_t n = uint8_t(attach.size() / 2); put(&n, 1); if (!attach.empty()) put(attach.data(), attach.size() * 2); s.attach = attach; }
                if (mask & M_FLAGS) { put(&flags, 4); put(&extra, 1); s.flags = flags; s.extra = extra; }
                if (mask & M_ANIM) { put(anim.data(), anim.size()); s.anim = anim; }
                add(r);
            }
            for (auto it = g_sent.begin(); it != g_sent.end();)
            {
                if (present.count(it->first)) { ++it; continue; }
                g_removed[it->first] = now;
                it = g_sent.erase(it);
            }
            for (auto it = g_removed.begin(); it != g_removed.end();)
            {
                if (now - it->second > 1.0 || present.count(it->first)) { it = g_removed.erase(it); continue; }
                std::vector<uint8_t> r(6);
                const uint16_t idx = it->first, gen = 0;
                memcpy(r.data(), &idx, 2); memcpy(r.data() + 2, &gen, 2); r[4] = 0; r[5] = M_REMOVED;
                add(r);
                ++it;
            }
            const double levelTime = double(int32_t(Guest::Read32(base, Level + 156))) / 1000.0;
            SendPieces(pieces, levelTime);
            { static double toldAt = 0; if (now - toldAt > 5) { toldAt = now; printf("coop: host: level time %.1f s\n", levelTime); fflush(stdout); } }   // (v46: does a pause stop it)
            static double toldAt = 0;
            if (now - toldAt > 20) { toldAt = now; size_t bytes = 0; for (auto& p : pieces) bytes += p.bytes.size(); printf("coop: host: snapshot %u: %zu entities known, %zu pieces, %zu bytes; soldiers: %d dead (%d with nothing playing), %d alive with nothing playing; most animations on one tree %d, %d cut short\n", g_snapId, g_sent.size(), pieces.size(), bytes, g_diagDead, g_diagDeadEmpty, g_diagAliveEmpty, g_animMost, g_animCut); fflush(stdout); g_animMost = 0; g_animCut = 0; }
            if (now - g_censusAt > 20)
            {
                // (v45u) what the men lying dead here are: their type and whether an animation poses them
                g_censusAt = now; std::string ex; int n = 0;
                for (uint32_t i = 0; i < 4096; i++)
                {
                    const uint32_t e = EntityAt(base, i);
                    if (!e || !Replicated(base, e, i)) continue;
                    const std::string m0 = ResName(base, ModelSlot(base, e, 0)); if (m0.rfind("c_", 0) != 0) continue;
                    const uint32_t tree = Guest::Read32(base, e + 552);
                    const std::vector<uint8_t> anim = AnimBytes(base, tree);
                    if (Valid(tree) && anim.size() >= 3 && anim[2] > 0) continue;
                    n++; if (ex.size() < 700) ex += " " + std::to_string(i) + "(type " + std::to_string(base[e]) + " " + BrocText(base, e + 564) + (Valid(tree) ? " tree" : " no tree") + " health " + std::to_string(int32_t(Guest::Read32(base, e + 776))) + " flags " + std::to_string(Guest::Read32(base, e + 632)) + ")";
                }
                printf("coop: host: %d men drawn with no animation playing:%s\n", n, ex.c_str()); fflush(stdout);
            }
            g_diagDead = g_diagDeadEmpty = g_diagAliveEmpty = 0;
        }

        // ---- config strings, server commands, HUD elements ----
        std::vector<std::string> g_csSent;
        bool g_csResend = false;
        void SendConfigstring(uint16_t index, const std::string& text)
        {
            if (g_csSent.size() < 1024) g_csSent.resize(1024);
            g_csSent[index] = text;
            std::vector<uint8_t> m(2 + text.size());
            memcpy(m.data(), &index, 2); memcpy(m.data() + 2, text.data(), text.size());
            SendReliable(R_CONFIGSTRING, m.data(), m.size());
        }
        void SendAllConfigstrings(const uint8_t* base)
        {
            int n = 0;
            for (uint16_t i = 0; i < 1024; i++)
            {
                const std::string t = BrocText(base, 0x82A568E0 + 16 + 4u * i);
                if (t.empty()) continue;
                SendConfigstring(i, t); n++;
            }
            printf("coop: host: %d config strings sent to the joining copy\n", n); fflush(stdout);
        }
        uint8_t g_hudSent[16][124];
        bool g_hudKnown[16] = {};
        void HudSync(const uint8_t* base)
        {
            static double at = 0;
            if (Now() - at < 0.1) return;
            at = Now();
            for (uint8_t i = 0; i < 16; i++)
            {
                const uint8_t* h = base + HudElems + 124u * i;
                if (g_hudKnown[i] && memcmp(h, g_hudSent[i], 124) == 0) continue;
                memcpy(g_hudSent[i], h, 124); g_hudKnown[i] = true;
                uint8_t m[125]; m[0] = i; memcpy(m + 1, h, 124);
                SendReliable(R_HUD, m, sizeof m);
            }
        }

        // ---- the bodies ----
        // Each player is drawn by a script_model dressed as one of the level's
        // friendly soldiers (his four models and his rifle) with the generic_human
        // tree, placed where the player is, turned the way he looks, and animated
        // by how he moves: standing, crouched or prone, still, walking or running,
        // forward, back or to a side. The animations are found by their names'
        // hashes (the same in every level), from what the level's soldiers play
        // in each of those (coop_probe.cpp COD3_PROBE40_ANIMS, Forest).
        // The host's body is hidden here and drawn on the joining copy; the
        // joining player's is drawn here and not on his copy (his own view).
        uint32_t g_template = 0;    // a friendly soldier whose look the bodies take
        enum Move { STAND_IDLE, STAND_WALK_F, STAND_WALK_B, STAND_WALK_L, STAND_WALK_R, STAND_RUN_F, CROUCH_IDLE, CROUCH_WALK_F, CROUCH_WALK_B, CROUCH_WALK_L, CROUCH_WALK_R, PRONE_IDLE, PRONE_CRAWL, MOVES };
        const uint32_t MoveHash[MOVES] = {
            0x92A23EE1, 0x444ED990, 0xF4C043BB, 0xF4C5D0F5, 0x055BB472, 0x4DE531AF,
            0x7CDAACCD, 0x6F035656, 0x8EE74CB5, 0x68D9B13C, 0x847EA06F,
            0xE92CB727, 0xE92CB727 };
        struct BodyState { int32_t anim = -1; float last[3] = {}; double at = -1; float speed = 0; int move = -1; double mirrorAt = -10; float lastT = 0; double bare = 0, bareAt = -1, toldAt = 0; uint32_t restarts = 0, replays = 0; };
        BodyState g_bodyState[2];
        uint32_t g_oldShown = 0; double g_oldShownAt = 0;   // (0.5.936) the joining player's old body, drawn till his new one plays
        int32_t AnimIndex(const uint8_t* base, uint32_t list, uint32_t hash)
        {
            const uint32_t count = std::min<uint32_t>(Guest::Read32(base, list + 4), 4096), entries = Guest::Read32(base, list + 8);
            for (uint32_t k = 0; k < count; k++) if (Guest::Read32(base, entries + 28 * k) == hash) return int32_t(k);
            return -1;
        }
        uint32_t FindTemplate(const uint8_t* base)
        {
            uint32_t best = 0; int bestScore = -1;
            for (uint32_t i = 2; i < 4096; i++)
            {
                const uint32_t e = EntityAt(base, i);
                if (!e || base[e] != 11 || int32_t(Guest::Read32(base, e + 776)) <= 0) continue;
                const std::string c = BrocText(base, e + 564);
                if (c.rfind("actor_ally", 0) != 0) continue;
                int score = 0;
                for (int sl = 0; sl < 4; sl++) if (Valid(ModelSlot(base, e, sl))) score++;
                if (c.find("_reg") != std::string::npos) score += 4;   // (an ordinary soldier, not a named one)
                if (Valid(Guest::Read32(base, e + 928))) score += 2;
                if (score > bestScore) { bestScore = score; best = e; }
            }
            return best;
        }
        // (0.5.896) weapon: the world model of the gun the player carries, in place of the soldier's own rifle
        uint32_t MakeBody(PPCContext& ctx, uint8_t* base, uint32_t from, const float at[3], const std::string& weapon = std::string())
        {
            const uint32_t org = Scratch(base) + 256;
            for (int k = 0; k < 3; k++) SetF(base, org + 4 * k, at[k]);
            PPCContext c = CallCtx(ctx);
            c.r3.u64 = GuestBroc(base, "script_model"); c.r4.u64 = org; c.r5.u64 = 0;
            CallGuest(c, base, 0x824B4510);   // the scripts' spawn
            const uint32_t e = FromRef(base, c.r3.u32);
            if (!e) return 0;
            for (int sl = 0; sl < 4; sl++)
            {
                const uint32_t m = ModelSlot(base, from, sl);
                if (!Valid(m)) continue;
                c = CallCtx(ctx);
                c.r3.u64 = e; c.r4.u64 = GuestText(base, ResName(base, m)); c.r5.u64 = uint64_t(-1); c.r6.u64 = uint64_t(sl);
                CallGuest(c, base, 0x8254EE80);   // G_SetModel
            }
            for (int k = 0; k < 7; k++)
            {
                const uint32_t m = Guest::Read32(base, from + 928 + 8 * k);
                if (!Valid(m)) continue;
                const int32_t bank = int32_t(Guest::Read32(base, e + 496));
                c = CallCtx(ctx);
                const std::string tag = BrocText(base, from + 932 + 8 * k);
                c.r3.u64 = e; c.r4.u64 = GuestText(base, !weapon.empty() && tag == "tag_weapon_right" ? weapon : ResName(base, m)); c.r5.u64 = GuestText(base, tag);
                c.r6.u64 = 1; c.r7.u64 = bank != -1 ? uint32_t(bank) : Guest::Read32(base, Guest::Read32(base, 0x82A2AB6C) + 40);
                CallGuest(c, base, 0x825587B0);   // attach
            }
            const uint32_t tree = Guest::Read32(base, from + 552);
            if (Valid(tree))
            {
                c = CallCtx(ctx); c.r3.u64 = e; c.r4.u64 = Guest::Read32(base, tree + 8);
                CallGuest(c, base, 0x82555B98);   // useanimtree's own step
            }
            c = CallCtx(ctx); c.r3.u64 = RefOf(base, uint32_t(IndexOf(base, e)));
            CallGuest(c, base, 0x824A6820);   // notsolid (the player's own box is his)
            c = CallCtx(ctx); c.r3.u64 = e; CallGuest(c, base, 0x82554D60);
            return e;
        }
        // a body's tree given the animations its soldier (g_template) plays now, twice a second
        void MirrorTemplate(PPCContext& ctx, uint8_t* base, uint32_t tree, BodyState& st)
        {
            if (Now() - st.mirrorAt < 0.5) return;
            st.mirrorAt = Now();
            const uint32_t from = g_template ? Guest::Read32(base, g_template + 552) : 0;
            if (!Valid(from) || Guest::Read32(base, from + 8) != Guest::Read32(base, tree + 8)) return;
            const uint32_t count = std::min<uint32_t>(Guest::Read32(base, Guest::Read32(base, tree + 8) + 4), 4096);
            for (uint32_t i = 0; i < count; i++)
            {
                const uint16_t r = Guest::Read16(base, from + 24 + 2 * i);
                float goal = 0.0f, rate = 1.0f;
                if (r) { const uint32_t rec = InfoPool + 52u * r; goal = F(base, rec + 32); rate = F(base, rec + 40); }
                const bool mine = Guest::Read16(base, tree + 24 + 2 * i) != 0;
                if (goal <= 0.001f && !mine) continue;
                if (!AnimJobRoom(base)) { st.mirrorAt = -10; return; }   // (the rest next frame)
                PPCContext c = CallCtx(ctx);
                c.r3.u64 = tree; c.r4.u64 = i; c.f1.f64 = goal; c.f2.f64 = 0.2; c.f3.f64 = rate > 0.01f ? rate : 1.0; c.r8.u64 = 0; c.r10.u64 = 0;
                CallGuest(c, base, 0x82500A40);   // set anim
            }
        }
        // the body of a player: placed, turned and animated by how he moves
        // (0.5.879) every node an entity's animation tree holds, whole: to read a real soldier's beside the
        // player's body in its T-pose (the tree's first words, then per node: index, name hash, the list entry's
        // word +4, and the 13 words of its 52-byte record)
        void DumpTree(const uint8_t* base, const char* label, uint32_t e)
        {
            if (!e || IndexOf(base, e) < 0) { printf("coop: dump %s: no entity\n", label); return; }
            const uint32_t tree = Guest::Read32(base, e + 552);
            printf("coop: dump %s at %.1f s: entity %d type %u flags632 %08X tree %08X at %.0f %.0f %.0f\n", label, Now(), IndexOf(base, e), base[e], Guest::Read32(base, e + 632), tree, F(base, e + 336), F(base, e + 340), F(base, e + 344));
            if (!Valid(tree)) return;
            std::string h; for (int k = 0; k < 6; k++) { char b[16]; snprintf(b, sizeof b, " %08X", Guest::Read32(base, tree + 4u * k)); h += b; }
            const uint32_t list = Guest::Read32(base, tree + 8);
            if (!Valid(list)) { printf("coop: dump %s tree:%s (no list)\n", label, h.c_str()); return; }
            const uint32_t count = std::min<uint32_t>(Guest::Read32(base, list + 4), 4096), entries = Guest::Read32(base, list + 8);
            printf("coop: dump %s tree:%s list %s count %u\n", label, h.c_str(), Text(base, Guest::Read32(base, list)).c_str(), count);
            int shown = 0;
            for (uint32_t k = 0; k < count && shown < 40; k++)
            {
                const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * k); if (!sl) continue;
                shown++;
                const uint32_t r = InfoPool + 52u * sl;
                std::string w; for (int i = 0; i < 13; i++) { char b[16]; snprintf(b, sizeof b, " %08X", Guest::Read32(base, r + 4u * i)); w += b; }
                printf("coop: dump %s node %u hash %08X e4 %04X e6 %04X slot %u t %.3f w %.3f g %.3f:%s\n", label, k, Guest::Read32(base, entries + 28 * k), Guest::Read16(base, entries + 28 * k + 4), Guest::Read16(base, entries + 28 * k + 6), sl, F(base, r + 16), F(base, r + 36), F(base, r + 32), w.c_str());
            }
        }
        void DriveBody(PPCContext& ctx, uint8_t* base, int who, uint32_t body, uint32_t player, const float view[3], uint32_t rec)
        {
            if (!body || !player || IndexOf(base, body) < 0) return;
            BodyState& st = g_bodyState[who];
            const double now = Now();
            const float o[3] = { F(base, player + 336), F(base, player + 340), F(base, player + 344) };
            static BodyMotion motion[2];   // (0.5.891)
            if (st.at < 0) motion[who] = BodyMotion{};
            BodyMotion& mo = motion[who];
            mo.Feed(now, o[0], o[1]);
            st.speed = mo.speed;
            const float vx = mo.vx, vy = mo.vy;
            memcpy(st.last, o, 12); st.at = now;
            uint32_t flags = rec ? Guest::Read32(base, rec + 208) : 0;
            int32_t height = rec ? int32_t(Guest::Read32(base, rec + 184)) : 60;
            // (0.5.869) the joining player's stance is his own copy's (his record's +208 and view height +184, in
            // his commands): client 1's record here is moved by his copy, and its own stance did not follow his
            if (who == 1 && g_cmdAt >= 0 && (g_cmd.flags & 0x10000u)) { flags = g_cmd.stance; height = int32_t((g_cmd.flags >> 8) & 0xFF); }
            const bool prone = height <= 20, crouch = !prone && ((flags & 0x20) || height <= 45);
            {
                static int32_t wasH[2] = { -1, -1 }; static uint32_t wasF[2] = {}; static int told = 0;
                if ((abs(height - wasH[who]) > 8 || ((flags ^ wasF[who]) & 0x20)) && told < 80) { told++; printf("coop: host: %s stance: view height %d, flags %08X (%s)\n", who == 0 ? "the host's" : "the joining player's", height, flags, prone ? "prone" : crouch ? "crouched" : "standing"); fflush(stdout); wasH[who] = height; wasF[who] = flags; }
            }
            const float yaw = view[1] * 0.0174533f;
            const float fwd = vx * cosf(yaw) + vy * sinf(yaw), side = -vx * sinf(yaw) + vy * cosf(yaw);
            int move;
            if (st.speed < 20.0f) move = prone ? PRONE_IDLE : crouch ? CROUCH_IDLE : STAND_IDLE;
            else if (prone) move = PRONE_CRAWL;
            else
            {
                const bool f = mo.ForwardAxis(fwd, side);
                if (crouch) move = f ? (fwd >= 0 ? CROUCH_WALK_F : CROUCH_WALK_B) : (side > 0 ? CROUCH_WALK_L : CROUCH_WALK_R);
                else if (f && fwd >= 0) move = st.speed > 170.0f ? STAND_RUN_F : STAND_WALK_F;
                else move = f ? STAND_WALK_B : (side > 0 ? STAND_WALK_L : STAND_WALK_R);
            }
            move = mo.Settle(now, move);   // (0.5.891)
            // (0.5.882) what he does, from his own buttons: firing, throwing, aiming
            {
                const int was = move; { uint32_t act = HostActBits(base, who); if (act == 0 && fabsf(Pitch180(view[0])) > AimPitchMin) act = 1; move = mo.Act(act, BodyAct(move, act)); }   // (0.5.895: looking up or down, he aims)
                static int last[2] = { -1, -1 }, told = 0;
                if (move != was && move != last[who] && move > 14 && told < 200) { told++; printf("coop: host: %s body: %s\n", who == 0 ? "the host's" : "the joining player's", BodyMoveText(move)); fflush(stdout); }
                last[who] = move;
            }
            // placed and turned (and never solid: a body drawn only)
            if (Guest::Read32(base, body + 324)) { static int told = 0; if (told++ < 4) { printf("coop: host: a body was solid (%08X): no longer\n", Guest::Read32(base, body + 324)); fflush(stdout); } Guest::Write32(base, body + 324, 0); }
            // the host's own body is the joining copy's to see: here it is kept out of sight, below the world
            // (drawn here it filled the host's view with the inside of its model); the snapshots say where it is
            for (const uint32_t at : { 336u, 32u }) for (int k = 0; k < 3; k++) SetF(base, body + at + 4 * k, o[k] - (who == 0 && k == 2 ? 6000.0f : 0.0f));
            for (const uint32_t at : { 352u, 72u }) { SetF(base, body + at, 0.0f); SetF(base, body + at + 4, view[1]); SetF(base, body + at + 8, 0.0f); }
            LinkEntity(ctx, base, body);
            const uint32_t tree = Guest::Read32(base, body + 552);
            // (0.5.877) how long the body has had nothing playing (its bind pose, the T-pose the user sees): no
            // tree, the move's slot let go, or its weight near nothing. Said every 10 s.
            {
                bool bare = !Valid(tree) || st.anim < 0; float t = -1, w = -1, goal = -1; uint16_t slot = 0;
                if (!bare) { slot = Guest::Read16(base, tree + 24 + 2 * uint32_t(st.anim)); if (!slot) bare = true; else { const uint32_t r = InfoPool + 52u * slot; t = F(base, r + 16); w = F(base, r + 36); goal = F(base, r + 32); if (!(w > 0.0f)) bare = true; } }
                if (bare && st.bareAt >= 0) st.bare += std::min(now - st.bareAt, 1.0);
                st.bareAt = now;
                if (now - st.toldAt > 10.0 && Valid(tree))
                {
                    // (0.5.878) the tree's nodes that weigh something: animations and parent nodes
                    const uint32_t list = Guest::Read32(base, tree + 8), count = std::min<uint32_t>(Guest::Read32(base, list + 4), 4096), entries = Guest::Read32(base, list + 8);
                    int leaves = 0, parents = 0, held = 0;
                    for (uint32_t k = 0; k < count; k++) { const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * k); if (!sl) continue; held++; if (!(F(base, InfoPool + 52u * sl + 36) > 0.3f)) continue; if (Guest::Read16(base, entries + 28 * k + 4)) parents++; else leaves++; }
                    printf("coop: host: %s body's tree: %d nodes held, weighing: %d animations, %d parent nodes\n", who == 0 ? "the host's" : "the joining player's", held, leaves, parents);
                }
                if (now - st.toldAt > 10.0) { st.toldAt = now; printf("coop: host: %s body: tree %s, move %d, animation %d, slot %u, time %.2f, weight %.2f (to %.2f); bare %.1f s so far, %u restarts, %u replays\n", who == 0 ? "the host's" : "the joining player's", Valid(tree) ? "there" : "NONE", st.move, st.anim, slot, t, w, goal, st.bare, st.restarts, st.replays); fflush(stdout); }
            }
            if (!Valid(tree)) return;
            const uint32_t list = Guest::Read32(base, tree + 8);
            // (v43) the move playing is kept playing: an animation that does not loop by itself (several of the
            // idles and walks) ran out and left the body in its bind pose, the T-pose seen for most of a mission;
            // so near its end it starts over, and its weight is put back if the tree let it go
            if (move == st.move && st.anim >= 0)
            {
                const uint16_t slot = Guest::Read16(base, tree + 24 + 2 * uint32_t(st.anim));
                if (!slot) { st.anim = -1; st.move = -1; }
                else
                {
                    const uint32_t r = InfoPool + 52u * slot;
                    // (0.5.877) started over one frame's step before its end: at a low frame rate it went from
                    // under 0.97 past its end between two frames, its slot was let go and it was blended in again
                    // from nothing (the bind pose for a moment, every time the idle ran out)
                    const float t = F(base, r + 16), step = std::clamp(t - st.lastT, 0.0f, 0.5f);
                    const bool end = t >= 0.97f || (step > 0.0f && step < 0.1f && t + 1.5f * step >= 1.0f);
                    if (end && OneShotMove(move)) { SetF(base, r + 40, 0.0f); st.lastT = t; }   // (0.5.892: held at its end, not begun again)
                    else if (end) { SetF(base, r + 16, 0.0f); SetF(base, r + 20, 0.0f); st.lastT = 0; st.restarts++; }
                    else st.lastT = t;
                    return;   // (0.5.890: its weight is TidyBodyTree's to keep)
                }
            }
            if (move == st.move) return;
            int32_t anim = AnimIndex(base, list, BodyMoveHash(move));
            if (anim < 0) { static int told = 0; if (told++ < 12) { printf("coop: host: the move %d (%s) is not in the body's list\n", move, BodyMoveText(move)); fflush(stdout); } anim = AnimIndex(base, list, BodyMoveHash(STAND_IDLE)); }
            if (anim < 0)
            {
                // this level's soldiers have none of the moves looked for (no T-pose for it): the body plays
                // what the soldier it was dressed as plays, his animations copied into its tree
                static int told = 0;
                if (told++ < 3) { printf("coop: host: a body's moves are not in its animation list %s (%u animations): it copies its soldier's\n", Text(base, Guest::Read32(base, list)).c_str(), Guest::Read32(base, list + 4)); fflush(stdout); }
                MirrorTemplate(ctx, base, tree, st);
                return;
            }
            if (anim == st.anim) { st.move = move; return; }
            // (0.5.892: not kept waiting for room in the pose queue any more: a full queue is posed early (v45v))
            // the new one in over a fifth of a second, the old one out ((0.5.877) at once when nothing was playing)
            PPCContext c = CallCtx(ctx);
            if (st.anim < 0) st.replays++;
            st.lastT = 0;
            // (0.5.878) THE T-POSE: the move was set on its own node only (sub_82500A40), and a node shows only as
            // much as its parent nodes weigh. The parents of a move never set before came in weighing nothing, so
            // the body stood in its bind pose with its idle "playing" at full weight (t877a: both screens). The
            // move is set with the scripts' own call that weighs the parents too (sub_82500B80), as the soldier
            // puppets' are; the rate, which that call does not take, is written after.
            c.r3.u64 = tree; c.r4.u64 = uint32_t(anim); c.f1.f64 = 1.0; c.f2.f64 = st.anim < 0 ? 0.0 : 0.2;
            CallGuest(c, base, 0x82500B80);   // set anim, the parents too
            if (const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * uint32_t(anim))) { SetF(base, InfoPool + 52u * sl + 40, move == STAND_RUN_F ? 1.15f : 1.0f); if (OneShotMove(move)) { SetF(base, InfoPool + 52u * sl + 16, 0.0f); SetF(base, InfoPool + 52u * sl + 20, 0.0f); } }
            if (st.anim >= 0)
            {
                c = CallCtx(ctx);
                c.r3.u64 = tree; c.r4.u64 = uint32_t(st.anim); c.f1.f64 = 0.0; c.f2.f64 = 0.2; c.f3.f64 = 1.0; c.r8.u64 = 0; c.r10.u64 = 0;
                CallGuest(c, base, 0x82500A40);
            }
            st.anim = anim; st.move = move;
        }
        void Bodies(PPCContext& ctx, uint8_t* base)
        {
            static const bool none = getenv("COD3_COOP_NOBODIES") != nullptr;   // (a test)
            if (none) return;
            const uint32_t me = Me(base);
            const bool second = SecondPlayerAlive(base);
            if (g_bodyGen != MissionCount()) { g_bodyGen = MissionCount(); g_asideBodies.clear(); g_hostBody = g_secondBody = g_template = 0; g_bodyState[0] = g_bodyState[1] = BodyState{}; }
            static int32_t hostBodyIdx = -1, secondBodyIdx = -1; static uint32_t hostBodyGen = 0, secondBodyGen = 0;
            auto stillOurs = [&](uint32_t& e, int32_t& idx, uint32_t& gen) {
                if (!e) return;
                if (idx < 0) { idx = IndexOf(base, e); gen = idx >= 0 ? GenAt(base, uint32_t(idx)) : 0; }
                if (idx < 0 || EntityAt(base, uint32_t(idx)) != e || GenAt(base, uint32_t(idx)) != gen || base[e] != 7) { e = 0; idx = -1; }
            };
            stillOurs(g_hostBody, hostBodyIdx, hostBodyGen);
            stillOurs(g_secondBody, secondBodyIdx, secondBodyGen);
            if (!g_hostBody) hostBodyIdx = -1;
            if (!g_secondBody) secondBodyIdx = -1;
            if ((!g_hostBody || (second && !g_secondBody)) && me)
            {
                static double tryAt = -10;
                if (Now() - tryAt < 3.0) return;
                tryAt = Now();
                if (!g_template || IndexOf(base, g_template) < 0 || base[g_template] != 11 || int32_t(Guest::Read32(base, g_template + 776)) <= 0) g_template = FindTemplate(base);
                if (!g_template) { static int told = 0; if (told++ < 5) { printf("coop: host: no friendly soldier to dress the players' bodies as yet\n"); fflush(stdout); } return; }
                const float at[3] = { F(base, me + 336), F(base, me + 340), F(base, me + 344) };
                if (!g_hostBody)
                {
                    g_hostBody = MakeBody(ctx, base, g_template, at);
                    if (g_hostBody) Guest::Write32(base, g_hostBody + 632, Guest::Read32(base, g_hostBody + 632) | 0x400u);   // (not drawn here)
                    printf("coop: host: the host's body is entity %d (as %s)\n", IndexOf(base, g_hostBody), BrocText(base, g_template + 564).c_str()); fflush(stdout);
                }
                if (second && !g_secondBody)
                {
                    g_secondBody = MakeBody(ctx, base, g_template, at);
                    printf("coop: host: the joining player's body is entity %d (as %s)\n", IndexOf(base, g_secondBody), BrocText(base, g_template + 564).c_str()); fflush(stdout);
                }
            }
            if (g_hostBody && me) { const uint32_t rec = Record(base, me); float v[3] = { F(base, rec + 172), F(base, rec + 176), F(base, rec + 180) }; DriveBody(ctx, base, 0, g_hostBody, me, v, rec); }
            if (g_secondBody && second) DriveBody(ctx, base, 1, g_secondBody, g_ent, g_cmd.view, g_rec);
            // (0.5.896) THE GUN HE CARRIES. A player's weapon is the word +132 of his record (the number in the
            // campaign's weapon list [0x82A2A2E0]; a weapon's world model is its text +1432: probe of t895a). The
            // host's is read here; the joining player's comes in his commands (the top byte of the flags: client
            // 1's record here does not follow his copy's). A body cannot change what it carries once it animates
            // (its skeleton is sized for the models it had: v45e), so when the gun changes the body is made again
            // with the new one and the old body let go; the joining copy gets a new puppet for the host's.
            {
                static std::string shown[2], want[2]; static double wantAt[2] = {}, madeAt[2] = {}; static uint32_t shownFor[2] = {};
                static std::map<std::string, uint32_t> spare[2]; static uint32_t spareGen = 0xFFFFFFFFu;   // (0.5.906) the bodies aside, by gun
                if (spareGen != g_bodyGen) { spareGen = g_bodyGen; spare[0].clear(); spare[1].clear(); }
                const uint32_t wl = Guest::Read32(base, 0x82A2A2E0u);
                for (int who = 0; who < 2 && Valid(wl) && g_template; who++)
                {
                    uint32_t& body = who == 0 ? g_hostBody : g_secondBody;
                    if (!body || IndexOf(base, body) < 0) continue;
                    if (shownFor[who] != body) { shownFor[who] = body; shown[who].clear(); want[who].clear(); }
                    uint32_t number = 0;
                    if (who == 0) { const uint32_t rec0 = me ? Record(base, me) : 0; if (rec0) number = Guest::Read32(base, rec0 + 132); }
                    else if (second && g_cmdAt >= 0 && Now() - g_cmdAt < 1.0) number = (g_cmd.flags >> 24) & 0xFFu;
                    if (number < 1 || number > 160) continue;
                    const uint32_t def = Guest::Read32(base, wl + 4 * number);
                    if (!Valid(def)) continue;
                    const uint32_t mt = Guest::Read32(base, def + 1432), nt = Guest::Read32(base, def + 8);
                    const std::string model = Valid(mt) ? Text(base, mt) : std::string(), name = Valid(nt) ? Text(base, nt) : std::string();
                    // (a grenade in the hand, the mortar's round, bare hands: the gun stays)
                    std::string low = model; for (char& ch : low) ch = char(tolower(static_cast<unsigned char>(ch)));
                    if (model.size() < 3 || low.find("nade") != std::string::npos || name == "interact" || name == "mortar_shell") continue;
                    if (model != want[who]) { want[who] = model; wantAt[who] = Now(); }
                    if (shown[who].empty())
                    {
                        // the first look: what the body was dressed with
                        for (int k = 0; k < 7; k++) { const uint32_t m = Guest::Read32(base, body + 928 + 8 * k); if (Valid(m) && BrocText(base, body + 932 + 8 * k) == "tag_weapon_right") shown[who] = ResName(base, m); }
                        if (shown[who].empty()) shown[who] = "?";
                    }
                    if (model == shown[who] || Now() - wantAt[who] < 0.3 || Now() - madeAt[who] < 1.0) continue;
                    const uint32_t player = who == 0 ? me : g_ent;
                    if (!player) continue;
                    const float at[3] = { F(base, player + 336), F(base, player + 340), F(base, player + 344) - (who == 0 ? 6000.0f : 0.0f) };
                    uint32_t fresh = 0; bool again = false;
                    if (const auto sp = spare[who].find(model); sp != spare[who].end())
                    {
                        const uint32_t c = sp->second; spare[who].erase(sp);
                        if (IndexOf(base, c) >= 0 && base[c] == 7 && std::find(g_asideBodies.begin(), g_asideBodies.end(), c) != g_asideBodies.end()) { fresh = c; again = true; }
                    }
                    if (!fresh) fresh = MakeBody(ctx, base, g_template, at, model);
                    madeAt[who] = Now();
                    if (!fresh) { static int told = 0; if (told++ < 6) { printf("coop: host: no new body for %s with %s\n", who == 0 ? "the host" : "the joining player", model.c_str()); fflush(stdout); } continue; }
                    if (again)
                    {
                        g_asideBodies.erase(std::remove(g_asideBodies.begin(), g_asideBodies.end(), fresh), g_asideBodies.end());
                        for (int k = 0; k < 3; k++) SetF(base, fresh + 336 + 4 * k, at[k]);
                        if (who == 1) Guest::Write32(base, fresh + 632, Guest::Read32(base, fresh + 632) & ~0x400u);
                    }
                    if (who == 0) Guest::Write32(base, fresh + 632, Guest::Read32(base, fresh + 632) | 0x400u);   // (not drawn here)
                    const int32_t oldIndex = IndexOf(base, body);
                    { static int told = 0; if (told++ < 60) { printf("coop: host: %s carries %s (weapon %u %s; was %s): his body is entity %d now (%s; was %d, put aside)\n", who == 0 ? "the host" : "the joining player", model.c_str(), number, name.c_str(), shown[who].c_str(), IndexOf(base, fresh), again ? "the one kept for this gun" : "new", oldIndex); fflush(stdout); } }
                    // the old one: hidden, never sent, kept for its gun
                    // (0.5.936) the joining player's old body stays drawn here till the new one has an animation on it
                    if (who == 1) { g_oldShown = body; g_oldShownAt = Now(); }
                    else Guest::Write32(base, body + 632, Guest::Read32(base, body + 632) | 0x400u);
                    g_asideBodies.push_back(body);
                    if (shown[who] != "?") spare[who][shown[who]] = body;
                    body = fresh; shownFor[who] = fresh; shown[who] = model;
                    (who == 0 ? hostBodyIdx : secondBodyIdx) = -1;
                    g_bodyState[who] = BodyState{};
                }
            }
            // (0.5.880) no parent node left weighing with nothing under it (the T-pose)
            for (int who = 0; who < 2; who++)
            {
                const uint32_t b = who == 0 ? g_hostBody : g_secondBody;
                if (!b || IndexOf(base, b) < 0) continue;
                // (0.5.895) the aim up and down, from his own view's pitch (the host's record, the joining player's commands)
                float pitch = 0.0f, share = 0.0f;
                if (who == 0) { const uint32_t rec0 = me ? Record(base, me) : 0; if (rec0) pitch = F(base, rec0 + 172); } else pitch = g_cmd.view[0];
                const int32_t tiltAnim = BodyTilt(ctx, base, Guest::Read32(base, b + 552), g_bodyState[who].move, pitch, share);
                const int n = TidyBodyTree(base, Guest::Read32(base, b + 552), g_bodyState[who].anim, tiltAnim, share);
                static int told = 0;
                if (n && told++ < 30) { printf("coop: host: %s body: %d parent nodes with nothing under them emptied\n", who == 0 ? "the host's" : "the joining player's", n); fflush(stdout); }
            }
            // (0.5.879) the dump: every 3 s, 50 times: the joining player's body and the soldier it was dressed as
            if (g_secondBody && second)
            {
                static double at = 0; static int n = 0;
                if (Now() - at >= 10.0 && n < 30)
                {
                    at = Now(); n++;
                    printf("coop: dump %d: the body's state: move %d animation %d\n", n, g_bodyState[1].move, g_bodyState[1].anim);
                    DumpTree(base, "body", g_secondBody);
                    DumpTree(base, "soldier", g_template);
                    fflush(stdout);
                }
            }
            // (v46) the joining player's body only while his copy is here (its commands coming): loading again or
            // gone, his body stood where it was left, in its bind pose (the host saw him T-posed)
            // (0.5.940) the players' bodies stop no trace: t939b, the hints' search found the MG for the host's player and
            // nothing for the joining player at the same place: his body stands where he is (the host's is far below),
            // and the scripts' notsolid (MakeBody) is for brush models only: its contents (+324) stayed, and the look
            // from his eye met his own body first
            std::vector<uint32_t> allBodies(g_asideBodies); allBodies.push_back(g_secondBody); allBodies.push_back(g_hostBody);
            for (const uint32_t b : allBodies)
                if (b && IndexOf(base, b) >= 0 && Guest::Read32(base, b + 324))
                {
                    static int told = 0;
                    if (told++ < 10) { printf("coop: host: a player's body (entity %d) was solid (contents %08X): not now\n", IndexOf(base, b), Guest::Read32(base, b + 324)); fflush(stdout); }
                    Guest::Write32(base, b + 324, 0);
                }
            if (g_secondBody && IndexOf(base, g_secondBody) >= 0)
            {
                bool here = second && g_cmdAt >= 0 && Now() - g_cmdAt < 1.5;
                // (0.5.936) and only once an animation of his plays on it: a body new at a change of gun stood a frame
                // or more in its bind pose (t935b: "move -1, animation -1")
                {
                    const BodyState& st = g_bodyState[1];
                    const uint32_t tr = Guest::Read32(base, g_secondBody + 552);
                    if (!Valid(tr) || st.anim < 0 || !Guest::Read16(base, tr + 24 + 2 * uint32_t(st.anim))) here = false;
                }
                const uint32_t f = Guest::Read32(base, g_secondBody + 632);
                const uint32_t want = here ? (f & ~0x400u) : (f | 0x400u);
                if (g_oldShown && (here || Now() - g_oldShownAt > 1.0))
                {
                    if (IndexOf(base, g_oldShown) >= 0) Guest::Write32(base, g_oldShown + 632, Guest::Read32(base, g_oldShown + 632) | 0x400u);
                    g_oldShown = 0;
                }
                if (want != f) { Guest::Write32(base, g_secondBody + 632, want); static int told = 0; if (told++ < 20) { printf("coop: host: the joining player's body %s\n", here ? "shown (his copy is here)" : "hidden (his copy is away or loading)"); fflush(stdout); } }
            }
        }
    }

    // (0.5.869) a downed player back in beside the other one. The joining player: room beside the host (from the
    // host's trail), his copy told to put him there (R_MISSION 2,1) and his old place not taken until it has.
    // The host: where the joining player stood a moment ago (his trail), else on him (client 1 is not solid
    // while they are in each other).
    namespace { float g_trail2[16][3] = {}; int g_trail2Count = 0, g_trail2Next = 0; }
    void HostRespawn(PPCContext& ctx, uint8_t* base, int who)
    {
        const uint32_t me = Me(base);
        if (!me || !SecondPlayerAlive(base)) return;
        if (who == 1)
        {
            float room[3]; RoomBeside(base, me, room);
            uint8_t m[16] = { 2, 1 };
            memcpy(m + 4, room, 12);
            SendReliable(R_MISSION, m, sizeof m);
            memcpy(g_holdPos, room, 12); g_hold = true; g_holdAt = Now();
            for (int k = 0; k < 3; k++) { SetF(base, g_rec + 4 * k, room[k]); SetF(base, g_rec + 16 + 4 * k, 0.0f); }
            for (const uint32_t at : { 32u, 336u }) for (int k = 0; k < 3; k++) SetF(base, g_ent + at + 4 * k, room[k]);
            LinkEntity(ctx, base, g_ent);
            printf("coop: host: the joining player comes back in at %.0f %.0f %.0f\n", room[0], room[1], room[2]); fflush(stdout);
            return;
        }
        const float h[3] = { F(base, g_ent + 336), F(base, g_ent + 340), F(base, g_ent + 344) };
        float to[3] = { h[0], h[1], h[2] + 4.0f };
        for (int n = 1; n <= g_trail2Count; n++)
        {
            const float* p = g_trail2[(g_trail2Next - n + 32) % 16];
            const float dx = p[0] - h[0], dy = p[1] - h[1];
            if (fabsf(p[2] - h[2]) > 20.0f || dx * dx + dy * dy > 400.0f * 400.0f) continue;
            if (dx * dx + dy * dy >= 48.0f * 48.0f) { to[0] = p[0]; to[1] = p[1]; to[2] = p[2] + 4.0f; break; }
        }
        const uint32_t rec = Record(base, me);
        if (!rec) return;
        for (int k = 0; k < 3; k++) { SetF(base, rec + 4 * k, to[k]); SetF(base, rec + 16 + 4 * k, 0.0f); }
        for (const uint32_t at : { 32u, 336u }) for (int k = 0; k < 3; k++) SetF(base, me + at + 4 * k, to[k]);
        LinkEntity(ctx, base, me);
        g_trailCount = 0;
        printf("coop: host: the host comes back in at %.0f %.0f %.0f (the joining player at %.0f %.0f %.0f)\n", to[0], to[1], to[2], h[0], h[1], h[2]); fflush(stdout);
    }

    uint32_t SecondPlayer() { return g_ent; }
    uint32_t SecondBody() { return g_secondBody; }
    uint32_t HostBody() { return g_hostBody; }
    bool IsPlayerEntity(const uint8_t* base, uint32_t e) { return e && (e == Me(base) || e == g_ent); }

    // the level scripts' dvars this mission (the letterbox, the gun, the HUD), for the joining copy
    namespace
    {
        std::vector<std::pair<std::string, std::string>> g_dvars;   // (name, value), in order, the latest of each
        uint32_t g_dvarFlags = 0;
        void SendDvar(const std::string& name, const std::string& value, uint32_t flags)
        {
            std::vector<uint8_t> m; m.push_back(E_DVAR);
            m.insert(m.end(), reinterpret_cast<const uint8_t*>(&flags), reinterpret_cast<const uint8_t*>(&flags) + 4);
            m.insert(m.end(), name.begin(), name.end()); m.push_back(0);
            m.insert(m.end(), value.begin(), value.end()); m.push_back(0);
            SendReliable(R_EVENT, m.data(), m.size());
        }
    }
    bool ScriptDvarIs(const char* name, const char* value)
    {
        for (const auto& [n, v] : g_dvars) if (_stricmp(n.c_str(), name) == 0) return v == value;
        return false;
    }
    void HostScriptDvar(const std::string& name, const std::string& value, uint32_t flags)
    {
        if (name.empty()) return;
        bool found = false;
        for (auto& [n, v] : g_dvars) if (_stricmp(n.c_str(), name.c_str()) == 0) { v = value; found = true; }
        if (!found && g_dvars.size() < 256) g_dvars.emplace_back(name, value);
        g_dvarFlags = flags;
        if (PeerConnected()) SendDvar(name, value, flags);
    }

    // (0.5.880) THE T-POSE, found in t879a's dump: the body's tree held parent nodes at full weight with no
    // animation under them (node 189 and its child 222 beside the idle 168 under node 6), left from an earlier
    // move whose animation had ended. A parent node with nothing under it gives the bind pose, and the idle was
    // blended with it: the arms half out. A list entry is 28 bytes: +4 its count of children (0: an animation),
    // +6 its parent's index. Each frame: the parents above the move playing weigh 1, and a parent node with no
    // weighing animation anywhere under it weighs nothing. Returns how many were emptied.
    // ---- (0.5.882) the bodies' moves by name, and the player's own actions on them ----
    // The animations' names are in the game's files and their hash is h * 33 + the lower-case letter (the list
    // entries' +0), so the moves are named here, not taken from what soldiers happened to play: three of the old
    // thirteen were wrong (crouch idle was hideLowWall_2, prone idle prone_shootfeet_straight45up, the step right
    // stand_step_right). Moves 0..12 are how he moves; 13.. are what he DOES, from his own buttons (the record's
    // +1352: 1 fire, 0x10 grenade, 8 aim), sent as they are: bit 1 firing, 2 throwing, 4 aiming.
    namespace
    {
        const char* const BodyMoveName[] = {
            "stand_alertb_idle1", "stand_walk_combat_loop_01", "combatrun_back", "combatrun_left", "combatrun_right", "combatrun_forward_1",
            "crouch_alert_A_idle", "crouchrun_loop_forward_1", "crouchrun_loop_back", "crouchrun_loop_left", "crouchrun_loop_right",
            "prone_aim_idle", "prone_crawl",
            "stand_aim_straight", "crouch_aim_straight",
            "stand_shoot_straight", "crouch_shoot_straight", "prone_shoot_straight",
            "stand_shoot_run_forward", "stand_shoot_run_back", "stand_shoot_run_left", "stand_shoot_run_right",
            "crouch_shoot_run_forward", "crouch_shoot_run_back", "crouch_shoot_run_left", "crouch_shoot_run_right",
            "stand_grenade_throw", "crouch_grenade_throw",
            "reload_stand_rifle", "reload_crouch_rifle", "reload_prone_rifle",
            "melee_right2right_1", "meleecrouch_right2right" };
        constexpr int BodyMoves = int(sizeof BodyMoveName / sizeof BodyMoveName[0]);
    }
    uint32_t BodyMoveHash(int move)
    {
        static uint32_t hash[BodyMoves]; static bool made = false;
        if (!made) { made = true; for (int k = 0; k < BodyMoves; k++) { uint32_t h = 0; for (const char* t = BodyMoveName[k]; *t; t++) h = h * 33 + uint32_t(tolower(static_cast<unsigned char>(*t))); hash[k] = h; } }
        return move >= 0 && move < BodyMoves ? hash[move] : hash[0];
    }
    const char* BodyMoveText(int move) { return move >= 0 && move < BodyMoves ? BodyMoveName[move] : "?"; }
    // (0.5.884) what he does is ONE thing: 0 nothing, 1 aiming, 2 firing, 3 throwing a grenade, 4 reloading, 5 a
    // rifle butt (melee). t883a's weapon states (the record's +136): 0 ready, 1 raising, 2 lowering, 3 firing,
    // 5 reloading, 10 and 11 the melee (weapon animation 8, button 0x10: NOT the grenade as 0.5.882 took it),
    // 15 the throw (button 0x2: lower the rifle, 15, then raise it).
    int BodyAct(int move, uint32_t act)
    {
        if (move < 0 || move > 12) return move;
        switch (act)
        {
        case 3: return move >= 11 ? move : move >= 6 ? 27 : 26;
        case 5: return move >= 11 ? move : move >= 6 ? 32 : 31;
        case 4: return move >= 11 ? 30 : move >= 6 ? 29 : 28;
        case 2: { static const int fire[13] = { 15, 18, 19, 20, 21, 18, 16, 22, 23, 24, 25, 17, 17 }; return fire[move]; }
        case 1: return move == 0 ? 13 : move == 6 ? 14 : move;
        }
        return move;
    }
    // the host: what a player is doing now (0 the host, 1 the joining player). A throw shows for 1.3 s from the
    // press, a melee for 0.8 s and a shot for 0.5 s, so a short one is seen through a snapshot.
    uint32_t HostActBits(const uint8_t* base, int who)
    {
        static double fireUntil[2] = { -1, -1 }, throwUntil[2] = { -1, -1 }, meleeUntil[2] = { -1, -1 }; static bool wasThrow[2] = {};
        bool fire = false, thr = false, aim = false, reload = false, melee = false;
        if (who == 0)
        {
            const uint32_t rec = Record(base, Me(base));
            const uint32_t b = rec ? Guest::Read32(base, rec + 1352) : 0, ws = rec ? Guest::Read32(base, rec + 136) : 0;
            static uint32_t before = 0; fire = ws == 3 && (b & 1); thr = (b & ThrowButtons) != 0 || ws == 15; aim = (b & 8) != 0; reload = (ws >= 5 && ws <= 9) || ws == 14;   /* (0.5.894) 14 is the reload's start, not a throw */ melee = rec && MeleeState(ws, before);
        }
        else if (g_cmdAt >= 0 && Now() - g_cmdAt < 0.5)
        {
            fire = (g_cmd.flags & 0x40000u) != 0; thr = (g_cmd.flags & 0x80000u) != 0; aim = (g_cmd.flags & 0x100000u) != 0;
            reload = (g_cmd.flags & 0x200000u) != 0; melee = (g_cmd.flags & 0x400000u) != 0;
        }
        // (0.5.943) on a mounted gun (0.5.945: the record's +1148, the gun's handle, +1144 1): the body aims, and shoots
        // while the trigger is held (the game has no gunner's animation for a player; t942a: his body stood idle by it)
        {
            const uint32_t pl = who == 0 ? Me(base) : g_ent;
            const uint32_t rec = pl ? Record(base, pl) : 0;
            if (rec && Guest::Read32(base, rec + 1148) != 0)   // (0.5.945: +1148 the gun's handle while on it; t944a)
            {
                aim = true;
                if (Guest::Read32(base, rec + 1352) & 1u) fire = true;
                static bool told[2] = {}; if (!told[who]) { told[who] = true; printf("coop: host: %s is on a mounted gun: his body aims and shoots with it\n", who == 0 ? "the host" : "the joining player"); fflush(stdout); }
            }
        }
        const double now = Now();
        if (fire) fireUntil[who] = now + 0.5;
        if (melee) meleeUntil[who] = now + 0.8;
        if (thr && !wasThrow[who]) throwUntil[who] = now + 1.3;
        wasThrow[who] = thr;
        const uint32_t act = now < throwUntil[who] ? 3u : now < meleeUntil[who] ? 5u : reload ? 4u : now < fireUntil[who] ? 2u : aim ? 1u : 0u;
        { static uint32_t was[2] = {}; static int told = 0; if (act != was[who] && act != 1 && was[who] != 1 && told < 300) { told++; printf("coop: host: %s does %u (was %u) at %.1f s\n", who == 0 ? "the host" : "the joining player", act, was[who], now); fflush(stdout); } was[who] = act; }
        return act;
    }

    int TidyBodyTree(uint8_t* base, uint32_t tree, int32_t anim, int32_t tiltAnim, float tilt)
    {
        if (!Valid(tree) || anim < 0) return 0;
        const uint32_t list = Guest::Read32(base, tree + 8);
        if (!Valid(list)) return 0;
        const uint32_t count = std::min<uint32_t>(Guest::Read32(base, list + 4), 4096), entries = Guest::Read32(base, list + 8);
        if (uint32_t(anim) >= count) return 0;
        static std::vector<uint8_t> alive; alive.assign(count, 0);
        auto up = [&](uint32_t k, uint8_t mark) { for (int guard = 0; guard < 32; guard++) { const uint32_t p = Guest::Read16(base, entries + 28 * k + 6); if (p >= count || p == k) break; alive[p] |= mark; if (!p) break; k = p; } };
        // (0.5.892) the animation asked for is not in the tree yet (its call waited a frame): nothing is touched.
        // The one that played was blended out towards it all the same, and the body stood with nothing weighing
        if (!Guest::Read16(base, tree + 24 + 2 * uint32_t(anim))) return 0;
        up(uint32_t(anim), 2);
        // (0.5.890) SMOOTH MOVES (the user's video of 0.5.889: no T-pose any more, but the other player "still
        // flickers": every change of move, and firing, aiming and reloading change it all the time, was a jump
        // from one pose to the next in one frame). A change is a blend again, a fifth of a second, made here and
        // nowhere else: the animation that played keeps what the new one does not yet weigh, so the two always
        // add up to one whole pose (never a part of one, which is what the T-pose was), and the parent nodes of
        // both are whole. Kept per tree; the weights are written every frame, their goals with them, so the
        // game's own blending has nothing to move.
        struct Mix { int32_t cur = -1, prev = -1; float mix = 1.0f; double at = -1; int32_t tilt = -1, prevTilt = -1; float p = 0, prevP = 0; };
        static std::map<uint32_t, Mix> mixes;
        if (mixes.size() > 16) mixes.clear();
        Mix& mx = mixes[tree];
        const double nowT = Now();
        auto held = [&](int32_t a) { return a >= 0 && uint32_t(a) < count && Guest::Read16(base, tree + 24 + 2 * uint32_t(a)) != 0 && Guest::Read16(base, entries + 28 * uint32_t(a) + 4) == 0; };
        if (mx.cur != anim)
        {
            // (a change in the middle of a blend: the one of the two that shows more is the one blended from)
            const int32_t from = (mx.mix < 0.5f && held(mx.prev)) ? mx.prev : mx.cur;
            // (0.5.895) what it blends from keeps its own aim up or down
            if (from == mx.cur) { mx.prevTilt = mx.tilt; mx.prevP = mx.p; }
            mx.prev = held(from) && from != anim ? from : -1;
            mx.cur = anim; mx.mix = mx.prev >= 0 ? 0.05f : 1.0f;
            mx.tilt = held(tiltAnim) ? tiltAnim : -1; mx.p = mx.tilt >= 0 ? std::clamp(tilt, 0.0f, 1.0f) : 0.0f;
        }
        else if (mx.at >= 0) mx.mix = std::min(1.0f, mx.mix + float(std::clamp(nowT - mx.at, 0.0, 0.1)) * 5.0f);
        // (0.5.895) THE AIM UP AND DOWN: a share of the animation playing is given to its aim-up or aim-down twin
        // (stand_aim_up beside stand_aim_straight ...), more the further he looks; the share moves at 4 a second
        {
            const int32_t wantTilt = held(tiltAnim) ? tiltAnim : -1; const float wantP = wantTilt >= 0 ? std::clamp(tilt, 0.0f, 1.0f) : 0.0f;
            const float step = mx.at >= 0 ? float(std::clamp(nowT - mx.at, 0.0, 0.1)) * 4.0f : 1.0f;
            if (wantTilt != mx.tilt) { if (mx.p > step && held(mx.tilt)) mx.p -= step; else { mx.tilt = wantTilt; mx.p = std::min(mx.p, step); } }
            else mx.p = wantP > mx.p ? std::min(wantP, mx.p + step) : std::max(wantP, mx.p - step);
            if (mx.tilt >= 0 && !held(mx.tilt)) { mx.tilt = -1; mx.p = 0; }
            if (mx.prevTilt >= 0 && !held(mx.prevTilt)) { mx.prevTilt = -1; mx.prevP = 0; }
        }
        mx.at = nowT;
        if (mx.prev >= 0 && (!held(mx.prev) || mx.mix >= 1.0f)) { mx.prev = -1; mx.prevTilt = -1; mx.prevP = 0; }
        if (mx.prev < 0) mx.mix = 1.0f;
        if (mx.prev >= 0) up(uint32_t(mx.prev), 2);
        if (mx.tilt >= 0) up(uint32_t(mx.tilt), 2);
        if (mx.prev >= 0 && mx.prevTilt >= 0) up(uint32_t(mx.prevTilt), 2);
        const float in = mx.mix * mx.mix * (3.0f - 2.0f * mx.mix);   // (eased)
        {
            int emptied = 0;
            for (uint32_t k = 0; k < count; k++)
            {
                const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * k); if (!sl) continue;
                const bool parent = Guest::Read16(base, entries + 28 * k + 4) != 0;
                float want = 0.0f;
                if (parent) want = (alive[k] & 2) ? 1.0f : 0.0f;
                else
                {
                    if (k == uint32_t(anim)) want += in * (1.0f - mx.p);
                    if (mx.tilt >= 0 && k == uint32_t(mx.tilt)) want += in * mx.p;
                    if (mx.prev >= 0 && k == uint32_t(mx.prev)) want += (1.0f - in) * (mx.prevTilt >= 0 ? 1.0f - mx.prevP : 1.0f);
                    if (mx.prev >= 0 && mx.prevTilt >= 0 && k == uint32_t(mx.prevTilt)) want += (1.0f - in) * mx.prevP;
                }
                const uint32_t r = InfoPool + 52u * sl;
                if (F(base, r + 36) != want || F(base, r + 32) != want)
                {
                    if (want == 0.0f && parent && F(base, r + 36) > 0.0f) emptied++;
                    SetF(base, r + 36, want); SetF(base, r + 32, want);
                }
            }
            return emptied;
        }
    }

    int BodyTilt(PPCContext& ctx, uint8_t* base, uint32_t tree, int move, float pitch, float& share)
    {
        share = 0.0f;
        if (!Valid(tree)) return -1;
        // (the moves' numbers: BodyMoveName: 13 stand_aim_straight, 14 crouch_aim_straight, 15 stand_shoot_straight, 16 crouch_shoot_straight)
        const char* up = nullptr; const char* down = nullptr;
        switch (move)
        {
        case 13: up = "stand_aim_up"; down = "stand_aim_down"; break;
        case 14: up = "crouch_aim_up"; down = "crouch_aim_down"; break;
        case 15: up = "stand_shoot_up"; down = "stand_shoot_down"; break;
        case 16: up = "crouch_shoot_up"; down = "crouch_shoot_down"; break;
        default: return -1;
        }
        pitch = Pitch180(pitch);   // (the view's pitch: under 0 is up)
        const float p = std::clamp(fabsf(pitch) / 45.0f, 0.0f, 1.0f);
        if (p < 0.04f) return -1;
        const char* name = pitch < 0.0f ? up : down;
        uint32_t h = 0; for (const char* t = name; *t; t++) h = h * 33 + uint32_t(tolower(static_cast<unsigned char>(*t)));
        const uint32_t list = Guest::Read32(base, tree + 8);
        if (!Valid(list)) return -1;
        const int32_t anim = AnimIndex(base, list, h);
        if (anim < 0) { static int told = 0; if (told++ < 8) { printf("coop: the aim's animation %s is not in the body's list\n", name); fflush(stdout); } return -1; }
        uint16_t sl = Guest::Read16(base, tree + 24 + 2 * uint32_t(anim));
        if (!sl)
        {
            PPCContext c = CallCtx(ctx);
            c.r3.u64 = tree; c.r4.u64 = uint32_t(anim); c.f1.f64 = 1.0; c.f2.f64 = 0.0;
            CallGuest(c, base, 0x82500B80);   // set anim, the parents too (its weight is TidyBodyTree's)
            sl = Guest::Read16(base, tree + 24 + 2 * uint32_t(anim));
            static int told = 0; if (told++ < 40) { printf("coop: a body aims %s (pitch %.0f): %s set, slot %u\n", pitch < 0.0f ? "up" : "down", pitch, name, sl); fflush(stdout); }
            if (sl) { SetF(base, InfoPool + 52u * sl + 36, 0.0f); SetF(base, InfoPool + 52u * sl + 32, 0.0f); }
        }
        if (!sl) return -1;
        // kept going: begun again before its end (run out, the tree lets it go)
        { const uint32_t r = InfoPool + 52u * sl; if (F(base, r + 16) >= 0.9f) { SetF(base, r + 16, 0.0f); SetF(base, r + 20, 0.0f); } }
        share = p;
        return anim;
    }

    // (0.5.916) an effect at p is the blast of one of the joining player's own missiles (one alive, or ended within a
    // second and a half, within 160 units): his copy plays its own
    bool NearHisBlast(const float p[3])
    {
        for (const auto& [i, h] : g_hisMissiles)
        {
            if (h.goneAt >= 0 && Now() - h.goneAt > 1.5) continue;
            const float dx = p[0] - h.pos[0], dy = p[1] - h.pos[1], dz = p[2] - h.pos[2];
            if (dx * dx + dy * dy + dz * dz < 160.0f * 160.0f) return true;
        }
        return false;
    }
    void HostNewPeer() { g_cmdAt = -1; g_cmd = CmdIn{}; g_csResend = true; }
    // (0.5.875) the joining player's Use held, from his own pad (K_CMD flags 0x20000), while his commands are fresh
    bool JoinerUseHeld() { return g_cmdAt >= 0 && Now() - g_cmdAt < 0.5 && (g_cmd.flags & 0x20000u) != 0; }
    void HostMissionStart(const std::string& name)
    {
        g_client1 = g_ent = g_rec = g_index = 0;
        g_inSince = -1;
        g_sent.clear(); g_removed.clear();
        for (auto& m : g_names) m.clear();
        g_csSent.clear();
        for (bool& k : g_hudKnown) k = false;
        g_hostBody = g_secondBody = g_template = 0;
        g_dvars.clear();
        uint8_t m[68] = {};
        const uint32_t count = MissionCount();
        memcpy(m, &count, 4);
        strncpy(reinterpret_cast<char*>(m + 4), name.c_str(), 63);
        SendReliable(R_MAPSTART, m, sizeof m);
        g_csResend = true;
    }

    void HostReceive(PPCContext& ctx, uint8_t* base, Kind kind, const uint8_t* data, size_t size)
    {
        if (kind == K_CMD && size >= sizeof(CmdIn))
        {
            CmdIn c; memcpy(&c, data, sizeof c);
            if (g_cmdAt >= 0 && int32_t(c.seq - g_cmd.seq) <= 0) return;   // (an older one, late)
            if (((c.flags ^ g_cmd.flags) & 0x20000u) && g_cmdAt >= 0) { static int told = 0; if (told++ < 40) { printf("coop: host: the joining player %s Use\n", (c.flags & 0x20000u) ? "holds" : "lets go of"); fflush(stdout); } }
            // (the joining copy frozen now: the scripts' dvars so far, again: its own scripts are stopped)
            if ((c.flags & 2) && !(g_cmd.flags & 2) && MyMap() == c.map)
            {
                for (const auto& [n, v] : g_dvars) SendDvar(n, v, g_dvarFlags);
                ResendNames();
                printf("coop: host: the joining copy is frozen: the scripts' %zu dvars sent again\n", g_dvars.size()); fflush(stdout);
            }
            // (v43) the joining copy now in our mission: every name and config string again. Those of the
            // mission's first second went while it was still loading, and it lost them (Saint-Lo: 7 names
            // of 400 known there, the host's world drawn as a handful of things)
            if (MyMap() == c.map && (g_cmdAt < 0 || strncmp(g_cmd.map, c.map, 31) != 0))
            {
                g_csResend = true;
                printf("coop: host: the joining copy is in %s: names and config strings sent again\n", c.map); fflush(stdout);
            }
            g_cmd = c; g_cmd.map[31] = 0;
            if (g_cmdAt < 0) { printf("coop: host: the joining player's commands are coming (he is at %.0f %.0f %.0f in %s)\n", c.origin[0], c.origin[1], c.origin[2], g_cmd.map); fflush(stdout); }
            g_cmdAt = Now(); g_cmds++;
        }
        (void)ctx; (void)base;
    }
    void HostReceiveReliable(PPCContext& ctx, uint8_t* base, RelKind kind, const uint8_t* data, size_t size)
    {
        if (kind == R_INVMINE) { InventoryHostReceive(data, size); return; }   // (0.5.902)
        // his own game hurt him (a fall, a trigger, a fire): given to client 1 here
        if (kind == R_SELFHIT && size >= 8 && SecondPlayerAlive(base))
        {
            int32_t d[2]; memcpy(d, data, 8);
            printf("coop: host: the joining player's own game hurt him: %d (means %d)\n", d[0], d[1]); fflush(stdout);
            if (d[0] > 0 && d[0] < 100000) InjectDamage(ctx, base, g_ent, d[0], d[1]);
        }
    }

    // client 1's think: his command where client 0's goes, his view angles,
    // then he is put where his copy says he is
    void HostClientThink(PPCContext& ctx, uint8_t* base)
    {
        if (!SecondPlayerAlive(base)) return;
        const bool his = g_cmdAt >= 0 && Now() - g_cmdAt < 1.0;
        if (!his) return;
        PPCContext c = CallCtx(ctx);
        c.r3.u64 = Guest::Read32(base, 0x82A2A1B8); c.r4.u64 = 6; c.r5.u64 = g_client1 + 360;
        uint8_t hostCmd[48];
        const uint32_t slot = CommandSlot(base);
        memcpy(hostCmd, base + slot, 48); memcpy(base + slot, g_cmd.cmd, 48);
        // his command on the host's clock (its time is his copy's: behind the record's, it would not be run at all)
        memcpy(base + slot, hostCmd, 4);
        {
            static uint32_t lastButtons = 0;
            uint32_t b; memcpy(&b, g_cmd.cmd + 4, 4); b = __builtin_bswap32(b);
            static int told = 0;
            if (b != lastButtons && told < 200) { told++; printf("coop: host: the joining player's buttons %08X (his record's command time %08X, the host's %08X)\n", Guest::Read32(base, slot + 4), Guest::Read32(base, g_rec + 32), Guest::Read32(base, slot)); fflush(stdout); }
            lastButtons = b;
        }
        // (0.5.955) LET GO OF USE AFTER A SWAP. The user (t952u): "i cant seem to pick up the weapon on the ground just the
        // ammo": his swaps happened here, then 2 s later the gun just dropped was taken back, and again, Use still held.
        // The host's own player holding Use 10 s swapped ONCE (t954b): the game wants Use let go first, and that latch is
        // not this client's (his copy never sees the swap). So after a swap of his (a gun gained and one lost between two
        // thinks) his Use (0x40, t952a) is held back from his command until he lets go of it.
        {
            static uint32_t ownedWas[5] = {}; static uint32_t forRec = 0; static bool latch = false; static int told = 0;
            uint32_t owned[5]; bool gained = false, lost = false;
            for (int k = 0; k < 5; k++) { owned[k] = Guest::Read32(base, g_rec + 1024 + 4 * k); if (forRec == g_rec) { if (owned[k] & ~ownedWas[k]) gained = true; if (ownedWas[k] & ~owned[k]) lost = true; } ownedWas[k] = owned[k]; }
            forRec = g_rec;
            const uint32_t buttons = Guest::Read32(base, slot + 4);
            if (gained && lost && (buttons & 0x40u)) { latch = true; if (told < 20) { told++; printf("coop: host: the joining player swapped guns: his Use held back till he lets go\n"); fflush(stdout); } }
            if (latch)
            {
                if (buttons & 0x40u) Guest::Write32(base, slot + 4, buttons & ~0x40u);
                else { latch = false; if (told < 20) { told++; printf("coop: host: the joining player let go of Use\n"); fflush(stdout); } }
            }
        }
        for (int k = 0; k < 3; k++) SetF(base, g_rec + 172 + 4 * k, g_cmd.view[k]);
        // (0.5.941) HIS VIEW HERE IS HIS VIEW THERE. The think makes the view from the command's angles plus this
        // client's delta angles (record +88..+96, set by SetClientViewAngle sub_82540168: short angle minus the
        // command's +1360..+1368). Client 1's deltas here are not his copy's (his spawn, any setplayerangles of his
        // copy's), so on the host he faced elsewhere than he looked: t940b, his copy at yaw -45, here 109.6. His
        // hints looked the wrong way (no MG prompt), and his shots went where he did not aim. The deltas are set
        // so his command gives the view his copy has. COD3_COOP_VIEWFIX=0: as before.
        {
            static const bool off = []() { const char* t = getenv("COD3_COOP_VIEWFIX"); return t && t[0] == '0'; }();
            if (!off)
            {
                static int told = 0; int32_t was[3] = {}, now[3] = {};
                for (int k = 0; k < 3; k++)
                {
                    uint32_t a; memcpy(&a, g_cmd.cmd + 12 + 4 * k, 4); a = __builtin_bswap32(a);
                    const int32_t want = int32_t(lrintf(g_cmd.view[k] * (65536.0f / 360.0f))) & 0xFFFF;
                    was[k] = int32_t(Guest::Read32(base, g_rec + 88 + 4 * k));
                    now[k] = (want - int32_t(a & 0xFFFF)) & 0xFFFF;
                    Guest::Write32(base, g_rec + 88 + 4 * k, uint32_t(now[k]));
                }
                if (told < 10 && (((was[1] - now[1]) & 0xFFFF) > 64 && ((now[1] - was[1]) & 0xFFFF) > 64)) { told++; printf("coop: host: the joining player's view deltas here set to his copy's view (yaw delta %d -> %d)\n", was[1], now[1]); fflush(stdout); }
            }
        }
        // (0.5.915) HIS SHOTS AND THROWS. The weapon's fire (sub_82569C40, from ClientThink's sub_8256C260 ->
        // sub_8256B060) fires only for the LOCAL client's entity: it compares the player with [0x82A2AB40][local
        // index + 1] and does nothing for anyone else. t914b: his throw made no grenade here; no test ever had a hit
        // of his on the host. For his think, that slot (+4: the host's player's) is his, and given back after.
        // COD3_COOP_C1LOCAL=0: as before.
        // (0.5.950) only for the WHOLE think when COD3_COOP_THINKLOCAL=1: the user saw the host's own gun flash at each
        // shot of the joining player's (the first-person weapon follows "the local player" somewhere in the think).
        // Now the slot is his only inside the fire itself (the sub_82569C40 hook below), which is all that needed it.
        static const bool notLocal = []() { const char* t = getenv("COD3_COOP_C1LOCAL"); const char* w = getenv("COD3_COOP_THINKLOCAL"); return (t && t[0] == '0') || !(w && w[0] == '1'); }();
        const uint32_t clients = Guest::Read32(base, 0x82A2AB40);
        const uint32_t slot1 = Valid(clients) && Guest::Read32(base, 0x82A2A1C8) == 0 ? clients + 4 : 0;
        const uint32_t hostInSlot = slot1 ? Guest::Read32(base, slot1) : 0;
        if (slot1 && !notLocal) Guest::Write32(base, slot1, g_ent);
        CallGuest(c, base, 0x82497450);   // VM_Call(game, 6: ClientThink, his entity number)
        if (slot1 && !notLocal) Guest::Write32(base, slot1, hostInSlot);
        memcpy(base + slot, hostCmd, 48);
        PlaceSecond(base);
        static int told = 0;
        if (told++ % 600 == 0) { printf("coop: host: client 1 thought (%llu commands in): at %.0f %.0f %.0f, health %d\n", (unsigned long long)g_cmds, F(base, g_ent + 336), F(base, g_ent + 340), F(base, g_ent + 344), int32_t(Guest::Read32(base, g_ent + 776))); fflush(stdout); }
    }

    void HostFrame(PPCContext& ctx, uint8_t* base)
    {
        const double now = Now();
        // (0.5.975) a solo test (COD3_COOP_SOLO): no friend, only the tests' aids run (TESTGO, TESTLIVE, TESTGOD)
        static const bool solo = getenv("COD3_COOP_SOLO") != nullptr;
        // (0.5.984) DIAGNOSTIC: the host's record +44 (what he is allowed: stances, ...) and +40/+48, every 5 s in a mission:
        // t983d, the co-op host's melee never began (four presses seen); alone it does (t983e)
        if (InMission(base))
        {
            static double at = 0; static int told = 0;
            if (Now() - at > 5.0 && told < 120)
            {
                at = Now();
                if (const uint32_t me0 = Me(base)) if (const uint32_t r0 = Record(base, me0))
                {
                    told++;
                    printf("coop: host: my record +40 %08X +44 %08X +48 %08X +12 %08X, entity +632 %08X, %s\n", Guest::Read32(base, r0 + 40), Guest::Read32(base, r0 + 44), Guest::Read32(base, r0 + 48), Guest::Read32(base, r0 + 12), Guest::Read32(base, me0 + 632), PeerConnected() ? "a friend in" : "alone");
                    // (0.5.985) the melee's own tests (sub_824936E8, pmove): [0x82A92360] +1512 must be set and +180 not 7;
                    // the record's +128 (an entity whose record's +1556 ...), +1556 of mine
                    const uint32_t p = Guest::Read32(base, 0x82A92360u);
                    printf("coop: host: melee tests: [82A92360] %08X (+1512 %08X, +180 %08X, +172 %08X), my record +128 %08X +1556 %08X, pm [829F1040] %08X\n",
                        p, Valid(p) ? Guest::Read32(base, p + 1512) : 0, Valid(p) ? Guest::Read32(base, p + 180) : 0, Valid(p) ? Guest::Read32(base, p + 172) : 0,
                        Guest::Read32(base, r0 + 128), Guest::Read32(base, r0 + 1556), Guest::Read32(base, 0x829F1040u));
                    fflush(stdout);
                }
            }
        }
        if (solo && InMission(base) && !PeerConnected())
        {
            if (const uint32_t me0 = Me(base)) if (const uint32_t r0 = Record(base, me0))
            {
                TestGo(ctx, base, me0, r0); TestLive(base, me0); TestAimModel(ctx, base, me0); TestViewScan(base, me0);
                static const bool god = getenv("COD3_COOP_TESTGOD") != nullptr;
                if (god && Guest::Read32(base, me0 + 780)) Guest::Write32(base, me0 + 776, Guest::Read32(base, me0 + 780));
            }
            g_inSince = -1; return;
        }
        if (!InMission(base) || !PeerConnected()) { g_inSince = -1; return; }
        if (g_inSince < 0) g_inSince = now;
        // (0.5.983) the host's player linked to a thing (a truck, a jeep's gun) or let go, and the scene count: t982h, the
        // co-op host held Use at Saint-Lo's truck and never got on (the solo host did, t982f)
        {
            static uint32_t linkWas = 0; static uint32_t scenesWas = 0;
            const uint32_t me0 = Me(base);
            const uint32_t link = me0 ? Guest::Read32(base, me0 + 916) : 0, scenes = Guest::Read32(base, 0x825F51A0);
            if (link != linkWas || scenes != scenesWas)
            {
                static int told = 0;
                if (told++ < 200) { printf("coop: host: my player's link %08X -> %08X, scenes %u -> %u, at %.0f %.0f %.0f\n", linkWas, link, scenesWas, scenes, me0 ? F(base, me0 + 336) : 0.0f, me0 ? F(base, me0 + 340) : 0.0f, me0 ? F(base, me0 + 344) : 0.0f); fflush(stdout); }
                linkWas = link; scenesWas = scenes;
            }
        }
        // the mission started again here (a checkpoint after a failure): the player's entity is a new one
        {
            static uint32_t meWas = 0; static std::string mapWas;
            const uint32_t me = Me(base);
            if (meWas && me != meWas && mapWas == MyMap())
            {
                MissionCounter()++;
                printf("coop: host: the mission started again here (count %u): the joining player comes back beside the host\n", MissionCount()); fflush(stdout);
                g_ent = 0; g_hostBody = g_secondBody = g_template = 0; g_bodyState[0] = g_bodyState[1] = BodyState{};
                uint8_t m[16] = { 2 };
                const float yaw = F(base, me + 356) * 0.0174533f;
                const float o[3] = { F(base, me + 336) - 48.0f * sinf(yaw), F(base, me + 340) + 48.0f * cosf(yaw), F(base, me + 344) + 4.0f };
                memcpy(m + 4, o, 12);
                SendReliable(R_MISSION, m, sizeof m);
                LastStandMissionStart();
            }
            meWas = me; mapWas = MyMap();
        }
        if (g_csResend && now - g_inSince > 0.5) { g_csResend = false; SendAllConfigstrings(base); ResendNames(); }
        // the second player once the joining player is in this mission too
        const bool joinerIn = g_cmdAt >= 0 && now - g_cmdAt < 2.0 && (g_cmd.flags & 1) && MyMap() == g_cmd.map;
        if (!SecondPlayerAlive(base))
        {
            if (g_ent)
            {
                printf("coop: host: the second player's entity is gone (the mission restarted?): made again, beside the host\n"); fflush(stdout); g_ent = 0; g_trailCount = 0;
                LastStandMissionStart();   // (v45u: St. Bert's checkpoint kept the host's entity: the joining player's last stand went on into the restarted mission, he bled out again and again)
                if (const uint32_t me = Me(base))
                {
                    RoomBeside(base, me, g_holdPos);
                    g_hold = true; g_holdAt = Now();
                    uint8_t m[16] = { 2 };
                    memcpy(m + 4, g_holdPos, 12);
                    SendReliable(R_MISSION, m, sizeof m);
                }
            }
            static double tryAt = -10;
            // (not while the level's intro has the host's player: its camera's animation waits for him alone,
            // and with a second player the intro never ends; the joining copy plays its own intro meanwhile)
            const bool intro = ScriptDvarIs("letterbox_enabled", "1") || ScriptDvarIs("introScreen", "1") || Guest::Read32(base, 0x825F51A0) != 0;
            static double introToldAt = -100;
            if (joinerIn && intro && now - g_inSince < 120.0 && now - introToldAt > 10.0) { introToldAt = now; printf("coop: host: the level's intro is playing: the second player after it\n"); fflush(stdout); }
            if (joinerIn && (!intro || now - g_inSince > 120.0) && now - g_inSince > 2.0 && now - tryAt > 5.0) { tryAt = now; MakeSecondPlayer(ctx, base); }
        }
        Trail(base);
        // (0.5.869) where the joining player has stood (a new point each 40 units), for the host's respawn beside him
        // (0.5.960) never while a scripted scene plays (a ride, a struggle, a cutscene) or while he moves up or down:
        // t959u, the host came back in on a point of the joining player's truck ride (his copy sets him beside the
        // host each frame, through things) and fell out of the map ("means 24")
        if (SecondPlayerAlive(base) && g_cmdAt >= 0 && now - g_cmdAt < 1.0 && !Holding() && Guest::Read32(base, 0x825F51A0) == 0 && fabsf(g_cmd.velocity[2]) < 4.0f)
        {
            const float* q = g_trail2[(g_trail2Next + 15) % 16];
            const float dx = g_cmd.origin[0] - q[0], dy = g_cmd.origin[1] - q[1], dz = g_cmd.origin[2] - q[2];
            if (!g_trail2Count || dx * dx + dy * dy + dz * dz > 40.0f * 40.0f) { memcpy(g_trail2[g_trail2Next], g_cmd.origin, 12); g_trail2Next = (g_trail2Next + 1) % 16; g_trail2Count = std::min(g_trail2Count + 1, 16); }
        }
        PlaceSecond(base);
        Bodies(ctx, base);
        {
            static uint32_t scenesWas = 0;
            const uint32_t scenes = Guest::Read32(base, 0x825F51A0);
            if (scenes != scenesWas) { printf("coop: host: %u scripted scene(s) playing (was %u)\n", scenes, scenesWas); fflush(stdout); scenesWas = scenes; }
        }
        // (0.5.938) a test aid: COD3_COOP_LISTTYPE=10 lists the entities of that type once a mission (where they stand)
        {
            static const int listType = []() { const char* t = getenv("COD3_COOP_LISTTYPE"); return t ? atoi(t) : -1; }();
            static uint32_t doneFor = 0xFFFFFFFFu;
            if (listType >= 0 && doneFor != MissionCount() && g_ent)
            {
                doneFor = MissionCount();
                for (uint32_t i = 2; i < 4096; i++)
                {
                    const uint32_t t = EntityAt(base, i);
                    if (!t || base[t] != listType) continue;
                    printf("coop: test: type %d: %u '%s' %s at %.0f %.0f %.0f yaw %.0f\n", listType, i, BrocText(base, t + 564).c_str(), ModelSlot(base, t, 0) ? ResName(base, ModelSlot(base, t, 0)).c_str() : "-", F(base, t + 336), F(base, t + 340), F(base, t + 344), F(base, t + 356));
                }
                fflush(stdout);
            }
        }
        // (0.5.965) a test aid: COD3_COOP_TESTGOD=1 tops both players' health up each frame (hands-off runs through
        // fights: Falaise's jeep, where both went down and back to the checkpoint before the ride ended)
        {
            static const bool god = []() { const char* t = getenv("COD3_COOP_TESTGOD"); return t && t[0] == '1'; }();
            if (god)
                for (const uint32_t e : { Me(base), g_ent })
                    if (e && int32_t(Guest::Read32(base, e + 776)) > 0 && int32_t(Guest::Read32(base, e + 780)) > 0) Guest::Write32(base, e + 776, Guest::Read32(base, e + 780));
        }
        // (0.5.959) where the host's player is, every 2 s (to find the levels' struggles and other scripted actions)
        {
            static double at = -10;
            const uint32_t me = Me(base);
            if (me && Now() - at > 2.0) { at = Now(); printf("coop: where: host %.0f %.0f %.0f yaw %.0f\n", F(base, me + 336), F(base, me + 340), F(base, me + 344), F(base, me + 356)); fflush(stdout); }
        }
        // (0.5.949) A PROBE: guns lying in the world (class weapon_*): where they come and when they go, with each
        // player's distance (dropped by the dead, picked up by whom). For the joining player's pickups off the ground.
        {
            struct Item { std::string cls; float pos[3]; };
            static std::map<uint32_t, Item> items; static int told = 0; static double lookedAt = -10;
            const bool look = Now() - lookedAt > 0.25;
            if (look) lookedAt = Now();
            std::set<uint32_t> seen;
            const uint32_t hostEnt = EntityAt(base, 0);
            auto dist = [&](uint32_t e, const float* p) { if (!e) return -1.0f; const float dx = F(base, e + 336) - p[0], dy = F(base, e + 340) - p[1]; return std::sqrt(dx * dx + dy * dy); };
            for (uint32_t i = 2; look && i < 4096 && told < 200; i++)
            {
                const uint32_t t = EntityAt(base, i);
                if (!t) continue;
                const std::string cls = BrocText(base, t + 564);
                if (cls.compare(0, 7, "weapon_") != 0) continue;
                seen.insert(i);
                if (items.count(i) && items[i].cls == cls) { items[i].pos[0] = F(base, t + 336); items[i].pos[1] = F(base, t + 340); items[i].pos[2] = F(base, t + 344); continue; }
                Item it{ cls, { F(base, t + 336), F(base, t + 340), F(base, t + 344) } };
                items[i] = it; told++;
                // (0.5.954) for COD3_COOP_TESTGUN: the newest gun within 900 of the joining player
                static const bool testGun = getenv("COD3_COOP_TESTGUN") != nullptr;
                if (testGun && g_ent && dist(g_ent, it.pos) >= 0 && dist(g_ent, it.pos) < 900.0f)
                    if (FILE* f = fopen("coop-testgun.txt", "w")) { fprintf(f, "%.1f %.1f %.1f\n", it.pos[0], it.pos[1], it.pos[2]); fclose(f); }
                printf("coop: item: %u %s (type %u) at %.0f %.0f %.0f; the host's player %.0f away, the joining player %.0f\n", i, cls.c_str(), unsigned(base[t]), it.pos[0], it.pos[1], it.pos[2], dist(hostEnt, it.pos), dist(g_ent, it.pos));
                fflush(stdout);
            }
            for (auto it = items.begin(); look && it != items.end(); )
            {
                if (seen.count(it->first)) { ++it; continue; }
                if (told < 200) { told++; printf("coop: item: %u %s gone from %.0f %.0f %.0f; the host's player %.0f away, the joining player %.0f\n", it->first, it->second.cls.c_str(), it->second.pos[0], it->second.pos[1], it->second.pos[2], dist(hostEnt, it->second.pos), dist(g_ent, it->second.pos)); fflush(stdout); }
                it = items.erase(it);
            }
        }
        // (0.5.914) A PROBE: every missile here (type 3: grenades), from its first frame to its last: t913a, the joining
        // player's frag was thrown here (his count 4 -> 3, its sounds) but no explosion of it was seen
        {
            struct Missile { double born; float pos[3]; std::string model; uint32_t words[4]; std::string cls; };
            static std::map<uint32_t, Missile> live; static int told = 0;
            std::set<uint32_t> now;
            for (uint32_t i = 2; i < 4096; i++)
            {
                const uint32_t m = EntityAt(base, i);
                if (!m || base[m] != 3) continue;
                now.insert(i);
                auto it = live.find(i);
                if (it == live.end())
                {
                    Missile x{ Now(), { F(base, m + 336), F(base, m + 340), F(base, m + 344) }, ModelSlot(base, m, 0) ? ResName(base, ModelSlot(base, m, 0)) : std::string("-"), { Guest::Read32(base, m + 548), Guest::Read32(base, m + 552), Guest::Read32(base, m + 616), Guest::Read32(base, m + 620) }, BrocText(base, m + 564) };
                    if (told++ < 80)
                    {
                        const uint32_t me = Me(base), sec = SecondPlayer();
                        const auto d = [&](uint32_t p) { if (!p) return -1.0f; const float dx = F(base, p + 336) - x.pos[0], dy = F(base, p + 340) - x.pos[1]; return sqrtf(dx * dx + dy * dy); };
                        printf("coop: probe: missile %u (%s, class '%s') made at %.0f %.0f %.0f: %.0f from the host's player, %.0f from the joining player\n", i, x.model.c_str(), BrocText(base, m + 564).c_str(), x.pos[0], x.pos[1], x.pos[2], d(me), d(sec));
                        fflush(stdout);
                    }
                    live[i] = x;
                    // (0.5.917) his own: made within 64 units of him (flat) and nearer him than the host's player
                    if (g_ent && Me(base))
                    {
                        const auto flat = [&](uint32_t p) { const float dx = F(base, p + 336) - x.pos[0], dy = F(base, p + 340) - x.pos[1]; return sqrtf(dx * dx + dy * dy); };
                        const float his = flat(g_ent), host = flat(Me(base));
                        // (0.5.937) frags only: his copy makes its own frag's blast, but not a smoke (t936c: his smoke showed
                        // on the host and NOT on his own screen while the host's was held back): smokes are the host's for both
                        if (his < 64.0f && his < host && x.cls == "grenade")
                        {
                            g_hisMissiles[i] = HisMissile{ GenAt(base, i), { x.pos[0], x.pos[1], x.pos[2] }, -1 };
                            static int toldHis = 0;
                            if (toldHis++ < 30) { printf("coop: host: missile %u is the joining player's own (his copy has its own): not sent\n", i); fflush(stdout); }
                        }
                    }
                }
                else for (int k = 0; k < 3; k++) it->second.pos[k] = F(base, m + 336 + 4 * k);
            }
            // (0.5.916) the joining player's own missiles: when each ends (its blast's effects are not sent near there)
            for (auto it = g_hisMissiles.begin(); it != g_hisMissiles.end();)
            {
                const uint32_t m = EntityAt(base, it->first);
                const bool alive = it->second.goneAt < 0 && m && base[m] == 3;
                if (alive) { for (int k = 0; k < 3; k++) it->second.pos[k] = F(base, m + 336 + 4 * k); ++it; continue; }
                if (it->second.goneAt < 0) it->second.goneAt = Now();
                if (Now() - it->second.goneAt > 3.0 && !(m && BrocText(base, m + 564) == "grenade")) { it = g_hisMissiles.erase(it); continue; }   // (0.5.922: kept while its leftover stays)
                ++it;
            }
            for (auto it = live.begin(); it != live.end();)
            {
                if (now.count(it->first)) { ++it; continue; }
                if (told++ < 80) { printf("coop: probe: missile %u (%s) gone after %.1f s at %.0f %.0f %.0f\n", it->first, it->second.model.c_str(), Now() - it->second.born, it->second.pos[0], it->second.pos[1], it->second.pos[2]); fflush(stdout); }
                it = live.erase(it);
            }
        }
        Snapshot(base);
        HudSync(base);
        if (SecondPlayerAlive(base)) { const uint32_t me0 = Me(base); InventoryHostFrame(base, me0 ? Record(base, me0) : 0, g_rec); }   // (0.5.901)
        if (const uint32_t me0 = Me(base)) if (const uint32_t r0 = Record(base, me0)) { TestGo(ctx, base, me0, r0); TestLive(base, me0); TestAimModel(ctx, base, me0); TestViewScan(base, me0); }   // (0.5.907, 0.5.972, 0.5.977: tests)
        // the joining player's state here (K_PS): his health; the rest in later steps
        static double psAt = 0;
        if (SecondPlayerAlive(base) && now - psAt > 0.05)
        {
            psAt = now;
            // health, max health, his number here, flags (1: the host is in a scripted scene), the host's
            // player's "allow" flags (record +44, 0x01F00000: stand, crouch, prone... set means not allowed)
            const uint32_t scenes = Guest::Read32(base, 0x825F51A0);
            const uint32_t me = Me(base);
            // then the host's own: his health, its most, the number of the body drawn for him, his place (for the HUD)
            struct { int32_t i[8]; float pos[3]; float yaw; uint32_t hint[3]; } ps{ { int32_t(Guest::Read32(base, g_ent + 776)), int32_t(Guest::Read32(base, g_ent + 780)), int32_t(g_index), (scenes || ScriptDvarIs("letterbox_enabled", "1") || ScriptDvarIs("introScreen", "1") ? 1 : 0) | (me && Guest::Read32(base, me + 916) ? 2 : 0),
                              int32_t(me ? Guest::Read32(base, Record(base, me) + 44) & 0x01F00000u : 0),
                              me ? int32_t(Guest::Read32(base, me + 776)) : 0, me ? int32_t(Guest::Read32(base, me + 780)) : 0,
                              g_hostBody ? IndexOf(base, g_hostBody) : -1 }, {} };
            if (me) { for (int k = 0; k < 3; k++) ps.pos[k] = F(base, me + 336 + 4 * k); ps.yaw = F(base, Record(base, me) + 176); }
            // (0.5.919) his prompt here (record +1156, +1160, +1164: what the game's hints found for him in the host's world)
            if (g_rec) for (int k = 0; k < 3; k++) ps.hint[k] = Guest::Read32(base, g_rec + 1156 + 4 * k);
            SendUnreliable(K_PS, &ps, sizeof ps);
        }
    }
}

using namespace Coop;

// (0.5.897) THE JOINING PLAYER'S TRIGGERS. The level's scripts are compiled code that asks who touched a trigger and
// acts for the host's player only (tests t897a/b: the host walking up the first path gets eight lines of speech, the
// joining player walking the same way two). A touch (and a use) is queued by sub_825471D8 as (the trigger's handle,
// the toucher's handle, the trigger's +204, the toucher's +204) at 0x82A4E790 + 3196, 16 bytes each, the count at
// +7292, and told to the scripts once a server frame by this function (the twins of multiplayer's sub_82567328,
// 0x82C281D0 and sub_8256FFC8: bots.cpp; the entity table is 0x82A2DAD0: 8 bytes an entity, its address and its
// generation; a handle is the index with the generation from bit 14 up). A touch by client 1 is told as the host's
// player's: whatever one player sets off is set off, for both. COD3_COOP_TRIGGERS=0: as before.
extern "C" PPC_FUNC(__imp__sub_825642A0);
PPC_FUNC(sub_825642A0)
{
    static const bool off = []() { const char* t = getenv("COD3_COOP_TRIGGERS"); return t && t[0] == '0'; }();
    if (IsHost() && !off)
    {
        constexpr uint32_t Queue = 0x82A4E790u;   // (0.5.899: handles by the table the rest of this code reads, EntityTable; 0.5.897 read one 0x200 before it and knew nobody)
        const uint32_t me = Me(base), second = SecondPlayer();
        const int32_t count = int32_t(Guest::Read32(base, Queue + 7292));
        const int32_t mine = me ? IndexOf(base, me) : -1;
        if (me && second && mine >= 0 && count > 0)
        {
            const uint32_t myHandle = RefOf(base, uint32_t(mine));
            static uint64_t given = 0; static double toldAt = 0; static std::map<uint32_t, uint32_t> by;
            for (int32_t i = 0; i < count && i < 256; i++)
            {
                const uint32_t at = Queue + 3196 + 16 * uint32_t(i);
                const uint32_t actH = Guest::Read32(base, at + 4);
                if (FromRef(base, actH) != second) continue;
                const uint32_t trigH = Guest::Read32(base, at), tidx = trigH & 0x3FFFu;
                const uint32_t trig = FromRef(base, trigH);
                Guest::Write32(base, at + 4, myHandle);
                Guest::Write32(base, at + 12, Guest::Read32(base, me + 204));
                given++;
                if (trig && Valid(trig) && by[tidx]++ == 0 && by.size() < 80)
                {
                    printf("coop: host: the joining player touched %s (entity %u, at %.0f %.0f %.0f): told to the scripts as the host's player's\n", BrocText(base, trig + 564).c_str(), tidx, F(base, trig + 336), F(base, trig + 340), F(base, trig + 344));
                    fflush(stdout);
                }
            }
            if (given && Now() - toldAt > 30) { toldAt = Now(); printf("coop: host: %llu touches of the joining player's given to the scripts as the host's so far (%zu triggers)\n", (unsigned long long)given, by.size()); fflush(stdout); }
        }
    }
    __imp__sub_825642A0(ctx, base);
}

// (0.5.909) THE JOINING PLAYER'S USE. t908a: the player's Use (sub_82551DB8: the thing he can use, its prompt, the use
// itself) ran 14000 times for the host's player and NEVER for the joining player. It is part of a player's end of
// frame, sub_8256CFB0(entity, time), which G_RunFrame (sub_8256D3E0) runs for clients 0 .. level +148 only: one in
// the campaign. A client that is not this machine's own gets the focus (depth of field) block [0x82A2AB60] = 0 there,
// so that part is skipped for him. Now, after the host's player's end of frame, the joining player's Use is run the
// same way: his prompts, and his Use on doors, charges, guns and the scripts' use triggers, are the game's own.
// COD3_COOP_USE=0: as before.
// (0.5.956) WHERE HIS MUZZLE FLASH IS. The user (t955u): with both players holding the sniper rifle, each shot of the
// joining player flashed the HOST's gun; with his Garand and the host's sniper, nothing. sub_8242E5F0(out, entity) puts
// a shot's flash at the tag_flash of: the client's FIRST-PERSON model of the shooter's weapon (0x82A9A460 + 196 *
// entity byte 3) when the shooter is [0x82A2AB40][local index + 1]; else the shooter's own model (+508); else his
// origin. His fire runs with that slot his (0.5.950), so his flash went on the host's first-person model of his gun,
// seen only when the host held the same one. Now for him: the host's player in that slot, and the flash at the gun of
// the body drawn for him (his entity has no model). COD3_COOP_FLASHFIX=0: as before.
extern "C" PPC_FUNC(__imp__sub_8242E5F0);
PPC_FUNC(sub_8242E5F0)
{
    static const bool off = []() { const char* t = getenv("COD3_COOP_FLASHFIX"); return t && t[0] == '0'; }();
    const uint32_t e = ctx.r4.u32;
    const uint32_t second = (!off && IsHost() && e) ? SecondPlayer() : 0;
    if (!second || e != second) { __imp__sub_8242E5F0(ctx, base); return; }
    const uint32_t clients = Guest::Read32(base, 0x82A2AB40u);
    const uint32_t slot1 = Valid(clients) && Guest::Read32(base, 0x82A2A1C8u) == 0 ? clients + 4 : 0;
    const uint32_t inSlot = slot1 ? Guest::Read32(base, slot1) : 0;
    const uint32_t hostEnt = Me(base);
    if (slot1 && inSlot == second && hostEnt) Guest::Write32(base, slot1, hostEnt);
    const uint32_t body = SecondBody();
    const bool useBody = body && Valid(body) && Guest::Read32(base, body + 508) != 0;
    if (useBody) ctx.r4.u64 = body;
    __imp__sub_8242E5F0(ctx, base);
    if (slot1 && inSlot == second && hostEnt) Guest::Write32(base, slot1, inSlot);
    static int told = 0;
    if (told < 5) { told++; printf("coop: host: the joining player's muzzle flash put at %s (not the host's first-person gun)\n", useBody ? "his body's gun" : "his place"); fflush(stdout); }
}

// (0.5.956) WHERE HIS SHOT STARTS. The user (t955u): his shots still flashed on the host's gun, "its specfically the
// sniper". sub_8254AAC8 (a shot's start, from the fire's sub_8254D930) takes, when the shooter's client +208 has bit
// 0x00100000 (the scope), the LOCAL player's camera (0x82A08D90 + 5840 * [local index] + 4480..4488) and not the
// shooter's eye: his scoped shot started at the host's camera. For him that bit is off for this call alone, so the
// game takes his own eye (origin + view height). COD3_COOP_SCOPEFIX=0: as before.
extern "C" PPC_FUNC(__imp__sub_8254AAC8);
PPC_FUNC(sub_8254AAC8)
{
    static const bool off = []() { const char* t = getenv("COD3_COOP_SCOPEFIX"); return t && t[0] == '0'; }();
    const uint32_t e = ctx.r3.u32;
    const uint32_t second = (!off && IsHost() && e) ? SecondPlayer() : 0;
    const uint32_t client = (second && e == second) ? Guest::Read32(base, e + 528) : 0;
    const uint32_t flags = Valid(client) ? Guest::Read32(base, client + 208) : 0;
    if (!Valid(client) || !(flags & 0x00100000u)) { __imp__sub_8254AAC8(ctx, base); return; }
    Guest::Write32(base, client + 208, flags & ~0x00100000u);
    __imp__sub_8254AAC8(ctx, base);
    Guest::Write32(base, client + 208, flags);
    static int told = 0;
    if (told < 5) { told++; printf("coop: host: the joining player's scoped shot starts at his own eye (not the host's camera)\n"); fflush(stdout); }
}

// (0.5.950) THE JOINING PLAYER'S FIRE, AND ONLY IT, AS THE LOCAL CLIENT'S. sub_82569C40 (a shot, a throw: sub_82558E58
// -> sub_82556688) does nothing unless its player is [0x82A2AB40][local index + 1]. 0.5.915 made that slot his for his
// whole ClientThink; the user then saw the HOST's first-person gun flash at each of his shots. The slot is his now for
// this call alone (COD3_COOP_THINKLOCAL=1: the whole think again, COD3_COOP_C1LOCAL=0: never).
extern "C" PPC_FUNC(__imp__sub_82569C40);
PPC_FUNC(sub_82569C40)
{
    static const bool wholeThink = []() { const char* w = getenv("COD3_COOP_THINKLOCAL"); return w && w[0] == '1'; }();
    static const bool never = []() { const char* t = getenv("COD3_COOP_C1LOCAL"); return t && t[0] == '0'; }();
    const uint32_t e = ctx.r3.u32;
    const uint32_t second = (!wholeThink && !never && IsHost() && e) ? SecondPlayer() : 0;
    if (!second || e != second) { __imp__sub_82569C40(ctx, base); return; }
    const uint32_t clients = Guest::Read32(base, 0x82A2AB40u);
    const uint32_t slot1 = Valid(clients) && Guest::Read32(base, 0x82A2A1C8u) == 0 ? clients + 4 : 0;
    const uint32_t hostInSlot = slot1 ? Guest::Read32(base, slot1) : 0;
    if (slot1) Guest::Write32(base, slot1, second);
    __imp__sub_82569C40(ctx, base);
    if (slot1) Guest::Write32(base, slot1, hostInSlot);
    static int told = 0;
    if (told < 3) { told++; printf("coop: host: the joining player's fire run as the local client's (that call only)\n"); fflush(stdout); }
}

// (0.6.1) set while the joining player's Use and hints run (below): what they start is his doing
static bool g_joinerUsePass = false;
extern "C" PPC_FUNC(__imp__sub_8256CFB0);
PPC_FUNC(sub_8256CFB0)
{
    const uint32_t e = ctx.r3.u32;
    __imp__sub_8256CFB0(ctx, base);
    static const bool off = []() { const char* t = getenv("COD3_COOP_USE"); return t && t[0] == '0'; }();
    if (off || !IsHost() || !e || e != Me(base)) return;
    const uint32_t second = SecondPlayer();
    if (!second || !Record(base, second) || int32_t(Guest::Read32(base, second + 776)) <= 0 || !Guest::Read32(base, second + 536)) return;
    {
        static bool told = false;
        if (!told) { told = true; printf("coop: host: the joining player's Use is run each frame (the end of frame's texts here: '%s' '%s' '%s')\n", Text(base, 0x82065B78u, 24).c_str(), Text(base, 0x82065B7Cu, 24).c_str(), Text(base, 0x82065B94u, 24).c_str()); fflush(stdout); }
    }
    const uint32_t focus = Guest::Read32(base, 0x82A2AB60u);
    Guest::Write32(base, 0x82A2AB60u, 0);
    // (0.5.915) the local client's slot is his for these calls too (they read the local player's weapon state from it)
    const uint32_t clients = Guest::Read32(base, 0x82A2AB40u);
    const uint32_t slot1 = Valid(clients) && Guest::Read32(base, 0x82A2A1C8u) == 0 ? clients + 4 : 0;
    const uint32_t hostInSlot = slot1 ? Guest::Read32(base, slot1) : 0;
    if (slot1) Guest::Write32(base, slot1, second);
    g_joinerUsePass = true;
    PPCContext c = CallCtx(ctx);
    c.r3.u64 = second;
    CallGuest(c, base, 0x82551DB8u);
    // (0.5.912) and his hints: sub_82553BB0 (the end of frame's next part) finds what he looks at that has a hint
    // ("Press and hold to pick up the M1 Garand": a trigger_use with sethintstring; record +1156/+1160/+1164 the
    // hint, +1216 the thing) and uses it. t911b: the host's player took the Garand that way (the scripts' giveweapon
    // after the trigger), with no thing to use at +1556. It writes one byte of the local HUD (0x82A2A0C9): kept.
    {
        const uint8_t hudByte = base[0x82A2A0C9u];
        PPCContext h = CallCtx(ctx);
        h.r3.u64 = second;
        CallGuest(h, base, 0x82553BB0u);
        base[0x82A2A0C9u] = hudByte;
    }
    g_joinerUsePass = false;
    if (slot1) Guest::Write32(base, slot1, hostInSlot);
    Guest::Write32(base, 0x82A2AB60u, focus);
    {
        // both players' hint fields when they change (to send the joining player's to his copy next)
        static uint32_t was[2][4] = {}; static int toldHint = 0;
        for (int who = 0; who < 2; who++)
        {
            const uint32_t r = Record(base, who ? second : e);
            if (!r) continue;
            const uint32_t v[4] = { Guest::Read32(base, r + 1156), Guest::Read32(base, r + 1160), Guest::Read32(base, r + 1164), Guest::Read32(base, r + 1216) };
            if (memcmp(v, was[who], sizeof v) != 0 && toldHint < 120)
            {
                toldHint++; memcpy(was[who], v, sizeof v);
                const uint32_t t = FromRef(base, v[3]);
                printf("coop: host: %s's hint: +1156 %08X +1160 %08X +1164 %08X, thing %08X (%s)\n", who ? "the joining player" : "the host's player", v[0], v[1], v[2], v[3], t ? BrocText(base, t + 564).c_str() : "-");
                fflush(stdout);
            }
        }
    }
    static uint32_t hintWas = 0xFFFFFFFFu; static int toldH = 0;
    const uint32_t hint = Guest::Read32(base, Guest::Read32(base, second + 536) + 132) >> 16;
    if (hint != hintWas && toldH < 60) { toldH++; hintWas = hint; printf("coop: host: the joining player's prompt here: %u\n", hint); fflush(stdout); }
}
// (0.5.910) THE SCRIPTS' ISTOUCHING (built-in 0x5CC, sub_824AD7C8(handle A, handle B) -> r3 0/1). The level's scripts
// ask "is the player in this place" of the host's player only (compiled code: level.player). With two players the
// answer is yes when EITHER is: asked of the host's player and no, it is asked again of the joining player.
// COD3_COOP_TOUCHING=0: as before.
extern "C" PPC_FUNC(__imp__sub_824AD7C8);
PPC_FUNC(sub_824AD7C8)
{
    const uint32_t a = ctx.r3.u32, b = ctx.r4.u32;
    PPCContext keep = ctx;
    __imp__sub_824AD7C8(ctx, base);
    static const bool off = []() { const char* t = getenv("COD3_COOP_TOUCHING"); return t && t[0] == '0'; }();
    if (off || !IsHost() || ctx.r3.u32 != 0) return;
    const uint32_t me = Me(base), second = SecondPlayer();
    if (!me || !second || int32_t(Guest::Read32(base, second + 776)) <= 0) return;
    const uint32_t myH = Guest::Read32(base, me + 500), hisH = Guest::Read32(base, second + 500);   // (an entity's own handle)
    if (!myH || !hisH) return;
    if (a != myH && b != myH) return;
    PPCContext c = keep;
    c.r3.u64 = a == myH ? hisH : a; c.r4.u64 = b == myH ? hisH : b;
    __imp__sub_824AD7C8(c, base);
    if (c.r3.u32 == 0) return;
    ctx.r3.u64 = 1;
    static int told = 0; static std::map<uint32_t, int> by;
    const uint32_t other = FromRef(base, a == myH ? b : a);
    const uint32_t key = (a == myH ? b : a) & 0x3FFFu;
    if (by[key]++ == 0 && told++ < 60) { printf("coop: host: the scripts asked whether the host's player touches %s (entity %u): no, but the joining player does: yes\n", other ? BrocText(base, other + 564).c_str() : "?", key); fflush(stdout); }
}

// SV_SetConfigstring(index, text): the host's, given to the joining copy too
PPC_FUNC(sub_82513C98)
{
    if (!IsHost()) { __imp__sub_82513C98(ctx, base); return; }
    const uint32_t index = ctx.r3.u32;
    const std::string text = ctx.r4.u32 ? Text(base, ctx.r4.u32, 1100) : std::string();
    __imp__sub_82513C98(ctx, base);
    if (index < 1024 && PeerConnected())
    {
        if (g_csSent.size() < 1024) g_csSent.resize(1024);
        if (g_csSent[index] != text) SendConfigstring(uint16_t(index), text);
    }
}

// a reliable command for a client: the host's player's (client 0's) are the
// joining copy's too, but for the config strings (sent above)
static const auto g_soloT0 = std::chrono::steady_clock::now();   // (0.5.976) about the process's start: the pad script's clock
PPC_FUNC(sub_82512970)
{
    // (0.5.976) a solo test: the objectives logged (no friend to send them to)
    static const bool solo = getenv("COD3_COOP_SOLO") != nullptr;
    if (solo && IsHost() && !PeerConnected() && ctx.r5.u32)
    {
        const std::string text = Text(base, ctx.r5.u32, 1100);
        static int told = 0;
        if (text.compare(0, 3, "cs ") != 0 && text.compare(0, 3, "bcs") != 0 && !text.empty() && told++ < 300)
        {
            const uint32_t me = Me(base);
            printf("coop: solo: server command (%.0f s) at %.0f %.0f %.0f: %s\n", std::chrono::duration<double>(std::chrono::steady_clock::now() - g_soloT0).count(), me ? F(base, me + 336) : 0.0f, me ? F(base, me + 340) : 0.0f, me ? F(base, me + 344) : 0.0f, text.substr(0, 120).c_str());
            fflush(stdout);
        }
    }
    if (IsHost() && PeerConnected() && ctx.r3.u32 == Guest::Read32(base, 0x829C39E0 + 8) && ctx.r5.u32)
    {
        const std::string text = Text(base, ctx.r5.u32, 1100);
        if (text.compare(0, 3, "cs ") != 0 && text.compare(0, 3, "bcs") != 0 && !text.empty())
        {
            SendReliable(R_SERVERCMD, text.data(), text.size());
            static int told = 0;
            if (told++ < 2000) { printf("coop: host: server command to the joining copy too: %s\n", text.substr(0, 120).c_str()); fflush(stdout); }
        }
    }
    __imp__sub_82512970(ctx, base);
}

// (0.5.960) the scene probe (COD3_COOP_SCENEPROBE) is replaced by the action probe in coop_actionprobe.cpp (0.5.972).

// (0.5.986) DIAGNOSTIC: sub_82560B88 (the record r3: its +128 entity, that entity's record's +1556 ...) asked by the melee
// (sub_824936E8, call returning to 0x82493718): a yes there means no melee. t985a: the co-op host's +128 is entity 1,
// the joining player (alone it is a thing with no record), and his melee never begins.
extern "C" PPC_FUNC(__imp__sub_82560B88);
PPC_FUNC(sub_82560B88)
{
    const uint32_t ps = ctx.r3.u32, from = uint32_t(ctx.lr);
    __imp__sub_82560B88(ctx, base);
    if (from == 0x82493718u)
    {
        static int told = 0; static uint32_t lastAnswer = 0xFFFFFFFFu;
        const uint32_t answer = ctx.r3.u32 & 0xFF;
        if (answer != lastAnswer && told < 40)
        {
            told++; lastAnswer = answer;
            const uint32_t h = Guest::Read32(base, ps + 128), e = EntityAt(base, h & 0x3FFF), er = e ? Guest::Read32(base, e + 528) : 0;
            const uint32_t use = Valid(er) ? Guest::Read32(base, er + 1556) : 0;
            printf("coop: melee asked sub_82560B88 for record %08X: answer %u (its +128 %08X: entity %u %08X, record %08X, that record's +1556 %08X %s)\n",
                ps, answer, h, h & 0x3FFF, e, er, use, e && e == SecondPlayer() ? "= THE JOINING PLAYER" : "");
            fflush(stdout);
        }
    }
}

// (0.6.0) A PROBE: who a scripted action (PickLiveGrenade ...) is started for. sub_82550400(r3 entity) takes the
// action's state from the table 0x82A59DA0 by THIS machine's local client number [0x82A2A1C8], not by the entity:
// t999a, the joining player's pick-up (r5 = his entity in sub_824F18C0) played on the host's player. Logged: the
// entity, the local number, and the table's first four entries (is there a state for a second local client?).
extern "C" PPC_FUNC(__imp__sub_82550400);
PPC_FUNC(sub_82550400)
{
    const uint32_t e = ctx.r3.u32;
    // (0.6.1) an action started by the joining player's Use (a live grenade's pick-up) would play on the HOST'S
    // player (one action state, this machine's: t999a, the host picked the grenade up and threw it by himself). Held
    // back until client 1 has a state of his own. COD3_COOP_JOINERACTIONS=1: let through as before.
    static const bool letThrough = []() { const char* t = getenv("COD3_COOP_JOINERACTIONS"); return t && t[0] == '1'; }();
    // (0.6.2) his doing = his Use pass, or (t1001a: the pick-up starts from 82546050, outside that pass) the local
    // client's slot [0x82A2AB40]+4 holding him (his fire/throw think runs with it swapped)
    const uint32_t clientsNow = Guest::Read32(base, 0x82A2AB40u);
    const bool hisSlot = IsHost() && Valid(clientsNow) && SecondPlayer() && Guest::Read32(base, clientsNow + 4) == SecondPlayer();
    // (0.6.4) t1003a: the guess [[0x82A2AB6C]+40] was wrong (a time). sub_82550400 is the live grenade's USE
    // callback, called by the game's "use" sub_82545F68(r3 activator, r4 entity) as callback(entity, activator,
    // activator): r4 here is who picked it up
    const bool hisOwn = IsHost() && SecondPlayer() && ctx.r4.u32 == SecondPlayer();
    if ((g_joinerUsePass || hisSlot || hisOwn) && IsHost() && !letThrough)
    {
        static int toldBack = 0;
        if (toldBack++ < 20) { printf("coop: host: an action the joining player's Use started (entity %08X, lr %08X) held back: it would play on the host's player\n", e, uint32_t(ctx.lr)); fflush(stdout); }
        return;
    }
    static int told = 0;
    if (IsHost() && told < 30)
    {
        told++;
        const uint32_t second = SecondPlayer();
        printf("coop: host: an action started for entity %08X by %08X%s (lr %08X): local number %u, states %08X %08X %08X %08X\n", e, ctx.r4.u32,
            ctx.r4.u32 && ctx.r4.u32 == second ? " = THE JOINING PLAYER" : (ctx.r4.u32 && ctx.r4.u32 == Me(base) ? " = the host's player" : ""), uint32_t(ctx.lr),
            Guest::Read32(base, 0x82A2A1C8u), Guest::Read32(base, 0x82A59DA0u), Guest::Read32(base, 0x82A59DA4u), Guest::Read32(base, 0x82A59DA8u), Guest::Read32(base, 0x82A59DACu));
        fflush(stdout);
    }
    __imp__sub_82550400(ctx, base);
}

#endif
