# Claude session 2026-11-01 (night): HANDOFF - start here

State at hand-off (~23:05): source and install **0.5.725** (built OK). Nothing committed. Friend zip
still on hold. Carentan is still installed as set P (unchanged this session: the user says it is "perfect
now": no freezes, no white-outs, no road grass; Rouen's sky stays for good).
Read this whole file; PATCH-NOTES.md 0.5.703-0.5.725 has each build's detail.

## What the user wants next (in order)
1. Play-test 0.5.725 (flamethrower + Isle UAV). The flamethrower is "near perfect" by the user's words.
2. Then **CoD2's scoped Mosin-Nagant** (the user asked to start it right after the flamethrower; its
   plan is in docs/CLAUDE-SESSION-2026-11-01.md, "Scoped Mosin"). Then Webley reload fingers, then Seelow.
3. Open on the flamethrower: the grey puffs on walls where it aims (a second bullet-hit path: the
   weapon's 23 impact records at def+2284 are cleared and stay clear, yet puffs remain); sprint/ADS/melee
   show WaW's idle pose (no WaW sprint anim: WaW uses sprint offsets); the arms model is CoD3's (WaW's
   arms mesh would be a separate job).
4. The user asked whether Isle's deathmatch is "the smaller variant" (gates blocked half the map). Not
   checked: our notes have nothing on per-mode map layouts. CoD3 likely closes parts of maps by mode;
   prove it from the level's data/scripts before answering firmly.

## The flamethrower: how it works now (0.5.703-0.5.725)
- **Fire, not bullets (704):** the weapon's 23 per-surface impact records (def+2284, WeaponMarks 0.5.93)
  cleared; its shot events 1/2/3/7/8/53 keep its own name (find nothing: no Thompson flash/brass).
  (703 tried ammoType +192 = 15: wrong switch, reverted.)
- **Others' flames drawn in the world** (flamethrower.cpp WorldDraw): chunk stream + World at War's
  ribbon (streamFlame/streamFuel from flameconfigs flame_thirdperson_mp in common_mp.ff), traced to the
  first wall every 60 ms (BotsFlameTrace, bots.cpp), hidden unless the gun or both the middle and end of
  the flame are in sight; fire/drips/smoke left behind (WaW's flameVar_ numbers) follow it. Fade with
  distance: full within 300, ((1400-d)/1100)^2, gone at 1400 (720).
- **WaW's arms animations on CoD3's arms** (709-711): tools/waw/xanim.py reads WaW XAnimParts (CoD4
  layout, all streams verified), wawpose.py poses WaW's rig (arms model viewmodel_usa_marine_arms is ONLY
  in SP/zombie zones: nazi_zombie_prototype.ff), make_flame_arms.py writes custom/flamethrower/
  flame_arms.bin (v2: idle/fire/raise/drop, hands by WaW's hand shape + middle knuckle, 15 finger joints
  along WaW's bones, two-bone elbows, the Thompson gun bones held at rest, the arms' pelvis at rest).
  In game (PlayFlameArms, sub_82186740 pose hook): table under the game's pelvis move (sway/bob), squeezed
  along the view by tan(32.5)/tan(fov/2) (fov: camera +96, 90; the arms are drawn at 90, NOT 80).
  Inputs: pose dumps of 0.5.708 holding the M2 (custom/webley/dump/pose_2.bin arms, pose_3.bin Thompson;
  the Colt's originals backed up in custom/webley/dump-colt-backup).
  Rebuild the table:
  `python -I tools/waw/make_flame_arms.py <zombie_proto.zone> <common_mp.zone> <G>/custom/webley/dump/pose_2.bin <G>/custom/webley/dump/pose_3.bin <cod3_hands_skel.json> <G>/custom/flamethrower/flame_arms.bin [check.png]`
  zones: `python -I tools/waw/wawimport.py ff "D:/Steam/steamapps/common/Call of Duty World at War/zone/english/<x>.ff" <out.zone>`
  (game data: keep out of git). The skeleton json (56 bones, bip01 rig + tag_camera) is at
  D:/Call of Duty 3 (USA, Europe)/carentan-work/cod3_hands_skel.json (copied from an old scratchpad).
- **The flame starts at the drawn nozzle** (717): the M2 view mesh's draw constants (dumped by
  COD3_FLAME_VSDUMP=1): c16-18 its rigid bone (to world, the game's placement incl. sway/lag/bob),
  c12-15 view*projection, c0-3 view, c8-11 projection (x scale 0.75 on 16:9 = 90 deg 4:3). Render::
  TrackMeshPoint/TrackedPoint project WaW's tag_flash (in the mesh: -0.013 -19.254 0.979) each draw.
  The draw reads our buffer at its GPU address (0x28000000 lower in the extended window: MeshTexKey).
- **The gun over its flame** (718): overlay.cpp MaskBuild puts the M2 mesh (Overlay::SetWeaponMaskMesh)
  through the same draw transforms (Render::TrackedTransform) into a stencil; the flame (ImGui background
  list) is drawn where stencil = 0 (hook g_ImGuiDX11DepthStencilOverride added to thirdparty
  imgui_impl_dx11.cpp).
- **Overheating, no reload** (720-725): WaW numbers (overheatRate 38, cooldownRate 13, overheatEndVal 50,
  unlimitedAmmo). FlameHeatTick refills the clip each frame for every player holding it, heat 3.8 a
  shot, 13/s cooling; overheated: the fire bit cleared in the player's newest command before ClientThink
  (bots.cpp sub_82593518 hook, MpFixesFlameOverheated). Our heat bar: a panel in the HUD's ammo-box look
  drawn OVER the ammo box (0.798..0.891 x 0.872..0.916 of the picture, measured at 1904x993: may misalign
  at other sizes). The HUD's own ammo widget (IGOAmmoWidget vtable 0x82070F64, parts hudammo/clipammo/
  totalammo at +12/+16/+20) could NOT be hidden (its +4, colours, place: none drawn from them); those
  edits are off (COD3_AMMO_WIDGET_TRY=1).
- Damage 26 a shot (718, was 25; the user: +5%).
- Switches: COD3_FLAME_BULLET_FX, COD3_FLAME_NO_ARMS, COD3_FLAME_VMFOV, COD3_FLAME_RELOAD, COD3_FLAME_TEST
  (draws ours constantly: did NOT draw in 707's tests, not understood), COD3_FLAME_VSDUMP.

## UAV plane (709-720)
- Path: an oval the spawn box's shape, made smaller 8% at a time until no spawn-box corner is farther
  from any point of it (height counted) than 92% of the draw distance (settings draw_distance -> r_zfar
  9000/6000/3500; the user has "far", 9000). The oval's y/x ratio is sent in the plane packet (read when
  long enough). Isle: extra 0.80 x 0.94 of the half-width, +12% +14% +7% height (3113 x 1722, 1913 over
  the players, at most 7893 from the corners). Carentan: 0.80 of the half-width, +12% height.
- Why it popped: draw distance far = r_zfar 9000 and Isle is 8280 long (a player at one end was ~9300
  from the far side of the old round circle). Earlier cause (0.5.608): a plane in the sky brush is
  outside the world (not drawn): kept under the sky by BotsSkyZ.

## Testing
- tools/waw/flame_test.sh <case> [env...]: mp_isle **deathmatch** (COD3_BIG_MODE=dm), 8 bots all on the
  flamethrower (temporary bot_kits.txt; the user's is restored after, sha1 149c2a2f...), screenshots with
  the overlay (COD3_SHOT_OVERLAY=1: also makes the USER's F12 shots include the flame during a test).
  PRIMARY=23 puts the test player on the M2. COD3_UAVTEST_AT=30 calls a UAV. WARN THE USER (game pops up).
- One run (723) stayed in the menus (no map): just run again.
- The user's own play logs this session: logs-backup/user-play-2026-11-01-708, -711 (711's is actually a
  self-test log); everything else is in logs-backup/20261101-*-flame7xx-result.

## Rules learned (also in memory)
- Bash heredocs eat backslashes (\\n -> newline) even in 'quoted' ones: write Python edit scripts with the
  Write tool to a file, then run it. Three broken builds/edits this session came from this.
- `near`/`far` are Windows macros: never name a variable that in a file including Windows.h.
- An mp_fixes edit that greps for exact text must handle CRLF (the scripts here do).
- Prove positions with the game's own numbers (the draw constants), not with our model of the view: two
  rounds of guessing the arms' fov/placement were wrong (80 vs 90; tag_camera vs model space).

## Continued 2026-11-02 ~00:00-01:00 (0.5.726-0.5.727, then Seelow; the user away, "keep going")
- 0.5.726: others' flamethrower SOUND close only (full within 200, ((1000-d)/800)^2, none past 1000).
- **CoD2's scoped Mosin-Nagant is in** (0.5.726-727): Guns[] "mosin_scoped" (copy of kar98_sniper, custom
  class weapon 24 / second 26, bot kits), view = CoD2's viewmodel_mosinnagant_cloth in the Kar98 body
  (k98.w_ge_RFL_VM; bolt/clips/scope parts hidden), world model w_mosin_scoped_WORLD (copy of
  w_ge_SNIP_WORLD with CoD2's mesh, shifted 11.5), kill icon hud_scoped_nagant (seen in the feed), CoD2's
  nagant_sniper.wav shot (the Kar98's fire sounds silenced for its holder). Data: tools/cod2import/
  make_mosin.py (texture atlas 1024x512 from three colour maps) -> custom/mosin; work files in
  D:\Call of Duty 3 (USA, Europe)\mosin-work. Test: tools/cod2import/mosin_test.sh (PRIMARY=24).
  Not yet: CoD2's bolt/reload moves (the Kar98's play; the Mosin's bolt is static), CoD2's numbers.
- **Seelow (WaW mp_seelow.ff): its world geometry is READ** (tools/waw/seelow_scan.py, seelow_world.py ->
  D:\Call of Duty 3 (USA, Europe)\seelow-work\world\seelow_world.obj + seelow_top.png, game data):
  vertices 230609 x 44 bytes at zone 0x250A8B2 (xyz, binormal sign +-1, colour, uv, lightmap uv, packed
  normal/tangent); GfxWorldDraw part at 0x146C92A (vertexCount, -1, 0, layer size 315480, -1, 0, 264836);
  16-bit indices from 0x146CC20 (758 after it; 0 misses in a 19416-index check); surfaces 6973 x 52 bytes
  at 0x2F7FDE8 (vld, firstVertex, u16 vertexCount, u16 triCount, baseIndex [indices relative to
  firstVertex], u32, u32 material (zone pointer), u8 lightmap/probe/primary light/flags, f32 bounds[6]):
  150596 triangles, all in range. The top-down picture shows the farms, roads, railway with its cars,
  fences, trench line and river valley. Box x -2260..7780, y -3114..7872 (sky/stray triangles left out).
- **Seelow's materials and textures are READ** (2026-11-02 night, tools/waw/seelow_materials.py,
  seelow_textures.py, seelow_preview.py -> seelow-work\materials.tsv, textures\*.png (51 images, all
  found in WaW main\iw_*.iwd), world\seelow_textured.png): a surface's material = (((ptr-1)&0x1FFFFFFF) -
  0x195A154)/8 into the world list of 127 (all 6973 surfaces in range, all 127 used). Blends are named by
  layers ("*12n_10n": layer 0 samplers cp/np/sp, layer i c<i>/n<i>/s<i>). Texture table entry 16 bytes
  (hash, sampler letters, state, semantic 2/5/8, 4, image: -1 inline [40-byte struct, name, 16 bytes
  with "DXT1"; a ',' name has its pixels inline] or a pointer to the TABLE SLOT where it was first written).
  The file<->block-4 distance is NOT constant (material structs and images load to other blocks; it grows
  ~0x100 a material): resolved by (1) layer tokens written inline, (2) shared addresses, (3) a curve
  through exact anchors (models: mc/x for wc/x, e.g. 0x5E61..0x8A18; world: 0xBA0F5A at the first table
  .. 0xBA3C53), only inside the anchored range. Checked: brick_red_single's slot holds
  berlin_wall_brick_plain3_c exactly where the curve says. Most-used: wc/seelow_wall_wood_old02 (51774
  tris), concrete_grimey01, trim planks, okinawa tunnel wood, rock_squared_blocks. water/sky: flat colours.
- **Seelow's cells are BUILT** (tools/waw/seelow_cells.py -> seelow-work\cells\cells.pkl: 146725 triangles,
  12 cells, 18 blocks, 27 MB; not yet in the game). cod2cells.py's cell writer is now a function,
  write_cells() (Carentan's cells.pkl byte-identical before/after, sha1 2e1194bd..., C mode + models);
  cod2import.iwi_levels takes WaW's IWi version 6 (same header and levels). Vertices: packed unit vectors
  (CoD4's: (b-127) * (w+192)/32385) checked: 13936 of 13959 faces agree with their normals, |n.t| 0.0007.
  Lightmaps: zone "*lightmapN_primary" (L8 1024x2048 / 1024x1024) and "_secondary" (A8R8G8B8, 512 wide,
  two maps stacked); a surface's lightmap uv addresses ONE half: the lower half is the smooth lit map
  (checked by top-down pictures, seelow-work\lightmaps\probe_*.png), used as CoD3's lightmap colour, alpha
  255. The primary gave noise in every layout tried: not used.
- **Seelow's collision: render triangles are too many** (145757 solid; a .cgb holds 65535 index bytes =
  ~21800 triangles and 65535 points; vertex clustering at 16 units still left 67650). seelow_collision.py
  (render triangles -> convert_terrain) is kept but superseded by:
- **WaW's own clip map is FOUND** (zone 0x3145D50, header words: name ptr 0x80FF3C35 (GfxWorld's), isInUse,
  planeCount 15817 + POINTER to GfxWorld's planes (0x17C9C7A, 20 bytes each), staticModels 1636 (80 bytes,
  from 0x3145EB4), materials 260 (72 bytes: name[64], surface flags, contents = CoD2's d3dbsp layout,
  from 0x3165DF4), brushSides 20987, brushEdges 124203, nodes 3257, leafs 3342, leafbrushNodes 12320,
  leafbrushes 25391, leafsurfaces 0, verts 19897, (66817: vec3s after the verts), (39390: u16s before
  the triangles), triCount 29157 (+ indices, walkable bits pointers), borders 3113, partitions 6437,
  aabbTrees 18953, cmodels 84, brushes 8298). Collision verts at 0x32391ED (19897 x f32[3]), then 66817
  vec3s, then 39390 u16, then the triangles' u16 indices at 0x334A5D1 (0x33372A5 + 2*39390): max index
  19896, 0 degenerate, edges median 94: the picture (seelow-work\world\clip_tris.png) is the ground,
  river bed, railway, roofs and fence posts. Walls are brushes (next).
