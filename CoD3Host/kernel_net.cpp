// The network: the Xbox 360's sockets on the host's, and the secure network
// layer (XNet) as a plain local network.
//
// Only the multiplayer executable imports these (CoD3MP.exe); the single
// player one starts Winsock and nothing more (kernel_objects.cpp). They are
// built into both all the same: nothing calls them in single player.
//
// The console's sockets are BSD sockets with two differences that matter
// here. Every call takes the caller kind as an extra first argument
// (NetDll_socket(caller, af, type, protocol)), and a socket's address is
// guest memory: big endian where the host is little, so sin_family is
// swapped on the way in and out, while the port and the address are in
// network order on both and copy as they are. Option values and ioctl
// arguments are guest words, big endian too.
//
// Games talk over VDP, the console's protocol 254: UDP with the game's
// data and its voice in one datagram. Both ends here are this runtime, so a
// VDP socket is a UDP socket and the datagram goes as the title wrote it.
//
// XNet is what makes an Xbox address out of an IP one: on the console a
// secure association, keyed by the session. On a local network it is the
// identity: an XNADDR carries the IP address of the machine that made it,
// and turning it back into an IN_ADDR gives that address. That is enough for
// System Link, which is what the title uses between consoles on one network.
// COD3_LANIP=a.b.c.d picks the address this machine gives out (a Hamachi or
// other VPN adapter's, say); otherwise the first IPv4 address that is not
// the loopback, preferring a private network's.

// Winsock before anything that brings in Windows.h, which would otherwise
// bring in the old winsock.h and clash with it.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "kernel.h"
#include "settings.h"
#include "net.h"
#include "version.h"   // 0.5.427: the build sent to the master server

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <map>
#include <mutex>
#include <random>
#include <string>
#include <vector>

#include <Windows.h>
#include <winhttp.h>   // 0.5.427: the master server (HTTPS)
#pragma comment(lib, "winhttp.lib")

#include <chrono>
#include <thread>

namespace
{
    // The console's numbers where they differ from Winsock's names.
    constexpr uint32_t XIPPROTO_VDP = 254;

    // For the settings menu: what went out and came in.
    std::atomic<unsigned long long> g_sent{ 0 }, g_received{ 0 };
    std::mutex g_lastFromMutex;
    std::string g_lastFrom;
    std::atomic<uint32_t> g_lastFromAddress{ 0 };   // in_addr's s_addr, for the side channel
    std::string g_addressWhy;

    std::once_flag g_started;
    void StartWinsock()
    {
        std::call_once(g_started, [] {
            WSADATA data{};
            const int result = WSAStartup(MAKEWORD(2, 2), &data);
            printf("net: Winsock %s\n", result == 0 ? "started" : "failed to start");
            fflush(stdout);
        });
    }

    // Guest socket handles: small numbers of this runtime's own, each naming
    // a host socket. The guest compares a socket with INVALID_SOCKET (~0) and
    // nothing else, so any other value will do.
    std::mutex g_socketMutex;
    std::map<uint32_t, SOCKET> g_sockets;
    uint32_t g_nextSocket = 0x100;
    thread_local int t_lastError = 0;

    SOCKET HostSocket(uint32_t guest)
    {
        std::lock_guard<std::mutex> lock(g_socketMutex);
        const auto found = g_sockets.find(guest);
        return found == g_sockets.end() ? INVALID_SOCKET : found->second;
    }

    // The result of a socket call, for the guest: the error, if any, is kept
    // for NetDll_WSAGetLastError.
    uint32_t Result(int value)
    {
        if (value == SOCKET_ERROR)
        {
            t_lastError = WSAGetLastError();
            return uint32_t(SOCKET_ERROR);
        }
        t_lastError = 0;
        return uint32_t(value);
    }

    // A guest sockaddr_in to the host's: the family swapped, the rest copied.
    bool ToHost(uint8_t* base, uint32_t guest, uint32_t length, sockaddr_in& out)
    {
        memset(&out, 0, sizeof(out));
        if (guest == 0 || length < 8) return false;
        out.sin_family = uint16_t(Guest::Read16(base, guest));
        memcpy(&out.sin_port, base + guest + 2, 2);
        memcpy(&out.sin_addr, base + guest + 4, 4);
        return true;
    }

    void ToGuest(uint8_t* base, const sockaddr_in& in, uint32_t guest)
    {
        Guest::Write16(base, guest, uint16_t(in.sin_family));
        memcpy(base + guest + 2, &in.sin_port, 2);
        memcpy(base + guest + 4, &in.sin_addr, 4);
        memset(base + guest + 8, 0, 8);
    }

    std::string Dotted(const in_addr& address)
    {
        char text[32] = {};
        inet_ntop(AF_INET, &address, text, sizeof(text));
        return text;
    }

    // 0.5.426: public servers groundwork. The bytes of the first few packets
    // of each kind, to learn the System Link search and its answer (port
    // 50000) and the start of a game's own traffic (port 1000, VDP), which
    // the master server and the hole punching will have to carry and
    // rewrite. Six of each port and direction, 256 bytes each at most.
    // COD3_NETDUMP=0: none; COD3_NETDUMP=N: N of each.
    void NetDump(const char* way, uint32_t socket, const uint8_t* data, int length, const sockaddr_in& peer, uint16_t localPort)
    {
        static const int limit = []() { const char* t = getenv("COD3_NETDUMP"); return t ? atoi(t) : 6; }();
        if (limit <= 0 || length <= 0) return;
        const uint16_t port = way[0] == 's' ? ntohs(peer.sin_port) : localPort;   // the service: where it went / where it came in
        const int kind = port == 50000 ? 0 : (port == 1000 || port == 28960) ? 1 : 2;   // (28960: the game's port underneath, 0.5.427)
        static std::atomic<int> counts[2][3] = {};
        if (counts[way[0] == 's' ? 0 : 1][kind].fetch_add(1) >= limit) return;
        std::string hex;
        const int shown = std::min(length, 256);
        for (int i = 0; i < shown; i++)
        {
            char t[4];
            snprintf(t, sizeof t, "%02X", data[i]);
            if (i && i % 32 == 0) hex += "\n  ";
            else if (i && i % 4 == 0) hex += ' ';
            hex += t;
        }
        printf("netdump: socket %u %s %d bytes %s %s:%u (local port %u)%s\n  %s\n", socket, way, length, way[0] == 's' ? "to" : "from",
            Dotted(peer.sin_addr).c_str(), ntohs(peer.sin_port), localPort, length > shown ? " (first 256)" : "", hex.c_str());
        fflush(stdout);
    }

    // 0.5.427: the game's own port 1000 (VDP) moved to a high port on this
    // PC. Port 1000 is below 1024, and routers and providers often drop it:
    // a STUN question from port 1000 on Justin's line never had an answer,
    // while 50000, 3074 and 28960 did. The game still binds, sends to and
    // hears from "1000"; underneath it is GamePort() (28960 unless
    // game_port / COD3_GAMEPORT say otherwise). Every PC in a game must use
    // the same build (and the same game port).
    constexpr uint16_t TitleGamePort = 1000;
    uint16_t GamePort()
    {
        static const uint16_t port = []() {
            const char* e = getenv("COD3_GAMEPORT");
            int p = e ? atoi(e) : Settings::Get().gamePort;
            if (p <= 0 || p > 65535) p = 28960;
            return uint16_t(p);
        }();
        return port;
    }


    // This machine's IPv4 networks, from the adapters that are up.
    //
    // System Link finds games by broadcast, and Windows sends a broadcast to
    // 255.255.255.255 out of one adapter only, the one with the default
    // route. A friend on Hamachi, ZeroTier or Radmin is on another adapter,
    // so a search would never reach them. Each broadcast the title sends
    // goes out of every adapter here instead, to its own network's
    // broadcast address (25.255.255.255 on Hamachi's, say).
    struct Adapter
    {
        in_addr address{};
        in_addr broadcast{};
        uint32_t mask = 0;            // host order
        std::string name;
        bool vpn = false;
        // A network with no room for others (a /32: Tailscale gives each
        // machine one) carries no broadcasts: only the peers list reaches
        // anyone on it.
        bool broadcasts() const { return mask != 0xFFFFFFFFu; }
    };

    const std::vector<Adapter>& Adapters()
    {
        static std::vector<Adapter> adapters;
        static std::once_flag once;
        std::call_once(once, [] {
            ULONG size = 32 * 1024;
            std::vector<uint8_t> buffer(size);
            auto* list = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
            const ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
            ULONG result = GetAdaptersAddresses(AF_INET, flags, nullptr, list, &size);
            if (result == ERROR_BUFFER_OVERFLOW)
            {
                buffer.resize(size);
                list = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());
                result = GetAdaptersAddresses(AF_INET, flags, nullptr, list, &size);
            }
            if (result != NO_ERROR) return;
            for (auto* at = list; at != nullptr; at = at->Next)
            {
                if (at->OperStatus != IfOperStatusUp || at->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
                char name[256] = {};
                WideCharToMultiByte(CP_UTF8, 0, at->FriendlyName, -1, name, sizeof(name) - 1, nullptr, nullptr);
                char description[256] = {};
                WideCharToMultiByte(CP_UTF8, 0, at->Description, -1, description, sizeof(description) - 1, nullptr, nullptr);
                std::string both = std::string(name) + " " + description;
                for (char& c : both) c = char(tolower(static_cast<unsigned char>(c)));
                for (auto* unicast = at->FirstUnicastAddress; unicast != nullptr; unicast = unicast->Next)
                {
                    if (unicast->Address.lpSockaddr->sa_family != AF_INET) continue;
                    Adapter adapter;
                    adapter.address = reinterpret_cast<sockaddr_in*>(unicast->Address.lpSockaddr)->sin_addr;
                    const uint32_t ip = ntohl(adapter.address.s_addr);
                    const ULONG prefix = unicast->OnLinkPrefixLength;
                    const uint32_t mask = prefix == 0 ? 0 : prefix >= 32 ? 0xFFFFFFFFu : ~((1u << (32 - prefix)) - 1);
                    adapter.broadcast.s_addr = htonl(ip | ~mask);
                    adapter.mask = mask;
                    adapter.name = name;
                    adapter.vpn = both.find("hamachi") != std::string::npos || both.find("zerotier") != std::string::npos ||
                                  both.find("radmin") != std::string::npos || both.find("tailscale") != std::string::npos ||
                                  both.find("wireguard") != std::string::npos || (ip >> 24) == 25 || (ip >> 24) == 26;
                    adapters.push_back(adapter);
                        printf("net: adapter \"%s\": %s, %s%s\n", name, Dotted(adapter.address).c_str(),
                        adapter.broadcasts() ? ("broadcast " + Dotted(adapter.broadcast)).c_str() : "no broadcasts (use the peers list)",
                        adapter.vpn ? ", a virtual LAN" : "");
                }
            }
            fflush(stdout);
        });
        return adapters;
    }

    bool IsOwnAddress(in_addr address)
    {
        for (const Adapter& adapter : Adapters())
            if (adapter.address.s_addr == address.s_addr) return true;
        return (ntohl(address.s_addr) >> 24) == 127;
    }

