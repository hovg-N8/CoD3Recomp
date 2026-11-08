# Public servers: finding and joining games over the internet

Goal (Justin, 2026-10-24): a public release where anyone can host a game and
anyone can find and join it, as in CoD4 and World at War, without Tailscale or
typing addresses.

Decided:
- Getting through routers: **UDP hole punching**, with **port forwarding** as
  the fallback for networks it cannot open.
- The game list: a **master server on Cloudflare Workers** (free plan;
  `tools/master-server`), **deployed 2026-10-24 at
  https://cod3-master.cod3port.workers.dev** (Justin's account, subdomain
  cod3port). All endpoints tested (announce, list, punch, punches, remove).
  Its `ip` field can be IPv6 (the request's own address): the game must send
  the IPv4 endpoints it learns from STUN, not rely on `ip`.
- Where players browse: **inside the game's own System Link list**.

## What System Link does today (0.5.425, kernel_net.cpp)

| Step | Port | How |
| --- | --- | --- |
| Search | UDP 50000 | broadcast to 255.255.255.255:50000; the port sends it out of every adapter and to each address in `system_link_peers` (Tailscale) |
| Answer | UDP 50000 | the host answers from its port 50000 with its game's details and its XNADDR |
| Join / play | UDP 1000 (VDP) | the joiner turns the host's XNADDR into an IP (`NetDll_XNetXnAddrToInAddr`: the XNADDR carries up to 7 of the host's addresses; the first reachable is used) and talks to IP:1000 |
| Side channel | UDP 28964 | the port's own: lobby rules, UAV plane, kill feed |

Seen in a real join (friend's 0.5.416 log): search out to :50000, one answer
back from the host's :50000, then both sides talking :1000 to :1000.

## Plan

1. **Learn the packets (0.5.426).** `netdump:` lines: the first 6 packets each
   way on ports 50000 and 1000 (256 bytes each), and the whole XNADDR and key
   when an address is resolved. Needed: the search and answer layout (where the
   game's name, map, mode, players and the host's XNADDR sit, and whether the
   answer echoes a value from the search).
2. **Master server (written, not deployed).** `tools/master-server`: announce,
   list by build, introductions (`/punch`, `/punches`), entries expire after
   30 s. Workers have no UDP, so the game learns its own public address from a
   public STUN server.
3. **Host side.** When a System Link game is hosted and "public" is on:
   - a STUN binding request from the game's own port 1000 socket (and 50000)
     to learn the public address the router gives it;
   - announce every 10 s (name from the settings, map, mode, players, build);
   - poll `/punches` every 2 s and send a few packets to each new joiner from
     ports 1000 and 50000, which opens the host's router to them;
   - remove itself when the game ends.
4. **Joiner side.** When the game searches:
   - fetch `/list` for this build;
   - either inject each listed game's answer into the search socket (if the
     answer can be stored at the master server and replayed) or punch to each
     listed host's 50000 and send the real search there;
   - on join, `/punch` the host, send packets to its public 1000, and map the
     host's address: the game keeps talking to "its" address, and the network
     layer swaps in the public IP and port both ways (sendto / recvfrom /
     XNetXnAddrToInAddr).
5. **Fallback.** No answer after ~5 s of punching: a message telling the host
   to forward UDP 1000 and 50000 (a forwarded host is reached directly).
6. **Before release.** Packet checks (sizes, rates) on everything from the
   internet; build version check; testing outside Tailscale.

## Open questions

- The search / answer layout (step 1's dump will say).
- Whether the game's join handshake on 1000 checks the source port (the
  router may change it): the address map in step 4 hides it either way.
- Symmetric NATs (some mobile / office networks) cannot be punched: fallback.

## Status

- 0.5.426: netdump (packet layout still to be read from a real 2-PC game).
- 0.5.427: DONE and tested (BENCH-PUBLIC.bat, 2026-10-24): hosting detected (search port 50000 bound), STUN from both sockets,
  announced as "HOVGtv's game" (game 66.131.169.81:28960, search 66.131.169.81:50000), seen on /list while the game ran.
  The game's port 1000 is 28960 underneath (game_port): Justin's line drops UDP 1000 (STUN test: no answer from 1000,
  answers from 50000 / 3074 / 28960, port kept by the router). All PCs need 0.5.427+.
- 0.5.428: joining written (list on search, introductions, punches both ways, XNADDR -> public address matched on the
  whole address set, public ports both ways, punch packets filtered); remove-on-exit tested (list empty right after the
  benchmark closed); announce now carries the LAN list. NOT yet tested between two homes.
- (done in 0.5.428) remove from the list when the process exits (the entry stayed until the 30 s expiry after BENCH-PUBLIC closed);
  map / mode / players from the search answer (needs the netdump); step 3 joining (punch + address map).
