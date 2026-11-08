#!/bin/bash
# usage: runvariant.sh <variant letter> <case name>
# env: SECONDS_RUN (60), SHOTS (COD3_SHOTS=10 25 45), EXTRA_ENV (more KEY=VALUE),
#      BOTS (0), SCENE (1: Carentan's spawns/flags in the level scene, Rouen's
#      props, trees and effects off; 0: Rouen's scene as it is)
W="D:/Call of Duty 3 (USA, Europe)/carentan-work"
G="D:/Call of Duty 3 (USA, Europe)/CoD3-PC-Port-0.4.0/game/mp/rouen"
T="D:/Call of Duty 3 (USA, Europe)/CoD3Recomp-main/CoD3Recomp-main/tools"
V=$1; mkdir -p "$W/test$V"
DROP=""; [ "${DROP_ASSETS:-1}" = 1 ] && DROP="--drop-assets"   # Rouen's asset-only zone blocks out
[ "${DROP_CELLS:-0}" = 1 ] && DROP="$DROP --drop-source-cells"   # Rouen's own cells out of the directories (test)
cd "$T/kapf" || exit 1
python -I merge_zone.py "$G/zn01.cod" "$W/test$V/zn01.cod" mp_rouen_zone01 "$W/c2/cells$V/cells.pkl" "$W/c2/carentan_all.cgb" 1,2,3,4,5,6,8,9,10,11 --textures $DROP | tail -1 | cut -c1-200
python -I merge_zone.py "$G/zn02.cod" "$W/test$V/zn02.cod" mp_rouen_zone02 "$W/c2/cells$V/cells.pkl" "$W/test1/zn02.cgb" 7 $DROP | tail -1 | cut -c1-120
python -I merge_zone.py "$G/zn03.cod" "$W/test$V/zn03.cod" mp_rouen_zone03 "$W/c2/cells$V/cells.pkl" "$W/test1/zn03.cgb" 0 $DROP | tail -1 | cut -c1-120
LEVEL="$W/scene/carentan.cod.orig"
if [ "${SCENE:-1}" = 1 ]; then
    cd "$T/carentan" || exit 1
    python -I scene.py zone "$W/test$V/zn01.cod" "$W/test$V/zn02.cod" "$W/test$V/zn03.cod" | cut -c1-160
    python -I scene.py level "$W/scene/carentan.cod.orig" "$W/scene/carentan.cod" "$W/c2/mp_carentan.d3dbsp" | tail -1
    LEVEL="$W/scene/carentan.cod"
    # every BSP leaf in cell 001 (Carentan's one cell): ONE_CELL_BSP=0 keeps Rouen's
    [ "${ONE_CELL_BSP:-1}" = 1 ] && python -I scene.py onecell "$LEVEL" | tail -1
    # Rouen's grass (level.bgib) out of the level's directory: DROP_GRASS=0 keeps it
    [ "${DROP_GRASS:-1}" = 1 ] && python -I scene.py drop "$LEVEL" .bgib | tail -1
fi
python -I - "$W/test$V" "$LEVEL" <<'PY'
import os, shutil, sys
P=r"D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0\saves\content\00000002\Carentan\mp\carentan"
files=[(os.path.join(sys.argv[1],z+".cod"),z+".cod") for z in ("zn01","zn02","zn03")]+[(sys.argv[2],"carentan.cod")]
for src,name in files:
    tmp=os.path.join(P,name+".new"); shutil.copyfile(src,tmp); os.replace(tmp,os.path.join(P,name))
print("installed")
PY
sha1sum "$G"/zn0*.cod | diff - "$W/test1/rouen-sha1-before.txt" >/dev/null && echo "Rouen unchanged" || echo "ROUEN CHANGED"
cd "$T/cod2import" && python -I run_test.py "$2" --map carentan --seconds ${SECONDS_RUN:-60} --bots ${BOTS:-0} --env "${SHOTS:-COD3_SHOTS=10 25 45}" $EXTRA_ENV 2>&1 | tail -2
