// Custom rules for a System Link match (a PC addition, multiplayer only).
//
// The System Link "Advanced Options" screen (the title's
// CreateLanSessionAdvancedMenu, MP_Xbox_system_link_CGA.PANEL) has six rows:
// time limit, score limit, auto team balance, team damage, voting and the
// punish system. Its panel has room for six and no more, so the extra rules
// go on a second page of the same screen, in the same six rows, the way the
// title's own scoreboards page with the shoulder buttons: L1 or R1 (or Page
// Up and Page Down, or the grenade keys) turns between "Advanced Options"
// and "Custom Rules". Everything on screen is still the title's own panel,
// font and spinners; only the words in the rows change.
//
// Custom Rules:
//   PLAYER HEALTH       50% to 300% of the usual (g_player_maxhealth)
//   RESPAWN TIME        the title's own mp_respawntime setting, which it
//                       stores and applies but never put on a System Link
//                       screen
//   ROUND LIMIT         the title's own mp_roundlimit, likewise
//   LAST MAN STANDING   the title's own mp_lastmanstanding, likewise
//   GRAVITY             g_gravity, from the usual down to a quarter
//   MOVEMENT SPEED      g_speed, up to 1.4 times the usual
//
// The first three of the title's own are kept where it keeps the host's
// choices (the game settings block the screen writes and the session's start
// reads); the rest are applied, as variables, where the title applies its own
// (the function that turns those choices into mp_* variables at the start of
// a session). The multiples are of each variable's default as the title
// registered it, and a rule left at its usual value puts the default back,
// so nothing is changed that nobody asked for.
//
// Only the multiplayer executable has this screen, so the file is only
// built there, and its addresses are the multiplayer program's own
// (make_mp_host.py copies it unchanged).

#ifdef COD3_MP

#include "kernel.h"
#include "net.h"
#include "dom.h"
#include "settings.h"
#include "class_editor.h"

#include <winsock2.h>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <thread>

void BotsOnRoster(const uint8_t* packet, int size, unsigned int from);   // bots.cpp (0.5.194)
void BotsOnStates(const uint8_t* packet, int size, unsigned int from);   // bots.cpp (0.5.207)
void MpFixesOnPlanePacket(const uint8_t* packet, int size, unsigned int from);   // mp_fixes.cpp (0.5.275): the UAV plane
void BotsAsk(int count);   // bots.cpp: bots wanted (clamped to the player limit's room)

// The originals of the functions hooked below.
extern "C" PPC_FUNC(__imp__sub_824D5A08);
extern "C" PPC_FUNC(__imp__sub_824E9540);
extern "C" PPC_FUNC(__imp__sub_823A8F70);
extern "C" PPC_FUNC(__imp__sub_82394628);
extern "C" PPC_FUNC(__imp__sub_823945C0);
extern "C" PPC_FUNC(__imp__sub_823F4078);
extern "C" PPC_FUNC(__imp__sub_823A1C48);   // the class screen's load
extern "C" PPC_FUNC(__imp__sub_824799D8);   // the level's setup: g_gravity from the map's own

namespace
{
    // The game settings block the System Link screens fill in, and the
    // fields this file reads and writes (bytes).
    constexpr uint32_t Settings = 0x82680CA8;
    constexpr uint32_t FieldLastManStanding = 90;
    constexpr uint32_t FieldRoundLimit = 97;      // index into 1, 4, 8 ... 24, unlimited
    constexpr uint32_t FieldRespawnTime = 99;     // index into 0, 5, 10 seconds

    // CreateLanSessionAdvancedMenu: its virtual table, and where it keeps
    // the panel's text and its six spinners.
    constexpr uint32_t AdvancedMenuVtable = 0x8208310C;
    constexpr uint32_t TitleText = 140, DescriptionText = 144, HelpText = 148;
    constexpr uint32_t RowLabels = 152;     // + 8 a row
    constexpr uint32_t RowSpinners = 248;   // + 4 a row
    constexpr int Rows = 6;

    // A spinner (FEMenuListBox): its value's text element, the choice, how
    // many choices and the choices (string objects, the text at +12).
    constexpr uint32_t SpinnerText = 16, SpinnerIndex = 24, SpinnerCount = 28, SpinnerItems = 32;

    // The title's functions.
    constexpr uint32_t SpinnerClear = 0x824E8EC8;
    constexpr uint32_t SpinnerAdd = 0x824FFA58;
    constexpr uint32_t MakeString = 0x821E0E48;
    constexpr uint32_t CvarFind = 0x8254A468;
    constexpr uint32_t CvarSet = 0x8254F0C8;   // (name, value or 0 for the default, force)

