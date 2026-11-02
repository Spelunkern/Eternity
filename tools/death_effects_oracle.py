#!/usr/bin/env python3
"""Execute original death effect cleanup over live intrusive lists and ability code."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
from equipment_oracle import definition


def main():
    cpu, digest = load_game()
    stack, user, info, nodes, objects, items, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x10000000
    for address in (stack, user, info, nodes, objects, items, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    for address, cleanup in ((0x454280, 0), (0x481990, 0), (0x4827b0, 0), (0x454ba0, 4), (0x482ad0, 4), (0x4d4f10, 8)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    cpu.mem_write(0x546f3c, struct.pack('<I', objects))
    cpu.mem_write(0x6882ac + 168 * 3, definition(30, 3, True))
    for slot, item_type in ((1, 16), (5, 5)):
        address, descriptor = items + slot * 256, items + 0x4000 + slot * 256
        cpu.mem_write(address + 0x30, struct.pack('<I', descriptor))
        cpu.mem_write(descriptor, definition(item_type, 1))
        cpu.mem_write(address + 0x46, b'\x03')  # conflicting gem resets item element to zero.
    offsets = (0x17c, 0x1220, 0x12ec, 0x1280, 0x1284, 0x1338, 0x133c, 0x1340, 0x1344, 0x1348,
               0x1350, 0x1358, 0x1360, 0x1428, 0x5778,
               *[0x13a4 + 44 * i + field for i in range(3) for field in (4, 8, 12, 16)])
    trace = []

    def word(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def hook(machine, address, size, context):
        if address == 0x4d4f10:
            sp = machine.reg_read(UC_X86_REG_ESP)
            ptr, length = struct.unpack('<II', cpu.mem_read(sp + 4, 8))
            data = bytes(cpu.mem_read(ptr, length))
            if length != 9 or data[:2] != b'\x02\x09':
                raise RuntimeError('Unexpected death cleanup request')
            identity, level = struct.unpack_from('<HB', data, 6)
            trace.append(f'D{identity},{level}')
        elif address == 0x4881c0:
            # Original freelist insertion is executed; inspect the returned instance.
            trace.append('F' + str(word(machine.reg_read(UC_X86_REG_EBP) + 8)))
        else:
            kind = {0x481990: 'M', 0x4827b0: 'V', 0x454ba0: 'R', 0x482ad0: 'U'}[address]
            trace.append(f'{kind}{cpu.mem_read(user + 0x1368, 1)[0]},{cpu.mem_read(user + 0x1354, 1)[0]},{word(user + 0x1358)}')

    for address in (0x4d4f10, 0x4881c0, 0x481990, 0x4827b0, 0x454ba0, 0x482ad0):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    rng = random.Random(0x488000)
    details = (0, 6, 61, 62, 63, 64, 65, 66, 67, 68, 70, 81, 82, 83, 85, 86, 87, 88, 89, 90, 91, 92, 94, 95)
    cases = []
    for detail in details:
        for penalty in (-1, 0, 5):
            for preserve in (0, 1, 2):
                cases.append((penalty, rng.randrange(4), [(detail, rng.choice((0, 3, 4)), rng.choice((199, 200, 201)), rng.choice((1, 100)), preserve, 100)]))
    for _ in range(512):
        cases.append((rng.choice((-1, 0, 1, 5)), rng.randrange(4), [
            (rng.choice(details), rng.choice((0, 3, 4)), rng.choice((199, 200, 201, 65535)), rng.choice((1, 2, 100)), rng.choice((0, 0, 1, 2, 255)), rng.choice((7, 100, 101, 102)))
            for _ in range(rng.randrange(9))]))
    print(f'# ps_game.exe SHA256 {digest}; ClearApplySkillByDeath 00488000; original SetSkillAbility and SetAttrAtk/Def execute')
    print('# penalty equipmentMask nodeCount; nodes(detail effect skill level preserve changeType); final DWORD fields ' + ' '.join(hex(value) for value in offsets) + '; WORD form, BYTE shape/noMob/blockDeath/blockDying/hpApplied, flags[9], retainedCount IDs, eventCount events')
    root = user + 0x5000
    for penalty, equipment, effects in cases:
        cpu.mem_write(user, bytes(0x6000));cpu.mem_write(objects, bytes(0x2000));cpu.mem_write(nodes, bytes(0x10000))
        for offset in offsets:
            cpu.mem_write(user + offset, struct.pack('<I', 3))
        for offset, value in ((0x17c, 10000), (0x1220, 100), (0x12ec, 2), (0x1280, 7), (0x1284, 8), (0x1358, 123), (0x578c, 9001), (0x145c, penalty & 0xffffffff)):
            cpu.mem_write(user + offset, struct.pack('<I', value))
        cpu.mem_write(user + 0x134c, struct.pack('<H', 987))
        cpu.mem_write(user + 0x1354, b'\x63');cpu.mem_write(user + 0x1364, b'\x01\x01\x01');cpu.mem_write(user + 0x1368, b'\x01')
        cpu.mem_write(user + 0x12cc, bytes([1] * 9))
        cpu.mem_write(root, struct.pack('<II', root, root));cpu.mem_write(user + 0xa94, struct.pack('<II', root, root))
        for slot, bit in ((1, 1), (5, 2)):
            if equipment & bit:
                cpu.mem_write(user + 0x1c4 + 4 * slot, struct.pack('<I', items + slot * 256))
        for index, (detail, effect, identity, level, preserve, change) in enumerate(effects):
            node, descriptor = nodes + index * 128, info + index * 160
            data = bytearray(160);data[0x31] = effect;data[0x97] = preserve
            struct.pack_into('<H', data, 0x32, detail);struct.pack_into('<H', data, 0x98, change)
            for ability, (kind, value) in enumerate(((1, 17), (4, 31), (16, 9))):
                struct.pack_into('<BxH', data, 0x7c + ability * 4, kind, value)
            cpu.mem_write(descriptor, bytes(data))
            last = word(root);cpu.mem_write(node, struct.pack('<5I', last, root, 1000 + index, identity, level))
            cpu.mem_write(node + 0x30, struct.pack('<I', descriptor));cpu.mem_write(last + 4, struct.pack('<I', node));cpu.mem_write(root, struct.pack('<I', node))
        cpu.mem_write(user + 0xab4, struct.pack('<I', len(effects)))
        trace.clear();cpu.mem_write(stack + 0x8000, struct.pack('<I', stop));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_EAX, user)
        cpu.emu_start(0x488000, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original death cleanup did not return')
        retained = [];cursor = word(root + 4)
        while cursor != root:
            retained.append(word(cursor + 8));cursor = word(cursor + 4)
        values = [struct.unpack('<i', cpu.mem_read(user + offset, 4))[0] for offset in offsets]
        print(penalty, equipment, len(effects), *(value for effect in effects for value in effect), *values,
              struct.unpack('<H', cpu.mem_read(user + 0x134c, 2))[0], cpu.mem_read(user + 0x1354, 1)[0], cpu.mem_read(user + 0x1366, 1)[0],
              cpu.mem_read(user + 0x1364, 1)[0], cpu.mem_read(user + 0x1365, 1)[0], cpu.mem_read(user + 0x1368, 1)[0],
              *cpu.mem_read(user + 0x12cc, 9), len(retained), *retained, len(trace), *trace, sep='\t')


if __name__ == '__main__':
    main()
