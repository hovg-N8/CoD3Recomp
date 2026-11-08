#pragma once
#include <cmath>
#include <cstdint>
#include <limits>
#include <initializer_list>

// Local collision steering. Queries use a full body sweep for travel and a
// point ray for cover. No guessed navigation or game-memory structures.
namespace BotNavigation {
struct Point { float x, y, z; };
struct Hit { bool valid = false; bool solid = true; float fraction = 0; Point end{}, normal{}; uint32_t handle=0, surfaceFlags=0; };
inline float Distance(Point a, Point b) { return std::hypot(a.x-b.x, a.y-b.y); }
inline bool Finite(Point p) { return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z); }

template<class Query> bool Flat(Point from, Point candidate, Query&& query, Point& landing);

// 0.5.205: up and down hills. Justin, on 0.5.204: "bots struggle to walk up
// inclines like hills". The flat test below sweeps the body level at the
// feet over the whole step (up to 128 units) and lets the far floor be at
// most 18 above or 30 below: any slope over about 8 degrees up or 13 down
// was a wall (0.5.204's log: a level sweep stopped at 3.5% by ground whose
// normal was 0.96 up). When the flat test fails, the step is walked in
// pieces of at most 48 units instead: each piece rises by up to a stair
// (18) plus its length (a 45 degree slope, as steep as a player walks), or
// falls by up to 30 plus its length, and must land on walkable ground
// (normal 0.65 up or more), so cliffs and walls still stop it. And (0.5.206)
// the height it gains or loses must match the tilt of the ground it lands
// on: a flat-topped crate is a ledge, not a hill.
// 0.5.578: `step` the piece length (48 as before, or 16: one stair at a
// time, up to 12 pieces, the same 192 reach).
template<class Query>
bool Climb(Point from, Point candidate, Query&& query, Point& landing, float step = 48.0f, int most = 4) {
    const float length = Distance(from, candidate);
    if (!(length > 1)) return false;
    const int pieces = int(std::ceil(length / step));
    if (pieces < 1 || pieces > most) return false;
    Point at = from;
    for (int i = 1; i <= pieces; ++i) {
        const float t = float(i) / float(pieces), piece = length / float(pieces);
        const Point to{from.x + (candidate.x - from.x) * t, from.y + (candidate.y - from.y) * t, at.z};
        const float rise = 18.0f + piece * 1.0f, fall = 30.0f + piece * 1.0f;
        // Up first (the headroom), then across at that height, then down
        // onto the ground at the piece's end.
        // 0.5.700: lifted one stair (18, a player's step) first, the whole
        // rise only if that is blocked: under a ceiling that follows a stair
        // down (Carentan's cellar stairs at 400-552 1816, 6 up per 8 under the
        // ground floor), the body raised 34 met the floor above, so the stairs
        // were no way at all and bots stood on them for minutes.
        const Point start{at.x, at.y, at.z + 1};
        Point top = start;
        bool passed = false;
        for (const float lift : {18.0f, rise}) {
            top = {at.x, at.y, start.z + lift};
            const Hit up = query(start, top, true);
            if (!up.valid || up.solid) return false;
            top.z = start.z + lift * up.fraction - 1;
            if (top.z < start.z + 16.0f) return false;   // not even a stair of room
            const Hit across = query(top, {to.x, to.y, top.z}, true);
            if (!across.valid || across.solid) return false;
            if (across.fraction >= 0.999f) { passed = true; break; }
            if (up.fraction < 0.999f) break;   // no more room above: the whole rise is no higher
        }
        if (!passed) return false;
        Hit floor = query({to.x, to.y, top.z}, {to.x, to.y, at.z - fall}, true);
        if (!floor.valid || floor.solid || floor.fraction >= 1 || !Finite(floor.end) ||
            !Finite(floor.normal) || floor.normal.z < 0.65f) return false;
        // 0.5.206: the rise (or fall) must be the ground's own tilt, not a
        // ledge. 0.5.205 on Fuel Plant: a piece could rise 66 onto any flat
        // top, so crates and pipes 20-60 high were "climbable" and bots
        // walked into them for good (272 stuck strikes in 6 minutes, one
        // bot 8+ times in one place). Now a flat landing allows the usual
        // stair (18) up or drop (30) down, and a tilted one that plus its
        // slope over the piece (with 10% to spare).
        const float nz = std::fmin(1.0f, floor.normal.z);
        const float tilt = std::sqrt(std::fmax(0.0f, 1.0f - nz * nz)) / nz;
        const float allowedUp = 18.0f + piece * tilt * 1.1f, allowedDown = 30.0f + piece * tilt * 1.1f;
        const float dz = floor.end.z - at.z;
        if (dz > allowedUp || -dz > allowedDown) return false;
        at = floor.end; at.z += 1;
    }
    landing = at;
    return true;
}

