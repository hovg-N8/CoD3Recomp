// Co-op v40, the joining copy's side (see coop.h, docs/COOP-V40-DESIGN.md):
// its own world stops once the level is in (its script threads ended each
// frame, the level's function table's dispatcher made a no-op, its soldiers
// and drones deleted), and it draws the host's world instead: each entity of
// his snapshots is one of this copy's own (the same map entity, adopted) or a
// puppet (a script_model given the host's models, attachments and animation
// tree), moved between snapshots 100 ms in the past and animated with what the
// host's plays. The joining player himself moves by his own game; his
// command, view and place go to the host each frame (K_CMD).
#ifndef COD3_MP

#include "coop.h"
#include "overlay.h"
#include "input.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <deque>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include <thread>
#include <chrono>

extern "C" PPC_FUNC(__imp__sub_82513628);

bool CoopCommitted(const void* p, size_t n);   // coop_memscan.cpp (0.5.987)

namespace Coop
{
    namespace
    {
        constexpr uint32_t LocalClients = 0x82A57900;
        uint32_t CommandSlot(const uint8_t* base) { return LocalClients + 5752 + (Guest::Read32(base, LocalClients + 5848) & 1) * 48; }
        enum : uint8_t { M_ORIGIN = 1, M_ANGLES = 2, M_MODELS = 4, M_ATTACH = 8, M_ANIM = 16, M_FLAGS = 32, M_REMOVED = 64, M_FULL = 128 };

        std::unordered_map<uint16_t, std::string> g_names;   // the host's names by id
        struct Sample { double t; float o[3]; float a[3]; };
        struct Node { uint16_t anim; float goal, weight, time, rate, blend; };
        struct Remote
        {
            uint16_t gen = 0; uint8_t type = 0;
            uint16_t models[4] = {}, cls = 0;
            std::vector<uint16_t> attach;
            uint32_t flags = 0; uint8_t extra = 0;
            uint16_t list = 0; std::vector<Node> nodes; bool animNew = false, held = false;
            std::deque<Sample> track;
            bool haveModels = false, haveAttach = false, haveFlags = false, haveAnim = false, havePlace = false;
            bool modelsNew = false, attachNew = false, flagsNew = false;
            double heard = 0;
            uint32_t local = 0;          // our entity for it
            int32_t localIndex = -1;
            uint32_t localGen = 0;       // (v43i) the generation of its slot when bound: a slot used again is not ours
            bool puppet = false;         // made here (else one of ours, adopted)
            std::vector<std::string> shownModels, shownAttach;
        };
        std::unordered_map<uint16_t, Remote> g_remote;
        std::set<uint32_t> g_puppets;     // our entities made for the host's
        bool g_frozen = false;
        std::unordered_map<uint32_t, uint32_t> g_threadFlags;
        std::set<uint32_t> g_oldThreads;   // (v46) the stopped world's threads, at a reload in this process   // (v46) each script thread's flags before we marked it ended
        uint32_t g_levelFnSaved = 0;          // (v46) the level's functions-by-name entry, before the freeze stubbed it
        bool g_vmHold = false;                // (v46) the VM kept stopped through a reload in this process, till the new level is up
        double g_reloadInGameAt = -1;         // (v46) when the reload in this process was asked for
        double g_reloadAskedAt = -1; int g_reloadTries = 0;   // (v46) the spmap asked, and how many times
        volatile uint32_t g_joinFrames = 0;   // (v46) frames of ours, for the reload's watchdog
        bool g_reloading = false;   // (v45) loading again for the host's checkpoint: no intro of ours waited for
        double g_inSince = -1, g_frozenAt = -1, g_firstSnapAt = -1;
        double g_lastSnapAt = -1;   // (0.5.893) when the host's last snapshot came
        uint32_t g_seq = 0;
        int32_t g_health = -1, g_maxHealth = -1;
        uint32_t g_hostFlags = 0, g_hostAllow = 0;
        int32_t g_hostHealth = 0, g_hostMaxHealth = 0, g_hostBodyIndex = -1;   // the host's player (K_PS)
        float g_hostPos[3] = {};
        bool g_catchUpHost = false;   // (0.5.964) put my player where the host stands, once (riding one of our vehicles at the freeze)
        float g_stable[3] = {}; double g_stableAt = -1;   // (v45u) where my player last stood still on firm ground
        float g_hostYaw = 0;
        uint32_t g_hostHint[3] = {}; double g_hostHintAt = -1;   // (0.5.919) my prompt as the host's world has it (record +1156..+1164)
        std::vector<std::tuple<std::string, std::string, uint32_t>> g_dvarPending;   // the host's scripts' dvars, to set here
        bool g_havePs = false;
        double g_psAt = -1;
        std::vector<std::pair<uint16_t, std::string>> g_csPending;
        std::vector<std::string> g_cmdPending;
        uint8_t g_hud[16][124]; bool g_hudHave[16] = {};

