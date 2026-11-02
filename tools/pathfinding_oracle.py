#!/usr/bin/env python3
"""Execute original A* searches, compression, cursor and reset behavior. Requires Unicorn."""
import argparse
import hashlib
import struct
from pathlib import Path
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game, ROOT


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--resume', type=Path, help='Append after an incomplete independent-search prefix')
    args = parser.parse_args();completed = {};known_maps = set()
    if args.resume:
        for line in args.resume.read_text().splitlines():
            if not line or line.startswith('#'):
                continue
            fields = line.split('\t')
            if fields[0] == 'N':
                known_maps.add(int(fields[1]))
            elif fields[0] == 'F':
                index = int(fields[1]);completed[index] = completed.get(index, 0) + 1
            else:
                raise RuntimeError('Resume supports only independent-search prefixes')
    cpu, digest = load_game();stack, obj, map_object, tiles, heap, stop = 0x20000000, 0x30000000, 0x31000000, 0x40000000, 0x42000000, 0x10000000
    for address in (stack, obj, map_object, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(tiles, 0x1000000);cpu.mem_map(heap, 0x1000000);cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    cpu.mem_write(0x503a3f, b'\xc3');cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for pointer in (0x5210cc, 0x5210d0):
        cpu.mem_write(pointer, struct.pack('<I', stop + 0x100))
    context = dict(allocation=heap)

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def hook(machine, address, size, _):
        if address == 0x503a3f:
            amount = number(machine.reg_read(UC_X86_REG_ESP) + 4);pointer = context['allocation'];context['allocation'] += (max(amount, 1) + 15) & ~15
            if context['allocation'] > heap + 0x1000000:
                raise RuntimeError('A* oracle node allocation limit')
            machine.mem_write(pointer, bytes(amount));machine.reg_write(UC_X86_REG_EAX, pointer)
    cpu.hook_add(UC_HOOK_CODE, hook, begin=0x503a3f, end=0x503a3f)

    def call(address, arguments=()):
        cpu.mem_write(stack + 0x8000, struct.pack('<' + 'I' * (len(arguments) + 1), stop, *(value & 0xffffffff for value in arguments)));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.emu_start(address, stop, timeout=120000000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError(f'Original A* did not return from {address:x}')
        return cpu.reg_read(UC_X86_REG_EAX) & 255

    def initialize(size, data):
        context['allocation'] = heap;cpu.mem_write(0x3ed34b8, bytes(12));cpu.mem_write(obj, bytes(40));cpu.mem_write(map_object, bytes(256));cpu.mem_write(map_object + 4, struct.pack('<2I', size, tiles));cpu.mem_write(tiles, bytes(data))
        cpu.reg_write(UC_X86_REG_EAX, obj);call(0x48eb40)

    def snapshot(result):
        cursor = number(obj + 0x10);cpu.reg_write(UC_X86_REG_EAX, obj);last = call(0x4a6b00)
        record = bytes([result, cpu.mem_read(obj + 0x24, 1)[0], last]) + bytes(cpu.mem_read(obj + 0x14, 16)) + bytes([bool(cursor)])
        if cursor:
            record += bytes(cpu.mem_read(cursor + 4, 8))
        chain = b'';node = number(obj + 8);seen = set()
        while node:
            if node in seen:
                raise RuntimeError('Original A* cyclic path')
            seen.add(node);chain += bytes(cpu.mem_read(node + 4, 8));node = number(node + 0x1c)
        return record + struct.pack('<I', len(chain) // 8) + chain

    def search(start, end):
        cpu.reg_write(UC_X86_REG_ESI, obj);cpu.reg_write(UC_X86_REG_EAX, end[0] & 0xffffffff)
        return snapshot(call(0x48eda0, (*start, end[1], map_object)))

    def fingerprint(data):
        value = 0xcbf29ce484222325
        for byte in data:
            value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        return value

    def endpoints(size, scenario):
        start = ((scenario * 7 + 1) % size, (scenario * 11 + 2) % size);end = ((scenario * 13 + 3) % size, (scenario * 17 + 4) % size)
        if scenario % 11 == 0:
            end = start
        elif scenario % 11 == 1:
            end = (start[0], end[1])
        elif scenario % 11 == 2:
            end = (end[0], start[1])
        elif scenario % 11 == 3:
            start = (-1, start[1])
        elif scenario % 11 == 4:
            end = (size, end[1])
        return start, end

    if not args.resume:
        print(f'# ps_game.exe SHA256 {digest}; AStar::FindPath 0048eda0, PathToLine 0048f4a0, ResetPath 0048ec70, Reset 0048ec60, SetPath 004a6b20, CursorNext 004a6ae0. Allocation/critical-section ports isolated; all tree, successor, collision and compression code executes.')
        print('# N map_id extent tile_hex. F map_id start_X/Z goal_X/Z expected_hex. B map_id begins persistent sequence; Q search; C advance; R reset; P reset_path; S set_path. Result: return BYTE,found BYTE,last BYTE,scan_bounds,optional cursor,full compressed node chain. A filename SHA256 canonical_bytes FNV1a64 for 8 local queries.')
    maps = []
    for index in range(16):
        size = (2, 4, 9, 17, 25, 33, 41, 65)[index % 8];data = bytearray((size * size + 7) // 8)
        for z in range(size):
            for x in range(size):
                blocked = (x * 11 + z * 19 + index) % 17 == 0 if index < 8 else (x == size // 2 and z != size // 3) or (z == size // 2 and x != size // 3)
                if blocked:
                    at = z * size + x;data[at // 8] |= 1 << (at % 8)
        maps.append((size, bytes(data)))
        if index not in known_maps:
            print('N', index, size, data.hex(), sep='\t')
        for scenario in range(96):
            if scenario < completed.get(index, 0):
                continue
            initialize(size, data);start, end = endpoints(size, scenario);print('F', index, *start, *end, search(start, end).hex(), sep='\t')
    for index, (size, data) in enumerate(maps):
        initialize(size, data);print('B', index, sep='\t')
        for scenario in range(6):
            start, end = endpoints(size, scenario + 20);print('Q', *start, *end, search(start, end).hex(), sep='\t')
            # No-op search must preserve the complete prior path/cursor/flag/window.
            print('Q', 0, 0, 0, 0, search((0, 0), (0, 0)).hex(), sep='\t')
            cpu.reg_write(UC_X86_REG_ECX, obj);call(0x4a6ae0);print('C', snapshot(0).hex(), sep='\t')
            if scenario % 3 == 0:
                call(0x48ec70, (obj,));print('P', snapshot(0).hex(), sep='\t')
            elif scenario % 3 == 1:
                cpu.reg_write(UC_X86_REG_ESI, obj);call(0x48ec60);print('R', snapshot(0).hex(), sep='\t')
            else:
                cpu.reg_write(UC_X86_REG_EDI, obj);call(0x4a6b20, (5, -7));print('S', 5, -7, snapshot(0).hex(), sep='\t')
                cpu.reg_write(UC_X86_REG_ECX, obj);call(0x4a6ae0);print('C', snapshot(0).hex(), sep='\t')
    for path in sorted((ROOT / '.local/server-data/Map').glob('*.svmap'), key=lambda path: int(path.stem)):
        data = path.read_bytes();size = struct.unpack_from('<I', data)[0];tile_data = data[4:4 + (size * size + 7) // 8];records = b''
        for scenario in range(8):
            initialize(size, tile_data);x, z = (size // 2 + scenario * 7) % size, (size // 2 + scenario * 13) % size
            end = (min(x + (scenario % 3) * 3, size - 1), min(z + (scenario % 4) * 2, size - 1));records += search((x, z), end)
        print('A', path.name, hashlib.sha256(data).hexdigest(), len(records), fingerprint(records), sep='\t')


if __name__ == '__main__':
    main()
