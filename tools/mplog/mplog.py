"""Boil a CoD3 multiplayer log (CoD3MP.log, often 25+ MB) down to ~60 lines.

Usage:
  python tools/mplog/mplog.py                 # the install's CoD3MP.log
  python tools/mplog/mplog.py --previous      # CoD3MP.previous.log
  python tools/mplog/mplog.py --friend        # newest Downloads\\CoD3MP*.log
  python tools/mplog/mplog.py some.log [more.log ...]

Read this first, then grep the log only for what it points at.
"""
import collections
import glob
import os
import re
import statistics
import sys

INSTALL = r"D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0"
DOWNLOADS = os.path.join(os.path.expanduser("~"), "Downloads")

TICK = re.compile(r"^\[ *(\d+)s\] fps (\d+) .*?allocated ([\d.]+) MB")
STATS = re.compile(r"^stats: ([\d.]+) fps, frame ([\d.]+) ms \(worst ([\d.]+)\)")
HEAP = re.compile(r"^heap: game heap ([\d.]+) of ([\d.]+) MB used \((\d+)%\), peak ([\d.]+), ([+-][\d.]+) MB in 30 s")
HEAPS = re.compile(r"^heaps: physical ([\d.]+) of ([\d.]+) MB")
PERF = re.compile(r"^perf: a game frame: .*?; (\d+) frames, (\d+) bots; main thread CPU ([\d.]+) ms")
HITCH = re.compile(r"^hitch: ([\d.]+) ms between swaps")
EMPTY = re.compile(r"\[(\w+) : pool[^\]]*is empty\]")
CTF = re.compile(r"^bots: (?:slot \d+ plays ctf: (.+?)(?: \(flag\))?$|slot \d+ (presses use on the \w+ flag)|slot \d+ on the enemy flag \(ctf\): (.+)$|ctf: (\d+) spawns moved)")
# Exceptions 0x406D1388 (thread naming) and 0x40010006/0x4001000A (debug print) are not crashes.
CRASH = re.compile(r"Unhandled|C0000005|access violation|Guest fault|watchdog|fatal", re.I)
CRASH_NOISE = re.compile(r"0x406D1388|0x4001000[6A]")
TROUBLE = re.compile(r"\b(failed|error|refused|overflow|stuck|timed out|skipped)\b", re.I)
# Routine once-a-second status lines that merely contain "failed"/"refused"/"skipped" counters.
TROUBLE_NOISE = re.compile(r"^(bots: native error (enqueue|drain)|bots: Pmove|hitch:|audio:|render:|xma:|\s+skipped:)")
WATCH = ("heapwait:", "fxguard:", "killcam guard:", "flagwatch: ", "ctf sides", "TAKEN by", "has the flag", "SLIDES", "scriptvars", "a hop toward", "the title reads the depth", "prompts: the button icon font")


def pct(values, p):
    s = sorted(values)
    return s[min(len(s) - 1, int(len(s) * p))] if s else 0


def norm(line):
    return re.sub(r"\b[0-9A-F]{8}\b|-?\d+(\.\d+)?", "#", line)[:110]