    // 0.5.435: this PC's internet address, asked of a public STUN server
    // from a socket of its own (any port: only the address is wanted);
    // 0 when no answer comes (two servers, two tries each, 1.5 s each).
    uint32_t StunInternetAddress()
    {
        const char* const names[2] = { "stun.l.google.com", "stun.cloudflare.com" };
        const char* const ports[2] = { "19302", "3478" };
        for (int server = 0; server < 2; server++)
        {
            addrinfo hints{}, * found = nullptr;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_DGRAM;
            if (getaddrinfo(names[server], ports[server], &hints, &found) != 0 || !found) continue;
            sockaddr_in to{};
            memcpy(&to, found->ai_addr, sizeof(to));
            freeaddrinfo(found);
            SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s == INVALID_SOCKET) continue;
            sockaddr_in any{};
            any.sin_family = AF_INET;
            bind(s, reinterpret_cast<sockaddr*>(&any), sizeof(any));
            DWORD timeout = 1500;
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            for (int attempt = 0; attempt < 2; attempt++)
            {
                uint8_t request[20] = { 0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xA4, 0x42 };
                for (int i = 8; i < 20; i++) request[i] = uint8_t(GetTickCount() * 2654435761u >> (i % 24));
                sendto(s, reinterpret_cast<const char*>(request), 20, 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
                uint8_t answer[512];
                const int n = recv(s, reinterpret_cast<char*>(answer), sizeof(answer), 0);
                if (n < 20 || answer[0] != 0x01 || answer[1] != 0x01 || memcmp(answer + 8, request + 8, 12) != 0) continue;
                const int end = std::min(n, 20 + ((answer[2] << 8) | answer[3]));
                for (int at = 20; at + 4 <= end;)
                {
                    const int type = (answer[at] << 8) | answer[at + 1], length = (answer[at + 2] << 8) | answer[at + 3];
                    const uint8_t* v = answer + at + 4;
                    if (at + 4 + length > end) break;
                    if ((type == 0x0020 || type == 0x0001) && length >= 8 && v[1] == 0x01)
                    {
                        uint32_t ip = (uint32_t(v[4]) << 24) | (uint32_t(v[5]) << 16) | (uint32_t(v[6]) << 8) | v[7];
                        if (type == 0x0020) ip ^= 0x2112A442u;
                        closesocket(s);
                        return htonl(ip);
                    }
                    at += 4 + ((length + 3) & ~3);
                }
            }
            closesocket(s);
        }
        return 0;
    }
    std::atomic<bool> g_internetSelf{ false };   // 0.5.435: LocalAddress is this PC's internet address

    // The IPv4 address this machine gives out as its own: the one other
    // players are told to send to. COD3_LANIP=a.b.c.d or the settings
    // menu's System Link address chooses; otherwise a virtual LAN's
    // (Hamachi and the like), then a home network's, then any.
    // 0.5.435: with Public game on, this PC's internet address (STUN) comes
    // first: in every working System Link game (a LAN, Tailscale) the
    // address in a PC's XNADDR is the address its packets come from; over
    // the internet the XNADDR said 100.87.12.49 while the packets came from
    // 184.183.32.227, and the joiner left the handshake at the step where
    // a working join goes on (0.5.433's two logs against 0.5.416's Tailscale
    // join: 14 54 50 10 11 95 32 32, then 29 when it works, 21 when not).
    in_addr LocalAddress()
    {
        static in_addr chosen{};
        static std::once_flag once;
        std::call_once(once, [] {
            StartWinsock();
            const char* why = nullptr;
            const char* publicSetting = getenv("COD3_PUBLIC");
            const bool publicGame = (publicSetting && publicSetting[0] == '1') || Settings::Get().publicServer;
            if (publicGame)
            {
                const uint32_t internet = StunInternetAddress();
                if (internet) { chosen.s_addr = internet; why = "its internet address: Public game is on"; g_internetSelf = true; }
                else { printf("net: Public game is on but no STUN server answered: System Link uses this PC's own network address\n"); fflush(stdout); }
            }
            std::string wanted = Settings::Get().lanAddress;
            if (const char* overridden = getenv("COD3_LANIP")) wanted = overridden;
            if (why == nullptr && !wanted.empty() && inet_pton(AF_INET, wanted.c_str(), &chosen) == 1) why = "chosen in the settings (or COD3_LANIP)";
            if (why == nullptr)
            {
                const Adapter* best = nullptr;
                int bestRank = 0;
                for (const Adapter& adapter : Adapters())
                {
                    const uint8_t* b = reinterpret_cast<const uint8_t*>(&adapter.address);
                    const bool isPrivate = b[0] == 10 || (b[0] == 192 && b[1] == 168) || (b[0] == 172 && (b[1] & 0xF0) == 16);
                    const int rank = adapter.vpn && adapter.broadcasts() ? 4 : isPrivate ? 3 : adapter.vpn ? 2 : (b[0] == 169 && b[1] == 254) ? 0 : 1;
                    if (rank > bestRank) { best = &adapter; bestRank = rank; }
                }
                if (best != nullptr) { chosen = best->address; why = best->vpn ? "a virtual LAN" : "this network's"; }
                else { inet_pton(AF_INET, "127.0.0.1", &chosen); why = "no network is up"; }
            }
            g_addressWhy = why;
            printf("net: this machine is %s to System Link (%s)\n", Dotted(chosen).c_str(), why);
            fflush(stdout);
        });
        return chosen;
    }

    // XNADDR: ina, inaOnline, wPortOnline, abEnet[6], abOnline[20].
    constexpr uint32_t XnAddrSize = 36;

    // An XNADDR carries one address, ina. This machine may be on several
    // networks (home, Hamachi, Tailscale...) and the one it gives out may
    // not be one the other machine shares. The online fields are unused on
    // a LAN, so the rest of this machine's addresses go there: inaOnline
    // and the twenty bytes of abOnline, five more. XNetXnAddrToInAddr on
    // the other machine picks the first one on a network it is on too.
    constexpr uint32_t MaxXnAddresses = 7;

    void WriteXnAddr(uint8_t* base, uint32_t guest, in_addr address, bool withOthers = false)
    {
        memset(base + guest, 0, XnAddrSize);
        memcpy(base + guest + 0, &address, 4);
        if (withOthers)
        {
            uint32_t slot = 1;
            for (const Adapter& adapter : Adapters())
            {
                if (adapter.address.s_addr == address.s_addr || slot >= MaxXnAddresses) continue;
                const uint32_t at = slot == 1 ? 4 : 16 + (slot - 2) * 4;   // inaOnline, then abOnline
                memcpy(base + guest + at, &adapter.address, 4);
                slot++;
            }
        }
        // A made up but stable Ethernet address, from the IP one, so two
        // machines never share one.
        const uint8_t* b = reinterpret_cast<const uint8_t*>(&address);
        const uint8_t mac[6] = { 0x00, 0x22, b[0], b[1], b[2], b[3] };
        memcpy(base + guest + 10, mac, 6);
    }

    // Whether an address is on one of this machine's networks. Tailscale's
    // /32s count as its whole range (100.64.0.0/10).
    bool Reachable(in_addr address)
    {
        const uint32_t ip = ntohl(address.s_addr);
        for (const Adapter& adapter : Adapters())
        {
            uint32_t mask = adapter.mask;
            const uint32_t own = ntohl(adapter.address.s_addr);
            if (mask == 0xFFFFFFFFu && (own & 0xFFC00000u) == 0x64400000u) mask = 0xFFC00000u;
            if (mask != 0 && (ip & mask) == (own & mask)) return true;
        }
        return false;
    }

    // The peers list: addresses a broadcast is sent to as well, one by one,
    // for a network that carries no broadcasts (Tailscale) or a friend
    // somewhere a broadcast does not reach. COD3_PEERS or the settings
    // menu's, separated by commas or spaces.
    const std::vector<in_addr>& Peers()
    {
        static std::vector<in_addr> peers;
        static std::once_flag once;
        std::call_once(once, [] {
            std::string text = Settings::Get().systemLinkPeers;
            if (const char* overridden = getenv("COD3_PEERS")) text = overridden;
            for (char& c : text) if (c == ',' || c == ';') c = ' ';
            size_t at = 0;
            while (at < text.size())
            {
                const size_t end = text.find(' ', at);
                const std::string item = text.substr(at, end == std::string::npos ? std::string::npos : end - at);
                at = end == std::string::npos ? text.size() : end + 1;
                in_addr address{};
                if (!item.empty() && inet_pton(AF_INET, item.c_str(), &address) == 1)
                {
                    peers.push_back(address);
                    printf("net: peer %s: System Link searches are sent there too\n", item.c_str());
                }
            }
            fflush(stdout);
        });
        return peers;
    }

    std::mt19937& Random()
    {
        static std::mt19937 generator{ std::random_device{}() };
        return generator;
    }

    void Announce(const char* what)
    {
        static std::mutex mutex;
        static std::map<std::string, int> seen;
        std::lock_guard<std::mutex> lock(mutex);
        if (seen[what]++ < 4)
        {
            printf("net: %s\n", what);
            fflush(stdout);
        }
    }
    // 0.5.427 / 0.5.428: public servers (docs/PUBLIC-SERVERS.md).
    // Hosting: while a System Link game is hosted here (a socket bound to
    // the search port 50000: only a host listens there) and Public game is
    // on, the game is on the master server's list: the public addresses
    // the router gives its sockets come from a public STUN server (asked
    // from those very sockets), it is announced every 10 s and taken off
    // when the search socket closes or the game ends.
    // Joining (0.5.428): when this PC searches System Link with Public game
    // on, the list for this build is fetched; each listed game is asked
    // for an introduction (with this PC's public and LAN addresses) and
    // gets copies of the search and "punch" packets at its public
    // addresses. The host sends punches back to each joiner it is told
    // of. Once both routers have seen the other side, the game's own
    // traffic flows: an XNADDR naming a remote PC's LAN address is
    // resolved to its public address, and the game's ports 1000 / 50000
    // to that PC are its public ports (both ways). Punch packets
    // ("CoD3PNCH") never reach the game. "public:" in the log.
    namespace Public
    {
        constexpr uint32_t Cookie = 0x2112A442u;
        constexpr char Punch[8] = { 'C', 'o', 'D', '3', 'P', 'N', 'C', 'H' };
        // 0.5.437: a side channel message (the rules, bots, UAV, Domination)
        // to or from a met PC, carried on the game socket (its port 28964 is
        // not open through the routers: only the game's way is punched)
        constexpr char SideTag[8] = { 'C', 'o', 'D', '3', 'S', 'I', 'D', 'E' };
        enum { Game, Search, Searcher, Sockets };
        std::atomic<SOCKET> g_game{ INVALID_SOCKET }, g_search{ INVALID_SOCKET }, g_searcher{ INVALID_SOCKET };
        std::atomic<uint32_t> g_searchGuest{ 0 };
        std::atomic<bool> g_hosting{ false };
        std::atomic<bool> g_closing{ false };   // 0.5.429: the process is ending: nothing more is announced
        std::atomic<int64_t> g_searchedAt{ 0 };   // ms (steady clock) of this PC's last System Link search
        struct Mapped { std::atomic<uint32_t> ip{ 0 }; std::atomic<uint32_t> port{ 0 }; uint8_t tx[12] = {}; };
        Mapped g_mapped[Sockets];
        std::mutex g_txMutex;
        std::mutex g_readMutex;   // the game's recvfrom on these sockets, and Drain
        sockaddr_in g_stun{};
        std::atomic<bool> g_haveStun{ false };
        std::mutex g_idMutex;
        std::string g_id, g_secret, g_master;   // this PC's hosted game on the list
        // 0.5.429: the last search's bytes (sent again to the public games
        // from the game's socket), and the hosts' answers to it that came in
        // on the game's socket, kept for the socket the game searched from.
        std::mutex g_payloadMutex;
        std::vector<uint8_t> g_searchPayload;
        struct Redirected { std::vector<uint8_t> data; sockaddr_in from{}; int64_t at = 0; };
        std::deque<Redirected> g_redirected;
        // 0.5.431: the hosts whose answer the current search socket has had
        // (the search goes several times, each copy is answered, and the game
        // lists every answer as a lobby: 0.5.430's test showed one lobby five times)
        SOCKET g_answeredSocket = INVALID_SOCKET;
        std::vector<uint32_t> g_answeredBy;

