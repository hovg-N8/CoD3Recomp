# Claude session 2026-11-07 (overnight, user asleep)

Source and install: 0.5.832 built OK at start of the night. Nothing committed. Friend zip on hold.
User's words before sleeping: keep working down the list, keep notes so the next session picks up.

## Done before the night

- Dual Colt muzzle flash (0.5.828-0.5.832): the Colt's own view flash (particle program
  1c2487958f971104, smoke tex 28090000 blend 07010706 and core tex 282A0000 blend 01000106, both ~10
  from the eye, clip rows c0/c3) is moved out with the right gun and drawn again mirrored at the left one
  for 150 ms after each dual shot (render_commands.cpp PrepareMirror, flash path). USER CONFIRMED in play:
  "lined up". The bright glow in test screenshots near x 0.12 is the bullet's impact spark (3200 away),
  not the flash: leave it alone. Test screenshots cannot catch the flash (too brief).
- 0.5.830's screen-place "core" test is off (`core = false && ...`); remove it with the other diagnostics.

## List for the night (in order)

1. tools/waw/seelow_install.sh: bring in the new game data (sp\vehfx pack, combined db pack).
2. Tank shell damage on Seelow.
3. Tank hit surface sound layers.
4. UAV Stuka check.
5. Dual Colts in third person (others see one Colt).
6. Remove diagnostics.
7. Seelow Night fog / arm lighting.

## Progress log

### ~01:20 install script (no build needed)
- tools/kapf/kapf_lists.py (`fli` / `main` modes) replaces the scratchpad nofli.py/mainonly.py; checked
  byte for byte: it remakes the installed mp\mp_merv\mp_merv_vehiclefx.cod and sp\vehfx copy from
  logs-backup\packs\mp_merv_vehiclefx.withfli.cod. seelow_install.sh now runs it (FLI emptied after the
  repack; sp\vehfx copy made if missing). The installed mp_merv_vehicles.cod (db + tank effects + textures,
  4.8 MB) is NOT what the script makes (db only, 0.8 MB, original in logs-backup\packs\
  mp_merv_vehicles.dbonly.cod): the extra effects never register (0.5.820), so either works.

### ~01:40 tank shell damage (0.5.833 diagnostic, 0.5.834 fix)
- COD3_DAMAGE_LOG=1 logs every G_Damage (sub_825862F8): target, attacker, inflictor + weapon byte, means,
  damage, health before/after.
- tankdmg1 (Seelow, `COD3_CMD=25:tank_fire 120`, 8 bots): shells DO hurt: means 10, inflictor weapon byte
  54, 96-106 damage, killed clients 7 and 9. But the attacker was the TANK (82A6E810, health 2400, no
  client): no kill credit, friendly fire unchecked, one shell took 81 off its own tank.
- 0.5.834: a means-10 hit whose attacker is no player goes to the player in that vehicle's seat 0 (entity
  +900 -> link -> vehicle; client +1300 seat). tankdmg3: "its driver (client 4) is the attacker", hit
  logged as by client 4. Merville comparison (tankdmg2) failed: the bot spawned 2500 away and never got in.
- Not checked: whether Merville's own game does the same (a human driver there): if the kill feed there
  already credits drivers, our fix only matters on Seelow; harmless either way.

### ~01:45 tank hit surface sounds: NOT A BUG
- SHELL_EXPLO_ROLLOFF_TANK / SHELL_EXPLO_<surface>_TANK are in NO .wbk at all (every mp\ and sp\ bank
  searched as text); every MP bank has only SHELL_EXPLO_MAINIMP_TANK, which plays on Seelow. Merville
  plays the same. Nothing to fix.

### ~01:50 UAV Stuka: draws fine
- stuka1: enemy UAV (COD3_UAVTEST_AT=40) made stuka_flying for side 1 from the cut pack
  (sp\crssrds\vehicles_stuka.cod, loaded as archive 9), its sounds on it.
- stuka3: plane_model stuka_flying (placed at a fixed 403 2324 310, NOT in front of you) seen with
  COD3_BENCH_VIEWS=403,1800,360,90,3: textured (grey-green, yellow tail band, A+HW, cross).

### ~02:00 dual Colts in third person (0.5.835-836): BUILT, NOT SEEN ON SCREEN
- COLT .45 DUAL's world model is now its own (Guns entry's world name w_us_pist_dual_WORLD, made in the
  sub_82446EA0 hook by DualColtWorldModel, mp_fixes.cpp, beside FlareWorldModel): the Colt's world model
  copied, its body mesh's own vertices drawn twice (360 vertices of 24 bytes, 684 indices), the second
  copy 5 units along z (the Colt's world body lies in x = barrel -4.7..6.1, y = grip..slide -3.0..2.9
  with the slide at +y, z = thickness 0..1), so across the hand; -z = the holder's left (up x forward),
  and 1 back along x. COD3_DUAL_WORLD_Z/_Y/_X tune it, COD3_DUAL_NO_WORLD=1 = one Colt.
- Log OK (dualworld2: "dual world: made 6E150000 ... the second -5.0 along z"). NOT verified by eye:
  cg_thirdperson does nothing in our MP; plane_model puts the model at a FIXED 403 2324 310 (not in
  front of you), and COD3_BENCH_VIEWS never turned to it (yaw stuck at 120/150 whatever is asked,
  pitch ignored). The hold is CoD3's two-handed pistol pose, so the left Colt sits beside the right one
  in the left hand's place; if it looks wrong in play (floating, wrong side), flip COD3_DUAL_WORLD_Z.
  Who sees it: the friend (online), killcams.

