<!-- claude-2026-11-05-handoff -->
**2026-11-05 (Claude):** 0.5.781-0.5.795. Map changes with tanks: 783 vehicle files once a map, 784 after-map archives reused (781 reload reverted), 787 DemonWare guard (sub_821C08B0), 788 Kangaroo before the map, 790 vehicle packs with the first map + Stuka cut. 791 Saint-Lo gentler cut (tanks visible), tankblast command. 792-795 tank events from Merville's table (sub_82555160; set name '<archive>.fx', EVENT.FX; unverified). Logs: game heap per extra archive, DemonWare allocation failures, physics presets/kinds per map, COD3_PEERTRAP.
<!-- /claude-2026-11-05-handoff -->

<!-- claude-2026-11-03-handoff -->
**UPDATE 2026-11-03 ~20:05 (Claude; source+install 0.5.758; nothing committed): flares have WaW's own sounds, burn whoever stands on them, and bots throw them (kits with the Mosin, Webley and flares). Bot smoke throws were broken before and now work. Read the top of docs/CLAUDE-SESSION-2026-11-03.md.**
**UPDATE 2026-11-03 ~19:00 (Claude; source+install 0.5.751; nothing committed): the signal flare is finished: thrown, flies, lights after 1 s where it lands, no smoke (its grenade sunk, WaW stick left), blinding verified. Read the top of docs/CLAUDE-SESSION-2026-11-03.md.**
**UPDATE 2026-11-03 ~17:30 (Claude; source+install 0.5.743; nothing committed): the signal flare now looks like WaW's flare stick (thrown, lying, in others' hands; first person keeps the game's grenade). Read the top of docs/CLAUDE-SESSION-2026-11-03.md.**
**2026-11-03 ~16:40 (Claude):** 0.5.739-0.5.741 World at War's signal flare: weapon signal_flare (smoke's copy, weapon 48), custom class SPECIAL row, flare.cpp (glow, sparks, WaW blinding numbers), tools/waw/make_flare.py -> custom\flare. Test flare verified (flare1); thrown-flare lighting fixed in 741 (name at 0xAA.. address), awaits the user's throw. Bots cannot throw the copy.
**2026-11-03 ~12:05 (Claude):** Seelow drain pipe: solid roof slab over its middle (seelow_clipmap PIPE_ROOF) so standing up inside is refused (the user stood up and their view went through the road). Collision rebuilt (old coll_m.kerbs1), both packs reinstalled. User's play log backed up as logs-backup\20261103-120416-user-play11.
**2026-11-03 ~11:20 (Claude):** Seelow fires centred on their 5 junked cars and set 15 under each car's origin (the vfire flames start ~40 over the point; seelow_install.sh SCENE_FIRES). Both packs reinstalled; test fires3: flames rise out of the car bodies. Data only, no build.
**2026-11-02 night to 11-03 (Claude):** 0.5.738 Mantle assist setting (off for players, bots keep it, bots.cpp BotsIsBotRecord). Seelow tools: seelow_clipmap drop_small_clip (prop-only clips kept), hull triangle sign fix (s.p - sw), 4 .cgb files (FILL bands; 4th = level unzoned.cgb via repack.py --replace), ladders 0x40b8, PIPE_CAPS, WALL_HOLES, KERBS; seelow_cells lamp_occluders/lamp_sees, glow lightmap, night_lights (fires, stoves, createfx), my_lights.txt + lamp_axes; scene.py fires() (vfire_panzerIV at WaW's burning cars).
<!-- /claude-2026-11-03-handoff -->

<!-- claude-2026-11-02-evening-handoff -->
**2026-11-02 evening (Claude):** 0.5.735 'where:' position log; 0.5.736 mantle to each helper's height; 0.5.737 step 28 on maps with custom\mantle (Seelow only). Tools: seelow_models SLOT_FIX (names past slot 82 by placed boxes), SWAP_UV (wheat, hedges, pine), UPRIGHT_CARDS (grass 27 deg); seelow_clipmap: shot-only floors solid, budget 30000; SEELOW_NIGHT 0.56,0.66,0.92 with lamp pools. All installed.
<!-- /claude-2026-11-02-evening-handoff -->

<!-- claude-2026-11-02-afternoon-handoff -->
**2026-11-02 afternoon (Claude):** Seelow/Seelow Night: seelow_clipmap.py no longer swaps WaW's 0x10000/0x20000 (CoD3 player mask is 0x2810011; WaW AI clip had blocked 206 of 292 mantle spots); WaW glass (0x10) out; SEELOW_MESH_BUDGET 30000 (boxed props 312 -> 127); seelow_models SWAP_UV for the wheat; night lightmaps brighter (SEELOW_NIGHT 0.40,0.475,0.69); side session: night fog = Longyard's record (0.5.733), sun = Longyard's. No code build of mine; 0.5.732-734 are the side session's debug switches.
<!-- /claude-2026-11-02-afternoon-handoff -->

<!-- claude-2026-11-02-midday-handoff -->
**2026-11-02 midday (Claude):** 0.5.729 mantling (WaW's 292 mantle helpers, Seelow/Seelow Night only); 0.5.730/731 vehicle pack + host never makes a vehicle from nothing (pack parked: crashes/hangs on vehicle-less maps). Seelow: Rouen leftovers repacked out (tools/kapf/repack.py; 7 -> 23 banks free), trees under the map gone, boxcar/hedgehog textures, fog ramp kept. New map pack Seelow Night (pack 6, ID 22): moonlit cells, Longyard's night sky; open: tan fog, arms lit by day.
<!-- /claude-2026-11-02-midday-handoff -->

<!-- claude-2026-11-02-handoff -->
**2026-11-02 (Claude):** 0.5.728 Mosin one gunshot sound (built, verified). Seelow: e_blend shader set (see-through fixed), WaW sky, WaW player-clip bit, model collision, GfxWorld draw list (grass/wheat/hedgerows, 7582 placements), Panzer wrecks, Rouen .dcgb dropped. Details: docs/CLAUDE-SESSION-2026-11-02.md.
<!-- /claude-2026-11-02-handoff -->

<!-- claude-2026-11-01-night-handoff -->
**Claude handoff 2026-11-01 ~23:05 (source and install 0.5.725, built OK; nothing committed; friend zip on hold; Carentan still set P, "perfect now" per the user): read docs/CLAUDE-SESSION-2026-11-01-night.md FIRST (whole file is the handoff).** Done: World at War's M2 flamethrower nearly finished: fire not bullets (impact records cleared), WaW's own arm animations on CoD3's arms (tools/waw xanim.py / wawpose.py / make_flame_arms.py), the flame starting at the nozzle as the game draws it (the M2 draw's own constants), the gun drawn over its flame (stencil mask), others' flames in the world (fading with distance, hidden behind walls), WaW's overheating instead of a reload (heat bar over the ammo box), damage 26. UAV: an oval fitted to the map and to the draw distance (Isle 3113 x 1722). Next: the user plays 0.5.725, then CoD2's scoped Mosin-Nagant (plan in CLAUDE-SESSION-2026-11-01.md), Webley fingers, Seelow. Open: grey wall puffs with the M2; Isle deathmatch's closed gates (a smaller layout? not checked).
<!-- /claude-2026-11-01-night-handoff -->

<!-- claude-2026-11-01-evening-handoff -->
**Claude handoff 2026-11-01 ~18:40 (source and install 0.5.702, built OK; Carentan installed as set P; nothing committed; friend zip on hold): read docs/CLAUDE-SESSION-2026-11-01-evening.md FIRST (whole file is the handoff).** Done today: Webley + flamethrower kill-feed icons; Carentan: CoD2-exact lightmaps, Rouen collision/props/trees/grass removed (renamed files are still loaded: drop them from the directory), floor gaps fixed (266 CoD2 triangles rebuilt), white-outs fixed (BSP leaves -> one cell), black walls fixed (decals on e_decal, drawn after walls; additive decals out). User: "the walls look perfect now". Open: the player "frozen" spot at -689 1601 (0.5.702 logs "player still"), check the last test's 23 low-triangle seconds, then flamethrower / Seelow / scoped Mosin.
<!-- /claude-2026-11-01-evening-handoff -->

<!-- claude-2026-11-01-handoff -->
**Claude handoff 2026-11-01 ~10:30 (source and install 0.5.689, built; nothing committed): read the top section of docs/CLAUDE-SESSION-2026-11-01.md first ("END OF SESSION HANDOFF").** Carentan (CoD2's real map, its own map, never a Rouen reskin): collision converted and proven in game; the drawn level converts (tools/cod2import/cod2cells.py) and IS DRAWN in game in a scratch harness of the placeholder pack. One open bug: our own textures come out white (their imports never find them); 0.5.689's COD3_TEXLOOKUP=1 logs the texture handler and binds: run tools/carentan/runvariant.sh "" <case> with EXTRA_ENV="COD3_TEXLOOKUP=1" next (warn the user before any launch). Work data: D:\Call of Duty 3 (USA, Europe)\carentan-work\ (game data, not in git). The pack is back to its placeholder.
<!-- /claude-2026-11-01-handoff -->

<!-- codex-2026-10-30-handoff -->
**Codex handoff 2026-10-30: source633; install0.5.632; 633 runtime UNTESTED.**
Read [CODEX-SESSION-2026-10-30.md](CODEX-SESSION-2026-10-30.md).
Root cause: Isle's mounted `mg42` animation entry shadows our handheld alias.
633 scopes the MG34 override to handheld setup. Build status: building: 0.5.633 handheld MG42 animation-table collision fix; no test request.
User requested wrap-up; all games stopped, original INI restored, no test queued.
<!-- /codex-2026-10-30-handoff -->

# CoD3 PC port: shared log between the two teams

This file is how the two teams hand work to each other between builds. Each
team runs its own AI assistant:

- **Team Justin**: Justin (HOVGtv) plus Claude. This is the main line: bots,
  networking, campaign guns, Kangaroo, lobby rules.
- **Team Friend**: Justin's friend plus his AI. Starting with the G43 (Gewehr 43).

Both AIs: read this file first, every session. Add an entry at the top of
the LOG section (below) for every build or handoff. Write it for an AI that
has never seen your conversation. Say what changed, which files and
functions, the game addresses involved, how you found them, how it was
tested, and what is still unproven. Then the other team's AI can merge your
work without guessing.

## Rules both teams keep

1. **Never share game data.** That means no ISO, .xex, .cod, .wbk, generated
   game code (`CoD3RecompLib/ppc*/`, `levels/*/`, `private/`), exes, PDBs,
   textures, shaders or dumps. Only handwritten source, scripts, docs and
   tests go between the teams. Each person regenerates the game code from
   their own disc (see "Building from this package").
2. **Version numbers do not collide.** The build takes its version from the
   first `## X.Y.Z` heading in `docs/PATCH-NOTES.md`.
   - Team Justin uses **0.5.194, 0.5.195, ...** (the main line).
   - Team Friend uses **0.5.900, 0.5.901, ...** for builds made on this
     package, so a log or a screenshot always says whose build it is. When a
     feature is merged into the main line, it gets the next main-line number
     there.
3. **Only play together on the same exe.** Two different builds (for example
   friend 0.5.901 with the G43 and Justin 0.5.195) can disagree about weapon
   tables and desync or crash in System Link. Merge first, then play.
4. **Send work back as files plus a log entry**: the changed source files (or
   a diff against 0.5.193), the new PATCH-NOTES entry, and an entry here.
   Mark anything not tested in a real match as UNTESTED.
5. **Logs are evidence.** When asking the other team about a bug, include the
   `CoD3MP.log` lines (grep for `bots:`, `campaign`, `weapons:`, and so on)
   and say which build produced them.

## What this package is

- Source of **0.5.193** exactly (the build installed October 16, 2026).
  Byte-identical to Justin's 0.5.193 files, rebuilt from his tree on October 17.
- Main docs: `docs/HANDOFF.md` (technical history, newest checkpoint at the
  top), `docs/PATCH-NOTES.md` (plain-words notes per build), `docs/ROADMAP.md`,
  `docs/NEXT-SESSION.md` (older start-here note from 0.5.171).
- Left out on purpose: game data and generated code (rule 1), the
  `tools/XenonRecomp` submodule (clone it, below), build trees, logs and
  `docs/build-logs/`.

## Building from this package

1. Get XenonRecomp at the pinned commit and apply the project's patch (see
   `tools/patches/README.md`; there is no .git here, so clone instead of
   using the submodule):
   `git clone https://github.com/hedge-dev/XenonRecomp.git tools/XenonRecomp`
   `git -C tools/XenonRecomp checkout ddd128bcca99fe8bfbb99bea583c972351fa6ace`
   `git -C tools/XenonRecomp apply ../patches/XenonRecomp.patch`
   `BUILD-MULTIPLAYER.ps1` applies `XenonRecomp-mp.patch` itself.
2. Put your own disc's `codmp_xenonf.xex` and `default.xex` (and the level
   DLLs for the campaign) in `CoD3RecompLib/private/`.
