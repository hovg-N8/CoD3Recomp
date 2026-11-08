// Co-op v40, design probe (Claude session, 2026-10-29). DIAGNOSTIC ONLY,
// off unless COD3_PROBE40 is set. Campaign program only.
//
// What it answers, by its log ("p40:" lines):
//  1. every model the game sets on an entity (G_SetModel, sub_8254EE80:
//     the entity r3, the model's name r4 (text), the bank r5, the slot r6
//     0..3; slot 0 lands at entity +556, 1..3 in the 16-byte block at
//     entity +984), with the resource it found: are the same models at the
//     same addresses in two copies of the game?
//  2. every hit (G_Damage, sub_82564888: target r3, inflictor r4,
//     attacker r5, dir r6, point r7, damage r8, flags r9, means r10): does
//     the AI shoot a second player?
//  3. a census of the entities (type, class, models, animation tree and
//     the animations playing) at COD3_PROBE40 seconds after a mission starts
//  4. COD3_PROBE40_PUPPET=1: a script_model made beside the first living
//     soldier, given his models and his animation tree (by the game's own
//     spawn, G_SetModel and useanimtree code), then kept beside him with the
//     animations he plays copied into its tree each frame: whether a
//     drawn-only copy of a soldier can be animated from another's state
//  5. COD3_PROBE40_FREEZE=s: from s seconds after the start, the scripts'
//     threads no longer run (sub_824BB6F8, the script VM's frame, skipped)
//     and at s+5 every soldier and drone is deleted: a frozen world.
#ifndef COD3_MP

#include "kernel.h"
#include "overlay.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>


