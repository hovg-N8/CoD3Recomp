// Co-op campaign, first experiment (Team Justin, session 4): two copies of
// the campaign on one PC. LOCAL DIAGNOSTIC, off unless COD3_COOP is set.
//
// The campaign program is a server and a client in one process, joined by a
// loopback: NET_SendLoopPacket (sub_82451C68: the socket r3, the length r4,
// the data r5, the address by value in r6..r8, its port at +18) puts a
// packet in a queue of four 3076-byte slots (3072 bytes and the length;
// get +12304, send +12308) at 0x82A0CFA8: with socket 4 the client's queue
// picked by the address's port (12312 bytes each), else the server's at
// 0x82A18F48. The server has one client (SV_Startup sub_82514240 makes
// svs.clients 4832 bytes) and connects it itself (SVC_DirectConnect
// sub_82514440, from SV_SpawnServer sub_82517A00).
//
// COD3_COOP=host: every packet the server sends its client (socket 4) is
//   also sent over UDP to 127.0.0.1:COD3_COOP_PORT+1 (default 28961).
// COD3_COOP=join: listens on that port; from the first packet in, the
//   local server's packets to its own client are dropped and the host's are
//   put in the client's queue instead (each frame, from Com_EventLoop
//   sub_82535E80). The joining copy's client then draws the host's world
//   from the host's packets alone: the mirror test.
// Everything is logged ("coop: ...").

#ifndef COD3_MP

#include "kernel.h"
#include "overlay.h"
#include "input.h"
#include "settings.h"
#include "blood.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <cctype>
#include <map>
#include <unordered_map>
#include <thread>
#include <set>
#include <cmath>

extern "C" PPC_FUNC(__imp__sub_82451C68);
extern "C" PPC_FUNC(__imp__sub_82535E80);
extern "C" PPC_FUNC(__imp__sub_82517A00);

namespace
{
    enum class Mode { Off, Host, Join };
    Mode CoopMode()
    {
        static const Mode mode = [] {
            const char* t = getenv("COD3_COOP");
            if (!t) return Mode::Off;
            if (_stricmp(t, "host") == 0) return Mode::Host;
            if (_stricmp(t, "join") == 0) return Mode::Join;
            return Mode::Off;
        }();
        return mode;
    }
    int Port()
    {
        static const int port = [] { const char* t = getenv("COD3_COOP_PORT"); return t ? atoi(t) : 28960; }();
        return port;
    }

    // The other PC's address: COD3_COOP_HOSTIP (on the joining PC) or
    // COD3_COOP_JOINIP (on the host), else the first of the System Link
    // peers in CoD3Recomp.ini (system_link_peers: the other's Tailscale
    // address, as for multiplayer), else this PC (two copies on one PC).
    std::string OtherPc()
    {
        static const std::string other = [] {
            const char* e = getenv(CoopMode() == Mode::Host ? "COD3_COOP_JOINIP" : "COD3_COOP_HOSTIP");
            std::string v = e ? e : "";
            if (v.empty())
            {
                std::string peers = Settings::Get().systemLinkPeers;
                for (char& c : peers) if (c == ',' || c == ';') c = ' ';
                const size_t a = peers.find_first_not_of(' ');
                if (a != std::string::npos) v = peers.substr(a, peers.find(' ', a) == std::string::npos ? std::string::npos : peers.find(' ', a) - a);
            }
            if (v.empty()) v = "127.0.0.1";
            printf("coop: the other PC is %s\n", v.c_str());
            fflush(stdout);
            return v;
        }();
        return other;
    }
    SOCKET g_socket = INVALID_SOCKET;
    sockaddr_in g_joiner{};
    bool OpenSocket()
    {
        if (g_socket != INVALID_SOCKET) return true;
        static bool tried = false;
        if (tried) return false;
        tried = true;
        WSADATA data;
        WSAStartup(MAKEWORD(2, 2), &data);
        g_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (g_socket == INVALID_SOCKET) { printf("coop: no socket (%d)\n", WSAGetLastError()); fflush(stdout); return false; }
        u_long nonblocking = 1;
        ioctlsocket(g_socket, FIONBIO, &nonblocking);
        int big = 1 << 20;
        setsockopt(g_socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&big), sizeof(big));
        g_joiner.sin_family = AF_INET;
        g_joiner.sin_port = htons(uint16_t(Port() + 1));
        inet_pton(AF_INET, "127.0.0.1", &g_joiner.sin_addr);
        if (CoopMode() == Mode::Join)
        {
            if (bind(g_socket, reinterpret_cast<sockaddr*>(&g_joiner), sizeof(g_joiner)) != 0)
            {
                printf("coop: join: could not listen on port %d (%d)\n", Port() + 1, WSAGetLastError());
                fflush(stdout);
                closesocket(g_socket);
                g_socket = INVALID_SOCKET;
                return false;
            }
        }
        printf("coop: %s, UDP %s port %d\n", CoopMode() == Mode::Host ? "host" : "join",
               CoopMode() == Mode::Host ? "sending to" : "listening on", Port() + 1);
        fflush(stdout);
        return true;
    }

    std::atomic<bool> g_mirroring{false};
    bool g_standinFallback = false;   // stand-ins may be any friendly soldier (after 30 s)
    bool g_standinAnyTarget = false;  // stand-ins may be soldiers on a script path (after 10 s)
    uint32_t g_mapGen = 1;
    bool g_filmSkipFromHost = false;  // the joining copy: the host's film is over (CWF1)
    bool g_filmPeerReady = false;     // the host: the joining copy has loaded too (CWR1)
    bool g_filmToldWaiting = false;
    uint32_t g_lastHostEpoch = 0;     // the joining copy: the host's latest count (CWL1)
    uint32_t g_hostEpoch = 1;         // the host: counts mission starts and restarts (sent in CWL1)
    bool g_restartWanted = false;     // the joining copy: the host restarted the mission
    bool g_hostRelaunching = false;   // the joining copy: the host's game is starting itself again (CWX1)            // counts missions started: per-mission lists start over
    uint64_t g_clientAddress[3] = {};
    bool g_haveClientAddress = false;
    uint64_t g_sent = 0, g_dropped = 0, g_injected = 0, g_received = 0;

    std::string Hex(const uint8_t* base, uint32_t at, uint32_t count)
    {
        std::string out;
        char b[4];
        for (uint32_t i = 0; i < count; i++) { snprintf(b, sizeof(b), "%02X", base[at + i]); out += b; }
        return out;
    }
}

// NET_SendLoopPacket(socket, length, data, address)
PPC_FUNC(sub_82451C68)
{
    const Mode mode = CoopMode();
    if (mode == Mode::Off) { __imp__sub_82451C68(ctx, base); return; }
    const uint32_t sock = ctx.r3.u32, length = ctx.r4.u32, data = ctx.r5.u32;
    const uint32_t port = uint32_t(ctx.r8.u64 >> 32) & 0xFFFF;
    static int logged = 0;
    if (logged < 60)
    {
        logged++;
        printf("coop: loop packet socket %u length %u port %u address %016llX %016llX %016llX: %s\n", sock, length, port,
               (unsigned long long)ctx.r6.u64, (unsigned long long)ctx.r7.u64, (unsigned long long)ctx.r8.u64,
               Hex(base, data, length < 24 ? length : 24).c_str());
        fflush(stdout);
    }
    static const bool mirror = getenv("COD3_COOP_MIRROR") != nullptr;
    if (mirror && sock == 4 && length > 0 && length <= 3072)
    {
        if (mode == Mode::Host && OpenSocket())
        {
            uint8_t buffer[3076];
            const uint32_t n = length;
            memcpy(buffer, &n, 4);
            memcpy(buffer + 4, base + data, length);
            sendto(g_socket, reinterpret_cast<const char*>(buffer), int(length + 4), 0, reinterpret_cast<sockaddr*>(&g_joiner), sizeof(g_joiner));
            if (++g_sent % 500 == 1) { printf("coop: host: %llu packets sent to the joining copy\n", (unsigned long long)g_sent); fflush(stdout); }
        }
        if (mode == Mode::Join)
        {
            g_clientAddress[0] = ctx.r6.u64; g_clientAddress[1] = ctx.r7.u64; g_clientAddress[2] = ctx.r8.u64;
            g_haveClientAddress = true;
            if (g_mirroring.load())
            {
                if (++g_dropped % 500 == 1) { printf("coop: join: %llu of the local server's packets dropped\n", (unsigned long long)g_dropped); fflush(stdout); }
                return;
            }
        }
    }
    __imp__sub_82451C68(ctx, base);
}


