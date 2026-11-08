#pragma once
#include "bot_navigation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

// 0.5.197: the bots learn each map as it is played. Justin: bots bunched in
// the crowded middle, never used the outskirts, the high places or the low
// ones (trenches, caves). Every half second every player's feet are noted,
// humans' and bots' alike, in cells 128 units across and 96 high: where
// players stand is where a bot can stand. Each move from one cell to the
// next within a moment is a link, one way (a drop is not a climb), so the
// stairs, ladders and gaps players use become routes the bots can follow.
// Deaths and kills are counted where they happened, so the bots learn which
// spots get them killed and which ones win fights. Kept per map in a file,
// so it grows match after match. Pure logic, tested on its own
// (docs/build-logs/test_bot_map_knowledge_197.cpp).
namespace BotNavigation {

struct MapKnowledge {
    static constexpr float Cell = 128.0f, Layer = 96.0f;
    static constexpr size_t MaxNodes = 60000;
    static constexpr int MaxLinks = 12;

    struct Node {
        int32_t cx = 0, cy = 0, cz = 0;
        Point at{};                       // the first place a player stood in it (a real standing spot)
        uint32_t visits = 0;
        uint16_t deaths = 0, kills = 0, reached = 0, failed = 0;
        std::vector<int32_t> links;       // cells walked to from here
    };
    // 0.5.202: Near (a spot within nearRadius of `towards`: an enemy spawn, a
    // flag) and Away (a spot at least 500 from `towards`: out of one's own
    // spawn), for pushing, checking the flag and leaving spawn.
    enum class Style : uint8_t { Hunter, Outskirts, High, Low, Near, Away };
    struct Shape { Point centre{}; float outer = 0, zLow = 0, zHigh = 0, zMid = 0; bool valid = false; };

    std::vector<Node> nodes;
    std::unordered_map<uint64_t, int32_t> index;
    struct Track { int32_t node = -1; Point at{}; uint32_t time = 0; };
    std::array<Track, 64> tracks{};
    uint32_t changes = 0;                 // since the last save
    Shape shape;
    uint32_t shapeAt = 0xFFFFFFFFu;       // node count the shape was measured at

    static int32_t CellOf(float v, float size) { return int32_t(std::floor(v / size)); }
    static uint64_t Key(int32_t cx, int32_t cy, int32_t cz) {
        return (uint64_t(uint32_t(cx) & 0xFFFFF) << 40) | (uint64_t(uint32_t(cy) & 0xFFFFF) << 20) | uint64_t(uint32_t(cz) & 0xFFFFF);
    }
    static float Flat(Point a, Point b) { return std::hypot(a.x - b.x, a.y - b.y); }

    void Clear() { nodes.clear(); index.clear(); crossings.clear(); tracks = {}; changes = 0; shapeAt = 0xFFFFFFFFu; shape = {}; }
    bool Ready() const { return nodes.size() >= 40; }

