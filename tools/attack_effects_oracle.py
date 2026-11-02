#!/usr/bin/env python3
"""Run original attack-triggered shape/sleep removal and ordered notices."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, info, nodes, party, zone, objects, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x35000000, 0x10000000
    for address in (0, stack, user, info, nodes, party, zone, objects, stop):
        cpu.mem_map(address, 0x10000)
    for address, cleanup in ((0x4d4f10, 8), (0x4d4d20, 8), (0x445680, 4), (0x4208c0, 12), (0x454ba0, 4), (0x482ad0, 4), (0x4827b0, 0)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    cpu.mem_write(0x546f3c, struct.pack('<I', objects))
    trace, state = [], {}
    root = user + 0x8000

    def word(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def signed(address):
        return struct.unpack('<i', cpu.mem_read(address, 4))[0]

    def mutate_group():
        scenario = state['scenario']
        if scenario & 8:
            cpu.mem_write(user + 0x173c, bytes(4))
        elif scenario & 16:
            cpu.mem_write(party + 0x114, b'\x01')
        elif scenario & 32:
            cpu.mem_write(user + 0x173c, struct.pack('<I', party));cpu.mem_write(party + 0x114, b'\x00')

    def hook(machine, address, size, context):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address in (0x4d4f10, 0x4d4d20):
            ptr, length = struct.unpack('<II', cpu.mem_read(sp + 4, 8));trace.append(('B' if address == 0x4d4f10 else 'S') + bytes(cpu.mem_read(ptr, length)).hex())
        elif address == 0x4208c0:
            actual_zone, ptr, length = struct.unpack('<III', cpu.mem_read(sp + 4, 12))
            if (actual_zone, machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX)) != (zone, 17, 23):
                raise RuntimeError('Unexpected shape-removal route')
            trace.append('C' + bytes(cpu.mem_read(ptr, length)).hex())
        elif address in (0x40a7c0, 0x4894b7):
            node = machine.reg_read(UC_X86_REG_EDI if address == 0x40a7c0 else UC_X86_REG_ESI)
            trace.append('F' + str(word(node + 8)));mutate_group()
        elif address == 0x445680:
            trace.append('P' + bytes(cpu.mem_read(machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX))).hex())
        else:
            kind = {0x454ba0: 'R', 0x482ad0: 'U', 0x4827b0: 'V'}[address]
            trace.append(f'{kind}{cpu.mem_read(user + 0x1354, 1)[0]},{signed(user + 0x1348)},{signed(user + 0x1344)},{word(user + 0x1358)}')
            if address == 0x4827b0 and state['scenario'] & 64:
                cpu.mem_write(user + 0x173c, struct.pack('<I', 0 if word(user + 0x173c) else party))

    for address in (0x4d4f10, 0x4d4d20, 0x445680, 0x4208c0, 0x40a7c0, 0x4894b7, 0x454ba0, 0x482ad0, 0x4827b0):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    # ID, level, detail, effect, change type. The protected 127 stops the search.
    variants = ([], [(200, 1, 82, 4, 0)], [(127, 1, 90, 4, 100), (201, 1, 88, 4, 0), (202, 1, 66, 3, 0)],
                [(200, 1, 88, 0, 0), (201, 1, 90, 4, 100)], [(200, 1, 90, 3, 100), (201, 1, 66, 3, 0)],
                [(127, 1, 90, 4, 101), (201, 1, 66, 0, 0)], [(127, 100, 90, 3, 102), (201, 1, 66, 4, 0)],
                [(200 + i, 1, 66, i % 5, 0) for i in range(70)],
                [(200, 1, 66, 3, 0), (201, 1, 62, 3, 0), (202, 1, 66, 4, 0), (203, 1, 88, 4, 0), (204, 1, 90, 4, 101)])
    print(f'# ps_game.exe SHA256 {digest}; RemApplySkillToMOB 00489560, RemApplySkillSleep 004893a0; original linked lists, frees, shape/group/removal packets')
    print('# op scenario shape fixed sleep nodeCount (id level detail effect change)*; final shape fixed sleep template form copiedPointer; remainingCount instanceIDs; eventCount events')
    for op in range(2):
        for variant in variants:
            for scenario in range(128):
                shape = (0, 13, 100, 101, 102, 255)[scenario % 6]
                fixed = (-1, 0, 1, -0x80000000)[scenario % 4]
                sleep = (-1, 0, 3, -0x80000000)[scenario // 4 % 4]
                state['scenario'] = scenario
                cpu.mem_write(user, bytes(0x9000));cpu.mem_write(objects, bytes(0x1000));cpu.mem_write(party, bytes(0x200));cpu.mem_write(nodes, bytes(0x10000))
                cpu.mem_write(user + 0xdc, struct.pack('<I', 9001));cpu.mem_write(user + 0x578c, struct.pack('<I', 9001))
                cpu.mem_write(user + 0xe0, struct.pack('<III', zone if scenario & 1 else 0, 17, 23));cpu.mem_write(user + 0x5774, b'\x01')
                cpu.mem_write(user + 0x173c, struct.pack('<I', party if scenario & 2 else 0));cpu.mem_write(party + 0x114, bytes([int(bool(scenario & 4))]))
                cpu.mem_write(user + 0x1354, bytes([shape]));cpu.mem_write(user + 0x1344, struct.pack('<ii', sleep, fixed))
                cpu.mem_write(user + 0x1358, struct.pack('<II', 321, 0x778899aa));cpu.mem_write(user + 0x134c, struct.pack('<H', 42))
                cpu.mem_write(root, struct.pack('<II', root, root));cpu.mem_write(user + 0xa94, struct.pack('<II', root, root));cpu.mem_write(user + 0xab4, struct.pack('<I', len(variant)))
                for index, (identity, level, detail, effect, change) in enumerate(variant):
                    node, descriptor = nodes + index * 128, info + index * 160
                    data = bytearray(160);struct.pack_into('<HB', data, 0, identity, level);struct.pack_into('<BH', data, 0x31, effect, detail);struct.pack_into('<H', data, 0x98, change)
                    cpu.mem_write(descriptor, bytes(data));last = word(root)
                    cpu.mem_write(node, struct.pack('<5I', last, root, 1000 + index, identity, level));cpu.mem_write(node + 0x30, struct.pack('<I', descriptor))
                    cpu.mem_write(last + 4, struct.pack('<I', node));cpu.mem_write(root, struct.pack('<I', node))
                trace.clear();cpu.mem_write(stack + 0x8000, struct.pack('<II', stop, user));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_EAX, user)
                cpu.emu_start(0x489560 if op == 0 else 0x4893a0, stop, count=100000)
                if cpu.reg_read(UC_X86_REG_EIP) != stop:
                    raise RuntimeError('Original attack effect removal did not return')
                remaining, cursor = [], word(root + 4)
                while cursor != root:
                    remaining.append(word(cursor + 8));cursor = word(cursor + 4)
                print(op, scenario, shape, fixed, sleep, len(variant), *(value for node in variant for value in node), cpu.mem_read(user + 0x1354, 1)[0],
                      signed(user + 0x1348), signed(user + 0x1344), word(user + 0x1358), struct.unpack('<H', cpu.mem_read(user + 0x134c, 2))[0], word(user + 0x135c),
                      len(remaining), *remaining, len(trace), *trace, sep='\t')


if __name__ == '__main__':
    main()
