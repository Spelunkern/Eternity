#!/usr/bin/env python3
"""Trace original standing recovery amount calculations without transport."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from original_oracle import load_game


def signed(value):
    return struct.unpack('<i', struct.pack('<I', value))[0]


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
            if machine.reg_read(UC_X86_REG_EDI) != user:
                raise RuntimeError('Foreign recovery owner')
            seen.append(tuple(map(signed, (machine.reg_read(UC_X86_REG_EAX), sp_amount, mp_amount))))
    cpu.hook_add(UC_HOOK_CODE, hook)
    print(f'# ps_game.exe SHA256 {digest}; periodic recovery 0048cfb0, standing branch 0048d014; SendRecoverAddSet 00482060')
    print('# maximum HP SP MP, percentage HP SP MP, standing HP SP MP, result HP SP MP')
    rng = random.Random(0x48d014)
    for case in range(1024):
        if case < 512:
            maximum = [rng.randrange(0, 200000) for _ in range(3)]
            rates = [rng.randrange(-100, 501) for _ in range(3)]
            flat = [rng.randrange(-2000, 2001) for _ in range(3)]
        else:
            maximum = [rng.randrange(-2**31, 2**31) for _ in range(3)]
            rates = [rng.randrange(-2**31, 2**31) for _ in range(3)]
            flat = [rng.randrange(-2**31, 2**31) for _ in range(3)]
        cpu.mem_write(user, bytes(0x2000))
        for offset, value in zip((0x17c,0x184,0x180,0x125c,0x1260,0x1264,0x1250,0x1254,0x1258), (*maximum,*rates,*flat)):
            cpu.mem_write(user + offset, struct.pack('<i', value))
        cpu.mem_write(user + 0x142c, struct.pack('<I', 2))
        cpu.mem_write(stack + 0xf000, struct.pack('<I', stop))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0xf000)
        cpu.reg_write(UC_X86_REG_ECX, user)
        cpu.reg_write(UC_X86_REG_EAX, 100)
        cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
        seen.clear()
        cpu.emu_start(0x48cfb0, stop, count=1000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop or len(seen) != 1:
            raise RuntimeError('Original standing recovery did not reach the callback')
        print(*maximum, *rates, *flat, *seen[0], sep='\t')

if __name__ == '__main__':
    main()
