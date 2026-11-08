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
        float g_trail[24][3] = {}; int g_trailCount = 0, g_trailNext = 0; double g_trailAt = -10;
        void Trail(const uint8_t* base)
        {
            const uint32_t me = Me(base);
            if (!me || Now() - g_trailAt < 0.5) return;
            g_trailAt = Now();
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
            std::vector<uint8_t> anim;
            double fullAt = -10;
        };
        std::unordered_map<uint16_t, SentState> g_sent;
        std::map<uint16_t, double> g_removed;   // removals told again for a second
        uint32_t g_snapId = 0;
        double g_snapAt = -1;
        uint16_t F16(float v, float scale) { return uint16_t(std::clamp(int32_t(lrintf(v * scale)), 0, 65535)); }
        uint32_t g_hostBody = 0, g_secondBody = 0, g_bodyGen = 0;   // the bodies drawn for the two players
        bool Replicated(const uint8_t* base, uint32_t e, uint32_t i)
        {
            const uint8_t type = base[e];
            if (type == 1 || type == 12) return false;           // players (bodies stand for them), spawners
            if (e == g_ent) return false;
            // (the joining player's own body: his copy is first person, his body there was drawn round his eye)
            if (e && e == g_secondBody) return false;
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
                uint8_t extra = 0;
                if (e == hostBody) flags &= ~0x400u;          // hidden here, drawn there
                if (e == hostBody)
                {
                    // (v43g) the host's stance for his body there: 4 crouched, 8 prone (the joining copy animates it itself)
                    const uint32_t hr = Record(base, Me(base));
                    if (hr) { if (int32_t(Guest::Read32(base, hr + 184)) <= 20) extra |= 8; else if (Guest::Read32(base, hr + 208) & 0x20) extra |= 4; }
                }
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
        struct BodyState { int32_t anim = -1; float last[3] = {}; double at = -1; float speed = 0; int move = -1; double mirrorAt = -10; };
        BodyState g_bodyState[2];
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
        uint32_t MakeBody(PPCContext& ctx, uint8_t* base, uint32_t from, const float at[3])
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
                c.r3.u64 = e; c.r4.u64 = GuestText(base, ResName(base, m)); c.r5.u64 = GuestText(base, BrocText(base, from + 932 + 8 * k));
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
        void DriveBody(PPCContext& ctx, uint8_t* base, int who, uint32_t body, uint32_t player, const float view[3], uint32_t rec)
        {
            if (!body || !player || IndexOf(base, body) < 0) return;
            BodyState& st = g_bodyState[who];
            const double now = Now();
            const float o[3] = { F(base, player + 336), F(base, player + 340), F(base, player + 344) };
            if (st.at >= 0 && now > st.at)
            {
                const float dt = float(now - st.at);
                const float v = sqrtf((o[0] - st.last[0]) * (o[0] - st.last[0]) + (o[1] - st.last[1]) * (o[1] - st.last[1])) / std::max(dt, 0.001f);
                st.speed += (std::min(v, 600.0f) - st.speed) * std::min(1.0f, dt * 8.0f);
            }
            const float vx = o[0] - st.last[0], vy = o[1] - st.last[1];
            memcpy(st.last, o, 12); st.at = now;
            const uint32_t flags = rec ? Guest::Read32(base, rec + 208) : 0;
            const int32_t height = rec ? int32_t(Guest::Read32(base, rec + 184)) : 60;
            const bool prone = height <= 20, crouch = !prone && (flags & 0x20);
            const float yaw = view[1] * 0.0174533f;
            const float fwd = vx * cosf(yaw) + vy * sinf(yaw), side = -vx * sinf(yaw) + vy * cosf(yaw);
            int move;
            if (st.speed < 20.0f) move = prone ? PRONE_IDLE : crouch ? CROUCH_IDLE : STAND_IDLE;
            else if (prone) move = PRONE_CRAWL;
            else
            {
                const bool f = fabsf(fwd) >= fabsf(side);
                if (crouch) move = f ? (fwd >= 0 ? CROUCH_WALK_F : CROUCH_WALK_B) : (side > 0 ? CROUCH_WALK_L : CROUCH_WALK_R);
                else if (f && fwd >= 0) move = st.speed > 170.0f ? STAND_RUN_F : STAND_WALK_F;
                else move = f ? STAND_WALK_B : (side > 0 ? STAND_WALK_L : STAND_WALK_R);
            }
            // placed and turned (and never solid: a body drawn only)
            if (Guest::Read32(base, body + 324)) { static int told = 0; if (told++ < 4) { printf("coop: host: a body was solid (%08X): no longer\n", Guest::Read32(base, body + 324)); fflush(stdout); } Guest::Write32(base, body + 324, 0); }
            // the host's own body is the joining copy's to see: here it is kept out of sight, below the world
            // (drawn here it filled the host's view with the inside of its model); the snapshots say where it is
            for (const uint32_t at : { 336u, 32u }) for (int k = 0; k < 3; k++) SetF(base, body + at + 4 * k, o[k] - (who == 0 && k == 2 ? 6000.0f : 0.0f));
            for (const uint32_t at : { 352u, 72u }) { SetF(base, body + at, 0.0f); SetF(base, body + at + 4, view[1]); SetF(base, body + at + 8, 0.0f); }
            LinkEntity(ctx, base, body);
            const uint32_t tree = Guest::Read32(base, body + 552);
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
                    if (F(base, r + 16) >= 0.97f) { SetF(base, r + 16, 0.0f); SetF(base, r + 20, 0.0f); }
                    if (F(base, r + 32) < 0.99f) { SetF(base, r + 32, 1.0f); }
                    return;
                }
            }
            if (move == st.move) return;
            int32_t anim = AnimIndex(base, list, MoveHash[move]);
            if (anim < 0) anim = AnimIndex(base, list, MoveHash[STAND_IDLE]);
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
            if (!AnimJobRoom(base)) return;   // (next frame)
            // the new one in over a fifth of a second, the old one out
            PPCContext c = CallCtx(ctx);
            c.r3.u64 = tree; c.r4.u64 = uint32_t(anim); c.f1.f64 = 1.0; c.f2.f64 = 0.2; c.f3.f64 = move == STAND_RUN_F ? 1.15 : 1.0; c.r8.u64 = 0; c.r10.u64 = 0;
            CallGuest(c, base, 0x82500A40);   // set anim
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
            if (g_bodyGen != MissionCount()) { g_bodyGen = MissionCount(); g_hostBody = g_secondBody = g_template = 0; g_bodyState[0] = g_bodyState[1] = BodyState{}; }
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
            // (v46) the joining player's body only while his copy is here (its commands coming): loading again or
            // gone, his body stood where it was left, in its bind pose (the host saw him T-posed)
            if (g_secondBody && IndexOf(base, g_secondBody) >= 0)
            {
                const bool here = second && g_cmdAt >= 0 && Now() - g_cmdAt < 1.5;
                const uint32_t f = Guest::Read32(base, g_secondBody + 632);
                const uint32_t want = here ? (f & ~0x400u) : (f | 0x400u);
                if (want != f) { Guest::Write32(base, g_secondBody + 632, want); static int told = 0; if (told++ < 20) { printf("coop: host: the joining player's body %s\n", here ? "shown (his copy is here)" : "hidden (his copy is away or loading)"); fflush(stdout); } }
            }
        }
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

    void HostNewPeer() { g_cmdAt = -1; g_cmd = CmdIn{}; g_csResend = true; }
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
            if (b != lastButtons && told < 20) { told++; printf("coop: host: the joining player's buttons %08X (his record's command time %08X, the host's %08X)\n", Guest::Read32(base, slot + 4), Guest::Read32(base, g_rec + 32), Guest::Read32(base, slot)); fflush(stdout); }
            lastButtons = b;
        }
        for (int k = 0; k < 3; k++) SetF(base, g_rec + 172 + 4 * k, g_cmd.view[k]);
        CallGuest(c, base, 0x82497450);   // VM_Call(game, 6: ClientThink, his entity number)
        memcpy(base + slot, hostCmd, 48);
        PlaceSecond(base);
        static int told = 0;
        if (told++ % 600 == 0) { printf("coop: host: client 1 thought (%llu commands in): at %.0f %.0f %.0f, health %d\n", (unsigned long long)g_cmds, F(base, g_ent + 336), F(base, g_ent + 340), F(base, g_ent + 344), int32_t(Guest::Read32(base, g_ent + 776))); fflush(stdout); }
    }

    void HostFrame(PPCContext& ctx, uint8_t* base)
    {
        const double now = Now();
        if (!InMission(base) || !PeerConnected()) { g_inSince = -1; return; }
        if (g_inSince < 0) g_inSince = now;
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
        PlaceSecond(base);
        Bodies(ctx, base);
        {
            static uint32_t scenesWas = 0;
            const uint32_t scenes = Guest::Read32(base, 0x825F51A0);
            if (scenes != scenesWas) { printf("coop: host: %u scripted scene(s) playing (was %u)\n", scenes, scenesWas); fflush(stdout); scenesWas = scenes; }
        }
        Snapshot(base);
        HudSync(base);
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
            struct { int32_t i[8]; float pos[3]; float yaw; } ps{ { int32_t(Guest::Read32(base, g_ent + 776)), int32_t(Guest::Read32(base, g_ent + 780)), int32_t(g_index), (scenes || ScriptDvarIs("letterbox_enabled", "1") || ScriptDvarIs("introScreen", "1") ? 1 : 0) | (me && Guest::Read32(base, me + 916) ? 2 : 0),
                              int32_t(me ? Guest::Read32(base, Record(base, me) + 44) & 0x01F00000u : 0),
                              me ? int32_t(Guest::Read32(base, me + 776)) : 0, me ? int32_t(Guest::Read32(base, me + 780)) : 0,
                              g_hostBody ? IndexOf(base, g_hostBody) : -1 }, {} };
            if (me) { for (int k = 0; k < 3; k++) ps.pos[k] = F(base, me + 336 + 4 * k); ps.yaw = F(base, Record(base, me) + 176); }
            SendUnreliable(K_PS, &ps, sizeof ps);
        }
    }
}

using namespace Coop;

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
PPC_FUNC(sub_82512970)
{
    if (IsHost() && PeerConnected() && ctx.r3.u32 == Guest::Read32(base, 0x829C39E0 + 8) && ctx.r5.u32)
    {
        const std::string text = Text(base, ctx.r5.u32, 1100);
        if (text.compare(0, 3, "cs ") != 0 && text.compare(0, 3, "bcs") != 0 && !text.empty())
        {
            SendReliable(R_SERVERCMD, text.data(), text.size());
            static int told = 0;
            if (told++ < 200) { printf("coop: host: server command to the joining copy too: %s\n", text.substr(0, 120).c_str()); fflush(stdout); }
        }
    }
    __imp__sub_82512970(ctx, base);
}

#endif
