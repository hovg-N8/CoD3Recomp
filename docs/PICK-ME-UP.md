**2026-11-13 (Claude): 0.6.4 EVERYWHERE, no game code changed. Versions roll over at 999 (0.5.1000-1004 renamed 0.6.0-0.6.4; next build 0.6.5). Exes rebuilt to say 0.6.4; friend zip CoD3MP-update-0.6.4.zip; launcher feed published 0.6.4; GitHub fork main ee2d32e (source + launcher/ + README) and release v0.6.4. Read docs/CLAUDE-SESSION-2026-11-13.md (launcher publish steps, the key never leaves AppData). Game work open: as below.**

**2026-11-12 afternoon (Claude): 0.6.4 built. Joiner lands with the host after Saint-Lo's wall (0.5.991/0.5.993, t993a); live-grenade pick-up works for the host (t993a); the joiner's went to the host (t999a); held back since 0.6.4 (t1004a). Nothing committed, no zip.**

**2026-11-12 morning (Claude): 0.5.990 built (a sprinting host no longer starts a false ride for the joiner, t990b). The joiner's cutscene camera is closed: Saint-Lo's hop is the host's own moved view (t990a). Nothing committed, no zip.**

**2026-11-12 overnight (Claude): 0.5.971-0.5.989 built, nothing committed, no zip. Saint-Lo's opening now runs hands-off by script (tools/coop/stlo_run.sh), two copies too; the "scripted actions are local-only" claim was wrong; action probe on in co-op; cutscene camera open. Read docs/CLAUDE-SESSION-2026-11-12.md.**

<!-- claude-2026-11-11-day -->
**DAY 2026-11-11 ~12:10 (Claude; the user at work, said: keep going until co-op is in perfect order; HEAVY FOCUS on the two players' animations seen both ways; also grenades, smokes, prompts, turrets, vehicles, missions, cutscenes). Start: 0.5.935 built (the headless sideways body FIXED: bodies were taken onto the joining copy's own entities of the same number). Progress lines below.**
- 14:15 0.5.942 built. DONE today (hands-off, both windows side by side in desktop shots, tools/coop/coop_test.py --desk-shots): (1) animation audit both ways in daylight (t936b): fire, reload, crouch, prone, melee, gun switch, aim, walk all right on the other copy; no jump move yet. (2) 0.5.936 no bind pose at a gun change (new body hidden till animated, old kept till then). (3) 0.5.936 test windows really side by side (settings resolution re-centred them). (4) 0.5.937 the joiner's smoke seen on his own screen (only frags are "his own"). (5) 0.5.941 FIX: the joiner's view on the host = his copy's view (client 1 delta angles +88..+96 set from the command +1360..; he faced yaw 109.6 on the host while looking at -45: hints and shots went the wrong way). (6) 0.5.942 the joiner mounts stationary MGs on both copies (his copy keeps its own misc_mg42 and draws the host's with it), fires it (host impacts), dismounts. OPEN: his body on the host has no gunner pose on the MG; jump move; vehicles; cutscenes.
- 14:35 0.5.945: the other player on a mounted gun is drawn aiming / shooting (on a gun = record +1148, the gun's handle; +1144 = 1). The game has no jump or gunner animation for soldiers (anim.cod names): the standing pose is the game's own.
- 15:15 END OF DAY RUN: source+install 0.5.947 built OK; nothing committed; no zip. Full list: docs/CLAUDE-SESSION-2026-11-11.md, section "Day session". Next: the user plays the joiner on 0.5.947 (MG, grenades, smokes, shooting, the bodies from both sides).
- 17:05 0.5.948 built OK (nothing committed, no zip). The user's played tests: t947u had NO input reaching the joiner (coop_test.py set COD3_BACKGROUND=1 = keyboard/mouse ignored; no pad connected): new `coop_test.py --played` (joiner keys on, host COD3_NOPAD). User report: after taking the Garand the joiner held no gun on his screen (held weapon 0: the script switches only the host's player). FIXED 0.5.948: the joining copy taps the weapon cycle (Y) when a gun is given while it holds none; t948a hands-off: "I hold weapon 2 now", the host sees him carry w_m1_WORLD, 0 s under 20 fps. The host's player still gets the Garand too (level script gives level.player).
- 18:20 0.5.952 built OK (nothing committed, no zip). 0.5.950: user saw the HOST's gun flash at the joiner's shots: the local slot ([0x82A2AB40]+4) is now his only inside the fire call sub_82569C40 (hook), not his whole think (COD3_COOP_THINKLOCAL=1 = old); kills still work (t950a..t952a). Flash itself not checkable in screenshots: needs the user's eyes. 0.5.949 "coop: item:" probe (weapon_* type 2). 0.5.951/952 PICKUPS OFF THE GROUND by the joiner: the game's swap runs on the host for him; his copy now reports only its own gains (no duplicate old gun) and presses the weapon cycle when its hands go empty while owning a gun: t952a holds the Gewehr 43 on both copies 1 s after. Recipe: Forest, pad walk+fire 60..160, TESTAIM 60-165, TESTGO 170:901,-5878,366,40,362, pad 174:x/2. Noted: sub_8254AAC8 (shot start) uses the LOCAL camera when client +208 bit 0x00100000 is set: look at it for vehicles/guns.
- 19:05 0.5.955 built OK (nothing committed, no zip). User's played Forest run t952u: "swap for null" and "just the ammo". FIXED: 0.5.953 the joining copy registers a gun the host's prompt names (client weapon info 0x82A9A460+196*w, +140 name; sub_82448C78 registers; prompt builder sub_824ED5A8, hint 12..139 = gun w=h-11, 140..267 = ammo w=h-139): t954a reads "swap Springfield for Gewehr 43". 0.5.955: the host's own player holding Use swaps once (t954b); the joiner swapped back every 2 s: now his Use (cmd button 0x40) is held back after a swap until released: t955a one swap, Gewehr in his hands on both. Test aid 0.5.954 COD3_COOP_TESTGUN="t:d" (host must have it set too, writes coop-testgun.txt). Muzzle flash fix (0.5.950) still needs the user's eyes.
- 19:35 0.5.958 built OK (nothing committed, no zip). User (t955u): the host's gun flashed at the joiner's shots ONLY when both held the same gun (sniper+sniper yes, Garand vs sniper no). Cause: sub_8242E5F0 puts a flash on the client's FIRST-PERSON model of the shooter's weapon when the shooter is the local slot (his fire runs as local since 0.5.950). FIX 0.5.956: hook, host player back in the slot for that call and his flash at his body's gun (logged in t957a; user to confirm by eye). Also sub_8254AAC8 scoped shot start = local camera: off for him (COD3_COOP_SCOPEFIX). User: swap "felt slow": 0.5.957/958 a swap in the host's word drops the old gun at once and presses the cycle at once toward the FIRST gained gun (a swap also gives an extra bit, 81 for the Gewehr): t958a Gewehr in hands the same second.
- 21:00 0.5.960 built OK (nothing committed, no zip). Struggles: the engine's scripted-action/scene system (names 0x825D1168.., scene list 0x825F51A0); a scene's PLAYER is an argument: sub_824F1E20 adds participants, the script methods sub_82535A40 / sub_82535B78 (built-in entries 0x820960A0/A8) put the player in. Saint-Lo struggle at 1160 -1414 (user-played t959u, after training + truck + wall hop: not reachable hands-off). Plan + details: docs/CLAUDE-SESSION-2026-11-11.md, end. Also fixed: coming back in beside a player whose trail was a ride (fell out of the map). Open: joiner's seat in the truck (bounces).
- 21:40 0.5.964 built OK (nothing committed, no zip). Rides: the joiner's side in a ride is now the vehicle's travel side, not the host's view (0.5.961); riding also when the host moves at vehicle speed (Falaise's jeep gun, 0.5.962: t962a he followed the route); off a ride he is put where the host stands, a driven ride ends when the host steps 64 from the stop (0.5.963); his own copy's intro vehicle deleted at the freeze with him on it -> put at the host (0.5.964). 0.5.963/964 NOT verified: hands-off players die in Falaise's opening; needs a played run. Struggle plan corrected: a scripted action only runs for the machine's own player (see the session doc).
- 22:40 0.5.970 built OK (nothing committed, no zip). HOST BODY FLICKER (user): each host gun change gives him another body; the joining copy drew the new one at pose weight 0 (T-pose) on its first frame (t969a: 3 of 12). FIXED 0.5.970: held hidden till its pose weighs (t970a: 11 of 13 swaps caught, ~1 frame hidden, no T-pose). The 0.5.968 probe found no other short-of-pose frame. Test aids: COD3_COOP_TESTGOD=1 (host), COD3_COOP_PREMAP, COD3_COOP_FORCEDVAR="skipto=2". GPU reaches 74-79 C in Forest runs now (idle ~59 with fans off): keep runs short, scale 0.5.
<!-- /claude-2026-11-11-day -->

