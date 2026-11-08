// 0.5.739: World at War's signal flare: its burning and its blinding.
//
// World at War (weapons/mp/signal_flare_mp, maps/mp/_flare.gsc in the user's
// own install): a flare stick thrown as the smoke grenade is (its offhand
// class), lit a second after (fuseTime 1), burning ten seconds as a white
// glow with sparks (misc/fx_flare_sky_white_10sec_mp: fxt_light_flare3_128).
// For eight of them (flare_duration) every player within 400 units
// (flare_effect_radius) whose sight reaches it has the flare's vision put
// over his view at 1 - distance / 400 (VisionSetLerpRatio: vision/flare.vision,
// bloom 7, the film tinted orange-red, brightness up), checked every 0.3 s
// (here also: while it is within 55 degrees of where he looks);
// looking away lets it go after 0.45 s, the flare burning out after 0.65 s.
// No side is spared (the script asks none), the thrower neither.
//
// Here: the glow, its core and sparks drawn where the flare lies, over the
// picture (Overlay::SetFlareHud), while our eye sees it; the vision as a wash
// of its tint over the whole picture with the glow swelling round the flare.
// The sight lines are mp_fixes.cpp's (Flare::Seen, the M2's wall trace).
// Its sounds, all World at War's own: its ignition (flare_exp, one of four)
// and its burning trail as it lights (0.5.753: xWMA, decoded by the port's
// FFmpeg at the coding's own rate, tools/waw/make_flare.py; the M2's before),
// the blinded ring and its fade (flareshock_eq).
// COD3_FLARE_TEST="x y z": a flare lit there every 12 s (screenshots: a
// throw cannot be scripted).
#include "flare.h"
#include "audio_out.h"
#include "flamethrower.h"
#include "overlay.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <random>
#include <vector>

#include <Windows.h>

#include "imgui.h"

namespace
{
    std::mutex g_lock;

    constexpr double GlowSeconds = 10.0;      // fx_flare_sky_white_10sec_mp
    constexpr double BlindSeconds = 8.0;      // flare_duration
    constexpr float Radius = 400.0f;          // flare_effect_radius
    constexpr double LookAwayWait = 0.45;     // flareLookAwayFadeWait
    constexpr double BurnOutWait = 0.65;      // flareBurnOutFadeWait
    constexpr size_t MostFlares = 16;

    struct Spark { float p[3], v[3], age, life; };
    struct Burning
    {
        uint32_t id = 0;
        float at[3]{};
        double start = 0.0, emitCarry = 0.0;   // (start: when it lights, after the fuse)
        bool seen = false, lit = false;
        std::vector<Spark> sparks;
    };
    std::vector<Burning> g_flares;

    struct View { bool valid = false; float eye[3]{}, fwd[3]{}, left[3]{}, up[3]{}; float fov = 80.0f; };
    View g_view;

    // the vision's strength now (0..1), and when it last had a reason to stay
    float g_level = 0.0f;
    double g_heldUntil = -10.0;
    bool g_blinded = false;
    double g_lastDraw = -1.0;
    std::mt19937 g_random{ 4321 };

    double Now()
    {
        static const auto start = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }

    float Distance(const float a[3], const float b[3])
    {
        const float d[3] = { a[0] - b[0], a[1] - b[1], a[2] - b[2] };
        return sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    }

    // a sound's gain at this distance from our eye (none past 2000)
    float Gain(const float at[3])
    {
        if (!g_view.valid) return 0.6f;
        const float f = std::clamp(1.0f - Distance(at, g_view.eye) / 2000.0f, 0.0f, 1.0f);
        return f * f;
    }

