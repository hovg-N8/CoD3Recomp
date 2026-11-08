# Claude session 2026-10-30 to 2026-10-31 (menus, countdown, crash guard, CoD2's Webley)

Source and install: **0.5.668** (built OK 2026-10-31 13:35; 0.5.657 at 03:42). Patch notes 0.5.634-0.5.657
have every detail. Play logs saved in the install's `logs-backup\user-play-2026-10-3*`
and the self-tests in `logs-backup\20261031-*`.

## Done and confirmed by the user
- Menus by mouse:
  - Clicks, arrows (two-column rows), quit YES, right click back (B then Back).
  - The team choice is steered by key-long presses; its gold frame is moved by us
    (FrameTrack/FrameShift, menu_mouse.cpp).
  - SPECTATOR in the team choice; the pause menu in the seat.
  - SWITCH SIDE opens the team choice; a side picked there from the seat spawns you.
  - User: "it works bro".
- PC key icons in MP (0.5.636: the icon font's address had bit 27; matched).
- The pre-match countdown (0.5.641, 0.5.647): it starts with the match clock, everyone
  is frozen, the bots are balanced 2 s before GO.

## Done, not yet played by the user
- 0.5.650: SPECTATOR shows no side (the menu's +0xF0/+0xF4 zeroed).
- 0.5.651: **the render list patch-block guard** (sub_8216DF98 checked first). It stops
  643's runaway crash, not the memory damage before it (see Open).
- 0.5.634: killcam explosions replayed (the server's event 56 kept and played again).
- **0.5.652-0.5.657: CoD2's Webley revolver in CoD3 MP** (below).

## The Webley (what exists now)
- **tools/cod2import/** (in the repository; game data never is):
  - `cod2import.py`: reads CoD2's xmodel/xmodelparts/xmodelsurfs (v20) and .iwi (v5)
    textures from the user's CoD2 .iwd files, and dumps raw files. The formats follow
    mauserzjeh/cod-asset-importer.
  - `meshview.py`: draws a .mesh to PNG.
  - `run_test.py`: one unattended CoD3MP test. It backs up the logs and the ini, sets
    the class and restores the ini; screenshots go to the result folder.
  - `guestpeek.py`: reads the running game's guest memory via the PDB's Guest::Base.
  - `cod3mp_weapon_fields.txt`: CoD3's weapon field table, 354 fields read from the
    running game at 0x825D77D0.
- **Game data** (made from CoD2, not in git), in `D:\Call of Duty 2 (USA)\webley` and
  copied to the install's `custom\webley\`: viewmodel_webley.mesh, webley_c.png,
  wpnfire_webley.wav, wpnrld_webley_rev.wav.
- **The weapon** (mp_fixes.cpp Guns[] "webley", a copy of the Colt, weapon 45 on Isle):
  - CoD2's numbers for 105 fields (WebleyFields): damage 45 to 22% over 384..800,
    clip 6, ammo 18, fire 0.135 s, reload 5 s, CoD2's kicks, spreads and sway.
  - In the class editor (primary 22 / sidearm 24, "WEBLEY").
- **Its view** (MpFixesWebleyView): CoD2's mesh (the speed-loader left out), scaled 0.49
  onto the Colt body, swapped in while held; the Colt's hammer, slide and magazine
  are hidden.
- **Its texture**: CoD2's, by the renderer (Render::SetMeshTexture; the draw matched by
  the buffer's GPU address 0x46100000; pixel slot 5 = colour map).
- **Its sounds**: CoD2's shot (CampaignEventName, shot events) and reload
  (sub_82554BA0). The Colt's fire/reload/rechamber sounds are silenced for whoever
  holds it. Audio::PlayClip mixes WAVs into the game's frames.
- Screenshot proof: logs-backup/20261031-034241-webley657-result.

## Webley, 2026-10-31 midday (0.5.658-0.5.665; patch notes have the details)
- 657 play: looked backwards and crashed on the last shot. 658: turned to the Colt's -y
  (the Colt's muzzle is at y -4.14), and the last-shot cue swap skipped for a gun with
  no shot cue (the crash: strlen on a null `gun.shot` in sub_8212B268).
- "Two guns merged": (1) CoD2's viewmodel has every bone at the origin; the parts are placed
  by its animations. cod2import `--pose viewmodel_webley_idle` (xanim v14 reader, after
  riicchhaarrd/xmodelconverter) assembles it. (2) Only the colour map was CoD2's; the
  Colt's normal (slot 3, DXT5) and other maps (4, 6) drew its panels. custom/webley/slots.txt
  now gives CoD2's normal/specular and a grey.
- 659: bots may carry it (bot_kits "webley"; a default medic line has it as pistol).
- 661-665: its own world model (w_brit_webley_WORLD, a deep copy of the Colt's with CoD2's
  weapon_webley mesh; confirmed drawn: buffer 46050000 in world2's log).
- Tools: run_test.py (COD3_WEBLEY_SHOTS for screenshots), COD3_WEBLEY_DUMP (fit dump),
  COD3_WEBLEY_TEX, COD3_WEBLEY_CLOCKWISE, COD3_WEBLEY_WORLD_SIGNS, COD3_WEBLEY_MARK_COLT.
- Still the Colt's: the arm animations (the reload looks like a magazine change). A
  converter for CoD2's xanims onto CoD3's arms is the next big Webley piece.
- World at War is not installed on this PC: the flamethrower waits for the user's copy.

## Webley: open, in order
1. **Played test**: fire and reload. Expect log lines "webley: CoD2's shot" and
   "webley: the Colt's sound ... not played". Firing could not be scripted (COD3_PAD
   stops the big test's join).
2. The fit in the hand (COD3_WEBLEY_FIT="scale x y z" without a rebuild), and aiming
   down the sights (the Colt's sight line).
3. **Others see the Colt**: the world model (weapon_webley_lod11, 1333 vertices,
   converted) is not swapped. The world mesh is shared by every Colt holder, so it
   needs a per-player way, e.g. a new world model name with a mesh of our own.
4. CoD2's animations (xanim, the speed-loader reload): a converter for xanim onto the
   Colt arms' bones. A project of its own.
5. Bots: the Webley is not in their loadouts.

## Carentan, 2026-10-31 afternoon: CoD2's side is read
- `tools/cod2import/cod2map.py <cod2 main> mp_carentan <out>`: IBSP v4 -> one .mesh a
  material (121 used, 118,352 triangles; only the triangles' own vertices: the soups'
  vertex counts are not to be trusted), entities, spawns (32 dm, 32 tdm, 16+16 ctf,
  16+16 sd), placed models (1532 of 65 kinds: grass clumps, crates, hedges, trees, the
  Opel truck), a picture from above (PIL). Sky: material sky_silotown.
- `tools/cod2import/cod2assets.py <cod2 main> mp_carentan <out>`: 172 materials' maps
  (colour/normal/specular/detail) -> 360 PNGs, 65 placed models -> .mesh.
- Output: D:\Call of Duty 2 (USA)\carentan (mp_carentan_top.png shows the town).
- Collision not read yet: lumps 1-6 (planes, brush sides, brushes...) and 22-36.

### How to get it into CoD3 (the plan from here)
A native CoD3 level (KAPF base archive: .scnb scene, .cgb/.dcgb/.ncgb collision grids,
.phyb, nav, lighting; zone archives) would need ~10 unknown formats written. Instead:
1. **Draw**: the Webley's world-model trick scaled up: deep copies of a CoD3 model, each
   carrying Carentan meshes (<= 65k vertices each) with CoD2's maps (SetMeshSlotTexture),
   attached to an entity at the world origin of a CoD3 MP map (a script_model the map
   has, or one spawned). First experiment: one material's mesh (v_grass or a house) drawn
   in Isle, to learn scale (both games use inches), lighting, culling of big meshes.
2. **Hide** the CoD3 map's own scene draws (the renderer knows each draw's buffers; the
   level archive's are one range), keep characters/effects.
3. **Collide**: CoD2's brushes (planes + brush sides) read, and the movement trace
   (CM_BoxTrace's MP address, via mp_addresses.txt) answered from them, or the
   triangles; bots' routes are learned from play.
4. **Spawns**: CoD2's tdm/ctf/sd points in place of the map's.
5. Placed models (grass, crates, trees) as more copies, last.

## World at War
Not installed on this PC (searched Program Files, D:\, Steam common, Downloads): the
flamethrower and WaW maps need the user's own copy installed first (Steam).

## The Webley's animations (next big Webley piece)
Its arms still play the Colt's (a magazine reload). CoD2's xanims are readable now
(read_xanim_pose); CoD3's .xeanim format is unknown (in the archives as files; the weapon
record names them at +72..+164: idle, fire, reload, ...). Steps: log the Colt's anim
names, extract one .xeanim (rebuild the KAPF/LZO extractor; notes in NEXT-SESSION.md
"KAPF STUDY"), learn its tracks, write CoD2's reload onto CoD3's arm bones.

## Carentan and other maps (research, as planned before)
- CoD2's mp_carentan.d3dbsp (iw_03.iwd, 25.6 MB) is IBSP version 4. Its materials,
  triangle soups, vertices, triangles and entities (spawns) are readable
  (cod-asset-importer ibsp.rs); its collision (brushes) is not covered there.
- The CoD3 side is the unknown: a level is a .cod archive (scene, collision, zones,
  lighting, nav). We have never made a level, only changed loaded ones.
- Suggested path:
  1. Dump an existing CoD3 MP level's archive structure (asset kinds, the collision
     format).
  2. Convert Carentan's geometry and spawns to a draw-only overlay on a flat CoD3 map,
     to learn the scale and lighting.
  3. The collision. Bots need navigation data (they learn routes from play).
- The World at War flamethrower and maps: later, the same way (WaW's formats differ
  again).

## Open from before
- Memory damage behind 643's crash (front end code calling a virtual of 0x827B2910, a
  list header). Seen in 643/644 after the match start's team/class choice.
- Killcam explosions on a joining machine; Xbox Live Create Match arrows; the friend
  zip is on hold.

## Carentan as its own map (evening; the user's decision)
- 0.5.667 drew Carentan's pieces over Isle (script_models carrying copies of the Colt's world
  model). It worked (textured; 66 of 120 before extended memory ran out) but the user wants
  **Carentan as its own map in the map list, its own level file like CoD2's, not pieces in
  another map**. 0.5.668 removed the overlay whole; Isle is as in 0.5.666.
- tools/kapf (new): `lzo.py` (LZO1X, kernel lzo1x_decompress_safe) and `kapf.py` (lists an
  archive's sections, blocks, files by type; `--files`; `--block N out`). MP map archives are
  NOT compressed (stored size 0; blocks of 0x300000 from 0x20000).
- A CoD3 MP level (Rouen):
  - base `rouen.cod`: 18 blocks, 568 files: .xetex 312, .xemesh 109, .bpeb 99, .xefx 26, and
    one each of .ib .bgib (grass) .btrb .zbbb .xmpbb .xmbb .aitbb (AI) .cfgstrb (config
    strings) .cgb (unzoned, 256 B) .dcgb .ncgb .dtrb .phyb .scnb (scene, 293 KB) .gdbb
    (game data, 170 KB) .dbb (115 KB) .bindicb .atrbb .heap, colorid/skybloom/hdi csv;
    sections AUDIO (rouen.wbk) and FLI.
  - zones `zn01..zn03.cod`: geometry cells `mp_rouen_NNN[_SPLITk].xecell` (up to 1.5 MB each),
    and per zone .ib .xmpbb .xmbb .lgridb (light grid) .cgb (collision grid, up to 435 KB)
    .dcgb .ncgb .dtrb .phyb .scnb.
  - a block is an 'APKF' pack (big-endian): 16-byte header, then a type table of 28-byte
    entries at 0x1C: u32 tag (LE fourcc: FX, MAT, TEX, MESH, MLTI, SKEL, AEPS, or a hash for
    level types), u32, u32, u32 offset (from the entry +12), u32 count, u32 size, u32 flags.
    A directory entry's index is (type << 16 | item).
- Steps to a native Carentan (each a project):
  1. APKF pack: find each file's bytes by (type, item); extract Rouen's level files.
  2. Map list: how the menu lists maps (mp_rouen etc.; table in the xex or a .gdbb/.cfgstrb)
     and how a map name finds its folder (Kernel::CurrentMap, the archive loader).
  3. Copy test: a "carentan" map that is a byte copy of a small map under a new name, listed
     and loadable (proves the list + loader path).
  4. Formats, in order: .xecell (geometry: CoD2 triangles + our textures as .xetex), .scnb
     (what cells/models draw where), .cgb/.dcgb/.ncgb (collision from CoD2's brushes),
     .gdbb/.dbb/.cfgstrb (spawns, flags, bomb sites, entities), .lgridb (lighting),
     .aitbb/.atrbb (AI), the rest copied from the small map if the game accepts that.
  5. Write mp\carentan\carentan.cod + zones with tools (game data stays local, never in git).

## Carentan step 1 done: a listed, loading map pack (evening, no build)
- **The map list.** Base maps: a 9-record table in the MP xex at 0x825D2240, 114 bytes a
  record (u16 index, char[32] menu key MPFRONTEND_X, char[32] loading title key, char[16]
  folder, char[32] location key), ended by index 0xFF, room for 64; counted by sub_82391778
  into 0x82BCCB20. Merville 0, Poisson 1, Eder Dam 2, Fuel Plant 3, Mayenne 4, Rouen 5,
  Forest 6, Hostage 7, Verdun 8.
- **The game's own way to add maps: a map pack.** saves\content\00000002\<pack>\ with
  displayname.txt (UTF-16LE, no BOM) and mp\dlmaps.txt, read as "%d %d %s %s %s %s %s":
  MapPack, ID, menu name (plain text, '_' for spaces), folder, loading title, location,
  language (US DE FR ES IT lines). Packs: 1 Champs (ID 9), 2 Valor (10-14), 3 Bravo
  (15-19; Marseilles is ordnance, ID 16). The level opens as dlc<MapPack>:\mp\<f>\<f>.cod
  (kernel_file.cpp maps dlcN: to the pack whose dlmaps.txt says N).
- **Carentan's pack: MapPack 4, ID 20,** "Carentan" / CARENTAN / "CARENTAN, FRANCE", folder
  `caren`, in the install's saves\content\00000002\Carentan (game data: local only). For
  now its files are Rouen's (zones and loadscr hard links; caren.cod a copy).
- **The FLI (the level's manifest, section FLI)** names the level's own paths:
  mp\rouen\rouen.cod, mp\rouen\zn01..03.cod, mp\weapusge.cod, plus zone names, ambient
  packages. If they name another folder, sub_8247D918 (the level archive check, kind 3)
  returns 0, the loader (sub_82535A18) shows an error and sends "reboot", and the game
  starts default.xex (first test, folder `carentan`). Its strings are packed on 4-byte
  boundaries and reached by self-relative pointers (u8 tag, u24 offset from the pointer:
  0x20+0x38 = 0x58 "mp_rouen_zone02"), so make_pack.py rewrites paths in place only:
  a folder no longer than the source's (5 for Rouen). Our own FLI writer (step 5) can
  use `carentan`.
- **The sound bank** is the one the archive's AUDIO section names (rouen.wbk), looked for in
  the map's folder: dlc4:\mp\caren\rouen.wbk.
- Tests (logs-backup\20261031-1829..1837-carentan-pack1..3): pack1 rebooted (FLI paths);
  pack2 loaded and ran 2 min (the player spawned, 118 fps, no crash) but no map sound
  bank; pack3 the same with rouen.wbk opened. Tests start the map with devmap (`--map
  caren`); **the menu's create-game list has not been looked at yet**.
- New tools: tools/kapf/xex.py (XEX2 retail-AES + basic compression to the loaded image,
  pure Python, 30 s; the image is game data, scratchpad only), tools/kapf/make_pack.py
  (the pack from a source map folder).
- Host code keyed on the map's folder name (dom.cpp flag spots, Kernel::CurrentMap() ==
  "rouen" checks, bot knowledge files) does not apply to `caren`.
- Next: step 4/5 for real (the formats), starting with the FLI writer (so the folder can be
  `carentan` and zone lists can change), then .xecell geometry from CoD2.

## Carentan step 2 done: the FLI writer, folder `carentan` (night, no build)

- The FLI's pointers are fix-up chains (sub_820F9968 / sub_820F9910 / sub_820F99D8, run on
  load). Word 0x18 is the offset of the fix-up table, always the block's last 16 bytes:
  u32 count (2), u32 offset of a list: u32 bits, then the chain starts (0x0C).
  - bits is 9 in most levels and 8 in ederdam; read it, don't assume it.
  - Each chain word: the low (31 - bits) bits are a signed offset from the word, the next
    `bits` bits are the distance in bytes to the chain's next word.
  - A word with bit 31 set ends the chain (its low 31 bits are the offset; with bit 30 set,
    the whole word is).
  - So the top byte of a pointer is not a type tag. My first guess was wrong.
- In every MP level (base and packs, 21), the FLI is the archive's last block, alone in its
  64 KB page, with 0xF000+ to spare. It can grow without moving anything.
- tools/kapf/fli.py: walks the chains (`pointers`), finds every mp\ path and its one pointer
  (`paths`), `move(block, old, new)`, and `replace_block` (writes it back, with the block
  table's used/bank sizes and the directory size to match).
  - A path that fits its old room is changed in place.
  - A longer one is zeroed and its new text appended after the fix-up table, with its
    pointer moved. The table and the chains stay put, and move() refuses if the set of
    pointer words changes.
- Checks: for all 20 other levels, move(x, x) gives the same bytes, and a move to a longer
  folder name resolves every path's pointer to its new text. Nothing else changes, and the
  file keeps its size.
- First test (20261031-201101-carentan-folder) hung, then faulted at 27 s: I had rewritten
  word 0x18 as the "length" (it is the table's offset), so the fix-up walk ran into
  "mp\carentan..." (r11 = "tan\"). Fixed: 0x18 is left alone.
- Second test (20261031-201400-carentan-folder2): 2 min 36 s, no crash, 144 fps median.
  - It opened dlc4:\mp\carentan\carentan.cod (archive 12, kind 3), zn01-03 and
    dlc4:\mp\carentan\rouen.wbk.
  - The game heap ended at 582 of 608 MB, the same as the `caren` run (pack3), so that is
    this level's normal.
- The pack is now saves\content\00000002\Carentan with mp\carentan\
  (dlmaps.txt: `4 20 Carentan carentan CARENTAN CARENTAN,_FRANCE`).
  - Old builds moved to build-backups\Carentan-pack-caren-20261031 (worked) and
    ...-carentan-hung-20261031 (the bad 0x18).
- make_pack.py now uses fli.py, so any folder name works.
- Still Rouen's level inside. Host code keyed on "rouen" doesn't apply.
- Next (step 3): the APKF pack's own files (extract and rewrite single files in MAIN), then
  CoD2 Carentan's geometry into .xemesh/.scnb, then collision and spawns.

## 0.5.669: no Colt left on the Webley (night)
- User: "part of the colt 45's model" still showed on the Webley.
  - The converted mesh and the buffer we hand the game are a clean Webley (checked offline
    from fit_dump.bin).
  - Moving the Webley up (COD3_WEBLEY_FIT="0 0 0 2.5") left the Colt's slide and hammer in
    the hand, drawn as black shards: hidden by +28 and +0x40+104 only.
  - Now hidden as the MG34's bipod is: +28, +0x40+88, +0x40+104 and +0x158+28 all 0.
- The silver look: the Colt's draw samples a 128x128 reflection cube in slot 2. The
  renderer now makes any *_cube.png a cube texture, and slots.txt gives
  2=dark_cube.png (in custom\webley and D:\Call of Duty 2 (USA)\webley).
- Tested (logs-backup 20261031-203451-webley669, -203559-webley669moved): clean, blued,
  nothing of the Colt left in the hand when moved.
- No-build tricks for next time: COD3_WEBLEY_FIT to separate our mesh from leftovers;
  COD3_WEBLEY_TEX slot overrides (2=cube, 3=normal, 4=specular, 5=colour, 6=?; 1 is the
  shadow map).

## The six-shot reload: where it starts
- The Colt's reload is two files in mp\weapusge.cod: w_arms_usCOLT_reload.xeanim (arms,
  0x14A0 bytes, block 11) and w_usCOLT_reload.xeanim (gun parts, 0x3F4, block 13);
  skeleton animdefs\colt.xeskel (block 1).
- A directory entry's index is (type << 16) | number: 1 .xetex, 2 .xeanim, 3 .xeskel,
  5 .xemesh.
- A block is an APKF pack: 'APKF', 0x04010000, ..., then type records (u32 tag as LE
  text: 'MAT', 'TEX', 'MESH', 'MLTI'..., then counts and table offsets). The .xeanim
  table isn't decoded yet.
- Steps: (1) APKF per-file extraction (Carentan needs it too); (2) read .xeanim
  (tracks, keys, bones per colt.xeskel); (3) CoD2's viewmodel_webley reload xanim
  (read_xanim_pose already reads xanims) onto CoD3's arm bones, with the cylinder,
  extractor and rounds as the gun's parts; (4) give the Webley its own anim names in the
  weapon record (+72..+164) and load them.

## The reload, paused for J's co-op merge (21:00)
- 0.5.670 (built) hooked sub_82475610 for COD3_ASSET_DUMP: the Colt's anims never pass
  through it. The source (not built yet) now also hooks sub_82475550 (r3 name, r4 a new
  resource object; sub_8218DCC0 finds or loads it by the name's key), which is the path
  anims take (assets: lines from 824731C0). The dump writes the object and 0x3000 bytes
  behind each pointer to custom\webley\dump\<name>.made.bin.
- CoD2: C:\Program Files (x86)\Activision\Call of Duty 2\main. viewmodel_webley_reload is
  151 frames at 30 fps (5.0 s), 81 bones: arms (j_shoulder/elbow/wrist/fingers _le/_ri),
  gun (gun_pivot, j_gun, j_chamber, j_bolt, j_bullet1-6, j_quickloader*). The arms' rest
  pose: xmodel/viewmodel_hands. The normal map is ~webley_n-gggr (x in alpha, y in green).
- Next: build, run with COD3_ASSET_DUMP="usCOLT colt", decode the dumps, then retarget.

## 0.5.671: J's co-op v46 merged (night)
- Source: Downloads CoD3-coop-v46-source.zip (build: CoD3-coop-v46.zip, not used). Their
  changes since v40 are in docs/COOP-V40-DESIGN.md (taken whole: "v43..v45", "v46").
- Merged three-way against their v40 zip (CoD3-coop-0.5.920-source.zip, the base of 0.5.622):
  - coop.cpp: git merge-file (ours, v40, v46), no conflicts; our 0.5.601 sprint kept.
  - The other coop files were v40 unchanged here, so they are v46 now; coop_swap.cpp is new
    (wrapped in ifndef COD3_MP like the others: the first build failed in CoD3MP without it).
  - Shared files: only J's additions put in by hand (heap guard, CoopRelaunchFresh, hidden
    start and Window::Reveal, Input::RealPad under the input lock, PRESS START). Their
    copies also undo our own later fixes (the log caps, noclip, prompts, cursor setting),
    so they were not taken whole.
- Our files before the merge: scratchpad coop/ours-before (this session only).
- Tests (tools: scratchpad coop_test.py; the copies' output came back on stdout, not in
  separate logs: COD3_LOG is ignored when stdout is a pipe):
  - Solo campaign, COD3_MAP=forest: 2 minutes, no crash; still in Forest's film at the end.
  - Two copies on this PC, COD3_COOP=host / join with COD3_COOP_MAP=forest, 7 minutes. Lobby,
    start, the film, the join's load, the second player (entity 317, 600 health), room made,
    the joining copy's freeze (37 soldiers and drones deleted), puppets 62-65 all animated
    (0 T-posed), no heap-guard hit, no crash.
  - Seen: one DbgBreakPoint at 0x825239B0 from 82512DB0 on the host as it made the second
    player (J's reliable command overflow is sub_82512970); both copies ran on.
  - Seen: after the freeze the host drew 5-14 fps and the frozen joining copy 90+ (both
    simulating 60 frames a second). Likely the two copies sharing one PC. Two PCs untested.
- The Webley reload is paused (section above); next is to run 0.5.671 with
  COD3_ASSET_DUMP="usCOLT colt" in multiplayer.

## Friend update 0.5.671 (night; the user asked for it)
- CoD3MP-update-0.5.671.zip (63 MB) and CoD3MP-source-0.5.671.zip (28 MB), beside the
  install folder (MAKE-FRIEND-UPDATE.ps1 source).
- MAKE-FRIEND-UPDATE.ps1 now puts custom/webley in every update (14 files: both meshes, the
  colour/normal/specular maps, slots.txt and its textures, both sounds; never fit_dump.bin
  or dump/), and the source zip carries tools/cod2import and tools/kapf (Python only).
- INSTALL-FRIEND-UPDATE.ps1 copies a custom folder too. UPDATE-README.txt explains the
  Webley files and the co-op lobby.
- Tried: the installer run on a scratch folder put all 14 files in place and verified the
  build.
- Not in it: Carentan (still Rouen's level inside) and the bots' Webley line (bot_kits.txt
  goes only in a full update).

## The reload, resumed: CoD3's anims as loaded (night, 0.5.671, no build)
- COD3_ASSET_DUMP="usCOLT colt" (MP) dumped all 20 Colt anims via sub_82475550 to
  custom/webley/dump (logs-backup/20261031-223105-animdump2). Arms anims name skeleton
  viewmodel_hands, gun anims skeleton colt.
- The anim object (vtable 0x82028CB4 at +0x6C): +0x14 name, +0x48 skeleton hash and name,
  +0xA0 the skeleton object (vtable 0x82028C78: bone names, parents, tags), +0xA8 length
  (f32; the Colt's reload 2.0667 s), +0xB0 30.0 fps, +0xB4 frames (63), +0xB8 bones (arms
  24, gun 8), then tables, and at +0x110 a packed key stream (+0x108 start, +0x10C end).
- The keys are chunked (sub_82188E68: chunks of N frames, each track decoded through its own
  object's method +72, 48 bytes a track; sub_82185B40 lets a chunk go). Writing this codec
  ourselves is a large reverse-engineering job.
- CoD3's arms are a Biped rig: bip01 l/r clavicle, upperarm, forearm, hand, finger0..4 with
  three joints, foretwist 1/2, gizmo_ammo_b, gizmo_ads, gizmo_weapon, tag_weapon,
  tag_utility, tag_cambase, tag_camera. CoD2's are j_shoulder/elbow/wrist/fingers _le/_ri,
  so CoD2's reload has to be retargeted through world poses, not copied by name.
- The Colt gun skeleton: tag_weapon2, tag_flash, tag_ammo, colt, colt_hammer, colt_slide,
  tag_brass, colt_mag, align_ammo, colt_mag_return, left_hand_align_dummy, orientation.
- Options: (A) write CoD3's codec (big); (B) at run time, while the Webley reloads,
  overwrite the view arms' bone poses after the game's own pose with CoD2's reload
  retargeted onto the Biped bones, and pose the Webley's own parts (cylinder, extractor,
  rounds, speed-loader) in our vertex buffer from the same frames (recommended); (C) only
  the gun's parts move, the arms stay the Colt's (smallest).

## The Webley's own reload, built (0.5.672-0.5.675, late night)
- Hook: sub_82186740 (MP; the frame's pose jobs, sub_8250EFF0: count 0x82BCBEFC, 820-byte
  jobs from 0x82B4F828) fills a model part's pose. r3 is the skeleton object (its name at
  +0x0C: viewmodel_hands for the first-person arms, 55 bones; colt for the gun, 11). The pose
  is a model-space 4x4 a bone (rows x, y, z, position), not local.
- CoD3's skeleton object: +0x70 bone count, +0x74 records (0x30 each: hash, name[32], +0x24,
  +0x28 the rotation's offset in the rest table, +0x2C u16 parent, u16 index), +0x94 the rest
  table (quaternions; where the arms' positions are is not worked out).
- tools/cod2import/xanim.py reads CoD2 xanims whole (every key, sampled with slerp) and
  draws skeletons; retarget.py makes custom/webley/webley_reload_arms.bin (151 frames x 55
  bones, relative to the pelvis at rest) and webley_reload_gun.bin (the gun's 15 bones'
  moves from the idle pose, and whether shown). Its inputs are the pose dumps of 0.5.672
  (COD3_POSE_DUMP: custom/webley/dump/pose_2.bin the arms, pose_3.bin the Colt) and the fit
  0.453 / -0.30 -0.64 -0.01.
- The mapping: one transform A from CoD2's view space to the arms' space through the gun
  (the fit, the Colt body bone, tag_weapon), so the hands stay on the gun. tag_weapon follows
  j_gun; the hands follow CoD2's wrists (the left hand drops CoD3's offset after frame 10:
  CoD3's left arm is off-screen with a pistol); the elbows are a two-bone solve with CoD3's
  lengths toward CoD2's elbow; the fingers take CoD2's turns; the twist bones follow the
  forearm.
- Game: MpFixesWebleyReloadBegan (our own Webley reload sound, or COD3_WEBLEY_RELOADTEST)
  starts it; PlayWebleyReload writes the arms each frame (0.12 s blend at both ends);
  PoseWebleyParts writes the Webley's vertices at each new frame (PoseWebley, then
  Render::MemoryFreed); WebleyColtPose holds the Colt's gun skeleton still. The speed-loader
  is in the mesh now, folded away at rest and left out of the size fit.
- Seen (scripted, screenshots in logs-backup/20261031-225947-reload674): the gun turns, breaks
  open with the cylinder up, a spent round drops, the left hand cradles it, brings the
  speed-loader, the gun closes. The left hand is very close to the camera at the speed-loader
  frames (CoD3's hands are big against the Colt-sized Webley).
- Not yet: a real reload from firing (cannot be scripted); whether the timing matches the
  ammo being added (reloadAddTime 3.5 s); an interrupted reload (sprint) keeps playing until
  the Webley is put away.

## Night of 2026-10-31 to 11-01: Webley finished off, Carentan and the flamethrower scoped

### The Webley (0.5.676-0.5.679)
- Cylinder (0.5.676): each shot turns it 60 degrees (clockwise from behind, eased 0.12 s)
  and its round is gone; the rounds shown are the clip (ps + 4 * (172 + def+1468)). In a
  reload: as it was until the cases fly (frame 35), all six fly, then from frame 89 the
  rounds the reload gives. The user confirmed it in play. COD3_WEBLEY_CLIPTEST=N for tests
  (it holds N even after a reload: that is the test switch, not the game).
- Fingers: per-joint bends (CoD2 fingers curl +z, the Biped -z; CoD2's right fingers run
  along -x); the thumbs are NOT flipped (CoD2 thumbs curl -z like the Biped's). Checked by
  the joints' own angles (identical to CoD2's) and screenshots.
- Framing (tables only, no build): the gun moves in the view as CoD2's (its turn in view
  space, its path scaled as the fit, 0.453); the hands keep CoD2's relation to the gun;
  the left hand blends to CoD3's off-screen rest while CoD2's is away from the gun
  (LEFT_NEAR 9.5 / LEFT_AWAY 13 CoD2 units). retarget.py now takes the mesh path too.
- Icon (0.5.677-679): CoD2's hud_webley (custom/webley/hud_webley.png). The kill feed and
  HUD take icon objects from the HUD's per-weapon table (sub_824EF868, +52 + 4 x weapon);
  the Webley's slot gets a copy of the Colt's object whose parts point at a copy of the
  Colt's image with its own texture place (MakeWebleyIcon), drawn as the PNG by the new
  Render::SetTextureImage. 0.5.677 tried the image look-up by name: never asked for.
- Kill feed: above the compass ("Bot 3 [icon] Bot 24").
- CoD2's view: cod2view.py renders it, but CoD2's viewmodel projection is unknown (its eye is
  2.7 units from the gun): the renders are too close to use.

### Carentan: what a real level needs (not done tonight)
- Rouen's level archive: .xetex 312, .xemesh 109 (APKF type MLTI), .bpeb 99, .xefx 26, csv,
  .ib (index buffer), .bgib, .btrb, .zbbb, .xmpbb, .xmbb, .aitbb (AI), .cfgstrb, .cgb
  (unzoned) .dcgb .ncgb (collision), .dtrb, .phyb, .scnb (scene), .gdbb/.dbb (game
  database: spawns, flags), .bindicb, .atrbb, .heap; zones zn01-03 hold more.
- APKF (a block): 'APKF', 0x04010000, 0x04000000, -1, 2, header size, 0, then type records
  28 bytes each (tag: 4CC MAT/TEX/MESH/MLTI/SKEL or a name hash; version; sub-count; a
  self-relative pointer to the type's entries; count; entry size; flags). A directory
  entry's index is (type's place in this list << 16) | entry. The entries' (offset, size)
  pairs match the files' sizes; their offsets' base is not pinned yet.
- A 1:1 Carentan needs our own writers for the scene, the three collision grids, meshes
  (MLTI), tiled DXT textures, lighting, the AI's routes and the game database. That is
  several sessions; CoD2's side is read already (cod2map.py: geometry, entities, spawns).

### The flamethrower (World at War: D:/Steam/steamapps/common/Call of Duty World at War)
- Its fastfiles are 'IWffu100' + version 0x183 + one zlib stream (plain zlib works:
  common_mp.ff 39 MB -> a 75 MB zone).
- In common_mp: viewmodel_usa_flamethrower, weapon_usa_flamethrower; anims
  viewmodel_flamethrower_idle/fire/lastfire/pullout(_fast)/first_pullout/putaway(_fast)/
  ADS_up/ADS_down; sounds weap_flamethrower_start/fireloop/cooldown/off/foley_F/R (and 3p);
  materials mc/mtl_viewmodel_flamethrower(_hose), mc/mtl_weapon_flamethrower; images
  usa_flamer_m2flamethrower_c/_s, usa_flamethrower_world_c/n/s (also in iw_09/iw_14/iw_26
  .iwd); FX weapon/muzzleflashes/fx_flamethrower_view_mp; the weapon file
  weapons/mp/m2_flamethrower_mp (text, iw_14.iwd).
- The flame itself is WaW's engine flame stream, set by flameconfigs/flame_firstperson_mp.cfg
  (about 100 flameVar_ settings: stream chunks, fire, drips, smoke, light): CoD3 has no
  such system, so it needs a stream renderer of ours.
- To read the models and anims: a parser of WaW's zone structures (XModel, XSurface packed
  vertices, XAnimParts) is needed; the assets lie in sequence in the zone.

## Carentan writers, step 1: the APKF pack (2026-11-01 morning)
- tools/kapf/apkf.py reads a block's type records and entries and places the inline files;
  tools/kapf/extract_plain.py writes every inline file of archives out (game data: keep out
  of git). Rules (in apkf.py's docstring): type records 28 bytes (tag, version, parts,
  self-relative entries, count, alignment, flags) up to tag 0; an entry is a name record
  (self-relative: hash + char[28]) and one size a part; data from header + 0x44 rounded to
  16, version 0 and 1 types inline in type order at each type's alignment.
- Checked by magics on Rouen, Verdun, Poissons, Fuel Plant (all archives): 90 of 92 right
  (loadscr.cod block 1's index buffer is at 0x800: a variation not handled yet).
- Inline (extractable now): .scnb (SCEN), .cgb/.dcgb/.ncgb (collision), .gdbb/.dbb/.cfgstrb
  (game database), .xenon.ib (IIBS), .btrb (BSPT), .dtrb (DTRB), .zbbb (ZBSB), .xmbb (XMB_),
  .xmpbb, .ng.lgridb (lighting grid), .aitbb, .atrbb, .phyb (PHDB), level.heap, level.bgib,
  csv, effects (FX). Version 2+ (MAT, TEX, MESH, MLTI = .xemesh/.xecell bundles, SKEL, AEPS)
  lie elsewhere in the block: next to work out.
- Rouen's world geometry: each zone has a .xecell (MLTI) bundle of cell_NNN_mesh_MMM meshes
  (zn01: 32 meshes, 109 materials) plus its .scnb scene, .cgb collision and .lgridb lighting.

## Carentan writers, step 2: the APKF pack fully decoded (2026-11-01, no build)
- Step 1's placement was half right: it held only because the checked files came before any
  typed resource. The real rules (now in tools/kapf/apkf.py's docstring, `apkf.read` returns
  a Pack):
  - word 0x10 is the number of memory regions (1 or 2). Their 24-byte records sit at
    header + 0x14: s32 self-relative name ("image" = the CPU's, "physical" = the GPU's),
    0, alignment, ?, size, ?. A type record is 20 + 4 * regions bytes: tag, version,
    parts, entries (self-relative from +12), count, then one alignment per region
    (0xFF000000 = nothing there; TEX's physical one is 0x01001000, MESH's 0x01000004).
  - image: from the region records' end rounded up to the image region's alignment (16
    mostly, so header + 0x44 with two regions; 0x800 in loadscr block 1). EVERY type's
    first part (plain files and resource descriptions alike) in type order, each at its
    type's alignment.
  - physical: from the image's end rounded up to the physical region's alignment (64K),
    the second parts (texture pixels, mesh vertices and indices) in type order, each at the
    alignment in its type's physical word (low 16 bits).
  - the tail, from the physical end rounded to 4 to the block's end: fix-ups (word indices
    into the image, to -1), then imports (image word index, type tag, byte offset of a name
    record in the name pool; to -1).
  - a pointer word: top 6 bits = base (0 = image, 0x3F = the name pool), low 26 bits = a
    WORD offset. Every typed resource's first word is its own name record (0xFC0000xx).
  - the name pool: 32-byte records (hash, char[28]) for the files and every name their data
    use (bones, textures, shaders, material parameters); its start = the lowest entry name.
- Verified on all 81 MP archives (504 blocks): every tail ends its block exactly; every
  fix-up (about 498,000) lands in its region, name-pool ones on a record start; every import
  (56,434) on a record. Texture fetch constants (+0x48 in the 0x60 TEX description) hold
  base 0 and the mip offset inside their own data (2048x2048 DXT1 bkgd: mips at 0x200000):
  the loader adds the placement. A mesh description's pointer lands on its vertex
  declaration (stride 0x34: position, normal, 3 uv, colour, tangent, binormal).
- tools/kapf/extract_plain.py now writes every file: CPU part as <name>, GPU part as
  <name>.gpu, typed ones prefixed b<block>.<TYPE>.<entry>.
- Open in the region records: words 3 and 5 (rouen block 0: 0/0x1C and 0x10000/0x1EA84);
  the image size word counts from 0 in loadscr block 1 (0x5158) but from the image start in
  rouen block 0 (0x12F70). Needed for the pack WRITER, not for reading.
- Next: MESH/MAT/TEX/MLTI descriptions (decode Rouen's meshes to OBJ and its textures to PNG
  to prove it), then the scene (.scnb) and collision, then the writer.

## Carentan writers, step 3: CoD3 textures and meshes read (2026-11-01, no build)
- tools/kapf/xetex.py: TEX to PNG. The 0x60 description holds the Xenos fetch constant at
  +0x48 (tiled bit 31 and pitch/32 in word 0; format and endian in word 1; width/height - 1
  in word 2; mips address in word 5). Untiled with render_resources.cpp's TiledOffset,
  pairs swapped (endian 1), DXT decoded through Pillow's DDS reader. Rouen zn01: all 103
  textures (DXT1 50, DXT3 16, DXT5 36, 8888 1) decode right (signs, decals, normal maps; the
  sky dome mp_rouen_bkgd 2048x2048).
- tools/kapf/xemesh.py: MESH to OBJ. Description: name, submesh count + list of (u32,
  ->submesh), LOD count + list of (->mesh, distance) (stove_b01 -> b02 at 800, b03 at
  1600), triangle count. Submesh: sphere, min, max, ->MAT, ->bone palette, ->vertex buffer
  (D3D header + fetch constant: GPU-part offset | 3, bytes/4 << 2 | endian 2), vertex count,
  ->index buffer (D3D header, GPU-part offset, bytes; 16-bit), index count, ->declaration
  (stride, ->12-byte elements ending at stream 0xFF), primitive 4 (list). World cells
  (cell_NNN_mesh_MMM in the zones) are in world space: zn01's 132 meshes render as Rouen's
  square and streets (168,415 vertices, 99,650 triangles). Vertex: FLOAT3 position, DEC3N
  normal/tangent/binormal, FLOAT2 uv x3 (uv1 = lightmap), COLOR; skinned adds UBYTE4
  indices and UBYTE4N weights (stride 0x3C).
- Rouen's level files by size (base / zone 1): xmpbb 813K, scnb 293K / 169K, gdbb 170K,
  dbb 115K, cfgstrb 105K, xmbb 88K / 57K, atrbb 60K, btrb (BSPT) 42K, xenon.ib 35K / 16K,
  dcgb 15K / 7K, cgb - / 436K, lgridb - / 199K, zbbb 9K, aitbb 3K.
- Order decided: the flamethrower next (the Webley pipeline exists), then back here for the
  scene (.scnb), collision (.cgb) and the game database (.gdbb), then the writers.

## The flamethrower (0.5.681, 2026-11-01 morning)
- World at War (D:/Steam/steamapps/common/Call of Duty World at War) has no loose assets:
  everything is in zone\english\*.ff ('IWffu100', 0x183, one zlib stream). tools/waw/
  wawimport.py reads the zone directly (findings in its docstring):
  - zone data has NO padding (alignment is in memory only); a pointer -1 = data follows.
  - XModel: 0xE4 bytes, then its name, bone names (u16 script strings; the zone header's
    string table at 0x34), parents, quats (8 a non-root bone), trans (16 a non-root bone:
    checked on the 13- and 2-bone models), part classes, base pose (32 a bone), surfaces
    (0x40 each), then each surface's vertices (32 bytes: xyz in the base pose, binormal
    sign, colour, half2 uv, packed normal, tangent), rigid lists (+collision trees: 40-byte
    header, 16-byte nodes, u16 leaves), triangles.
  - sounds: RIFF files inline after their names (MS ADPCM; decoded to PCM, checked by
    lag-1 correlation 0.55-0.99). images: iw_NN.iwd images/*.iwi version 6 (28-byte header,
    largest level last).
- viewmodel_usa_flamethrower: bones j_gun, tag_flash (+17.7 x), hose1..11; 2 surfaces (body
  3679 v, hose 389 v; hose material = mp_w_skin_grey50_c + detail maps). Rendered textured
  offline: right.
- make_flamethrower.py writes custom\flamethrower (mesh, texture with grey band, slots
  stand-ins, flame_fire/flame_lick sheets with alpha from brightness, kill icon, 4 sounds).
- Weapon numbers: weapons/mp/m2_flamethrower_mp: damage 50 per 0.2 s, maxDist 340,
  enemyCrosshairRange 485, overheat 38/13; anims viewmodel_flamethrower_*; sounds
  weap_flamethrower_start/fireloop/cooldown (files sfx/weapon/special/flame_thrower/...).
- In game (0.5.681): Guns "flamethrower" (copy of the Thompson), class primary 23 / sidearm
  25; MpFixesFlameView swaps w_tommy.w_usSMG_VM for the M2 (fit worked out offline from the
  Thompson mesh in weapusge.cod block 10 via xemesh.py), hides w_mag/w_bullet/
  w_charging_handle; the flame is an overlay (screen-space sprites, flamethrower.cpp).
- Next for it: test screenshots (muzzle placement, fit), the world model for others, its kill
  icon, the WaW arm animations (idle/fire/pullout) if the Thompson's look wrong.

## Carentan writers, step 4 (started): scene and collision, first look (2026-11-01)
- .scnb (zn01, 168,764 bytes): 'SCEN', f32 1.34, ...; +0x18 u32 instance count (334);
  instances from 0x50, 0xF0 bytes each, raw dumps of the exporter's structs: most words are
  its stale pointers (0x0012Fxxx stack, 0x00B7xxxx heap: the loader must overwrite them;
  a writer can copy them). Per instance: +0x08 colour (rgb floats), +0x70 a handle
  (0x0F01xxxx, also stale), +0x74..+0xA4 the transform (3 rotation rows with w, position
  xyz, 1), +0xC0..+0xE0 its inverse rotation. Then a model/effect name table (u32 head:
  low 16 = 8, high 16 = name length rounded to 4 x 24; u32 0; name padded to 4): 27 names
  (crates, sandbags, chairs, flowers; fog, smoke, fire effects). Then per-name groups (16
  bytes: stale, 0, u32 count, u32 0x0040xxxx): counts 37, 2, 29, ... (322 over 24 groups,
  not 334: not understood yet). World cells are NOT instanced here (world space already),
  so a first Carentan could ship CoD2's static models baked into its cells and an empty
  instance list.
- .cgb (zn01, 436,224 bytes): bounds min (-4801, -3905, -257) / max (5057, 3263, 1153), a
  grid, then typed primitives: 46 boxes first (0x24 bytes: u32 flags 0x0803xxxx, u32,
  centre xyz, half extents xyz, radius), then other kinds (different sizes). Terrain's
  representation (triangles?) still to find.
- Neither file has a magic check in the code (no 'SCEN' constant in ppc_mp): loaded by a
  generic reader.

## The flamethrower in game (0.5.681-0.5.687, morning of 2026-11-01)
- Class weapon "M2 FLAMETHROWER" (primary 23 / sidearm 25), a copy of the Thompson (weapon 46
  on Isle). Tests: logs-backup/20261101-06xx/07xx flame681..686, flamebots687, flamepose687.
- View: WaW's viewmodel in the Thompson body's place; fitted by TWO points (0.5.682): the
  rear grip on the pistol grip, the tube just behind the front grip in the left palm (5.7
  degrees nose up, scale 0.532). One point (0.5.681, scale 0.391) left the left hand open
  under nothing. The Thompson's draws read the COLOUR MAP FROM SLOT 4 (the Colt's: 5):
  slots.txt "2=dark_cube.png 3=flat_nm.png 5=grey.png", SetMeshTexture(..., 4).
- Flame (flamethrower.cpp): chunks simulated in camera space (nozzle point back-projected
  from its screen place 0.515/0.565 at depth 18; ~950 u/s toward the aim 600 out, drag 1.2,
  gravity 260, size 0.8 -> 17 units, 150/s), projected at 65 degrees across, drawn along
  their screen motion with WaW's flamethrower_fire frames (by age). 0.5.684's 2D version
  piled into a ball; 0.5.685's sizes filled the screen. 0.5.686 looks like a jet of fire.
- Screenshots with the overlay: COD3_SHOT_OVERLAY=1 (0.5.684 made it work in HDR: the HDR
  path took the shot before the UI layer).
- World model (0.5.683/684): w_usa_flamethrower_WORLD looked up as w_ussmg_world, a copy with
  WaW's weapon_usa_flamethrower in its body (no turn: both models muzzle +x, z up, origin at
  the hand). Seen in a bot's hands (bot_pose flamethrower via COD3_EXEC works with bots
  added later).
- Sounds: ignition, loop, cool-down (ours), the mono loop for others (heard in the bot test).
- Not done: WaW's own arm animations (the Thompson's are used), the kill icon (Thompson's),
  others' flames are not drawn (only heard), no overheating (100-round tank).

## Carentan writers, step 4b: the zone collision (.cgb) layout (2026-11-01 07:40)
Rouen zn01's mp_rouen_zone01.cgb (436,224 bytes), arrays separated by runs of 0x43 ('C')
filler; big-endian:
- 0x00 min xyz, u32 0x58; 0x10 max xyz, stale; 0x20 centre xyz (min+max)/2, stale;
  0x30 f32 radius (5001.1 x sqrt 3: the root cube's half size 5001.1); then 9 pairs (stale
  pointer, count): A 3954, B 1834, C 1732, D 917, E 917, F 4830, G 14984, H 9987, I 6124;
  then stale words and floats to 0xD0.
- A 3954 x 36 at 0xD0: volumes: u32 flags (0x08xxxxxx or 0; low 16 bits 0x1240/0x32C0/
  0x0240: contents-like), u32 flags (0x015040A0, 0x000040A0, 0x00500000...), centre xyz,
  half extents xyz, radius.
- B 1834 x 4 at 0x22CE0: (first << 16 | count) into F (the last ends at 4830): BRUSHES,
  each a run of planes.
- C 1732 x 10 at 0x24990: 3 x s16 (a cell's coordinates?), u16 start, u16 count into G
  (starts run 0, 12, 20, ...; max 14976).
- D 917 x 4 at 0x28D40: (first << 16 | count) into H (max start 9975 + 12 = 9987).
- E 917 x 10 at 0x29BA0: 3 x s16, u16 start, u16 count into I (max 6116).
- F 4830 x 16 at 0x2BF80: PLANES (unit normal xyz, distance).
- H 9987 x 4 at 0x3ED60: packed points (bytes like 03 28 01 00 / 73 20 03 32: quantized).
- G 14984 x 2 after H (H 39948 + G 29968 + 7 filler = the run's 69923 bytes).
- last run (0x4FE90, 108912 bytes): 10928 bytes not placed yet, then I? 6124 x 16 at the
  end (six s16 and a u32: a box-tree's nodes, like 'ff a3 00 b7 ff 05 01 02 ff 5e 00 c5
  00 00 00 6d'); the file ends with 80 01 x6, ffffffff records.
Reading: brushes (B -> planes F) for solid shapes, and a second set (D/E/H/G/I: packed
points in cells with a box tree) most likely the triangle collision (terrain, models).
CoD2's Carentan collision is brushes + terrain patches: B/F map directly from CoD2's
brushes and planes.

## Carentan writers, step 4c: collision volumes solved, meshes open (2026-11-01 08:00)
- A (3954 volumes) is three runs in order: 1203 plain boxes, then 1834 BRUSHES (A[1203 + k]
  takes B[k]'s planes: all 1834 brushes' planes cut their own box, none missing even at 1
  unit), then 917 MESHES (A[3037 + m] takes D[m] and E[m]). 1203 + 1834 + 917 = 3954; the
  header stores only the total and B's and D's counts (boxes = A - B - D).
- A brush = its box (A: centre, half extents: the axial sides) + its angled planes (B -> F;
  e.g. list 0: two opposite vertical pairs, a box turned in xy). CoD2's brushes convert to
  this directly (axial sides into the box, the rest as planes).
- Meshes: E[m] = (s16 origin xyz = the box's min corner, start/count into I); D[m] = start/
  count into H (packed 4-byte points). The points are NOT plain bit fields from E's origin
  (best of all splits/scales: 48% inside their box): perhaps normalised to the box, so an
  inside test cannot find them; open.
- C (1732: s16 xyz within the zone, start/count into G; counts 8/12/16/20) and G (u16,
  high nibbles 0-15, low 12 bits up to 4048): not understood yet (a grid's lists? packed
  polygons?). I: 16-byte records (3 s16 min/max pairs + u32), a box tree, at the end; a
  10,928-byte run before it not placed.
- Plan for Carentan's collision: CoD2 brushes -> A boxes/brushes + B/F planes; CoD2 terrain
  triangles as thin brushes (face plane, 3 edge planes, bottom), so the mesh half is not
  needed. Still needed: what C/G and I are (the look-up structures), to write them.

## Carentan writers, step 4d: the .cgb fully decoded (2026-11-01 08:30)
See tools/kapf/cgb.py's docstring and docs/CLAUDE-SESSION-2026-11-01.md. Corrections to 4b/4c:
the third array is the brushes' CORNER SETS (origin + 4-byte packed corners), the 4-byte
array after the planes is the corners, then u8 TRIANGLE INDICES (meshes, via the first 917
table), then 4-byte MESH POINTS (via the second 917 table: origin + start/count), then the
box tree (the 16-byte table at the end, its size in the header at 0xC0). The first header
word pair is (boxes << 16 | brushes, volumes). All 49 MP .cgb files parse and check.