    // Variables, by the names the title already has in its image.
    constexpr uint32_t NameMaxHealth = 0x820647D0;   // "g_player_maxhealth"
    constexpr uint32_t NameGravity = 0x82060D3C;     // "g_gravity"
    constexpr uint32_t NameSpeed = 0x82060D4C;       // "g_speed"

    struct Rule
    {
        const char* label;
        const char* choices[8];
        int count;
    };
    const Rule Page2[Rows] = {
        { "PLAYER HEALTH", { "50%", "100%", "150%", "200%", "300%" }, 5 },
        { "RESPAWN TIME", { "INSTANT", "5 SECONDS", "10 SECONDS" }, 3 },
        { "ROUND LIMIT", { "1", "4", "8", "12", "16", "20", "24", "UNLIMITED" }, 8 },
        { "LAST MAN STANDING", { "MPFRONTEND_OFF", "MPFRONTEND_ON" }, 2 },
        { "GRAVITY", { "NORMAL", "LOW", "MOON" }, 3 },
        { "MOVEMENT SPEED", { "NORMAL", "FAST", "VERY FAST" }, 3 },
    };
    const float HealthScale[] = { 0.5f, 1.0f, 1.5f, 2.0f, 3.0f };
    const float GravityScale[] = { 1.0f, 0.5f, 0.25f };
    const float SpeedScale[] = { 1.0f, 1.2f, 1.4f };

    // The first page, as the title builds it (its load and its setup).
    const char* const Page1Labels[Rows] = {
        "MPFRONTEND_TIME_LIMIT_ALLCAPS", "MPFRONTEND_SCORE_LIMIT_ALLCAPS",
        "MPFRONTEND_AUTO_TEAM_BALANCE_ALLCAPS", "MPFRONTEND_TEAM_DAMAGE_ALLCAPS",
        "MPFRONTEND_AAR_VOTING_OPTION_ALLCAPS", "MPFRONTEND_PENALTY_VOTE_ALLCAPS",
    };
    const char* const TimeLimits[] = { "5", "10", "20", "30", "60" };

    int g_page = 0;   // 0 the title's page, 1 Custom Rules, 2 Game Modes
    int g_health = 1, g_gravity = 0, g_speed = 0;
    int g_domination = 0;   // Game Modes: Domination on a Team Battle on a map with its flags (dom.cpp)

    // The third page, Game Modes: modes the title has the makings of but
    // never finished, made here (dom.cpp). One row so far; the rest blank.
    const Rule Page3[Rows] = {
        { "DOMINATION", { "MPFRONTEND_OFF", "ON (TEAM BATTLE, ROUEN)" }, 2 },
        // 0.5.277: bots as a match setting (Justin: anyone with the build who
        // hosts turns bots on in the lobby). The host's server adds them at
        // the match's start (bots.cpp BotsAsk, as the console's bot_add);
        // the others get them from the host (the bots' roster). FILL: as
        // many as the player limit leaves (22 at 24 players, 30 at 32).
        { "BOTS", { "MPFRONTEND_OFF", "2", "4", "8", "12", "16", "22", "FILL" }, 8 },
        { "", { " " }, 1 }, { "", { " " }, 1 }, { "", { " " }, 1 }, { "", { " " }, 1 },
    };
    const int BotCounts[] = { 0, 2, 4, 8, 12, 16, 22, 99 };
    int g_botChoice = 0;
    bool g_touched[3] = {};

    // Text the title is handed lives in guest memory for good: an element
    // may keep the pointer.
    uint32_t GuestString(uint8_t* base, const char* text)
    {
        static std::unordered_map<std::string, uint32_t> strings;
        static uint32_t block = 0, used = 0;
        auto found = strings.find(text);
        if (found != strings.end()) return found->second;
        const uint32_t size = uint32_t(strlen(text)) + 1;
        if (block == 0 || used + size > 0x1000)
        {
            block = Guest::AllocatePhysical(0x1000);
            used = 0;
        }
        const uint32_t at = block + used;
        memcpy(base + at, text, size);
        used += (size + 3) & ~3u;
        strings.emplace(text, at);
        return at;
    }

    // Calls a function of the title below the caller's stack.
    uint32_t Call(PPCContext& ctx, uint8_t* base, uint32_t function,
                  uint32_t r3, uint32_t r4 = 0, uint32_t r5 = 0)
    {
        PPCContext call = ctx;
        call.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
        call.r3.u32 = r3;
        call.r4.u32 = r4;
        call.r5.u32 = r5;
        call.lr = 0;
        Guest::Lookup(function)(call, base);
        return call.r3.u32;
    }
    uint32_t Virtual(uint32_t object, uint32_t offset, uint8_t* base)
    {
        return Guest::Read32(base, Guest::Read32(base, object) + offset);
    }

