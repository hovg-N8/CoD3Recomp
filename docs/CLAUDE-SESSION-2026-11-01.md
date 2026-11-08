# Claude session 2026-11-01 (handoff)

## Carentan playable (2026-11-01 midday, Claude, user away; no build, source 0.5.690)
Current test set: **cellsM** (byte-0 lightmaps, 2026-11-01 16:50; was cellsL) (`C2_TEST=C C2_ONE_CELL=1 C2_MODELS=1 cod2cells.py ... cellsL`), installed by
`bash tools/carentan/runvariant.sh L <case>` (env BOTS, SECONDS_RUN, SHOTS, EXTRA_ENV="COD3_BIG_MODE=ctf|war|dm",
SCENE=1 default, DROP_ASSETS=1 default). Pack's carentan.cod now = scene-edited copy; pristine in
carentan-work/scene/carentan.cod.orig.
- **Scene** (tools/carentan/scene.py, records kept, values changed; mp_rouen.scnb entity: origin +0x08,
  angles +0x50, +0x14 -> model/brush name, +0x24 -> target, +0x28 -> targetname): every spawn class from
  CoD2 Carentan's (dm, tdm; sided classes from mp_ctf_spawn_allied/_axis; CTF primary = nearest the side's
  flag), CTF flags + stands to CoD2's allied_flag (457 103 -5) / axis_flag (357 2752 -32), player start and
  intermission from CoD2, Rouen's 45 SpeedTree trees (script_origin + speedtree prop) and sbmodel1 sunk 8000;
  zone scenes' instances/groups/effects off (scnb.with_instances_disabled). Archive round trips exact.
  War (3) / dom (5) on open-ground points along allied->axis flag line ((457,103) (432,765) (407,1428)
  (454,2214) (357,2752)), HQ 10 by farthest-point sampling of dm spawns; their trigger_multiples (brush
  models *2..*24, +0x14) given origin = new - old.
- **CTF WORKS**: carentan-ctf-play690 (20261101-121144): bases 357 2752 / 457 103, a bot TOOK flag 0 and
  brought it home; spawns at Carentan's. Pictures: Carentan streets, no Rouen props.
- **WAR WORKS**: first try (carentan-war690) the middle point (an "open ground" snap, 407 1428) was never
  reached (closest bot 90). Objective points now sit on CoD2's own dm spawns under open sky (nearest to
  each place on the allies->axis line, 300 apart): (457,103) (192,1280) (926,1612) (216,2088) (357,2752).
  carentan-war2-690 (20261101-124319, 10 bots, 240 s): flag in play at 926 1612, the GAME counted
  "side 1: 1..3 on it" in 24 samples: a trigger's origin DOES move its brush model (Quake rule), so
  war/dom/HQ zones are in Carentan. (dcgb rewrite not needed for that.)
- **INVISIBLE WALLS (user report) = CoD2's clip brushes around its placed models** (clip_nosight 392,
  clip 199, clip_player 58, clip_nosight_metal 30 ...: in our collision, faithfully) while the 1532
  misc_models were not drawn. FIX: tools/cod2import/cod2models.py (C2_MODELS=1): each placement (65
  models + the intact S&D vehicles) baked as world surfaces in CoD2's 68-byte vertex layout (Quake
  AnglesToAxis, modelscale, top LOD, tangents from uvs), lit by the brightest ground lightmap texel under it
  / on a ring at its radius (incl. sun mask). 205802 triangles; in game 500k triangles a frame at 144 fps.
- **BANKS**: CoD3 has 96 banks of 3 MB for ALL open archives; every block takes one. carentan-models-ctf690
  (cellsJ, 55 blocks, zn01 66 blocks) did not load (nothing drawn). Fixes: normal/spec maps capped at 512
  (dxt_texture(raw, 512): drops CoD2's top level), BLOCK_BUDGET 0x2C0000, merge_zone --drop-assets (the
  source zone's blocks holding only MESH/MAT/TEX/MLTI: zn01 b3,b4,b5,b7,b9, zn02 b2). cellsK/L: 38 blocks,
  all 11 splits bound (carentan-modelsK-ctf690 20261101-122829): bushes, hedges, crates etc. drawn.
- Shader choice now also by CoD2's technique-set name (word 0x38): "alphatest" -> ambient_* (foliage)
  e_trans_cullnone, else e_decal_punchthru; "_blend"/"_add" -> e_trans_cullnone (models' foliage had tech
  word 0x20030000 and drew solid green on e_blend). cellsL has it; not yet looked at in a picture.
- Open: interiors very dark (our lightmaps are BRIGHTER than Rouen's: shaded median 70-95 vs 49, sunlit
  153-190 vs ~100, so CoD3 lights interiors some other way: light grid .ng.lgridb is still Rouen's);
  bots stick on the sloped wooden floor at ~(407..545, 1814) (CoD2 geometry, unstuck loop puts them back
  there); one CTF bot pressed use on the allied flag 168x without a take (no wall between, cause unknown);
  still Rouen's: level archive (gdbb, light grid, AI files, BSP .btrb, zone boxes .zbbb reach y 3262 while
  Carentan reaches 3895, cfgstrb), sky, ambient sounds; CoD2 turrets not placed.

## Webley kill-feed icon (0.5.691-0.5.693, Claude, 2026-11-01 ~13:15)
Tests: bots all on the Webley (temporary bot_kits.txt "1 rifleman webley 0 0"; the user's restored
after each, sha1 149c2a2f... checked; backup in carentan-work/bot_kits.user-backup.txt), mp_isle dm.
- 691 (slot copy only, names left the Colt's): kill feed = Colt silhouette, no white box -> the kill
  feed draws by NAME, not the HUD slot object.
- 692 (COD3_TEXLOOKUP_PREFIX=hud_webley): the game looks hud_webley up through sub_82477E70 ~100
  times a match, from 824409C0 (scope 0) and 82440A18 (scope 0xC), always -> 0 (= the white box).
  For hud_colt45 that call returns 5E5B41E4, the image object MakeWebleyIcon copies.
- 693 (default now): a 0 for hud_webley in sub_82477E70 is answered with MakeWebleyIcon's copy
  (6E040000). Result (logs-backup/20261101-131109-webley-icon693): no white box, but the icon is
  STILL THE COLT'S PICTURE. So the copy is drawn but its texture is not CoD2's: the copy's fetch
  constant (+0x48) points at fresh texels (6E050000, GPU 46050000) that Render::SetTextureImage was
  to fill from hud_webley.png; the Colt shows, so the draw does not use that constant/upload.
  NEXT: find what the kill feed's draw actually fetches (dump the copy's words and the texture the
  renderer binds for that quad), or write the picture as real texels (128x128 DXT5, tiled, endian 1,
  8 levels, 64 KB like hud_colt45 in global.cod) into whatever texture it binds.
  COD3_WEBLEY_COLT_ICON=1 restores the old state fully.
- **SOLVED (0.5.694-0.5.696, afternoon, user back):** two faults.
  1. The image object = the TEX file's description (0x60 bytes each, back to back; +0x14 -> its own
     D3D header at +0x2C, fetch constant at +0x48). The draw binds via +0x14; the copy still pointed
     at the Colt's header. 694 moves self-pointers into the copy -> Webley lines went BLANK.
  2. 695's fetch log: the game cuts a texture address to 29 bits (46050000 -> fetched as 06050000),
     so extended-window texels (AllocateExtended) are read from someone else's memory. 696 puts the
     texels in Guest::AllocatePhysical memory (AC990000 -> GPU 0C990000) -> "render: texture at
     0C990000 given the picture ...hud_webley.png" and CoD2's revolver icon IS in the kill feed
     (logs-backup/20261101-160808-icons696-result, shots 2, 3, 5, 8, 10).
  Lesson for any custom texture the GAME fetches: its texels must be in physical memory.
- Generalised (OwnIcon table, mp_fixes.cpp): hud_webley from hud_colt45, hud_m2_flamethrower from
  hud_thompson (WaW's icon, custom\flamethrower\hud_m2_flamethrower.png, 128x64).
  The flamethrower's names are set like the Webley's (+1860 kill feed, +1440 HUD). In 696 it was a white
  box: its icon never came through sub_824784B8. 697: the kill feed's own name lookup (sub_82477E70,
  OwnIconFor) makes a missing icon from its source in the same scope. COD3_FLAME_THOMPSON_ICON=1
  keeps the Thompson's.
