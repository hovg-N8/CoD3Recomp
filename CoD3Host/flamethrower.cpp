// 0.5.681: World at War's M2 flamethrower: its flame and its sounds.
//
// The flame. World at War draws a stream of fire chunks with its engine's
// flame system (flameconfigs, about a hundred flameVar_ settings); CoD3 has
// none, so the stream here is drawn over the picture (Overlay::SetWeaponHud):
// fire sprites from World at War's own flamethrower_fire sheet (8 x 8 frames,
// two 32-frame fireballs that swell, burn and fade; alpha from brightness,
// tools/waw/make_flamethrower.py) thrown from the nozzle's place on the
// screen toward the crosshair, each a fireball's life long: smaller as it
// goes away, falling a little, wavering. A lick of fire (fxt_fire_flame_lick)
// flickers at the nozzle while it burns.
// 0.5.703: others' flames are drawn too (they were only heard): a stream of
// the same chunks in the world, from their gun toward their aim, projected
// with the view the game draws (its view block: Flame::Camera), hidden
// while a wall stands between us and their gun. Every flame, ours too,
// stops at the first wall in its way (mp_fixes.cpp traces it) and spreads
// out over it there, as World at War's does.
// COD3_FLAME_MUZZLE="x y": the nozzle's place, as fractions of the picture
// (default 0.515 0.565, the nozzle at rest in 0.5.682's screenshots);
// COD3_FLAME_TEST=1: the flame drawn whenever it is in
// hand (for screenshots: firing cannot be scripted).
//
// The sounds (World at War's, 16-bit PCM): the ignition when the trigger is
// pulled, the 1.5 s burning loop played again while it is held (a little
// overlapped), the cool-down when it is let go. Another's: the mono loop,
// fainter with distance.
#include "flamethrower.h"
#include "audio_out.h"
#include "overlay.h"
#include "render.h"   // 0.5.717: Render::TrackedPoint

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <mutex>
#include <random>
#include <vector>

#include <Windows.h>

#include "imgui.h"

namespace
{
    std::mutex g_lock;
    bool g_held = false;
    double g_lastShot = -10.0, g_loopNext = 0.0, g_fireStart = -10.0;
    bool g_firing = false;
    std::map<uint32_t, double> g_otherLoop;

    constexpr double ShotGap = 0.2;     // past this since the last shot, the trigger is let go (fireTime 0.1)
    constexpr double LoopLength = 1.40; // the loop is 1.49 s: played again a little before its end

    // 0.5.685: a fire chunk in the camera's space (x right, y down, z
    // forward; units as the game's), thrown from the nozzle toward the aim,
    // slowed by the air and pulled down, drawn where it projects: the stream
    // narrows into the distance toward the crosshair and droops at its end
    // as World at War's does (0.5.684's chunks moved on the screen alone and
    // piled up into one ball at the nozzle).
    struct Particle { float age, life, x, y, z, vx, vy, vz, grow, spin; int sequence; bool stopped; float dripAt; };
    std::vector<Particle> g_particles;
    double g_lastDraw = -1.0, g_emitCarry = 0.0;
    std::mt19937 g_random{ 1234 };
    float g_ownReach = 600.0f;   // 0.5.703: our aim's distance to the first wall

    // 0.5.703: the view as the game draws it (origin; forward, left, up axes;
    // its field of view across a 4:3 picture)
    struct View { bool valid = false; float eye[3]{}, fwd[3]{}, left[3]{}, up[3]{}; float fov = 80.0f; double at = -10.0; };
    View g_view;
    // 0.5.703: another's flame, in the world
    struct Chunk { float age, life, p[3], v[3], travelled, grow, dripAt; int sequence; bool stopped; };
    struct Stream { double lastShot = -10.0, emitCarry = 0.0; float muzzle[3]{}, dir[3]{}; float reach = 450.0f; bool visible = true; std::vector<Chunk> chunks; };
    std::map<uint32_t, Stream> g_streams;
    // 0.5.709: the nozzle as World at War's animation puts it (Flame::Nozzle),
    // in the view's space, and when it was last given
    float g_nozzle[3] = { 0.0f, 0.0f, 0.0f };
    double g_nozzleAt = -10.0;
    float g_nozzleFov = 80.0f;   // the arms' fov the nozzle is seen with
    float g_heat = 0.0f;          // 0.5.720: our overheating, 0..100
    bool g_overheated = false;
    double g_heatAt = -10.0;
    constexpr double StreamGap = 0.3;   // a shooter's flame dies this long after his last shot event

    // 0.5.703: what a flame leaves behind, World at War's three kinds with its
    // own numbers (flameconfigs/flame_firstperson_mp and _thirdperson_mp in
    // common_mp.ff): fire where a chunk lands, burning on (fireLife 1.30 +
    // 0.32, rising: gravity -290 to -172, friction 4.77, from 0.2 of the
    // chunk's size to 1.2 of it + 7.7); drips falling from the stream
    // (dripsLife 0.85 + 0.52, gravity 330 to 421, friction 0.82, 0.5 of the
    // chunk's size to nothing); smoke over it (smokeLife 1.55 + 0.51, rising:
    // gravity -172 to -235, friction 3.41, growing by 89.5, in over 0.11 s and
    // out over the last 0.8, at most 0.9 for others' flames and 0.35 for ours).
    enum class Kind { Fire, Drip, Smoke };
    struct Ember { float p[3], v[3], age, life, s0, s1, g0, g1, friction, alpha, spin; Kind kind; int sequence; uint32_t owner; bool visible; };   // (owner 0: ours)
    std::vector<Ember> g_embers;
    constexpr size_t MostEmbers = 700;

