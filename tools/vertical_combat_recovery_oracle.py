#!/usr/bin/env python3
"""Trace the original periodic sitting/attack recovery and scheduler gates."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    user, stack, stop = 0x30000000, 0x20000000, 0x10000000
    for base in (user, stack, stop):
        cpu.mem_map(base, 0x10000)
    cpu.mem_write(0x482060, b'\xc2\x08\x00')
    seen = []
    def hook(machine, address, size, _):
        if address == 0x482060:
            sp = machine.reg_read(UC_X86_REG_ESP)
            sp_amount, mp_amount = struct.unpack('<2I', machine.mem_read(sp + 4, 8))
            seen.append((machine.reg_read(UC_X86_REG_EDI), machine.reg_read(UC_X86_REG_EAX), sp_amount, mp_amount))
    cpu.hook_add(UC_HOOK_CODE, hook)
    print(f'# ps_game.exe SHA256 {digest}; periodic recovery 0048cfb0; CUser::SendRecoverAddSet 00482060')
    print('# now next_tick attack_mode recovery_mode presentation hp_recovery sp_recovery mp_recovery calls final_next final_recovery')
    for now, next_tick, mode, recovery, motion in itertools.product((99, 100, 0xffffffff), (100, 0xfffffffe), (0, 1, 2, 3), (1, 2, 3), (0, 1, 2)):
        cpu.mem_write(user, bytes(0x2000))
        def put(offset, value): cpu.mem_write(user + offset, struct.pack('<I', value))
        for offset, value in ((0x1484, next_tick), (0x142c, mode), (0x1274, recovery), (0x1244, 11), (0x1248, 22), (0x124c, 33), (0x1268, 44), (0x126c, 55), (0x1270, 66)):
            put(offset, value)
        cpu.mem_write(user + 0x1434, bytes((motion,)))
        cpu.mem_write(stack + 0xf000, struct.pack('<I', stop))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0xf000)
        cpu.reg_write(UC_X86_REG_ECX, user)
        cpu.reg_write(UC_X86_REG_EAX, now)
        seen.clear()
        cpu.emu_start(0x48cfb0, stop, count=1000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original periodic recovery did not return')
        amounts = (44, 55, 66) if recovery == 1 else (11, 22, 33)
        if seen and seen != [(user, *amounts)]:
            raise RuntimeError(f'Unexpected recovery owner or arguments: {seen!r}')
        end = struct.unpack('<I', cpu.mem_read(user + 0x1484, 4))[0]
        end_mode = struct.unpack('<I', cpu.mem_read(user + 0x1274, 4))[0]
        print(now, next_tick, mode, recovery, motion, *amounts, len(seen), end, end_mode, sep='\t')

if __name__ == '__main__':
    main()
