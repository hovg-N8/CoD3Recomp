// Co-op v40: the co-op heads-up display, World at War's (drawn with the
// overlay's Dear ImGui, over the game's own HUD):
// - the other player's name over his head in his colour, through walls, and
//   his health bar at the screen's lower left;
// - him down: a red cross and "REVIVE" over him with his bleed-out time, and
//   "<name> needs to be revived!" now and then; reviving him: a progress bar;
// - you down: the screen greying red as you bleed out, "You are down" and the
//   time left; being revived: "<name> is reviving you" and the bar;
// - the scores (kills, revives, downs, points; competitive co-op's), held Tab
//   or Back, and at a mission's end.
// The game thread fills a HudState each frame (HudUpdate); the window thread
// draws a copy of it.
#ifndef COD3_MP

#include "coop.h"
#include "overlay.h"
#include "input.h"
#include "settings.h"

#include "../thirdparty/imgui/imgui.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>

namespace Coop
{
    namespace
    {
        std::mutex g_lock;
        HudState g_state{};
        double g_stateAt = -10;
        const ImU32 Colour[2] = { IM_COL32(110, 230, 120, 255), IM_COL32(110, 180, 255, 255) };   // the host green, the friend blue
        bool Active()
        {
            std::lock_guard<std::mutex> lock(g_lock);
            return g_state.active && Now() - g_stateAt < 1.0;
        }
        // a point in the world on the screen (the view's place and angles, the game's horizontal fov
        // for 4:3, widened to the window's shape when the picture fills it)
        bool Project(const HudState& s, const float p[3], int width, int height, float& x, float& y)
        {
            const float d[3] = { p[0] - s.eye[0], p[1] - s.eye[1], p[2] - s.eye[2] };
            const float pitch = s.view[0] * 0.0174533f, yaw = s.view[1] * 0.0174533f;
            const float fwd[3] = { cosf(pitch) * cosf(yaw), cosf(pitch) * sinf(yaw), -sinf(pitch) };
            const float right[3] = { sinf(yaw), -cosf(yaw), 0.0f };
            const float up[3] = { sinf(pitch) * cosf(yaw), sinf(pitch) * sinf(yaw), cosf(pitch) };
            const float z = d[0] * fwd[0] + d[1] * fwd[1] + d[2] * fwd[2];
            if (z < 1.0f) return false;
            const float rx = d[0] * right[0] + d[1] * right[1] + d[2] * right[2];
            const float uy = d[0] * up[0] + d[1] * up[1] + d[2] * up[2];
            const float aspect = float(width) / float(std::max(height, 1));
            const float tanH43 = tanf(std::clamp(float(s.fov), 40.0f, 120.0f) * 0.5f * 0.0174533f);
            const float tanH = s.fill ? tanH43 * aspect / (4.0f / 3.0f) : tanH43 * (16.0f / 9.0f) / (4.0f / 3.0f);
            const float tanV = tanH / aspect;
            x = width * 0.5f * (1.0f + rx / (z * tanH));
            y = height * 0.5f * (1.0f - uy / (z * tanV));
            return x > -50 && x < width + 50 && y > -50 && y < height + 50;
        }
        void Text(ImDrawList* dl, float x, float y, ImU32 c, const char* t, float scale = 1.0f, bool centre = true)
        {
            ImFont* f = ImGui::GetFont();
            const float size = ImGui::GetFontSize() * scale;
            const ImVec2 w = f->CalcTextSizeA(size, 10000.0f, 0.0f, t);
            const float px = centre ? x - w.x * 0.5f : x;
            dl->AddText(f, size, ImVec2(px + 1, y + 1), IM_COL32(0, 0, 0, 200), t);
            dl->AddText(f, size, ImVec2(px, y), c, t);
        }
        void Bar(ImDrawList* dl, float x, float y, float w, float h, float frac, ImU32 c)
        {
            dl->AddRectFilled(ImVec2(x - 1, y - 1), ImVec2(x + w + 1, y + h + 1), IM_COL32(0, 0, 0, 170));
            dl->AddRectFilled(ImVec2(x, y), ImVec2(x + w * std::clamp(frac, 0.0f, 1.0f), y + h), c);
        }
        void Draw(int width, int height)
        {
            HudState s;
            { std::lock_guard<std::mutex> lock(g_lock); s = g_state; }
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            const float k = std::max(1.0f, height / 720.0f);
            const int me = s.me, other = 1 - s.me;
            const double t = Now();
            // the other player: his name over his head, a red cross and his time when he is down
            if (s.havePartner)
            {
                float x, y;
                const float head[3] = { s.pos[other][0], s.pos[other][1], s.pos[other][2] + (s.down[other] ? 30.0f : 78.0f) };
                if (Project(s, head, width, height, x, y))
                {
                    const float dx = s.pos[other][0] - s.eye[0], dy = s.pos[other][1] - s.eye[1];
                    const float dist = sqrtf(dx * dx + dy * dy);
                    const float scale = std::clamp(1.4f - dist / 3000.0f, 0.8f, 1.4f) * k;
                    if (s.down[other])
                    {
                        const float pulse = 0.6f + 0.4f * float(sin(t * 6.0));
                        const ImU32 red = IM_COL32(230, 40, 40, int(255 * pulse));
                        const float r = 10.0f * scale;
                        dl->AddRectFilled(ImVec2(x - r * 0.35f, y - r), ImVec2(x + r * 0.35f, y + r), red);
                        dl->AddRectFilled(ImVec2(x - r, y - r * 0.35f), ImVec2(x + r, y + r * 0.35f), red);
                        char line[64]; snprintf(line, sizeof line, "REVIVE  %d", int(std::ceil(s.bleed[other])));
                        Text(dl, x, y + r + 4, red, line, scale * 0.9f);
                    }
                    Text(dl, x, y - 22.0f * scale, Colour[other], s.names[other], scale);
                }
                // his health, lower left
                const float bx = 20 * k, by = height - 70 * k;
                Text(dl, bx, by - 18 * k, Colour[other], s.names[other], k, false);
                Bar(dl, bx, by, 160 * k, 8 * k, s.maxHealth[other] > 0 ? float(s.health[other]) / float(s.maxHealth[other]) : 0.0f,
                    s.down[other] ? IM_COL32(220, 50, 50, 255) : Colour[other]);
                if (s.down[other] && fmod(t, 3.5) < 2.5)
                {
                    char line[96]; snprintf(line, sizeof line, "%s needs to be revived!", s.names[other]);
                    Text(dl, width * 0.5f, height * 0.30f, IM_COL32(255, 80, 80, 255), line, 1.3f * k);
                }
                if (s.down[other] && s.revive[other] > 0.0f)
                {
                    char line[96]; snprintf(line, sizeof line, "Reviving %s", s.names[other]);
                    Text(dl, width * 0.5f, height * 0.58f, IM_COL32(255, 255, 255, 255), line, 1.1f * k);
                    Bar(dl, width * 0.5f - 120 * k, height * 0.58f + 26 * k, 240 * k, 8 * k, s.revive[other], IM_COL32(255, 255, 255, 230));
                }
                else if (s.down[other] && s.nearDowned) Text(dl, width * 0.5f, height * 0.58f, IM_COL32(255, 255, 255, 255), "Hold [Use] to revive", 1.1f * k);
            }
            // you, down: the screen greying red as you bleed out
            if (s.down[me])
            {
                const float left = std::clamp(s.bleed[me] / std::max(s.bleedTotal, 1.0f), 0.0f, 1.0f);
                dl->AddRectFilled(ImVec2(0, 0), ImVec2(float(width), float(height)), IM_COL32(90, 0, 0, int(40 + 110 * (1.0f - left))));
                Text(dl, width * 0.5f, height * 0.22f, IM_COL32(255, 70, 70, 255), "YOU ARE DOWN", 1.6f * k);
                char line[96];
                if (s.revive[me] > 0.0f) snprintf(line, sizeof line, "%s is reviving you", s.names[other]);
                else snprintf(line, sizeof line, "%s can revive you: %d s", s.names[other], int(std::ceil(s.bleed[me])));
                Text(dl, width * 0.5f, height * 0.22f + 34 * k, IM_COL32(255, 255, 255, 255), line, 1.1f * k);
                Bar(dl, width * 0.5f - 120 * k, height * 0.22f + 64 * k, 240 * k, 8 * k, s.revive[me] > 0.0f ? s.revive[me] : left,
                    s.revive[me] > 0.0f ? IM_COL32(255, 255, 255, 230) : IM_COL32(220, 50, 50, 230));
            }
            // the scores: Tab or Back held, and at the end of a mission
            const bool held = (GetAsyncKeyState(VK_TAB) & 0x8000) != 0 || (Input::State().buttons & 0x0020) != 0;
            if (held || t < s.scoresUntil)
            {
                const float w = 420 * k, h = 140 * k, x0 = width * 0.5f - w * 0.5f, y0 = height * 0.62f;
                dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), IM_COL32(0, 0, 0, 170), 6.0f);
                Text(dl, x0 + 14 * k, y0 + 10 * k, IM_COL32(255, 255, 255, 255), "CO-OP", 1.1f * k, false);
                const char* heads[5] = { "", "Kills", "Revives", "Downs", "Score" };
                const int columns = s.scoring ? 5 : 4;
                for (int c = 1; c < columns; c++) Text(dl, x0 + (150 + (c - 1) * 70) * k, y0 + 40 * k, IM_COL32(200, 200, 200, 255), heads[c], k);
                for (int p = 0; p < 2; p++)
                {
                    const float yy = y0 + (66 + p * 30) * k;
                    Text(dl, x0 + 14 * k, yy, Colour[p], s.names[p], k, false);
                    const int v[4] = { s.kills[p], s.revives[p], s.downs[p], s.score[p] };
                    for (int c = 0; c < columns - 1; c++) { char n[16]; snprintf(n, sizeof n, "%d", v[c]); Text(dl, x0 + (150 + c * 70) * k, yy, IM_COL32(255, 255, 255, 255), n, k); }
                }
            }
            // points as they come (competitive co-op's "+10")
            if (s.scoring && t < s.pointsUntil && s.points[me] != 0)
            {
                char n[16]; snprintf(n, sizeof n, "%+d", s.points[me]);
                const float a = float(std::clamp((s.pointsUntil - t) / 1.2, 0.0, 1.0));
                Text(dl, width * 0.5f + 40 * k, height * 0.5f - 30 * k - (1.0f - a) * 20 * k, IM_COL32(255, 230, 90, int(255 * a)), n, 1.2f * k);
            }
        }
    }

    void HudFrame(PPCContext& ctx, uint8_t* base)
    {
        (void)ctx;
        HudState s{};
        const uint32_t me = Me(base);
        const uint32_t rec = Record(base, me);
        s.me = IsHost() ? 0 : 1;
        const int other = 1 - s.me;
        s.active = me && rec && InMission(base) && PeerConnected() && (IsHost() ? SecondPlayer() != 0 : JoinFrozen());
        if (!s.active) { HudUpdate(s); return; }
        const Settings::Values v = Settings::Get();
        s.fov = v.fov; s.fill = v.fillScreen;
        const std::string mine = MyName();
        std::string theirs = PeerName();
        if (theirs.empty()) theirs = IsHost() ? "Friend" : "Host";
        strncpy(s.names[s.me], mine.c_str(), 31); strncpy(s.names[other], theirs.c_str(), 31);
        // this copy's view: the player's eye (his place and view height) and his view angles
        for (int k = 0; k < 3; k++) { s.pos[s.me][k] = F(base, me + 336 + 4 * k); s.view[k] = F(base, rec + 172 + 4 * k); }
        memcpy(s.eye, s.pos[s.me], 12);
        s.eye[2] += float(std::clamp(int32_t(Guest::Read32(base, rec + 184)), 8, 80));
        s.health[s.me] = int32_t(Guest::Read32(base, me + 776)); s.maxHealth[s.me] = int32_t(Guest::Read32(base, me + 780));
        // the other player
        if (IsHost())
        {
            const uint32_t two = SecondPlayer();
            s.havePartner = two && Valid(two) && base[two] == 1;
            if (s.havePartner)
            {
                for (int k = 0; k < 3; k++) s.pos[1][k] = F(base, two + 336 + 4 * k);
                s.health[1] = int32_t(Guest::Read32(base, two + 776)); s.maxHealth[1] = int32_t(Guest::Read32(base, two + 780));
            }
        }
        else s.havePartner = JoinPartner(base, s.pos[0], s.health[0], s.maxHealth[0]);
        LastStandHud(s);
        s.scoring = ScoringOn();
        if (s.havePartner && s.down[other] && !s.down[s.me])
        {
            const float dx = s.pos[other][0] - s.pos[s.me][0], dy = s.pos[other][1] - s.pos[s.me][1];
            s.nearDowned = dx * dx + dy * dy < 80.0f * 80.0f;
        }
        HudUpdate(s);
    }

    void HudUpdate(const HudState& s)
    {
        static bool registered = false;
        if (!registered) { registered = true; Overlay::SetHud(Active, Draw); }
        std::lock_guard<std::mutex> lock(g_lock);
        g_state = s;
        g_stateAt = Now();
    }
}

#endif
