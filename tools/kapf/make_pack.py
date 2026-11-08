"""Makes a map pack the way the game's downloadable ones are laid out
(saves\\content\\00000002\\<pack>\\, displayname.txt and mp\\dlmaps.txt), so the
game lists the map with the others (as Marseilles, ordnance, from the Bravo
pack). The map's files are hard links to a source map's, the zones and the
rest under their own names; the level's archive, <source>.cod, is a copy
named <folder>.cod (the title opens dlcN:\\mp\\<folder>\\<folder>.cod) with
its FLI's paths moved to the folder. The sound bank keeps its name: the
title asks for the one the archive's AUDIO section names (rouen.wbk), in
the map's folder (0.5.668 test: mp\\caren\\rouen.wbk).

dlmaps.txt's columns (the packs' own header): MapPack, ID, FrontEnd
DisplayName, PakName, LoadingScreen Title, LoadingScreen Location, Language.
Underscores stand for spaces. Packs in the game: 1 Champs (ID 9), 2 Valor
(10-14), 3 Bravo (15-19); base maps are 0-8.

usage:
  python -I make_pack.py <source map folder> <content folder> <pack name>
         <folder> <pack number> <id> <menu name> <title> <location>
e.g. make_pack.py ...\\game\\mp\\rouen ...\\saves\\content\\00000002 Carentan
     carentan 4 20 Carentan CARENTAN CARENTAN,_FRANCE
"""
import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

LANGUAGES = ("US", "DE", "FR", "ES", "IT")


def main():
    if len(sys.argv) != 10:
        sys.exit(__doc__)
    source, content, pack_name, folder, pack, ident, menu, title, location = sys.argv[1:]
    source, content = Path(source), Path(content)
    pack_dir = content / pack_name
    map_dir = pack_dir / "mp" / folder
    if pack_dir.exists():
        sys.exit(f"{pack_dir} is there already: not touching it")
    map_dir.mkdir(parents=True)
    (pack_dir / "displayname.txt").write_bytes(f"Call of Duty 3 Map - \"{menu.replace('_', ' ')}\"".encode("utf-16-le"))
    lines = ["//  Made by tools/kapf/make_pack.py (CoD3 Recomp); laid out as the game's map packs.",
             "",
             "//\tMapPack\t\tID\tFrontEnd DisplayName\tPakName\t\tLoadingScreen Title\tLoadingScreen Location\t\tLanguage",
             ""]
    for language in LANGUAGES:
        lines.append(f"\t{pack}\t\t{ident}\t{menu}\t\t{folder}\t\t{title}\t\t{location}\t\t{language}")
        lines.append("")
    (pack_dir / "mp" / "dlmaps.txt").write_bytes("\r\n".join(lines).encode("latin-1"))
    for f in sorted(source.iterdir()):
        if not f.is_file():
            continue
        name = f.name
        if f.suffix.lower() == ".cod" and f.stem.lower() == source.name.lower():
            name = folder + f.suffix
            # The level's own archive: a copy (a link would change the
            # source's), its FLI's paths moved to the new folder.
            data = bytearray(f.read_bytes())
            relocate_fli(f, data, source.name, folder)
            (map_dir / name).write_bytes(data)
            print(f"  {map_dir / name} <- {f.name} (copy, FLI paths moved)")
            continue
        os.link(f, map_dir / name)
        print(f"  {map_dir / name} <- {f.name}")
    print(f"pack {pack} '{pack_name}': map {ident} {menu} in {map_dir}")


def relocate_fli(path, data, old, new):
    """The FLI (the level's manifest: its archive, zones and the weapons'
    by path, "mp\\rouen\\zn01.cod") names its folder; the title will not
    load a level whose manifest names another (sub_8247D918 returns 0 and
    the loader sends "reboot"). fli.move puts the paths in the new folder
    (appending the ones that outgrow their room), fli.replace_block writes
    the block back with the archive's sizes to match."""
    import fli
    import kapf
    archive = kapf.Archive(path)
    block = archive.block(0, [s[0] for s in archive.sections].index("FLI"))
    try:
        moved, changed = fli.move(block, old, new)
        fli.replace_block(archive, data, moved)
    except ValueError as e:
        sys.exit(f"{path}: {e}")
    for text, now in changed:
        print(f"    FLI: {text} -> {now}")


if __name__ == "__main__":
    main()
