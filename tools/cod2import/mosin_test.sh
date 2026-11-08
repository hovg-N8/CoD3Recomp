#!/bin/bash
# usage: flame_test.sh <case> [extra env...]   every bot on the scoped Mosin, mp_isle dm, overlay in the shots
CASE=$1; shift
G="/d/Call of Duty 3 (USA, Europe)/CoD3-PC-Port-0.4.0"
B="D:/Call of Duty 3 (USA, Europe)/carentan-work/bot_kits.user-backup.txt"
tasklist 2>/dev/null | grep -i cod3 && { echo "game running"; exit 1; }
cat > "$G/bot_kits.txt" <<'EOF'
# TEST (Claude, scoped Mosin 0.5.727): every bot on the scoped Mosin. The user's file is restored after.
1  rifleman   mosin_scoped 0 0
EOF
cd "/d/Call of Duty 3 (USA, Europe)/CoD3Recomp-main/CoD3Recomp-main/tools/cod2import" && python -I run_test.py "$CASE" --map mp_isle --seconds 100 --bots 8 ${PRIMARY:+--primary $PRIMARY} --env COD3_BIG_MODE=dm COD3_SHOT_OVERLAY=1 "COD3_SHOTS=25 33 41 49 57 65 73 81 89" "$@" 2>&1 | tail -1
cp -p "$B" "$G/bot_kits.txt"; sha1sum "$G/bot_kits.txt"
R=$(ls -d "$G"/logs-backup/*"$CASE"-result | tail -1); echo "$R"