namespace
{
    constexpr uint32_t EntityTable = 0x82A2DCD0;
    constexpr uint32_t InfoPool = 0x82A59DB0;
    bool On() { static const bool on = getenv("COD3_PROBE40") != nullptr; return on; }
    double Clock() { static const auto t0 = std::chrono::steady_clock::now(); return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count(); }
    bool Valid(uint32_t a) { return a >= 0x40000000u && a < 0xC0000000u; }
    float F(const uint8_t* base, uint32_t a) { const uint32_t w = Guest::Read32(base, a); float f; memcpy(&f, &w, 4); return f; }
    void SetF(uint8_t* base, uint32_t a, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, a, w); }
    uint32_t EntityAt(const uint8_t* base, uint32_t i) { const uint32_t e = Guest::Read32(base, EntityTable + i * 8); return Valid(e) ? e : 0; }
    uint32_t RefOf(const uint8_t* base, uint32_t i) { return i | (Guest::Read32(base, EntityTable + i * 8 + 4) << 14); }
    int32_t IndexOf(const uint8_t* base, uint32_t e) { for (uint32_t i = 0; i < 4096; i++) if (Guest::Read32(base, EntityTable + i * 8) == e) return int32_t(i); return -1; }
    std::string Text(const uint8_t* base, uint32_t a, uint32_t max = 64)
    {
        std::string s;
        if (!Valid(a) && !(a >= 0x82000000u && a < 0x83000000u)) return "?";
        for (uint32_t i = 0; i < max; i++) { const uint8_t c = base[a + i]; if (c < 32 || c >= 127) break; s += char(c); }
        return s;
    }
    std::string BrocText(const uint8_t* base, uint32_t field)   // a Broc::string field: its block, text at +12
    {
        const uint32_t b = Guest::Read32(base, field);
        return Valid(b) ? Text(base, b + 12) : std::string("-");
    }
    std::map<uint32_t, std::string> g_modelName;   // (unused: names are the resources' own, +72)
    int g_setModelLogged = 0, g_damageLogged = 0;
    double g_missionAt = -1;
    double g_releasedAt = -1;   // when the scripts last set letterbox_enabled "0" (the intro over)
    bool g_freeze = false;
    PPCContext Call(PPCContext& ctx) { PPCContext c = ctx; c.r1.u32 = (c.r1.u32 - 0x1000) & ~0xFu; c.lr = 0; return c; }
    // a Broc::string made here (never freed: its count held high), and a slot holding it
    uint32_t MakeString(uint8_t* base, const std::string& s)
    {
        const uint32_t block = Guest::AllocatePhysical(uint32_t(16 + s.size() + 1 + 15) & ~15u);
        memset(base + block, 0, 16 + s.size() + 1);
        Guest::Write16(base, block + 8, 0x4000);
        memcpy(base + block + 12, s.c_str(), s.size() + 1);
        const uint32_t slot = Guest::AllocatePhysical(16);
        Guest::Write32(base, slot, block);
        return slot;
    }
    uint32_t ModelSlot(const uint8_t* base, uint32_t e, int slot)
    {
        if (slot == 0) return Guest::Read32(base, e + 556);
        const uint32_t m = Guest::Read32(base, e + 984);
        static const uint32_t at[4] = { 0, 0, 8, 4 };   // G_SetModel: slot 1 at +0, 2 at +8, 3 at +4
        return Valid(m) ? Guest::Read32(base, m + at[slot]) : 0;
    }
    std::string ResName(const uint8_t* base, uint32_t res) { return Valid(res) ? Text(base, Guest::Read32(base, res + 72)) : std::string("-"); }
    std::string ListName(const uint8_t* base, uint32_t tree)
    {
        if (!Valid(tree)) return "-";
        const uint32_t list = Guest::Read32(base, tree + 8);
        return (list >= 0x40000000u) ? Text(base, Guest::Read32(base, list)) : "?";
    }
    struct Node { uint16_t anim; uint32_t rec; };
    std::vector<Node> Active(const uint8_t* base, uint32_t tree)
    {
        std::vector<Node> out;
        if (!Valid(tree)) return out;
        const uint32_t list = Guest::Read32(base, tree + 8);
        const uint32_t count = Valid(list) ? Guest::Read32(base, list + 4) : 0;
        for (uint32_t i = 0; i < count && i < 4096; i++)
            if (const uint16_t r = Guest::Read16(base, tree + 24 + 2 * i)) out.push_back(Node{ uint16_t(i), InfoPool + 52u * r });
        return out;
    }
    void Census(const uint8_t* base)
    {
        std::map<int, int> types;
        int shownActors = 0, shownDrones = 0;
        for (uint32_t i = 0; i < 4096; i++)
        {
            const uint32_t e = EntityAt(base, i);
            if (!e) continue;
            const uint8_t type = base[e];
            types[type]++;
            const uint32_t tree = Guest::Read32(base, e + 552);
            const uint32_t m0 = ModelSlot(base, e, 0);
            if (!m0 && !tree) continue;
            std::string models;
            for (int s = 0; s < 4; s++) { const uint32_t m = ModelSlot(base, e, s); if (!m) continue; char t[160]; snprintf(t, sizeof t, " [%d]%08X=%s/%s", s, m, g_modelName.count(m) ? g_modelName[m].c_str() : "?", ResName(base, m).c_str()); models += t; }
            for (int k = 0; k < 7; k++) { const uint32_t m = Guest::Read32(base, e + 928 + 8 * k); if (!m) continue; char t[160]; snprintf(t, sizeof t, " att%d %s@%s", k, ResName(base, m).c_str(), BrocText(base, e + 932 + 8 * k).c_str()); models += t; }
            printf("p40: ent %u type %u class '%s' tname '%s' +500 %08X at %.0f %.0f %.0f flags632 %08X tree %08X (%s, %zu playing)%s\n", i, type, BrocText(base, e + 564).c_str(), BrocText(base, e + 572).c_str(),
                   Guest::Read32(base, e + 500), F(base, e + 336), F(base, e + 340), F(base, e + 344), Guest::Read32(base, e + 632), tree, ListName(base, tree).c_str(), Active(base, tree).size(), models.c_str());
            if ((type == 11 && shownActors++ < 3) || (type == 7 && shownDrones++ < 2))
                for (const Node& n : Active(base, tree))
                    printf("p40:    anim %u rec %u: time %.3f old %.3f blend %.3f goal %.3f weight %.3f rate %.3f +0 %04X +2 %04X +4 %08X +44 %08X\n", n.anim, (n.rec - InfoPool) / 52, F(base, n.rec + 16), F(base, n.rec + 20), F(base, n.rec + 28), F(base, n.rec + 32), F(base, n.rec + 36), F(base, n.rec + 40),
                           Guest::Read16(base, n.rec), Guest::Read16(base, n.rec + 2), Guest::Read32(base, n.rec + 4), Guest::Read32(base, n.rec + 44));
        }
        std::string t;
        for (auto& [k, v] : types) { char b[24]; snprintf(b, sizeof b, " %d:%d", k, v); t += b; }
        printf("p40: census at %.0f s: by type%s\n", Clock(), t.c_str());
        const uint32_t lists = Guest::Read32(base, 0x82A2A204);
        if (Valid(lists))
        {
            const uint32_t tab = Guest::Read32(base, lists + 4);
            const uint32_t n = Valid(tab) ? Guest::Read32(base, tab) : 0;
            const uint32_t entries = Valid(tab) ? Guest::Read32(base, tab + 4) : 0;
            printf("p40: anim lists at %08X: %u\n", tab, n);
            for (uint32_t k = 0; k < n && k < 64; k++)
                printf("p40:   list %u at %08X '%s' count %u\n", k, entries + 12 * k, Text(base, Guest::Read32(base, entries + 12 * k)).c_str(), Guest::Read32(base, entries + 12 * k + 4));
        }
        fflush(stdout);
    }

    // ---- the puppet test ----
    uint32_t g_source = 0, g_puppet = 0;
    double g_puppetAt = -1;
    void CopyAnims(PPCContext& ctx, uint8_t* base, uint32_t fromTree, uint32_t toTree)
    {
        const std::vector<Node> want = Active(base, fromTree);
        std::set<uint16_t> wanted;
        for (const Node& n : want)
        {
            wanted.insert(n.anim);
            if (!Guest::Read16(base, toTree + 24 + 2 * n.anim))
            {
                PPCContext c = Call(ctx);
                c.r3.u64 = toTree; c.r4.u64 = n.anim; c.f1.f64 = F(base, n.rec + 32); c.f2.f64 = 0.0;
                Guest::Lookup(0x82500B80)(c, base);   // the scripts' "set anim, parents alone"
            }
            const uint16_t r = Guest::Read16(base, toTree + 24 + 2 * n.anim);
            if (!r) continue;
            const uint32_t rec = InfoPool + 52u * r;
            for (const uint32_t at : { 16u, 20u, 28u, 32u, 36u, 40u }) Guest::Write32(base, rec + at, Guest::Read32(base, n.rec + at));
        }
        for (const Node& n : Active(base, toTree))
            if (!wanted.count(n.anim)) { SetF(base, n.rec + 32, 0.0f); SetF(base, n.rec + 36, 0.0f); }
    }
    uint32_t TextIn(uint8_t* base, const std::string& t)
    {
        static std::map<std::string, uint32_t> texts;
        if (!texts.count(t)) { const uint32_t a = Guest::AllocatePhysical(uint32_t(t.size() + 16) & ~15u); memcpy(base + a, t.c_str(), t.size() + 1); texts[t] = a; }
        return texts[t];
    }
    uint32_t Bank(const uint8_t* base, uint32_t e)
    {
        const int32_t b = int32_t(Guest::Read32(base, e + 496));
        return b != -1 ? uint32_t(b) : Guest::Read32(base, Guest::Read32(base, 0x82A2AB6C) + 40);
    }
    // a script_model dressed as entity src: its four models, its attachments, its tree
    uint32_t MakePuppet(PPCContext& ctx, uint8_t* base, uint32_t src, const float at[3], float yaw)
    {
        static const uint32_t cls = MakeString(base, "script_model");
        const uint32_t org = Guest::AllocatePhysical(16);
        for (int k = 0; k < 3; k++) SetF(base, org + 4 * k, at[k]);
        PPCContext c = Call(ctx);
        c.r3.u64 = cls; c.r4.u64 = org; c.r5.u64 = 0;
        Guest::Lookup(0x824B4510)(c, base);   // the scripts' spawn
        const uint32_t ref = c.r3.u32;
        const uint32_t e = (ref & 0x3FFF) < 4096 ? EntityAt(base, ref & 0x3FFF) : 0;
        if (!e) return 0;
        for (int s = 0; s < 4; s++)
        {
            const uint32_t m = ModelSlot(base, src, s);
            if (!Valid(m)) continue;
            c = Call(ctx);
            c.r3.u64 = e; c.r4.u64 = TextIn(base, ResName(base, m)); c.r5.u64 = uint64_t(-1); c.r6.u64 = uint64_t(s);
            Guest::Lookup(0x8254EE80)(c, base);
            printf("p40: puppet: slot %d %s -> %08X (source's %08X)\n", s, ResName(base, m).c_str(), ModelSlot(base, e, s), m);
        }
        for (int k = 0; k < 7; k++)
        {
            const uint32_t m = Guest::Read32(base, src + 928 + 8 * k);
            if (!Valid(m)) continue;
            c = Call(ctx);
            c.r3.u64 = e; c.r4.u64 = TextIn(base, ResName(base, m)); c.r5.u64 = TextIn(base, BrocText(base, src + 932 + 8 * k));
            c.r6.u64 = (Guest::Read8(base, src + 621) >> k) & 1; c.r7.u64 = Bank(base, e);
            Guest::Lookup(0x825587B0)(c, base);   // attach's own step
            printf("p40: puppet: attached %s at %s -> %08X\n", ResName(base, m).c_str(), BrocText(base, src + 932 + 8 * k).c_str(), Guest::Read32(base, e + 928 + 8 * k));
        }
        const uint32_t srcTree = Guest::Read32(base, src + 552);
        if (Valid(srcTree))
        {
            c = Call(ctx);
            c.r3.u64 = e; c.r4.u64 = Guest::Read32(base, srcTree + 8);
            Guest::Lookup(0x82555B98)(c, base);   // useanimtree's own step (the source's list)
        }
        SetF(base, e + 352, 0.0f); SetF(base, e + 356, yaw); SetF(base, e + 360, 0.0f);
        c = Call(ctx); c.r3.u64 = e; Guest::Lookup(0x82554D60)(c, base);
        c = Call(ctx); c.r3.u64 = e; Guest::Lookup(0x82513198)(c, base);
        return e;
    }
    void InFront(const uint8_t* base, float out[3], float* yaw, float dist)
    {
        const uint32_t me = EntityAt(base, 1);
        const float y = F(base, me + 356) * 0.0174533f;
        out[0] = F(base, me + 336) + cosf(y) * dist; out[1] = F(base, me + 340) + sinf(y) * dist; out[2] = F(base, me + 344);
        *yaw = F(base, me + 356) + 180.0f;
    }
    void Puppet(PPCContext& ctx, uint8_t* base)
    {
        static const bool want = getenv("COD3_PROBE40_PUPPET") != nullptr;
        if (!want || g_missionAt < 0) return;
        const double now = Clock();
        if (!g_puppet)
        {
            static const double at = [] { const char* t = getenv("COD3_PROBE40"); return t ? atof(t) : 40.0; }();
            if (now - g_missionAt < at + 5) return;
            // the first living ordinary soldier with a tree and an attachment (his rifle)
            for (int pass = 0; pass < 2 && !g_source; pass++)
                for (uint32_t i = 2; i < 4096 && !g_source; i++)
                    if (const uint32_t e = EntityAt(base, i); e && base[e] == 11 && int32_t(Guest::Read32(base, e + 776)) > 0 && Valid(Guest::Read32(base, e + 552)) && (pass == 1 || Valid(Guest::Read32(base, e + 928)))) g_source = e;
            if (!g_source) return;
            float p[3], yaw;
            InFront(base, p, &yaw, 140.0f);
            const uint32_t e = MakePuppet(ctx, base, g_source, p, yaw);
            printf("p40: puppet: entity %d (type %u, tree %08X %s) for source %d (%s, tree %s)\n", e ? IndexOf(base, e) : -1, e ? base[e] : 0, e ? Guest::Read32(base, e + 552) : 0,
                   e ? ListName(base, Guest::Read32(base, e + 552)).c_str() : "-", IndexOf(base, g_source), BrocText(base, g_source + 564).c_str(), ListName(base, Guest::Read32(base, g_source + 552)).c_str());
            fflush(stdout);
            g_puppet = e ? e : 1; g_puppetAt = now;
            Overlay::RequestScreenshot();
            return;
        }
        if (g_puppet < 0x40000000u) return;
        if (IndexOf(base, g_source) < 0 || IndexOf(base, g_puppet) < 0) { printf("p40: puppet: source or puppet gone\n"); fflush(stdout); g_puppet = 1; return; }
        const uint32_t ft = Guest::Read32(base, g_source + 552), tt = Guest::Read32(base, g_puppet + 552);
        if (Valid(ft) && Valid(tt)) CopyAnims(ctx, base, ft, tt);
        float p[3], yaw;
        InFront(base, p, &yaw, 140.0f);
        for (int k = 0; k < 3; k++) SetF(base, g_puppet + 336 + 4 * k, p[k]);
        SetF(base, g_puppet + 356, yaw);
        PPCContext c = Call(ctx); c.r3.u64 = g_puppet; Guest::Lookup(0x82513198)(c, base);
        static double shotAt = 0;
        if (now - g_puppetAt > 2 && now - shotAt > 5 && now - g_puppetAt < 40) { shotAt = now; Overlay::RequestScreenshot(); printf("p40: puppet: picture (source %zu playing, puppet %zu)\n", Active(base, ft).size(), Active(base, tt).size()); fflush(stdout); }
    }
    void KillScripts(uint8_t* base)
    {
        constexpr uint32_t vm = 0x82A90128;
        int n = 0;
        for (uint32_t t = Guest::Read32(base, vm + 4); Valid(t) && t != vm + 8 && n < 4096; t = Guest::Read32(base, t), n++)
            Guest::Write32(base, t + 16, Guest::Read32(base, t + 16) | 0x4Cu);
        static int told = 0;
        if (told++ < 3) { printf("p40: freeze: %d script threads told to end\n", n); fflush(stdout); }
    }
    void Freeze(PPCContext& ctx, uint8_t* base)
    {
        static const double at = [] { const char* t = getenv("COD3_PROBE40_FREEZE"); return t ? atof(t) : -1.0; }();
        if (at < 0 || g_missionAt < 0) return;
        const double now = Clock() - g_missionAt;
        if (!g_freeze && now >= at)
        {
            g_freeze = true;
            const uint32_t table = Guest::Read32(base, 0x82A2A2A0);
            if (Valid(table) || (table >= 0x82000000u && table < 0x8A000000u))
            {
                printf("p40: freeze: the level's function table %08X: B60 %08X B64 %08X -> the empty 82497FF0\n", table, Guest::Read32(base, table + 0xB60), Guest::Read32(base, table + 0xB64));
                Guest::Write32(base, table + 0xB60, 0x82497FF0);
            }
            printf("p40: freeze: the scripts stop now\n"); fflush(stdout);
        }
        if (g_freeze) KillScripts(base);
        static bool cleared = false;
        if (!cleared && now >= at + 5)
        {
            cleared = true;
            int n = 0;
            for (uint32_t i = 2; i < 4096; i++)
                if (const uint32_t e = EntityAt(base, i); e && (base[e] == 11 || (base[e] == 7 && Valid(Guest::Read32(base, e + 552)))) && e != g_source && e != g_puppet)
                {
                    PPCContext c = Call(ctx); c.r3.u64 = RefOf(base, i);
                    Guest::Lookup(0x824A7280)(c, base);   // the scripts' delete
                    n++;
                }
            printf("p40: freeze: %d soldiers and drones deleted\n", n); fflush(stdout);
            Overlay::RequestScreenshot();
        }
    }
}