<!-- claude-2026-11-11-overnight -->
**OVERNIGHT 2026-11-10 23:00 to 2026-11-11 03:00 (Claude; the user asleep, handed over "get co-op truly working"): source+install 0.5.933 built OK; nothing committed (fork still 0.5.851); no friend zip. READ docs/CLAUDE-SESSION-2026-11-11.md (what is proven, the engine facts, test aids, OPEN list).** In short, all proven in hands-off two-copy runs (0 s under 20 fps in every run, GPU at most 75 C): the joining player's Use, hints and pick-ups run on the host (he took Saint-Lo's Garand himself, objectives moved); his prompts on his screen are the host's world's; HIS SHOTS AND GRENADES ARE FIRED IN THE HOST'S WORLD (never before) and he KILLED a German there (t926a); his own grenade shows once on his screen; the host's grenades and blasts reach him; no friendly fire between the two; his fire does not move the host's view. FIXED my own regression 0.5.920-0.5.925 (every soldier hidden on the joining copy). OPEN: one-person scripted events (tackle) go to the host's player, mid-mission cutscene camera (no case found hands-off), firing a mounted gun, the taken Garand still drawn on his barrel (cosmetic), the training's tutorial steps are the host's pad by design. NEXT: the user plays the joiner on 0.5.933.
<!-- /claude-2026-11-11-overnight -->

<!-- claude-2026-11-10-late -->
**UPDATE 2026-11-10 ~22:25 (Claude; the user asleep, said: keep working): source+install 0.5.906 built OK; nothing committed (fork still 0.5.851); no friend zip. CO-OP tonight (0.5.901-906), all in docs/PATCH-NOTES.md:
(1) THE JOINING PLAYER IS ARMED ON THE HOST: the host's client 1 owned no weapon (record +132 = 0), so his shots and grenades were on his own screen only. New CoD3Host/coop_inventory.cpp: client 1 gets the host's player's set and every later grant; what the joining copy owns (its start, a pickup) goes to the host (R_INVMINE) and back (R_INV); ProtocolVersion 40007. Record layout: reserve +288+4*(weapon+1460), clip +656+4*(weapon+1468), owned bits +1024 (5 words), bolt bits +1044. Proven: t901a (Forest), t902u (Saint-Lo, played).
(2) Button 0x4000 is CROUCH (0x2000 prone), not the smoke: every crouch was shown as a throw. Proven t903u.
(3) THE SCRIPTS' "DELETE" (sub_824A7280, Coop::DeleteEntity) IS UNLINK: a body replaced at a change of gun was never deleted: frozen soldiers on the host, and the joining player's own old body on his copy (white, headless, on its side, round his eye: the user's clips). 0.5.906: a body put aside is hidden, never sent, and kept for its gun (one body a gun). Proven hands off in t906a (Forest: both players swap between two bodies; screenshots clean) and t906b (Saint-Lo, no change of gun possible hands off). NOT yet seen by the user.
(4) HEAT: two copies took the GPU from 64 to 84 C in 20 s and the runs t902u/t903u/t904a ran under 10 fps (that was 'flashing in and out'). ALWAYS run two-copy tests as `python -I tools/coop/guarded_test.py TAG SECONDS ... --env COD3_SCALE=0.6` (stops the game at 84 C): with the scale the GPU stays under 60 C and both copies hold 60 fps.
STILL OPEN: Saint-Lo's grenade step (does the mission hear his frag now? needs the user to throw at the target as the joiner: no trigger_damage was queued by him in t902u/t903u, unknown if he threw); the joining player's Use on doors/charges/mounted guns; the cutscene camera; a weapon the host's player swaps away stays with the joining player; the host's T-pose seen from the joiner (not seen in any screenshot: the host unarmed in Saint-Lo stands with his arms down). TO TAKE OUT: NearProbe (0.5.904, coop_inventory.cpp), the 0.5.895 weapon-number probe and the 0.5.898 touch probes (coop_host.cpp). The real entity delete is not found (G_Spawn 0x82543568). Next test for the user: Saint-Lo as the joiner with --host-env COD3_NOPAD=1, switch guns a few times, crouch, throw frags at the training target.**
<!-- /claude-2026-11-10-late -->

<!-- claude-2026-11-10-night -->
**HANDOFF 2026-11-10 ~20:50 (Claude, session limit): source 0.5.900 = a PROBE build queued at 20:48 (check build-status.txt; install 0.5.899 until built); nothing committed; no friend zip. Working on: the host's mission does not hear what the JOINING player does (Saint-Lo training: his grenades 'did not register'; pickup and shots only on his own screen). Walk-in triggers are fixed (0.5.899). Read the LAST bullet block (20:15-20:50 HANDOFF) of docs/CLAUDE-SESSION-2026-11-09.md: it has the lead (script events via sub_8251BE50), the next test, and what to remove.**
<!-- /claude-2026-11-10-night -->

<!-- claude-2026-11-10-evening -->
**UPDATE 2026-11-10 ~20:15 (Claude): source+install 0.5.899 built OK; nothing committed (fork still 0.5.851); no friend zip. CO-OP since the afternoon note: aim up/down on the other player's body (0.5.895), the gun he really carries (0.5.896, the body is made again at a change of gun), an empty clip's reload no longer shown as a throw (0.5.894), and THE JOINING PLAYER'S TRIGGERS fire the level's events (0.5.899, proven hands-off in t899a). Tests: --host-env COD3_NOPAD=1 = the user's pad drives only the joiner; --env COD3_NOPAD=1 = hands off. OPEN before the friend test (the user's list): the joining player's Use on doors/charges/mounted guns, the cutscene camera for him; then the friend zip. Take the 0.5.898 trigger probe out. Read the LAST bullet block of docs/CLAUDE-SESSION-2026-11-09.md (19:40-20:15).**
<!-- /claude-2026-11-10-evening -->

<!-- claude-2026-11-10-afternoon -->
**UPDATE 2026-11-10 ~14:20 (Claude): source+install 0.5.893 built OK; nothing committed (fork still 0.5.851); no friend zip. CO-OP: the user allows CAPPED two-copy tests (COD3_UNLOCKFPS=0) and plays and records them himself (one pad drives both copies). User-confirmed: T-pose gone, smoke fixed, move flicker gone (0.5.891). Proven in logs: real deaths go through down/revive/both-down=checkpoint (t890a), a throw no longer ends in a melee swing (t892b). NOT yet exercised: 0.5.893's 'nothing let go on a snapshot gap' (the vanish-and-pop). In played tests the host's background copy falls to 0-1 fps (cause unknown), so the user's videos understate smoothness: two PCs are the real test. Read the last two bullets at the END of docs/CLAUDE-SESSION-2026-11-09.md (13:20-14:20) for the open list.**
<!-- /claude-2026-11-10-afternoon -->