        float LerpAngle(float from, float to, float f) { const float d = fmodf(to - from + 540.0f, 360.0f) - 180.0f; return from + d * f; }
        void Place(const Remote& r, double t, float o[3], float a[3])
        {
            const Sample* x = &r.track.front();
            const Sample* y = &r.track.back();
            if (t >= y->t) { memcpy(o, y->o, 12); memcpy(a, y->a, 12); return; }
            if (t <= x->t) { memcpy(o, x->o, 12); memcpy(a, x->a, 12); return; }
            for (size_t i = 0; i + 1 < r.track.size(); i++)
                if (r.track[i + 1].t > t) { x = &r.track[i]; y = &r.track[i + 1]; break; }
            const double span = y->t - x->t;
            const float f = span > 1e-4 ? float((t - x->t) / span) : 1.0f;
            if (span > 0.6) { memcpy(o, y->o, 12); memcpy(a, y->a, 12); return; }   // (a gap: no slide across it)
            for (int k = 0; k < 3; k++) { o[k] = x->o[k] + (y->o[k] - x->o[k]) * f; a[k] = LerpAngle(x->a[k], y->a[k], f); }
        }
        uint32_t AnimList(const uint8_t* base, const std::string& name)
        {
            const uint32_t lists = Guest::Read32(base, 0x82A2A204);
            if (!Valid(lists)) return 0;
            const uint32_t tab = Guest::Read32(base, lists + 4);
            if (!Valid(tab)) return 0;
            const uint32_t n = std::min<uint32_t>(Guest::Read32(base, tab), 256), entries = Guest::Read32(base, tab + 4);
            for (uint32_t k = 0; k < n; k++)
                if (Text(base, Guest::Read32(base, entries + 12 * k)) == name) return entries + 12 * k;
            return 0;
        }
        uint32_t Bank(const uint8_t* base, uint32_t e)
        {
            const int32_t b = int32_t(Guest::Read32(base, e + 496));
            return b != -1 ? uint32_t(b) : Guest::Read32(base, Guest::Read32(base, 0x82A2AB6C) + 40);
        }
        const std::string* Name(uint16_t id) { const auto it = g_names.find(id); return it == g_names.end() ? nullptr : &it->second; }
        void Unbind(PPCContext& ctx, uint8_t* base, Remote& r);
        void SetModels(PPCContext& ctx, uint8_t* base, Remote& r)
        {
            std::vector<std::string> want(4);
            for (int s = 0; s < 4; s++) if (r.models[s]) { const std::string* n = Name(r.models[s]); if (!n) return; want[s] = *n; }
            if (want == r.shownModels) return;
            // (v45h) not the first time either: ours, adopted, came with a tree made for their own models: other
            // models on it had more bones than its poses had room for, and the poses were written over the
            // next entity's memory (its link fields: the world's links walked for ever, Island at 102 s)
            if (Valid(Guest::Read32(base, r.local + 552)))   // (v45e: as for attachments)
            {
                bool same = true;
                for (int s = 0; s < 4; s++) if (!want[s].empty() && ResName(base, ModelSlot(base, r.local, s)) != want[s]) same = false;
                if (same) { r.shownModels = want; return; }
                if (r.puppet) Unbind(ctx, base, r); else r.shownModels = want;
                return;
            }
            for (int s = 0; s < 4; s++)
            {
                if (want[s].empty() || (r.shownModels.size() == 4 && r.shownModels[s] == want[s])) continue;
                if (ResName(base, ModelSlot(base, r.local, s)) == want[s]) continue;
                PPCContext c = CallCtx(ctx);
                c.r3.u64 = r.local; c.r4.u64 = GuestText(base, want[s]); c.r5.u64 = uint64_t(-1); c.r6.u64 = uint64_t(s);
                CallGuest(c, base, 0x8254EE80);   // G_SetModel
            }
            r.shownModels = want;
            PPCContext c = CallCtx(ctx); c.r3.u64 = r.local; CallGuest(c, base, 0x82554D60);
        }
        void SetAttach(PPCContext& ctx, uint8_t* base, Remote& r)
        {
            std::vector<std::string> want;
            for (size_t k = 0; k + 1 < r.attach.size(); k += 2)
            {
                const std::string* m = Name(r.attach[k]); const std::string* t = Name(r.attach[k + 1]);
                if (!m || !t) return;
                want.push_back(*m); want.push_back(*t);
            }
            if (want == r.shownAttach) return;
            // (v45e) once a thing animates, its skeleton is sized for the models it had then: models attached
            // later wrote their bones past it (the crash in sub_8216E680 > sub_824F38E8). A puppet whose host's
            // thing changed its attachments is made again (or taken from the pool, dressed the same); ours,
            // adopted, keep what they have.
            if (Valid(Guest::Read32(base, r.local + 552)))
            {
                // (v45s) kept as it is, with what it carried: a new puppet each time stood in its bind pose till its
                // animations were set again (the T-posed Germans on Island's bridge, who drop and pick up things)
                r.shownAttach = want;
                return;
            }
            // ours as they are
            std::vector<std::string> have;
            for (int k = 0; k < 7; k++) { const uint32_t m = Guest::Read32(base, r.local + 928 + 8 * k); if (Valid(m)) { have.push_back(ResName(base, m)); have.push_back(BrocText(base, r.local + 932 + 8 * k)); } }
            if (have != want)
            {
                for (size_t k = 0; k + 1 < have.size(); k += 2)
                {
                    PPCContext c = CallCtx(ctx);
                    c.r3.u64 = r.local; c.r4.u64 = GuestText(base, have[k]); c.r5.u64 = GuestText(base, have[k + 1]);
                    CallGuest(c, base, 0x825585E8);   // detach
                }
                for (size_t k = 0; k + 1 < want.size(); k += 2)
                {
                    PPCContext c = CallCtx(ctx);
                    c.r3.u64 = r.local; c.r4.u64 = GuestText(base, want[k]); c.r5.u64 = GuestText(base, want[k + 1]);
                    c.r6.u64 = 1; c.r7.u64 = Bank(base, r.local);
                    CallGuest(c, base, 0x825587B0);   // attach's own step (no collision)
                }
            }
            r.shownAttach = want;
        }
        // (v44b) puppets are never freed while the world is frozen: one no longer needed is hidden and kept
        // here, and the next host entity that needs one takes it. Freed and made again (dozens a minute in a
        // fight), they broke the world's area tree and a later link walked it for ever (v43n..v44a)
        struct Pooled { uint32_t e = 0; int32_t index = -1; uint32_t gen = 0; uint32_t list = 0; std::vector<std::string> models, attach; };
        std::vector<Pooled> g_pool;
        std::set<uint32_t> g_sweepHidden;   // (v44r) ours hidden only because the host had no such thing then
        std::set<uint32_t> g_retired;   // (v44p) ours we removed or retired: never adopted again
        void Unbind(PPCContext& ctx, uint8_t* base, Remote& r);
        void SetAnims(PPCContext& ctx, uint8_t* base, Remote& r)
        {
            const std::string* listName = Name(r.list);
            if (!listName) return;
            uint32_t tree = Guest::Read32(base, r.local + 552);
            const uint32_t list = AnimList(base, *listName);
            if (!list) return;
            if (Valid(tree) && Guest::Read32(base, tree + 8) != list && r.puppet)
            {
                // (v43i) the host's entity took another animation list: a new puppet for it (next frame),
                // not a second tree made on this one: the old tree's memory was let go twice that way
                Unbind(ctx, base, r);
                return;
            }
            if (Valid(tree) && Guest::Read32(base, tree + 8) != list) return;   // (v44s) ours, adopted, on another list: left as it is
            if (!Valid(tree))
            {
                // (v45n) no tree made on one of ours (adopted), nor on a brush model (an "sbmodel": no bones to
                // pose): Island's bunker pieces got one at about 100 s, their poses were written over the
                // world's sector tree next to them, and the next link walked it for ever
                const std::string m0 = ResName(base, ModelSlot(base, r.local, 0));
                if (!r.puppet || m0.rfind("sbmodel", 0) == 0 || m0.empty()) { static int told = 0; if (told++ < 10) { printf("coop: join: no animation tree made on %s (%s)\n", r.puppet ? "a puppet" : "one of ours", m0.c_str()); fflush(stdout); } return; }
            }
            if (!Valid(tree) || Guest::Read32(base, tree + 8) != list)
            {
                PPCContext c = CallCtx(ctx); c.r3.u64 = r.local; c.r4.u64 = list;
                CallGuest(c, base, 0x82555B98);   // useanimtree's own step
                tree = Guest::Read32(base, r.local + 552);
                if (!Valid(tree)) return;
            }
            const uint32_t count = std::min<uint32_t>(Guest::Read32(base, list + 4), 4096);
            {
                // (v45u) nothing playing on the host's: a dead soldier lying as his body's physics put him there, not
                // by an animation. Zeroed here he stood up in his bind pose (the T-posed Germans by Island's bridge,
                // dead there): what he played last is kept, at its end and stopped (a death's end: lying down)
                bool any = false;
                for (const Node& n : r.nodes) if (n.anim < count && (n.weight > 0.01f || n.goal > 0.01f)) { any = true; break; }
                if (!any)
                {
                    if (!r.held)
                    {
                        r.held = true;
                        for (uint32_t i = 0; i < count; i++)
                        {
                            const uint16_t slot = Guest::Read16(base, tree + 24 + 2 * i);
                            if (!slot) continue;
                            const uint32_t rec = InfoPool + 52u * slot;
                            const float w = F(base, rec + 36);
                            if (!(w > 0.001f)) continue;
                            SetF(base, rec + 16, 0.99f); SetF(base, rec + 20, 0.99f); SetF(base, rec + 40, 0.0f); SetF(base, rec + 32, w);
                        }
                        static int told = 0; if (told++ < 10) { printf("coop: join: the host's %s has nothing playing (dead, lying as he fell): held as he was\n", r.type == 11 ? "soldier" : "thing"); fflush(stdout); }
                    }
                    return;
                }
                r.held = false;
            }
            std::set<uint16_t> wanted;
            // (v44s) every value the host sent, made safe first: a time outside 0..1 or not a number is a frame
            // index far outside the animation, and the pose was written outside its buffer (a crash after the
            // host's checkpoint, in sub_8216E680)
            auto safe = [](float v, float lo, float hi, float dflt) { return std::isfinite(v) ? std::clamp(v, lo, hi) : dflt; };
            bool deferred = false;
            for (Node n : r.nodes)
            {
                if (n.anim >= count) continue;
                n.time = safe(n.time, 0.0f, 0.999f, 0.0f); n.goal = safe(n.goal, 0.0f, 1.0f, 0.0f); n.weight = safe(n.weight, 0.0f, 1.0f, 0.0f);
                n.rate = safe(n.rate, 0.0f, 8.0f, 1.0f); n.blend = safe(n.blend, 0.0f, 10.0f, 0.2f);
                wanted.insert(n.anim);
                if (!Guest::Read16(base, tree + 24 + 2 * n.anim))
                {
                    if (!AnimJobRoom(base)) { deferred = true; continue; }   // (next frame)
                    PPCContext c = CallCtx(ctx);
                    c.r3.u64 = tree; c.r4.u64 = n.anim; c.f1.f64 = std::max(n.goal, 0.001f); c.f2.f64 = 0.0;
                    CallGuest(c, base, 0x82500B80);   // set anim, the parents alone
                }
                const uint16_t slot = Guest::Read16(base, tree + 24 + 2 * n.anim);
                if (!slot) continue;
                const uint32_t rec = InfoPool + 52u * slot;
                // (the time only when it is a new one or out by more than a tenth: the engine moves it on itself)
                const float t = F(base, rec + 16);
                if (r.animNew || fabsf(t - n.time) > 0.1f) { SetF(base, rec + 16, n.time); SetF(base, rec + 20, n.time); }
                SetF(base, rec + 28, n.blend); SetF(base, rec + 32, n.goal); SetF(base, rec + 40, n.rate);
                // (v45u) the weight always the host's: nothing in a frozen world moves a weight on to its goal, so a
                // move begun after the first (weight 0, goal 1) never showed while the one before was put to 0:
                // the T-posed soldiers and the host's body standing in their bind pose
                SetF(base, rec + 36, n.weight);
            }
            for (uint32_t i = 0; i < count; i++)
            {
                const uint16_t slot = Guest::Read16(base, tree + 24 + 2 * i);
                if (!slot || wanted.count(uint16_t(i))) continue;
                const uint32_t rec = InfoPool + 52u * slot;
                SetF(base, rec + 32, 0.0f); SetF(base, rec + 36, 0.0f);
            }
            r.animNew = deferred;
        }
        // (v43g) the host's body: animated here by how it moves and his stance, as the host animates the
        // joining player's (coop_host.cpp DriveBody), not from the host's tree: that body lives below his
        // world there, where its animations were not kept moving, and here it stood in its bind pose
        struct LocalBody { int32_t anim = -1, prev = -1; double blendAt = -1; int move = -1; float last[3] = {}; double at = -1; float speed = 0; uint32_t tree = 0; float lastT = 0; double bare = 0, bareAt = -1, toldAt = 0; uint32_t restarts = 0, replays = 0; };
        LocalBody g_lb;
        void DriveHostBody(PPCContext& ctx, uint8_t* base, Remote& r)
        {
            enum { STAND_IDLE, STAND_WALK_F, STAND_WALK_B, STAND_WALK_L, STAND_WALK_R, STAND_RUN_F, CROUCH_IDLE, CROUCH_WALK_F, CROUCH_WALK_B, CROUCH_WALK_L, CROUCH_WALK_R, PRONE_IDLE, PRONE_CRAWL, MOVES };
            static const uint32_t MoveHash[MOVES] = {
                0x92A23EE1, 0x444ED990, 0xF4C043BB, 0xF4C5D0F5, 0x055BB472, 0x4DE531AF,
                0x7CDAACCD, 0x6F035656, 0x8EE74CB5, 0x68D9B13C, 0x847EA06F,
                0xE92CB727, 0xE92CB727 };
            uint32_t tree = Guest::Read32(base, r.local + 552);
            if (!Valid(tree))
            {
                // his tree: the list the host gave it, else the one his soldiers use (a soldier puppet's)
                uint16_t listId = r.list;
                if (!listId) for (const auto& [i, other] : g_remote) if (other.type == 11 && other.list) { listId = other.list; break; }
                const std::string* listName = listId ? Name(listId) : nullptr;
                const uint32_t list = listName ? AnimList(base, *listName) : 0;
                static int told = 0;
                if (told++ < 3) { printf("coop: join: the host's body: animation list %s (%s)\n", listName ? listName->c_str() : "none yet", r.list ? "his" : "a soldier's"); fflush(stdout); }
                if (!list) return;
                PPCContext c = CallCtx(ctx); c.r3.u64 = r.local; c.r4.u64 = list;
                CallGuest(c, base, 0x82555B98);   // useanimtree's own step
                tree = Guest::Read32(base, r.local + 552);
                if (!Valid(tree)) return;
            }
            if (tree != g_lb.tree) { g_lb = LocalBody{}; g_lb.tree = tree; }
            // (0.5.880) no parent node left weighing with nothing under it (the T-pose; see TidyBodyTree)
            // (0.5.895: TidyBodyTree is called once, after this, with the aim's up and down)
            const uint32_t list = Guest::Read32(base, tree + 8);
            auto index = [&](uint32_t hash) -> int32_t {
                const uint32_t count = std::min<uint32_t>(Guest::Read32(base, list + 4), 4096), entries = Guest::Read32(base, list + 8);
                for (uint32_t k = 0; k < count; k++) if (Guest::Read32(base, entries + 28 * k) == hash) return int32_t(k);
                return -1;
            };
            const double now = Now();
            // (0.5.877) how long the host's body has had nothing playing here (the T-pose), said every 10 s
            {
                bool bare = g_lb.anim < 0; float t = -1, w = -1, goal = -1; uint16_t slot = 0;
                if (!bare) { slot = Guest::Read16(base, tree + 24 + 2 * uint32_t(g_lb.anim)); if (!slot) bare = true; else { const uint32_t rc = InfoPool + 52u * slot; t = F(base, rc + 16); w = F(base, rc + 36); goal = F(base, rc + 32); if (!(w > 0.0f)) bare = true; } }
                if (bare && g_lb.bareAt >= 0) g_lb.bare += std::min(now - g_lb.bareAt, 1.0);
                g_lb.bareAt = now;
                if (now - g_lb.toldAt > 10.0)
                {
                    const uint32_t count = std::min<uint32_t>(Guest::Read32(base, list + 4), 4096), entries = Guest::Read32(base, list + 8);
                    int leaves = 0, parents = 0, held = 0;
                    for (uint32_t k = 0; k < count; k++) { const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * k); if (!sl) continue; held++; if (!(F(base, InfoPool + 52u * sl + 36) > 0.3f)) continue; if (Guest::Read16(base, entries + 28 * k + 4)) parents++; else leaves++; }
                    printf("coop: join: the host's body's tree: %d nodes held, weighing: %d animations, %d parent nodes\n", held, leaves, parents);
                }
                if (now - g_lb.toldAt > 10.0) { g_lb.toldAt = now; printf("coop: join: the host's body: move %d, animation %d, slot %u, time %.2f, weight %.2f (to %.2f); bare %.1f s so far, %u restarts, %u replays\n", g_lb.move, g_lb.anim, slot, t, w, goal, g_lb.bare, g_lb.restarts, g_lb.replays); fflush(stdout); }
            }
            if (r.track.empty()) return;
            const float* o = r.track.back().o;
            static BodyMotion mo;   // (0.5.891: see coop.h)
            if (g_lb.at < 0) mo = BodyMotion{};
            mo.Feed(now, o[0], o[1]);
            g_lb.speed = mo.speed;
            const float vx = mo.vx, vy = mo.vy;
            memcpy(g_lb.last, o, 12); g_lb.at = now;
            const bool prone = (r.extra & 8) != 0, crouch = !prone && (r.extra & 4);
            const float yaw = r.track.back().a[1] * 0.0174533f;
            const float fwd = vx * cosf(yaw) + vy * sinf(yaw), side = -vx * sinf(yaw) + vy * cosf(yaw);
            int move;
            if (g_lb.speed < 20.0f) move = prone ? PRONE_IDLE : crouch ? CROUCH_IDLE : STAND_IDLE;
            else if (prone) move = PRONE_CRAWL;
            else
            {
                const bool f = mo.ForwardAxis(fwd, side);
                if (crouch) move = f ? (fwd >= 0 ? CROUCH_WALK_F : CROUCH_WALK_B) : (side > 0 ? CROUCH_WALK_L : CROUCH_WALK_R);
                else if (f && fwd >= 0) move = g_lb.speed > 170.0f ? STAND_RUN_F : STAND_WALK_F;
                else move = f ? STAND_WALK_B : (side > 0 ? STAND_WALK_L : STAND_WALK_R);
            }
            move = mo.Settle(now, move);   // (0.5.891)
            { static uint8_t was = 0; static int told = 0; if ((r.extra >> 4) != was && (r.extra >> 4) != 1 && was != 1 && told < 300) { told++; printf("coop: join: the host does %u (was %u) at %.1f s\n", r.extra >> 4, was, Now()); fflush(stdout); } was = r.extra >> 4; }
            { uint32_t act = uint32_t(r.extra) >> 4; if (act == 0 && fabsf(Pitch180(r.track.back().a[0])) > AimPitchMin) act = 1; move = mo.Act(act, BodyAct(move, act)); }   // (0.5.882; 0.5.895: looking up or down, he aims) what the host does: 0x10 firing, 0x20 throwing, 0x40 aiming
            // the move playing kept playing (near its end it starts over, its weight put back)
            // (v45u) the weights moved on here, a fifth of a second to blend: the frozen world does not do it
            {
                const float dt = g_lb.blendAt >= 0 ? float(std::min(now - g_lb.blendAt, 0.25)) : 0.0f;
                g_lb.blendAt = now;
                auto ramp = [&](int32_t a, float to) {
                    if (a < 0) return; const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * uint32_t(a)); if (!sl) return;
                    const uint32_t rc = InfoPool + 52u * sl; float w = F(base, rc + 36);
                    w = to > w ? std::min(to, w + dt * 5.0f) : std::max(to, w - dt * 5.0f);
                    if (!std::isfinite(w)) w = to;
                    SetF(base, rc + 36, w); SetF(base, rc + 32, to);
                };
                // (0.5.890) the blend is TidyBodyTree's now (coop_host.cpp), the same on both copies
                (void)ramp;
            }
            if (move == g_lb.move && g_lb.anim >= 0)
            {
                const uint16_t slot = Guest::Read16(base, tree + 24 + 2 * uint32_t(g_lb.anim));
                if (slot)
                {
                    const uint32_t rec = InfoPool + 52u * slot;
                    // (0.5.877) started over one frame's step before its end (see the host's DriveBody)
                    const float t = F(base, rec + 16), step = std::clamp(t - g_lb.lastT, 0.0f, 0.5f);
                    const bool end = t >= 0.97f || (step > 0.0f && step < 0.1f && t + 1.5f * step >= 1.0f);
                    if (end && OneShotMove(move)) { SetF(base, rec + 40, 0.0f); g_lb.lastT = t; }   // (0.5.892: held at its end)
                    else if (end) { SetF(base, rec + 16, 0.0f); SetF(base, rec + 20, 0.0f); g_lb.lastT = 0; g_lb.restarts++; }
                    else g_lb.lastT = t;
                    return;
                }
                g_lb.anim = -1;
            }
            int32_t anim = index(BodyMoveHash(move));
            if (anim < 0) { static int toldM = 0; if (toldM++ < 12) { printf("coop: join: the move %d (%s) is not in the host's body's list\n", move, BodyMoveText(move)); fflush(stdout); } anim = index(BodyMoveHash(STAND_IDLE)); }
            if (anim < 0) { static int told2 = 0; if (told2++ < 3) { printf("coop: join: the host's body: none of the moves in its list\n"); fflush(stdout); } if (r.haveAnim && r.list) SetAnims(ctx, base, r); return; }
            if (anim != g_lb.anim)   // (0.5.892: no waiting for room in the pose queue: in a fight there was none for seconds, and the host's body stood with nothing playing)
            {
                PPCContext c = CallCtx(ctx);
                const bool nothing = g_lb.anim < 0;   // (0.5.877) nothing was playing: in at once, not blended from the bind pose
                if (nothing) g_lb.replays++;
                g_lb.lastT = 0;
                // (0.5.878) THE T-POSE: the move was set on its own node only (sub_82500A40), and a node shows only as
                // much as its parent nodes weigh. The parents of a move never set before came in weighing nothing, so
                // the body stood in its bind pose with its idle "playing" at full weight (t877a: both screens). The
                // move is set with the scripts' own call that weighs the parents too (sub_82500B80), as the soldier
                // puppets' are; the rate, which that call does not take, is written after.
                c.r3.u64 = tree; c.r4.u64 = uint32_t(anim); c.f1.f64 = 1.0; c.f2.f64 = nothing ? 0.0 : 0.2;
                CallGuest(c, base, 0x82500B80);   // set anim, the parents too
                if (const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * uint32_t(anim))) { SetF(base, InfoPool + 52u * sl + 40, move == STAND_RUN_F ? 1.15f : 1.0f); if (OneShotMove(move)) { SetF(base, InfoPool + 52u * sl + 16, 0.0f); SetF(base, InfoPool + 52u * sl + 20, 0.0f); } }
                if (nothing) { const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * uint32_t(anim)); if (sl) { SetF(base, InfoPool + 52u * sl + 36, 1.0f); SetF(base, InfoPool + 52u * sl + 32, 1.0f); } }
                if (g_lb.anim >= 0)
                {
                    c = CallCtx(ctx);
                    c.r3.u64 = tree; c.r4.u64 = uint32_t(g_lb.anim); c.f1.f64 = 0.0; c.f2.f64 = 0.2; c.f3.f64 = 1.0; c.r8.u64 = 0; c.r10.u64 = 0;
                    CallGuest(c, base, 0x82500A40);
                }
                if (g_lb.prev >= 0 && g_lb.prev != anim && g_lb.prev != g_lb.anim) { const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * uint32_t(g_lb.prev)); if (sl) { SetF(base, InfoPool + 52u * sl + 36, 0.0f); SetF(base, InfoPool + 52u * sl + 32, 0.0f); } }
                g_lb.prev = g_lb.anim;
                static int told = 0, toldAct = 0;
                if (move > 14 ? toldAct++ < 200 : told++ < 40) { printf("coop: join: the host's body plays move %d %s (animation %d of %u)\n", move, BodyMoveText(move), anim, Guest::Read32(base, list + 4)); fflush(stdout); }
            }
            g_lb.anim = anim; g_lb.move = move;
        }
        // one of ours for the host's entity: the same map entity, or a puppet
        // (0.5.942) our own stationary machine guns are kept (not deleted at the freeze): the host's of the same number and
        // model is drawn by ours, so the joining player's own Use takes it here as it does there (t941a: on the host he was
        // on the MG42, on his copy he still held his rifle in front of a puppet of it)
        bool KeptGun(const uint8_t* base, uint32_t e) { return e && base[e] == 10 && BrocText(base, e + 564) == "misc_mg42"; }
        bool Bind(PPCContext& ctx, uint8_t* base, uint16_t idx, Remote& r)
        {
            const std::string* cls = Name(r.cls);
            // (0.5.995) a probe: each missile of the host's (type 3) as it is drawn here (t994a: German grenades landed
            // by the joining player, his copy's test never saw one)
            if (r.type == 3) { static int told = 0; static std::set<uint32_t> once; if (told < 60 && once.insert((uint32_t(idx) << 16) | r.gen).second) { told++; const std::string* m0 = r.models[0] ? Name(r.models[0]) : nullptr; printf("coop: join: the host's missile %u: class %s, model %s, place %s\n", idx, cls ? cls->c_str() : "?", m0 ? m0->c_str() : (r.models[0] ? "(name not known)" : "-"), r.havePlace ? "yes" : "no"); fflush(stdout); } }
            if (!cls || !r.havePlace) return false;
            // (0.5.935) ours is taken for the host's only when it wears the same model too. t934b: the host made bodies
            // in slots it had freed (the joining player's new body in 244, the Garand's; the host's in 130 and 282),
            // and the joining copy took its own thing of that number (a Garand, a soldier without the host's tree)
            // for them: the host's body drawn as something else, in its own pose, sideways, flashing. Never ours
            // for the host's body either.
            const bool sameModel = [&]() {
                if (!r.models[0]) return true;
                const std::string* m = Name(r.models[0]);
                const uint32_t mine = EntityAt(base, idx);
                return m && mine && ResName(base, ModelSlot(base, mine, 0)) == *m;
            }();
            if (const uint32_t mine = EntityAt(base, idx); mine && sameModel && int32_t(idx) != g_hostBodyIndex && !g_puppets.count(mine) && !g_retired.count(mine) && !(Guest::Read32(base, mine + 632) & 0x400u) && !(Guest::Read32(base, mine + 16) & 0x100000u) && base[mine] == r.type && r.type != 11 && r.type != 1 && (r.type != 10 || KeptGun(base, mine)) && r.type != 2 && r.type != 3 && BrocText(base, mine + 564) == *cls)
            {
                r.local = mine; r.localIndex = idx; r.puppet = false; r.localGen = GenAt(base, idx);
                // (v44n) ours, hidden while the host had none (or before his checkpoint): shown again. Hidden ones
                // were not taken before, so after the host's restart every one of his things became a puppet
                // (539 on Island, none of ours), and the animation of so many overran its buffers: a crash
                // (v44u: a hidden one of ours is never shown again: shown by its flag alone, the renderer read
                // what had been let go while it was hidden, a crash in sub_82154DC0; a puppet stands in for it)
                return true;
            }
            if (r.models[0] == 0 && r.attach.empty() && !r.list) return false;   // (nothing to draw)
            // (v44n) a ceiling on puppets: the game animates and draws only so many
            if (g_puppets.size() >= 360) { static double toldAt = -100; if (Now() - toldAt > 10) { toldAt = Now(); printf("coop: join: %zu puppets already: the host's %u (%s) not drawn\n", g_puppets.size(), idx, cls->c_str()); fflush(stdout); } return false; }
            {
                const std::string* listName = r.list ? Name(r.list) : nullptr;
                const uint32_t want = listName ? AnimList(base, *listName) : 0;
                // (v44f) only a puppet that already wears exactly these models: a model slot the new entity
                // has empty is not cleared by G_SetModel, and a plane's puppet came back as a soldier with the
                // plane still on it (Saint-Lo: a P-47 on the ground)
                std::vector<std::string> wantModels(4);
                for (int sl = 0; sl < 4; sl++) if (r.models[sl]) { const std::string* n = Name(r.models[sl]); if (!n) return false; wantModels[sl] = *n; }
                for (size_t k = 0; k < g_pool.size(); k++)
                {
                    Pooled& p = g_pool[k];
                    const bool alive = p.index >= 0 && EntityAt(base, uint32_t(p.index)) == p.e && GenAt(base, uint32_t(p.index)) == p.gen && base[p.e] != 0;
                    if (!alive) { g_pool.erase(g_pool.begin() + long(k)); k--; continue; }
                    std::vector<std::string> wantAttach;
                    for (size_t a = 0; a + 1 < r.attach.size(); a += 2) { const std::string* m = Name(r.attach[a]); const std::string* t = Name(r.attach[a + 1]); if (m && t) { wantAttach.push_back(*m); wantAttach.push_back(*t); } }
                    if (p.list != want || p.models != wantModels || p.attach != wantAttach) continue;   // (v45e: the same attachments too)
                    r.local = p.e; r.localIndex = p.index; r.localGen = p.gen; r.puppet = true;
                    r.shownModels = p.models; r.shownAttach = p.attach; r.animNew = true;
                    g_puppets.insert(p.e);
                    Guest::Write32(base, p.e + 632, Guest::Read32(base, p.e + 632) & ~0x400u);
                    g_pool.erase(g_pool.begin() + long(k));
                    return true;
                }
            }
            const uint32_t org = Scratch(base) + 256;
            for (int k = 0; k < 3; k++) SetF(base, org + 4 * k, r.track.back().o[k]);
            PPCContext c = CallCtx(ctx);
            c.r3.u64 = GuestBroc(base, "script_model"); c.r4.u64 = org; c.r5.u64 = 0;
            CallGuest(c, base, 0x824B4510);   // the scripts' spawn
            const uint32_t e = FromRef(base, c.r3.u32);
            if (!e) { static int told = 0; if (told++ < 20) { printf("coop: join: no puppet could be made for the host's %u (%s)\n", idx, cls->c_str()); fflush(stdout); } return false; }
            r.local = e; r.localIndex = IndexOf(base, e); r.puppet = true; r.localGen = r.localIndex >= 0 ? GenAt(base, uint32_t(r.localIndex)) : 0;
            g_puppets.insert(e);
            { static int told = 0; if (told++ < 150 && r.type != 11) { const std::string* m0 = r.models[0] ? Name(r.models[0]) : nullptr; printf("coop: join: puppet %08X for the host's %u (type %u, %s, %s)\n", e, idx, r.type, cls->c_str(), m0 ? m0->c_str() : "-"); fflush(stdout); } }
            c = CallCtx(ctx); c.r3.u64 = RefOf(base, uint32_t(r.localIndex));
            CallGuest(c, base, 0x824A6820);   // notsolid
            r.shownModels.clear(); r.shownAttach.clear(); r.animNew = true;
            return true;
        }
        void Unbind(PPCContext& ctx, uint8_t* base, Remote& r)
        {
            const bool mine = r.local && r.localIndex >= 0 && EntityAt(base, uint32_t(r.localIndex)) == r.local && GenAt(base, uint32_t(r.localIndex)) == r.localGen && base[r.local] != 0;
            if (!mine && r.puppet) g_puppets.erase(r.local);
            if (mine && r.puppet)
            {
                Guest::Write32(base, r.local + 632, Guest::Read32(base, r.local + 632) | 0x400u);
                if (Guest::Read32(base, r.local + 324)) { PPCContext c = CallCtx(ctx); c.r3.u64 = RefOf(base, uint32_t(r.localIndex)); CallGuest(c, base, 0x824A6820); }   // (v45u: a brush puppet put by: not solid)
                g_puppets.erase(r.local);
                const uint32_t tree = Guest::Read32(base, r.local + 552);
                if (g_frozen && g_pool.size() < 16) g_pool.push_back(Pooled{ r.local, r.localIndex, r.localGen, Valid(tree) ? Guest::Read32(base, tree + 8) : 0u, r.shownModels, r.shownAttach });
                else DeleteEntity(ctx, base, uint32_t(r.localIndex));
            }
            // (v44u) ours, adopted: left as it is (the host's checkpoint makes his things again under new numbers:
            // ours is taken again at once; if the host truly has it no more, the sweep below hides it)
            else if (mine) {}
            r.local = 0; r.localIndex = -1;
        }
        // ours gone: an actor by the scripts' actor delete (sub_824A72D8: freed now, or marked for the
        // actor system to free when it may), anything else by the scripts' delete
        void Remove(PPCContext& ctx, uint8_t* base, uint32_t index)
        {
            const uint32_t e = EntityAt(base, index);
            if (!e) return;
            Guest::Write32(base, e + 632, Guest::Read32(base, e + 632) | 0x400u);   // (out of sight now, whenever it is freed)
            g_retired.insert(e);
            if (g_frozen && base[e] != 11)
            {
                if (base[e] != 1 && Guest::Read32(base, e + 324)) Guest::Write32(base, e + 324, 0);   // (v45u: not solid either)
                return;   // (v44b) frozen: the rest only hidden, never freed by us
            }
            if (base[e] == 11 && Valid(Guest::Read32(base, e + 532))) { PPCContext c = CallCtx(ctx); c.r3.u64 = RefOf(base, index); CallGuest(c, base, 0x824A72D8); }
            else DeleteEntity(ctx, base, index);
        }
        void KillScripts(uint8_t* base)
        {
            int n = 0;
            for (uint32_t t = Guest::Read32(base, ScriptVm + 4); Valid(t) && t != ScriptVm + 8 && n < 4096; t = Guest::Read32(base, t), n++)
            {
                const uint32_t f = Guest::Read32(base, t + 16);
                if (!g_threadFlags.count(t)) g_threadFlags[t] = f;   // (v46: put back before a reload in this process)
                Guest::Write32(base, t + 16, f | 0x4Cu);
            }
        }
        std::string ThreadCensus(const uint8_t* base)
        {
            std::map<uint32_t, int> by; int n = 0;
            for (uint32_t t = Guest::Read32(base, ScriptVm + 4); Valid(t) && t != ScriptVm + 8 && n < 100000; t = Guest::Read32(base, t), n++) by[Guest::Read32(base, t + 16)]++;
            std::string out = std::to_string(n) + " threads, flags:";
            int k = 0; for (auto& [f, c] : by) { if (k++ >= 12) { out += " ..."; break; } char b[32]; snprintf(b, sizeof b, " %X x%d", f, c); out += b; }
            return out;
        }
        // how the joining copy's script VM is stopped: 0 the old way (threads marked each frame, the VM run),
        // 1 marked once (the VM run), 2 marked once and the VM not run from the freeze on (the default)
        int VmMode()
        {
            static const int mode = [] { const char* m = getenv("COD3_COOP_VM"); if (!m) return 2; if (!_stricmp(m, "old")) return 0; if (!_stricmp(m, "once")) return 1; return 2; }();
            return mode;
        }
        void Freeze(PPCContext& ctx, uint8_t* base)
        {
            g_frozen = true; g_frozenAt = Now();
            const uint32_t table = Guest::Read32(base, 0x82A2A2A0);
            if (Valid(table))
            {
                const uint32_t was = Guest::Read32(base, table + 0xB60);
                if (was != 0x82497FF0u) g_levelFnSaved = was;   // (v46: put back for a reload in this process)
                Guest::Write32(base, table + 0xB60, 0x82497FF0);   // the level's functions by name: none run here now
            }
            KillScripts(base);
            // its scripted scenes (the level's intro camera and the like) end here: the host's go on
            const uint32_t scenes = Guest::Read32(base, 0x825F51A0);
            { PPCContext c = CallCtx(ctx); CallGuest(c, base, 0x824F47B0); }
            int deleted = 0;
            // (v45l) our vehicles go for good, first (their riders with them): left in the world and only
            // hidden, a vehicle whose driver we had deleted freed his animations again every frame (a double
            // free each frame, then a malloc that never returned: Falaise's jeep, at 30 s)
            {
                const uint32_t me = Me(base);
                if (me && Guest::Read32(base, me + 916)) { PPCContext c = CallCtx(ctx); c.r3.u64 = me; CallGuest(c, base, 0x82540D78); }
                int vehicles = 0;
                for (uint32_t i = 2; i < 4096; i++)
                    if (const uint32_t e = EntityAt(base, i); e && base[e] == 10 && !g_puppets.count(e) && !KeptGun(base, e))
                    {
                        // (0.5.964) riding it (Falaise's intro jeep, t963a): left where it was, in its place, while the host's
                        // jeep went on: put where the host stands
                        if (me) { const float dx = F(base, e + 336) - F(base, me + 336), dy = F(base, e + 340) - F(base, me + 340), dz = F(base, e + 344) - F(base, me + 344); if (dx * dx + dy * dy + dz * dz < 250.0f * 250.0f) g_catchUpHost = true; }
                        Guest::Write32(base, e + 632, Guest::Read32(base, e + 632) | 0x400u); DeleteEntity(ctx, base, i); vehicles++;
                    }
                if (vehicles) { printf("coop: join: %d of our vehicles deleted\n", vehicles); fflush(stdout); }
            }
            // (v45r) our drones (script_models with a tree) are deleted for good too, now, once: only hidden, each
            // still had its pose made every frame, and with the host's drawn on top the frame's pose jobs ran
            // past what fits (soldiers left in their bind pose when theirs waited)
            {
                int drones = 0;
                for (uint32_t i = 2; i < 4096; i++)
                    if (const uint32_t e = EntityAt(base, i); e && base[e] == 7 && Valid(Guest::Read32(base, e + 552)) && !g_puppets.count(e)) { Guest::Write32(base, e + 632, Guest::Read32(base, e + 632) | 0x400u); DeleteEntity(ctx, base, i); drones++; }
                if (drones) { printf("coop: join: %d of our drones deleted\n", drones); fflush(stdout); }
            }
            for (uint32_t i = 2; i < 4096; i++)
                if (const uint32_t e = EntityAt(base, i); e && (base[e] == 11 || (base[e] == 10 && !KeptGun(base, e)) || (base[e] == 7 && Valid(Guest::Read32(base, e + 552)))) && !g_puppets.count(e)) { Remove(ctx, base, i); deleted++; }
            // (v43g) a census of what is left of our own world, by type: what still simulates here
            {
                int count[16] = {};
                for (uint32_t i = 0; i < 4096; i++) if (const uint32_t e = EntityAt(base, i); e && base[e] < 16) count[base[e]]++;
                printf("coop: join: left of our world by type:");
                for (int t = 0; t < 16; t++) if (count[t]) printf(" %d:%d", t, count[t]);
                printf("\n"); fflush(stdout);
            }
            printf("coop: join: this copy's world stops here: its scripts ended, %u scene(s) ended, %d of its soldiers and drones deleted; the host's world is drawn from now\n", scenes, deleted); fflush(stdout);
        }
    }

    bool JoinFrozen() { return g_frozen; }
    // the script VM's frame is not run on the joining copy from its freeze on (v43a: run once more after the
    // freeze, it spun for ever on Island, its threads told to end and its soldiers gone under it)
    bool JoinSkipVm() { return (g_frozen || g_vmHold) && VmMode() == 2; }
    bool JoinIsPuppet(const uint8_t* base, uint32_t e) { (void)base; return g_puppets.count(e) != 0; }
    namespace { bool g_ownLetterbox = false, g_ownIntroScreen = false; }
    void JoinOwnScriptDvar(const std::string& name, const std::string& value)
    {
        if (g_frozen) return;
        if (_stricmp(name.c_str(), "letterbox_enabled") == 0) g_ownLetterbox = value == "1";
        if (_stricmp(name.c_str(), "introScreen") == 0) g_ownIntroScreen = value == "1";
    }

    uint32_t JoinLocalFor(const uint8_t* base, uint16_t n)
    {
        if (n == 0xFFFD) return Me(base);
        if (n == 0xFFFE) n = g_hostBodyIndex >= 0 ? uint16_t(g_hostBodyIndex) : 0xFFFF;
        if (n >= 4096) return 0;
        const auto it = g_remote.find(n);
        if (it != g_remote.end() && it->second.local && Valid(it->second.local) && base[it->second.local] != 0) return it->second.local;
        // the level's own (the world, its fixed things above 1000): the same number here
        if (n == 0 || n >= 1000) return EntityAt(base, n);
        return 0;
    }

    bool JoinPartner(const uint8_t* base, float pos[3], int& health, int& maxHealth)
    {
        if (!g_havePs || Now() - g_psAt > 2.0) return false;
        health = g_hostHealth; maxHealth = g_hostMaxHealth;
        memcpy(pos, g_hostPos, 12);
        // his body as drawn here (between snapshots, 100 ms back) rather than his last word
        if (g_hostBodyIndex >= 0)
        {
            const auto it = g_remote.find(uint16_t(g_hostBodyIndex));
            if (it != g_remote.end() && it->second.local && Valid(it->second.local) && base[it->second.local] != 0)
                for (int k = 0; k < 3; k++) pos[k] = F(base, it->second.local + 336 + 4 * k);
        }
        return true;
    }

    void JoinMissionStart(const std::string& name)
    {
        g_remote.clear(); g_puppets.clear(); g_pool.clear(); g_retired.clear(); g_sweepHidden.clear(); g_stableAt = -1; g_threadFlags.clear();   // (the names: cleared by the host's R_MAPSTART, in order with his new ones)
        g_frozen = false; g_inSince = -1; g_frozenAt = -1; g_firstSnapAt = -1; g_lastSnapAt = -1;
        g_health = g_maxHealth = -1; g_psAt = -1;
        g_ownLetterbox = g_ownIntroScreen = false;
        for (bool& h : g_hudHave) h = false;
        (void)name;
    }

    void JoinReceive(PPCContext& ctx, uint8_t* base, Kind kind, const uint8_t* data, size_t size)
    {
        (void)ctx;
        if (kind == K_FX) { if (g_frozen) FxReceive(data, size); return; }   // (0.5.872)
        if (g_vmHold) return;   // (v46) reloading in this process: the host's new world waits for our new level
                                 // (taken into the old one, every new thing of his became a puppet: hundreds, out of memory, Mayenne)
        if (kind == K_PS && size >= 20)
        {
            int32_t ps[5]; memcpy(ps, data, 20);
            g_health = ps[0]; g_maxHealth = ps[1]; g_hostFlags = uint32_t(ps[3]); g_hostAllow = uint32_t(ps[4]); g_psAt = Now(); g_havePs = true;
            if (size >= 44)
            {
                int32_t h[3]; memcpy(h, data + 20, 12); memcpy(g_hostPos, data + 32, 12);
                g_hostHealth = h[0]; g_hostMaxHealth = h[1]; g_hostBodyIndex = h[2];
            }
            if (size >= 48) memcpy(&g_hostYaw, data + 44, 4);
            if (size >= 60) { memcpy(g_hostHint, data + 48, 12); g_hostHintAt = Now(); }   // (0.5.919)
            return;
        }
        if (kind != K_SNAP || size < 16 || !InMission(base)) return;
        const double t = PacketTime();   // the host's time this was sent at
        { const double gap = g_lastSnapAt >= 0 ? Now() - g_lastSnapAt : 0; static int told = 0; if (gap > 1.5 && told++ < 40) { printf("coop: join: no snapshot from the host for %.1f s (his copy stood still, or the line did): nothing let go\n", gap); fflush(stdout); } }
        g_lastSnapAt = Now();
        size_t at = 16;
        if (g_firstSnapAt < 0) { g_firstSnapAt = Now(); printf("coop: join: the host's snapshots are coming\n"); fflush(stdout); }
        while (at + 6 <= size)
        {
            uint16_t idx, gen; memcpy(&idx, data + at, 2); memcpy(&gen, data + at + 2, 2);
            const uint8_t type = data[at + 4], mask = data[at + 5];
            at += 6;
            if (mask & M_REMOVED)
            {
                const auto it = g_remote.find(idx);
                if (it != g_remote.end()) { Unbind(ctx, base, it->second); g_remote.erase(it); }
                continue;
            }
            Remote& r = g_remote[idx];
            // (0.5.890) a grenade that lands (a missile, type 3, become a thing, type 0) keeps its puppet: a new one
            // was made for it, and the smoke that followed the old one was left where that one was let go
            if (r.gen == gen && r.type == 3 && type == 0 && r.local && r.puppet) r.type = type;
            if (r.gen != gen || r.type != type) { if (r.local) Unbind(ctx, base, r); r = Remote{}; r.gen = gen; r.type = type; }
            r.heard = Now();
            if (mask & M_ORIGIN || mask & M_ANGLES)
            {
                Sample s{ t, {}, {} };
                if (!r.track.empty()) { memcpy(s.o, r.track.back().o, 12); memcpy(s.a, r.track.back().a, 12); }
                if (mask & M_ORIGIN) { if (at + 12 > size) return; memcpy(s.o, data + at, 12); at += 12; }
                if (mask & M_ANGLES) { if (at + 6 > size) return; int16_t a[3]; memcpy(a, data + at, 6); at += 6; for (int k = 0; k < 3; k++) s.a[k] = float(a[k]) * (180.0f / 32768.0f); }
                if (!r.track.empty() && s.t <= r.track.back().t) r.track.back() = s; else r.track.push_back(s);
                while (r.track.size() > 2 && r.track.back().t - r.track.front().t > 1.5) r.track.pop_front();
                r.havePlace = true;
            }
            if (mask & M_MODELS) { if (at + 10 > size) return; memcpy(r.models, data + at, 8); memcpy(&r.cls, data + at + 8, 2); at += 10; r.haveModels = true; }
            if (mask & M_ATTACH) { if (at + 1 > size) return; const uint8_t n = data[at++]; if (at + n * 4u > size) return; r.attach.assign(n * 2, 0); if (n) memcpy(r.attach.data(), data + at, n * 4u); at += n * 4u; r.haveAttach = true; }
            if (mask & M_FLAGS) { if (at + 5 > size) return; memcpy(&r.flags, data + at, 4); r.extra = data[at + 4]; at += 5; r.haveFlags = true; }
            if (mask & M_ANIM)
            {
                if (at + 3 > size) return;
                uint16_t list; memcpy(&list, data + at, 2); const uint8_t n = data[at + 2]; at += 3;
                if (at + n * 10u > size) return;
                if (list != r.list) r.animNew = true;
                r.list = list;
                std::vector<Node> nodes(n);
                for (uint8_t k = 0; k < n; k++)
                {
                    const uint8_t* q = data + at + k * 10u;
                    uint16_t anim, time, rate, blend; memcpy(&anim, q, 2); memcpy(&time, q + 4, 2); memcpy(&rate, q + 6, 2); memcpy(&blend, q + 8, 2);
                    nodes[k] = Node{ anim, q[2] / 255.0f, q[3] / 255.0f, time / 65535.0f, rate / 1000.0f, blend / 1000.0f };
                }
                at += n * 10u;
                r.nodes = std::move(nodes);
                r.haveAnim = true;
            }
        }
    }

    // (v46) the host went back to a checkpoint: this copy loads its mission again in this process, behind the
    // game's own loading screen (World at War's clients reload with the server), not as a new process (v45,
    // the window closed and opened again). The stopped world's script VM is kept stopped till the new level
    // is up (v45a: loaded again with it running, it spun on the old world's threads), the level's functions
    // by name are put back, and a watchdog starts this program again from nothing if the reload stalls.
    void ReloadWatchdog()
    {
        static bool started = false;
        if (started) return;
        started = true;
        std::thread([] {
            uint32_t seen = g_joinFrames; double seenAt = Now();
            for (;;)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                const double at = g_reloadInGameAt;
                if (at < 0) { seen = g_joinFrames; seenAt = Now(); continue; }
                if (g_joinFrames != seen) { seen = g_joinFrames; seenAt = Now(); }
                const bool stalled = Now() - seenAt > 45.0, late = Now() - at > 120.0;
                if (stalled || late)
                {
                    printf("coop: join: the reload in this process %s: started again from nothing instead\n", stalled ? "stalled (no frame for 45 s)" : "took over 120 s"); fflush(stdout);
                    ReloadMission();
                    return;
                }
            }
        }).detach();
    }
    bool JoinReloadInGame(uint8_t* base)
    {
        // (v46) off unless asked for (COD3_COOP_RELOAD_INGAME=1): Mayenne's new level crashed on the stopped world's
        // leftovers; the swap behind the window (coop_swap.cpp) is the way now
        if (const char* f = getenv("COD3_COOP_RELOAD_INGAME"); !f || f[0] != '1') return false;
        const std::string map = MyMap();
        if (map.empty()) return false;
        int threads = 0;
        for (uint32_t t = Guest::Read32(base, ScriptVm + 4); Valid(t) && t != ScriptVm + 8 && threads < 100000; t = Guest::Read32(base, t)) threads++;
        printf("coop: join: the host went back to a checkpoint: %s loaded again in this process (script VM: %d threads, %u pending; %s)\n", map.c_str(), threads, Guest::Read32(base, ScriptVm + 16), ThreadCensus(base).c_str()); fflush(stdout);
        // the stopped world's threads stay marked ended through the level's end (put back as they were, Mayenne's
        // ran again in the shutdown on the things we had deleted, and the game crashed); they are taken out of
        // the VM's list once the new level is up (JoinMissionLoaded), so its VM never runs on them (v46a: it spun)
        g_oldThreads.clear();
        for (uint32_t t = Guest::Read32(base, ScriptVm + 4), n = 0; Valid(t) && t != ScriptVm + 8 && n < 100000; t = Guest::Read32(base, t), n++) g_oldThreads.insert(t);
        g_threadFlags.clear();
        const uint32_t table = Guest::Read32(base, 0x82A2A2A0);
        if (Valid(table) && g_levelFnSaved) Guest::Write32(base, table + 0xB60, g_levelFnSaved);
        g_vmHold = true;
        g_reloadInGameAt = Now(); g_reloadAskedAt = Now(); g_reloadTries = 1;
        Notice("Co-op: back to the checkpoint...", 4000);
        Kernel::QueueConsoleCommand("spmap " + map);
        ReloadWatchdog();
        return true;
    }
    // (v46) the mission's new level is up (after SV_SpawnServer): the VM runs again (till our next freeze)
    void JoinMissionLoaded(uint8_t* base)
    {
        if (!g_vmHold) return;
        int threads = 0;
        for (uint32_t t = Guest::Read32(base, ScriptVm + 4); Valid(t) && t != ScriptVm + 8 && threads < 100000; t = Guest::Read32(base, t)) threads++;
        printf("coop: join: the level is up again (script VM: %d threads, %u pending; %s)\n", threads, Guest::Read32(base, ScriptVm + 16), ThreadCensus(base).c_str()); fflush(stdout);
        // the old level's threads out of the VM's list (a list linked both ways: +0 next, +4 the one before),
        // only if every link in it says so
        {
            { const uint32_t h = Guest::Read32(base, ScriptVm + 4); printf("coop: join: VM list words %08X %08X %08X %08X; first %08X: %08X %08X\n", Guest::Read32(base, ScriptVm + 0), Guest::Read32(base, ScriptVm + 4), Guest::Read32(base, ScriptVm + 8), Guest::Read32(base, ScriptVm + 12), h, Valid(h) ? Guest::Read32(base, h) : 0, Valid(h) ? Guest::Read32(base, h + 4) : 0); }
            std::vector<uint32_t> nodes; bool both = true;
            uint32_t prev = ScriptVm + 4;
            for (uint32_t t = Guest::Read32(base, ScriptVm + 4), n = 0; Valid(t) && t != ScriptVm + 8 && n < 100000; t = Guest::Read32(base, t), n++) { nodes.push_back(t); if (Guest::Read32(base, t + 4) != prev && !(prev == ScriptVm + 4)) both = false; prev = t; }
            int taken = 0;
            if (both && !nodes.empty())
            {
                for (uint32_t t : nodes)
                {
                    if (!g_oldThreads.count(t)) continue;
                    const uint32_t next = Guest::Read32(base, t), before = Guest::Read32(base, t + 4);
                    if (before == ScriptVm + 4 || t == Guest::Read32(base, ScriptVm + 4)) Guest::Write32(base, ScriptVm + 4, next); else Guest::Write32(base, before, next);
                    if (next != ScriptVm + 8 && Valid(next)) Guest::Write32(base, next + 4, before);
                    else if (next == ScriptVm + 8 && Guest::Read32(base, ScriptVm + 12) == t) Guest::Write32(base, ScriptVm + 12, before);   // (the list's end node's link back)
                    taken++;
                }
            }
            printf("coop: join: %d of the old level's threads taken out of the VM's list%s (%s)\n", taken, both ? "" : " (NOT: its links are not both ways)", ThreadCensus(base).c_str()); fflush(stdout);
            g_oldThreads.clear();
        }
        g_vmHold = false;
    }
    void JoinReceiveReliable(PPCContext& ctx, uint8_t* base, RelKind kind, const uint8_t* data, size_t size)
    {
        if (kind == R_INV) { InventoryJoinReceive(data, size); return; }   // (0.5.901)
        if (kind == R_EVENT && size >= 2 && data[0] == E_CALL) { EventsReceive(data, size); return; }
        if (kind == R_EVENT && size >= 7 && data[0] == E_DVAR)
        {
            uint32_t flags; memcpy(&flags, data + 1, 4);
            const char* name = reinterpret_cast<const char*>(data + 5);
            const size_t n = strnlen(name, size - 5);
            if (5 + n + 1 < size)
            {
                const char* value = name + n + 1;
                g_dvarPending.emplace_back(std::string(name, n), std::string(value, strnlen(value, size - 5 - n - 1)), flags);
            }
            return;
        }
        if (kind == R_NAME && size >= 3)
        {
            uint16_t id; memcpy(&id, data, 2);
            g_names[id] = std::string(reinterpret_cast<const char*>(data + 3), size - 3);
            return;
        }
        if (kind == R_MAPSTART && size >= 68)
        {
            uint32_t count; memcpy(&count, data, 4);
            char name[64]; memcpy(name, data + 4, 63); name[63] = 0;
            printf("coop: join: the host starts %s (count %u)\n", name, count); fflush(stdout);
            g_names.clear();   // his names from here on are this mission's
            if (MyMap() != name) FollowMap(name, count);
            return;
        }
        if (kind == R_CONFIGSTRING && size >= 2)
        {
            uint16_t index; memcpy(&index, data, 2);
            {
                std::string text(reinterpret_cast<const char*>(data + 2), size - 2);
                bool replaced = false;
                for (auto& [i, t] : g_csPending) if (i == index) { t = std::move(text); replaced = true; break; }   // (the latest of each)
                if (!replaced) g_csPending.emplace_back(index, std::move(text));
            }
            return;
        }
        if (kind == R_SERVERCMD && size >= 1) { g_cmdPending.emplace_back(reinterpret_cast<const char*>(data), size); return; }
        if (kind == R_HUD && size >= 125 && data[0] < 16) { memcpy(g_hud[data[0]], data + 1, 124); g_hudHave[data[0]] = true; return; }
        if (kind == R_MISSION && size >= 16 && data[0] == 2)
        {
            // the host's mission started again (his checkpoint): I come back in beside him
            float o[3]; memcpy(o, data + 4, 12);
            const uint32_t me = Me(base), rec = Record(base, me);
            const bool room = size >= 2 && data[1] == 1;   // (1: only room made beside the host as I come in)
            printf("coop: join: %s at %.0f %.0f %.0f\n", room ? "the host makes room: I come in" : "the host's mission started again: back in beside him", o[0], o[1], o[2]); fflush(stdout);
            // (v45u) room made near where I stand on firm ground: not lower than that ground (Mayenne: the host's
            // floor there is 40 below what mine stands on; put at his height I was inside my own and fell out of the world)
            if (room && rec && g_stableAt >= 0)
            {
                const float dx = o[0] - g_stable[0], dy = o[1] - g_stable[1];
                if (dx * dx + dy * dy < 200.0f * 200.0f && o[2] < g_stable[2] + 4.0f) { printf("coop: join: room at my own ground's height (%.0f, not %.0f)\n", g_stable[2] + 4.0f, o[2]); fflush(stdout); o[2] = g_stable[2] + 4.0f; }
            }
            if (rec)
            {
                for (int k = 0; k < 3; k++) { SetF(base, rec + 4 * k, o[k]); SetF(base, rec + 16 + 4 * k, 0.0f); }
                for (const uint32_t at : { 32u, 336u }) for (int k = 0; k < 3; k++) SetF(base, me + at + 4 * k, o[k]);
                LinkEntity(ctx, base, me);
            }
            if (!room && g_frozen) { g_reloading = true; if (!JoinReloadInGame(base)) ReloadMission(); }   // (v45: a new process; v46: in this one)
            if (!room) Notice("Co-op: back to the checkpoint.", 3000);
            return;
        }
        (void)ctx;
    }

    // (0.5.972) a test: COD3_COOP_TESTLIVE=1 (or a pad mask in hex, default 4000: X, Use): when a grenade (a missile,
    // type 3) has lain still within 90 units of this copy's player for a moment, the buttons are held for 1.2 s, once
    // per grenade: whether a live grenade can be picked up (either copy; the joining copy's Use goes to the host).
    // (0.5.983) a probe: COD3_COOP_VIEWSCAN="t1 t2 t3 ...": at t1 the game's image (0x82000000..0x82C30000) is searched
    // for three floats that are this copy's eye (x, y within 2 units of the player, z 20..80 above his feet): the
    // client game's view (refdef origin) among them; at each later time those places are read again (a scripted
    // cutscene's camera shows as a place far from the player). For the joining player's cutscene camera.
    void TestViewScan(uint8_t* base, uint32_t me)
    {
        static std::vector<double> times; static bool parsed = false; static size_t next = 0; static std::vector<uint32_t> found;
        if (!parsed)
        {
            parsed = true;
            if (const char* t = getenv("COD3_COOP_VIEWSCAN")) { std::string s(t); size_t at = 0; while (at < s.size()) { const size_t sp = s.find(' ', at); times.push_back(atof(s.substr(at, sp == std::string::npos ? std::string::npos : sp - at).c_str())); if (sp == std::string::npos) break; at = sp + 1; } }
        }
        if (next >= times.size() || !me || Now() < times[next]) return;
        const float o[3] = { F(base, me + 336), F(base, me + 340), F(base, me + 344) };
        if (next == 0)
        {
            // (0.5.987) t986e: nothing in the image (0x82000000..): the heaps too (virtual 0x40000000.., physical
            // 0xA0000000..), 64 KB at a time, what is committed only; the eye 0..90 above the feet
            const std::pair<uint32_t, uint32_t> ranges[] = { { 0x82000000u, 0x83000000u }, { 0x40000000u, 0x80000000u }, { 0xA0000000u, 0xC0000000u } };
            // (0.5.989) and how much was searched, and how many places hold my FEET (the entity's own origin must be one:
            // t988a found nothing at all)
            uint32_t chunks = 0, feet = 0, firstFeet = 0;
            for (const auto& [from, to] : ranges)
                for (uint32_t chunk = from; chunk < to && found.size() < 200; chunk += 0x10000u)
                {
                    if (!CoopCommitted(base + chunk, 0x10000)) continue;
                    chunks++;
                    for (uint32_t a = chunk; a + 12 <= chunk + 0x10000u && found.size() < 200; a += 4)
                    {
                        const float x = F(base, a), y = F(base, a + 4);
                        if (!(fabsf(x - o[0]) < 2.0f && fabsf(y - o[1]) < 2.0f)) continue;   // NaN fails too (t987a: 64 NaN places)
                        const float z = F(base, a + 8);
                        if (z - o[2] > 10.0f && z - o[2] < 90.0f) found.push_back(a);
                        else if (fabsf(z - o[2]) < 1.0f) { if (!feet) firstFeet = a; feet++; }
                    }
                }
            printf("coop: view scan at %.0f s: me at %.0f %.0f %.0f (entity %08X), %zu places hold my eye; %u committed 64 KB blocks searched, %u places hold my feet (first %08X)\n", Now(), o[0], o[1], o[2], me, found.size(), chunks, feet, firstFeet);
        }
        else printf("coop: view scan at %.0f s: me at %.0f %.0f %.0f\n", Now(), o[0], o[1], o[2]);
        for (const uint32_t a : found)
        {
            std::string v; for (int k = -3; k < 12; k++) { char b[24]; snprintf(b, sizeof b, " %.2f", F(base, a + 4 * k)); v += b; }
            printf("coop: view scan:   %08X:%s\n", a, v.c_str());
        }
        fflush(stdout);
        next++;
    }

    void TestLive(uint8_t* base, uint32_t me)
    {
        static const uint16_t mask = []() -> uint16_t { const char* t = getenv("COD3_COOP_TESTLIVE"); if (!t || !t[0]) return 0; const unsigned long v = strtoul(t, nullptr, 16); return uint16_t(v > 1 ? v : 0x4000); }();
        if (!mask || !me) return;
        // (0.5.998) after a pick-up, the throw back: the grenade button held 1 s (the game throws on its release)
        static double throwAt = -1;
        if (throwAt > 0 && Now() >= throwAt) { throwAt = -1; Input::HoldButtons(0x0200, 1000); printf("coop: test: the picked-up grenade thrown back (grenade button held 1 s)\n"); fflush(stdout); }
        struct Seen { float p[3]; double stillSince; bool done; uint32_t gen; };
        static std::map<uint32_t, Seen> seen;
        const double now = Now();
        std::set<uint32_t> alive;
        std::set<uint32_t> hisNades;
        if (g_frozen) for (const auto& [hi, r] : g_remote) if (r.local && (r.type == 3 || (r.type == 0 && Name(r.cls) && *Name(r.cls) == "grenade"))) hisNades.insert(r.local);
        // (0.5.997) every slot (t996a: the joining copy's puppets for the host's grenades lie past 1024: never seen)
        for (uint32_t i = 1; i < 4096; i++)
        {
            const uint32_t e = EntityAt(base, i);
            if (!e || e == me) continue;
            // (0.5.994) on the joining copy the host's grenades are puppets (other types): known by their model's name
            bool isNade = base[e] == 3 || (base[e] == 0 && BrocText(base, e + 564) == "grenade");
            // (0.5.996) or by the host's own word: what stands here for a missile of his (t995a: his German grenades
            // were drawn here, the model's name did not find them)
            if (!isNade && hisNades.count(e)) isNade = true;
            if (!isNade && g_frozen && ModelSlot(base, e, 0))
            {
                std::string m = ResName(base, ModelSlot(base, e, 0));
                for (char& c : m) c = char(tolower(uint8_t(c)));
                isNade = m.find("nade_world") != std::string::npos;
            }
            if (!isNade) continue;
            const float p[3] = { F(base, e + 336), F(base, e + 340), F(base, e + 344) };
            const float dx = p[0] - F(base, me + 336), dy = p[1] - F(base, me + 340), dz = p[2] - F(base, me + 344);
            {
                static std::map<uint32_t, uint32_t> told; static int lines = 0;
                // (0.5.992) told by generation + 1: a first-generation grenade (0) was never told (t990g: none of 37 seen)
                if (told[i] != GenAt(base, i) + 1 && lines < 60) { told[i] = GenAt(base, i) + 1; lines++; printf("coop: test: a grenade (%u, %s) %.0f from me (height %.0f)\n", i, ResName(base, ModelSlot(base, e, 0)).c_str(), sqrtf(dx * dx + dy * dy), dz); fflush(stdout); }
            }
            if (dx * dx + dy * dy > 90.0f * 90.0f || fabsf(dz) > 80.0f) continue;
            alive.insert(i);
            auto it = seen.find(i);
            if (it == seen.end() || it->second.gen != GenAt(base, i)) { seen[i] = { { p[0], p[1], p[2] }, -1, false, GenAt(base, i) }; continue; }
            Seen& s = it->second;
            const float mx = p[0] - s.p[0], my = p[1] - s.p[1], mz = p[2] - s.p[2];
            memcpy(s.p, p, 12);
            if (mx * mx + my * my + mz * mz > 1.0f) { s.stillSince = -1; continue; }
            if (s.stillSince < 0) s.stillSince = now;
            if (!s.done && now - s.stillSince > 0.15)
            {
                s.done = true;
                Input::HoldButtons(mask, 1200);
                throwAt = now + 1.5;
                printf("coop: test: grenade %u lies %.0f from me: buttons %04X held 1.2 s\n", i, sqrtf(dx * dx + dy * dy), mask); fflush(stdout);
            }
        }
        for (auto it = seen.begin(); it != seen.end();) { if (alive.count(it->first)) ++it; else it = seen.erase(it); }
    }

    // (0.5.907) a test: COD3_COOP_TESTGO="170:x,y,z 200:x,y,z,d": at that many seconds of this process this copy's player
    // is put there (feet), or (four numbers) d units short of the point x,y along his own heading, at height z:
    // facing it. Hands-off tests of Use prompts and damage triggers (either copy).
    void TestGo(PPCContext& ctx, uint8_t* base, uint32_t me, uint32_t rec)
    {
        const double now = Now();
            struct Go { double at; float v[6]; int n; bool done; };
            static std::vector<Go> gos; static bool parsed = false;
            if (!parsed)
            {
                parsed = true;
                if (const char* t = getenv("COD3_COOP_TESTGO"))
                {
                    std::string all(t); size_t at = 0;
                    while (at < all.size())
                    {
                        size_t end = all.find(' ', at); if (end == std::string::npos) end = all.size();
                        const std::string item = all.substr(at, end - at); at = end + 1;
                        Go g{}; g.n = sscanf(item.c_str(), "%lf:%f,%f,%f,%f,%f,%f", &g.at, &g.v[0], &g.v[1], &g.v[2], &g.v[3], &g.v[4], &g.v[5]) - 1;
                        if (g.n >= 3) gos.push_back(g);
                    }
                }
            }
            // (0.5.954) COD3_COOP_TESTGUN="t:d": at t, d short of the newest gun dropped near the joining player (the
            // host's item watch writes it to coop-testgun.txt), on the line from here, looking at it (tests of pickups)
            {
                static double gunAt = -1, gunD = 40; static bool gunParsed = false, gunDone = false;
                if (!gunParsed) { gunParsed = true; if (const char* t = getenv("COD3_COOP_TESTGUN")) { double d = 40; if (sscanf(t, "%lf:%lf", &gunAt, &d) >= 1) gunD = d; else gunAt = -1; } }
                if (gunAt >= 0 && !gunDone && now >= gunAt)
                {
                    gunDone = true;
                    float gx = 0, gy = 0, gz = 0;
                    FILE* f = fopen("coop-testgun.txt", "r");
                    if (f && fscanf(f, "%f %f %f", &gx, &gy, &gz) == 3)
                    {
                        const float yaw = atan2f(gy - F(base, rec + 4), gx - F(base, rec + 0)) * 57.29578f;
                        Go g{}; g.at = now; g.n = 6; g.v[0] = gx; g.v[1] = gy; g.v[2] = gz; g.v[3] = float(gunD); g.v[4] = gz; g.v[5] = yaw;
                        gos.push_back(g);
                        printf("coop: %s: test: the newest gun near the joining player is at %.0f %.0f %.0f\n", IsHost() ? "host" : "join", gx, gy, gz); fflush(stdout);
                    }
                    else { printf("coop: %s: test: no gun written to coop-testgun.txt\n", IsHost() ? "host" : "join"); fflush(stdout); }
                    if (f) fclose(f);
                }
            }
            for (Go& g : gos)
            {
                if (g.done || now < g.at) continue;
                g.done = true;
                float p[3] = { g.v[0], g.v[1], g.v[2] };
                float aim[3] = {}; bool aimed = false;
                if (g.n >= 5)
                {
                    // (0.5.913) five numbers: x,y,z the point to look at, d how far short of it, the last the feet's
                    // height there: put d units short along my heading, and turned to look straight at the point
                    // (the game's own setplayerangles, sub_82540168)
                    // (0.5.932) a sixth number: the heading to stand along (else my own)
                    const float yaw = (g.n >= 6 ? g.v[5] : F(base, rec + 176)) * 0.0174533f;
                    p[0] = g.v[0] - g.v[3] * cosf(yaw); p[1] = g.v[1] - g.v[3] * sinf(yaw); p[2] = g.v[4];
                    const float dx = g.v[0] - p[0], dy = g.v[1] - p[1], dz = g.v[2] - (p[2] + 60.0f);
                    aim[0] = -atan2f(dz, sqrtf(dx * dx + dy * dy)) * 57.29578f; aim[1] = atan2f(dy, dx) * 57.29578f; aimed = true;
                }
                else if (g.n >= 4)
                {
                    const float yaw = F(base, rec + 176) * 0.0174533f;
                    p[0] -= g.v[3] * cosf(yaw); p[1] -= g.v[3] * sinf(yaw);
                }
                for (int k = 0; k < 3; k++) { SetF(base, rec + 4 * k, p[k]); SetF(base, rec + 16 + 4 * k, 0.0f); SetF(base, me + 32 + 4 * k, p[k]); SetF(base, me + 336 + 4 * k, p[k]); }
                if (aimed)
                {
                    const uint32_t s = Scratch(base);
                    for (int k = 0; k < 3; k++) SetF(base, s + 4 * k, aim[k]);
                    PPCContext c = CallCtx(ctx);
                    c.r3.u64 = me; c.r4.u64 = s;
                    CallGuest(c, base, 0x82540168u);
                }
                printf("coop: %s: test: put at %.0f %.0f %.0f (yaw %.0f%s, pitch %.0f)\n", IsHost() ? "host" : "join", p[0], p[1], p[2], F(base, rec + 176), aimed ? " set" : "", aim[0]); fflush(stdout);
            }
    }

    // (0.5.977) a test: COD3_COOP_TESTAIMMODEL="230-256:helmet_us_objctv": in those seconds my player is turned four times a
    // second to the nearest thing (within 3000) whose model's name holds the text; one aimed at for 2 s is taken as done
    // and the next nearest follows (Saint-Lo's training helmets: they move when the step begins, t976a).
    void TestAimModel(PPCContext& ctx, uint8_t* base, uint32_t me)
    {
        (void)ctx;
        static double from = -1, to = -1; static std::string want; static bool parsed = false;
        if (!parsed)
        {
            parsed = true;
            if (const char* t = getenv("COD3_COOP_TESTAIMMODEL")) { char w[128] = {}; if (sscanf(t, "%lf-%lf:%127s", &from, &to, w) == 3) want = w; }
        }
        const double now = Now();
        if (want.empty() || now < from || now > to || !me) return;
        const uint32_t rec = Record(base, me);
        if (!rec) return;
        // (0.5.979) STEERED LIKE A PLAYER: the right stick, from the error between the angles wanted and the record's own
        // view (+172 pitch, +176 yaw), sights down the whole time, a shot only when settled within 0.4 degrees. The game's
        // setplayerangles (0.5.977) moved the record's angles but not the host's drawn view (t978b: the view never turned).
        static std::set<uint32_t> done;
        static uint32_t target = 0; static double targetSince = 0, lastShot = 0; static int settled = 0, shots = 0;
        const float eye[3] = { F(base, me + 336), F(base, me + 340), F(base, me + 344) + 60.0f };
        auto isOne = [&](uint32_t e) { return e && !(Guest::Read32(base, e + 632) & 0x400u) && ResName(base, ModelSlot(base, e, 0)).find(want) != std::string::npos; };
        if (target && (!isOne(target) || now - targetSince > 6.0))
        {
            printf("coop: test: done with %08X after %d shots (%s)\n", target, shots, isOne(target) ? "timed out" : "its model changed: hit"); fflush(stdout);
            done.insert(target); target = 0;
        }
        if (!target)
        {
            float bestD = 3000.0f;
            for (uint32_t i = 1; i < 4096; i++)
            {
                const uint32_t e = EntityAt(base, i);
                if (!e || e == me || done.count(e) || !isOne(e)) continue;
                const float dx = F(base, e + 336) - eye[0], dy = F(base, e + 340) - eye[1], dz = F(base, e + 344) - eye[2];
                const float d = sqrtf(dx * dx + dy * dy + dz * dz);
                if (d < bestD) { bestD = d; target = e; }
            }
            if (!target) return;
            targetSince = now; settled = 0; shots = 0;
            printf("coop: test: aiming at %08X (%s) %.0f away, at %.0f %.0f %.0f\n", target, want.c_str(), bestD, F(base, target + 336), F(base, target + 340), F(base, target + 344)); fflush(stdout);
        }
        const float dx = F(base, target + 336) - eye[0], dy = F(base, target + 340) - eye[1], dz = F(base, target + 344) + 4.0f - eye[2];
        const float wantPitch = -atan2f(dz, sqrtf(dx * dx + dy * dy)) * 57.29578f, wantYaw = atan2f(dy, dx) * 57.29578f;
        float ey = wantYaw - F(base, rec + 176); while (ey > 180.0f) ey -= 360.0f; while (ey < -180.0f) ey += 360.0f;
        const float ep = wantPitch - F(base, rec + 172);
        auto stick = [](float err) { float v = -err / 6.0f; if (fabsf(err) > 0.4f && fabsf(v) < 0.3f) v = v < 0 ? -0.3f : 0.3f; if (fabsf(err) <= 0.4f) v = 0;   /* 0.2 is inside the game's dead zone (t980a) */ return std::clamp(v, -1.0f, 1.0f); };
        Input::SetLook(stick(ey), stick(ep), 60);   // stick up looks up (pitch falls): t979a went down with the sign flipped
        Input::PullTriggers(300, 0);
        settled = (fabsf(ey) < 0.6f && fabsf(ep) < 0.6f) ? settled + 1 : 0;
        if (settled >= 3 && now - lastShot > 0.6)
        {
            Input::PullTriggers(0, 90); lastShot = now; shots++;
            // (0.5.982) COD3_COOP_TESTHITMODEL=1: each settled shot also hits the target through the game's own G_Damage
            // (sub_82564888: target, inflictor, attacker, dir, point, damage, flags, means; 2 = MOD_RIFLE_BULLET), my
            // player the attacker. t981a: 44 settled shots at Saint-Lo's helmets and none fell (the record's angles are
            // not exactly the drawn view); this lets a hands-off run past the training's one shooting step.
            static const bool hit = getenv("COD3_COOP_TESTHITMODEL") != nullptr;
            if (hit)
            {
                PPCContext c = CallCtx(ctx);
                c.r3.u64 = target; c.r4.u64 = me; c.r5.u64 = me; c.r6.u64 = 0; c.r7.u64 = 0; c.r8.u64 = 50; c.r9.u64 = 0; c.r10.u64 = 2;
                CallGuest(c, base, 0x82564888u);
                static int toldHit = 0;
                if (toldHit++ < 30) { printf("coop: test: %08X hit through G_Damage (50, rifle bullet)\n", target); fflush(stdout); }
            }
        }
        static int toldAngles = 0;
        if (toldAngles++ % 30 == 0 && toldAngles < 900) { printf("coop: test: aim error yaw %.1f pitch %.1f (view %.1f %.1f)\n", ey, ep, F(base, rec + 176), F(base, rec + 172)); fflush(stdout); }
    }

    // (0.5.923) a test: COD3_COOP_TESTAIM="150-200": in those seconds of this process my player is turned four times a
    // second to look at the nearest enemy soldier drawn here (an axis soldier of the host's, within 2500 units), at
    // his chest. Hands-off tests of the joining player's shots on the host's soldiers (with COD3_PAD's trigger).
    void TestAim(PPCContext& ctx, uint8_t* base, uint32_t me, uint32_t rec)
    {
        static double from = -1, to = -1; static bool parsed = false;
        if (!parsed) { parsed = true; if (const char* t = getenv("COD3_COOP_TESTAIM")) sscanf(t, "%lf-%lf", &from, &to); }
        const double now = Now();
        if (from < 0 || now < from || now > to) return;
        static double at = 0;
        if (now - at < 0.25) return;
        at = now;
        const float eye[3] = { F(base, me + 336), F(base, me + 340), F(base, me + 344) + 60.0f };
        uint32_t best = 0; float bestD = 6000.0f; uint16_t bestIdx = 0;
        for (const auto& [idx, r] : g_remote)
        {
            if (!r.local || r.type != 11) continue;
            const std::string* cls = r.cls ? Name(r.cls) : nullptr;
            if (!cls || cls->find("axis") == std::string::npos) continue;
            if (Guest::Read32(base, r.local + 632) & 0x400u) continue;
            const float dx = F(base, r.local + 336) - eye[0], dy = F(base, r.local + 340) - eye[1];
            const float d = sqrtf(dx * dx + dy * dy);
            if (d < bestD) { bestD = d; best = r.local; bestIdx = idx; }
        }
        if (!best)
        {
            static int toldNone = 0;
            if (toldNone++ < 3)
            {
                std::string w; int n = 0;
                for (const auto& [idx, r] : g_remote) { if (r.type != 11 || n++ >= 12) continue; const std::string* c = r.cls ? Name(r.cls) : nullptr; char b[160]; snprintf(b, sizeof b, " %u(%s, local %08X%s)", idx, c ? c->c_str() : "?", r.local, r.local && (Guest::Read32(base, r.local + 632) & 0x400u) ? " hidden" : ""); w += b; }
                printf("coop: join: test: no enemy to aim at; the host's soldiers heard (%d):%s\n", n, w.c_str()); fflush(stdout);
            }
            return;
        }
        const float dx = F(base, best + 336) - eye[0], dy = F(base, best + 340) - eye[1], dz = F(base, best + 344) + 45.0f - eye[2];
        const uint32_t s = Scratch(base);
        SetF(base, s, -atan2f(dz, sqrtf(dx * dx + dy * dy)) * 57.29578f); SetF(base, s + 4, atan2f(dy, dx) * 57.29578f); SetF(base, s + 8, 0.0f);
        PPCContext c = CallCtx(ctx);
        c.r3.u64 = me; c.r4.u64 = s;
        CallGuest(c, base, 0x82540168u);
        static uint16_t toldFor = 0xFFFF; static int told = 0;
        if (bestIdx != toldFor && told++ < 40) { toldFor = bestIdx; printf("coop: join: test: aiming at the host's soldier %u, %.0f away\n", bestIdx, bestD); fflush(stdout); }
        (void)rec;
    }

    void JoinFrame(PPCContext& ctx, uint8_t* base)
    {
        g_joinFrames = g_joinFrames + 1;
        // (v46) a checkpoint reload's new copy, frozen beside the host for a second: shown, and the old one ends
        if (g_frozen && SwapPending() && Now() - g_frozenAt > 1.0) SwapReveal("back in beside the host");
        // (v46) the reload asked for and the level not yet loading: our frozen world waits, untouched; the load is
        // asked again every 6 s, and after three times this program starts again from nothing as before
        if (g_vmHold)
        {
            if (Now() - g_reloadAskedAt > 6.0)
            {
                if (g_reloadTries >= 3) { printf("coop: join: the reload in this process never began: started again from nothing instead\n"); fflush(stdout); g_vmHold = false; ReloadMission(); return; }
                g_reloadTries++; g_reloadAskedAt = Now();
                printf("coop: join: the reload has not begun: asked again (%d)\n", g_reloadTries); fflush(stdout);
                Kernel::QueueConsoleCommand("spmap " + MyMap());
            }
            return;
        }
        if (g_reloadInGameAt >= 0 && g_frozen && g_frozenAt > g_reloadInGameAt)
        {
            printf("coop: join: back in beside the host %.1f s after his checkpoint, in this process\n", Now() - g_reloadInGameAt); fflush(stdout);
            g_reloadInGameAt = -1;
        }
        const double now = Now();
        const uint32_t me = Me(base);
        const uint32_t rec = Record(base, me);
        if (!me || !rec) { g_inSince = -1; return; }
        if (g_inSince < 0) { g_inSince = now; printf("coop: join: in the mission %s\n", MyMap().c_str()); fflush(stdout); }
        if (g_frozen) { TestGo(ctx, base, me, rec); TestAim(ctx, base, me, rec); TestLive(base, me); TestAimModel(ctx, base, me); TestViewScan(base, me); }
        // my command, view and place, each frame
        {
            struct { uint32_t seq; uint8_t cmd[48]; float view[3]; float origin[3]; float velocity[3]; uint32_t stance; uint32_t flags; char map[32]; } m{};
            m.seq = ++g_seq;
            memcpy(m.cmd, base + CommandSlot(base), 48);
            for (int k = 0; k < 3; k++) { m.view[k] = F(base, rec + 172 + 4 * k); m.origin[k] = F(base, rec + 4 * k); m.velocity[k] = F(base, rec + 16 + 4 * k); }
            m.stance = Guest::Read32(base, rec + 208);
            // (0.5.869) bits 8..15 my view height (60 standing, 40 crouched, 11 prone), 0x10000: it is there
            m.flags = 1 | (g_frozen ? 2 : 0) | 0x10000u | (uint32_t(std::clamp(int32_t(Guest::Read32(base, rec + 184)), 0, 255)) << 8);
            { const uint32_t w = Guest::Read32(base, rec + 132); if (w >= 1 && w <= 160) m.flags |= w << 24; }   // (0.5.896) the gun I carry: its number in the weapon list
            // (0.5.875) 0x20000: my Use is held (pad X; the keyboard's Use and Reload are X too): the revive
            if (Input::Buttons() & 0x4000) m.flags |= 0x20000u;
            // (0.5.882) what I do, from my own record's buttons: 0x40000 firing, 0x80000 the grenade button, 0x100000 aiming
            // (0.5.884) from my record: firing (state 3 with the trigger), the throw (button 2 or states 12..16), aiming,
            // reloading (5..9), the melee (10, 11)
            { const uint32_t b = Guest::Read32(base, rec + 1352), ws = Guest::Read32(base, rec + 136); if (ws == 3 && (b & 1)) m.flags |= 0x40000u; if ((b & ThrowButtons) || ws == 15) m.flags |= 0x80000u; if (b & 8) m.flags |= 0x100000u; if ((ws >= 5 && ws <= 9) || ws == 14) m.flags |= 0x200000u;   /* (0.5.894) 14 is the reload's start, not a throw */ static uint32_t before = 0; if (MeleeState(ws, before)) m.flags |= 0x400000u; }
            {
                static int32_t wasH = -1; static uint32_t wasF = 0; static int told = 0;
                const int32_t hh = int32_t(Guest::Read32(base, rec + 184));
                if ((abs(hh - wasH) > 8 || ((m.stance ^ wasF) & 0x20)) && told < 60) { told++; printf("coop: join: my stance: view height %d, flags %08X\n", hh, m.stance); fflush(stdout); wasH = hh; wasF = m.stance; }
            }
            strncpy(m.map, MyMap().c_str(), 31);
            SendUnreliable(K_CMD, &m, sizeof m);
        }
        // the freeze: once in for a second and the host's world is coming
        // (not while the host is in a scripted scene, such as the level's intro: ours plays out too; 60 s at most)
        // (nor while our own intro plays: its camera has my player until it ends; 90 s at most)
        const bool ownIntro = g_ownLetterbox || g_ownIntroScreen || Guest::Read32(base, 0x825F51A0) != 0;
        if (!g_frozen && g_firstSnapAt >= 0 && g_havePs && now - g_inSince > 1.0 && ((!(g_hostFlags & 3) && (!ownIntro || g_reloading)) || now - g_inSince > 150.0)) { Freeze(ctx, base); g_reloading = false; }   // (v45m: nor while the host rides a vehicle of the scripts: Falaise's jeep, ours rides along with him till it stops)
        if (!g_frozen) { EventsFrame(ctx, base, false); return; }
        // (v43: the old way marked every thread each frame; on Island the script VM's frame then spun forever
        // moving threads between its lists: the joining copy at 0 fps. Now they are told once, at the freeze,
        // and the VM is not run here from then on: see sub_824BB6F8 below)
        if (VmMode() == 0) KillScripts(base);
        // the host's scripts' dvars (the letterbox, the gun, the HUD), ours
        for (const auto& [name, value, flags] : g_dvarPending)
        {
            PPCContext c = CallCtx(ctx);
            c.r3.u64 = GuestText(base, name); c.r4.u64 = GuestText(base, value); c.r5.u64 = flags;
            CallGuest(c, base, 0x8252B850);
            static int told = 0;
            if (told++ < 60) { printf("coop: join: the host's scripts set %s \"%s\"\n", name.c_str(), value.c_str()); fflush(stdout); }
        }
        g_dvarPending.clear();
        // the host's sounds, speech, music and earthquakes
        EventsFrame(ctx, base, true);
        // our own scripts linked my player to things of our stopped world (the intro's mover): free;
        // while the scripts carry the host's player (an intro, a ride), mine rides beside him
        if (Guest::Read32(base, me + 916))
        {
            PPCContext c = CallCtx(ctx); c.r3.u64 = me; CallGuest(c, base, 0x82540D78);   // G_EntUnlink
            printf("coop: join: my player was linked to our stopped world: freed\n"); fflush(stdout);
        }
        if (g_frozen && rec) InventoryJoinFrame(base, rec);   // (0.5.901) my weapons and rounds are the host's word
        // (v45u) where I last stood on firm ground (still for a second, not falling): fallen out of the world
        // from there (300 below it), I am put back
        if (g_frozen && rec)
        {
            static double stillSince = -1; static float was[3] = {};
            const float o[3] = { F(base, me + 336), F(base, me + 340), F(base, me + 344) };
            const float vz = F(base, rec + 24);
            const bool still = fabsf(vz) < 1.0f && fabsf(o[2] - was[2]) < 0.5f && Guest::Read32(base, rec + 1452) == 0;
            memcpy(was, o, 12);
            if (!still) stillSince = -1; else if (stillSince < 0) stillSince = now;
            else if (now - stillSince > 1.0) { memcpy(g_stable, o, 12); g_stableAt = now; }
            if (g_stableAt >= 0 && o[2] < g_stable[2] - 300.0f && vz < -100.0f)
            {
                const float dx = o[0] - g_stable[0], dy = o[1] - g_stable[1];
                if (dx * dx + dy * dy < 600.0f * 600.0f)
                {
                    printf("coop: join: fallen out of the world (%.0f below where I last stood): put back there\n", g_stable[2] - o[2]); fflush(stdout);
                    for (int k = 0; k < 3; k++) { SetF(base, rec + 4 * k, g_stable[k] + (k == 2 ? 4.0f : 0.0f)); SetF(base, rec + 16 + 4 * k, 0.0f); }
                    for (const uint32_t at : { 32u, 336u }) for (int k = 0; k < 3; k++) SetF(base, me + at + 4 * k, g_stable[k] + (k == 2 ? 4.0f : 0.0f));
                    LinkEntity(ctx, base, me);
                }
            }
        }
        // (v45u) DIAGNOSTIC: fallen far below the host near him across: what of ours is there
        if (g_frozen && g_havePs && now - g_psAt < 1.0)
        {
            const float dx = F(base, me + 336) - g_hostPos[0], dy = F(base, me + 340) - g_hostPos[1], dz = F(base, me + 344) - g_hostPos[2];
            static double toldAt = -100; static int told = 0;
            if (dz < -200.0f && dx * dx + dy * dy < 400.0f * 400.0f && now - toldAt > 15.0 && told < 4)
            {
                toldAt = now; told++;
                printf("coop: join: FALLING: me at %.0f %.0f %.0f (record %.0f %.0f %.0f, velocity %.0f %.0f %.0f, noclip %u), the host at %.0f %.0f %.0f, his flags %X\n",
                    F(base, me + 336), F(base, me + 340), F(base, me + 344), F(base, rec + 0), F(base, rec + 4), F(base, rec + 8), F(base, rec + 16), F(base, rec + 20), F(base, rec + 24), Guest::Read32(base, rec + 1452), g_hostPos[0], g_hostPos[1], g_hostPos[2], g_hostFlags);
                std::set<uint32_t> adoptedNow; for (auto& [i, r] : g_remote) if (r.local && !r.puppet) adoptedNow.insert(r.local);
                for (uint32_t i = 1; i < 4096; i++)
                {
                    const uint32_t e = EntityAt(base, i);
                    if (!e || e == me || base[e] == 0) continue;
                    const std::string m0 = ResName(base, ModelSlot(base, e, 0));
                    const float ex = F(base, e + 336) - g_hostPos[0], ey = F(base, e + 340) - g_hostPos[1], ez = F(base, e + 344) - g_hostPos[2];
                    const bool near = ex * ex + ey * ey < 400.0f * 400.0f && ez > -300.0f && ez < 200.0f;
                    if (!near && m0.rfind("sbmodel", 0) != 0) continue;
                    std::string box; for (uint32_t at = 280; at < 336; at += 4) { char b[16]; snprintf(b, sizeof b, " %.0f", F(base, e + at)); box += b; }
                    printf("coop: join:   %u type %u %s %s at %.0f %.0f %.0f contents %08X flags %08X%s%s%s%s |%s\n", i, base[e], BrocText(base, e + 564).c_str(), m0.c_str(), ex, ey, ez, Guest::Read32(base, e + 324), Guest::Read32(base, e + 632),
                        g_puppets.count(e) ? " puppet" : "", adoptedNow.count(e) ? " adopted" : "", g_sweepHidden.count(e) ? " swept" : "", g_retired.count(e) ? " retired" : "", box.c_str());
                }
                fflush(stdout);
            }
        }
        // (0.5.961) the host's travel: speed and heading from his place's changes (one per host message), smoothed
        static float rideVel[2] = {}; static float travel = 0; static bool haveTravel = false; static double fastSince = -1, slowSince = -1, stillSince = -1; static float stopPos[3] = {}; static bool stopped = false;
        {
            static float lastPos[3] = {}; static double lastAt = -1;
            if (g_havePs && g_psAt != lastAt)
            {
                if (lastAt > 0 && g_psAt - lastAt > 0.001 && g_psAt - lastAt < 0.5)
                {
                    const float dt = float(g_psAt - lastAt);
                    const float vx = (g_hostPos[0] - lastPos[0]) / dt, vy = (g_hostPos[1] - lastPos[1]) / dt;
                    const float raw = sqrtf(vx * vx + vy * vy);
                    if (raw > 3000.0f) { rideVel[0] = rideVel[1] = 0; fastSince = -1; }   // a jump (a checkpoint, a comeback): no travel
                    else
                    {
                        rideVel[0] = 0.6f * rideVel[0] + 0.4f * vx; rideVel[1] = 0.6f * rideVel[1] + 0.4f * vy;
                        // (0.5.990) fast = the smoothed speed too, not one reading: the readings' times are when the host's
                        // messages ARRIVED, and two that come close together make a sprinting host (~285) or a stumble
                        // read over 300 once. One such reading used to start a ride that held until he stood still.
                        const float smooth = sqrtf(rideVel[0] * rideVel[0] + rideVel[1] * rideVel[1]);
                        if (raw > 300.0f && smooth > 300.0f) { if (fastSince < 0) fastSince = now; slowSince = -1; stopped = false; stillSince = -1; }
                        else
                        {
                            // not fast for the whole first second: no ride (a ride under way is ended by the stop below)
                            if (fastSince >= 0 && now - fastSince <= 1.0) fastSince = -1;
                            if (slowSince < 0) slowSince = now;
                            // the stop's place: where he was when he last rolled, fixed once still for 0.5 s
                            if (!stopped) { if (raw > 30.0f) { memcpy(stopPos, g_hostPos, 12); stillSince = -1; } else { if (stillSince < 0) stillSince = now; if (now - stillSince > 0.5) stopped = true; } }
                        }
                        if (rideVel[0] * rideVel[0] + rideVel[1] * rideVel[1] > 40.0f * 40.0f)
                        {
                            const float want = atan2f(rideVel[1], rideVel[0]);
                            if (!haveTravel) { travel = want; haveTravel = true; }
                            float d = want - travel; while (d > 3.14159f) d -= 6.28318f; while (d < -3.14159f) d += 6.28318f;
                            travel += 0.2f * d;
                        }
                    }
                }
                else { rideVel[0] = rideVel[1] = 0; }
                memcpy(lastPos, g_hostPos, 12); lastAt = g_psAt;
            }
        }
        if (g_catchUpHost && g_frozen && g_havePs && now - g_psAt < 1.0)
        {
            g_catchUpHost = false;
            const float o[3] = { g_hostPos[0], g_hostPos[1], g_hostPos[2] + 2.0f };
            for (int k = 0; k < 3; k++) { SetF(base, rec + 4 * k, o[k]); SetF(base, rec + 16 + 4 * k, 0.0f); SetF(base, me + 336 + 4 * k, o[k]); SetF(base, me + 32 + 4 * k, o[k]); }
            LinkEntity(ctx, base, me);
            printf("coop: join: I rode one of our vehicles at the freeze: put where the host stands (%.0f %.0f %.0f)\n", o[0], o[1], o[2]); fflush(stdout);
        }
        // riding: the host carried by the scripts (a truck ride), or (0.5.961) moving at a vehicle's speed (over 300 a
        // second, faster than any run) for a second: t961a, Falaise's jeep with the host on its gun, the joining player
        // was left at the start
        const bool hostCarried = (g_hostFlags & 2) && g_havePs && now - g_psAt < 1.0;
        // (0.5.963) a driven ride ends only when the host has stepped 64 away from where the vehicle stopped (off its gun,
        // out of it): before, it ended 2 s after the stop, with him (and so the joining player) still in the vehicle
        if (fastSince >= 0 && stopped)
        {
            const float sx = g_hostPos[0] - stopPos[0], sy = g_hostPos[1] - stopPos[1];
            if (sx * sx + sy * sy > 64.0f * 64.0f) { fastSince = -1; slowSince = -1; stopped = false; stillSince = -1; }
        }
        const bool hostDriven = fastSince >= 0 && now - fastSince > 1.0 && g_havePs && now - g_psAt < 1.0;
        const bool rideNow = hostCarried || hostDriven;
        // (v45j) riding beside the host, mine passes through things (the record's +1452, the game's own noclip): set
        // down each frame beside him inside the vehicle's own model, his movement was pushed out of it again and again
        {
            static bool ours = false;
            if (rideNow && !ours) { ours = true; Guest::Write32(base, rec + 1452, 1); printf("coop: join: riding beside the host (%s): through things\n", hostCarried ? "carried by the scripts" : "at a vehicle's speed"); fflush(stdout); }
            else if (!rideNow && ours)
            {
                // (0.5.963) off the ride on the HOST's own spot (the user: at the vehicle's stop "the joiner is stuck in
                // the vehicle"): set down beside him inside the vehicle's model, he was left in it with his collision back.
                // The host stands where the scripts put him off it; the two players do not block each other.
                ours = false;
                if (g_havePs && now - g_psAt < 1.0)
                {
                    const float o[3] = { g_hostPos[0], g_hostPos[1], g_hostPos[2] + 2.0f };
                    for (int k = 0; k < 3; k++) { SetF(base, rec + 4 * k, o[k]); SetF(base, rec + 16 + 4 * k, 0.0f); SetF(base, me + 336 + 4 * k, o[k]); SetF(base, me + 32 + 4 * k, o[k]); }
                    LinkEntity(ctx, base, me);
                    printf("coop: join: off the ride: put where the host stands (%.0f %.0f %.0f)\n", o[0], o[1], o[2]); fflush(stdout);
                }
                Guest::Write32(base, rec + 1452, 0);
            }
        }
        // (0.5.971) after a scripted passage of the host's (Saint-Lo's wall hop and the struggle behind it, t959u): the
        // scripts carried HIM over the wall and the joining player stayed behind it, with no way across (the hop is a
        // scripted action, only the host's machine's own player can play it). When the host's scene ends (K_PS flag 1
        // down for a second) having moved him 150 or more, and I am 80 or more from him, I am put beside him.
        // (0.5.991: was 350. t990a/d/e: put on the host's spot MID-hop (on the wall), the joining player dropped back
        // on the field side, 171 from him with the wall between: never caught up. The user saw him stuck there.)
        {
            static bool inScene = false; static float sceneFrom[3] = {}; static double offSince = -1;
            const bool fresh = g_havePs && now - g_psAt < 1.0;
            const bool sceneNow = fresh && (g_hostFlags & 1);
            if (sceneNow) { if (!inScene) { inScene = true; memcpy(sceneFrom, g_hostPos, 12); } offSince = -1; }
            else if (inScene && fresh)
            {
                if (offSince < 0) offSince = now;
                else if (now - offSince > 1.0)
                {
                    inScene = false; offSince = -1;
                    const float mx = g_hostPos[0] - sceneFrom[0], my = g_hostPos[1] - sceneFrom[1], mz = g_hostPos[2] - sceneFrom[2];
                    const float jx = F(base, me + 336) - g_hostPos[0], jy = F(base, me + 340) - g_hostPos[1], jz = F(base, me + 344) - g_hostPos[2];
                    const float moved = sqrtf(mx * mx + my * my + mz * mz), apart = sqrtf(jx * jx + jy * jy + jz * jz);
                    static int told = 0;
                    if (told++ < 40) { printf("coop: join: the host's scene ended: it moved him %.0f, I am %.0f from him\n", moved, apart); fflush(stdout); }
                    if (g_frozen && !rideNow && moved >= 150.0f && apart >= 80.0f && Guest::Read32(base, rec + 1452) == 0)
                    {
                        // on his own spot, as off a ride (0.5.963): beside it could be inside the wall just crossed; the two
                        // players do not block each other
                        const float o[3] = { g_hostPos[0], g_hostPos[1], g_hostPos[2] + 2.0f };
                        for (int k = 0; k < 3; k++) { SetF(base, rec + 4 * k, o[k]); SetF(base, rec + 16 + 4 * k, 0.0f); SetF(base, me + 336 + 4 * k, o[k]); SetF(base, me + 32 + 4 * k, o[k]); }
                        LinkEntity(ctx, base, me);
                        printf("coop: join: left behind by the host's scripted passage: put beside him (%.0f %.0f %.0f)\n", o[0], o[1], o[2]); fflush(stdout);
                    }
                }
            }
        }
        if (rideNow)
        {
            // (0.5.961) a SEAT, not an orbit. The user (t959u): in the truck the joining player "is bouncing around and
            // hasn't taken a proper seat". The side was taken from the host's VIEW yaw (every look round swung him about
            // the host) and the host's place came in steps. Now: the side of the way the host TRAVELS (smoothed; kept
            // while stopped), and the host's place carried on between his words by his speed (0.15 s at most).
            const float ahead = float(std::min(0.15, now - g_psAt));
            const float hx = g_hostPos[0] + rideVel[0] * ahead, hy = g_hostPos[1] + rideVel[1] * ahead;
            const float yaw = haveTravel ? travel : g_hostYaw * 0.0174533f;
            // right of the way of travel: (sin, -cos) of the heading, as before
            const float o[3] = { hx + 36.0f * sinf(yaw), hy - 36.0f * cosf(yaw), g_hostPos[2] };
            for (int k = 0; k < 3; k++) { SetF(base, rec + 4 * k, o[k]); SetF(base, rec + 16 + 4 * k, 0.0f); SetF(base, me + 336 + 4 * k, o[k]); SetF(base, me + 32 + 4 * k, o[k]); }
            LinkEntity(ctx, base, me);
            static bool told = false;
            if (!told) { told = true; printf("coop: join: the host's player is carried by the scripts: mine rides beside him\n"); fflush(stdout); }
        }
        // config strings and server commands from the host, given to our own server
        // a few a frame: each is a reliable command for our own client, and its buffer holds only so many
        // before our client has acknowledged them ("Reliable command buffer overflow": the game stopped,
        // the joining copy at 0 fps, when the host's whole set came at once)
        {
            // (v44k) and only those that change something: the host sends all of them again at each
            // checkpoint, and each one given to our server is a reliable command whose text fills our
            // client's command pool; full, the pool was cleared under commands still to be run ("reliable
            // command" warnings by the thousand on Island, then our client dropped and spawned again:
            // the joining copy froze in the world's links). An unchanged one costs nothing now.
            // (v44n) and one config string at most twice a second: an objective's marker that follows a
            // moving thing (Island's tank) changes every frame, and each change was a reliable command
            int budget = 6;
            size_t used = 0;
            static double csAt[1024] = {};
            std::vector<std::pair<uint16_t, std::string>> later;
            for (; used < g_csPending.size() && budget > 0; used++)
            {
                const uint16_t csIndex = g_csPending[used].first;
                if (csIndex < 1024 && BrocText(base, 0x82A568E0 + 16 + 4u * csIndex) == g_csPending[used].second) continue;
                if (csIndex < 1024 && now - csAt[csIndex] < 0.5) { later.push_back(std::move(g_csPending[used])); continue; }
                if (csIndex < 1024) csAt[csIndex] = now;
                budget--;
                { static std::map<uint16_t, int> per; static int told = 0; static double sumAt = 0; per[csIndex]++;
                  if (told++ < 15) { printf("coop: join: config string %u from the host: \"%s\"\n", csIndex, g_csPending[used].second.substr(0, 80).c_str()); fflush(stdout); }
                  if (Now() - sumAt > 20) { sumAt = Now(); std::string t; int n = 0; for (auto& [k, v] : per) if (v > 3 && n++ < 12) t += " " + std::to_string(k) + "x" + std::to_string(v); printf("coop: join: config strings given most:%s\n", t.c_str()); fflush(stdout); } }
                PPCContext c = CallCtx(ctx);
                c.r3.u64 = g_csPending[used].first; c.r4.u64 = GuestText(base, g_csPending[used].second);
                CallGuest(c, base, 0x82513C98);
            }
            g_csPending.erase(g_csPending.begin(), g_csPending.begin() + long(used));
            for (auto& l : later) { bool have = false; for (auto& [i, t] : g_csPending) if (i == l.first) { have = true; break; } if (!have) g_csPending.push_back(std::move(l)); }
            used = 0;
            for (; used < g_cmdPending.size() && budget > 0 && !g_cmdPending[used].empty(); used++, budget--)
            {
                PPCContext c = CallCtx(ctx);
                c.r3.u64 = 0; c.r4.u64 = GuestText(base, "%s"); c.r5.u64 = GuestText(base, g_cmdPending[used]);
                __imp__sub_82513628(c, base);
            }
            g_cmdPending.erase(g_cmdPending.begin(), g_cmdPending.begin() + long(used));
            static double toldAt = -10;
            if (g_csPending.size() + g_cmdPending.size() > 200 && Now() - toldAt > 10) { toldAt = Now(); printf("coop: join: %zu of the host's config strings and commands waiting their turn\n", g_csPending.size() + g_cmdPending.size()); fflush(stdout); }
        }
        for (int i = 0; i < 16; i++) if (g_hudHave[i]) memcpy(base + HudElems + 124u * i, g_hud[i], 124);
        // our soldiers that came after the freeze (a trigger's spawn) and our own grenades and
        // missiles (the joining player's: the host's game throws his for real; we see that one): gone
        {
            static double sweepAt = 0;
            const bool soldiers = now - sweepAt > 0.5;
            if (soldiers) sweepAt = now;
            for (uint32_t i = 2; i < 4096; i++)
            {
                const uint32_t e = EntityAt(base, i);
                if (!e || e == me || g_puppets.count(e)) continue;
                if ((base[e] == 3 || base[e] == 10) && (Guest::Read32(base, e + 632) & 0x400u)) continue;
                if (base[e] == 3 || (soldiers && (base[e] == 11 || (base[e] == 10 && !KeptGun(base, e))))) Remove(ctx, base, i);
                // (v43m) our own items (weapons, pickups) are left to the game, only hidden: deleted, their own
                // think went on linking them as they were freed and the world's links broke (v43l)
                else if (base[e] == 2 && !(Guest::Read32(base, e + 632) & 0x400u)) Guest::Write32(base, e + 632, Guest::Read32(base, e + 632) | 0x400u);
            }
        }
        // the host's entities
        const double show = ShowTime();
        std::set<uint32_t> adopted;
        for (auto it = g_remote.begin(); it != g_remote.end();)
        {
            Remote& r = it->second;
            // (0.5.893) THE OTHER PLAYER GONE AND BACK (the user: he "goes invisible and then pops back"). A thing not
            // heard of for 3 s was let go, by the clock: when the host's copy stood still for 3 s (tests t891a and
            // t892b: his copy at 0 to 1 frames a second for a minute beside the played one) EVERYTHING was let go,
            // the host's body with it, and made again when he came back. A thing is let go when the snapshots that
            // DID come have not named it for 3 s; with no snapshots at all the world stays as it was.
            if (g_lastSnapAt - r.heard > 3.0) { Unbind(ctx, base, r); it = g_remote.erase(it); continue; }
            if (r.local && (EntityAt(base, uint32_t(r.localIndex)) != r.local || GenAt(base, uint32_t(r.localIndex)) != r.localGen || base[r.local] == 0)) { if (r.puppet) g_puppets.erase(r.local); r.local = 0; r.localIndex = -1; r.shownModels.clear(); r.shownAttach.clear(); }
            if (!r.local && !Bind(ctx, base, it->first, r)) { ++it; continue; }
            if (!r.puppet) adopted.insert(r.local);
            if (r.haveModels) SetModels(ctx, base, r);
            if (!r.local) { ++it; continue; }   // (v45e: let go, made again next frame)
            if (r.haveAttach) SetAttach(ctx, base, r);
            if (!r.local) { ++it; continue; }
            if (r.haveFlags)
            {
                uint32_t f = Guest::Read32(base, r.local + 632);
                const bool hide = (r.flags & 0x400u) || (r.extra & 2);
                f = hide ? (f | 0x400u) : (f & ~0x400u);
                Guest::Write32(base, r.local + 632, f);
                // (v45u) a brush model (a door, a bridge, a floor the scripts move): solid here as it is at the
                // host's, where his scripts put it. Puppets were never solid: after the host's checkpoint the
                // joining copy's own was not his (other numbers), and Mayenne's floor was missing: he fell
                // through the world for ever
                if (ResName(base, ModelSlot(base, r.local, 0)).rfind("sbmodel", 0) == 0)
                {
                    const bool solidThere = (r.extra & 1) && !hide, solidHere = Guest::Read32(base, r.local + 324) != 0;
                    if (solidThere != solidHere && r.localIndex >= 0)
                    {
                        PPCContext c = CallCtx(ctx); c.r3.u64 = RefOf(base, uint32_t(r.localIndex));
                        CallGuest(c, base, solidThere ? 0x824A68F8 : 0x824A6820);   // the scripts' solid / notsolid (brush models only)
                        static int told = 0; if (told++ < 30) { printf("coop: join: %s %s of the host's %u (%s): %s\n", r.puppet ? "puppet" : "our", ResName(base, ModelSlot(base, r.local, 0)).c_str(), unsigned(it->first), r.puppet ? "made" : "kept", solidThere ? "solid" : "not solid"); fflush(stdout); }
                    }
                }
            }
            // (0.5.970) THE HOST'S CHANGE OF BODY WITHOUT A FLICKER. The user: seen from the joining player the host's body
            // does "that flicker and tpose animation really fast". A change of gun gives the host another body (231 / 233);
            // t969a (0.5.969's probe, 12 changes): the new one was DRAWN here with nothing on it (pose weight 0.00, a
            // T-pose) on its first frame in 3 of them, and the old one was hidden by the host in the same snapshot (the
            // host gone meanwhile). Now the new body is held hidden till its pose weighs, from the moment the snapshot
            // puts it here, and the old one stays drawn (in its last pose) till then, a second at most.
            {
                static int32_t was = -1, before = -1; static double changedAt = -1; static bool newPosed = true; static int told = 0;
                if (g_hostBodyIndex != was) { before = was; was = g_hostBodyIndex; changedAt = Now(); newPosed = false; }
                if (r.local && (int32_t(it->first) == was || int32_t(it->first) == before))
                {
                    const uint32_t tr = Guest::Read32(base, r.local + 552);
                    float sum = 0;
                    if (Valid(tr)) { const uint32_t lst = Guest::Read32(base, tr + 8); const uint32_t cnt = Valid(lst) ? std::min<uint32_t>(Guest::Read32(base, lst + 4), 4096) : 0, ent = Valid(lst) ? Guest::Read32(base, lst + 8) : 0; for (uint32_t k = 0; k < cnt; k++) { const uint16_t sl = Guest::Read16(base, tr + 24 + 2 * k); if (sl && !Guest::Read16(base, ent + 28 * k + 4)) sum += F(base, InfoPool + 52u * sl + 36); } }
                    const uint32_t f = Guest::Read32(base, r.local + 632);
                    if (int32_t(it->first) == was)
                    {
                        if (sum >= 0.5f) newPosed = true;
                        else if (!(f & 0x400u)) { Guest::Write32(base, r.local + 632, f | 0x400u); if (told < 30) { told++; printf("coop: join: the host's new body %u held hidden: no pose on it yet (+%.3f s)\n", unsigned(it->first), Now() - changedAt); fflush(stdout); } }
                    }
                    else if (!newPosed && Now() - changedAt < 1.0 && sum >= 0.5f && (f & 0x400u))
                    {
                        Guest::Write32(base, r.local + 632, f & ~0x400u);
                        if (told < 30) { told++; printf("coop: join: the host's old body %u kept drawn till the new one has a pose (+%.3f s)\n", unsigned(it->first), Now() - changedAt); fflush(stdout); }
                    }
                }
            }
            if (int32_t(it->first) == g_hostBodyIndex)
            {
                DriveHostBody(ctx, base, r);
                // (0.5.936) a body of the host's is not drawn till an animation of his plays on it: a new one (a change
                // of gun makes one, t935b: "move -1, animation -1") stood a frame or more in its bind pose, arms out
                if (r.local)
                {
                    const uint32_t tr = Guest::Read32(base, r.local + 552);
                    bool bare = !Valid(tr) || g_lb.anim < 0 || !Guest::Read16(base, tr + 24 + 2 * uint32_t(g_lb.anim));
                    // (0.5.970) and nothing weighing on it yet (an animation set but at 0: its first frame, a T-pose)
                    if (!bare)
                    {
                        const uint32_t lst = Guest::Read32(base, tr + 8); const uint32_t cnt = Valid(lst) ? std::min<uint32_t>(Guest::Read32(base, lst + 4), 4096) : 0, ent = Valid(lst) ? Guest::Read32(base, lst + 8) : 0;
                        float sum = 0; for (uint32_t k = 0; k < cnt; k++) { const uint16_t sl = Guest::Read16(base, tr + 24 + 2 * k); if (sl && !Guest::Read16(base, ent + 28 * k + 4)) sum += F(base, InfoPool + 52u * sl + 36); }
                        if (sum < 0.5f) bare = true;
                    }
                    const bool hostHides = (r.flags & 0x400u) || (r.extra & 2);
                    const uint32_t f = Guest::Read32(base, r.local + 632);
                    const uint32_t want = (bare || hostHides) ? (f | 0x400u) : (f & ~0x400u);
                    if (want != f) { Guest::Write32(base, r.local + 632, want); static int told = 0; if (told++ < 20) { printf("coop: join: the host's body %s\n", bare ? "held hidden: no animation of his on it yet" : "drawn: his animation plays"); fflush(stdout); } }
                }
                if (r.local)
                {
                    // (0.5.895) the host's aim up and down: his pitch comes in his body's first angle
                    const uint32_t tree = Guest::Read32(base, r.local + 552);
                    float share = 0.0f;
                    const int32_t tiltAnim = r.track.empty() ? -1 : BodyTilt(ctx, base, tree, g_lb.move, r.track.back().a[0], share);
                    TidyBodyTree(base, tree, g_lb.anim, tiltAnim, share);
                    // (0.5.968) A PROBE: the user sees the host's body "flicker and tpose really fast" now and then. A frame
                    // whose playing animations do not add up to a whole pose (a T-pose share) is logged with what played.
                    if (Valid(tree))
                    {
                        const uint32_t lst = Guest::Read32(base, tree + 8);
                        const uint32_t cnt = Valid(lst) ? std::min<uint32_t>(Guest::Read32(base, lst + 4), 4096) : 0, ent = Valid(lst) ? Guest::Read32(base, lst + 8) : 0;
                        float sum = 0; int leaves = 0; std::string what;
                        for (uint32_t k = 0; k < cnt; k++)
                        {
                            const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * k); if (!sl) continue;
                            if (Guest::Read16(base, ent + 28 * k + 4) != 0) continue;   // a parent node
                            const float w = F(base, InfoPool + 52u * sl + 36);
                            if (w > 0.001f) { sum += w; leaves++; if (what.size() < 120) { char b[40]; snprintf(b, sizeof b, " %u:%.2f", k, w); what += b; } }
                        }
                        static int bad = 0, told = 0; static double windowAt = -1;
                        const bool hidden = (Guest::Read32(base, r.local + 632) & 0x400u) != 0;
                        if (!hidden && sum < 0.9f)
                        {
                            bad++;
                            if (told < 60) { told++; printf("coop: join: THE HOST'S BODY SHORT OF A POSE: %.2f (%d playing:%s), move %d anim %d tilt %d share %.2f\n", sum, leaves, what.c_str(), g_lb.move, g_lb.anim, tiltAnim, share); fflush(stdout); }
                        }
                        if (Now() - windowAt > 30.0) { if (windowAt >= 0 && bad) { printf("coop: join: the host's body short of a pose in %d frames in 30 s\n", bad); fflush(stdout); } bad = 0; windowAt = Now(); }
                    }
                }
            }   // (0.5.890: the blend, after a change of move too)
            else if (r.haveAnim && r.list) SetAnims(ctx, base, r);
            if (!r.local) { ++it; continue; }   // (let go for a fresh puppet)
            if (r.havePlace && !r.track.empty())
            {
                float o[3], a[3];
                Place(r, show, o, a);
                if (int32_t(it->first) == g_hostBodyIndex) a[0] = 0.0f;   // (0.5.895: his pitch, not the body's: it stands upright)
                // (v43l) never linked: an entity the game is freeing (its +16 0x100000), one gone, or a place
                // that is not a number or off the world (the world's area tree walked for ever: v43k, after a restart)
                bool sane = base[r.local] != 0 && !(Guest::Read32(base, r.local + 16) & 0x100000u);
                for (int k = 0; k < 3; k++) if (!std::isfinite(o[k]) || fabsf(o[k]) > 65536.0f || !std::isfinite(a[k])) sane = false;
                if (!sane)
                {
                    static int told = 0;
                    if (told++ < 10) { printf("coop: join: the host's %u (type %u) not placed: its entity is going or its place is bad (%g %g %g)\n", it->first, base[r.local], o[0], o[1], o[2]); fflush(stdout); }
                    if (Guest::Read32(base, r.local + 16) & 0x100000u) { if (r.puppet) g_puppets.erase(r.local); r.local = 0; r.localIndex = -1; }
                    ++it; continue;
                }
                for (const uint32_t at : { 336u, 32u }) for (int k = 0; k < 3; k++) SetF(base, r.local + at + 4 * k, o[k]);
                for (const uint32_t at : { 352u, 72u }) for (int k = 0; k < 3; k++) SetF(base, r.local + at + 4 * k, a[k]);
                LinkEntity(ctx, base, r.local);
            }
            ++it;
        }
        // our level's things the host has no more of (after his first full second): hidden
        if (g_firstSnapAt >= 0 && now - g_firstSnapAt > 2.0)
        {
            static double hideAt = 0;
            if (now - hideAt > 1.0)
            {
                hideAt = now;
                int hidden = 0;
                for (uint32_t i = 2; i < 4096; i++)
                {
                    const uint32_t e = EntityAt(base, i);
                    if (!e || e == me || g_puppets.count(e) || adopted.count(e) || base[e] == 1 || base[e] == 12) continue;
                    if (!ModelSlot(base, e, 0) && !Valid(Guest::Read32(base, e + 928))) continue;
                    if (Guest::Read32(base, e + 632) & 0x400u) continue;
                    Guest::Write32(base, e + 632, Guest::Read32(base, e + 632) | 0x400u);
                    g_sweepHidden.insert(e);
                    // (v45u) and not solid: hidden, it was still there to stand in. Mayenne: the joining player put
                    // beside the host was inside one, and fell through the world for ever
                    // (the scripts' notsolid is for brush models only: a vehicle of ours, Mayenne's truck, kept its
                    // collision. Its contents put to nothing: the world's traces look at them each time)
                    if (base[e] != 11 && Guest::Read32(base, e + 324)) { static int told = 0; if (told++ < 20) { printf("coop: join: our hidden %u (type %u, %s) not solid now (contents were %08X)\n", i, base[e], ResName(base, ModelSlot(base, e, 0)).c_str(), Guest::Read32(base, e + 324)); fflush(stdout); } Guest::Write32(base, e + 324, 0); }
                    hidden++;
                    static int toldThing = 0;
                    if (toldThing++ < 40) { printf("coop: join: hidden: our %u (type %u, %s, %s)\n", i, base[e], BrocText(base, e + 564).c_str(), ResName(base, ModelSlot(base, e, 0)).c_str()); fflush(stdout); }
                }
                static int told = 0;
                if (hidden && told++ < 20) { printf("coop: join: %d of our level's things the host has not: hidden\n", hidden); fflush(stdout); }
            }
        }
        // what the host's player may do (stand, crouch, prone...: the scripts' allow flags), mine too
        if (g_havePs) Guest::Write32(base, rec + 44, (Guest::Read32(base, rec + 44) & ~0x01F00000u) | (g_hostAllow & 0x01F00000u));
        // down (the host's last stand): lying down
        DownStance(base, me, PlayerDown(1));
        // my health is the host's
        if (g_health >= 0 && now - g_psAt < 1.0)
        {
            Guest::Write32(base, me + 776, uint32_t(std::max(g_health, 1)));
            if (g_maxHealth > 0) Guest::Write32(base, me + 780, uint32_t(g_maxHealth));
            Guest::Write32(base, rec + 272, uint32_t(std::max(g_health, 1)));
        }
        static double toldAt = 0;
        if (now - toldAt > 20)
        {
            toldAt = now;
            int bound = 0, puppets = 0; for (auto& [i, r] : g_remote) { if (r.local) bound++; if (r.puppet) puppets++; }
            { int treed = 0, still = 0, untreed = 0; std::string ex;
              for (auto& [i, r] : g_remote) { if (!r.local || !r.puppet || (Guest::Read32(base, r.local + 632) & 0x400u)) continue; if (r.type != 11 && !(r.type == 7 && r.list)) continue;
                const uint32_t tree = Guest::Read32(base, r.local + 552); if (!Valid(tree)) { untreed++; if (ex.size() < 200) ex += " " + std::to_string(i) + "(no tree)"; continue; } treed++;
                const uint32_t cnt = std::min<uint32_t>(Guest::Read32(base, Guest::Read32(base, tree + 8) + 4), 4096); float w = 0;
                for (uint32_t k = 0; k < cnt; k++) { const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * k); if (sl) w += F(base, InfoPool + 52u * sl + 36); }
                if (w < 0.01f) { still++; if (ex.size() < 200) ex += " " + std::to_string(i) + "(" + std::to_string(r.nodes.size()) + " nodes)"; } }
              printf("coop: join: soldier puppets: %d with a tree, %d of them with nothing playing (T-pose), %d with no tree:%s\n", treed, still, untreed, ex.c_str()); }
            { // (v45u) every puppet dressed as a man (a "c_" model) with no tree or nothing on it: his bind pose
              std::string ex; int n = 0;
              for (auto& [i, r] : g_remote) { if (!r.local || !r.puppet || (Guest::Read32(base, r.local + 632) & 0x400u)) continue;
                const std::string m0 = ResName(base, ModelSlot(base, r.local, 0)); if (m0.rfind("c_", 0) != 0) continue;
                const uint32_t tree = Guest::Read32(base, r.local + 552); float w = 0;
                if (Valid(tree)) { const uint32_t cnt = std::min<uint32_t>(Guest::Read32(base, Guest::Read32(base, tree + 8) + 4), 4096);
                  for (uint32_t k = 0; k < cnt; k++) { const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * k); if (sl) w += F(base, InfoPool + 52u * sl + 36); } }
                if (Valid(tree) && w >= 0.01f) continue;
                n++; if (ex.size() < 600) { const std::string* c = Name(r.cls); ex += " " + std::to_string(i) + "(type " + std::to_string(r.type) + " " + (c ? *c : "?") + (Valid(tree) ? " tree" : " no tree") + (r.haveAnim ? " anim" : "") + " list " + std::to_string(r.list) + " nodes " + std::to_string(r.nodes.size()) + (r.held ? " held" : "") + ")"; } }
              if (n) { printf("coop: join: %d puppets dressed as men in their bind pose:%s\n", n, ex.c_str()); fflush(stdout); } }
            { const uint32_t cl0 = Guest::Read32(base, 0x829C39E0 + 8); if (Valid(cl0)) { printf("coop: join: our client's command pool: size %u, base %08X, cursor %08X (used %d)\n", Guest::Read32(base, cl0 + 4), Guest::Read32(base, cl0 + 8), Guest::Read32(base, cl0 + 20), int(Guest::Read32(base, cl0 + 20) - Guest::Read32(base, cl0 + 8))); } }
            int used = 0; for (uint32_t i = 0; i < 4096; i++) if (const uint32_t e = EntityAt(base, i); e && base[e] != 0 && !(Guest::Read32(base, e + 632) & 0x400u)) used++;
            printf("coop: join: the host's world: %zu entities heard, %d drawn (%d puppets, %zu ours adopted), %zu names; my health %d; %d entity slots used, VM threads pending %u\n", g_remote.size(), bound, puppets, adopted.size(), g_names.size(), g_health, used, Guest::Read32(base, ScriptVm + 16));
            fflush(stdout);
        }
    }
}