3. Run `BUILD-MULTIPLAYER.bat`. It recompiles the game code (about 5 minutes
   the first time), builds CoD3MP.exe and CoD3.exe, and installs them into the
   port folder. Its log goes to `build-mp-log.txt`.

## Pointers for the G43 (from Team Justin)

The campaign guns in MP live in `CoD3Host/mp_fixes.cpp`:
- `CampaignGun Guns[]` (around line 2224) has one entry per gun: its name,
  which MP gun it copies (`from`), the localized name, view and world models,
  HUD icon, the animation family to rename (`armsFrom` -> `armsTo`), clip,
  reload timings, drop/raise percentages, which archive it comes from, the
  gunshot sound prefix, and an optional scope. The Lee-Enfield, Bren, Sten,
  FG42, FG42 Scoped and Mills bomb entries are worked examples, with the
  reasoning for each number in the comments.
- `CampaignArchives[]` lists the campaign weapon archives loaded in MP (St
  Lambert's `sp\mace2\weapons.cod` and Fuel Plant's
  `sp\fuelplnt\weapons.cod`). The ROADMAP says the G43 is in the **Forest**
  archive, so it probably needs `sp\forest\weapons.cod` added as archive 2.
  Check the memory notes around "Maps whose memory is too full for the
  campaign weapons archive" (just after `CampaignStandIn`). A third archive
  may not fit on every map.
- `CampaignStandIn(name)` gives the copy's gun on maps where the archive is
  not loaded, so classes never get an empty slot.
- Custom class weapon names: `WeaponName` / `CustomWeapons` near line 407.
  Bot loadouts: `CoD3Host/bot_loadouts.h` (the four custom primaries) and
  `bots.cpp` (`BotsPrimaryName`).
- The G43 is semi-automatic, so the closest MP copy is the M1 Garand
  (`m1garand`) or the Kar98 for the reload, and the German "geRFL" animation
  family. Its campaign sound names and reload length should come from Forest's
  sound bank, the same way the Sten's came from `fuelplnt.wbk` (see its comment).

## LOG (newest first)

### 2026-10-30 12:40: Team Justin (Claude), 0.5.633 MG42 validated

- The handheld MG42 vanished on Isle because Isle has a mounted-gun `mg42` animation entry (13 items, none
  resolved) that shadowed our MG34 alias (Codex found it; mp_fixes.cpp 0.5.633 scopes the override to the handheld
  MG42's view and arms setup). Validated on Isle War: the gun and arms are drawn and fire, and the snapshot check
  passes. Rouen control and the bots' third-person MG42 are still to check.

### 2026-10-30 00:10: Team Justin (Claude, spectator side session), 0.5.622 to 0.5.631

- Spectator: SPECTATOR is a fourth row of the team choice, drawn by the game (menu_mouse.cpp SeatEntry/SeatWidget);
  the seat's HUD is off in the free camera and on while following someone (spectator.cpp).
- Killcam (bots.cpp Killcam): its own clock (performance counter), the killer's eye blended and held in both cgame
  snapshots, the HUD on. Explosions are still missing: not carried by the replayed objects.
- Bots' grenades: the frag and smoke keys are 0x2 only (NadeButtonSearch); a wandering key threw extra grenades.
- MG42 in first person is not drawn at all (no hands). Open; see docs/CLAUDE-SESSION-2026-10-30.md.
- Co-op v40 (Team Friend's 0.5.920) is merged as 0.5.622, with Frank's campaign sprint; zips sent at 0.5.622.

### MULTIPLAYER 0.5.426-0.5.478 (Team Justin, Claude, 2026-10-25): no co-op changes
- coop.cpp untouched since v38 (0.5.425). Campaign builds get two shared host pieces ported from
  FrankHUN88453's CoD3Recomp: 0.5.477 cpu_depth.cpp (lamp coronas fade behind walls: the depth resolve the
  title reads on the CPU is written back through render_d3d11.cpp's WriteDepthBack) and 0.5.478
  pc_prompts.cpp + render_prompts.cpp (button prompts drawn as key caps while keyboard/mouse was used last;
  input.cpp Input::PadPrompts/MenuMode; setting button_prompts 0 auto, 1 pad, 2 keys).
- Multiplayer only: public internet games (master list, 0.5.426-0.5.437), Hellfire Barrage killstreak,
  lobby to 32, bots (War/CTF/SCTF/HQ, routing), killcam pool fixes (1 KB particle pool x8 in 0.5.476).
- New tools: tools/mpmatch/mpmatch.py and addrvote.py find a SP function's / data address's MP twin.
- Details: docs/CLAUDE-SESSION-2026-10-25.md; per version docs/PATCH-NOTES.md.

### CO-OP TEST 9 (Team Justin, 2026-10-24): v38 on 0.5.425, island, host logs in coop-test-9-for-friend.zip
- Both PCs on 0.5.425 (= your v38 coop.cpp + our read-only 0.5.316 mortar probe). Justin hosted over Tailscale.
  Reported: the players out of sync; a mission restart made the joiner rewatch the film and start from the beginning.
- Restart cause (host log + coop.cpp): the host restarted in place at 476 s, then relaunched at 522 s with 664
  bytes of launch data (XamLoaderSetLaunchData). CoopBeforeRelaunch sends only CWX1; on the joiner both paths end
  in `spmap <map>` (a fresh start, film included). The checkpoint / launch data never reaches the joiner.
  Suggested: forward g_launchOut to the joiner and relaunch it the host's way (COD3_LAUNCHDATA) instead of spmap;
  for the in-place restart, run what the host ran.
- Desync: the joiner's stand-in on the host snapped 300-700 units off (51 s, 120 s, 170 s); the stuck guard
  fired "5 times in 0.0 s" (its 2 s window reads 0.0 s); replaced 4 times. Network fine (delay 104-130 ms,
  jitter 3-20 ms). Details: COOP-TEST-9-README.txt in the zip. Joiner's logs wanted back.

### CO-OP v38 SOURCE (Team Friend, 2026-10-24): answers co-op test 8 (drops, grenades, shots, facing, bodies)
- File: `CoD3Host/coop.cpp` only (campaign, `#ifndef COD3_MP`), on top of v36 (the co-op in 0.5.320..0.5.416).
  Diff: `coop-v38-vs-v36.diff`. **New messages (CWG3, CWA1, CWI1): both PCs need the new build.**
- Dropped weapons (type 2) and grenades (type 3) are no longer sent/paired/moved (test 8's spinning MP40s);
  a dropped weapon picked up or cleared in one copy is deleted in the other (CWI1).
- Grenades: CWG3 the frame one appears, with its own trajectory (entity +32 start, +44 speed); the other copy
  throws one and writes the same +32/+44 (same arc). One-PC test: the joiner's came down 0-15 units from his
  on the host. CWG2 (landing) moves it there if 80+ off once still; an early id is never thrown twice.
- Soldiers' shots: hooked `sub_823ED9B8` (actor shoot; called only by the shoot builtins `sub_824B8FF8` /
  `sub_824B9088`). Host sends each soldier shot (CWA1, his clock); joiner's paired soldiers fire only then, at
  the display delay. The joiner's stand-in on the host fires only when the joining player does.
- Facing: orientmode "face angle" written directly (actor +372 = 1, +376/+388 const at 0x82062C54, +380/+384
  yaw): players' bodies face where the player looks; still soldiers face as the host's do.
- Stuck guard (5 snaps in 2 s), unpaired host soldiers get one of the joiner's own unpaired soldiers (no
  longer deleted: kept hidden below the level), then spawners of his kind / his side; MakeBody rotates
  spawners; the joiner drains the film port every frame; "no body for the host" diagnostic.
- Tested: two copies on one PC, Forest (soldiers ~3 units apart, 0 snaps, 0 unpaired, 400+ soldier shots
  mirrored). UNTESTED between two PCs. Full notes: `COOP-V38-NOTES.txt`.

## 0.5.404-0.5.407 small CPU cuts and a heap freeze watch (Claude, 2026-10-23 19:55)

- 0.5.404 NativeCull::Walk prefetches children (mp_fixes.cpp; no gain, kept).
- 0.5.405 write_watch.cpp: g_clean[page] = written+1 (0 = not armed), Clean
  is one read a page.
- 0.5.406 render_d3d11.cpp ConvertDepthRows: SSE4.1 20e4 depth encode.
- 0.5.407 mp_fixes.cpp HeapWait wrappers on sub_82185D18 / sub_82185B40: a
  cache that asks the game heap (sub_820DE188 via sub_820DE7B8) and, refused,
  frees its oldest entry and retries for ever. 0.5.406 froze there with the
  cache list empty. Now logs "heapwait:" after 100 empty reclaims and sleeps
  1 ms a retry. Why the heap is full is unproven.
- Tested: Rimling 32p and Marseille city bench clean on 0.5.407.
- GitHub release NOT done yet: public repo hovg-N8/CoD3Recomp is still at 0.5.183. Claude's session had no GitHub account linked (add_repo refused). Release text ready: docs/RELEASE-0.5.407.md (source only, no binaries, credit FrankHUN88453/CoD3Recomp). Upload handwritten source/docs/scripts only.

## 0.5.396-0.5.403 main thread and the effects crash (Claude, 2026-10-23 14:50)

- Fiber pool (coroutines.cpp): RULE: any fiber switch must carry the SSE
  control word (each fiber has its own MXCSR; PPCContext caches csr).
- Native: title memcpy sub_8231E8C0 (mp_fixes.cpp); indirect calls inline
  (cod3_mmio.h, Mmio::g_callChecks); FxGuard on sub_820FC850.
- The "effects crash" (Codex 0.5.384 index-off) is chain sub_824472C0 ->
  sub_820FC850 -> call through emitter +200 holding a non-object; seen
  again in 0.5.397/398 (crashed) and 0.5.401 (caught). Guarded, cause open.

## 0.5.391-0.5.395 drawing thread (Claude, 2026-10-23 12:50)

- New file CoD3Host/render_queue.h; the queue, the drawing thread
  (DrawerMain) and RenderQueue::Sync/RingWrite/Upload are in
  render_d3d11.cpp; render_resources.cpp routes ring maps and buffer
  UpdateSubresource through it and calls RenderQueue::Sync(site) before
  anything that releases a D3D object or uses the context directly.
  RULE for new renderer code: inside Render::Draw (the command thread,
  RenderQueue::Deferring() true) never call the context directly and never
  Reset/release a D3D object without RenderQueue::Sync() first.
- On only in heavy frames (>=4000 draws). Off switch COD3_DRAWTHREAD=0.
- Measured (A/B in one run): command thread -13..15% at ~5100 draws;
  fps 51.6 vs 49.4 and 50.3 vs 50.4 (main thread ~19 ms co-limits).
  No crash or visual fault in 5 runs.

## FINAL CODEX HANDBACK (2026-10-23, 0.5.384)

See docs/CODEX-HANDOFF-0.5.384.md FIRST. Source/install384, next385; no385 edits.
Final legacy-control test completed00:36:40, no particle failures/Guest fault;
index-off run before it DID crash. Root cause unresolved, cannot exclude regression.
All nine logs, three modified source snapshots, backups/diffs/parsers/reproduction
launchers preserved outside Git in D:\Call of Duty 3 (USA, Europe)\CoD3-local-handoff-2026-10-23-Codex-384.
No more tests/builds scheduled. Constant-shadow optimization and particle diagnostic
NOT implemented. User authorized agent-launched tests; stopped for low account usage.

### 0.5.384 (2026-10-23): whole-thread CPU measurement

render_stats.cpp EndFrame: GetThreadTimes once per reporting second, kernel+
user deltas / frame count and draw count. Print cp-thread lines only when
render profiling; off COD3_THREADCPUPROFILE=0. Excludes sleeping/descheduling;
first interval, changed thread and failed query prime baseline. Scope times
remain elapsed wall clocks; do NOT subtract them from OS thread CPU time.
No rendering behavior changed. Built OK, installed 00:24:04; test pending. Need true CP baseline.

### 0.5.383 (2026-10-23): index-key lookup front cache

render_resources.cpp IndexBufferFor: 2048 direct-mapped cached handles;
validate live and all four key words (physical/count/width/reset). Deque never
shrinks/clears; handle reuse remains safe via full-key/live check. Misses go
to existing g_indexKeys.Find. ALL page-watch/content checks unchanged.
Off: COD3_INDEXLOOKUPCACHE=0. Built OK, installed 00:15:55; test pending. Source backup local.
Previous profile: IndexBufferFor ~3.45% CP self + KeyTable<4>::Find ~0.8%;
this experiment targets lookup work, not index data hashes (small).

### 0.5.382 (2026-10-23): remaining packet history

gpu.cpp ExecuteBuffer skips diagnostic Remember for type-3 DRAW_INDX,
DRAW_INDX2, SET_CONSTANT, SET_CONSTANT2, SET_SHADER_CONSTANTS, NOP. All
handlers execute as before. Waits/fences/interrupts/indirect and unknown/other
packets retained. COD3_REMEMBERDRAWS=1 restores 381 type-3 history;
COD3_REMEMBERALLPACKETS=1 restores full pre380 history. Built OK, installed 00:04:11 (2.6 min); test pending.

### 0.5.381 preparation: audit redirects the next change

Justin ran PROFILE-MARSEILLE-AUDIT.bat on 0.5.380. Result: index only
~1.5-6 MB per five seconds in late windows; vertex ~330-621 MB. Repeated
model buffers of 3120-4080 bytes are below the current 4 KB threshold.
381 lowers VertexBufferFor watchStillBytes default to 1 KB; existing
COD3_WATCHSTILL_KB=4 restores 380. Other stable/midFrame/falseWrites checks
unchanged. No index-buffer code change, since it already watches pages.
Built OK, installed 23:55:45 (2.7 min); test pending; compare NORMAL test with 380 normal, not with audit.
Audit busy views >4000 draws: 46.3 fps, 3.433 us cpu/draw (64 samples).
Audit logging has extra overhead; do not call this a regression of 380.

### 0.5.380 (Codex picking up Team Justin, 2026-10-22): packet history cost

- gpu.cpp ExecuteBuffer: type-0/type-2 recent-packet history disabled by
  default; all type-3 history preserved. COD3_REMEMBERALLPACKETS=1 restores
  old behavior. No guest command semantics changed. Built OK and installed 23:38:54 (3 min); Justin tested; result below.
- Baseline runtime log is 0.5.378; installed build-status says 0.5.379 OK.
- No Git metadata in build root. Source backed up locally; no upload.

### 0.5.305 - 0.5.341 (Team Justin, 2026-10-20/21): see NEXT-SESSION.md

- Not written here build by build: killcam (as the killer: poses, shots,
  events), PR #2 merge (0.5.340: HDR via RenoDX/Prism, F11 Optimizations;
  "Depth copy on its own thread" crashed MP, OFF by default in 341), bot
  rifle grenades / demolitionist / Rusher 2, artillery cut on kills, sound
  bank 0x4000000. One line per build at the top of docs/NEXT-SESSION.md,
  player-facing in PATCH-NOTES.md. 0.5.341 built, untested.