    void Leave(Kind kind, const float at[3], const float vel[3], float chunkSize, bool ours, uint32_t owner = 0)
    {
        if (g_embers.size() >= MostEmbers) return;
        std::uniform_real_distribution<float> u(0.0f, 1.0f);
        Ember e{};
        for (int k = 0; k < 3; k++) { e.p[k] = at[k]; e.v[k] = vel[k]; }
        e.kind = kind;
        e.owner = owner;   // 0.5.711: another's is hidden while his flame is (behind a wall)
        e.visible = true;
        e.sequence = u(g_random) < 0.5f ? 0 : 1;
        e.spin = (u(g_random) - 0.5f) * 2.0f * 3.27f;   // fireMaxRotVel 187 degrees a second
        if (kind == Kind::Fire)
        {
            e.life = 1.3043f + 0.3244f * u(g_random);
            const float scale = 0.9349f + 0.1496f * u(g_random);
            for (int k = 0; k < 2; k++) e.v[k] *= scale * 0.1504f;   // fireVelocityAddSideways
            e.v[2] = e.v[2] * scale * 0.1504f + 140.3957f - 196.7115f * u(g_random);
            e.g0 = -290.5f; e.g1 = -171.5f; e.friction = 4.7713f;
            e.s0 = 0.2008f * chunkSize; e.s1 = 1.1984f * chunkSize + 7.7405f; e.alpha = 0.9f;
        }
        else if (kind == Kind::Drip)
        {
            e.life = 0.85f + 0.524f * u(g_random);
            const float scale = 0.8854f + 0.2072f * u(g_random);
            for (int k = 0; k < 3; k++) e.v[k] *= scale;
            e.v[2] += 4.7064f - 20.0f * u(g_random);
            e.g0 = 330.2f; e.g1 = 421.3f; e.friction = 0.8211f;
            e.s0 = 0.4976f * chunkSize; e.s1 = 0.0f; e.alpha = 0.9f;
        }
        else
        {
            e.life = (ours ? 1.3179f : 1.55f) + 0.5072f * u(g_random);
            for (int k = 0; k < 3; k++) e.v[k] *= 0.3965f;
            e.v[2] += 20.0867f;
            e.p[2] += 10.0f;   // smokeOriginOffset
            e.g0 = -172.2f; e.g1 = -235.0f; e.friction = 3.409f;
            e.s0 = chunkSize; e.s1 = chunkSize + 89.53f; e.alpha = ours ? 0.35f : 0.9f;
        }
        g_embers.push_back(e);
    }

    // a fire chunk landing on a wall or the ground: the fire it leaves and,
    // for one in two, smoke (World at War spawns fire through a chunk's life,
    // most of it where the stream stops)
    void Land(const float at[3], const float vel[3], float chunkSize, bool ours, uint32_t owner = 0)
    {
        std::uniform_real_distribution<float> u(0.0f, 1.0f);
        if (u(g_random) < 0.45f) Leave(Kind::Fire, at, vel, chunkSize, ours, owner);
        if (u(g_random) < 0.18f) Leave(Kind::Smoke, at, vel, chunkSize, ours, owner);
    }

    double Now()
    {
        static const auto start = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }

    bool Testing() { static const bool t = getenv("COD3_FLAME_TEST") != nullptr; return t; }

    // (a shot within the gap: the flame never outlives the shots, even when
    // Tick stops being called, dead or between maps)
    bool Burning(double now) { return g_held && ((g_firing && now - g_lastShot <= ShotGap) || Testing()); }

    bool HudActive()
    {
        std::lock_guard<std::mutex> guard(g_lock);
        return (g_held && (Burning(Now()) || !g_particles.empty() || Now() - g_heatAt < 0.5)) || !g_streams.empty() || !g_embers.empty();   // (0.5.720: the heat bar too; 0.5.725: always while held, its panel covers the ammunition box)
    }

    void Muzzle(float& x, float& y)
    {
        // the tube's end (0.5.708, the user: "move the start point"; measured on
        // 707's full-size screenshot, the ring at the tube's mouth at 0.525
        // 0.577; 0.5.705's 0.513 0.58 sat a little left of it, on the tube)
        static float mx = 0.525f, my = 0.577f;
        static bool read = false;
        if (!read)
        {
            read = true;
            if (const char* t = getenv("COD3_FLAME_MUZZLE")) sscanf(t, "%f %f", &mx, &my);
        }
        x = mx; y = my;
    }

    // One sprite: a frame of a sheet of columns x rows, centred, turned, at an
    // alpha; tall: the cell's height over its width (the fireballs' 2: 512 / 8
    // by 1024 / 8; the licks' 1)
    void Sprite(ImDrawList* list, ImTextureID tex, int columns, int rows, int frame, float cx, float cy, float size, float angle, float alpha, float tall, int grey = 255)
    {
        const float u0 = float(frame % columns) / columns, v0 = float(frame / columns) / rows;
        const float u1 = u0 + 1.0f / columns, v1 = v0 + 1.0f / rows;
        const float c = cosf(angle) * size * 0.5f, s = sinf(angle) * size * 0.5f;
        const float hx = c, hy = s, vx = -s * tall, vy = c * tall;
        const ImU32 col = IM_COL32(grey, grey, grey, int(std::clamp(alpha, 0.0f, 1.0f) * 255.0f));
        list->AddImageQuad(tex, ImVec2(cx - hx - vx, cy - hy - vy), ImVec2(cx + hx - vx, cy + hy - vy), ImVec2(cx + hx + vx, cy + hy + vy),
                           ImVec2(cx - hx + vx, cy - hy + vy), ImVec2(u0, v0), ImVec2(u1, v0), ImVec2(u1, v1), ImVec2(u0, v1), col);
    }

