// Co-op v40 (Team Friend, Claude session, 2026-10-29): the campaign played
// the World at War way: one world, the host's, and the joining player a real
// second player in it. The design: docs/COOP-V40-DESIGN.md. This file: the
// mode, the network (one UDP socket each, unreliable messages and a reliable
// ordered channel), the lobby (the joining copy follows the host into his
// mission), the shared film, the hooks into the game's frame, and the
// campaign's sprint (kept from the co-op of 0.5.237 on: it is not co-op only).
//
// COD3_COOP=host | join     (off otherwise: the campaign as ever)
// COD3_COOP_PORT=n          the host listens on n (28970), the joining copy on n+1
// COD3_COOP_HOSTIP=a.b.c.d  the joining copy: the host (else the first System
//                           Link peer of the F11 settings, else this PC)
// COD3_COOP_MAP=name        the host: start that mission 3 s after the friend is in
// COD3_COOP_SHOTS="t t"     screenshots at those seconds (tests)
// Everything is logged with "coop:" at the line's start.
#ifndef COD3_MP

#include "coop.h"
#include "overlay.h"
#include "input.h"
#include "settings.h"
#include "heap_trace.h"

#include <winsock2.h>
#include <ws2tcpip.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include <unordered_set>
#include <thread>

extern "C" PPC_FUNC(__imp__sub_82451C68);   // NET_SendLoopPacket
extern "C" PPC_FUNC(__imp__sub_82535E80);   // Com_EventLoop: once a frame
extern "C" PPC_FUNC(__imp__sub_82517A00);   // SV_SpawnServer: a mission starting
extern "C" PPC_FUNC(__imp__sub_82512338);   // the server's client think (client 0)
extern "C" PPC_FUNC(__imp__sub_8252B850);   // a dvar set by its name