    // a point of the world on the picture (flamethrower.cpp's Project: the
    // game's fov across a 4:3 picture, widened to the window's shape)
    bool Project(const float p[3], float W, float H, float& x, float& y, float& scale)
    {
        const View& v = g_view;
        const float d[3] = { p[0] - v.eye[0], p[1] - v.eye[1], p[2] - v.eye[2] };
        const float depth = d[0] * v.fwd[0] + d[1] * v.fwd[1] + d[2] * v.fwd[2];
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

    void Sprite(ImDrawList* list, ImTextureID tex, float cx, float cy, float size, float angle, ImU32 colour)
    {
        const float c = cosf(angle) * size * 0.5f, s = sinf(angle) * size * 0.5f;
        list->AddImageQuad(tex, ImVec2(cx - c + s, cy - s - c), ImVec2(cx + c + s, cy + s - c), ImVec2(cx + c - s, cy + s + c),
                           ImVec2(cx - c - s, cy - s + c), ImVec2(0, 0), ImVec2(1, 0), ImVec2(1, 1), ImVec2(0, 1), colour);
    }

    ImU32 Colour(int r, int g, int b, float alpha) { return IM_COL32(r, g, b, int(std::clamp(alpha, 0.0f, 1.0f) * 255.0f)); }

    bool HudActive()
    {
        std::lock_guard<std::mutex> guard(g_lock);
        return !g_flares.empty() || g_level > 0.0f;
    }

    // World at War's vision over the picture: what the strongest flare our
    // eye sees within 400 units asks for, held after it is lost for the
    // script's waits, then let go
    void Blinding(double now, float dt)
    {
        float want = 0.0f;
        bool ending = false;   // the one seen is about to burn out: the longer wait
        for (const Burning& f : g_flares)
        {
            if (now < f.start || now - f.start > BlindSeconds || !g_view.valid || !f.seen) continue;
            const float d = Distance(f.at, g_view.eye);
            if (d >= Radius) continue;
            // looked at: within 55 degrees of the view's middle (the script's
            // "look away" wait; a flare at one's feet counts, nearer than 40)
            const float dir[3] = { f.at[0] - g_view.eye[0], f.at[1] - g_view.eye[1], f.at[2] - g_view.eye[2] };
            const float ahead = (dir[0] * g_view.fwd[0] + dir[1] * g_view.fwd[1] + dir[2] * g_view.fwd[2]) / std::max(d, 1.0f);
            if (ahead < 0.57f && d > 40.0f) continue;
            want = std::max(want, 1.0f - d / Radius);
            ending = ending || now - f.start > BlindSeconds - 0.1;
        }
        if (want > 0.0f)
        {
            g_level = std::max(want, g_level - dt * 2.0f);   // nearer at once, farther as one walks away
            g_heldUntil = now + (ending ? BurnOutWait : LookAwayWait);
        }
        else if (g_level > 0.0f && now > g_heldUntil) g_level = std::max(0.0f, g_level - dt / 0.3f);
        if (!g_blinded && g_level > 0.25f)
        {
            g_blinded = true;
            Audio::PlayClip(Flare::Dir() + "flare_shock.wav", 0.45f);
            printf("flare: blinded (%.2f)\n", g_level); fflush(stdout);
        }
        else if (g_blinded && g_level <= 0.0f)
        {
            g_blinded = false;
            Audio::PlayClip(Flare::Dir() + "flare_shock_fade.wav", 0.35f);
            printf("flare: sight back\n"); fflush(stdout);
        }
    }

    void HudDraw(int width, int height)
    {
        std::lock_guard<std::mutex> guard(g_lock);
        const double now = Now();
        const float dt = g_lastDraw < 0 ? 0.0f : float(std::clamp(now - g_lastDraw, 0.0, 0.1));
        g_lastDraw = now;
        const float W = float(width), H = float(height);
        g_flares.erase(std::remove_if(g_flares.begin(), g_flares.end(), [&](const Burning& f) { return now - f.start > GlowSeconds; }), g_flares.end());
        Blinding(now, dt);
        const ImTextureID star = (ImTextureID)(uintptr_t)Overlay::ImageTexture(Flare::Dir() + "flare_glow.png");
        const ImTextureID soft = (ImTextureID)(uintptr_t)Overlay::ImageTexture(Flare::Dir() + "flare_core.png");
        ImDrawList* list = ImGui::GetBackgroundDrawList();
        std::uniform_real_distribution<float> u(0.0f, 1.0f);
        for (Burning& f : g_flares)
        {
            const float age = float(now - f.start);
            // 0.5.746: in flight (its fuse: World at War's fuseTime 1, age < 0)
            // its spitting sparks only, a little; lit at the fuse's end, where
            // its grenade is then
            const bool flying = age < 0.0f;
            if (!flying && !f.lit)
            {
                f.lit = true;
                // 0.5.753: World at War's own: its ignition (flare_exp: one of
                // flare_00..03, a pop and its hiss) and its burning trail (11 s)
                const int which = int(f.id % 4);
                Audio::PlayClip(Flare::Dir() + "flare_0" + char('0' + which) + ".wav", 0.85f * Gain(f.at));
                Audio::PlayClip(Flare::Dir() + "flare_burn.wav", 0.55f * Gain(f.at));
                static int told = 0;
                if (told < 40) { told++; printf("flare: lit at %.0f %.0f %.0f (%.0f from our eye)\n", f.at[0], f.at[1], f.at[2], g_view.valid ? Distance(f.at, g_view.eye) : -1.0f); fflush(stdout); }
            }
            const float fade = flying ? 0.35f : std::clamp(float(GlowSeconds) - age, 0.0f, 1.5f) / 1.5f * std::min(1.0f, age / 0.15f);
            // sparks thrown up from its head (fx_flare_start: World at War's flare spits)
            f.emitCarry += dt * 45.0 * fade;
            while (f.emitCarry >= 1.0 && f.sparks.size() < 120)
            {
                f.emitCarry -= 1.0;
                const float a = u(g_random) * 6.2831853f, side = 25.0f + 45.0f * u(g_random);
                f.sparks.push_back({ { f.at[0], f.at[1], f.at[2] + 5.0f }, { cosf(a) * side, sinf(a) * side, 70.0f + 110.0f * u(g_random) }, 0.0f,
                                     0.35f + 0.45f * u(g_random) });
            }
            for (Spark& s : f.sparks)
            {
                s.age += dt;
                s.v[2] -= 320.0f * dt;
                for (int k = 0; k < 3; k++) s.p[k] += s.v[k] * dt;
                if (s.p[2] < f.at[2]) { s.p[2] = f.at[2]; s.v[2] *= -0.3f; s.v[0] *= 0.6f; s.v[1] *= 0.6f; }
            }
            f.sparks.erase(std::remove_if(f.sparks.begin(), f.sparks.end(), [](const Spark& s) { return s.age >= s.life; }), f.sparks.end());
            if (!g_view.valid || !f.seen || !soft || !star) continue;
            const float head[3] = { f.at[0], f.at[1], f.at[2] + 5.0f };
            float x, y, scale;
            if (!Project(head, W, H, x, y, scale)) continue;
            const float flicker = 0.86f + 0.14f * sinf(float(now) * 23.0f) * sinf(float(now) * 7.3f + float(f.start));
            auto px = [&](float units, float least) { return std::max(least, units * scale); };
            // the halo, its red-white core, the star of its glare (in flight its core only)
            if (flying) Sprite(list, soft, x, y, px(20.0f * flicker, 4.0f), 0.0f, Colour(255, 220, 200, 0.8f));
            else
            {
                Sprite(list, soft, x, y, px(170.0f * flicker, 10.0f), 0.0f, Colour(255, 120, 100, 0.55f * fade));
                Sprite(list, soft, x, y, px(55.0f * flicker, 6.0f), 0.0f, Colour(255, 235, 225, 0.95f * fade));
                Sprite(list, star, x, y, px(95.0f * flicker, 8.0f), float(now) * 0.15f + float(f.start), Colour(255, 245, 240, 0.85f * fade));
            }
            for (const Spark& s : f.sparks)
            {
                float sx, sy, ss;
                if (!Project(s.p, W, H, sx, sy, ss)) continue;
                const float t = s.age / s.life;
                Sprite(list, soft, sx, sy, std::max(2.0f, 4.0f * ss), 0.0f, Colour(255, 215, 175, (1.0f - t) * fade));
            }
        }
        // the vision: its orange-red tint, brightened, over the picture, and
        // the glow of the nearest seen flare swelling round it
        if (g_level > 0.0f)
        {
            list->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), Colour(255, 150, 110, 0.62f * g_level));
            list->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), Colour(255, 236, 226, 0.22f * g_level));
            if (soft)
                for (const Burning& f : g_flares)
                {
                    if (!f.seen || !g_view.valid || Distance(f.at, g_view.eye) >= Radius) continue;
                    const float head[3] = { f.at[0], f.at[1], f.at[2] + 5.0f };
                    float x, y, scale;
                    if (Project(head, W, H, x, y, scale)) Sprite(list, soft, x, y, W * (0.4f + 1.4f * g_level), 0.0f, Colour(255, 240, 230, 0.7f * g_level));
                }
        }
    }

    void Register()
    {
        static bool done = false;
        if (done) return;
        done = true;
        Overlay::SetFlareHud(HudActive, HudDraw);
    }
}