    // A text element of the panel: its text by key (the title looks the key
    // up and shows it as it is when there is no such key).
    void SetText(PPCContext& ctx, uint8_t* base, uint32_t element, const char* text)
    {
        if (element == 0) return;
        Call(ctx, base, Virtual(element, 132, base), element, GuestString(base, text));
    }

    void ShowChoice(PPCContext& ctx, uint8_t* base, uint32_t spinner)
    {
        const int index = int16_t(Guest::Read16(base, spinner + SpinnerIndex));
        const uint32_t items = Guest::Read32(base, spinner + SpinnerItems);
        const uint32_t item = items ? Guest::Read32(base, items + 4 * index) : 0;
        const uint32_t text = item ? item + 12 : GuestString(base, "");
        const uint32_t element = Guest::Read32(base, spinner + SpinnerText);
        if (element != 0) Call(ctx, base, Virtual(element, 144, base), element, text);
    }

    void Fill(PPCContext& ctx, uint8_t* base, uint32_t spinner, const char* const* choices, int count, int index)
    {
        if (spinner == 0) return;
        Call(ctx, base, SpinnerClear, spinner);
        const uint32_t slot = (ctx.r1.u32 - 0x800) & ~0xFu;
        for (int i = 0; i < count; i++)
        {
            Call(ctx, base, MakeString, slot, GuestString(base, choices[i]));
            Call(ctx, base, SpinnerAdd, spinner, slot);
        }
        const int filled = int16_t(Guest::Read16(base, spinner + SpinnerCount));
        if (index >= filled) index = filled - 1;
        if (index < 0) index = 0;
        Guest::Write16(base, spinner + SpinnerIndex, uint16_t(index));
        ShowChoice(ctx, base, spinner);
    }

    uint32_t Spinner(uint8_t* base, uint32_t menu, int row)
    {
        return Guest::Read32(base, menu + RowSpinners + 4 * row);
    }

    void HelpBar(PPCContext& ctx, uint8_t* base, uint32_t menu)
    {
        SetText(ctx, base, Guest::Read32(base, menu + HelpText),
            g_page == 0 ? "~menu_back BACK      ~updown MOVE      ~r1 CUSTOM RULES"
            : g_page == 1 ? "~menu_back BACK      ~updown MOVE      ~r1 GAME MODES"
                          : "~menu_back BACK      ~updown MOVE      ~r1 ADVANCED OPTIONS");
    }

    int Clamp(int value, int count) { return value < 0 ? 0 : value >= count ? count - 1 : value; }

    void ShowPage2(PPCContext& ctx, uint8_t* base, uint32_t menu)
    {
        g_page = 1;
        SetText(ctx, base, Guest::Read32(base, menu + TitleText), "CUSTOM RULES");
        SetText(ctx, base, Guest::Read32(base, menu + DescriptionText), "SELECT THE CUSTOM RULES FOR THIS MATCH");
        const int values[Rows] = {
            g_health,
            Guest::Read8(base, Settings + FieldRespawnTime),
            Guest::Read8(base, Settings + FieldRoundLimit),
            Guest::Read8(base, Settings + FieldLastManStanding) ? 1 : 0,
            g_gravity,
            g_speed,
        };
        for (int row = 0; row < Rows; row++)
        {
            SetText(ctx, base, Guest::Read32(base, menu + RowLabels + 8 * row), Page2[row].label);
            Fill(ctx, base, Spinner(base, menu, row), Page2[row].choices, Page2[row].count,
                 Clamp(values[row], Page2[row].count));
        }
        HelpBar(ctx, base, menu);
    }

    void ShowPage3(PPCContext& ctx, uint8_t* base, uint32_t menu)
    {
        g_page = 2;
        SetText(ctx, base, Guest::Read32(base, menu + TitleText), "GAME MODES");
        SetText(ctx, base, Guest::Read32(base, menu + DescriptionText),
                "DOMINATION: HOLD THE FIVE FLAGS (TEAM BATTLE, ROUEN). BOTS: ADDED AT THE MATCH START");
        for (int row = 0; row < Rows; row++)
        {
            SetText(ctx, base, Guest::Read32(base, menu + RowLabels + 8 * row), Page3[row].label);
            Fill(ctx, base, Spinner(base, menu, row), Page3[row].choices, Page3[row].count,
                 row == 0 ? Clamp(g_domination, Page3[row].count) : row == 1 ? Clamp(g_botChoice, Page3[row].count) : 0);
        }
        HelpBar(ctx, base, menu);
    }

