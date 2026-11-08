// Co-op v40: the lobby, World at War's way (Team Friend, Claude session,
// 2026-10-29). F2 (the campaign, outside a mission) opens it:
// - "Host a game": this copy hosts (UDP 28970). The game is announced each
//   second where a System Link search goes (each network's broadcast address
//   and the settings' System Link peers: a friend's Tailscale address), to
//   this PC (UDP 28972) and to any address invited from the lobby.
// - "Find games": the games heard are listed; Join (or an address typed in)
//   makes this copy the joining one (UDP 28971 here).
// - The lobby: both players with their ready marks and pings; the host picks
//   the mission, the difficulty and the scoring; the joining player marks
//   himself ready; the host starts: 5 s counted down on both, then both go
//   into the mission together (the joining copy follows the host's).
// - Leaving says goodbye; the host's leaving closes the lobby for both. The
//   host back at the main menu after a mission: the lobby is open again, and
//   the joining copy leaves the mission too.
// COD3_COOP=host|join (the launchers, the tests) starts in the lobby already;
// COD3_COOP_MAP starts the host's mission by itself once the friend is in.
#ifndef COD3_MP

#include "coop.h"
void CoopRelaunchFresh();   // kernel_xam.cpp (v45b)
bool CoopSwapRelaunch();    // coop_swap.cpp (v46)
#include "kernel.h"
#include "net.h"
#include "overlay.h"
#include "settings.h"
#include "input.h"
#include "window.h"