### CO-OP v36 SOURCE (Team Friend, 2026-10-20): answers co-op test 7 (build mismatch, film after a host death)
- File: `CoD3Host/coop.cpp` only (campaign, `#ifndef COD3_MP`). Same messages as v35. Diff: `coop-v36-vs-v35.diff`.
- Test 7 was not a v35 test. The joiner ran 0.5.276 (CWS1/CWG1) against Justin's 0.5.314 (CWS2/CWG2). The joiner
  dropped all world parts and grenades without a word; the host dropped the joiner's grenades too. **Both PCs must
  run the same CoD3.exe.** The host now says so on screen when the joiner's CWP1 is not 44 bytes or a CWG1 arrives.
- Host death: the host came back at a checkpoint (no film), and the joiner restarted the mission with its film,
  waiting for a CWF1 that never came. The host now sends CWF1 (his count) every second while in a mission and
  alive. The joiner's own A also ends a film after 15 s.
- New: `COD3_COOP_SHOTLOG=1` diagnostic. Shot detection was checked with it (Forest taps, Chambois taps/aim/holds:
  every shot counted), not changed.
- Tested: two copies on one PC, Forest. UNTESTED: a host death between two PCs. Still open: the host's relaunch for
  a retry or the next mission (his coop-relaunch.log stops right after each relaunch).
- Full notes: `COOP-V36-NOTES.txt`.

### CO-OP v35 SOURCE (Team Friend, 2026-10-20): co-op netcode rework, answers co-op test 6

- CoD3Host/coop.cpp only (campaign program; nothing in CoD3MP), made on 0.5.313. Full notes:
  COOP-V35-NOTES.txt in the package. Built here from 0.5.313 + this file; one-PC test (two
  copies, Forest) in COOP-V35-ONE-PC-TEST.txt. UNTESTED between two PCs. Both PCs need the
  same CoD3.exe (new CWS2/CWG2 messages, 44-byte CWP1).
- World sync: 1.1 KB CWS2 parts with the host's clock instead of one 12-32 KB message (10+
  Tailscale fragments, any loss lost it all); only soldiers/players/movers sent; joiner
  interpolates on the sender's clock (0.1 s + jitter) and predicts 0.2 s ahead when late;
  entries not heard for 1 s are dropped (the joiner kept 348 stale ones).
- Soldiers on the joiner walk on their own legs (setgoalpos, led 0.35 s), pushed only past a
  32-unit dead zone, snapped past 320; their angles are never written (the sideways running and
  the cartwheels: a dead host soldier's body was sent as a mover and its lying angles given to a
  living joiner soldier, who was never killed). Dead host soldiers stay "soldier, health 0".
- Pairing within 800 units, living with living; unpaired host soldiers get a dospawn from a
  matching spawner; the joiner's unpaired soldiers hidden at 0.75 s, deleted at 4 s.