namespace Coop
{
    // ---- the basics -------------------------------------------------------
    // the mode: COD3_COOP=host|join from the start (the launchers, the tests),
    // else what the lobby (F2, coop_lobby.cpp) chooses
    namespace { std::atomic<int> g_modeNow{ -1 }; }
    Mode GetMode()
    {
        int m = g_modeNow.load(std::memory_order_relaxed);
        if (m < 0)
        {
            const char* t = getenv("COD3_COOP");
            m = !t ? 0 : _stricmp(t, "host") == 0 ? 1 : _stricmp(t, "join") == 0 ? 2 : 0;
            g_modeNow.store(m);
        }
        return Mode(m);
    }
    double Now()
    {
        static const auto start = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    }
    int32_t IndexOf(const uint8_t* base, uint32_t e)
    {
        if (!e) return -1;
        const uint32_t n = Guest::Read32(base, e + 500) & 0x3FFF;
        if (n < 4096 && EntityAt(base, n) == e) return int32_t(n);
        for (uint32_t i = 0; i < 4096; i++) if (EntityAt(base, i) == e) return int32_t(i);
        return -1;
    }
    std::string Text(const uint8_t* base, uint32_t a, uint32_t max)
    {
        std::string s;
        if (!(Valid(a) || (a >= 0x82000000u && a < 0x83000000u))) return s;
        for (uint32_t i = 0; i < max; i++) { const uint8_t c = base[a + i]; if (c < 32 || c >= 127) break; s += char(c); }
        return s;
    }
    std::string BrocText(const uint8_t* base, uint32_t field)
    {
        const uint32_t b = Guest::Read32(base, field);
        return Valid(b) ? Text(base, b + 12) : std::string();
    }
    uint32_t ModelSlot(const uint8_t* base, uint32_t e, int slot)
    {
        if (slot == 0) return Guest::Read32(base, e + 556);
        const uint32_t m = Guest::Read32(base, e + 984);
        static const uint32_t at[4] = { 0, 0, 8, 4 };   // G_SetModel: slot 1 at +0, 2 at +8, 3 at +4
        return Valid(m) && slot < 4 ? Guest::Read32(base, m + at[slot]) : 0;
    }
    std::string ResName(const uint8_t* base, uint32_t res) { return Valid(res) ? Text(base, Guest::Read32(base, res + 72)) : std::string(); }
    // guest memory for co-op's small things, from 1 MB blocks (AllocatePhysical hands out 64 KB at least:
    // a block each was 64 KB a text, and the joining copy ran out of memory)
    uint32_t GuestAlloc(uint32_t size)
    {
        static uint32_t block = 0, used = 0;
        constexpr uint32_t BlockSize = 1u << 20;
        size = (size + 15u) & ~15u;
        if (size > BlockSize) return Guest::AllocatePhysical(size);
        if (!block || used + size > BlockSize) { block = Guest::AllocatePhysical(BlockSize); used = 0; }
        const uint32_t a = block + used;
        used += size;
        return a;
    }
    uint32_t GuestText(uint8_t* base, const std::string& text)
    {
        static std::map<std::string, uint32_t> kept;
        const auto it = kept.find(text);
        if (it != kept.end()) return it->second;
        const uint32_t a = GuestAlloc(uint32_t(text.size() + 16));
        memcpy(base + a, text.c_str(), text.size() + 1);
        kept[text] = a;
        return a;
    }
    uint32_t GuestBroc(uint8_t* base, const std::string& text)
    {
        static std::map<std::string, uint32_t> kept;
        const auto it = kept.find(text);
        if (it != kept.end()) return it->second;
        const uint32_t block = GuestAlloc(uint32_t(16 + text.size() + 1));
        memset(base + block, 0, 16 + text.size() + 1);
        Guest::Write16(base, block + 8, 0x4000);   // its count, held high: never freed
        memcpy(base + block + 12, text.c_str(), text.size() + 1);
        const uint32_t slot = GuestAlloc(16);
        Guest::Write32(base, slot, block);
        kept[text] = slot;
        return slot;
    }
    uint32_t Scratch(uint8_t* base)
    {
        static uint32_t s = 0;
        if (!s) { s = GuestAlloc(512); memset(base + s, 0, 512); }
        return s;
    }
    PPCContext CallCtx(PPCContext& ctx) { PPCContext c = ctx; c.r1.u32 = (ctx.r1.u32 - 0x1000) & ~0xFu; c.lr = 0; return c; }
    void CallGuest(PPCContext& c, uint8_t* base, uint32_t function) { if (PPCFunc* f = Guest::Lookup(function)) f(c, base); }
    uint32_t Me(const uint8_t* base)
    {
        for (const uint32_t i : { 1u, 0u })
            if (const uint32_t e = EntityAt(base, i); e && Guest::Read32(base, e + 528) && base[e] == 1) return e;
        return 0;
    }
    bool InMission(const uint8_t* base) { return Me(base) != 0; }
    void DeleteEntity(PPCContext& ctx, uint8_t* base, uint32_t index)
    {
        PPCContext c = CallCtx(ctx);
        c.r3.u64 = RefOf(base, index);
        CallGuest(c, base, 0x824A7280);   // the scripts' delete
    }
    // (v43n diagnostic) the link in progress: a link that never returns (the joining copy at 0 fps in the
    // world's area tree, v43n/v43o) names its entity from a watcher thread
    namespace
    {
        std::atomic<uint32_t> g_linkEnt{ 0 };
        std::atomic<double> g_linkAt{ 0 };
        struct LinkInfo { uint32_t e; uint8_t type; float o[3], mins[3], maxs[3]; uint32_t flags, contents, linked; char cls[48]; char model[48]; uint32_t words[32]; };
        LinkInfo g_linkInfo{};
        std::mutex g_linkMutex;
        void LinkWatcher(const uint8_t* base)
        {
            static std::atomic<bool> started{ false };
            if (started.exchange(true)) return;
            std::thread([base] {
                uint32_t told = 0;
                for (;;)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    const uint32_t e = g_linkEnt.load();
                    if (!e || e == told || Now() - g_linkAt.load() < 3.0) continue;
                    told = e;
                    LinkInfo i; { std::lock_guard<std::mutex> l(g_linkMutex); i = g_linkInfo; }
                    printf("coop: STUCK linking entity %08X (type %u, %s) at %g %g %g, mins %g %g %g maxs %g %g %g, flags %08X contents %08X linked %08X, for %.1f s\n",
                        i.e, i.type, i.cls, i.o[0], i.o[1], i.o[2], i.mins[0], i.mins[1], i.mins[2], i.maxs[0], i.maxs[1], i.maxs[2], i.flags, i.contents, i.linked, Now() - g_linkAt.load());
                    printf("coop: STUCK entity's model %s; its first 128 bytes:", i.model);
                    for (int k = 0; k < 32; k++) printf("%s%08X", k % 8 ? " " : "\n   ", i.words[k]);
                    printf("\n"); fflush(stdout);
                }
            }).detach();
            (void)base;
        }
    }
    void LinkEntity(PPCContext& ctx, uint8_t* base, uint32_t e)
    {
        LinkWatcher(base);
        {
            std::lock_guard<std::mutex> l(g_linkMutex);
            g_linkInfo.e = e; g_linkInfo.type = base[e];
            for (int k = 0; k < 3; k++) { g_linkInfo.o[k] = F(base, e + 336 + 4 * k); g_linkInfo.mins[k] = F(base, e + 296 + 4 * k); g_linkInfo.maxs[k] = F(base, e + 308 + 4 * k); }
            g_linkInfo.flags = Guest::Read32(base, e + 632); g_linkInfo.contents = Guest::Read32(base, e + 324); g_linkInfo.linked = Guest::Read32(base, e + 100);
            snprintf(g_linkInfo.cls, sizeof g_linkInfo.cls, "%s", BrocText(base, e + 564).c_str());
            snprintf(g_linkInfo.model, sizeof g_linkInfo.model, "%s", ResName(base, ModelSlot(base, e, 0)).c_str());
            for (int k = 0; k < 32; k++) g_linkInfo.words[k] = Guest::Read32(base, e + 4 * k);
        }
        g_linkAt = Now(); g_linkEnt = e;
        PPCContext c = CallCtx(ctx);
        c.r3.u64 = e;
        CallGuest(c, base, 0x82513198);
        g_linkEnt = 0;
    }
    void Notice(const std::string& text, int ms) { Overlay::Notice(text, ms); }

    // ---- the network ------------------------------------------------------
    namespace
    {
        // the other side's clock (as co-op v35's): what our time minus its is,
        // learnt quickly from a faster packet, slowly from a slower one
        struct NetClock
        {
            bool have = false;
            double offset = 0, jitter = 0.02;
            void Feed(double local, double remote)
            {
                const double sample = local - remote;
                if (!have || fabs(sample - offset) > 5.0) { have = true; offset = sample; jitter = 0.02; return; }
                const double d = sample - offset;
                offset += d < 0 ? d * 0.5 : d * 0.01;
                jitter += (fabs(d) - jitter) * 0.05;
            }
            double Delay() const
            {
                static const double base = [] { const char* t = getenv("COD3_COOP_DELAY"); return t ? std::clamp(atof(t), 0.0, 1.0) : 0.1; }();
                return std::clamp(base + 1.5 * jitter, base, base + 0.25);
            }
        };
        struct OutMessage { uint32_t seq; std::vector<uint8_t> bytes; };
        SOCKET g_sock = INVALID_SOCKET;
        sockaddr_in g_peer{};
        bool g_havePeer = false;
        double g_peerSeen = -1;
        uint32_t g_mySession = 0, g_peerSession = 0;
        NetClock g_clock;
        std::deque<OutMessage> g_relOut;
        uint32_t g_relNext = 0;       // our next reliable message's number
        uint32_t g_relExpect = 0;     // the next of the other side's we want
        double g_relSentAt = -1;
        double g_packetTime = 0;
        bool g_relDirty = false;
        uint64_t g_bytesOut = 0, g_bytesIn = 0, g_packetsOut = 0, g_packetsIn = 0;
        std::string g_myMap;
        uint32_t g_missionCount = 0;
        std::string g_hostAddress;    // the joining copy: the lobby's choice (else COD3_COOP_HOSTIP, the settings' peers)
        int Port() { static const int p = [] { const char* t = getenv("COD3_COOP_PORT"); return t ? atoi(t) : 28970; }(); return p; }
        std::string HostAddress()
        {
            if (!g_hostAddress.empty()) return g_hostAddress;
            const char* e = getenv("COD3_COOP_HOSTIP");
            std::string v = e ? e : "";
            if (v.empty())
            {
                std::string peers = Settings::Get().systemLinkPeers;
                for (char& c : peers) if (c == ',' || c == ';') c = ' ';
                const size_t a = peers.find_first_not_of(' ');
                if (a != std::string::npos) { const size_t b = peers.find(' ', a); v = peers.substr(a, b == std::string::npos ? std::string::npos : b - a); }
            }
            return v.empty() ? "127.0.0.1" : v;
        }
        void Put(std::vector<uint8_t>& out, const void* p, size_t n) { const uint8_t* b = static_cast<const uint8_t*>(p); out.insert(out.end(), b, b + n); }
        void SendPacket(Kind kind, const void* data, size_t size)
        {
            if (g_sock == INVALID_SOCKET || !g_havePeer) return;
            uint8_t packet[1400];
            if (size + 24 > sizeof packet) return;
            memcpy(packet, "CO40", 4);
            packet[4] = kind; packet[5] = 0; packet[6] = packet[7] = 0;
            memcpy(packet + 8, &g_mySession, 4);
            memcpy(packet + 12, &g_relExpect, 4);
            const double t = Now();
            memcpy(packet + 16, &t, 8);
            if (size) memcpy(packet + 24, data, size);
            sendto(g_sock, reinterpret_cast<const char*>(packet), int(size + 24), 0, reinterpret_cast<const sockaddr*>(&g_peer), sizeof g_peer);
            g_bytesOut += size + 24; g_packetsOut++;
        }
        // the reliable messages not yet acknowledged, oldest first, as many packets as it takes (8 at most a call)
        void FlushReliable(bool force)
        {
            if (g_relOut.empty() || !g_havePeer) return;
            const double now = Now();
            if (!force && !g_relDirty && now - g_relSentAt < 0.1) return;
            g_relSentAt = now; g_relDirty = false;
            size_t i = 0;
            for (int packets = 0; packets < 8 && i < g_relOut.size(); packets++)
            {
                std::vector<uint8_t> p;
                while (i < g_relOut.size() && p.size() + g_relOut[i].bytes.size() <= 1150) { Put(p, g_relOut[i].bytes.data(), g_relOut[i].bytes.size()); i++; }
                if (p.empty()) { i++; continue; }   // (a message too big: never: SendReliable cuts them)
                SendPacket(K_REL, p.data(), p.size());
            }
        }
        void ResetSession(uint32_t session)
        {
            printf("coop: net: the other side is a new session (%08X, was %08X): reliable channel started again\n", session, g_peerSession); fflush(stdout);
            if (IsHost()) HostNewPeer();   // (v45b) a joining copy started again: everything told again
            g_peerSession = session;
            g_relExpect = 0;
            g_relOut.clear();
            g_relNext = 0;
            g_clock = NetClock{};
        }
    }
    int NetPort() { return Port(); }
    // the lobby's choice of mode (the game thread, outside a mission): the socket closed
    // and everything of the old session forgotten; NetOpen opens the new one
    void SetMode(Mode mode, const std::string& hostAddress)
    {
        if (g_sock != INVALID_SOCKET) { closesocket(g_sock); g_sock = INVALID_SOCKET; }
        g_havePeer = false; g_peerSeen = -1; g_peerSession = 0;
        g_relOut.clear(); g_relNext = 0; g_relExpect = 0; g_relDirty = false;
        g_clock = NetClock{};
        g_hostAddress = mode == Mode::Join ? hostAddress : std::string();
        g_modeNow.store(int(mode));
        // (the process the title starts for its next mission keeps the choice)
        SetEnvironmentVariableA("COD3_COOP", mode == Mode::Host ? "host" : mode == Mode::Join ? "join" : nullptr);
        SetEnvironmentVariableA("COD3_COOP_HOSTIP", mode == Mode::Join ? hostAddress.c_str() : nullptr);
        _putenv_s("COD3_COOP", mode == Mode::Host ? "host" : mode == Mode::Join ? "join" : "");
        _putenv_s("COD3_COOP_HOSTIP", mode == Mode::Join ? hostAddress.c_str() : "");
        printf("coop: lobby: co-op is now %s%s%s\n", mode == Mode::Host ? "hosting" : mode == Mode::Join ? "joining " : "off", mode == Mode::Join ? hostAddress.c_str() : "", ""); fflush(stdout);
    }
    void NetForgetPeer() { if (IsHost()) g_havePeer = false; g_peerSeen = -1; }
    std::string& MyMap() { return g_myMap; }
    uint32_t MissionCount() { return g_missionCount; }
    uint32_t& MissionCounter() { return g_missionCount; }
    bool PeerConnected() { return g_havePeer && g_peerSeen >= 0 && Now() - g_peerSeen < 10.0; }
    double PeerSeenAt() { return g_peerSeen; }
    double PeerTimeNow() { return Now() - g_clock.offset; }
    double ShowTime() { return Now() - g_clock.offset - g_clock.Delay(); }
    double PacketTime() { return g_packetTime; }
    bool g_silenced = false;
    void NetSilence() { if (g_sock != INVALID_SOCKET) { closesocket(g_sock); g_sock = INVALID_SOCKET; } g_silenced = true; }
    bool Swapping() { return g_silenced; }
    bool NetOpen()
    {
        if (g_silenced) return false;
        if (g_sock != INVALID_SOCKET) return true;
        static double lastTry = -10;
        if (Now() - lastTry < 1.0) return false;
        lastTry = Now();
        static bool started = false;
        if (!started) { started = true; WSADATA d; WSAStartup(MAKEWORD(2, 2), &d); g_mySession = (GetTickCount() * 2654435761u) ^ GetCurrentProcessId(); if (!g_mySession) g_mySession = 1; }
        SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (s == INVALID_SOCKET) return false;
        u_long nb = 1; ioctlsocket(s, FIONBIO, &nb);
        BOOL yes = TRUE;
        setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof yes);
        int big = 4 << 20;
        setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&big), sizeof big);
        setsockopt(s, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&big), sizeof big);
        sockaddr_in me{}; me.sin_family = AF_INET; me.sin_addr.s_addr = htonl(INADDR_ANY);
        me.sin_port = htons(uint16_t(Port() + (IsJoin() ? 1 : 0)));
        if (bind(s, reinterpret_cast<sockaddr*>(&me), sizeof me) != 0)
        {
            // (a process the title relaunched can start before the old one lets the port go)
            printf("coop: net: port %d is busy (%d): trying again\n", Port() + (IsJoin() ? 1 : 0), WSAGetLastError()); fflush(stdout);
            closesocket(s);
            return false;
        }
        g_sock = s;
        if (IsJoin())
        {
            g_peer.sin_family = AF_INET; g_peer.sin_port = htons(uint16_t(Port()));
            inet_pton(AF_INET, HostAddress().c_str(), &g_peer.sin_addr);
            g_havePeer = true;
            printf("coop: net: joining the host at %s:%d (UDP %d here), session %08X\n", HostAddress().c_str(), Port(), Port() + 1, g_mySession);
        }
        else printf("coop: net: hosting on UDP %d, session %08X\n", Port(), g_mySession);
        fflush(stdout);
        return true;
    }
    void SendUnreliable(Kind kind, const void* data, size_t size) { SendPacket(kind, data, size); }
    void SendReliable(RelKind kind, const void* data, size_t size)
    {
        // (a message is at most 1100 bytes; a longer one is cut: none is, by design)
        size = std::min<size_t>(size, 1100);
        OutMessage m;
        m.seq = g_relNext++;
        const uint16_t len = uint16_t(size);
        Put(m.bytes, &m.seq, 4); m.bytes.push_back(kind); Put(m.bytes, &len, 2);
        if (size) Put(m.bytes, data, size);
        g_relOut.push_back(std::move(m));
        g_relDirty = true;
        if (g_relOut.size() > 20000) { printf("coop: net: 20000 reliable messages unanswered: the other side is gone; dropped\n"); fflush(stdout); g_relOut.clear(); }
    }
    void FilmReceive(const uint8_t* data, size_t size);
    void NetPoll(PPCContext& ctx, uint8_t* base, bool full)
    {
        if (!NetOpen()) return;
        uint8_t p[1500];
        for (int n = 0; n < 256; n++)
        {
            sockaddr_in from{}; int fromLen = sizeof from;
            const int got = recvfrom(g_sock, reinterpret_cast<char*>(p), sizeof p, 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
            if (got < 24) break;
            if (memcmp(p, "CO40", 4) != 0) continue;
            g_bytesIn += uint32_t(got); g_packetsIn++;
            uint32_t session, ack; double t;
            memcpy(&session, p + 8, 4); memcpy(&ack, p + 12, 4); memcpy(&t, p + 16, 8);
            if (IsHost())
            {
                if (!g_havePeer || from.sin_addr.s_addr != g_peer.sin_addr.s_addr || from.sin_port != g_peer.sin_port)
                {
                    char a[64]; inet_ntop(AF_INET, &from.sin_addr, a, sizeof a);
                    printf("coop: net: the joining player is at %s:%d\n", a, ntohs(from.sin_port)); fflush(stdout);
                }
                g_peer = from; g_havePeer = true;
            }
            if (session != g_peerSession) ResetSession(session);
            g_peerSeen = Now();
            g_clock.Feed(Now(), t);
            g_packetTime = t;
            while (!g_relOut.empty() && int32_t(g_relOut.front().seq - ack) < 0) g_relOut.pop_front();
            const Kind kind = Kind(p[4]);
            const uint8_t* data = p + 24;
            const size_t size = size_t(got) - 24;
            if (LobbyReceive(base, kind, data, size)) continue;
            if (kind == K_FILM) { FilmReceive(data, size); continue; }
            if (!full) continue;   // (in a film: the rest waits, the reliable ones come again)
            if (kind == K_REL)
            {
                size_t at = 0;
                while (at + 7 <= size)
                {
                    uint32_t seq; uint16_t len;
                    memcpy(&seq, data + at, 4); const RelKind rk = RelKind(data[at + 4]); memcpy(&len, data + at + 5, 2);
                    if (at + 7 + len > size) break;
                    const uint8_t* msg = data + at + 7;
                    at += 7 + len;
                    if (seq != g_relExpect) continue;   // (an old one again, or one after a lost one: it comes again)
                    g_relExpect++;
                    if (rk == R_LASTSTAND || rk == R_DOWN || rk == R_HIT || rk == R_MISSION) LastStandReceive(ctx, base, rk, msg, len);
                    if (IsHost()) HostReceiveReliable(ctx, base, rk, msg, len);
                    else JoinReceiveReliable(ctx, base, rk, msg, len);
                }
                continue;
            }
            if (IsHost()) HostReceive(ctx, base, kind, data, size);
            else JoinReceive(ctx, base, kind, data, size);
        }
        FlushReliable(false);
        static double statsAt = 0;
        if (Now() - statsAt > 30)
        {
            statsAt = Now();
            printf("coop: net: %llu packets / %llu KB out, %llu / %llu KB in, %zu reliable unanswered, clock offset %.3f jitter %.3f\n",
                   (unsigned long long)g_packetsOut, (unsigned long long)(g_bytesOut >> 10), (unsigned long long)g_packetsIn, (unsigned long long)(g_bytesIn >> 10),
                   g_relOut.size(), g_clock.offset, g_clock.jitter);
            fflush(stdout);
        }
    }
}