namespace
{
    // COD3_PROBE40_ANIMS=1: which of generic_human's animations the soldiers
    // play standing still, walking and running, standing, crouched and prone,
    // forward, back and to the side: for the players' bodies. Every 0.2 s each
    // living soldier: his pose (actor +448, a name hash), his speed and the
    // way he goes against the way he faces, and the leaves of his tree with
    // weight 0.3 or more (the list entry's +0 is the name's hash, +4 its
    // children). Counted, and the most played printed each 30 s.
    uint32_t NameHash(const char* t) { uint32_t h = 0; for (; *t; t++) h = h * 33 + uint32_t(tolower(static_cast<unsigned char>(*t))); return h; }
    void AnimStats(const uint8_t* base)
    {
        static const bool on = getenv("COD3_PROBE40_ANIMS") != nullptr;
        if (!on) return;
        static double at = 0, printAt = 0;
        const double now = Clock();
        if (now - at < 0.2) return;
        const double dt = now - at;
        at = now;
        struct Seen { float o[3]; double t; };
        static std::map<uint32_t, Seen> last;
        static std::map<std::string, std::map<uint32_t, std::pair<int, double>>> counts;   // situation -> anim hash -> (count, rate sum)
        static std::map<uint32_t, uint32_t> indexOf;   // anim hash -> its index (this level)
        static const uint32_t hStand = NameHash("stand"), hCrouch = NameHash("crouch"), hProne = NameHash("prone");
        for (uint32_t i = 2; i < 4096; i++)
        {
            const uint32_t e = EntityAt(base, i);
            if (!e || base[e] != 11 || int32_t(Guest::Read32(base, e + 776)) <= 0) continue;
            const uint32_t actor = Guest::Read32(base, e + 532), tree = Guest::Read32(base, e + 552);
            if (!Valid(actor) || !Valid(tree)) continue;
            const float o[3] = { F(base, e + 336), F(base, e + 340), F(base, e + 344) };
            auto it = last.find(e);
            Seen prev = it != last.end() ? it->second : Seen{ { o[0], o[1], o[2] }, now - dt };
            last[e] = Seen{ { o[0], o[1], o[2] }, now };
            if (it == last.end()) continue;
            const float vx = (o[0] - prev.o[0]) / float(dt), vy = (o[1] - prev.o[1]) / float(dt);
            const float speed = sqrtf(vx * vx + vy * vy);
            const uint32_t pose = Guest::Read32(base, actor + 448);
            const char* p = pose == hStand ? "stand" : pose == hCrouch ? "crouch" : pose == hProne ? "prone" : "other";
            const char* sp = speed < 15 ? "still" : speed < 110 ? "walk" : speed < 220 ? "run" : "fast";
            const float yaw = F(base, e + 356) * 0.0174533f;
            const float fwd = vx * cosf(yaw) + vy * sinf(yaw), side = -vx * sinf(yaw) + vy * cosf(yaw);
            const char* dir = speed < 15 ? "-" : fabsf(fwd) >= fabsf(side) ? (fwd > 0 ? "fwd" : "back") : (side > 0 ? "left" : "right");
            const std::string key = std::string(p) + "/" + sp + "/" + dir;
            const uint32_t list = Guest::Read32(base, tree + 8);
            const uint32_t count = std::min<uint32_t>(Guest::Read32(base, list + 4), 4096), entries = Guest::Read32(base, list + 8);
            for (uint32_t k = 0; k < count; k++)
            {
                const uint16_t r = Guest::Read16(base, tree + 24 + 2 * k);
                if (!r) continue;
                const uint32_t rec = InfoPool + 52u * r;
                if (F(base, rec + 36) < 0.3f) continue;
                const uint32_t entry = entries + 28 * k;
                if (Guest::Read16(base, entry + 4) != 0) continue;   // (a parent node: not an animation)
                const uint32_t hash = Guest::Read32(base, entry);
                indexOf[hash] = k;
                auto& c = counts[key][hash];
                c.first++; c.second += F(base, rec + 40);
            }
        }
        if (now - printAt > 30)
        {
            printAt = now;
            for (auto& [key, m] : counts)
            {
                std::vector<std::pair<int, uint32_t>> v;
                int total = 0;
                for (auto& [h, c] : m) { v.emplace_back(c.first, h); total += c.first; }
                std::sort(v.rbegin(), v.rend());
                std::string line;
                for (size_t k = 0; k < v.size() && k < 8; k++) { char t[64]; snprintf(t, sizeof t, " %08X(#%u)x%d r%.2f", v[k].second, indexOf[v[k].second], v[k].first, m[v[k].second].second / std::max(1, v[k].first)); line += t; }
                printf("p40: anims %-22s %5d:%s\n", key.c_str(), total, line.c_str());
            }
            fflush(stdout);
        }
    }
}