    // The rules, shared over the side channel (kernel_net.cpp). Every machine
    // in a System Link game runs its own server for its own player, so a
    // joiner must have the host's rules on his own machine, and the title
    // never sends them. The machine whose Custom Rules page set the rules
    // (the host) sends them every second: "C3R1", then health, gravity,
    // speed, respawn time, round limit and last man standing, a byte each.
    // A machine that set none asks ("C3R?") and takes what comes back.
    std::atomic<bool> g_chosen{ false };   // this machine's own rules page set the rules
    std::atomic<uint32_t> g_received{ 0 }; // the host's six bytes, packed, and 0x80000000 when new
    std::atomic<uint32_t> g_receivedRest{ 0 };
    uint8_t g_mine[7] = {};                // what this machine sends, kept by TakePage2 and TakePage3
    std::atomic<bool> g_mineReady{ false };

    void SideThread()
    {
        for (;;)
        {
            // Room for the largest (Domination's state, 89 bytes): a smaller
            // buffer makes Windows drop the datagram (WSAEMSGSIZE).
            uint8_t packet[2048];   // 0.5.275: 2048 (a 30-bot roster is 608 bytes)
            unsigned int from = 0;
            int got;
            while ((got = Net::SideReceive(packet, sizeof(packet), &from)) >= 4)
            {
                if (memcmp(packet, "C3R?", 4) == 0 && g_chosen.load() && g_mineReady.load())
                {
                    uint8_t reply[11] = { 'C', '3', 'R', '1' };
                    memcpy(reply + 4, g_mine, 7);
                    Net::SideSend(reply, sizeof(reply), from);
                }
                else if (got >= 8 && memcmp(packet, "C3DS", 4) == 0)
                    Domination::OnState(packet, got, from);
                else if (got >= 8 && memcmp(packet, "C3BR", 4) == 0)
                    BotsOnRoster(packet, got, from);   // 0.5.194: the host's bots (bots.cpp)
                else if (got >= 8 && memcmp(packet, "C3BS", 4) == 0)
                    BotsOnStates(packet, got, from);   // 0.5.207: their lives (bots.cpp)
                else if (got >= 6 && (memcmp(packet, "C3UP", 4) == 0 || memcmp(packet, "C3UH", 4) == 0 || memcmp(packet, "C3UD", 4) == 0 || memcmp(packet, "C3KS", 4) == 0 || memcmp(packet, "C3KU", 4) == 0 || memcmp(packet, "C3KM", 4) == 0 || memcmp(packet, "C3KB", 4) == 0 || memcmp(packet, "C3KH", 4) == 0 || memcmp(packet, "C3MF", 4) == 0 || memcmp(packet, "C3MP", 4) == 0))
                    MpFixesOnPlanePacket(packet, got, from);   // 0.5.275: the UAV plane on every machine (mp_fixes.cpp)
                else if (got >= 10 && memcmp(packet, "C3R1", 4) == 0 && !g_chosen.load())
                {
                    if (packet[4] >= 5 || packet[5] >= 3 || packet[6] >= 3 || packet[7] >= 3 || packet[8] >= 8) continue;
                    const uint32_t packed = 0x80000000u | uint32_t(packet[4]) | uint32_t(packet[5]) << 4 |
                        uint32_t(packet[6]) << 8 | uint32_t(packet[7]) << 12 | uint32_t(packet[8]) << 16 |
                        uint32_t(packet[9] ? 1 : 0) << 20 | uint32_t(got >= 11 && packet[10] ? 1 : 0) << 21;
                    if ((g_receivedRest.load() & 0x7FFFFFFFu) != (packed & 0x7FFFFFFFu))
                    {
                        g_receivedRest.store(packed);
                        g_received.store(packed);
                        in_addr address{}; address.s_addr = from;
                        printf("lobby rules: the host's rules came from %u.%u.%u.%u\n", from & 255, (from >> 8) & 255,
                               (from >> 16) & 255, from >> 24);
                        fflush(stdout);
                    }
                }
            }
            if (g_chosen.load() && g_mineReady.load())
            {
                uint8_t rules[11] = { 'C', '3', 'R', '1' };
                memcpy(rules + 4, g_mine, 7);
                Net::SideSend(rules, sizeof(rules));
            }
            else if (!g_chosen.load())
                Net::SideSend("C3R?", 4);
            Sleep(1000);
        }
    }

    void StartSide()
    {
        static std::once_flag started;
        std::call_once(started, [] { std::thread(SideThread).detach(); });
    }

