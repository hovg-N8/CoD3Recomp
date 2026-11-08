# Claude session 2026-11-02 evening to 2026-11-03 ~16:40: HANDOFF (read this first)

**State: source and install 0.5.769. TANKS INSTALLED ON SEELOW for the user to drive (11-04 ~01:15): Seelow's seelow.cod = the tank scene (copy: logs-backup\seelow.cod.with-tanks; before: logs-backup\seelow.cod.before-veh), game\mp\mp_merv\mp_merv_vehicles.cod = the db pack (copy: logs-backup\mp_merv_vehicles.dbpack.cod), vehicles.txt = the user's 12 Seelow lines + Merville's 2 (= logs-backup\vehicles.txt.seelow-try; before: vehicles.txt.before-seelow2). seelow_install.sh now does both (VEHICLES=0: none). Nothing committed. Friend zip on hold.**

## 11-05 ~12:35: 0.5.780 installed (untested): named events + cue shots
- The user on 779 (logs-backup\20261105-user-play-tankguns6): no shot audio, no explosions, and "no audio
  when driving" (yet the log has PanzerIV_Idle/Move found, as on 775 when heard: ask again). 0 "tank
  event" lines: weapon events (sub_82555160) stop at the weapon's event set in the MAP's table before
  sub_82554BA0; NAMED events (sub_82556868) go straight to sub_82554BA0 (779's scope switch applies).
- 780: shots = Merville cues via sub_824AFD70 (MpFixesPlayCue; 778 proved audible); cannon flash = named
  weapon_<turret>_weapon_world_flash; impact = named projexplode_<turret>_dirt on own origins from the frame
  (Hellfire way) + cue SHELL_EXPLO_DIRT_TANK at the spot. Event names listed in PATCH-NOTES 0.5.780.

## 11-05 ~12:20: 0.5.779 installed (untested): tank events from Merville's event table
- The user on 778 (logs-backup\20261105-user-play-tankguns5): turret MG and cannon heard (sub_824AFD70
  cues worked), but no explosion ("firing a dud"); and: no grenade stand-ins, "exactly like how a tank
  shoots". Cause: weapon events (sub_82554BA0) look up the MAP archive's event table: scope
  *(*0x82BCC828 + 40). 779: for tank-gun event names that scope = the vehicles' pack during the call
  (Merville's own event table: shots, MGs, projexplode_<turret>_<surface> -> tank_* effects in the fx
  pack). All 776-778 stand-ins removed. Log: "vehicles: tank event ... made from Merville's table".
- If impacts still unseen: check the "tank event projexplode_..." lines (made? -> effect lookup scope),
  and whether the impact event goes through sub_82554BA0 at all.

## 11-05 ~00:20: 0.5.778 installed (untested): cues via sub_824AFD70, frag blast with parking
- The user: "Smoke fix + real sounds". mp_merv.wbk is Treyarch's NSLB (XMA), not XACT: no extractor.
  Instead the shot cues are played through the game's own cue-play call sub_824AFD70 (args copied from
  its caller sub_823AFFE0; yours with zero vectors = not placed). Blast: fraggrenade event 56 on 6 own
  origins, parked 60000 down after 4 s. Waiting on the user's play ("shot cue ... played" lines).
- If still silent: try r7 0 / the player's position vectors; or the vehicle's own sound path (the engine
  cues play: find where vehicles start a cue on their emitter).

## 11-05 ~00:20: 0.5.777 played: lingering smoke, still silent guns (decision pending)
- The user: "a weird shot with smoke that just sits there and doesnt disappear, no explosion sounds or
  audio from the machine gun turret". Log (logs-backup\20261105-user-play-tankguns3, copied while the
  game still ran): PANZERIV_FIRE_1ST (590652) and MG50CAL_FIRE_1ST (589983) found and asked for at the
  player via sub_82556868 (36 MG rounds) yet unheard; 8 blasts replayed as panzerschreck event 56 on our
  origins: its smoke stays on the (persistent) origin, no blast seen or heard.
- Options offered: (1) quick: replay a frag grenade's explosion instead and park the origin after a few
  seconds (no lingering smoke); (2) reliable gun sounds: extract the tank sounds from mp_merv.wbk to WAV
  (new tool: XACT wave bank + XMA via FFmpeg, cue table in the .gdbb) and play them with Audio::PlayClip
  like the Webley/Mosin; (3) proper: merge Merville's tank entries into Seelow's .dbb/.gdbb.

## 11-05 ~00:05: 0.5.777 installed (untested): shell blasts on our own origins, shots at the player
- The user on 0.5.776 (logs-backup\20261104-user-play-tankguns2): explosion heard not seen; no MG or
  turret shot sound. Log: cues found (MG50CAL_FIRE_3RD; SHERMAN_FIRE_3RD -1 -> SHELL_EXPLO_MAINIMP_TANK at
  each shot = the boom heard) but played at the gun's entity (not a player) = unheard.
- 777: shell event 56 queued (MpFixesQueueTankBlast) and played from MpFixesPlaneFrame via
  MpFixesKillcamFx as the panzerschreck's on our script_origins; shots: player within 250 of the gun ->
  1ST cue at the player; Sherman cannon tries PANZERIV_FIRE before the boom. Waiting on the user.

