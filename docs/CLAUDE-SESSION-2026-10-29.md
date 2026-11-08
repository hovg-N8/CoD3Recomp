# Claude session 2026-10-27 to 2026-10-29 (CTF bots, merges, fill-screen outline)

Source/install at the end: 0.5.608 (queued 00:20 on 10-29 with a 5-minute 8-bot test; check
build-status.txt and bench-ctf-rouen-small.log). PATCH-NOTES 0.5.541-0.5.608 have the evidence for
every change. A second Claude session ("Spectator mode", local_56d0ca1b...) worked on the spectator cam,
mouse menus, campaign sprint and the team-select Spectator seat (its notes are in PATCH-NOTES too).

## Where CTF stands (Rouen)
- Works: sides and spawns (own flag), no friendly fire, attackers leave base, regroup, push, go in by
  the enemy base's recorded "ways out" (saved per map in bots\maps\rouen-ctf-ways.txt), shoot on the
  run, take the flag; carriers leave by a way out, go home along their own trail, enter their own
  base by its ways out; escorts form round the carrier; a standoff (both flags out) sends attackers
  after our flag; defenders hold cover; hunters chase the enemy carrier. Real captures happen
  ("flagwatch: flag N CAPTURED by slot S"), e.g. 591 (2), 604-607 8-bot tests (1-2 per 5-10 min).
- Weak: carriers still need the stuck-carrier help (teleport forward after 10 s with no progress)
  2-4 times a carry. Causes found and fixed one by one (circling base exits 607, route start behind
  him 607, slopes 591/594, raised edges 608). Bots "stall while moving" ~8-11% of samples
  (tools/ctfreport/stalls.py), spread over many small places.