- CWG2 grenades: every host grenade (soldiers' too) with its thrower; joiner soldiers keep no
  grenades (actor +2576 = 0); magicgrenade retried from 4 release points (test 6 NOT thrown =
  no arc found); grenades thrown for the other copy never sent back; the stand-in is shielded
  from his own grenade (test 6: 586/449/821).
- Stance changes on bodies held 0.25 s; no borrowing of named allies (Dixon) unless
  COD3_COOP_ANYALLY=1; MakeBody tried for a minute.
- Sprint (campaign): multiplayer-like: tap, 0.25 s speed ramp, 4 s stamina, 1.5 s winded, MP's
  quick-recovery numbers. Native MP sprint (sub_8248E970 / sub_82495980) has no counterpart in
  default.xex, so still g_speed.
- Still open: test 6 item 5 (after the host's relaunch nothing reconnected); please send the
  joiner's coop-join-play.log and coop-relaunch.log from the next test.

### 0.5.304 SOURCE (Team Justin, 2026-10-20): mortar on interact arms

- mp_fixes.cpp only.

### 0.5.303 SOURCE (Team Justin, 2026-10-20): mortar crash fix 2, mortar_yaw

- mp_fixes.cpp, kernel_video.cpp.

### 0.5.302 SOURCE (Team Justin, 2026-10-20): mortar arms on the round, interaction end

- mp_fixes.cpp only.

### 0.5.301 SOURCE (Team Justin, 2026-10-20): mortar interaction from mortar_idle

- mp_fixes.cpp only.

### 0.5.300 SOURCE (Team Justin, 2026-10-20): campaign mortar interaction test

- mp_fixes.cpp, input.cpp, input.h.

### 0.5.299 SOURCE (Team Justin, 2026-10-20): campaign mortar model

- mp_fixes.cpp only.

### 0.5.298 SOURCE (Team Justin, 2026-10-20): mortar-only archives

- mp_fixes.cpp: filtered KAPF copies made locally from the player's own
  campaign files (never shipped). docs/KILLCAM-PLAN.md added.

### 0.5.297 SOURCE (Team Justin, 2026-10-20): Laison archive off by default

- mp_fixes.cpp only.

### 0.5.296 SOURCE (Team Justin, 2026-10-20): Laison archive after the map

- mp_fixes.cpp only.

### 0.5.295 SOURCE (Team Justin, 2026-10-20): Laison archive off by default

- mp_fixes.cpp only.

### 0.5.294 SOURCE (Team Justin, 2026-10-20): Laison archives out of map lists

- mp_fixes.cpp only.

### 0.5.293 SOURCE (Team Justin, 2026-10-20): weapons past 63 test switch

- mp_fixes.cpp only.

### 0.5.292 SOURCE (Team Justin, 2026-10-20): 63-weapon limit guard

- mp_fixes.cpp only. NOTE for both teams: never more than 63 weapons on a map
  (player ownership mask is 64 bits at state +1056).

### 0.5.291 SOURCE (Team Justin, 2026-10-20): campaign mortar files (Laison)

- mp_fixes.cpp only (new hook sub_825683D8).

### 0.5.290 SOURCE (Team Justin, 2026-10-20): Fuel Plant plane height

- mp_fixes.cpp only.

### 0.5.289 SOURCE (Team Justin, 2026-10-20): mortar hands fix, Fuel Plant plane

- mp_fixes.cpp only.

### 0.5.288 SOURCE (Team Justin, 2026-10-20): mortar hands

- mp_fixes.cpp: campaign gun mortar_hands (interact copy, mortar arms, 81mm round).

### 0.5.287 SOURCE (Team Justin, 2026-10-20): 624 MB extended window, 608 MB map block

- kernel.h, gpu_address.cpp, write_watch.cpp, native_state.cpp,
  kernel_memory.cpp (comment), mp_fixes.cpp. Both exes share kernel.h.

### 0.5.286 SOURCE (Team Justin, 2026-10-20): mortar archive off by default

- mp_fixes.cpp: laison vehicles.cod (v_81mm) only with COD3_MORTAR_MODEL=1
  (285 overflowed the map block on Eder Dam).

### 0.5.285 SOURCE (Team Justin, 2026-10-20): mortar killstreak

- mp_fixes.cpp: mortar killstreak (4 kills, v_81mm prop, aim by view,
  10 rounds, host-only radius damage MOD_MORTAR_SPLASH), C3MF/C3MP/C3KM,
  PlaneNetVersion 3. input.cpp/.h: SetMortar. bots.cpp: BotsSideOf,
  BotsGroundZ, BotsLineClear. lobby_rules.cpp: dispatch.

### 0.5.267 SOURCE (Team Justin, 2026-10-19): plane_model Stuka test

- mp_fixes.cpp + kernel_video.cpp: plane_model console test.

### 0.5.275 SOURCE (Team Justin, 2026-10-19): UAV plane on every machine

- mp_fixes.cpp: C3UP/C3UH/C3UD side-channel plane sync (engine, flybys,
  effects 272-274 included); bots.cpp: 30-bot roster, 32-entry mirror
  tables; lobby_rules.cpp: dispatch + 2048 receive buffer.

### 0.5.266 SOURCE (Team Justin, 2026-10-19): Typhoon for both UAV sides; sound fallback

- mp_fixes.cpp: v_Typhoon both sides; plane sounds fall back to the frame.

### 0.5.265 SOURCE (Team Justin, 2026-10-19): UAV plane sound from sound events

- mp_fixes.cpp: plane sounds queued, played inside the weapon sound event
  hook; plane_sound shot test.

### 0.5.264 SOURCE (Team Justin, 2026-10-19): UAV whole-map circle; plane_sound

- mp_fixes.cpp: spawn-point circle, mean height, plane_sound test;
  kernel_video.cpp: plane_sound console command.

### 0.5.263 SOURCE (Team Justin, 2026-10-19): UAV fixes (262 regressions)

- mp_fixes.cpp: no lowering rule, no r5=1 loop; flybys as the flag script.

### 0.5.262 SOURCE (Team Justin, 2026-10-19): UAV engine loop; rocket delay

- mp_fixes.cpp: engine loop via playloopsound on a script_origin; rocket
  hits delayed by flight time.

### 0.5.261 SOURCE (Team Justin, 2026-10-19): UAV flybys on the host's player

- mp_fixes.cpp: PlaneSound like the Lee-Enfield shot (r8 0, own entity).

### 0.5.260 SOURCE (Team Justin, 2026-10-19): plane lowers when out of world

- mp_fixes.cpp: plane height lowered while its +16 bit 0x40000000 is set.

### 0.5.259 SOURCE (Team Justin, 2026-10-19): UAV sound origin; Poissons

- mp_fixes.cpp: flybys on a script_origin near the host; no plane files on
  Poissons.

### 0.5.258 SOURCE (Team Justin, 2026-10-19): UAV shots by ray, hit marker

- mp_fixes.cpp: MpFixesPlaneShots (clip drop + view ray), hit marker in the
  streak HUD, flyby sounds via sub_82556868.

### 0.5.257 SOURCE (Team Justin, 2026-10-19): UAV plane shootable, sound

- mp_fixes.cpp: MpFixesPlaneHit (from the G_Damage hook), bigger circle,
  engine loop (playloopsound 824CF5F8).

### 0.5.255 SOURCE (Team Justin, 2026-10-19): plane archives after the guns

- mp_fixes.cpp: Allies' UAV plane = Typhoon (sp\forest\vehicles.cod);
  MpFixesLoadPlaneArchives after the campaign weapons archives.

### 0.5.254 SOURCE (Team Justin, 2026-10-19): UAV plane moves

- mp_fixes.cpp: moveto is 824C8288; players' origin from ps; side 2 = Allies.

### 0.5.253 SOURCE (Team Justin, 2026-10-19): UAV plane (visible)

- mp_fixes.cpp: killstreaks for 32; UAV plane (MpFixesUavPlane,
  MpFixesPlaneFrame: script_model + script movers), P-47/Stuka campaign
  archives loaded with every map. settings.h/.cpp: uav_plane (ini).

### 0.5.252 SOURCE (Team Justin, 2026-10-18): death capture off; flat draw list

- bots.cpp HostDeathCapture: screenshots/frame log only with
  COD3_DEATH_CAPTURE=1 (froze the game on death). render_commands.cpp
  DumpDraws negative count = flat draws only; mp_fixes asks -300.

### 0.5.251 SOURCE (Team Justin, 2026-10-18): overhead icons for players 25-32

- scripts/patch_recomp.py: cap32 kinds SIDE12W/SIDE12R (new) +
  MpHudSide declaration; docs/player-limit/cap32-sites.csv: 4 rows
  (824F9410 icon writer, 82507D28 icon drawer: N4 loop ends, entries past
  slot 24 to a side block). mp_fixes.cpp: MpHudSide. No change at 24.

### 0.5.250 SOURCE (Team Justin, 2026-10-18): friendly markers / compass walk 32

- docs/player-limit/cap32-sites.csv: N at 8250689C (82506540, friendly
  marker writer) and 82506384 (82505CE8, compass list). Their tables and
  the drawer (824E2CB0, 32 entries) already hold 32. Found by joining the
  cap probe hits with the unpatched sites. UNTESTED in a match.

### 0.5.249 SOURCE (Team Justin, 2026-10-18): brightness setting; draw lists

- settings.h/.cpp brightness (ini "brightness", 50..150), overlay.cpp F11
  Graphics slider, render_d3d11.cpp GammaTable bends the display LUT.
- render_commands.cpp/.h DumpDraws (PrepareDraw wraps PrepareDrawInner);
  mp_fixes.cpp asks for one list per binocular zoom level. 248's 32-player
  scoreboard confirmed in a match (15 + 14 rows).

### 0.5.248 SOURCE (Team Justin, 2026-10-18): scoreboard walks all players

- scripts/patch_recomp.py: cap32 kinds ROWFULL0 / ROWFULLNE / ROWFULLNZ
  (force a compare when the row count r17 has reached 24 and MpCapClients
  > 24). docs/player-limit/cap32-sites.csv: 5 rows for 823A0180 and
  8239EC38 (player loops 24 -> MpCapClients, rows capped at 24). No change
  at 24 players. UNTESTED in a match at 32.
- render_commands.cpp/.h: SetBinocularsUp (depth of field pass drawn with
  binoculars up; skip reasons logged). mp_fixes.cpp calls it beside
  g_binocularsOut = out. UNTESTED.
- bots.cpp BigTest: scoreboard toggle only with COD3_BIG_SCOREBOARD=1.

### 0.5.247 SOURCE (Team Justin, 2026-10-18): max_players setting (24/32)

- settings.h/.cpp: maxPlayers (ini max_players, 24 or 32); overlay.cpp F11
  Multiplayer checkbox. mp_fixes.cpp MpCapClients: COD3_PLAYERS env first,
  else max_players read straight from CoD3Recomp.ini at static init (before
  Settings::Load). Default 24: no change for normal play. Friend update
  scripts (game folder) now carry PLAY-COOP-HOST/JOIN.bat, COOP-README.txt,
  LOCAL-COOP-TEST.bat and the newest coop-test-*-for-friend.zip.

### 0.5.246 SOURCE (Team Justin, 2026-10-18): killfeed colours at the HUD hand-off

- bots.cpp: 0.5.245's sub_8244DB20 wrapper removed. New sub_82529D78 wrapper
  (only caller sub_82456920 at 0x82456CC4: r5 attacker colour, r7 victim
  colour, rgb floats): during the line, victim (1, .25, .25), attacker
  green (.25, 1, .25) on the host's side else red. No co-op files.

### 0.5.245 SOURCE (Team Justin, 2026-10-18): killfeed victim always red

- bots.cpp: new sub_8244DB20 wrapper (killfeed name colour: team r3, out
  r4). sub_82456920's wrapper sets g_feedVictimColour = the side opposite
  the host's; the first colour call of the line (the victim, 0x82456B68)
  takes it, the attacker's (0x82456C3C) is untouched. No co-op files.

### 0.5.244 SOURCE (Team Justin, 2026-10-18): bot smoke rounds; 1K particle pool only at 24

- mp_fixes.cpp kit extras: after the 2664/2864 script calls, a bot's smoke
  (and frag) rounds at record +4*(172 + def+1468) are written directly when
  still 0 (0.5.243 "smoke count look": smokegrenade_axis owned, clip 0).
  sub_8210ACA0: at 24 only the 1024-byte pool is doubled (all 9 at 32).
  0.5.243 rimling crashed during load in 821C08B0 <- 821C1758 <- 821C1AA0
  <- 823FB698 <- 823FBA08 (script coroutine; session message reading);
  cause unknown. No co-op files touched.

### 0.5.243 SOURCE (Team Justin, 2026-10-18): effects pools x2 always; killfeed side; smoke count look

- 0.5.242 tested: BIG-TEST-32 finished (28 bots, no apsMemory, no crash);
  BIG-TEST rimling + mp_farm finished, but 24 rimling logged apsMemory 1K
  pool empty x46. mp_fixes.cpp sub_8210ACA0 now doubles for 24 too.
- bots.cpp: sub_82456920 (killfeed line) logs victim/attacker/host sides
  (first 40) and, for a bot victim whose team +4 is not 1/2, uses his
  session player's +604 for the line. BotExtras: one-time "smoke count
  look" dump (bots always "have no smoke left"). No co-op files touched.

### 0.5.242 SOURCE (Team Justin, 2026-10-18): 32 test round 3 (particle memory pools, damage table)

- 0.5.241 tested: 24-player BIG-TEST rimling + mp_farm clean (full 150 s,
  20 bots). BIG-TEST-32 (28 bots) crashed at ~2 min in the particle system
  (apsMemory pool size 1024 count 512 empty x22, then a half-built effect).
- mp_fixes.cpp: sub_8210ACA0 wrapper (particle memory pools added at start
  by sub_82430A58 / sub_8242EEE8) doubles each pool's count when
  MpCapClients > 24. MpDamageSink() + new cap32 kind SINK337 (scripts/
  patch_recomp.py, docs/player-limit/cap32-sites.csv row 82580D84): damage
  by attackers past slot 24 no longer lands on victim record +1344.
- 24-player play unchanged. No co-op files touched.

### 0.5.241 SOURCE (Team Justin, 2026-10-18): 32-player test round 2

- First 32 test (0.5.240): 28 bots + host played a full match. Now: the
  per-player object pool (24 at 0x82607890) grows to 36 when 32 are in play
  (mp_fixes.cpp MpPool504 + cap32 P504 sites), and four roster name/team
  getters that still read the old inline list are redirected (affects 24
  too). No co-op files touched.

### 0.5.240 SOURCE (Team Justin, 2026-10-18): player count 24/32 switch (test only), roster fixes

- Default play unchanged (24). COD3_PLAYERS=32 turns on 32 for testing:
  mp_fixes.cpp MpCapClients, read by 189 reviewed generated-code sites
  (scripts/patch_recomp.py patch_cap32, docs/player-limit/cap32-sites.csv)
  and by PlayerCapacity::Active(). Session messages range player numbers by
  it, so both PCs must use the same value.
- Fixed for 24 as well: four game functions that still read the session's
  inline player list after 0.5.213 moved it (823DD0E8, 823DC678, 823DC918,
  823EC678). kernel.cpp/kernel.h gain Kernel::FaultHook (used by the
  roster trap, COD3_ROSTER_TRAP=1). Team Friend: kernel.cpp changed by 4
  lines only; no co-op files touched.

### 0.5.239 SOURCE (Team Justin, 2026-10-18): player-cap probe, BIG-TEST

- No gameplay change. scripts/patch_recomp.py `patch_cap_probe` adds a
  counter call before 910 retail instructions that use 23/24/25 (and the
  EntityList compares with 92/96/100/128), listed in
  docs/player-limit/cap-probe-sites.csv; mp_fixes.cpp records hits and
  compared values to cap-probe.csv. bots.cpp BigTest (COD3_BIG_TEST) is an
  automatic devmap + 20 bot run. Purpose: find which 24s are the player
  limit before raising it to 32. Team Friend: nothing to merge for co-op;
  if you take patch_recomp.py, take the CSV with it.

### 0.5.238 SOURCE (Team Justin, 2026-10-18): DLC loading pictures, photo variant

- bots.cpp PlanViews COD3_PHOTO_VARIANT; loading pictures in textures\replace\maps\<map> (game folder only).

### 0.5.237 CO-OP TEST 6 (Team Justin, 2026-10-18 18:30): first two-PC run on 0.5.237, findings for Team Friend

Full findings + host logs: docs/build-logs/COOP-TEST-6-FINDINGS.txt and
coop-test-6-for-friend.zip. Justin was host. Works: film share, MakeBody,
stance/move/shot/grenade sync, CWX1. Open (coop.cpp, Team Friend): stand-in
takes huge damage when the joiner's smoke arrives; "NOT thrown" grenades;
stand-in lost at 382 s, borrowed soldier 6 then dixon after a restart;
after the relaunch the host only reached the menu (no reconnect); stance
pairs. Team Friend: please send coop-join-play.log + your coop-relaunch.log.
No code change, no new version.

### 0.5.237 SOURCE (Team Justin, 2026-10-18): Team Friend's co-op v33 + v34 merged

- coop.cpp v34 unchanged. Your input.cpp/input.h were older than ours
  (0.5.231 added Input::PressButton for the photo tour): your
  input-sprint.diff was applied onto ours instead; please take our
  input.cpp/input.h from the next package. kernel_xam.cpp: your 5 lines.
  Main line 0.5.237; both PCs need it. UNTESTED between two PCs.

### 0.5.236 SOURCE (Team Justin, 2026-10-18): photo tour eye-height views and learning runs

- bots.cpp PhotoTour only (PlanViews: 12 views; COD3_PHOTO_LEARN learning
  run). Nothing that touches play or co-op.

### 0.5.235 SOURCE (Team Justin, 2026-10-18): Team Friend's coop.cpp v32 + native_options.cpp merged

- coop.cpp v32 and native_options.cpp (campaign options kept,
  saves\options-sp.bin) taken unchanged from coop-for-friend.zip;
  title_fixes.cpp and the bats were already identical. Main line now 0.5.235
  (includes 0.5.234). Both PCs need 0.5.235 for co-op. UNTESTED between two
  PCs.

### 0.5.234 SOURCE (Team Justin, 2026-10-18): death view blur; per-map texture replacements

- render_commands.cpp/h: RenderCommands::SetPlayerDown(bool); the depth of
  field pass 56ccd878 is drawn while the host's player is down even with
  aim_depth_of_field off (the death "double vision" cc16ad46 is laid over
  it; skipped, the two copies were sharp). bots.cpp HostDeathCapture calls
  it. UNTESTED in a match.
- texture_pack.cpp/h, render_resources.cpp: textures\replace\maps\<map>\
  <key>.png used only on that map (Kernel::CurrentMap); 1024x512 textures
  with no replacement dumped even with texture_dump off (12 a run).
  render_stats.cpp: COD3_GPU_DRAWS opt in again. No game data in any of it.

### 0.5.233 SOURCE (Team Justin, 2026-10-18): free photo mode

- bots.cpp PhotoTour: COD3_PHOTO_FREE (fly with noclip). Co-op test 4 findings sent (coop-test-4-for-friend.zip).

### 0.5.232 SOURCE (Team Justin, 2026-10-18): photo tour views from above

- bots.cpp PhotoTour: 8 elevated views a map; process ended after the last shot.

### 0.5.231 SOURCE (Team Justin, 2026-10-18): photo tour fixes

- input.cpp Input::PressButton; tour presses A through team/class menus; one map per run.

### 0.5.230 SOURCE (Team Justin, 2026-10-18): Team Friend's coop.cpp v31 merged

- coop.cpp v31 + title_fixes.cpp (shared film skip) taken unchanged from
  coop-for-friend (1).zip. Bats unchanged from v30.

### 0.5.229 SOURCE (Team Justin, 2026-10-18): photo tour + F10 photo mode

- bots.cpp PhotoTour (COD3_PHOTO_TOUR): devmap each DLC map, HUD/gun off,
  4 screenshots a map for loading pictures. overlay.cpp: F10 photo mode.

### 0.5.228 SOURCE (Team Justin, 2026-10-18): bot revive confirmed; GPU draw timing; death frame log

- Medic bot revive works (Plyr_Revive, sub_82201D40, called with both handles).
- render_stats.cpp: COD3_GPU_DRAWS on by default in CoD3MP, 1 frame in 8.
- Co-op test 3 findings sent (coop-test-3-for-friend.zip): the title's
  relaunch (default.xex) at mission end/fail ends the co-op session.

### 0.5.227 SOURCE (Team Justin, 2026-10-18): Team Friend's coop.cpp v30 + film skip merged

- coop.cpp v30 and title_fixes.cpp (COD3_SKIPFILMS) taken unchanged from
  coop-for-friend.zip; PLAY-COOP-HOST/JOIN.bat updated in the game folder.
- Also contains 0.5.226 (bots.cpp revive via Plyr_Revive, render profile).

### 0.5.226 SOURCE (Team Justin, 2026-10-18): revive through Plyr_Revive; render profile on

- bots.cpp: medic bots call sub_82201D40 (Plyr_Revive) with both script
  handles (message 93 from the host to itself is never handled); a report
  per host death from every medic; HostDeathCapture (screenshots + log).
- render_stats.cpp: stats/profile/gpu timing default on in CoD3MP (=0 off).
- Ordnance perf: bots ~2 ms, server ~7.5 ms, client/render 15-30 ms a frame.

### 0.5.225 SOURCE (Team Justin, 2026-10-18): direct bot revive; frame time split

- bots.cpp: medic bots call sub_823E86A8 (session revive) directly at the
  body; "perf:" lines split the main thread's frame (bots / server / rest).

### 0.5.224 SOURCE (Team Justin, 2026-10-18): shotgun bots, screen smoke, revive fix

- bots.cpp only: BotHoldsShotgun -> aggressive close range; screen smoke for
  objective/rusher bots pushing at distant enemies; BotRevive aims at the
  fall position (g_deadAt) and searches within 1000.
- coop.cpp v29 from Team Friend merged unchanged (0.5.224 build).
- Co-op test 1 (two PCs, Saint-Lo) findings sent as coop-test-1-for-friend.zip:
  no stand-in outside Forest (IsRifleman matches only 'actor_ally_amer_reg').

### 0.5.223 SOURCE (Team Justin, 2026-10-18): co-op campaign (coop.cpp) in the main line

- CoD3Host/coop.cpp (campaign only, #ifndef COD3_MP) added to COD3_HOST_SOURCES.
  Off unless COD3_COOP=host|join. Forest mission; UDP 28962/28963 over
  Tailscale. Both PCs need the same CoD3.exe. Launchers PLAY-COOP-HOST.bat /
  PLAY-COOP-JOIN.bat; notes docs/COOP-README.txt.

### 0.5.222 SOURCE (Team Justin, 2026-10-18): bot throw sets weapon in hand

- bots.cpp: on a throw's first command the bot's record+164 (weapon in hand)
  is set to the grenade, as the host's record shows during a throw (the
  server ignored the command's weapon word for the bot). Revive trace added.

### 0.5.221 SOURCE (Team Justin, 2026-10-18): campaign menus at the machine's frame rate

- CoD3Host/kernel_video.cpp TitlePlaying (non-MP): unlocked unless a film is
  open (0x82A2A24C). COD3_SPMENUFPS=0 for the old levels-only rule.

### 0.5.220 SOURCE (Team Justin, 2026-10-18): killfeed colours, bot throws, special ammo

- bots.cpp: sub_82456920 wrapper (killfeed): CurrentClient 0x82BCBE9C put to
  the host's 0 while the line is coloured (bot kills happen inside
  BotClientScope). Throws: no stance/sprint/fire for 60 commands after a
  throw starts (they cancelled the offhand). Medics fire after 2 s at a body.
- mp_fixes.cpp: sub_821EE6C0 / sub_821EE7C8 wrappers set the "special" spare
  to 4 (mines, rifle grenades); their defs' maxAmmo/startAmmo 4.

### 0.5.219 SOURCE (Team Justin, 2026-10-18): bot grenades fixed (weapon word); 4 mines / 4 rifle grenades; wall feelers

- bots.cpp: a throw = button 0x2 with command +8 naming the grenade's weapon
  number on its first command(s) (found from the host's own throws). Bots now
  do that. BotFeelers: forward/side sweeps every 150 ms, slide along walls,
  remember spots hit 3 times (g_failedRoutes). Flag holders crouch less.
  Medics keep going to a body unless an enemy is within 500.
- mp_fixes.cpp: mine and *_rg weapon defs: clip + reserve = 4 (maxAmmo +1472,
  startAmmo +1452).
- Untested (NOT BUILT when written).

### 0.5.218 SOURCE (Team Justin, 2026-10-18): player-cap prep, EntityList level slot

- scripts/patch_recomp.py: new patch_entity_list_level (added to the mp
  chain). EntityList (*0x82BCC7FC) = vtable + 24 player entities (+4..+96) +
  level entity (+100); players past 24 would hit +100. The object is now 136
  bytes, the level pointer read at +132 (40 sites) and written at both +100
  and +132 (4 sites) while the cap stays 24. Site list:
  docs/player-limit/entitylist-level-sites.csv. No gameplay change.
- CoD3Host/bots.cpp: PlayerLimitProbe logs the two slots.
- Both PCs must build this together (generated code changes).

### 0.5.217 SOURCE (Team Justin, 2026-10-18): bots call for a medic

- CoD3Host/bots.cpp: CallMedic(ctx, base, entity) calls the script callback
  at *(0x82BCBF64)+3352 (reenable_medic_call) with the entity number, as the
  host's session handler sub_823E3860 does for a player's death-screen
  "call medic". Dead bots call once, 1.5 s after death.
- Campaign menu upscales are game data: made on Justin's PC from his disc,
  not shared.

### 0.5.216 SOURCE (Team Justin, 2026-10-18): medic bots revive; CoD3MP.log fix

- CoD3Host/log.cpp: 0.5.215 (CoD3MP as a windows program) wrote an empty
  log because stdout reopened onto fd 0. Fixed (stdin first, plus a dup onto
  the real stdout/stderr descriptors). If you merged 0.5.215's console
  change, take this too.
- CoD3Host/bots.cpp: BotRevive. The game's revive: medic kit "mp_revive"
  (weapon class 11, def+176) fired within 100 units looking at a dead (record
  +1924 == 4) teammate's player entity -> sub_82575910 finds it ->
  sub_823E86A8 session message 93 -> script Plyr_Revive. Medic bots go to
  the nearest dead teammate and do that; dead bots hold their respawn up to
  20 s while a medic comes. Details: HANDOFF "### 216".
- Untested (NOT BUILT when written).

### 0.5.215 SOURCE (Team Justin, 2026-10-18): bot grenade button search; no console window

- Files: CoD3Host/bots.cpp (BotNadeAmmo, NadeButtonSearch g_fragSearch /
  g_smokeSearch, host learner after the host throw trace, g_throwUntil),
  CoD3Host/main.cpp + CoD3Host/CMakeLists.txt + CoD3Host/kernel.cpp (CoD3MP
  is a windows program now: /ENTRY:mainCRTStartup, WIN32_EXECUTABLE ON;
  `--console` / COD3_CONSOLE=1 opens one), CoD3Host/texture_pack.cpp
  (campaign writes textures\used-campaign.txt), version_number.h 0.5.215.
- Why: bots pressed 0x80/0x2 and never spent a grenade (0.5.213 traces);
  the host's presses were 0x4000/0x8000/0x200. The bots now try candidates
  and keep the one that lowers their grenade ammo (record+4*(172+clip) and
  +4*(80+spare) over weapon defs named like grenades). Also learnt from the
  host when his own count drops.
- Finding for the player limit: EntityList (*0x82BCC7FC) holds players at
  +4..+96 (24) and a level/world entity at +100 (41 direct uses). Players
  past 24 need new entity indices or that entity moved.
- Untested (NOT BUILT when written). Merge: bots.cpp hunks are new
  functions after BotGrenadeButton plus the throw/smoke blocks.

### 0.5.214 PREPARED (Team Justin, client date 2026-10-17)

Found a safe next implementation stage: replace sub_824046D8's six-wide client scan (82404850..824048E0), which would overrun a 32-entry pool if only its compare were changed. Added bounded search helper with preserved r10/r11/r31 outputs and existing rejection/setup/connect continuation. Also fixed explicit session-roster mapping to support reserved slots 24-31 while direct inline fields keep their addresses. player_capacity.h centralizes existing 24 active/32 storage/20 bots values. Active cap is STILL 24.

Exact original CoD3Scan address lookup repairs the audit blocker. All 160 compare, 49 candidate-store, 3 loop and 66 roster-add shapes verified; two loop candidates are actually indirect calls with 24 as an argument. Nearby session code has transitional value 25, so immediate-24-only auditing is incomplete.

Tests: Python scan-patch/old storage-patch suites PASS; actual generated patch applied. C++ tests/build/install/runtime pending at initial entry. Installed build remains 213 until later record. Full resume information: docs/player-limit/RESUME-214.md; live status: docs/build-logs/validation-214.txt. Do not share original game disassembly or generated C.


### 2026-10-18 PLAYER-CAP AUDIT (Team Justin)

- Research checkpoint only; no build or gameplay behavior changed. The 32-player goal remains plausible based on 32-bit session masks and 32-entry backing for server-client, game-player and session-player storage. The 0.5.213 test established clean storage during one solo run, not usable slots 24-31.
- `BotsAsk` remains clamped to 20 requested bots and `MaxClients` is 24. The solo run did not prove added-player behavior.
- The catalog contains 1,107 sites; 67 are marked done. A current-source offset mapper proved unreliable after generated-code patches; preserve original PPC instruction context and validate alignment before using candidate rows for edits. Details and remaining open areas: `docs/player-limit/AUDIT-20261018.md`.
- Next: repair exact site mapping, classify loops/comparisons/sentinel values, and investigate entity, local-player, UI, join/leave and respawn paths. Keep the active cap at 24 until an integration stage covers these paths.


### 0.5.213 SOURCE UPDATE (Team Justin, 2026-10-17): relocate session roster; keep cap 24

- The 0.5.212 match log validates the prior reserves in that run: all 24
  relocated game-player records pass with zero stale pointers and clean spare
  records/guard; server-client slots 24-31 report zero changed bytes. Justin
  reports the Lee-Enfield scoped view now works normally in 0.5.212.
- New `CoD3Host/session_player_storage.h`; `mp_fixes.cpp` lazily allocates a
  guarded 32 x 784-byte roster per session object, copies the original 24
  entries, and redirects only addresses inside the inline roster. The session
  object's adjacent fields remain in place. `COD3_NO_SESSION_RELOCATION=1`
  disables the redirect for comparison.
- `scripts/patch_recomp.py mp` rewrites the 66 catalogued session-base adds in
  53 functions, with all sites checked before generated C is written. The
  existing 784-byte strides follow the redirected base. Session, bot and
  active-player limits, sentinels and UI are still 24; slots 24-31 are not
  usable yet.
- Tests: new address mapping C++ test and generated-code patch tests; existing
  player-storage patch tests pass. Release targets CoD3MP and CoD3 build. Host
  bot/session helpers route through the relocated roster accessor. Corrected
  binaries are installed and packaged. Solo runtime checks remain pending; a
  second PC is only needed for later networked capacity validation. Details:
  docs/build-logs/validation-213.txt.

### 0.5.213 SOLO RUNTIME (Team Justin, 2026-10-17)

- User's log confirms version 0.5.213, 32 x 784-byte session roster, and 30
  repeated clean checks of 24 records with spare entries and guard clean. The
  prior player-storage diagnostic also remained clean during the match.
- The first map reached 20 bots; after mp_merv -> mp_fuel the roster reset to
  zero then three bots. A later bot_add 20 was refused because the local client
  reported session number 24. Existing notes define >=24 as no local player place,
  so it may have been entered after active play. Check bot counts through a hosted
  map change before treating this as a defect; do not raise the cap.
- About 303 seconds ran with no fatal crash signature. Session relocation passes
  this solo run's integrity check; respawn was not confirmed. See
  `docs/build-logs/validation-213.txt`.
### 0.5.212 SOURCE UPDATE (Team Justin, 2026-10-17): remove ADS-triggered full frame dump

- `CoD3Host/mp_fixes.cpp`, `MpFixesMg42View`: retain lightweight Lee-Enfield
  ADS transition/settings trace, remove `Render::LogNextFrame()` on the first
  ADS entries. The 0.5.210 live trace showed the synchronous full-frame dump
  firing at each test aim and potentially creating the hitch under test.
- Diagnostic isolation only; no proven visual scope fix. Source change and
  runtime validation status are in `docs/build-logs/validation-212.txt`.
- Current 0.5.210 user observation: scope zoom partly appears, but its outside
  mask is not fully black and ADS feels like half the scope animation is
  missing. The weapon inherits its scope overlay/reticle/zoom fields from
  `kar98_sniper`; compare that stock scope in-game before altering the data.
- Compared against the friend's 0.5.900 source package: the Lee-Enfield
  weapon row and 77 custom fields are unchanged in the current merged source.
  Preserve these known-good weapon values while isolating later ADS/render
  changes; the first recorded black-scope report is in the 0.5.208 handoff.
- Source and local game folder now have 0.5.212. Justin reports the scoped
  Lee-Enfield now works normally. The match log also shows the relocated
  player records and spare server-client slots clean for that run; see the
  0.5.212 live result above.

### 0.5.211 SOURCE UPDATE (Team Justin, 2026-10-17): cache fitted mesh bounds

- `CoD3Host/mp_fixes.cpp`: `WorldFitFrame` is called from the frame hook and
  previously rebuilt each kept campaign mesh's original vertex bounds on
  every frame. `WorldMesh::originalBox` now calculates and stores those bounds
  during `Keep`; `VertexBox` reuses the cache. Reload checks and transforms
  remain in place.
- Intended to reduce recurring CPU work that may contribute to the reported
  ADS lag. It does not fix the scoped Lee-Enfield black screen. Release build,
  portable fit test, and 0.5.211 update package passed; see validation-211.
  Windows denied replacing the local CoD3MP.exe, so the port folder was
  restored to 0.5.210. Neither user issue is verified as resolved.

### 0.5.210 BUILT AND INSTALLED (Team Justin, 2026-10-17): server-client bounds probe

- scripts/patch_recomp.py mp now validates sub_825336F0 and changes both the
  allocation and memset sizes from 0x1D400 to 0x27000 (32 x 4,992 bytes).
  The remaining player loops and MaxClients stay at 24.
- MpFixesFrame marks client records 24-31 and scans the 39,936 spare bytes
  every 10 seconds, reporting the first changed offsets and byte count.
- Release build/install PASS; generated code checked to pass 0x27000 to both
  allocator and memset. Package outputs/CoD3MP-update-0.5.210.zip. No live
  match run; spare-region scan and 0.5.209 mirror spawning need validation.
- The 0.5.208 friend log confirms intermittent lag: 23/286 FPS samples below
  60, including 0-1 FPS at 79-81s, 0 FPS at 90-93s and 0-1 FPS at 108-113s.
  The final 76 FPS sample is not representative. No RTT was logged, and file
  reads/memory stayed flat through later stalls; cause remains unknown.

### 0.5.209 BUILT AND INSTALLED (Team Justin, 2026-10-17)

- In CoD3Host/bots.cpp, sub_824D0AF0 now recognizes joiner copies from the
  mirrored session roster and locally runs the same placement/model/finalizer
  path already used for server bots. This addresses the observed health 0,
  state 0/3 copies: the normal route had treated them as remote-owned and
  network-routed their spawn. Scans client slots 1-23 because mirrors can be
  assigned below slot 4. The callback uses spare local-client index 3 through
  BotClientScope and restores the joiner's prior client/view state.
- Move the bot grenade/smoke trace until after cmd+8 receives its selected
  weapon; the previous trace sampled the cleared word. Friend's 0.5.208 log
  has no bot throw decisions or throw traces, so the missing animation's
  command/state cause is still unknown. No gameplay input changed yet.
- Release CoD3MP and CoD3 built and installed; installed executable hashes
  match. Friend package is outputs/CoD3MP-update-0.5.209.zip. Not tested in a
  live match.
- Active client cap remains 24. The 32-record player-storage stage passed
  in 0.5.208; the server-client list, sessions, sentinels and UI remain to be
  expanded before additional players or bot slots are enabled.
- User clarified the last game was intentionally ended early due to lag;
  neither user's log ending should be treated as evidence of a crash.

### 208 FRIEND MATCH RESULT (Team Native, 2026-10-17): intentionally ended early due to lag

- User supplied Downloads/CoD3MP (14).log: 0.5.208 on Rouen. Last complete
  time marker 286 s, 76 fps, active player. The user clarified both players
  intentionally ended the match early due to lag. Four early
  0x4001000A/06 debugger exception codes; twelve outside-allocation reports
  are all during initial map load. No bad calls or effects-pool failures.
- Relocation: 30 post-map checks: relocated 24, old pointers 0, spare and
  guard clean every time. Player limit remains 24.
- Confirmed multiplayer issue: all 299 joiner mirror spawn messages report
  health 0 / state 0 or 3 immediately after spawn ("NOT alive after the
  spawn"). Only one host death was applied to a mirror. Trace/fix
  MirrorHostBots' spawn; do not claim bot life mirroring works yet.
- Scoped rifle: 8 ADS enters and leaves; aim/shell-shock blur off. One native
  FOV sample 83.45 at ADS .617, other sampled ADS values stayed 90. Log has
  no visual black-screen marker, so root cause unresolved.
- Physical allocator reports 170.4/511 MB handed out and 0 free ranges at the
  tail, but zero ranges is also reported at startup and remains flat once the
  map loads; this does not establish out-of-memory. Need friend's Windows
  Application Error/WER exception code and faulting module if reproduced.
- Detailed review: outputs/Friend-match-review-2026-10-17.txt in the Codex
  task outputs. Raw 16 MB log not copied into source tree.


208 validation: Release build/install PASS; helper/climb/patch tests PASS.
Eder Dam bot smoke to 84 s PASS (all 24 relocated, old pointers 0, spare/guard
clean). Long matches, map changes and System Link remain untested.

### 208 BUILT AND INSTALLED (Codex, 2026-10-17): player-record relocation + Team Native merge

- Read NEXT-SESSION/HANDOFF/PATCH-NOTES/SHARED-LOG and corrected study.
- patch_recomp.py patch_player_records redirects exactly three absolute bases:
  sub_82506540 r3, sub_82593FA8 r29, sub_82595118 r11. All remain 24 loops.
  level.clients and entity+512 then follow the relocated storage. Allocation
  is Guest::AllocateExtended(32*2992+64), copied initial 24 records, zero spare
  eight, A7 guard. MpFixesFrame reports pointer/guard checks every 10 seconds.
  Environment COD3_NO_PLAYER_RELOCATION bypasses before first use; allocation
  failure falls back consistently. Patch validates all sites before writes.
- IMPORTANT CORRECTION: sub_82594FD0 initializes a separate 24 x 6224 array
  beginning 0x82C03A50. It is NOT a gclient end marker; do not relocate it with
  the 2992 array. 825611D8/8256CF80 follow pointers; stride stays unchanged.
- Incoming Downloads/CoD3Recomp-0.5.206-for-Team-Native_1.zip: reconstructed
  its 0.5.205 ancestor by reversing supplied diff, three-way merged only its
  code delta. bots.cpp/kernel_video.cpp/mp_fixes.cpp merged without conflicts;
  added world_model_fit.h, tests/world_model_fit_test.cpp and pose test scripts.
  Preserved mainline 206 navigation and 207 C3BS. Incoming version not adopted.
- User reports intermittent black scope on Lee-Enfield ADS. Both installed
  aim_depth_of_field and shell_shock_blur already 0. No proven cause/fix.
  Added bounded local scoped ADS transitions and first 3 render frame traces
  in MpFixesMg42View. No FOV/scope/renderer changes guessed.
- Validation/build status: see docs/build-logs/validation-208.txt (updated
  after checks). No live-match or System Link validation claimed.
- Next: run a 20/22-bot match and change maps, look for player-storage lines
  (old pointers 0, spare clean, guard clean). Then enlarge 4992 server-client
  list separately; session 784 records/sentinel/UI still follow afterwards.

### 2026-10-17, Team Justin: 0.5.206 for Team Native: campaign guns' third-person models
- Built on 0.5.205 (the package you have). Diff: `CHANGES-0.5.205-to-0.5.206.diff`.
- Files: new `CoD3Host/world_model_fit.h` (pure: world_models.txt parsing,
  transforms, an optional "auto" fit; tests `tests/world_model_fit_test.cpp`,
  `g++ -std=c++20 -I CoD3Host`, all pass); `CoD3Host/mp_fixes.cpp`
  (`g_copyWorld[]` recorded in MakeGun before +1432 is replaced;
  `WorldFitSeeMesh` called from the sub_82446AB8 hook for every mesh;
  `WorldFitFrame` from MpFixesMg42View each frame; `MakeMg42Body` now reads
  the MG42 body's vertices and box as loaded through `WorldFitOriginal` /
  `WorldFitOriginalBox`, since the third-person MG42 is the same mesh and is
  moved in place; `MpFixesCampaignWeaponNumber` for bots.cpp);
  `CoD3Host/bots.cpp` (`BotsPose`, `BotsPoseTest`, `PoseFrame` at the top of
  FeedBots, `PoseHolds` skips Think for posed bots and forces their weapon;
  `#include "overlay.h"`); `CoD3Host/kernel_video.cpp` (console
  `bot_pose_test`, `bot_pose`); `port/tests/RUN-GUN-POSE-TEST.ps1/.bat`;
  version_number.h; PATCH-NOTES.
- Problem: bots holding the MG42 looked wrong. Its world model is the
  campaign AI's `w_ge_MG42_nobipod` (meshes `mg42.` and `stand.`); in a
  multiplayer hand it sat ~14 units above the hands, ~25 behind, about 25%
  oversized, the stand hanging from the hands.
- Fix: each campaign world model's meshes ("<part>.<model>") are kept as
  loaded and written again through a per-gun transform from
  world_models.txt (beside the exe, re-read when saved). Default: `* off`,
  `mg42 0 0 0 0.8 25 0 -14`, `mg42 hide stand`. Vertices (52-byte: float3
  position, packed 10:10:10 normal/tangent/binormal at +12/+44/+48) and the
  mesh box (+0x40+32 min, +0x40+48 max; +44 is padding) rewritten; a hidden
  part gets its counts zeroed (+28, +0x40+88, +0x40+104, +0x158+28, as the
  MG34 bipod in the MG42 view), not its vertices (parts may share a buffer).
  A mesh reloaded at the same address is noticed by its first vertex.
- The "auto" fit (turn/scale/move from both models' boxes) is in but OFF by
  default: it guessed the MG42's barrel end wrong (turned it backwards).
- Tested by Team Justin (this build, Eder Dam, offline, scripted): pose test, all
  8 campaign guns beside their copies, side/front/left; MG42 in the hands.
  First person MG42 checked with the fit off and on: identical (an earlier
  build read the box wrong and shifted it; fixed).
- Found, NOT fixed: after a respawn, a bot with a campaign gun in his kit
  owns it (kit slot byte set, e.g. "slots 39 and 11") but FeedBots' weapon
  choice takes the pistol (rounds() sees none for it). Posed bots are forced
  (`g_poseWeapon`); real matches may show bots with campaign kits on
  pistols. Also seen: during the pose test the screen split into four with
  "SIDE SELECTION" in three panes some runs (bots taking local places?), not
  investigated.
- UNTESTED: System Link.


### 2026-10-17, Team Justin: 0.5.207 (bot deaths on joiners; roles)
- Why joiners saw bots freeze and vanish: their copies of the host's bots
  were never spawned on the joiner (health 0, state 0), so nothing died
  there. New side-channel packet "C3BS" (host -> joiners, 250 ms): each bot's
  alive flag, life count and last death (killer number, means, weapon,
  hitLoc, damage, dir). The joiner spawns its copy per life (sub_822029F0)
  and kills it through sub_82587268 with the host's values. lobby_rules.cpp
  dispatches C3BS. COD3_NO_MIRROR_LIFE=1 disables it.
- Bot roles are balanced per side (25/25/28/22).
- NOT tested in a match yet. Your log: look for "mirror number N spawned
  here" / "dies here as on the host".
- Source: CoD3Recomp-0.5.207-source.zip (replaces 0.5.205; base your work on
  it, versions 0.5.901+, diffs against 0.5.207). Start at docs/NEXT-SESSION.md.

### 2026-10-17, Team Justin: 0.5.206 (crate fix; the players' array found)
- 0.5.205 tested: effects pools held (peak 329/8192), no crash.
- bot_navigation.h Climb: the rise must match the landing ground's tilt
  (crates are ledges). Test docs/build-logs/test_bot_climb_206.cpp.
- The 24 server players' records: 0x82BF21D0, 2992 bytes each, contiguous,
  no room after. About 6 functions address it directly (see HANDOFF 206).
  If you touch player records, use entity+512, not that address.
- NOT tested in a match yet.

### 2026-10-17, Team Justin: 0.5.205 (effects pools, hills, War flag teams)
- Host crash in the 0.5.204 match: the particle system's pools ran out with
  22 bots ("apsMemory : pool(size=128, count=4096) is empty", "apsEffect :
  pool is empty"), then bad calls in its drawing (sub_82110FD0). Sizes are
  set in sub_82430A58 via sub_82109F38/F48/F58/F68 (table 0x825B0FD4). We
  double them in mp_fixes.cpp (overrides at the end). If your weapons add
  effects, these pools are where they come from.
- BotNavigation::Reachable now climbs slopes up to 45 degrees in pieces
  (bot_navigation.h, test docs/build-logs/test_bot_climb_205.cpp).
- War: 40% of each side plays the flag (takers on it, screens toward the
  enemy spawn).
- Your missing bot death animations: new "bots: death watch here:" lines on
  the joiner. Please send your CoD3MP.log from a 0.5.205 match.
- Player-limit study correction: 0x82C2ECF0 x 6352 is the 4 local clients,
  not the 24 server players. The real array is being located by the probe.
- Source: CoD3Recomp-0.5.205-source.zip (replaces the 0.5.204 package; base
  your work on it, versions 0.5.901+, diffs against 0.5.205).
- NOT tested in a match yet.

### 2026-10-17, Team Justin: 0.5.204 source package for you
- CoD3Recomp-0.5.204-source.zip: the whole handwritten source at 0.5.204 with
  both of your 0.5.900 packages merged. Please base your next work on it
  (0.5.901+), and send a diff against it, not against 0.5.193.
- CHANGES-0.5.193-to-0.5.204.diff in the package root shows every code change
  since your base. docs/kangaroo has the Kangaroo write-up. port/examples has
  the vehicles.txt and bot_kits.txt we play with.
- The tests in tests/ and docs/build-logs/test_*.cpp pass (g43_selection_test.py
  needs a git checkout).

### 2026-10-17, Team Justin: 0.5.204 stability fix (please merge too)
- 0.5.203 froze (fps 0) about 15 s after bot_add 24. Cause: the game's shared
  small-block pool *(0x82BCBF5C) (sub_820E0D40; bins 20/32/64/128/256 bytes,
  4096/2048/4096/1024/450 blocks; heap fallback flag off) ran out. The
  script built-ins endon/wait/waitframes (sub_824C9258, sub_824C4840,
  sub_824C4910, sub_824C9350) don't null-check and link "0+4" into the
  thread's record list, so the endon search sub_824C0AE0 loops forever.
- Fix in mp_fixes.cpp (end of file): sub_820E0D40 falls back to
  sub_820DE750(16,size) on 0 (the pool's free already returns non-pool
  blocks to the heap), and sub_824C0AE0 has a step cap. "pool:" log lines
  every 30 s show bin use.
- For you: your campaign archives and the 36 MB sound memory add to what this
  pool holds. If you hook anything that allocates through sub_820E0D40, keep
  the null check. Please send a CoD3MP.log with the "pool:" lines from a
  game with your new guns loaded.
- NOT tested in a match yet.

### 2026-10-17, Team Justin: your second 0.5.900 merged into 0.5.203
- Applied as the change between your two packages (56 hunks) on top of our
  0.5.202: everything went in; your tests pass; lists only appended. Bots can
  carry the MG42 and the scoped Lee-Enfield (bot_kits.h). Your scoped
  Lee-Enfield counts as a sniper rifle in our code (name has "sniper": our
  scoped walk/sway settings and bot overwatch apply to it).
- Please start from the 0.5.203 source next time (ask Justin for a
  package): sending a diff against 0.5.193 again means we untangle what we
  already merged. Your next numbers: 0.5.901+.
- What you added, as you logged it:
- Sticky bomb: custom class grenade kind 4 = multiplayer's own unused
  stickygrenade (item 30, full weapon file in weapusge.cod). Also found in
  MP memory: rifle-grenade attachments on both rifles (M1GARAND_RG,
  K98_RG), a handheld MG42 model (w_ge_MG42_nobipod), Panzerfaust impact
  effects and its 3rd-person fire sound. No bayonets or suppressors.
- MG42 (primary 20, sidearm 22): an MG34 copy with the AI MG42 model,
  the MP Horch MG42's own gunfire (forest.wbk MG42_FIRE_1ST/3RD_LASTSHOT
  = MG42_HORCH_FIRE_1ST/3RD) and 50 ms fireTime; no first-person MG42
  exists in the game. A gun with no animations of its own now uses its
  copy's reload/handling event sounds (the 0x82555E90/F90 own-name check).
- MG42 view: the MG34 view model's body mesh is swapped in place for a turned,
  scaled copy of the AI MG42 body while the MG42 is held (MpFixesMg42View).
  The MG34 bipod is hidden then; the body sits on the MG34 drum and sights.
- G43 reload 3.45 s so the bolt/magazine sounds of GR_G43 meet the hands.
- MG42 aims from the shoulder (BAR's class, no bipod deploy / forced prone);
  a second body fit is drawn while aiming so the sights are clear.
- Scoped Lee-Enfield (Laison's lee_enfield_sniper): primary 21 / sidearm 23;
  Laison archive + bank loaded, map block 495 MB.
- Session 4: MG42 aiming view rebuilt from measured sight positions (rear
  sight now in front of the eye, front post in the notch) and a smooth, lower
  raise/lower; the MG34 aims from the shoulder too (ini `mg34_no_deploy`,
  default 1, changes a stock gun: set 0 to restore); scoped Lee-Enfield had
  endless rounds (its campaign fireDelayMP 0.1 skipped the clip in MP), now 0,
  so it empties, reloads and plays GR_Enfield_Scope_start/_reload/_end; it now
  works the bolt out of the scope like the scoped Kar98; its double shot (the
  copy's Springfield shot events) is gone; both Lee-Enfields use the Kar98's
  idle (the British idle put the right hand into the butt at spawn and after
  reloads). See the patch note.

### 2026-10-17, Team Justin: 0.5.202 source (NOT BUILT yet)
- Bots aim down sights (command button 0x8 = the sights; launchers fire only
  aimed). Spawn-aware errands: leave spawn, push to the enemy spawn, check
  the War flag, own/enemy-half leaning, sniper overwatch.
- Player limit Phase 1: docs/player-limit/sites.csv (1,107 sites in 421
  functions) + scripts/player_limit_sites.py. If your work touches the
  player-state array (0x82C2ECF0) or loops to 24, tell us: those are the
  sites that will move.
- Files: bots.cpp, bot_map_knowledge.h, version_number.h, docs.

### 2026-10-17, Team Justin: 0.5.201 source (NOT BUILT yet)
- Bots: one rocket per press for launchers (2.5 s apart, not under 300 units,
  not with a teammate within 300 of the target); no shot with a teammate in
  the line of fire (step aside instead); no grenade onto teammates.
- Files: bots.cpp, version_number.h.

### 2026-10-17, Team Justin: 0.5.200 source (NOT BUILT yet)
- 0.5.199 (with your G43) built and played on Rouen: the G43 works. Bots
  still bunched (56% within 300 of a teammate), so 0.5.200 spreads them:
  fewer objective players (25%, max 4 a side on the War flag), errands kept
  unless an enemy is within 550, at most two chasers per enemy, lanes when
  closing in, wider separation, easier high/low ground picks.
- Files: bots.cpp, bot_map_knowledge.h, version_number.h. Nothing of yours
  changed.

### 2026-10-17, Team Justin: Team Friend's 0.5.900 (G43) merged into 0.5.199
- Your diff applied cleanly onto 0.5.199 (bots changed a lot, mp_fixes.cpp
  only moved; one settings.cpp hunk needed fuzz, checked by hand). Your two
  C++ tests pass; your selection test (git) redone by hand: every list only
  appended. Thank you: very clear notes.
- We added "gewehr43" to bot_kits.h (bots can carry it via bot_kits.txt).
- Main line now: 0.5.199 = bots 194..199 + your G43 and campaign sounds.
  Next time start from the 0.5.199 source (ask Justin for a package) so the
  merge is smaller. Your next numbers: 0.5.901 and up.
- Things in 0.5.197/198 that touch weapons and may matter to you:
  scoped-rifle sway/walk (BotsScopedWeapon, by name: sniper/scoped/springfield,
  so the G43 is not affected); bot kits and grenade gifts in sub_82206028 /
  sub_821F28C8 / MpFixesBinoculars; MpFixesSwitchWeaponFor (script +2692).

### 2026-10-17, Team Friend: 0.5.900, Gewehr 43 in multiplayer
- Built on this package (0.5.193). Diff: `G43-0.5.900-vs-0.5.193.diff` beside
  the package. Files: `CoD3Host/mp_fixes.cpp`, `class_editor.cpp`,
  `overlay.cpp`, `settings.cpp`, `settings.h` (ini `g43_reload`); the map
  block size in `sub_820E0158` (mp_fixes.cpp),
  `version_number.h`, new `CoD3Host/campaign_animation.h`, `campaign_sound.h`,
  new tests `tests/campaign_animation_test.cpp`, `campaign_sound_test.cpp`,
  `g43_selection_test.py`, `docs/PATCH-NOTES.md`.
- `bots.cpp` and `bot_*.h` NOT touched; 0.5.194/0.5.195 should merge cleanly.
  In `mp_fixes.cpp` the G43 hunks are separate from your bot-kit hooks.
- What and why: see the 0.5.900 patch note. Key points for you:
  - G43 stats come from its real campaign weapon file (G43Fields[]), damage
    kept at the Garand's for balance.
  - Campaign sounds: every campaign gun now plays its own campaign shot and
    reload in MP (see the 0.5.900 note). Causes found: campaign banks were
    only registered, never loaded (now sub_824AE270); the engine's 8.5 MB
    bank memory could not hold them (now its own 36 MB of physical memory,
    map block unchanged at 480 MB); shot events are named
    weapon_<copy>_weapon_* so the swap never matched; and the campaign banks
    must be searched after the map's or native guns pick up campaign
    sounds (OrderedCueLookup). User-tested: Bren and FG42 distinct again.
  - `native_commands = 1` breaks MP rendering (missing world, partial
    first-person guns). Our first test had it on by accident.
- Tested: offline match, user confirms everything works. UNTESTED: System
  Link (both PCs must run the same exe), bots with the G43.

### 2026-10-17, Team Justin: 0.5.199 source (NOT BUILT yet)
- 0.5.198 tested (mp_guns): bot_kits.txt and grenades work. Bug found and
  fixed: TacticalMove was given two clocks (GetTickCount from the new vehicle
  code, serverTime from Think), so stuck detection fired twice a frame. Rule
  for both teams: anything that feeds g_tactical / TacticalMove must use
  Think's serverTime.
- Vehicles: bots now press 0x20|0x40|0x400 and look at the vehicle (untested).
- New: Support/Medic bots switch to their special (mp_ammo, mp_ammo_axis,
  mp_revive) with the script's switch (+2692) and fire it near teammates.
  mp_fixes.cpp has a new MpFixesSwitchWeaponFor(ctx, base, entity, name).
- Files: bots.cpp, mp_fixes.cpp, version_number.h.

### 2026-10-17, Team Justin: 0.5.198 source (NOT BUILT yet)
- 0.5.197 built and tested by Justin (solo, Merville). Map learning works
  (1054 places saved). Fixed in 198: every gun aimed-walked at sniper speed
  (the scope test matched all weapons, now by name), the vehicle claim gave up
  at once, a bot stuck above a floor-blocked enemy, and errands now use learnt
  routes.
- NEW bot_kits.txt (game folder) + CoD3Host/bot_kits.h: weight, class
  (support/light/rifleman/medic/heavy/antiarmor/scout = the game's 0..6),
  main gun (custom class list incl. campaign guns), frags, smokes. **G43 team:
  once the G43 is in mp_fixes.cpp's CustomWeapons list, add "g43" to
  BotKits::KnownPrimary and bots can carry it via bot_kits.txt.**
- Bots get grenades their class lacks (script give +2864 count), throw far
  more often.
- Player-limit probe: the "free" gap after the player states is in use, so
  the states must be relocated (PLAYER-LIMIT-STUDY.md updated).
- Files: bots.cpp, mp_fixes.cpp (sub_82206028, sub_821F28C8,
  MpFixesBinoculars, sway block), bot_loadouts.h (two declarations),
  bot_map_knowledge.h (Reach), new bot_kits.h, version_number.h.

### 2026-10-17, Team Justin: 0.5.197 source (NOT BUILT yet, UNTESTED)
- Bots learn each map: new `CoD3Host/bot_map_knowledge.h` (cells of everyone's
  footsteps, one-way links, deaths and kills, A* routes, saved in
  `bots\maps\<map>.txt` beside the exe). Bots get "styles" (outskirts, high
  ground, low ground, hunter) and go on errands there when no enemy is near.
  They also have "form" (kills/deaths) that biases aggressive/defensive.
- Mounted guns: player state +240 bit 0x100000 = on a mounted gun (a 196 bot
  froze 3.5 min on one: reload 0x400 is also use). Bots now fight from it or
  get off (0x20|0x400), or respawn after 5 tries. Vehicles (entity +900 link,
  seat ps+1300): bots crew up to 2 vehicles a side. UNTESTED.
- Sprint limited (1.5 s winded), scoped walk 30% and sway 50% (new ini keys
  sniper_walk, sniper_sway, bot_vehicles). Weapon def field table found at
  0x825D77D0 (name, offset, type) if you need G43 fields: e.g. adsZoomFov
  0x640, adsSwayMaxAngle 0x6D0, moveSpeedScale 0x63C, weaponClass 0xB0.
- HD world textures: `UPSCALE-TEXTURES.bat` in the game folder (Real-ESRGAN +
  texconv on the user's own PC; results in textures\replace\hd, never shared
  publicly). texture_pack.cpp dumps in the background, lists per map.
- Kangaroo for the friend: it is a local placement (vehicles.txt). The friend
  update now carries vehicles.txt. If your build makes its own friend
  packages, include vehicles.txt too.
- Files changed: bots.cpp (ExploreStep, BotSeats, BotKitProbe,
  MapKnowledgeTick, PlayerLimitProbe, Think hooks, sub_82487718), mp_fixes.cpp
  (quick sprint block, scoped sway block in MpFixesFrame), settings.h/.cpp,
  overlay.cpp, texture_pack.cpp/.h, render_resources.cpp (one NoteMap call),
  new bot_map_knowledge.h, version_number.h. G43 work in mp_fixes.cpp's Guns[]
  should still merge cleanly: the sprint/sway edits are in MpFixesFrame only.

### 2026-10-17, Team Justin: player-limit study (read-only, no build)
- New `docs/PLAYER-LIMIT-STUDY.md`. Question: can a match go past 24 players
  (Justin's goal is 16v16 or 18v18)?
- Verdict: **32 players is possible but takes many builds (about 6-10). 36
  also needs wider network masks.** Nothing was changed.
- Key evidence, checked against the decrypted retail MP image:
  - The server client array is heap memory: `sub_825336F0` allocates
    `0x1D400` (24 x 4992) and memsets it.
  - Player states are a static array at `0x82C2ECF0` (24 x 6352). It is
    followed by an all-zero, unreferenced gap of 80,972 bytes
    (`0x82C54070`-`0x82C67CBC`), room for 12 more records.
  - Session players (24 x 784 at object+0x1010) butt against the local slots
    at +0x5991, so they must be relocated. 88 functions use those offsets and
    55 use the 784 stride.
  - 24 is also the "no player" value (17 store sites).
  - The player masks are 32-bit (`sub_823D8630`, `sub_823DC870`).
  - 128 functions compare against 24.
- Proposed next step: a Phase 0 probe build (logging only) to watch the gap
  and log entity numbering. Not started; it waits on Justin's go.
- Team Friend: if you add per-player arrays, size them from a constant, not a
  literal 24, so a later raise is easy.

### 2026-10-17, Team Justin: 0.5.196 (bot navigation), built from source, UNTESTED
- 0.5.195 tested solo by Justin: prone/cover/smoke/melee work; bots still
  stuck high up, near ladders, and bunched.
- 0.5.196: new `CoD3Host/bot_unstick.h` (net-progress stuck detection, drop
  downs, wander, teammate separation, objective spread). `bots.cpp`
  TacticalMove gets a wider ladder scan, ladder descent and stuck escalation.
  Files changed since 0.5.195: `bots.cpp`, new `bot_unstick.h`,
  `version_number.h`, docs. Does not touch `mp_fixes.cpp` (G43 area).

### 2026-10-17, Team Justin: handgun asset audit (read-only, no build)
- Report: `docs/PISTOL-ASSET-AUDIT.md`. No game files or builds changed.
- Shipped handguns: only `colt` (MP ID 8) and `p38` (MP ID 15), both MP only.
  The campaign has no handgun weapon or asset in any of its 15 levels.
- Cut feature found: `sub_821EE270` in codmp_xenonf.xex (from
  `_mp_loadout.bro`) gives `coltdual`/`p38dual` (by team) with pistol ammo ×2
  or ×4 by level. It is in the loadout native table at 0x82094578, next to the
  bazooka/satchel/mine helpers. The weapons it names have no definition or
  assets anywhere, so restoring means registering them as Colt/P38 copies.
- Luger, Beretta M1934, MAS1935 (+ dual names): text-bank display names only
  (shared Treyarch text bank, with PS2 controller strings). Webley and
  Hi-Power: no trace at all.
- G43 note for Team Friend: the weapon key `WEAPON_GEWEHR43` and
  `WEAPONFILE:gewehr43` exist in the campaign (Laison), and `gewehr43` is in
  both executables' weapon tables. Models are in Forest and Laison.


### 2026-10-17, Team Justin: package for Team Friend (0.5.193 baseline)
- Sent: this package (0.5.193 source, docs, this log).
- Main line has moved on since. Not in this package:
  - **0.5.194**: host bots shown to joined players. The host sends a "C3BR"
    roster over the side channel, and joiners create session players for the
    host's bots through the game's own join (sub_824046D8). `bot_add` is
    host-only. Built by Justin, UNTESTED in a two-PC match.
  - **0.5.195**: bots remember failed jumps/routes (new
    `bot_failed_routes.h`). Combat brain: real prone (closed-loop stance
    control from eye height), more cover and peeking, retreat behind smoke,
    melee charges, flanking, aggressive/defensive styles, tracking the last
    seen position including height. UNTESTED.
- Files the main line changed after 0.5.193: `CoD3Host/bots.cpp`,
  `lobby_rules.cpp`, `bot_obstacle_navigation.h`, new `bot_failed_routes.h`,
  `version_number.h`, `docs/HANDOFF.md`, `docs/PATCH-NOTES.md`. For the G43
  you should mostly be in `mp_fixes.cpp` (plus maybe `bot_loadouts.h`), so a
  merge should be clean. If you touch `bots.cpp`, say exactly which functions.
- Next on the main line: 0.5.196, bots using vehicles (driving, gunner seats,
  turrets).







### 0.5.380 test result

Justin TESTED 0.5.380: >4000 draws median 45.6 -> 49.3 fps, cp cpu/draw 3.382 -> 3.218 us (49 vs 43 samples); 4500-5500 draws 45.3 -> 48.95 fps, 3.357 -> 3.170 us (25 vs 20). Different matches. Overall render median unchanged 53.25 fps. Native checks all matched (inverse 2000, cull 3000, slerp 5000); no fatal/native-failure markers. Existing spare-slots diagnostic warning occurs 32 times in BOTH logs. CP ExecuteBuffer active profile median self 13.2% -> 10.6% (not 16%: handoff quoted one window).

Requested same-build PROFILE-MARSEILLE-AUDIT to locate remaining index hashes.
IndexBufferFor already watches after eight unchanged looks; no index source
change yet. Do not claim a new page-watch optimization without measuring.

## 0.5.381 normal test result (2026-10-23)

Justin TESTED 381: >4000 draws 48.7 fps / 3.224 us cpu per draw vs 380 49.3 / 3.218; no clear busy-view speedup. 4500-5500: 48.6 fps / 3.228 us vs 48.95 / 3.170. Overall gameplay render median 54.15 vs 53.25 fps. All profile summaries median fingerprint KB 943 vs 4155 (includes startup/menu summaries). All native checks matched; no fatal/native-failure markers. Keep lower threshold for less scanning, but do NOT claim busy-view performance improved.

## 0.5.382 normal test result (2026-10-23)

Justin TESTED 382: >4000 draws 46.0 fps / 3.132 us cpu per draw vs 381 48.7 / 3.224, BUT median busy draws rose 4691 -> 5284. In 5000-5499 bin: 46.3 fps both, cpu/draw 3.190 -> 3.087 (7 vs 10 samples). 4500-5500: 46.75 fps / 3.122 us (16 samples, med 5144.5 draws) vs 381 48.6 / 3.228 (28 samples, med 4770.5). Overall render fps 54.4. Different bot matches, no fixed-workload causal claim. All native checks matched; no fatal/native-failure markers. ExecuteBuffer active self-share ~11.35% vs 381 11.3%; no clear share reduction.

## Measurement correction (Codex, 2026-10-23)

Important: the inherited handoff and earlier Codex status messages called
render cpu_ms*1000/draws the whole drawing-thread cost. Source inspection
shows this is incorrect: render_stats.cpp computes cpuMilliseconds from
ONLY drawNanoseconds + resolveNanoseconds + presentNanoseconds, measured
inside Render::Draw/Resolve/Present (render_d3d11.cpp). It EXCLUDES normal
packet decoding/register writes outside those scopes. Thus the ~3.38 ->
3.13 us ratios remain valid RENDER-SCOPE comparisons, but cannot directly
measure ExecuteBuffer optimization or prove its causality. Sampling self
shares and matched draw-bin frame times must also be used. The separate
profile draw-only number excludes resolve/present too and uses draws+skips.
No full-thread CPU measurement yet. Do not propagate the older interpretation.
Justin explicitly extended the session beyond the original hour: keep working
on smoothness/stability, while documenting all changes for Claude.

## 0.5.383 normal test result (2026-10-23)

Justin TESTED 383: >4000 draws 47.6 fps, render-scope CPU/draw 3.080 us (35 samples, med 5062 draws); 382 46.0 fps /3.132 us (61 samples, med5284). 4500-5500: 47.6 fps/3.131us vs382 46.75/3.122 (mixed). 5000-5499 fps46.4 vs46.3, scope CPU/draw3.111 vs3.087 (no clear speedup). IndexBufferFor active sampled self median3.45%->2.9%; KeyTable<4>::Find0.8%->0.75%. Overall render fps56.8 vs54.4, different workload. All native checks matched; no fatal/native-failure markers. Keep as provisional; same-build off-switch comparison desirable. Do not call render-scope ratios full CP CPU.

## 0.5.384 first full-thread baseline (2026-10-23)

Justin TESTED 384 normal. New TRUE whole-CP baseline: >4000 draws n57, med5534 draws, 42.9fps,21.88ms CPU/frame,3.925us CPU/draw,95.6% core. 4500-4999 n19:19.23ms,4.132us,47.3fps (render line47.7 due rounding/boundary at4999.5); 5000-5499 n5:20.83ms,3.792us,45.7fps. Overall render median56.7fps. Renderer-scope busy CPU/draw3.091us is NOT full CP. Agent now authorized by Justin to launch batch tests while he watches. Same-build 384 COD3_INDEXLOOKUPCACHE=0 comparison launched automatically00:28:03, batch PID11764/game22984; IN PROGRESS.

## 0.5.384 same-build index-off comparison CRASH (2026-10-23)

Agent launched BIG-PROFILE-32.bat with ONLY COD3_INDEXLOOKUPCACHE=0 at
00:28:03. User was running around playing. It crashed at ~110 s game time
(~90 s into the bot match), NOT a completed 150 s run. Archive:
Codex chat work/cod3-380/test-0.5.384-index-off.log (absolute directory in
VALIDATION-0.5.380-381.md). Do not treat its partial fps as a successful test.

First failures: apsGroup allocation asks 0x53C62AD0 bytes (~1.4 GB), then
apsMemory effect pool size128/count8192 empty, then bad indirect calls in
sub_82110FD0 <- sub_820FC850 <- sub_824472C0, finally Guest fault read
0x84C28277 / C0000005. There are 350 apsGroup alloc failures and 368
pool-empty messages. Last periodic counts before escalation were only
196/8192 effects, 297/4096 nodes (peaks395/815), so don't blindly enlarge
pools. First bad bytes is product of group count(r4) and config+4 from
sub_82109310 (copied to group+120, count stored+156). No input values yet.
Its caller sub_821106B8 frees the group/returns0 on constructor failure;
parent sub_820FC850 handles null group. Why inputs corrupt is OPEN.

Existing mp_fixes.cpp 0.5.205 comment describes the same particle drawing
call chain after pool failures. 0.5.205 enlarged group/effect/node pools;
0.5.242..244 enlarged general memory pools. They are active in this log:
4MB group,8192 effects,4096 nodes,general pools1024 blocks each. No proof
that the current crash is identical to the old one or caused by new work.

Earlier broad 'no fatal markers' checks were too narrow to catch bad-call
warnings; all saved successful runs including 378 already have 53-68
'not a function' diagnostics (mostly ordinals/nulls), but NO Guest fault or
unhandled C0000005. The index-off run has 112, including corrupt objects.
The distinction matters: don't call every old warning a new regression.

Control run launched 00:33-ish (check process/log timestamp), batch27392:
same384 with COD3_REMEMBERALLPACKETS=1, COD3_WATCHSTILL_KB=4,
COD3_INDEXLOOKUPCACHE=0; restores pre380 performance behavior. IN PROGRESS.
No performance source edits made; proposed constant-shadow-copy reduction
NOT implemented. Pause performance changes until crash investigation.

## 0.5.408 Codex preparation (2026-10-23)
HeapWait exact phase request and safe eight-heap metadata audit. Off COD3_HEAPWAIT_AUDIT=0. No allocation semantics changed; freeze cause unresolved. Build/test pending. Release407 still needs GitHub connection.

## 408 result and release407 publication (Codex)
408 BUILT OK (3.6min), installed, Rimling32 completed clean: heapwait0/fxguard0/native differs0/Guest fault0. Median gameplay55.5fps at3015draws vs saved40758.3fps at2319draws, no >4000-draw samples; not matched-load regression evidence. Main CPU17.345 vs16.025ms. Freeze NOT reproduced; diagnostic path unexercised. Fresh407 city baseline also completed clean (~58.75/57.41/53.32/55.41 fps,3782/2371/4111/3642 draws); camera yaw differs from historical comparisons.
GitHub account hovg-N8 linked; source-only v0.5.407 PUBLISHED https://github.com/hovg-N8/CoD3Recomp/releases/tag/v0.5.407 at40db64a6856fbf7dce494f715b1c9284e587a4fe, treeb64cf3fa9c33be3ff7fe89bf3d5785238fade1fc. 101 text/source changes audited, no game assets/generatedPPC/binaries/logs uploaded. Release snapshot excludes408. Source release workflow40051496183 succeeded, release draftfalse/assets[], tagSHA verified. Public main fast-forward requested to sameSHA. Nested local checkout untouched atA3; fetch before future pushes.
