# Claude session 2026-11-02 (morning to midday): HANDOFF

**State: source 0.5.731, built OK and installed. Nothing committed. Friend zip on hold.**
Earlier today: docs/CLAUDE-SESSION-2026-11-02.md (the overnight Seelow work).

## Where things stand
- **Seelow (day)**: installed, plays (CTF, 8 bots, 144 fps, no crash; test seelow22day). WaW's own sky.
  Rouen's leftovers repacked out of all four archives (tools/kapf/repack.py, run by
  tools/waw/seelow_install.sh): level 18 -> 12 blocks, zn01 45 -> 39, zn02/zn03 4 -> 2; 23 banks free
  (was 7). Rouen's 45 trees moved 60000 down (they showed under the map at 8000 down). Boxcar textured
  (LOD 1 materials from LOD 0), hedgehogs no longer black (name guess deterministic). Fog ramp kept real
  (--keep-tex fogramp: it had become a stand-in).
- **Mantling (0.5.729)**: WaW's 292 mantle helpers -> custom\mantle\<map>.txt; PM_StepSlideMove steps up
  to 62 within reach of one (mp_fixes.cpp). Seelow and Seelow Night only. Bots seen climbing 44-53.
- **Seelow Night (new map pack)**: pack 6, ID 22, folder seelownight, `devmap seelownight`. Install:
  `NIGHT=1 bash tools/waw/seelow_install.sh`. Moonlit cells (cells2n: seelow_cells.py SEELOW_NIGHT=
  "0.32,0.38,0.55"), Longyard's night sky (Bravo pack; its dome painting, clouds and fog ramp painted
  into the sky textures by seelow_sky.py --night). Tests night1-4: plays, CTF with bots, no crash.
  Still open: the distance fog is tan (its colour is not the fog ramp nor the colour tables; likely the
  scene's lighting record or the zones' light data), the player's arms and gun lit as by day (Rouen's
  light grid). Grafting Longyard's sky mesh (repack.py --graft) drew white: kept as a tool, unused.
- **Vehicles**: NOT working yet; parked safe. vehicles.txt has no Seelow/Rouen lines; the pack is
  switched off (game\mp\mp_merv\mp_merv_vehicles.cod.off). 0.5.731: the host never makes a vehicle from
  nothing (it crashed Seelow). Findings (tests seelow20/21tanks, rouenA_merv, rouenB*_pack):
  - On a map without vehicles, Merville's WHOLE archive beside it works (rouenA_merv): Merville's scene
    brings 10 real vehicles; the host moved one to the saved place. But 45 blocks / 133 MB.
  - The cut pack (tools/kapf/vehicle_pack.sh + vehicle_scene.py: a scene of 22 vehicles, all game types,
    parked): without Merville's model register (.ng.xmbb/.xmpbb) the vehicle's model instance is
    garbage (crash in sub_824427C0); with the register and its 152 models the level load hangs in
    sub_82472608/sub_824709D8 (B2, B3); with the soldiers' textures kept too, it ran out of guest memory
    in the level load (B4, memcpy in sub_82189FA8 at 664 MB).
  - Archives loaded beside the weapons (kind 8) do NOT use the map's 96 banks: memory is the limit.
  - Next: decode what the register loader (sub_82472608 -> sub_824709D8) walks, or cut Merville's
    register down to the vehicles (XMB_ header + string table, scnb-like pointer chains).
- The user's placements for Seelow (5 Panzer IV, 5 Sherman at WaW's tank places, a Kangaroo, a BMW):
  logs-backup\vehicles.txt.seelow-try.

## Seelow Night polish (~12:00-12:15, after the user's "Polish Seelow Night"; no build)
Goal: the tan distance haze and the player's arms/gun lit as by day. NEITHER fixed yet. Tried
(both still in the night install, harmless; tests night5/night6 show no visible change):
- tools/waw/night_lightgrid.py: the zones' light grid (.ng.lgridb) header: +0x28 sun direction (unit),
  +0x38 sun colour (0.503 0.488 0.442), +0x44 grid place, +0x54 a second colour (0.381 0.234 0.078),
  the same in Rouen's three zones; both colours scaled (LIGHTGRID_NIGHT=0.25,0.3,0.5). No change seen:
  the per-sample grid data (the bulk of the file, not decoded) probably lights the view model.