#include "../thirdparty/imgui/imgui.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace Coop
{
    namespace
    {
        struct Mission { const char* map; const char* title; };
        // the campaign's chapters, in order
        const Mission Missions[] = {
            { "saint_lo", "Saint-Lo" }, { "island", "The Island" }, { "nightd", "Night Drop" }, { "mayenne", "Mayenne Bridge" },
            { "falaise", "Falaise Road" }, { "fuelplnt", "Fuel Plant" }, { "blkbrn", "The Black Baron" }, { "forest", "The Forest" },
            { "laison", "Laison River" }, { "crssrds", "The Crossroads" }, { "hostage", "Hostage" }, { "stbert", "The Corridor of Death" },
            { "mace2", "The Mace" }, { "chambois", "Chambois" } };
        constexpr int MissionTotal = int(sizeof Missions / sizeof Missions[0]);
        // the game's three: SV_SpawnServer reads the value of the dvar at 0x82A8E610 (+32) and sets
        // g_gameskill and the player's health (1200 / 600 / 300) from it
        const char* const Difficulties[] = { "Easy", "Normal", "Hard" };
        constexpr uint32_t SkillDvar = 0x82A8E610;
        int MissionIndex(const std::string& map) { for (int i = 0; i < MissionTotal; i++) if (map == Missions[i].map) return i; return -1; }
        std::string MissionTitle(const std::string& map) { const int i = MissionIndex(map); return i >= 0 ? Missions[i].title : map; }

        // ---- the messages ----
        enum LobbyState : uint8_t { L_OPEN = 0, L_COUNTDOWN = 1, L_STARTED = 2, L_CLOSED = 3 };
        struct Hello { uint32_t version; uint8_t inMission, ready, leaving, pad; char map[64]; char name[32]; double sentAt; float rtt; uint32_t pad2; };
        struct Beacon
        {
            uint32_t version, count; uint8_t inMission, state, difficulty, scoring;
            char map[64]; char name[32]; char pick[32];
            float countdown; uint8_t friendReady, pad[3]; double echo;
        };
        struct Announce { char magic[4]; uint32_t version, session; uint16_t gamePort; uint8_t state, players, inMission, difficulty, pad[2]; char name[32]; char pick[32]; };

        // ---- both sides ----
        std::string g_peerName;
        bool g_connectedSaid = false;
        std::mutex g_lock;            // the view and the actions (the window's thread draws the lobby)
        // ---- the host's lobby ----
        int g_pick = 7;               // The Forest
        int g_difficulty = -1;        // -1: the game's own (whatever the menu set)
        bool g_scoring = true;
        uint8_t g_state = L_OPEN;
        double g_startAt = -1, g_startedAt = -1;
        bool g_friendReady = false;
        float g_friendRtt = -1;
        double g_helloSentAt = 0, g_helloHeardAt = -1;
        uint32_t g_lobbyId = 0;
        std::vector<std::string> g_invites;
        // ---- the joining copy ----
        std::string g_hostMap; uint32_t g_hostCount = 0; double g_followAt = -100;
        double g_swapAt = -1;   // (v46) the checkpoint reload's swap began (the loading screen)
        bool g_ready = false;
        float g_rtt = -1;
        Beacon g_beacon{}; bool g_haveBeacon = false; double g_beaconAt = -1;
        double g_joinedAt = -1; std::string g_joinAddress;
        bool g_leaveNow = false;      // the host closed the lobby (handled next frame, not inside the network's read)
        std::string g_status; double g_statusAt = -100;
        void Status(const std::string& s) { g_status = s; g_statusAt = Now(); printf("coop: lobby: %s\n", s.c_str()); fflush(stdout); }

        int Difficulty(const uint8_t* base)
        {
            if (g_difficulty >= 0) return g_difficulty;
            const int32_t v = int32_t(Guest::Read32(base, SkillDvar + 32));
            return v >= 0 && v <= 2 ? v : 1;
        }
        void SetDifficulty(uint8_t* base, int d)
        {
            d = std::clamp(d, 0, 2);
            const uint32_t name = Guest::Read32(base, SkillDvar);
            Guest::Write32(base, SkillDvar + 32, uint32_t(d));
            printf("coop: lobby: difficulty %s (%s = %d)\n", Difficulties[d], Text(base, name, 40).c_str(), d); fflush(stdout);
        }
    }

    std::string MyName()
    {
        static std::string windowsName;
        const Settings::Values v = Settings::Get();
        std::string n(v.playerName, strnlen(v.playerName, sizeof v.playerName));
        if (n.empty())
        {
            if (windowsName.empty()) { char u[64] = {}; DWORD len = sizeof u; windowsName = GetUserNameA(u, &len) ? u : "Player"; }
            n = windowsName;
        }
        return n.substr(0, 31);
    }
    std::string PeerName() { return g_peerName; }
    bool ScoringOn() { return IsHost() ? g_scoring : (!g_haveBeacon || g_beacon.scoring != 0); }
    void FollowMap(const std::string& name, uint32_t count) { g_hostMap = name; g_hostCount = count; g_followAt = -100; }
    // (v45) World at War's checkpoint restart: every client loads the level again with the server. The joining
    // copy's frozen world cannot follow the host's back to a checkpoint (his things come back under other
    // numbers, ours were hidden, and every one of his became a puppet: hundreds, and the game crashed); so it
    // loads its mission fresh and freezes again, beside the host, as when it first came in
    void ReloadMission()
    {
        const std::string map = MyMap();
        if (map.empty()) return;
        g_followAt = Now();
        printf("coop: lobby: the host went back to a checkpoint: this copy starts again and follows him into %s\n", map.c_str()); fflush(stdout);
        Notice("Co-op: back to the checkpoint...", 4000);
        // (v45b) as a new process: loaded again in this one ("spmap"), the script VM still had the stopped
        // world's threads and spun for ever on them (v45a). A fresh start has none, as at the first join.
        // (v46) behind this window (coop_swap.cpp): this copy goes quiet on the network and shows the loading
        // screen while the new one loads hidden; the old way if it cannot be started
        NetSilence();
        g_swapAt = Now();
        if (::CoopSwapRelaunch()) return;
        SetEnvironmentVariableA("COD3_COOP_RELOAD", "1"); _putenv_s("COD3_COOP_RELOAD", "1");
        ::CoopRelaunchFresh();
    }

    // ---- hello and beacon (co-op on) ------------------------------------------
    void Lobby(PPCContext& ctx, uint8_t* base)
    {
        (void)ctx;
        const double now = Now();
        const bool in = InMission(base);
        static double sayAt = 0;
        if (now - sayAt >= (in ? 1.0 : 0.25))
        {
            sayAt = now;
            if (IsJoin())
            {
                Hello h{}; h.version = ProtocolVersion; h.inMission = in ? 1 : 0; h.ready = g_ready ? 1 : 0; h.sentAt = now; h.rtt = g_rtt;
                strncpy(h.map, MyMap().c_str(), 63);
                strncpy(h.name, MyName().c_str(), 31);
                SendUnreliable(K_HELLO, &h, sizeof h);
            }
            else if (PeerConnected())
            {
                Beacon b{}; b.version = ProtocolVersion; b.count = MissionCount(); b.inMission = in ? 1 : 0;
                b.state = g_state; b.difficulty = uint8_t(Difficulty(base)); b.scoring = g_scoring ? 1 : 0;
                strncpy(b.map, MyMap().c_str(), 63);
                strncpy(b.name, MyName().c_str(), 31);
                strncpy(b.pick, Missions[g_pick].map, 31);
                b.countdown = g_state == L_COUNTDOWN ? float(std::max(0.0, g_startAt - now)) : 0.0f;
                b.friendReady = g_friendReady ? 1 : 0;
                b.echo = g_helloHeardAt >= 0 ? g_helloSentAt + (now - g_helloHeardAt) : 0.0;
                SendUnreliable(K_BEACON, &b, sizeof b);
            }
        }
        if (IsHost())
        {
            static bool said = false;
            if (PeerConnected() && !said)
            {
                said = true;
                printf("coop: lobby: the joining player is connected\n"); fflush(stdout);
                Notice("Co-op: " + (g_peerName.empty() ? std::string("your friend") : g_peerName) + " joined the lobby.", 5000);
            }
            if (said && (PeerSeenAt() < 0 || now - PeerSeenAt() > (in ? 30.0 : 10.0)))
            {
                said = false; g_friendReady = false;
                printf("coop: lobby: the joining player is gone\n"); fflush(stdout);
                Notice("Co-op: your friend has left.", 6000);
                if (g_state == L_COUNTDOWN) { g_state = L_OPEN; Status("The start was called off: your friend left."); }
            }
            // the countdown, then the mission for both
            if (g_state == L_COUNTDOWN && now >= g_startAt)
            {
                g_state = L_STARTED; g_startedAt = now;
                Overlay::SetLobbyOpen(false);
                SetDifficulty(base, Difficulty(base));
                printf("coop: lobby: starting %s\n", Missions[g_pick].map); fflush(stdout);
                Kernel::QueueConsoleCommand(std::string("spmap ") + Missions[g_pick].map);
            }
            // (v45u) the mission asked for and not begun (the menu took no notice of it once, Island, L20):
            // asked again every 15 s, three times
            static bool inSinceStart = false; static int asked = 0; static double askedAt = -1;
            if (g_state != L_STARTED) { inSinceStart = false; asked = 0; askedAt = -1; }
            if (in) inSinceStart = true;
            if (g_state == L_STARTED && !in && !inSinceStart && asked < 3 && now - std::max(g_startedAt, askedAt) > 15)
            {
                asked++; askedAt = now;
                printf("coop: lobby: %s did not begin: asked again\n", Missions[g_pick].map); fflush(stdout);
                Kernel::QueueConsoleCommand(std::string("spmap ") + Missions[g_pick].map);
            }
            // back at the main menu after a mission: the lobby is open again
            if (g_state == L_STARTED && !in && (inSinceStart || now - g_startedAt > 60) && now - g_startedAt > 10 && Overlay::MainMenuShown()) { g_state = L_OPEN; Status("Back in the lobby."); }
            if (in && g_state != L_STARTED) { g_state = L_STARTED; g_startedAt = now; }
            // COD3_COOP_MAP (the tests): the host's mission by itself once the friend is in
            static const char* const want = getenv("COD3_LAUNCHDATA") ? nullptr : getenv("COD3_COOP_MAP");
            static double connectedAt = -1;
            static bool started = false;
            if (PeerConnected() && connectedAt < 0) connectedAt = now;
            if (want && want[0] && !started && MyMap().empty() && connectedAt >= 0 && now - connectedAt > 3)
            {
                started = true;
                printf("coop: lobby: starting %s (COD3_COOP_MAP)\n", want); fflush(stdout);
                Notice(std::string("Co-op: starting ") + want + "...", 5000);
                Kernel::QueueConsoleCommand(std::string("spmap ") + want);
            }
        }
        else
        {
            // follow the host into his mission (and again after 20 s if it did not take)
            if (!g_hostMap.empty() && g_hostMap != MyMap() && now - g_followAt > 20)
            {
                g_followAt = now;
                Overlay::SetLobbyOpen(false);
                if (g_haveBeacon) SetDifficulty(base, g_beacon.difficulty);
                printf("coop: lobby: the host is in %s: going there\n", g_hostMap.c_str()); fflush(stdout);
                Notice("Co-op: joining the host's mission (" + MissionTitle(g_hostMap) + ")...", 5000);
                Kernel::QueueConsoleCommand("spmap " + g_hostMap);
            }
            // the host back at the main menu: this copy leaves the mission too
            static double hostOutSince = -1;
            const bool hostOut = g_haveBeacon && now - g_beaconAt < 3 && !g_beacon.inMission && g_beacon.state == L_OPEN;
            if (!(in && hostOut)) hostOutSince = -1;
            else if (hostOutSince < 0) hostOutSince = now;
            else if (now - hostOutSince > 3)
            {
                hostOutSince = now + 30;   // (once)
                g_hostMap.clear();
                printf("coop: lobby: the host left the mission: back to the main menu\n"); fflush(stdout);
                Notice("Co-op: the host left the mission. Back to the lobby.", 6000);
                Kernel::QueueConsoleCommand("disconnect");
            }
        }
    }

    // the lobby's messages, before the sides' own
    bool LobbyReceive(uint8_t* base, Kind kind, const uint8_t* data, size_t size)
    {
        (void)base;
        if (kind == K_HELLO && IsHost())
        {
            static bool toldVersion = false;
            uint32_t version = 0; if (size >= 4) memcpy(&version, data, 4);
            if ((version != ProtocolVersion || size < sizeof(Hello)) && !toldVersion)
            {
                toldVersion = true;
                printf("coop: lobby: the joining copy speaks co-op %u, this one %u: both PCs need the same CoD3.exe\n", version, ProtocolVersion); fflush(stdout);
                Notice("Co-op: your friend's game is a different build. Both need the same CoD3.exe.", 10000);
            }
            if (size < sizeof(Hello)) return true;
            Hello h; memcpy(&h, data, sizeof h);
            h.name[31] = 0;
            if (g_peerName != h.name) { g_peerName = h.name; printf("coop: lobby: the joining player is \"%s\"\n", g_peerName.c_str()); fflush(stdout); }
            if (h.leaving)
            {
                printf("coop: lobby: the joining player left the lobby\n"); fflush(stdout);
                NetForgetPeer();
                g_friendReady = false;
                return true;
            }
            if (bool(h.ready) != g_friendReady) { g_friendReady = h.ready != 0; printf("coop: lobby: the joining player is %s\n", g_friendReady ? "ready" : "not ready"); fflush(stdout); }
            g_friendRtt = h.rtt; g_helloSentAt = h.sentAt; g_helloHeardAt = Now();
            return true;
        }
        if (kind == K_BEACON && IsJoin())
        {
            static bool toldVersion = false;
            uint32_t version = 0; if (size >= 4) memcpy(&version, data, 4);
            if ((version != ProtocolVersion || size < sizeof(Beacon)) && !toldVersion)
            {
                toldVersion = true;
                printf("coop: lobby: the host speaks co-op %u, this one %u: both PCs need the same CoD3.exe\n", version, ProtocolVersion); fflush(stdout);
                Notice("Co-op: the host's game is a different build. Both need the same CoD3.exe.", 10000);
            }
            if (size < sizeof(Beacon)) return true;
            Beacon b; memcpy(&b, data, sizeof b);
            b.map[63] = 0; b.name[31] = 0; b.pick[31] = 0;
            if (!g_connectedSaid) { g_connectedSaid = true; printf("coop: lobby: connected to the host\n"); fflush(stdout); Notice("Co-op: in " + std::string(b.name) + "'s lobby.", 5000); }
            if (g_peerName != b.name) { g_peerName = b.name; printf("coop: lobby: the host is \"%s\"\n", g_peerName.c_str()); fflush(stdout); }
            if (b.state == L_CLOSED) { g_leaveNow = true; return true; }
            if (b.echo > 0) { const float rtt = float(Now() - b.echo); if (rtt >= 0 && rtt < 5) g_rtt = g_rtt < 0 ? rtt : g_rtt + (rtt - g_rtt) * 0.2f; }
            if (b.inMission && b.map[0] && g_hostMap != b.map) { g_hostMap = b.map; g_followAt = -100; }
            g_hostCount = b.count;
            g_beacon = b; g_haveBeacon = true; g_beaconAt = Now();
            return true;
        }
        return false;
    }
}