    // 0.5.703: a point of the world on the picture, as the game's view draws
    // it (its fov is across a 4:3 picture, widened to the window's shape);
    // scale: pixels a unit at that depth. False behind the view.
    bool Project(const View& v, const float p[3], float W, float H, float& x, float& y, float& scale, float& depth)
    {
        const float d[3] = { p[0] - v.eye[0], p[1] - v.eye[1], p[2] - v.eye[2] };
        depth = d[0] * v.fwd[0] + d[1] * v.fwd[1] + d[2] * v.fwd[2];
        if (depth < 4.0f) return false;
        const float rx = -(d[0] * v.left[0] + d[1] * v.left[1] + d[2] * v.left[2]);
        const float uy = d[0] * v.up[0] + d[1] * v.up[1] + d[2] * v.up[2];
        const float aspect = W / std::max(H, 1.0f);
        const float tanH = tanf(std::clamp(v.fov, 20.0f, 130.0f) * 0.5f * 3.14159265f / 180.0f) * aspect / (4.0f / 3.0f);
        const float tanV = tanH / aspect;
        scale = (W * 0.5f) / (depth * tanH);
        x = W * 0.5f + rx * scale;
        y = H * 0.5f - uy * (H * 0.5f) / (depth * tanV);
        return true;
    }

    // a world chunk's size: World at War's third person stream, 10 units at
    // the gun swelling toward 110 (streamChunkStartSize, EndSize), its
    // distance scalar 0.7 applied
    float ChunkSize(const Chunk& c)
    {
        const float f = std::clamp(c.age / c.life, 0.0f, 1.0f);
        return (10.0f + 67.0f * powf(f, 0.8f)) * c.grow;
    }