using namespace Coop;

// ---- the lobby: coop_lobby.cpp ------------------------------------------------

// ---- the film of a mission, together -------------------------------------
// (as co-op 0.5.231 did it) The host skips with A only once the joining copy
// has loaded too (60 s at most); the joining copy's film ends when the
// host's does. The film's loop runs instead of the frame, so the network is
// read here too.
namespace
{
    bool g_filmPeerReady = false, g_filmOver = false;
    uint32_t g_filmCount = 0;
    void FilmSend(char what)
    {
        uint8_t m[8] = { uint8_t(what) };
        const uint32_t count = MissionCount();
        memcpy(m + 4, &count, 4);
        SendUnreliable(K_FILM, m, 8);
    }
}
namespace Coop
{
    void FilmReceive(const uint8_t* data, size_t size)
    {
        if (size < 8) return;
        if (data[0] == 'R' && IsHost() && !g_filmPeerReady) { g_filmPeerReady = true; printf("coop: film: the joining copy has loaded\n"); fflush(stdout); }
        if (data[0] == 'F' && IsJoin() && !g_filmOver) { g_filmOver = true; printf("coop: film: the host's film is over\n"); fflush(stdout); }
    }
}
bool CoopFilmActive() { return GetMode() != Mode::Off; }
bool CoopFilmStop(bool pressed)
{
    static double filmAt = -1, lastSaid = -1;
    const double now = Now();
    if (g_filmCount != MissionCount() + 1 || filmAt < 0) { g_filmCount = MissionCount() + 1; filmAt = now; g_filmOver = false; printf("coop: film: playing (%s)\n", MyMap().c_str()); fflush(stdout); }
    static PPCContext none{};
    if (NetOpen()) NetPoll(none, Guest::Base, false);   // (lobby and film messages only: no guest calls here)
    if (IsJoin())
    {
        if (now - lastSaid > 0.5) { lastSaid = now; FilmSend('R'); }
        if (pressed && now - filmAt > 15) { printf("coop: film: skipped here (no word from the host after %.0f s)\n", now - filmAt); fflush(stdout); return true; }
        if (pressed) { static double toldAt = -10; if (now - toldAt > 5) { toldAt = now; Notice("Co-op: the host skips the film for both of you (or press again after 15 s).", 3000); } }
        return g_filmOver;
    }
    if (!pressed) return false;
    if (g_filmPeerReady || !PeerConnected() || now - filmAt > 60) return true;
    static double toldAt = -10;
    if (now - toldAt > 3) { toldAt = now; Notice("Co-op: waiting for your friend to finish loading...", 3000); }
    return false;
}
void CoopFilmEnded()
{
    if (IsHost()) { for (int k = 0; k < 3; k++) FilmSend('F'); printf("coop: film: over here: the joining copy is told\n"); fflush(stdout); }
    else if (IsJoin()) FilmSend('R');
}
// kernel_xam.cpp, before the title starts itself again (the next mission):
// the joining copy hears of the next mission from the new process.
void CoopBeforeRelaunch()
{
    if (GetMode() == Mode::Off) return;
    printf("coop: lobby: this copy starts itself again for the next mission\n"); fflush(stdout);
}

