"""Structural reader/writer for SCEN .scnb files from the user's own CoD3.

Big endian. Header size 0x58. +0x54 is the file-relative relocation-tail
offset; tail is count, file-relative table offset, then the table: bit width,
chain roots. Pointer decoding is the game's sub_820F99D8 / sub_820F9910;
scene load is sub_8247C6C8. The serializer preserves opaque exporter/runtime
fields and chain topology, and re-encodes every pointer. It does NOT invent
scene records or claim unknown sections are safe to remove.

Known header arrays (count, pointer): entities +0x10/+0x14 (pointer array),
model instances +0x18/+0x1C (240 bytes), model groups +0x20/+0x24 (16 bytes),
effects +0x30/+0x34 (96 bytes). Consumers: sub_8247B780, sub_8247C590,
sub_824722C8. Entity property pairs are +0xD4/+0xD8, 8-byte records.

usage: python -I scnb.py --roundtrip <scene.scnb> [more ...]
"""
import argparse
import struct
from pathlib import Path


def word(data, offset):
    return struct.unpack_from('>I', data, offset)[0]


class Scene:
    """Editable fixed-layout scene; pointers are decoded to file offsets.

    data retains all non-pointer bytes. Change pointers through targets, not
    through data. write() regenerates pointer encodings and checks ranges.
    Resizing requires a separate semantic serializer, intentionally absent.
    """
    def __init__(self, data):
        if data[:8] != bytes.fromhex('5343454e3fab851f'):
            raise ValueError('unsupported SCEN signature/version')
        self.data = bytearray(data)
        self.size = len(data)
        self.tail = word(data, 0x54)
        count, table = struct.unpack_from('>2I', data, self.tail)
        if table != self.tail + 8 or count < 1 or table + count * 4 != len(data):
            raise ValueError('unsupported relocation table layout')
        self.bits = word(data, table)
        if not 1 <= self.bits <= 30:
            raise ValueError('invalid relocation width')
        self.roots = [word(data, table + 4 * k) for k in range(1, count)]
        self.targets = {}
        self.encoding = {}
        shift = 31 - self.bits
        mask = (1 << shift) - 1
        for root in self.roots:
            at = root
            while True:
                if at in self.targets or at % 4 or not 0 <= at <= self.tail - 4:
                    raise ValueError(f'invalid/duplicate relocation at {at:X}')
                value = word(data, at)
                if value & 0x80000000:
                    delta = value if value & 0x40000000 else value & 0x7FFFFFFF
                    step = 0
                    mode = value >> 30
                else:
                    delta = value & mask
                    if delta & (1 << (shift - 1)):
                        delta -= 1 << shift
                    step = value >> shift
                    mode = 0
                    if not step:
                        raise ValueError('unterminated pointer chain')
                target = (at + delta) & 0xFFFFFFFF
                if not 0 <= target < self.tail:
                    raise ValueError(f'pointer {at:X} targets outside scene: {target:X}')
                self.targets[at] = target
                self.encoding[at] = (mode, step)
                struct.pack_into('>I', self.data, at, target)
                if not step:
                    break
                at += step
        self._array(0x10, 4)
        self._array(0x18, 240)
        self._array(0x20, 16)
        self._array(0x30, 96)

    def _array(self, at, stride):
        count = word(self.data, at)
        if not count:
            return []
        start = self.targets[at + 4]
        if start + count * stride > self.tail:
            raise ValueError(f'array {at:X} exceeds scene')
        return list(range(start, start + count * stride, stride))

    @property
    def entities(self):
        return [self.targets[at] for at in self._array(0x10, 4)]

    @property
    def instances(self):
        return self._array(0x18, 240)

    @property
    def groups(self):
        return self._array(0x20, 16)

    @property
    def effects(self):
        return self._array(0x30, 96)

    def string(self, at):
        end = self.data.index(0, at, self.tail)
        return self.data[at:end].decode('latin-1')

    def properties(self, entity):
        count = word(self.data, entity + 0xD4)
        if not count:
            return []
        start = self.targets[entity + 0xD8]
        return [(self.string(self.targets[start + 8*k]),
                 self.string(self.targets[start + 8*k + 4])) for k in range(count)]

    def entity_info(self, entity):
        # sub_82479E08 transfers +8 to runtime origin (+336), +80 to
        # runtime angles (+352), and the class pointer +4 to runtime +548.
        return {'offset': entity,
                'class': self.string(self.targets[entity + 4]),
                'origin': struct.unpack_from('>3f', self.data, entity + 8),
                'angles': struct.unpack_from('>3f', self.data, entity + 80),
                'properties': self.properties(entity)}

    def with_instances_disabled(self):
        """Experimental harness: disable zone model/effect consumers.

        sub_8247C590 guards instance (+24) and group (+32) loops on zero;
        sub_824722C8 likewise guards effects (+48). Keep payloads, all
        relocation links, entities and visibility/other header fields intact.
        This is deliberately NOT an empty scene or a standalone map writer.
        Runtime drawing/streaming behavior still needs an in-game test.
        """
        scene = Scene(self.write())
        for at in (0x18, 0x20, 0x30):
            struct.pack_into('>I', scene.data, at, 0)
        return scene

    def write(self):
        if len(self.data) != self.size:
            raise ValueError('resizing fixed-layout scene is unsupported')
        data = bytearray(self.data)
        shift = 31 - self.bits
        for at, target in self.targets.items():
            if not 0 <= target < self.tail:
                raise ValueError(f'pointer {at:X} outside scene')
            mode, step = self.encoding[at]
            delta = target - at
            if mode:
                value = delta & 0xFFFFFFFF
                if mode == 2 and 0 <= delta < 0x40000000:
                    value |= 0x80000000
                elif mode != 3 or value >> 30 != 3:
                    raise ValueError('terminal pointer mode no longer fits')
            else:
                if not -(1 << (shift-1)) <= delta < (1 << (shift-1)):
                    raise ValueError('pointer exceeds existing chain encoding')
                value = step << shift | (delta & ((1 << shift)-1))
            struct.pack_into('>I', data, at, value)
        return bytes(data)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--roundtrip', action='store_true', required=True)
    parser.add_argument('files', nargs='+', type=Path)
    args = parser.parse_args()
    for path in args.files:
        data = path.read_bytes()
        scene = Scene(data)
        if scene.write() != data:
            raise ValueError(f'{path}: roundtrip mismatch')
        print(f'{path.name}: exact, {len(scene.targets)} pointers, '
              f'{len(scene.entities)} entities, {len(scene.instances)} instances, '
              f'{len(scene.groups)} groups, {len(scene.effects)} effects')


if __name__ == '__main__':
    main()