    // 0.5.703: others' streams: chunks thrown from the gun along the aim
    // (about 780 units a second, slowed by the air: World at War's flame
    // reaches about 340 units, its maxDist), stopped at the first wall,
    // spreading and climbing there; drawn with our view
    void WorldDraw(ImDrawList* list, ImTextureID fire, ImTextureID lick, float W, float H, double now, float dt)
    {
        std::uniform_real_distribution<float> u(0.0f, 1.0f);
        for (auto it = g_streams.begin(); it != g_streams.end();)
        {
            Stream& s = it->second;
            const bool burning = now - s.lastShot <= StreamGap;
            for (Chunk& c : s.chunks)
            {
                c.age += dt;
                const float drag = expf((c.stopped ? -4.0f : -1.2f) * dt);
                for (int k = 0; k < 3; k++) c.v[k] *= drag;
                c.v[2] += (c.stopped ? 60.0f : -200.0f) * dt;
                const float step = sqrtf(c.v[0] * c.v[0] + c.v[1] * c.v[1] + c.v[2] * c.v[2]) * dt;
                for (int k = 0; k < 3; k++) c.p[k] += c.v[k] * dt;
                if (!c.stopped && c.dripAt > 0.0f && c.age >= c.dripAt)
                {
                    c.dripAt = 0.0f;
                    Leave(Kind::Drip, c.p, c.v, ChunkSize(c), false, it->first);
                }
                if (!c.stopped && (c.travelled += step) >= s.reach)
                {
                    c.stopped = true;
                    Land(c.p, c.v, ChunkSize(c), false, it->first);
                    // across the wall: any direction square to the aim, a little up
                    float side[3] = { -s.dir[1], s.dir[0], 0.0f };
                    const float a = (u(g_random) - 0.5f) * 2.0f, b = (u(g_random) - 0.5f) * 2.0f;
                    for (int k = 0; k < 3; k++) c.v[k] = 150.0f * a * side[k] - 30.0f * s.dir[k];
                    c.v[2] += 100.0f * b + 40.0f;
                }
            }
            s.chunks.erase(std::remove_if(s.chunks.begin(), s.chunks.end(), [](const Chunk& c) { return c.age >= c.life; }), s.chunks.end());
            if (burning)
            {
                s.emitCarry += dt * 80.0;
                while (s.emitCarry >= 1.0)
                {
                    s.emitCarry -= 1.0;
                    const float speed = 720.0f + 120.0f * u(g_random), spread = 0.05f;
                    float d[3] = { s.dir[0] + (u(g_random) - 0.5f) * spread, s.dir[1] + (u(g_random) - 0.5f) * spread, s.dir[2] + (u(g_random) - 0.5f) * spread };
                    Chunk c{};
                    c.age = u(g_random) * dt;
                    c.life = 0.6f + 0.2f * u(g_random);
                    for (int k = 0; k < 3; k++) { c.p[k] = s.muzzle[k] + s.dir[k] * 4.0f; c.v[k] = d[k] * speed; }
                    c.grow = 0.8f + 0.5f * u(g_random);
                    // one in four drops a drip early in its flight (streamDripsChunkRandFrac 0.24)
                    c.dripAt = u(g_random) < 0.25f ? c.life * 0.236f * u(g_random) + 0.02f : 0.0f;
                    c.sequence = u(g_random) < 0.5f ? 0 : 1;
                    s.chunks.push_back(c);
                }
            }
            else s.emitCarry = 0.0;
            if (!burning && s.chunks.empty()) { it = g_streams.erase(it); continue; }
            // 0.5.718: another's flame is a close thing (the user: "i keep seeing intense
            // flames and the drip effect" from across the map): full to 1200 units
            // away, gone past 1800
            float closeness = 1.0f;
            if (g_view.valid)
            {
                const float dx = s.muzzle[0] - g_view.eye[0], dy = s.muzzle[1] - g_view.eye[1], dz = s.muzzle[2] - g_view.eye[2];
                // 0.5.720: the user, "a little less visible from far but more visible the
                // closer you get": full within 300, falling away faster with distance,
                // gone at 1400 (718: full to 1200, gone past 1800)
                const float fade = std::clamp((1400.0f - sqrtf(dx * dx + dy * dy + dz * dz)) / 1100.0f, 0.0f, 1.0f);
                closeness = fade * fade;
            }
            if (g_view.valid && s.visible && closeness > 0.0f)
            {
                struct Drawn { float x, y, size, angle, alpha, depth; int frame; bool core; };
                std::vector<Drawn> drawn;
                // 0.5.705: World at War's flame ribbon while it burns: one
                // continuous flame from the gun (streamFlame: 2.5 units across
                // at the gun to 35 at its end, 246.7 long, its animation looping
                // in 0.41 s) round a brighter fuel core (streamFuel: 4 to 12
                // across, 200 long, 0.15 s), drooping as the stream does
                // (gravity 250 at 1000 units a second); the fire chunks show past
                // it (before, close by, a stream was a chain of fireballs)
                const float ribbon = burning ? std::min(s.reach, 246.7435f) : 0.0f;
                if (ribbon > 4.0f)
                {
                    auto along = [&](float d, float out[3])
                    {
                        const float t = d / 1000.0f;
                        for (int k = 0; k < 3; k++) out[k] = s.muzzle[k] + s.dir[k] * d;
                        out[2] -= 0.5f * 250.0f * t * t;
                    };
                    constexpr int Segments = 40;
                    for (int part = 0; part < 2; part++)
                    {
                        const bool core = part == 1;
                        const float length = core ? std::min(ribbon, 200.0f) : ribbon;
                        const float loop = core ? 0.1528f : 0.4088f;
                        for (int i = 0; i < Segments; i++)
                        {
                            const float t = powf((i + 0.5f) / Segments, 1.4f);   // closer together at the gun, where it is thin
                            float p[3], q[3];
                            along(t * length, p);
                            along(t * length + 6.0f, q);
                            float x, y, scale, depth, x2, y2, s2, d2;
                            if (!Project(g_view, p, W, H, x, y, scale, depth)) continue;
                            const float across = core ? 4.0f + 8.0f * t : 2.5f + 32.5f * t;
                            const float size = std::min(across * 1.6f * scale, H * 0.5f);   // (a fire cell's flame fills about 0.6 of it)
                            if (size < 0.8f) continue;
                            const float angle = Project(g_view, q, W, H, x2, y2, s2, d2) && (fabsf(x2 - x) + fabsf(y2 - y) > 0.5f) ? atan2f(y2 - y, x2 - x) - 1.5707963f : 0.0f;
                            const float alpha = closeness * (core ? 0.7f : 0.85f) * std::min(1.0f, (1.0f - t) / 0.25f);
                            const int frame = core ? int((now / loop + i * 0.37) * 16.0) % 16
                                                   : (i & 1) * 32 + 4 + int((now / loop + i * 0.21) * 16.0) % 16;
                            drawn.push_back({ x, y, size, angle, alpha, depth - (core ? 0.5f : 0.0f), frame, core });
                        }
                    }
                }
                for (const Chunk& c : s.chunks)
                {
                    float x, y, scale, depth;
                    if (!Project(g_view, c.p, W, H, x, y, scale, depth)) continue;
                    const float f = c.age / c.life;
                    const float size = std::min(ChunkSize(c) * scale, H * 0.6f);
                    if (size < 1.0f) continue;
                    // inside the ribbon a flying chunk is the ribbon; it fades in toward its end
                    const float show = (ribbon > 4.0f && !c.stopped) ? std::clamp((c.travelled - 0.7f * ribbon) / (0.3f * ribbon), 0.0f, 1.0f) : 1.0f;
                    if (show <= 0.0f) continue;
                    // drawn along its motion on the picture (the cells are tall)
                    const float ahead[3] = { c.p[0] + c.v[0] * 0.02f, c.p[1] + c.v[1] * 0.02f, c.p[2] + c.v[2] * 0.02f };
                    float x2, y2, s2, d2;
                    const float angle = Project(g_view, ahead, W, H, x2, y2, s2, d2) && (fabsf(x2 - x) + fabsf(y2 - y) > 0.5f) ? atan2f(y2 - y, x2 - x) - 1.5707963f : 0.0f;
                    const float alpha = 0.9f * closeness * show * std::min(1.0f, c.age / 0.04f) * (1.0f - f * f);
                    drawn.push_back({ x, y, size, angle, alpha, depth, c.sequence * 32 + std::min(31, int(f * 32.0f)), false });
                }
                std::sort(drawn.begin(), drawn.end(), [](const Drawn& a, const Drawn& b) { return a.depth > b.depth; });
                for (const Drawn& d : drawn)
                {
                    if (d.core && lick) Sprite(list, lick, 4, 4, d.frame, d.x, d.y, d.size, d.angle, d.alpha, 1.0f);
                    else if (!d.core) Sprite(list, fire, 8, 8, d.frame, d.x, d.y, d.size, d.angle, d.alpha, 2.0f);
                }
            }
            ++it;
        }
    }

    // 0.5.703: a place on the picture at a depth along the view, back into the world
    void Unproject(const View& v, float x, float y, float depth, float W, float H, float out[3])
    {
        const float aspect = W / std::max(H, 1.0f);
        const float tanH = tanf(std::clamp(v.fov, 20.0f, 130.0f) * 0.5f * 3.14159265f / 180.0f) * aspect / (4.0f / 3.0f);
        const float tanV = tanH / aspect;
        const float rx = (x - W * 0.5f) / (W * 0.5f) * depth * tanH, uy = -(y - H * 0.5f) / (H * 0.5f) * depth * tanV;
        for (int k = 0; k < 3; k++) out[k] = v.eye[k] + v.fwd[k] * depth - v.left[k] * rx + v.up[k] * uy;
    }

