#!/usr/bin/env python3
"""Trace the original live blessing replacement of recovery contributions."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    user, definition, stack = 0x30000000, 0x31000000, 0x20000000
    for base in (user, definition, stack):
        cpu.mem_map(base, 0x10000)
    print(f'# ps_game.exe SHA256 {digest}; live blessing recovery replacement 0048ba63..0048bb50')
    print('# maximum HP SP MP; old recovery HP SP MP; previous contributions; new rates; resulting recovery; new contributions')
    random_source = random.Random(0x48ba63)
    for case in range(1024):
        if case < 512:
            maximum = [random_source.randrange(0, 200000) for _ in range(3)]
            recovery = [random_source.randrange(-10000, 10001) for _ in range(3)]
            old = [random_source.randrange(-10000, 10001) for _ in range(3)]
            rates = [random_source.randrange(-500, 501) for _ in range(3)]
        else:
            maximum, recovery, old, rates = ([random_source.randrange(-2**31, 2**31) for _ in range(3)] for _ in range(4))
        cpu.mem_write(user, bytes(0x2000))
        cpu.mem_write(definition, bytes(0x1000))
        for offset, value in zip((0x17c,0x184,0x180,0x1244,0x1248,0x124c,0x14ec,0x14f0,0x14f4), (*maximum,*recovery,*old)):
            cpu.mem_write(user + offset, struct.pack('<i', value))
        for offset, value in zip((0x40,0x44,0x48), rates):
            cpu.mem_write(definition + offset, struct.pack('<i', value))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0xf000)
        cpu.reg_write(UC_X86_REG_ESI, user)
        cpu.reg_write(UC_X86_REG_EDI, definition)
        cpu.reg_write(UC_X86_REG_EBX, 0)
        cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
        cpu.emu_start(0x48ba63, 0x48bb50, count=1000)
        if cpu.reg_read(UC_X86_REG_EIP) != 0x48bb50:
            raise RuntimeError('Original blessing replacement did not reach continuation')
        result = [struct.unpack('<i', cpu.mem_read(user + offset, 4))[0] for offset in (0x1244,0x1248,0x124c,0x14ec,0x14f0,0x14f4)]
        print(*maximum, *recovery, *old, *rates, *result, sep='\t')

if __name__ == '__main__':
    main()
