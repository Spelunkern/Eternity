#!/usr/bin/env python3
"""Run original debuff cancellation, duel-list cleanup and duel end with real ledgers."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
from equipment_oracle import definition

OFFSETS = (0x17c, 0x1220, 0x12ec, 0x1284, 0x1340, 0x133c, 0x1344, 0x1348, 0x1350, 0x1358,
           0x1360, 0x1370, 0x54cc, 0x54d0, 0x54d4, 0x1528, 0x152c)


def main():
    cpu, digest = load_game()
    stack, user, info, nodes, objects, items, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x10000000
    for address in (stack, user, info, nodes, objects, items, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    for address, cleanup in ((0x454280, 0), (0x481990, 0), (0x4827b0, 0), (0x482590, 0), (0x454ba0, 4), (0x4d4f10, 8)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    cpu.mem_write(0x546f3c, struct.pack('<I', objects))
    cpu.mem_write(items + 0x30, struct.pack('<I', items + 0x1000));cpu.mem_write(items + 0x1000, definition(16, 1));cpu.mem_write(items + 0x46, b'\x03')
    cpu.mem_write(0x6882ac + 168 * 3, definition(30, 3, True))
    trace = []

    def word(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def signed(address):
        return struct.unpack('<i', cpu.mem_read(address, 4))[0]

    def hook(machine, address, size, context):
        if address == 0x4d4f10:
            sp = machine.reg_read(UC_X86_REG_ESP);ptr, length = struct.unpack('<II', cpu.mem_read(sp + 4, 8))
            data = bytes(cpu.mem_read(ptr, length))
            if length != 9 or data[:2] != b'\x02\x09':
                raise RuntimeError('Unexpected duel cleanup persistence request')
            identity, level = struct.unpack_from('<HB', data, 6);trace.append(f'D{identity},{level}')
        elif address == 0x47c6db:
            trace.append('F' + str(word(machine.reg_read(UC_X86_REG_ESI) + 8)))
        elif address == 0x4b8aa0:
            trace.append(f'H{signed(user+0x54cc)},{signed(user+0x54d0)},{signed(user+0x1528)}')
        else:
            kind = {0x481990: 'M', 0x4827b0: 'V', 0x482590: 'S', 0x454ba0: 'A'}[address]
            trace.append(kind + ','.join(str(value) for value in (signed(user+0x1340), signed(user+0x133c), signed(user+0x1344), signed(user+0x1348), cpu.mem_read(user+0x1354,1)[0], word(user+0x1358))))

    for address in (0x4d4f10, 0x47c6db, 0x4b8aa0, 0x481990, 0x4827b0, 0x482590, 0x454ba0):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    rng = random.Random(0x47c610)
    details = (0, 6, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 81, 82, 88, 90, 91, 94, 95)
    cases = []
    for detail in details:
        for effect in (0, 3, 4):
            for shape in (0, 100, 101, 102):
                cases.append((0, rng.randrange(2), rng.choice((-0x80000000, -1, 0, 2, 0x7fffffff)), shape, [(detail, effect, 200, 1)]))
    for _ in range(512):
        cases.append((1, rng.randrange(2), rng.choice((-0x80000000, -1, 0, 2, 0x7fffffff)), rng.choice((0, 13, 100, 101, 102)),
                      [(rng.choice(details), rng.choice((0, 3, 4)), rng.choice((127, 200, 201)), rng.choice((1, 2, 100))) for _ in range(rng.randrange(9))]))
    for _ in range(128):
        cases.append((2, rng.randrange(2), rng.choice((-0x80000000, -1, 0, 2, 0x7fffffff)), rng.choice((0, 13, 100, 101, 102)), []))
    print(f'# ps_game.exe SHA256 {digest}; CancleApplySkillDebuff 00487ed0, PvPClearDebuff 0047c610, PvPModeEnd 0047b200; original ability/element/list/ledger code')
    print('# op armor counter shape duelState duelType targetType count; nodes(detail effect id level); final DWORDs ' + ' '.join(hex(offset) for offset in OFFSETS) + '; shape form noMob hpActive hpID cachedPointer damageCount; remainingCount IDs; eventCount events')
    root = user + 0x6000;damage_root = user + 0x7000
    for op, armor, counter, shape, effects in cases:
        duel_state = rng.choice((-1, 0, 1, 3, 5));duel_type = rng.choice((-0x80000000, -1, 0, 2));target_type = rng.choice((-1, 0, 1, 2))
        cpu.mem_write(user, bytes(0x9000));cpu.mem_write(objects, bytes(0x1000));cpu.mem_write(nodes, bytes(0x10000))
        initial = (10000, 100, 2, 8, counter, counter, counter, counter, 888, 321, 4, 123456, duel_state, duel_type, 777, target_type, 999)
        for offset, value in zip(OFFSETS, initial):
            cpu.mem_write(user + offset, struct.pack('<I', value & 0xffffffff))
        cpu.mem_write(user + 0x578c, struct.pack('<I', 9001));cpu.mem_write(user + 0x1354, bytes([shape]));cpu.mem_write(user + 0x134c, struct.pack('<H', 42))
        cpu.mem_write(user + 0x1366, b'\x01');cpu.mem_write(user + 0x1368, b'\x01');cpu.mem_write(user + 0x136a, struct.pack('<H', 200));cpu.mem_write(user + 0x135c, struct.pack('<I', 0x778899aa))
        if armor:
            cpu.mem_write(user + 0x1c8, struct.pack('<I', items))
        cpu.mem_write(root, struct.pack('<II', root, root));cpu.mem_write(user + 0xa94, struct.pack('<II', root, root))
        cpu.mem_write(user + 0xab4, struct.pack('<I', len(effects)))
        for index, (detail, effect, identity, level) in enumerate(effects):
            node, descriptor = nodes + index * 128, info + index * 160
            data = bytearray(160);struct.pack_into('<BH', data, 0x31, effect, detail)
            for ability, (kind, value) in enumerate(((1, 17), (4, 31), (16, 9))):
                struct.pack_into('<BxH', data, 0x7c + ability * 4, kind, value)
            cpu.mem_write(descriptor, bytes(data));last = word(root)
            cpu.mem_write(node, struct.pack('<5I', last, root, 1000 + index, identity, level));cpu.mem_write(node + 0x30, struct.pack('<I', descriptor))
            cpu.mem_write(last + 4, struct.pack('<I', node));cpu.mem_write(root, struct.pack('<I', node))
        first, second = nodes + 0x8000, nodes + 0x8020
        cpu.mem_write(damage_root, struct.pack('<II', second, first));cpu.mem_write(first, struct.pack('<II', damage_root, second));cpu.mem_write(second, struct.pack('<II', first, damage_root))
        for node, source in ((first, 1), (second, 2)):
            cpu.mem_write(node + 8, b'\x01');cpu.mem_write(node + 12, struct.pack('<I', source))
        cpu.mem_write(user + 0x1538, struct.pack('<II', damage_root, damage_root));cpu.mem_write(user + 0x1558, struct.pack('<I', 2))
        cpu.mem_write(0x3ed3518, bytes(12));trace.clear()
        cpu.mem_write(stack + 0x8000, struct.pack('<II', stop, user));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_EAX, user)
        cpu.reg_write(UC_X86_REG_EDI, user if op==2 else info)
        cpu.emu_start((0x487ed0, 0x47c610, 0x47b200)[op], stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original duel cleanup did not return')
        remaining, cursor = [], word(root + 4)
        while cursor != root:
            remaining.append(word(cursor + 8));cursor = word(cursor + 4)
        print(op, armor, counter, shape, duel_state, duel_type, target_type, len(effects), *(value for effect in effects for value in effect),
              *(word(user + offset) for offset in OFFSETS), cpu.mem_read(user+0x1354,1)[0], struct.unpack('<H',cpu.mem_read(user+0x134c,2))[0],cpu.mem_read(user+0x1366,1)[0],
              cpu.mem_read(user+0x1368,1)[0],struct.unpack('<H',cpu.mem_read(user+0x136a,2))[0],word(user+0x135c),word(user+0x1558),len(remaining),*remaining,len(trace),*trace,sep='\t')


if __name__ == '__main__':
    main()