    int32_t Exact(Point p) const {
        auto it = index.find(Key(CellOf(p.x, Cell), CellOf(p.y, Cell), CellOf(p.z, Layer)));
        return it == index.end() ? -1 : it->second;
    }
    // The known cell nearest p (same or neighbouring cells, any layer within
    // 2), or -1 when none is within `radius`.
    int32_t Nearest(Point p, float radius = 300.0f) const {
        if (!Finite(p)) return -1;
        const int32_t e = Exact(p);
        if (e >= 0) return e;
        const int32_t cx = CellOf(p.x, Cell), cy = CellOf(p.y, Cell), cz = CellOf(p.z, Layer);
        const int reach = std::max(1, int(std::ceil(radius / Cell)));
        int32_t best = -1; float bestScore = 1e30f;
        for (int dx = -reach; dx <= reach; dx++)
            for (int dy = -reach; dy <= reach; dy++)
                for (int dz = -2; dz <= 2; dz++) {
                    auto it = index.find(Key(cx + dx, cy + dy, cz + dz));
                    if (it == index.end()) continue;
                    const Node& n = nodes[size_t(it->second)];
                    const float flat = Flat(n.at, p), up = std::fabs(n.at.z - p.z);
                    if (flat > radius || up > 200) continue;
                    const float score = flat + up * 2.0f;
                    if (score < bestScore) { bestScore = score; best = it->second; }
                }
        return best;
    }
    // 0.5.553: the known cells within `radius` of p (the same cells and score
    // as Nearest), nearest first, at most `most`: for a start the bot can walk
    // to, the nearest by distance being past a wall often enough.
    std::vector<int32_t> NearestList(Point p, float radius, size_t most) const {
        std::vector<std::pair<float, int32_t>> found;
        if (!Finite(p)) return {};
        const int32_t cx = CellOf(p.x, Cell), cy = CellOf(p.y, Cell), cz = CellOf(p.z, Layer);
        const int reach = std::max(1, int(std::ceil(radius / Cell)));
        for (int dx = -reach; dx <= reach; dx++)
            for (int dy = -reach; dy <= reach; dy++)
                for (int dz = -2; dz <= 2; dz++) {
                    auto it = index.find(Key(cx + dx, cy + dy, cz + dz));
                    if (it == index.end()) continue;
                    const Node& n = nodes[size_t(it->second)];
                    const float flat = Flat(n.at, p), up = std::fabs(n.at.z - p.z);
                    if (flat > radius || up > 200) continue;
                    found.push_back({ flat + up * 2.0f, it->second });
                }
        std::sort(found.begin(), found.end());
        std::vector<int32_t> out;
        for (size_t i = 0; i < found.size() && out.size() < most; i++) out.push_back(found[i].second);
        return out;
    }
    Point At(int32_t cell) const { return cell >= 0 && size_t(cell) < nodes.size() ? nodes[size_t(cell)].at : Point{ NAN, NAN, NAN }; }
    // 0.5.554: links a bot cannot walk straight from spot to spot (a player's
    // jump or climb, or two cells' spots with a corner between), found by the
    // bots' route checks this session: dearer by BadLinkCost in PathFrom, not
    // removed (a place with no other way still has one).
    // 0.5.594: 20000 (was 2500): a way round a corner on Rouen is often longer
    // than 2500, so the route still took the bad link. 592's play: bots round
    // 610 1280 had 453 1090 next (no straight way: no step at all), the route
    // made again sent them back to 622 1277, then 453 1090 again (30 of 202 stalls).
    static constexpr float BadLinkCost = 20000.0f;
    std::unordered_map<uint64_t, uint8_t> badLinks;
    static uint64_t LinkKey(int32_t a, int32_t b) { return (uint64_t(uint32_t(a)) << 32) | uint32_t(b); }
    void BadLink(int32_t a, int32_t b) { badLinks[LinkKey(a, b)] = 1; }
    bool IsBadLink(int32_t a, int32_t b) const { return badLinks.count(LinkKey(a, b)) != 0; }
    // 0.5.580: where a player last walked each link this session: the half
    // second's step from the one cell into the other (two real standing spots
    // he went between), so a link whose two cells' spots have a corner between
    // them can still be walked the way he went (the doorways into buildings).
    struct Crossing { Point from{}, to{}; };
    std::unordered_map<uint64_t, Crossing> crossings;
    bool CrossingOf(int32_t a, int32_t b, Crossing& out) const {
        auto it = crossings.find(LinkKey(a, b));
        if (it == crossings.end()) return false;
        out = it->second; return true;
    }
    int32_t Touch(Point p) {
        const int32_t cx = CellOf(p.x, Cell), cy = CellOf(p.y, Cell), cz = CellOf(p.z, Layer);
        const uint64_t key = Key(cx, cy, cz);
        auto it = index.find(key);
        if (it != index.end()) { Node& n = nodes[size_t(it->second)]; if (n.visits < 0xFFFFFFF0u) n.visits++; return it->second; }
        if (nodes.size() >= MaxNodes) return -1;
        Node n; n.cx = cx; n.cy = cy; n.cz = cz; n.at = p; n.visits = 1;
        nodes.push_back(n);
        const int32_t id = int32_t(nodes.size() - 1);
        index[key] = id;
        changes++;
        return id;
    }
    void Link(int32_t from, int32_t to) {
        if (from < 0 || to < 0 || from == to) return;
        auto& links = nodes[size_t(from)].links;
        if (std::find(links.begin(), links.end(), to) != links.end() || int(links.size()) >= MaxLinks) return;
        links.push_back(to);
        changes++;
    }
    // One player's feet now (who: his slot). A move to a new cell within
    // 1.5 s and 260 units (not a respawn or a teleport) links the two.
    void Observe(int who, Point feet, uint32_t now, bool alive) {
        if (who < 0 || who >= int(tracks.size())) return;
        Track& t = tracks[size_t(who)];
        if (!alive || !Finite(feet)) { t.node = -1; return; }
        const int32_t n = Touch(feet);
        if (n < 0) return;
        if (t.node >= 0 && n != t.node && now - t.time <= 1500 && Flat(t.at, feet) <= 260.0f && std::fabs(t.at.z - feet.z) <= 200.0f)
        { Link(t.node, n); crossings[LinkKey(t.node, n)] = { t.at, feet }; }
        t.node = n; t.at = feet; t.time = now;
    }
    static void Bump(uint16_t& v) { if (v < 60000) v++; }
    void Death(Point at)   { const int32_t n = Nearest(at, 200); if (n >= 0) { Bump(nodes[size_t(n)].deaths); changes++; } }
    void Kill(Point at)    { const int32_t n = Nearest(at, 200); if (n >= 0) { Bump(nodes[size_t(n)].kills); changes++; } }
    void Reached(int32_t n){ if (n >= 0 && size_t(n) < nodes.size()) { Bump(nodes[size_t(n)].reached); changes++; } }
    void Failed(int32_t n) { if (n >= 0 && size_t(n) < nodes.size()) { Bump(nodes[size_t(n)].failed); changes++; } }