// ---- finding games, the window's actions, the view ----------------------------
namespace Coop
{
    namespace
    {
        struct Game { uint32_t session = 0; std::string name, pick, address; uint8_t state = 0, players = 1, inMission = 0, difficulty = 1; double seen = 0; };
        enum ActKind { A_HOST, A_JOIN, A_LEAVE, A_PICK, A_DIFF, A_SCORING, A_READY, A_START, A_CANCEL, A_INVITE };
        struct Action { ActKind kind; int value; std::string text; };
        struct View
        {
            Mode mode = Mode::Off;
            bool inMission = false, connected = false, haveBeacon = false, ready = false, friendReady = false, scoring = true;
            std::string myName, peerName, joinAddress, status, hostMap, addresses;
            double statusAt = -100, joinedAt = -1, now = 0;
            int pick = 7, difficulty = 1;
            uint8_t state = L_OPEN;
            float countdown = 0, rtt = -1, friendRtt = -1;
            std::vector<Game> games;
            bool swapping = false; double swapAt = -1; std::string myMap;
        };
        View g_view;
        std::vector<Action> g_actions;
        std::atomic<double> g_windowAt{ -100 };   // the window's thread: when the lobby was last drawn open
        std::vector<Game> g_games;
        SOCKET g_find = INVALID_SOCKET, g_tell = INVALID_SOCKET;

        void Act(ActKind kind, int value = 0, const std::string& text = std::string()) { std::lock_guard<std::mutex> lock(g_lock); g_actions.push_back({ kind, value, text }); }

