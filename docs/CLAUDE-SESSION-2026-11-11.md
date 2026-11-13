# Claude session 2026-11-10 23:00 to 2026-11-11 (overnight, the user asleep)

The user handed over at 23:00: "get co-op truly working": the joining player must be able to do
everything a single player can, in the host's game, in real time, both ways (cutscenes,
interactions, reloads, animations), no guessing, back up notes. All work was hands-off two-copy
tests through `tools/coop/guarded_test.py` with `COD3_SCALE=0.6` (no run went under 20 fps; the GPU
peaked at 75 C, the watchdog never fired). Source and install end at **0.5.933** (built OK).
Nothing committed (fork still 0.5.851). No friend zip.

## What is PROVEN now (logs and screenshots in tools/coop/coop-out/TAG)

| What | Version | Test |
|---|---|---|
| The joining player's Use runs on the host (doors, use triggers, mounted guns go this way) | 0.5.909 | t909a |
| His hint/pick-up system runs on the host: he took Saint-Lo's M1 Garand himself, the scripts gave it, objectives moved | 0.5.912 | t912a |
| His prompt on his own screen is the host's world's ("Press and hold E to pick up the M1 Garand"), gone after | 0.5.919 | t919a (screenshots) |
| His shots and grenades are FIRED IN THE HOST'S WORLD (before: never, in any test) | 0.5.915 | t915a |
| He KILLED a German soldier on the host (score line) | 0.5.926 | t926a |
| His own grenade is shown once on his screen (the host's copy of it and its blast held back) | 0.5.918, 0.5.922 | t918a, t921a |
| The host's grenade and its explosion reach his screen | (before) | t913a, t915a |
| No friendly fire between the two players (12 hits refused) | 0.5.924 | t924a |
| His fire does not kick the host's view | 0.5.915 | t927a |
| Hits of his on non-soldier things (targets, damage triggers) told as the host's player's | 0.5.907 | t920a |
| "Is the player here" (istouching) true when either player is | 0.5.910 | (built, no failing case seen) |
| A gun the host's player swaps (pickup) stays his own | 0.5.908 | (built; no swap in tests) |

## The engine facts found (see memory coop-client1-local-only-code)

- G_RunFrame (sub_8256D3E0) runs a player's end of frame sub_8256CFB0 for clients 0..level+148 = 1.
  Its parts sub_82551DB8 (Use: record +1556) and sub_82553BB0 (hints and pick-ups: +1156, +1160,
  +1164, the thing at +1216) are now also run for client 1 from a hook on sub_8256CFB0 (focus block
  0x82A2AB60 = 0, as the game does for a client that is not the machine's own).
- The weapon's fire sub_82569C40 (ClientThink sub_8256D2B8 -> sub_8256C260 -> sub_8256B060 ->
  here -> sub_82558E58; a grenade sub_82556688) fires only for [0x82A2AB40][local index + 1]. For the
  joining player's think, table+4 holds his entity and is given back after. Table+8 is the WORLD's.
- The scripts' hide (built-in 0x5E0: sub_824A4A60 -> sub_820B23F8) sets the model instance's
  (+548) +236. Soldiers have 0 there while drawn: 0.5.920 hid every soldier on the joining copy until
  0.5.926 (the rule is now "seen over 0.5, then 0", never type 11).
- Saint-Lo's training: the Garand is a trigger_use with sethintstring (saint_lo_sub_8906D798 waits,
  then giveweapon to level.player). Its tutorial steps "move and look", "Pull LMB to fire", "Pull RMB
  to aim" read the HOST's own pad: host-only, by the level's design.
- Missile watch: a missile first seen within 64 units (flat) of the joining player and nearer him than
  the host's player is his (his copy throws its own); its generation changes after it is made, so it is
  known by number while the slot stays a missile, and its leftover (type 0 "grenade") while it stays.

## Test aids added (joining copy unless said)

- `COD3_COOP_TESTGO="t:x,y,z"` put there; `"t:x,y,z,d"` d short of x,y along the heading;
  `"t:x,y,z,d,feet"` d short of the point x,y,z, feet at that height, turned to look straight at it
  (setplayerangles sub_82540168). Works on the host's copy too (its own player).