namespace
{
    // COD3_PROBE40_DUMP="5 15 60": the player's entity (1040 bytes) and record
    // (2224) at those seconds after the mission starts, as words, and a
    // screenshot: what changes in a scripted scene (letterbox, frozen controls)
    void Dumps(const uint8_t* base)
    {
        static const char* const spec = getenv("COD3_PROBE40_DUMP");
        if (!spec || g_missionAt < 0) return;
        static std::vector<double> at;
        static bool parsed = false;
        if (!parsed) { parsed = true; for (const char* t = spec; *t;) { char* end; const double v = strtod(t, &end); if (end == t) break; at.push_back(v); t = end; } }
        const double now = Clock() - g_missionAt;
        for (size_t i = 0; i < at.size(); i++)
        {
            if (now < at[i]) continue;
            at.erase(at.begin() + long(i));
            const uint32_t me = EntityAt(base, 1);
            const uint32_t rec = me ? Guest::Read32(base, me + 528) : 0;
            printf("p40: dump at %.1f s: entity %08X record %08X\n", now, me, rec);
            auto dump = [&](const char* what, uint32_t from, uint32_t size) {
                for (uint32_t o = 0; o < size; o += 32)
                {
                    printf("p40: dump %s +%04X:", what, o);
                    for (uint32_t k = 0; k < 32; k += 4) printf(" %08X", Guest::Read32(base, from + o + k));
                    printf("\n");
                }
            };
            if (me) dump("ent", me, 1040);
            if (rec) dump("rec", rec, 2224);
            dump("cl", 0x82A57900, 7280);
            fflush(stdout);
            Overlay::RequestScreenshot();
            break;
        }
    }
}

