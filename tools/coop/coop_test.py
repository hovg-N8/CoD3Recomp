"""Two copies of the campaign on this PC: a co-op host and a joining copy.

usage: python -I coop_test.py TAG SECONDS [--map forest] [--host-pad "..."] [--join-pad "..."]
                              [--host-shots "t t"] [--join-shots "t t"] [--env K=V ...] [--host-env K=V] [--join-env K=V]
Logs: <out>/TAG/host.log, join.log; screenshots copied to <out>/TAG/shots.
"""
import argparse, os, shutil, subprocess, sys, time, datetime, glob

INSTALL = r"D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0"
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "coop-out")

ap = argparse.ArgumentParser()
ap.add_argument("tag"); ap.add_argument("seconds", type=int)
ap.add_argument("--map", default="forest")
ap.add_argument("--host-pad", default=""); ap.add_argument("--join-pad", default="")
ap.add_argument("--host-shots", default=""); ap.add_argument("--join-shots", default="")
ap.add_argument("--env", action="append", default=[])
ap.add_argument("--host-env", action="append", default=[])
ap.add_argument("--join-env", action="append", default=[])
ap.add_argument("--join-delay", type=int, default=40)
ap.add_argument("--solo", action="store_true")   # the host alone (route finding: half the heat)
ap.add_argument("--played", action="store_true")   # the user plays the joining copy (see env_for)
ap.add_argument("--desk-shots", default="")   # seconds from the host's start: the whole desktop (both windows side by side)
a = ap.parse_args()

if "CoD3" in subprocess.run("tasklist", capture_output=True, text=True).stdout:
    sys.exit("a CoD3 process is running: not started")

out = os.path.join(OUT, a.tag); os.makedirs(os.path.join(out, "shots"), exist_ok=True)
stamp = datetime.datetime.now().strftime("%Y%m%d-%H%M%S")
backup = os.path.join(INSTALL, "logs-backup", stamp + "-coop-" + a.tag + "-before"); os.makedirs(backup, exist_ok=True)
for n in ("CoD3.log", "CoD3.previous.log", "CoD3MP.log", "CoD3MP.previous.log"):
    p = os.path.join(INSTALL, n)
    if os.path.exists(p): shutil.copy2(p, backup)
shots_dir = os.path.join(INSTALL, "screenshots")
before = set(os.listdir(shots_dir)) if os.path.isdir(shots_dir) else set()

def env_for(mode, pad, shots, extra):
    e = dict(os.environ)
    e.update({"COD3_SPINWAIT": "0", "COD3_COOP": mode, "COD3_BACKGROUND": "1", "COD3_SHOT_OVERLAY": "1"})
    if mode == "host": e.update({"COD3_COOP_MAP": a.map, "COD3_WINDOW_RECT": "0,40,960,600"})
    else: e.update({"COD3_COOP_HOSTIP": "127.0.0.1", "COD3_WINDOW_RECT": "960,40,960,600"})
    # --played: the user drives the joining copy (keyboard/mouse when its window is in front, or a pad);
    # the host plays itself (COD3_BACKGROUND=1 ignores keyboard and mouse, so the joiner must not have it)
    if a.played:
        if mode == "host": e["COD3_NOPAD"] = "1"
        else: e["COD3_BACKGROUND"] = "0"
    if a.solo: e["COD3_COOP_SOLO"] = "1"
    if pad: e["COD3_PAD"] = pad
    if shots: e["COD3_COOP_SHOTS"] = shots
    for kv in a.env + extra:
        k, v = kv.split("=", 1); e[k] = v
    return e

exe = os.path.join(INSTALL, "CoD3.exe")
hl = open(os.path.join(out, "host.log"), "wb"); jl = open(os.path.join(out, "join.log"), "wb")
t0 = time.time()
host = subprocess.Popen([exe], cwd=INSTALL, env=env_for("host", a.host_pad, a.host_shots, a.host_env), stdout=hl, stderr=subprocess.STDOUT)
class _NoJoin:
    def poll(self): return 0
if a.solo: join = _NoJoin()
else:
    time.sleep(a.join_delay)
    join = subprocess.Popen([exe], cwd=INSTALL, env=env_for("join", a.join_pad, a.join_shots, a.join_env), stdout=jl, stderr=subprocess.STDOUT)
desk = sorted(float(x) for x in a.desk_shots.split()) if a.desk_shots else []
grab = None
if desk:
    try:
        from PIL import ImageGrab
        grab = ImageGrab.grab
    except Exception as ex:
        print("no desktop shots:", ex); desk = []
while time.time() - t0 < a.seconds:
    if desk and time.time() - t0 >= desk[0]:
        t = desk.pop(0)
        try: grab(all_screens=True).save(os.path.join(out, "shots", "desk-%06.1f.png" % t))
        except Exception as ex: print("desktop shot failed:", ex)
        continue
    time.sleep(0.05 if desk and desk[0] - (time.time() - t0) < 2.1 else 2)
    if host.poll() is not None and join.poll() is not None: break
print("host exit:", host.poll(), "join exit:", join.poll(), "after %.0f s" % (time.time() - t0))
subprocess.run("taskkill /F /IM CoD3.exe", capture_output=True, shell=True); subprocess.run("taskkill /F /IM CoD3MP.exe", capture_output=True, shell=True)
time.sleep(2)
hl.close(); jl.close()
for n in ("coop-host-relaunch.log", "coop-join-relaunch.log", "coop-join-reload.log"):
    p = os.path.join(INSTALL, n)
    if os.path.exists(p) and os.path.getmtime(p) > t0: shutil.copy2(p, out)
if os.path.isdir(shots_dir):
    for n in sorted(set(os.listdir(shots_dir)) - before): shutil.move(os.path.join(shots_dir, n), os.path.join(out, "shots", n))
print("out:", out, "shots:", len(os.listdir(os.path.join(out, "shots"))))
