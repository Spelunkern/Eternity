#!/usr/bin/env python3
"""Execute original RemApplySkillHide, including linked-list removal and real shape routing. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, info, nodes, party, zone, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x10000000
    for address in (0, stack, user, info, nodes, party, zone, stop):
        cpu.mem_map(address, 0x10000)
    for address, cleanup in ((0x4d4f10, 8), (0x4d4d20, 8), (0x445680, 4), (0x40a7c0, 0), (0x4208c0, 12)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    root = user + 0x8000
    context = {}

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address in (0x4d4f10, 0x4d4d20):
            ptr, length = struct.unpack('<2I', machine.mem_read(sp + 4, 8))
            packet = bytes(machine.mem_read(ptr, length))
            context['events'].append(('B' if address == 0x4d4f10 else 'S') + packet.hex())
        elif address == 0x4208c0:
            actual_zone, ptr, length = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
            if (actual_zone, machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX)) != (zone, 17, 23):
                raise RuntimeError('Unexpected hide-removal cell route')
            context['events'].append('C' + bytes(machine.mem_read(ptr, length)).hex())
        elif address == 0x40a7c0:
            node = machine.reg_read(UC_X86_REG_EDI)
            context['events'].append('F' + str(number(node + 8)))
            if context['scenario'] & 16:
                machine.mem_write(user + 0x173c, bytes(4))
            elif context['scenario'] & 32:
                machine.mem_write(party + 0x114, b'\x01')
        elif address == 0x445680:
            packet = bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX)))
            context['events'].append('P' + packet.hex())

    cpu.hook_add(UC_HOOK_CODE, hook)
    variants = ([], [(82, 4)], [(82, 4), (88, 4), (88, 3), (82, 4)], [(88, 0), (82, 4), (88, 4)])
    print(f'# ps_game.exe SHA256 {digest}; RemApplySkillHide 00489290, SendShape 00482590, SendSkillRemove 00482a70; Ghidra work/actor-hide-removal.')
    print('# scenario final_shape remaining_count remaining_IDs event_count ordered events (B DB-agent, S self, C cell, F free, P group)')
    for s in range(256):
        context.update(scenario=s, events=[])
        cpu.mem_write(user, bytes(0x9000));cpu.mem_write(party, bytes(0x200))
        cpu.mem_write(root, struct.pack('<2I', root, root));cpu.mem_write(user + 0xa94, struct.pack('<I', root))
        cpu.mem_write(user + 0xdc, struct.pack('<I', 9001));cpu.mem_write(user + 0x578c, struct.pack('<I', 9001))
        cpu.mem_write(user + 0xe0, struct.pack('<3I', zone if s & 2 else 0, 17, 23))
        cpu.mem_write(user + 0x5774, b'\x01')
        cpu.mem_write(user + 0x1354, bytes([100 + s % 3 if s & 1 else 0]))
        cpu.mem_write(user + 0x1358, struct.pack('<I', 321));cpu.mem_write(user + 0x134c, struct.pack('<H', 42))
        cpu.mem_write(user + 0x173c, struct.pack('<I', party if s & 4 else 0));cpu.mem_write(party + 0x114, bytes([int(bool(s & 8))]))
        for i, (detail, effect) in enumerate(variants[s >> 6]):
            node, definition = nodes + i * 0x80, info + i * 160
            cpu.mem_write(definition, bytes(160));cpu.mem_write(definition + 0x31, struct.pack('<BH', effect, detail))
            previous = number(root)
            cpu.mem_write(node, struct.pack('<5I', previous, root, 1000 + i, 200 + i, 1))
            cpu.mem_write(node + 0x30, struct.pack('<I', definition))
            cpu.mem_write(previous + 4, struct.pack('<I', node));cpu.mem_write(root, struct.pack('<I', node))
            cpu.mem_write(user + 0xab4, struct.pack('<I', number(user + 0xab4) + 1))
        cpu.mem_write(stack + 0x8000, struct.pack('<I', stop))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_EBX, user)
        cpu.emu_start(0x489290, stop, count=10000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original hide removal did not return')
        if (number(user + 0x1358), struct.unpack('<H', cpu.mem_read(user + 0x134c, 2))[0]) != (321, 42):
            raise RuntimeError('Unexpected transformation-field change')
        remaining, node = [], number(root + 4)
        while node != root:
            if len(remaining) > 16:
                raise RuntimeError('Original hide list did not terminate')
            remaining.append(number(node + 8));node = number(node + 4)
        print(s, cpu.mem_read(user + 0x1354, 1)[0], len(remaining), *remaining, len(context['events']), *context['events'], sep='\t')


if __name__ == '__main__':
    main()