        // COD3_PUBLIC=1: on whatever the settings say (tests: BENCH-PUBLIC.bat)
        bool On() { static const bool forced = []() { const char* e = getenv("COD3_PUBLIC"); return e && e[0] == '1'; }(); return forced || Settings::Get().publicServer; }

        int64_t NowMs() { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
        uint32_t Be32(const uint8_t* p) { return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3]; }
        uint16_t Be16(const uint8_t* p) { return uint16_t((p[0] << 8) | p[1]); }
        SOCKET SocketOf(int which) { return which == Game ? g_game.load() : which == Search ? g_search.load() : g_searcher.load(); }

        std::string Endpoint(int which)
        {
            const uint32_t port = g_mapped[which].port.load();
            if (!port) return "";
            in_addr a{};
            a.s_addr = g_mapped[which].ip.load();
            return Dotted(a) + ":" + std::to_string(port);
        }

        // "a.b.c.d:port" -> address (network order) and port.
        bool ParseEndpoint(const std::string& text, uint32_t& ip, uint16_t& port)
        {
            const size_t colon = text.rfind(':');
            if (colon == std::string::npos) return false;
            in_addr a{};
            if (inet_pton(AF_INET, text.substr(0, colon).c_str(), &a) != 1) return false;
            const int p = atoi(text.c_str() + colon + 1);
            if (p <= 0 || p > 65535) return false;
            ip = a.s_addr; port = uint16_t(p);
            return true;
        }

        // 0.5.431: 169.254.x.x is the placeholder Windows gives an adapter
        // with no network (Tailscale switched off gave both test PCs
        // 169.254.83.107): it tells no PC from another, so it is neither sent
        // nor matched.
        bool Placeholder(uint32_t address) { const uint8_t* b = reinterpret_cast<const uint8_t*>(&address); return b[0] == 169 && b[1] == 254; }

        // This PC's LAN addresses (the ones its XNADDR carries), for the other side's map.
        std::vector<std::string> LanAddresses()
        {
            std::vector<std::string> out;
            const in_addr given = LocalAddress();   // 0.5.431: the one System Link gives out (lan_address) first
            if (given.s_addr && !Placeholder(given.s_addr)) out.push_back(Dotted(given));
            for (const Adapter& adapter : Adapters())
                if (out.size() < 4 && adapter.address.s_addr && !Placeholder(adapter.address.s_addr) && adapter.address.s_addr != given.s_addr)
                    out.push_back(Dotted(adapter.address));
            return out;
        }
        std::string LanJson()
        {
            std::string json = "[";
            for (const std::string& a : LanAddresses()) json += (json.size() > 1 ? ",\"" : "\"") + a + "\"";
            return json + "]";
        }

        // ---- the other PCs met through the master server ----
        struct Remote { uint32_t ip = 0; uint16_t game = 0, search = 0; uint32_t lan[4] = {}; int64_t seen = 0; };
        std::mutex g_remoteMutex;
        std::vector<Remote> g_remotes;

        void AddRemote(const std::string& game, const std::string& search, const std::vector<std::string>& lan)
        {
            Remote r;
            uint16_t searchPort = 0;
            uint32_t searchIp = 0;
            if (!ParseEndpoint(game, r.ip, r.game)) return;
            if (ParseEndpoint(search, searchIp, searchPort) && searchIp == r.ip) r.search = searchPort;
            for (size_t i = 0; i < lan.size() && i < 4; i++) { in_addr a{}; if (inet_pton(AF_INET, lan[i].c_str(), &a) == 1) r.lan[i] = a.s_addr; }
            r.seen = NowMs();
            std::lock_guard<std::mutex> lock(g_remoteMutex);
            for (Remote& old : g_remotes)
                if (old.ip == r.ip) { const bool news = old.game != r.game || old.search != r.search; old = r; if (!news) return; goto report; }
            g_remotes.push_back(r);
        report:
            static int told = 0;
            if (told++ < 40) { in_addr a{}; a.s_addr = r.ip; printf("public: met %s (game port %u, search port %u, %zu LAN addresses)\n", Dotted(a).c_str(), r.game, r.search, lan.size()); fflush(stdout); }
        }

        // XNetXnAddrToInAddr: an XNADDR's addresses -> the public address of
        // the remote PC they belong to. Matched on the whole set, not one
        // address: two homes often both use 192.168.0.x (Justin's friend's PC
        // is 192.168.0.89), so one LAN address can be another PC's and ours.
        // All of them ours: it is this PC. Else the remote sharing the most.
        bool PublicFor(const uint32_t* candidates, int count, uint32_t& out)
        {
            // 0.5.431: this PC's own XNADDR leads with the address it gives out
            // (0.5.430's test: Justin's own XNADDR, 100.79.11.48 first, was
            // matched to his friend's PC through the shared 169.254.83.107)
            if (count > 0 && candidates[0] && candidates[0] == LocalAddress().s_addr) return false;
            bool allOurs = true, any = false;
            for (int i = 0; i < count; i++)
            {
                if (!candidates[i] || Placeholder(candidates[i])) continue;
                any = true;
                in_addr c{};
                c.s_addr = candidates[i];
                if (!IsOwnAddress(c)) allOurs = false;
            }
            if (!any || allOurs) return false;
            std::lock_guard<std::mutex> lock(g_remoteMutex);
            const Remote* best = nullptr;
            int bestScore = 0;
            for (const Remote& r : g_remotes)
            {
                int score = 0;
                for (int i = 0; i < count; i++)
                {
                    if (!candidates[i] || Placeholder(candidates[i])) continue;
                    if (candidates[i] == r.ip) score += 8;
                    for (uint32_t l : r.lan) if (l && l == candidates[i]) score++;
                }
                if (score > bestScore || (score && score == bestScore && best && r.seen > best->seen)) { best = &r; bestScore = score; }
            }
            if (!best) return false;
            out = best->ip;
            return true;
        }

        // 0.5.434: a met PC's public address -> the address it gives out
        // (the first of the LAN addresses it sent: since 0.5.431 its
        // lan_address). XNetInAddrToXnAddr makes that PC's XNADDR from it, as
        // on a LAN; made from the public address it matched nothing the
        // joiner had sent, and the host refused the join after the handshake.
        bool GivenAddressFor(uint32_t ip, uint32_t& given)
        {
            std::lock_guard<std::mutex> lock(g_remoteMutex);
            for (const Remote& r : g_remotes)
                if (r.ip == ip && r.lan[0]) { given = r.lan[0]; return true; }
            return false;
        }

        // The game's ports, to and from a remote PC: its public ones. To
        // anyone else, the game's port 1000 is GamePort().
        uint16_t OutPort(uint32_t ip, uint16_t title)
        {
            {
                std::lock_guard<std::mutex> lock(g_remoteMutex);
                for (const Remote& r : g_remotes)
                    if (r.ip == ip)
                    {
                        if (title == TitleGamePort && r.game) return r.game;
                        if (title == 50000 && r.search) return r.search;
                    }
            }
            return title == TitleGamePort ? GamePort() : title;
        }
        uint16_t InPort(uint32_t ip, uint16_t port)
        {
            {
                std::lock_guard<std::mutex> lock(g_remoteMutex);
                for (const Remote& r : g_remotes)
                    if (r.ip == ip)
                    {
                        if (port == r.game) return TitleGamePort;
                        if (port == r.search) return 50000;
                    }
            }
            return port == GamePort() ? TitleGamePort : port;
        }

        // 0.5.429: a packet from a met PC's search port (its game's answer to
        // our search, sent to our game socket): kept for the search socket.
        bool FromRemoteSearch(const sockaddr_in& from)
        {
            const uint16_t port = ntohs(from.sin_port);
            std::lock_guard<std::mutex> lock(g_remoteMutex);
            for (const Remote& r : g_remotes)
                if (r.ip == from.sin_addr.s_addr && r.search && r.search == port) return true;
            return false;
        }
        void KeepAnswer(const uint8_t* data, int n, const sockaddr_in& from)
        {
            std::lock_guard<std::mutex> lock(g_payloadMutex);
            // 0.5.431: one waiting answer per host (the newest)
            for (auto it = g_redirected.begin(); it != g_redirected.end();)
                if (it->from.sin_addr.s_addr == from.sin_addr.s_addr) it = g_redirected.erase(it); else ++it;
            if (g_redirected.size() >= 32) g_redirected.pop_front();
            Redirected r;
            r.data.assign(data, data + n);
            r.from = from;
            r.at = NowMs();
            g_redirected.push_back(std::move(r));
            static int told = 0;
            if (told++ < 10) { printf("public: a public game answered our search (%d bytes from %s:%u)\n", n, Dotted(from.sin_addr).c_str(), ntohs(from.sin_port)); fflush(stdout); }
        }
        // For the game's recvfrom on its search socket: a kept answer (fresh ones only).
        bool PopAnswer(SOCKET socket, uint8_t* buffer, int capacity, sockaddr_in& from, int& length)
        {
            std::lock_guard<std::mutex> lock(g_payloadMutex);
            while (!g_redirected.empty() && NowMs() - g_redirected.front().at > 5000) g_redirected.pop_front();
            if (socket != g_answeredSocket) { g_answeredSocket = socket; g_answeredBy.clear(); }
            // 0.5.431: each host's answer once per search socket; later copies dropped
            while (!g_redirected.empty())
            {
                Redirected& r = g_redirected.front();
                const uint32_t by = r.from.sin_addr.s_addr;
                if (std::find(g_answeredBy.begin(), g_answeredBy.end(), by) != g_answeredBy.end()) { g_redirected.pop_front(); continue; }
                g_answeredBy.push_back(by);
                length = std::min(capacity, int(r.data.size()));
                memcpy(buffer, r.data.data(), size_t(length));
                from = r.from;
                g_redirected.pop_front();
                return true;
            }
            return false;
        }

        // 0.5.431: the first game packets to and from each PC met (to see a join's handshake)
        void NoteGameTraffic(bool sent, uint32_t ip, int length)
        {
            static std::mutex mutex;
            static std::map<uint64_t, int> counts;
            bool met = false;
            {
                std::lock_guard<std::mutex> lock(g_remoteMutex);
                // 0.5.437: a met PC in the game with this one stays met (its
                // punches stop once its search ends; 2 min on, it was dropped)
                for (Remote& r : g_remotes) if (r.ip == ip) { met = true; if (!sent) r.seen = NowMs(); }
            }
            if (!met) return;
            std::lock_guard<std::mutex> lock(mutex);
            int& count = counts[(uint64_t(ip) << 1) | (sent ? 1u : 0u)];
            if (count++ >= 8) return;
            in_addr a{};
            a.s_addr = ip;
            printf("public: game packet %s %s (%d bytes, #%d)\n", sent ? "sent to" : "received from", Dotted(a).c_str(), length, count);
            fflush(stdout);
        }

