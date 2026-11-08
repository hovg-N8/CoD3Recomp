# CoD3 handoff — October 30, 2026 (Codex)

**UPDATE 2026-10-30 12:38 (Claude): 0.5.633 VALIDATED on Isle War (run_case.py isle-633 --primary 20 --snapshot):
override log 13->25 items, MG42 and arms drawn and firing, check_snapshot.py PASS on isle-633.json. Remaining:
Rouen control, 8-bot Isle check, bots third-person MG42. Logs: logs-backup/20261030-123552-isle-633-result.**

## Start here / exact status

USER REQUESTED WRAP-UP. No new tests are queued or to be launched this session.
Source: **0.5.633**. Installed version last read: **0.5.632**.
Build watcher status at handoff write:
```
2026-10-30T08:29:43
building: 0.5.633 handheld MG42 animation-table collision fix; no test request
```
**633 has NOT been runtime validated. Do not call the MG42 fixed yet.**
No game process was running at wrap-up. The original INI was restored and its
SHA256 matched the saved original byte-for-byte:
B78C601C42BCDCDC9F0075F329996EB5B012AE34C79D7CF72A9B90DBC12A2469.
Check build-status.txt and installed VERSION.txt before resuming. A build was
already in progress when user requested wrap-up; do not bump source mid-build.

## Paths / read order

Source: `D:\Call of Duty 3 (USA, Europe)\CoD3Recomp-main\CoD3Recomp-main`
Install/logs: `D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0`
Session tools, pre-change backups and runtime JSON: `C:\Users\hovgt\Documents\Codex\2026-10-30\hey\work\mg42-632`
Read CLAUDE.md, top of PICK-ME-UP.md, THIS handoff, then the older
CLAUDE-SESSION-2026-10-30.md and 2026-10-29.md for the remaining work.
The older Claude note ending631 is historical; this Codex note supersedes it.

## Root cause found, evidence preserved

The handheld MG42 was created under name `mg42`, borrowing MG34's gun/arms.
The alias in mp_fixes.cpp sub_820D1320 only ran when the original lookup missed.
**Isle already has a native animation-table entry named mg42 for a mounted gun.**
That hit bypassed the intended MG34 alias. Rouen has no conflicting entry.

Read-only snapshots using the installed PDB's Guest::Base symbol proved:
- Isle's native mg42 animation table: 13 items, **0 resolved animations**.
- Native mg34 and mg34_arms: 25 items, **24 resolved animations** each.
- Weapon definitions (names and scalar fields) are otherwise identical on
  working Rouen and failing Isle.
- MG42 drawing-cache animation rates at +32,+36,+40,+52,+56,+60,+64 are all ZERO
  on Isle; on Rouen they are positive and equal the MG34 donor's rates.
- Actual weapon vertex buffers and shaders ARE submitted on Isle. A roughly
  90-degree wrong pose places the model out of view / produces a giant slab.

Key runtime JSON files in the session-tools folder:
`isle-anims-full.json` (native table collision proof), `isle-live.json`
(bad rates), `rouen-live.json` (good rates). No raw memory was written to the game.
`check_snapshot.py` FAILS isle-live.json and PASSES rouen-live.json before the
source fix. This is the integration regression proof, not a mock test.

## Tests already done on632 — do not repeat these hypotheses

- Rouen CTF primary MG42: all five diagnostic stages render; restore works.
- Rouen CTF MG34 primary -> MG42 secondary: both render.
- Isle War FG42 -> MG42: absent gun/hands, with8 bots AND with0 bots.
- The plain stage (body replacement OFF, bipod suppression OFF) also fails on
  Isle. Therefore the initial inference 'the swap alone is the root cause' was
  disproved. The user's plain test used a class/map combination that drew.
- MG42 primary on Isle also fails, so FG42 switching is NOT required.
- Original LMG class3 alone: fails. Original class AND bipod animations
  (COD3_MG42_DEPLOY=1): fails. Those behaviors were not changed in the fix.
- Rouen WAR primary MG42 renders: **map-dependent collision**, not CTF vs War.
- No stability/60fps claim from these short diagnostic captures.

## 633 patch, not yet validated in play

Only product source touched: `CoD3Host/mp_fixes.cpp`.
- Wrap native view setup8245E720 (weapon index r3) and arms setup82465920
  (weapon index r4) in a thread-local scope ONLY for the appended handheld
  MG42 index recorded in g_campaignWeapon.
- Inside that scope, sub_820D1320 overrides an existing hit ONLY for
  mg42/mg42_arms, ONLY in the animation manager's table [[82BCBED8]+4].
