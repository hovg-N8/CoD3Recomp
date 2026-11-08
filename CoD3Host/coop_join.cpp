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

#include <algorithm>
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
        uint32_t g_seq = 0;
        int32_t g_health = -1, g_maxHealth = -1;
        uint32_t g_hostFlags = 0, g_hostAllow = 0;
        int32_t g_hostHealth = 0, g_hostMaxHealth = 0, g_hostBodyIndex = -1;   // the host's player (K_PS)
        float g_hostPos[3] = {};
        float g_stable[3] = {}; double g_stableAt = -1;   // (v45u) where my player last stood still on firm ground
        float g_hostYaw = 0;
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
        struct LocalBody { int32_t anim = -1, prev = -1; double blendAt = -1; int move = -1; float last[3] = {}; double at = -1; float speed = 0; uint32_t tree = 0; };
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
            const uint32_t list = Guest::Read32(base, tree + 8);
            auto index = [&](uint32_t hash) -> int32_t {
                const uint32_t count = std::min<uint32_t>(Guest::Read32(base, list + 4), 4096), entries = Guest::Read32(base, list + 8);
                for (uint32_t k = 0; k < count; k++) if (Guest::Read32(base, entries + 28 * k) == hash) return int32_t(k);
                return -1;
            };
            const double now = Now();
            if (r.track.empty()) return;
            const float* o = r.track.back().o;
            if (g_lb.at >= 0 && now > g_lb.at)
            {
                const float dt = float(now - g_lb.at);
                const float v = sqrtf((o[0] - g_lb.last[0]) * (o[0] - g_lb.last[0]) + (o[1] - g_lb.last[1]) * (o[1] - g_lb.last[1])) / std::max(dt, 0.001f);
                g_lb.speed += (std::min(v, 600.0f) - g_lb.speed) * std::min(1.0f, dt * 8.0f);
            }
            const float vx = o[0] - g_lb.last[0], vy = o[1] - g_lb.last[1];
            memcpy(g_lb.last, o, 12); g_lb.at = now;
            const bool prone = (r.extra & 8) != 0, crouch = !prone && (r.extra & 4);
            const float yaw = r.track.back().a[1] * 0.0174533f;
            const float fwd = vx * cosf(yaw) + vy * sinf(yaw), side = -vx * sinf(yaw) + vy * cosf(yaw);
            int move;
            if (g_lb.speed < 20.0f) move = prone ? PRONE_IDLE : crouch ? CROUCH_IDLE : STAND_IDLE;
            else if (prone) move = PRONE_CRAWL;
            else
            {
                const bool f = fabsf(fwd) >= fabsf(side);
                if (crouch) move = f ? (fwd >= 0 ? CROUCH_WALK_F : CROUCH_WALK_B) : (side > 0 ? CROUCH_WALK_L : CROUCH_WALK_R);
                else if (f && fwd >= 0) move = g_lb.speed > 170.0f ? STAND_RUN_F : STAND_WALK_F;
                else move = f ? STAND_WALK_B : (side > 0 ? STAND_WALK_L : STAND_WALK_R);
            }
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
                if (g_lb.prev >= 0 && g_lb.prev != g_lb.anim) ramp(g_lb.prev, 0.0f);
                ramp(g_lb.anim, 1.0f);
            }
            if (move == g_lb.move && g_lb.anim >= 0)
            {
                const uint16_t slot = Guest::Read16(base, tree + 24 + 2 * uint32_t(g_lb.anim));
                if (slot)
                {
                    const uint32_t rec = InfoPool + 52u * slot;
                    if (F(base, rec + 16) >= 0.97f) { SetF(base, rec + 16, 0.0f); SetF(base, rec + 20, 0.0f); }
                    if (F(base, rec + 32) < 0.99f) SetF(base, rec + 32, 1.0f);
                    return;
                }
                g_lb.anim = -1;
            }
            int32_t anim = index(MoveHash[move]);
            if (anim < 0) anim = index(MoveHash[STAND_IDLE]);
            if (anim < 0) { static int told2 = 0; if (told2++ < 3) { printf("coop: join: the host's body: none of the moves in its list\n"); fflush(stdout); } if (r.haveAnim && r.list) SetAnims(ctx, base, r); return; }
            if (anim != g_lb.anim && AnimJobRoom(base))
            {
                PPCContext c = CallCtx(ctx);
                c.r3.u64 = tree; c.r4.u64 = uint32_t(anim); c.f1.f64 = 1.0; c.f2.f64 = 0.2; c.f3.f64 = move == STAND_RUN_F ? 1.15 : 1.0; c.r8.u64 = 0; c.r10.u64 = 0;
                CallGuest(c, base, 0x82500A40);   // set anim
                if (g_lb.anim >= 0)
                {
                    c = CallCtx(ctx);
                    c.r3.u64 = tree; c.r4.u64 = uint32_t(g_lb.anim); c.f1.f64 = 0.0; c.f2.f64 = 0.2; c.f3.f64 = 1.0; c.r8.u64 = 0; c.r10.u64 = 0;
                    CallGuest(c, base, 0x82500A40);
                }
                if (g_lb.prev >= 0 && g_lb.prev != anim && g_lb.prev != g_lb.anim) { const uint16_t sl = Guest::Read16(base, tree + 24 + 2 * uint32_t(g_lb.prev)); if (sl) { SetF(base, InfoPool + 52u * sl + 36, 0.0f); SetF(base, InfoPool + 52u * sl + 32, 0.0f); } }
                g_lb.prev = g_lb.anim;
                static int told = 0;
                if (told++ < 6) { printf("coop: join: the host's body plays move %d (animation %d of %u)\n", move, anim, Guest::Read32(base, list + 4)); fflush(stdout); }
            }
            g_lb.anim = anim; g_lb.move = move;
        }
        // one of ours for the host's entity: the same map entity, or a puppet
        bool Bind(PPCContext& ctx, uint8_t* base, uint16_t idx, Remote& r)
        {
            const std::string* cls = Name(r.cls);
            if (!cls || !r.havePlace) return false;
            if (const uint32_t mine = EntityAt(base, idx); mine && !g_puppets.count(mine) && !g_retired.count(mine) && !(Guest::Read32(base, mine + 632) & 0x400u) && !(Guest::Read32(base, mine + 16) & 0x100000u) && base[mine] == r.type && r.type != 11 && r.type != 1 && r.type != 10 && r.type != 2 && r.type != 3 && BrocText(base, mine + 564) == *cls)
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
                    if (const uint32_t e = EntityAt(base, i); e && base[e] == 10 && !g_puppets.count(e)) { Guest::Write32(base, e + 632, Guest::Read32(base, e + 632) | 0x400u); DeleteEntity(ctx, base, i); vehicles++; }
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
                if (const uint32_t e = EntityAt(base, i); e && (base[e] == 11 || base[e] == 10 || (base[e] == 7 && Valid(Guest::Read32(base, e + 552)))) && !g_puppets.count(e)) { Remove(ctx, base, i); deleted++; }
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
        g_frozen = false; g_inSince = -1; g_frozenAt = -1; g_firstSnapAt = -1;
        g_health = g_maxHealth = -1; g_psAt = -1;
        g_ownLetterbox = g_ownIntroScreen = false;
        for (bool& h : g_hudHave) h = false;
        (void)name;
    }

    void JoinReceive(PPCContext& ctx, uint8_t* base, Kind kind, const uint8_t* data, size_t size)
    {
        (void)ctx;
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
            return;
        }
        if (kind != K_SNAP || size < 16 || !InMission(base)) return;
        const double t = PacketTime();   // the host's time this was sent at
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
        // my command, view and place, each frame
        {
            struct { uint32_t seq; uint8_t cmd[48]; float view[3]; float origin[3]; float velocity[3]; uint32_t stance; uint32_t flags; char map[32]; } m{};
            m.seq = ++g_seq;
            memcpy(m.cmd, base + CommandSlot(base), 48);
            for (int k = 0; k < 3; k++) { m.view[k] = F(base, rec + 172 + 4 * k); m.origin[k] = F(base, rec + 4 * k); m.velocity[k] = F(base, rec + 16 + 4 * k); }
            m.stance = Guest::Read32(base, rec + 208);
            m.flags = 1 | (g_frozen ? 2 : 0);
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
        // (v45j) riding beside the host (his player carried by the scripts: Falaise's jeep), mine passes
        // through things (the record's +1452, the game's own noclip): set down each frame beside him inside
        // the vehicle's own model, his movement was pushed out of it again and again and the frame never ended
        {
            static bool ours = false;
            const bool riding = (g_hostFlags & 2) && g_havePs && now - g_psAt < 1.0;
            if (riding && !ours) { ours = true; Guest::Write32(base, rec + 1452, 1); printf("coop: join: riding beside the host: through things\n"); fflush(stdout); }
            else if (!riding && ours) { ours = false; Guest::Write32(base, rec + 1452, 0); }
        }
        if ((g_hostFlags & 2) && g_havePs && now - g_psAt < 1.0)
        {
            const float yaw = g_hostYaw * 0.0174533f;
            const float o[3] = { g_hostPos[0] + 36.0f * sinf(yaw), g_hostPos[1] - 36.0f * cosf(yaw), g_hostPos[2] };
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
                if (base[e] == 3 || (soldiers && (base[e] == 11 || base[e] == 10))) Remove(ctx, base, i);
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
            if (now - r.heard > 3.0) { Unbind(ctx, base, r); it = g_remote.erase(it); continue; }
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
            if (int32_t(it->first) == g_hostBodyIndex) DriveHostBody(ctx, base, r);
            else if (r.haveAnim && r.list) SetAnims(ctx, base, r);
            if (!r.local) { ++it; continue; }   // (let go for a fresh puppet)
            if (r.havePlace && !r.track.empty())
            {
                float o[3], a[3];
                Place(r, show, o, a);
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
