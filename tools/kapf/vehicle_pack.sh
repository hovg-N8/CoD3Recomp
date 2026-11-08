#!/bin/bash
# The vehicles' pack (0.5.730): Merville's archive cut to its vehicles (Sherman, Panzer IV,
# Horch, WC51, BMW, the Harley's model, the mounted guns), their textures, track and damage
# effects, shaders, skeletons, drive animations, and Merville's game database (the
# VEHICLEFILE / VEHICLEPHYSICSFILE entries the host reads), repacked.
# The host loads it beside any map without vehicles of its own that has vehicles saved for
# it (vehicles.txt) or with vehicles_everywhere 1, in place of Merville's whole archive.
# 0.5.731: with Merville's scene made a scene of vehicles alone (vehicle_scene.py): a map's
# vehicles drive only when they are a scene's own. And with Merville's model register
# (.ng.xmbb / .ng.xmpbb) and every model it names (152: the vehicles, the soldiers' parts,
# Merville's props and brush models): without the register a vehicle's model instance was
# garbage (tests seelow20/21tanks, rouenB_pack: crash), with it but not its models the load
# hung (rouenB2_pack). The soldiers' textures are left to the map's own (--external Rouen's:
# every level archive has them), Merville's props' as 8x8 stand-ins (never placed): 22 blocks.
# Game data: written into the install's game folder, never the repository.
GM="D:/Call of Duty 3 (USA, Europe)/CoD3-PC-Port-0.4.0/game/mp"
cd "$(dirname "$0")" || exit 1
REG=$(python -I - "$GM/mp_merv/mp_merv.cod" <<'PY'
import re, sys
sys.path.insert(0, ".")
import kapf, apkf
parts = kapf.decompose(kapf.Archive(sys.argv[1]))
listed = {e[0].lower() for e in parts["sections"][0]["entries"]}
names = set()
for blk in parts["sections"][0]["blocks"]:
    p = apkf.read(blk["data"])
    for f in p.files:
        if f["name"].lower().endswith((".ng.xmbb", ".ng.xmpbb")):
            names |= {m.decode().lower() for m in re.findall(rb"[A-Za-z0-9_$.]{3,}", p.cpu(f))}
print("|".join(re.escape(n) + r"\.xemesh" for n in sorted(names) if n + ".xemesh" in listed))
PY
)
KEEP='^(v_|vg_)|\.(gdbb|dbb|cfgstrb|scnb|xefx|xeskel|xeanim|ng\.xmbb|ng\.xmpbb)$|treadfx|wheelfx|peelout|vexplode|vfire|vdamage|exhaust|tank_|^mf_|tankshell|^hint_'
python -I repack.py "$GM/mp_merv/mp_merv.cod" "$GM/mp_merv/mp_merv_vehicles.cod.new" --keep "^(v_|vg_|tankshell|c_)" \
    --keep-tex '^hint_|tankshell' --drop "^(?!.*($KEEP|^($REG)$))" --follow-imports \
    | tail -4 | cut -c1-200 \
    && python -I vehicle_scene.py "$GM/mp_merv/mp_merv_vehicles.cod.new" \
    && mv -f "$GM/mp_merv/mp_merv_vehicles.cod.new" "$GM/mp_merv/mp_merv_vehicles.cod"
