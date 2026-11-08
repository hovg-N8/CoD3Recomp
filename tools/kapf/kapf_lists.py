"""Empty file lists of a KAPF archive's sections (as the host's KapfFilter makes
the Laison copies). Writes a new file; the input is left as it is.

    python -I kapf_lists.py fli  <in.cod> <out.cod>    # the FLI section's list only
    python -I kapf_lists.py main <in.cod> <out.cod>    # every section's but MAIN's

Used by tools/waw/seelow_install.sh for the vehicles' effects packs (2026-11-06):
mp\\mp_merv\\mp_merv_vehiclefx.cod with its FLI list emptied, and the copy the
host loads from sp\\vehfx\\mp_merv_vehiclefx.cod with only MAIN's left.
"""
import struct, sys

mode, src, dst = sys.argv[1], sys.argv[2], sys.argv[3]
assert mode in ('fli', 'main'), mode
d = bytearray(open(src, 'rb').read())
assert d[:4] == b'KAPF', 'not a KAPF archive'
sections = struct.unpack_from('<I', d, 0x0C)[0]
for s in range(1, sections):
    rec = 0x48 + 24 * (s - 1)
    name = d[rec:rec + 12].split(b'\0')[0].decode('latin-1')
    blocks, files = struct.unpack_from('<HH', d, rec + 12)
    empty = name.upper() == 'FLI' if mode == 'fli' else True
    if empty:
        struct.pack_into('<H', d, rec + 14, 0)
    print(f'section {s} {name}: {blocks} blocks, {files} files{" -> list emptied" if empty else ""}')
open(dst, 'wb').write(d)