std::string Flare::Dir()
{
    static std::string dir;
    if (dir.empty())
    {
        char path[MAX_PATH] = {};
        GetModuleFileNameA(nullptr, path, MAX_PATH);
        std::string d(path);
        const size_t slash = d.find_last_of("\\/");
        dir = (slash == std::string::npos ? std::string(".") : d.substr(0, slash)) + "\\custom\\flare\\";
    }
    return dir;
}

uint32_t Flare::Ignite(const float at[3], float fuse)
{
    Register();
    std::lock_guard<std::mutex> guard(g_lock);
    if (!std::isfinite(at[0]) || !std::isfinite(at[1]) || !std::isfinite(at[2])) return 0;
    if (g_flares.size() >= MostFlares) g_flares.erase(g_flares.begin());
    static uint32_t ids = 0;
    Burning f;
    f.id = ++ids;
    for (int k = 0; k < 3; k++) f.at[k] = at[k];
    f.start = Now() + std::max(0.0f, fuse);
    g_flares.push_back(f);
    static int told = 0;
    if (told < 40) { told++; printf("flare: %u thrown at %.0f %.0f %.0f (%.0f from our eye), lights in %.1f s\n", f.id, at[0], at[1], at[2], g_view.valid ? Distance(at, g_view.eye) : -1.0f, fuse); fflush(stdout); }
    return f.id;
}