// (0.5.919) THE JOINING PLAYER'S PROMPTS. sub_82553BB0 (the player's hints: "Press and hold to pick up the M1 Garand")
// finds what this copy's player looks at in THIS copy's world, which is frozen: a prompt here could be for a thing
// the host's world no longer has (the Garand already taken) or miss a new one. Once frozen, the prompt shown is the
// one the host's world has for him (his record +1156, +1160, +1164 there, sent with K_PS), as long as it is fresh.
// COD3_COOP_HOSTHINTS=0: as before.
extern "C" PPC_FUNC(__imp__sub_82553BB0);
PPC_FUNC(sub_82553BB0)
{
    using namespace Coop;
    const uint32_t e = ctx.r3.u32;
    __imp__sub_82553BB0(ctx, base);
    static const bool off = []() { const char* t = getenv("COD3_COOP_HOSTHINTS"); return t && t[0] == '0'; }();
    if (off || !IsJoin() || !g_frozen || !e || e != Me(base) || g_hostHintAt < 0 || Now() - g_hostHintAt > 1.0) return;
    const uint32_t rec = Record(base, e);
    if (!rec) return;
    static uint32_t was[3] = { 1, 1, 1 }; static int told = 0;
    for (int k = 0; k < 3; k++) Guest::Write32(base, rec + 1156 + 4 * k, g_hostHint[k]);
    if (memcmp(was, g_hostHint, sizeof was) != 0 && told < 60) { told++; memcpy(was, g_hostHint, sizeof was); printf("coop: join: my prompt from the host's world: %08X %08X %08X\n", g_hostHint[0], g_hostHint[1], g_hostHint[2]); fflush(stdout); }
}

