<!-- claude-2026-11-05-handoff -->
**Bots, 2026-11-05 (Claude):** no bot code changed. Bots on Seelow go for the tanks ('bots: vehicle: slot N goes for mp_shermanTank ... as driver/gunner') and sometimes fire them (tankevents2/4), often giving up ('took too long', 'a fight'). A test can only see a tank shot when a bot fires one.
<!-- /claude-2026-11-05-handoff -->

<!-- claude-2026-11-03-flare2 -->
**0.5.752-0.5.758 (Claude):** bots' smoke throws had never gone (named the smoke one command, the game took the gun back): now 30 commands. Flare bots: BotNadeWeapon prefers the flare; weapon names at 0xAA.. allowed (5 checks); mp_smokegrenade_special taken at the swap. bot_kits.txt "flare" column; new kits scout mosin_scoped flare, rifleman webley flare, light flare. Verified flarebots5 (29 lit), flarekits1 (28 bots).
<!-- /claude-2026-11-03-flare2 -->

<!-- claude-2026-11-03-flare -->
**0.5.739-0.5.741 (Claude):** bots.cpp NadeKindOf counts "flare" as smoke; BotNadeWeapon names only a grenade the bot owns (+1056 mask) (the signal flare shares the smoke's rounds). Bots given the flare (COD3_BOT_FLARES=1, tests only) cannot throw it: 14 tries spent nothing. Normal kits unchanged.
<!-- /claude-2026-11-03-flare -->

<!-- claude-2026-11-02-evening-handoff -->
**Bots, 2026-11-02 evening (Claude):** no bot code changed. Seelow tests: mantle736 (a 67 climb), step737 (8 bots CTF: no crash, stuck 21 vs 23). Bot stuck counts are noisy run to run: use the user's 'where:' lines for walkability.
<!-- /claude-2026-11-02-evening-handoff -->

<!-- claude-2026-11-02-afternoon-handoff -->
**Bots, 2026-11-02 afternoon (Claude):** Seelow collision without the clip-bit swap: in an 8-bot CTF test (unswap1) bots mantled 11 times (2 before), no crash. Stuck counts are noisy (23 vs 17). No bot code changed.
<!-- /claude-2026-11-02-afternoon-handoff -->

<!-- claude-2026-11-02-midday-handoff -->
**Bots, 2026-11-02 midday (Claude):** 0.5.729 the big test sends devmap again if no map archive opened in 15 s (seelow14bots lost its devmap: not the .dcgb drop). Mantling (mp_fixes.cpp, PM_StepSlideMove) lets bots climb WaW's ledges on Seelow (44-53 units seen). No other bot code changed.
<!-- /claude-2026-11-02-midday-handoff -->

<!-- claude-2026-11-02-handoff -->
**Bots, 2026-11-02 (Claude):** no bot code changed. Seelow bot stalls: 29 (no prop collision) -> 73 (props) -> 46 (WaW player clip fixed); stalls cluster at WaW mantle ledges, the water mill, hedgehogs (bot routes don't know the props). The last run (seelow14bots, after dropping Rouen's .dcgb) had NO bot activity: check first.
<!-- /claude-2026-11-02-handoff -->

<!-- claude-2026-11-01-night-handoff -->
**Bots, 2026-11-01 night (0.5.703-0.5.725, Claude):** bots carry and fire the M2 flamethrower (test kit: tools/waw/flame_test.sh); their flames are drawn for us in the world (flamethrower.cpp), traced to walls with BotsFlameTrace (bots.cpp, new export); WaW overheating applies to them too: an overheated player's fire bit is cleared in his newest command before ClientThink (bots.cpp sub_82593518 hook, MpFixesFlameOverheated in mp_fixes.cpp). The 0.5.702 'player still' diagnostic was removed (0.5.703).
<!-- /claude-2026-11-01-night-handoff -->

<!-- claude-2026-11-01-evening-handoff -->
**Bots, 2026-11-01 (0.5.700-0.5.702, Claude):** Climb (bot_navigation.h) lifts one stair (18) before the whole rise (steep stairs under a ceiling were "no way": Carentan cellar 400-552 1816); Detour trace budget 96 (was 64), short escape 40 (was 24) (bot_obstacle_navigation.h); NetProgress "room" test: within 450 of where he was 30 s ago with a far goal = strike 4 at once -> unstuck along his route (bot_unstick.h). Carentan 10-bot CTF: stuck lines 172 -> 46 once Rouen's collision was gone. 0.5.702: "player still" diagnostic (bots.cpp PlayerStuckWatch) and COD3_BENCH_LOOKAT.
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

# CoD3 Recomp: the bots, from the first to now (0.4.3 to 0.5.476)

Every change to the multiplayer bots and their logic, gathered from
PATCH-NOTES.md (all sessions and agents), by theme and in order within each.
The full detail of each is in PATCH-NOTES.md under its version.

## 1. Making bots exist at all (0.4.3 to 0.5.165)

The original game has no bots in multiplayer. Getting a bot into a match
without breaking the host was the long first stretch.

- 0.4.3: first experimental bots on the DLC maps.
- 0.5.60 / 0.5.61: bots added from the console (`bot_add`).
- 0.5.109, 0.5.113 to 0.5.117: each bot set up in its own context (not the
  host's current player), a valid default class, the game's own spawn
  finalizer, model selection and a drawable body.
- 0.5.125 to 0.5.127: full animation set-up; bodies animated after the
  server frame (bots were invisible before).
- 0.5.130 to 0.5.131: each bot gets a session player (visible) and a name
  ("Bot 1" to "Bot 24", later 32).
- 0.5.132 to 0.5.134: kits given again on respawn, an empty-handed bot asks
  for his last gun, guns switched when one runs dry.
- 0.5.135 to 0.5.148: the hard part: each bot as its own player without
  taking over the host (local places, player numbers, online-player joins
  past the limit of 3, the host's controls, field of view and gun view
  kept safe, no phantom split-screen player).
- 0.5.150 to 0.5.164: no gun flicker; each bot's own gun-view memory; the
  host's controls no longer written over; bots' deaths, death animations,
  kill feed and kills credited properly.
- 0.5.169: fix for bots respawning every frame.
- 0.5.171 to 0.5.176, 0.5.185: deaths played on the host, stance and input
  isolated, the repeated bot-kill crash traced and contained.
- 0.5.182, 0.5.187 to 0.5.189: owned guns recovered on kit change, the game's
  own weapon drops on death, each bot's own network send timers.
- 0.5.183, 0.5.207: bots balanced across both teams; balanced roles.
- 0.5.194, 0.5.207, 0.5.209: the host's bots shown to everyone in the match,
  joiners see them die.
- 0.5.204: the freeze after adding bots fixed.
- 0.5.277: bots as a lobby setting (how many, from the lobby).

## 2. Fighting

- 0.5.88: crouch, throw grenades, take flags.
- 0.5.90: sprint, and a reaction time before the first shot.
- 0.5.110: seek cover, steer round obstacles.
- 0.5.166: aim at you prone, grenade your cover, hide to reload.
- 0.5.170: melee up close, throw smoke, reload, jump.
- 0.5.186: aiming above/below the target corrected.
- 0.5.195: prone, cover, smoke for cover, melee charges, flanking; "form"
  (a bot on a streak plays bolder, one dying often more carefully).
- 0.5.201: no rocket volleys; bots stop shooting their teammates (hold fire
  with a teammate in the line).
- 0.5.202: aim down the sights; leave spawn; push; check the flag; snipers
  take overwatch.
- 0.5.215, 0.5.219, 0.5.220, 0.5.222: the real grenade buttons found, throws
  not cancelled, frags and smokes that actually land; 4 mines and 4 rifle
  grenades.
- 0.5.224: shotgun bots rush in; more smoke.
- 0.5.243, 0.5.244: smoke grenades thrown again.
- 0.5.278: bots lay mines.
- 0.5.326, 0.5.329: rifle grenades used (and working).
- 0.5.416: the grenade check remembers each weapon's kind.
- 0.5.455: one grenade at a time (2.5 s between any two throws, shorter
  presses, only the two real grenade keys tried: smokes and stickies were
  thrown in bunches).
- 0.5.456 (aim, Justin: "they lock on and beam me"):
  - aim at the body, waist to upper chest, drifting, not the head;
  - aim settles: 6 degrees of spread at first, down to 2 over 1.2 s, more
    from the hip;
  - reaction 450 to 850 ms (was 350 to 650);
  - sights up past 250 units (was 700);
  - rifle grenades more often.
- 0.5.457: bazooka bots draw their pistol up close (they stood still before);
  stop to fight crouched or prone far more often.

## 3. Classes, kits and support roles

- 0.5.168: roles and classes.
- 0.5.192, 0.5.193, 0.5.198: varied kits and primaries; kits you can
  customize; many more grenades.
- 0.5.199: Support ammo packs and Medic health packs.
- 0.5.216, 0.5.217, 0.5.220: medics revive; dead bots shout for a medic;
  more medics.
- 0.5.224: medics keep the body in sight.
- 0.5.284: the Demolitionist class; 0.5.326: Demolitionists drive.
- 0.5.329: Rusher 2 (a very aggressive flanker class with its own errands).
- 0.5.331: the class artillery comes faster with kills.
- 0.5.456: packs used far more (one teammate near is enough, or for himself
  when hurt or short of rounds; only an enemy within 1500 stops it).

## 4. Moving through the map

- 0.5.110: steer round obstacles.
- 0.5.190: keep moving through tight spawns and doorways.
- 0.5.195: no more jump loops.
- 0.5.196: get unstuck (no-progress test), spread out, use ladders, drop off
  ledges; failed spots remembered.
- 0.5.197: bots learn each map as it is played (where players walk, as
  linked cells, kept in a file per map, growing match after match); use
  vehicles and mounted guns.
- 0.5.199: stuck-bot clock fix.
- 0.5.200: spread out over the map.
- 0.5.205: climb hills.
- 0.5.206: stop walking into crates.
- 0.5.738: bots always get the mantle assist on converted maps (Seelow): players have it as an F11 setting, off by default; test mantle738: 8 bot lifts over WaW windows/ledges (31-60 up) in 90 s.
- 0.5.219: "feelers": a sweep ahead as they walk, sliding along walls; spots
  where they keep bumping are learned and routed round.
- 0.5.282: keep learning; scouts look out of windows (lookouts).
- 0.5.457: the long way round: two in three take their own side route to a
  far objective instead of all on one road.
- 0.5.465: no spawning into the enemy's sights: a bot spawned within 1200
  of an enemy is moved to his side's spawn farthest from them.
- 0.5.466: to objectives along the learnt map (A* route over where players
  have walked, deaths dearer); the last-resort unstick after some 20 s
  (moved along his route or to his spawns).
- 0.5.467: the look round: at a wall, a 24-way sweep at knee and chest height
  for a doorway or opening; routes followed round corners (the farthest
  route spot in clear sight).
- 0.5.468: the route check throttled (every 250 ms).
- 0.5.469: stairs walked up (they were taken for walls); the look round at
  the first wall, narrower doorways; move on after 5 to 10 s at each spot
  (was 15 to 35), campers excepted.
- 0.5.473: with a real objective at last (CTF), stuck lines on Rouen fell
  from some 2000 to 325 a game and last-resort moves from 262 to 13: most of
  the wall-walking had been bots with nowhere to go milling in their spawns.
- 0.5.475 / 0.5.476: CTF spawns moved by the side's own flag (see section 5).

## 5. Game modes and objectives

- 0.5.149 to 0.5.151, 0.5.165: War learnt; bots play War's flag; not every
  bot camps the flag.
- 0.5.205: smarter War flag play (takers onto the flag, screens out front,
  about half of each side).
- Domination (our own mode): bots each take their own place round the flag.
- 0.5.463, 0.5.464: Capture the Flag, Single CTF and Headquarters: the
  objectives found on the map (the flags' touch areas), attackers go for the
  enemy flag and carry it home, one flanker in three guards; the spawn
  points no longer taken for flags.
- 0.5.471: War: takers commit to the flag in play (70% quota), a War readout
  every 10 s ("war: flag N in play ..."); the flags changed hands.
- 0.5.472 / 0.5.473: CTF flags found as the placed trigger_multiples off
  0 0 0 (Rouen: -1792 -91 and 2112 462); carrying kept for the life once on
  the enemy flag, till home.
- 0.5.474: the flag is taken with the use button, not by touch: on a flag a
  bot stands and presses use in pulses for 3 s; the flag props followed in
  the log ("prop N moved").
- 0.5.475: the game spawns each side by the ENEMY's flag; each side's own
  flag set once a map ("ctf sides"), spawns moved by it; defenders: the
  campers and one bot in three. A bot took the flag (prop 49 left its base).
- 0.5.476: every spawn more than 900 from its own flag moved by it (players
  too); a dropped flag chased (attackers pick it up, defenders take it back).
  Confirmed in 0.5.478 (150+ spawns moved).
- 0.5.479: on the flag, a bot walks onto its very place while pressing use
  (5 s): in 0.5.478 they reached it 23 times and stood still 80 out, never
  taking it.

## 6. Killstreaks and bots

- 0.5.441, 0.5.442: a killstreak's kills (Hellfire, mortar, artillery) earn
  no streak; one set of rewards per life.
- 0.5.453: bots' (and joiners') Hellfire falls at once.

## 7. Bots over the network and performance

- 0.5.194, 0.5.207: the host runs the bots; everyone sees them, joiners see
  them die.
- 0.5.436: the log no longer floods in games with bots.
- 0.5.437: over the internet (public games), the bots' roster and lives go
  by the game's own connection.
- 0.5.446: the killcam freeze with 30 bots (the effects memory pools four
  times the game's).
- 0.5.468: the sound flood with 31 bots capped (128 decodes per audio frame).
- 0.5.470: the freeze loading CTF (a mutant no one owned) repaired after 3 s.
- 0.5.476: the killcam freeze with 31 bots in CTF: the 1 KB particle pool
  eight times the game's (it ran empty, then the game's malloc hung).
- The 32-player work (0.5.239 on; the lobby to 32 in 0.5.462) makes room for
  up to 30 bots and two players.

## Where things stand (0.5.476)

Working: bots fight, use classes and their equipment, revive, play War (the
flags change hands), Domination and CTF/HQ (flags found, taken with use,
carried), learn maps, route to objectives, get unstuck, take stairs.

Untested (0.5.476): CTF spawns by the side's own flag every time; dropped
flags chased.

Still to do: bots leaving spawn and crossing the map to the enemy flag in
CTF (Justin, 0.5.475: "no bots actually walked up to the enemy flag"; stuck
in spawn, into walls); real room clearing (checking each room of a building
before moving on); the weapons' sound flood at its source.

## 0.5.628-0.5.631 (2026-10-30, spectator side session)

- Killcam: own clock (QueryPerformanceCounter), the killer's eye blended and held in both cgame snapshots, the HUD on
  (COD3_KILLCAM_EYEBLEND / COD3_KILLCAM_HUD =0 off). Explosions: object places remembered per occupancy
  (ObjMemo); still none found in the replayed objects.
- Grenades: the key trial looks 3 s after the press, with two misses before moving on (629). Both searches are 0x2
  only (631): a key that wandered after a slow sticky (1.3 s) threw extra or wrong grenades.
