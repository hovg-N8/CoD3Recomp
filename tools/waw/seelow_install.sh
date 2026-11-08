#!/bin/bash
# Seelow (WaW's mp_seelow) into the install as its own map pack, the way Carentan
# is (tools/carentan/runvariant.sh): pack 5, ID 21, folder seelow, Rouen's zones as
# the harness with Seelow's cells (seelow_cells.py, C2_ONE_CELL=1), collision
# (seelow_clipmap.py, one .cgb a zone) and spawns/flags (scene.py level from
# mp_seelow.ents). Game data: everything stays local. Rouen's own files are
# checked unchanged at the end.
# NIGHT=1 (0.5.731): Seelow Night, its own pack beside it (pack 6, ID 22, folder
# seelownight, devmap seelownight): the cells' lightmaps moonlit (CELLS=cells2n:
# seelow_cells.py with SEELOW_NIGHT), and Longyard's night sky (its dome painting,
# clouds and fog ramp from the Bravo pack) painted into the sky textures
# (seelow_sky.py --night) in place of WaW's day sky.
# usage: seelow_install.sh            (env: CELLS=cells2m (static models, LOD materials fixed; cells1m before; cells1 without), COLL=coll_m (with the models' collision; coll without), DROP_GRASS=1, ONE_CELL_BSP=1, NIGHT=0)
W="D:/Call of Duty 3 (USA, Europe)/seelow-work"
G="D:/Call of Duty 3 (USA, Europe)/CoD3-PC-Port-0.4.0/game/mp/rouen"
C="D:/Call of Duty 3 (USA, Europe)/CoD3-PC-Port-0.4.0/saves/content/00000002"
T="D:/Call of Duty 3 (USA, Europe)/CoD3Recomp-main/CoD3Recomp-main/tools"
if [ "${NIGHT:-0}" = 1 ]; then
    PACK=SeelowNight; F=seelownight; I="$W/install_night"; CELLS="${CELLS:-cells2n}"
    PACKARGS="SeelowNight seelownight 6 22 Seelow_Night SEELOW_NIGHT SEELOW_HEIGHTS,_GERMANY"
else
    PACK=Seelow; F=seelow; I="$W/install"; CELLS="${CELLS:-cells2m}"
    PACKARGS="Seelow seelow 5 21 Seelow SEELOW SEELOW_HEIGHTS,_GERMANY"
fi
P="$C/$PACK/mp/$F"
mkdir -p "$I"
sha1sum "$G"/zn0*.cod "$G"/rouen.cod > "$I/rouen-sha1-before.txt"
if [ ! -f "$P/$F.cod" ]; then
    cd "$T/kapf" || exit 1
    python -I make_pack.py "$G" "$C" $PACKARGS | tail -3
fi
[ -f "$I/seelow.cod.orig" ] || cp "$P/$F.cod" "$I/seelow.cod.orig"
cd "$T/kapf" || exit 1
python -I merge_zone.py "$G/zn01.cod" "$I/zn01.cod" mp_rouen_zone01 "$W/$CELLS/cells.pkl" "$W/${COLL:-coll_m}/seelow_zn01.cgb" 1,2,3,4,5,6,8,9,10,11 --textures --drop-assets | tail -1 | cut -c1-200
python -I merge_zone.py "$G/zn02.cod" "$I/zn02.cod" mp_rouen_zone02 "$W/$CELLS/cells.pkl" "$W/${COLL:-coll_m}/seelow_zn02.cgb" 7 --drop-assets | tail -1 | cut -c1-200
python -I merge_zone.py "$G/zn03.cod" "$I/zn03.cod" mp_rouen_zone03 "$W/$CELLS/cells.pkl" "$W/${COLL:-coll_m}/seelow_zn03.cgb" 0 --drop-assets | tail -1 | cut -c1-200
cd "$T/carentan" || exit 1
# CoD3's own burning-vehicle fire (Rouen's vfire_panzerIV, its zone 1 scene's effect records) at
# WaW's five burning cars (its createfx mp_fire_medium spots, 2026-11-03): SCENE_FIRES="" for none.
# Moved onto each static_berlin_junkedcar's box middle (WaW's spots stood 9-37 off; the user asked them
# centred). Height: each car's origin - 15 (test fires2: the effect's flames start ~40 over its point, so
# at WaW's height, ~30 over the car's origin, they began at the roof; Rouen sets its vfire_panzerIV
# ~20-40 under the ground at its Panzer). WaW's own: 3206 1163 -272|2796 -1869 -283|364 2036 -89|
# 1973 4746 -84|4059 4723 -18
# 2026-11-04: under the cars, not on top (the user): each car's origin - 40, so the flames (from ~40 over
# the point) start at the ground under the car. (Was origin - 15: -314 -333 -132 -123 -65.)
SCENE_FIRES="${SCENE_FIRES-3240 1177 -339|2805 -1866 -358|374 2060 -157|1953 4754 -148|4079 4723 -90}" \
    python -I scene.py zone "$I/zn01.cod" "$I/zn02.cod" "$I/zn03.cod" | cut -c1-160