// (0.5.953) THE NAME OF A GUN ON THE GROUND IN HIS PROMPT. The user (t952u): "swap for null". sub_824ED5A8 builds
// the prompt from the hint (+1156): 12..139 = a gun on the ground, weapon h - 11, "swap <held> for <name>" with the
// name from the client's weapon info 0x82A9A460 + 196 * w, +140, filled when the client registers that weapon
// (sub_82448C78, which the same builder calls itself for 140..267, ammunition). This copy's world froze before it ever
// saw (say) a Gewehr 43, so its entry was empty. A gun the host's hint names is registered here first, once.
extern "C" PPC_FUNC(__imp__sub_824ED5A8);
PPC_FUNC(sub_824ED5A8)
{
    using namespace Coop;
    if (IsJoin() && g_frozen && g_hostHintAt >= 0 && Now() - g_hostHintAt <= 1.0)
    {
        const uint32_t h = g_hostHint[0];
        if (h >= 12 && h <= 139)
        {
            const uint32_t w = h - 11;
            static bool tried[160] = {};
            const uint32_t list = Guest::Read32(base, 0x82A2A2E0u);   // this copy's weapon definitions: w must be one
            if (w < 160 && !tried[w] && Valid(list) && Valid(Guest::Read32(base, list + 4 * w)) && Guest::Read32(base, 0x82A9A460u + 196 * w + 140) == 0)
            {
                tried[w] = true;
                const uint32_t saved = ctx.r3.u32;
                PPCContext c = CallCtx(ctx); c.r3.u64 = w;
                CallGuest(c, base, 0x82448C78u);
                printf("coop: join: weapon %u named in the host's prompt was not known here: registered (its name now %s)\n", w, Valid(Guest::Read32(base, 0x82A9A460u + 196 * w + 140)) ? "there" : "still missing");
                fflush(stdout);
                ctx.r3.u64 = saved;
            }
        }
    }
    __imp__sub_824ED5A8(ctx, base);
}

