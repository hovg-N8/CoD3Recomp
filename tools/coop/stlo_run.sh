#!/usr/bin/env bash
# Saint-Lo's training by script, the host alone (or with a joining copy: JOIN=1).
# usage: bash tools/coop/stlo_run.sh TAG SECONDS [extra guarded_test args...]
# Shots every 3 s from 226 to SECONDS-10. Prints the objectives seen and the test aids' lines.
set -e
cd "$(dirname "$0")/../.."
tag=$1; secs=$2; shift 2
route=$(python -I tools/coop/stlo_route.py --upto "${UPTO:-truck}")
GO=$(echo "$route" | sed -n '1p' | cut -d= -f2-)
PAD=$(echo "$route" | sed -n '2p' | cut -d= -f2-)
AIM=$(echo "$route" | sed -n '3p' | cut -d= -f2-)
shots=$(python -I -c "print(' '.join(str(t) for t in range(${SHOTS_FROM:-226}, $secs - 10, ${SHOTS_EVERY:-3})))")
solo="--solo"; [ "${JOIN:-0}" = "1" ] && solo=""
# GOD=0: no health top-up (the aid is on whenever the variable exists, so it is left out)
god="--env COD3_COOP_TESTGOD=1"; [ "${GOD:-1}" = "0" ] && god=""
python -I tools/coop/guarded_test.py "$tag" "$secs" $solo --map saint_lo --env COD3_SKIPFILMS=1 --env COD3_NOFILMS=1 \
  --env COD3_UNLOCKFPS=0 --env COD3_SCALE=0.5 --env COD3_NOPAD=1 $god \
  --host-env "COD3_COOP_TESTGO=$GO" --host-env "COD3_COOP_TESTAIMMODEL=$AIM" --host-env COD3_COOP_TESTHITMODEL=1 --host-pad "$PAD" --host-shots "$shots" "$@" 2>&1 | tail -2
grep -h "solo: server command\|server command to the joining copy too: object\|coop: test: aiming\|test: put at" "tools/coop/coop-out/$tag/host.log" | cut -c1-170
