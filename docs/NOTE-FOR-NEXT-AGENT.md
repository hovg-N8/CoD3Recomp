# Note for the next agent (2026-11-02, ~12:15)

Hi. Please read `docs/CLAUDE-SESSION-2026-11-02-midday.md` first: it has the details behind everything below.

## State
- Version 0.5.731 is built and installed. Nothing is committed, and the friend zip is on hold.
- `vehicles.txt` holds only the user's two original Merville lines.
- The vehicle pack is switched off: `game\mp\mp_merv\mp_merv_vehicles.cod.off`.

## The two maps
- **Seelow (day)**, `devmap seelow`:
  - Done and tested.
  - Keeps World at War's own sky. Never change that.
  - Rebuild with `bash tools/waw/seelow_install.sh`.
- **Seelow Night**, `devmap seelownight` (pack 6, ID 22):
  - Uses Longyard's night sky (Bravo pack) and moonlit lighting.
  - Rebuild with `NIGHT=1 bash tools/waw/seelow_install.sh`.
  - Night-only changes go behind `NIGHT=1`.

## What the user wants next
1. **Polish Seelow Night.**
   - **The horizon haze is still tan.** The engine reads entity keys `fogcolor` / `fognear` / `fogfar` in spawn function `sub_82564698` (class table entry 31 at `0x825FE894`). Rouen's scene has no such entity, so the defaults are used. Name that class, then give Seelow Night's scene one record with night fog values. `tools/kapf/vehicle_scene.py` shows how to repoint a spare scene record.
   - **The player's arms and gun are lit as in daylight.** The light grid's header colours don't do it, so the per-sample grid data probably does.
2. **Vehicles on Seelow** (5 Panzer IV, 5 Sherman at World at War's tank spots, a Kangaroo, a BMW). The placements are saved in `logs-backup\vehicles.txt.seelow-try`.
   - Only Merville's whole archive works beside a vehicle-less map, and it's too big.
   - The cut pack:
     - crashes without Merville's model register (`.ng.xmbb` / `.xmpbb`);
     - hangs with it (in `sub_82472608`);
     - runs out of memory when the soldiers' textures are kept as well.
   - Next: work out what that register loader reads, or cut the register down to the vehicles.
   - Don't add Seelow or Rouen lines to `vehicles.txt`, and don't turn the pack back on, until this is solved.

## Rules learned the hard way
- **Seelow's cells must be built as one cell** (`C2_ONE_CELL=1`, now the default). Otherwise the map loads invisible.
- **`tools/kapf/repack.py`** frees banks. Its rules:
  - Textures go into blocks before whatever imports them.
  - A zone's index (`.xenon.ib`) comes first.
  - Keep `fogramp` (`--keep-tex`): the engine asks for it by name.
  - The level manifest's per-archive bank number must be lowered to the new block count (`--fli`).
- **Archives loaded beside the weapons don't use the map's 96 banks.** Memory is their limit.

## The user's standing rules
- Tell them before every game launch, and back up the logs first (`run_test.py` does this).
- When they say "check the log", run `python -I tools/mplog/mplog.py` before anything else.
- No `.bat` files, never upload game data, don't sync the fork.
- Carentan keeps Rouen's sky.