template<class Query>
bool Reachable(Point from, Point candidate, Query&& query, Point& landing) {
    if (!Finite(from) || !Finite(candidate)) return false;
    if (Flat(from, candidate, query, landing)) return true;
    // 0.5.591: up to 8 pieces (384; was 4, 192): route spots are looked for up
    // to 350-400 off, and on a hill none that far was ever "walkable" (590's
    // carriers on Rouen's middle, 100-600 x 400-1000: their feet traces met
    // ground 0.90-0.99 up, the route "lost" every few seconds, 1511 times)
    if (Climb(from, candidate, query, landing, 48.0f, 8)) return true;
    // 0.5.578: stairs, a step at a time: a 48 piece over a flight of 8-16
    // high steps rises 24-48 onto a flat tread, more than its 18 for a flat
    // landing, so stairs were "not walkable". 577's tests: side 2's attackers
    // held at the flag building at -1792 -91, whose floor (z 42) is about 35
    // above the street (z 6-8), the step's hull meeting the risers (normal
    // up 0). A crate higher than a stair still fails on the one piece.
    return Climb(from, candidate, query, landing, 16.0f, 12);
}

template<class Query>
bool Flat(Point from, Point candidate, Query&& query, Point& landing) {
    if (!Finite(from) || !Finite(candidate)) return false;
    // Try walking at actual feet height first. Raising the entire standing
    // hull18 units rejects low doorways the player can walk through normally.
    Point start{from.x,from.y,from.z+1},end{candidate.x,candidate.y,from.z+1};
    Hit travel=query(start,end,true);
    if (!travel.valid || travel.solid || travel.fraction<0.999f) {
        Point raised{from.x,from.y,from.z+18};
        Hit up=query(start,raised,true);
        if(!up.valid || up.solid || up.fraction<0.999f)return false;
        start=raised;end.z=raised.z;
        travel=query(start,end,true);
        if(!travel.valid || travel.solid || travel.fraction<0.999f)return false;
    }
    Hit floor = query(end,{end.x,end.y,from.z-32},true);
    if (!floor.valid || floor.solid || floor.fraction < 0 || floor.fraction >= 1 ||
        !Finite(floor.end) || !Finite(floor.normal) || floor.normal.z < 0.65f ||
        floor.end.z < from.z-30 || floor.end.z > from.z+18) return false;
    landing = floor.end; landing.z += 1;
    return true;
}

template<class Query>
bool Step(Point from, Point goal, int handedness, bool stuck, Query&& query, Point& out) {
    float distance = Distance(from,goal);
    if (!std::isfinite(distance) || distance < 20) return false;
    const float yaw = std::atan2(goal.y-from.y,goal.x-from.x);
    const float length = std::fmin(distance,128.0f);
    float best = -std::numeric_limits<float>::infinity(); bool found=false;
    for (float degrees : {0.0f,35.0f,-35.0f,70.0f,-70.0f,105.0f,-105.0f,180.0f}) {
        const float angle = yaw + degrees * handedness * 0.01745329252f;
        Point candidate{from.x+std::cos(angle)*length,from.y+std::sin(angle)*length,from.z}, landing;
        if (!Reachable(from,candidate,query,landing)) continue;
        if (!stuck && degrees == 0) { out=landing;return true; }
        const float progress=distance-Distance(landing,goal);
        // A stuck bot commits to a lateral escape rather than alternating
        // between equally blocked directions on successive frames.
        const float escape=stuck && degrees>=35 && degrees<=105 ? 95.0f : 0;
        const float score=progress-std::fabs(degrees)*0.07f+escape;
        if (score > best) { best=score;out=landing;found=true; }
    }
    return found;
}

template<class Query, class SightQuery>
bool Cover(Point from, Point threatEye, Query&& query, SightQuery&& sight, Point& out) {
    const float away = std::atan2(from.y-threatEye.y,from.x-threatEye.x);
    float best=std::numeric_limits<float>::infinity();bool found=false;
    for (float radius : {96.0f,160.0f,224.0f})
        for (float degrees : {0.0f,45.0f,-45.0f,90.0f,-90.0f,135.0f,-135.0f,180.0f}) {
            const float angle=away+degrees*0.01745329252f;
            Point landing,candidate{from.x+std::cos(angle)*radius,from.y+std::sin(angle)*radius,from.z};
            if (!Reachable(from,candidate,query,landing)) continue;
            Hit shield=sight(threatEye,{landing.x,landing.y,landing.z+32},false);
            if (!shield.valid || shield.solid || shield.fraction >= 0.97f) continue;
            // Prefer close shelter, and avoid moving toward the attacker.
            float score=radius+std::fmax(0.0f,Distance(from,threatEye)-Distance(landing,threatEye))*2;
            if (score<best) {best=score;out=landing;found=true;}
        }
    return found;
}
}