- night_lightgrid.py --fog: Rouen's fog effects in the level archive (AEPS gfog_mist_7c7c50z_wide_night,
  fog_mist_lt_7c7c1c_night): their (1,1,1,1) colour multipliers scaled (FOG_NIGHT=0.15,0.18,0.3).
  No change seen: these are not the distance fog.
Ruled out for the fog colour: fogramp (decodes as zeros in Rouen, not a colour ramp), the colour
tables (*.colorid.csv, identical in Rouen and Longyard), gdbb text. Found: the engine reads entity
keys fov/fogcolor/fognear/fogfar (strings 0x82065368..80) in sub_82564698 (a spawn function, class
table entry 31 at 0x825FE894; class not yet named), with defaults when the key is missing: Rouen's
scene has no such keys. The fog reaches shaders as the FogColor/FogValues constants (g_vFogColor).
NEXT for the fog: name class 31 (the host's class table: names' ids at 0x82BCCC68) and add a record
of it with fogcolor/fognear/fogfar to Seelow Night's scene (scnb.py can repoint a spare record's
class and properties, as vehicle_scene.py does); or find the default fog colour that sub_82564698
falls back to. For the arms: decode the light grid samples, or take Longyard's light values.

## Class 31 named (next session, 2026-11-02, read-only, no build)
- Class names: table 0x825FE740 (53 entries, copied to 0x82BCCC68 by sub_8255C420). **Class 31 =
  props_skyportal** (sub_82564698). Format 0x82065388 "%.2f %.2f %.2f %.1f %i %.2f %.2f %.2f %i %i" =
  origin xyz, fov (default 90), keys-found count, fogcolor rgb (default 0 0 0), fognear (0), fogfar (300),
  sent as config string 10 (sub_82531538(10, ...)). So these keys are the 3D sky portal's own fog, not
  the world's distance fog, and the defaults are black. Adding a props_skyportal to Seelow Night is
  unlikely to change the tan horizon haze; the haze colour must come from somewhere else.
- MP image: python -I tools/kapf/xex.py game\codmp_xenonf.xex <out.bin> [text] (keep it out of the repo).

## Seelow: "sideways" grass/wheat and invisible walls (~12:45-13:00, user report; no build)
- Nothing of Rouen collides: the three zone .cgb are WaW's (installed = seelow-work\coll_m, sha1
  checked), Rouen's unzoned.cgb is empty (0 boxes/brushes), the .ncgb are 96-byte stubs, .dcgb dropped;
  .phyb (helmet_pop), .dtrb (global/vehicle damage), .zbbb, .aitbb are generic tables, not shapes.
- Invisible walls in windows: WaW's breakable glass (contents 0x10, 7 brushes on berlin_window_white_grimy:
  six 40x42x64 window fillers at x 3036-3276 y 3590-3904 and x 2624 y 3764-4004, one pane at x -117
  y 5216-5264) were solid and unbreakable here. seelow_clipmap.py now leaves them out ("brushes left out:
  glass: 7"); coll_m rebuilt (old: coll_m.before-glass), day and night reinstalled. NOT yet play-tested.
- Windows WaW mantles through: ~100 of the 292 mantle helpers have under 70 units of headroom (CoD3's
  standing hull); the 0.5.729 assist only raises the step height, so at low windows the player must crouch.
  Possible next: crouch the player in the assist when the headroom above the helper is low.
- Grass/wheat (13:00-13:30): the WHEAT was sideways (tests wheat3: heads lying flat). Rendered from its
  own data (side view, both uv orders): as read the stalks lie across, with u and v swapped they stand.
  seelow_models.SWAP_UV = {"foliage_cod5_wheat_small"}; the grass (LOD 0 and 1), hedgerow, flowers, water
  tower, pine render right as read (left alone; a quarter-turn of the grass cards was tried and undone).
  cells2m and cells2n rebuilt (old: *.before-grass / cells2n.before-wheat), both packs reinstalled; test
  wheat4: wheat upright in rows.
- Falling through (13:33, test drop1): the player dropped (no noclip) at 1900 -875 (ground -312, below
  Rouen's zone-box floor -256), 2000 6000 and 3000 8000 (outside Rouen's .zbbb boxes) stands on the ground:
  Rouen's zone boxes are not the cause. WaW's walkable ground is all in our .cgb (7 sample points missing,
  near 1798 -2010). The holes are local: find them from a play log, "bots: host 0 record x y z" once a
  second (falls = z far below the ground; stuck = same x y while moving). `where` prints nothing to the log.
- Camera tests: COD3_CMD="46:noclip;52:cg_drawgun 0;58:setviewpos x y z yaw" (space commands >= 4 s apart;
  one second apart drops some), COD3_SHOTS counts from the match's first frame (~14 s before clock 58).

## Seelow: windows and invisible walls = the clip-bit swap; brighter night (~14:00-14:20, no build)
- The user's play (they played inside the side session's postoff test, logs-backup\20261102-130440-postoff-
  result: host positions move): no fall; stuck ~10 s at 718 -123 -314 between a bank and the Panzer
  wreck's clip. Their pictures 130519/130530 (Seelow Night): an orange glow dot in the sky (handed to the
  side session: likely the sun/moon glow with a Rouen-era picture; Longyard has Falaise_sky_moon).
