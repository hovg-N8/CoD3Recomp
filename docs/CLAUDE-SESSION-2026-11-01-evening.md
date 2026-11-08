# Claude session 2026-11-01 (afternoon/evening): HANDOFF - start here

State at hand-off (~18:40): source and install **0.5.702** (built OK). Carentan installed as **set P**
(`bash tools/carentan/runvariant.sh P <case>`). Nothing committed. Friend zip still on hold.
User's last word: "the walls look perfect now". Session ended at the usage limit.

## How to work on Carentan (scratch harness on Rouen's archives)
- Install / test: `BOTS=10 SECONDS_RUN=240 SHOTS="COD3_SHOTS=30 90" EXTRA_ENV="COD3_BIG_MODE=ctf" bash tools/carentan/runvariant.sh P <case>`
  (variant letter = carentan-work/c2/cells<letter>; it merges zn01-03 from Rouen's, edits the
  level scene, sets the BSP to one cell, drops Rouen's grass, installs into
  saves/content/00000002/Carentan/mp/carentan, runs tools/cod2import/run_test.py). WARN THE USER
  before any run (the game pops up). Results: logs-backup/<stamp>-<case>-result.
- Rebuild cells: `C2_TEST=C C2_ONE_CELL=1 C2_MODELS=1 python -I tools/cod2import/cod2cells.py "C:/Program Files (x86)/Activision/Call of Duty 2/main" mp_carentan mp_rouen "D:/Call of Duty 3 (USA, Europe)/carentan-work/c2/cells<X>"`
- Rebuild collision: `python -I tools/cod2import/cod2collision.py "C:/Program Files (x86)/Activision/Call of Duty 2/main" mp_carentan carentan-work/rouen-coll/mp_rouen_zone01.cgb carentan-work/c2/carentan_all.cgb`
- Camera aimed at a point (0.5.702): `EXTRA_ENV="COD3_BENCH_VIEW_SECONDS=200 COD3_BENCH_VIEWS=x,y,z,0,0 COD3_BENCH_LOOKAT=tx,ty,tz,dist"`
  (the bench never holds a yaw; the camera follows the view instead).
- Tools (new this session, tools/carentan): probe.py (CoD2 brushes/tris near a point), holes.py (floor
  gaps on a grid; "above" -140 skips CoD2's giant caulk floor), ghost_hits.py (bots' hits vs CoD2
  collision; its body model gives false "floor" hits), scene.py `onecell` / `drop` (below), floor.py.

## What was fixed this session (all verified in game unless said)
1. **Webley + flamethrower kill-feed icons** (0.5.694-697): image object +0x14 points at its own D3D
   header (copy must move it); texels must be in Guest::AllocatePhysical memory (the game cuts texture
   addresses to 29 bits). OwnIcon table in mp_fixes.cpp. Verified (icons697).
2. **Interior lighting** (data): CoD2 lights an unbumped surface with byte 0 of each lightmap plane
   (its lib/lightmap.hlsl + $lightmapweights builder in gfx_d3d_mp_x86_s.dll), not the average.
   CoD3 world shader: colour x ((1 + 0.2 normal.z) x lm.rgb + sun x shadow x lm.a x N.L + dyn lights).
3. **Rouen's collision was live under Carentan** (user was right): renamed files are still loaded (the
   loader walks the archive DIRECTORY by type). merge_zone.py leaves Rouen's .cgb out. Stuck bots 172 -> 46.
4. **Bots**: Climb lifts a stair (18) before the whole rise (steep cellar stairs under a ceiling);
   Detour 96 traces; NetProgress "room" test (450 units / 30 s -> strike 4) (0.5.700/701).
5. **Falls under the map**: 266 CoD2 collision triangles with 0xFFFFFFFF vertex numbers were dropped;
   cod2collision.corners() rebuilds them from plane + svec/tvec (corners (s0,t0),(s0+1,t0),(s0,t0+1));
   264/266. Gaps 400 -> 28 (map edge only).
6. **Rouen props / trees / grass**: zone .ng.xmbb/.ng.xmpbb (props) out of the directory; SpeedTree key
   renamed `xpeedtree`; **level.bgib (Rouen's grass in the road) out of the level's directory** (set P).
7. **View going white** on roofs (19k of 409k triangles drawn): the camera's cell comes from Rouen's
   BSP (mp_rouen.btrb; nodes from 0x58, 16 bytes: (-2, plane, front, back) or a leaf (cell, ...);
   519 leaves, 141 "no cell"). scene.py `onecell`: every leaf -> cell 001. Verified (no drops in the
   same views). Note: dropping Rouen's cells (DROP_CELLS=1, test only) changes the sky - not used.
8. **Black walls**: blended decals (phong_blend: SOZE, LYON, CAMOT, posters) on e_decal (Rouen's blended
   decal shader, e_blend's 9 params, overlay 1) AND drawn after solid surfaces (cell submesh order:
   solid, alpha test, decals, blends). Additive decals (phong_add: lightstain_04, near-black DXT1)
   left out (they painted a facade black). User: "the walls look perfect now".

## OPEN (in order)
1. **Player "frozen in place"** twice at -689 1601 0 (grass alley; nothing solid there; bots walk it).
   0.5.702 logs "player still: ..." (what the body box meets 8 units each way) when the user stands
   still 2 s: NEXT: user plays, then grep "player still" in his log (the bench runs fill it with
   uninformative lines: ignore those).
2. **Check the last test** logs-backup/20261101-183459-carentan-P702-result: 23 seconds under 100k
   triangles (the LOOKAT camera flew behind walls on purpose; confirm it is not the white-out back).
3. Grass: the user still sees grass "in the middle of the road" -> level.bgib now dropped (set P, not yet
   seen by the user). CoD2's own grass = 950 brush_newgrass_* models along walls: correct, keep.
4. Still Rouen's: sky (keep: the user is fine with it), ambient sounds (rouen.wbk), light grid,
   .dcgb (trigger brush models: war/dom use them), .phyb, AI files, cfgstrb, colorid csvs.
5. CoD2's map-wide trigger_hurt (z -165..-149) not in the port (only matters if a gap remains).
6. CoD2 turrets not placed; mantle windows can't be vaulted in CoD3 (no mantling).
7. Then the user's list: flamethrower (WaW arm anims, others' flames), Seelow (WaW, needs a T4 zone
   reader: notes in CLAUDE-SESSION-2026-11-01.md "Seelow groundwork"), scoped Mosin (plan there too).

## Rules learned (also in memory)
- Rename is NOT hiding: anything left in an archive's directory is loaded by type.
- Texels the game fetches must be in physical guest memory (29-bit addresses).
- Always prove a Rouen-leftover theory with numbers (bots stood on Rouen floors to the unit) before
  fixing; the user was right twice ("invisible Rouen geometry", "grass in the road").
- Logs: back up CoD3MP.log before any run (user plays are in logs-backup/user-play-2026-11-01-*).