- **SEELOW IS IN THE GAME** (2026-11-02 ~01:35-01:55, install only, no build; 0.5.727 host unchanged):
  map pack saves\content\00000002\Seelow (MapPack 5, ID 21, folder seelow, "Seelow" / SEELOW /
  SEELOW_HEIGHTS, GERMANY; dlc5:\mp\seelow). One script: tools/waw/seelow_install.sh (make_pack from
  Rouen, merge_zone x3 with seelow-work\cells1\cells.pkl (C2_ONE_CELL=1) and coll\seelow_zn0{1,2,3}.cgb,
  scene.py zone/level/onecell/drop .bgib, atomic install, Rouen's sha1 checked unchanged).
  - Collision = WaW's own clip map (tools/waw/seelow_clipmap.py): terrain 29157 triangles (material per
    triangle from AABB leaf -> partition; partition = u8 tris, u8 borders, 2, first tri, verts, first vert,
    ptr: 20 bytes; AABB = origin, u16 material, u16 children, half size, u32: T4's order), brushes 8298
    (80 bytes; sides at (ptr - 0x2584C4C)/12, 12 bytes: plane ptr (block 0x1528DA4 + 20 k), material);
    left out: 61 portal, 222 bare detail, 292 detail+0x01000000 (mantle), 114 entity brushes (inside a
    sub-model's local box). Split by x into 3 .cgb (15-16k points each), all cgb.check() OK.
  - scene.py (tools/carentan) now takes WaW .ents (waw_entities: ctf spawns _allies -> _allied, flags
    ctf_flag_allies/axis -> allied_flag/axis_flag, info_player_start from the allies' tdm start) and env
    SCENE_CGB ('|' list), SCENE_GROUND ("-450,300" for Seelow), SCENE_REACH (900); Carentan's defaults kept.
  - Tests (logs-backup\20261102-0135..0153 seelow1, seelow2bots, seelow3sun0, seelow4views, seelow5water):
    no crash, 144 fps median, heap 582/608 (Carentan's level); bots roam the whole map (positions y -1000..
    7000), fight (58 kill lines), 29 short stuck stalls. Lightmap alpha (sun) 255 bleached open ground ->
    0 (WaW's map holds the sun); water (water_seelow1, temp_muddy_water, no lightmap: lmi 31) drew bright
    cyan -> murky alpha 170 on e_trans_cullnone; unlit surfaces get a grey lightmap c2lm2 (110, no sun);
    BOX = the world model's bounds (backdrop hills and pine walls back; sky out by material).
    Views for tests: COD3_BENCH_VIEWS="3000,-1800,900,95,22;1305,-1300,-160,80,3;2800,3000,120,250,6;
    4100,6300,250,200,8" COD3_BENCH_VIEW_SECONDS=25 (the yaw does not hold: LOOKAT works around it).
  - **STATIC MODELS ADDED (~02:20-03:00)**: tools/waw/seelow_models.py, merged by seelow_cells.py (SEELOW_MODELS=1
    default) -> seelow-work\cells1m (install default CELLS=cells1m; cells1 = without models). Models by the
    asset list (block 0x277C, 8 bytes an entry, the n-th type-5 entry = the n-th XModel header in the file;
    81 models, sizes checked against the placed bounds); axis = invScaledAxis read by COLUMNS (922 of 1627
    boxes within 8 units, rows 830); material handles and image pointers resolved by the models' file<->block
    curve (72 self-referencing handle arrays, exact) + image bias 0x6D, window 0x80; 3 images guessed by name
    (logpile, tinhathood, hedgehog_wet -> static_berlin_hedgehog_metal_c); black barrels and 10k triangles
    of unresolved handles grey. Destroyed Panzer (9) not read (multi-part). LOD: LOD0, LOD1 where LOD0 > 1000
    tris, the pine at LOD2 (FAR_LOD).
    TWO LIMITS FOUND (tests seelow6models..seelow10final): (1) the game draws a cell's _SPLIT0.._SPLIT31
    only: 34 splits hid the last two (seelow_cells.py now refuses > 32; final 31); (2) e_decal_simple_punchthru
    draws NOTHING for these converted materials, e_decal_simple cuts alpha out cleanly: every Seelow material
    is on e_decal_simple now (trees, pine walls, fences appeared). Final: 41 blocks, 92.7 MB, no crash,
    144 fps, heap 582/608, banks: zn01 took 42 of 49 free (2 left at the end: near the limit).
    Still missing: WaW's sun-shadow map.
  - **MODEL COLLISION ADDED (2026-11-02 ~06:00-06:35)**: seelow_clipmap.py model_collision (default on;
    SEELOW_MODEL_COLLISION=0 off) -> seelow-work\coll_m (install default COLL=coll_m; coll = without).
    XModel header +0x98: collision surfaces ptr (-1), count, contents; the surfaces (44 bytes: tris ptr, count,
    mins, maxs, bone, contents, surface flags) after the model's materials (found by shape), their triangles
    after the array (48 bytes: plane n,d; s xyz,w; t xyz,w: corners where s.p+sw, t.p+tw are 0/1; the cart's
    190 all inside its box read so). Per hull: contents without 1 (canopies) out; under 12 high (480 rubble
    chunks, 163k triangles) out; convex (own planes close into the stored box: 377, e.g. crates) = exact brush;
    the least box-filling non-convex hulls as meshes within SEELOW_MESH_BUDGET 20000 (maples 80, hedgehogs,
    lamps, gates, wagons, boxcars, water tower/wheel); the rest (285) their stored box turned with the model.
    Plane to world: N = n turned by the axes, N.w <= d*scale + N.origin. Probes (tools/carentan/floor.py)
    stop at crate/cart/boxcar/maple tops; WaW's own clip map already had brushes round many props (crates,
    hay, barrels, sandbags within a few units). .cgb: 36-38k points each, check OK.
    Tests seelow11coll / seelow12mesh: no crash, 142-144 fps; bot stalls 29 (no model collision) -> 46 (boxes)
    -> 73 (meshes), the worst at the water mill (the wheel's own mesh crosses the path beside it) and the
    hedgehogs: real obstacles the bots' routes do not know (bot navigation follow-up, not wrong collision).
  - **USER PLAYED SEELOW (2026-11-02 ~06:40, log logs-backup\20261102-064141-userplay-seelow)**: see-through
    walls/roofs/fences/props, walked through things, invisible walls, snagging, stuck; "1 to 1 with WaW";
    "not Rouen's skybox ... World at War Seelow's actual skybox" (trees seen under the map). FIXED (~07:30):
    1. SEE-THROUGH = e_decal_simple does not write depth (CoD3's decal shader): later draws covered nearer
       surfaces. Proof: test seelowhouseout (house front gone from outside, far walls' insides shown), every
       triangle reversed (seelowhouse_flip): front whole but ground gone = drawing order, not facing (all
       150596 triangles' normals agree with WaW's winding). Now Carentan's C-mode set (cod2cells write_cells
       with C2_TEST=C): e_blend (+ flat normal c2flatn / black specular c2flats for now), e_decal_punchthru
       for alpha-tested, e_trans_cullnone for WaW's two-sided (chainlink, barbed, leaves, needles, grass,
       water), e_decal overlay for decals. Test seelowhouse_blend: the half-timbered house solid.
       WaW cull modes read from material state bits (first stateBits word bits 14-15, 0x4000 none, 0x8000
       back: IW3's): only those few are two-sided; walls/terrain/roofs back-culled like ours.
    2. SKY = WaW's (tools/waw/seelow_sky.py, run by seelow_install.sh, SKY=0 keeps Rouen's): WaW's sky is
       model skybox_see1 (header zone 0x36EFDC5, name a pointer: wawimport.read_xmodel(start=)) with
       seelow_1_dome_bkgd + see1_cl_bottom_c + see1_cl_top_c; CoD3's is sky.xemesh (block 17: dome "sky",
       cloud_btm01/top01, effect cdsky) with mp_rouen_bkgd (2048 DXT1, block 0), mp_rouen_cl_sun (block 4),
       mp_rouen_cl_btm (block 7). Both fisheye domes; mappings from both meshes: Rouen uv angle = -az - 75,
       WaW = az + 165, radius by elevation tables; pixels rewritten in place (same size/format/levels, own
       DXT1/DXT5 encoder). Cloud textures swapped (tiling in both).
    3. COLLISION: WaW's player clip is 0x20000, CoD3's 0x10000 (Rouen: 0x10000 alone 96 volumes, 0x20000
       alone never): seelow_clipmap.cod3_contents swaps the two bits; 441 volumes now player clip (were
       monster clip: walked through). The 292 left-out brushes ARE mantle (mount_mantle_over, mantle_on/over).
       Bot test seelow13clip: stalls 73 -> 46, put back 14 -> 6, no crash, 144 fps.
    Not yet: WaW's own normal/specular maps (flat now), the cl_sun dome patches show WaW top clouds.
  - **0.5.728 (built, installed ~08:37)**: the Mosin's CoD2 shot once a shot (900 ms window; it played on
    event 1 and again on event 3 ~280 ms later). Test mosin728: "yours" event 1 only, bots' event 13 once.
    Crosshairs: the user got them back themselves (tests xhair2/11/23/24 showed Thompson, scoped Kar98,
    flamethrower crosshairs on Isle; the Mosin's def equals the scoped Kar98's but names/models/icon).
    Seelow data (same time): props' shared materials resolved (handles point at material RECORDS ~0x24-0x28
    before their texture tables; placed by the tables' own curve, not the handle arrays' (~0x276 off));
    black barrels by name guess (static_peleliu_blackbarrel01_c); 648956 of 654940 prop triangles textured.
    Seelow snagging still open: bot stall spots show ledges of 19-60 units (WaW players MANTLE there; CoD3
    has no mantle: 35 of ~100 stalls), lips of 11-19 round the mill, and model boxes. Next: a mantle/
    step-up assist in the host, or ramps at WaW's own mantle brushes (mount_mantle_over: 283, kept out).
  - **USER PLAY 3 (~09:00): missing train/water tower textures, no grass, empty wheat field, invisible
    things walked into; "nothing left over from any other map"; "replace seelows tanks with cod 3's if
    we have to".** Found and fixed (data, seelow_install.sh / seelow_models.py / wawimport.py / cod2cells.py):
    - ROUEN LEFTOVERS in Seelow's archives (directories listed): every .dcgb (Rouen's dynamic collision:
      its doors' and breakables' brushes as text, 61 in the level, more per zone, at Rouen's places) now
      dropped from all four directories (DROP_DCGB=0 keeps). unzoned.cgb is empty (256 bytes), .ncgb are
      headers. Still Rouen's and kept: light grid (.lgridb), BSP (.btrb), zone boxes, scene records (moved),
      ib/phyb/dtrb/gdbb/dbb/aitbb (not yet shown to matter).
    - ALL DRAWN MODELS: the clip map's list has only the 1636 that collide; GfxWorld's draw list has 7582
      (zone 0x2FD8630, 92 bytes: origin, axis rows, scale, model slot, 32, cull distance), found as the one
      long run of model pointers; its placements equal the clip map's for all 1636 (transform checked).
      Slot 91 (5350 placements, scale 0.7, cull 1400) = the grass clump (header order puts a tread there;
      SLOT_NAMES). LOD: trees LOD 2, grass LOD 1, others LOD 1 over 300 triangles; the rubble chunk
      simplified on a 4-unit grid (340 -> 170): 26 splits, 37 blocks, 92 MB (the full list at the old
      rules: 38 splits, 116 MB). cod2cells.write_cells: fewer submeshes a mesh when four overflow a bank.
    - wawimport.read_xmodel: a surface's rigid records come ALL first, then their collision trees (the
      66-bone Panzer wreck failed); 9 wrecks now drawn, material mc/mtl_panzer4_body (panzeriv_c) by the
      nearest earlier table when the curve has no anchor.
  - (older) Missing yet: Seelow's 1636 static models (trees, carts, sandbags, fences: the clip map's staticModels
    80-byte list: xmodel ptr (0x800027D1-ish: a slot in the zone's asset list), origin, axis, bounds) and
    their collision; WaW's sun-shadow primary lightmap; Rouen's sky (kept, like Carentan); menu listing
    not looked at by eye; the dlmaps location text.
- Seelow next (older plan): collision (clipMap:
  brushes/planes/terrain, find by shape like the vertices); static models; then the Carentan pipeline
  (cells in a harness zone, collision .cgb, scene.py entities from seelow-work\mp_seelow.ents).