// 6. the dvars that hide the HUD and the gun or draw the letterbox, as they
//    change (both copies), and who sets them by name (sub_8252B850)
namespace
{
    const char* const Watched[] = { "letterbox_enabled", "introScreen", "cg_draw2D", "cg_drawGun", "cg_drawCrosshair", "cg_hudAlpha", "cg_letterbox", "g_gameskill", "hud_enabled", "ui_hud_hardcore" };
    void DvarWatch(PPCContext& ctx, uint8_t* base)
    {
        static double at = 0;
        if (Clock() - at < 1.0) return;
        at = Clock();
        static uint32_t was[16]; static bool have[16] = {};
        static uint32_t names[16] = {};
        for (size_t i = 0; i < sizeof Watched / sizeof Watched[0]; i++)
        {
            if (!names[i]) { const size_t n = strlen(Watched[i]); names[i] = Guest::AllocatePhysical(uint32_t(n + 16) & ~15u); memcpy(base + names[i], Watched[i], n + 1); }
            PPCContext c = ctx; c.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu; c.lr = 0;
            c.r3.u64 = names[i];
            if (PPCFunc* f = Guest::Lookup(0x825235D0)) f(c, base); else return;
            const uint32_t d = c.r3.u32;
            const uint32_t v = Valid(d) || (d >= 0x82000000u && d < 0x83000000u) ? Guest::Read32(base, d + 32) : 0xFFFFFFFFu;
            if (!have[i] || v != was[i]) { have[i] = true; was[i] = v; float f; memcpy(&f, &v, 4); printf("p40: dvar %s = %d (%08X, %g)%s\n", Watched[i], int32_t(v), v, f, d ? "" : " (none)"); fflush(stdout); }
        }
    }
}