// ---- the campaign's sprint (kept from co-op 0.5.237 / v35) ------------------
// The campaign has the engine's "+sprint" but does nothing with it; while
// sprint is held (tap to sprint, input.cpp) going forward, standing and not
// aiming, g_speed is raised to 1.5 times over a quarter second, for 4 s of
// stamina (COD3_SPRINT_SECONDS), then 1.5 s out of breath; stamina comes back
// from empty in 3 s after 0.75 s. COD3_NOSPRINT=1 turns it off.
namespace
{
    uint32_t g_mapGen = 1;
    void SprintPoll(const uint8_t* base)
    {
        // 0.5.601 (kept in the v40 merge, 0.5.622): the game's own sprint is back
        // (sprint.cpp): this one only with COD3_OLDSPRINT=1 (or COD3_NOREALSPRINT=1,
        // the game's left empty)
        static const bool off = getenv("COD3_NOSPRINT") != nullptr ||
                                (getenv("COD3_OLDSPRINT") == nullptr && getenv("COD3_NOREALSPRINT") == nullptr);
        static const double length = [] { const char* t = getenv("COD3_SPRINT_SECONDS"); return t ? std::clamp(atof(t), 1.0, 60.0) : 4.0; }();
        static bool on = false;
        static int32_t normal = 190, written = 190;
        static uint32_t gen = 0;
        static float stamina = 1.0f;
        static double lastAt = -1, onSince = -1, stoppedAt = -1, windedUntil = -1;
        const double now = Now();
        const float dt = lastAt < 0 ? 0.0f : float(std::clamp(now - lastAt, 0.0, 0.1));
        lastAt = now;
        const uint32_t me = Me(base);
        const uint32_t rec = Record(base, me);
        const bool inGame = rec != 0;
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
            if (stamina <= 0.0f) { stamina = 0.0f; windedUntil = now + 1.5; want = false; }
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
        }
        else if (!want && on) { on = false; stoppedAt = now; speed(normal); }
        if (on)
        {
            const double ramp = std::min(1.0, (now - onSince) / 0.25);
            const int32_t step = int32_t(ramp * 5.0 + 0.5);
            speed(normal + normal * step / 10);
        }
    }
    // COD3_COOP_SHOTS="60 90": screenshots at those seconds (scripted tests)
    void Shots()
    {
        static const char* const shots = getenv("COD3_COOP_SHOTS");
        if (!shots) return;
        static std::vector<double> at;
        static bool parsed = false;
        if (!parsed) { parsed = true; for (const char* t = shots; *t;) { char* end; const double v = strtod(t, &end); if (end == t) break; at.push_back(v); t = end; } }
        const double now = Now();
        for (size_t i = 0; i < at.size(); i++)
            if (now >= at[i]) { Overlay::RequestScreenshot(); printf("coop: screenshot at %.1f s\n", now); fflush(stdout); at.erase(at.begin() + long(i)); break; }
    }
}

