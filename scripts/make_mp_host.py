"""Writes the host sources for CoD3MP.exe, the multiplayer executable.

The multiplayer program is the same host around another recompiled
executable, codmp_xenonf.xex: the same engine, linked at other addresses.
The host names guest functions and variables by their single player
addresses (sub_822F2678, 0x829F1BB8...). This copies CoD3Host into a
folder of the build with each of those replaced by its multiplayer twin
from CoD3Host/mp_addresses.txt, and:

  - leaves out the single player's diagnostic traces (title_trace,
    heap_trace, pool_trace, table_trace, anim_heap_trace, probe), which
    hook dozens of functions to watch them and change nothing; a small
    file of empty stand-ins takes the place of what the rest calls in them
  - writes kernel_stubs.cpp and kernel_import_table.inc for the multiplayer
    executable's imports (docs/kernel-imports-mp.txt), as
    gen_import_stubs.py does for the single player one's.

A single player address left in the code of a file that is kept, with no
multiplayer twin in the table, stops the build: hooking the function at the
same address in the other executable would hook something else. In
comments, an address without a twin is left as it is.

    python scripts/make_mp_host.py <output folder>
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
HOST = ROOT / "CoD3Host"
MAP = HOST / "mp_addresses.txt"
IMPORTS = ROOT / "docs" / "kernel-imports-mp.txt"

LEFT_OUT = {"title_trace.cpp", "heap_trace.cpp", "pool_trace.cpp", "table_trace.cpp",
            "anim_heap_trace.cpp", "probe.cpp", "kernel_stubs.cpp", "kernel_import_table.inc"}

# Files written for the multiplayer program alone, with its own addresses:
# copied as they are (they compile to nothing in the single player one).
MP_NATIVE = {"blood_render.cpp", "blood.cpp", "button_prompts.cpp", "lobby_rules.cpp", "mp_fixes.cpp", "bots.cpp", "dom.cpp", "class_editor.cpp", "native_options.cpp"}

STANDINS = """// Written by scripts/make_mp_host.py: the single player's diagnostic traces
// are not in the multiplayer program, and these take the place of what the
// rest of the host calls in them.
#include "heap_trace.h"
#include "pool_trace.h"

void HeapTrace::Destroying(const PPCContext&, uint8_t*) {}
void PoolTrace::Report() {}
void PoolTrace::Waited(uint64_t) {}
"""

# An address in the single player image: 0x8XXXXXXX or sub_8XXXXXXX, or the
# bare eight digits a macro pastes after sub_.
TOKEN = re.compile(r"(?<![0-9A-Za-z])(0x|sub_)?(8[2-9A-C][0-9A-F]{6})(?![0-9A-Fa-f])")
IMAGE = (0x82000000, 0x82C30000)


def load_map():
    table = {}
    for line in MAP.read_text(encoding="ascii").splitlines():
        line = line.split("#", 1)[0].strip()
        if not line:
            continue
        sp, mp = line.split()
        table[int(sp, 16)] = int(mp, 16)
    return table


def code_part(line, in_block):
    """The part of a line outside comments, and whether a /* */ is still open."""
    out = []
    i = 0
    while i < len(line):
        if in_block:
            end = line.find("*/", i)
            if end < 0:
                return "".join(out), True
            i = end + 2
            in_block = False
        elif line.startswith("//", i):
            break
        elif line.startswith("/*", i):
            in_block = True
            i += 2
        elif line[i] == '"':
            # A string: kept as code (a printed address is the program's own).
            j = i + 1
            while j < len(line) and line[j] != '"':
                j += 2 if line[j] == "\\" else 1
            out.append(line[i:j + 1])
            i = j + 1
        else:
            out.append(line[i])
            i += 1
    return "".join(out), in_block


def _was_mp_ifdef(lines, at):
    """Whether the #if that the #else at line `at` belongs to is #ifdef COD3_MP."""
    depth = 0
    for i in range(at - 1, -1, -1):
        d = lines[i].strip()
        if d.startswith("#endif"):
            depth += 1
        elif d.startswith("#if"):
            if depth == 0:
                return d.startswith("#ifdef COD3_MP") or d == "#if COD3_MP"
            depth -= 1
    return False


