# Claude session 2026-11-07 evening to 2026-11-08 morning: friend's kill feed, War triggers, Seelow ground, bots (tanks, extra guns), clean-up

## LATEST: HANDOFF 2026-11-08 ~03:30 (READ THIS FIRST)

**State:** source + install **0.5.865**, built OK, last test clean (botmix865: Seelow, 12 bots,
6.5 min, no crash, 104 fps median). **Nothing committed**: the fork (D:\Call of Duty 3 (USA,
Europe)\CoD3Recomp-fork) is still at 0.5.851. No friend zip since 0.5.851 (the user: "Not yet").
The user went to bed ~00:20 and asked for overnight work on everything that did not need the friend.

### What the user confirmed in play (2026-11-07 evening)
- Seelow: no more falling under the map ("its fixed"), and the hill/tracks by the middle flag fixed
  after the flat-top slabs (coll_s).
- Tank machine gun with a human gunner works.

### Done tonight (0.5.852-0.5.865), each with a test
| build | what | proof |
|---|---|---|
| 852 | friend's kill feed for bots + death sync (C3BS sent at once, the mirror dies at health 0 too, obituary callback); bots spare mates (tank/artillery; grenades later reverted); Mosin scoped-in = Kar98 scope view | friend's 851 log: 245 mirror deaths, 0 "dies here", feed only humans. NOT yet seen with the friend |
| 852 (map data) | War/Dom/HQ triggers moved into mp_rouen.dcgb (tools/carentan/triggers.py); Seelow's .dcgb back in its directory | carentan-war852, seelow-backstop852: the game's holder 53377998 changed, play moved flag to flag |
| 852 (map data) | Seelow backstop slabs (seelow_clipmap.py backstops(), now gap 0: coll_s), rail upright faces out (RAILS) | user walked it: fixed |
| 853 | COD3_ADS_TEST="a b" (tests can hold the sights; COD3_PAD holds stop the side pick) | mosinscope854 |
| 854 | tank drivers follow learnt-map routes; far tanks get more time; bot log spam capped (COD3_BOT_DIAG=1 for all) | botveh854: give-ups 9 -> 4; log 7.1 -> 5.6 MB |
| 855 | grenades back to 0.5.851 (user: grenades may hurt mates); bullets hold for a mate in the line 34 + 4% (was 30 + 3%); tank route cells within 900 | botmix856: routes found (34 spots) |
| 856 | drivers aim the turret and fire at enemies 250-3000 (no mate within 350); random extra gun for ~60% of bots without a kit second gun (not anti-armor): flamethrower, webley, colt_dual, mosin_scoped, kar98_sniper, lee_enfield_sniper, springfield, m1garand, kar98, mp44, thompson, mp40, bren, sten, gewehr43, fg42, shotgun | botmix856: first bot tank shell ever |
| 857 | the extra gun stays in hand (g_wantGun, as Rusher 2) | botveh857: one switch, no flip-flop (856 had x59) |
| 858-863 | WHY bots' tanks never moved: every vehicle code read of the move-lock byte (0x82C73024 + his +2804) zeroes a bot's moves. The tank's kind has +540 set (109) so the vehicle think (sub_82591B40) takes the tanks' way (sub_82589C98, sub_82591450, sub_82590FC8), not sub_8257FAF0. 861: the whole think runs with the bots' bytes clear (BotLocksClear) | tankdrive863 (Seelow, Merville): a bot's tank MOVES for the first time (yaw turns, ~200 units in 100 s) |
| 864 | a person's tank logged for comparison ("vehicle think, a person driving") | needs the user to drive |
| 865 | extra gun roll from std::mt19937 (the tick roll gave 0 of 6) | botmix865: 4 bots with extra guns, each used |

### Still open (in order)
1. **Bots' tanks crawl** (~5 units/s, the place changing every other 2.5 s sample: 0, 12, 0, 7 ...
   Seelow; 0, 27, 0, 26 ... Merville: not the ground). The input reaches the tank now. Suspect: the
   host treats the bot's vehicle as another machine's and applies corrections now and then. Next:
   the user drives a tank once (any map) -> compare "vehicle think, a person driving" lines (speed,
   +1936 command times) with the bot's "tank drive test" lines (tankdrive863_* logs). Test command:
   COD3_CMD="25:tank_fire 110 1" COD3_TANK_DRIVE_TEST=1 (the tank_fire bot also drives).
