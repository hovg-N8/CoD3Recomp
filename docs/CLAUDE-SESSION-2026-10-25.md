# Claude session 2026-10-24 / 25: 0.5.426 to 0.5.478 (multiplayer)

Written at the end of the session for the next agent. Source and local install
are **0.5.478** (built OK 2026-10-25 22:13; friend zip
`D:\Call of Duty 3 (USA, Europe)\CoD3MP-update-0.5.478.zip`). Every version has
its own entry at the top of `docs/PATCH-NOTES.md`; this file is the overview, the
open threads and the how-tos. Justin is testing 0.5.478 now.

## Latest (0.5.479, 2026-10-25 22:50)

- **Long-game freeze (top issue).** 0.5.478 CTF, Rouen, 31 bots, froze at
  about 9.5 min: watchdog no-swap; heap 0 (descriptor 0x82A84120, +1156
  capacity, +1160 used, +1164 peak) at 611 of 637 MB; a cache (82A91DE0,
  caller 82185D80) asking 50432 bytes for ever ("heapwait: audit"). Earlier
  the 128-byte effects (16000) and "apsEffect : pool is empty". The map
  block cannot grow (extended window 0x48000000-0x6F000000 full). 0.5.479
  logs "heap: game heap X of Y MB used ... +N MB in 30 s": if it climbs all
  match it is a leak; then hook the MP heap (dlmalloc; locked alloc
  sub_820DE188 ~ SP sub_820CBC40, free sub_820DE470 ~ SP sub_820CBF58;
  wrappers 820DE750/820DE848/820DE080/820DE7B8/820DE8B0, matches 0.6-0.9,
  verify) and count live bytes by caller. Suspects to rule out: things
  done per spawn (MakeString in the bot spawn), the killcam, the Hellfire.
- **CTF.** Spawns by own flag WORK (0.5.476; 150+ "spawns moved"). Bots
  reached the enemy flag and pressed use 23 times in 0.5.478, never took it
  (props 49/50 never moved); they stood still within 80. 0.5.479: walk onto
  the flag's exact place while pressing use (BotFlagUse), 5 s. If still no
  "prop 49/50 moved": the pickup may need something else (in 0.5.475 slot 10
  did take it once); compare with Justin taking it.

## What this session did, by theme

**Public internet games without Tailscale (0.5.426-0.5.437).** A master list
(`masterServer`, Cloudflare worker), host on the public list, join by internet
address (0.5.435 confirmed working between Justin and his friend). 0.5.437: the
side channel (UAV, custom rules, bots' roster and lives, Domination) rides the
game's own connection (`CoD3SIDE` tag, kernel_net.cpp Public::SideOut/KeepSide/
PopSide). 0.5.436: the log flood with bots fixed.

**Hellfire Barrage killstreak (0.5.438-0.5.455, mp_fixes.cpp).** Was "Typhoon";
d-pad right; map-wide: 200 shells on a grid over the whole map, the game's own
artillery shell entities (pool of 48, used by turns) with the
`projexplode_mp_binoculars_<surface>` events, kills in the kill feed as
artillery, the glow parked and faded after 6 s, bombers overhead (C3KB/C3KH
packets), one "artillery called" line only. Killstreaks at 2/4/6/8, one set per
life, unused ones carry over, killstreak kills earn no streak. Justin: "it was
intense bro" (0.5.454/455 accepted).

**Freezes and memory.** 0.5.446: effects pools x4 (killcam freeze with 30
bots). 0.5.468: XMA decodes capped at 128 a frame (the 31-bot sound flood,
"8 fps alien"). 0.5.470: a mutant no thread owned but not reading free
(kernel_objects.cpp, repaired after 3 s; fired once in 0.5.471, no freeze).
**0.5.476: the 1 KB particle pool x8** (it ran empty 40 times in 0.5.475's
CTF, then the host hung in the game's malloc, sub_820DF428, in a killcam; the
friend's PC was fine). Watch for `apsMemory : pool(size=1024 ...) is empty`.

**Lobby to 32 players (0.5.458-0.5.462, lobby_rules.cpp).** The create screen's
max players list capacity (sub_824E8FC8 6 -> 8) and the 28/32 entries
(sub_823AA120). Confirmed: "the max players list goes to 32 now".

**Small player features (0.5.455).** Noclip toggled by a double tap of A once
the console's noclip was used; the return point is an F11 cheat (off by
default, `return_point`); brightness slider to 200%.

**Bots (bots.cpp; full list in docs/BOT-PATCH-LOG.md).** Aim at the body with
settling spread, slower reaction, sights past 250; one grenade at a time;
packs used; bazooka pistol swap; crouch/prone; the long way round; spawn
relocation away from enemies; routes over the learnt map (A*), look round for
doors, stairs, unstuck; War takers (0.5.471, the flags change hands); CTF,
SCTF, HQ (0.5.463-0.5.476, below).

**Ported from FrankHUN88453's CoD3Recomp (upstream, 7 commits ahead of the
fork).** 0.5.477 the lamps' coronas fade behind walls (cpu_depth.cpp, using
OUR depth write back, not his compute path); 0.5.478 button prompts as keys
with the automatic pad/keyboard switch (pc_prompts.cpp, render_prompts.cpp,
F11 Controls "Button prompts"). Already ours before: the shader loop-id fix and
direct mouse look. Not taken: his single-exe/self-updater/embedded FFmpeg,
mouse menus, video menu, widescreen, campaign sprint (all on SP addresses).
**Do not "sync fork" on GitHub**: his 0.5.0 numbering and changes to input/
overlay/settings/renderer/installer would conflict with ours; port piece by
piece.