// ---- the hooks ---------------------------------------------------------------
void NativeOptionsPoll(uint8_t* base);              // native_options.cpp
void Probe40Frame(PPCContext& ctx, uint8_t* base);  // coop_probe.cpp (diagnostic)

// Com_EventLoop: once a frame, on the title's own thread
PPC_FUNC(sub_82535E80)
{
    Probe40Frame(ctx, base);
    HeapTrace::SetGuard(GetMode() == Mode::Join);   // (v45k)
    NativeOptionsPoll(base);
    SprintPoll(base);
    Shots();
    LobbyFrame(ctx, base);
    SwapFrame();
    if (GetMode() != Mode::Off && NetOpen())
    {
        // (a process the title relaunched for its next mission: its messages to a log of their own)
        static bool reopened = false;
        if (!reopened) { reopened = true;
            if (const char* r = getenv("COD3_COOP_RELOAD"); r && r[0] && !IsHost()) { freopen("coop-join-reload.log", "a", stdout); printf("\ncoop: ---- started again for the host's checkpoint ----\n"); fflush(stdout); }
            if (getenv("COD3_LAUNCHDATA")) { freopen(IsHost() ? "coop-host-relaunch.log" : "coop-join-relaunch.log", "a", stdout); printf("\ncoop: ---- started again for the next mission ----\n"); fflush(stdout); } }
        NetPoll(ctx, base, true);
        Lobby(ctx, base);
        if (IsHost()) HostFrame(ctx, base);
        else JoinFrame(ctx, base);
        LastStandFrame(ctx, base);
        HudFrame(ctx, base);
    }
    __imp__sub_82535E80(ctx, base);
}