    // The host's rules, when new ones came, into this machine's settings.
    bool TakeReceived(uint8_t* base)
    {
        StartSide();
        if (g_chosen.load()) return false;
        const uint32_t packed = g_received.exchange(0);
        if (!(packed & 0x80000000u)) return false;
        g_health = packed & 15;
        g_gravity = (packed >> 4) & 15;
        g_speed = (packed >> 8) & 15;
        for (uint32_t block : { Settings, Settings + 0xB0 })
        {
            Guest::Write8(base, block + FieldRespawnTime, uint8_t((packed >> 12) & 15));
            Guest::Write8(base, block + FieldRoundLimit, uint8_t((packed >> 16) & 15));
            Guest::Write8(base, block + FieldLastManStanding, uint8_t((packed >> 20) & 1));
        }
        g_domination = (packed >> 21) & 1;
        printf("lobby rules: playing by the host's: health %d%%, gravity %d, speed %d, respawn %u, round limit %u, last man standing %u\n",
               int(HealthScale[g_health] * 100), g_gravity, g_speed, (packed >> 12) & 15, (packed >> 16) & 15, (packed >> 20) & 1);
        printf("lobby rules: the host's domination %s\n", g_domination ? "on" : "off");
        fflush(stdout);
        return true;
    }

    // The choices on the second page, kept: three where the title keeps
    // its own, the rest here.
    void TakePage2(uint8_t* base, uint32_t menu)
    {
        int index[Rows];
        for (int row = 0; row < Rows; row++)
        {
            const uint32_t spinner = Spinner(base, menu, row);
            index[row] = spinner ? Clamp(int16_t(Guest::Read16(base, spinner + SpinnerIndex)), Page2[row].count) : 0;
        }
        g_health = index[0];
        Guest::Write8(base, Settings + FieldRespawnTime, uint8_t(index[1]));
        Guest::Write8(base, Settings + FieldRoundLimit, uint8_t(index[2]));
        Guest::Write8(base, Settings + FieldLastManStanding, uint8_t(index[3]));
        g_gravity = index[4];
        g_speed = index[5];
        g_mine[0] = uint8_t(g_health); g_mine[1] = uint8_t(g_gravity); g_mine[2] = uint8_t(g_speed);
        g_mine[3] = uint8_t(index[1]); g_mine[4] = uint8_t(index[2]); g_mine[5] = uint8_t(index[3]);
        g_mine[6] = uint8_t(g_domination);
        g_mineReady = true;
        g_chosen = true;
        StartSide();
        printf("lobby rules: health %d%%, respawn %d, round limit %d, last man standing %d, gravity %d, speed %d\n",
            int(HealthScale[g_health] * 100), index[1], index[2], index[3], g_gravity, g_speed);
        fflush(stdout);
    }

    void TakePage3(uint8_t* base, uint32_t menu)
    {
        const uint32_t spinner = Spinner(base, menu, 0);
        g_domination = spinner ? Clamp(int16_t(Guest::Read16(base, spinner + SpinnerIndex)), Page3[0].count) : 0;
        // 0.5.277: the bots' row: this machine hosts, its server adds them.
        {
            const uint32_t bots = Spinner(base, menu, 1);
            g_botChoice = bots ? Clamp(int16_t(Guest::Read16(base, bots + SpinnerIndex)), Page3[1].count) : 0;
            BotsAsk(BotCounts[g_botChoice]);
            printf("lobby rules: bots %d (%s)\n", BotCounts[g_botChoice], Page3[1].choices[g_botChoice]);
        }
        g_mine[0] = uint8_t(g_health); g_mine[1] = uint8_t(g_gravity); g_mine[2] = uint8_t(g_speed);
        g_mine[3] = Guest::Read8(base, Settings + FieldRespawnTime); g_mine[4] = Guest::Read8(base, Settings + FieldRoundLimit);
        g_mine[5] = Guest::Read8(base, Settings + FieldLastManStanding); g_mine[6] = uint8_t(g_domination);
        g_mineReady = true;
        g_chosen = true;
        StartSide();
        printf("lobby rules: domination %s\n", g_domination ? "on" : "off");
        fflush(stdout);
    }

    // Whichever of the extra pages is up, its choices kept.
    void TakeCurrent(uint8_t* base, uint32_t menu)
    {
        if (g_page == 1) TakePage2(base, menu);
        else if (g_page == 2) TakePage3(base, menu);
    }

