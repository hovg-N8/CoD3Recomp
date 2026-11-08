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
#include <cstdio>
#include <cstring>
#include <deque>
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

    void EventsReceive(const uint8_t* data, size_t size)
    {
        if (size < 2 || data[0] != E_CALL || data[1] >= TableSize) return;
        if (g_pending.size() < 512) g_pending.emplace_back(data, data + size);
    }

    void EventsFrame(PPCContext& ctx, uint8_t* base, bool frozen)
    {
        if (!frozen) { g_pending.clear(); return; }
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