// a dvar set by its name (name, value, flags): the level scripts' setdvar (sub_824AB260,
// sub_824AB300, sub_824AB398: the letterbox, the gun and the HUD shown or not...) on the
// host are the joining copy's too (its own scripts are stopped)
void Probe40DvarSet(PPCContext& ctx, uint8_t* base);   // coop_probe.cpp (diagnostic)
PPC_FUNC(sub_8252B850)
{
    Probe40DvarSet(ctx, base);
    const uint32_t lr = uint32_t(ctx.lr);
    // (v46) DIAGNOSTIC (COD3_COOP_DVARLOG=1): every dvar set by name, each name and value once, for the pause
    if (static const bool log = getenv("COD3_COOP_DVARLOG") != nullptr; log && ctx.r3.u32 && ctx.r4.u32)
    {
        static std::set<std::string> seen;
        const std::string n = Text(base, ctx.r3.u32, 64), val = Text(base, ctx.r4.u32, 64);
        if (seen.size() < 3000 && seen.insert(n + "=" + val).second) { printf("coop: dvar set: %s = %s (from %08X)\n", n.c_str(), val.c_str(), lr); fflush(stdout); }
    }
    // (0.5.967) a test aid: COD3_COOP_FORCEDVAR="skipto=2;start=x": the level scripts' setdvar of that name takes this
    // value instead (a level's start point: the scripts set skipto "0" themselves at the start)
    if (lr >= 0x824AB260u && lr < 0x824AB478u && ctx.r3.u32 && ctx.r4.u32)
        if (const char* force = getenv("COD3_COOP_FORCEDVAR"))
        {
            const std::string n = Text(base, ctx.r3.u32, 64);
            std::string all(force); size_t at = 0;
            while (at <= all.size())
            {
                size_t end = all.find(';', at); if (end == std::string::npos) end = all.size();
                const std::string one = all.substr(at, end - at); at = end + 1;
                const size_t eq = one.find('=');
                if (eq != std::string::npos && _stricmp(one.substr(0, eq).c_str(), n.c_str()) == 0)
                {
                    printf("coop: test: the scripts' %s \"%s\" forced to \"%s\"\n", n.c_str(), Text(base, ctx.r4.u32, 64).c_str(), one.substr(eq + 1).c_str()); fflush(stdout);
                    ctx.r4.u64 = GuestText(base, one.substr(eq + 1));
                }
            }
        }
    if (GetMode() == Mode::Host && lr >= 0x824AB260u && lr < 0x824AB478u && ctx.r3.u32 && ctx.r4.u32)
        HostScriptDvar(Text(base, ctx.r3.u32, 64), Text(base, ctx.r4.u32, 256), ctx.r5.u32);
    if (GetMode() == Mode::Join && lr >= 0x824AB260u && lr < 0x824AB478u && ctx.r3.u32 && ctx.r4.u32)
        JoinOwnScriptDvar(Text(base, ctx.r3.u32, 64), Text(base, ctx.r4.u32, 256));
    __imp__sub_8252B850(ctx, base);
}

