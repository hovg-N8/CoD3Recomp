# Claude session 2026-11-12 evening .. 2026-11-13 (versions, launcher, GitHub release)

No game code changed. Source and install 0.6.4 (a same-code rebuild). On GitHub: fork main ee2d32e,
release v0.6.4 published (by the user, in Edge). Friend zip CoD3MP-update-0.6.4.zip made. Launcher feed
publishes 0.6.4.

## Version numbers roll over at 999

- The version is the newest `## X.Y.Z:` heading of docs/PATCH-NOTES.md (BUILD-MULTIPLAYER.ps1 reads it).
  Nothing rolled it over, so builds went 0.5.999 -> 0.5.1000 .. 0.5.1004.
- The user's rule: the last number stops at 999, then the middle one goes up (0.5.999 -> 0.6.0, 0.6.999 ->
  0.7.0). Never write 0.x.1000. Written into CLAUDE.md (build loop now says `## 0.6.N:` and
  `^(built|failed): 0\.6\.N`) and into the note at the top of PATCH-NOTES.md.
- Renamed 0.5.1000..0.5.1004 -> 0.6.0..0.6.4 in: PATCH-NOTES.md (headings and text), PICK-ME-UP, NEXT-SESSION,
  CLAUDE-SESSION-2026-11-12, coop_host.cpp comments, version_number.h, build-status.txt,
  build-request.taken.txt, docs/build-logs/build-0.6.0..0.6.4.log (renamed files), tools/coop/coop-out
  t1000a..t1004c logs, the install's VERSION.txt and PATCH-NOTES.txt, a memory file.
  NOT renamed: the game's own run logs (CoD3MP.log and backups: what the game printed), the test run names
  t1000a..t1004c, and the old zip CoD3MP-update-0.5.1004.zip.
- BUILD-MULTIPLAYER.ps1's fallback (+1 on VERSION.txt when the notes have no heading) rolls over too.
- Rebuilt 0.6.4 with no code change (build-status "built: 0.6.4 OK in 4.1 min", 19:57) so the exes say 0.6.4.
- Trap: a wait loop that greps build-status.txt for "built: 0.6.4" matched the OLD line (it already said
  0.6.4 after the rename). Wait for the status file's timestamp line to change instead.

## The launcher (user's own, made 2026-11-08 in another tool)

- Author kit: C:\Users\hovgt\Downloads\CoD3-Launcher-Author-Kit (bin\Publisher.exe, player\CoD3Launcher.exe,
  src\ C# .NET 4.8, hosting\worker.mjs + wrangler.toml, docs\). GAME-FOLDER.txt = the install.
- Feed: https://cod3-updates.hovgtv.workers.dev/stable.json (R2 bucket cod3-port-updates). The payload is
  base64 JSON {schema, version, notes, files[path,size,sha256,chunks]} + a signature.
- Signing key: %LOCALAPPDATA%\HOVGtv\CoD3Publisher\private-key.xml. NEVER copy or upload it.
- Launcher state per install: <install>\.cod3-launcher (expected-release.json, backups, cache, staging).
  Launcher settings: %LOCALAPPDATA%\HOVGtv\CoD3Launcher\settings.json.
- Publish a release (game closed):
  1. `bin\Publisher.exe prepare <kit folder>`: packages + signs the install into release-candidate\ (no upload).
     Check release-candidate\inventory.json (0.6.4: 317 files, 3.49 GB).
  2. ASK the user (it goes to every player), then
     `bin\Publisher.exe publish .\release-candidate .\player\public-key.xml .\hosting\wrangler.toml`.
     Uploads missing objects, then the release snapshot, then stable.json LAST. A dropped connection
     (it happened once at 269/626) is safe: re-run, it skips what is up.
  3. Check: fetch stable.json, decode payload, version must be the new one.
- Version check: System.Version (numeric per part), so 0.6.4 > 0.5.1004 > 0.5.868. A local VERSION.txt newer
  than the feed blocks the update ("not downgraded").
- Last release before today: 0.5.868 (2026-11-08). Now: 0.6.4.
- The launcher does NOT update itself and is not in the launcher feed. The CURRENT launcher is V3 (0.3.3,
  56 MB exe, 2026-11-12 19:55), made in Codex: C:\Users\hovgt\Documents\Codex\2026-11-08\bro-wanna-s
  (outputs\CoD3-Launcher-V3.zip; its source in work\cod3-launcher, with src, assets, tools, docs). Same
  public key as the kit. It shows 0.6.4 as "0.6.04" (display only). Downloads\CoD3-Player-Launcher (1).zip
  is the old v2 (0.2.0).
- Since 2026-11-13 the friend zip carries V3: <install>\CoD3Launcher\ (the 4 player files, extracted
  from the V3 zip) is copied by MAKE-FRIEND-UPDATE.ps1 (after the full/lean block, so both) and
  INSTALL-FRIEND-UPDATE.ps1 copies 'CoD3Launcher' into the friend's game folder. port\ copies updated.
  For a new launcher: replace the files in <install>\CoD3Launcher and remake the zip.
- The fork's launcher/ is the author kit's v1 (0.1.0) source; V3's source (work\cod3-launcher) is NOT on
  GitHub yet.
- 2026-11-13: C: ran full (0 MB). Files over 500 MB in Downloads were moved to D:\Downloads-moved-from-C
  (ISOs, .7z, zips, and a half-finished "Unconfirmed 708256.crdownload").

## GitHub

- Fork clone D:\Call of Duty 3 (USA, Europe)\CoD3Recomp-fork, copied over with the tar command in
  CLAUDE-SESSION-2026-11-07.md (its commit section). Commit ee2d32e "Release 0.6.4 source" (81 files) on top
  of 3b960c3 (the 0.5.851 handoff docs, never pushed until now). The user pushed with GitHub Desktop.
- New in the repo: launcher/ (src, tests, hosting, docs, scripts, player\public-key.xml, README.txt, exe.config).
  Its .gitignore: bin/, player/*.exe, *.exe, *.dll, release-candidate/, private-key.xml, GAME-FOLDER.txt.
  hosting/wrangler.toml carries the Cloudflare account id (user was told; left as is).
- README.md: version 0.6.4, new sections (bots drive tanks, two-player co-op, launcher, version numbers),
  "Where it stands (0.6.4)". docs/RELEASE-0.6.4-NOTES.md = the release text.
- Release v0.6.4 "co-op fixes, game launcher" on main, made by the user in Edge (the built-in browser is not
  signed in to GitHub; no gh CLI on this PC). Checked through the public API.
- These handoff docs (2026-11-13) are NOT in the fork yet.

## Open (unchanged game work)

Co-op: the other player's pose on a mounted gun, a jump move, more vehicles and cutscenes, a two-PC test over
the internet (the user planned one with the friend). Bots: tank pathing rough on some maps.
