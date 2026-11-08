# Claude session 2026-11-05 (0.5.781 - 0.5.795): tanks on Seelow, map changes, the tanks' shots

**State at handoff (~22:20):** source and install **0.5.795** (built OK). Nothing committed. Friend zip on
hold. The game is closed. Earlier work on this vehicle thread (0.5.759-0.5.780: the database pack, Saint-Lo's
models, engine sounds, fires under the cars) is in the top sections of docs/CLAUDE-SESSION-2026-11-03.md.

User rules (verbatim, still in force): "warn me before any test launches the game. Back up my play logs
before any run. Don't run builds while the game is open. Never upload game data or videos. No .bat files."
The user wants exact CoD3 tank behaviour ("exactly like how a tank shoots", as on Merville), no stand-ins,
and is frustrated with regressions: test what you change on screen, not only in the log.

## Where to pick up (in order)

1. **Verify 0.5.795's tank events.** No test has seen it work yet: in tankevents5 no bot got into a tank (they
   go for one, then give up). Ask the user to fire a tank's cannon and its MG on Seelow, then "check the log":
   - `vehicles: the map's archive N (object X) is named 'Y': a tank gun's events looked up as mp_merville.fx`
     (once). Y is the map archive's own name.
   - `vehicles: <gun>'s event N made by the game from Merville's table (the map's M) -> R`: R must be >= 0
     now. 0.5.793 and 0.5.794 gave -1 for every event.
   - The user should see the muzzle flash, the shell's hit (tank_dirt etc.) and hear the shots.
2. **If R is still -1:** the set lookup is `sub_8254D628(r3 = r24, the first argument of sub_82555160;
   r4 = record +500; r5 = the "<name>.fx" text; r7 = "EVENT.FX" at 0x8206725C)`. Its scope r24 may not reach
   the pack's database. Read sub_8254D628 and what r24 is, and compare with Merville itself (log the map
   archive's name there: it should be mp_merville). A test can fire a tank only through bots (luck), or add a
   host command that drives the turret's fire.
3. **If R >= 0 but nothing is seen:** check the effect lookup (`assets: tank_dirt (from 824402E8)`) under the
   pack's scope (sub_82554BA0's 779 switch) and that the vehicles' effects pack is in the lists.
4. Heap with Merville FIRST is not measured since the gentler Saint-Lo cut (0.5.791). Estimate ~604 of 608
   MB at the end of Merville's load. If Laison's mortar files come in short there, trim more (see below).
5. Still open from the user: engine sound while driving (cues found in the log, not confirmed by ear),
   explosive damage from the shell (not checked), the Kangaroo and BMW on Seelow (later).

## What was found and fixed (each with its test)

### Map changes with tanks (the user: "seelow crashed my game when trying to load", Merville then Seelow)

Several separate problems, each hiding the next:

- **0.5.783** vehicle files read once per map load. `sub_82561838` is called twice a map, and every call
  re-read Merville's and the Kangaroo's vehicle files, so the kinds overflowed the table (17 -> 31 -> 45).
- **0.5.784** took out 0.5.781's forced fresh reload of Laison's after-map archives. Reloading them caused
  a 254k assert storm (DbgBreakPoint 0x82543B8C from sub_8218D9B4) and a freeze (mapchain1). They have been
  reused since 0.5.296 without trouble.
- **0.5.787** DemonWare guard. After a map with tanks, the next map's `bdSession::leave` (strings at
  0x820333C8; sub_821C1AA0 -> sub_821C1758 -> sub_821B1DC8 -> sub_821C08B0) walked a "peer" list that was
  static memory (0x826F9xxx/0x8270xxxx), with data 0x10 or 0x4 and a garbage count. It read through all
  memory (mapchain2/3, seelowseelow1). The COD3_PEERTRAP=1 write trap (0.5.786) showed DemonWare's own code
  writing those words at match start: it is not a list. sub_821C08B0 now skips a list that cannot be one
  ("demonware: guard: NOT walked"). It fires about once per map change, on the bad peer only. Calls through
  0x826F/0x8270 pointers ("call: through a pointer to 0x8270....") also happen on vanilla map changes
  (rouenmerv1) and are long-standing.
- **0.5.788** the Kangaroo before the map on Merville and Poissons too. Loaded after the map, it lived in
  Merville's banks, and Seelow then reused it after they were freed: a call through 0x82704190 (mervseelow5).
  The Saint-Lo cut carries v_tracked_tread/wheel, which is why the Kangaroo used to wait for the map.
- **0.5.790** the vehicles' effects pack, database pack and Saint-Lo cut load with the FIRST map, whatever
  it is, when a map other than Merville/Poissons has vehicles saved. They are kept and used only on a map
  with tanks. On a later map they loaded into a heap still holding the last map (602-603.6 of 608 MB) and
  came in partly: 35 of 76 entries, 2 of 8 kinds, no tanks (mervseelow6, mervchain1). The heap's "used"
  barely drops at a map change.
