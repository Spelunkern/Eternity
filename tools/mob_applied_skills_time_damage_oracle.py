#!/usr/bin/env python3
"""Execute complete original CMob::TimeDamage and original CRT conversion."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x10000)
    mob, definition, info, output, stack, stop = [base+x for x in (0, 0x2000, 0x3000, 0x4000, 0xe000, 0xf000)]

    def put(at, *values):
        cpu.mem_write(at, struct.pack('<'+'I'*len(values), *[x & 0xffffffff for x in values]))

    # Execute the original CRT setup, rather than assume an emulator default.
    put(stack, stop)
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.reg_write(UC_X86_REG_FPCW, 0x37f)
    cpu.emu_start(0x51136e, stop, count=10000)
    assert cpu.reg_read(UC_X86_REG_EIP) == stop
    control = cpu.reg_read(UC_X86_REG_FPCW)
    assert control & 0xf00 == 0x200  # 53-bit precision, nearest rounding.

    print(f'# ps_game.exe SHA256 {digest}; complete CMob::TimeDamage 004a3be0, actual _ftol2 0051c424; no ports or executable patches. actual _setdefaultprecision 0051136e returned CW {control:04x}; verifies control restoration, read-only inputs and output guards.')
    print('# type damageHP/SP/MP currentHP/SP/MP maximumHP/SP/MP elapsed_ms expectedHP/SP/MP (DWORD bits)')
    words = [0, 1, 2, 3, 33, 99, 100, 101, 255, 32767, 32768, 65534, 65535]
    dwords = [0, 1, 2, 3, 99, 100, 101, 30000, 65535, 16777215, 16777216, 16777217,
              0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff]
    ticks = [0, 1, 2, 999, 1000, 1499, 1500, 2999, 3000, 3001, 4500,
             90000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff]
    rng = random.Random(0x4a3be0)
    cases = []
    # Full type coverage, then systematic supported-type boundary combinations.
    for kind in range(256):
        cases.append((kind, [65535, 33, 1], [0xffffffff, 65535, 32768], [16777217, 1, 65535], 3000))
    for kind in (0, 1, 2, 3, 4, 5, 6, 255):
        for i in range(1024):
            amounts = [words[(i//(13**slot)) % 13] for slot in range(3)]
            current = [dwords[i % 16], words[(i//16) % 13], words[(i//208) % 13]]
            maximum = [dwords[(i//13) % 16], words[(i//3) % 13], words[(i//7) % 13]]
            cases.append((kind, amounts, current, maximum, ticks[(i//11) % 16]))
    for _ in range(4096):
        cases.append((rng.choice((0, 3, 4, 5)), [rng.randrange(65536) for _ in range(3)],
                      [rng.randrange(2**32), rng.randrange(65536), rng.randrange(65536)],
                      [rng.randrange(2**32), rng.randrange(65536), rng.randrange(65536)], rng.randrange(2**32)))
    for kind, amounts, current, maximum, elapsed in cases:
        cpu.mem_write(base, bytes(0xe000))
        put(mob+0xa8, definition)
        put(mob+0x224, current[0])
        cpu.mem_write(mob+0x228, struct.pack('<2H', *current[1:]))
        put(definition+0x30, maximum[0])
        cpu.mem_write(definition+0x34, struct.pack('<2H', *maximum[1:]))
        cpu.mem_write(info+0x6e, bytes([kind]))
        cpu.mem_write(info+0x70, struct.pack('<3H', *amounts))
        before = bytes(cpu.mem_read(mob, 0x3000))+bytes(cpu.mem_read(info, 0x100))
        cpu.mem_write(output-4, b'\xa5'*20)
        put(stack, stop, elapsed)
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.reg_write(UC_X86_REG_EAX, info)
        cpu.reg_write(UC_X86_REG_EDI, mob)
        cpu.reg_write(UC_X86_REG_ESI, output)
        cpu.reg_write(UC_X86_REG_FPCW, control)
        cpu.emu_start(0x4a3be0, stop, count=10000)
        assert cpu.reg_read(UC_X86_REG_EIP) == stop
        assert cpu.reg_read(UC_X86_REG_FPCW) == control
        assert bytes(cpu.mem_read(output-4, 4))+bytes(cpu.mem_read(output+12, 4)) == b'\xa5'*8
        assert before == bytes(cpu.mem_read(mob, 0x3000))+bytes(cpu.mem_read(info, 0x100))
        expected = struct.unpack('<3I', cpu.mem_read(output, 12))
        print(kind, *amounts, *current, *maximum, elapsed, *expected, sep='\t')


if __name__ == '__main__':
    main()
