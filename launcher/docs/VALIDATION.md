# First-build validation — November 8, 2026

- C# x64 compilation succeeded using Windows' .NET Framework compiler.
- 29 real-filesystem updater/publisher tests passed, zero failed.
- WinForms form construction/control check passed (Campaign, Multiplayer,
  Verify/Repair, and disabled preview launching).
- 7 Worker routing/response tests passed, zero failed.
- First full install inventory: 317 selected files, 3,486,474,421 bytes
  (3.25 GiB).
- All six packs selected; all 109 finished texture replacements selected.
- Multi-chunk publisher-to-installer roundtrip passed on synthetic content.
- Independent review's six material findings have regression coverage and fixes.

Limits: no full 3.25 GiB candidate was built or uploaded, no live update service
is deployed, no two-PC gameplay test occurred, and no screenshot-based visual
acceptance is claimed. A Windows link test uses an existing OS link; creating a
new junction is denied in the restricted process. No original game/source files
were modified. The launcher needs a published signed release before online
updates can be tested end to end.