- `COD3_COOP_TESTAIM="a-b"`: turned 4 times a second to the nearest axis soldier drawn (6000 units).
- Saint-Lo recipe: host pad `200:ly+50/1.2 202:rx+40/1 204:ry+30/0.5 205:ry-30/0.5` (step 1);
  joiner `COD3_COOP_TESTGO=178:8165,9373,300` + pad `180:ry-50/0.7 200:x/3` takes the Garand.
- Forest combat recipe: joiner `COD3_COOP_TESTGO=60:984,-5595,380`, `COD3_COOP_TESTAIM=62-100`,
  pad `rt/0.2` every 2 s from 64.

## OPEN (not done, with what is known)

1. **One-person scripted events** (an enemy tackles the player, a struggle prompt): the level scripts
   aim them at level.player (the host's). With touches told as the host's (0.5.897), the joining
   player walking into such a trigger sets it off FOR THE HOST. Making it his needs the scripts'
   player to be him for that event: not found how (no builtin names a struggle; look in a level that
   has one: its trigger, then the thread it starts).
2. **Cutscene camera**: every scripted scene seen in host logs (tests since t870) plays during the
   level's intro, which the joining copy plays itself. No mid-mission scene was reachable hands-off;
   0x825F51A0 is the scene count; built-in 0xBE4 is a sound/movie stream, not a camera. The SP view
   block (MP: 0x82C7D490 + 6368 * client, origin +4960, angles +5024; SP twin not found: only
   sub_82571678 uses those offsets) would be the way to show the host's view.
3. **Mounted guns**: his Use reaches them now (sub_82551DB8 runs for him), firing one is untested (Saint-Lo's three "turrets" at its start are the Shermans' guns, t932a); the
   turret/vehicle built-ins 0xA20..0xA34 use the LOCAL player's vehicle: likely the next local-only
   blocker.
4. **The taken Garand still lies on the barrel on the joining copy** (cosmetic): it is his copy's own
   entity; nothing rifle-like is near him on the host after the pickup.
5. **Starts that stall**: twice (t914a, t928a) the host stayed on the front end after "starting
   saint_lo/forest"; a rerun worked. Not looked into.
6. The training's tutorial input steps are host-only (by design of the level).
7. Weapons the joining player picks up off the ground: his copy picks up locally and tells the host
   (R_INVMINE, 0.5.902); the ground item stays on the host. Not looked into.

## Next steps

- The user plays the joiner (Saint-Lo and Forest) on 0.5.933: Garand, prompts, shooting, grenades
  (his own once, the host's), kills, no friendly fire.
- Then OPEN 3 (a level with a mountable MG near its start), 1, 2.

## Last runs (0.5.930-0.5.933)

t930a/t933a (Forest) and t930b (Saint-Lo) on the final code: 0 s under 20 fps on both copies; his
grenade his (not sent, blast held back), the host's grenade drawn on his copy, his grenade on the host
refused (no friendly fire), down and back in after 10 s; the Garand taken by him with the host's
prompt. No kill in t930a/t933a (aimed at a soldier 272 away, 16 shots, then shot down); t926a had one.
Two starts stalled on the front end (t914a, t928a); reruns were fine.

## Day session 2026-11-11 (12:10-15:15, the user at work; 0.5.934 to 0.5.947, all built OK)

The user: keep going till co-op is in perfect order, heavy focus on the two players' animations seen
both ways; keep the test windows side by side. Every run hands off, 0 s under 20 fps, GPU at most 65 C.

PROVEN (logs + side-by-side desktop shots, coop_test.py --desk-shots):
- 0.5.935 THE "HEADLESS TORSO SIDEWAYS FLASHING" (t933u, the user, joining window): the joining copy took
  its OWN entity of the same number for the host's bodies (made in slots the host had freed: his new
  body in 244, the Garand's). Now one of ours is taken only with the same model, never for the host's body.
- Animation audit both ways (t936b, Saint-Lo daylight, 120 apart): fire, reload, crouch, crouch fire,
  prone, melee, change of gun, aim, walk, strafe right on the other copy. The game has no soldier jump
  or gunner animation (anim.cod's 2623 names): the standing pose is the game's own.
- 0.5.936 no bind pose at a change of gun (new body hidden till animated, the old kept till then);
  test windows really side by side (the settings' resolution re-centred them).
- 0.5.937 the joining player's smoke on his own screen (only his frags are "his own").
- 0.5.941 FIX: client 1's delta angles (record +88..+96) set so his command gives his copy's view: on the
  host he faced yaw 109.6 while looking at -45 (hints and shots went the wrong way).
- 0.5.942 stationary MGs: his copy keeps its own misc_mg42 and draws the host's with it: his prompt, his
  gunner's view, firing (impacts on the host), off again. 0.5.943/945: a player on a gun (record +1148 =
  the gun's handle, +1144 = 1) is drawn aiming / shooting on the other copy.
- t945b: the joining player alone fought forward in Forest: two kills, triggers, "Objectives updated." x2.
- 0.5.946 tests: COD3_COOP_MAP's start asked again (three stalled starts in ~45 runs).
- 0.5.947 temporary probes out. Test aids kept: COD3_COOP_TESTGO "t:x,y,z,d,feet,yaw", COD3_COOP_TESTAIM,
  COD3_COOP_LISTTYPE, coop_test.py --desk-shots.

STILL OPEN: vehicles beyond scripted rides (turrets on vehicles are still deleted on the joining copy: the
Falaise crash), mid-mission cutscene camera (none reachable hands-off), one-person scripted events (go to the
host's player), the name tag sits ~60 px high, ground weapon pickups untested (Saint-Lo's Thompson is not
pickable for either player at that point).

## Evening 2026-11-11 (0.5.948 to 0.5.958, the user playing some runs)

DONE (all built; hands-off proof in tools/coop/coop-out/t948a..t958a, played t947v/t948u/t952u/t955u):
- 0.5.948 a gun given while the joiner holds none is raised (weapon cycle tap, Input::PressWeaponCycle).
- coop_test.py --played: the joiner's keyboard/mouse live (COD3_BACKGROUND=0), host COD3_NOPAD.
- 0.5.949 "coop: item:" watch of weapon_* (type 2) entities.
- 0.5.950/956 MUZZLE FLASH: the host's gun flashed at the joiner's shots when both held the same gun.
  sub_8242E5F0 places a shot's flash on the cg FIRST-PERSON model of the shooter's weapon when the
  shooter is the local slot. His fire alone is local now (hook on sub_82569C40), and in sub_8242E5F0 the
  host is put back and his flash goes to his body's gun. NEEDS THE USER'S EYES (flashes never in shots).
- 0.5.956 sub_8254AAC8: a scoped shot (client +208 0x00100000) started at the LOCAL camera: off for him.
- 0.5.951..958 PICKUPS OFF THE GROUND by the joiner: the game's swap runs on the host; his copy reports
  only its own gains (no duplicate gun); the gun's name in his prompt (0.5.953: cg weapon info
  0x82A9A460 + 196*w, +140, registered by sub_82448C78; prompt builder sub_824ED5A8, hint 12..139 = gun
  h-11, 140..267 = ammo h-139); one swap per Use (0.5.955: his Use, cmd button 0x40, held back after a
  swap till released; the host's own player does this natively); new gun in hands the same second
  (0.5.957/958).
- Test aid COD3_COOP_TESTGUN="t:d" (the host must have it set too: it writes coop-testgun.txt).

FOUND, NOT DONE (big pieces):
- STRUGGLES (an enemy grabs the player): level script code (CoD3RecompLib/levels/<map>), animations
  c_k98_mc<lvl>s1/s2_struggle (enemy) and w_armsk98_mc<lvl>s1/s2_struggle (player's arms), aimed at
  level.player. Levels: saint_lo/mid, fuelplnt/melee (+fuelplnt), crssrds/zn03, falaise(+zn11), laison,
  mace2, nightd(+zn9_hse), stbert/zn04, blkbrn, chambois, forest. Engine side: sub_82546998 = the melee
  block (notifies the enemy "anim_melee::Melee_BlockAttack"; shake/rumble for the local player only),
  called from sub_82567B90. Making a struggle the joiner's = the scripts' player must be him for that
  thread, plus his copy playing the arms animation: multi-day.
- VEHICLE GUNS/DRIVING: the joining copy deletes its vehicles at the freeze (they crashed); scripted
  rides put him beside the host (coop_join.cpp ~1151). Player-driven/manned vehicle sections are level
  scripts on level.player too. A "ride-along view" (his copy's camera from the host's seat) is the way
  in; not started.

### Struggles: the engine's SCRIPTED-ACTION system (found 2026-11-11 late, nothing built yet)

- A struggle is one use of an engine system of scripted player actions. Its name tables are at
  0x825D1168 (controls: ButtonMash, ButtonPress, StickSwirl, StickToggleHoriz, StickToggleVert,
  Rowboat), 0x825D1190 (actions: PlayAnims, PlayPlayerAnim, PlaceItem, Push, StrengthTest,
  ScaleAnimSpeed, LeverPush, Melee, MeleeInitiate, MeleeStart, MeleeSuccessSetup, MeleeSuccess,
  MeleeFailure, MeleeStagedInitiate, MeleeStagedSuccess, MeleeDropWeapon, MortarIdle, MortarStart,
  MortarLoad, VehicleBase, VehicleIdle, VehicleTurn, VehicleRelease, VehicleLink, PickLiveGrenade,
  RowboatInit, Rowboat), 0x825D11FC (blends: LerpNone, SnapPlayer, LerpOther, LerpPlayer, StagedLerp,
  StagedSnap, StagedLerpPlayer, StagedSnapPlayer), 0x825D1220 (LEFTBUTTON, DOWNBUTTON ...).
  No pointer to these tables exists in the image and no lis/addi of them was found: they are reached
  from a base computed elsewhere. => find the user at RUN TIME: hook the action start.
- So the same system carries struggles, live-grenade throw-backs, mortars, lever pushes, strength
  tests, vehicle boarding (VehicleLink) and rowboats: making it the joining player's when he sets it off
  would cover far more than struggles.
- Saint-Lo's struggle assets: zone mid.cod (saint_lo_middle), animations c_k98_mcsl1s{1,2}_{start,wait,
  thrust,struggle,winning,losing,repel,succeed,death,camhit,main} and w_armsk98_mcsl1s{1,2}_* (player's
  arms), MCSL1_CAMHIT in saint_lo.cod; w_armsk98_mcstlo_gundrop. Level code (saint_lo.dll ->
  CoD3RecompLib/levels/saint_lo) holds no readable names (hashed: x*33+c lowercase, tools/coop/scratch).
- Engine: sub_82546998 = melee block (notify "anim_melee::Melee_BlockAttack"; local-only shake/rumble),
  from sub_82567B90.
- Unpacker for study: python -I tools/kapf/extract_plain.py <out> <archive.cod>; image: tools/kapf/xex.py.
NEXT: reach a struggle in a test (Saint-Lo middle zone: position not known yet), hook the action
start to see its arguments (which player), then decide how the joining player can own it.
- SAINT-LO'S FIRST STRUGGLE FOUND (t959u, the user playing the host, 0.5.959 "coop: where:" lines; host clock):
  truck ride 47..524 s (scripted, no input needed; scene count 1..2), off the truck at 1637 -1150 23;
  the wall hop (a prompt) ~547..551 s from ~1557 -1455 over to 1160 -1414 136; THE STRUGGLE 553..583 s
  held at 1160 -1414 136 (view swinging), free at 585 s. Scenes: 524 (1), 543 (2), 551 (1), 583, 592 (0).
  The user: the joining copy (hands off) stayed behind the wall; in the truck his copy "bounced around
  and hasn't taken a proper seat" (riding beside the host 36 units sideways: coop_join.cpp ~1160): to fix.
- 0.5.960 (built): trails for coming back in skip scripted scenes and vertical movement (the user fell out of the
  map coming back beside the joiner, t959u). Scene probe COD3_COOP_SCENEPROBE=1 (coop_host.cpp end, 11 hooks).
- t960a (hands off, Saint-Lo): the host never left the training start (8347 9449): the training steps
  (move/look, Garand, fire, aim ...) need input; a hands-off run cannot reach the struggle without them.
  SCENE PROBE RESULT: sub_824F1E20(r3 node, r4 PARTICIPANT, r5, r6) adds a participant to a scene: at the
  level's intro (46 s) called twice with r4 = the HOST'S PLAYER from script methods at 0x82535A5C and
  0x82535BA0 (r5 = 2), and many times with r4 = another entity (A02251E0, likely the scene's actors) from
  0x824F4938 (inside sub_824F4870). => a scene's player is an argument: giving a scene (a struggle) to the
  joining player means the script method's caller entity (self) being his entity on the host.
NEXT: name the two script methods (0x82535A5C / 0x82535BA0 are inside sub_8253xxxx built-ins: find which
  function holds them and its built-in number), then a test that calls it on client 1 (e.g. the intro scene
  or a struggle reached by the user playing).
- The two script methods that put a PLAYER into a scene: sub_82535A40 (built-in table entry 0x820960A0,
  flags 0x40004D03) and sub_82535B78 (entry 0x820960A8, flags 0x4000C205); the table is {function, flags}
  pairs (0x82096060.. continues sub_82535048, 82535180, ...). sub_82535A40 is also called by sub_8244C248.
  PLAN (not built): (1) when the joining player sets a trigger off (0.5.897 tells it as the host's), note
  it; when one of these methods then runs on the host's player within a few seconds, run it on client 1
  instead (his entity as self); (2) his copy must show it: the arms animation and the locked view are the
  LOCAL client's (cg), so his copy has to start the same scene on its own player with the host's actor's
  puppet: send the method's arguments (R_ message) and call it there. Test first on the struggle the user
  reaches (or the level intro scene at 46 s, which uses the same methods).
- CORRECTION TO THE PLAN (read the two methods): sub_82535A40 / sub_82535B78 are scripted-ACTION handlers
  (r3 the action node, r4 THE PLAYER, r5, r6). Each calls sub_824F1E20, then CANCELS the action (node +0 = 0)
  unless r4 is the LOCAL player ([0x82A2AB40][local index + 1]); after that it sets up the local camera /
  view (sub_823F1D30 with the player's place, ...). So an action can only be run for this machine's own
  player. Running it for client 1 on the host = cancelled (the script may wait for ever); making him local
  for the call = the HOST's camera plays it. => the joining player's struggle has to be RUN ON HIS COPY
  (his camera, his arms, his mash) with the German as a real local actor there for its length, and the host's
  world only told the outcome. Large; not started. Do NOT build "scene on client 1 on the host".

## Night 2026-11-11 22:00-23:30 (the user asleep; 0.5.961 to 0.5.970, all built OK)

- RIDES: the joiner's seat (0.5.961: side of the travel direction, not the host's view; host place filled in
  between messages), riding when the host moves at vehicle speed (0.5.962, Falaise's jeep gun: t962a he
  followed the route), off a ride at the host's own spot / driven ride ends when the host steps 64 from the
  stop (0.5.963), his copy's intro vehicle deleted at the freeze with him on it -> put at the host (0.5.964).
  t966a (TESTGOD): ended ~200 from the host, standing. His copy's own intro still runs till its freeze
  (Falaise 105 s): he can be shot down in it (TESTGOD covers the host's world only).
- RESPAWN out of the map (user, t959u): trails skip scenes and vertical movement (0.5.960).
- HOST BODY FLICKER/T-POSE seen by the joiner (user): FIXED 0.5.970 (new body after a gun change held hidden
  till its pose weighs; t970a 11/13 caught, no T-pose frames; 0.5.968 probe: no other short-of-pose frame).
- Test aids: COD3_COOP_TESTGOD=1 (host: both players' health topped up), COD3_COOP_PREMAP="cmd;cmd" (console
  before spmap), COD3_COOP_FORCEDVAR="name=value" (scripts' setdvar replaced). Saint-Lo start points: forcing
  skipto=2 or start=event2 did NOTHING (t970b/c: still the training start): the struggle needs a played run.
- GPU: Forest/Falaise runs reach 71-79 C now; idle rests ~59-63 (fans off at idle). Keep runs short, scale 0.5.
- WHY DRIVING / STRUGGLES / LIVE-GRENADE THROWBACKS ARE NOT THE JOINER'S YET: they are the engine's scripted
  actions (VehicleLink, Melee*, PickLiveGrenade ...); their handlers (sub_82535A40 / sub_82535B78 seen) cancel
  unless the player is THIS machine's local player, then drive the local camera. The joiner's copy must run
  them itself with the host's actor/vehicle made a real local entity for the action's length. Design work;
  not started. Mounted MGs (misc_mg42) DO work for him (0.5.942-945); triggers, Use, hints, pickups do.
