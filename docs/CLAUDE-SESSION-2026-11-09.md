# Claude session 2026-11-09: co-op campaign (2 players first, then up to 4)

KEEP THIS FILE CURRENT. The user expects session limits; whoever picks up reads this first.
**START AT "WORK ORDER FOR THE NEXT AGENT" (search for it): the ordered list of what is left, with file and line pointers.**

## What the user asked (2026-11-09)
Get the co-op CAMPAIGN (CoD3.exe, not multiplayer) properly in sync, World at War style:
1. Players see each other fully: the other player's crouch/prone/moves (a drawn copy of the
   player as player 2, later up to 4).
2. Grenades in sync.
3. Both interact with the level together; cutscenes in sync.
4. Death rule (the user's, replaces WaW's 30 s bleed-out + revive): a dead player RESPAWNS
   AFTER 5 SECONDS as long as another player is alive; if everyone is down/dead, the game goes
   back to the last checkpoint for everybody.
5. First make 2 players work, then go to 4.
The user gave permission to use the PC (two game copies on this PC for tests). Warn in chat
before each launch (memory: warn-before-game-launch).

### The user's clarification (2026-11-09 ~21:25, their words in short)
Every joining player is their own playable character in OUR single-player world, as if all
play single player together: see each other; ANY player can trigger the level's events;
everything in sync: player animations, grenades, gunfire, jumping, crouching, running.
"It doesn't matter if there is lag, we just need to see each other's true inputs."
So: correctness over smoothness. Prefer showing what the other player really did (his real
stance, shots, throws, jumps, from his own inputs) over guessing it from how he moves.
Consequences for the work list:
- Bodies: today the move is GUESSED from speed and direction (13 moves). Wanted: driven by
  the player's true state (stance, firing, reloading, throwing, jumping/in air, aiming).
- Gunfire of the other player must be seen and heard (muzzle flash, tracer, sound, impacts).
- Triggers: client 1 is a real player entity on the host, so touch triggers should fire for
  him; scripts that test `level.player` only will not. Needs a test per kind of trigger.