## 11-04 ~23:15: tank engine sounds WORK; guns via Rouen's database (0.5.776 installed, untested)
- The user on 0.5.775: tank audio heard, no gunshots or explosions (logs-backup\20261104-user-play-
  tanksound1: PanzerIV_Idle/Move, TANK_HATCH_* found; no gun cue ever looked up).
- Cause: weapon events (sub_82556180 name+event) use the MAP's database: Seelow = Rouen's .dbb (event and
  impact tables: projexplode only bazooka/panzerschreck/grenades; PanzerIV_Turret/MG50cal only ring-off)
  and .gdbb (cue tables). Merville's tables are not consulted for events.
- 0.5.776 (CampaignEventName): turret event 56 -> named "panzerschreck" (its explosion); cannon event 25 /
  MG event 2 -> cue played by name at the shooter (sub_82556868), first found by sub_8212B268 of
  PANZERIV|SHERMAN_FIRE_1ST/3RD, SHELL_EXPLO_MAINIMP_TANK, SHELL_EXPLO_MAINIMP / MG50CAL_FIRE_*,
  MG30cal_FIRE_*. COD3_TANK_SOUNDS=0 off. Waiting on the user's play ("vehicles: ...'s shot sound" lines).
- Proper fix later: merge Merville's tank entries into Seelow's .dbb/.gdbb (format work) for its own
  impact effects (tank_dirt etc.) instead of the panzerschreck's.

## 11-04 ~22:55: tank sounds and explosions still open (0.5.773-775 installed); fires under the cars
- The user: still no sounds, no explosions (0.5.773). Fires: "put the fires under the cars" (day and
  night): seelow_install.sh SCENE_FIRES at each car's origin - 40 (was - 15); BOTH packs reinstalled
  (backup logs-backup\seelow-packs-before-fires-under\day|night). Picture seelowfx4: fire low at the car.
- Packs now: game\mp\mp_merv\mp_merv_vehicles.cod = database only (0.8 MB), mp_merv_vehiclefx.cod =
  the vehicles' effects (5.0 MB, in every list, g_vehicleFxArchive). seelow_install.sh builds both.
- Sounds: the packs register mp\mp_merv\mp_merv.wbk with no drive -> dlc5: NOT FOUND (773). 774: D:
  path (opens). 775: registered + asked to load when the map's bank registers, as the campaign banks
  (log: "sounds: Merville's bank D:\mp\mp_merv\mp_merv.wbk registered beside mp\seelow\rouen.wbk ->
  5D11016C, asked to load"). Test seelowfx6: nobody fired; only Tank_Suspension asked (-1, at spawn,
  maybe before the bank finished loading). UNVERIFIED: the user must drive/fire and "check the log".