void Flare::MoveTo(uint32_t id, const float at[3])
{
    std::lock_guard<std::mutex> guard(g_lock);
    for (Burning& f : g_flares)
        if (f.id == id) for (int k = 0; k < 3; k++) f.at[k] = at[k];
}

void Flare::Camera(const float eye[3], const float axes[9], float fov)
{
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
        g_view.valid = std::isfinite(eye[0]) && std::isfinite(axes[0]) && fov > 1.0f;
    }
    // the test's flare, every 12 s
    static float test[3];
    static const bool testing = []() { const char* t = getenv("COD3_FLARE_TEST"); return t && sscanf(t, "%f %f %f", &test[0], &test[1], &test[2]) == 3; }();
    static double next = -1.0;
    if (testing)
    {
        const double now = Now();
        if (next < 0.0) next = now + 3.0;
        if (now >= next) { next = now + 12.0; Ignite(test); }
    }
}

bool Flare::Lit(uint32_t id)
{
    std::lock_guard<std::mutex> guard(g_lock);
    const double now = Now();
    for (const Burning& f : g_flares)
        if (f.id == id) return now >= f.start && now - f.start < GlowSeconds;
    return false;
}

int Flare::Count()
{
    std::lock_guard<std::mutex> guard(g_lock);
    return int(g_flares.size());
}

bool Flare::At(int index, float at[3], uint32_t& id)
{
    std::lock_guard<std::mutex> guard(g_lock);
    if (index < 0 || index >= int(g_flares.size())) return false;
    for (int k = 0; k < 3; k++) at[k] = g_flares[size_t(index)].at[k];
    id = g_flares[size_t(index)].id;
    return true;
}

void Flare::Seen(uint32_t id, bool seen)
{
    std::lock_guard<std::mutex> guard(g_lock);
    for (Burning& f : g_flares)
        if (f.id == id) f.seen = seen;
}