    // The map's shape from where players have been: its middle (by visits),
    // how far out its outskirts start (70% of cells are nearer the middle),
    // and its low, middle and high ground (25th, 50th, 75th height).
    const Shape& Measure() {
        if (shapeAt == nodes.size()) return shape;
        shapeAt = uint32_t(nodes.size());
        shape = {};
        if (!Ready()) return shape;
        double sx = 0, sy = 0, sw = 0;
        std::vector<float> zs; zs.reserve(nodes.size());
        for (const Node& n : nodes) { const double w = std::min<uint32_t>(n.visits, 50); sx += n.at.x * w; sy += n.at.y * w; sw += w; zs.push_back(n.at.z); }
        if (sw <= 0) return shape;
        shape.centre = { float(sx / sw), float(sy / sw), 0 };
        std::vector<float> ds; ds.reserve(nodes.size());
        for (const Node& n : nodes) ds.push_back(Flat(n.at, shape.centre));
        std::sort(ds.begin(), ds.end()); std::sort(zs.begin(), zs.end());
        auto at = [](const std::vector<float>& v, float q) { return v[std::min(v.size() - 1, size_t(q * float(v.size() - 1) + 0.5f))]; };
        shape.outer = at(ds, 0.70f);
        shape.zLow = at(zs, 0.15f); shape.zMid = at(zs, 0.50f); shape.zHigh = at(zs, 0.85f);   // 0.5.200: 15th/85th (were 25th/75th)
        shape.valid = true;
        return shape;
    }

    // A place for a bot of this style to go: the outskirts, high ground or
    // low ground, somewhere players have walked out of (so it can be left
    // again), better where fights were won and worse where players died or
    // bots failed to get, away from places other bots are already going
    // (claimed), not right where he is. -1: nothing fits yet.
    // 0.5.198: every cell a route reaches from the cell nearest `from`
    // (1: reachable). 0.5.197's errands mostly went "straight" (29 of 41) at
    // spots no known route led to, and half were given up as too slow.
    std::vector<uint8_t> Reach(Point from, size_t most = 30000) const {
        std::vector<uint8_t> seen(nodes.size(), 0);
        const int32_t start = Nearest(from, 300);
        if (start < 0) return seen;
        std::vector<int32_t> queue{ start }; seen[size_t(start)] = 1;
        for (size_t at = 0; at < queue.size() && queue.size() < most; at++)
            for (int32_t next : nodes[size_t(queue[at])].links)
                if (!seen[size_t(next)]) { seen[size_t(next)] = 1; queue.push_back(next); }
        return seen;
    }
    // reach (optional): only cells marked in it; maxAway: no farther than this.
    // towards / lean (0.5.202): a pull toward that point for any style (lean
    // > 0: nearer is better, < 0: farther), e.g. a defender's own half, an
    // attacker's enemy half; and the point Near and Away are measured from.
    int32_t Pick(Style style, Point from, unsigned seed, const std::vector<Point>& claimed,
                 const std::vector<uint8_t>* reach = nullptr, float maxAway = 1e30f,
                 Point towards = { NAN, NAN, NAN }, float lean = 0.0f, float nearRadius = 900.0f) {
        const Shape& s = Measure();
        if (!s.valid || style == Style::Hunter) return -1;
        if ((style == Style::Near || style == Style::Away) && !Finite(towards)) return -1;
        int32_t best = -1; float bestScore = -1e30f;
        uint32_t h = seed * 2654435761u + 0x9E3779B9u;
        for (size_t i = 0; i < nodes.size(); i++) {
            const Node& n = nodes[i];
            h ^= h << 13; h ^= h >> 17; h ^= h << 5;
            if (n.visits < 2 || n.links.empty()) continue;
            if (reach && (i >= reach->size() || !(*reach)[i])) continue;
            float score;
            if (style == Style::Outskirts) { const float d = Flat(n.at, s.centre); if (d < s.outer) continue; score = d / std::max(s.outer, 1.0f); }
            else if (style == Style::High) { if (n.at.z < s.zHigh || n.at.z < s.zMid + 32) continue; score = (n.at.z - s.zMid) / 60.0f; }   // 0.5.200: 32 above the middle (was 64: Rouen had almost none)
            else if (style == Style::Low) { if (n.at.z > s.zLow || n.at.z > s.zMid - 32) continue; score = (s.zMid - n.at.z) / 60.0f; }   // 0.5.200: 32 below (was 48)
            else if (style == Style::Near) { const float d = Flat(n.at, towards); if (d > nearRadius) continue; score = 2.0f - d / nearRadius; }
            else { const float d = Flat(n.at, towards); if (d < 500) continue; score = std::min(d, 1500.0f) / 500.0f; }
            score = std::min(score, 4.0f);
            if (lean != 0.0f && Finite(towards)) score += lean * (1.0f - std::min(Flat(n.at, towards), 4000.0f) / 4000.0f);
            score += std::min<float>(n.kills, 10) * 0.4f - std::min<float>(n.deaths, 10) * 0.6f - std::min<float>(n.failed, 6) * 1.2f;
            const float away = Flat(n.at, from);
            if (away < 300 || away > maxAway) continue;
            if (away > 4000) score -= 1.0f;
            for (const Point& c : claimed) if (Finite(c) && Flat(c, n.at) < 500) score -= 3.0f;
            score += float(h % 1000) / 1000.0f * 1.5f;   // each bot his own choice
            if (score > bestScore) { bestScore = score; best = int32_t(i); }
        }
        return best;
    }