<!-- claude-2026-11-10-handoff -->
**HANDOFF 2026-11-10 ~13:00 (Claude): source+install 0.5.889 built OK (12:48); NOTHING committed (fork still 0.5.851); no friend zip. CO-OP CAMPAIGN (CoD3.exe). THREE FIXES IN, NONE TESTED: (1) 0.5.887 a real death (the grenade that failed the mission past the down/revive rule): with two players a player takes every G_Damage hit behind a health shield and a lethal one goes to the rule; (2) 0.5.889 the T-pose's flicker: one animation weighs each frame, moves snap (TidyBodyTree); (3) 0.5.889 the smoke grenade that stood in the air: an effect that follows a missile waits for the missile's puppet (K_FX wait, ProtocolVersion 40004). THE USER'S PC SHUT DOWN FROM HEAT during a two-copy test (t888a, 91 s in): the user said NO TESTS FOR NOW. Do not launch the game without asking; when allowed, add --env COD3_UNLOCKFPS=0 (30 fps) to coop_test.py. Read the section '2026-11-10 (Claude, continuing' at the END of docs/CLAUDE-SESSION-2026-11-09.md: it has the one test line that covers all three fixes and what to read in the logs. Waiting on the user: either a capped two-copy test, or their own play of 0.5.889 with the friend (both PCs need it) and the logs after. Asked, unanswered: on whose screen the hovering smoke was seen.**
<!-- /claude-2026-11-10-handoff -->

<!-- claude-2026-11-09-handoff -->
**HANDOFF 2026-11-09 ~23:15 (Claude; source+install 0.5.874 built OK; NOTHING committed (fork still 0.5.851); no friend zip. CO-OP CAMPAIGN (CoD3.exe) work, two players): read docs/CLAUDE-SESSION-2026-11-09.md FIRST, its section "WORK ORDER FOR THE NEXT AGENT" (nine items in order with file/line pointers; item 1 is the user's last request: down 10 s and revivable; no friend zip yet).** Verified on two copies on this PC (Forest): the user's death rule (down 5 s, back in beside the other player, both players; 0.5.869), each player sees the other's crouch and prone (0.5.869), the host's world's effects (muzzle smoke, impacts, explosions) play on the joining copy (0.5.872-874: 461 played, 24 refused in t874a). NOT verified: both down = checkpoint for both, real deaths, two PCs, the grenade's own explosion seen up close. Open, in order: effects made by sub_82429238/sub_824245D8 (about 500 a test not sent: placed after they are made), the other player's fire/reload/throw/jump on his body (true inputs, not guessed moves), the joining player's weapons and ammo from the host, triggers fired by the joining player, cutscene camera, then 4 players (everything is hard-coded for 2). Tests: tools/coop/coop_test.py with --env COD3_SKIPFILMS=1 --env COD3_NOFILMS=1 ALWAYS (the game freezes in videos with two copies; never script A before the mission). The JARC menu patch's DividerFilter.h was made campaign-only (it stopped every build).
<!-- /claude-2026-11-09-handoff -->
**LATEST 2026-11-09 late (Claude): source+install 0.5.875 built OK; NOTHING committed (fork still 0.5.851); no friend zip. Work order item 1 DONE and tested (t875a): down 10 s, revive by holding Use (pad X) beside the downed player for 3 s, not revived = back in beside the other, ProtocolVersion 40003. NEXT: item 2 of the WORK ORDER in docs/CLAUDE-SESSION-2026-11-09.md (the effects not sent yet), then 3..9 in order. See its section 'PROGRESS AFTER THE WORK ORDER'. UPDATE 0.5.876 (built 22:56, test t876a PASSED): work order item 2 mostly DONE: effects from sub_82429238/82429330 are sent (their matrices were whole; only the rows' 4th numbers were stack leftovers, now sent as 0 0 0 1): join played 1161, refused 0 (t874a: 461 / 24). Left of item 2: sub_824245D8's by-handle effects (28 in the test, all from caller 820FBD70) and effects that follow a player. USER REPORT 23:05: 'still seeing the other player in a tpose position as the idle stance' (not seen in t876a's joiner screenshot: the host's body stands in its idle there; which screen he saw it on was asked). Source+install are 0.5.876. UPDATE 23:30: source+install are 0.5.878. THE OPEN PROBLEM IS THE OTHER PLAYER'S T-POSE (user report, reproduced on both screens in t877a and still there in t878a): 0.5.877 (idle restarted a frame early) and 0.5.878 (moves set with sub_82500B80 so the parent nodes weigh) did NOT cure it. Read 'T-POSE: WHAT IS KNOWN' in docs/CLAUDE-SESSION-2026-11-09.md before anything else. UPDATE 23:55: T-POSE FIXED in 0.5.880 (source+install; test t880a: both screens show the other player in his idle after walking, strafing and prone). Cause: parent nodes of the body's animation tree left weighing with no animation under them (TidyBodyTree, coop_host.cpp). Nothing committed, no zip. NEXT: the rest of work order item 2 (by-handle effects from caller 820FBD70, effects that follow a player), then item 3 (true inputs on the body), and the small things listed under 'AFTER THE T-POSE' in the session file. UPDATE 2026-11-10 00:05: source+install 0.5.881 (test t881a PASSED): the by-handle effects are sent too (the waiting list at 8290B5D0 emptied by sub_820FBD00): join played 199, refused 0. WORK ORDER ITEM 2 IS DONE except effects that follow a player (left to item 3). NEXT: item 3 (the other player's fire, reload, throw, jump on his body), then 4..9. Nothing committed, no zip. HANDOFF 2026-11-10 00:45 (session limit): source+install 0.5.884, built OK, test t884a run. Item 3 PART DONE: the other player's fire, grenade throw, melee, reload and aim show on his body from his true state (both directions heard and played). FIRST THING TOMORROW, A BUG IN THE DEATH RULE: in t884a the host was 'KILLED BY A GRENADE' and the mission FAILED straight to the checkpoint with NO last stand (no 'is down' line): a real grenade death goes round the down/revive rule. Read 'HANDOFF 2026-11-10' in docs/CLAUDE-SESSION-2026-11-09.md. Nothing committed, no zip.**


<!-- claude-2026-11-08-handoff -->
**HANDOFF 2026-11-08 ~03:30 (Claude, overnight, user asleep; source+install 0.5.865 built OK; NOTHING committed (fork still 0.5.851); no friend zip since 0.5.851): read the LATEST section of docs/CLAUDE-SESSION-2026-11-08.md FIRST.** User confirmed: Seelow falls and hill/tracks fixed; tank MG with a human gunner works. Done: friend's kill feed/death sync for bots (needs the friend), War captures on Carentan/Seelow (tools/carentan/triggers.py), Seelow backstop slabs (coll_s) and rails, Mosin scope, bot log spam capped, bots hold fire for mates in line (grenades as before), random extra gun for bots (ours too), bots fire tanks, bots' tanks finally MOVE (move-lock in the vehicle think) but crawl ~5 u/s. Clean-up: ~15.6 GB to the Recycle Bin (docs/CLEANUP-2026-11-08.md). Open: tank speed (user drives once for a baseline), friend tests, flare HUD (not reproduced: ask where/when), commit/push, friend zip.
<!-- claude-2026-11-08-handoff -->

<!-- claude-2026-11-07-handoff -->
**HANDOFF 2026-11-07 ~22:00 (Claude; source+install 0.5.851 built OK; PUSHED to GitHub: fork main b6f76d5, release v0.5.851 published; full friend zip CoD3MP-update-0.5.851.zip made): read the LATEST section at the top of docs/CLAUDE-SESSION-2026-11-07.md FIRST.** User confirmed in play: Seelow tanks + weapons, Seelow Night arms (perfect, fog stays), dual Colt flashes lined up. New: tank kills go to the driver (0.5.834); bots get into tanks by themselves, Shermans too (steering no longer fights the turn, corners, learnt-map route; 0.5.841-851). Commits now go through D:\Call of Duty 3 (USA, Europe)\CoD3Recomp-fork (a git clone; the working source has no .git) and GitHub Desktop pushes them. Left: bots' tank give-ups and driving pathing, third-person dual Colts unseen, Kangaroo treads on Merville unchecked, tank MG with a human gunner untested, trim diagnostics.
<!-- /claude-2026-11-07-handoff -->

<!-- claude-2026-11-06-handoff -->
**HANDOFF 2026-11-06 ~02:40 (Claude, overnight, user asleep; source+install 0.5.810 built OK; nothing committed; friend zip on hold): read docs/CLAUDE-SESSION-2026-11-06.md FIRST.** Tanks on Seelow: bots fire the cannon (move-lock byte, 0.5.798), its own events from mp_merville.fx (looked up in the vehicles' pack, 0.5.797): flash, fire, reload, hits by surface (seen on screen). Tank MG: its own fire and flash per round (0.5.808, MG50cal_FIRE_3RD heard in the log). Mortar: its hit is the game's own mortar_81mm_turret shell (Laison's projexplode_81mm_<surface>, damage, seen on screen); its loading/fire/incoming sounds were missing on Seelow because the campaign banks were opened on dlc5: (0.5.805: D:). New: COLT .45 DUAL (class editor), the left arm and Colt drawn mirrored (seen on screen; not yet seen firing or reloading). Test commands: tank_fire, mortar_test.
<!-- /claude-2026-11-06-handoff -->

<!-- claude-2026-11-05-handoff -->
**HANDOFF 2026-11-05 ~22:20 (Claude; source+install 0.5.795 built OK; nothing committed; friend zip on hold): read docs/CLAUDE-SESSION-2026-11-05.md FIRST.** Tanks on Seelow: map changes fixed (Seelow->Merville->Seelow and Merville->Seelow->Merville run clean; DemonWare bdSession::leave guard, Kangaroo and vehicle packs loaded with the first map, vehicle files read once a map). Tanks drawn whole again (Saint-Lo cut vehicles_panzer.cod, gentler; 785-790's tight cut drew them invisible). Tank shots/hits: the game's own events from Merville's table via a hook on sub_82555160 (0.5.795) are UNVERIFIED: the user fires a tank, then check the 'made by the game from Merville's table -> R' lines (R must be >= 0).
<!-- /claude-2026-11-05-handoff -->

<!-- claude-2026-11-03-handoff -->
**UPDATE 2026-11-03 ~20:05 (Claude; source+install 0.5.758; nothing committed): flares have WaW's own sounds, burn whoever stands on them, and bots throw them (kits with the Mosin, Webley and flares). Bot smoke throws were broken before and now work. Read the top of docs/CLAUDE-SESSION-2026-11-03.md.**
**UPDATE 2026-11-03 ~19:00 (Claude; source+install 0.5.751; nothing committed): the signal flare is finished: thrown, flies, lights after 1 s where it lands, no smoke (its grenade sunk, WaW stick left), blinding verified. Read the top of docs/CLAUDE-SESSION-2026-11-03.md.**
**UPDATE 2026-11-03 ~17:30 (Claude; source+install 0.5.743; nothing committed): the signal flare now looks like WaW's flare stick (thrown, lying, in others' hands; first person keeps the game's grenade). Read the top of docs/CLAUDE-SESSION-2026-11-03.md.**
**UPDATE 2026-11-03 ~16:40 (Claude; source+install 0.5.741; nothing committed): World at War's SIGNAL FLARE (class editor SPECIAL row) + Seelow fires centred + drain pipe roof. Read the top section of docs/CLAUDE-SESSION-2026-11-03.md.** Open: ask the user to throw a flare (scripts cannot throw); WaW's flare stick model; bots cannot throw it.
**HANDOFF 2026-11-03 ~01:00 (Claude; source+install 0.5.738; nothing committed; friend zip on hold): read docs/CLAUDE-SESSION-2026-11-03.md FIRST.** Seelow/Seelow Night: 4 collision files (every prop's real shape, prop-triangle sign fixed), ladders confirmed, pipe/wall hole/tunnel kerb fixed, WaW's 5 burning cars get CoD3's vehicle fire, night lamps stay in their rooms, the user's 21 lanterns + a sandbag (seelow-work\my_lights.txt, night only). 0.5.738: Mantle assist F11 setting, off for players, bots keep it. Everything in it is installed and tested (fires1 screenshots: the cars burn).
<!-- /claude-2026-11-03-handoff -->

<!-- claude-2026-11-02-evening-handoff -->
**UPDATE 2026-11-03 ~00:05 (Claude; source+install 0.5.738 built OK; nothing committed):** Mantle assist is an F11 setting, off for players, bots keep it. Seelow: 4 collision files (all 500 prop shapes), wall hole widened, ladders confirmed by the user; Seelow Night: the user's 16 lamps, moonlight +5%. Read the evening file's sections from "~22:45 FINAL" down.
**UPDATE ~22:45 (Claude; install 0.5.737, no C++ build; nothing committed): read the "~22:45 FINAL" section of docs/CLAUDE-SESSION-2026-11-02-evening.md.** Seelow + Seelow Night reinstalled: WaW's small clip out (kept where it is a prop's only collision: rocks), prop hull triangles' sign fixed (invisible walls round hedgehogs, trees, train cars, water tower), ladders flagged, drain pipe crawlable, every lamp/fire lit at night and kept to its room, the user's own lanterns (seelow-work\my_lights.txt).
**HANDOFF 2026-11-02 ~20:45 (Claude; source+install 0.5.737; Seelow + Seelow Night installed; nothing committed; friend zip on hold): read docs/CLAUDE-SESSION-2026-11-02-evening.md FIRST.** Seelow's collision now matches WaW's data spot by spot (no clip swap, glass out, shot-only floors solid, prop budget 30000, model names past slot 82 fixed: sandbags not flowers, Opel trucks not Panzers); foliage upright (wheat, hedges, pine, grass cards); Seelow Night brighter with WaW lamp pools and Longyard's moon/fog; host: 'where:' position log (0.5.735), mantle to helper height (0.5.736), step 28 on converted maps (0.5.737). WAITING: the user's yes/no on removing WaW's 1320 small player-clip brushes (the 'invisible objects').
<!-- /claude-2026-11-02-evening-handoff -->

<!-- claude-2026-11-02-afternoon-handoff -->
**HANDOFF 2026-11-02 ~15:40 (Claude; 0.5.734 installed; Seelow + Seelow Night reinstalled; nothing committed; friend zip on hold): read docs/CLAUDE-SESSION-2026-11-02-afternoon.md FIRST.** Fixed: WaW clip bits no longer swapped (AI clip had walled 206 of 292 climb spots/windows), WaW glass out, prop collision budget 30000 (boxed props 312 -> 127), wheat upright (SWAP_UV), Seelow Night brighter. Open: user still reports stuck/falls/invisible things (not re-tested after the last reinstall; player position not logged in normal play), 'Rouen grass' (none found: ask for a screenshot), night sky orange dot + moon + WaW lamps (side session local_90c76740).
<!-- /claude-2026-11-02-afternoon-handoff -->

<!-- claude-2026-11-02-midday-handoff -->
**HANDOFF 2026-11-02 ~12:15 (Claude; source+install 0.5.731 built OK; nothing committed; friend zip on hold): read docs/CLAUDE-SESSION-2026-11-02-midday.md FIRST.** Seelow plays clean (Rouen leftovers repacked out: 23 banks free; mantling 0.5.729). Seelow Night (devmap seelownight; NIGHT=1 tools/waw/seelow_install.sh): moonlit, Longyard's night sky; OPEN (the user's current task "polish Seelow Night"): tan distance fog (lead: fogcolor/fognear/fogfar entity keys, sub_82564698) and arms/gun lit by day. Vehicles NOT working: parked safe (pack .off, no vehicles.txt lines).
<!-- /claude-2026-11-02-midday-handoff -->

<!-- claude-2026-11-02-handoff -->
**HANDOFF 2026-11-02 ~09:25 (Claude; source+install 0.5.728 built OK; nothing committed; friend zip on hold): read docs/CLAUDE-SESSION-2026-11-02.md FIRST.** Seelow (WaW) is an installed map pack (devmap seelow), mid-way to the user's '1 to 1 with WaW': open = verify the last install by eye (grass/wheat/wrecks), the last bot run did not play (dropped Rouen .dcgb?), snagging (WaW mantle ledges, turned model boxes), tanks (user OK'd CoD3's). Mosin now one shot sound (0.5.728).
<!-- /claude-2026-11-02-handoff -->

**2026-11-02 ~02:00 (Claude, overnight, no build; install 0.5.727): SEELOW (WaW) IS A PLAYABLE MAP PACK** (saves\content\00000002\Seelow, devmap seelow; rebuild with tools/waw/seelow_install.sh): WaW's own geometry, textures, lightmap and clip-map collision, spawns/flags from mp_seelow.ents; bots roam it, 144 fps, no crash. ~03:00: its 1627 static models (pine forest, maples, carts, crates, train cars) are in too. ~06:35: the models collide too (WaW's own model collision hulls; bots stall more round the water mill and hedgehogs). Missing: WaW's sun-shadow map; bot routes don't know the props. ~07:30 after the user's play: see-through fixed (e_decal_simple wrote no depth -> e_blend, Carentan's set), WaW's own Seelow sky in, WaW player clip bit swapped (0x20000 -> CoD3 0x10000). Details: docs/CLAUDE-SESSION-2026-11-01-night.md, "SEELOW IS IN THE GAME".

<!-- claude-2026-11-01-night-handoff -->
**Claude handoff 2026-11-01 ~23:05 (source and install 0.5.725, built OK; nothing committed; friend zip on hold; Carentan still set P, "perfect now" per the user): read docs/CLAUDE-SESSION-2026-11-01-night.md FIRST (whole file is the handoff).** Done: World at War's M2 flamethrower nearly finished: fire not bullets (impact records cleared), WaW's own arm animations on CoD3's arms (tools/waw xanim.py / wawpose.py / make_flame_arms.py), the flame starting at the nozzle as the game draws it (the M2 draw's own constants), the gun drawn over its flame (stencil mask), others' flames in the world (fading with distance, hidden behind walls), WaW's overheating instead of a reload (heat bar over the ammo box), damage 26. UAV: an oval fitted to the map and to the draw distance (Isle 3113 x 1722). Next: the user plays 0.5.725, then CoD2's scoped Mosin-Nagant (plan in CLAUDE-SESSION-2026-11-01.md), Webley fingers, Seelow. Open: grey wall puffs with the M2; Isle deathmatch's closed gates (a smaller layout? not checked).
<!-- /claude-2026-11-01-night-handoff -->

<!-- claude-2026-11-01-evening-handoff -->
**Claude handoff 2026-11-01 ~18:40 (source and install 0.5.702, built OK; Carentan installed as set P; nothing committed; friend zip on hold): read docs/CLAUDE-SESSION-2026-11-01-evening.md FIRST (whole file is the handoff).** Done today: Webley + flamethrower kill-feed icons; Carentan: CoD2-exact lightmaps, Rouen collision/props/trees/grass removed (renamed files are still loaded: drop them from the directory), floor gaps fixed (266 CoD2 triangles rebuilt), white-outs fixed (BSP leaves -> one cell), black walls fixed (decals on e_decal, drawn after walls; additive decals out). User: "the walls look perfect now". Open: the player "frozen" spot at -689 1601 (0.5.702 logs "player still"), check the last test's 23 low-triangle seconds, then flamethrower / Seelow / scoped Mosin.
<!-- /claude-2026-11-01-evening-handoff -->

**2026-11-01 ~17:55 (Claude; install 0.5.701 + Carentan set N, nothing committed): falls under the map fixed (266 CoD2 collision triangles with missing vertices rebuilt), Rouen prop batches + trees off, blended posters on e_decal. Read docs/CLAUDE-SESSION-2026-11-01.md "Carentan: falls under the map...". Install: runvariant.sh N.**

**2026-11-01 ~17:30 (Claude; source/install 0.5.701 built OK, nothing committed): Carentan had ROUEN'S COLLISION live (renamed, still loaded): removed (merge_zone.py), stuck bots 172 -> 46. Read docs/CLAUDE-SESSION-2026-11-01.md section "ROUEN'S COLLISION WAS STILL LIVE".** Install Carentan with `bash tools/carentan/runvariant.sh M <case>`.

**2026-11-01 ~16:55 (Claude, user here; source and install 0.5.699 built OK, nothing committed): read docs/CLAUDE-SESSION-2026-11-01.md, sections "Webley kill-feed icon" (SOLVED for Webley AND flamethrower) and "Carentan interiors" (lightmaps now CoD2-exact: byte 0, set cellsM installed; CoD3 world constants read: amb = 1 + 0.2 x normal.z).** 0.5.698/699 carry a small render diagnostic (programs sampling a 512x512 8888 texture, 24 lines max) that can go.

**2026-11-01 ~16:25 (Claude, user back; source and install 0.5.697, built OK, nothing committed): read docs/CLAUDE-SESSION-2026-11-01.md, sections "Webley kill-feed icon" (SOLVED: own icons for the Webley AND the flamethrower in the kill feed, 0.5.697 verified) and "Carentan interiors" (in progress: shaders dumped, world shader not yet identified).** Bot test kit script: scratchpad webley_test.sh (restores the user's bot_kits.txt, sha1 149c2a2f...).

**2026-11-01 ~13:20 (Claude, user away; source and install 0.5.693, built OK): read docs/CLAUDE-SESSION-2026-11-01.md, its FIRST sections ("Carentan playable", "Webley kill-feed icon", "Scoped Mosin", "Seelow groundwork").** 0.5.693: the Webley's kill-feed look-up answered (no white box) but the picture is still the Colt's (next step in the notes). Final Carentan check carentan-final693: CTF flag taken/returned, foliage fine, 144 fps.

**2026-11-01 ~13:00 (Claude, user away; source 0.5.691): read docs/CLAUDE-SESSION-2026-11-01.md, its FIRST sections ("Carentan playable", "Seelow groundwork").** Carentan plays: CoD2's spawns/flags/objectives (CTF capture seen, war zone occupancy counted by the game), CoD2's 1532 placed models drawn (they were the invisible walls), Rouen's props/trees/effects off, Rouen's asset-only zone blocks dropped (96 banks x 3 MB limit). Installed in the pack: cellsL + scene. Seelow: entities extracted; world needs a WaW zone deserializer (plan in the notes). 0.5.691: COD3_WEBLEY_ICON_SLOT test switch.

**2026-11-01 ~11:50 (Codex, then Claude; source/install 0.5.690, no newer build, nothing committed): read the FIRST section of docs/CLAUDE-SESSION-2026-11-01.md ("Codex continuation", its (Claude) bullets at the end).** Textures bind (0.5.690 directory order) and Carentan now DRAWS in the Rouen harness (cellsH: all geometry in Rouen's outdoor cell 001; the white ground was our grid putting the town in Rouen's small room-cell slots 005/006). Open: our stain decals draw solid black on e_decal_simple, dark interiors, Rouen's scene props still there. The note below is older.

**HANDOFF 2026-11-01 ~10:30 (Claude): source and install 0.5.689 (built), nothing committed. Read the top section of docs/CLAUDE-SESSION-2026-11-01.md ("END OF SESSION HANDOFF").** CoD2 Carentan's drawn level is drawn in CoD3 (scratch harness in the placeholder pack); open bug: our textures are white (imports not found); next step: run the 0.5.689 COD3_TEXLOOKUP=1 test. Warn the user before any game launch.

**2026-11-01 night (Claude, no build): read docs/CLAUDE-SESSION-2026-11-01.md, "Carentan step 2".** Writers now exact against the game's own files: APKF blocks from parts (504/504), zone index .xenon.ib (46/46 zones), world-cell meshes (929/929), textures (6809/6809 2D), materials (1190 world-type). CoD2 Carentan's 266 DXT textures read with full mips. Next: convert Carentan's geometry + lightmaps into cells/materials/textures, build zone archives, test in game (warn the user first).

**2026-11-01 afternoon (Claude, no build): read docs/CLAUDE-SESSION-2026-11-01.md, "Carentan collision".** The .cgb writer is done (all 55 MP files rewrite byte for byte) and CoD2 Carentan's whole collision converts to it (tools/cod2import/cod2collision.py), verified against CoD2's data (200000/200000 points, 12819/12819 terrain triangles). Next: the level's zones, scene, drawn geometry, spawns. Carentan = CoD2's real map as its own map, never a Rouen reskin.

**Handoff 2026-11-01 08:30 (Claude): read docs/CLAUDE-SESSION-2026-11-01.md.** Carentan's zone collision (.cgb) is fully decoded (tools/kapf/cgb.py checks all 49 MP .cgb files OK): next is its writer, then CoD2's brushes into it.

**0.5.687 (2026-11-01 07:25, Claude; read docs/CLAUDE-SESSION-2026-10-31.md, "The flamethrower in game" and "Carentan writers" steps 2-4).** World at War's M2 flamethrower is in (class weapon 23): WaW's model in both hands, its own colours, a flame drawn from the nozzle, WaW's ignition/loop/cool-down sounds, WaW's numbers, its world model in others' hands; bots may carry it. Tools: tools/waw (fastfile reader, make_flamethrower.py). Carentan: CoD3's level archives are fully decoded at the pack level (tools/kapf/apkf.py), its textures and meshes read (xetex.py, xemesh.py); the scene and collision formats are next.

**0.5.680 (2026-11-01 00:25, Claude; read docs/CLAUDE-SESSION-2026-10-31.md, "Night of 2026-10-31 to 11-01").** The Webley: cylinder turns a sixth a shot and shows the rounds left (user confirmed); reload fingers and thumbs fixed per joint; the reload's gun moves in the view as CoD2's (tables from retarget.py, data only). Kill feed icon: our own (hud_webley) drew a white box, so the Colt's stays by default (COD3_WEBLEY_OWN_ICON=1 to work on it). Carentan (a real level: scene, collision, meshes, textures, lighting, AI, game database writers) and WaW's flamethrower (zone parser for models/anims; WaW's own flame-stream system) are scoped in the notes, not started.

**0.5.675 (2026-10-31 23:10, Claude; read docs/CLAUDE-SESSION-2026-10-31.md, "The Webley's own reload, built").** The Webley reloads as in CoD2: CoD2's reload played on CoD3's arms at run time (sub_82186740 hook, tables from tools/cod2import/retarget.py in custom/webley), the gun breaking open, the empties out, the speed-loader in. Scripted tests only (COD3_WEBLEY_RELOADTEST); a real reload from firing is the user's test. The left hand is very close to the camera at the speed-loader.

**0.5.671 (2026-10-31 21:35, Claude; read docs/CLAUDE-SESSION-2026-10-31.md, "0.5.671: J's co-op v46 merged").** J's co-op v46 is in: a stable joining copy, the seamless checkpoint reload, the controller lobby, the mission running behind the pause menu. A two-copy Forest test on this PC played through the join and the freeze with no crash. Open: the host's low fps beside the joining copy on one PC, one DbgBreak on making the second player. The Webley reload is paused at the asset dump (0.5.670/671, COD3_ASSET_DUMP).

**0.5.669 (2026-10-31 20:40, Claude; read docs/CLAUDE-SESSION-2026-10-31.md, "0.5.669" and "The six-shot reload").** The Colt's slide and hammer no longer show as shards on the Webley, and the Colt's chrome cube no longer makes it silver; tested clean. Next for the Webley: its own reload animation (APKF extraction, then .xeanim).

**Carentan step 2 (2026-10-31 20:20, Claude, no build; read docs/CLAUDE-SESSION-2026-10-31.md, "Carentan step 2 done").** The pack's folder is now `carentan` (saves\content\00000002\Carentan, map 20). tools/kapf/fli.py rewrites a level's manifest; its pointers are load-time fix-up chains (word 0x18 is the fix-up table's offset: never move it). Tested: loads, plays 2.5 min, sound. Still Rouen's level inside; next is the APKF files, then CoD2 geometry.

**Carentan step 1 (2026-10-31 18:40, Claude, no build; read docs/CLAUDE-SESSION-2026-10-31.md, "Carentan step 1 done").** Carentan is a map pack (saves\content\00000002\Carentan, MapPack 4, ID 20, folder `caren`), listed and loaded the way the game's DLC maps are; for now it is Rouen's level under the new name (tested with devmap: loads, plays, sound). The menu list is not yet checked by eye. Next: our own FLI writer, then CoD2 geometry (.xecell). Tools: tools/kapf/xex.py, make_pack.py.

**0.5.668 (2026-10-31 16:50, Claude): read docs/CLAUDE-SESSION-2026-10-31.md ("Carentan as its own
map").** The user wants Carentan as its own map in the map list with its own level file (not drawn over
another map); 0.5.667's overlay was removed in 0.5.668 (Isle as before). tools/kapf reads CoD3 archives
(MP ones are uncompressed); next: the APKF pack's files, the map list, a renamed copy of a small map.
Webley reload (CoD2's xanims onto CoD3's arms) is queued after.

**0.5.666 (2026-10-31 13:45, Claude): read docs/CLAUDE-SESSION-2026-10-31.md ("Webley, midday" and
"Carentan, afternoon").** Webley: faces forward, posed from CoD2's idle anim, CoD2's normal/specular maps,
last-shot crash fixed, its own world model (others see it), bots may carry it, sized by height (sights on
the Colt's). Still the Colt's arm animations. Carentan: read and converted (tools/cod2import/cod2map.py,
cod2assets.py); in-game drawing is the next experiment. WaW not installed.

**0.5.657 (2026-10-31 03:45, Claude): read docs/CLAUDE-SESSION-2026-10-31.md.** CoD2's Webley is in CoD3 MP
  (class editor WEBLEY; CoD2's model, texture, numbers and sounds; tools/cod2import). Not yet played: its
  fire/reload sounds, others see the Colt. Menus by mouse done (user: "it works"); countdown; render list guard
  (643's crash). Carentan: research plan in the session file.

**0.5.633 MG42 fix VALIDATED on Isle (2026-10-30 12:38, Claude):** run_case.py isle-633: override log 13->25 items, MG42 + arms drawn and firing, check_snapshot.py PASS. Source/install 0.5.633. Not done (user: "not now"): Rouen control, 8-bot Isle, bots' third-person MG42. Then: killcam explosions (trace a live grenade), confirm the 631 grenade keys. Logs: logs-backup/20261030-123552-isle-633-result.

<!-- codex-2026-10-30-handoff -->
**Codex handoff 2026-10-30: source633; install0.5.632; 633 runtime UNTESTED.**
Read [CODEX-SESSION-2026-10-30.md](CODEX-SESSION-2026-10-30.md).
Root cause: Isle's mounted `mg42` animation entry shadows our handheld alias.
633 scopes the MG34 override to handheld setup. Build status: building: 0.5.633 handheld MG42 animation-table collision fix; no test request.
User requested wrap-up; all games stopped, original INI restored, no test queued.
<!-- /codex-2026-10-30-handoff -->

# Pick me up (read this first)



**0.5.631 (2026-10-30 00:08, side session, END: usage limit): read docs/CLAUDE-SESSION-2026-10-30.md.** Done:
  SPECTATOR as the team choice's own fourth row (looks like the game's), the seat's HUD follows the camera, killcam
  steady and with the killer's HUD ("looking nice"), bots' grenade keys 0x2 only (double throws). Open: MG42 invisible
  in first person (no hands either; PLAY-MG42-PLAIN.bat tells the swap from the weapon), killcam explosions (not in
  the replayed objects at all), co-op on two PCs.

**0.5.622 (2026-10-29 20:15, side session):** co-op v40 (Team Friend's 0.5.920 source) merged: World at War style
  campaign co-op, F2 lobby (coop*.cpp, docs/COOP-V40-DESIGN.md); kept our real sprint, left out their forced picture
  defaults; the campaign starts (BENCH-CAMPAIGN-START.bat). Untested on two PCs. Menus: lobby lists learn their row
  (0.5.620), main menu and System Link known (+250, +246), clicks always take (0.5.621).

**0.5.616 (2026-10-29 12:25, side session):** the seat's blur fixed (the game's last full screen pass 2a90db64 mixes in a
  blurred copy while the team choice is up; in the seat its texture 1 gets the frame: render_commands.cpp, verified by
  screenshot); free camera has its own aim (mouse via mouse_look.cpp, right stick); SPECTATOR drawn as a team-choice row;
  lobby screens with no menu entries got the old mouse back, and "menu: shape" lines log each menu class for making them
  pointable from the next play log. Open (not mine): an intermittent startup stall since 0.5.611 ("no swap for five seconds,
  after 1", next to mp_fixes.cpp's "hud images" probe reading 0x446A2AAB, a float, as a pointer).

**0.5.613 (2026-10-29 11:05) - spectator, menus by mouse, campaign (side session).** PATCH-NOTES 0.5.590-0.5.613;
notes in the side-session folder (spectator-0.5.490.md).
- SPECTATOR'S SEAT (0.5.608-613, self-tested 5 times, BENCH-SPECTATE-SEAT.bat, COD3_SPECTATE_SEATTEST): a SPECTATOR
  button under the team choice (or F6 / Back+Y, also mid-match): no soldier (the game's own spectator, state 1, kept
  by not picking a side; mid-match the game's SpawnSpectator sub_82214B30), menus taken down, cg_draw2D 0, our camera
  (free / his eyes / behind, fire/aim next/previous). Leaving: team choice back, or mid-match a spawn at the side's
  point (sub_822029F0, as bots). spectator.cpp, menu_mouse.cpp, one line in mp_fixes.cpp MpFixesLauncher.
- Spectating with the soldier left standing: F11 > Multiplayer > Spectator buttons (0.5.490); gun kept with the view
  (0.5.604). Writing +1924 directly does not stick (scripts own it).
- MENUS BY MOUSE: campaign 0.5.595 (Frank's ac3b2a52 port), multiplayer 0.5.602 (vtables matched from image dumps:
  COD3_IMAGEDUMP; SetSelection +192, update sub_824D5FC0, FEImage 0x820705B4, FEText 0x820702DC); 0.5.613 MP menus in a
  match free the pointer. Pointer toggle F11 Controls (0.5.600). NOT yet tried by a person.
- CAMPAIGN: 0.5.599 legal-screen freeze fixed (look shaders compile on a thread); 0.5.601 sprint (Frank's sprint.cpp).
- Before any game run: back up the four logs (install logs-backup\); the image-dump runs lost a play log once.

**0.5.608 (2026-10-29 00:20) - START HERE.** Read docs/CLAUDE-SESSION-2026-10-29.md (CTF state, open items in order:
608 test results, the Fill-the-screen smoke outline via PLAY-FRAMELOG.bat, campaign checks, load crash).

**0.5.598 (2026-10-28 21:05) - START HERE: CTF, latest.** PATCH-NOTES 0.5.578-0.5.598 have the evidence.
- Natural 10-min self-tests: takes 579 2, 580 1, 581 1, 582 3, 583 3 (583: side 2's first). CAPTURES: the "1 capture"
  reported for 579-582 was a render line miscounted ("captured programs"); real captures unknown there, 583 had 0.
  Fixed: ctfreport counts flag home straight from carried; 584 logs "flagwatch: flag N CAPTURED by slot S".
  82 fps, no crashes. Summary: `python -I tools/ctfreport/ctfreport.py <log>`.
- 578 stairs walkable; 579 route-tail logs; 580 links keep the real half-second step a player took (doorways);
  581 attackers within 450 of the enemy flag don't fall back hurt; 582 each flag's "ways out" (trails of players
  walking out of its base) are followed backwards by attackers within 700 of it; 583 attackers shoot while
  running for the flag; 584 carriers follow those ways out forwards.
  585 ways out saved per map (bots\maps\rouen-ctf-ways.txt, 32 so far); 586 stuck-carrier help never puts a
  carrier back toward the enemy base. First confirmed capture: 584 (slot 27, helped twice). 585/586: carriers now
  leave the enemy base by a way out and get to mid-map (-800 750), then die; 0 captures. 587 Team Friend look-update-2 (only film_hd.cpp + tools were new). 588 a take goes to the
  other side only (586 carrier test had side 2 defenders taking their own flag). 588 natural: 1 take, 1 capture
  (slot 6, put forward 4 times by the stuck-carrier help: carriers stall at full speed, cause: 590/591).
  589/590 carriers go home along their own trail this life (reversed); 591 Reachable walks a slope up to 384
  (was 192: on hills no route spot was walkable, routes lost every few s). 591 natural: 3 bot takes, 2 captures
  (bot slot 21 on his own trail, helped 3x; host slot 0 for side 2), bots 0.9 ms a frame.
  592 carrier into own flag building by its ways out; 594 BadLinkCost 20000; 596 bots leave own base by its ways out.
  Stalls (tools/ctfreport/stalls.py: 2 traces 3 s apart, both to goal, under 30 moved): 591 8.7%, 594 8.1%, 596 8.6%;
  the hot spots move around (600 1200 halved, own base halved), the total is a plateau about 8%. 597 back-and-forth stuck test (8 s within 200; fired under 25 times
  a match); 598 standoff: attackers hunt the enemy carrier when ours is home but our flag is out (not yet seen in a
  test). 598: stalls 7.2% (lowest), 1 take, no capture, no crash. Next hot spot: side 2 base 1800 400.
  Intermittent LOAD crash before any bot runs (583 GPU hang at the menu; 596 memset in sub_82137C38 from sub_82142248,
  write past committed guest memory): reruns were fine. Log kept in the session scratchpad (crash-0.5.596-load.log).
- Open: side 2 never takes the flag at -1792 -91. Their routes now go the right way in (582 trails), but they
  die on the approach (combat, not walls). Carriers are often put back along their trail by the stuck helper.
- Next after CTF: Frank's menus-by-mouse (ac3b2a52) and direct mouse look (596d4609) for MP; campaign exe rebuild
  with the graphics merge; check other modes/maps; friend zip when wanted.

**0.5.571 (2026-10-28 03:00) - START HERE: CTF, where it stands.** Read PATCH-NOTES 0.5.541-0.5.571 for the evidence.
- Fixed and verified by self-test: sides/teams (host's own team read is right; bots' given side; flags from bot spawns
  read the OTHER way round, the port spawns bots by the enemy end; 545), match start (each team at its own flag, 558/570),
  the freeze (a bot's death wrote a HUD arrow past the 4 local players into the heap: sub_82452580 guarded, 557;
  hardware watch on 82C89A68 still on, COD3_LISTWATCH=0 off), learnt-map routing (bad links avoided 554, route start
  walkable 553, re-anchor 560, "lost" only if no spot within 200 563), base exits 85%+, regroup before assault (567),
  carriers use Think's route home (568), Team Friend's graphics merged (555, look off by default).
- Self-test tools: BENCH-CTF-ROUEN.bat (4 min), -LONG (10 min), -CARRY (COD3_CTF_CARRIERTEST=1: attackers put by the
  enemy flag every 60 s). Summary script: scratchpad ctfreport.py (copy it into tools/ if kept); ctftrace lines in logs.
- Results: natural takes happen (567: 3 in 10 min); the carry test takes 10/10 and saw full captures (570, 571: slot 27
  carried 16 s and capture value 1). Lone carriers die 0.3-2 s after the take in the enemy base.
- NEXT: the 571 carrier test placed NO escorts (0 "as his escort" lines: the BotsGroundZ/120 check or only one living
  attacker found) - fix that, then measure carries with escorts. Then a played match by the user.
- Also asked: Frank's "menus by mouse" (ac3b2a52) and "mouse turns the view directly" (596d4609) are not ported yet;
  his key prompts are (0.5.478).

**0.5.538 (2026-10-27 13:20) - START HERE: CTF WORKS FOR BOTS.** Take, carry, capture, drop, retake and return all work. The client requests bots now make (BotFlagClaim/CarrierUnstick, bots.cpp): take = Callbacks +3420 (side whose flag, &handle, 0); capture = +3432 (slot, 1) on own flag with it home (0.5.533 proved it); drop when killed = +3424 (&handle). A carrier holds the flag weapon (command weapon) and never fires (firing drops it). Carriers walk their own trail home; ctf-routes.txt keeps routes from every run to the enemy flag (the user's too) and attackers walk them. The unattended test (BENCH-CTF-ROUEN) is now a clean match: the probe/traps need COD3_CTF_PROBE=1. The user plays in those matches. Next: carriers die before home (escort/pathing), half attack / half defend is in since 0.5.484.

**0.5.525 (2026-10-27 01:55) - START HERE: BOTS TAKE FLAGS.** The CTF take is REQUESTED BY THE PLAYER'S CLIENT (message handler sub_823EB8F0 -> Callbacks 0x82BCBF64 +3420 = sub_82205788(side whose flag, &handle, 0) -> flag handler sub_82204A08). Bots have no client, so BotFlagClaim (bots.cpp) makes the request for them: works for both sides (0.5.517-518). +3424 (sub_822049D8) DROPS the carried flag; +3428 = sub_821F33A0. Carriers walk their own trail back (BotTrail, 0.5.522), CarrierUnstick puts them along it / by the flag. CAPTURE NOT YET SEEN: asking +3420 with own side or carried side at the base did nothing; even the host TELEPORTED home while carrying did not score, so a capture likely needs WALKING into the zone: needs a real match to judge. Carrier per flag by its own weapon (mp_flag_allies = side 2's flag). Takes no longer flip the sides.

**0.5.515 (2026-10-27 00:10) - START HERE:** FLAG PICKUP, engine side PROVEN FINE for bots: touch -> sub_8256C168 queues (sub_82567328) -> consumer sub_8256FFC8 (wrapped in bots.cpp, logs "touchqueue") resolves both handles, all bot flag touches PASS -> sub_820B0898 notifies the script "trigger" with the bot. So the CTF SCRIPT receives the bot and refuses him. Ruled out: button, look, sides, raw entity/record/session fields (all copied), script table fields (table = +0 count, +8 ptr to (hash,value) pairs; bots lacked 7 fields and had FEFEFE junk; filled in: no effect), the side callbacks (+3320 with (h,1,1) only notifies; session side change sub_823EA4E0), contents (+324 same). COD3_BOT_SIDE_CALLBACK=1 exists (test bat sets it). Next: trace the script interpreter (sub_82204A08) for the flag thread after the "trigger" notify, bot vs host. Also fixed today: MG42/FG42 clip (0.5.502), CTF code alive while a flag is out (0.5.503).

**0.5.505 (2026-10-26 21:45) - START HERE:** FLAG PICKUP: pickup is by TOUCH (the idle host takes flags standing in his spawn; X not needed). Bots touch the right flag (its trigger records them) but the CTF SCRIPT refuses them on a script-side property. Ruled out: the button, looking at it, the sides, every raw entity/record/session field where host and bots differ (all copied to a frozen bot: no take; 498's "bot take" was the idle host, credited by distance). Script variables: lookup works (sub_821DE680(0x4000|entity-table index) then sub_821DE958(vars, hash of name via sub_820E10E0); "spawntime" found) but the field list could not be enumerated (the vars pointer leads into a string pool). Next if continued: decode the script variable store or trace the pickup bytecode. Carrier is now taken from the flag weapon (mp_flag*). FIXED today: 0.5.502 MG42/FG42 clip with a rifleman-based custom class (SpecialToFour restores clips); 0.5.503 CTF code no longer switches off while a flag is out (bases kept). Unattended test: BENCH-CTF-ROUEN.bat via test-request.txt; CtfTakeProbe (bots.cpp) only runs with COD3_BIG_MODE set. Defender once blocked XenonRecomp.exe (allowed by the user).

**0.5.495 (2026-10-26 evening):** build was blocked by Windows Defender (WinError 225 on tools/build-scan/XenonRecomp/XenonRecomp/XenonRecomp.exe, a false positive); the user allowed it. 495's pad scan: the words that follow X are mostly the host's RELOAD animation (X reloads): 826FEE38 is read by animation code (sub_823ED000 -> sub_820B85C8), not the take check. Pad-word hunting is noisy; still unknown what the take checks. Facts: bots fire the flag trigger but are refused; the host takes it only with the PAD (not with X written into his command). Open question worth one test: can a REMOTE human (the friend) take a flag? If yes, bots differ from remote players, not just from the host.

**0.5.494 (2026-10-26 02:20) - START HERE:** WHY BOTS NEVER TAKE THE FLAG: the CTF take reads the LOCAL PAD. Proved in 493: X in the host's network command (what bots send) did NOT take it; the pad's X did. The take runs in the script interpreter (sub_82205788 -> sub_82204A08 -> sub_821E5718 -> sub_820C4E60 -> sub_824C65B0 moves the flag model). 494's unattended test (bench-ctf-rouen.log, may have run after the user left) logs "padscan" (guest words that follow X) and "pad word ... read by" (the script built-in that reads them, during the host's take). NEXT: override that built-in so a bot holding 0x40 in his command counts as pressing. Self-test loop: test-request.txt = BENCH-CTF-ROUEN.bat with a build request. Spectator mode (F6 free camera / follow) built in 0.5.490 by another session, untested.

**0.5.487 (2026-10-26 01:15):** UNATTENDED CTF TESTS WORK: test-request.txt = BENCH-CTF-ROUEN.bat (CTF Rouen, 24 bots, 240 s, log bench-ctf-rouen.log). Sides now from PROOF (ctf-flags.txt; Rouen: side 1 owns -1792 -91). 487 test: bots at the REAL enemy flag pressed use 812 times, its trigger fired 78 times with a bot on it, NO take: the game refuses bots after activation. Not the button, not the look, not the sides. Next: find what the CTF take checks (player vs bot); record+136 ("ground") is 0 for every bot always, check the host value. Dead bodies still "SLIDE" (velocity zeroing did not stop it; may be normal death motion). Hops on strike 2 now happen.

**0.5.484 (2026-10-26):** CTF spawns confirmed right by the user. Bots still never took a flag (X alone: no touch at all; combo: touched, refused). 484 tries looking down at the flag (pickup prompt) with X alone / X+activate in turns; TAKEN line says which. CTF roles: half attack, quarter middle, quarter home; escorts ring a carrier. SLIDES lines hunt the T-pose sliding. Open: navigation out of spawn (1200 stuck lines), bots spreading out.

**0.5.483 (2026-10-26):** bots take flags holding 0x40 ALONE (the user's button; 0x20|0x40|0x400 never worked). Carrier sprints home (CarrierHome), no firing, teammates escort. CTF sides set at match start (remembered per map, start sweep). "objectives: ctf settings:" probes for a return-time setting (user wants dropped flags home after 6 s: not done). Then: bots spread out.

**0.5.482 (2026-10-25 late):** CTF on Rouen: the game STARTS each side by the ENEMY flag (proved in 481: the user, side 1, took and scored the flag at 2112 462, where side 1 starts). Own flag = farther from the first spawns; a take corrects it. "flagwatch:" lines give each flag's real state (TAKEN by slot / dropped at / home): taken = flag script_model at z -9970. Bots use it. Pickup = hold use on the flag. Next: bots spread out (clustering). Run tools/mplog/mplog.py first.

Last updated by Claude, 2026-10-25 22:50. **Source/install 0.5.479** (friend zip
`D:\Call of Duty 3 (USA, Europe)\CoD3MP-update-0.5.479.zip`).
**Top issue: long games freeze with the game heap full** (0.5.478: 611 of 637 MB at
9.5 min); 0.5.479 logs "heap: game heap ..." every 30 s to find out whether it leaks.
Justin is testing it. **Read docs/CLAUDE-SESSION-2026-10-25.md first**: what
0.5.426-0.5.478 did, the open CTF thread, the how-tos (build, friend zip, logs,
the SP->MP address tools in tools/mpmatch/).

- **Main open thread: CTF bots on Rouen.** 0.5.476 (untested): every spawn
  more than 900 from its own flag moved by it (the game spawns each side by
  the ENEMY's flag); dropped flags chased; use pressed on flags (the flag is
  taken with use). Justin, 0.5.475: "no bots actually walked up to the enemy
  flag", bots stuck in spawn and walking into walls.
- **0.5.477** the lamps' glow fades behind walls (ported from FrankHUN88453's
  CoD3Recomp). **0.5.478** button prompts as keys, switching to the pad's
  icons when the pad is used (ported; F11 Controls "Button prompts"). Both
  untested: grep "the title reads the depth" and "prompts: the button icon
  font".
- **Killcam freeze (host, 0.5.475):** the 1 KB particle pool ran empty, then
  the game's malloc hung; 0.5.476 made that pool 8x. Grep "apsMemory".
- **Do not "sync fork"** with FrankHUN88453's upstream: port pieces by hand.
- Waiting on Justin: brightness slider in the MP Options menu, bot room
  clearing, auto-updater (on hold).
- Bot history: docs/BOT-PATCH-LOG.md. Per-version detail: docs/PATCH-NOTES.md.

## Earlier (2026-10-24, 0.5.425, performance work)

Last updated by Claude, 2026-10-24 17:25. Install/source 0.5.425: co-op v38 merged (campaign only). OPEN: an intermittent STARTUP RACE in the vertical blank handler (freed read at 0xA0D90004, a long stall, once a crash at map load): see the end of docs/CLAUDE-SESSION-2026-10-23.md. 0.5.424: pshufb byte swaps (WriteRegisterRunFrom 6.5% -> 4.4% of cp) + native rotate sub_82147008. SMALL NATIVE PIECES ARE AT DIMINISHING RETURNS: read 'Where it stands' at the end of docs/CLAUDE-SESSION-2026-10-23.md. 0.5.423: native table search sub_820B64D0 (3000/3000; 1.1% -> 0.02% of main). 0.5.422: native packed-vector unpack sub_82181520 (3000/3000; 1.65% -> 0.60% of main). 0.5.421: native box sweep sub_8248BB48 (5000/5000; 1.1% -> 0.09% of main). 0.5.420: native segment-box test sub_82490A30 (5000/5000; 0.9% -> under 0.1% of main). 0.5.419: native trace tree walk (record-and-replay check, 3000/3000), walk 2.3% -> 1.6%. 0.5.418 fixed the freeze when a killcam ends (both host and friend hit it in 416); not yet proven in a 2-player game: look for "killcam guard: ... killcam not playing". Friend's 14 fps in 416 was their GPU slowing 7x for ~55 s (game's CPU work steady 3-5 ms); cause outside the log. Next main-thread targets (0.5.421 profile): sub_82164160 1.7% (a loop of virtual calls: little to gain), sub_8241B2A8 0.9%, sub_82167E28 0.8%, sub_82147008 0.75%, leaf tests sub_82499F58 / sub_8249DDC8, sub_8248C708 (0.6%, VMX flush switches); Killcam::Record memcpy 0.8%. Table: docs/CLAUDE-SESSION-2026-10-23.md. Read newest NEXT-SESSION STATE.

**Goal:** a steady 60 fps on Marseille (map "ordnance") at 32 players.

**Where we are (0.5.407):** Marseille city views (BENCH-MARSEILLE-CITY.bat)
68-69 fps with profilers on, 70-76 without (were 63-64 at 0.5.396); main
thread 14.2 ms a frame (13.4 without profilers). Heavy views (5000+ draws)
~47-57 fps. Rimling 32p median 45 (0.5.369) -> 65 (0.5.405).

**What changed 0.5.391-0.5.407:** a second drawing thread for heavy frames
(4000+ draws; from 3000 was worse); script-thread fibers pooled (SSE control
word carried across switches, stacks cleared on reuse); the title's memcpy
native; calls through function pointers inline; constant packets copied four
words at a time; profiler names callers; box tree walk SSE2; written-page check
one read; depth copy SSE4.1; guards below.

**Open problems:** (1) the "effects crash" (job worker, sub_824472C0 ->
sub_820FC850, also in Codex 0.5.384) is guarded from 0.5.403 ("fxguard:" in
the log), cause unknown. (2) One freeze (0.5.406 Rimling): game heap full, a
cache (sub_82185D18) retrying for ever; 0.5.407 logs "heapwait:" and sleeps
1 ms a retry; cause unknown. Grep test logs for both words first.

**What limits it now:** heavy views: the command thread (17-19 ms); elsewhere
the main thread (~14 ms, fully busy). CPU target stays x86-64-v2 (no AVX2:
friends' older PCs). Switches: COD3_DRAWTHREAD=0, COD3_FIBERPOOL=0,
COD3_FIBERCLEAR=0, COD3_NATIVE_MEMCPY=0, COD3_FXGUARD=0, COD3_HOSTPROFILE_SELF=1.

**GitHub:** source-only v0.5.407 published by Codex: https://github.com/hovg-N8/CoD3Recomp/releases/tag/v0.5.407 (commit40db64a6856fbf7dce494f715b1c9284e587a4fe). No game data/binaries/logs. Release407 excludes408. Local nested Git checkout still atA3; fetch before pushing.

**How to test (no clicking needed):** put the bat's name in test-request.txt
(source root), then a build-request.txt whose text starts "test only". The
auto-builder starts the bat (BENCH-*.bat or BIG-PROFILE-32.bat only). Or
request a real build and the test starts after it. BENCH-MARSEILLE.bat = 32
players, camera held at 4 places (35 s each), bench-ordnance*.log;
BENCH-MARSEILLE-AB-*.bat flip a setting every 6 s at the same place.

**How to build:** commit source and docs first (new "## 0.5.N:" at the top of
docs/PATCH-NOTES.md), then build-request.txt in a separate step. Use a UNIQUE
local file name for every file you send (the bridge can resend a stale copy).

**Never upload game data to GitHub** (ISO, .cod/.xex, generated ppc/ppc_mp,
textures, shaders, exes).

**408:** exact heap-wait request/overflow and safe heap capacity/usage audit; off COD3_HEAPWAIT_AUDIT=0. Rimling test completed clean; freeze not reproduced. See newest STATE.

**Details:** docs/NEXT-SESSION.md (top lines), docs/HANDOFF-MARSEILLE-60FPS.md,
docs/CODEX-HANDOFF-0.5.384.md (history), docs/RELEASE-0.5.407.md.


