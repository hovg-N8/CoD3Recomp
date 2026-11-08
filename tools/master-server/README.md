# CoD3 PC Port master server

The public game list and the hole punching's introductions, as a Cloudflare
Worker. It carries no game traffic: hosts announce themselves, players fetch
the list, and a joiner's request to join is passed to the host so both can
send packets at each other's public address. See `docs/PUBLIC-SERVERS.md`.

## Live

https://cod3-master.cod3port.workers.dev (deployed 2026-10-24; `/health` answers
`{"ok":true,...}`). On Windows, PowerShell blocks `npx` (a script): use
`npx.cmd wrangler deploy`.

## Deploy (once)

1. Make a free Cloudflare account at https://dash.cloudflare.com/sign-up.
2. Install Node.js (LTS) from https://nodejs.org.
3. In this folder:

   ```
   npx wrangler login
   npx wrangler deploy
   ```

   `login` opens the browser to allow it; `deploy` prints the address, like
   `https://cod3-master.<your-name>.workers.dev`.
4. Check it: open `https://cod3-master.<your-name>.workers.dev/health` in a
   browser. It should say `{"ok":true,"servers":0}`.
5. Put that address in the game's settings (`master_server` in CoD3Recomp.ini,
   once the game side is built).

Updating later: change `src/index.js`, run `npx wrangler deploy` again.

## Free plan limits

100,000 requests a day. A host announces every 10 s (8,640 a day per host
running all day) and polls for joiners every 2 s while hosting; a browsing
player fetches the list when the game searches. A few dozen hosts at once fit.

## API

| Request | Body / query | Answer |
| --- | --- | --- |
| `POST /announce` | `{id?, secret?, name, map, mode, players, maxPlayers, version, game: "ip:port", search: "ip:port", lan?}` | `{id, secret, ip}` |
| `POST /remove` | `{id, secret}` | `{ok}` |
| `GET /list?version=V` | | `{servers: [{id, ip, name, map, mode, players, maxPlayers, version, game, search, age}]}` |
| `POST /punch` | `{server, version, game, search}` | `{ok, host: {game, search}}` |
| `POST /punches` | `{id, secret}` | `{joiners: [{game, search, ip, at}]}` |
| `GET /health` | | `{ok, servers}` |

Hosts drop off the list 30 s after their last announce; introductions last 20 s.
