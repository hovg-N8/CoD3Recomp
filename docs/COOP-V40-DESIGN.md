# Co-op v40: the campaign played the World at War way

Team Friend (Claude session), started 2026-10-29. Replaces the mirrored
co-op (v35..v39, two campaigns kept alike) with the design Call of Duty:
World at War's co-op campaign uses. Two players (the host and one friend).

## The rule everything follows

**One world.** Only the host's game runs the mission: its scripts, its
soldiers, its spawners, its damage, its checkpoints. The joining player's
game loads the same level, then stops running it (its script threads and
its soldiers are gone) and becomes a window onto the host's world: every
soldier, drone, vehicle, door and grenade it shows is a drawn-only copy
("puppet") that the host's snapshots move and animate. Nothing can drift
apart, because there is nothing on the joining side to drift.

The joining player himself is a real player of the host's world: the host's
server has a second client (client 1, the work of COOP-SECOND-SLOT.md), with
his own entity, record and sentient, so the enemy AI sees him, aims at him
and hurts him by the game's own rules; his shots and grenades are made by
the host's game from his own commands.

| | host | joining copy |
|---|---|---|
| level scripts, spawners, AI | run | stopped once the level is in (its own soldiers and drones deleted) |
| soldiers, drones, vehicles, movers | real | puppets: script_models given the host's models and animation tree, moved and animated by snapshots |
| the joining player | client 1: his commands in, his place from his copy | his own player, moved by his own game (no input lag) |
| damage | all of it, the joining player's shots too | none (G_Damage does nothing there) |
| health, weapons, ammo of the joining player | the host's client 1 | copied from the host |
| objectives, HUD, messages | made by the host's scripts | the host's config strings and server commands, given to its own server |
| sounds, effects, music | the host's | the scripts' sound/effect calls replayed (later milestone) |
| last stand, revive, mission failed | decided here | shown |

## Why not the old way

The mirrored co-op ran two whole campaigns and pulled the joining copy's
soldiers toward the host's. Every test found a new way for the two to
disagree (soldiers that could not walk where the host's were, grenades off
by bodies, soldiers that fired on their own, a host body stuck 470 units
off, missions restarting at different points). It can be made better, not
right: two simulations of the same battle never stay the same. World at War
never tried: its clients only draw the server's world.

## What the campaign gives us (found in default.xex)

- **The server and game** (COOP-SECOND-SLOT.md): Quake 3 / CoD, for one
  client. svs 0x829C39E0; level 0x82A4E790 (+148 max clients, +8 the 33
  sentients, 336 bytes each, +20..+144 the 32 actors); the client table
  `*0x82A2AB40` (entry client + 1).
- **The frame**: SV_Frame `sub_82518E40` runs the script VM
  (`sub_824BB6F8`, 0x82A90128) first, then G_RunFrame `sub_8256D3E0`
  (via `sub_825173F8`), then SV_SendClientMessages `sub_825161D0`.
  G_RunFrame runs every entity through `sub_8256C010` (by type: 3 missiles
  `sub_8256B7E8`, 2 items, movers `sub_82559F10`, 4/7/11 `sub_8256A460`)
  and Treyarch's component managers by tag (XANM, ANIM, DETH, DRON, PATH,
  HRGN, ANFY) through `sub_8251DF90`.
- **Entities**: 1040-byte blocks from G_Spawn `sub_82543568`, kept in the
  table at 0x82A2DCD0 (4096 slots: the entity and a generation; a script
  reference is index | generation << 14). Type byte at +0 (1 player, 2 item,
  3 missile, 7 drone, 10 vehicle, 11 actor, 12 spawner), +336 origin, +352
  angles, +500 number, +528 player record, +532 actor, +536 sentient (+4
  its team), +540 vehicle, +552 animation tree, +556 model (slot 0),
  +564 classname, +572 targetname, +624 takedamage, +632 flags (0x400
  hidden), +776 health, +780 max health, +984 models 1..3.
- **Models**: G_SetModel `sub_8254EE80` (entity, name, bank, slot 0..3);
  a character (setmodel of a "cdChar") picks its body, head and gear at
  random (`sub_823D9610`), so a puppet takes the host's four models by
  name, not the character.
- **Animation trees** (animation.md): tree at +552, a 16-bit slot per
  animation of its list, records of 52 bytes in the pool at 0x82A59DB0
  (+16 time, +20 old time, +28 blend speed, +32 goal weight, +36 weight,
  +40 rate). "Set anim, parents alone" `sub_82500B80` takes a record;
  useanimtree's step `sub_82555B98` (entity, list) makes a tree.