// a dvar set by its name (coop.cpp's hook of sub_8252B850: name, value, flags)
void Probe40DvarSet(PPCContext& ctx, uint8_t* base)
{
    if (On() && ctx.r3.u32 && ctx.r4.u32)
    {
        const std::string name = Text(base, ctx.r3.u32, 40);
        for (const char* w : Watched)
            if (_stricmp(name.c_str(), w) == 0)
            {
                // the callers, from the guest stack's back chain (each frame's saved LR at its -8)
                std::string chain;
                uint32_t frame = ctx.r1.u32;
                for (int d = 0; d < 6 && Valid(frame); d++)
                {
                    const uint32_t up = Guest::Read32(base, frame);
                    if (!Valid(up) || up <= frame) break;
                    char t[16]; snprintf(t, sizeof t, " %08X", Guest::Read32(base, up - 8));
                    chain += t; frame = up;
                }
                printf("p40: set %s \"%s\" (from %08X;%s)\n", name.c_str(), Text(base, ctx.r4.u32, 40).c_str(), uint32_t(ctx.lr), chain.c_str()); fflush(stdout);
                if (_stricmp(name.c_str(), "letterbox_enabled") == 0 && Text(base, ctx.r4.u32, 4) == "0" && g_releasedAt < 0 && ctx.lr) g_releasedAt = Clock();
                break;
            }
    }
}

void Probe40Frame(PPCContext& ctx, uint8_t* base)
{
    if (!On()) return;
    DvarWatch(ctx, base);
    { void Probe40LevelStrings(const uint8_t* base); if (EntityAt(base, 1)) Probe40LevelStrings(base); }
    // the player's place each second (where the intro takes him)
    {
        static double at = 0;
        const uint32_t p = EntityAt(base, 1) ? EntityAt(base, 1) : EntityAt(base, 0);
        const uint32_t rec = p ? Guest::Read32(base, p + 528) : 0;
        if (Clock() - at >= 1.0 && rec && (Valid(rec) || (rec >= 0x82000000u && rec < 0x83000000u)))
        {
            at = Clock();
            // and the local client's command (its 48 bytes: what the pad and keys asked)
            constexpr uint32_t LocalClients = 0x82A57900;
            const uint32_t slot = LocalClients + 5752 + (Guest::Read32(base, LocalClients + 5848) & 1) * 48;
            char cmd[120] = {}; for (int k = 0; k < 48; k++) snprintf(cmd + k * 2, 3, "%02X", base[slot + k]);
            // the whole record 3 s and 8 s after the intro lets go (to compare a run alone with one with a friend)
            {
                static int dumps = 0;
                const double releasedAt = g_releasedAt;
                if (releasedAt >= 0 && dumps < 2 && Clock() - releasedAt > (dumps == 0 ? 3.0 : 8.0))
                {
                    dumps++;
                    printf("p40: record dump %d (entity %08X record %08X):", dumps, p, rec);
                    for (uint32_t at = 0; at < 2224; at += 4) { if (at % 64 == 0) printf("\np40: rec+%04u", at); printf(" %08X", Guest::Read32(base, rec + at)); }
                    printf("\np40: entity dump %d:", dumps);
                    for (uint32_t at = 0; at < 1024; at += 4) { if (at % 64 == 0) printf("\np40: ent+%04u", at); printf(" %08X", Guest::Read32(base, p + at)); }
                    printf("\n"); fflush(stdout);
                }
            }
            printf("p40: me at %.0f %.0f %.0f vel %.0f %.0f %.0f flags %08X buttons %08X linked %08X cmd %s\n", F(base, rec), F(base, rec + 4), F(base, rec + 8), F(base, rec + 16), F(base, rec + 20), F(base, rec + 24),
                   Guest::Read32(base, rec + 208), Guest::Read32(base, rec + 1352), Guest::Read32(base, p + 916), cmd);
            fflush(stdout);
        }
    }
    const uint32_t me = EntityAt(base, 1);
    const bool in = me && Valid(Guest::Read32(base, me + 528));
    if (in && g_missionAt < 0) { g_missionAt = Clock(); printf("p40: in a mission at %.1f s\n", g_missionAt); fflush(stdout); }
    if (!in) return;
    static const double at = [] { const char* t = getenv("COD3_PROBE40"); return t ? atof(t) : 40.0; }();
    static int censuses = 0;
    if (censuses < 2 && Clock() - g_missionAt >= at + censuses * 60) { censuses++; Census(base); }
    Puppet(ctx, base);
    Freeze(ctx, base);
    AnimStats(base);
    Dumps(base);
}