### ~02:10 Seelow Night arms (0.5.837): DONE, seen
- The arms/gun pixel program 9a14f83313a256a4 reads 9 SH terms c18-c26 from Rouen's light grid (day).
  render_commands.cpp: on seelownight those constants are scaled as they are packed for the draw
  (0.20 0.22 0.32; Render::SetNightLight, set in mp_fixes.cpp where the weapons' archive loads, by
  Kernel::CurrentMap()). COD3_NIGHT_ARMS="r,g,b" tunes, "1,1,1" off. nightarms1 (on) vs nightarms0
  (off): arms dim and blue vs lit like day. Day Seelow and other maps untouched.
- Not decoded: the light grid's samples (the "real" fix would rewrite them in the night zones).

### ~02:16 cleanup (0.5.838)
- 0.5.830's dead "core" screen-place test and its logs removed; moved-flash log cut to 4 lines.
  dualflash8: flash still moved, 22 test shots, no crash.
- Still in, all env-gated: COD3_FLASH_DRAWS (blend:/draws: lines), COD3_DAMAGE_LOG, COD3_EVENT_SHOTS,
  COD3_ALL_EFFECTS. Ungated but capped: effects lookup and asset-slot lines from 0.5.815-820.

## Where to pick up
1. User plays: Seelow Night (are the arms right? too dark near lamps?), tank kills credited to the
   driver on Seelow, dual Colts (unchanged first person).
2. Third-person dual Colts: needs a look (friend online, or a killcam by a bot holding them: bots never
   pick colt_dual today).
3. Seelow Night fog: open per the 11-02 notes only if the user still sees a problem.

## USER'S PLAY RESULTS (2026-11-07, on 0.5.838, solo)
- Tanks on Seelow: working; the tanks' weapons "work great". CROSSED OFF.
- Seelow Night arms (0.5.837 scale): "perfect". CROSSED OFF. Don't decode the light grid for this.
- Seelow Night fog: the user is happy with it: LEAVE AS IT IS. CROSSED OFF.
- Dual Colts third person: not seen (solo; needs the friend online or a killcam). Still open.
- Still open after this: bots can't enter the Shermans on Seelow; Kangaroo treads on Merville (0.5.815)
  unchecked; ungated tank-explosion diagnostics; nothing committed; friend zip on hold.

## ~12:40-14:25 Bots and the Shermans on Seelow (0.5.839-0.5.848), user away
- The user DID get into a Sherman by hand today (their play log, 0.5.838: at 1605 163 by the Sherman at
  1524 238, side 2, then event 41 for mp_shermanTank): so only the bots' way in was broken.
- Not distance from the host, not team (side 1 and side 2 bots both failed; the user is side 2).
- Cause 1 (0.5.843): Think's yaw steering (bots.cpp ~5588, wanted yaw less learnt offset, 8 deg/frame)
  ran every frame and pulled the bot back as fast as LookAtVehicle/vehicle code turned him: at the press
  his view was 50-160 deg off the tank (sherman6 log). g_lookVehicleUntil[slot] (real-time ms) now
  pauses it while LookAtVehicle has his facing (0.5.843) and while he walks to his vehicle (0.5.848).
- Cause 2 (0.5.844-846): a tank only let the bot in from a CORNER: four sides at 110 never worked even
  facing it exactly (sherman7); rear-left corner at 106 did (sherman8). tank_fire's driver and gunner now
  try 8 places (sides + corners) at 95 then 130 (32 presses); the bots' own vehicle code walks to the
  nearer rear corner (100 from the middle, yaw +-135) and presses there (was: walk at the middle until 75,
  which the hull never allowed).
- Results: sherman9 (tank_fire 120 2, COD3_TANK_FIRE_KIND=sherman): full Sherman crew, 37 cannon shots,
  25 MG bursts. botveh5 (4 bots, 5 min, no tank_fire): two bots got into Panzers by themselves, first
  press each, and drove ("is out after N s"). No bot reached a Sherman on his own in that run: the one who
  tried was stuck around 3800 1250 (12 "is stuck: backs up" lines overall): the Shermans sit low by the
  river, a walk across the map from where they spawned. That is navigation, not getting in.
- Diagnostics added (cheap, keep for now): tank_fire "pressing use: ..." per try; "on his way to ..."
  every 4 s per bot; COD3_VEHICLE_PROBE=1 also logs sub_8255EFE0 outside the seat request;
  COD3_TANK_FIRE_KIND=sherman|panzer test switch.

## ~19:40-20:15 Bots' pathing to tanks (0.5.849-0.5.851)
- 0.5.849: a bot more than 300 from his tank's rear corner gets that corner as Think's goal
  (g_vehicleGoal/g_vehicleGoalAt, read in Think next to the objective goal; no ring, no long way), so the
  learnt map's A* route takes him there; the vehicle code leaves his orders alone meanwhile.
- 0.5.850: within 300, the corner changes every 8 s he is not in (nearest first, all four).
- 0.5.851: switching to a corner on the far side, he walks round the tank on a ring 170 out (45-degree
  steps) instead of into the hull.
- Normal matches, 4 bots, 6 min, Seelow (tanks entered by bots on their own):
  botveh2-4 (before): 0 | botveh5 (0.5.848): 2 Panzer | botveh6 (0.5.849): 3 Panzer + 1 Sherman |
  botveh7 (0.5.850): 1 Sherman | botveh8 (0.5.851): 3 Panzer + 1 Sherman, no crash.
- Still: ~9 "took too long" give-ups a match and ~24 "is stuck: backs up" (driving). Possible next steps:
  longer give-up time for a far tank; driving pathing. botveh6 had the old boot-time "GPU is hung"
  guest text (seen in many earlier logs, harmless).