- The existing MG34 alias then supplies handheld animations. Existing native
  mounted-gun hits outside that scope remain unchanged; the native table asset
  itself is not modified.
- Dedicated log `mg42 animations: handheld ... overrides native entry ...
  (13 items) with mg34 ... (25 items)` makes the path observable.
- Normal gun geometry, handling, grenade and killcam logic were not changed.

632 also added opt-in COD3_MG42_DIAG (retained): hold MG42 continuously to cycle
plain/body-only/bipod-only/both/plain every6seconds, with automatic screenshots,
bounded mesh metadata and next-frame logging2seconds into each stage. Per-stage
clock prevents slow captures skipping phases. Default behavior unchanged.

Source backups: `mp_fixes.before.cpp` (631), `mp_fixes.before633.cpp` (632),
PATCH-NOTES.before*.md. No Git repository; local backups and source diffs only.
PATCH-NOTES.md has632 diagnostic and633 fix entries.

## Next agent — finish this first

1. Confirm633 build success. If failed, inspect only compiler error ranges in
   build-mp-log.txt. Finish the existing patch before bumping another version.
2. Run the SAME Isle War test after preserving all4logs. Tests are explicitly
   authorized by the user. Example (direct EXE via helper, no .bat invocation):
   `python -I "C:\Users\hovgt\Documents\Codex\2026-10-30\hey\work\mg42-632\run_case.py" isle-633 --primary 20 --snapshot`
   The helper refuses concurrent games, archives logs+INI, selects MG42,
   restores the exact saved INI in finally and saves the resulting logs.
   It enables the diagnostic cycle. Do not leave a test running unattended if
   wrapping up; verify restoration again after it exits.
3. Keep game active; UI/focus can otherwise stall automated A/menu presses.
   Use Computer Use skill and @oai/sky. One click Auto Select and class if needed.
   Mouse-wheel scroll120 switches weapons reliably; quick F can be missed.
4. Verify a visible MG42 AND hands, proper aim, firing/reload, and switching to
   another weapon/back. Check dedicated13->25 override log.
5. Snapshot: `python -I "C:\Users\hovgt\Documents\Codex\2026-10-30\hey\work\mg42-632\snapshot_guest.py" <pid> isle-633-live`
   It dynamically resolves Guest::Base from CURRENT installed PDB; read-only
   OpenProcess0x410/ReadProcessMemory. Then run check_snapshot.py on that JSON.
   Expect all7rates equal the resolved MG34 donor. Native mounted mg42 table can
   still contain13items; the fix intentionally overrides lookup, not the asset.
6. One Rouen control and an8-bot Isle check are appropriate nearest regressions.
   Do NOT claim fixed from build success alone.

## Remaining priorities after MG42

- Killcam explosions: trace a real live grenade explosion. Recorded objects in
  previous3killcams had no explosion events; do not repeat the same event patch.
- Confirm631 grenade fix: no 'spent nothing twice'; one grenade per decision.
- Later: fill-screen smoke outline, intermittent startup stall, heap96%, CTF,
  co-op2PC checks as older handoffs describe. No work on those this session.

## Evidence locations / operating rules

All pre-run and result logs remain LOCAL in install/logs-backup. Important:
- 2026-10-30-codex-pickup-1793333525 (original630 failure log).
- 2026-10-30-mg42-primary-632; 2026-10-30-mg42-secondary-632.
- 2026-10-30-mg42-isle-bots-632; 2026-10-30-mg42-isle-nobots-632.
- 20261030-120511-native-deploy-result.
- 20261030-120833-isle-primary-constants-result.
- 20261030-121126-rouen-primary-constants-result.
- 20261030-121626-rouen-state-result; 20261030-121800-isle-state-result.
Other case archives have UTC timestamps and -before/-result suffixes.
Screenshots stay in install/screenshots. Original630 000021..000032;
successful632 003941/003946/003952/003958/004004; reproduced Isle004726/004731;
class3 failure080347. Frame-log captures can stall briefly / generate large logs.

ALWAYS run `python -I tools/mplog/mplog.py` FIRST, then targeted ranges/grep.
Never dump the full log or huge cpp files. Preserve all4rotating logs before runs.
Build via new PATCH-NOTES entry + build-request.txt; no .bat launches by agent.
Do not change execution policy. User started normal AUTO-BUILD.bat; watcher now
works. Old `built: ?` status came from a TEST-ONLY request, not an unknown exe.
No friend zip unless asked; no uploads/commits; never upload game data/generated
PPC/binaries/shaders/textures/logs. CPU target remains x86-64-v2, no AVX2.
User values efficient targeted investigation, no guesses, persistent handoff.
