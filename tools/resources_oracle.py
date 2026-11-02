#!/usr/bin/env python3
"""Execute original resource mutations and capture ordered network/damage operations. Requires Unicorn."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, amounts_ptr, party, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x10000000
    for address in (0, stack, user, amounts_ptr, party, stop):
        cpu.mem_map(address, 0x10000)
    for address, cleanup in ((0x4b8aa0, 0), (0x445680, 4), (0x421220, 32), (0x4d4d20, 8)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    trace = []

    def words(address, count):
        return struct.unpack('<' + 'I' * count, cpu.mem_read(address, 4 * count))

    def hook(machine, address, size, context):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x4b8aa0:
            trace.append('D')
        elif address == 0x445680:
            if words(sp + 4, 1)[0] != user:
                raise RuntimeError('Unexpected party exclusion')
            trace.append('P' + bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX))).hex())
        elif address == 0x421220:
            zone, ptr, length, position, radius, cid, unknown, channel = words(sp + 4, 8)
            if (zone, position, radius, cid, unknown, channel) != (party + 0x1000, user + 0xd0, 0x42700000, 9001, 0, 5):
                raise RuntimeError('Unexpected zone route')
            trace.append('V' + bytes(machine.mem_read(ptr, length)).hex())
        elif address == 0x4d4d20:
            ptr, length = words(sp + 4, 2)
            trace.append('S' + bytes(machine.mem_read(ptr, length)).hex())

    cpu.hook_add(UC_HOOK_CODE, hook)
    rng = random.Random(0x4505a0)
    boundaries = [((100, 200, 300), (1000, 2000, 3000), (1, 2, 3)),
                  ((100, 200, 300), (100, 200, 300), (1, 2, 3)),
                  ((99, 199, 299), (100, 200, 300), (100, 100, 100)),
                  ((65536, 70000, 100000), (65536, 70000, 100000), (0, 0, 0)),
                  ((100, 200, 300), (50, 60, 70), (0, 0, 0)),
                  ((0, 0, 0), (100, 200, 300), (-1, -2, -3)),
                  ((0x7fffffff, 0xffffffff, 0x80000000), (0x7fffffff, 0xffffffff, 0), (1, 1, -1)),
                  ((10, 20, 30), (100, 200, 300), (0, 0, 0))]
    print(f'# ps_game.exe SHA256 {digest}; Heal 004505a0, SendRecoverAdd 00481f20, SendRecoverAddSet 00482060, SendMaxHP/SP/MP 00481990/00481aa0/00481ba0, SendEnergyAllMax 004822f0/004823d0')
    print('# operation zone party(-1/0/1) initializing attack_mode recovering; current HP/MP/SP; maximum HP/MP/SP; amounts HP/SP/MP; final HP/MP/SP; actual amounts HP/SP/MP; event_count; ordered D(reset)/V(view)/P(party)/S(self) + hex payload')
    entries = (0x4505a0, 0x481f20, 0x482060, 0x481990, 0x481ba0, 0x481aa0, 0x4822f0)
    for op, entry in enumerate(entries):
        cases = [(cur, maximum, amounts, zone, group, 0, 0, 0)
                 for cur, maximum, amounts in boundaries for zone in (0, 1) for group in (-1, 0, 1)]
        for _ in range(120):
            cur = tuple(rng.choice((rng.randrange(200000), rng.randrange(0x100000000))) for _ in range(3))
            maximum = tuple(rng.choice((rng.randrange(200000), rng.randrange(0x100000000))) for _ in range(3))
            amounts = tuple(rng.randrange(-100000, 100001) for _ in range(3))
            cases.append((cur, maximum, amounts, rng.randrange(2), rng.randrange(-1, 2), rng.randrange(2), rng.randrange(3), rng.randrange(2)))
        for cur, maximum, amounts, zone, group, initializing, attack_mode, recovering in cases:
            trace.clear()
            cpu.mem_write(user, bytes(0x6000))
            cpu.mem_write(user + 0x1238, struct.pack('<3I', *cur))
            cpu.mem_write(user + 0x17c, struct.pack('<3I', *maximum))
            cpu.mem_write(user + 0xdc, struct.pack('<II', 9001, party + 0x1000 if zone else 0))
            cpu.mem_write(user + 0x173c, struct.pack('<I', party if group >= 0 else 0))
            cpu.mem_write(party + 0x114, bytes([max(0, group)]))
            cpu.mem_write(user + 0x57b5, bytes([initializing]))
            cpu.mem_write(user + 0x142c, struct.pack('<I', attack_mode))
            cpu.mem_write(user + 0x1434, bytes([recovering]))
            cpu.mem_write(amounts_ptr, struct.pack('<3i', *amounts))
            cpu.mem_write(stack + 0x8000, struct.pack('<III', stop, amounts[1] & 0xffffffff, amounts[2] & 0xffffffff))
            cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
            cpu.reg_write(UC_X86_REG_EAX, user if op == 6 else amounts_ptr if op == 0 else amounts[0] & 0xffffffff)
            for reg in (UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI):
                cpu.reg_write(reg, user)
            cpu.emu_start(entry, stop, count=10000)
            if cpu.reg_read(UC_X86_REG_EIP) != stop:
                raise RuntimeError('Original resource function did not return')
            final = words(user + 0x1238, 3)
            actual = struct.unpack('<3i', cpu.mem_read(amounts_ptr, 12))
            print(op, zone, group, initializing, attack_mode, recovering, *cur, *maximum, *amounts, *final, *actual, len(trace), *trace, sep='\t')


if __name__ == '__main__':
    main()
