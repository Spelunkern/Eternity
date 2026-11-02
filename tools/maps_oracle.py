#!/usr/bin/env python3
"""Execute original CMap::LoadMap on staged and synthetic server maps. Requires Unicorn."""
import hashlib
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game, ROOT


def main():
    cpu, digest = load_game()
    stack, obj, filename, heap, stop = 0x20000000, 0x30000000, 0x31000000, 0x40000000, 0x10000000
    for address in (0, stack, obj, filename, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(heap, 0x4000000);cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    for address in (0x503ae3, 0x505afd, 0x510785, 0x50c649):
        cpu.mem_write(address, b'\xc3')
    context = dict(data=b'', position=0, allocation=heap)

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503ae3:
            amount = struct.unpack('<I', machine.mem_read(sp + 4, 4))[0]
            pointer = context['allocation'];context['allocation'] += (max(amount, 1) + 15) & ~15
            if context['allocation'] > heap + 0x4000000:
                raise RuntimeError('Map oracle allocation limit')
            machine.mem_write(pointer, bytes(amount));machine.reg_write(UC_X86_REG_EAX, pointer)
        elif address == 0x505afd:
            machine.reg_write(UC_X86_REG_EAX, 12)
        elif address == 0x50c649:
            machine.reg_write(UC_X86_REG_EAX, 0)
        elif address == 0x510785:
            fd, pointer, amount = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
            data = context['data'][context['position']:context['position'] + amount]
            if fd != 12 or len(data) != amount:
                raise RuntimeError('Original map loader reached invalid/truncated file')
            machine.mem_write(pointer, data);context['position'] += amount;machine.reg_write(UC_X86_REG_EAX, amount)

    cpu.hook_add(UC_HOOK_CODE, hook)

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def canonical():
        result = bytes(cpu.mem_read(obj + 4, 4))
        size = number(obj + 0x10);result += struct.pack('<I', size) + bytes(cpu.mem_read(number(obj + 8), size))
        for count_offset, pointer_offset, width in ((0x68, 0x6c, 20), (0x60, 0x64, 12)):
            count, pointer = number(obj + count_offset), number(obj + pointer_offset)
            result += struct.pack('<I', count) + bytes(cpu.mem_read(pointer, count * width)) if count else bytes(4)
        count, pointer = number(obj + 0x14), number(obj + 0x18);result += struct.pack('<I', count)
        for i in range(count):
            area = pointer + i * 40;result += bytes(cpu.mem_read(area, 36))
            spawns, spawn_pointer = number(area + 0x20), number(area + 0x24)
            for j in range(spawns):
                result += bytes(cpu.mem_read(spawn_pointer + j * 36, 8))
        count, pointer = number(obj + 0x40), number(obj + 0x44);result += struct.pack('<I', count)
        for i in range(count):
            npc = pointer + i * 20;result += bytes(cpu.mem_read(npc, 12))
            positions, position_pointer, angles = struct.unpack('<3I', cpu.mem_read(npc + 8, 12))
            for j in range(positions):
                result += bytes(cpu.mem_read(position_pointer + j * 12, 12)) + bytes(cpu.mem_read(angles + j * 4, 4))
        count, pointer = number(obj + 0x50), number(obj + 0x54)
        result += struct.pack('<I', count) + (bytes(cpu.mem_read(pointer, count * 48)) if count else b'')
        count, pointer = number(obj + 0x58), number(obj + 0x5c);result += struct.pack('<I', count)
        for i in range(count):
            bind = pointer + i * 16;result += bytes(cpu.mem_read(bind, 12))
            boxes, box_pointer = number(bind), number(bind + 12)
            if boxes:
                result += bytes(cpu.mem_read(box_pointer, boxes * 24))
        count, pointer = number(obj + 0x70), number(obj + 0x74)
        result += struct.pack('<I', count) + (bytes(cpu.mem_read(pointer, count * 40)) if count else b'')
        return result

    def load(data):
        context.update(data=data, position=0, allocation=heap);cpu.mem_write(obj, bytes(0x1000))
        cpu.mem_write(stack + 0xf000, struct.pack('<I', stop))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0xf000);cpu.reg_write(UC_X86_REG_EDI, obj);cpu.reg_write(UC_X86_REG_ECX, filename)
        cpu.emu_start(0x425780, stop, count=10000000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop or cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
            raise RuntimeError('Original map load failed')
        return canonical()

    def fingerprint(data):
        value = 0xcbf29ce484222325
        for byte in data:
            value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        return value

    print(f'# ps_game.exe SHA256 {digest}; CMap::LoadMap 00425780; CBitArray::Create 00425250. Original constructors/derived fields execute; only file I/O and allocation isolated.')
    print('# A filename SHA256 consumed canonical_bytes FNV1a64; S input_hex consumed canonical_hex. Canonical record layout documented in tests/maps_unit.cpp; pointer/runtime-only fields omitted; allocation padding zeroed.')
    for path in sorted((ROOT / '.local/server-data/Map').glob('*.svmap'), key=lambda path: int(path.stem)):
        data = path.read_bytes();records = load(data)
        print('A', path.name, hashlib.sha256(data).hexdigest(), context['position'], len(records), fingerprint(records), sep='\t')
    for size in (0, 7, 40, 81):
        data = struct.pack('<I', size) + bytes((i * 17 + size) & 255 for i in range((size * size + 7) // 8))
        if size == 0:
            data += bytes(7 * 4)
        else:
            data += struct.pack('<I3fii', 1, 1.25, -2.5, 3.75, -1, 30000)
            data += struct.pack('<I3f', 1, 4, 5, 6)
            boxes = ((1.25, 2, 9.75, 8.99, 3, 8.2), (4, 2, 3, 4.5, 2, 3.75), (0, 0, 0, float('inf'), 0, float('nan')))
            data += struct.pack('<I', len(boxes))
            for box in boxes:
                data += struct.pack('<6fI4i', *box, 2, 1, 5, -1, 0)
            data += struct.pack('<I3i', 1, 3, 99, 4)
            for angle in (0, 1.25, -1.25, 100):
                data += struct.pack('<4f', 7, 8, 9, angle)
            data += struct.pack('<I', 2)
            for country in (0, 1001):
                data += struct.pack('<3fiHHi3f', 10, 11, 12, country, 1, 65535, 9, 20, 21, 22)
            data += struct.pack('<I3i6f', 1, 1, -1, 7, 1, 2, 3, 4, 5, 6)
            data += struct.pack('<I6fiBBH', 1, 1, 2, 3, 4, 5, 6, -2, 255, 128, 65535)
        records = load(data)
        print('S', data.hex(), context['position'], records.hex(), sep='\t')


if __name__ == '__main__':
    main()