        // the search's socket (UDP 28972, shared by every copy on this PC), open while the window is
        SOCKET FindSocket(bool want)
        {
            if (!want) { if (g_find != INVALID_SOCKET) { closesocket(g_find); g_find = INVALID_SOCKET; } return INVALID_SOCKET; }
            if (g_find != INVALID_SOCKET) return g_find;
            SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s == INVALID_SOCKET) return s;
            BOOL yes = TRUE;
            setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof yes);
            setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof yes);
            u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
            sockaddr_in me{}; me.sin_family = AF_INET; me.sin_addr.s_addr = htonl(INADDR_ANY); me.sin_port = htons(uint16_t(NetPort() + 2));
            if (bind(s, reinterpret_cast<sockaddr*>(&me), sizeof me) != 0) { closesocket(s); return INVALID_SOCKET; }
            g_find = s;
            return s;
        }
        void Heard(const uint8_t* p, int got, uint32_t from)
        {
            if (got < int(sizeof(Announce)) || memcmp(p, "CO4L", 4) != 0) return;
            Announce a; memcpy(&a, p, sizeof a);
            a.name[31] = 0; a.pick[31] = 0;
            if (a.version != ProtocolVersion) return;
            char text[64]; in_addr ia{}; ia.s_addr = from; inet_ntop(AF_INET, &ia, text, sizeof text);
            for (Game& g : g_games)
                if (g.session == a.session)
                {
                    // (one game heard two ways: the network's own address beats this PC's)
                    if (g.address == "127.0.0.1" && strcmp(text, "127.0.0.1") != 0) g.address = text;
                    g.name = a.name; g.pick = a.pick; g.state = a.state; g.players = a.players; g.inMission = a.inMission; g.difficulty = a.difficulty; g.seen = Now();
                    return;
                }
            Game g; g.session = a.session; g.name = a.name; g.pick = a.pick; g.address = text; g.state = a.state; g.players = a.players; g.inMission = a.inMission; g.difficulty = a.difficulty; g.seen = Now();
            g_games.push_back(g);
            printf("coop: lobby: found %s's game at %s\n", g.name.c_str(), text); fflush(stdout);
        }
        void Search()
        {
            uint8_t p[512];
            if (const SOCKET s = FindSocket(true); s != INVALID_SOCKET)
                for (int n = 0; n < 64; n++)
                {
                    sockaddr_in from{}; int len = sizeof from;
                    const int got = recvfrom(s, reinterpret_cast<char*>(p), sizeof p, 0, reinterpret_cast<sockaddr*>(&from), &len);
                    if (got <= 0) break;
                    Heard(p, got, from.sin_addr.s_addr);
                }
            for (int n = 0; n < 64; n++)
            {
                uint32_t from = 0;
                const int got = Net::SideReceive(p, sizeof p, &from);
                if (got < 0) break;
                Heard(p, got, from);
            }
            const double now = Now();
            g_games.erase(std::remove_if(g_games.begin(), g_games.end(), [&](const Game& g) { return now - g.seen > 4.0; }), g_games.end());
        }
        // the host: his game, where a search hears it
        void Announce1(const uint8_t* base)
        {
            static double at = 0;
            if (Now() - at < 1.0) return;
            at = Now();
            Announce a{}; memcpy(a.magic, "CO4L", 4); a.version = ProtocolVersion; a.session = g_lobbyId; a.gamePort = uint16_t(NetPort());
            a.state = g_state; a.players = PeerConnected() ? 2 : 1; a.inMission = InMission(base) ? 1 : 0; a.difficulty = uint8_t(Difficulty(base));
            strncpy(a.name, MyName().c_str(), 31); strncpy(a.pick, Missions[g_pick].map, 31);
            Net::SideSend(&a, sizeof a);   // each network's broadcast, the System Link peers
            if (g_tell == INVALID_SOCKET)
            {
                g_tell = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                BOOL yes = TRUE; setsockopt(g_tell, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof yes);
                u_long nb = 1; ioctlsocket(g_tell, FIONBIO, &nb);
            }
            auto tell = [&](const char* address) {
                sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(uint16_t(NetPort() + 2));
                if (inet_pton(AF_INET, address, &to.sin_addr) == 1) sendto(g_tell, reinterpret_cast<const char*>(&a), sizeof a, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
            };
            tell("127.0.0.1"); tell("255.255.255.255");
            for (const std::string& i : g_invites) { tell(i.c_str()); in_addr ia{}; if (inet_pton(AF_INET, i.c_str(), &ia) == 1) Net::SideSend(&a, sizeof a, ia.s_addr); }
        }
        // goodbye, a few times (unreliable), before the socket goes
        void Goodbye(const uint8_t* base)
        {
            if (!NetOpen()) return;
            for (int k = 0; k < 3; k++)
            {
                if (IsJoin()) { Hello h{}; h.version = ProtocolVersion; h.leaving = 1; strncpy(h.name, MyName().c_str(), 31); SendUnreliable(K_HELLO, &h, sizeof h); }
                else if (IsHost()) { Beacon b{}; b.version = ProtocolVersion; b.state = L_CLOSED; strncpy(b.name, MyName().c_str(), 31); (void)base; SendUnreliable(K_BEACON, &b, sizeof b); }
            }
        }
        void Leave(const uint8_t* base, const std::string& why)
        {
            Goodbye(base);
            SetMode(Mode::Off);
            g_peerName.clear(); g_connectedSaid = false; g_hostMap.clear(); g_haveBeacon = false; g_ready = false; g_rtt = -1;
            g_friendReady = false; g_friendRtt = -1; g_state = L_OPEN; g_invites.clear(); g_helloHeardAt = -1;
            if (!why.empty()) Status(why);
        }
        std::string Addresses()
        {
            static std::string text; static double at = -100;
            if (Now() - at < 10) return text;
            at = Now();
            const Net::Summary net = Net::Status();
            text = net.address;
            return text;
        }
    }

    void LobbyFrame(PPCContext& ctx, uint8_t* base)
    {
        (void)ctx;
        const double now = Now();
        const bool in = InMission(base);
        if (!g_lobbyId) g_lobbyId = (GetTickCount() * 2246822519u) ^ (GetCurrentProcessId() << 7) ^ 0x4C4F4259u;
        // the window's wishes
        std::vector<Action> actions;
        { std::lock_guard<std::mutex> lock(g_lock); actions.swap(g_actions); }
        for (const Action& a : actions)
        {
            switch (a.kind)
            {
            case A_HOST:
                if (in || IsHost()) break;
                if (IsJoin()) Leave(base, "");
                SetMode(Mode::Host);
                g_state = L_OPEN; g_friendReady = false; g_peerName.clear();
                Status("You are hosting. Your friend can find your game (F2, then Join) or type your address.");
                break;
            case A_JOIN:
                if (in || a.text.empty()) break;
                if (GetMode() != Mode::Off) Leave(base, "");
                SetMode(Mode::Join, a.text);
                g_joinAddress = a.text; g_joinedAt = now; g_ready = false; g_haveBeacon = false; g_connectedSaid = false; g_hostMap.clear();
                Status("Joining " + a.text + "...");
                break;
            case A_LEAVE: if (!in) Leave(base, "You left the lobby."); break;
            case A_PICK: if (IsHost() && g_state == L_OPEN) g_pick = std::clamp(a.value, 0, MissionTotal - 1); break;
            case A_DIFF: if (IsHost() && g_state == L_OPEN) g_difficulty = std::clamp(a.value, 0, 2); break;
            case A_SCORING: if (IsHost()) g_scoring = a.value != 0; break;
            case A_READY: if (IsJoin()) { g_ready = a.value != 0; printf("coop: lobby: %s\n", g_ready ? "ready" : "not ready"); fflush(stdout); } break;
            case A_START:
                if (IsHost() && g_state == L_OPEN && !in) { g_state = L_COUNTDOWN; g_startAt = now + (PeerConnected() ? 5.0 : 2.0); printf("coop: lobby: the countdown to %s\n", Missions[g_pick].map); fflush(stdout); }
                break;
            case A_CANCEL: if (IsHost() && g_state == L_COUNTDOWN) { g_state = L_OPEN; Status("The start was called off."); } break;
            case A_INVITE:
                if (IsHost() && !a.text.empty() && std::find(g_invites.begin(), g_invites.end(), a.text) == g_invites.end())
                { if (g_invites.size() >= 4) g_invites.erase(g_invites.begin()); g_invites.push_back(a.text); Status("Invited " + a.text + ": your game is sent there each second."); }
                break;
            }
        }
        if (g_leaveNow) { g_leaveNow = false; Leave(base, "The host closed the lobby."); }
        // the joining copy: no answer from the address
        if (IsJoin() && !PeerConnected() && g_joinedAt >= 0 && now - g_joinedAt > 12 && !in)
        {
            static double toldAt = -100;
            if (now - toldAt > 12) { toldAt = now; Status("No answer from " + g_joinAddress + " yet. Is the host's game in the co-op lobby? Is UDP 28970-28972 allowed through both firewalls?"); }
        }
        // finding games while the window is open (co-op off), telling of ours while hosting
        const bool windowOpen = now - g_windowAt.load() < 1.0;
        if (GetMode() == Mode::Off && windowOpen && !in) Search(); else { FindSocket(false); g_games.clear(); }
        if (IsHost() && !in && g_state != L_STARTED) Announce1(base);
        // the view for the window
        View v;
        v.mode = GetMode(); v.inMission = in; v.connected = PeerConnected(); v.haveBeacon = g_haveBeacon; v.ready = g_ready; v.friendReady = g_friendReady;
        v.scoring = ScoringOn(); v.myName = MyName(); v.peerName = g_peerName; v.joinAddress = g_joinAddress; v.status = g_status; v.statusAt = g_statusAt;
        v.hostMap = MyMap(); v.joinedAt = g_joinedAt; v.now = now; v.rtt = g_rtt; v.friendRtt = g_friendRtt; v.games = g_games;
        v.addresses = Addresses();
        if (IsJoin() && g_haveBeacon)
        {
            v.pick = std::max(0, MissionIndex(g_beacon.pick)); v.difficulty = g_beacon.difficulty; v.state = g_beacon.state;
            v.countdown = g_beacon.countdown > 0 ? float(std::max(0.0, g_beacon.countdown - (now - g_beaconAt))) : 0.0f;
            v.friendReady = g_beacon.friendReady != 0;
        }
        else
        {
            v.pick = g_pick; v.difficulty = Difficulty(base); v.state = g_state;
            v.countdown = g_state == L_COUNTDOWN ? float(std::max(0.0, g_startAt - now)) : 0.0f;
        }
        // COD3_COOP_LOBBYTEST (the tests): the window's presses by themselves: the joining copy marks
        // itself ready 3 s after the host's beacon; the host starts 3 s after his friend is ready
        if (getenv("COD3_COOP_LOBBYTEST") && !in)
        {
            static double readyAt = -1, startAt = -1, foundAt = -1;
            static bool opened = false;
            if (const char* m = getenv("COD3_COOP_LOBBYMAP"); m && IsHost() && g_state == L_OPEN && MissionIndex(m) >= 0) g_pick = MissionIndex(m);
            static const double openAt = [] { const char* t = getenv("COD3_COOP_LOBBYOPENAT"); return t ? atof(t) : 15.0; }();
            if (!opened && now > openAt) { opened = true; Overlay::SetLobbyOpen(true); }
            if (GetMode() == Mode::Off)
            {
                g_windowAt.store(now);   // (as if the window were open: the search runs)
                if (!g_games.empty()) { if (foundAt < 0) foundAt = now + 2; else if (now >= foundAt) { printf("coop: lobby test: joining %s's game at %s\n", g_games[0].name.c_str(), g_games[0].address.c_str()); fflush(stdout); Act(A_JOIN, 0, g_games[0].address); foundAt = now + 1e9; } }
            }
            if (IsJoin() && g_haveBeacon && !g_ready) { if (readyAt < 0) readyAt = now + 3; else if (now >= readyAt) { g_ready = true; printf("coop: lobby test: ready\n"); fflush(stdout); } }
            if (IsHost() && g_friendReady && g_state == L_OPEN) { if (startAt < 0) startAt = now + 3; else if (now >= startAt) { g_state = L_COUNTDOWN; g_startAt = now + 5; startAt = now + 1e9; printf("coop: lobby test: start\n"); fflush(stdout); } }
        }
        v.swapping = Swapping(); v.swapAt = g_swapAt; v.myMap = MyMap();
        std::lock_guard<std::mutex> lock(g_lock);
        g_view = std::move(v);
    }
}

