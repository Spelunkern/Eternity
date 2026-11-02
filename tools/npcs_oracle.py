#!/usr/bin/env python3
"""Execute the original NPC definition loader and positional lookup. Requires Unicorn."""
import hashlib
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game, ROOT


def main():
    cpu, digest = load_game()
    stack, obj, file, heap, stop = 0x20000000, 0x30000000, 0x31000000, 0x40000000, 0x10000000
    for address in (0, stack, obj, file, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(heap, 0x4000000);cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    for address in (0x503ae3, 0x510785):
        cpu.mem_write(address, b'\xc3')
    context = dict(data=b'', position=0, allocation=heap)

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503ae3:
            amount = number(sp + 4);pointer = context['allocation'];context['allocation'] += (max(amount, 1) + 15) & ~15
            if context['allocation'] > heap + 0x4000000:
                raise RuntimeError('NPC oracle allocation limit')
            machine.mem_write(pointer, bytes(amount));machine.reg_write(UC_X86_REG_EAX, pointer)
        elif address == 0x510785:
            fd, pointer, amount = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
            data = context['data'][context['position']:context['position'] + amount]
            if fd != 12 or len(data) != amount:
                raise RuntimeError('Original NPC loader input changed')
            machine.mem_write(pointer, data);context['position'] += amount;machine.reg_write(UC_X86_REG_EAX, amount)

    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, arguments=()):
        cpu.mem_write(stack + 0x8000, struct.pack('<' + 'I' * (len(arguments) + 1), stop, *arguments));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.emu_start(address, stop, count=15000000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original NPC operation did not return')

    def load(data):
        context.update(data=data, position=0, allocation=heap);cpu.mem_write(obj, bytes(104));cpu.mem_write(file, struct.pack('<I', 12))
        cpu.reg_write(UC_X86_REG_ECX, obj);call(0x4c9410, (file,))
        if cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
            raise RuntimeError('Original NPC load failed')
        records = b'';counts = []
        for group in range(13):
            count, pointer = struct.unpack('<2I', cpu.mem_read(obj + 8 * group, 8));counts.append(count);records += struct.pack('<I', count)
            width = (304, 1120)[group] if group < 2 else 292
            for i in range(count):
                at = pointer + width * i;records += bytes(cpu.mem_read(at, 276))
                for count_offset, pointer_offset in ((276, 280), (284, 288)):
                    length, link = number(at + count_offset), number(at + pointer_offset)
                    records += struct.pack('<I', length) + (bytes(cpu.mem_read(link, length * 2)) if length else b'')
                if group == 0:
                    length, items = number(at + 296), number(at + 300)
                    records += bytes(cpu.mem_read(at + 292, 1)) + struct.pack('<I', length) + (bytes(cpu.mem_read(items, length * 2)) if length else b'')
                elif group == 1:
                    records += bytes(cpu.mem_read(at + 292, 828))
        # Lookup must use group and row position rather than the file's embedded IDs.
        for group in range(-1, 16):
            for index in (-1, 0, 1, 2, (counts[group - 1] + 1) if 1 <= group <= 13 else 7):
                cpu.reg_write(UC_X86_REG_EDX, obj);cpu.reg_write(UC_X86_REG_ECX, group & 0xffffffff);cpu.reg_write(UC_X86_REG_EAX, index & 0xffffffff);call(0x41d640)
                valid = 1 <= group <= 13 and 1 <= index <= counts[group - 1]
                expected = number(obj + (group - 1) * 8 + 4) + (index - 1) * ((304, 1120)[group - 1] if group <= 2 else 292) if valid else 0
                if cpu.reg_read(UC_X86_REG_EAX) != expected:
                    raise RuntimeError('Original positional NPC lookup changed')
        return records

    print(f'# ps_game.exe SHA256 {digest}; CNpcData::Load 004c9410, GetNpc 0041d640. File read and allocation ports isolated; 85 lookup cases per load.')
    print('# A distribution SHA256 consumed canonical_bytes FNV1a64. S input_hex consumed canonical_hex. Canonical: 13 counts, common 276-byte prefixes, start/end WORD lists, shop BYTE/item list or three 276-byte gates.')
    path = ROOT / '.local/server-data/NpcQuest.SData';data = path.read_bytes();records = load(data)
    value = 0xcbf29ce484222325
    for byte in records:
        value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
    print('A', hashlib.sha256(data).hexdigest(), context['position'], len(records), value, sep='\t')
    def string(value):
        return struct.pack('<I', len(value)) + value
    for scenario in range(6):
        data = b''
        for group in range(13):
            count = (scenario + group) % 3;data += struct.pack('<I', count)
            for i in range(count):
                data += struct.pack('<BH', 255 - group, 500 + i)
                if group == 0:
                    data += bytes([scenario + 2])
                data += struct.pack('<3iI', -500 + i, scenario, 2147483647, (0, 1, 2, 3, 0xffffffff, 65536)[scenario])
                data += string((b'', b'a', b'NPC name with more than thirty characters', bytes(range(1, 256)), b'with\0suffix', b'plain')[scenario]) + string(b'discarded text')
                if group == 0:
                    data += struct.pack('<I', 2) + bytes([0, 255, 128, 7])
                elif group == 1:
                    for gate in range(3):
                        data += struct.pack('<H3f', 500 + gate, gate + 0.25, (-100, 0, 0.1, 100, 16777216, -0.9)[scenario], -gate - 0.5)
                        data += string(bytes([65 + gate]) * (gate * 127)) + struct.pack('<I', 0xffffffff - gate)
                data += struct.pack('<I3HI', 3, 0, 65535, 500, 0)
        records = load(data);print('S', data.hex(), context['position'], records.hex(), sep='\t')


if __name__ == '__main__':
    main()
