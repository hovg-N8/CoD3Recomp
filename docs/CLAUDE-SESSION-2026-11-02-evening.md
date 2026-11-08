# Claude session 2026-11-02 (afternoon to evening): HANDOFF

**State: source and install 0.5.737 (built OK). Seelow and Seelow Night installed ~19:45 with every fix
below. Nothing committed. Friend zip on hold.** Step-by-step detail (tests, numbers, dead ends):
docs/CLAUDE-SESSION-2026-11-02-afternoon.md. Earlier today: CLAUDE-SESSION-2026-11-02-midday.md.
A side session ("Seelow Night: find the tan haze colour", local_90c76740) did the night sky, fog, lamps:
its sections are in the midday and afternoon files.

## ~20:50 update: small clip brushes removed (the user: yes)
seelow_clipmap.py drop_small_clip(): 1200 of WaW's 1320 small player-clip brushes out (bottom < 72 above
the floor under their middle, or sunk in it); 120 up high kept, the 516 tall kept (SEELOW_SMALL_CLIP=1
keeps all). Exactly 1200 .cgb volumes gone, nothing else changed (unchanged rebuild was byte-identical
first). Old: seelow-work\coll_m.before-smallclip. Both packs reinstalled (Rouen unchanged). Bot test
logs-backup\20261102-205306-smallclip-result: 8 bots CTF, flag taken, 0 unsticks, 20 stuck lines (21
before), 121 fps median. A "GPU is hung" stall at start-up (66 s, before any file was read; just after
the user's PC crashed), then normal play. Next: the user plays Seelow, "check the log".

## ~22:05 update: after play7 (logs-backup\20261102-user-play7, Seelow Night)
User: rocks walkable through, ladders not climbable, can't get round hedgehogs (tracks, tunnel
entrance), no lights in the bunker. Done (both packs installed; tests NOT run: the user opened the game):
- seelow_clipmap.py drop_small_clip v2: WaW's small clip is the ONLY collision of some props: kept where
  a clip-map placement taller than 28 and 16 wide has no hull of ours (rocks 131 boxes, tractor 9,
  wagon wheel 4, big rubble 4); floor_under() fixes boxes sunk into ground over a lower floor (the 90x90
  hedgehog boxes were kept as "up high"): 1125 out, 47 up high kept, 0 hedgehogs boxed. 33 creek rocks
  have no clip in WaW either. Ladders: WaW's 5 ladder brushes now surface 0x40b8 + type (as Rouen's two:
  0x8010000 / 0xd040b8); they were walls (surface 0). Old: coll_m.smallclip1, coll_m.before-smallclip.
- seelow_cells.py: lamp parts glow (own flat lightmap GLOW_LIGHTMAP, index LIGHTMAPS+1; day only WaW's
  _on ones); at night all 55 lamps (hang_lamp4_off too) + 8 fireplaces + 4 stoves + WaW's 5 createfx fires
  (the effects script is in the zone) + seelow-work\my_lights.txt (user's own "x y z"); pools at least
  LAMP_WARM (235,190,125) within 220 fading to 520. Cells: cells3m (day), cells3n (night; cells3n.lamps1/
  .lamps2 earlier tries). Install: CELLS=cells3m / NIGHT=1 CELLS=cells3n seelow_install.sh.
- Test lamps1 (first glow, weaker pools): bulbs lit, tunnels dim brown. lamps2 + coll2 bot test pending.
- Start-up "GPU is hung" + 15-66 s stall since the user's PC crash (not in earlier logs): watch.
- Own lights plan: user stands still 10 s at dark spots; read those from "where:" lines into my_lights.txt.

## ~22:45 FINAL of the evening (user played live 22:04-22:26, logs-backup\20261102-user-play8)
Installed both packs (CELLS=cells3m / NIGHT=1 CELLS=cells3n, COLL coll_m = scratch new4; Rouen unchanged):
- **Model hull triangles had the wrong sign** (seelow_clipmap model_collision: corners where s.p - sw,
  t.p - tw are 0/1; read with + until now): 132 of the hedgehog's 148 triangles, 446/546 box car, 79/90
  maple stood up to 363 units outside their models = invisible walls round every meshed prop (hedgehogs,
  trees, train cars, the water tower: a stray wall in front of its ladder at 2791-2796 -1247..-1153).
  Now every triangle inside its box; .cgb mesh points 51000 -> 34000. Budget 35000+ still overflows a
  .cgb (65535 indices): kept 30000 (101 props in turned boxes, e.g. the Opel wrecks).
- Drain pipe under the north road (x 719-1281, y 6146-6211): its end rings (48 triangles, hole 22 wide at
  the floor) out of the collision; the user got in at 1257 6170 and stuck (PIPE_CAPS).
- Night lamps stay in their rooms: seelow_cells lamp_occluders/lamp_sees: a texel lit only from in front
  of its surface (vertex normal) and in sight of the lamp past WaW's 3647 solid brushes near lamps (exact
  convex test); ceiling lamps nudged out of their mounts (21). LAMP 180/450, WARM (215,172,112). Lit
  texels 74561 -> 21665. User's lamps: my_lights.txt "x y z lantern_on" places WaW's lantern (seelow_models
  EXTRA_PLACEMENTS, night only): barrel 977 2254, work bench 2760 205, crates 2353 -2553; pool 1338 5623.
