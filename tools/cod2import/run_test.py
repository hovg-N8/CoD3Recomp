"""One unattended local test of CoD3MP (the Webley work): the logs and the ini
backed up first, the class set for the test, the ini put back byte for byte.

usage: python -I run_test.py <case> [--map mp_isle] [--seconds 90] [--bots 0]
                             [--primary N] [--sidearm N] [--special N] [--env KEY=VALUE ...]
Results: <game>/logs-backup/<stamp>-<case>-result (CoD3MP.log and the
screenshots taken during the run).
"""
import argparse
import datetime
import os
import re
import shutil
import subprocess
import time
from pathlib import Path

GAME = Path(r"D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0")

ap = argparse.ArgumentParser()
ap.add_argument("case")
ap.add_argument("--map", default="mp_isle")
ap.add_argument("--seconds", type=int, default=90)
ap.add_argument("--bots", type=int, default=0)
ap.add_argument("--primary", type=int)
ap.add_argument("--sidearm", type=int)
ap.add_argument("--special", type=int)   # 0.5.739: class 1's special grenade (1: the signal flare)
ap.add_argument("--env", nargs="*", default=[])
args = ap.parse_args()

running = subprocess.run(["tasklist", "/FI", "IMAGENAME eq CoD3MP.exe", "/FO", "CSV", "/NH"], capture_output=True).stdout.lower()
if b'"cod3mp.exe"' in running:
    raise SystemExit("CoD3MP.exe is running: not starting another")
stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
before = GAME / "logs-backup" / f"{stamp}-{args.case}-before"
before.mkdir(parents=True)
for name in ("CoD3MP.log", "CoD3MP.previous.log", "CoD3.log", "CoD3.previous.log", "CoD3Recomp.ini"):
    if (GAME / name).exists():
        shutil.copy2(GAME / name, before / name)
ini = GAME / "CoD3Recomp.ini"
original = ini.read_bytes()
text = original.decode("utf-8-sig")
text = re.sub(r"(?m)^active_class = \d+", "active_class = 1", text)
if args.primary is not None:
    text = re.sub(r"(?m)^class_1_primary = \d+", f"class_1_primary = {args.primary}", text)
if args.sidearm is not None:
    text = re.sub(r"(?m)^class_1_sidearm = \d+", f"class_1_sidearm = {args.sidearm}", text)
if args.special is not None:
    if re.search(r"(?m)^class_1_special = \d+", text):
        text = re.sub(r"(?m)^class_1_special = \d+", f"class_1_special = {args.special}", text)
    else:
        text = re.sub(r"(?m)^(class_1_grenades = \d+)", lambda m: f"{m.group(1)}\nclass_1_special = {args.special}", text)
shots_dir = GAME / "screenshots"
shots_before = set(shots_dir.glob("*.png")) if shots_dir.exists() else set()
env = os.environ.copy()
for key in tuple(env):
    if key.startswith("COD3_"):
        del env[key]
env.update(COD3_PLAYERS="32", COD3_BIG_BOTS=str(args.bots), COD3_BIG_SECONDS=str(args.seconds),
           COD3_BIG_MODE="war", COD3_BIG_TEST=args.map)
for pair in args.env:
    k, _, v = pair.partition("=")
    env[k] = v
result = GAME / "logs-backup" / f"{stamp}-{args.case}-result"
try:
    ini.write_text(text, encoding="utf-8")
    started = time.time()
    proc = subprocess.Popen([str(GAME / "CoD3MP.exe")], cwd=GAME, env=env)
    print(f"{args.case}: PID {proc.pid}, {args.map}, {args.seconds} s", flush=True)
    try:
        proc.wait(timeout=args.seconds + 240)
    except subprocess.TimeoutExpired:
        proc.kill()
        print(f"{args.case}: did not end by itself; ended", flush=True)
    print(f"{args.case}: ran {time.time() - started:.0f} s, exit {proc.returncode}", flush=True)
finally:
    ini.write_bytes(original)
    result.mkdir(exist_ok=True)
    for name in ("CoD3MP.log", "CoD3MP.previous.log"):
        if (GAME / name).exists():
            shutil.copy2(GAME / name, result / name)
    if shots_dir.exists():
        for shot in sorted(set(shots_dir.glob("*.png")) - shots_before):
            shutil.copy2(shot, result / shot.name)
    print(f"{args.case}: ini put back; result {result}", flush=True)