- seelow_clipmap.cod3_contents NO LONGER SWAPS 0x10000/0x20000 (the morning's swap was wrong): the space a
  player climbs at 206 of WaW's 292 mantle helpers is inside a WaW 0x20000 brush (AI clip); WaW 0x10000 is
  the stair ramps (27 of 137 sloped vs 7 of 440). Clip-map material names are shifted (clip_vehicle with
  mantle bits): judge by geometry. coll_m rebuilt (old: coll_m.before-unswap); test unswap1 (8 bots, CTF):
  bots mantle 11 times (2 before), no crash.
- Seelow Night brighter: cells2n with SEELOW_NIGHT="0.40,0.475,0.69" (was 0.32,0.38,0.55; old: cells2n.
  before-brighter). Day and night reinstalled (night keeps --sun and --fogrec). Not yet seen by the user.

## Seelow Night's lamp light (~18:10-18:30, side session)
- WaW's lamp light is baked in its lightmaps; the night scale (SEELOW_NIGHT) dimmed it with everything.
  seelow_cells.py (night only, SEELOW_LAMPS=1 default): texels of the world within LAMP_FAR (350) of a lit
  lamp model keep up to LAMP_KEEP (0.9) of their baked light (full within LAMP_NEAR 120): each world
  triangle's lightmap uvs rasterised, each texel's place by its barycentrics. Lit lamps (45):
  lantern_on 10, lights_tinhatlamp_on 11 (z -160..-170: the tunnels), lantern_hang 2, lights_indlight 1,
  lights_sconce2 21 (the _off lamps and fireplaces left out). 41038 texels of lightmap 0, 4116 of 1.
- 18:28 mishap: a queued job of mine moved cells2n to cells2n.before-lamps and my lamp build
  (SEELOW_NIGHT 0.40,0.475,0.69) in as cells2n, then began a night install (killed partway) while the
  other session was rebuilding cells2n with 0.56,0.66,0.92. Told it; its rebuild + NIGHT=1 reinstall is
  the one to keep (it includes the lamp pools by default).

## Arms lit by day, Rouen's lamps, the light grid (~17:30-17:55, side session; no build of mine)
- The arms (ps 9a14f83313a256a4) take 9 rgb terms c18-c26 (spherical harmonics from the zone's light
  grid at the player). Seelow Night ~0.20 neutral (+ ~0.15 directional), Longyard ~0.03 bluish, ~0 higher
  (tests arms_seelownight, arms_longyard; compare with scratch cmpconst.py).
- .ng.lgridb layout (Rouen and Longyard alike): header words +0x04 light count, +0x08 section C start,
  +0x0C its count, +0x10 section A start (= end of the light list), +0x14 section B start, +0x18 bounds;
  from +0x28 the light list, 28-byte records (xyz or a direction, w 1 point / 0 direction, rgb): record 0
  the sun (the old 'SUN' at +0x38), record 1 the old 'SECOND'. Rouen 150 lights (fills ~0.08-0.12 at
  Rouen's places, 11 warm: its lamps, in pairs 8 apart); Longyard 303, its sun record 0.01. Section A:
  16-bit fractions (Rouen 3773, Longyard 48). Section B: packed u32 (00, idx?, ?, weight?) (Rouen 15559,
  bytes 1 ~0x2e-0x30; Longyard 8668, ~0xd9-0xfd). Section C: one u32 per grid cell (0xfc1f empty?).
- Tried, none moved the arm terms at the same spot (amb1 spawned where arms_seelownight did: 0.2048
  both): all light colours scaled (--lights 0.15,0.2,0.3, installed in the night zones), Rouen's lamps
  sunk, worldspawn ambient 0.03 (test only, restored). So the arms' light is baked in sections A-C (or
  the sun record with them): decode B's fields next (Longyard as the reference), or graft Longyard's
  sun record (0.01) and compare.
- Rouen's lamps (the orange dot): night_lightgrid.py --sink-lamps moves its 11 warm point lights 60000
  down, DAY AND NIGHT (seelow_install.sh runs it for both before the night scaling). Installed in both
  packs (backups logs-backup\seelow-zones-before-lamps, seelownight-zones-before-lamps). cpu_depth.cpp:
  'the light coronas (the oil lamps and the level's lights, a glow sprite each)'. Not yet seen in game.