SCENE_CGB="$W/${COLL:-coll_m}/seelow_zn01.cgb|$W/${COLL:-coll_m}/seelow_zn02.cgb|$W/${COLL:-coll_m}/seelow_zn03.cgb" SCENE_GROUND="-450,300" SCENE_REACH=900 \
    python -I scene.py level "$I/seelow.cod.orig" "$I/seelow.cod" "$W/mp_seelow.ents" | tail -25
[ "${ONE_CELL_BSP:-1}" = 1 ] && python -I scene.py onecell "$I/seelow.cod" | tail -1
[ "${DROP_GRASS:-1}" = 1 ] && python -I scene.py drop "$I/seelow.cod" .bgib | tail -1
# 2026-11-04 (0.5.769): WaW's 10 tanks (5 Panzer IV, 5 Sherman for WaW's T-34s) as the level scene's own
# script_vehicle records (made from Rouen's sunk SpeedTree records): drivable as a map's own vehicles.
# The host brings their kinds (the vehicles' database pack, below) and models (sp\saint_lo\vehicles.cod)
# when vehicles.txt has Seelow lines. Day only (vehicles.txt has no seelownight lines). VEHICLES=0: none.
[ "${VEHICLES:-1}" = 1 ] && [ "${NIGHT:-0}" != 1 ] && python -I scene.py vehicles "$I/seelow.cod" "$W/mp_seelow.ents" | tail -3
# Rouen's dynamic collision (.dcgb: its doors' and breakables' brushes, as text, at Rouen's places:
# invisible walls in Seelow, the user 2026-11-02) out of every directory: DROP_DCGB=0 keeps them
if [ "${DROP_DCGB:-1}" = 1 ]; then
    for f in seelow zn01 zn02 zn03; do python -I scene.py drop "$I/$f.cod" .dcgb | tail -1; done