// G_Damage's log (the hook is coop_laststand.cpp's)
void Probe40Damage(const uint8_t* base, uint32_t t, uint32_t a, int32_t dmg, int32_t mod, uint32_t flags, int32_t before)
{
    if (!On()) return;
    const bool player = Valid(t) && (base[t] == 1);
    if (player || g_damageLogged++ < 150)
    {
        printf("p40: damage %d (means %d, flags %X) to ent %d type %u '%s' from ent %d type %u '%s': health %d -> %d\n", dmg, mod, flags,
               Valid(t) ? IndexOf(base, t) : -1, Valid(t) ? base[t] : 0, Valid(t) ? BrocText(base, t + 564).c_str() : "-",
               Valid(a) ? IndexOf(base, a) : -1, Valid(a) ? base[a] : 0, Valid(a) ? BrocText(base, a + 564).c_str() : "-", before, Valid(t) ? int32_t(Guest::Read32(base, t + 776)) : 0);
        fflush(stdout);
    }
}


// 7. the sound system's commands (sub_825344xx..82535Bxx build one, sub_8252AC20 sends it):
//    who plays what, with which arguments, and what an alias (the r5 of most) looks like,
//    to replay the host's sounds on the joining copy
namespace
{
    void SoundLog(const char* what, PPCContext& ctx, uint8_t* base)
    {
        static int told = 0;
        if (!On() || told >= 400) return;
        told++;
        std::string alias;
        const uint32_t a = ctx.r5.u32;
        if (Valid(a) || (a >= 0x82000000u && a < 0x83000000u))
        {
            char t[160]; snprintf(t, sizeof t, " alias %08X: %08X %08X %08X %08X %08X %08X", a, Guest::Read32(base, a), Guest::Read32(base, a + 4), Guest::Read32(base, a + 8), Guest::Read32(base, a + 12), Guest::Read32(base, a + 16), Guest::Read32(base, a + 20));
            alias = t;
            const uint32_t n = Guest::Read32(base, a);
            if (Valid(n) || (n >= 0x82000000u && n < 0x83000000u)) alias += " '" + Text(base, n, 48) + "'";
            alias += " '" + Text(base, a, 24) + "'";
        }
        printf("p40: sound %s from %08X: r3 %08X r4 %08X r5 %08X r6 %08X r7 %08X r8 %08X%s\n", what, uint32_t(ctx.lr), ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, alias.c_str());
        fflush(stdout);
    }
}
extern "C" PPC_FUNC(__imp__sub_82535668);
extern "C" PPC_FUNC(__imp__sub_82535788);
extern "C" PPC_FUNC(__imp__sub_82535568);
extern "C" PPC_FUNC(__imp__sub_82535460);
PPC_FUNC(sub_82535668) { SoundLog("5668", ctx, base); __imp__sub_82535668(ctx, base); }
PPC_FUNC(sub_82535788) { SoundLog("5788", ctx, base); __imp__sub_82535788(ctx, base); }
PPC_FUNC(sub_82535568) { SoundLog("5568", ctx, base); __imp__sub_82535568(ctx, base); }
PPC_FUNC(sub_82535460) { SoundLog("5460", ctx, base); __imp__sub_82535460(ctx, base); }