    // The first page's words and choices back as the title made them; the
    // title's setup then puts the choices from the settings block back.
    void RestorePage1(PPCContext& ctx, uint8_t* base, uint32_t menu)
    {
        g_page = 0;
        SetText(ctx, base, Guest::Read32(base, menu + TitleText), "MPFRONTEND_CREATEGAME_ADVANCED_OPTIONS");
        SetText(ctx, base, Guest::Read32(base, menu + DescriptionText), "MPFRONTEND_ADV_GAME_CREATE");
        static const char* const offOn[] = { "MPFRONTEND_OFF", "MPFRONTEND_ON" };
        for (int row = 0; row < Rows; row++)
        {
            SetText(ctx, base, Guest::Read32(base, menu + RowLabels + 8 * row), Page1Labels[row]);
            if (row == 0) Fill(ctx, base, Spinner(base, menu, row), TimeLimits, 5, 0);
            else if (row >= 2) Fill(ctx, base, Spinner(base, menu, row), offOn, 2, 0);
            // The score limit's choices depend on the game mode: the title's
            // setup makes them again.
        }
    }

    // The default of a variable, as the title registered it.
    float Default(PPCContext& ctx, uint8_t* base, uint32_t name)
    {
        const uint32_t cvar = Call(ctx, base, CvarFind, name);
        if (cvar == 0) return 0.0f;
        const uint32_t text = Guest::Read32(base, cvar + 8);
        if (text == 0) return 0.0f;
        return float(atof(reinterpret_cast<const char*>(base + text)));
    }

    void Apply(PPCContext& ctx, uint8_t* base, int which, uint32_t name, float scale)
    {
        if (scale == 1.0f)
        {
            if (!g_touched[which]) return;
            g_touched[which] = false;
            Call(ctx, base, CvarSet, name, 0, 1);
            printf("lobby rules: %s back to its default\n", reinterpret_cast<const char*>(base + name));
            fflush(stdout);
            return;
        }
        const float usual = Default(ctx, base, name);
        if (!(usual > 0.0f)) return;
        char value[32];
        snprintf(value, sizeof(value), "%g", double(usual * scale));
        Call(ctx, base, CvarSet, name, GuestString(base, value), 1);
        g_touched[which] = true;
        printf("lobby rules: %s %s (default %g)\n", reinterpret_cast<const char*>(base + name), value, double(usual));
        fflush(stdout);
    }

    bool OnAdvancedMenu(uint8_t* base, uint32_t menu)
    {
        return menu != 0 && Guest::Read32(base, menu) == AdvancedMenuVtable;
    }
}

// The class screen (its virtual table 0x82084644; its load sub_823A1C48 finds
// its parts by name in its panel, +72): the custom class's main weapon is
// chosen there with the shoulder buttons, and shown in the line under the
// title (cs_text_title_description).
namespace
{
    constexpr uint32_t ClassMenuVtable = 0x82084644;
    constexpr uint32_t FindElement = 0x824EE230;   // (panel, name) -> element
    const char* const WeaponNames[] = { "THE CLASS'S OWN", "M1 GARAND", "THOMPSON", "BAR", "SPRINGFIELD (SCOPED)",
                                        ".30 CAL MACHINE GUN", "BAZOOKA", "TRENCH GUN", "KAR98K", "MP40", "STG 44",
                                        "KAR98K (SCOPED)", "MG34", "PANZERSCHRECK", "LEE-ENFIELD", "BREN", "STEN", "FG42", "FG42 (SCOPED)" };
    constexpr int WeaponCount = int(sizeof(WeaponNames) / sizeof(WeaponNames[0]));

    void ShowCustomWeapon(PPCContext& ctx, uint8_t* base, uint32_t menu)
    {
        const uint32_t panel = Guest::Read32(base, menu + 72);
        if (!panel) return;
        const uint32_t element = Call(ctx, base, FindElement, panel, GuestString(base, "cs_text_title_description"));
        const Settings::Values values = Settings::Get();
        char text[128];
        const int slot = values.activeClass - 1;
        if (slot < 0 || slot >= Settings::Values::CustomClasses)
            snprintf(text, sizeof(text), "CUSTOM CLASS: NONE   (~r1 CREATE A CLASS)");
        else
        {
            const int primary = values.classPrimary[slot] >= 0 && values.classPrimary[slot] < WeaponCount ? values.classPrimary[slot] : 0;
            const int sidearm = values.classSidearm[slot] >= 0 && values.classSidearm[slot] <= 19 ? values.classSidearm[slot] : 0;
            // The second weapon: a pistol, or (3 on) a main weapon in its place.
            std::string second = sidearm == 1 ? " + COLT" : sidearm == 2 ? " + P38" : "";
            if (sidearm >= 3 && sidearm - 2 < WeaponCount) second = std::string(" + ") + WeaponNames[sidearm - 2];
            snprintf(text, sizeof(text), "CUSTOM CLASS: %s - %s%s   (~r1 EDIT)", values.className[slot],
                     primary ? WeaponNames[primary] : "CLASS WEAPON", second.c_str());
        }
        SetText(ctx, base, element, text);
    }
}

