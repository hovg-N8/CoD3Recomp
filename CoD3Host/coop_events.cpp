// Co-op v40: the host's sounds, speech, music and earthquakes, on the joining
// copy too (Team Friend, Claude session, 2026-10-29). The joining copy's own
// scripts are stopped once its world is the host's, so what the host's
// scripts play (a line of speech, the music, a sound on a soldier, an
// earthquake) and what the engine plays for the host's soldiers (their
// weapons' and grenades' sounds) is sent and played there the same way.
//
// Each forwarded function is the one the scripts (or the engine) call, with
// its arguments: entity references and pointers become the host's entity
// numbers (the joining copy uses its own puppet for that number), a script
// string or a text becomes its text, a vector its three floats. A notify the
// host's scripts wait for is not asked for on the joining copy (its scripts
// wait for nothing). The joining player's own weapon is his copy's to sound.
#ifndef COD3_MP

#include "coop.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace Coop
{
    namespace
    {
        // registers r3..r10 as bits 0..7
        struct Fwd
        {
            uint32_t func;
            uint8_t ref, broc, ptr, text, vec, zero, out;   // entity ref, script string slot, entity pointer, text, vector, sent as 0, a result slot
            uint8_t floats;                                 // f1.. that matter
            const char* what;
        };
        const Fwd Table[] = {
            { 0x824B51E0, 0x01, 0x02, 0, 0, 0, 0x04, 0, 0, "speech" },
            { 0x824B51E8, 0x01, 0x02, 0, 0, 0, 0x04, 0, 0, "speech (no wait)" },
            { 0x824B2E28, 0x01, 0x02, 0, 0, 0, 0, 0, 0, "playsound" },
            { 0x824B3018, 0x01, 0x02, 0, 0, 0, 0x08, 0, 0, "playsound (notify)" },
            { 0x824AD260, 0, 0x01, 0, 0, 0, 0, 0, 0, "musicplay" },
            { 0x824AD238, 0, 0x01, 0, 0, 0, 0, 0, 1, "musicplay (fade)" },
            { 0x824A8498, 0, 0, 0, 0, 0, 0, 0, 0, "musicstop" },
            { 0x824ACE70, 0, 0x01, 0, 0, 0, 0, 0, 1, "2D sound" },
            { 0x824A7C28, 0, 0, 0, 0, 0x04, 0, 0, 3, "earthquake" },
            { 0x82534ED0, 0, 0, 0x02, 0x04, 0, 0, 0x01, 0, "sound on a thing" },
        };
        constexpr int TableSize = int(sizeof Table / sizeof Table[0]);
        constexpr uint16_t HostPlayer = 0xFFFE, JoiningPlayer = 0xFFFD, NoEntity = 0xFFFF;

        uint16_t HostNumber(const uint8_t* base, uint32_t e)
        {
            if (!e || !Valid(e)) return NoEntity;
            if (e == Me(base)) return HostPlayer;
            if (e == SecondPlayer()) return JoiningPlayer;
            const int32_t i = IndexOf(base, e);
            return i >= 0 ? uint16_t(i) : NoEntity;
        }
        void PutU32(std::vector<uint8_t>& m, uint32_t v) { m.insert(m.end(), reinterpret_cast<uint8_t*>(&v), reinterpret_cast<uint8_t*>(&v) + 4); }
        void PutText(std::vector<uint8_t>& m, const std::string& t) { const uint16_t n = uint16_t(std::min<size_t>(t.size(), 200)); m.insert(m.end(), reinterpret_cast<const uint8_t*>(&n), reinterpret_cast<const uint8_t*>(&n) + 2); m.insert(m.end(), t.begin(), t.begin() + n); }

        // the host: the call, as it is made
        void Forward(PPCContext& ctx, uint8_t* base, int which)
        {
            if (!IsHost() || !PeerConnected() || !InMission(base) || !SecondPlayer()) return;
            const Fwd& f = Table[which];
            const uint32_t r[8] = { ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32, ctx.r9.u32, ctx.r10.u32 };
            // (the joining player's own weapon: his copy sounds it)
            for (int k = 0; k < 8; k++) if ((f.ptr >> k) & 1) if (r[k] && r[k] == SecondPlayer())
            {
                static int told = 0;
                if (told++ < 10) { printf("coop: host: the joining player's %s sounds (his own copy plays it): '%s'\n", f.what, (f.text >> 2) & 1 ? Text(base, r[2], 40).c_str() : ""); fflush(stdout); }
                return;
            }
            std::vector<uint8_t> m;
            m.push_back(E_CALL); m.push_back(uint8_t(which));
            for (int k = 0; k < 8; k++)
            {
                const uint8_t bit = uint8_t(1 << k);
                if (f.ref & bit) { const uint32_t i = r[k] & 0x3FFF; const uint32_t e = r[k] ? FromRef(base, r[k]) : 0; const uint16_t n = e ? HostNumber(base, e) : (i < 4096 ? uint16_t(i) : NoEntity); m.push_back(uint8_t(n)); m.push_back(uint8_t(n >> 8)); }
                else if (f.ptr & bit) { const uint16_t n = HostNumber(base, r[k]); m.push_back(uint8_t(n)); m.push_back(uint8_t(n >> 8)); }
                else if (f.broc & bit) PutText(m, Valid(r[k]) || (r[k] >= 0x82000000u && r[k] < 0x83000000u) ? BrocText(base, r[k]) : std::string());
                else if (f.text & bit) PutText(m, Text(base, r[k], 200));
                else if (f.vec & bit) { for (int c = 0; c < 3; c++) PutU32(m, Guest::Read32(base, r[k] + 4 * c)); }
                else PutU32(m, r[k]);
            }
            const double fl[4] = { ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64 };
            for (int k = 0; k < f.floats; k++) { const float v = float(fl[k]); uint32_t w; memcpy(&w, &v, 4); PutU32(m, w); }
            SendReliable(R_EVENT, m.data(), m.size());
            static int told = 0;
            if (told++ < 40) { printf("coop: host: %s for the joining copy too\n", f.what); fflush(stdout); }
        }

        // the joining copy: played once its world is the host's
        std::deque<std::vector<uint8_t>> g_pending;
    }

    // ---- (0.5.872) the host's effects on the joining copy ----
    // Every effect of the game is made by sub_82423F08 (r3 an id, r4 the effect's record, r5 a matrix of four
    // 16-byte rows with the place in the fourth, r6 and r7 entity references it follows, r8 a tag, r9 a flag).
    // The records are at the same addresses on both copies (test t871a: 14 of 14, the words before them too),
    // so the host sends the record's address with a sum of its first 48 words, and the joining copy plays it
    // only if its own record there adds up the same. An effect that follows a player is played where it was
    // made (the player's own copy draws his own; the host's first-person tags are not on his body there).
    namespace
    {
        struct FxMsg { uint32_t id, def, check, r8, r9; float f1; uint16_t a, b; uint8_t mat[64]; uint32_t lr; uint32_t wait; float born; };   // (0.5.889: wait: it follows a missile; born: when it came, the joining copy's clock)
        std::deque<FxMsg> g_fx;
        // (0.5.873) what makes the record: its first four words, its list (+80) and that list's count (+84). The
        // sum of 48 words (0.5.872) took in words that change as the game runs: 57 of 67 refused (test t872a)
        uint32_t FxCheck(const uint8_t* base, uint32_t def) { uint32_t s = 0; for (const uint32_t at : { 0u, 4u, 8u, 12u, 80u, 84u }) s = s * 31u + Guest::Read32(base, def + at); return s; }
        thread_local bool t_fxOurs = false;
        uint64_t g_fxSent = 0, g_fxPlayed = 0, g_fxRefused = 0;
    }
    void FxForward(PPCContext& ctx, uint8_t* base)
    {
        if (!IsHost() || t_fxOurs || !PeerConnected() || !InMission(base) || !SecondPlayer()) return;
        const uint32_t def = ctx.r4.u32, mat = ctx.r5.u32;
        if (!Valid(def) || !Valid(mat)) return;
        // (0.5.874) only from the two callers that give the whole matrix here: the event executor (sub_82533208)
        // and "an effect on a thing" (sub_8242F588). sub_824245D8 and sub_82429238/82429330 make the effect first
        // and place it after: theirs came with no place (0 0 0) or not a matrix at all (test t873a)
        const uint32_t lr = uint32_t(ctx.lr);
        // (0.5.876) sub_82429238 (a place and three axes) and sub_82429330 (a place) do give the whole matrix:
        // only the fourth number of each row is whatever was on their stack, which is what the joining copy's
        // check refused (t873a, and 24 of the event executor's own in t874a). Those four are not sent now.
        // sub_824245D8 alone makes the effect at 0 0 0 and hands back a handle its caller moves: still not sent,
        // counted by its own caller (the return address it saved, 168 above the stack here).
        const bool whole = (lr >= 0x82533208u && lr < 0x825338C0u) || (lr >= 0x8242F588u && lr < 0x8242F800u) || (lr >= 0x82429238u && lr < 0x824293E8u);
        if (!whole && lr >= 0x824245D8u && lr < 0x82424698u)
        {
            static std::map<uint32_t, uint32_t> by; static double at = 0;
            by[Guest::Read32(base, ctx.r1.u32 + 168)]++;
            if (Now() - at > 30) { at = Now(); std::string w; for (const auto& [a, n] : by) { char b[32]; snprintf(b, sizeof b, " %08X x%u", a, n); w += b; } printf("coop: host: effects made by handle (sub_824245D8), by caller:%s\n", w.c_str()); fflush(stdout); }
        }
        if (!whole) { static uint64_t skipped = 0; static double at = 0; skipped++; if (Now() - at > 30) { at = Now(); printf("coop: host: %llu effects not sent so far (made by callers that place them later; the last from %08X)\n", (unsigned long long)skipped, lr); fflush(stdout); } return; }
        // (at most 150 a second: a long burst is not worth a flood)
        static double windowAt = 0; static int inWindow = 0;
        const double now = Now();
        if (now - windowAt > 1.0) { windowAt = now; inWindow = 0; }
        if (++inWindow > 150) return;
        FxMsg m{};
        m.id = ctx.r3.u32; m.def = def; m.check = FxCheck(base, def); m.r8 = ctx.r8.u32; m.r9 = ctx.r9.u32; m.f1 = float(ctx.f1.f64); m.lr = lr;
        const uint32_t ea = ctx.r6.u32 ? FromRef(base, ctx.r6.u32) : 0, eb = ctx.r7.u32 ? FromRef(base, ctx.r7.u32) : 0;
        m.a = ctx.r6.u32 ? HostNumber(base, ea) : NoEntity; m.b = ctx.r7.u32 ? HostNumber(base, eb) : NoEntity;
        // (the joining player's own: his copy draws it)
        if (m.a == JoiningPlayer || m.b == JoiningPlayer) return;
        // (0.5.916) the blast of his own grenade: his copy plays its own
        { const float at[3] = { F(base, mat + 48), F(base, mat + 52), F(base, mat + 56) }; if (NearHisBlast(at)) { static int n = 0; if (n++ < 10) { printf("coop: host: an effect at the joining player's own grenade's blast: his copy plays its own\n"); fflush(stdout); } return; } }
        // (0.5.889) it follows a missile (a grenade in the air, its smoke): the joining copy waits for its puppet
        // (0.5.890) which of the two is the missile: 1 the first, 2 the second
        m.wait = ((ea && Valid(ea) && base[ea] == 3) ? 1u : 0u) | ((eb && Valid(eb) && base[eb] == 3) ? 2u : 0u);
        memcpy(m.mat, base + mat, 64);
        // (0.5.876) the rows' fourth numbers are not part of it (stack leftovers in most callers): 0 0 0 1
        { static const uint8_t zero[4] = { 0, 0, 0, 0 }, one[4] = { 0x3F, 0x80, 0, 0 }; for (int k = 0; k < 4; k++) memcpy(m.mat + 16 * k + 12, k == 3 ? one : zero, 4); }
        { static std::map<uint32_t, uint32_t> by; static double at = 0; by[lr]++; if (now - at > 30) { at = now; std::string w; for (const auto& [a, n] : by) { char b[32]; snprintf(b, sizeof b, " %08X x%u", a, n); w += b; } printf("coop: host: effects sent, by caller:%s\n", w.c_str()); fflush(stdout); } }
        SendUnreliable(K_FX, &m, sizeof m);
        g_fxSent++;
        static double toldAt = 0;
        if (now - toldAt > 20) { toldAt = now; printf("coop: host: %llu effects sent to the joining copy so far\n", (unsigned long long)g_fxSent); fflush(stdout); }
    }
    // ---- (0.5.881) the effects made by handle ----
    // sub_824245D8 has one caller, sub_820FBD00: it empties a list of waiting effects kept at 8290B5D0 (64-byte
    // entries, their count at +4096, room for 64): +0 the effect's record, +4 a time, +8 and +32 what
    // sub_820FE4D0 turns into the place it gives the new effect (sub_820E9D70), +48 a number put at +108 of the
    // effect, +52 the id. The host sends each waiting entry whole just before the game plays it; the joining
    // copy puts it into its own list and lets its own sub_820FBD00 play it.
    namespace
    {
        constexpr uint32_t FxQueue = 0x8290B5D0;
        struct FxQMsg { uint32_t magic, check; uint8_t entry[64]; };   // (72 bytes: told from FxMsg by its size)
        std::deque<FxQMsg> g_fxq;
        uint64_t g_fxqSent = 0, g_fxqPlayed = 0, g_fxqRefused = 0;
        thread_local bool t_fxqOurs = false;
    }
    void FxQueueForward(PPCContext& ctx, uint8_t* base)
    {
        if (!IsHost() || t_fxqOurs || !PeerConnected() || !InMission(base) || !SecondPlayer()) return;
        const uint32_t q = ctx.r3.u32;
        if (q != FxQueue) { static int told = 0; if (told++ < 3) { printf("coop: host: the waiting effects' list is at %08X, not where it was read\n", q); fflush(stdout); } return; }
        const uint32_t n = std::min<uint32_t>(Guest::Read32(base, q + 4096), 64);
        for (uint32_t k = 0; k < n; k++)
        {
            FxQMsg m{}; m.magic = 0x46585131u;
            memcpy(m.entry, base + q + 64u * k, 64);
            const uint32_t def = Guest::Read32(base, q + 64u * k);
            if (!Valid(def)) continue;
            // (0.5.916) debris of the joining player's own grenade's blast (+32..+40 the place): his copy makes its own
            { const float at[3] = { F(base, q + 64u * k + 32u), F(base, q + 64u * k + 36u), F(base, q + 64u * k + 40u) }; if (NearHisBlast(at)) continue; }
            m.check = FxCheck(base, def);
            SendUnreliable(K_FX, &m, sizeof m);
            g_fxqSent++;
            static int told = 0;
            if (told++ < 6) { std::string w; for (int i = 0; i < 16; i++) { char b[40]; const uint32_t x = Guest::Read32(base, q + 64u * k + 4u * i); if (i >= 1 && i <= 12) snprintf(b, sizeof b, " %g", double(F(base, q + 64u * k + 4u * i))); else snprintf(b, sizeof b, " %08X", x); w += b; } printf("coop: host: a waiting effect sent:%s\n", w.c_str()); fflush(stdout); }
        }
        static double toldAt = 0;
        if (n && Now() - toldAt > 20) { toldAt = Now(); printf("coop: host: %llu waiting effects (made by handle) sent to the joining copy so far\n", (unsigned long long)g_fxqSent); fflush(stdout); }
    }
    void FxReceive(const uint8_t* data, size_t size)
    {
        if (size == sizeof(FxQMsg)) { FxQMsg q; memcpy(&q, data, sizeof q); if (q.magic == 0x46585131u && g_fxq.size() < 128) g_fxq.push_back(q); return; }
        if (size < sizeof(FxMsg)) return;
        FxMsg m; memcpy(&m, data, sizeof m);
        m.born = float(Now());
        if (g_fx.size() < 256) g_fx.push_back(m);
    }
    namespace
    {
        void FxFrame(PPCContext& ctx, uint8_t* base)
        {
            static uint32_t mem = 0;   // a matrix, on a 16-byte line (the game loads it as vectors)
            if (!mem) { mem = (GuestAlloc(256) + 15u) & ~15u; }
            int n = 0;
            std::vector<FxMsg> later;   // (0.5.889) those waiting for a missile's puppet
            while (!g_fx.empty() && n++ < 48)
            {
                const FxMsg m = g_fx.front(); g_fx.pop_front();
                if (!Valid(m.def) || FxCheck(base, m.def) != m.check)
                {
                    g_fxRefused++;
                    static int told = 0;
                    if (told++ < 10) { printf("coop: join: an effect of the host's not played: the record at %08X is not the same here\n", m.def); fflush(stdout); }
                    continue;
                }
                memcpy(base + mem, m.mat, 64);
                for (int k = 0; k < 4; k++) SetF(base, mem + 16u * k + 12u, k == 3 ? 1.0f : 0.0f);   // (0.5.876)
                bool ok = true;
                for (int k = 0; k < 16; k++) { const float f = F(base, mem + 4u * k); if (!std::isfinite(f) || fabsf(f) > 200000.0f) ok = false; }
                if (!ok)
                {
                    g_fxRefused++;
                    static int told = 0;
                    if (told++ < 12) { std::string w; for (int k = 0; k < 16; k++) { char b[16]; snprintf(b, sizeof b, " %g", double(F(base, mem + 4u * k))); w += b; } printf("coop: join: an effect of the host's (%08X, from %08X) not played: its matrix is not right:%s\n", m.def, m.lr, w.c_str()); fflush(stdout); }
                    continue;
                }
                // what it follows: our puppets for the host's two things, else nothing (played where it was made)
                uint32_t ra = 0, rb = 0;
                const bool player = m.a == HostPlayer || m.b == HostPlayer || m.a == JoiningPlayer || m.b == JoiningPlayer;
                if (!player)
                {
                    const uint32_t ea = m.a != NoEntity ? JoinLocalFor(base, m.a) : 0, eb = m.b != NoEntity ? JoinLocalFor(base, m.b) : 0;
                    const bool haveA = m.a == NoEntity || ea, haveB = m.b == NoEntity || eb;
                    // (0.5.889) THE SMOKE THAT STOOD IN THE AIR (the user: the joining player's smoke grenade "stays
                    // still in the air and never lands"): an effect that follows a grenade came before the grenade's
                    // puppet was made here, and was played where it was made, the throw's place in the air. It waits
                    // for the puppet now, a second at most.
                    // (0.5.890) test t889a: the user's smoke still stood in the air: "(0052 012C): NO puppet" though the
                    // grenade's puppet (012C) was there: the effect names a second thing (0052) that has no puppet
                    // here, and for want of that one it followed nothing. It waits for the missile's puppet only,
                    // and follows whichever of the two is here.
                    const bool missileHere = (!(m.wait & 1u) || ea) && (!(m.wait & 2u) || eb);
                    if (m.wait && !missileHere && float(Now()) - m.born < 1.0f) { later.push_back(m); continue; }
                    if (m.wait) { static int toldW = 0; if (toldW++ < 20) { printf("coop: join: an effect that follows a missile (%04X %04X, missile %u): %s after %.2f s\n", m.a, m.b, m.wait, missileHere ? (haveA && haveB ? "its puppet is here" : "the missile's puppet is here, the other thing has none") : "NO puppet, played where it was made", double(float(Now()) - m.born)); fflush(stdout); } }
                    {
                        if (ea && IndexOf(base, ea) >= 0) ra = RefOf(base, uint32_t(IndexOf(base, ea)));
                        if (eb && IndexOf(base, eb) >= 0) rb = RefOf(base, uint32_t(IndexOf(base, eb)));
                    }
                }
                PPCContext c = CallCtx(ctx);
                c.r3.u64 = m.id; c.r4.u64 = m.def; c.r5.u64 = mem; c.r6.u64 = ra; c.r7.u64 = rb; c.r8.u64 = m.r8; c.r9.u64 = m.r9; c.f1.f64 = m.f1;
                t_fxOurs = true;
                CallGuest(c, base, 0x82423F08);
                t_fxOurs = false;
                g_fxPlayed++;
                static int told = 0;
                if (told++ < 60) { printf("coop: join: the host's effect %08X played at %.0f %.0f %.0f (follows %04X %04X -> %08X %08X, from %08X)\n", m.def, F(base, mem + 48), F(base, mem + 52), F(base, mem + 56), m.a, m.b, ra, rb, m.lr); fflush(stdout); }
            }
            for (const FxMsg& w : later) g_fx.push_back(w);
            // (0.5.881) the host's waiting effects: into our own list, played by the game's own sub_820FBD00
            if (!g_fxq.empty())
            {
                uint32_t have = Guest::Read32(base, FxQueue + 4096);
                int put = 0;
                while (!g_fxq.empty() && have < 64)
                {
                    const FxQMsg q = g_fxq.front(); g_fxq.pop_front();
                    uint32_t def; memcpy(&def, q.entry, 4); def = __builtin_bswap32(def);
                    bool ok = Valid(def) && FxCheck(base, def) == q.check;
                    for (int i = 1; ok && i <= 12; i++) { uint32_t w; memcpy(&w, q.entry + 4 * i, 4); w = __builtin_bswap32(w); float f; memcpy(&f, &w, 4); if (!std::isfinite(f) || fabsf(f) > 200000.0f) ok = false; }
                    if (!ok) { g_fxqRefused++; static int told = 0; if (told++ < 8) { printf("coop: join: a waiting effect of the host's not played (record %08X or its numbers not right)\n", def); fflush(stdout); } continue; }
                    memcpy(base + FxQueue + 64u * have, q.entry, 64);
                    have++; put++;
                }
                if (put)
                {
                    Guest::Write32(base, FxQueue + 4096, have);
                    PPCContext c = CallCtx(ctx); c.r3.u64 = FxQueue;
                    t_fxqOurs = true; CallGuest(c, base, 0x820FBD00); t_fxqOurs = false;
                    g_fxqPlayed += uint64_t(put);
                }
            }
            static double toldQ = 0;
            if ((g_fxqPlayed || g_fxqRefused) && Now() - toldQ > 20) { toldQ = Now(); printf("coop: join: %llu of the host's waiting effects played, %llu not\n", (unsigned long long)g_fxqPlayed, (unsigned long long)g_fxqRefused); fflush(stdout); }
            static double toldAt = 0;
            if (Now() - toldAt > 20) { toldAt = Now(); printf("coop: join: %llu of the host's effects played, %llu not (record or place not right)\n", (unsigned long long)g_fxPlayed, (unsigned long long)g_fxRefused); fflush(stdout); }
        }
    }

    void EventsReceive(const uint8_t* data, size_t size)
    {
        if (size < 2 || data[0] != E_CALL || data[1] >= TableSize) return;
        if (g_pending.size() < 512) g_pending.emplace_back(data, data + size);
    }

    void EventsFrame(PPCContext& ctx, uint8_t* base, bool frozen)
    {
        if (!frozen) { g_pending.clear(); g_fx.clear(); g_fxq.clear(); return; }
        FxFrame(ctx, base);
        static uint32_t slots = 0;   // guest memory for the strings' slots, the vectors and the results
        if (!slots) { slots = GuestAlloc(4096); memset(base + slots, 0, 4096); }
        int played = 0;
        while (!g_pending.empty() && played < 64)
        {
            const std::vector<uint8_t> m = std::move(g_pending.front());
            g_pending.pop_front();
            played++;
            const Fwd& f = Table[m[1]];
            size_t at = 2;
            uint32_t r[8] = {};
            uint32_t mem = slots + 64;   // (the first 64 bytes: the result slot)
            bool ok = true;
            auto getU16 = [&](uint16_t& v) { if (at + 2 > m.size()) { ok = false; return; } v = uint16_t(m[at] | (m[at + 1] << 8)); at += 2; };
            auto getU32 = [&](uint32_t& v) { if (at + 4 > m.size()) { ok = false; return; } memcpy(&v, &m[at], 4); at += 4; };
            auto getText = [&](std::string& t) { uint16_t n = 0; getU16(n); if (!ok || at + n > m.size()) { ok = false; return; } t.assign(reinterpret_cast<const char*>(&m[at]), n); at += n; };
            for (int k = 0; k < 8 && ok; k++)
            {
                const uint8_t bit = uint8_t(1 << k);
                if ((f.ref | f.ptr) & bit)
                {
                    uint16_t n = 0; getU16(n);
                    if (n == NoEntity) { r[k] = 0; continue; }
                    const uint32_t e = JoinLocalFor(base, n);
                    if (!e) { ok = false; break; }   // (a thing not drawn here: not played)
                    r[k] = (f.ref & bit) ? RefOf(base, uint32_t(IndexOf(base, e))) : e;
                }
                else if (f.broc & bit) { std::string t; getText(t); r[k] = GuestBroc(base, t); }
                else if (f.text & bit) { std::string t; getText(t); r[k] = GuestText(base, t); }
                else if (f.vec & bit) { for (int c = 0; c < 3; c++) { uint32_t w = 0; getU32(w); Guest::Write32(base, mem + 4 * c, w); } r[k] = mem; mem += 16; }
                else getU32(r[k]);
                if (f.zero & bit) r[k] = 0;
                if (f.out & bit) r[k] = slots;
            }
            float fl[4] = {};
            for (int k = 0; k < f.floats && ok; k++) { uint32_t w = 0; getU32(w); memcpy(&fl[k], &w, 4); }
            if (!ok) continue;
            PPCContext c = CallCtx(ctx);
            c.r3.u64 = r[0]; c.r4.u64 = r[1]; c.r5.u64 = r[2]; c.r6.u64 = r[3]; c.r7.u64 = r[4]; c.r8.u64 = r[5]; c.r9.u64 = r[6]; c.r10.u64 = r[7];
            c.f1.f64 = fl[0]; c.f2.f64 = fl[1]; c.f3.f64 = fl[2]; c.f4.f64 = fl[3];
            CallGuest(c, base, f.func);
            static int told = 0;
            if (told++ < 60) { printf("coop: join: the host's %s, here too\n", f.what); fflush(stdout); }
        }
    }
}

using namespace Coop;

#define COOP_FORWARD(addr, index) \
    extern "C" PPC_FUNC(__imp__sub_##addr); \
    PPC_FUNC(sub_##addr) { Forward(ctx, base, index); __imp__sub_##addr(ctx, base); }

COOP_FORWARD(824B51E0, 0)
COOP_FORWARD(824B51E8, 1)
COOP_FORWARD(824B2E28, 2)
COOP_FORWARD(824B3018, 3)
COOP_FORWARD(824AD260, 4)
COOP_FORWARD(824AD238, 5)
COOP_FORWARD(824A8498, 6)
COOP_FORWARD(824ACE70, 7)
COOP_FORWARD(824A7C28, 8)
COOP_FORWARD(82534ED0, 9)

#endif
