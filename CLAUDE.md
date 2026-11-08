# CoD3 Recomp (Call of Duty 3 static-recomp PC port)

## Start here
- Read `docs/PICK-ME-UP.md` (top section only), then the newest `docs/CLAUDE-SESSION-*.md`.
  Don't read older sections or the whole transcript history unless the task needs them.
- Source: this folder. Install, logs and friend zips: `D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0`
  (`CoD3MP.log`, `CoD3MP.previous.log`, `MAKE-FRIEND-UPDATE.ps1`). Friend logs arrive as `Downloads\CoD3MP (N).log`.
- Fork: github.com/hovg-N8/CoD3Recomp. Upstream: FrankHUN88453/CoD3Recomp.

## Reading logs (save usage)
`CoD3MP.log` is 25+ MB. ALWAYS run the summary first, then grep only for what it points at:
```
python -I tools/mplog/mplog.py              # install log
python -I tools/mplog/mplog.py --previous   # the run before
python -I tools/mplog/mplog.py --friend     # newest Downloads\CoD3MP*.log
```
It reports: version, how the run ended (crash/watchdog), fps by minute, game-heap trend (0.5.479+),
pools that ran empty, guard hits, CTF bot actions, and the most common trouble lines.
Never cat or broadly grep the whole log into context; use `grep -c`, `sed -n 'A,Bp'`, `cut -c1-200`.

## Build and test loop
1. New `## 0.5.N:` entry at the top of `docs/PATCH-NOTES.md`.
2. Write `build-request.txt` in the source root (no test-request unless asked; don't run .bat files).
3. Wait for `build-status.txt` to match `^(built|failed): 0\.5\.N` (run the wait in the background).
4. ON HOLD since 2026-10-25 (user: solo tests for now, friend later): skip the zip unless asked.
   Make the friend zip (`D:\Call of Duty 3 (USA, Europe)\CoD3MP-update-0.5.N.zip`):
   `powershell -NoProfile -ExecutionPolicy Bypass -File "D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0\MAKE-FRIEND-UPDATE.ps1"`.
   Finish by stating the version and zip path.
5. The user and their friend play, then say "check the log".

Handoff (when asked): update `PICK-ME-UP.md`, `NEXT-SESSION.md`, `HANDOFF.md`, `SHARED-LOG.md`,
`BOT-PATCH-LOG.md`, and a new `docs/CLAUDE-SESSION-<date>.md`.

## Hard rules
- Never upload game data to GitHub: ISO, .cod/.xex, generated ppc/ppc_mp, textures, shaders, exes, logs.
- CPU target stays x86-64-v2, no AVX2 (friends' older PCs).
- Don't change the PowerShell execution policy.
- Don't "sync fork" with upstream. Port pieces by hand, mapping SP addresses to MP via
  `CoD3Host/mp_addresses.txt` and `tools/mpmatch/`.

## Code map
- `CoD3Host/mp_fixes.cpp`: multiplayer fixes, guards, heap watch (very large; grep, read by offset).
- `CoD3Host/bots.cpp`: bots (navigation, CTF, classes).
- `CoD3Host/kernel_video.cpp`: per-frame hooks.