        // 0.5.430: the PCs met lately (a broadcast of the game's goes to each:
        // 0.5.429's test showed the host answering a search by broadcast, to
        // 255.255.255.255, which only the host's own home heard).
        std::vector<uint32_t> MetAddresses()
        {
            std::vector<uint32_t> out;
            if (!On()) return out;
            std::lock_guard<std::mutex> lock(g_remoteMutex);
            const int64_t now = NowMs();
            for (const Remote& r : g_remotes)
                if (now - r.seen < 120000) out.push_back(r.ip);
            return out;
        }

        // A System Link search's copies, to every listed game's search port (from sendto).
        std::vector<sockaddr_in> SearchTargets()
        {
            std::vector<sockaddr_in> out;
            if (!On()) return out;
            std::lock_guard<std::mutex> lock(g_remoteMutex);
            const int64_t now = NowMs();
            for (const Remote& r : g_remotes)
                if (r.search && now - r.seen < 60000)
                {
                    sockaddr_in a{};
                    a.sin_family = AF_INET;
                    a.sin_addr.s_addr = r.ip;
                    a.sin_port = htons(r.search);
                    out.push_back(a);
                }
            return out;
        }

        void SendPunch(SOCKET s, const std::string& endpoint)
        {
            uint32_t ip; uint16_t port;
            if (s == INVALID_SOCKET || !ParseEndpoint(endpoint, ip, port)) return;
            sockaddr_in to{};
            to.sin_family = AF_INET; to.sin_addr.s_addr = ip; to.sin_port = htons(port);
            for (int i = 0; i < 2; i++) sendto(s, Punch, sizeof(Punch), 0, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
        }

        // ---- 0.5.437: the side channel to met PCs ----
        bool IsMet(uint32_t ip)
        {
            for (const uint32_t met : MetAddresses()) if (met == ip) return true;
            return false;
        }
        // To every met PC (or only one, when only is met); false: nothing sent this way.
        bool SideOut(const void* data, int size, uint32_t only)
        {
            const SOCKET s = g_game.load();
            if (s == INVALID_SOCKET || size <= 0 || size > 1400) return false;
            std::vector<uint32_t> targets = MetAddresses();
            if (only) { if (std::find(targets.begin(), targets.end(), only) == targets.end()) return false; targets.assign(1, only); }
            if (targets.empty()) return false;
            uint8_t packet[1408];
            memcpy(packet, SideTag, sizeof(SideTag));
            memcpy(packet + sizeof(SideTag), data, size);
            for (const uint32_t ip : targets)
            {
                sockaddr_in to{};
                to.sin_family = AF_INET; to.sin_addr.s_addr = ip; to.sin_port = htons(OutPort(ip, TitleGamePort));
                sendto(s, reinterpret_cast<const char*>(packet), int(sizeof(SideTag)) + size, 0, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
            }
            static std::atomic<int> told{ 0 };
            if (told.fetch_add(1) < 6) { printf("public: side channel message (%.4s, %d bytes) sent on the game's way to %zu PC(s)\n", static_cast<const char*>(data), size, targets.size()); fflush(stdout); }
            return true;
        }
        std::mutex g_sideMutex;
        std::deque<std::pair<std::vector<uint8_t>, uint32_t>> g_sideInbox;
        bool IsSide(const uint8_t* packet, int n) { return n > int(sizeof(SideTag)) && memcmp(packet, SideTag, sizeof(SideTag)) == 0; }
        void KeepSide(const uint8_t* packet, int n, const sockaddr_in& from)
        {
            std::lock_guard<std::mutex> lock(g_sideMutex);
            if (g_sideInbox.size() >= 256) g_sideInbox.pop_front();
            g_sideInbox.push_back({ std::vector<uint8_t>(packet + sizeof(SideTag), packet + n), from.sin_addr.s_addr });
            static int told = 0;
            if (told++ < 6) { printf("public: side channel message (%.4s, %d bytes) came on the game's way from %s\n", reinterpret_cast<const char*>(packet + sizeof(SideTag)), n - int(sizeof(SideTag)), Dotted(from.sin_addr).c_str()); fflush(stdout); }
        }
        int PopSide(void* data, int size, uint32_t* from)
        {
            std::lock_guard<std::mutex> lock(g_sideMutex);
            if (g_sideInbox.empty()) return -1;
            auto& front = g_sideInbox.front();
            const int n = std::min(size, int(front.first.size()));
            memcpy(data, front.first.data(), n);
            if (from) *from = front.second;
            g_sideInbox.pop_front();
            return n;
        }

        // ---- STUN ----
        // On the public thread only (g_stun is written once, before g_haveStun).
        bool ResolveStun()
        {
            if (g_haveStun.load()) return true;
            const char* const names[2] = { "stun.l.google.com", "stun.cloudflare.com" };
            const char* const ports[2] = { "19302", "3478" };
            for (int i = 0; i < 2; i++)
            {
                addrinfo hints{}, * found = nullptr;
                hints.ai_family = AF_INET;
                hints.ai_socktype = SOCK_DGRAM;
                if (getaddrinfo(names[i], ports[i], &hints, &found) == 0 && found)
                {
                    memcpy(&g_stun, found->ai_addr, sizeof(g_stun));
                    freeaddrinfo(found);
                    g_haveStun = true;
                    printf("public: STUN server %s (%s:%u)\n", names[i], Dotted(g_stun.sin_addr).c_str(), ntohs(g_stun.sin_port));
                    fflush(stdout);
                    return true;
                }
            }
            static bool told = false;
            if (!told) { told = true; printf("public: no STUN server could be found (no internet?)\n"); fflush(stdout); }
            return false;
        }

        // A STUN binding request (RFC 5389, 20 bytes) from that socket.
        void Ask(int which)
        {
            const SOCKET s = SocketOf(which);
            if (s == INVALID_SOCKET || !ResolveStun()) return;
            uint8_t request[20] = { 0x00, 0x01, 0x00, 0x00, 0x21, 0x12, 0xA4, 0x42 };
            {
                std::lock_guard<std::mutex> lock(g_txMutex);
                static std::mt19937 random{ std::random_device{}() };
                for (uint8_t& b : g_mapped[which].tx) b = uint8_t(random());
                memcpy(request + 8, g_mapped[which].tx, 12);
            }
            sendto(s, reinterpret_cast<const char*>(request), sizeof(request), 0, reinterpret_cast<const sockaddr*>(&g_stun), sizeof(g_stun));
        }

        // From recvfrom: true when the packet is the STUN server's answer to
        // one of ours (then it is not the game's to see).
        bool Answer(SOCKET s, const uint8_t* p, int n, const sockaddr_in& from)
        {
            if (n < 20 || p[0] != 0x01 || p[1] != 0x01 || Be32(p + 4) != Cookie) return false;
            if (!g_haveStun.load() || from.sin_addr.s_addr != g_stun.sin_addr.s_addr) return false;
            int which = -1;
            {
                std::lock_guard<std::mutex> lock(g_txMutex);
                for (int w = 0; w < Sockets; w++)
                    if (s == SocketOf(w) && memcmp(p + 8, g_mapped[w].tx, 12) == 0) which = w;
            }
            if (which < 0) return false;
            const int end = std::min(n, 20 + int(Be16(p + 2)));
            uint32_t ip = 0, port = 0;
            for (int at = 20; at + 4 <= end;)
            {
                const uint16_t type = Be16(p + at), length = Be16(p + at + 2);
                const uint8_t* v = p + at + 4;
                if (at + 4 + length > end) break;
                if ((type == 0x0020 || (type == 0x0001 && !port)) && length >= 8 && v[1] == 0x01)
                {
                    const bool xored = type == 0x0020;
                    port = Be16(v + 2) ^ (xored ? 0x2112u : 0u);
                    ip = htonl(Be32(v + 4) ^ (xored ? Cookie : 0u));
                    if (xored) break;
                }
                at += 4 + ((length + 3) & ~3);
            }
            if (port)
            {
                const bool portChanged = g_mapped[which].port.exchange(port) != port;
                const bool ipChanged = g_mapped[which].ip.exchange(ip) != ip;
                const char* const names[Sockets] = { "game port 1000", "search port 50000", "search socket" };
                if (portChanged || ipChanged) { printf("public: the router shows this PC's %s as %s\n", names[which], Endpoint(which).c_str()); fflush(stdout); }
            }
            return true;
        }

        // The game reads its sockets only when it expects something (its port
        // 1000 not until someone else is in the game: 0.5.427's first test).
        // Looked at from here without taking anything: a STUN answer of ours
        // or a punch at the head is taken, anything else is left for the game.
        void Drain(int which)
        {
            const SOCKET s = SocketOf(which);
            if (s == INVALID_SOCKET) return;
            std::unique_lock<std::mutex> lock(g_readMutex, std::try_to_lock);
            if (!lock) return;
            for (int i = 0; i < 8; i++)
            {
                fd_set readable;
                FD_ZERO(&readable);
                FD_SET(s, &readable);
                timeval none{};
                if (select(0, &readable, nullptr, nullptr, &none) != 1) return;
                uint8_t packet[1500];
                sockaddr_in from{};
                int fromLength = sizeof(from);
                const int n = recvfrom(s, reinterpret_cast<char*>(packet), sizeof(packet), MSG_PEEK, reinterpret_cast<sockaddr*>(&from), &fromLength);
                const bool punch = n == int(sizeof(Punch)) && memcmp(packet, Punch, sizeof(Punch)) == 0;
                const bool side = n > 0 && which == Game && IsSide(packet, n);   // 0.5.437
                const bool answer = n > 0 && !punch && !side && which == Game && FromRemoteSearch(from);
                if (n <= 0 || !(punch || side || answer || Answer(s, packet, n, from))) return;
                fromLength = sizeof(from);
                recvfrom(s, reinterpret_cast<char*>(packet), sizeof(packet), 0, reinterpret_cast<sockaddr*>(&from), &fromLength);
                if (side) KeepSide(packet, n, from);
                if (answer) KeepAnswer(packet, n, from);
                if (punch)
                {
                    static int told = 0;
                    if (told++ < 10) { printf("public: a punch came in from %s:%u: the way is open\n", Dotted(from.sin_addr).c_str(), ntohs(from.sin_port)); fflush(stdout); }
                }
            }
        }

        // ---- the master server ----
        // An HTTP(S) request: the answer's body, or "".
        std::string Request(const wchar_t* verb, const std::string& url, const std::string& body, int timeoutMs = 5000)
        {
            static HINTERNET session = WinHttpOpen(L"CoD3PCPort", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session) return "";
            const std::wstring wide(url.begin(), url.end());
            URL_COMPONENTS parts{};
            parts.dwStructSize = sizeof(parts);
            wchar_t hostName[256] = {}, path[1024] = {};
            parts.lpszHostName = hostName; parts.dwHostNameLength = 255;
            parts.lpszUrlPath = path; parts.dwUrlPathLength = 1023;
            if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts)) return "";
            std::wstring fullPath = path;
            if (parts.lpszExtraInfo && parts.dwExtraInfoLength) fullPath.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
            std::string answer;
            HINTERNET connection = WinHttpConnect(session, hostName, parts.nPort, 0);
            HINTERNET request = connection ? WinHttpOpenRequest(connection, verb, fullPath.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
            if (request)
            {
                WinHttpSetTimeouts(request, timeoutMs, timeoutMs, timeoutMs, timeoutMs);
                const wchar_t* headers = L"Content-Type: application/json\r\n";
                if (WinHttpSendRequest(request, headers, DWORD(-1), body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()),
                                       DWORD(body.size()), DWORD(body.size()), 0) &&
                    WinHttpReceiveResponse(request, nullptr))
                {
                    DWORD available = 0;
                    while (WinHttpQueryDataAvailable(request, &available) && available)
                    {
                        std::string chunk(available, '\0');
                        DWORD read = 0;
                        if (!WinHttpReadData(request, chunk.data(), available, &read) || !read) break;
                        answer.append(chunk.data(), read);
                        if (answer.size() > 262144) break;
                    }
                }
                WinHttpCloseHandle(request);
            }
            if (connection) WinHttpCloseHandle(connection);
            return answer;
        }