def summarize(path):
    out = []
    say = out.append
    size = os.path.getsize(path)
    version, last_tick, ticks = "?", None, []
    stats, heap, heaps, perf = [], [], [], []
    hitches = 0
    pools = collections.Counter()
    pool_first = {}
    ctf = collections.Counter()
    crashes, watched = [], collections.defaultdict(list)
    trouble = collections.Counter()
    tail = collections.deque(maxlen=4)
    now = 0

    with open(path, encoding="utf-8", errors="replace") as f:
        for n, line in enumerate(f, 1):
            line = line.rstrip("\n")
            if line:
                tail.append(f"{n}: {line[:160]}")
            if n < 5 and "version" in line:
                version = line.rsplit("version", 1)[-1].strip()
            m = TICK.match(line)
            if m:
                now = int(m.group(1))
                last_tick = (now, float(m.group(3)))
                ticks.append((now, int(m.group(2))))
                continue
            m = STATS.match(line)
            if m:
                stats.append((now, float(m.group(1)), float(m.group(3))))
                continue
            m = HEAP.match(line)
            if m:
                heap.append((now, n, float(m.group(1)), float(m.group(2)), int(m.group(3)), float(m.group(5))))
                continue
            m = HEAPS.match(line)
            if m:
                heaps.append((now, float(m.group(1)), float(m.group(2))))
                continue
            m = PERF.match(line)
            if m:
                perf.append((int(m.group(2)), float(m.group(3))))
                continue
            m = HITCH.match(line)
            if m:
                if float(m.group(1)) >= 50:
                    hitches += 1
                continue
            m = EMPTY.search(line)
            if m:
                pools[m.group(1)] += 1
                pool_first.setdefault(m.group(1), (n, now))
                continue
            m = CTF.match(line)
            if m:
                if m.group(4):
                    ctf["spawns moved to own flag (running total)"] = int(m.group(4))
                else:
                    ctf[m.group(1) or m.group(2) or "on the enemy flag: " + m.group(3)] += 1
                continue
            if CRASH.search(line) and not CRASH_NOISE.search(line):
                crashes.append(f"{n} @{now}s: {line[:180]}")
            for w in WATCH:
                if w in line:
                    watched[w].append(n)
            if TROUBLE.search(line) and not TROUBLE_NOISE.match(line):
                trouble[norm(line)] += 1

    say(f"== {path}")
    dur = f"{last_tick[0] // 60}m{last_tick[0] % 60:02d}s" if last_tick else "?"
    say(f"version {version}; {size / 1048576:.1f} MB, {n} lines; ran {dur}")

    say("-- how it ended")
    if crashes:
        for c in crashes[:6]:
            say("  " + c)
    else:
        say("  no crash, hang or watchdog line")
    for t in tail:
        say("  last " + t)

    say("-- frame rate (stats lines, once a second)")
    live = [s for s in stats if s[0] > 20]  # skip loading
    if live:
        fps = [s[1] for s in live]
        say(f"  median {statistics.median(fps):.0f} fps, worst 5% {pct(fps, 0.05):.0f}, min {min(fps):.0f}; "
            f"{sum(f < 30 for f in fps)} s under 30, {sum(f < 50 for f in fps)} s under 50; {hitches} swaps of 50+ ms")
        by_min = collections.defaultdict(list)
        for t, f_, _ in live:
            by_min[t // 60].append(f_)
        say("  by minute: " + " ".join(f"{m}:{statistics.median(v):.0f}" for m, v in sorted(by_min.items())))
    if perf:
        say(f"  bots {perf[-1][0]}; main thread CPU {statistics.median(p[1] for p in perf):.1f} ms median, max {max(p[1] for p in perf):.1f}")

    say("-- memory")
    if heap:
        first, last = heap[0], heap[-1]
        peak = max(heap, key=lambda h: h[2])
        say(f"  game heap: {first[2]:.0f} MB @{first[0]}s -> {last[2]:.0f} of {last[3]:.0f} MB ({last[4]}%) @{last[0]}s; peak {peak[2]:.0f} @{peak[0]}s")
        mins = max(1, (last[0] - first[0]) / 60)
        say(f"  growth {(last[2] - first[2]) / mins:+.1f} MB a minute on average")
        jumps = sorted(heap, key=lambda h: -h[5])[:3]
        say("  biggest 30 s jumps: " + ", ".join(f"{h[5]:+.1f} MB @{h[0]}s (line {h[1]})" for h in jumps))
        step = max(1, len(heap) // 10)
        say("  trend: " + " ".join(f"{h[0] // 60}m:{h[2]:.0f}" for h in heap[::step]))
    else:
        say("  no 'heap: game heap' lines (before 0.5.479)")
    if heaps:
        say(f"  physical {heaps[0][1]:.0f} -> {heaps[-1][1]:.0f} of {heaps[-1][2]:.0f} MB handed out")
    if last_tick:
        say(f"  host allocated {last_tick[1]:.0f} MB at the end")

    say("-- pools and guards")
    if pools:
        for p, c in pools.most_common():
            say(f"  {p} pool empty {c}x, first line {pool_first[p][0]} @{pool_first[p][1]}s")
    else:
        say("  no pool ran empty")
    for w in WATCH:
        lines = watched.get(w)
        say(f"  {w.rstrip(':')}: " + (f"{len(lines)}x, first line {lines[0]}" if lines else "none"))

    say("-- CTF bots")
    if ctf:
        for k, c in ctf.most_common(10):
            say(f"  {c:5d}  {k}")
    else:
        say("  no CTF bot lines (not a CTF match?)")

    say("-- other trouble lines (most common shapes)")
    for k, c in trouble.most_common(8):
        say(f"  {c:5d}  {k}")
    return out


def main(argv):
    paths = [a for a in argv if not a.startswith("--")]
    if "--friend" in argv:
        logs = glob.glob(os.path.join(DOWNLOADS, "CoD3MP*.log"))
        if not logs:
            sys.exit("no CoD3MP*.log in Downloads")
        paths.append(max(logs, key=os.path.getmtime))
    if "--previous" in argv:
        paths.append(os.path.join(INSTALL, "CoD3MP.previous.log"))
    if not paths:
        paths = [os.path.join(INSTALL, "CoD3MP.log")]
    for p in paths:
        print("\n".join(summarize(p)))
        print()


if __name__ == "__main__":
    main(sys.argv[1:])