// The class screen's load: then the custom weapon's line.
PPC_FUNC(sub_823A1C48)
{
    const uint32_t menu = ctx.r3.u32;
    __imp__sub_823A1C48(ctx, base);
    PPCContext call = ctx;
    ShowCustomWeapon(call, base, menu);
}

// The menu system's input: one button at a time, to the menu in front. On
// the Advanced Options screen the shoulder buttons (which it has no use for)
// turn the page.
PPC_FUNC(sub_824D5A08)
{
    const uint32_t system = ctx.r3.u32, button = ctx.r4.u32;
    const int32_t current = int32_t(Guest::Read32(base, system + 28));
    if (current >= 0)
    {
        const uint32_t menu = Guest::Read32(base, Guest::Read32(base, system + 4) + 4 * current);
        // The custom class editor, while open, has the buttons.
        if (ClassEditor::IsOpen()) return;
        if (menu && Guest::Read32(base, menu) == ClassMenuVtable)
        {
            ShowCustomWeapon(ctx, base, menu);   // as the editor left it
            if (button >= 256 && button <= 8192)
            {
                printf("lobby rules: class screen button %u: the editor\n", button);
                fflush(stdout);
                ClassEditor::Open();
                return;
            }
        }
        if (OnAdvancedMenu(base, menu))
        {
            static int said = 0;
            if (said < 40) { said++; printf("lobby rules: button %u on the advanced options\n", button); fflush(stdout); }
            if (button >= 256 && button <= 8192)
            {
                if (g_page == 0)
                {
                    PPCContext call = ctx;
                    call.r3.u32 = menu;
                    __imp__sub_82394628(call, base);   // the first page into the settings block
                    ShowPage2(ctx, base, menu);
                }
                else if (g_page == 1)
                {
                    TakePage2(base, menu);
                    ShowPage3(ctx, base, menu);
                }
                else
                {
                    TakePage3(base, menu);
                    RestorePage1(ctx, base, menu);
                    PPCContext call = ctx;
                    call.r3.u32 = menu;
                    __imp__sub_823A8F70(call, base);   // the title's setup: choices from the block
                    HelpBar(ctx, base, menu);
                }
                return;
            }
        }
    }
    __imp__sub_824D5A08(ctx, base);
}

// FEMenu's spinner maker: room for eight choices in the Advanced Options
// spinners, the most the second page has.
PPC_FUNC(sub_824E9540)
{
    if (OnAdvancedMenu(base, ctx.r3.u32) && ctx.r5.s32 < 8) ctx.r5.s64 = 8;
    __imp__sub_824E9540(ctx, base);
}

// The screen's setup, each time it is shown: always from the first page.
PPC_FUNC(sub_823A8F70)
{
    const uint32_t menu = ctx.r3.u32;
    if (g_page != 0)
    {
        TakeCurrent(base, menu);
        RestorePage1(ctx, base, menu);
    }
    __imp__sub_823A8F70(ctx, base);
    HelpBar(ctx, base, menu);
}

// The screen's choices into the settings block. On the second page the first
// page's are there already.
PPC_FUNC(sub_82394628)
{
    if (g_page != 0)
    {
        TakeCurrent(base, ctx.r3.u32);
        return;
    }
    __imp__sub_82394628(ctx, base);
}

// Back out of the screen: from the second page, its choices kept and the
// first page back before the title keeps the first page's.
PPC_FUNC(sub_823945C0)
{
    const uint32_t menu = ctx.r3.u32;
    if (g_page != 0)
    {
        TakeCurrent(base, menu);
        RestorePage1(ctx, base, menu);
        PPCContext call = ctx;
        __imp__sub_823A8F70(call, base);
    }
    __imp__sub_823945C0(ctx, base);
}

// The session's settings into variables, as the match starts: the custom
// rules' with them (on a joiner, the host's when they came).
uint32_t g_settingsChoice = 1;   // its argument, for doing it again mid-match
PPC_FUNC(sub_823F4078)
{
    TakeReceived(base);
    g_settingsChoice = ctx.r3.u32;
    __imp__sub_823F4078(ctx, base);
    PPCContext call = ctx;
    Apply(call, base, 0, NameMaxHealth, HealthScale[g_health]);
    Apply(call, base, 1, NameGravity, GravityScale[g_gravity]);
    Apply(call, base, 2, NameSpeed, SpeedScale[g_speed]);
}

