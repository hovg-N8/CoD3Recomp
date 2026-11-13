# World at War file audit (2026-11-08)

Read-only audit of the user's own Steam install, version 1.6 (`version.inf`, the 1.7 patch
is not installed). The WaW Mod Tools are not installed either, so their extra files were
not checked.

Covered: 128 fastfiles, 35 iwd archives, both exes. Scripts in the fastfiles are plain
source with developer comments intact (1,893 `.gsc` copies, about 65,000 comments).

- Tools: `tools/wawaudit/` (`dump.py`, then `xref_levels.py`, `xref_audio.py`, `xref_tokens.py`).
- Dump: `D:\Call of Duty 3 (USA, Europe)\WaW-Audit\` (outside the source tree, game data,
  never upload). Extracted art and `_contact_sheet.png` are in its `images\` folder.

**How "known" was judged:** web searches plus fan wiki and TCRF search snippets. TCRF's
pages served an anti-AI page instead of their content, so its full lists could not be
read. "No report found" means only that these searches did not find one.

**Bottom line on secret maps:** no hidden playable map exists in this install. No cut
level has a fastfile, so there is no geometry to load. Everything below is leftovers.

## Possibly unreported (no report found)

### Der Riese's code has branches for three unreleased zombie maps
`nazi_zombie_factory` scripts test `level.script` against `nazi_zombie_coast`,
`nazi_zombie_paris` and `nazi_zombie_theater` (`_zombiemode_weapons.gsc`,
`_zombiemode_spawner.gsc`, `_zombiemode_perks.gsc`, `_loadout.gsc`).

- Paris's Mystery Box gets its own light. Comment at `_zombiemode_weapons.gsc:568`: "we need
  a smaller light in the catacombs for paris, the generic one fills the area under the tower."
- Coast and Paris use the riser-to-door zombies and Der Riese's two-switch perk power.
- Theater has its own perk-prompt visibility code (`_zombiemode_perks.gsc:755`).
- Text left in `localized_nazi_zombie_factory.ff`: `ZOMBIE_COAST_ZIPLINE` ("Press & hold &&1
  to use zipline [Cost: 1000]"), `ZOMBIE_COAST_ZIPLINE_ACTIVATE`, `ZOMBIE_COAST_ZIPLINE_WAIT`.

Context: a Paris map teased on Der Riese's noticeboard and later reworked into Moon is
reported on fan wikis, and a forum post quotes a developer on a Paris concept with an
Eiffel Tower and catacombs. These scripts are first-hand file evidence for that. The
inference that Coast and Theater became Call of the Dead and Kino der Toten is mine; the
files only show the names.

### Verrückt's rising fountain turret
`nazi_zombie_asylum.gsc:1433` `Fountain_Mg42_Activate()` is complete. The fountain top sinks
200 units, the MG42s rise, and they drop back when done. The call at line 1635 is commented
out: "cut for now :(". Prompt text survives: "activate the Fountain Turret [Cost: 750]".

The shipped map still has `fountain_top`, the `fountain_mg` turret and `fountain_turret`
pieces. Only the trigger `trig_courtyard_mg` is gone.

### Campaign levels and a 2007 schedule
`maps/_status.gsc` holds per-level start and milestone dates for: `trn`, `mak`, `pel1`, `pel2`,
`fly`, `hol1-3`, `see1-3`, `rhi1-3`, `oki1-3`, `ber1-3`.

- **Okinawa 1 (`oki1`):** still a live entry in `devgui_maps_sp.cfg`, with hooks in `_load.gsc`,
  `_introscreen.gsc`, `_status.gsc`. Its text and loading screen do not exist.
- **Seelow 3 (`see3`):** commented out in the same dev menu, intro code remains.
- **`living_battlefield`:** test level with a dev-menu line, `_loadout.gsc` case and two orphan
  music files in `Music/Mission/L_Battlefield`. `vehicle_test`, `mp_physics_test` and
  `mp_ca_vehicle_test` are named in `playeranim.script`.
- **Training (`trn`):** on the schedule, plus `SFX/Levels/training` and one orphan drill sergeant file.

### Cut Seelow tank gameplay
Unreferenced objectives in `see2` (none found in its scripts): "Commandeer the Tiger tank",
"Advance towards Seelow using the Tiger tank", "Destroy the fuel supplies", "Clear enemy
tanks around train", "Destroy incoming Panther tanks".

Also: `pel1/maps/_type97.gsc` dates a cut, "7/31/08 turning this off now that tankmantling is cut".

### Other cut zombie prompts
`ZOMBIE_TRAP_DOOR`, `ZOMBIE_PROJ_LIFT`, `ZOMBIE_PROJ_DOOR`, `ZOMBIE_BUILD_BRIDGE`,
`ZOMBIE_CABINET_OPEN_*`, `ZOMBIE_BUTTON_SOUTH_FLAMES`, `ZOMBIE_WEAPON_STICKY`,
`ZOMBIE_WEAPON_MOSIN_GL_1500`. Reviving voice lines were also dropped ("cut , but leave the
script just in case", `_laststand.gsc:632`).

### Unused audio (18 files nothing names)
- Music: `orphan_ann_sign-on`, `mx_stalin_push`, `mx_underscore_brave_soldat` (8 MB),
  `L_Battlefield/MX_SBowl`, `cave_stinger`.
- Der Riese player0 lines: `crawler_attack_00-02`, `dog_harder_00-02`, `monkey_explode_00-02`, `level_start_01`.
- Others: `vo_ig_drill_dist`, `ship_horn`, `TEMP/BG_Wind_00`.
- One Holland line was shipped inside Seelow's fastfile: `stream/vo/hol1/pa/hol1_igd_gemp_000a.wav`.

### Developer notes worth reading
- `mak_anim.gsc:655-657`: two Sullivan lines marked "THIS SHOULD BE CUT!!!!!" One was cut (no
  caller for `dont_like_surpises`). The other, "home_in_this", still plays (`mak.gsc:2812`).
- `_quotes.gsc:149`: "for the 4/21/08 milestone the man wanted the death quotes out".
- `_vehicle.gsc`: "No longer needed, since we do not have helicopters".
- `playeranim.script`: "Vehicle animations are CUT".
- `mp_outskirts.gsc:50` and `mp_roundhouse.gsc:90`: patches closing "secret" rooms that were exploits.
- `ber2` public demo code (`IsCoopEPD`) with its own art: `ber2_epd_coming_soon`, `splash_screen_sp_ber2_epd`.

### Odd unused files
`weapons/sp/panzershark`, with display name `WEAPON_PANZERSHARK` and `viewmodel_panzershark`
(no text or model left). `compass_map_mp_dog_test`, a test map minimap. `WEAPON_POTATO`
("Potato"), `WEAPON_FIREBLOB` ("Napalm Blob"), `WEAPON_RUSSIAN_FLAG` ("Red Army Banner").

## Already documented (per fan wikis or TCRF snippets)
- **British campaign in Holland and Rhineland** (`hol1-3`, `rhi1-3`). This install has skies
  (`holland_1_dome_bkgd`, `holland_2_dome_bkgd`), Fallschirmjäger textures, British objective
  icons, Bren and Lee-Enfield HUD art.
- **Cut MP maps Atoll, Carrier, Beachhead, Cavern, Wetlands.** Loading screens and minimaps
  are here. Carrier and Cavern are stamped "Temp". Beachhead and Wetlands are still in
  `playlists.info`, in vehicle and tank playlists.
- **Kamikaze** cut killstreak (`kamikaze_gunner_mp`).
- Der Riese's audio Easter eggs (`phono_egg`, `meteor_egg`, `radio_egg`, flytrap).

## Not finished
- Sound aliases that exist but no script plays (the biggest remaining source of cut dialogue).
- Models and animations nothing uses.
- 2,411 images no name search matched. This count is inflated: names built at runtime
  (such as `mission_screen_*`) look unused. Only map art was reviewed.
- 359 weapon files were checked by name only; 37 had no match, mostly vehicle turrets.
- `xref_levels` and the unreferenced text list are noisy for `MENU_`, `WEAPON_` and `PERKS_`,
  since many are referenced through tables. Only the items named above were read individually.
