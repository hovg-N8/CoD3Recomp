CO-OP CAMPAIGN EXPERIMENT (from Team Justin, Oct 18 2026) - UNTESTED BETWEEN TWO PCs
=====================================================================================
Source and scripts only (no exe, no game data), per the shared log's rules.

What it is
- Two players in the campaign's Forest mission, over Tailscale. The host plays the
  campaign as normal; the joining player's copy follows the host's world.
- Tested only as two copies on one PC (Justin's), many 7-minute runs. Never yet
  between two PCs.

How it works (short)
- The campaign program's own server/client link carries almost nothing (its
  client reads the world straight from memory), so coop.cpp sends the world itself:
  the host sends every entity's place, angles and health 20 times a second (UDP
  port 28962); the joining copy pairs them with its own (by classname, nearest
  first, kept), walks its soldiers to the host's places (the scripts' setgoalpos),
  and kills its copy of any soldier the host's has lost.
- Each copy borrows one of the mission's American riflemen as a stand-in for the
  other player. The joining player sends his place on UDP port 28963.
- The joining player's hits on Germans are dealt on the host; hits on his stand-in
  on the host are taken from his health. Killed, he comes back beside the host after
  5 seconds (the mission does not fail).
- Everything is off unless COD3_COOP is set (host or join). It only touches the
  campaign program (it is empty in the multiplayer build).

To build (the campaign program, CoD3.exe)
1. Copy CoD3Host\coop.cpp into your CoD3Host folder.
2. In CoD3Host\CMakeLists.txt add "coop.cpp" to the host's source list
   (COD3_HOST_SOURCES, e.g. after "weapon_dump.cpp" or the last entry).
3. Build the CoD3 target (the campaign program) with the Forest mission's code
   recompiled (scripts\recompile_level.ps1 forest), as you build the campaign.

To play
- Both: Tailscale on; allow CoD3.exe through the Windows firewall (UDP 28962-28963).
- The host: PLAY-COOP-HOST.bat beside CoD3.exe. It asks for the joining PC's Tailscale
  address (Enter uses the System Link peer from the F11 settings).
- The joining player: PLAY-COOP-JOIN.bat, with the host's Tailscale address.
- A lobby: both start the game and go to the main menu. The joining copy finds
  the host (a note on screen says when you are connected). The host picks the
  mission from the menu (chapter select; Forest only for now) and the joining
  copy starts the same mission by itself.
- Logs: coop-host-play.log / coop-join-play.log beside the exe ("coop:" lines).

Known rough edges
- The stand-ins are ordinary riflemen with their own brains: they follow the
  players but may face away or shoot by themselves.
- The mission's story messages and objectives run on both copies separately.
- The joining player's hits are matched to where he aims (within 6 degrees).
- Forest only so far.