- **697 VERIFIED** (logs-backup/20261101-161520-icons697-result): kill feed shows CoD2's revolver for
  Webley kills and WaW's M2 silhouette for flamethrower kills; both "given the picture" lines logged.

## Carentan: falls under the map, Rouen props/trees, black posters (2026-11-01 ~17:50, Claude; data only, install 0.5.701)
User play (in the norouencoll701 run, logs-backup/user-play-2026-11-01-701, his shots 17:24:49/57, 17:25:01):
"fall under in areas", "trees under the map", "grass where it shouldn't be", "textures/shaders not loading".
- FALLS: 266 of CoD2 Carentan's 13283 collision triangles have 0xFFFFFFFF vertex numbers; cod2collision
  left them out -> ~400 gaps (32-grid) in streets/grass (e.g. 0..512 2048..2560). CoD2 collides by plane +
  edge equations (svec, tvec: corners at (s0,t0),(s0+1,t0),(s0,t0+1), checked on all whole triangles,
  error < 0.002), so cod2collision.corners() rebuilds any missing corner from one known one (264 of 266;
  2 with none are elsewhere). Dedupe now by corner coordinates (by vertex numbers, rebuilt ones collided).
  carentan_all.cgb regenerated (old: carentan_all.before-tri-fix.cgb); gaps 400 -> 28, all at the map's
  outer edge where CoD2 draws no ground. tools/carentan/holes.py (grid scan, "above" -140 skips the giant
  caulk floor #7934 at -256..-152). CoD2's model 1 = a map-wide trigger_hurt at z -165..-149 (falling out
  kills): NOT in our port (Rouen's level trigger_hurts are Rouen's); not needed once the gaps are closed.
- TREES under the map: Rouen's 45 SpeedTrees were only sunk 8000; scene.py now also renames their
  "speedtree" property key to "xpeedtree" (same length, in place) so no tree is made.
- GRASS/props: Rouen's static prop batches mp_rouen_zoneNN.ng.xmbb/.ng.xmpbb (wild flowers = the dry grass,
  hedges, sandbags, burnt cars, Panzer IV husk, Opel Blitz, furniture) were still loaded; merge_zone.py
  leaves them out of the directory (--keep-source-props = old).
- BLACK WALLS: CoD2 phong_blend decals/posters (mtl_caen_decal_camot at 1368 2152 facing -y, biscuits,
  v_poster*, v_sign*) were on e_trans_cullnone (alpha TEST): their clear parts drew black. Now e_decal
  (Rouen's blended decals: wall stains, bullet holes; e_blend's nine params, overlay 1). Set cellsN.
- Test carentan-fixes701 (logs-backup/20261101-174751, 10 bots CTF 240 s): no crash; stuck 41, max strike 4,
  1 unstuck; player never below -140; bots below -140 only in CoD2's cellar (two rooms, floor -144).
  Shots: no dry grass on streets. The CAMO wall itself not yet seen (bench views don't hold yaw).
  INSTALL NOW = `bash tools/carentan/runvariant.sh N <case>` (cellsN + new cgb + no Rouen cgb/props + trees off).

## Carentan: ROUEN'S COLLISION WAS STILL LIVE (fixed 2026-11-01 ~17:25, Claude) - the user was right
- User (twice): "invisible assets or geometry from rouen obscuring paths". Proof: bots' logged traces
  stopped on Rouen's floors to the unit (z 16.0 at 432 853 / 897 571, 16.96 at 607 689; CoD2 there
  -8 / 8 / -3.6). Cause: merge_zone.py only RENAMED Rouen's zone collision (xp_rouen_zoneNN.cgb,
  436/219/201 KB) and the loader walks the archive DIRECTORY by type, whatever the name.
- Fix (data): merge_zone.py leaves the source's .cgb out of the directory (--keep-source-cgb = old).
  Installed with `bash tools/carentan/runvariant.sh M <case>` (cellsM + this).
- A/B, 10 bots CTF 240 s: 0.5.700 + Rouen collision: 172 stuck lines, max strike 25, 23 unstucks,
  46 walls learnt; 0.5.701 without it (logs-backup/20261101-172302-carentan-norouencoll701): 46, 4, 3, 12.
- Tools: tools/carentan/probe.py (CoD2 brushes/tris near a point), ghost_hits.py (bots' hits vs CoD2's
  collision; its body-box model gives false "floor" hits on terrain, e.g. -233 1999: read with care).
  Rouen's zone cgbs extracted to carentan-work/rouen-coll/ (floor.py on them = Rouen's floors).
- Still Rouen's in the pack: .dcgb (small, ~10 entries/zone: likely triggers + brush models; war/dom
  zones use them), .ncgb, .phyb, .dtrb, light grid, level btrb/zbbb/aitbb/gdbb, sky, sounds.
- Bots (0.5.700/701): Climb lifts a stair (18) before the whole rise (cellar stairs 400-552 1816 under
  the ground floor were "no way"); Detour 96 traces; NetProgress "room" test (450/30 s -> strike 4).
  CoD2 mantle windows (e.g. 654-666 504-552, sill 40) are not passable in CoD3 (no mantling): left as is.

## Carentan interiors (started 2026-11-01 ~16:20, Claude)
- carentan-interior697 (logs-backup/20261101-161846-...): camera at 470 1814 165 (upper floor of the
  wooden-floor house: floor z 160, roof ~350; floor.py), COD3_NOSHADERCACHE=1 COD3_DUMPHLSL=scratchpad\hlsl
  (630 ps / 288 vs dumped). NOTE: with the cache off the first ~30 s draw NOTHING (black screen while
  ~900 shaders compile); the user saw that and asked. Not a game fault.
- Interior picture: wallpaper, wood floor, banister and lamp all visible but dim (mean grey 23-26 of 255);
  NOT black, so the lightmap's RGB counts there (sun mask alpha 0). Outdoors view fine.
- Bench views do not turn (yaw stays -89.7: known).
- Next: find which dumped ps draws the world (log the ps hash of draws binding a 512x512 format-6
  texture, i.e. our c2lm*) and read how it scales lightmap.rgb, then compare with CoD2's overbright.
