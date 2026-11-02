#!/usr/bin/env python3
"""Execute Ghidra-recovered scalar HP damage and CDamage list operations."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                              UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP)
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, party, stop, heap = 0x20000000, 0x30000000, 0x31000000, 0x10000000, 0x32000000
    for address in (stack, user, party, stop, heap):
        cpu.mem_map(address, 0x10000)
    ledger, output = user + 0x1538, user + 0x7000
    trace = []
    allocated = 0

    def words(address, count=1):
        return struct.unpack('<' + 'I' * count, cpu.mem_read(address, 4 * count))

    def invoke(entry, args=(), regs=()):
        sp = stack + 0x8000
        cpu.mem_write(sp, struct.pack('<' + 'I' * (len(args) + 1), stop, *(v & 0xffffffff for v in args)))
        cpu.reg_write(UC_X86_REG_ESP, sp)
        for reg, value in regs:
            cpu.reg_write(reg, value & 0xffffffff)
        cpu.emu_start(entry, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original function did not return')
        return cpu.reg_read(UC_X86_REG_EAX) & 0xff

    # Win32 critical sections and external equipment/passive/network ports only.
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    cpu.mem_write(stop + 0x110, b'\xc2\x08\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    cpu.mem_write(0x5210c8, struct.pack('<I', stop + 0x110))
    for address, cleanup in ((0x503a3f, 0), (0x45b290, 0), (0x487020, 4), (0x4d4d20, 8), (0x445680, 4)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))

    def hook(machine, address, size, context):
        nonlocal allocated
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503a3f:
            if words(sp + 4)[0] != 20 or allocated >= 0xff00:
                raise RuntimeError('Unexpected damage allocation')
            machine.reg_write(UC_X86_REG_EAX, heap + allocated)
            allocated += 20
        elif address == 0x45b290:
            trace.append('W')
        elif address == 0x487020:
            trace.append('H')
        elif address == 0x4b8b60:
            kind, amount = words(sp + 4, 2)
            trace.append(f'R{kind},{machine.reg_read(UC_X86_REG_EBX)},{amount}')
        elif address == 0x4d4d20:
            ptr, length = words(sp + 4, 2)
            trace.append('S' + bytes(machine.mem_read(ptr, length)).hex())
        elif address == 0x445680:
            if words(sp + 4)[0] != user:
                raise RuntimeError('Unexpected group exclusion')
            trace.append('P' + bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX))).hex())

    for address in (0x503a3f, 0x45b290, 0x487020, 0x4b8b60, 0x4d4d20, 0x445680):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    invoke(0x4b8a50, regs=((UC_X86_REG_ESI, ledger),))
    rng = random.Random(0x44c120)
    print(f'# ps_game.exe SHA256 {digest}; Damage 0044c120, HP party 004824b0; CDamage ctor/Reset/Add/GetMax 004b8a50/004b8aa0/004b8b60/004b8c40')
    print('# D: HP MP SP maximumHP mode protection kind source amount party -> result HP MP SP event_count events; L: reset kind source amount -> entry_count (kind source amount)* maxPresent (kind source amount)?')
    cases = []
    for hp, maximum, amount in ((1, 100, 1), (100, 100, 100), (49, 100, 49), (50, 100, 50), (0, 100, 1),
                                (100, 100, 0), (100, 100, -1), (0x80000000, 100, 0),
                                (1, 0xffffffff, -0x80000000), (0x7fffffff, 0xffffffff, 0x7fffffff)):
        for mode in (0, 1, 2):
            for protection in (0, 0x12345678):
                for kind in (1, 3):
                    for group in (-1, 0, 1):
                        cases.append((hp, 0x11223344, 0xaabbccdd, maximum, mode, protection, kind, 12345, amount, group))
    for _ in range(640):
        cases.append((rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32), rng.getrandbits(32),
                      rng.randrange(3), rng.choice((0, rng.getrandbits(32))), rng.choice((1, 3, -1, 259)),
                      rng.getrandbits(32), rng.randrange(-0x80000000, 0x80000000), rng.randrange(-1, 2)))
    for hp, mp, sp, maximum, mode, protection, kind, source, amount, group in cases:
        invoke(0x4b8aa0, regs=((UC_X86_REG_ESI, ledger),))
        cpu.mem_write(user + 0x1238, struct.pack('<3I', hp, mp, sp))
        for offset, value in ((0x17c, maximum), (0x142c, mode), (0x12fc, protection), (0xdc, 9001), (0x173c, party if group >= 0 else 0)):
            cpu.mem_write(user + offset, struct.pack('<I', value))
        cpu.mem_write(party + 0x114, bytes([max(0, group)]))
        trace.clear()
        result = invoke(0x44c120, (amount,), ((UC_X86_REG_EAX, kind), (UC_X86_REG_EBX, source), (UC_X86_REG_ECX, user)))
        print('D', hp, mp, sp, maximum, mode, protection, kind, source, amount, group, result,
              *words(user + 0x1238, 3), len(trace), *trace, sep='\t')
    invoke(0x4b8aa0, regs=((UC_X86_REG_ESI, ledger),))
    for step in range(2048):
        reset = step % 64 == 0 or rng.randrange(20) == 0
        if reset:
            invoke(0x4b8aa0, regs=((UC_X86_REG_ESI, ledger),))
        kind, source = rng.choice((0, 1, 2, 3, 255, 256, 259, -1)), rng.randrange(4)
        amount = rng.choice((0, 1, -1, 0x7fffffff, -0x80000000, rng.randrange(-1000, 1000)))
        invoke(0x4b8b60, (kind, amount), ((UC_X86_REG_ESI, ledger), (UC_X86_REG_EBX, source)))
        entries = []
        sentinel = words(ledger)[0]
        cursor = words(sentinel + 4)[0]
        while cursor != sentinel:
            entries.extend((cpu.mem_read(cursor + 8, 1)[0], words(cursor + 12)[0], struct.unpack('<i', cpu.mem_read(cursor + 16, 4))[0]))
            cursor = words(cursor + 4)[0]
        present = invoke(0x4b8c40, regs=((UC_X86_REG_ESI, ledger), (UC_X86_REG_EBX, output)))
        best = (cpu.mem_read(output + 8, 1)[0], words(output + 12)[0], struct.unpack('<i', cpu.mem_read(output + 16, 4))[0]) if present else ()
        print('L', int(reset), kind, source, amount, len(entries) // 3, *entries, present, *best, sep='\t')
    invoke(0x4b8aa0, regs=((UC_X86_REG_ESI, ledger),))
    present = invoke(0x4b8c40, regs=((UC_X86_REG_ESI, ledger), (UC_X86_REG_EBX, output)))
    print('E', present, sep='\t')


if __name__ == '__main__':
    main()