// ---- the window (the overlay's thread) -----------------------------------------
namespace Coop
{
    namespace
    {
        const ImVec4 Gold(0.93f, 0.80f, 0.45f, 1.0f), Grey(0.70f, 0.70f, 0.70f, 1.0f), Green(0.45f, 0.90f, 0.45f, 1.0f), Red(0.95f, 0.40f, 0.35f, 1.0f);
        bool Available() { return true; }
        float g_uiScale = 1.0f;   // (v46) the lobby's and loading screen's text, by the window's height (1.8 at 1080)
        void Shadowed(ImDrawList* dl, ImVec2 at, ImU32 c, const char* t, float scale)
        {
            ImFont* f = ImGui::GetFont(); const float size = ImGui::GetFont()->FontSize * scale * g_uiScale;
            dl->AddText(f, size, ImVec2(at.x + 2, at.y + 2), IM_COL32(0, 0, 0, 200), t);
            dl->AddText(f, size, at, c, t);
        }
        std::string Ping(float rtt) { if (rtt < 0) return "-"; char t[32]; snprintf(t, sizeof t, "%d ms", int(rtt * 1000 + 0.5f)); return t; }
        void PlayerRow(int slot, const std::string& name, bool host, bool present, bool ready, float rtt)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%d", slot);
            ImGui::TableSetColumnIndex(1);
            if (!present) { ImGui::TextColored(Grey, "Waiting for a player..."); return; }
            ImGui::TextUnformatted(name.c_str());
            if (host) { ImGui::SameLine(); ImGui::TextColored(Gold, "[HOST]"); }
            ImGui::TableSetColumnIndex(2);
            if (ready) ImGui::TextColored(Green, "READY"); else ImGui::TextColored(Grey, "not ready");
            ImGui::TableSetColumnIndex(3); ImGui::TextUnformatted(Ping(rtt).c_str());
        }