- 698 log: world programs found (ps 94bf726b375d3b06 e_blend-like: lm t2, normal, colour, spec;
  ps 1b8706673819b84e: colour t3 + lm t2 + shadow t1, 4-tap PCF). Decoded 1b87:
  out = colour x (amb x lm.rgb + c3.rgb(sun) x shadow x lm.a x saturate(N.L) + r1.rgb)
  amb = (c1.x + worldNormal.z) x c5.x + c4.y; r1 = the vs's 2 dynamic lights (c13/c14/c16), the vs does
  NOT read vertex colour or the light grid. lm.a IS the sun mask (as assumed). c[i] in a translated
  program is PACKED: file constant = program->constantMap[i] (698's constant log was wrong; 699 fixed).
- **CoD2's own lightmap maths** (its HLSL is in iw_07.iwd materials/shaders/lib/lightmap.hlsl):
  lightmapDiffuse.c = dot(tex(lightmapSamplerC), tex(lightmapWeightSampler, normalmapSample)), out =
  colour x vertexColour x (lightmapDiffuse + sun) + spec. The weights texture $lightmapweights
  (A8R8G8B8 32x32) is built in gfx_d3d_mp_x86_s.dll at 0x1000FE90 (helper 0x1000FD70, basis table at
  file 0x1938C4: (0,0,1), (0,.816,.577), (-.707,-.408,.577), (.707,-.408,.577)): w0 = clamp(1 -
  tilt/0.9553), the rest (1-w0) split between the 2 nearest tilted by azimuth; sum 1. An unbumped
  surface = weights (1,0,0,0) = BYTE 0 of each plane. Data agrees: byte 0 is ~25% above the others
  everywhere (shade 98 vs 65/76/79). Our average was ~20% too dark. cod2cells.py now uses byte 0
  (C2_LM_AVG=1 old) -> cellsM. (Bumped surfaces would need the other 3; CoD3's shader has 1 lm.)
- 699 (logs-backup/20261101-164430-carentan-lmM699): the REAL constants of ps 1b87 (packed c -> file c):
  c1=f16 x 0; c2=f17 far sun position; c3=f18 sun colour 0.99 0.93 0.81; c4=f254 (0, 1, -1, 0.502);
  c5=f255 x 0.2. So amb = 1 + 0.2 x normal.z: walls 1.0, floors 1.2, ceilings 0.8. These are engine
  constants (not a Rouen map setting): CoD3 uses lm.rgb ~1:1 like CoD2 does.
- A/B same room (abL/abM-699, 20261101-1647/1649, last shots): walls 31->45, floor 40->48 (mean grey,
  HUD excluded). CURRENT SET IS NOW cellsM (installed: `bash tools/carentan/runvariant.sh M <case>`).
- r_overbrightBits: only in CoD2 developers' personal cfgs; CoD2's shader has no x2, and its sunny
  byte-0 texels are ~200/255, so no doubling: CoD2 = 1 x byte 0. Interiors are now CoD2's numbers;
  what is left is CoD3's own tone/post (not CoD2's) - not touched.
- Bench yaw is NOT controlled even with COD3_BENCH_CLIENTTURN=1 (spawn yaw kept); A/B views
  only line up by luck. Fixing that would make lighting comparisons exact.

## Scoped Mosin (CoD2) groundwork (Claude, 2026-11-01)
All in the user's CoD2: weapons/mp/mosin_nagant_sniper_mp (damage 90, clip 5, ammo 60, fireTime
0.33, rechamberTime 1, reloadTime 0.6 (per-round loop), adsZoomFov 15, adsTransInTime 0.25,
maxDamageRange 2000, sound weap_nagant_sniper, icon gfx/icons/hud@scoped_nagant.tga), gunModel
xmodel/viewmodel_mosinnagant_cloth (parts viewmodel_mosinnagant_cloth2), hands viewmodel_hands_cloth,
world xmodel/weapon_mosinnagantscoped_cloth, anims xanim/viewmodel_mosincloth_* (idle, fire,
rechamber, intro/loop/end = per-round reload, ads_up/down, pullout(_fast), putaway(_fast), melee),
images weapon_mosin_c / weapon_mosin_cloth_c (+ n/s), hud_scoped_nagant.iwi, sound
weapons/nagant/nagant_sniper.wav. Plan (as the Webley 0.5.652-680): Guns[] entry "mosin_scoped" as a
copy of CoD3's kar98_sniper (scope, rechamber, per-round reload), MosinFields from the CoD2 file,
custom\mosin\ data via cod2import.py (model --pose viewmodel_mosincloth_idle, images, sound), view
mesh swap like MpFixesWebleyView, world model like WebleyWorldModel, class_editor + settings clamps,
then the reload/rechamber via retarget.py. Several builds.

## Seelow (World at War) groundwork (2026-11-01 midday, Claude)
- No WaW mod tools on this PC (no raw/, map_source/, .d3dbsp): Seelow exists only as
  zone/english/mp_seelow.ff (+ _load, localized_). Decompressed zone (66053540 bytes) saved in
  D:\Call of Duty 3 (USA, Europe)\seelow-work\mp_seelow.zone (game data, never in git).
- Its entity string extracted (seelow-work\mp_seelow.ents): node_pathnode 1668, script_struct 258,
  mp_twar_spawn 128 (WaW War), mp_tdm_spawn 112, mp_dm_spawn 112, mp_dom_spawn 65, trigger_multiple 54,
  mp_ctf_spawn_allies/axis 22/22 (+16 start each), mp_sd 16/16, sab, script_brushmodel 18,
  script_vehicle 10 ... So spawns/objectives map onto CoD3's classes like Carentan's did.