- Caution: a test run during another session's build crashed the load (arms2, 17:38): wait for
  build-status 'built' before any run.

## Seelow Night's moon, and WaW's lights (~15:00-15:35, side session; no build)
- The moon: Longyard draws it as its own quad (its sky mesh's 4-vertex submesh, moon.sky /
  falaise_sky_moon 512x512 DXT5 with alpha, at azimuth -9.7, elevation 25.1, 662 out). Rouen's sky mesh
  has none, so seelow_sky.py --night now paints it into the dome (mp_rouen_bkgd): each dome texel's
  direction (Rouen's layout, checked against the dome mesh's own uvs: exact above the horizon) cast onto
  Longyard's quad, laid over by its alpha (MOON=0 skips it). Installed (backup logs-backup\
  seelownight.cod.before-moon); seen in the installed texture, NOT yet seen in game: the photo tour's
  aim does not hold (COD3_PHOTO_SKY's pitch is not applied, the yaw varies run to run).
- The moon is only a picture: the map's light is the baked lightmaps (cells2n, SEELOW_NIGHT) and the
  worldspawn sun (Longyard's sunlight 0.18, direction -55 340): a brighter night = those.
- WaW Seelow's lights: mp_seelow.ents has NO light entities (WaW compiles lights into the world). The
  zone has lamp models (lights_hang_lamp, lights_tinhatlamp_on, lantern_on), light_glow_indoor_short
  (45 uses), light_flare, fires (mp_fire_*, fireplaces, fx_mp_fire_*). Their light is already in WaW's
  lightmaps, dimmed evenly at night. Bringing them in = WaW's GfxWorld primary lights (no reader yet) to
  CoD3 light/corona scene records (spare records repointed, as vehicle_scene.py does) and glow effects:
  a job of its own.

## Seelow Night's orange dot in the sky (~14:10-14:30, side session; build 0.5.734): NOT fixed yet
- User's shots screenshots\CoD3-20261102-130519/130530.png; caught again in tour skydot1 view 6
  (CoD3-20261102-141448.png, dot at 1168,332: ~12 deg above the horizon, ~15 deg right of yaw 340).
- The draw (frame log of that shot): a 4-index quad, ps 37456d13395f7b55, blend 01060106, right after the
  fog pass, texture 64x64 DXT5 = global.cod's `dynamiclight` (the engine's own light-glow sprite, warm
  centre 247 203 140). Too small for the per-draw thumbnails (160x110).
- NOT a placed Rouen object: every scene (level + 3 zones) has instances/groups/effects switched off
  (0), no light/corona entity; Rouen's lamp/lantern meshes and vlight_* effects are only definitions.
  `dynamiclight` is also a level FLI key, and the engine has r_flares.
- Tried: sun direction turned 180 deg (sundir.py, restored after): inconclusive, because the photo tour's
  aim is not repeatable (same place, different facing each run; pitch -55 also fights the pitch limit).
- Next: aim reliably (a host switch to hold an exact view, or read the quad's vertices: vertex slot 95 at
  the draw, 24 dwords) to get the glow's world position; or one match with r_flares 0 / r_dynamiclight 0.
  Then decide: sun/moon glow (give it Longyard's look) or a leftover light (remove it).
- 0.5.734: COD3_PHOTO_SKY="pitch yaw" for the photo tour; tour pictures honour COD3_SHOTS_DRAWS.

## Seelow Night's tan haze: FIXED (~13:50, side session; build 0.5.733)
- The user's idea (copy Longyard's tree look) led there. 0.5.733 COD3_SHOTS_DRAWS=1 + COD3_FRAME_LOGS=1
  pictures every draw of a timed screenshot's frame (test draws1): the trees went light at draw 531,
  pixel program af5356c5ec765583 = the game's FOG PASS (reads the depth, blends its c2 by distance).
- Its values (tests fogpass_seelownight / fogpass_longyard): Rouen's c2 0.425 0.375 0.325 (tan),
  Longyard's 0.06 0.06 0.1. Stored in the level archive's FLI section as 9-float records: near, far,
  rgb, 0, strength (c0.z), 1.0, clip. Rouen/Seelow: 200 6000 .425 .375 .325 0 .1 1 10000 (x4);
  Longyard: 400 10800 .06 .06 .1 0 .195 1 16000 (x3).
- night_lightgrid.py --fogrec <longyard.cod> <level.cod> copies Longyard's first 8 floats (clip kept);
  run by the night install (seelow_install.sh NIGHT=1). Installed (backup logs-backup\
  seelownight.cod.before-fogrec); test fogfix: c0/c2 = Longyard's exactly, no tan on the horizon or the
  leaves' edges. Day Seelow untouched (still Rouen's 200 6000 tan: WaW's day fog is another question).
- Still open: the arms/gun lit as by day.

## Seelow Night's tan haze: the hunt (~12:25-13:06, side session; build 0.5.732)
Still NOT found. Ten test runs (logs-backup\*-fog1..5, skip1..4, frame1, postoff); ini restored after
postoff (backup CoD3Recomp.ini.before-postoff).
- Worldspawn sun: the world pixel program 94bf726b375d3b06 reads c0, c16-c25, c255 (its cached DXBC:
  %LOCALAPPDATA%\CoD3Recomp\shaders\<hash>.ps.<ver>.bin, disassembled with d3dcompiler_47 D3DDisassemble).
  c22 = worldspawn suncolor (Rouen 0.99 0.93 0.81), c21 sun position, c17-c20 zero (point lights), c23-25
  scalars 0.5 0.25 300. Its output is lighting x textures: no fog mix at all. night_lightgrid.py --sun
  gives Seelow Night Longyard's sun (installed; fog3 shows c22 = .55 .63 .7): haze unchanged. Kept
  (Longyard's real night sun; backup logs-backup\seelownight.cod.before-sun).
- 0.5.732: COD3_DRAWLOG_PSDUMP also writes a VSDUMP line (vertex constants). The world's vertex program
  dd87d38c79ee4eb7: matrices, c26-c28 small vectors, c30 sun position: no colour (fog4).
- COD3_SKIPPS: 1b87.. tree leaves, bce8.. trunks, 9a14.. sleeve (draws 3-9 = view model), 2a89.. a
  full-screen blend after the world: the band stays each time. 5d27.. (first draws) = a blur (gaussian
  weights), not a clear; no hardware clears in the frame.
- Textures: the dome's horizon ring is near black; the world draws' 32x32 RGBA (slot 3) are c2flatn/
  c2flats stand-ins; no 256x8 fog ramp on the world program. sky_see1 faces are left out (flag 0x4).
- Host grade off (postoff): the tan band and the tan OUTLINES of leaves and branches are still there,
  fainter. So the game draws it. The outlines suggest it is behind the foliage: alpha edges show it.
- Next: COD3_D3DFRAME=auto frame log + F12-style per-draw pictures (COD3_FRAME_LOGS) to see which draw
  first puts the band in; or skip the remaining programs one by one (f0a5.., 149f.., 1ae2.. (n2176,
  blended), 10ff.., af53.., 2224.., 0a8f.., fe65.., 3745..). A day Seelow shot from the same spot would
  tell whether the band is WaW's own backdrop (hills/pine walls, UNLIT lightmap 110 x SEELOW_NIGHT).

## Tools added today
- tools/kapf/repack.py: repacks an archive into fewer blocks (stand-ins for unreached textures,
  --drop/--follow-imports/--external/--keep-tex/--graft/--fli/--fli-also; built-in pointer check).
  Rules learned: textures in blocks before what imports them; a zone's index first; a pack's manifest
  number (5th float per archive in the FLI) is a bank minimum: lower it with the block count.
- tools/kapf/vehicle_pack.sh, vehicle_scene.py (vehicles), tools/waw/seelow_mantle.py (mantle boxes).

## The user's wishes (unchanged)
Seelow 1:1 with WaW, nothing of Rouen left; Seelow keeps WaW's sky, Carentan keeps Rouen's; Seelow Night
uses CoD3's night map's sky (Longyard). Vehicles: Panzer/Sherman/Kangaroo/BMW on Seelow, eventually any
vehicle on any map. Warn before launches; back up logs; no .bat; no uploads; no fork sync.
