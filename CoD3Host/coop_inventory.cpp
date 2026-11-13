// Co-op (0.5.901): the joining player's weapons and ammunition are the host's to keep.
//
// What was wrong (t899u Saint-Lo, t900a Forest): the host's second player (client 1) owned NO weapon. His
// record is made by the game's own ClientBegin, and only the level's scripts hand out guns, to the host's
// player alone. His commands asked for weapon 12 (his record's +1356) and his weapon (+132) stayed 0: on
// the host he fired nothing and threw nothing, so the joining player's shots and grenades were on his own
// screen only and the mission never saw them.
//
// A player's record (campaign, from the code: sub_82477948, sub_8246FB98 and 80 more):
//   +288 + 4 * (weapon +1460)   rounds in reserve, by the weapon's ammunition number
//   +656 + 4 * (weapon +1468)   rounds in the clip, by its clip number (grenades are counted here)
//   +1024, five words           the weapons he owns, a bit each
//   +1044                       bolt to be worked, a bit each (each copy's own, not touched)
//
// The host: client 1 with no weapon at all gets the host's player's whole set; from then on what the
// host's player is GIVEN (a new weapon, more rounds that are not a reload) client 1 is given too. What
// client 1 has is sent to his copy (R_INV), which takes it as its own.
#ifndef COD3_MP

