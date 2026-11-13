# Launcher v2 - 0.2.0

The player window now places Campaign, Multiplayer, Verify / Repair, folder
selection, saved audio toggle, update status and progress in a left sidebar.
Release notes remain in a compact panel over the lower-right battlefield.
JARC and Treyarch credits are deliberately small along the bottom.

Use the new player ZIP to replace the previous launcher files (keep all four
files together). Existing saved folder preferences are retained. This is a
launcher replacement; game releases currently do not update the launcher
executable itself. Share the new player ZIP with existing players once.
It still uses the current public signing key and Cloudflare update endpoint.
No new game release or Worker deployment is required for this visual update.

To update the author tools, close BUILD/PUBLISH first, then extract the whole
author kit over the existing author folder. Keep release-candidate, and keep
the existing private signing identity in AppData. The kit includes only the
public key. Run the usual BUILD-RELEASE and PUBLISH-RELEASE commands for later
game updates. BUILD.ps1 recompiles the player with its embedded media.

Battle ambience is a quiet original procedural wind/distant-battle loop, not
extracted game music. It can be muted, remembers the choice, pauses before
game launch, resumes on process exit, and stops when the launcher closes.
Repeated launches are blocked while a game started by this launcher runs.
If audio playback is unavailable, the launcher remains usable.

Artwork: original generated WWII battlefield backdrop and JARC twin-J arch
emblem. The emblem is independent branding for the community project.
Treyarch's unchanged logo was retrieved from the official studio website:
https://www.treyarch.com/content/dam/atvi/treyarch/treyarch-2023/common/header-logo.png
Original game credit is separate from JARC's community port/enhancement credit;
the launcher identifies itself as an independent community project.
Existing upstream port credits in the project remain applicable.

Validation: 36 updater/publisher checks, compiled UI/resource smoke check,
audio lifecycle and failed/repeated-launch checks, and rendered window review.
The real COD3 game and a live content deployment were not run for this UI change.