// (v46) SV_Frame (sub_82518E40): World at War's co-op does not stop for the pause menu. The game skips its
// frame while the local client's byte at 0x82A90A6A (+ the local client's index at 0x82A2A1C8) is set,
// which the pause menu sets: with two players in a mission it is let go here (only while the client is
// active, state 2 at 0x829C3F08: the game also sets it while loading). COD3_COOP_PAUSE=1 lets it pause.
extern "C" PPC_FUNC(__imp__sub_82518E40);
PPC_FUNC(sub_82518E40)
{
    static const bool allowPause = getenv("COD3_COOP_PAUSE") != nullptr;
    static double inSince = -1; static uint32_t inGen = 0xFFFFFFFFu;
    if (inGen != g_mapGen || !InMission(base)) { inGen = g_mapGen; inSince = Now(); }
    uint32_t held = 0xFFFFFFFFu; uint8_t was = 0;
    if (!allowPause && GetMode() != Mode::Off && PeerConnected() && InMission(base) && Now() - inSince > 10.0)   // (not the level's own first seconds)
    {
        const uint32_t index = Guest::Read32(base, 0x82A2A1C8);
        if (index < 4 && Guest::Read32(base, 0x829C3F08) == 2 && base[0x82A90A6Au + index] != 0)
        {
            // the menu's byte let go for the server's frame only, and put back after it: the menu stays open
            // (cleared for good, the pause menu closed at once)
            held = index; was = base[0x82A90A6Au + index];
            base[0x82A90A6Au + index] = 0;
            static int told = 0;
            if (told++ < 3) { printf("coop: the pause menu does not stop the mission in co-op (World at War's way)\n"); fflush(stdout); }
        }
    }
    __imp__sub_82518E40(ctx, base);
    if (held != 0xFFFFFFFFu && base[0x82A90A6Au + held] == 0) base[0x82A90A6Au + held] = was;
}

// SV_SpawnServer: a mission starting here (its name in r3)
PPC_FUNC(sub_82517A00)
{
    if (GetMode() != Mode::Off && ctx.r3.u32)
    {
        std::string name = Text(base, ctx.r3.u32, 63);
        MyMap() = name;
        g_mapGen++;
        printf("coop: lobby: this copy starts %s\n", name.c_str()); fflush(stdout);
        if (IsHost())
        {
            MissionCounter()++;
            HostMissionStart(name);
        }
        else JoinMissionStart(name);
        LastStandMissionStart();
    }
    __imp__sub_82517A00(ctx, base);
    if (GetMode() == Mode::Join) JoinMissionLoaded(base);
}

// NET_SendLoopPacket: the server's packets for client 1 (the host's second
// player) have nowhere to go: his copy draws from the snapshots of co-op v40
PPC_FUNC(sub_82451C68)
{
    if (GetMode() == Mode::Host)
    {
        const uint32_t port = uint32_t(ctx.r8.u64 >> 32) & 0xFFFF;
        if (port == 1 && ctx.r3.u32 == 4) return;
    }
    __imp__sub_82451C68(ctx, base);
}

// the server's client think (VM call 6 for client 0): client 1 thinks after him
PPC_FUNC(sub_82512338)
{
    __imp__sub_82512338(ctx, base);
    static const bool noThink = getenv("COD3_COOP_NOC1THINK") != nullptr;   // (a test: client 1 never thinks)
    if (GetMode() == Mode::Host && !noThink) HostClientThink(ctx, base);
}


static void CheckSectorList(uint8_t* base, uint32_t obj)
{
    using namespace Coop;
    {
        const uint32_t node = Guest::Read32(base, obj + 220);
        if (node && (Valid(node) || (node >= 0x82000000u && node < 0x83000000u)))
        {
            std::unordered_set<uint32_t> seen;
            uint32_t prev = 0, p = Guest::Read32(base, node + 16);
            while (p)
            {
                const bool loop = !(Valid(p) || (p >= 0x82000000u && p < 0x83000000u)) || seen.size() >= 4096 || seen.count(p);
                if (loop)
                {
                    static int told = 0;
                    if (told++ < 10) { printf("coop: a sector's list of entities closed on itself at %08X: cut\n", p); fflush(stdout); }
                    if (prev) Guest::Write32(base, prev + 224, 0); else Guest::Write32(base, node + 16, 0);
                    break;
                }
                seen.insert(p);
                prev = p; p = Guest::Read32(base, p + 224);
            }
        }
    }
}

