"""Runs tools/coop/coop_test.py with the arguments given, watching the graphics card's temperature:
at LIMIT degrees or more the game is killed. Prints the temperatures seen."""
import subprocess, sys, time

LIMIT = 84
root = r"D:\Call of Duty 3 (USA, Europe)\CoD3Recomp-main\CoD3Recomp-main"


def temp():
    try:
        out = subprocess.run(["nvidia-smi", "--query-gpu=temperature.gpu", "--format=csv,noheader"], capture_output=True, text=True, timeout=10).stdout
        return int(out.strip().splitlines()[0])
    except Exception:
        return -1


start = temp()
print("gpu before:", start)
p = subprocess.Popen([sys.executable, "-I", root + r"\tools\coop\coop_test.py"] + sys.argv[1:], cwd=root)
seen = []
killed = False
while p.poll() is None:
    time.sleep(5)
    t = temp()
    seen.append(t)
    if t >= LIMIT and not killed:
        killed = True
        print("gpu at", t, "after", len(seen) * 5, "s: the game is stopped")
        subprocess.run("taskkill /F /IM CoD3.exe", capture_output=True, shell=True)
print("gpu every 20 s:", " ".join(str(x) for x in seen[::4]))
print("gpu most:", max(seen) if seen else -1, "killed" if killed else "not killed")
