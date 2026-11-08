# CoD3 Recomp: patch notes since 0.4.0

**Current source version: 0.5.851.** See the newest entry below.

Historical versioning note: 0.5.0 was the build of October 11, 2026, evening.
From now on, every rebuild numbers itself one higher: 0.5.1, 0.5.2 and so on.
The number shows in the window's title, at the top of CoD3MP.log, in the F11
menu's title bar, and in VERSION.txt in the game folder.

The numbers between 0.4.0 and 0.5.0 below are given after the fact, grouping
the work by what it added. Only 0.4.0 and 0.5.0 were real builds with those
names.

---

## 0.5.851: round the tank to its other corners

- botveh7 (0.5.850): one bot drove a Sherman by himself; the others hovered 170-375 off, walking straight
  at a corner on the tank's far side into its hull. When the corner is more than 50 degrees round from him
  (seen from the tank's middle) he walks the next 45 degrees of a ring 170 out first.

---

## 0.5.850: near his tank, a bot tries its other corners

- botveh6 (0.5.849, 4 bots, 6 min): a bot walked to a Sherman by himself and drove it, and bots got into
  Panzers three times. 7 of the 8 who gave up (took too long) were 130-340 from the corner they had: walled
  or blocked. Within 300 of it, the corner he heads for now changes every 8 s he is not in (nearest
  first, all four: rear and front). Log: "tries another corner".
- (The "GPU is hung" guest text at its start is the old boot-time message, seen in many earlier logs;
  the match ran its full time.)

---

## 0.5.849: bots take the learnt map's route to their vehicle

- botveh5 (0.5.848): bots got into Panzers by themselves, but the one going for a Sherman (by the river)
  got stuck round 3800 1250: the vehicle code walked him straight at it. Now, more than 300 from the
  tank's rear corner, that corner is his goal in Think and the learnt map's route (A*, as to an
  objective; no ring round it, no long way) takes him there; the last 300 as before. Log: "routes to".

---

## 0.5.848: bots walk to the vehicle they chose

- botveh4 (0.5.847): every bot going for a tank walked AWAY from it (~1000 to ~2000 in 40 s): Think's
  steering turned him to its own goal as fast as the vehicle code turned him to the tank. It is left out
  while he walks to his vehicle too (as while he lines up on it, 0.5.843).

---

## 0.5.847: DIAGNOSTIC: a bot's way to his vehicle logged every 4 s

- botveh2 (0.5.846, 4 bots, 5 min): still no presses; 6 gave up for taking too long. "on his way to":
  his distance from the tank's middle and its corner, and where he is.

---

## 0.5.846: bots go to a tank's rear corner to get in

- sherman9 (0.5.845): a full Sherman crew from tank_fire: driver and gunner in, 37 cannon shots, 25 machine
  gun bursts.
- botveh1 (12 bots, 4 min, no tank_fire): the bots went for Shermans 36 times and Panzers 32, gave up 57
  times for a fight, never pressed use: they walked at the tank's middle until 75 from it, and the hull
  stops them further out. Now they walk to its nearer rear corner (100 from the middle, 135 degrees round
  from its front) and press use there, facing it.

---

## 0.5.845: the gunner tries the eight places too

- sherman8 (0.5.844): A BOT GOT INTO A SHERMAN on Seelow (slot 4, side 1, from its rear-left corner at
  106, facing it) and fired 30 shots. The gunner (four sides at 110) never got in: he gets the driver's
  sixteen places too.

---

## 0.5.844: tank_fire's driver tries eight places round the tank

- sherman7 (0.5.843): the bot faced the Sherman exactly at each press (view yaw = its bearing) and still
  was never let in, from its four sides at 110. The user (side 2, like the bot) got into one from its rear
  corner. Now the sides and corners at 95, then at 130: 16 places, two presses each.

---

## 0.5.843: the bots' steering lets go while they line up on a vehicle

- sherman6: pressing use, the bot's view was 50-160 degrees off the tank and hardly turned: Think's
  steering (the wanted yaw less the learnt offset, 8 degrees a frame) pulled him back each frame as much as
  LookAtVehicle turned him. While LookAtVehicle has his facing (250 ms after its last call) that steering
  is left out. Helps tank_fire and the bots' own vehicle seats alike.

---

## 0.5.842: DIAGNOSTIC: tank_fire logs where the bot is and looks while pressing use

- sherman5 (0.5.841): still no Sherman. Each try now logs his place, distance and height from the tank's
  middle, his view yaw against the tank's bearing, his pitch and the buttons sent.

---

## 0.5.841: bots face the tank they get into

- sherman4/panzer1: the game's vehicle-vs-player check (sub_8255EFE0) was never reached for a Sherman, and a
  Panzer only sometimes: the use was not aimed at the tank. The user's play today (0.5.838) shows a person
  gets into a Sherman on Seelow (by 1524 238; event 41 for mp_shermanTank), so the bots' way in was wrong:
  tank_fire put the bot beside the tank and turned him 30 degrees toward it once a try (pitch only after).
  LookAtVehicle (tank_fire's driver and gunner, and the bots' own vehicle seats) now turns him to face the
  tank every frame (a third of what is left, at most 8 degrees).

---

## 0.5.840: DIAGNOSTIC: the vehicle-vs-player check logged everywhere (COD3_VEHICLE_PROBE=1)

- sherman1-3 (Seelow, COD3_TANK_FIRE_KIND=sherman): an Allied bot (side 1) fails all five Shermans, the
  nearest 234 from the host (so not the distance from the host), and the game never got a seat request
  (no vehuse lines): the use is refused before it. sub_8255EFE0 (vehicle vs player) now logs outside the
  request too.

---

## 0.5.839: test switch: tank_fire takes a Sherman first (COD3_TANK_FIRE_KIND)

- For the open "no bot gets into a Sherman on Seelow": COD3_TANK_FIRE_KIND=sherman (or panzer) makes
  tank_fire pick that kind first; its "takes" line names the bot's side.

---

## 0.5.838: the dual flash's dead test taken out

- 0.5.830-832's screen-place "core" test (off since 0.5.832) and its log lines removed; the moved-flash
  log 4 lines a run. No change in play. nightarms1/0 (0.5.837 on/off, Seelow Night): the arms dim and
  blue with it, lit as by day without.

---

## 0.5.837: Seelow Night's arms lit by moonlight

- Seelow Night's arms and gun were lit as by day: the arms' pixel program (9a14f83313a256a4) takes 9 rgb
  terms c18-c26 from the zone's light grid, which is Rouen's afternoon (~0.20 at night; Longyard ~0.03
  bluish; scaling the grid's lights, ambient or lamps never moved them: 2026-11-02). On seelownight those
  terms are now scaled as they are sent (0.20 0.22 0.32: dim and blue). COD3_NIGHT_ARMS="r,g,b" another
  scale, "1,1,1" off. Day Seelow and every other map untouched. Log: "night light: map ...".

---

## 0.5.836: the second world Colt beside the first, not on top

- dualworld1: the Colt's world body lies in x (barrel) and y (grip to slide), z its thickness (box
  -4.7..6.1, -3.0..2.9, 0..1): 0.5.835's 5 along y stacked the guns. Now 5 along z, to the holder's left
  (up x forward: -z when the slide is at +y, found from the front vertices' mean y; logged).
  COD3_DUAL_WORLD_Z / _Y / _X.

---

## 0.5.835: others see two Colts (the dual Colts' world model)

- COLT .45 DUAL's world model is its own, w_us_pist_dual_WORLD: the Colt's looked up and copied, its body
  drawn twice from the Colt's own vertices, the second set 5 along y (the holder's left) and 1 back along
  the barrel (COD3_DUAL_WORLD_Y / _X): a Colt in each hand of the pistol's two-handed hold, in the Colt's
  own material. COD3_DUAL_NO_WORLD=1: the one Colt. Log: "dual world: made ...".

---

## 0.5.834: a tank shell's kill is its driver's

- tankdmg1 (Seelow, COD3_DAMAGE_LOG=1): the shells DO hurt (means 10, about 100 damage: clients 7 and 9
  killed), but the attacker given was the tank itself (82A6E810, no client): nobody got the kill, friendly
  fire was not checked, and one shell took 81 off its own tank. Now a means-10 hit whose attacker is no
  player is given to the player in that vehicle's seat 0 (the driver, who fires the cannon). Log:
  "damage: a tank shell's hit: its driver (client N) is the attacker".

---

## 0.5.833: DIAGNOSTIC: every hit logged (COD3_DAMAGE_LOG=1)

- For the tank shell's damage on Seelow (unchecked): with COD3_DAMAGE_LOG=1 each G_Damage call logs the
  target, attacker, inflictor (and its weapon byte), means, damage and the target's health before and after.
- tools/kapf/kapf_lists.py (empties a pack's FLI list, or every list but MAIN's); seelow_install.sh now makes
  the vehicles' effects packs as installed (mp_merv's FLI emptied, sp\vehfx copy MAIN only): both checked
  byte for byte against the installed files.

---

## 0.5.832: 0.5.830's screen-place test off (it moved impact sparks)

- flashdraws7: the bright glow seen in the shots was the bullet's impact spark on the far wall (3200 from
  the eye), not the muzzle flash. The Colt's own view flash is the draws 10 from the eye (smoke 28090000,
  core 282A0000 additive), which 0.5.828 already moves right and mirrors left. 0.5.830's test is off; the
  moved-flash log names each draw's texture and blend.

---

## 0.5.831: DIAGNOSTIC: where each flash particle draw lands on the screen

- dualflash6: draws taken as the core were moved, yet the bright core in the shots stayed at x 0.13,
  y -0.21 (the unmoved gun's muzzle). COD3_FLASH_DRAWS's blend lines now give each particle draw's whole
  screen box and depth range, to find the core's draw.

---

## 0.5.830: the dual Colts' flash core found by its place on the screen

- dualflash5: the smoke moved but the bright core (additive, blend 01000106) did not: its first vertex
  reads 2300-5400 from the eye, yet it lands where the unmoved gun's muzzle is (x 0.04-0.10). In the shot
  window an additive particle draw landing there (x -0.02..0.25, y -0.6..0.2) is moved and mirrored too.

---

## 0.5.829: the dual Colts' flash core moved too

- dualflash4: 0.5.828 moved the flash's smoke (tex 28090000) but its bright additive core (tex 271E2000)
  stayed where the unmoved gun's muzzle is, between the guns: its draw had no constant equal to the view's
  w row, so it was skipped. For the particle program its own c0/c3 are used then, and the view's rows may
  be up to 2 frames old.

---

## 0.5.828: the dual Colts' flash test reads the flash's own vertex

- 0.5.825-827 never moved the flash: the near-eye test read the first vertex of the whole shared particle
  buffer (a fire 4359 units off), not the draw's own lowest vertex (0.5.825's change landed on the
  diagnostic instead). dualflash3 shows the Colt flash draws at 10 from the eye, side 0.15-0.31, in the shot
  window, so they now pass.

---

## 0.5.824: the dual Colts' muzzle flashes on both barrels

- The user: the left Colt should flash like the right, and both flashes must line up with the barrels now
  the guns are set apart. The right flash is the Colt's own view flash, drawn where the unmoved gun's muzzle
  is (so it sat inward of the moved right gun); 0.5.811's left one was a flash made on a script_origin at a
  guessed place.
- Found (flashdraws1-4): the flash is particle draws (program 1c2487958f971104) whose own clip rows are
  c0 (x) .. c3 (w; the same as the view model's c15). Now, for 150 ms after each dual Colt shot, a blended
  draw whose first vertex is within 150 of the eye on the right is moved as the right gun is (x row + spread
  x w row) and drawn again mirrored as the left gun is (-x row - spread x w row): the game's own flash at
  both barrels. The rows are found by value (the one equal to the view model's w row this frame, and three
  before it). The script_origin flash only with COD3_DUAL_SCRIPT_FLASH=1.
- Diagnostics: COD3_FLASH_DRAWS=1 logs the draws after the first three shots.

---

## 0.5.820: the tank shell's hit explodes (the level's artillery explosion), its blast the bazooka's

- Why Merville's own tank effects cannot show on another map, found over tankboom5-16 and fxlist1: an
  effect is looked up through the asset manager's slot of an archive and made (a runtime object, not the
  file) only from the LEVEL's own archive: no archive loaded beside a map ever registers one (the effects
  pack and database pack get no slot at all; Laison's effects pack has a slot but 0 effects), whether from
  mp\mp_merv or spehfx, with or without FLI/AUDIO lists, the effects grafted into Seelow's level archive
  or into the database pack. On Merville they work because they are in Merville's level.
- So a tank effect not found is made with what this level has: the shell's hit (tank_<surface>) with the
  artillery's explosion (artillery_<surface>, artillery_dirt, artillery_generic: the explosion the
  mortar's hits make on Seelow), the gun's blast (mf_panzerIV_dirt_w, mf_sherman_generic_w) with the
  bazooka's (mf_bazooka_w, mf_panzerschreck_w, mf_bazooka_v). Logged "effects: <tank effect> made with
  this level's <effect>". The tank's own events, sounds and hit places are unchanged.
- Test diagnostics left in: COD3_ALL_EFFECTS=1 logs every effect looked up.
- Game data: the effects pack also at spehfx\mp_merv_vehiclefx.cod (0.5.816); mp_merv_vehicles.cod now
  holds the tank effects beside Merville's database (harmless; the db-only one in logs-backup\packs).

---

## 0.5.816: the tank effects pack loaded from sp\vehfx (the tank explosion)

- Why the tank's muzzle blast and shell explosion never showed on Seelow: effects are found through the
  asset manager's slot of each archive (*(0x82BCC824 + 4*(n+13)), filled as an archive's files are set up).
  tankboom11 logged the slots: the vehicles' effects pack (5) and database pack (6) 00000000, as the front
  end's (2, 3), while every other archive loaded beside the map has one (weapusge, all the sp\ packs). The
  two packs were loaded from mp\mp_merv\, another map's folder, and their slots were released when the
  level started. Not their content: with or without their FLI lists, effects grafted into Seelow's own level
  archive or into the database pack, nothing was found (tankboom5-10). The effects pack is now loaded
  from sp\vehfx\mp_merv_vehiclefx.cod (copied there; its FLI list empty).

## 0.5.815: the Kangaroo's treads; the archives' lookup tables logged

- The user (0.5.814): "the treads werent on the kangaroo on merville". The Kangaroo's archive is loaded
  after Saint-Lo's tank cut (0.5.788), which has v_tracked_tread and v_tracked_wheel, but the cut was put
  in the lookup lists (g_tankArchive) only at the end of the vehicles' loading, after the Kangaroo's:
  its materials found no shaders. Now set as soon as the cut is in.
- The tank effects: tankboom4/5 found them in NO archive (the effects pack with and without its FLI list:
  not the cause, the pack put back as it was). mervfx1: on Merville they are found in the map's own
  archive's lists. Logged once now: each archive's lookup table, its categories' entry counts.

## 0.5.814: a tank effect not found: every archive tried once (which has it)

- tankboom3 (0.5.813): mf_panzeriv_dirt_w, tank_concrete, tank_mud still NOT FOUND with the effects pack
  tried. Now a missing tank_/mf_ effect is looked for in the effects pack and then in every archive (0-63)
  once per name; the archive that has it is used from then on and logged ("found in archive N" / "in NO
  archive").

## 0.5.813: the tank's muzzle blast and its shells' explosions drawn

- dual4 (0.5.812): "effects: mf_panzeriv_dirt_w looked up in archive 6: NOT FOUND". A tank gun's events
  are made with the vehicles' DATABASE pack as the scope (0.5.779/0.5.797), and an effect is looked up in
  the scope archive (sub_824402C8 -> sub_82474FD8, category 9); the tanks' effects (mf_panzerIV_dirt_w,
  mf_sherman_generic_w, tank_dirt/mud/grass/metal/concrete/water/generic, tank_shell_trail) are in the
  vehicles' EFFECTS pack (mp_merv_vehiclefx.cod). Not found, no effect: the user's "i wanna see the
  explosion when i shoot a round with my tank". Now an effect not found while the scope is the database
  pack is looked for again in the effects pack. Logged "... found in the vehicles' effects pack".
- dual4: the dual Colts' reload one at a time seen on screen: first only the right Colt (its slide back,
  the left out of sight), then only the left one with the other hand feeding it (14 rounds after).

## 0.5.812: tests: the tank effects' lookups logged; a dual Colt test from inside the match

- Logged: each tank / mortar effect looked up by name (sub_824402C8: category 9 in the scope archive), the
  archive it was looked for in and found / NOT FOUND (no effect drawn), for the user's unseen tank hits.
- COD3_DUAL_TEST=1: with the dual Colts first in hand, 16 shots 0.6 s apart from 5 s (the clip emptied),
  6 more at 25 s and a reload at 32 s (Input::TestHold). COD3_PAD's presses had reached the menus and
  started the campaign (dual3); these come only in a match.
- COD3_EVENT_SHOTS_STEP / _COUNT: the burst's spacing (ms) and length.

## 0.5.811: dual Colts: reload one at a time, hip fire only, a left flash, set apart

The user's play of 0.5.810: "when reloading its showing two hands come into screen", no aiming down the
sights with two pistols, a flash on the second pistol, the guns "really close together almost touching".
- Reload one pistol at a time: when this machine's player's Colt reload sound comes (sub_82554BA0), for the
  gun's reload time (+1548 ms, +1552 when empty) the left gun and arm are not drawn: the free left hand
  reloads the right Colt (the Colt's own reload); every frame of the right arm's draws is recorded. Then for
  as long the right gun and arm are not drawn and the left plays that recording mirrored (the right hand
  reloads the left Colt), the right trigger held back meanwhile (Input::SetDualPistols).
- Hip fire only: the left trigger (and the mouse's aim button) kept from the title while they are in hand.
- The left gun's muzzle flash: on each of his dual Colt shots (event 1) the Colt's own flash event is made on
  a script_origin at the left gun's muzzle (COD3_DUAL_FLASH="ahead right up", default 34 11 -7).
- Set apart: the right gun moved right and the left one left by 10% of the screen's half width each
  (COD3_DUAL_SPREAD, default 0.10).
- Tests: COD3_EVENT_SHOTS=<part of an event's name>: screenshots every 100 ms for 1.5 s from such an event;
  tank_fire takes a Panzer IV first (no bot has got into a Sherman on Seelow).
- The user's play: their tank shells' hits were made (projexplode_panzeriv_turret_mud / _metal: the
  effects tank_mud / tank_metal from the vehicles' effects pack) but no explosion was seen; checked next.

## 0.5.810: the dual Colts can be picked (the ini's class weapon limits)

- dual1 (0.5.809): colt_dual made (weapon 49 on Seelow, a clip of 14) and the arms' meshes found, but the
  class's primary 25 was read back as 0: the ini's limits were 24 (primary) and 26 (sidearm). Now 25 and 27.

## 0.5.809: dual Colts (first try): a left arm with its own Colt

- mgcheck2 (0.5.808): the tank machine gun's own fire is heard: weapon_mg50cal_sherman_tank_gunner_weapon_
  fire_3rd -> MG50cal_FIRE_3RD, 269 rounds in 32 bursts; its world flash and ringoff made too.
- New weapon "COLT .45 DUAL" (colt_dual: class editor primary 25 or sidearm; a copy of the Colt, 14
  rounds). The retail game has only the cut give code and the names. While it is held, the first-person
  arm and Colt are drawn a second time mirrored left to right as the left arm and its Colt: the renderer
  draws each draw of the Colt's view meshes and the arms' meshes (their vertex buffers,
  Render::SetMirrorBuffers) again with the vertex program's c12 (the clip's x row) negated and no culling
  (RenderCommands::PrepareMirror). Both guns move together (one animation). COD3_DUAL_NO_MIRROR=1: off.
- Logged: "dual colts: the arms' mesh ...", "dual colts: in hand: N meshes mirrored", "render: mirror:
  vertex program ... mirrored (c12 ...)".

## 0.5.808: the tank machine gun's events by their whole names, a round each 110 ms

- mgcheck1 (0.5.807): the gunner fired 8 bursts and the named events were made, but cut to 48 characters
  ("weapon_mg50cal_sherman_tank_gunner_weapon_fire_3", found nothing: 0) and every frame (event 25 comes each
  frame while the gun fires: 1121 in 8 bursts). Named events now take 111 characters, and the MG's own fire
  and flash are made once each 110 ms a gunner (the .50 cal's ~550 rounds a minute).
- seelowcheck3 (0.5.807, the host outdoors near the Panzer IV): on screen, the mortar's real shell going off
  on the tank (fire, sparks, a smoke column; kill feed "HOVGtv [mortar] Bot 14") and a tank shell's dust.
  The cannon's hits: projexplode_panzeriv_turret_grass/mud/rock/wood/plaster with SHELL_EXPLO_MAINIMP_TANK.

## 0.5.807: the tank machine gun's rounds make its own fire and flash

- seelowcheck2 (0.5.806): the gunner got in (seat 1) and fired 7 bursts. Its event 2 is the bullet's HIT
  (bullethit_<surface>: it matched on some surfaces, not others), not the shot. The round itself is event
  25 (from 82591174, about ten a second while firing). Each round now makes the gun's own named fire
  (weapon_mg50cal_sherman_tank_gunner_weapon_fire_1st/_3rd) and world flash from Merville's table, unless
  the game's own shot event (8, as the cannon's from 8246A5D8) came for a machine gun within 0.5 s.
- seelowcheck2: the mortar's real hits again (projexplode_81mm_wood: the host had set it up in a barn).

## 0.5.806: tests: a tank gunner of the driver's side; the mortar's range for a test

- seelowcheck1 (0.5.805, Seelow): the campaign banks open from D:\sp\... now, and the mortar's own sounds
  are found: Mortar_Slide (its loading), Mortar_Fire, Mortar_Miss (10 of 10 each); the real hits made
  projexplode_81mm_mud; the tanks' engine and tread cues are found too (PanzerIV_Idle, PanzerIV_Loop,
  Tank_Suspension, TANK_HATCH_OPEN/CLOSE). Still not found: SHELL_EXPLO_DIRT/ROCK/WOOD_TANK (no
  multiplayer or campaign bank on the disc has those names as text; the tank hit's SHELL_EXPLO_MAINIMP_TANK
  plays).
- tank_fire's gunner is a bot of the driver's side (seelowcheck1's could not get in, 16 tries).
- COD3_MORTAR_TEST_RANGE=N: a test's mortar rounds land N ahead, in the camera's view.

## 0.5.805: the campaign sound banks load on map packs (Seelow): the mortar's sounds back

- mortarreal1 (0.5.803, Seelow): the mortar's REAL hit works: the game's own mortar_81mm_turret shell,
  launched over the landing point, made Laison's projexplode_81mm_wood and projexplode_81mm_mud (the
  surfaces it hit); the stand-ins went off.
- The same run: Mortar_Slide, Mortar_Fire, Mortar_Miss -> -1 on Seelow (found on Merville), and the tank
  hits' SHELL_EXPLO_DIRT_TANK / ROCK_TANK / WOOD_TANK -> -1. The campaign banks were never read on Seelow:
  "dlc5:\sp\mace2\mace2.wbk NOT FOUND" (and forest, laison). The map's bank path comes with no drive, so
  their paths were relative and the game put them on the map pack's drive. They are now always D:\sp\....
  (The user: "the mortars werent making there loading sounds".)

## 0.5.804: the tank machine gun's own fire and flash

- The tank machine gun's shot (event 2 of mg50cal_sherman_tank_gunner) matches no rule in Merville's set on
  Seelow (-1 every time, tankmortar2), so it was silent and had no flash, though mp_merville.dbb holds its
  own weapon_mg50cal_sherman_tank_gunner_weapon_fire_1st/_3rd and _world_flash. When its shot matched no
  rule, each shot now makes those named events at the gunner from Merville's table (fire_1st for this
  machine's player in that vehicle, else fire_3rd, and the world flash). Logged: "...'s shot (event 2)
  matched no rule: its named events made by name".

## 0.5.803: the mortar round's real hit: the game's own mortar shell

- mervtank2 (0.5.802, Merville): the round's hit made as mortar_81mm_turret's event 56 on a script_origin
  still made only the set's generic events: such an event has no surface (the game adds it from the
  shell's own hit), so no projexplode_81mm_<surface> rule matched.
- Now, on the host, where our round lands the game's own mortar_81mm_turret shell is launched by the
  vehicle shell launcher (sub_82582A98: owner entity, origin, direction, fuse) from 60 over the spot,
  straight down, with the shooter as its owner (his kill) and Laison's archive as its scope. The game finds
  the surface and makes Laison's own projexplode_81mm_<surface> (effect and sound) and the shell's damage.
  The stand-ins play only if that launch fails. COD3_MORTAR_REAL_HIT=0: off.
- Correction: "<gun>'s shell goes off at" (0.5.800) logged event 56 from 0x82582D98, which is the vehicle
  shell's LAUNCH (sub_82582A98 queues event 56 for class-2 weapons), not its hit. The tank shells were not
  bursting at the muzzle; their real hits are the projexplode_panzeriv_turret_rock/wood/plaster events.

## 0.5.802: the mortar's hit as its own weapon's; tank_fire takes his side's tank

- mervtank1 (0.5.801, Merville, the control): Laison's set laisonriver.fx is found; the tube's event 1 makes
  weapon_81mm_weapon_view_flash. But the round's event 56 under "mortar_shell" (the set has no rules for
  it) made the set's GENERIC events (footsteps, landings, body_impact), and that wrongly turned the stand-ins
  off. Laison's own hit is projexplode_81mm_<surface> (the tube's weapon, mortar_81mm_turret): the round's
  hit is now that weapon's event 56, and the stand-ins go off only once a projexplode_81mm event is made.
- On Merville the mortar's own cues play: Mortar_Slide, Mortar_Fire, Mortar_Miss (8 of 8 each).
- A weapon event made inside another one's starts with the game's own scope (the outer one's is put back).
- tank_fire: his own side's tank first (Allies a Sherman, Axis a Panzer IV): mervtank1's Axis bot pressed
  use at a Sherman twelve times.

## 0.5.801: mortar_test; the mortar's stand-in blasts off once its own explosion works

- tankmortar2 (0.5.800, Seelow): the driver fires the Panzer IV's cannon and its own events play, but every
  shell goes off within ~120 of the tank at muzzle height (e.g. tank 3812 4775 -47, shell 3856 4663 65).
  The gunner fired the 50 cal (34 bursts) but none of its events matched in Merville's set (-1), though
  mp_merville.dbb has weapon_mg50cal_sherman_tank_gunner_weapon_fire_3rd etc. Next: the same on Merville.
- "mortar_test [rounds]" (console/COD3_CMD): the host's mortar set up and a round fired every 2.5 s
  (tests cannot press d-pad or RT; streak_give mortar alone was never set up).
- The mortar round's stand-ins (the panzerschreck's and frag's blasts, the hand-played Mortar_Explo) are
  played only until the round's own event 56 has been made from Laison's table once
  ("the mortar round's own explosion works"). COD3_MORTAR_STANDIN=1: always.

## 0.5.800: where tank shells and mortar rounds go off; tank_fire skips tanks it cannot enter

- tankfire4 (0.5.798, sound logged): the tank's own cues are found and played: PANZERIV_FIRE_3RD,
  GS_Tank_ringoff, GR_Tank (its reload), SHELL_EXPLO_MAINIMP_TANK (the shell's hit). Not found:
  SHELL_EXPLO_ROLLOFF_TANK, Tank_Suspension. Every shell went off on surface "none".
- Logged now: "<gun>'s shell goes off at x y z" (tank shells, mortar rounds), and with each tank_fire shot
  the driver's view pitch and yaw and the tank's place, to find where the shells fly.
- tankmortar1 (0.5.799): the bot tried the same Sherman ten times; a tank he cannot get into is now skipped.

## 0.5.799: the mortar's own events from Laison's table; tank_fire brings a gunner

- 0.5.798's test (tankfire3): the bot fires the Panzer IV's cannon, the set mp_merville.fx is found in the
  vehicles' pack and Merville's own events play: weapon_panzeriv_turret_weapon_world_flash, _fire_3rd,
  _fire_3rd_ringoff, and the shell's projexplode_panzeriv_turret_rock / wood / plaster / none.
- The mortar had the same fault: its round's explosion (mortar_shell event 56) and the tube's events are
  looked up in the map's table, which has no mortar (0.5.308: "made no explosion"; the panzerschreck's and
  frag's blasts stood in). Now they are looked up in Laison's database (archive g_laisonArchive, the set
  laisonriver.fx, read from laisonriver.dbb) and each of the set's events is made with Laison's archive as
  the scope. Logged "the mortar's event set looked up ..." and "mortar event ... made from Laison's table".
  COD3_MORTAR_EVENTS=0: off.
- "tank_fire [seconds] [crew]": with crew 2 (the default) a second bot gets in as the gunner once the driver
  is in and fires the tank's machine gun in bursts (1.2 s every 4 s) at the same ground.

## 0.5.798: bots can fire a tank's cannon

- 0.5.797's test (tankfire2): the bot got into the Panzer IV and held fire 41 times, but no tank event
  came. 796's tankfire1 turret events were the HOST's ("host buttons 00000001" just before them).
- A vehicle's update (sub_82582280) fires its gun only for a rider whose move-lock byte (0x82C73024 + his
  client's +2804) is clear, with fire in his buttons (+1940). That byte is set for every bot (ThinkBots
  clears it only during his own think), so a bot in a tank could never fire the cannon. While a vehicle
  updates, the bots' bytes are now clear, as during their thinks. Bots in tanks now fire them at enemies too.

## 0.5.797: the tank guns' event set looked for in the vehicles' pack

- 0.5.796's test (tankfire1: a bot in a Panzer IV fired 41 times on Seelow): every tank event still -1.
  -1 is sub_82555160's start value: the set was not found, so none of its events was made. The lookup
  (sub_8254D628 -> sub_820D71E8) searches only the scope archive: 795 gave it the right name
  (mp_merville.fx) but in the map's archive (Rouen's copy), 794 the pack but no name.
- Now both: the name is mp_merville.fx and, for that one lookup, the scope is the vehicles' pack (Merville's
  database). Logged: "the set mp_merville.fx looked up in the vehicles' pack ...: found / NOT found".

## 0.5.796: "tank_fire [seconds]": a test bot fires a tank's own gun

- Tests could not fire a tank, and bots got into one only by luck (tankevents5: none did), so 0.5.795's
  tank events were never seen. The console/COD3_CMD command "tank_fire [seconds]" (60 by default) takes the
  first living bot to the free tank nearest the host (the game's teleport, sub_8255E118), has him press use
  until he is in, then fire the tank's gun (fire held 0.3 s every 3 s) at the ground ~700 ahead of it. The
  shot, shell and events are the game's own; nothing is made by hand. Logged "bots: tank_fire ...".

## 0.5.795: the tank guns' event sets: the MAP archive named mp_merville for the call

- 0.5.794's test (tankevents4, 9 min, 4 bots: a bot fired a Panzer IV and a Sherman's 50 cal): still -1 for
  every event. sub_82472BD8 gives no object for the pack's archive (00000000), so the set's name was read
  from nothing.
- Now the scope stays the map's own archive, and its object is named mp_merville for the call: the set is
  looked up as mp_merville.fx (in Merville's database, loaded with the pack); each of its events is then
  made from Merville's table by sub_82554BA0's switch (0.5.779, by the events' names). Logged once: the
  map archive's own name.

## 0.5.794: the tank guns' event sets looked up as mp_merville.fx

- 0.5.793's test (tankevents2): the switch now caught the tank guns (sherman_turret events 25, 26, 27, 2,
  53, 7, 41, 45, 46; the 50 cal's 25, 26), but every lookup came back -1. sub_82555160 finds a weapon's set
  by NAME: the scope archive's name (its object's +4, sub_82472BD8) and ".fx" (0x82067258), category
  EVENT.FX (0x8206725C). Merville's database has the entry as mp_merville.fx; the pack's own name found none.
- For a tank gun's event the pack's archive is named mp_merville for that call (and its scope the pack, as
  before), then given its name back. Logged once: the pack archive's real name.

## 0.5.793: the tank event switch reads the record's weapon name where it is

- 0.5.792's test (tankevents1): the switch read the event record's +500, 0 every time, so no tank gun was
  recognised. sub_82556180 fills the record with the weapon's name at +16 (up to 128 bytes) and the event
  number at +292: read there now. Bots on Seelow drive and fire the Panzers in tests (PanzerIV_Turret
  events 25, 26, 56 in tankevents1 and blastview2), so a test can show the switch working.

## 0.5.792: the tanks' own shot and hit events, made by the game from Merville's table

- The user (0.5.790): "why dont the tanks shoot an explosive round like they do on merville". A weapon's
  numbered event (the shot, the shell's hit) is made by sub_82555160, which looks the weapon's event set up
  in the MAP's event table (scope *(*0x82BCC828 + 40), the map's archive) and then makes each event
  (sub_82554BA0). Seelow's table is Rouen's, with no tank guns: nothing was made. 0.5.779 switched the
  scope one step too late (in sub_82554BA0, never reached); 0.5.780 made the events by hand at our own
  origins (heard, never seen: blastview1/2 on 0.5.791 could not show even the Hellfire's beside it).
- sub_82555160: for a tank gun (its record's weapon at +500: panzeriv_turret, sherman_turret, the tanks'
  MGs) the scope is Merville's game database (the vehicles' pack) for the whole call, so the game makes
  Merville's own events, its fire sounds, flashes and projexplode_<turret>_<surface> (tank_dirt, tank_metal
  ... in the effects pack), exactly as on Merville. Logs: the first 16 weapon events with their +500 key,
  and "<gun>'s event N made by the game from Merville's table". COD3_TANK_EVENTS=0: off.
- 0.5.780's hand-made shots and hits are off (COD3_TANK_STANDIN=1 brings them back).

## 0.5.791: Saint-Lo's cut keeps both tanks (790's were invisible); "tankblast" to look at a shell's hit

- The user on 0.5.790: Merville then Seelow loaded, but the tanks were invisible, the mounted gun plain grey,
  and a tank's shot was heard with no explosion seen where it hit.
- 0.5.785's cut of Saint-Lo's archive kept only the Panzer IV (--keep '(panzer|tankshell)', the Sherman out):
  the 50 cal's textures became 8x8 stand-ins, and the cut, first in the lists, answered for the tanks' parts
  with nothing to draw. A gentler cut (every model's textures kept, the Sherman and the guns in; the P-47,
  the Fw 190's parts and the prisoners' walks out; 7 blocks of 8, 17.5 MB) draws both tanks whole, textured,
  with the 50 cal (test tankview1: a Panzer IV and a Sherman on Seelow, side by side screenshots). Installed
  in its place; seelow_install.sh builds it so. (The tight cut: logs-backup\vehicles_panzer.v1-invisible.cod.)
- The play log: the shell's explosion event (Merville's projexplode_sherman_turret_dirt, whose effect is
  weapon/tank/tank_dirt, in the vehicles' effects pack) was made at the hit 2 times of 6 shots, ~140 units
  away, yet nothing was seen. Console/COD3_CMD "tankblast x y z event" makes one where asked, for a test to
  look at it beside the Hellfire's (projexplode_mp_binoculars_dirt, seen).

## 0.5.790: the vehicles' packs with the first map, and the Stuka's own cut (Seelow after Merville: no tanks)

- 0.5.789 tested: Merville then Seelow (mervseelow7) ran all 297 s, Seelow with its vehicles (10 of its own
  moved to their places). The physics presets (5, Merville's names) and kinds behave: not the cause.
- mervseelow6 (0.5.788), the same test, had no tanks on Seelow: the vehicles' database pack was loaded into
  a full game heap (603.6 of 608 MB) and only part of it came in (35 of its 76 entries: 2 kinds of 8, none
  of its 6 guns). On a map after the first the last map's memory is still held when these load (Seelow
  first: the effects pack at 384 MB; Seelow after Merville: at 602-603.6 MB).
- The kept archives fill the heap: the four campaign weapons archives ~120 MB, the Stuka's (Crossroads')
  12.5, the mortar's (Laison's vehicles) 10.7, the Panzer IV's cut 8.7, the Kangaroo's 8.1, the Typhoon's 3.3.
- The vehicles' effects and database packs and the Panzer IV's cut are loaded with the FIRST map, whatever
  it is, when a map other than Merville/Poissons has vehicles saved; kept, used only on a map with tanks
  ("the vehicles' packs kept loaded for a map with tanks (not used on X)").
- sp\crssrds\vehicles_stuka.cod (install only, seelow_install.sh): the Stuka, the guns and the weapons'
  textures; its Sherman (Laison's has one), Kangaroo textures, A-20, C-47 out; 3 blocks of 5, 4.1 MB.
  Loaded in place of the whole one when present: about 6 MB back on every map.

## 0.5.789: logs of the vehicles' physics presets and kinds from map to map

- 0.5.788 tested: Merville then Seelow (mervseelow6) ran all 337 s, no crash; the cut and the Kangaroo were
  loaded before Merville and kept for Seelow ("still loaded as 5 / 6"). But Seelow had no vehicles: the
  pack's vehicle files gave 2 kinds of 8 ("Merville's vehicle files read (2 kinds now)"), Laison's
  vehicle names came out wrong ("none"), Seelow's own scene vehicles were not there, and a call went through
  0x82703610 as Seelow's archive opened. Something the game keeps from map to map still holds Merville's.
- Logged now: as each map makes its vehicles, the kinds and the physics presets (the pool of 10 at
  0x8267CD80, each preset's name), and each vehicle file read with the kinds before and after.

## 0.5.788: the Kangaroo's archive before the map on Merville too (Seelow after Merville froze)

- 0.5.787 tested: Seelow, Merville, Seelow (mapchain4) ran all 478 s, no crash; the guard caught the one bad
  peer list once (data 00000004, capacity 112, count 24) as Merville started; the session's real lists were
  taken out as normal. Seelow loaded again after Merville with its world, fires and vehicles.
- Merville then Seelow (mervseelow5) still froze, as Seelow set up its vehicles: a call through 0x82704190.
  On Merville the Kangaroo's archive is loaded AFTER the map (0.5.85: it needs v_tracked_tread and
  v_tracked_wheel, then only in the map's archive), into Merville's banks; Seelow took it as "still loaded"
  (13) after those banks were freed. Seelow first never showed it: there the Kangaroo loads before the map.
- The Panzer IV's cut (0.5.785) has both shaders: on Merville and Poissons it is loaded first and the
  Kangaroo's archive after it, before the map, as on Seelow; both are kept for the maps after. The after-map
  load stays only for an install without the cut.

## 0.5.787: guard: DemonWare's session leave skips a peer that is not one (Merville after Seelow)

- peertrap1 (0.5.786, Seelow then Merville, the trap on): the "list" at 0x826F9BA0 that bdSession::leave
  walked is written all along by DemonWare's own code at the match's start (sub_821AEE30, sub_821AF9F0,
  sub_821AFB08, sub_821AFCE8, from the game's sub_82403840 / sub_823D7768) with {0x10, 0x70, 0x6F}: not a
  list at all. bdSession::leave (sub_821C1758) reaches it through one of the session's peers, a pointer to
  0x826F9B50 (static memory) where a peer should be (sub_821B1DC8: the peer's +80): the walk read from 0x10
  up through all memory.
- rouenmerv1 (Rouen then Merville, 0.5.786): no crash. Only a map with the tanks before Merville crashed.
- sub_821C08B0 (take an item out of a list): a list that cannot be one (data outside the memory the game
  allocates from, count above capacity, capacity above 65536) is not walked, and logged ("demonware: guard:
  NOT walked"). The first 12 good ones are logged too, to compare.

## 0.5.786: a write trap on DemonWare's list (who corrupts it on a map change after Seelow)

- 0.5.785's test (mapchain3): Seelow's heap still ends at 603 MB with the Panzer IV's cut (the cut took 8.7 MB
  where the whole took about 16, but the heap fills to the same top either way), and no DemonWare
  allocation failed: the heap was not the cause. Merville after Seelow crashed the same way again, in
  bdSession::leave walking the list at 0x826F9BA0 (static memory, zeros in the image) with a garbage count.
- Ruled out: the vehicle physics pool (15, the game checks it; at 0x8267D600-0x82680B70), the vehicle kinds
  (once a map since 783), Laison's after-map archives (reused across maps since 0.5.296; 784).
- COD3_PEERTRAP=1: the list's page made read-only from the first map load; each write to its three words
  (data, capacity, count) logs the words before and after and the host frames as exe offsets ("peertrap:").
  Off unless set. The Panzer IV's cut stays (8.7 MB in place of about 16).

## 0.5.785: the Panzer IV's own cut of Saint-Lo's archive (map changes after Seelow ran the heap out)

- 0.5.784's test (mapchain2: Seelow, Merville, Seelow) still crashed on Merville, 8 s after the change. The
  frames are the game's DemonWare session code: bdSession::leave (strings at 0x820333C8, sub_821C1AA0 ->
  sub_821C1758 -> sub_821C08B0) walking a peer list with nothing under it, reading on up to 0x1C760000.
  seelowseelow1 (0.5.782) died the same way; mervseelow1-4 died elsewhere, each differently.
- The game heap (the 608 MB map block) is the common thread: Seelow alone 582 MB, Seelow with the tanks
  603.5 MB (4.5 MB left), Merville 590.6 MB (+21 MB of Seelow's tank archives = past 608). The one
  map-change run that lived (mylamps1, 0.5.737, five Seelow Night loads, no vehicles) had 17 MB left.
- The extra 21 MB are Saint-Lo's vehicles.cod (8 blocks: the Panzer IV, the Sherman, the P-47, the
  Kangaroo, Fw190 parts, prisoners' walks), the vehicles' effects and the database pack. Only the Panzer IV
  is needed from Saint-Lo's: the Sherman's model is in Laison's vehicles.cod, loaded on every map for the
  mortar (and Crossroads', the Stuka's).
- sp\saint_lo\vehicles_panzer.cod (install only, seelow_install.sh): the Panzer IV's models, wreck,
  skeleton, tread/wheel shaders, the shell and the 50 cal; 4 blocks, 8.9 MB. Loaded in place of the whole
  one when present ("Saint-Lo's Panzer IV (its cut)"). kapf.py's decompose() now takes compressed blocks
  (the campaign's), writing them plain.
- Logs: the game heap after each extra archive and campaign weapons archive loads; "demonware: an allocation
  of N bytes failed" if DemonWare's allocator (sub_821AD880) comes back empty.

## 0.5.784: Laison's after-map archives reused again (Merville after Seelow froze)

- Test mapchain1 on 0.5.783 (Seelow, then Merville, then Seelow): Seelow loaded twice fine, the vehicle files
  read once a map ("already read for this map: not again"). Merville in between froze: right after
  Laison's mortar archives were loaded again from scratch (0.5.781's mapBound: "loaded again, not reused"),
  the game's own asserts fired 254,000 times (DbgBreakPoint at 0x82543B8C from sub_8218D9B4) and the
  frames stopped (watchdog).
- 0.5.781's forced fresh reload of Laison's and the Kangaroo's after-map archives is taken out: they are
  reused as before 781. The crashes 781 aimed at came from the vehicle files read twice a map, fixed in 783.

## 0.5.783: vehicle files read once a map load (the real cause of the map-change crashes)

- 0.5.782's test (mervseelow4) still crashed; Seelow then Seelow crashed too (seelowseelow1): any second
  map with vehicles. The logs show why: the game calls sub_82561838 ("room for vehicles") twice a map, at
  the load ("asked for 20") and again before the match ("asked for 10" / "8"), and the hook read
  Merville's and the Kangaroo's vehicle files at every call: the kinds went 17 -> 31 -> 45 on Seelow and
  14 -> 27 on Merville, past the game's table, the memory after it overwritten during the first map; the
  next map's vehicles then spawned from it (sub_824427C0 under sub_8257E220, calls through garbage).
- The reads (Merville's pack, the Kangaroo's archive and its kind, Laison's mortar) are done once a map
  load (keyed by the map's archive number), and again only if the game has reset the kinds below what
  they left (a map with vehicle files of its own re-reads them at its second call). Log: "vehicles: the
  vehicle files already read for this map (N kinds): not again". Laison's old once-a-map guard (by the
  map's name) skipped a second load of the same map: replaced.
- 781's and 782's changes stay (after-map archives loaded fresh; Saint-Lo's models first): both safer.

## 0.5.782: the tanks' models from Saint-Lo's archive first

- 0.5.781's test (logs-backup/20261105-125104-mervseelow3-result): the after-map archives were loaded
  again ("... loaded again, not reused"), but Seelow after Merville still crashed the same way, spawning
  its vehicles (sub_824427C0 under sub_8257E220).
- Seelow has no tank models of its own, so v_shermanm4a3 is found in the extra archives in list order,
  where the Stuka's (Crossroads) and the mortar's (Laison) archives, which carry a v_shermanm4a3 too, came
  before Saint-Lo's. Both are kept from map to map; loaded beside Merville, their tanks' shaders were
  Merville's (gone on the next map). A fresh Seelow worked only because they were loaded beside it.
- The list hook (sub_82474CB8) puts Saint-Lo's archive (g_tankArchive, loaded fresh with each map) and the
  vehicles' effects pack ahead of every other extra archive.

## 0.5.781: archives loaded after a map are not reused by the next (Merville then Seelow crash)

- The user on 0.5.780: "seelow crashed my game when trying to load" (logs-backup/20261105-user-crash-780):
  Merville first, then Seelow. Reproduced (test mervseelow1, nobody firing): right after "room for 36
  vehicles", Seelow's vehicles' spawn read garbage (sub_824427C0 under sub_82581190, then calls through
  0x00000002 ... and 'APKF', a host fault in the video thread). Not the guns' code.
- Cause: on Merville the Kangaroo's archive and Laison's three (the campaign mortar) are loaded AFTER the
  map, into the map's own banks (33/34, Merville's archive's); with Merville gone their memory is freed,
  but LoadExtra still found their old objects and reused them on Seelow ("still loaded as 13-16"): every
  lookup through them read Seelow's data. With the Kangaroo off (mervseelow2) Laison's were still reused:
  still broken.
- ExtraArchive.mapBound: set after those after-map loads; LoadExtra then loads such an archive again on
  the next map instead of reusing it ("... was loaded after the last map (in its banks): loaded again").
  The archives kept on purpose (campaign weapons, Stuka, Typhoon, mortar's v_81mm, banks 78/79) are
  reused as before.

## 0.5.780: the tank guns through Merville's NAMED events; shots by its cues

- The user on 0.5.779 (logs-backup/20261105-user-play-tankguns6): no shot sound, no explosion. The log: not
  one "tank event" line: 779's switch never came. A WEAPON event (name + number) is made by sub_82555160,
  which first finds the weapon's event set in the map's table: Rouen's has no tank guns, so it stops there,
  before sub_82554BA0. A NAMED event (queued by sub_82556868, as the Hellfire's are) goes straight to
  sub_82554BA0, where 779 makes it from Merville's table.
- Merville's event names (its .dbb): weapon_panzeriv_turret_weapon_fire_1st/3rd, _world_flash, _view_flash;
  weapon_sherman_turret_weapon_*; weapon_mg50cal_sherman_tank_gunner_weapon_*; weapon_mg30cal_sherman_tank_*;
  projexplode_panzeriv_turret_<surface>, projexplode_sherman_turret_<surface>.
- A cannon shot (event 25) / an MG round (event 2): its sound Merville's own cue (PANZERIV/SHERMAN_FIRE_x,
  MG50CAL_FIRE_x, MG30cal_FIRE_x; the first a bank has) played as 778's (sub_824AFD70, heard; yours not
  placed, another's at the gun: MpFixesPlayCue); the cannon's flash its named world_flash event at the gun.
- Its shell's explosion (event 56): queued; from the frame, Merville's named projexplode_<turret>_dirt on
  one of 6 own script_origins at the hit (the Hellfire's way: effects on an origin are seen), parked after
  6 s, and the cue SHELL_EXPLO_DIRT_TANK at that place (sounds on an origin are not heard).
- Log: "vehicles: <gun>'s shot cue ...", "vehicles: a tank shell's explosion: Merville's ...", and 779's
  "tank event ... made from Merville's table" for the named events.

## 0.5.779: the tank guns' own events from Merville's event table (stand-ins out)

- The user on 0.5.778 (logs-backup/20261105-user-play-tankguns5): the turret and a tank's shot heard
  now, but no explosion where the shell lands ("like im firing a dud"); and: "i dont know why were using
  grenade animations it needs to be exactly like how a tank shoots".
- Why every stand-in fell short: a weapon event (shot, impact per surface) is made by sub_82554BA0 from
  the map archive's event table (sub_82490220 -> sub_820D71E8, scope *(*0x82BCC828 + 40) = the map's
  archive number): Rouen's for Seelow, which has no tank guns.
- Now, while a tank gun's event is made (its name holds panzeriv_turret, sherman_turret, _sherman_tank or
  _gunner), that scope is the vehicles' pack (Merville's game database: its own event table first, then
  every list archive: the effects pack, the banks), put back after. Merville's own entries play: the
  shots, the MGs, projexplode_<turret>_<surface> (tank_dirt, tank_metal ...). COD3_TANK_EVENTS=0: off.
  Log: "vehicles: tank event <name> made from Merville's table ...".
- 776-778's stand-ins taken out: the panzerschreck/frag replays (MpFixesTankBlastsFrame), the cues played
  by hand, the event-56 renaming.

## 0.5.778: the tank guns' real cues through the game's own play call; no lingering smoke

- The user on 0.5.777 (logs-backup/20261105-user-play-tankguns3): "a weird shot with smoke that just
  sits there and doesnt disappear, no explosion sounds or audio from the machine gun turret". The log:
  PANZERIV_FIRE_1ST (590652) and MG50CAL_FIRE_1ST (589983) found and asked for through sub_82556868
  at the player, never heard; the panzerschreck's blast on our origin left its smoke there.
- Merville's bank is Treyarch's own format (NSLB, XMA): no WAV extraction. The shots' cues are played
  as the game plays a cue itself, sub_824AFD70 (as sub_823AFFE0: r3 out, r4 *0x82BCBF80, r5 the name,
  r6 0, r7 1, r8 0, r9/r10 16-byte vectors, f1-f4 the constant at 0x825DA49C): yours not placed (zero
  vectors, as that caller), another's at the gun. Log: "vehicles: <gun> shot cue <name> played ...".
- The shell's explosion is the frag grenade's (event 56) on 6 origins of our own, each moved 60000
  under the map 4 s after (no smoke left in sight). Log: "... explosion played (the frag grenade's)".

## 0.5.777: tank shells' explosions seen, shots heard where the player is

- The user on 0.5.776 (logs-backup/20261104-user-play-tankguns2): "i can hear the sound of an explosion
  but i dont see one also the machine gun or turret still isnt making noise when firing". The log: the
  cues were found (MG50CAL_FIRE_3RD from Merville's bank; SHERMAN_FIRE_3RD -1, so SHELL_EXPLO_MAINIMP_TANK
  for the cannon: the boom heard at each shot), but played at the shooter's entity, the tank's gun (not
  a player: "another's"), and not heard. The renamed explosion (panzerschreck) was heard, not seen: the
  shell entity is gone at once and its effect with it.
- Explosions: queued (MpFixesQueueTankBlast) and played from the frame (MpFixesPlaneFrame) as the
  killcam plays a kept explosion (MpFixesKillcamFx): the panzerschreck's event 56 on one of our own
  script_origins at the shell's place and angles.
- Shots: this machine's player within 250 of the gun is the one in that tank: the first-person cue,
  played at him; another's: the third-person one at the gun. Cannon names: <gun>_FIRE_x, PANZERIV_FIRE_x,
  SHELL_EXPLO_MAINIMP_TANK, SHELL_EXPLO_MAINIMP.

## 0.5.776: the tank guns' shots and explosions through Rouen's database

- The user on 0.5.775 (logs-backup/20261104-user-play-tanksound1): "i can hear the tank audio but i cant
  hear the gun shots or explosions yet". The tanks' own cues now play (PanzerIV_Idle, PanzerIV_Move,
  TANK_HATCH_*: Merville's bank loads); the guns' events look nothing up.
- Why: a weapon's event sounds and effects come from the MAP's game database (.dbb event and impact
  tables, .gdbb cue tables): Seelow's is Rouen's, which has no Panzer/Sherman turret or tank MG entries
  (only their ring-off tails; projexplode only for bazooka, panzerschreck and grenades).
- CampaignEventName (the weapon event hook): on a map with the vehicles' pack, a tank shell's explosion
  (event 56) is played as the panzerschreck's (Rouen's database has its effect and sound per surface);
  a cannon shot (event 25) and an MG round (event 2) play a cue by name at the shooter (sub_82556868):
  the first that a loaded bank has (cue lookup sub_8212B268): PANZERIV/SHERMAN_FIRE_1ST/3RD (Merville's
  bank), else SHELL_EXPLO_MAINIMP_TANK, SHELL_EXPLO_MAINIMP; MG50CAL_FIRE_*, else MG30cal_FIRE_*.
  COD3_TANK_SOUNDS=0: off. Log: "vehicles: <gun>'s shot sound (...): <name>", "... shell explosion
  played as the panzerschreck's".

## 0.5.775: Merville's sound bank loaded with the map's

- The user on 0.5.773: "still no sounds or explosions". 0.5.774's test (logs-backup/20261104-223959-
  seelowfx5-result; no shell fired in it): Merville's bank now opens from D: (file ... opened), but
  Tank_Suspension still -1: it was asked to load early, as the vehicles' packs registered it (while the
  weapons' archive loads), and the request is lost before the map's banks are set up.
- Now loaded exactly as the campaign banks are, when the map's own bank registers (sub_824964E8 hook):
  registered with the map's kind and archive under the key mp_merv.wbk, path D:\mp\mp_merv\mp_merv.wbk,
  then asked to load (sub_824AE270), queued as a campaign bank. Only with the vehicles' packs beside the
  map (g_vehicleFxArchive / the database pack). 773's early request taken out (774's drive fix kept).
  Log: "sounds: Merville's bank D:\mp\mp_merv\mp_merv.wbk registered beside ...".

## 0.5.774: Merville's sound bank on the game's drive

- 0.5.773's test (logs-backup/20261104-223046-seelowfx4-result; the user drove a Sherman and fired 7
  shells in it): both packs load, the bank was asked to load, but the tanks' cues were still -1: the
  packs name the bank without a drive (mp\mp_merv\mp_merv.wbk) and beside a DLC map it was looked for on
  the map's drive (dlc5:\mp\mp_merv\mp_merv.wbk NOT FOUND), never opened. Its registration now gets
  D:\mp\mp_merv\mp_merv.wbk (as the campaign banks get D:\sp\...); the second pack's duplicate
  registration (FFFFFFFF) is not asked to load again.
- The asset log (sub_820B00C0, first 4000 names) also logs tank_*, projexplode*, weapon/*, mf_sherman*,
  mf_panzer*, vexplode* past the 4000 (it was full before the first shot): whether the shells' effects
  are looked up at all.
- Seelow day and night reinstalled with the fires under the cars (seelow_install.sh, 0.5.773): pictures in
  seelowfx4 show the car fire at 3240 1177 low at the car.

## 0.5.773: the tanks' sounds and a separate effects pack; Seelow's car fires under the cars

- The user on 0.5.772 + the effects pack (logs-backup/20261104-user-play-tankfx1): still no shell
  explosion, and no tank sound at all (cannon, the MG on top). The log: cues sherman_idle, Sherman_Move,
  Tank_Suspension, SHELL_EXPLO_DIRT_TANK, TANK_HATCH_OPEN, IMPACT_PLAYER_TANK_HIT -> -1.
- Sounds: the pack's database names mp\mp_merv\mp_merv.wbk, so the bank was registered (sub_824964E8)
  but never asked to load (as the campaign banks before 0.5.900). It now is, right after registering,
  as they are (sub_820B5170 key, sub_824AE270, queued as a campaign bank: searched after the map's).
  Not on Merville itself. Log: "sounds: Merville's bank ... asked to load".
- Explosions: the shells' effects are looked up in the map's lists, which the database pack must stay
  out of (771). The effects now have their own archive, mp\mp_merv\mp_merv_vehiclefx.cod (Merville's
  tank_*, the tanks' mf_*, exhaust_*, treadfx_*, peeloutfx_*, wheelfx_*, vdamage_*, vfire_*, vexplode_*
  and their textures; no database; 5.0 MB), loaded before the database pack and put in every list
  (g_vehicleFxArchive). The database pack is the database alone again (0.8 MB).
- seelow_install.sh: builds both packs; Seelow's car fires (day and night) at each car's origin - 40
  (was - 15): the flames start at the ground under the car, not on top (the user).

## 0.5.772: the vehicles' pack out of the map's lists again

- 0.5.771's test (logs-backup/20261104-212639-seelowfx1-result; the user fired 8 Panzer shells in it): no
  crash, but "weapons: 76 set up from the map": with the pack in the map's lists the map's own weapon
  set-up read all 24 of Merville's WEAPONFILEs from it (35 -> 76), and every campaign and own gun after
  was "left out: the game's 63 weapons are all in use" (Lee-Enfield, Bren, Sten, FG42, Webley,
  flamethrower, Mosin, signal flare...): the Mosin in hand drew green.
- The pack is kept out of the map's and zones' lists again (as 759/770); the effects stay in the pack
  (771's rebuild), named by its own database (set up in its scope). Whether the shells now explode:
  the user's next shot.
- Its check (seelowfx2): weapons normal again (35 -> 48 -> 54 -> 56) but the Mosin still glowed green:
  771's pack had every mf_ effect, the players' guns' muzzle flashes too (mf_fp_b25..100_v, mf_m1garand_rg,
  mf_shotgun_v/w, mf_cone_*, mf_cil_*, mf_bazooka_*). Pack rebuilt with the vehicles' only
  (mf_panzeriv_dirt_w, mf_sherman_generic_w, mf_20_armour_v): 82 files, 5.5 MB; test seelowfx3: the
  Mosin normal, no crash. Installed; seelow_install.sh builds it so. (No build: game data only.)

## 0.5.771: the tanks' effects (shell explosions)

- The user on 0.5.770 (logs-backup/20261104-user-play-tankguns1): "the machine gun works but shooting the
  tank seemed to not cause an explosion". The Panzer's shells hit (6 "event 56" explosions logged at the
  hit places) but drew nothing: their effects (projexplode_panzeriv_turret_<surface> ->
  weapon/tank/tank_<surface>.effect) are Merville's tank_* effect files, left out of the vehicles' pack
  with every other vehicle effect (muzzle flashes mf_*, tank_shell_trail, exhaust_*, treadfx_*,
  peeloutfx_*, wheelfx_*, vdamage_*, vfire_*, vexplode_*).
- The vehicles' pack rebuilt with them (repack.py --follow-imports keeps their textures): 3 blocks,
  94 files, 5.6 MB (was 0.8 MB). seelow_install.sh builds it so.
- mp_fixes.cpp: 759's keeping of the vehicle archive out of the map's and zones' lists now only for
  Merville's WHOLE archive (g_vehicleArchiveWhole): the pack is searched, after the map's own files.

## 0.5.770: the tanks' guns

- The user's play on 0.5.769 (logs-backup/20261104-user-play-tanks1, 23 min, no crash, heap steady at
  603 of 608 MB): "the vehicles work except shooting": the turret does not fire or turn, nor the mounted
  gun from the other seat.
- A vehicle's guns are weapons set up from WEAPONFILE entries (PanzerIV_Turret, Sherman_Turret,
  mg30cal_sherman_tank, mg50cal_sherman_tank_gunner, mg42_horch_gunner, mg50cal_wc51_gunner), Merville's
  own, in its game database (the vehicles' pack): only its VEHICLEFILEs were read.
- After the map's weapons (sub_8249E970), the pack's WEAPONFILEs are read as the map's are
  (sub_8254B318 ... sub_8249A380), only those whose names hold turret / _tank / _gunner
  (g_vehicleWeaponFilter; mg30cal and panzerschreck are the map's already), before the vehicle files
  name them. Their models (tankshell, vg_50cal, vg_mg42) are in Crossroads' vehicles archive (the Stuka's,
  loaded on every map). Log: "vehicles: the pack's weapon file ...", "the vehicles' guns read".
- Its check (logs-backup/20261104-211759-seelowguns1-result): the 6 set up (48 -> 54 weapons, Laison's
  then 56 of 63), the other 18 left out; Seelow's 10 tanks at the start; 144 fps, no crash (the window
  was closed at 2.5 min). Firing not tested (tests cannot get in a vehicle).

## 0.5.769: tanks from Seelow's own scene (vehicle data pack + Saint-Lo's models)

- Test seelowdb1 (no build): Merville's game database alone (repack.py: mp_merville.gdbb/.dbb/.cfgstrb,
  0.8 MB) as game\mp\mp_merv\mp_merv_vehicles.cod loads beside Seelow with no level of its own: the 8 MP
  vehicle kinds and the Kangaroo read from it, Seelow's world whole, no assert, no freeze.
- tools/carentan/scene.py vehicles <level .cod> <.ents>: WaW's script_vehicle places (panzer4_mp ->
  mp_panzeriv / v_panzer_iv, t34_mp -> mp_shermanTank / v_shermanm4a3) as script_vehicle records in the
  level scene, made from Rouen's sunk SpeedTree script_origins (the vehicle record's pointer slots
  exactly): class id 0x36, class, model, origin, angles, gametypes + vehicletype and their hash index
  (+0xE0: name hash, value). Strings where those records' own lighting values were. Seelow: 10 (5 + 5).
- mp_fixes.cpp: with the vehicles' database pack in use, Saint-Lo's sp\saint_lo\vehicles.cod (Panzer IV,
  Sherman models; no level) is loaded beside it and put in every archive list (g_tankArchive).
- Its test (logs-backup/20261104-010202-seelowtanks1-result): Seelow's own vehicles at its start: 5
  mp_panzeriv, 5 mp_shermanTank, textured at WaW's places; Seelow whole; no crash or freeze in 237 s.
  Heap 602.7 of 608 MB. Driving not tested yet.
- tools/waw/seelow_install.sh: the tanks (scene.py vehicles, day only) and the db pack (built into
  game\mp\mp_merv\mp_merv_vehicles.cod when missing); VEHICLES=0 leaves both out. Installed for play.

## 0.5.768: the level put back after Merville's archive (no new hook)

- ITS TEST (logs-backup/20261104-003103-seelowveh18-result): the level state was empty before AND after
  Merville's archive loaded ("00000000 ... 00000000, a level is in 0 -> 0"): Merville's level is made
  later (with the map's level, not in LoadExtra), so this put-back does nothing. The run also froze:
  Merville's archive reported "loaded" with none of its files handled, then a guest read at 0x30000014
  from sub_824783A0 under sub_82474CB8 while the Kangaroo's archive loaded (watchdog at 30 s). The
  whole-archive load is not reliable either. The block only runs while Merville's archive is loaded
  beside another map (vehicles.txt lines or vehicles_everywhere): never in normal play.

- 0.5.767's check (logs-backup/20261104-000314-seelowbase4-result): Seelow without vehicles plays.
- Merville's archive (vehicles beside another map) made Merville THE level, so Seelow's cells were
  never drawn. MpFixesLoadVehicleArchive now saves the level state before loading Merville's archive
  (the current level 0x82BCC894, the "a level is in" block 0x82C9A040 (660 bytes), the level name
  0x82C8A1C0 (268 bytes)) and writes it back after: the map's level, loaded later, is the only one.
  No new hook (761/762/766's hooks on the loaders broke loading). Log: "vehicles: the level before
  Merville's archive ... after it ... (put back)". COD3_MERV_LEVEL=1: as 767.

## 0.5.767: back to 765 (the level hook out)

- 0.5.766's test (logs-backup/20261103-235611-seelowveh17-result, with Merville): the load froze right
  after Seelow's archive opened (game heap 520 of 608 MB, nothing more loaded); the hook's own line never
  came (it did nothing), yet the load changed. Like 761/762 (764's crash without Merville), a new hook on
  these loader functions breaks loading even while it only passes the call on. Taken out: as 765, which
  plays Seelow without vehicles (seelowbase3).
- Vehicles on Seelow stay parked (vehicles.txt without Seelow lines).

## 0.5.766: Merville's level kept from becoming the level

- 0.5.765's baseline (logs-backup/20261103-234629-seelowbase3-result): Seelow without Merville loads
  and plays again: one of the 761/762 hooks caused 764's crash. Both are gone now (they did not help).
- Its pictures, against the vehicle tests' at the same spots, turn the problem round: with Merville's
  archive loaded, Seelow's OWN world is not drawn (houses, walls, ground: its cells), only its trees,
  reeds and fires over empty space: the "tan ground". War had Merville's 8 flags, not Seelow's 3.
- Merville's archive carries a level of its own, and its level handler (sub_824346C8) made it the
  level (0x82BCC894; the "a level is in" block at 0x82C9A040; its name at 0x82C8A1C0). Seelow's,
  after it, met a level already in (the DbgBreakPoint from 0x82434730 in every vehicle test).
- While Merville's archive loads beside a map, those are put back as they were after its level
  handler. Log: "vehicles: Merville's level ... not made the level". COD3_MERV_LEVEL=1: as 765.

## 0.5.765: bisect build (761/762 hooks compiled out)

- 0.5.764's test with vehicles (logs-backup/20261103-232834-seelowveh16-result): 185 of Merville's world
  meshes unbound (cell_*, sbm*), no crash, but the pictures the same: the tan is not those.
- Its baseline WITHOUT Merville (seelowbase1, seelowbase2): crashed twice at ~27 s, a guest read of
  0xBAD10001 / 0xBB350009 in sub_82140EA8 under sub_8247CE30, while LoadExtra loaded the Typhoon's
  archive (MpFixesLoadPlaneArchives). 0.5.758 loaded the same archive on Seelow (flarekits1).
- This build compiles out the two hooks new since 758 that are present on every run (761's
  sub_82474FD8 diagnostic, 762's sub_82476E88 unbinding; COD3_MERV_HOOKS 0) to see whether one of
  them causes it.

## 0.5.764: Merville's world meshes by their real names

- 0.5.763's test (logs-backup/20261103-232110-seelowveh15-result; the first try, seelowveh14, typed
  devmap before the DLC was mounted and the game started the campaign: closed): Merville's archive
  binds 485 meshes as it loads; its world's are cell_N_mesh_N (165, its 8 cells) and sbm1-sbm20 (its
  prop batches). 762 looked for the material names, none came.
- 762's unbinding now takes those names (cell_<digits>_mesh_<digits>, sbm<digits>), only while
  Merville's archive loads beside another map. COD3_MERV_WORLD=1 keeps them.
- 763's load log only with COD3_MERV_LOADLOG=1.

## 0.5.763: what Merville's archive registers as it loads (diagnostic)

- 0.5.762's test (logs-backup/20261103-230713-seelowveh13-result): loads, vehicles placed, but no
  "left unbound" line: the MESH handler never saw a .mp_merville_ or .sbmodel name while Merville
  loaded. Its world comes in another way.
- While Merville's archive loads beside a map, each resource it makes (sub_82475550, "made"), puts in
  a table (sub_82475610, "put" with its kind) or binds as a mesh (sub_82476E88, "mesh") is logged
  ("merville load:" lines, 1500 at most).

## 0.5.762: Merville's world left out beside another map

- 0.5.761's test (logs-backup/20261103-214418-seelowveh11-result): Seelow's terrain names are found in
  Seelow's own archives only (scopes 14/16; Merville's not searched). Not a name clash: the tan is
  Merville's own level, loaded with its archive: its 8 cells' meshes (matNNNN_NNNN.mp_merville_000_SPLITk,
  368) and its 20 prop batches' (matNNNN.sbmodelN).
- While Merville's archive loads beside another map (the vehicles), each of those meshes is unbound
  again right after the MESH handler (sub_82476E88) binds it: its slot in Merville's index cleared.
  A cell or batch with no meshes bound draws nothing. Vehicles, props and the rest kept.
  COD3_MERV_WORLD=1: as 761. Log: "vehicles: Merville's world mesh ... left unbound".

## 0.5.761: which archive Seelow's terrain names come from (diagnostic)

- 0.5.760's test (logs-backup/20261103-213527-seelowveh10-result): still tan. The no-scope list is
  only the archives being loaded ("0 1 7", "0 1 9"): Merville was never in it. 760's change taken out.
- While Merville's archive is beside a map, each look-up of a terrain name (pg_*, lm0*) is logged
  ("scopes:" lines, 80 at most): its scope, the archives searched, and which of them have the name.

## 0.5.760: Merville's archive searched last

- 0.5.759's test (logs-backup/20261103-212315-seelowveh9-result): no crash, the Panzer, Sherman,
  Kangaroo and BMW moved to their Seelow spots and drawn with their textures, but the ground still
  flat tan: the map's lists were not the way.
- Seelow's archives and Merville's share 580 names, among them the terrain's: pg_tpatch_000_NNN,
  pg_colorlm_000_NNN (each patch's colour map), lm0000-0002 (Seelow is built on Rouen's files).
  The terrain looks them up with no scope (sub_82474FD8 scope -1: every loaded archive,
  sub_82474648's list, the first that has the name wins), and Merville's archive (5) is loaded
  before Seelow's (14-17).
- That list now has Merville's and the Kangaroo's archives last: the map's own names win, the
  vehicles' still found. COD3_MERV_LAST=0: as 759. Log: "archives: every archive's list".

## 0.5.759: Merville's archive out of the map's texture lists

- Vehicles on Seelow (Merville's whole archive loaded beside it, test seelowveh1): Seelow's ground drew
  flat tan, smeared. With Merville's cells and prop batches taken out of its archive the tan stayed, so
  it is not Merville's world: Seelow (built on Rouen's files) finds textures of the same names in
  Merville's archive, as Fuel Plant did in Laison's (0.5.294).
- Merville's archive is left out of the map's and its zones' archive lists, as Laison's are; its own
  list (its vehicles) and the Kangaroo's still have it. COD3_MERV_LISTS=all: as 758.

## 0.5.758: the throw trace out

- 0.5.757's test (logs-backup/20261103-195142-flarebots5-result, 8 bots, every other with flares,
  throwing every 10 s): BOTS THROW FLARES: 29 lit in 4 minutes, each flying, lit after its fuse, come to
  rest with the stick laid and its grenade sunk; the smoke-bots' smokes go too.
- The 752-757 smoke-kind throw trace taken out (COD3_BOT_SMOKE_EVERY and COD3_BOT_FLARES stay, for tests).
- Its test (logs-backup/20261103-200031-flarekits1-result, 28 bots, no flare switch): the install's
  bot_kits.txt lines given out (slot 28 a scout with the scoped Mosin and flares, 29 a rifleman with
  the Webley second and flares, 27 a medic with the Webley); the kits' flares thrown (11) and lit
  (31 in all), no crash.

## 0.5.757: bots see the flare by its name

- 0.5.756's test (logs-backup/20261103-194244-flarebots4-result): the flare-bots' throws named weapon 0.
  bots.cpp's grenade look-ups skip a weapon whose name lies at or past 0xA0000000; the copies' names
  (GuestText) lie at 0xAA... (the event hook's 0.5.741 trap again). Five such checks now allow up to
  0xC0000000 (BotNadeAmmo, BotNadeWeapon, the rifle grenade's, two logs).

## 0.5.756: bots throw the flare they have

- 0.5.755's test (logs-backup/20261103-193321-flarebots3-result): THE BURN WORKS: our player on a lit
  flare, god off, 18 -> 14 -> 10 -> 6 (4 a tick, means 14). The flare-bots (slots 8 and 10) "threw
  smoke" 21 and 19 times and spent nothing, never naming the flare (no flare trace): another smoke-kind
  of the same pool came first (mp_smokegrenade_special, 26, before 48).
- BotNadeWeapon names the signal flare first when he has it with rounds; SmokeToFlare also takes
  mp_smokegrenade_special; "bots: slot N throws smoke" names the weapon.

## 0.5.755: the burn takes health; the bots' flare throws traced

- 0.5.754's test (logs-backup/20261103-192357-flarebots2-result): the bots' SMOKE throws now go ("event
  56 for smokegrenade", 11 in 4 min). No bot's flare: the one traced flare-bot (slot 8) threw just as he
  spawned, before his smoke became the flare; his later ones were past the trace's 12. The burn
  touched our player ("flare: 7 burns player 0") but took nothing ("100 -> 100") with means 0.
- The burn's means is 14 (the mortar's: damage, no shell shock, per the hellfire's notes).
- The throw trace keeps 8 for flares, 4 for smokes.

## 0.5.754: bots' smoke and flare throws go

- 0.5.753's test (logs-backup/20261103-191503-flarebots1-result, 8 bots, COD3_BOT_SMOKE_EVERY=10000,
  every other bot with flares): the bots' kits with the scoped Mosin and the Webley were made (their
  world models looked up). But no bot's smoke-kind throw went, smoke (25) or flare alike: their first
  command named it, the rest their gun (weapon 1, 13), and the game took the gun back (+168 3 then 1,
  +164 the gun), nothing spent. Our own throws name the grenade for the whole throw.
- Now a bot's smoke-kind throw names it for 30 commands (his frags keep the one: they go).
- (Our player died to a bot before the test's camera move: the burn is still to be seen.)

## 0.5.753: World at War's flare sounds, its burn, bots' kits with it, the Mosin and the Webley

- Its own sounds: World at War's ignition (flare_exp: flare_00..03, a pop and its hiss, 2.6-4.5 s) and
  burning trail (flashbang_asplode_trail, 11 s) as it lights, in place of the M2's. They are xWMA (WMA v2
  in 2230-byte packets): FFmpeg's wmav2 misread every packet at the header's rate (12000 bytes a
  second: its superframes' offsets are read with the rate's width); at 48 kbit/s each decodes to its own
  length (the file's dpds count, less the decoder's first 2048). wawimport.xwma_to_pcm picks the rate
  that matches; make_flare.py writes flare_00..03.wav and flare_burn.wav.
- The user: "slight burning damage if it comes into contact or if someone steps on it". A burning flare
  (lit, not burnt out) burns a living player whose feet are within 28 across and 40 up or down: 4 every
  0.5 s, the thrower's (the player nearest the grenade as it left the hand), his own too, his side
  spared; the game's G_Damage, means 0, on the host. COD3_FLARE_BURN=n (0: none). (World at War's does
  none: this is ours.)
- Bots' kits: "flare" in any optional column of bot_kits.txt makes the kit's smokes flares
  (BotsKitFlare). New default lines (and added to the install's bot_kits.txt, the user's kept, a copy in
  logs-backup/bot_kits.before-753.txt): a scout with the scoped Mosin and flares, a rifleman with the
  Webley as his second and flares, a light with flares.
- (752's diagnostic kept: bots' flare throws are still to be made to work.)

## 0.5.752: why bots cannot throw the flare (diagnostic)

- The user: flares for the bots too (and the Webley and the scoped Mosin). Bots given the flare
  (COD3_BOT_FLARES) named it in their throws and spent nothing (0.5.740-741), where our own throws of it
  work.
- DIAGNOSTIC: a bot's smoke-kind throw (smoke or flare) followed 40 commands ("bots: smoke-kind throw":
  what he sends, his +164/+168/+172/+240, the smoke pools). COD3_BOT_SMOKE_EVERY=ms makes bots throw their
  smoke that often (tests); COD3_BOT_FLARES=2 gives the flare to every other bot (by his handle), to set
  flare throws beside smoke throws in one run.

## 0.5.751: a flare lying in a hollow still shines

- Tests (no build after): flaresmoke6 (the shed: the flares still "hidden": they come to rest inside a
  thin post, WaW brush 3142-3176 1890-1895 up to 236, under a beam at z 32-44: that corner, not the
  check); flaresmoke7 (logs-backup/20261103-184832, thrown west onto open ground from 2800 1929): never
  hidden, glowing all their burn about 800 units off, no smoke.
- First person, at 10 screenshots a second (flarehand5, logs-backup/20261103-185257): through the whole
  throw CoD3 draws no grenade on the picture (the gun drops, a hand's edge at the bottom for a frame,
  then the flare in flight), for the smoke and the frag alike. Nothing to swap there (and its model,
  BOX_SOMTHING, is the frag's too): left as it is.

- 0.5.750's test (logs-backup/20261103-183505-flaresmoke5-result): the same: each flare "hidden" from
  our eye as it rolled into the train shed's track bed (it rests at z 3, the floor about it 11.7), a
  rail's lip in the line from eye height, 226 units off.
- Now seen if the line to its head is clear, or the line to 20 or to 40 over it stops within its last
  tenth (its light spills over a lip). World at War's sight check would hide it there; its light is
  still seen over the lip.

## 0.5.750: a flare lying on the floor is still seen

- 0.5.749's test (logs-backup/20261103-182703-flaresmoke4-result, three throws): NO smoke; each
  grenade sunk (its place at +32, +112 and +336 of its entity) and a stick laid where it rested. But as
  each landed (at the floor's height, z 3) it went dark and the blinding let go ("sight back") with the
  view unmoved: the sight trace to its head (5 up) met the floor's edge.
- Now it is seen if the line to its head or to 20 over it is clear.

## 0.5.749: no smoke from a thrown flare: its grenade goes, its stick stays

- 0.5.748's log (logs-backup/20261103-181522-flaresmoke2-result): after the thrown model (+1932) the
  smoke's projectile words: +1924 940 and +1928 120 (World at War's flare's speeds too), +1936 1 (the
  frag's 0), then nothing. Test flaresmoke3 with +1936 cleared: still smoke, starting about 5 s after
  each throw (the grenade's own fuse), with no event our hooks see.
- So a flare's grenade, once it comes to rest (about 1.5 s after the throw), is sunk 30000 under the
  map (every copy of its place in its entity: its origin and its trajectory's base; the log names
  them), and World at War's flare stick lies in its place as a script_model (w_signal_flare_WORLD,
  turned as the grenade lay; six a map, used again in turn). Its smoke is made where nobody sees it.
  COD3_FLARE_KEEP_GRENADE=1: as before.
- The 747-748 diagnostics taken out.

## 0.5.748: the flare's smoke looked for, closer (diagnostic)

- 0.5.747's log (logs-backup/20261103-180819-flaresmoke1-result): no effect's name in the flare's
  weapon (its names: models, animations, icons, "smokegrenade" clip and ammo, "smokegrenades" at
  +1480). 746's smoke was ours (no bot threw one; no smoke event came for the flare): the projectile's
  own trail, it seems, played as long as it lies.
- DIAGNOSTIC: the words +1900..+2116 (after the thrown model, +1932) logged with the smoke's and the
  frag's beside them; COD3_FLARE_CLEAR="1936 1940" zeroes those words of the flare's (tests).

## 0.5.747: the flare's smoke looked for (diagnostic)

- 0.5.746's test (logs-backup/20261103-175925-flarehand4-result, five throws): each flare flew from the
  hand, lit 1 s after (about 230 units down the shed, "came to rest" on the floor) and blinded at 0.43,
  World at War's 1 - 230/400. But thick white smoke grew where they lay, throw after throw: the smoke
  grenade's own cloud, with no event of its own.
- DIAGNOSTIC: every word of the flare's weapon that leads to a name (directly or through a pointer) is
  logged at its making, to find the smoke's effect.

## 0.5.746: the thrown flare flies, then lights where it lands

- 0.5.745's test (logs-backup/20261103-174847-flarehand3-result, five quick throws): every flare lit 10
  units from our eye, at once: the grenade's event 56 is raised as it leaves the hand (a smoke grenade
  "goes off" as thrown and smokes where it lies), its entity (r4) then at the hand. The HUD now says
  "SIGNAL FLARE".
- Now the flare follows its grenade's entity (+336, FlareSight's FollowFlares) until it lies still for
  0.5 s (4 s at most; dropped if the entity jumps), and lights after World at War's fuse (fuseTime 1):
  in flight a small core and its spitting sparks, then the glow, the M2's ignition sound and the
  blinding, where it is. Flare::Ignite(at, fuse) returns its id, Flare::MoveTo(id, at).
- 745's "the smoke is event 10" was wrong: no event 8 or 10 ever came for the flare (only 19, 56 and the
  animation sounds); the grey smoke in 744's pictures was the two bots' own smokes. Taken out again.

## 0.5.745: the thrown flare without smoke, its name

- 0.5.744's test (logs-backup/20261103-173952-flarehand2-result): the left bumper threw it, our own
  throws named the flare (weapon 0x30), and each LIT ("flare: lit", blinded 0.97: held 1.5 s, it
  cooked off in the hand) - the thrown flare's light now seen. But the smoke's grey cloud rose with it,
  and the HUD said "WEAPON_SIGNAL_FLARE".
- The cloud is the smoke's event 10 (sub_825563D0, from 8246A5D8, the same moment as event 56): none for
  the flare. Its name is the text "SIGNAL FLARE" (a key the game has not is shown as it is).
- The test's throw is a 0.3 s press (COD3_FLARE_THROW_HOLD=s), to see the arm throw it.
- The in-hand grenade while cooking sits under the picture (only the hand's edge shows).

## 0.5.744: a test can throw the flare

- For the first-person look (the user: "Do first person next"): which model CoD3 holds in the hand
  for the smoke is not known (its weapon's view model is BOX_SOMTHING, the frag's pineapple, pin and
  spoon; the Mills bomb's notes say the hand holds the world model). A throw has to be seen: the
  console's +smokegrenade does nothing and COD3_PAD's presses keep a test from joining a side
  (flarehand1 sat on SIDE SELECTION). 740's flare throw was a pad's 0x0100, the left bumper.
- COD3_FLARE_THROW_TEST="20 40": the left bumper held 1.5 s that many seconds after the view is first
  drawn (Input::PressButton each frame).

## 0.5.743: the flare stick in its own colours

- 0.5.742's tests (logs-backup/20261103-170315-flaremodel1 to -171952-flaremodel5, Seelow's train shed,
  COD3_FLARE_MODEL_TEST="3100 1929 16"): the stick made and drawn at its size beside the smoke can, but
  white (the can's texture): the can's draws have three textures, in slots 2 (256x256), 3 (64x64) and 1
  (128x128), none in 4 or the lowest the 742 default chose. With the colour map in 2 and a flat normal
  and grey in 1 and 3 it drew grey; with the colour map in all three (flaremodel5, screen 172201) World
  at War's red flare paper, its label and wooden grip.
- Now the colour map in slot 2 by default, and custom\flare\world_slots.txt "1=flare_world_c.png
  3=flare_world_c.png" (make_flare.py writes it).
- First person: the smoke's in-hand grenade is the shared view model BOX_SOMTHING (the frag's too), not
  the world model: still the game's grenade there.

## 0.5.742: the signal flare looks like World at War's flare stick

- The user: "Do the model next". The flare's world model (in the thrower's hand, thrown and lying) is
  its own, w_signal_flare_WORLD: looked up as the American smoke's (w_us_smoke_grenade_WORLD) and given a
  copy of it whose body draws World at War's weapon_mp_flare_stick (custom\flare\weapon_flare.mesh,
  flare_world_c.png). The can (weapusge.cod block 15: 2.5 across, 0.05 to 7.7 up from its origin,
  52-byte vertices) and the stick (1.9 across, 3.4 below its origin to 10.6 above) are both z up: drawn
  as World at War has it, not turned, scaled or moved. Its colour map in the draw's lowest slot (the
  can's slot is not known; COD3_FLARE_WORLD_SLOT=n), COD3_FLARE_NO_WORLD=1 keeps the can.
- The M2's world model code is now shared (SwapWorldModel with a WorldSwap: mesh, texture, slot, name);
  the M2's is made as before.
- Test: COD3_FLARE_MODEL_TEST="x y z" stands the flare's model there, the smoke's 20 along y beside it.

## 0.5.741: thrown flares light

- 0.5.740's test (logs-backup/20261103-161045-flare2-result, Seelow, 10 bots): every swap clean
  ("smoke taken, flare owned"), our own throws named the flare (the host's command, weapon 0x30) and
  the game threw it: 4 "event 56 for signal_flare", but none lit. The weapon's name in that event lies
  in the physical memory's view (r5 0xAA6110E4), and the hook asked for an address under 0xA0000000.
  Now any name.
- The state's +240 is flags, not the offhand (0x10 or 0x18 on every player): 740's write of the
  flare's number there is taken out. The slot that held the smoke still gets the flare.

## 0.5.740: the signal flare is the one thrown

- 0.5.739's test (logs-backup/20261103-160042-flare1-result, Seelow, 6 bots): the flare is weapon 48,
  sharing the American smoke's rounds (clip pool #20); our smoke and the bots' swapped for it; the test
  flare lit every 12 s, drawn with its sparks, blinding at 150 units (0.59, the orange-red wash) and
  letting go when the view turned away. But the bots threw SMOKE: BotNadeWeapon picked the first
  smoke-kind weapon by number with rounds (the smoke, 24, shares the flare's), owned or not.
- Now the bots' throw names only a grenade he owns (bots.cpp BotNadeWeapon).
- SmokeToFlare: the state's offhand (+240) and the weapon slot that held the smoke (slot 4, +1068: the
  only two places 739's log found the smoke's number) now hold the flare, so our smoke key throws it.
  Its log says whether the smoke is still owned.

## 0.5.739: World at War's signal flare (first build)

- The user: "i want the signal flare in cod 3 from world at war so we have more then just the smoke
  grenade". A custom class's new SPECIAL row (class editor; ini class_N_special): SMOKE or SIGNAL
  FLARE (WAW).
- The weapon: "signal_flare", a copy of the American smoke grenade (Guns, as the Mills bomb is of the
  frag): World at War's is a grenade of the smoke's offhand class with a 1 s fuse
  (weapons/mp/signal_flare_mp in the user's iw_14.iwd). Its model is still the smoke's.
- At one's spawn with it chosen (SmokeToFlare, after the kit): the flare given, the smoke's rounds put
  on it, the smoke taken (its bit in the 64-bit weapons mask, state +1056). The first three swaps log
  the state words that held the smoke's number (an offhand choice, if the smoke key still throws smoke).
- Where it goes off (its event 56, the smoke's cloud) no cloud: it lights there (flare.cpp). World at
  War's maps/mp/_flare.gsc: 8 s, 400 units, everyone whose sight reaches it (no side spared) has
  vision/flare.vision over the view at 1 - distance / 400, checked every 0.3 s, let go 0.45 s after
  looking away (0.65 s as it burns out). Here: a wash of its orange-red tint and the glow swelling
  round it; the flare's glow (fxt_light_flare3_128, its fx's own image), core and sparks drawn where
  it lies for 10 s (fx_flare_sky_white_10sec_mp) while a wall does not hide it (FlareSight: the M2's
  wall trace from our eye, every 0.3 s). Overlay::SetFlareHud draws it.
- Sounds: the flare's throw (flashbang_toss_b), the blinded ring and its fade (flareshock_eq,
  flareshock_fade_eq). Its own ignition and burning are xWMA, which the port's FFmpeg would not decode:
  the M2's ignition and far loop for now.
- tools/waw/make_flare.py writes custom\flare (textures, sounds, WaW's flare-stick meshes for later).
- Tests: COD3_FLARE_TEST="x y z" lights one there every 12 s; COD3_BOT_FLARES=1 gives every bot the
  flare for his smoke (bots throw it as their smoke: bots.cpp counts "flare" as smoke).
- Not yet: WaW's flare stick in the hand and on the ground (meshes made), its kill icon, its own
  ignition sound; others on other machines (a friend) see a smoke's place without a cloud.

## 0.5.738: Mantle assist is a setting, off for players (bots keep it)

- Settings (F11), Movement: "Mantle assist" (mantle_assist in the ini), off by default. Off, a player
  jumps and crouches through Seelow's windows and over its ledges himself (the user: "I would rather
  have to jump and crouch than have it auto do it"); on, walking at one of WaW's mantle spots lifts him
  over (0.5.729/0.5.736). Bots always have it (bots.cpp BotsIsBotRecord), so they still get through
  windows. The 28-unit step of 0.5.737 stays for everyone.
- Seelow / Seelow Night (tools only, installed): WaW's small clip out (kept as the rocks' collision),
  props' collision triangles fixed (sign), 4 collision files (the level's unzoned.cgb), ladders, drain
  pipe, a wall hole widened; at night every lamp and fire lit and kept to its room, the user's 16 lamps
  (seelow-work\my_lights.txt), moonlight +5%. See docs/CLAUDE-SESSION-2026-11-02-evening.md.

## 0.5.737: a 28-unit step on converted maps (Seelow's lumpy ground)

- mp_fixes.cpp: on maps with WaW mantle helpers (custom\mantle\<map>.txt: Seelow, Seelow Night) every
  step goes up to 28 units, not 18; the helpers still lift to their own height. The user's plays snagged
  on bank feet, rubble and trench lips of 19-46 units where the collision is WaW's own, point by point.
  Every CoD3 map (no mantle file) moves as before.
- Seelow (tools only, installed): foliage uv (wheat, hedges, pine) and upright grass cards; model names
  past slot 82 by placed boxes (sandbags not flower patches, destroyed Opel trucks not Panzers); shot-only
  floors made solid; brighter Seelow Night with WaW's lamp pools.

## 0.5.736: mantling climbs each WaW helper's own height (Seelow's trenches)

- mp_fixes.cpp: the mantle assist (PM_StepSlideMove, sub_824A87E0) stepped 62 at every helper; 51 of
  WaW Seelow's 292 helpers are taller (trench walls, high ledges: 63-127). It now steps the helper's top
  over the feet (+2), up to 130. From the user's play (0.5.735's "where:" lines): 15 s jumping in a trench
  at 2700-2850 1250-1640 whose walls' helpers are 63 and 65 high.

## 0.5.735: the player's position in the log once a second (Seelow's falls and stuck places)

- bots.cpp: LogHostWhere writes "where: x y z | velocity x y z" for the host's own player once a second of
  real time, in every game. FeedBots' "bots: host 0 record" line follows the commands' clock and stopped
  after ~80 s of the user's plays, so their falls under Seelow and stuck places could not be found.
- Seelow collision (tools only, reinstalled): WaW's clip bits no longer swapped (AI clip had blocked 206 of
  292 climb spots and windows); WaW glass out; prop collision budget 30000 (props in boxes 312 -> 127).

## 0.5.734: the photo tour can look one way (Seelow Night's orange dot in the sky)

- bots.cpp: COD3_PHOTO_SKY="pitch yaw" turns every photo tour view that way; with COD3_SHOTS_DRAWS=1 and
  COD3_FRAME_LOGS=1 each tour picture's frame has its draws pictured too. For the user's report of a
  small orange light high in Seelow Night's sky (at the worldspawn sun direction, now Longyard's -55 340).
- Seelow Night's fog (0.5.733's find, tools only): night_lightgrid.py --fogrec gives the night pack
  Longyard's fog record (400 10800 .06 .06 .1, strength .195): the tan haze was Rouen's fog colour.

## 0.5.733: timed test screenshots can picture their frame's draws (Seelow Night's haze)

- mp_fixes.cpp: COD3_SHOTS_DRAWS=1 (with COD3_SHOTS and COD3_FRAME_LOGS=1) logs each timed screenshot's
  frame and pictures every draw (screenshots\CoD3-<moment>-draws.bin), as the screenshot key does, to see
  which draw first paints Seelow Night's tan horizon unattended.
- Found on the way (test tree1, Longyard ly1): the trees' leaf program 1b8706673819b84e takes its light
  from c18, already Longyard's moonlight (.55 .63 .7) through night_lightgrid.py --sun, and the models'
  unlit lightmap c2lm2 is moonlit (35 42 61): the leaves are not tan themselves; the tan shows through
  their cut-out edges from behind. Longyard draws its trees with other programs.

## 0.5.732: the draw log's constant dump writes the vertex constants too (Seelow Night's haze)

- render_commands.cpp: COD3_DRAWLOG_PSDUMP=hash also writes that draw's 256 vertex constants (a VSDUMP
  line with the vertex program's hash). The world program's pixel constants hold no fog colour: c22 is
  the worldspawn suncolor (tests fog1-fog3), and changing it to Longyard's moonlight left the tan haze.
- tools/waw/night_lightgrid.py --sun (run by the night install): Seelow Night takes Longyard's worldspawn
  sun (suncolor .55 .63 .7, sunlight 0.18 and the rest), binary record and text. Not the haze (fog3).
- Skip tests (COD3_SKIPPS): 1b8706673819b84e = tree leaves, bce81994346669fe = tree trunks,
  9a14f83313a256a4 = the player's sleeve (draws 3-9 are the view model). None is the haze.

## 0.5.731: the vehicles' pack brings its own scene of vehicles; none made from nothing

- 0.5.730's Seelow test (seelow20tanks) crashed the map start making the first saved Panzer: the map had
  no vehicles of its own, so the host made one from nothing (sub_824427C0 walked its bad model parts).
  Test rouenA_merv: with Merville's whole archive beside Rouen, Merville's SCENE came too, its 10 vehicles
  became Rouen's own and one was moved to the saved place: no crash. The pack had dropped the scene.
- tools/kapf/vehicle_scene.py (run by vehicle_pack.sh): the pack keeps Merville's scene, made a scene of
  vehicles alone: 12 of its script origins (the vehicle's pointer slots exactly: the scene's pointers are
  chains) made vehicles from Merville's own records, 6 Panzer IV, 6 Sherman, 3 WC51, 3 Horch, 4 BMW; all
  in every game type (the tanks were team-only: pointed at the BMW's list); parked 60000 down (the host
  moves those a map wants); the other 18 class names silenced ('x' first: no Merville spawn, flag or
  trigger), its props, groups and effects off.
- mp_fixes.cpp: a saved vehicle with none of the map's of its kind left is skipped (logged), never made
  from nothing (never drivable; crashed Seelow).
- vehicles.txt: Seelow's 12 back (5 Panzer IV, 5 Sherman at WaW's tank places, a Kangaroo, a BMW).

## 0.5.730: the vehicles' pack (CoD3's vehicles on any map; Seelow first)

- tools/kapf/vehicle_pack.sh (repack.py): Merville's archive cut to its vehicles (Sherman, Panzer IV,
  Horch, WC51, BMW, the Harley's model, the mounted guns), their 138 textures, track/wheel/damage effects,
  shaders, skeletons, drive animations and Merville's game database (the VEHICLEFILE and
  VEHICLEPHYSICSFILE entries), 45 blocks -> 20: game\mp\mp_merv\mp_merv_vehicles.cod (game data, local).
- mp_fixes.cpp (MpFixesLoadVehicleArchive): a map without vehicles of its own gets the pack (in Merville's
  whole archive's place when the pack is there) when vehicles.txt has vehicles for it, or with
  vehicles_everywhere 1 as before. The Kangaroo's archive follows whichever was loaded.
- Seelow (data): the Rouen leftovers repacked out (tools/kapf/repack.py; seelow_install.sh): its level
  archive 18 -> 12 blocks (219 Rouen-only textures as 8x8 stand-ins), zn01 45 -> 39, zn02/zn03 4 -> 1
  (Rouen's hidden old cells, index, collision, prop batches out), the level manifest's per-archive bank
  numbers set to match: 7 -> 25 banks free. Rouen's trees moved 60000 down (past r_zfar).
- vehicles.txt: Seelow's 10 WaW tank places (5 Panzer IV where WaW has Panzers, 5 Shermans for its
  T-34s), a Kangaroo by the Allied start, a BMW by the Axis start. Vehicles made where the map has none of
  that kind were not drivable (0.5.71-0.5.73): next.

## 0.5.729: mantling on Seelow; the big test sends a lost devmap again

- Mantling (mp_fixes.cpp, PM_StepSlideMove sub_824A87E0): World at War lets players climb 19-60 unit
  ledges and low walls where its map has mantle helper brushes; CoD3 steps 18 at most, so on Seelow
  players and bots stuck at trench lips and walls. tools/waw/seelow_mantle.py writes WaW's own 292 helper
  volumes to custom\mantle\seelow.txt; a player whose feet are within 32 units of one (and up to 62 below
  its top) steps up to 62 units for that move (the step height 18.0 at 0x82682774, swapped for the call
  only). No jump needed (bots climb too). Maps with no file (every CoD3 map, Carentan) move as before.
  Log: "moves: N mantle volumes for seelow", "moves: mantle at x y z, up h" (first 40).
- Big test (bots.cpp): seelow14bots never loaded: "devmap seelow" was put on the title's command buffer,
  both "set" lines before it ran, the devmap did not (no file opened, no session, skipped after 2 min).
  The same install loaded twice minutes earlier, so not the dropped .dcgb. If no map archive is opened
  15 s after a devmap, it is sent again (every 15 s within the 2 minutes).

## 0.5.728: the scoped Mosin fires one gunshot sound

- The user: "when you fire the mosin nagant it makes two bullet sounds instead of just the one sniper
  shot". Their log (logs-backup/20261102-080904-userplay-seelow2, CoD3MP.previous.log): every shot played
  CoD2's nagant_sniper.wav twice, "mosin: CoD2's shot (event 1, yours)" and then "(event 3, yours)"
  ~250-300 ms later (the bolt), past the 200 ms once-a-shot window. The Kar98's own fire sound was already
  silenced. Now one shot's events play the sound once within 900 ms (the bolt-action Mosin cannot fire
  again sooner), yours and other players' alike (mp_fixes.cpp, CampaignEventName).
- Crosshairs: the user reported none on the Mosin and the flamethrower, then got them back themselves.
  Checked in passing (tests xhair2/11/23/24): Thompson, scoped Kar98 and flamethrower showed theirs on
  Isle; the Mosin's weapon definition differs from the scoped Kar98's only in names, models and icon
  (COD3_WEAPON_DIFF). No change.
- Seelow (data, not this build): drawn on Carentan's shader set (e_decal_simple wrote no depth: the
  see-through walls), WaW's own sky, WaW's player clip bit, its props' shared materials resolved (handles
  point at material records: the furniture's planks, the water tower's wood); see
  docs/CLAUDE-SESSION-2026-11-01-night.md.

## 0.5.727: the scoped Mosin's world model and kill icon

- 0.5.726's test (logs-backup/20261101-235249-mosin726-result): CoD2's Mosin drawn in the Kar98's place in
  first person (2 Kar98 bodies, 12 parts hidden), its atlas in slot 4 ("put in slot 4: yes"), the scope,
  wood and cloth as CoD2's.
- Its world model: w_mosin_scoped_WORLD, the scoped Kar98's (w_ge_SNIP_WORLD) looked up and copied with
  CoD2's weapon_mosinnagantscoped_cloth in its body (MosinWorldModel, made from FlameWorldModel by
  script), moved 11.5 ahead (CoD2's origin is mid-rifle, the Kar98's at the hand; COD3_MOSIN_WORLD_SHIFT).
- Its kill icon and HUD icon: CoD2's hud_scoped_nagant (custom\mosin), made from the scoped Kar98's
  (OwnIcon; COD3_MOSIN_KAR_ICON=1 keeps the Kar98's).
- Test kit: tools/cod2import/mosin_test.sh (every bot on the Mosin; PRIMARY=24 for the test player).

## 0.5.726: CoD2's scoped Mosin-Nagant (first build); others' flamethrowers heard only close

- 0.5.725's test (logs-backup/20261101-225513-flame725-result): the heat panel covers the ammunition box;
  no ammunition numbers while the M2 is held.
- Others' flamethrower sound (the user: "i hear alot of other bots or players flame throwers when ... not
  even near these players"): full within 200, ((1000 - d) / 800) squared, none past 1000 (3D; was a
  straight fade to 3000).
- **CoD2's scoped Mosin-Nagant** (the user's own CoD2; custom\mosin by tools/cod2import/make_mosin.py from
  cod2import.py's outputs in D:\Call of Duty 3 (USA, Europe)\mosin-work): viewmodel_mosinnagant_cloth
  posed by viewmodel_mosincloth_idle (the parked stripper clip left out), weapon_mosinnagantscoped_cloth,
  their three colour maps (weapon_mosin_c, weapon_mosin_cloth_c, mosin_cloth_frayed_c) in one 1024 x 512
  atlas (the game draws one colour map a mesh), nagant_sniper.wav, nagant_rld.wav, hud_scoped_nagant.
  - Guns[] "mosin_scoped", a copy of the scoped Kar98 (kar98_sniper: its scope, zoom, bolt, reload), last
    in the table. Custom class main weapon 24 / second 26 ("MOSIN-NAGANT (SCOPED)"), bot kits.
  - Its view (MpFixesMosinView): the Kar98's body (k98.w_ge_RFL_VM) drawn from our buffers while it is
    held; its bolt, clips and the scope model (bolt, k98_bolt_handle, k98_mag, k98_mag_first,
    k98_mag_stripper, stripper .w_ge_RFL_VM; scope.w_ge_RFL_SCOPE) draw nothing (CoD2's has its own).
    The fit: muzzle on muzzle, butt on butt, bore on bore (scale 0.672; COD3_MOSIN_FIT), colour map slot
    4 (COD3_MOSIN_SLOT: the Kar98 body's draw's slots are logged on its first draws).
  - Its shot is CoD2's (events 1, 3, 7, 13, 53, once a shot; others' fainter to 4000); the Kar98's fire
    sounds are not played for whoever holds it (sub_82554BA0).
  - Not yet: its world model (others see a scoped Kar98), its kill icon, CoD2's own bolt/reload moves.

## 0.5.725: the heat bar in the ammunition box's place, covering it

- 0.5.724's test (logs-backup/20261101-224823-flame724-result): the counter still shown: the widget's parts
  (their colours and place) are not what is drawn. Those edits are off (COD3_AMMO_WIDGET_TRY=1 keeps them).
- Instead the heat bar is a panel in the box's own look (dark teal, a thin gold edge) drawn over it while the
  M2 is held (measured at 0.798..0.891 x 0.872..0.916 of the picture), the heat inside it; always while
  held, heat or none.

## 0.5.724: the ammunition counter hidden for real with the M2

- 0.5.723's dump (logs-backup/20261101-224146-flame723b-result; 723's first run stayed in the menus): in
  these parts +4 is a float (1.0). The box (hudammo, an image: vtable 820705B4) has its place at +20/+24
  and two colours at +64/+68 (FFFFFFFF); the numbers (clipammo, totalammo: texts, 820702DC) their place
  at +44/+48 and colours at +92/+96 (RGBA 8FB1C9FF, 9BB8B8FF). While the M2 is held: the colours' alpha 0
  and the place 2000 lower; put back after.

## 0.5.723: the ammunition widget's parts logged (diagnostic)

- 0.5.722's test (logs-backup/20261101-223239-flame722-result): "hidden" logged, but the counter still
  shown: +4 is not these parts' visibility. The widget and its three parts' first 160 bytes logged three
  times with the M2 held and three without ("ammo widget"), to find what shows them.

## 0.5.722: no ammunition counter with the M2, only its heat bar

- The user: "the over heating bar worked but it still showed ammo ... remove the ammo counter and just keep
  the cooldown bar". The HUD's ammunition widget (RTTI .?AVIGOAmmoWidget@@, its virtual table 0x82070F64)
  finds its parts by name in its load (slot 1, sub_824F2A70: "hudammo" at +12, "clipammo" +16, "totalammo"
  +20, by FindElement 0x824EE230) and updates them each frame (slot 2, sub_824FAD20). While our M2 is held
  their visibility byte (+4) is 0, after that update and each frame; put back when it is put away.

## 0.5.721: the heat bar under the ammunition box

- 0.5.720's test (logs-backup/20261101-222112-flame720-result): overheated twice, the clip kept full (no
  reload), the bar yellow to red and flashing; the UAV oval 3113 x 1722 at 1913 over the players, at most
  7893 from the corners. But the bar lay over the ammunition box's numbers: now just under it.

## 0.5.720: World at War's overheating instead of a reload; others' flames fade with distance; Isle's UAV 7% higher

- Overheating (the user: "i think it used a cool down instead of an actual reload animation"): World at
  War's weapon file has unlimitedAmmo 1, overheatWeapon 1, overheatRate 38, cooldownRate 13,
  overheatEndVal 50. For every player holding the M2 (FlameHeatTick, each frame on the host): its clip
  filled again (never a reload), each shot since adding 3.8 heat (a shot a tenth of a second), cooling
  13 a second; at 100 it has overheated and fires no more (the fire bit off in his newest command before
  ClientThink, bots.cpp sub_82593518) until under 50. Our own heat as a bar at the bottom right (yellow
  to red, flashing while overheated). COD3_FLAME_RELOAD=1: the old 100-round tank.
- Others' flames, drips and what they leave (the user: "a little less visible from far but more visible
  the closer you get"): full within 300, then (1400 - distance) / 1100 squared, gone at 1400 (718: full
  to 1200, gone past 1800).
- Isle's UAV 7% higher again (the user: "at least another 7% higher"); the oval still fitted to the
  draw distance with its height counted.

## 0.5.719: Isle's UAV oval a little wider

- The user: the oval "seemed too tight to the map". Isle's extra factor 0.94 (was 0.85): 3113 x 1722
  (was 2815 x 1557); at most 7864 from the spawn box's corners, inside 92% of draw distance far (8280).
- 0.5.718's test (logs-backup/20261101-220905-flame718-result): the gun's mask drawn (4068 vertices,
  3806 triangles), the flame under the gun; the oval 2815 x 1558, at most 7586; no crash.

## 0.5.718: the gun over its flame; others' flames close only; 4% more damage; the UAV an oval within the draw distance

- 0.5.717's test (logs-backup/20261101-215521-flame717-result, 18 shots): the flame leaves the tube's
  mouth in every firing frame wherever the gun is drawn. The user: "its alot better now", but "the fire
  seems to be spraying from ontop of the guns model ... put the fire underneath it, and the gun above".
- The gun over its flame (overlay.cpp): the flame is drawn over the finished picture; now before it the
  M2's own mesh (its vertices as written, Overlay::SetWeaponMaskMesh) is put on the screen with its own
  draw's transforms this frame (Render::TrackedTransform: constants 16-18, then 12-15) into a stencil,
  and the weapon picture (ImGui's background list) is drawn where the gun is not (a depth-stencil
  override hook in thirdparty imgui_impl_dx11.cpp, g_ImGuiDX11DepthStencilOverride).
- Others' flames, their drips and what they leave are a close thing (the user: "i keep seeing intense
  flames and the drip effect" from across the map): full to 1200 units away, gone past 1800.
- Damage 26 a shot (was 25; the user: "5% stronger": 25 x 1.05 as a whole number).
- The UAV (the user: Isle's plane "was popping in and out of frame, it needs to stay visible at all
  times no matter where on the map someone is"): 0.5.711's Isle play log: the circle 3312 round over a
  spawn box of 8280 x 4581, and the user's draw distance far (r_zfar 9000): a player at one end was
  about 9300 from the far side. Now an oval the spawn box's shape, made smaller 8% a time (to a quarter
  at most) until no corner of the spawn box is farther from any point of it (its height counted) than
  92% of the draw distance; the oval's y over x goes to the other machines (the plane packet, read when
  long enough). All maps; Isle's and Carentan's sizes as before, then fitted.

## 0.5.717: the flame starts where the M2's own draw puts the tube's mouth

- The user (714, screenshots taken during that test): walking or strafing, the flame left the barrel
  and started beside it: the game moves the whole arms model in the view (Call of Duty 4's gun
  placement: sway, lag, bob), which the nozzle we worked out did not have.
- 0.5.716's dumps of the M2 view mesh's draw (custom/flamethrower/vsconst_1-3.bin): constants 16-18
  the mesh's rigid bone (into the world, the game's placement in it), 12-15 the view and projection
  (x scale 0.75 on 16:9: the arms are drawn with 90 degrees for 4:3, the camera's +96, not 80), 0-3
  the view, 8-11 the projection. The nozzle (World at War's tag_flash, in the mesh as the fit puts
  it: -0.013 -19.254 0.979) through them: (0.563, 0.638) and (0.562, 0.640) on the picture.
- Render::TrackMeshPoint / TrackedPoint: each draw of the M2 puts the nozzle on the screen with its
  own constants; the flame starts there (before: from World at War's pose, or a fixed point).
- The pose's squeeze takes the camera's fov (90: tan 32.5 / tan 45 = 0.637), not 80.
- The constants probe (COD3_FLAME_VSDUMP) matches the buffer by its GPU address (0x28000000 lower in
  the extended window: 0.5.714 and 715 wrote nothing).

## 0.5.716: the constants probe by the GPU's address of the buffer

- 0.5.715 wrote nothing either: in the extended window the GPU's address is 0x28000000 lower (as
  render_prompts.cpp's MeshTexKey), not a masked alias.

## 0.5.715: the constants probe by the buffer's physical alias

- 0.5.714 wrote nothing: the draw reads the M2's buffer by its physical alias (46190000 for 6E190000).

## 0.5.714: the M2 view mesh's vertex constants (diagnostic)

- 0.5.713's test (logs-backup/20261101-212817-flame713-result): no placement matrix near the eye anywhere
  in the cgame area (the arms are likely placed relative to the eye). The exact way: COD3_FLAME_VSDUMP=1
  writes the vertex program's 256 constants of the M2 view mesh's draw to
  custom/flamethrower/vsconst_<n>.bin, three times 8 s apart (Render::ProbeVertexBuffer), to find the
  constants that put the mesh, and so its nozzle, on the screen.

## 0.5.713: the gun placement search widened (diagnostic)

- 0.5.712's test (logs-backup/20261101-212205-flame712-result): no rotation matrix within 40 of the eye
  in the two blocks. Now both layouts (3 x 4 floats; 4 rows of 16 bytes, the place at +48), within 80,
  over the whole cgame area 0x82C70000..0x82C90000.

## 0.5.712: Isle's UAV higher and tighter; finding where the game puts the arms in the view (diagnostic)

- Isle's UAV (the user: "higher by another 14 %", "tighter by at least 15%"): the circle 85% of
  0.5.711's (68% of the spawns' half-width, at least 1200), 14% higher again above the players.
- The flame still leaves the tube's end when strafing (the user, 711). CoD3 has Call of Duty 4's
  gun placement (cg_gun_rot_*, cg_gun_move_*, cg_bobWeapon* in the executable's strings): the arms
  model is put in the view each frame by a matrix of its own (sway, lag, bob), which the nozzle
  did not have (it took the model's space for the view's). DIAGNOSTIC: while the M2 is held,
  every 3 s (20 times), each rotation matrix within 40 of the eye in the local client's cgame
  block (0x82C7D490, 6368 bytes) and view-weapon block (0x82C7A270, 0xC88) is logged
  ("flamethrower placement:") with its place and forward against the view's.

## 0.5.711: World at War's grip; the arms and flame sway with the view; others' flames not through walls

- 0.5.710's test (logs-backup/20261101-210018-flame710-result): World at War's pose, the tube's
  mouth on its nozzle spot; but the left hand kept CoD3's Thompson grip (the tube across an open
  palm). The user: "still not holding it properly", "when sway left to right the flame doesnt
  stick to the model of the tube", "i can see other peoples flames through walls ... from far
  away".
- Grip (tools/waw/make_flame_arms.py): each hand turned by World at War's hand's shape (the wrist
  to the middle knuckle, the index knuckle to the little finger's) and placed by its middle
  knuckle on World at War's; each finger joint (thumb, index, middle, ring, little: 15 a hand)
  along World at War's matching bone against the palm. At idle CoD3's joints land 0.00-0.32
  units from World at War's (scaled); the wrists 0.5 (CoD3's hand is 25% longer).
- Sway: the game sways and bobs the arms by their pelvis: the table (now version 2, with the
  pelvis at rest) goes under the pelvis's move from its rest each frame, and so does the nozzle.
- Others' flames: seen only with their gun in sight, or both the middle and the end of the flame
  (the middle alone let flames show over walls); their fire, drips and smoke follow the flame's
  sight (they had none).
- The UAV's tighter, 12% higher circle on Isle too (the user: "the uav was supposed to be for
  the map we keep testing on"; kept on Carentan).

## 0.5.710: World at War's flamethrower pose in the right place

- 0.5.709's test (logs-backup/20261101-205310-flame709-result): World at War's animations played
  on the arms, no crash, but the nozzle was at 11.1 -2.5 -12.0 (World at War's: 13.3 -2.3 -2.7
  squeezed): the pose went under the game's tag_camera, which in play is about 9 below the eye
  (the dump's was the identity), and the squeeze took the camera's +96 fov (90) while the arms are
  drawn with 80 (measured): 0.637 instead of 0.759.
- Now the arms' model space is the view's (the table straight in), the squeeze and the flame's
  nozzle use 80 (COD3_FLAME_VMFOV to try another): the nozzle at World at War's (0.577, 0.662).
  The game's tag_camera is logged once.

## 0.5.709: World at War's own flamethrower animations on the arms; the flame from its nozzle; Carentan's UAV circle

- World at War's animations read (tools/waw/xanim.py): Call of Duty 4's XAnimParts layout (framerate
  at +0x28, boneCount[10] by part type); every stream used up exactly and all 745 rotations unit
  length over the M2's idle, fire, pullout and putaway. tools/waw/wawpose.py poses World at War's
  first-person rig (viewmodel_usa_marine_arms from nazi_zombie_prototype.ff: it is in no MP zone;
  the M2's bones hung from tag_weapon); arm lengths come out the model's own every frame. A bone an
  animation names with no keys keeps its model's rest (tag_flash 17.7 ahead of j_gun).
- Why our flame looked wrong: World at War holds the M2 low and right (tag_flash 33 ahead, 4.3
  right, 5.1 down: (0.576, 0.661) on a 16:9 picture at its 65 degrees); the Thompson's pose put the
  tube's mouth near the middle (0.525, 0.577), so the stream went straight away from the eye.
- tools/waw/make_flame_arms.py: custom/flamethrower/flame_arms.bin, World at War's idle, fire,
  pullout and putaway on CoD3's arms (camera space, World at War's view scaled to the M2's fit
  0.532): tag_weapon puts the fitted M2 exactly on World at War's j_gun (tools/waw/flamefit.py);
  the hands keep CoD3's grip on the M2 and follow World at War's wrists; two-bone elbows toward
  its elbows; fingers and twists with them; the Thompson gun's 8 bones at rest (pose dumps of
  0.5.708 holding the M2, logs-backup/20261101-203934-flamedump-result).
- In game (PlayFlameArms, sub_82186740): while the M2 is held the arms take the table under the
  game's tag_camera, squeezed along the view by tan(32.5)/tan(fov/2) (CoD3 draws the arms with its
  world fov, about 94-96 across measured; World at War with 65): seen as World at War shows them.
  Raise = World at War's pullout in CoD3's 0.6 s; idle; fire while it burns (0.08 s blends). The
  Thompson gun's bones held (its fire/reload moved the M2). COD3_FLAME_NO_ARMS=1: the Thompson's.
- The flame starts at World at War's tag_flash as the animation has it each frame (Flame::Nozzle,
  projected with the view's fov), no longer a fixed point on the picture.
- Carentan's UAV plane (the user: "needs a tighter circle as it flys far outside the map
  boundaries", "raise the height of the plane by 12%"): the 2500 floor held it past the map's edge;
  now 80% of the spawn box's half-width (at least 1200) and 12% higher above the players.
- Not yet: sprint, ADS and melee show the idle pose (World at War's have no sprint animation of
  their own: offsets); overheating instead of the reload; the grey puffs where it hits.

## 0.5.708: our flame's start point moved onto the tube's mouth

- Measured on 0.5.707's full-size screenshot (logs-backup/20261101-195853-flame707own2-result):
  the ring at the tube's mouth is at 0.525 0.577 of the picture; 0.5.705's point (0.513 0.58) sat a
  little left of it, on the tube. Now 0.525 0.577 (COD3_FLAME_MUZZLE still overrides).
- Open: the point is fixed on the picture (the gun's bob and sway move the tube around it), and
  COD3_FLAME_TEST=1 drew no flame of ours in 707's test (not yet understood): our flame is only
  seen in screenshots when the test player really fires.

## 0.5.707: our flame starts at the tube's end, nothing drawn back over the gun

- 0.5.706's own-flame test (COD3_FLAME_TEST=1, logs-backup/20261101-194911-flame706own-result):
  the flame sat on the tip but was a ball over it: the ribbon's pieces (1.6 x the stream's size,
  centred on it, drawn twice as tall as wide) and the far chunks (300 pixels tall over the
  crosshair) hung back over the tube and the hand. The user: "still feel like the fire is coming
  out of the barrel and not the end of the tip of the tube itself".
- The ribbon is a cone from the tube's end: the nozzle's width (0.012 of the picture) at the tip,
  widening with its distance from the tip on the picture, never more than the stream's true size;
  pieces 1.2 tall (were 2). Our chunks are 0.55 of their size, 1.3 tall.
- Nothing of our flame reaches below the tip's height but a nozzle's width (AboveTip).
- 0.5.706's bot test (logs-backup/20261101-194633-flame706-result): others' flames are now one
  continuous flame across the field; 106 fps.

## 0.5.706: your own flame leaves the nozzle's tip as a narrow jet

- The user: our flame "comes out of the top portion of the gun near my hand and not the actual end
  tip". Measured in 704's screenshots: the start point was on the tip (0.515 0.565 at rest; firing
  0.508-0.517, 0.585-0.59), but the first chunks were drawn 70-100 pixels across right at the
  nozzle (0.8 units at depth 18), a ball of fire over the hand.
- Our flame now leaves the tip as World at War's first person ribbon (flame_firstperson_mp:
  streamFlame 2.5 to 15 units across over 246.7, looping in 0.41 s; streamFuel core 4 to 12 over
  200, 0.15 s): the nozzle's width (0.012 of the picture) at the tip, the stream's true size from
  50 units out, toward the crosshair, drooping, stopped by a wall. Chunks only past 0.7 of it.
- The nozzle point is the tip while firing: 0.513 0.58 (COD3_FLAME_MUZZLE still overrides). The
  pilot lick at the tip is the nozzle's size (was 0.05 of the height).
- Not yet: the point follows the gun's bob and sway (it is fixed on the screen).

## 0.5.705: others' flamethrowers burn as one flame, World at War's ribbon

- 0.5.704's self-test (logs-backup/20261101-193220-flame704-result): 23 of 23 bullet hit records
  cleared and still empty all match (no hits, no holes); others' flames seen; 114 fps; no crash.
  But up close another's stream was a chain of separate fireballs.
- World at War draws a continuous flame from the gun (flame_thirdperson_mp: streamFlame 2.5 units
  across at the gun to 35, 246.7 long, its animation looping in 0.41 s; streamFuel, a brighter core,
  4 to 12 across, 200 long, 0.15 s). Now drawn the same: 40 overlapping fire frames along the
  stream (closer together at the gun), drooping with its gravity (250 at 1000 units a second),
  stopped by the first wall, with the lick frames as its core. The flying fire chunks show only
  past 0.7 of the ribbon's length (fading in), where WaW's stream breaks up.

## 0.5.704: the flamethrower's bullet hits removed the right way

- 0.5.703's self-test (logs-backup/20261101-192400-flame703-result, mp_isle dm, 8 bots all on the
  flamethrower): others' flames are seen (in the wheat, toward the camera, hidden behind walls),
  ours lands on the hay bales with fire and drips; 105 fps average (0.5.687's same test: 101).
  But the ammo type was the wrong switch: the game logged the Thompson's as 0, not "smg".
- A weapon's bullet hits are its own 23 records at +2284 (one a surface: effect and mark, see
  WeaponMarks, 0.5.93): the flamethrower's (the Thompson's, copied) are cleared now, logged as
  "bullet hit records cleared". The ammo type is left as the Thompson's.
- The "GPU is hung" lines at start-up in 703's log are not new (0.5.641, 0.5.647 and the Webley play
  have them too); the game went on.

## 0.5.703: the flamethrower shoots fire, not bullets; others' flames seen; fire left where it lands

- (Replaced in 0.5.704: the ammo type is not the switch.) Fire, not bullets: its ammo type is now the type list's "none" (15; the list at 0x825D7880:
  spread, smg, pistol, rifle, lmg, hmg, umg, grenade, smoke, rocket, molotov, artillery, mortar,
  tank, b17, none). A bullet's hit is the game database's bullethit_<ammo type>_<surface> (sparks,
  dust, blood, their sounds), which only rifle/pistol/smg/umg/hmg/lmg have: no bullet hits now.
  Its shot events 1 (muzzle flash), 2 and 8 (brass and the rest), 3/7/53 (shot sound) keep its own
  name and find nothing: no Thompson flash or brass. COD3_FLAME_BULLET_FX=1 keeps the old way.
- Others' flames are drawn (they were only heard): a stream of World at War's fire chunks in the
  world, from the shooter's gun (eye, 12 down, 30 ahead, 7 right) along his aim, projected with the
  view the game draws (its view block 0x82C7D490: origin +4960, axes +4976; the camera's fov +96),
  hidden while a wall stands between our eye and both his gun and the middle of his flame.
- Every flame stops at the first wall in its way (a trace each 60 ms a shooter) and spreads there.
- What a flame leaves, with World at War's own numbers (flameconfigs/flame_firstperson_mp and
  _thirdperson_mp in common_mp.ff): fire burning on where it lands (1.3 s, rising), drips falling
  from the stream (0.85 s), smoke rising and spreading (1.55 s; faint, 0.35, for our own as in WaW).
  WaW's burn on players (5 s, a hit each second) is NOT in yet.
- The 0.5.702 "player still" diagnostic is gone (the user: no freezes, Carentan "perfect now").
- WaW's weapon file (iw_14.iwd weapons/mp/m2_flamethrower_mp) for the next steps: weaponType gas
  (CoD3 has a "gas" weapon type too: 20 places test it, e.g. sub_8248E100 gives it its own weapon
  animations 21/22), unlimited ammo with overheating (38 up, 13 down, usable again at 50).

## 0.5.702: why the player freezes (diagnostic); a bench camera that aims at a point

- The user's Carentan play (logs-backup/user-play-2026-11-01-carentanN): "frozen in place" twice at
  -689 1601 0 (an open grass alley; nothing solid there in the converted collision, bots walk it).
  The local player standing still for 2 s while alive is now logged ("player still") with what
  his body meets 8 units each way and 2 up/down (start solid, fraction, handle, normal, surface).
- COD3_BENCH_LOOKAT=x,y,z,distance: the bench view never held the yaw it was given, so the camera
  now follows the view, that distance back from the point, the point in the middle of the picture.
- Data after the build (Carentan set P, tools only): blended decals (phong_blend: SOZE, LYON, CAMOT, posters) on
  e_decal and drawn after the solid surfaces of their cell (they were black: blended over nothing before
  the wall was drawn); additive decals (phong_add, near-black lightstain_04) left out (a black facade);
  Rouen's grass (level.bgib) left out of the level's directory (tools/carentan/scene.py drop). The user:
  "the walls look perfect now".
- Data (no build): Carentan's view going white (19k triangles drawn of 409k, seconds at a time,
  on roofs at 1404 2494 375 / 1657 2184 154): the camera's cell comes from Rouen's BSP tree
  (mp_rouen.btrb: 519 leaves, 141 "no cell", 74 in cell 000, 45 in 007, 12 in room 006); every
  leaf now in cell 001, Carentan's one cell (tools/carentan/scene.py onecell): no drop at all
  in the same test (493-516k every second).

## 0.5.701: bots leave dead-end rooms; Carentan without Rouen's collision

- 0.5.700's test (logs-backup/20261101-170825-carentan-stairs700, 10 bots CTF 240 s): no bot
  stuck on the cellar stairs any more (1 line; 85 in 0.5.690's match), but one walked down into
  the cellar (a dead end, 512 x 408) and paced it for 3 minutes. NetProgress
  (bot_unstick.h) now also counts "still within 450 of where he was 30 s ago, goal far" as the
  fourth strike at once, so the unstuck (along his learnt route) follows.
- THE USER'S INVISIBLE WALLS: Rouen's collision was still loaded under Carentan. The Carentan
  pack's zone archives kept Rouen's zone collision renamed (xp_rouen_zoneNN.cgb), and the loader
  walks the archive directory by type whatever the name. Bots' traces stopped on Rouen's
  floors to the unit (z 16.0 at 432 853 and 897 571, 16.96 at 607 689, where CoD2's ground is
  -8, 8, -3.6). tools/kapf/merge_zone.py now leaves the source's collision out of the directory
  (data only: the pack's zones rebuilt; tools/carentan/ghost_hits.py checks bots' hits
  against CoD2's collision).

## 0.5.700: bots walk steep stairs under a ceiling (Carentan's cellar)

- 0.5.699's tests (logs-backup/20261101-1644..1649): Carentan's lightmaps now CoD2's (byte 0,
  set cellsM); the world program's real constants: ambient = 1 + 0.2 x normal up (walls 1.0),
  sun colour 0.99 0.93 0.81: the engine's, not a map setting.
- Carentan's worst bot trap (carentan-ctf-play690: one bot 13 strikes on the cellar stairs at
  462 1809, the unstuck put him back on them): stairs 6 up per 8 under the ground floor. Climb
  (bot_navigation.h) raised the body a stair plus the piece (34) before testing the way across,
  and under the floor above that raised body hit it: no way up or down the stairs was "walkable",
  so the detour, the short escape and the wander all failed and he stood still. Each piece is
  now tried at one stair's lift (18, a player's step) first, the whole rise only if that is
  blocked. Detour's traces 96 (was 64) and the short escape's 40 (was 24): a piece may now
  take two tries.

## 0.5.699: the world program's constants, read the right way (diagnostic)

- 0.5.698's test (logs-backup/20261101-163130-carentan-prog698): the world programs found.
  The simplest (ps 1b8706673819b84e) draws colour x (ambient x lightmap.rgb + sun colour x
  shadow x lightmap.a x N.L + the vertex program's dynamic lights), ambient = (c1.x + normal
  up) x c5.x + c4.y. Its constants were logged from the wrong place: a program's c[i] is the
  file's constant map[i]. Now logged by the program's constant map.
- Carentan's lightmaps (tools, not the build): CoD2 lights an unbumped surface with the
  first of each colour's four direction coefficients alone (its own lib/lightmap.hlsl and the
  $lightmapweights builder in gfx_d3d_mp_x86_s.dll); the conversion averaged all four, about
  20% darker. tools/cod2import/cod2cells.py now takes the first (C2_LM_AVG=1: the old way).

## 0.5.698: which program draws Carentan's walls (diagnostic)

- 0.5.697's test (logs-backup/20261101-161520-icons697): both own icons in the kill feed (the
  Webley's revolver, the flamethrower's M2), each picture given to the texture fetched.
- Carentan's interiors are dim (carentan-interior697: mean grey ~25 of 255 inside, lit by the
  lightmap's colour alone). The log now names the programs that sample a 512x512 8888 texture
  (our converted lightmaps; Rouen's are DXT5), the textures they fetch and their pixel constants
  c0-c15, once each, to read how CoD3 scales a lightmap.

## 0.5.697: the flamethrower's kill-feed icon made where the kill feed asks for it

- 0.5.696's test (logs-backup/20261101-160808-icons696, bots on the Webley and the
  flamethrower, mp_isle dm): THE WEBLEY'S OWN ICON (CoD2's revolver) IS IN THE KILL FEED; the
  log shows its picture given to the texture the kill feed fetches. The flamethrower's kills
  showed a white box: its icon was never asked for through the image look-up that made the
  Webley's. The kill feed's name look-up (sub_82477E70) now makes a missing own icon itself,
  from its source's image found in the same scope (hud_thompson for the flamethrower).

## 0.5.696: the Webley's and the flamethrower's own kill-feed icons

- 0.5.695's log (logs-backup/20261101-155953-webley-icon695): the copy's texture IS fetched,
  but as 06050000: the game cuts a texture's address to 29 bits, so the texels placed in the
  extended window (46050000) were read from someone else's memory (nothing drawn). The icon's
  texels now come from the title's physical memory (Guest::AllocatePhysical), whose addresses
  survive that cut, so the placed picture (custom\webley\hud_webley.png) is the one drawn.
- The icon work is now a table (OwnIcon): the flamethrower gets World at War's
  hud_m2_flamethrower (custom\flamethrower\hud_m2_flamethrower.png) the same way, made from the
  Thompson's icon (its name read from the gun). COD3_FLAME_THOMPSON_ICON=1 keeps the Thompson's.

## 0.5.695: Webley kill-feed icon, what the draw fetches (diagnostic)

- 0.5.694's test (logs-backup/20261101-155050-webley-icon694, bots all on the Webley): the
  Webley's kill lines now show NO icon (0.5.693: the Colt's), so the draw goes through the copy
  now, but the picture placed for the copy's texture was never uploaded. The words moved
  (mask 20200020: +0x14, +0x54, +0x74) show the image object in memory is not laid out like the
  TEX file's description. This build logs the Colt's image words and every fetch of the placed
  picture's texture (also with its address cut to 29 bits).

## 0.5.694: the Webley's kill-feed icon copy binds its own texture

- The Colt's image object (a TEX description, tools/kapf/xetex.py) keeps a pointer at +0x14 to
  its own D3D texture header at +0x2C; the draw binds through that pointer. The copy made for
  hud_webley still pointed at the Colt's header, so the Colt's texels were drawn (no upload at
  the copy's own place was ever logged). Words of the image pointing into itself now point into
  the copy, so its own fetch constant (CoD2's hud_webley picture) is the one drawn.

## 0.5.693: the Webley's kill-feed icon look-up answered (picture still the Colt's)

- Tested (logs-backup/20261101-131109-webley-icon693, bots all on the Webley): no white box
  any more, but the icon drawn is still the Colt's picture: the copy is used, its texture is
  not CoD2's yet (see docs/CLAUDE-SESSION-2026-11-01.md, "Webley kill-feed icon").

- 0.5.692's test (logs-backup/20261101-130413-webley-iconname692b, bots all on the Webley,
  COD3_TEXLOOKUP_PREFIX=hud_webley): the kill feed and the HUD look the icon's name up
  through sub_82477E70 about 100 times a match (from 824409C0, scope 0, and 82440A18, scope
  0xC); no archive has hud_webley, so 0 came back: 0.5.680's white box. For the Colt the same
  call returns the image object (hud_colt45 -> 5E5B41E4) that MakeWebleyIcon copies (since
  0.5.677) and whose texture draws CoD2's picture. Now a 0 for hud_webley there is answered
  with that copy.
- So the Webley's own icon (names +1860/+1440 hud_webley, its HUD slot's copy) is on by
  default; COD3_WEBLEY_COLT_ICON=1 keeps the Colt's. (COD3_WEBLEY_OWN_ICON and
  COD3_WEBLEY_ICON_SLOT are no longer needed.)

## 0.5.692: the texture-name diagnostic takes a prefix (Webley icon hunt)

- COD3_TEXLOOKUP=1 with COD3_TEXLOOKUP_PREFIX=hud logs the texture look-ups by name
  (sub_82477E70) of names starting hud, with the caller's address. 0.5.691's test
  (logs-backup/20261101-125343-webley-iconslot691): with only the Webley's icon slot given its
  picture and the names left the Colt's, the kill feed shows the Colt's silhouette (no white
  box): the kill feed draws by the NAME (+1860), not the slot. This finds which look-up it uses.
  Off by default; nothing else changes.

## 0.5.691: the Webley's icon, slot only (test switch); Carentan playable (tools)

- COD3_WEBLEY_ICON_SLOT=1: the Webley's HUD icon slot gets its copy of the Colt's icon
  object with the Webley's picture (as COD3_WEBLEY_OWN_ICON), but its icon NAMES stay the
  Colt's. 0.5.680's white box came with both done at once (a name look-up of hud_webley
  finds nothing). Off by default; nothing else in the game changes.
- Carentan (tools and data only, the map pack; see docs/CLAUDE-SESSION-2026-11-01.md,
  "Carentan playable"): CoD2's spawns, CTF flags, war/dom/HQ objectives (their triggers
  moved: tested, the game counts players in the war zone at Carentan's point), CoD2's 1532
  placed models drawn (their clip brushes were the invisible walls), Rouen's props, trees
  and effects off, Rouen's asset-only zone blocks dropped (CoD3's 96 banks of 3 MB).

## 0.5.690: Carentan texture binding and corrected diagnostics

- Converter C2_TEST=C under test: actual CoD2 normal/spec maps and complete mip chains with e_blend materials; 138 images/1333 mips recovered byte-for-byte from GPU layout. Shader numeric parity and standalone scene remain open.
- COD3_TEXLOOKUP now reads the name from the actual resource entry (r4), rather than treating the block descriptor (r3) as a texture. Logs the resolved image part after the original handler. No texture behavior change.
- Data-only Carentan scratch-harness texture binding fixed (200/200 logged lookups now nonzero, zero misses): list texture directory entries before cells, as all 41 shipped zones containing both do. Previously only blocks were reordered; 690 proves texture handlers ran after failed MAT imports. Offline regression fails before, passes after; all resource blocks unchanged.
- 689 scratch Carentan/Rouen harness reproduced missing c2 texture lookups; handler names were blank, so absence of c2 handler lines was inconclusive.

## 0.5.689: the texture diagnostic, binding side

- COD3_TEXLOOKUP=1 also logs the TEX resource handler (sub_82476F38: name, description,
  pack) and the category-0 binds (sub_824750C0: name, scope, texture, result). 0.5.688's
  log: every lookup of the converted level's textures (c2*) found nothing, in scopes 0xD, 7, 8.

## 0.5.688: a diagnostic for the Carentan level work (texture lookups)

- COD3_TEXLOOKUP=1 logs the texture lookups by name that materials' imports go through
  (sub_82477E70: scope, hash, result), for the converted level's textures (c2*) and the first
  30 others. Off by default; nothing else changes.
- Carentan (tools only, see docs/CLAUDE-SESSION-2026-11-01.md): CoD2 Carentan's world cells
  now draw in CoD3 (test zones in the placeholder pack); its own textures still come out
  white (their lookups fail): this build is to see why.

## 0.5.687: bots may carry the flamethrower

- bot_kits.txt may name "flamethrower" as a kit's main weapon (BotKits::KnownPrimary), and
  bot_pose takes it. The default kits are unchanged.
- 0.5.686's test (logs-backup/20261101-071145-flame686-result): the flame now a jet of WaW's
  fire from the nozzle toward the crosshair; the world model made (the Thompson's world body
  m1a1_0 given WaW's 1288 vertices, turned to its winding) and given on every look-up.

## 0.5.686: the flame's chunks smaller and faster

- 0.5.685's test (logs-backup/20261101-070621-flame685-result): a stream now, but one huge
  fireball over the middle of the picture: chunks 3 to 40 units across, a chunk 100 units out
  being 15 pixels a unit at 1904 across. Now 0.8 to about 17 units, thrown at about 950
  units a second with less drag (it reaches out before it swells), 150 a second, alpha 0.9.

## 0.5.685: the flame as a stream

- 0.5.684's test (logs-backup/20261101-065959-flame684-result): the M2 in its own colours
  in both hands, the flame at the nozzle, but one ball of fire: its chunks moved on the
  screen alone and piled up at the nozzle (which is just under the crosshair).
- Now each chunk flies in the camera's space: from the nozzle's point (its place on the
  screen, 18 units ahead) toward the aim 600 units out at about 700 units a second, slowed
  by the air, pulled down a little, swelling from 3 to 40 units as it burns, and is drawn
  where it projects (65 degrees across), along its own motion: the stream narrows into the
  distance toward the crosshair and droops at its end. 120 chunks a second.

## 0.5.684: the flamethrower's colours, its flame's place, screenshots with the overlay

- 0.5.682's test (logs-backup/20261101-065050-flame682-result): the two-point fit is right
  (the tube in the left palm, the nozzle past it), but the M2 was all grey: the Thompson's
  draws read their colour map from slot 4 (the Colt's from 5), where grey.png went. Now
  the M2's texture goes to slot 4 and custom\flamethrower\slots.txt is
  "2=dark_cube.png 3=flat_nm.png 5=grey.png" (world_slots.txt for the world model's).
- The flame starts at the nozzle's tip as the screenshots have it (0.515, 0.565 of the
  picture; COD3_FLAME_MUZZLE still moves it).
- COD3_SHOT_OVERLAY (tests) now works in HDR too and whenever the overlay draws: the
  screenshot is taken after the overlay is laid over (it was the picture alone: the flame
  was never in a test's screenshot).

## 0.5.683: the flamethrower's world model

- Others see World at War's M2 in its holder's hands (and dropped): its world model's name,
  w_usa_flamethrower_WORLD, is looked up as the Thompson's (w_ussmg_world) and given a copy
  of it whose body draws weapon_usa_flamethrower (FlameWorldModel, as the Webley's; 52-byte
  vertices here). Not turned, scaled or moved: both models have the muzzle along +x and z
  up, and both origins are the hand's (the M2's grips 10 behind and 5 ahead, the Thompson's
  8 behind and 4-12 ahead; measured on weapusge.cod block 13 with xemesh.py).
  make_flamethrower.py writes weapon_flamethrower.mesh and flamethrower_world_c.png.
  COD3_FLAME_NO_WORLD=1: the Thompson's.

## 0.5.682: the flamethrower's fit in both hands

- 0.5.681's test (logs-backup/20261101-064117-flame681-result): made and swapped in as
  planned (weapon 46, 23 fields, texture in the colour slot, winding turned to the
  Thompson's), but fitted by one point: the rear grip was in the right hand while the tube ran
  under an open left hand (the Thompson's palm holds a handguard from below).
- Now fitted by two points, worked out on both meshes: the rear grip on the pistol grip and
  the tube just behind the front grip in the left palm: 5.7 degrees nose up about the rear
  grip, scale 0.532 (the nozzle 4.5 units past the Thompson's muzzle). COD3_FLAME_FIT=
  "scale pitch x y z" (scale 0: as fitted; pitch: degrees more).
- (The flame was drawn but missing from the screenshots: they are taken before the overlay
  unless COD3_SHOT_OVERLAY=1.)

## 0.5.681: World at War's M2 flamethrower

- A new class weapon, "M2 FLAMETHROWER" (primary 23, sidearm 25), made from the user's own
  World at War by tools/waw (wawimport.py reads its fastfiles: XModel, inline WAV sounds,
  IWi images; make_flamethrower.py writes custom\flamethrower). A copy of the Thompson.
- Its view: WaW's viewmodel_usa_flamethrower (4068 vertices, its hose on a grey band of the
  texture) drawn in the Thompson body's place while held (MpFixesFlameView, as the Webley's);
  the Thompson's magazine, round and charging handle hidden. Fitted offline on both meshes:
  the M2's rear grip on the Thompson's pistol grip, scale 0.391 from the hands' spacing.
  COD3_FLAME_FIT="scale x y z" to adjust without a rebuild.
- Its flame (flamethrower.cpp): WaW's flamethrower_fire sprites (8x8 frames) thrown from the
  nozzle's place on the screen to the crosshair over the view (Overlay::SetWeaponHud, new;
  Overlay::ImageTexture loads PNGs for ImGui), a fireball's life each, plus a flickering lick
  at the nozzle. COD3_FLAME_MUZZLE="x y" places the nozzle; COD3_FLAME_TEST=1 burns it
  whenever held (screenshots).
- Its sounds: WaW's ignition, 1.5 s burning loop (again while the trigger is held) and
  cool-down; others' flames as WaW's mono loop, fainter with distance. The Thompson's shots
  are silent for it (its name is in no sound table).
- Its numbers (FlameFields, from weapons/mp/m2_flamethrower_mp): 25 a shot at 10 a second
  (WaW's 250 a second), full to 250 units, none past 450, trace 500; a 100-round tank and
  200 spare (CoD3 has no overheating); WaW's light kick and 65 zoom.
- Not yet: others see a Thompson in its hands; the kill feed shows the Thompson's icon; the
  flame is ours only (others' flames are heard, not seen).

## 0.5.680: the Webley's kill feed icon back to the Colt's (for now)

- With every bot on the Webley (a test bot_kits.txt, the user's put back after), Webley
  kills showed a white box: the kill feed looks hud_webley up by name somewhere our copies
  do not reach (not sub_824784B8, not sub_824EE308; the menus' image entries are built at
  the front end). So the Colt's icon stays by default, as before 0.5.677;
  COD3_WEBLEY_OWN_ICON=1 turns the Webley's on, for finding that look-up
  (logs-backup/20261101-001200-iconkills).

## 0.5.679: the Webley's icon in its weapon slot

- 0.5.678's look (logs-backup/20261101-000330-fields678) found no icon handle in the
  definition, only names. The kill feed and the HUD take a weapon's icon object from the
  HUD's table, one a weapon (sub_824EF868, +52 + 4 x weapon), which the port already fills
  for the campaign guns. The Webley's slot now gets a copy of the Colt's icon object, its
  parts given the Webley's image (MakeWebleyIcon, from the Colt part's own image, drawn
  as CoD2's hud_webley). The field dump of 0.5.678 is taken out again.

## 0.5.678: a look at the Webley's icon fields (diagnostic)

- 0.5.677's test (logs-backup/20261031-235548-icon677) set the Webley's item icon to
  hud_webley, but no image or menu look-up of that name came. So the kill feed must take a
  handle resolved when the Colt's definition loaded. This build logs the Webley
  definition's words +1400..+1900 (a name, or the first words of what a word points at)
  to find it.

## 0.5.677: the Webley's own icon in the kill feed; its reload framed as CoD2's

- The Webley has its own icon, CoD2's hud_webley (custom\webley\hud_webley.png), in the
  kill feed (+1860) and on the HUD (+1440); before, the Colt's.
  - No CoD3 archive has an image by that name, so the image look-up (sub_824784B8)
    answers hud_webley with a copy of the Colt's image (MakeWebleyIcon). Its texture
    constant, found in the image object, points at a place of its own.
  - The renderer gives that place CoD2's picture: the new Render::SetTextureImage, a PNG
    for the texture at a given address.
  - COD3_WEBLEY_COLT_ICON=1 keeps the Colt's.
- The reload's tables (data only; retarget.py):
  - The gun moves in the view as CoD2's does: it comes up to the eye to throw the cases.
    Before, it went away and to the side while the left hand came up into the camera.
    Its turn since frame 0 is applied in view space, its path scaled as the fit is. The
    hands keep CoD2's relation to the gun.
  - The thumbs bend the right way: CoD2's thumb joints curl about -z like the Biped's,
    so they are not flipped as the fingers are.
  - The left hand stays off-screen while CoD2's is away from the gun (to the pouch and
    back), blended for real (position, turn, the elbow's bend), not by its offset alone.
- tools/cod2import/cod2view.py draws CoD2's own first-person view of an animation. It
  does not frame like CoD2 yet: CoD2's viewmodel projection is not known.

## 0.5.676: the Webley's cylinder shows the rounds left; the reload's fingers fixed

- The cylinder as a revolver's. Each round fired turns it a sixth of a turn (clockwise seen
  from behind, as a Webley's, eased over 0.12 s), and its round is gone. So the rounds in
  it are the rounds left: after 3 shots, 3 (the clip, read each frame).
  - CoD2's own Webley does neither: its fire animation does not move the cylinder.
  - The cylinder (its centre, axis and the rounds' firing order: the top one first) comes
    from the mesh's rounds (retarget.py, webley_reload_gun.bin version 2).
- In the reload: the cylinder stays as it was until the extractor throws the cases out
  (frame 35, 1.17 s; all six cases fly). From when the new rounds are in (frame 89,
  2.97 s), it holds as many as the reload gives (the clip and the spare rounds, six at
  most), unturned.
- The fingers in the reload: each finger joint now takes CoD2's own bend of that joint
  since frame 0, in its own frame. CoD2's bend about z is the Biped's -z, and CoD2's right
  fingers run along -x. Before this, the whole-world turns, with CoD3's left hand at rest
  off-screen, bent the fingers about the wrong axes.
- The left hand is turned by its own shape (wrist to middle knuckle, index knuckle to
  little finger's) to match CoD2's, rather than by its rest pose.
- COD3_WEBLEY_CLIPTEST=N: N rounds left, for tests.
- tools/cod2import/meshview.py can be imported (no run on import).

## 0.5.675: the Colt's reload kept off the Webley

- During the Webley's reload the game still plays the Colt's gun animation on the gun's
  skeleton (colt), and the Webley is drawn on its body bone. That skeleton's pose (11 bones)
  is now kept from the frames before the reload and put back on each frame of it, so only
  CoD2's reload moves the Webley.
- Tested so far with the scripted reload only (COD3_WEBLEY_RELOADTEST); a real reload, from
  firing, still needs playing.

## 0.5.674: the Webley's own parts in its reload; the left hand helps

- The Webley's parts move with CoD2's reload: the cylinder swings out, the spent rounds
  fly out, and the speed-loader comes in with six new rounds. custom\webley\
  webley_reload_gun.bin (retarget.py) holds each of the gun's bones' move per frame, and
  whether it is shown (CoD2 parks the speed-loader and the spent rounds far off the gun).
  At each new frame of the reload the Webley's vertices are written again
  (PoseWebley) and the renderer takes them again (Render::MemoryFreed).
- The speed-loader is kept in the mesh now, folded away at rest; it is left out of the
  Webley's size fit.
- The left hand follows CoD2's left hand: it cradles the gun, fetches the speed-loader
  and loads it. CoD3's own offset (its left arm is off-screen with a pistol) is only
  used in the first and last 10 frames.
- The friend update carries both tables.

## 0.5.673: the Webley's own reload on the arms (first try)

- CoD2's Webley reload (5 s: the gun opened, the empties out, the speed-loader in), played
  on CoD3's first-person arms instead of the Colt's magazine change.
  tools/cod2import/retarget.py makes custom\webley\webley_reload_arms.bin from the user's
  CoD2 and a pose dump of CoD3's arms (0.5.672):
  - the gun follows CoD2's
  - the hands follow CoD2's wrists, and the elbows are solved with CoD3's arm lengths
  - the fingers take CoD2's turns
- The game's pose function (sub_82186740) writes model-space matrices, 64 bytes a bone. For
  the arms (skeleton viewmodel_hands) while the Webley reloads, the played bones get the
  table's frame under this frame's pelvis (view sway kept), blended in and out over 0.12 s.
- Begins with our own player's Webley reload sound; ends after 5 s or when the Webley is
  put away. COD3_WEBLEY_RELOADTEST="8" plays it 8 s after the Webley is first held, for
  tests. COD3_WEBLEY_NO_RELOAD=1 keeps the Colt's.
- Not yet: the Webley's own parts (the cylinder swinging out, the rounds, the speed-loader)
  move with it; the friend update carries the table (MAKE-FRIEND-UPDATE.ps1).

## 0.5.672: a look at the arms' pose (diagnostic, multiplayer)

- Toward the Webley's own reload, played on the arms at run time. sub_82186740 fills each
  animated model part's pose, 64 bytes a bone, in the frame's pose jobs (sub_8250EFF0).
  COD3_POSE_DUMP=1 writes each model part's first pose from 20 s in to
  custom\webley\dump\pose_<n>.bin, with the model object and the names it points at, to
  find the view arms and the pose's layout. Off unless set.

## 0.5.671: co-op v46 from J merged

- J's co-op v46 (Downloads\CoD3-coop-v46-source.zip, with the build CoD3-coop-v46.zip; what
  changed since v40 is in docs/COOP-V40-DESIGN.md, "v43..v45" and "v46"). It makes the
  joining copy stable:
  - the script VM no longer spins on ended threads, and our drones and physics stop once frozen
  - the reliable command buffer no longer overflows
  - puppets are pooled and reused, and our pose jobs are capped
  - the joining copy never kills its own player
  - the pose queue begins again at 150 (no more T-posed soldiers)
  - checkpoints reload seamlessly behind the window (coop_swap.cpp)
  - a full-screen CO-OP CAMPAIGN lobby works with the controller (Y, C, a click or F2 on the
    title screen or main menu)
  - with two players, the mission runs on behind the pause menu (COD3_COOP_PAUSE=1 pauses)
- Merged by hand, three-way against their v40 (the base of our 0.5.622 merge):
  - coop.cpp: their v46 changes with our 0.5.601 sprint kept (git merge-file, no conflicts).
  - coop.h and coop_events/host/hud/join/laststand/lobby/probe.cpp: ours were their v40
    unchanged, so they are their v46.
  - coop_swap.cpp is new; CMakeLists gains it.
- Their copies of shared files were not taken whole: ours have our own fixes since. Only
  their additions were put in:
  - heap_trace.cpp/.h: the joining copy's heap guard (HeapTrace::SetGuard). Every block is
    followed in the campaign so a second free can be told and skipped.
    COD3_NOHEAPFOLLOW=1 turns the following off. Campaign only (CoD3MP leaves this file out).
  - kernel_xam.cpp: CoopRelaunchFresh.
  - window.cpp/.h: COD3_WINDOW_RECT, COD3_COOP_HIDDEN, Window::Reveal.
  - input.cpp/.h: Input::RealPad, taken under the input lock that Refresh reads the pads
    with (theirs read without it, from the drawing thread).
  - title_trace.cpp: "PRESS START" counts as the main menu, for the lobby's panel.
- Checked: none of the 28 game hooks in the co-op files is hooked elsewhere in ours, and
  every host function they call exists here.
- Also in this build (multiplayer, diagnostic): COD3_ASSET_DUMP now also catches assets made
  through sub_82475550, the path the weapons' animations take.

## 0.5.670: a look at the Colt's animations (diagnostic)

- Toward the Webley's own reload. COD3_ASSET_DUMP="usCOLT colt" logs each loaded file
  whose name holds one of the words: sub_82475610 puts each in its kind's table. It also
  writes 16 KB from the file's value to custom\webley\dump\, to learn the .xeanim and
  .xeskel formats. Off unless set.

## 0.5.669: no Colt left on the Webley

- The user still saw parts of the Colt on the Webley. A test with the Webley moved up
  (COD3_WEBLEY_FIT="0 0 0 2.5", logs-backup/20261031-202858-webleymoved-result) left the
  Colt's slide and hammer in the hand, drawn as broken shards. The Colt's parts were
  hidden by their triangle and index counts only. They are now hidden the way the MG34's
  bipod is: the triangle, vertex and index counts and the index buffer's size, all none.
- The Webley drew silver: the Colt's draw has a reflection cube in slot 2 (128x128, the
  Colt's chrome), which nothing replaced. A texture file named *_cube.png is now made as
  a cube (the picture on all six faces). custom/webley/slots.txt gives slot 2
  dark_cube.png (16x16, dark grey; made by hand, in custom/webley).
- Carentan (no build needed for it): the pack's folder is `carentan`
  (tools/kapf/fli.py, docs/CLAUDE-SESSION-2026-10-31.md "Carentan step 2 done").

## 0.5.668: the Carentan overlay taken out again

- The user wants Carentan as a map of its own in the map list, with its own level file as
  CoD2 has it, not CoD2 pieces drawn over a CoD3 map. 0.5.667's overlay (the carentan
  console command, COD3_CARENTAN, the cod2_carentan_<n> model copies) is removed whole;
  Isle and the other maps are as in 0.5.666. 0.5.667's test showed the copies drawn and
  textured (screenshots in logs-backup/20261031-164004-carentan1-result), 66 of 120 before
  the extended memory ran out.
- tools/cod2import/cod2install.py stays (copies a converted map into custom\).

## 0.5.667: CoD2's Carentan drawn in CoD3 (first try)

- tools/cod2import/cod2install.py: a converted CoD2 map into the install's custom\carentan\
  (pieces.txt: one .mesh a material with its colour, normal and specular maps; spawns.txt).
  Sky left out. Carentan: 120 pieces, 254 maps, 47 MB.
- Each piece is a script_model of its own at its centre (the UAV plane's PlaneSpawn), its
  model "cod2_carentan_<n>" a copy of the Colt's world model (as the Webley's world model)
  carrying the piece's vertices. The model setter keeps a pointer, not a model number, so
  120 new names take nothing from a list.
- Console: carentan [here | off | x y z]: here puts CoD2's first tdm spawn at your feet.
  Tests: COD3_CARENTAN=here, COD3_CARENTAN_SHOTS="8 14" (screenshots).
- Tuning without a rebuild: COD3_CARENTAN_AXES (a 3x3 matrix), COD3_CARENTAN_SLOT (the
  colour map's slot, 5), COD3_CARENTAN_TEX, COD3_CARENTAN_ONLY, COD3_CARENTAN_NO_BOX,
  COD3_CARENTAN_CLOCKWISE. The Colt model's words are logged once (its parts' base pose).
- Drawn only: no collision, the CoD3 map stays under it.

## 0.5.666: the Webley sized by height (its sights on the Colt's); Carentan read

- 0.5.665's tests: the world copy is drawn (its buffer 46050000's texture made in world2's
  log) and a bot carries it barrel forward (world3's screenshots).
- The view: scaled to the Colt body's height, not its length. By length the (longer)
  Webley came out 11% small and its sights 0.25-0.32 below the Colt's (fit_dump: Colt
  front/rear sight tops 1.229/1.301, the Webley's 0.982/0.984), so aiming would look low.
- The mesh-texture draw lines: the first two of each mesh (the world copy's slots too).
- CARENTAN (tools, no game change): tools/cod2import/cod2map.py reads CoD2's IBSP v4
  (mp_carentan: 121 materials used, 118,352 triangles, 128 spawns, 1532 placed models of 65
  kinds) into .mesh files a material, the entities, spawns and models lists and a picture
  from above; cod2assets.py converts its materials' maps (CoD2 material files: texture
  table at 0x3C, count at 0x34) and its placed models. Output in D:\Call of Duty 2 (USA)\carentan.

## 0.5.665: the Webley's world copy made deep; its maps by default

- 0.5.664's tests (logs-backup/*-texA..D-result): slot 5 BC1 1024 the colour map, 3 BC3
  512 the normal map (DXT5, x in alpha: CoD2's "gggr" the same), 2 a cube (reflection), 1
  R32 the shadow map, 4 and 6 BC1. CoD2's normal in 3 and specular in 4 took the Colt's
  panels off; 6 = specular washed it white, 6 = grey looks as A without the Colt's map.
  Default now custom/webley/slots.txt: `3=webley_n.png 4=webley_s.png 6=grey.png`.
- The world copy was given out on every look-up but never drawn (no draw of its buffer in
  any test, bots holding it in view). The Colt's model is now copied with all its arrays
  (its six count/pointer pairs reach up to 0x230 bytes past it), every reference to a
  Colt mesh in the copy moved to ours. Only one of its 4 parts has a mesh (the others
  null). COD3_WEBLEY_MARK_COLT=1 draws the Colt's own world model white, to tell them apart.
- Tried COD3_EXEC="set sv_cheats 1;set cg_thirdPerson 1" to see it: no third person.

## 0.5.664: the Webley's other texture slots

- 0.5.663's dump (the Colt body and the Webley as drawn, side and top): both point the same
  way, the Webley's grip in the Colt's grip: the fit is right. What drew "another gun on
  top": only the colour map (slot 5) was CoD2's; the Colt's other maps (its normal and
  specular maps) were still used, read through the Webley's texture coordinates.
- Render::SetMeshSlotTexture: a PNG for any other pixel slot of a mesh's draws. The
  mesh-texture draw lines now give each slot's format and size. COD3_WEBLEY_TEX=
  "6=flat_n.png 3=black.png" or custom/webley/slots.txt picks them (files in custom/webley).
- CoD2's own maps converted beside it: webley_n.png (normal, ~webley_n-gggr) and
  webley_s.png (specular), plus flat_n/black/white/grey stand-ins.

## 0.5.663: diagnostic: the Webley's fit dumped

- 0.5.662's test: the Colt's triangles face counter-clockwise too (388 of 388): the
  facing was never the problem.
- COD3_WEBLEY_DUMP=1 writes the Colt body's and the Webley's drawn positions and
  triangles (custom\webley\fit_dump.bin) to compare their fit offline.

## 0.5.662: the Webley's triangles face the way the Colt's do

- User: "looks like another gun model is on top of our webley ... two guns merged".
  CoD2's triangles face counter-clockwise to their normals (all 2663 view / 1103 world).
  The Colt's own are now checked the same way (signed agreement with its packed normals);
  if CoD3's face the other way, the Webley's are turned round, else its outside is culled
  and its inside drawn through it. Logged on the "the Colt's normals" line.
  COD3_WEBLEY_CLOCKWISE=0/1 forces it. Same for the world copy.
- 0.5.661's test: the world copy was made (6E040000) but none was drawn in view.

## 0.5.661: others see a Webley, not a Colt

- The Webley's world model is its own: w_brit_webley_WORLD (the weapon's +1432 and +1436),
  looked up as the Colt's w_us_pist_WORLD (sub_82446EA0) and given a copy of it
  (WebleyWorldModel): the handle, its model and meshes copied (pointers inside each moved
  with it), the body mesh (most triangles) given CoD2's weapon_webley world mesh
  (custom\webley\weapon_webley.mesh: 1333 vertices, 1103 triangles), turned from CoD2's
  axes (z up) to the Colt's (y up, z across), signs from where the vertices gather, scaled
  to the Colt's length, centred on its box; the Colt's other parts not drawn; no detail
  levels. Bounds set to the new box. COD3_WEBLEY_NO_WORLD=1: the Colt's again;
  COD3_WEBLEY_WORLD_SIGNS="sx sy" overrides the turn.
- Render::SetMeshTexture takes several meshes, each with its slot (-2: the draw's lowest).
  The world copy gets webley_c.png in the lowest (COD3_WEBLEY_WORLD_SLOT to pick another).

## 0.5.660: probe: the Colt's world model look-up

- 0.5.659's bot test (logs-backup/20261031-115718-webleybots-result): eight bots with only
  the Webley for 150 s, no crash; the Colt's fire_3rd and last_shot_3rd silenced, CoD2's
  shot played.
- For others to see a Webley (not the Colt): the look-up of w_us_pist_WORLD (sub_82446EA0)
  logged with what it gives ("webley world probe"), to clone it with CoD2's world mesh.
- THE WEBLEY'S PARTS (user: "still looks weird"): CoD2's viewmodel has every bone at the
  origin; CoD2 places the barrel/top strap (j_bolt), cylinder (j_chamber), rounds and
  trigger from its animations. tools/cod2import now reads xanim v14 (after
  riicchhaarrd/xmodelconverter, GPL) and poses the mesh from viewmodel_webley_idle's first
  frame (`model viewmodel_webley out.mesh --pose viewmodel_webley_idle`). The install's
  custom\webley\viewmodel_webley.mesh is the posed one (no rebuild: data); in game it is
  fitted at scale 0.405 (logs-backup/20261031-121320-webleyposed-result).

## 0.5.659: bots may carry the Webley

- bot_kits.txt takes "webley" as a main or second weapon (the second takes the pistol's
  place). A new default line: a medic with the Webley as his pistol. The install's
  bot_kits.txt got the same line.
- 0.5.658's self-test (logs-backup/20261031-115055-webley658b-result): the Webley now faces
  forward in the hand (hammer at the thumb, barrel away).

## 0.5.658: the Webley faces forward; its last shot no longer crashes

- The user's play of 0.5.657: the Webley looked backwards, and the game crashed on firing.
- The crash: the Colt's last-shot event (weapon_colt_weapon_last_shot_1st) by a Webley holder
  went into the campaign shot-cue swap (sub_8212B268), which read the Webley's shot cue name,
  null (strlen on 0). That swap is now skipped for a gun with no shot cue, and the last-shot
  event is silenced for Webley holders like the Colt's other fire sounds (CoD2's shot plays).
- The model: CoD2's barrel (+x, the grip at -x reaching down to z -4) is the Colt's -y (the
  Colt body's muzzle end at y -4.14, its grip end at +1.24). 0.5.657 put it along +y: the
  grip forward. Now turned 180 degrees about z (CoD2 x,y,z -> Colt y, -x, z), its back end on
  the Colt body's back end. Normals turned the same way.

## 0.5.657: the Webley in CoD2's texture by default

- 0.5.656's self-test (logs-backup/20261031-033203-webley656-result): the Webley's draws
  matched at last ("mesh texture ... for the vertex buffer 46100000: made (512x512)").
  They have six textures (pixel program slots 5, 6, 3, 2, 1, 4; samplers 0-5) and none
  in slot 0, so nothing was put.
- With COD3_MESHTEX_SLOT=5 (logs-backup/20261031-033412-webley656s5-result) the Webley
  is drawn in CoD2's dark blued steel and dark grips, not the Colt's shiny finish. Slot 5
  (sampler 0) is the colour map, now the default.
- Unattended firing could not be tested: with COD3_PAD set, the big test's own presses
  never join a side (20261031-033616-webley656fire-result: "did not load"), so the
  shot and reload sounds wait for a played test ("webley: CoD2's shot", "webley: the
  Colt's sound ... not played").

## 0.5.656: the Webley's texture by the GPU's address; CoD2's reload, the Colt's sounds silenced

- 0.5.655's self-test (logs-backup/20261031-032542-webley655-result): still no
  "mesh texture" line. The Webley's buffers come from Guest::AllocateExtended, the
  extended window (kernel.h: CPU 0x48000000-0x6F000000), whose GPU address is 0x28000000
  lower (gpu_address.cpp): the draws carry 0x46100000 for our 0x6E100000. The mesh
  texture's buffer is now kept as the GPU's address and compared whole.
- The Webley's sounds (sub_82554BA0, the weapon events' sounds: the event's name and the
  sound handle of whose it is):
  - For whoever holds the Webley (his state's +164), the Colt's fire, reload and
    rechamber sounds are not played (the event looked up under a name no table has).
  - On a reload CoD2's custom\webley\wpnrld_webley_rev.wav plays (4.1 s, once per
    2 s each).
  - CoD2's shot plays from CampaignEventName (0.5.653).
  - Yours at 0.9, another's at 0.6 fading to none by 4000 units.
  - Logged: "webley: the Colt's sound ... not played".

## 0.5.655: the Webley's texture found by its buffer's physical address

- 0.5.654's self-test (logs-backup/20261031-031716-webley654b-result):
  - Spawned with the Webley (6 | 24 on the HUD). CoD2's view mesh was made (2727
    vertices, 2039 triangles; the speed-loader left out), scaled 0.49 onto the Colt
    body's box, and swapped in ("in hand ... 1 Colt bodies, 3 parts hidden").
  - The screenshots show a revolver in the hand (top strap, barrel, cylinder), in the
    Colt's texture: no "render: mesh texture" line, so the texture was never matched.
  - The running game's memory (guestpeek) has the Colt body's vertex description on our
    buffer (6E100003, 2727 x 52 bytes) and its index description on ours (6117 indices).
- The draws' fetch constants carry the buffer's physical address, without the
  0x60000000 of its guest address. The mesh texture's buffer is now compared by physical
  address (& 0x1FFFFFFC) on both sides.

## 0.5.654: the Webley's missing HUD icon no longer crashes the start

- 0.5.653's self-test (logs-backup/20261031-030938-webley653-result) crashed 2 s in:
  strlen from the HUD images' hook (sub_82503EB8 -> IsCampaignIcon), which lower-cased
  every campaign gun's icon name, and the Webley has none (null).
- Now IsCampaignIcon and the HUD icon set-up skip a gun with no icon of its own (the
  Webley keeps the Colt's).

## 0.5.653: the Webley, step 2: a weapon of its own, CoD2's numbers, CoD2's gun in your hands

- 0.5.652's probe (logs-backup/20261031-024957-probe652-result):
  - The Colt's view model has four parts: colt.w_us_pist_VM (the body, 1093
    vertices, 878 triangles), colt_hammer, colt_slide and colt_mag.
  - Its vertices are 52 bytes (from its declaration): position (3 floats), a packed
    normal +12, three texture coordinate pairs +16/+24/+32 (the same), a colour +40, a
    packed tangent +44 and binormal +48.
- CoD3's weapon field table (354 fields: name, offset, kind) read from the running game
  (0x825D77D0, tools/cod2import/guestpeek.py) and kept as
  tools/cod2import/cod3mp_weapon_fields.txt.
- tools/cod2import/cod2import.py reads CoD2's models (xmodel / xmodelparts /
  xmodelsurfs version 20) and textures (.iwi version 5) from the user's CoD2 .iwd files.
  The formats follow mauserzjeh/cod-asset-importer.
  - The Webley's view model: 3292 vertices, 2663 triangles, bones applied.
  - Its texture: webley_c, 512x512 DXT1, written as a PNG.
  - Its weapon file is read too. tools/cod2import/meshview.py draws a mesh to check it.
  - Written to D:\Call of Duty 2 (USA)\webley and copied to the game folder's
    custom\webley (not in the repository).
- The weapon (mp_fixes.cpp Guns[]): "webley", a copy of the Colt (its arms and moves),
  named "Webley", appended last.
  - Its numbers are CoD2's (WebleyFields): every field CoD3's record names the same,
    105 of them (clip 6, ammo 18, fire time 0.135 s, reload 5 s, raise and drop 0.25 s,
    move speed 1.1, aim zoom 80, every spread, view kick and gun kick, sway), less
    the view placements (CoD2's arms') and the crosshair's sizes.
  - Damage in CoD3's terms: 45, minimum 22% (CoD2's 10), full to 384 units, least
    from 800.
  - Each field is logged with the Colt's value it replaces.
  - In the class editor's main weapons and sidearms (WEBLEY; class_N_primary 22,
    class_N_sidearm 24).
  - Gun code that assumed every campaign gun has its own world model and icon now
    skips them when it has none (the Webley: others see the Colt's).
- Its view (MpFixesWebleyView), as the MG42's in the MG34's:
  - While it is held, the Colt body mesh's counts and buffers are CoD3 buffers made from
    CoD2's Webley: turned so its barrel runs along the Colt's, scaled to the Colt body's
    length, its back and bottom on the Colt body's. COD3_WEBLEY_FIT="scale x y z" moves
    it.
  - The Colt's hammer, slide and magazine are not drawn. The speed-loader (CoD2's reload
    only) is left out.
  - Normals are packed the way the Colt's are (found from its triangles).
- Its texture (render_prompts.cpp, Render::SetMeshTexture): draws reading the Webley's
  vertex buffer get custom\webley\webley_c.png in the pixel program's texture slot
  COD3_MESHTEX_SLOT (0). The first three such draws log their textures.
- Its gunshot: CoD2's (custom\webley\wpnfire_webley.wav, 16-bit 44.1 kHz) mixed into
  the game's sound (audio_out.cpp Audio::PlayClip: our own clips, read once, resampled
  to 48 kHz, added to the front left and right before the limiter) on the Webley's shot
  events (1, 3, 7, 53). Yours is at 0.9; another's is at 0.6, fading to nothing by 4000
  units. The Colt's shot still plays under it for now.
- Not yet: the reload's sound (its event to be found from this build's log), its
  world model (the Colt's), CoD2's animations (the Colt's arms and moves, stretched to
  CoD2's 5-second reload).
- The first build failed: four printf strings broken by a script's "\n"; fixed.
- Untested until the self-test.

## 0.5.652: the Webley, step 1: a probe of the pistols' meshes

- The user: bring CoD2's Webley revolver into CoD3's multiplayer as a custom weapon
  ("dont stop until you have that pistol in our cod 3 port"). Its files are read from the
  user's CoD2 install (main/iw_13.iwd and iw_14.iwd: xmodel/viewmodel_webley,
  xmodelparts/viewmodel_webley2, xmodelsurfs/viewmodel_webley2, images/webley_c.iwi,
  weapons/mp/webley_mp, sound/weapons/Webley). They stay on this PC, never on GitHub.
- The plan follows the MG42's (0.5.900, 0.5.633): a new weapon in Guns[] as a copy of the
  Colt (its view, arms and moves), with the Colt's body mesh swapped while it is held for
  a mesh made from CoD2's Webley, its other parts hidden.
- This build only logs the pistols' meshes as they load (sub_82446AB8): each name with
  "pst", "colt", "p38" or "pistol", its mesh, piece, vertex declaration and first three
  vertices ("webley probe:", the first 40), to learn where CoD3 keeps a vertex's texture
  coordinates.

## 0.5.651: the render list's patch block checked before it is applied (643's crash)

- The crash of 643 at 3m45s (logs-backup/user-play-2026-10-31-643): the render worker's
  nglListSend (sub_82142248, thread sub_821424B0) applies a list's patch block with
  sub_8216DF98. Its +0 is a count of 32-byte entries (a place, then 7 words written
  there) and its +4 a count of 12-byte entries (a place, +8 a count of 16-byte vectors
  that follow, 16-aligned). A damaged block sent it writing from 0x061D0000 and reading
  from 0x732E0000 until it passed the 2 GB of guest memory. The same worker ran away in
  0.5.340 (memory overwritten by the depth copy thread) and 0.5.596.
- Memory was already damaged before it:
  - From 218 s: calls through non-functions (0x82705790, then 0x40E4, 0x380FF and
    0xC from a table at 0x8271B010, and entity handles and level times written there).
  - The first such call, in 643 at 84 s and 644 at the same place: front end code (the
    manager 0x82C6B1FC and the pause flag in its registers) calling a virtual of
    0x827B2910, which holds a list header (829E5810, 100, 98), not an object. Both
    shortly after the match start's team and class choices.
  - 642 had 9 such calls, 643 65 and 644 76; 645 to 648 had 1 or 2 each. The writer is
    not found yet.
- Now (mp_fixes.cpp) sub_8216DF98 is checked first. A block is not applied (logged as
  "render guard: a render list's patch block not applied", the first 20) when:
  - a count is past 16384,
  - a place is outside guest memory (the heap and stacks 0x40000000-0x7FFFFFFF, the
    image 0x82/0x83, the physical view 0xA0000000-0xBFFFFFFF),
  - or its vectors run outside it.
  The low places it wrote in 643 appear in no log but as bad reads.
- This stops the runaway, not the damage before it; that is still to be found.
- Untested until played.

## 0.5.650: SPECTATOR on the team choice shows no side, as AUTO SELECT

- The user, 649: "it works bro" (the gold frame follows the row); "when you hover over
  spectator it still shows the team flag for axis ... it should really just be like how
  auto assign looks blank/darked out".
- 647/648's probes: the side's soldier and flag follow the menu's +0xF0 / +0xF4 (1.0 /
  0.1 on ALLIES and AXIS, 0 / 0 on AUTO SELECT). The game sets them for its own rows only,
  so SPECTATOR (ours, row 3) kept AXIS's. Now (menu_mouse.cpp SeatEntry) they are 0 / 0
  while SPECTATOR is highlighted, and 1.0 / 0.1 again when the highlight goes back up to
  AXIS.
- Untested until played.

## 0.5.649: the team choice's gold frame follows the selected row (moved by us)

- The user, 648: the frame "still didnt move" in PLAY-NO-SPECTATOR-ROW.bat either, so our
  fourth row is not the cause. Logs: logs-backup/user-play-2026-10-31-648
  (CoD3MP-normal.log, the copy on the desktop; CoD3MP-nospectatorrow.log).
- 648's probe, 12 row changes: only the menu's +0x20 (the row), +0x2C, +0x6C/+0x70/+0x110
  (the soldier shown) and +0xF0/+0xF4 changed. No entry, text, the panel (+0x54,
  "MP_SS_PM_options.PANEL"), the object at +0x3C or the layer (0 widgets) changed, so
  nothing in this port places the frame on the row selected.
- Now (menu_mouse.cpp FrameTrack / FrameShift):
  - Every 250 ms while the team choice is up, its frame lines are found: the quads drawn
    in the last 150 ms (sub_824D7E60) that are thin (under 6 high), wide (over 200) and
    within the rows.
  - The row they frame is the one whose middle is nearest theirs (ALLIES as left).
  - Each is drawn moved by the selected row's place less the framed row's, and put back
    as it was right after its draw.
  - Logs the lines found ("menu: the team choice's gold frame: N lines around y ...",
    the first 6 times).
- 648 also confirms the seat fix: after SWITCH SIDE in the seat, ALLIES joined you
  ("joining side 1, kit 2") and you were playing (state 3) a moment later, twice.
- Untested until played.

## 0.5.648: a side taken after SWITCH SIDE in the seat spawns you; the frame probe widened

- The user, 647: "when in spectator if i click to change teams to allies or axis, it never
  spawns you in from spectator to changing teams it just leaves you in a frozen spectator
  cam"; the gold frame "is just stuck on allies still and never moves ... it needs to
  move like it used to before we added spectator as a 4th option".
- 647's log (logs-backup/user-play-2026-10-31-647):
  - Seat -> pause (Start in the seat works) -> SWITCH SIDE -> the seat left -> the team
    choice -> a side taken -> menus gone, and you still the game's spectator (state 1).
    The game's join goes on only from the match's start.
  - Now A on AUTO SELECT, ALLIES or AXIS there is remembered (FilterPad). Once the menus
    are gone and you are still state 1, you join that side as the seat's leaving does
    (Rejoin: the side, your kit, a spawn at its point). AUTO SELECT is the side the game
    gave you, else the allies. Log: "spectator: after SWITCH SIDE row N taken in the
    seat: joining side S".
- The frame probe (0.5.647): over 12 row changes, no word of the four entries or their
  texts changed. Only the menu's own fields did (+0x20 the row, +0x6C/+0x70/+0x110 the
  side shown, +0xF0/+0xF4, +0x2C), so the frame is not in the rows. The probe now also
  dumps the menu's objects at +0x3C and +0x54 and every widget of its layer (+0x48),
  to find the frame's widget and what moves it. Compare with PLAY-NO-SPECTATOR-ROW.bat.
- Untested until played.

## 0.5.647: the countdown starts with the match; a probe of the team choice's gold frame

- The user: "the prematch timer needs to start the moment the game timer starts ... even
  if im still in the menus and havent picked my weapon yet let the countdown happen
  regardless".
  - Now (bots.cpp Prematch) the countdown starts at the level's first server frame (its
    clock running), not at your first spawn (644: 15 s in).
  - Players who spawn during it are held like the rest.
  - The sides are balanced 2 s before GO, so the bots have joined by then (was 1 s in).
- The user, 646 (screenshots CoD3-20261031-003855/003859/003903): "the little yellow box
  around allies never moves to axis, spectator or auto assign"; "we need to make it so
  the gold frame moves through the selections as intended". The arrow keys and the
  d-pad do not move it either. The soldier shown follows the selection (American, then
  German), so the game's selection moves; the frame does not. The 29 October screenshot
  (0.5.627) already had the frame on ALLIES with AUTO SELECT selected. The menu dumps hold
  no front end quad for it.
- PROBE (menu_mouse.cpp SeatEntry): each time the team choice's highlighted row
  changes (12 times at most), every word that changed in the menu (+0..+0x200), its four
  entries (+0..+0x40) and their texts (+0..+0xA0) since the dump before ("menu: frame
  probe"). The first look dumps all of them. It changes nothing.
- PLAY-NO-SPECTATOR-ROW.bat (game folder, COD3_NOSEATENTRY=1): the team choice without our
  fourth row, to see whether the frame moves there.
- Untested until played.

## 0.5.646: the team choice's highlight by key-long presses; the pause menu in the spectator's seat

- The user, 645: "i cannot scroll through the change teams menu i cannot change it to
  where its highlighted on allies, or auto assign or spectator"; "when in game and
  picking spectator it still doesnt let you pause the game with the start button ... or
  the escape button ... when in spectator you should still be able to pull up the menus".
- 645's log (logs-backup/user-play-2026-10-31-645):
  - SWITCH SIDE -> team choice -> a click on SPECTATOR took the seat properly (the game's
    SpawnSpectator, state 3 -> 1).
  - The pause flag was cleared when the team choice went.
  - The team choice's highlight showed neither SetSelection nor a row written by us.
    Only the game's own cross handling moves it, and it took none of the 2-frame presses
    of 0.5.639.
- Now (input.cpp ListSteps) the presses that steer the team choice are 90 ms down and
  90 ms up, as a key held (MenuMouse::SlowSteps). The other menus keep the fast ones.
- In the seat Escape's Start reached the game (the mouse freed each time), but the game
  puts up no pause menu for a spectator (state 1). Now Start or Escape in the seat with
  no menu up asks for it (spectator.cpp). menu_mouse.cpp's Tick finds the pause menu
  (82083934) by its class in the team choice's system (645: system 8268A950, menu 0),
  sets the pause flag (0x82C73024) and puts it up. The game's own back / RESUME then
  takes it down. Logged as "menu: the pause menu put up in the spectator's seat".
- Untested until played.

## 0.5.645: SWITCH SIDE's team choice keeps the game paused while it is up

- The user, 644: the team choice from SWITCH SIDE comes up, but "its a little unresponsive
  ... the only menu that is highlighted is axis"; "even with the binoculars in the
  background of our menus it still never shows whats highlighted moving up and down
  anymore"; SPECTATOR after dying on a team "never truly puts you in spectator it just
  kinda shows you dead on the floor".
- 644's log (logs-backup/user-play-2026-10-31-644):
  - SWITCH SIDE put the team choice up six times, each "up and updated" within 16 ms.
  - 1-2 s later: "input: the mouse is held by the window and steers". 644 had cleared
    the pause flag (0x82C73024), so the in-game menus stopped updating (no highlight
    moved, the menu no longer counted as up), the window took the mouse for aiming, and
    the d-pad's presses took the binoculars out.
  - No "SPECTATOR taken" line: A on our fourth row reached the game's own team code,
    which made you a spectator with no camera of ours.
- Now the pause flag stays set while that team choice is up, as for the pause menu
  (menus updated, the mouse free, the cross the menu's, the binoculars left alone). It is
  cleared when the team choice goes, whether a side is taken, SPECTATOR is taken (the
  seat), or you go back (MenuMouse::Tick: "SWITCH SIDE's team choice gone: the pause flag
  cleared").
- Untested until played.

## 0.5.644: the pause menu's SWITCH SIDE opens the team choice

- The user, 643: "when you click to change teams mid games it doesnt pull up that second
  menu ... it should pull up the options to choose, the opposite team, auto assign,
  allies, axis, and spectator"; on picking a team at the start "its still scrolling and
  highlighting through the team choices as it should" (643's steering works).
- 643's log (logs-backup/user-play-2026-10-31-643):
  - The pause menu (82083934) reads SELECT KIT, SWITCH SIDE, RESPAWN, OPTIONS, EDIT
    GAME SETTINGS, QUIT. 643 looked for "SWITCH TEAM", so nothing changed. The game's
    SWITCH SIDE only asks YES / NO (82071504).
  - The countdown ran: "prematch: the countdown starts", "sides before the start: allies
    16, axis 16" (nothing to move), "GO" 10 s later.
- Now A (a click, Enter) on SWITCH SIDE (or SWITCH TEAM) never reaches the game
  (spectator.cpp). In menu_mouse.cpp:
  - The pause menu's system has its menu -1, and the pause flag (the byte togglepaused
    sets, 0x82C73024) is cleared.
  - The team choice of the match's start is put up (OpenTeamChoice): AUTO SELECT, ALLIES,
    AXIS, SPECTATOR. The game applies it as at the start.
  - In the seat, the seat is left first.
  - If no team choice is known, the pause menu stays.
  - Log: "menu: SWITCH SIDE: ... put up", then "the team choice is up and updated N ms
    after" when the game really shows it. 0.5.611 found a team choice put up with no menu
    up was never drawn; this one takes the pause menu's place while menus are updating.
- The crash at 3m45s (643's log): the game's own sub_82142248 -> sub_8216DF98 copied
  past the 2 GB of guest memory ("commit: guest write at 0x061D0000 ... read at
  0x732E0000", then the fault at 0x47E00000). It is the intermittent crash of 0.5.583 and
  0.5.596 (the same sub_82142248), older than this week's work; still open. The "more
  than 128 sounds" lines start at the main menu and were in 642's log too.
- Untested until played.

## 0.5.643: the team choice follows the pointer; the pause menu in the spectator's seat; SWITCH TEAM to the team choice

- The user, 642: the menus all click now, but on the team choice "it still doesnt scroll
  or highlight the box its supposed be hovering over"; in the seat "both pausing on
  controller and hitting escape never pulled up the pause menu"; "we need to make a
  menu that is included in the mp in game pause menus that says change team, but this
  menu need to open into a sub menu that shows both allies axis or spectator again".
- 642's log (logs-backup/user-play-2026-10-30-642): the seat was taken by the cross's
  down from AXIS and a click (640 works). No countdown line: the seat was taken before
  the first spawn.
- Team choice (82084854): SetSelection does not show there, so it is steered as the
  lists are: the cross brings the highlight to the row under the pointer at the game's
  frame rate, and SPECTATOR is reached past AXIS (spectator.cpp). A click on SPECTATOR
  takes the seat.
- The seat's hiding (menu_mouse.cpp) now takes down only the menus the game puts up for
  a spectator to join by: the team choice, the class choice (82084644) and the spawn
  screen (82085174). The pause menu and its questions stay up and are pointed at. While
  a menu is up in the seat, the pad's buttons are the menu's, not the camera's, and the
  HUD is drawn.
- SWITCH TEAM: the game's MP strings have it among the pause menu's (SELECT WEAPON,
  SWITCH TEAM, PLAYERS LIST, HOST OPTIONS, CALL VOTE, SUICIDE). The game's own entry is
  left to work: it opens the team choice, which has our SPECTATOR. Taken in the seat,
  the seat is left first without rejoining the old side ("spectator: SWITCH TEAM in the
  seat"), so the team choice comes up.
- "menu: shape" dumps now list what each entry reads, to confirm the pause menu's
  entries from the next log.
- Untested until played (also still untested: the countdown).

## 0.5.642: every arrow row in the System Link settings by mouse

- The user, 641: "i cant click on the arrows in the max numbers of players section and in
  the advance options or settings menu i cant click and of the arrows in their such as
  gravity, team kill, etc. i was able to click a few but nothing else".
- 641's log (logs-backup/user-play-2026-10-30-641):
  - Create Match (82082F0C): 640's wider name boxes put MAX NUMBER OF PLAYERS at
    248..392. Its centre fell between the arrows, so the arrows went to the name's
    entry again (the cross's right only moved the highlight). "Between the arrows" is
    now by where a box starts.
  - Advanced Options (8208310C): 6 rows of arrows and 12 entries (a name and a value a
    row). Some values' boxes are not found (no entry 5 or 7), and "shown 8" leaves out
    the last rows' entries, so those arrows had no entry.
- Now on a menu of only arrow rows with two entries each, row k's arrows are entry
  2k+1's (the value's). Clicking the value between them is found by the row's height
  from its arrows, not by the entry's box.
- Custom Rules (gravity and the rest) did not come up in 641's log. If it is laid out
  the same way, it is covered.
- 641's play reached no match, so the countdown is untested.
- Untested until played.

## 0.5.641: a pre-match countdown, with the sides balanced before the start

- The user: "if we added a pre match timer would be perfect and while the pre match timer
  counts down ... make the auto balance teams re arrange anybody so teams stay balanced in
  case any players spawn out of place or on the wrong side it can correct it before the
  game starts".
- On the machine running the server (bots.cpp Prematch, from SV_Frame's hook):
  - It starts the first moment this machine's player is alive in a level (record +304
    above 0, +1924 3). A new level is the level's time (0x82C2826C) going back.
  - It lasts COD3_PREMATCH seconds: 10 by default, 0 turns it off, at most 30.
  - While it runs:
    - Every player's command has its buttons (+8) and forward, right and up
      (+28..+30) cleared before Pmove: no moving, jumping, firing or throwing.
      The look stays.
    - The bots' brains rest as a posed bot's does (still, not firing, no stuck tests).
    - The player's own unstick waits.
  - At GO every bot's progress tracker is reset, so standing still doesn't count as
    being stuck.
- Balance, 1 s in: the connected players' sides are counted (bots by their given side,
  the rest by their team). While one side has two or more over the other, a bot of the
  larger side is given the smaller one (g_botSide, PickSide) and respawned at its spawn.
  Players are not moved. Log: "prematch: sides before the start", "prematch: bot slot N
  moved".
- On the screen (Overlay::SetMatchHud, a fifth HUD slot): "MATCH BEGINS IN" and a big
  number in the middle, then "GO!" for 1.2 s.
- Not yet: the count on a joining machine (it is frozen by the host, but sees no count).
- Untested until played.

## 0.5.640: SPECTATOR on the team choice reached by the pointer, the keys and the pad

- The user: "in the menus you cannot actually scroll down to spectator and click it. We
  need to make that option clickable".
- From the play logs (team choice 82084854, 4 entries): every entry's box was 18 wide
  by its widget's field (118..136), so the pointer on most of "SPECTATOR" (or "ALLIES")
  found no entry. The seat's own box (input.cpp's click on SPECTATOR) was never set since
  0.5.623 replaced our drawn row with a real fourth entry. And the game's cross only walks
  its own three rows.
- Now (menu_mouse.cpp, spectator.cpp):
  - Every entry's box is at least as wide as its words (about 9 a letter + 10), never
    over the next box on its row.
  - The seat's box is SPECTATOR's row (at least 120 wide). The pointer on it highlights
    it (SetSelection, or the row written when the game refuses its fourth), and a click
    takes the seat.
  - The cross's down from AXIS goes to SPECTATOR and up from it goes to AXIS. The game
    sees neither press ("menu: the team choice's row N by the cross"). A, Enter or a click
    on it takes the seat, as since 0.5.623.
- Untested until played.

## 0.5.639: the lists follow the pointer at the game's frame rate

- The user: "can we scroll through the menu's fast and skip over menu entrys and get
  directly to the one we highlight without having to watch it scroll"; "if i click it it
  clicks instantly"; "i want the menus being 1 to 1 with how other pc menus work and feel".
- Menus with entries of their own (the pause menu, Create Match's rows, the team
  choice) were already highlighted at once (SetSelection). The lobby's lists (the
  multiplayer main menu +250, System Link's lists +246, the learnt ones) and the
  off-screen quit question are steered by presses of the cross. Those were 60 ms down
  every 150 ms, so the highlight was seen crawling. Now (input.cpp ListSteps) a press is
  down until a whole frame of the game's has read it, then up until a whole frame has
  read that (two swaps each, at least 8 ms each): about 25 ms a row at 144 fps, 60 at
  60. A press the menu drops is made again, since the highlight is compared with the row
  under the pointer after each one.
- A click on a row the highlight has not reached is A once it gets there, now within 3 s
  (was 1.5 s) for long lists.
- Not done: writing a list's row directly (instant). +250 has a mirror (+362) and
  another object changes with it; a wrong row on the main menu can start the campaign.
  Left until a test shows it is safe.
- Untested until played.

## 0.5.638: the quit question's YES, its texts found in the right order

- The user: "i still cant click yes when i click quit, and still the back button didnt
  work in the create a match system link menu". Both plays were 0.5.636: CoD3MP.log was
  last written at 22:59:56, and 0.5.637, with the right button's B then Back, was
  installed at 23:00:05. The right button is still untested in Create Match.
- 636's log (logs-backup/user-play-2026-10-30-637): the question was pointed at by its
  texts ("entry 0 at 28,131..208,151 entry 1 at 28,151..208,171"), but those are another
  YES and NO (at 118,137 and 118,157), not the ones drawn at 320,267 and 320,287. 636
  sorted the texts by address, so the system's came before the question's own. Now the
  menu's own texts are looked at first, in the order found (the system's after).
- Untested until played.

## 0.5.637: the right button goes back in every multiplayer menu

- The user, 636: "the rmb isnt working in the game rules menus so we need to make whatever
  was the back button is now the right mouse button ... the create a match screen i
  couldnt back out of or the game settings menu"; the arrows work now.
- 636's play log (logs-backup/user-play-2026-10-30-636): no level was loaded (38 files),
  so the right button pressed Back. Create Match (82082F0C) and Game Rules (8208310C)
  took no Back, but they go back with B, as a match's menus do.
- Now in multiplayer a right click presses B (80 ms). If the same menu is still up 250 ms
  later, it presses Back as well (80 ms), for any screen that goes back with Back. Never
  both at once, so a menu never goes back twice. Logged as "menu: right button: B did not
  go back from X, Back". MenuMouse::CurrentMenu gives the menu up.
- 636 also confirms the key icons: "render: the button prompts drawn as keys (19 cells)".
- Untested until played.

## 0.5.636: the quit question's YES and the lobby's arrows by mouse; the key icons in MP

- The user, 635: right click goes back now ("good"), but "i still cant click into any of
  the sub menus that say yes or no i can only click no also i still cant click any of the
  side arrows left or right so with the mouse i cant change maps".
- The quit question (635's play log, logs-backup/user-play-2026-10-30-635): seen as an
  off-screen dialog, but every click was A on NO. Cause: the search for the texts a
  dialog shows went only to +128 inside each object, and its YES and NO are at +148 and
  +152 (the "menu: shape" dump looks to +160, so it saw them). Now to +256. Such a
  dialog is also steered as the lobby's lists are: the cross brings the highlight to the
  entry under the pointer, one press at a time, and the click is A once it is there.
  The log says "menu: X (vtable Y) pointed at by its texts, steered by the cross: ...".
- The arrows (System Link Create Match, 82082F0C): each row has two entries, the
  name's (248..298) and the value's (392..441, between the arrows at 306 and 528).
  The arrows were given to the name's entry, so a click highlighted it, and the cross's
  right then only moved the highlight over to the value's. Now a row's arrows go to the
  entry between them. Xbox Live's Create Match (8207EA5C) has no entries of its own, so it
  is still not pointable there.
- Key icons: 635's probe found the icon font drawn at 17312000 (256x128) while we asked
  for 1F312000. The font texture's +32 word carries bit 27 besides the address in
  multiplayer, so the match now also accepts the address without it. Expect
  "render: the button prompts drawn as keys" in the log, and key caps in place of the pad's
  icons.
- Untested until played.

## 0.5.635: the quit question's YES by mouse; right click goes back in a match; why the key icons never show in MP

- The user: in a match, pause > QUIT > the question "just stays on no and i cant use
  the mouse to essential go up and click yes"; "the right click (RMB) should allow you to
  go back to the previous menu"; "add the pc icons finally".
- The quit question, from the played logs (user-play-2026-10-29-*, play630): it is menu
  vtable 82071504 (4 entries, NO highlighted). It is not in the dialogs' system class
  (0x82071478), so it was pointed at as an ordinary menu. Its entries are laid out off
  the screen (-111,-50..111,6, or -111,-22..111,34). The YES and NO you see are texts it
  holds through an object at +8 (+148 "YES" at 320,267, +152 "NO" at 320,287). The
  pointer was over no entry, so a click was A on the highlighted one: NO.
  Now (menu_mouse.cpp) a menu none of whose shown entries lies on the screen is pointed
  at as a dialog. Its entries are found by the texts that read the same, now looked for
  in the menu's own objects too, not only the system's. If none read the same, the shown
  entries are matched in order to the visible texts, top to bottom, when the counts agree.
- Right click: in a match's menus (pause, its questions, team and class choice) it is
  now B, which goes back. Before any level is loaded it stays Back, as it was for the
  front end. The "in a level" test is input.cpp's (40 files opened), which stays true
  once a match has loaded, so after a match the front end gets B as well. Check that B
  goes back there.
- Key icons (Frank's prompts as keys, 0.5.478): the campaign's logs say "render: the
  button prompts drawn as keys", but no multiplayer log ever did. The icon font and its
  texture are found ("prompts: the button icon font ... texture 5F312000"), yet no draw
  reads that texture. New logs to find out why:
  - "prompts: now keys asked for / the pad's icons ..." each time the choice changes.
  - While keys are asked for and none have been drawn, 150 ms every 10 s while a menu
    is up (6 times a run): "prompts: keys asked for texture X, not drawn; N small textures
    drawn: ...", with each texture's address and size. The icons' real texture should be
    in there.
- Untested until played.

## 0.5.634: killcam explosions: the server's explosions kept and played again

- 629-631's killcams: no explosion in any replayed object, either as an event object
  or in an object's own events. cgame's entity events (CG_EntityEvent, sub_8246BC18)
  only handle events 186-220 (shots and similar), and none of them is an explosion.
- The 630 play log has every live explosion, all as "weapons: event 56 for <weapon>"
  (k98_rg, stickygrenade, smokegrenade_axis), called through sub_82556180 from 825836A4.
  That is the server's projectile explosion (sub_82583310), which writes the grenade
  entity's origin +336 and angles +352 and plays its weapon's event 56 (effect and sound)
  once. The killcam puts entities back, so it never plays that call again.
- Now each call with event 56 is kept (time, place, angles, weapon name; the last 64) from
  the sub_82556180 wrapper (mp_fixes.cpp -> BotsKillcamNoteEvent in bots.cpp). During a
  killcam, each server frame plays the ones whose time the replay has just passed
  (Killcam::PlayBlasts). They play through the same function at the same place
  (MpFixesKillcamFx), on 8 script_origins in turn, so an effect that stays on its entity
  doesn't jump. These replays are not kept again. COD3_KILLCAM_BLASTS=0 turns it off.
- Logs: "killcam: explosion kept: ..." (the first 40), "killcam: explosion of then played:
  ... ms from the death" (the first 30), and at each killcam's end "killcam: explosions of
  then (0.5.634): N kept in the replay's time, M played again".
- Only on the machine that runs the server (the host). A joining PC's own killcam is not
  covered (its explosions arrive another way). Untested until played.

## 0.5.633: handheld MG42 avoids Isle's mounted-gun animation-table name collision

- Reproduced absent gun/hands and giant grey triangle on Isle/War with or without
  bots, as primary or secondary, including the plain body/bipod diagnostic stage.
  Rouen/War and Rouen/CTF draw correctly. Original class/animations did not fix it.
- Read-only runtime snapshots found identical weapon definitions, but Isle has an
  existing `mg42` animation entry for a mounted gun: 13 items, zero resolved
  animations. Native `mg34` and `mg34_arms` have 25 items, 24 resolved. Our alias
  only ran on lookup misses, so it accepted the wrong existing table on Isle.
- Scope animation lookup to our appended handheld MG42 during native view/arms
  setup (8245E720 / 82465920). Use MG34's handheld tables even if the mounted
  entry already exists. Outside these scopes native table hits stay unchanged.
- Runtime regression check: seven MG42 reload/raise/drop animation-rate fields
  must match the resolved donor. It fails on the 632 Isle snapshot and passes
  on the working Rouen snapshot. 633 build and runtime validation pending.
- No changes to gun geometry, handling, grenade or killcam behavior in this fix.
- VALIDATED 2026-10-30 (Claude, Codex run_case.py isle-633 --primary 20 --snapshot, Isle War, 0 bots, 1m41s,
  no crash): "mg42 animations: handheld mg42 overrides native entry 4E7133F8 (13 items) with mg34 4E713548
  (25 items)" logged at each raise; the MG42 drawn with its arms and firing on Isle (75 -> 38 rounds) through the
  diagnostic cycle (screenshots CoD3-20261030-083636/083650/083702/083708); check_snapshot.py PASSES isle-633.json
  (fails 632 isle-live.json, passes rouen-live.json). The ini was restored byte for byte. Still to do: a Rouen
  control, an 8-bot Isle check, and the bots' third-person MG42 ("looked weird").

## 0.5.632: MG42 disappearance isolation capture (diagnostic, no claimed fix)

- User confirmed PLAY-MG42-PLAIN shows the MG34 body AND hands for both
  weapons. The weapon can draw; the disabled body/bipod swap is implicated.
- Added opt-in COD3_MG42_DIAG: hold MG42 for 30 seconds without switching.
  Every 6 seconds: plain, body replacement only, bipod suppression only,
  both, then plain again. After 2 seconds each phase requests a screenshot
  and one frame log, plus bounded full mesh metadata for both view bodies,
  the source world body and replacement. Distinguishes the two mutations
  in one run, with restoration at the end. Normal behavior is unchanged.
- PLAY-MG42-DIAG.bat backs up the four rotating logs before launching;
  enables frame logging (630 lacked it), clears the plain-test override.
  No test launched by the agent. Runtime results pending.

## 0.5.631: bots' frags and smokes both on the grenade key; the MG42 in view looked at again

- The user's 630 play (2.2 min, no crash; 1 killcam, screenshots).
- Grenades: the smoke key was found as 0x2 ("smoke button found: 00000002 (slot 8, ammo
  2 -> 1)", a smokegrenade_axis thrown), and so was the frag key. 630's rule (the smoke search
  never presses the frag key) would have kept smokes off 0x2 for good whenever the frag key was
  found first. It is taken back. Across every log kept (10-23 to 10-30), the frag and smoke keys
  found were 0x2 nearly always (the host's own smoke too). The few 0x80, 0x4000 and 0x8000
  "found" came while the search wandered (other throws' spends, "4 -> 0"), and a wandering key
  is what threw a second grenade. Both searches now press 0x2 only; the weapon named in the
  throw's first command picks frag or smoke.
- MG42: 630's screenshots (00:00:21-32) holding it show no gun and no hands at all (the grey
  at the left is a wall beside him). The swap only replaces the MG34 body and hides the bipod
  (6 parts), never the hands, so the whole view model is not drawn. The frame log needs
  COD3_FRAME_LOGS=1, so none was made. PLAY-MG42-PLAIN.bat (COD3_MG42_NO_BODY=1, game folder)
  tells the swap from the weapon itself.
- Killcam: 1 more (types 0 x181, 7 x23), still no objects' events; explosions still to be found.

## 0.5.630: a bot's smoke never presses the frag key; the MG42 in view looked at

- The user's 629 play (8.7 min, no crash; 2 killcams): "killcam was looking nice bro".
- Killcam explosions, still none: in both killcams, of 146-152 objects (types 0 x117-123,
  7 x25, 14 x4), not one object's own sequence (+184) moved during the replay ("objects' own
  events: 0 new"), and no event objects. A CoD3 explosion is not in the replayed objects'
  events; where it comes from is still to be found (next: a live grenade's explosion traced).
- Grenades: the frag key was found on the first throw ("frag button found: 00000002 (slot 9,
  ammo 2 -> 1)"). No smoke was thrown all match (no smokegrenade weapon event), and the smoke
  search kept pressing 0x2 (the frag key) and 0x80 in turn, so a smoke decision on 0x2 could
  throw a frag (a second grenade). The smoke search never presses the frag key once it is known.
  Bots' smokes still do not come out; that is to be found.
- The user: "i cant see the mg42 anymore in my hands ... it shoots but i cant see it"; bots with
  it "looked weird when shooting". The MG42 body is made the same as in every log since 0.5.610
  (5849 vertices, agreement 391/204), and the MG34 body is swapped and its parts hidden as
  before. Nothing changed since 0.5.620 touches it. Once a run, 2 s into holding it, the log now
  gives the body drawn (its buffers), the next frame's every draw and a screenshot: is our MG42
  body drawn at all.

## 0.5.629: bots throw one grenade a decision; killcam explosions (2nd way)

- The user's 628 play (7.6 min, no crash, 2 killcams by client 5): "bots also i notice are
  throwing more then one grenade at a time, including smokes".
- Grenades: the bots' throw key is learnt by trial (a press, then a look at their grenades
  1.5 s on). In 628's log slot 5's sticky (key 0x2) went off 1.3 s after its press (the game's
  stickygrenade throw events at 46446906..46448234 ms), after the look: "frag button 00000002
  spent nothing ... trying the next", so every bot's frags then pressed 0x80. Slot 8's smoke
  did the same ("smoke button 00000002 spent nothing"). A press then threw another kind as
  well, or one more. The look is now 3 s after the press, and a key is given up only after two
  misses in a row.
- Killcam explosions: 628's two killcams had 0 event objects. CoD3's explosions are not event
  objects of their own: a grenade that goes off keeps its place and adds the explosion to its
  own sequence (its type may change). 628 started such an object again at its sequence, so the
  explosion counted as played. Now an object present the frame before keeps what it has played
  (whatever its type). One that comes into the list after the first frame starts one short of
  its sequence, so its newest event plays. On the first frame it starts at its sequence (nothing
  from before the replay again). The end-of-killcam line also gives the objects' own events (new,
  played) and the objects' types.
- The 628 eye and HUD changes as they were (no word on them yet).

## 0.5.628: killcam: explosions, the killer's HUD, a steady eye

- The user: "i dont see explosions in the bots killcams also the killcams could be a little
  better they still are a little bouncy"; "stay the person who killed you's true vision, hud,
  explosions and solid camera pisitionings instead of the jumping back and forth".
- Explosions: cgame's event check (8246C208) plays an event object (type +0 past 18: an
  explosion, an impact) once, when its +1000 is 0, then sets it to 1. The replay kept one +1000
  for each object's place for the whole killcam, and event objects come and go in the same
  places, so after the first one in a place every later one counted as played. Now an object's
  place starts again (0 for an event object, its sequence for others) when it was not there
  the frame before, its type changed, or an event object is somewhere else. The end of each
  killcam logs "killcam: event objects (explosions, impacts): N in the replay, M of them played".
- Steady eye, three causes:
  - The killcam's clock was GetTickCount64 (steps of 15.6 ms), and samples are a server frame
    (15 ms) apart, so the replay's moment held for frames and then jumped. It has its own clock
    now: the performance counter, in ms.
  - The killer's eye (his place, speed and view) is blended between the two samples around the
    moment, while the rest of his state (his shots) is exactly the earlier sample's, as before.
    There is no blend across a jump of more than 128 units (a respawn).
  - cgame interpolates the followed ps between the two snapshots it holds, which were given the
    killer's state at the moments they were read: past, then back. Each frame both now hold the
    eye of now (only place, speed and view).
  COD3_KILLCAM_EYEBLEND=0 turns this off.
- The killer's HUD: the HUD stays on during the killcam (cgame draws it from his ps).
  COD3_KILLCAM_HUD=0 turns it off as before.
- bots.cpp (Killcam), changed by the spectator session at the user's request.
- Self-test (BENCH-CTF-ROUEN.bat, 5m34s, 24 bots): no crash, but the host was never killed, so
  no killcam played: untested until played (grep "killcam: event objects" and "killcam: over").

## 0.5.627: SPECTATOR's string made as the game makes its own

- 626's self-test (no crash, the seat as before): no other AXIS in the text; its string holds
  its characters itself (4AF96D7C 00140004 00010000 "AXIS": +0 points at its own +12, +4 room
  20 and length 4, +8 1, +12 the characters). Ours pointed its +0 elsewhere, so what reads the
  characters in place still read AXIS. SPECTATOR's string is now made the same way.
- "menu: SPECTATOR a second on": whether the copy's string is still ours a second later.
- Self-test: the team choice reads AUTO SELECT, ALLIES, AXIS, SPECTATOR, all four in the game's
  own font and lines (screenshots\CoD3-20261029-224417.png); the string stayed ours. The run
  was closed at 2m42s, before the match part (626's ran through: free camera, no HUD).

## 0.5.626: the fourth row reads SPECTATOR

- 625's self-test (no crash; the seat, the match's seat and the way back as before): the team
  choice's fourth row is drawn by the game in its place, font and line
  (screenshots\CoD3-20261029-222340.png), but it read "AXIS": the words drawn are held in
  another string than the text's +28. Every word of the copy that points at AXIS's characters
  (or at a string whose first word does; ASCII or UTF-16) now points at SPECTATOR in a copy of
  our own; "menu: SPECTATOR:" tells what was found.

## 0.5.625: SPECTATOR drawn as the team choice's own text; the seat's HUD follows the camera

- 624's probe: the team choice draws its texts from its layer (the menu's +72, also the front
  end's manager's +372; its +12 the widgets' array, +16 count and room), not from its entries.
  The copy of AXIS's text is now put in that list too (an array of our own: the game's widgets
  and SPECTATOR's), placed one step under AXIS (the step between ALLIES and AXIS), and the
  game's array and count are put back when the team choice is not up. Same font, colours and
  highlight as the game's three.
- The user: "if i spectate a player i should be able to see hud, but if i go freecam absolutely
  no hud". In the seat the HUD now follows the camera: off in the free camera, on (his) on his
  eyes and behind him; sent only when it changes.
- Self-test: BENCH-SPECTATE-SEAT.bat (the team choice's picture, the seat in the match).

## 0.5.624: where the team choice draws its texts from (probe)

- 623's self-test (no crash; the seat and the way back as before): the fourth entry was taken
  (count 4) but its text not drawn (screenshots\CoD3-20261029-220244.png: AUTO SELECT,
  ALLIES, AXIS only): the menu draws its texts from a list of its own, not from its entries.
- "menu: seat probe" lines: where AXIS's entry and text are held (the menu, its system, the
  front end's manager, two levels), and their words, once.

## 0.5.623: SPECTATOR is the team choice's own fourth entry

- The user's 622 play (Merville CTF, 15.6 min, no crash): bots got within 300 of the enemy
  flag in 8 of 40 attacker lives, no take; their defence held. And on the team choice: "i
  dont want it its own box it needs to look like an exact game setting".
- SPECTATOR is now one of the team choice's own entries, under AUTO SELECT, ALLIES, AXIS:
  the game lays it out, draws it in its own font, highlights it, and the cross and the
  pointer move onto it as onto the others. It is a copy of AXIS's entry and of its text,
  the string made "SPECTATOR", in an entry array of our own with the game's three (the
  entries and texts are owned by the menu's lists at +88 / +96, which its teardown
  sub_823ABDD8 deletes; the array is only cleared, so the copy is never the game's to
  free). Taking it (A, Enter, a click) is the spectator's seat; that A never reaches the
  game. When the team choice is not up, the game's own array and count go back.
- The row drawn over the menu (0.5.608-615) is gone (COD3_NOSEATENTRY=1: back, and no
  entry). F6 / Back + Y still take and leave the seat.
- Self-test (BENCH-SPECTATE-SEAT.bat): its picture of the team choice shows the entry.

## 0.5.622: co-op v40 from Team Friend merged (World at War style co-op campaign)

- Team Friend's co-op v40 (their 0.5.920, source CoD3-coop-0.5.920-source.zip; design in
  docs/COOP-V40-DESIGN.md). The mirrored co-op (v35..v39: two campaigns kept alike) is
  replaced by World at War's way: only the host runs the mission; the joining player's game
  loads the level, stops its own soldiers and scripts, and shows the host's world (puppets
  moved by the host's snapshots), while he is a real second player of the host's game.
- How to play (campaign): both start CoD3.exe and press F2. One hosts, the other joins (found
  on the network, or the host's Tailscale address). In the lobby the host picks mission,
  difficulty and scoring; the friend clicks Ready; the host Starts (5 s countdown). Downed:
  30 s to be revived (hold Use beside him 3 s). Tab (Back) shows the scores. Firewall: UDP
  28964 and 28970-28972. Both players need this same CoD3.exe.
- Merged by hand: their tree (0.5.900) is not ours. Taken: coop.cpp (rewritten, 622 lines)
  and the new coop.h, coop_host/join/laststand/hud/lobby/events/probe.cpp; the lobby's hooks
  in overlay.cpp/.h (F2, MainMenuShown, Escape closes it, COD3_SHOT_OVERLAY);
  COD3_NOBOUNCE in movement_fixes.cpp; the CMakeLists entries. None of their 19 game hooks is
  hooked elsewhere in ours; what other files call of coop.cpp (CoopBeforeRelaunch, the shared
  film's CoopFilm*) is still there.
- Kept of ours: 0.5.601's real sprint (their coop.cpp's speed-raising sprint stays off unless
  COD3_OLDSPRINT=1). Dropped: our coop.cpp's mortar probe (a measuring log of 0.5.316).
  Left out: their settings.cpp change that turns the Remastered look and 4x shadows on for
  every ini once (a picture choice, not co-op: F11 > Graphics has them).
- Their tests: two copies on one PC, Forest, scripted. Untested: two PCs, other missions, a
  mission's end and the next. The old co-op (0.5.425's v38) is in the scratchpad backup.

## 0.5.621: a click on a lobby list always takes something

- 620's self-test (bench-menulearn.log): the main menu, left out of the known lists, was
  learnt from two presses (+250) and then followed the scripted pointer to rows 4, 0, 2.
- The user's 620 play (13.8 min, no crash): System Link's Create / Find Match list
  (82083D3C) was learnt by itself (+246; now known from the start). The seat in a match was
  taken and left: he was spawned again as axis with his own class. But on the learnt list
  five clicks did nothing: one came while the highlight was still on its way to the row,
  others off the rows.
- Now a click on a row whose highlight is still on its way takes it once it gets there
  (at most 1.5 s), the button held steers too, and a click over no row is A on the row
  highlighted, as on the other menus.

## 0.5.620: the lobby's lists learnt while playing

- 619's walk (bench-menufields2.log): THE MAIN MENU FOLLOWS THE POINTER: the scripted pointer
  over rows 3, 1 and 2 moved its row 0 -> 1 -> 2 -> 3, 3 -> 2 -> 1, 1 -> 2. Rows 1 and 2 open
  Create Match (8207EA5C: its row is not a small number of the menu or its rows: still the old
  mouse), row 0 Xbox Live Player Match (8207E35C, entries of its own: pointable), row 3 82071504.
- Every other list is learnt as it is used: on a menu of no entries of its own with row bars,
  a halfword under 32 that steps by one (or wraps) as its highlight moves, all else still, is its
  row; twice at the same place and that class of menu is steered by the pointer from then on
  ("menu: learnt"). Until then the old mouse (moving steps the highlight, which is what teaches
  it). COD3_MENULEARN=0 off. BENCH-MENULEARN.bat tests it on the main menu.

## 0.5.619: the scripted pointer steers the lists in a test

- 618's walk (bench-menufields2.log): the scripted pointer never moved the main menu's row:
  with the keyboard and mouse ignored (a test) the input read stops before the lists' steps.
  It now makes them there too when COD3_MENUCURSOR is set. Also seen: the main menu wraps
  (up from row 0 is row 4); row 3 opens 82071504 (4 entries) with 8207B874 behind it.
- BENCH-MENUFIELDS2.bat again: rows 3, 1, 2 pointed at, then rows 2 and 1 walked.

## 0.5.618: the multiplayer main menu pointed at as a PC menu

- 617's walk (bench-menufields.log): on the multiplayer main menu (8207E95C) each down
  press put its halfword +250 (and +362) one up, each up one down: its highlighted row.
  On Xbox Live's match options (8207EA5C) no small number of the menu's first 1 KB moved
  with the cross (+130 moved only with A and B): its rows keep it.
- The main menu is now pointed at: the row under the pointer (its full width bars, 20
  high) is reached by presses of the cross, so the highlight follows the pointer as a PC
  menu's does, and a click takes it (A). While the pointer rests (2 s), the keys and the
  pad move the highlight as before. Other lists join a table (menu_mouse.cpp, ListMenus)
  as their row's place is found.
- "menu: field" now watches the objects such a menu holds too (Xbox Live's rows).
  COD3_MENUCURSOR="t:x,y ..." puts the pointer at places of the menus for a test.
  BENCH-MENUFIELDS2.bat: the main menu pointed at by it, then a deeper screen walked.

## 0.5.617: where the lobby's lists keep their row (diagnostic)

- The user's 616 play log (front end, 85 s): the System Link game settings' arrows work
  (the map went from Merville to Abbaye du Deuil by the pointer's clicks). The menus that
  cannot be pointed at yet are lists of no menu entries of their own: the multiplayer main
  menu (8207E95C: rows as 20 high bars at 111..211), System Link (82083D3C: CREATE MATCH
  at 118,118, FIND MATCH at 118,138 over bars 111..131, 131..151) and Xbox Live's match
  options (8207EA5C: rows of objects at the menu's +440.. each with its label at +44
  and value at +16).
- "menu: field" lines: each small number of such a menu's first 1 KB that changes, to find
  its highlighted row. BENCH-MENUFIELDS.bat walks the front end by scripted presses
  (keyboard and mouse ignored) and closes itself; log bench-menufields.log.
- No change to play.

## 0.5.616: the spectator's seat without the team choice's blur

- 615's frames (bench-spectate-frames.log, COD3_FRAME_LOGS=1: the team choice, the seat
  taken from it, play, the seat in a match): the game's last full screen pass (vs
  d76d2474, target 1) is drawn with pixel program 2a90db64 behind the team choice and in
  the seat taken from it, and with 09fa43c5 in play and in the seat taken in a match.
  2a90db64 reads the frame (texture 0, 1040x624) and a quarter size blurred copy of it
  (texture 1, 260x156): the blur. The game keeps that one while it has the team choice up,
  and the seat takes the menu down without closing it.
- In the seat, that pass is given the frame for the blurred copy too
  (render_commands.cpp), so it draws the frame sharp. Logged "the blur behind the team
  choice taken out".
- Self-test (BENCH-SPECTATE-SEAT.bat): the seat's picture should be sharp.

## 0.5.615: SPECTATOR as a row of the team choice; the blur looked for

- 614's self-test pictures (screenshots\CoD3-20261029-115553.png, 115559.png): the team
  choice is AUTO SELECT, ALLIES, AXIS as light text rows on thin gold lines, its 3D
  background blurred; in the seat the whole scene stays that blurred. The F11 option for
  the aim's depth of field is off (aim_depth_of_field = 0), so it is not that pass: it is
  the team choice's own background blur, left on as the game still has the menu up.
- SPECTATOR is now drawn as the team choice's fourth row, as its own: the same left edge
  and row height under AXIS, the same light text (gold under the pointer), a thin gold line.
- The seat's self-test takes a picture and a frame's draws in play and in the seat in a
  match too; BENCH-SPECTATE-SEAT-FRAMES.bat runs it with the frames logged in full, to tell
  the blur's draws. Not fixed yet: the blur.

## 0.5.614: the free camera turns in the seat; the lobby's screens take the mouse again

- The user, on 0.5.613: in the spectator's seat from the team choice "you cannot fly left
  or right only up and down" and the background "stays blurry"; the multiplayer menus
  could not be clicked where wanted (game settings, the map's arrows); and Spectator
  should be "its own box like how allies or axis is".
- The free camera has its own aim now: from the view when it starts, turned by the mouse
  (mouse_look.cpp hands its movement to the spectator while the free camera is on) and the
  right stick. In the seat the game's spectator script holds the view still, so the camera
  had read a view that never turned.
- The lobby's screens (613's play log: menus of 0 entries of their own, 8207E95C, 8207EA5C,
  82083D3C ...): where the pointer finds no entry, arrow or button the mouse works as before
  0.5.595 again (moving steps the highlight, a click is A); 595-613 had left it doing nothing
  but A there. Each menu class's shape goes to the log once ("menu: shape": its entries, its
  texts with where they are and what they read, its pictures), to make those screens
  pointable from the next played log.
- The seat's self-test now takes a picture and a frame's draws on the team choice and in the
  seat, to find the blur. Not fixed yet: the blur, and the Spectator box's look.

## 0.5.613: F6 is the spectator's seat everywhere; the multiplayer's menus free the pointer in a match

- 612's self-test (bench-spectate-seat.log): LEAVING THE SEAT IN A MATCH WORKS. He was
  spawned at a point of his side (sub_822029F0, as the bots are), state 3 a frame later
  with full health, and played on (died, was wounded, respawned) to the match's end. The
  team-choice seat worked a fourth time.
- F6 (Back + Y) now takes the spectator's seat in a match too, not only on the team choice:
  your soldier is taken out (that life ends) and you are the game's spectator, no HUD, only
  the camera; F6 again spawns you at your side's point with your kit. The spectating that
  leaves your soldier standing is F11's buttons (F6 ends it). F11 > Multiplayer >
  Spectator has a "Spectator's seat" button too.
- Multiplayer menus in a match (the pause menu, the kit choice after a death) let the
  pointer go for clicking, and the mouse is taken back for looking when they close, as the
  campaign's do: the self-tests' logs show no menu up during play.
- Self-test (BENCH-SPECTATE-SEAT.bat).

## 0.5.612: leaving the seat in a match spawns you, as the bots are spawned

- 611's self-test (bench-spectate-seat.log): the seat on the team choice and in the match
  both worked again; leaving it in the match kept side 2 and kit 2 and went the
  session's way (sub_823EA4E0), but he stayed a spectator (state 1, health 0) for the
  remaining 120 s.
- Now, still a spectator after that, he is spawned the way the bots are (bots.cpp,
  Respawn): the game's spawn at one of his side's points, sub_822029F0, given his entity
  number and the side's name ("allies" for side 1, "axis"). Logged "spawned at a point".
- Self-test again (BENCH-SPECTATE-SEAT.bat).

## 0.5.611: leaving the seat in a match joins you again, as you were

- 610's self-test (bench-spectate-seat.log, no crash): the team-choice seat worked a third
  time and the seat in a match again (state 3 -> 1, kept 60 s). But the team choice put up
  again after it was never updated or drawn during play (no "menu: up" for it), so he
  would have stayed a spectator.
- Now, taking the seat in a match keeps your side and kit (the team record's +4, the
  record's +1920), and leaving it joins you again with them through the game's own side
  and kit callbacks, as the host's team and kit menus join a player (Callbacks
  0x82BCBF64 +3320 / +3328, given the entity number; in the session's mode
  sub_823EA4E0, as bots.cpp's PickSide / PickKit). The team choice only when no side was
  kept. Logged "joined again as side N, kit N".
- Self-test again (BENCH-SPECTATE-SEAT.bat).

## 0.5.610: the team choice back after the seat in a match

- 609's self-test (bench-spectate-seat.log; 5.2 min, 80 fps median, no crash): THE SEAT IN
  A MATCH WORKS. The game's own SpawnSpectator took the host out of play (state 3 -> 1,
  health 0, no body), the game kept him so for the whole 60 s (no respawn), and the
  cameras ran: free, then his eyes and behind him over four bots in turn. The team-choice
  seat worked again as in 608.
- What was left: leaving the seat in a match left him the game's spectator with no menu
  to join from. The team choice is now put up again (its system and place as seen when
  he joined; the menus' system 8268A950, menu 10 in the test), to pick a side.
- The "call through a pointer to 0x00000000" lines in these logs are not the seat's: the
  main session's runs without it have them too (26-57 a run).
- Self-test again (BENCH-SPECTATE-SEAT.bat): "menu: the team choice put up again".

## 0.5.609: the spectator's seat in a match too

- 608's self-test (bench-spectate-seat.log; 5.2 min, 72 fps median, no crash): THE SEAT
  ON THE TEAM CHOICE WORKS. Before a side is picked the host is the game's spectator
  (state 1, followed client -1, pm_type 4); the seat took the team choice down once and
  the game never put it up again in 40 s, the state stayed 1; leaving put it back, the
  test joined (the kit choice came, then in) and played 240 s with 24 bots.
- Taking the seat in a match never happened: the only caller was the menu systems'
  update, which runs only while a menu is up. mp_fixes.cpp's frame (MpFixesLauncher)
  now calls the spectator's frame too (one line, SpectatorGameFrame).
- Self-test again (BENCH-SPECTATE-SEAT.bat), for the seat in a match.

## 0.5.608: one throw a side at a time; carriers hop raised edges; the UAV plane under the sky

607's two 5-minute 8-bot tests (BENCH-CTF-ROUEN-SMALL, now 300 s, the user's call):
0 takes, then 2 takes and 1 capture; stalls 13.8% and 11.1%. Carrier slot 4 left the
enemy base by its way out in one go (607's once-a-life), but at 900 700 his route's
next spot was 68 off, the step "blocked" by ground 0.98 up at his feet (a raised edge),
and he detoured 160 off and back for 10 s: never stalled long enough to try the jump.
- Grenades and smoke (the user: bots "throw multiple" smokes or stickies): a side's
  bots throw one at a time, 3 s apart within 900 of the last (SideThrowFree); 0.5.454's
  2.5 s gap was each bot's own, and a side's wave going in threw smoke all at once
  (607: slots 4, 6, 8 and 10 in the same instant). Our own orders are one a bot (the
  game's weapon events in the log are the host's own, not the bots').
- A step blocked by a raised edge (the feet's trace meets ground over 0.7 up) tries
  the jump over it at once (was only after stalling).
- The UAV plane (the user: it "disappears sometimes when circling the map"; 0.5.260:
  an entity over the sky's brush is outside the world, not linked: not drawn, not
  sent): each step of its circle a line up from 900 over the players finds the sky
  there (BotsSkyZ, bots.cpp; anything under 1000 over the players is a roof, not the
  sky); the plane flies at most 200 under it, never under 600 over the players, at
  most 150 up or down a step. Logged ("under the sky") when that moves it.
- COD3_UAVTEST_AT=N: the enemy UAV plane N s into a match and every 70 s after (a
  self-test; BENCH-CTF-ROUEN-SMALL sets 30).

### Also in 0.5.608: the spectator's seat on the team choice (side session)

- The user: "as its own option for when we spawn into a game similar to how you pick
  allies axis or auto assign ... no map, no hud no nothing just pure camera".
- Under Allies / Axis / Auto-assign there is a SPECTATOR button (click it, or F6 / Back +
  Y while the team choice is up). Taken, you are not a soldier at all: the game keeps you
  as its own spectator, its menus are taken down, the HUD is off (cg_draw2D 0), and only
  the camera is left: Y free camera / his eyes / behind him, fire / aim next / previous
  player (bots and people), the free camera moved by move, look, jump up, crouch down,
  sprint fast. F6 (Back + Y) leaves the seat and puts the team choice back, to join.
- Why this works where 0.5.602's state write did not: every player who joins is the
  game's own spectator until he picks a side and a kit (the scripts' CallbackPlayerJoin,
  CallbackPlayerEnter and StartRound call SpawnSpectator, sub_82214B30: session state 1
  through the engine's builtins, no followed client, a spawn at a spectator's point; the
  local player's thread SpawnLocalSpectator, sub_82214EB8). The seat keeps you so by not
  picking a side; nothing is written under the scripts.
- In a match already, taking the seat calls the game's SpawnSpectator for you first.
- menu_mouse.cpp: the game's menus can be taken down (their system's active menu -1) and
  put back; the team choice is vtable 82084854. Self-test: BENCH-SPECTATE-SEAT.bat.

## 0.5.607: no going round in circles out of a base

606's 8-bot test (1 bot capture-free carry each side, 2 takes by the host, 1
capture), with the half-second carrier trace: carrier slot 7 (side 2, flag 0 from
-1792 -91 home to 2112 462) went round in two loops. In the base he took it from,
the way out (0.5.584) was picked six times, each from another spot of another way,
often behind him (his velocity turned round at each: -208, +195, -203). Outside it
(-1200 50) the way's end came, his learnt-map route home started at the nearest
walkable spot, -1205 68, behind him, back he went within 600 of the flag, the way
out's last three spots again, and so on. The stuck help put him on 4 times.
- A base's way out (the carrier's out of the enemy base, 0.5.584; anyone's out of
  his own, 0.5.596) is taken once a life.
- A route made over the learnt map starts at the walkable spot (of the eight nearest)
  with the least way round toward the goal (to it plus from it to the goal), not the
  nearest one.

## 0.5.606: carriers traced every half second

The user: fewer bots, so it is not as chaotic and we see whether the bots really
walk their paths. BENCH-CTF-ROUEN-SMALL.bat (8 bots, 4 a side, 10 minutes,
bench-ctf-rouen-small.log). Its first run (0.5.605): 4 bot takes (2 by side 2), 2
captures (slots 4 and 7, side 2), stalls 8.4% (24 bots: 7.2-8.6%: the paths, not the
crowd). Every carrier was still put forward by the stuck help, 2 to 4 times, with
almost no one shooting. Slot 4 ran at 200 for 12 s at a "walkable" route spot 330
off and stayed round -1700 350; the 3 s ctftrace cannot show why. (Two carriers'
trails began by the enemy flag, where the port's Respawn puts them before moving
them home: those took the base's ways out instead.)
- A carrier, every half second: "carrytrace:" his place, velocity, his route's next
  spot (how far) and how many are left, and how long since it was made.

## 0.5.605: "Leave the match while spectating" taken back out (604's self-test)

- 604's self-test (bench-spectate.log, 4.6 min, 76 fps median, no crash): with the
  host check fixed, the switch put the player in state 1, and the game put him back
  in state 3 the very next frame, 6860 times in 90 s as it was tried again; the first
  change also moved him across the map (1991 557 to 1042 -1196: a respawn). He was
  back in the match at the end. The state belongs to the match scripts (their
  sessionstate), which keep it; writing it under them does not work.
- The switch is no longer in F11 (only with COD3_SPECTATE_LEAVE=1, for later work
  on the scripts' side), and it now gives up after the first refusal while
  spectating instead of trying again every frame.
- Spectating itself is unchanged: free camera, his eyes (the gun kept with the
  view since 0.5.604) and behind him; your player stays where he is.

## 0.5.604: spectator: the gun kept with the view; "Leave the match" on any host

- 603's self-test (bench-spectate.log, Rouen CTF, 24 bots, 5.4 min, 89 fps median,
  no crash): following from his eyes worked for 90 s over 6 bots. Two findings:
- THE GUN OUT OF FRAME (user, 592): in a bot's quick turn the camera (yaw 137.3),
  the record it was made from (145.3) and cgame's player state, which the gun is
  placed from (153.3), were each one step of the turn apart (8 degrees a server
  frame): the gun pointed up to 16 degrees off the view and swayed with the jumps.
  Bots turn far faster than people, so only some showed it. The camera's angles,
  once made, now go into cgame's player state, so the gun is placed and sways as
  the view turns. COD3_SPECTATE_GUNSYNC=0 turns it off.
- "LEAVE THE MATCH" never switched on in the test: its host check was the lobby's
  (false in a match not started from the lobby). Now the bots' own "this machine
  runs the match" check.
- 603 also showed no multiplayer menu "up" during play (the menu lines stop at the
  match's start), so the menu pointer leaves play alone.
- Self-test again (BENCH-SPECTATE.bat).

## 0.5.603: a self-test of the game's spectator state

- COD3_SPECTATE_TEST=from,to[,mode]: from 'from' to 'to' seconds after a level's
  first frame the host spectates (mode 2, his eyes, by default) with "Leave the
  match while spectating" on, the next player every 15 s; logged "spectator: test:".
- BENCH-SPECTATE.bat (game folder): Rouen CTF with 24 bots for 240 s as
  BENCH-CTF-ROUEN.bat, spectating from 60 s to 150 s; log bench-spectate.log.
- No change to play otherwise.

## 0.5.602: multiplayer menus by mouse; the game's own spectator state

- MULTIPLAYER MENUS BY MOUSE (as the campaign's, 0.5.595): point at an entry and it is
  highlighted with the move sound; a click takes it (over nothing: the highlighted
  one); arrows, option values and sliders step; dialogs' buttons. Right click and
  Escape stay Back, Page Up/Down the bumpers. In a match the pointer is not let go
  by itself (Escape frees it, as before) until a log shows which menus are up there.
- How it was found: the multiplayer's front end is the campaign's classes built again
  with other virtual slots, so none of its functions is a twin; both images were
  dumped (COD3_IMAGEDUMP, kept out of the repo) and matched by their vtables:
  60 menu classes with the move sound at +28 and SetSelection at +192
  (sub_824E9170, 0.90 of the campaign's; its cross handler sub_824DDFA8 calls +28
  then +192); menu, entry, FEImage (0x820705B4, draw an exact twin) and FEText
  (0x820702DC) fields all the campaign's; a system's menus at +4 and the active one
  at +28; the update at +52 of five system classes, all calling sub_824D5FC0
  (hooked); the dialog system 0x82071478 (its buttons found by their text); quads
  drawn by sub_824D7E60.
- SPECTATOR, "Leave the match while spectating" (F11 > Multiplayer > Spectator; host
  only, experimental, off by default). Found the game's own spectator: a player's
  session state (record +1924) is 3 playing, 4 dead, 1 SPECTATOR; the game's follow
  cycle (sub_82560928) works only in state 1 and steps through players in state 3;
  +1344 is whose view a player has (his own number, or the followed player's). With
  it on, spectating puts your player in state 1 (not drawn, not hit, the bots look
  for state 3 only) with +1344 the followed player; stopping puts him back in
  state 3 where he was, aiming as he was. If the game changes the state meanwhile
  (a round's end), it is left to the game. Log: "spectator: game's spectator:".
- Not tested yet: (1) the multiplayer lobby and menus with the mouse; (2) spectate
  with the new switch on, see that bots ignore you and that you come back where you
  left.

## 0.5.601: the campaign's own sprint, as FrankHUN88453 put it back

- Campaign only. The single player keeps the whole of the multiplayer's sprint
  (the run, the weapon lowered, the fatigue); only its +sprint / -sprint commands
  were emptied. They are given their multiplayer bodies again (they press the
  sprint key button, 0x829C8E68 +360), as FrankHUN88453 did (ac3b2a52,
  sprint.cpp, ported to CoD3Host/sprint.cpp).
- Sprint is held as before: L3 (or the sprint key) while going forward, or tap
  to sprint (F11). It now sends the game's own +sprint, so it has the sprint's
  run and animation and runs out of breath as the game decides.
- The fatigue meter: the stance figure (bottom of the HUD) fills from the feet up
  with the stamina left while sprinting or getting the breath back, as the
  multiplayer shows it (the single player's widget lost that draw).
- The co-op work's sprint (g_speed raised 1.5 times, coop.cpp) is off;
  COD3_OLDSPRINT=1 brings it back, COD3_NOREALSPRINT=1 leaves the game's empty.
- Not tested yet. Log: "sprint: +sprint (the game's own sprint)".

## 0.5.600: the mouse pointer can be shown; menu clicks never do nothing

- 599 confirmed: the campaign starts again ("the merge worked great").
- The pointer (user: "toggle on or off so i can see my mouse cursor"): F11 > Controls >
  "Hide the mouse pointer while playing" (hide_cursor in CoD3Recomp.ini, on by
  default as before). Off: the pointer shows over the game window. On or off, it
  now always shows in a menu with the mouse free (the front end, the campaign's
  pause menu and a mission's end), so the menus can be pointed at. Team Friend's
  look hid it whenever the window was in front, menus too.
- Campaign menus (user, 599: the left click "didnt register" in the pause menu's
  save and quit): the pointer was hidden, and a click only acted over the entry
  it was found over. Now a click over no entry, arrow or slider takes the entry
  highlighted (A), as a click did before 0.5.595.
- Log: "menu: up:" for each menu that comes up (its class, entries, the mouse held
  or free) and "menu: click:" for what each click did, so a menu that still does
  not take a click shows why.

## 0.5.599: the campaign froze at the legal screen (the look's programs compiled on a thread)

- 598's campaign froze at launch (user: "tried to but froze"). CoD3.log: the legal
  film opened, then "guest: The GPU is hung!" (the title's own GPU timeout,
  822EC230), and "render: the look programs are ready" / "the look's targets made"
  only came after it. The first film turns the look on (film smoothing and
  sharpening are on by default, RenderPost::Active), and its 13 programs were
  compiled right there on the drawing path with D3DCompile, which stopped the
  drawing long enough for the title to give up on the GPU. The campaign had not
  been run since 0.5.467, before the look came in (0.5.555); the multiplayer
  program has no film at its start.
- The look's programs now compile on a thread of their own; until they are ready
  the picture goes without the look (a film's first second or so plain).
- The menu mouse (0.5.595) was not on the frozen thread's stack; it stays in.
- Not tested yet: start the campaign and see the legal and Activision films and
  the menu come up.

## 0.5.598: a standoff sends the attackers after our flag

597's natural 10-minute match: a standoff. Side 1's slot 6 took flag 1 at about 1
minute and held it at his own flag (-1800 -200) to the end, more than 7 minutes,
never able to capture: side 2's slot 27 had taken flag 0 (and again after a death).
Stalls read 15.6%, but 338 of 630 were that: the two carriers waiting at their flags
and the bots round them (every attacker of a side escorts its carrier; only the
defenders went after the enemy carrying their flag). The back-and-forth test
(0.5.597) never fired.
- Our carrier within 600 of our flag and our flag out (carried or lying): two
  attackers (formation places 0 and 1) stay by him, the rest hunt the enemy carrier
  ("hunts the enemy carrying his flag (standoff)") or return it if it lies out.

## 0.5.597: going back and forth counts as stuck

596's natural 10-minute match (its first run crashed loading, before any bot: a
memset in sub_82137C38 from sub_82142248 wrote past committed guest memory; the
rerun was clean): stalls 8.6% (594: 8.1%); its own base's stalls halved (60 bots
left it by a way out), the rest spread over many places. Of the 318 stall samples,
177 had no stall or strike counted by the bot himself and over half were moving at
100-250: going back and forth. The stuck test (0.5.196: not 56 from where he was
3.5 s ago) starts again whenever he gets 56 off, so a swing wider than that never
counted.
- Also stuck: still within 200 of where he was 8 s ago while he wants to move and
  his goal is over 300 off; the usual remedies follow (another way, a jump or hop,
  a wander). Counted in the log once in 25.

## 0.5.596: bots leave their own base by its ways out

594's natural 10-minute match: stalls 8.1% of samples going somewhere (591: 8.7%);
the place 600-800 x 1200-1400 halved (42 -> 21) with bad links dearer. 3 bot takes,
0 captures, 2 returns; bots 0.94 ms a frame. 75 of the 302 stalls were side 1's
bots in and by their own flag's building (-2000..-1700 x -400..0): its route spots
past its walls ("first (none walkable)", "on the route within 200 (not straight)",
no step at all).
- Any bot within 350 of his own flag (at home) going more than 700 from it leaves by
  one of its base's ways out (0.5.582's trails), forwards, the one ending nearest
  where he goes; then his route from there. Logged ("leaves his base by a way out").

## 0.5.595: campaign menus by mouse (ported from FrankHUN88453)

- Its build came out labelled 0.5.596 (the version line moved while it built); every build from 0.5.596 on has it.
- Campaign only (CoD3.exe). Point at an entry of the game's own menus and it is
  highlighted, with the menu's move sound; a left click takes it, only the entry
  under the pointer. Arrows and an option's value: a click steps it, held repeats.
  Sliders (volume, sensitivities): the left half lowers, the right half raises.
  The chapter select's strip: a chapter clicked scrolls to it, the middle one takes
  it. Dialogs: their buttons. Right click and Escape go back (B).
- In a level, the pause menu and a mission's end let the pointer go while they are
  up, and the mouse is taken back for looking when they close.
- Ported from FrankHUN88453's CoD3Recomp (ac3b2a52, menu_mouse.cpp) into the new
  CoD3Host/menu_mouse.cpp; input.cpp's campaign blocks from his. Left out: his VIDEO
  page, his Quit entry and his test scripts. Where the menus are in the window
  follows this tree's fill-the-screen (the largest 16:9 in the middle).
- Multiplayer unchanged: its menus are another system (its menu update, the
  highlight's move and the cross's steps have no twin in CoD3MP).
- Not tested yet. COD3_MENULOG=1 logs what the pointer is over ("menu:");
  COD3_NOMENUMOUSE=1 turns the pointing off.

## 0.5.594: a bad link costs 20000 (bots bumping into walls)

The user, playing 592: "some bots still seem confused walking into walls". Their
log: of 2436 samples of a bot going somewhere (two ctftraces 3 s apart, both "to
goal"), 202 (8.3%) had him under 30 from where he was: every role (attackers 106,
middle 50, cover 28), so not the defenders' doing. The worst place, 600-800 x
1200-1400 (30): bots by 610 1280 had 453 1090 next on their route, no straight way
there (no step at all, legs 0 0), the route made again sent them to 622 1277, then
453 1090 again. That link is a player's move a bot cannot walk, marked bad (0.5.554),
but a bad link only cost 2500 more, and the way round on Rouen is often longer.
- A bad link costs 20000 more (was 2500): any real way round wins; one with no
  other way is still kept. tools/ctfreport/stalls.py counts the stalls.

## 0.5.593: spectator: "Smooth his eyes" (the bots' gun out of frame)

The user, on 0.5.592: following some bots from their eyes, their gun was not in
frame properly. Suspected cause: the view used a smoothed copy of his place and aim,
while the gun comes from cgame's own snapshot interpolation, so a bot's quick turns
put the two apart. (spectator.cpp, spectator.h, overlay.cpp; the spectator session's
work.)
- F11 > Multiplayer > Spectator > "Smooth his eyes" (on by default) turns that
  smoothing off live, to compare.
- Logged: "spectator: eyes" twice a second while following from his eyes: the
  camera, the record it was made from, cgame's ps (cg+3136, where the gun is placed
  from) and his live record (weapon, ADS, flags, pm_type).

## 0.5.592: a carrier goes into his own flag's building by its ways out

591's natural 10-minute match: 3 bot takes, 2 captures (bot slot 21 home on his own
trail, 94 spots; the host for side 2), bots 0.86-0.97 ms a frame (590: 1.0-1.1),
routes "lost" 1297 times (590: 1511). Slot 21 still needed the stuck help 3 times,
the last 207 from his flag, outside its building ("put by his flag").
- A carrier within 700 of his own flag (at home) goes in by one of his own base's
  ways out backwards (as attackers go into the enemy's, 0.5.582), once a life, over
  his own trail's route. Logged ("carrier, home").

## 0.5.591: up a hill counts as walkable up to 384 off

590's natural 10-minute match: 3 takes (side 1), 0 captures. Two carriers now took
their own trail home (28 and 44 spots; their trails began 2600 from home, mid-map),
but in Rouen's middle (100-600 x 400-1000) they went back and forth and the stuck
help put them on 6 times: their route was "lost (none walkable)" every few seconds
(1511 the match). The traces met the ground (normal 0.90-0.99 up): it is a hill.
Reachable's flat sweep fails on any slope past 8 degrees, and its walk in 48 pieces
went to 4 pieces at most (192), while the route's spots are looked for up to
350-400 off: on a hill, none of them was walkable.
- Reachable walks a slope in up to 8 pieces of 48 (384). Stairs (16 x 12) as before.

## 0.5.590: a carrier's own trail taken if it heads home

589's natural 10-minute match: 3 takes (side 1), 0 captures, 1 return. All three
carriers logged "his own way home: 0 spots (none: not begun by his flag)": the trail
asked for (begun within 1500 of his flag) was never there (a revived bot's life, a
port move, starts it over mid-map).
- A carrier's trail this life is his way home if it has 4 spots or more and began
  at least 500 nearer his flag than the flag he took; past its first spot his route
  goes on over the learnt map. Each carrier's trail (spots, where it began) logged.

## 0.5.589: carriers go home the way they came

588's natural 10-minute match: 1 take, 1 capture (slot 6), but the stuck-carrier help
put him forward 4 times. He was not stuck on walls: his learnt-map route home was
"lost" and made again from wherever he stood, each time another way (697 1272, then
1113 1244 3 s on, 400 back east; -8 774 then -124 1382 north), so in 10 s he got
nowhere.
- At the take a carrier's own trail this life (BotTrail: a spot every 120 walked,
  from his spawn) is kept as his way home, if it began within 1500 of his flag. His
  route is that way backwards from the spot of it nearest home he can walk to
  straight (within 400), then his flag; taken again a second after any route made
  over it. Logged. (Before 0.5.568 CarrierHome walked the trail back without the
  route's walkable checks; this goes through them.)
- Otherwise (no such trail) 0.5.584's ways out of the base, then the learnt map.

## 0.5.588: a flag is never "taken" by its own side

586's carrier test (attackers put by the enemy flag every 60 s): 15 bot takes and
1 capture - by the host (slot 0), who picked flag 1 up where a bot carrier had
dropped it and brought it home; side 1's two bot carriers were killed mid-map
(-122 918, -456 381). But from 3.5 minutes in, flag 1 was "TAKEN" 13 times by side
2's own defenders standing by it ("not pressing"): the take goes to the nearest
living player, and the real taker (a side 1 attacker the test put there, 234 off)
was further away. The stuck-carrier help then put those defenders on their own
flag every 10 s for the rest of the match.
- A take goes to the nearest living player of the OTHER side (a bot's side by
  BotSide) within 250 (was anyone within 200); the flag weapon's holder still wins.

## 0.5.587: Team Friend's look update 2 (film player)

look-update-2.zip (Team Friend, on top of look-for-justin.zip, merged in 0.5.555).
Compared three ways (their first zip, this one, ours): of its 8 source files only
film_hd.cpp differs from their first zip; the rest (Film shadows, fill the screen in
multiplayer, the cursor hidden while playing, films toned down) were already in it,
so already in ours since 0.5.555, with our own changes kept (HDR, Unstick, the film
pacer's sync, PresentLocked). Taken in whole:
- film_hd.cpp: the remastered film player draws and matches on the film's 16:9
  rectangle when Fill the screen is on (matches failed on the black bars), match
  threshold 0.85, the overlay mask keeps only light text compared blurred.
- tools/remaster-films.py (--out, skips finished films), tools/upscale-textures.py
  (--two-from, cool-down breaks).
Ours before it: scratchpad backup-0.5.586. Remastered films are game data, not sent.

## 0.5.586: the stuck carrier is never put back toward the enemy base

585's self-tests: the first (no ways out saved yet) 1 take, 0 captures; it saved 32
ways out (16 a flag). The second read them in (32 put to their flags) and the
carrier took one: slot 6 went out of the base at 2112 462 by 2051 712 -> 1487 905.
But 10 s stuck at 1021 1320, the stuck-carrier help put him "back along his own
trail" at 1998 1211, 1000 back by the enemy base; later he crawled at 440 1000
(velocity 210 to a walkable spot, 45 units in 3 s; cause not found) and was killed.
0 captures, 1 return.
- The stuck-carrier help steps back along his trail only to a point within 300 of
  him (out of a dead end); otherwise it puts him forward along his route home.

## 0.5.585: the ways out of each base are kept between games

584's natural 10-minute match: 2 takes, 1 real capture ("flagwatch: flag 1 CAPTURED
by slot 27"; the stuck-carrier help moved him twice on the way: along his route,
then by his flag 208 out). No carrier took a way out: the base he took the flag
from (2112 462) had 0 ways known at the first take, 1 at the second (3 the whole
match; flag 0's: 12): side 2's players seldom start out within 200 of their flag.
- The ways out are saved with the map knowledge (bots\maps\<map>-ctf-ways.txt, a
  line each) and read in at the next game on the map, each put to the flag within
  350 of its start.
- A way out starts within 300 of the flag (was 200).
- A carrier in the base who takes none logs why, once a life.

## 0.5.584: carriers go out by the ways out; captures logged as such

583's natural 10-minute match (its first run hung the GPU at the menu before any
bot ran; run again): 3 takes, the first by side 2 (slot 26, the flag at -1792
-91), and "to goal, shooting" 27 times. No capture. CORRECTION: the self-test
summary counted a render line ("captured programs") as a capture: 579-582's
"1 capture" each were that line, not captures (those logs are gone). Captures are
now counted as a flag going home straight from carried (no drop between).
Carriers: slot 4's route out of the base at 2112 462 ran into a wall (1856 344 ->
1726 356); the stuck help put him back 10 s on, and he was shot there.
- flagwatch logs "flag N CAPTURED by slot S" (home straight from carried).
- A carrier within 600 of the flag he took goes out by one of that base's ways out
  (0.5.582's trails), forwards: from a spot of one he can walk to straight, the
  way ending nearest his own flag, to its end; his route home from there.

## 0.5.583: attackers shoot on the run to the flag

582's natural 10-minute match: 3 takes, 1 capture (side 1), 83 fps. The ways out of
each flag's base were kept (19) and taken backwards 7 times, side 2's by the right
way into the building at -1792 -91, but they died on the way: side 2's three
attacker deaths nearest that flag (211, 242, 418 from it) were all running "to
goal", shot by its defenders in cover 130-300 off. Running for the flag, a bot
looked at the flag and Think returned before its aim and fire: no shot back.
- A CTF attacker (not a carrier) running for the flag with an enemy in sight within
  900 looks and fires at him while his legs keep to the flag's way (TacticalMove's
  move-to point, as flanking uses), weaving, no sprint while firing. Traced as
  "to goal, shooting".

## 0.5.582: attackers go in by the ways out of the flag's base

581's natural 10-minute match: 1 take, 1 capture (side 1), 82 fps. Side 2 had 4
lives within 300 of the flag at -1792 -91, all outside its building's east wall
(x -1520..-1580): the route ran down to the flag itself and they went straight at
it into the wall ("no route spot (1 left)": 13 of 14 samples).
- Each flag's ways out: a player walking from within 200 of a flag at home is
  followed a spot a half second till 650 out; the last 16 kept a flag. Side 1's
  bots walk out of that building every life.
- A CTF attacker within 700 of the enemy flag (at home) takes one backwards: from
  the spot he can walk to straight that is fewest spots from the flag, then the
  rest of it to the flag. Once a second till he has one; logged.

## 0.5.581: attackers by the enemy flag don't fall back

580's natural 10-minute match: 1 take, 1 capture (side 1 again); side 2's routes
into the building at -1792 -91 now run along the real way in (its south door:
-1793 -396, -1795 -304, then the flag; the way side 1's bots walk out), but few got
there: 12 samples of an attacker within 450 of the enemy flag in 10 minutes, and
there slot 4, 273 from the flag on that way in, fell back behind smoke (health 35
or under) and died 300 back. 100 links were walked through a player's step
(2601 steps known by the end).
- A CTF attacker within 450 of the enemy flag doesn't fall back hurt; he goes for
  the flag (the grab is a touch).

## 0.5.580: routes go through the doorways players used

579's natural 10-minute match: the first natural capture (side 1, slot 14; the
stuck-carrier help moved him 4 times on the way). Side 2 still never got into the
flag building at -1792 -91: its route ran -1795 -304 -> -1675 -148, two learnt
spots with the building's wall corner between them (the link came from a side 1
bot walking out, but not along that straight line), the bot hit the wall, the
route was "lost", worked out again round the east side, and so on (lost 1179
times). The side 1 bots spawning there walk out by the east (-1600..-1536,
y -192..-128) and north.
- The learnt map now keeps, for each link walked this session, the half second's
  step that made it (two real standing spots). A link not straight from cell spot
  to cell spot is kept if the bot can get to that step's start and from its end;
  the route then goes through those two spots instead of being marked bad.
- Every link within 700 of a flag is checked when a route is made (was only the
  first six).
- The bad-links-by-flag log: 50 a flag (579's 80 were all at flag 1).

## 0.5.579: what the routes into the flag buildings do (log)

578's natural 10-minute match: stairs walkable at no cost (bots 1.0 ms a frame, 56
traces); 11 attacker lives within 700 of the enemy flag (577: 5), side 2's first
three within 300 of the flag at -1792 -91; 2 bot takes (side 1), both carriers
killed about 2 s on. But side 2's "within 300" were still outside the building's
east wall (x -1511..-1575): their routes flipped between a way in on the north
(-1571 234) and the south (-1702 -354), and with the route's last spot left (the
flag) they went straight at it into the wall. Only a route's first 6 links are
checked when it is made, so the last ones into a building never are.
- Logged: each learnt-map link found not walkable within 600 of a flag (both
  ends), and once a life an attacker's route's last four spots when he comes
  within 400 of the enemy flag.

## 0.5.578: stairs are walkable (a step at a time)

577's check: no crash, 85 fps; side 2's attackers still held at the flag building
at -1792 -91. The learnt map has well-used ways in on every side (10278 and 9808
visits at the busiest), but the floor there is at z 42 and the streets at z 6-8:
stairs. Reachable's Climb goes in pieces of 48 and allows only 18 of rise onto a
flat landing (0.5.206, so crates are not climbable); a 48 piece over a flight of
8-16 high steps rises 24-48 onto a flat tread, so stairs were "not walkable" and
the step's hull met the risers (normal up 0).
- Reachable tries Climb again in pieces of 16 (up to 12: the same 192) when the
  48 pieces fail: each stair then rises its own height; a crate higher than a
  stair still fails on its one piece.

## 0.5.577: the learnt map leaves out falls and the port's teleports

576's natural 10-minute match: one bot take (slot 6), the carrier killed 260 out of
the enemy base; 4 attacker lives within 300 of the enemy flag; side 2's attackers
still held at the east wall of the flag building at -1792 -91.
- A player's place is not recorded on the learnt map while he falls fast (over 300
  a second up or down) or within 1.5 s of this port teleporting him: 575's map had
  spots at z -29388..-13524 under the flag at -1792 -91 (a fall through the world),
  and teleports are not ways anyone walked.

## 0.5.576: within 300 of the enemy flag an attacker dashes for it

575's assault test, clear-first off: 5 takes, 2 full captures (slots 22 and 14, each
about 31 s home across Rouen) in about 10 assaults (574 with clear-first: none). The
natural 10-minute match then: 4 attacker lives within 300 of the enemy flag (the
first ever), but no take: slot 4 stood 147 from the flag for half a minute in Think's
fight (an enemy within the runner's 250 there always); side 2's attackers circled
220-320 from the flag at -1792 -91, held by the building's east wall (x about -1600).
- Within 300 of the enemy flag an attacker always runs for it (the grab is a touch
  at 60), as a carrier always runs home.
- Seen: learnt-map spots at z -29388..-13524 under that flag (something fell
  through the world there, likely the carrier test's placing): not linked, so not
  routed over; to be cleaned.

## 0.5.575: clear-first off again (measured)

574's assault test (waves of three put 500-800 from the enemy flag, about 10
assaults): no take. The attackers stalled 350-400 from the flag (one got to 247),
mostly in Think's fight, trading shots with defenders in cover, till killed.
- The clear-first rule is off unless COD3_CTF_CLEARFIRST=1; the same assault test
  is run again without it, to compare like with like.

## 0.5.574: the assault clears the flag before the grab; the test as a real assault

573's carrier test: another full capture (slot 27, 11 s), one carrier lived 61 s
(but made only a third of the way); the other 10 died 1-6 s after the take, at the
enemy flag, shot by its cover defenders, escorts or not. The test put the taker ON
the flag with every defender still in place, which a real assault does not.
- Within 700 of the enemy flag an attacker fights an enemy in sight within 600
  before going for the flag (the wave clears the defenders); 700-1600 he still runs
  on (0.5.566).
- The carrier test (BENCH-CTF-CARRY.bat) now puts the wave of three at learnt-map
  spots 500-800 from the enemy flag on the home side: they fight in and take it
  themselves, so takes, carries and captures are measured as an assault plays.

## 0.5.573: off his route, back to the last spot reached

572's carrier test: escorts now put (14 places); carriers with escort lived longer
(two carried 21 s, one halfway home, 1945 from his flag), the rest died within 1-4 s
near the enemy flag (killers alive 10-80 s, so not fresh spawns on the flag). The
21 s carrier made half the way he could have: at 650 450 his next route spot was
within 200 but not walkable straight, and the step's small Detour search sent him
100-150 off his way again and again.
- When none of the next spots is walkable straight, he first goes back to the last
  spot of his route he reached (he walked from it; the next one is linked to it),
  if he can walk there straight, before the "within 200" rule (0.5.563).

## 0.5.572: the carrier test's escorts stand where players have stood

571's carrier test: 8 carries, one a full capture (slot 27, 16 s home, capture value
1); the others died within about 2 s. But no escort was put (7 tests of 7): their
places were worked out by offsets from the flag (150 home, 120 aside), which never
had ground at the flag's height among Rouen's buildings.
- The escorts' places are learnt-map spots 100-400 from the flag, at its height
  (within 80), on the home side, 80 apart. Fewer than three put is logged
  ("carriertest: only N put").

## 0.5.571: the carrier test as an assault (three together)

570's carrier test: the take works every time (10 of 10), and one capture was
asked and the flag went home with the capture's value 1 (slot 27). But each lone
carrier died 0.3 to 1.4 s after the take, in the enemy's base (killers: a cover
defender, a middle holder, an attacker). A lone runner in the enemy base is not
how an assault comes in since the regroup (0.5.567: three or more).
- The test now puts three attackers: the taker 40 from the flag, two more 150 out
  either side of the way home, so it measures a carry with escort.

## 0.5.570: the "put by the game" check leaves the port's own moves alone

569's carrier test: 10 bots put by the enemy flag, no take: each was taken by
0.5.558's check (a living bot put over 1000 away by the enemy's end counts as a
game re-spawn) and moved home. That check now skips a bot this port moved itself
in the last 2 s (g_portMovedAt), as the carrier test and the CTF spawn moves mark.

## 0.5.569: a carrier test (BENCH-CTF-CARRY.bat)

568's 10-minute self-test: no take this time (567: 3), 95 fps (so 567's 12 fps was
outside the game, as read). Takes are too rare to judge the carry home by.
- COD3_CTF_CARRIERTEST=1 (set only by the new BENCH-CTF-CARRY.bat, 600 s, log
  bench-ctf-carry.log; never in a played game): every 60 s with both flags at home,
  one living attacker of each side by turns is put 40 from the enemy flag; his own
  pickup takes it, and the test shows what the carry home does ("carriertest:").

## 0.5.568: a carrier runs home by his route, not into the wall

567's 10-minute self-test: the regroup worked (44 pushes, 39 of them three or more
attackers together) and bots TOOK the enemy flag 3 times (slots 27, 11, 27; the
first bot takes since the spawns were put right). Every carrier died within
seconds: slot 11 stood 9 s at 1823 215, speed 0, Think's legs along his learnt route
home (111 60) and CarrierHome's, straight back along his own trail, the other way
(-108 -66), into a wall. (The run's frame rate, median 12, was the graphics card
busy with something else: the game's own work per frame was unchanged, bots 1 ms,
main thread 11.5 ms, the same draws, the GPU's time 15-38 ms against 7-8.)
- CarrierHome's legs only when Think gave none, or within 300 of his flag
  (straight onto it, to touch it). Its no firing, no aiming, sprint stay.
- A carrier always runs in Think (never its fight branch: he cannot fire).

## 0.5.567: attackers regroup before the assault

566's 10-minute self-test: running on for the flag in the last 1600 changed
nothing (no attacker in 35 lives within 700 of the enemy flag; killed 1000-1500
out, now more by middle holders, 13): they arrive one or two at a time where most
of the enemy is.
- Coming into 1700-2300 of the enemy flag an attacker holds (crouched, watching
  ahead) until three of his side's attackers are in the wave (1500-2600 out,
  whatever their lane) or 15 s have gone; then they go in, running on for the
  flag (0.5.566). Logged: "goes in: N of his side's attackers in the wave".

## 0.5.566: CTF attackers run for the flag in the last stretch

565's self-test (4 min): exits back to 81%, progress 54, 11 lives within 1500 of the
enemy flag, lanes harmless; attacker-on-attacker kills 18 (lanes do not stop them).
A new 10-minute test (BENCH-CTF-ROUEN-LONG.bat, 600 s, log bench-ctf-rouen-long.log):
no take; of 36 attacker lives none came within 700 of the enemy flag, 15 reached
700-1500, and both sides' were killed about 1250 out from it (side 1 at x 850 to
1160, side 2 at x -500), many fighting, the killers mostly the enemy's attackers:
the stream of their respawns leaving base, met one or two at a time.
- Within 1600 of the enemy flag a CTF attacker runs on for it unless an enemy is
  within 250 (was 900), as War's flag taker does since 0.5.471.

## 0.5.565: lanes only out in the middle

564's self-test: attacker-on-attacker kills 5 (563: 12), but only because fewer got
anywhere: side 1's attacker lives left base 4 of 9 (561-563: 85-87%), 3 lives within
1500 of the enemy flag (563: 12), progress 24 (563: 63). A lane from the spawn
pushed routes out of the cramped base into its walls.
- A lane is used only when the route is made over 900 from both flags, and costs
  0.25 a unit (was 0.5), so the base exits and the last stretch take the best way.

## 0.5.564: attackers split into left, middle and right lanes

563's self-test: best progress yet (median 63 toward the enemy flag in 3 s, 26%
stalled, 87% of attacker lives out of base), but no bot within 700 of the enemy
flag: 11 of the 12 lives that came within 1500 were killed there. By role, of 24
attacker deaths 12 were by the OTHER side's attackers (middle holders 9, cover
defenders 3): both sides' attackers took the same best way through the same
streets, head on.
- A CTF attacker's route keeps to his lane where the streets allow: by his rank
  left, middle or right of the line between the flags; a learnt spot on the far
  side of that line costs 0.5 per unit over it in the route search (a
  preference, not a wall: where there is one way, it is still taken). The same
  split as the loose line's 140 apart (0.5.541). Middle lane: as before.

## 0.5.563: a bot near his route is not "lost"

562's self-test: lives within 1500 of the enemy flag 11 of 34 (561: 8 of 26), no
crash; the new count says why routes were made again: "lost" (no route spot
walkable straight) 1700 times, against goal moved 38, ran out 102, no spot reached
in 5 s 137, about every 3-4 s a bot, each new route maybe pointing another way.
Reachable is one hull sweep past 192, so on uneven streets a far spot is seldom
"walkable" though he stands right by his route.
- None walkable straight but a route spot (of the next 24) within 200: the
  farthest along of those is steered to (the earlier ones dropped), his step
  finding its way round what is between; only with none within 200 is he
  re-anchored or given a new route.

## 0.5.562: a squad wait holds in place; route remakes counted by reason

561's self-test: 85% of attacker lives left base, but they circled mid-map (one
life within 700 of the enemy flag, no take). Followed life by life: routes changed
length almost every sample and turned back (slot 10 from -603 -1409 back to
-1232 -487), the turn-backs with "waits for his squad". (Checked and ruled out:
the learnt map's death costs; 4734 of 4786 spots have none, 1-14 average a band.)
- A leader waiting for his squad now holds where he is, crouched, watching toward
  the enemy flag, up to 6 s in 20 (was: a goal behind him, so his route was made
  again backwards, and forwards another way when the wait ended). His goal and
  route stay as they were.
- Why each route is made again is counted ("routes worked out again: goal moved,
  ran out, lost, no spot reached in 5 s", once a minute) and shown in ctftrace.

## 0.5.561: the step agrees with the route spot; a squad is who is near

560's self-test: side 1's attackers now left base 15 of 20 times (559: 6 of 11),
progress 48 toward the enemy flag in 3 s (best yet), 29% stalled. But no flag was
taken: no attacker came within 700 of the enemy flag, 33 of 39 lives never within
1500. Mid-map they moved (speed 167) and seldom fought (24 of 554 samples), but:
- At 850 -930 the route picker found a spot 140-220 north walkable (Reachable all
  the way) while the 96 step toward it was refused, the hull meeting raised ground
  (normal up 0.99): Climb lets a piece rise by its length, so a slope or steps
  pass as one longer walk and fail as a shorter one; the step turned off on a
  detour. A goal within 192 (four pieces of 48) is now tested the whole way, as
  the picker tests it, and stepped to.
- 82 of 554 mid-map samples were a leader waiting for his squad, its average
  pulled back by attackers still in base or just spawned: the squad is now the
  attackers within 1200 of him.

## 0.5.560: a bot that loses his route finds his place on it again

559's self-test: side 2's attackers left base 14 of 16 times, side 1's (the user's
side, the cramped flag at -1792 -91) only 6 of 11. The trapped ones ran at full
speed (150-220) inside the flag's building, x -2100..-1500, every route out
going east into the wall at about x -1600; none of the route's next six spots was
walkable from where a detour had left them, so no spot was reached, and each 5 s
a new route from another start aimed at another wall. (The local Detour search
is 12 steps, greedy toward the goal: it cannot find a door that faces away.)
- None of the next six spots walkable: the farthest walkable spot further along
  the same route (up to 24 spots, once a second) is taken and those before it
  dropped ("re-anchored"), keeping his way out.
- None on the route walkable: the route is worked out again at once (1 s apart),
  from a start he can walk to, not after 5 s.

## 0.5.559: attackers commit to one way out of their base

558 (user: "things were alot better"): every bot's first life now at his own end
(24 game re-spawns caught, the old move 0 times, later lives 55 of 55). Next:
attackers leaving base. Of 26 attacker lives, 15 left (over 1000 from their flag)
within 6-12 s; 11 never did, nearly all round side 1's flag (-1800 0). Their goal
flipped between the north side of the flag's building and the south (-1841 264 /
-1572 -159 by turns): the route was worked out again every 3 s from a slightly
different start, with links found bad meanwhile, and came out by the other exit,
so they turned back and forth.
- A route he is getting along is kept; it is worked out again only when the goal
  moves 200, it runs out (3 s apart then), or none of its spots has been reached
  for 5 s.
- Reaching any of the next six spots drops those before it (the picker steers to
  the farthest walkable one), so a shorter route is real progress.

## 0.5.558: the match start no longer puts each team at the other's flag

557's self-test proved the 0.5.557 fix: no float writes over the renderer list, no
guard repairs (556: five of each), 12 bot deaths' HUD arrows skipped. The user:
"alot of bots are crowded on the flag ... its like they are all grouped on our
flag". 45% of trace samples were within 500 of a bot's own flag, mostly
attackers and middle holders standing at the spawn spots. Followed through: every
bot's FIRST life (24 of 24) ended by the ENEMY flag, every later one (55 of 55) by
his own. At the start the game spawns each bot a second time "the game's way"
after this port's CTF move (slot 4: moved home, spawned again by the enemy's end),
and then the 0.5.464 "spawned near an enemy" move (60 times) sent him to "his
side's spawn" by the allies/axis names, the wrong end (0.5.545). So each team
began at the other's flag: the enemy crowd on the user's flag.
- In CTF the 0.5.464 move is off (CtfSpawnSides places every spawn).
- CtfSpawnSides also takes a living bot (no flag on him) that the game puts over
  1000 away, nearer the enemy's flag than his own, as a spawn and moves him home
  ("put ... by the game while alive ... taken as a spawn").

## 0.5.557: the stray writer found and stopped (a bot's death wrote a HUD arrow past the 4 local players)

556's hardware watch caught all five float writes over the renderer list
82C89A60 in its self-test, every one by sub_82452580, from the player-death code
(sub_82587268 -> sub_825862F4), the game script calling in. sub_82452580 puts a
damage direction (the HUD's arrow toward a hit, 36 bytes) into the local player's
client game state, cg 0x82C7D490 + 6040 + r7 * 6368; the title has four. A bot's
death passes his slot (4-31), so the arrow went past the four: bot 7's lands
exactly on the list's count (0x82C7EC28 + 7 * 6368 + 32), the corruption the
0.5.185 guard has repaired since; every other slot's landed elsewhere, unguarded,
the likely cause of 555's broken heap (its last death before the freeze: bot 7).
- sub_82452580 is skipped for a client number of 4 or more (a bot has no HUD), as
  its own early exit does; the host's arrows are unchanged. Logged: "guard:
  damage direction for client N ... skipped" (the first 12).
- The 556 watch stays on: the proof is no more "far above any real count" lines
  and no more "discarded elements BEFORE zero-byte copy".

## 0.5.556: catching the stray float writer; the heaps at a freeze

555 played 34 min, froze at 7 min 47 s: the main thread for ever in the game's
malloc (sub_820DF428), from drawing a player (sub_82400810 -> animation code);
a broken heap, as in 0.5.446 and 0.5.476 (no pool ran empty this time). Not the
graphics merge (its drawing thread idle, nothing queued) nor the bot routing
(host side only). Before it, five times, float data over the renderer list
82C89A60 (count 0x3F800000 = 1.0), the last a moment before the freeze: the
0.5.185 guard repairs that list; whoever writes there was never found, and the
memory beside it has no guard.
- A hardware write watch on that list's count (82C89A68) in every multiplayer
  game from 15 s: a count over 100000 is reported with the guest call chain of
  the function that wrote it, the instruction's function and the list's other
  two words ("watchpoint:"). Once a minute: how many times it was written (the
  watch's cost). COD3_LISTWATCH=0 turns it off; COD3_WATCH still takes over.
- At a stall (watchdog "no swap for five seconds") the game heaps are dumped:
  each one's capacity, use, peak and fallback, and heap 0's descriptor
  ("heapdump:").

## 0.5.555: Team Friend's graphics ("look v1") merged

Team Friend's look-for-justin.zip (made on 0.5.313; LOOK-NOTES.md in it says what
each part does), merged by hand onto 0.5.554. All in F11's Graphics page and
CoD3Recomp.ini:
- Look presets (Off, Remastered, Clean, Cinematic, War film, Vivid, Custom) with
  auto exposure, shadow lift, clarity, sharpening, bloom, saturation, contrast,
  warmth, vignette, grain, a night look; surface light at night; ambient
  occlusion (render_post.cpp, new).
- Fill the screen: the picture the window's shape (21:9 and others), the game's
  own field of view widened for it (title_fixes.cpp: campaign sub_82443A98, MP
  sub_8245CDC0), the HUD kept 16:9 in the middle.
- Shadow detail 1-4 (the shadow maps drawn finer).
- Films: smoothed to the screen's rate (frames made between), sharpened, cleaned,
  shadows lifted; remastered films from a .c3film beside the .wmv (film_hd.cpp,
  new; none shipped).
- The mouse pointer hidden while the game window has focus and F11 is closed.
How it fits this tree (it was 233 versions older):
- HDR on: this tree's HDR present path is used as before; the look is SDR only.
- Their alpha coverage left out: this tree has its own (theirs would have given
  every pixel program two SV_Coverage outputs).
- Their film pacer thread now lets this tree's drawing thread finish
  (RenderQueue::Sync, as render_queue.h asks) before it uses the context.
- Fixed where the patch tool put things: two globals had landed inside a class
  (render_resources.cpp), declarations in the middle of comments.
- Defaults as shipped: the look Off, Fill the screen on, shadow detail 2, film
  smoothing on. The shader translator's version string changed: shader caches
  are rebuilt once.

## 0.5.554: learnt-map links a bot cannot walk are found and avoided

553's self-test: routes now start where the bot can walk to (803 of 822), progress
back to 11, refused steps 916 (from 1200), two flags taken; but the next spots
were still out of reach 555 times (slot 22 circled round -650 -700 for half a
minute, the same place the user's attackers circled). The learnt links are any
player's moves between two neighbouring cells (jumps and climbs up to 200 high
too), and each cell's spot is where it was first stood on, so two linked spots
can have a corner or a fence between them.
- Each new route's first six links are walked by the test the bot's step uses,
  spot to spot; a failing link is marked (for every bot, this session) and costs
  2500 more in the route search (not removed: a place with no other way keeps
  it), and the route is worked out again (three rounds at most).
- Logged: "learnt map: N links found not walkable by a bot so far".

## 0.5.553: a route starts where the bot can walk to

552's self-test: still no better (median 5), and in 739 of 891 picks no route spot
was walkable, the first one 80-400 off with a wall between (583 of the refused
steps hit it). MapKnowledge::Path starts at the learnt spot nearest by distance,
walls not counted (the next street over, a room behind the wall), and leaves
that spot out, so the route began past the wall.
- The route now starts at the nearest of the eight closest learnt spots (within
  400) that he can walk to straight by his step's own test, that spot first
  (MapKnowledge::NearestList, PathFrom). None: as before. ctftrace says which.

## 0.5.552: learnt routes followed spot by spot, round the corners

551's self-test: in 736 of 797 attacker samples none of the next route spots
could be reached straight by his body, nor the first, which was steered to
anyway (544 of those had the step refused and turned off the goal). The cause:
every spot within 140 of him was dropped, and those are the ones that lead round
the corner beside him; what was left lay past the corner, straight at it the
wall, so he detoured and circled.
- A spot is dropped only within 40 of him (was 140).
- The spot steered to is the farthest of the next six he can walk to straight by
  the same test his step uses (Reachable: body box, stair, slope in pieces of 48,
  walkable ground); none: the first. The 550 box test and chest line are replaced
  by it. ctftrace says "walkable" or "first (none walkable)".

## 0.5.551: trace says which route spot was picked

550's change made no difference (self-test: median progress 8 toward the enemy
flag in 3 s, 1234 refused direct steps against 549's 1211). One reading: when
none of the next four spots passes the (now stricter) body test, the picker
falls back to the first spot unchecked. ctftrace now ends with "route: spot k of
n, passed / first (...), distance", or "no route spot", so that is measured
before anything else changes.

## 0.5.550: the next route spot picked by the bot's body, not a thin line

549's self-test said why the straight step was refused: 1181 times the box swept
at the feet and a stair up hit the world (handle 4000) a third of the way, the
walls (normal up 0, 1062 times); never a player. The spot on the learnt route
had been picked as the farthest of the next four with a clear LINE at knee and
chest, and a line slips past a corner the body's box does not; the step then
went 90-180 off the goal and the bot circled (546-549: median progress 12-13
toward the enemy flag in 3 s, a sprint is about 600).
- The spot is now picked by the bot's own box swept there a stair up (the same
  test his step uses) and a line at the chest. Applies to every bot following
  a learnt route.

## 0.5.549: trace says why the straight step is "blocked"

548's self-test: in 285 of the 355 attacker samples whose legs pushed over 90
off the goal, the direct 96 step toward the goal was not Reachable, so a detour
step 90-180 off the goal was taken; with the legs right the step was direct
188 of 271 times. The goal itself had a clear line at knee and chest, so the
hull, floor or climb test refuses what the line test allows. ctftrace now adds,
for a refused direct step, which test failed: the hull at the feet and a stair
up (fraction, the handle hit: 0x4001-0x4040 is a player, slot + 1, and the
normal), or the floor under its end (none, too steep, too far down, too high).

## 0.5.548: trace adds TacticalMove's step (why the legs turn away)

547's self-test (saved routes off): no better (median 12 toward the enemy flag in
3 s, 289 of 697 stalled). The new columns showed why:
- the view faces the goal (679 of 727 within 30 degrees),
- but the legs from Think point ahead only 224 of 727 times (129 backwards); in
  322 of the 323 samples with legs over 90 off the goal the view was right.
- Not the team mates' separation push: legs are off as often with no mate
  within 220 (67%) as with one (63%).
So the step TacticalMove steers to is off the goal's way. The trace now says
where the step is, how far off the goal's way, how it was chosen (direct,
detour while escaping / blocked, short escape, wander, drop, jumps, a stuck
hop), and whether an escape or wander is on, the stall samples and strikes.

## 0.5.547: saved-route steering off (measured); trace adds view, speed, direction

546's 4-minute CTF self-test (24 bots, 1851 trace lines):
- Attackers were on "to goal" 770 of 833 samples, sprinting, the goal a median
  271 ahead and pointing at the enemy flag (638 of 713 within 60 degrees; only
  10 pointing away): the goals are right, the movement is not.
- With the saved route (ctf-routes.txt) on his legs an attacker made a median 3
  units toward the enemy flag in 3 s, 52% of samples stalled; on his own way
  13, 39% stalled. Near the flag it pulled against his own path (slot 7 held at
  276 from it, its legs turned another way each sample).
- A bot took the enemy flag (slot 6); as carrier his legs come from CarrierHome
  ("think 0 0", "final" its own) and he froze at 1866 729, 847 684 and
  -435 176, moving only by the 10 s unstick teleports, and died 1383 from home.
Changes:
- The saved routes no longer steer attackers (COD3_CTF_SAVED_ROUTES=1 turns them
  back on, for comparing). They are still learnt.
- ctftrace adds his view, his real speed, and how far off his goal the final
  legs push him and he really moves: where a goal does not become movement.

## 0.5.546: CTF trace (why attackers crawl)

0.5.545 played 18 min (CTF Rouen): sides right (host side 1, every spawn by his
own flag, killed only by an enemy), but no flag taken: no bot came within 600
of the enemy flag; attacker slot 4 made 1000 in 2.5 min and circled one 300
patch for 1.5 min, seeing no enemy and not stuck. The log does not say which
steering drives him, so a trace first, no behaviour change:
- `ctftrace:` every 3 s for each bot with a CTF errand: where he is, how far
  the enemy flag is, his errand, which way out of Think he took (to goal,
  fight, explore, roam, retreat, hold, ladder), his goal (the next step) and
  his legs after Think, after the wall feelers, and in the end (after the
  saved route), firing and sprinting.
- For the 4-minute unattended CTF test (BENCH-CTF-ROUEN.bat).

## 0.5.545: the host's team is his own again; bots' spawns read the right way round

0.5.544 was wrong (user: spawned "with the enemy team in their spawn", killed
by his own bots). 544's log: his team read 1, put on side 2 by the port; side 1
bots 4 and 11, his team, killed him. Every match of 541-544 put side by side:
- The host's team read was right every time (541 1, 542 2, 543 2 then 1, 544 1).
- The game spawns the host by his own flag (read 1 by -1792 -91 in 541, 543,
  544; read 2 by 2112 462 in 542, 543), and his takes were of the far flag
  (541: read 1 took 2112 462; 542: read 2 took -1792 -91). On Rouen side 1 owns
  -1792 -91, side 2 owns 2112 462.
- The port's own bot spawns (Respawn) put every bot by the ENEMY flag (side 1
  by 2112 462 35 of 35, side 2 by -1792 -91 36 of 36). 544 read them as "own",
  so both teams were swapped. Bot takes prove nothing: the port makes them.
Now:
- The host's side is his own team read; nothing overrides it.
- Each side's flag is a vote: a bot spawn votes for the OTHER flag (1), a host
  spawn for the one he is by (3), a take by a player for the other one (8).
  Simulated on 543 and 544: side 1 owns -1792 -91, decided once and never
  flipped; every host spawn by his own team's flag; the bots that killed him
  in 544 would have been team mates holding fire.
- A host spawn by the other flag than his side's is left where the game put him.
- (Still in from 544: revived bots get a fresh body where they lay.)

## 0.5.544: CTF teams from what the game does; revived bots stand up properly

Worked out from the 542 and 543 logs (71 bot spawns, the UAV team lines, the
takes and the kills) before any change:
- The game spawns every player by his OWN flag, respawns too: side 1 by
  2112 462 35 of 35, side 2 by -1792 -91 36 of 36 (by the side each bot was
  given). 0.5.543's "respawns are random" was wrong: it came from the team read
  at the moment of a spawn, which is the other side 46 times of 71 (the UAV
  line, two seconds later, has the given side). 540-543 used that read, so
  bots were moved to the enemy flag, 543 decided side 1 owned -1792 -91, and
  the attackers ran to their own flag and stood there.
- The host's team read is numbered the other way from the bots' in every
  match (541, 542, 543 twice), shown by where the game spawns him and by his
  takes. In 542 and 543 the bots that killed him were his own team.
- Now: a bot's side is the one he was given, everywhere (Side() too, so no
  bot targets a team mate by the spawn-time read). Each side's flag is a vote
  of where the game spawns its bots. The host's side is the side owning the
  flag the game spawns him by, every spawn (or a flag he takes). Simulated on
  both logs: side 1 owns 2112 462 from the 2nd-3rd bot spawn; the host side 1
  in 542 and side 2 in 543's last match, and the bots that killed him there
  were his team mates.
- Revive: a medic bot's revive (the script's Plyr_Revive) left the bot's body
  in its death pose (user: "a bot in the dead animation running around"; 543's
  slot 13). He is now spawned again the game's way and put back where he lay
  with the revive's health. A revived bot is no longer moved to a flag.

## 0.5.543: CTF sides from the game's own start spawns; the host's side from his first spawn

Checked against the 541 and 542 logs before building:
- At the match start the game spawns every bot by its own team's flag (12 of 12
  in both). Respawns are spread over the map (542: 18 of 30 nearer the enemy
  flag), so only the start counts. Bot takes agree: each took the other flag.
- Each side's flag now comes from the bots' start spawns, nearest flag = own.
  ctf-flags.txt (the "proven" file) is no longer used: takes were credited to the
  host's team as read, which is flipped, so the file flipped (541 and 542 both
  had the host's read wrong: 1 vs real 2, then 2 vs real 1).
- The host's side comes from his first spawn of the match (the flag it is
  clearly nearer), or from a flag he takes (only the enemy's can be taken).
  Until it is known he is not moved. Bots use it to tell friend from foe: in 542
  he was killed by side 1 bots, his own team.
- Forgotten between matches, so each match works it out again.

## 0.5.542: the host's side fixed again; bots off unclimbable routes; more spawn room

- The host's side is his entity's team once it has held 2 s. 0.5.540's guess
  from the kit (American = side 1) was wrong on Rouen: 541's log had team 1,
  kit 2, so his own bots shot him, the enemy left him alone, and he was put by
  the enemy flag.
- Saved CTF routes: an attacker making no progress along one for 4 s drops it
  for 60 s and tries the next; with none left he finds his own way. Side 1's
  routes go over raised places a bot can't climb, and its attackers walked into
  the wall at -1103 132 all game.
- A cramped flag (side 1's on Rouen had 1 spawn spot) gets up to 10 more spawn
  spots from places players have run, 150-650 from it, on standing ground.

## 0.5.541: unstick; CTF defenders in cover, attackers as a squad, escort formation

- Unstick (F11, on by default; `unstick` in the settings file): hold a move key
  and move under 4 units for 2 s and you are put back at the newest spot you
  walked normally, at least 48 away and 3 s old. Not on ladders, prone, or with
  movement flags never seen while walking (a mounted gun). 5 s cooldown. Logged
  as `unstick:`. Nothing knife related changed.
- CTF defenders: the home guards each take a cover spot by their flag (ground
  220-700 out with walls on 2-6 sides, seeing the flag or the way the enemy
  comes; up to 12 per flag, logged once a map). The middle ones hold the
  middle. At their place they crouch and sweep their view 55 degrees either side
  of the enemy's approach, and fight from there.
- CTF attackers push as a unit: a loose line 140 apart until 700 from the enemy
  flag, and one more than 450 ahead of his squad waits for it (at most 6 s in 20).
  No more "long way round" detours for attackers.
- Escort: every attacker (and a middle defender within 1500) takes a place in a
  formation round the carrier, the host included, facing home: point man ahead,
  two flanks, two behind.

## 0.5.540: the host's own side; no spawn spot past the map's edge

- 539 played (user): "i kept spawning near the enemy flag"; "i fell outside
  the map when respawning". His entity's team read 2 at each of his CTF
  spawns while the UAV check had him on 1: the CTF code put him by side 2's
  flag (2112 462), the enemy's for him. Side() now gives the host's side
  from his kit (American = 1, the bots' own rule; MpFixesOwnSide) and logs
  when the entity's team disagrees.
- A ring spawn spot must also be in sight of the flag (chest and knee height,
  ground to ground), so none lies past a wall or the map's edge.

## 0.5.539: CTF spawns AT the team's flag

- 538 played (user, side 1): "i spawned on my flag but the rest of my team
  didnt"; enemy bots took his flag once. Every respawn was moved (49 of 49),
  but to Rouen's spawn points 1000-1400 from the flag: both sides spent most
  of the match a median 1250 from their own flag, and hardly anyone reached
  the enemy flag (6 presses in all).
- Now a respawn more than 500 from his flag (was 900) is put on a ring
  160-360 round it, each spot's ground found by a trace ("N spawn spots
  round the flag"), or a map spawn point within 700; the 1400 ones only if
  there is none.

## 0.5.538: the test's hacks off; a killed bot carrier drops the flag

- The user plays in the unattended test's matches; the probe's hacks (the
  host teleported to the enemy flag and home, pad X taps, bots' script
  fields written, side 2 not taking) broke his game ("spawns really
  broken"). The probe (CtfTakeProbe, and its traps) now runs only with
  COD3_CTF_PROBE=1, which BENCH-CTF-ROUEN.bat does not set. The spawn moves
  themselves were right in his log (each side to its own flag's half).
- User: a dropped flag "just goes invisible ... but it shows on the radar":
  the game never dropped a killed bot carrier's flag (it stayed hidden).
  Now when a carrier bot dies the port calls the drop callback (+3424) for
  him ("carrier slot N killed: his flag dropped").
- ctf-routes.txt has 3 routes for side 2 on Rouen: the user's own runs.

## 0.5.537: trails start over at a jump

- 536 saved its first route from the test host, whose trail held his
  teleports (spawn -> enemy flag -> home), so a bot walked a "route" with
  jumps across the map. Now a step over 400 between trail points (a
  teleport, a spawn move, an unstick) starts the trail over; only walked
  paths become routes. The bad ctf-routes.txt was deleted.

## 0.5.536: CTF routes learnt from every run to the enemy flag

- 535: no bot reached the enemy flag but one (slot 6, 49 presses), in 4
  minutes: getting there is as hard as getting home.
- The user's idea, automatic: whenever a player (bot or the host) reaches
  the enemy flag with a trail begun by his own flag, that trail is saved as
  a route for his side and map in ctf-routes.txt (game folder; kept across
  sessions; 16 a side). Attackers (their CTF role, not fighting) walk a
  saved route toward the enemy flag, one by slot, from the point nearest
  them; carriers walk their own trail home.

## 0.5.535: carriers walk their trail home without the wall feelers turning them

- 534 (carriers walking): carrier 26 went 3975 -> 1287 from home in 70 s
  before being killed, mostly by the stuck watchdog's jumps: between them
  the wall feelers ("keeps meeting a wall ... rerouting") turned him about
  by the enemy flag.
- Now the carrier's steering runs after the feelers (his trail is ground he
  walked), and he starts from the trail point nearest him (the later ones
  dropped), then on toward home.

## 0.5.534: BOTS CAPTURE; now walking home is the test

- 533: BOT CAPTURES WORK. Three carriers (slots 10, 6, 14), on their own
  flag with it home, asked Callbacks +3432 (their slot, 1) as a client
  does, and the enemy flag went home at once (171, 238, 262 s): the capture.
  Take, carry and capture are all done now.
- Unattended test: the "put him home" teleport off, so carriers must walk
  home (their trail, the stuck watchdog); side 2 still does not take in the
  test, so side 1's flag stays home for their captures.

## 0.5.533: the capture asked with the client index

- 532: no bot capture asked: both flags were out at once (a capture needs
  your own flag home, as in the game). The host's capture went through
  _imp__sub_82203E00 straight from sub_823E3298 (so its wrapper saw
  nothing); sub_823E3298 passes the message's first value as a client index
  and a 0/1 second.
- A bot carrying, on his own flag at its base, asks Callbacks +3432 (his
  slot, 1 / 0 by turns, every 2 s); "flag N home" names the last value.
- Unattended test only: side 2 bots do not take, so side 1's flag stays home.

## 0.5.532: bots ask for the capture, as a client does

- 531: carriers now keep the flag (bot 5 held it 93 s at his base); the
  "flag returned" spam is gone. The host captured again (102 s) and the trap
  caught it: the flag was moved home by sub_82203E00, called through
  Callbacks +3432 by sub_823E3298, a CLIENT MESSAGE handler like the take's.
  So the capture is asked for by the player's client too, and bots, with no
  client, never asked.
- A bot carrying, on his own flag (within 40) at its base, now asks every
  2 s: Callbacks +3432 (his slot, his side - 1). sub_82203E00 is wrapped to
  log a real capture's arguments ("the CAPTURE called").

## 0.5.531: carriers keep the flag; no "flag returned" spam; the capture's +496 traced

- User: "it says enemy flag returned over and over"; "bots are still
  struggling to keep the flag held". The capture request A (his own side at
  his base) made the game announce the flag returned twice a second: the
  capture requests (A, B, C) are all gone (a capture is the game's own,
  walking onto the home flag). 530: a carrier's melee charge fired the flag
  through his own command and dropped it: now a bot holding a flag has no
  fire, melee, grenade, smoke, aim, reload or use in his command.
- The home triggers' +496, set when the host captured and never for a bot
  carrier, is now write-trapped ("a flag trigger's +496 written by").

## 0.5.530: carriers never fire the flag; the host's capture traced

- 529: the game's drop of a bot carrier's flag came from sub_82590C30, the
  server's weapon fire: the port fires bots' weapons itself, and firing the
  flag DROPS it (the carriers by their flag "fired" at the first enemy).
  Now a bot holding a flag weapon is never fired.
- Bots by their own flag still were not given the capture (no script thread
  started on the home trigger for them). Unattended test: the host takes a
  flag at 35 s and, carrying, is put by his own flag; the flag trap now logs
  each distinct chain that moves a flag model once (take, drop, return,
  capture), to find the capture's code.

## 0.5.529: who drops a bot carrier's flag at his own base

- 528 (clean test; the host parked out of the way): bots took flags (126,
  173, 209, 225 s); put by their home flag, they did not capture: the GAME
  called the drop callback (+3424 sub_822049D8) for the carrier (246 s), the
  flag fell at his base and was returned. In 526 the host's carry, by his
  flag, was captured with no drop. So the game chooses drop for a bot where
  it captures for a player.
- The drop wrapper now logs its native callers ("the drop's callers"), to
  find that choice.

## 0.5.528: a clean bot capture test

- The user: a capture is automatic, walking onto the home flag's spot. 526's
  test agrees: the host, carrying, put by his flag, CAPTURED at 72 s with
  no request at all (server side); his home triggers had +496 set (a script
  thread waiting) at that touch. Bot 26 at 106 s: touches told to the
  script, but +496 stayed 0 and no capture, likely muddled by the host's.
- Unattended test: no host takes or captures (the host is parked by his own
  flag at the start); a bot carrying 20 s is put by his own flag and his
  home triggers' +496 logged.

## 0.5.527: no more dropping the flag at home

- 526 (test and user): bot carriers now hold the flag (the user can see
  it). At their own base, request C (+3424) made them DROP it there; it lay
  25 s and went home by itself (no score). C is no longer asked; A and B
  (+3420, his side / the carried flag's side) stay, harmless.
- The capture's real request is still unknown: the user's own capture, with
  the flagcode wrappers on, will show it.

## 0.5.526: bot flag carriers hold the flag (seen, and dropped when killed)

- 525 played (user): guns right; bots took flags (both sides at 132-133 s)
  but no flag showed in a carrier's hands, and a carrier killed dropped
  nothing visible (log: carrier slot 2 died at 191 s, the flag stayed
  hidden to the end). The game's take gives the flag as a weapon
  (mp_flag_allies/_axis) and switches to it; the bot's command still named
  his gun, so he switched straight back.
- Now a bot owning a flag weapon asks for it in his command: the flag is in
  his hands, and the game drops it when he dies. FlagEntityWatch no longer
  logs a "take by slot -1" every frame for a flag hidden with no holder.

## 0.5.525: a real capture watched; dead carriers carry nothing

- 524: the carrier put by his base died there (health 0), so the drop test
  did not run; and a dead carrier stayed a "carrier" after respawning.
- Now a carrier who dies carries nothing ("flag N's carrier slot S died").
- Unattended test: the host is put on the enemy flag at 35 s (he takes it by
  touch), and 5 s after, carrying, by his own flag: his client's capture is
  logged by all three message callbacks (+3420 sub_82205788, +3424
  sub_822049D8 "drop", +3428 sub_821F33A0, now wrapped too).

## 0.5.524: a flag dropped at the carrier's base left there

- 523: Callbacks +3424 (request C) DROPS the carried flag: right after it,
  the carrier's flag fell by his own flag; A (his side) and B (the carried
  flag's side) via +3420 did nothing at his base. Each drop was then taken
  straight back by the same bot ("take (lying out)"), 5 times.
- Now the enemy's flag lying within 300 of a bot's own flag is not taken
  back, in case the drop there is the capture.

## 0.5.523: the capture asked three ways; carriers near home go straight in

- 522: both sides' bots took flags at once (126-127 s); carriers walked their
  trails back; the host never carried, so his capture was not seen.
  Callbacks: +3420 sub_82205788 (the flag request), +3424 sub_822049D8 (a
  player's handle to sub_821F8CC0), +3428 sub_821F33A0.
- At his own flag a carrier now asks in turn: A his side (+3420), B the
  carried flag's side (+3420), C +3424 with his handle; the next "flag N
  home" line names the last one asked.
- Within 300 of home a carrier goes straight to his flag; the stuck
  watchdog within 400 puts him by it (522 put one back along his trail).

## 0.5.522: carriers walk their own trail home; the host's capture watched

- 521's capture test: the carrier put by his own flag asked (his side, 1)
  20 times; the flag handler ran each time (its own-flag branch) but did
  not score. So a capture is asked some other way.
- Carriers home (user: "lets do both fixes at once"): each bot keeps his own
  trail this life (a point every 120 walked); carrying, he walks it back
  (the last points first), then to his flag; stuck 10 s, he is put two
  trail points back (before: the learnt map's route, which often led away).
- Unattended test: the host, carrying a flag 5 s, is put by his own flag;
  the flagcode wrappers (log cap 3000) show what a real capture calls, and
  Callbacks +3420/+3424/+3428 are printed.

## 0.5.521: the capture tried apart from the pathing (test)

- 520: the sides held all match; bots of both sides took flags (27 for
  side 2, 22 for side 1). No capture: the carriers wandered 2 minutes; the
  watchdog's route points often led away (2981 -> 3619 from home).
- Unattended test only: a bot carrying a flag 20 s is put 20 from his own
  flag, where BotFlagClaim asks to CAPTURE ("CAPTURE TEST"): does the
  request score?

## 0.5.520: each flag's carrier by its own flag weapon; sides no longer flipped by takes

- 519: bots of both sides took flags; the carrier watchdog moved them on;
  bots asked to return dropped flags; 26 capture asks. But one flag's take
  was credited to the other flag's carrier (the first holder of ANY flag
  weapon), and "ctf sides corrected by a take" then swapped the sides
  mid-match: carriers headed for the wrong home (3900 away).
- Now a take is credited to the holder of THAT flag's weapon (side 2's flag
  gives mp_flag_allies, side 1's mp_flag_axis); takes no longer correct the
  sides, and only a player's owner-checked take saves a proof.

## 0.5.519: the flag carrier always gets home

- 518: bots of BOTH sides took flags (slot 19 side 1 at 149 s, slot 6 side 2
  at 235 s): the side word fixed. No capture yet: carrier 19 circled one spot
  (about 960 270) for 100 s; the navigation's stuck test never fired (the
  jitter counted as progress).
- New CarrierUnstick: a carrier not 150 nearer home in 10 s is put 3 spots
  further along his learnt route home ("carrier slot N stuck ... put at").

## 0.5.518: bots take, capture and return flags (the client requests)

- 517 WORKED: bot slot 18 (side 1) stood on side 2's flag, the port asked
  the server's flag callback for him (2, his handle, 0), the game's flag
  handler ran for the bot and he TOOK the flag (98 s). But a side 2 bot
  asking with 2 at side 1's flag was refused 14 times: the first word is
  the side whose flag it is. And the carrier hit one wall 11 times mid-map.
- Now BotFlagClaim asks as a client does in all four cases: the enemy flag
  at its base or lying out (take: the enemy side), his own flag at its base
  while he carries theirs (CAPTURE: his side), his own lying out (return:
  his side). Every 500 ms within 60.
- A stuck flag carrier is put along his route at strike 4 whatever is in
  sight (was: only with no enemy within 1500).

## 0.5.517: bots ASK for the flag, as a player's client does (the CTF fix)

- 516's wraps: the flag handler (sub_82204A08) and its caller (sub_82205788)
  ran only for the host's takes, never a bot's. Their caller sub_823EB8F0
  is a NETWORK MESSAGE handler: a player's CLIENT, seeing him in the
  flag's pick-up area, sends a request; the server's callback (Callbacks
  0x82BCBF64 +3420) then gives the flag. A bot has no client, so nothing
  ever asked: the cause behind all of 474-516's bots never taking a flag.
- Now, while a bot stands on the enemy flag at its base (within 60), every
  500 ms that callback is called for him as the host's request was: (2, a
  pointer to his handle, 0), in his client scope ("the server's pick-up
  callback ... called for him").

## 0.5.516: the CTF flag handler wrapped (compiled script)

- 515: players' and bots' contents (+324) the same; not the gate.
- CoD3's scripts are compiled native code: sub_82204A08 is the CTF flag
  handler itself (it gives the carrier the flag weapon, gi +2664 +2872
  +2692, then hides the base flag); its caller sub_82205788. Both wrapped
  (every build): "flagcode:" lines with each call's arguments and the
  player's slot (BOT / host), and the handler's return, to see where a
  bot's touch stops.

## 0.5.515: the second flag trigger: contents logged

- 514: bot touches of flag trigger 111 reach the script (all PASS), but bot
  22 (side 2), at the enemy flag -1792 -91, touched 111 23 times and its twin
  112 (same place) never; the host's takes had both triggers of the flag
  record him. The engine's touch loop matches the toucher's contents (+324)
  against a mask, so a team bit bots lack would keep them off 112.
- Unattended test: +324 of the host and of every bot (with side), and
  +320/+324/+328 of the four flag triggers, logged 25 s in.

## 0.5.514: the touch queue, flag triggers only

- 513: 192000 queued touches by bots, every one PASSED (the script is told);
  but the 60 logged were all other triggers (spawn and zone volumes), none
  the flags'. Now only touches of the flags' trigger_multiples are checked
  and logged ("touched FLAG trigger"), with their own counts.

## 0.5.513: the touch queue: are bots' touches thrown away?

- 512's traps: a touch (bot or player) runs ClientThink -> ... ->
  sub_8256C168, which does not tell the script itself: sub_82567328 queues
  (trigger handle, toucher handle, both +204s) at 0x82C281D0 + 3196 (count
  +7292). Its consumer sub_8256FFC8 resolves both handles in the entity
  table (generation checked) and compares both +204s with the queued ones;
  only then sub_820B0898 notifies "trigger". A failed check drops the touch
  silently.
- sub_8256FFC8 is wrapped: every queued touch by a bot is checked the same
  way first and logged ("touchqueue: ... PASSES / the BOT's handle does
  not resolve / the BOT's +204 changed ..."), with running counts.
  (Every build, not only the unattended test: a log line per touch, capped
  at 60.)

## 0.5.512: who writes a flag trigger's touch fields

- 511: bots' script tables grown to the host's 14 fields (no crash): still
  no bot take. The script's player fields are not the gate.
- Unattended test: the flags' trigger_multiples' touch fields (+760 the
  time, +764, +804 the toucher) are write-trapped too; each distinct writing
  chain is logged once ("flagtrap: a flag trigger's touch field +N written
  by: ..."), to see whether a bot's touch goes on into the script like the
  host's or stops in the engine.

## 0.5.511: bots' script tables given the host's missing fields (test)

- 510: bots' 1C4A56B2 and 14D6461A set every second: no bot take.
- Unattended test: each bot's script table grown to the host's: a new
  entries array with the bot's own 8 entries plus the 7 only the host had
  (his values; 7654A134, a time, the bot's spawn time), 1C4A56B2 = the
  bot's side. The table's memory is the port's, not the script's: watch for
  a crash if the script grows it later.

## 0.5.510: bots' unset script fields filled in (test)

- 509 read the players' script tables right: the host has 15 fields, a bot
  8. A bot's 1C4A56B2 is FEFEFF00 (uninitialised fill; the host's is 1, his
  side) and his 14D6461A FEFEFEFE (the host 0); 7 fields only the host has
  (1FA6AD2F=-1, 35249286, 4C3C14A1=00FFFFFF, 4D2B5732, 72013241, 7654A134
  a time, B8F24ACB). The script's player set-up never fully ran for bots.
- Unattended test: each second every bot's 1C4A56B2 is set (side 1 bots 1;
  side 2 bots 2 in even slots, 1 in odd) and 14D6461A to 0; a bot's take
  logs which value he had.

## 0.5.509: the script variable table read the right way

- 508: every bot's side set again through the session's side change: still
  no bot take (the host took the flag several times; the flag-weapon owner
  check corrected two "nearest bot" credits to him).
- The lookup sub_821DE958 shows the table: +0 the field count, +8 a pointer
  to the entries (8 bytes each: name hash, value). 505 looked for the pairs
  at +8 itself. Now every field of the host and of a bot of each side is
  listed ("scriptfields: host all / bot1 all") and diffed (ONLY THE HOST /
  DIFFER / ONLY BOT1).

## 0.5.508: bots' sides through the session, as the game's menu does

- 507's (handle, 1, 0) took the callback's other branch: no take either;
  back to (handle, 1, 1), which is what the game's own side menu
  (sub_8239FB10) passes in the local mode. In a session game that menu
  instead calls the session's side change, sub_823EA4E0(mode object,
  entity, side, 0, 0); bots never went that way.
- Unattended test: 25 s in, every bot's side is set again that way (or by
  the callback in the local mode): "ctfprobe: N bots' sides set again".

## 0.5.507: the side callback with its join argument

- 506's test: the bots went through the side callback (24 calls, no crash)
  but nothing changed (no bot take). The callback (sub_82202070, args the
  handle, then two words) returns at once when its third word is non-zero,
  and the port has always passed (.., 1, 1), the menu path too. Now bots
  (COD3_BOT_SIDE_CALLBACK=1) get (.., 1, 0): the join itself (alive/state
  checks, the team set by sub_821E9EE0 and sub_821F1008, the spawn).

## 0.5.506: bots' script-side team (the likely CTF refusal)

- 505: the script fields could not be listed (the variables pointer leads
  into a string pool). But the log has always said, for every bot,
  "server-only side N ...; menu callback skipped": with g_botCamera on (the
  default) PickSide sets the engine's team only and skips the game's side
  callback (Callbacks +3320), the script's join-team logic. So the scripts
  never learn a bot's team, and CTF's (comparing the player's team with
  the flag's) gives him nothing; likely War's captures too.
- COD3_BOT_SIDE_CALLBACK=1 now calls that callback for bots as well (off by
  default until tested). BENCH-CTF-ROUEN.bat sets it.

## 0.5.505: every script field of host and bots, diffed

- 504: the field lookups work ("spawntime" found on the host and both
  bots; the name hash sub_820E10E0 matches the script's field names); none of
  the other 29 guessed names is CoD3's. A field is stored as (hash, value).
- Unattended test: each player's variables dumped raw (+0..+0x200), and
  every word there that the player's own lookup finds is taken as a field:
  "scriptfields: ONLY THE HOST / DIFFER / ONLY BOT1" lists.

## 0.5.504: the players' script fields, host vs bots

- 503: CTF stayed on while the flag was out (bases kept). The read watch
  saw the script read almost nothing of a player's own memory (one read in
  three bot windows and the host's): the script's checks use its own
  variables (team, pers, ...), not the entity.
- Unattended test: "scriptfields:" lines: the host's and one bot per side's
  script variables by name (handle = 0x4000 | entity-table index; vars
  sub_821DE680; field sub_821DE958 by the hash sub_820E10E0), for 30 names
  (team, pers, sessionteam, sessionstate, ...), plus the known ability field
  (14D6461A) to show the lookups work.

## 0.5.503: CTF keeps working while a flag is out

- 502's test logged no script reads: the probe, like ALL the CTF code
  (sides, spawn moves, the bots' flag goals, FlagEntityWatch), runs only
  while exactly two flag places are seen; a taken flag's triggers move off
  and a dropped one adds its own, so from the host's take at 74 s to the
  end nothing CTF ran (the dropped flag lay 130 s, no one went for it).
  Likely behind much of the users' "bots never return/score" reports.
- Now the two bases are kept per map once found ("objectives: ctf bases
  kept"); while a flag is out the count stays 2 at the bases ("a flag out:
  the two bases kept"); the flag's own state comes from FlagEntityWatch.

## 0.5.502: MG42 fix narrowed to clips; the script's reads of a player

- 501's MG42/FG42 fix also put back spares #116-118, which are the
  specials' own (the 4 mines and rifle grenades went back to 2): now it
  restores clips only.
- 501's trial (498's whole set copied to a bot, at a flag at its base): NOT
  taken. So 498's "bot take" was the idle host; copying fields is not it.
- The read watch again, with the interpreter's code from the game's function
  table (496's name lookup failed): "vmread: <who> <region> +<offset> =
  <value> by <function>" for up to three bots pressing at the enemy flag,
  then for the host put 18 from it (his take).

## 0.5.501: MG42/FG42 full clips for a rifleman-based custom class; real flag carrier

- Fixed (user: custom class 1, made from the rifleman, medic with the MG42 or
  the FG42: 4-5 rounds then a forced reload): the rifleman's special is the
  rifle grenade, which 0.5.220's SpecialToFour sets to 4 through the game's
  "special" ammo call; with no rifle for it the 4 landed on the gun (the
  MG42's clip and spare are one pool, #14). Now the player's clips and
  spares are kept around that call and any pool it changed that is not a
  mine's or a rifle grenade's is put back ("class: ... the special's 4 hit
  clip #N ... put back").
- CTF: who carries a flag now comes from the game: the owner of a flag
  weapon (mp_flag, mp_flag_axis, mp_flag_allies), not the nearest player.
  498's "a bot took it" may have been the idle host (39 away, he takes the
  flag by standing in his spawn). 500's trial A (+132 +228) took nothing.
  The trial again with 498's whole set, on the host's own flag.

## 0.5.500: trial A again, on a flag at its base

- 499: trial A (record +132 +228) void: the flag was not at its base (the
  idle host, in his spawn by the enemy flag, had taken it at 209 s). Trial B
  (record +1320 +1324 +1328) at a flag at its base: NOT taken. So +132 or
  +228. Now trial A on the other side's enemy flag (at its base).
- Weapon pools (499): the MG42 is clip 75 in pool #14 (the MG34's), the
  FG42 clip 20 in its own pool #32 with the BAR's reserve: no clash with
  the 4-round mine/rifle-grenade pools. The 4-5 round reloads need a log of
  a real game ("bots: host buttons" shows any reload press).

## 0.5.499: a BOT took the flag; narrowing the field; weapon pools logged

- 498's trial: bot slot 6, frozen 18 from the enemy flag with X and the
  host's host-only words copied to him, TOOK the flag (and ran it home).
  So bots are refused for one of those player fields.
- Two trials now: A (the host's side, 200 s) copies record +132 +228 only;
  B (the other side, 210 s) copies record +1320 +1324 +1328 only.
- User: the MG42 and the FG42 fire 4-5 rounds then must reload. Every
  weapon's clip size and clip/ammo pools now logged at map load ("weapons:
  pool"), to see whether they share the mines' or rifle grenades' pools
  (held to 4 since 0.5.219).

## 0.5.498: bot trial with the host's own words copied

- 497's trial: a bot frozen 18 from the enemy flag holding X did NOT take
  it; the host (put on top of the bot by mistake) took it 40 s later just
  standing there, and in 496 re-took it several times without pressing:
  the flag is taken by TOUCH, and something about being a bot is refused.
- Now the trial bot gets every host-only word of the 488 diff copied from
  the host each frame (record +132 +228 +776 +1320 +1324 +1328 +1488,
  session +624 +636, entity +620 bit 0). The host's control goes 36 to the
  side of the bot.

## 0.5.497: bot vs host flag trial; bots press like the host

- 496's test: the host TOOK the flag with X (0x40) in his network command
  only, no pad, within 1 s: the "local pad" theory was wrong. (496's read
  watch logged nothing: the interpreter's symbol lookup failed.) Also
  fixed: 493-496's teleport left the height unset (a stale value).
- Unattended test: at 200 s a BOT on the host's side gets the host's
  conditions (put 18 from the enemy flag, frozen, X alone in his command,
  6 s): "BOT TRIAL: the bot TOOK / did NOT take it"; then the host the same
  ("HOST TRIAL", the control).
- In play, bots at the enemy flag now go to 12 out (was 30), stand up, stop
  sprinting and firing, and hold X alone steadily (no turns with activate).

## 0.5.496: which player fields the CTF script reads (bot vs host)

- 495: the pad scan caught mostly the reload animation X starts
  (826FEE38, read by animation code); the take check still unknown.
  sub_821E5718 is the script's "hide" (origin z - 10000); the interpreter
  (sub_82204A08) calls built-ins directly.
- Unattended test: a player's entity (1 KB), record (8 KB) and session
  entry (1 KB) are read-watched, and every read made with the interpreter on
  the stack is logged once per (field, reader): "vmread: <who> <region>
  +<offset> = <value> by <function>". Up to three bots for 3 s each while
  pressing at the enemy flag, then the host during his take.

## 0.5.495: every reader of the pad's X words logged

- 494's test: the padscan found 12 words that follow X (82C2FD7C and
  82C2FDAC hold 0x40 while it is down); the host took the flag with the
  pad again (side 2, -1792 -91, from 18); but no read of those words came
  through the script interpreter's frames during the take.
- Now only the button-like words are watched (the X bit, or small
  counts/flags), and every distinct reader's stack is logged during the
  host's pad taps at the flag.

## 0.5.494: the take reads the local pad: find the reader

- 493's trap and control: X in the host's network command did NOT take the
  flag; the pad's X did (from 18-23 out). So the CTF take reads the LOCAL
  pad, which a bot never has: the reason bots never took a flag. The
  model's move came from the script interpreter: sub_82205788 ->
  sub_82204A08 -> sub_821E5718 -> sub_820C4E60 -> sub_824C65B0.
- Unattended test: "padscan" (X held/let go three times, the image diffed)
  finds the words that follow the pad's X; during the host's pad taps at
  the flag those words are read-watched and every read from the script
  interpreter logged with its stack: the built-in that reads the pad, to
  override for bots next.

## 0.5.493: flag trap (who writes a flag model's position)

- 492: pad assist gave bots nothing (597 presses); the host's control at
  -1792 -91 failed both ways (from 30 out). Probing fields is not finding
  the take's check.
- Unattended test only: the pages holding the flags' script_models are made
  read-only; a write to a model's origin logs the native stack with the
  recompiled function names ("flagtrap: ... written by: sub_..."), then is
  let through. The host's take (now from 18 out, where the user took it)
  names the game's CTF take code, to read its checks.

## 0.5.492: CTF take: does the check read the local pad?

- 491: no script method names in memory (CoD3's scripts are compiled in).
- Unattended test: from 60 to 190 s, while a bot presses use at the enemy
  flag, the local pad's X is tapped too (a bot's take is then logged "WITH
  THE LOCAL PAD'S X TAPPED"). At 200 s the host goes by the enemy flag with
  X in his command only (as bots send it) for 6 s, then the pad's X.

## 0.5.491: CTF take: the script method table searched

- 489's experiment: no group of bots took a flag (987 presses, 54 trigger
  hits); the host took it again. The four host-only fields are not it.
- Theory: the take's script asks useButtonPressed(), which on the console
  reads a local pad and is never true for a bot. The unattended test now
  logs "ctfprobe: method <name>" with where each script method's name and
  its table entry (the function after it) are, for a hook next.

## 0.5.490: spectator: free camera and following players

- New, multiplayer: **F6** spectates (or F11 > Multiplayer > Spectator, or Back + Y on the pad). F6 again stops.
- **Free camera:** flies anywhere while the match runs. Move and look as normal, jump goes up, crouch goes down, sprint is 3x faster (speed slider in F11).
- **Following:** the weapon switch key (Y) goes free camera -> **his eyes** -> **behind him** -> free camera. Fire (RT) goes to the next player, aim (LT) to the previous one, bots and people alike. From his eyes you see his gun, his aim and his shots, the way the killcam shows the killer. If the one you follow dies, the next one alive is taken after 3 s.
- The game has no spectator team you can join mid-match, and noclip only works on a devmap. So this is your own view only: **your player stays where you left him**, and he can still be shot. Your moving, shooting, using and grenades are held back while you spectate. If you die, the game's own death screen and kit menu work as normal.
- How it works: the free and behind cameras are written over the game's view each frame (the killcam's first, camera-only way). "His eyes" gives the game the followed player's live state, flagged as followed, the same way the killcam replay does with a recorded one. This is in the new file CoD3Host/spectator.cpp. bots.cpp only gets one call added in each killcam hook.
- Not tested yet. In the log: "spectator: mode", "spectator: following client", "spectator: cgame's frame drawn as client". If "his eyes" looks wrong, "behind him" is the safe mode.

## 0.5.489: CTF take experiment (unattended test only)

- 488's test: the control worked: the host (god), put 30 from the enemy
  flag and tapping X, TOOK it at once. Host vs all 22 bots differ in:
  session +624 (1/0), session +636 (-1/0), record +776 (1/0), record +1488
  (1.0/0), entity +620 (C001/C000: god), timestamps and the local player's
  view data.
- Experiment: bots by slot % 5 get one candidate each second (0 none,
  1 session+624=1, 2 session+636=-1, 3 record+776=1, 4 record+1488=1.0);
  a bot's take logs its group. The host's control moved to 200 s.

## 0.5.488: CTF take probe (unattended test only)

- In the unattended CTF test (COD3_BIG_MODE set): 25 s after the sides are
  known, "ctfprobe:" lists every word where all living bots agree and the
  host differs (entity 1 KB, player record 8 KB, session player 1 KB); at
  30 s the host (god) is put 30 from the enemy flag and X tapped every
  300 ms for 10 s, as a control ("the flag is the HOST's" / "was NOT taken").

## 0.5.487: CTF flag owners proven by takes and kept (ctf-flags.txt)

- 486's unattended CTF test (BENCH-CTF-ROUEN, 24 bots, 4 min) ran by
  itself. Bots pressed use 1034 times, a trigger fired for a bot, none
  taken: the match-start guess gave side 1 the flag at 2112 462, but every
  user game proved side 1 owns -1792 -91 (481: the user, side 1, took 2112
  462; 482: the user, side 2, took -1792 -91), so bots were at their OWN flag.
  "Teams start at the enemy flag" does not always hold.
- Now the owners come from proof: ctf-flags.txt in the game folder
  ("<map> <x> <y>", side 1's own flag), Rouen seeded; every take writes it
  (the taker's side does not own the flag taken). The start guess only for
  a map with no proof. "ctf sides (proven for ...)".
- 485's scriptvars lines read text, not a variable table (entity +484 is not
  a player's script handle): no longer logged.

## 0.5.486: unattended CTF tests

- COD3_BIG_MODE=<mode> for the big test sets mp_gametype (and g_gametype)
  before the map loads. New BENCH-CTF-ROUEN.bat in the game folder: CTF on
  Rouen, 24 bots, 240 s, closes itself, log kept as bench-ctf-rouen.log.
  Started through test-request.txt (user: "run short tests of these builds
  ... the game run and test itself").

## 0.5.485: script variables compared (bots vs player); dead bodies held still; hops when stuck

- 0.5.484 played: only the user took a flag; dropped at 114 s, it lay out
  the rest of the match with 98 bot runs to take it and 101 to return it,
  none able (user: bots crowding, never scoring). So bots can do nothing
  with a flag, take or return: most likely the CTF script compares a
  player's script-side team with the flag's, and bots lack it.
- New "scriptvars" lines: the host's script variables and the first bot's
  of each side, with what the bot is MISSING, has ONLY, and what DIFFERS;
  20 s into a CTF match and at each take.
- 140 of 484's 150 "SLIDES" were dead or downed bots (health <= 0, state 4
  or 5) gliding 50-300 a second: velocity now held at 0 while dead or
  down (user: bodies sliding, T-poses).
- Stuck: a hop toward the goal at strike 2 (483: not one jump in 1200
  stuck lines); unstuck placement at strike 4 (was 6).
- A dropped flag draws only the middle defenders (their own) and half the
  attackers (the enemy's), not everyone.

## 0.5.484: CTF roles (half attack, half defend, escorts); flag pickup looking at it; sliding bots

- 0.5.483 played (CTF Rouen, 11.5 min): spawns right (user: "spawning on the
  proper side with our flag works now"); dropped flags went home by
  themselves (5 to 28 s); bots took no flag (116 presses with X alone and no
  touch logged at all: with 0x20|0x40|0x400 the trigger saw them but gave
  nothing); 1200 navigation stuck lines; some bots "stuck in a tpose or a
  down animation ... sliding around the map". The settings probe read "?"
  for every name, the time limit too: the probe does not work that way.
- Roles in CTF, by order within the team, whatever the class (user's plan):
  even ranks attack the enemy flag; odd ranks defend, half holding the
  middle (spread across the way), half by their own flag. A team mate with
  the enemy flag: attackers ring round him 150 to 300 out (escort). Their
  own flag taken or dropped: every defender after it.
- Pickup: stop 30 out, face the flag and look down at it (X counts as use
  only with the pickup prompt, which needs the flag in view); 8 s, turns of
  X alone and X with activate (no reload); the TAKEN line says which.
- No moving unless alive and playing (a downed body was driven); new
  "SLIDES" lines for a bot moving while not playing or sliding with no legs
  ordered, with when the port last teleported him.

## 0.5.483: bots take flags with X alone; carriers go home; sides at the start

- 0.5.482 played (CTF Rouen, 14 min, user on side 2): sides right (the user,
  side 2, took the flag at -1792 -91); but decided only at 46 s (4 respawns
  a side), so the match start was not moved; bots pressed use 1092 times
  (762 at the enemy flag, 330 at their own dropped one) and took nothing;
  the dropped flag lay 13 minutes.
- Button: the user took the flag holding 0x40 alone ("host buttons
  00000044, 00000040", then TAKEN); bots held 0x20|0x40|0x400 (the vehicle
  combo, with reload). Now 0x40 alone on flags (FlagUseButton).
- Flag carrier (user: "his mission is to get back to his flag to score no
  matter what"; then: "the flag carrier cant shoot so just make his mission
  home while his team mates defend him"): he sprints along his route home
  (CarrierHome, from g_steer), never firing, stopping or crouching; no long
  way, no seats, revives, packs or mines; attackers escort him.
- Sides at the match start: remembered per map, else from where the living
  stand when CTF is first seen; then everyone away from his flag moved once.
  Respawn spots: nearer his own flag first (482 moved the user farther).
- "objectives: ctf settings: ..." once a map: which return-time setting the
  game has, for the user's 6 s dropped-flag return (not done yet).

## 0.5.482: CTF sides right way round; bots see the real flag state

- 0.5.481 played: the user's whole team (side 1) moved to the enemy flag;
  bots still took no flag; the user took and scored the flag at 2112 462.
- flagwatch showed it: the user (side 1) took the flag at 2112 462 at 61 s
  (its model to z -9970), it lay at 204 -531 at 76 s, home at 102 s
  (scored). Side 1 STARTS by 2112 462, so the game starts each side by the
  ENEMY flag (as reported at 0.5.474); 481 took the nearer flag as its own.
- Own flag = the one farther from the side's first spawns again, locked a
  map. A take now checks it: the taker's side cannot own the flag taken
  ("ctf sides corrected by a take").
- Each flag's state from the game: "flagwatch: flag N TAKEN by slot S",
  "dropped at", "home". Bots: carrying only when the game says they carry;
  attackers escort a team mate carrying or run to it dropped; defenders
  chase the enemy carrying theirs; use pressed at the enemy flag only while
  it is at its base, and on dropped flags.
- Next: bots spreading out (user: "bots clustering together").

## 0.5.481: CTF sides locked from the match start; no fake carrying; flag watcher

- 0.5.480 played (CTF on Rouen, 12.5 min): no flag taken (387 use presses);
  many bots stayed by their spawns; spawns still wrong. Game heap flat at
  583 MB from 1 min to 12.5 min: no leak.
- 480 re-decided the sides from respawns and flipped at 73 s: the game STARTS
  side 1 by 2112 462 but respawns it toward -1792 -91. Now decided once a
  map from each side's first 4 spawns, and a side owns the flag it starts by
  (user: "my team should be spawning near our flag and the enemys team near
  theirs"). Log: "ctf sides (from the first spawns)".
- A bot no longer counts as carrying for having been within 60 of the enemy
  flag at its base (480: 58 turned home without it); only with the flag out
  and on him.
- Defenders: campers and one bot in five (was one in three).
- New "flagwatch:" lines: the script_models and trigger_multiples at the
  flags, compared every 250 ms; a changed word logged with who stands within
  200 (slot, side, bot/PLAYER). Take the enemy flag once by hand to show the
  game's "taken" signal.

## 0.5.480: CTF flags un-swapped (spawns, flag pickup)

- 0.5.479 played (CTF on Rouen, 3 min): bots pressed use on "the enemy flag"
  93 times, none taken; the user: spawns "at the enemy flag" and "stacked/
  stuck"; they took the flag themselves holding use.
- Cause: since 0.5.475 each side's own flag came from g_spawns' centre, which
  put side 1 "by 2112 462" while the game really spawned it around -500 -800
  (the -1792 -91 half). So own and enemy flags were swapped: players were
  moved to the enemy flag, and bots went to their own flag and pressed use
  there.
- Now each side's own flag is decided from its last 16 real spawns (before
  any move): the flag farther from where the game spawns it. Re-decided on
  every spawn (a half-time switch follows); logged as "ctf sides (from N and
  M real spawns)". Until known, no CTF goals or spawn moves.
- Spawn moves go only to the map's own spawn points (g_spawns' learnt places
  dropped), never onto someone when a free one exists.
- Bots press use only at the enemy flag (or a dropped one), held (after a
  200 ms release) instead of tapped.
- 0.5.479's heap line: 589 MB used 30 s in (the map), then flat at 592 to
  the end: no leak in 3 minutes; a long match still needed.

## 0.5.479: the long-game freeze watched (heap); CTF bots step onto the flag

- 0.5.478 played (CTF on Rouen, 31 bots, about 9.5 min): froze (Justin:
  "the game keeps crashing when i play longer games"). Not a crash: the
  watchdog's no-swap; the game heap (heap 0) at 611 of 637 MB, a cache
  (82A91DE0, from 82185D80) asking 50432 bytes for ever: the 0.5.406 heap
  freeze. Before it the 128-byte effects (16000) and the apsEffect pool ran
  empty. The map block cannot grow (the extended window, 0x48000000 to
  0x6F000000, is full at 608 MB + 16).
- New "heap: game heap X of Y MB used, peak, +N MB in 30 s" every 30 s
  (MpFixesHeapReport, from VdSwap), to see whether it climbs all match (a
  leak, then find who) or jumps.
- CTF: 478's spawns were moved by the side's own flag (150+ moves, "ctf: N
  spawns moved"); bots reached the enemy flag and pressed use 23 times, but
  its props never moved (no pickup; Justin: "bots are not actually grabbing
  the flag like i did"). They stood still within 80, short of the flag's
  small touch area. Now they walk onto its very place (within 10) while
  pressing use, 5 s (was 3).
- 0.5.477 / 0.5.478 (lamps, prompts): not yet reported.

---

## 0.5.478: button prompts as keys, switching with what you use (ported)

- Ported from FrankHUN88453's CoD3Recomp (ac3b2a52, "the prompts as
  keys"). The prompts' button icons are characters of one font
  (i_button_icons_xe); while the keyboard and mouse are what was used last,
  the renderer draws that font's texture as key caps naming the keys
  (render_prompts.cpp, his; pc_prompts.cpp, trimmed): every prompt then
  names keys, even text already on the screen.
- Automatic switch (Justin: "if i start the game with the mouse and
  keyboard but midgame use a controller will it then switch"): a pad's
  button, a trigger pulled or a stick past its deadzone shows the pad's
  icons; a key, a click or the mouse moved shows the keys. No pad plugged
  in: the keys. F11, Controls: "Button prompts": Automatic, Controller
  buttons or Keyboard and mouse keys (button_prompts in the ini).
- The labels follow our own input mapping and binds: A jump (SPACE), B
  crouch (C), X use (E), Y weapon (F), the triggers fire and aim (LMB RMB),
  the bumpers grenade and smoke, the cross the arrows, Start pause (ESC),
  Back objectives (TAB); in a menu A is LMB, B and Back RMB, Start ENTER.
- Multiplayer: the front end's manager 0x82A8DF2C is 0x82C6B1FC (25
  matched functions, all agreeing). Left out from his: the campaign's
  tutorial sentences by hash, the Marketplace-as-Quit menu entry.
- The log says once "prompts: the button icon font ..." and "render: the
  button prompts drawn as keys" when they are in use.

---

## 0.5.477: the lamps' glow no longer shows through walls (ported)

- Ported from FrankHUN88453's CoD3Recomp (d35101ce, "render: the depth the
  title reads on the CPU written back to guest memory"). The light coronas
  (oil lamps, the levels' lights) fade by a depth the title reads on the
  CPU from one depth resolve (sub_82420F48, its texture object at
  0x82A2ABDC); the resolves stay on the host, so the title read the far
  plane and every corona shone through walls.
- New cpu_depth.cpp tells the renderer which resolve that is
  (Render::CpuReadsDepthAt); that one resolve is then always written back
  to guest memory through our own depth write back (WriteDepthBack, until
  now only for the debug probe's depth_writeback), not his separate
  compute path. COD3_DEPTHREADBACK=0 turns it off.
- Multiplayer: the function and the object matched by hand to sub_82438DF0
  and 0x82BCC8AC (the same code, scheduled otherwise), in mp_addresses.txt.
- The log says once "render: the title reads the depth resolved to ... on
  the CPU" when it is in use.

---

## 0.5.476: CTF spawns always by your own flag; dropped flags; killcam freeze

- 0.5.475 played (CTF on Rouen, 31 bots). The sides were set right ("side 1
  spawns by 2112 462, its own flag -1792 -91"), but only 17 spawns were
  moved in the whole game: only to a map spawn point no one stood on, and
  there are 1 to 5 by each flag (Justin: "the spawns were still backwards
  ... we need to spawn on our own flag everytime we die").
- Now every spawn more than 900 from his own flag is moved by it, to the
  map's spawn points within 1400 of his flag or where the other side's game
  spawns were (they are by his flag), the one with the most room (none
  with an enemy within 900 where there is one), a little at random; never
  skipped for want of a free spot. "ctf: N spawns moved" every 25.
- A bot took the enemy flag (slot 10; prop 49 off its base), fell, and the
  flag lay at -71 284 the rest of the game. Now a flag whose prop is 100 or
  more off its base is "out": attackers go to it where it is (pick it up),
  defenders to their own (take it back); use is pressed there too, and a
  bot on it counts as carrying.
- Freeze in a killcam (host only; the friend's game was fine): the 1 KB
  particle-memory pool (2048 blocks, four times the game's) ran empty 40
  times mid-game, then the main thread hung in the game's malloc, as in
  0.5.445. The 1 KB pool is now eight times the game's (4096 blocks, 4 MB
  more). Justin: "increase the size of the pool so it doesnt happen again".

---

## 0.5.475: CTF sides spawn by their own flag; more defenders

- 0.5.474 played (CTF on Rouen). Justin: "the enemies spawn on our flag and
  we spawn on the enemies flag making it easy to capture ... Teams should
  spawn on there respected flag side". The game put the allies about
  1300 1600 (by the flag at 2112 462) and the axis about -1650 -1100 (by
  the one at -1792 -91); the bots took the flag by their spawn for their own,
  so 9 "presses use on his own flag" were on the enemy's.
- Now, once a map, each side's own flag is the one farther from where its
  game spawns are (logged: "ctf sides: ..."), and everyone, the players too,
  who spawns nearer the enemy's flag than his own is moved to a spawn point
  within 1400 of his own flag (none within 80 of a living player, away from
  living enemies where there is one; at random among those). The bots' own
  and enemy flags and the use on the flag go by the same sides.
- Defenders: the campers and one bot in three stay by their own flag, each
  his own place 150 to 390 out (was one flanker in three, 300 out); the
  rest go for the enemy flag and bring it home.

---

## 0.5.474: CTF bots press use to take the flag

- 0.5.473 played (CTF on Rouen). Both flags found (-1792 -91 and 2112 462);
  734 "goes for the enemy flag", 134 "guards his flag"; stuck lines down
  from some 2000 to 325, last-resort moves from 262 to 13. Five times a bot
  stood on the enemy flag, but none ever had it (Justin: "still they havent
  picked up a flag"): the flag is taken with the use button (the game's
  hint_flag_pickup), not by touch.
- Now a bot on a flag (within 80, the enemy's or his own) stands and presses
  use in pulses (400 ms on, 200 off) for up to 3 s, then moves on
  ("presses use on the enemy flag").
- The props are followed: one that leaves where it was first seen by 100 or
  more is logged ("objectives: prop N moved from ... to ..."), so the next
  log shows a flag really taken and carried.

---

## 0.5.473: CTF bots go for the flags and carry them home

- 0.5.472 played (CTF on Rouen, 31 bots). The model names read empty, so no
  objective was taken at all: the bots had no flag to go to and milled about
  their spawns (most of the "stuck" lines, and 262 last-resort moves, were
  by the two spawns). Justin: "never seen any bot grab the flag".
- The flags on Rouen are the placed trigger_multiples (two at each base,
  -1792 -91 and 2112 462; the rest sit at 0 0 0). A trigger_multiple off
  0 0 0 is now an objective, one per place (those within 150 the same).
- Carrying: once a bot has been on the enemy flag (within 60) he is taken to
  carry it for that life, and heads for his own flag till he is within 90 of
  it ("home with the flag") or dies. Before, he counted as carrying only
  while still at the enemy base, so he turned round as he left it.
- The log says "on the enemy flag: takes it home" and "home with the flag".

---

## 0.5.472: CTF finds the flags by their models

- 0.5.471 played (War then CTF on Rouen). War: the flags changed hands (the
  middle taken by side 2 at 92 s, flag 2 then in play, the middle back in
  play at 158 s); 0 to 6 of a side near the flag in play at a time. The
  mutant repair of 0.5.470 fired once (0x104, 284 s, the map change): no
  freeze.
- CTF: no flag ever picked up (Justin). The trigger_multiples taken as the
  flags have their origin at 0 0 0 (a brush trigger's place is its bounds),
  so the bots went to the map's middle ("goes to the trigger"); and the
  props' listing (60 lines) had been used up on the map before.
- Now a script_model's model is read by name (the text +540 points at, or
  at the pointer it holds), and in CTF the props whose model name has
  "flag" in it are the flags (in Headquarters, "radio" or "hq"); nothing at
  0 0 0 is taken; the props' listing is kept per map and mode (80 lines),
  with their model names.

---

## 0.5.471: War: bots take the flag in play and push on

- 0.5.470's War on Rouen: the middle flag (1) in play from 91 s to the end
  (386 s), never taken; both sides' bots went for it (about 250 times) and
  traded shots round it, never holding it (Justin: "the enemy bots are not
  catching and hoding the objective and pushing to the next").
- A taker keeps on to the flag unless an enemy is within 250 (was 600), and
  fights from on it, crouched (as before when on it).
- 70% of each side plays the flag in play (was 55%; 14 at most, was 11), two
  in three of them takers (was one in two), the rest screening the enemy's
  way in. Once a flag is taken the War script puts the next in play and the
  bots go on to it.
- Every 10 s the log says the flag in play and how many of each side are on
  it (150) and near it (450): "war: flag N in play ... (contested)".

---

## 0.5.470: the freeze loading CTF on Rouen (a lock that could never be taken)

- 0.5.469: Justin's game froze for good loading CTF on Rouen (after a War
  match with bots). The watchdog: the main thread (loading the map,
  sub_82141E50 under sub_8252D870) and a worker waiting with no timeout on
  mutant 0x104 (the busiest lock, signalled 86 million times), which was
  "free" (no owner); the main thread in the infinite wait's 3 s re-look,
  never able to take it.
- A mutant's owner is kept here, but whether it is signalled is read from
  its body in guest memory (+DISPATCH_SIGNAL_STATE): with that word written
  0 by anything while it had no owner, it could never be taken again.
- Now a wait that has gone 3 s on a mutant with no owner that does not read
  as free puts it right (signalled, depth 0), and the waiter takes it at
  once: a hitch of 3 s, not a freeze. Logged: "mutant repaired".

---

## 0.5.469: bots take stairs, look round at the first wall, and keep moving

- 0.5.468 played (War on Rouen): median 72 fps; the sound cap stepped in
  (76 reports, some 375,000 decodes put off), and the sounds counted by
  name were few (artillery, pain, the UAV's engine), so the flood comes by
  the weapons' sound events (31 bots firing). Walls met 83, stuck 69, 80
  look-rounds, 2 unstuck. Justin: bots "walk into walls and not find the
  door or stairs ... clear houses, and rooms and keep moving on unless
  needing to stay to defend or attack objectives".
- Stairs: the feeler 18 above the feet met a stair's later risers and the
  stairs were slid along as a wall. Now with the way clear 44 high and a
  floor ahead 2 to 40 above his feet, flat enough to stand on, it is a step
  and he walks on up. Logged: "bots: feelers: slot N takes the stairs".
- The look round (24 ways, 240 out, knee and chest high) at the first wall
  met (was the second at one spot), every 2.5 s at most, and a way 55 clear
  is enough (was 70: narrower doorways).
- On an errand, at each spot reached, all but the campers move on after 5
  to 10 s (was 15 to 35 s); the flag, push, exit and overwatch waits as
  before.

---

## 0.5.468: the sound flood that sank Rouen to 8 fps capped; finding its source

- 0.5.467 played (War on Rouen, 31 bots). The bots far better at walls:
  walls met 39 (was 357), stuck 36 (was 110), 80 look-rounds, 3 unstuck.
  But (Justin) "the game got very slow and sounded like an alien running at
  8fps towards the end": from 134 s, 5 to 13 fps for 45 s, the bots' own
  work 2 to 3 ms a frame, the GPU 17 ms, the frame 100 ms. The sound: 100 to
  260 XMA contexts going in each audio frame (peaks 440 to 516), where 40 to
  60 had been, from about when the bots began reaching War's flag in
  numbers (the flag's capture loop, MP_FlagCap_loop, was asked for then).
- Now at most 128 contexts are decoded in one audio frame; past that a
  context is enabled but decoded on its next turn (logged once and every
  5000: "xma: more than 128 sounds going").
- Every 10 s the log names the sounds asked for most ("sounds: the last 10
  s, most asked for"), to find what flooded.
- The bots' route look-ahead traces every 250 ms (was every frame).

---

## 0.5.467: bots look round for doors and stairs, and follow their routes round corners

- 0.5.466 played (War on Rouen): 110 "stuck" (was 193), the worst at strike 6
  (was 9), 4 unstuck; routes found for 35 of the first 40 asked; the UAV
  "looked and sounded good". But 357 walls met, and Justin: "still noticing
  alot of bots just walking into walls ... they need to learn to turn and
  look around and walk through doors and into other rooms and be able to
  walk upstairs and keep actively searching for new routes".
- Following a route: the farthest of the next four standing spots he has a
  clear line to (knee and chest high), else the next one (466 walked
  straight at the next spot, 128 to 256 off, cutting corners into walls).
- A look round at a wall: meeting one again where he met one (within 80, in
  12 s), he sweeps 24 ways (every 15 degrees) 240 out at knee and chest
  height (a doorway or a stair's opening is clear at both) and takes the
  clearest way (70 at least), the nearer to where he was going the better,
  held 1.5 s. Logged: "bots: feelers: slot N looks round".

---

## 0.5.466: bots route to the objective over the learnt map; unstuck at last; Rouen's UAV circle

- Justin, War on Rouen: "alot of the bots seem to get stuck walking around,
  we need to fix that and have them actively find a way to the
  flag/objectives". The log: 193 "stuck", 62 of them round -800 0, strikes
  up to 9 (half a minute and more); the bots steer straight at their goal
  and feel their way round what they meet.
- Now a bot with an objective (War's flag, Domination's, the CTF and
  Headquarters ones, the long-way point) follows a route over the learnt
  map (where players have walked, cell to cell: MapKnowledge::Path, A*,
  deaths dearer) a standing spot at a time, worked out again every 3 s or
  when the goal moves 200; straight at it only when no route is known.
  Logged: "bots: slot N routes to his objective: K steps".
- Stuck six times running (some 20 s), with no enemy in sight within 1500,
  he is put three standing spots along that route (the game's own
  teleport), or by his side's spawns when no route is known. Logged: "bots:
  navigation slot N unstuck".
- The UAV plane on Rouen (Justin: it flies "to far on the outskirts ... they
  flicker in and out of the draw distance"): its circle 10% tighter and
  its height above the players 2% more.

---

## 0.5.465: bots no longer spawn into the enemy's sights

- Justin on 0.5.463's CTF: "the enemy bots all kept seeming to spawn in one
  area i did a couple times with them too and they just beamed me
  everytime". Its log: most bots of both sides were spawned by the game in
  the map's middle (Rouen, around 0..750 x 0..750), the CTF bases far from
  it (allies about 1250..1800 x 1250..1800, axis -1000..-1650 x
  -1450..-1850): both sides met there at every spawn.
- Now after the game spawns a bot, with a living enemy within 1200 of him he
  is moved (the game's own teleport, sub_8255E118, as the return point's) to
  his side's spawn point (a spawn entity named for allies or axis, any spawn
  when the map has none) farthest from every living enemy, when that is at
  least 300 farther than where he was put. Logged: "bots: slot N spawned D
  from an enemy: moved to his side's spawn".

---

## 0.5.464: CTF bots go for the flags, not the spawns

- 0.5.463's CTF match on Rouen (Justin): the mode was seen ("ctf"), but the
  "ctf" in the spawn points' names (spawn_ctf_allies_primary, ...) made them
  the objectives, so the bots ran at the enemy's spawn. The map's other
  class names were worldspawn, info_player_deathmatch, spawn_intermission,
  script_model and trigger_multiple.
- Now names starting with "spawn" are never objectives, and in the flag
  modes (ctf, sctf) the trigger_multiple entities (the flags' touch areas)
  are. The first time on a map every script_model and trigger_multiple is
  logged with where it is and its model ("objectives: trigger_multiple N at
  ..."), to tell the flags (and the Headquarters radio) for certain.

---

## 0.5.463: bots play Capture the Flag, Single Capture the Flag and Headquarters

- 0.5.462 tried by Justin: the lobby's max players goes to 32.
- Bots knew War (its flag in play) and Domination; in the other objective
  modes they only fought (Justin: "make sure they can play and learn other
  games modes"). Now every 2 s in any mode but War, Battle and Team Battle
  the entity table is read for entities whose class name has "flag", "ctf",
  "hq" or "radio" in it, and where they are. The first time in a map every
  class name seen is logged ("objectives: mode ... class names") and each
  objective found ("objectives: <name> at x y z").
- Two flags or more (Capture the Flag): a side's flag is the one nearest its
  spawn, the enemy's the one nearest theirs. Objective players and rushers
  go for the enemy's, and with it on them (within 90: carried) back to
  their own to bring it in; one flanker in three guards his own flag (300
  out). One (Single Capture the Flag, the Headquarters radio): all but the
  campers go to it. Objective bots go onto the flag itself (no ring round
  it as in Domination). Logged: "bots: slot N plays <mode>: ...".
- The lobby's debug logging (0.5.458 to 0.5.462) taken out.

---

## 0.5.462: the max players list made with room for 32

- 0.5.461's log: the right screen at last ("create screen setup: ... choices
  6 (counts 4 8 12 16 20 24) 32 players on") and 28 and 32 added, but the
  list stayed at six: FEMenuListBox's constructor (sub_824E8FC8) takes how
  many choices it will hold (r5, kept at +30, its array allocated for that
  many, with the game's own allocator) and both adds stop there.
- Now while the create screen's setup (sub_823AA120) runs, a list made with
  room for six is made with room for eight (the F11 "32 players" setting
  on), so its array is the game's own, of the right size, and 28 and 32 go
  in after the six.

---

## 0.5.461: the lobby's max players to 32, on the right screen

- 0.5.460's log: neither 0.5.459 hook ran on Justin's create screen (they
  are another screen's), and its max players list was never added through
  the spinner function logged (sub_824FFA58).
- The System Link create screen's setup is sub_823AA120: the max players
  list (the menu's +276) is built from a table of six counts (0x82061990),
  each turned to text and added with sub_824E8E18, the counts kept in the
  menu at +156 (room for 30) for when the choice is taken.
- Now, with the F11 "32 players" setting on (and the six as expected, the
  sixth 24), "28" and "32" are added the same way after them, and their
  counts written into the menu's table after the six, so the title's own
  code takes them as it takes the others. 0.5.459's hooks are gone. Logged:
  "lobby: create screen setup" (the menu, the list, its choices and the six
  counts) and "lobby: max players list to 32".

---

## 0.5.460: the lobby's max players: what the screen's setup sees

- 0.5.459 tried by Justin: the list still stops at 24, and nothing of its
  own in the log ("lobby: max players" never written), F11 at 32 players:
  its setup hook ran into one of its checks (the list at the menu's +300
  with six choices) or is not the screen's.
- Now both hooks say what they see once entered ("lobby: create screen
  setup: menu, list, choices, choice, count, 32 players on/off" and "lobby:
  create screen taken"), and the choices log keeps only number choices
  (0.5.458's 400 lines were used up by the words before the 4 to 24 list).

---

## 0.5.459: the lobby's max players goes to 32

- 0.5.458's log (Justin started matches at 12 and 24): the count is the
  game settings block's byte +91 (0x0C, 0x18). The System Link create
  screen's setup (sub_823AA900) turns it into the max players list's choice
  (count / 4 - 1: 4, 8, 12, 16, 20, 24; the list at the menu's +300, so the
  "10" asked for was 12), and taking the screen's choices (sub_823B0B98)
  turns the choice back through a table of six in the menu (+180).
- With the F11 "32 players" setting on, the list now has 28 and 32 too. A
  pick of either is taken as 24 by the title's own code (its table has six),
  then the byte (both copies of the block) is set to 28 or 32. A count past
  24 kept from the last match is shown as 24 to the setup (it reads its
  choice's text at once), then the list is filled to 32 at that choice.

---

## 0.5.458: finding the lobby's max players choice (to take it to 32)

- Justin: "in game settings it only goes up to 24 ... can we raise the in
  game menu to 32 now?" The F11 "32 players" switch already makes room for
  32 at start-up; the lobby's own choice stops at 24. Its choices are added
  one by one at run time (FEMenuListBox, sub_824FFA58), but how the pick
  becomes the player count (a number read from the text, an index plus a
  base, or a table of 24) is not known, and choices past the table's end
  would break hosting.
- This build logs it: every choice added to a spinner ("lobby menu: spinner
  ... choice N 'text'", the first 400) and the game settings block (both
  copies, 0x82680CA8) as each match starts ("lobby settings: block"). With a
  match started at two different max players, the field and its meaning
  show, and 0.5.459 can add 25 to 32.

---

## 0.5.457: bazooka bots draw their pistol up close; more crouch and prone; the long way round

- A launcher bot (bazooka, panzerschreck) never fires within 300 units, so
  up close he stood there: now with an enemy in sight within 350 he takes
  out his pistol (colt, p38, webley or tt33, owned and loaded), and his
  launcher again past 700 while it has rockets; 3 s between swaps.
- Stopping to shoot from a crouch or prone (Justin: "not alot of bots take
  cover or go prone"): past 450 units (defensive 300; was 700 and 400), two
  times in three (was one in three); prone when hurt (60 or less),
  defensive or past 900 (was 50 and 1100), half the time.
- The long way round (Justin: "still some grouping up instead of splitting
  up even if that means taking the long way to the objectives"): to an
  objective 1400 or more away, two bots in three first go to a point of
  their own beside the straight way (half way along, 600 to 1200 off it,
  left or right by turns, on ground a trace finds near his height), then on
  to it; done there or 20 s on. "bots: slot N takes the long way" in the log.
- Built without a test run (Justin). Left for next time: the other game
  modes, the brightness slider in the multiplayer menu.

---

## 0.5.456: bots aim at the body, settle their aim, use their sights and packs more

- Aim (Justin: "aim more from the waist to the upper chest area with less
  being killed immediately by headshots because they seem to just lock on
  and beam me"): the point aimed at was the target's eye height less 2 (the
  head); now 45% to 75% of his eye height (waist to upper chest), drifting
  every 0.7 s, each bot on his own rhythm (crouched and prone scale with it).
- The shot's spread settles: 6 degrees at first, down to 2 over 1.2 s from
  first sight of the target (was 2.5 always), 1.5 more from the hip.
- Reaction 450 to 850 ms before the first shot (was 350 to 650).
- Sights up (ADS) at anyone in sight past 250 units unless charging (was past
  700, and never when playing aggressive).
- Ammo and health packs (0.5.453's whole test: one of each): only an enemy
  within 1500 stops it (was 5000), one teammate within 700 is enough (was two
  within 450), or none when he needs it himself (a medic at 70 health or
  less, a support with 30 rounds or fewer to spare); 30 s between (was 60),
  the first 12 to 27 s into his life.
- Rifle grenades more often: riflemen every 7 to 11 s (was 10 to 16),
  Demolitionists 5 to 7 s (was 6 to 9).
- Left for next time: bazooka bots taking out their pistol up close, cover,
  prone and splitting up, the other game modes.

---

## 0.5.455: noclip by a double tap; the return point a cheat; brighter; bots one grenade at a time

- 0.5.454 tested: 20 or more Hellfire kills (the log's count stops at 20),
  the artillery's damage type (means 18) seen, 20 and 19 deaths in its two
  busiest 10 s, no crash, no pool empty.
- Noclip (Justin: "double tap a to come out of it or to activate it if i
  use the console commands to put it on"): once noclip has been typed in
  the console, two presses of A (or the jump key) within 350 ms send noclip
  again, on or off: drop to the ground and fly again without the console.
- The return point (L3 held standing still saves the spot, X held 2 s goes
  back) is now a cheat in F11 ("Cheat: return point", return_point in
  CoD3Recomp.ini), off by default: L3 and X work as normal without it.
- Brightness in F11 now goes to 200% (was 150%) for the darker maps.
- Bots' grenades (Justin: "when bots throw smokes or sticky grenades its
  like they throw multiple at a time"): 0.5.453's log had the throw keys'
  search wandering (a sticky thrown with 0x2 was not counted as a smoke, so
  the smoke search went on to 0x8000, and frags pressed 0x2 and "spent
  nothing", so they were thrown again). Now only the two grenade keys (0x2,
  0x80) are tried, a bot throws no grenade of any kind within 2.5 s of his
  last, and each press is 200 ms (was 350).

---

## 0.5.454: the Hellfire deadlier, its kills in the kill feed, 200 shells

- 0.5.453's test: 20 Hellfire kills in its 20 s (the caller's, "killed by
  host 0", and teammates' own), but Justin "wasnt seeing anybody die on the
  killfeed": they were the mortar's splash (means 14), which the game's
  multiplayer never uses. Now as the artillery's (means 18), still counted
  for no killstreak.
- Deadlier (Justin: "make sure they actaully kill people now will lower the
  amount a little"): death within 280 units (was 170), the blast to 700
  (was 560), 60 at its edge, the concussion to 1000 (was 950); the shells by
  the players 80 to 900 units off (was 120 to 1200).
- 200 shells (was 240), one every 95 ms (still 19 s).

---

## 0.5.453: the Hellfire's glow fades; its shell entities used by turns

- 0.5.452 played by Justin: "i seen the explosions ... it was intense bro"
  (the artillery's own explosion at every shell); its test: no crash, no
  pool empty, 22 deaths in its busiest 10 s. But "those little red balls
  stay around the map ... they dont ever disappear ... we can keep the
  glow its cool but just let it fade out after a little while".
- The glow (the panzerschreck's, played on the effect origins) stays on its
  origin until the origin is moved. Each origin used by a shell is now put
  away under the map 6 s later (not while it is in use again), so each
  glow lasts about 6 s and the last ones go 6 s after the Hellfire.
- 0.5.451 made a new entity for every shell (240 a Hellfire, never freed);
  now 48 used by turns.

---

## 0.5.452: the Hellfire's shells explode as the artillery's

- 0.5.451's test: the game's shell function ran on every Hellfire shell
  (its think 9 set, sub_8255D7C0) and its incoming was heard, but think 9
  only places the shell (think 8 30 s on); still no artillery damage. Justin:
  "i can hear it but i cant see or feel it".
- The artillery's explosion, from the game's files: the strike's shells are
  the spotter binoculars' projectiles (weapusge.cod: projectileDelay 6000,
  projExplosionEffect), and they explode by surface with the events
  projexplode_mp_binoculars_<brick|concrete|dirt|flesh|glass|grass|metal|
  mud|none|rock|sand|water|wood> (in every map's event table), whose effects
  are artillery/artillery_<concrete|dirt|metal|generic>.effect.
- Now each Hellfire shell plays that event by name at its spot (as the
  planes' sounds are played): dirt, grass, mud, rock and concrete by turns.
  The explosion comes 1.1 s after the game's own incoming (the shell entity
  of 0.5.451), our stand-in whistle gone on the host. The panzerschreck's
  fire, the mine's blast near the player, the damage and the concussion as
  before.

---

## 0.5.451: the Hellfire's shells are the game's own artillery shells

- 0.5.450's test: the spotter binoculars have no explosion of their own
  (its effect, sound and projectile fields all empty), so its "artillery
  explosion" drew nothing. Justin: "i could hear it but i couldnt see it
  and it only felt isolated in one area not all around".
- Found in a real strike (bench-ordnance-city.log): after the call,
  SHELL_INCOMING is played from 0x82583770, in sub_82583728: it takes a
  shell entity, plays the incoming sound on it and sets it to think again
  1.1 s later as think 9 (+764, the time at +760). G_RunThink
  (sub_8255DA38) calls think 9 from the table at 0x825FCFD0: the shell's
  explosion (SHELL_EXPLO_MAINIMP followed).
- Now on the host each of the Hellfire's 240 shells is also an entity made
  at its spot and handed to sub_82583728, so the game itself whistles and
  explodes it as an artillery shell, and sends it to every player. Ours
  (effects and blast) stay with it for this test; the log says which
  damage was the game's artillery ("artillery: a shell's damage", means 18).

---

## 0.5.450: the Hellfire's artillery call once, the artillery shell's explosion everywhere

- 0.5.449's test: "Artillery strike called." on the screen and the
  compass's artillery markers for every one of the 48 barrages, but still
  no artillery damage at all (no "artillery: a shell's damage" line), and
  Justin: "no actual artillery explosions and animations yet ... i see the
  red dots ... also its still repeating it when you call the hellfire in,
  its only supposed to say it once". The game's barrage (sub_821F70A8) is
  the strike's call (message, firing, scout's call, markers), not its
  shells.
- Now the game's barrage once, with the first shell (its call once); every
  shell after is ours: 240, one every 80 ms (19 s), half by the living
  players (120 to 1200 off), half over a 10 x 10 grid of the map.
- Each shell's first effect is the spotter binoculars' explosion
  (mp_binoculars, event 56): a real strike's events are the binoculars'
  (the strike is their projectile), so its explosion should be the
  artillery shell's. The log says once what that weapon's explosion effect
  and sound are ("killstreak: mp_binoculars ... explosion effect").
- The incoming sound is the artillery's (IMP_INCOMING, was the mortar's
  whistle). The panzerschreck's blast and, near the player, the mine's, as
  before; the blast's damage and the concussion as before.

---

## 0.5.449: the Hellfire's artillery barrages drop their shells

- 0.5.448's test: 48 barrages called for the caller, his dispenser started,
  but no artillery damage at all; Justin saw "little red balls around the
  map" (the barrages' markers) and none of the artillery's shelling or
  sounds ("i just want this being one really big artillery across the
  whole maps radius").
- 448 parked the dispenser's ready time an hour away, as after a killstreak
  strike; an armed strike's is kept at 0 (ready) until its barrage comes.
  Now the ready time is 0 from the start and before each barrage, and
  parked an hour away only when the Hellfire is over.
- "artillery: a shell's damage ..." in the log (the first 40): the game's
  artillery landing (means 18).

---

## 0.5.448: the Hellfire is the game's own artillery, with mortar rounds mixed in

- Justin on 0.5.447: "no shell shock, no big explosions like the
  artillery ... switch it to the artillerys shelling", "make sure shelling
  happens accross the whole map artillery style, maybe mix in a bit or
  mortar style too".
- Why 0.5.438's game barrages did nothing: the barrage (sub_821F70A8) works
  only for a player with an artillery dispenser, which a killstreak's
  artillery starts before its call (and its repeat 8 s later works with the
  dispenser's ready time parked). The Hellfire's caller had none.
- Now the Hellfire starts the caller's dispenser (StartDispenser), parks its
  ready time an hour away, and from 400 ms on calls the game's whole barrage
  48 times, one every 420 ms (20 s): the game's own shells, explosions,
  sounds, shock and damage, seen by every player as the game sends them.
  One in three by a living player (each in turn), the rest over a 6 x 6
  grid of the map (the spawn points' box and the ground covered, 800 past).
- With each barrage three of our own shells (the mortar's landing, as since
  0.5.440: effects, whistle, blast, on every machine) over more grid cells,
  so the map between the barrages is shelled too.
- The artillery's firing and the scout's call (MP_Artillery_*, Scout_*) are
  let through for the first barrage only, not 48 times.
- The bombers as before. Its kills: the game's artillery (means 18) and our
  shells count for no killstreak.

---

## 0.5.447: the Hellfire twice as thick

- Justin: "i still feel like the hellfire wasnt accross the whole map's
  areas". 144 shells over the whole map were about two for each house-sized
  spot in 18 s: from any one place, a few.
- Now 288 shells, one every 70 ms (20 s). Half by the living players, each
  in turn, 120 to 1200 units off (was one in four, 150 to 900); half over a
  10 x 10 grid of the map (was 8 x 8), 800 units past the spawn points' box
  (was 400).
- 0.5.446 tested (rimling, 20 bots): no "pool is empty" at all (four
  times the particle pools), no crash, 48 whistles; but 1 shell shock in
  the whole run (Justin: "it just didnt have that intensity yet"). The
  concussion (560 to 950 units) is now 12 damage as a grenade's splash
  (means 11; was 4 as the mortar's, which the game gives no shock), still
  never a death.
- A shell within 1800 of the player: its third effect the mine's blast
  (mp_mine; was a second panzerschreck).
- 0.5.446 also carries the planes' own sounds (ARPL_) as the Garand's shot,
  not the Kar98's that the Hellfire's explosions and whistles take, and the
  bombers' engine every 1.3 s (was 1.8) and flyby every 1.1 s (was 1.5).

---

## 0.5.446: the killcam freeze with 30 bots; the Hellfire made heavier

- 0.5.445 tested over the internet without Tailscale (Justin hosting, his
  friend joined, 30 bots, Domination): the lobby rules, the bots, both
  players' UAVs (seen and heard on the other machine) and Domination all
  came over the game's way ("public: side channel message ... came on the
  game's way"). The 0.5.436 limit kept the log whole.
- It froze as a killcam began (233 s), and the friend was dropped. The
  watchdog's stacks: the main thread for ever in the game's malloc
  (sub_820DF428 on the heap at 82A84120, its lists broken), from killcam
  set-up code (sub_82548C20). Before it, ~2700 "apsMemory : pool is empty"
  in 10 s: the particle system's effect nodes (224 bytes, 4096), effects
  (128 bytes, 8192) and its 32, 512 and 1024-byte blocks (1024 each), all
  already twice the game's (0.5.205, 0.5.242).
- Now four times the game's: 8192 effect nodes, 16000 effects (the most),
  4096 of the third kind, and 2048 of each block of 1 KB and less (the
  larger blocks and the group memory stay twice). About 4 MB more.
- The Hellfire Barrage (Justin: "the next level intensity patch for our
  hellfire streak and its sounds and animations"):
  - each shell is heard coming in: the falling shell's whistle (Mortar_Miss,
    the mortar's) 250 above its spot when within 2500 of the player (one
    each 350 ms at most), and it lands 700 ms later;
  - a shell within 1800 of the player has a second fireball 70 units off
    the first (96 effect origins, was 64), for a wider blast;
  - from the blast's edge (560) to 950 units: a concussion, 4 damage as the
    victim's own, for the game's flinch and shock as from its artillery
    (never a death: only with 25 health or more left).
  - Effects per shell otherwise kept at two, so the particle pools above
    are not filled by it.

---

## 0.5.445: the Hellfire's explosions made to be seen; killstreaks at 2, 4, 6, 8

- Killstreaks at 2, 4, 6 and 8 kills in a row (Justin): the UAV 2 (was 3),
  the mortar 4, the artillery 6 (was 5), the Hellfire Barrage 8 (was 7).
  The host's settings choose (F11; killstreak_*_kills in CoD3Recomp.ini).
- The first try at this build (0.5.444) did not build: three log lines of
  its had a line end in place of 
.
- 0.5.442's test (it had 443's changes but the killstreak rule): 104 shells
  logged from -891 to 3832 x -2478 to 2398, 59 deaths in its 18 s against
  41 in the rest of the game; but Justin saw "no explosions or kills ...
  or sounds of planes flying above" (its kills, counted for no killstreak
  since 441, no longer show as "client 0 killed" in the log).
- Each shell's effects: the frag's and the panzerschreck's again (the
  mortar's two seen ones, 0.5.298 on). 442 had the campaign round's in
  place of the frag's, and that one is not seen (0.5.308): one seen effect
  a shell.
- The 64 origins of the effects are made ahead (under the map), and the
  first shell falls 300 ms later: 442 made each as its first effect was
  played on it, and an event on an entity made in that frame is not seen.
- Each origin is moved to its spot with the game's mover (moveto, which the
  players' snapshots carry) and its effect played 150 ms later, the blast's
  damage with it: 442 wrote the spot into the entity alone (PlaneFx's way),
  so for the players the effect stayed where the origin had been.
- COD3_HELLFIRE_TEST: every 24th shell 450 units in front of the host's
  player, with a screenshot 0.4 and 1 s later (screenshots\), to see them.
- (Justin: "i want this intense and crazy like the artillery".)
- The explosion's sound within 3000 (or one shell in six), at most one each
  400 ms: fewer of the Kar98 event's voices taken from the bombers' sounds.

---

## 0.5.443 (built in 0.5.445): the Hellfire's explosions seen everywhere, the bombers heard, lower; killstreak kills earn no streak

- Justin watching 0.5.440: "still no hellfire around the map", "there was
  no plane sounds flying by", planes "glitch in and out before they all
  rendered in".
- The explosions: every effect was played on the UAV plane's one origin
  (PlaneFx), moved to each spot in turn; an entity carries one event a
  snapshot and its effects go with it, so most of the 144 were never seen
  where they fell (the blasts' damage was there: 17 kills). Now each effect
  has an origin of its own (64, by turns, each used again 4 s on): two a
  shell (the frag's or the campaign round's, and the panzerschreck's).
- The bombers' sound: every sound of ours is the Kar98's shot event with
  its cue swapped, and the explosions' eight a second took its voices. The
  explosion's sound now only within 4000 of this machine's player (or one
  shell in four), at most one each 200 ms.
- The bombers 2200 above the highest ground stood on (was 3000), each wave
  150 higher than the one before (was 250), from 1500 past the map's edge
  (was 3000): nearer, so not at the edge of the draw distance.
- Killstreak kills earn no killstreak (Justin: "once used they shouldnt
  count towards more streaks, so even the mortar and artillery"): kills by
  the Hellfire, the mortar's blast (means 14) and artillery (means 18) are
  still the killer's on the scoreboard, but his run of kills stays where it
  was. As before: each reward once a life, at its count; ones not called
  are kept through death and add up with the next earned.

---

## 0.5.441: a Hellfire's kills earn no killstreak (built as 0.5.442)

- 0.5.440's test (rimling, 20 bots): 144 shells from -911 to 3745 x -2414
  to 2195 (the whole map's box), 17 kills in one Hellfire, all the
  caller's; the bombers flew (3 waves of 5, 3200 up, 7.3 s a pass); 50 to
  100 fps through it; no crash. But its kills earned the caller the next
  Hellfire 4 s in (7 in a row): for a joiner or a bot (theirs fall at
  once) one would have followed another for good.
- Now a Hellfire shell's kills are still the caller's (the scoreboard) but
  count for no killstreak, as Call of Duty 4's killstreak rewards.

---

## 0.5.440: the Hellfire Barrage bombs the whole map, with bombers over it

- 0.5.439's test: the game's barrage, called at our spots, did not drop its
  shells there (Justin: "only isolated to a certain area", "im not feeling
  it every where"): no artillery death at all in that run, and the first
  run's went to a bot that had called the game's artillery before. Its
  firing call also sounded with every one ("only once should be enough").
- Now every strike is an explosion of ours, at its spot, as the mortar
  killstreak's round lands: the panzerschreck's and the frag's effects (and
  the campaign round's, when Laison's weapons are in) and Mortar_Explo, on
  every machine (C3KH from the host, so joiners see and hear each one, over
  the internet too); the blast on the host: every living player within 560
  units (400 damage within 170, 45 at the edge, a third through cover).
  The caller's damage to his enemies (his kills, his killstreak), the
  victim's own to the caller and his side: it hurts everyone.
- The whole map ("we essentially need to just bomb the whole map"): 144
  shells, one every 0.125 s (18 s). Three in four over an 8 x 8 grid of the
  map (the spawn points' box and the ground covered, 400 units more round
  it), each cell in a shuffled order, then again; one in four by a living
  player, each in turn.
- The artillery's firing call once, at the start.
- 0.5.439's attacker swap in G_Damage is gone (our shells name the caller).

- With a Hellfire Barrage, bombers fly high over the map while the shells
  fall: 3 waves of 5 planes in a V (the lead, then pairs 300 units to each
  side and 350 back), one wave every 6 s, each on another heading, from
  3000 units past one edge of the map to as far past the other at 1800
  units a second. Height: 3000 above the highest ground the players have
  stood on (COD3_BOMBER_HEIGHT to change it; each wave 250 higher than the
  one before). Typhoons and Stukas (the UAV plane's models) by turns.
- Every machine flies its own (the host sends C3KB, as the UAV plane's
  C3UP, 0.5.275), so a joiner sees and hears them too, over the internet
  as well (0.5.437).
- Their sound: the P-47's engines (ARPL_P47_1 and _2 by turns, every 1.8 s,
  while a wave is within 12000 across) and its flybys (By01..07, every 1.5
  s within 6000), on a sound origin of each wave's that follows from the
  player's eyes toward its lead, 900 out at most, as the UAV plane's.
- The planes are kept, parked far under the map, for the next Hellfire.

---

## 0.5.439: the Hellfire Barrage hits the whole map, and its kills are the caller's

- 0.5.438's test (rimling, 20 bots): all 30 barrages fell, no crash, 80 to
  100 fps through it, strikes from -595 to 3433 x -2368 to 2423. But the
  area was only the ground the players had covered so far, and its 6 kills
  went to a bot that had called the game's own artillery before (the
  shells take the last caller of the game's artillery).
- The whole map: the box of the map's spawn points (as the UAV plane's
  circle) joined with the ground covered, 400 units more round it, cut into
  a 5 x 5 grid; the cells are hit in a shuffled order, a random spot in
  each, so no part of the map is missed. One in three still falls by a
  living player, each in turn. Off the map (nothing under the spot): a
  second spot halfway to the middle, else by a player.
- 40 barrages (was 30), one every 0.45 s (still 18 s).
- The kills: while a Hellfire's shells fall (12 s after the last), artillery
  damage (means 18) is the caller's to his enemies (his kills, his
  killstreak) and the victim's own to the caller and his side, so it hurts
  everyone (friendly fire off or not). A teammate killed by it counts as a
  suicide.
- The log shows the first damages during a Hellfire as given ("damage
  during the Hellfire"), to check the means' place.

---

## 0.5.438: the Hellfire Barrage (the Typhoon run's place)

- The Typhoon's strafing run is now the HELLFIRE BARRAGE: artillery over the
  whole map, for everyone. Same place on the ladder (killstreak_typhoon_kills,
  7 in a row; F11 "Kills for the Hellfire Barrage"), same box ("HELLFIRE").
- No binoculars: pick it (d-pad down) and d-pad right unleashes it at once.
  30 of the scout's whole barrages, one every 0.6 s (18 s). Every other one
  falls by a living player, each player in turn, 150 to 900 units off him
  (the caller too: no one is safe); the rest anywhere on the ground the
  players have covered on this map, 400 units past its edges, on the
  ground found by a line straight down (off the map: by a player instead).
- Joiners' and bots' Hellfire (counted on the host) falls as soon as it is
  earned. One Hellfire at a time; a second waits until the first is over.
- COD3_HELLFIRE_STRIKES (4 to 80) sets how many barrages; COD3_HELLFIRE_TEST
  =seconds starts the host's own that long after its player is in play
  (BENCH-HELLFIRE.bat: rimling, 20 bots). streak_give hellfire in the console.

---

## 0.5.437: public games get the rules, bots, UAV and Domination too

- 0.5.435's internet game played, but this runtime's own messages (the side
  channel: the host's lobby rules, the bot roster and lives, the UAV plane,
  killstreaks, mortars, Domination's flags and score) went to port 28964,
  to the LAN broadcast, the peers list and the last sender. Over the
  internet the routers have only the game's way open (the punched port
  28960), so a public joiner had none of it.
- Now each side channel message to a PC met through the master server goes
  on the game's socket, to its public game port, tagged "CoD3SIDE"; the
  receiver takes it off before the game sees it (in recvfrom and in the
  public thread's Drain) and hands it to the side channel's reader. A met PC
  is not also sent it at port 28964 (no doubles). LAN and Tailscale players
  get it as before.
- A met PC now stays met while game packets come from it (before, it was
  dropped 2 minutes after its last punch, and a long match would have lost
  the broadcasts sent to it).
- "public: side channel message ... sent on the game's way" and "... came on
  the game's way" in the log (the first 6 each).

---

## 0.5.436: the log no longer floods in games with bots

- 0.5.435 connected two homes over the internet without Tailscale (System
  Link's own list, Public game on, bots added by the host). The first public
  join to work.
- The host's log of that game was cut short: with bots in, the game reported
  "apsMemory : pool is empty" about 2700 times a second, and the 0.5.176
  error-queue probe (bots.cpp, sub_8210DCF8) wrote two lines for each, so
  64 MB of log filled in 80 s and the join was lost. The game itself carries
  on (50 to 65 fps then).
- That probe now shows the first 40, then one line every 10 s with how many
  more there were and the last one's arguments. The death-queue lines
  before and after a player dies show for the first 40 deaths only.

---

## 0.5.435: public servers: a PC's address is its internet address

- The public join's packets set side by side with a working join (the
  friend's 0.5.416 log, over Tailscale), seen from the joiner: 14 out, 54
  in, 50 out, 10 in, 11 out, 95 in, 32 out, 32 in, the same both times;
  then a working join sends 29 (and gets 281, the game) where the public
  one sent 21 and was let go. The joiner decides differently after
  reading the host's packets; it never calls XNetInAddrToXnAddr, so
  0.5.434's change (on the host) cannot be all of it.
- In every working System Link game the address in a PC's XNADDR is the
  address its packets come from (Tailscale: 100.87.12.49 both). Over the
  internet the XNADDR said 100.87.12.49 and the packets came from
  184.183.32.227. Rather than mend each place the game compares the two,
  the internet case is made the LAN case: with Public game on, this PC's
  System Link address (the XNADDR's first) is its internet address, asked
  of a STUN server at start (a socket of its own; Google's, else
  Cloudflare's; no answer: the address as before). Then a PC's XNADDR and
  its packets agree, the addresses are each home's own (no two alike),
  and only the ports are translated (1000 -> the game port, as before).
- A packet the game sends to this PC's own internet address goes to
  127.0.0.1 (and comes back as from that address): no round trip through
  the router.
- Public game off: nothing changes (LAN, Tailscale). With it on, play over
  the internet; a Tailscale game wants it off on both PCs.

## 0.5.434: public servers: the joiner as the host knows him

- 0.5.433 tested by Justin and his friend. The DLC loading pictures are
  back: chosen from a lobby, the picture first drawn with no map known was
  taken up again ("replaced from maps\mp_guns"). Public join: each PC
  now resolves itself to its own address (100.79.11.48, 100.87.12.49) and
  the handshake ran both ways, but right after the joiner's 21-byte packet
  the host sent a new 29-byte one, and the join ended as before.
- During the join the host turns the address the joiner's packets come
  from into an XNADDR (XNetInAddrToXnAddr, 4 calls), never the other way.
  Over the internet that address is the joiner's public one
  (184.183.32.227), and an XNADDR made from it matched nothing the joiner
  sent about himself (made from 100.87.12.49 on a LAN or Tailscale).
- Now the XNADDR of a PC met through the master server is made from the
  address it gives out (the first LAN address it sent: its lan_address),
  as on a LAN; sending to that address goes to its public one (0.5.428).
  "public: the XNADDR for ... is made from its own address" in the log.

## 0.5.433: the DLC maps' loading pictures back

- 0.5.432 tested (BENCH-PUBLIC): this PC's own XNADDR now resolves to the
  address it gives out (100.79.11.48), not the 169.254 placeholder; listed
  while hosting, off the list at the end; clean run.
- Justin: the photo tour's loading pictures for the DLC maps (0.5.229 to
  0.5.238, textures
eplace\maps\<map>\) no longer show. They are there
  and still taken when a map is known before its loading picture is drawn
  (the benchmark loads its map first: "05F0F9261D8E1859 replaced for
  ordnance"). But chosen from a lobby, the loading picture is drawn while no
  map is known yet ("while the map is \"\""): it was made with no
  replacement, and 0.5.234's re-check covered only a texture already
  replaced for another map, so it stayed the generic picture.
- Now a texture made with no replacement is taken up again once the map
  known has its own copy of it (render_resources.cpp, texture_pack.cpp).
  The map is looked at once a frame, and each texture is checked once a
  map change, not at every look.

## 0.5.432: public servers: each PC its own address

- 0.5.431 tested by Justin and his friend: one lobby each (fixed), and a
  join got further than ever: both logs show the same packets sent and
  received in order (14, 54, 50, 10, 11, 95, 32, 32, 21 bytes, then
  10-byte keep-alives both ways every 2 s), then after ~9 s a 14-byte
  packet from the host and the join was given up. The network was fine;
  the game refused the join.
- Both PCs resolved their OWN XNADDR to 169.254.83.107: with Tailscale
  off, its adapter gets that placeholder on both, and it was the first
  "reachable" address. Two players at one address. Now a PC's own XNADDR
  resolves to the address it gives out (lan_address: 100.79.11.48 and
  100.87.12.49 here), and a 169.254.x.x placeholder is never chosen as a
  reachable address.
- Not yet: the port's side channel (lobby rules, the host's bots, the UAV
  plane, Domination) reaches only the Tailscale peers list, not PCs met
  through the master server.

## 0.5.431: public servers: joining, and one lobby per game

- 0.5.430 tested by Justin and his friend: each saw the other's lobby in
  System Link (the host's answer reached the searcher), but clicking it
  said "joining" for ever, and a refresh showed the one lobby five times.
- Joining: both logs show each PC resolving its OWN XNADDR to the other's
  public address at every join (Justin's 100.79.11.48 / 192.168.0.106 to
  184.183.32.227, his friend's 100.87.12.49 / 192.168.0.89 to
  66.131.169.81): with Tailscale switched off, both PCs' Tailscale
  adapter had the same placeholder 169.254.83.107, which matched. Now an
  XNADDR leading with the address this PC gives out is this PC's, and
  169.254.x.x addresses are neither sent to the master server nor matched.
- Lobbies: every copy of a search was answered and every answer listed.
  Each host's answer now reaches the game once per search socket; the
  search goes once from the game socket and once more 1 s later (was every
  0.5 s for 3 s), and no longer also from the search socket.
- "public: game packet sent to / received from" (the first 8 each way, per
  PC met): a join's handshake in the log.

## 0.5.430: public servers: a host's answer reaches the searcher

- 0.5.429 tested by Justin and his friend: both games went on the list
  (the friend's too, this time), introductions and punches went through
  ("a punch came in from 184.183.32.227:28960: the way is open"), and the
  friend's searches reached the host's port 50000 ("received 14 bytes from
  184.183.32.227:1000"). But the host's game answered each search with a
  111-byte broadcast to 255.255.255.255 (System Link answers by
  broadcast): only the host's own home heard it, and neither saw the
  other's game.
- A broadcast of the game's now also goes to every PC met through the
  master server in the last 2 minutes, at that PC's public port for the
  game's port (as Tailscale peers get it from the peers list): the host's
  answer goes to the searcher's game socket, which hands it to the search
  socket (0.5.429). "public: a broadcast of N bytes ... also to" in the log.
- The netdump now holds the search (14 bytes) and its answer (111 bytes):
  map, mode and players in the list come from that next.

## 0.5.429: public servers: the first two-home test's two faults

- 0.5.428 tested by Justin (host 66.131.169.81) and his friend (184.183.32.227),
  each hosting in turn: neither saw the other's game.
  - Friend hosting: never announced. A closed search socket's handle came
    back from Windows as the hosting search socket, so one socket stood in
    two of our slots; of the two STUN answers it got, one was taken and the
    other went to the game as a packet ("received 32 bytes from
    74.125.250.129" every 2 s), the search port's public address was never
    known and the game never went on the list.
  - Justin hosting: the friend found the game on the list and was
    introduced; the host punched back, but at the search socket of that
    moment, and the game opens a new search socket every 2.5 s (51445,
    51446, 51447, 51449): nothing ever came in on the host's port 50000.
    Both routers keep port numbers (51445 inside is 51445 outside).
- Fixes (kernel_net.cpp):
  - a STUN answer is matched to its question by its id over every slot
    its socket is in; the search socket's slot is emptied when the game
    closes that socket;
  - a searching PC sends its search copies from its game socket (28960,
    which does not change and which the host has punched at), twice a
    second for 3 s after each search; the host punches the joiner's game
    socket from its search port too; the host's answer, arriving on the
    joiner's game socket, is handed to the game's current search socket.
  - closing: once the game is ending, the public thread announces nothing
    more (the first 0.5.429 test took the game off the list, then the
    thread put it back before the process ended).

## 0.5.428: public servers, step 3: joining public games

- 0.5.427 tested (BENCH-PUBLIC.bat): hosting a public game put "HOVGtv's
  game" on the master server's list (game 66.131.169.81:28960, search
  66.131.169.81:50000); clean run, all eight native checks passed. Its
  entry stayed until the 30 s expiry after the game closed (fixed here).
- Joining (multiplayer, kernel_net.cpp), with Public game on:
  - when this PC searches System Link, the master server's list for this
    build is fetched every 3 s; each listed game is asked for an
    introduction (with this PC's public game / search addresses from STUN
    and its LAN addresses) and gets "punch" packets from this PC's game
    socket; the game's own search is copied to each listed game's public
    search port;
  - the host fetches its introductions every 2 s and punches back at each
    joiner from its game and search sockets: once both routers have seen
    the other side, the way is open both ways;
  - the PCs met this way are remembered: an XNADDR whose addresses belong
    to one of them resolves to its public address (matched on the whole
    set of addresses, not one: two homes are often both 192.168.0.x), and
    the game's ports 1000 / 50000 to it are its public ports, both ways;
  - punch packets ("CoD3PNCH") never reach the game.
  The master server keeps the joiner's LAN addresses with an
  introduction (redeployed).
- The hosted game leaves the public list when the game closes (the window,
  the kernel's exit and the benchmarks' end), at most 2 s.
- Untested between two homes: needs two PCs on different internet
  connections, both on this build with Public game on (F11).

## 0.5.427: public servers, step 2: a hosted game goes on the public list

- 0.5.426 tested (city benchmark): clean, all eight native checks passed;
  no "netdump:" lines (the benchmark has no search or join: they come with
  a real System Link game).
- The master server is live: https://cod3-master.cod3port.workers.dev
  (tools/master-server, Justin's Cloudflare account; announce, list,
  punch, punches and remove all tested).
- Multiplayer, kernel_net.cpp: while a System Link game is hosted here (a
  socket bound to port 50000, the search port: only a host listens there)
  and Public game is on, the game goes on the public list:
  - the public address the router gives the game's port 1000 socket (and
    the search port's) is asked of a public STUN server (Google's, else
    Cloudflare's) from that very socket; the answer is taken out of that
    socket before the game reads it ("public: the router shows this PC's
    port 1000 as ...");
  - the game is announced every 10 s (name, max players, build, the two
    public addresses) and taken off the list when the search socket
    closes; joiners' introductions are asked for every 2 s and logged
    (step 3 answers them).
  - the game reads its port 1000 socket only once someone else is in the
    game, so a STUN answer there waited unread (first test: the search
    port's address came back, 66.131.169.81:50000, port 1000's never did,
    and the game was not announced): the public thread now looks at the
    head of both sockets every half second and takes a STUN answer of ours
    (anything else is left for the game; the game's own reads of these
    sockets and this look share a lock).
  - still no answer for port 1000, and a STUN test from this PC with the
    game closed said why: port 1000 has no answer at all on Justin's line,
    while 50000, 3074 and 28960 do (the router keeps the port number).
    Routers and providers often drop ports below 1024. So the game's port
    1000 is now port 28960 underneath: the game still binds, sends to and
    hears from "1000"; the network layer binds 28960, sends to 28960 and
    shows 28960 as 1000. game_port in the ini / COD3_GAMEPORT changes it.
    Every PC in a game needs this build (and the same game port): LAN and
    Tailscale games with 0.5.426 or older will not connect to it.
- Settings (F11, System Link section; CoD3Recomp.ini [game]):
  public_server (off by default), server_name (empty: "<player name>'s
  game"), master_server (the address above). COD3_PUBLIC=1 turns it on for
  tests (BENCH-PUBLIC.bat: the city benchmark hosting a public game).
- Not yet: map, mode and player count in the list (they come from the
  search answer's layout, still to be read), joining (step 3).

## 0.5.426: public servers, step 1: the packets written down

- 0.5.425 tested (city benchmark): the first run crashed at map load after a
  72 s stall at startup (a freed-memory read in the vertical blank's handler
  chain, an older intermittent race also seen in 0.5.422 and 0.5.423, logged
  in docs/CLAUDE-SESSION-2026-10-23.md); the rerun was clean, all eight native
  checks passed. Co-op test 9 (v38, island) host logs sent to Team Friend.
- Public servers (docs/PUBLIC-SERVERS.md): find and join games over the
  internet, as in CoD4 and World at War: a master server on Cloudflare
  Workers (written, tools/master-server, not deployed yet), UDP hole punching
  with port forwarding as the fallback, public games in the game's own System
  Link list.
- This build only writes down what the hole punching will have to carry
  (multiplayer, kernel_net.cpp): "netdump:" lines with the bytes of the first
  6 packets each way on port 50000 (the System Link search and its answer)
  and port 1000 (the game's own traffic), 256 bytes each at most, and the
  whole XNADDR and key when a player's address is resolved.
  COD3_NETDUMP=0: none; =N: N of each. Nothing else changes.

## 0.5.425: co-op v38 from Team Friend merged

- 0.5.424 tested (city benchmark): clean, no heap waits, effects-guard
  hits, access violations or freezes; "rotate native:" 3000 of 3000 and
  the other seven native checks passed. WriteRegisterRunFrom 6.5% of the
  command thread in 0.5.423's busy views, 4.4% now. The camera faced yaw
  -115: view 3 was the heaviest scene measured yet (5740 draws, 16080
  texture switches a frame), 45.6 fps with the command and drawing
  threads each at 20-22 ms (3.4-3.9 us a draw): this design's ceiling.
- Co-op v38 (Team Friend, docs/COOP-V38-NOTES.txt; diff against v36 in
  docs/build-logs/coop-v38-vs-v36.diff), answering co-op test 8.
  CoD3Host/coop.cpp only, the campaign program only (#ifndef COD3_MP):
  multiplayer is not touched. Applied on top of our coop.cpp, which is
  v36 plus 0.5.316's read-only mortar probe (kept).
  - Dropped weapons and grenades are no longer sent or moved by the world
    sync (test 8's spinning MP40s); a dropped weapon picked up or cleared
    in one copy is deleted in the other (new message CWI1).
  - Grenades sent the frame they appear, with their own trajectory, and
    thrown on the same arc in the other copy (CWG3); the landing (CWG2)
    moves it there if it lies 80+ units off.
  - The host's soldiers' shots sent (CWA1, hooking the actor shoot
    sub_823ED9B8) and fired by their partners on the joiner at the
    display delay; the joiner's own soldiers no longer fire on their own.
  - Players' bodies face where that player looks; still soldiers face
    as the host's do.
  - A stuck guard; unpaired host soldiers take one of the joiner's own
    unpaired soldiers (no longer deleted, kept hidden), then spawners;
    the joiner reads the film port every frame; a "no body for the host"
    diagnostic.
  - New messages: both PCs need this CoD3.exe. Tested by Team Friend on
    one PC (two copies, Forest); untested between two PCs.

## 0.5.424: byte order turned round in one shuffle; a rotate, native

- 0.5.423 tested (city benchmark): clean, no heap waits, effects-guard
  hits, access violations or freezes; "table search native:" 3000 of
  3000 and the other six native checks passed. sub_820B64D0's share of
  the main thread: 1.10% in 0.5.422's run, 0.02% now. The camera faced
  yaw 90 as in 0.5.421's run, at about the same draws: the main thread
  12.7 / 12.8 / 13.9 / 16.3 ms a frame against 13.8 / 13.6 / 13.5 /
  15.9; 58.2 / 56.3 / 63.3 / 75.0 fps against 57.6 / 55.6 / 61.7 / 71.1.
  (The benchmark's batch file did not copy its log this run; CoD3MP.log
  was read.)
- 0.5.423 without profilers (BENCH-MARSEILLE-CITY-PLAIN, yaw 0): 81.0 /
  81.2 / 81.7 / 68.5 fps at 2188 / 2159 / 1870 / 3928 draws. In the
  busiest views (5000 draws, yaw 90) the main thread is no longer what
  holds the frame (12.7 ms): the command thread is (15.6-16.7 ms a
  frame), with the drawing thread about as busy (about 16 ms).
- The constant and register runs read from guest memory, and the index
  copies, turned each word round with four shifts and two ors (two
  shifts, an or and two half-word shuffles for the indices'). One SSSE3
  byte shuffle each now (x86-64-v2 has it): Gpu's WriteRegisterRunFrom
  (6.5% of the command thread in 0.5.423's busy views) and
  RenderCommands' Swap16Into / Swap32Into. The same bytes.
- sub_82147008 (0.75% of the main thread; from sub_82147498, a merge)
  rotates a range of 8-byte elements, std::rotate's way round the cycles
  with a division a step for their greatest common divisor. The elements
  are moved whole, never read as values, so the result is the same bytes
  however they are moved: native, the first ones put aside, the rest
  moved down, those after them. Only when the original moves anything
  (the middle strictly inside the range); else the game's own. The first
  3000 calls of a match run the game's own first, then the range is put
  back, rotated natively and compared byte for byte ("rotate native:" in
  the log); one difference and the game's own from then on.
  COD3_NATIVE_ROTATE=0 turns it off.

## 0.5.423: a table searched by its 32-byte key, natively

- 0.5.422 tested (city benchmark): clean, no heap waits, effects-guard
  hits, access violations or freezes; "unpack native:" 3000 of 3000 (27
  stores in the last) and the other five native checks passed.
  sub_82181520's share of the main thread: 1.65% in 0.5.421's run, 0.60%
  now. The camera faced yaw 63.5 (pitch 7), a facing no earlier run had:
  58.4 / 57.8 / 63.6 / 66.1 fps at 4413 / 4450 / 2876 / 3269 draws, the
  main thread 14.2 / 14.7 / 15.4 / 17.1 ms a frame; not compared.
- sub_820B64D0 (1.1% of the main thread in 0.5.422's busy views; called
  from sub_8253C130, sub_8253B130, sub_82542DB8 and sub_8253CC70) looks a
  32-byte key up in a table of 48-byte entries, one by one, each by eight
  word loads byte-swapped and compared. Equal words are equal bytes: each
  entry is now two 16-byte compares. The answer is written as the
  original does, the pair (the table's owner, the index), or none (0 and
  the word already in the stack slot the original builds the pair in).
  The first 3000 calls of a match also run the game's own and compare
  the pair ("table search native:" in the log); one difference and the
  game's own from then on. COD3_NATIVE_TABLESEARCH=0 turns it off.

## 0.5.422: a stream of packed vectors unpacked natively

- 0.5.421 tested (city benchmark): clean, no heap waits, effects-guard
  hits, access violations or freezes; "box sweep native:" 5000 of 5000
  and the other four native checks passed. sub_8248BB48's share of the
  main thread: 1.1% in 0.5.420's run, 0.09% now. The camera faced yaw 90
  as in 0.5.418's run: the main thread's CPU a frame 13.8 / 13.6 / 13.5 /
  15.9 ms against 14.3 / 13.3 / 15.5 / 17.6 (0.5.419-0.5.421's trace
  pieces about 0.4 ms of it, the rest the match); 57.6 / 55.6 / 61.7 /
  71.1 fps at 4920 / 4791 / 3710 / 1873 draws.
- sub_82181520 (1.65% of the main thread in 0.5.421's busy views; called
  from sub_82189380) unpacks a stream of packed vectors: three stream
  positions rounded up, then for each entry four big-endian 16-bit values
  made floats as the recompiled vupkd3d128 does (the bits of 3.0 plus the
  value), scaled and offset by two constant vectors (denormals flushed as
  the vector unit does) and stored at the entry's address from a table.
  The generated code built each 8-byte read out of two partial vector
  loads and did the unpack lane by lane. Native, the same values.
- Checked: the first 3000 calls of a match first work out, writing
  nothing, what they would store; the game's own then runs and every
  store, the three positions and r3 must be the same ("unpack native:" in
  the log); one difference and the game's own from then on.
  COD3_NATIVE_UNPACK=0 turns it off.

## 0.5.421: a moving box against a box's faces, native

- 0.5.420 tested (city benchmark): clean, no heap waits, effects-guard
  hits, access violations or freezes. "box native:" 5000 of 5000; the
  walk 3000 of 3000 (180 calls in the last); the slab test and the piece
  sort 3000 of 3000. sub_82490A30's share of the main thread: 0.86% in
  0.5.419's run, under 0.1% now. The camera faced yaw -180 as in
  0.5.415's run: the main thread's CPU a frame in views 1-3 13.6 / 14.2 /
  14.2 ms against 14.3 / 14.0 / 13.9 (the fighting near the camera moves
  it more than these steps do); 63.4 / 69.4 / 58.2 / 68.4 fps.
- sub_8248BB48 (1.1% of the main thread in 0.5.420's busy views; called
  from the trace's pieces, sub_824972D0 and sub_82497958) sweeps the
  trace's box (its half sizes at +80) along the segment against two sets
  of a box's faces: per face the distances of the two ends less the half
  size, then the enter and exit fractions as in the slab test, 0 as soon
  as they cross. A hit writes the enter fraction (+288) and the face's
  normal (+272); none, +317 and, if no face had the end beyond it, a
  zero fraction and +316. Native, step for step as the recompiled code,
  single precision where it rounds. The first 5000 calls of a match also
  run the game's own from the same trace and compare r3 and the 48 bytes
  it writes ("box sweep native:" in the log); one difference and the
  game's own from then on. COD3_NATIVE_SWEEP=0 turns it off.

## 0.5.420: a segment against a box, native

- 0.5.419 tested (city benchmark): clean, no heap waits, effects-guard
  hits, access violations or freezes. "walk native:" the tree walk
  matched the game's on 3000 of 3000 walks (233 calls in the last);
  both "trace native:" checks 3000 of 3000. The walk's share of the main
  thread: 1.63% (native walk 1.59, the hook 0.04) against 2.31% in
  0.5.418's run. The camera faced yaw 0 this run, lighter scenes (1677
  to 3737 draws): 71.6 / 75.5 / 75.5 / 66.9 fps, not compared.
- sub_82490A30 (0.9% of the main thread in 0.5.419's busy views; called
  from the trace's pieces, sub_82497958 and sub_824972D0) tests a segment
  against a box: first with the vector unit, apart (0) if any of x, y, z
  of the boxes' gap reaches a limit; then the segment's enter and exit
  fractions against the box's faces, z, y, x, the lower corner's and then
  the upper's, 0 as soon as enter is not below exit, else 1. Native, step
  for step as the recompiled code, single precision where it rounds and
  denormals flushed in the vector part as there. It writes nothing, so
  the first 5000 calls of a match simply run both and compare ("box
  native:" in the log); one difference and the game's own from then on.
  COD3_NATIVE_BOX=0 turns it off.

## 0.5.419: the world trace's tree walk, native

- 0.5.418 tested (city benchmark): clean, no heap waits, effects-guard
  hits, access violations or freezes; both "trace native:" checks 3000
  of 3000. The camera faced yaw 90 (pitch 20): 56.2 / 56.6 / 61.4 / 69.2
  fps at 4528 / 4899 / 3753 / 1899 draws, near 0.5.413's 54.5 fps at
  5000+ draws at that facing. No killcam in the benchmark, so the new
  guard had nothing to catch: that wants a two-player game.
- sub_824A0940, the trace's walk down the world's tree (2.3-2.6% of the
  main thread in 0.5.418's busy views, the main thread's busiest after
  the world search), is native. At each node the plane splits the
  segment and one side or both are walked, the nearer first; then the
  node's two lists of pieces each go through the slab test and, where it
  does not miss, a leaf test (sub_82499F58 / sub_8249DDC8, still the
  game's own). The same steps as the recompiled code: the same guest
  stack frames (208 bytes a level) with the midpoints and boxes where the
  original keeps them, single precision where it rounds, the midpoint's
  vector part with denormals flushed as the vector unit does.
- Checked differently from the earlier native pieces, since the leaf
  tests write into the trace and must not run twice: the first 3000
  walks of a match are the game's own, writing down every call they make
  (arguments, results, the trace after). The native walk then runs from
  the same trace against that record: each call it would make must be
  the next one written down, whose result is put back instead of calling
  again; at the end the calls and the trace must be the same. One
  difference and the game's own walk from then on ("walk native:" in the
  log). COD3_NATIVE_WALK=0 turns it off.

## 0.5.418: the killcam freeze at the replay's end

- 0.5.417 tested (city benchmark): clean, no heap waits, effects-guard
  hits, access violations or freezes ("spare slots IN USE" 31 times, as
  before). "trace native:" the slab test and the piece sort each matched
  the game's on 3000 of 3000 calls. Their share of the main thread: the
  slab test 0.7-1.1% and the sort 0.8-1.0% against 1.8% and 1.6% in
  0.5.414's run. The camera looked down this run (pitch 38, 1800-3200
  draws against 2700-4300), so its frame rates (70.2 / 67.0 / 66.2 /
  60.5) are not compared with 0.5.416's.
- 0.5.416 played by two (the host and a friend over the network, 32
  players): froze on the frame a killcam ended. During the replay the
  guard refused the same model instance (5F586190, entity 82A59010
  type 2) a dozen times, as it was meant to. Then "killcam: over (played
  out)", and from the next second nothing was drawn (the watchdog: no
  swap for five seconds). The game's main thread was not waiting on
  anything: it was walking the scene's model list in sub_8243E198 /
  sub_82433B88 for ever, with that same instance in r4 and r31. The guard
  has run only while the replay plays since 0.5.369, so the same add
  came once more on the frame the replay ended, unguarded, and the
  instance followed itself in the list.
- The tail check (the instance is already the list's last one: the add
  that makes the loop) now runs on every add, killcam or not. It is one
  read. The walk of the whole list stays for the replay only, as in
  0.5.369. Adds refused outside the replay are logged with a count of
  their own ("killcam guard: ... killcam not playing"), so the replay's
  dozen messages do not hide them. COD3_SCENETAILGUARD=0: replay only,
  as before.
- Not found: why the game adds that instance twice at all (entity type
  2, around the replay). The guard stops the freeze; the cause is still
  open.
- The friend's log (0.5.416, 32 threads at 3.5 GHz, NVIDIA, over
  Tailscale) froze the same way on its own: the main thread in
  sub_8243E198 for ever with its own doubled instance (5EFD5C70, entity
  82A5A010 type 2, caller 8245C314) right after "killcam: over". Both
  machines need 0.5.418.
- The friend's 14-24 fps (52-105 s, as low as 6 at 96-100 s; 100-190
  either side) was the GPU: the same ~2000 draws, the CPU's part of a
  frame 3-5 ms throughout, but the GPU's 46-58 ms a frame against 7
  (the wait showing first in resolves, 70-95 ms, then in present, 40-48
  ms). Video memory steady (580-610 MB), scale 1.731 all along. It came
  back by itself at 106 s. Not found in the log: what slowed the GPU (a
  smoke cloud lasts about that long, but the log names no throw; or
  something else on that PC using the GPU, or its clocks).

## 0.5.417: two of the world trace's pieces, native; the sampler asked for in front of its table

- 0.5.416 tested (city benchmark): clean, no heap waits, effects-guard
  hits or access violations (the "spare slots IN USE" player-limit
  warning, 32 times, as in 0.5.415's run: 31). BotNadeAmmo and strstr,
  0.7-1.0% each in 0.5.415's profiles, are gone from them. The camera's
  facing changed in view 4 (yaw -57, pitch 27, 3938 draws against 2804),
  so the runs compare in views 1-3 only: 63.7 / 65.4 / 57.4 fps against
  60.9 / 68.0 / 60.3 at about the same draws (3670 / 2748 / 4154 against
  3883 / 2867 / 4311). Views 2 and 3 a little slower: the match (the
  main thread holds the frame there), not shown to be the change.
- Two changes in this one build (written as 0.5.417 and 0.5.418, built
  together).
- The world trace (movement, bullets, the bots' sight lines) is now the
  main thread's busiest work after the world search: two of its pieces,
  1.8% and 1.6% of the main thread in 0.5.414's run, are native.
  sub_824851F8 tests a segment against a box's two sets of faces;
  sub_824850D0 sorts the pieces of a list that match what the trace
  looks for into three lists of up to 128. Both are copied step for step
  from the recompiled code, every value rounded to single precision where
  the original rounds it. Checked off line against a transcription of
  the recompiled code: 6 million segments (1.35 million hits) and 1500
  lists, not one difference, writes to memory included.
- The first 3000 calls of each in a match also run the game's own and
  compare ("trace native:" in the log); one difference and the game's
  own is used from then on. COD3_NATIVE_SLAB=0 / COD3_NATIVE_SORT=0
  turn them off.
- Every texture of every draw looked its sampler up in the samplers'
  table (some 12000 a frame in Marseille's busy views, for a handful of
  different samplers), on the command thread. The handle of each key
  lately asked for is now kept in front of the table (512 entries),
  emptied with it when the filtering setting changes. The second drawing
  thread only replays Direct3D calls, so the front is the command
  thread's alone. COD3_SAMPLERFRONT=0: the table always.

## 0.5.416: the bots' grenade check remembers each weapon's kind

- 0.5.415 tested (city benchmark): clean, no heap waits, effects-guard
  hits, mismatches or access violations; 3000 world-search checks
  matched. The camera faced yaw -180, as in 0.5.412's and 0.5.414's runs,
  so the views compare one for one. Frame rate, 0.5.414 -> 0.5.415:
  view 1 59.3 -> 60.7, view 2 66.4 -> 68.0, view 3 (the busy one, 4155
  draws) 52.2 -> 60.0, view 4 62.2 -> 66.5. The main thread's CPU a
  frame in view 3: 18.7 ms -> 16.0 (0.5.412: 17.8); lower in every view.
  The walk and its push_back were 4.9% of the main thread, now 3.0%.
  Some of view 3's difference is the match (how much fighting near the
  camera), but every view moved the same way.
- Each time a bot's grenades were asked about, all 127 weapons' names
  were read, lower-cased and searched up to nine times for "grenade",
  "smoke" and the like (BotNadeAmmo and strstr together about 1.5% of
  the main thread on Marseille at 32 players). Each weapon's kind is now
  remembered with the name it was worked out from, and worked out again
  only when the name is different (a new map). COD3_NADEKINDCACHE=0:
  worked out every time, as before.

## 0.5.415: the world search's stack without push_back

- 0.5.414 tested (city benchmark): clean, no heap waits, effects-guard
  hits, mismatches or access violations; 3000 world-search checks
  matched. The camera faced yaw -180 all run, as in 0.5.412's, so the
  two compare view by view: the busy view (4206 draws) took the command
  thread 14.2 ms a frame against 15.3 (3.43 us a draw against 3.65, 8%
  less, 0.5.413 and 0.5.414 together); memcpy's share fell from 3.8% to
  2.3%. That view's frame rate went with the main thread, which had more
  to do in this run (18.7 ms of CPU a frame against 17.8: more fighting
  near it): 52.2 fps against 54.9. The main thread holds the frame.
- The game's world search (NativeCull::Walk, the main thread's busiest
  single function on Marseille, 3.3%) kept the boxes still to look at
  in a vector, and its push_back took another 1.1% of the main thread.
  The stack is now a plain array sized once for the deepest it can get
  (the roots and three a level). Checked against the vector on 3000
  random trees: the same pieces in the same order. The game's own
  3000-query comparison at the start of each match still runs.
  COD3_CULL_VECTORSTACK=1: the vector again.

## 0.5.414: the shader constants copied without memcpy calls

- 0.5.413 tested (city benchmark): clean, no heap waits, effects-guard
  hits, mismatches or access violations. The camera faced yaw 90 all run,
  as in 0.5.410's: the same four views, draw counts within 0.5%. Busy
  views (78 samples over 4000 draws): the command thread 16.2 ms a frame
  against 17.0 in 0.5.410 (3.19 us a draw against 3.35, 5% less); view 3
  8% less. The frame rate stayed (54.5 against 54.6): in these views the
  game's main thread (17.4 ms of CPU a frame: the server and bots 6.9,
  the client and its drawing 11.5) is now what holds the frame. 0.5.411
  to 0.5.413 together; 0.5.415 and on are for the main thread.
- Each draw's shader constants are copied into the constant ring, the
  ones each program reads. A run of consecutive constants went as one
  call to the library's memcpy, a short copy of a length known only at
  run time: 3.2% of the command thread in 0.5.410's busy views was
  memcpy called from there. They are now copied a constant (16 bytes) at
  a time in a plain loop, front to back as before. Checked against the
  old copy on 2000 random constant maps: the same bytes, both in the
  ring and in the copy kept of what was sent. COD3_CONSTLIBCOPY=1: the
  memcpy calls again.

## 0.5.413: the drawing tables' entries kept together

- 0.5.412 tested (city benchmark): clean, no heap waits, effects-guard
  hits, mismatches or access violations; the 3000 world-search checks
  matched. The profiler's D3D11CoreRegisterLayers (the GetDesc calls,
  named after the nearest export) went from 0.8-0.9% of the command
  thread to nothing. The camera faced yaw -180 the whole run, other
  scenes again, so its frame rates are not compared with 0.5.411's.
- The test ran with COD3_BENCH_CLIENTTURN (BENCH-MARSEILLE-CITY-TURN.bat):
  the local client's view yaw climbed to 5760 while the player kept
  facing -180. In the bot test neither setviewpos nor the client's
  angles turn him; runs are compared by draw counts and the profiler.
- The textures', vertex buffers' and index buffers' entries were kept
  in std::deques. Microsoft's library makes a deque of anything over
  16 bytes one entry a block, so each look up by handle (some 25000 a
  frame in Marseille's busy views, on the command thread) went through
  the deque's map to an entry of its own somewhere on the heap. They
  are now kept in blocks of 256 that never move: one small table of
  blocks, then the entry, with its neighbours beside it. As with the
  deque, an entry stays where it is while more are added. Nothing
  else changes.

## 0.5.412: the texture view check asks once a view

- 0.5.411 tested (city benchmark): clean, no heap waits, effects-guard
  hits, mismatches or access violations. The camera faced about where
  0.5.408's run did (pitch 18-21, yaw 157-164): between 4000 and 4500
  draws, 3.43 us a draw at 54.2 fps (42 samples) against 0.5.408's
  3.65 us at 53.2 fps (39 samples), some 6% less a draw. Not the very
  same scenes, so a good sign rather than proof. The texture lookup's
  share of the packet loop's time fell from 7.8% to 6.2% (profiler).
- Every texture of every draw had its view asked for its format
  (GetDesc, a call into Direct3D) by a check that only writes to the
  log when a view is of the wrong kind: some 12000 calls a frame in
  Marseille's busy views, on the command thread. Each view is now asked
  once and noted (the texture pack's replacements, always of another
  kind, included). Nothing drawn
  changes; the most a reused view could cost is one log line.
  COD3_VIEWCHECKALL=1 asks on every draw, as before.

## 0.5.411: the texture lookup's front cache

- 0.5.410 tested (Marseille city benchmark, 32 players): clean, with
  no heap waits, effects-guard hits, mismatches or access violations.
  Every view faced yaw 90, heavier than earlier runs: 72 samples between
  5000 and 5500 draws, median 54.75 fps, command thread 17.0 ms a frame,
  3.35 us a draw. That is the baseline for 0.5.411.
- 0.5.410's camera hold did not work: setviewpos was sent again every
  second, yet every view faced yaw 90 the whole run. Here setviewpos
  places the player but does not turn him. The facing stays one way for
  a run (it differs between runs), so runs are compared at the same draw
  counts. The resend is now off (COD3_BENCH_HOLDTURN=1 turns it on); the
  "bench: hold check" line stays, saying which way each view faced.
- Every texture of every draw asked two tables in turn: is this a
  surface a resolve made, and which texture is it. That is two hash
  probes per texture, some 24000 a frame in Marseille's busy views, on
  the command thread that limits those views. A small cache now sits in
  front of them. A hit needs the same address, format, size and kind of
  texture as before, no resolve made, remade or let go since, and the
  texture still in use under that same key; then it is exactly the
  answer the two tables would give. The checks for changed texels run
  as before. COD3_TEXTURELOOKUPCACHE=0 turns the cache off.

## 0.5.410: the benchmark camera holds its facing

- 0.5.409 tested (Rimling, 32 players): clean. All 3000 world-search
  checks matched; no heap waits, effects-guard hits, mismatches or access
  violations; median 55.8 fps at about 3760 draws.
- The Marseille benchmark moved the camera to the right places but did
  not keep it facing the way it was told. 0.5.408's city run asked for
  pitch 0 yaw 0 at every view; views 2-4 faced pitch 20.7 yaw 156.6.
  0.5.407's run faced -180 three times and -114.3 once. So per-view
  numbers from different runs were not the same scenes.
- The facing is now checked twice a second. When it is more than 1.5
  degrees off, setviewpos is sent again (at most once a second), logged
  "bench: turned back". Each view also logs "bench: hold check" near its
  end with where he faces. COD3_BENCH_HOLDTURN=0 restores once-a-view.
- Benchmark only; nothing changes when playing.

## 0.5.409: Restore the bounded world-search self-check scratch

- mp_fixes.cpp sub_824193E0 still placed its self-check object 24 KB below
  the guest stack, despite the 0.5.385 note saying this was fixed. Restore
  placement 128 bytes below the original walk's 2304-byte frame. The walk's
  only callback is intercepted before any guest-stack use; the 32-byte object
  fits within the Xbox 360 ABI red zone below that frame.
- COD3_CULL_NEARSTACK=0 restores the old placement for comparison. The native
  culling algorithm and its 3000-query comparison remain unchanged.
- This removes a possible small-stack corruption source; it is not a proven
  cause of heap exhaustion or the effects crash. Built OK; Rimling clean
  (3000/3000 checks matched).

## 0.5.408: Identify the heap request that cannot make progress

- The cache has two allocation loops. Its old wait report always used the
  entry's largest request, even if a smaller later level was failing.
  mp_fixes.cpp HeapWait now reports the current loop's exact units, stride,
  header, 64-bit size/overflow, allocator and eight game-heap capacity/usage
  records on the first and hundredth empty reclaim (12 reports maximum).
- Diagnostic reads cannot demand-commit guest memory. Normal successful
  allocation behavior and the existing 1 ms retry sleep are unchanged.
  COD3_HEAPWAIT_AUDIT=0 disables the additional diagnostic reports.
- Built OK; automated Rimling 32-player test completed with no heap
  waits, effects-guard hits, native mismatch or access violation. The
  freeze did not reproduce, so the new diagnostic path was not exercised.
  This is evidence collection, not a fix for the freeze.

## 0.5.407: a watch on a heap freeze

- 0.5.406's Rimling run froze 30 s into the match (a first in all of
  today's runs): the main thread in sub_82185D18, a cache that asks the
  game's heap (sub_820DE188, through sub_820DE7B8) for a block and, when
  refused, lets go of its own oldest entry (sub_82185B40) and asks again,
  for ever. Its own list was already empty, so nothing was let go and the
  heap stayed full: the game spun at full speed with no frame drawn.
- The first time this happens the request and the cache are now logged
  ("heapwait:"), and the loop sleeps a millisecond a turn from then on, so
  the threads that hold the heap's memory get to give it back (and the
  game does not burn a core while it waits). Why the heap is full is the
  next question; the log line says which cache and how much it wanted.

## 0.5.406: the depth copies four texels at a time

- 0.5.405: clean. The drawing thread used from 3000 draws instead of
  4000 (A/B in one run): lighter command thread at 3700 draws but 56.1
  against 58.1 fps, so it stays at 4000. Rimling at 32 players, against
  0.5.369's run: median 65 fps (was 45), slowest tenth 56 (was 41), the
  server frame 6.1 ms (was 9.1). No crash.
- On Rimling the command thread's biggest piece of its own work is the
  depth copies written back into the title's memory (ConvertDepthRows,
  4.9% there, and all the helper threads with it). The float depth
  (20e4) is now encoded four texels at a time; any four with a texel at
  or below zero or below 20e4's normal range go one by one as before.

## 0.5.405: the written-page check in one read

- 0.5.404 measured: no change from the box tree walk's early reads (the
  same views as 0.5.402, within a fraction of a frame), and no crash. Run
  with no profiling at all (BENCH-MARSEILLE-CITY-PLAIN.bat, the frame rate
  as played): 71.5 / 70.4 / 75.8 / 65.5 fps on the views 0.5.403 measured
  at 68.8 / 69.1 / 68.4 / 62.7 with the profilers on; the main thread
  13.4 ms a frame.
- The game is still built for any x86-64 processor with SSE4.2 (2008 on):
  Justin's friends may have older PCs, so no AVX2 build.
- Whether a buffer's or texture's pages were written since it was last
  looked at (WriteWatch::Clean, several times a draw on the command
  thread) took two reads a page from two tables; it is now one word a page.

## 0.5.404: the box tree walk asks for its nodes early

- 0.5.403's run: clean (no crash, no bad call, the effects guard never
  needed). The same four views: 68.8 / 69.1 / 68.4 / 62.7 fps, the main
  thread 14.2 ms a frame.
- The box tree walk (NativeCull::Walk, still the main thread's busiest
  function at 3-5%) waits on memory for each node it pops; it now asks
  for a node's four children (64 bytes) as soon as it pushes them.

## 0.5.403: a guard for the effects crash

- The job worker's bad call that 0.5.397 and 0.5.398 crashed on is not
  the fiber pool's: Codex's 0.5.384 "index off" crash, before the pool
  existed, is the same chain (sub_824472C0 -> sub_820FC850, an effect's
  emitters), and it is the long unexplained "effects crash". 0.5.402 ran
  clean (no crash, no bad call); the pool stays, with its stacks cleared.
- Where the bad pointer comes from is not known yet. Until it is, the
  effect update checks its emitters first: an emitter that is not a heap
  address is left out, and an emitter's attached object (+200) that is
  not a plausible object (an aligned heap address whose first word is a
  vtable in the image) is treated as none, before the update calls
  through it. The first dozen are logged ("fxguard:") with what surrounds
  them, to find the cause. COD3_FXGUARD=0: off.

## 0.5.402: a fiber used again starts as a fresh one

- 0.5.401 (calls through pointers inline, constant words four at a time),
  the same four views again: 73.4 / 67.1 / 69.6 / 61.4 fps, the main
  thread 14.0 ms a frame. No crash, but the job worker's bad call that
  0.5.397 and 0.5.398 crashed on came back twice (a vtable read where an
  object was expected), so the fiber pool still differs from fresh fibers
  in some way the title notices.
- The one difference left: a new fiber's stack is all zeros, a used one
  holds the last script thread's frames. Something reading stack memory it
  never wrote reads zero on a fresh stack and stale values on a used one.
  The used part of a pooled fiber's stack is now cleared before it runs the
  next script thread. COD3_FIBERCLEAR=0: not cleared (to compare);
  COD3_FIBERPOOL=0: no pool at all.

## 0.5.401: calls through pointers inline, constant words four at a time

- 0.5.400 (native memcpy), the same four views as 0.5.397's clean run:
  69.2 / 67.5 / 68.0 / 62.5 fps against 64.3 / 64.0 / 66.5 / 59.9, the
  main thread 14.2 ms a frame against 14.9 (15.8 with neither the fiber
  pool nor the memcpy). No crash, none of the bad calls.
- Every call the title makes through a function pointer (virtual calls,
  10865 places in its code) went out of line through Mmio::CallIndirect
  and its trace checks: 1.7% of the main thread. A call to a function of
  the title's that is there now goes straight to it, inline; anything
  else (a level's DLL, a bad pointer, or any of the call traces on) still
  goes through CallIndirect as before.
- The command thread's constant packets and register runs (its busiest
  lines, about 7% together) are turned round and stored four words at a
  time with SSE2 instead of one at a time.

## 0.5.400: the title's memcpy native

- 0.5.399 ran the city benchmark twice with no crash and none of the bad
  calls that led up to 0.5.397's and 0.5.398's (the fiber pool's fix
  holds). Heavy views (5000-5700 drawn) 47-54 fps, held by the command
  thread (17-19 ms); lighter ones 56-69.
- The profile by source line: the main thread's 3% "sub_8231E8C0" is the
  title's own memcpy, used by every thread. It is now the host's (same
  bytes, same order; a copy whose destination starts inside its source,
  or one near the device registers, is still the title's own).
  COD3_NATIVE_MEMCPY=0: the title's own always.

## 0.5.399: the fiber pool's crash found

- 0.5.398's runs: with the fiber pool, two of three benchmark runs since
  0.5.397 crashed near the end in a job worker (a call through a bad
  pointer, sub_824472C0 -> sub_820FC850 -> sub_82110FD0), never seen in
  any run before; with the pool off (COD3_FIBERPOOL=0) the run was clean
  and that bad call never happened at all. Same views as 0.5.397's clean
  run: pool off 61.2 / 61.7 / 61.0 / 56.5 fps, main thread 15.8 ms;
  pool on 64.3 / 64.0 / 66.5 / 59.9, 14.9 ms.
- The cause: each fiber has its own SSE control word (rounding, flush to
  zero), but the title's code keeps one copy of it in the shared context
  and only sets the processor's when that copy says it changed. A fiber
  used again started with whatever its last script thread left, so a
  script thread could run with the wrong rounding or flush mode and
  compute wrong numbers (indices, handles) that a job later used.
- The control word now goes with every switch into and out of a script
  thread's fiber, both ways.

## 0.5.398: the main thread measured, the box tree walk quicker

- 0.5.397 measured: the same four views as 0.5.396 (same draw counts) at
  64.3 / 64.0 / 66.5 / 59.9 fps against 62.9 / 63.7 / 63.8 / 57.5, the
  main thread 14.9 ms a frame against 15.4: the fiber pool took the
  NtFreeVirtualMemory share off it entirely. One of 0.5.397's two runs
  crashed after 130 s on a call through a bad pointer in a job worker
  (never seen before); the second ran clean. Watched.
- The "perf: a game frame" line also gives the main thread's CPU time a
  frame, so its waiting can be told from its work.
- The box tree walk (NativeCull, 5.6% of the main thread) tests a node's
  six bounds with one SSE2 compare instead of six reads and compares
  (still checked against the game's own walk for its first 3000 queries).

## 0.5.397: script thread fibers kept and used again

- 0.5.396's profiler, now able to say who called: the main thread's 3.7%
  in NtFreeVirtualMemory (and 0.6% in ZwAllocateVirtualMemory) is
  Coroutines::Destroy and Begin: every one of the level's script threads
  got a new host fiber (a 4 MB stack reserved from Windows) and handed it
  back when it ended, many times a frame with 32 players.
- A fiber whose script thread ran to its end now goes into a pool (up to
  64) and runs the next script thread; one whose thread is still inside
  a yield is deleted as before. COD3_FIBERPOOL=0: the old way. The
  COD3_TRACECOROUTINES line says how many fibers were made and reused.

## 0.5.396: the profiler says who called

- With the drawing thread done, the game's main thread is the other half
  of Marseille's limit (about 19 ms a frame in heavy views). Its sampled
  profile in those views has 4.3% in NtFreeVirtualMemory (memory handed
  back to Windows every frame) and 0.7% in ZwAllocateVirtualMemory, but
  could not say from where: since 0.5.371 it records only the function a
  sample fell in, because walking a running thread's stack froze the game.
- The profiler now walks the stack while the thread is stopped, by the
  modules' own unwind tables read once (no system lock the stopped thread
  could hold) and never outside the stopped thread's stack, and names, for
  a system function, the first caller back in the game's executable.
  COD3_HOSTPROFILE_SELF=1 is the old "where it was" only, =0 the old walk.
- BENCH-MARSEILLE-CITY.bat: the benchmark from four points high over the
  middle of the city (3000-5200 drawn whatever way the camera faces).

## 0.5.395: the drawing thread only for heavy frames

- Across the A/B runs of 0.5.392-0.5.394 the drawing thread made the
  command thread lighter (7-13% a draw at 4000-5200 drawn, less in
  lighter views), but in views where the game's main thread held the
  frame rate the frame rate was often a little lower with it on (about
  1.5 fps on average): its core is one the game's own threads could use.
- It is now used only in heavy frames, where the drawing is what holds
  the frame: from 4000 draws a frame, off again under 3000
  (COD3_DRAWTHREAD_MIN=draws; 0 = every frame; COD3_DRAWTHREAD=0 = never).
  The A/B benchmark compares that against off.

## 0.5.394: two small costs off the command thread

- 0.5.393's A/B: in those views (2400-4300 drawn) the game's main
  thread, at about 17 ms a frame, held the frame rate as much as the
  drawing; the command thread was 2-4% lighter with the drawing thread.
  Its sampled profile showed two costs that are not drawing work: the
  system clock read twice a draw for the counters (2.7%), and system
  calls waking the drawing thread when it had fallen asleep between
  batches (about 2-4%).
- The counters' timing now uses the processor's time stamp counter
  (measured against the clock once a frame), and the drawing thread
  waits about a third of a millisecond, by that counter, before it
  sleeps, whatever a pause instruction costs on the processor.

## 0.5.393: a cheaper hand over to the drawing thread

- 0.5.392's A/B over the middle of Marseille (4000-5200 things drawn):
  with the drawing thread 51.4 / 53.7 / 50.6 fps, without 47.4 / 53.6 /
  54.7, the command thread 7-13% lighter a draw. The drawing thread is
  busy only about 30% of the time and keeps a draw or two behind, yet the
  command thread saved less than half of what it handed over: writing
  the queue cost it nearly as much as the calls it gave away.
- The queue is now 4 MB instead of 64 MB, so it stays in the
  processor's cache, and it is handed over every 8 draws instead of
  every draw (or at once when anything has to wait for it).
- A ring write too big for the queue (over 512 KB) is mapped on the
  command thread after the drawing thread catches up.

## 0.5.392: measuring the drawing thread

- 0.5.391 ran the whole Marseille benchmark with no crash and no
  graphical fault, but the command thread's time per draw did not drop
  (3.38 us a draw against 3.31 before). This build measures why: a
  "queue:" line every second (with the render profile) says how often
  and how long the command thread waited for the drawing thread, from
  which place, and how busy the drawing thread really was.
- COD3_DRAWTHREAD_TOGGLE=seconds turns the drawing thread on and off in
  turns in one run (logged as the benchmark's settings A and B), and
  BENCH-MARSEILLE-AB-DRAWTHREAD.bat runs the benchmark that way.

## 0.5.391: a second drawing thread

- The heaviest views on Marseille (6000+ things drawn a frame) were held
  to about 40 fps by one thread doing everything for the graphics card:
  reading the game's draw commands, working each draw out, and making the
  Direct3D calls. The Direct3D calls (binding, drawing, filling the
  constant and index buffers, small buffer uploads) now run on a thread
  of their own, fed by a queue, while the first thread works out the next
  draw. Everything the queue carries is copied from the game's memory
  when it is queued, so the game sees its draws finish in the same order
  as before.
- Anything else that touches the graphics card (resolves, the present,
  texture uploads, a cache dropping something) first waits for the
  drawing thread to finish what it has, then runs as before.
- COD3_DRAWTHREAD=0 turns it off. With the render profile on, the log has
  a "draw-thread:" line beside "cp-thread:", and the render report says
  how many draws were queued and how often the command thread waited.

## 0.5.390: the benchmark compares settings in place

- 0.5.389's benchmark ran (Claude started it through the auto-builder,
  which can now start a benchmark batch file on request). The camera
  goes to the four places but keeps the spawn's direction (the game
  ignores setviewpos), so each run sees different views. Across all
  runs the drawing thread takes about 23 ms a frame at 6000 things
  drawn (16.6 is 60 fps); the constant copy saved about 2% of that.
- The benchmark can now switch a setting back and forth every 6 seconds
  at the same place (COD3_BENCH_TOGGLE="A|B"), so a setting is compared
  on exactly the same view. BENCH-MARSEILLE-AB-DETAIL.bat compares model
  detail Maximum with the game's own; BENCH-MARSEILLE-AB-FOV.bat compares
  field of view 90 with the game's 65.

## 0.5.389: the benchmark camera turns (second try)

- 0.5.388 still faced one way: in the bot test the host's view comes
  from the bots' own commands, not the client's angles. Each view is
  now set with the game's own "setviewpos" console command, which turns
  the player on the server's side as a spawn does.

## 0.5.388: the benchmark camera turns

- 0.5.387's pictures showed the camera at the right places but always
  facing the same way, and one of the high views out past the edge of
  the map. The turn is now corrected a few times a second instead of
  every frame (each correction was added again before the last had
  reached the server), and that view is replaced by another high one
  over the city.

## 0.5.387: the benchmark takes a picture of each view

- The first benchmark run (0.5.386) finished cleanly but drew far fewer
  things than the same places did in the photo tour, so the camera may
  not be where it should be. The benchmark now saves a picture of each
  view (screenshots folder) and logs where the player really is.

## 0.5.386: A fixed Marseille benchmark; one copy less a draw

- Tested 0.5.385 (Claude ran it): the bot match finished cleanly, the
  native routines matched the game's again, and the effects watch found
  nothing odd. That match never reached the busy views (the camera stays
  where you spawn), which is why a fixed benchmark was needed.
- New BENCH-MARSEILLE.bat: the same 32-player bot match, but the camera
  is held at four fixed views of Marseille, 35 seconds each: two from
  high over the city (the heaviest scenes, 6000-7000 things drawn) and
  two down long streets at eye height. Every build is measured on the
  same scenes. Log: bench-ordnance.log.
- The drawing thread copied every draw's shader settings twice; the
  second copy is only needed by a mode that is off (the native command
  lists), so it is only made when that mode is in use.
  COD3_ALWAYSSENTFILE=1 makes the copy always, as before.

## 0.5.385: Watching for the effects crash

- One test of 0.5.384 (Codex's, with the index lookup cache turned off)
  crashed about 90 seconds into the match: an effect asked for 1.4 GB
  for its particles, the effect pools ran dry, and the game called into
  garbage. It hasn't happened in the other runs and its cause is unknown.
- Now every particle group that asks for an impossible amount (over
  16 MB) or has an unreadable setting is written to the log with the
  numbers it was given and who asked, so the next time it happens it
  tells us which effect and from where. The game does exactly what it
  did before. COD3_FXGROUPWATCH=0 turns the watching off.
- The native world search's self-check (first 3000 searches) now puts
  its stand-in object just under the game's own working space instead
  of 24 KB down the stack, which a thread with a small stack might not
  have.

## 0.5.384: Measure the whole command processor

- Profiling-only measurement in render_stats.cpp EndFrame: once per
  reporting second, read the command thread's kernel+user CPU time using
  GetThreadTimes. New cp-thread lines include packet processing and driver
  CPU work and exclude sleeping/descheduling. There are no new per-draw
  clock calls. COD3_THREADCPUPROFILE=0 disables the measurement.
- This corrects a gap in the existing render CPU figure, which only times
  Draw/Resolve/Present. Those are elapsed scope times; do not subtract
  them from thread CPU time or call either one the other's remainder.
- Justin tested: busiest-view command-thread median 21.88 ms CPU per
  frame at 5534 draws, using 95.6% of one core. It is saturated there.
  This is the first full-thread baseline; earlier render CPU numbers
  excluded packet processing. All native self-checks matched.

- Follow-up stability: same-build index-cache-off comparison crashed in
  particle effects; a final pre380-behavior control completed with no
  allocation errors/access violation. Cause unresolved; do not call
  stability established. Final evidence: CODEX-HANDOFF-0.5.384.md.

## 0.5.383: Faster lookup of repeated index buffers

- render_resources.cpp IndexBufferFor: a small front cache remembers
  recently used index-buffer handles, avoiding the full key-table hash
  and probe on hits. The complete live key (address/count/width/reset) is
  checked; page-watch and content checks still execute as before.
- COD3_INDEXLOOKUPCACHE=0 restores the previous lookup path. This targets
  cache lookup overhead; it does not skip index-content validation.
- Justin tested: overall median 56.8 fps, busy views 47.6 fps. Matched
  5000-5499-draw views stayed about 46.4 fps (previously 46.3); no clear
  speedup there. Sampled index lookup self share fell 3.45% to 2.9%.
  Different matches; all native checks passed. Same-build off comparison
  still desirable before calling this a confirmed gain.

## 0.5.382: Less packet history around every draw

- gpu.cpp ExecuteBuffer: skip the recent-packet diagnostic copy for type-3
  draws, inline constant writes and NOPs. Keep synchronization, fences,
  interrupts, indirect calls and other type-3 packets in the history.
  Command execution and payload reads used by command handlers are unchanged.
- COD3_REMEMBERDRAWS=1 restores the 0.5.380/381 history; existing
  COD3_REMEMBERALLPACKETS=1 restores the entire pre-380 history.
- Justin tested: busy-view drawing CPU fell from 3.224 to 3.132
  microseconds per draw. Busy-view fps was 46.0 vs 48.7, but median
  busy draw count rose 4691 to 5284. In 5000-5499-draw views fps stayed
  46.3, while CPU per draw fell 3.190 to 3.087 microseconds. Different
  matches; native self-checks passed.

## 0.5.381: Watch the remaining small models

- The 0.5.380 fingerprint audit found index buffers were only a few MB
  per five seconds, while vertex buffers were hundreds of MB. Many of
  the repeatedly checked models were just below the 4 KB watch cutoff
  (roughly 3 to 4 KB).
- render_resources.cpp VertexBufferFor: lower the default stable-model
  page-watch threshold from 4 KB to 1 KB. Existing safeguards for buffers
  that change within a frame or share frequently written pages remain.
  COD3_WATCHSTILL_KB=4 restores the 0.5.380 threshold.
- Justin tested: median hashing fell from about 4.1 MB to 0.94 MB per
  frame, but busy-view performance was essentially unchanged (49.3 to
  48.7 fps, 3.218 to 3.224 microseconds per draw). Overall render median
  53.25 to 54.15 fps. No clear busy-view speedup; native checks passed.

## 0.5.380: Less diagnostic work in the drawing thread

- gpu.cpp ExecuteBuffer: register (type 0) and padding (type 2) packets no
  longer copy their payload into the recent-packet diagnostic ring. Type 3
  packets still keep their history. Packet decoding, register writes, waits,
  fences and draw execution are unchanged. COD3_REMEMBERALLPACKETS=1 restores
  the complete old diagnostic history for comparison or troubleshooting.
- Justin tested: views over 4000 draws rose from 45.6 to 49.3 fps;
  drawing-thread CPU time per draw fell from 3.382 to 3.218 microseconds.
  Different bot matches; overall median stayed 53.25 fps. All native
  self-checks passed. Baseline log was 0.5.378; visual feedback not yet given.

## 0.5.379: Marseille's train tracks drawn over things

- Tested 0.5.378 (Justin ran it): the busiest views on Marseille went
  from 39 to 45 fps (each thing drawn 4.04 -> 3.39 microseconds on the
  drawing thread), the slowest moments the best yet (43), no crashes.
- Fixed: the rails and sleepers on Marseille drew over the crates, posts
  and your own gun when you were close to them, lying down or under a
  wagon (Justin's video; turning Steady decals off cured it). Steady
  decals pulls the layers laid over a surface (tracks, marks, bullet
  holes) a hair towards you so they stop flickering; part of that pull
  grows with how edge-on the surface is, and with no limit it grew past
  whole objects. It is now limited to a tiny amount. Steady decals can
  be left on. COD3_DECALNUDGECLAMP=0 removes the limit.

## 0.5.378: Marseille's busy views, part 2

- Tested 0.5.377 (Justin ran it): the drawing thread now re-reads 3.6 MB
  a frame instead of 10, no crashes, but the frame rate was about the
  same (55 average this run, 58 last; the bots don't play the same match
  twice). The log shows why: the frame rate follows the number of things
  drawn. Up to about 3000 it holds 55 to 61; the views with 4500 to 5400
  fall to 38 to 43, because the drawing thread spends about 3.6
  microseconds on each one.
- One of those costs: the game keeps its moving geometry (marks, effects)
  in one 12 MB pool, and the frame's first draw compared all 12 MB with
  the copy on the graphics card to find the few pieces that changed. The
  memory already says which pages were written, so only those pieces are
  compared now. COD3_BIGCOMPAREALL=1 compares it all as before.

## 0.5.377: Marseille's busy views

- Tested 0.5.376 (Justin ran it, Marseille, 32 players): 59 fps average
  (43 this morning), slowest moments 41, no crashes; the three native
  routines matched the game's own on every check (5000 blends, 3000
  searches, 2000 inverses). Most of the match held 57 to 65; the dips to
  27 to 50 came in the views with the most on screen.
- In those views the drawing thread was re-reading 7500 small models
  (the soldiers' and the props', 23 to 32 KB each) every frame, 300 to
  400 MB a second, to see whether they had changed, which they hadn't.
  Models of 32 KB and up were already watched instead (the memory itself
  says when it's written); now everything from 4 KB up is.
  COD3_WATCHSTILL_KB=32 puts the old size back.

## 0.5.376: the soldiers' animation blend

- The blend between two bone rotations, done for every bone of every
  soldier every frame, now runs as native code (3.7% of the main thread
  on Marseille at 32 players). Same arithmetic as the game's; the first
  5000 blends also run the game's own and must match it bit for bit, or
  the game's own is used from then on. COD3_NATIVE_SLERP=0 turns it off.

## 0.5.375: Marseille, back to the main thread

- Tested 0.5.374 (Justin ran it, Marseille, 32 players): 50 fps on
  average (was 42), slowest moments 39 (was 36), no crashes. The drawing
  thread now waits a third of the time, so the game's main thread is the
  limit again, at 19.4 ms a frame (16.6 is 60 fps).
- The game's search of the world for what is near a box (what to draw
  and what to test against) now runs as native code. It was the biggest
  single thing on the main thread, 6.5%. For its first 3000 searches the
  game's own search runs alongside and the two lists of what they found
  are compared; if they ever differ, the log says so and the game's own
  is used from then on. COD3_NATIVE_CULL=0 turns it off.

## 0.5.374: Marseille, the drawing thread, part 2

- Two small vertex buffers the game rewrites during every frame (effects
  and tank treads) were checked byte by byte, all 128 KB of each, by
  every draw that used them: most of the 16 MB the drawing thread checked
  each frame on Marseille. Now a copy of what the graphics card has is
  kept, and each draw compares only the vertices it actually uses,
  sending just the parts that changed. A draw whose vertices can't be
  worked out compares the whole buffer, as safe as before.
  COD3_MIDFRAMEHASH=1 puts the old way back.

## 0.5.373: Marseille, the drawing thread

- Tested 0.5.372 on Marseille at 32 players: the new matrix inverse gave
  the same answer as the game's own on all 2000 checks, and the main
  thread got about 2 ms a frame quicker. The slowest moments went from
  28 to 36 fps; the average stayed at 42 because the drawing thread is
  now what holds it back.
- The drawing thread's most common job, writing a draw's settings into
  the graphics registers, now goes through the quick path the shader
  constants already used, instead of one register at a time with every
  debugging check. That was about a tenth of the drawing thread.
  COD3_SLOWREGISTERS=1 puts the old way back.

## 0.5.372: Marseille, first cut at the main thread

- The 0.5.371 profile of Marseille at 32 players worked (no freeze). It
  showed the graphics card isn't the limit: the game's main thread is,
  at about 23 ms a frame (server 8 ms, the rest 14 ms), with the drawing
  thread close behind it.
- The game's 4x4 matrix inverse now runs as native code. It took 5% of
  the main thread, used 24 places in the server's code, and did every
  number through memory. Same steps and the same rounding as before; the
  first 2000 uses also run the old one and the log says if they ever
  differ. COD3_NATIVE_INVERSE=0 turns it off.

## 0.5.371: the profiler made safe, ready for Marseille

- The profiler froze the game twice. It walked back through a running
  thread's calls to see who called what; now by default it only notes
  where the thread is at each sample, which is what the fixes need and
  can't upset the game (COD3_HOSTPROFILE_SELF=0 for the old way).
- BIG-PROFILE-32.bat now takes a map and profiles both the game's main
  thread and the drawing thread; with no map it does Marseille
  (ordnance), the map that drops lowest.
- No change to play.

## 0.5.370: frame rate, step 2: two big wastes on the main thread removed

- I ran the profiler on your PC during a 32-player match (Rimling, 28
  bots, BIG-PROFILE-32.bat in the game folder). Two things that have
  nothing to do with playing the game were taking almost a fifth of the
  game's main thread:
  - About 11%: reading settings. Several of our checks looked up their
    on/off switch in the environment every time they ran, and one of them
    (the 32-player roster) runs for every player, every frame. Each lookup
    locks and searches the whole environment. Now each setting is read
    once and remembered.
  - About 7%: writing the log. Every line went straight out to the log
    file the moment it was printed, about 300 lines a second with bots.
    The log is now collected and written out every tenth of a second
    instead. If the game crashes, what was waiting is still written first.
- Measured (Rimling, 32-player test, 28 bots): 43 fps before, 53 fps now
  (median; the slowest moments 39 -> 49). That's faster than before all
  the recent updates (47 on October 17). Bots now take about 1 ms a frame
  (was 2.6), the match about 7 ms (was 9.8).
- COD3_LOG_UNBUFFERED=1 puts the log back to line-by-line, if ever needed.
- The profiler itself froze the game after about 95 s of play (the game
  stopped; the log kept going). Known from before (0.5.236); it gave 20
  good reports first. Not a problem in normal play (it's off).

## 0.5.369: frame rate, step 1: measure, and cut what we added that runs all the time

- You (01:06): frames have dropped by about half on every map since we
  added so much, especially with 32 players.
- What the logs say, same map (Merville) and the same 28-31 bots:
  0.5.247 (Oct 17) ran at a median 41 fps, the 0.5.367 log at 39. So the
  recent updates cost a few frames there, not half. What halves the frame
  rate is the 32 players themselves: each frame the game runs the match
  (about 9-10 ms with 32 players), the bots (about 2.5 ms) and then draws
  (about 12 ms), one after the other: about 25 ms, so about 40 fps.
  With fewer players, all three are smaller.
- Cut now: two safety checks added for the killcam (on the game's list
  of things to draw) ran on every frame, walking that whole list each
  time something was added to it. They only ever caught anything during
  the killcam, so now they only run then.
- Measured now: the killcam's recording, which runs all the time
  ("perf: killcam recorder ... ms" in the log every 10 s).
- Next steps: see where the match's 9-10 ms and the drawing's 12 ms go
  (fix the profiler run that cut the log off last time), then cut there.

## 0.5.368: the soldier-pose watch, sharpened

- 0.5.367 tested (you played a while, saw no odd poses). Its log: 9
  killcams, all played out, guns put back in hands every time, and the game
  never rebuilt a body mid-killcam (so 0.5.366's guess wasn't the cause in
  that session). The watch itself over-reported: a value I took for "this
  body is in its dead mode" is set on every living body, guns have
  different bone counts (93, 94, 96, 102 all normal), and a body that isn't
  on screen isn't posed at all, so it looks "frozen".
- Now the watch only flags what can't be normal: a body on screen in the
  last second, not in a vehicle, moving, whose pose hasn't changed for 6
  seconds (the T pose), one still using the killcam's pose memory, and
  impossible bone counts. When you next see a T pose or a "dead" soldier
  walking around, the log should name him.

## 0.5.367: a watch on soldiers in the wrong pose

- You (00:40): this has been happening now and then for a while: some
  soldiers in the T pose, or lying on the ground looking dead while they're
  clearly alive and moving. So it isn't only the killcam.
- Every 2 seconds the game now checks every living soldier's body and
  writes to the log when one is in its "dead" mode while alive, uses the
  killcam's own pose memory, has a bone count that doesn't match what he's
  carrying, or hasn't changed pose for 6 seconds while he moves (the
  T pose). Each line says how long ago the last killcam ended, so it shows
  whether the killcam is to blame or something else is.
- Includes 0.5.366's fix (the killcam keeps the game's rebuilds).

## 0.5.366: no more T-pose soldiers after a killcam

- You, on 0.5.365: vehicles work with 32 players and no crash, the
  killcam is almost what it should be, but some soldiers stand in the
  T pose (arms out, no gun), one right next to your body after the
  killcam in clip 00.33.12, and they keep running around like that.
- What I think happens: for each frame of the killcam the replay swaps in
  each body's recorded pose and gun, then puts the body back after. If the
  game rebuilds a body during that frame (a replayed weapon switch makes it
  lay the body out again), putting back the old layout mixes the old and
  the new: a body whose pose can't be worked out, which the game draws in
  the T pose until it rebuilds that body again.
- Now the replay only puts back what it changed: if the game rebuilt a body
  during the frame, the game's new layout is kept. The log says each time
  this happens ("rebuilt by the game during the replayed frame"), so the
  next test confirms it.

## 0.5.365: crash in the killcam after a vehicle death fixed

- You, on 0.5.364: vehicles work with 32 players (confirmed: you got in),
  but the game crashed shortly after you were killed.
- The crash: during the killcam, the game looked up a point on a body (a
  "tag", like where the hand or a seat is) and followed a model that
  wasn't real any more, into empty memory. The killcam puts each body's
  gun back in its hands (0.5.359) from what it recorded; a recording made
  around the vehicle could hold a model that was no good by then.
- Now the killcam checks every recorded model the same way that lookup
  reads it before putting it on a body, and only when the body itself is
  the same one. And the lookup itself skips any model it can't read
  instead of crashing (the log says which body, once).
- The vehicle test logging from 0.5.361 to 0.5.363 is off now
  (COD3_VEHICLE_PROBE=1 turns it back on).

## 0.5.364: vehicles work again with 32 players

- FOUND (0.5.363's log, 32 players): your "let me in" request got through
  every check except the seat: the game said the seat was taken, then
  looked for whoever was in it and found no one.
- Why: each vehicle keeps one byte per seat saying which player sits there,
  and a special number meaning "empty". In the original game that number
  is 24 (one past the last of 24 players). When we raised the limit to 32,
  one of the places that empties the seats was changed to write 32, but the
  places that check "is this seat empty?" still looked for 24. So every
  seat looked taken. Bots hit the same wall.
- Now every place that empties a seat writes the same "empty" number
  (24 with the 24-player limit, 32 with 32), and every check looks for that
  same number: 11 places in the game's code, all in its vehicle seat
  handling. With the 24-player limit nothing changes.
- The vehicle logging from 0.5.361 to 0.5.363 is still in for this test.

## 0.5.363: vehicles: following the "let me in" message

- 0.5.362 caught it: getting into a vehicle is a message inside the game's
  session ("player n wants seat s of vehicle v"). The game handles it in
  five checks (who sent it, the vehicle, the player, the player against
  the vehicle, the seat) and then seats you.
- At 32 players one of those says no, or the message never arrives. This
  build writes each check's answer to the log, so one try at the jeep with
  32 players shows which one, and that is the line to fix.

## 0.5.362: vehicles: catching the moment you get in

- 0.5.361's two runs, side by side: at 24 players, the moment you get in,
  your player gets linked to the vehicle; at 32, that link never happens,
  though you stand right there and the prompt shows. Your player, your
  state and the vehicle otherwise look the same in both.
- This build catches the game in the act: the first time you stand next to
  a vehicle, it watches the spot where that link is written, and when it is
  written, the log gets the exact chain of game code that did it. With
  that chain I can follow the same path at 32 and see where it stops.

## 0.5.361: vehicles: finding what the 32-player limit breaks

- What we know: with the 24-player limit (PLAY-24-PLAYERS.bat) you get in
  the jeep and the motorbike; with your usual 32 the "hold to enter" prompt
  shows but nothing happens, and the bots can't get in either. The killcam
  is not it (PLAY-NO-KILLCAM.bat changed nothing).
- I went through the parts of the 32-player work that touch vehicles (the
  vehicle seat checks, the per-player object pool, the slot the map's
  "level" entity used to share with player 25): each is handled, so this
  build measures instead of guessing.
- While you stand next to a vehicle, the game writes your player and the
  vehicle to the log once a second. One short run at 24 and one at 32
  side by side shows exactly what's different, and that is the fix.
- COD3_VEHICLE_PROBE=0 turns it off.

## 0.5.360: killcam timed like CoD 4: your death, then the kill, then you falling

- You, on 0.5.359: it still isn't smooth, you can't tell when he actually
  kills you, and it freezes slightly when you die.
- I went through 22.17.22 frame by frame. The killcam started on the very
  frame you died, with no moment to see your own death. It also ended
  exactly on the kill: the last frame then sat still for 0.4 s (23
  identical frames), which is the freeze you saw. The kill itself was the
  last frame, so it never "landed".
- Now it runs the way CoD 4 does:
  1. You die and see your own death as normal (1.5 s, recording keeps going).
  2. The killcam plays from 4 s before the kill until 1.2 s after it, so
     you see him line up, the shot, and yourself drop, and then it ends.
  3. Back to the game.
- The recording holds 6 s now (it held 4.8 s), to fit the extra second
  and a half.
- To tune: COD3_KILLCAM_DELAY (ms before it starts, default 1500),
  COD3_KILLCAM_PRE (default 4000), COD3_KILLCAM_POST (default 1200).
- Vehicles on Eder Dam: noted, waiting on which vehicle and whether the
  "enter" prompt showed.

## 0.5.359: killcam: your gun back in your hands

- 0.5.358's log showed exactly how a body holds its gun: the body is a
  list of models (the soldier, then the gun) with 92 bones for the soldier
  plus 4 for the gun. When you die the game shortens that list back to just
  the soldier (the gun's entry stays written, but is no longer counted).
  The killcam was putting back your 96-bone pose of then on a one-model
  body, so the gun was never drawn.
- Now each recorded moment also keeps the body's model list, and in the
  killcam every replayed body gets the list it had then, put back right
  after each frame. Your replayed self, and every bot, holds the gun he
  was holding at that moment (and switches when he switched).
- Also explains 0.5.353: the "fallen body" bytes it reset were really the
  gun's place in that list, which is why you vanished in that build.
- To turn it off: COD3_KILLCAM_GUNS=0. The end-of-killcam log line counts
  "guns put back in hands".

## 0.5.358: killcam: finding your gun on your body

- 0.5.357 tested by you (clips 22.17.22 and 22.18.03): the killer's hands
  and gun now show for the whole killcam, from the house out to the street.
  Still missing: the gun YOU were holding, on your body in the replay.
- What I found: a body's skeleton has 92 bones without a gun and 96 with
  one (the gun brings its own 4). Your body showed 92 even while alive in
  one check and 96 in another, so the game puts the gun on and takes it off.
  The killcam put back your recorded pose, but not your gun.
- This build writes to the log every time a body's gun goes on or comes off
  (yours and a few bots'), with what changed. That shows exactly what to put
  back so your replayed body holds its gun. Nothing else changes; the
  killcam works the same as 0.5.357.

## 0.5.357: killcam: the killer's gun in his hands the whole time

- 0.5.356 tested by you (clips 21.57.20 and 21.58.19): a big step. You are
  in the replay running at him with your rifle, his aim follows you (the
  log: 0 degrees off his aim at the moment he killed you), and the melee
  that killed you plays. Still off: his hands and gun only showed up for
  the last half second (the melee), the first half second shows the
  ground, and the "Waiting for respawn" text shows over the end.
- The gun: the game's "draw the gun in view" step checks the killer's
  state (all fine in the log: weapon, alive, normal movement) and then
  two switches of THIS machine's own view, plus whether a gun model is
  ready. Your player is dead, so your view's switches say "no gun". For
  the killcam's frames those two switches are now opened (put back right
  after each frame), and all of the checks are written to the log four
  times a second so the next test shows exactly which one was holding
  the gun back if it still does not show.
- To turn this off: COD3_KILLCAM_GUNGATE=0.

## 0.5.356: killcam: the killer's real aim, and his gun back

- 0.5.355 tested by you (video): the killer's view stared at a wall while
  he was shooting you, and his gun was not in view.
- The wall: for a bot, the game keeps two view directions. The one the
  killcam used is where his body faces; his shots go along the other one
  (the direction his "machine" sends with each move: the game fires along
  that one, which I found when building the bots). The killcam now looks
  along the direction he actually shot.
- The gun: missing since 0.5.352, when I stopped the game drawing the
  killer's body during his killcam. That step also gets his gun ready for
  the view, so it is back on. The double-drawing problem it was meant to
  stop is now refused where the game makes its drawing list (and now
  anywhere in the list, not just at the end).

## 0.5.355: killcam: you back in the picture (the "stand up" change undone)

- 0.5.354 tested by you: no loop (the game refused the double add every
  time, as planned: it was a crate-like object, type 3), but you still
  were not in the killcams.
- The log says the game drew you in every frame of every killcam, right
  in front of the killer. What changed when you vanished (0.5.353) was
  the "stand the fallen bodies up" change I made for the weapons: it sets
  three of your body's settings back to "alive", and with your body in the
  fallen state that makes it not show at all. It is turned off again.
- So this build should show you again (as in 0.5.351/352), still without
  your weapon in hand; the weapon gets its own fix next.

## 0.5.354: killcam: the drawing-list loop stopped at its source; you back in it

- 0.5.353 tested by you (3 killcams): all still off, and you were no longer
  in them at all.
- What the log showed: the drawing-list loop still happened in every
  frame of every killcam, and my safety cut from 0.5.350 then threw away
  everything after that point in the list for that frame: you were among
  the things thrown away. That is why you vanished (and likely why other
  things looked "off").
- Found exactly how the loop is made: the game adds a model to the end of
  its list; adding the model that is already at the end makes it point at
  itself. Now that second add is simply refused, so the list stays whole,
  nothing is thrown away, and the log names which object it was.
- Also undone: 0.5.353 kept too much of each player's current data during
  the replay; now only their current body and record are kept, the rest is
  as it was then.

## 0.5.353: killcam: bodies stand with their weapons; one more loop cause

- 0.5.352 tested by you: still no weapons seen, the moment you're killed
  looks off, and (from the log) the drawing-list loop still happened, now
  on a body that belongs to no current player.
- That body: a bot who died and respawned since gets a new body; the
  replay was handing him his OLD one, which by now is someone else's (a
  fallen body), so that body was drawn twice. Now every player keeps his
  current body; only his state, place, aim and pose of then are replayed.
- Weapons: your body's set-up logged alive and dead shows the game puts a
  dead body in a "fallen" mode (three settings change), and a fallen body
  is drawn without a weapon. In the replay, everyone who is dead now but
  was alive then is put back in the living mode for the length of the
  frame, so they should be drawn with their weapons, you included.

## 0.5.352: the killcam freeze's real cause fixed; your weapon looked into

- 0.5.351 tested by you (2-minute video, 3 killcams): no crash; "a lot
  better but still a bit off, and I don't see the weapon I'm holding".
- The freeze guard from 0.5.350 caught the cause in the act: one player's
  body was being added to the drawing list twice in the same frame. That
  is the killer: in the replay he is the camera (his gun and arms are drawn
  from his body), and his body was also drawn as a player. A spectator
  following someone never sees that person's body, so now it isn't drawn.
  That removes the loop at the root, and may also clean up what looked
  "off" around the view.
- Your weapon: when you die you drop it, and your body's set-up changes
  (it loses the weapon's parts). The replay restores your moving pose but
  not the weapon in your hands yet. This build writes your body's set-up
  to the log once alive and once dead, so the next build can put the
  weapon back exactly.

## 0.5.351: crash after a killcam fixed

- 0.5.350 tested by you: no freeze this time (and the killcam in your
  video looks right: the killer's view, his gun, you running at him), but
  the game crashed a little after a killcam, while you were shooting.
- Why (from the crash report): a part of the game that runs side by side
  with the drawing (it handles effects attached to objects) read some of
  an object's bookkeeping while the killcam had put the values of 4
  seconds ago there; those pointed at things that no longer exist.
- Now the killcam puts back only each object's state (what it is, where,
  how it moves, its angles and events): exactly what a Call of Duty
  snapshot carries and what CoD4's killcam replays. The game's own
  bookkeeping is never touched.
- Still to do: the red "hurt" tint of your own death shows over parts of
  the killcam, and the "Waiting for respawn" countdown over its end.

## 0.5.350: killcam freeze guarded (and caught in the act)

- 0.5.349 froze again in a killcam, in the same place as 0.5.347: the game
  walking, forever, its list of models to draw, because one model had been
  put in that list twice and the list became a loop.
- Now, before the game walks that list, it is checked; a loop is cut where
  it closes, so the game carries on instead of freezing. And the log says
  exactly which model it was and which object it belongs to
  ("killcam guard"), so the real cause can be fixed next.

## 0.5.349: killcam: you move again in it

- 0.5.348 tested by you: no freeze (fixed), "a little better but still
  needs work". The log showed your own recorded pose was not used at all
  in 348: the replay's new pose memory sits in a range a safety check of
  mine turned away, and a fallback then posed you frozen. Fixed: you are
  given your recorded, moving pose again, and the fallback never runs
  while the replay holds your body.
- Please tell me in a sentence or two what still looks wrong (for
  example: the camera's height or direction, who is where, a body doing
  the wrong thing, missing shots or effects), so the next fix goes
  straight at it.

## 0.5.348: killcam freeze fixed

- 0.5.347 tested by you: the game froze about 2.5 seconds into a killcam.
- Why (from the log): the freeze was the game going round and round a
  list of the models it draws, forever. The killcam had put back an
  object of then (a smoke grenade was being thrown) including the link
  to its model, but that model had been removed and its slot reused
  since; the game then put it into its list a second time, and the list
  became a loop.
- Now the objects of then are put back with their places, states and
  everything the drawing reads, but their links to models and the game's
  own records stay the current ones, so the game's lists stay sound.
- The log counts how often an object's model had changed since then.

## 0.5.347: killcam: every body in its real pose; no more standing corpse

- 0.5.346 tested by you: still off, and after dying your body stood stuck
  upright until you respawned.
- The standing corpse: since 0.5.341 the killcam's recorder posed your own
  body every frame (the game never does in first person) so it could be
  replayed, and it kept doing that after you died, over the game's own
  death pose. It now only does it while you are alive.
- Bodies in the killcam: the log of your test said only about 1 body in 4
  got its recorded pose; the rest kept their pose of now. The pose was put
  only when the body's skeleton matched the one of then exactly (a dead
  body has fewer bones than a living one; a bot who respawned has a new
  set-up). Now, for the length of the replay, each body is given the
  recorded pose with the skeleton set-up of then, and its own is put back
  after, so every body moves as it really did.
- Please record a video of the next test (a few killcams), so I can see
  exactly what still looks off.

## 0.5.346: the true killcam, step 2: everything of then, exactly

- 0.5.345's measuring worked (thanks for the test). It showed exactly what
  the game's drawing side reads when it draws a frame: the list of the
  match's objects (about 250: you and the bots, 25 others like flags and
  pickups, about 150 more like grenades, rockets, smoke and map objects),
  each object's own data, the players' records and their bodies. And what
  it writes itself (which shots and sounds it already played).
- The killcam now records ALL of that, every server frame (about 66 a
  second), for the last 4.8 seconds, and plays back exactly the frame of
  that moment: the same objects in the same places, the grenades and
  rockets that were flying, the smoke, everyone's exact pose and aim. No
  more blending between frames (that was the last piece of guessing).
- Still the same: the game's own drawing code draws it, from the killer's
  eyes, with his gun, his shots and their sounds.
- Not yet: the "Waiting for respawn" countdown over the end of the
  killcam; that is next once this is confirmed.
- If something looks broken: COD3_KILLCAM_OBJECTS=0 turns the new part
  off (players only, as 344), COD3_KILLCAM_BLEND=1 brings the blending
  back.

## 0.5.345: the true killcam, step 1: measuring (no play change)

- You asked for the real thing, as Call of Duty 4: an exact recording of
  what happened before you were killed, replayed by the game itself.
- How CoD4 does it: the server keeps the last seconds of the snapshots it
  sends (everything the player's game draws from); the killcam plays them
  back through the game's own drawing code. Nothing is filled in.
- What is different here: on the hosting PC the game's drawing side does
  not get snapshots at all; it reads the match's memory directly. So the
  recording has to hold exactly the memory the drawing side reads, every
  server frame, and the replay has to put exactly that back. To do that
  without guessing, this build MEASURES it: for a few frames it watches
  every read and write the drawing side makes to the players, the other
  objects (grenades, rockets, flags...), the players' records, bodies and
  network marks, and writes the list to the log.
- What you do: play one normal match with bots (at least 4) for a minute,
  get killed once so a killcam plays, then quit. You will feel a short
  hitch twice (30 s in, and 1 s into the first killcam): that is the
  measuring. Then send me the log (don't start the game again first).
- Next build: the recorder and the replay built on that list (every
  object, every server frame, no blending), replacing the current killcam.

## 0.5.344: killcam: you running, not just standing

- 0.5.343 tested by you: you stood up in the killcam, but did not move
  (no running).
- What the log showed: while you play, your body's pose IS recorded
  properly and moves (running, turning). In the killcam, the game writes
  your dead body's pose into place every frame, after the killcam had put
  your recorded pose there: that is why 341 showed you lying. 343 posed
  you itself right before drawing, which stood you up, but froze you.
- Now your recorded, moving pose is put in right before the game draws
  you, so the replay shows what you were really doing. One catch found in
  the log: your body has 96 bones alive and 92 once dead; the first 92 of
  the recorded ones are used (the log says so if it happens).

## 0.5.343: killcam: your body animated from how you moved (try 2)

- 0.5.342 tested by you: still lying on the ground, not running at him.
  The log shows the killcam itself working (you drawn, alive, health 100,
  the killer's shots and events played, the death text hidden): only your
  body's pose is wrong.
- Why (best reading): leaving your pose alone in 342 meant the game drew
  whatever your body's pose was right then, and right then you are dead,
  so: lying. Now, just before the game draws you in the replay, your body
  is posed from your recorded self (alive, moving), the same way the bots'
  bodies are posed every frame.
- The log now prints your body's pose numbers while you play (every 10 s)
  and before and after this step in the replay, so if you still lie down
  I can see exactly what the game is doing with your body.
- Same as before: close the game after the test and tell me before you
  open it again.

## 0.5.342: killcam: you running, not lying on the floor

- 0.5.341 tested by you: you were in the killcam, but lying on the floor.
- Why: your own body is never animated by the game while you play (you
  see through its eyes, in first person), so its pose stays the last one
  it had: the fall from your last death. 341 recorded that pose and put it
  on the replayed you. Now your own pose is left out: in the replay you
  are drawn like any other player, and the game animates you from how you
  actually moved (running, crouching, turning). Everyone else keeps their
  recorded poses.
- Log: the killcam's end line also says "your own pose left to cgame N
  times". COD3_KILLCAM_OWN_POSE=1 brings back 341's way, to compare.
- Note: the 341 test's log was replaced by two later starts of the game,
  so this fix comes from your description and the code. Please send the
  CoD3MP.log right after a killcam test next time (before starting the
  game again).

## 0.5.341: killcam: poses, shots, no death text; depth copy off by default

- Poses: every body in the replay now has the pose it had at that moment.
  A body's pose is its bone matrices (rig +112, from the entity's rig at
  +492), filled in place once a server frame; the replay moved bodies but
  each kept its pose of now (you: wounded, lying). The recorder keeps each
  player's bones with every sample, and during the replay each body is
  given its recorded bones (blended between samples) for cgame's frame and
  the scene, put back after. Your own body (never drawn in first person,
  never evaluated) is now evaluated by the recorder. COD3_KILLCAM_POSES=0
  turns this off.
- Shots: cgame plays an entity's events (shots, flashes, sounds) when its
  event sequence (+184) passes what it has played (+1000). The replay wrote
  the recorded +1000 back every frame, so no event was ever new. The replay
  keeps its own count now, so the killer's shots play.
- "You are critically wounded" no longer shows over the killcam: the death
  panel (sub_823A3690) is updated as hidden while it plays.
- F11 > Graphics > Optimizations "Depth copy on its own thread" is OFF by
  default (it crashed 0.5.340 multiplayer when aiming right after loading
  in). Saved under a new name, so a 0.5.340 settings file does not turn it
  back on.
- Log: "killcam: client N's pose", "killcam: replayed event", "killcam:
  death text hidden", and at the end "killcam: over (...): N events played,
  poses put N times".

## 0.5.340: merged GitHub pull request #2 (legoliamneeson's PC work)

- Brought in the pull request merged on GitHub (from legoliamneeson, built on 0.5.183): PC performance, stutter, picture and sound work, all on top of our 0.5.339.
  - **HDR mode** (F11 > HDR), on the RenoDX shader library and the Prism tone mapper (both MIT, in thirdparty/): tone mappers, peak/game/UI brightness and RenoDX's sliders.
  - **Optimizations**, each with its own switch in F11 > Graphics > Optimizations, so if anything looks wrong it can be traced to one: depth on its own thread, faster constants, shader load cache, staging uploads, texture pool and more; sharp shadows, fast shadow maps, smooth foliage edges, steady decals, effects resolution.
  - Sharper picture on Windows display scaling above 100% (the game window is now DPI aware).
  - Sound: a limiter instead of clipping; audio channels setting.
  - Log counters that stop counting once they have said their lines (they were counted forever).
- Everything of ours is kept: killcam, bots, mortar, killstreaks, the console-less window (we keep WIN32_EXECUTABLE ON for multiplayer: no extra cmd window), max players 32, sprint hold.
- No game data in any of it (checked).
- If the picture or sound misbehaves after this, F11 > Graphics > Optimizations: turn them off one at a time and tell me which one.

## 0.5.339: killcam: you, at last (the "followed player" rule)

- 338 confirmed: the Panzerschreck's explosion is back (the map's sound bank loads now), and the other players are drawn in the killcam. The log shows the game was drawing you too, every frame, but you were not seen.
- Why: the killcam shows the killer's view as a spectator "following" him would see it, and the game hides the body of the player being followed (you do not see the body you look out of). The followed player is named by his number, and during the killcam the killer's number points at YOUR place in the player list (the two places are swapped so the game takes the killer for you). So the game hid you, thinking you were the one being followed. The killcam now names place 1, which holds the killer: he is hidden, you are shown.
- Still to do: other players' poses in the killcam ("one guy looked like he was laying down while running"): their positions are from the replay but their body animation is still the live one. That needs the animation state recorded too; next after this.

## 0.5.338: killcam: everyone back in it (my sum was wrong); map sounds

- FOUND IT, and it was my mistake: 337's counts showed that during the killcam not one player went to the game's player drawing, nobody at all, not only you. Since 333 the killcam has marked every replayed player as "in the picture" with the game's frame number, but I read that number from the wrong address (a sum done wrong: 0x82C281D4 instead of 0x82C28274). So every replayed player failed the game's own check and none was drawn: not you, not the guy in front of you. That also matches "I can't see anyone else". Fixed: you, the killer's targets and everyone else he saw should be drawn where they were.
- Panzerschreck explosion and other missing sounds: on Merville the map's own sound bank was not loading (its explosion, melee, grenade throw, enemy gunshot, dying and ambience sounds were all missing from the log). The campaign sound banks loaded for the campaign guns and the mortar left too little sound memory for the map's bank, loaded last. The sound memory is raised from 36 MB to 64 MB.
- Still to come in the killcam: explosions, smoke and other effects as they were (they show as they are now; only players are replayed so far).

## 0.5.337: killcam diagnostic (why you are skipped)

- 336's log: during the killcam you ARE in the list of things the game puts in the picture and you pass its test, yet the game's player drawing never gets you. This build counts, every 30 frames of a killcam, how many things go through each step (and whether you do), to find the exact step that drops you. Nothing else changes.
- Bazooka sound: you heard it this time (336), so it was not the build; it may depend on the map (it was missing on Merville). Next time it is silent, tell me which map.

## 0.5.336: killcam: you in the picture (closing in)

- 335: the killcam looked clean, but you were still not in it. The killcam switch-over now also covers the exact routine that puts the players' bodies into the scene (in case the step 335 covered is not the one that runs it), and the log says, once per killcam, whether you are in that routine's list and whether you pass its test, with the numbers. With that line the last piece is certain.
- The blue "double" around the other player: in your video there is a pale, blurred figure in the smoke during the killcam. Noted; if it is a player drawn twice (where he is now and where he was), the same line will show it.
- Bazooka with no sound: the log shows your reload sound being sent as yours (so not the bots' sound filter), but the bazooka's firing sound is not found in the sounds loaded on Merville (BAZOOKA_FIRE_3RD missing). Being looked into next, separately from the killcam.

## 0.5.335: killcam: you in the picture (found it)

- 334's log settled it: the killer did turn onto you (3 degrees off his aim, 360 away, at the moment you died), but the game was never asked to draw you. The game adds the players' bodies to the picture in a second, separate step of drawing a frame, and the killcam's switch-over only covered the first step (your view, the gun). In the second step everyone was where they are now and you were still "this machine's player", so left out. The killcam now covers both steps: you, and everyone else, where you were.
- The bot's view hugging the walls (334's video): the killer was a bot walking along the houses with his aim held on you through the walls (334's log: 24 to 63 degrees off, turning onto you as you came round). That is how the bots move and aim, the killcam is showing it faithfully; making bots look where they walk instead is a bot change for later if you want it.

## 0.5.334: killcam: you in the picture (try 2, with the log to prove it)

- 333's videos: the killer's gun, his hands and his view are right now, but you are still not seen. One more thing found in the game's player drawing: a body is left out when a certain flag is on it, which your own player may carry as this machine's player. It is now taken off the replayed you (only while you were alive in it).
- To settle it either way, the log now says, twice a second during the killcam, where you were from the killer's eyes (how far, and how many degrees off his aim), and whether the game was asked to draw you. So if you are still not seen, the next fix is exact.

## 0.5.333: you in the killcam, the moment you are shot; mortar on level ground

- Killcam (332: smoother, you saw them walk, but not yourself or the moment they shot you):
  - The game only draws the players that are in your own last update from the server, and your own player is never in it (you are the one looking). So the you of 4 seconds ago was there but never drawn. During the replay every recorded player is marked as in it, so you are drawn, and so is anyone the killer saw, even if you could not see them from where your body lies.
  - Players are now recorded after they die too, so the replay shows you hit and going down, not frozen standing at your last moment alive.
- Mortar (332: arms in view, standing to the left of it): the log shows the tube was put down 58 units above your feet, on a ledge in front of you (the ground search took anything up to 60 above), so your view sat low and off under it. Now the tube only goes on ground within 20 above or 60 below your feet, else at your feet' height. The log says where ("mortar: ground under the tube").
- Bots throw smoke 5% less often (every 47 to 68 s at most, was 45 to 65 s), every class that carries smoke.
- From 332's log: Rusher 2's artillery works (he called two strikes and they came down), and his kills took 10, 15, 20, 25 s off his wait. You played a scout but none of your kills took anything off: the log now says why each time ("nothing to take off": probably your artillery was already ready, unused).

## 0.5.332: the killcam as the killer (his gun, his shots, you in the picture)

- 331's killcam played its full 4 seconds but felt like "watching a ghost": no gun, no muzzle flash, and you could not see yourself being killed. Dug into how the game draws a frame on the host PC and found why:
  - The gun in your view is drawn from your own player on the server: his state and also his body's model and animations. 331 gave your player the killer's state but your body stayed your (dead) body, so the gun never showed and nothing flashed.
  - Every player's body is drawn except your own player's (you don't see yourself in first person). Your past self was put back where you stood, but as "you" it was never drawn.
- So now, only while each killcam frame is drawn, the game is told the killer is you: your player's place in the game's list holds the killer, and the killer's place holds you. The game then draws exactly what he saw: his view, his gun with its own animations and muzzle flashes, his body left out, and your body drawn like any other player, where you were, alive, until he shot you. Everyone else is where they were, as in 329. Put straight back after each frame; nothing in the real match changes.
- Bots switching to a special (Rusher 2's binoculars, rifle grenades, mines, packs): 331's log showed the switch never finished (no binocular fire, no barrage, the rounds unspent): the bot's orders kept asking for the gun in his hands while the switch was under way. They now ask for the weapon being switched to until it is up.
- COD3_KILLCAM_AS_KILLER=0 goes back to 331's way, if this misbehaves.

## 0.5.331: artillery earned faster with kills

- The class artillery (the scout's and Rusher 2's, about 130 s to come back) now comes faster with every kill: each kill takes time off that player's wait, more for each kill in a row this life (10 s, then 15, 20, 25, and 30 s at most per kill). Everyone's, people and bots. On screen for you: "ARTILLERY -15 s (60 s left)", or "ARTILLERY READY". The killstreak artillery is not changed.

## 0.5.330: the killcam plays all the way through

- 329's killcam stopped after half a second and went back to you lying dead (you saw it, and the log says "over (respawned)" each time). My mistake: while the replay draws, your player is shown alive as you were, and the check for "has he respawned?" read that replayed you instead of the real one. It now reads the real one, so the replay runs its full 4 seconds.
- From the same test's log: rifle grenades are now given (riflemen and Demolitionists had 3-4 each) and the bots took them out 26 times, but they were put away again at once: every bot's orders always asked for the first gun of his kit. Now a bot holding a special (rifle grenade, mine, pack, binoculars) keeps it until he is done with it.
- Rusher 2 showed up (one a side, Sten, Bren and Mills bombs all given), but the same thing kept the Bren in his hands; he now gets the Sten on the move and the Bren holding the objective.
- Rusher 2's artillery: he had no spotter's binoculars. His class's artillery is now started for him each life, and when it is ready he is given the spotter's binoculars with a round, uses them, and waits for it to recharge.

## 0.5.329: smoother killcam with everyone in it, Rusher 2, rifle grenades that work

- Killcam (you saw it in 328, a bit laggy, and only the killer's view was from the past):
  - Recorded every server frame now (it was 30 times a second from the drawing, out of step with the game), and the killer's place and aim are blended smoothly between frames instead of jumping.
  - Everyone else is shown where they were at that moment too, you included (alive, as you were when he shot you). Their places and turning are blended the same way.
  - The killer's own body is hidden during it (you are looking through his eyes).
  - COD3_KILLCAM_OTHERS=0 keeps everyone else where they are now, if that part misbehaves.
- New bot kit, Rusher 2 (one a side), exactly as you asked: the Sten, the Bren as his second gun, Mills bombs, built on the scout so his class special is the artillery called with the binoculars (his own, not the killstreak one).
  - Very aggressive: a flanker who presses every fight.
  - His errands go round: push up to the enemy's spawn, clear the low ground (inside, room to room) toward them, back to the objective and hold it a while with the Bren, a flank round the outskirts, then again.
  - Takes the Bren when holding the objective, the Sten on the move.
  - Artillery: someone in sight 900 to 4500 away and no one within 600, he takes out his binoculars, puts them to his eyes, marks the spot and goes back to his gun (at most every 30 s, when it is ready).
  - In bot_kits.txt as "rusher2"; the grenade column now also takes "mills".
- Rifle grenades: 328's log showed no rifleman or Demolitionist bot had any (the class special is never given to bots). They are now given once a life, and the bots take them out by name (the way they take out mines and packs), fire, and go back to their gun.
- Binoculars with the sides missing: that is the old on-and-off problem from 0.5.244-252, not something from these builds (in 328 it happened before any killcam). Next time it happens, a screenshot with them up, and whether it is every time or only sometimes, helps.

## 0.5.328: killcam replay, second try

- 327 handed the killer's recorded state to the game, but you still saw your own body: on the host PC the game draws your view from your player's state on the server itself, not from what it received. Now, only while the picture is being made, your player on the server holds the killer's recorded state, and your real one is put back straight after, before the match moves on. So the view, the gun in hand and the aim should be his.
- Rifle grenades: 327's bots fired none. This build logs what each rifleman and Demolitionist has for rifle grenades (owned or not, rounds), so the next fix is exact.

## 0.5.327: the real killcam replay (first try)

- The killcam is now replayed by the game itself from your killer's own recorded state: meant to show his first-person view with his gun in hand, his aim moving the way it did and his shots, for the last 4 seconds before you died (first try: tell me what you see). Before, only the camera was moved to where his eyes were.
- Found how the game feeds your screen: on the host PC, the picture is made from your own player's state copied straight from the server. During the killcam it is handed the killer's recorded state instead, marked as "following" him (the way a spectator watching him would see it), so nothing in the real match is changed.
- Still the same: the other players (and your own body) are where they are now, not where they were. That is the next part.
- If anything looks broken, COD3_KILLCAM_REPLAY=0 goes back to 0.5.326's camera-only killcam.

## 0.5.326: Demolitionists drive, bots use their rifle grenades, killcam names the killer

- Killcam: the HUD (the "You are critically wounded" text and the rest) is hidden while it plays and comes back after; the banner names who killed you ("KILLCAM - Bot 7").
- Two Demolitionists a side (was one), and more riflemen: an extra rifleman line near the top of bot_kits.txt, so more bots carry rifle grenades.
- Demolitionists play aggressively (rushers, pressing the fight whenever not badly hurt) and throw their sticky bombs more often (every 5 to 8 s instead of 9 to 14 s).
- Demolitionists use vehicles: they take one even with a fight going on (unless an enemy is within 500), always as the driver, drive it to a far part of the map (over 1500 away, a different kind of place each time), get out there to fight, and take a vehicle again later.
- Every bot that has a rifle grenade (riflemen, Demolitionists) now switches to it and fires it at enemies it can see 400 to 2200 away, with the arc raised for the distance, never near its own side; then it switches back to its gun. About every 10 to 16 s, Demolitionists every 6 to 9 s. Log: "fires a rifle grenade".
- Note: bot_kits.txt in the game folder was updated (the Demolitionist and rifleman lines moved to the top; your other lines kept as they were).

## 0.5.325: killcam groundwork 2 (diagnostic)

- No change in play. Every 10 s the log records where the game keeps the players it draws, for the killcam's next step: everyone shown where they were (you alive and moving, the killer's shots), not where they are now.

## 0.5.324: killcam from the killer's real eyes

- 323 moved the view, but to the wrong place: a bot's record gave its height as 0 and its aim as level, so the camera sat at the killer's feet. The killcam now takes the killer's position from the game's own entity (and a bot's aim from it too).

## 0.5.323: killcam shows

- 322 played the killcam but nothing showed: it changed the game's camera settings after the view was already made from them. Now the killer's view goes into the view itself, just before the game finishes it.

## 0.5.322: KILLCAM (first version)

- When another player or a bot kills you, "KILLCAM" comes up and you see the last 4 seconds before your death through the killer's eyes, at real speed, then the normal death view and respawn.
- First version: the camera replays the killer's view; the other players and effects are shown where they are now, not where they were. On the host's PC (and offline with bots); the joining PC comes later.
- COD3_KILLCAM=0 turns it off.

## 0.5.321: killcam groundwork (diagnostic)

- Nothing changes in play. The log records how the game builds your view (its view modes when you die and respawn, the camera's numbers, where other players sit on the client side), so the killcam can be built on it in one go.

## 0.5.320: co-op v36 from the friend

- Co-op: the host warns on screen when the joining PC runs an older build (both need the same CoD3.exe); a joiner restarted after the host dies no longer sits in an unskippable film (the host says his film is over every second, and after 15 s the joiner's own A ends it). Messages unchanged from v35.

## 0.5.319: the mortar camera where the campaign puts it

- Measured from the screenshots: the campaign camera at the mortar is not your eye. It sits right behind the tube (about 15 behind the muzzle, a little over it), 3.4 times nearer than 318's view. Now the same.
- No arms hanging in view at the mortar (they come back when you put it away).
- The launch glow and smoke at the muzzle go away after 1.5 s.
- The view no longer changes after the first shot.

## 0.5.318: the mortar view exactly as the campaign

- Your eye now sits exactly where the campaign puts it over the tube: 48 units behind it and 49 above its base (the campaign's tube stands on a ledge 17 units up; here the view comes down 17 instead).
- No camera jumps: RT never starts the campaign's loading step.

## 0.5.317: mortar lined up with the campaign (measured)

- Measured in the campaign at Laison: you stand (no crouch), the tube is 48 units straight ahead of you, facing exactly where you face. The multiplayer mortar now does the same; the tube sits on the ground, so you look a little more down onto its top.
- The round comes in from your right, over the muzzle, then down the tube.
- The tube no longer stands on top of you (it pushed you aside in 315).

## 0.5.316: the campaign mortar measured

- Campaign only: while you are at a mortar (Laison), the log records exactly where the game puts you, your view and the tube ("mortar probe:" lines), so the multiplayer mortar can be lined up to match. Nothing in play changes.

## 0.5.315: mortar without camera jumps; co-op v35

- Mortar: the campaign's loading step is off. It sent the camera across the map at every shot and never showed arms in multiplayer. You crouch over the tube, look level, and the round is fed in and fired by RT.
- Co-op v35 from the friend (smoother soldiers, grenades, deaths and weapon drops, sprint): both players need this build.
- MAKE-FRIEND-UPDATE.bat also makes a source zip.

## 0.5.314: mortar camera stays put when firing, no black screen (test)

- The campaign mortar's loading step pressed and held one of your buttons after loading (how the campaign fires its round). In multiplayer it never let go: the camera flew off while firing, and putting the mortar away mid-load could leave the screen black. That press is now left out.
- A put-away no longer restarts the mortar the next time you set it up.
- Camera a little lower again: mortar_view default -8 0 -18.

## 0.5.313: mortar aim steady after the first shot (test)

- After each shot the mortar's interaction goes back to its idle step, so the next RT loads again and the aim ring reads the same as on the first shot (it stayed stuck in the loading step before).
- The log shows the aim (pitch, yaw, range) once a second.

## 0.5.312: mortar feed from the hands, lower camera, no screenshot lag

- The round now comes up from your hands, over the muzzle, then slides in and fires.
- The camera at the mortar is lower (shows more of the tube): mortar_view default -8 0 -14.
- Screenshots no longer lag the game: the full frame log and per-draw pictures (the big -draws.bin files) only happen with COD3_FRAME_LOGS=1.

## 0.5.311: no crash; the round fed into the tube (test)

- 310's crash fixed: the round is no longer put in the hands (its model breaks the arms' moves in multiplayer).
- RT now feeds a round: it appears over the muzzle, slides in, and then fires (about 0.8 s). Everyone sees it.
- mortar_view takes two more numbers, the muzzle's forward and up (default 24 42), to line the round up with the tube.

## 0.5.310: the mortar round in your hands (test)

- At the mortar you hold the campaign's own round (mortar_shell) on the multiplayer's interaction arms, so the loading moves should show the round going into the tube.
- If it crashes: set COD3_MORTAR_ROUND=0 to go back to bare arms.

## 0.5.309: mortar camera back and down (test)

- At the mortar the camera sits a little back (8) and down (6), through the interaction's own view offset.
- New console command: mortar_view <forward> <right> <up> moves it live.

## 0.5.308: mortar explosions back (test)

- Each round's landing again sets off the panzerschreck's and the frag's explosions (seen before), plus the campaign round's own. The round's explosion data is logged once to find why it shows nothing.

## 0.5.307: mortar faces your aim, you crouch at it (test)

- The tube turned a quarter left: its muzzle now leans away from you, the way you aim.
- You crouch when the mortar goes down (the campaign's eye height at it) and stand back up when you put it away.
- mortar_yaw takes a fifth number: 1 crouch at the mortar, 0 stand.

## 0.5.306: mortar tube stands where you stand (test)

- The campaign puts you right on the tube's spot (its offsets are 0 forward, 0 right, eye 45 up), and the arms are drawn in the tube's frame. The tube now stands at your feet and turns with you.
- mortar_yaw now takes four numbers: facing, forward, right, up (e.g. mortar_yaw -90 0 0 0), so the tube can be lined up live.
- Once a second while on the mortar, the log shows what the interaction does with the view and the arms ("mortar look").

## 0.5.305: mortar placed where the campaign stands you (test)

- The tube is placed using the campaign turret's own player offsets (mortar_81mm_turret forward/right/up), so the arms' moves line up with it. The values are logged.

## 0.5.304: mortar arms on the multiplayer's own interaction arms (test)

## 0.5.303: mortar crash fix 2 (test)

- The campaign round's arms are set up before you hold it (the crash).
- Console: mortar_yaw <degrees> turns the tube while you are at it; tell
  Claude the number that faces your aim.

## 0.5.302: mortar arms fix (test)

- At the mortar you hold the 81mm round (the campaign's), so its arms
  play the moves; putting it away ends the interaction cleanly (no more
  camera in the void).

## 0.5.301: mortar crash fix (test)

- The campaign mortar's first step is campaign-only and crashed
  multiplayer: it now begins at its second (holding the round). The tube
  faces the way you aim.

## 0.5.300: campaign mortar arms (test)

- Setting up the mortar killstreak now starts the campaign mortar's own
  interaction: its arms and the loading of the round. Test: streak_give
  mortar, d-pad down, d-pad right, watch your hands, press RT.

## 0.5.299: the campaign mortar visible

- spawnvehicle mortar (or mortar_81mm) puts down the campaign mortar with
  its model.

## 0.5.298: the campaign mortar's files, done properly (test)

- The game now makes small copies of Laison's files on your PC (the first
  time a map loads) holding only the mortar's own files: no Laison level
  data, so the map draws as normal. Test: the world draws, then
  spawnvehicle mortar_81mm on Eder Dam.

## 0.5.297: maps draw again

- Laison's file is off again (the world drew invisible with it, loaded
  before or after the map). The mortar killstreak works as in 0.5.290.

## 0.5.296: Laison's file after the map (test)

- Laison's file is loaded after the map instead of before, so the map's
  own textures are found first. Test Fuel Plant's floors and walls, and
  spawnvehicle mortar_81mm on Eder Dam.

## 0.5.295: Fuel Plant fixed

- Laison's file is no longer loaded (it made Fuel Plant's floors and walls
  invisible). The mortar killstreak works as in 0.5.290.

## 0.5.294: Fuel Plant's invisible world (fix attempt)

- Laison's mortar files are no longer searched when the map draws its
  floors and walls.

## 0.5.293: weapon limit test switch

- COD3_WEAPONS_PAST_63=1 lets the campaign mortar's turret and round use
  the game's spare weapon room past 63 (for full maps like Fuel Plant).
  Test only.

## 0.5.292: weapon limit fix

- The game holds at most 63 weapons on a map. Fuel Plant already uses
  all 63 with the campaign guns, and the mortar's additions went past it
  (why your gun stayed out). Nothing goes past 63 now; the binocular
  stand-in hands are gone.

## 0.5.291: the campaign mortar's files (test)

- The campaign mortar's own files are loaded from Laison: the 81mm as a
  vehicle, its turret, its round, the arms' moves and effects.
- The mortar killstreak's rounds use the campaign's own launch flash and
  dirt explosion when they load.
- Test: spawnvehicle mortar_81mm in the console, and try to use it.

## 0.5.290: Fuel Plant plane

- 4% higher again (11% over the other maps' height); its circle round the
  whole map again.

## 0.5.289: mortar hands fixed, Fuel Plant plane

- The mortar's hands and round should now show (0.5.288 put the gun away
  but drew nothing in its place).
- Fuel Plant: the UAV plane flies 7% higher and its circle is a little
  further inside the map.

## 0.5.288: mortar hands

- At the mortar your gun is put away: you hold an 81mm round in the
  campaign mortar's own arms pose. Your gun comes back when you put the
  mortar away.

## 0.5.287: more memory for maps

- With big memory on, maps now get 608 MB to load into instead of 495
  (about 113 MB more), so the campaign guns, both UAV planes and the
  mortar all fit together, with room to spare for more.
- The mortar's tube is back in front of you.

## 0.5.286: loading fix

- 0.5.285 froze loading Eder Dam: the mortar's model file did not fit in
  memory beside the campaign guns. It is left out for now: the mortar
  works, but its tube is not drawn in front of you.

## 0.5.285: the mortar killstreak

- 4 kills in a row earns a MORTAR (it shows in the killstreak box, below
  the UAV). Pick it with d-pad down, set it up with d-pad right.
- The campaign's 81mm mortar is set down in front of you and you stay put
  behind it. Turn to aim it; look up to fire further, down to fire closer
  (600 to 5000 units). A red ring on the screen shows where the round
  lands; the panel at the bottom shows rounds left and the range.
- RT fires: 10 rounds, one every 1.6 seconds. Each round is loaded, fired,
  flies its arc, whistles in and explodes.
- Much bigger blast than a grenade: a sure kill close in, damage out to
  more than twice a frag's reach, less through walls. It never hurts you
  or your own team.
- D-pad right puts it away; rounds left are kept for later. Dying or
  running out puts it away too.
- Works for both PCs: the other player sees your mortar, the rounds and
  the explosions. Both PCs need this build.
- Console: streak_give mortar (to test).

## 0.5.284: the Demolitionist bot class

- New bot loadout, saved in bot_kits.txt as you described it:
  DEMOLITIONIST: main weapon the Bren, second main weapon the MG42 (in
  place of the pistol), sticky bombs as his grenades, and rifle grenades
  (built on the rifleman, whose special they are). Weight 2, so a couple
  spawn in every match (at least one a side).
- bot_kits.txt can now give any kit a second main weapon and sticky bombs
  (two optional columns after smokes).
- Fix for 0.5.283: bots in the 3 new slots would have spawned without
  their kit; they get it now.

## 0.5.283: full 32 (test)

- Bots can now use the last 3 player slots the game normally keeps for
  split-screen players on your PC. FILL at 32 players gives 31 bots solo
  (32 in the match), or 30 bots with your buddy (32 total).
- These slots are filled last, so anyone joining in the lobby still gets
  in first. Someone joining mid-match when all 32 are full can't get in
  yet (a bot would have to leave for them: later).
- Test solo first: 32 players, BOTS: FILL. Watch for anything odd on YOUR
  screen (split screen, camera, HUD, controls) and count the scoreboard.

## 0.5.282: bots keep learning; scouts look out of windows

- Lookouts: every long-range kill (1100+ away, yours, your buddy's or a
  bot's) teaches the bots where the shooter stood and which way he looked
  (a window, a ledge, a roof edge). The same spot again scores it higher.
  Saved per map, so it builds up game after game.
- Snipers go to the best-scoring lookouts for their watch and watch that
  way first. Scouts and snipers walking past a lookout stop 4-6 s, sweep
  the view out of it, then move on (they don't camp in it).
- Sharpening: at the start of each game the old lessons (where bots died,
  got kills, got stuck, and the lookout scores) count 85%, so recent games
  matter most and the bots keep adapting.
- Includes 0.5.281 (killfeed colours by team).

## 0.5.281: killfeed colours by team

- Both names in a kill line are coloured from YOUR point of view: your
  team green, the enemy red. Your teammate kills an enemy: green killed
  red. An enemy kills you or a teammate: red killed green. (Before, the
  one who died was always red, so you saw "red killed red".)
- Each PC colours it for its own player, so your buddy sees his side green.

## 0.5.280: plane higher on Poissons, circle a little tighter

- Poissons: the plane flies another 700 units higher.
- Every map: the UAV's circle is 2% smaller, a bit in from the outskirts.
- 279 check (from your log): other players' reload / weapon-raise sounds
  were dropped (20+ of them), your own were still played.

## 0.5.279: no more bot reloads in your ears; Poissons plane higher

- The bots' reload, rechamber and gun-handling sounds are no longer
  played as if they were yours (their reloads went through the very same
  first-person sound as your own). Their gunshots are untouched.
- Poissons: the UAV plane flies higher (it was only ~200 above the
  ground).
- 278 worked: the lobby BOTS setting filled the match, and the anti-armor
  bots were given mines and laid them.

## 0.5.278: bots get mines; reload sound trace, take 2

- Mines: no bot class carries mines, so none were laid. Anti-armor bots
  and campers (not medics or support) now get mines each life and put
  them down near the flag or where they hold.
- Reload sounds: the first trace looked in the wrong place. This one
  watches the reload animations' own sound events. Play a few minutes
  with bots reloading near and far from you, then I read the log.
- Smokes left as they are (you said they're perfect now).

## 0.5.277: bots as a lobby setting + bot behaviour pass

- BOTS is now a match setting: System Link -> Advanced Options, turn the
  page (L1/R1) to GAME MODES, BOTS: OFF, 2, 4, 8, 12, 16, 22 or FILL (as
  many as the player limit leaves). The host's game adds them when the
  match starts; everyone else gets them from the host. Anyone who hosts
  with our build can use it.
- Far fewer smoke grenades: one at most per bot (none for heavies and
  anti-armor), and 45-65 s between a bot's smokes (was 16-26 s).
- More bazookas / Panzerschrecks: three anti-armor kit lines now.
- Mines: bots that carry mines put them down near the flag (or where
  they camp), up to three a life.
- More bots play the objective (38% of each side, was 25%), fewer camp.
- Diagnostic for the bot reload sounds you hear as your own: play a
  minute near and far from bots, and I read the "reload sounds" lines.

## 0.5.276: two-PC UAV fixes

- Every plane message reached your buddy's PC twice, so every hit
  counted double on his copy (that's why your second plane dropped to
  one rocket on his screen). Messages are now counted once.
- His own UAV: kills are counted on the host, so his PC never knew he
  had earned one. The host now tells his PC; he gets "UAV EARNED" and
  d-pad right calls it, and everyone sees his plane.
- Both PCs must use the same player limit (both 24 or both 32): the
  first try had you at 32 and him at 24.

## 0.5.275: the UAV plane for everyone in the lobby

- System Link is peer to peer: every PC runs its own copy of the game
  world, so the plane only existed on the PC that called it. Now that PC
  tells the others, and each PC flies its own copy on the same circle:
  everyone sees it, hears its engine and flybys from where THEY stand,
  and sees the hit flashes and explosions.
- Hits are shared: your buddy's bullets on his screen take health off
  the plane on yours too, and when it goes down, it goes down for all.
- 32-player prep: the bot copies sent to a joiner now go up to 30 bots
  (was 20) and the tables for them hold 32 (were 24: would have
  overflowed at 32).
- BOTH PCs need this version: after it builds, run MAKE-FRIEND-UPDATE
  and send your buddy the update.
- Test plan: 1) a 24-player game with your buddy: both of you call UAVs,
  see/hear each other's plane, shoot it down. 2) then 32 players (F11)
  and send me BOTH logs: the game's own network code still has a few
  24-player spots I can only fix from what the two logs show.

## 0.5.274: see your hits on the plane

- Bullet hits: a small flash on the plane where the bullets land.
- Bazooka / Panzerschreck hits: an explosion on the plane.
- Shot down: it explodes and puffs smoke as it starts to fall, then
  blows up again (with smoke) where it hits the ground.
- (Includes 0.5.273: your engine sound + crash sounds.)

## 0.5.273: your engine pick + crash sound

- Engine: ARPL_P47_2 restarted every 5.45 s (your pick) is the default.
- Shot down: an explosion when the killing hit lands, and a crash
  explosion when it hits the ground 2.5 s later.

## 0.5.272: steady engine + flyby only when it passes

- One steady engine sound for the plane from the moment it arrives;
  the swoosh flyby only when the plane passes near you.
- If the engine stops after a few seconds, tune it live in the console:
    plane_engine ARPL_P47_1 5000   (start it again every 5 s)
    plane_engine ARPL_P47_2 0      (another engine, once)
  Tell me which sounds best and I'll make it the default.

## 0.5.271: Stuka for the Germans (try), cleaner plane sound

- 270: the plane sound works! The "lots of engines" were 6 different
  flyby recordings started 3 s apart, playing over each other. Now one
  flyby every 7 s.
- Stuka: the game only finds models from its campaign file when that
  file is loaded early, the way the very first UAV build did it (the one
  where you saw a plane off the map: that was the Stuka). Loaded early
  again; the German UAV uses the Stuka when the game finds it, else the
  Typhoon. Test: be American, streak_give enemyuav.

## 0.5.270: plane sound, the real fix (hopefully)

- Found why every plane sound was silent: the game only plays sounds
  through its list of "sound events", and multiplayer has no plane event,
  so the request was thrown away. The plane's flybys now ride on a rifle
  shot event with the sound swapped for the plane's (the same trick that
  makes the Lee-Enfield shot work).
- Test: streak_give enemyuav and listen. Also plane_model stuka_flying
  once (it logs which Crossroads models the game can find).

## 0.5.269: Stuka fix try + bot sound tracking

- The Stuka was never found by name, so the game kept the Typhoon on it.
  Its archive was last in a list that holds at most 32; it now goes
  earlier. Test: plane_model stuka_flying (you should see a Stuka now).
- Bot reload sounds: this build logs, for each reload sound, whose player
  it belongs to and how far away. Play a minute with 2 bots near and far.
- Plane sounds: every P-47 sound is silent even though the rifle shot
  plays the same way: their group in the campaign sound file is likely
  never loaded. Next on the list.

## 0.5.268: try any sound by name

- plane_sound name ARPL_P47_2b now   plays that sound right away (any name)
  Try: ARPL_P47_1b, ARPL_P47_2b, ARPL_P47_3b, ARPL_P47_4b, ARPL_P47_1

## 0.5.267: Stuka test command

- plane_model stuka_flying        puts the Stuka right in front of you
- plane_model stuka_flying clear  same, with one suspect flag forced off
- plane_model v_Typhoon           the Typhoon the same way, to compare
  Tell me which ones you can see.

## 0.5.266: plane visible for both teams; sound test works without bots

- The German Stuka never showed up (it flew its circle, invisible). Both
  teams now get the Typhoon, the plane you've already seen working.
- 265's sounds waited for a gunshot, so with no bots nothing played.
  Now they play anyway after a moment.
- Test (no bots needed): type  plane_sound shot me now
  You should hear a rifle shot. Yes or no tells us where the problem is.

## 0.5.265: plane sound, new approach

- 264 test: the whole-map circle and the height are good. None of the
  plane_sound commands made a sound, not even the flags' sound, so the
  problem was WHERE the sound was played from, not which sound.
- Now the plane's sounds are played from inside the game's own gun sound
  code, the same way our scoped Lee-Enfield shot is played (and you hear
  that one). Flybys play at your own player by default.
- Test: call a plane (streak_give enemyuav) during a match with bots
  shooting. Listen for flybys every 3 s. Check: plane_sound shot me
  (plays a rifle shot through the same path; if you hear that but no
  flybys, the flyby names are the problem, not the path).

## 0.5.264: UAV circles the whole map; height fixed; sound test command

- Whole map: the circle is now worked out from the map's spawn points
  (they cover everywhere play happens), not from where the players are.
- Height: it was set from the highest player, so with you in noclip it
  went higher each call. Now it's the players' average height + 1400.
- Sound: instead of another blind build, a console test so we find the
  answer in one session. Call a plane (streak_give enemyuav), then type:
    plane_sound flag origin   (a sound we KNOW you hear, the flags')
    plane_sound flag me
    plane_sound flag bot
    plane_sound flyby plane
    plane_sound flyby origin
    plane_sound flyby bot
    plane_sound engine bot
  Each plays once right away (stand near a bot for "bot"). Tell me which
  ones you heard. The plane's flybys then keep using the last choice.

## 0.5.263: fixes 262 (plane gone, bazooka silent)

- 0.5.262 broke two things, both mine:
  - The plane disappeared on Merville: my "bring it down on Poissons"
    rule was firing on every map and flew it down into the trees. Now the
    plane flies at its normal height everywhere, and lower only on Poissons.
  - Your bazooka went silent: the engine loop I added used a setting the
    game didn't expect, and it stopped other sounds. Taken out.
- Sound: I found exactly how the game plays the flags' flapping sound
  (which you hear). The plane's flyby sounds are now played that same
  way, from a point near you in the plane's direction.
- Rocket hit markers: the X shows when the rocket reaches the plane.

## 0.5.262: UAV engine sound (looped, like the flags), rocket hit markers

- 0.5.261: still silent; you shot it down with the panzerschreck.
- Sound: single sounds started by my code never reach your speakers.
  Looped sounds do (the flags flapping on the maps are played exactly
  that way). So the plane now has its P-47 engine as a loop, coming
  from the plane's direction near you, and getting quieter as it flies
  further away.
- Rocket hit markers: a rocket hit used to count the moment you fired
  (so the X flashed while you were still watching the rocket fly). Now
  it counts when the rocket would reach the plane, so the X shows then.
- Still to come: an explosion when a rocket hits and when it crashes.

## 0.5.261: UAV plane sound, try 3

- 0.5.260: you saw the plane on Poissons (fixed), still no sound.
- The flyby sounds now play the same way the game's sounds that you DO
  hear are played (the scoped Lee-Enfield's shot, for example): on your
  own player, whenever the plane is within reach. They won't come from
  the plane's direction yet, but you should hear them while it's near.
- Test: any map, streak_give enemyuav, listen while it circles. Then, if
  you have time, load Merville and then Poissons in the same session
  (the crash from before) and tell me if it still crashes.

## 0.5.260: UAV plane on Poissons, try 2

- 0.5.259 on Poissons (fresh start): no crash, the plane was made and
  flew its circle, but you never saw it. The game marked it as outside
  the map's world there (Poissons' sky is low: it's a town). Now, when
  that happens, the plane comes down a bit at a time until it's back
  inside, staying at least 300 above the highest player.
- Test again: fresh start, Poissons, streak_give enemyuav. Did you also
  hear the flybys this time (on any map)?

## 0.5.259: UAV plane sound you can hear; Poissons crash avoided

- 0.5.258 on Merville: circling, hit markers, damage all working.
- Sound: the flybys were playing, but on the plane itself, thousands of
  units away, too far to be heard. Now they play from a point near you,
  in the plane's direction, so you hear it from where it is in the sky.
- Poissons crashed (the graphics side ran out and hung) when you went
  there after Merville with the plane files loaded. The plane stays on
  Poissons for now; test: start the game fresh and go straight to
  Poissons. If that works, the problem is only memory left over from the
  map before, and I'll clear that on map change.

## 0.5.258: UAV hit markers, shooting it really works, flyby sounds

- 0.5.257 on Merville: perfect circle (thanks). But no shot ever counted
  (the game doesn't let bullets hit a plane made this way) and no sound.
- Shooting it: now every shot anyone fires is checked against the enemy
  plane's position: a shot that passes through it counts. About 15 rifle
  or MG hits brings it down; a bazooka/panzerschreck hit counts as 300 of
  its 600. Bots' shots count too.
- Hit markers: a white X flashes in the middle of your screen each time
  one of your shots hits the enemy UAV.
- Sound: the P-47 flyby sounds (from the game's own sounds) now play on
  the plane every 3 seconds while it's up.
- 5% slower again.
- Test: streak_give enemyuav, then shoot it.

## 0.5.257: shoot the UAV down; bigger circle, a bit slower, engine sound

- You confirmed the plane circles. Changes from your notes:
  - Bigger circle: it now goes round all the ground the players have
    covered on the map so far (not just where they are right now).
  - 5% slower.
  - Engine sound: the P-47's engine loop from the game's own sounds,
    attached to the plane so it comes and goes as it flies past.
- Shoot it down: the enemy's plane takes damage from bullets and
  explosions (rockets count extra). About 12 rifle hits, 20 MG hits, or
  one rocket plus a few bullets. Your own team can't hurt it. When it's
  shot down it dives into the ground and the UAV it was flying for ends
  ("YOUR UAV WAS SHOT DOWN" / "ENEMY UAV SHOT DOWN").
- Test: console streak_give enemyuav puts the OTHER side's plane up so
  you can shoot it (your own team's can't be hurt). streak_give uav then
  d-pad right is still your own.

## 0.5.256: plane check (no play change)

- 0.5.255 tested by you on Guns: the map loaded fine, the UAV ran, but
  you saw no plane. The log says the plane was made and told to fly, but
  not where it actually went. This build writes its real position every
  2 seconds so I can see what it does. Same test again please: console
  streak_give uav, d-pad right, look up for 30 seconds.

## 0.5.255: fixes the freeze loading a map; Allies' plane is now the Typhoon

- 0.5.254 froze while loading Merville. The two plane files plus the
  campaign guns' files were too much memory for the biggest maps: the
  last campaign guns file never finished loading. Two changes:
  - The Allies' plane is now the Hawker Typhoon. Its file is 2.6 MB;
    the P-47's was 13 MB (it came packed with tanks and guns).
  - The plane files load last, after the campaign guns, so the guns
    always get their memory first.
- Germans still get the Stuka. Everything else from 0.5.254 is in here
  (the plane moves now, circles over the players, right side).

## 0.5.254: the UAV plane actually flies over the fight now

- 0.5.253 tested by you: the plane showed up but sat far off the side of
  the map and never moved. Three mistakes of mine, all fixed:
  - I was using the game's "turn" command instead of its "move" command,
    so the plane only ever turned in place where it first appeared.
  - I read the players' positions from the wrong place (they all read
    as 0), so the circle's centre was wrong.
  - The sides were swapped: you're American and got the Stuka. Americans
    now get the P-47, Germans the Stuka.
- Same test as before: console streak_give uav, then d-pad right. It
  should fly in, circle above everyone for 30 seconds, then fly off.

## 0.5.253: the UAV is a real plane now (part 1: you can see it)

- Get 3 kills in a row, then press d-pad right to call the UAV, same as
  before: enemies show on your compass for 30 seconds. NEW: a plane now
  flies in, circles high over where everyone is fighting for those 30
  seconds, then flies off. Allies get the P-47 Thunderbolt and Germans get
  the Stuka, both taken from the campaign.
- To test it fast: open the console and type streak_give uav, then press
  d-pad right.
- Killstreaks now count all 32 players (they stopped at 24 before).
- Not yet: shooting the plane down (part 2, next build), and its engine
  sound.
- If a map won't load or the plane looks wrong (sideways, upside down,
  not there at all), set uav_plane = 0 in CoD3Recomp.ini to turn the
  plane off, and tell me what you saw.

## 0.5.252: no more freeze when you die; binocular check aimed better

- 0.5.251 tested by you: you see the ranks over everyone now. Done.
- The couple-of-seconds freeze when you died: that was our own leftover
  test tool. On your first three deaths each match it took 5 screenshots
  and wrote a whole frame's worth of drawing details into the log. The
  death-view problem it was hunting was fixed back in 0.5.236, so it is
  off now (it only runs if we turn it on for a test).
- Binocular sides: the check from 0.5.249 only looked at the first 700
  things drawn each frame, which are all the world. The binocular mask is
  drawn last, with the HUD. It now lists only the flat HUD/overlay draws,
  at both zooms, once per match. Nothing for you to do except tell me if
  the sides were see-through that match.
- 24-player play: no change.

## 0.5.251: status icons over heads for players past 24

- Second half of the "no icon over their head" problem with 32 players.
  The game has two kinds of over-the-head markers. 0.5.250 fixed the first
  (the friendly marker list). The second is the icon list itself: the rank
  or class icon over a teammate, the medic sign over someone who is down,
  and the mark over an enemy who has been spotted. That list only had room
  for 24 players, with other game data right after it, so players 25 to 32
  got nothing. Those 8 now get their own extra space, so the game fills and
  draws icons for all 32.
- Checked and NOT player limits (no change needed): about 15 other places
  that still say 24 or 25 turned out to be text compares (A to Z), sizes
  of 24-byte records, and a 5..24 state switch. Nothing to fix there.
- Next to look at: a vehicle-seat check that only finds the first 24
  players (82428010).
- 24-player play: no change.

## 0.5.250: rank icons over every teammate's head (32 players)

- You saw it: with 32 players, some teammates had no rank icon over their
  head, so they were hard to tell from enemies. The game's code that
  collects your teammates for those icons only looked at the first 24
  players. It now looks at all of them. The place the icons are kept
  already had room for 32, and the code that draws them already draws 32,
  so nothing else had to move.
- The same for the compass: the list of players it shows also stopped at
  24; now it takes all 32 (its room was already 32 too).
- 0.5.249 tested by you: the binoculars worked; the draw lists from that
  good run are kept to compare with a bad one if the sides go again.
- 24-player play: no change.

## 0.5.249: brightness slider; binocular sides looked at closer

- F11, Graphics: "Brightness", 50% to 150% (100% is the game's own). It
  works in multiplayer and the campaign alike, right away on Apply, and is
  saved as brightness in CoD3Recomp.ini. Blacks stay black and whites
  white; the middle tones come up or down. (Putting the slider into the
  game's own multiplayer options menu would mean changing the game's menu
  files, so it lives in F11.)
- 0.5.248 tested by you: the scoreboard showed 15 and 14 with 28 bots: the
  32-player scoreboard works.
- Binoculars: 248's change (the aiming blur drawn while they are up) did
  not fix the sides at the x2 zoom. This build writes, once at each zoom,
  a list of everything drawn in a frame (CoD3MP.log, "render: draw list")
  so the two can be compared and the missing black found.

## 0.5.248: the scoreboard shows the players past 24

- With 32 players on, the scoreboard (both the in-match one and the
  end-of-round one) used to look only at the first 24 players. Now it looks
  at all of them: in team games each side's list (up to 16) is complete,
  and the team totals count everyone.
- Free-for-all with more than 24 players: the list still has 24 rows (that
  is all the game's scoreboard can draw), so the last few joined players
  are left off it. Their scores still count.
- 24-player play: no change.
- Binoculars: you saw the sides go black, then not, with the HD textures
  off too, so the textures are not the cause. While the binoculars are at
  your eyes the game's aiming blur pass is now drawn (it is off by default
  and may be what paints the black round the lenses), and the log says
  what was left out each time you lower them ("render: binoculars down").
- BIG-TEST.bat / BIG-TEST-32.bat no longer open and close the scoreboard
  by themselves every 25 seconds (set COD3_BIG_SCOREBOARD=1 to bring it
  back). Hold Back yourself to check the scoreboard.
- Kill feed: "red killed red" is the rule you picked working: an enemy
  (red) killed one of your team (the one who dies is always red).

## 0.5.247: 32 players is a setting now (test)

- F11, Multiplayer: "32 players (test, restart the game)". With it on and
  the game restarted, a match takes up to 32 players: you and up to 28
  bots (debug.txt: bot_add 28). It is saved as max_players in
  CoD3Recomp.ini. BIG-TEST-32.bat still works as before.
- Still a test: the scoreboard lists only 24 players, and a System Link
  game between two PCs at 32 hasn't been tried (both PCs would need it on).
- The friend update (MAKE-FRIEND-UPDATE.bat) now also packs the co-op
  launchers (PLAY-COOP-HOST/JOIN.bat), COOP-README.txt and the newest
  co-op test findings zip, and installs them.

## 0.5.246: kill feed colours, take two

- 0.5.245 tested by you: smokes are back (the bots throw them). But the kill
  feed still showed green killed green.
- Why: 0.5.245 changed the colour the game uses in team games, but your
  match takes the game's other colouring, which paints a name green when
  it is "a player on this PC", and the bots count as being on your PC.
- Now the colours are set right where the kill line is shown: the one who
  dies is red, the killer is green if he's on your side, red if not.
- Audio sounding late in the Rimling test: still looking. The sound itself
  isn't dropping anything; the game ran slower (28-33 fps) with all the new
  smoke, which is the likely cause.

## 0.5.245: kill feed: whoever dies is red

- Your pick: in the kill feed the player who dies is always shown red
  (you included), and the killer keeps his team's colour (your team green,
  the other team red). Bots and players alike.
- Includes 0.5.244 (bot smoke grenades, safer effects memory), not yet
  tested.

## 0.5.244: bots throw smoke grenades again; safer effects memory

- Smoke grenades: 0.5.243's log showed why the bots never throw one. Each
  bot is given his side's smoke grenade, but with 0 of them in it. Their
  smokes (and extra frags, if any are missing) are now filled in when
  they spawn. The log says "rounds set to N".
- Rimling crashed on 0.5.243 while loading (Farm played fine). It crashed
  somewhere new, in the game's own network-message code, so it may be a
  rare one. To be safe, normal 24-player games now get only the one
  bigger effects pool that actually ran out, not all nine. 32-player games
  keep all nine (they were tested fine).
- Kill feed: 0.5.243's log shows the teams are read right (every bot on
  side 1 or 2, you on side 1), so the colours follow teams: your team
  green, the other team red. Tell me which way you want them (see below).
- Binoculars not fully black when aiming: noted, next.

## 0.5.243: bigger effects memory for normal games too; kill feed colours; bot smokes

- 0.5.242 tested (you ran both tests): the 32-player game (you + 28 bots)
  now plays the full time with no crash. The 24-player games on Rimling and
  Farm also played through.
- But the 24-player Rimling game ran the same effects memory dry 46 times
  (the thing that crashed the 32 game). It didn't crash this time, but it
  could. The bigger effects memory is now on for every game, not only 32.
- Kill feed: you said bots you kill aren't red any more. The log now says,
  for each kill line, which team each name is on, and if a dead bot's team
  reads as "none" at that moment, his real team is used for the line.
- Smoke grenades: the bots never threw one in any of the three tests.
  Every bot is given smokes, but the check that counts them always reads
  zero, so they think they have none. The log now shows exactly what that
  check sees (the first three times), so the next build can fix it.

## 0.5.242: 32-player test, round three: the effects memory and the damage table

- Tested 0.5.241 myself (Claude ran BIG-TEST-32 and BIG-TEST from your game
  folder). Normal 24-player games: both maps (Rimling, Farm) ran the full
  2.5 minutes with 20 bots, no crash, no warnings.
- The 32-player game (you plus 28 bots, Rimling) played 2 minutes and then
  crashed. The game's effects (smoke, sparks, dust) take their memory from
  nine pools of 512 blocks; with 29 players the 1 KB pool ran dry ("is
  empty" 22 times in the log) and an effect was built half-made. With 32
  players those pools now get twice the blocks (about 8 MB more).
- Also for 32: the game remembers who hurt each player in a table with room
  for 24. Damage from players 25-32 was being added onto another part of the
  hurt player's data. It now goes to a spare spot instead.
- Normal games are still 24 players; none of this changes them.
- Still to do for 32 as a setting: the scoreboard only lists 24 players, and
  a game between two PCs set to 32 hasn't been tried.

## 0.5.241: 32-player test, round two

- The first 32-player test (0.5.240) ran the full match on Rimling with you
  plus 28 bots, about 40 fps, no crash. Bots in the new slots spawned,
  fought, died and respawned.
- Fixed from its log: the game keeps a pool of 24 objects, one per player.
  The 25th player got none, and the game wrote into memory it shouldn't.
  With 32 players the pool now holds enough.
- Fixed: player names for the scoreboard, and a team count, were read from
  the session's old player list (stale since 0.5.213). This affects normal
  24-player games too.
- Normal games are still 24 players. The 32 test is BIG-TEST-32.bat.

## 0.5.240: first 32-player test build (normal games stay at 24)

- **Normal games are unchanged: still 24 players.** 32 players only switches
  on for the new test, BIG-TEST-32.bat.
- What the test from 0.5.239 showed: of the ~900 places in the game's code
  that use 24, about 350 run during a match. 189 of them were worked through
  by hand (the server's player list, the game's player list, the session's
  player list, the "nobody" markers, and the player numbers the session
  sends). Those now follow a setting: 24 normally, 32 in the test.
- **BIG-TEST-32.bat** (game folder) loads Rimling with 32 players allowed and
  adds 28 bots, then closes by itself. Claude runs it and reads the log.
  Expect crashes or odd things in this test at first; that is how the rest of
  the work gets found.
- Also found and fixed: four places where the game still read the session's
  old player list after 0.5.213 moved it (finding a player by his account,
  counting players, clearing a field, and a backwards walk). These run in
  normal 24-player games too.

## 0.5.239: big-game prep: the player-limit probe and BIG-TEST.bat

- No change to how the game plays. This build measures, so the next ones
  can raise the 24-player limit safely.
- The game now counts how often each of the 910 places in its code that
  use the number 24 (or 23 and 25) runs, and which numbers they see. It
  writes that to **cap-probe.csv** in the game folder every 30 seconds.
- **BIG-TEST.bat** (game folder) loads a map, joins, adds 20 bots, lets them
  play for 2.5 minutes, opens the scoreboard now and then, then closes the
  game. Results: big-test-<map>.log and cap-probe-<map>.csv.
- Also helpful: play one normal System Link match with bot_add 20 for a few
  minutes (the test above uses devmap, which skips the lobby). Afterwards
  leave cap-probe.csv in the game folder for Claude.

## 0.5.238: loading pictures for 7 DLC maps; second photo set for the rest

- Your picks are in as loading pictures: Rimling 6, Seine River 4
  (brightened), Aller Haut 7, Hilltops 9, Isle 9, Shipyard 5, Champs 5.
- PHOTO-TOUR-RESHOOT.bat takes a new set of 12 shots (other camera spots)
  for Gare Centrale, Stalag, Guns and Marseille.

## 0.5.237 test 6 (no new build): first co-op game between two PCs on 0.5.237

- **Working:** the opening film is shared, the other player's soldier
  appears, and crouching, jumping, moving, shots and grenades show up on
  both PCs.
- **Still wrong:** the joining player's soldier gets hurt by his own smoke
  grenades, a few grenades don't get thrown on the host's side, his soldier
  can vanish (the game then borrows Dixon), and after a mission restart the
  next mission doesn't start together (the host lands on the main menu).
- Findings sent to your friend: coop-test-6-for-friend.zip.

## 0.5.237: your friend's co-op v33 + v34 merged (grenades, sprint, fixes from test 5)

- **No more random shots and crouches** on the other player's soldier: a
  shot now only counts while the fire button is held (reloads, melee and
  weapon switches no longer fire it), and crouch/prone come from the game's
  real stance, not the camera height.
- **Jumps** no longer float the soldier up and down.
- **Grenades and smoke** thrown by either player now land on the other PC
  too.
- **Sprint works in the campaign** (hold or tap sprint while moving forward).
- **Friendly fire on the other player's soldier** is off for the host (a
  melee on it failed the mission in test 5).
- **Next mission / retry:** the joining copy now follows the host into the
  next mission instead of the session ending.
- Both PCs need 0.5.237. Tested by your friend on one PC, not yet between
  two. Includes the photo tour (0.5.236).

## 0.5.236: the photo tour shoots like a photographer

- **12 pictures a map** instead of 8: 4 from high over the map, 2 from roof
  height, and up to 6 at eye height, standing in a street and looking down
  it. The streets are picked from where the bots walk: the longest open
  lines through each map, spread around it, never two from the same corner,
  tilted a touch up so the buildings fill the picture.
- **Maps the bots haven't learnt yet** (Gare Centrale, Seine River, Stalag,
  Hilltops, Isle, Champs, Shipyard) get a learning run first: 16 bots run
  around the map for four minutes, then the game restarts with no bots and
  takes the pictures. So the tour takes longer on those maps (about 6
  minutes each).
- **Marseille loading picture:** your shot 7 is now also set for the second
  picture the loading screen uses (the Call of Duty 3 splash).
- PHOTO-TOUR.bat does all 11 maps; PHOTO-TOUR-REST.bat does all but
  Marseille.

## 0.5.235: your friend's co-op update (v32) merged; campaign options kept

- **Co-op: each player gets his own soldier.** Instead of borrowing one of
  the mission's riflemen (which the mission kept swapping out, 8 times a
  minute in test 4), each copy now spawns a fresh friendly soldier of its
  own for the other player.
- **Co-op: crouch and prone carry over**, and when the other player fires,
  his soldier fires too (real bullets on that side).
- **Co-op: finishing Forest no longer sends the joiner back to the start**
  (the end of the mission was taken for a restart). Gates and doors swing
  on both PCs.
- **Campaign: your options stay put.** Brightness, volume and the other
  Options are saved when you change them and put back the next time you
  start the campaign (they reset every launch before).
- Both PCs need 0.5.235 to play co-op together. Still not done: grenades,
  jump animation. Tested by your friend on one PC only, not yet between
  two PCs.
- Includes everything in 0.5.234 (death flash, Marseille loading picture).

## 0.5.234: death flash fixed; Marseille's loading picture; what really slows Marseille

- **Death flash (the sharp "split screen" glimpse):** found it. When you
  die, the game blurs the screen and lays two shifted copies of it over
  each other (its "double vision"). Our "Blur while aiming" setting is off,
  and switching it off also removed that blur, so the two copies were
  sharp: two of every star icon, two bodies. Now the blur is drawn while
  you're down, and blur while aiming stays off when you're alive. Die
  once and check: it should look soft and dreamy, not doubled and sharp.
- **Marseille loading picture (your pick, shot 7):** in
  textures\replace\maps\ordnance. Maps can now have their own loading
  picture (textures\replace\maps\<map>\), since the DLC maps all share one.
  The game also saves any 1024x512 picture with no replacement into
  textures\dump, so if Marseille's loading screen still shows the old
  picture, the right one is in there and I'll swap it.
- **Marseille's slowdown is not your graphics card.** The "gpu" figure
  counts the time from a frame's first drawing command to its last, waits
  included, so it's as long as the frame whenever the card is waiting on
  us. The real limit is our drawing thread: about 7 microseconds of its
  time per thing drawn. Quiet spots draw about 1,500 things a frame (55
  fps); the big views over the city draw 5,000 to 6,000 (25 fps). Turning
  off MSAA or console depth won't fix that. Next: one run of
  PROFILE-CPU.bat standing in a slow spot, to name the functions that take
  the time.
- The draw-by-draw graphics card timing from 0.5.228 is off again (it cost
  frame rate and was what made the card look guilty).

## 0.5.233: free photo mode (fly around yourself)

- **PHOTO-FREE.bat:** asks which map, loads it, spawns you with god mode and
  noclip so you can fly anywhere (move keys; jump goes up, crouch goes
  down). F10 hides the HUD and gun, F12 takes the picture. Quit when done.
- **PHOTO-TOUR-REST.bat:** the automatic tour for the 7 maps the first run
  didn't reach.
- The first tour run's pictures (Rimling, Marseille, Gare Centrale, Seine
  River: 8 each) are numbered contact sheets for you to pick from.

## 0.5.232: photo tour: shots from above, game closes itself

- Rimling's four pictures came out (no HUD, no gun) but stood at head
  height against walls and trees, one inside a house, and the game froze
  instead of closing, so the tour stopped there. Now 8 pictures a map from
  high above, around the map and looking into its middle, and the game is
  closed outright after the last one so the next map starts.

## 0.5.231: photo tour fixed (team menu, one map per run)

- The first try stopped at the team menu (a test map opens it like any
  game) and the next map change ended the game session ("game session no
  longer available"). Now the tour presses A for you through the team and
  class menus, and PHOTO-TOUR.bat starts the game once per map (load, spawn,
  4 pictures, quit, next map). About a minute and a half a map.
- The test maps opened as four-player split screen: the tour now takes the
  three extra screens out, so the pictures are full screen.

## 0.5.230: your friend's co-op v31 merged (shared films, smoother movement)

- **Films in co-op:** no more auto-skip (it left the joiner stuck in his
  film). The host presses A to skip, but only once the joiner has loaded
  too ("Co-op: waiting for your friend to finish loading..." until then);
  when the host's film ends, the joiner's ends with it.
- **Smoother soldiers and players** on the other PC: they're shown a tenth
  of a second behind and slid between positions, and soldiers no longer
  restart their walk every quarter second.
- **Firewall / Tailscale:** allow UDP ports 28960-28965 (two new ports).
- Both PCs need 0.5.230 for co-op. Includes the photo tour and F10 photo
  mode from 0.5.229.

## 0.5.229: photo tour for DLC loading pictures; F10 photo mode

- **Photo tour:** double-click PHOTO-TOUR.bat in the game folder and leave it.
  The game loads each of the 11 DLC maps by itself, hides the HUD and the
  gun, flies the camera to four spots a map (high up, looking into the
  middle of the map) and takes a picture at each, then quits. About a
  minute a map. The pictures land in the screenshots folder; you pick the
  good ones and I turn them into loading pictures.
- **F10 photo mode:** in any game, F10 hides the HUD and the gun (F10 again
  brings them back); F12 then takes a clean picture.
- Includes everything in 0.5.228.

## 0.5.228: three medics a team; Marseille is the graphics card; the death flash found

- **Medics work** (you were revived in 0.5.227's test). There were still only
  2 in 20 bots: each team hands out every kit in the list once before the
  weights count, so the weight did nothing. bot_kits.txt now has three medic
  lines (shotgun, Thompson, MP40): about 3 medics a team.
- **Marseille:** the new timing shows the graphics card itself is the limit
  there: 20 ms of card time a frame in quiet spots, 55-90 ms in the heavy
  ones (smoke, fire), on your RTX 2070. This build times the drawing step by
  step on the card (one frame in eight) so the next log names the exact
  effects that cost the most.
- **The death flash:** your screenshots show it. When you die from an
  explosion or a bot, the game freezes a picture of the moment you were hit
  and lays it over the death camera, fading it out ("double vision"). On the
  Xbox that picture is soft; here it shows up sharp, which looks like a
  split-screen glimpse. This build logs your first death's drawing so I can
  find that exact layer and blur it like the console does.
- Texture dumping was still on in your settings (left from the campaign
  menu pass): turned off.

## 0.5.227: your friend's co-op update (v30) merged

- **Co-op campaign:** your friend's newest co-op code is in. Players should
  move smoothly instead of jumping forward, stand-ins that a mission deletes
  are replaced, planes no longer drive along the ground on the joiner, and
  if the host's mission fails and restarts, the joiner restarts with him.
  The host launcher can also ask which mission to start once your friend
  connects (to skip Saint-Lo's grenade pickup, which still stops things).
- **Films in co-op:** a mission's film skips itself as soon as the level has
  loaded, so you both start together. Only in co-op; normal campaign is
  unchanged.
- **Both PCs need this build:** the co-op lobby message changed size, so an
  older copy and this one won't see each other.
- Everything from 0.5.226 (medics, Marseille timing, death screenshots) is
  included.

## 0.5.226: medics stand you up for real; Marseille: it's the drawing side

- **Medics:** last build's "send the revive" went nowhere: on the host the game
  sends that message to itself and never hears it. Now the medic runs the
  game's own revive script directly when he uses his kit at the body: full
  health, back on your feet, his revive counted. Log: "stands slot N up".
- **Medics coming to you:** every time you go down, each medic on your team
  writes in the log how far away he is and what he is busy with, so if none
  comes we know why. More bots pick the medic kit now (bot_kits.txt: medic
  weight 6, was 3; last match had only 2 medics in 20 bots).
- **Marseille:** the new timing lines show the bots cost about 2 ms and the
  game's server about 7.5 ms a frame. The other 15-30 ms is the client and
  drawing side, even with no bots at all (25-35 ms on that map alone). So the
  fix is in the graphics path, not the bots. This build turns on the graphics
  timing lines by default in multiplayer, so the next log shows which step
  of drawing eats the time.
- **The flash when you die:** for your first three deaths the game now takes
  five screenshots in the first half second (screenshots folder) and logs
  what the view is doing, so the "split-screen glimpse" can be pinned down.

## 0.5.225: medics revive directly; Marseille slowdown measured

- **Medics:** no bot revive has ever got through, because the game's own
  check (a short look from the medic's eyes at the body) never fired for a
  bot. Now, when a medic bot reaches a body and uses his kit, the revive is
  sent straight to the game the same way the game does after a successful
  check. Medics also only give up for an enemy close by (300, was 500):
  last match 6 of 7 trips were abandoned because deaths happen mid-fight.
  Log: "revive: slot N sends the revive for slot M directly", then "REVIVED".
- **Marseille (ordnance) slowdown:** your profiling run showed it isn't the
  graphics: the game's main thread is 97-100% busy the whole time on that
  map (graphics thread 77%). This build measures how much of that main-thread
  time is the bots ("perf: a game frame: bots X ms ...") so the next fix goes
  at the right part.

---

## 0.5.224: shotgun bots rush in; more smoke; medics keep the body in sight

- **Bot grenades work** (your 0.5.223 test) and the killfeed colours are fixed.
- **Shotgun bots:** a bot holding a shotgun now always plays aggressive
  (unless nearly dead): sprints in weaving from beyond ~450 units, closes to
  point-blank, never stops to snipe at range, and shoots instead of running
  in to melee.
- **More smoke:** besides when hurt or falling back, objective players and
  rushers now throw a smoke to cross under when they push forward with an
  enemy in sight 700 to 2500 away (at most every 16 to 26 s each).
- **Medics:** last match a medic reached YOUR body twice and lost it each
  time: when you're dead your player moves with your spectator view, so the
  "body" kept leaving. Medics now go to where you fell. They only go for
  bodies within 1000 units (longer trips always ended with the man already
  respawned), and dead bots wait up to 25 s for one.
- **Co-op test 2 ready:** your friend's new co-op code (v29) is in: stand-ins now
  work on any mission (Saint-Lo too), and each new mission starts fresh.
- **Marseille frame rate:** your log shows the game's graphics thread is
  busy 100% of the time on that map (about 4,500 draws a frame, 15-22 ms of
  work) - see the request below for a profiling run so I can fix the right
  thing.

---

## 0.5.223: co-op campaign experiment merged (Forest)

- **Your friend's co-op campaign test is in the build** (coop.cpp, from the
  coop-for-friend package). Two players in the campaign's Forest mission over
  Tailscale: the host plays as normal, the joining player's game follows the
  host's world (soldiers' places, health) and each side borrows one of the
  mission's American riflemen as the other player. Off unless started with
  the co-op launchers, so the normal campaign is unchanged.
- **To try it:** both of you need the same build. The host runs
  PLAY-COOP-HOST.bat (beside CoD3.exe), the other runs PLAY-COOP-JOIN.bat;
  each asks for the other's Tailscale address (Enter uses the System Link
  peer from F11). Allow CoD3.exe through the firewall (UDP 28962-28963).
  Both go to the main menu; the host picks Forest from chapter select and the
  other game follows. Logs: coop-host-play.log / coop-join-play.log.
- Rough edges (from its notes): only Forest; the stand-in soldiers have
  their own brains; story/objectives run on each PC separately. Never yet
  tested between two PCs. See COOP-README.txt in the game folder.

---

## 0.5.222: bot grenades, next try; revive trace

- **Bot grenades:** your 0.5.221 test showed the bots' throws now really run
  (the game starts the throw the way it does for you), but the game kept
  thinking the bot was holding his gun, so it had nothing to throw. When you
  throw, the game switches what you're holding to the grenade for the length
  of the throw. Bots now get the same switch at the start of their throw.
- **Rifle grenades:** you saw 3: that's 4 in total, one already loaded on the
  rifle plus 3 spare. Mines get 4 the same way (check when you can).
- **Medics:** a medic reached a body and tried 3 times, but the game's revive
  never even checked (it marks a miss when it does). This build logs exactly
  what he's holding and doing while he tries ("bots: revive trace"), so the
  next log tells us why.
- **Killfeed:** the log shows the fix working (10 bot-on-bot kills drawn as
  you see them). Tell me if the colours look right now.
- Two more campaign mission pictures in HD (17 of the 38 campaign menu
  pictures done; the rest a few per build).

---

## 0.5.221: campaign at 144 fps in its menus too; more campaign pictures in HD

- **Campaign frame rate:** campaign levels already ran at 144 (your last
  campaign log shows 144 fps in a level, held there by frame_cap = 144 in the
  F11 menu; set it higher or off for more). Its menus stayed at the console's
  rate. Now, as in multiplayer, only films (intros, briefings, a moving menu
  background) keep the console's speed, so they play at the right speed;
  menus and levels run as fast as your PC and the cap allow.
- **Campaign pictures upscaled (4x):** the whole start menu (background, logo,
  frames, font, button icons, lock), the three sepia war photos, and the
  mission pictures and their thumbnails as they finish (all in
  textures\replace; no rebuild needed for those).

---

## 0.5.220: killfeed colours; bot throws not cancelled; 4 mines/rifle grenades given; more medics

- **Killfeed:** when bots killed each other both names showed green. The
  game colours each name by comparing that player's team with "you", but a
  bot's kill happens while the game is briefly set up as that bot, so "you"
  pointed at the wrong player and everyone looked like a teammate. The
  killfeed line is now drawn as you see it: enemies red, teammates green.
- **Bot grenades and smokes:** your test showed a bot's throw now really
  starts, then gets cancelled a moment later: the bot was also pressing
  crouch and sprint during the throw (you press nothing else). Now for about
  two seconds after a throw starts the bot doesn't crouch, sprint or fire,
  so the grenade gets out.
- **4 mines and 4 rifle grenades (this time for real):** 0.5.219 changed the
  wrong number. Mines and rifle grenades are handed out by their own part of
  the game (1, 2 or 3 by rank); that now gives 4, and 4 is also the most an
  ammo crate fills you back to. Log: "class: mines special set to 4".
- **Medics:** there was only ONE medic bot in a 20-bot match, and he reached a
  body once but never used his kit (he kept waiting to face it exactly). He
  now uses it after 2 seconds at the body, and medics are 3x more common in
  bot kits (bot_kits.txt, in your game folder).

---

## 0.5.219: bot grenades for real; 4 mines and 4 rifle grenades; bots feel for walls

- **Why bot grenades never landed (from your last match):** when you throw,
  the very first instant of the throw also tells the game WHICH grenade
  (its weapon number), then goes back to your gun. Frags and smokes both use
  the same throw button. The bots pressed the button while still naming
  their gun, so nothing happened. Now a bot's throw names the grenade
  (frag or smoke from his kit) for the first moments, uses the same button
  you do, and doesn't sprint while throwing. Log: "throws a grenade ...
  weapon N" and "frag button found" / "smoke button found".
- **4 mines and 4 rifle grenades:** every mine and rifle grenade now holds 4
  in total (what's loaded plus spares), for everyone including bots. You start
  with them, and an ammo crate fills you back up to 4 once you've used them.
  The log lists each one ("weapons: ... mine ... max").
- **Bots on the flag crouch less:** holding a flag with nobody around they
  crouch a third of the time and stand and shuffle about the rest; fighting
  on the flag they crouch half the time and strafe when up.
- **Bots feel ahead for walls:** while moving, each bot checks a little way
  ahead (and to both sides) several times a second. If a wall is coming he
  turns along it toward the more open side instead of walking into it; if
  he's boxed in he backs off. If he keeps meeting the same wall (3 times in
  12 s) he remembers that spot and plans a different way round from then on.
  Log: "bots: feelers".
- **Medics:** last match they set off to revive 11 times but gave up 8 times
  as soon as any enemy was in view. Now only an enemy close by (500) stops
  them, and never once they're at the body.
- Includes 0.5.218 (room for more players in the game's player list).

---

## 0.5.218: more players, step: room in the game's player list

- **What it is:** the game keeps one small list of the players in a match:
  exactly 24 places, with the map's own object stored right after them.
  Players 25 to 32 would have overwritten that map object. This build makes
  the list bigger (room for 32) and moves the map object to the end, in all
  42 places the game uses it.
- **What you'll notice:** nothing. Still 24 players and 20 bots. This only
  makes room. While we stay at 24, the map object is kept in both its old and
  new place, so anything we missed still works.
- **The log checks it:** "player limit: entity list ... +100 X +132 X spare
  slots clean". Both numbers should match and the spare slots stay clean.
- Also in this build (no rebuild needed for pictures): the campaign start
  menu pictures as they finish upscaling.

---

## 0.5.217: campaign start menu in HD; dead bots shout for a medic

- **Campaign start menu upscaled (4x).** From your menu pass
  (textures\used-campaign.txt: 38 menu pictures the campaign shows; the rest
  were movie frames): the start menu background, the CALL OF DUTY 3 logo,
  the menu frames, the controller button icons, the menu text font and the
  lock icon. They go straight into textures\replace (game folder only,
  never GitHub). No rebuild needed for these: just start the campaign.
- **Still to do, a few per build:** the mission pictures (17 big ones and
  their small thumbnails) and the three sepia photos.
- **Dead bots shout "Medic!"** A second and a half after a bot falls he calls
  for a medic, the same call the game makes when a player picks "call medic"
  on his death screen (the voice line and whatever the game shows with it).
  Medic bots already go to the bodies (0.5.216).
- texture saving is off again in CoD3Recomp.ini.

---

## 0.5.216: medic bots revive; the log works again

- **CoD3MP.log was empty in 0.5.215.** Hiding the console window broke where
  the log text went: everything printed went nowhere, so your last match
  left a 0-byte log (that match's grenade results are lost). Fixed: the log
  is written again with no console window.
- **Medic bots revive teammates.** Found how the game's own revive works: a
  medic takes out his medic kit (mp_revive), stands over a dead teammate's
  body (within about 100 units, looking at it) and fires it. Medic bots now
  do exactly that when no enemy is close: they pick the nearest dead
  teammate (you or a bot) within reach that no other medic is going for,
  run to the body, look down at it and use the kit, up to three tries.
  The log says "bots: revive: slot N (medic) goes to revive slot M" and
  "REVIVED" when it works.
- **Dead bots wait for a medic.** A dead bot normally comes back after 4
  seconds. If a medic is on his way to him, he now waits for him (up to 20
  seconds) instead ("calls for a medic" in the log). The game's own "Medic!"
  shout and the icon over the body are not done for bots yet.
- **You can be revived by bots.** When you die, stay down (don't pick a class
  and respawn straight away) and a medic bot nearby should come for you.
- **Campaign pictures:** texture saving is switched on in CoD3Recomp.ini for
  the campaign menu pass (see below). Turn it off again afterwards, or tell
  me and I will.

---

## 0.5.215: bots find the real grenade buttons; no console window; campaign menu list

- **No more black cmd window.** CoD3MP.exe now opens only the game window.
  Everything still goes to CoD3MP.log as before. If you ever want the live
  console back, start it with `--console` (or set COD3_CONSOLE=1). First-time
  setup (no game folder yet) still opens one, because it asks questions.
- **Bot grenades and smokes.** The log proved the bots were pressing buttons
  that do nothing: they "threw" but never spent a grenade. Your own presses
  in the last log were different buttons. Now each throw tries the next
  likely button and checks whether the bot's grenade count actually went
  down; the first one that works is kept for every bot. The log says
  "bots: frag button found" / "bots: smoke button found". A bot with no
  grenades left no longer pretends to throw.
- **Fastest way to settle it:** in a match, throw one frag and one smoke
  yourself. When your own count drops, the game logs which key you pressed
  ("bots: host spent a frag ... keys pressed") and hands that key straight to
  the bots.
- **More players:** found that the game's entity list keeps exactly 24 player
  slots, with the level's own object right after them (slot 25), used in 41
  places. Going past 24 players has to move that or give the extra players
  slots elsewhere. Written down for the next step; the limit is still 24
  players / 20 bots.
- **Campaign menu pictures:** the campaign now keeps its own list of the
  pictures it shows (textures\used-campaign.txt), so multiplayer no longer
  wipes it. Next: browse the campaign menus once, send that file, and the
  missing ones get upscaled a couple at a time.

---

## 0.5.214: prepare the join path for extra player slots

- Replace the old six-at-a-time client search with a bounded search that can safely handle a future 32-entry server.
- Add explicit session-roster mapping for all 32 reserved records. Runtime addressing stays at 24 until player limits and sentinels are activated together.
- Player and bot limits remain 24 and 20. This is preparation for added players; 32-player gameplay is not enabled yet.
- Build and runtime status: docs/build-logs/validation-214.txt.

---

## 0.5.213: move the session roster into 32-entry storage

- The game now has a separate 32-entry block ready for its session-player
  records. It copies the current 24 entries and checks that the eight spare
  records stay untouched, with a guard to catch overruns.
- The playable limit remains 24. This update prepares the session layer for
  16v16; it does not yet let players or bots use the additional slots.
- The change can be rolled back for comparison with
  `COD3_NO_SESSION_RELOCATION=1`. The user validated the storage checks in one solo run through a match and map change. The active cap stays 24; expanded-slot behavior and two-PC capacity validation remain outstanding.

---

## 0.5.212: remove forced renderer dump from scoped ADS

- Lee-Enfield ADS tracing still records each aim transition and scope settings,
  but no longer forces a full renderer-frame dump as soon as you aim. That
  diagnostic dump could add a hitch exactly when testing the scope.
- The user confirmed scoped ADS works normally again on 0.5.212. This build preserves those weapon values while continuing player-storage work.

## 0.5.211: cache campaign world-model bounds

- The per-frame weapon-model fit no longer walks every vertex of each saved
  campaign mesh just to rebuild its unchanged bounds. It computes and caches
  those bounds when the mesh is captured, then reuses them while fitting.
- This targets CPU work that could contribute to frame drops. It does not
  change the Lee-Enfield's first-person scope overlay; the black-screen issue
  still needs a targeted in-game repro and renderer trace.

---

## 0.5.210: reserve and watch eight more server-client records

- The game now allocates room for 32 server-client records and clears the
  full block. The active limit stays 24; no new player or bot slot can use the
  extra space yet. The log marks slots 24â€“31 and reports any writes to them.
- This is a bounds-check step toward 16v16. Session players, player-number
  sentinels, game/UI loops and other limits still need to be expanded before
  the lobby can accept more than 24.
- Includes 0.5.209's joiner-side mirror spawn fix and corrected throw trace.

---

## 0.5.209: joiner-side bot spawn path

- Fixed the likely reason the other player saw host bots stay dead on his
  screen. His game had joined each bot into the session, so the normal spawn
  route treated the bot as owned by the host and sent placement over the
  network. The joiner now places recognized copies locally and runs the
  game's spawn finalizer in a scoped spare local-client context, restoring the
  joiner's view state afterwards.
- The attached match log had no bot grenade/smoke decisions or throw traces,
  so it cannot confirm why their animations are missing. The throw trace now
  records the final command weapon after selection, making the next test log
  useful when a bot actually attempts a throw.
- The 24-player cap is unchanged. The relocated 32-record storage passed its
  checks in the previous match; expanding server clients, sessions and UI is
  still required before raising the cap.

---

## 0.5.208: larger player storage; merged world-model fitting; scope investigation

- First step toward 16v16: server player records now live in a dedicated
  32-record allocation. The playable limit is still 24: server clients,
  sessions, player-number sentinels and UI must be expanded in later builds.
  Logs check relocated pointers, the unused eight records and an end guard.
  COD3_NO_PLAYER_RELOCATION=1 restores the original storage for comparison.
- Merged the Team Native 0.5.206 package's world-model fitting, MG42
  third-person correction and optional bot pose tools, preserving 0.5.206's
  crate fix and 0.5.207's mirror lives, role balance and throw tracing.
- Investigating intermittent black screens when ADS with Lee-Enfield Scoped.
  Added bounded ADS-transition and render tracing; no confirmed fix yet.
- Real-match and System Link validation pending. Both PCs need the same EXE.

---

## 0.5.207: Your friend sees the bots die; balanced bot roles

- **Bot deaths for whoever joins (untested).** Your friend saw bots freeze,
  then vanish, instead of dying. The 0.5.206 logs showed why: on his
  machine, his copy of you is alive (and his game plays your deaths
  itself), but his copies of your bots were never spawned there. With no
  live copy, there was nothing to kill and no death animation to play.
  - Your game now tells the other machine, four times a second, whether each
    bot is alive, which life it's on, and how it last died (killer, cause,
    weapon, direction, hit location).
  - His game spawns its copy of each bot the game's normal way when that bot
    starts a new life. When your game says the bot died, his copy dies
    through the game's own death code, with your killer, cause and
    direction, so the death animation plays.
  - His log says it with lines like "mirror number N spawned here" and
    "mirror number N dies here as on the host". If this causes trouble,
    starting his game with COD3_NO_MIRROR_LIFE=1 turns it off.
- **Bot roles are balanced per side.** Roles were picked by a random roll
  that came out lopsided when bots were added together (last match: 7
  campers, 8 flankers, 4 objective, 1 rusher out of 20). Each new bot now
  takes the role its side is shortest of: about 25% objective, 25% rusher,
  28% flanker, 22% camper. That's 3, 2, 3, 2 with 10 per side.
- **Bot grenades and smokes: not working, being traced.** You were right:
  the bots decide to throw ("throws a grenade" in the log), but nothing
  comes out. Your Fuel Plant match had no grenade or smoke throw at all, and
  the Merville one about 6 throws for 71 tries. They do carry grenades. This
  build logs what happens during the first 12 bot throws and during your own
  ("throw trace" lines). **Throw a frag and a smoke yourself** in the next
  match, so the log has a working throw to compare against.
- Both PCs need 0.5.207 to play together: send your friend the new update
  zip. Keep native_commands = 0.

---

## 0.5.206: Bots stop walking into crates; where the 24 players live

- **0.5.205 played well:** no crash, no effects shortage (the most effects
  at once was 329 of 8,192). The bots played War's flag as takers and screens,
  and spread out more (the nearest teammate was usually 350-620 units away,
  was 150-470).
- **Fixed: bots stuck against crates and pipes (Fuel Plant).** 0.5.205's hill
  climbing also counted flat-topped crates and pipes 20 to 60 units high as
  "climbable". Bots walked into them and stayed there: 272 "stuck" lines in
  6 minutes, one bot stuck 8+ times in the same spot. Now the climb has to
  match the tilt of the ground: a slope can rise as steeply as it really
  slopes, but a flat top only allows the normal step (18 units). Hills still
  work up to 45 degrees.
- **More players: found where the 24 players are.** The game keeps them in
  one block of 24 records of 2,992 bytes each (at 0x82BF21D0). There is no
  room after it, so it has to be moved to a bigger block. The good news:
  only about 6 functions point at it directly; the rest go through each
  player. That makes moving it much smaller than the study feared. That's the
  next player-limit build.
- Keep native_commands = 0. Both PCs need the same build to play together.

---

## 0.5.205: Effects crash fixed, bots climb hills, smarter War flag play

- **The crash after a while with 22 bots is fixed (untested).** The host ran
  out of room for effects: muzzle flashes, bullet impacts, smoke and
  explosions. The game keeps a fixed number of them (4,096). With 22 bots
  firing (MG42s especially), that filled up from about 6 minutes in. From then
  on, the effects code damaged memory until the game went down. Your
  friend's game never ran out.
  - The game now gets twice the room: 8,192 effects, twice the effect parts
    and twice the effects memory. That's about 5 MB more.
  - The "pool:" line in CoD3MP.log now also shows how many effects are in use
    and the most at once. If those keep climbing all match, something is
    leaking, and the next log will show it.
- **Bots walk up and down hills.** The check for "can I step there" tested
  the ground flat, so any slope over about 8 degrees uphill (13 degrees down)
  counted as a wall, and the bot stood still or turned back. Now, when the
  flat test fails, the bot tries the step in short pieces up the slope, up to
  45 degrees, as steep as a player can walk. Walls and cliffs still stop it,
  and flat ground costs no extra work.
  - This should also help the clumping: on Merville most bots were stuck in
    the low middle of the map, because the hills around it were "walls".
- **Smarter War flag play.**
  - About 40% of each side now plays the flag in play (3 to 7 bots). The
    objective-role bots go first, then rushers and flankers; campers keep
    their posts. Before, it was at most 4 bots per side.
  - Half of them are takers: they go onto the flag itself, where the capture
    counts (before, they stood in a ring 120 to 360 units away and seldom
    took it). Takers also keep pushing when the enemy is not close.
  - The other half screen: they hold 200 to 450 units out, on the side the
    enemy comes from, each at their own angle.
- **Your friend not seeing enemy death animations: diagnosis build.** His
  log shows his game never learns that one of your bots died. On his
  machine, every bot's health stays at 100 all match. Your game does record
  how each bot died, the same way it does for you. This build logs, on his
  machine, whether that death information arrives for each bot, and for
  you as a comparison. Send both logs after a match and I can fix the right
  side.
- **More players: a correction.** The study's plan to grow the players'
  records into the free space after them was wrong. That array is the four
  local players' (split screen), not the 24 server players'. That's why the
  "free" space was in use. This build logs where the 24 players' records
  really are, so the next step can be planned on facts.
- Keep native_commands = 0. Both PCs need 0.5.205 to play together.

---

## 0.5.204: Stability fix: the freeze after adding bots

- **The crash is fixed.** It wasn't a real crash: the game froze for good (FPS 0)
  about 15 seconds after bot_add 24, while a bot was respawning.
  - What happened: the game keeps one shared store of small memory blocks.
    Most of its small allocations come from it, including the "endon" and
    "wait" records of the game scripts every player runs. With 24 bots, plus
    the campaign weapons, sounds and maps that are now loaded beside the
    game's own, that store ran out.
  - The original game never checks for this. It wrote a record into memory it
    didn't have, which broke a list, and then went round that list forever.
  - Now, when the store is full, the block comes from the game's bigger
    general memory instead. The game itself has a setting for exactly this,
    but it was switched off for this store. As a second guard, if one of those
    lists ever does break, it's noted in the log and skipped instead of
    freezing.
- **New "pool:" lines in CoD3MP.log** every 30 seconds show how full each size
  of that store is. Extra lines appear the moment it runs out. That tells us
  how close a full lobby gets and what fills it, which matters for more
  players.
- **About the frame drops:** in your log, the slow part (11-13 FPS, once down
  to 3) came right after the map loaded, from about 30 s to 115 s, before you
  added any bots. Later the same view ran at 50-60 FPS. The time was going
  into the PC side of drawing, not the game's own code. The cause isn't
  pinned down yet. If it happens again, tell me what you were doing (menus,
  class editor, F11 menu, just spawned) and send the log.
- Nothing else changed: bots, guns and your friend's weapons are as in
  0.5.203. Keep native_commands = 0.

---

## 0.5.203: Your friend's new guns: MG42, scoped Lee-Enfield, sticky bomb (merged)

- **From your friend's team, their second 0.5.900 package**, merged on top of
  0.5.202 (all of 0.5.202's bot work is in):
  - **MG42**, a new main weapon: the MG42's real gunfire and fire rate, its
    model in your hands and on the ground, aims from the shoulder.
  - **Scoped Lee-Enfield** ("LEE-ENFIELD (SCOPED)"): its own scope, sounds
    and reload; works the bolt out of the scope like the scoped Kar98.
  - **Sticky bomb** (the British No. 74) as a custom class grenade.
  - **The MG34 now aims from the shoulder too** (no forced prone bipod).
    Set mg34_no_deploy = 0 in CoD3Recomp.ini for the original.
  - The G43's reload lined up with its sound (3.45 s).
  - The game loads one more campaign level's weapons (Laison's, for the
    scoped Lee-Enfield), so its map memory is a little bigger (495 MB, was
    480 MB). Keep native_commands = 0.
- **Bots can carry the new guns:** bot_kits.txt gets "heavy mg42" and
  "scout lee_enfield_sniper" lines (added to your file). The scoped
  Lee-Enfield counts as a sniper rifle: slow scoped walk, less sway, and bots
  with it take overwatch.
- Their notes, as they wrote them:

- Sticky bomb (British No. 74) as a custom class grenade: kind 4, "STICKY
  BOMB (BRITISH)" in the class editor. It is multiplayer's own: item 30,
  weapon_stickygrenade, WEAPON_STICKYGRENADE in weapusge.cod (noBounce: it
  sticks; 3 s fuse; 900 damage in a 147 radius in MP; US grenade throw
  animations), loaded on every map but given to no class. Tested: spawned
  with two in the grenade slot (Eder Dam). Throwing/sticking UNTESTED by hand.
- MG42 (primary 20 / sidearm 22, appended): the campaign has it only
  mounted and in AI soldiers' hands (w_ge_MG42_nobipod; no first-person
  model or arm animations), so it is a copy of the MG34 with the MG42 model
  others see in your hands and on the ground, the MG34 in your own view,
  the MG34's icon, the MG42's real gunfire, the same sounds the game's own
  multiplayer MG42 (the Horch turret) plays: Forest's MG42_FIRE_1ST_LASTSHOT
  (2.58 s, first person) and MG42_FIRE_3RD_LASTSHOT (3.06 s, others), which
  match MG42_HORCH_FIRE_1ST/_3RD byte for byte in size and length; on maps
  whose bank has the Horch cues and no campaign banks, those; else the
  MG34's (Forest's MG42_FIRE_1ST_LOOP is the mounted gun's loop, never
  stopped when played a shot at a time) and its fire rate, 50 ms a shot (the MG34's 70). Its reload and bipod
  sounds are the MG34's. The AI model in your own view was tried: it sits
  above the view with only a hand, so not used. Tested: made as weapon 45, its model found,
  spawned with it. User confirmed firing and sound with MG42_FIRE_3RD; the _LASTSHOT cues found in the auto-test log, UNTESTED by ear.
  Stats checked against the game's MG42 data (no handheld MG42 file exists):
  the turrets' 50 ms (MP Horch) fire time is used; damage and range stay
  the MG34's (same 7.92 mm round; the turrets' 50/110 damage is for mounted guns).
- MG42 first-person body: the MG34 view model's gun body is swapped, only
  while the MG42 is in hand, for a copy of the AI MG42's body (mesh
  "mg42.w_ge_MG42_nobipod"), turned a quarter about the up axis, scaled 0.40,
  lined up on the MG34 body's barrel end and moved 3 back and 0.4 across, which
  puts its sights where the MG34's were and its receiver against the MG34 drum
  (sub_82446AB8 finds the meshes; MpFixesMg42View changes the drawn mesh's
  buffers in place each frame). The MG34's bipod (base and legs) is not drawn
  while the MG42 is held (its counts and index size set to none). The drum,
  charging handle and all animations stay. COD3_MG42_BODY="scale x y z rear"
  tunes the fit. Fit chosen from side-by-side screenshots against the MG34.
- MG42 aiming: the MG34's class (3) made aiming a bipod deploy (forced prone,
  "Prone blocked", wall mounting); the MG42 takes the BAR's class (0) and its
  deploy animations are left out, so it aims from the shoulder. While aiming
  (player state +172, the sights' fraction, over 0.5) a second MG42 body is
  drawn, moved (-0.2, +6, -0.7) so its front sight sits on the sight line and
  the receiver is out of the eye (it clipped into the view). No wall mounting.
- Scoped Lee-Enfield ("LEE-ENFIELD (SCOPED)", primary 21 / sidearm 23): Laison's
  WEAPON_LEE_ENFIELD_SNIPER as a copy of the scoped Kar98: the Lee-Enfield view
  model with Laison's scope model (attach1Model lee_enfield_scope, tag
  w_lee_enfield), the brRFL moves incl. reloadSCOPE, its icon and 77 of its file's
  numbers (10 rounds, zoom 15, 1.4/0.7/1.0 s reload parts; damage the Kar98's).
  Its shot (a sniper's: event 13) is played as the Kar98's shot event with the
  cue Lee_Enfield_Sniper_FIRE_1ST (laison.wbk) and SniperRingoff; its bolt is
  GR_Enfield_rechamber. Laison's weapons archive and bank are loaded too; the
  map block is 495 MB (480 hung the load with four campaign archives).
  Its reload now plays (session 4, see below).
- Session 4 (2026-10-17, built and auto-tested; hand test pending):
  - MG42 aiming view: the MG42's rear sight is now where the MG34's is. Both
    bodies were written out as meshes (diagnostic COD3_MESH_DUMP=folder) and
    the sights measured: the MG34's rear notch at y -6.57, z 3.75 of its body
    (front post y -22.5); the MG42's notch at x -18.55, z 4.76 of the AI
    model (front post x 2.4, tip 4.69). The old aiming fit (-0.2, +6, -0.7)
    had put the MG42's rear sight about 10.7 units behind the MG34's, so the
    camera sat in front of it ("halfway down the barrel"). New fit: scale
    0.40, x -0.22, y -4.72, z -0.97, then tipped 0.5 degrees muzzle up and
    turned -0.7 degrees about that notch (new BodyFit pitch/yaw, the 6th and
    7th numbers of COD3_MG42_ADS_BODY / _CYCLE) so the front post sits in the
    notch on the screen's centre (screenshots, COD3_MG42_ADS_CYCLE).
  - MG42 raise/lower: the aiming body is now made again step by step from the
    hip fit to the sights' fit as the sights' fraction (+172, smooth 0..1)
    moves, and dipped up to 1.0 lower half way (COD3_MG42_DIP). Before, one
    jump at 0.5 let the gun slide through the view, riding high. (A third
    body buffer could not be allocated, so the aiming body's own is reused.)
  - MG34 aims from the shoulder too (new ini `mg34_no_deploy`, default 1): in
    the sub_82486BF8 hook the stock mg34's class (+176) becomes the BAR's and
    its lmgDeployedAnim/DeployAnim/BreakdownAnim (+148/+152/+156) are
    emptied, as for the MG42. No forced prone / bipod deploy / wall mount.
    Set `mg34_no_deploy = 0` for the stock behaviour.
  - Scoped Lee-Enfield: endless rounds fixed. Its campaign fireDelayMP 0.1 s
    (+1520) made every shot skip taking a round from the clip in
    multiplayer, so it never emptied and never reloaded (that was the
    "silent reload"). Now 0, the scoped Kar98's. With rounds counting down
    the reload plays its sounds through the scoped Kar98's events (16 start,
    14 each round, 17 end) and CampaignReloadCue: GR_Enfield_Scope_start,
    _reload x10, _end (laison.wbk), seen in the auto-test log. Its clip is
    now the Lee-Enfield's (`lee_enfield`, 10 rounds) instead of one of its own.
  - Scoped Lee-Enfield rechamberWhileAds 0 (campaign 1): it drops out of the
    scope to work the bolt each shot, as the scoped Kar98 does (with 1 the
    bolt was worked behind the scope, seen only after the first shot).
  - Scoped Lee-Enfield double shot: with fireDelay 0 its shots also send
    the scoped Kar98's own gunshot events (weapon_kar98_sniper_weapon_fire_*),
    which played SPRINGFIELD_FIRE_1ST + SniperRingoff 0.1 s before its own
    Lee_Enfield_Sniper_FIRE_1ST (found in COD3_SOUND_TIMES). Those events
    are now silent for a lee_enfield_sniper shooter (sub_82554BA0 hook), as
    is the Kar98 ring-off its own shot event carried (COD3_ENFIELD_RINGOFF=1
    restores that one). Auto-test: no SPRINGFIELD/SniperRingoff cue left.
  - Both Lee-Enfields: the right hand clipped into the butt at spawn and
    after every reload, but not after a shot (the rack's last pose). The
    campaign's British idle (w_arms_brRFL_idle_a) raises the right hand into
    view on multiplayer's arms. Both now keep the Kar98's idle
    (w_arms_geRFL_idle_a: weapon fields +72/+76 and the animation entry,
    KeepsCopyIdle); the sights' idle stays British. Screenshots at spawn,
    after a shot and after a reload now match, hand out of view.
    COD3_ENFIELD_BRITISH_IDLE=1 restores the British idle.
  - Screenshots taken in the same second no longer overwrite each other
    (overlay.cpp: -2, -3, ... added to the name).
  - Unused, off by default: a Lee-Enfield stock bend (COD3_ENFIELD_BEND /
    _CYCLE) written while looking for the clipping; not needed.
  - Diagnostics (env only): COD3_MESH_DUMP, COD3_RELOAD_TRACE (sub_824B13A0
    reload sound kinds), COD3_WEAPON_DIFF="a b", COD3_MG42_BLEND_SHOTS,
    COD3_PS_WATCH=2 (hex), COD3_ANIM_ITEMS=1 (animation entry records). The weapons log lists each campaign gun's clip and
    ammunition numbers.
- G43 reload 3.45 s (campaign 3.95): its one reload sound, GR_G43, starts with
  the reload (mag out 0.4 s, mag seated 2.4 s, bolt 3.0 s), while in
  multiplayer the animation's last part ends with the reload time; at 3.95 the
  bolt was pulled ~0.5 s after its sound. Lined up from timed screenshots.
- Diagnostics (env only): COD3_SOUND_TIMES=1 logs every sound and cue with ms
  times; COD3_RELOAD_SHOTS=1 screenshots through a reload; COD3_SHOTS="s s".

## 0.5.202: Bots aim down their sights, leave spawn, push, check the flag, and snipe from overwatch

- **Bots aim down their sights now.** They never did, and that's why no
  rockets flew in 0.5.201: a bazooka or Panzerschreck only fires while
  aimed. Launchers and sniper rifles always aim. Other guns aim past 700
  units, unless the bot is charging in or pushing hard. The shot waits until
  the sights are up.
- **Bots learn where each team spawns**, from where players appear after
  dying.
  - **Leaving spawn:** for their first 15 seconds a bot heads out along a
    learned route to a spot at least 500 units from his own spawn, before
    anything else.
  - **Pushing:** half the rushers push all the way up to the enemy's spawn
    area and wait there a few seconds in ambush.
  - **Checking the flag:** a quarter of the bots, every other errand, go and
    look at the War flag and watch it a few seconds.
  - **Defenders and attackers:** campers and bots that keep dying favor spots
    on their own half; rushers and bots on a streak favor the enemy's half.
- **Snipers watch from overwatch.** A bot with a scoped rifle takes high
  ground, lies down scoped in, and watches the busiest ways (where players
  walk most) in turn for 35 to 60 seconds. Then he moves up toward the enemy
  and does it again.
- **More players:** Phase 1 has started. Every place in the game's code that
  has to change for more than 24 players is now listed: 1,107 places in 421
  functions (docs/player-limit/sites.csv, explained in
  PLAYER-LIMIT-STUDY.md). The next builds patch them a group at a time,
  starting with a safety-check build.
- 0.5.201 on Rouen (map known): about 49% of bots within 300 of a teammate
  (was 56% on Rouen in 0.5.199), 7 to 15 bots on errands at a time.

## 0.5.201: No more rocket volleys, bots stop shooting their teammates

- **Bazookas and Panzerschrecks fire one rocket at a time.** Bots fire by
  tapping the trigger every other frame, which rifles need. With a launcher
  that looked like a volley of rockets. Now a bot presses once, then waits
  2.5 seconds. He also won't fire it at anyone closer than 300 units (his
  own blast), or when a teammate is within 300 units of the target.
- **Bots hold fire when a teammate is in the way.** Before every shot a bot
  checks whether a teammate stands in his line of fire (a little wider the
  farther the shot, for the gun's spread). If one does, he doesn't shoot. He
  steps aside to clear the line, then shoots.
- **Grenades** aren't thrown when a teammate is within about 300 units of
  where they would land.
- The log says when it happens ("holds fire: teammate N in the way",
  "fires his launcher ... (one shot)").
- 0.5.200 on Ederdam, a map the bots had never seen: fewer bots bunched
  (48% within 300 of a teammate on average, down to 25%; was 56%), and
  high and low ground now picked (22 of 36 errands; was 3 of 37).

## 0.5.200: Bots spread out over the map

- What your Rouen test showed: 56% of the time a bot was within 300 units
  of a teammate. Three reasons:
  - **11 of 20 bots had the objective role** and all ran to the one War flag.
  - **Everyone else went for the nearest enemy**, so they all arrived from
    the same direction. An errand (outskirts, high or low ground) was dropped
    for any enemy within 900 to 1700 units, which with 20 bots is almost
    always (only 37 errands all match).
  - **High and low ground were almost never picked** on a map as flat as
    Rouen (3 of 37 errands).
- Now:
  - Only a quarter of bots get the objective role (was 40%), and at most 4
    per side play the War flag. They spread round it 120 to 360 units out,
    instead of piling onto it.
  - Bots on an errand keep to it unless an enemy is in sight or within 550.
  - An enemy already chased by two teammates counts as farther away, so the
    others go after someone else.
  - Closing in on an enemy they can't see, bots take their own lane up to
    520 units to either side, so a group arrives from several directions.
  - Teammates keep about twice the distance from each other when they're not
    fighting.
  - High and low ground are picked from the top and bottom 15% of the map's
    heights, at least 32 units above or below the middle, so even flat maps
    have some.
- The log now measures it every 30 seconds ("bots: spread: ... nearest
  teammate median ..., N within 300, N on errands"), so we can see if it
  helped.
- Map learning: Rouen now has 528 places learned, on top of Merville and
  mp_guns.

## 0.5.199: The Gewehr 43 and campaign gun sounds (merged); stuck-bot clock fix; vehicles; Support and Medic packs

- **Fixed a bug that broke stuck detection in 0.5.198.** While a bot walked
  to a vehicle, the code that notices stuck bots was reading two different
  clocks. It thought bots were stuck twice every frame (30,847 log lines on
  mp_guns) and kept restarting their unstick attempts, so a really stuck bot
  never escalated to jumping, dropping down or wandering off. One clock again.
- **Bots try harder to get in vehicles.** In 0.5.198 they reached the
  motorcycles and pressed use 40 times but never got on. Now they also press
  the console's "use" (X), walk right up to it, and look at it. The log says
  each try ("presses use ... link"). If they still can't get on, the log
  tells us what to try next.
- **Support and Medic bots drop their packs (first try).** A Support bot
  drops his ammo pack, and a Medic his health pack, when two or more teammates
  are close and no enemy is in sight. Then he goes back to his gun. At most
  once a minute; the game's own recharge timer still decides whether the pack
  is ready. The log says each step ("bots: pack ...").
- **German bots get German smoke grenades** (smokegrenade_axis, the Germans'
  own).
- Smoke throws are logged more fully (the log had stopped counting them).
- Map learning: mp_guns learned 687 places in your test, added to Merville's
  1,054.

- **The Gewehr 43 is in (from your friend's team, their 0.5.900).** The
  campaign's G43 with its own models, animations and real campaign weapon
  numbers, in Create-a-Class (main weapon or second weapon). And **every
  campaign gun now plays its own campaign gunshots and reloads** (Sten, Bren,
  FG42, FG42 Scoped, Lee-Enfield, G43). Their full notes are below. Keep
  native_commands = 0 in CoD3Recomp.ini.
- Bots can carry the G43 too: bot_kits.txt has a "rifleman gewehr43" line
  (added to your existing file; new files get it by default).

### Merged into 0.5.199: Gewehr 43 in multiplayer (Team Friend's 0.5.900, on the 0.5.193 package)

- New campaign gun, the Gewehr 43 (Forest's), made like the Lee-Enfield,
  Sten, Bren and FG42: a copy of the M1 Garand with the G43's own view and
  world models (w_g43_VM, g43_world), HUD icon and `geg43` animations.
  Appended after the Mills bomb, so every existing weapon keeps its number;
  custom class primary 19 / sidearm 21 (older saved choices unchanged).
  Forest's `sp\forest\weapons.cod` is campaign archive 2 (big_memory only),
  `forest.wbk` its sound bank (Fuel Plant's slot null).
- Its own numbers: `G43Fields[]` in mp_fixes.cpp sets 93 fields from the
  campaign G43's weapon file (read from memory in single player Forest)
  wherever it differs from the Garand's, the file's MP value where it has
  one, at the offsets of the game's own field table (0x825D77D0): 10-round
  magazine, 120 reserve, fireTime 0.1 s and fireDelay 0.1 s, reload 3.95 s
  (top-up 2.5 s), partial reloads allowed (noPartialReload 0), adsZoomFov
  65, melee 150, drop/raise 0.4 s, its recoil, sway, spread and view
  offsets. Damage stays the Garand's (84, 53%, 1000/3000) on purpose: the
  campaign file's MP damage (100, to 8000/10000) was never multiplayer's
  balance (its Garand says 50 where multiplayer's is 84).
- Every campaign gun plays its own campaign sounds now: the G43
  (GEWEHR43_FIRE_1ST/3RD, GR_G43 from forest.wbk), and the Sten, Bren,
  FG42, FG42 Scoped and Lee-Enfield (STEN_FIRE, BREN_FIRE, fg42_FIRE,
  fg42sniper_FIRE, LEE_ENFIELD_FIRE; GR_STEN, GR_BREN, GR_fg42,
  GR_enfield_reload, GR_Enfield_rechamber from mace2.wbk). Before, none of
  them did in multiplayer (the Bren and FG42 both played the BAR):
  - The campaign banks were only registered with the bank manager
    (sub_824964E8), never loaded into the sound engine. Now they are, as
    the map's is: sub_824AE270(manager [0x82BCBF7C], key, 1).
  - The engine's bank memory (start-up configuration +24/+28,
    sub_821297E8) was 0x880000 from the map block: too small for
    forest.wbk (8.8 MB) and mace2.wbk (13.8 MB), so the loader let them go.
    Taking 36 MB more of the map block hung the map load. Now the engine
    is given its own 36 MB of physical memory (Guest::AllocatePhysical);
    the map block stays 480 MB.
  - Shot events are named weapon_<copy>_weapon_<what> in this game
    (weapon_bar_weapon_fire_1st); only bare <copy>... names were matched,
    so no campaign gun's shot was ever swapped. Both forms now
    (CampaignDonorEvent).
  - The engine searches slots in order and the campaign banks sit before
    the map's, so multiplayer's own guns would have found the campaign
    versions of shared names (THOMPSON_FIRE_1ST is in forest.wbk). The
    look-up now searches every other slot first, the campaign banks last
    (OrderedCueLookup).
  - Reloads: each copy's reload cue becomes the gun's own
    (CampaignReloadCue), and the campaign animations' gr_* names play the
    bank's own cues instead of the copies'.
  - A campaign cue still not found falls back to the copy's, never silence.
  All only with campaign_guns and campaign_sounds on.
- `campaign_animation.h`: the G43's own ADS and fire animations for the
  Garand moves it has none of. `campaign_sound.h`: the Garand's events used
  for it, its clip ping/ejection muted, its reload cue GR_M1G -> GR_G43.
- New ini `g43_reload` (seconds, 0 = 3.95) to tune without a build.
- IMPORTANT: `native_commands = 1` (F11 "Native command lists
  (experimental)") loses multiplayer world geometry; keep it 0.
- Tested by the user in an offline match: models, map, firing, sound,
  reloads (partial too) and handling "perfect". UNTESTED: System Link
  between two PCs (both need this exe), bots with the G43 (not given to
  them).

## 0.5.198: Bot kits you can customize, many more grenades, fixes from the 0.5.197 test

- **Bot kits are yours to set.** A new file, bot_kits.txt, sits beside
  CoD3MP.exe. The game writes it the first time with good defaults. Each line
  is one kit:
  - how often it's used
  - the class it builds on (support, light, rifleman, medic, heavy,
    antiarmor, scout)
  - the main gun: the class's own, any multiplayer gun, or the campaign guns
    (lee_enfield, bren, sten, fg42, fg42_scoped)
  - how many frag and smoke grenades
  New bots take whichever kit their team has least of. Save the file and the
  next bots you add use it, no restart needed.
- **Way more grenades.** Every bot now gets frags and smokes by default.
  Classes whose kit has no grenades are given them when they spawn (for
  example, riflemen had no frags and most classes had no smoke). Bots throw:
  - frags about twice as often, and also at enemies bunched together or who
    just ducked behind cover
  - smoke when they're hurt or falling back, sooner than before
  - smoke ahead of themselves when they run for the flag or charge in from
    range
- **Fixed: every gun walked slowly while aiming in 0.5.197.** The scope check
  matched every weapon, so all guns aimed-walked at the sniper speed and
  swayed half as much. Now it's only the scoped rifles (snipers and the
  scoped FG42).
- **Fixed: bots never got into vehicles.** A bot picked a vehicle and gave up
  on it in the same moment (78 times in your log). They now walk over and
  press use.
- **Fixed: a bot standing above an enemy he can't reach.** One bot stood five
  minutes on a floor right above his enemy, looking straight down. Now an
  enemy close by but a floor away, and out of sight for 6 seconds, is left
  alone for 20 seconds.
- **Last resort for stuck bots:** a bot that hasn't gotten anywhere for 70
  seconds goes back to spawn.
- **Bot errands use learned routes.** In 0.5.197 most errands walked straight
  at far spots and half were given up. Bots now pick spots a learned route
  reaches, or something close by. Merville already has over 1,000 learned
  places saved from your test.
- **More players study:** the probe showed the free-looking memory after the
  player data is in use. More players means moving that data too, so it's a
  bigger job (see PLAYER-LIMIT-STUDY.md).

## 0.5.197: Bots learn the maps, use vehicles and mounted guns; HD textures; sprint and sniper feel

- **Bots learn each map as you play.** Every half second the game notes
  where everyone walks (you and the bots), how people get from one spot to
  the next (stairs, ladders, ramps, drops), and where people die and get
  kills. The bots plan routes over the places players have really walked. It's
  saved per map (bots\maps\<map>.txt beside CoD3MP.exe), so they get better
  every match. The first match on a map they still roam the old way while
  they learn; after a few minutes they start using it.
- **Bots spread over the whole map.** When no enemy is close or in sight,
  each bot has his own kind of spot:
  - some walk the outskirts,
  - some climb to high ground and watch from there (lying down or crouched),
  - some go down into low ground (trenches, gullies) and wait crouched,
  - the rest hunt as before. Campers take the high ground.
  They pick spots away from where other bots are going, and avoid spots where
  players keep dying. A bot who dies three times on his way to his kind of
  spot tries a different kind.
- **Bots adapt to how they're doing.** A bot on a streak plays bolder; one
  who keeps dying plays it safe from cover.
- **The frozen bot is fixed.** In your last test one bot stood 3.5 minutes
  in one spot. He'd reloaded next to a mounted machine gun (reload is also
  the "use" button) and got stuck on it. Now a bot on a mounted gun fights
  from it while an enemy is in reach, and gets off when nobody is. If that
  fails he's sent back to spawn. Bots remember where mounted guns are:
  they don't reload there by accident, and campers sometimes man them on
  purpose.
- **Bots in vehicles (first try).** Up to two vehicles a team get a bot
  crew: a driver and a gunner. They walk to a free vehicle and get in. The
  driver heads for the War flag or the map's outskirts and backs up when he
  gets stuck; the gunner and riders shoot from their seats. A bot will also
  hop in beside you when you're driving nearby. They get out after two
  minutes or if the vehicle is stuck. The log says every step ("bots:
  vehicle ..."), because steering and getting in are untested: please send
  the log. F11: "Bots use vehicles and mounted guns" turns this off.
- **Sprint isn't unlimited any more.** Run out of breath and you have to walk
  for 1.5 seconds before you can sprint again. When you stop sprinting
  yourself, your breath starts coming back after 0.75 s and is full in
  3 s. (0.5.42 had refilled it almost straight away.)
- **Sniper scopes:** walking with a scope up is slower (30% of normal; other
  guns keep their aiming walk speed), and the scope sways half as much. Both
  are in F11: "Scoped walk speed" and "Scoped sway". Hold breath still
  steadies it further.
- **HD textures for the world.** The texture swapper from 0.5.70 (menus) now
  has a tool for whole maps: UPSCALE-TEXTURES.bat upscales a map's
  textures 4x on your graphics card (bushes, trees, walls, vehicles, guns).
  See HD-TEXTURES-README.txt. They live on your graphics card, not in the
  game's memory, so they need no extra game memory. The game no longer
  stutters while it saves textures (they're written in the background), and
  each map now gets its own list of the textures it uses.
- **The Kangaroo for your friend:** the Kangaroo comes from your
  vehicles.txt, which the friend update never included, so his game showed
  the map's normal vehicle there. MAKE-FRIEND-UPDATE now packs it and the
  installer puts it in place. Big HD texture folders stay out of the update
  zip (use MAKE-TEXTURE-PACK.bat for those).
- **More players study, first probe:** the log now says whether the free
  memory after the 24 players' data stays unused during a match ("player-limit
  probe"), and where the lobby's player list lives. Nothing changes in play.
- Bots' kits are logged once a life ("bots: kit ..."), to find how the
  Support's ammo packs and the Medic's health packs are used, so bots can
  hand them out next.
- Found: the "model detail" setting has never done anything. The game never
  reads r_lodscale/r_lodbias (leftovers from the Quake 3 engine). The real
  detail levels are per model and SpeedTree's; that's later work.

## 0.5.196: Bots get unstuck, spread out, use ladders and drop off ledges

- What you saw on 0.5.195: bots stuck up high, near ladders they never used,
  and bunched up in front of obstacles. Your log showed why. A stuck bot
  jiggles back and forth, so the old check (did he move a bit this second?)
  never called him stuck. One bot stood in one spot for 20 seconds like that.
- Stuck now means "hasn't gotten anywhere in 3.5 seconds", jiggling or not.
  Each time it happens he tries harder: first another way round (and he
  remembers that spot), then a jump, a drop off a ledge, or a ladder, then
  a short wander in a fresh direction to break free.
- Spreading out: teammates closer than about two metres push each other
  aside. Bots going for a flag each pick their own spot around it instead of
  all running to the same point.
- Ladders: bots now look for ladders all around them, out to about 4 metres
  and at two heights (before: straight ahead, about 2 metres). They look when
  their target is above or below them, or when they're stuck. On a ladder they
  now climb DOWN when what they want is below them.
- High ground: bots can step off ledges up to about 4.5 metres when their
  target is below. That's under fall-damage height. They never step into a
  pit or walk through a wall.
- To test (solo is fine): same as before, and watch the spots where bots got
  stuck. Log lines: "stuck (no progress ...): tries ...", "approaches native
  ladder", "drops down toward a lower goal".

## 0.5.195: Smarter bots: prone, cover, smoke, melee charges, flanking, and no more jump loops

- What you saw on 0.5.191: some bots fail a jump, then keep trying the
  same jump instead of finding another way.
- Now a bot remembers where he got stuck. That's a jump that left him in
  the same spot, or walking straight into something for about 3 seconds.
  He won't try that same move again for 45 seconds, and longer if he keeps
  failing there. He turns and takes a different way round instead. He
  remembers up to 6 spots at a time, keeps them when he dies and respawns,
  and forgets them on a new map.
- To test (solo is fine): host a match, add bots, and watch the ones that
  used to get stuck. They should give up on a bad jump after one try and
  head another way. The log says "jump at ... failed (strike 1 ...): going
  round" and "stuck going to ... rerouting" when it happens.
- Prone: bots now actually go prone. They go flat to shoot at long range,
  when they're hurt, when they're playing defensive, and in cover when badly
  hurt. They stand back up to move or jump.
- Cover: besides seeking cover when hurt or reloading, they sometimes take
  cover in a long gunfight too (rushers don't). In cover they pop up to shoot
  and duck back down every 1.5 seconds, or lie flat if badly hurt. They throw
  smoke to cover the run when they're at half health or less.
- Falling back: badly hurt (35 or less) with you close, a bot turns to face
  you, throws smoke between you, then sprints away for about 3 seconds.
- Melee charges: close in (under about 6 metres) with an empty gun, a big
  health lead, or an aggressive mood, a bot sprints straight at you to melee.
- Aggressive or defensive: each fight, a bot picks a style from his role and
  his health versus yours. Aggressive bots close in and strafe hard. Defensive
  bots keep their distance, stay low and go prone.
- Flanking: flankers (and some objective bots with no flag to play) go round
  the side to come at you from an angle instead of running straight at you.
- Tracking: bots now fight whoever they can actually see, not the nearest
  enemy behind a wall. When you duck out of sight they aim at the spot (and
  height) where they last saw you for 3 seconds, so they look up stairs and
  down drops instead of staring through the floor.
- Log lines to look for: "goes prone / crouched / standing", "charges in to
  melee", "falls back behind smoke", "flanks round to", "throws smoke".
- Also in this build: 0.5.194's host-only bot list (still needs a test with
  your friend).

## 0.5.194: The host's bots for everyone in the match

- What you two found: bots showed up only on the PC that ran bot_add. Your
  friend's log and yours explain it. Each PC keeps its own list of the players
  in the match, and a PC only shows a player who is on that list. Bots were
  only ever on the host's list, so the other PC had nowhere to put them.
- Now the host's game sends everyone a bot list (name, team, class) every
  second. A joined player's game adds the host's bots to its own list the same
  way the game adds a real player who joins. Nobody else types anything, and
  the bots still run only on the host.
- bot_add now works only for the host. A joined player who types it gets
  "only the host can add bots" in the log, instead of ghost bots that only
  he could see.
- To test: your friend (or you) hosts and adds bots. The other one joins and
  does NOT add bots. Check whether the joined player sees the bots, whether
  they move, and whether they show on the scoreboard. Then send both
  CoD3MP.log files (from both PCs, before launching the game again).
- Not done yet: a bot removed on the host may stay on the joined player's
  screen until the next map. The bot jump fix is next after this.

## 0.5.193: Varied native/custom bot kits and primary weapon selection

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

## 0.5.192: Bot loadout prototype - built, not installed

- Prepared11 balanced loadout templates: seven native kits plus existing
  FG42, Sten, Bren and Lee-Enfield through the native custom weapon-name path.
- MP/SP builds and loadout policy tests PASS. Not installed: live191 memory
  inspection exposed unreliable inventory ordering (most firstslot rifle,
  second pistol; some reversed). Prototype assumed secondslot primary.
  Kit callback can also receive another class despite saved record fields.
- Superseded by193 before user testing. Fix both inventory-order assumptions
  and class enforcement in the native kit callback. Preserve191 movement.

## 0.5.191: Use native rounded-capsule navigation traces

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

## 0.5.190: Keep bots moving through tight spawn areas and doorways

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

## 0.5.189: Isolate each bot's native network-send timers

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

## 0.5.188: Re-arm bot weapon drops after each completed respawn

- A fresh kill in 187 produced no drop attempt: asynchronous respawn could
  leave the previous life's drop flag set. Reset it when the bot is observed
  alive again, preserving one native drop attempt per corpse.
- Log fresh pickup handle, position, model, rig and expiry for inspection.
  Native item/model/network handling is unchanged.
- Lifecycle tests PASS across eight asynchronous respawns; existing capture,
  duplicate and ownership tests PASS. Source preservation checks and MP/SP
  builds PASS; all four installed artifact hashes match, VERSION 188.
- Startup and live visible/pickable drops remain pending. Initial 187 native
  creation succeeded, but that alone does not prove rendered pickup models.

## 0.5.187: Run the native weapon-drop path for bot deaths

- User186 reports bots can kill them while prone; dropped guns still absent.
  Native drop requires a local-player place; bots deliberately occupy none.
  Their custom respawn also skips the normal network receiver's drop call.
- Capture the held owned kit weapon before native death clears it; queue one
  native drop on the dead frame, before respawn. Temporarily satisfy only
  that bot's local-place check within that drop call, using its isolated
  context. Keep native ammo/eligibility, item creation and network handling.
- Log whether native pickup creation returns an entity, separating missing
  creation from invisible models. Failed creation restores temporary slot
  order; repeated death callbacks cannot duplicate drops in the same life.
- Lifecycle tests PASS; sourceguards PASS aim/navigation/crashguard unchanged.
  MP/SP builds PASS; installed with no gameprocess, all4artifact hashes match,
  VERSION187. Startup PASS: visible MP menu144FPS. Live dropped-weapon
  creation/visibility still pending.
- UAV markers, idle behavior, remote visibility and renderer list's original
  corrupting writer remain open. No claim these are fixed in187.

## 0.5.186: Correct bot aiming above/below the target

- Native firing math uses forward.z=-sin(pitch), while the bot used the
  opposite pitch sign. Live185 sample confirmed a higher target needed
  -2.88 degrees but the bot used+2.89. Correct pitch for both command and
  shot; grenade loft follows the native upward sign too.
- Normalize wrapped viewpitch before learning spawn offsets. Preserve
  valid crouched/prone muzzle height instead of raising all bots to60.
- Tests PASS: nativevector alignment across elevations/yaw,16bit wrapping,
  stance eyeheight/fallback and grenade loft. Source guards PASS: navigation,
  team assignment, shot scheduling and185crash guard unchanged. MP/SP builds
  PASS; installed after gameclose; all4artifact hashes match, VERSION186.
  Startup PASS: visible MP menu144FPS. Live vertical aiming unconfirmed.
- USER185: held guns visible and bots killable. Snapshot logged11 death
  calls (including host),4 prevented corrupt-list copies, no queuewatch write.
  Root list corruption remains under investigation. Missing dropped guns,
  bot UAV markers, idle behavior and paired-client visibility remain open.

## 0.5.185: Guard the renderer-list clear implicated in bot death crashes

- Both183 and184 logs captured the same writer clearing a critical pointer
  after bot death: native820D3638 from82436FC0.184 crash evidence archived.
- For the exact temporary renderer list, discard old elements before the
  native negative-capacity clear branch allocates0 bytes and copies them.
  Retain native allocation/free cleanup; other lists and normal clears run
  unchanged. Log original metadata and reset completion for live diagnosis.
- Regression PASS: reproduced zero-byte-copy failure; guard prevents it while
  preserving cleanup, nearby canaries and normal/empty/other-list behavior.
  Source guard PASS: all184 navigation/team/weapon/death/input hooks unchanged.
  MP/SP builds PASS; installed with no processrunning, all4artifact hashes
  match and VERSION185. Startup PASS: visible MP menu144FPS.
  Live bot-kill test pending.
- Crash fix remains unconfirmed in-game; reason for negative metadata still
  under investigation. Prone aiming and remote visibility remain unresolved.

## 0.5.184: Persistent obstacle detours, supported jumps and native ladders

- Moving roaming bots now use obstacle routing too. They keep a chosen
  waypoint until reached/blocked, remember recent positions, and search
  alternate routes using bounded body sweeps and floor-support checks.
- Try low obstacles with a clearance/landing-tested native jump. Failed
  attempts back off for8 seconds and seek another route; blind/random hops
  removed. No teleporting through geometry.
- Nearby native ladder surfaces get a supported approach; once attached,
  native forward input climbs. Combat aiming no longer turns the bot away
  while climbing. Failed progress triggers cooldown and alternate routing.
- Helper tests PASS: wall detour, query cap, enclosed/no-floor rejection,
  low jump, tall obstacle/roof/gap rejection and ladder-only detection.
  Source guards PASS: all four roaming branches routed; death/render/input
  isolation hooks unchanged. Final MP/SP builds PASS; all4 installed
  EXE/PDB hashes match; VERSION184. Startup PASS: visible MP menu144FPS.
  Live obstacle/jump/ladder behavior remains unconfirmed.
- User183 confirms visible held guns. Remote teammate/dropped gun visibility
  remain unconfirmed. Reported3v1 bot split is expected with a solo host
  (total3v2). Also reports prone-target aiming still shoots overhead.
- Combat crash reproduced183; watch captured the actual clearing writer
  (820D3638 from82436FC0) after bot death. Evidence archived locally.
  Crash and aiming are NOT fixed by184. Navigation remains experimental;
  local collision routing is not a complete map navigation mesh.

## 0.5.183: Balance new bots across both teams

- User requests bots alongside either human, visible from either team;
  chose automatic balancing. Old code forced all bots opposite the host
  during setup, session creation, every Think correction and respawn.
- Each new bot now joins the smaller team, counting connected humans and
  assigned bots. Ties choose opposite host. Bot retains its own side across
  respawns and host team changes; newly created slots get a fresh choice.
  Existing bots are not shuffled when humans change teams or disconnect.
- Tests PASS: solo hosts either side, friends together/opposed, 20 additions.
  All six setup/session/think/respawn call sites checked; death/render hooks
  unchanged. MP/SP builds PASS (existing kernel printf warning).
- Installed after user closed182; no process verified, all4EXE/PDB SHA256
  matched; VERSION183. Startup/live team assignment and paired
  client visibility pending. This fixes assignment; remote visibility is
  still unresolved. Weapon fix182 also awaits live visual confirmation.

## 0.5.182: Recover an owned gun when a bot's kit changes

- Latest181 runtime: many empty-handed bots request weapons17/18/29 despite
  inventory00801300 (owns8/9/12/23; kit slots12/8). The remembered lastHeld
  weapon survives class/respawn changes; native selection rejects that stale
  unowned request.8280 matching empty/unowned command observations in log;
  count is repeated observations, not distinct bots or independent trials.
- Only when held AND pending weapons are0: retain remembered weapon if still
  owned; otherwise choose owned primary/secondary from current kit, or0 when
  kit notready. Submit normal usercmd selection; no forced model/weapon writes.
  Existing ammo switching, teleport and death/watch/event routines unchanged.
- Actual helper tests PASS stale IDs fromlog, valid cached gun, secondary/no-kit,
  invalid IDs and changedkit. Source checks confirm death/watch sections intact.
  MP/SP builds PASS; installed with no gameprocess, all4EXE/PDB SHA256match,
  VERSION182. Logsarchived. Startup PASS: visible multiplayer menu at144FPS.
  Live held/dropped visibility still unconfirmed.
- User confirms latest save/return works perfectly (181 uses heldX2sec).
  New network report: friend could not see bots on his team; joining hostteam
  made them visible. User saw friend/bot scores on kills. Exact friendroster
  status unconfirmed. Track team-dependent remote bodyvisibility separately
  from weapons/scoring. No friendpackage produced: user updated it themselves.

## 0.5.181: Hold X to go back to your saved point, from anywhere

- **Justin:** holding R3 kept saying "RETURN POINT BLOCKED" (or rejected the
  hold); he wants it to take him back to wherever he saved, from anywhere,
  even on odd geometry.
- **Hold X (Xbox controller) for 2 seconds** to go back (was R3; Justin
  asked for X). Works while moving, aiming down sights or on any ground;
  it needs only that you're alive, in play, not in a menu and not in a
  vehicle. X still reloads/uses as normal on a press; only a 2-second hold
  returns you, and "RETURNING" shows once it has been held half a second.
- **Never blocked:** the game's check for clear floor at the saved point is
  now only used to fine-tune the landing. If it finds none, you're put on
  the exact spot you saved, which is where you were standing.
- **Unchanged:** saving with L3 (hold 1 second) still needs you standing
  still, so sprinting isn't affected. R3 is plain melee again. Holding X
  also starts a reload if your clip isn't full.
- **Test:** save with L3 somewhere odd (a slope, a ledge), walk off, hold X
  for 2 seconds while moving and while standing; tap X to reload as usual.

## 0.5.180: Two-second R3 return hold

- User confirms179 teleport worked perfectly; requests R3hold2seconds.
- R3 return now requires2000ms continuously held while stationary/eligible.
  L3 save remains1000ms. Save/return notices reflect2seconds.
- Actual timer tests PASS: R3no action at1000/1999ms, action at2000ms,
  release/cancel/no-repeat and clockwrap; L3 still1000ms. MP/SP builds PASS.
  Bot/death/watch/UAV logic unchanged. Other botissues remain unresolved.
- Installed with no gameprocess; all4 EXE/PDB hashes match, VERSION180.
  Startup180 PASS: visible menu leftopen. Live2sec test pending.

## 0.5.179: Hold R3 one second; save the current position directly

- Later178 log DOES show one savedpoint and three native returncalls. Earlier
  no-success snapshot was incomplete. Userperception still pending; original
  failure cause not established.179 retains requestedR3hold/feedback changes.

- User: unsure L3 saved; R3 did not return. Requests holding R3 one second
  and saving wherever standing.178 log had no tactical-point success lines.
- Both L3(save/replace) and R3(return) now use separate one-second hold timers,
  fire once until release, cancel on movement/menu/ADS/death. Save captures
  current finite player coordinates directly; no save-time landing trace.
  Return retains the full-box grounded/clearance trace and native origin update.
- Replace volatile render-triangle eligibility with native local-state2;
  allow stationary crouched/prone positions and tolerate normal vertical
  gravity samples. Host live health/state/pause/ADS/velocity/local-state checks
  were valid. Raw hold detection retained even when gated, enabling rejection
  logging. Notices show saving, saved, returning and missing-point status.
- User latest: bot weapon models absent both in hands AND after weapon drops.
  Unresolved; bot combat/death/watch code unchanged. Previous statement that
  bots were untested is superseded by this visual report; crash still unconfirmed.
- MP/SP builds PASS; actual routing/timer tests PASS for independent1sec holds,
  release/cancel/no-repeat, raw diagnostics, moving melee, ADS and keyboard.
  Installed after no process verified; all4 EXE/PDB SHA256match, VERSION179.
  Startup179 PASS: visible MP menu leftopen. Live179 hold test pending.

## 0.5.178: Experimental stationary L3 save / R3 return point

- User requested a fun tactical-insertion-style return point: hold L3 while
  still to save; clarified R3 teleports back. A new one-second hold replaces
  the saved point. It is a manual return point, with native respawn unchanged.
- Controller-only physical L3 sampled before sprint filtering. Hold must remain
  stationary, upright, alive, on foot, out of ADS and menus for one second.
  Moving L3 keeps sprint; scoped L3 keeps steady aim. With a saved point,
  stationary R3 tap returns once per press; moving R3 keeps melee. Keyboard
  sprint/melee unaffected. Point resets with host/map-clock change.
- Full player-box native trace checks ground/clearance before saving or return;
  blocked positions reject with notice. Native8255E118 updates entity/player
  origins and teleport bit; clears velocity on return. On-screen save/return
  notices and tactical-point log lines. No game files uploaded.
- Bot crash/watch/death/UAV routines unchanged.177 live log confirms pointer
  watch/helper armed; user reports some bot weapons still invisible. Crash,
  unwanted movement, partial guns, UAV and artillery remain unresolved.
- MP/SP builds PASS, including final controller-only refinement. Actual input
  routing/hold tests PASS: timing, one-shot, release/cancel, wrap, ADS, moving
  sprint/melee, keyboard and missing-marker behavior. Native teleport test
  pending. Installed after userclosed177; all4EXE/PDB hashes match, VERSION178.
  Startup178 PASS: visible multiplayer menu, leftopen; live teleport pending.
  Both build stages, test/scope
  logs and177 runtime snapshot archived in docs/build-logs.

## 0.5.177: Trace the first writer clearing the native error-queue pointer

- 176 FAILED extended combat: user reports crash and residual unwanted movement.
  Kills of8,15,7 credited. All three native death before/after headers valid0.
  After thirdkill, unrelated globals become0 and invalid indirectcalls occur;
  queue POINTER becomes0, so176's countFFFFFFFF was guest0, not queue header.
  No producer enqueue logged. Do not describe this as a proven queue overflow.
- Existing hardwarewatch armed synchronously from AddBot skipped its own/main
  thread (ArmWatchpoints explicitly excludes caller).177 changes target to
  queue-pointer global82A87860 and arms from a helper so main is included.
  Existing exception handler should name the writer; runtime watch still needs
  verification. No gameplay/death-path redesign in this diagnostic patch.
- Null queue now returns without reading or clearing guest0. Previously176's
  count quarantine could write0 to guest0 after pointer destruction, itself
  touching the host weapon view. Other valid queue processing unchanged.
- MP/SP builds PASS; extracted actual drain wrapper tests PASS including null
  queue no-write, valid0..8 native calls, oversized counts quarantine.
  Installed while no gameprocess; all4 EXE/PDB hashes match, VERSION177.
  Startup177 PASS: visible menu, leftopen. Combat/writer-watch test pending. Crash, movement, UAV, weapons and artillery
  remain unresolved; this is diagnostic containment, not a crashfix claim.

## 0.5.176: Trace and contain the repeated bot-kill crash

- 175 user test FAILED: killing a bot crashed. Same native824309F0 drain as172;
  175 fault RVAa84002 guest13E10088 after2GB demand commits. Preserved full log.
  Bot7 died, host kill credit logged; native death/animation/script steps ran.
- Further native analysis identifies the list as the ERROR MESSAGE queue,
  not a draw-command list.8210DCF8 only admits8 entries (260bytes each),
  constructor8210DD70 allocates8*260. Fresh175 menu read-only snapshot:
  queue82685ED0 count0 buffer50113440. No evidence yet of what corrupts it.
- Diagnostic176 logs error enqueue format/arguments/caller, queue header before
  and after player death, and count/header before native drain. If count>8,
  log and reset that invalid count instead of scanning arbitrary guest memory
  up to2GB. This is containment, NOT a confirmed fix for the original corruption.
  Valid0..8 counts retain native processing.175 stance-event fix retained;
  live175 logs show bot165 events intercepted under client0, caller8246BBF0.
- MP/SP builds PASS; extracted actual queue-wrapper tests PASS (valid0..8
  delegate,9/1000000/FFFFFFFF quarantine without native walk). Installed after
  menu session closed; all4 EXE/PDB SHA256 hashes match; VERSION176.
  Startup176 PASS: visible multiplayer menu, left open. Match test pending.
  Native producer/death evidence and logs in docs/build-logs.
- Live176 first kill PASS: bot8 death/native scripts, hostkillcredit and respawn;
  process responding. Error queue0 both before/after, no containment triggered.
  One kill only; repeated kills and prone confirmation still pending. Crash
  fix notclaimed. Runtime snapshot archived in docs/build-logs.


## 0.5.175: Keep bot stance events out of the host's local controls

- 174 FAILED the prone test: user still forced up after1-2sec. Its extra
  stance-latch save did not resolve the source of the commands.
- Live isolation on174: debug bot_sticks1,0,0,0 disables AI commands but retains
  bots/session processing. User confirmed stays prone. Then bot_buttons1 makes
  them fire without AI stance/jump/movement: user again confirmed stays prone.
- Native8246A7D8(entity,event,param) cases165/166/167 write stand/crouch/prone to
  stance latch82B57804[CurrentClient], then return. These are local input
  effects, distinct from the bot body movement. Events can be handled outside
  the isolated bot think/session scopes, with the host's client current.
- Skip only165..167 for validated bot entities; host and every other event
  continue through native handler. Log source bot, event, current client/caller.
  This is the only gameplay change from174. Death, UAV, weapons, artillery code
  unchanged. Does not disable AI stance commands or clamp the host prone.
- MP/SP builds PASS; actual-wrapper test PASS768 cases (all host/unknown
  sources delegate; only bot165..167 skipped). Native-case review PASS.
  Installed after userclosed174; all four EXE/PDB SHA256 hashes match, VERSION175.
  Original debug.txt restored and verified: AI buttons/movement active again.
  Startup175 PASS: visible multiplayer menu, left open. Live match test pending.
  Build/tests/runtime evidence archived in docs/build-logs.

## 0.5.174: Isolate the host stance latch and normal bot session context

- 173 user result: cannot remain prone longer than 1-2 seconds; UAV still has
  no bot markers; only some bot guns appear. Artillery also fails to fire.
- Read-only173 trace confirms host stance command bits change alongside eye
  height. Native82525338 consumes a separate stance latch at82B57804, outside
  171's kbuttons/HostInput saves. Native82571930 clears all four stance latches.
- Preserve host latch plus slot0 hold fields (+1608..1619) across isolated bot
  steps; log any restoration. Normal82400810 bot updates now use the bot's
  private gun view, scratch local context, and command slot, just as explicit
  bot think does. Previously the normal session tail could run them under0.
  ReKitBots now uses BotClientScope instead of calling its script under host0.
- This patch targets bot context/input isolation. UAV and death code unchanged.
  Live173 weapon snapshot: many living bots have in-hand/pending0 with owned
  guns; visible guns have nonzero IDs and a second rig part. Marker timestamp
  team+72 is0 for most bots; radar inactive at snapshot, so no marker fix claimed.
  Artillery173 log reached ready (520B5264), failed firing step still unresolved.
- MP/SP builds PASS (existing kernel printf warning), source checks PASS.
  Installed after user closed game; all four EXE/PDB SHA256 hashes match,
  VERSION174. Startup smoke test PASS:174 visible at multiplayer menu, left open.
  Live match test pending.

## 0.5.173: Restore bot weapon/session updates with narrower input isolation

- 172 live test FAILED: bot weapon models missing; shooting a second bot crashed.
  User confirms kill-feed credit AND death animations still work.
- Preserved 172 crash log in work/CoD3MP-0.5.172-crash.log. Matching PDB resolves
  fault RVA a83152 to native824309F0, a 260-byte-entry renderer work-list drain;
  its read reached guest165000FC after 2GB of demand commits. This identifies
  the failure site, not the writer that corrupted the list. Cause unproven.
- Restore the 171 supplemental bot session updates removed in172. Require each
  bot's private gun-view block, explicitly lock physical input for scratch
  client3, and filter82400810 so its all-player tail updates only the selected
  bot during that supplemental call. Native normal frame remains unfiltered.
  Scratch lock, selected-player scope, local place, timers and gun-view pointer
  are restored afterward. Existing key/input restore retained as a backstop.
- Death/obituary hooks and UAV routine unchanged. Static checks PASS, MP/SP
  builds PASS, installed both EXEs/PDBs with matching SHA256. Existing unrelated
  kernel printf format warning remains. Startup smoke test PASS:173 visible at multiplayer menu, left open.
  Match regression test pending: bot guns,
  multiple kills, crash stability and host prone/stand. UAV remains unresolved.

## 0.5.172: Stop replaying the host player update for every bot

-171 user result: bot kills now appear correctly in the kill feed. Random
  prone/stand changes still occur, and UAV still shows no bot markers. Death
  animation was not separately confirmed by this latest report.
-167's extra call to82403840 for each bot also runs that routine's tail over
  all24 session players, including the host, under the bot's scratch local
  context.171 restored key/input bytes afterward, but that cannot undo player
  actions already processed by the repeated session ticks.
-Remove the supplemental per-bot session ticks; keep the real native session
  frame once.171's now-confirmed lethal-hit/obituary path remains unchanged.
  This is the only gameplay change. UAV work follows separately.
-Source checks PASS: no supplemental82403840 call, death hooks and UAV routine
  unchanged. MP/SP builds PASS; both EXEs/PDBs installed, executable hashes verified, VERSION172. Build/source-check logs archived in docs/build-logs. Live prone/kill-feed regression pending.

## 0.5.171: The bots' deaths played on the host (death animation, kill feed); prone guard

- **Justin:** still no death animation or kill feed for bots; lying prone
  he sometimes stood up as if someone else had the controls.
- **Found (0.5.170's trace):** at Justin's death the game ran its whole
  "player dies" routine: the gun dropped, the death animation, the scripts'
  death steps and the kill message. At a bot's death none of it ran. The
  game hands a lethal hit to the online session, which plays the death on
  the dead player's own console; a bot has no console of his own, so his
  death was never played anywhere.
- **Fix:** a bot's lethal hit now takes the game's own offline path, which
  plays the death right there on the host, then posts the kill.
- **Prone:** the bots' per-frame session update now can't touch the host's
  keys or input state (they're put back after it, and the log says if a
  bot's update had changed them).
- **Test:** War with about 10 bots: kill several (death animation? your
  kill in the feed?); lie prone near bots for a while.

## 0.5.170: Bots melee, throw smoke, reload and jump; tracing the bots' deaths

- **Justin:** still no death animation for any bot; he wants the bots to
  melee up close and to use what their class carries (smoke grenades,
  bazookas, artillery).
- **The buttons:** found in the game's own table of its keys, so these are
  the real ones and not guesses: melee, smoke grenade, reload, the class
  grenade, lean, use, and the jump (the "up" move).
- **Melee:** a bot with an enemy in sight within about 70 units swings.
- **Smoke:** a bot at half health or less with an enemy in sight throws his
  smoke grenade, once every 30 seconds or so.
- **Reload:** a bot whose gun's clip is empty, with rounds to spare,
  presses reload.
- **Jump:** a bot that wants to go forward but hasn't moved in a second
  (stuck on something low) jumps; running about with no one in sight,
  bots now and then hop (rushers and flankers more often).
- **Not yet:** bazookas and the scout's artillery need the bot to pick
  that weapon or the binoculars; next.
- **Death animation:** 0.5.169's log showed the bots' records die exactly as
  the host's does (state 4 and the dead movement), so the missing animation
  is in the steps after. The log now names the game's death steps as they
  run (the server's, the scripts' "Player_Dying" and "DeathState", landing
  on the ground) and whose death each is, for the host's and the bots'.
- **Test:** War with about 10 bots: get close (do they melee?), hurt them
  (smoke?), watch them reload and jump; kill a few bots AND get killed by
  one, then send the log.

## 0.5.169: Fix: bots respawning every frame; three menu pictures upscaled

- **Justin:** with 24 bots none came in right: all of them flashing in and
  out as if falling, and they couldn't be killed.
- **Cause:** 0.5.168's death-state change was put in the middle of the
  "dead long enough? respawn" check, which then ran for every LIVING bot
  every frame. That check is back as it was, and the death state is set
  on its own, so its own test is still in.
- **UAV:** the log now says, every 2 s while a UAV is on, what the compass
  checks for the first six bots: his side against the host's, and when he
  last "fired" against the level's time.
- **Menus:** textures\used.txt showed every big menu picture before the
  match was already upscaled except three sepia photos (tanks, troops) seen
  just before the match. They're upscaled x4 now (in textures\replace,
  not GitHub).
- **Test:** War with plenty of bots: do they come in and play normally; kill
  some (death animation? kill feed?); watch the compass as they come in;
  call a UAV.

## 0.5.168: Bot deaths marked as the host's are; bots enemy on the compass from the start; roles and classes; a list of the menus' pictures

- **Justin:** he wants a mix: some bots rushing, some roaming the edges of
  the map, some playing the objective, some camping; and each with a
  different class and weapons.
- **Roles:** every bot is now one of four: objective (goes for the flag in
  play), rusher (sprints at you, runs the map when no one's about), flanker
  (long runs around the map, changing direction now and then) or camper
  (holds a spot crouched, moving on every minute or so). In a fight they
  all fight. The log says which each bot is.
- **Classes:** each bot gets one of the game's seven classes at random, so
  their guns differ. (Custom classes and the campaign guns for bots come
  later.)
- **Menu pictures:** every picture the game shows is now listed in
  textures\used.txt with whether an upscaled copy was used, to find the
  Create Lobby and System Link pictures still missing one.
- **Death animations:** the host's player record goes from state 3 (in
  play) to 4 a moment after he dies; the bots' stayed 3, and the game draws
  a player in state 3 as alive, so a shot bot froze standing and vanished
  at respawn. A dead bot is now put in state 4 too, and back to 3 when he
  respawns. The log says "dead: state 3 -> 4" for the first few.
- **Compass:** a new bot's side was 0 for a moment before it was picked,
  which the compass showed as a friendly green arrow. His side is now set
  when he's made.
- **Test:** War with plenty of bots: kill a few (death animation? kill feed?), watch the compass as they come in; a mix of behaviour and of guns; then
  walk through Create Lobby and the System Link screens.

## 0.5.167: The bots' deaths through the game's own player update; the UAV shows bots

- **Deaths and kill feed:** a player's death is written down and sent to
  every console only by the game's once-a-frame update of the players at
  this console; the bots were never part of it. Now each bot gets that same
  update every frame, so his deaths are recorded like the host's: the death
  animation and the kill feed should follow.
- **UAV:** Justin: the UAV never showed the bots. The compass only marks
  enemies who fired in the last 15 seconds, and a bot's shots never counted
  as firing for it. While a UAV is on, every bot now counts as having just
  fired: all of them on the compass, the whole time it runs.
- **Test:** kill some bots (death animation, kill feed); call in a UAV and
  look at the compass; still no trouble for you.

## 0.5.166: Smarter bots: they aim at you prone, grenade your cover, hide to reload

- **Justin:** make the bots better: shoot him when he lies down, take
  cover more, throw grenades.
- **Prone and crouched:** the bots aimed at a standing player's head
  height whenever someone's eyes were low, so they shot over anyone lying
  down. They now aim at where your head really is, and look from their own
  eyes when they crouch.
- **Grenades:** besides at someone in the open, a bot now throws one at
  someone he just saw duck behind cover nearby.
- **Cover:** a bot goes for cover when his gun runs dry with someone about,
  and sooner when hurt.
- **Compass:** right after a bot respawned he showed for a moment as a
  friendly green arrow: the game's spawn gave him the host's side for a
  frame. His side is now set straight after the spawn.
- **Test:** go prone in front of a bot; hide behind a wall after he has
  seen you; watch whether they back off to reload.

## 0.5.165: Learning the bots' deaths; not every bot camps the flag

- **Justin, on 0.5.164 (20 bots):** the kill feed shows him dying to the
  bots, but not his kills on them; still no death animations; and most bots
  just stood still.
- **Standing still:** in War every bot went to the one flag in play and
  held it, crouched. Now a third of them hunt instead and the rest go for
  the flag.
- **Deaths (learning):** when anyone dies, the game's record of that player
  in the match is now written to the log. Comparing a bot's death with the
  host's shows what the bots' deaths are missing.
- **Test:** War with bots: kill a few bots, and get killed a couple of
  times too (both are needed for the comparison).

## 0.5.164: The bots' deaths and kills, sent under the host's console id

- **Justin, on 0.5.163:** still no death animations (they freeze and
  vanish) and the kill feed doesn't show his kills on them.
- **Why (likely):** the game passes a death and a kill around as a message
  naming the console the player belongs to; each bot's record carried a
  made-up console id nobody answers to, so those messages went nowhere.
  They worked when the bots used the host's own record, id and all.
- **Now:** each bot's record carries the host's console id.
- **Test:** kill some bots: death animation, kill feed; many bots still fine.

## 0.5.163: The bots' deaths, from their own "console" marked as this one

- **Justin, on 0.5.162:** bot_add 20 works: lots of bots all over the map.
  But still no death animations: they freeze and vanish.
- **Why:** the deaths (and kill feed) worked only while a bot shared the
  host's own "console" record; several parts of the game look straight at
  that record's "this console" mark, which each bot's own copy had off.
- **Now:** each bot keeps his own record, but marked as this console's.
- **Test:** kill some bots: death animation, kill feed; bot_add 20 still
  fine; nothing odd for you.

## 0.5.162: The bots' deaths and kill feed (local place answers)

- **Justin, on 0.5.161:** the bots hold their guns again. Still no death
  animation, and nothing in the kill feed.
- **Why:** those worked back when a bot held one of the console's local
  player places (0.5.137-0.5.144). The game asks "is this one of ours" by
  those places, and the bots no longer hold one.
- **Now:** for a bot that question answers yes, without giving him a place
  (no split-screen limit of 3 comes back).
- **Also:** bot_add 20 said 20, but only 14 were really added, and only
  the first 6 were drawn (the rest never got the session player the game
  draws them through). Now all 20 get added and each one gets drawn.
- **Test:** kill the bots: death animation and kill feed; three bots, no
  trouble for you; then bot_add 20.

## 0.5.161: Bots counted as this console's players again (guns, deaths, kill feed)

- **Justin, on 0.5.160:** three bots in, all stable, no bugs for him. But
  the bots have no gun in their hands, no death animation, and their deaths
  don't show in the kill feed.
- **Why:** all three are things the game expects an online player's own
  console to send; a bot has none. They were made online-style players
  (0.5.145) because counting them as this console's caused the zoom, the
  host's guns going full auto and freezes, but that trouble was really
  their controls being written over the host's (fixed in 0.5.159).
- **Now:** the bots count as this console's players for all of that again,
  while keeping everything of their own that fixed the bugs.
- **Test:** three bots: guns in their hands, death animations, kill feed;
  and still no trouble for you (gun, zoom, prone, freezes).

## 0.5.160: The bots' death animation back

- **Justin, on 0.5.159:** two bots, no bugs: his gun, view and controls are
  fine, and the bots move well and kill him. Only their death animation is
  missing (since 0.5.145).
- **Why:** the game plays a death for an online player by telling that
  player's console to; a bot has no console of his own, so nobody played it.
- **Now:** for their deaths the bots count as this console's players, so
  the host plays the animation, as before 0.5.145.
- **Test:** kill each bot: he falls; also try a third bot.

## 0.5.159: Found it: the bots' controls were written over the host's

- **Justin, on 0.5.158:** it froze or crashed again after bot 2, and his gun
  glitched for a second.
- **Found (0.5.158's log named the step):** each frame the bots' "controller
  input" (where to move, look, fire) was written into a list the game keeps
  for the players at this console, which only has room for 4. Bot 1 (player
  4) landed on the host's own input state: that is what made him stand up
  from prone, feel like someone else had control, and glitch his gun. Bot 2
  (player 5) landed on the host's gun view: gun gone, then the crash.
- **Now:** each bot's input goes into a space of his own, and the server
  reads it from there. Nothing of theirs touches the host's any more, for
  any number of bots.
- **Test:** War, bot 1, bot 2, then more: gun steady, prone stays prone, no
  freeze or crash; the bots still move and shoot.

## 0.5.158: Pinning down the step that takes the host's gun

- **Justin, on 0.5.157:** still the same: soon after bot 2 his gun was gone.
- **What the log showed:** the War effects weren't it (none were skipped),
  and the processor watch didn't catch the write, though the host's gun
  view pointers were wiped right as bot 2 was added.
- **Now (to find it):** those pointers are checked after every single step
  of adding, setting up, spawning and running the bots, and of the game's
  frame around them; the log names the first step after which they changed.
- **Test:** War, bot 1, then bot 2.

## 0.5.157: Found what breaks the host with bots in War

- **Justin, on 0.5.156:** it still crashed soon after bot 2; dig deep.
- **Found:** War's script gives every player little screen effects (a view
  kick and such) through two of the game's own functions, by player
  number. Those functions are meant for the players at this console
  (numbers 0 to 3): they switch the game's "which local player" to that
  number. For a bot (4, 5...) that number was past the end of the game's
  lists for local players, so the next code that used it wrote over the
  host's gun view pointers: the gun reset to nothing, then the crash. The
  log of 0.5.156 shows those pointers wiped right when bot 2 joined.
- **Now:** those screen effects are skipped for anyone not at this console,
  and the game's "which local player" isn't touched. The processor watch is
  back on the gun view pointers in case anything else still writes them.
- **Test:** War, add bot 1, bot 2, more: gun stays, no crash.

## 0.5.156: The host's gun view object itself guarded

- **Justin, on 0.5.155:** the game crashed as soon as he added the second bot.
- **What the log showed:** the pointer was never emptied; it always points
  at the host's own gun view. That gun view itself is what gets wrecked.
- **Now:** it is kept exactly as it was around every bot step (the log lists
  what a step changed), and the processor watches the part of it the crash
  reads, naming whatever writes it.
- **Test:** War, bot 1, then bot 2.

## 0.5.155: Catching what empties the host's gun view

- **Justin, on 0.5.154:** after the second bot his guns were invisible and
  he couldn't switch them back.
- **What the log showed:** the spot 0.5.154 watched was never written; the
  host's gun view isn't scribbled on, it's taken away: the game's pointer to
  it is emptied when a bot is set up, and the game then reads an empty one.
- **Now (to find it):** the processor watches that pointer from the first
  bot on and names whatever code empties it.
- **Test:** War, add bot 1, then bot 2, until the gun goes.

## 0.5.154: Catching what wrecks the host's gun with a second bot

- **Justin, on 0.5.153:** one bot was fine; adding the second, his gun
  glitched and the game crashed soon after.
- **What the log showed:** the bots' own gun view memory works, but the
  moment the second bot comes in, something still writes into the host's
  gun view through another way, and the game later crashes on it.
- **Now (to find it):** when the second bot is added, the processor itself
  is told to report every write to that spot of the host's gun view, with
  the game code that made it. The put-back guard also lists exactly what
  each bot step changed.
- **Test:** War, add two bots, play until it breaks.

## 0.5.153: Each bot has his own gun view memory

- **Justin:** with bots meant to be a game setting, they should have their
  own gun view memory rather than going through the host's.
- **Now:** every bot gets a block of memory of his own for the game's
  first-person gun view, used whenever anything runs for him (his set-up,
  thinking, spawning, the game's own runs of his player). Nothing he does
  lands in the host's any more, however many bots there are. The put-back
  guard from 0.5.151 and 0.5.152 stays as a safety net.
- **Test:** War with two or more bots: your gun stays, you can move, no
  freeze or crash; the bots still move, shoot and go for the flag.

## 0.5.152: The game's own bot spawns and thinking guarded too

- **Justin, on 0.5.151:** his gun was almost right, then glitched, then he
  froze, as if something else had a little control. (The bots now go for
  War's flag in play: the log shows them following it as it moves.)
- **Why (from the log):** besides our own steps, the game itself spawns and
  runs the bots' players, and those runs too wrote into the host's gun view
  memory: his gun was taken out of his hands, then the game crashed in that
  same gun code.
- **Now:** whatever the game runs for a bot's player keeps the host's gun
  view memory and field of view as they were.
- **Test:** two bots: your gun stays, you can move, no crash.

## 0.5.151: Bots play War's flag; the host's gun guarded

- **Justin, on 0.5.150:** after the two bots came in he had no gun again,
  and a vehicle he got in wouldn't drive or let him out. He took War flags.
- **War:** his captures in the log showed how War keeps its flags: one flag
  at a time is in play (the middle one first, then the next along after
  each capture). The bots now head for the flag in play and stay on it,
  unless an enemy is close; they fight for it like players do.
- **The gun:** the game keeps the host's gun view in memory that the bots'
  own runs of the same code also wrote into. Now that memory is put back
  exactly as it was after every bot's thinking and set-up.
- **Test:** War with two bots: they go for the flag in play; your gun shows;
  vehicles work.

## 0.5.150: No more gun flicker from the bots

- **Justin, on 0.5.148:** with a second bot his guns are fine now, but they
  still glitch in and out a little while he just holds them.
- **Why:** the game's first-person gun view was also run for the bots, who
  have none; it shared its saved field of view with the host's and kept
  knocking it to 0 for a frame.
- **Now:** that view is skipped for bots, and the shared saved value is
  healed when it breaks. Also has 0.5.149's War flag logging.
- **Test:** a War match with two bots: guns steady; take a flag or two.

## 0.5.149: Learning War's flags (for the bots)

- **Justin:** the bots should play War's flags.
- **Found:** War keeps its flags as the map's "war_flag" pieces in a list,
  each with its own set of the script's values (which flag it is, who holds
  it, how far it's taken), named by number, not by word.
- **Now (learning only, nothing changes in play):** in a War match, every
  2 seconds each flag's values are written to the log when they change
  ("war: flag ..."). One match where flags get taken shows which value is the
  holder; then the bots get sent to the right flag.
- **Test:** play a War match (bots or not) and take a flag or two.

## 0.5.148: The host's gun view healed (no gun with two bots)

- **Justin, on 0.5.147:** with two bots his view shows no gun, though he can
  switch weapons.
- **Why (from the log):** his own weapon view keeps a saved copy of the field
  of view and puts it back every frame. A bot's code had once left the field
  of view at 0 at the moment it was saved, so his view kept coming back to 0
  and the gun was drawn with no view at all.
- **Now:** when that saved copy is broken, it and the field of view are both
  put right, so it can't keep coming back.
- **Test:** two or more bots: your gun shows.

## 0.5.147: No phantom split-screen player for the bots (freeze, gun gone)

- **Justin, on 0.5.146:** with two bots there was no zoom, but he froze and
  his gun disappeared.
- **Why (from the log):** the game came to run a second "local player" (a
  split-screen player that doesn't exist) on the place the bots use for
  their set-up; its view and weapon code then wrecked the host's view, gun
  and controls each frame.
- **Now:** while the bots are online-style players, the game's three spare
  split-screen places are kept empty, and that phantom player's frame is
  skipped.
- **Test:** add two or more bots: you can move, your gun shows, no zoom.

## 0.5.146: The host's field of view kept while bots think

- **Justin, on 0.5.145:** one bot shows and there is no zoom, but his death
  animation is gone. With a second bot nothing crashed, but his own guns
  stopped drawing properly, he stood up from prone by himself, and he heard
  reload sounds.
- **What the log showed:** every frame, while a bot thought, the game's
  weapon-view code wrote the bot's own saved field of view (0) over the
  host's.
- **Now:** for every bot, however many, the host's field of view is put back
  exactly as it was after the bot's thinking and set-up.
- **Still open:** the bot's death animation (since 0.5.145), the reload
  sounds; the prone and gun drawing may be fixed by this, to be checked.
- **Test:** two or more bots: your guns draw and pull out normally, prone
  stays prone, no zoom.

## 0.5.145: Bots join as online players (no limit of 3)

- **Justin:** he wants every bot he adds to work, not a patch per bot.
- **Why the old way couldn't:** each bot posed as one of the host's split
  screen players, and the game only has 4 of those; past that, memory was
  overwritten (the zoom, the crashes).
- **Now:** every bot is his own online player, as if he had joined from
  another console; the game takes up to 24 of those. Such a player was
  invisible before (0.5.135) because the game draws an online player from
  what his console sends; for a bot the host now draws him from his own
  body, as it does its own player.
- The old way can be put back from the console: `bot_remote 0` and
  `bot_local 1` before adding bots.
- **Test:** add 1 bot, then several: visible, with his gun, kills in the
  feed, no zoom, no freeze or crash when one dies.

## 0.5.144: The host's field of view guarded (the zoom with two bots)

- **Justin, on 0.5.143:** two bots still zoom him in, and switching teams
  and respawning doesn't bring it back.
- **What the log shows:** the zoom starts the moment the second bot is
  created. The game's view starts from a stored field-of-view number, never
  lower than 1, and the host's came out at exactly 1: that stored number is
  being wrecked.
- **Now:** the number is watched around every step of a bot's set-up and
  thinking; if it goes bad it is put back, and the log names the step that
  did it ("bots: the base field of view went ..."), so the real cause can be
  fixed next.
- **Test:** add two bots: no zoom.

## 0.5.143: The bots' thinking uses their local number too (the zoom)

- **Justin, on 0.5.142:** adding the second bot no longer crashes the game,
  but still zooms him in (log: the host's field of view drops to 1).
- **Why:** 0.5.142 fixed the bots' set-up, but each frame, while a bot
  thinks and moves, the port still told the game his player number (4, 5)
  as the local player, past the 4-entry lists. The second bot's number
  landed on the host's view.
- **Now:** the bot's thinking uses his local number (1 to 3) as well; the
  one place that needs his real player number (fetching his commands) gets
  it back.
- **Test:** two bots: no zoom; both still move, aim and shoot.

## 0.5.142: Bots get their own local player number (towards two bots)

- **Justin, on 0.5.141:** killing the bot no longer freezes him (confirmed).
  Adding a second bot zoomed him in and crashed the game.
- **Why:** the game keeps some things for up to 4 local players (split
  screen), numbered 0 to 3. While setting a bot up, the port told the game the
  bot's player number (4, 5, ...) as the local player, so the game wrote past
  the end of those 4-entry lists and over other data. One bot happened to
  survive it; a second did not. Also, the second bot took the first bot's
  local place while the first was dead.
- **Now:** each bot gets his own local number 1 to 3 when added and keeps it;
  his set-up and his local place use it. A dead bot's place is given back
  once he is alive again. Up to 3 bots fit this way.
- **Test:** add a bot, then a second one: no zoom, no crash; kill each.

## 0.5.141: The bot leaves his local place while he is dead (host freeze)

- **Justin, on 0.5.140:** still frozen in place when the bot dies. He did
  see the bot's death animation (confirmed).
- **What 0.5.140 showed:** the flag it watched was never touched by a death
  (wrong lead). But the log shows the real trouble: right at the bot's death
  the game's session update calls through a wrecked object, and other parts
  of the game then trip over bad pointers. Something of this machine's local
  players gets wrecked when a bot who holds a local place dies.
- **Now:** as soon as a bot is dead, he gives up his local place, before the
  session update sees him; he gets it back when he respawns. Log: "bots: slot
  N is dead: leaves his local place".
- **Test:** add a bot, kill him, check you can still move and pause, and that
  his death animation still shows.

## 0.5.140: A bot's death no longer freezes the host

- **Justin, on 0.5.139:** still frozen in place when the bot dies; he can't
  even pause.
- **Why:** the game's handling of a session player's death (`sub_82404DD0`)
  ends by clearing a flag of this machine whenever the dead player belongs to
  this machine. The bot shares the host's machine, so his death cleared the
  host's flag, and the host's controls and pause stopped.
- **Now:** after that handler runs, if the flag was cleared while the host is
  still alive, it is put back. Log: "bots: a session death: local flag ..."
  and "bots: a bot's death cleared the host's local flag: put back".
- **Test:** add a bot, kill him, check you can still move and pause.

## 0.5.139: A bot is spawned out of his local place (the host's freeze)

- **Justin, on 0.5.138:** the bot's animations look right now. But he is
  still frozen in place each time the bot dies, though 0.5.138 put the
  current client back (log: "current client was 4 ...: put back").
- **Why (likely):** the bot's respawn goes the game's way through the
  session; holding a local place (0.5.137), he is spawned as one of this
  machine's local players, and that set-up takes the host's controls. Before
  0.5.137 (no local place) his spawns never froze the host.
- **Now:** for the length of his spawn (`Respawn`), the bot's local place is
  emptied, then given back. Log: "bots: slot N spawned out of his local place".

## 0.5.138: The host's controller kept on the host after a bot dies

- **Justin, on 0.5.137:** adding a bot no longer zooms him in (log: "slot 4
  takes local place 1"). The bot's kills now show in the kill feed, and he
  thinks he saw the bot's death animation. But after he killed the bot he was
  frozen in place: the log shows his commands still arriving, health 100,
  state 3, and his position not changing.
- **Why (likely):** the game's current client (0x82BCBE9C, also the local
  player it is working for) is left on the bot's after his death, so the
  host's controller drives nobody.
- **Now:** before and after every server frame, with bots in local places, a
  current client other than the host's 0 is put back to 0 (logged, first 30
  times: "bots: current client was N ...: put back"). Only the host plays on
  this machine, so 0 is always his.

## 0.5.137: Each bot takes a free local place (not the host's)

- **Justin, on 0.5.135 with `bot_remote 0` and two bots:** the zoom-in came
  back and the game crashed (guest read at 0x37660000 in `sub_8251A848`, the
  animation code, from `sub_82593D50` in the server frame). One bot works.
- **Why (likely):** the session keeps its machine's four local players as four
  bytes at +0x5991 of its object (session-player numbers, 24+ for none;
  `sub_824046D8`, `sub_823D8FB0`, `sub_823D8FF0`). The bots were in none, so
  "which local player is this" found nothing and fell back to the current
  client (the host, 0): his view zoomed, his guns misbehaved, and with two
  bots both landed on the same place.
- **Now:** when a bot is given his session player, he also takes a free local
  place (1 to 3) of his own. The host's place 0 is never touched. Up to three
  bots this way; a fourth logs "no free local place". Console `bot_local 0`
  turns it off for new bots. Log: "bots: slot N takes local place K".
- Watch: no zoom on bot_add, host's guns normal, two bots without a crash,
  and that the screen does NOT split (four local places are split-screen's).

## 0.5.136: Bots share the host's machine object again (visible)

- **Justin, on 0.5.135:** the bot is invisible again with his own "not on
  this machine" object. So the player update that draws him
  (`sub_82400810`) is for this machine's own players; a remote one is drawn
  from his machine's network messages, which a bot doesn't send.
- **Now:** `bot_remote` is off by default again (0.5.134's behaviour: visible,
  reloads, switches). The host's full-auto/endless-ammo moment and the
  missing kill feed and death animation stay open; they need the specific
  local-player paths the bot wrongly takes found and fenced off, not a
  remote identity.

## 0.5.135: Each bot is his own machine's player, not a second local one

- **Justin, on 0.5.134:** the bot now reloads (his reload sounds heard) and
  switches to his pistol when his main gun runs dry (log: "out of rounds for
  19: asks for 15"). But for a while Justin's own guns went full auto with
  endless rounds and his hits on the bot didn't count; then it stopped and the
  bot killed him. The bot's deaths never show in the kill feed, and he still
  has no death animation.
- **Why (likely):** a session player's +4 is a counted holder (+4 its count)
  of a machine's player object (+8). That object's +0x38 byte means "on this
  machine" (`sub_821AE440`, which `sub_824046D8` uses to pick the local or
  the network path) and +0x34 tells machines apart (`sub_821AE448`). The
  bot's entry, copied from the host's, shared both, so the game took him for
  a second local player of the host's own machine.
- **Now:** each bot's session player gets its own copy of the holder and the
  object (0x100 bytes each, its count set high so the game never frees our
  memory), "not on this machine" (+0x38 = 0) with an id of its own (+0x34).
  Console `bot_remote 0` gives new bots the shared one again.
- Watch: the host's guns, the kill feed, the death animation, and whether he
  still shows and moves (a remote player may be treated differently).

## 0.5.134: Bots switch guns when one runs dry

- **Justin, on 0.5.133:** the bot's gun shows and he kills Justin (confirmed).
  He never switches weapons, and on death he still vanishes with no death
  animation.
- **Now:** when the gun he holds has no rounds left (clip + spare), the bot
  asks for his other kit gun (the main one at record +1066, else the pistol
  at +1065) that still has rounds. Layout from the last session's reader
  (work/read_weapon_cycle.py): weapon defs at 0x82BCBF9C, clip at
  +4*(172 + def+1468), spare at +4*(80 + def+1460), owned bits at +1056.
  Log: "bots: slot N is out of rounds for A: asks for B".
- Reloading is left to the game (not traced: no reload button found yet).
  Death animation still open.

## 0.5.133: A bot asks for his last weapon when his hands are empty

- **Justin, on 0.5.132:** still no gun, no death animation. The log: the kit
  given again five times a life, "after the kit: in hand 0, waiting 0" each
  time. The first kit put weapon 19 in his hands; after the session respawn
  the kit gives nothing new (he still owns it) and nothing raises it.
- **Now:** a player asks for the weapon he wants up through his command's
  weapon (+8). A bot with nothing in hand now asks for the last weapon he
  held (19 in the logs) instead of 0. If the gun still doesn't come up, he
  doesn't own it after the respawn and the kit needs its inventory part redone.

## 0.5.132: A bot is given his kit again after his session respawn (his gun)

- **Justin, on 0.5.131:** the bot is named "Bot 9". He shoots and damages,
  but no gun shows in his hands, and on death he freezes until he respawns.
  `bot_setup 0` (the kit menu's callback) zoomed Justin's view again, and the
  new bot still had no weapon: that callback is not the way.
- **Why no gun:** the log shows the class kit (`sub_82206028`) given to each
  bot at his first spawn ("class: kit for handle 00004005 ... class 0",
  "holds weapon 19"), then the session respawn (0.5.130) spawns him again
  after it and he holds weapon 0 from then on.
- **Now:** a living bot (state 3, health > 0) with nothing in hand or waiting
  (+164, +196) is given his kit again by calling `sub_82206028` with his own
  script handle, at most once a second and five times a life. No menu
  callback, nothing of the host's touched. Log: "bots: slot N alive with no
  weapon: kit given again" and "after the kit: in hand X".
- Watch: whether the gun model now shows, and whether the death freeze was
  the empty hands (death animations may be chosen by the weapon held).

## 0.5.131: Bots named "Bot 1" to "Bot 24"

- **Justin, on 0.5.130:** the automatic session player works (log: "slot 4
  given session player 1"), but the bot shows Justin's own name.
- **Now:** the bot's session player gets its own name at +104 (where 0.5.130's
  log shows the host's), "Bot N" with N picked at random from 1 to 24, no two
  bots sharing one. Log: "bots: slot N is named Bot M". If the scoreboard
  still shows the host's name, the name is also kept elsewhere (the client's
  userinfo) and needs the same change there.

## 0.5.130: Bots get session players automatically (the bot is visible)

- **Found (Claude, live test with Justin on 0.5.129):** the empty render record
  at entity +532 was a dead end: it is the local first-person weapon record
  (built by `sub_82468D58` for the local client only). What a host bot lacks
  is a **session player**: 24 entries of 784 bytes at session +48832
  (+0 number <24, +4 a counted object, +8 client slot). Every frame
  `sub_82403840` walks them and runs `sub_82400810` (the player's whole
  visual/animation update) only for filled entries.
- **Proved live:** debug.txt `bot_session 2` (gives each bot a copy of the
  host's entry and respawns him the game's way) in a System Link Merville
  match, then `bot_bring`: Justin SAW the bot for the first time. It fought,
  took damage (health 63), died and respawned. No crash. Its gun is still
  invisible (bot weapon 0, a known separate issue).
- **Now:** each bot is given a session player automatically, half a second
  after `bot_add`'s bots are in, when a session exists (not in a devmap). The
  copied +4 object's count (+4 of it) is raised for the copy, as
  `sub_824046D8` does, so the bot's entry going away can't free the host's.
- Untested in this build: automatic path, map change/restart with bots,
  scoreboard name (the entry is a copy of the host's, so it may show his).

## 0.5.129: Put away the launcher to the previously held weapon

- User confirms128 Y weapon cycling works again; log shows19->15->19 native
  selections and equipped-state transitions. Mouse-wheel shares the edge path,
  but independent live wheel confirmation has not been supplied.
- Launcher previously always returned to class primary. Save the current
  weapon name/id when taking it out and return to it when putting it away.
  Check actual equipped launcher rather than a stale toggle, reset on spawn,
  and fall back to class primary if saved weapon is no longer owned/valid.
- Only launcher return behavior changes. Invisible bot/radar remain unresolved;
  animation and model updates run but do not prove visible scene submission.
- Actual-source return regression PASS: pistol/primary round trips, cycling away from launcher, invalid/unowned saved return. Both MP/SP builds PASS; installed EXEs/PDBs and executable hashes verified. Build/test logs archived in docs/build-logs. Pistol->launcher->pistol still needs live confirmation.

## 0.5.128: Deliver repeated Y/wheel presses to native weapon cycling

-126 trace proved first primary->pistol cycle succeeds and later raw Y edges
  never reach the cycle command.127 user again confirms cannot return primary.
- Count raw Y/wheel edges in shared input, then consume once per host frame
  through native82457A60(1,0), preserving native selection/ammo/animation logic.
  Suppress the parallel native Y report only while the bridge is active to
  prevent a double cycle. No forced weapon state or loadout rewrite.
- Drain/discard menu presses; bridge disabled while paused, F11 open, dead,
  mounted, nonlocal context, or native weapon-use guards fail. Native menu and
  vehicle Y remain available. Campaign input retains its original reporting.
-127 model update now runs, but user STILL confirms invisible bot; visibility
  and radar remain unresolved. No bot behavior change in128.
- Input latch and actual-source bridge guard regression PASS, including repeated cycles, held button, inactive/paused reporting, menu/F11/dead/mounted/nonlocal guards and no deferred presses. Both MP/SP builds PASS; installed after closing127, executable hashes match. Weapon switching still needs live confirmation.

## 0.5.127: Evaluate host-bot animated bodies after the server frame

- Live126 trace found exactly one bot rig update during model composition,
  before its animation component existed, and no animated-model update after
  adding the component. Host bots bypass the network-player visual updater.
- After each server frame, ask the native rig update8251ED08(rig,-1) to evaluate
  registered bots with their own valid model, pose and animation component.
  Reject missing/stale rigs and rigs owned by another entity. Local player
  slots0..3 are excluded; native hidden-state and LOD checks remain intact.
- This changes only bot body evaluation. Weapon cycling is still unresolved:
  126 captured primary->pistol native select success, but later Y input edges
  did not reach native cycle. The continuous-slot-swap suspicion is unproven;
  do not treat it as the established cause. Radar/equipment remain unresolved.
- Actual-source guard regression PASS: local/unregistered/missing/stale rigs excluded; valid bots evaluated once per tick with native mode -1. Both MP/SP builds PASS; installed and EXE hashes verified. Scripted live bot test PASS: native model/animated-body update counts advance together, no crash observed during capture. User live127 check FAILED: bot still invisible after bring; body evaluation alone is insufficient.

## 0.5.126: Trace invisible-bot model updates and blocked weapon cycling

- User confirmed 125 bot still invisible after being brought beside host.
  Native animation allocation/registration passed; this does not establish
  visible rendering. Read-only state shows animation advances, valid model,
  private pose buffer, and no equipped bot weapon. Radar remains unresolved.
- User reproduced Y/Triangle and wheel failing to change weapons; launcher
  out/away restores primary. Input edges reach game; owned MP44/P38 have
  ammunition. Older per-frame slot rearrangement is a suspect, not proven.
- Diagnostic-only wrappers trace bot rig update/animated update and local
  native weapon cycle/selection, including native guard inputs and caller.
  Each wrapper calls the original once with unchanged input context.
  No gameplay fix is claimed and no slot/render behavior is changed.
- Both MP/SP builds PASS (work/build-0.5.126.log). Installed after user closed125; both executable hashes match build outputs. Live trace pending. Existing Kangaroo/settings fixes retained.

## 0.5.125: Complete native animation setup for spawned bots

- Live124 read-only comparison found valid German character geometry and92
  bones, but bot rig+32 animation component was0. Host has its own component;
  both reference valid animation data with the same verified resource type.
- Normal network-player setup calls8243F138(rig,0) after model setup. Add that
  native constructor once after bot spawn finalization if its component is
  absent. It allocates per-entity state and schedules the normal animation task;
  no host animation state or session entry is copied.
- Diagnostic comparison: bot_setup5 skips this new step on subsequent spawns;
  bot_setup1 is the normal default with animation setup enabled.
- Actual-source spawn-guard harness PASS: absent/existing component, comparison switch, and separate bot state. Both MP/SP builds PASS; installed after user closed124. Live native constructor PASS: bot component82700310 and registration flags61 confirmed by read-only inspection. Visual visibility still pending. Radar and weapon0 remain unresolved;
  no radar behavior change in this build. Settings and Kangaroo fixes retained.

## 0.5.124: Correct the live pause-menu sensitivity sliders

-123 live D-pad fix confirmed by user. Sensitivity reset reproduced in logs:
  startup100/100 became35/47, then37/50 after increasing. Audio stayed25/50.
- Read-only, validated process-memory inspection identified the actual controller
  panel820716DC and sliders82070AC4, value+24/min+28/max+32; old maxima37/50.
  These were not the separate menu path patched in120-122.
- Set those two real native sliders to10..200%, step10; native redraw divides
  by the actual maximum, so100% is half the bar and200% fills it. Preserve
  native UI sounds, other controls and default100%. Save after the observed
  native controller apply8250A4D0, independently of audio.
- Actual-source hooks harness passes100->90->100,200->190->200,
  independent axes, bounds, other controls, native effects, save callbacks,
  unchanged audio and initialization from old resource limits before draw.
  Log:work/test-live-slider-0.5.124.log. Both executables built/installed,
  VERSION124 verified; build log:work/build-0.5.124.log (exit0).
- Live user opened pause Controller: read-only memory check confirms both
  actual sliders value100/min10/max200; native preferences100/100/audio25.
  Saved work/live-options-124-controller-open.txt. User lower/raise/apply
  speed test now USER CONFIRMED FIXED (lower/raise returns to same speed).
  Live log confirms90/100 cycles; final chosen90/90 applied and written to INI,
  with volume50% unchanged. Disk saving verified; full relaunch still untested.
  Snapshot:work/CoD3MP-0.5.124-confirmed-sensitivity-fix.log.
- No bot behavior change.123 D-pad fix remains user-confirmed.

## 0.5.123: Block launcher equipment changes while paused

- One functional change: D-pad left no longer equips/puts away the launcher
  while the native pause/Options menu is open. Consume the input edge while
  paused so holding it across menu close cannot trigger a delayed switch.
  Uses the native pause byte already used to block binocular menu input.
- Add bounded read-only native preference-apply caller/value and actual
  gameplay preference-change diagnostics. These identify the unobserved
  settings path; they do not claim to fix the sensitivity/audio reset.
- User confirms122 pause -> Options -> Controller adjustment becomes slower
  and appears to reset audio. Log has no hooked controller-menu adjustments
  or native_options saves. Previous120-122 unit tests did not exercise the
  screen path actually reached by this test; integration remains unresolved.
- Contrary to the missing-bot appearance,122 log confirms slot4 spawned with
  generic_human55B1C9F4, instance68CF56D0, active AI. Visibility/radar unresolved.
  Preserved work/CoD3MP-0.5.122-user-settings-reset.log.
- Actual launcher-handler input-sequence harness passes blocked menu press,
  held input across close, and normal single gameplay press. Log:
  work/test-launcher-pause-0.5.123.log. Both executables built and installed
  after verifying no game process; VERSION123 verified. Build log:
  work/build-0.5.123.log. First attempt found a duplicate existing pad-look
  hook; moved trace into that existing hook and rebuild passed (exit0).
  Startup visibly reached the MP menu. Automated navigation did not reliably
  register, so live pause/slider reproduction remains uncompleted. Startup
  diagnostics showH100/V100/audio25 (100%/50%); the user's200% startup report
  describes perceived speed, not a confirmed persisted native200 value.

## 0.5.122: Native MP controller sensitivity up to level20 (200%)

- Extend both axes of the in-game controller slider from level15/150% to
  level20/200%, still in steps of10%. Default remains10/100%; existing saved
  settings are preserved. The native labels show the selected level and percent.
- Normalize the full bar to200 and allow saving/reloading values through200.
  Raise the free-look yaw ceiling435->580 degrees/sec so the new range can
  affect actual turning. Native ADS scaling and mounted yaw limits are retained.
- User confirms121's menu reaches15(150%). This patch extends that confirmed
  MP screen; campaign range extension remains outside this patch.
- Actual-source native Options harness passes10..200 adjustment bounds,
  independent axes,200->full bar, save callbacks, restore/defaults and yaw cap.
  Actual Settings.cpp disk test passes200 reload and invalid201 fallback.
  Logs: work/test-native-options-0.5.122.log and test-native-settings-0.5.122.log.
- Both executables compiled successfully with the existing configured CMake
  build (exit0). Log: work/build-0.5.122.log. Both installed after verifying
  the game closed; port VERSION.txt is0.5.122.
  Live turning feel and real restart persistence remain unverified.
  No bot, UAV or weapon-switch change in this patch. User confirmed121 bot
  remains invisible after verified teleport beside host, with no radar marker;
  nonzero generic_human instance alone did not fix it.

## 0.5.121: Release mouse capture during window moves/resizes

- One change: suspend clipping/recentering during title-bar/border operations,
  interactive move/resize and window-mode/resolution changes. Require clicks
  inside the client area before gameplay recaptures the mouse; dragging a title
  bar must not be treated as clicking into the game. A client click resumes aim.
- Fit the restored/resized window to its nearest monitor work area when needed,
  keeping the title bar reachable. Preserve valid placements and maximized state.
- Geometry test passes offscreen corner/negative positions, oversized and tiny
  sizes, unchanged valid position, negative-origin monitor and taskbar offsets.
  Actual source mouse capture harness also passes release, blocking warps/
  recapture while suspended, and fresh centre on resume. Tests preserved in
  chat work/test_mouse_capture.cpp and test_window_geometry.cpp.
- Both executables built/installed as0.5.121. MP visibly reached menu with
  title bar reachable. Automated border drag was not completed (bounds retry,
  then user input detected); no live drag-success claim. Captured-game resize
  still requires user test. Build log:work/build-0.5.121-completed.log.
- Includes120 native MP controller1â€“15 and volume/sensitivity persistence.
  Native labels/live feel/relaunch still unverified; bot visibility next priority.
## 0.5.120: Native MP controller levels1â€“15 and remembered Options

- Replace in-match controller screen's old0..50 adjustment clamp with levels
  1..15 (10..150% in steps10), default10/100%. Normalize the native bar to150
  and show the level/percentage beside horizontal and vertical sensitivity.
- Native gameplay consumes68/72*.01, so these change actual stick gain.
  Raise the free-look yaw ceiling290->435degrees/sec: the old ceiling could
  flatten high settings. Only CL_GamepadMove references that constant; mounted
  explicit yaw limits and native ADS scaling remain. WaW/MW2 feel uncalibrated.
- Persist native volume and both axes through CoD3Recomp.ini's native_options
  fields, captured when leaving native controller/audio screens. Restore once
  after older console saves load; subsequent menu changes remain untouched.
  F11 master volume and mouse sensitivity remain separate. Volume defaults50%.
- Scope: extended range, numeric labels and native save callbacks are MP's
  observed controller screen. Campaign receives new constructor defaults;
  campaign slider extension/save callbacks and other frontend paths unverified.
- Tests: actual Settings.cpp passes defaults, disk reload at150/differentaxes,
  independent F11 gain, invalid values. Actual native_options.cpp harness passes
  override of a simulated original50 clamp, independent axes, bounds, bar range,
  non-axis controls, callbacks, reload over old profile, Restore Defaults and
  yaw-cap update. Native label rendering/live feel/relaunch pending.
- Both executables built/installed successfully as0.5.120; completed build log
  preserved at chat work/build-0.5.120-completed.log. Live menu/relaunch still
  needs checking in121 (which includes these changes).
- Window move/resize bug reported: input.cpp keeps old ClipCursor rectangle/
  recenter point during resize. Separate following patch; bot visibility is
  next gameplay priority. No bot/UAV/weapon-switch change in120.
## 0.5.119: Native Options defaults test

- Development build A: hook verified native preferences constructor in SP/MP.
  Controller horizontal/vertical defaults100; audio defaults25 out of50,
  which is50% (native audio applies value*.02). F11 settings unchanged.
- Controller range150 and saved preference migration deferred to120;
  this development build is NOT the completed requested settings update.
  Add bounded controller widget diagnostics to identify its native range API.
- Justin clarified118 restart persistence is NOT tested yet. Full audio slider
  and original controller range in118 are expected:118 only added storage.
- User request:100%=level10,150%=level15, faster controller look like later
  Call of Duty games. Exact matching turn rates require gameplay calibration.
- Development A both executables built/installed as0.5.119; preserved build
  log work/build-0.5.119-A-completed.log. User confirms lower audio; native
  controller bars full, but original range adjustment still capped. Captured
  work/CoD3MP-0.5.119-native-defaults-test.log. Restart persistence untested.
- No bot/UAV/weapon-switch change. Bots are next priority after this patch.
## 0.5.118: Native profile settings storage

- One functional change: replace discarded XamUserWriteProfileSettings writes
  and the all-unset read stub with persistent per-user native profile storage.
  Use the title's verified 40-byte records, including scalar and binary data;
  reconstruct guest payload pointers when reading instead of saving addresses.
- Save to SavesRoot()/profiles/user-N.bin using a temporary file and atomic
  replacement. Bound counts/payloads, reject corrupt files, retain existing
  settings on partial writes, and release locks before completion callbacks.
- Validation: actual profile_store.h codec passes disk save/reload, supported
  types, binary/empty data, every truncated input, corruption/duplicate records,
  payload/count limits, and unchanged output on failed decode. Test source is
  in chat workspace work/test_profile_store.cpp. A second harness compiling
  the actual kernel_xam.cpp profile helpers/imports also passes40-byte guest
  layout, disk reload, size query/short buffer, pointer rebasing, user isolation,
  and failed write preserving prior data. Native menu/relaunch untested.
- Both executables built/installed successfully as0.5.118. MP visibly reached
  its menu; log confirms native15-setting read. Automated menu inputs were
  unreliable; native menu save/relaunch remains untested. Alt+F4 injection did
  not close the window, so the test process was stopped explicitly. This is
  not evidence of a normal-close regression or a successful close test.
- Scope split to respect one change per build: native volume first-run50%,
  controller first-run100%, and native slider150% are NOT implemented here.
  Native sensitivity fields verified at preferences+68/+72 (integer percent);
  preserve ADS sensitivity factor separately. Those defaults/UI changes follow.
- No new bot/UAV/weapon-switch changes. 0.5.117's drawable candidate has no
  live visibility confirmation; continue treating invisible bots as open.
## 0.5.117: Create each bot's native drawable human object

- 0.5.116 live test: class0, health100/state3, ownership allowance logged.
  Bot still invisible beside Justin; entity+536 drawable stays0. Ownership
  allowance was insufficient. UAV also still does not show this bot.
- One functional change: mirror native network player setup at823F818C and
  82400D40: lookup generic_human via824B5688(820653E4), then8257C090(entity,
  entry) creates a separate drawable object and rebuilds that player's rig.
  Run after placement, before the normal spawn finalizer, only if drawable0.
  Verify entry+4 nonzero as native script path824CDFAC does. Log factory
  entry and resulting drawable. No shared instance or copied host model.
- Weapon switching also reported broken with controller Y/Triangle AND mouse
  wheel. Respawn can equip Kar or pistol but switching stays stuck. Add only
  input-edge diagnostics identifying controller vs keyboard/wheel Y presses.
  No speculative weapon repair or UAV change in this build.
- Validation: native setup/factory checked, generic_human text verified with
  CoD3Scan. Both executables built and installed successfully as0.5.117.
  Visible body, camera and equipment need gameplay
  testing. Weapon switching and UAV remain open issues.
## 0.5.116: Allow native model selection for host bots without local sessions

- 0.5.115 live result: valid class0 reaches kit setup, bot alive, but Justin
  still sees no soldier and no UAV compass marker. Weapon remains0. The
  valid-class change alone did not resolve presentation/equipment.
- One change: allow registered, generation-validated bot handles through the
  model selector821FB3E0's local-ownership query824C3490 at return address
  821FB408 only. Native check requires one of four local session entries;
  bots have none. Other ownership/input/camera/network calls stay native.
- This lets the existing side-aware model definition setter824CB9E8 run,
  followed by normal accessory selection. No fake session, cloned model
  pointer, hardcoded soldier asset or blanket ownership override.
- UAV remains a separate issue:825049C8 requires recent ps+132 activity even
  with enemy_radar enabled. Weapon selection may also need server handling.
- Validation: exact query call site, handle generation and model definition
  setter checked against native code. Both executables built and installed
  successfully as0.5.116; model/camera gameplay
  confirmation required. This is a candidate, not a confirmed visibility fix.
## 0.5.115: Give bots a valid default class before spawning

- Live 0.5.114 test confirms native finalizer82211DF8 gives the bot health100,
  state3 and movement type0. Bringing it beside Justin confirms it remains
  invisible. Log: native kit setup receives class -1, no weapon, model pointer0.
- One change: initialize bot class selectors ps+1920/+1922 to0 instead of
  FFFF, the unselected join sentinel. Bots skip the local kit menu, so no one
  replaced that sentinel. Verified native getters820CE3D8/820CE538 and kit
  path8220D1C8 ->82206028. The native spawn finalizer still supplies the kit.
- Keep camera/team isolation and spawn finalization. No host model-pointer
  copying, manual model attachment or local menu callback.
- UAV activation was reported, but bot compass markers are unconfirmed;
  the current implementation enables native enemy_radar. Track separately.
- Validation: native field/kit call chain inspected. Both executables built
  and installed successfully as0.5.115. No gameplay/model confirmation yet; fresh
  match must verify valid class, model/weapon, normal camera and combat.
## 0.5.114: Run the native spawn finalizer for host bots

- Justin confirms 0.5.113 leaves his camera normal. Log confirms native and
  local camera FOV 90, but bot slot 4 remains health 0 / move type 6 / state 0.
- One change: after native bot placement, invoke the missing spawn finalizer
  at callbacks+3312 with the bot's handle and flag 0, matching offline spawn
  82406BB8. Placement 82593608 clears health; the normal game path follows it
  with this callback. Keep the 0.5.113 team/menu bypass and client isolation.
- Native default callback 82211DF8 performs player state/health/loadout setup.
  Log the callback address and resulting state. No blanket host copying or
  manual forced health/state writes.
- Validation: call signature/order checked against native 82406BB8 and
  824050B4. Both executables built/installed successfully; visible multiplayer
  menu startup passed. Automated menu inputs did not advance the menu, so
  living bot, combat and camera still need fresh-match gameplay testing.
## 0.5.113: Keep bot team/kit setup out of the local player's menus

- Justin confirms sniper ADS/L3 steady aim and sounds work, and Kangaroo
  exits work. Bot_add still zooms the host on 0.5.111; context-only fix did
  not solve it. Host FOV cvar90 and ADS/weapon survive the setup calls.
- One camera-focused change: bots set their server team and existing session
  side directly, using the two fields verified in native 823EA4E0, then use
  the existing server respawn path. Do not invoke the local side/kit menu
  callbacks for bots. Keep the default spawn kit. This removes the remaining
  local-menu side effects; gameplay confirmation is still required.
- bot_camera 1 (default) now includes direct team assignment/menu bypass and
  context isolation. 0 restores the legacy menu/context path for comparison.
- Observe actual native FOV calculation 82453810 and local-camera callback
  824674E0 every two seconds: return FOV, host ADS/flags, camera FOV/auto/tween.
  No forced FOV/ADS or broad memory restore. These traces distinguish native
  camera state from a rendering problem if the zoom persists.
- Validation: team fields and camera field accesses checked against generated
  native code. Both executables built and installed successfully. Fresh-match
  bot_add/camera/teams still need gameplay testing.
  User test on 0.5.112: bot_setup 3 then bot_add 1 left the camera normal,
  but the created bot stayed inactive (health 0, move type 6, state 0). This
  narrows suspicion toward team setup without proving the cause.
  Includes 0.5.112 contact-radius trial (18->13 observed); visual height pending.

## 0.5.112: Lower Kangaroo wheel contact height slightly

- Justin confirms right/passenger and rear/gunner dismounts now work well
  on 0.5.111. The vehicle still appears about 4-6 inches above the ground.
- One change: give each Kangaroo a private Horch physics-preset copy before
  native initialization, reducing its contact wheel radius by five game units.
  The original mp_horsch preset remains untouched for other vehicles.
- Field verified with the original physics loader/table: wheel_radius +20,
  preset name +112, final constructed float +200. Copy only 204 verified bytes
  into a 208-byte private allocation. Validate radius and keep it at least 8.
- `kangaroo_drop 0` restores original contact height at next map start;
  default 5, adjustable 0-8. Use map_restart after changing it. No accumulated
  origin offset; model, collision body and passengers follow native physics.
  Handling, tread logic and the confirmed exit placement code are unchanged.
- Validation: both executables built/installed. Visible Merville run confirms
  the private preset initializes with contact radius 18 -> 13. Actual ride
  height and driving still require gameplay confirmation.
  Bot/camera work follows; bot camera status on 0.5.111 requested from Justin.

## 0.5.111: Let scoped L3 reach native hold-breath without a sprint latch

- Justin requested sniper steady aim with L3, and clarified: walking forward
  keeps ADS up; sprint is disabled while scoped; only aim-out leaves the scope.
- Original MP default controls bind L3 to +sprintbreath. The port's
  tap-to-sprint filter cleared L3 whenever ADS was held, preventing native
  hold-breath. Preserve the physical L3 press during ADS, cancel the sprint
  latch, and report ADS forward movement as walking. Release L3 stops breath;
  leaving ADS does not reactivate a previous sprint latch. ADS itself is unchanged.
- Reuse native steadying, breath timing, recovery and sounds rather than
  modifying shared weapon/sway definitions. Applies to multiplayer; campaign
  has different binocular/hold-breath bindings and retains its input mapping.
- `scope_breath 0` restores the old L3 filter; 1 enables the fix (default).
- Validation: compiled input tests pass for scoped standing/walking L3,
  ADS cancellation of a hip sprint, release, aim-out, hold-input mode and
  rollback. Both executable builds passed and installed as 0.5.111. Initial compile
  failure from missing input.h in the console handler was fixed. Live sniper sway and all newly
  changed game behaviors still require gameplay confirmation.

## 0.5.110: Bots seek cover, aim at head height and steer around obstacles

- Justin requested cover, head aim and better directions toward objectives
  and enemy players. Add one optional tactical decision layer, enabled by
  default; `bot_tactics 0` restores the previous brain, 1 enables it.
- Aim near the target's eye/head height, accounting for crouch height. Check
  visibility every 200 ms, require reaction time after visibility returns,
  and require pitch as well as yaw alignment before firing. Blocked enemies
  no longer pull bots away from their Domination objective.
- Full-body collision sweeps and grounded landing checks choose local route
  steps around walls. Reject void and steep/drop paths; stalled bots commit
  to a lateral escape. Apply movement relative to view direction while aiming.
- Injured/low-health bots look for reachable nearby shelter with a blocked
  enemy-to-crouched-body ray, travel there, crouch, and reassess after four
  seconds. Route planning is throttled; this is local collision steering,
  not a map-wide navigation mesh. Existing Domination goals supply objectives.
- Validation: native trace layout reviewed. Compiled planner tests pass for
  wall detour, committed escape, void/steep rejection, reachable shelter and
  refusal to invent cover in open space. Both game builds passed and installed; live bot
  behavior/camera and Kangaroo exits still need playtesting. Sniper L3 steady
  aim requested and queued for its own build.

## 0.5.109: Isolate bot setup from the host's current-player context

- Justin requested the persistent zoom after bot_add be investigated before
  bot improvements. Prior FOV resets and skipping kit selection did not fix it.
- The normal bot ClientThink already selects the bot in engine CurrentClient
  (82BCBE9C) and restores it; creation, side/kit callbacks and respawn did not.
  Scope those setup calls to the bot too, restoring the previous context on
  every return. This removes a host-context leak; camera causality needs
  gameplay confirmation rather than another assumed cg_fov reset.
- Log the stage/slot/restored context and host ADS/weapon before and after.
  `bot_camera 0` restores the old setup context; `bot_camera 1` enables the
  isolation (default). No camera state is continuously forced or overwritten.
- Validation: source paths reviewed; both executables built and installed. Kangaroo normal fix from
  0.5.108 included; bot cover/head aim/routing remains the next separate build.

## 0.5.108: Read the ground normal instead of the ground height

- Justin confirmed 0.5.107 still leaves every passenger/gunner exit in the
  hull or below the vehicle. Log shows the hook and remembered direction
  work, but every candidate fails the grounded-landing check.
- Fix one field: native trace +0 is hit position, +16 is plane normal.
  Normal Z is +24, not +8 (hit height). On Merville the negative hit height
  rejected valid flat ground. Verified native 824A0468 constructs +0 as
  start + travel * fraction and copies the plane separately to +16.
- Add landing trace values to the log. Preserve entry-direction handling,
  collision clearance, native placement fallback, driving and ride height.
- Validation: native producer reviewed; both builds passed and installed. Actual dismount
  gameplay remains to be checked. Bot upgrade requested separately.

## 0.5.107: Finish dismounts through the actual vehicle exit path

- Justin confirmed right-side and rear turret dismounts still end inside
  the hull on 0.5.106. The live log contains unlink caller 8257AD34;
  neither caller intercepted in 0.5.106 occurred. No placement ran.
- Apply grounded placement after the entire native 8257ABF0 dismount,
  after its origin setter, only once the vehicle flag and link clear.
  Remember the entry tag: left returns left, right returns right, rear
  turret entry returns behind the hull. Blocked landing tries an alternate.
- Keep the existing trace clearance and native-origin setter. Console
  kangaroo_exit 0 restores native behavior; 1/2 provide a fallback direction
  if no entry tag was recorded. Ride height and treads are unchanged.
- Validation: native function and live caller reviewed; both executables built
  and installed. Merville startup passed. Background test stopped at the
  user's report of audible audio without a visible window; it did not reach
  the dismount. Right-side and rear seat placement still need live testing.

## 0.5.106: Give Kangaroo dismounts a grounded side landing

- Justin on 0.5.105: driver-side entry and exit no longer dip under the
  map. The exit clips slightly; passenger-side and rear-turret exits put
  him in the middle of the hull. The hull also floats a little too high.
- One change: after the native exit-completion unlink, find a landing
  outside the hull with clearance for the player's full collision box.
  Prefer left, sweep across above the hull and down onto walkable ground;
  try farther points and then right if blocked. Reject solid starts, walls,
  steep landings, missing ground and landing on the Kangaroo itself. Use
  the game's player-origin setter to keep player, entity and network origin
  together. If no candidate is clear, retain the native position.
- The native trace layout was checked against generated code; scratch
  vectors/filter/result and a guest call stack have their own allocation.
  Only the verified exit-completion callers 0x82585120/0x82585374 move the
  player. Other Kangaroo unlinks log their caller/seat for diagnosis.
- Console: `kangaroo_exit 0` restores native exit placement, `1` prefers
  left (default), `2` prefers right. 0.5.104's stable entry rotation stays.
- Validation: real Windows/MP header syntax check and both builds passed. Installed 0.5.106. Scripted Merville startup passed, but teleport/use did not reach the vehicle, so native
  seat exit tests remain pending. Ride height and bullet flicker are separate open
  items; driving, tread movement, wheel posing and ride height are unchanged.

## 0.5.105: Closing the game also ends its console and audio

- Justin reported that closing the game window left its console and music
  running. Reproduced on 0.5.103: CoD3MP remained alive after Alt+F4.
- WM_CLOSE previously stopped only the window thread. The guest and audio
  threads kept running. It now flushes the log and calls ExitProcess(0),
  matching the existing campaign/multiplayer handoff, without the diagnostic
  exit path's "Press Enter" prompt. Applies to both executables.
- Validation: both executables built and installed. Clicked X on campaign
  and multiplayer 0.5.105; each log recorded "window: close requested;
  ending the game process", and process checks confirmed each executable
  exited. Borderless preference was restored after testing.
- 0.5.104's entry-orientation change is included; Kangaroo entry/exit gameplay and bullet flicker remain
  open. No vehicle-driving, tread, exit-placement or renderer change here.

## 0.5.104: Stabilize the Kangaroo's side-entry orientation

- Justin confirms 0.5.103 drives well and its treads work. Entry dips through
  the ground; exit leaves the player inside the hull. Bullet holes still flicker.
- First isolated change: the left/right entry stand-ins are road wheels. The
  player-link matrix follows their rotating orientation. At sub_82571A58,
  retain the entry position but replace only its nine rotation floats with
  the hull's angle-derived orientation. Wheel and tread poses are untouched.
- Console: `kangaroo_entry 0` restores the old link orientation immediately;
  `kangaroo_entry 1` enables the fix (default).
- Validation: initial attempts hit inaccessible cached Ninja/Python paths;
  local caches now use VS Ninja and bundled Python. A duplicate declaration
  compile error was removed. Both executables built and installed; MP startup smoke test passed.
  Entry/exit behavior remains unverified. This build
  does not change exit placement or bullet-hole rendering.

## 0.5.103: The Kangaroo's treads roll going straight

- **Justin, on 0.5.102 (`kangaroo_build 2`):** it drives again. The treads
  move when turning but not going straight.
- **Now:** after the game's tread distance update (`sub_82568600`), the
  distance the Kangaroo moved along its facing that frame is added to both
  treads' distances (record +1308 right, +1312 left), so they roll going
  ahead and back. Untested; if they run too fast or slow, scale `ahead` in
  that hook.
- `kangaroo_build 2` (Horch physics) is the default, as in 0.5.102.

## 0.5.102: The Kangaroo back on the Horch's physics

- **Justin, on 0.5.101:** he can get in, but it still slides back and forth
  on hills and doesn't drive properly; the camera goes under the map for a
  moment before he can drive.
- **Now:** `kangaroo_build 2` is the default: still made a Kangaroo from the
  spawn (its own model, box, six wheels, side doors, tread scroll), but with
  the Horch's physics preset (`mp_horsch`), which drove "really nice" in
  0.5.81 to 0.5.98. The tank preset (`mp_tankdefault`) on a four wheel kind
  is what slid on hills. `kangaroo_build 1` gives the tank preset back.
- **Untested:** the camera dipping under the map when getting in. If it
  stays, compare the cameras (kind +432..+499) with the Horch's.

## 0.5.101: The Kangaroo drives as in 0.5.99 again, with its side doors

- **Justin, on 0.5.100:** with type 3 (tracked physics on a four wheel kind)
  the driving was broken: it rocked back and forth on hills and the treads
  didn't drive properly. With `kangaroo_type 2` (a whole tank kind) he
  couldn't get in at all, not even at the gun.
- **Now:** type 1 is the default again (0.5.99's driving, which worked but is
  slow on hills), keeping 0.5.100's side-door fix and 0.5.99's hull hitbox,
  tank preset and tread scroll. Types 2 and 3 stay in the console for testing.
- **Why 3 broke:** some of the game's tank driving reads the kind's type, not
  only the physics flag, and a tank's spawn fills values (record +0x414/+0x418
  from kind +708/+712, read only for type 2) that a type 1 spawn leaves empty.
  Tracked physics on a type 1 kind is half one, half the other.

## 0.5.100: The Kangaroo's side doors back; it drives as a tracked vehicle; a handoff file

- **Justin, on 0.5.99:** the Kangaroo's hitbox is "nice and fat again" as it
  should be. But he could only get in at the gun on the back (then switch to
  the driver's seat), it drove slowly, and it got stuck climbing out of a
  ravine. Bullet holes still come in slowly and flicker.
- **The doors:** a vehicle's ways in are looked up once, as it is set up. The
  Kangaroo's model has no `tag_enter_left/right`, so 0.5.78 answers those with
  stand-ins, but only for entities known as Kangaroos. 0.5.99 set it up as a
  Kangaroo before it was known as one, so the side ways in found nothing. It is
  now known as a Kangaroo before it is set up.
- **Driving:** the tanks' physics preset was being driven by the four-wheeler
  code (its physics object lacked the tracked flag 0x20, which a type 2 kind
  gets). That flag is now set on the Kangaroo's physics object, so the driving
  (`sub_82423660`) and the posing (`sub_8242B938`) treat it as tracked, like the
  campaign's. Its kind stays a four-wheeler's, so its seats and ways in don't
  change. `kangaroo_type 1` gives the four-wheeler driving again; `2` makes the
  whole kind a tank (untested). Both apply from the next map start.
- **Handoff:** docs/HANDOFF.md says what this project is, how it's built and
  tested, where everything is, what is done and what is open, so another
  assistant can pick up where this one left off.
- **Bullet holes:** still open. 0.5.99's in-frame buffer watch never fired in
  testing (no "written within a frame" line), so the marks aren't being
  rewritten inside a big buffer mid-frame. That fix stays in (harmless); the
  cause is elsewhere. Next suspects are in HANDOFF.md.

## 0.5.99: The Kangaroo made a Kangaroo from the start; bullet holes drawn as they're written

- **Justin, on 0.5.98 with `kangaroo_tread 10`:** no crash. The treads roll,
  nicely when turning, hardly when driving straight. Its collision feels
  smaller than the hull, and bullet holes still flash before they settle.
- **Why the Kangaroo was still a Horch underneath:** a vehicle is set up as it
  spawns (`sub_8257E3A8`): its physics object comes from its kind's
  vehiclePhysics preset (+540: the Horch's `mp_horsch`, the Sherman's
  `mp_tankdefault`) and its body, wheels and size from the model it has then.
  The Kangaroo was made over from the Horch *after* that, so it kept the
  Horch's physics body and car physics.
- **Now:** as the map's vehicles spawn, the Horch that will be the Kangaroo is
  given the Kangaroo's model and kind first. It is set up as a Kangaroo from the
  start, with the tanks' physics preset (the Kangaroo is a Sherman's hull, and
  the campaign's tracked vehicles use `tankdefault`). Its box is its own
  model's (282 x 128 x 124). Log: "kangaroo: the map's mp_horch made a Kangaroo
  as it is set up" and "kangaroo: set up: physics object ..., preset ...".
  `kangaroo_build 2` keeps the Horch's physics; `kangaroo_build 0` goes back to
  0.5.98's way.
- **Treads going ahead:** each tread is moved by the distance driven, scaled by
  the kind's texureScrollScale (+76). The Kangaroo had the Horch's 1.0 instead
  of the tanks' 5.5, so going straight its treads ran at a fifth of a tank's.
  It now has the tanks' scroll. The game's own tread poses (`kangaroo_tread 10`)
  are the default now.
- **Bullet holes:** a vertex buffer over a quarter megabyte was only looked at
  by the first draw of each frame. The game writes its dynamic geometry (the
  marks among it) into one big pool piece by piece through the frame, so later
  draws got what that place held a frame or more before: marks flashing until
  the pool's layout settled. Now such a buffer's pages are watched from the
  frame's first look, and each later draw sends up only the pieces written
  since. This is in each player's own renderer, so everyone on this build sees
  marks as they're made. `COD3_NOLIVEBUFFERS=1` turns it off.

## 0.5.98: The crashes found: 0.5.93's weapon look wrote past the weapon list

- **Reproduced:** Claude started a Merville match on 0.5.97 and the game died
  loading it. The log, right before the garbage reads began: "marks: 91
  weapons looked over, 466 surface impacts given from others".
- **Why:** 0.5.93's bullet-hole helper went over 255 places of the game's
  weapon list, past its end, took whatever lay there for weapons, and wrote
  impact records into it: 466 pointers into the game's memory. On Justin's
  first 0.5.94 run what lay past the list didn't look like weapons, so
  nothing was written and it ran fine; after that it did, and the game's
  memory was wrecked (0.5.95, 0.5.96 and 0.5.97 all died this way, at the
  side menu or the spawn, each time somewhere different). 0.5.96's renderer
  undo and 0.5.97's tread undo were aimed at the wrong things.
- **Fixed:** that look and its filling in are gone. It was never needed:
  every real weapon already has all 22 surfaces' impacts. Only the read-only
  "marks: in hand ..." line stays. 0.5.94's bullet-hole fix (the renderer
  hashing the marks' pool whole) stays.

## 0.5.97: Back to 0.5.94's Kangaroo (the crash at the spawn)

- **Justin, on 0.5.96:** a garbled picture, then the game closed the moment
  he spawned. The log: a null call in the model code's chain (sub_8243E198
  and on), then a read of guest memory that isn't there. So 0.5.95's crash
  wasn't the renderer's buffer looks (0.5.96 undid those and it still came).
  The other thing 0.5.95 changed was giving the Kangaroo's treads and
  sprockets the game's tank poses again.
- **Undone:** the Kangaroo's four tread and sprocket parts are held still
  again by default, exactly as in 0.5.94, which ran without trouble. The
  console's `kangaroo_tread 10` gives the game's poses (with the wheel list
  put right) for trying by hand. Everything now is as 0.5.94 was.

## 0.5.96: 0.5.95's crash undone

- **Justin, on 0.5.95:** the game closed just as he was about to pick a side.
  The log: a read of guest memory at 0x37630000, which isn't there, from the
  renderer. 0.5.95 had every later draw of a frame look over a vertex buffer
  of up to a megabyte again, the whole of the most it had ever been. A buffer
  the game since made smaller, or moved, ran that look off the end of memory.
- **Undone:** buffers over a quarter megabyte are looked at once a frame
  again, as in 0.5.94 (which ran without trouble). 0.5.94's whole-hash of
  them stays, and 0.5.95's Kangaroo treads stay.

## 0.5.95: The Kangaroo's treads run; bullet holes without the flicker

- **Justin, on 0.5.94:** the Kangaroo's treads and wheels sit exactly right
  now (the log: its wheel bones were the Horch's 4 11 9 10, bone 4 being
  tag_tread_left; now its own 7 11 5 9). But they don't move. And bullet
  holes come much sooner, but still flicker a little before they settle.
- **Treads:** the game's own tread and sprocket poses are given again on the
  Kangaroo (the treads' texture runs with how far each side has driven: the
  v_tracked_tread shader reads the tread bone's pose; the sprockets turn).
  They were only ever thrown off by the Horch's wheel at bone 4, now gone.
  `kangaroo_tread 9` holds all four still as 0.5.94 did, if this looks wrong.
- **Bullet holes:** a vertex buffer the game rewrites between the draws of one
  frame is looked at again by each later draw, but only if it was under a
  quarter megabyte. The marks' pool is bigger, so the frame's later draws
  took its first copy while marks were being added. Now up to a megabyte.

## 0.5.94: Bullet holes show at once and stay (campaign and multiplayer)

- **Justin, on 0.5.92 (0.5.93 wasn't built yet):** in multiplayer the bullet
  holes do appear, but slowly, only after a while, and they flicker in and
  out, as in the campaign. So the marks are made; it's how they're drawn.
- **Why:** the renderer keeps a copy of each of the game's vertex buffers and
  only sends one again when it sees it change. A buffer of a quarter to one
  megabyte that had been still for eight looks was then checked by
  sampling sixty four words (or left to the page watch). The game keeps
  every bullet mark's few vertices in one pool of that size, and a new mark
  is a handful of them: the samples missed it, so a mark showed only when a
  sample happened to fall on it ("after a while"), and the draw flicked
  between the old pool and the new.
- **Fixed:** a vertex buffer under a megabyte is always hashed whole (every
  byte, each look) and never left to the page watch. Bigger ones are
  compared whole already. This is shared by the campaign and multiplayer.
- 0.5.93's changes (the Kangaroo's wheel bones, the weapons' impacts) are in
  this build too: it's the first build with them.

## 0.5.93: The Kangaroo's tread was being turned as a wheel; weapons' bullet impacts filled in

From Justin's two dossiers (kangaroo-technical-dossier, bullet-impact-dossier).

- **The spinning left tread, found.** The Kangaroo is the map's Horch made
  over: its model is swapped in and it's posed as a four-wheeler. A
  vehicle's wheel bones are kept as bone numbers at +36 of its physics
  object, found once by name in the order of the game's wheel-name table
  (0x825D5C6C: front left, front right, back left, back right, middle left,
  middle right), and turned and sprung each frame by `sub_8242B938`. They
  were found on the Horch's model. On the Kangaroo's skeleton bone 4 is
  `tag_tread_left`, so the Horch's wheel at 4 turned the whole tread round:
  the spinning tread in every video. Every earlier tread change moved the
  wrong thing.
- **Fixed:** each frame the Kangaroo's wheel list is set from its own
  skeleton by name, so its own road wheels turn and the treads aren't
  touched by it. Log: "kangaroo: wheel bones were ..., now ... (its own by
  name)". The tank tread mover now leaves all four tread and sprocket parts
  alone on the Kangaroo (0.5.90's always-posed left tread is undone).
  `kangaroo_tread 1` still gives the game's own way for comparing.
- **Bullet holes in multiplayer.** Each weapon keeps one impact record for
  each of 23 surfaces (concrete, metal, wood, plaster, brick and so on) at
  +2284, found by "<weapon>_<SURFACE>" when the weapon loads
  (`sub_8249A380`); the record names the impact effect and the mark. A
  weapon with none for a surface never leaves a mark there. Now, once a
  map's weapons are loaded, any weapon missing a surface's record is given
  another weapon's (an original rifle's first), and each weapon's count is
  logged ("marks: weapon ... has N of 22 surfaces' impacts"). Every five
  seconds the log gives the weapon in hand's concrete, metal, wood, plaster
  and brick records, to show whether a missing mark is missing upstream
  (no record) or in the drawing.

## 0.5.92: Hunting the bots' zoom and the Kangaroo's tread

- **Justin, on 0.5.91:** adding bots still zooms his view in; the tread is
  still off. The log shows 0.5.91's view fix never ran: the console's
  bot_add doesn't go through the session path it was put in. The bots are set
  up by two of the game's own menu callbacks instead (the side switch,
  sub_823EA4E0, and the kit menu's, +3328 of 0x82BCBF64), each given the
  bot's handle, and the host's weapon is seen pulled down and raised again
  just then. Those menu callbacks are the host's own menus', and look to
  act on his own view too.
- **Tried first:** the kit callback is left out for bots (they keep the
  kit they're given anyway). The console's `bot_setup N` changes it live:
  1 (the default) kit left out, 2 side switch left out, 3 both, 0 neither.
  The host's view is now logged (and his field of view set again) from a
  bot's first moments, whichever way he's added.
- **The tread:** every five seconds the log gives the Kangaroo's tread,
  sprocket and middle wheel bone matrices ("kangaroo: bone tag_tread_left
  ..."), parked or moving, so the bad tread can be compared with the good one
  number by number.

## 0.5.91: Your view put back after adding bots

- **Justin, on 0.5.90:** adding bots still leaves his own view zoomed in with
  no gun showing, and aiming zooms it *out* while held. So his field of view
  became narrower than his sights' own: the bots' setup resets his view
  settings (each bot's session player is a copy of his).
- **Fixed (first try):** his field of view (the F11 menu's) is set again one
  second and three seconds after the bots are given out.
- **To find the rest:** for twenty seconds after, the log gives his view's
  state each second ("bots: host view Ns after the bots: cg_fov ...,
  cg_fovscale ..., cg_drawgun ..., in hand ..., +172 ..., +220 ...").

## 0.5.90: The Kangaroo's left tread always posed; bots sprint and take a moment to react

- **Justin, on 0.5.89:** the wheels show but don't turn, and one tread still
  spins off as in the video. The log shows the wheeled-vehicle posing never
  ran for it ("posed as a wheeled vehicle" never written): the Kangaroo
  isn't posed through that tank branch at all.
- **What the tests add up to:** the left tread sits right only with the
  game's pose (0.5.85: both posed, left right and right off; 0.5.87's video:
  left posed but only while moving, so parked it lay thrown off; 0.5.88 and
  0.5.89: left never posed, always thrown off). The right tread sits right
  only without one. The game gives that pose only while the vehicle moves
  (`sub_82434918`, the entity's speed at +716 above nought).
- **Fixed:** the left tread is always given the game's pose (a parked
  Kangaroo's speed is made a hair above nought for that one call); the right
  tread and both sprockets are left where the model has them. Log:
  "kangaroo: left tread always posed, right tread and gears kept in place".
- **Bots sprint** (0x4, the button the host's commands carry while running
  flat out) when running to a flag, or toward an enemy over 1200 units off
  while not shooting.
- **Bots take a moment to react:** on seeing a new nearest enemy he waits
  350 to 650 ms before firing, instead of shooting the instant he turns.

## 0.5.89: The Kangaroo's treads stay on its hull, its wheels turn; the bots' real buttons

- **Justin, on 0.5.88:** the left tread is still thrown off exactly as in
  the video, and the wheels show but never turn. So the tread mover wasn't
  the only thing moving it.
- **Why:** each frame `sub_8242B938` poses a vehicle's parts (its physics
  object: the entity at +624, flags at +640). For a tracked vehicle (flag
  0x10, the Sherman's kind the Kangaroo's is copied from) it calls
  `sub_8241E908`, which lays each tread along a line through three of the
  vehicle's wheel points (+800 to +820) and writes the tread's bone outright.
  The Kangaroo's record is built from the Horch's, so those points aren't its
  own, and the tread is laid along the wrong line: off the front and over
  the top. Without 0x10 the same routine poses each wheel instead (turning
  and springs, `sub_8241D168`), as for the jeeps.
- **Fixed:** the Kangaroo is posed as a wheeled vehicle (0x10 cleared just
  for that call): its six road wheels turn, its treads stay where its model
  has them. Log: "kangaroo: posed as a wheeled vehicle ...", with its wheel
  bones. `kangaroo_tread 1` gives the game's own way back, `7` the tank's
  way with only the tread aiming left out.
- **Tested 0.5.88 on Merville:** the Kangaroo's treads and sprockets are no
  longer moved ("kangaroo: treads and gears kept in the model's own place").
  Pressing each key while watching the host's commands in the log gave the
  real buttons: the stance key 0x2000 (a tap crouches, held it tries to go
  prone: "Prone blocked"), the frag grenade 0x80, the smoke grenade 0x2.
  0x4000 is in every command. 0.5.88's guesses (0x200 and 0x4000) were wrong,
  so its bots never crouched and their "grenade" did nothing.
- **Fixed:** a bot taps the stance key for one frame when he wants to go
  down or come back up (held, it would put him prone), and comes back
  standing after a death. His grenade is 0x80, held a third of a second and
  let go.
- **The Kangaroo's hitbox:** its entity's words around the box (+200 to +336)
  match the tanks' in everything but the box and where it is (+224 1, +228
  0x10 in all three), so walking into its hull isn't its box or its contents.
  The tanks' bodies stop players by their models' collision; next is
  whether the Kangaroo's model (from the campaign's archive) has any.

## 0.5.88: Bots crouch, throw grenades and take flags; the Kangaroo's treads stay on

- **Bots crouch.** At range (over 700 units) a bot now and then stops and
  crouches to shoot, about a third of the time, a couple of seconds at a go.
  Holding a flag, he crouches on it. The crouch button is taken as 0x200, as
  in the later games of the same engine family (the host's own commands show
  0x200 among their buttons). Log: "bots: slot N crouches" / "stands".
- **Bots throw grenades.** At an enemy 450 to 1400 units off that he's facing,
  every 18 to 25 seconds (none in his first seconds): the grenade button
  (taken as 0x4000) held for a third of a second, aimed a little up, then let
  go. Log: "bots: slot N throws a grenade at D".
- **Bots play Domination.** On the host, with the flags placed, each bot heads
  for a flag his side doesn't hold (nearest first, the bots leaning toward
  different flags so they spread out). Once every flag is his side's, he
  defends one the other side is taking, or stands at the nearest. He fights
  instead when an enemy comes within 900 units, and on a flag he stays put and
  fights from there. `Domination::BotGoal` in dom.cpp.
- **Each bot a little different:** his slot shifts his strafing, crouching and
  grenade timings, so they don't all move in step.
- **The Kangaroo's treads and sprockets stay on.** Justin's video of 0.5.87:
  with the right tread held, the left tread was thrown off whole (out past
  the front and over the top) and a drive sprocket stuck out of the hull at
  the back. The game's tread mover (`sub_82434918`) moves the tanks' tread
  and gear parts to make them roll, and on the Kangaroo's model it carries
  those parts bodily out of place. Now none of the Kangaroo's four (both
  treads, both sprockets) is moved by it: they stay where the model has
  them. Log: "kangaroo: treads and gears kept in the model's own place".
- **For trying other ways:** the console's `kangaroo_tread N`: 0 the above
  (the default), 1 the game's own for all four, 2 to 5 the right tread moved
  along its bone's -X, +Y, -Y or +Z instead, 6 the left tread's movement
  given to the right.
- **For the hitbox:** the vehicle probe also logs each vehicle's entity
  words +200 to +336 (the Kangaroo's, the Panzer's, the Sherman's) to find
  why players walk into the Kangaroo's hull but not the tanks'. Its box is
  already like theirs (-141 -60 0 to 141 68 124).

## 0.5.87: The campaign starts again; the Kangaroo's right tread kept in place

- **Justin's test of 0.5.86:** the right tread still sticks up and out of
  the hull when the Kangaroo drives. Copying the left side's distance didn't
  help, so that copy is gone again.
- **Why:** while a tracked vehicle drives, `sub_82434918` gives each tread
  and gear tag a moved pose. The Kangaroo's `tag_tread_right` isn't set up
  like the tanks', so the moved pose throws it out of place.
- **Fixed:** on a Kangaroo that one tag keeps the model's own pose, as it
  does when standing still. The left tread, the gears and the wheels move as
  before. Its log line: "kangaroo: right tread kept in the model's own place".

- **The campaign starting again.** Justin found the campaign stuck at its
  opening movies (CoD3.log: drawing stops 8 s in, all threads waiting). Texture
  dumping was on for the menu upscales, and a playing movie makes a new
  640x360 or 1280x720 texture every frame. Writing each one out held up the
  drawing thread until the movie player locked up. Movie frames are never
  dumped now, and a run dumps at most 3000 textures. Dumping is also switched
  off in CoD3Recomp.ini (texture_dump = 0). Tested: with it off, the campaign
  reaches its main menu and a mission starts.
- **The last five menu banners upscaled** (1024x512: 98378C, AD18FF, B9187D,
  D1B1BE, F9EE7F), 4x with Real-ESRGAN like the rest, in textures\replace.

## 0.5.86: The Kangaroo's right tread in line

- **Justin's test of 0.5.85:** the treads now show. The left tread sits
  right, but the right one is out of place.
- **Why:** the game moves each tread bone by how far that side has driven
  (`sub_82434918`, called while the model is drawn: left from the vehicle
  record's +1312, right from +1308, times 0.0045). The Kangaroo uses the
  Horch's record, and a four-wheeler keeps something else at +1308.
- **Fixed:** every frame, the Kangaroo's right-side distance is set to its
  left side's. Its log line: "kangaroo: right tread's distance ... set to the
  left's ...".

- **Killstreaks for the other players.** The host counts everyone's kills, but
  only the host has the killstreak box. Now when another player earns
  artillery or the Typhoon, the host arms it for him at once. He's given the
  spotter's binoculars and they're put in his hands (his other weapons kept).
  He fires them at the spot, as the host does. His UAV still can't show: the
  radar is drawn on each player's own machine.
- **His killstreak box:** while he holds those spotter's binoculars, his own
  machine shows the killstreak box with the strike waiting ("MARK THE SPOT:
  fire the binoculars"). His kill count and picking streaks stay on the
  host's side for now.

## 0.5.85: The Kangaroo's treads and wheels: their shaders found

- **Why the treads and wheels never drew.** The Kangaroo's model draws its
  treads with `v_tracked_tread` and its wheels with `v_tracked_wheel`. Its own
  archive (`sp\mace2\kangaroo.cod`) doesn't have them; in the campaign, Mace 2
  loads them from its `vehicles.cod`. Merville's and Poissons' archives do have
  them (the Sherman and Panzer use them). But an archive only looks for its
  assets in its own list of archives, and the map's archive wasn't in the
  Kangaroo's list. So both shaders were never found, and those parts weren't
  drawn.
- **Fixed:** the map's own archive is now added at the end of those lists. On
  Merville and Poissons, the Kangaroo's archive now loads after the map's (from
  the vehicles' setup) instead of before it. Its log line: "vehicles: mp_merv's
  own archive (8) is in: the Kangaroo's after it...".

- **The spotter's round, written straight in.** Tested 0.5.84:
  - The box now works: d-pad down cycles UAV â†’ Artillery â†’ Typhoon â†’ UAV, and the UAV call works ("UAV ONLINE", 30 seconds).
  - The artillery call brings the spotter's binoculars up and the strike stays ready (no longer spent early). But they still show 0 rounds and won't fire: the script's clip call doesn't take for binoculars.
  - Now, while a strike is armed, one round is written into his clip and his ammunition directly, as the launcher's rockets are.
- **The F11 box at the top left:** that's "Renderer statistics" in F11 â†’
  Graphics. Untick it to keep only "Show FPS". (Setting, no code change.)

## 0.5.84: The strike spent only when it lands; a round in the spotter's binoculars

- **Tested 0.5.83:** the box called the artillery and the dispenser started.
  But the dispenser sets its own first clock as it starts, and the box took
  that for the strike being called. The charge was used up and the rifle came
  back before the binoculars were up. When they did come up (a second call),
  they had 0 rounds and would not fire.
- **Fixed:** a call is now spent only when its barrage actually comes (the
  barrage hook marks it fired). Until then the strike is kept ready, and every
  quarter second the spotter's binoculars (`mp_binoculars`) are given one round
  in the clip and one spare. Before, the round was set once, as the weapon
  switch began, and didn't stick.

## 0.5.83: Strikes fire from the box; the Kangaroo on its own curve

- **Strikes called from the box.** 0.5.82 armed the artillery but the spotter's
  binoculars came out empty and nothing fired. The ready-time variable outlives
  the scout's dispenser thread; the box read a stale value and started no
  dispenser. Now every call from the box starts the dispenser and puts one round
  in the spotter's binoculars (the mark). Artillery and the Typhoon strafe both
  go through this.
- **The Kangaroo's own kind** now also takes the game's own Kangaroo subtype (8)
  and the Kangaroo driving curve (`vehicleCurveFile` = Kangaroo; `Kangaroo.bvce`
  is in `mp\global.cod`). It also loads the campaign Kangaroo vehicle file when
  its archive has one: speed, bounds, seats, tread scroll and cameras.
- **New console command `kangaroo_type 1|2`:** the Kangaroo drives as four
  wheel (1, the default; it works) or as a tank (2: tracked, to test whether the
  treads draw and scroll). Takes effect from the next map start.

## 0.5.82: The killstreak box; the Kangaroo's own kind, speed and box

- 0.5.81's test (Justin): the Kangaroo drives "really nice" and its gun
  works, but it still drives too fast for a tank's hull, parts of it can
  still be walked through, and no treads or wheels are seen. 0.5.81 set its
  box before its model: setting the model sets the box from the model.
- The Kangaroo's box (250 long, 112 wide, 95 high) is now set after its
  model.
- The vehicle file's fields are named by the game's own table of them
  (0x825FE3C8): type +32, subtype +34, numSeats +68, hudIndex +72,
  texureScroll +40 and texureScrollScale +76, maxSpeed +80, accel, rotRate,
  bounds radius, height and length +116 to +124, health +128, the cameras
  +432 to +499, the seats' interaction file +644. The game's subtypes:
  Jeep, Motorcycle, Truck, Wagon, Halftrack, Horch, Sherman and Kangaroo:
  the Kangaroo was meant for multiplayer.
- The Kangaroo's own archive's vehicle files are now read in the map's
  start as Merville's are: the campaign's "Kangaroo" file. The Kangaroo is
  its own kind, mp_kangaroo, a copy of the Horch's file (its seats'
  interaction and animations) given the campaign Kangaroo's speed,
  bounds, health, seats, cameras and treads' scroll (the Sherman's driving
  and bounds and 8 seats when the campaign's file is not there). The real
  Horch keeps its own.
- The log gives, a few seconds in, the Kangaroo's and the tanks' boxes as
  linked, their model objects' first words (to find why the Kangaroo's
  treads and wheels are never drawn) and the tanks' kind files.
- The killstreak box (as in Call of Duty 4 and after), on the host's
  screen: streaks earned are kept, listed at the bottom right with the
  kills in a row, the picked one framed. D-pad down picks the next, d-pad
  right calls it. The UAV goes up at once; the artillery and the Typhoon
  take the spotter's binoculars out (mp_binoculars: the class binoculars,
  mp_binoculars_nospot, cannot mark a spot, which is why a strike earned as
  a rifleman could not be called) and the strike falls where they are
  fired, then back to the main weapon. D-pad up now puts the binoculars
  away as well as taking them out (d-pad down still does when there is no
  streak to pick).

## 0.5.81: The Kangaroo's own box; its seats and whereabouts logged

- 0.5.80's test (Claude, then Justin, System Link War on Merville): the
  Kangaroo (the map's Horch with the Kangaroo's model and 0.5.78's tag
  stand-ins) can be got into at its side, drives "really nice", and its gun
  can be manned and fired. Parts of it could be walked through: its box was
  the Horch's (198 long, 79 wide, 76 high). The driver sits deep in the hull,
  the view nearly black. Still no treads or wheels to be seen.
- Its box is now its hull's: 250 long, 112 wide, 95 high (the entity's mins
  at +240 and maxs at +256, set before the map start links it).
- Each moved vehicle's kind file is logged word by word (with the text its
  words point at): its seats, their tags and animations, to give the
  Kangaroo its six passengers next.
- Where the Kangaroo is goes to the log every five seconds while it moves.
- The Typhoon, as Justin wants it next: D-pad right once earned, a fade to
  black, and the player flies the campaign's Typhoon (v_Typhoon, its rocket
  trails, the P-47 wing guns' strafe hits on each surface, the flyby sounds)
  over the map for the run, steering it and firing freely; his soldier stays
  where he called it and can be killed meanwhile, which ends the run.

## 0.5.80: The Kangaroo on the Horch again, with its ways in; the jeep's move undone

- 0.5.79's test (Claude, System Link War on Merville): a tank's collision is
  a brush of the map's own at the place the map gives it (a tank-sized box
  of the map's data around the Sherman's place) and does not move with the
  tank: on the Sherman the moved Kangaroo could be walked through and not
  got into. Justin: the Kangaroo is to be its own vehicle, not the Sherman.
- The Kangaroo now takes the map's Horch first (a box for collision, which
  moves with it, and a gunner's seat: on the Horch, in 0.5.74, its gun could
  be fired), then a WC51; never a tank. With 0.5.78's stand-ins for the tags
  its model lacks (tag_enter_left/right, tag_steeringwheel, the hatches...)
  the Horch's ways in and its driver's place are the Kangaroo's own.
- Justin's test: the moved jeep could no longer be got into. 0.5.76 had
  carried every position near the old place in its entity and record; that
  is undone, back to 0.5.73's copies only (drivable then).
- Each moved vehicle's entity is logged word by word, to find the box it is
  hit by and make the Kangaroo's the size of its hull next.
- Found: calling a strike works only with the scout's binoculars; a
  rifleman's (mp_binoculars_nospot) do nothing when fired, so a killstreak
  earned in another class cannot be called yet.

## 0.5.79: Where a moved vehicle's collision stays; streak_give for testing

- 0.5.78's test (Claude, System Link War on Merville): the Kangaroo asks
  for its ways in when it is set up (sub_8256CCE8) and now gets its
  stand-ins (tag_enter_left/right its middle wheels, the back ones
  tag_enter_back). Seen in daylight it is textured (olive, the white star,
  the side plates): the "all black" of 0.5.76 was its shadowed side.
- But Use still does nothing, and walking into it goes through it: its
  collision stayed where the Sherman stood (1140 -3899): there something
  unseen blocks the way. The same is likely why the moved jeep clunks.
- This build looks for every copy of a moved vehicle's old place left in
  memory (outside its entity and record, already carried), at the map's
  start and again a few seconds in, and logs each with the words around it:
  where the collision (or the physics body) keeps its place.
- streak_give uav / artillery / typhoon in the console gives the host's
  player that killstreak, to test the Typhoon without seven kills.

## 0.5.78: The Kangaroo's ways in; the Typhoon's strafing run

- 0.5.77's test (Claude, System Link War on Merville): the log listed every
  vehicle's tags. The Kangaroo's model has its tracks' and wheels' tags
  (tag_tread_left/right, tag_gear_left/right, all six wheels), its driver,
  gunner, six passengers, tag_enter_back and its gun's mount; the Sherman's
  turret model is still on it (tag_turret, tag_barrel, tag_flash). What it
  lacks, against the Panzer, the Horch, the BMW and the WC51, were names
  only known by their hashes until now: tag_enter_left, tag_enter_right,
  tag_enter_back_left/right (where a vehicle is got into), tag_hatch_left/
  right, tag_enter_turret and tag_gunhull.
- Every tag lookup on a Kangaroo (sub_82434E60, under all of them) now
  answers a missing one with the nearest it has: the side ways in its
  middle wheels, the back ones tag_enter_back, the hatches the driver's and
  gunner's places, the turret's way in and the hull gun its gun's mount.
  The log says each stand-in used.
- The Typhoon (7 kills in a row): earned as a strike, called like the
  artillery with the binoculars. Its run is five strikes one after another
  along the line from you to the spot you marked, 300 units apart and
  centred on it, 0.9 s between. "TYPHOON INBOUND" on the caller's screen.
  The artillery's strikes earned first are called first.
- The barrage's log line gives its target point (x y z).
- 0.5.77's log also showed that pressing Use next to the Kangaroo asked its
  model for no tag at all, so getting in may stop before any tag. The log
  now lists, for every vehicle on the map (not only the Kangaroo), which
  tags are asked from where, to compare Use on the jeep with Use on the
  Kangaroo. (Its own comment in mp_fixes.cpp.)

## 0.5.77: Test build: what the Kangaroo is missing (logs)

- 0.5.76's test (System Link War on Merville, by Claude on the host): the
  Kangaroo still cannot be got into (Use next to it does nothing), it draws
  all black, hovers a little and shows bare suspension posts, no wheels or
  tracks. The moved jeep: 10 positions and 7 axes carried with it.
- 0.5.76's "getting in" change hooked sub_82570558, which only turns the
  tank's turret, barrel and recoil bones; getting in is elsewhere.
- This build logs, for the Kangaroo only, every tag the game looks up on its
  model (sub_82435668) and whether the model has it, and once a map lists
  every vehicle's tags by name, so the next build can give the Kangaroo the
  ones it lacks.

## 0.5.76: The Kangaroo can be got into; moved vehicles carry all of themselves; a full friend package

- 0.5.75's test: the Kangaroo (the map's Sherman with its model) has a
  tank's collision, no floating turret, but could not be got into or driven,
  and still shows no wheels or tracks. The moved jeep still clunks as it
  drives; the map's own, unmoved, does not.
- Getting in: a tank is got into at its hatches and aims from its turret
  and barrel, found each frame on its model by name (sub_82570558, the
  names hashed once). The Kangaroo's model has no hatches, turret or barrel.
  For the Kangaroo those names are now its own while that runs: the hatches
  its driver's and gunner's places (tag_driver, tag_gunner), the turret and
  barrel its gun's mount and barrel (tag_gunner_turret, tag_gunner_barrel).
- The clunk: moving a vehicle left copies of where it stood behind (its
  wheels' places, its axes). Every position near where it stood, in its
  entity and its vehicle record, is now carried to the new place, turned by
  the change of facing, and every flat axis turned with it; the log says how
  many.
- Friend package: MAKE-FRIEND-PACKAGE.bat now makes a full first-time
  package on the build you are on: the port, the version and patch notes,
  the shaders, the DLC tool, the upscaled textures and your settings made
  theirs (named Friend, System Link pointed at your address, texture dump
  off). START-HERE-FRIEND.txt says what to do.

## 0.5.75: The Kangaroo is the map's Sherman with the Kangaroo's model

- Tested on 0.5.74 (System Link War on Merville): the saved jeep is the
  map's WC51 and drives, but it clunks a lot while driving. The Kangaroo
  (the map's Horch with the Kangaroo's model) fires its gun but could not be
  driven (Y / F changes seat, from the gun to the wheel), it could be walked
  through in places (its collision is the Horch's, much smaller) and had no
  wheels or tracks.
- The Kangaroo is a Sherman's hull, and its model carries tracks, wheels and
  side plates of its own (ng_tread, ng_wheel1, ng_wheel2, ng_sideplate) on
  the Sherman's tags (tag_tread_left/right, tag_gear_left/right). So it now
  takes the map's Sherman first: a tank's size, collision, tracks and
  driving; then a WC51, then a Horch. It no longer takes a motorcycle.
- A devmap with mp_gametype war gives Merville only a Horch and four
  motorcycles; War made from the menus (System Link) gives it the tanks and
  the WC51 too (the map gives its tanks to tdm, war and hq).
- Upscaled textures in place: the button icons, the controller picture, both
  menu fonts, both photos and the first menu background; two backgrounds
  still to come.

## 0.5.74: The Kangaroo borrows a Horch when the WC51 is taken; g_gametype works

- 0.5.73's test: the saved jeep was the map's own WC51 moved, and it drives.
  The Kangaroo could not be manned because Merville has one WC51 and the
  jeep took it, so the Kangaroo was made new (not drivable). Now a saved
  vehicle with none of its kind left borrows the nearest match from the map:
  the Kangaroo takes a WC51, else a Horch (it has a gunner's seat and gun),
  else a motorcycle; a jeep takes a WC51, else a Horch. The one borrowed gets
  the asked for model.
- g_gametype (the PC Call of Duty name) did nothing: this game's setting is
  mp_gametype (dm, tdm, ctf, hq, scf, dom, war, sd). g_gametype is now passed
  on as mp_gametype.
- Upscaled textures (separate from the build, in textures\replace): the
  button icon sheet, the controller picture, both menu fonts, two photos and
  three menu backgrounds, four times the size (Real-ESRGAN).

## 0.5.73: Saved vehicles are the map's own, moved there (drivable)

- 0.5.72's test: vehicles made in the map's start stay put across restarts
  but still cannot be driven, manned or got into. The comparison made at
  the start (both fresh) leaves few differences: the map's own carry what
  the map gives them, their collision brush (+878) among it.
- So a saved vehicle (vehicles.txt) is now one of the map's own vehicles of
  that kind, moved to the saved place in the map's start, before its
  physics begins: every copy of its origin and yaw rewritten (the entity's
  +32, +304, +336, +832, +76, +356; the record's +192, +208, +448, +228,
  +244, +468). It keeps everything that makes it drivable. The Kangaroo is
  one of the map's WC51s with the Kangaroo's model.
- When the map has no vehicle of that kind left, a new one is made as
  before (not drivable yet), and the console says which kinds the map has.
  On Merville in a devmap (deathmatch) those are its Horches and BMWs;
  g_gametype war (then devmap) brings out its tanks and trucks too.

## 0.5.72: Saved vehicles made with the map's own, at its start

- 0.5.71's comparison (a Sherman put down against Merville's Horch): ours
  have no physics filled in (the wheels' places, the body's orientation,
  all nothing in the vehicle record from +1300 on, its physics slot -1
  where the map's has one), which is why they hover with no wheels on the
  ground and cannot be got into. The map's vehicles are made during its
  start, before the game begins.
- So the saved vehicles (vehicles.txt, vehicle_mark) are now made inside
  the map's start too, in its last pass over the vehicles (sub_82564520),
  right beside the map's own and treated the same; the script's setup of
  the vehicles then runs for them as for the map's. To try one: put it
  down with vehicle_mark <kind>, then map_restart (or devmap the map again).
- spawnvehicle still puts one down at once, as before, to see where it
  would go.
- Split screen: picking Split Screen on the multiplayer menu (A) and then
  typing devmap starts the map for four local players. Type devmap from
  the main multiplayer menu without choosing anything first.

## 0.5.71: Vehicles: what ours lack, read from the map's; vehicle names without mp_

- 0.5.70's test: the pause menu guard works (the game's own pause byte);
  the Kangaroo appears (the WC51's vehicle file, the Kangaroo's model) but
  hovers above the ground without its wheels, and neither it, its .50 cal
  nor any vehicle put down can be used; the map's own can be driven.
- Each vehicle put down is now compared, word by word, with one of the
  map's own (the entity's first 1000 bytes and the vehicle record), and
  the differences written to the log: what ours are missing.
- The version number back on track: BUILD-MULTIPLAYER now takes it from
  the newest entry of these notes (this one: 0.5.71), so the window's
  title, the log, VERSION.txt and the notes always agree. It used to count
  up from the last build, which fell behind whenever a build failed. The
  notes are also copied beside the game as PATCH-NOTES.txt at each build.
- spawnvehicle takes a vehicle file's name without its mp_ too
  (motorcycle_harley, wc51).
- The ini (by hand, this once): texture_dump = 1, and the artillery at 5
  kills (killstreak_kills became killstreak_artillery_kills).

## 0.5.70: Killstreak ladder (UAV, longer artillery); texture packs (menus upscaling, step one)

- Killstreaks (host), kills in a row without dying, one of each a life:
  - 3: UAV, every enemy on your compass for 30 seconds (the compass's own
    enemy_radar drawing, its record at 0x82B2CF90, switched on and off).
    "UAV ONLINE" across the screen.
  - 5: artillery barrage, called with the binoculars; a killstreak's
    barrage is started a second time once the first has fallen (8 s on),
    so it lasts twice as long.
  - 7: the Typhoon: announced, the strafing run itself comes later.
  - F11: Killstreaks, the kills for each, the UAV's time.
  - New on-screen notices for these (Overlay::Notice).
  - For now the UAV and the notices show on the host's own screen only.
- Texture packs (texture_pack.cpp): texture_dump = 1 in the ini saves every
  texture the game shows, once, as textures\dump\<key>.png beside
  CoD3MP.exe. A picture saved as textures\replace\<key>.png (or .dds), at
  any size, is then used in the game's place: upscale the menu art and
  fonts, drop them in, and they are drawn sharp. On by default
  (texture_replace = 1); nothing changes until files are there.

## 0.5.69: Pause menu read from the game itself; the Kangaroo on by default; no invisible Harley

- The binoculars and the pause menu, fixed at the source: Start's command
  (togglepaused, sub_82527280) flips a byte per local player at
  0x82C73024, the same one that stops the player's moves while a menu is
  up. The binoculars now read it: while it is set the d-pad is the
  menu's. (0.5.63 to 0.5.68 tried to guess the menu from the buttons.)
- The Kangaroo is on by default on maps with a WC51 to copy (Merville,
  Poissons, or any map with vehicles_everywhere 1): no kangaroo 1 and map
  reload needed. kangaroo 0 turns it off from the next map.
- vehicles_everywhere alone loads Merville's archive now (the Kangaroo no
  longer pulls all 133 MB of it in).
- spawnvehicle harley on a map without the Harley's model (all but
  Poissons; Merville has only its wreck) says so instead of putting down
  an invisible one.

## 0.5.68: Start and the d-pad counted as they are pressed (pause menu guard, fourth try)

- 0.5.66's log: the pause menu was never seen opening. The binoculars look
  at the buttons in the game's frame, twenty times a second, and Escape's
  Start lasts only a few of the pad's reads, so the press fell between two
  frames. The input layer now counts presses of Start, d-pad up and d-pad
  down as they happen, and the binoculars take the counts each frame
  (presses older than a second, from the menus or a load, are dropped).
- 0.5.66 confirmed: vehicles now appear where you look (the state's +208
  is the view's yaw), and the script's vehicle setup runs for each.

## 0.5.67: The Kangaroo APC (first try); Merville's vehicle files read at the right time

- New console command: kangaroo 1, typed before loading a map. The
  Kangaroo's campaign archive (sp\mace2\kangaroo.cod, about 5 MB) is then
  loaded beside the map, and a new vehicle kind "mp_kangaroo" is made as a
  copy of the WC51 truck's vehicle file. spawnvehicle kangaroo (or apc)
  puts one down with the Kangaroo's model (v_Kangaroo). On a map without
  vehicles of its own, Merville's archive comes too (the WC51 is in it).
  It drives as the WC51 does for now; its own handling and seats come next.
- vehicles_everywhere: Merville's vehicle files are now read where the map
  start makes its vehicle records (sub_82561838); 0.5.62 waited for
  sub_8256CE08, which the map start never calls.

## 0.5.66: Spawned vehicles get the script's setup and face your way; pause menu guard redone

- 0.5.64's test: the jeep set up (its own vehicle record) but could not be
  driven or its gun manned, and the Harley was "put down" out of sight.
  - The multiplayer script sets up each map vehicle at the map's start
    (sub_821F2B40 in _mp_tankdrive.bro, run over every script_vehicle):
    damage effects, blowing up when flipped or under the world, and what
    lets players get in. A vehicle put down later never had it; it is now
    run for each one.
  - Where "in front of you" is: the view's yaw is the player state's +208;
    the entity's +356 (0.5.64) stayed 0 in a devmap, so every vehicle went
    east of you. The log says both, to be sure.
- Binoculars in the pause menu, third try: Start (Escape) was looked for
  only after the "in a level" test, which the pause menu fails, so it was
  never marked. Now Start is seen first and marks the menu open; it counts
  as closed on anything only play does (right stick, a trigger, the view
  turning, the mouse taken back). The log says each change.

## 0.5.65: Vehicle placement (vehicle_mark, vehicle_clear, vehicle_list)

- Vehicles put down for good: vehicle_mark <kind> puts one in front of you
  and saves it in vehicles.txt (beside CoD3MP.exe: map, kind, x y z, yaw).
  At the start of every game on that map the host puts all of the map's
  saved vehicles down (a few seconds in, after the map's own). vehicle_list
  says the map's, vehicle_clear forgets them. The file can be edited by
  hand, and shared with a friend who hosts.
- Only the machine running the game (the host) makes vehicles; anyone
  else's spawnvehicle says so.

## 0.5.64: Binoculars not deaf after a pause; vehicle command spellings

- 0.5.63's pause guard read the wrong number for the view (the player
  state's +208, which never turned), so after the first Start, or Enter
  (which is Start too), the binoculars ignored the d-pad for good. It now
  reads the player entity's yaw (+356, the one spawnvehicle uses), and
  Enter typed into the console no longer counts as Start.
- The vehicle command also answers to vehiclespawn, vechiclespawn and
  vehicle (spawnvehicle jeep, vehiclespawn jeep, vehicle jeep).
- 0.5.63 confirmed: each map now has room for 16 more vehicles (Merville:
  21 in a devmap).

## 0.5.63: Room for more vehicles; the pause menu's d-pad leaves the binoculars alone

- spawnvehicle said "maximum (5) reached" on Merville: the map makes only
  as many vehicle records as it asks for (5 in a devmap), and its own
  vehicles take them all. Each map now gets 16 more (sub_82561838 hooked:
  1872 bytes each, 30 KB), for vehicles put down by hand.
- The d-pad in the pause menu took the binoculars out or put them away.
  Start (or Escape) now marks the menu open, and the binoculars ignore the
  d-pad until the view turns again (only play turns it).

## 0.5.62: Vehicles on any map (vehicles_everywhere)

- New console command: vehicles_everywhere 1, typed before loading a map
  (devmap rouen, ...). On a map without vehicles of its own, Merville's
  archive is then loaded beside it (as the campaign weapons archives are),
  its asset look-ups added to every list, and its VEHICLEFILE and
  VEHICLEPHYSICSFILE entries read after the map's own (sub_8256CE08 hooked:
  sub_8254B318 over Merville's archive). spawnvehicle then works there.
  Merville's archive is big (133 MB): watch for memory trouble.
- Fixed: the campaign sound banks' loop read past its one-entry list for
  the second campaign archive.
- spawnvehicle, 0.5.61's test: the models appeared, but every vehicle took
  the same vehicle record, so only a model each and no vehicle to drive.
  The record is only kept when the kind is found among the map's vehicle
  files, so that look-up is now checked first: when it fails the console
  says which kinds the map has. And when all the map's vehicle records
  (20) are in use, the console says the maximum is reached instead of
  silently failing. Each spawn says on the console whether it set up.
- The log lists how many vehicle kinds each map reads at its start.

## 0.5.61: Vehicles get their models; console bot_add back to adding only

- 0.5.60's binoculars: confirmed working (light zoom, x3 on the left
  trigger, d-pad up/down).
- spawnvehicle: the vehicles were made (the log showed each one set up)
  but had no model, so there was nothing to see. The map's own vehicle
  entities carry one; it is now set the script's way (SetModel,
  sub_824CE130) before the vehicle is set up: v_shermanm4a3, v_panzer_iv,
  v_horch1a, v_wc51, v_bmw, v_harley.
- bot_add from the console no longer gives the bots their sessions: in a
  devmap game that crashed the game a little after (a read of unmapped
  memory). The bots' turn comes with the bot work (0.7); until then they
  stand.

## 0.5.60: Binoculars fixed (light zoom, follow what is in hand); bots spawn from the console

- 0.5.59's binoculars went wrong: they kept their own idea of "out" and
  took the wrong weapon kind (7) for the binoculars', so a moment after
  coming out they lost track: aim stayed held on whatever was in your hands
  (the zoomed, gunless view at spawn), and d-pad down would not put them
  away. Now whether they are out is read from the weapon in your hands
  every frame, nothing kept apart, and aim is only ever held while the
  binoculars themselves are in hand.
- The view through them was the game's own, far too close. Now d-pad up
  gives a light zoom (1.5x your field of view), the left trigger 3x (twice
  that), and again back to 1.5x. D-pad down: back to your gun.
- bot_add from the console now also gives the bots their session and
  spawns them the game's way (what debug.txt's bot_session 2 did), so they
  play instead of standing dead. Use it, then kill 3 for the killstreak.

## 0.5.59: Binoculars out/away on the d-pad; artillery killstreak; spawnvehicle

- Binoculars: d-pad up brings them straight to your eyes, d-pad down puts
  them away. On the keyboard: B out, N away (Binoculars out and Binoculars
  away in the F11 keys list). While they are out the left trigger (aim, the
  right mouse button) zooms x2, and again back to normal (the binoculars'
  field of view narrowed to show things twice as big). The right trigger
  does nothing with them unless you have an artillery strike waiting.
- Binoculars for everyone: the host now gives them to every player and bot
  whose kit it gives, not only to his own player.
- Artillery killstreak (host): every 3 kills in a row without dying earns a
  strike, called in with the binoculars like the scout's. Keep the streak
  going for more (6, 9...). Earned strikes wait through a death until used.
  It uses the scout's own ArtilleryDispenser script, started for the player
  (or, for a scout, his own made ready at once). F11: "Artillery killstreak"
  and "Kills for artillery" (killstreak_artillery, killstreak_kills).
  The log says each kill in a row, each strike earned and called.


- New console command: bot_add <n> adds bots on the host until there are n
  (as debug.txt's bot_add does), for testing the killstreak.
- New console command: spawnvehicle <kind> puts a vehicle down about 220
  units in front of you, facing your way. Kinds: sherman (or tank), panzer,
  horch, wc51 (or jeep, truck), bmw, harley (or motorcycle), or a vehicle
  file's own name (mp_shermantank, ...). Host only.
- It makes a "script_vehicle" entity the way the map's own are made (G_Spawn,
  the class name, origin and angles, then G_CallSpawnEntity with
  "vehicletype" in the spawn's key/values). For now it works on maps that
  carry vehicle files: Merville (mp_merv) and Poissons. Next: loading those
  vehicles' files on other maps, then the Kangaroo.
- The log says what happened with each one ("vehicles: ...").

## 0.5.58: Binoculars for every class; the campaign's binoculars no longer pop up mid-run

- Multiplayer: every class now spawns with binoculars besides its own kit
  (only the scout had them: "mp_binoculars_nospot", his special). D-pad up
  takes them out; hold the right bumper to keep them at your eyes (while
  they are out, the right bumper aims instead of throwing a grenade); let
  go of the bumper, or press d-pad up again, and you are back on your main
  weapon.
  Your other slots and the class's own special (mines, ammo, revives) stay
  as they were. Setting: "Binoculars on d-pad up" in the F11 menu
  (binoculars in the ini), on by default.
- Campaign: the binoculars could come up in the middle of a run and stop
  it. The +binoculars command (sub_8250CDB0) is now ignored while running
  (sprint held, including the tap-to-sprint hold, or the stick pushed most
  of the way). Standing or walking, they come up as before. The log says
  which buttons were down each time, to pin the cause down for sure.
- Build: multiplayer-only blocks (#ifdef COD3_MP) in shared files keep
  their own addresses (make_mp_host.py had been checking and translating
  them as if they were the campaign's).

## 0.5.57: The console works in multiplayer (devmap, god, noclip, ...)

- The console (the key left of 1, ` or ~) sent its commands to the
  campaign's command buffer addresses, which are something else in the
  multiplayer program. It now uses multiplayer's own (cmd_text at
  0x82B93444, Cbuf_AddText sub_8247FA38).
- The developer commands are all still in the multiplayer program, with
  their code: devmap <map> (a map with cheats on), god, invinc, noclip, ufo,
  notarget, give, take, kill, where, setviewpos, jumptonode, dropweapon,
  and the render views wireframe, fullbright, fogswitch. "help" lists them.

## 0.5.56: The Mills bomb in the hand (try); melee lunge 5% shorter

- The melee lunge reaches 5% less far: it takes its target from 122 units
  (was 128). Both players need the same build (movement is predicted).

- 0.5.55 works: the Mills bomb explodes with the frag's effects and sounds,
  and its own model is thrown and lies on the ground.
- In the hand: when the frag's first-person model is its world model (the
  grenade itself in the hands), the Mills bomb's world model is put there
  too. The log says what the frag's is (weapons: mills_bomb's copy's view
  model ...).

## 0.5.55: The Mills bomb explodes

- 0.5.54 works on a map: Mace 2 and Fuel Plant load, all six campaign
  weapons are made, the FG42 Scoped shows its scope on the ground.
- The Mills bomb's explosion (and its pin and throw sounds) came as
  events under its own name, which the maps' effect tables do not have:
  no sound, no explosion (the damage was done). A campaign weapon with no
  sounds of its own now has those events as its copy's (the frag's).

## 0.5.54: Mace 2 for St. Lambert; the campaign's scoped FG42; the Mills bomb

- The campaign weapons archives are now Mace 2's (the Polish mission:
  the Bren, the Lee-Enfield and the scoped FG42) and Fuel Plant's (the
  Sten, the FG42), in place of St. Lambert's; one campaign sound bank,
  Mace 2's, has every campaign gun's sounds (STEN, BREN, LEE_ENFIELD, fg42,
  fg42sniper, the GR_ reloads).
- FG42 Scoped: the campaign's own scoped world model (w_fg42_WORLD, the
  scope on the gun others hold and the one on the ground), its scoped icon
  (hud_fg_42_scoped) and its own gunshots (fg42sniper_FIRE). The scope on
  the gun in your own hands is a separate campaign model (w_FG42_scope) the
  campaign attached in its own code: not yet.
- The Mills bomb, the British grenade: a custom class's grenade kind
  "MILLS BOMB (BRITISH)" (the in-game class editor; ini grenade_type 3).
  The frag's handling, the Mills bomb's model when thrown and lying.
- Found while looking: Mace 2's machine gun (w_geLMG) is the MG34, which
  multiplayer has; the MG42 is only mounted and AI models (no first-person
  animations). The G43 is complete in Forest (a third archive, later).
- Both players need this build: the weapon list grew (mills_bomb).

## 0.5.53: FG42 Scoped: visible, and its own name

- 0.5.52's scope view works (the German scope when aiming). Its world
  model (w_fg42_world) is a model file holding only the noscope FG42, so the
  dropped gun drew nothing: it uses the FG42's model now (the campaign has
  no scoped FG42 model to show the scope on).
- Its own name, "FG42 Scoped" (WEAPON_FG42_SCOPED, given by the text
  lookup sub_8254D180), in the pick-up prompt and elsewhere.

## 0.5.52: FG42 (scoped)

- A fifth campaign gun: the FG42 (scoped), beside the FG42 in the class
  menus (F11, the in-game class editor, the class screen) as its scope
  attachment. The same gun (BAR copy, FG42 animations, 20 rounds, its own
  gunshots, reload and icon), with the scoped Kar98's German scope view (its
  overlay, reticle and zoom: the campaign FG42's scope is the same German
  kind) and the campaign's scoped world model (w_fg42_world).
- The plain FG42 is the campaign's iron-sight one (Fuel Plant's, the
  "noscope" model): its sight picture is the campaign's own, only its zoom
  is the BAR's (fg42_ads_fov in the ini changes it).

## 0.5.51: Sights zoom per campaign gun (ini)

- 0.5.50 works: the campaign guns fire with their own campaign gunshots,
  keep their muzzle flashes, and show their own pick-up icons.
- The campaign guns' sights zoom (adsZoomFov) is their copy's (the FG42's
  and Bren's the BAR's, the Sten's the Thompson's, the Lee-Enfield's the
  Kar98's). New ini settings, no build needed to change them:
  fg42_ads_fov, sten_ads_fov, bren_ads_fov, enfield_ads_fov (degrees; a
  bigger number zooms less; 0: the copy's). The log gives each copy's.

## 0.5.49/0.5.50: Pick-up icons done; gunshots by the shooter

- 0.5.48 (shown as 0.5.49): all four guns show their own pick-up icons.
- Gunshots: a shot event under the campaign name never reaches the sound
  code (it fails before, so 0.5.48's lookup as the copy was never asked).
  Now every shot keeps the copy's name again (campaign_shot_events = 0:
  flashes and the copy's sound found), and the sound code knows the
  shooter: each shot event records the shooter's sound handle (his entity's
  +484, which the message carries at +496) with the campaign gun he fired.
  When that shooter's copy event (thompson, bar, kar98) is made, its
  gunshot cues become the campaign gun's (STEN_FIRE_1ST, ...).

## 0.5.48: Campaign gunshots through the map's event table; icon copies

- What a shot sounds like comes from the map archive's "event" table
  (sub_82554BA0 / sub_82490220), by the event's name: it has only the
  multiplayer guns, so a campaign gun's shot found nothing (silent) but its
  last shot's ring-off (the "sniper" sound). A campaign gun's event is now
  looked up as its copy's (sten as thompson), and while it is made the
  copy's gunshot cues are swapped for the campaign gun's own
  (THOMPSON_FIRE_1ST: STEN_FIRE_1ST, from the campaign banks).
- 0.5.47's spare menu images do not exist: all three got the list's
  default (the hand) object. Now each gun without an icon object gets its
  own copy of the Sten's, with its own image. The Sten's showed (0.5.47).

## 0.5.47: Icons through spare menu images; sound lookup logging

- 0.5.46: the guns' own shots played by name found no sound (that name
  path plays the game's sound aliases, not bank cues), and the Sten stayed
  a white box (the menus' image entries are temporary: the image is kept on
  the image object).
- The pick-up icons: each campaign gun's HUD icon object gets its own
  image on its parts (+100, as the object's own image setting does). The
  Sten has an object (hud_sten_smg); the others take menu image objects no
  multiplayer gun uses (M1 Carbine, G43, MG34 (ge_), Thompson full auto).
- The sound engine's cue lookups are logged (cues: lines), to see what a
  shot asks for.

## 0.5.46: Campaign gunshots played; the Sten's icon

- 0.5.45's test: with shot events 3, 7 and 53 under the campaign names the
  guns went silent (no sound is found by those names) and kept their muzzle
  flash (event 1). Now each shot of a campaign gun plays its own campaign
  gunshot by name at the shooter (STEN_FIRE, fg42_FIRE, BREN_FIRE,
  LEE_ENFIELD_FIRE; _1ST for you, _3RD for the others), with event 1, and
  events 3, 7, 53 keep the campaign names so the copy's shot stays silent
  (campaign_shot_events = 14, now the default).
- The pick-up icons are the menus' images, made at the front end before
  the campaign archives load: the Sten's (hud_sten_smg, in the menus' list)
  got the default image, the white box. It is given its own image once the
  campaign archives are in. The others are not in the menus' list (a hand):
  the log lists the list's images to find them a place.

## 0.5.45: Campaign pick-up icons; gunshots test 2

- The pick-up prompt's gun icons are images looked up in one archive only,
  so the campaign guns' (in the campaign weapons archives) came back as the
  default: the hand or the white box. Now an image not found there is looked
  for in the campaign archives too, and the four guns get their own icons
  (campaign_icons, on).
- 0.5.44's test: shot event 1 under the campaign name lost the muzzle flash,
  so event 1 carries it. This test keeps event 1 the copy's and gives the
  other three (3, 7, 53) the campaign name (campaign_shot_events = 14), to
  hear whether the campaign gunshot comes through with the flash kept.

## 0.5.44: Test build for the icons and gunshots (logs)

- The pick-up icons come from the menus' own image list (sub_824EE308,
  searched by name), which only has the multiplayer guns: so the campaign
  icons fell back to a hand or a white box. This build logs that list's
  images (hud images: lines) to make the campaign ones the same way.
- Every asset looked up by name is logged once (assets: lines), to find
  what a shot looks up (its muzzle flash) and how its sound is chosen.
- campaign_shot_events (ini): which shot events (1, 3, 7, 53 as 1, 2, 4,
  8) keep the campaign gun's own name. This test: 1 (event 1 only).

## 0.5.42: Catch your breath

- Running out of stamina: it now comes back to half in a third of a
  second, so sprinting again straight away is a real sprint instead of
  short stumbling bursts (with quick_sprint, on by default).
- Pick-up icons back to the copies' (campaign_icons off): the campaign
  icons are found, but still draw as a white box (the Sten's, DXT5) or a
  hand (DXT1). The log shows them the same shape as the multiplayer ones,
  except their archive has no bank (-1); that is the lead for next time.
- The campaign gunshot sounds did not take: the shots' sound names are not
  in the weapon's fields (the log says so for each gun). Still the copies'.
- Sten reload back to 3.4 s (3.6 was worse).

## 0.5.41: Aim in the air, campaign gunshots, campaign icons, reload tuning

- Aim down the sights while jumping (quickscopes in the air). The game
  only let the sights come up on the ground. ini: ads_in_air (1 on).
  Both players need this build for it to behave the same on both sides.
- The Sten, FG42, Bren and Lee-Enfield fire with their own campaign
  gunshot sounds (STEN_FIRE_1ST, fg42_FIRE_1ST, BREN_FIRE_1ST,
  LEE_ENFIELD_FIRE_1ST and the 3rd-person ones), and keep their muzzle
  flashes. ini: campaign_shot_sounds (1 on).
- The campaign guns' own pick-up icons are back on (campaign_icons, now
  on by default): 0.5.36 drew them before 16-bit textures were supported.
  The log describes each icon, to compare with the multiplayer ones if
  they still draw wrong (set campaign_icons = 0 to go back to the copies').
- Reload times can be tuned in the ini without a build: sten_reload,
  fg42_reload, bren_reload (seconds; 0 = the built-in 3.4, 2.8, 3.8).
  Your ini tries the Sten at 3.6 s.

## 0.5.40: Dying drops the gun in your hands

- Dying dropped the pistol instead of the gun you held (the gun in hand is
  kept in the slot pick-ups replace, and death dropped the other slot).
  Now the slots are put back the right way the moment you die, so the gun
  in your hands (FG42, Sten, Bren, Lee-Enfield or any other) falls.

## 0.5.39: Swap the gun in your hands (third try)

- 0.5.37/0.5.38 never reached the players: their states are in the game's
  own data (0x82...), which the check took for a bad address and skipped.
  Now the gun in your hands is kept in the slot a pick-up replaces.
- Pick-up icons: the copies' (Thompson for the Sten, BAR for the Bren and
  FG42, Kar98 for the Lee-Enfield), as in 0.5.37.

## 0.5.37: Swap the gun in your hands (second try); icons (test build)

- Confirmed (0.5.36): every campaign gun's model now lies on the ground.
- The weapon in your hands is read from where the game keeps it for the
  host's player (the entity's own byte), so picking up swaps the gun
  you're holding. 0.5.36 read another field and never swapped; the log now
  shows what is in hand.
- The pick-up prompt shows the copy's icon (the Thompson's for the Sten,
  the BAR's for the Bren and FG42, the Kar98's for the Lee-Enfield) instead
  of a white box or a hand. campaign_icons = 1 tries the campaign's own.

## 0.5.36: Pick-ups swap the gun in your hands; FG42 and icons on the ground (test build)

- **Picking up swaps the weapon you're holding**, not always the pistol.
  The game swaps whatever is in your second slot; the weapon in your hands
  is now kept in that slot (for every player on the host).
- **The FG42 shows on the ground** (and in other players' hands): its world
  model is w_fg42_WORLD_noscope; the name used before does not exist.
- **The campaign guns' pick-up icons** (the white box): they were found,
  but drawn from a 16-bit texture format the renderer skipped (and said
  nothing about). 16-bit textures (5:6:5, 1:5:5:5, 4:4:4:4, 8:8) are now
  drawn; any other skipped format is named in the log.

## 0.5.35: Dropped campaign guns' models and pick-up icons (test build)

- The pick-up prompt's icon (the white box, or a glove) and the dropped
  gun on the ground both come from names only found in the campaign
  archive (hud_sten_smg, w_fg42_world, ...). The game looks those up in
  the map's and the global archives only. Both are now looked for in the
  campaign archives too.
- Picking up: the game swaps a picked-up main weapon for your second slot's
  weapon (the pistol in a custom class), as it does with its own guns.
- The log lists the campaign guns' items, models and icons as found.

## 0.5.34: Sten timing, dropped gun items, pick-up tracing (test build)

- Sandbags and the darker textures: confirmed fixed (0.5.33).
- Sten reload 3.4 s (was 3.2). From 0.5.33's video: the magazine sounds
  were within 0.25 s, the bolt 0.5 s after the animation's bolt.
- Each campaign gun's dropped item has its own name ("weapon_sten" and so
  on). It was the copy's ("weapon_thompson", "weapon_bar"), shared with the
  copy and, for the Bren and FG42, with each other.
- The log records the weapon slots when they change, and every HUD icon
  looked up: for picking up a Sten putting down the pistol instead of the
  FG42, and the white box in the pick-up prompt.

## 0.5.33: Dark textures with big memory fixed (test build)

- Found with your tests: the dark and black patches (Merville's sandbags)
  came from big_memory, not the campaign guns. The graphics library has 11
  more places that turn a memory address into a graphics address, written a
  different way from the 225 handled in 0.5.19. Those cut addresses in the
  extra memory down to the first 512 MB, so the GPU read the wrong data for
  some surfaces. They now keep the extra memory's addresses too.

## 0.5.32: Dropped campaign guns look right (test build)

- A dropped gun lying on the ground uses its own model slot (+1436, set for
  the BAR and the Kar98, empty for the Thompson). The FG42 and Bren showed
  a BAR there and the Lee-Enfield a Kar98; only the Sten looked right. All
  four now show their own model on the ground.
- The Sten's reload is 3.2 s (was 2.6) and the FG42's 2.8 s (was 2.4).
  From the 0.5.31 video: the Sten's sound has the magazine out at 0.9 s, in
  at 1.5 s and the bolt at 2.6 s, but at 2.6 s the animation's bolt came
  0.7 s early. 3.2 s puts each moment within about 0.3 s.
- The log times each reload's sound events, to fine-tune them further.

## 0.5.31: Reloads in sync with their sounds; campaign gunshots (test build)

- The campaign reload sounds were measured from the sound banks. The game's
  own reloads last as long as their sounds (BAR 4.0 s to 3.9, Thompson 2.3
  to 2.33), so the campaign guns now do too:
  - Sten 2.6 s (GR_STEN 2.53 s; was 3.7)
  - FG42 2.4 s (GR_fg42 2.33 s; was 3.27)
  - Bren 3.8 s (GR_BREN 3.73 s; was 3.53)
  The animations play faster or slower to fit, as in the campaign.
- Gunshots ask for the campaign guns' own sounds too (STEN_FIRE_1ST,
  fg42_FIRE_1ST, BREN_FIRE_1ST, LEE_ENFIELD_FIRE_1ST). Tested: the shots
  lost their muzzle flashes and played the wrong sounds, so
  campaign_gunshots is now off by default (the copy's shots and flashes).

## 0.5.29: Campaign sound banks (test build)

- The St Lambert and Fuel Plant sound banks (stbert.wbk, fuelplnt.wbk) load
  beside every map's own bank. They hold the campaign guns' real sounds
  (GR_STEN, GR_fg42, GR_BREN, GR_enfield_reload, ...).
- The campaign guns' reload sounds now ask for their own names again, to
  play those real sounds. Gunshots still use the copy's for now.
- New setting campaign_sounds (on by default): 0 goes back to the borrowed
  reload sounds of 0.5.26.

## 0.5.27: Sound bank tracing (test build)

- Reload sounds confirmed working for all four campaign guns (0.5.26). The
  Sten's borrowed Thompson reload is shorter than its own animation.
- The log now shows which code opens each sound bank (.wbk), the first step
  to loading the campaign levels' banks for the guns' own sounds.

## 0.5.26: Reload sounds for the campaign guns (test build)

- Sounds asked for by name that only exist in the campaign levels' sound
  banks (GR_STEN, GR_fg42, GR_BREN, GR_enfield_reload, ...) now play the
  matching multiplayer sound of the gun each is a copy of: Thompson's for the
  Sten, the BAR's for the FG42 and Bren, the Kar98's for the Lee-Enfield.
- The log lists every sound name the game asks for once, to check the
  mapping.

## 0.5.25: Weapon event logging (test build)

- Every weapon event is logged, and five more event senders map the campaign
  guns to their copies.

## 0.5.24: Poissons whole with all four campaign guns (test build)

- **The campaign weapons files no longer use archive banks.** They now load
  into the big map memory (big_memory) instead of taking 19 of the 96 banks.
  The map and its zones keep every bank, so Poissons should load whole with
  the Lee-Enfield, Bren, Sten and FG42.
- The log lists the sound events each campaign gun asks for, to find why the
  Sten's and FG42's reloads are silent.

## 0.5.23: The Sten and FG42; big memory on by default

- **big_memory is on by default** (new installs and your friend's too).
- **Sten** (British SMG, 32 rounds, copy of the Thompson's handling) and
  **FG42** (German paratrooper automatic rifle, 20 rounds, the BAR's
  handling), with their own models, arm and gun animations, HUD icons and
  reload times. In Create-a-Class's lists. They come from the Fuel Plant
  weapons file, loaded beside St Lambert's (big_memory only).
- They use the Thompson's and BAR's sounds for now.
- The log counts the archive banks in use, to see how full maps get with
  two campaign files.

## 0.5.22: Poissons' missing half found (works)

- The 0.5.21 log showed it: every archive takes "banks" of 3 MB from a
  pool of 88. Poissons, its zones and the weapons nearly fill it, and the
  campaign guns' file (9 banks) left no room for zone 1 (10 banks). It was
  never about raw memory.
- With `big_memory = 1` the pool is made at its most, 96 banks, from the
  bigger map block, and Poissons gets the campaign guns again.

## 0.5.21: Memory past 512 MB works; Poissons digging

- **big_memory works:** with it on, the game's map block (480 MB) lives
  past the Xbox's 512 MB, and mp_fuel played perfectly. The 512 MB of
  graphics memory went from 492 MB used to 123 MB.
- Poissons' zone 1 still didn't load after mp_fuel even with 110 MB more,
  so that isn't memory at all. The log now lists every archive the game
  opens and every streaming allocation that fails, to find what blocks it.

## 0.5.20: Memory past 512 MB, step 2 (off by default)

- Step 1's count: on two maps, every one of the graphics library's
  address cuts that ran was handed nothing but addresses. So they now
  keep a new **extended window**: processor addresses 0x50000000 to
  0x6F000000 (496 MB above the game's own virtual memory), which the GPU
  sees at 0x20000000 to 0x3F000000. Its 15 conversions back are patched
  to match.
- The port's renderer, its resource caches and its memory-write watch
  know the window too.
- **`big_memory = 1`** in CoD3Recomp.ini puts the game's map block there:
  480 MB instead of 370, and the 370 MB it held in the Xbox's 512 MB left
  free for textures and buffers. Off by default until tested.

## 0.5.19: Memory past 512 MB, step 1 (measuring)

- The build now routes each of the game's ~210 "cut this address to
  512 MB" spots in its graphics library through a counter. Nothing changes
  in play: each spot does exactly what it did. The log's memory report
  lists what every spot was given, to tell which ones only ever see
  addresses and can be taught about memory past 512 MB.

## 0.5.18: The Bren, and smoother switching

- **Switching:** the 0.5.17 video showed the Lee-Enfield still leaving
  0.3 to 0.5 s of empty hands, both putting it away and taking it out: its
  campaign animations hide the rifle early when putting it away and show
  it late when taking it out. Put-away is now 30% of the Kar98's time
  (400 to 120 ms) and take-out 70% (500 to 350 ms). The Bren gets the same.

- The campaign's **Bren** light machine gun joins the Lee-Enfield: a copy
  of the BAR's handling (automatic) with the Bren's own model, arm and gun
  animations, HUD icon and a 30-round magazine, reloaded at its own
  animation's speed. In Create-a-Class's main and second weapon lists.
- It uses the BAR's sounds for now. On Poissons a Bren class gets the BAR.

## 0.5.17: Lee-Enfield switches faster

- Switching away from the Lee-Enfield left empty hands on screen for about
  half a second: its put-away animation hides the rifle early, but it ran
  on the Kar98's longer timer. The timer is now 60% of that.

## 0.5.16: Campaign guns' file loaded once

- 0.5.15's unloading freed the room (Poissons' zone 1 loaded after
  mp_fuel), but the game still looked up the unloaded file afterwards and
  crashed. Taken back out.
- The file is now loaded once and reused by the maps after, instead of
  possibly being loaded again on every map.
- Known: after a map with the campaign guns, Poissons is short of memory
  until the game is restarted. Play Poissons first, or restart before it.
  (Fixed in 0.5.22 with big_memory = 1.)

## 0.5.15: Campaign guns' file let go between maps (crashed, taken out)

- The campaign guns' file (27 MB) stayed in memory after its map ended, so
  the next map started short: Poissons after mp_fuel lost its zone 1 even
  with the guns skipped. It's now unloaded when the next map's weapons load.

## 0.5.14: Campaign guns skip Poissons (fixed)

- 0.5.13 didn't recognise Poissons (it looked for the map's name in the
  wrong place), so the guns' file still loaded there. The map's name now
  comes from the map file the game opens.

## 0.5.13: Campaign guns skip Poissons

- 16 MB more for the map wasn't enough: Poissons' zone 1 still never
  loaded. So on Poissons the campaign weapons file isn't loaded at all,
  and a class with the Lee-Enfield gets the Kar98 there. Every other map
  keeps the campaign guns.
- `extra_map_memory` is back to 0 by default (it didn't help, and it takes
  room the graphics may need).

## 0.5.12: More room for maps with the campaign guns (test)

- The memory report showed what fills the Xbox's 512 MB: the game's own
  370 MB block for maps, and about 112 MB of graphics (textures, model
  buffers) made by the Xbox's graphics library. All of it is read by the
  graphics side, so none of it can move to the spare memory.
- About 18 MB is never handed out, though. With `campaign_guns` on, the
  map block now takes 16 MB more of it (`extra_map_memory` in
  CoD3Recomp.ini, 0 to 18).

## 0.5.11: Memory report

- The log's memory report now names the part of the game that asked for
  each piece of memory, past the system's own helpers, to find what can
  move out of the full graphics memory.

## 0.5.10: Lee-Enfield bolt, second try (works)

- The game looks up a weapon's animations in two lists: one by its name,
  and a second by its name plus "_arms". The Lee-Enfield only had the
  first. It now gets the second as well, taken from the Kar98's and swapped
  to the Lee-Enfield's own moves.

## 0.5.9: Lee-Enfield bolt (test)

- Working the bolt: the arms were doing the Lee-Enfield's moves while the
  rifle itself stayed still, so the hands waved in the air where the rifle
  should have been. The rifle's own moves are now swapped in as well.
- More detail in the log about the gun's animations.

## 0.5.8: Lee-Enfield magazine

- The Lee-Enfield holds **10 rounds**, its own magazine instead of the Kar98's 5.
- Its reload is the campaign's, in three parts: the bolt opens, a stripper
  clip of 5 goes in (twice from empty), the bolt closes. Each part plays at
  its real speed instead of being stretched to the Kar98's reload time.
- The shot sound plays.

## 0.5.7: Lee-Enfield arms

- The Lee-Enfield now plays its own arm animations (aim, fire, bolt, reload,
  raise, drop) instead of the Kar98's, so the left hand stays on the gun when
  you fire aimed.
- The memory report in the log now names who asked for the memory one level
  further up, to find what could move out of the full pool.

## 0.5.0: Campaign weapons groundwork (experimental, off by default)

- The campaign's guns are being brought into multiplayer. Their files sit in
  the single-player levels: St. Lambert has the **Lee-Enfield** and the
  **Bren**, Mace has those two plus the **FG42**.
- The **Lee-Enfield** is set up as a real multiplayer weapon: a copy of the
  Kar98's settings (damage, clip, handling) with the Lee-Enfield's models,
  animations and HUD icon put in. It's in the Create-a-Class main weapon and
  second weapon lists. It fires and reloads.
- The game can now load a campaign weapons file during a match and find the
  models inside it. The F8 model test shows the Lee-Enfield and Bren models
  in the world.
- `campaign_guns` and `campaign_test` in CoD3Recomp.ini, both off by
  default.
- **Not finished:** the Lee-Enfield in your hands is still invisible, and its
  name reads "null". Loading the whole St. Lambert weapons file also takes
  about 29 MB, which leaves big maps like Poissons short of memory: part of
  the map doesn't load. The plan is a small file of our own with just the
  campaign guns. Until then, keep `campaign_guns = 0` for normal play.
- **New: version numbers.** Every build gets its own number (see above).

## 0.4.10: Loadout extras

- **Second weapon:** a custom class's sidearm slot can hold any main weapon,
  for a sniper and a bazooka, or two rifles. The default is still your side's
  pistol.
- **Rocket launcher as equipment:** everyone spawns with their side's launcher
  (Bazooka for the Americans, Panzerschreck for the Germans) on top of their
  class's weapons. **D-pad left** takes it out, and again puts it away. It
  comes with 1 rocket loaded and 4 spare. Anti-Armor doesn't get a second
  one. You can turn it off in F11.
- The launcher no longer pushes out your main weapon or your pistol.

## 0.4.9: Domination flags fixed

- Captured flags now hang from the pole in the holder's colours, American or
  German. They show for both players, and the old flag disappears when a
  point changes hands.
- **Joiners now get the host's Domination updates:** flag owners, capture
  progress and score. The message had been too big for the receiving side, so
  it was dropped.
- Flag spots for more maps, placed by the host with F7: mp_merv, Poissons and
  Ederdam, saved in dom_flags.txt.
- Flags hang at the right height on the pole, and the pole no longer pokes
  through Rouen C's ceiling.
- Known: the flags don't wave, though you can hear them.

## 0.4.8: Aiming and abilities

- **Faster walk while aiming down sights**, with an F11 slider for how fast:
  40% of normal walking is the game's own, and 80% is the default.
- **Ability recharge:** the Scout's artillery, the Medic's health packs and
  Support's ammo come back sooner. The F11 slider defaults to half the wait,
  so artillery returns in about 90 seconds instead of 3 minutes.

## 0.4.7: Custom classes

- Five saved custom classes, each with a name, main weapon, sidearm, base
  class, grenade type and grenade count.
- **Create-a-Class editor on the kit screen: press R1.** Change it with the
  d-pad, A to use and B to close.
- The base class decides your ability: a Scout's artillery, a Medic's
  healing, and so on.
- Everyone starts with at least two grenades.
- Four of the second grenade slot (smoke) and of mines, instead of one.
- The class screen shows your custom class under its title.

## 0.4.6: Graphics options and movement

- **Graphics options in F11:** fog, draw distance, effects and dynamic
  lights.
- **Quick landing:** a heavy landing no longer slows you for up to two
  seconds.
- **Quick sprint recovery:** stamina comes back soon after a sprint.

## 0.4.5: Domination (the cut game mode, restored)

- Domination brought back, run on top of Team Battle: five flags (A to E),
  capture by standing on them, and score over time while you hold them.
- Rouen's own flag spots are built in. On other maps the host presses **F7**
  to place five flags where he stands, and the spots are saved.
- A **Game Modes** page in the lobby's Custom Rules turns it on or off.
- On-screen flag bar, capture progress, and flag markers in the world with
  letter, colour and distance.
- The host runs the match and sends flags and score to everyone else.

## 0.4.4: Custom rules for everyone

- The host's Custom Rules (health, gravity, speed, respawn time, round limit,
  last man standing) now reach everyone in the lobby. They're sent over a
  side channel on UDP port 28964.
- The host's own low gravity no longer gets reset by the map.

## 0.4.3: DLC maps and bots (experimental)

- **DLC maps:** dlc-tool\UNPACK-DLC.bat finds the Xbox 360 map packs (Champs,
  Valor and Bravo) and installs them. The maps show up in System Link's map
  list.
- **Bots (experimental, host only):** players that spawn, walk, aim and shoot
  on the host's machine. They're still a debug feature, not real opponents
  yet.

## 0.4.2: Multiplayer polish

- Sharp aiming by default, aim blur off, and a Multiplayer tab in F11.
- Multiplayer runs at your PC's frame rate, with an optional cap.
- No more glows showing through walls.
- Direct mouse aim, and CoD4-style bounces off slopes.
- **Custom Rules page** on System Link's Advanced Options screen.
- Tap to sprint.
- The shell shock's black, blurred screen can be turned off in F11.
- A melee lunge, like World at War's bayonet.
- Faster drawing (vertex buffers, waits), and a CPU profiler.
- System Link fix: replies now reach the right address.
- MAKE-FRIEND-PACKAGE / MAKE-FRIEND-UPDATE scripts to send the port to a
  friend.

## 0.4.1: Multiplayer arrives

- **CoD3MP.exe:** the multiplayer game, recompiled from the Xbox 360's
  multiplayer executable.
- System Link across every network adapter, including Tailscale for playing
  over the internet, with a peers list in the settings.
- Your own player name, and a separate identity per PC.
- Controller and audio volume fixes, and build scripts.

---

## Known issues (0.5.10)

- Lee-Enfield: after a reload or coming out of aim, the right hand can sit
  slightly inside the stock until the next shot.

## Known issues (0.5.0)

- Campaign guns: invisible in your hands, name reads "null", and big maps
  run out of memory with them on. Off by default.
- Domination flags don't wave.
- Frame rate can drop on very big maps like mp_merv.
- Bots are a debug feature only.