- **Damage**: G_Damage `sub_82564888` (target, inflictor, attacker, dir,
  point, damage, flags, means, ...). A player's die `sub_8255F320`, his
  death `sub_8255EF30` ("INGAME_PLAYER_DIED", the mission failed).
- **ClientBegin** `sub_8254A060` gives a player a sentient (team 2) from the
  33 (`sub_823C39D8`), his health from g_player_maxhealth.
- **Config strings**: SV_SetConfigstring `sub_82513C98` (index < 1024,
  text) keeps them in sv (0x82A568E0 +16) and sends "cs N text" through
  SV_SendServerCommand `sub_82513628` (client, format, ...): objectives
  (config string 16 + n) and HUD texts go this way.
- **HUD elements**: 16 of 124 bytes at 0x829B01B8 (cleared by
  `sub_82559E38`).
- **Spawning by script**: spawn `sub_824B4510` (classname Broc::string,
  origin, ...); a Broc::string is a pointer to a block whose +8 is a 16-bit
  count and +12 the text.

## The network

One UDP port each way (COD3_COOP_PORT, default 28970: the host listens on
it, the joining copy on +1, so two copies run on one PC). Every packet
carries the sender's clock. Two kinds of message:

- **unreliable**, newest wins: the host's snapshots (20 a second, in
  pieces of at most 1200 bytes, for Tailscale's 1280), the joining player's
  commands and place (each frame), his player state from the host;
- **reliable, in order**: config strings, server commands, model names,
  puppets made and gone, last stand and revive, mission start, restart and
  end. Sent until acknowledged; acknowledgements ride on every packet.

The joining copy shows the host's world 100 ms in the past plus the jitter
it measures, between two real snapshots, as WaW's clients do.

## Milestones

