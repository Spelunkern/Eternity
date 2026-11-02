#!/usr/bin/env python3
"""Execute the original final periodic-heal calculation with its CRT precision setting. Requires Unicorn."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    machine, digest = load_game()
    stack, user, info, source, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x10000000
    for address in (0, stack, user, info, source, stop):
        machine.mem_map(address, 0x10000)
    machine.reg_write(UC_X86_REG_FPCW, 0x37f)
    machine.mem_write(stack + 0x8000, struct.pack('<I', stop))
    machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
    machine.emu_start(0x51136e, stop, count=10000)
    if machine.reg_read(UC_X86_REG_FPCW) & 0xf00 != 0x200:
        raise RuntimeError('Original CRT precision setting changed')
    machine.mem_write(stop + 0x100, b'\xc3')
    machine.mem_write(0x5210e4, struct.pack('<I', stop + 0x100))
    machine.mem_write(0x4505a0, b'\xc3')
    machine.mem_write(0x4bd8d0, b'\xc2\x0c\x00')
    result = []

    def hook(cpu, address, size, context):
        if address == stop + 0x100:
            cpu.reg_write(UC_X86_REG_EAX, 10000)
        elif address == 0x4505a0:
            result.extend(struct.unpack('<3i', cpu.mem_read(cpu.reg_read(UC_X86_REG_EAX), 12)))

    machine.hook_add(UC_HOOK_CODE, hook)
    rng = random.Random(0x485130)
    cases = [(age, rates) for age in (0, 1, 10, 1001, 2999, 3000, 3001, 10000, 0x7fffffff, 0xffffffff)
             for rates in ((0, 0, 0), (3, 7, 65535), (300, 3000, 30000), (65535, 65535, 65535))]
    cases += [(rng.randrange(0x100000000), tuple(rng.randrange(65536) for _ in range(3))) for _ in range(160)]
    print(f'# ps_game.exe SHA256 {digest}; RemApplySkillBuff 00485130; _setdefaultprecision 0051136e selects 53-bit precision')
    print('# elapsed_ms HP_rate SP_rate MP_rate; HP SP MP amounts before the world Heal/broadcast operations')
    for age, rates in cases:
        result.clear()
        definition = bytearray(160)
        struct.pack_into('<H', definition, 0x32, 81)
        struct.pack_into('<3H', definition, 0x8e, *rates)
        machine.mem_write(info, bytes(definition))
        machine.mem_write(source + 0x24, struct.pack('<I', (10000 - age) & 0xffffffff))
        machine.mem_write(stack + 0x8000, struct.pack('<I', stop))
        machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        machine.reg_write(UC_X86_REG_ECX, user)
        machine.reg_write(UC_X86_REG_EDX, info)
        machine.reg_write(UC_X86_REG_EBX, source)
        machine.emu_start(0x485130, stop, count=10000)
        if machine.reg_read(UC_X86_REG_EIP) != stop or len(result) != 3:
            raise RuntimeError('Original periodic heal calculation did not return')
        print(age, *rates, *result, sep='\t')


if __name__ == '__main__':
    main()
