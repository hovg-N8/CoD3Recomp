#pragma once
#include <algorithm>
#include <cstdint>
namespace HitmarkerRules {
// The native SP damage path compares sentient teams at +4 as well.
inline bool Eligible(bool playerAttacker, bool actorTarget, bool sameEntity,
                     uint32_t attackerTeam, uint32_t targetTeam, int health, int damage) {
    return playerAttacker && actorTarget && !sameEntity && attackerTeam != 0 &&
           targetTeam != 0 && attackerTeam != targetTeam && health > 0 && damage > 0;
}
inline bool Confirmed(int before, int after) { return before > 0 && after < before; }
inline float Alpha(uint64_t age, int duration, int opacity) {
    if (age >= uint64_t(duration)) return 0;
    const float fade = std::min(1.0f, float(duration - age) / (duration * 0.5f));
    return fade * opacity / 100.0f;
}
}