- Explosions: the effects pack is in the lists; whether tank_* is looked up: 774+ logs tank_*,
  projexplode*, weapon/* names past the asset log's 4000 ("assets: tank_dirt ..."). No shell fired since.
- If sounds still fail: check whether cue lookups (sub_824AFE80, OrderedCueLookup) see the slot of
  mp_merv.wbk (g_campaignSlots), and whether Tank_Suspension is asked only once at spawn (cached -1).

## 11-04 ~21:45: shell explosions (0.5.771-772; pack with the tank effects installed)
- The user: MGs work, but the tank shell makes no explosion. Their play (in test seelowguns1's window)
  logged 6 Panzer shell hits (event 56): the effects (tank_<surface>, via projexplode_*_turret_* in the
  pack's database) were not in the pack. Pack rebuilt with the vehicles' effects (tank_*, exhaust_*,
  treadfx_*, peeloutfx_*, wheelfx_*, vdamage_*, vfire_*, vexplode_*, the tank mf_ only): 5.5 MB, installed
  (copy logs-backup\mp_merv_vehicles.dbpack-tankfx.cod).
- Traps on the way: 0.5.771 put the pack back in the map's lists -> the map read all 24 Merville
  WEAPONFILEs (76 weapons), every own/campaign gun "left out" (Mosin green); 772 reverted that. With
  every mf_ in the pack the Mosin still glowed green (players' first-person flashes); tank mf_ only: fine.
- NOT yet seen: a shell explosion (no shell fired in fx2/fx3). The user's next shot decides; if still
  none, the effect lookups may need the pack in some list at impact time (not the map list: weapons!).

## 11-04 ~21:20: the tanks' guns (0.5.770 built and installed)
- The user's play (logs-backup\20261104-user-play-tanks1, 23 min, no crash): "the vehicles work except
  shooting": turret and mounted gun do not turn or fire. Cause: the guns are WEAPONFILE weapons
  (PanzerIV_Turret, Sherman_Turret, mg30cal_sherman_tank, mg50cal_sherman_tank_gunner, + Horch/WC51
  gunners) in Merville's game database; only VEHICLEFILEs were read from the pack.
- 0.5.770 reads them in the sub_8249E970 hook (filter g_vehicleWeaponFilter: turret/_tank/_gunner).
  Check seelowguns1: 6 set up, 54 -> 56 weapons with Laison's; no crash. Waiting on the user to fire.
- If firing still fails: look at the vehicle kind's turretWeapon/turretGunnerWeapon fields (cfgstrb keys)
  resolving by name, and at tankshell (a projectile model in Crossroads' archive).

## 11-04 ~01:15: tanks installed for play
- The user: "Yes, install them". Installed as in the State line. To undo: copy back
  logs-backup\seelow.cod.before-veh and vehicles.txt.before-seelow2, delete mp_merv_vehicles.cod.
- tools/waw/seelow_install.sh: `scene.py vehicles` after the level scene (day only), and the db pack
  built into game\mp\mp_merv\mp_merv_vehicles.cod when missing (VEHICLES=0 skips both).
- Waiting on the user: drive a Panzer and a Sherman ("check the log"). Watch for: heap (602.7/608 MB in
  the test), the vehicles' tilt (the host's move to the saved spot may flatten WaW's pitch/roll), turret
  and treads drawing (v_tracked_tread/wheel come from Saint-Lo's or Merville's? check), hits/damage.

## 11-04 ~01:05: TANKS ON SEELOW WORK (0.5.769, test seelowtanks1; install reverted after)
- The user: "Yes, keep going". 0.5.769 + scene.py vehicles + the database pack: test seelowtanks1
  (logs-backup/20261104-010202-seelowtanks1-result): "seelow's own at its start: mp_panzeriv x5,
  mp_shermanTank x5"; "12 saved: 10 of the map's own moved there"; pictures: the Panzer IV behind the
  stone wall at 3814 4780, Shermans by the water tower, all textured; Seelow's houses/walls/ground whole;
  War 3 flags; 237 s, no crash, no freeze.
- Heap 602.7 of 608 MB (as with Merville whole); 2319+54 guest DbgBreakPoints again while Laison's
  mortar effects load (none with the db pack alone: Saint-Lo's archive in the lists brings them back;
  harmless so far). Driving NOT tested (tests cannot drive); the Kangaroo and BMW lines are skipped (no
  record of those kinds; BMW has no campaign model).
- To install for play: seelow.cod through `python -I tools/carentan/scene.py vehicles <seelow.cod>
  <seelow-work\mp_seelow.ents>` (test copy: scratchpad seelow_veh.cod), the db pack as
  game\mp\mp_merv\mp_merv_vehicles.cod (scratchpad mp_merv_db.cod; recipe in the ~00:50 section), and
  vehicles.txt's Seelow lines (logs-backup\vehicles.txt.seelow-try: the trigger for loading the packs).
  Not yet in seelow_install.sh.

## 11-04 ~00:50: step 1 WORKS: Merville's game database alone (no build; install reverted)
- The user: "Yes, start step 1". Built (scratchpad, game data) with:
  `python -I tools/kapf/repack.py <game>/mp/mp_merv/mp_merv.cod <out>/mp_merv_db.cod --keep '^$'
   --drop '^(?!.*\.(gdbb|dbb|cfgstrb)$)' --follow-imports`  -> 1 block, 3 files
  (mp_merville.cfgstrb/.dbb/.gdbb), 0.8 MB (Merville's archive: 45 blocks, 133 MB).
- Test seelowdb1 (logs-backup/20261104-004529-seelowdb1-result): copied in as
  game\mp\mp_merv\mp_merv_vehicles.cod (the host loads that name in place of Merville's whole archive:
  no code change) with vehicles.txt.seelow-try. Loads, no freeze, ZERO guest DbgBreakPoints; "Merville's
  vehicle files read (8 kinds now)" from it; the Kangaroo made (15 kinds); Seelow's own world drawn
  (ground, trees, houses); War has Seelow's 3 flags. Every saved spot "skipped" (Seelow has no vehicle
  records yet: step 3). Game heap 590.8 of 608 MB (plain Seelow 582.2; Merville whole 602.8).
- Reverted after the test: mp_merv_vehicles.cod removed (the old pack still .off), vehicles.txt as before.
- Next: step 2 (Saint-Lo's sp\saint_lo\vehicles.cod beside the map for v_panzer_iv / v_shermanm4a3,
  ~16 MB: heap headroom is ~17 MB) and step 3 (scene.py: WaW's 12 script_vehicle spots into Seelow's
  scene as script_vehicle records). Step 3 is the big one.

## 11-04 ~00:50: campaign vehicle archives looked into (no build, no test)
The user: "Yes, look into it". Offline only:
- Models in the campaign's sp\<level>\vehicles.cod (each with its own model register .ng.xmbb/.xmpbb and
  .dcgb, NO level: loads beside a map like the Kangaroo's/Typhoon's/Stuka's): Panzer IV in fuelplnt
  (19.5 MB), mace2 (18.9 MB, + Hanomag, GMC truck), saint_lo (16 MB, + Sherman, P-47); Sherman in
  crssrds, laison, saint_lo; Opel Blitz, SAS jeep, Horch(d1) in fuelplnt. NO BMW motorcycle anywhere in the
  campaign (only Merville's archive has v_bmw).
- The MP kinds (mp_panzeriv, mp_shermanTank, mp_motorcycle_bmw, mp_wc51, mp_horch) come from Merville's
  game database (mp_merville.gdbb / .dbb / .cfgstrb; vehicle_pack.sh kept those for the VEHICLEFILE /
  VEHICLEPHYSICSFILE entries). Campaign vehicle archives have no .gdbb.
- Drivable vehicles only come from a map's own scene (script_vehicle records); made-from-nothing ones were
  never drivable (0.5.731). Seelow's scene is OURS (tools/carentan/scene.py level from
  seelow-work\mp_seelow.ents), and WaW's Seelow ents already hold 12 script_vehicle spots (e.g. 3813.8
  4780.4 -39.1: the user's Panzer spot), which scene.py does not carry over today.
- Plan offered: (1) a tiny archive of only Merville's .gdbb/.dbb/.cfgstrb beside Seelow: does it load
  with no level, and are the mp kinds read from it; (2) Saint-Lo's vehicles.cod beside it for the
  Panzer IV and Sherman models; (3) scene.py writes WaW's 12 script_vehicle spots into Seelow's own
  scene as script_vehicle records (layout from Merville's 228-byte records, vehicle_scene.py); (4) host
  glue (vehicle files from the tiny archive; the Kangaroo from a Sherman as now). BMW left out.

## 11-04 ~00:35: 0.5.768, level put back around LoadExtra (vehicles parked)
- The user: "Yes, try it". 768 saves/restores 0x82BCC894, 0x82C9A040 (660), 0x82C8A1C0 (268) around
  LoadExtra(g_merville). Test seelowveh18: before AND after were empty (level 0, flag 0 -> 0): Merville's
  level is NOT made while its archive loads; it is made later, with the map's level load. So this cannot
  work from there without a hook on the level path (and hooks there broke loading: 764, 766).
- The same run froze: Merville's archive "loaded" with no files handled (45 log lines, not ~550), then a
  guest read at 0x30000014 from sub_824783A0 under sub_82474CB8 during the Kangaroo's archive load.
  The whole-archive load is flaky too (seelowveh1-16 loaded; 17 and 18 froze).
- Parked. vehicles.txt as before. Ideas left: find who calls sub_824346C8 for Merville during the map's
  load (a log in an EXISTING hook on that path), or give up Merville's archive and build vehicles from
  the Kangaroo-style per-vehicle campaign archives (sp\*\vehicles.cod hold Shermans/Panzers: Laison's has
  Shermans) which have no level of their own.

## 11-03 23:00 to 11-04 00:05: vehicles on Seelow, the real picture (0.5.762-767; all reverted to 765)
The user: "Keep going". Tests seelowveh13-17, seelowbase1-4 (vehicles.txt.seelow-try only during runs).
- THE TAN IS NOT MERVILLE DRAWN OVER SEELOW. Baseline pictures at the same spots without Merville
  (seelowbase3) show Seelow's houses, stone walls, water tower and textured ground; with Merville's
  archive loaded ALL of Seelow's cells are missing (only trees, reeds, fires, over empty space: the
  "tan"). Also War got Merville's 8 flags instead of Seelow's 3: Merville's scene spawns too.
- Merville's archive carries its own level. Its level handler sub_824346C8 (r4 the level) stores the
  current level at 0x82BCC894, sets the "a level is in" word 0x82C9A044 (block 0x82C9A040, 660 bytes)
  and the level name at 0x82C8A1C0 (268 bytes). Seelow's level, after it, hits the debug message at
  0x82434730 (the DbgBreakPoint seen right after seelow.cod opens in every Merville test).
- 762-764: Merville's world meshes are cell_N_mesh_N (165) and sbm1-20 (763's load log, test
  seelowveh15); unbinding them (sub_82476E88 slot cleared) worked (185) but changed nothing visible.
- HOOK TRAP: adding a hook to sub_82474FD8 / sub_82476E88 (761/762) made Seelow WITHOUT Merville crash
  loading the Typhoon's archive (guest read 0xBAD10001 in sub_82140EA8, seelowbase1-2); 765 with them
  compiled out: fine. 766's hook on sub_824346C8 (restore the level globals after Merville's handler)
  never even fired, yet Seelow's load froze right after seelow.cod opened (seelowveh17). Unexplained:
  maybe COFF weak-alias binding or a call into these from a fiber; check before hooking them again.
- 0.5.767 = 765 (no new hooks; 759's map-list exclusion stays, harmless; 763's load log behind
  COD3_MERV_LOADLOG=1). seelowbase4: Seelow plays.
- Next idea (not tried): instead of hooking, write the level globals back from an existing safe hook
  point AFTER Merville's archive has loaded (MpFixesLoadVehicleArchive, after LoadExtra returns):
  snapshot 0x82BCC894 / 0x82C9A040+660 / 0x82C8A1C0+268 before LoadExtra(g_merville), restore after.
  And Merville's scene (flags, spawns) would still need silencing.

## 11-03 ~22:00: vehicles on Seelow, host side (0.5.759-761 built; vehicles.txt reverted)
The user: "Yes, try it" (leave Merville's archive whole, ignore its world host-side). Tests
seelowveh9-12 (vehicles.txt.seelow-try in place for each run only, put back after):
- Every run loads, no crash: the Panzer, Sherman, Kangaroo and BMW go to the user's spots and draw
  WITH their textures. The ground is still flat tan/brown.
- Name clashes ruled out: Seelow and Merville share 580 names (pg_tpatch_000_NNN, pg_colorlm_000_NNN,
  lm000N, sky, fogramp...), but 761's "scopes:" lines show Seelow's terrain look-ups (scope 14/16)
  search 16 4 14 1 0 9.. (Merville 5 not there since 759) and find the names bound only in Seelow's
  own archives. 760 (Merville last in the no-scope list) did nothing: that list is only the
  archives being loaded ("0 1 7"); taken out in 761.
- So the tan is Merville's own world drawn (as the 21:30 tests said): its load registers 8 cell
  bundles (mp_merville_000_SPLIT0-7, through sub_82473110 -> sub_820B00C0) and 20 sbmodel prop
  batches; the tan is a flat plane with straight edges over Seelow's ground (reeds poke through).
- Draw recorder (seelowveh12, screenshots\CoD3-20261103-215141-draws.bin, reader in the session
  scratchpad draws.py): the 160x110 pictures are too coarse; the first draws go to a 1938x1938
  shadow target. Not settled which system draws the tan.
- Next: find the draw of the tan plane by its vertex buffer (Merville's bank memory) or skip
  Merville's cells/sbmodel batches where they are registered (only for g_vehicleArchive), keeping
  its vehicles. The ~2300 guest DbgBreakPoints come from Laison's mortar effects load (archive 19),
  only with Merville loaded; harmless so far.

## 11-03 ~21:30: vehicles on Seelow, digging (no build; all reverted)
The user: "Yes, dig into it" (why Seelow's ground goes tan with Merville's archive). 7 tests, seelowveh2-8:
- Merville's level archive holds its whole world: terrain (.btrb), cells (.xecell), prop batches
  (level.bgib, sbm meshes), collision (.cgb, .dcgb), scene (props, spawns, vehicles), model register.
  Beside Seelow it is all live: the tan ground and a Merville stone wall are Merville's level over Seelow's.
- Its parts are load-bearing: dropping .cgb/.dcgb froze the load (sub_8241E410 under sub_8242C9D8);
  dropping .btrb froze it; ANY scene edit (vehicle_scene.py, also with no copied records, its own
  vehicles only, at the user's spots) froze the vehicles' spawn (sub_82561448 under sub_8247B780);
  dropping only .xecell + level.bgib loaded, but the tan and the wall stayed.
- Only the untouched archive loads (seelowveh1): then the host moves Merville's own Panzer, Sherman,
  Kangaroo (from a WC51) and BMW to the user's spots; Merville has 1 Panzer IV, 2 Shermans, 4 BMWs,
  2 WC51s, 1 Horch.
- Next idea: leave the archive whole and skip its terrain / collision / props HOST-SIDE (find the
  loaders that register .btrb, .cgb, the scene's instances, and skip them for the vehicle archive), or
  shift them far away. vehicle_scene.py takes <vehicles.txt> <map> now (VEHICLE_NO_COPIES=1: tests).
- Install state: vehicles.txt as before (3 lines), only the old mp_merv_vehicles.cod.off left; test
  packs deleted. Freezes were closed by the test watcher (watchdog line), the ini put back each time.

## 11-03 ~20:30: vehicles on Seelow tried again (no build; reverted)
- The user: "can we get the tanks back on seelow or add a motorcycle and a kangaroo?". Test seelowveh1
  (0.5.758, vehicles.txt with the user's 12 Seelow placements, the cut pack still .off): Merville's WHOLE
  archive (133 MB) and the Kangaroo's loaded beside Seelow, no crash; Seelow's own vehicles: 1 Panzer IV,
  1 Sherman, 1 Kangaroo, 4 BMW, 1 WC51 (Merville has one of each tank); the host moved the Panzer, the
  Sherman, the Kangaroo and a BMW to the user's spots, skipped the 8 other tank spots. BUT Seelow's
  ground drew flat tan (its terrain textures lost). Reverted: vehicles.txt as before (backup
  logs-backup\vehicles.txt.before-seelow2). Next, if wanted: why the textures go (memory, or Merville's
  same-named textures replacing Seelow's), then the cut pack's register loader, then copies for more tanks.

## 11-03 ~20:05: flare sounds, burn, bots with flares / Mosin / Webley (0.5.752-0.5.758)
- WaW's own flare sounds: ignition flare_00..03 + burning trail flare_burn (11 s), xWMA decoded by the
  port's FFmpeg at 48 kbit/s (the header's 12000 B/s misreads every packet; wawimport.xwma_to_pcm picks
  the rate whose length matches the dpds chunk). make_flare.py writes them.
- Burn (the user's idea; WaW has none): a lit flare hurts a living player whose feet are within 28
  across / 40 up-down: 4 every 0.5 s, the thrower's credit (nearest player at the throw), his side
  spared, him not. G_Damage means 14 (means 0 did nothing). Verified 18->14->10->6 (flarebots3).
  COD3_FLARE_BURN=n.
- Bots: their smoke throws had silently failed (they named the grenade one command only; the game took
  the gun back). Now 30 commands. Flare bots also needed: BotNadeWeapon prefers the flare; names at
  0xAA.. allowed (five checks in bots.cpp); mp_smokegrenade_special taken at the swap.
  bot_kits.txt "flare" in any optional column = the kit's smokes are flares (BotsKitFlare). New lines
  (defaults + the install's file, backup logs-backup\bot_kits.before-753.txt): scout mosin_scoped
  flare; rifleman webley flare; light flare. Verified with 28 bots (flarekits1).
- Test switches: COD3_BOT_SMOKE_EVERY=ms, COD3_BOT_FLARES=1|2, COD3_FLARE_THROW_TEST, COD3_FLARE_TEST.

## 11-03 ~19:00: the thrown flare finished (0.5.744-0.5.751)
- COD3_FLARE_THROW_TEST="86 100" (0.5.744) presses the left bumper (the special grenade) 0.3 s at those
  seconds after the view starts: tests CAN throw now (COD3_PAD cannot: it stops the side pick).
- Event 56 comes as the grenade LEAVES THE HAND (r4 = the grenade entity, at the hand): the flare
  follows the entity (FollowFlares, +336) and lights after WaW's 1 s fuse where it is (flare.cpp:
  Ignite(at, fuse) returns an id; MoveTo). Verified: lit ~230 down the shed, blinded 0.43 (= 1-230/400).
- The smoke grenade's own cloud comes ~5 s after the throw from the grenade's own fuse, through no hook
  of ours (not event 2/4/8/10/56; weapon +1936 is not it). Fix (0.5.749, MpFixesFlareRest): when it
  rests, every copy of its place in the entity (+32, +112, +336) is sunk 30000 under the map and a
  script_model of w_signal_flare_WORLD (PlaneSpawn; 6 a map, reused) lies there, turned as it lay.
  Verified: no smoke (flaresmoke4, -5, -7).
- Seen: head, or 20/40 over it within the line's last tenth (0.5.751). The shed test spot is a bad
  corner (a post); open ground fine (flaresmoke7).
- HUD name: Guns' title is the plain text "SIGNAL FLARE" (keys the game lacks print raw; the Webley,
  M2 and Mosin still show WEAPON_... keys - not asked about).
- First person: no grenade is ever drawn on screen in CoD3's throw (flarehand5 at 10 shots a second):
  nothing to do there. Screenshots: several a second are fine (files -2, -3).

## 11-03 ~17:30: the flare stick model (0.5.742-0.5.743)
- w_signal_flare_WORLD (Guns' signal_flare worldModel: in the thrower's hand, thrown, lying) = a copy of
  the American smoke's world model with WaW's weapon_mp_flare_stick (custom\flare\weapon_flare.mesh), as
  WaW has it (both z up from the origin; not turned or scaled). The M2's world-model code is now shared:
  SwapWorldModel + a WorldSwap (FlameWorldModel and FlareWorldModel use it; the M2's still "made").
- Texture: the can's draws have slots 2 (256), 3 (64), 1 (128): the colour map in all three (default
  slot 2 + custom\flare\world_slots.txt "1=flare_world_c.png 3=flare_world_c.png"). Proof: test
  flaremodel5 (logs-backup\20261103-171952-flaremodel5-result, screen 172201): red paper, label, wooden
  grip beside the white smoke can. COD3_FLARE_MODEL_TEST="x y z" stands both models (test only).
- First person keeps the game's grenade: the smoke's view model is BOX_SOMTHING, shared with the frag
  (its animated nade/pin/spoon parts). Swapping it needs a held-only body swap like MpFixesFlameView.
- Tests' camera drifts between shots (the test harness turns the view): find the subject in a contact
  sheet, then crop.

## 11-03 afternoon: World at War's SIGNAL FLARE (0.5.739-0.5.741), and two Seelow fixes
The user: "i want the signal flare in cod 3 from world at war so we have more then just the smoke
grenade". Done as a custom class's SPECIAL row (class editor; ini class_N_special 1).
- Weapon: Guns "signal_flare" (mp_fixes.cpp), the American smoke's copy (weapon 48 on Seelow, shares its
  rounds: clip pool #20). Spawn swap: SmokeToFlare (after the kit, from MpFixesBinoculars): flare given,
  the smoke's rounds put on it, the smoke's bit cleared in the 64-bit mask (state +1056), the weapon
  slot that held the smoke (slot 4, +1068) given the flare. State +240 is FLAGS (0x10/0x18), not the
  offhand: 0.5.740 wrongly wrote 48 there, 741 took it out.
- Lighting: sub_82556180 event 56 with r5 text "signal_flare" -> Flare::Ignite(r4 +336), no smoke cloud.
  r5 lies in the physical memory's view (0xAA6110E4): 739/740 checked < 0xA0000000 and never lit; 741
  fixed. Proof the hook gets it: flare2's log has 4 "event 56 for signal_flare" (the logger's ReadText
  of the same r5).
- flare.cpp (new): WaW _flare.gsc numbers (8 s blind, 400 radius, 1 - d/400, 0.3 s sight checks, 0.45
  look-away / 0.65 burn-out waits) + looked-at within 55 degrees; glow 10 s (fxt_light_flare3_128 star,
  fxt_light_flare core), sparks; the wash = orange-red tint (WaW vision/flare.vision film tint, bloom).
  Overlay::SetFlareHud (7th HUD slot). Sight traces: FlareSight (mp_fixes, BotsFlameTrace).
  bots.cpp feeds Flare::Camera.
- Tested (flare1, logs-backup\20261103-160042-flare1-result): COD3_FLARE_TEST="3150 1929 13" (inside the
  concrete train shed on Seelow; stand at 3000 1929 12): burning glow + sparks, blind 0.59 at 150, sight
  back on turning away. Screens 160306/160318/160326 (wash), 160258/160322 (glow).
- NOT verified in game: a THROWN flare lighting (the test harness cannot throw: console +smokegrenade
  does nothing, like +forward). flare2 showed our player's throw naming weapon 48 and the game
  throwing it (4 event 56), lit fixed in 741 after that. ASK THE USER to throw one.
- Bots cannot throw the copy: with COD3_BOT_FLARES=1 their throws named 48 and spent nothing (14 tries,
  flare3/flare4). Normal bots keep the smoke (flares only via that test switch). BotNadeWeapon now names
  only an owned grenade (0.5.740; frags still thrown).
- Sounds: WaW's own flare ignition/burn are xWMA (RIFF XWMA, WMA v2): the port's FFmpeg avcodec-62
  rejected every packet (AVERROR_INVALIDDATA, extradata flags 31/15/7 tried): uses the M2's
  flm_ignite + flm_loop_far. The throw (flashbang_toss_b: not wired yet) and the blinded ring
  (flareshock_eq + fade) are WaW's.
- tools/waw/make_flare.py -> custom\flare (glow/core png, hud_icon_flare, flare_world_c.png,
  weapon_flare.mesh + viewmodel_flare.mesh from WaW's flare stick, the sounds). run_test.py --special N.
- Open: WaW's flare stick model in hand and on the ground (meshes made; do as FlameWorldModel /
  MpFixesFlameView), the throw sound, its kill icon (hud_icon_flare via SetOwnIconSource), a friend's
  machine (event 56 runs on the host; untested), bots throwing it.
Seelow, same afternoon: fires centred on their cars and set 15 under each car's origin (the vfire
flames start ~40 over the point; Rouen's own sits under the ground), test fires3; the drain pipe got a
solid roof slab (seelow_clipmap PIPE_ROOF) so standing inside is refused (old collision coll_m.kerbs1).
Seelow and Seelow Night are map packs built from the user's own World at War files (tools/waw, data in
`D:\Call of Duty 3 (USA, Europe)\seelow-work`). The user says the collision and the night lighting are
"great"; they spent the end of the session placing their own lanterns live.

Older detail, by step (tests, numbers, dead ends): docs/CLAUDE-SESSION-2026-11-02-evening.md (its sections
from "~20:50 update" down), -afternoon.md, -midday.md.

## FINAL BUILD STATUS
Everything below is INSTALLED and tested (see "Last batch" at the bottom). Nothing pending.

## What the user has now (both packs unless said)
Collision (tools/waw/seelow_clipmap.py -> seelow-work\coll_m, 4 files):
- WaW's small player clip (< 120 tall, 1320 boxes) out at walking height (drop_small_clip), EXCEPT where it
  is a prop's only collision (rocks 131 boxes, tractors, wagon wheels, big rubble) and up high (47).
- Model hull triangles: WaW XModel collision triangle corners are where s.p - sw and t.p - tw are 0/1. The
  old + reading put stray walls up to 363 units out round every meshed prop (hedgehogs, trees, box cars,
  the water tower). Fixed; every triangle now inside its model's box.
- FOUR .cgb files: a .cgb holds 65535 mesh index bytes (3 a triangle, the binding limit) and points. The
  4th goes into the level archive as unzoned.cgb (repack.py --replace; Merville keeps ALL its collision in
  its level archive, proven for Seelow by drop test file4). Bands filled by real index/point use.
  SEELOW_MESH_BUDGET 60000: all 500 prop hulls meshed, 0 boxes.
- Ladders: WaW's 5 ladder brushes keep surface 0x40b8 + a type (as Rouen's). USER CONFIRMED the water
  tower ladder climbs.
- Drain pipe under the north road: its end rings out (PIPE_CAPS). (11-03 ~12:05) PIPE_ROOF: a solid slab
  (x 855-1142, y 6146-6210, z -56..-48, as WaW's own two over the pipe's ends) so a player cannot stand
  up in the middle: the user stood at 945 6166 and was lifted till their head met the road, view
  through the ground. The pipe's mesh shell is solid but CoD3's stand check does not stop on a thin mesh
  from inside; it does on a brush. Old collision coll_m.kerbs1; both packs reinstalled (not tested
  in game: needs a crawl). Wall hole at 2620 3794 widened
  (WALL_HOLES, 41 jagged pieces). Tunnel-entrance parapet edge out (KERBS, 11 triangles; the user's
  "stuck on two textures" at 3349 2825).
Night lighting (tools/waw/seelow_cells.py -> cells3n; day cells3m):
- Moonlight SEELOW_NIGHT=0.59,0.69,0.97 (the user asked +5%; was 0.56,0.66,0.92).
- Every WaW lamp lit (55, the 10 switched-off ones too), 8 fireplaces, 4 stoves, WaW's 5 fires (parsed from
  its createfx script in the zone), the user's lanterns. Lamp parts glow (own flat lightmap).
- Light stays in its room: lamp_occluders/lamp_sees (facing + line of sight past 3905 WaW solid brushes).
  LAMP_NEAR 180, LAMP_FAR 450, LAMP_WARM (215,172,112). The user liked it; do not brighten unasked.
- The user's lanterns: seelow-work\my_lights.txt (21 lanterns + 1 sandbag). Format "x y z <model>
  [lean_deg dx dy]"; night only (place_my_lamps -> seelow_models.EXTRA_PLACEMENTS). Day Seelow has NONE
  (the user: "only install lamps on Seelow Night").
Fire (tools/carentan/scene.py fires(), seelow_install.sh SCENE_FIRES):
- WaW's 5 fires are all burning junked cars. Each gets CoD3's own vehicle fire (vfire_panzerIV, Rouen's,
  kept in the level archive) via Rouen's zone 1 scene: 5 effect records from its first fire, renamed,
  upright, moved. Effect record = 96 bytes: 3x4 matrix rows, origin +48, name pointer +64.
- (11-03 ~11:20, the user asked them centred) Each fire at its junked car's model-box middle (xy) and the
  car's origin z - 15. The effect's flames start ~40 above its point: at WaW's spots (~30 over the car)
  they floated over the roofs and read as "behind" the cars (test fires2). Rouen sets its own
  vfire_panzerIV ~20-40 under the ground at its Panzer (-29 -1250 -22). Test fires3
  (logs-backup\20261103-111450-fires3-result): flames rise out of all 5 car bodies, interiors glow.
Host (C++):
- 0.5.738: F11 "Mantle assist" setting (ini mantle_assist), OFF by default for players (the user would
  rather jump and crouch); bots always (bots.cpp BotsIsBotRecord). Test mantle738: 8 bot lifts, no crash.

## How we worked live (reuse this, the user liked it)
- The user plays and says "here"/"lamp here"/"hole"/"stuck". Read the LIVE log's last "where:" line
  (`grep "^where:" CoD3MP.log | tail`): position only, no view angle. Heading = their last few steps.
- Find the prop: seelow_models.draw_placements (origin, axis rows forward/left/up, scale) + the model's own
  box (wawimport.read_xmodel, +0xAC/+0xB8); work in the prop's own frame (left/right/front/back, its top).
  Clip-list placements (smd.placements) give world boxes but are coarse: the work bench's put a lantern 11
  off its edge.
- Ground/wall heights: seelow_clipmap.read(z) terrain triangles + solid brushes (contents & 1).
- Stuck spots: decode the INSTALLED collision at the spot (cgb.Cgb volumes; mesh triangles via
  c.tris / c.point_sets / cgb.point), not just WaW's data.
- Never install while the game runs (check `Get-Process CoD3*`). Data builds (cells, coll) are fine meanwhile.

## Commands
- Collision: `python -I tools/waw/seelow_clipmap.py <seelow-work>/mp_seelow.zone <seelow-work>/coll_m.3files/seelow_zn01.cgb <out prefix>`
  (4 files by default) then copy into seelow-work\coll_m.
- Night cells: `SEELOW_NIGHT=0.59,0.69,0.97 python -I tools/waw/seelow_cells.py <zone> <seelow-work>/materials.tsv "D:/Steam/steamapps/common/Call of Duty World at War/main" mp_rouen <seelow-work>/cells3n`
  (day: same without SEELOW_NIGHT into cells3m; only needed if seelow_cells/seelow_models change for day).
- Install: `CELLS=cells3m bash tools/waw/seelow_install.sh` then `NIGHT=1 CELLS=cells3n bash tools/waw/seelow_install.sh`.
- Tests: tools/cod2import/run_test.py (backs up logs; warn the user first). COD3_CMD timed from game start
  (~100 s is safe), COD3_SHOTS from the match's first frame. "+forward" via COD3_CMD does not move the player.

## Open, in the user's order
1. Seelow Night: arms and gun lit as by day (light grid samples; night_lightgrid.py).
2. (DONE as a setting, 0.5.738) low-window crouch: the user prefers manual; bots keep the assist.
3. Slot 82's real model (3 small objects); the stable doorway at 996 861.
4. Vehicles: drivable ones parked (midday file). Static vehicle props could be placed like the lanterns, but
   would need collision (extend EXTRA_PLACEMENTS to model_collision and to day Seelow).
5. "Something missing" at 810 3109 (WaW traversal over a ~95 ledge at y 3040-3075): ask for a screenshot.
6. Friend zip: on hold until the user asks.

## Last batch (2026-11-03 ~00:55): INSTALLED AND TESTED
Collision with KERBS (coll_m; old coll_m.hole1), fires (SCENE_FIRES default in seelow_install.sh),
cells3n with 21 lanterns + sandbag (76 lamps; old cells3n.prev). The user's last play backed up first as
logs-backup\20261103-user-play10 (0.5.738, no "GPU is hung" any more). Test fires1
(logs-backup\20261103-003617-fires1-result): CoD3's vehicle fire burns at the cars 3243 1179, 4078 4719
and 367 2067 (flames, smoke, sparks, lighting the ground), the user's lanterns beside them glow. The car
at 3243 1179: its flames stand at WaW's fx spot (3206 1163, ~40 off the car) and read a little high and
behind it from the south-east; nudging that one spot onto the car is a possible polish, not asked for.
DONE 11-03 ~11:20 (user asked): all 5 fires centred on their cars and lowered (see Fire above); both
packs reinstalled, tested fires3.