fi
# Seelow's own WaW sky (dome painting resampled into CoD3's sky dome, clouds swapped): SKY=0 keeps
# Rouen's. Not at night: Longyard's sky is grafted in below.
[ "${SKY:-1}" = 1 ] && [ "${NIGHT:-0}" != 1 ] && (cd "$T/waw" && python -I seelow_sky.py "$I/seelow.cod" "D:/Steam/steamapps/common/Call of Duty World at War/main" | tail -1)
# at night Longyard's night sky (its dome, clouds and fog ramp) painted into the same textures,
# and the zones' light grid (what lights the player's arms and gun, the soldiers) moonlit
[ "${NIGHT:-0}" = 1 ] && (cd "$T/waw" && python -I seelow_sky.py "$I/seelow.cod" --night "$C/Bravo Map Pack/mp/longyard/longyard.cod" | tail -5)
# day and night: Rouen's 9 street lamps out of the zones' light list (their coronas were the orange dot in the sky)
(cd "$T/waw" && python -I night_lightgrid.py --sink-lamps "$I/zn01.cod" "$I/zn02.cod" "$I/zn03.cod" | cut -c1-200)
[ "${NIGHT:-0}" = 1 ] && (cd "$T/waw" && python -I night_lightgrid.py "${LIGHTGRID_NIGHT:-0.25,0.3,0.5}" "$I/zn01.cod" "$I/zn02.cod" "$I/zn03.cod" | cut -c1-200)
# and the rest of the light grid's 150 lights (what lights the arms and gun, as Longyard's ~0.03: test arms_*)
[ "${NIGHT:-0}" = 1 ] && (cd "$T/waw" && python -I night_lightgrid.py --lights "${LIGHTS_NIGHT:-0.15,0.2,0.3}" "$I/zn01.cod" "$I/zn02.cod" "$I/zn03.cod" | cut -c1-200)
[ "${NIGHT:-0}" = 1 ] && (cd "$T/waw" && python -I night_lightgrid.py --fog "${FOG_NIGHT:-0.15,0.18,0.3}" "$I/seelow.cod" | cut -c1-200)
# and Longyard's sun (worldspawn suncolor and the rest): Rouen's suncolor was the tan haze (test fog2)
[ "${NIGHT:-0}" = 1 ] && (cd "$T/waw" && python -I night_lightgrid.py --sun "$C/Bravo Map Pack/mp/longyard/longyard.cod" "$I/seelow.cod" | cut -c1-200)
# and Longyard's fog (the fog pass's colour, near, far, strength): Rouen's tan fog was the haze (test fogpass)
[ "${NIGHT:-0}" = 1 ] && (cd "$T/waw" && python -I night_lightgrid.py --fogrec "$C/Bravo Map Pack/mp/longyard/longyard.cod" "$I/seelow.cod" | tail -1 | cut -c1-200)
# Rouen's props' textures out of the level archive (8x8 stand-ins of the same names) and its
# files repacked into fewer blocks: 18 -> 12 of the game's 96 banks (repack.py; REPACK=0 keeps it)
if [ "${REPACK:-1}" = 1 ]; then
    GM="D:/Call of Duty 3 (USA, Europe)/CoD3-PC-Port-0.4.0/game"
    OUTSIDE="$GM/mp/global.cod|$GM/mp/weapusge.cod|$GM/mp/anim.cod|$GM/mp/fend.cod|$GM/mp/loadscr.cod|$GM/sp/crssrds/vehicles.cod|$GM/sp/laison/vehicles.cod|$GM/sp/mace2/weapons.cod|$GM/sp/fuelplnt/weapons.cod|$GM/sp/forest/weapons.cod|$GM/sp/laison/weapons.cod|$GM/sp/forest/vehicles.cod|$GM/sp/laison/laison_mortar.cod"
    # the zones first: Rouen's renamed old cells, their meshes and materials, its old index,
    # collision and prop batches out of the blocks; its zone props' textures as stand-ins
    # (zn01 45 -> 39 blocks, zn02 and zn03 4 -> 1)
    for z in zn01 zn02 zn03; do
        (cd "$T/kapf" && python -I repack.py "$I/$z.cod" "$I/$z.repacked.cod" \
            --keep "^(mp_rouen_\d+_split|c_|w_|p_mp_|pfx_|sky)" --keep-tex "^fogramp$" --drop "^xp_rouen_" \
            --outside-archives "$OUTSIDE|$I/seelow.cod" \
            | grep -v "^outside: .* not read" | tail -3 | cut -c1-200) && mv -f "$I/$z.repacked.cod" "$I/$z.cod"
    done
    # (NIGHTSKY: Longyard's sky mesh grafted in (repack.py --graft) drew white, tests night1-3;
    # its textures are painted into Rouen's instead, above. Kept empty.)
    NIGHTSKY=()
    # the 4th collision file (seelow_clipmap.py writes 4: the zones' three hold 65535 mesh
    # index bytes each, full at 186000 with props in boxes) as the level's own unzoned.cgb,
    # read as Merville's whole collision is from its level archive (mp_merville.cgb)
    UNZONED=()
    [ -f "$W/${COLL:-coll_m}/seelow_zn04.cgb" ] && UNZONED=(--replace "unzoned.cgb=$W/${COLL:-coll_m}/seelow_zn04.cgb")
    (cd "$T/kapf" && python -I repack.py "$I/seelow.cod" "$I/seelow.repacked.cod" \
        --keep "^(c_|hc_|cg_|ng_|w_|p_mp_|pfx_|v_|sky|stand\.|radio)" --keep-tex "^fogramp$" --fli "mp\\$F\\$F.cod" \
        --fli-also "mp\\$F\\zn01.cod=$I/zn01.cod|mp\\$F\\zn02.cod=$I/zn02.cod|mp\\$F\\zn03.cod=$I/zn03.cod" \
        --outside-archives "$OUTSIDE" "${NIGHTSKY[@]}" "${UNZONED[@]}" \
        | grep -v "^outside: .* not read" | tail -8 | cut -c1-300) && mv -f "$I/seelow.repacked.cod" "$I/seelow.cod"
fi
python -I - "$I" "$P" "$F" <<'PY'
import os, shutil, sys
src, dst, folder = sys.argv[1], sys.argv[2], sys.argv[3]
for name in ("zn01.cod", "zn02.cod", "zn03.cod", "seelow.cod"):
    target = folder + ".cod" if name == "seelow.cod" else name
    tmp = os.path.join(dst, target + ".new")
    shutil.copyfile(os.path.join(src, name), tmp)
    os.replace(tmp, os.path.join(dst, target))   # (a hard link to Rouen's is replaced, not written through)
print("installed into", dst)
PY
# The vehicles' database pack (2026-11-04): Merville's game database alone (its VEHICLEFILE /
# VEHICLEPHYSICSFILE entries: the MP vehicle kinds), 0.8 MB, no level of its own (Merville's whole
# archive beside a map made itself the level: the map's world vanished). The host loads it, and
# Saint-Lo's vehicles.cod for the models, beside a map with vehicles.txt lines. Game data: install only.
VM="D:/Call of Duty 3 (USA, Europe)/CoD3-PC-Port-0.4.0/game/mp/mp_merv"
if [ "${VEHICLES:-1}" = 1 ] && [ ! -f "$VM/mp_merv_vehicles.cod" ]; then
    (cd "$T/kapf" && python -I repack.py "$VM/mp_merv.cod" "$VM/mp_merv_vehicles.cod" --keep '^$' \
        --drop '^(?!.*\.(gdbb|dbb|cfgstrb)$)' --follow-imports | tail -2 | cut -c1-200)
