COD3 PC Launcher — first build

Run CoD3Launcher.exe and select your existing extracted COD3 PC port folder.
Keep CoD3Launcher.exe.config and the publisher's public-key.xml next to it.
Your own installed base game is required.

The launcher checks for content updates when it opens and provides Campaign,
Multiplayer, and Verify / Repair. Names, controls, graphics, peers and saves
are preserved. The multiplayer directory is cod3-master.hovgtv.workers.dev.

AUTHOR: BUILD-RELEASE.cmd creates public-key.xml in this folder. Include it
when distributing the player folder. Never distribute the private signing key.

Until the update service and a signed release are published, the launcher
can only launch an existing installation; it cannot fetch unpublished content.