1. Host: client 1 as a full second player (his own commands and view, his
   place from his copy, the AI's target, his damage) - plus his body.
2. Joining copy: frozen once the level is in; puppets for every soldier,
   drone, vehicle, mover, item and grenade of the host's; animation trees
   replicated; interpolation.
3. The joining player's health, weapons and ammo from the host; the host's
   view effects for his hits.
4. Config strings, server commands, HUD elements; then sounds, effects and
   music.
5. World at War's rules (below).
6. Lobby, mission flow, restarts, the relaunch between missions.

## World at War's co-op rules, as kept here

From WaW's `maps\_laststand.gsc` (as its successors carry it) and its co-op:

- **Last stand**: a player brought to 0 by bullets, melee, grenades,
  explosives or fire does not die: he goes down (health 1, prone), keeps
  only his pistol, the AI stops aiming at him (ignoreme), and he bleeds out
  over 30 s (the screen greying in the second half).
- **Revive**: a teammate within 40 units holding Use for 3 s brings him
  back ("Press and hold [Use] to revive <name>", a progress bar, "<name> is
  reviving you"); the downed one gets his weapons back and full health.
- **Mission failed** (both back at the last checkpoint): when the other
  player is down too (everyone in last stand), when a downed player bleeds
  out, or when a player is killed by what last stand does not allow
  (falling, crushing, a trigger, suicide).
- **HUD**: "<name> needs to be revived!" to the other player, a revive
  icon over the downed player, the two players' names in their colours.
- **Scoring** (competitive co-op): points for kills, headshots and
  revives, lost for going down; shown at the mission's end.
- **Difficulty for two**: the enemy's accuracy and the damage taken
  scaled for two players (values ours: WaW's own table is not public).

## As built (2026-10-29, Claude session)

Files (campaign program only; empty in multiplayer): `coop.h`, `coop.cpp`
(network, hooks, the mode), `coop_lobby.cpp` (F2 lobby), `coop_host.cpp`
(second player, snapshots, config strings, script dvars), `coop_join.cpp`
(frozen world, puppets, ride-along), `coop_laststand.cpp` (last stand,
revive, mission failed, scores), `coop_hud.cpp` (ImGui co-op HUD),
`coop_events.cpp` (sounds, speech, music, earthquakes), `coop_probe.cpp`
(diagnostics, COD3_PROBE40 only). `overlay.cpp/.h`: the lobby's F2 window,
`MainMenuShown()`, `COD3_SHOT_OVERLAY` screenshots. `movement_fixes.cpp`:
`COD3_NOBOUNCE` (tests).

### The lobby (World at War's)
- F2 anywhere outside a mission. **Host a game**, or **Join** a game found on
  the network (each network's broadcast and the System Link peers of F11, UDP
  28964 side channel, plus UDP 28972 on this PC), or join by address
  (Tailscale). The host can **Invite** an address.
- Both players listed with READY marks and pings; the host picks the mission
  (the 14 chapters), the difficulty (Easy / Normal / Hard: the value SV_SpawnServer
  reads at 0x82A8E610 +32) and competitive scoring; the joining player marks
  himself ready; **Start** counts 5 s down on both screens, then both load the
  mission. Leaving says goodbye; the host leaving closes the lobby for both;
  the host back at the main menu after a mission brings the joining copy back too.
- The choice is kept in the environment (COD3_COOP, COD3_COOP_HOSTIP), so the
  process the title starts for its next mission stays in co-op.

### Engine facts found here
- **The "client table" at *(0x82A2AB40) is [1] the player, [2] the world's
  entity** (0x82A2AB40 + 4 * (client + 1)). The second player's entry is
  written only while he connects (SV_DirectConnect, SV_ClientEnterWorld) and
  the world put back. Left there (as v39/v40 did), every use of "the world"
  found him: the host's player lost the ground and slid about with no input,
  and the scripts' notifies to the level (the intro camera's "anim done")
  went astray, so the Forest intro never ended (letterbox, no gun, no HUD).
- Level scripts set the letterbox, the gun, the HUD and more by
  `setdvar` (sub_824AB260/300/398 -> sub_8252B850). The host sends each to the
  joining copy, and all of them again when it freezes.
- An entity's link (linkto) is +916 (a 112-byte record, +0 the parent);
  G_EntUnlink is sub_82540D78. The Forest intro does not link the player: a
  camera animation carries him.
- Sound commands: sub_82534ED0 (an alias or weapon name on an entity: actors'
  shots, grenades), sub_82535568/82535460 (playsound), sub_82535788
  (playsoundatposition), sub_82535668 (loops); speech sub_824B3FF0 (the voice
  system sub_82485240, its notify at the voice object +44); music
  sub_824897A0/824895C0 (player *(0x82A2A2CC)), stop sub_8247D7A8.
- The engine's notify is sub_824927C0 (entity, name hash); the scripts' is
  sub_824A5268.
- Guest::AllocatePhysical hands out 64 KB at least: small co-op things come
  from 1 MB blocks (GuestAlloc), or the joining copy runs out of memory.
- Player +44 flags 0x100000 / 0x200000 (allowstand / allowcrouch false):
  a downed player lies down.

### Not done yet
- Muzzle flashes, tracers, impacts and other effects of the host's soldiers
  on the joining copy (FX_PlayFX sub_82423F08: r3 an id, r4 the effect).
- A downed player keeping only his pistol; difficulty scaled for two.
- Lag compensation of the joining player's shots.
- The next mission after a mission's end (the title relaunching), between
  two PCs.

## v43..v45 (Claude session, 2026-10-29/30 night): the joining copy made stable

What froze or crashed the joining copy, what each was, and what is done now. Every item was found
in the two-copies-on-one-PC tests (work/claude-coop2: runner.py, coop2_test.py, summarize.py,
make_level_jobs.py; logs in outputs/coop2-test/{host,join}/TAG.log and TAG-reload.log).

- **The script VM** (sub_824BB6F8) spun for ever on threads we had marked ended (0x4C) every frame
  (Island). Now marked once at the freeze and the VM's thread loop is not run on the frozen copy;
  only its housekeeping (sub_824A5EE8) runs, so freed objects are released.
- **Our own drone spawner** (sub_82521C70: max_drones, drone_animtree) went on making background
  soldiers under the frozen world: stopped once frozen. **Our physics world** (sub_824139B8 and the
  relink of what it moved, sub_82550368): stopped once frozen.
- **The client's reliable command text pool** overflowed (DbgBreak "Reliable command buffer
  overflow", sub_82512970): an objective marker that follows a moving thing (Island's tank) changed
  config string 32 every frame. Now an unchanged config string is not given to our server, and one
  index at most twice a second (the latest kept).
- **Puppets are never freed while frozen**: kept hidden in a pool (64) and reused for a host thing
  wearing the same models, attachments and animation list. Once a thing animates, its models and
  attachments are never changed (the skeleton was sized for the first ones: poses written past it,
  over the next entity: the crash in sub_8216E680 and the endless link walk). A changed host thing
  gets a fresh puppet. Our own hidden things are never shown again (the renderer crashed on them).
- **Set anim** calls of ours wait their turn (at most 3 pose jobs of ours a frame, AnimJobRoom), and
  the frame's pose jobs (sub_824F38E8) are checked: one naming a model part let go is dropped.
- **The world's sector tree** walk (sub_82476250) is written out with every node remembered: a loop
  is cut. (None was seen after the fixes above, but it ended two freezes before them.)
- **Deaths and checkpoints**: the joining copy never kills its own player (only the host decides);
  a crush by a scripted vehicle (Island's landing craft) does not kill a player when two play. When
  the host goes back to a checkpoint, the joining copy starts itself again as a new process (the
  stopped world's script threads cannot be reloaded in place: sub_824BB6F8 spun) and follows the host
  into his mission, as World at War's clients reload with the server; the host holds client 1 beside
  himself until that copy is frozen again (90 s at most), then lets it go.
- **The heap guard**: on the joining copy every block of the title's heap is followed, and a free of
  one it does not hold (a second free) is not done (heap_trace.cpp, HeapTrace::SetGuard).
- **Vehicles of ours** are deleted at the freeze (their riders with them), and the copy does not freeze
  while the host's player is carried by the scripts (Falaise's jeep): our own ride plays out first.

### v45t..v45u: the T-posed men on the joining copy

- **The pose job queue** (sub_824F39A0, 160 entries of 820 bytes at 0x829C91F8, one per animated
  model a frame, keyed by its DObj in **r3**): our cap compared the key against r7 (the register the
  function moves r3 into, not yet set when our hook runs), so every further pose wanted of a model
  already queued was refused once 40 were queued, and that man stood in his bind pose. Now matched on
  r3 and the cap is 150 of the 160 (the joining copy poses 100..150 models a frame in a fight; the
  host about 40). Seen in the screenshots: Island's bridge, no T-posed soldier left.
- **Weights**: the frozen world moves no animation weight on to its goal, so the host's weight is
  written every snapshot (it was only on a new list), and the host's body on the joining copy blends
  its moves itself (a fifth of a second).
- **Nothing playing on the host's** (a soldier who has just fallen): what he played last is kept at
  its end and stopped, not zeroed (zeroed, he stood in his bind pose).
- The host now sends up to 100 animations of a tree (48 before; the most seen is 18).
- **The lobby**: a mission asked for and not begun (the menu let one `spmap` go by, Island L20) is
  asked again every 15 s, three times.
- **After a checkpoint** (v45u): the host's last stand state is reset whenever the second player is made
  again (St. Bert's checkpoint kept the host's entity, so the joining player's bleed-out went on into the
  restarted mission). Room beside the host is never taken from another floor of his trail, is made 24
  above his feet, and on the joining copy never below the ground it last stood on (Mayenne's start: the
  host's floor is 40 below the joining copy's there; put at his height it fell out of the world). The
  joining copy puts itself back where it last stood if it falls 300 below it; the host also makes room
  again if the joining player is stuck far below him. Our hidden things are made not solid (contents 0),
  and a host brush model's puppet is solid or not as the host's is.

### v46: the seamless checkpoint reload, the lobby screen, the pause

- **Checkpoint reload behind the window** (coop_swap.cpp): the old joining copy goes quiet on the network
  (its socket let go, NetSilence), shows World at War's loading screen, and starts a new copy hidden
  (COD3_COOP_HIDDEN, at the old window's place: COD3_WINDOW_RECT). The new copy joins and freezes as at
  a first join; a second after its freeze it shows its window and sets the old one's named event
  (COD3_COOP_SWAP), and the old one ends. The old one also ends if the new one never says (150 s), and
  restarts the old way if the new one dies. Reloading in the same process (COD3_COOP_RELOAD_INGAME=1)
  stays off: Mayenne's new level crashed on the stopped world's leftovers.
- **The lobby screen** (coop_lobby.cpp): a CO-OP CAMPAIGN panel on the title screen and main menu (Y, C,
  a click, or F2) opens a full-screen lobby: host, games on the network, join by address; then the
  missions, difficulty, scoring, two player cards, start / ready / leave. The real controller drives it
  (Input::RealPad fed to ImGui's gamepad navigation; the game's input is held while it is open).
- **The pause menu** (coop.cpp, SV_Frame sub_82518E40): the frame is skipped while the local client's
  byte at 0x82A90A6A is set (the menus set it); with two players in a mission (after its first 10 s) it
  is let go for the server's frame and put back after, so the mission runs on behind the menu.
  COD3_COOP_PAUSE=1 lets it pause.
- **Poses**: the pose queue (160 entries) is posed early and begun again when it reaches 150, instead of
  refusing what comes after (soldiers past 150 stood in their bind pose); the key is r3, not r7.