    // 0.5.703: the fire, drips and smoke flames leave behind (Leave above)
    void EmberDraw(ImDrawList* list, ImTextureID fire, float W, float H, float dt)
    {
        for (Ember& e : g_embers)
        {
            e.age += dt;
            const float f = std::clamp(e.age / e.life, 0.0f, 1.0f);
            const float drag = expf(-e.friction * dt);
            for (int k = 0; k < 3; k++) e.v[k] *= drag;
            e.v[2] -= (e.g0 + (e.g1 - e.g0) * f) * dt;   // the config's gravity: positive pulls down
            for (int k = 0; k < 3; k++) e.p[k] += e.v[k] * dt;
        }
        g_embers.erase(std::remove_if(g_embers.begin(), g_embers.end(), [](const Ember& e) { return e.age >= e.life; }), g_embers.end());
        if (!g_view.valid || g_embers.empty()) return;
        // 0.5.711: another's leftovers seen only while his flame is (the user, 710:
        // "i can see other peoples flames through walls ... from far away")
        for (Ember& e : g_embers)
            if (e.owner) { const auto s = g_streams.find(e.owner); if (s != g_streams.end()) e.visible = s->second.visible; }
        struct Drawn { float x, y, size, angle, alpha, depth; int frame, grey; };
        std::vector<Drawn> drawn;
        for (const Ember& e : g_embers)
        {
            float x, y, scale, depth;
            if (!e.visible || !Project(g_view, e.p, W, H, x, y, scale, depth)) continue;
            // 0.5.718: another's leftovers close only, as his flame (full to 1200, gone past 1800)
            const float fadeAway = std::clamp((1400.0f - depth) / 1100.0f, 0.0f, 1.0f);   // 0.5.720: as his flame
            const float nearby = e.owner ? fadeAway * fadeAway : 1.0f;
            if (nearby <= 0.0f) continue;
            const float f = std::clamp(e.age / e.life, 0.0f, 1.0f);
            const float size = std::min((e.s0 + (e.s1 - e.s0) * f) * scale, H * 0.8f);
            if (size < 1.0f) continue;
            float alpha;
            int frame, grey = 255;
            if (e.kind == Kind::Smoke)
            {
                // a burnt-out fireball's last frames, darkened: soft grey smoke
                const float in = std::min(1.0f, e.age / 0.1107f), out = std::min(1.0f, (e.life - e.age) / 0.8004f);
                alpha = e.alpha * in * out * 0.6f;
                frame = e.sequence * 32 + 24 + std::min(7, int(f * 8.0f));
                grey = 70;
            }
            else
            {
                alpha = e.alpha * std::min(1.0f, e.age / 0.05f) * (1.0f - f * f);
                // fire: the fireball's burning frames, looped; a drip: its first, bright ones
                frame = e.sequence * 32 + (e.kind == Kind::Drip ? std::min(7, int(f * 8.0f)) : 4 + int(e.age * 30.0f) % 20);
            }
            drawn.push_back({ x, y, size, e.spin * e.age, alpha * nearby, depth, frame, grey });
        }
        std::sort(drawn.begin(), drawn.end(), [](const Drawn& a, const Drawn& b) { return a.depth > b.depth; });
        for (const Drawn& d : drawn) Sprite(list, fire, 8, 8, d.frame, d.x, d.y, d.size, d.angle, d.alpha, 2.0f, d.grey);
    }

    // 0.5.720: World at War's overheating bar while it is held (bottom right,
    // over the ammunition): yellow to red as it heats, flashing while it has
    // overheated and cools to half
    void HeatBar(float W, float H, double now)
    {
        if (!g_held || now - g_heatAt > 0.5) return;
        ImDrawList* fg = ImGui::GetForegroundDrawList();
        // 0.5.725: in the ammunition box's place, over it (the user: "remove the
        // ammo counter and just keep the cooldown bar"; the HUD's own parts would
        // not hide: 722-724): a panel in the box's own look (dark teal, a thin
        // gold edge; measured in 724's screenshots at 0.798..0.891 x 0.872..0.916
        // of the picture), the heat bar inside it
        const float bx0 = W * 0.7965f, bx1 = W * 0.8925f, by0 = H * 0.8705f, by1 = H * 0.9175f;
        fg->AddRectFilled(ImVec2(bx0, by0), ImVec2(bx1, by1), IM_COL32(22, 32, 34, 255), H * 0.008f);
        fg->AddRect(ImVec2(bx0, by0), ImVec2(bx1, by1), IM_COL32(150, 128, 70, 230), H * 0.008f, 0, std::max(1.5f, H * 0.002f));
        const float pad = H * 0.012f;
        const float x = bx0 + pad, y = by0 + (by1 - by0) * 0.5f - H * 0.006f, w = (bx1 - bx0) - 2 * pad, h = H * 0.012f;
        fg->AddRectFilled(ImVec2(x - 2, y - 2), ImVec2(x + w + 2, y + h + 2), IM_COL32(0, 0, 0, 200), 2.0f);
        const float f = std::clamp(g_heat / 100.0f, 0.0f, 1.0f);
        const bool flash = g_overheated && fmod(now, 0.5) < 0.25;
        const ImU32 col = g_overheated ? (flash ? IM_COL32(255, 60, 30, 255) : IM_COL32(150, 30, 20, 255))
                                       : IM_COL32(255, int(220 - 160 * f), int(60 - 40 * f), 235);
        if (f > 0.0f) fg->AddRectFilled(ImVec2(x, y), ImVec2(x + w * f, y + h), col, 2.0f);
        fg->AddRect(ImVec2(x - 2, y - 2), ImVec2(x + w + 2, y + h + 2), IM_COL32(220, 220, 220, 120), 2.0f);
    }

