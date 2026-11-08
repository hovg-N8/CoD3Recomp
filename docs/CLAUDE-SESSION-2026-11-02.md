# Claude session 2026-11-02 (overnight + morning): HANDOFF

**State: source 0.5.728, built OK and installed. Nothing committed (no git remote). Friend zip on hold.**
Seelow (World at War's mp_seelow) is an installed, playable map pack; work on it is mid-way (see "Where to
pick up"). Older detail: docs/CLAUDE-SESSION-2026-11-01-night.md (sections "SEELOW IS IN THE GAME",
"MODEL COLLISION ADDED", "USER PLAYED SEELOW", "USER PLAY 3").

## The user's standing wishes (in their words where it matters)
- Seelow "1 to 1 with how it should be in waw, buildings and all", "smooth and playable as it is like in
  world at war", "make sure there is nothing left over from any other map we may have built off of".
- Seelow uses WaW's own Seelow sky (done). Carentan KEEPS Rouen's sky (their earlier final call).
- "if we have to replace seelows tanks with cod 3's tanks for the map do it" (not done; see below).
- Warn before every game launch; back up play logs before any run; no .bat files; never upload game data;
  x86-64-v2; don't sync the fork. Their play logs: "check the log" -> tools/mplog/mplog.py first.

## Where to pick up (in order)
1. **Verify the last install by eye** (installed ~09:13, not yet seen by the user or by me):
   pictures were taken but not looked at: logs-backup\20261102-091525-seelowwheat-result,
   ...-091740-seelowwreck-result (combined: logs-backup\wheat_wreck.png). Check grass/wheat are drawn
   (they go through e_trans_cullnone / e_decal_punchthru; e_decal_punchthru is NOT yet proven to draw on
   our materials: if wheat/branches are missing, put alpha-tested ones on e_trans_cullnone or e_blend).
2. **The last bot run did not play** (seelow14bots: 0 bot lines, "big test: loading seelow" only): find out
   why before trusting it (dropping the .dcgb files may have broken something; compare with seelow13clip,
   which worked with .dcgb still present). If the .dcgb drop breaks the bots/level: DROP_DCGB=0 in
   seelow_install.sh and look for another way (empty the dcgb instead of dropping it).
3. **Snagging / getting stuck on Seelow** (the user's biggest complaint, still open):
   - WaW's collision is 1:1 now (its clip map), so what's left is WaW-vs-CoD3 movement: WaW lets players
     MANTLE over 19-60 unit ledges (trench edges, low walls); CoD3 has no mantle. ~1/3 of bot stall spots
     are such ledges. Plan: a host "mantle/step-up assist" (when pressing forward against a ledge <= ~60
     with room above, lift the player), or ramps/clip at WaW's own mantle brushes (283 mount_mantle_over
     brushes are in the clip map, currently left out: seelow_clipmap.py "contents 0x9000000").
   - 312 model hulls are turned boxes (non-convex hulls over the 20000-triangle mesh budget): boxes are
     fuller than the shapes; candidates for invisible bumps.
   - Rouen leftovers still loaded (not collision): light grid .lgridb, BSP .btrb, zone boxes .zbbb, the
     level scene's Rouen records (moved), .phyb/.dtrb/.gdbb/.dbb/.aitbb/.ib. Check them if invisible
     things remain (memory: renamed entries still load; drop from the directory and prove with numbers).
4. **Tanks**: WaW Seelow's drivable Panzers/T-34s are not converted (vehicles are entities). The 9
   destroyed Panzer WRECKS (static models) are now drawn and collide. The user OK'd using CoD3's own
   tanks instead if needed.
5. Remaining texture gaps: ~5.5k model triangles grey (odd surfaces of the boxcar/water tower); flat
   normal/specular maps for everything (WaW's _n/_s maps not converted: bank budget, ~5 banks free).
6. Rouen's sun-glow cloud patches on the sky dome now show WaW's top clouds (cosmetic).

## What was done this session (big picture)
- Flamethrower, UAV, Mosin work: see docs/CLAUDE-SESSION-2026-11-01-night.md (0.5.703-0.5.727).
- 0.5.728: the scoped Mosin's CoD2 shot plays once a shot (it played on event 1 and on event 3 ~280 ms
  later; window 200 -> 900 ms). Verified: test mosin728. Crosshairs: the user got them back themselves.
- Seelow, end to end (all tools in tools/waw, all data in D:\Call of Duty 3 (USA, Europe)\seelow-work,
  game data: never commit):
  - seelow_world.py (geometry), seelow_materials.py (127 world materials -> colour images; pointer
    resolution by curves through exact anchors), seelow_textures.py (IWD -> PNG), seelow_cells.py (cells,
    lightmaps, models, shaders), seelow_models.py (static models: GfxWorld draw list 7582 placements,
    materials, LODs), seelow_clipmap.py (WaW clip map -> 3 .cgb, model collision), seelow_sky.py (WaW sky
    into CoD3's sky dome), seelow_install.sh (everything into the pack; run it after any change).
  - Pack: saves\content\00000002\Seelow (MapPack 5, ID 21, folder seelow, devmap seelow); Rouen's zones
    are the harness; Rouen's own files sha1-checked unchanged on every install.

## Hard-won facts (also in memory: converted-map-cell-limits)
- The game draws a cell's _SPLIT0.._SPLIT31 only (seelow_cells.py refuses >32). Banks: 96 x 3 MB shared;
  Seelow uses ~38 blocks (zn01 took 42 of 49 free in an earlier run).
- e_decal_simple writes NO depth (see-through walls); e_decal_simple_punchthru draws nothing. Use
  Carentan's C-mode set (e_blend + normal/specular maps, e_decal_punchthru, e_trans_cullnone, e_decal).
- WaW player clip = contents 0x20000; CoD3's = 0x10000 (swap, seelow_clipmap.cod3_contents).
- WaW zone pointers: block 4, but the file<->block distance drifts (other blocks interleave): resolve by
  curves through exact anchors, per region (tables vs handle arrays differ by ~0x276).
- XModel material handles point at material RECORDS (~0x24-0x28 before their texture tables); image
  pointers at table slots (+0x6D bias on the handle curve); a surface's rigid records all come first, then
  their collision trees (wawimport fixed); some model names are pointers (sky dome: read_xmodel(start=)).
- WaW lightmaps: *lightmapN_secondary lower half = colour, alpha (sun) 0 (WaW already includes the sun).
- WaW material cull modes: state bits word 0 bits 14-15 (0x4000 none, 0x8000 back), IW3 layout.

## Test kit
- tools/cod2import/run_test.py <case> --map seelow --seconds N --bots N [--primary N] --env ...
- Views: COD3_BENCH_VIEWS="x,y,z,yaw,pitch;..." COD3_BENCH_VIEW_SECONDS=25 COD3_BENCH_LOOKAT=x,y,z,dist
  (the yaw does not hold; LOOKAT centres a point). COD3_BIG_MODE=dm.
- Mosin: tools/cod2import/mosin_test.sh (restores bot_kits.txt, sha1 149c2a2f...). Flame: tools/waw/flame_test.sh.
- Primary numbers: 2 Thompson, 11 Kar98 scoped, 22 Webley, 23 flamethrower, 24 Mosin.