- The world itself: "maps/mp/mp_seelow.d3dbsp" occurs ONCE in the zone (16818632): clipMap,
  GfxWorld, ComWorld, GameWorldMp, MapEnts share names by pointer reuse. wawimport.py only finds
  XModels by name search; reading the world needs a real WaW (T4) zone deserializer for clipMap_t
  and GfxWorld (vertices: xyz, binormal sign, colour, uv, lightmap uv, packed normal/tangent;
  surfaces: material, lightmap, vertex/index ranges; lightmaps; static model placements). The
  asset-list read in wawimport's header notes gives type 0xFF000000 for all 711 (offset wrong);
  the list is at zone+0x27AD (found by its 711 (type, -1) pairs): types 5 x207 (materials), 7 x298
  (images: in the ff), 9 x89 + 26 x89 (sounds), 32 x18, 3 x2, 6 x2, and single 1, 12, 13, 15, 17, 24
  (the world assets among these). Asset data starts after the list (0x27AD + 8 x 711). The struct layouts are public (e.g.
  OpenAssetTools' T4 headers). Then the same pipeline as Carentan: world -> cod2cells-style
  cells (own cell 001 in a harness zone), collision -> cgb (brushes/planes like CoD2's),
  entities -> scene.py, static models -> cod2models-style baking. Multi-session work.

## Codex continuation (2026-11-01): texture binding investigation in progress
- Read CLAUDE.md, PICK-ME-UP top and END OF SESSION HANDOFF in order. Outer source root is the build root; nested CoD3Recomp has the git metadata. No game data committed.
- First requested 689 test completed: tools/carentan/runvariant.sh "" carentan-texbind, EXTRA_ENV=COD3_TEXLOOKUP=1. User warned beforehand: Carentan geometry/collision inside a Rouen scratch harness, Rouen props remain. Rouen SHA1 check unchanged. run_test restored INI; exit 0, 117 s total. Logs/screenshots: install/logs-backup/20261101-103433-carentan-texbind-result; previous logs backed up in matching -before.
- Converted texture lookups still return zero; no c2 bind lines. IMPORTANT: 689 texhandler diagnostic arguments were wrong. Disassembly proves r3 = block descriptor (regions at +8/+12, types +16), r4 = type entry; 689 read r3+0x10 as a texture name. Thus blank handler names do NOT prove converted handlers never run.
- Offline comparisons: original and converted TEX records both tag 0x00584554/version 3/parts 2/image align 4/physical mapping 0x01001000; all appended KAPF blocks have the real zone flag 0x82 and 3 MB banks. Across shipped MP archives 304 TEX resources use type index 0, so type index zero itself is valid. New zone index contains all 119 c2 textures (222 TEXTURE names total vs Rouen's 103), with DISCTEX/DISCTEXSIZE empty in both. Screenshot confirms white world surfaces while Rouen harness props retain textures.
- 690 requested: diagnostic only, correct handler name from entry's name pointer, log resolved image pointer after original handler. Not a texture fix. Next: wait build-status 690, warn before launch, repeat diagnostic and inspect c2 handler/image/bind output. Continue with evidence, no format guesses.
- 690 built OK (3.5 min), diagnostic test carentan-texbind690 ended exit 0 (108 s), INI restored. Result install/logs-backup/20261101-104041-carentan-texbind690-result. Corrected output proves c2 TEX handlers DO run, image pointers nonzero, bind scope 0xD matches lookup scope. Crucial order: first failed c2 lookup log line 7017; first c2 TEX bind line 7260. Handler registration is too late for MAT import resolution.
- Found shipped directory invariant: 41/41 MP zn*.cod archives with both .xetex and .xecell list ALL textures before ALL cells (not just their block layout). Our directory listed converted cells first, textures last (entry 276 vs first cell 122). Added tools/carentan/check_texture_order.py; fails on old generated archive and passes on real Rouen + corrected output.
- tools/kapf/merge_zone.py now partitions directory: index first, texture entries, remaining files. Generated test/zn01-dirorder.cod; proved directory record multiset unchanged and all 31 blocks byte-identical to prior test/zn01.cod. Installed only zn01 via copied temp+os.replace. NEXT run 690 diagnostic to prove corrected directory binds textures before MAT import resolution and inspect screenshot. This is a Rouen scratch harness, not final map.
- Directory-order runtime proof: carentan-dirorder690 logged 200/200 nonzero c2 lookups, zero misses (previous test 200/200 zero). Screenshot 104339 shows textured terrain rather than white. Simple e_decal_simple materials still do not draw solid buildings correctly; next use existing cellsB/e_blend control, then actual CoD2 normal/spec textures. Both sampled CoD2 *-gggr and CoD3 rock$br_02_wall01_n are DXT5 with red=blue and independent alpha/green near 128; investigate shader packing before any transform.
- e_blend control: carentan-eblend690 (cellsB, corrected directory) exit 0, 97 s, INI restored; result logs-backup/20261101-104449-carentan-eblend690-result. Screenshots still at Rouen's spawn; next camera uses COD3_BENCH_VIEWS in Carentan's actual centre. Floor at (0,0) below1000 =46 from converted collision.
- Added C2_TEST=C conversion mode: actual CoD2 normal/spec maps, source $identitynormalmap/$black when material omits the slot, e_blend/e_decal_punchthru/e_trans_cullnone by source technique. Still stock reference normal_scale=0.5/spec_power=300 pending shader numeric parity; not final 1:1 lighting claim. Output c2/cellsC/cells.pkl: 34 blocks,58.5MB,258 textures. First generation caught absent specular slots; handled with source black. No game install touched by conversion.
- All138 source normal/spec images and1333 mip levels recovered byte-for-byte from converted GPU data (xetex.level_blocks/swap_pairs vs iwi_levels). No speculative channel transform: gggr data preserved. tools/carentan/check_material_textures.py failed on cellsB's Rouen normal map; run on cellsC before launch.
- (Claude, after Codex's session ran out) Last run was carentan-ownmaps690 (cellsC, 10:48; result logs-backup/20261101-104821-carentan-ownmaps690-result): exit clean, 1m36s, 144 fps median, no crash. 200/200 c2 lookups resolve (c2lm*, c2n_* included), 0 misses. check_material_textures.py PASS on cellsC (212 normal + 212 spec imports bundled); check_texture_order.py PASS on the installed zn01 (361 textures before 27 cells). Screenshots are still at Rouen's spawn (Rouen trees/props), so the CoD2 normal/spec look is NOT judged yet. The Carentan pack's zn01-03 currently hold the cellsC test zones (not the placeholder links); Rouen's sha1s unchanged. Next: same cellsC run with the camera at Carentan's centre (COD3_BENCH_VIEWS), warn the user first. The scene/spawn agents' results were not written down before the cut-off.
- (Claude) carentan-centre690: cellsC, camera inside Carentan via COD3_BENCH_VIEWS (spots from CoD2's mp_dm/tdm spawns: x -1273..1959, y -1280..3488; coords are 1:1). Command: SECONDS_RUN=70 SHOTS="COD3_SHOTS=8 20 32 44" EXTRA_ENV="COD3_TEXLOOKUP=1 COD3_BENCH_VIEW_SECONDS=12 COD3_BENCH_VIEWS=1000,1216,40,0,5;1404,2654,40,45,5;343,-1600,1000,90,30;1790,800,40,135,5" bash tools/carentan/runvariant.sh C carentan-centre690 (runvariant.sh now takes SECONDS_RUN and SHOTS). Exit 0, 107 s, 0 c2 lookup misses, Rouen unchanged. Result logs-backup/20261101-105613-carentan-centre690-result (bench pictures are the *-2.png). Seen: CoD2's stone walls draw with their own texture and bump (good); BUT large areas still draw flat white/pale lilac: most of the ground from above, plaster/door reveals, some grass edges. Dark sliver triangles fan out around one ruined building (view 1). Rouen's props/trees float over everything (scene still Rouen's, expected). setviewpos doesn't hold yaw (known, 0.5.411). Next: find which c2 materials/techniques the white surfaces use (likely terrain/multi-layer blends or lightmap-only), and the sliver geometry.
- (Claude) WHY THE GROUND IS WHITE (offline, no launch): not lightmaps (c2lm0 mean 65 vs Rouen's lm 49-67; white areas in shots are flat ~145 grey, not clipped). CoD2 material word 0x38 = technique-set name. 8 Carentan materials are "effect_multiply": the v_dec_darkstain0x ground decals (13060+6702+2868+2646+... tris, no lightmap), near-white DXT1 (mean ~205) meant to MULTIPLY the ground. We drew them via e_trans_cullnone as opaque near-white sheets = the white ground with dark patches. CoD3 MP has no multiply world shader (shader census of all MP MATs: e_blend 3556, e_blend_blender 922, e_blend2 289, e_decal 229, ... e_decal_simple 103); Rouen's alpha-blended scorch marks use e_decal_simple + DXT5. Fix in cod2cells.py (C mode): effect_multiply -> texture c2x_* = black, alpha 255 - brightness (8888, no mips; exact for grey since dst*c = lerp(dst, 0, 1-c)), shader e_decal_simple, overlay 1. Output c2/cellsD (36 blocks, 266 textures; 23 MATs e_trans_cullnone -> e_decal_simple, all else same as cellsC); check_material_textures PASS; c2x textures read back black with alpha 0..253. NOT TESTED IN GAME YET: bash tools/carentan/runvariant.sh D carentan-multiply690 with the centre690 env. The sky is already left out (sky*/surf&4, 71 tris), not a cause. Still open after this: 4 "phong_add" light-stain decals (additive, also on e_trans_cullnone), the phong_blend* layers (v_grass_blend, v_mud_blend) fade by vertex alpha but our vertices are forced FFFFFFFF (opaque layers), plaster door reveals flat pink (unchecked), sliver triangles near one ruin.
- (Claude) carentan-multiply690 (cellsD, same views/env as centre690): exit 0, 107 s, 0 c2 misses, 8 c2x lookups, texture order PASS. Result logs-backup/20261101-110914-carentan-multiply690-result. THE GROUND IS STILL WHITE: the stain decals were not the main cause (the change stays: it is CoD2's real blend for those 8 materials). New evidence: cellsB2/eblend690 (all e_blend, Rouen's normal/spec maps, our diffuse + lightmaps) shows GREEN textured grass; every cellsC/D run (CoD2's own normal/spec, and tech 4 -> e_trans_cullnone, tech 2 -> e_decal_punchthru) shows white. So it is one of those two C-mode differences. CoD2 maps: normals DXT5 "gggr" (mean 129 all channels, same stats as Rouen's _n DXT5); v_grass spec RGB 7 / A 243, plaster spec 89, Rouen _s DXT1 RGB 31-41. Next (needs launches, ask first): cellsE = C but every material e_blend (as B) -> separates the shaders from the maps; then if needed cellsF = C with Rouen's _n/_s on v_grass only -> normal vs spec. Caveat: B's and C's pictures are not the same camera spot.
- (Claude) carentan-alleblend690 (cellsE = C2_TEST=C C2_SHADERS=eblend: CoD2's own normal/spec, all 287 MATs e_blend with colour maps, as B): exit 0, 107 s, 0 c2 misses, texture order PASS. Result logs-backup/20261101-111907-carentan-alleblend690-result. GROUND STILL WHITE at every view (eye level at 1790,800 white under Rouen's grass sprites, as C/D). So C's shader choice is NOT the cause; the remaining C-vs-B difference is CoD2's own normal/spec maps (B = Rouen's rock _n/_s on everything: green grass). Caveats: B's picture was at a different spot (Rouen's spawn); the views' facing differs run to run (setviewpos places but doesn't turn; COD3_BENCH_CLIENTTURN=1 exists, untried here), so most pairs aren't the same scene. Next: (1) cellsB at these 4 spots as a same-spot baseline; (2) cellsF = CoD2 normals + Rouen's spec on all (green -> spec maps; white -> normal maps).
- (Claude) carentan-baselineB690 (cellsB: Rouen's _n/_s, all e_blend) and carentan-specF690 (cellsF = C2_TEST=C C2_SHADERS=eblend C2_SPEC=rouen: CoD2 normals, Rouen's spec), same 4 spots, COD3_BENCH_CLIENTTURN=1 (did NOT hold the facing either: B all yaw ~-170, F drifting). Results logs-backup/20261101-112805-carentan-baselineB690-result, 20261101-113011-carentan-specF690-result; both exit 0, 0 misses. BOTH WHITE at the same spots as C/D/E (eye level at 1790,800 white under grass sprites; view 1 white). The old green B picture was just another place. So normal maps, spec maps and shader choice are all cleared.
- (Claude) ROOT CAUSE CANDIDATE, lightmap ALPHA: Rouen's lm* (DXT5) alpha is the SUN VISIBILITY mask (open ground white with tree-shadow blobs, interiors 0; means 0-30). Ours was 255 on every texel = full sun everywhere. CoD2 keeps the sun map separately: lump 1's 4th part per lightmap, 1024x1024 bytes, same lightmap uvs (lightmap.hlsl: lightmapSamplerSun * dynamic shadow * Phong sun; lightmapDiffuse = dot(coeffs R/G/B planes, lightmapWeightSampler(normal sample)); the weight texture is made by the engine, no file). Ground texels: coefficients ~175-243, sun 255; shaded texels sun 0. cod2cells.lightmap_texture now puts the sun map (2x2 averaged) in alpha (default, all modes). cellsG = C2_TEST=C (= cellsD + sun alpha): c2lm0/1 alpha means 51/31, picture matches Rouen's pattern; check_material_textures PASS. NOT TESTED IN GAME. Still unknown: the RGB scale (our ground RGB ~230 vs Rouen's ~140; plain average of the 4 coefficients because the weights texture isn't a file); if the open ground stays white after G, that is next (compare shaded vs sunlit ground in G's pictures).
- (Claude) carentan-sunmask690 (cellsG, + 5th view 1790,800,700 pitch 85): exit 0, 112 s, 0 misses. Result logs-backup/20261101-113704-carentan-sunmask690-result. Facing this run: every view yaw -28 pitch 0 (pitch not held either). STILL WHITE. Full-resolution crops: the "ground" is perfectly flat, textureless, exactly the horizon-haze colour, Rouen's grass/trees standing on it = NOTHING DRAWN THERE (fog/void), not a white material.
- (Claude) WHY NOTHING IS DRAWN THERE (strong lead, offline): CoD2's drawn surfaces under view 4 (1790,800) are Carentan's CHURCH floor (v_floor_church01, z -24) yet the picture shows open air: the church isn't drawn. Our 4x3 grid (over the world model incl. sky box: -8512..10816 x -6864..11648) puts nearly all of Carentan in cell 5 (77298 tris) and cell 6 (33103); views 2 and 4 and the east of view 1 are cell 6; walls seen in D's view 1 are cell 5. All splits are asked for and found (asset log: 005_SPLIT0-2, 006_SPLIT0-1, then the missing SPLIT3/SPLIT2). Rouen's own cells (game/mp/rouen): 000 (zn03, 20209 tris, 4330x4775 units), 001 (zn01, 45303 tris, 5829x7152, the centre), 007 (zn02, 28319 tris) are the big OUTDOOR cells; 002-006 and 008-011 are tiny one-storey rooms (200-650 units, z 24..152). So the town landed in two of Rouen's room slots, which Rouen's own visibility data (still Rouen's) presumably draws only from nearby. sub_82441A58 (called from sub_82478A30/82478B30) is the per-cell load/unload, not the per-frame visibility; the per-frame cell choice is not traced yet (cell table global at lis -32067 / -14188, many readers). All three zones are opened (zn02/zn03 too).
- (Claude) cellsH = C2_TEST=C C2_ONE_CELL=1 (new option in cod2cells.py: every triangle in cell 001, Rouen's central outdoor cell in zn01): 1 cell, 5 splits (max 32), 26 blocks, 266 textures, 108 MATs; check_material_textures PASS. runvariant.sh H needs no change (cells 0, 2-11 simply absent; Rouen's cells stay hidden). NOT TESTED IN GAME. If Carentan then draws everywhere: the white was cell visibility; the real level's own zones/cells need their own visibility data (or all-outdoor cells) later.
- (Claude) **carentan-onecell690 (cellsH, all of Carentan in Rouen's outdoor cell 001): CARENTAN DRAWS.** Result logs-backup/20261101-114640-carentan-onecell690-result; exit 0, 112 s, 144 fps median, 0 c2 misses; asset log: mp_rouen_001_SPLIT0-4 bound, SPLIT5 asked (missing, as designed). View 1 (1000,1216): Carentan's street fully textured and lit (stone houses, roofs, the "cafes Jeanne" sign, iron fences, poles and wires, ground). View 3 (high, south): green fields and a road to the map's edge. View 4: inside the church, its walls drawn (dark). So THE WHITE WAS CELL VISIBILITY (the town in Rouen's room slots 005/006), not materials/maps/lightmaps. Facing this run: yaw -45 pitch 0 for views 1-4.
- (Claude) NEW BUG from the 0.5.690-era stain change: view 2's right half is a flat (3,2,2) black, stddev 0. Ray cast from the camera (1404,2654,40) at yaw -45..-85 hits v_dec_darkstain08 (lm 31) on a wall 42-59 units away: our multiply decal (black, alpha = 1 - brightness, 8888) draws SOLID black. Our MAT matches the game's 63 e_decal_simple MATs exactly (overlay 1, position_light0 0, technique 0, same layout; e.g. Eder Dam's decal$oil_stain_c), so e_decal_simple does not soft-blend by texture alpha (opaque or alpha-tested). Fix options: leave effect_multiply decals out for now (they only darken), or find a CoD3 FX that really alpha-blends (decode FX render states). Also open: interiors dark (church (8,7,6) with faint texture: our plain average of CoD2's 4 lightmap coefficients may be too dark; CoD2's weight texture is engine-made), Rouen's scene props/trees still present, view facing still not held (bench views).
- (Claude) cod2cells.py: effect_multiply decals are now LEFT OUT in C mode (info["skip"]; the c2x black-alpha texture code removed); 26729 triangles. cellsI = C2_TEST=C C2_ONE_CELL=1: 1 cell, 4 splits, 23 blocks, 258 textures; e_blend 105, e_trans_cullnone 28, e_decal_punchthru 3, no e_decal_simple; check_material_textures PASS. Test carentan-nostains690 (logs-backup/20261101-115557-carentan-nostains690-result): exit 0, 112 s, 0 misses, texture order 361/18; facing yaw -179.5 pitch 0 at every view.
- (Claude) VERIFIED BY RAY GRIDS (eye at z = origin + 60, i.e. 100 for the bench's z 40; HFOV ~80; LEFT = +yaw, the standard convention, confirmed: it predicts H's view 2 exactly, distant houses left, stain sheet right): H's view-2 black = v_dec_darkstain08 (42-84 units). In I's view 2 the stains would have covered the centre/right above the door; the picture shows textured stone (brightened 6x: stone and the arched wooden door v_shutter_01 fully textured; the scene is just dim, lightmap ~100/255, sun 0). THE STAIN BLACK IS GONE.
- (Claude) STILL OPEN: a flat black band at the far left of I's view 2 (screen x 69-331 of 1904, stepped outline, stddev ~0). CoD2's geometry there = the same v_stonewall03 (lm0 ~97-100, sun 0) at 49-57 units with nothing in front; no Rouen scene instance/entity within ~200 units (instance position = floats 36-38 of the 240-byte record). Not explained. Other open: dim/dark interiors, Rouen's scene props, bench facing.
- Parallel investigations discovered spawns actually in .scnb (spawn_war_axis etc.), NOT .gdbb. Scene agent owns exact roundtrip serializer; spawn agent owns mapping source spawn entities; both no launches/install writes.

## END OF SESSION HANDOFF (2026-11-01 ~10:30) - PICK UP HERE
State: source and install **0.5.689** (built OK; 0.5.688/689 add only an off-by-default
diagnostic, COD3_TEXLOOKUP=1). Nothing committed to git. No friend zip (on hold). The
"Carentan" pack (saves\content\00000002\Carentan) is back to its placeholder (hard links
to Rouen's zones; Rouen's zone sha1s checked unchanged). Game data of this work (not in git):
`D:\Call of Duty 3 (USA, Europe)\carentan-work\` (c2\cells*\cells.pkl = converted blocks,
c2\carentan_all.cgb = whole collision, c2\mp_carentan.d3dbsp = CoD2's map, test1\ = empty
zone cgbs + Rouen's sha1s). Helper scripts: tools/carentan/ (runvariant.sh = merge + install +
run one test; ppcfunc.py = print a recompiled function by address; floor.py; iibs_probe.py).
Re-checked at the final wrap-up: the pack's zn01-03 are hard links again (4 links each, Rouen's
sizes), Rouen's zone sha1s match test1\rouen-sha1-before.txt, carentan-work\ and tools\carentan\
are in place and point at carentan-work (not the old session scratchpad, which is gone).
0.5.689 is built but its COD3_TEXLOOKUP test has NOT been run yet: that is the first thing to do.

The user's goal: CoD2's Carentan ported 1:1 as its OWN map (never a Rouen reskin; Rouen
copies only as a scratch harness, say so). WARN THE USER BEFORE EVERY GAME LAUNCH.

Where it stands (details in "Carentan step 2" below):
- Writers verified against the game's own files: .cgb collision (55/55), APKF blocks
  (504/504), KAPF archives (69/81), zone index .xenon.ib (46/46), cell meshes (929/929),
  textures (6809/6809 2D), materials (1190 world). Carentan's collision converts and is
  proven in game; its drawn level converts (cod2cells.py) and IS DRAWN IN GAME (tests
  cells3/cells4/A2 in logs-backup 20261101-1004..1010): geometry, cells, materials and the
  e_blend shader work (A2 with Rouen's textures looks right).
- THE ONE OPEN BUG: our own textures (c2t_*, c2lm*, c2white) come out white. 0.5.688 showed
  every material import lookup of them misses (sub_82477E70 -> sub_82474FD8 category 0, in
  zone scopes 0xD/7/8). A texture is bound to the index only by its TEX handler
  (sub_82476F38: makes the GPU texture via sub_82140880("image")/sub_82140490, and only if
  that succeeds binds the name with sub_824750C0 category 0 in scope sub_8246F1A0()).
  0.5.689 logs that handler ("texhandler:") and the binds ("texbind:"). NEXT STEP:
    bash tools/carentan/runvariant.sh "" carentan-texbind   with  EXTRA_ENV="COD3_TEXLOOKUP=1"
  (warn the user first), then in the result's CoD3MP.log: grep texhandler/texbind/texlookup.
  If the handler never runs for c2*: the textures' blocks are not processed as TEX (check
  block.py's TEX type record, the directory entries, where Rouen keeps zone TEX). If it runs
  but no bind: sub_82140490 returns 0 (our resource not found in the pack's 'image' region
  lists). If it binds in another scope than the lookups use: load order / scope (put the
  textures in the base archive, as Rouen keeps 312 of its textures there).
- After textures: the real look (CoD2 normal/spec maps: decode their "gggr" packing vs
  CoD3's), then the scene (.scnb, so Rouen's props go), spawns (.gdbb), light grid, the
  other zone files, our own carentan.cod + zones (no Rouen left), its FLI zone list, bots.

State before this session's work: source and install **0.5.687**, built OK, tested (no crash).
Friend zip NOT made (on hold per CLAUDE.md); MAKE-FRIEND-UPDATE.ps1 is ready for it.
The user's bot_kits.txt was restored byte for byte after a test (sha1 149c2a2f...).
Details of every step: docs/CLAUDE-SESSION-2026-10-31.md, sections from "Carentan writers,
step 1" to "step 4c", "The flamethrower (0.5.681...)" and "The flamethrower in game".

## What the user asked for (in order)
1. Webley exactly like CoD2's: done (0.5.669-0.5.680). Open: its own kill-feed icon (a white
   box came up; Colt's icon kept: COD3_WEBLEY_OWN_ICON=1 to work on it).
2. Carentan from CoD2, 1:1, as its own playable map: **in progress** (formats being decoded;
   the user's last choice: "Carentan collision next").
3. World at War's flamethrower (model, animations, sounds, everything): **done as a first
   version** (0.5.681-0.5.687). Open: WaW's own arm animations.
4. Seelow (WaW map): only after the three above. Not started (needs Carentan's writers).

## Flamethrower (0.5.681-0.5.687) - what exists
- Class weapon "M2 FLAMETHROWER": primary 23 / sidearm 25 (settings.cpp clamps raised,
  class_editor.cpp lists, mp_fixes.cpp CustomWeapons). Guns[] "flamethrower", a copy of the
  Thompson; FlameFields (WaW numbers: 25 a shot at 10/s, full to 250 units, none past 450,
  maxRange 500, 100-round tank + 200).
- Data: custom\flamethrower\ (install) made by tools/waw/make_flamethrower.py from the user's
  WaW (D:/Steam/steamapps/common/Call of Duty World at War): viewmodel/weapon meshes, colour
  maps, slots.txt / world_slots.txt, flame_fire.png / flame_lick.png, kill icon, 4 sounds.
  tools/waw/wawimport.py reads WaW fastfiles (XModel, inline WAVs with MS-ADPCM decode, IWi).
- View: MpFixesFlameView swaps the Thompson body (w_tommy.w_usSMG_VM) for the M2, hides
  mag/bullet/charging handle. Two-point fit (scale 0.532, 5.7 deg nose up; COD3_FLAME_FIT).
  The Thompson's draws take the COLOUR MAP IN SLOT 4 (the Colt's: 5).
- World model: FlameWorldModel (w_usa_flamethrower_WORLD looked up as w_ussmg_world; no
  turn/scale). Seen in a posed bot's hands (COD3_EXEC="bot_pose flamethrower 150 90").
- Flame: CoD3Host/flamethrower.cpp, an overlay (Overlay::SetWeaponHud, new; Overlay::
  ImageTexture, new): chunks simulated in camera space, projected at 65 deg, WaW's sprite
  frames. COD3_FLAME_MUZZLE, COD3_FLAME_TEST=1. Sounds via Audio::PlayClip on the shot events
  ("flamethrower" text in the CampaignEventName path).
- Tests: COD3_SHOT_OVERLAY=1 puts the overlay into screenshots (fixed for HDR in 0.5.684).
  tools/cod2import/run_test.py <case> --primary 23 --env COD3_FLAME_TEST=1 COD3_SHOT_OVERLAY=1
  "COD3_FLAME_SHOTS=20 25". Results: logs-backup/20261101-06xx..07xx-flame*-result.
- Open: WaW arm animations (XAnimParts found in the zone, e.g. viewmodel_flamethrower_idle at
  0x280b6f0 in common_mp's zone; framerate 30 at +0x28; the compressed data layout is
  unknown, not guessed), its kill icon, others' flames not drawn (heard only), overheating.

## Carentan - where it stands
Tools (tools/kapf, all Python, read the user's own game files):
- apkf.py: an archive block (APKF) fully decoded and verified on all 81 MP archives / 504
  blocks: regions (image/physical), type records, every file's CPU and GPU parts, the tail
  (fix-ups and imports), pointers (top 6 bits base: 0 image, 0x3F name pool; word offsets).
- extract_plain.py: every file of archives out (CPU part, .gpu for GPU part).
- xetex.py: textures to PNG (Xenos fetch constant at +0x48, untiling, DXT via Pillow).
- xemesh.py: meshes to OBJ (submeshes, vertex declarations, 16-bit index lists). Rouen
  zone 1's world cells render right (world space).
- cgb.py (NEW, this session): the zone collision (.cgb) FULLY decoded; parses and checks all
  49 .cgb files of the MP maps: OK. Format in its docstring:
  - header counts; volumes (36 bytes) = plain boxes, then brushes, then meshes;
  - brushes = box + angled planes (start/count into planes);
  - brush corner sets (origin s16 + 4-byte packed corners) for the first brushes only;
  - meshes = u8 triangle indices + 4-byte packed points from an origin;
  - packed point: LE 24-bit w + byte b: x=(w&0x7FF)/4, y=((w>>11)&0x7FF)/4, z=b+(w>>22)/4;
  - a 4-wide box tree at the end (s16 -(floor(min)-1), floor(max)+1 per axis + child byte
    offset or volume index; header 0xB8 roots, 0xBC levels, 0xC0 bytes).
  Open: why some brushes have no corner set (NOT a size rule: tested, 4038 mismatches);
  what the volume flag words mean (contents/surface: copy Rouen's common values); the header
  radius formula (Rouen: 5001.1 x sqrt 3); words at 0x40-0x80 are stale pointers.
- Other level files seen but not decoded: .scnb (scene: 0xF0-byte instance records with a
  4x4 transform, then a model/effect name table and per-model groups; most words are stale
  exporter pointers), .gdbb/.dbb/.cfgstrb (game database: spawns), .lgridb (lighting grid),
  .btrb (BSPT), .zbbb, .xmbb/.xmpbb, .aitbb, .dcgb/.ncgb (other collision), .xenon.ib (IIBS).
- CoD2's side is already read (tools/cod2import/cod2map.py: geometry, entities, spawns).
- The Carentan pack (folder "carentan", saves\content\00000002\Carentan) loads, but is
  Rouen's content renamed (FLI writer, tools/kapf/fli.py).

## Next steps for Carentan (in order)
1. A .cgb WRITER in cgb.py (round-trip test: parse Rouen zn01, write it back, compare all
   but the stale words; then a hand-made tiny file like mp_forest_zone13.cgb).
2. CoD2 brushes -> volumes (axial sides to the box, the rest as planes), corners by plane
   intersection; CoD2 terrain triangles as thin brushes or as meshes (format known now).
3. Then the scene (.scnb) with no instances, the world cells (MESH/MAT/TEX/MLTI writer:
   apkf.py needs a writer too), spawns in the game database (.gdbb), lighting (flat?).
4. Test by swapping one Rouen zone's collision in a copy of the pack (blocks are stored raw,
   so same-size in-place edits are easy; other sizes need the block rebuilt).

## Carentan collision: writer and CoD2 conversion DONE (2026-11-01, afternoon, no build)
The user: Carentan must be CoD2's actual map as its own map (memory: carentan-real-port-not-
reskin); a Rouen copy is only ever a scratch harness to prove a file loads.
- tools/kapf/cgb.py: write() (all 55 MP .cgb files rewrite byte for byte), build() (boxes,
  brushes, meshes -> a file), build_tree(), corner sets, `--roundtrip`. Rules found and
  checked on all 49 zone files (docstring): radius = max(|vol centre - centre| + vol radius)
  + 1; tree leaf box = C casts of f32 min-1 / max+1 (0 mismatches), packed bottom-up,
  levels = least with ceil(n/4^(L-1)) <= 16, by level, runs padded to 4; corner set iff
  flags0 & 0x201 (exact: 31396 = 19671 + 11725), corners truncated toward zero to quarters,
  origin floor(min), padded to 4 with the last; no axial planes in brushes; mesh rules.
  The exporter's own grouping order of the tree is not reproduced (the game follows the
  offsets; our trees pass the check and are the same size on all files).
- Flags: both games' infoParms tables read from their executables (CoD2MP_s.exe; CoD3
  codmp_xenonf.xex at 0x825D9B58, unpacked by tools/kapf/xex.py). Same bits except
  surface 0x40000 (CoD2 nocastshadow / CoD3 castshadow), contents 0x40 (canshootclip /
  physicsclip), 0x01000000 (CoD2 mantle / CoD3 origin), CoD2's compiler bit 0x20000000.
- tools/cod2import/cod2collision.py: CoD2 IBSP collision (lumps after riicchhaarrd/bsp.c:
  4 planes, 5 sides, 6 brushes, 29-34 collision verts/edges/tris/borders/partitions/aabb,
  35 models) -> .cgb. Carentan: 4318 boxes, 4021 brushes (3022 with corners; 96 too big
  for corner packing split into 603 exact slices), 4040 terrain meshes (14943 triangles,
  688 halved to fit), left out: 61 mantle, 168 portal/hint, entity brushes (models 1-8).
  VERIFIED against CoD2's own data: 200000 of 200000 random points solid/empty alike,
  flags alike; all 12819 terrain triangles on a mesh. Output (scratch, game data):
  scratchpad c2/carentan_all.cgb, 1.27 MB, check OK.
  usage: python -I cod2collision.py "C:/Program Files (x86)/Activision/Call of Duty 2/main"
  mp_carentan <a real zone .cgb as header template> <out.cgb> [zone min xyz max xyz]
- tools/kapf/apkf.py rebuild(): a block laid out again with plain files replaced (any size),
  pointers relocated through the fix-ups; all 504 MP blocks rebuild byte for byte; a grown
  .cgb keeps all 249 fix-ups right. Region record: name, 0, align, 0, size (physical: to the
  last GPU byte, unaligned), self-relative start. tools/kapf/swap_file.py: an archive copy
  with files swapped (block in its bank, used size, directory size).
- IN-GAME PROOF (logs-backup\20261101-083028-carentan-cgb1-result, devmap carentan, 0.5.687):
  the pack's zn01 got Carentan's collision (crop x 500..2300, y -200..1900, 345 KB), zn02/03
  empty .cgb files; Rouen's visuals stayed. The player came to rest at -151.9 / -143.9 /
  -120.0 where Carentan's floor is -152 / -144 / -120 and Rouen has no floor: the game uses
  our files. No crash, 138 fps. The pack was put back (hard links to Rouen's zones; Rouen's
  sha1s checked unchanged). The user saw Rouen and asked why: warn before any game launch.
- Not yet: whether the game takes one zone this big (Rouen zn01 is 3954 volumes; ours
  12379: the counts fit the format's 16-bit fields) or Carentan should be cut in zones;
  that comes with the level's zone layout (.scnb, the pack's zone list).

## Carentan step 2 (drawn level), started 2026-11-01 evening (no build)
- apkf.py: name_hash = h*33+c from 0, lowercased (every MP name record; names of 27+ chars
  are stored cut to 27 but hashed whole; plain-file type tag = hash of the extension).
  decompose()/compose(): a block from its parts (header = 0x1C fixed, types 28 bytes each,
  a zero word, entry tables, 32-byte name pool, region records, image, physical, tail):
  all 504 MP blocks compose byte for byte. --roundtrip checks both.
- Cells: the game asks for <level>_NNN then _SPLIT0.. (until one is missing) by name; the
  names come from the zone's .xenon.ib (IIBS), the zone's resource index:
  'IIBS', 0x4000A3D7, u32 13 categories, chain-start word 0x0C, u32 0x10 fix-up table
  offset (FLI-style chains, bits 9/10/11 seen); categories from 0x14, 32 bytes: u32 id,
  name[12] (TEXTURE FONT MESHFILE MESH ANIMFILE ANIM SCNANIM ANIMOFFSET SKELETON EFFECT FX
  DISCTEX DISCTEXSIZE), u32 slots, ->table, u32 names, ->name list; table = (hash, name
  index) pairs as an implicit search tree: size the least 2^k-1 >= names, middle
  (lo+hi-1)//2, breadth-first, empty (0,0): EXACT on all 220 tables; hashes = name_hash
  (15493/15493). Name list: (->string, 0) pairs; strings sorted (looks alphabetical).
  tools/kapf/iibs.py: read/write; names sorted by bytes, kept in the exporter's case and
  full length (the archive's are lowercased and cut, so an index lists names, not archive
  entries: for Carentan we name our own); strings padded to 4; per category: table, list,
  strings; chains in address order, a new chain when a step does not fit `bits`.
  ALL 46 ZONE INDEXES WRITTEN BYTE FOR BYTE. The 10 level-wide indexes (mp_rouen.xenon.ib
  etc., with an EFFECT category) still differ: needed for carentan.cod later.
- tools/kapf/cellmesh.py: parse()/build() of world-cell MESH resources; ALL 929 cell meshes
  of every MP zone rebuilt word for word (pointers by place) and their GPU parts byte for
  byte (--roundtrip). Layout: header 0x40 (+0x1C triangles, +0x20 sphere), list (0, ->sub),
  subs from the list end rounded to 16, each 0x7C + its format's elements + 0x40 (VB object
  00200001 1 0 0 0 FFFF0000, fetch offset|3, 0x10000000|bytes/4<<2|2; IB object 20200002 ..,
  offset, bytes), padded to 16 with CD except the last. GPU: per sub VB then u16 IB, CD-padded
  to 4 before each buffer, no end pad. Derived values: sub box = vertex bounds, sub sphere =
  box centre + half diagonal (1346/1346 exact); +0x30 float = sqrt(area/pi) exactly for whole
  quads only (465/1346; the exporter used its own source polygons: we write sqrt(total
  area/pi), APPROXIMATE); mesh sphere: neither the union of its subs nor the cell's (the
  exporter's source groups): we write the union's box sphere (tight, so culling is right).
- TEXTURES DONE (tools/kapf/xetex.py levels/level_image/level_blocks/tex_desc/tex_gpu): the
  Xenos layout after Xenia (texture_util.cc GetGuestTextureLayout/GetPackedMipOffset,
  fetched to scratchpad ref/xenia): base at base address with pitch = width rounded to 32
  BLOCKS; mips from the mip address, a surface each (pow2 sizes, 32-block aligned, 4 KB
  apart) up to the packed level (log2 shorter side - 4), which holds every smaller level at
  its packed offset; shorter side <= 16: all levels in the base, mip address 0. Fetch: w4 =
  max level << 6, w5 = mip address | 0x800 packed | 0x200 2D; endian 1 (DXT) / 2 (8888);
  +0x14 -> own D3D object (+0x2C). ALL 6809 2D MP texture descriptions word for word, every
  used texel byte for byte (the packed tail's unused space holds exporter leftovers; we
  write CD). Mip placement checked by picture: 6901 packed levels match the scaled level
  above better than the swapped place (scattered noise otherwise). Cube maps (21) not done.
- MATERIALS DONE (tools/kapf/cellmat.py build/parse, --roundtrip): header 0x40 (->name, ->'fx',
  0, 0, +0x10 overlay flag, FX import at +0x14, ..., +0x24 count, +0x28 ->list), 0x24-byte
  parameter records (->name, CD x3, type 2 int/3 float/0xB texture, ->value, size 4/0x1C,
  CD x2), values from the next 16-byte line, 16 or 32 bytes each, CD-filled; a texture value
  = TEX import + CD x6. Rouen/Verdun/Poissons zones: 1190 rebuilt exactly; 16 differ (prop
  shaders with float4 params), 25 other layouts (props): not needed for world cells.
  World shaders (Rouen): e_blend 423 (9 params: position_light0, cmn_lightmap_texture,
  lightmap_texture, normalmap_texture, normal_scale, spec_power, diffusemap_texture,
  specularmap_texture, technique; or 12 with detail), e_decal_punchthru (alpha test),
  e_blend_alpha, e_decal_simple / e_trans_cullnone (5: no normal/spec). +0x10 = 1 on decals
  (e_decal 20/21, e_decal_simple 9/9, e_trans_cullnone 8/8; e_blend 14/423): CoD2 decal sort.
- More writers: kapf.compose()/decompose() (archive: header, sections, block tables, per
  section directory + names, then all paths, strings padded to 4, blocks on 64K): 69 of 81
  MP archives byte for byte (Merville's zones, anim.cod, casdyn.cod differ: not looked at).
  Directory lists plain files, .xetex, .xemesh/.xecell (not MAT/MESH); size field: plain =
  bytes, TEX = gpu + 0x10010, MLTI = a budget (gpu + ~64K + ...; formula unknown: we write
  members' total + 0x11000). tools/kapf/block.py: a block from resources with symbolic refs.
  tools/kapf/cellmat.py, cellmesh.py, xetex.tex_desc/tex_gpu as above.
- CoD2 lightmaps (lump 1, 2 x 4 MB): planes r, g, b = 512x512 RGBA, each ONE colour channel
  for 4 directions (+1024^2 shadow); plain colour = each plane's 4 channels averaged (picture
  check: neutral, spread 2.1 vs 12.5). Soup's 2nd u16 = lightmap index (31 = none).
  CoD3 cell vertex: uv0 surface, uv1 lightmap (0..1), uv2 0, colour FFFFFFFF, DEC3N x in low
  bits (8417 vs 3330), front = CCW. CoD2 material: byte 0x0D sort (0x0B decal), word 0x28
  technique (1 opaque, 2 alpha test, 4 blend).
- tools/cod2import/cod2cells.py: Carentan -> 12 cells (4x3 grid, numbered like Rouen's
  000-011), 289 submeshes, 80 meshes, 287 MATs, 119 TEX (118 colour maps + 2 lightmaps 8888
  + white), sky/$default left out; 22 blocks, 28 MB; every block re-parses (mesh/mat/tex/
  bundle checks). tools/kapf/merge_zone.py: test zones = Rouen's zone archive with its cells,
  index and collision renamed out of reach (x-prefix), our blocks + a cgb/index block added.
- IN-GAME TEST 1 (logs-backup\20261101-095347-carentan-cells1-result, 0.5.687, devmap
  carentan): no crash, 144 fps. The game asked for all our cells (mp_rouen_NNN_SPLITk) AND the
  renamed xp_rouen_* names: it walks the DIRECTORY, not the index. Rouen's buildings gone.
  Picture: a white flat world (Rouen props floating, trees, sky fine): our geometry looks
  drawn untextured/unlit white. Textures are not in the asset log (it stops at 4000 names).
  Next: A/B runs (C2_TEST=A: e_blend with Rouen's textures; B: e_blend with ours).
- HOW THE GAME FINDS A ZONE'S CELLS (read in the recompiled code): sub_82441A58 formats
  <level>_<NNN>, looks it up, else _SPLIT0.. until one is missing (max 32), and draws every
  MESH listed in each bundle (tag 0x4853454D). The lookup sub_82474FD8(registry, category,
  scope, name) -> sub_820CFF58 searches each of the scope's zone INDEXES (.xenon.ib, the
  category's tree by hash) and returns the entry's BOUND word (the name list's second
  word, 0 until a resource is bound to it): 0 = missing. Binders that search and store:
  MESHFILE (sub_82475610), MESH (sub_82476E88), FONT, EFFECT, FX; none found for TEXTURE.
- Tests 2-3: index block last / old index first in the directory: nothing bound. Test 3
  (cells3, index FIRST in the directory, the old index's entry dropped): CARENTAN'S CELLS
  DRAWN (all white). Test 4: textures' blocks before the cells: still white. Test A2 (e_blend,
  Rouen's textures): CARENTAN DRAWN AND LIT (cobbled street, walls, poles, wires): meshes,
  materials and the shader work. Test B2 (e_blend, our textures + Rouen's normal/spec):
  shading from Rouen's normal maps, our diffuse and lightmaps WHITE: our TEX lookups fail
  (sub_824784B8 TEX branch -> sub_82477E70 -> sub_82474FD8 category 0 -> none -> default).
  Every real zone has its index first (65/65); directory texture order = index order in 44
  of 65 (so not positional binding). Rouen MATs import zone textures from other blocks 1228
  times (works there). 0.5.688 adds COD3_TEXLOOKUP=1 (logs those lookups) to see why.
- Test harness scripts (scratchpad w/): runvariant.sh <variant> <case>; cells pkl variants
  c2/cells (simple shaders), c2/cellsA, c2/cellsB (C2_TEST=A/B in cod2cells.py).
- CoD2 side: cod2import.iwi_levels(): every DXT level of a v5 .iwi (stored smallest first).
  Carentan: 121 materials, 269 images, 266 full DXT chains (all checked level 1 ~ level 0/2),
  3 special (one level, RGB 0x2, an odd size): later. PC DXT -> CoD3 = swap byte pairs.
- TEX description (0x60, as first seen): +0 width, height, 0, 0, +0x10 ->own
  name, +0x14 ->something in the image (not yet looked at), +0x18 0x4000, ..., +0x2C D3D
  header 00200003 1 0 0 0 FFFF0000 FFFF0000, +0x48 the fetch constant (xetex.py): w0
  tiled|pitch/32<<22|2, w1 format|endian<<6, w2 (h-1)<<13|(w-1), w3 0xD10 (DXT) / 0xC14
  (8888), w4 mip levels (0x100 16px, 0x200 128-256, 0x240 512x256, 0x280 1024; 0 none),
  w5 mip address|0xA00 (0x200 no mips). Lightmap pages: pg_colorlm_NNN_MMM, 8888, 64x64, no
  mips, in the zones. NEXT: the Xenos mip layout (packed tail) checked by rebuilding every
  MP texture's GPU part from its decoded levels, then MAT (params, FX e_blend), then the
  Carentan cells.
- MESH/MAT/MLTI dumped (pointers marked): MESH = submesh list (0x130 each: bounds, ->MAT,
  VB/IB fetch, 52-byte vertex: pos FLOAT3, normal DEC3N, 3x uv FLOAT2, colour, tangent,
  binormal); MAT = FX import (e_blend) + named params incl. LIGHTMAP textures (lm0001):
  CoD3's world is lightmapped, so CoD2's lightmaps (lump 1 + lightmap uvs) can come over.

## Rules learned this session (also in memory)
- Bash heredocs eat backslashes ("\\n" became a real newline in mp_fixes.cpp and broke the
  0.5.683 build; "\f" became a form feed in PATCH-NOTES). Use Write/Edit for such text.
- Don't run the game while the builder builds; run_test.py backs up logs and the ini.
- Git Bash turns a leading "/" in arguments into a Windows path (wawimport sound names).