// the script VM's frame (r3 the VM, 0x82A90128). On the joining copy, once frozen, nothing of its own
// world runs scripts: not run. And never run on a pending list that would keep it in its loop for ever
// (its last entry its own head while it is not empty: the loop at 0x824BB9A0 would take nothing from it).
extern "C" PPC_FUNC(__imp__sub_824BB6F8);
PPC_FUNC(sub_824BB6F8)
{
    using namespace Coop;
    if (GetMode() == Mode::Join && ctx.r3.u32 == ScriptVm)
    {
        if (JoinSkipVm())
        {
            // (v43f) the thread loop is not run, but the VM's own housekeeping is: the objects freed
            // by deletes (a puppet gone, our own soldiers removed) are released there; without it every
            // deleted puppet stayed in the world, still drawn and in its bind pose (v43c: T-posed soldiers
            // piling up where the host's had gone, and the entity slots climbing)
            Guest::Write32(base, ScriptVm + 2136, 0);
            PPCContext c = CallCtx(ctx); c.r3.u64 = ScriptVm;
            CallGuest(c, base, 0x824A5EE8);
            return;
        }
        const uint32_t a = ScriptVm + 16;
        if (Guest::Read32(base, a + 4) != a + 8 && Guest::Read32(base, a + 12) == a + 4)
        {
            static int told = 0;
            if (told++ < 5) { printf("coop: join: the script VM's pending list is broken (first %08X last %08X, %u): the VM not run this frame\n", Guest::Read32(base, a + 4), Guest::Read32(base, a + 12), Guest::Read32(base, a)); fflush(stdout); }
            return;
        }
    }
    __imp__sub_824BB6F8(ctx, base);
}