- Tests: BENCH-CTF-ROUEN-LONG.bat (24 bots, 10 min), BENCH-CTF-ROUEN-SMALL.bat (8 bots, 5 min, the
  user's preference; also launches the enemy UAV plane 30 s in, COD3_UAVTEST_AT), BENCH-CTF-CARRY.bat.
  Summaries: tools/ctfreport/ctfreport.py, tools/ctfreport/stalls.py; carriers: "carrytrace:" lines
  every 0.5 s (606).
- The user: wrap the bot work up and move on to other maps if the logic is good enough. It is (all
  roles act); the map knowledge and ways out are learnt per map as it is played.

## 0.5.608 result (built 00:16, 8-bot 5-minute test, log bench-ctf-rouen-small-608.log)
No crash; 2 bot takes, 0 captures, 1 return; stalls 9.3%; 22 throws (one a side at a time);
37 "tries a cleared low jump" (the raised-edge hop works). UAV plane: flew all 64 samples with
+16 = 00000000 (inside the world) and no "under the sky" change on Rouen: the disappearing could
not be reproduced here (Rouen's sky is high). Next: ask the user which map it vanished on; check
its "+16" there, and the plane's draw distance / LOD if +16 stays 0.

## Open items for the next session (in order)
1. Read 0.5.608's 8-bot test: throws one a side at a time ("throws a grenade"/"throws smoke" lines),
   "tries a cleared low jump" for carriers, UAV "under the sky" lines and the plane's "+16" word
   (0x40000000 = outside the world, the disappearing plane) in "uav plane: side N phase" lines.
2. Fill-screen outline (the user's top graphics bug): with F11 Fill the screen ON, in smoke a ghost
   of the gun's outline / a smoke-free band beside poles and walls, a few pixels to the side
   (screenshots/CoD3-20261028-233034.png, -234240.png; crops in the session scratchpad). Turning
   Fill the screen OFF removes it (user confirmed). Effects resolution full (2x) made it smaller.
   bench-fill-frame.log (BENCH-FILL-FRAME.bat, a logged frame indoors, no smoke): every surface,
   resolve and crop is widened consistently (frame 1040x624 guest -> 2070x1080 host, widen 1.150;
   half-size smoke/soft particles 520x312 at pitch 560; the composite ps 2a892b1433c357fe reads the
   half smoke 0C750000 and the full depth 04350000). Not found statically. NEXT: the user runs
   PLAY-FRAMELOG.bat, turns Fill the screen on, stands in smoke, presses F12 once: framedump\d-NNNN.bmp
   pictures every draw + framelog-play.log; find the draw where the gun's silhouette shifts.
   The user's ini has fill_screen = 0 (they turned it off) and effects_resolution = 2.
3. Campaign: the user tested 0.5.599+ (freeze fixed by the spectator session). Mouse menu clicks and
   the cursor toggle (0.5.600) and sprint (0.5.601) not confirmed in play.
4. Friend zip: on hold by the user ("send it later"); 0.5.600/0.5.601 zips exist in the folder above.
5. Intermittent LOAD crash before any bot runs (583 GPU hang at the menu; 596 memset in
   sub_82137C38 from sub_82142248), 2 in ~25 runs; reruns fine. Log in the session scratchpad.
   Also seen by the spectator session: bench-spectate-seat.log (0.5.612) "watchdog: no swap for five
   seconds, after 9" at 54 s during map load, and 0.5.611 one at 1 s (logs in the install's
   logs-backup\). Likely the same load-time stall; look at the stacks there.
6. [MOSTLY DONE 0.5.628-0.5.629, see CLAUDE-SESSION-2026-10-30.md: bounce fixed and the user liked it;
   explosions still missing, as they are not in the replayed objects.]
   Killcam (user, 2026-10-29, to the spectator session): "i dont see explosions in the bots killcams
   also the killcams could be a little better they still are a little bouncy". Not changed yet;
   the spectator session left bots.cpp alone. Evidence so far is from READING the code only. There
   is no log or line for it yet, and no killcam was recorded or logged for this.
   a) Missing explosions, likely cause: bots.cpp Killcam::SwapInObjects, line ~2690:
      `if (it == g_objDone.end()) it = g_objDone.emplace(e, Guest::Read32(base, e + 184)).first;`
      runs after `memcpy(base + e, rec, 484)`. So an object's "events played" (cgame's +1000) starts
      at its own recorded sequence (+184) the first frame the replay sees it. An object born
      mid-replay already carrying its event (a temp event entity: a grenade or rocket explosion, an
      impact) counts that one event as played, and cgame (CG_CheckEvents 8246C208, from 8246C2E0)
      never plays it. A grenade's own later events would still play, because it existed before them.
      Proposed change: remember which objects were listed on the replay's first frame (g_objDone is
      cleared at line ~3152 when a killcam starts). Those keep today's init. An object first seen on
      a later frame starts at seq - 1, so its last event plays. First confirm, in 8246C208, whether
      event-type entities (eType past the event threshold) use +1000 as a "played" flag instead of a
      sequence; if so, start them at 0 instead.
      To confirm before changing: log once per killcam, for each object first seen after frame 1, its
      type (entity +4), +184 and the +1000 written. Then expect explosion-type objects with "played"
      == sequence.
      Test: COD3_BOTS bench or a play session; die to a bot's grenade or rocket and watch the
      killcam. The killcam end-line counters (line ~3196, with g_objDone.size()) should still print,
      with no "killcam guard" hits in mplog.
   b) Bounce: COD3_KILLCAM_BLEND is off by default since 0.5.346 (Killcam::Around, line ~2499: exact
      sample, no blend). Samples are a server frame (15 ms) apart. At 60-100+ fps each frame takes
      the sample at or before t, so the killer's eye steps and holds.
      Proposed change: blend only the followed killer's eye (ps origin and view angles, with
      LerpAngle for angles) between samples a and b, while objects, poses and the other players stay
      exact (that is why 346 turned 329's blend off). Alternatively, smooth the replay clock t.
      Test: COD3_KILLCAM_BLEND=1 first shows whether blending alone removes the bounce. Then a
      camera-only blend, judged in play and in a recorded video of a few killcams.

## Lessons
- printf "\n" written through a bash heredoc + python becomes a real newline: use the Edit tool or
  chr(92)+'n' (check with grep -n 'printf("[^"]*$').
- Count captures only as "flag home straight from carried" (a render line "captured programs" was
  once counted as captures).
- The game logs weapon events only for the host's own player.
- Builder reads "Current source version" when it FINISHES: don't bump it mid-build.
- Copy the play logs aside before any test run (logs rotate); don't build while CoD3MP.exe runs.