    void HudDraw(int width, int height)
    {
        std::lock_guard<std::mutex> guard(g_lock);
        const double now = Now();
        const float dt = g_lastDraw < 0 ? 0.0f : float(std::clamp(now - g_lastDraw, 0.0, 0.1));
        g_lastDraw = now;
        const bool burning = Burning(now);
        const float W = float(width), H = float(height);
        // the view's projection: CoD3's 65 degrees across
        const float focal = (W * 0.5f) / tanf(65.0f * 0.5f * 3.14159265f / 180.0f);
        // the nozzle: where its tip is on the screen, 18 units ahead
        float mx, my;
        Muzzle(mx, my);
        // 0.5.709: World at War's tag_flash where its animation has the gun this
        // frame, on the picture with the view's own fov (the fixed point above
        // only while no animation gives one)
        if (now - g_nozzleAt < 0.25 && g_view.valid && g_nozzle[0] > 1.0f)
        {
            const float aspect = W / std::max(H, 1.0f);
            const float tanH = tanf(std::clamp(g_nozzleFov, 20.0f, 130.0f) * 0.5f * 3.14159265f / 180.0f) * aspect / (4.0f / 3.0f), tanV = tanH / aspect;
            mx = 0.5f + 0.5f * (-g_nozzle[1] / g_nozzle[0]) / tanH;
            my = 0.5f + 0.5f * (-g_nozzle[2] / g_nozzle[0]) / tanV;
        }
        // 0.5.717: best of all, where the M2's own draw put the tube's mouth this
        // frame (Render::TrackedPoint: the game's sway, bob and lag in it; the
        // user, 714: "when i walk left or right or forwards or back the flame
        // doesnt even shoot out the barrel")
        {
            float tx, ty;
            if (Render::TrackedPoint(tx, ty)) { mx = tx; my = ty; }
        }
        constexpr float NozzleDepth = 18.0f;
        // 0.5.707: the tube's end on the picture; nothing of our flame reaches
        // below it (over the tube and the hand) but a nozzle's width
        const float tipX = mx * W, tipY = my * H, tipPx = W * 0.012f;
        auto AboveTip = [&](float y, float size, float tall) { return std::min(size, 2.0f * std::max(0.0f, tipY - y) / std::max(1.0f, tall) + tipPx); };
        const float nx = (mx * W - W * 0.5f) * NozzleDepth / focal, ny = (my * H - H * 0.5f) * NozzleDepth / focal;
        // age the stream: drag (World at War's flame slows within a second),
        // a little gravity, the chunk swelling as it burns
        std::uniform_real_distribution<float> splash(-1.0f, 1.0f);
        for (Particle& p : g_particles)
        {
            p.age += dt;
            const float drag = expf((p.stopped ? -4.0f : -1.2f) * dt);
            p.vx *= drag; p.vy *= drag; p.vz *= drag;
            p.vy += (p.stopped ? -60.0f : 260.0f) * dt;   // spread on a wall, fire climbs
            p.x += p.vx * dt; p.y += p.vy * dt; p.z += p.vz * dt;
            // 0.5.703: a drip falls from it into the world (World at War's own
            // flame drips too)
            if (!p.stopped && p.dripAt > 0.0f && p.age >= p.dripAt && g_view.valid)
            {
                p.dripAt = 0.0f;
                float at[3];
                Unproject(g_view, W * 0.5f + p.x * focal / p.z, H * 0.5f + p.y * focal / p.z, p.z, W, H, at);
                const float vel[3] = { g_view.fwd[0] * 500.0f, g_view.fwd[1] * 500.0f, g_view.fwd[2] * 500.0f };
                Leave(Kind::Drip, at, vel, 12.0f, true);
            }
            // 0.5.703: a wall ahead: the chunk stops there and spreads over it,
            // and leaves fire and smoke where it landed
            if (!p.stopped && p.z >= g_ownReach - 6.0f)
            {
                if (g_view.valid)
                {
                    float at[3];
                    Unproject(g_view, W * 0.5f + p.x * focal / p.z, H * 0.5f + p.y * focal / p.z, std::max(8.0f, g_ownReach - 6.0f), W, H, at);
                    const float vel[3] = { g_view.fwd[0] * 300.0f, g_view.fwd[1] * 300.0f, g_view.fwd[2] * 300.0f };
                    Land(at, vel, 30.0f, true);
                }
                p.stopped = true;
                p.z = std::max(NozzleDepth + 1.0f, g_ownReach - 6.0f);
                p.vz = 0.0f;
                p.vx = 160.0f * splash(g_random); p.vy = 120.0f * splash(g_random) - 40.0f;
            }
        }
        g_particles.erase(std::remove_if(g_particles.begin(), g_particles.end(), [](const Particle& p) { return p.age >= p.life || p.z < 2.0f; }), g_particles.end());
        if (burning)
        {
            // 150 chunks a second, toward the aim 600 units out (the crosshair)
            g_emitCarry += dt * 150.0;
            std::uniform_real_distribution<float> u(0.0f, 1.0f);
            const float tx = -nx, ty = -ny, tz = std::max(60.0f, std::min(600.0f, g_ownReach)) - NozzleDepth;
            const float tl = sqrtf(tx * tx + ty * ty + tz * tz);
            while (g_emitCarry >= 1.0)
            {
                g_emitCarry -= 1.0;
                const float speed = 900.0f + 150.0f * u(g_random), spread = 0.04f;
                const float dx = tx / tl + (u(g_random) - 0.5f) * spread, dy = ty / tl + (u(g_random) - 0.5f) * spread - 0.03f, dz = tz / tl;
                const float lead = u(g_random) * dt;   // spread over the frame, not in clumps
                g_particles.push_back({ lead, 0.55f + 0.2f * u(g_random), nx, ny, NozzleDepth, dx * speed, dy * speed, dz * speed,
                                        0.8f + 0.5f * u(g_random), (u(g_random) - 0.5f) * 0.6f, u(g_random) < 0.5f ? 0 : 1, false,
                                        u(g_random) < 0.25f ? 0.03f + 0.15f * u(g_random) : 0.0f });
            }
        }
        else g_emitCarry = 0.0;
        const ImTextureID fire = (ImTextureID)(uintptr_t)Overlay::ImageTexture(Flame::Dir() + "flame_fire.png");
        const ImTextureID lick = (ImTextureID)(uintptr_t)Overlay::ImageTexture(Flame::Dir() + "flame_lick.png");
        if (!fire) return;
        ImDrawList* list = ImGui::GetBackgroundDrawList();
        WorldDraw(list, fire, lick, W, H, now, dt);   // others' first: ours is nearer
        EmberDraw(list, fire, W, H, dt);
        HeatBar(W, H, now);   // 0.5.720
        // 0.5.705: our flame leaves the nozzle as World at War's first person
        // ribbon (flame_firstperson_mp: streamFlame 2.5 units across to 15
        // over 246.7, looping in 0.41 s; streamFuel 4 to 12 over 200, 0.15 s):
        // as narrow as the nozzle at its tip (0.012 of the picture), widening
        // as it goes, toward the crosshair, drooping, stopped by a wall. The
        // flying chunks show only past 0.7 of it (the user, 704: the flame
        // came out as a ball over the hand: the chunks were 70 to 100 pixels
        // across at the nozzle, the first thing they drew)
        const float ribbon = burning ? std::max(40.0f, std::min(g_ownReach, 246.7435f)) : 0.0f;
        if (ribbon > 0.0f)
        {
            struct Piece { float x, y, size, angle, alpha, z; int frame; bool core; };
            std::vector<Piece> pieces;
            const float nozzlePx = W * 0.012f;
            auto at = [&](float t, float& x, float& y, float& z)
            {
                z = NozzleDepth + (ribbon - NozzleDepth) * t;
                const float d = t * ribbon, fall = 0.5f * 250.0f * (d / 1000.0f) * (d / 1000.0f);
                const float px = nx * (1.0f - t), py = ny * (1.0f - t) + fall;
                x = W * 0.5f + px * focal / z; y = H * 0.5f + py * focal / z;
            };
            constexpr int Segments = 40;
            for (int part = 0; part < 2; part++)
            {
                const bool core = part == 1;
                const float length = core ? std::min(1.0f, 200.0f / ribbon) : 1.0f;
                const float loop = core ? 0.1528f : 0.4088f;
                for (int i = 0; i < Segments; i++)
                {
                    const float t = length * powf((i + 0.5f) / Segments, 2.0f);   // close together at the nozzle
                    float x, y, z, x2, y2, z2;
                    at(t, x, y, z);
                    at(std::min(1.0f, t + 0.02f), x2, y2, z2);
                    const float u = t / length;
                    const float across = core ? 4.0f + 8.0f * u : 2.5f + 12.5f * u;
                    // 0.5.707: a cone from the tube's end (the user, 706: "coming out of
                    // the barrel, not the end of the tip"): the nozzle's width at the
                    // tip, widening with the distance from it on the picture, never
                    // more than the stream's true size, so no piece reaches back over
                    // the tube (706's pieces, 1.6 x the stream's size and centred on
                    // it, hung back over the barrel and the hand)
                    const float fromTip = hypotf(x - tipX, y - tipY);
                    const float size = (core ? 0.6f : 1.0f) * std::min(nozzlePx + 0.8f * fromTip, std::max(nozzlePx, across * focal / z));
                    const float angle = (fabsf(x2 - x) + fabsf(y2 - y) > 0.5f) ? atan2f(y2 - y, x2 - x) - 1.5707963f : 3.14159265f;
                    const float alpha = (core ? 0.75f : 0.9f) * std::min(1.0f, (1.0f - u) / 0.25f);
                    const int frame = core ? int((now / loop + i * 0.37) * 16.0) % 16 : (i & 1) * 32 + 4 + int((now / loop + i * 0.21) * 16.0) % 16;
                    pieces.push_back({ x, y, size, angle, alpha, z - (core ? 0.5f : 0.0f), frame, core });
                }
            }
            std::sort(pieces.begin(), pieces.end(), [](const Piece& a, const Piece& b) { return a.z > b.z; });
            for (const Piece& p : pieces)
            {
                if (p.core && lick) Sprite(list, lick, 4, 4, p.frame, p.x, p.y, AboveTip(p.y, p.size, 1.0f), p.angle, p.alpha, 1.0f);
                else if (!p.core) Sprite(list, fire, 8, 8, p.frame, p.x, p.y, AboveTip(p.y, p.size, 1.2f), p.angle, p.alpha, 1.2f);
            }
        }
        if (g_particles.empty()) return;
        // far ones first, the nozzle's last (on top)
        std::vector<const Particle*> order;
        for (const Particle& p : g_particles) order.push_back(&p);
        std::sort(order.begin(), order.end(), [](const Particle* a, const Particle* b) { return a->z > b->z; });
        for (const Particle* p : order)
        {
            // 0.5.705: inside the ribbon a flying chunk is the ribbon (fading in toward its end)
            const float show = (ribbon > 0.0f && !p->stopped) ? std::clamp((p->z - 0.7f * ribbon) / (0.3f * ribbon), 0.0f, 1.0f) : 1.0f;
            if (show <= 0.0f) continue;
            const float s = p->age / p->life;                    // 0 at the nozzle, 1 burnt out
            const float x = W * 0.5f + p->x * focal / p->z, y = H * 0.5f + p->y * focal / p->z;
            // a chunk 0.8 units across at the nozzle, swelling to about 17
            // (0.5.685's 3 to 40 filled the middle of the picture: a chunk 100
            // units out is 15 pixels a unit at 1904 across)
            // (0.5.707: 0.55 of it, 1.3 tall instead of 2, and never over the tube:
            // 706's far chunks, 300 pixels tall over the crosshair, reached down
            // over the gun)
            const float worldSize = 0.55f * (0.8f + 16.0f * powf(s, 0.8f)) * p->grow;
            const float size = ribbon > 0.0f ? AboveTip(y, worldSize * focal / p->z, 1.3f) : worldSize * focal / p->z;
            // drawn along its own motion on the screen (the cells are tall)
            const float sx = (p->vx * p->z - p->x * p->vz), sy = (p->vy * p->z - p->y * p->vz);
            const float along = atan2f(sy, sx) - 1.5707963f;
            const int frame = p->sequence * 32 + std::min(31, int(s * 32.0f));
            const float alpha = 0.9f * show * std::min(1.0f, p->age / 0.04f) * (1.0f - s * s);
            if (size < 0.8f) continue;
            Sprite(list, fire, 8, 8, frame, x, y, size, along + p->spin * s, alpha, 1.3f);
        }
        if (burning && lick)
        {
            // the pilot lick at the tip, the nozzle's size (was 0.05 of the height, over the hand)
            const int frame = int(now * 24.0) % 16;
            Sprite(list, lick, 4, 4, frame, mx * W, my * H - W * 0.006f, W * 0.016f, 0.0f, 0.85f, 1.0f);
        }
    }