// ---- Path 1, step 1: the world itself, sent by the host ------------------
// The campaign's client reads the world from the server's memory, not from
// its updates (the mirror test: 15-byte snapshots). So the host sends the
// world: the entity table the scripts use (0x82A2DCD0: 4096 pairs of the
// entity's address and a number; an entity's type byte at +0, origin +336,
// angles +352, actor +532, sentient +536, player +528, animation tree +552)
// as (index, type, origin, angles), on its own port (COD3_COOP_PORT + 2).
// The joining copy compares it with its own world (how far apart the two
// copies' soldiers are) and, with COD3_COOP_APPLY=1, puts its entities where
// the host's are. The host also logs, at COD3_COOP_PROBE seconds, where else
// an actor's and the player's position is kept (for the next step).
namespace
{
    constexpr uint32_t EntityTable = 0x82A2DCD0;
    void FilmSend(const char* tag);   // (the film's part, below)
    struct SyncEntry { uint16_t index; uint8_t type; uint8_t flags; float origin[3]; float angles[3]; uint32_t key; int32_t health; };
    static_assert(sizeof(SyncEntry) == 36);
    // What an entity is, the same in both copies whatever its number: its
    // type and its classname, targetname, target and script_noteworthy
    // (+564, +572, +580, +596; script strings, or texts when pointers).
    uint32_t KeyPart(const uint8_t* base, uint32_t e, uint32_t at)
    {
        uint32_t v = Guest::Read32(base, e + at);
        if (v < 0x40000000u || v >= 0xC0000000u) return v;
        // a Broc::string: its text's address first, when that is one
        if (const uint32_t inner = Guest::Read32(base, v); inner >= 0x40000000u && inner < 0xC0000000u && base[v] != 0 && !(base[v] >= 32 && base[v] < 127 && base[v + 1] >= 32 && base[v + 1] < 127)) v = inner;
        uint32_t h = 5381;
        for (uint32_t i = 0; i < 64; i++) { const uint8_t c = base[v + i]; if (!c) break; h = h * 33 + c; }
        return h;
    }
    std::string FieldText(const uint8_t* base, uint32_t e, uint32_t at)
    {
        uint32_t v = Guest::Read32(base, e + at);
        char t[16];
        if (v < 0x40000000u || v >= 0xC0000000u) { snprintf(t, sizeof t, "#%X", v); return t; }
        if (const uint32_t inner = Guest::Read32(base, v); inner >= 0x40000000u && inner < 0xC0000000u && !(base[v] >= 32 && base[v] < 127 && base[v + 1] >= 32 && base[v + 1] < 127)) v = inner;
        std::string out;
        for (uint32_t i = 0; i < 48; i++) { const uint8_t c = base[v + i]; if (c < 32 || c >= 127) break; out += char(c); }
        return "'" + out + "'";
    }
    std::string Fields(const uint8_t* base, uint32_t e)
    {
        std::string out;
        for (const uint32_t at : { 556u, 564u, 572u, 580u, 596u, 604u }) out += " +" + std::to_string(at) + "=" + FieldText(base, e, at);
        return out;
    }
    uint32_t EntityKey(const uint8_t* base, uint32_t e)
    {
        uint32_t h = Guest::Read8(base, e);
        // (only the classname: the scripts change a soldier's targetname and
        // target as he goes, 'area1_obs_germans1_spawned' then nothing, a
        // path node 'pgm37' given; a pair once made is kept, and a new one is
        // the nearest of the same classname, so this is enough)
        h = h * 1000003u ^ KeyPart(base, e, 564);
        return h;
    }
    SOCKET g_world = INVALID_SOCKET;
    sockaddr_in g_worldTo{};
    bool OpenWorld()
    {
        if (g_world != INVALID_SOCKET) return true;
        // (0.5.236) tried again each second for a minute: a process the title
        // relaunched for its next mission can start before the old one has
        // let its ports go (tests 3-5: co-op did not survive a relaunch)
        static int tries = 0;
        static auto lastTry = std::chrono::steady_clock::time_point{};
        const auto nowTp = std::chrono::steady_clock::now();
        if (tries >= 60 || (tries > 0 && nowTp - lastTry < std::chrono::seconds(1))) return false;
        tries++; lastTry = nowTp;
        static bool started = false;
        if (!started) { started = true; WSADATA data; WSAStartup(MAKEWORD(2, 2), &data); }
        g_world = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (g_world == INVALID_SOCKET) return false;
        u_long nb = 1; ioctlsocket(g_world, FIONBIO, &nb);
        int big = 4 << 20;
        setsockopt(g_world, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&big), sizeof(big));
        setsockopt(g_world, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&big), sizeof(big));
        g_worldTo.sin_family = AF_INET;
        g_worldTo.sin_port = htons(uint16_t(Port() + 2));
        inet_pton(AF_INET, OtherPc().c_str(), &g_worldTo.sin_addr);
        if (CoopMode() == Mode::Join)
        {
            sockaddr_in any{}; any.sin_family = AF_INET; any.sin_port = htons(uint16_t(Port() + 2)); any.sin_addr.s_addr = htonl(INADDR_ANY);
            if (bind(g_world, reinterpret_cast<sockaddr*>(&any), sizeof(any)) != 0)
            {
                printf("coop: world: could not listen on port %d yet (try %d)\n", Port() + 2, tries); fflush(stdout);
                closesocket(g_world); g_world = INVALID_SOCKET;
                return false;
            }
        }
        printf("coop: world sync %s on port %d\n", CoopMode() == Mode::Host ? "sending" : "receiving", Port() + 2);
        fflush(stdout);
        return true;
    }
    float F(const uint8_t* base, uint32_t at) { const uint32_t w = Guest::Read32(base, at); float f; memcpy(&f, &w, 4); return f; }
    void SetF(uint8_t* base, uint32_t at, float f) { uint32_t w; memcpy(&w, &f, 4); Guest::Write32(base, at, w); }
    uint32_t EntityAt(const uint8_t* base, uint32_t i)
    {
        const uint32_t e = Guest::Read32(base, EntityTable + i * 8);
        return e >= 0x40000000u && e < 0xC0000000u ? e : 0;
    }
    double Seconds()
    {
        static const auto start = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }

    // ---- (co-op v35) the network clock and the movement buffer ----------
    // Test 6 and the joining player's own logs: soldiers cartwheeling in
    // place, running sideways, popping up far away. The causes fixed here:
    // - the host's world went as one message of 340 entities (12 KB, 32 KB
    //   at most) twenty times a second. Tailscale carries 1280-byte packets,
    //   so each was ten pieces or more, and losing any one lost all of it:
    //   whole updates went missing, then the next one jumped. Now: parts of
    //   30 entries (1.1 KB, one packet each, each useful alone), and only
    //   what can move (soldiers, the players, things that have moved).
    // - places were timed by when they arrived, so internet jitter became
    //   speed jitter. Now each update carries the sender's clock; the
    //   receiver learns the offset and shows everything a little in the
    //   past (0.1 s plus the jitter it measures) between two real updates,
    //   and predicts ahead from the last speed (0.2 s at most) when one is
    //   late, as multiplayer games do.
    // - the entities the host deleted stayed in the joining copy's list for
    //   good (348 for minutes in the joiner's log), so their partners were
    //   pulled to old places. Now an entry not heard for a second is gone.
    struct NetClock
    {
        bool have = false;
        double offset = 0, jitter = 0.02;
        void Feed(double local, double remote)
        {
            const double sample = local - remote;
            if (!have || fabs(sample - offset) > 5.0) { have = true; offset = sample; jitter = 0.02; return; }
            const double d = sample - offset;
            offset += d < 0 ? d * 0.5 : d * 0.01;   // quick to a faster packet, slow to a slower route
            jitter += (fabs(d) - jitter) * 0.05;
        }
        double Delay() const
        {
            static const double base = [] { const char* t = getenv("COD3_COOP_DELAY"); return t ? std::clamp(atof(t), 0.0, 1.0) : 0.1; }();
            return std::clamp(base + 1.5 * jitter, base, base + 0.25);
        }
        // the sender's time to show now
        double Show(double local) const { return local - offset - Delay(); }
    };
    float LerpAngle(float from, float to, float f)
    {
        const float d = fmodf(to - from + 540.0f, 360.0f) - 180.0f;
        return from + d * f;
    }
    struct Sample { double t; float o[3]; float a[3]; };
    // One thing's recent places, by the sender's clock.
    struct Track
    {
        std::deque<Sample> s;
        uint32_t key = 0;
        void Add(double t, const float o[3], const float a[3])
        {
            if (!s.empty() && t <= s.back().t) { if (t == s.back().t) return; s.clear(); }   // (the sender started again)
            s.push_back(Sample{ t, { o[0], o[1], o[2] }, { a[0], a[1], a[2] } });
            while (s.size() > 40 || (s.size() > 2 && s.back().t - s.front().t > 2.0)) s.pop_front();
        }
        // place, angles and speed at sender time t
        bool At(double t, float o[3], float a[3], float v[3]) const
        {
            v[0] = v[1] = v[2] = 0;
            if (s.empty()) return false;
            const Sample* x = &s.front();
            const Sample* y = &s.front();
            if (t >= s.back().t) { y = &s.back(); x = s.size() > 1 ? &s[s.size() - 2] : y; }
            else if (t > s.front().t)
                for (size_t i = 0; i + 1 < s.size(); i++) if (s[i + 1].t > t) { x = &s[i]; y = &s[i + 1]; break; }
            const double span = y->t - x->t;
            if (span > 0.001 && span < 0.6) for (int k = 0; k < 3; k++) v[k] = float((y->o[k] - x->o[k]) / span);
            if (t >= s.back().t)
            {
                // late: carried on at the last speed for 0.2 s at most (not
                // up or down: a fall or a jump would overshoot)
                const float ahead = float(std::min(t - y->t, 0.2));
                for (int k = 0; k < 3; k++) { o[k] = y->o[k] + (k < 2 ? v[k] * ahead : 0.0f); a[k] = y->a[k]; }
                return true;
            }
            const float f = span > 0.0001 ? float((t - x->t) / span) : 1.0f;
            for (int k = 0; k < 3; k++) { o[k] = x->o[k] + (y->o[k] - x->o[k]) * f; a[k] = LerpAngle(x->a[k], y->a[k], f); }
            return true;
        }
    };
    // A stance shown on a body only once it has held 0.25 s (test 6: pairs
    // like "->1" then "->4" a few milliseconds apart made odd crouches).
    struct StanceFilter
    {
        uint32_t shown = 1, pending = 1;
        double since = 0;
        uint32_t Get(uint32_t raw, double now)
        {
            if (raw != pending) { pending = raw; since = now; }
            if (pending != shown && now - since >= 0.25) shown = pending;
            return shown;
        }
    };

    // Where else a position is kept: offsets in [0, span) of a block whose
    // three floats equal the given ones.
    std::string Copies(const uint8_t* base, uint32_t block, uint32_t span, const float v[3], uint32_t skip)
    {
        std::string out;
        char t[16];
        if (!(fabsf(v[0]) + fabsf(v[1]) + fabsf(v[2]) > 1.0f)) return " (zero)";
        for (uint32_t at = 0; at + 12 <= span; at += 4)
        {
            if (at == skip) continue;
            if (fabsf(F(base, block + at) - v[0]) < 0.5f && fabsf(F(base, block + at + 4) - v[1]) < 0.5f && fabsf(F(base, block + at + 8) - v[2]) < 0.5f)
            { snprintf(t, sizeof t, " +%u", at); out += t; }
        }
        return out.empty() ? " none" : out;
    }
    void Probe(uint8_t* base)
    {
        std::map<int, int> types;
        int valid = 0, actors = 0, shown = 0;
        for (uint32_t i = 0; i < 4096; i++)
        {
            const uint32_t e = EntityAt(base, i);
            if (!e) continue;
            valid++;
            types[Guest::Read8(base, e)]++;
            const uint32_t actor = Guest::Read32(base, e + 532), player = Guest::Read32(base, e + 528);
            if (actor) actors++;
            if ((actor || player) && shown < 6)
            {
                shown++;
                const float o[3] = { F(base, e + 336), F(base, e + 340), F(base, e + 344) };
                const float a[3] = { F(base, e + 352), F(base, e + 356), F(base, e + 360) };
                printf("coop: probe: entity %u key fields +564 %08X +572 %08X +580 %08X +596 %08X\n", i, Guest::Read32(base, e + 564), Guest::Read32(base, e + 572), Guest::Read32(base, e + 580), Guest::Read32(base, e + 596));
                for (const uint32_t at : { 564u, 572u })
                    if (const uint32_t v = Guest::Read32(base, e + at); v >= 0x40000000u && v < 0xC0000000u)
                        printf("coop: probe:   +%u -> %s\n", at, Hex(base, v, 24).c_str());
                printf("coop: probe: entity %u at %08X (type %u, %s %08X, tree %08X) origin %.1f %.1f %.1f angles %.1f %.1f %.1f; origin also at%s; angles also at%s\n",
                       i, e, Guest::Read8(base, e), player ? "player" : "actor", player ? player : actor, Guest::Read32(base, e + 552),
                       o[0], o[1], o[2], a[0], a[1], a[2], Copies(base, e, 1200, o, 336).c_str(), Copies(base, e, 1200, a, 352).c_str());
                if (player && player >= 0x40000000u && player < 0xC0000000u)
                    printf("coop: probe: the player's record %08X: origin at%s; its angles at%s\n", player,
                           Copies(base, player, 8000, o, 0xFFFFFFFF).c_str(), Copies(base, player, 8000, a, 0xFFFFFFFF).c_str());
                if (actor && actor >= 0x40000000u && actor < 0xC0000000u)
                    printf("coop: probe: the actor's record %08X: origin at%s\n", actor, Copies(base, actor, 4000, o, 0xFFFFFFFF).c_str());
            }
        }
        std::string list;
        char t[24];
        for (auto& [type, n] : types) { snprintf(t, sizeof t, " %d:%d", type, n); list += t; }
        printf("coop: probe at %.0f s: %d entities, %d actors; by type%s\n", Seconds(), valid, actors, list.c_str());
        fflush(stdout);
    }

    std::map<uint16_t, SyncEntry> g_hostWorld;
    // (co-op v35) the joining copy: when each of the host's entries was last
    // heard, their recent places, and the clocks of the two senders
    std::map<uint16_t, double> g_hostSeen;
    std::unordered_map<uint16_t, Track> g_tracks;
    NetClock g_hostClock;          // the joining copy: the host's clock
    NetClock g_joinerClock;        // the host: the joining copy's clock
    Track g_joinerTrack;           // the host: the joining player's places
    double g_lastWorldAt = -1;     // the joining copy: the host's world last heard
    int32_t g_hostPlayerIndex = -1;
    double g_standinShield = -1;   // the host: the stand-in's own grenade cannot hurt him till then
    uint32_t g_snaps = 0;          // soldiers put in place outright (too far off to walk)
    std::map<uint16_t, uint32_t> g_pairs;   // the joining copy: the host's entity number -> ours

    // ---- the players' stand-ins (path 1, the second player) -------------
    // The campaign has one player, so each copy shows the other's player as
    // one of its own friendly soldiers: on the host, a rifleman of the
    // level's (A) walks where the joining player is; on the joining copy,
    // another (B, sent with the host player's place) walks where the host
    // is. Their own copies of the two are put out of sight below the level:
    // A on the joining copy (that is the joining player himself), B on the
    // host. Sync entries mark them: 4 the joining player's stand-in, 8 the host's.
    int32_t g_avatarA = -1, g_avatarB = -1;   // the host's entity numbers
    bool g_avatarSpawned = false;
    uint32_t g_hostStance = 1, g_joinerStance = 1;   // 1 standing, 2 crouching, 4 prone
    uint32_t g_hostShots = 0, g_joinerShots = 0;     // shots fired so far (CWQ1 / CWP1)
    bool g_haveHostShots = false, g_haveJoinerShots = false;
    uint32_t g_hostBodyEnt = 0;                // the joining copy: its own soldier for the host
    float g_joinerPlace[6] = {};               // the joining player's origin and angles, on the host
    double g_joinerSeen = -1;
    uint32_t EntityRef(const uint8_t* base, uint32_t e)
    {
        for (uint32_t n = 0; n < 4096; n++)
            if (Guest::Read32(base, EntityTable + n * 8) == e) return n | (Guest::Read32(base, EntityTable + n * 8 + 4) << 14);
        return 0xFFFFFFFF;
    }
    void PutAt(uint8_t* base, uint32_t e, const float origin[3], const float* angles)
    {
        for (const uint32_t at : { 32u, 44u, 304u, 336u, 416u })
            for (int k = 0; k < 3; k++) SetF(base, e + at + 4 * k, origin[k]);
        if (angles) for (const uint32_t at : { 72u, 352u }) for (int k = 0; k < 3; k++) SetF(base, e + at + 4 * k, angles[k]);
        const uint32_t actor = Guest::Read32(base, e + 532);
        if (actor >= 0x40000000u && actor < 0xC0000000u) for (int k = 0; k < 3; k++) SetF(base, actor + 720 + 4 * k, origin[k]);
    }
    uint32_t g_scratch = 0;
    void WalkTo(PPCContext& ctx, uint8_t* base, uint32_t e, const float origin[3], float radius = 24.0f)
    {
        const uint32_t sentient = Guest::Read32(base, e + 536);
        const uint32_t ref = EntityRef(base, e);
        if (!g_scratch || sentient < 0x40000000u || ref == 0xFFFFFFFF) return;
        for (int k = 0; k < 3; k++) SetF(base, g_scratch + 4 * k, origin[k]);
        SetF(base, sentient + 24, radius);
        PPCContext call = ctx;
        call.r1.u32 = (call.r1.u32 - 0x400) & ~0xFu;
        call.r3.u64 = ref; call.r4.u64 = g_scratch; call.lr = 0;
        Guest::Lookup(0x824B1168)(call, base);   // the scripts' setgoalpos
    }
    // Follows a place: walks there, or is put there when far (or hidden).
    void Follow(PPCContext& ctx, uint8_t* base, uint32_t e, const float origin[3], const float angles[3], bool walkNow, float snap)
    {
        const float dx = F(base, e + 336) - origin[0], dy = F(base, e + 340) - origin[1], dz = F(base, e + 344) - origin[2];
        if (sqrtf(dx * dx + dy * dy + dz * dz) > snap) PutAt(base, e, origin, angles);
        else if (walkNow) WalkTo(ctx, base, e, origin);
    }
    // Smooth movement (0.5.230, buffered in 0.5.231; rebuilt in co-op v35).
    // Before v35 every soldier was slid toward the host's place each frame
    // while his own legs walked him there too, and his body was turned to
    // the host's facing whatever way his legs were going: the two fought
    // (the jitter), he ran sideways, and a soldier paired with a body the
    // host had lost was given the body's angles (lying down) while alive:
    // the cartwheels. Now (DriveActor):
    // - he walks with his own legs to where the host's soldier is going
    //   (the scripts' setgoalpos, 0.35 s ahead of him when he moves), so his
    //   steps and facing are his own and match his movement
    // - he is pushed only when he is off by more than a dead zone (32 units,
    //   a player's body 16), gently, faster the more he is off
    // - only more than COD3_COOP_SNAP (320) off is he put there outright
    // - a soldier's pitch and roll are never written; a player's body turns
    //   to where the player looks only while standing still
    // Things that are not soldiers (doors, gates, vehicles) are placed
    // exactly where the buffer says (DriveThing).
    struct DriveState { double frameAt = -1, walkAt = -1, seen = -1; float goal[3] = {}; uint32_t e = 0; };
    std::unordered_map<uint32_t, DriveState> g_drive;   // by this copy's entity
    void DriveActor(PPCContext& ctx, uint8_t* base, uint32_t e, const Track& track, double t, bool body, double now)
    {
        float p[3], a[3], v[3];
        if (!track.At(t, p, a, v)) return;
        // (0.5.236) a player's body keeps to the lowest height of the last
        // 0.6 s: a jump (up and down within that) leaves him on the ground,
        // as the soldier has no jump of his own to show (test 5: he floated)
        if (body) for (const Sample& s : track.s) if (s.t <= t && s.t >= t - 0.6 && s.o[2] < p[2]) p[2] = s.o[2];
        static const float snap = [] { const char* t = getenv("COD3_COOP_SNAP"); return t ? float(atof(t)) : 320.0f; }();
        DriveState& st = g_drive[e];
        const bool fresh = st.e != e || now - st.seen > 2.0;
        if (fresh) { st = DriveState{}; st.e = e; }
        st.seen = now;
        const float frame = st.frameAt < 0 ? 0.0f : float(std::clamp(now - st.frameAt, 0.0, 0.1));
        st.frameAt = now;
        float cur[3] = { F(base, e + 336), F(base, e + 340), F(base, e + 344) };
        const float d[3] = { p[0] - cur[0], p[1] - cur[1], p[2] - cur[2] };
        const float err = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        const float speed = sqrtf(v[0] * v[0] + v[1] * v[1]);
        if (fresh || err > snap)
        {
            const float yaw[3] = { 0.0f, a[1], 0.0f };
            PutAt(base, e, p, yaw);
            if (!fresh && g_snaps++ < 100) { printf("coop: move: entity %08X was %.0f off: put in place\n", e, err); fflush(stdout); }
        }
        else
        {
            const float dead = body ? 16.0f : 32.0f;
            if (err > dead && frame > 0)
            {
                const float rate = std::min(700.0f, (body ? 2.0f : 1.25f) * speed + (err - dead) * (body ? 6.0f : 3.0f) + 40.0f);
                const float step = std::min(err - dead * 0.5f, rate * frame);
                for (int k = 0; k < 3; k++) cur[k] += d[k] / err * step;
                PutAt(base, e, cur, nullptr);
            }
        }
        if (now - st.walkAt >= 0.2)
        {
            float to[3] = { p[0], p[1], p[2] };
            if (speed > 20.0f) { to[0] += v[0] * 0.35f; to[1] += v[1] * 0.35f; }
            const float gx = to[0] - st.goal[0], gy = to[1] - st.goal[1], gz = to[2] - st.goal[2];
            // (and again each second while he is off: this copy's own scripts
            // may have sent him elsewhere since)
            if (st.walkAt < 0 || gx * gx + gy * gy + gz * gz > 16.0f * 16.0f || (err > (body ? 16.0f : 32.0f) && now - st.walkAt >= 1.0))
            {
                st.walkAt = now; memcpy(st.goal, to, 12);
                WalkTo(ctx, base, e, to, speed > 20.0f ? 24.0f : 12.0f);
            }
        }
        if (body && speed < 15.0f && err < 40.0f && frame > 0)
        {
            const float was = F(base, e + 356);
            const float turn = fmodf(a[1] - was + 540.0f, 360.0f) - 180.0f;
            const float most = 540.0f * frame;
            const float yaw = was + std::clamp(turn, -most, most);
            SetF(base, e + 356, yaw); SetF(base, e + 76, yaw);
        }
    }
    void DriveThing(uint8_t* base, uint32_t e, const Track& track, double t)
    {
        float p[3], a[3], v[3];
        if (track.At(t, p, a, v)) PutAt(base, e, p, a);
    }
    // (0.5.233) A body of our own for the other player: a new soldier from
    // one of the level's friendly spawners (the scripts' dospawn,
    // sub_824B9118: the spawner, a name (an empty one here), nothing), so no
    // mission script owns him, kills him or deletes him. Test 4: the
    // borrowed soldiers kept dying or being removed and the host "jumped
    // around" (8 swaps in a minute). Returns his entity number, or -1.
    // (0.5.233) a player's stance shown on his body: the stances the soldier
    // may take (the scripts' allowedstances, sub_8249D140, keeps them at
    // the actor's +656: 1 stand, 2 crouch, 4 prone) set to the one he is in,
    // so the soldier goes down or up with him.
    void SetStance(uint8_t* base, uint32_t e, uint32_t stance)
    {
        const uint32_t actor = Guest::Read32(base, e + 532);
        if (actor < 0x40000000u || actor >= 0xC0000000u) return;
        if (stance != 1 && stance != 2 && stance != 4) stance = 1;
        if (Guest::Read32(base, actor + 656) != stance) { printf("coop: body: stance -> %u\n", stance); fflush(stdout); }
        Guest::Write32(base, actor + 656, stance);
    }
    // (0.5.236) this copy's player, followed each frame: his stance and his
    // shots. Test 5 had false crouches (the view height, +184, is eased
    // between stances and dips on landing) and false shots (the weapon
    // time, +52, also moves for reloading, melee, raising...). Now:
    // - stance from the movement flags (+208: 0x20 crouched, seen in the
    //   dumps), prone from a view height of 20 or less held for 0.3 s
    //   (prone's is about 11); a change held 0.15 s before it counts
    // - a shot is a rise of the weapon time while the attack button (the
    //   command's buttons, +1352, bit 1; the dumps) is down
    uint32_t g_myStance = 1, g_myShots = 0;
    void TrackMe(const uint8_t* base, double now)
    {
        const uint32_t me = EntityAt(base, 1);
        const uint32_t rec = me ? Guest::Read32(base, me + 528) : 0;
        if (rec < 0x40000000u || rec >= 0xC0000000u) return;
        const uint32_t flags = Guest::Read32(base, rec + 208);
        const int32_t height = int32_t(Guest::Read32(base, rec + 184));
        static double lowSince = -1;
        if (height <= 20) { if (lowSince < 0) lowSince = now; } else lowSince = -1;
        const uint32_t raw = (lowSince >= 0 && now - lowSince > 0.3) ? 4 : (flags & 0x20) ? 2 : 1;
        static uint32_t pending = 1; static double pendingSince = 0;
        if (raw != pending) { pending = raw; pendingSince = now; }
        if (pending != g_myStance && now - pendingSince >= 0.15)
        {
            printf("coop: me: stance %u -> %u (flags %X, view height %d)\n", g_myStance, pending, flags, height); fflush(stdout);
            g_myStance = pending;
        }
        const int32_t t = int32_t(Guest::Read32(base, rec + 52));
        const bool attack = (Guest::Read32(base, rec + 1352) & 1) != 0;
        // DIAGNOSTIC (COD3_COOP_SHOTLOG=1): the weapon time, the buttons and
        // the pad's trigger whenever one changes, to see a quick click
        static const bool shotLog = [] { const char* v = getenv("COD3_COOP_SHOTLOG"); return v && v[0] == '1'; }();
        if (shotLog)
        {
            static uint32_t was[5] = {};
            const uint32_t is[5] = { uint32_t(t), Guest::Read32(base, rec + 1352), Input::State().rightTrigger, Guest::Read32(base, rec + 48), Guest::Read32(base, rec + 56) };
            if (memcmp(was, is, sizeof(is)) != 0)
            {
                memcpy(was, is, sizeof(is));
                static int told = 0;
                if (told++ < 40000) { printf("coop: shotlog %.3f t %d buttons %X trigger %u +48 %X +56 %X\n", now, t, is[1], is[2], is[3], is[4]); fflush(stdout); }
            }
        }
        static int32_t last = 0;
        if (t > last + 40 && t <= 1200 && attack)   // (a reload with the trigger held set it to 1900 in the one-PC test)
        {
            g_myShots++;
            static int told = 0;
            if (told++ < 200) { printf("coop: me: shot %u (weapon time %d -> %d)\n", g_myShots, last, t); fflush(stdout); }
        }
        last = t;
    }
    // (0.5.237) Grenades. A thrown grenade is an entity of type 3, class
    // 'grenade' or 'smoke_grenade' (the watch run: it flew about 0.7 s, lay
    // still, went off about 5 s after it was thrown; +0x320 its weapon: 3 the
    // frag, 7 the smoke). Once it lies still (0.15 s) its place is sent and
    // the other copy has its thrower there throw one to it (the scripts'
    // magicgrenade, sub_824B27A8: the actor, from, to, f1 the fuse; it takes
    // the weapon from the actor's +2564, set to the thrown one for it), so it
    // flies, lands and goes off there too, its damage that copy's own.
    // (co-op v35) CWG2: an id (sent twice, taken once), the thrower (the
    // sender's entity number; 1 is the sender's player), kind, place, weapon,
    // fuse. The host sends every grenade, the soldiers' too (the joining
    // player saw almost none: only the players' were sent, and the joining
    // copy's soldiers threw their own); the joining copy only its player's.
    // A grenade must have flown 40 units to count (test 6: one smoke lying
    // on the ground was taken for a new throw three times).
    struct MyNade { double firstAt = 0, stillSince = -1; float first[3] = {}, last[3] = {}; bool sent = false; uint32_t kind = 1, weapon = 0; uint16_t thrower = 1; };
    std::vector<uint8_t> g_nadeOut;   // CWG2 messages waiting to be sent (32 bytes each)
    // the grenades thrown here for the other copy (RemoteGrenade): never sent
    // back as if they were ours (each would go round and round)
    std::set<uint32_t> g_madeHere;
    void TrackGrenades(const uint8_t* base, double now)
    {
        static std::map<uint32_t, MyNade> mine;
        static std::set<uint32_t> seen;
        static uint32_t gen = 0;
        static uint32_t nextId = 1;
        if (gen != g_mapGen) { gen = g_mapGen; mine.clear(); seen.clear(); }
        const uint32_t me = EntityAt(base, 1);
        if (!me || !Guest::Read32(base, me + 528)) return;
        const bool everyone = CoopMode() == Mode::Host;
        std::set<uint32_t> present;
        for (uint32_t i = 2; i < 4096; i++)
        {
            const uint32_t e = EntityAt(base, i);
            if (!e || base[e] != 3) continue;
            present.insert(e);
            if (seen.count(e)) continue;
            seen.insert(e);
            if (g_madeHere.count(e)) continue;
            const std::string c = FieldText(base, e, 564);
            if (c != "'grenade'" && c != "'smoke_grenade'") continue;
            // its thrower: the nearest soldier or player beside it when it appears
            uint32_t thrower = 0; float best = 150.0f * 150.0f;
            for (uint32_t n = 1; n < 4096; n++)
            {
                if (!everyone && n != 1) break;
                const uint32_t s = EntityAt(base, n);
                if (!s || !(Guest::Read32(base, s + 532) || Guest::Read32(base, s + 528))) continue;
                const float dx = F(base, e + 336) - F(base, s + 336), dy = F(base, e + 340) - F(base, s + 340), dz = F(base, e + 344) - F(base, s + 344);
                if (dz < -40 || dz > 140) continue;
                if (dx * dx + dy * dy < best) { best = dx * dx + dy * dy; thrower = n; }
            }
            if (!thrower) continue;
            MyNade n; n.firstAt = now; n.kind = c == "'smoke_grenade'" ? 2 : 1; n.weapon = Guest::Read32(base, e + 0x320); n.thrower = uint16_t(thrower);
            for (int k = 0; k < 3; k++) n.first[k] = n.last[k] = F(base, e + 336 + 4 * k);
            mine[e] = n;
            if (thrower == 1) { printf("coop: me: threw a %s (weapon %u)\n", n.kind == 2 ? "smoke grenade" : "grenade", n.weapon); fflush(stdout); }
        }
        for (auto it = mine.begin(); it != mine.end();)
        {
            MyNade& n = it->second;
            const bool gone = !present.count(it->first);
            if (!n.sent && !gone)
            {
                float p[3]; for (int k = 0; k < 3; k++) p[k] = F(base, it->first + 336 + 4 * k);
                const float moved = fabsf(p[0] - n.last[0]) + fabsf(p[1] - n.last[1]) + fabsf(p[2] - n.last[2]);
                memcpy(n.last, p, 12);
                if (moved < 1.0f) { if (n.stillSince < 0) n.stillSince = now; } else n.stillSince = -1;
            }
            if (!n.sent && (gone || (n.stillSince >= 0 && now - n.stillSince >= 0.15) || now - n.firstAt > 2.5))
            {
                n.sent = true;
                const float fx = n.last[0] - n.first[0], fy = n.last[1] - n.first[1], fz = n.last[2] - n.first[2];
                if (fx * fx + fy * fy + fz * fz >= 40.0f * 40.0f)
                {
                    const float fuse = n.kind == 2 ? 0.5f : std::max(0.4f, 5.0f - float(now - n.firstAt));
                    const uint32_t id = nextId++;
                    const uint16_t kind = uint16_t(n.kind);
                    uint8_t m[32]; memcpy(m, "CWG2", 4); memcpy(m + 4, &id, 4); memcpy(m + 8, &n.thrower, 2); memcpy(m + 10, &kind, 2);
                    memcpy(m + 12, n.last, 12); memcpy(m + 24, &n.weapon, 4); memcpy(m + 28, &fuse, 4);
                    for (int k = 0; k < 2; k++) g_nadeOut.insert(g_nadeOut.end(), m, m + 32);
                    static int told = 0;
                    if (n.thrower == 1 || told++ < 40)
                    {
                        printf("coop: %s %s landed at %.0f %.0f %.0f: sent (fuse %.1f)\n", n.thrower == 1 ? "me: my" : "world: a soldier's",
                               n.kind == 2 ? "smoke grenade" : "grenade", n.last[0], n.last[1], n.last[2], fuse);
                        fflush(stdout);
                    }
                }
            }
            if (gone) it = mine.erase(it); else ++it;
        }
        for (auto it = g_madeHere.begin(); it != g_madeHere.end();) { if (!present.count(*it)) { seen.erase(*it); it = g_madeHere.erase(it); } else ++it; }
    }
    // A grenade of the other copy's, thrown here by the soldier at bodyIndex.
    // (co-op v35) Test 6: 4 of 13 were "NOT thrown": magicgrenade found no
    // arc from the hand to the place (its three tries, sub_823D6DB0,
    // sub_823D6EB0 and sub_823D85B8, all failed). Now it is tried again from
    // higher, from halfway, and last dropped from just above the place.
    bool RemoteGrenade(PPCContext& ctx, uint8_t* base, int32_t bodyIndex, const float to[3], uint32_t kind, uint32_t weapon, float fuse)
    {
        static uint32_t vec = 0;
        if (!vec) vec = Guest::AllocatePhysical(32);
        const uint32_t e = bodyIndex >= 0 ? EntityAt(base, uint32_t(bodyIndex)) : 0;
        const uint32_t actor = e ? Guest::Read32(base, e + 532) : 0;
        if (!vec || actor < 0x40000000u || actor >= 0xC0000000u) { printf("coop: body: no soldier to throw the other copy's grenade\n"); fflush(stdout); return false; }
        const float at[3] = { F(base, e + 336), F(base, e + 340), F(base, e + 344) };
        const float tries[4][3] = {
            { at[0], at[1], at[2] + 64.0f },
            { at[0], at[1], at[2] + 110.0f },
            { (at[0] + to[0]) * 0.5f, (at[1] + to[1]) * 0.5f, std::max(at[2], to[2]) + 120.0f },
            { to[0], to[1], to[2] + 48.0f },
        };
        const uint32_t was = Guest::Read32(base, actor + 2564);
        if (weapon > 0 && weapon < 1024) Guest::Write32(base, actor + 2564, weapon);
        auto nades = [&] { std::set<uint32_t> n; for (uint32_t i = 2; i < 4096; i++) if (const uint32_t g = EntityAt(base, i); g && base[g] == 3) n.insert(g); return n; };
        int used = -1;
        for (int k = 0; k < 4 && used < 0; k++)
        {
            for (int j = 0; j < 3; j++) { SetF(base, vec + 4 * j, tries[k][j]); SetF(base, vec + 16 + 4 * j, to[j]); }
            const std::set<uint32_t> before = nades();
            PPCContext call = ctx;
            call.r1.u32 = (call.r1.u32 - 0x400) & ~0xFu;
            call.r3.u64 = uint32_t(bodyIndex) | (Guest::Read32(base, EntityTable + uint32_t(bodyIndex) * 8 + 4) << 14);
            call.r4.u64 = vec; call.r5.u64 = vec + 16;
            call.f1.f64 = std::clamp(double(fuse), 0.1, 10.0);
            call.lr = 0;
            Guest::Lookup(0x824B27A8)(call, base);
            for (const uint32_t g : nades()) if (!before.count(g)) { g_madeHere.insert(g); used = k; }
        }
        Guest::Write32(base, actor + 2564, was);
        printf("coop: body: %d threw the other copy's %s to %.0f %.0f %.0f (weapon %u, fuse %.1f): %s (try %d)\n", bodyIndex,
               kind == 2 ? "smoke grenade" : "grenade", to[0], to[1], to[2], weapon, fuse, used >= 0 ? "in the air" : "NOT thrown", used + 1);
        fflush(stdout);
        return used >= 0;
    }
    // CWG2 in: each is sent twice, taken once
    bool NewGrenadeId(uint32_t id)
    {
        static std::deque<uint32_t> ids;
        if (std::find(ids.begin(), ids.end(), id) != ids.end()) return false;
        ids.push_back(id);
        if (ids.size() > 64) ids.pop_front();
        return true;
    }
    uint32_t MyStance(const uint8_t*) { return g_myStance; }
    uint32_t MyShots(const uint8_t*) { return g_myShots; }
    // a soldier fires his weapon once (the scripts' shoot, sub_824B8FF8)
    void Shoot(PPCContext& ctx, uint8_t* base, uint32_t index)
    {
        PPCContext call = ctx;
        call.r1.u32 = (call.r1.u32 - 0x400) & ~0xFu;
        call.r3.u64 = index | (Guest::Read32(base, EntityTable + index * 8 + 4) << 14);
        call.lr = 0;
        Guest::Lookup(0x824B8FF8)(call, base);
    }
    // the other player's shots shown: his body fires as many times as he did
    // (at most one a frame; a backlog of more than 5 is dropped)
    void ShowShots(PPCContext& ctx, uint8_t* base, int32_t index, uint32_t theirs, uint32_t& shown)
    {
        if (index < 0) { shown = theirs; return; }
        if (theirs - shown > 5) shown = theirs - 1;
        if (shown != theirs) { shown++; Shoot(ctx, base, uint32_t(index)); static int told = 0; if (told++ < 200) { printf("coop: body: fired (their shot %u)\n", shown); fflush(stdout); } }
    }
    int32_t MakeBody(PPCContext& ctx, uint8_t* base, uint32_t avoid)
    {
        if (!g_scratch) return -1;
        uint32_t spawner = 0xFFFFFFFF;
        for (int pass = 0; pass < 2 && spawner == 0xFFFFFFFF; pass++)
            for (uint32_t i = 0; i < 4096; i++)
                if (const uint32_t e = EntityAt(base, i); e && base[e] == 12)
                {
                    const std::string c = FieldText(base, e, 564);
                    if (c.rfind("'actor_ally_", 0) != 0) continue;
                    if (pass == 0 && c.find("_reg") == std::string::npos) continue;
                    spawner = i; break;
                }
        if (spawner == 0xFFFFFFFF) return -1;
        std::set<uint32_t> before;
        for (uint32_t i = 0; i < 4096; i++) if (const uint32_t e = EntityAt(base, i); e && Guest::Read32(base, e + 532)) before.insert(e);
        Guest::Write32(base, g_scratch + 48, 0);   // the empty name
        PPCContext call = ctx;
        call.r1.u32 = (call.r1.u32 - 0x400) & ~0xFu;
        call.r3.u64 = spawner | (Guest::Read32(base, EntityTable + spawner * 8 + 4) << 14);
        call.r4.u64 = g_scratch + 48; call.r5.u64 = 0;
        call.lr = 0;
        Guest::Lookup(0x824B9118)(call, base);
        for (uint32_t i = 0; i < 4096; i++)
            if (const uint32_t e = EntityAt(base, i); e && e != avoid && Guest::Read32(base, e + 532) && !before.count(e))
            {
                printf("coop: players: a soldier of our own (%u, from spawner %u %s) for the other player\n", i, spawner, FieldText(base, EntityAt(base, spawner), 564).c_str());
                fflush(stdout);
                return int32_t(i);
            }
        return -1;
    }
    void HideBelow(uint8_t* base, uint32_t e)
    {
        const float o[3] = { F(base, e + 336), F(base, e + 340), -20000.0f };
        PutAt(base, e, o, nullptr);
        Guest::Write32(base, e + 624, 0);   // takes no damage
    }
    // Damage dealt the way the scripts' dodamage does it (sub_820B2720: the
    // entity, a point, a direction, the amount), from a place: it falls or
    // flinches as the game makes it. Its takedamage (+624) is let on for it.
    void DoDamage(PPCContext& ctx, uint8_t* base, uint32_t e, float amount, const float from[3])
    {
        const uint32_t ref = EntityRef(base, e);
        if (!g_scratch || ref == 0xFFFFFFFF || amount <= 0) return;
        float p[3] = { F(base, e + 336), F(base, e + 340), F(base, e + 344) + 40.0f };
        float d[3] = { p[0] - from[0], p[1] - from[1], p[2] - from[2] };
        const float n = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        if (n < 0.01f) { d[0] = 0; d[1] = 0; d[2] = 1; } else for (float& v : d) v /= n;
        for (int k = 0; k < 3; k++) { SetF(base, g_scratch + 16 + 4 * k, p[k]); SetF(base, g_scratch + 32 + 4 * k, d[k]); }
        const uint32_t was = Guest::Read32(base, e + 624);
        Guest::Write32(base, e + 624, 1);
        PPCContext call = ctx;
        call.r1.u32 = (call.r1.u32 - 0x400) & ~0xFu;
        call.r3.u64 = ref; call.r4.u64 = g_scratch + 16; call.r5.u64 = g_scratch + 32;
        call.f1.f64 = amount; call.lr = 0;
        Guest::Lookup(0x820B2720)(call, base);
        if (int32_t(Guest::Read32(base, e + 776)) > 0) Guest::Write32(base, e + 624, was);
    }
    sockaddr_in g_hostFrom{};
    double g_downAt = -1;   // when the joining player was killed (-1: alive)
    // ---- the lobby: the joining copy follows the host into his mission ----
    // Both start at the main menu. The joining copy says hello (CWJ1, each
    // second, to port + 3); the host answers with the mission he is in (CWL1
    // and its name, each second, to port + 2). When the host starts one
    // (SV_SpawnServer, sub_82517A00: its name in r3), the joining copy starts
    // the same, as the chapter select does ("spmap name").
    std::string g_myMap;            // the mission this copy last started
    double g_peerSeen = -1;         // when the other copy was last heard
    bool g_peerToldConnected = false;
    float g_hostPlace[6] = {};   // the host player's place, on the joining copy
    bool g_haveHostPlace = false;
    // The joining player put back beside the host, at his full health
    // (maxhealth +780): his record's origin (+0)
    // and the entity's copies of it.
    void Respawn(uint8_t* base, uint32_t me)
    {
        const uint32_t rec = Guest::Read32(base, me + 528);
        if (rec < 0x40000000u || !g_haveHostPlace) return;
        const float yaw = g_hostPlace[4] * 0.0174533f;
        const float o[3] = { g_hostPlace[0] - 60.0f * sinf(yaw), g_hostPlace[1] + 60.0f * cosf(yaw), g_hostPlace[2] + 8.0f };
        for (int k = 0; k < 3; k++) SetF(base, rec + 4 * k, o[k]);
        for (const uint32_t at : { 32u, 336u }) for (int k = 0; k < 3; k++) SetF(base, me + at + 4 * k, o[k]);
        const int32_t full = int32_t(Guest::Read32(base, me + 780));
        Guest::Write32(base, me + 776, uint32_t(full > 0 ? full : 100));
        printf("coop: players: back in, beside the host at %.0f %.0f %.0f (health %d)\n", o[0], o[1], o[2], int32_t(Guest::Read32(base, me + 776)));
        fflush(stdout);
    }   // where the host's world comes from (the joining copy answers there)
    bool IsRifleman(const uint8_t* base, uint32_t e)
    {
        if (!Guest::Read32(base, e + 532)) return false;
        const int32_t health = int32_t(Guest::Read32(base, e + 776));
        const std::string c = FieldText(base, e, 564);
        // Any ordinary friendly soldier ("..._reg..." in the class name: Forest's
        // actor_ally_amer_reg_*, Saint-Lo's actor_ally_stlo_amer_reg_*, ...). After
        // 30 s with none, any friendly soldier at all (COD3_COOP_ANYALLY=0 turns
        // that off). The first two-PC test was on Saint-Lo, where the old
        // Forest-only test found nobody, so neither player could see the other.
        if (!(health > 0 && health < 1000) || c.rfind("'actor_ally_", 0) != 0) return false;
        if (FieldText(base, e, 572) == "'delete_me'") return false;   // the scripts are about to remove him
        // (0.5.230) one with a script path to follow (+580, his target) is
        // taken only after 10 s with none free: Saint-Lo's first pick was a
        // patroller the scripts later deleted, and his stand-in went with him
        if (const std::string t = FieldText(base, e, 580); t != "#0" && t != "''" && !g_standinAnyTarget) return false;
        if (c.find("_reg") != std::string::npos) return true;
        // (co-op v35) off unless COD3_COOP_ANYALLY=1: test 6 borrowed Dixon,
        // and a named one the story needs must not be a stand-in
        static const bool anyAlly = [] { const char* t = getenv("COD3_COOP_ANYALLY"); return t && t[0] == '1'; }();
        return anyAlly && g_standinFallback;
    }
    // Every soldier and the player, with its key and place (at a screenshot).
    void ListSoldiers(uint8_t* base)
    {
        for (uint32_t i = 0; i < 4096; i++)
        {
            const uint32_t e = EntityAt(base, i);
            if (!e || !(Guest::Read32(base, e + 532) || Guest::Read32(base, e + 528))) continue;
            printf("coop: soldier %u key %08X health %d at %.0f %.0f %.0f (+32: %.0f %.0f %.0f) yaw %.0f\n", i, EntityKey(base, e), int32_t(Guest::Read32(base, e + 776)),
                   F(base, e + 336), F(base, e + 340), F(base, e + 344), F(base, e + 32), F(base, e + 36), F(base, e + 40), F(base, e + 356));
        }
    }
    void WorldSync(PPCContext& ctx, uint8_t* base)
    {
        const Mode mode = CoopMode();
        if (mode == Mode::Off) return;
        // (0.5.236) a process the title relaunched for its next mission writes
        // its messages to coop-relaunch.log beside the game (they were lost)
        {
            static bool done = false;
            if (!done) { done = true; if (getenv("COD3_LAUNCHDATA")) { freopen("coop-relaunch.log", "a", stdout); printf("\ncoop: ---- relaunched for the next mission ----\n"); fflush(stdout); } }
        }
        if (!OpenWorld()) return;
        const double now = Seconds();
        TrackMe(base, now);
        TrackGrenades(base, now);
        if (mode == Mode::Host)
        {
            static double probeAt[2] = { -1, -1 };
            if (probeAt[0] < 0) { const char* t = getenv("COD3_COOP_PROBE"); probeAt[0] = t ? atof(t) : 150; probeAt[1] = probeAt[0] + 50; }
            for (double& p : probeAt) if (p > 0 && now >= p) { p = 0; Probe(base); }
            if (!g_scratch) g_scratch = Guest::AllocatePhysical(64);
            // DIAGNOSTIC (COD3_COOP_WATCHHEALTH=1): the first living German's
            // health watched, to find the game's damage function by who writes it.
            if (getenv("COD3_COOP_WATCHHEALTH"))
            {
                // the first soldier seen losing health (not dying) is watched,
                // so his next wound shows the writer
                static std::map<uint32_t, int32_t> was;
                static uint32_t watching = 0;
                for (uint32_t i = 0; i < 4096; i++)
                {
                    const uint32_t e = EntityAt(base, i);
                    if (!e || !Guest::Read32(base, e + 532)) continue;
                    const int32_t h = int32_t(Guest::Read32(base, e + 776));
                    auto it = was.find(e);
                    if (it != was.end() && h < it->second && h > 0 && watching != e)
                    {
                        watching = e;
                        printf("coop: soldier %u (%s) lost health %d -> %d: watching it (%08X)\n", i, FieldText(base, e, 564).c_str(), it->second, h, e + 776);
                        fflush(stdout);
                        // (armed from another thread: a thread cannot set its own debug registers)
                        const uint32_t address = e + 776;
                        std::thread([address] { Kernel::WatchWrite(address, false); Kernel::ArmWatchpoints(); }).detach();
                    }
                    was[e] = h;
                }
            }
            // the joining player's place (port + 3)
            {
                static SOCKET in = INVALID_SOCKET;
                static double inTried = -10;
                if (in == INVALID_SOCKET && now - inTried >= 1)
                {
                    inTried = now;
                    in = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                    u_long nb = 1; ioctlsocket(in, FIONBIO, &nb);
                    sockaddr_in any{}; any.sin_family = AF_INET; any.sin_port = htons(uint16_t(Port() + 3)); any.sin_addr.s_addr = htonl(INADDR_ANY);
                    if (bind(in, reinterpret_cast<sockaddr*>(&any), sizeof(any)) != 0)
                    { printf("coop: could not listen on port %d yet\n", Port() + 3); fflush(stdout); closesocket(in); in = INVALID_SOCKET; }
                }
                uint8_t m[64];
                for (int n = 0; n < 32; n++)
                {
                    const int got = recv(in, reinterpret_cast<char*>(m), sizeof m, 0);
                    if (got < 4) break;
                    // the joining player hit one of the soldiers: the host's takes it
                    if (got == 12 && memcmp(m, "CWD1", 4) == 0)
                    {
                        uint16_t index; int32_t amount;
                        memcpy(&index, m + 4, 2); memcpy(&amount, m + 8, 4);
                        if (const uint32_t e = EntityAt(base, index); e && Guest::Read32(base, e + 532) && int32_t(Guest::Read32(base, e + 776)) > 0 &&
                            FieldText(base, e, 564).rfind("'actor_axis", 0) == 0 && amount > 0 && amount <= 1000)
                        {
                            const float eye[3] = { g_joinerPlace[0], g_joinerPlace[1], g_joinerPlace[2] + 60.0f };
                            DoDamage(ctx, base, e, float(amount), eye);
                            printf("coop: players: the joining player hit soldier %u (%s) for %d: health now %d\n", index, FieldText(base, e, 564).c_str(), amount, int32_t(Guest::Read32(base, e + 776)));
                            fflush(stdout);
                        }
                        continue;
                    }
                    if (got == 4 && memcmp(m, "CWJ1", 4) == 0)
                    {
                        g_peerSeen = now;
                        if (!g_peerToldConnected)
                        {
                            g_peerToldConnected = true;
                            printf("coop: lobby: the joining player is connected\n"); fflush(stdout);
                            Overlay::Notice("Co-op: your friend is connected. Pick a mission and they will follow.", 6000);
                        }
                        continue;
                    }
                    // the joining player's grenade: his stand-in throws it here
                    // (co-op v35: and his own grenade cannot hurt him till it
                    // is over; test 6: 586, 449 and 821 taken from his own smoke)
                    if (got == 32 && memcmp(m, "CWG2", 4) == 0)
                    {
                        uint32_t id, weapon; uint16_t kind; float to[3], fuse;
                        memcpy(&id, m + 4, 4); memcpy(&kind, m + 10, 2); memcpy(to, m + 12, 12); memcpy(&weapon, m + 24, 4); memcpy(&fuse, m + 28, 4);
                        if (NewGrenadeId(id) && g_avatarA >= 0)
                        {
                            g_standinShield = now + std::clamp(double(fuse), 0.5, 6.0) + 1.0;
                            RemoteGrenade(ctx, base, g_avatarA, to, kind, weapon, fuse);
                        }
                        continue;
                    }
                    // (co-op v36) an older joining copy (test 7: 0.5.276 against 0.5.314) sends
                    // 36-byte CWP1 and 28-byte CWG1 and cannot read CWS2/CWG2: it sees none of
                    // the host's world, and its grenades are not taken here. Said, not hidden.
                    if ((got == 28 && memcmp(m, "CWG1", 4) == 0) || (got != 44 && memcmp(m, "CWP1", 4) == 0))
                    {
                        static bool told = false;
                        if (!told) { told = true; printf("coop: players: the joining copy runs an older co-op build (%s, %d bytes): both PCs need the same CoD3.exe\n", got == 28 && m[3] == 'G' ? "CWG1" : "CWP1", got); fflush(stdout); }
                        static double noticeAt = -100;
                        if (now - noticeAt >= 30) { noticeAt = now; Overlay::Notice("Co-op: your friend's game is an older build: they cannot see your world. Both need the same CoD3.exe.", 10000); }
                    }
                    if ((got != 28 && got != 32 && got != 36 && got != 44) || memcmp(m, "CWP1", 4) != 0) continue;
                    memcpy(g_joinerPlace, m + 4, 24);
                    if (got >= 32) memcpy(&g_joinerStance, m + 28, 4);
                    if (got >= 36) { memcpy(&g_joinerShots, m + 32, 4); g_haveJoinerShots = true; }
                    // (co-op v35) the joining copy's clock with it (44 bytes)
                    {
                        double t = now;
                        if (got >= 44) { memcpy(&t, m + 36, 8); g_joinerClock.Feed(now, t); }
                        else g_joinerClock.Feed(now, now);
                        g_joinerTrack.Add(t, g_joinerPlace, g_joinerPlace + 3);
                    }
                    if (g_joinerSeen < 0) { printf("coop: players: the joining player is in, at %.0f %.0f %.0f\n", g_joinerPlace[0], g_joinerPlace[1], g_joinerPlace[2]); fflush(stdout); }
                    g_joinerSeen = now;
                }
            }
            // the lobby beacon: the mission the host is in, each second
            {
                static double lastBeacon = 0;
                if (now - lastBeacon >= 1.0)
                {
                    lastBeacon = now;
                    uint8_t m[72] = {}; memcpy(m, "CWL1", 4);
                    memcpy(m + 4, g_myMap.c_str(), std::min<size_t>(g_myMap.size(), 63));
                    memcpy(m + 68, &g_hostEpoch, 4);
                    sendto(g_world, reinterpret_cast<const char*>(m), 72, 0, reinterpret_cast<sockaddr*>(&g_worldTo), sizeof(g_worldTo));
                }
                // (0.5.230) COD3_COOP_MAP=name (the host launcher asks for it):
                // that mission is started 3 s after the friend connects, the way
                // the chapter select does, so a mission can be chosen that the
                // menu has not unlocked, or a broken part skipped
                {
                    // (not in a process the title relaunched for its next mission: that one
                    // goes where its launch data says, COD3_LAUNCHDATA)
                    static const char* const want = getenv("COD3_LAUNCHDATA") ? nullptr : getenv("COD3_COOP_MAP");
                    static double connectedAt = -1;
                    static bool started = false;
                    if (g_peerToldConnected && connectedAt < 0) connectedAt = now;
                    if (want && want[0] && !started && g_myMap.empty() && connectedAt >= 0 && now - connectedAt > 3)
                    {
                        started = true;
                        printf("coop: lobby: starting %s (COD3_COOP_MAP)\n", want); fflush(stdout);
                        Overlay::Notice(std::string("Co-op: starting ") + want + "...", 5000);
                        Kernel::QueueConsoleCommand(std::string("spmap ") + want);
                    }
                }
                if (g_peerToldConnected && now - g_peerSeen > 90)   // (a mission loading runs no frames for a minute or more)
                {
                    g_peerToldConnected = false;
                    printf("coop: lobby: the joining player is gone\n"); fflush(stdout);
                    Overlay::Notice("Co-op: your friend has disconnected.", 6000);
                }
            }
            // (0.5.230) the mission restarted here (mission failed, then
            // retry / checkpoint): the host player back alive after being
            // dead. The joining copy is
            // told (the epoch in CWL1) and starts the mission again too.
            {
                const uint32_t me = EntityAt(base, 1);
                const bool inMission = me && Guest::Read32(base, me + 528);
                const int32_t health = inMission ? int32_t(Guest::Read32(base, me + 776)) : 0;
                static bool wasDead = false;
                static int lastCount = 0;
                static uint32_t gen = 0;
                if (gen != g_mapGen || !inMission) { gen = g_mapGen; lastCount = 0; if (!inMission) wasDead = false; }
                int count = 0;
                for (uint32_t i = 0; i < 4096; i++) if (EntityAt(base, i)) count++;
                bool restarted = false;
                if (inMission && health <= 0) wasDead = true;
                else if (inMission && wasDead) { wasDead = false; restarted = true; }
                // (0.5.233: no longer "most of the world gone at once": that is also the
                // mission ending, test 4, and sent the joining copy back to its start)
                lastCount = inMission ? count : 0;
                if (restarted)
                {
                    g_hostEpoch++;
                    g_avatarA = g_avatarB = -1;
                    printf("coop: lobby: the mission restarted here (%d entities): the joining copy restarts too\n", count); fflush(stdout);
                }
                // (co-op v36) the host is playing, so no film of his is on: said each
                // second (CWF1 with his count). Test 7: he died, came back at a
                // checkpoint with no film, and the joining copy, started again from
                // the mission's start, sat in its film waiting for his to end.
                static double filmOverSaid = -10;
                if (inMission && health > 0 && now - filmOverSaid >= 1.0) { filmOverSaid = now; FilmSend("CWF1"); }
            }
            // The joining player's body here (A): a soldier of our own
            // (0.5.233, MakeBody), or, when the level has no friendly spawner
            // left after 10 tries, one of the level's riflemen (as before).
            // Gone (deleted, killed, or about to be deleted): replaced.
            if (g_avatarA >= 0)
            {
                const uint32_t e = EntityAt(base, uint32_t(g_avatarA));
                if (!e || !Guest::Read32(base, e + 532) || int32_t(Guest::Read32(base, e + 776)) <= 0 || FieldText(base, e, 572) == "'delete_me'")
                {
                    printf("coop: players: stand-in %d is gone: choosing another\n", g_avatarA); fflush(stdout);
                    g_avatarA = -1;
                }
            }
            if (g_avatarA < 0 && g_joinerSeen >= 0)
            {
                static double lastLook = 0, firstLook = 0;
                static int tries = 0;
                static uint32_t gen = 0;
                bool anyActor = false;
                for (uint32_t i = 2; i < 4096 && !anyActor; i++) if (const uint32_t e = EntityAt(base, i); e && Guest::Read32(base, e + 532)) anyActor = true;
                if (gen != g_mapGen || !anyActor) { gen = g_mapGen; firstLook = now; tries = 0; g_standinAnyTarget = false; g_standinFallback = false; }
                if (!g_standinAnyTarget && now - firstLook > 10) g_standinAnyTarget = true;
                if (!g_standinFallback && now - firstLook > 30) g_standinFallback = true;
                static const bool borrow = getenv("COD3_COOP_BORROW") != nullptr;
                if (anyActor && now - lastLook > 2)
                {
                    lastLook = now;
                    // (co-op v35) 30 tries (a minute), not 10: test 6 borrowed
                    // after 20 s, then Dixon after a restart
                    if (!borrow && tries < 30)
                    {
                        tries++;
                        if (const int32_t made = MakeBody(ctx, base, 0); made >= 0) { g_avatarA = made; g_avatarSpawned = true; }
                        else if (tries == 30) { printf("coop: players: no friendly spawner made a soldier: borrowing one of the level's\n"); fflush(stdout); }
                    }
                    else
                        for (uint32_t i = 0; i < 4096 && g_avatarA < 0; i++)
                            if (const uint32_t e = EntityAt(base, i); e && IsRifleman(base, e))
                            {
                                g_avatarA = int32_t(i); g_avatarSpawned = false;
                                printf("coop: players: soldier %u (%s) stands in for the joining player here\n", i, FieldText(base, e, 564).c_str());
                                fflush(stdout);
                            }
                }
            }
            if (g_avatarA >= 0 && g_joinerSeen >= 0)
                if (const uint32_t e = EntityAt(base, uint32_t(g_avatarA)))
                {
                    // He is hit for the joining player: whatever he loses goes to the
                    // joining copy (CWH1), and he is kept far from dying here.
                    // (0.5.236) Not by the host himself: while the host attacks
                    // (any of the command's low buttons, +1352) toward him
                    // (within 30 degrees) or beside him (120 units), he takes
                    // nothing, so it is no friendly fire (test 5: a melee on him
                    // failed the mission and the two copies came apart).
                    bool fromHost = false;
                    if (const uint32_t me = EntityAt(base, 1); me)
                        if (const uint32_t rec = Guest::Read32(base, me + 528); rec >= 0x40000000u && rec < 0xC0000000u && (Guest::Read32(base, rec + 1352) & 0xFF))
                        {
                            float to[3] = { F(base, e + 336) - F(base, me + 336), F(base, e + 340) - F(base, me + 340), F(base, e + 344) - F(base, me + 344) };
                            const float n = sqrtf(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]);
                            const float pitch = F(base, rec + 172) * 0.0174533f, yaw = F(base, rec + 176) * 0.0174533f;
                            const float aim[3] = { cosf(pitch) * cosf(yaw), cosf(pitch) * sinf(yaw), -sinf(pitch) };
                            fromHost = n < 120.0f || (n > 1 && (to[0] * aim[0] + to[1] * aim[1] + to[2] * aim[2]) / n > 0.866f);
                        }
                    Guest::Write32(base, e + 624, fromHost ? 0 : 1);
                    const int32_t h = int32_t(Guest::Read32(base, e + 776));
                    static uint32_t ready = 0;   // the stand-in given his 100000 once, before any is counted
                    if (ready != e) { ready = e; if (h > 0) Guest::Write32(base, e + 776, 100000); }
                    else if (h > 0 && h < 100000 && 100000 - h <= 1000)
                    {
                        // (co-op v35) not while his own grenade is out (it is
                        // thrown from him), and never more than 250 at once
                        const int32_t lost = std::min(100000 - h, 250);
                        if (now < g_standinShield) { static int told = 0; if (told++ < 20) { printf("coop: players: the stand-in's own grenade hit him for %d: not sent\n", 100000 - h); fflush(stdout); } }
                        else
                        {
                            uint8_t m[8]; memcpy(m, "CWH1", 4); memcpy(m + 4, &lost, 4);
                            sendto(g_world, reinterpret_cast<const char*>(m), 8, 0, reinterpret_cast<sockaddr*>(&g_worldTo), sizeof(g_worldTo));
                            printf("coop: players: the joining player's stand-in was hit for %d: sent to the joining copy\n", lost);
                            fflush(stdout);
                        }
                    }
                    if (h > 0) Guest::Write32(base, e + 776, 100000);
                    DriveActor(ctx, base, e, g_joinerTrack, g_joinerClock.Show(now), true, now);
                    static StanceFilter stance;
                    SetStance(base, e, stance.Get(g_joinerStance, now));
                    static uint32_t shown = 0;
                    if (g_haveJoinerShots) ShowShots(ctx, base, g_avatarA, g_joinerShots, shown);
                }
            static double last = 0;
            if (now - last < 0.05) return;
            last = now;
            for (size_t at = 0; at + 32 <= g_nadeOut.size(); at += 32)
                sendto(g_world, reinterpret_cast<const char*>(g_nadeOut.data() + at), 32, 0, reinterpret_cast<sockaddr*>(&g_worldTo), sizeof(g_worldTo));
            g_nadeOut.clear();
            {
                uint8_t q[12]; memcpy(q, "CWQ1", 4);
                const uint32_t stance = MyStance(base), shots = MyShots(base);
                memcpy(q + 4, &stance, 4); memcpy(q + 8, &shots, 4);
                sendto(g_world, reinterpret_cast<const char*>(q), 12, 0, reinterpret_cast<sockaddr*>(&g_worldTo), sizeof(g_worldTo));
            }
            // (co-op v35) the world as CWS2 parts: the sequence, the host's
            // clock, the part and the number of parts, then up to 30 entries.
            // Only soldiers, the players and what has moved are sent (the
            // joining copy moves nothing else), so 30-60 entries, not 340.
            static uint32_t sequence = 0;
            sequence++;
            std::vector<SyncEntry> list;
            list.reserve(256);
            for (uint32_t i = 0; i < 4096; i++)
            {
                const uint32_t e = EntityAt(base, i);
                if (!e) continue;
                SyncEntry s{};
                s.index = uint16_t(i); s.type = Guest::Read8(base, e);
                s.flags = uint8_t((Guest::Read32(base, e + 532) ? 1 : 0) | (Guest::Read32(base, e + 528) ? 2 : 0));
                if (s.flags & 2) { const uint32_t st = MyStance(base); if (st == 2) s.flags |= 32; else if (st == 4) s.flags |= 128; }   // (0.5.233) the host's stance
                for (int k = 0; k < 3; k++) { s.origin[k] = F(base, e + 336 + 4 * k); s.angles[k] = F(base, e + 352 + 4 * k); }
                s.key = EntityKey(base, e);
                if (s.flags & 1)
                {
                    static std::set<uint32_t> seen;
                    if (seen.insert(e ^ s.key).second && seen.size() < 200) printf("coop: world: host soldier %u key %08X:%s\n", i, s.key, Fields(base, e).c_str());
                }
                s.health = int32_t(Guest::Read32(base, e + 776));
                // 16: it has moved since the host first saw it (only those, and
                // soldiers, are moved on the joining copy: the first two-PC test
                // dragged a piece of floor about, paired with the wrong one)
                {
                    static std::map<uint32_t, std::array<float, 3>> first;
                    static std::set<uint32_t> moved;
                    static std::map<uint32_t, std::array<float, 3>> firstAngles;
                    static std::map<uint32_t, uint32_t> wasSoldier;   // (co-op v35) entity -> its key while it was a soldier
                    static uint32_t gen = 0;
                    if (gen != g_mapGen) { gen = g_mapGen; first.clear(); moved.clear(); firstAngles.clear(); wasSoldier.clear(); }
                    auto [it, fresh] = first.emplace(e, std::array<float, 3>{ s.origin[0], s.origin[1], s.origin[2] });
                    if (!fresh && !moved.count(e) && fabsf(it->second[0] - s.origin[0]) + fabsf(it->second[1] - s.origin[1]) + fabsf(it->second[2] - s.origin[2]) > 2.0f) moved.insert(e);
                    // (0.5.233) or turned: a gate or door swings without moving (test 4:
                    // Forest's gate stayed shut on the joining copy)
                    if (!moved.count(e))
                    {
                        auto [ia, freshA] = firstAngles.emplace(e, std::array<float, 3>{ s.angles[0], s.angles[1], s.angles[2] });
                        if (!freshA && fabsf(ia->second[0] - s.angles[0]) + fabsf(ia->second[1] - s.angles[1]) + fabsf(ia->second[2] - s.angles[2]) > 1.0f) moved.insert(e);
                    }
                    if (moved.count(e)) s.flags |= 16;
                    // (co-op v35) a soldier who has lost his actor (his body)
                    // is still sent as a soldier, dead: the joining copy kills
                    // its own and leaves the body be. Before, the body was sent
                    // as a moving thing, and the joining copy's soldier paired
                    // with it, still alive, was given its lying-down angles
                    // each frame (the cartwheels) and never died (no weapon
                    // dropped there).
                    if (s.flags & 1) wasSoldier[e] = s.key;
                    else if (const auto w = wasSoldier.find(e); w != wasSoldier.end())
                    {
                        if (w->second == s.key) { s.flags |= 1; s.health = std::min(s.health, 0); }
                        else wasSoldier.erase(w);
                    }
                }
                if (int32_t(i) == g_avatarA) { s.flags |= 4; if (g_avatarSpawned) s.flags |= 64; }
                if (int32_t(i) == g_avatarB)
                {
                    s.flags |= 8;
                    if (const uint32_t me = EntityAt(base, 1); me && Guest::Read32(base, me + 528))
                        for (int k = 0; k < 3; k++) { s.origin[k] = F(base, me + 336 + 4 * k); s.angles[k] = F(base, me + 352 + 4 * k); }
                }
                if (!(s.flags & (1 | 2 | 4 | 8 | 16))) continue;
                list.push_back(s);
            }
            constexpr size_t PerPart = 30;
            const uint16_t parts = uint16_t((list.size() + PerPart - 1) / PerPart);
            for (uint16_t part = 0; part < parts; part++)
            {
                uint8_t m[20 + PerPart * sizeof(SyncEntry)];
                memcpy(m, "CWS2", 4); memcpy(m + 4, &sequence, 4); memcpy(m + 8, &now, 8); memcpy(m + 16, &part, 2); memcpy(m + 18, &parts, 2);
                const size_t from = size_t(part) * PerPart, n = std::min(PerPart, list.size() - from);
                memcpy(m + 20, list.data() + from, n * sizeof(SyncEntry));
                sendto(g_world, reinterpret_cast<const char*>(m), int(20 + n * sizeof(SyncEntry)), 0, reinterpret_cast<sockaddr*>(&g_worldTo), sizeof(g_worldTo));
            }
            static uint64_t sends = 0;
            if (++sends % 200 == 1)
            {
                printf("coop: world: sent %llu times (sequence %u, %zu entries in %u parts); the joining copy's clock: delay %.0f ms, jitter %.0f ms\n",
                       (unsigned long long)sends, sequence, list.size(), parts, g_joinerClock.Delay() * 1000.0, g_joinerClock.jitter * 1000.0);
                fflush(stdout);
            }
            return;
        }
        // the joining copy
        uint8_t packet[65536];
        for (int n = 0; n < 64; n++)
        {
            const int got = recv(g_world, reinterpret_cast<char*>(packet), sizeof packet, 0);
            if (got < 8) break;
            if ((got == 68 || got == 72) && memcmp(packet, "CWL1", 4) == 0)
            {
                g_peerSeen = now;
                if (!g_peerToldConnected)
                {
                    g_peerToldConnected = true;
                    printf("coop: lobby: connected to the host\n"); fflush(stdout);
                    Overlay::Notice("Co-op: connected to the host. Wait at the main menu: you will follow them into their mission.", 8000);
                }
                std::string map(reinterpret_cast<const char*>(packet + 4), strnlen(reinterpret_cast<const char*>(packet + 4), 63));
                static double lastAsked = -100;
                uint32_t& lastEpoch = g_lastHostEpoch;
                uint32_t epoch = 0;
                if (got >= 72) memcpy(&epoch, packet + 68, 4);
                if (lastEpoch && epoch && epoch != lastEpoch && !map.empty() && map == g_myMap)
                {
                    g_restartWanted = true;
                    printf("coop: lobby: the host restarted %s\n", map.c_str()); fflush(stdout);
                }
                if (epoch) lastEpoch = epoch;
                if (g_hostRelaunching && !map.empty())
                {
                    g_hostRelaunching = false;
                    if (map == g_myMap) g_restartWanted = true;   // (a retry: the same mission again)
                    printf("coop: lobby: the host is back, in %s\n", map.c_str()); fflush(stdout);
                }
                static bool toldLeft = false;
                if (map.empty() && !g_myMap.empty() && !toldLeft) { toldLeft = true; Overlay::Notice("Co-op: the host is not in a mission.", 6000); }
                if (!map.empty()) toldLeft = false;
                if (!map.empty() && (map != g_myMap || g_restartWanted) && now - lastAsked > 15)
                {
                    lastAsked = now;
                    g_restartWanted = false;
                    printf("coop: lobby: the host is in %s: following\n", map.c_str()); fflush(stdout);
                    Overlay::Notice("Co-op: following the host into " + map + "...", 5000);
                    g_hostWorld.clear(); g_hostSeen.clear(); g_tracks.clear(); g_lastWorldAt = -1;
                    Kernel::QueueConsoleCommand("spmap " + map);
                }
                continue;
            }
            // (co-op v35) a grenade of the host's world: his own (thrower 1, his
            // body here throws it) or a soldier's (that soldier's partner here)
            if (got == 32 && memcmp(packet, "CWG2", 4) == 0)
            {
                uint32_t id, weapon; uint16_t thrower, kind; float to[3], fuse;
                memcpy(&id, packet + 4, 4); memcpy(&thrower, packet + 8, 2); memcpy(&kind, packet + 10, 2); memcpy(to, packet + 12, 12); memcpy(&weapon, packet + 24, 4); memcpy(&fuse, packet + 28, 4);
                if (!NewGrenadeId(id)) continue;
                uint32_t who = 0;
                if (thrower == 1) who = g_hostBodyEnt;
                else if (const auto p = g_pairs.find(thrower); p != g_pairs.end() && Guest::Read32(base, p->second + 532)) who = p->second;
                if (!who) who = g_hostBodyEnt;   // (his partner is not here: the host's body throws it, from above the place if need be)
                int32_t index = -1;
                if (who) for (uint32_t i = 0; i < 4096; i++) if (EntityAt(base, i) == who) { index = int32_t(i); break; }
                if (index >= 0)
                {
                    // (this copy's soldiers keep no grenades, see below: one is lent for the throw)
                    const uint32_t actor = Guest::Read32(base, who + 532);
                    const uint32_t ammo = actor >= 0x40000000u && actor < 0xC0000000u ? Guest::Read32(base, actor + 2576) : 0;
                    if (actor >= 0x40000000u && actor < 0xC0000000u) Guest::Write32(base, actor + 2576, 1);
                    RemoteGrenade(ctx, base, index, to, kind, weapon, fuse);
                    if (actor >= 0x40000000u && actor < 0xC0000000u) Guest::Write32(base, actor + 2576, ammo);
                }
                continue;
            }
            if (got == 4 && memcmp(packet, "CWX1", 4) == 0)
            {
                if (!g_hostRelaunching) { printf("coop: lobby: the host's game is starting itself again (next mission or retry)\n"); fflush(stdout); }
                g_hostRelaunching = true;
                Overlay::Notice("Co-op: the host's game is loading the next part. Stay where you are...", 8000);
                continue;
            }
            if (got == 12 && memcmp(packet, "CWQ1", 4) == 0)
            {
                memcpy(&g_hostStance, packet + 4, 4);
                memcpy(&g_hostShots, packet + 8, 4); g_haveHostShots = true;
                continue;
            }
            if (got == 8 && memcmp(packet, "CWH1", 4) == 0)
            {
                int32_t amount; memcpy(&amount, packet + 4, 4);
                if (const uint32_t me = EntityAt(base, 1); me && Guest::Read32(base, me + 528) && amount > 0)
                {
                    // (the scripts' dodamage leaves a player alone: his health,
                    // +776, is taken down here, and at nothing he is killed the
                    // game's way, sub_82401C58)
                    const int32_t h = int32_t(Guest::Read32(base, me + 776)) - amount;
                    Guest::Write32(base, me + 776, uint32_t(h > 0 ? h : 1));
                    // Killed: the joining player is not the campaign's player,
                    // so his death does not fail the mission. He is brought
                    // back beside the host, at full health (Respawn below).
                    if (h <= 0 && g_downAt < 0) g_downAt = now;
                    printf("coop: players: hit for %d in the host's world: health now %d\n", amount, int32_t(Guest::Read32(base, me + 776)));
                    fflush(stdout);
                }
                continue;
            }
            if (memcmp(packet, "CWS1", 4) == 0)
            {
                static bool told = false;
                if (!told) { told = true; printf("coop: world: the host runs an older co-op build (CWS1): both PCs need the same CoD3.exe\n"); fflush(stdout); Overlay::Notice("Co-op: the host's game is a different build. Both need the same CoD3.exe.", 10000); }
                continue;
            }
            // (co-op v35) a part of the host's world, with his clock
            if (got >= 20 && memcmp(packet, "CWS2", 4) == 0)
            {
                double t; memcpy(&t, packet + 8, 8);
                g_hostClock.Feed(now, t);
                g_lastWorldAt = now;
                for (int at = 20; at + int(sizeof(SyncEntry)) <= got; at += sizeof(SyncEntry))
                {
                    SyncEntry s; memcpy(&s, packet + at, sizeof s);
                    g_hostWorld[s.index] = s;
                    g_hostSeen[s.index] = now;
                    Track& track = g_tracks[s.index];
                    if (track.key != s.key) { track = Track{}; track.key = s.key; }
                    track.Add(t, s.origin, s.angles);
                }
            }
        }
        // (co-op v35) what the host no longer sends is gone there (deleted,
        // or no longer moving: an entity that stopped is left where it is)
        if (g_lastWorldAt >= 0)
            for (auto it = g_hostSeen.begin(); it != g_hostSeen.end();)
            {
                if (g_lastWorldAt - it->second > 1.0) { g_hostWorld.erase(it->first); g_tracks.erase(it->first); it = g_hostSeen.erase(it); }
                else ++it;
            }
        // hello to the host each second (the lobby), and the host's going quiet
        {
            static double lastHello = 0;
            if (now - lastHello >= 1.0)
            {
                lastHello = now;
                sockaddr_in to = g_worldTo; to.sin_port = htons(uint16_t(Port() + 3));
                inet_pton(AF_INET, OtherPc().c_str(), &to.sin_addr);
                sendto(g_world, "CWJ1", 4, 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
                if (!g_peerToldConnected) { static double lastNote = -100; if (now - lastNote > 20) { lastNote = now; Overlay::Notice("Co-op: looking for the host at " + OtherPc() + "...", 4000); } }
            }
            if (g_peerToldConnected && now - g_peerSeen > 90)   // (a mission loading runs no frames for a minute or more)
            {
                g_peerToldConnected = false;
                printf("coop: lobby: the host is gone\n"); fflush(stdout);
                Overlay::Notice("Co-op: the host has disconnected.", 6000);
            }
        }
        // this copy's player's place, to the host (port + 3), twenty times a second
        {
            static double lastSent = 0;
            if (now - lastSent >= 0.05)
                if (const uint32_t me = EntityAt(base, 1); me && Guest::Read32(base, me + 528))
                {
                    lastSent = now;
                    uint8_t m[44]; memcpy(m, "CWP1", 4);
                    for (int k = 0; k < 3; k++) { const float o = F(base, me + 336 + 4 * k), a = F(base, me + 352 + 4 * k); memcpy(m + 4 + 4 * k, &o, 4); memcpy(m + 16 + 4 * k, &a, 4); }
                    const uint32_t stance = MyStance(base); memcpy(m + 28, &stance, 4);   // (0.5.233)
                    const uint32_t shots = MyShots(base); memcpy(m + 32, &shots, 4);
                    memcpy(m + 36, &now, 8);   // (co-op v35) this copy's clock
                    sockaddr_in to = g_worldTo; to.sin_port = htons(uint16_t(Port() + 3));
                    inet_pton(AF_INET, OtherPc().c_str(), &to.sin_addr);
                    sendto(g_world, reinterpret_cast<const char*>(m), 44, 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
                    for (size_t at = 0; at + 32 <= g_nadeOut.size(); at += 32)
                        sendto(g_world, reinterpret_cast<const char*>(g_nadeOut.data() + at), 32, 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
                    g_nadeOut.clear();
                }
        }
        if (!g_scratch) g_scratch = Guest::AllocatePhysical(64);
        g_hostPlayerIndex = -1;
        for (auto& [i, s] : g_hostWorld) if (s.flags & 2) {   // the host player himself (his entry)
            if (!g_haveHostShots) g_hostStance = (s.flags & 128) ? 4 : (s.flags & 32) ? 2 : 1;
            g_hostPlayerIndex = i;
            memcpy(g_hostPlace, s.origin, 12); memcpy(g_hostPlace + 3, s.angles, 12); g_haveHostPlace = true; }
        // (0.5.230) once in a mission, the joining player is brought to the
        // host if far from him (the host restarted at a checkpoint, this copy
        // at the mission's start), 5 s after this copy's player is in
        {
            static uint32_t doneGen = 0;
            static double inSince = -1;
            const uint32_t me = EntityAt(base, 1);
            if (!(me && Guest::Read32(base, me + 528))) inSince = -1;
            else if (inSince < 0) inSince = now;
            if (doneGen != g_mapGen && inSince >= 0 && now - inSince > 5 && g_haveHostPlace && !g_hostWorld.empty())
            {
                doneGen = g_mapGen;
                const float dx = F(base, me + 336) - g_hostPlace[0], dy = F(base, me + 340) - g_hostPlace[1];
                if (sqrtf(dx * dx + dy * dy) > 1500.0f) { printf("coop: players: far from the host after loading: brought to him\n"); fflush(stdout); Respawn(base, me); }
            }
        }
        // (0.5.233) the host's body here: a soldier of our own (MakeBody), or,
        // when no friendly spawner makes one in 10 tries, one of the
        // level's riflemen taken out of the pairing. Slid to the host's place
        // (his entry, flag 2) each frame, never hurt here.
        {
            static int32_t body = -1;
            static int tries = 0;
            static double lastTry = 0;
            static uint32_t bodyGen = 0;
            if (bodyGen != g_mapGen) { bodyGen = g_mapGen; body = -1; tries = 0; g_hostBodyEnt = 0; }
            if (body >= 0)
            {
                const uint32_t e = EntityAt(base, uint32_t(body));
                if (!e || e != g_hostBodyEnt || !Guest::Read32(base, e + 532) || int32_t(Guest::Read32(base, e + 776)) <= 0 || FieldText(base, e, 572) == "'delete_me'")
                { printf("coop: players: the host's body here (%d) is gone: another\n", body); fflush(stdout); body = -1; g_hostBodyEnt = 0; }
            }
            const uint32_t me = EntityAt(base, 1);
            const bool inMission = me && Guest::Read32(base, me + 528);
            static const bool borrow = getenv("COD3_COOP_BORROW") != nullptr;
            if (body < 0 && inMission && g_haveHostPlace && !g_hostWorld.empty() && now - lastTry > 2)
            {
                lastTry = now;
                if (!borrow && tries < 10)
                {
                    tries++;
                    body = MakeBody(ctx, base, 0);
                    if (body < 0 && tries == 10) { printf("coop: players: no friendly spawner made a soldier: borrowing one of the level's\n"); fflush(stdout); }
                }
                else
                    for (uint32_t i = 0; i < 4096 && body < 0; i++)
                        if (const uint32_t e = EntityAt(base, i); e && IsRifleman(base, e)) { body = int32_t(i); printf("coop: players: soldier %u (%s) is the host's body here\n", i, FieldText(base, e, 564).c_str()); fflush(stdout); }
                if (body >= 0) g_hostBodyEnt = EntityAt(base, uint32_t(body));
            }
            if (body >= 0 && g_hostBodyEnt)
            {
                const uint32_t e = g_hostBodyEnt;
                Guest::Write32(base, e + 624, 0);
                Guest::Write32(base, e + 776, 100000);
                if (const auto track = g_tracks.find(uint16_t(g_hostPlayerIndex)); g_hostPlayerIndex >= 0 && track != g_tracks.end())
                    DriveActor(ctx, base, e, track->second, g_hostClock.Show(now), true, now);
                static StanceFilter stance;
                SetStance(base, e, stance.Get(g_hostStance, now));
                static uint32_t shown = 0;
                if (g_haveHostShots) ShowShots(ctx, base, body, g_hostShots, shown);
            }
        }
        // DIAGNOSTIC: COD3_COOP_TESTDOWN=seconds: killed then, to try the way back
        if (const char* t = getenv("COD3_COOP_TESTDOWN")) { static bool done = false; if (!done && now >= atof(t)) { done = true; g_downAt = now; } }
        if (g_downAt >= 0)
        {
            if (const uint32_t me = EntityAt(base, 1); me && Guest::Read32(base, me + 528))
            {
                static bool told = false;
                if (!told) { told = true; printf("coop: players: killed in the host's world: back in 5 s\n"); fflush(stdout); }
                Guest::Write32(base, me + 776, 1);
                if (now - g_downAt >= 5)
                {
                    if (!g_haveHostPlace) { printf("coop: players: the host's place is not known yet\n"); fflush(stdout); g_downAt = now; }
                    else { Respawn(base, me); g_downAt = -1; told = false; }
                }
            }
        }
        // (co-op v35) the host's world is applied unless COD3_COOP_APPLY=0
        // (it took COD3_COOP_APPLY=goal before; the launchers set that)
        static const bool apply = [] { const char* t = getenv("COD3_COOP_APPLY"); return !(t && t[0] == '0'); }();
        static uint32_t vector = 0;
        if (apply && !vector) vector = Guest::AllocatePhysical(64);
        // this copy's own player takes no damage of his own either
        if (apply) if (const uint32_t me = EntityAt(base, 1); me && Guest::Read32(base, me + 528)) Guest::Write32(base, me + 624, 0);
        // The host's entries paired with this copy's entities by what they
        // are (the key): a pair, once made, is kept while both still are what
        // they were; a new one takes, of this copy's unpaired entities with
        // that key, the nearest to the host's place. (Pairing by order among
        // the same key shuffled the pairs: 35 to 61 changed between checks.)
        // (co-op v35) A living soldier of the host's is paired only with a
        // living one of ours within 800 units (the joiner's log: soldiers
        // 20000 apart were paired, then pulled across the map: the "random
        // spawning"); with none near, one is made for him (below). A pair
        // whose soldier here has died while the host's lives is let go.
        std::map<uint16_t, uint32_t>& pairs = g_pairs;
        {
            static uint32_t pairsGen = 0;
            if (pairsGen != g_mapGen) { pairsGen = g_mapGen; pairs.clear(); }
        }
        auto alive = [&](uint32_t e) { const uint32_t a = Guest::Read32(base, e + 532); return a >= 0x40000000u && a < 0xC0000000u && int32_t(Guest::Read32(base, e + 776)) > 0; };
        {
            std::map<uint32_t, uint32_t> keyOf;   // this copy's entity -> its key
            std::map<uint32_t, std::vector<uint32_t>> mine;
            const uint32_t me = EntityAt(base, 1);
            for (uint32_t i = 0; i < 4096; i++) if (const uint32_t e = EntityAt(base, i); e && e != g_hostBodyEnt && e != me) { const uint32_t k = EntityKey(base, e); keyOf[e] = k; mine[k].push_back(e); }
            std::set<uint32_t> taken;
            for (auto it = pairs.begin(); it != pairs.end();)
            {
                const auto h = g_hostWorld.find(it->first);
                const auto k = keyOf.find(it->second);
                const bool lostHere = h != g_hostWorld.end() && (h->second.flags & 1) && h->second.health > 0 && !alive(it->second);
                if (h == g_hostWorld.end() || k == keyOf.end() || k->second != h->second.key || taken.count(it->second) || lostHere) it = pairs.erase(it);
                else { taken.insert(it->second); ++it; }
            }
            for (auto& [i, s] : g_hostWorld)
            {
                if (pairs.count(i) || (s.flags & 64)) continue;   // (64: the host's own soldier for us: we are him)
                if ((s.flags & 1) && s.health <= 0) continue;   // a dead soldier of the host's is not given a living one of ours (it killed a second)
                auto it = mine.find(s.key);
                if (it == mine.end()) continue;
                uint32_t best = 0; float bestD = 1e30f;
                for (const uint32_t e : it->second)
                {
                    if (taken.count(e)) continue;
                    if ((s.flags & 1) && !alive(e)) continue;
                    const float dx = F(base, e + 336) - s.origin[0], dy = F(base, e + 340) - s.origin[1], dz = F(base, e + 344) - s.origin[2];
                    const float d = dx * dx + dy * dy + dz * dz;
                    if (d < bestD) { bestD = d; best = e; }
                }
                // (0.5.230) a thing that is not a soldier is paired only with one of
                // ours close by: test 2 had aeroplanes driving about on the
                // ground, ours paired with the host's ground vehicles
                if (!(s.flags & 1) && bestD > 1500.0f * 1500.0f) best = 0;
                // (the story's own soldiers, Dixon and the like, are one of a
                // kind: paired wherever they are; the one-PC test hid them)
                if ((s.flags & 1) && bestD > 800.0f * 800.0f && best && FieldText(base, best, 564).find("_reg") != std::string::npos) best = 0;
                if (best) { pairs[i] = best; taken.insert(best); }
            }
        }
        // (co-op v35) A living soldier of the host's with no partner here for
        // 1.5 s is given one: a new soldier from one of this copy's spawners
        // of the same kind (the scripts' dospawn, as MakeBody), put where the
        // host's is. Four a second at most, three tries each.
        if (apply && !g_hostWorld.empty())
        {
            static std::map<uint16_t, double> since;
            static std::map<uint16_t, int> tries;
            static double lastMade = 0;
            static uint32_t gen = 0;
            if (gen != g_mapGen) { gen = g_mapGen; since.clear(); tries.clear(); }
            for (auto it = since.begin(); it != since.end();) { if (!g_hostWorld.count(it->first) || pairs.count(it->first)) it = since.erase(it); else ++it; }
            for (auto& [i, s] : g_hostWorld)
            {
                if (!(s.flags & 1) || s.health <= 0 || (s.flags & (2 | 4 | 8 | 64)) || pairs.count(i)) continue;
                const double first = since.emplace(i, now).first->second;
                if (now - first < 1.5 || now - lastMade < 0.25 || tries[i] >= 3 || !g_scratch) continue;
                lastMade = now;
                tries[i]++;
                uint32_t spawner = 0xFFFFFFFF; float bestD = 1e30f;
                for (uint32_t n = 0; n < 4096; n++)
                    if (const uint32_t e = EntityAt(base, n); e && base[e] == 12 && ((uint32_t(s.type) * 1000003u) ^ KeyPart(base, e, 564)) == s.key)
                    {
                        const float dx = F(base, e + 336) - s.origin[0], dy = F(base, e + 340) - s.origin[1];
                        if (dx * dx + dy * dy < bestD) { bestD = dx * dx + dy * dy; spawner = n; }
                    }
                if (spawner == 0xFFFFFFFF) { if (tries[i] == 1) { printf("coop: world: the host's soldier %u: no spawner of his kind here\n", i); fflush(stdout); } tries[i] = 3; continue; }
                std::set<uint32_t> before;
                for (uint32_t n = 0; n < 4096; n++) if (const uint32_t e = EntityAt(base, n); e && Guest::Read32(base, e + 532)) before.insert(e);
                Guest::Write32(base, g_scratch + 48, 0);   // the empty name
                PPCContext call = ctx;
                call.r1.u32 = (call.r1.u32 - 0x400) & ~0xFu;
                call.r3.u64 = spawner | (Guest::Read32(base, EntityTable + spawner * 8 + 4) << 14);
                call.r4.u64 = g_scratch + 48; call.r5.u64 = 0;
                call.lr = 0;
                Guest::Lookup(0x824B9118)(call, base);
                uint32_t made = 0;
                for (uint32_t n = 0; n < 4096 && !made; n++)
                    if (const uint32_t e = EntityAt(base, n); e && Guest::Read32(base, e + 532) && !before.count(e)) made = e;
                if (made)
                {
                    const float yaw[3] = { 0.0f, s.angles[1], 0.0f };
                    PutAt(base, made, s.origin, yaw);
                    pairs[i] = made;
                    printf("coop: world: the host's soldier %u had no partner here: made one (from spawner %u %s)\n", i, spawner, FieldText(base, EntityAt(base, spawner), 564).c_str());
                }
                else printf("coop: world: the host's soldier %u: spawner %u made nobody (try %d)\n", i, spawner, tries[i]);
                fflush(stdout);
            }
        }
        static double last = 0;
        if (now - last >= 5 && !g_hostWorld.empty())
        {
            last = now;
            int same = 0, otherType = 0, missing = 0, actors = 0;
            double sum = 0, worst = 0;
            for (auto& [i, s] : g_hostWorld)
            {
                const auto p = pairs.find(i);
                const uint32_t e = p == pairs.end() ? 0 : p->second;
                if (!e) { if (s.flags & 1) missing++; continue; }
                if (e != EntityAt(base, i)) otherType++;   // (here: paired with another number)
                same++;
                if ((s.flags & 1) && s.health > 0)
                {
                    actors++;
                    const double dx = F(base, e + 336) - s.origin[0], dy = F(base, e + 340) - s.origin[1], dz = F(base, e + 344) - s.origin[2];
                    const double d = sqrt(dx * dx + dy * dy + dz * dz);
                    sum += d; if (d > worst) worst = d;
                }
            }
            {
                int told = 0;
                for (auto& [i, s] : g_hostWorld)
                    if ((s.flags & 1) && s.health > 0 && !pairs.count(i) && told++ < 4)
                        printf("coop: world: the host's soldier %u (key %08X) has no pair here\n", i, s.key);
            }
            printf("coop: world at %.0f s: host's %zu entries: %d paired (%d under another number), %d living soldiers with no pair; %d soldiers %.1f apart on average (worst %.1f); clock delay %.0f ms, jitter %.0f ms; %u put in place outright so far\n",
                   now, g_hostWorld.size(), same, otherType, missing, actors, actors ? sum / actors : 0.0, worst, g_hostClock.Delay() * 1000.0, g_hostClock.jitter * 1000.0, g_snaps);
            fflush(stdout);
        }
        // This copy's own soldiers the host has none of (spawned here only:
        // "wandering_player1_spawned" Germans) are let go (the scripts'
        // delete, sub_824A7280). (co-op v35) Hidden after 0.75 s unpaired
        // (the scripts' hide: flag 0x400 of +632), shown again if paired,
        // let go after 4 s (8 before: they ran about where the host has
        // nobody). The one-PC test: delete leaves a living soldier be (the
        // same ones were let go again and again), so one still there is put
        // out of the way below the level each second (a later pair brings
        // him back: he is far off, so he is put in place). The story's own
        // soldiers (no "_reg" in the class) are never hidden or let go.
        if (apply && g_lastWorldAt >= 0 && now - g_lastWorldAt < 1.0)
        {
            static std::map<uint32_t, double> unpairedSince;
            static std::set<uint32_t> hidden;
            static uint32_t gen = 0;
            if (gen != g_mapGen) { gen = g_mapGen; unpairedSince.clear(); hidden.clear(); }
            std::set<uint32_t> pairedSet;
            for (auto& [i, p] : pairs) pairedSet.insert(p);
            for (auto it = hidden.begin(); it != hidden.end();)
            {
                bool still = false;
                for (uint32_t n = 0; n < 4096 && !still; n++) if (EntityAt(base, n) == *it) still = true;
                if (!still) { it = hidden.erase(it); continue; }
                if (pairedSet.count(*it)) { Guest::Write32(base, *it + 632, Guest::Read32(base, *it + 632) & ~0x400u); it = hidden.erase(it); continue; }
                ++it;
            }
            for (uint32_t n = 0; n < 4096; n++)
            {
                const uint32_t e = EntityAt(base, n);
                if (!e || e == g_hostBodyEnt || !Guest::Read32(base, e + 532) || pairedSet.count(e) || int32_t(Guest::Read32(base, e + 776)) <= 0) { if (e) unpairedSince.erase(e); continue; }
                if (FieldText(base, e, 564).find("_reg") == std::string::npos) continue;
                auto [it, fresh] = unpairedSince.emplace(e, now);
                if (now - it->second >= 0.75 && !hidden.count(e)) { hidden.insert(e); Guest::Write32(base, e + 632, Guest::Read32(base, e + 632) | 0x400u); }
                if (now - it->second < 4) continue;
                static std::set<uint32_t> tried;
                if (fresh) tried.erase(e);
                if (tried.count(e))
                {
                    static std::map<uint32_t, double> putAt;
                    if (now - putAt[e] >= 1.0) { putAt[e] = now; HideBelow(base, e); }
                    continue;
                }
                tried.insert(e);
                PPCContext call = ctx;
                call.r1.u32 = (call.r1.u32 - 0x400) & ~0xFu;
                call.r3.u64 = n | (Guest::Read32(base, EntityTable + n * 8 + 4) << 14); call.lr = 0;
                printf("coop: world: our soldier %u (%s) has no partner on the host: let go\n", n, FieldText(base, e, 564).c_str());
                fflush(stdout);
                hidden.erase(e);
                Guest::Lookup(0x824A7280)(call, base);   // the scripts' delete
            }
        }
        // (co-op v35) This copy's soldiers throw no grenades of their own (the
        // grenadeammo, actor +2576, kept at 0): the host's soldiers' grenades
        // come from the host (CWG2), thrown here by their partners.
        if (apply)
        {
            static double lastAmmo = 0;
            if (now - lastAmmo >= 0.5)
            {
                lastAmmo = now;
                for (uint32_t n = 2; n < 4096; n++)
                    if (const uint32_t e = EntityAt(base, n); e && e != g_hostBodyEnt)
                        if (const uint32_t a = Guest::Read32(base, e + 532); a >= 0x40000000u && a < 0xC0000000u) Guest::Write32(base, a + 2576, 0);
            }
        }
        // Each of the joining copy's entities moved where the host's is (the
        // origin's copies: an entity's +32, +44, +304, +336, +416; an actor's
        // record +720; the angles' +72, +352), shown from the buffer at the
        // host's time a little in the past (co-op v35, DriveActor / DriveThing).
        if (apply)
        {
            const double show = g_hostClock.Show(now);
            for (auto& [i, s] : g_hostWorld)
            {
                const auto p = pairs.find(i);
                const uint32_t e = p == pairs.end() ? 0 : p->second;
                if (!e || (s.flags & 2)) continue;
                if (s.flags & 4) { HideBelow(base, e); continue; }   // the joining player's stand-in: that is us
                const auto track = g_tracks.find(i);
                if (track == g_tracks.end()) continue;
                const uint32_t actor = Guest::Read32(base, e + 532);
                const uint32_t sentient = Guest::Read32(base, e + 536);
                const bool ourSoldier = actor >= 0x40000000u && actor < 0xC0000000u;
                if (s.flags & 8)   // the host's stand-in: walks where the host is
                {
                    Guest::Write32(base, e + 624, 0);
                    if (ourSoldier) DriveActor(ctx, base, e, track->second, show, true, now);
                    continue;
                }
                if (s.flags & 1)
                {
                    // a soldier's body is left where it fell; a soldier is
                    // never moved by anything but a soldier of the host's
                    if (!ourSoldier) continue;
                    const int32_t health = int32_t(Guest::Read32(base, e + 776));
                    if (health <= 0) continue;
                    // Damage is the host's: this copy's soldiers take none of
                    // their own, and one is killed here when the host's has died
                    // (the scripts' dodamage, sub_820B2720: the entity, a point, a
                    // direction, the amount), so he falls as the host's did
                    // (and drops his weapon here as there).
                    if (s.health <= 0)
                    {
                        if (!vector) continue;
                        const uint32_t ref = EntityRef(base, e);
                        if (ref == 0xFFFFFFFF) continue;
                        Guest::Write32(base, e + 624, 1);
                        for (int k = 0; k < 3; k++) SetF(base, vector + 4 * k, F(base, e + 336 + 4 * k) + (k == 2 ? 48.0f : 0.0f));
                        SetF(base, vector + 16, 0.0f); SetF(base, vector + 20, 0.0f); SetF(base, vector + 24, 1.0f);
                        PPCContext call = ctx;
                        call.r1.u32 = (call.r1.u32 - 0x400) & ~0xFu;
                        call.r3.u64 = ref;
                        call.r4.u64 = vector; call.r5.u64 = vector + 16;
                        call.f1.f64 = double(health) + 1000.0;
                        call.lr = 0;
                        Guest::Lookup(0x820B2720)(call, base);
                        printf("coop: world: soldier %u (the host's %u) killed, as the host's was (health now %d)\n", ref & 0x3FFF, i, int32_t(Guest::Read32(base, e + 776)));
                        fflush(stdout);
                        continue;
                    }
                    // Wounds dealt here: a soldier is kept at 100000 (so nothing
                    // here kills him), and what he loses in a frame, when he
                    // stands where this copy's player is aiming (within 6
                    // degrees), is the player's hit: sent to the host (CWD1).
                    // (enemies only: friendly soldiers take nothing here, and the
                    // story's own (100000000, the scripts' shield) are left be:
                    // treating one as wounded killed Dixon on the host)
                    const bool enemy = FieldText(base, e, 564).rfind("'actor_axis", 0) == 0;
                    if (!enemy) Guest::Write32(base, e + 624, 0);
                    else
                    {
                        Guest::Write32(base, e + 624, 1);
                        if (health < 100000 && 100000 - health <= 1000)
                        {
                            const int32_t lost = 100000 - health;
                            const uint32_t me = EntityAt(base, 1);
                            const uint32_t rec = me ? Guest::Read32(base, me + 528) : 0;
                            if (me && rec >= 0x40000000u)
                            {
                                const float pitch = F(base, rec + 172) * 0.0174533f, yaw = F(base, rec + 176) * 0.0174533f;
                                const float aim[3] = { cosf(pitch) * cosf(yaw), cosf(pitch) * sinf(yaw), -sinf(pitch) };
                                float to[3] = { F(base, e + 336) - F(base, me + 336), F(base, e + 340) - F(base, me + 340), F(base, e + 344) + 40.0f - F(base, me + 344) - 60.0f };
                                const float n = sqrtf(to[0] * to[0] + to[1] * to[1] + to[2] * to[2]);
                                const float cosine = n > 1 ? (to[0] * aim[0] + to[1] * aim[1] + to[2] * aim[2]) / n : 0;
                                if (cosine > 0.9945f)
                                {
                                    uint8_t m[12] = {}; memcpy(m, "CWD1", 4); memcpy(m + 4, &i, 2); memcpy(m + 8, &lost, 4);
                                    sockaddr_in to2 = g_worldTo; to2.sin_port = htons(uint16_t(Port() + 3));
                                    inet_pton(AF_INET, OtherPc().c_str(), &to2.sin_addr);
                                    sendto(g_world, reinterpret_cast<const char*>(m), 12, 0, reinterpret_cast<sockaddr*>(&to2), sizeof(to2));
                                    printf("coop: players: we hit the host's soldier %u for %d (aim %.3f)\n", i, lost, cosine);
                                    fflush(stdout);
                                }
                            }
                        }
                        Guest::Write32(base, e + 776, 100000);
                    }
                    if (sentient >= 0x40000000u && sentient < 0xC0000000u) DriveActor(ctx, base, e, track->second, show, false, now);
                    continue;
                }
                // a thing that is not a soldier: only one that has moved on the
                // host, and never one of ours that is (or was) a soldier
                if (ourSoldier || !(s.flags & 16) || FieldText(base, e, 564).rfind("'actor_", 0) == 0) continue;
                DriveThing(base, e, track->second, show);
            }
        }
    }
}

// loadgame_restart: the checkpoint command does not pass through
// SV_SpawnServer. Remove transient host effects on both sides of restore.
extern "C" PPC_FUNC(__imp__sub_82518518);
PPC_FUNC(sub_82518518)
{
    Blood::Clear();
    __imp__sub_82518518(ctx,base);
    Blood::Clear();
}

// SV_SpawnServer: a mission starting here (its name in r3).
PPC_FUNC(sub_82517A00)
{
    Blood::Clear();
    if (CoopMode() != Mode::Off && ctx.r3.u32)
    {
        std::string name;
        for (uint32_t i = 0; i < 63; i++) { const uint8_t c = base[ctx.r3.u32 + i]; if (!c) break; name += char(c); }
        g_myMap = name;
        // a new mission: its soldiers are new, so new stand-ins and pairs
        g_mapGen++;
        if (CoopMode() == Mode::Host) g_hostEpoch++;
        g_restartWanted = false;
        g_drive.clear();
        g_tracks.clear(); g_hostSeen.clear(); g_hostWorld.clear(); g_lastWorldAt = -1; g_pairs.clear();
        g_joinerTrack = Track{};
        g_filmSkipFromHost = false;
        g_filmPeerReady = false;
        g_filmToldWaiting = false;
        g_avatarA = g_avatarB = -1;
        g_standinFallback = false;
        printf("coop: lobby: this copy starts %s\n", name.c_str());
        fflush(stdout);
        // the host tells the joining copy at once, before his own loading
        // (no frames run while it loads), so both load together
        if (CoopMode() == Mode::Host && OpenWorld())
        {
            uint8_t m[72] = {}; memcpy(m, "CWL1", 4);
            memcpy(m + 4, name.c_str(), std::min<size_t>(name.size(), 63));
            memcpy(m + 68, &g_hostEpoch, 4);
            for (int k = 0; k < 3; k++) sendto(g_world, reinterpret_cast<const char*>(m), 72, 0, reinterpret_cast<sockaddr*>(&g_worldTo), sizeof(g_worldTo));
        }
    }
    __imp__sub_82517A00(ctx, base);
}

void NativeOptionsPoll(uint8_t* base);   // native_options.cpp: the options kept between plays
namespace
{
    // (0.5.237) Sprinting in the campaign. The campaign has the engine's
    // "+sprint" but does nothing with it (a test: no faster); its player
    // speed is the g_speed setting (190; the test: 300 took him 526 units in
    // 2 s against 323). While sprint is held (the left stick pressed in, or
    // latched by tap-to-sprint, input.cpp), going forward, standing and not
    // aiming, g_speed is set to 1.5 times what it was, and put back after.
    // COD3_NOSPRINT=1 turns it off.
    // (co-op v35) Made to work like multiplayer's sprint. The campaign
    // program has no sprint of its own to switch on: multiplayer's
    // (sub_8248E970 starts it, sub_82495980 runs the stamina at record +1168)
    // has no counterpart in default.xex. So, as multiplayer plays it:
    // - a tap of sprint runs (toggle_sprint, as before), and the run ends
    //   when you stop going forward, aim, fire, crouch or go prone
    // - the speed comes up over a quarter second (it jumped at once)
    // - a run lasts 4 s (COD3_SPRINT_SECONDS); run dry and you are out of
    //   breath: no sprint for 1.5 s, then it comes back from a quarter;
    //   stopped yourself, the stamina starts to come back after 0.75 s and
    //   is full from empty in 3 s (multiplayer's quick sprint recovery,
    //   mp_fixes.cpp)
    void SprintPoll(const uint8_t* base)
    {
        static const bool off = getenv("COD3_NOSPRINT") != nullptr;
        static const double length = [] { const char* t = getenv("COD3_SPRINT_SECONDS"); return t ? std::clamp(atof(t), 1.0, 60.0) : 4.0; }();
        static bool on = false;
        static int32_t normal = 190, written = 190;
        static uint32_t gen = 0;
        static float stamina = 1.0f;
        static double lastAt = -1, onSince = -1, stoppedAt = -1, windedUntil = -1;
        const double now = Seconds();
        const float dt = lastAt < 0 ? 0.0f : float(std::clamp(now - lastAt, 0.0, 0.1));
        lastAt = now;
        const uint32_t me = EntityAt(base, 1);
        const uint32_t rec = me ? Guest::Read32(base, me + 528) : 0;
        const bool inGame = rec >= 0x40000000u && rec < 0xC0000000u;
        auto speed = [&](int32_t value) { if (value != written) { written = value; Kernel::QueueConsoleCommand("g_speed " + std::to_string(value)); } };
        if (gen != g_mapGen) { gen = g_mapGen; stamina = 1.0f; windedUntil = -1; if (on) { on = false; speed(normal); } }
        bool want = false;
        if (inGame && !off)
        {
            const uint32_t flags = Guest::Read32(base, rec + 208);
            const int32_t height = int32_t(Guest::Read32(base, rec + 184));
            const bool firing = (Guest::Read32(base, rec + 1352) & 1) != 0;
            want = Input::SprintHeld() && !(flags & 0x20) && height >= 55 && !firing && now >= windedUntil && stamina > 0.0f;
        }
        if (on)
        {
            stamina -= float(dt / length);
            if (stamina <= 0.0f)
            {
                stamina = 0.0f; windedUntil = now + 1.5; want = false;
                static int told = 0; if (told++ < 20) { printf("sprint: out of breath: no sprint for 1.5 s\n"); fflush(stdout); }
            }
        }
        else if (now >= windedUntil)
        {
            if (windedUntil > 0) { windedUntil = -1; stamina = std::max(stamina, 0.25f); stoppedAt = now - 1.0; }
            if (now - stoppedAt > 0.75) stamina = std::min(1.0f, stamina + dt / 3.0f);
        }
        if (want && !on)
        {
            const int32_t was = int32_t(Guest::Read32(base, rec + 84));
            if (written == normal) normal = was >= 50 && was <= 400 ? was : 190;
            on = true; onSince = now;
            static int told = 0; if (told++ < 20) { printf("sprint: on (g_speed %d up to %d, stamina %.2f)\n", normal, normal * 3 / 2, stamina); fflush(stdout); }
        }
        else if (!want && on) { on = false; stoppedAt = now; speed(normal); }
        if (on)
        {
            // up to 1.5 times over a quarter second, in steps of a tenth
            const double ramp = std::min(1.0, (now - onSince) / 0.25);
            const int32_t step = int32_t(ramp * 5.0 + 0.5);
            speed(normal + normal * step / 10);
        }
    }
}
// (0.5.316, Team Justin) The campaign mortar's camera, measured: for the
// multiplayer mortar killstreak (mp_fixes.cpp) to put the player, the view
// and the tube exactly as the campaign does at Laison. Twice a second, while
// an entity whose classname or model names a mortar is within 300 units of
// the player: the player's entity (origin +336, angles +352), his record
// (origin +0, view height +184, flags +208, and the floats +160..+300 to
// find the view angles), and each such entity's index, type, class, origin
// and angles, also in the player's own frame (forward, right, up). Logged
// as "mortar probe:" (COD3_MORTAR_PROBE=0 turns it off). Read-only.
namespace
{
    void MortarProbePoll(uint8_t* base)
    {
        static const bool off = []() { const char* t = getenv("COD3_MORTAR_PROBE"); return t && t[0] == '0'; }();
        if (off) return;
        static double last = 0;
        const double now = Seconds();
        if (now - last < 0.5) return;
        last = now;
        const uint32_t me = EntityAt(base, 1);
        const uint32_t rec = me ? Guest::Read32(base, me + 528) : 0;
        if (rec < 0x40000000u || rec >= 0xC0000000u) return;
        const float px = F(base, me + 336), py = F(base, me + 340), pz = F(base, me + 344), pyaw = F(base, me + 356);
        std::string found;
        for (uint32_t i = 0; i < 4096; i++)
        {
            const uint32_t e = EntityAt(base, i);
            if (!e || e == me) continue;
            const float ex = F(base, e + 336), ey = F(base, e + 340), ez = F(base, e + 344);
            const float dx = ex - px, dy = ey - py, dz = ez - pz;
            if (dx * dx + dy * dy + dz * dz > 300.0f * 300.0f) continue;
            std::string names;
            for (const uint32_t at : { 556u, 564u, 572u, 580u, 596u, 604u }) names += FieldText(base, e, at);
            std::string low = names;
            for (char& c : low) c = char(tolower(uint8_t(c)));
            if (low.find("mortar") == std::string::npos && low.find("81mm") == std::string::npos) continue;
            const float r = pyaw * 0.0174533f;
            const float fwd = dx * cosf(r) + dy * sinf(r), right = dx * sinf(r) - dy * cosf(r);
            char line[512];
            snprintf(line, sizeof line, "\nmortar probe:   entity %u type %u%s at %.2f %.2f %.2f angles %.2f %.2f %.2f; from the player: forward %.2f right %.2f up %.2f, yaw %+.2f",
                     i, Guest::Read8(base, e), Fields(base, e).c_str(), ex, ey, ez, F(base, e + 352), F(base, e + 356), F(base, e + 360), fwd, right, dz, F(base, e + 356) - pyaw);
            found += line;
        }
        if (found.empty()) return;
        std::string floats;
        for (uint32_t at = 160; at <= 300; at += 4)
        {
            char t[48];
            snprintf(t, sizeof t, " +%u=%.2f", at, F(base, rec + at));
            floats += t;
        }
        printf("mortar probe: player at %.2f %.2f %.2f angles %.2f %.2f %.2f; record origin %.2f %.2f %.2f view height %d flags %X\nmortar probe:   record floats%s%s\n",
               px, py, pz, F(base, me + 352), pyaw, F(base, me + 360), F(base, rec), F(base, rec + 4), F(base, rec + 8),
               int32_t(Guest::Read32(base, rec + 184)), Guest::Read32(base, rec + 208), floats.c_str(), found.c_str());
        fflush(stdout);
    }
}
// Com_EventLoop: the joining copy takes the host's packets in first.
// COD3_COOP_SHOTS="60 90": screenshots at those seconds after the start.
PPC_FUNC(sub_82535E80)
{
    NativeOptionsPoll(base);
    Blood::Update(ctx,base);
    // Objectives visibility is maintained at the native HUD draw, not by reopening its panel.
    SprintPoll(base);
    MortarProbePoll(base);   // (0.5.316, Team Justin) the campaign mortar's camera, measured
    if (const char* shots = getenv("COD3_COOP_SHOTS"))
    {
        static const auto start = std::chrono::steady_clock::now();
        static std::vector<double> at;
        static bool parsed = false;
        if (!parsed) { parsed = true; for (const char* t = shots; *t;) { char* end; const double v = strtod(t, &end); if (end == t) break; at.push_back(v); t = end; } }
        const double now = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        for (size_t i = 0; i < at.size(); i++)
            if (now >= at[i]) { Overlay::RequestScreenshot(); printf("coop: screenshot at %.1f s\n", now); ListSoldiers(base); fflush(stdout); at.erase(at.begin() + long(i)); break; }
    }
    // DIAGNOSTIC: COD3_COOP_RECDUMP="110 118": the player's entity (1024
    // bytes) and record (4096 bytes) printed at those seconds, to find his
    // stance and firing by comparing dumps
    if (const char* dumps = getenv("COD3_COOP_RECDUMP"))
    {
        static std::vector<double> at;
        static bool parsed = false;
        if (!parsed) { parsed = true; for (const char* t = dumps; *t;) { char* end; const double v = strtod(t, &end); if (end == t) break; at.push_back(v); t = end; } }
        const double now = Seconds();
        for (size_t i = 0; i < at.size(); i++)
            if (now >= at[i])
            {
                at.erase(at.begin() + long(i));
                const uint32_t me = EntityAt(base, 1);
                const uint32_t rec = me ? Guest::Read32(base, me + 528) : 0;
                printf("coop: dump at %.1f s: entity %08X record %08X\n", now, me, rec);
                auto dump = [&](const char* what, uint32_t from, uint32_t size) {
                    if (from < 0x40000000u || from >= 0xC0000000u) return;
                    for (uint32_t o = 0; o < size; o += 32)
                    {
                        printf("coop: dump %s +%04X:", what, o);
                        for (uint32_t k = 0; k < 32; k += 4) printf(" %08X", Guest::Read32(base, from + o + k));
                        printf("\n");
                    }
                };
                dump("ent", me, 1024); dump("rec", rec, 4096);
                fflush(stdout);
                break;
            }
    }
    // DIAGNOSTIC: COD3_SPEEDTEST=t: the player's place at t, t+3 (walking as
    // he is), then t+5, t+8 with the record's speed (+84, 190) written as 300
    // each frame: whether that field makes him faster (for sprinting)
    if (const char* st = getenv("COD3_SPEEDTEST"))
    {
        const double t0 = atof(st), now = Seconds();
        static int step = 0;
        const uint32_t me = EntityAt(base, 1);
        const uint32_t rec = me ? Guest::Read32(base, me + 528) : 0;
        if (rec >= 0x40000000u && rec < 0xC0000000u)
        {
            const double marks[4] = { t0, t0 + 2, t0 + 3, t0 + 5 };
            if (step < 4 && now >= marks[step])
            {
                printf("coop: speedtest %d at %.1f s: %.1f %.1f %.1f (speed field %d)\n", step, now, F(base, me + 336), F(base, me + 340), F(base, me + 344), int32_t(Guest::Read32(base, rec + 84)));
                fflush(stdout);
                step++;
            }

        }
    }
    // DIAGNOSTIC: COD3_COOP_ENTWATCH=seconds: from then on, each entity that
    // appears is printed (type, fields, place, 1024 bytes once) and followed
    // four times a second until it goes, to find thrown grenades
    if (const char* w = getenv("COD3_COOP_ENTWATCH"))
    {
        static std::map<uint32_t, int> watched;   // entity -> times printed
        static std::set<uint32_t> known;
        static bool primed = false;
        static double lastAt = 0;
        const double now = Seconds();
        if (now >= atof(w) && now - lastAt >= 0.25)
        {
            lastAt = now;
            std::set<uint32_t> present;
            for (uint32_t i = 0; i < 4096; i++) if (const uint32_t e = EntityAt(base, i)) present.insert(e);
            if (!primed) { primed = true; known = present; printf("coop: entwatch: %zu entities known\n", known.size()); }
            for (const uint32_t e : present)
                if (!known.count(e))
                {
                    known.insert(e); watched[e] = 0;
                    uint32_t index = 0; for (uint32_t i = 0; i < 4096; i++) if (EntityAt(base, i) == e) { index = i; break; }
                    printf("coop: entwatch: new %u at %08X type %u%s\n", index, e, base[e], Fields(base, e).c_str());
                    for (uint32_t o = 0; o < 1024; o += 32)
                    {
                        printf("coop: entwatch %08X +%04X:", e, o);
                        for (uint32_t k = 0; k < 32; k += 4) printf(" %08X", Guest::Read32(base, e + o + k));
                        printf("\n");
                    }
                }
            for (auto it = watched.begin(); it != watched.end();)
            {
                if (!present.count(it->first)) { printf("coop: entwatch: %08X gone\n", it->first); it = watched.erase(it); continue; }
                if (it->second++ < 40)
                    printf("coop: entwatch: %08X at %.0f %.0f %.0f (+32: %.0f %.0f %.0f)\n", it->first, F(base, it->first + 336), F(base, it->first + 340), F(base, it->first + 344), F(base, it->first + 32), F(base, it->first + 36), F(base, it->first + 40));
                ++it;
            }
            fflush(stdout);
        }
    }
    WorldSync(ctx, base);
    static const bool mirror = getenv("COD3_COOP_MIRROR") != nullptr;
    if (mirror && CoopMode() == Mode::Join && OpenSocket())
    {
        static uint32_t buffer = 0;
        if (!buffer) buffer = Guest::AllocatePhysical(4096);
        uint8_t packet[3100];
        // (at most three a frame: the client's queue has four slots)
        for (int i = 0; i < 3; i++)
        {
            const int got = recv(g_socket, reinterpret_cast<char*>(packet), sizeof(packet), 0);
            if (got <= 4) break;
            uint32_t length;
            memcpy(&length, packet, 4);
            if (length > 3072 || int(length + 4) != got) continue;
            g_received++;
            if (!g_mirroring.load() && g_haveClientAddress)
            {
                g_mirroring.store(true);
                printf("coop: join: the host's packets are coming in: mirroring the host from now\n");
                fflush(stdout);
            }
            if (!g_mirroring.load() || !buffer) continue;
            memcpy(base + buffer, packet + 4, length);
            PPCContext call = ctx;
            call.r1.u32 = (ctx.r1.u32 - 0x400) & ~0xFu;
            call.r3.u64 = 4; call.r4.u64 = length; call.r5.u64 = buffer;
            call.r6.u64 = g_clientAddress[0]; call.r7.u64 = g_clientAddress[1]; call.r8.u64 = g_clientAddress[2];
            call.lr = 0;
            __imp__sub_82451C68(call, base);
            if (++g_injected % 500 == 1) { printf("coop: join: %llu of the host's packets given to the client\n", (unsigned long long)g_injected); fflush(stdout); }
        }
    }
    __imp__sub_82535E80(ctx, base);
}

// ---- a mission's film, together (0.5.231) ----
// Test 3: the host skipped the film and the joining player was left in his,
// a mission behind. Now (title_fixes.cpp asks here, inside the film's loop):
// - the host skips with A as before, but only once the joining copy has
//   loaded too (CWR1, which it sends from its own film); until then a press
//   shows "waiting for your friend". (60 s at most, or the friend gone.)
// - when the host's film ends (skipped or played out) the joining copy is
//   told (CWF1) and its film ends there too; its own A does nothing.
// On UDP port + 4 (the joining copy) and + 5 (the host), read here.
namespace
{
    SOCKET g_film = INVALID_SOCKET;
    bool OpenFilm()
    {
        if (g_film != INVALID_SOCKET) return true;
        static double lastTry = -10;
        if (Seconds() - lastTry < 1) return false;
        lastTry = Seconds();
        if (!OpenWorld()) return false;   // (Winsock started there)
        g_film = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (g_film == INVALID_SOCKET) return false;
        u_long nb = 1; ioctlsocket(g_film, FIONBIO, &nb);
        // the joining copy listens on port + 4, the host on port + 5 (so two
        // copies on one PC can be tried too)
        const int mine = Port() + (CoopMode() == Mode::Host ? 5 : 4);
        sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons(uint16_t(mine)); a.sin_addr.s_addr = INADDR_ANY;
        if (bind(g_film, reinterpret_cast<sockaddr*>(&a), sizeof a) != 0) { printf("coop: film: port %d is in use\n", mine); fflush(stdout); closesocket(g_film); g_film = INVALID_SOCKET; return false; }
        return true;
    }
    // tagged with the host's count, so one left over from another mission is not taken
    void FilmSend(const char* tag)
    {
        if (!OpenFilm()) return;
        char m[8]; memcpy(m, tag, 4);
        const uint32_t epoch = CoopMode() == Mode::Host ? g_hostEpoch : g_lastHostEpoch;
        memcpy(m + 4, &epoch, 4);
        sockaddr_in to{}; to.sin_family = AF_INET; to.sin_port = htons(uint16_t(Port() + (CoopMode() == Mode::Host ? 4 : 5)));
        inet_pton(AF_INET, OtherPc().c_str(), &to.sin_addr);
        for (int k = 0; k < 3; k++) sendto(g_film, m, 8, 0, reinterpret_cast<sockaddr*>(&to), sizeof to);
    }
    void FilmPoll()
    {
        if (!OpenFilm()) return;
        char m[16];
        for (int n = 0; n < 32; n++)
        {
            const int got = recv(g_film, m, sizeof m, 0);
            if (got < 8) break;
            uint32_t epoch; memcpy(&epoch, m + 4, 4);
            const uint32_t mine = CoopMode() == Mode::Host ? g_hostEpoch : g_lastHostEpoch;
            if (epoch != mine) { printf("coop: film: %.4s for count %u left (ours is %u)\n", m, epoch, mine); fflush(stdout); continue; }
            if (memcmp(m, "CWF1", 4) == 0 && CoopMode() == Mode::Join && !g_filmSkipFromHost) { g_filmSkipFromHost = true; printf("coop: film: the host's film is over\n"); fflush(stdout); }
            if (memcmp(m, "CWR1", 4) == 0 && CoopMode() == Mode::Host && !g_filmPeerReady) { g_filmPeerReady = true; printf("coop: film: the joining copy has loaded\n"); fflush(stdout); }
        }
    }
}

bool CoopFilmActive() { return CoopMode() != Mode::Off; }
// asked each pass of the film's loop: whether to stop now (pressed: A was)
bool CoopFilmStop(bool pressed)
{
    static double filmAt = -1, lastReady = -1;
    static uint32_t filmGen = 0;
    const double now = Seconds();
    if (filmGen != g_mapGen || filmAt < 0)
    {
        filmGen = g_mapGen; filmAt = now;
        printf("coop: film: playing (mission %s, the host's count %u)\n", g_myMap.c_str(), CoopMode() == Mode::Host ? g_hostEpoch : g_lastHostEpoch); fflush(stdout);
    }
    FilmPoll();
    if (CoopMode() == Mode::Join)
    {
        if (now - lastReady > 0.5) { lastReady = now; FilmSend("CWR1"); }
        // (co-op v36) never stuck: after 15 s of film the joining player's own press ends it
        if (pressed && now - filmAt > 15) { printf("coop: film: skipped here (no word from the host after %.0f s)\n", now - filmAt); fflush(stdout); return true; }
        if (pressed) { static double toldAt = -10; if (now - toldAt > 5) { toldAt = now; Overlay::Notice("Co-op: the host skips the film for both of you (or press again after 15 s).", 3000); } }
        return g_filmSkipFromHost;
    }
    if (!pressed) return false;
    if (g_filmPeerReady || !g_peerToldConnected || now - filmAt > 60) return true;
    if (!g_filmToldWaiting) { g_filmToldWaiting = true; printf("coop: film: skip held: the joining copy is still loading\n"); fflush(stdout); }
    static double toldAt = -10;
    if (now - toldAt > 3) { toldAt = now; Overlay::Notice("Co-op: waiting for your friend to finish loading...", 3000); }
    return false;
}
// the film's loop is over here (skipped or played out)
void CoopFilmEnded()
{
    if (CoopMode() == Mode::Host) { FilmSend("CWF1"); printf("coop: film: over here: the joining copy is told\n"); fflush(stdout); }
    else if (CoopMode() == Mode::Join) FilmSend("CWR1");
}

// (0.5.236) kernel_xam.cpp, just before the title starts itself again for
// its next mission (or a retry): the host tells the joining copy, which
// then follows him into whatever he starts next, the same mission too.
void CoopBeforeRelaunch()
{
    if (CoopMode() != Mode::Host || g_world == INVALID_SOCKET) return;
    for (int k = 0; k < 5; k++) sendto(g_world, "CWX1", 4, 0, reinterpret_cast<sockaddr*>(&g_worldTo), sizeof(g_worldTo));
    printf("coop: lobby: starting again for the next mission: the joining copy is told\n"); fflush(stdout);
}

#endif