2. **Friend tests** (needs the friend on 0.5.865): his kill feed shows bot kills; bots drop without
   freezing/cartwheeling (his log: "mirror number N dies here as on the host ... kill feed line made").
3. **Flare HUD** (the user: "it shows all 3 grenade symbols instead of just the flare"): NOT
   reproduced. flarehud852/852b (class 1: 4 grenades + flare special): one grenade icon with a count
   (x4, x3 after a test flare throw; the count went down for the FLARE while the icon is a frag's),
   "SIGNAL FLARE" over it. Ask: where on screen and when (spawn, holding the bumper, after a throw?).
   Pictures: logs-backup\20261108-002926-flarehud852b-result.
4. Bots and friendly fire: the guards are in; a match WITH friendly fire on has not been run (no
   test hook sets it; the System Link "team damage" row does).
5. Dual Colts third person, Kangaroo treads on Merville: unchanged (need eyes).
6. Commit + push (tonight's code, tools/carentan/triggers.py, seelow_clipmap.py/seelow_install.sh
   changes, docs) through CoD3Recomp-fork + GitHub Desktop, when the user says.
7. Friend zip: 0.5.865 + the new Carentan, Seelow, Seelow Night packs (they changed tonight).

### Clean-up (the user: "clearing up the leftover junk")
~15.6 GB to the Recycle Bin (NOT deleted): old friend/source zips 0.5.193-0.5.671, co-op test zips,
the nested old repo copy CoD3Recomp\, CoD3Host\_backup_before_controller_fix\, seelow-work
coll_b/coll_r, screenshots\*-draws.bin, build-backups\before-0.5.2xx. Full list:
docs/CLEANUP-2026-11-08.md. Left for the user: screenshots\*.png (~15 GB, mostly test frames, maybe
his own), logs-backup (25 GB of records).

### Traps learnt tonight
- The build stamps its version from PATCH-NOTES.md's "Current source version" line WHEN IT ENDS: an
  edit that fails (Edit's "file modified since read") leaves the old number (0.5.854 and 855 were each
  built twice under the old number). Check the header after every notes edit.
- A build started before an edit may hold part of it: request the next build only after the edits.
- Bash heredocs eat backslashes (again: a python heredoc's "\\n" became a newline, and "\b" in notes
  text became a backspace char). Use the Edit tool or chr(92).
- Bots' vehicle work: the move-lock byte. Every bot has it set (networked "not this machine's
  player"); any vehicle routine reading it ignores a bot. BotLocksClear (bots.cpp) clears them for
  one call.
- mp_rouen.dcgb layout (tools/carentan/triggers.py docstring): 112-byte records (bounds, radius,
  model number at +0x68), then cgb-style volumes; Rouen's brush models are world-space at origin 0;
  CoD3 does not move them by the entity's origin.
- Converted maps: a one-sided mesh lets a player sink in and fall through; a solid brush slab with
  its top ON the surface (not under it: under it held the player inside the ground) fixes both.

### Test logs of tonight (CoD3-PC-Port-0.4.0\logs-backup\)
20261107-232226-carentan-war852, 20261107-232815-seelow-war852 (the user's falls),
20261107-234020-seelow-backstop852, 20261107-234601-seelownight-backstop852,
20261108-000738-play-after-hill (the user's play), 20261108-002643-flarehud852,
20261108-002926-flarehud852b, 20261108-003238-mosinscope852 (side pick stall),
20261108-004409-mosinscope854, 20261108-004602-botveh854, 20261108-012108-botmix856,
20261108-013247-botveh857, 20261108-014324-botveh858, 20261108-015259-botveh859,
20261108-020145-botveh860, 20261108-020556-tankthink860, 20261108-020919-botveh860b,
20261108-021928-botveh861, 20261108-022915-botveh862, 20261108-023908-tankdrive863_1,
20261108-024352-tankdrive863_merv, 20261108-025635-botmix864, 20261108-030916-botmix865.
Earlier this session: 20261107-224620-mptest (the user's and the friend's 0.5.851 MP logs).