#include "coop.h"
#include "input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace Coop
{
    namespace
    {
        constexpr uint32_t Owned = 1024, OwnedWords = 5, CountsFrom = 288, CountsTo = 1024;

        struct WeaponSlots { uint32_t number; uint32_t reserve, clip; std::string name; };
        // every weapon of the mission's list [0x82A2A2E0] with its two places in a record
        std::vector<WeaponSlots> Weapons(const uint8_t* base)
        {
            std::vector<WeaponSlots> out;
            const uint32_t list = Guest::Read32(base, 0x82A2A2E0u);
            if (!Valid(list)) return out;
            for (uint32_t v = 1; v < 160; v++)
            {
                const uint32_t def = Guest::Read32(base, list + 4 * v);
                if (!Valid(def)) continue;
                const uint32_t text = Guest::Read32(base, def + 8);
                if (!Valid(text)) continue;
                const uint32_t a = Guest::Read32(base, def + 1460), c = Guest::Read32(base, def + 1468);
                if (a >= 92 || c >= 92) continue;
                out.push_back(WeaponSlots{ v, CountsFrom + 4 * a, 656 + 4 * c, Text(base, text, 40) });
            }
            return out;
        }
        std::vector<uint32_t> Places(const std::vector<WeaponSlots>& ws)
        {
            std::vector<uint32_t> p;
            for (const auto& w : ws) { p.push_back(w.reserve); p.push_back(w.clip); }
            std::sort(p.begin(), p.end()); p.erase(std::unique(p.begin(), p.end()), p.end());
            return p;
        }
        bool Owns(const uint8_t* base, uint32_t rec, uint32_t v) { return (Guest::Read32(base, rec + Owned + 4 * (v >> 5)) >> (v & 31)) & 1; }
        bool OwnsAny(const uint8_t* base, uint32_t rec) { for (uint32_t k = 0; k < OwnedWords; k++) if (Guest::Read32(base, rec + Owned + 4 * k)) return true; return false; }
        std::string Names(const uint8_t* base, uint32_t rec, const std::vector<WeaponSlots>& ws)
        {
            std::string s;
            for (const auto& w : ws) if (Owns(base, rec, w.number)) { char b[96]; snprintf(b, sizeof b, " %s (%d in the clip, %d more)", w.name.c_str(), int32_t(Guest::Read32(base, rec + w.clip)), int32_t(Guest::Read32(base, rec + w.reserve))); s += b; }
            return s.empty() ? " nothing" : s;
        }

        // ---- the host ----
        uint32_t g_forRec = 0;                       // the client 1 record the "before" below is for
        std::map<uint32_t, int32_t> g_before;        // the host's player's counts a frame ago
        uint32_t g_ownedBefore[OwnedWords] = {};
        std::vector<uint8_t> g_sent; double g_sentAt = -10;
        std::vector<uint8_t> g_his;                  // (0.5.902) what the joining copy says it owns, until taken
        int g_told = 0;

        // ---- the joining copy ----
        std::vector<uint8_t> g_got; double g_gotAt = -1;
        std::map<uint32_t, double> g_differsSince;
        uint32_t g_mineSent[OwnedWords] = {}; bool g_mineEver = false; double g_mineAt = -10, g_mineMoreAt = -10;   // (0.5.902) what I told the host I own
        double g_goneSince[160] = {};
        uint32_t g_ownGain[OwnedWords] = {}, g_lastOwned[OwnedWords] = {}; bool g_lastOwnedKnown = false;   // (0.5.951)
        uint32_t g_swapTarget = 0;   // (0.5.957) the gun a swap on the host gave me: to be put in my hands at once
    }

    void InventoryHostFrame(uint8_t* base, uint32_t hostRec, uint32_t secondRec)
    {
        if (!hostRec || !secondRec) { g_forRec = 0; return; }
        const std::vector<WeaponSlots> ws = Weapons(base);
        if (ws.empty()) return;
        const std::vector<uint32_t> places = Places(ws);
        const bool fresh = g_forRec != secondRec;
        // 1. he owns nothing (just made, or made again): the host's player's whole set
        if (!OwnsAny(base, secondRec) && OwnsAny(base, hostRec))
        {
            for (const uint32_t p : places) Guest::Write32(base, secondRec + p, Guest::Read32(base, hostRec + p));
            for (uint32_t k = 0; k < OwnedWords; k++) Guest::Write32(base, secondRec + Owned + 4 * k, Guest::Read32(base, hostRec + Owned + 4 * k));
            if (g_told < 80) { g_told++; printf("coop: inventory: the joining player owned no weapon here: given the host's player's:%s\n", Names(base, secondRec, ws).c_str()); fflush(stdout); }
        }
        // (0.5.902) what his own copy owns and he does not have here (its loadout at the start, a pickup there):
        // given here, with his copy's counts. (Saint-Lo's training: the host's player starts with nothing, and
        // 0.5.901 took the joining player's guns away for it.)
        if (!g_his.empty())
        {
            const uint8_t* d = g_his.data();
            std::map<uint32_t, uint32_t> count;
            uint16_t n; memcpy(&n, d + 4 * OwnedWords + 4, 2);
            size_t at = 4 * OwnedWords + 6;
            for (uint32_t i = 0; i < n && at + 6 <= g_his.size(); i++, at += 6) { uint16_t p; uint32_t v; memcpy(&p, d + at, 2); memcpy(&v, d + at + 2, 4); count[p] = v; }
            for (const auto& w : ws)
            {
                uint32_t word; memcpy(&word, d + 4 * (w.number >> 5), 4);
                if (!((word >> (w.number & 31)) & 1) || Owns(base, secondRec, w.number)) continue;
                const uint32_t bits = secondRec + Owned + 4 * (w.number >> 5);
                Guest::Write32(base, bits, Guest::Read32(base, bits) | (1u << (w.number & 31)));
                for (const uint32_t p : { w.reserve, w.clip }) if (count.count(p) && int32_t(Guest::Read32(base, secondRec + p)) < int32_t(count[p])) Guest::Write32(base, secondRec + p, count[p]);
                if (g_told < 80) { g_told++; printf("coop: inventory: the joining player's own copy has %s: his here too (%d in the clip, %d more)\n", w.name.c_str(), int32_t(Guest::Read32(base, secondRec + w.clip)), int32_t(Guest::Read32(base, secondRec + w.reserve))); fflush(stdout); }
            }
            g_his.clear();
        }
        // 2. what the host's player was given since the last frame, his too
        // (0.5.907) a SWAP is the host's player's own: a gun gained in the same frame as one lost is a pickup off
        // the ground (one gun for another), and in co-op each player's pickups are his own. Before, the joining
        // player was given the new gun and kept the old one too. Only gains with nothing lost (the scripts' gives:
        // the training's rifle, a level's start) are both players'.
        bool swapped = false;
        if (!fresh)
            for (uint32_t k = 0; k < OwnedWords; k++) if (g_ownedBefore[k] & ~Guest::Read32(base, hostRec + Owned + 4 * k)) swapped = true;
        if (swapped && g_told < 80)
        {
            g_told++;
            std::string lost, got;
            for (const auto& w : ws)
            {
                const bool had = (g_ownedBefore[w.number >> 5] >> (w.number & 31)) & 1, has = Owns(base, hostRec, w.number);
                if (had && !has) lost += " " + w.name;
                if (!had && has) got += " " + w.name;
            }
            printf("coop: inventory: the host's player gave up%s%s%s: his own swap, the joining player keeps his\n", lost.c_str(), got.empty() ? "" : " for", got.c_str()); fflush(stdout);
        }
        if (!fresh && !swapped)
        {
            for (const auto& w : ws)
            {
                if (!Owns(base, hostRec, w.number) || ((g_ownedBefore[w.number >> 5] >> (w.number & 31)) & 1) || Owns(base, secondRec, w.number)) continue;
                const uint32_t at = secondRec + Owned + 4 * (w.number >> 5);
                Guest::Write32(base, at, Guest::Read32(base, at) | (1u << (w.number & 31)));
                for (const uint32_t p : { w.reserve, w.clip }) if (int32_t(Guest::Read32(base, secondRec + p)) < int32_t(Guest::Read32(base, hostRec + p))) Guest::Write32(base, secondRec + p, Guest::Read32(base, hostRec + p));
                if (g_told < 80) { g_told++; printf("coop: inventory: the host's player was given %s: the joining player too (%d in the clip, %d more)\n", w.name.c_str(), int32_t(Guest::Read32(base, secondRec + w.clip)), int32_t(Guest::Read32(base, secondRec + w.reserve))); fflush(stdout); }
            }
            // more rounds in one place with no other place less by as many (that would be a reload)
            for (const uint32_t p : places)
            {
                const int32_t now = int32_t(Guest::Read32(base, hostRec + p)), was = g_before.count(p) ? g_before[p] : now;
                const int32_t more = now - was;
                if (more <= 0) continue;
                bool moved = false;
                for (const uint32_t q : places) if (q != p && g_before.count(q) && g_before[q] - int32_t(Guest::Read32(base, hostRec + q)) == more) { moved = true; break; }
                if (moved) continue;
                const int32_t his = int32_t(Guest::Read32(base, secondRec + p));
                const int32_t to = std::min(his + more, std::max(his, now));
                if (to == his) continue;
                Guest::Write32(base, secondRec + p, uint32_t(to));
                if (g_told < 80) { g_told++; printf("coop: inventory: the host's player was given %d round(s) at +%u (now %d): the joining player's %d -> %d\n", more, p, now, his, to); fflush(stdout); }
            }
        }
        g_forRec = secondRec;
        g_before.clear();
        for (const uint32_t p : places) g_before[p] = int32_t(Guest::Read32(base, hostRec + p));
        for (uint32_t k = 0; k < OwnedWords; k++) g_ownedBefore[k] = Guest::Read32(base, hostRec + Owned + 4 * k);
        // 3. what he has here, to his copy: when it changes (ten times a second at most) and every two seconds
        std::vector<uint8_t> m;
        auto put32 = [&](uint32_t v) { const uint8_t* b = reinterpret_cast<const uint8_t*>(&v); m.insert(m.end(), b, b + 4); };
        auto put16 = [&](uint16_t v) { const uint8_t* b = reinterpret_cast<const uint8_t*>(&v); m.insert(m.end(), b, b + 2); };
        for (uint32_t k = 0; k < OwnedWords; k++) put32(Guest::Read32(base, secondRec + Owned + 4 * k));
        put32(Guest::Read32(base, secondRec + 132));
        put16(uint16_t(places.size()));
        for (const uint32_t p : places) { put16(uint16_t(p)); put32(Guest::Read32(base, secondRec + p)); }
        const double now = Now();
        if ((m != g_sent && now - g_sentAt > 0.1) || now - g_sentAt > 2.0)
        {
            static uint32_t weaponWas = 0xFFFFFFFFu;
            const uint32_t weapon = Guest::Read32(base, secondRec + 132);
            if (weapon != weaponWas && g_told < 80)
            {
                g_told++; weaponWas = weapon;
                std::string name = "none";
                for (const auto& w : ws) if (w.number == weapon) name = w.name;
                printf("coop: inventory: the joining player holds weapon %u (%s) here; his command asks for %u; he owns:%s\n", weapon, name.c_str(), Guest::Read32(base, secondRec + 1356), Names(base, secondRec, ws).c_str()); fflush(stdout);
            }
            g_sent = m; g_sentAt = now;
            SendReliable(R_INV, m.data(), m.size());
        }
    }

    void InventoryHostReceive(const uint8_t* data, size_t size)
    {
        if (size < 4 * OwnedWords + 6) return;
        g_his.assign(data, data + size);
    }

    void InventoryJoinReceive(const uint8_t* data, size_t size)
    {
        if (size < 4 * OwnedWords + 6) return;
        g_got.assign(data, data + size); g_gotAt = Now();
    }

    // the joining copy, each frame of the host's world: the host's word for what I own, and for a count once
    // mine has differed from his for 0.6 s (my own shots count down here first; his come a moment later)
    void InventoryJoinFrame(uint8_t* base, uint32_t rec)
    {
        if (!rec) return;
        static int told = 0;
        const double now = Now();
        // (0.5.902) what I own here, to the host: at the start, whenever I own something I did not (a pickup),
        // and every five seconds
        {
            // (0.5.951) only what I gained HERE (my start, a pickup of my copy's own), never what the host gave me: t950a,
            // his pickup on the host swapped his Springfield for a Gewehr 43, my copy (given the Gewehr) told the whole
            // list again, Springfield and all, and the host gave the Springfield back (two rifles and one on the ground).
            uint32_t mine[OwnedWords]; bool more = !g_mineEver;
            for (uint32_t k = 0; k < OwnedWords; k++)
            {
                const uint32_t o = Guest::Read32(base, rec + Owned + 4 * k);
                g_ownGain[k] |= g_lastOwnedKnown ? (o & ~g_lastOwned[k]) : o;
                g_ownGain[k] &= o;
                g_lastOwned[k] = o;
                mine[k] = g_ownGain[k];
                if (mine[k] & ~g_mineSent[k]) more = true;
            }
            g_lastOwnedKnown = true;
            if (more || now - g_mineAt > 5.0)
            {
                const std::vector<WeaponSlots> ws = Weapons(base);
                if (!ws.empty())
                {
                    const std::vector<uint32_t> places = Places(ws);
                    std::vector<uint8_t> m;
                    auto put32 = [&](uint32_t v) { const uint8_t* b = reinterpret_cast<const uint8_t*>(&v); m.insert(m.end(), b, b + 4); };
                    auto put16 = [&](uint16_t v) { const uint8_t* b = reinterpret_cast<const uint8_t*>(&v); m.insert(m.end(), b, b + 2); };
                    for (uint32_t k = 0; k < OwnedWords; k++) put32(mine[k]);
                    put32(Guest::Read32(base, rec + 132));
                    put16(uint16_t(places.size()));
                    for (const uint32_t p : places) { put16(uint16_t(p)); put32(Guest::Read32(base, rec + p)); }
                    SendReliable(R_INVMINE, m.data(), m.size());
                    if (more && told < 60) { told++; printf("coop: inventory: told the host what I own:%s\n", Names(base, rec, ws).c_str()); fflush(stdout); }
                    if (more) g_mineMoreAt = now;
                    g_mineAt = now;
                    memcpy(g_mineSent, mine, sizeof mine); g_mineEver = true;
                }
            }
        }
        // (0.5.948) a gun given to me while I hold none (t947v: the training's Garand, given by the host's scripts,
        // which switch only THEIR player to it): drawn the way the player would, with the weapon cycle (Y), so my
        // copy raises it and my command asks the host for it. Only after a new gun bit, at most three taps.
        {
            static uint32_t ownedWas[OwnedWords] = {}; static bool ownedKnown = false;
            static int tapsLeft = 0; static double tapAt = -10;
            bool gained = false;
            for (uint32_t k = 0; k < OwnedWords; k++) { const uint32_t o = Guest::Read32(base, rec + Owned + 4 * k); if (ownedKnown && (o & ~ownedWas[k])) gained = true; ownedWas[k] = o; }
            ownedKnown = true;
            const uint32_t held = Guest::Read32(base, rec + 132);
            if (gained && held == 0) { tapsLeft = 3; tapAt = now - 10; }
            // (0.5.952) or hands emptied while I own a gun (t951a: a pickup's swap on the host took the gun I held and
            // gave the new one; my copy held weapon 0 after, the host's too, as its command asked for the old one).
            // Once each time my hands go empty, after 0.5 s.
            static double emptySince = -1; static bool emptyTapped = false;
            if (held != 0) { emptySince = -1; emptyTapped = false; }
            else if (emptySince < 0) emptySince = now;
            if (held == 0 && !emptyTapped && emptySince >= 0 && now - emptySince > 0.2 && OwnsAny(base, rec)) { emptyTapped = true; if (tapsLeft == 0) { tapsLeft = 3; tapAt = now - 10; } }
            // (0.5.957) a swap on the host (a gun gained as one is lost): pressed at once and again every 0.45 s till the
            // gun picked up is the one in my hands (the user: the switch "felt slow")
            static uint32_t target = 0;
            if (g_swapTarget) { target = g_swapTarget; g_swapTarget = 0; tapsLeft = 4; tapAt = now - 10; }
            if (target ? (held == target || !Owns(base, rec, target)) : held != 0) { tapsLeft = 0; target = 0; }
            if (tapsLeft > 0 && now - tapAt > (target ? 0.45 : 0.8))
            {
                tapsLeft--; tapAt = now;
                Input::PressWeaponCycle();
                if (told < 60) { told++; printf("coop: inventory: a gun given and none in my hands: the weapon cycle pressed (%d more at most)\n", tapsLeft); fflush(stdout); }
            }
            static uint32_t heldWas = 0xFFFFFFFFu;
            if (held != heldWas && told < 60) { told++; heldWas = held; printf("coop: inventory: I hold weapon %u now\n", held); fflush(stdout); }
        }
        // (the host needs a moment to take what I told it: his word counts from 2 s after that)
        if (g_got.empty() || now - g_gotAt > 6.0 || now - g_mineMoreAt < 2.0) return;
        const uint8_t* d = g_got.data();
        bool ownedChanged = false;
        // what the host has for me and I lack: mine at once. What I have and the host lacks for 2 s: gone. (0.5.957) Gone
        // at once when the same word gives me a gun I lack (a swap on the host), and that gun is the one to hold.
        uint32_t gainedGun = 0; bool lostGun = false;
        for (uint32_t v = 1; v < 160; v++)
        {
            uint32_t w; memcpy(&w, d + 4 * (v >> 5), 4);
            const bool his = (w >> (v & 31)) & 1, mine = Owns(base, rec, v);
            if (his && !mine && !gainedGun) gainedGun = v;   // (0.5.958) the first: t957a gave 11 (the Gewehr 43) and 81 with it, never held
            if (!his && mine) lostGun = true;
        }
        const bool swapWord = gainedGun && lostGun;
        if (swapWord) { g_swapTarget = gainedGun; if (told < 60) { told++; printf("coop: inventory: a swap on the host: weapon %u to my hands now\n", gainedGun); fflush(stdout); } }
        for (uint32_t v = 1; v < 160; v++)
        {
            uint32_t w; memcpy(&w, d + 4 * (v >> 5), 4);
            const bool his = (w >> (v & 31)) & 1, mine = Owns(base, rec, v);
            if (his == mine) { g_goneSince[v] = 0; continue; }
            const uint32_t bits = rec + Owned + 4 * (v >> 5);
            const uint32_t bit = 1u << (v & 31);
            if (his) { Guest::Write32(base, bits, Guest::Read32(base, bits) | bit); g_lastOwned[v >> 5] |= bit; g_ownGain[v >> 5] &= ~bit; ownedChanged = true; continue; }
            if (!swapWord)
            {
                if (g_goneSince[v] == 0) { g_goneSince[v] = now; continue; }
                if (now - g_goneSince[v] < 2.0) continue;
            }
            Guest::Write32(base, bits, Guest::Read32(base, bits) & ~bit); g_lastOwned[v >> 5] &= ~bit; g_ownGain[v >> 5] &= ~bit; g_goneSince[v] = 0; ownedChanged = true;
        }
        // (0.5.951) the gun the HOST put in my hands (a pickup off the ground swaps and raises the new one there; a level's
        // script switches its player): my copy follows with the weapon cycle (Y), the game's own way, until it holds the
        // same. Only when the host's changed while mine had not for a second (never against my own switch on its way).
        {
            static uint32_t hostHeldWas = 0, myHeldWas = 0; static double myHeldAt = -10, chaseAt = -10; static int chaseLeft = 0;
            uint32_t hostHeld; memcpy(&hostHeld, d + 4 * OwnedWords, 4);
            const uint32_t held = Guest::Read32(base, rec + 132);
            if (held != myHeldWas) { myHeldWas = held; myHeldAt = now; }
            if (hostHeld != hostHeldWas)
            {
                hostHeldWas = hostHeld;
                if (hostHeld >= 1 && hostHeld < 160 && hostHeld != held && now - myHeldAt > 1.0) { chaseLeft = 3; chaseAt = now - 10; if (told < 60) { told++; printf("coop: inventory: the host put weapon %u in my hands (I hold %u): following\n", hostHeld, held); fflush(stdout); } }
            }
            if (chaseLeft > 0 && (held == hostHeld || !Owns(base, rec, hostHeld))) chaseLeft = 0;
            if (chaseLeft > 0 && now - chaseAt > 0.9)
            {
                chaseLeft--; chaseAt = now;
                Input::PressWeaponCycle();
                if (told < 60) { told++; printf("coop: inventory: the weapon cycle pressed to reach weapon %u (I hold %u; %d more at most)\n", hostHeld, held, chaseLeft); fflush(stdout); }
            }
        }
        uint16_t n; memcpy(&n, d + 4 * OwnedWords + 4, 2);
        size_t at = 4 * OwnedWords + 6;
        for (uint32_t i = 0; i < n && at + 6 <= g_got.size(); i++, at += 6)
        {
            uint16_t p; uint32_t v; memcpy(&p, d + at, 2); memcpy(&v, d + at + 2, 4);
            if (p < CountsFrom || p >= CountsTo || (p & 3)) continue;
            const uint32_t mine = Guest::Read32(base, rec + p);
            if (mine == v) { g_differsSince.erase(p); continue; }
            auto it = g_differsSince.find(p);
            if (it == g_differsSince.end()) { g_differsSince[p] = now; continue; }
            if (now - it->second < 0.6) continue;
            Guest::Write32(base, rec + p, v);
            g_differsSince.erase(it);
            if (told < 60) { told++; printf("coop: inventory: my count at +%u was %d, the host has %d for me: taken\n", p, int32_t(mine), int32_t(v)); fflush(stdout); }
        }
        if (ownedChanged && told < 60)
        {
            told++;
            printf("coop: inventory: what I own is the host's word now:%s\n", Names(base, rec, Weapons(base)).c_str()); fflush(stdout);
        }
    }

}
#endif
