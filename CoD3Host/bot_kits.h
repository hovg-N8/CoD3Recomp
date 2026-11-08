#pragma once
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

// 0.5.198: the bots' kits, from bot_kits.txt beside the executable (Justin:
// customize the bots' loadouts completely, campaign guns included, and more
// grenades). Each line is one kit: how often (a weight), the game's class it
// builds on (its items, ability, model and pistol), the main weapon (any of
// the custom class list, campaign guns included; "-" for the class's own),
// how many frag grenades and how many smoke grenades. New bots take the kit
// their side has least of, by weight. Pure logic, tested on its own
// (docs/build-logs/test_bot_kits_198.cpp).
namespace BotKits {

struct Kit {
    unsigned weight = 1;
    uint16_t base = 0;          // the game's class, 0..6
    char primary[24] = {};      // "" : the class's own
    int frags = -1;             // -1: the kit's own count
    int smokes = -1;            // -1: the kit's own (most classes have none)
    // 0.5.284: optional columns: a second main weapon in the pistol's place
    // ("-" none), and the grenade's kind ("frag" or "sticky"). A class
    // name of our own (demolitionist) is kept as the kit's label.
    char secondary[24] = {};
    bool sticky = false;
    bool mills = false;         // 0.5.329: "mills": the campaign's Mills bomb (Rusher 2)
    bool flare = false;         // 0.5.753: "flare" (any optional column): his smokes are World at War's signal flares
    char label[16] = {};
};

// The game's seven classes, as the kits show them (0.5.197 kit log).
inline const char* const* ClassNames() {
    static const char* const names[7] = { "support", "light", "rifleman", "medic", "heavy", "antiarmor", "scout" };
    return names;
}
inline int ClassFromText(const std::string& t) {
    if (t.size() == 1 && t[0] >= '0' && t[0] <= '6') return t[0] - '0';
    if (t == "demolitionist") return 2;   // 0.5.284: built on the rifleman (his special: rifle grenades)
    if (t == "rusher2") return 6;         // 0.5.329: Rusher 2, built on the scout (his special: artillery, called with the binoculars)
    for (int i = 0; i < 7; i++) if (t == ClassNames()[i]) return i;
    return -1;
}
// Main weapons a kit may name: mp_fixes.cpp's custom class list (a campaign
// gun not set up on a map is given as the gun it copies).
inline bool KnownPrimary(const std::string& name) {
    static const char* const known[] = { "m1garand", "thompson", "bar", "springfield", "mg30cal", "bazooka", "shotgun", "kar98", "mp40",
                                         "mp44", "kar98_sniper", "mg34", "panzerschreck", "lee_enfield", "bren", "sten", "fg42", "fg42_scoped",
                                         "gewehr43", "mg42", "lee_enfield_sniper",   // 0.5.199/0.5.203: Team Friend's G43, MG42, scoped Lee-Enfield
                                         "webley",     // 0.5.659: CoD2's revolver (a pistol: best as a second weapon)
                                         "flamethrower",   // 0.5.687: World at War's M2
                                         "mosin_scoped" };   // 0.5.726: CoD2's scoped Mosin-Nagant
    for (const char* k : known) if (name == k) return true;
    return false;
}

inline const char* DefaultText() {
    return
        "# Call of Duty 3 recompiled: the bots' kits (0.5.198). One kit a line:\n"
        "#   weight  class  main-weapon  frags  smokes\n"
        "# weight: how often, against the other lines (0 turns a line off).\n"
        "# class: support, light, rifleman, medic, heavy, antiarmor or scout: the\n"
        "#   game's class the kit builds on (its pistol, items, ability, look).\n"
        "# main-weapon: - for the class's own, or one of m1garand thompson bar\n"
        "#   springfield mg30cal bazooka shotgun kar98 mp40 mp44 kar98_sniper mg34\n"
        "#   panzerschreck, or the campaign guns lee_enfield bren sten fg42 fg42_scoped\n"
        "#   gewehr43 mg42 lee_enfield_sniper, or CoD2's webley (a pistol) and\n"
        "#   mosin_scoped, or World at War's flamethrower.\n"
        "# frags / smokes: grenades at each spawn (- for the class's own; 0 to 4).\n"
        "# class may also be demolitionist (a rifleman with rifle grenades) or\n"
        "#   rusher2 (a scout: artillery called with his binoculars).\n"
        "# Saved changes are used for the next bots added (bot_add), no restart.\n"
        "# 0.5.277: one smoke at most (none for heavies and anti-armor), more\n"
        "#   anti-armor (the class's own launcher).\n"
        "# 0.5.326: two Demolitionists a side and more rifle grenades: the lines\n"
        "#   are given out in order first, so those come first.\n"
        "1 demolitionist bren      2 0 mg42 sticky\n"
        "1 demolitionist bren      2 0 mg42 sticky\n"
        "# 0.5.329 (Justin): Rusher 2: the Sten, the Bren as his second gun, Mills\n"
        "#   bombs, the scout's artillery; flanks, pushes to the enemy's spawn,\n"
        "#   holds with the Bren, back to the objective.\n"
        "1 rusher2    sten         2 0 bren mills\n"
        "3 rifleman   -            2 1\n"
        "2 rifleman   -            2 1\n"
        "2 support    -            2 1\n"
        "2 light      -            2 1\n"
        "1 medic      -            1 1\n"
        "1 heavy      -            2 0\n"
        "3 antiarmor  -            1 0\n"
        "1 antiarmor  -            2 0\n"
        "1 antiarmor  -            1 1\n"
        "1 scout      -            1 1\n"
        "1 rifleman   lee_enfield  2 1\n"
        "1 heavy      bren         2 0\n"
        "1 light      sten         2 1\n"
        "1 support    fg42         2 1\n"
        "1 scout      fg42_scoped  1 1\n"
        "1 rifleman   gewehr43     2 1\n"
        "1 heavy      mg42         2 0\n"
        "1 scout      lee_enfield_sniper 1 1\n"
        "# 0.5.659: CoD2's Webley revolver in the medics' pistol's place.\n"
        "1 medic      -            1 1 webley\n"
        "# 0.5.753: CoD2's scoped Mosin-Nagant, the Webley as a rifleman's second,\n"
        "#   and World at War's signal flare: \"flare\" in any optional column makes\n"
        "#   the kit's smokes flares.\n"
        "1 scout      mosin_scoped 1 1 flare\n"
        "1 rifleman   -            2 1 webley flare\n"
        "1 light      -            2 1 flare\n"
        "# 0.5.284 (Justin): the Demolitionist: the Bren, sticky bombs, rifle\n"
        "#   grenades (his rifleman base's special) and the MG42 as his second.\n"
        "#   Optional columns after smokes: second main weapon (- none), then\n"
        "#   frag or sticky.\n";
}

// Lines that do not read are skipped and told in `problems`; an empty
// result means "use the defaults".
inline std::vector<Kit> Parse(const std::string& text, std::string* problems = nullptr) {
    std::vector<Kit> kits;
    std::istringstream in(text);
    std::string line; int number = 0;
    auto count = [](const std::string& t, int& out) -> bool {
        if (t == "-") { out = -1; return true; }
        if (t.empty() || t.size() > 2) return false;
        for (char c : t) if (!isdigit((unsigned char)c)) return false;
        out = atoi(t.c_str()); if (out > 4) out = 4; return true;
    };
    while (std::getline(in, line)) {
        number++;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream words(line);
        std::string w, c, p, f, s, second, grenade;
        if (!(words >> w)) continue;
        Kit kit; bool ok = (words >> c >> p >> f >> s) ? true : false;
        // 0.5.753: "flare" may stand in any of the optional columns
        std::vector<std::string> extra;
        for (std::string e; ok && (words >> e);) { if (e == "flare") kit.flare = true; else extra.push_back(e); }
        if (ok && extra.size() > 0) {
            second = extra[0];
            if (second != "-") { ok = KnownPrimary(second); if (ok) strncpy(kit.secondary, second.c_str(), sizeof(kit.secondary) - 1); }
            if (ok && extra.size() > 1) { grenade = extra[1]; if (grenade == "sticky") kit.sticky = true; else if (grenade == "mills") kit.mills = true; else if (grenade != "frag" && grenade != "-") ok = false; }
        }
        if (ok && c == "demolitionist") strncpy(kit.label, "demolitionist", sizeof(kit.label) - 1);
        if (ok && c == "rusher2") strncpy(kit.label, "rusher2", sizeof(kit.label) - 1);
        int weight = -1;
        if (ok) { weight = atoi(w.c_str()); ok = !w.empty() && isdigit((unsigned char)w[0]) && weight >= 0 && weight <= 100; }
        const int cls = ok ? ClassFromText(c) : -1;
        ok = ok && cls >= 0 && (p == "-" || KnownPrimary(p)) && count(f, kit.frags) && count(s, kit.smokes);
        if (!ok) { if (problems) { char b[96]; snprintf(b, sizeof(b), "line %d not understood; ", number); *problems += b; } continue; }
        if (weight == 0) continue;
        kit.weight = unsigned(weight); kit.base = uint16_t(cls);
        if (p != "-") { strncpy(kit.primary, p.c_str(), sizeof(kit.primary) - 1); }
        kits.push_back(kit);
    }
    return kits;
}
inline const std::vector<Kit>& Defaults() { static const std::vector<Kit> d = Parse(DefaultText()); return d; }

// The kit his side is shortest of for its weight (team[i] / weight), then
// the whole game's, then the first.
inline unsigned Select(const std::vector<Kit>& kits, const std::vector<unsigned>& team, const std::vector<unsigned>& total) {
    unsigned pick = 0;
    for (unsigned i = 1; i < kits.size(); i++) {
        const double mine = double(i < team.size() ? team[i] : 0) / kits[i].weight, best = double(pick < team.size() ? team[pick] : 0) / kits[pick].weight;
        const double all = double(i < total.size() ? total[i] : 0) / kits[i].weight, allBest = double(pick < total.size() ? total[pick] : 0) / kits[pick].weight;
        if (mine < best - 1e-9 || (mine < best + 1e-9 && all < allBest - 1e-9)) pick = i;
    }
    return pick;
}

}
