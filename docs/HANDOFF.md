<!-- claude-2026-11-05-handoff -->
**Claude handoff 2026-11-05 ~22:20 (0.5.795 built and installed; nothing committed; friend zip on hold):** docs/CLAUDE-SESSION-2026-11-05.md is the handoff. 0.5.781-0.5.795: map changes with tanks fixed and tested both ways; tanks visible (Saint-Lo cut 7 of 8 blocks); Stuka cut (game\sp\crssrds\vehicles_stuka.cod); kapf.py decompresses campaign blocks; tank events through sub_82555160 + Merville's mp_merville.fx (0.5.795, unverified).
<!-- /claude-2026-11-05-handoff -->

<!-- claude-2026-11-03-handoff -->
**UPDATE 2026-11-03 ~20:05 (Claude; source+install 0.5.758; nothing committed): flares have WaW's own sounds, burn whoever stands on them, and bots throw them (kits with the Mosin, Webley and flares). Bot smoke throws were broken before and now work. Read the top of docs/CLAUDE-SESSION-2026-11-03.md.**
**UPDATE 2026-11-03 ~19:00 (Claude; source+install 0.5.751; nothing committed): the signal flare is finished: thrown, flies, lights after 1 s where it lands, no smoke (its grenade sunk, WaW stick left), blinding verified. Read the top of docs/CLAUDE-SESSION-2026-11-03.md.**
**UPDATE 2026-11-03 ~17:30 (Claude; source+install 0.5.743; nothing committed): the signal flare now looks like WaW's flare stick (thrown, lying, in others' hands; first person keeps the game's grenade). Read the top of docs/CLAUDE-SESSION-2026-11-03.md.**
**UPDATE 2026-11-03 ~16:40 (Claude; source+install 0.5.741; nothing committed): World at War's SIGNAL FLARE (class editor SPECIAL row) + Seelow fires centred + drain pipe roof. Read the top section of docs/CLAUDE-SESSION-2026-11-03.md.** Open: ask the user to throw a flare (scripts cannot throw); WaW's flare stick model; bots cannot throw it.
**Claude handoff 2026-11-03 ~01:00 (0.5.738 built and installed; nothing committed; friend zip on hold):** docs/CLAUDE-SESSION-2026-11-03.md is the handoff. Seelow collision finished with the user live (4 .cgb files via the level's unzoned.cgb, WaW hull triangles' sign fixed, ladders, pipe, wall hole, tunnel kerb); night lighting kept to rooms, user's lanterns (my_lights.txt), CoD3 vehicle fire on WaW's 5 burning cars; Mantle assist setting.
<!-- /claude-2026-11-03-handoff -->

<!-- claude-2026-11-02-evening-handoff -->
**Claude handoff 2026-11-02 ~20:45 (0.5.737 built and installed; nothing committed; friend zip on hold):** docs/CLAUDE-SESSION-2026-11-02-evening.md is the handoff (detail: ...-afternoon.md). Seelow/Seelow Night collision WaW-exact, models and foliage fixed, night lit; open: small WaW clip brushes (user's call), daylit arms at night.
<!-- /claude-2026-11-02-evening-handoff -->

<!-- claude-2026-11-02-afternoon-handoff -->
**Claude handoff 2026-11-02 ~15:40 (0.5.734 installed; nothing committed; friend zip on hold):** docs/CLAUDE-SESSION-2026-11-02-afternoon.md is the handoff. Seelow collision: clip-bit swap removed, glass out, prop budget 30000; wheat upright; Seelow Night brighter and Longyard's fog (side session). Collision complaints not yet re-tested.
<!-- /claude-2026-11-02-afternoon-handoff -->

<!-- claude-2026-11-02-midday-handoff -->
**Claude handoff 2026-11-02 ~12:15 (0.5.731 built and installed; nothing committed; friend zip on hold):** docs/CLAUDE-SESSION-2026-11-02-midday.md is the handoff. Seelow (day) clean and tested; Seelow Night new (devmap seelownight, NIGHT=1 tools/waw/seelow_install.sh); vehicles parked; tools/kapf/repack.py is the archive repacker (rules in the session file).
<!-- /claude-2026-11-02-midday-handoff -->

<!-- claude-2026-11-02-handoff -->
**Claude handoff 2026-11-02 ~09:25 (0.5.728 built and installed; nothing committed):** docs/CLAUDE-SESSION-2026-11-02.md is the handoff (Seelow pipeline in tools/waw, data in seelow-work, install with tools/waw/seelow_install.sh; hard-won limits listed there and in memory converted-map-cell-limits).
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

# Current source / local install: 0.5.633 (2026-10-30 12:40), MG42 fix validated on Isle. Read docs/PICK-ME-UP.md, then docs/CODEX-SESSION-2026-10-30.md and docs/CLAUDE-SESSION-2026-10-30.md.
<!-- codex-2026-10-30-handoff -->
**Codex handoff 2026-10-30: source633; install0.5.632; 633 runtime UNTESTED.**
Read [CODEX-SESSION-2026-10-30.md](CODEX-SESSION-2026-10-30.md).
Root cause: Isle's mounted `mg42` animation entry shadows our handheld alias.
633 scopes the MG34 override to handheld setup. Build status: building: 0.5.633 handheld MG42 animation-table collision fix; no test request.
User requested wrap-up; all games stopped, original INI restored, no test queued.
<!-- /codex-2026-10-30-handoff -->

# Current source / local install: 0.5.631 (2026-10-30 00:08). Read docs/PICK-ME-UP.md, then docs/CLAUDE-SESSION-2026-10-30.md and docs/CLAUDE-SESSION-2026-10-29.md.
# Current source / local install: 0.5.479 (2026-10-25 22:50). Read docs/PICK-ME-UP.md, then docs/CLAUDE-SESSION-2026-10-25.md.
# (was) Current source / local install: 0.5.409 / 0.5.409; built OK, Rimling test running. Source-only GitHub release407 published; see top NEXT-SESSION STATE. Read PICK-ME-UP.md and top STATE lines.

# CoD3 Recomp: handoff for the next assistant

Last updated **October 23, 2026**. Source / local install: **0.5.409 / 0.5.409** (built OK; read newest NEXT-SESSION STATE for test result).

**START HERE:** docs/PICK-ME-UP.md, then top STATE lines in docs/NEXT-SESSION.md and newest PATCH-NOTES.md. GitHub source-only v0.5.407 is published at https://github.com/hovg-N8/CoD3Recomp/releases/tag/v0.5.407; main/tag both verified at 40db64a6856fbf7dce494f715b1c9284e587a4fe. Release excludes local408/409. Local nested Git checkout is untouched and stale: fetch before pushing. Older sections below are historical.

**READ FIRST (2026-10-23, Codex):** the detailed older sections below stop
at 0.5.304. Current work is documented in docs/PICK-ME-UP.md and
HANDOFF-MARSEILLE-60FPS.md; every build is at the top of NEXT-SESSION.md
and PATCH-NOTES.md. Read those before using historical state below.
Latest: 380 packet diagnostics improved busy views; 381 lowered vertex watch
threshold and cut scanning, but busiest-view speed essentially unchanged.
382 removes more diagnostic packet copies; tested, busy CPU ~3.13 us/draw.
383 index-key front cache tested: no clear matched-bin fps gain; provisional.
384 true whole-thread CPU measurement tested. See CODEX-HANDOFF-0.5.384.md for final crash/control results. Reproducible
380/381 results and local log/backup paths: VALIDATION-0.5.380-381.md.

Read HANDOFF first, then latest PATCH-NOTES. Never upload game files.

**Two teams now (2026-10-17):** also read and update docs/SHARED-LOG.md
every build. Justin's friend + his AI work from the source package Justin
sends (latest: D:/Call of Duty 3 (USA, Europe)/CoD3Recomp-0.5.207-source.zip;
handwritten source only), versions 0.5.901+. Main line stays 0.5.194+.
Merge their files by hand (delta between their packages, patch -F2).
Start a new session at docs/NEXT-SESSION.md.

## GitHub publication VERIFIED � October 16, 2026

Published main: 71fd81c27996c7e6a50fe7c680708fc52c79f3d2
https://github.com/hovg-N8/CoD3Recomp/commit/71fd81c27996c7e6a50fe7c680708fc52c79f3d2
Desktop push complete and independent remote SHA verification PASS.
GitHub Desktop's existing clean checkout (now fast-forwarded to this commit):
D:/Call of Duty 3 (USA, Europe)/CoD3Recomp-main/CoD3Recomp-main/CoD3Recomp
Use that checkout for future pushes. Scratch checkout below is a review copy.
CLI git-remote-https.exe crashed during push; Desktop succeeded. No game
crash/build occurred. 35 source/docs/test files; portable tests PASS; no game
files, binaries, generated game code, runtime logs or dumps uploaded.
Publication record: chat work/github-publication-0.5.183.txt.
This confirmation is local; published handoff contains the preparation record.

## GitHub source checkpoint � October 16, 2026

User authorized updating https://github.com/hovg-N8/CoD3Recomp through183.
Source ZIP folder has no .git. Dedicated checkout for future pushes:
C:/Users/hovgt/Documents/Codex/2026-10-14/i-m-working-on-a-pc/work/github-CoD3Recomp
Base remote commit a0dca2e (0.5.86), branch main. Source/docs update prepared;
push verification recorded in local work/github-publication-0.5.183.txt.
Only handwritten host source, supporting scripts, portable helper tests and
docs included. No game files, generated game code, EXE/PDB, raw logs, textures,
archives or dumps. Preserve fork README credits and expanded .gitignore.
Build/match test status unchanged by publication; see VALIDATION-0.5.183.md.

## CURRENT CHECKPOINT - this supersedes historical build/test claims below

### 304 SOURCE CHANGE (Claude, 2026-10-20): interact arms for the mortar

### 303 SOURCE CHANGE (Claude, 2026-10-20): Laison weapons' drawing set-up; mortar_yaw

### 302 SOURCE CHANGE (Claude, 2026-10-20): mortar_shell in hand; interaction end

### 301 SOURCE CHANGE (Claude, 2026-10-20): mortar interaction from mortar_idle

### 300 SOURCE CHANGE (Claude, 2026-10-20): campaign mortar interaction

- mp_fixes.cpp MortarInteraction (sub_8251AE38 on the local controller);
  input.cpp/.h SetMortarPassFire.

### 299 SOURCE CHANGE (Claude, 2026-10-20): mortar_81mm model, sky kept out

### 298 SOURCE CHANGE (Claude, 2026-10-20): mortar-only KAPF copies

- mp_fixes.cpp: KapfFilter/MakeMortarArchives (file lists cut down), the
  three ExtraArchives point at the copies, g_laisonZoneArchive (zn08's arms).
  docs/KILLCAM-PLAN.md: killcam design draft.

### 297 SOURCE CHANGE (Claude, 2026-10-20): Laison off; vehicle read once a map

### 296 SOURCE CHANGE (Claude, 2026-10-20): Laison after the map; KAPF notes

- MpFixesLoadMortarArchive keeps a bank copy; MpFixesLoadLaisonAfterMap (from
  the sub_82561838 hook, before the mortar vehicle read) loads laison + fx,
  interactions, then MpFixesLaisonWeapons. KAPF format notes in NEXT-SESSION.

### 295 SOURCE CHANGE (Claude, 2026-10-20): Laison archive off by default

- COD3_CAMPAIGN_MORTAR=1 to load it (breaks Fuel Plant's world).

### 294 SOURCE CHANGE (Claude, 2026-10-20): Laison out of map/zone asset lists

- 292 confirmed the invisible world is Laison's archives. sub_82474CB8 hook
  skips g_laisonArchive/g_laisonFxArchive for lists of kind 3/4 archives.

### 293 SOURCE CHANGE (Claude, 2026-10-20): weapon room past 63 (test)

- Tables hold 91 weapons; the 63 cap is the player mask (state +1056, 2
  words, ~100 sites). COD3_WEAPONS_PAST_63=1: Laison's turret/round may be
  64-65. Phase 2 (bigger mask) not started.

### 292 SOURCE CHANGE (Claude, 2026-10-20): 63-weapon limit

- Player weapon ownership is a 64-bit mask (state +1056..+1063; slots
  +1064): weapon numbers must stay <= 63. MakeGun and the Laison weapons
  read check it; mortar_hands removed from Guns[].

### 291 SOURCE CHANGE (Claude, 2026-10-20): campaign mortar files from Laison

- mp_fixes.cpp: g_laisonFile/g_laisonFxFile (LoadExtra after the mortar
  archive), g_laisonArchive/g_laisonFxArchive in the asset lists; filtered
  WEAPONFILE read (mortar_81mm_turret, mortar_shell) at the end of the
  sub_8249E970 hook; filtered VEHICLEFILE read (mortar_81mm) in the
  sub_82561838 hook (new sub_825683D8 hook); 82515CB8 for the interactions;
  MortarCampaignWeapons() picks the campaign effects for the streak rounds.

### 290 SOURCE CHANGE (Claude, 2026-10-20): Fuel Plant plane x1.11, radius back

### 289 SOURCE CHANGE (Claude, 2026-10-20): mortar hands from binoculars; Fuel Plant plane

- mortar_hands from mp_binoculars_nospot (288's interact copy drew nothing).
  MpFixesPlaneFrame: mp_fuel cz = ground + 1498, radius x0.92.

### 288 SOURCE CHANGE (Claude, 2026-10-20): mortar hands

- mp_fixes.cpp: Guns[] mortar_hands (from interact, armsFrom interact_idle_a
  -> ei_MortarIdle, v_81mm_shell models, archive 2); CampaignAnimEntry maps
  w_arms_interact_idle_a -> w_arms_ei_MortarIdle; MortarHands() in
  MortarStart/MortarStop. Load anim + mortar fx: only in laison.cod/fx.cod.

### 287 SOURCE CHANGE (Claude, 2026-10-20): extended window 624 MB

- kernel.h ExtendedCpuBase 0x48000000 (was 0x50000000), ExtendedShift
  0x28000000; GPU extended 0x20000000-0x47000000. gpu_address.cpp
  GpuMaskBits mask | 0xE0000000; write_watch.cpp Pages and native_state.cpp
  g_streamPages to the window end; mp_fixes.cpp sub_820E0158 big map block
  0x26000000 (608 MB). Mortar archive (laison vehicles) on with big_memory.

### 286 SOURCE CHANGE (Claude, 2026-10-20): mortar archive off

- 285 hung Eder Dam loading sp\\laison\\weapons.cod (4th gun archive) after
  the +8 MB sp\\laison\\vehicles.cod: map block full. MpFixesLoadMortarArchive
  only with COD3_MORTAR_MODEL=1.

### 285 SOURCE CHANGE (Claude, 2026-10-20): mortar killstreak

- mp_fixes.cpp, block "0.5.285: THE MORTAR KILLSTREAK" (before the
  killstreak box): MortarUse (local), MortarShell[16], MortarProp[4]
  (others' tubes), MortarSoundAt (script_origin pool of 3, the plane's
  toward-listener placement), MortarLaunch/MortarBlast (G_Damage 825862F8:
  r3 target r4/r5 attacker r6 0 r7 point r8 damage r9 0 r10 14
  MOD_MORTAR_SPLASH, stack +84 0 +92 -1 as sub_8258A040), MpFixesMortarFrame
  (from MpFixesPlaneFrame), MpFixesMortarPacket ('f' C3MF, 'p' C3MP, 'm'
  C3KM), MpFixesMortarUsedOut. Streak.mortars; earned at MortarKills() (4).
  StreakKind + StreakMortar (box 4 kinds); HUD ring/panel in StreakHudDraw.
  MpFixesModelFound(name). g_mortarFile sp\laison\vehicles.cod (8 MB),
  loaded after the Stuka (before the guns) or before the planes without
  campaign guns; g_mortarArchive in the asset lists (sub_82474CB8).
  PlaneNetVersion 3, C3KS 4 bytes/entry.
- input.cpp/.h: SetMortar/TakeMortarFires (held in place, RT counted).
- bots.cpp (end, inside COD3_MP): BotsSideOf, BotsGroundZ, BotsLineClear.
- lobby_rules.cpp: C3KM/C3MF/C3MP dispatched.
- To tune from the first test: kMortarYawOffset (tube direction),
  shell pitch (-90 offset), cue names (Mortar_Slide/Fire/Miss/Explo).

### 284 SOURCE CHANGE (Claude, 2026-10-20): Demolitionist kit

- bot_kits.h: Kit.secondary[24], sticky, label[16]; Parse: optional cols
  6 (second weapon, KnownPrimary or -) and 7 (frag|sticky|-); class
  "demolitionist" = 2 (rifleman: rifle-grenade special) with label.
  Default + Justin's bot_kits.txt line: 2 demolitionist bren 2 0 mg42 sticky.
- mp_fixes.cpp: thread_local g_botSpawnSecondary/g_botSpawnSticky set in
  the sub_82206028 hook (BotsSecondaryName/BotsKitSticky, bots.cpp);
  sub_821DF378: bot pistol name -> WeaponName(second); sub_821E6238: bot
  sticky -> 0x82039438 ("stickygrenade").
- bots.cpp: the last 4 "slot=4" loops (BotSlot, kit balance, ReKit, kit
  helpers) -> FirstBotSlot().

### 283 SOURCE CHANGE (Claude, 2026-10-20): bots in spare local slots (full 32)

- player_capacity.h: BotsInLocalSlots() (COD3_BOTS_ALL_SLOTS != "0"),
  FirstBotSlot() 1 or 4, MaxBotsNow() = Active() - 1 (or - 4).
- bots.cpp: AddBot searches 4..31, then 3, 2, 1; 26 "for (x = 4; x <
  ActiveSlots()" loops -> FirstBotSlot() (each checks g_bot); CommandPlayer
  (own ring for a bot below 4), SV_GetUsercmd hook, IsBotSessionPlayer,
  localBot, killfeed "who", player-dies hook: from FirstBotSlot.
- Unverified: the title's local-player code for clients 1-3 (loopback in
  sub_825334E8 is per local player entry; bots skip snapshots); the
  scoreboard shows at most 24 rows (ROWFULL, 248).

### 282 SOURCE CHANGE (Claude, 2026-10-20): lookouts + decay

- bots.cpp: BotLookout {at, yaw, score, usedAt}, g_lookouts (300 max);
  BotsLearnKill(attacker, victim) (mp_fixes G_Damage hook when the
  target's +776 goes <= 0): flat >= 1100 -> merge (160 units, 80 z, 35 deg:
  score +1, yaw +30% of the gap) or add. LookoutFile = <map>.lookouts.txt;
  LoadLookouts at map change (score x0.85, < 0.5 dropped); saved with the
  map knowledge (SaveMapKnowledge). Map nodes deaths/kills/failed x0.85
  after Load. BestLookout(around, range): score - d/2000 > 1.4, not taken,
  not used in 60 s. Sniper errand: dest = Nearest(lookout) when free; at
  arrival the lookout's way (x2) first in ex.watch. ExploreStep route:
  kit 6 or sniper within 200 / 80 z of a lookout (score >= 1, not the last
  one within 90 s): hold 4-6 s (holdUntil, scanUntil, scanYaw sweep 0.6
  rad), giveUpAt extended. "near" is a Windows macro (renamed around).
- Full 32: PlayerCapacity::ReservedLocalClients = 4: bots in slots 4..31
  (28 at 32). People: host 0, joiner(s) 1..3.

### 281 SOURCE CHANGE (Claude, 2026-10-19): killfeed victim by side

- bots.cpp sub_82529D78 hook: the victim (r7) painted by his side (+520
  -> +4) against g_feedLine.hostSide (this machine's player): red if not
  the same; no side = red (245 rule: victim always red). Killer as before.

### 280 SOURCE CHANGE (Claude, 2026-10-19): Poissons +1600, radius x0.98

- 279 log: 20 "of another player not played" (raise x15, rechamber x4),
  own events still traced ("= me" 10). Plane at 686 on Poissons.

### 279 SOURCE CHANGE (Claude, 2026-10-19): others' first-person handling sounds dropped

- 278 TESTED Poissons: "lobby rules: bots 99 (FILL)"; mines: antiarmor
  slots 9/10/11/26/27 + light camper 30 given, "takes out mp_mine",
  "laid". Plane height 207 / 194 (poissons ground + 400 with ground =
  players' mean z ~ -200): too low.
- Trace: 25 events all from lr 82555344 (sub_82555160), msg +0 00000501
  +432 4 +500 0 for bots and for "me" alike: no third-person form.
- Fix in sub_82554BA0 hook: event "weapon_*" without "_fire" containing
  reload/rechamber/raise/drop/bolt/_ads/pullout/putaway/clip, whose +496
  handle's entity is an EntityList player other than mine: r3 = 0,
  return. COD3_OTHERS_RELOADS=1 turns it off. Poissons plane +900.

### 278 SOURCE CHANGE (Claude, 2026-10-19): mines given to bots; reload trace in 82554BA0

- 277 TESTED: Poissons, 28 bots, 390 s, fps median 39.6 (p10 36.2), 70
  smokes / 94 frags, kits: antiarmor x5, 19 bazooka/panzerschreck event
  56s, roles objective 10 rusher 7 flanker 7 camper 4. No "lobby rules:
  bots" line (bots came from bot_add). mp_mine owned by nobody -> 0 mines.
  "reload sounds:" at 82556868: 0 lines.
- mp_fixes.cpp: MpFixesGiveMines(entity): holder {entity+484}, rank 1,
  sub_821EE6C0 (hooked: spare 4). bots.cpp BotLayMine: Lay gets since/
  given (reset each death); kit 5 or camper (not kit 0/3) -> given once a
  life after 3 s; "bots: mine: slot N (class) given mines: owns/none".
- mp_fixes.cpp sub_82554BA0 hook: "reload sounds: event X for handle H:
  entity E (player N, d away) | msg +0 +8 +432 +500 +504 +524 +528".
  Hypothesis: bots are local index 3 (bots.cpp "shared scratch, remote
  bot"), so the sound code plays their events as a local player's.

### 277 SOURCE CHANGE (Claude, 2026-10-19): lobby BOTS row; bot gear/roles; reload trace

- 276 TESTED two PCs: 24 OK (C3UP once each, joiner "host says we earned
  a UAV", C3KU back), 32 OK ("32 in play", roster 28 bots, ~41 fps host;
  slot 8 mostly route 0 / stalled). Friend's log not read (Justin: skip).
- lobby_rules.cpp (LF line ends!): Page3 row 1 BOTS {OFF,2,4,8,12,16,22,
  FILL}; TakePage3 -> BotsAsk(BotCounts[i]) (99 = FILL, clamped by
  MaxBotsNow). Description text updated.
- bots.cpp: smokes: evasive needs now >= nextSmoke, screen only rushers
  slot%3==0, next 45-65 s. Roles want {0.38,0.25,0.25,0.12}; war quota
  (n*11+10)/20, 3..11. BotLayMine after BotDispense (owned bit +1056,
  near g_war flag < 900 or camper, 8-23 s first, then 35-55 s, 3 a life;
  switch, 0.9 s, fire 0.4 s, 1.2 s, back). WeaponNumber fwd-declared.
- bot_kits.h defaults + Justin's game-folder bot_kits.txt: smokes 1 (0
  heavy/antiarmor), antiarmor lines x3 (ASSUMED the class's own main is
  the launcher: check "bots: loadout" + whether they fire rockets).
  docs/build-logs/test_bot_kits_203.cpp expects 15 defaults (now 17;
  not part of the build).
- mp_fixes.cpp 82556868 hook: "reload sounds:" GR_/GF_/RELOAD/RECHAMBER/
  BOLT: entity, player index, = me, distance, +484/+480, r6 r7 r8, lr.

### 276 SOURCE CHANGE (Claude, 2026-10-19): dedup side packets; joiner killstreak sync

- 275 TESTS: (1) Justin max_players 32, friend 24 (session storage "32 in
  play" vs "24 in play"): friend's own server spawned him at -6526 -6074
  8115 and he fell forever; his roster came as 24 bots; on a team switch
  Justin's game hung in sub_820C27E0 (a list walk: corrupt list) from
  8257A6C8 respawn (sub_82563F10 -> 82561448 -> 824C4AA8). (2) Both 24:
  friend got C3UP twice ("from another machine" x2; Justin's peers list
  has 100.93.247.25 and 100.87.12.49, and his side send goes to each),
  hits doubled (his copy -180 when Justin's had 60). Killstreaks only
  counted on the host (friend's G_Damage sees no bot deaths).
- Packets v2: + 8 bytes (machine id, count); MpFixesOnPlanePacket drops
  own and repeats (512 kept). C3KS (host, 1 s): per non-bot client:
  session number (BotsSessionNumberOfSlot), kills, uavs; joiner
  (BotsLocalSessionNumber, not hosting) sets g_streaks[own]; 2.5 s guard
  after his call; C3KU (his number) -> host uavs--. UAV only (artillery /
  Typhoon on a joiner would fire in his world only).
- Open: bots' reload sounds heard as own, two-PC too; bots off objective.

### 275 SOURCE CHANGE (Claude, 2026-10-19): UAV plane over the side channel; 32 safety in bots.cpp

- P2P (HANDOFF 194): every machine runs its own server; script entities
  are not shared. MpFixesUavPlane now picks the first angle and sends
  "C3UP" (ver 1, side, seconds u16 BE, cx cy cz radius angle floats LE)
  via Net::SideSend; lobby_rules.cpp hands C3UP/C3UH/C3UD to
  MpFixesOnPlanePacket (queue, 64 max), processed at the top of the asks
  in MpFixesPlaneFrame: P -> remote UavPlaneAsk (not re-sent);
  H/D -> MpFixesPlaneHit(..., remote=true) (D = 99999).
- MpFixesPlaneHit(remote=false): own-side check, then only this machine's
  shooters (attacker == own EntityList[1], or host && BotsIsBotEntity);
  hits sent as C3UH (side, hit u16, rocket); shot down sends C3UD.
- bots.cpp: BotsIsBotEntity, BotsThisMachineHostsBots; BotRoster
  MaxEntries 20 -> 30; g_mirror/g_joinTriedAt/g_kitTold/g_mirrorLife/
  KilledLife/LifeKnown/SpawnTried, conflictTold, lastHealth/lastState/
  primed [24] -> [32] (indexed by session number < SessionMaxNow = 32).
  lobby_rules.cpp receive buffer 512 -> 2048 (roster 8 + 30*20 = 608).
- NOT changed (need two-PC 32 logs): 825321C8 free-client search
  (6-unrolled, r30 += 6 < 24, 4992 stride: N would run to 36), its
  session-number bound li r9,24; 823DB378 (lhz 16-bit mask, 24-word
  stack array); 823D6518/823D6C08; session max 823CE344 (823D1B54/
  823D1BDC); 82428010 (vehicle occupant, byte table 0x82C73024).

### 274 SOURCE CHANGE (Claude, 2026-10-19): plane hit / crash effects

- sub_82583310 (a projectile's explosion) writes the effect entity's
  origin +336..344 and angles +352..360, then sub_82556180(out (stack),
  entity, weapondef+8 name, 56). Event 56 = the weapon's explosion fx +
  sound; event 1 = muzzle flash. PlaneFx(x,y,z,weapon,event) does that on
  its own script_origin (g_planeFxHandle), angles -90 0 0.
- MpFixesPlaneHit: damage >= 100 -> bazooka 56; else thompson 1 (60 ms
  throttle), 30-unit jitter. Shot down: panzerschreck 56 + smokegrenade 56
  at the plane; crash (g_planeCrashAt) at the dive's end point:
  panzerschreck 56, fraggrenade 56, smokegrenade 56 (+50 z).
- Untested: whether event 1 with no tag_flash shows; client-side only.

### 273 SOURCE CHANGE (Claude, 2026-10-19): engine default, crash sound; mortar notes

- 272 TESTED: Stuka seen (loaded as 5 before the guns, found by name
  67EC1FC0). ARPL_P47_1 is a one-shot (no loop). Justin tuned live:
  ARPL_P47_2 every 5450 ms = default now.
- Crash: g_planeCrashAt[k] = shot-down + 2500 ms; "UAV:Mortar_Explo" at
  the hit, then UAV:Mortar_Explo + UAV:SHELL_EXPLO_MAINIMP (cue swap).
- Plane sounds are local to the host (his sound origin). For joiners:
  server-side 824CF5F8 (r5 0) on the plane would send the event; the
  joiner's own 82554BA0 hook would swap the cue (untested, two PCs).
- MORTAR research (Justin wants a mortar killstreak later): campaign
  Laison has it all: sp\laison\vehicles.cod v_81mm (tags tag_barrel,
  tag_turret), laison.cod INTERACTIONFILE:mortar + INTERACTSTATEFILE
  mortar_idle/load/start, Weapon_mortar shakerumble; laison fx.cod
  mortar_launch, mortar_trail, mortar_dirt, mortar_water_laison (forest
  fx.cod mortar_dirt too); sounds Mortar_Fire/Explo/Miss/Slide in
  forest.wbk + laison.wbk (both loaded beside MP maps); v_81mm_shell in
  forest/laison weapons.cod (loaded); w_arms_ei_MortarIdle anim. MP xex
  has MOD_MORTAR(_SPLASH), InteractStateMortarLoad (not Idle/Start: SP
  only), prompts "[HOLD_USE] to Use Mortar"/"Press ~r1 to fire the
  mortar" (mp global.cod), MP_GS_MortarFire(B) in mp global.wbk. Plan:
  mortar barrage killstreak on the artillery code (sub_821F70A8) with
  Mortar_* cues, more smaller rounds; optional v_81mm prop.

### 272 SOURCE CHANGE (Claude, 2026-10-19): engine + near-pass flyby

- Engine: once per UAV (on[k]) at p.sound via PlaneSound (frame path,
  heard), cue g_planeEngineCue (default ARPL_P47_1), repeat every
  g_planeEngineEvery ms (0 = once). Unknown whether ARPL_P47_1..4 loop.
  Console plane_engine <cue> <ms> (kernel_video.cpp).
- Flyby: only while p.dist < 3500 (host to plane), 9 s apart.

### 271 SOURCE CHANGE (Claude, 2026-10-19): Stuka archive before the guns

- 270 TESTED: flybys heard (cues ARPL_P47_By01..07 found: 590630.. and
  By04 655931). Overlap fixed: flyby every 7 s.
- 270 probes: stuka_flying/stuka/v_a20/v_gunturret 0; v_shermanM4A3,
  v_shermanM4A3_d1, tankshell found (Merville has them). 253 (Stuka
  loaded as 6, before the guns) showed the Stuka. Now
  MpFixesLoadStukaEarly before the gun loop (sub_8247DD38 hook);
  MpFixesLoadPlaneArchives skips it then. PlaneSpawn: side 1 gets
  stuka_flying if MpFixesStukaFound (sub_82446EA0) else v_Typhoon.
- crssrds.wbk (not loaded) has Plane_Explode, Dogfight_Plane1/2,
  P47_FlyBy_Plane1/2, ARPL_Dual_P47_By01: crash sound candidates.

### 270 SOURCE CHANGE (Claude, 2026-10-19): plane cues via an event

- 268 log: no "cues: name ARPL_*" ever: sub_82556868's name is an event
  name; sub_82554BA0 looks it up in the map's event table (sub_82490220)
  -> cue names -> sub_8212B268 (OrderedCueLookup, campaign slots 2nd
  pass). ARPL_* have no event: dropped. FLAG_FLAPPING too (Flag_Flapping
  cue -1). LEE_ENFIELD_FIRE_3RD heard because the 82554BA0 hook makes it
  weapon_kar98_weapon_fire_3rd + cue override.
- Now: 82554BA0 hook: sent "ARPL_*" or "UAV:<cue>" -> event
  weapon_kar98_weapon_fire_3rd, t_cueOverride = cue, t_eventGun null.
  Log "uav plane: sound X made as the Kar98's shot event". Default where
  = 1 (sound origin, toward the plane). Watch for "cues: name ARPL_..".
- 269: Stuka list not full (no "list full" line), lookup still 0, and
  SetModel left +540 0, +16 then 0x40000000 (= no model). plane_model
  now probes stuka_flying, stuka, v_a20, v_shermanM4A3_d1, v_gunturret,
  tankshell, v_Typhoon, v_shermanM4A3: tells if crssrds is registered.
- 269 bot sounds: sub_82555E90/F90 had only grenade entities: reload
  sounds come another way (next: SoundTimes-style trace with entity).

### 269 SOURCE CHANGE (Claude, 2026-10-19): asset list order, bot sound log

- 268 TESTED: plane_model: v_Typhoon found 6D581B30; stuka_flying direct
  lookup 0 and SetModel left +540 = the Typhoon's model (so 265's flying
  "Stuka" was an empty model). ARPL_P47_1b/2b/3b/1 played from the frame
  (logged), all mute; the rifle shot audible. mace2.wbk names: a PLANES
  group (ARPL_P47_*) beside the weapon ones: suspect group not loaded.
- sub_82474CB8 hook: extra order now weapons, P47, Stuka, vehicle,
  Kangaroo, map (std::rotate); a full list (count 32) logs
  "archives: list X full". If not full -> the Stuka model is not
  registered at all (look at crssrds archive registration).
- CampaignEventName: "bot sounds:" for sub_82555E90/F90 (animation
  sounds: reloads): entity, its player index, = me, distance, +484/+480
  vs mine, r3/r5/r6 (150 lines). Bots' reloads heard as own (12v12).

### 268 SOURCE CHANGE (Claude, 2026-10-19): plane_sound name <ALIAS>

- 266 TESTED: LEE_ENFIELD_FIRE_3RD heard from the frame (PlaneSound at
  own list entity) - so 258-264's path was fine; the P-47 aliases are the
  problem (likely streamed/long in sp\mace2\mace2.wbk). Typhoon seen for
  enemyuav. Bot reload sounds as own: also at 12v12.
- plane_sound name <ALIAS>: which = 4, any alias (kept in a static set).

### 267 SOURCE CHANGE (Claude, 2026-10-19): plane_model Stuka diagnostic

- SetModel (sub_824CE130): asset lookup sub_820D71E8 with name+6 (a
  prefix skipped); not found -> sub_8256FD68 (ent, name, bank): looks up
  sub_82446EA0(out, *(0x82BCC88C), bank (ent+480 or *(0x82BCC828)+40),
  name); needs model+32 and +32->+44->+0 non-zero, stores model at +540.
- crssrds vehicles.cod has stuka_flying (+ stuka/stuka_low/stuka_med/
  propeller/col parts) beside many tanks; forest's has only v_Typhoon.
- plane_model <name> [clear]: script_model 450 in front of the player at
  +120 z; logs entity +16/+20/+620/+968, +540 model and its +32/+44/+76,
  then 1 s later; direct lookup logged. clear: +16 &= ~0x40000000 each
  frame. kernel_video.cpp: console command (not syntax-checked here:
  audio_out.h not staged; copy of the plane_sound block).

### 266 SOURCE CHANGE (Claude, 2026-10-19): Typhoon both sides; frame fallback

- 265 TESTED: with bots, flybys played inside events at own entity
  (out words fine): unheard. Without bots, only one event (his own shot,
  event 1). Stuka (side 1) flew a full circle at 1400 on Merville twice,
  never seen; its +16 had 0x40000000 (the Typhoon's 0, 263 log).
- Sound names: strings of the banks: ARPL_P47_1..4(b), By01-03/06/07 in
  sp\mace2\mace2.wbk only (By04 in forest.wbk); none in mp\global.wbk or
  mp_merv.wbk (Flag_Flapping is). LEE_ENFIELD_FIRE_3RD in mace2+laison.
  mp xex has _mp_audio script keys plane_flyby / dist_chatter (the MP
  maps' own ambient flyby spawner): a lead if the P-47 aliases stay mute.
- Now: Typhoon for both sides (MpFixesPlaneModelLoaded = P47/Typhoon
  archive); queued sound -> frame (PlaneSound) after 300 ms unconsumed;
  plane_sound ... now -> frame at once. Log "played from the frame".
- Justin also: Colt 45 sounds wrong + no reload sound; bots' reload etc.
  heard as if from himself even far away (open item, 32 players).

### 265 SOURCE CHANGE (Claude, 2026-10-19): plane sound from the game's sound events

- 264 TESTED (Justin 17:13): circle round the whole map + height good.
  All 7 plane_sound tests silent incl. FLAG_FLAPPING at his own player
  (log: 82556868 accepted every call, out words like 002A0002). So every
  call made from the server frame (MpFixesLauncher) is unheard, whatever
  entity/alias. The only port-made sound known heard (Lee-Enfield scoped
  shot) is made from inside the weapon event hooks (sub_82556180 etc.,
  CampaignEventName) with that ctx, stack -0x400, out on that stack.
- Now: PlaneSoundChosen queues (MpFixesQueuePlaneSound: atomic ent +
  alias); PlaneSoundFromEvent at the top of CampaignEventName plays it
  (ent 0/invalid -> the event's r4). Default where = 2 (host's player).
  plane_sound shot = LEE_ENFIELD_FIRE_3RD. Log "uav plane: X played in
  event N ... at entity E (+484 ..)". Needs weapon events (bots shooting).
- 82556868 notes: r30=*(0x82BCBE04) sound world; 825556A8 takes ent +484
  (handle, checked vs 0x82BD1FC0 table in 8254B888) and +480 (tag) when
  r7=-1; command kind 7, alias at +508, list of 2048 at +8192.

### 264 SOURCE CHANGE (Claude, 2026-10-19): spawn-point circle, mean height, plane_sound

- 263 TESTED (Justin 15:54): plane back; still no plane sound (flag-style
  call on the script_origin); circle covers only part of the map; height
  grew each call (he was in noclip: ask.cz used the highest player).
- Circle: all entities 0..4095 (table 0x82BD1FC0) whose class (+548 string
  object, text +12) has "spawn": box -> centre, half the larger side
  (2500..12000); log "uav plane: N spawn points span". Height: players'
  mean z + 1400 (p.ground = mean; crash ground = p.ground - 100).
- plane_sound console (kernel_video.cpp -> MpFixesPlaneSoundTest): where
  (plane / origin / me / bot nearest) x which (flyby / engine ARPL_P47_1 /
  flag FLAG_FLAPPING), plays once now and sets the flybys' choice. Theory:
  a no-model script_origin or the own player never gets its sound events
  to the client; a bot (a client entity in the snapshot) or the modelled
  plane (but far) might. The flag test separates "where" from "alias".

### 263 SOURCE CHANGE (Claude, 2026-10-19): undo 260 lowering + 262 loop; flag-style sound

- 262 TESTED (Justin 15:19, mp_merv): plane NOT seen, bazooka SILENT, no
  plane sound. Log: +16 0x40000000 on Merville too (so not "out of the
  world"; set since 257/259, cause unknown) -> 260's rule lowered it to
  ground+300 on Merville (lost in trees). The loop: 824CF5F8 with r5 1 put
  1 at the sound origin's +888 (a sound index, not a flag): other sounds
  went silent.
- The map flag script (sub_821ED9xx, FLAG_FLAPPING at 0x82039698) calls
  table +184 (824CF5F8) with r3 handle, r4 alias string, r5 0, r6 0, r7 0
  -> sub_82556868(out, entity, alias, r6 0, r7 -1, r8 0). Same now for the
  flybys, on the sound origin (script_origin), every 3 s within 9000.
- Heights: 1400 over the highest player; Poissons 400 (260 seen low there).

### 262 SOURCE CHANGE (Claude, 2026-10-19): engine loop on the sound origin; rocket delay

- 261 TESTED (Justin 14:39): no sound; panzerschreck shot it down (3 hits
  logged at fire time, 300 each), no hit marker noticed, no explosion.
- sub_82556868 does not keep r4 (passes it to 825556A8 with r7) and makes a
  type-7 sound message; called from the server frame (we) it is never heard
  (ids returned), though the client weapon path's calls are. Heard from the
  server frame: playloopsound 824CF5F8 (FLAG_FLAPPING asked from 824CF700).
- Now: one-shots off; ARPL_P47_1 looped (824CF5F8 r5 1) on p.sound (the
  script_origin) when it is made; p.sound kept 900 toward the plane (3000+
  away: 900 + 0.6 x beyond 3000); parked with the plane.
- Rockets: hit applied after distance / 1.5 ms (RocketHit queue in
  MpFixesPlaneShots). NEXT: explosion effect at rocket hits / the crash
  (find MP playfx; or the artillery strike sub_821F70A8 at the crash point).

### 261 SOURCE CHANGE (Claude, 2026-10-19): flybys on the host's player

- 260 TESTED (Justin 14:19): plane SEEN on Poissons (the out-of-world
  lowering works); still no sound on any map.
- PlaneSound now as the port's heard Lee-Enfield shot: sub_82556868 with
  r4 = the host's EntityList entity, r7 -1, r8 0 (r8 was left from ctx
  before), every 3 s while the plane is within 6000. If heard: move to a
  directional source later (the script_origin p.sound is still made and
  moved, unused for sound now).
- Memory work agreed with Justin (clear leftovers on map change, campaign
  gun archives on demand, measure free room): first needs a Merville then
  Poissons repro on 261 (the 258 crash: GPU hung). The Kangaroo archive
  is reused, not duplicated, on that change, so "leftovers" is not proven.

### 260 SOURCE CHANGE (Claude, 2026-10-19): plane comes down when out of the world

- 259 TESTED (Justin 14:06) Poissons FRESH: no crash (so 258's crash was
  Merville's leftovers + planes; still to fix on map change). Archives 5-8
  guns, 9 Typhoon, 10 Stuka, map 11. Stuka made, circled at 1400 over the
  highest player, moving fine, NOT seen. Its +16 = 0x40000000 all along
  (Merville 256: 0). Taken as out of the world (no area): in phase 2,
  every 2 s while the bit is set, p.cz -= 250 down to p.ground + 300.
  If the bit is something else, the log shows it never clearing.

### 259 SOURCE CHANGE (Claude, 2026-10-19): UAV sound origin; no planes on Poissons

- PROCESS NOTE: re-committing a file from the SAME staged path can send
  the earlier upload (the 13:44 commit put back the old mp_fixes.cpp; the
  13:48 build ran the Poissons skip). Commit a changed file from a new
  output folder each time (outputs/<ver>b/...). Rebuilt as 0.5.259 again.

- 258 TESTED (Justin 13:39, mp_merv): hit markers seen, 8 "hit for" lines;
  sounds ARPL_P47_By01..07 started (sub_82556868 ids 0x50005...), none heard
  (plane 2500..9000 + 1400 up: past the aliases' reach).
- Poissons after Merville CRASHED: map 15 kind 3 (weapons 5-8, planes 9/10
  "still loaded"), then call through 0xCDCDCDCD in sub_82291160 <-
  82291980 <- 82291F80 <- 825A1614 (and 822C0638), 2048 MB committed.
  Before it: "The GPU is hung" (D3D's own message) = graphics memory full.
  Justin wants the plane on Poissons: archives kept there; test Poissons
  FRESH (first map). If fresh works: leftovers from Merville (Kangaroo
  archive 12, Merville's) fill it; free or skip those on change. If fresh
  hangs too: on Poissons only, a smaller Axis plane (Typhoon for both) or
  one campaign weapons archive fewer. MpFixesPlaneModelLoaded guards a
  plane without its model.
- Sound: p.sound = a script_origin (824CEAD8 "script_origin") moved every
  100 ms to the host's eyes + 900 toward the plane (moveto 0.1 s); flybys
  every 3 s on it when the plane is within 9000.

### 258 SOURCE CHANGE (Claude, 2026-10-19): UAV shots by ray, hit marker, flybys

- 257 TESTED (Justin 13:28, mp_merv): circle perfect; no "hit for" lines
  (bullets never reach G_Damage for the script_model; Typhoon's archive has
  no col. mesh anyway); no sound: playloopsound 824CF5F8's r5 is stored at
  +888 = the loop sound itself (we passed 1); its one-shot call
  sub_82556868 (out, entity, alias text, 0, -1) returned 0x100004.
- MpFixesPlaneShots (each host frame while a plane is up): per player, the
  held weapon's clip (record +164 held, def list 0x82BCBF9C, clip record
  +4*(172 + def+1468)) going down = shots; ray from record +0..8 (+60 z)
  along +204 pitch (down +) / +208 yaw vs a 380-unit ball round the enemy
  plane's +336; hit -> MpFixesPlaneHit (40, rockets 150 -> x2). Logs
  "uav plane: client N shot S (weapon) at side K's plane: D away, missed by M"
  (first 40). Host's own hit -> g_planeHitMarker 220 ms -> X drawn in
  StreakHudDraw (StreakHudActive true while it shows).
- Sound: PlaneSound = sub_82556868 on the plane every 3 s, ARPL_P47_By01..07
  in turn. kPlaneSpeed 993.

### 257 SOURCE CHANGE (Claude, 2026-10-19): UAV Build B (damage), radius, speed, sound

- 256/255 TESTED (Justin 13:01): "a perfect circle", ~80% there: wants a
  bigger radius (the whole map), 5% slower, shootable, plane sound.
- Radius: players' positions sampled every 1 s per map (bounding box);
  circle = box centre, 0.45 x larger side (2500..9000), height max(top
  seen, now) + 1400. Speed kPlaneSpeed 1045 (was 1100).
- Damage: on fly-in ent +608 = 1 (G_Damage's takedamage), health 600 (ours).
  The G_Damage hook calls MpFixesPlaneHit first: a hit on a plane entity
  is counted there (damage >= 100 counts x2) and the game's own damage is
  NOT run; the plane's own side is ignored (attacker +520 -> +4). At 0:
  rotateto pitch 55 roll 120 and moveto ahead 1800 to the ground (top seen
  - 100) in 2.5 s, phase 4, parked after 3 s; the host's UAV ends when its
  side's plane falls (g_uavUntil = 1). Logs "uav plane: side N hit for".
  UNKNOWN: whether bullets/rockets reach G_Damage for a script_model at
  all (contents/clip from col.<model>): if no "hit for" lines, look there.
- Sound: playloopsound 824CF5F8 (+0xB8; r3 handle, r4 alias string, r5
  non-zero kept at +888, refused if +888 set) with ARPL_P47_1 at spawn;
  the loop stays on the entity (reused, parked under the map).
- NOTE: the edits to edit/mp_fixes.cpp for this build were partly found
  already made in the scratchpad (from an earlier attempt of this turn);
  reviewed line by line before use.

### 256 SOURCE CHANGE (Claude, 2026-10-19): plane position log

- 255 TESTED (Justin 12:53, mp_guns): loaded fine (Typhoon 9, Stuka 10, map
  11). He was side 1: Stuka made at -9504 4412 2099, circle over 53 -47 at
  1499 r 1547; all phases logged; he saw NO plane (253's unmoved one was
  visible at its spawn point). So the moves do something now: where to?
- "uav plane: side N phase P at x y z (angles; +16 +20 +620), aiming at"
  every 2 s while up. NEXT: if the origin never changes the mover is not
  run for it (check +16 flags / trajectory +20 vs a map script_model); if
  it goes elsewhere fix the point; if it is right, it is fog/size.

### 255 SOURCE CHANGE (Claude, 2026-10-19): map-load freeze; Typhoon

- 254 (Justin 12:18): froze loading mp_merv. Log: archives 5 P-47, 6 Stuka,
  then campaign weapons 7 mace2, 8 fuelplnt loaded, 9 forest "loading" and
  never "loaded"; fps 0 from 50 s, frames still counting (logic stuck in
  the load). 253 (same order) had loaded all four + the map (kind 3 as 11):
  so we are at the edge of the map block (big_memory 495 MB) on Merville.
  The "read of heap memory never handed out" x16 lines are in both (normal).
- FIX: Allies' plane = v_Typhoon from sp\forest\vehicles.cod (2.6 MB, only
  the Typhoon; tag_body/tag_prop) instead of chambois\veh.cod (13 MB). Plane
  archives now load in MpFixesLoadPlaneArchives, at the end of
  sub_8247DD38's campaign-guns loop (or right away when campaign_guns 0).
  Stuka (crssrds, 10.7 MB) kept. If Merville still hangs: drop the Stuka
  file too (then both sides Typhoon, or find a smaller Stuka source).

### 254 SOURCE CHANGE (Claude, 2026-10-19): UAV plane fixes

- 253 TESTED (Justin 11:56, mp_merv): Stuka seen, far off the map's side,
  never moved. Log: made at 9116 -6725 2002, "flying in over 0 -104 at
  height 1402" (players' +336 read ~0), all phases logged, no motion.
- 824C7FC0 is NOT moveto: it calls 824C5AB0 (angles +352, trajectory +60
  = a rotation). moveto is 824C8288 (+0xA38: vec r4 absolute, f1-f3; ->
  824C5C30: origin +336, trajectory +20; skipped if the point equals the
  vector at 0x82A93408). 824C84A0 = relative move from +336. 824C80A8
  rotateto (AngleSubtract vs +352) is right.
- Players' position: ps (+512) +0..+8 (as the set-origin call 8255E118).
- Sides: Justin (American) had team +4 = 2: side 2 = Allies -> P-47.
- Screenshot 115526: plane visible in the fog, small: fine once overhead.

### 253 SOURCE CHANGE (Claude, 2026-10-19): UAV plane, Build A (visible, no damage)

- Justin (00:05): UAV = 3 kills, plane circles 30 s then flies away, must be
  shootable (gunfire or rockets) to end the UAV early, "just like other
  Call of Duty UAV killstreaks". Agreed plan: Build A (this) visible plane;
  Build B damage/shoot-down, bots. Picked P-47 (Allies) + Stuka (Axis).
- EXISTING (found, not new): killstreak ladder in mp_fixes.cpp (G_Damage
  hook sub_825862F8 counts kills in a row; UAV = enemy_radar 0x82B2CF90+12
  = 1 on the host's own machine, killstreak_uav_kills 3, uav_seconds 30;
  artillery; "Typhoon" run = strikes only, no model). Streak arrays and
  ClientOf were 24: now 32 / MpCapClients.
- PLANE MODELS in the campaign archives (strings of the .cod files): P-47
  v_rep_P47 (saint_lo\vehicles.cod, chambois\veh.cod 13 MB), Stuka
  stuka_flying (crssrds\vehicles.cod 10 MB, also the A-20 v_a20), Typhoon
  v_Typhoon (blkbrn\vehicles\planes.cod 2.5 MB, forest\vehicles.cod),
  Fw 190 in 4 parts (saint_lo). No C-47 mesh (texture only). MP global
  sounds have ARPL_P47_1..4(b), ARPL_P47_By01..07.
- MP SCRIPT TABLE (0x82A98030 = *(0x82BCBF64), filled at run time): pairs
  list at 0x825FF090 (244) + slot stores (sub_824D1380, 824D0C68,
  824D3338): scratchpad mp_table.json, 996 offsets. Offsets differ from
  the campaign table after the low ones (+0x264 spawn 824CEBE8, +0x268
  spawn() 824CEAD8, +0x5A0 is NOT delete). Movers: all call sub_824BB360
  (mover class check): moveto 824C7FC0 (+0xA5C: handle r3, vec r4, f1 time,
  f2 acc, f3 dec), rotateto 824C80A8 (+0xA4C, angles in degrees),
  rotatevelocity 824C81C8 (+0xA48), 824C5900/59D8 (+0xA64/A60: stop?).
  Delete native not identified: the plane is parked at z -60000 and reused.
- CODE (mp_fixes.cpp): ExtraArchive g_p47File / g_stukaFile loaded in
  MpFixesLoadVehicleArchive each map (Settings uavPlane, ini uav_plane),
  added to every archive list (sub_82474CB8 extra[+3], [+4]).
  MpFixesUavPlane(base, caller, seconds) at the UAV call (side from caller
  +520 -> +4: 1 Allies, 2 Axis; centre = players' mean, radius 0.7 x
  spread clamped 1500..4000, height top player + 1400) queues an ask;
  MpFixesPlaneFrame (MpFixesLauncher, host frame): spawns script_model via
  824CEAD8, SetModel 824CE130, fly-in moveto 4 s, circle steps every 400 ms
  (moveto + rotateto, 1100 u/s, bank 25), off along the tangent 6 s, parked.
  kPlaneYawOffset 0 = GUESS (model nose on +X): fix from a screenshot.
  Logs "uav plane: ...". Syntax: same errors as the original with stubs
  (g++ -fsyntax-only, simde from GitHub); checker proven to reach the code.
- UNTESTED. Risks: archives too big for memory (uav_plane = 0 turns off),
  model axis/scale, mover natives on a script_model mid-game, the plane
  visible only when its archive is in the joiner's lists too.
- 253 BUILT OK (00:30, 2nd try: "far" is a Windows macro, renamed). Not
  played yet.
- BUILD B FINDINGS (research, no code yet): both plane models carry a
  collision mesh (col.v_rep_P47, col.stuka_flying) and tag_prop / tag_body.
  G_Damage sub_825862F8 args: r3 target, r4 inflictor, r5 attacker, r6 dir,
  r7 point, r8 damage (9999 = force), r9 flags, r10 means of death; it
  returns at once unless target +608 (takedamage) != 0. Plan: on spawn set
  plane +608 = 1 and our own health (e.g. 600; rockets x4); in the hook, if
  r3 is a plane entity, subtract r8 ourselves and do NOT call the original;
  at 0: UAV off (g_uavUntil) when it is the host's side, notice "UAV SHOT
  DOWN by <attacker>", crash: rotateto nose -60 and moveto down 2.5 s, then
  park. First thing to log in B: whether bullets reach G_Damage for it at
  all (if not: the entity needs contents/link from its col mesh).
- NEXT (Build B): damage. The G_Damage hook sees target r3: if it is the
  plane's entity, count damage (needs the plane to have collision/contents
  and takedamage: compare a spawned script_model's fields with a
  destructible's), then fall/burn (moveto down + effect), UAV off early,
  notice "UAV SHOT DOWN by X". Sound: playloopsound candidate +0xB8
  824CF5F8 (reads +888) with ARPL_P47_1. Bots: shoot at the plane, call
  UAVs.

### 252 SOURCE CHANGE (Claude, 2026-10-18): death freeze; HUD-only draw list

- 251 TESTED (Justin 23:42, 32 in play): "I see all the ranks". Log:
  "overhead icons: players 24-31 of HUD object 829E4410 at AA3D0000".
- Death freeze (couple of seconds on death): bots.cpp HostDeathCapture
  (0.5.226/228 debug) took screenshots at frames 2/6/12/20/35 of the first
  3 deaths and Render::LogNextFrame at frame 7 of death 1 ("397 of 2076
  draws pictured", ~19k log lines). Now only with COD3_DEATH_CAPTURE=1;
  SetPlayerDown (death blur) unchanged.
- Binoculars (bad run 251, x2 zoom): 249's DumpDraws listed the FIRST 700
  draws = world only (604 drawn, 96 "no target" 1-index points with a
  16x16 scissor, vs 10e10ea9 ps 5d27834e). The mask is a late flat draw.
  DumpDraws(why, -N): only draws without Z enable (depthControl bit 1),
  over the next 8000 draws; mp_fixes asks -300 at each zoom level. 249's
  good-run list is lost (log overwritten) and was world-only anyway. NEXT:
  compare a good and a bad run's flat lists (Justin says good/bad).
- Scoreboard: 28 bots + host = 29; Justin counted 15 + 14 = everyone.
  Told him. 32 rows would only matter with 32 humans/bots (rows cap 24 per
  board, 248).
- OPEN: bots' reload/foley sounds heard as if from his own character with
  32 (asked whether it happens at 24). Suspect sound emitter/entity lookup
  for slots past 24 falling back to the listener; not found yet.

### 251 SOURCE CHANGE (Claude, 2026-10-18): overhead icon table past 24

- 250 played by Justin (23:26, mp_hosta, 32 in play, bots added; log clean,
  only the usual 0x406D1388 thread-name notice). He did not notice whether
  every teammate had an icon: still open, ask again on 251.
- HOW FOUND: 250 cap-probe.csv joined with cap-probe-sites.csv minus
  cap32-sites.csv. 824F97C8 (824F9410, cmpwi r17,96, range 4..96) and
  82508130 (82507D28, cmpwi r24,96): loops r17/r24 = 0,4..92 over
  EntityList[1..24] (lwz 4(EntityList+r)). 824F9410 (per HUD object r14,
  +4 enabled, +8 local view) writes a 12-byte entry per slot at r14+76
  (+0 flag, +1 byte, +4 alpha float, +8 icon kind): kind = entity +1928
  (rank) for teammates (<= 4), 3 talking? (823EA1E0), 4 = down/medic (+1924
  == 4, alpha fades over 5000 ms from +136), 7 = enemy spotted (+1924 3,
  flags +240 bit, 15000 ms). Distance fade r26/r21. 82507D28 draws each
  flagged entry at the head tag ("BIP01 HEAD", 0x8206D414; "_point"
  0x82065444) through widget obj+12+8*kind. Table 24 x 12 = +76..+364; the
  object has fields right after (+372 byte read by 82506C08), so no room.
- FIX: cap32 kinds SIDE12W (824F97C4 addi r27,r27,12) / SIDE12R (8250812C
  addi r29,r29,12): when the index reaches 96 (slot 24) and MpCapClients >
  24, the entry pointer jumps to MpHudSide(base, obj) (+1 for the reader's
  r29 = entry+1): mp_fixes.cpp, 16 objects max, 8 x 12 (+32) bytes each,
  AllocatePhysical, log "overhead icons: players 24-31 of HUD object".
  Both loop ends N4. Dry run of patch_cap32 then patch_cap_probe on a copy
  of the generated C (probe lines stripped): 227 sites in 143 functions,
  910 probe sites, OK. Helper syntax-checked standalone.
- Classified NOT player (record in compare review): 82276D80 (vector<T24>
  size, divw by 24), 823231C0/8231BBF0/820E0338 (A..Z case fold, cmplwi 25),
  8212B5D0/8227BCF8/8227C160/822C59E8 (24-byte memcpy/strncmp lengths),
  82171288 (li r6,24 arg), 824ADA88/82553780 (li r4,25 arg), 82400810
  (entity +84 state switch 5..24), 8248D1F0 (2-word loop 16..24),
  822BA360/822BED10 (type 22/23 compare). 823EFCA8 voice: harmless (250).
- OPEN, next: 8242808C in 82428010 (cmpwi r29,24, 1.1M hits): finds a
  vehicle's occupant (+624 -> +524 -> +484 handle) in EntityList[1..24];
  index r29 reads a byte table at 0x82C73024 (24 bytes?). Players past 24
  are "not found" there. Check the table's size/users before N.
- Study tools this session: scratchpad xexdump.py (XEX2 basic compression,
  retail AES key via python cryptography) -> mp.bin image at 0x82000000.

### 250 SOURCE CHANGE (Claude, 2026-10-18): friendly markers + compass for 32

- Justin (23:03, 249, 28 bots): "some of the added players don't have a
  rank above their head, hard to tell they are friendly". Also: binoculars
  worked this run (both draw lists, no zoom and x2, are in that log:
  "render: draw list" - they are a GOOD-run baseline; the bad state is
  intermittent). bot_add 32 gave 28 bots (32 total incl. host + locals).
- HOW FOUND: joined the 249 cap-probe.csv with cap-probe-sites.csv minus
  cap32-sites.csv (hot sites still compared with 24). 8250689C in
  82506540 (cmpwi r25,24, hits 244752, range 1..24): loop over
  EntityList[1..24], same team as the local player (r24), alive (+0x784),
  writes a 36-byte entry at obj + 0x188 + 36*idx, idx = (record -
  MpPlayerRecords) / 2992 (82506540's base already relocated by
  patch_player_records, so idx is 0..31): +0x188 time, +0x18C/0x190/0x194
  position (entity +0x70/+0x80), +0x198 flags (bit 0x20). The drawer
  824E2CB0 walks obj+0x18C with li r26,0x20 (32 entries), reads +0x198 bit
  0x20: room for 32 already. The obj is one of 4 local HUD objects of
  0x1B48 (init 82506E78). So only the writer loop: cap32 N.
- 82505CE8 (called right after it from 82506C08): compass player list,
  loop unrolled by 8 (r23 += 8, cmpwi r23,24 at 82506384), table obj+0x5F8
  memset 0x500 = 32 x 0x28. cap32 N too (32 = four passes).
- Both UNTESTED in a match. Expected: every teammate past slot 24 gets the
  rank icon; compass shows them.
- Other hot 24-sites still open (from the same join, by hits): 82276D80
  li r10,24 (51M), 823231C0 cmplwi 25 (13.5M), 8231BBF0, 823EFCA8 (voice,
  harmless), 8212B5D0, 8227BCF8/8227C160 (li r5,24 memsets?), 822BA360,
  824F9410/82507D28 (cmpwi 96 = 4x24), 82400810. Classify next.

### 249 SOURCE CHANGE (Claude, 2026-10-18): brightness; binocular draw lists

- 248 TESTED (Justin 22:49, after a restart with max_players 32, 28 bots):
  scoreboard 15 + 14 rows. Note: max_players only applies at start (his
  first try was in the running game: 20 bots, 24 cap).
- Binoculars still see-through at the sides at the x2 zoom (the port's
  zoom: weapon +1600 FOV x3, mp_fixes view(2)). 248 log: "binoculars down
  after 484 frames; draws left out meanwhile: no target x34848" (72 a
  frame; normal play ~50 a frame "no target"). DOF-pass-on did not help.
- render_commands.cpp: DumpDraws(why, n) lists the next n draws (vs, ps,
  prim, count, colour mask, depth control, stencil, blend0, scissor, drawn
  or skip reason). PrepareDraw is now a wrapper over PrepareDrawInner.
  mp_fixes calls it 60 frames into each zoom level, once per level per run
  (700 draws). NEXT: diff the "no zoom" and "x2 zoom" lists: the side
  bars' draw (likely a 2D quad, maybe "no target" or a scissor) present in
  one, missing/skipped in the other.
- Brightness: Settings brightness (50..150, ini "brightness", F11 Graphics
  slider). render_d3d11.cpp GammaTable(): the display LUT (title's, or
  identity) entries raised to 100/brightness per channel; re-uploaded when
  it changes. Justin asked for the campaign's slider in the MP options
  menu: not done (menu data); F11 instead.

### 248 SOURCE CHANGE (Claude, 2026-10-18): scoreboard past 24

- 823A0180 (scoreboard, rows +0x4C, 24 x 12 B, count r17) and 8239EC38
  (second scoreboard, rows +0x88, 24 x 20 B, team totals +0x3D4..0x3E8)
  each walked EntityList[1..24] (li r23,24 at 823A023C; li r20,24 at
  8239ECE4). Now cap32 kind N (MpCapClients).
- Rows stay 24 (arrays and the widget are sized 24): new cap32 kinds in
  scripts/patch_recomp.py, all no-ops at 24 players:
  ROWFULL0 (823A024C "cmplwi cr6,r28,0": entity counts as empty once r17
  >= 24), ROWFULLNE (8239EE40 "cmpw cr6,r28,r10": not my team) and
  ROWFULLNZ (8239EE4C, 2nd "cmplwi cr6,r10,0": team-only flag set), so in
  8239EC38 the team totals still add every player but no row 25 is
  written. 8239F030 (cmpwi r17,24) clears rows up to 24: unchanged.
- Dry run of the patcher on the two functions OK (7 sites).
- Binoculars: Justin, 22:13, with texture_replace = 0: black sides came
  and went ("they were [fine] ... then I seen them mess up again"), so NOT
  the HD textures. render_commands.cpp: SetBinocularsUp(bool) (called each
  frame from MpFixesBinoculars: g_heldSpotter || own binoculars out) draws
  the depth of field pass 56ccd878 while up (as SetPlayerDown does) and
  prints "render: binoculars down after N frames; draws left out
  meanwhile: <reason> xN" (first 40). Read that next: if "empty scissor"
  or another reason shows, that is the side bars.
- bots.cpp BigTest: the Back press every 25 s (scoreboard toggle) only with
  COD3_BIG_SCOREBOARD=1 (Justin: it kept popping up while he watched).
- CORRECTION: +0x3C6A0 of *(0x82BC CB00) (stored 24 at 823D1B54 /
  823D1BDC) is a session STATE code (other stores 2, 4, 9; compares
  cmpwi 24 in 823BB888, 823C8420, 823C85F0, 823CA7C0, 823CB450), NOT a
  max-players field. Do not cap32 it.
- 247 BIG-TEST-32-MAPS RESULT: all five maps finished clean, 28 bots +
  host the whole 150 s each, no crash (only the usual 0x406D1388 thread-name
  notice at start), median fps rimling 47, mp_farm 43, ordnance 43,
  mp_merv 41, longyard 42; 40-50 killfeed lines a map, coloured by the
  rule. Solo 32 is solid on five maps.
- Justin's ini: texture_replace set back to 1 (22:25) after the test.
- At the time of this build BIG-TEST-32-MAPS (247; rimling mp_farm
  ordnance mp_merv longyard) was running; read big-test-32-<map>.log.

### 247 SOURCE CHANGE (Claude, 2026-10-18): 32 players as a setting

- Justin: "how close are we" for more bots. Answer given: solo 32 works
  (242 BIG-TEST-32 full match, 37 fps); needed: a setting (this build), the
  scoreboard's 24 rows (823A0180, menu widget rows: real work), more maps;
  two-PC 32 needs testing together (network loops left at 24: 823ED658,
  823D6518/823D6C08/823DB378, 825321C8; session max-players field 24 at
  823CE344 / +0x3C6A0 stored by 823D1B54/823D1BDC: joining past 24 may be
  refused).
- MpCapClients (mp_fixes.cpp, static init) = COD3_PLAYERS if set, else
  CoD3Recomp.ini "max_players" (GetModuleFileNameW dir) >= 32 -> 32. The
  setting is Settings::Values::maxPlayers (load/save "max_players"), F11
  Multiplayer checkbox "32 players (test, restart the game)". Fixed for
  the run (generated code reads MpCapClients everywhere).
- Game folder (not source): MAKE-FRIEND-UPDATE.ps1 / INSTALL-FRIEND-UPDATE
  .ps1 / UPDATE-README.txt now carry the co-op bats, COOP-README.txt and the
  newest ..\coop-test-*-for-friend.zip (Justin asked, 21:47).
- Binocular ADS: left/right not fully black (only the centre fine). Test
  set up: Justin's ini texture_replace = 0 (Claude changed it 21:48; SET IT
  BACK TO 1 after). If the sides go black with it off, the upscaled
  binocular overlay texture in textures\replace is the cause.

### 246 SOURCE CHANGE (Claude, 2026-10-18): killfeed colours set at sub_82529D78

- 0.5.245 TESTED (Justin, BIG-TEST): smokes thrown ("throws smoke ... 2
  left", 84 smoke lines in the rimling log; 0.5.244 fix confirmed). Kill
  feed still "green killed green" though the log said victim drawn red.
- sub_82456920 has two colour paths: team game (0x82B913E4 +0x14 set):
  sub_8244DB20 per name; otherwise sub_8247E2F0(entity) ("this machine's
  player": green (k,1,k) else red (1,k,k), k = 0.25 at 0x820631A4). Bots
  are this machine's (sub_823D9B50 wrappers), so all green. Both colours
  end at sub_82529D78 (HUD feed add; r3 1, r4 killer text, r5 killer
  colour r1+0x70, r6 victim text, r7 victim colour r1+0x60, r8 means).
  0.5.246 paints r7 red and r5 by the attacker's side vs the host's there
  (g_feedLine set by the 82456920 wrapper). The 8244DB20 wrapper is gone.
- Justin: audio "delayed or strange" in the 245 rimling BIG-TEST. Log: 0
  dropped, xma 0 refused, audio frames keep real time; fps 28-33 (was ~42
  on 242's 24 rimling) with smokes now thrown (gpu 26-29 ms). Asked him if
  mp_farm sounded fine and whether it is only in heavy smoke.
- Rimling LOADED on 0.5.245 (the 243 load crash did not come back).
- Justin (21:45, on 0.5.245): smokes seen; binocular ADS blackout now FINE
  (no change made for it; reported fixed). Closed unless it comes back.

### 245 SOURCE CHANGE (Claude, 2026-10-18): killfeed victim always red (Justin's choice 1)

- Justin chose: the victim's name always red, the killer in his team's
  colour. sub_8244DB20 has exactly two callers, both in sub_82456920
  (0x82456B68 victim r25, then 0x82456C3C attacker r27). The 82456920
  wrapper sets g_feedVictimColour = 3 - host side (host entity +520 -> +4,
  only if 1/2); the 8244DB20 wrapper replaces r3 with it on the first call
  whose team is 1/2, then clears it. Covers the host's own death (his team
  object is the local one, so writing the team word could not work).
  Log: "bots: killfeed line: ... (victim drawn red)".
- 0.5.244 not yet tested when 245 was written (smoke rounds, 1K pool).

### 244 SOURCE CHANGE (Claude, 2026-10-18): bot smoke rounds; 1K pool only at 24; 243 load crash

- 0.5.243 TESTED (Justin ran BIG-TEST.bat): mp_farm finished, 0 apsMemory.
  Rimling CRASHED at 36 s uptime while loading (after devmap from mp_merv):
  "Guest fault: read at guest address 0x3B000000"; frames (symbolized with
  the 0.5.243 exe/pdb): 821C08B0 <- 821C1758 <- 821C1AA0 <- 823FB698 <-
  823FBA08 <- Mmio::CallIndirect <- 821E4CA8 <- 821D8CA8 <- coroutine
  Start (a script thread reading a session message). New path; not seen
  before. Only 243 change active there: all 9 particle pools x2 at 24 (~11
  MB more from the game allocator 820FB738). 0.5.244 doubles only the 1 KB
  pool at 24 (the one that ran dry), all 9 at 32. If the crash comes back
  with that, it is not the pools.
- Smoke count look (243): slot 5 [24 smokegrenade owned 0 clip#20=0]
  [25 smokegrenade_axis owned 1 clip#21=0] [10 stielhandgranate owned 1
  clip#10=2]: the smoke is given (2664) but its rounds stay 0 (the 2864
  count call does not set them). Kit extras now write record +4*(172 +
  def+1468) = count when 0, for the smoke and the frag, if owned. Log
  "bots: kit extras: <name> rounds set to N".
- Killfeed (243 log, mp_farm, 40 lines): bot sides are all 1/2, host side
  1; colours follow sub_8244DB20 (different team from the local player =
  red). Justin: "it should be red when you die not green" - ASKED which
  rule he wants (victim always red? his own death red?). No change yet.
- Justin: binoculars ADS overlay not fully black (edges). Not looked at
  yet (cf. the Lee-Enfield scope blackout history in NEXT-SESSION).

### 243 SOURCE CHANGE (Claude, 2026-10-18): particle pools x2 always; killfeed side log/fix; smoke count look

- 0.5.242 TESTED (Justin ran BIG-TEST-32 then BIG-TEST; he asked Claude
  NOT to drive his computer any more: ask him to run tests): 32 rimling
  "finished", 28 bots / 29 session players, 0 apsMemory, roster trap 0,
  36.9 fps, effects most 456 / nodes most 965. 24 rimling finished but 46x
  apsMemory (1K pool, requests 0xE0/0x180/0x40); 24 mp_farm finished, 0.
  "call: through a pointer to ..." lines at start-up (8280B610, 827B4110,
  8279B190, 8278BF90, 827009x0) are in every run (known, handled); one
  0x40xx from sub_823C9F00 <- sub_82403840 (per-bot session update, voice
  object) at 24 too: handled, not a crash.
- sub_8210ACA0 doubling now unconditional (24 and 32).
- Justin: bot victims no longer red in the killfeed. Colour = sub_8244DB20
  (team r3 = entity +0x208 -> +4; 1/2 compared with the CurrentClient+1
  entity's; else white). sub_82456920 (r3 victim entity, r4 attacker)
  wrapper now logs "bots: killfeed line: victim .. side .., attacker ..
  side .., host side .." (40) and, for a bot victim side not 1/2, writes
  his session player's +604 side for the call and restores it.
- Smokes: no "throws smoke" in any 242 log; "has no smoke left" 65-97 per
  run while loadouts give 1-2 smokes (mp_fixes kit extras: OnWeaponFor
  2664 smokegrenade/_axis + 2864 count). BotNadeAmmo(smoke) = 0 always;
  frags work ("frag button found 2", 56 throws). 0.5.243 logs "bots: smoke
  count look, slot N: [w name owned clip#i=n spare#j=m] | slots ..." 3x.
  NEXT: read it; fix BotNadeAmmo's smoke path (name/def/ammo index).

### 242 SOURCE CHANGE (Claude, 2026-10-18): particle memory pools x2 at 32; damage-from table sink

- 0.5.241 TESTED (Claude launched BIG-TEST-32.bat then BIG-TEST.bat from
  Explorer via computer use, click tier: double-click the .bat):
  - 24 regression: big-test-rimling.log + big-test-mp_farm.log, version
    0.5.241, both "finished", 20 bots / 21 session players all along, no
    apsMemory messages, no fault. 24.2 fps rimling, 50.4 fps mp_farm.
  - 32: big-test-32-rimling.log: 28 bots / 29 session players 0..120 s,
    roster trap 0 faults, player-object-pool 36 x 1392 OK. CRASH ~130 s:
    "Guest fault: read at guest address 0x22DC3341", host frames symbolized
    with llvm-symbolizer --obj=CoD3MP.exe (stage CoD3MP.exe + .pdb, absolute
    address 0x140000000+off, --no-inlines): 82108EA8 <- 820FBE20 (effect
    instance's child list +0x54/+0x5A) <- 8243B878 (spawn effect) <-
    82440118 <- 82442840 <- 82552BE8 <- 82554BA0 (sound/cue event) <-
    825555C0 <- 8252E2E0 <- 82557D60. Before it, 22x "native error enqueue
    ... [apsMemory : pool(size=%d, count=%d) is empty] args 400 200 ..."
    (pool +0x14 size 1024, +0 count 512).
- Particle memory: sub_82430A58 (particle start-up, after 82109F38..F68 +
  8210ACB8) adds 9 pools via sub_8210ACA0(count r3, size r4) -> 8210AB30 ->
  8210A378 into the manager *(0x82A883F0); 512 x 16/32/128/256/512/1K/2K/4K/
  8K. sub_8242EEE8 adds 13 more (500x64 .. 15x16K). Best-fit alloc
  sub_8210A638 -> sub_8210A4C8 (pool empty -> the apsMemory message, null).
  0.5.242: wrapper doubles count when MpCapClients > 24 (~8 MB + ~2.6 MB).
  Log "effects: memory pool of N-byte blocks, M of them".
- Damage table 82580B40 (G_Damage-ish): victim record r9, loop over
  EntityList[1..24] for the attacker (cmpwi r10,24), not found -> li r10,-1,
  then record + 4*(337 + index) += damage: index -1 = record +1344 (another
  field). cap32 SINK337 at 82580D84 (addi r11,r10,337): with N > 24 and
  index < 0, r11 = (MpDamageSink - r9) >> 2 so the rlwinm/lwzx/stwx reach a
  64-byte AllocatePhysical sink. Readers of +1348 still see 24 only (credit
  from 25-32 not kept).
- 823EFCA8 (24-loops, stack arrays) is the VOICE relevance per local player
  (session +0x139D0 voice object, 823B8088): players 24+ just aren't voice
  candidates. Harmless for bots; humans take low slots. Left at 24.
- Scoreboard 823A0180: rows at object +0x4C, memset 0x120 (24 x 12), loop
  li r23,24 over EntityList[1..24]; list widget at object +0x338 (vtable,
  +0xF0 row items; sub_824E6240/824E6520 fill cells). 32 needs the widget/
  menu to have 32 rows and room after +0x16C: NOT done.
- patch_recomp.py: cap32 declarations are now added line by line when
  missing (a file patched by 0.5.240/241 already had the old block, so a new
  helper like MpDamageSink would not have been declared). Dry run on
  ppc_recomp.90.cpp: SINK337 patched once, idempotent.
- NEXT: build 0.5.242, rerun BIG-TEST-32.bat (expect "finished", no
  apsMemory), then BIG-TEST.bat; then scoreboard; then a two-PC 32 test
  (both PCs COD3_PLAYERS=32).

### 241 SOURCE CHANGE (Claude, 2026-10-18): 32-test round 2: per-player object pool, roster name getters

- 0.5.240 BIG-TEST-32 (rimling, COD3_PLAYERS=32, 28 bots + host): full 150 s,
  ~40 fps, 29 session players, bots in slots 24-31 spawn/die/respawn
  (death drops for slots 24-31 in the log). Log: build-logs/
  big-test-32-240-rimling.log (summary lines only kept in HANDOFF).
- Problems seen: (1) "call: through a pointer to 0x40xx" from sub_823C9F00
  (voice object at session+0x139D0 read as 0 -> guest address 0 held entity
  handles 0x4005..0x401C): sub_82427170 hands out per-entity objects (entity
  +504) from a pool of 24 at 0x82607890 (1392 bytes each; +0x8280 pointers,
  +0x82E0 indices, +0x8340 objects base (static 0x82607890), +0x8344
  count); probe 824271A8 saw the count reach 24. Fix: MpPool504 (mp_fixes):
  with 32, a block of 36 objects + 36 pointers + 36 indices, the base word
  pointed at it on first use; cap32 kinds P504 (the 14 addi -32128/-32032
  sites in 820D5BD0 free, 820D64E8 reset, 820D6580 fill, 82422390,
  82426A48, 82427170 alloc) and P504d (fill's index stores +96..+116 -> +48),
  N for the fill bound 820D660C (6 at a time -> 36) and alloc 824271A8.
  (2) roster trap: inline roster entry 1 +104 (names) read via 821DF2E0 /
  820B00B8 / 8254D098 / 8254D180 (string helpers) and entry 0 +604 by
  823D82A0. Name/team getters not redirected: 8239EF3C, 823A03B4, 823D9054
  (addi +4216 = roster +104) and 823D82B4 (+4716 = roster +604): R4216/R4716.
- CheckPlayerStorage / CheckSessionPlayerStorage count Active() slots.
- Still open: scoreboard shows 24 rows (823A0180 rows at +76, 24 x 12);
  823EFCA8 relevance loops at 24; second 24-pool at 0x825FFBE0+0x327C0
  (820D6710, only 0..5 used); 82580B40 damage table.

### 240 SOURCE CHANGE (Claude, 2026-10-18): runtime player count (24 / 32), 189 cap32 sites, roster trap

PROBE RESULT (0.5.239, BIG-TEST rimling + mp_farm, 20 bots, 21240 server
frames, devmap HAS a session: 21 session players): 357 of 910 sites ran.
Patterns: player loops see 1..24 every frame (e.g. 823F01D8 375/frame,
823E5FC0 71/frame); "nobody" tests see 0 and 24 (local places +22929);
EntityList walks 4..96; 30800/30932 "registry of 24" (82435810 etc.) and
switch/clamp compares are unrelated. Files: build-logs/cap-probe-239-*.csv.

- MpCapClients (mp_fixes.cpp): 24, or 32 with COD3_PLAYERS=32. Read by the
  generated code and PlayerCapacity::Active()/MaxBotsNow().
- scripts/patch_recomp.py patch_cap32 (runs before patch_cap_probe; the
  probe now skips cap32 note lines): docs/player-limit/cap32-sites.csv,
  189 sites / 128 functions, located like the probe (asm, occurrence,
  total). Kinds: N (24), N-1, N+1 (25 = "special" local place / ranged
  max), N4 (EntityList byte 96), set4992 (0x1D400 client-list ends ->
  N*4992), delta2992 (82594A38 record-init end), guard100 (EntityList +100
  level copy stored only while N <= 24), R<off> (redirect an inline roster
  base through MpSessionPlayerAddress), Rend22928 (MpSessionRosterEnd),
  groups6 (6-unrolled roster loops: (N+5)/6 groups). At N=24 every line
  computes the original value.
- Covered: server clients (825304DC/82531758/825350B4 ends, 825314EC,
  825315F4 countdowns, 82530294, join 8240480C/824048E4); game: level
  maxclients 82594A08 (level 0x82C281D0 +148), record init 82594A38, record
  countdown 82595128, player entity spawn 8249B1FC, 824BADC8, EntityList
  clear 824B1BE0, EntityList walks 825627DC/82562944/8258ADA0, 824BEADC,
  8248FEB4; session: roster loops, local-place "nobody" tests and stores
  (24 -> N, 25 -> N+1), 61 ranged read/write maxima (821AF1E0 read,
  821AFF20 write, li r6,24/25), party-list inits (11 bytes of 24).
- Deliberately LEFT at 24 (players 24+ are skipped there, no overflow):
  823EFCA8 (per-frame relevance: stack arrays 24 x 4 and 24 x 12 at
  r1+112/+212), scoreboard 823A0180/8239EC38 (24 rows of 12 at +76),
  82580B40 (per-entity damage table at entity+1348, 24 words; fallback
  index -1 -> slot 336), 824F9410 / 82507D28 (EntityList walks writing
  12-byte entries), 823ED658 (network), 823D6518/823D6C08/823DB378
  (per-player message loops), 825321C8 (6-unrolled server scan),
  823D1D60 etc. (lobby arrays strides 160/200/80/48), 823CE344 /
  +0x3C6A0 (session max-players field, stored 24 by 823D1B54/823D1BDC).
- ROSTER MISSES FOUND (inline roster read after 0.5.213's relocation;
  fixed for 24 too): 823DD0E8 (find by xuid, base +4248), 823DC678 (count,
  base +4900, 6-unrolled), 823DC918 (clear +144, base +4256), 823EC678
  (backwards walk from object+22928). Session storage now 36 entries
  (session_player_storage.h) so the 6-unrolled walks stay inside.
- Roster trap (COD3_ROSTER_TRAP=1): pages wholly inside the inline roster
  are PAGE_NOACCESS once the roster moves; Kernel::FaultHook (new, first in
  GuestFaultHandler) logs "roster trap: sub_X read/WROTE ... entry n +f",
  opens the page, single-steps (EFlags TF) and closes it again; summary
  every 30 s. Finds the remaining missed roster accesses at run time.
- Host: bots.cpp arrays sized StorageClients (32), loops bounded by
  ActiveSlots(); SessionMax -> SessionMaxNow(); bot names 1..N; BotsAsk
  clamps to N-4. mp_fixes: FindFree/MpSessionPlayerAddress use Active();
  spare server slots are not marked 0xA7 when 32 are in play.
- BIG-TEST-32.bat: COD3_PLAYERS=32, 28 bots, roster trap, rimling;
  big-test-32-<map>.log, cap-probe-32-<map>.csv. BIG-TEST.bat takes
  COD3_BIG_TAG for the names.
- Syntax: bots.cpp / mp_fixes.cpp stub-only errors; patch dry run on the
  0.5.238 generated copy: cap32 189 + probe 910, idempotent, 14 changed
  files clang-clean.

### 239 SOURCE CHANGE (Claude, 2026-10-18): player-cap probe (910 sites) + BIG-TEST (no play change)

- Goal (Justin: "keep working until this is figured out ... big games"):
  sort the ~900 retail instructions that compare with / load 23/24/25 into
  "player limit, must become 32" vs unrelated, by RUNTIME evidence instead
  of reading each by hand.
- scripts/patch_recomp.py `patch_cap_probe` (runs LAST in the mp chain):
  docs/player-limit/cap-probe-sites.csv (id,address,function,asm,occurrence,
  total,reg). 910 sites in 574 functions: every `cmpwi/cmplwi rX,23|24|25`,
  `li rX,23|24|25`, `subfic ..,24|25`, plus `cmpwi/cmplwi ..,92|96|100|128`
  inside the 396 functions that load EntityList (lwz -14340(rX)). Inserts
  `MpCapProbe(id, ctx.rX.u32); // cap probe 0.5.239` before the site's code
  line (value 0 for li/subfic). Located by (asm text, occurrence among
  identical asm lines, total) in the post-chain generated C; every site is
  verified before anything is written (dry run in scratch: 910 inserted,
  second run idempotent, 70 files clang -fsyntax-only clean except the
  pre-existing coroutine goto in ppc_recomp.78).
- CSV built from the retail disassembly (llvm-objdump of the decrypted
  image) aligned to the generated C by counting asm comment lines from the
  function start; the alignment treats "// session roster 0.5.213" as an
  instruction and jumps to 824048E4 after "// server client scan 0.5.214".
  Skipped: 824048B4 (inside the replaced scan), 82483124 (already 32).
- mp_fixes.cpp: MpCapProbe/MpCapProbeWrite/MpCapProbeTick. Per site hits,
  low/high (int32), 64-bit mask of values 0..63 seen, count >= 64. Writes
  cap-probe.csv (exe folder) every 30 s, at exit, and from BigTest.
  COD3_NO_CAP_PROBE turns it off.
- bots.cpp BigTest (COD3_BIG_TEST=<maps>, COD3_BIG_BOTS=20,
  COD3_BIG_SECONDS=150): devmap, A until alive, god, BotsAsk; status line
  every 15 s ("big test:"), back (scoreboard) every 25 s; at the end writes
  the probe and TerminateProcess. Called after PhotoTour::Frame in
  PPC_FUNC(sub_82535810), as is MpCapProbeTick.
- Game folder BIG-TEST.bat: one process per map (a second devmap in one run
  loses the session), copies CoD3MP.log -> big-test-<map>.log and
  cap-probe.csv -> cap-probe-<map>.csv.
- Reading it: a player loop shows hits ~ frames x 24 and seen 0..24; a "no
  player" test sees 24; sites with 0 hits are dead in these tests. devmap
  has no session (bots get no session player), so the session layer needs
  a real System Link match too.
- Next: merge the probe files, classify live sites, write the 24->32 site
  list (same CSV-driven patch, with value checks), drop the EntityList +100
  copy stores, then raise PlayerCapacity::ActiveClients.

### 238b RESEARCH (Claude, 2026-10-18 19:45, no code change): player cap - all 212 compare/store/loop 24 sites classified

- Loading pictures done for all 11 DLC maps (game folder textures\replace\maps\<map>\, both keys);
  mpdepot2 10, mpstalag 11, mp_guns 4, ordnance 4 from the v3 reshoot (mapping json in build-logs).
- Original code disassembly WITHOUT CoD3Scan: llvm-objcopy -I binary -O elf32-powerpc mp.bin mp.elf;
  llvm-objdump -D --triple=powerpc (address = offset + 0x82000000). mp.bin = decrypted image
  (scratchpad xexwork/unxex.py). Never upload either.
- docs/player-limit/COMPARE-REVIEW-239.md + compare-review-239.csv: groups A session roster 62,
  B server clients 6, C lobby/party arrays in the session object 14 (NEW: storage still 24 - strides
  160/200/80/48/8 at session +632/+7536/+4443/+6336), D entity/local-place 10, E UI 12, F state enum
  14 (leave), G unrelated 93 (registry 0x82C77850 of 24 singletons etc.), H unknown 1.
- Next: relocate group C storage; entity slots 24..31; then bounds A+B+C+D behind one switch.

### 238 SOURCE CHANGE (Claude, 2026-10-18): loading picture picks installed; COD3_PHOTO_VARIANT

- Justin's picks (v2 sheets = photo-tour.log view N): rimling 6 (172240),
  longyard 4 (173449, gamma 0.75 x1.12), mpmonte 7 (173632), mp_hilltops 9
  (174608), mp_isle 9 (175216), shipyard 5 (175923), mp_farm 5 (180532).
  Pillar bars cropped, 2:1 crop at 45% from top, 2048x1024 Lanczos, each in
  game folder textures\replace\maps\<map>\ as BOTH
  1024x512_f18_05F0F9261D8E1859.png and 1024x512_f18_4884F46D37EDE122.png.
  Still to confirm which key the loading screen shows. Reshoot: mpdepot2,
  mpstalag, mp_guns, ordnance (ordnance keeps shot 7 for now).
- bots.cpp PlanViews: COD3_PHOTO_VARIANT (0-5): high cameras +v*0.6 rad,
  roof +v*1.1 rad, first v*6 accepted streets skipped. Game folder:
  PHOTO-TOUR-RESHOOT.bat (variant 1, the 4 maps).
- Next after pictures (Justin): back to the player cap (cap 24; HANDOFF 218,
  docs/player-limit/RESUME-214.md, entitylist-uses-218.csv).

### 237 TEST 6 (Claude, 2026-10-18 18:30, no code change): first two-PC co-op on 0.5.237

- Justin HOST, joiner 100.87.12.49, Forest. Logs: game folder
  coop-host-play.log + coop-relaunch.log; findings for Team Friend in
  docs/build-logs/COOP-TEST-6-FINDINGS.txt, zip coop-test-6-for-friend.zip
  (D:/Call of Duty 3 (USA, Europe)). Joiner's logs NOT yet received.
- WORKS: film share + held skip (45-83 s), MakeBody 232 from spawner 22
  (96 s), stance/jump/move and shots both ways, grenades/smoke with fuses,
  CWX1 relaunch notice. Justin: closer to in sync than test 5.
- PROBLEMS: stand-in hit for 586/449/821 exactly when the joiner's smoke
  arrived (153/221/266 s; own projectile hurting the stand-in?); 3 smokes +
  1 frag "NOT thrown" (267-284 s); stand-in 232 gone at 382 s, no friendly
  spawner, borrowed soldier 6, then mission restart (402 s) made dixon
  (soldier 3) the stand-in; relaunch at 419 s: relaunched host only reached
  the front-end menu (log ends ~5 s, world sent once, no reconnect);
  stance changes arrive in quick pairs (->1 then ->4). Host fps min 7 /
  median 53 / max 144. All co-op items are Team Friend's (coop.cpp).

### 237 SOURCE CHANGE (Claude, 2026-10-18): Team Friend co-op v33 + v34 merged; photo tour results

- coop-for-friend (2).zip (Justin's Downloads, 18:07): coop.cpp (106728 B,
  v34) taken unchanged. input.cpp/input.h in the package were made from a
  pre-0.5.231 copy (no Input::PressButton, which the photo tour needs), so
  their input-sprint.diff was applied by hand onto ours (l3/r3 pad names,
  g_sprintHeld, Input::SprintHeld()); kernel_xam.cpp: their 3+2 lines
  (CoopBeforeRelaunch before Relaunch for default.xex, campaign only);
  result identical to theirs except our PressButton. native_options.cpp
  and title_fixes.cpp in the package equal ours. Their notes:
  docs/build-logs/COOP-TEST-5-REPLY.txt, COOP-V34-NOTES.txt.
- v33: shots = +52 rise while rec +1352 bit 1 (attack) and <= 1200 ms;
  stance from rec +208 0x20 (crouched), prone from view height <= 20 held
  0.3 s, changes held 0.15 s; body keeps the lowest height of 0.6 s
  (jumps); host attacks toward the joiner's body do no damage; CWX1 on
  relaunch, joiner follows, coop-relaunch.log, socket retries; delay 0.15 s.
  v34: grenades (type 3 'grenade'/'smoke_grenade', CWG1, magicgrenade
  sub_824B27A8); campaign sprint via g_speed x1.5 while Input::SprintHeld
  (COD3_NOSPRINT=1 off).
- Photo tour 0.5.236 ran on all 10 maps: learning runs worked (mpdepot2
  670 places, longyard 828, mpstalag 737, mp_hilltops 360, mp_isle 734,
  shipyard 421, mp_farm 890), 12 shots each; contact sheets in the game
  folder's photo-picks\<Map>-shots-v2.jpg. Some eye-height shots face a
  wall (paths over walls). Learning runs show split screen on screen only.

### 236 SOURCE CHANGE (Claude, 2026-10-18): photo tour: eye-height street views, learning runs; 0.5.235 test results

- Claude ran Justin's test on his PC (computer use, 16:55-17:05, 0.5.235,
  PROFILE-CPU.bat, System Link Marseille, bot_add 20, died once):
  DEATH VIEW FIXED (blurred, critically wounded view soft like the console).
  LOADING PICTURE: used.txt shows the System Link load used
  1024x512_f18_4884F46D37EDE122 (the global CoD3 splash upscale) at 82.0 s,
  not 05F0F926 (which the 16:29 run replaced per map). Shot 7 now also at
  textures\replace\maps\ordnance\1024x512_f18_4884F46D37EDE122.png (game
  folder only). Unconfirmed which one the screen shows in which path.
  PROFILE: CoD3MP.log stopped at 307 s (cut mid-dump, 2.0 MB) while the
  game ran 4 more minutes, and no "profile of the" report was ever written
  (ProfileHostThread is only started for "cp", kernel_video.cpp:1335).
  Unexplained; look at log.cpp's pipe reader and the profiler thread
  (LoadSymbols / SymFromAddr on CoD3MP.pdb) before asking for another run.
  Also: the device bridge timed out twice during play (heavy load).
- Computer use cannot turn the camera: the game ignores the synthetic
  absolute mouse moves, and typed console text is unreliable. So the tour
  plans the views itself.
- bots.cpp PhotoTour::PlanViews (>= 20 learnt places): 4 high (0.75 radius,
  z max(mz+450, place+300), at the middle), 2 roof (0.45 radius at +0.3 and
  +pi rad, place+200), up to 6 eye height: for every places/400-th place and
  16 directions, places ahead within 110 lateral / 220 vertical, followed
  while gaps <= 350, depth >= 900; deepest first, cameras >= 1400 apart and
  not within 20 degrees of a near one (< 2500); z place+64; pitch
  clamp(atan2(z - (farZ+40), depth) - 3, -4, 6). Tested standalone on
  ordnance/rimling data (0.1 s; same as the Python prototype). The old
  8-view code is kept behind "else if (false)".
- COD3_PHOTO_LEARN=<map>: devmap, alive -> god, BotsAsk(16), stage 4 for
  240 s, SaveMapKnowledge, next map/terminate. PHOTO-TOUR.bat calls :one per
  map: a learning run first when bots\maps\<map>.txt is under 8000 bytes.

### 235 SOURCE CHANGE (Claude, 2026-10-18): Team Friend coop.cpp v32 + native_options.cpp (options kept) merged

- From coop-for-friend.zip (Justin, 16:30): CoD3Host/coop.cpp (88413 B,
  v32), native_options.cpp (15188 B), title_fixes.cpp (byte-identical to
  ours, not written), PLAY-COOP-HOST/JOIN.bat and COOP-README.txt (identical
  to the game folder's). Taken unchanged; their notes:
  docs/build-logs/COOP-TEST-4-REPLY.txt.
- v32: MakeBody (dospawn sub_824B9118 from an actor_ally_* spawner, flag 64
  for the host's body; borrow fallback, COD3_COOP_BORROW=1); stance from the
  record's +184 view height to actor +656 (allowedstances sub_8249D140);
  shots by the record's weapon time +52 rise -> shoot sub_824B8FF8; CWP1 36
  bytes, CWQ1 12 bytes on port+2; mission end not taken for a restart;
  turned things (1 degree) sent as moved (flag 16); COD3_COOP_MAP ignored
  under COD3_LAUNCHDATA.
- native_options.cpp (campaign only, #ifndef COD3_MP): state block
  0x82A8E610 +60..+107 and the brightness float +400 saved to
  saves\options-sp.bin on change (polled once a second from coop.cpp's
  sub_82535E80 hook, NativeOptionsPoll) and restored in the defaults hook
  within 30 s of a non-relaunched start. COD3_NOSAVEOPTIONS=1 off. +400 as
  brightness unconfirmed (their note).
- No new host symbols outside the three files; multiplayer unaffected
  (both are #ifndef COD3_MP). UNTESTED between two PCs; both PCs need
  0.5.235 (CWP1/CWQ1 sizes changed). Friend update: MAKE-FRIEND-UPDATE.bat.

### 234 SOURCE CHANGE (Claude, 2026-10-18): death view blur; per-map texture replacements; Marseille is the cp thread, not the GPU

- Evidence: Justin's 0.5.233 Marseille run (CoD3MP.log 16:01, 20 bots, one
  death; death screenshots 155906/155906-2/155931/155932(-2); frame 10493
  logged at death frame 7, draws.bin beside it).
- DEATH FLASH. Frame 10493's post chain: scene resolve 04350000 (1040x624),
  particles at half size (0A8E0000/0A780000, ps 39753ca3, 2a892b14), then
  013C0000 (fmt 6) resolve; 10e0411b downsample -> 02100000 260x156;
  0925661f blur; 56ccd878 (the DOF blend back, 42 dw) and 5fb07e1b (shell
  shock pass, 231 dw) were loaded and DRAW_INDX'd by the title but never
  drawn by the host (no "frame ... draw" line; skips "aim blur off x71");
  then cc16ad469f5cb8cc (vs d76d2474, vte 400) samples 013C0000 twice =
  the double vision; bloom 22241069 / 0a8fb702 / fe657844. The crop of
  155906-2 shows two sharp copies (two star icons, two bodies). Cause:
  aim_depth_of_field=0 (settings default) skips 56ccd878 always
  (render_commands.cpp IsAimBlurProgram). Fix: RenderCommands::
  SetPlayerDown(bool) (atomic); the skip is not applied while the host's
  player is down. bots.cpp HostDeathCapture calls it every server frame
  (dead = health <= 0 and state 3/4; record 0 -> false). Logs "render: the
  host's player is down/up again". 5fb07e1b (shell_shock_blur) left as is
  (its copy was black, render_commands.cpp 1083). Host only: a joiner's
  copy does not run sub_82535810's hook.
- MARSEILLE PERF (correction of 228's "GPU bound"): RenderStats' gpu ms is
  end - begin timestamps of the frame = GPU timeline span incl. idle, so it
  tracks the frame time whenever the card waits. Per draw stamps gave
  d24754093992 (world, 5 textures, normal mapped; avg 104 indices = 35 tris
  a draw in frame 10493) ~9 us a draw, depth only draws 13 us: the arrival
  pace, not the cost. Frame time vs draws (stats lines): 1177-1756 draws
  18-19 ms (floor), 2927 24.6, 4711 35.7, 5141 40.1: about 7 us a draw
  above ~2000 draws. profile: 3.8 us a draw of draw path (buffers 1.2,
  textures 0.55, constants 0.56, bind 0.57); cp waits for the title 1%;
  "fingerprints over 33 MB a frame" (8.5k looks, 5.7k by pages). So the cp
  thread is the limit: ~3.8 us draw path + ~3 us elsewhere (PM4 parse,
  register writes ~300k a frame, driver). render_stats.cpp: COD3_GPU_DRAWS
  now opt in (was RenderSwitch, default on in MP since 228).
  NEXT: PROFILE-CPU.bat (COD3_HOSTPROFILE=cp,main) standing in a heavy
  Marseille view; then cut the top functions (likely the buffer
  fingerprinting and the packet loop).
- PER-MAP REPLACEMENTS: texture_pack.cpp Scan(): replace\maps\<map>\<key>
  (exactly 3 path parts, first "maps") goes to g_mapReplacements[map], not
  the global table. Load(..., std::string* forMap): the current map's
  (Kernel::CurrentMap lower case, set when dlcN:\mp\<map>\<map>.cod opens)
  first; used.txt says "replaced for <map>"; log "replaced from
  maps\<map>\". render_resources.cpp TextureEntry.replacedFor (cleared at
  each UploadTexture); a cache hit whose replacedFor != CurrentMapKey() is
  uploaded again (the shared DLC picture may sit at the same address with
  the same texels on the next map).
- LOADING PICTURE: 0.5.233 used.txt: 1024x512_f18_05F0F9261D8E1859
  NOT-REPLACED first at 19.6 s, after menus (<= 6.6 s) and before the map's
  textures (22.3 s); D:\mp\loadscr.cod read. Not proven to be the loading
  picture (no dump existed). Dump(): with texture_dump off, a 1024x512
  texture with no replacement is still saved (12 a run), logged "a 1024x512
  picture with no replacement ... while the map is". Shot 7 (150928, 69..1833
  content, 2:1 crop at 45% from top, 2048x1024 Lanczos) committed as
  game folder textures\replace\maps\ordnance\1024x512_f18_05F0F9261D8E1859.png.
  Check the next log for "replaced from maps\ordnance" and the dump; if
  the loading screen draws a sub rectangle, refit to the dump's framing.
- Syntax: texture_pack/render_stats/render_commands/render_resources with
  stub windows/d3d11 + ppc stub: error counts identical to the unedited
  files (52/0/4/13, stub only), none on changed lines.

### 233 SOURCE CHANGE (Claude, 2026-10-18): free photo mode (COD3_PHOTO_FREE)

- 0.5.232 tour (photo-tour.log): rimling, ordnance (8 views from above each),
  mpdepot2, longyard (spawn views, 0-1 learnt places) done and each process
  ended by TerminateProcess; the run stopped after longyard (7 maps left).
  Shots 150748-151159; contact sheets made for Justin (outputs/photos).
- PhotoTour::Free(): COD3_PHOTO_FREE=<map>: same load/A-press/spawn, then
  "god / noclip" only, Overlay::Notice, stage 3; split-screen clients kept
  at 0 in stage 3. HUD left on (F10 toggles in overlay.cpp's state).
- Game folder: PHOTO-FREE.bat (asks the map), PHOTO-TOUR-REST.bat (the 7
  remaining maps through PHOTO-TOUR.bat).

### 232 SOURCE CHANGE (Claude, 2026-10-18): photo tour views from above; TerminateProcess at the end

- 0.5.231 live: rimling spawned (A presses worked, state 1 -> 3), 4 views
  shot (145344-145356): HUD/gun hidden OK, split screen gone, but low (+140
  over the place) and blocked by walls/trees, view 4 indoors. "quit" then
  froze the game (fps 0, window stayed): PHOTO-TOUR.bat stuck on map 1.
- PlanViews: 8 views: k*45 deg around the centroid at 0.75 (even k) or 0.45
  (odd k) of the max radius, snapped to the nearest learnt place; z =
  max(mz+450, place z+300); yaw to the centroid; pitch = atan2(z-mz, dist)
  capped 35. Spawn fallback: 4 yaws x (+350 z pitch 20, +60 z pitch 5).
- End: COD3_PHOTO_QUIT -> detached thread, Sleep 3 s, TerminateProcess.

### 231 SOURCE CHANGE (Claude, 2026-10-18): photo tour: A through the team/class menus; one map per process

- 0.5.230 tour log: "devmap rimling" loaded (map knowledge loaded) but the
  host stayed on the team select (never alive) -> 2 min timeout; the next
  "devmap ordnance" from inside the game ended with "game session no longer
  available" (Justin). Fixes: input.cpp/h Input::PressButton(name) (atomic
  buttons + 2 reads, returned first by ScriptedButtons); PhotoTour presses A
  every 1.5 s once g_mapKnowName == map and the host is not alive (state 3,
  health > 0), from 10 s after the devmap. PHOTO-TOUR.bat loops the maps,
  one CoD3MP.exe run each (start /wait, COD3_PHOTO_TOUR=<map>,
  COD3_PHOTO_QUIT=1), appending each CoD3MP.log to photo-tour.log.
- devmap from the menu came up as 4-player split screen (4 team menus,
  Justin). PhotoTour writes LocalState(1..3) (0x82C7A270 + k*0xc88 + 0xb8)
  to 0 every frame once the map is in ("local client k was N").
- Syntax: bots.cpp 0 (stub localtime_s only), input.cpp 41 stub-only (same
  as before).

### 230 SOURCE CHANGE (Claude, 2026-10-18): Team Friend coop.cpp v31 + title_fixes film sharing

- From ~/Downloads/coop-for-friend (1).zip (their 0.5.231 numbering;
  COOP-TEST-3-REPLY.txt in build-logs). coop.cpp replaced whole (adds
  CoopFilmActive/CoopFilmStop/CoopFilmEnded, CWR1/CWF1 on UDP port+4/+5,
  interpolation buffer COD3_COOP_DELAY (0.1 s), COD3_COOP_SNAPPY=1 old way).
  Same overrides (sub_82451C68, sub_82517A00, sub_82535E80).
- title_fixes.cpp replaced whole: v30 auto-skip removed; sub_824D0470 inside
  the film asks CoopFilmStop(pressed) in co-op; sub_824EEBA0 calls
  CoopFilmEnded after the film. COD3_SKIPFILMS=1 outside co-op still skips.
  (title_fixes is campaign-only, coop.cpp campaign-only: MP unaffected.)
- PLAY-COOP-HOST/JOIN.bat identical to v30 (no change in the game folder).
- Syntax (campaign defines): coop.cpp 0, title_fixes.cpp 0.

### 229 SOURCE CHANGE (Claude, 2026-10-18): PhotoTour (DLC loading pictures); F10 photo mode

- DLC dlmaps.txt (Bravo/Valor/Champs) give each map only a loading title and
  location, no picture: all 11 DLC maps share one loading image (to find:
  dump while loading a DLC map). Per-map replacement needs a texture_pack
  change (folder per map) - NOT done yet; pictures first.
- bots.cpp namespace PhotoTour, called after the server frame in the
  sub_82535810 hook. COD3_PHOTO_TOUR=1 (11 DLC paks) or a comma list;
  COD3_PHOTO_QUIT=1 quits at the end. 20 s after start: "devmap <pak>"
  (2 min timeout), wait g_mapKnowName == pak and host health > 0 (+8 s),
  "god / noclip / cg_draw2D 0 / cg_drawGun 0", 4 s, views: from
  bots\maps\<pak>.txt (>= 20 places) the farthest place per quadrant
  around the centroid within 85% of the max radius, +140 z, yaw to the
  centroid, pitch 10 down; else 4 yaws from the spawn (+60 z). Each view
  held 4.2 s (record +0..8 written, +16..24 zeroed; client view angles at
  0x82A57900 + index*6352 +1668/+1672 nudged by the difference to record
  +204/+208), screenshot at 3.5 s. Log "photo tour: <map> view N ...
  screenshot HHMMSS". (0.5.106 note: setviewpos did not move the host, so
  the origin is written.) Known maps: ordnance rimling mpmonte mp_guns
  shipyard; the other six use spawn views.
- overlay.cpp: F10 toggles cg_draw2D/cg_drawGun; F12 skips the draw log in
  photo mode. Help line updated.
- Game folder: PHOTO-TOUR.bat.
- Syntax: bots.cpp 0 (stub-only localtime_s); overlay.cpp stub-only
  (VK_F10 missing from the stub).

### 228 SOURCE CHANGE (Claude, 2026-10-18): GPU per-draw timing in MP; death frame draw log; medic kit lines

- 0.5.227 live (ordnance, 20 bots): REVIVE WORKS: slot 20 "stands slot 0 up
  (Plyr_Revive, handles 00008001 00008015): health now 100, state 3" ->
  REVIVED. Other trips: several "took too long" (22 s, navigation), host
  "not revivable" while state 3 health <0 (dying, before state 4).
  Medics still 2/20: bot_kits.h picks the kit with the lowest team
  count/weight, ties -> first line; with 15 lines and ~10 bots a side every
  line is used once and weights never matter. bot_kits.txt: 2 more medic
  lines (thompson, mp40) after scout -> 3 medics a side.
- Render (stats/profile on): RTX 2070, 1800x1080 MSAA 4x (antialiasing=3),
  console_depth=1. Quiet: 50 fps, cpu 11.6 ms, GPU 19.4 ms, 2208 draws,
  4552 texture switches. Heavy: 11-18 fps, GPU 48-92 ms, 4000-5500 draws.
  profile: 3.4-4.7 us a draw (buffers 1.1-1.8, textures 0.5-0.9, constants
  0.55, bind 0.55). => GPU-bound. Suspects: console_depth (depth written in
  the pixel program, no early Z: overdraw of smoke/fire), MSAA 4x resolves.
  render_stats.cpp: COD3_GPU_DRAWS default on in MP, sampled 1 frame in 8
  ((g_frames % 8) == 0 in BeginFrame): "gpu draws: ... the most:" list of
  pixel programs by GPU ms every 5 s.
- Death flash (0.5.226 capture, death by Bot 4): screenshot frame 2 full
  blur, frame 6 sharp standing view (frozen moment of the hit), 12/20 that
  picture ghosted over the ground death cam. A retail "double vision" pass
  whose copy is sharp here (console's is low-res). HostDeathCapture now
  ignores state 0 (spawn screen; 0.5.226's deaths 1-2) and calls
  Render::LogNextFrame at frame 7 of the first death (draw log + draws.bin
  in screenshots) to find the pass/program.
- Game folder: CoD3Recomp.ini texture_dump 1 -> 0 (left on since the
  campaign menu pass).
- Syntax: bots.cpp 0; render_stats.cpp 41 stub-only (same as before).

### 227 SOURCE CHANGE (Claude, 2026-10-18): coop.cpp v30 + title_fixes film skip from Team Friend

- From ~/Downloads/coop-for-friend.zip (COOP-TEST-2-REPLY.txt in build-logs).
  coop.cpp v30 replaced whole (SmoothFollow stand-ins; +580 script-target
  soldiers only after 10 s; 'delete_me' never; stand-in replaced when gone;
  30 s fallback from first soldier; non-soldiers paired within 1500; restart
  count in CWL1 bytes 68-71 (lobby packet 68 -> 72: both PCs must match);
  joiner brought beside host >1500 away 5 s after load; COD3_COOP_MAP).
  Same overrides as v29 (sub_82451C68, sub_82517A00, sub_82535E80).
- title_fixes.cpp: theirs == ours + title_fixes-filmskip.diff (checked): adds
  sub_824D0470 override (answers 1 inside sub_824EEBA0 when COD3_SKIPFILMS,
  default on when COD3_COOP set) and a log line in sub_824EEBA0. Campaign
  only (#ifndef COD3_MP). No other host file overrides sub_824D0470.
- Game folder: PLAY-COOP-HOST.bat (asks for COD3_COOP_MAP) and
  PLAY-COOP-JOIN.bat from the package; COOP-README.txt unchanged.
- Syntax (campaign defines, stub6 winsock): coop.cpp 0, title_fixes.cpp 0.

### 226 SOURCE CHANGE (Claude, 2026-10-18): revive via Plyr_Revive; host-down medic report; render profile on in MP; death capture

- 0.5.225 live (ordnance, 20 bots): "perf:" over 5 s with bots: bots 1.4-3.1 ms
  (40-83 traces a frame, 0.4-0.7 ms), server frame 6.8-8.6 ms, rest 10-30 ms;
  before bots (host alone): rest 25-36 ms, 135-188 frames/5 s. render line:
  22.6-54 fps, render cpu 11-15 ms, 6319 draws, 15164 texture switches, 641
  program switches a frame; "cp: 0% waiting for the title" (CP always busy).
  Kernel: NtWaitForSingleObjectEx/NtReleaseMutant x285M each in 279 s (job
  lock, ~1M/s, known). Conclusion: client/render side, not bots.
- Revive: 6 direct sub_823E86A8 calls (message 93), none took; medic still
  held shotgun (MpFixesSwitchWeaponFor mp_revive does not change +164).
  sub_823E86A8 = build msg 93 (two 24-bit session indices via sub_823E5F30)
  and sub_823D8F08 send: host to itself is never handled. Now stage 3 calls
  sub_82201D40 (Plyr_Revive, natively compiled .bro) with r3 = &deadHandle,
  r4 = &medicHandle (handle = (*(0x82BD1FC0+8*index+4) << 14) | index,
  index = slot+1), inside BotClientScope(target slot). COD3_REVIVE_DIRECT=
  message restores 0.5.225, =0 kit trace only. Log "stands slot N up
  (Plyr_Revive ...): health now H, state S".
- Host down: each living same-side medic logs once per host death: distance
  (g_deadAt), job stage/target, enemy within 500, next-try delay, claimed by
  another; or "not revivable" with health/state if state != 4. 0.5.225 log
  had no medic trip to slot 0 at all.
- render_stats.cpp: RenderSwitch(): in COD3_MP, COD3_RENDER_STATS,
  COD3_RENDER_PROFILE, COD3_GPU_PROFILE default ON (=0 off). Next log has
  "stats:" + "profile: X us a draw: state/targets/pipeline/textures/
  buffers/constants/bind/indices/draw" + gpu ms: pick the dominant section.
- HostDeathCapture (bots.cpp, from the sub_82535810 hook): first 3 host
  deaths, 60 frames of "death view:" (CurrentClient, +1924, +1932, gun view
  pointers 0x82C37070..7C) and Overlay::RequestScreenshot at frames
  2/6/12/20/35. For Justin's "glimpse of me like split screen" at death.
- Game folder bot_kits.txt: medic weight 3 -> 6.
- Syntax: bots.cpp 0 errors; render_stats.cpp only the stub D3D errors (42,
  same as the unmodified file).

### 225 SOURCE CHANGE (Claude, 2026-10-18): direct revive call; perf split logging

- 0.5.224 live (ordnance, PROFILE-MULTIPLAYER): threads over 5 s: guest 0
  (main game thread) 96-100%, command processor 77%, guest 1/4 (821424B0)
  ~35%. Render profile ~3.5 us a draw (buffers 1.5, bind 0.57, textures
  0.53, constants 0.5), 4860 constant blocks, 16 MB fingerprinted a frame.
  So the frame is bound by the guest main thread (Com_Frame: client + server
  + FeedBots), not the renderer. Revive: 7 trips, 6 aborted "enemy in sight".
- bots.cpp: BotTrace wrapper counts/times traces; sub_82535810 hook times
  FeedBots vs __imp__sub_82535810 (server frame) vs the rest; "perf: a game
  frame: bots X ms (N traces a frame, Y ms of them), server frame Z ms, the
  rest W ms" every 5 s.
- BotRevive: stage 3 calls sub_823E86A8(*(*(0x82BCCAC0)), target entity,
  medic entity) once per try (Job.sent) - the call sub_82579408 makes after
  sub_82575910's hit. COD3_REVIVE_DIRECT=0 disables. Abort distance 300.


### 224 SOURCE CHANGE (Claude, 2026-10-18): shotgun aggression; screen smoke; revive at fall position

- 0.5.223 live: bot grenades seen by Justin (record+164 write works);
  killfeed fixed. Smoke: 9 throws, 36 "has no smoke left" (kits carry 1-2).
  Revive: slot 20 reached slot 0 (Justin) twice, never fired: d > 70 reset
  (dead player's entity follows the spectator view). 6 "he is up again"
  (targets 900-1560 away respawned first).
- bots.cpp: BotHoldsShotgun (def name shotgun/trench) -> FightAggressive
  unless health <= 25; closeIn 110, backOff 0, sprint+weave beyond 450, no
  range stop, no melee charge with a loaded shotgun. Screen smoke in
  BotExtras for RoleObjective/RoleRusher moving forward with g_seeDist
  700-2500. BotRevive: g_deadAt[] (record origin at first death sighting)
  used as the body point; search radius 1000; reset distance 120; respawn
  hold 25 s.
- coop.cpp replaced with Team Friend's v29 (~/Downloads/coop-for-friend (1)): stand-ins = any actor_ally_*_reg*, fallback any ally after 30 s; per-mission reset at SV_SpawnServer (g_mapGen); flag 16 moved-only sync. Their reply: COOP-TEST-1-REPLY.txt. Note: a joiner needs every mission recompiled (their joiner lacked saint_lo).
- Ordnance (Justin's "Marseille") perf, 0.5.223 log: avg 44.7 fps over 359
  samples (fuel 60.6); render cpu median 15.1 ms, p90 21.8 ms; 4300-5000
  draws, ~11k texture binds (already de-duplicated in BindTexture), "cp: 0%
  of the time waiting for the title" = the command processor thread is the
  limit. Next: a PROFILE-MULTIPLAYER.bat run on that map (COD3_THREADTIME,
  COD3_GPU_PROFILE, COD3_RENDER_PROFILE) to split the CP time.


### 223 SOURCE CHANGE (Claude, 2026-10-18): coop.cpp merged (campaign co-op experiment)

- From Justin's coop-for-friend package (~/Downloads/coop-for-friend; README
  says "from Team Justin, session 4", untested between two PCs). Added
  CoD3Host/coop.cpp unchanged and "coop.cpp" at the end of COD3_HOST_SOURCES
  (CMakeLists). Whole file is #ifndef COD3_MP (make_mp_host.py skips it; MP
  copy empty). Overrides sub_82451C68 (NET_SendLoopPacket), sub_82535E80
  (Com_EventLoop), sub_82517A00 (SV_SpawnServer) - no other host file
  overrides these (title_fixes only mentions 82517A00 in a comment). Uses
  Kernel::WatchWrite/ArmWatchpoints/QueueConsoleCommand, Overlay::Notice/
  RequestScreenshot, Settings systemLinkPeers - all exist. Syntax-checked
  (campaign defines, Linux stubs + a BSD winsock stub): 0 errors.
- Runtime: COD3_COOP=host|join (PLAY-COOP-HOST/JOIN.bat in the game folder),
  COD3_COOP_HOSTIP / COD3_COOP_JOINIP, COD3_COOP_APPLY=goal, UDP 28962/28963.
  Forest only; levels/forest was already recompiled (recomp_forest.log).
- Game folder: PLAY-COOP-HOST.bat, PLAY-COOP-JOIN.bat, COOP-README.txt;
  repo copy of the README: docs/COOP-README.txt.


### 222 SOURCE CHANGE (Claude, 2026-10-18): record+164 = grenade at throw start; revive trace

- 0.5.221 live (bot_add 20): bot throw (slot 10, stielhandgranate #16, in
  clip 1/0): command 0x2 + word 0x10 first, quiet after; +168 = 2 for ~21
  commands then 0, but +164 stayed the gun (0x15). Host: +164 = grenade
  (0x0B) from the first command through the throw, word back to gun after.
  Search tried all 4 bits: nothing spent. Now the command write also sets
  record+164 = g_throwWeapon on the throw's first command.
- Revive: one medic reached a body, 3 tries, record+2948 stayed -1
  (sub_82575910 never ran: the revive check in sub_82463A48 needs def+172
  == 4 and +176 == 11, reached from sub_82464AF8). "bots: revive trace"
  logs held weapon, its +172/+176, +168, +2948 during the fire stage.
  Note bots also fire through the direct FireWeapon (sub_82590C30) call
  while wantsFire; that path does not do revives.
- Killfeed wrapper confirmed active ("current client 3 -> 0"). Rifle
  grenades "special set to 4" logged (Justin saw 3: one loaded + 3).
- Campaign upscales: + mission 1024x512_f18_5EE26A2430BFDBC5, C7895A4E9A3FAF01.
- 0.5.221 was the first build made by AUTO-BUILD (OK, 2.6 min).


### 221 SOURCE CHANGE (Claude, 2026-10-18): campaign menus unlocked; campaign upscales

- kernel_video.cpp TitlePlaying (campaign build): only the film player
  (0x82A2A24C non-zero) keeps the console rate; menus unlocked too (was:
  sv_running && no film). COD3_SPMENUFPS=0 restores the old rule. 0.5.215
  campaign log already showed 144 fps in a level (frame_cap 144), menus 30.
- Campaign upscales in textures\replace (Justin's PC only): start menu 7,
  sepia 3 (F606FAA4, AC7B4839, F481CE70), then mission art + thumbnails
  (esr/overnight.sh order). List: HANDOFF 217.
- Auto-build: AUTO-BUILD.ps1/.bat/-AT-STARTUP.bat in repo root (see
  NEXT-SESSION). Justin has to start it once himself (no remote shell; File
  Explorer control is single-click only).


### 220 SOURCE CHANGE (Claude, 2026-10-18): killfeed CurrentClient; throw quiet; special count 4; medic fire

- 0.5.219 live: bots' frag/smoke with 0x2 + grenade weapon word: record +168
  became 2 (offhand started, as host) then 0 after 2 commands; bot buttons
  0x2002 (stance tap) then 0x4 (sprint) after release; host presses only 0x2
  (or 0x80) and his +164 = grenade (0xC) for ~16 commands, +196 = last
  offhand. Fix: g_throwQuiet = 60 commands: strip StanceButton/Sprint/fire
  at command write; weapon word grenade on the first command only.
- Killfeed: sub_82456920(victim, attacker, mod): teamBased byte
  *(0x82BA13E4+20) -> sub_8244DB20(team = ent+520 -> +4): compares with
  EntityList[*(0x82BCBE9C)+1]'s team. CurrentClient (0x82BCBE9C) is the
  bot's local place inside BotClientScope, where bot kills happen. Wrapper
  sets it to 0 for the call when g_bots > 0 and it is non-zero. Non-team
  path colours by svs client +1880 == 2 (all bots forced 2) - unchanged.
- Specials: sub_821EE6C0 (mp_mine) / sub_821EE7C8 (m1garand_rg / k98_rg) give
  the weapon (+2664 of builtin table 0x82A98030), set "special" clip 0
  (+2864) and spare 1/2/3 by rank (+2872). Wrappers in mp_fixes.cpp call
  +2872 again with ("special" script string via 0x821E0E48, 4); release
  0x821DD938. Defs: mp_mine clip 10 start 10 max 10 (clip and reserve share
  pool #26); k98_rg clip 5 start 10 max 5; m1garand_rg clip 3 start 3 max 5.
  maxAmmo/startAmmo now set to 4 for *mine*/*_rg. Crate refill unverified.
- Revive: 0.5.219 one medic (slot 7) of 20; at body once, never fired
  (yaw error never < 12). Now fires at < 20 deg or after 2 s. bot_kits.txt
  (game folder) medic weight 1 -> 3.


### 219 SOURCE CHANGE (Claude, 2026-10-18): grenade weapon word; special ammo 4; flag crouch; feelers

- 0.5.217 live (host, 20 bots, map change ok - bots re-added): host
  "spent a frag" x4 and "spent a smoke" all with keys 0x2 only; throw trace
  host: the FIRST command of each throw has weapon word +8 = 5 (the grenade),
  then 0x18 (gun) while 0x2 stays held ~11 frames. Bots sent 0x6 (0x2 +
  sprint) with their gun (0x2A): nothing spent. Fix: g_throwWeapon (best
  frag/smoke weapon number from the kit, BotNadeWeapon) written into command
  +8 for the first 2 commands of a throw (g_throwFrames); candidates now
  {0x2,...} for both, no mutual avoid; smoke press is 0x2 alone (no sprint,
  no fire). The 0.5.215 trace trigger compared serverTime with GetTickCount
  (Think's `now` is serverTime) - now on g_throwFrames / 0x2.
- Revives 0.5.217: 11 starts, 1 reached the body, 8 aborted "enemy in sight"
  (<900). Now <500 and only before the body (stage < 2).
- mp_fixes.cpp (weapon tuning at load, before the campaign clip print): defs
  named *mine* or *_rg: reserve = 4 - clipSize (min 1) into maxAmmo +1472
  and startAmmo +1452 when lower (COD3_SPECIAL_AMMO=n). Logged per weapon
  with class (+176), clip/reserve indices. Whether the support crate refills
  specials up to maxAmmo is the assumption to check.
- Flag hold: idle at flag crouch cycle 4 s of 12 (slot-staggered) + small
  strafe; fighting at flag crouch 2 s on/off + strafe.
- BotFeelers (after BotRevive/BotDispense, every brain bot): every 150 ms a
  body sweep 44 ahead at feet+18 along the actual move (from forward/right
  and view yaw +208); hit with normal.z <= 0.6 -> slide (move minus its
  normal component) toward the more open of +-40 deg feelers (40 units);
  head-on and both shut -> back off 400 ms. 3 bumps within 80 units in 12 s
  -> g_failedRoutes[slot].Remember(feet, 64 ahead), haveStep=false.
  "bots: feelers: N walls felt, M spots learnt" every 30 s.
- PlayerLimitProbe line was absent in 0.5.217 (expected: 0.5.218 code).


### 218 SOURCE CHANGE (Claude, 2026-10-18): EntityList level slot +100 -> +132 (player-cap prep)

- EntityList (*0x82BCC7FC) is a 104-byte C++ object allocated in
  sub_825583F8 (sub_820DE848(4, 104, ...), `li r4,104` at 82558A24) and built
  by sub_824830E0 (vtable 0x82077004; registers itself in a 24-entry system
  registry 0x82C77850 - unrelated cap; clears +4..+96 with li r9,24, +100=0).
  Layout: +0 vtable, +4..+96 player entities (client n at +4*(n+1); spawned
  as 24 by sub_8249B1E0 when +4 is empty), +100 the level entity
  (sub_825638A0 spawn, stored by sub_82535A18 / sub_82593FA8; not an index-25
  entity number). sub_824B1B28 (map restart) frees and clears +4..+96, +100.
- Audit: every function, EntityList loads and register copies followed to
  their uses (study/lib.py dataflow; volatile regs dropped at calls): 508
  loads -> 402 indexed (player by slot+1), 42 const +100 (40 reads, 2
  stores), const +0/+4/+8 few, 37 pointer walks from +4 (24-count loops; need
  32 at activation), 14 unresolved. Lists: docs/player-limit/
  entitylist-level-sites.csv (patched sites) and entitylist-uses-218.csv
  (all 508, with use kind).
- scripts/patch_recomp.py `patch_entity_list_level` (runs last in `mp`):
  per site (function, asm text, occurrence, expected count) - refuses if a
  count differs; 40 reads -> +132; the 2 level stores + constructor + map
  restart clear write BOTH +100 and +132 (a missed reader still sees the
  level while the cap is 24); size 104 -> 136; constructor clear 24 -> 32.
  Idempotent via "// entity list 0.5.218" marks (46 expected). Verified on
  a copy of the 2026-10-17 generated ppc_mp: 46 marks, second run no-op.
- bots.cpp PlayerLimitProbe: every 5 s logs (on change/mismatch) "player
  limit: entity list ... level +100 X +132 Y spare slots clean/IN USE".
- Activation still needs: the dual +100 write removed; the 37 pointer walks
  and sub_8249B1E0 / sub_824B1B28 counts at 32; the indexed readers checked
  for bounds (slot < 24 assumptions); plus RESUME-214's list.


### 217 SOURCE CHANGE (Claude, 2026-10-18): bot medic call; campaign menu upscales

- **Medic call:** host-side handler sub_823E3860 (vtable 0x820A0DB0, a
  session message): roster entry (784 stride) +4120 = client, then the
  script callback *(*(0x82BCBF64)+3352) = reenable_medic_call (sub_821F0DA0,
  registered in sub_82216FE0) with r3 -> word holding entity+484. bots.cpp
  `CallMedic` (namespace, after PickSide) calls it the same way (scratch below
  r1, BotClientScope "medic call"); in the respawn block, once per death,
  1.5 s after death with record+1924 == 4. COD3_NO_MEDIC_CALL=1 disables.
  The table also has 3344 (spotter miss) and 3348 (revive miss, "nobody to
  revive") callbacks used by sub_82463A48.
- **Campaign menus:** used-campaign.txt (Justin's pass, 0.5.215 campaign):
  1257 entries, all but 39 movie frames (_f2_). The 38 NOT-REPLACED: start
  menu 2048x1024_f18_D2261FA1C34D05A3 (background), 2048x512_f20_127DD5C9B8C2B5D0
  (CoD3 logo), 256x128_f20_C046CE5AB5EAA7F3 (pad buttons), 512x256_f20_88F5203782BB9483
  (font), 1024x512_f18_9769DB58A2861484 / 98D96D87D4077A89 (menu frames),
  128x128_f20_10D6C2D03BB4DAC9 (lock); sepia 512x256_f18_F606FAA4 / AC7B4839 /
  F481CE70; mission art 1024x512 x17 + 128x64 thumbnails x15. This build:
  the start-menu seven (esr/esrgan_np.py; 2048 inputs capped at 4096).
  Next builds: sepia + mission art, a few each.


### 216 SOURCE CHANGE (Claude, 2026-10-18): medic revive for bots; log fix for the no-console CoD3MP

- **0.5.215 regression:** CoD3MP.log 0 bytes. In a WIN32 program fds 0-2
  are closed; log.cpp's freopen(stdout,"NUL") took fd 0, then the pipe was
  _dup2'd onto 1/2 while stdout kept writing fd 0 (NUL). Fix: reopen stdin
  first (fd 0) when _fileno(stdin) < 0, and after the _dup2s also dup the
  pipe onto _fileno(stdout)/_fileno(stderr) if they are not 1/2. main.cpp's
  --console path reopens CONIN$ first for the same reason.
- **Revive mechanics (retail):** weapon class list at 0x825D7838 (def+176):
  rifle0 None1 smg2 lmg3 pistol4 grenade5 rocketlauncher6 turret7 spotter8
  non-player9 sniper10 revive11 health12 ammo13 interact14 mine15 flag16
  spread17. sub_82463A48(entity, weapon) fire check: def+172==4 &&
  def+176==11 -> sub_82575910(entity, view, &target): 100-unit trace from the
  eye (radius ~30 sweep of entities by abs bounds +272/+288); success needs
  hit entity +512 record, record+1924 == 4 (dead), and +520->+4 equal to the
  medic's (team). Miss: record+2948 = level time (MPGAME_MEDIC_NOBODY_TO_REVIVE
  hint, sub_824FE358). sub_82579408 (fire): target -> sub_823E86A8(session,
  target, medic) = session message 93 with both 24-bit session indices ->
  script Plyr_Revive (_mp_common.bro, sub_82201D40, table 0x820949E8 id
  0x4000cc03): health = g_player_maxhealth, loadout again (sub_821FB7E0).
  reenable_medic_call (sub_821F0DA0) plays american_/german_medic 5 s after
  the call; the call itself is the death-screen option
  (MPGAME_SELECT_KIT_CALL_MEDIC, sub_82392F40) - not driven for bots yet.
  "Critical" spectate states exist (UpdateSpectateCritical[GoingToDie]).
- **bots.cpp:** `g_reviveTarget[]` + `MedicComing(slot)` in the namespace.
  `BotRevive` (before BotKitProbe; called before BotDispense, which is
  skipped while reviving): medic (kit base 3), no enemy seen within 900,
  nearest dead teammate (health<=0, state 4, same Side, dead <25 s, not
  claimed) within 1600; walk (TacticalMove) to <38 units of the body centre
  (entity +272..+288, fallback record origin); MpFixesSwitchWeaponFor
  "mp_revive"; stop, aim (g_yaw/g_pitch eased, g_wantYaw/g_wantPitch = body,
  since command +28/+32 carry the server's view); fire 450 ms after 900 ms;
  check 2 s: up -> "REVIVED", else retry (3 tries, logs view pitch and
  record+2948). Respawn: dead bots skip the 4 s respawn while MedicComing,
  up to 20 s ("calls for a medic").
- Untested. If revives never take: compare record+2948 (nobody hint set =
  trace missed: closer/lower aim), or the session message for bot targets.
- Also: texture_dump = 1 set in the game's CoD3Recomp.ini for Justin's
  campaign menu pass (used-campaign.txt). Upscaler rebuilt in the scratchpad
  without torch: esr/esrgan_np.py (numpy RRDBNet, loads RealESRGAN_x4plus.pth
  directly; 256x128 in ~50 s; matches the old 0.5.169 upscale within 0.6/255).
  PyPI is blocked in this session; github release downloads work.


### 215 SOURCE CHANGE (Claude, 2026-10-18): grenade button search; no console; EntityList player block; campaign used list

- **Grenades.** 0.5.213 log: bot throw traces show presses of 0x80 (frag) and
  0x2 (smoke) spending nothing (record +160..+215 unchanged). Host button log
  in that match: 0x4000, 0x8000, 0x200 presses, never 0x80/0x2. (Handoff 170's
  kbutton table says 0x80 = "class (grenade)" and 0x8000 is set by
  CL_FinishMove; the throw evidently is not plain 0x80 for a server-fed bot.)
  bots.cpp now: `BotNadeAmmo(base, record, smoke)` sums clip+spare
  (def+1468 -> record+4*(172+i), def+1460 -> record+4*(80+i)) over weapon defs
  whose lowercase name contains grenade/nade/mills/frag/stiel/sticky/mk2/m24
  (smoke counted separately). `NadeButtonSearch` g_fragSearch {0x4000,
  0x8000, 0x80, 0x200} / g_smokeSearch {0x8000, 0x4000, 0x2, 0x200}: each
  throw presses the current candidate for 350 ms, 1.5 s later the ammo is
  re-read; drop -> `found` (logged "bots: frag button found: X"), unchanged
  -> next candidate ("spent nothing; trying the next"). Trials where the bot
  was dead/late (>2.5 s) or ammo rose are discarded. Frag and smoke never
  share a found bit. No ammo -> no throw ("has no grenades left").
  Host learner (after the host throw trace): ORs host buttons over the last
  90 frames; when the host's frag/smoke count drops, logs "bots: host spent a
  frag (a -> b); keys pressed before it X" and adopts the first candidate in
  X. Throw trace now triggers on g_throwUntil (any candidate), not 0x82.
  If no candidate works: the throw likely needs a weapon word / offhand
  state in the command (+8) or the 0x200 local-client flag; compare the
  host's "throw trace host" lines around his "host spent a frag".
- **Console.** CMakeLists: CoD3MP WIN32_EXECUTABLE ON + /ENTRY:mainCRTStartup
  (main() unchanged). main.cpp (COD3_MP): AllocConsole + CONOUT$/CONIN$ when
  `--console`, COD3_CONSOLE!=0, or `<exe dir>\game\default.xex` is missing
  (first-time installer prompts). Run() accepts `--console`. kernel.cpp
  Kernel::Exit waits for Enter only when a console exists. log.cpp already
  handled FILE_TYPE_UNKNOWN (stdout -> NUL, log file kept). Campaign CoD3.exe
  unchanged (still a console program).
- **Player limit finding (classifies catalog loop_24 at 0x824B1BE0).** In
  sub_824B1B28 (called from sub_8258F0E8, sub_825909D8 [g_reloading],
  sub_82590AC0, sub_82593FA8 [level load]) r3 = *(0x82BCC7FC) (EntityList):
  `li r10,24; mtctr; stw 0` clears words +4..+96, then `stw 0,100(r28)`.
  So EntityList+4*(1..24) = the 24 player entities (bots use
  entities+4*(slot+1)), and +100 (index 25) is a special level/world entity:
  41 direct `lwz rX,100(EntityList)` sites (camera FOV, "global"
  script_model, g_gravity, wbk_loaded, map_restart, sub_82587268 player died,
  tank mantle...). >24 players cannot simply use indices 25..32: either move
  index 25's object (41+ sites) or map players 24..31 to new indices. TRUE
  player loop; must change together with the entity-number audit.
- **Campaign upscales.** texture_pack.cpp: the campaign writes
  textures\used-campaign.txt (MP keeps used.txt), so MP runs stop
  overwriting it. Ask Justin: texture_dump=1, browse the campaign menus (not
  movies: dumping during movies hangs), quit, set it back, send
  used-campaign.txt; upscale NOT-REPLACED 512+ menu art a couple per build
  (Real-ESRGAN x4, keep the dump name, textures\replace). The ESRGAN setup in
  the old scratchpad is gone; reinstall (pip torch cpu + RealESRGAN_x4plus).
- Syntax: bots.cpp 0 errors (-DCOD3_MP, stubs); main/kernel/texture_pack:
  stub-only errors (freopen_s, VirtualAlloc, wchar paths). NOT BUILT.


### 214 PREPARED (Codex, client date 2026-10-17): bounded join scan; roster slots 24-31 addressable

Original CoD3Scan disassembly now resolves all catalog addresses; do not count generated C comments. Verified the opcode/immediate shapes for 160 comparisons, 49 candidate stores, 3 supposed loops, and 66 roster adds. Two supposed loops are indirect calls (mtctr uses r11, not the register loaded with 24).

Found and patched two prerequisites: the six-wide join scan in sub_824046D8 would read slots 32-35 if its bound alone were raised; it now uses a bounded search and retains the existing full/setup/connect paths. The 0.5.213 roster mapper excluded slot 24+, so explicit roster calls now use a separate 32-entry mapping API while generic adjacent-field classification stays at 24. Bot/player active limits remain 20/24 through the new player_capacity.h.

Source version 0.5.214 prepared; installed build still 0.5.213 at this checkpoint. Python patch tests and existing storage patch tests PASS; actual generated patch applied. C++ tests/build/install/runtime PENDING until later validation entries. **Resume at docs/player-limit/RESUME-214.md and docs/build-logs/validation-214.txt.** Exact backup: chat work/before-214; original audit: docs/build-logs/player-cap-original-sites-214.csv (local only).


### 2026-10-18 PLAYER-CAP AUDIT (Codex): 32 remains plausible; audit mapping needs repair

- The 32-player target remains plausible: three storage layers now have 32-entry backing, and the study identifies 32-bit session masks as the protocol ceiling for this target. The 0.5.213 solo test validated storage integrity only; active cap is still 24.
- Current bot console request is clamped at 20 in `BotsAsk`; `bots.cpp` still uses `MaxClients = 24`. Thus the last solo match did not exercise slots 24-31.
- Rechecked `sites.csv`: 1,107 rows; 66 session base offsets plus one server allocation are marked done. Remaining categories and counts are recorded in `docs/player-limit/AUDIT-20261018.md`.
- Found an audit reliability issue: the catalog's fourth field is `status`, whereas its generator emits original instruction text. More importantly, simple function-offset mapping against current generated C can misalign after prior generated-code edits. Do not use index-only output to rewrite 24s. Reconstruct and validate exact PPC instruction context before classifying or patching sites.
- Entity numbering, UI capacities, per-client animation/render storage, join/leave/respawn and local-player assumptions remain open. No source code or build changed in this audit.
- Next: repair the mapping using retail PPC instruction order and patch history; classify player sites; audit the host/joiner and UI paths; then prepare a guarded integration stage before enabling slots 24-31.


### 213 SOURCE CHANGE (Codex, 2026-10-17): relocate the session roster

- The user's 0.5.212 match log validated the prior storage stages: all 24
  player records remained in the relocated 32-record block (zero stale
  pointers, spare clean, guard clean); server-client slots 24-31 remained
  unchanged. User reports the scoped Lee-Enfield now works normally on 212.
- The next 16v16 prerequisite is the session's 24 x 784-byte roster at
  session-object +0x1010. Added `CoD3Host/session_player_storage.h` and
  `MpSessionPlayerAddress` in `mp_fixes.cpp`: allocate 32 records in extended
  memory, copy the original 24 on first access, keep an A7 guard, and redirect
  only effective addresses inside the original roster. Host bot/session helpers in `bots.cpp` also use the relocated roster accessor. The outer session
  object's other fields remain at their original offsets. `COD3_NO_SESSION_RELOCATION=1`
  provides a rollback path.
- `scripts/patch_recomp.py mp` reads the site catalogue and redirects all 66
  roster-base additions in 53 generated functions. It validates every
  function/site before writing; the 41 stride users keep their existing 784
  stride and follow the redirected roster pointer. The active player cap,
  bot cap, sentinels, session loops and UI remain 24. This build does not
  enable slots 24-31.
- `MpFixesFrame` checks the eight spare records and guard every ten seconds.
  `tests/session_player_storage_test.cpp` covers address mapping;
  `tests/session_player_patch_test.py` covers catalogue patching, idempotence
  and abort-before-write on a mismatch.
- Build/tests and final binary hashes: see `docs/build-logs/validation-213.txt`. Corrected 0.5.213 is installed locally and packaged. Solo runtime checks of the relocated session roster, bot session, respawns and map transitions remain pending; a second PC is not required for this stage.

### 213 SOLO RUNTIME (Team Native, 2026-10-17): storage clean; bot transition follow-up

- The 0.5.213 log records the 32 x 784-byte session allocation and 30 clean
  copies of all 24 active records; spare entries and guard remain clean. The
  existing player-storage check also reports 24 relocated, no old pointers,
  spare clean and guard clean during the active match.
- A bot session reached 20 bots on `mp_merv`; the match changed to `mp_fuel`.
  The roster then reported zero bots and later three. A later `bot_add 20` was
  ignored because this client reported session number 24. The existing notes define
  >=24 as no local place, so this may have happened after active play. Check bot
  counts through a hosted map change before treating this as a defect.
- The run lasted about five minutes with no fatal/crash signature. This validates
  the roster relocation for this run only; the active cap remains 24. Respawn
  behavior was not explicitly confirmed by the user. Full details are in
  `docs/build-logs/validation-213.txt`.
### 212 LIVE TEST (Team Native, 2026-10-17): scope restored; storage checks clean

- User reports Lee-Enfield scoped ADS now works normally on the installed
  0.5.212 build after `Render::LogNextFrame()` was removed from ADS entry.
- The match log confirms 24 relocated player records, zero old-array
  pointers, clean spare records/guard; it also repeatedly reports server
  client slots 24-31 clean (0 bytes changed). This validates the storage
  reserves for that run only; it does not allow additional slots.
- Player cap remains 24. Session-roster relocation is staged in 0.5.213; solo runtime validation is required before raising any gameplay limits.

### 212 SOURCE CHANGE (Codex, 2026-10-17): make scoped-ADS tracing nonintrusive

- The 0.5.210 live log shows Lee-Enfield ADS transitions immediately followed
  by `Render::LogNextFrame()` captures. That call forces a full renderer frame
  dump at the exact action being measured, adding logging/render work during
  ADS. Removed the forced dump from `MpFixesMg42View`; lightweight ADS
  transition/settings traces remain.
- Compared with the friend's `G43-0.5.900-for-friend (2).zip` source and
  shared handoff: the Lee-Enfield setup is unchanged in the current source.
  It copies `kar98_sniper`, keeps the Laison view/scope model, applies the same
  77 `LeeScopedFields` values (including zoom 15 and 0.42 s zoom-in), and
  inherits the same overlay pointer. The friend recorded an offline match
  where the weapon worked. Recopying those weapon definitions would not
  restore the current behavior; preserve them and investigate post-merge
  ADS/render behavior.
- This is a diagnostic-isolation change, not a confirmed scope-rendering fix.
  The captures contained a valid overlay pointer and changing render targets,
  but no explicit black-frame marker. The first black-scope report appears in
  the 0.5.208 handoff after the 0.5.206 merge; no intentional Lee-Enfield
  scope-field change is recorded there. During the current 0.5.210 test the user
  reports the zoom partly works, the outer scope mask is not fully black, and
  it feels like half of the ADS scope animation is missing. The overlay and
  reticle fields are copied from `kar98_sniper`; compare the stock scoped
  Kar98 and Lee-Enfield before changing overlay/animation data. Test repeat
  ADS with 0.5.212 and compare frame-time logs. Do not install or overwrite
  while CoD3MP.exe is running.
- Source 0.5.212 was built and installed; the local match result is recorded
  above. Build/package hashes are in `docs/build-logs/validation-212.txt`.

### 211 SOURCE CHANGE (Codex, 2026-10-17): cache world-model vertex bounds

- In `CoD3Host/mp_fixes.cpp`, `WorldFitFrame` ran in the frame hook and called
  `VertexBox` for each kept campaign mesh every frame. That function rescanned
  every saved vertex even though `Keep` had already preserved the original
  data. `WorldMesh::originalBox` now caches those bounds as each mesh is
  captured; repeated frame fits reuse the box. The fit still detects reloads
  through `Keep`'s buffer identity and first-vertex checks.
- This targets avoidable CPU work behind the reported ADS/game lag. It does
  not address the black screen: the 0.5.210 trace confirms a non-null scope
  overlay pointer but does not identify the rendered black frame.
- Release build, `world_model_fit_test`, and friend package pass; see
  `docs/build-logs/validation-211.txt`. Local install was blocked because
  Windows denied replacing CoD3MP.exe. The port folder was restored to 0.5.210.
  The 0.5.211 package is in the Codex task outputs. In-game validation pending.

### 210 SOURCE CHANGE (Codex, 2026-10-17): server-client spare bounds probe

- scripts/patch_recomp.py mp now validates sub_825336F0's allocation and
  initialization sizes, changing both from 0x1D400 (24 x 4,992) to 0x27000
  (32 x 4,992). All existing player loops and MaxClients remain 24.
- MpFixesFrame marks the extra eight records and scans them every 10 seconds.
  Any changed bytes and their offsets are logged. This is bounds-check only;
  no extra slot is enabled. Release build/install PASS; executable hashes
  match. Friend package: outputs/CoD3MP-update-0.5.210.zip.
- Next: solo 20-bot match to confirm the spare eight stay clean, then joined
  match for the 0.5.209 mirror fix. Do not raise the player/bot cap before the
  session layer, sentinels, remaining client loops and UI are expanded.

### 209 SOURCE CHANGE (Codex, 2026-10-17): joiner mirror spawn dispatch

- The user confirmed both players intentionally closed the 0.5.208 game early
  because it was laggy; this was not a reported crash. Friend log ended during
  live execution, as expected. Do not infer a crash from its ending.
- The log confirms intermittent frame-rate stalls: 23/286 one-second FPS
  samples are below 60, including 0-1 FPS at 79-81s, 0 FPS at 90-93s and
  0-1 FPS at 108-113s. Final sample 76 FPS is not representative. File reads
  and allocated memory stay flat through those later windows; the log has no
  network RTT. Lag is real but still unlocated. See the task output review.
- User also confirms host-bot frag/smoke animations remain invisible.
  CoD3MP (14).log contains zero `throws a grenade`, `throws smoke`, `throw
  trace slot`, or `throw trace host` lines. It cannot diagnose a throw that
  never occurred during that match.
- Root cause found for mirror spawn failure: on the joiner, each mirror has a
  session entry. Spawn dispatch sub_824D0AF0 routes session-owned players to
  the original network path; the host-owned bot therefore never gets placed
  in the joiner's game. Change: recognize roster-marked mirror entries by
  session number and route only those through the existing local bot path
  (sub_82593608, model setup, callback +3312 finalizer, animation rig). Scan
  remote slots 1-23 as mirrors can be assigned below slot 4. Mirror callbacks
  use BotClientScope's spare local index 3 and restore the joiner's client/view
  state on exit.
- Improve throw trace: current command +8 was printed before it is populated
  with the selected weapon, so its value was always the cleared zero. Move
  the diagnostic after final weapon selection. Still no gameplay change to
  grenade input until a host-vs-bot trace identifies the missing field/state.
- 0.5.209 Release build and install PASS; SHA256 matches build. Friend update
  package: outputs/CoD3MP-update-0.5.209.zip. No match test yet. Next validate
  a joined match with bot throws, mirror health/state and respawns; capture
  render timing/call context during the low-FPS windows. Keep the active-player cap at 24:
  server-client slots/list and session/UI/sentinels remain unexpanded.

### 208 FRIEND MATCH RESULT (Team Native, 2026-10-17): intentionally ended early due to lag

- User supplied Downloads/CoD3MP (14).log: 0.5.208 on Rouen. Last complete
  time marker 286 s, 76 fps, active player. The user clarified his friend
  closed first, then he closed his own game early because it was laggy. The
  file ends midway through a live diagnostic snapshot because they ended the
  match. Four early
  0x4001000A/06 debugger exception codes; twelve outside-allocation reports
  are all during initial map load. No bad calls or effects-pool failures.
- Relocation: 30 post-map checks: relocated 24, old pointers 0, spare and
  guard clean every time. Player limit remains 24.
- Lag: 23/286 FPS samples below 60. Severe clusters at 79-81s, 90-93s and
  108-113s reported 0-1 FPS; final sample was 76. No RTT measurement. File
  reads and allocated memory stayed flat in the later stalls, so cause is
  unresolved.
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
- Release build/install PASS; world-model and climb tests PASS; patch tests
  3 PASS. Eder Dam automated bot smoke through 84 s: relocated 24, old
  pointers 0, spare/guard clean. Full matches/map changes/System Link still
  untested. See docs/build-logs/validation-208.txt and smoke-208.log.
- Next: run a 20/22-bot match and change maps, look for player-storage lines
  (old pointers 0, spare clean, guard clean). Then enlarge 4992 server-client
  list separately; session 784 records/sentinel/UI still follow afterwards.

### 207 SOURCE READY (Claude, 2026-10-17): mirror lives over C3BS; balanced roles

- 0.5.206 friend log ("freeze then disappear"):
  - Death watch: every mirror's joiner-side record (entity+512) was
    health 0, state 0 all match. The host's player copy was 100/3, and its
    deaths run on the joiner: "player dies (sub_82587268): host 1 killed by
    bot 8", then PLAYER_DEATH_FLYING, DeathState.
  - The session block +0x100.. is live movement state (changes constantly),
    not a death record.
  - Each machine computes deaths of its live copies; bot copies were never
    spawned there. The host plays bot deaths the no-session way (0.5.167,
    sub_823FD118 r3 -> zero), so no session death message exists for bots.
- Fix:
  - Host: g_botLife[slot]++ after each Respawn spawn. The
    sub_82587268 hook captures r4 inflictor, r5 attacker, r6 damage, r7
    means, r8 weapon, r9 dir pointer (3 floats), r10 hitLoc, and the ninth
    arg at r1+0x54 (the routine reads it at 468(r1) after stwu -384).
  - SendBotStates: "C3BS", version 1, header 8 + 18 bytes per bot (number,
    alive (health>0 && state 3), life, killer session number, means, weapon,
    hitLoc, side, int16 damage, int16 timeOffset, int16 dir*30000), every
    250 ms. Dispatched in lobby_rules.cpp -> BotsOnStates.
  - Joiner (MirrorHostBots): a copy not alive here whose host life is new
    (or never tried) is spawned via MakeString(allies/axis) +
    SpawnAtPoint(sub_822029F0) with its entity+484 handle (retry 1.5 s).
    If the host says dead and it's alive here (once per life): call
    sub_82587268 with the copy, killer entity (session number -> joiner
    slot -> entity; else self), and the host's values (dir in a 64-byte
    block, ninth at r1+0x54).
  - Log lines "mirror number N spawned here ..." / "... dies here as on
    the host ...". COD3_NO_MIRROR_LIFE disables it. The death watch now
    logs only health/state transitions (cap 600).
  - UNTESTED risks: spawn side effects on the joiner (class code, current
    client 0); the joiner may also kill copies itself from shots (then they
    stay down until the host's next life).
- Roles: per side, the role furthest below the target mix (25/25/28/22)
  goes to each new bot (was a clock-seeded roll: 7 camper, 8 flanker, 4
  objective, 1 rusher of 20).
- 0.5.206 host live (mp_fuel, 20 bots, 366 s):
  - map knowledge 897 -> 1039 places, 2018 -> 2530 ways
  - 15 kits used (campaign guns included)
  - stuck 72 (was 272 on 0.5.205)
  - War flag lines 55; spread median 231-551
  - grenades 50 / smokes 36
  - vehicles tried 30x (19 give-ups in fights); medic packs used 3x
- Grenades: 0.5.206 had 0 weapon events for any grenade/smoke (50
  "throws a grenade", 36 smoke). 0.5.204 had 6 sticky throws (bursts of 21
  events = one throw seen per client) for 71 attempts. Kits do hold frags
  (kit lines: fraggrenade(1/0), mills_bomb(1/0), stickygrenade(2/0)).
  Command +4 does carry 0x80 (written at the command build). Unknown why
  Pmove doesn't throw: suspect a command field the client fills on a
  throw (offhand/weapon byte, cf. cl_grenadeButtonDown in sub_8252DB50) or
  a weapon-state gate. "bots: throw trace slot N" (first 12 throws, 1.2 s
  of frames: buttons, command +8, record +160..+215 and +1056..+1071) and
  "throw trace host" (host's own 0x80/0x2 presses, same words, plus
  command +12..+23) to compare.
- Syntax: bots.cpp, lobby_rules.cpp clean. NOT BUILT.

### 206 SOURCE READY (Claude, 2026-10-17): Climb rises bounded by landing tilt; gclient array found

- 0.5.205 live (Justin solo host, mp_fuel War, 20 bots, 366 s, quit):
  - aps pools made (8192 effects / 4096 nodes); peak effects 329, nodes 516;
    0 aps errors. No crash. 7 bad calls.
  - 135 "plays War's flag" lines. Spread median 348-622 (was 154-471).
  - BUT 272 "stuck" strikes (0.5.204: 51 in 472 s); slot 12 stuck at
    -356 -450 -62 with 8+ strikes.
- Cause: Climb let a 48-unit piece rise 18+48 onto any walkable (flat)
  top, so crates and pipes of 20-60 passed. Fix: allowed rise per piece =
  18 + piece*tan(landing tilt)*1.1 (fall 30 + same); tilt from the
  floor normal. test_bot_climb_206.cpp adds 24/40/60 crates rejected;
  190/196/195 tests PASS.
- Player limit (0.5.205 probe): the 24 records (entity+512) are a
  contiguous array at 0x82BF21D0, stride 2992, ending 0x82C03A50.
  0x82C03A50 is itself referenced (no gap). Entities are at 66E0F440 etc.,
  step -1072 (heap blocks). Direct addressing of the array: sub_82593FA8
  (init: timescale/checkpoint strings; base and +2992 steps), sub_82595118,
  sub_82506540, sub_82594FD0 (uses 0x82C03A50); stride-only users:
  sub_825611D8, sub_8256CF80. Next: relocate to a 32x2992 block
  (allocate, patch these sites), plus the server client list (4992 stride)
  and the session layer.
- NOT BUILT.

### 205 SOURCE READY (Claude, 2026-10-17): aps pools x2, hill climbing, War flag teams, death watch

- 0.5.204 test (Justin host + friend, Merville War, 22 bots): no endon
  freeze ("pool:" bins at most ~30%, 0 fallbacks). Host crashed at 472 s.
  - From 353 s the native error queue fills with "apsMemory : pool(size=128,
    count=4096) is empty" (caller 8210A504 = sub_8210A4C8, the aps pool
    alloc), "apsEffect : pool is empty" (sub_820FD110) and "apsGroup : alloc
    failed" (sub_82109310).
  - From 434 s, bad indirect calls in sub_82110FD0 <- sub_820FC850 <-
    sub_824472C0 <- nglListSend sub_82142248 (450 of them). Fatal unhandled
    C0000005 at 472 s. The joiner had 0 aps errors.
- aps sizes: sub_82430A58 calls the setters sub_82109F38 (group memory
  0x200000), F48 (nodes, 2048 x 224 B), F58 (effects, 4096 x 128 B) and F68
  (1024). They fill the table at 0x825B0FD4 (+0, +4, +6, +8 int16).
  sub_8210ACB8 then builds the pools:
  - nodes: sub_82110608, holder *(0x82A89474)
  - effects: sub_820FD090, holder *(0x82A8786C)
  - pool = holder+8: +0 count, +4 used, +8 peak, +20 size, +24 data, +28
    int16 free list (so max 32767)
  - memory comes from the aps allocator *(0x825B0E80) vtable+4 ->
    sub_824372A0(size, align, 2)
- Fix (mp_fixes.cpp, end of file): overrides of F38/F48/F58/F68 double
  each, capped at 16000. sub_8210ACB8 is wrapped to log whether the pools
  were made. LogPool appends effects/nodes "used of count (most)".
  NOT root-caused: load versus leak. Watch the "most" numbers.
- Also in that log: 46 bad calls sub_823C9F00 <- sub_82403840 <- our
  per-bot sub_82404580 update (0.5.167). *(r30) is 0, so vtable+72 is
  garbage. Not fatal (skipped). Same path as the death block send.
- Hills: BotNavigation::Reachable = Flat (the old test) || Climb. Climb
  splits the step into at most 4 pieces of 48 or less: up-sweep of 18+len,
  across, then down-trace to -30-len; floor normal.z >= 0.65.
  - Test docs/build-logs/test_bot_climb_205.cpp PASS: 10-40 degrees up and
    down, 5 degrees in 4 queries, 60 degrees/walls rejected, stairs kept,
    bounded Detour up 25 degrees.
  - test_bot_navigation_190, unstick_196 and failed_routes_195 still PASS.
  - When no step was reachable, the bot stood (forward=0); that's the
    "struggle" on hills.
- War: the War block in the brain (after Domination::BotGoal) replaced.
  Quota per side = clamp(round(40%), 3, 7); order is objective, then rusher,
  then flanker, by slot; campers are excluded. Even rank = taker (20-90
  from the flag, atFlag < 70). Odd = screen 200-450 out towards
  SpawnCentre(enemy), +-47 degrees. Takers' toFlag holds while the enemy is
  over 600 away (was 900). The Domination ring stays (skipped when
  g_war.valid). Log line "bots: slot N plays War's flag ... as a taker or
  screen".
- Death animations on the joiner: friend's 0.5.204 log, every mirror's
  record health is 100 all match, no death steps for bots. The host fills
  the bots' death blocks (+0x100..+0x13C; dumps compared). New in
  MirrorHostBots: "bots: death watch here:" logs, for each mirror and the
  host's player, block changes plus health/state (+304/+1924) transitions
  (cap 400). Next: compare the host's player (whose deaths show) with the
  mirrors.
- Player limit CORRECTION: 0x82C2ECF0 + 4*6352 = 0x82C35030 (HostInput),
  and the gun views 0x82C37070 lie in "record 5". That array is the 4
  LOCAL clients (cg-side, 6352 B), not the 24 server players. 503 constant
  sites in 174 functions (docs/player-limit/player-state-sites.csv): almost
  all record 0 (base) plus 4/5 (HostInput, gun views). PlayerLimitProbe now
  logs each slot's entity and record (entity+512) with steps, once
  per match with bots, to find the real 24-client array and its size.
- Syntax: bots.cpp clean; mp_fixes stub-only errors (VirtualQuery). NOT BUILT.

### 204 SOURCE READY (Claude, 2026-10-17): script-pool exhaustion freeze fixed

- 0.5.203 log: bot_add 24 at 133 s, fps 0 from 149 s. Watchdog: guest
  thread 0 parked in sub_824C0AE0 (callers sub_824C9258 <- ... <-
  sub_822029F0 (our Respawn) <- sub_82580080). Last lines: "slot 20
  respawns", "spawning as allies", "before" with no "after". (sub_8231E0E0
  in the stack is memset, stale.)
- sub_824C0AE0 walks the running script thread's record list (+24, sentinel
  +32), looking for type 3 with key (+20 of the link). It is the endon
  built-in's check: MP built-in table 0x825FF0E0 (offset, func): 0x4C wait
  sub_824C4840, 0x50 waitframes sub_824C4910, 0x54 sub_824C9350, 0x58 endon
  sub_824C9258 (cf. docs/builtins-table.md, SP 0x58 = endon). The thread
  object is *(0x82C72778+2132).
- Those four allocate from the game's small-block pool *(0x82BCBF5C)
  (sub_820E0D40(pool,size,0)) and don't null-check: with 0 they link
  "0+4" into the list, which turns it into an endless cycle.
- Pool: made in sub_8254A758 (sub_820E0B88(..., 2)); its pointer is also at
  0x82B3E8F0 and 0x82BCxxxx -16940/-16964/-16792/-16960/-16968/-16880/-16560.
  Bins 20x4096, 32x2048, 64x4096, 128x1024, 256x450. Pool layout: +0 bin
  pointers, +36 int16 sizes, +52 count, +56 flags. Bin layout: +4 total, +8
  free, +20 peak, +24/+28 range, +40 free list. A request tries every bin it
  fits. Flags bit 1 (heap fallback when the bins are full) is off, so it
  answers 0 ("all pool exhausted", the print is empty in retail).
  sub_820E0E60 (free) sends a pointer outside every bin range to
  sub_820DE980 (heap free).
- Fix (mp_fixes.cpp, at the end):
  - sub_820E0D40 override: on 0 it calls sub_820DE750(16,size) (the same
    call the pool's own fallback makes). It logs the first five, then powers
    of two, plus a "pool:" fill report every 30 s (checked every 16k calls).
  - sub_824C0AE0 reimplemented as the original, with a step cap of 100000
    and link < 0x10000 treated as corrupt (logs; answers 1 = present, so no
    insert). It also tracks the longest list.
- NOT root-caused: who fills the pool. Suspects: campaign archives and the
  sound engine (friend's 0.5.203), 24 bots' script threads, or a leak. The
  "pool:" lines will show it.
- FPS: 0.5.203 ran 11-13 FPS from 32 s to 112 s (render 70-90 ms, 365 ms at
  108 s) with ~2,000 draws, then 10-12 ms at the same draw count from 120 s.
  This was before bots. It is host render-thread time (guest samples look the
  same in both stretches). No counter tracks it (uploads were lower in the
  slow stretch). Next: per-phase render timing.
- Syntax: mp_fixes clean in the new block (stub-only errors elsewhere).
  Version 0.5.204. NOT BUILT.

### 203 SOURCE READY (Claude, 2026-10-17): Team Friend's second 0.5.900 merged

- G43-0.5.900-for-friend (1).zip: still a diff vs 0.5.193. Merged as the
  delta between their first and second package files (56 hunks) applied with
  patch -F2 onto 0.5.202: all applied (mp_fixes hunk 5, a declaration, and
  settings.cpp hunk 3, the mg34_no_deploy save line, with fuzz 1, both read).
  New: CustomWeapons +"mg42" (20) +"lee_enfield_sniper" (21); class grenade
  kind 4 sticky bomb; MpFixesMg42View (MG34 view-model body swapped per
  frame); mg34_no_deploy (default 1: stock MG34 gets the BAR's class +176 and
  no deploy anims); Laison archive + bank; map block 495 MB (sub_820E0158);
  sub_82554BA0 hook (silences the copy's shot events for lee_enfield_sniper).
  Their tests PASS here (campaign_sound, campaign_animation); list-order check
  PASS (only appended). bot_kits.h: mg42 + lee_enfield_sniper known, default
  kits "1 heavy mg42 2 1", "1 scout lee_enfield_sniper 1 2" (test_bot_kits_203
  PASS). BotsScopedWeapon matches lee_enfield_sniper (name has "sniper").
- Syntax: mp_fixes, class_editor, settings, bots clean apart from stub-only
  errors. NOT BUILT.

### 202 SOURCE READY (Claude, 2026-10-17): ADS, spawn-aware errands, overwatch, site catalogue

- Live 201 (Justin, Rouen, map known 528 cells): spread within-300 12/20,
  9/20, 9/18, 9/19 (avg ~49%), errands 68 (36 reached); holds fire 97;
  launcher shots 0 (one antiarmor bot of 20). Justin: own bots struggle to
  leave spawn, enemy bots never push to his spawn, wants flag checks,
  defensive/aggressive behaviour, snipers holding and watching flanks.
- Cause of no rockets: bots never pressed ADS. Command buttons +4: 0x8 is the
  sights (+speed; mp_fixes reads (command+4)&8). Launchers are ADS-only.
- bots.cpp:
  - Fire block: wantAds = visible && reacted && |yawErr|<20 && (launcher ||
    scoped || (flat>700 && !aggressive && !charge)); adsSince per slot; shot
    needs 350 ms of ADS (launcher 700); fire presses keep 0x8.
  - g_spawns[3] (32-ring per side, filled in MapKnowledgeTick on a
    dead->alive transition of any player), g_spawnedTick[slot],
    SpawnCentre(side). HeldDef(record).
  - BotExplore.purpose {Normal, Exit, Push, Flag, Overwatch} + watch[].
    ExploreStep: justSpawned (15 s, within 900 of own centre) -> Exit
    (Style::Away from own centre); sniper (held def scoped) -> Overwatch
    (High, lean 1.5 to enemy centre); rusher && slot%2==0 -> Push (Near
    enemy centre, 700); slot%4==1 every other errand -> Flag (Near War flag,
    350); else lean 1.5 to own (camper/form<=-2) or enemy (rusher/form>=2)
    centre. Fallbacks: reachable, within 1200, then Normal. Hunters run
    special purposes too. Abort radius: Exit/Push 400, others 550, start 700
    for special. Holds: Exit 0.5 s, Push 6-12 s, Flag 6-10 s, Overwatch
    35-60 s (prone + ADS when scoped; looks at Watch() points 4 s each).
- bot_map_knowledge.h: Style::Near/Away, Pick(..., towards, lean, nearRadius)
  (lean added after the 4.0 cap), Watch(from, count). test_bot_map_knowledge_202
  PASS.
- Player limit: scripts/player_limit_sites.py -> docs/player-limit/sites.csv
  (1,107 sites in 421 functions; kinds in PLAYER-LIMIT-STUDY.md; addresses
  only, no instruction text in the repo). Next: bounds-check build (svs
  clients 32 long, extra records poisoned and watched).
- Syntax clean (old DWORD warning only). NOT BUILT.

### 201 SOURCE READY (Claude, 2026-10-17): launcher single shot, no team fire

- Live 200 (Justin, Ederdam, new map): "bots: spread:" median nearest
  190/269/415/290/387, within-300 13/20, 12/20, 5/16, 12/20, 4/16 (avg 48%,
  was 56% on Rouen 199); errands 36 (14 high, 8 low, 14 outskirts), reached
  13, took-too-long 20; vehicles: 97 give-ups "a fight", 4 use presses, 0
  entries; packs: medic mp_revive / support mp_ammo_axis taken out and "used"
  (effect unconfirmed). Justin: bots kill teammates a lot; a bot's bazooka
  looks like several rockets.
- bots.cpp (before Think): BotHoldsLauncher (held def+8 name has bazooka /
  panzerschreck / panzerfaust / _rg); ForEachMate (living same-side players,
  chest = origin + 0.7 eye); MateInLine (point-segment distance, t in
  (0.02,1.02), clearance 30 + 3% of the distance along, z weighted 0.5);
  MateNear(radius, |dz|<160). Fire block: aimed && !mateBlocks && (!launcher
  || (flat>300 && now>=g_nextRocket)); launcher = one press, next in 2.5 s;
  mateBlocks = MateInLine || (launcher && MateNear 300) -> g_sidestepUntil
  600 ms (dir alternates), applied AFTER the final TacticalMove (it rewrites
  forward/right). Grenade throw also requires !MateNear(target, 320).
- Syntax clean (old DWORD warning only). NOT BUILT.

### 200 SOURCE READY (Claude, 2026-10-17): spreading the bots

- Live 199 (Justin, Rouen solo, 20 bots; G43 works per Justin): 980
  mobility samples: roles objective 539 / rusher 196 / flanker 147 / camper
  98 (11 of 20 objective); per 5 s round, median nearest-bot distance 225,
  56% of bots within 300 of another; densest 500-unit cell 170 samples (the
  War flag). Errands 37 (34 outskirts, 2 high, 1 low), reached 21. Stuck
  lines 57 (the clock fix works). Packs: 30 "bots: pack:" lines (not yet
  analysed for success). Vehicles: no presses (no vehicles on Rouen).
- bots.cpp:
  - role roll objective <25 (rusher <50, flanker <78, camper rest).
  - War goal only while objectiveRank < 4 (objective-role bots of his side
    with a lower slot); flag ring = golden angle, 120 + (slot%4)*80.
  - g_targetOf[slot] (set in Think after the target loop); rank *= 1 +
    0.8*(chasers-1) when 2+ same-side bots chase that enemy (not for the
    preferred/visible one).
  - lanes: final TacticalMove gets moveGoal = target + perpendicular *
    ((slot*7%5)-2)*260 * ramp((dist-600)/600) when found, !visible, !atFlag,
    !flanking, enemyDistance > 600.
  - Separation radius 220 when !state.visible (90 in a fight; was 72).
  - ExploreStep: on an errand (dest>=0 or holding) aborts only for a visible
    enemy or one within 550 (start ranges unchanged).
  - MapKnowledgeTick: "bots: spread:" every 30 s (median nearest same-side
    living player for each bot, share <300, bots on errands).
- bot_map_knowledge.h: Measure zLow/zHigh at the 15th/85th height
  percentile; High needs >= zHigh and >= zMid+32, Low <= zLow and <=
  zMid-32; height score /60. test_bot_map_knowledge_198 still PASS.
- Syntax clean (old DWORD warning only). NOT BUILT.

### 199 SOURCE READY (Claude, 2026-10-17): clock fix, vehicle use, packs

- Live 198 (Justin, mp_guns solo, ~15 min): kits OK (bot_kits.txt written,
  12 kits, custom primaries requested 48x, "kit grenade" counts set 20x,
  "kit extras: smokes" 30x); grenades 125, smoke requests 68 + "throws smoke"
  37 (capped by the shared told<60); errands 44 (24 reached); floor-ignore 2;
  strikes>=20 respawn 2; map knowledge mp_guns 687 cells. BUG: 30,847
  "stuck strike 1": BotSeats called TacticalMove with GetTickCount() while
  Think uses serverTime -> NetProgress since/lastStuck flip clocks every call
  (stuck twice a frame while walking to a vehicle, strikes never escalate).
  Vehicles: 45 claims, 40 "presses use" at BMW/Harley motorcycles, 0 entries
  (buttons were 0x20|0x400). Log ended mid-line (game still running at copy).
- bots.cpp: BotSeats(..., serverTime) -> TacticalMove(serverTime). ExploreStep
  sets ex.activeTick = GetTickCount(); MapKnowledgeTick's onErrand uses it (was
  comparing GetTickCount with the serverTime holdUntil).
- UseButtons = 0x20 activate | 0x40 activatereload | 0x400 reload (button map
  from HANDOFF 170). Approach to 75 (was 110). LookAtVehicle(): pitch toward
  vehicle origin+30 via BotAim::Pitch*PitchSign, 5 deg/frame, also while use is
  held. Log adds view-off and entity+900 link per try.
- NEW BotDispense (FeedBots after BotSeats): kit base 0 support / 3 medic;
  special = mp_ammo / mp_ammo_axis / mp_revive (strings in sub_821F3EC0 /
  sub_821F3DF0, both HealthAmmoDispenser); 2+ live teammates within 450, no
  visible enemy, not in vehicle/MG, first after 20-40 s then 60 s apart;
  stages: switch (mp_fixes MpFixesSwitchWeaponFor -> script +2692 on the
  bot's handle, found by scanning 0x82BD1FC0 for his entity, cached), 0.9 s,
  fire 0.4 s, 0.8 s, switch back to the gun that was in hand (def+8 name).
  UNTESTED: read "bots: pack:" lines; if the special is not in his
  inventory the switch does nothing (then check how _mp_loadout gives it).
- mp_fixes.cpp: German smoke gift "smokegrenade_axis".
- Syntax clean (bots.cpp only the old DWORD warning; mp_fixes only the 4
  stub errors). NOT BUILT.
- MERGED Team Friend 0.5.900 (G43-0.5.900-for-friend.zip, diff vs 0.5.193):
  applied with patch -F3 onto 199 (mp_fixes 23 hunks, offsets +57/+143;
  settings.cpp save-line hunk fuzz 3, checked by hand: both sniper_walk line
  and g43_reload present). New CoD3Host/campaign_animation.h,
  campaign_sound.h; tests/ (campaign_animation_test, campaign_sound_test PASS
  here; g43_selection_test.py needs git: same check done by hand, all five
  weapon lists only appended). CustomWeapons +"gewehr43" (index 19), class
  primary max 19 / sidearm 21. Forest archive = campaign archive 2, forest.wbk
  bank, sound engine given its own 36 MB physical (AllocatePhysical),
  OrderedCueLookup, CampaignDonorEvent, G43Fields[] (93 fields via the field
  table at 0x825D77D0). bot_kits.h: gewehr43 known + default "1 rifleman
  gewehr43 2 2" (test_bot_kits_199 PASS). Not touched by them: bots.cpp.
  native_commands must stay 0. UNTESTED in our tree: build, two-PC System Link.
- 199 first build FAILED (bots.cpp BotDispense): a local named `near` -- minwindef.h
  defines near/far as empty macros. Renamed nearMates. Never name anything
  near/far in code after <windows.h> (the Linux syntax stubs now define them).

### 198 SOURCE READY (Claude, 2026-10-17): bot_kits.txt, grenades, 197 test fixes

- Live 197 (Justin, Merville solo, 17 min, 20 bots, 0.5.197 built OK):
  map knowledge 1054 cells / 2405 links saved (bots\maps\mp_merv.txt 44 KB);
  errands 41 (12 routed, 29 straight; 22 "took too long"); vehicle claims 77,
  all "gone" at once (mine looked up before the claim); 0 mounted guns;
  grenades 75, smokes 16; slot 6 strikes 1..96 at 875 236 -8 (enemy 30-140
  flat but a floor below, pitch ~67, never visible). Player-limit probe: gap
  byte 0x82C54082 set after 1 frame, 6215 bytes set all match -> gap IN USE
  (player states must be relocated); session object on the heap (0x67070970).
  Scoped test BROKEN in 197: def+0x650 nonzero for every weapon (pointer to
  ""), so all guns ADS-walked at sniperWalk and all aimed sway halved.
- NEW bot_kits.h (test_bot_kits_198 PASS): Kit{weight, base 0..6
  support/light/rifleman/medic/heavy/antiarmor/scout (from 197 kit log),
  primary (mp_fixes CustomWeapons names), frags, smokes (-1 = class's own)};
  Parse/DefaultText/Select (team[i]/weight, then total). bots.cpp
  LoadBotKits(): exe-dir bot_kits.txt (written with defaults if missing,
  re-read when mtime changes); selection block in the bot add path; g_botKit[]
  copies; BotsPrimaryName/BotsBaseClass from g_botKit; new BotsKitGrenades
  (frags, smokes, american = BotSide==1). bot_loadouts.h keeps the old
  constexpr table (old tests) and declares BotsKitGrenades/BotsScopedWeapon.
- mp_fixes.cpp: sub_82206028 sets thread_local g_botSpawn/g_botFrags/
  g_botSmokes for bot kits; sub_821F28C8 writes "grenade"/"smokegrenade"
  counts for bots and notes which slots the kit had; kits missing one queue a
  BotGrenadeGift; MpFixesBinoculars gives it after the kit (script +2664
  give, +2864 clip count; side frag fraggrenade/stielhandgranate, smoke
  "smokegrenade"), slots 1064..1073 restored. UNTESTED: confirm "bots: kit
  extras" and that given smokes throw (SmokeButton 0x2).
- Scoped = BotsScopedWeapon(def): name at def+8 contains sniper/scoped or is
  springfield (move hook sub_82487718 and the sway block).
- Grenades: every 9-14 s (was 18-25), 350-1500 visible (300 when 2+ enemies
  within 300 of the target), 250-1200 for 6 s after losing sight. Smoke:
  health<=60, cooldown 16-26 s (was 30-45), evasive after 6 s; tactical
  g_wantSmoke when toFlag or charging >400 with an enemy within 1800 (every
  25-40 s).
- Floor-blocked enemy: found && !visible && flat<300 && |dz|>90 for 6 s ->
  g_ignoreSlot/Until 20 s (skipped in the target loop).
- BotSeats: progress.strikes >= 20 (and not in a vehicle) -> Respawn.
  Vehicle claim sets mine = best.
- MapKnowledge::Reach(from) BFS mask; Pick(..., reach, maxAway). ExploreStep
  picks reachable first, else within 1200. test_bot_map_knowledge_198 PASS.
- Syntax: bots.cpp clean (old DWORD warning), mp_fixes only the 4 known stub
  errors. NOT BUILT.
- Next: Support ammo / Medic health packs (they are class "specials" from
  sub_821EE6C0/7C8, sub_821F3DF0/3EC0 via sub_821FB7E0, not weapons with
  rounds: the 197 kit probe never listed them); verify vehicle entry/seat 0 =
  driver; real LOD; relocation plan for 16v16.

### 197 SOURCE READY (Claude, 2026-10-17): map knowledge, explore styles, MG/vehicle seats, sprint, sniper, HD texture tool

- Live 196 (Justin, solo): ladder approaches 11 (10 goal above), drops 40,
  strikes mostly 1-2. ONE frozen bot: slot 9 at 1197 1953 -99, strikes 1..60
  "another way", mobility move 0 0, view yaw stuck -176.9 while sent 124
  (aim off -103 for minutes). Cause: on a mounted gun (reload 0x400 = use on
  the X button). Player state +240 & 0x100000 = mounted; entity +900 link ->
  vehicle (Guest::Read32(link)), seat = ps+1300 (from the Kangaroo exits).
- NEW bot_map_knowledge.h (pure; test docs/build-logs/test_bot_map_knowledge_197
  PASS): cells 128x128x96 (Key cx,cy,cz), first-seen standing point, visits,
  deaths/kills/reached/failed, directed links on a move within 1.5 s / 260
  flat / 200 up (no teleport links). Measure(): visit-weighted centre, 70th
  pct radius = outskirts, z 25/50/75 pct. Pick(style) scores (kills+, deaths-,
  failed-, claimed within 500 -3, hash jitter). Path(): A* over links, a death
  costs 200. Save/Load text "CoD3 bot map 1 N".
- bots.cpp:
  - MapKnowledgeTick (server frame, after PlayerLimitProbe): Kernel::CurrentMap()
    change -> save old / load bots\maps\<map>.txt beside the exe; every
    500 ms Observe all 24 slots (walking only: not in vehicle, not mounted);
    alive->dead = Death(last feet), g_form-- for bots, style switch after 3
    deaths on errands (g_styleShift); kill credited to a bot whose
    g_lastSeenSlot == dead within 1.5 s (Kill at his feet, g_form++); save every 60 s.
  - BotStyle(slot): slot%10 table {Outskirts,High,Low,Hunter,...}, camper=High.
  - ExploreStep() called in Think right after the toFlag target block when
    !toFlag && !atFlag: errand unless an enemy is visible or within range
    (outskirts 900 / high 1700 / low 1100); hunters explore only with no
    enemy at all. Gives up on timeout (20 s + 15/unit) or 3 new NetProgress
    strikes (Failed()). Hold 15-35 s high/low (High odd slots prone), 2-6 s
    outskirts. Think: `exploring` sets target=look; idle block skipped;
    flank skipped; explore branch before the toFlag run branch (sprint far,
    TacticalMove to waypoint with its real z so ladder/drop logic fires).
  - g_aimOff (yaw error), g_form (+-5, decays 1/min): form>=2 aggressive,
    <=-3 defensive (not rushers).
  - BotSeats (FeedBots, after BotStanceControl): mounted -> remember MG spot,
    fight while target visible and |aimOff|<30, else press 0x20|0x400 (use)
    for 150+250n ms, 5 tries then Respawn(). Near known MG spot: strip 0x400
    (no accidental reload/mount); camper/High bots mount on purpose
    (cooldown 45-60 s). Vehicles (Settings botVehicles, default on): pool
    0x82BCBDB4, count int16 0x82C281D0+3088, 1872-byte records, handle +368
    -> 0x82BD1FC0 table; kind +376 -> 0x82B2DC90 name. Up to 2 bot-crewed
    vehicles a side, slot%3!=2, no visible enemy, within 1800 (600 beside a
    human driver). Approach with TacticalMove, at 110 press use (6 tries).
    Inside: seat 0 = driver ASSUMED (log "seat %d"); steer right=-turn*2.5
    and view yaw toward the War flag / an outskirts spot; reverse 1.5 s when
    <40 units in 2.5 s; out after 120 s or 6 stuck checks. Riders: Think aim/
    fire, no moves; out after 20 s with no driver and nothing to shoot.
    ALL UNTESTED: read "bots: vehicle:" lines. If use never enters, try 0x20
    alone or a longer hold; if seat 0 is not the driver, use the moved test.
  - BotKitProbe: weapons with rounds, once a life, 6 times a bot -> find the
    Support ammo pack / Medic health pack weapon (special slot) for next build.
  - PlayerLimitProbe: gap 0x82C54070..0x82C67CBC zero check each server
    frame, report every 2 min; session object static/heap once.
  - sub_82487718: scoped (def+0x650 overlay != 0) walk = sniperWalk (30%)
    even below the game's 40%; others unchanged.
- mp_fixes.cpp: quick sprint: dry -> stamina pinned 0 for 1.5 s, then 0.25;
  refill after 0.75 s at full-in-3 s (was half in 0.33 s, refill at 250 ms
  full in 1.5 s). Scoped weapons: aimed sway 0x6D0,0x6D8,0x6DC,0x6E0,0x6E4
  and aimed idle 0x69C = original * sniperSway% (50), every 2 s, originals
  kept per def (re-taken when values change). Weapon def field table found
  at 0x825D77D0 (354 entries: name, offset, type).
- Settings: sniper_walk (30), sniper_sway (50), bot_vehicles (1); F11 sliders
  + checkbox (overlay.cpp).
- texture_pack.cpp: dumps on a worker thread (256 MB queue cap, written as
  .part then renamed), cap 30000; NoteMap(key) -> textures\maps\<map>.txt
  (texture_dump only, >=32 px); VRAM total logged every 100 loads.
  render_resources.cpp calls NoteMap after Key.
- Port scripts (in the game folder, copies in port/): UPSCALE-TEXTURES.ps1/.bat
  (realesrgan-ncnn-vulkan x4 + texconv BC7 -m 0, alpha via System.Drawing,
  colour dilation before upscale, -keepcoverage 0.5 for hard alpha, normal
  maps and R8 skipped, out textures\replace\hd), HD-TEXTURES-README.txt;
  MAKE/INSTALL-FRIEND-UPDATE now carry vehicles.txt (friend never saw the
  Kangaroo: it is a local placement) and leave replace\hd out of the update.
- Found: model_detail (r_lodscale/r_lodbias) is a placebo; those dvar
  globals (0x82BA3314/0x82BA3344) are read by nothing but their registration.
- Syntax: bots.cpp clean (old DWORD warning only); mp_fixes, settings,
  texture_pack: only stub-environment errors outside the edits. NOT BUILT.
- Next: read 197 log (vehicle/MG/explore lines, player-limit probe), bots
  hand out ammo/health packs (from the kit probe), real LOD (XModel/SpeedTree),
  12v12 fill setting, objectives in CTF/HQ.

### 196 BUILT FROM SOURCE (Claude, 2026-10-17): net-progress unstick, separation, ladders, drops

- Live 195 (Justin, solo): prone WORKS (63x "prone -> standing" from eye 8;
  crouch eye ~25-30). Stuck problem: only 1 "stuck going" and 3 failed
  jumps logged, yet mobility samples show bots static up to 20 s with route
  1 / stalled 0 (jitter >12 units per 900 ms sample defeats stalledSamples).
  Pairs stuck on identical spots (slots 4/15 at 577 2081 -236; 10/14 at
  -1954 2329 -96). One ladder approach in the whole match.
- New CoD3Host/bot_unstick.h (pure, tests PASS):
  * NetProgress (3.5 s window, 56 units, strikes forgiven after 15 s quiet).
  * DropDown (64 out, floor 30..180 below, normal z>=0.65).
  * Wander (300..500 reachable, away from failed spots/goal line).
  * Separation (teammates <72 units, same floor +-64).
  * SpreadGoal (golden angle ring 40..100, inside War's 120 hold radius).
- bots.cpp TacticalMove:
  * goalZ=target[2] kept (planner flattens to feet) and g_goalZ[slot]; on a
    ladder forward=-127 when goalZ<feet-90 (down).
  * scanLadder lambda: 8 dirs x 160 units at +32/+72, 700 ms throttle,
    triggered by no step/stalled/strikes/goal above(>+140)/below(<-90).
    Replaces the 5-dir/80 inline scan.
  * NetProgress escalation: strike1 Remember+flip+escape; 2 JumpOver,
    DropDown, ladder; 3+ Wander 5 s (goal override).
  * Planner tries DropDown when goal below before the jump/detour.
  * Separation added to final stick (70*min(push,1.5)).
- Think: SpreadGoal on War/Domination goal when !atFlag.
- Logs: "stuck (no progress in 3.5 s, strike N) at x y z: tries ...",
  "approaches native ladder D away (goal above|below|level)",
  "drops down toward a lower goal (D below)",
  "stuck N times: tries the ladder".
- Risks: ladder DESCENT from the top still needs the bot to find the ladder
  face from above (scan at +32/+72 may miss it below the ledge); drops cover
  most high spots. Fall damage threshold assumed >180 units (unverified).

### 195 BUILT FROM SOURCE (Claude, 2026-10-17): failed-jump/stall route memory + combat brain

- Brain (same build, Justin's asks: more prone/cover when hurt, smoke for
  evasion, retreat, melee rush, flank, defensive/aggressive, vertical tracking):
  * Stance: BotCrouch only records a wish; new BotProne; BotStanceControl after
    BotExtras each command: closed loop on eye height record+220 (<24 prone,
    <50 crouch): hold 0x2000 up to 900 ms for prone (retry 1.6 s), tap for
    stand/crouch (0.7 s settle). Prone downgraded to crouch while moving,
    stand while jumping. HOLD-FOR-PRONE IS INFERRED from 0.5.89's note; if the
    log shows "-> prone" repeating with eye staying ~40-60, the hold length or
    semantics are wrong. ResetBotStance on death.
  * Target: top-3 nearest kept; if chosen not visible, every 300 ms trace LOS
    to the others and switch (g_preferSlot sticky 1.5 s, rank x0.25).
    Last seen pos (with height) kept 3 s; target = it while hidden.
  * Fight style: aggressive (rusher, or health>=70 vs target<=40), defensive
    (health<=45, camper). Ranges: closeIn 250/500/900, backOff 80/150/450;
    stop-and-shoot prone when hurt/defensive/>1100 half the time.
  * Charge (visible, <230, empty clip via BotClipEmpty / aggressive / +30 hp):
    forward 127, sprint until 110; melee reach 96 when charging.
  * Retreat (health<=35, visible, <750, 9 s cooldown, not in cover): 450 ms
    facing enemy with g_wantSmoke, then sprint 3 s to a point 420 away.
  * Flank (flanker or every 3rd objective slot with no goal; enemy 500-2200;
    not defensive): point 75 deg round at 0.6*dist (max 700); TacticalMove
    new moveGoal param; ends at 90 units or enemy <400 visible.
  * TacticalMove: proactive cover in long fights (seen >3 s, not rusher, 1/3
    of 2.5 s windows), cover 6.5 s when health<=35 + smoke if <=50; at cover
    prone if <=35 else peek (crouch/stand every 1.5 s).
  * BotExtras smoke also on g_wantSmoke if last smoke >=15 s ago.

- Justin's 191 report: bots fail a jump and retry the same obstacle. Cause in
  TacticalMove: a failed jump only set nextJump=now+8000 and pushed the
  landing into the 8-entry visits ring, which the 900 ms position samples
  overwrite in ~7 s; after 8 s JumpOver at the same spot passed again.
  g_tactical is also zeroed on every death.
- New CoD3Host/bot_failed_routes.h (pure, tested): FailedRoutes, 6 spots
  {start,target,until,strikes}; Remember (same place = start AND target
  within 72 -> strike+1, memory 45 s x min(strikes,4)); Blocks (target within
  48, or start+target within 72); NearFailedTarget; Tick clears when time
  goes backwards (new map).
- bots.cpp: g_failedRoutes[slot] + g_failedEntity[slot] (kept across deaths,
  cleared for a new entity). Failed jump -> Remember(jumpStart, step), flip
  hand, escape 3 s. Straight step stalled 3 samples -> Remember(feet, step).
  Direct step, JumpOver and Detour (new optional failed/now params; skips
  candidates on failed targets) all consult it. Logs: "navigation slot N
  jump at X Y failed (strike S, P places remembered): going round",
  "navigation slot N stuck going to ... (strike S): rerouting".
- Tests: docs/build-logs/test_bot_failed_routes_195.cpp PASS (memory, strikes,
  expiry, capacity, new-map clear, NaN, detour takes the other side of a
  wall); test_bot_navigation_190 PASS unchanged. clang syntax check of
  bots.cpp with stubs PASS. 194 roster code included, still untested.

### 194 BUILT FROM SOURCE (Claude, 2026-10-17): host bots replicated to joiners

- Test on 193 (friend hosts, Justin joins): host bot_add -> only host sees
  them. Joiner bot_add -> bots on joiner's scoreboard only, invisible. When
  both ran it, each saw "bots" (host's data landing on the joiner's local
  session entries 2..21 + joiner's own local AI). Logs: host
  CoD3MP.previous.log from friend (D:\cod3\CoD3-for-friend), Justin's CoD3MP.log.
  Both logs: humans are session numbers 0 (host, local place 0) and 1, bots
  get session players 2..21 = slots 4..23 on WHICHEVER machine ran bot_add.
- Cause: P2P design (lobby_rules: every machine runs its own server). A remote
  player exists on a machine only through a session player entry (object
  +0x1010, 24x784, indexed by session number) joined by sub_824046D8(object,
  player, &origin, &angles). Remote path of 824046D8: free svs slot,
  SetUpSlot 82532540, ConnectIntoWorld 82593608(e,origin,angles,0,0),
  state 1, record +1920/+1922 = -1 (kit set later by owner's class message),
  side from +604, BeginPlayer 8257A6C8, gamestate 82530720. Callers:
  82405158 (HandlePlayerSetup, mid-match, checks object+22933 started) and
  82407538 (match start; joins entries with +8 < 0; origin entry+496..+504,
  angles (0, entry+536, 0)). Host bots had no entries on joiners.
- 194: bots.cpp block "BotRoster" (before UavShowsBots decl).
  Host (ThisMachineHosts: no session, or local place byte object+0x5991 == 0,
  unless a roster from another machine came within 10 s) sends "C3BR" over
  Net::SideSend every 1 s: ver 1, host number, count, then 20 bytes/bot
  (number, slot, side, kit int8, name 15+nul). lobby_rules SideThread
  dispatches C3BR to BotsOnRoster (mutex copy). Joiner MirrorHostBots (after
  UavShowsBots in the SV_Frame hook, only when g_bots==0, roster < 5 s old,
  object+22933 != 0): copies the HOST'S entry on this machine (its holder,
  +4 refcount+1 = host machine) into entry[number], sets number, +8=-1,
  name +104, side +604, calls 824046D8 (vectors at new r1+0x200), then sets
  record +1920/+1922 to the roster kit while it reads 0xFFFF. Never touches
  an entry that is already a real player. bot_add refused on non-host
  ("bots: only the host can add bots").
- Logs to read: host "bots: roster sent"; joiner "bots: roster from",
  "given a session player here", "joined here in slot", "given the host's
  kit", every 5 s "bots: mirror number N slot S: health, at x y z, kit,
  drawable". Moving positions = host packets apply. Static = need state sync
  (fallback: host streams per-bot origin/angles/health over side channel).
- NOT handled yet: removal of mirrors when host bots leave (stale until map
  change; map start re-joins entries with +8<0 via 82407538). Entry copy
  carries the host's XUID/stats fields like the host-side bot copies do.
  Syntax-checked with clang (stubs) only; Justin builds. No GitHub push.


### 193 INSTALLED: Varied native/custom bot kits and primary weapon selection

- User confirms191 bots now move. Some still fail jumps and retry the same
  obstacle; explicit failed-route memory is the next separate patch.
- Balance11 loadout templates by team count, then total count: seven native
  kits plus existing FG42, Sten, Bren and Lee-Enfield custom-primary kits.
  Seven native classes remain0..6; custom primaries use suitable base kits,
  NOT invented out-of-range native class IDs. Bots retain their assigned kit.
- Enforce assigned base class inside native82206028 kit callback. Override
  its main-weapon name through existing WeaponName/CampaignStandIn handling,
  so native grant/ammo/model setup and map fallback remain in use. Humans'
  custom classes and sidearms are unchanged.
- Live191 proved slot order differs: rifle/pistol in either1065/1066, and
  previous comment claiming fixed secondslot primary was wrong. Examine both
  slots, reject Colt/P38 as primary, require owned valid definition and ammo.
  Request armed primary through normal usercmd; preserve sidearm fallback
  when primary runs out. No forced unlimited ammo or fabricated ownership.
- Tests PASS balanced20-bot distribution/all11 profiles, custom four present,
  both inventory orders, pistol-only/empty/unowned rejection and ammo fallback.
  MP/SP builds PASS;191 movement/aim and death/network/input hooks preserved.
  Four installed hashes match. Live loadout/model/respawn verification pending.
-192 prototype was never installed; this corrects its slot assumption before
  testing.189 peer visibility remains unconfirmed despite all20 native sender
  schedules advancing. No GitHub push/package/game file upload.

NEXT: user start hosted match/bot_add, verify all four custom gun names
in kit requests and held weapons; inspect native inventory/name/owned bits
after respawn. Scope bot_loadouts.h and BotsPrimaryName/BotsBaseClass APIs.
191 live/user movement confirmed; failed-jump reroute still requested and
being prepared separately (194). Keep capsule checks, no box regression.

### LIVE191 CONFIRMED - bots moving; loadout193 in progress

User confirms bots moving, some fail jumps then retry rather than reroute.
Initial telemetry all20 changed position;19/20 latest route1. Capsule191
is helping; do not revert to box mode. Still needs failed-jump route memory.
192 built/testsPASS, never installed: live inspection proved secondslot is
usually PISTOL (first rifle), sometimes reversed.193 checks BOTH slots using
names+ammo and enforces assigned base class in native82206028 kit callback.
Adds existing four custom primaries via existing mp_fixes WeaponName fallback,
not new assets. Seven vanilla+four custom templates balance by team/total.
Custom names user confirmed FG42/Sten/Bren/Lee-Enfield, already loaded.
NEXT AFTER193: failed-jump corridor memory and reroute (separate194 build).
Friend same188 all-bots invisible remains UNRESOLVED;189 clocks are only a
candidate fix, no two-PC confirmation. Records never claim peer success.

### 191 INSTALLED: Use native rounded-capsule navigation traces

- User190: many bots on both teams still stand at spawn. Live20-bot match
  confirms several living bots repeatedly have route0 and move0/0; others
  move.189 counters advance for all20 bots, but peer visibility is untested.
- Navigation body checks were box traces (r9=0). Native82571EA0 selects
  rounded-capsule mode with r9=1 into82534200, forwarded into824A0468.
  Use that mode for body clearance/support; point sight rays remain0.
- Add throttled completed-result trace logs, including actual fraction,
  solid flags, normal and hit handle. Read-only external scratch sampling
  can catch a query in flight; zero fields there are not completed results.
- MP/SP builds PASS; source checks verify capsule wrapper contract and
  unchanged190 AI/network/death/input code. Four installed hashes match.
  Startup/native live movement test pending; do not claim idle bots fixed.
- User also reports pistol spawns and requests diverse classes, including
  already-customized FG42, Sten, Bren, Lee-Enfield. All four are loaded in
  the current live MP weapon table (names verified). Loadout patch next.

NEXT: test native capsule movement on current map; capture completed trace
logs for still-idle bots and compare mobility positions.190 geometric tests
passed but failed to explain actual native idle behavior; native trace mode
was not modeled by those tests. Body bounds live verified[-15,-15,0] to
[15,15,70] on host and all20 bots. Current map had stuck6/14/16/17/18/23.
Do not infer no movement is resolved until live191. Keep189 network clock
isolation; friend all-bots visibility still open, needs same-build two PCs.
Next separate loadout patch: balance native classes instead of random0..6;
prefer owned class primary over pistol at spawn, preserve ammo fallback.
User explicitly requested existing custom guns FG42/Sten/Bren/Lee-Enfield.
Live names+8 confirmed IDs42..46 on this map (42Enfield43Bren44Sten45FG42
46scopedFG42); NEVER hardcode these IDs across maps. Resolve names per map.
Inspect existing mp_fixes custom class hooks before selecting extended kits.
Raw catalog/trace/runtime records local only. No game-file uploads.

### 190 INSTALLED - movement recovery; includes189 network clocks

USER188 host sees friend, bots, and dropped weapons on ground. Same188 on
friend's PC: ALL bots invisible, still lethal. Both teams have idle bots at
spawn. Ground weapon visibility host CONFIRMED; pickup usability and remote
drop visibility not tested. Archived188 runtime log includes friend match.

- Route checks now try normal feet height first; only use an18-unit step
  after checking upward clearance. Previously raising the entire hull could
  reject a doorway the standing player can walk through.
- If the96-unit detour has no route, try a bounded32-unit supported escape.
  Still require body clearance and floor support. No forced movement through
  walls, teleportation, invented ladders or paths across missing floor.
- Long-range shooting pauses now require a visible enemy. Hidden enemies
  no longer cause the bot to stop and crouch to aim through a wall. Campers
  patrol8seconds before a maximum3second no-enemy lookout pause, replacing
  the old40-60second spawn hold. Objective flag defense/cover holds remain.
- Add5second per-bot mobility diagnostics: role, health, position, requested
  movement, route availability, stalled samples and visibility.
- Geometry tests PASS: low doorway, too-low ceiling, cramped-spawn escape,
  enclosed/void rejection, wall detours, low jumps and ladder detection.
  MP/SP builds PASS.189 network integration and185 death/input/crash hooks
  preserved;188 drop rearm preserved. All four installed hashes match.
- Includes189 send-clock isolation. Startup PASS: visible190 MP menu144FPS. Hosted live movement and two-PC
  bot visibility pending. Local collision steering remains bounded; this is
  not a global map navigation mesh or completed squad-intelligence rewrite.

Startup smoke PASS:190 visible MP menu144FPS, System Link player selection
reachable. Automated Ready/A taps did not advance selection. Asked user to
start a hosted match and bot_add4; no further automated input pending reply.

NEXT TESTS: in a normal hosted match add4+ bots, observe spawn departure
and low doorway routes. Check mobility logs for requested moves versus real
position changes; route0 repeatedly means collision search failure, not a
successful hold. Test kills/drop rearm and host prone/input regression.
Friend must run190 too; verify both teams before/after respawns and latejoin.
Native189 replication counters count serializer/send CALLS, not delivery.
If friend still invisible, compare host189/190 replication counters against
friend session roster and remote state823EC930/82400810. Do not claim clock
isolation alone proves visibility. Bots copy host session entries; native
roster registration/peer receive still needs investigation if sends succeed.
Other unresolved: UAV compass markers, artillery, original corrupting writer
of renderer list82C89A60;185 guard only contains the resulting bad clear.
No GitHub upload or friend package created. Never upload game/generated files.

### 189 BUILT: Isolate each bot's native network-send timers

- User188 confirms host sees dropped guns. Friend running the same188 can
  see the host but ALL bots are invisible and can kill him. Both teams have
  bots stuck after spawning. Pick-up usability remains unconfirmed.
- Native82403840 uses CurrentClient-indexed full/near/far send timers at
  object+(8884/8888/8892+client)*4. All bots borrow client3, so one consumes
  that timer before the next bot runs. Save/load persistent timers per bot,
  restore scratch client's timers, reset on changed session/player identity.
- Add per-bot native serialization/send-call counters for diagnosis. These
  are calls, NOT proof of peer receipt, roster creation or rendered models.
- MP/SP builds PASS; scheduling test PASS for20 independent bots across
  3seconds, scratch restoration and changed identity reset. Think unchanged.
- Built checkpoint; not installed separately. Included in the next movement
  build. Remote visibility still requires a two-PC test; roster/receiver
  diagnostics may still be needed. No guarantee this alone fixes all peers.

Native send-throttle branches verified at82403BE4..82403E50. Full/near/far
clock offsets for scratch3:8ADC/8AEC/8AFC. Existing global85B0/861C timer
restore did not isolate these. New bot_network_clock.h stores each bot clock.
Do not claim friend visibility fixed without user test. Original188 drop
visibility confirmed by host; keep successful lifecycle fix.

### 188 INSTALLED - re-arm bot weapon drops after asynchronous respawn

USER187: no visible gun/pickup prompt. Initial native drops returned nonzero
items, but the requested fresh kill produced no new drop attempt. Native
respawn can finish after Respawn returns, so the healthy-return-only reset
left the previous life's attempted flag set. ObserveAlive now re-arms the
per-life state when ThinkBots actually observes positive health. Preserve
the existing dead-state else chain; no continuous respawning of living bots.
Native drop eligibility, model creation and replication remain unchanged.
Fresh creation now logs item handle, position, model, rig and expiry before
the item can expire or its address be reused. Old item addresses are not
proof of a fresh bot drop; do not infer ownership from stale memory.

Tests PASS: eight successive asynchronous respawn lives, one attempt per
corpse, independent bots, pre-death capture and owned-kit fallback. Source
checks PASS: aim/navigation/185 crash guard and narrow local-place override
preserved. MP/SP build PASS; all four installed EXE/PDB hashes match.
Startup and live repeated-kill visibility/pickup tests PENDING. Do not claim
visible dropped weapons fixed yet. Next: kill one bot twice across respawn,
check both fresh native-drop/snapshot records, inspect the fresh item promptly
if models/prompts still absent. Keep logs local. No GitHub upload/package.
Other open work unchanged: UAV compass markers, idle/navigation validation,
friend visibility, artillery and original renderer-list corruption writer.
Stop after this update as requested; resume from this checkpoint next time.

### 187 INSTALLED - native bot death weapon drop; live test pending

USER186: bots now seem able to kill them while prone (positive vertical-aim
result; high/low platforms not yet confirmed). Still no visible dropped guns.
Archive CoD3MP-0.5.186-prone-feedback.log;186 aim retained unchanged.

Drop investigation: native82580E38 first calls823E5CC0, which directly walks
this machine's4local-player places. Bots intentionally have none. Existing
bot-local823D9B50 hook cannot satisfy this gate. Native call only found in
network spawn receiver82405058; bots' custom Respawn bypasses it. PlayerDie
82587268 clearsheld164/entity+3 and never calls82580E38. So need both queued
native drop call and narrowly scoped local-place gate for that call.

187 captures an owned kit weapon in PlayerDie wrapper BEFORE native clears
held fields. bot_death_drop.h tracks one attempt per entity/life; reset on
slot creation and healthy Respawn. Dead-frame Think sets native deadstate4,
then attempts drop before respawning4seconds later. Uses isolatedBotClientScope,
firstslot1065 swapped only when selected weapon is secondslot1066, then
normal82580E38. If no pickup created, restore our temporaryslot swap.
All native eligibility/ammo/model/pickup/cleanup/network replication remains
native. No forced models/inventory or human changes. Native823E5CC0 override
ONLY while explicitdrop active, r4 exact botentity, caller82580E74. Outside
that scope runs original.82580080 hook logs returned pickup entity (0means
native rejection, not necessarily rendering). DropDeadBotWeapon logs slot,
weapon and pickup. No repeatedattempts/infinite itemcreation on corpse.
TestsPASS pre-death capture, duplicates, perbot independence, nextlife/entity
reuse, ownedfallback/noinventory. SourceguardsPASS aim/nav/185crashguard intact.
MP/SP buildsPASS, no gameprocess beforeinstall;4EXE/PDB SHA256match,
VERSION187/portnotes updated. Startup187 PASS: visible multiplayer menu
144FPS, left open. Live dropped-weapon creation/visibility pending.
Next test: bot_add4, kill rifle/pistol bots; inspect ground/pickup prompt and
'native dropped item' / 'death drop' logs, then respawnkill again. If pickup
entity nonzero but invisible, inspect native item model path82486BF8/world
model1436/1432 next; do not create duplicate items as rendering workaround.
Friend-side pickup visibility still untested. UAV/idleAI/high-low aim queued.
185guard still catches renderer list floatcorruption; rootwriter unresolved.
No187GitHub push/friendpackage. Keep rawlogs/generatednativecode local.

### 186 INSTALLED - vertical aiming correction; live test pending

USER185: heldbotguns visible and bots killable; no dropped guns; UAV no
compass enemy markers; some idle bots; high-ground enemybot fires level,
not downwards. Also earlier prone target shot overhead. Address vertical
aim only186; keep drops/UAV/idling as separate next patches.

185 live snapshot:11 native death calls logged (includes host),4 prevented
zero-byte-copy cases and no hardware queue-pointer writes. Example list
metadata buffer4318B8FE capacityBF992A6E count3F800000(float1.0). Looks like
float/vector data overwriting list, root writer still unknown. Guard catches
it and queue remains82685ED0. Several successful kills USERconfirmed;
not proof all death crashes resolved. Evidence live-feedback-185-summary.json
and CoD3MP-0.5.185-live-feedback.log archived locally.

186 proof: native8256E978 takes view/shotpitch into8254E120, which outputs
forward.z=-sin(pitch). Old Think used +atan2(dz,flat), exactly inverted.
Live185 bot6 needed-2.88 degrees to higher target, view/shot+2.89 instead.
Correct wantPitch=-atan2; upward grenade loft negative14 insteadpositive.
Normalize native viewPitch to signed[-180,180] before adaptive offset, so
357deg doesn't become360deg offset. Actual command/shot already both use
wantPitch, now consistent. New bot_aim.h tested nativevector dot alignment
across yawquadrants/elevations and16bit command wrapping, prone/crouch eyes,
invalid fallback and grenade loft. PASS.
Old muzzlehook raised ALL botmuzzlesbelow50 to60 even legitimate prone/
crouch eye heights.186 only replaces invalid/unset eyeheight; valid native
stance origin retained, matching Think's sourceeye. No gameplay stateflag
or aiming pose forced directly. No change to185guard,184navigation, team
assignment or shot schedule (sourceguardsPASS). MP/SP buildsPASS.
User closed185; no gameprocess verified beforeinstall. All4EXE/PDB hashes
match sourcebuild; VERSION186/portnotes current. Startup186 PASS: visible
multiplayer menu144FPS, left open. Live vertical-aim test pending. Next live: proneinfront ofstandingbot; standbelow and
above enemy; check botlook/shot elevation, then kill/respawn stability.
Drops/UAV/idleAI queued. Friend visibility still unconfirmed. No newGitHub
push/friendpackage this patch.

### 185 INSTALLED - renderer-list clear guard; kill test pending

USER184: game crashed again. Runtime184 reproduced same failure after bot7
killed by host: normal PlayerDie/deferrednotifications then watch82A87860
written by820D3638, lr820D367C, from82436FC0/824483C0. Globalszero/invalid
calls/gamehang. Saved local CoD3MP-0.5.184-crash.log. 184 log also shows two
cleared-low-jump attempts; no proof of successful jump/ladder/detour yet.

185 only adds wrapper820D3638 and bot_list_clear.h. Native caller82436FC0
passes exact secondary renderer vector82C89A60. Layout buffer+0, capacity+4,
count+8 (verified820D35A0/820D3638/820D36E8). Nonnegativecapacity nativeclear
only zeroscount. Negativecapacity nativebranch allocates0 bytes then copies
old positivecount before cleanup and finalcount0. Prepare clearscountFIRST
only for this exact vector and negativecapacity/positivecount. Calls native
for original allocation/free/metadata cleanup; other lists and normal clears
unchanged. Logs original buffer/capacity/count/caller and completedreset queue.
This is defensive protection of the observed copy path, NOT proof of why
capacity became negative or initial corruption's root cause. Need actual
runtime metadata/kill repro. Do not claim crash fixed without live evidence.
Existing184 nav/team/weapon/death/input wrappers byte-identical (sourceguard).
No new aim change; prone-target overheadshots still open. MP/SP buildsPASS.
Regression harness models nativezeroalloccopy and catches baseline overflow;
actual Prepare helper prevents it for counts1/4/176/INT_MAX, normal/empty/
other lists unchanged, cleanup/canaries preserved. PASS.
No gameprocess beforeinstall, all4EXE/PDB SHA256match; VERSION185 andportnotes.
Startup185 PASS: visible multiplayer menu144FPS, leftopen. Automated live
menu test interrupted by userinput; no botdeath test performed by assistant.
Next: match, bot_add4, kill several and keepopen;
inspect 'renderer list clear' logs, queue watch, killfeed/respawn/heldguns.
If guard never triggers yetwatch fires, use context diagnostics at allocator
8213F780 (caller820D367C) to verify savedr31 and destination/count aftercall.
Keep rawlogs/dumps local; no185 GitHub upload or friendpackage made.

### 184 INSTALLED - navigation patch; live test pending

USER183: bots stop at obstacles or keep pushing forward. Requests detours,
jump attempts with another route after failure, and ladders. Also confirms
bot guns visible183 (dropped guns and friend visibility NOT confirmed).
User sees3 bots on enemy team/1 alongside them: with solo host total3v2,
expected balancing for5 total players. Other human presence unconfirmed.
NEW aiming repro: prone directly in front of bot; it shoots above player.
Keep as separate pitch/target-height issue; NOT fixed184.

184 navigation only: all four moving/no-enemy roaming returns now route.
Persist waypoints until reached/blocked instead of recomputing each250ms.
Bounded local detour search64 collision queries/32nodes/depth4; body sweeps
and floor support on every edge, recent-position avoidance and committed
escape direction. No global map/navmesh or guarantee every maze is solved.
Supported low jump:40-unit clearance,64 forward, safe landing;100ms upmove
pulse,700ms commitment, cooldown8sec after failed progress. Removes old
blind/random hops, which couldn't fire when routing zeroed forward input.
Ladder: static native824A5270 validates surfaceFlags&8, ps+44 bit0x10,
normalps+100..108.824A9D28 uses forward input for vertical climbing. Read
trace+36 flags; near ladder chest ray plus body/floor-supported approach.
Native ladder move priority over aiming; no sprint/fire/jump whileclimbing,
stand command via existing BotCrouch. Failed vertical progress3500ms gives
cooldown8sec and alternate route. Approach radius80, not full map discovery.
Source regression guards: four roaming branches route, death/render/input
isolation hooks byte-identical183. No changes to team/weapon recovery or
teleport. C++ testsPASS wall detour across successive waypoints,64querycap,
enclosed/cliff/NaN rejection, lowjump and tall/roof/gap rejection, ladderflag.
Initial and final MP/SP buildsPASS. Game19332 exited; no game process
verified before installation. All4EXE/PDB installed and SHA256matched.
VERSION184/port patch notes updated. Startup184 PASS: visible multiplayer
menu144FPS, left open. Live detour/jump/ladder test still pending.

CRASH183 REPRO CAPTURED: bot7 killed by host, lethal/PlayerDie and deferred
DeathState/Player_Dying completed; watch FINALLY named clearing writer:
sub_820D3638 <-82436FC0 <-824483C0 <-82448A18 <-8231E0E0 <-82463978.
82A87860 becomes0, then broad globals0/invalidindirectcalls, game hangs.
820D3638 is native dynamic array reset; negativecapacity path allocates0
bytes then loops copying priorcount. Need capture vector pointer/capacity/
count/allocator return BEFORE invocation; don't assume queueoverflow or
patch allocator size without proof. Pointerwatch report lr820D367C locates
copy-loop path; this is new hard evidence, not a crash fix. Retain hooks.
Evidence chat work/CoD3MP-0.5.183-combat-crash.log (localarchive only).
184 does NOT fix combat crash. Next patch should target this captured writer,
then prone aiming; original navigation remains experimental pending live.

### 183 INSTALLED - stable balanced bot teams; live test pending

User explicitly chose balance across both teams. Source now uses per-slot
team cache, assigned on new bot creation from smaller team counts including
connected humans; tie opposite host. All six BotSide call sites now supply
slot; setup/session correction/Think/death respawn preserve that choice.
No live mid-match auto-shuffling on human join/leave/team change. Cache
cleared when a bot slot is newly created. Removed old bot_buttons0x4000
team override from BotSide; do not use that bit to change teams now.
Tests and both builds PASS; death/render hooks byte-identical182. Logs
archived. User closed182; no game process verified. All4EXE/PDB installed
and SHA256matched; VERSION183 and port patch notes updated. Startup183
and live match test pending.
Next live test: bot_add4 should populate both teams and survive respawns;
paired friend on each team must independently confirm body/gun visibility.
Remote visibility unresolved; this is assignment fix, not proven net fix.
Weapon recovery182 remains visually unconfirmed. Teleport unchanged.

### 182 - owned bot weapon recovery; live test pending

181 WAS installed/running: portVERSION181, sourceheader181, runtimeheader181.
Historical181 "notyetbuilt" heading below is stale. Original181buildlog notfound;
do not invent its test/build results. USER NOW confirms setting/returning works
perfectly. Current181controls: L3hold1sec save, Xhold2sec return; retain them.
Friendpackage request cancelled: userupdatedit. No package/archive created.

Bot weapons now USER UNCERTAIN (maybeback/forgets). Do not mark visiblefixed.
Concrete latest181log evidence supports one selection correction: emptyheld0,
pending0, commandweapon17/18/29 but ownedbits00801300 (8,9,12,23); kit12/8.
lastHeldcache persisted acrosskit changes and was reused withoutownershipcheck.
182 recovers onlyempty/nopending bot: rememberedifowned, else ownedmain/pistol,
else0. No directpsweapon/model writes. Other ammo-selection anddeath/watch/event
code unchanged (sourcecheckPASS). Teleport input/mp_fixes notedited182.
Regressiontests actualhelper PASS; observedlogcases included. Evidence
work/bot-stale-weapon-evidence-181.json +CoD3MP-0.5.181-before-weapon-fix.log.
8280 is repeatedloggedobservations, NOT distinctbots. Livecorrectrequest,
heldgun/rigpart anddroppedmodel stillneedverification after182install.

NEW MULTIPLAYER BUG: hostaddedbots tofriendteam; friendcouldn'tseethem until
switchingtohostteam. User says couldsee friend/bot scores whenthey killedhim.
This confirms somescoring, NOT friendroster/bodyreplication. Friendremote
same-team botbodyvisibility isseparateissue; don'tconfusewithmissingguns.
Needpairedclienttest; no fixclaim. Originalcombatcrash andunwantedmovement,
UAV, artillery unresolved. Latest181log had manykills over2055seconds withno
capturedfatal, but that doesNOT establish crashfixed. Pointerwatcharmed;
no clearingwriterreport found. Retain diagnostic hooks pendingrepro.

182MP/SP builds PASS; installed with no processrunning. All4artifacts
SHA256match; VERSION182/portnotesupdated, build/testlogsarchived.
Startup182 PASS: visible multiplayer menu at144FPS; left open for testing.
Nextlive testspawn+respawnbots, inspect
ownedcommand8/12 vs heldweapon and secondrigpart, drops. Friendteamtestlater.

### Next task: bots on either team and remote visibility

User wants bots visible to both humans and able to play alongside either
human on either team. BotSide(base), bots.cpp currently returns opposite
host team globally. All setup, respawn, SessionBots and Think correction
reuse it, so a bot cannot retain an independent team assignment. This was
confirmed source behavior; changed in183. Need per-bot team ownership
through spawn/respawn; do not repeatedly reassign based on host team.
User chose automatic balance across both teams; implemented in183. Separate remote visibility path remains unproven:
GiveSessionPlayer copies host session identity; host's sub_82400810 and
sub_823D9B50 hooks generate local body state. Must trace outbound session
state and remote consumption; no claim a team change alone fixes visibility.
183 built as a separate team-assignment patch;182 weapon live test pending.

### 181 - RETURN = HOLD X (controller) from anywhere; landing-trace fallback (NOT yet built/tested)
- Justin then asked for held X instead of R3. input.cpp (COD3_MP block
  after the pad read): back = (fromPad & PadX 0x4000), no stationary/ADS
  `ready` requirement, and X is NOT masked from the game (reload/use still
  work on the press); the R3 mask line was removed (R3 = plain melee).
  mp_fixes: notices say X; "RETURNING" only after 500 ms held (so reloads
  show nothing); the reject notice/log only for the L3 save now.
- USER on 180: R3 shows "RETURN POINT BLOCKED"; wants return to wherever he
  saved, even on odd geometry. 180 log (CoD3MP.log): many "tactical point:
  hold rejected; local state 2 pause 0 current 0" = the strict eligibility
  (alive/state3, ps+240 & 0x100000, host+900, ADS ps+172, |vel| x/y<5 z<40)
  failed while he held R3; BLOCKED = TacticalLanding (sub_82534200 box trace
  +8/-16 around the saved point, needs fraction<1, normal z>=0.6, not
  startsolid) found no floor.
- mp_fixes.cpp TacticalInsertionFrame (COD3_MP, end of file): new canReturn =
  host/ps valid, current client 0, not paused (0x82C73024), overlay closed,
  local state (0x82C7A270+0xB8)==2, health>0, state 3, not mounted
  (ps+240 & 0x100000) and not in a vehicle (host+900). The R3 2000 ms hold
  timer uses canReturn; the strict `eligible` still gates the L3 save and
  Input::SetTacticalPointInput (L3/R3 masking), so sprint/melee unaffected
  while moving (R3 may melee as the hold starts).
- If TacticalLanding fails, position (scratch+192, always set unless the
  allocation failed) gets the saved point with z+1 and sub_8255E118
  (SetOrigin, r1 = position-448, r3 host entity, r4 position) runs anyway.
  Log: "tactical point: landing trace found no floor; returning to the exact
  saved spot". Velocity zeroed after as before.
- Check: clang syntax pass with scratchpad stubs (32 stub/ImGui errors, all
  before line 2855, identical in the unmodified file; none in the change).

### 180 INSTALLED - R3return hold2sec; L3save unchanged1sec

USER confirms179 teleportworkedperfectly.180 changes ONLY R3duration2000ms
and displayedholdmessages; Savehelper accepts duration default1000ms soL3
unchanged. TestsPASS thresholds1000/1999/2000, release/cancel/no-repeat/wrap.
MP/SP buildsPASS. Existing botissues/deathcrash/watch unchanged/unresolved.
No gameprocess seen after userClosedreply. All4artifacts installed, SHA256match, VERSION180
+portnotes updated. Startup180 PASS: visible multiplayer menu leftopen. Next liveR3duration test.179confirmedruntime archived.

LATEST178 log update while user still playing: saved2070.7,-24.1,-48.5,
three returned lines (29570/31878/31929 in live log). Thus178 native path DID
activate later; initial no-success snapshot was earlier. Userperception not
confirmed; do not claim original neverworked. Laterruntime archived separately.
179 is requestedR3hold+feedback/eligibility change; actual original blockerunknown.

### 179 INSTALLED - L3 save anywhere stationary / R3 hold1sec

User178: cannot tell ifL3 saved, R3 no teleport; asksR3hold1sec andsavewherever
standing. No tacticalpoint success logs. Read-only178 host20704: validstate3,
health100, pause0, current0, ADS0, velocity0, nativeLocalState2.
179 direct finiteposition save (no save landingtrace), both L3/R3 independent
one-second hold timers. Return stillnativefullboxgroundedtrace/SetOrigin.
Render-triangle gating replaced by nativeLocalState2; allowsstationaryother
stances; toleratesgravityZ<40 while horizontalvelocity<5. Rawheld input visible
evenwhenineligible forrejectlog; notices SAVING/SAVED/RETURNING/NO POINT.
Inputroutingandtimertests PASS, bothbuilds PASS. Game20704 exited; no process verified. All4artifacts installed, SHA256match,
VERSION179 and portnotes updated; final178runtime archived. Startup179 PASS: visible MP menu leftopen. Next
then testL3notice/walkaway/R3holdreturn. Native teleport stillunverified.
LATEST botvisualreport: weapons missing in hands AND as droppedweapons.
No bot/crashfix179. Inspect both attachment and droppedentity model paths later;
do not lose this droppedweapon detail. Crash/unwantedmovement/UAV/artillery
remain unresolved. All tests/build/runtime snapshots archived in docs/logs.

### 178 INSTALLED - experimental return point (user authorized)

User clarified controls: hold L3 one second to SAVE/REPLACE position; R3 tap
while stationary returns. No automatic respawn redirection. Feature separate
from crash work;177bots.cpp/death/watch/UAV unchanged. User also reports some
bot guns still invisible177.177runtime watchtarget82A87860 + helperarmed seen;
no clearing-writer report yet in saved runtime snapshot. Other bugs unresolved.

178 touches input.cpp,input.h,mp_fixes.cpp,new tactical_point_input.h +version.
Raw PHYSICAL controller L3/R3 sampled before sprintfilter. Savehold1sec cancel
on movement/death/menus/ADS/vehicle/nonstanding. Releasing re-arms; no repeat
while held. R3return one perpress onlywithmarker+eligible. Moving L3/R3 normal
sprint/melee; scopedL3 steady aim; keyboard actions unchanged. Saveholdreplaces
marker, persists until host/map-clock change. Physicsvelocity<5 eachaxis,
sticks<8000, hostpmflags lowstancebits0. Pause82C73024/Overlay/menu render gate.
TacticalLanding uses private0x21000 stack + fullbox82534200 downtrace with
normalZ>=.6 and solid/fraction checks.8255E118 is verified nativeSetOrigin
updatesps/entity/interpolationorigin and toggles teleportbit; velocityzeroafter.
Not yet lived-tested; trace or eligibility may need correction basedlogs.

Both buildstages PASS (work/build-0.5.178-initial.log, finalbuild-0.5.178.log).
Actual extractedinput +holdhelper regression PASS; checkscope logarchived.
Userclosed177; no process at install. All4artifacts copied, SHA256match,
VERSION178 + portnotes updated.177finalruntime archived. Startup178 PASS:
visible multiplayer menu, leftopen. Next startmatch
and testL3hold notice, walkaway, standstillR3return, replace
point, ADS/sprint/melee unaffected. Retain crashwatch and capturewriterifcrash.
Save runtime before restart. Do not claim botissues or teleport livefixed.

### 177 INSTALLED - 176 FAILED extended combat; capture actual clearing writer

User: game crashed aftershooting/killing more; still felt unwanted movement.
176 firstkill passed ONLY. Then kills15 and7 credited (three total); each
82587268 BEFORE/AFTER queue82685ED0 count0 entries50113440. Afterthirdkill:
invalid calls001C0C01/00000D00, many globals82B3/82BD/82C7/82C2/82A9 now0.
176 drain: queueOBJECT00000000 countFFFFFFFF entriesFFFFFFFF. Thus it read
host guest0, NOT a corrupt real queue count. No error producer logged.
176 then wrote0 toguest0 via quarantine, an additional diagnostic bug nowfixed.
Broader corruption happens AFTER native playerdie returns, perhaps deferred
scripts/render/animation. Do not assert memset responsible from call-chain
membership alone. native_crt.cpp MP source is address-translated: build mp_src
already overrides8231E0E0, so no missingMPmemsetfix. Earlier weapon-request live
read aftercorruption had invalid ps pointers; work/weapon-requests-176.log is
INVALID EVIDENCE, do not infer weapon inventory from it.
Runtime preserved work/CoD3MP-0.5.176-third-kill-corruption.log; copieddocs/logs.

177 instruments existing watch: target82A87860 errorqueuePOINTER, beforefirstbot.
Old AddBot synchronousArmWatchpoints skipped caller(main), so main writes were
not watched. Now a CreateThread helper callsArmWatchpoints (main included),
then logs armed; closeshelperHANDLE. Existing watcher/handler reused. Runtime
proof watch actuallyfires is pending; no selftestsuccessclaimed. First watch
wins: ensure no COD3_WATCH environment or other earlier watch steals target.
Null queue guard now returns0 without touching guest0. All other gameplay incl
175stance-eventfilter unchanged. Residual movement still unresolved.
MP/SP builds PASS; actualwrapper tests PASS null-no-write, valid and oversized
counts. Installed no-gameprocess, all4hashes match, VERSION177. Startup177 PASS: visible multiplayer menu, leftopen for user.
Next launch/startmatch/addbots; verify watchtarget82A87860/helper armed log;
killbots to reproduce; capture FIRST watchpoint report/host+guest call chain.
No crashfixed claim. Keepuser informed and preserve runtime before restarting.
UAV, inconsistentweapons/weapon0, artilleryfire deferred until stability.

### Latest176 live test - ONE successful bot kill, guard NOT exercised

User completed requested kill test. Process13980 responding. Runtime archived
as build-logs/CoD3MP-0.5.176-death-test.log (snapshot). Bot8 killedbyhost means2;
82584188,8220B008,821E5028 and kill credit logged; slot8 subsequently respawned.
Queue BEFORE/AFTER both object82685ED0 count0 entries50113440. No enqueue,
nonzero drain, quarantine, or fault in captured log. This is one successful
kill WITHOUT containment intervening, NOT proof crashfixed. Waiting for user
2-3more kills and hands-off15sec prone test. No new build warranted yet.

### 176 INSTALLED - repeated death crash, error-queue diagnostic containment

175 USER: killing bot crashed. Not stable; stance gameplay success unconfirmed.
Crash work/CoD3MP-0.5.175-kill-crash.log, copied to docs/build-logs.175 PDB resolves
RVAa84002=>824309F0, callers824483C0/82448A18/82463978/8246C748, same172chain.
Fault guest13E10088 after2GB demandcommits. Log: bot7 died tohost, means2,
82584188 +8220B008 +821E5028 ran and host killcreditlogged, then runawaydrain.
175 logs ALSO prove bot165 stance events reached client0, caller8246BBF0,
and175guard intercepted them. Keep it; do not blame this narrow change for
same172crash without evidence.

CRITICAL corrected identification:824309F0 drains native ERROR MESSAGE queue,
not renderer drawlist.8210DCF8 producer caps count8, entries260bytes;
8210DD70 constructor allocates8*260. Fresh175menu read-only: global82A87860
points82685ED0, count0, buffer50113440. Need capture corruption origin.
Native evidence work/error-queue-175.txt, crash-list-users-175.txt,
player-die-native-175.txt; initial snapshot from work/read_error_queue_175.py.
176 logs producer8210DCF8 format/args/caller, before/after82587268 header,
and824309F0 count/header. count>8 is logged and reset without walking arbitrary
memory. DIAGNOSTIC CONTAINMENT ONLY: corruption source notfixed/proven.
If valid count still crashes, inspect callee/register changes in native drain.
If header changes during playerdie, inspect its weapon drop/script/local-tail;
record+2804 holds server slot4..23 and some native death tail uses it for local
arrays (check820B0D10 local-entity gate), but no speculative remap implemented.

176 MP/SP builds PASS; actual wrapper test PASS valid0..8delegate and oversized
counts quarantine. All4artifacts installed with matchingSHA256, VERSION176.
Menu-only175session closed through nativeAltF4 beforeinstall; startup176 PASS, visible menu left open for user.
Next: startmatch/addbots/killONE and leaveopen ifpossible; inspect
native error enqueue/death queue/drain/CORRUPT lines. Do not declare crashfixed
merely because quarantine avoids the late fault. Original debug unchanged,
AI active. Keep all newbuild/tests/crash logs archived and version notes current.
UAV, partial weapon models/weapon0, artilleryfire remain unresolved/queued.

### 175 INSTALLED - native bot stance events affect host input

174 user STILL forced standing. No success claim. Read-only previous evidence
and174 log preserved in work/CoD3MP-0.5.174-stance-failed.log.
Live174 isolation: added bot_sticks1,0,0,0 (manual neutral commands, AI skipped)
-> USER stays prone. Added bot_buttons1 (fire only) -> USER stays prone again.
Bots/session ticks kept active in both tests. This narrows to AI/non-fire actions.
Current port debug.txt has these temporary overrides! MUST restore saved original
work/debug-before-stance-isolation-174.txt before175 match test (otherwise a
false positive). Original contains move_watch1 and bot_bring63 only.

Native event handler8246A7D8(r3entity,r4event,r5param) dispatches event-162 switch:
cases3/4/5 => events165/166/167 =>8246AEB4/AEF4/AF34. Each only writes0/1/2
(stand/crouch/prone) to82B57804[CurrentClient], then returns. It can be reached
by session events823FEEA0 outside original bot scopes, with CurrentClient0.
175 wrapper skips ONLY those events when IsBotEntity validates source; other
events/host still native. Logs first60 event/slot/currentclient/caller. Intended
to fix input-source error, not force host stance. AI body movement unchanged.
Native evidence work/script-local-actions-174.txt and generated72.cpp switch.
Build MP/SP PASS (existing kernel format warning); source review log in work.
User closed174; all four175artifacts installed, matchingSHA256, VERSION175.
Original debug restored and verified (no bot_sticks/bot_buttons overrides).
Actual extracted wrapper regression PASS768cases; test log archived alongside
build log, native-case evidence and final174runtime log in docs/build-logs.
Startup175 PASS: visible multiplayer menu, left open for user.
Next ACTIVE-AI prone test. Live175 fix not yet confirmed.
Other issues remain: UAV markers, bot weapon0, artillery notfiring,
172 crash stability notproven. Temporary test statements above are historical;
debug overrides have now been removed.

### 174 INSTALLED - bot context and host stance isolation

173 user: forced standing after1-2sec, UAV absent, only some bot weapons visible.
Also reports artillery not firing. Log: artillery called from box, binoculars out,
then ready520B5264; no successful fire confirmed. Crash stability still unproven.
Game PID9692 was open during diagnosis/build. Preserve log before any relaunch.
Read-only evidence work/stance-live-173.log/.json, live-173-summary.txt,
marker-weapons-173.log; runtime copy CoD3MP-0.5.173-user-stance-uav-weapons.log.
Many living bots have weapon/pending0, inventory valid, rig only1part; visible
weapons IDs19 with2parts. Do not treat this solely as a shader/model failure.
Bot marker team+72 mostly0; snapshot radar0. Need native marker output/alpha
verification in active UAV. ps+1932 is NOT team; use entity+520 -> +4.

174 adds host stance latch82B57804 and CL slot0 hold fields Players+1608..1619
preservation to existing KeepFov bot-step scope, logs changes. Native82525338
reads that latch to emit stancebits;82571930 resets all4latches. This memory was
outside171's key/input restore. Normal82400810 bot player update previously ran
with host current context0: now scoped KeepFov + BotGunViewScope + g_thinkSlot.
173 supplemental selected-player filter retained. ReKitBots lacked bot context:
now BotClientScope around82206028. These are isolation changes; actual culprit
and live fix NOT yet proven. No stance clamp or forced-prone behavior added.
Death/obituary and UAV routines byte-identical173. MP/SP build PASS; static checks
PASS. work/build-0.5.174.log and check-input-context-0.5.174.log archived below.
User closed173; all four174 EXE/PDBs installed with matching SHA256; VERSION174
and port patchnotes updated. Final173 runtime log archived in docs/build-logs.
Startup smoke test PASS:174 visible at multiplayer menu; left open for user.
Next: test prone hands-off15sec, bot guns, multi-kill stability.
Then separate UAV fade/marker update, weapon selection if still0, artilleryfire.

### 173 INSTALLED � recover 172 weapon regression; isolate supplemental session ticks

LATEST USER TEST on172: killed one bot, crashed shooting second; bot weapons
missing. Kill-feed credit AND death animations confirmed by user.172 is failed;
do not describe it as a successful stance fix. Host stance/UAV still unresolved.
172 crash preserved at work/CoD3MP-0.5.172-crash.log. Matching172 PDB symbols:
RVA a83152 -> __imp__sub_824309F0; callers824483C0/82448A18/82463978/8246C748.
Fault read guest165000FC after2GB demand commits, inside a renderer list drain
(260-byte entries). Failure site known, corruption source NOT established.
Native evidence work/crash-native-172.txt. Do not hide fault with a pointer skip.

173 restores171's supplemental82403840 calls (weapon/session setup still needed)
with narrower isolation: g_supplementalBotPlayer scopes each call;82400810 skips
all other session players during that call only, so host is not replayed under
bot scratch context. Lock MoveLocked[3] explicitly before native call and restore
it after; require a private bot gun view rather than fall back to shared memory.
Retains171 key/input restore.171 death/obituary and UAV code unchanged.
Source snapshots work/bots-0.5.171-before-input-fix.cpp and
work/bots-0.5.172-before-regression-fix.cpp;173 check log and MP/SP build log in
work and docs/build-logs. Build PASS; both EXEs/PDBs installed with matching
SHA256, VERSION173. Startup smoke test PASS:173 visible at multiplayer menu. Game left open for user.
Live match test pending; crash fix NOT yet confirmed.
Next test: bot weapon models, kill several bots through respawns, host prone
without touching stance/jump. Retain confirmed death animation/kill-feed.
If crash repeats, resolve against173 PDB (172 RVAs no longer apply), preserve log,
consider renderer work-list producer / second death effects; avoid assuming
session removal caused renderer corruption merely because timing correlates.
UAV separate follow-up after stability; no new AI features until stable.

### UAV follow-up: marker fade check not covered by169 diagnostics

Native825049C8 reads entity+520 -> +72 into marker timestamp. Its tail computes
alpha from levelTime minus that timestamp; old ps+132 firing checks alone do
NOT prove visibility. Native evidence work/uav-marker-path-171.txt; concise
plan work/UAV-next-after-172.txt. Old128 host team+72 valid, bot0, but current171
must be checked because session behavior changed since then. Next separate
patch after173 stability test: inspect team+72 and actual native output alpha,
then refresh correct bot marker age while UAV active if confirmed stale.
172 UAV function byte-identical. Do not mark UAV fixed or runtime stance172
confirmed before user result. Latest user confirmed kill feed only; death
animation not separately asserted in that message.

### 171 - BOT DEATHS ROUTED THE NO-SESSION WAY (sub_823FD118) + host input guard
- STATUS FOR THE NEXT SESSION: 171 not yet tested. Open asks: bot death anim
  + kill feed (171 should fix), prone stand-up (171 guard), bazooka/scout
  artillery/vehicles/scorestreaks for bots, bunching/objective dedication
  (War + Domination), custom classes + campaign guns, lobby fill setting,
  friend update incl. textures (later), UAV (169 log: compass gate passes
  for all bots; Justin hasn't re-checked).
- HOW IT WAS FOUND (method worth reusing): 170 added DEATH_TRACE hooks
  (macro at the end of bots.cpp: PPC_FUNC wrapper printing r3-r5 named by
  WhoIs(): host/bot entity, record +512, team +520, handle index slot+1).
  Strings "PLAYER_DEATH_FLYING", "Player_Dying", "DeathState" were found by
  scanning mp.bin for death-related strings, then xref.py for users. 170's
  log: host death -> sub_82584188 (PLAYER_DEATH_FLYING, r3 victim entity,
  r4 attacker entity, r5 means 0x26) -> sub_8220B008 (script DeathState) ->
  sub_821E5028 (Player_Dying notify). Bot deaths: NONE of them.
  (sub_8258C778 is NOT the death: called every frame for every player.)
- Chain: sub_82584188 is called only by sub_82587268 = PLAYER_DIE(self r3,
  inflictor r4, attacker r5, damage r6, means r7, ...: sets health +0x308
  -> record +0x130, drops gun sub_82583310, death anim, scripts).
  sub_82587268's callers: sub_823FD118 (direct) and sub_823FBF78 (a session
  message handler, no direct callers). sub_823FD118 (called by
  sub_8258BA98, the damage code): lwz r3,0(r3); if 0 (no session) ->
  player_die(r4, r6, r6, r7, r8, r9, r10, ...) then the obituary callback
  *(0x82BCBF64)+0xce4 (entity +0x1e4 handles = kill feed); else ->
  sub_823ED658 (builds message type 0x2E, queued via sub_823D8F08 on session
  +0x5998) which is played by the victim's machine. Bots have no machine.
- 171 FIX: wrapper on sub_823FD118: if r4 is a bot entity (and bots remote)
  r3 is pointed at a zeroed 16-byte AllocatePhysical block, so the original
  takes the no-session branch on the host. Logs "lethal hit ... played here"
  and "player dies (sub_82587268): ..." (first 40-60). IF IT FAILS: check
  the log lines; if sub_823FD118 is never called for bots, the lethal
  damage path is elsewhere (trace sub_8258BA98 / sub_825862F8). If the
  death plays but the session gets confused (respawn), consider also
  sending the 0x2E message, or calling sub_823FBF78's tail.
- Host input guard (prone stand-up): sub_82404580 wrapper (167's per-bot
  sub_82403840 calls) saves kbuttons 0x82B53E20 (0x300 bytes; CL_FinishMove
  sub_82522B10 reads AND clears their wasPressed words) and the host's CL
  input 0x82C35030 (0x804) before the bot loop, restores after if changed,
  logs "a bot's session update changed the host's keys/input". If the log
  never says so and prone still breaks, look at 170's jump (+26 up move)
  or the 0x2000 stance taps instead.
- Earlier this session (168-170): 168 broke the respawn else-if chain
  (every live bot respawned every frame: 34k "respawns"), fixed in 169.
  Lesson: insert new tests as separate ifs, never inside if/else chains.
  Death state +1924 3->4 is set by the game itself (168's write unneeded).
  Menu textures: used.txt (texture_pack.cpp) showed all pre-match menu art
  replaced except 3 sepia photos (256x128_f18_17B182E0B3901241,
  67521EDF83599FBC, 2E277FD3C3C3E062) -> ESRGAN x4 (tools/esrgan.py +
  RealESRGAN_x4plus.pth in scratchpad) -> game textures\replace. Never
  GitHub. Committing: PATCH-NOTES once landed STALE (size mismatch):
  always verify sizes with device_list_dir and re-commit from a fresh dir.

### 170 - bot melee/smoke/reload/jump; death-step tracer
- 169 tested: bots fine again. Still no death anim. Log: dead bot record ==
  dead host record (move type 6, +1924 state 4, health <0): server side OK.
  168's "state 3 -> 4" never needed (game sets 4 itself).
- Button bits from CL_FinishMove (sub_82522B10): loop over kbuttons at
  0x82B53E20+0x138+0x18*n -> bit n. Handlers from the Cmd_AddCommand list
  (sub_8252C0F8): attack 0x1, smokegrenade 0x2, sprint 0x4, melee 0x10,
  activate 0x20, activatereload 0x40, class (grenade) 0x80, reload 0x400,
  leanleft 0x800, leanright 0x1000; 0x2000 stance, 0x8000 extra set by
  CL_FinishMove; 0x200 set when the local client's +0x65c (0x82C2DCF0+
  0x18d0*lc) is non-zero. Jump = command +26 (upmove) 127.
- BotExtras (after Think in FeedBots): melee when g_seeDist < 72 (Think
  sets g_seeDist = flat distance to a visible target); smoke at health<=50
  with a visible enemy <1500, 30-45 s apart; reload when clip 0 and spare
  >0; jump when forward>60 but moved <24 units in 1 s, plus random hops.
- Session player diff host vs bot at death (165 dumps): +0x50 is weapon
  anim flags (sub_823DDB08: 0x100 = weapon in slot +0x454, 0x4000 = fired
  recently), +0x54 = move anim from velocity (sub_823F2620), +0xD0 = the
  weapon. None is a death flag. sub_82400810's anim switch (+0x64 time,
  +0x54 anim via sub_823D42C8) is for vehicles/mounts.
- DEATH_TRACE hooks (first 40 calls each, args named host/bot entity/
  record/team/handle): sub_8258C778 (server death), sub_8220B008 (script
  DeathState), sub_821E5028 (Player_Dying notify, gated by a vtable+0x30c
  check), sub_821E4660 (PLAYER_DYING), sub_82463B78 (playerDeathHitGround),
  sub_82584188 (PLAYER_DEATH_FLYING). Compare a bot death with the host's.
- Not done: bazooka / scout artillery for bots (needs a weapon switch to the
  launcher, or binoculars + fire).

### 169 - fix 168's respawn-every-frame; UAV compass diagnostic; 3 menu photos
- 168 inserted the state-4 `if` between `else if (deadSince==0)` and
  `else if (now-deadSince>4000)`, so the respawn branch ran for live bots
  (deadSince 0) every frame: 34k "respawns" lines, bots flashing/falling.
  Now separate: `if (health<=0 && deadSince && now-deadSince>4000 && ...)`.
  168's state-4 change itself never ran (no "state 3 -> 4" lines) - still
  untested.
- sub_825049C8 (compass enemies): loops entity list [1..24] (slots 0-23);
  needs entity+0x208 -> +4 == enemy of host's side (table {0,2,1}) and
  record(+0x200)+0x84 != 0 and +0x84+15000 >= *(0x82C281D0+0x9c); position
  from *(entity+0x208)+0x150/0x154. UavShowsBots now logs "UAV check" each
  2 s while on.
- used.txt (168): all menu pictures >=128 before the match were replaced
  except 256x128_f18_{17B182E0B3901241,67521EDF83599FBC,2E277FD3C3C3E062}
  (sepia photos); ESRGAN x4 -> 1024x512, committed to the game's
  textures\replace. In-match NOT-REPLACED (409) are world/HUD, not menus.

### 168 - dead bots state 4; side at creation; bot roles + random classes; textures\used.txt for menu upscales
- Roles (g_role, at creation; printed "slot N is a <role>, class K"):
  objective 40% (only they take War's flag goal; replaces 165's slot%3),
  rusher 25% (sprints at >300 units; idles with long sprinting runs),
  flanker 20% (idle: long straight runs, heading changed every 6-11 s),
  camper 15% (idle: crouched, looking about; roams 8 s every ~40-60 s).
  With a target in sight all roles fight as before.
- Class: record +1920/+1922 (getters sub_820CE3D8 / sub_820CE538 -> the
  kit, sub_82206028) set to a random 0..6 per bot (was 0 for all). Custom
  classes / campaign guns for bots NOT done yet (mp_fixes' custom-class code
  applies to one's own spawn only).
- texture_pack.cpp: every texture >= 128 across, once, to textures\used.txt
  ("secs key replaced|NOT-REPLACED"): visit Create Lobby / System Link
  screens, then list NOT-REPLACED keys, stage their textures\dump PNGs,
  upscale (ESRGAN x4 in scratchpad: RealESRGAN_x4plus.pth), commit to
  textures\replace (game data stays on his PC, never GitHub).
- Death state: record +1924 (host goes 3 -> 4 shortly after death per 165's
  dumps; sub_82400810 at 0x82400ba4 tests == 3 = alive). ThinkBots' dead
  block now writes 4 for a dead bot still at 3 ("dead: state 3 -> 4");
  Respawn writes 3 back if alive and still 4. If anims still missing, next:
  find who writes 4 for the host (watch hook on writes to +1924 via the
  functions calling sub_82400810's caller), and kill feed = obituary path.
- AddBot: team+4 (entity+520 -> team object) = BotSide(base) at creation
  (was 0 -> friendly green compass arrow until the side was set).

### 167 - per-bot session local-player update (death data) + UAV shows bots
- Wrapper on sub_82404580 (session frame; r3 = Session(), f1 = dt): after
  it, for each bot with a session player: places[3] (Session+0xAEB0+0x5991
  +3) = his number (only if place 3 is empty or already his), KeepFov,
  BotGunViewScope(slot, current client 3), call sub_82403840(Session+0xAEB0,
  f1 = same dt), restore place 3 and the timer words +0x85B0/+0x861C.
  Log "the session's local player update run for each bot" (3x).
  Expect bot deaths to fill session +0x100..+0x138 -> death anim, kill feed.
- UAV: compass enemy markers (sub_825049C8) need ps+0x84 (last fired time)
  within 15000 of level time 0x82C2826C (= 0x82C281D0+0x9c). UavShowsBots
  sets every bot's ps+0x84 = level time each frame while 0x82B2CF90+12 == 1.
  (Justin: the UAV never showed bots; wants all enemies always shown while
  it runs.) Note this marks bots on the compass only while UAV is on.
- Justin's next asks (queued for 168+): bot roles (rushers, flankers
  wandering the outskirts, objective players, campers), every bot a random
  class/weapon, incl. custom classes with campaign guns.

### 166 - bot brain: prone aim, grenades at hidden targets, cover on reload
- Made while 165 was being tested (165 result unknown when made).
- Think: own eye from record+220 (was +60 fixed); target eye: valid 4..80
  (was 20..80, else 58) and height = clamp(eye-2, 6, 72) (was min 24) ->
  aims at prone/crouched heads. Grenade: also when the target was seen in
  the last 4 s, now hidden, 300-1100 units. TacticalMove cover search also
  when the in-hand clip is 0 with spare rounds (reloading), and at health
  <= 60 (was 40).
- Also: Respawn re-applies the bot's side (team+4, session +604) right after
  SpawnAtPoint (Justin: friendly green compass ping at each bot respawn;
  165's log: 48 "back to side" corrections a frame later).
- 165 RESULT (deaths): kill feed shows only host deaths. Session player
  dumps: the HOST's at death has +0x100..+0x128 floats, +0x130 0x1002,
  +0x138 0xA2000 (a death/state block); a BOT's are all 0 there. Writers of
  that block: sub_823F2E20 (called only from sub_82403840 = the session's
  per-LOCAL-player update, CurrentClient -> place +0x5991 -> session
  player; builds update/death messages), sub_823F78B0 (from sub_823F9528),
  sub_8240DEB8, sub_823CFCB8. Bots have no place -> never updated there ->
  no death message -> no death anim / kill feed. PLAN 167: after the
  session frame (sub_82404580, which calls sub_82403840 for active local
  clients), for each bot: places[3] = his number, CurrentClient = 3, call
  sub_82403840(Session()+0xAEB0, same f1), saving/restoring the session
  timer words (inner +0x85B0 float, +0x861C int) it advances, places[3],
  CurrentClient; KeepFov + BotGunViewScope around it. MoveLocked[3] = 1
  skips its input part.

### 165 - death diagnostics (session player dumps) + War: 1/3 of bots hunt
- 164 tested (20 bots, War): kill feed shows the HOST's deaths (bot killed
  host) but NOT bot deaths; still no bot death animation; most bots stood
  still (log: per-bot distinct positions 2-5 for many; they all held War's
  one flag crouched: Think's "at his flag, hold it" branch).
- History check: kill feed for bot VICTIMS was never confirmed; death anim
  confirmed only at 140 (bot held local place 1 + shared host machine
  object, place still held at death; 141 released it while dead).
- 165: at each death of host or bot, the FeedBots death watch also dumps
  his whole session player (784 bytes, at death and 300 ms on): diff a bot's
  against the host's to find what the dying player's machine sets. Also
  War goal only for slot % 3 != 0 (the rest hunt/wander).
- Next: from the dumps, set the missing death fields for bots (or find the
  local-place-only death path, e.g. in sub_82403840's place loop).

### 164 - bot's own machine object keeps the HOST's machine id (+0x34)
- 163 tested: still no death animation (freeze, vanish), no kill feed
  ("doesn't say I'm getting the kills"). Log: sub_823E9998 wrapper runs at
  each death (local branch builds a session message: type 0x18 / 0x6c
  bytes, queued via sub_823D8F08 on session+0x10000-0x5150).
- Theory: the session's death/kill messages name the machine (+0x34 id);
  the bot's machine id (host id ^ 0x5B0000NN) belongs to nobody, so the
  message is dropped. In 140-144 (death anim CONFIRMED in 140) the bot used
  the host's object itself, id included.
- 164: own object copy keeps host's +0x34 (only changed if g_botRemoteFlag).
  Now bot object == host object except being a separate allocation.
- If still missing: compare with 140 = shared holder (+4) itself; consider
  pointing the bot's session +4 at the host's holder again (bump count) now
  that commands/gun view/local index are separated.

### 163 - bot's own machine object marked LOCAL (+0x38 = 1)
- 162 tested: bot_add 20 -> lots of bots seen ("amazing"), no host bugs.
  Still NO death animation (bots freeze, then vanish at respawn); kill feed
  not checked separately (assume still missing); reloads not seen.
- Death anim/kill feed worked only with the host's shared machine object
  (137-144). Direct +0x38 checks via sub_821AE440 (callers sub_821B2BF8,
  sub_821B35E0 x2, sub_821B3900, sub_821C3318, sub_823D8C08, sub_823E4D80,
  sub_823FB298, sub_824046D8, sub_82405158, sub_82405A88) bypass the
  sub_823D9B50 hook.
- 163: GiveSessionPlayer's own object now keeps +0x38 = 1 (g_botRemoteFlag
  false; true restores 0). Own holder/object/id (+0x34) kept.
- NEXT (Justin): more bot logic (prone aim, cover, grenades), lobby fill
  setting both teams; fix kill feed if 163 doesn't.

### 162 - bots answer the session's local-place checks
- 161 tested: bots' WEAPON MODELS back. Still no death animation, no kill
  feed for their deaths.
- In 137-144 both worked while the bot held a local place (+0x5991).
  sub_823D8FF0(obj, number) = "number is one of this machine's places";
  sub_823D8FB0(obj, entry) = local index or -1. Callers of 8FF0:
  sub_823DCD88 (from sub_823C2850/823C37A8), sub_823E33B8, sub_823E57D0,
  sub_823E5978, sub_823EF7A8, sub_823F9440, sub_823F9528, sub_823FA9B8,
  sub_823FCD18, sub_82407538 (from sub_82407948/82407990/824D0C08).
- 162: wrappers: 8FF0 -> 1 for a bot's number; 8FB0 -> 3 for a bot entry
  (when it would be -1). Logs "answered as one of this machine's places",
  "given local index 3". RISK: index-3 users; if something breaks, try
  8FF0 alone.
- ALSO in 162: bot_add 20 on 161 added only 14 (tries cap 8 per request ->
  now 64) and only the first 6 got session players (SessionBots ran only
  when g_bots >= asked). Now every 30 frames any bot older than 30 frames
  without a session player gets one. Note: record +12 showed huge floats for
  some bots (159133.9 ...) - not investigated.

### 161 - bots count as local in EVERY sub_823D9B50 check
- 160 tested: 3 BOTS STABLE (no host bugs; killstreaks, artillery fine).
  But no death animation, no kill feed for their deaths, and NO WEAPON
  MODELS in their hands again. 160's sub_823E9998 wrapper did run (20 logs)
  but that function builds a damage/hit message (types 0x28 / 0x6c), not
  the death animation itself.
- All three missing things = "taken from a remote player's own console".
  The reasons bots were made remote (135-144: zoom, host full-auto guns,
  freezes) were most likely the command-ring overflow fixed in 159.
- 161: sub_823D9B50 returns 1 for any bot session player whenever bots are
  remote (own machine object kept; no local place; own command ring and gun
  view block). 140's death-flag restore (sub_82404DD0) still guards the
  freeze. If something regresses, the old narrow behaviour = remove this
  block (only g_drawingBot / g_botsLocal overrides).
- Other direct machine checks (sub_821AE440 on +0x38) callers not covered:
  sub_821B2BF8, sub_821B35E0, sub_821B3900, sub_821C3318, sub_823D8C08,
  sub_823E4D80, sub_823FB298, sub_824046D8, sub_82405158, sub_82405A88.

### 160 - remote bots' death animation (sub_823E9998 as local)
- 159 CONFIRMED by Justin: 2 bots, no gun/view/control bugs, bots move,
  shoot and kill him. Gun view pointers untouched during play. Remaining:
  NO death animation (since 145, remote bots). 3rd bot not tried yet.
- Server-side death (sub_8258C778, near the gun drop sub_82580E38) calls
  sub_823E9998(session, handle r4, handle r5, ...): resolves session
  players (sub_823E5F30) and if the first is NOT local (sub_823D9B50)
  builds a network message for his machine (alloc 0x18...), else plays it
  locally (0x823E9C68). Bots' machine = host -> never played.
- 160: wrapper on sub_823E9998 bumps g_botsLocal; the sub_823D9B50 wrapper
  returns 1 for any bot session player while g_botsLocal > 0. Log "a death
  (sub_823E9998) played with the bots as this machine's" (20x).

### 159 - ROOT CAUSE FOUND: bot commands written past the 4-entry local array
- 158's bisect: "GUN VIEW POINTERS CHANGED at FeedBots" every frame after
  bot 2 (slot 5) -> 00000000 then 7ADF0000 00000000 4270C6CF... i.e. a
  command (server time, buttons, weapon, angles, floats) written there.
- Players = 0x82C2ECF0, 6352 (0x18D0) each, ring +1688 (64 x 48), newest
  +4760 is the CLIENT'S per-LOCAL-player command array: 4 entries only.
  Entry 4 = 0x82C34E30 (the CL per-local-player input state 0x82C35030,
  0x804 each, is inside it: slot 4's ring 0x82C354C8.. covers the HOST's
  input struct -> prone standing up, "someone else has control", gun
  glitches with ONE bot). Entry 5 = 0x82C36700, ring 0x82C36D98..0x82C37998
  covers the gun view pointers 0x82C37070 -> gun gone, weapon reselected 0,
  crash. SV_GetUsercmd (sub_82530270) only bounds-checks < 24.
- 159: CommandPlayer(slot): bots (slot >= 4) get their own 6352-byte block
  (same layout); FeedBots writes there; NewestCommand reads there; the
  sub_82530270 wrapper copies a bot's newest command from his block into r4
  and returns. Log "slot N's own command ring at X".
- Many earlier band-aids (KeepFov low memory + host view object restore,
  BotGunViewScope, local client state guards, sub_8251A950/82593D50 skips,
  War view-kick skips, CheckFov) may now be unnecessary; remove one by one
  once 159 is confirmed, testing each.

### 158 - bisect: which step changes the gun view pointers 0x82C37070
- 157 tested: same (gun gone after bot 2). No "view kick ... skipped" lines
  at all (War callback theory not hit). Hardware watch on 0x82C37070 armed
  ("watching ...") but NEVER reported, though the 4 pointers went from
  829D8810.. to 00000000 x4 right at bot 2's add, then floats/garbage
  (7C190000, C2E16628...). The watch mechanism may not catch it (alias
  window? host write?) - don't rely on it.
- Sequence: "bot in slot 5 begun" -> "setup context create ... restored" ->
  "slot 5 holds weapon 0" -> "native select client 0 caller 82557DA0
  target 0" (pointers already 0 there).
- 158: GunPointersCheck(where) compares the 4 words after every step
  (SV hook: between frames, FeedBots, WatchWar, SV_Frame, lobby/mp fixes,
  SetUpBots, session players, ThinkBots, ReKitBots, UpdateBotBodies,
  AddBot; inside AddBot around SetUpSlot and BeginPlayer; every
  BotClientScope stage). Log "GUN VIEW POINTERS CHANGED at <step>".

### 157 - DEEP DIG: War script sets CurrentClient to a bot's client number
- 156 tested: crash shortly after bot 2. Host object 829D8810 NOT changed
  by bot steps, watch on its +4 never fired. KEY LOG at bot 2's add:
  "weapons: native select client 0 caller 82557DA0 target 0" (sub_82557D60
  reads 0x82C37070[cc]+0x18 pending select -> host weapon 0) and the gun
  view POINTER ARRAY 0x82C37070..7C went 00000000 x4, then floats
  (6CBE0000 / 4301371E = 129.2f): an overflow INTO the array.
- Memory map: 0x82C35030 = 4 x 0x804 per-local-client CL structs (index =
  CurrentClient; init sub_825967D8 clears 4 + a word at +0x2010 =
  0x82C37040), 0x82C37050 (0x20, sub_820B9680 / sub_8245AEB0), then the
  gun view pointers 0x82C37070. CurrentClient = 4 -> entry 4 = 0x82C37040,
  its +0x30 IS the pointer array.
- Found a game path that sets CurrentClient to ANY client number:
  sub_8244A1D0 (view kick: CurrentClient = r9 if != -1, writes cg
  0x82C7D490 + n*0x18E0 +0x1844..0x1850). Reached from engine callbacks in
  table *(0x82BCBF64): +0x400 = sub_824C9810, +0x404 = sub_824B52A0 (set up
  in sub_824D3338). Called by compiled script incl. War's (sub_82214EB8,
  sub_82212190, sub_821E4CA8, sub_821EC2F0/3A8/4D0) per player client num.
  For bots (4, 5) -> CurrentClient left at 4/5 inside SV frame, CL lists
  indexed past the end.
- 157: wrappers skip sub_8244A1D0 (r9 > 3), sub_824C9810 / sub_824B52A0
  (r4 > 3). Log "view kick ... for client N (not a local player) skipped".
  Watch back on 0x82C37070 (armed at first bot add) in case it persists.
- Also queued (Justin): lobby fill / bots on both teams as a match setting;
  prone aim, cover, grenades. War objective logic is already in (151).

### 156 - guard the host's REAL gun view object (0x829D8810, 0x400 bytes)
- 155 tested: crashed as soon as bot 2 was added. The pointer watch never
  fired: 0x82C37070 entries are real static objects 0x400 apart: 829D8810
  (host), 829D8C10, 829D9010, 829D9410 - never emptied. The "object 0"
  heals come from sub_82519178's OTHER callers (sub_820B1540, sub_82462C68,
  sub_8257E118, sub_825853A8 pass their own, null for a bot) - harmless now.
  Crash: sub_8251A848 (only from sub_8251A950) on the host's object
  829D8810: its +4 sub-object's table is garbage -> the OBJECT is wrecked.
- 156: KeepFov also snapshots/restores 0x400 bytes at 0x82C37070[0] around
  every bot step (logs "wrote the host's gun view object X, put back: +off
  old->new"); hardware watch moved to that object's +4.
- Note: 153's BotGunViewScope swaps entry [3] (normally 829D9410, a real
  unused object) for a zeroed block; consider using the real [3] object or
  copies of it if bots misbehave.

### 155 - watch the host's gun view POINTER 0x82C37070[0]
- 154 tested: guns invisible and unchangeable after bot 2. Watch on guest
  0x4 never fired. Low-memory diffs at bot spawn steps: +000 and +024 get
  0x520EBxxx, +018 toggles 0/1/FFFFFFFF (some list/struct at guest 0 used by
  a spawn path; put back by KeepFov). Right at bot 2's creation: heal with
  object 0 and saved 0.00, "client 0 in hand 0". Host also lost "in hand"
  20 s after bot 1 (10688).
- Theory: the host's entry 0x82C37070[0] is normally a real object (152's
  log once showed 0x67490000) and gets EMPTIED by bot code; then the game
  falls back on guest 0x0 (zeros) -> FOV 0, no gun.
- 155: hardware watch on 0x82C37070 armed when the first bot is added; the
  "adding a bot" line prints its value; the host-view lines print all 4
  entries. Expect "watchpoint: guest address 0x82C37070 was written ...
  now holds 0x00000000, which is not a pointer" with the writer's chain.

### 154 - find the writer of the host's gun view (hardware watch on guest 0x4)
- 153 tested: own gun view blocks allocated (slot 4 A7E20000, slot 5
  A7EA0000), 1 bot fine, 2nd bot -> host "in hand 0" + heal at once, gun
  glitchy, crash a bit later: bad calls in sub_8251A848 <- sub_82592CD0
  (host's own ClientThink_real) <- sub_82534DB0: the host's null gun-view
  object at guest 0x0 (+4 = sub-object whose table is called) wrecked.
  KeepFov still puts low memory back at our spawn steps -> bot steps write
  low memory through some OTHER null pointer, not 0x82C37070[3].
- 154 (diagnostic): KeepFov's log lists every changed word (old->new, 12);
  when the 2nd bot is added, Kernel::WatchWrite(0x4) + ArmWatchpoints():
  log "watchpoint: guest address 0x00000004 was written ... chain ...".
  (WatchWrite cannot watch address 0 itself: 0 means "nothing".)

### 153 - each bot gets his OWN gun-view memory (Justin's idea)
- Made while 152 was being tested (152 result not known when 153 was made).
- The per-local-client gun view array 0x82C37070[cc] (4 entries): host's
  [0] is 0 (his view lives at guest 0x0), the bots' shared index 3 was 0 too.
  153: BotGunViewBlock(slot) allocates 0x2000 zeroed bytes per bot;
  BotGunViewScope points [3] at it (and CurrentClient = 3 when asked) for:
  BotClientScope (create/side/kit/spawn), each ThinkBots iteration, and the
  game's own sub_82593608 / sub_82593518 for a bot's entity (which also set
  g_thinkSlot so the SV_GetUsercmd swap 3 -> slot works). [3] restored after.
  KeepFov (low-memory restore) stays as a backstop; its "put back" log
  should now go quiet. Log: "slot N's own gun view memory at X".

### 152 - guard guest 0x0-0x7FF around the game's own bot spawn/think
- 151 tested (War, ederdam, 2 bots): War goal WORKS ("flag in play is 2 at
  -1482 1782 -95", then 3 after a capture). Gun "almost better", then
  glitched, then host froze with a feel of "someone else had control".
  Log: KeepFov put back low memory 4x (only at our spawn scopes). Then
  "slots: client 0 in hand 0" (host's gun gone, command weapon 0), then bad
  calls + crash: sub_8251A848 <- sub_82593608 <- sub_82534DB0 <- SV_Frame
  (game's own call, not ours; garbage view objects, commit reads 0x93D1..).
- sub_82593608 = player spawn/think by entity (callers incl. sub_82404DD0,
  sub_824046D8, sub_824D0A40). 152 wraps it and ClientThink sub_82593518
  (handle -> entity via 0x82BD1FC0): for a bot's entity, KeepFov around it.
- If it persists: give bots their own view object (point 0x82C37070[cc]
  at a scratch block during bot work) - Justin's idea, cleaner.

### 151 - War goal for bots + guard guest 0x0-0x7FF (host's null view object)
- 150 tested (War, 2 bots): host had NO GUN again after the bots came; got
  in a vehicle and couldn't drive or get out. FOV fine (healed only once).
- Host's weapon-view object 0x82C37070[0] is NULL: its fields are guest
  0x0.. (+0x74 FOV, 0x28-0x3c floats, +0x1b8 list...). Bot code paths with
  their (also null) object write the same low memory. 151: KeepFov now
  snapshots guest 0x0-0x7FF and restores it after every bot think
  (ThinkBots iteration) and BotClientScope (create/side/kit/spawn); log
  "a bot's step wrote the host's weapon view (guest 0xNNN on): put back".
- WAR DECODED from 150's dumps (flag table hashes): F430CD43 = 0 for the ONE
  flag in play (middle at start, next along after each capture), -999
  (0xFFFFFC19) otherwise; 53377998 = holder float (-1.0 for the side that
  captured; Justin's side, side mapping still unknown); F2F5EAB4 = a piece
  entity handle (0x401A..); 54E300B7 = per-flag constant; AF35F29B = 0.
  151: WatchWar finds the flag in play, its origin from the flag handle's
  entity (+336) else the piece's, sets g_war; Think uses it as the goal when
  no Domination goal (reach 120 units). Log "war: the flag in play is N at
  x y z: the bots go there".
- Next: prone aim, cover, grenades (Justin's asks); bot death animation
  (remote); reload sounds; ADS off to the side.

### 150 - bots' first-person weapon view skipped; heal guest 0x74
- 148 tested: 2nd bot OK, host guns OK, but slight glitching in/out while
  holding guns. Justin captured War flags on 148 (no War dump yet: 149 had
  not been built). Log: "weapon view 00000000 (current client 0) put FOV to
  0 (saved -1)": sub_82519178's object (0x82C37070[cc]) is NULL, so its
  "+0x74 saved FOV" is guest address 0x74 (and sub_8251A950 writes floats at
  0x28..0x3c), shared by every null-object caller incl. bot contexts.
- 150: wrapper on sub_8251A950 skips it when CurrentClient != 0 and bots are
  remote; the 148 heal now also writes guest 0x74 when the object is 0.
- Also carries 149's War dump: test both in one War match with 2 bots.

### 149 - War flags learnt (diagnostic only, for the bots' War objective)
- 148 NOT yet tested when 149 was made (Justin building 148). Note on 147:
  both bots VISIBLE, they kill the host; after the host respawns his gun
  shows and FOV is normal, but ADS looks "off to the side".
- Justin's next asks: bots play War objectives first; then aim at a prone
  host, more cover, grenades.
- War is native (compiled script "_mp_war.bro"). WAR main 0x822190f0
  (in sub_82218BE8). sub_82218660: flags = level object (0x82A99494)
  +0x138 array of "war_flag" entity handles (count +0x140; element via
  sub_8221DB70(level+0x138, i)); level+0xb0 = count-1 at start. Per flag
  sub_82211880 sets script variables (hash-keyed): 0x07AA1578 = index,
  0xAF34F29B = 0, floats 0x53377998 etc. Script var table:
  sub_821DE680(handle) -> table (+0 count, +8 -> pairs {hash, value}).
  WAR_TouchFlag at 0x821d5ae8 / 0x82211dac; WAR_FlagUpdate in sub_82211880.
- 149: WatchWar (SV_Frame, every 2 s, only if mp_gametype == "war") logs
  each flag's whole table when it changes: "war: flag i of n ...". Also
  logs "war: game type now ...". Next: Justin plays War, captures a flag;
  diff the dumps to find owner/progress hashes, then give bots a War goal
  (reuse Domination::BotGoal-style hook in Think).

### 148 - heal the weapon-view object's saved FOV (+0x74)
- 147 tested: still no gun in host's view with 2 bots (weapon switching
  works), no zoom (CheckFov restores). Log: no "local client" lines at all
  (phantom-client theory WRONG; its guards stay, harmless), FOV still
  zeroed every frame "after the server frame" -> it is the HOST's own
  sub_82593D50 -> sub_8251A950 -> sub_82519178 putting obj+0x74 (saved FOV)
  back into 0x82BA2BC0, and obj+0x74 itself is 0: saved (0x82515bf0 /
  0x8251af28) at a moment a bot's code had left the global at 0. Then
  sub_825158b8 builds the view with FOV 0 inside the same call -> no gun.
- 148: wrapper on sub_82519178: if the call leaves the global < 1 while it
  was sane before, restore the global AND obj+0x74. Log "weapon view X
  (current client N) put the field of view to 0 ... healed".

### 147 - no phantom local client for remote bots (freeze, gun gone)
- 146 tested (bot_add 2 at once): no zoom, but host FROZE and his weapon
  disappeared. Log: "base field of view went 0.00 at after the server frame"
  (36x) - i.e. outside our code: chain sub_8251A950 <- sub_82593D50 <-
  sub_82534DB0 <- SV_Frame. sub_82593D50 uses 0x82C37070[CurrentClient]
  (local client view/weapon object); sub_82534DB0 loops the 4 local clients
  (0x82C7A270 + k*0xc88, +0xb8 == 2 means in play) setting CurrentClient=k.
  Theory: local client 3 (the bots' shared scratch index) came up "in play".
- 147: (a) BotClientScope saves/restores local client idx's +0xb8 state and
  logs "set local client N's state A -> B"; (b) KeepHostCurrent remembers
  local states while no bots and restores clients 1-3 when bots are remote
  (log "local client N's state A -> B ... put back"); (c) wrapper on
  sub_82593D50 skips it when CurrentClient is 1-3, bots remote, and no bot
  think running (log "local client N's frame skipped").
- Next after this: death animation (remote), reload sounds, prone, and
  objectives for native modes (Justin asked; needs which mode he plays).

### 146 - keep the global FOV across every bot think/setup
- 145 tested: 1 bot visible & no zoom; NO death animation now (remote path).
  2nd bot: no crash, but host's guns don't draw/pull out properly, host
  stands up when prone, and he hears (the bots') reload sounds.
- 145 log: "base field of view went 0.00 at think, slot 4" every frame once
  slot 5 exists. Writers of 0x82BA2BC0: sub_82519178 (0x82519250: global =
  obj+0x74, obj from sub_8251A950 <- ClientThink path, the weapon view
  state; a bot's +0x74 is 0) and sub_8250D0C0 area (0x8250d460). Savers:
  0x82515bf0, 0x8251af28 (obj+0x74 = global).
- 146: KeepFov RAII around each bot's ThinkBots iteration, and exact restore
  in BotClientScope's destructor (CheckFov stays as a backstop).
- OPEN (145/146): death animation with remote bots (probably the local path
  in the death/corpse code checks sub_823D9B50 or machine +0x38 too - extend
  g_drawingBot-style override to that path); host prone->stand and broken
  gun draw with 2 bots (maybe the same FOV 0 flicker; re-check after 146);
  bots' reload sounds heard as the host's own (2D sound for "local" player).

### 145 - BOTS AS REMOTE PLAYERS (scalable: no 3-bot limit)
- Justin wants any number of bots, not per-bot patches. Local places (137)
  cap at 3 (4 local clients) and every per-local list overflowed.
- 0.5.144 was built but NOT tested before this.
- Key find: sub_82400810 (session player visual update; sub_82403840 calls
  it for ALL 24 session players when CurrentClient is the first local)
  branches on sub_823D9B50(player) = "his machine object is local". Local ->
  body from his entity (0x82400c1c path); remote -> from the network copy
  (sub_823EC930), which a bot never gets -> the 135 remote bot was invisible.
- 145: g_botRemote default TRUE (own machine object copy, +0x38 = 0), and
  g_botLocal default FALSE (no local places). Wrapper on sub_82400810 sets
  g_drawingBot for a bot's session player (player+8 = slot, g_bot[slot]);
  wrapper on sub_823D9B50 returns 1 only for that player during that call.
  All remote bots use local index 3 as shared scratch current client
  (ReservePlace), SV_GetUsercmd swap still maps 3 -> slot during think.
  KeepHostCurrent now runs whenever bots exist.
- Log: "drawn as this machine's", "local index 3 (shared scratch...)".
- Watch: visible? gun? kill feed (was missing when not local, 134)? death
  freeze gone? zoom? Console bot_remote 0 / bot_local 1 brings 144 back.

### 144 - guard the base FOV float 0x82BA2BC0 (zoom with 2 bots)
- 143 tested: 2 bots (bot_add 2, both same tick) still zoom; team change +
  respawn doesn't clear it. Bots presumably still move (not reported broken).
  Log: "native FOV client 0 => 1.00" immediately after the 2 bots are
  created (before session places), 90.00 before.
- sub_82453810 (view FOV) starts from the float at 0x82BA2BC0
  (lis -0x7d46 +0x2bb8, +8), clamps to >= 1.0 (0x8206178C) -> result 1.00
  means that float is wrecked (< 1). The dvar string cg_fov still reads 90.
- 144: CheckFov() keeps the last sane value (10..170) and restores it if it
  goes bad; called around every BotClientScope (create/side/kit/spawn),
  after each bot's think, and before/after SV_Frame. Logs "the base field of
  view went X (bits ...) at <stage>, slot N" -> names the step that wrecks it.
  The FOV log line now also prints "(base X)".
- If the base stays 90 and FOV still 1.00, the cause is the ps/cg branch of
  sub_82453810 instead (cg arrays at 0x82C7D490 + CurrentClient*0x18e0).

### 143 - bot think also uses the local index as current client
- 142 tested: 2nd bot no longer crashes the game (only one bad call at its
  creation), but the host still zooms: log "native FOV client 0 => 1.00"
  from bot 2 on (90.00 with one bot).
- ThinkBots still wrote the SLOT (4, 5) into CurrentClient for each bot's
  whole think (ClientThink, firing). ClientThink (sub_82593518) passes
  CurrentClient to SV_GetUsercmd (sub_82530270, r3 = server client, <24), but
  the rest of the think path uses it as a local index (4-entry arrays).
- 143: ThinkBots writes g_placeIndex[slot] (1-3); a new wrapper on
  sub_82530270 swaps r3 back to the slot when it equals the thinking bot's
  local index (g_thinkSlot). The PmoveSingle lock byte 0x82C73024+idx is
  zeroed/restored as well as +slot.
- RISK: if bots stop moving/aiming, the usercmd swap isn't reaching
  (check sub_82593608's call, which computes the client from a pointer).

### 142 - bots use a LOCAL index (1-3) as the current client, not their slot
- 141 CONFIRMED: killing one bot no longer freezes the host.
- Adding a 2nd bot: zoom + crash again. Log: bad calls start right after
  "bot in slot 5 begun", chain sub_82513F48 <- sub_8252D590 <- sub_8252D870
  <- sub_8252E2E0 (client frame). sub_8252D590 passes 0x82C37070[CurrentClient]
  (a 4-entry per-local-client array); sub_82513F48 also indexes the byte
  array 0x82C73024 by it. The game itself only ever stores 0..3 into
  CurrentClient (0x82BCBE9C): loops over 4 local clients at 0x82C7A270+0xb8,
  stride 0xc88. BotClientScope (0.5.109) wrote the bot's SLOT (4, 5) there
  -> out-of-bounds writes; likely also the cause of the 0x4001/0x4035
  (script-handle-like) values found in wrecked vtables.
- 142: each bot reserves his own local index 1..3 at creation
  (`ReservePlace`, g_placeIndex). BotClientScope (create/side/kit/spawn)
  writes that index instead of the slot. GiveSessionPlayer uses that index
  as his local place (fixes: 2nd bot took bot 1's place 1 while it was
  emptied by 141). ReleaseDeadPlaces also gives the place back once he is
  alive again (141 emptied it again just after respawn and never restored);
  it doesn't run during Respawn (g_inSpawn).
- NOT changed: ThinkBots still writes the slot into CurrentClient around
  ClientThink (comment says SV_GetUsercmd path needs it). If 2 bots still
  crash, that is the next suspect. More than 3 bots: no index left (falls
  back to slot, unsafe).
- Log lines: "bots: slot N's local index K", "slot N takes local place K",
  "slot N is alive: back in his local place".

### 141 - bot leaves his local place while dead (host freeze, real lead)
- 0.5.140 tested: still frozen; death animation CONFIRMED visible. The 140
  hook showed `sub_82404DD0` runs at spawns, not deaths, flag was 0 -> 0
  (wrong lead, harmless; can be removed).
- Real lead in CoD3MP.log: in the frame the bot dies, "call: through a
  pointer to 0x00004035, which is not a function" with chain
  sub_823C9F00 <- sub_82404580 <- sub_82406C70 <- sub_82593D50 <- sub_82534DB0
  <- SV_Frame. sub_82404580 is the session frame; it calls sub_823C9F00 on
  *(session+0x139D0) (voice/talker-like object, vtable +0x48/+0x4c), whose
  vtable is wrecked. Then other threads hit bad pointers too (0x6FFFD478,
  0x82A98030, 0x5212BFB4, 0xC2721EE0). Memory corruption caused by the death
  of a bot holding a local place (same chain as the two-bot crash of 135).
- 141: wraps `sub_82406C70`; before it, any bot that has been alive once and
  is now dead (health <= 0 or state != 3) leaves his local place (byte 0xFF).
  Respawn now gives the place back also when it was 0xFF. Log: "bots: slot N
  is dead: leaves his local place".
- If still frozen: find who writes over *(session+0x139D0)'s first word at the
  bot's death (write watch), or give the bot no local place and fix the zoom
  another way.

### 140 - undo the "local flag" a bot's death clears (host freeze)
- Found by disassembly: the session layer's death handler `sub_82404DD0`
  (session object in r3; it calls the gun drop `sub_82580E38`) ends with
  `if (sub_823D9B50(player)) [[0x82BCCAC0]]+8 = 0` — i.e. for a player whose
  machine object is local it clears a byte. The bot shares the host's machine
  object, so his death cleared it and the host froze (no move, no pause).
- bots.cpp now wraps `sub_82404DD0`: reads that byte before, calls the
  original, and if it changed while the host (client 0) is alive (health > 0,
  state 3) and bots exist, writes it back. Logs "bots: a session death: local
  flag A -> B ..." (40x) and "bots: a bot's death cleared the host's local
  flag: put back".
- If the freeze persists, check the log line: if the flag did not change, the
  freeze is elsewhere in that handler (the `0x82BCBF64`+0xcf0 callback, or
  `sub_82593608` / `sub_823F9A78`).

### 139 - spawn the bot outside his local place
138 did NOT fix the freeze (reset fired, host still frozen after bot death).
Bot animations confirmed good on 138. 139 empties the bot's local place byte
during Respawn (his spawn goes the game's way via session and as a local
player it took the host's controls), restores it after.

### 138 - host frozen after killing the bot (current client left on bot)
137 CONFIRMED: no zoom on bot_add, bot's kills in kill feed, death anim
probably seen. NEW: host frozen after killing the bot (commands arrive,
nothing moves). 138 resets CurrentClient 0x82BCBE9C to 0 before/after each
SV_Frame when bots hold local places. If that doesn't fix it, find who sets
it on the bot's death and what the host's input path reads instead.

### 137 - bot takes a free LOCAL PLACE (session object +0x5991, 4 bytes)
Session object = (session-player list address) - 0x1010; local places at
+0x5991 (bytes: session numbers of this machine's 4 local players, >=24 none).
sub_823D8FB0(obj, entry) -> local index or -1; sub_823D8FF0 -> is local.
Bots had none, so local lookups fell back to the host (zoom/gun/crash).
137 writes the bot's session number into a free place 1..3 (bot_local 0/1).
RISK: split-screen viewports. Test: zoom, guns, 2 bots, screen not split.

### CRASH with TWO bots (0.5.135, bot_remote 0, bot_add 2)
Both got session players (1 and 2, copies of the host's), then zoom-in bug
returned and the game crashed: Guest fault read 0x37660000. Stack:
__imp__sub_8251A848+0x482 <- sub_8251A950 <- sub_82593D50 <- sub_82534DB0 <-
SV_Frame(sub_82535810). 8251xxxx is the rig/animation code (near 8251ED08).
Likely a per-local-player array (4 entries) indexed by a "local" bot beyond
its size, or two entries sharing the host's machine holder. One bot works.
Until fixed: keep to ONE bot. Investigate sub_82593D50 (what index it uses
from the session/machine object) before allowing more.
Justin asked about campaign-style AI (actors) instead of player bots: the
actor/AI system lives in the campaign exe (CoD3.exe), not CoD3MP; porting it
is a very large job. Bots stay player-based for now.

### 136 - bot_remote default OFF (135 made the bot INVISIBLE again)
A remote machine object stops sub_82400810 drawing him: that update is for
this machine's players. Keep the shared object. Open: host guns briefly
full-auto/unlimited (likely a local-player path keyed by machine/local index
taking the bot), kill feed missing, no death animation. Find those specific
paths; don't change his identity again.

### 135 - bot gets his own machine object (session +4 holder/+8 object)
134 CONFIRMED: bot reloads and switches to pistol. Justin: host guns briefly
full-auto/unlimited and hits not registering; bot kills missing from kill
feed; no death animation. Session +4 holder -> +8 machine object: +0x38
"local" byte (sub_821AE440), +0x34 machine id (sub_821AE448). Shared with
host = bot treated as a 2nd LOCAL player. 135 copies both per bot (0x100
bytes, AllocatePhysical, count 0x10000 so never freed), +0x38=0, own id.
Console bot_remote 0 reverts. RISK: remote path may expect a network peer.
Also: committing files sometimes lands a STALE copy (sizes differ). Always
copy to a fresh outputs folder before device_commit_files, and verify sizes.

### 134 - bot switches gun when out of rounds (CONFIRMED)
Death animation STILL missing on 133 (bot freezes/vanishes). Next: trace
what plays a player's death animation (server player_die vs session
update sub_82400810 for dead players; does sub_82403840 skip dead entries?).

### 133 - bot asks for last weapon: CONFIRMED (Justin: gun visible, bot killed him)
132 FAILED: kit re-given 5x, "in hand 0" each time. 133: FeedBots puts the
last weapon he held (19) into his command +8 when in hand is 0. Next if it
fails: check ownership/ammo after session respawn (ps +1040.. ammo words).
Justin also: bot "got stuck looking for me" (movement/tactics, not traced).

### 132 - bot gun (kit re-given after session respawn)
131 CONFIRMED: bot named "Bot 9". bot_setup 0 (menu kit callback) ZOOMS the
host again and gave no weapon: never use it. Real cause of weapon 0: the
session respawn spawns him after his first kit. 132 ReKitBots calls the kit
sub_82206028(&handle, class 0) for living empty-handed bots. Animation note:
sub_82400810 computes the move/anim bytes itself (sub_823F2620 from the
velocity), so movement animation is NOT a frozen copy; the death freeze
may come from weapon 0. Test 132: gun model, death animation.

### Open bot items after 130 (Justin, live)
- Bot VISIBLE automatically on 130. Kills count (log: "killstreak: client 0
  killed client 4"). But on death he freezes in his pose, then vanishes at
  respawn: no death animation. Likely cause: session player +0x260..+0x2D0
  is the networked animation state the owning machine sends each frame; the
  bot's is a frozen copy of the host's. sub_82400810 reads it (see field
  list +0x44..+0x2d0). Bot brain must write these fields (stance, move,
  death) each frame, or a native writer must run for him.
- Gun model invisible: bot weapon slot 0 (log "bots: bot 4 weapon 4005 0 0"),
  though he fires and damages. Trace how a real player's weapon is equipped
  (third-person weapon model attach) and do it for bots.
- Justin wants: switch to pistol when primary is empty, reload.

### 131 - bot names
130 CONFIRMED: auto session player given ("slot 4 given session player 1").
Bot showed host's name: session player name is text at +104 (32 bytes).
131 writes "Bot N" (random unused 1..24). If scoreboard still shows host
name, also check client userinfo/name in svs client (not yet traced).

### 130 BUILT - BOT VISIBLE (session player), Claude session Oct 15

Justin confirmed 129 launcher return and mouse-wheel cycling WORK.
The entity+532 "empty render record" lead is WRONG: it is the local
first-person weapon scene record (sub_82468D58, local client only). Do not
chase it further. The real gate: session players (session +48832, 24 x 784;
+0 number, +4 counted object, +8 slot). sub_82403840 calls sub_82400810
(full player visual update) only for filled entries; host bots had none.
Live proof on 129: debug.txt `bot_session 2` + `bot_bring` -> Justin SAW the
bot, it fought, took damage, died, respawned, no crash. Gun still invisible
(weapon 0). 130 gives bots session players automatically after bot_add
(Session()!=0 only) and bumps the shared +4 object's count. NEXT: test 130
(auto visibility, map restart, scoreboard name), then the invisible gun
(bot weapon 0 / equipment), then radar markers (may now work via session).

### 129 INSTALLED - final checkpoint before usage cap

User confirms128 Y cycling WORKS again; log19->15->19 and actual held follows.
Do not reopen as unresolved without new report. Wheel shares bridge but user
specifically confirmed controller Y; independent live wheel check pending.
User reports pistol->launcher->away selects primary; old code hardcoded
ownPrimary.129 saves actual held weapon name/id, uses actual launcher-in-hand
instead of stale toggle, returns to saved weapon if still owned; fallback
primary and reset saved return on spawn. Only mp_fixes.cpp behavior changed.
Actual-source return test PASS (test-launcher-return-0.5.129.log); both MP/SP build PASS (build-0.5.129.log).128 closed via native Alt-F4; no game process during install. Both EXEs/PDBs installed, executable hashes match, VERSION129. Game left CLOSED; no background test instance. Native user settings preserved; debug.txt reset to move_watch1 while closed. Pistol->launcher->pistol live check pending. User added bot again; read-only
work/live-bot-state-128-baseline.json captured and bot_bring1 requested. Bot
visibility NOT fixed; next trace visible scene submission rather than more
animation setup. All runtime/helper files remain local.

### Invisible bot: next investigation (scene submission, no visibility fix claimed)

Read-only128 evidence: work/scene-state-128.txt and live-bot-state-128-baseline.json.
Host entity670BF1E0 rig8299D810; bot4 entity670BE120 rig8299D610. Bot model,
92 bones, private pose and animation update are present and advancing. Its
entity+532 points66F221B0 whose first112 bytes are all zero. Host+532 points
66F22540, populated with render record: +92=8299D710, +96=host entity.
CAUTION host record rig D710 differs from host main rig D810; this may be a
first-person weapon scene cache. Empty bot cache is a lead, NOT proven cause.
Do not copy host cache or blindly force render flags/registration.
8248FA50(entity) lazily allocates/cache-initializes entity+532 (first104 zero).
82446458(scene record) checks record+92 rig192 against global825FCE40 view mask,
then calls82444880(rig,entity,worldTransform,...), or82444358 for extra entity.
82446810 traverses scene-visible objects and calls82446458; alternate handles
call824456F8. Need trace whether a LIVE bot reaches scene preparation/submission,
then identify the native registration path and all gates before changing it.
Candidate factory callers8247B780,82468D58,8243E898 are saved for inspection.
Evidence/disassembly: work/body-scene-entry.txt, body-render-submit.txt,
scene-initialization.txt, scene-factory-callers.txt, body-scene-pipeline.txt,
bot-render-function-index.txt, scene-object-lifecycle.txt. The guessed reader
viewmask address8260CE40 is WRONG; native -32160/-12736 computes825FCE40.
User prioritizes making bot visible; launcher129 was the already-prepared small
fix completed before cap. Next functional bot patch must be separate. No bot
visibility/radar success claimed. Native weapon0 also remains unresolved.

Build/test logs125..129 copied into docs/build-logs for durable project records.
Runtime128 confirmation log: work/CoD3MP-0.5.128-confirmed-cycle-final.log.
Source scratch path is C:/Users/hovgt/Documents/Codex/2026-10-14/i-m-working-on-a-pc/work.
No uploads, commits, pushes, or game-file sharing performed.

### 128 installed - repeated weapon-cycle input bridge

127 user confirms STILL INVISIBLE after bring and animated updates; do NOT
claim visibility fixed. Also repeat-confirmed pistol cannot switch to primary.
Bot live127 had health<0 after user shooting (death logged); do not infer
render hiding from corpse flags620 C020 as original alive cause. Need next
trace of visible scene membership/submission, not more animation allocation.
128 only weapon input change: raw Y/wheel rising edges in input.cpp latch;
MpFixesWeaponCycle drains each host frame, dispatches native82457A60(1,0).
Bridge eligible only native weapon-ready/live/on-foot/local0/menus closed;
Xam normal MP report masks Y while bridge enabled, avoiding double-dispatch.
Pause/F11/dead/mounted presses drained, native menu/vehicle Y preserved;
SP unchanged reporting. Native cycle still owns selection/ammo/animation.
New header weapon_cycle_input.h plus input.h/.cpp, kernel_xam.cpp, mp_fixes.cpp.
Tests PASS: work/test-weapon-cycle-0.5.128.log (header latch and actual-source bridge guards). MP/SP builds PASS: work/build-0.5.128.log. Installed128 after native Alt-F4 closed127 and process exit verified. Both EXEs/PDBs copied, executable hashes match, VERSION128. debug.txt reset while closed; choices preserved. Preserved work/CoD3MP-0.5.127-user-invisible-and-stuck-pistol.log. Live weapon verification pending.128 launched visibly through Sky, window526094 title confirmed and native main menu rendered. F11 open/close works; automated grave taps did not open console, so leave native menu to user rather than repeated taps. No hidden test instance running. Ask user start match then test repeated Y/wheel in both directions. User subsequently confirms Y cycling fixed; see129 checkpoint above.

### 127 installed - evaluate bot animated body, live check required

126 fresh match bot4: ONLY model-update call is composition82447C60, component0.
125/126 constructor creates component afterward; NO animated update calls seen
in126 over multiple minutes. Native network-player path calls8251ED08 as part
of visual update, absent for host-only bot.127 adds UpdateBotBodies after
ThinkBots in SV_Frame hook: registered slot>=4, ps1932==2, rig owner matches,
valid model/pose/component, then native8251ED08(rig,-1). Native gates retained.
No host/session copy, no local input/camera context change; no weapon change.
126 weapon test: first Y reaches82457A60 (caller8254331C through dispatcher
82543290 -> 8247F7B0), selects15 from19, equipped becomes15. Subsequent Y edges
logged by input.cpp but NO more native-cycle calls. So per-frame slot swap is
NOT proven cause; investigate dispatch/press-release/virtual input gate next.
User says pistol now out, cannot switch back. Gameplay left running.
Computer Use: selected126 window656010, attempted Tab/Escape taps missed by
polling; user manually returned from scoreboard and reproduced Y. Do not
keep retrying automated key taps. No game files uploaded.
127 actual-source guard test PASS (work/test-bot-body-0.5.127.log), MP/SP build PASS (work/build-0.5.127.log). User authorized take over; closed126 via native Alt-F4, process exited naturally. Preserved work/CoD3MP-0.5.126-final.log. Installed both EXEs/PDBs, hashes match, VERSION127. Scripted native-console runtime test PASS: devmap mp_merv at12s, bot_add1 at45s. Model and animated-update counts advance together (428/427,536/535,638/637); no runtime crash observed through capture. Evidence work/CoD3MP-0.5.127-scripted-body-update.log. Visual visibility STILL UNVERIFIED. Shell-launched test process19152 window was not exposed to Sky despite main title; stopped only that owned test process after capturing log to avoid background music. Relaunch through Sky for visible manual match; no COD3_CMD env left set.

### 126 installed - diagnostic trace, no fix claim

125 user explicitly confirmed bot STILL INVISIBLE after bot_bring1. Animation
component exists/advances and pose buffers valid; this was only a partial fix.
Read-only evidence work/render-state-125.txt, bot-animation-live-125.txt.
Entity304..316 is a cached world/collision query, NOT proven render bounds;
82530720 -> 824940E8 updates it. Do not blindly write host values into bot.
125 user also reproduces weapon cycle failure: Y x3/wheel no change. Launcher
out/away restores primary. Logs show controller Y edges, held stays19/15.
work/weapon-cycle-stuck-125.txt confirms owned MP44/P38, ammo30+67 /7+14,
selected global82B9F310=19, held19. Continuous slot swap in MpFixesLauncher
(mp_fixes.cpp around1926) is suspect, not yet proven. Do not remove blindly.
126 adds observe-only wrappers in bots.cpp for8251ED08/8251E5F0 (bot model
update) and82457A60/82485D28 (native local cycle/selection); delegates exactly
once, retains return context. Both MP/SP builds PASS; 126 installed after user confirmed CLOSED125. EXE hashes match staged build; both PDBs copied; VERSION126 verified. Live trace pending.
Prepared build work/build_126.cmd and log work/build-0.5.126.log.
Next live test: add bot, press Y and wheel, examine new 'weapons:' / model
update logs. If native cycle never runs, inspect command binding path; if
select returns target but held unchanged, inspect usercmd/movement path.
User confirmed CLOSED; no game processes present during126 installation. debug.txt reset to move_watch1 while closed to avoid automatic re-teleport on next launch. Preserved125 log at work/CoD3MP-0.5.125-invisible-and-weapon-cycle.log. Preserve logs
before next launch and ask close only after staged build ready.

### 125 live test - animation construction verified, visibility pending

User added bot in fresh125 match. Read-only inspection confirms bot4 rig8299D610
component82700310 (host826FE8D0), registration flags61; constructor succeeded.
Evidence work/bot-animation-live-125.txt and live-bot-state-125-animation-created.json.
Bot alive/moving but ps164 weapon0, entity304 bounds0 remain. User confirmed it remains INVISIBLE after teleport. debug.txt bot_bring1 requested to bring bot beside host;
visual visibility and radar still unverified. Do not claim invisible bot fixed.

### 125 active bot work - missing animation component proven

User wants bots now, acknowledges Kangaroo/controller/volume fixed. Focus
visibility then radar before expanded AI, one change/build. 125 now installed.
User freshmatch bot_add1; read-only inspector work/read_live_bots.py validates
Guest base, reads only safe ReadProcessMemory. Snapshot work/live-bot-state-124.txt
and work/live-bot-state.json. Bot4entity677B6A00 ps82BF5090, definition
c_mp_GE_ass_Lod2, rig8299D610, generic_human695F8DA0,1model92bones. Hostrig
8299D810 has2models96bones. Bot weapon0 andps132recentactivity0.
Native animation component: host rig+32=826FE810; bot rig+32=0. Both source
model parts+8->+56 animresource65F3D600 type82028C78, matching native required
signature at825C3570. Evidence work/bot-animation-before-125.txt.
Native8243F138(rig,0) constructs animationcomponent rig+32, validatesresource,
private allocation, schedules task8250F1C0 (setsentity624 bit0x40). Host624=61,
bot21. Normal networksetup near82400D40 already calls8243F138.125 adds this
native step after bot spawn finalizer, guarded rig exists/component absent.
bot_setup bit4 disables only newstep forA/B; default1enabled,5old. No radarfix.
Actual-source guard harness PASSED (work/test-bot-animation-0.5.125.log). Both MP and SP built successfully (work/build-0.5.125.log). User confirmed CLOSED124; no game process present at installation. Installed both EXEs/PDBs and VERSION125. Live native component creation and visible character are still unverified; do not claim fixed.
Native level base CORRECTION: lis-32061 plus-32304 =82C281D0, NOT82BB81D0
(oldLogWhere incorrectlyprints game0). Real level+148 maxclients24, timevalid;
world loop DOES processbot (serverstate1 samehost,connected+1880=2both).
Do not falselyfix serverclientstate. No code correction to oldLogWhere yet.
An attempted autonomous testlaunch was blocked by existing game guard; NO
extra test process launched. User subsequently closed the game for installation.

### USER CONFIRMED124 - sensitivity reset fixed

Justin answered the lower/raise/apply test "Yes, same speed again" and explicitly
"yes it is fixed".123 D-pad menu fix also user-confirmed. Do not re-open these
as unresolved regressions without a new report.
Live124 log confirms horizontal100->90->100 and vertical90->100 cycles. User
subsequently chose90/90 and applied; log saved controller horizontal90 vertical90
volume50%, pad look then consumed90/90/audio25. Actual port INI now has
native_volume50,controller_horizontal90,controller_vertical90. Preserve this
user choice; do not force200 on next startup.200 is the maximum,100 the default.
Snapshot:work/CoD3MP-0.5.124-confirmed-sensitivity-fix.log.
Saving to disk is VERIFIED; full close/relaunch restoration still not tested.
No new build needed for this confirmation;124 remains installed. No game process
stopped or settings changed during record update. Invisible bots/radar next
priority before expanded AI; weapon switching and window drag live test remain
open. No bot visibility fix claimed. All game files remain local.

### INSTALLED124 live checkpoint - supersedes earlier pending states

124 BOTH built/installed after user CLOSED123, no processverified; VERSION124.
Build work/build-0.5.124.log exit0. Hooks test pass plus init-before-first-draw
regression with old37/50 limits. Updated generator work/prepare_live_slider_test.py
and actual-sourcework/test_live_slider_124.cpp. Fake test initially lacked mapped
localprofile address82C8A324; added shim, retest PASS. No source test failure.
124 visible startup passed; user started match/opened actual pause Controller.
Read-only live memory PROVES actual primary sliders value100/min10/max200:
work/live-options-124-controller-open.txt. Globalprefs100/100/audio25 unchanged.
Range initialized before first render via824F6758 guarded controller vtable,
or before first step if resource assigned old limits late; setter820BDA20
changes range only for verified controller axes. Native bar value/max uses200.
ControllerInitialize maps panel+80 local index to profile via82C8A270+index*3208
+180. Preserves native up/down effects, return value; overridesstep10 andredraw.
Observed actual apply8250A4D0 saves native Capture(controller) afteroriginal.
Current async user test asks lowerhorizontal once/raise once/leave Options,
confirm same turning speed. Await logs live horizontal90/100 and savedfields;
real disk-relaunch not yet proven. Game LEFT OPEN for this test (window132710,
process new124); do not install while running. No hidden launch.
123 D-pad fix now USER CONFIRMED.123 audio did not reset in traces (25/50).
Bot visibility/radar remain unresolved; no bot change124. Next gameplay priority.
Docs/port note copies updated; all files local. No uploads/network.

### 124 active fix - actual sliders identified

123 user confirms D-pad menu bug FIXED. Sensitivity still resets. Live logs
captured nativeapply caller8250A57C H35/V47 then37/50; audio stayed25/50.
Actual apply8250A4D0 copies panel+288 preferences via824DA350 then825095D8.
Read-only ctypes ReadProcessMemory with checked return values and validated
Guest base (matching vtable825010F8) succeeded, no unsafe dereferences/helper
exe. work/read_live_options.py, work/live-options-123.txt. No process writes.
Actual controller panel vtable820716DC objects829DB010/829DCC10/etc, children
array+4 indices2/3 sliders. Slider vtable82070AC4, setter820BDA20 /getter821BEB70;
current+24,min+28,max+32. Actual maxima37(horizontal) and50(vertical).
Native up82509E00/down82509E58 step1 and redraw824E8C40 dividescurrent/max.
124 source adds guarded actual slider setter range10..200, step hooks10%,
controller apply8250A4D0 saves Capture(controller). No bot changes.
Actual hooks harness PASS observed slider layout and100->90->100,
200->190->200,independent axes,bounds,untouched other control,save200/10,
audio unchanged. work/test-live-slider-0.5.124.log, test_live_slider_124.cpp.
123 log preserved work/CoD3MP-0.5.123-live-slider-reset.log. User CLOSED123
explicitly for install.124 build underway; not installed until successful.
Need inspect real widget initialization in124 too: setter may be called before
its parent's child array is assigned; first-step init/render guard may be needed.
Prior120-122 unrelated menu hooks retained, not evidence of integration.

### FINAL 0.5.123 checkpoint - October15,2026

User requested finishing quickly due usage limit and preserving all records.
INSTALLED:0.5.123, both exe/PDBs copied; port VERSION.txt verified123.
Build work/build-0.5.123.log exit0. Initial duplicate pad-look hook link failure
preserved work/build-0.5.123-first-link-failure.log; fixed by helper call from
existing mouse_look.cpp hook, not a second override. No outstanding build job.
One functional fix: launcher D-pad handler drops presses while native pause
byte82C73024 is set, updates previous buttons before returning so held input
cannot become a deferred equipment switch. Actual handler sequence harness
PASS:work/test_launcher_pause_123.cpp and test-launcher-pause-0.5.123.log.
Native live D-pad check still needed. Startup self-test reached visible MP menu.
Game LEFT OPEN for user: window527000 titlev0.5.123. No hidden game launch.
Sky Down navigation did not reliably change selection; subsequent screenshot
showed chat overlay/user activity. Do not claim in-match slider reproduction.

SETTINGS STILL BROKEN IN LIVE USER TEST: pause->Options->Controller decrease
then increase feels much slower than startup and appears to reset audio.
User calls startup200% by feel; new123 startup trace actually shows native
H100/V100/audio25, i.e.100% and50% volume. Native yaw ceiling580deg/sec may
explain faster initial feel. Port INI still had NO native_options save fields.
Prior unit mocks and user15(150%) reply did NOT confirm actual screen integration.
No120-122controller menu/adjust logs were observed. Need identify actual live
menu callback before another fix, not assume824E7358 is reached.
123 diagnostic logs first40 native825095D8 applies prefs/caller/H/V/audio/pause;
existing8252B578 hook calls NativeOptionsTracePad on changes (first40).
Startup caller82559108 then82559434 applied100/100/audio25 to each of4profiles.
Current log snapshot work/CoD3MP-0.5.123-startup-diagnostics.log.
NEXT: user start match, open pause Options Controller, lower/raise once, return
to gameplay; read native options apply/pad look lines and saved INI. Then fix
actual menu overwrite/range path, preserving audio on controller apply. Native
widget frontend823D5608/823D5198 remains unhooked; actual path unestablished.
Do not claim sensitivity/audio persistence fixed or live slider test passed.

BOTS STILL INVISIBLE/RADAR ABSENT.122 DID create slot4 despite user finding none:
entity677B2C90 ps82BF5090 generic_human55B1C9F4 instance68CF56D0. Active AI
logged, visual/radar unresolved.121 verified bot_bring1 beside host also invisible.
Preserved work/CoD3MP-0.5.122-user-settings-reset.log and121confirmed-invisible.
No bot behavior change123. Earlier bot_session1 diff request occurred after game
closed, so no fresh diff. debug.txt restored to move_watch1. Do not use session2
(host clone) due prior crashes. Bots next gameplay priority after settings.

All files remain local; no uploads/network. Patch notes synced beside port as
PATCH-NOTES.md/.txt. Work files are in chat cwd/work; source project on D drive.

### 123 active development - settings integration regression

User tested122: pause -> Options -> Controller slider adjustment becomes much
slower, seems to reset audio; D-pad in menu pulls bazooka. Native_options INI
fields still absent.122 log has NO controller menu/adjustment or saved messages:
previous screen identification and unit mocks did not establish integration.
Do not claim native range/persistence fixed. User prior15% reply was insufficient
without actual logs/display. Need native apply caller and pad prefs change trace.
123 source: launcher D-pad handler now checks82C73024 native pause byte AFTER
updating lastButtons (no delayed edge). This is the one functional change.
Read-only bounded40 native825095D8 apply prefs/caller/values and8252B578 pad
value-change diagnostics added; existing mouse_look.cpp hook calls helper
NativeOptionsTracePad (first separate duplicate hook caused link failure,
resolved; preserved build-0.5.123-first-link-failure.log). Both executables
built/installed123 after no game processes verified; VERSION123 checked.
Actual-source launcher press-sequence test passes, log
work/test-launcher-pause-0.5.123.log; build work/build-0.5.123.log exit0.
No live123 menu trace yet. Need open actual pause Options/controller, decrease
one step and inspect new apply caller and pad look value-change traces.
Do not call settings reset fixed; do not expand slider hooks without observing
actual path. No bot code changed123.
122 latest bot DID spawn slot4/entity677B2C90/ps82BF5090, generic_human55B1C9F4,
drawable68CF56D0, active AI but not seen. Preserve
work/CoD3MP-0.5.122-user-settings-reset.log. Radar still unresolved.
Launched122 visibly for own menu test, window1442184; Sky input reported user
input detected immediately. Stopped UI input. Game later observed closed before123 installation. No live menu adjustment/test completed by agent.

### 122 development checkpoint � supersedes installed121 status below

User confirms121 native controller menu reaches15(150%). Now requests maximum
20(200%) through the same native slider. 122 changes both axes10..200, steps10,
bar normalization200->50, Settings validation through200 and free-look cap
0.580deg/ms (580deg/sec). Defaults remain100; saved150 remains150.
122 both executables BUILT successfully (cmake --build build --target CoD3MP
CoD3 exit0); no full recompile of unchanged guest code needed. Build source
version_number.h122, generated mp_src/native_options.cpp updated by build.
Actual-source tests PASS200 bounds/bar/save/callback/defaults/cap; Settings
reload200 and reject201. Logs work/build-0.5.122.log and test-*-0.5.122.log.
122 BOTH INSTALLED after verifying CoD3MP/CoD3 absent, copied exe/PDBs and
VERSION122. Reset debug.txt to original move_watch1 after game closed.
No new runtime122 test yet. Asked user launch122/match/bot_add1 and leave open;
then toggle bot_session1 for read-only host/bot state diff (NOT2, prior crashes).
121 bot_session1 was requested after it had closed: NO DIFF COLLECTED.
Preserved final failed visibility log work/CoD3MP-0.5.121-confirmed-invisible.log.
121 live bot_add1 yielded slot4 generic_human558119B4, drawable69F26D80,
spawn finalizer82211DF8; active crouch/aim/fire logs. This confirms a model
object exists, NOT visibility. User CONFIRMS still invisible after bot_bring1;
UAV/radar still no marker. Teleport confirmed log record/entity near host. Captured log at
work/CoD3MP-0.5.121-bot-test.log. No native_options saved fields observed in
port INI at this checkpoint; restart persistence still unverified.
Read live bot generic_human/drawable result next; visibility remains priority.
No live persistence/turn feel/window drag success established yet.

### INSTALLED121 final build checkpoint

121bothbuilt/installed; VERSIONverified, buildDone preservedat
work/build-0.5.121-completed.log. MPvisiblyreachedmainmenu/titlebarreachable.
Automated borderdragNOTcompleted: firstoutsidebounds, refreshedthenuserinput
wasdetected. StoppedUIinputs; doNOTclaimliveresizepassed. GameLEFTOPENforuser,
window titleCall of Duty 3 Multiplayer - recompiled v0.5.121. Askbeforeinstall
anythingwhilegameopen; userhasalreadyagreedclosingbetweenbuilds whenasked.
Actualinputcaptureharness PASSED: work/test_mouse_capture.cpp generatedfrom
actualSetHold/MouseDelta/SuspendMouseCapture withfakeWinAPI, prefix/suffix
preserved. Logs test-mouse-capture-0.5.121.log andgeometrylogpreserved.
Native MPcontrollerlevels1–15 labels/feel andmenu/relaunch stillUNVERIFIED;
asyncquestionpending asksusercheck10(100%)->15(150%) incontrollerOptions and
leavescreenopenforvisualQA. UseSkyfreshstate, don'tinterferewithuserinputs.
Continue afterreply, checklogs/screenshot andpersistedINI native_options fields.
121 retains117botdrawablecandidate. Bot visibilityisnextGAMEPLAYpriority:
userexplicitlyaskedaftersettings. No121botbehaviorchanges,noUAV/weaponfixclaim.
Windowfixwasrequestedmidsettingsworkandimplementedasseparate121build.
No gamefilesuploaded/networkused.
### CURRENT121 source/build — supersedes pending120 statements below

120 BOTH executables built/installed; preserved logwork/build-0.5.120-completed.log.
Source has testedactualnativeOptions MP1–15, labels, savecallbacks, default50%.
No120livecheck yet. Userclosed119; noCoD3MPprocess observedbefore121 build.
121 buildingseparately forwindowresizebug, includes120changes:
- Input::SuspendMouseCapture(bool) blocks MouseDelta warps/ReadKeyboardcapture,
  releasesheldclip. Calledduringmove/resize, nonclienttitle/borderbuttons,
  windowmode/resolutionchanges. Captureclick nowrequirespointerinsideclient.
- window_geometry.h pureFit usedbyWindow::KeepVisible afterrestore/dragresize;
  clampstodisplayworkarea, preservingvalidplacement/maximizedstate.
- actualprojectheadergeometrytest work/test_window_geometry.cpp/.exe passes
  offscreen corners/negative positions, oversized/tiny, unchangedvalidplace,
  leftmonitor/offsettaskbar. Logwork/test-window-geometry-0.5.121.log.
- mousecapture actualsourceharnesswork/test_mouse_capture.cpp beingchecked;
  prefix/suffixextractactualSetHold,MouseDelta,SuspendMouseCapture.
Live121startup, nativeLabels/level15feel, save/relaunch andwindowdragpending.
Noadditionalgameplaychanges. Bots nextpriority aftersettings/windowrecovery.
Do not falselyclaimbotvisible,UAVfixed,orweapon-switchfixed.
### CURRENT120 source/build — supersedes119A below

119A user confirms audio lower; sensitivity fullbar stillnotfaster afteradjust
because original UI clamps50. User opened native controller screen while inmatch;
this is824E7560 populate /824E7358 adjust /824E72C0 draw, NOT823D5608.
Screenshot shows simplebars with no numericvalues. Removed unused119diagnostics.
120 source now native_options.cpp hooks actualMP screen:10..150 steps10,
level/percent labelsmenu+112/+116, values+216/+220, bars+200/+204; preserves
native arrowanimation using originalthenoverridesvalue. Barhelper maps150->50.
Lookup label.vtable+132 uses persistent gueststrings, copycontext/stack-0x2000.
Yaw fallbackconstant82085A6C .29->.435deg/ms; verifiedONLYnative8252B578
loads21148? CORRECTION offset23148(lis-32248); actualaddress82085A6C.
Mountedexplicitcapbranch skipsfallback; ADSfactor unchanged. Feel uncalibrated.
Settings.h/.cpp add nativeVolume50,controllerHorizontal100/Vertical100,
load/validate/save INI native_options. Ready() restoresprimaryprefs once after
console save mayload, controller/audioapply hooks capturetoSettings::Set.
SP onlyconstructor defaults; extendedUI/persistence callbacks implementedMP.

Tests passedactualsource: work/test_native_settings.cpp includes Settings.cpp,
work/test-native-settings-0.5.120.log; test_native_options.cpp generated from
native_options_harness_prefix/suffix and actualmodule (includes removed), fakes
nativeoldclamp50, checksbounds/axis/bar/callback/restore/defaults/cap. Allpass.
Preserve test code/logs. Native label display, feel and menu/relaunch untested.
Userclosed119 toallow120 install.120 build pending/underway. Still119installed
untilDone. Record final result before asking user tolaunch.

NEW window bug: switchingwindowed/resizing pusheswindowcorner, can'tdragbar.
Likelycause: input.cpp SetHold calculatesg_centre/ClipCursor onlyonstatechange;
MouseDelta keepsSetCursorPos oldcentre while OS moves/resizes. NextSEPARATE
build suspend capture duringWM_ENTERSIZEMOVE/EXIT and modechanges; prevent
ReadKeyboard recapture /MouseDelta recenter while suspended, keep titlebar in
monitorworkarea on exit/windowedrestore. Window.cpp nofixyet. User canEscape
freecursor/AltEnter mode. Userrequestedbotvisibility priorityAFTERsettings;
recoverwindowbugthenbots beforeexpandedAI. Visibility stillunconfirmed.
### CURRENT 119 development A — supersedes118 checkpoint below

User clarified118 restart persistence has NOT been tested. No failure established.
Latest priority: finish native volume/controller patch, then invisible bots FIRST,
before expanded AI. Visible bot fix still unconfirmed. User closed118 forbuild.
119 development A built/installed both executables successfully; preserved log
chat work/build-0.5.119-A-completed.log. This is NOT a complete150% patch yet.
CoD3Host/native_options.cpp added to CMake, MP_NATIVE inmake_mp_host.py:
SP824D9C50 / MP82506E78 native preferences constructor hooks set horizontal68
and vertical72 to100, volume92/96/100 to25. Native50 audio is FULL volume:
application825095D8 uses float*.02. Existing F11 master70 remains unchanged.
MP823D5608 controller menu population hook logs widget arraymenu+4 elements
+8/+12, vtable methods192..208 and widget words16..112 (bounded4reports).
Asked user to launch119A, open native controller Options, leave screen there;
read portCoD3MP.log diagnostics to identify actual slider range API, thenfinish.
No119 gameplay/menu confirmation yet. Do NOT call150% implemented.
Frontend audio apply823F6DC0 writes92/96/100 then calls825095D8; audioadjust
823F6F40 clamps0..50 and appliesvolume. In-gameaudio824DB070 writesvolume.
Controller menuapply823D5198 writes68/72, in-gamecontroller824DB360 writes
frommenu+216/+220. Constructor defaults original24horizontal/32vertical.
Native padlook8252B578 readspercent*.01, ADSfactor separately+1632. Native
unrestricted yawcap constant0.29degrees/msec at82085A6C may caphighsettings;
no turncap change yet, no exactWaW/MW2 calibrationclaim. User wants100%=10,
150%=15, faster actualcontrollerturning (notjustF11orcosmeticlabels).
### Installed 0.5.118 — active checkpoint

Continue working; Justin requested records for continuity, not a stop/handoff.
Both executables built/installed as118; VERSION.txt verified. Build log ends
Done; preserved chat work/build-0.5.118-completed.log (117log also preserved).
One functional change: native Xam profile persistence in kernel_xam.cpp.
Writer now saves typed native records; reader uses verified40-byte layout
and rebases payload pointers. Bounds, corrupt-file rejection, atomic replace,
read locks released before completion callbacks; no guest pointers on disk.
Files stored under SavesRoot()/profiles/user-N.bin. profile_store.h is codec.

Tests passed (chat workspace work/):
- test_profile_store.cpp/.exe: actual codec disk reload, types1..6, binary/
  empty data, every truncation, corruption, duplicate IDs, count/size limits,
  failed decode preserving prior output. Log:test-profile-store-0.5.118.log.
- test_profile_api.cpp/.exe: actual helpers/imports extracted from kernel_xam
  between namespace CompleteOverlapped forward declaration and Controllers;
  wrapped by profile_api_harness_prefix.cpp/suffix.cpp. Native40-byte record
  layout, file reload, sizing/short buffers, pointer rebasing, user isolation,
  failed write preserving saved file pass. Log:test-profile-api-0.5.118.log.
- MP visibly reached menu; log confirms15-setting profile read. Captured log
  work/CoD3MP-0.5.118-startup.log. UI input injection unreliable; native menu
  save/relaunch UNTESTED. Alt+F4 injection did not close; stopped own test PID
  explicitly. No successful normal-close retest, no proven regression.

Next: native volume FIRST-RUN50%, sensitivity FIRST-RUN100%, slider150%.
These defaults/UI changes are NOT implemented118. Verify native fields and
slider max; do not force defaults every launch and overwrite saved choices.
Controller preferences MP82C6B730 stride6984: +68 horizontal/+72 vertical
integer percentages, +65 invertY. Controller Options read823D5608 and apply
823D5198, widget arraymenu+4; sensitivity widgets atarray+8/+12. Gameplay
native8252B578 reads percentage*0.01, separately multiplies ADSfactor+1632.
Slider max/default initializer and volume field still require verification.
F11-only draft withdrawn: masterVolume default/fallback70, original Apply;
no SetMasterVolume API. Installed INI master_volume70 restored. Backup:
work/CoD3Recomp-before-volume118.ini. Do not double attenuate inadvertently.

117 bot drawable candidate is retained in118; visibility STILL unconfirmed.
Latest user asks whether fixed: answer candidate installed, no live proof.
UAV and weapon switching remain open. Do not label them fixed.
CoD3Scan has safe --ascii/--words/--datafind modes; no arbitrary-data disasm.
### Latest user requirements (native game Options, not F11-only)

1. Native menu volume starts50% on first/default launch, then remembers the
   last user-set volume when closing/relaunching. Do not reset every launch.
2. Controller sensitivity starts100%, remembers changes, and native slider
   goes up to150%. Both axes may exist; inspect native menu before changing.
3. Continue bot visibility, UAV markers and weapon switching afterward.
   Preserve confirmed Kangaroo exits and native scoped L3 steady aim/sounds.
4. One functional change per build, notes every build, self-test where useful,
   concise status after tests; never upload any game data to GitHub.

### Verified gameplay / outstanding bugs

- Kangaroo left/right/rear exits confirmed good by Justin; ride height still
  looks4-6in high.112 private wheel radius18->13 initialized, visual result
  not confirmed. Bullet-hole flicker remains open after vehicle/bot work.
- Scope ADS/L3 hold-breath, sounds and walking while scoped confirmed good.
  Closing game window terminates process/audio (105 tested campaign and MP).
-113 user camera remained normal after bot_add; native FOV90 logged, but
  bot inactive.114 missing spawn-finalizer fixed: health100/state3/move0,
  movement and damage observed. Bot still invisible even beside host.
-115 gives valid class0 instead of-1, but still invisible and weapon0.
-116 exact ownership query allowance at821FB408 was hit, yet drawable
  entity+536 stays0. This did not solve visibility.
-117 adds native generic_human object factory after placement, before
  finalizer:824B5688(820653E4) ->8257C090(entity, entry), guard entry+4.
  Its live result is STILL NOT REPORTED. Do not claim visibility fixed.
- UAV activation/message confirmed but no bot marker on compass. Native
 825049C8 gates enemy_radar82B2CF90+12 AND ps+132 recent activity(15s).
  Other compass widgets still need review. No UAV change115-117.
- User cannot switch weapons using controller Y/Triangle OR mouse wheel.
  Respawn may equip pistol or Kar; then switching fails in either direction.
  Host inventory logs include Kar98_sniper21 and pistol15. Whether this
  happens before bot_add is unanswered.117 logs rising PadY source ininput.cpp;
  both controller/wheel sharePadY; sprint filter preserves that bit. Not fixed.
- All temporary debug.txt probes were restored to original move_watch1.
  bot_bring is available only throughdebug.txt; changing its value (including
  restoring0) triggers placement again. bot_session1 only logs differences;
 2 clones session state and has past crash risk, so do NOT use2 routinely.

### Evidence and settings research

Chat workspace root:
C:/Users/hovgt/Documents/Codex/2026-10-14/i-m-working-on-a-pc
Snapshots inwork/: CoD3MP-0.5.114-invisible-class-minus1.log,
CoD3MP-0.5.115-class0-still-invisible.log,
CoD3MP-0.5.116-invisible-after-bring.log. Current port log may be117 if run.

- kernel_xam.cpp XamUserReadProfileSettings currently returns allunset;
  Header8/Entry24 is wrong for native title consumer. Writer discards writes.
- MP wrapper8222C0C8(user,count,records,overlapped) invokes import with
  r3=0, r4=user, r5=count, r6=records, r7=overlapped. SP analogue82195E68.
- Native writer824135D8 builds ONE40-byte record atstack+96:
  +0 source2, +8 user index, +16 ID63E83FFF, +24 byte type6(binary),
  +32 byte length, +36 guest payload pointer. r4count1, r5record, r6overlap0
  at wrapper call. Verify layout and limits before implementing persistence.
- Native reader82414788 walks40-byte returned records, uses +0presence,
  +16ID, +32scalar/size, +36payload. Requested IDs15 include10040018,
 10040015,10040002,10040003,4064000F,402C0011,10040005,5004000B,
 10040004,1004000C,1004000D,1004000E,63E83FFF,63E83FFE,63E83FFD.
- Native profile state: four832-byte objects; MP base82BAC0D0
  (lis-32069/addi-16176).82414A60 allocates blob size4 forindex12,
 1000 forindices13/14, size512 otherwise. Native defaults/binary schema and
  precise sensitivity/volume fields are NOT established. Do not guess them.
- Native menu labels: volume8206BC28(FEMENU_COP_VOLUME);
  vertical8206BE1C, horizontal8206BE34; MPGAME sensitivity labels8206C3F0/
 8206C40C. String pointer tables at82061A30(volume),82061B40/44(horizontal/
  vertical), duplicates82063734 and820636B8/BC. Native global tables using
  lis-32163 are825Dxxxx, NOT8206xxxx; don't confuse these bases.
- mouse_look.cpp native pad hook SP8250FF48 -> generated MP8252B578;
  clientActive base MP82C2ECF0 stride6352, sensitivity factor+1632 is also
  modified forADS, so do NOT overwrite it as a persisted slider blindly.
- Existing settings.cpp F11 persistence works through Settings::Set->Save,
  but does not solve native menu/profile persistence. Avoid double attenuation
  by forcing both host master and native volume to50 without intention.

### Inspector incident / safe continuation

Broad CoD3Scan --disasm0x82000000 131072 caused memory-read Application Error.
Current inspection was stopped; csrss-hosted error dialog was dismissed and
verified absent via window list. Do not run that data-disassembly command
again. Use --ascii/--words fordata; --disasm only verified code/small known
instructions. Earlierbreath_lookup.exe also crashed; never relaunch it.
Do not claim an inspector crash was a game crash. No game files uploaded.

### Next action

Finish native Options persistence plus volume defaults/sensitivity150 with
verified layout and bounded writes. Test save/reload with actual code and
native menu if possible. Only then build118 and rewrite its pending notes
around final behavior/results. If implementation must be split acrossbuilds,
state exact scope honestly. Preserve117 bot factory change and open tests.

## What this is

A PC port of **Call of Duty 3 (Xbox 360)**, made by static recompilation with
XenonRecomp. It's a fork of FrankHUN88453/CoD3Recomp, now on Justin's GitHub.
There are two programs:

- **CoD3.exe**: the campaign.
- **CoD3MP.exe**: multiplayer. Most current work is here.

The recompiled PowerPC code is in `CoD3RecompLib/ppc` (campaign) and
`CoD3RecompLib/ppc_mp` (multiplayer). It's generated, so don't edit it by hand.

All hand-written code is in `CoD3Host/`. The game's functions are hooked by
defining `PPC_FUNC(sub_XXXXXXXX)` and calling the original through
`__imp__sub_XXXXXXXX`; the recompiled functions are weak aliases.
Multiplayer-only code is inside `#ifdef COD3_MP`.

**Owner:** Justin ("Native", "brudda"). He tests every build and wants a short
status after each test. He prefers the assistant to test changes itself where
it can.

## Never do this

- **Never upload game data to GitHub.** That includes the ISO, `.cod`/`.xex`
  files, generated `ppc`/`ppc_mp` sources, textures, shaders and exes. Only
  hand-written source and docs go up.
- **Never write into game structures past a bound you haven't verified.**
  The 0.5.95–0.5.97 crashes all came from one helper (0.5.93's weapon-marks
  fill). It walked 255 slots of the weapon list, but the list is shorter, so it
  wrote 466 pointers into random game memory. The crashes showed up somewhere
  different each time.

## Folders (on Justin's PC)

- **Source:** `D:\Call of Duty 3 (USA, Europe)\CoD3Recomp-main\CoD3Recomp-main`
- **Game / port folder:** `D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0`
  - `CoD3MP.log` is the current run's log, `CoD3MP.previous.log` the run before.
  - `CoD3.log` is the campaign's log.
  - `CoD3MP.pdb` is for symbolizing crashes.
  - `CoD3Recomp.ini` holds the settings (`texture_dump`, `watch_writes`,
    class loadouts and so on).
  - `textures\replace\` holds the upscaled textures (4x Real-ESRGAN).
  - `game\` holds the original disc files (`mp\`, `sp\<level>\*.cod`).
- **Investigation material from Justin:**
  - `kangaroo-technical-dossier\`
  - `bullet-impact-dossier\`
  - `Call-of-Duty-3-investigation.zip`: indexes and strings of every archive.
  - `CLAUDE-READ-FIRST-BULLET-HOLES.txt`

Build-tool note: this session had inaccessible cached WinGet Ninja and Python
paths. CMake caches now use VS BuildTools bundled Ninja and Codex bundled
Python; add bundled Python to PATH when running the normal build bat.

## Building and versioning

- **Build:** run `BUILD-MULTIPLAYER.bat` in the source folder. It builds both
  exes, takes about 5 minutes, and logs to `build-mp-log.txt`, which ends with
  "Done." or "STOPPED".
- **Version:** the build takes it from the **first `## X.Y.Z` heading** in
  `docs/PATCH-NOTES.md`. To make a new version, add a new entry on top.
- **Watch out:** `BUILD-MULTIPLAYER.ps1` sits next to the `.bat`, and opening
  it from Explorer's Recent list doesn't run the build. Run the `.bat`.
- **Syntax check before building (in a sandbox):**
  `clang++ -std=c++20 -fsyntax-only -fms-extensions -DCOD3_MP` with stub
  `windows.h`/`imgui.h`, plus the XenonRecomp headers (`XenonUtils` and
  `thirdparty/simde`). Errors that come only from the Windows/ImGui stubs are
  expected.

## Testing

1. Launch `CoD3MP.exe` from the port folder.
2. Main menu → System Link → Ready → Play Game → Create Match (War, Merville)
   → Auto Select → a class.
3. Menu keys: Space or Enter selects, Backspace goes back, arrows move.
   When driving the game remotely, click an item first; keys sometimes don't
   register otherwise.
4. Read `CoD3MP.log`. Every feature logs its own lines: `kangaroo:`,
   `vehicles:`, `bots:`, `marks:`, `render:`.
5. The F11 menu has settings, an FPS display and a console.

**Console commands (host code):**

- `kangaroo_tread 0-10`: how the treads are posed. 10 is the default: the
  game's own poses with the Kangaroo's wheel list corrected.
- `kangaroo_type 1|2|3`: 1 is the default (four-wheel kind, tank preset;
  drives and can be entered, but slow on hills). 3 (tracked physics flag on a
  four-wheel kind) drove broken: rocking on hills. 2 (whole tank kind) could not
  be entered at all. Both tested by Justin on 0.5.100.
- `kangaroo_build 0|1|2`: how the Kangaroo is made at map start. 1 is the
  default (a Kangaroo from spawn with the tank physics preset), 2 is the same
  but keeps the Horch's physics, 0 is the old pre-0.5.99 way.
- Bots: `bot_add`, `bot_setup N`.
- Killstreaks: `streak_give uav|artillery|typhoon`.
- Vehicles: `spawnvehicle <kind>`, `kangaroo 0/1`, `vehicles_everywhere 0/1`.

## How the Kangaroo works now (multiplayer, `mp_fixes.cpp`)

The Kangaroo APC is a campaign vehicle. Its model `v_Kangaroo` comes from
`sp\mace2\kangaroo.cod`. On Merville and Poissons it's loaded after the map's
own archive, because its tread and wheel shaders `v_tracked_tread` and
`v_tracked_wheel` live there.

**1. A kind of its own.** `MakeKangarooKind`, called from `sub_82561838`,
copies the Horch's vehicle file into a new kind, `mp_kangaroo`.
`KangarooPrepareKind` then fills it in:

- driving, bounds and health from the Sherman;
- tread scroll from the Sherman (texureScrollScale +76 = 5.5);
- `vehiclePhysics` +540 = `mp_tankdefault` (the Sherman's);
- subtype 8, curve `Kangaroo`, 8 seats.

Vehicle file field table: 0x825FE3C8. Kinds table: 0x82B2DC90; count at
0x82B34594.

**2. Made a Kangaroo as it spawns (0.5.99).** `sub_8257E3A8` (entity r3,
record r4, kind r5) sets a vehicle up: seat tags, then the physics object at
record +1304. That object is built from the kind's preset and the model's
bones and size. Our hook does this, in order:

- for each saved Kangaroo placement on the map, takes the map's Horch;
- registers its entity as a Kangaroo;
- sets the `v_Kangaroo` model;
- swaps r5 to `mp_kangaroo`;
- calls the original;
- only with `kangaroo_type 3`: sets the tracked flag 0x20 on the physics
  object (+640). Not the default: it drove broken.

Then `sub_82564520` (map start) moves it to its saved place from
`vehicles.txt`.

**3. Tag stand-ins.** `sub_82434E60` answers tags the model lacks:
`tag_enter_left/right` become the middle wheels, the hatches become driver
and gunner, and so on. This only applies to entities in `g_kangarooEntities`.

**4. Wheels.** The physics object keeps its wheel bone numbers at +36, found
by the name order at 0x825D5C6C. `FixKangarooWheels` re-checks them each
frame. When it was built from the Horch, bone 4 (`tag_tread_left`) was turned
like a wheel, which was the old spinning tread.

**5. Treads.** `sub_82568600` adds the distance driven to record +1308/+1312,
scaled by kind +76. `sub_82434918` poses the tread and sprocket bones from
those distances (the shader scrolls by the tread bone's pose).
`sub_8242B938` poses the vehicle and, with flag 0x20, calls `sub_8241E908`,
which aims the treads along the wheel points.

**Physics object layout:** preset pointer +0x250, entity +0x270 (624), flags
+0x280 (640). Presets are looked up by `sub_82418520(name)`; the object is
initialised by `sub_82420998`, and `sub_8241BB90` drives the wheel joints.

**Entity layout:** origin +336, yaw +356, mins +240, maxs +256, DObj +492.
A DObj's skeleton is reached through model +128 → +36.. → +8 (count +16,
12-byte entries from +20 with a name hash at +4; the hash is h*33 + tolower).

## Current continuation (0.5.112)

Justin's 0.5.107 log proved every passenger/rear exit reached the hook but
failed grounding. 0.5.108 corrects a definite native trace layout error:
hit position is +0, plane normal +16, so normal Z is +24, not +8. Installed
builds 0.5.109/110 include that fix, and Justin now confirms side/rear dismounts work well.

Justin added requests for fixing bot_add camera zoom, bot cover/head aim/
objective and enemy routing, and sniper L3 steady aim. Separate builds:
109 scopes bot setup to its own CurrentClient; 110 adds tactical behavior;
111 preserves MP native +sprintbreath through the tap-sprint filter. Latest
clarification: walking forward stays scoped, L3 is breath while scoped,
sprint latch cancels during ADS, and only manual aim-out leaves the scope.
Both builds for 109/110 passed. Planner and input tests passed against actual
source headers. 111 also built and installed after fixing the console handler's missing input.h include.
Justin confirms L3 hold-breath, scope behavior and native sounds work.
Camera zoom still fails on 0.5.111; context isolation was insufficient.

Driving/treads remain good; ride height and bullet flicker are still open.
Closing X in both executables was actually tested on 0.5.105: close marker
logged, process exited, console/audio stopped. No hidden game tests since
Justin reported hearing invisible test audio. Keep one feature per build.

## Open items, most wanted first

1. **Kangaroo exit placement: CONFIRMED WORKING by Justin on 0.5.111.** 0.5.106
   did not move any exit: live log used 8257AD34, outside its allowlist.
   Hook 8257ABF0 AFTER native dismount cleanup and origin placement;
   require player link +900 zero and client +240 bit 0x100000 clear.
   Entry tags from 82571A58 are remembered per player/vehicle: left/right
   return to that side, rear/turret returns behind (local negative X).
   Full player-box sweep above hull then down checks grounded clearance.
   IMPORTANT: trace +0 is hit position, +16 normal, +24 normal Z; the
   old +8 slope check rejected all Merville ground. Watch new trace values.
   Watch `kangaroo: exit: placed on left/right/rear` or failed landing.
   Console `kangaroo_exit 0` restores native placement, 1/2 fallback left/right
   when no entry tag is recorded. Preserve this working code.
   - sub_8255E118 updates authoritative player/entity origin.
   - Trace sub_82534200: result/start/mins/maxs/end/filter in r3..r8;
     flags r9/r10 and stack +84/+92; own guest scratch/stack.
   - Stable entry rotation from 0.5.104 remains. Keep build 2 (Horch
     physics), type 1, tread mode 10; ride height unchanged.
2. **Bot_add zoom.** 0.5.109 scopes create, side/kit callbacks and respawn
   to CurrentClient 82BCBE9C, like existing bot movement. RAII restores the
   original context and logs host ADS/weapon before/after. bot_camera 0/1
   rollback. Justin confirms zoom still fails on 0.5.111. Host ADS/weapon
   are unchanged around setup and cg_fov stays90. Next isolation: fresh match,
   bot_setup 3 then bot_add 1 skips both menu callbacks. Asked Justin via
   async test question; await result before another speculative fix.
3. **Tactical bots.** 0.5.110: bot_tactics 0/1 rollback, head/eye-height aim,
   LOS and pitch firing gates, reaction after visibility returns; collision
   route stepping, stuck escape, damage/low-health nearby cover and crouching.
   Pure header planner tests passed six scenarios. Live behavior untested.
   Current objective source is Domination::BotGoal; route steering is local,
   with no full-map navmesh/maze guarantee. Grenades require visibility.
4. **Scoped L3 hold breath.** 0.5.111 fixes tap-sprint erasing physical L3
   during ADS. Native MP config binds L3 +sprintbreath (breath/timing/sounds
   already exist). Walking keeps ADS; sprint latch is cancelled; no auto
   aim-out. scope_breath 0/1. Input tests pass. Campaign has different
   binocular/breath mappings and retains its old mapping. Justin confirms
   native MP steady aim/sounds/scoped walking work on 0.5.111.
5. **Kangaroo rides slightly too high.** 0.5.112 built/installed: private
   208-byte Horch physics preset (copy204 verified bytes), wheel_radius +20
   reduced from18 to13 by default. Hook82420998 changes r5 before native init.
   Only registered Kangaroos; shared presets untouched. Console kangaroo_drop
   0..8 (default5), then map_restart; 0 original. Visible runtime log confirms
   radius18->13. Actual visual/driving test pending; initial stationary origin
   still logs -105, so do not claim a measured five-unit hull drop yet.

6. **Bullet holes come in slowly and flicker (campaign and multiplayer).**
   - 0.5.94 made vertex buffers under 1 MB always hash whole, so holes appear
     much sooner.
   - 0.5.99 added a write-watch for big buffers within a frame
     (`render_resources.cpp`, `liveSince`). It never fired in testing, so the
     marks aren't rewritten inside a big buffer mid-frame.
   - Next suspects:
     - (a) The decal draws' depth offset. `render_commands.cpp` moves the
       constant poly offset (0.0001) into vertex depth, and the Z pass is
       pushed "behind". Check whether marks z-fight when the camera moves.
     - (b) Small buffers (≤256 KB) are only sample-checked (64 words) in later
       draws of a frame.
     - (c) The index buffer path, which is page-watched once stable.
     - (d) The game's dynamic decal manager itself (`dynamicdecalmgr`, MP
       `sub_825583F8`; see the bullet-impact dossier).
   - Useful env vars:
     - `COD3_VBAUDIT=1` with `COD3_DENSEALL=1`
     - `COD3_NOLIVEBUFFERS=1`
     - `COD3_OLDPOLYOFFSET=1`
     - `COD3_NOPOLYOFFSET=1`
     - `COD3_NOMIDFRAME=1`
7. **Campaign menu, UI and text upscales,** a few images at a time:
   1. Set `texture_dump=1`, browse the menus, then set it back to 0
      (dumping during movies hangs the campaign).
   2. Run `LIST-DUMPS`.
   3. Upscale 4x with Real-ESRGAN.
   4. Put the results in `textures\replace\` under the dumped hash name.

## Working style that has worked

- **Change one thing at a time, log it, and test it.** Each change gets a
  console switch to put the old behaviour back.
- **When a crash appears, reproduce it and read the log** before blaming the
  newest change. 0.5.96 and 0.5.97 undid innocent changes because of that.
- **Write a patch-note entry for every build:** what Justin saw, why it
  happened, what changed, and how it was tested.

0.5.106 test update: both executables built and installed. Scripted devmap mp_merv loaded and ran without a crash. The host stayed at its spawn (4741,320,-116.7), despite queued setviewpos/use, so no exit hook was exercised. Test process 16356 was stopped explicitly. Passenger/rear turret side exits still require a real seat test. Ride height remains a separate investigation; saved spawn Z -128 versus stationary Z -105 is evidence, not a confirmed cause.

0.5.107 continuation: user confirms 0.5.106 still fails right/rear exits. Live log only contains unlink callers 82571CFC (relink) and 8257AD34 (native vehicle dismount), never the 0.5.106 allowlist. New hook wraps 8257ABF0 and applies after native origin setter/cleanup, with link zero and client+240 bit0x100000 cleared. Entry tags remembered by player/vehicle through 82571A58: left/right/rear. Rear sweeps to local X below hullMinX; sides to local Y outside bounds. Build/test pending. Old open-item description above describes 0.5.106 and is superseded by this continuation.

0.5.107 test status: both builds passed, installed VERSION 0.5.107. Merville startup passed. Background test PID21852 was stopped and absence verified after user heard invisible test audio. It never reached a dismount; no claim of live seat validation. Do not launch another hidden audio-producing session while user is testing.

0.5.108: all 0.5.107 exits failed grounded check. Found definite layout error: trace hit position +0, normal +16, fraction +32; slope must read normalZ +24, not positionZ +8. Native producer 824A0468 proves start+travel*fraction goes to +0 and plane to +16. Fixed and added per-candidate fraction/height/normal diagnostics. User requested bot cover, head aim, objective/enemy routing while finishing exits; implement as separate subsequent build. No hidden audible gameplay tests.

0.5.109 camera investigation: bot creation, side/kit selection and respawn lacked ThinkBots' CurrentClient scope (82BCBE9C). Added RAII scope with stage/slot and host ADS/weapon diagnostics, console bot_camera 0/1 rollback. This corrects a definite inconsistent context, but zoom causality still needs live bot_add confirmation. Do not claim camera fixed based on compilation. User also requested cover/head aim and better objective/enemy routes; next separate build. 0.5.108 both executables built/installed; no hidden game run.

0.5.110: tactical bot layer with bot_tactics 0/1. bot_navigation.h pure collision steering/cover planner, tests in chat work/test_bot_navigation.cpp passed six scenarios. bots.cpp native traces use own guest stack + corrected result layout, 200ms LOS cache, head/eye-height aim, pitch gate, post-visibility reaction, collision route steps every250ms, damage/low-health cover every2.5s, four-second shelter. Existing Domination objectives only; local routes do not guarantee escaping large mazes. Both builds pending. 0.5.109 camera isolation built/installed but live zoom test outstanding. User requested sniper ADS L3 steady aim next, separate build.

0.5.111 sniper input: original MP config/mp_xenon_a.cfg binds L3 +sprintbreath. Tap-sprint filter stripped this while ADS. New sprint_input.h preserves raw scoped L3, cancels latch, reports ADS movement as walking. No automatic aim-out or ADS movement restriction. Native game supplies breath/steadiness/recovery. scope_breath 0/1 rollback. Campaign mapping untouched (SP binds L3 binoculars, Up holdbreath). Compiled input-state tests passed; builds pending. 0.5.110 both builds passed/installed with corrected visibility accepting enemy handle as well as clear fraction. All live tests remain outstanding; do not claim gameplay confirmation.

Final installed status: VERSION 0.5.111. Both exes built successfully after repairing the missing input.h include. Bot planner and sprint-input tests were recompiled against the actual source headers and passed. No hidden game launch. Live exit, camera, tactical AI and breath/sway tests are still required. Native hold-breath inhale/exhale assets exist; user offered replacement audio, but no replacement is needed until native behavior is verified. Next user test should isolate Kangaroo right-side/rear exits first, then bot_add camera, then tactics and scope controls. Ride height and bullet flicker remain open.

0.5.112 continuation: Justin confirms the Kangaroo side/rear exits work and feel good on 0.5.111. Remaining visual gap estimated 4-6 inches. Native physics table 825FEAF0 has wheel_radius +20, susp_adj +32, name +112; constructor 82418C80 last float +200. Hook 82420998 before initialization, give only registered Kangaroo entities a private 208-byte copy (copy204 known bytes), reduce mp_horsch radius by5, min8. No shared preset mutation, no entity teleport. kangaroo_drop 0..8 (default5), next map start/restart. Build pending. Other behaviors untouched. Asked optional status of bot_add camera on 0.5.111 before changing camera again. Inspection helper breath_lookup crashed from local executable-data reading and was stopped; use existing CoD3Scan, no more helper launches.

0.5.112 runtime/current camera research: current visible game PID4456 belongs to user/test, leave its controls alone. Header/notes say112, both exes installed. Native FOV calculator 82453810 reads CurrentClient82BCBE9C and host record ADS+172; base cvar float82BA2BC0; local view array82C7D490 stride6368, cached weapon pointer+4732, ADS transition flag+4644, zoom tween+6212/+6216/+6220/+6224. Camera callbacks 824553A8/824553F0 and824674E0 write/use FOV+96 in camera object; +48 gates recomputation, +272/+276/+280 tween. 824674E0 camera+480 selects local-view array. No camera writes or hooks added yet. Existing generated code inspected only; no edits. Helper breath_lookup was stopped after crash; use existing CoD3Scan for inspection (disasm supports data words).

0.5.113: Justin confirms native sniper steady aim/sounds and exits work; zoom persists on111. Optional bot_setup3 isolation was not run. Camera-focused candidate fix bypasses both local menu callbacks by default (g_botCamera1), directly writes team+4 and existing SessionPlayer+604 as native823EA4E0 does, then existing server respawn. No forced host FOV. bot_camera0 legacy path. Added observational FOV hook82453810 (f1 return) and camera824674E0 (object+480 local index0; fields+48,+96,+272,+276,+280), throttled2s. Build pending; need fresh match bot_add, camera/teams/runtime. Current game112 user actively testing; do not seize controls or kill it. Height112 private radius18->13 initialized, actual hull drop not yet confirmed.

### 0.5.113 build and latest isolation result

Both executables built and installed successfully. Justin tested bot_setup 3,
then bot_add 1 on 0.5.112: no visible bot and no zoom. Log confirms slot 4 was
created and the spawn path ran, but health stayed 0, move type 6, state 0.
This is an inactive bot, not a successful live spawn/camera fix. Snapshot:
chat work/CoD3MP-0.5.112-setup3-inactive-bot.log. Team/menu setup is implicated,
but actual cause remains unproven. 0.5.113 uses direct team assignment and
keeps the local menu callbacks out; native FOV diagnostics are included.
Next fresh-match test must use bot_setup 1 before bot_add 1, since setup 3
skips the new team assignment too. Verify a living visible bot, teams and
normal hip/ADS camera. Sniper steady aim/sounds and Kangaroo exits confirmed.
Kangaroo actual ride height remains unconfirmed. No game is running now.

### 0.5.113 live result: camera normal, bot still inactive

Justin ran bot_setup 1 / bot_add 1: view remained normal, no bot found.
Log verifies direct side 1 assignment, no SessionPlayer, and spawn leaves
slot 4 at health 0, move type 6, state 0. Actual native and camera FOV remain
90 while hip firing. Camera symptom improved, but live bot spawning is NOT
fixed. Do not send Justin searching for a living bot until spawn succeeds.
Existing console streak_give uav grants a charge; d-pad right calls it.
UAV draws enemies and cannot reveal an inactive bot as a living enemy.
Snapshot in chat work/CoD3MP-0.5.113-normal-view-inactive-bot.log.

### 0.5.114: missing native bot spawn finalizer

Native offline spawn 82406BB8 calls 82593608(entity, origin, angles, 1, 0),
THEN callbacks+3312(handle pointer, 0). Bot hook 824D0AF0 only did placement.
82593608 explicitly clears ps+304 health. Default finalizer 82211DF8 sets
state through callback table+4944 (value3), health/loadout through game APIs.
Added the missing finalizer with the bot's handle and existing isolated
current-client scope. No forced health or state writes. Build pending;
needs fresh-match bot_setup1/bot_add1 with visible live bot and normal ADS.

Justin's enhanced battlefield AI roadmap: squad coordination (advance,
defend, support, attack together); dynamic cover (peek, crouch, relocate,
avoid exposure); suppression/morale (pinned vs aggressive); tactical
awareness (last known enemies, gunfire, flanks, grenade response); human-like
combat (natural aim, reload choices, mistakes, personalities). Implement in
separate reviewable changes after living bots spawn reliably. Existing110
local collision/cover/head aiming logic needs real combat validation first.

0.5.114 result: both exes built/installed successfully. Visible launch via
computer-use reached multiplayer menu, title confirms114. Click/Down/Space
inputs did not advance native menu; no match or bot runtime validation yet.
Game intentionally remains visibly open at menu for Justin's fresh-match
bot_setup1 / bot_add1 test. Do not claim health/spawn is fixed from build.
No test debug.txt modification was made; existing move_watch1 remains.

### 0.5.114 live confirmation / 0.5.115 class fix

Finalizer82211DF8 succeeds: bot health100, state3, move type0. Justin cannot
see him even after debug bot_bring1 places him beside host; bot subsequently
moves/chases. debug bot_session1 logs differences (read-only, no session
clone); restored original debug.txt containing move_watch1 afterward.
Class log explicitly shows bot class -1; ps+1920/+1922 FFFF, entity+536 model
pointer0, weapon0. Native getters820CE3D8/820CE538 read these halfwords;
8220D1C8 calls82206028 with sign extended class. Initialize both to0 for
new bots instead of join sentinelFFFF. Native finalizer supplies valid kit.
115 build pending. Need fresh match bot_setup1/bot_add1 model/weapon/camera.
Snapshot chat work/CoD3MP-0.5.114-invisible-class-minus1.log. UAV reports
activation but actual bot markers not verified; implementation simply enables
enemy_radar at82B2CF90+12, native compass825049C8 and related widgets need
inspection if radar still misses live visible bots. No UAV code change115.

UAV read-only finding during115 build: native825049C8 gates on
EnemyRadar82B2CF90+12, then enumerates opposing player entities. Each candidate
also requires ps+132 !=0 and ps+132+15000 >= server time (82504AE0 onward).
This path still filters recent gunfire even with radar enabled. Thus activation
alone does not prove continuous bot-location markers; review other compass
widgets and this timer before a separate UAV fix. No UAV source changed115.

0.5.115 installed: normal build completed successfully for both executables.
Game remains closed; no new gameplay test launched. Next: fresh match,
bot_setup1 / bot_add1; check log class0 vs previous-1 and visible soldier/gun,
normal hip/ADS camera, combat. User can call streak_giveuav then d-padright,
but UAV continuous markers remain a separate unresolved issue. All diagnostic
debug.txt edits from114 have been restored. No uploads or network activity.

### 0.5.116 presentation investigation

115 confirmed class0 at82206028, living bot still invisible, weapon0. Native
ownership query824C3490 calls823E5CC0, which searches FOUR LOCAL session
entries for the entity; bots have none. In model selector821FB3E0, exact
query return821FB408 gates model definition assignment via callbacks+2752
(native824CB9E8 storesps+200), then821FB300 selects accessories. Hook only
that query/callsite for a registered bot with valid handle generation. No
other ownership overrides, session copying, hardcoded model or input changes.
116 build pending. Needs visual/camera confirmation; may need later native
model build/equipment dispatch work. Weapon0 and UAV remain open. Native
UAV825049C8 still uses ps+132 recent-activity filter while radar enabled.

Justin asks whether bots can be configured in pre-match settings and whether
his friend can see them. Explained current design is authoritative host AI;
pre-match count/difficulty/team controls can be added. Friend-side visibility
and state replication are NOT verified. Need two-client test after local
model/equipment works; do not promise session/network correctness merely
because host AI runs. Existing SendClientSnapshot hook skips packets TO bot
clients only, not necessarily bot entities from real clients' snapshots.

0.5.116 built/installed successfully for campaign and MP. No gameplay test
launched; prior automated menu inputs do not register reliably. Source and
native callsite review passed; live model/normal camera still required.
Next fresh-match bot_setup1/bot_add1, inspect model eligibility log plus
class0, visible soldier, weapon (still possibly0). No UAV change this build.

### 0.5.117: native drawable creation, weapon switch report

116: bot class0 alive, ownership allowance logged, still invisible when
brought beside Justin; entity+5360, +540 has definition. Debug probes restored.
Missing native network setup step found:823F818C and82400D40 lookup text
820653E4 (CoD3Scan verifies generic_human) through824B5688, then call8257C090
(entity, entry). Factory82516590 creates separate instance;8257C090 stores
it inentity+536 and calls8257A6C8 to rebuild rig. Added after placement and
before114finalizer if drawable absent; guard entry+4 per824CDFAC. Logs entry
and drawable.117build pending. Need visual/camera test; don't claim fixed.
User also cannot switch weapons with controller Y/Triangle or mousewheel;
respawn equips pistol or Kar but cycling both ways fails. Host log has
primary21(kar98_sniper) and pistol15, inventory exists. Await optional fresh
match BEFORE bot_add comparison. Input-edge Y diagnostics added117 only,
no weapon behavior change. Both inputs map toPadY; sprint filter preservesY.
Snapshot work/CoD3MP-0.5.116-invisible-after-bring.log in chat directory.
UAV remains unresolved: message activation but no botcompass marker.

117 result: both executables built/installed, native source review passed.
No gameplay launch this build. Game closed for installation. Next test should
check Y AND wheel before bot_add, then bot_setup1/bot_add1; inspect117factory
log for nonzero entity+536, and visible bot after bringing near if needed.
All prior debug.txt modifications restored. Host inputY logs distinguish
controller/keyboard-wheel and remain diagnostic only. Weapon switching and
UAV are unresolved, not repaired117. User wants concise status each test.

### 0.5.118 volume persistence (separate user-requested patch)

User requests50% starting level, then remember last volume. Existing F11
slider only updates g_edit; Settings::Set/Save requires Apply. Added volume
only SetMasterVolume(int), clamps0..100, changes live setting under mutex,
saves immediately on slider change. F11 explains auto-save. Default/fallback
50 instead70; existing saved values respected. Installed INI set50 once,
backup chat work/CoD3Recomp-before-volume118.ini. Tests/build pending.
No bot/UAV/weapon changes118.117 drawable change awaits live result, and
weapon switching with Y/Triangle AND mousewheel remains unresolved; first
fresh-match test before bot_add still requested. Do not lose AI roadmap.