// 8. the effects (FX_PlayFX, sub_82423F08: r3 an id or -1, r4 the effect, r5.. where)
extern "C" PPC_FUNC(__imp__sub_82423F08);
PPC_FUNC(sub_82423F08)
{
    static int told = 0;
    if (On() && told < 300)
    {
        told++;
        const uint32_t e = ctx.r4.u32;
        std::string fx;
        if (Valid(e) || (e >= 0x82000000u && e < 0x83000000u))
        {
            char t[160]; snprintf(t, sizeof t, " fx %08X: %08X %08X %08X %08X", e, Guest::Read32(base, e), Guest::Read32(base, e + 4), Guest::Read32(base, e + 8), Guest::Read32(base, e + 12));
            fx = t;
            const uint32_t n = Guest::Read32(base, e);
            if (Valid(n) || (n >= 0x82000000u && n < 0x83000000u)) fx += " '" + Text(base, n, 48) + "'";
            fx += " '" + Text(base, e, 24) + "'";
        }
        auto vec = [&](uint32_t a) { char t[64]; if (Valid(a) || (a >= 0x82000000u && a < 0x83000000u)) snprintf(t, sizeof t, "(%.0f %.0f %.0f)", F(base, a), F(base, a + 4), F(base, a + 8)); else snprintf(t, sizeof t, "%08X", a); return std::string(t); };
        printf("p40: fx from %08X: r3 %08X r5 %s r6 %s r7 %s r8 %08X r9 %08X f1 %g%s\n", uint32_t(ctx.lr), ctx.r3.u32, vec(ctx.r5.u32).c_str(), vec(ctx.r6.u32).c_str(), vec(ctx.r7.u32).c_str(), ctx.r8.u32, ctx.r9.u32, ctx.f1.f64, fx.c_str());
        fflush(stdout);
    }
    __imp__sub_82423F08(ctx, base);
}

// 9. the scripts' notifies (0x74, sub_824A5268: entity ref r3, name hash r4), to compare a
//    mission played alone with one played with a friend
extern "C" PPC_FUNC(__imp__sub_824A5268);
PPC_FUNC(sub_824A5268)
{
    static int told = 0;
    if (On() && (told < 1500 || ctx.r4.u32 == 0xC8EF1B34u))
    {
        told++;
        const uint32_t ref = ctx.r3.u32, index = ref & 0x3FFF;
        const uint32_t e = index < 4096 ? EntityAt(base, index) : 0;
        uint32_t frame = ctx.r1.u32; std::string chain;
        for (int d = 0; d < 3 && Valid(frame); d++) { const uint32_t up = Guest::Read32(base, frame); if (!Valid(up) || up <= frame) break; char t[16]; snprintf(t, sizeof t, " %08X", Guest::Read32(base, up - 8)); chain += t; frame = up; }
        printf("p40: notify %08X to %u (type %u %s) at %.1f s;%s\n", ctx.r4.u32, index, e ? base[e] : 0, e ? BrocText(base, e + 564).c_str() : "-", Clock() - g_missionAt, chain.c_str());
        fflush(stdout);
    }
    __imp__sub_824A5268(ctx, base);
}

// 10. the engine's notifies (sub_824927C0: the entity r3, the name hash r4), who sends what
extern "C" PPC_FUNC(__imp__sub_824927C0);
PPC_FUNC(sub_824927C0)
{
    static int told = 0;
    if (On() && Valid(ctx.r3.u32) && ctx.r4.u32 != 0xE7803C50u && (told < 1200 || ctx.r4.u32 == 0x1BDABECFu))
    {
        told++;
        const uint32_t e = ctx.r3.u32;
        printf("p40: engine notify %08X to %d (type %u %s) from %08X at %.1f s\n", ctx.r4.u32, IndexOf(base, e), base[e], BrocText(base, e + 564).c_str(), uint32_t(ctx.lr), Clock() - g_missionAt);
        fflush(stdout);
    }
    __imp__sub_824927C0(ctx, base);
}

// 11. the notifies with an argument (0x78, 0x7C, 0x84) and the level's own strings asked about
namespace
{
    void NotifyArgLog(const char* what, PPCContext& ctx, uint8_t* base)
    {
        static int told = 0;
        if (!On() || told >= 600) return;
        told++;
        const uint32_t index = ctx.r3.u32 & 0x3FFF;
        const uint32_t e = index < 4096 ? EntityAt(base, index) : 0;
        printf("p40: notify%s %08X to %u (type %u %s) at %.1f s from %08X\n", what, ctx.r4.u32, index, e ? base[e] : 0, e ? BrocText(base, e + 564).c_str() : "-", Clock() - g_missionAt, uint32_t(ctx.lr));
        fflush(stdout);
    }
}
extern "C" PPC_FUNC(__imp__sub_824A2360);
extern "C" PPC_FUNC(__imp__sub_824A2280);
extern "C" PPC_FUNC(__imp__sub_824A21A8);
PPC_FUNC(sub_824A2360) { NotifyArgLog("(int)", ctx, base); __imp__sub_824A2360(ctx, base); }
PPC_FUNC(sub_824A2280) { NotifyArgLog("(string)", ctx, base); __imp__sub_824A2280(ctx, base); }
PPC_FUNC(sub_824A21A8) { NotifyArgLog("(hash)", ctx, base); __imp__sub_824A21A8(ctx, base); }
void Probe40LevelStrings(const uint8_t* base)
{
    if (!On()) return;
    static bool done = false;
    if (done) return;
    done = true;
    for (const uint32_t a : { 0x89006CD0u, 0x89006CC8u, 0x89006C00u + 0x10u * 0 })
        printf("p40: the level's text at %08X: '%s'\n", a, Text(base, a, 64).c_str());
    fflush(stdout);
}

#endif