// The level's setup, as each map loads (after the settings above): it sets
// g_gravity to the map's own value, over the rules' one. The rules again.
PPC_FUNC(sub_824799D8)
{
    __imp__sub_824799D8(ctx, base);
    PPCContext call = ctx;
    Apply(call, base, 0, NameMaxHealth, HealthScale[g_health]);
    Apply(call, base, 1, NameGravity, GravityScale[g_gravity]);
    Apply(call, base, 2, NameSpeed, SpeedScale[g_speed]);
}

// Each server frame (bots.cpp's hook): on a joiner, the host's rules as soon
// as they come, even mid-match.
void LobbyRulesTick(PPCContext& ctx, uint8_t* base)
{
    if (!TakeReceived(base)) return;
    PPCContext inner = ctx;
    inner.r1.u32 = (ctx.r1.u32 - 0x2000) & ~0xFu;
    inner.r3.u64 = g_settingsChoice;
    __imp__sub_823F4078(inner, base);
    Apply(inner, base, 0, NameMaxHealth, HealthScale[g_health]);
    Apply(inner, base, 1, NameGravity, GravityScale[g_gravity]);
    Apply(inner, base, 2, NameSpeed, SpeedScale[g_speed]);
}

// For dom.cpp: Domination asked for (this machine's own choice, or the
// host's that came), and whether this machine is the one that chose (the
// host, who keeps the flags and the score for everyone).
bool LobbyDomination() { return g_domination != 0; }
bool LobbyIsHost() { return g_chosen.load(); }

// 0.5.461: THE LOBBY'S MAX PLAYERS TO 32 (Justin: "in game settings it only
// goes up to 24 ... can we raise the in game menu to 32 now?"), with the F11
// "32 players" setting on. The count is the settings block's byte +91 (458's
// log: 12 and 24 in Justin's two matches; 460's: 8 and 24). 459 hooked
// another screen's (sub_823AA900; 460: never called on Justin's). The System
// Link create screen's own setup is sub_823AA120: its max players list (the
// menu's +276) is built from a table of six counts (0x82061990), each made
// text and added with sub_824E8E18, and the counts kept in the menu (+156,
// room for 30) for taking the choice back. Now 28 and 32 are added the same
// way after it, their counts in the menu's table after the six.
// 0.5.462: 461's two adds were refused: the list is made with room for six
// (FEMenuListBox's constructor sub_824E8FC8: r5, kept at +30, its choices'
// array allocated for that many) and every add stops there. While the create
// screen's setup runs, a list made with room for six gets room for eight.
static bool g_inCreateSetup = false;
extern "C" PPC_FUNC(__imp__sub_824E8FC8);
PPC_FUNC(sub_824E8FC8)
{
    if (g_inCreateSetup && ctx.r5.u32 == 6 && ::Settings::Get().maxPlayers >= 32) ctx.r5.u64 = 8;
    __imp__sub_824E8FC8(ctx, base);
}
extern "C" PPC_FUNC(__imp__sub_823AA120);
PPC_FUNC(sub_823AA120)
{
    const uint32_t menu = ctx.r3.u32;
    g_inCreateSetup = true;
    __imp__sub_823AA120(ctx, base);
    g_inCreateSetup = false;
    const uint32_t list = Guest::Read32(base, menu + 276);
    const bool ok = list >= 0x40000000u && list < 0xA0000000u;
    const int choices = ok ? int16_t(Guest::Read16(base, list + SpinnerCount)) : -1;
    {
        static int told = 0;
        if (told++ < 20)
        {
            printf("lobby: create screen setup: menu %08X list %08X choices %d (counts %u %u %u %u %u %u) 32 players %s\n", menu, list, choices,
                   Guest::Read32(base, menu + 156), Guest::Read32(base, menu + 160), Guest::Read32(base, menu + 164), Guest::Read32(base, menu + 168),
                   Guest::Read32(base, menu + 172), Guest::Read32(base, menu + 176), ::Settings::Get().maxPlayers >= 32 ? "on" : "off");
            fflush(stdout);
        }
    }
    if (!ok || ::Settings::Get().maxPlayers < 32 || choices != 6 || Guest::Read32(base, menu + 176) != 24) return;
    const uint32_t slot = (ctx.r1.u32 - 0x800) & ~0xFu;
    static const char* const more[2] = { "28", "32" };
    for (int k = 0; k < 2; k++)
    {
        Call(ctx, base, MakeString, slot, GuestString(base, more[k]));
        Call(ctx, base, 0x824E8E18, list, slot);
        Guest::Write32(base, menu + 156 + 4 * uint32_t(6 + k), k ? 32u : 28u);
    }
    printf("lobby: max players list to 32 (%d choices)\n", int16_t(Guest::Read16(base, list + SpinnerCount)));
    fflush(stdout);
}
#endif