    void Register()
    {
        static bool done = false;
        if (done) return;
        done = true;
        Overlay::SetWeaponHud(HudActive, HudDraw);
    }
}

std::string Flame::Dir()
{
    static std::string dir;
    if (dir.empty())
    {
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string d(path);
        const size_t slash = d.find_last_of("\\/");
        dir = (slash == std::string::npos ? std::string(".") : d.substr(0, slash)) + "\\custom\\flamethrower\\";
    }
    return dir;
}

void Flame::Held(bool held)
{
    Register();
    std::lock_guard<std::mutex> guard(g_lock);
    if (held != g_held)
    {
        printf("flamethrower: %s\n", held ? "in hand" : "put away");
        fflush(stdout);
    }
    g_held = held;
    if (!held) { g_firing = false; g_particles.clear(); }
}

void Flame::OwnShot()
{
    std::lock_guard<std::mutex> guard(g_lock);
    const double now = Now();
    if (!g_firing)
    {
        g_firing = true;
        g_fireStart = now;
        Audio::PlayClip(Dir() + "flm_ignite.wav", 0.9f);
        g_loopNext = now + 0.12;   // the burning loop just after the ignition
        static int told = 0;
        if (told++ < 20) { printf("flamethrower: trigger pulled\n"); fflush(stdout); }
    }
    g_lastShot = now;
}

