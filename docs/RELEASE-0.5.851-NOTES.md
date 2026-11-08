# CoD3 Recomp 0.5.851

Source release. Everything since 0.5.424 (the last update on this fork). Full per-build details are in
`docs/PATCH-NOTES.md`. No game files are included: you need your own copy of Call of Duty 3.

---

## New maps

**Seelow (from World at War)**
- World at War's Seelow as a playable CoD3 map pack (`devmap seelow`): its own geometry, textures, props,
  collision, ladders and spawns, built with the tools in `tools/waw`.
- Mantling: climb over World at War's walls, trench lips and windows like in WaW (a "Mantle assist"
  setting, off for players, bots keep it). Steps up to 28 units on the converted ground.
- Tanks and vehicles on Seelow: Panzer IVs, Shermans, a Kangaroo and a BMW bike, placed from the map's own
  scene.

**Seelow Night**
- A separate night version (`devmap seelownight`) with Longyard's night sky, moon, fog and moonlight.
- Your arms and gun are lit by moonlight instead of daylight.

**Carentan (from Call of Duty 2)**
- CoD2's Carentan ported in as its own map (`tools/cod2import`, `tools/carentan`): its geometry, textures,
  collision, spawns and flags.
- Bots learn to use its stairs and cellars and leave dead-end rooms.

## Tanks

- Drive and gun tanks on maps that never had them (Seelow first), with Merville's own tank data.
- Tank cannon: Merville's real shot sounds, muzzle blast, reload sound and shell. Shell hits explode,
  using the map's artillery explosion.
- Tank machine gun: its own fire sound, muzzle flash and ring-off, a round every 110 ms.
- Tank shells do real damage, and the kill is credited to the driver (it used to go to the tank itself).
- Engine, tread and suspension sounds play on map packs.
- Fixed crashes and freezes when changing maps between Seelow and Merville (vehicle files read once a
  load, kept archives, a DemonWare session guard).
- A fix for the Kangaroo's missing treads on Merville (not yet checked in play).

## Bots and vehicles

- Bots get into tanks on their own, Shermans and Panzers: their steering no longer undoes the turn to the
  tank, they get in from the tank's corners, take the learnt map's route to it and walk around it to a
  free corner.
- Bots can fire a tank's cannon (and a second bot mans the machine gun).

## Weapons

**COLT .45 DUAL (new weapon)**
- Two Colts: a second, mirrored left arm and Colt.
- Reloads one pistol at a time, hip fire only (no aiming down sights).
- Muzzle flash on both barrels, lined up with the spread-apart guns.
- Others see two Colts in your hands.
- Pick it in the class editor as a primary or a sidearm.

**World at War's M2 flamethrower**
- Shoots fire, not bullets: WaW's flame stream from the nozzle, fire left where it lands, and it burns.
- WaW's own pose, grip and animations on the arms; its world model in others' hands.
- Overheats like in World at War (a heat bar instead of an ammo count and reload).
- Others' flames are seen and heard only close by, never through walls.
- Its own kill-feed icon. Bots can carry it.

**World at War's signal flare**
- A special grenade: thrown, it lands, lights up, burns anyone standing on it and blinds.
- WaW's flare stick model and its own sounds. Bots carry and throw it.

**CoD2's Webley revolver**
- Its own weapon with CoD2's model, numbers and reload; the cylinder shows the rounds left.
- Others see a Webley, with its own kill-feed icon. Bots can carry it.

**CoD2's scoped Mosin-Nagant**
- Its own model, scope, sound, world model and kill icon.

**Other weapon fixes**
- MG42 / FG42 full clips in a rifleman-based custom class.
- Handheld MG42 no longer disappears on Isle.

## Killstreaks

- Mortar: its real loading sound, firing sound and explosions, with real hits and damage.
- Hellfire Barrage: the game's own artillery shells over the whole map with bombers, kills credited to
  the caller and shown in the kill feed; killstreak kills don't earn more streaks.
- Killstreaks at 2, 4, 6 and 8 kills.
- UAV plane: stays under the sky and inside the map's draw distance; a fitted circle on Rouen, Carentan
  and Isle; the German UAV is a Stuka.

## Bots

**Capture the Flag (new)**
- Bots play Capture the Flag, Single Capture the Flag and Headquarters.
- They take, carry, capture and return flags; carriers hold the flag and go home the way they came.
- Attackers and defenders, squads, escorts, left/middle/right lanes; routes and base exits learnt from play
  and kept between games.

**Getting around**
- Bots route to objectives over the map they have learnt (A*), take stairs and doorways, look around for
  ways out and stop getting stuck going back and forth.
- They no longer spawn into the enemy's sights; spawns follow your own flag in CTF.

**Fighting**
- They aim at the body, settle their aim, use their sights, crouch and go prone more.
- Bazooka bots draw their pistol up close. Bots throw one grenade at a time and use smoke properly.
- Bots use ammo and health packs more.

## Online and lobby

- Public servers: host a game on a public list and join others' public games over the internet.
- Max players goes to 32 in the lobby.
- The DLC maps' loading pictures are back.
- A pre-match countdown, with the sides balanced before the start.

## Spectator

- A SPECTATOR choice on the team screen (and F6 anywhere): no HUD, just a free camera or following players.
- Leave the seat to join the match again, switch sides from the pause menu.

## Menus and controls

- Mouse in the multiplayer and campaign menus: lists, arrows, the quit question, the lobby; right click goes
  back.
- Button prompts shown as keyboard keys and switching with what you're using.
- Noclip by a double tap (a cheat), with a return point.

## Killcam

- Explosions replay in the killcam, with the killer's HUD and a steady view.
- Fixed killcam freezes, including after vehicle deaths and with 30 bots.

## Co-op (from the co-op team)

- Co-op v38, v40 (World at War style co-op campaign) and v46 merged.

## Graphics and look

- The look updates ("look v1" and film player update 2) merged.
- Lamps' glow no longer shows through walls.

## Stability and performance

- The campaign no longer freezes at the legal screen.
- Fixed a freeze loading CTF on Rouen, a long-game freeze (a stray write from a bot's death), a sound
  flood that dropped Rouen to 8 fps, and a render list crash.
- The log no longer floods in games with bots.

## Tools (for map and weapon makers)

- `tools/waw`: World at War map and weapon import (Seelow, the flamethrower, the flare).
- `tools/cod2import`, `tools/carentan`: Call of Duty 2 map and weapon import.
- `tools/kapf`: CoD3 archive tools (repack, effects packs, `kapf_lists.py`).
- `tools/mplog`: a summary of the multiplayer log.
- `tools/master-server`: the public server list.