    // 0.5.202: places worth watching from `from` (a sniper's overwatch): the
    // busiest cells 600 to 2500 away, each at least 30 degrees of bearing from
    // the others, busiest first.
    std::vector<Point> Watch(Point from, int count = 3) const {
        std::vector<std::pair<uint32_t, int32_t>> busy;
        for (size_t i = 0; i < nodes.size(); i++) {
            const float d = Flat(nodes[i].at, from);
            if (d >= 600 && d <= 2500 && nodes[i].visits >= 3) busy.push_back({ nodes[i].visits, int32_t(i) });
        }
        std::sort(busy.begin(), busy.end(), [](auto& a, auto& b) { return a.first > b.first; });
        std::vector<Point> out; std::vector<float> bearings;
        for (auto& b : busy) {
            const Point p = nodes[size_t(b.second)].at;
            const float bearing = std::atan2(p.y - from.y, p.x - from.x);
            bool apart = true;
            for (float o : bearings) { float d = std::fabs(bearing - o); if (d > 3.14159265f) d = 6.2831853f - d; if (d < 0.5236f) apart = false; }
            if (!apart) continue;
            out.push_back(p); bearings.push_back(bearing);
            if (int(out.size()) >= count) break;
        }
        return out;
    }

    // A route over the links from the cell nearest `from` to `goal`: the
    // cells' standing spots in order (A*, a death costs 200 units). Empty
    // when there is no known way.
    std::vector<Point> Path(Point from, int32_t goal, size_t maxExpand = 8000) const {
        return PathFrom(Nearest(from, 300), goal, maxExpand, false);
    }
    // 0.5.553: from a start cell chosen by the caller; `withStart`: the start's
    // own spot first in the route (Path leaves it out).
    std::vector<Point> PathFrom(int32_t start, int32_t goal, size_t maxExpand = 8000, bool withStart = true) const {
        std::vector<Point> out;
        for (const int32_t c : PathCells(start, goal, maxExpand, withStart)) out.push_back(nodes[size_t(c)].at);
        return out;
    }
    // 0.5.564: a lane: spots on the far side of the line a..b from the lane's
    // side (lane -1 or +1, by the line's left/right) cost `perUnit` per unit
    // they lie over it, so a route keeps to its side where the streets allow.
    struct Lane { Point a{}, b{}; int side = 0; float perUnit = 0.0f; };
    float LaneCost(const Lane& lane, Point p) const {
        if (lane.side == 0) return 0.0f;
        const float dx = lane.b.x - lane.a.x, dy = lane.b.y - lane.a.y, len = std::max(1.0f, std::hypot(dx, dy));
        const float across = ((p.x - lane.a.x) * -dy + (p.y - lane.a.y) * dx) / len;   // + on the left of a->b
        const float wrong = -float(lane.side) * across;
        return wrong > 0.0f ? wrong * lane.perUnit : 0.0f;
    }
    // The same as cells (0.5.554: so the caller can check its links).
    std::vector<int32_t> PathCells(int32_t start, int32_t goal, size_t maxExpand = 8000, bool withStart = true, const Lane* lane = nullptr) const {
        std::vector<int32_t> out;
        if (start < 0 || size_t(start) >= nodes.size() || goal < 0 || size_t(goal) >= nodes.size()) return out;
        if (withStart && start != goal) out.push_back(start);
        if (start == goal) { out.push_back(goal); return out; }
        const Point target = nodes[size_t(goal)].at;
        std::unordered_map<int32_t, float> cost;
        std::unordered_map<int32_t, int32_t> came;
        using Item = std::pair<float, int32_t>;
        std::priority_queue<Item, std::vector<Item>, std::greater<Item>> open;
        cost[start] = 0; open.push({ Distance(nodes[size_t(start)].at, target), start });
        size_t expanded = 0;
        while (!open.empty() && expanded < maxExpand) {
            const int32_t cur = open.top().second; open.pop();
            if (cur == goal) break;
            expanded++;
            const float here = cost[cur];
            for (int32_t next : nodes[size_t(cur)].links) {
                const Node& n = nodes[size_t(next)];
                const float step = Distance(nodes[size_t(cur)].at, n.at) + std::min<float>(n.deaths, 10) * 200.0f +
                                   (IsBadLink(cur, next) ? BadLinkCost : 0.0f) +   // 0.5.554
                                   (lane ? LaneCost(*lane, n.at) : 0.0f);           // 0.5.564
                auto found = cost.find(next);
                if (found != cost.end() && found->second <= here + step) continue;
                cost[next] = here + step; came[next] = cur;
                open.push({ here + step + Distance(n.at, target), next });
            }
        }
        if (!came.count(goal)) return {};
        std::vector<int32_t> way;
        for (int32_t at = goal; at != start; at = came[at]) way.push_back(at);
        std::reverse(way.begin(), way.end());
        out.insert(out.end(), way.begin(), way.end());
        return out;
    }