        std::string Field(const std::string& json, const char* key)
        {
            const std::string tag = std::string("\"") + key + "\":\"";
            const size_t at = json.find(tag);
            if (at == std::string::npos) return "";
            const size_t from = at + tag.size(), to = json.find('"', from);
            return to == std::string::npos ? "" : json.substr(from, to - from);
        }
        std::vector<std::string> ListField(const std::string& json, const char* key)
        {
            std::vector<std::string> out;
            const std::string tag = std::string("\"") + key + "\":[";
            const size_t at = json.find(tag);
            if (at == std::string::npos) return out;
            const size_t end = json.find(']', at);
            for (size_t q = json.find('"', at + tag.size()); q != std::string::npos && q < end;)
            {
                const size_t close = json.find('"', q + 1);
                if (close == std::string::npos || close > end) break;
                out.push_back(json.substr(q + 1, close - q - 1));
                q = json.find('"', close + 1);
            }
            return out;
        }
        // The objects of a JSON array, one string each, cut at each start tag.
        std::vector<std::string> Objects(const std::string& json, const char* firstKey)
        {
            std::vector<std::string> out;
            const std::string tag = std::string("{\"") + firstKey + "\":";
            for (size_t at = json.find(tag); at != std::string::npos;)
            {
                const size_t next = json.find(tag, at + 1);
                out.push_back(json.substr(at, next == std::string::npos ? std::string::npos : next - at));
                at = next;
            }
            return out;
        }

        // Printable ASCII only, no quotes or backslashes (it goes into JSON).
        std::string Clean(const char* text, size_t max)
        {
            std::string out;
            for (const char* c = text; *c && out.size() < max; c++)
                if (*c >= 32 && *c < 127 && *c != '"' && *c != '\\') out += *c;
            return out;
        }

        void Remove()
        {
            std::string id, secret, master;
            {
                std::lock_guard<std::mutex> lock(g_idMutex);
                id.swap(g_id); secret.swap(g_secret); master = g_master;
            }
            if (id.empty()) return;
            Request(L"POST", master + "/remove", "{\"id\":\"" + id + "\",\"secret\":\"" + secret + "\"}", 2000);
            printf("public: the game is off the public list\n");
            fflush(stdout);
        }

        void SendSearchCopies();   // (below)

        void Thread()
        {
            int64_t lastAsk = 0, lastAnnounce = 0, lastPoll = 0, lastList = 0;
            std::map<std::string, int64_t> introduced;   // server id -> when this PC asked to be introduced
            int failures = 0;
            for (;;)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                const int64_t now = NowMs();
                if (g_closing.load()) continue;   // 0.5.429: the game is closing (0.5.429's first test: re-announced after the remove)
                const bool hosting = On() && g_hosting.load();
                const bool searching = On() && now - g_searchedAt.load() < 15000;
                if (!hosting) Remove();
                if (!hosting && !searching) { g_mapped[Search].port = 0; continue; }
                const std::string master = Clean(Settings::Get().masterServer, 95);
                if (master.empty()) continue;

                // The routers' mappings: asked every 30 s (every 2 s until known).
                const bool known = g_mapped[Game].port.load() != 0 && (!hosting || g_mapped[Search].port.load() != 0) &&
                    (!searching || g_mapped[Searcher].port.load() != 0);
                if (now - lastAsk > (known ? 30000 : 2000))
                {
                    lastAsk = now;
                    for (int w = 0; w < Sockets; w++) Ask(w);
                }
                for (int w = 0; w < Sockets; w++) Drain(w);
                if (!g_mapped[Game].port.load()) continue;

                std::string id, secret;
                { std::lock_guard<std::mutex> lock(g_idMutex); id = g_id; secret = g_secret; }
                if (hosting && g_mapped[Search].port.load() && now - lastAnnounce > 10000)
                {
                    lastAnnounce = now;
                    const Settings::Values v = Settings::Get();
                    std::string name = Clean(v.serverName, 47);
                    if (name.empty()) { const std::string player = Clean(v.playerName, 15); name = player.empty() ? "CoD3 game" : player + "'s game"; }
                    std::string body = "{";
                    if (!id.empty()) body += "\"id\":\"" + id + "\",\"secret\":\"" + secret + "\",";
                    body += "\"name\":\"" + name + "\",\"map\":\"\",\"mode\":\"\",\"players\":0,\"maxPlayers\":" + std::to_string(v.maxPlayers >= 32 ? 32 : 24) +
                        ",\"version\":\"" COD3_VERSION "\",\"game\":\"" + Endpoint(Game) + "\",\"search\":\"" + Endpoint(Search) + "\",\"lan\":" + LanJson() + "}";
                    const std::string answer = Request(L"POST", master + "/announce", body);
                    const std::string gotId = Field(answer, "id");
                    if (!gotId.empty())
                    {
                        if (id.empty()) { printf("public: \"%s\" is on the public list (%s; game %s, search %s)\n", name.c_str(), master.c_str(), Endpoint(Game).c_str(), Endpoint(Search).c_str()); fflush(stdout); }
                        std::lock_guard<std::mutex> lock(g_idMutex);
                        g_id = id = gotId; g_secret = secret = Field(answer, "secret"); g_master = master; failures = 0;
                    }
                    else
                    {
                        if (answer.find("not yours") != std::string::npos) { std::lock_guard<std::mutex> lock(g_idMutex); g_id.clear(); g_secret.clear(); }
                        if (failures++ < 5) { printf("public: the master server did not take the game: %s\n", answer.empty() ? "(no answer)" : answer.substr(0, 200).c_str()); fflush(stdout); }
                    }
                }
                // Hosting: the joiners the master server introduces get punches
                // from both sockets, which opens this PC's router to them.
                if (hosting && !id.empty() && now - lastPoll > 2000)
                {
                    lastPoll = now;
                    const std::string answer = Request(L"POST", master + "/punches", "{\"id\":\"" + id + "\",\"secret\":\"" + secret + "\"}");
                    for (const std::string& joiner : Objects(answer, "game"))
                    {
                        const std::string game = Field(joiner, "game"), search = Field(joiner, "search");
                        AddRemote(game, search, ListField(joiner, "lan"));
                        SendPunch(g_game.load(), game);
                        SendPunch(g_search.load(), search);
                        SendPunch(g_search.load(), game);   // 0.5.429: the joiner searches from its game socket
                        static int told = 0;
                        if (told++ < 20) { printf("public: a player asks to join from %s: punching\n", game.c_str()); fflush(stdout); }
                    }
                }
                // Searching: the public games of this build, each introduced
                // and punched at (the search's copies go to them from sendto).
                if (searching && g_mapped[Searcher].port.load() && now - lastList > 3000)
                {
                    lastList = now;
                    const std::string answer = Request(L"GET", master + "/list?version=" COD3_VERSION, "");
                    const std::string mine = Endpoint(Game);
                    int listed = 0;
                    for (const std::string& server : Objects(answer, "id"))
                    {
                        const std::string sid = Field(server, "id"), game = Field(server, "game"), search = Field(server, "search");
                        if (sid.empty() || sid == id || game == mine || game.empty()) continue;
                        listed++;
                        AddRemote(game, search, ListField(server, "lan"));
                        if (now - introduced[sid] > 20000)
                        {
                            introduced[sid] = now;
                            Request(L"POST", master + "/punch", "{\"server\":\"" + sid + "\",\"version\":\"" COD3_VERSION "\",\"game\":\"" + mine +
                                "\",\"search\":\"" + Endpoint(Searcher) + "\",\"lan\":" + LanJson() + "}");
                        }
                        SendPunch(g_game.load(), game);
                        SendPunch(g_game.load(), search);   // 0.5.429: the search goes from the game socket
                    }
                    static int told = 0;
                    if (told++ < 10) { printf("public: searching: %d public game(s) of this build listed\n", listed); fflush(stdout); }
                }
                // 0.5.429: the search again, from the game socket (the one the hosts
                // have punched at; the game opens a new search socket every
                // 2.5 s), twice a second for 3 s after each search.
                {
                    static int64_t lastCopies = 0;
                    const int64_t since = now - g_searchedAt.load();
                    if (searching && since > 900 && since < 2500 && now - lastCopies > 1500) { lastCopies = now; SendSearchCopies(); }   // 0.5.431: one more copy, 1 s on
                }
            }
        }

        void Start()
        {
            static std::once_flag started;
            std::call_once(started, [] { std::thread(Thread).detach(); });
        }

        void SendSearchCopies()
        {
            const SOCKET game = g_game.load();
            if (game == INVALID_SOCKET) return;
            std::vector<uint8_t> payload;
            { std::lock_guard<std::mutex> lock(g_payloadMutex); payload = g_searchPayload; }
            if (payload.empty()) return;
            for (const sockaddr_in& remote : SearchTargets())
                sendto(game, reinterpret_cast<const char*>(payload.data()), int(payload.size()), 0, reinterpret_cast<const sockaddr*>(&remote), sizeof(remote));
        }

        // From sendto: this PC searched System Link (a broadcast to port 50000).
        void Searched(SOCKET s, const uint8_t* data, int length)
        {
            g_searcher = s;
            g_searchedAt = NowMs();
            if (length > 0 && length <= 1400) { std::lock_guard<std::mutex> lock(g_payloadMutex); g_searchPayload.assign(data, data + length); }
            Start();
            SendSearchCopies();
        }
    }

}

// --- Sockets -----------------------------------------------------------------

// SOCKET NetDll_socket(caller, int af, int type, int protocol)
// 0.5.428: the hosted game off the public list before the process ends
void Net::PublicShutdown() { Public::g_closing = true; Public::Remove(); }

PPC_FUNC(__imp__NetDll_socket)
{
    Kernel::CountImport("NetDll_socket");
    StartWinsock();
    const int af = int(ctx.r4.u32), type = int(ctx.r5.u32);
    int protocol = int(ctx.r6.u32);
    const bool vdp = uint32_t(protocol) == XIPPROTO_VDP;
    if (vdp) protocol = IPPROTO_UDP;
    const SOCKET host = socket(af, type, protocol);
    if (host == INVALID_SOCKET)
    {
        t_lastError = WSAGetLastError();
        printf("net: socket(%d, %d, %u) failed, error %d\n", af, type, ctx.r6.u32, t_lastError);
        fflush(stdout);
        ctx.r3.u32 = 0xFFFFFFFFu;
        return;
    }
    uint32_t guest;
    {
        std::lock_guard<std::mutex> lock(g_socketMutex);
        guest = g_nextSocket++;
        g_sockets[guest] = host;
    }
    printf("net: socket %u: %s\n", guest, vdp ? "VDP, as UDP" : type == SOCK_DGRAM ? "UDP" : type == SOCK_STREAM ? "TCP" : "other");
    fflush(stdout);
    t_lastError = 0;
    ctx.r3.u32 = guest;
}