fi
# The vehicles' effects pack (0.5.773): shell explosions tank_*, the tanks' muzzle flashes, exhaust,
# tread/wheel/peel-out dust, damage, fire, destruction; 5.0 MB, no database. Its own archive, in every
# lookup list (the shells' explosions are looked up in the map's); the database pack must stay out of
# the map's lists (0.5.771: the map then read all 24 of Merville's weapons). Only the tanks' mf_: with
# every mf_ the players' first-person flashes came too and the Mosin drew green (seelowfx2).
if [ "${VEHICLES:-1}" = 1 ] && [ ! -f "$VM/mp_merv_vehiclefx.cod" ]; then
    (cd "$T/kapf" && python -I repack.py "$VM/mp_merv.cod" "$VM/mp_merv_vehiclefx.cod" --keep '^$' \
        --drop '^(?!(tank_|mf_panzeriv|mf_sherman|mf_20_armour|exhaust_|treadfx_|peeloutfx_|vdamage_|vexplode_|vfire_|wheelfx))' \
        --follow-imports | tail -2 | cut -c1-200)
    # (2026-11-06, 0.5.815-820) its FLI list emptied; and the copy the host loads now, from
    # sp\vehfx (g_vehicleFxFile), with every list but MAIN's emptied. The tank shells' explosions on
    # Seelow are this level's artillery effects (0.5.820): no extra archive ever registers an effect.
    python -I "$T/kapf/kapf_lists.py" fli "$VM/mp_merv_vehiclefx.cod" "$VM/mp_merv_vehiclefx.cod.tmp" | tail -1 && \
        mv -f "$VM/mp_merv_vehiclefx.cod.tmp" "$VM/mp_merv_vehiclefx.cod"
fi
VFX="D:/Call of Duty 3 (USA, Europe)/CoD3-PC-Port-0.4.0/game/sp/vehfx"
if [ "${VEHICLES:-1}" = 1 ] && [ ! -f "$VFX/mp_merv_vehiclefx.cod" ]; then
    mkdir -p "$VFX"
    python -I "$T/kapf/kapf_lists.py" main "$VM/mp_merv_vehiclefx.cod" "$VFX/mp_merv_vehiclefx.cod" | tail -1
fi
# Saint-Lo's vehicles.cod cut (0.5.791): the Panzer IV, the Sherman, the guns, the shell, the shaders and
# every texture they use; the P-47, the Fw 190's parts and the prisoners' walks out; 7 blocks of 8, 17.5 MB.
# (0.5.785's tighter cut, the Panzer alone with --keep '(panzer|tankshell)', drew the tanks invisible and
# the 50 cal grey: the user, 0.5.790.) Game data: install only; delete vehicles_panzer.cod to rebuild it.
SL="D:/Call of Duty 3 (USA, Europe)/CoD3-PC-Port-0.4.0/game/sp/saint_lo"
if [ "${VEHICLES:-1}" = 1 ] && [ ! -f "$SL/vehicles_panzer.cod" ]; then
    (cd "$T/kapf" && python -I repack.py "$SL/vehicles.cod" "$SL/vehicles_panzer.cod" --keep '.' \
        --drop '(p47|fw190|f190|prisoner|patrolwalk|propeller)' \
        --follow-imports | tail -2 | cut -c1-200)
fi
# The Stuka's cut of Crossroads' vehicles.cod (0.5.790): the UAV's Stuka, the guns and the weapons'
# textures; its Sherman (Laison's has one), Kangaroo textures, A-20 and C-47 out; 3 blocks of 5, 4.1 MB.
# Loaded on every map: about 6 MB of the game heap back (Seelow after Merville ran it out). Install only.
CR="D:/Call of Duty 3 (USA, Europe)/CoD3-PC-Port-0.4.0/game/sp/crssrds"
if [ ! -f "$CR/vehicles_stuka.cod" ]; then
    (cd "$T/kapf" && python -I repack.py "$CR/vehicles.cod" "$CR/vehicles_stuka.cod" --keep '.' \
        --drop '(sherman|kangaroo|v_a20|a20_|c47)' --follow-imports | tail -2 | cut -c1-200)
fi
# WaW's mantle helpers for the host's mantle (0.5.729, mp_fixes.cpp): custom\mantle\<folder>.txt
(cd "$T/waw" && python -I seelow_mantle.py "$W/mp_seelow.zone" "$C/../../../custom/mantle/$F.txt" | tail -1)
sha1sum -c "$I/rouen-sha1-before.txt" >/dev/null && echo "Rouen unchanged" || echo "ROUEN CHANGED"