- Tests (logs-backup\20261102-2234..2241): lamps3 = bunker/tunnels lit warm gold inside, brick room
  lit with its doorway dark, WaW fire lights the grass and a window; ladder1 = player put at the water
  tower ladder (2785 -1199, rungs ahead) did not rise, but "+forward" from COD3_CMD did not move him at
  all either: inconclusive; coll3 = 8 bots CTF, 144 fps, flag taken, 16 stuck lines (20 before), 0
  unsticks, no crash. No "GPU is hung" in any of the three (gone after the crash day?).
- **~23:05 FOUR collision files (installed):** a .cgb's limit is 65535 mesh index bytes (3 a triangle)
  and points; the 3 zone files were 62133/62229/61722 full. Merville proves the level archive's .cgb is
  collision (mp_merv: 10 zones, no zone .cgb, all 1.1 MB in mp_merville.cgb), so seelow_clipmap writes
  4 files (bands by real index/point fill, FILL 64500), the 4th installed as seelow.cod's unzoned.cgb via
  repack.py --replace (new; swap_file.py can't: the block passes its 3 MB bank; level 12 -> 13 blocks).
  SEELOW_MESH_BUDGET 60000: all 500 prop hulls meshed, 0 boxes (248130 index bytes). Proven: test file4
  dropped the player 60 above 3 floors only file 4 holds (4213 -34, 4104 1048, 4021 1254): landed on
  each; coll4: 8 bots, 144 fps, flag taken x2, 0 unsticks, no falls. Old: coll_m.3files.
- **Ladders WORK (user, ~00:20):** climbed the water tower ladder to its top landing (2892 -1130 -62) in
  the 4-file install. WaW ladder brushes with surface 0x40b8 + type are all CoD3 needs (test ladder1 was
  inconclusive only because COD3_CMD "+forward" does not move the player).
- **~23:45 INSTALLED (user: "solid rebuild"):** both packs with WALL_HOLES; Seelow Night cells3n with the
  user's 16 lights (15 lanterns + 1 pool; my_lights.txt) = 88 night lights, 28 splits; day Seelow cells3m
  unchanged (the user's lamps are night only, as asked). Play log logs-backup\20261103-user-play9.
  Tests: coll5 (8 bots) clean, 141 fps, flag x4, 1 unstick at 3344 2834; mylamps1 (camera at the
  user's lanterns) cut at 90 s by a window close request before the camera moves: lanterns not yet seen.
- mylamps2 (re-run, full 217 s): the barrel lantern stands on its lid, the knocked-over one lies on its
  side at the junked car's back-left wheel, the fence one sits on its post; all glow and light round
  them. The user: "lamps look great". Placing a user's lamp: their position from the live log's last
  "where:" line, their heading from their last steps, the prop from draw_placements + its model box.
- **~00:05 (11-03) BUILT 0.5.738** (source + install): "Mantle assist" setting (F11 movement, ini
  mantle_assist), OFF by default for players (the user would rather jump and crouch), bots always
  (bots.cpp BotsIsBotRecord). Test mantle738: 8 bot mantle lifts (31-60 up), 143 fps, no crash, 15
  stuck. Seelow Night moonlight +5% (cells3n with SEELOW_NIGHT=0.59,0.69,0.97; old cells3n.moon56).
- Placed vehicles: static WaW vehicle models could be placed like the lanterns (extend EXTRA_PLACEMENTS
  to day + model_collision); drivable vehicles still parked (midday file). User asked, not started.
- (was) Staged, not installed (game open): WALL_HOLES (house hole 2616-2624 3742-3846: 41 jagged pieces out,
  104 x 72 open; coll_m = scratch hole1, old coll_m.4files); my_lights.txt now 16 entries (lean support:
  "x y z lantern_on deg dx dy", seelow_cells.lamp_axes). Install: night cells rebuild (cells3n) + both.
- Open: "something missing" at 810 3109 (WaW traversal over a ~95 ledge at y 3040-3075; not identified,
  ask for a screenshot); ladders flagged 0x40b8 but climbing not yet proven (test ladder1); start-up
  "GPU is hung" stall since the PC crash; arms/gun daylit at night; low-window crouch; slot 82; vehicles.

## Waiting on the user (answered: yes, done above)
**Decision asked, not answered:** remove WaW's 1320 small player-clip brushes (< 120 tall, over rubble and
debris: the user's "invisible objects") to make Seelow easier to walk, or keep them as WaW has them. The
516 tall ones (walls, map edges, roofs) stay either way. If yes: in tools/waw/seelow_clipmap.py read(),
leave out brushes with contents & 0x10000 and not & 1 whose box is under 120 tall; rebuild coll_m, run
seelow_install.sh and NIGHT=1 seelow_install.sh, then a bot test.

## Done today (all verified in game or by data; tools in tools/waw, data in seelow-work)
Collision (seelow_clipmap.py, coll_m):
- No clip-bit swap: WaW = CoD3 (0x10000 player clip, 0x20000 AI clip). The swap had walled 206 of WaW's
  292 climb spots (windows). CoD3's player mask is 0x2810011.
- WaW glass (0x10, 7 window fillers) left out; up-facing shot-only terrain (0x2080, 108 triangles) solid
  (the user fell through one at 3559 4912).
- Prop collision budget 30000 (props in boxes 312 -> 101); a .cgb holds at most 65535 mesh points.
Models (seelow_models.py, cells2m/cells2n):
- SLOT_FIX: model names past slot 82 by placed boxes (model_headers finds 203 of 207). Slot 86 = sandbags
  (were drawn as blue field-flower patches), 90 = destroyed Opel Blitz trucks (were Panzer wrecks with
  Panzer collision), 83-85, 87-89, 91 too. Slot 82 still unproven (3 small objects).
- SWAP_UV: wheat, both hedgerows, pine (texture turned as read). UPRIGHT_CARDS: the grass clump's far
  cards map the image's 27-degree blades straight up. User: "all the bushes looked perfect".
- All 7582 WaW static models placed (91 models). No Rouen models, grass, effects or lights left.
Night (seelow_cells.py SEELOW_NIGHT=0.56,0.66,0.92 + side session's SEELOW_LAMPS, Longyard sun/fog/moon):
- WaW Seelow has no light entities: light is baked from 55 lamp models (45 lit), all placed; Seelow
  Night keeps their pools bright. Rouen's 11 street lamps moved out (side session). The moon is the
  white disc in the sky (Longyard's). Open: the player's arms/gun lit as by day (light grid samples).
Host (C++):
- 0.5.735 "where: x y z | velocity" once a second (the user's position in every play log).
- 0.5.736 mantle assist climbs each WaW helper's own height (up to 130).
- 0.5.737 step 28 (not 18) on maps with a custom\mantle file (Seelow, Seelow Night only).

## How the user's problems were found (reuse this)
1. Copy CoD3MP.log aside (logs-backup\<date>-user-playN), count "where:" lines, noclip toggles
   ('"noclip" put on'), "unstick:" lines. Noclip dives are not falls: line toggles up with "where:".
2. Route analysis: falls = z 40+ below WaW's terrain; snags = velocity > 40 but < 45 units moved in a
   second; unstick lines name the spot. Scratch script pattern: see the afternoon file.
3. At each spot list player-solid volumes (flags & 0x10011) in coll_m\seelow_zn0[1-3].cgb (tools/kapf/
   cgb.py) and placements (seelow_models.placements / draw_placements).
4. Videos: NVIDIA instant replays in C:\Users\hovgt\Videos\NVIDIA\CoD3-PC-Port-0.4.0 (end time in the
   name). tmp-video\ has view.html + srv.py (".claude/launch.json" config "videosave", port 8766): copy
   the mp4 in as play.mp4, open view.html, fetch it as a blob, grid() + save() write frame grids. Delete
   the mp4 copy after (game footage, never commit).
5. Camera tests: run_test.py with COD3_CMD="46:noclip;52:cg_drawgun 0;58:setviewpos x y z yaw" and
   COD3_SHOTS (commands >= 4 s apart). Warn the user before every launch.

## Open, in order
1. The user's decision on the small clip brushes (above).
2. Seelow Night: arms/gun daylit (side session's notes: light grid samples not decoded).
3. Low windows need a crouch (56 of 292 helpers have 50-71 room; WaW's mantle crouches itself).
4. Slot 82's real model; the stable doorway at 996 861 (~23 wide in WaW's brushes).
5. Vehicles on Seelow: parked (midday file).