// int NetDll_closesocket(caller, SOCKET s)
PPC_FUNC(__imp__NetDll_closesocket)
{
    Kernel::CountImport("NetDll_closesocket");
    SOCKET host = INVALID_SOCKET;
    {
        std::lock_guard<std::mutex> lock(g_socketMutex);
        const auto found = g_sockets.find(ctx.r4.u32);
        if (found != g_sockets.end()) { host = found->second; g_sockets.erase(found); }
    }
    // 0.5.427: the search port's socket closing = the hosted game is over
    if (ctx.r4.u32 == Public::g_searchGuest.load()) { Public::g_hosting = false; Public::g_search = INVALID_SOCKET; Public::g_searchGuest = 0; }
    if (host != INVALID_SOCKET && host == Public::g_game.load()) Public::g_game = INVALID_SOCKET;
    if (host != INVALID_SOCKET && host == Public::g_searcher.load()) Public::g_searcher = INVALID_SOCKET;   // 0.5.429
    ctx.r3.u32 = host == INVALID_SOCKET ? Result(SOCKET_ERROR) : Result(closesocket(host));
}

// int NetDll_bind(caller, SOCKET s, const sockaddr* name, int length)
PPC_FUNC(__imp__NetDll_bind)
{
    Kernel::CountImport("NetDll_bind");
    sockaddr_in address{};
    const SOCKET host = HostSocket(ctx.r4.u32);
    if (host == INVALID_SOCKET || !ToHost(base, ctx.r5.u32, ctx.r6.u32, address))
    {
        t_lastError = WSAENOTSOCK;
        ctx.r3.u32 = uint32_t(SOCKET_ERROR);
        return;
    }
    // 0.5.427: the game's port 1000 is GamePort() underneath (routers drop 1000)
    const bool gamePort = ntohs(address.sin_port) == TitleGamePort;
    if (gamePort) address.sin_port = htons(GamePort());
    const int result = bind(host, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    // 0.5.427: the game's own port and the search port (bound only by a host)
    if (result == 0 && gamePort) Public::g_game = host;
    if (result == 0 && ntohs(address.sin_port) == 50000)
    {
        Public::g_search = host; Public::g_searchGuest = ctx.r4.u32; Public::g_hosting = true;
        Public::Start();
        printf("public: hosting (search port bound); %s\n", Public::On() ? "public game is on: putting it on the list" : "public game is off (F11 > Public game)");
        fflush(stdout);
    }
    printf("net: socket %u bound to %s:%u%s%s\n", ctx.r4.u32, Dotted(address.sin_addr).c_str(), ntohs(address.sin_port),
        gamePort ? " (the game's port 1000)" : "", result == 0 ? "" : " (failed)");
    fflush(stdout);
    ctx.r3.u32 = Result(result);
}

// int NetDll_setsockopt(caller, SOCKET s, int level, int name, const char* value, int length)
PPC_FUNC(__imp__NetDll_setsockopt)
{
    Kernel::CountImport("NetDll_setsockopt");
    const SOCKET host = HostSocket(ctx.r4.u32);
    if (host == INVALID_SOCKET) { t_lastError = WSAENOTSOCK; ctx.r3.u32 = uint32_t(SOCKET_ERROR); return; }
    const int level = int(ctx.r5.u32), name = int(ctx.r6.u32);
    const uint32_t value = ctx.r7.u32, length = ctx.r8.u32;
    int result = 0;
    if (value != 0 && length == 4)
    {
        const int option = int(Guest::Read32(base, value));
        result = setsockopt(host, level, name, reinterpret_cast<const char*>(&option), sizeof(option));
    }
    else if (value != 0 && length == 1)
    {
        const int option = base[value];
        result = setsockopt(host, level, name, reinterpret_cast<const char*>(&option), sizeof(option));
    }
    // An option the host does not know (the console has a few of its own)
    // is accepted and ignored: refusing it would stop the title for nothing.
    if (result == SOCKET_ERROR && WSAGetLastError() == WSAENOPROTOOPT)
    {
        char what[96];
        snprintf(what, sizeof(what), "setsockopt(level 0x%X, option 0x%X) not on the host, ignored", level, name);
        Announce(what);
        result = 0;
    }
    ctx.r3.u32 = Result(result);
}

// int NetDll_ioctlsocket(caller, SOCKET s, long command, u_long* argument)
PPC_FUNC(__imp__NetDll_ioctlsocket)
{
    Kernel::CountImport("NetDll_ioctlsocket");
    const SOCKET host = HostSocket(ctx.r4.u32);
    if (host == INVALID_SOCKET) { t_lastError = WSAENOTSOCK; ctx.r3.u32 = uint32_t(SOCKET_ERROR); return; }
    u_long argument = ctx.r6.u32 != 0 ? u_long(Guest::Read32(base, ctx.r6.u32)) : 0;
    const int result = ioctlsocket(host, long(ctx.r5.u32), &argument);
    if (result == 0 && ctx.r6.u32 != 0) Guest::Write32(base, ctx.r6.u32, uint32_t(argument));
    ctx.r3.u32 = Result(result);
}

// int NetDll_sendto(caller, SOCKET s, const char* buffer, int length, int flags,
//                   const sockaddr* to, int toLength)
PPC_FUNC(__imp__NetDll_sendto)
{
    Kernel::CountImport("NetDll_sendto");
    const SOCKET host = HostSocket(ctx.r4.u32);
    if (host == INVALID_SOCKET) { t_lastError = WSAENOTSOCK; ctx.r3.u32 = uint32_t(SOCKET_ERROR); return; }
    sockaddr_in to{};
    // The seven arguments are r3 to r9: the caller first, so the address
    // is r8 and its length r9. Read one register on (r9 and r10), every
    // sendto had no address and went nowhere, and no System Link search
    // ever left the machine.
    const bool addressed = ToHost(base, ctx.r8.u32, ctx.r9.u32, to);
    // 0.5.427 / 0.5.428: the game's port 1000 is GamePort() underneath; to a PC met
    // through the master server, its ports 1000 / 50000 are that PC's public ones
    const uint16_t titlePort = addressed ? ntohs(to.sin_port) : 0;   // (the game's own, before the translation)
    // 0.5.435: to this PC's own internet address: to itself, here
    if (addressed && g_internetSelf.load() && to.sin_addr.s_addr == LocalAddress().s_addr) inet_pton(AF_INET, "127.0.0.1", &to.sin_addr);
    if (addressed) to.sin_port = htons(Public::OutPort(to.sin_addr.s_addr, ntohs(to.sin_port)));
    // A broadcast goes out of every adapter, to each network's own
    // broadcast address, as well as the way Windows sends it (above).
    if (addressed && to.sin_addr.s_addr == INADDR_BROADCAST)
    {
        static std::atomic<int> shown{ 0 };
        std::vector<in_addr> targets;
        for (const Adapter& adapter : Adapters())
            if (adapter.broadcasts()) targets.push_back(adapter.broadcast);
        for (const in_addr& peer : Peers()) targets.push_back(peer);
        // 0.5.428: this PC searched; the public games' search ports get the search too
        if (ntohs(to.sin_port) == 50000)
            Public::Searched(host, base + ctx.r5.u32, int(ctx.r6.u32));   // (its copies go from the game socket, 0.5.429)
        // 0.5.430: and to every PC met through the master server, at its public
        // port for that title port (a host's answer to a search is a broadcast)
        // (not a search: its copies go from the game socket, and a second way in made a second lobby, 0.5.431)
        if (titlePort != 50000) for (const uint32_t met : Public::MetAddresses())
        {
            sockaddr_in remote = to;
            remote.sin_addr.s_addr = met;
            remote.sin_port = htons(Public::OutPort(met, titlePort));
            sendto(host, reinterpret_cast<const char*>(base + ctx.r5.u32), int(ctx.r6.u32), int(ctx.r7.u32),
                   reinterpret_cast<const sockaddr*>(&remote), sizeof(remote));
            static int told = 0;
            if (told++ < 10) { printf("public: a broadcast of %u bytes (port %u) also to %s:%u\n", ctx.r6.u32, titlePort, Dotted(remote.sin_addr).c_str(), ntohs(remote.sin_port)); fflush(stdout); }
        }
        for (const in_addr& target : targets)
        {
            sockaddr_in each = to;
            each.sin_addr = target;
            const int sent = sendto(host, reinterpret_cast<const char*>(base + ctx.r5.u32), int(ctx.r6.u32), int(ctx.r7.u32),
                                    reinterpret_cast<sockaddr*>(&each), sizeof(each));
            if ((shown.load(std::memory_order_relaxed) < 8 && shown.fetch_add(1) < 8))
            {
                printf("net: socket %u broadcast %u bytes to %s:%u%s\n", ctx.r4.u32, ctx.r6.u32,
                    Dotted(each.sin_addr).c_str(), ntohs(each.sin_port), sent == SOCKET_ERROR ? " (failed)" : "");
                fflush(stdout);
            }
        }
    }
    const int result = addressed
        ? sendto(host, reinterpret_cast<const char*>(base + ctx.r5.u32), int(ctx.r6.u32), int(ctx.r7.u32),
                 reinterpret_cast<sockaddr*>(&to), sizeof(to))
        : send(host, reinterpret_cast<const char*>(base + ctx.r5.u32), int(ctx.r6.u32), int(ctx.r7.u32));
    if (result != SOCKET_ERROR) g_sent.fetch_add(1);
    if (addressed && result != SOCKET_ERROR && titlePort == TitleGamePort) Public::NoteGameTraffic(true, to.sin_addr.s_addr, int(ctx.r6.u32));   // 0.5.431
    if (addressed && result != SOCKET_ERROR)
    {
        sockaddr_in local{};
        int localLength = sizeof(local);
        getsockname(host, reinterpret_cast<sockaddr*>(&local), &localLength);
        NetDump("sent", ctx.r4.u32, base + ctx.r5.u32, int(ctx.r6.u32), to, ntohs(local.sin_port));   // 0.5.426
    }
    static std::atomic<int> shown{ 0 };
    if ((shown.load(std::memory_order_relaxed) < 12 && shown.fetch_add(1) < 12))
    {
        printf("net: socket %u sent %u bytes to %s:%u%s\n", ctx.r4.u32, ctx.r6.u32,
            addressed ? Dotted(to.sin_addr).c_str() : "its peer", addressed ? ntohs(to.sin_port) : 0,
            result == SOCKET_ERROR ? " (failed)" : "");
        fflush(stdout);
    }
    ctx.r3.u32 = Result(result);
}

// int NetDll_recvfrom(caller, SOCKET s, char* buffer, int length, int flags,
//                     sockaddr* from, int* fromLength)
PPC_FUNC(__imp__NetDll_recvfrom)
{
    Kernel::CountImport("NetDll_recvfrom");
    const SOCKET host = HostSocket(ctx.r4.u32);
    if (host == INVALID_SOCKET) { t_lastError = WSAENOTSOCK; ctx.r3.u32 = uint32_t(SOCKET_ERROR); return; }
    sockaddr_in from{};
    int fromLength = sizeof(from);
    // This socket's own port, to know its own broadcasts when they come
    // back: a console does not hear itself, and a title that did would find
    // its own game in the list, or answer its own search.
    sockaddr_in local{};
    int localLength = sizeof(local);
    getsockname(host, reinterpret_cast<sockaddr*>(&local), &localLength);
    int result;
    // 0.5.427: not at the same time as the public servers' Drain on these two sockets
    std::unique_lock<std::mutex> publicLock(Public::g_readMutex, std::defer_lock);
    if (host == Public::g_game.load() || host == Public::g_search.load() || host == Public::g_searcher.load()) publicLock.lock();
    // 0.5.429: a public game's answer to our search came in on the game socket: here it is
    bool redirected = false;
    if (host == Public::g_searcher.load())
        redirected = Public::PopAnswer(host, base + ctx.r5.u32, int(ctx.r6.u32), from, result);
    for (; !redirected;)
    {
        fromLength = sizeof(from);
        result = recvfrom(host, reinterpret_cast<char*>(base + ctx.r5.u32), int(ctx.r6.u32), int(ctx.r7.u32),
                          reinterpret_cast<sockaddr*>(&from), &fromLength);
        // 0.5.427: a STUN server's answer to the public servers' question is not the game's
        if (result != SOCKET_ERROR && Public::Answer(host, base + ctx.r5.u32, result, from)) continue;
        // 0.5.428: a punch only opens the routers; it is not the game's
        if (result == int(sizeof(Public::Punch)) && memcmp(base + ctx.r5.u32, Public::Punch, sizeof(Public::Punch)) == 0) continue;
        // 0.5.437: a side channel message on the game's way is the side channel's
        if (result > 0 && host == Public::g_game.load() && Public::IsSide(base + ctx.r5.u32, result)) { Public::KeepSide(base + ctx.r5.u32, result, from); continue; }
        // 0.5.429: an answer to our search on the game socket is the search socket's
        if (result > 0 && host == Public::g_game.load() && Public::FromRemoteSearch(from)) { Public::KeepAnswer(base + ctx.r5.u32, result, from); continue; }
        if (result == SOCKET_ERROR || from.sin_port != local.sin_port || !IsOwnAddress(from.sin_addr)) break;
    }
    if (result != SOCKET_ERROR)
    {
        g_received.fetch_add(1);
        {
            std::lock_guard<std::mutex> lock(g_lastFromMutex);
            g_lastFrom = Dotted(from.sin_addr) + ":" + std::to_string(ntohs(from.sin_port));
        }
        g_lastFromAddress.store(from.sin_addr.s_addr);
        from.sin_port = htons(Public::InPort(from.sin_addr.s_addr, ntohs(from.sin_port)));
        if (g_internetSelf.load() && from.sin_addr.s_addr == htonl(INADDR_LOOPBACK)) from.sin_addr = LocalAddress();   // 0.5.435: itself, by its own address
        if (ntohs(from.sin_port) == TitleGamePort) Public::NoteGameTraffic(false, from.sin_addr.s_addr, result);   // 0.5.431   // 0.5.427 / 0.5.428: the game hears "1000" / "50000"
        NetDump("received", ctx.r4.u32, base + ctx.r5.u32, result, from, ntohs(local.sin_port));   // 0.5.426
        // The sender's address at r8 and its length's word at r9 (the
        // caller is r3), as for sendto.
        if (ctx.r8.u32 != 0) ToGuest(base, from, ctx.r8.u32);
        if (ctx.r9.u32 != 0) Guest::Write32(base, ctx.r9.u32, 16);
        static std::atomic<int> shown{ 0 };
        if ((shown.load(std::memory_order_relaxed) < 12 && shown.fetch_add(1) < 12))
        {
            printf("net: socket %u received %d bytes from %s:%u\n", ctx.r4.u32, result,
                Dotted(from.sin_addr).c_str(), ntohs(from.sin_port));
            fflush(stdout);
        }
    }
    ctx.r3.u32 = Result(result);
}

// int NetDll_WSAGetLastError(void)
PPC_FUNC(__imp__NetDll_WSAGetLastError)
{
    Kernel::CountImport("NetDll_WSAGetLastError");
    ctx.r3.u32 = uint32_t(t_lastError);
}

// --- XNet: the secure network layer, as a plain local network -------------------

// int NetDll_XNetStartup(caller, const XNetStartupParams* params)
PPC_FUNC(__imp__NetDll_XNetStartup)
{
    Kernel::CountImport("NetDll_XNetStartup");
    StartWinsock();
    LocalAddress();
    ctx.r3.u32 = 0;
}

// int NetDll_XNetCleanup(caller)
PPC_FUNC(__imp__NetDll_XNetCleanup)
{
    Kernel::CountImport("NetDll_XNetCleanup");
    ctx.r3.u32 = 0;
}

// int NetDll_XNetRandom(caller, BYTE* buffer, UINT length)
PPC_FUNC(__imp__NetDll_XNetRandom)
{
    Kernel::CountImport("NetDll_XNetRandom");
    for (uint32_t i = 0; i < ctx.r5.u32; i++) base[ctx.r4.u32 + i] = uint8_t(Random()());
    ctx.r3.u32 = 0;
}

// DWORD NetDll_XNetGetTitleXnAddr(caller, XNADDR* address)
PPC_FUNC(__imp__NetDll_XNetGetTitleXnAddr)
{
    Kernel::CountImport("NetDll_XNetGetTitleXnAddr");
    if (ctx.r4.u32 != 0) WriteXnAddr(base, ctx.r4.u32, LocalAddress(), true);
    // XNET_GET_XNADDR_ETHERNET | STATIC: an address on a local network, and
    // nothing online.
    ctx.r3.u32 = 0x02 | 0x04;
}

// DWORD NetDll_XNetGetEthernetLinkStatus(caller)
PPC_FUNC(__imp__NetDll_XNetGetEthernetLinkStatus)
{
    Kernel::CountImport("NetDll_XNetGetEthernetLinkStatus");
    // XNET_ETHERNET_LINK_ACTIVE | 100MBPS | FULL_DUPLEX: a cable is in.
    ctx.r3.u32 = 0x01 | 0x02 | 0x08;
}

// int NetDll_XNetXnAddrToInAddr(caller, const XNADDR* address, const XNKID* key, IN_ADDR* out)
PPC_FUNC(__imp__NetDll_XNetXnAddrToInAddr)
{
    Kernel::CountImport("NetDll_XNetXnAddrToInAddr");
    if (ctx.r4.u32 == 0 || ctx.r6.u32 == 0) { ctx.r3.u32 = uint32_t(WSAEINVAL); return; }
    // The first of the machine's addresses on a network this one is on;
    // its main one if none is.
    const uint32_t xn = ctx.r4.u32;
    in_addr chosen{};
    memcpy(&chosen, base + xn, 4);
    // 0.5.428: a PC met through the master server: its public address, before
    // any LAN look (its LAN address may look like one of this network's)
    bool met = false;
    {
        uint32_t candidates[MaxXnAddresses] = {}, outside = 0;
        for (uint32_t slot = 0; slot < MaxXnAddresses; slot++)
            memcpy(&candidates[slot], base + xn + (slot == 0 ? 0 : slot == 1 ? 4 : 16 + (slot - 2) * 4), 4);
        if (Public::PublicFor(candidates, int(MaxXnAddresses), outside)) { chosen.s_addr = outside; met = true; }
        // 0.5.432: this PC's own XNADDR is the address it gives out. Both test
        // PCs resolved themselves to their Tailscale adapter's placeholder
        // 169.254.83.107 (Tailscale off), the same address on both: the join
        // got through the handshake and was then refused.
        if (!met && candidates[0] && candidates[0] == LocalAddress().s_addr) { chosen.s_addr = candidates[0]; met = true; }
    }
    for (uint32_t slot = 0; slot < MaxXnAddresses && !met; slot++)
    {
        const uint32_t at = slot == 0 ? 0 : slot == 1 ? 4 : 16 + (slot - 2) * 4;
        in_addr candidate{};
        memcpy(&candidate, base + xn + at, 4);
        if (candidate.s_addr == 0 || Public::Placeholder(candidate.s_addr)) continue;   // (0.5.432: never a 169.254 placeholder)
        if (Reachable(candidate)) { chosen = candidate; break; }
    }
    memcpy(base + ctx.r6.u32, &chosen, 4);
    static std::atomic<int> shown{ 0 };
    if ((shown.load(std::memory_order_relaxed) < 8 && shown.fetch_add(1) < 8))
    {
        // 0.5.426: the whole XNADDR (36 bytes) and the key, for the public servers' rewrite
        std::string raw, key;
        for (uint32_t i = 0; i < 36; i++) { char t[4]; snprintf(t, sizeof t, "%02X%s", base[xn + i], (i % 4 == 3) ? " " : ""); raw += t; }
        if (ctx.r5.u32 != 0) for (uint32_t i = 0; i < 8; i++) { char t[4]; snprintf(t, sizeof t, "%02X", base[ctx.r5.u32 + i]); key += t; }
        printf("net: a player's address resolved to %s (XNADDR %s key %s)\n", Dotted(chosen).c_str(), raw.c_str(), key.empty() ? "none" : key.c_str());
        fflush(stdout);
    }
    ctx.r3.u32 = 0;
}

// int NetDll_XNetInAddrToXnAddr(caller, IN_ADDR address, XNADDR* out, XNKID* outKey)
PPC_FUNC(__imp__NetDll_XNetInAddrToXnAddr)
{
    Kernel::CountImport("NetDll_XNetInAddrToXnAddr");
    // The address arrives in a register, as the guest loaded it: its bytes
    // in network order are the register's from the top.
    const uint32_t value = ctx.r4.u32;
    in_addr address{};
    uint8_t* b = reinterpret_cast<uint8_t*>(&address);
    b[0] = uint8_t(value >> 24); b[1] = uint8_t(value >> 16); b[2] = uint8_t(value >> 8); b[3] = uint8_t(value);
    // 0.5.434: a PC met through the master server: its XNADDR is made from the
    // address it gives out, as on a LAN (not from its public address)
    uint32_t given = 0;
    if (Public::GivenAddressFor(address.s_addr, given))
    {
        static int told = 0;
        in_addr g{};
        g.s_addr = given;
        if (told++ < 8) { printf("public: the XNADDR for %s is made from its own address %s\n", Dotted(address).c_str(), Dotted(g).c_str()); fflush(stdout); }
        address = g;
    }
    if (ctx.r5.u32 != 0) WriteXnAddr(base, ctx.r5.u32, address);
    if (ctx.r6.u32 != 0) memset(base + ctx.r6.u32, 0, 8);
    ctx.r3.u32 = 0;
}

// int NetDll_XNetTsAddrToInAddr(caller, const TSADDR* address, DWORD service, const XNKID* key, IN_ADDR* out)
PPC_FUNC(__imp__NetDll_XNetTsAddrToInAddr)
{
    Kernel::CountImport("NetDll_XNetTsAddrToInAddr");
    Announce("XNetTsAddrToInAddr: title servers are Xbox Live's, not here");
    ctx.r3.u32 = uint32_t(WSAEINVAL);
}

// int NetDll_XNetInAddrToString(caller, IN_ADDR address, char* text, int length)
PPC_FUNC(__imp__NetDll_XNetInAddrToString)
{
    Kernel::CountImport("NetDll_XNetInAddrToString");
    const uint32_t v = ctx.r4.u32;
    char text[32];
    snprintf(text, sizeof(text), "%u.%u.%u.%u", v >> 24, (v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
    if (ctx.r5.u32 != 0 && ctx.r6.u32 > 0)
    {
        const size_t n = std::min<size_t>(strlen(text), ctx.r6.u32 - 1);
        memcpy(base + ctx.r5.u32, text, n);
        base[ctx.r5.u32 + n] = 0;
    }
    ctx.r3.u32 = 0;
}

// int NetDll_XNetUnregisterInAddr(caller, IN_ADDR address)
PPC_FUNC(__imp__NetDll_XNetUnregisterInAddr) { Kernel::CountImport("NetDll_XNetUnregisterInAddr"); ctx.r3.u32 = 0; }

// int NetDll_XNetConnect(caller, IN_ADDR address)
PPC_FUNC(__imp__NetDll_XNetConnect) { Kernel::CountImport("NetDll_XNetConnect"); ctx.r3.u32 = 0; }

// DWORD NetDll_XNetGetConnectStatus(caller, IN_ADDR address)
PPC_FUNC(__imp__NetDll_XNetGetConnectStatus)
{
    Kernel::CountImport("NetDll_XNetGetConnectStatus");
    ctx.r3.u32 = 2;   // XNET_CONNECT_STATUS_CONNECTED: there is nothing to negotiate on a LAN
}

// int NetDll_XNetCreateKey(caller, XNKID* id, XNKEY* key)
PPC_FUNC(__imp__NetDll_XNetCreateKey)
{
    Kernel::CountImport("NetDll_XNetCreateKey");
    for (uint32_t i = 0; i < 8 && ctx.r4.u32 != 0; i++) base[ctx.r4.u32 + i] = uint8_t(Random()());
    for (uint32_t i = 0; i < 16 && ctx.r5.u32 != 0; i++) base[ctx.r5.u32 + i] = uint8_t(Random()());
    // The key's kind is in the top bits of its first byte: a System Link
    // session's are zero.
    if (ctx.r4.u32 != 0) base[ctx.r4.u32] &= 0x0F;
    ctx.r3.u32 = 0;
}

// int NetDll_XNetRegisterKey(caller, const XNKID* id, const XNKEY* key)
PPC_FUNC(__imp__NetDll_XNetRegisterKey) { Kernel::CountImport("NetDll_XNetRegisterKey"); ctx.r3.u32 = 0; }

// int NetDll_XNetUnregisterKey(caller, const XNKID* id)
PPC_FUNC(__imp__NetDll_XNetUnregisterKey) { Kernel::CountImport("NetDll_XNetUnregisterKey"); ctx.r3.u32 = 0; }

// int NetDll_XNetQosListen(caller, const XNKID* id, const BYTE* data, UINT length, DWORD bitsPerSecond, DWORD flags)
PPC_FUNC(__imp__NetDll_XNetQosListen) { Kernel::CountImport("NetDll_XNetQosListen"); ctx.r3.u32 = 0; }

// int NetDll_XNetQosLookup(caller, ... XNQOS** out): the quality of service
// probes are Xbox Live's matchmaking. Refused: the title then lists a
// session without them.
PPC_FUNC(__imp__NetDll_XNetQosLookup)
{
    Kernel::CountImport("NetDll_XNetQosLookup");
    Announce("XNetQosLookup refused: no Xbox Live quality probes here");
    ctx.r3.u32 = uint32_t(WSAEOPNOTSUPP);
}

// --- XAM: sessions, friends, privileges, voice ------------------------------------

// DWORD XamUserCheckPrivilege(DWORD user, DWORD privilege, BOOL* result)
PPC_FUNC(__imp__XamUserCheckPrivilege)
{
    Kernel::CountImport("XamUserCheckPrivilege");
    // Everything allowed: the player owns the machine.
    if (ctx.r5.u32 != 0) Guest::Write32(base, ctx.r5.u32, 1);
    ctx.r3.u32 = 0;
}

// DWORD XamUserAreUsersFriends(DWORD user, const XUID* xuids, DWORD count, BOOL* result, XOVERLAPPED* overlapped)
PPC_FUNC(__imp__XamUserAreUsersFriends)
{
    Kernel::CountImport("XamUserAreUsersFriends");
    if (ctx.r6.u32 != 0)
        for (uint32_t i = 0; i < ctx.r5.u32; i++) Guest::Write32(base, ctx.r6.u32 + i * 4, 0);
    ctx.r3.u32 = 0;
}

// DWORD XamUserCreateStatsEnumerator(...): leaderboards are Xbox Live's.
PPC_FUNC(__imp__XamUserCreateStatsEnumerator)
{
    Kernel::CountImport("XamUserCreateStatsEnumerator");
    Announce("XamUserCreateStatsEnumerator refused: no Xbox Live leaderboards");
    ctx.r3.u32 = 0x80070005;   // access denied: not signed in to Live
}

// DWORD XamShowGamerCardUIForXUID(DWORD user, XUID xuid)
PPC_FUNC(__imp__XamShowGamerCardUIForXUID)
{
    Kernel::CountImport("XamShowGamerCardUIForXUID");
    ctx.r3.u32 = 0x000004C7;   // ERROR_CANCELLED: as if the guide was closed again
}

// DWORD XamSessionCreateHandle(HANDLE* handle)
PPC_FUNC(__imp__XamSessionCreateHandle)
{
    Kernel::CountImport("XamSessionCreateHandle");
    // A session handle is something to hand back to XamSessionRefObjByHandle
    // and to close; an event of the runtime's own serves.
    static std::atomic<uint32_t> next{ 0xF5000000u };
    if (ctx.r3.u32 != 0) Guest::Write32(base, ctx.r3.u32, next.fetch_add(4));
    ctx.r3.u32 = 0;
}

// DWORD XamSessionRefObjByHandle(HANDLE handle, void** object)
PPC_FUNC(__imp__XamSessionRefObjByHandle)
{
    Kernel::CountImport("XamSessionRefObjByHandle");
    // A block of guest memory stands for the session object: the title only
    // passes it on to the session calls, which are its own code over XNet.
    static std::mutex mutex;
    static std::map<uint32_t, uint32_t> objects;
    uint32_t object;
    {
        std::lock_guard<std::mutex> lock(mutex);
        uint32_t& slot = objects[ctx.r3.u32];
        if (slot == 0) slot = Guest::AllocateKernelObject(256);
        object = slot;
    }
    if (ctx.r4.u32 != 0) Guest::Write32(base, ctx.r4.u32, object);
    ctx.r3.u32 = object != 0 ? 0 : 0x8007000E;
}

// DWORD XamVoiceCreate(DWORD user, DWORD flags, void** voice): no headset here.
PPC_FUNC(__imp__XamVoiceCreate)
{
    Kernel::CountImport("XamVoiceCreate");
    if (ctx.r5.u32 != 0) Guest::Write32(base, ctx.r5.u32, 0);
    ctx.r3.u32 = 0x8007048F;   // ERROR_DEVICE_NOT_CONNECTED
}

// BOOL XamVoiceHeadsetPresent(void* voice)
PPC_FUNC(__imp__XamVoiceHeadsetPresent) { Kernel::CountImport("XamVoiceHeadsetPresent"); ctx.r3.u32 = 0; }

// DWORD XamVoiceClose(void* voice)
PPC_FUNC(__imp__XamVoiceClose) { Kernel::CountImport("XamVoiceClose"); ctx.r3.u32 = 0; }

// DWORD XamVoiceSubmitPacket(void* voice, DWORD channel, const void* data)
PPC_FUNC(__imp__XamVoiceSubmitPacket) { Kernel::CountImport("XamVoiceSubmitPacket"); ctx.r3.u32 = 0; }

// DWORD XMsgCancelIORequest(XOVERLAPPED* overlapped, BOOL wait)
PPC_FUNC(__imp__XMsgCancelIORequest) { Kernel::CountImport("XMsgCancelIORequest"); ctx.r3.u32 = 0; }

// --- The settings menu's view of it ------------------------------------------------

Net::Summary Net::Status()
{
    Summary summary;
    const in_addr address = LocalAddress();
    summary.address = Dotted(address) + " (" + g_addressWhy + ")";
    for (const Adapter& adapter : Adapters())
        summary.adapters.push_back(adapter.name + ": " + Dotted(adapter.address) +
            (adapter.broadcasts() ? "" : ", no broadcasts: friends need adding as peers") +
            (adapter.vpn ? " (virtual LAN)" : ""));
    for (const in_addr& peer : Peers()) summary.peers.push_back(Dotted(peer));
    {
        std::lock_guard<std::mutex> lock(g_socketMutex);
        summary.sockets = unsigned(g_sockets.size());
    }
    summary.sent = g_sent.load();
    summary.received = g_received.load();
    {
        std::lock_guard<std::mutex> lock(g_lastFromMutex);
        summary.lastFrom = g_lastFrom;
    }
    return summary;
}


// The side channel: a small UDP socket of this runtime's own (port 28964),
// for what the title itself never sends to a joiner (the custom rules). A
// message goes where a System Link search goes (each network's broadcast
// address and the peers list) and to the machine the game last heard from.
namespace Net
{
    namespace
    {
        constexpr u_short SidePort = 28964;
        SOCKET g_side = INVALID_SOCKET;
        std::once_flag g_sideOpened;

        SOCKET Side()
        {
            std::call_once(g_sideOpened, [] {
                StartWinsock();
                SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                if (s == INVALID_SOCKET) return;
                BOOL yes = TRUE;
                setsockopt(s, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&yes), sizeof(yes));
                setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&yes), sizeof(yes));
                u_long nonBlocking = 1;
                ioctlsocket(s, FIONBIO, &nonBlocking);
                sockaddr_in local{};
                local.sin_family = AF_INET;
                local.sin_port = htons(SidePort);
                local.sin_addr.s_addr = htonl(INADDR_ANY);
                const bool bound = bind(s, reinterpret_cast<sockaddr*>(&local), sizeof(local)) == 0;
                printf("net: side channel on port %u%s\n", SidePort, bound ? "" : " (could not bind: sending only)");
                fflush(stdout);
                g_side = s;
            });
            return g_side;
        }

        void SendTo(SOCKET s, const void* data, int size, in_addr address)
        {
            sockaddr_in to{};
            to.sin_family = AF_INET;
            to.sin_port = htons(SidePort);
            to.sin_addr = address;
            sendto(s, reinterpret_cast<const char*>(data), size, 0, reinterpret_cast<sockaddr*>(&to), sizeof(to));
        }
    }

    void SideSend(const void* data, int size, uint32_t only)
    {
        // 0.5.437: met PCs (Public game) get it on the game's way, and not
        // again at their port 28964 (which their router drops, or not: twice)
        const bool viaGame = Public::SideOut(data, size, only);
        if (only != 0 && viaGame) return;
        const SOCKET s = Side();
        if (s == INVALID_SOCKET) return;
        if (only != 0)
        {
            in_addr address{}; address.s_addr = only;
            SendTo(s, data, size, address);
            return;
        }
        std::vector<in_addr> targets;
        in_addr everyone{}; everyone.s_addr = INADDR_BROADCAST;
        targets.push_back(everyone);
        for (const Adapter& adapter : Adapters())
            if (adapter.broadcasts()) targets.push_back(adapter.broadcast);
        for (const in_addr& peer : Peers()) targets.push_back(peer);
        if (const uint32_t last = g_lastFromAddress.load(); last && !(viaGame && Public::IsMet(last))) { in_addr a{}; a.s_addr = last; targets.push_back(a); }
        for (const in_addr& target : targets) SendTo(s, data, size, target);
    }

    int SideReceive(void* data, int size, uint32_t* from)
    {
        if (const int kept = Public::PopSide(data, size, from); kept >= 0) return kept;   // 0.5.437: from a met PC, on the game's way
        const SOCKET s = Side();
        if (s == INVALID_SOCKET) return -1;
        for (;;)
        {
            sockaddr_in sender{};
            int length = sizeof(sender);
            const int got = recvfrom(s, reinterpret_cast<char*>(data), size, 0, reinterpret_cast<sockaddr*>(&sender), &length);
            if (got == SOCKET_ERROR) return -1;
            if (IsOwnAddress(sender.sin_addr)) continue;   // our own broadcast
            if (from) *from = sender.sin_addr.s_addr;
            return got;
        }
    }
}