// (v44) the joining copy is a window onto the host's world, as World at War's (and every id Tech 3
// game's) clients are: they simulate nothing of the world, they draw the server's snapshots. Ours still
// ran its physics world and relinked whatever the physics moved (sub_824139B8, then sub_82550368 over
// the moved entities' list built by sub_8256CB90): its own dropped weapons, grenades and bodies, some
// already freed by the freeze. One of them linked twice broke the world's area tree and the next link
// walked it for ever (v43n, v43p: the joining copy at 0 fps after a death). Frozen, neither runs.
extern "C" PPC_FUNC(__imp__sub_824139B8);
PPC_FUNC(sub_824139B8)
{
    if (Coop::GetMode() == Coop::Mode::Join && Coop::JoinFrozen())
    {
        static bool told = false;
        if (!told) { told = true; printf("coop: join: this copy's physics world stops (the host's moves its things)\n"); fflush(stdout); }
        return;
    }
    __imp__sub_824139B8(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_82550368);
PPC_FUNC(sub_82550368)
{
    if (Coop::GetMode() == Coop::Mode::Join && Coop::JoinFrozen()) return;
    __imp__sub_82550368(ctx, base);
}

// (v44m) the drones' own spawner (sub_82521C70, once a frame from Com_Frame; it makes background soldiers
// as script_models: "max_drones", "drone_animtree", "_drones::name_drone"). The host's drones are drawn
// here from his snapshots; ours kept being made each frame under the frozen world and linked into it,
// and that is where the joining copy froze (v44g, v44k: sub_825214E8 > spawn > link, for ever).
extern "C" PPC_FUNC(__imp__sub_82521C70);
PPC_FUNC(sub_82521C70)
{
    if (Coop::GetMode() == Coop::Mode::Join && Coop::JoinFrozen())
    {
        static bool told = false;
        if (!told) { told = true; printf("coop: join: this copy's drone spawner stops (the host's drones are drawn)\n"); fflush(stdout); }
        return;
    }
    __imp__sub_82521C70(ctx, base);
}

// (v45f) the frame's pose jobs (sub_824F38E8: count at 0x82A2A228, 820-byte jobs from 0x829C91F8: +0 the
// model part (its model at +0, the part at +4), +12 and on the poses to fill, +812 how many). On the joining
// copy some jobs named a model part let go meanwhile (a puppet or one of ours freed, its tree with it): the
// bone count read through it was garbage and the poses were written far past their end (the crash in
// sub_8216E680 on Island and Mayenne). Every job is checked first; one that does not add up is dropped.
extern "C" PPC_FUNC(__imp__sub_824F38E8);
PPC_FUNC(sub_824F38E8)
{
    if (Coop::GetMode() != Coop::Mode::Off)
    {
        using namespace Coop;
        constexpr uint32_t Jobs = 0x829C91F8, Stride = 820, CountAt = 0x82A2A228;
        const int32_t count = int32_t(Guest::Read32(base, CountAt));
        { static int32_t most = 0; static double at = 0; most = std::max(most, count); if (Now() - at > 30) { at = Now(); printf("coop: pose jobs: at most %d a frame in the last 30 s\n", most); fflush(stdout); most = 0; } }
        int32_t kept = 0, dropped = 0;
        for (int32_t i = 0; i < count && i < 150; i++)
        {
            const uint32_t job = Jobs + Stride * uint32_t(i);
            const uint32_t part = Guest::Read32(base, job + 0);
            const uint32_t n = Guest::Read32(base, job + 812);
            bool ok = Valid(part) && n >= 1 && n <= 199;
            uint32_t bones = 0;
            if (ok)
            {
                const uint32_t model = Guest::Read32(base, part), index = Guest::Read32(base, part + 4);
                ok = (Valid(model) || (model >= 0x82000000u && model < 0x83000000u)) && index < 64;
                if (ok) { const uint32_t table = Guest::Read32(base, model + 100); ok = Valid(table) || (table >= 0x82000000u && table < 0x83000000u); if (ok) bones = Guest::Read32(base, table + index * 20); }
                ok = ok && bones >= 1 && bones <= 256;
            }
            for (uint32_t k = 0; ok && k < n; k++) ok = Valid(Guest::Read32(base, job + 12 + 4 * k));
            if (!ok) { dropped++; continue; }
            if (kept != i) memmove(base + Jobs + Stride * uint32_t(kept), base + job, Stride);
            kept++;
        }
        if (dropped)
        {
            Guest::Write32(base, CountAt, uint32_t(kept));
            static int told = 0;
            if (told++ < 20) { printf("coop: %d of the frame's %d pose jobs dropped (their model part let go)\n", dropped, count); fflush(stdout); }
        }
        if (count > 150) { static int told = 0; if (told++ < 5) { printf("coop: %d pose jobs this frame: the rest dropped\n", count); fflush(stdout); } Guest::Write32(base, CountAt, uint32_t(kept)); }
    }
    __imp__sub_824F38E8(ctx, base);
}

// (v45q) a pose job queued (sub_824F39A0): the array has no bound of its own. The game queues a dozen or so
// a frame; the joining copy, with its puppets, up to 68, and what lies past the array was written over (the
// world's sector tree among what broke). New jobs past 40 a frame wait for the next frame: that pose is a
// frame late, nothing more.
extern "C" PPC_FUNC(__imp__sub_824F39A0);
PPC_FUNC(sub_824F39A0)
{
    if (Coop::GetMode() != Coop::Mode::Off)
    {
        const int32_t count = int32_t(Guest::Read32(base, 0x82A2A228));
        if (count >= 140)
        {
            // (v45u) what the jobs are, once in a while: their keys and how many poses each
            static double dumpedAt = -100; const double t = Coop::Now();
            if (t - dumpedAt > 30)
            {
                dumpedAt = t; std::string ex; std::map<uint32_t, int> parts;
                for (int32_t i = 0; i < count && i < 150; i++)
                {
                    const uint32_t j = 0x829C91F8 + 820u * uint32_t(i);
                    parts[Guest::Read32(base, j)]++;
                    if (i < 40) { char b[64]; snprintf(b, sizeof b, " %08X/%08X/%u", Guest::Read32(base, j + 8), Guest::Read32(base, j), Guest::Read32(base, j + 812)); ex += b; }
                }
                printf("coop: pose jobs at %d: %zu distinct parts; key/part/poses:%s\n", count, parts.size(), ex.c_str());
                for (int k = 0; k < 3; k++) { const uint32_t key = Guest::Read32(base, 0x829C91F8 + 820u * uint32_t(k) + 8); if (!Coop::Valid(key)) continue; std::string h; for (int w = 0; w < 24; w++) { char b[12]; snprintf(b, sizeof b, " %08X", Guest::Read32(base, key + 4 * w)); h += b; } printf("coop:   key %08X:%s\n", key, h.c_str()); }
                fflush(stdout);
            }
        }
        if (count >= 150)
        {
            // (v45v) the queue full (150 of its 160; the next global of the image is at 0x829E9278 = 160 x 820 on):
            // what it holds is posed now (the frame's own flush, sub_824F38E8, which does nothing else) and it
            // starts again empty (as sub_824F0CD8 empties it each frame). Refused (v45u), the men past 150 stood
            // in their bind pose: the joining copy poses 100..200 models a frame in a fight (every puppet is a
            // script model posed each frame), the host about 40. The queue's index returned is used by no caller.
            static int told = 0; if (told++ < 5) { printf("coop: the pose queue full (%d): posed now, and begun again\n", count); fflush(stdout); }
            PPCContext c = Coop::CallCtx(ctx);
            Coop::CallGuest(c, base, 0x824F38E8);
            Guest::Write32(base, 0x82A2A228, 0);
            static uint32_t flushes = 0; static double toldAt = 0; flushes++;
            if (Coop::Now() - toldAt > 30) { toldAt = Coop::Now(); printf("coop: the pose queue posed early %u times in the last 30 s\n", flushes); fflush(stdout); flushes = 0; }
        }
    }
    __imp__sub_824F39A0(ctx, base);
}

#endif