// (v45i) the world's sector tree, entered by an entity's link (sub_82476250, from SV_LinkEntity). On the
// joining copy the tree came to have a loop (a child pointing back up), and the descent never ended: the copy
// froze in a link (Island, at 100..120 s, more than once). This is the same walk, written out, with each node
// remembered: a child seen before is cut from its parent (made the end marker) and the walk goes on.
extern "C" PPC_FUNC(__imp__sub_824646B8);
extern "C" PPC_FUNC(__imp__sub_8246F080);
extern "C" PPC_FUNC(__imp__sub_82476250);
PPC_FUNC(sub_82476250)
{
    using namespace Coop;
    if (GetMode() == Mode::Off) { __imp__sub_82476250(ctx, base); return; }
    const uint32_t obj = ctx.r3.u32, minsAt = ctx.r4.u32, maxsAt = ctx.r5.u32;
    const uint32_t bounds = Guest::Read32(base, 0x82A2ABC8);   // (lis -32093, -21560)
    constexpr uint32_t S = 0x829F1DE8;                         // (lis -32097, +7656)
    const uint32_t end = S + 172;
    const uint32_t mask = Guest::Read32(base, obj + 100);
    const uint32_t scratch = ctx.r1.u32 - 256;                  // (below the caller's frame, as the original's own)
    const uint32_t bmin = scratch + 88, bmax = scratch + 80;
    for (int restart = 0; restart < 8; restart++)
    {
        Guest::Write32(base, bmin, Guest::Read32(base, bounds + 60)); Guest::Write32(base, bmin + 4, Guest::Read32(base, bounds + 64));
        Guest::Write32(base, bmax, Guest::Read32(base, bounds + 72)); Guest::Write32(base, bmax + 4, Guest::Read32(base, bounds + 76));
        uint32_t node = S + 128;
        uint32_t seen[96]; int depth = 0;
        bool straddles = false;
        for (;;)
        {
            if (depth < 96) seen[depth++] = node;
            Guest::Write32(base, node + 8, Guest::Read32(base, node + 8) | mask);
            const uint32_t axis = Guest::Read32(base, node) & 3u;
            const float dist = F(base, node + 12);
            uint32_t childAt = 0;
            if (F(base, minsAt + 4 * axis) > dist) { SetF(base, bmin + 4 * axis, dist); childAt = node + 32; }
            else if (F(base, maxsAt + 4 * axis) < dist) { SetF(base, bmax + 4 * axis, dist); childAt = node + 36; }
            else { straddles = true; break; }
            const uint32_t child = Guest::Read32(base, childAt);
            if (child == end) break;
            bool loop = !Valid(child) && !(child >= 0x82000000u && child < 0x83000000u);
            for (int k = 0; k < depth && !loop; k++) if (seen[k] == child) loop = true;
            if (loop || depth >= 96)
            {
                static int told = 0;
                if (told++ < 10) { printf("coop: the world's sector tree had a loop (node %08X's child %08X): cut there\n", node, child); fflush(stdout); }
                Guest::Write32(base, childAt, end);
                break;
            }
            node = child;
        }
        const uint32_t cur = Guest::Read32(base, obj + 220);
        const bool subset = (Guest::Read32(base, obj + 228) & ~mask) == 0;
        auto keep = [&]() {
            Guest::Write32(base, obj + 228, mask);
            for (int k = 0; k < 2; k++) { Guest::Write32(base, obj + 232 + 4 * k, Guest::Read32(base, minsAt + 4 * k)); Guest::Write32(base, obj + 240 + 4 * k, Guest::Read32(base, maxsAt + 4 * k)); }
        };
        if (straddles && node == cur && subset) { keep(); return; }
        if (cur == 0)
        {
            // (v45p) not put in a list it is in already (that closed the list on itself)
            bool already = false;
            { int n = 0; for (uint32_t p = Guest::Read32(base, node + 16); p && n < 4096; p = Guest::Read32(base, p + 224), n++) if (p == obj) { already = true; break; } }
            Guest::Write32(base, obj + 220, node);
            if (!already) { Guest::Write32(base, obj + 224, Guest::Read32(base, node + 16)); Guest::Write32(base, node + 16, obj); }
            else { static int told = 0; if (told++ < 10) { printf("coop: an entity linked into a sector it was in already: not twice\n"); fflush(stdout); } }
        }
        else if (!(node == cur && subset))
        {
            CheckSectorList(base, obj);
            PPCContext c = ctx; c.r3.u64 = obj; c.r1.u64 = ctx.r1.u64 - 512;
            __imp__sub_824646B8(c, base);
            continue;
        }
        keep();
        PPCContext c = ctx; c.r3.u64 = node; c.r4.u64 = bmin; c.r5.u64 = bmax; c.r1.u64 = ctx.r1.u64 - 512;
        __imp__sub_8246F080(c, base);
        return;
    }
    static int told = 0;
    if (told++ < 5) { printf("coop: an entity could not be linked into the sector tree\n"); fflush(stdout); }
}

// (v45p) an entity out of its sector (sub_824646B8): its sector's list of entities is walked to find it.
// On the joining copy that list once closed on itself (an entity put in twice) and the walk never ended
// (Chambois at 96 s). The list is checked first: a loop is cut where it closes.
PPC_FUNC(sub_824646B8)
{
    if (Coop::GetMode() != Coop::Mode::Off) CheckSectorList(base, ctx.r3.u32);
    __imp__sub_824646B8(ctx, base);
}

#endif