## CTF where it stands (the main open thread)

Rouen, CTF, 31 bots. What the logs showed and what was done:

- Flags: the two placed trigger_multiples off 0 0 0, -1792 -91 and 2112 462
  (0.5.473; model names read empty in 0.5.472). Flag props: script_model 49
  (-1792 -91) and 50 (2112 462); 83/84 the stands.
- The flag is taken with the USE button, not by touch (0.5.474: BotFlagUse,
  400 ms on / 200 off for 3 s within 80 of a flag). In 0.5.475 slot 10 took
  the enemy flag (prop 49 left its base), then died; it lay at -71 284.
- The game spawns each side BY THE ENEMY'S FLAG (allies about 1300 1600 by
  2112 462, axis about -1650 -1100 by -1792 -91). Justin wants each side to
  spawn by its own flag. 0.5.475 sets `g_ctfOwn` once a map (own flag = the one
  farther from the side's game spawns; logged "bots: ctf sides: ...") and
  CtfSpawnSides moves spawns (players too) with the game's teleport
  sub_8255E118. 0.5.475 moved only 17 (it wanted a free map spawn point; 1-5 per
  flag). **0.5.476 moves every spawn more than 900 from its own flag**, to map
  spawn points within 1400 of it or the other side's recorded game spawns, the
  one with most room; logs "ctf: N spawns moved" every 25. UNTESTED.
- 0.5.476: a flag whose prop is 100+ off its base is "out" (g_obj.out/live):
  attackers go to it, defenders chase their own, use is pressed there.
  Defenders: campers + one bot in three (0.5.475). UNTESTED.
- Still bad (Justin, 0.5.475): "no bots actually walked up to the enemy flag",
  bots stuck in spawn / walking into walls. With spawns now at the far ends the
  route across the map is long; the next log shows whether they make it. Look
  at `bots: mobility slot N role ... route R stalled S`, "stuck (no progress",
  "unstuck: put" and where they cluster (spawn exits). Possible next steps: a
  spawn-exit waypoint per base learnt from where living players leave spawn;
  check g_mapKnow has links across the middle of Rouen.

What to grep in a CTF log: `ctf sides`, `ctf: slot`, `spawns moved`,
`presses use`, `on the enemy flag`, `home with the flag`, `prop 49 moved`,
`prop 50 moved`, `plays ctf`.

## Other open items Justin asked for (not done)

- Brightness as a slider in the MP Options menu (as in the campaign), not only
  F11.
- Room clearing for bots (checking each room before moving on).
- Weapon sound flood at its source (capped only).
- Auto-updater: on hold (Justin said hold off). Upstream's updater.cpp is a
  model, but it pulls his releases; decide private vs public first.

## Checks for 0.5.477 / 0.5.478 (Justin testing now)

- Lamps: "render: the title reads the depth resolved to ... on the CPU" once a
  lamp is in view. If never: the MP match (sub_82438DF0 / 0x82BCC8AC in
  mp_addresses.txt) is wrong.
- Prompts: "prompts: the button icon font XXXXXXXX: first 32, N characters,
  texture ..." and "render: the button prompts drawn as keys (N cells)". If the
  font line ends "(not as expected ...)" the MP font layout differs
  (FrontEndManager 0x82C6B1FC, offsets +8/+4/+36/+40/+44, texture +20/+32).

## How-tos learnt this session

- **Build:** write `build-request.txt` (any text) in the source root; the
  auto-builder writes `build-status.txt` "building:/built:/failed: 0.5.N";
  the version auto-increments. Wait with `grep -qE '^(built|failed): 0\.5\.N'`
  (the description may contain the next number). No test request unless one
  is wanted (Justin: "no need to run .bat files").
- **Friend zip:** `powershell -NoProfile -ExecutionPolicy Bypass -File
  "D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0\MAKE-FRIEND-UPDATE.ps1"`
  makes `D:\Call of Duty 3 (USA, Europe)\CoD3MP-update-<ver>.zip` (61 MB). Do
  not change the PowerShell execution policy itself.
- **Logs:** `CoD3-PC-Port-0.4.0\CoD3MP.log` (this launch) and
  `CoD3MP.previous.log` (one before): a second launch overwrites; ask Justin to
  send right after a match. Friend logs come as `Downloads\CoD3MP (N).log`.
- **Multiplayer addresses:** host code is written with SP addresses and
  `scripts/make_mp_host.py` maps every one through `CoD3Host/mp_addresses.txt`
  (LF line endings; a SP-range address with no entry fails the MP build, even
  in a comment). New tools: `tools/mpmatch/mpmatch.py` (functions) and
  `tools/mpmatch/addrvote.py` (data). Dry run: `python scripts/make_mp_host.py
  <tmp dir>`.
- **Harness pitfalls:** heredoc Python turns "\\n" into real newlines inside
  printf strings: put such strings in with the Edit tool and check with
  `grep -n 'printf("[^"]*$'`. `near` is a Windows macro. Re-Read before Edit
  when a file changed.
- **Hang diagnosis:** the watchdog prints "what each thread is blocked on" and
  host stacks; a main thread inside sub_820DF428 (malloc) means a broken heap,
  usually after a pool ran empty.

## Rules (unchanged)

Never upload game data to GitHub (ISO, .cod/.xex, generated ppc/ppc_mp,
textures, shaders, exes). CPU target stays x86-64-v2 (no AVX2). Do not change
security settings (execution policy).