def translate(path, table, problems):
    text = path.read_text(encoding="ascii", errors="strict")
    lines = text.split("\n")
    in_block = False
    # The preprocessor's blocks: one that is single player only (#ifndef
    # COD3_MP, or the #else of #ifdef COD3_MP) is not compiled here, so its
    # addresses need no twin.
    # A block that is multiplayer only (#ifdef COD3_MP) already names the
    # multiplayer's own addresses: those are left as they are, unchecked.
    stack = []   # for each open #if: "sp" or "mp" when its current branch is only that program's, else None
    for n, line in enumerate(lines):
        directive = line.strip()
        if directive.startswith("#if"):
            if directive.startswith("#ifndef COD3_MP") or directive == "#if !COD3_MP":
                stack.append("sp")
            elif directive.startswith("#ifdef COD3_MP") or directive == "#if COD3_MP":
                stack.append("mp")
            else:
                stack.append(None)
        elif directive.startswith("#else") and stack:
            stack[-1] = {"sp": "mp", "mp": "sp"}.get(stack[-1])
        elif directive.startswith("#endif") and stack:
            stack.pop()
        if "sp" in stack:
            lines[n] = line
            in_block = code_part(line, in_block)[1]
            continue
        if "mp" in stack:
            in_block = code_part(line, in_block)[1]
            continue
        skipped = False
        code, in_block_after = code_part(line, in_block)
        for m in ([] if skipped else TOKEN.finditer(code)):
            value = int(m.group(2), 16)
            if not (IMAGE[0] < value <= IMAGE[1]) or value in table:
                continue
            problems.append(f"{path.name}:{n + 1}: {m.group(0)} has no multiplayer twin in {MAP.name}")
        in_block = in_block_after

        def replace(m):
            value = int(m.group(2), 16)
            if value not in table:
                return m.group(0)
            return (m.group(1) or "") + f"{table[value]:08X}"
        lines[n] = TOKEN.sub(replace, line)
    return "\n".join(lines)


def write_imports(out):
    names = sorted({line.split()[1] for line in IMPORTS.read_text(encoding="ascii").splitlines() if len(line.split()) == 2})
    implemented = set()
    for listing in (HOST / "implemented_imports.txt", HOST / "implemented_imports_mp.txt"):
        for line in listing.read_text(encoding="ascii").splitlines():
            line = line.split("#", 1)[0].strip()
            if line:
                implemented.add(line)
    stubbed = [n for n in names if n not in implemented]
    (out / "kernel_stubs.cpp").write_text(
        "// Written by scripts/make_mp_host.py from docs/kernel-imports-mp.txt.\n"
        '#include "kernel.h"\n\n' +
        "\n".join(f'PPC_FUNC(__imp__{n}) {{ Kernel::Unimplemented("{n}", ctx, base); }}' for n in stubbed) + "\n",
        encoding="ascii", newline="\n")
    (out / "kernel_import_table.inc").write_text(
        "// Written by scripts/make_mp_host.py. Import name to host function.\n" +
        "\n".join(f'{{ "{n}", __imp__{n} }},' for n in names) + "\n", encoding="ascii", newline="\n")
    return len(names), len(stubbed)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    out = pathlib.Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    table = load_map()
    problems = []
    written = 0
    for path in sorted(HOST.iterdir()):
        if not path.is_file() or path.suffix not in (".cpp", ".h", ".inc") or path.name in LEFT_OUT:
            continue
        text = path.read_text(encoding="ascii") if path.name in MP_NATIVE else translate(path, table, problems)
        target = out / path.name
        # Only rewritten when changed, so the build does not redo every file.
        if not target.exists() or target.read_text(encoding="ascii") != text:
            target.write_text(text, encoding="ascii", newline="\n")
        written += 1
    standins = out / "mp_trace_standins.cpp"
    if not standins.exists() or standins.read_text(encoding="ascii") != STANDINS:
        standins.write_text(STANDINS, encoding="ascii", newline="\n")
    imports, stubbed = write_imports(out)
    if problems:
        print("\n".join(problems), file=sys.stderr)
        sys.exit(f"{len(problems)} single player addresses without a multiplayer twin")
    print(f"CoD3MP host: {written} files in {out}, {imports} imports ({stubbed} still stubs)")


if __name__ == "__main__":
    main()