- **Verified:** mapchain4 (Seelow -> Merville -> Seelow, 478 s, 0.5.787) and mervchain2 (Merville -> Seelow
  -> Merville, 468 s, 0.5.790): no crash, Seelow with 8 kinds, 6 tank guns, 10 tanks placed.

Not causes (ruled out with logs): the game heap for DemonWare (0.5.785 logs failed DemonWare allocations:
none), the vehicle physics pool (15, checked by the game), the physics presets (10, checked; Merville's 5
kept across maps, static).

### Memory: the cuts (game data, install only, built by tools/waw/seelow_install.sh)

- `game\sp\saint_lo\vehicles_panzer.cod`: Saint-Lo's vehicles.cod without the P-47, Fw 190 parts and
  prisoner walks (`repack.py --keep '.' --drop '(p47|fw190|f190|prisoner|patrolwalk|propeller)'`), 7 blocks,
  17.5 MB, both tanks drawn whole (tankview1 screenshots). **0.5.785-0.5.790's tighter cut (Panzer only,
  --keep '(panzer|tankshell)') drew the tanks INVISIBLE and the 50 cal grey** (the user's play on 0.5.790).
  A cut's dropped textures become 8x8 stand-ins and it sits first in the lists. That one is kept as
  logs-backup\vehicles_panzer.v1-invisible.cod. Delete vehicles_panzer.cod to rebuild it.
- `game\sp\crssrds\vehicles_stuka.cod`: Crossroads' archive without its Sherman, Kangaroo textures, A-20 and
  C-47 (3 blocks of 5, 4.1 MB), loaded on every map for the UAV's Stuka. The Stuka in flight has NOT been
  looked at on screen since the cut: check it.
- tools/kapf/kapf.py `decompose()` now decompresses campaign (LZO) blocks, so repack.py takes them.
- Heap per kept archive (LoadExtra logs it now): 4 campaign weapons archives ~120 MB, Saint-Lo cut ~17,
  Laison vehicles (mortar) 10.7, Kangaroo 8.1, Stuka cut 3.9, Typhoon 3.3, effects 5, database 0.6.
  Seelow first ends at 602.9 of 608.

### The tanks' shots (the user: "why dont the tanks shoot an explosive round like they do on merville")

- How the game does it: a weapon event is queued by `sub_82556180` (record: weapon name at +16, up to 128
  bytes; event number at +292) and made by `sub_82555160`. That looks the weapon's event SET up by NAME:
  `<scope archive's object name (sub_82472BD8, +4)>` + ".fx", category "EVENT.FX". The scope is
  *(*0x82BCC828 + 40), the map's archive. Then each event of the set goes through sub_82554BA0. Seelow's
  map is a Rouen copy with no tank guns in its set, so nothing is made. Merville's database (in the pack)
  has "mp_merville.fx". Its events: projexplode_sherman_turret_dirt -> weapon/tank/tank_dirt.effect
  (tank_dirt.ng.bpeb is in the effects pack).
- 0.5.780's hand-made events (our own script_origins, named events, cue play): the shots were heard, the
  hits never seen. Off since 0.5.792 (COD3_TANK_STANDIN=1 brings them back). "tankblast x y z event"
  console/COD3_CMD command (0.5.791) makes one where asked; blastview1/2 could not show even the
  Hellfire's control blast (timing or placement), so treat those pictures as inconclusive.
- 0.5.792: hook sub_82555160, switch the scope for tank guns (read +500: wrong, always 0). 0.5.793: reads
  the name at +16: tank events caught (sherman_turret 25/26/27/2/53/7/41/45/46, 50 cal 25/26/2/53/7/54), all
  -1. 0.5.794: renamed the PACK archive's object: sub_82472BD8 has none for it (00000000), so still -1.
  **0.5.795: the scope stays the map's, and the MAP archive's object is named mp_merville for the call.
  UNVERIFIED.** COD3_TANK_EVENTS=0 turns the switch off.

## Tests this session (logs-backup\<stamp>-<name>-result)

mapchain1-4, mervseelow5-7, mervchain1-2, rouenmerv1 (control: no crash), peertrap1 (write trap),
tankview1 (tanks visible with the gentler cut), blastview1-2 (inconclusive), tankevents1-5 (bots fire tanks
only sometimes: tankevents2 and 4 had shots). The user's play on 0.5.790: logs-backup\20261105-userplay-790.
Test command shape (tools/cod2import): `python -I run_test.py <name> --map seelow --seconds 540 --bots 4
--env "COD3_CMD=110:noclip;114:setviewpos 3500 4780 60 0" "COD3_SHOTS=100"`. Shots are about 24 s after
the CMD clock.
