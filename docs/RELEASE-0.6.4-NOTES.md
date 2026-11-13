# CoD3 Recomp 0.6.4

Source release. Everything since 0.5.851 (the last update on this fork). Full per-build details are in
`docs/PATCH-NOTES.md`. No game files are included: you need your own copy of Call of Duty 3.

About the number: the last part of the version stops at 999, so the build after 0.5.999 is 0.6.0.
The five builds first numbered 0.5.1000 to 0.5.1004 are 0.6.0 to 0.6.4 in the notes. This is the
first release where the game itself shows a 0.6 number.

---

## Co-op campaign (two players, experimental)

Most of this release. Two players play the campaign together (`COD3_COOP=host|join`).

**Going down and coming back**
- A downed player has 10 seconds and can be revived by holding Use; after that he comes back in.
- If both players are down, both go back to the last checkpoint.

**Seeing each other**
- The other player is seen moving, aiming up and down, firing, reloading, throwing, meleeing,
  crouching and switching guns on his own body, with the gun he really carries.
- No more T-pose or bind pose when idle or at a change of gun.
- Effects, smoke and grenades show on both screens; each player sees his own grenade once.

**The joining player counts in the host's world**
- His shots and throws are fired in the host's world, and his hits on the level's things count for
  the mission.
- His Use, hints and prompts are the game's own: picking up and swapping guns off the ground (the gun
  comes up in his hands, with its name in the prompt), triggers, and the host's soldiers drawn for him.
- No friendly fire between the two players.
- Mounted guns: he mounts, aims and fires stationary MGs, and the other player sees it.
- Rides: a seat beside the host when the host is driven (Falaise's jeep gun), and off the ride where
  the host stands.
- After a scripted scene (Saint-Lo's wall) he is brought along to the host's side; a sprinting host no
  longer pulls him along.
- An action he starts (a live grenade's pick-up) no longer plays on the host's player.

**Still open:** the other player's pose on a mounted gun, a jump move, more vehicles and cutscenes,
and a full test between two PCs over the internet.

**Testing:** hands-off test tools for two copies on one PC in `tools/coop/` (`coop_test.py`,
`guarded_test.py`, Saint-Lo's scripted route `stlo_run.sh`).

## Multiplayer: bots and tanks

- Bots drive tanks at full speed along known routes, and fire the cannon and machine gun of the tank
  they drive.
- Bots carry a random extra gun (the added guns too) that stays in hand.
- Bots hold fire a little wider when a teammate is in the line.
- A joining player sees bots die (kill feed and death) and no longer sees them cartwheel.
- The freeze in flare matches fixed; the Mosin's scope fixed.

## Launcher (new)

A Windows launcher for players, in `launcher/`:
- Campaign, Multiplayer and Verify / Repair, on your existing install of the PC port.
- Checks a signed release each time it opens and downloads only the files that changed: the game, all
  six map packs, the texture replacements and the custom guns. Backs up what it replaces, resumes
  broken downloads, repairs damaged files, and never downgrades a newer local build.
- Your names, controls, graphics, peers and saves are kept.
- Included here: the launcher's source and tests, the Cloudflare update service, and the scripts that
  package and publish a release. Not included: the signing key, packaged game files, and built exes.

## Tools

- `tools/wawaudit/`: a World at War audit (levels, sounds, cut content); results in `docs/WAW-AUDIT.md`.
- `tools/carentan/triggers.py`, and updates to `tools/mplog`, `tools/waw` and the build script
  (version roll-over).