## State when this session started
- Source and install 0.5.868 (multiplayer work). Nothing co-op changed since 0.5.671 (the merge
  of Team Friend's co-op v46, 2026-10-31). Last two-PC co-op play: 2026-10-31
  (install folder coop-host-relaunch.log).
- The co-op in the tree IS already the World at War design (docs/COOP-V40-DESIGN.md, read it):
  one world (the host's), the joining copy freezes its own level and draws the host's
  snapshots as puppets; the joining player is client 1 on the host.

## Code map (CoD3Host/, campaign program only, all inside #ifndef COD3_MP)
- coop.cpp: network (UDP 28970/28971, "CO40" packets, reliable channel), frame hook
  sub_82535E80, SV_SpawnServer hook, pause, sector-tree guards.
- coop_host.cpp: client 1 (MakeSecondPlayer, HostClientThink, PlaceSecond), snapshots
  (20/s), config strings, the two BODIES (script_models dressed as a friendly soldier:
  FindTemplate/MakeBody/DriveBody), K_PS (joining player's health etc).
- coop_join.cpp: Freeze, puppets (Bind/SetModels/SetAttach/SetAnims), DriveHostBody,
  the checkpoint reload, pose-queue guards.
- coop_laststand.cpp: down state, G_Damage hook sub_82564888, player die hook sub_8255F320.
- coop_events.cpp: host's sounds/speech/music/earthquake calls replayed on the joiner.
- coop_hud.cpp (ImGui HUD), coop_lobby.cpp (F2 lobby), coop_swap.cpp (reload behind window),
  coop_probe.cpp (COD3_PROBE40 diagnostics: logs fx calls "p40: fx from ...").
- EVERYTHING IS HARD-CODED FOR 2 PLAYERS (g_cmd, g_ent, g_p[2], one peer socket). 4 players
  is a later refactor: peers array, client block of 4, per-peer snapshots/ack.

## Findings from reading the code (not yet verified in play unless said)
- Joining player's body on the HOST: DriveBody reads stance from the host's client-1 record
  (+208 & 0x20 crouch, +184 <= 20 prone), NOT from the stance his copy sends (K_CMD
  CmdIn.stance = his own record +208, sent but never used). Suspect for "I cannot see his
  crouch". Fix planned: send his own +184 and +208 and use them.
- Host's body on the JOINER: stance bits ride in snapshot `extra` (4 crouch, 8 prone);
  DriveHostBody picks the move. Needs a test to see if the crouch move exists in the list
  (log: "the host's body plays move N", "none of the moves in its list").
- EFFECTS ARE NOT REPLICATED (design doc "Not done yet"): the joiner sees no explosions,
  muzzle flashes, tracers or impacts of the host's world. A host grenade is a script_model
  puppet that just vanishes. This is most of "grenades not in sync".
  Leads: FX play sub_82423F08 (r3 id or -1, r4 effect def, r5 place); event executor
  sub_82533208 (file ppc_recomp.70.cpp) calls sub_8242F588 / sub_82423F08; other callers
  sub_824245D8, sub_82429238, sub_82429330. Sound twin already forwarded: sub_82534ED0.
  MP twin knowledge: mp_fixes.cpp ~16748 (sub_82555160 in MP = a weapon's numbered event).
- The joiner's own grenades: his local missile entities (type 3) are hidden every frame
  (coop_join.cpp ~1015); he sees the host's copy as a puppet ~100+ ms later.
- Joining player's weapons/ammo are NOT synced (only health): pickups happen on the host's
  client 1, his copy keeps its own inventory. Open.
- Cutscenes: films (bink) are synced (K_FILM). Level intros play separately on each copy
  before the freeze; mid-level scripted scenes: joiner "rides beside the host".

## Test tool
tools/coop/coop_test.py (also in this session's scratchpad): starts host + join on this PC.
  python -I tools/coop/coop_test.py TAG SECONDS --host-pad "..." --join-pad "..." \
      --host-shots "t t" --join-shots "t t" [--env K=V] [--host-env K=V] [--join-env K=V]
Output: coop-out/TAG/host.log, join.log, shots/ beside the script. It backs up the install
logs first (logs-backup/<stamp>-coop-TAG-before) and kills every CoD3.exe at the end.
Pad script times are seconds from each process's start; the join starts 40 s after the host.
Useful env: COD3_COOP_TESTDOWN=t (joining player goes down t s into the mission),
COD3_PROBE40=1 (diagnostics, noisy).

## Log of this session
- 16:0x read docs + all coop*.cpp. Wrote the test tool.
- Baseline run "base1" (Forest, 380 s, install 0.5.868 unchanged) started ~16:25. RESULT: see below.

## Plan (in order)
1. Baseline numbers from base1 (what works on 0.5.868).
2. New death rule in coop_laststand.cpp + coop_hud.cpp + a host teleport helper
   (5 s down, respawn beside the living player, everyone down = checkpoint).
3. Stance of the bodies both ways.
4. Effects/explosions to the joiner (forward the host's fx/event calls).
5. Build 0.5.869 (PATCH-NOTES entry, build-request.txt), two-copy tests, then a friend zip
   only if the user asks.
6. Later: weapons/ammo sync, cutscene camera, 4 players.

## Results / changes (append below)

### base1 (install 0.5.868 unchanged, Forest, two copies on this PC, 380 s)
Logs: this session's scratchpad coop-out/base1 (may be gone later; the facts are here).
- WORKS: lobby by COD3_COOP_MAP, film, join's load, second player made (entity 287, health
  600), freeze (35 soldiers/drones deleted), 85-97 puppets + 6-7 adopted, both bodies drawn
  with name tags (screenshot: the joiner's body seen by the host, standing), speech forwarded.
- The host's body on the joiner only ever played moves 0 and 1 (stand idle / walk) although
  the host's pad script pressed B (crouch) at 250 s. No stance was logged anywhere, so it is
  not known whether B crouched him or the stance read is wrong: 0.5.869 logs it.
- One-PC artifact (probably): 3-4 fps on both, a "GPU is hung" guest message, and the host's
  watchdog "no swap for five seconds" near 300 s. Two copies starve each other; do not read
  frame rates from these tests.
- Seen again: guest DbgBreakPoint at 0x825239B0 from 82512DB0 on the host as the second
  player is made (as on 2026-10-31); both ran on.
- Timings for pad scripts (host process clock): mission in at ~150 s, second player ~165 s,
  joiner frozen ~185 s. Level time 34 s at the second player's making.

### 0.5.869 (source changed, build queued 21:07)
Files: coop_laststand.cpp (rule), coop_host.cpp (HostRespawn, the joiner's trail g_trail2,
stance from K_CMD, stance log), coop_join.cpp (view height in K_CMD flags, stance log),
coop_hud.cpp (texts), coop.h (HostRespawn). PATCH-NOTES 0.5.869 has the details.
- Rule: any death -> down 5 s (COD3_COOP_RESPAWN) -> HostRespawn(who) beside the other, full
  health, 3 s safe. Other already down -> g_allDown -> MissionFailed (host's own death ->
  the game's checkpoint; the joiner follows by the existing reload/swap path).
- The revive code is still in LastStandFrame but unreachable (a `continue` before it).
- FIRST BUILD FAILED (21:08, not our code): the "JARC campaign menu polish" patch installed
  2026-11-08 22:46 (D:\Call of Duty 3 (USA, Europe)\README.txt, Install-Menu-Code.ps1) added
  CoD3Host/DividerFilter.h with two campaign menu class addresses (0x82071DDC, 0x82071724)
  that have no multiplayer twin: scripts/make_mp_host.py refused, CMake stopped. The source
  had not been buildable since that install (the installed CoD3.exe of 22:46 is their
  prebuilt one). Fix: IsMenuClass is inside #ifndef COD3_MP (false in multiplayer). The
  original header is in this session's scratchpad (DividerFilter.h.before). Requeued 21:15.
  NOTE: their RESTORE-MENU-CODE.cmd refuses files changed after install (this header now is).
- BUILT 21:13: 0.5.869 OK (source and install; CoD3.exe 21:13). Nothing committed, no zip.
- Test t869a started 21:20 (tools/coop/coop-out/t869a): TESTDOWN=60, TESTDOWN0=110, crouch
  presses host 230/244 s, join 205/216 s. Result below if this session got that far.
- t869a: NO RESULT. The host stalled in Forest's opening film ("coop: film: playing", then
  "watchdog: no swap for five seconds"; guest stack ... 8212DF10 824EECC0), the joiner was
  never told of the mission. A two-copies-on-one-PC hazard (the old LOCAL-COOP-TEST.bat
  warns of it; base1 passed the same spot). Not the new code: neither rule nor stance ran.
  Rerun as t869b at ~21:35 with the same settings.
- t869b: NO RESULT either, and the user saw "one of the games froze". Cause: the test's own
  scripted A presses (every 4 s from 30 s, meant for the film). The joiner connected late, so
  the presses walked the host's main menu and picked Multiplayer: the host process started
  CoD3MP.exe and ended; the joiner sat frozen. NEVER script A before the mission is up
  (memory: mp-main-menu-row4...). The runner now also kills CoD3MP.exe.
- 0.5.870 (queued ~21:45): COD3_SKIPFILMS=1 counts as the host's A on the film in co-op
  (title_fixes.cpp), so tests use NO presses before the mission; COD3_COOP_FXLOG=1 logs the
  first 400 fx calls (coop_probe.cpp sub_82423F08) without the rest of COD3_PROBE40.
  Test line to use from now on:
    python -I tools/coop/coop_test.py TAG 350 --env COD3_SKIPFILMS=1 \
      --host-env COD3_COOP_TESTDOWN=60 --host-env COD3_COOP_TESTDOWN0=110 \
      --host-pad "230:b 244:b" --join-pad "205:b 216:b"
- 0.5.870 BUILT 21:28 (the builder's clock; source and install).
- t870a: NO RESULT. The host froze at ~90 s in the title's ATTRACT film (nobody pressed
  Start; guest stack 823494F8 8212A560 8212A17C 8213DAB8 8212DF10 824EF508 824EF9B8 824CF174:
  the film player waiting on the GPU's sync, critical section 0xABBE0104 with a garbage
  owner). THREE of four runs stalled in a film (t869a mission film, t870a attract, base1 at
  ~300 s in play with the same 8212DF10 frames). Open question: is this only two copies on
  one PC, or does a single copy freeze in films too since the JARC render patch? ASK THE
  USER whether solo campaign freezes in videos. Not investigated further.
- t870b (~22:00): same test with --env COD3_NOFILMS=1 (kernel_file.cpp: films not opened)
  and COD3_SKIPFILMS=1. USE BOTH for every co-op test.
- **t870b: PASSED (0.5.870, films off, Forest, 360 s; tools/coop/coop-out/t870b).**
  - Respawn, joining player: "is down (means 2): back in 5 s" -> "comes back in at 1293
    -6989 375" -> "is back in beside the host (health 600)"; join log "the host makes room:
    I come in at 1293 -6989 375". Prone while down (view height 11).
  - Respawn, host: "the host is down" -> "the host comes back in at 1293 -6991 334 (the
    joining player at 1293 -6991 330)" -> back in, health 600. The joiner had not moved, so
    his trail had no point 48 away and the host came in ON him; the old "in each other: room
    made" rule then moved the JOINER (join log: third "makes room ... 1306 -7046 354").
    Works, but the wrong player is moved: better to give the host a free spot (TODO).
  - Stance VALUES confirmed from the game: standing = view height 66, flags 0x18; crouched =
    40, 0x38 (bit 0x20); prone = 11, 0x58 (bit 0x40). Record +184 and +208 are right.
  - Joiner's crouch/prone reach the host ("the joining player's stance: ... crouched/prone").
  - Host's crouch reaches the joiner: "the host's body plays move 6" (crouch idle) and the
    joiner's screenshot shows the host's body crouched in front of him with his name tag.
  - While DOWN the host logged "crouched" (40), not prone: DownStance may not force the
    host's own player prone (the joiner's did go prone). Minor, open.
  - With films off both copies ran 48-80 fps (not 3-4): the slow runs were the films.
  - One "watchdog: no swap for five seconds" on the host early (after 6761 frames, while
    the joiner loaded); it ran on.
  - NOT covered: both down at once (checkpoint for both), real deaths by the enemy, two PCs.
  - FX log (COD3_COOP_FXLOG, 78 calls in 6 quiet minutes) for the effects work: callers of
    sub_82423F08 by return address: 82533634 (event executor sub_82533208, r3 = 8),
    8242F6F4 (inside sub_8242F588: effect on an entity, r6/r7 entity refs, r8 a small
    number = tag/bone?), 82424660 (sub_824245D8, f1 = a time), 82429318. The effect def (r4)
    has NO name at +0 (floats: 3F800000 0 0 0), and r5 is not a place (a unit vector / matrix
    row). So forwarding needs: (1) a def -> name (or def -> table index) mapping that is the
    same on both copies, (2) the place/matrix layout at r5. Not done.
- The user said "Yes, effects next" (~22:15).
- 0.5.871 (log only) + test t871a (both players fire and throw, COD3_COOP_FXLOG=1 on both):
  **the effect records (r4 of sub_82423F08) are at the SAME guest addresses on both copies**
  (host 14 distinct, join 14 distinct, every one both used is the same address with the same
  words before it, e.g. A66EC6D0, A66F3CE0, A66ED870, A63F1F60). Same PC, same build; two
  PCs not checked, hence the checksum below.
  sub_82423F08's arguments: r3 id (6 or 8 seen), r4 record, r5 a matrix of 4 x 16 bytes
  (origin in row 4, r5 + 48), r6/r7 entity REFERENCES it follows (index | generation << 14),
  r8 a tag (u16), r9 a flag. sub_8242F588 (effect on an entity) builds the matrix and calls it.
- 0.5.872 (queued ~22:25): coop_events.cpp FxForward (host; called from the sub_82423F08
  hook, which lives in coop_probe.cpp) -> K_FX (unreliable, 92 bytes) -> coop_join.cpp
  JoinReceive -> FxReceive -> FxFrame (from EventsFrame once frozen) calls sub_82423F08 with
  the record checked by a sum of its 48 words. Log lines: "effects sent to the joining copy",
  "the host's effect ... played at", "not played: the record at ... is not the same here".
  Patch script kept: scratchpad fx_patch.py. NOT TESTED when this was written.
  Test to run: coop_test.py t872a 330 --env COD3_SKIPFILMS=1 --env COD3_NOFILMS=1
    --host-pad "215:rt/2 225:rs 240:rt/1 262:rs" --join-pad "190:rt/2 200:rs"
    --join-shots "176 177 178 186 187 188 189 201" (host fires at 215 = join clock 175).
- 0.5.872 BUILT, test t872a: the pipeline WORKS (host "54 effects sent", join "the host's
  effect ... played at ..." 10 times, no crash, join 59 fps), but 57 of 67 were refused by
  the 48-word sum: the record holds words that change as the game runs (A6706110 played once,
  refused later). An effect following the host's player came through as "follows 0010 FFFE"
  and played unattached at his muzzle's place (1277 -6911 417). One odd one: A66F1BB0 from
  the sub_824245D8 path arrives with place 0 0 0 (its r5 is something else?): harmless so far.
- 0.5.873 (queued ~22:40): FxCheck = words +0,+4,+8,+12,+80,+84 only. If refusals remain,
  log which words differ, or drop the check and trust the address (same build, same level).
  Screenshots of t872a NOT looked at (too few effects played to judge).
- 0.5.873 BUILT, test t873a: the narrowed check refuses NOTHING now. 42 played, 57 refused
  for "matrix not right" (silently). Cause: callers sub_824245D8 (LR 82424660) and
  sub_82429238/82429330 (LR 82429318) make the effect and PLACE IT AFTERWARDS: at
  sub_82423F08 their r5 has no place (played at 0 0 0: A63F1F60 x17, A66F1BB0 x9) or is no
  matrix. The two good paths played right: A67053E0 "follows FFFF 0113" (a thing), and the
  host's muzzle A66EF450 / A66F83D0 "follows 0010 FFFE" at 1261 -6909 418.
- 0.5.874 (queued ~22:55): FxForward sends only LR inside sub_82533208 (event executor)
  or sub_8242F588 (effect on a thing); the rest counted ("effects not sent so far"); a
  refused matrix is logged with its caller and 16 floats. TODO LATER: hook sub_824245D8 and
  sub_82429238/82429330 themselves (the scripts' playfx / looping fx?) to send theirs.
  (A bash heredoc ate the \n in two printf strings while patching: repaired with
  scratchpad fx3b.py. USE THE Write TOOL FOR PATCH SCRIPTS, as memory says.)
- **0.5.874 BUILT 22:18, test t874a (tools/coop/coop-out/t874a): PASSED.**
  - Join: "461 of the host's effects played, 24 not". Host: "498 effects not sent so far
    (made by callers that place them later; the last from 82429318)".
  - The joiner's screenshot (shots/CoD3-20261109-222254.png) shows smoke puffs where the
    enemy soldiers stand and fire, and a fire glow to the right: the host's world's effects,
    on the joining copy for the first time.
  - Most "follows" pairs resolve to nothing there (e.g. 0008/0003, 002C/0105: the host's
    entities with no model are not replicated, so there is no puppet): those play where
    they were made, which is right for a thing that stands still. One resolved (00E9).
  - Not looked at: the host's own muzzle flash and his grenade's burst seen from the joiner
    (the two were not facing each other at those seconds). No crash, no watchdog.

## WORK ORDER FOR THE NEXT AGENT (written 2026-11-09 ~23:30, the user's latest words)

The user, last message of the session: NO friend zip yet. Write the handoff, then get the
rest of the co-op in sync: "players can see other players and everything included,
animations, grenades, effects, crouching, going prone, running, reloading, everything that
another player does should work flawlessly, including triggering events or being in
cutscenes together. Also make the character revivable if they go down, so change the
bleedout/respawn timer to 10 seconds instead of 5."

NOTHING below is started. Source and install are 0.5.874. Do them in this order, one build
and one two-copy test each, and update this file + PATCH-NOTES after every build.

### PROGRESS AFTER THE WORK ORDER (newest last)
- **0.5.875 (built 22:42, tested t875a): ITEM 1 DONE.** Bleedout 10 s. Revive reachable again
  (the `continue` removed); a revive under way holds the 10 s clock; revived = full health,
  3 s safe, FL_NOTARGET cleared. Use is NOT read from record +1352 any more: it is the pad's X
  (input.cpp maps the keyboard's Use and Reload to X): host = Input::Buttons() & 0x4000, joiner =
  K_CMD flags 0x20000 (JoinerUseHeld in coop_host.cpp, fresh commands only). COD3_COOP_USEMASK
  is gone; COD3_COOP_TESTUSE stays. COD3_PAD holds buttons now ('200:x/6'). HUD texts changed.
  ProtocolVersion 40003.
- **t875a (Forest, 330 s, TESTDOWN=60 TESTNEAR=1 TESTDOWN0=95, host pad '150:x/180', join pad
  '110:x/220'): PASSED.** Joiner down: 'back in 10 s', countdown 10..2, not revived (the host's
  X hold had not begun), 'is back in beside the host (health 600)': the 10 s respawn works.
  Host down: at ~0 s left 'the joining player holds Use' -> 'is reviving the host' -> the clock
  held (-0, -2 s left) -> 'the host revived' 3 s later. So the joiner's real pad X reaches the
  host and revives. No watchdog, no crash.
  NOT covered: the HOST reviving the joiner with his own pad (same Input::Buttons call the
  joiner's copy uses, but not run), the HUD texts on screen (no screenshots), both down.
- **0.5.876 (built 22:56, tested t876a): ITEM 2, the main part.** Reading ppc_recomp.52.cpp:
  sub_82429238(r3 id, r4 record, r5 place, r6 3x3 axes) and sub_82429330(r3, r4, r5 place) build
  a WHOLE matrix on their stack and pass it to sub_82423F08; only the 4th float of each row is
  never written (stack leftovers), and FxFrame's finite/size check refused them for that (so
  did 24 of the event executor's own in t874a). The work order's 'placed after the call' was
  wrong for these two. Now: FxForward also sends LR in 82429238..824293E8, the host writes the
  four 4th floats as 0 0 0 1, FxFrame sets them again. sub_824245D8 is the only true
  'by handle' maker (identity matrix at 0 0 0, returns handle +28 of the emitter; no direct
  callers in the recompiled code, so it is called through a pointer): not sent, counted by its
  own caller (saved LR at r1+168 inside sub_82423F08).
- **t876a (same pads as t872a): PASSED.** Join: 1161 of the host's effects played, 0 not.
  Host: 'effects sent, by caller: 82429318 x607 8242F6F4 x535'; 'effects made by handle
  (sub_824245D8), by caller: 820FBD70 x28'. Watchdog 'no swap for five seconds' once on the
  host, 4 times on the join (it ran at 3 fps at times: two copies on one PC); both ran on.
  Joiner screenshot 230048: the host's body stands in a proper idle with his rifle and name.
- LEFT OF ITEM 2: (a) the 28 by-handle effects: find what 820FBD70 is (a script builtin?
  playloopedfx?) and how it moves the handle (sub_82424320/82424350/824243C8/824244A8 take
  handles); (b) effects that follow a player play unattached / the joiner's own are not sent.
- **USER REPORT (23:05, while t876a ran): 'still seeing the other player in a tpose position
  as the idle stance'.** Not reproduced in the joiner's screenshot. Suspects, host side
  (DriveBody, coop_host.cpp ~646): body with no tree returns silently; an idle that ends
  between two frames at low fps frees its slot (the 0.97 restart is missed) and is blended in
  again from nothing; the body hidden/shown flips ~16 times in a test ('his copy is away or
  loading': g_cmdAt older than 1.5 s). Asked the user which screen and whether in his own play.
#### T-POSE: WHAT IS KNOWN (23:30, source+install 0.5.878, NOT fixed)
- t876a (nobody moved): the joiner's shot 230048 shows the host's body in a proper idle with
  his rifle. t877a and t878a (the user moved both players: walk, run, crouch, prone): the
  other player's body stands arms out, rifle hanging, on BOTH screens (t877a shots 231432
  host view, 231527 join view; t878a shot 232644 host view). So it starts after other moves
  have been played, or after some time; standing still from the start it was right.
- 0.5.877: both sides log every 10 s 'body: ... move, animation, slot, time, weight (to goal);
  bare N s' and restart the idle one frame's step before its end; a move set when nothing
  played comes in with no blend. Did not cure it.
- 0.5.878: moves set with sub_82500B80 (= sub_824FE120 goal weight, sub_824F0E38, sub_824F7170,
  sub_824F1B60: the soldier puppets' call) instead of sub_82500A40; a 'body's tree: N nodes
  held, weighing: A animations, P parent nodes' line. t878a: host says '13 nodes held,
  weighing: 1 animations, 12 parent nodes', idle 168 at weight 1.00 with its time running,
  AND the host's shot 232644 still shows the T-pose. So neither the node's weight nor its
  parents' explain it: the drawn pose is not coming from what these fields say.
- Odd numbers to chase: (1) 'bare' (weight <= 0.3 or no slot, sampled each frame) still grows
  by up to 7 s in 10 s while the 10 s line prints weight 1.00 and the slot number changes
  between lines (224 -> 177): the node is let go and made again in between, yet 'replays'
  stays 1, so it is not DriveBody's own replay path making it: something else (the engine's
  tree update? MirrorTemplate? the body hidden/shown flips?) frees and remakes nodes.
  (2) animations 333/336/250/231 always print time 0.00 and 'restarts' runs into the hundreds
  and thousands: the one-frame-early restart fires nearly every frame on those (their time
  field steps coarsely, or is not 0..1): those moves are FROZEN at their first frame now.
  Take the early restart out again or limit it (step < 0.1). (3) late in t878a the join's
  g_lb was reset again and again (restarts back to 0, bare 20 -> 3.5, slot numbers climbing
  590 -> 848 -> 1276, weight stuck at 0.03): the host's body's tree address changes, i.e. the
  body puppet is being rebound/remade over and over there.
- NEXT STEP PROPOSED: stop guessing. With COD3_PROBE40-style code, dump side by side every
  held node (index, hash, the 52-byte record) of (a) a real soldier standing idle and (b) the
  player's body in its T-pose, on the host, once a second, and take a screenshot at the same
  second. And check whether the body's pose is posed at all: the join log's 'pose jobs' /
  'pose queue posed early' lines belong to that machinery (coop_join.cpp ~1160).
- t878a also: join stalled (watchdog) 3 times, host once; both ran to the end.
#### T-POSE: FOUND AND FIXED (0.5.879 dump, 0.5.880 fix, test t880a PASSED, 23:50)
- 0.5.879: DumpTree (coop_host.cpp) prints every held node of the joining player's body's
  tree and of the soldier it was dressed as (g_template): 'coop: dump body/soldier node K
  hash e4 e6 slot t w g: 13 words'. Early restart limited to steps under 0.1.
- WHAT THE DUMP TAUGHT (t879a): an animation list entry is 28 bytes: +0 name hash, +4 (u16)
  count of children (0 = an animation, else a parent node), +6 (u16) the parent's index.
  Info record (52 bytes at InfoPool + 52*slot): +16 time, +20 old time, +32 goal weight,
  +36 weight, +40 rate, +44 a pointer on animations. generic_human: 0 root <- 1 <- 2 <- 6
  <- 168 (stand idle); 189 (parent, under 6) <- 222 (parent). The idle body held 168 and
  6/2/1/0 at weight 1 AND 189 and 222 at weight 1 with no animation under them (left by an
  earlier move's animation ending). A parent node with nothing under it = the bind pose,
  blended by node 6 with the idle: arms half out.
- 0.5.880: TidyBodyTree(base, tree, anim) in coop_host.cpp (declared in coop.h), called each
  frame for both bodies on the host (Bodies) and for the host's body on the join
  (DriveHostBody): parents above the move playing get weight = goal = 1; a parent node with
  no weighing animation anywhere under it gets 0. Log: 'N parent nodes with nothing under
  them emptied' (30 each side in t880a). Moves are still set with sub_82500B80 (0.5.878).
- t880a: the user moved both players (host log: moves 0, 1, 3, 11; join: 0, 1, 2, 4). Late
  shots: 234935 (host's view: the joining player stands in his idle, rifle in both hands),
  234938 (join's view: the host in his idle). Tree at idle: '5 nodes held, weighing: 1
  animations, 4 parent nodes'. Host 0 watchdogs, join 4 (5 s stalls, ran on).
#### AFTER THE T-POSE: small things seen on the way (open)
- On the join the host's body flips walk forward / walk back (moves 1 <-> 2) every frame
  while he walks: direction from the difference of two track points is noise. Use the
  host's velocity (send it in K_PS / the snapshot) or smooth it.
- 'bare' seconds still add up a little (8.6 s host / 4.3 s join in t880a): mostly the 0.2 s
  blends at each change of move (counted as bare by the 0.3 weight line), not checked further.
- The dump (every 10 s, 30 times) and the 10 s body lines are still in: take them out or
  put them behind an env switch when the body work (item 3) is done.
- Late in t878a the join's g_lb was reset over and over (the host's body's tree address
  changing): not seen again in t880a's last lines (slot 851 steady), not explained.
#### ITEM 2 FINISHED: the by-handle effects (0.5.881, test t881a PASSED, 2026-11-10 00:05)
- 820FBD70 is inside sub_820FBD00(r3 = 8290B5D0): a list of waiting effects, 64-byte entries,
  count at +4096 (room for 64). Per entry: sub_824245D8 through a vtable (+0 the effect's
  record, +4 a time, +52 the id), then sub_820FE4D0(out, entry+8, entry+32) builds its place,
  sub_820E9D70(handle, place) sets it, and entry+48 (a float) goes to handle+108.
- Host: PPC_FUNC(sub_820FBD00) in coop_probe.cpp calls Coop::FxQueueForward (coop_events.cpp)
  before the game's own: each entry sent whole as a 72-byte K_FX message (FxQMsg, told from
  FxMsg by size). Join: FxReceive queues them, FxFrame checks the record (FxCheck) and that
  words 1..12 are sane floats, appends to its own list at 8290B5D0 and calls 0x820FBD00.
- t881a (pads as t872a): host '188 waiting effects sent' (last print), join '199 of the
  host's waiting effects played, 0 not'. No watchdog on either side, no crash (the one
  'exception 0x406D1388' at startup in every log is the thread-name one).
- TO CHECK LATER: ordinary effects in t881a: host 1027 sent, join 712 played, 0 refused (in
  t876a 1147 / 1161). Unreliable packets, the join's 256 queue / 48 a frame, and effects sent
  before the join was in the world are all uncounted: add a 'dropped' count on the join
  before believing the gap is harmless. The host's line 'N effects not sent so far (made by
  callers that place them later)' is now wrong for these (they ARE sent by the list): reword.
- Left of item 2 on purpose: effects that follow a player play where they were made; the
  other player's shots (flash, sound at his body) go with item 3.
#### HANDOFF 2026-11-10 00:45 (the user stopped for the session limit)
State: source + install 0.5.884, built OK. Nothing committed, no zip. No game running.
Builds since item 2: 0.5.882 (moves by name, fire/throw/aim), 0.5.883 (reload), 0.5.884
(melee and grenade the right way round, one action code). Tests t882a, t883a, t884a.

**1. BUG TO FIX FIRST (the user's rule 1): a real grenade death skips last stand.**
t884a host.log ~line 7900-8268: menu 'MISSION FAILED / YOU WERE KILLED BY A GRENADE', then
'the mission started again here (count 2)'; NO 'last stand: the host is down' line. The host's
weapon state sat at melee/odd for 10 s before (dead). So the death did not come through
sub_8255F320 (the player's die, where LastStandTakes is asked) or LastStandTakes said no.
Look at: LastStandTakes (coop_laststand.cpp) for a means/attacker exclusion (grenade means,
own grenade = attacker is self?), and whether the scripts fail the mission themselves on a
grenade death (a missionfailed script call: hook where 'MissionFailed' is handled; the host
log has 'server command ... scr_fade' just before). Test: scripted grenade at the feet
(pad 'rs' looking down, or COD3_COOP_TESTDOWN-style real damage with means = grenade).
Forced downs (TESTDOWN) work; REAL deaths were never verified and this one failed.

**2. Item 3, what is in and proven**
- Animation NAMES are readable: scratch tool names.py (read sp/anim.cod + the level's .cod,
  hash = h*33 + lower-case letter) wrote every word with its hash. Table BodyMoveName in
  coop_host.cpp (33 moves; BodyMoveHash / BodyMoveText / BodyAct / HostActBits in coop.h).
- The player's record: +136 weapon state (0 ready, 1 raising, 2 lowering, 3 firing, 4 bolt,
  5 reloading, 10/11 melee, 15 throw), +668 rounds in the clip, +672 a grenade count, +1280
  weapon animation number (| 0x200 toggle: 2 fire, 8 melee, 9 lower, 0xA raise), +1352 buttons
  (1 fire, 2 GRENADE, 8 aim, 0x10 MELEE; the older note 'rs = 0x10 = grenade' was wrong).
- One action code per player: 1 aim, 2 fire, 3 throw, 4 reload, 5 melee. Joiner -> host in
  K_CMD flags (0x40000 fire, 0x80000 throw, 0x100000 aim, 0x200000 reload, 0x400000 melee);
  host -> joiner in the high 4 bits of his body's snapshot 'extra' byte.
- t884a: host log 'the host does N' and join log 'the host does N' agree line for line (40 s
  clock offset) until the checkpoint; join played stand_shoot_straight, stand_grenade_throw,
  melee_right2right_1; host played the joiner's shoot, throw, melee (and reload in t883a).
  t882a shot 001627: the host seen with his rifle at his shoulder while firing.
- NOTE for reading tests: when the host goes back to a checkpoint the joining copy SWAPS to
  a new process; coop_test's join.log is the old one and stops there (that is why t883a/t884a
  looked like 'the join heard only some').

**3. Item 3, still to do**
- Jump / in the air (no ground field mapped: log shows 'velocity up ~50' at jumps; find the
  ground entity word with the record log around the A button, whose bit is not in 0x17).
- The aim's pitch (view +172) on the body; the weapon he holds (attach by world model name).
- His shots' flash and sound at his body on the other screen (effects that follow a player).
- Screenshots of throw, melee, reload (only firing was seen in a picture; the rest in logs).
- On the join the host's body flips walk forward/back each frame while walking.
- Take out the test logging when done: the 3..10 s tree dump, the 10 s body lines, 'record
  after buttons', 'weapon state', 'does N'.

**4. Then** items 4..9 of the work order, and the effects count gap (t881a: 1027 sent, 712
played) to measure.

### 1. Rule change: down 10 s, revivable (small, do first) -- DONE 0.5.875, see PROGRESS above
File CoD3Host/coop_laststand.cpp (+ coop_hud.cpp texts).
- Bleedout(): default 5.0 -> 10.0 (env COD3_COOP_RESPAWN stays).
- Revive is still in LastStandFrame but UNREACHABLE: 0.5.869 put
  `continue;   // (no revive: the 5 s bring him back)` right after the respawn block.
  Remove that line; the code under it (near + UseHeld -> p.revive over ReviveTime() 3 s ->
  full health) runs again. On a finished revive also set p.safeUntil = now + 3.0 (as the
  respawn does) and clear FL_NOTARGET (already there).
- Rule to keep: revived within 10 s = up where he lies; not revived = at 10 s he respawns
  beside the other player (HostRespawn, as now); both down = checkpoint for both (g_allDown).
- THE USE BUTTON IS NOT VERIFIED. UseHeld() reads record +1352 & 0x40 (COD3_COOP_USEMASK).
  Seen in logs: +1352 bit 0x1 = fire, 0x10 = the grenade button (pad "rs"), 0x2 = ? (pad "b"
  made 0->2). Find Use by a test: the pad script cannot HOLD a button (presses last 2 reads),
  so either add a hold for buttons to input.cpp's COD3_PAD ("40:x/3") or log +1352 while
  the user presses Use. If it cannot be settled, make revive "stand within reach for 3 s"
  (no button) and say so on the HUD. For client 1 the record on the host gets his buttons
  from his K_CMD command (HostClientThink), so +1352 of g_rec should follow his pad.
- HUD (coop_hud.cpp ~103-137): 0.5.869 changed the texts to "DOWN n", "<name> is down:
  back in n s", "Back in beside <name> in n s" and REMOVED the line
  `else if (s.down[other] && s.nearDowned) Text(... "Hold [Use] to revive" ...)`: put a
  revive prompt back, and "<name> is reviving you" is still there (s.revive[me] > 0).
- Tests that exist: COD3_COOP_TESTDOWN=t (joiner down), COD3_COOP_TESTNEAR=1 (the host put
  beside him), COD3_COOP_TESTUSE="a b" (the host holds Use from a to b s of mission time),
  COD3_COOP_TESTDOWN0=t (host down). Both down: TESTDOWN=60 + TESTDOWN0=62 (never run).

### 2. The effects that are not sent yet (~500 a test)
coop_events.cpp FxForward only sends when the caller of sub_82423F08 is sub_82533208 or
sub_8242F588. The others (sub_82429238 / sub_82429330, return address 82429318; and
sub_824245D8, return address 82424660, f1 = a time) make the effect and place it AFTER the
call, so at sub_82423F08 there is no place. Read those three in
CoD3RecompLib/ppc/ppc_recomp.52.cpp (lines ~23860, ~35038, ~35147), find where they write
the matrix into the emitter that sub_82423F08 returned (r3), hook THEM, and send the same
K_FX message from there. Also still open: an effect that follows the host's PLAYER plays
unattached on the joiner; one that follows the JOINING player is not sent at all (so the
host's view of the joiner's shots depends on the host's own game playing them for client 1:
check that the host sees and hears the joiner's gunfire).

### 3. The other player's body from his TRUE state (the big one)
Today: coop_host.cpp DriveBody (the joiner's body on the host) and coop_join.cpp
DriveHostBody (the host's body on the joiner) pick 1 of 13 moves (MoveHash[]: stand/crouch
idle and walks, run, prone idle/crawl) from speed + stance. No firing, reloading, throwing,
jumping, aiming, melee, death. The user wants every action seen ("true inputs", lag is OK).
- What each side must send: stance (done), buttons (+1352: fire 0x1, grenade 0x10, ...),
  weapon state (the record's weapon/anim fields: NOT mapped yet), on ground or in the air
  (record: ground entity field, NOT mapped; velocity z is +24), view pitch (+172).
  Joiner -> host: K_CMD (struct CmdIn, both files must match). Host -> joiner: K_PS.
- Animations: the bodies use the level's "generic_human" list (1179 animations in Forest).
  Names are hashes (list entries are 28 bytes, +0 the hash). To find fire/reload/throw/
  jump/pain/death animations: run with COD3_PROBE40=1 and COD3_PROBE40_ANIMS (coop_probe.cpp
  ~314: logs what soldiers play standing/crouched/prone) while AI soldiers shoot, reload
  and throw, and take the hashes they use. The pitch of the aim could use the soldiers' aim
  blend animations the same way.
- Better long-term (WaW's way): give client 1 a real third-person player model on the host
  driven by the engine's own player animation code, if the campaign has it (it draws no
  player body; multiplayer does). Not investigated.
- The gun: the body wears the template soldier's rifle. It should show the player's
  current weapon (attach by the weapon's world model name).

### 4. Grenades
Now: the host's grenades are script_model puppets on the joiner (they fly, then vanish);
the burst's effect should now arrive by K_FX (not yet seen up close: test with the two
players facing each other; coop_test.py pad "rs" throws). The joiner's OWN grenade: his
local missile is hidden every frame (coop_join.cpp ~1021, type 3) and he sees the host's
copy of it ~100 ms later; make sure it appears at all (the host throws it from client 1's
commands only if client 1 HAS grenades: see 5). Explosion sound: "sound on a thing"
(sub_82534ED0) is already forwarded.

### 5. The joining player's weapons and ammo
Only health is synced (K_PS). Client 1's record on the host started as a COPY of the
host's record (MakeSecondPlayer: memmove rec0 -> rec), then lives its own life: pickups
and ammo use happen there, while the joiner's copy keeps its own inventory. They drift: the
joiner can fire a gun or throw a grenade his client 1 no longer has. Fix: the host sends
client 1's weapon/ammo part of the record in K_PS and the joiner writes it into its own
record (same 2224-byte layout on both). The offsets of weapons/ammo in the record are NOT
mapped: diff the record before/after a shot, a reload, a pickup, a weapon switch.

### 6. Triggers and level events by the joining player
Client 1 is a real player entity (type 1, team 2 sentient), so touch triggers should fire
for him, but scripts that test `level.player` (the host) will not. Test: a level with an
early trigger (Forest's first advance, Saint-Lo), the joiner walks in first, the host stays
back; watch the host's log for the scripts moving on. If they do not: find the script
"istouching/trigger" checks, or (simple and probably enough) when client 1 is inside a
trigger's bounds that the host's player has not fired, fire it for the host's player.
Also: use-triggers (doors, planting, mounted guns) for the joiner.

### 7. Cutscenes together
Films (bink) are shared (K_FILM). Level intros play separately before the freeze. During
the host's mid-level scripted scenes the joiner only "rides beside the host" (coop_join.cpp
~953-970, g_hostFlags & 2) with no camera. Wanted: the joiner's view follows the host's
scene camera (send the host's view origin/angles while scenes > 0 or letterbox is on, and
hold the joiner's view to it), HUD/letterbox dvars are already forwarded.

### 8. Small things seen in tests
- The host respawns ON the joiner when the joiner has not moved (his trail g_trail2 is
  empty), then the old "in each other" rule moves the JOINER away. Give the host a free spot.
- The host shows crouched (view height 40), not prone, while down; the joiner goes prone.
- Films freeze the game when two copies load (3 of 4 tests). ANSWERED by the user (end of
  2026-11-09): solo play "has not frozen in a bit to my knowledge". So treat it as a
  two-copies-on-one-PC hazard: keep both film switches in tests, do not chase it.
- ProtocolVersion (coop.h, 40002) was not bumped although K_CMD and K_FX changed: bump it
  with the next wire change so mixed builds refuse each other.

### 9. Four players (after 2 is solid)
Everything is for one peer: coop.cpp (g_peer, one reliable channel), coop_host.cpp (g_cmd,
g_client1/g_ent/g_rec, a client block of 2, bodies[2]), coop_laststand.cpp (g_p[2],
State), coop_hud.cpp (names[2]), the lobby's two cards. Plan: a Peer struct array [3], the
client block sized for 4, snapshots sent to each peer, acks per peer, K_PS per peer, each
joiner told of the other joiners' bodies in the snapshot (they are ordinary entities there).

### Rules of the road (from this session)
- Two-copy test: python -I tools/coop/coop_test.py TAG 330 --env COD3_SKIPFILMS=1
  --env COD3_NOFILMS=1 [--host-pad ... --join-pad ... --host-env K=V ...]. ALWAYS both film
  switches. NEVER script A/Start before the mission is up. Warn the user in chat before
  each launch. Mission is up ~150 s on the host's clock; the join starts 40 s later.
- Patch scripts and anything with backslashes: the Write tool, never a bash heredoc.
- Build: bump "Current source version" + a PATCH-NOTES entry, write build-request.txt,
  wait for build-status.txt (about 3 min). Not while a game is running.
- No friend zip, no commit, unless the user asks.

## HANDOFF (2026-11-09 ~23:15)
State: source and install 0.5.874, built OK. Nothing committed (the fork is 0.5.851). No
friend zip (the user has not asked). No game process left running.

### Verified (two copies on this PC, Forest, films off)
- Death rule: down 5 s, back in beside the other player, full health, for the joining player
  and for the host (t870b).
- Stance: each sees the other's crouch and prone (t870b, log and screenshot).
- Effects: the host's world's effects play on the joining copy (t874a).

### Not verified
- Both down at once -> checkpoint for both (code in, never triggered).
- A real death by the enemy (the tests forced the downs).
- Two PCs (effect record addresses, lag, the friend's build).
- The host's own shots and grenade burst as the joiner sees them.

### TO DO, in order
1. Play test between two PCs on 0.5.874, or the both-down two-copy test.
2. Effects not sent: sub_82429238/82429330 (LR 82429318, ~500 a test) and sub_824245D8 (LR
   82424660, f1 a time): hook those callers, find where they place the effect, send that.
3. True inputs on the bodies: firing, reloading, grenade throw, in the air (jump/fall), aim.
   Today DriveBody / DriveHostBody choose 1 of 13 moves from speed and stance only.
   The joiner's buttons are in K_CMD (cmd bytes 4..7); the host's in his record +1352.
   Also the other player's shots: sound and flash at his body on both screens.
4. The joining player's weapons and ammo from the host's client 1 (only health is synced).
5. Triggers by the joining player (client 1 is a real player entity; scripts testing
   level.player only will not fire): test in a level with a door/trigger.
6. Host respawn lands ON the joiner when the joiner has not moved (his trail is empty);
   the host shows crouched, not prone, while down.
7. Cutscene camera for the joiner during the host's scripted scenes.
8. Four players: peers array in coop.cpp, a client block of 4, per-peer K_CMD/K_PS/acks,
   g_p[4], bodies[4], the lobby's cards.
9. Films freeze with two copies loading (3 of 4 tests): ask the user about solo play.

- OLDER NOTE, the tests first planned for 0.5.869 (A, B and D are done; C is still open):
  A. respawn: --host-env COD3_COOP_TESTDOWN=40 (joiner down 40 s into the mission): expect
     "is down ... back in 5 s", "comes back in at", join log "the host makes room: I come in".
  B. host down: COD3_COOP_TESTDOWN0=40: expect "the host comes back in at".
  C. both: TESTDOWN=40 and TESTDOWN0=42: expect "both players down ... back to the
     checkpoint", then the host's restart and the joiner's reload.
  D. crouch: pad B on each side after ~200 s; read "stance: view height" lines.

## 2026-11-10 (Claude, continuing; newest last)
- 10:50 picked up at 0.5.884 (another session's overnight work, see its HANDOFF above).
- THE GRENADE DEATH (rule 1 passed by a real death): t885b reproduced it (host throws "ls"
  at his feet: "MISSION FAILED / YOU WERE KILLED BY A GRENADE", NO "coop: damage:" line after
  G_Damage and NO "coop: die:" line). All blast functions (sub_82565F98, 82566280, 825666A8,
  825669C0, 82567B90) call G_Damage sub_82564888, so the death made inside it leaves the call
  without returning to our hook and not through sub_8255F320 (not proven directly).
  The write watch (COD3_FOVWATCH=second,address) is no use here: armed, the pad script's
  holds stopped working (t885d).
- 0.5.887 FIX: with two players a player takes every G_Damage hit behind a health shield
  (+1000000 for the call); after the call the real loss is measured, health set to what is
  left; nothing left = LastStandTakes (down / both down) and health 1. t887a: five grenades
  at the host's feet, NO mission failed (t885b failed at the second). But no line showed a
  lethal hit absorbed, so 0.5.888 logs every shielded hit ("coop: shield:"); t888a running.
- Pad facts: taps ("ls") do nothing at high fps; use holds ("ls/0.5"). "ls/0.5" = grenade
  throw (buttons 0x2, action 3). With films off the mission is up ~45 s on the host's clock.
- USER REPORTS (11:5x, from play):
  1. A smoke grenade thrown by the JOINING player stays still in the air and never lands:
     a ball of smoke hovering. Likely cause (not checked): its smoke effect FOLLOWS the
     grenade entity; FxFrame plays an effect whose "follows" entity has no puppet "where it
     was made", i.e. at the throw point in the air. Fix idea: keep such an effect waiting
     for its puppet for a moment, and attach; check also that the joiner's own grenade
     (thrown for real by client 1 on the host) gets a puppet on his copy at all.
  2. The T-pose still flickers: the other player flips from his pose to a T-pose for a
     fraction of a second, very often. The body logs already count it ("bare N s so far, M
     restarts"): frames where no animation weighs on the body's tree. Look at DriveBody /
     DriveHostBody / TidyBodyTree: a loop restart or a move change leaves one frame with
     zero weight. Fix idea: never let the total weight be zero (keep the old animation at
     full weight until the new one weighs; restart a loop by wrapping its time, not by
     removing and setting it again).
- 12:15 THE PC SHUT DOWN (heat, the user says) 91 s into two-copy test t888a. 0.5.888 had
  built and installed (12:12); source, notes and the auto-builder were intact after the
  reboot. NO two-copy test since, and none without the user's yes: two copies flat out are
  the likely load. For the next ones use --env COD3_UNLOCKFPS=0 (the console's 30 fps) to
  halve the load. So 0.5.887's grenade-death fix is STILL UNPROVEN (t887a: 5 grenades, no
  mission failed, but no "would have died" line; 0.5.888's "coop: shield:" lines never ran).
- 0.5.889 (queued 12:55, written without a test):
  * T-pose flicker: TidyBodyTree (coop_host.cpp) now makes exactly ONE animation weigh each
    frame (its parent chain whole, every other held node zero). Cause as read in the code:
    at a move change the new animation blended in from 0 over 0.2 s while Tidy forced its
    parents to 1 at once = a full-weight group with nothing in it = the bind pose, at every
    change of move (fire/aim/reload change it constantly). Moves now snap, no blend.
  * Hovering smoke: K_FX gets `wait` (the effect follows a type 3 missile) and `born`;
    FxFrame holds a waiting effect up to 1 s for the missile's puppet, then plays it on the
    puppet. Log: "an effect that follows a missile (a b): its puppet is here / NO puppet".
    Not known yet: whether the smoke is a "follows" effect at all (it could come by the
    by-handle list of 0.5.881), and on whose screen the user saw it (asked, no answer yet).
  * ProtocolVersion 40004 (K_FX is longer).
- TO TEST when the user allows a two-copy run (one run can cover all three):
    coop_test.py t889a 320 --env COD3_SKIPFILMS=1 --env COD3_NOFILMS=1 --env COD3_UNLOCKFPS=0
      --host-pad "236:ry-/3 240:ls/0.5 252:ls/0.5 264:ls/0.5" --join-pad "150:ls/0.5 165:rt/2 175:ls/0.5"
  read: host "coop: shield:" / "would have died" / "is down"; both logs "bare N s so far"
  (should stay near 0) ; join "an effect that follows a missile". The smoke grenade's own
  button is not known (ls = frag): find it, or ask the user to throw one.
- 13:20-13:50 (new session; the user allowed capped two-copy tests: COD3_UNLOCKFPS=0 held both
  at 60 fps or less, no heat trouble in two 320 s runs). The user PLAYS the tests now (one pad
  drives BOTH copies: "my smoke makes him throw" is that, not a bug) and records them
  (Videos/NVIDIA; frame viewer tmp-video).
  * t889a (0.5.889): T-pose gone (user). Still "flicker" = 889's one-frame move changes; smoke
    still hung: "(0052 012C): NO puppet": the effect's FIRST thing (0052) has no puppet.
  * 0.5.890: TidyBodyTree blends (cur + prev add up to 1, parents of both whole, 0.2 s);
    K_FX wait = which of the two is the missile, follows whichever is here; a landing grenade
    (type 3 -> 0) keeps its puppet; ProtocolVersion 40005. t890a: smoke FIXED (user). REAL
    DEATHS PROVEN: would have died / is down / revived (both ways, by the user) / both down =
    checkpoint. Open: the fail screen says "KILLED BY A GRENADE" whatever killed.
  * Still "flickering when moving" (user, t890a): join log "plays move 18 / 20 / 18 / 20":
    the way of moving chosen per frame from a place that comes in steps. 0.5.891: BodyMotion
    (coop.h): 0.3 s window, axis kept unless 1.5x, 0.15 s before a new way is taken.
  * The user on WaW: only a hint (a Treyarch game); what matters is smooth, in sync, revive,
    respawn, checkpoints.
- 13:50-14:20 (same session). 0.5.891 t891a: "the flickering is gone" (user). 0.5.892: a throw
  no longer ends in a rifle-butt swing (weapon state 11 follows a throw too: MeleeState),
  throws/swings/reloads one-shot (OneShotMove, BodyMotion::Act), the bodies' anim changes no
  longer wait for AnimJobRoom, TidyBodyTree touches nothing while the asked anim has no slot.
  t892b PROVED the swing fix. 0.5.893: "invisible then pops back" (user) = the joining copy
  let go of everything not heard for 3 s by the clock, and in PLAYED two-copy tests the
  host's background copy falls to 0-1 fps for a minute (t890a 59 s, t891a 114 s, t892b 89 s
  under 10 fps; unplayed t892a/t893a: 1 s). Now by snapshots that did come (g_lastSnapAt).
  t893a (0.5.893): barely played, both copies 60 fps throughout, no gap: THE FIX DID NOT GET
  EXERCISED. Unknown: why the background copy starves when the user plays the other (asked:
  did he maximise the played window?). So his videos show the other player through a host at
  a few fps: judge animation smoothness on two PCs.
  OPEN next: aim pitch on the body (he does not tilt), the fail screen's "KILLED BY A
  GRENADE", soldier puppets' anim changes still wait for AnimJobRoom (coop_join.cpp ~265),
  then work order items 5 (weapons/ammo), 6 (triggers), 7 (cutscene camera), 9 (4 players).
- 18:30-19:35 (same session; source+install 0.5.896, built OK, nothing committed, no zip).
  * 0.5.894: weapon state 14 is the reload's start (0 -> 14 -> 5), not a throw.
  * 0.5.895: AIM UP/DOWN: stand/crouch _aim_up/_down and _shoot_up/_down twins blended by
    pitch/45 (BodyTilt + TidyBodyTree's tilt share); the host's pitch rides in his body's
    first snapshot angle. Logs of t895a/t896a: "a body aims up/down ... set" on both copies.
  * 0.5.896: THE GUN HE CARRIES: weapon list [0x82A2A2E0] (sub_824650F8), name +8, world
    model +1432; the player's weapon number is his record's +132; the joiner's comes in
    K_CMD flags' top byte. The body is MADE AGAIN with the gun on tag_weapon_right (detach is
    0x825585E8 but an animating body must not change attachments: v45e). t896a: "the host
    carries w_springfield_world ... his body is entity N now", both players, no crash; the
    screenshot shows the other player with the scoped Springfield.
  * TEST SET-UP THAT WORKS: --host-env COD3_NOPAD=1 makes the host copy ignore the real pad:
    the user's pad then drives ONLY the joiner and the host is the script's (no mirroring).
    --env COD3_NOPAD=1 for a hands-off run. Scripted "ls/0.5" = buttons 0x2 = the SMOKE
    grenade here (host's entity "smoke_grenade"); "rs" gave 0x2 too in t896c: not understood.
  * User: "the person joining cannot see the host's smoke". NOT reproduced: t896d (hands
    off, only the host throws): the join's screenshots show his smoke cloud grow, join log
    "puppet for the host's 235 (type 3, smoke_grenade)" + "its puppet is here". Ask when/where.
  * NOT STARTED (the user's list before the friend test): the joining player using the
    world (use-triggers, doors, mounted guns), firing level events (scripts that test the
    host only), and the cutscene camera for the joining player (work order items 6 and 7).
- 19:40-20:15 (same session; source+install 0.5.899, built OK, nothing committed, no zip).
  THE JOINING PLAYER'S TRIGGERS WORK (0.5.899, hands-off test t899a: he walks the first
  path alone: 7 trigger_multiples "told to the scripts as the host's player's", ten lines of
  speech; before (t897b/c): two). How: the touch queue 0x82A4E790 (+3196 entries of 16
  bytes: trigger handle, toucher handle, each one's +204; count +7292), written by
  sub_825471D8(trigger, toucher), told to the scripts by sub_825642A0 each server frame
  (twins of MP's 82567328 / 82C281D0 / 8256FFC8, found with scratch twin.py: functions by
  shape). In sub_825642A0's hook (coop_host.cpp) a touch whose toucher is client 1 becomes
  the host's player's. Handles: entity +500 = index | generation << 14 (FromRef/RefOf).
  A trigger's box: +272 least corner, +288 greatest. Touch function table at 825F3848..
  (825661A8 = trigger_multiple: wants the toucher's takedamage +624).
  The 0.5.898 probe (coop: probe: queued / trigger N ...) is STILL IN: take it out.
  * NOT DONE of the user's list: (a) the joining player's USE on doors, charges, mounted
    guns (does a use go through the same queue? the probe's "queued: ... by the joining
    player" with a trigger's +764 = 18 may be one; test with a trigger_use level);
    (b) the cutscene camera for the joining player (K_PS needs the host's view angles and
    eye; Forest has no scene after the intro: pick a level with one; do not break the
    intro's own sequence: coop_join.cpp ~932 and ~1011); (c) aim tilt while moving;
    (d) what a trigger does "to the one who touched" goes to the host (hurt triggers).
  * Scratch tools (session scratchpad, not in the repo): twin.py (MP function -> SP twin by
    masked instruction shape), callers.py (callers of a function with the code before the
    call), wdef.py. Worth moving into tools/mpmatch if used again.
- 20:15-20:50 HANDOFF (session limit). Source 0.5.900 (a PROBE build, queued 20:48: check
  build-status.txt; install is 0.5.899 until it builds). Nothing committed, no zip.
  * USE / "the mission does not hear the joining player". User played the joiner in
    Saint-Lo's training (t899u, --map saint_lo; the intro takes ~170 s before he is in): the
    prompt showed and he picked up the gun and shot helmets ON HIS OWN SCREEN, but the
    grenade step "did not register". Host log: no trigger_use and no trigger_damage queued
    by him, objectives did not move. So his own copy shows the prompt/pickup locally; the
    host's scripts wait for the HOST's player. Saint-Lo: trigger_use 130 at 8170 9372 261
    (box +272 8140 9343 240 .. +288 8191 9404 370), 5 trigger_damage, 9 trigger_lookat.
  * Lead being probed: events go to an entity's script object (entity +516) through
    sub_8251BE50(object, entity, name, args): 7 (name, handler) pairs from object +8, parent
    at +64; sub_820A0840(entity, name, &arg) is one sender. 0.5.900 logs "coop: probe: event
    NAME for the host's player / the joining player: the host's scripts HANDLE / do not".
    NEXT: run Forest scripted (both fire, throw "ls/0.5", reload) and Saint-Lo played, read
    those lines, then mirror the joining player's events that the host's player's object
    handles onto the host's player (NOT death/damage: find their names first).
  * Also needed for this (work order item 5): client 1's inventory on the host is a copy of
    the host's record at join time; the joiner's own copy has its own. His throws/shots on
    the host use the HOST-side inventory. Sync host -> joiner is not done.
  * Take out the probes when done: TouchProbe/TriggerProbe (0.5.898), the event probe
    (0.5.900), the weapon-number probe (0.5.895).
- 2026-11-10 20:55-22:25 (new session, Fable; 0.5.901-906 built, nothing committed, no zip). The whole account
  is the '2026-11-10 ~22:25' note at the top of docs/PICK-ME-UP.md and the entries 0.5.901..0.5.906 of
  docs/PATCH-NOTES.md. In short:
  * The 0.5.900 event probe (sub_8251BE50) was a dead end and is out: 4 events a run, none from a shot/throw.
  * Cause of 'the mission does not hear the joining player': client 1 on the host owned no weapon. Fixed by
    coop_inventory.cpp (R_INV / R_INVMINE, ownership both ways, counts the host's after 0.6 s).
  * 0x4000 in the record's buttons is crouch, not a throw (ThrowButtons = 0x2).
  * Coop::DeleteEntity (sub_824A7280) is the scripts' UNLINK. Bodies replaced at a change of gun stayed for
    ever; now put aside (hidden, not sent) and reused by gun (g_asideBodies, spare[who][model]).
  * Heat: tools/coop/guarded_test.py + COD3_SCALE=0.6 for every two-copy run (GPU under 60 C, 60 fps both).
  * Tests: t900a t901a (Forest), t901u t902u t903u (Saint-Lo, the user as the joiner), t904a t905a (heat),
    t906a (Forest, the fix proven), t906b (Saint-Lo hands off, clean). Scratch tools in the session scratchpad:
    bits.py/idx.py (record offsets from the code), dis.py (a function's instructions), guarded_test.py (kept in
    tools/coop).
  * Open, in order: the user's look at 0.5.906 as the joiner in Saint-Lo (guns, crouch, frags at the target);
    then Use (doors, charges, mounted guns), the cutscene camera, the probes out, the friend zip.