        // ---- (v46) the lobby as a screen of the game's own, World at War's way ----
        // the real controller, for the lobby's own navigation (the game's input is held while it is open)
        struct PadEdges { uint16_t down = 0, pressed = 0; int16_t lx = 0, ly = 0; };
        PadEdges ReadPad()
        {
            static uint16_t was = 0;
            PadEdges p;
            Input::RealPad(p.down, p.lx, p.ly);
            p.pressed = uint16_t(p.down & ~was);
            was = p.down;
            return p;
        }
        void FeedImGuiPad(const PadEdges& p, bool active)
        {
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
            io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
            const uint16_t b = active ? p.down : 0;
            io.AddKeyEvent(ImGuiKey_GamepadDpadUp, (b & 0x0001) != 0);
            io.AddKeyEvent(ImGuiKey_GamepadDpadDown, (b & 0x0002) != 0);
            io.AddKeyEvent(ImGuiKey_GamepadDpadLeft, (b & 0x0004) != 0);
            io.AddKeyEvent(ImGuiKey_GamepadDpadRight, (b & 0x0008) != 0);
            io.AddKeyEvent(ImGuiKey_GamepadFaceDown, (b & 0x1000) != 0);    // A: choose
            io.AddKeyEvent(ImGuiKey_GamepadFaceRight, (b & 0x2000) != 0);   // B: back
            const float lx = active ? p.lx / 32767.0f : 0, ly = active ? p.ly / 32767.0f : 0;
            io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickLeft, lx < -0.4f, std::max(0.0f, -lx));
            io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickRight, lx > 0.4f, std::max(0.0f, lx));
            io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickUp, ly > 0.4f, std::max(0.0f, ly));
            io.AddKeyAnalogEvent(ImGuiKey_GamepadLStickDown, ly < -0.4f, std::max(0.0f, -ly));
        }
        enum Page { P_MAIN, P_ADDRESS };
        std::atomic<int> g_page{ P_MAIN };
        const ImU32 CGold = IM_COL32(236, 200, 112, 255), CText = IM_COL32(232, 224, 205, 255), CDim = IM_COL32(150, 142, 125, 255), CGreen = IM_COL32(120, 220, 120, 255);
        void DText(ImDrawList* dl, ImVec2 at, ImU32 c, const std::string& t, float scale) { Shadowed(dl, at, c, t.c_str(), scale); }
        float TextW(const std::string& t, float scale) { return ImGui::GetFont()->CalcTextSizeA(ImGui::GetFont()->FontSize * scale * g_uiScale, 1e5f, 0, t.c_str()).x; }
        float LineH(float scale) { return ImGui::GetFont()->FontSize * scale * g_uiScale; }
        // a big menu entry, World at War's: the text alone, gold and marked when chosen
        bool MenuItem(const char* label, bool enabled = true, float scale = 1.6f)
        {
            ImGui::PushID(label);
            const float h = ImGui::GetFontSize() * scale + 14;
            const ImVec2 at = ImGui::GetCursorScreenPos();
            const float w = ImGui::GetContentRegionAvail().x;
            ImGui::BeginDisabled(!enabled);
            const bool pressed = ImGui::InvisibleButton("##item", ImVec2(w, h));
            const bool hot = ImGui::IsItemHovered() || ImGui::IsItemFocused();
            ImGui::EndDisabled();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            if (hot && enabled)
            {
                dl->AddRectFilledMultiColor(at, ImVec2(at.x + w, at.y + h), IM_COL32(236, 200, 112, 70), IM_COL32(236, 200, 112, 0), IM_COL32(236, 200, 112, 0), IM_COL32(236, 200, 112, 70));
                dl->AddRectFilled(at, ImVec2(at.x + 5, at.y + h), CGold);
            }
            DText(dl, ImVec2(at.x + 18, at.y + 7), !enabled ? CDim : hot ? CGold : CText, label, scale);
            ImGui::PopID();
            return pressed && enabled;
        }
        void SectionTitle(const char* t)
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 at = ImGui::GetCursorScreenPos();
            DText(dl, at, CGold, t, 1.15f);
            const float y = at.y + ImGui::GetFontSize() * 1.15f + 6;
            dl->AddLine(ImVec2(at.x, y), ImVec2(at.x + ImGui::GetContentRegionAvail().x, y), IM_COL32(236, 200, 112, 110), 1.5f);
            ImGui::Dummy(ImVec2(1, ImGui::GetFontSize() * 1.15f + 14));
        }
        void PlayerCard(const char* slot, const std::string& name, bool host, bool present, bool ready, float rtt, float w, float h)
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 a = ImGui::GetCursorScreenPos(), b(a.x + w, a.y + h);
            dl->AddRectFilled(a, b, IM_COL32(20, 18, 14, 225));
            dl->AddRect(a, b, present ? IM_COL32(236, 200, 112, 150) : IM_COL32(120, 112, 95, 90), 0, 0, 1.5f);
            DText(dl, ImVec2(a.x + 14, a.y + 10), CDim, slot, 0.95f);
            if (!present)
            {
                const int dots = int(ImGui::GetTime() * 2) % 4;
                DText(dl, ImVec2(a.x + 14, a.y + h * 0.42f), CDim, std::string("Waiting for a player") + std::string(size_t(dots), '.'), 1.2f);
            }
            else
            {
                DText(dl, ImVec2(a.x + 14, a.y + h * 0.36f), CText, name, 1.5f);
                if (host) DText(dl, ImVec2(a.x + 14, a.y + h * 0.70f), CGold, "HOST", 1.0f);
                const std::string r = ready ? "READY" : "NOT READY";
                DText(dl, ImVec2(b.x - TextW(r, 1.1f) - 14, a.y + h * 0.70f), ready ? CGreen : CDim, r, 1.1f);
                if (rtt >= 0)
                {
                    // the connection, as four bars
                    const int bars = rtt < 0.06f ? 4 : rtt < 0.12f ? 3 : rtt < 0.2f ? 2 : 1;
                    for (int i = 0; i < 4; i++)
                    {
                        const float x = b.x - 60 + i * 11, top = a.y + 14 + (3 - i) * 5;
                        dl->AddRectFilled(ImVec2(x, top), ImVec2(x + 7, a.y + 34), i < bars ? CGreen : IM_COL32(90, 85, 75, 200));
                    }
                }
            }
            ImGui::Dummy(ImVec2(w, h));
        }
        void Prompt(ImDrawList* dl, float& x, float y, const char* button, ImU32 buttonColour, const char* what)
        {
            const float r = ImGui::GetFontSize() * 0.62f;
            dl->AddCircleFilled(ImVec2(x + r, y + r), r, buttonColour);
            const float bw = TextW(button, 0.85f);
            dl->AddText(ImGui::GetFont(), ImGui::GetFontSize() * 0.85f, ImVec2(x + r - bw * 0.5f, y + r - ImGui::GetFontSize() * 0.43f), IM_COL32(15, 15, 15, 255), button);
            DText(dl, ImVec2(x + r * 2 + 8, y + r - ImGui::GetFontSize() * 0.55f), CText, what, 1.0f);
            x += r * 2 + 8 + TextW(what, 1.0f) + 34;
        }

        // the main menu's co-op entry (the lobby closed); true when drawn
        bool MenuEntry(const View& v, int width, int height, float k)
        {
            if (!Overlay::MainMenuShown()) return false;
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            const float W = float(width), H = float(height);
            const float bw = std::max(430 * k, TextW("Your lobby: waiting for a friend", 1.0f) + 120 * k), bh = LineH(1.55f) + LineH(1.0f) + 34 * k, x = W - bw - 40 * k, y = H - bh - 60 * k;
            fg->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + bw, y + bh), IM_COL32(10, 9, 7, 210), IM_COL32(30, 24, 12, 210), IM_COL32(30, 24, 12, 210), IM_COL32(10, 9, 7, 210));
            fg->AddRectFilled(ImVec2(x, y), ImVec2(x + 6 * k, y + bh), CGold);
            Shadowed(fg, ImVec2(x + 22 * k, y + 12 * k), CGold, "CO-OP CAMPAIGN", 1.55f);
            std::string line = "Play the campaign with a friend";
            if (v.mode == Mode::Host) line = std::string("Your lobby: ") + (v.connected ? v.peerName + " is in" : "waiting for a friend");
            else if (v.mode == Mode::Join) line = v.connected ? "In " + v.peerName + "'s lobby" : "Joining " + v.joinAddress + "...";
            Shadowed(fg, ImVec2(x + 22 * k, y + 12 * k + LineH(1.0f) * 1.55f + 6 * k), CText, line.c_str(), 1.0f);
            const char* key = "Y  /  C";
            Shadowed(fg, ImVec2(x + bw - TextW(key, 1.0f) - 18 * k, y + bh - LineH(1.0f) - 12 * k), CDim, key, 1.0f);
            // opened by the pad's Y, the C key, or a click on it
            static bool cWas = false, clickWas = false;
            const PadEdges p = ReadPad();
            bool open = false;
            if (Window::HasFocus())
            {
                const bool c = (GetAsyncKeyState('C') & 0x8000) != 0;
                if (c && !cWas) open = true;
                cWas = c;
                const bool click = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
                if (click && !clickWas)
                {
                    POINT pt{}; GetCursorPos(&pt);
                    if (HWND w = static_cast<HWND>(Window::Handle())) { ScreenToClient(w, &pt); RECT rc{}; GetClientRect(w, &rc);
                        const float sx = rc.right > 0 ? W / float(rc.right) : 1.0f, sy = rc.bottom > 0 ? H / float(rc.bottom) : 1.0f;
                        const float px = pt.x * sx, py = pt.y * sy;
                        if (px >= x && px <= x + bw && py >= y && py <= y + bh) open = true; }
                }
                clickWas = click;
                if (p.pressed & 0x8000) open = true;
            }
            if (open) { g_page.store(P_MAIN); Overlay::SetLobbyOpen(true); }
            return true;
        }

        void DrawScreen(const View& v, int width, int height, float k)
        {
            const float W = float(width), H = float(height);
            const PadEdges pad = ReadPad();
            const bool typing = ImGui::GetIO().WantTextInput;
            FeedImGuiPad(pad, true);
            ImDrawList* bg = ImGui::GetBackgroundDrawList();
            bg->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(4, 4, 3, 215));
            bg->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(W, H * 0.22f), IM_COL32(45, 34, 14, 160), IM_COL32(45, 34, 14, 160), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0));
            bg->AddRectFilledMultiColor(ImVec2(0, H * 0.8f), ImVec2(W, H), IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32(45, 34, 14, 170), IM_COL32(45, 34, 14, 170));
            const float left = W * 0.06f, top = H * 0.07f;
            DText(bg, ImVec2(left, top), CGold, "CO-OP CAMPAIGN", 2.6f);
            std::string sub = "Host a game or join a friend's";
            const bool host = v.mode == Mode::Host, lobby = v.mode != Mode::Off && (host || (v.connected && v.haveBeacon));
            if (v.inMission) sub = "In a co-op mission";
            else if (lobby) sub = host ? "Your game  -  " + std::string(v.connected ? "2" : "1") + "/2 players" : v.peerName + "'s game";
            else if (v.mode == Mode::Join) sub = "Joining " + v.joinAddress + "...";
            DText(bg, ImVec2(left, top + LineH(1.0f) * 2.6f + 4), CDim, sub, 1.2f);
            const float bodyTop = top + LineH(1.0f) * 4.4f + 20 * k, bodyBottom = H * 0.88f;
            // one window over all of it, for the widgets and the pad's navigation
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(W, H));
            ImGui::SetNextWindowBgAlpha(0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.14f, 0.10f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.55f, 0.45f, 0.22f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.75f, 0.62f, 0.30f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.11f, 0.09f, 0.95f));
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.55f, 0.45f, 0.22f, 0.55f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.65f, 0.53f, 0.26f, 0.75f));
            ImGui::PushStyleColor(ImGuiCol_NavHighlight, ImVec4(0.93f, 0.80f, 0.45f, 1.0f));
            ImGui::Begin("##coop_screen", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar);
            ImGui::SetWindowFontScale(g_uiScale);
            auto close = [&] { g_page.store(P_MAIN); Overlay::SetLobbyOpen(false); };
            const float colW = W * 0.30f, rightX = W * 0.40f, rightW = W * 0.54f;
            if (v.inMission)
            {
                ImGui::SetCursorScreenPos(ImVec2(left, bodyTop));
                ImGui::BeginChild("info", ImVec2(W * 0.6f, 0), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
                SectionTitle("IN A MISSION");
                ImGui::TextColored(Gold, "%s", MissionTitle(v.hostMap).c_str());
                ImGui::Text("%s and %s", v.myName.c_str(), v.peerName.empty() ? "(nobody yet)" : v.peerName.c_str());
                ImGui::Text("Connection: %s   ping %s", v.connected ? "good" : "lost", Ping(v.mode == Mode::Host ? v.friendRtt : v.rtt).c_str());
                ImGui::Spacing();
                ImGui::TextColored(Grey, "To leave, quit the mission from the pause menu.");
                ImGui::Spacing();
                if (MenuItem("RESUME")) close();
                ImGui::EndChild();
            }
            else if (v.mode == Mode::Off)
            {
                // left: the choices
                ImGui::SetCursorScreenPos(ImVec2(left, bodyTop));
                ImGui::BeginChild("menu", ImVec2(colW, bodyBottom - bodyTop), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
                if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
                if (MenuItem("HOST GAME")) Act(A_HOST);
                if (MenuItem("JOIN BY ADDRESS")) g_page.store(P_ADDRESS);
                if (MenuItem("BACK")) close();
                ImGui::Dummy(ImVec2(1, 30 * k));
                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + colW - 20);
                ImGui::TextColored(Grey, "In co-op you and a friend play the same mission. A player the enemy brings down can be revived by the other: go to him and hold Use. The mission fails if both of you are down, or if one bleeds out.");
                ImGui::PopTextWrapPos();
                ImGui::EndChild();
                // right: games found, and joining by address
                ImGui::SetCursorScreenPos(ImVec2(rightX, bodyTop));
                ImGui::BeginChild("games", ImVec2(rightW, bodyBottom - bodyTop), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
                SectionTitle("GAMES ON YOUR NETWORK");
                if (v.games.empty()) { const int dots = int(v.now * 2) % 4; ImGui::TextColored(Grey, "Searching%.*s", dots, "..."); }
                for (const Game& g : v.games)
                {
                    ImGui::PushID(int(g.session));
                    char label[200];
                    snprintf(label, sizeof label, "%s      %s      %u/2  %s", g.name.c_str(), MissionTitle(g.pick).c_str(), g.players, g.inMission ? "playing" : g.state == L_COUNTDOWN ? "starting" : "in the lobby");
                    const bool can = g.players < 2 && !g.inMission;
                    ImGui::BeginDisabled(!can);
                    if (ImGui::Selectable(label, false, 0, ImVec2(0, ImGui::GetFontSize() * 1.8f))) Act(A_JOIN, 0, g.address);
                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
                ImGui::Dummy(ImVec2(1, 24 * k));
                SectionTitle("JOIN BY ADDRESS");
                static char address[64] = {};
                static bool filled = false;
                if (!filled)
                {
                    filled = true;
                    std::string a = getenv("COD3_COOP_HOSTIP") ? getenv("COD3_COOP_HOSTIP") : "";
                    if (a.empty()) { std::string peers = Settings::Get().systemLinkPeers; for (char& c : peers) if (c == ',' || c == ';') c = ' '; const size_t s = peers.find_first_not_of(' '); if (s != std::string::npos) a = peers.substr(s, peers.find(' ', s) == std::string::npos ? std::string::npos : peers.find(' ', s) - s); }
                    strncpy(address, a.c_str(), sizeof address - 1);
                }
                ImGui::TextColored(Grey, "Your friend's LAN or Tailscale address:");
                ImGui::SetNextItemWidth(rightW * 0.5f);
                if (g_page.load() == P_ADDRESS) { ImGui::SetKeyboardFocusHere(); g_page.store(-1); }
                const bool enter = ImGui::InputText("##address", address, sizeof address, ImGuiInputTextFlags_EnterReturnsTrue);
                ImGui::SameLine();
                if ((ImGui::Button("  JOIN  ") || enter) && address[0]) Act(A_JOIN, 0, address);
                ImGui::Dummy(ImVec2(1, 16 * k));
                ImGui::TextColored(Grey, "You are \"%s\" (F11, Multiplayer: Player name). This PC: %s", v.myName.c_str(), v.addresses.c_str());
                ImGui::EndChild();
                if (!typing && (pad.pressed & 0x2000)) close();
            }
            else if (!lobby)
            {
                ImGui::SetCursorScreenPos(ImVec2(left, bodyTop));
                ImGui::BeginChild("joining", ImVec2(W * 0.6f, 0), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
                const int dots = int(v.now * 2) % 4;
                ImGui::TextColored(Gold, "Joining %s%.*s", v.joinAddress.c_str(), dots, "...");
                ImGui::TextColored(Grey, "Your friend's game must be hosting (Co-op Campaign, Host Game).");
                ImGui::Dummy(ImVec2(1, 20 * k));
                if (MenuItem("CANCEL")) Act(A_LEAVE);
                ImGui::EndChild();
                if (!typing && (pad.pressed & 0x2000)) Act(A_LEAVE);
            }
            else
            {
                const bool editable = host && v.state == L_OPEN;
                const int pick = std::clamp(v.pick, 0, MissionTotal - 1);
                // left: the missions
                ImGui::SetCursorScreenPos(ImVec2(left, bodyTop));
                ImGui::BeginChild("missions", ImVec2(colW, bodyBottom - bodyTop), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
                SectionTitle("MISSION");
                for (int i = 0; i < MissionTotal; i++)
                {
                    char label[64]; snprintf(label, sizeof label, "%2d   %s", i + 1, Missions[i].title);
                    ImGui::BeginDisabled(!editable && i != pick);
                    const float rowH = std::min(ImGui::GetFontSize() * 1.45f, (bodyBottom - bodyTop - ImGui::GetFontSize() * 2.6f) / float(MissionTotal) - ImGui::GetStyle().ItemSpacing.y);
                    if (ImGui::Selectable(label, i == pick, 0, ImVec2(0, rowH)) && editable) Act(A_PICK, i);
                    if (i == pick && ImGui::IsWindowAppearing()) ImGui::SetItemDefaultFocus();
                    ImGui::EndDisabled();
                }
                ImGui::EndChild();
                // right: the mission, the settings, the players, the start
                ImGui::SetCursorScreenPos(ImVec2(rightX, bodyTop));
                ImGui::BeginChild("lobby", ImVec2(rightW, bodyBottom - bodyTop), ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
                {
                    ImDrawList* dl = ImGui::GetWindowDrawList();
                    const ImVec2 at = ImGui::GetCursorScreenPos();
                    char chapter[32]; snprintf(chapter, sizeof chapter, "CHAPTER %d", pick + 1);
                    DText(dl, at, CDim, chapter, 1.0f);
                    DText(dl, ImVec2(at.x, at.y + ImGui::GetFontSize() * 1.1f), CText, Missions[pick].title, 1.9f);
                    ImGui::Dummy(ImVec2(1, ImGui::GetFontSize() * 3.2f));
                }
                SectionTitle("SETTINGS");
                ImGui::BeginDisabled(!editable);
                ImGui::TextColored(Grey, "Difficulty");
                for (int d = 0; d < 3; d++)
                {
                    ImGui::SameLine(0, d == 0 ? 30 * k : 10 * k);
                    const bool on = std::clamp(v.difficulty, 0, 2) == d;
                    if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.62f, 0.50f, 0.24f, 1.0f));
                    char b[32]; snprintf(b, sizeof b, "  %s  ", Difficulties[d]);
                    if (ImGui::Button(b)) Act(A_DIFF, d);
                    if (on) ImGui::PopStyleColor();
                }
                bool scoring = v.scoring;
                if (ImGui::Checkbox("Competitive scoring (points for kills and revives)", &scoring)) Act(A_SCORING, scoring ? 1 : 0);
                ImGui::EndDisabled();
                if (!host) ImGui::TextColored(Grey, "The host chooses the mission and the settings.");
                ImGui::Dummy(ImVec2(1, 14 * k));
                SectionTitle("PLAYERS");
                const float cw = (rightW - 20 * k) * 0.5f, ch = ImGui::GetFontSize() * 4.2f;
                if (host) { PlayerCard("PLAYER 1", v.myName, true, true, true, -1, cw, ch); ImGui::SameLine(0, 20 * k); PlayerCard("PLAYER 2", v.peerName, false, v.connected, v.friendReady, v.friendRtt, cw, ch); }
                else { PlayerCard("PLAYER 1", v.peerName, true, true, true, v.rtt, cw, ch); ImGui::SameLine(0, 20 * k); PlayerCard("PLAYER 2", v.myName, false, true, v.ready, -1, cw, ch); }
                ImGui::Dummy(ImVec2(1, 18 * k));
                if (host)
                {
                    if (v.state == L_COUNTDOWN) { if (MenuItem("CANCEL THE START") || (pad.pressed & 0x0010)) Act(A_CANCEL); }
                    else
                    {
                        const bool can = !v.connected || v.friendReady;
                        if (MenuItem(v.connected ? "START GAME" : "START ALONE", can) || (can && (pad.pressed & 0x0010))) Act(A_START);
                        if (!can) ImGui::TextColored(Grey, "      Waiting for %s to be ready", v.peerName.c_str());
                    }
                    static char invite[64] = {};
                    ImGui::SetNextItemWidth(rightW * 0.4f);
                    ImGui::InputTextWithHint("##invite", "invite: a friend's address", invite, sizeof invite);
                    ImGui::SameLine();
                    if (ImGui::Button("  INVITE  ") && invite[0]) Act(A_INVITE, 0, invite);
                    ImGui::SameLine(); ImGui::TextColored(Grey, "This PC: %s", v.addresses.c_str());
                }
                else
                {
                    if (MenuItem(v.ready ? "NOT READY" : "READY") || (!typing && (pad.pressed & 0x4000))) Act(A_READY, v.ready ? 0 : 1);
                }
                if (MenuItem("LEAVE LOBBY", true, 1.2f)) Act(A_LEAVE);
                ImGui::EndChild();
                if (!typing && (pad.pressed & 0x2000)) close();   // (B: back to the game's menu, the lobby kept)
            }
            // the status line and the controller's prompts
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            if (!v.status.empty() && v.now - v.statusAt < 12) DText(fg, ImVec2(left, H * 0.895f), CDim, v.status, 1.0f);
            {
                float x = left; const float y = H * 0.94f;
                Prompt(fg, x, y, "A", IM_COL32(110, 200, 90, 255), "Select");
                Prompt(fg, x, y, "B", IM_COL32(220, 80, 70, 255), "Back");
                if (lobby && !host) Prompt(fg, x, y, "X", IM_COL32(80, 140, 230, 255), v.ready ? "Not ready" : "Ready");
                if (lobby && host) Prompt(fg, x, y, ">", IM_COL32(220, 220, 220, 255), v.state == L_COUNTDOWN ? "Cancel" : "Start");
                DText(fg, ImVec2(W - left - TextW("Esc / F2  Close", 1.0f), y), CDim, "Esc / F2  Close", 1.0f);
            }
            ImGui::End();
            ImGui::PopStyleColor(7);
            ImGui::PopStyleVar(2);
        }

        void Draw(int width, int height, bool open)
        {
            View v; { std::lock_guard<std::mutex> lock(g_lock); v = g_view; }
            ImDrawList* fg = ImGui::GetForegroundDrawList();
            const float k = std::max(1.0f, height / 900.0f);
            g_uiScale = std::max(1.0f, height / 600.0f);
            // (v46) the checkpoint reload: World at War's loading screen while the new copy loads behind this one
            if (v.swapping)
            {
                const float W = float(width), H = float(height);
                const double t = v.now - (v.swapAt > 0 ? v.swapAt : v.now);
                const int fade = int(std::clamp(t / 0.6, 0.0, 1.0) * 255);
                fg->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), IM_COL32(6, 6, 5, fade));
                fg->AddRectFilledMultiColor(ImVec2(0, H * 0.62f), ImVec2(W, H), IM_COL32(40, 30, 14, 0), IM_COL32(40, 30, 14, 0), IM_COL32(60, 44, 18, fade * 3 / 4), IM_COL32(60, 44, 18, fade * 3 / 4));
                const std::string title = MissionTitle(v.myMap.empty() ? v.hostMap : v.myMap);
                const float ts = 2.6f, tw = ImGui::GetFont()->CalcTextSizeA(LineH(1.0f) * ts, 1e5f, 0, title.c_str()).x;
                Shadowed(fg, ImVec2(W * 0.08f, H * 0.70f), IM_COL32(240, 220, 170, fade), title.c_str(), ts);
                (void)tw;
                const int dots = int(v.now * 2.5) % 4;
                char line[64]; snprintf(line, sizeof line, "Loading the last checkpoint%.*s", dots, "...");
                Shadowed(fg, ImVec2(W * 0.08f, H * 0.70f + LineH(1.0f) * ts + 10 * k), IM_COL32(210, 200, 180, fade), line, 1.25f);
                // a bar that runs while it loads
                const float bx = W * 0.08f, by = H * 0.86f, bw = W * 0.84f, bh = 6 * k;
                fg->AddRectFilled(ImVec2(bx, by), ImVec2(bx + bw, by + bh), IM_COL32(255, 255, 255, fade / 6));
                const float run = float(std::fmod(t * 0.35, 1.0)), seg = 0.22f;
                const float a0 = std::max(0.0f, run - seg), a1 = std::min(1.0f, run);
                fg->AddRectFilled(ImVec2(bx + bw * a0, by), ImVec2(bx + bw * a1, by + bh), IM_COL32(240, 205, 115, fade));
                Shadowed(fg, ImVec2(W * 0.08f, by + bh + 12 * k), IM_COL32(170, 160, 140, fade), "Mission failed. Both players return to the last checkpoint together.", 1.0f);
                return;
            }
            // the countdown, open or not
            if (!v.inMission && v.mode != Mode::Off && v.state == L_COUNTDOWN)
            {
                char t[96]; snprintf(t, sizeof t, "%s  -  starting in %d", MissionTitle(Missions[std::clamp(v.pick, 0, MissionTotal - 1)].map).c_str(), int(std::ceil(v.countdown)));
                const float w = ImGui::GetFont()->CalcTextSizeA(LineH(1.0f) * 1.8f, 10000, 0, t).x;
                Shadowed(fg, ImVec2(width * 0.5f - w * 0.5f, height * 0.12f), IM_COL32(240, 205, 115, 255), t, 1.8f);
            }
            if (!open)
            {
                // the main menu's way in (v46): a co-op entry drawn on the game's own front end
                FeedImGuiPad(PadEdges{}, false);
                if (v.inMission) return;
                if (MenuEntry(v, width, height, k)) return;
                std::string hint = "F2  Co-op";
                if (v.mode == Mode::Host) hint = std::string("Co-op lobby: hosting, ") + (v.connected ? "2" : "1") + "/2 players  -  F2";
                if (v.mode == Mode::Join) hint = v.connected ? "Co-op lobby: " + v.peerName + "'s game  -  F2" : "Co-op: joining " + v.joinAddress + "...  -  F2";
                const float w = TextW(hint, 1.1f);
                Shadowed(fg, ImVec2(width - w - 24 * k, height - 48 * k), IM_COL32(235, 225, 200, 230), hint.c_str(), 1.1f);
                return;
            }
            g_windowAt.store(Now());
            DrawScreen(v, width, height, k);
        }
    }
}

// registered the first frame
namespace { struct Register { Register() { Overlay::SetLobby(Coop::Available, Coop::Draw); } } g_register; }

#endif
