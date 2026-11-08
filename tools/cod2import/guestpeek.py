"""Read a running CoD3MP's guest memory (read only), for finding layouts.

usage:
  python -I guestpeek.py fields <table address>     the weapon file's field table:
        12-byte entries (name, offset, kind) until a name is not text
  python -I guestpeek.py words <address> <count>    big-endian words
  python -I guestpeek.py text <address>             a string
Guest::Base is found from CoD3MP.pdb (llvm-pdbutil), as Codex's snapshot_guest.py does.
"""
import ctypes as c
import re
import struct
import subprocess
import sys
from ctypes import wintypes as w
from pathlib import Path

EXE = Path(r"D:\Call of Duty 3 (USA, Europe)\CoD3-PC-Port-0.4.0\CoD3MP.exe")


def open_game():
    out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq CoD3MP.exe", "/FO", "CSV", "/NH"], capture_output=True, text=True).stdout
    m = re.search(r'"CoD3MP\.exe","(\d+)"', out)
    if not m:
        raise SystemExit("CoD3MP.exe is not running")
    pid = int(m.group(1))
    k = c.WinDLL("kernel32", use_last_error=True)
    p = c.WinDLL("psapi", use_last_error=True)
    k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    k.OpenProcess.restype = w.HANDLE
    k.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
    k.ReadProcessMemory.restype = w.BOOL
    p.EnumProcessModules.argtypes = [w.HANDLE, c.POINTER(c.c_void_p), w.DWORD, c.POINTER(w.DWORD)]
    h = k.OpenProcess(0x410, False, pid)
    if not h:
        raise SystemExit("cannot open the game")
    mods = (c.c_void_p * 1024)()
    need = w.DWORD()
    p.EnumProcessModules(h, mods, c.sizeof(mods), c.byref(need))

    def read(at, size):
        b = c.create_string_buffer(size)
        n = c.c_size_t()
        if not k.ReadProcessMemory(h, c.c_void_p(at), b, size, c.byref(n)) or n.value != size:
            raise OSError(f"unreadable {at:x}")
        return b.raw

    image = EXE.read_bytes()
    pe = struct.unpack_from("<I", image, 0x3C)[0]
    sections = pe + 24 + struct.unpack_from("<H", image, pe + 20)[0]
    sym = subprocess.check_output([r"C:\Program Files\LLVM\bin\llvm-pdbutil.exe", "dump", "--globals",
                                   "--global-name=Guest::Base", str(EXE.with_suffix(".pdb"))], text=True)
    sec, off = map(int, re.findall(r"addr = (\d+):(\d+)", sym)[0])
    rva = struct.unpack_from("<I", image, sections + (sec - 1) * 40 + 12)[0]
    base = struct.unpack("<Q", read(mods[0] + rva + off, 8))[0]
    return lambda at, size: read(base + at, size)


def main():
    blob = open_game()
    u32 = lambda at: struct.unpack(">I", blob(at, 4))[0]

    def text(at):
        try:
            raw = blob(at, 64).split(b"\0")[0]
        except OSError:
            return None
        return raw.decode("ascii") if raw and all(32 <= b < 127 for b in raw) else None

    if sys.argv[1] == "fields":
        at = int(sys.argv[2], 16)
        for i in range(1200):
            name = text(u32(at + 12 * i))
            if name is None:
                print(f"end at entry {i}")
                break
            print(f"{name} {u32(at + 12 * i + 4)} {u32(at + 12 * i + 8)}")
    elif sys.argv[1] == "words":
        at, n = int(sys.argv[2], 16), int(sys.argv[3])
        print(" ".join(f"{u32(at + 4 * i):08X}" for i in range(n)))
    elif sys.argv[1] == "text":
        print(text(int(sys.argv[2], 16)))


main()