    // Kept as text: a line of the cell, its spot and counts, then its links.
    std::string Save() const {
        std::ostringstream o;
        o << "CoD3 bot map 1 " << nodes.size() << "\n";
        char line[160];
        for (const Node& n : nodes) {
            snprintf(line, sizeof(line), "%d %d %d %.0f %.0f %.0f %u %u %u %u %u", n.cx, n.cy, n.cz, n.at.x, n.at.y, n.at.z, n.visits,
                     unsigned(n.deaths), unsigned(n.kills), unsigned(n.reached), unsigned(n.failed));
            o << line;
            for (int32_t l : n.links) o << ' ' << l;
            o << "\n";
        }
        return o.str();
    }
    bool Load(const std::string& text) {
        Clear();
        std::istringstream in(text);
        std::string head; size_t count = 0;
        if (!std::getline(in, head) || sscanf(head.c_str(), "CoD3 bot map 1 %zu", &count) != 1 || count > MaxNodes) return false;
        std::string line;
        while (nodes.size() < count && std::getline(in, line)) {
            std::istringstream l(line);
            Node n; unsigned d, k, r, f;
            if (!(l >> n.cx >> n.cy >> n.cz >> n.at.x >> n.at.y >> n.at.z >> n.visits >> d >> k >> r >> f)) { Clear(); return false; }
            n.deaths = uint16_t(std::min(d, 60000u)); n.kills = uint16_t(std::min(k, 60000u));
            n.reached = uint16_t(std::min(r, 60000u)); n.failed = uint16_t(std::min(f, 60000u));
            int32_t link;
            while (l >> link && int(n.links.size()) < MaxLinks) n.links.push_back(link);
            index[Key(n.cx, n.cy, n.cz)] = int32_t(nodes.size());
            nodes.push_back(std::move(n));
        }
        for (Node& n : nodes)
            n.links.erase(std::remove_if(n.links.begin(), n.links.end(), [&](int32_t l) { return l < 0 || size_t(l) >= nodes.size(); }), n.links.end());
        changes = 0; shapeAt = 0xFFFFFFFFu;
        return nodes.size() == count;
    }
};

}
