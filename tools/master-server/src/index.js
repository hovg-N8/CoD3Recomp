// CoD3 PC Port master server: the public game list and the hole punching's
// introductions, on Cloudflare Workers (one Durable Object holds the list).
//
// Hosts announce themselves every 10 s and drop off 30 s after their last
// announce. Players fetch the list for their build. A player who wants to
// join asks for an introduction; the host picks it up on its next poll and
// both send packets at each other's public UDP address (learned from a
// public STUN server by the game itself), which opens both routers.
//
// Nothing here carries game traffic: only small JSON requests.
//
// API (all JSON):
//   POST /announce  {id?, secret?, name, map, mode, players, maxPlayers, version,
//                    game: "ip:port", search: "ip:port", lan?: ["ip", ...]}
//                   -> {id, secret, ip}      (ip: the address this request came from)
//   POST /remove    {id, secret}
//   GET  /list?version=V                    -> {servers: [...]}
//   POST /punch     {server, version, game: "ip:port", search: "ip:port", lan?: ["ip", ...]}
//                   -> {ok, host: {game, search}}
//   POST /punches   {id, secret}            -> {joiners: [{game, search, lan, ip, at}]}
//   GET  /health                            -> {ok, servers}

const ANNOUNCE_TTL_MS = 30_000;
const PUNCH_TTL_MS = 20_000;
const MAX_SERVERS = 2000;
const MAX_PUNCHES_PER_SERVER = 64;

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    if (request.method === "OPTIONS") return cors(new Response(null, { status: 204 }));
    const id = env.DIRECTORY.idFromName("global");
    const stub = env.DIRECTORY.get(id);
    const forwarded = new Request(url.toString(), request);
    forwarded.headers.set("x-client-ip", request.headers.get("CF-Connecting-IP") || "");
    return cors(await stub.fetch(forwarded));
  },
};

function cors(response) {
  const r = new Response(response.body, response);
  r.headers.set("Access-Control-Allow-Origin", "*");
  r.headers.set("Access-Control-Allow-Headers", "content-type");
  return r;
}

function json(value, status = 200) {
  return new Response(JSON.stringify(value), { status, headers: { "content-type": "application/json" } });
}

function text(value, max) {
  return typeof value === "string" ? value.slice(0, max) : "";
}

function int(value, lo, hi) {
  const n = Number.isFinite(value) ? Math.trunc(value) : 0;
  return Math.min(hi, Math.max(lo, n));
}

// "a.b.c.d:port", IPv4 only (the game is IPv4).
function endpoint(value) {
  if (typeof value !== "string") return "";
  const m = /^(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3}):(\d{1,5})$/.exec(value);
  if (!m) return "";
  const parts = m.slice(1, 5).map(Number);
  const port = Number(m[5]);
  if (parts.some((p) => p > 255) || port < 1 || port > 65535) return "";
  return value;
}

function randomHex(bytes) {
  const a = new Uint8Array(bytes);
  crypto.getRandomValues(a);
  return [...a].map((b) => b.toString(16).padStart(2, "0")).join("");
}

export class Directory {
  constructor(state, env) {
    this.state = state;
    this.servers = new Map(); // id -> {secret, info, ip, seen}
    this.punches = new Map(); // id -> [{game, search, ip, at}]
  }

  sweep(now) {
    for (const [id, s] of this.servers) if (now - s.seen > ANNOUNCE_TTL_MS) { this.servers.delete(id); this.punches.delete(id); }
    for (const [id, list] of this.punches) {
      const fresh = list.filter((p) => now - p.at < PUNCH_TTL_MS);
      if (fresh.length) this.punches.set(id, fresh); else this.punches.delete(id);
    }
  }

  async fetch(request) {
    const url = new URL(request.url);
    const now = Date.now();
    this.sweep(now);
    const ip = request.headers.get("x-client-ip") || "";
    let body = {};
    if (request.method === "POST") {
      try { body = await request.json(); } catch { return json({ error: "bad json" }, 400); }
      if (typeof body !== "object" || body === null) return json({ error: "bad json" }, 400);
    }

    switch (`${request.method} ${url.pathname}`) {
      case "GET /health":
        return json({ ok: true, servers: this.servers.size });

      case "POST /announce": {
        let id = text(body.id, 32), secret = text(body.secret, 64);
        const existing = id && this.servers.get(id);
        if (existing && existing.secret !== secret) return json({ error: "not yours" }, 403);
        if (!existing) {
          if (this.servers.size >= MAX_SERVERS) return json({ error: "full" }, 503);
          id = randomHex(8); secret = randomHex(16);
        }
        const info = {
          name: text(body.name, 48) || "CoD3 server",
          map: text(body.map, 32),
          mode: text(body.mode, 32),
          players: int(body.players, 0, 64),
          maxPlayers: int(body.maxPlayers, 1, 64),
          version: text(body.version, 16),
          game: endpoint(body.game),
          search: endpoint(body.search),
          lan: Array.isArray(body.lan) ? body.lan.slice(0, 4).map((a) => text(a, 15)) : [],
        };
        if (!info.version || !info.game) return json({ error: "version and game endpoint needed" }, 400);
        this.servers.set(id, { secret, info, ip, seen: now });
        return json({ id, secret, ip });
      }

      case "POST /remove": {
        const s = this.servers.get(text(body.id, 32));
        if (s && s.secret === text(body.secret, 64)) { this.servers.delete(text(body.id, 32)); this.punches.delete(text(body.id, 32)); }
        return json({ ok: true });
      }

      case "GET /list": {
        const version = url.searchParams.get("version") || "";
        const servers = [];
        for (const [id, s] of this.servers) {
          if (version && s.info.version !== version) continue;
          servers.push({ id, ip: s.ip, ...s.info, age: Math.round((now - s.seen) / 1000) });
        }
        return json({ servers });
      }

      case "POST /punch": {
        const id = text(body.server, 32);
        const s = this.servers.get(id);
        if (!s) return json({ error: "no such server" }, 404);
        if (text(body.version, 16) !== s.info.version) return json({ error: "different build" }, 409);
        const joiner = { game: endpoint(body.game), search: endpoint(body.search), lan: Array.isArray(body.lan) ? body.lan.slice(0, 4).map((x) => text(x, 15)) : [], ip, at: now };
        if (!joiner.game) return json({ error: "game endpoint needed" }, 400);
        const list = (this.punches.get(id) || []).filter((p) => p.game !== joiner.game);
        list.push(joiner);
        this.punches.set(id, list.slice(-MAX_PUNCHES_PER_SERVER));
        return json({ ok: true, host: { game: s.info.game, search: s.info.search } });
      }

      case "POST /punches": {
        const id = text(body.id, 32);
        const s = this.servers.get(id);
        if (!s || s.secret !== text(body.secret, 64)) return json({ error: "not yours" }, 403);
        const joiners = this.punches.get(id) || [];
        this.punches.delete(id);
        return json({ joiners });
      }
    }
    return json({ error: "not found" }, 404);
  }
}