void Flame::OtherShot(uint32_t entity, float gain)
{
    std::lock_guard<std::mutex> guard(g_lock);
    const double now = Now();
    double& next = g_otherLoop[entity];
    if (now < next || gain <= 0.01f) return;
    next = now + LoopLength;
    Audio::PlayClip(Dir() + "flm_loop_far.wav", gain);
}

void Flame::Tick()
{
    std::lock_guard<std::mutex> guard(g_lock);
    const double now = Now();
    if (!g_firing) return;
    if (now - g_lastShot > ShotGap || !g_held)
    {
        g_firing = false;
        Audio::PlayClip(Dir() + "flm_cooldown.wav", 0.8f);
        return;
    }
    if (now >= g_loopNext)
    {
        g_loopNext = now + LoopLength;
        Audio::PlayClip(Dir() + "flm_loop.wav", 0.75f);
    }
}

void Flame::Camera(const float eye[3], const float axes[9], float fov)
{
    std::lock_guard<std::mutex> guard(g_lock);
    for (int k = 0; k < 3; k++)
    {
        g_view.eye[k] = eye[k];
        g_view.fwd[k] = axes[k];
        g_view.left[k] = axes[3 + k];
        g_view.up[k] = axes[6 + k];
    }
    g_view.fov = fov;
    g_view.at = Now();
    g_view.valid = std::isfinite(eye[0]) && std::isfinite(axes[0]) && fov > 1.0f;
}

bool Flame::Eye(float eye[3])
{
    std::lock_guard<std::mutex> guard(g_lock);
    if (!g_view.valid) return false;
    for (int k = 0; k < 3; k++) eye[k] = g_view.eye[k];
    return true;
}

void Flame::OwnReach(float units)
{
    std::lock_guard<std::mutex> guard(g_lock);
    g_ownReach = std::clamp(units, 8.0f, 600.0f);
}

void Flame::OtherStream(uint32_t entity, const float muzzle[3], const float dir[3], float reach, bool visible)
{
    std::lock_guard<std::mutex> guard(g_lock);
    Stream& s = g_streams[entity];
    s.lastShot = Now();
    for (int k = 0; k < 3; k++) { s.muzzle[k] = muzzle[k]; s.dir[k] = dir[k]; }
    s.reach = std::clamp(reach, 4.0f, 600.0f);
    s.visible = visible;
}

bool Flame::Firing()
{
    std::lock_guard<std::mutex> guard(g_lock);
    return Burning(Now());
}

float Flame::Fov()
{
    std::lock_guard<std::mutex> guard(g_lock);
    return g_view.valid ? g_view.fov : 80.0f;
}

void Flame::Nozzle(const float at[3], float fov)
{
    std::lock_guard<std::mutex> guard(g_lock);
    g_nozzleFov = fov;
    for (int k = 0; k < 3; k++) g_nozzle[k] = at[k];
    g_nozzleAt = Now();
}

void Flame::Heat(float heat, bool overheated)
{
    std::lock_guard<std::mutex> guard(g_lock);
    g_heat = heat;
    g_overheated = overheated;
    g_heatAt = Now();
}
