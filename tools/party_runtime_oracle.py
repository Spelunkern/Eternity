#!/usr/bin/env python3
"""Execute original party state, level factor and range (including real Distance2)."""
import random
import struct
from original_oracle import load_game
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_FPCW


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x100000)
    cpu.mem_map(0, 0x10000)
    # Windows CRT TLS port; real sqrt and error paths remain intact.
    cpu.mem_write(0x50d0ca, b"\xb8" + struct.pack("<I", base + 0xe0000) + b"\xc3")
    party, candidate, reference = base, base + 0x10000, base + 0x20000
    stack, stop, output = base + 0xf0000, base + 0xff000, base + 0xfe000
    def put(at, value):
        cpu.mem_write(at, struct.pack('<I', value & 0xffffffff))
    def call(at, arg=0, floating=False):
        put(stack, stop)
        put(stack + 4, arg)
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
        cpu.reg_write(UC_X86_REG_EDI, party)
        cpu.reg_write(UC_X86_REG_EAX, party if at == 0x444e40 else reference)
        cpu.reg_write(UC_X86_REG_ECX, candidate)
        # Store original x87 return without converting it in Python.
        cpu.mem_write(stop, b'\xdd\x1d' + struct.pack('<I', output))
        cpu.emu_start(at, stop if not floating else stop + 6, count=100000)
        return struct.unpack('<d', cpu.mem_read(output, 8))[0] if floating else cpu.reg_read(UC_X86_REG_EAX) & 255
    print(f'# ps_game.exe SHA256 {digest}; ChkPartyExpByLevel 00444d80; GetPartyExpByLevel 00444e40; EnableApplyRange 0045de60; Distance2 0041b1f0')
    rng = random.Random(45)
    for case in range(400):
        kind = case % 4
        count = case % 32
        old = (0, 1, 60, 0x80000000, 0xffffffff)[case % 5]
        levels = [None if kind == 1 and rng.randrange(3) == 0 else rng.randrange(65536) for _ in range(count)]
        put(party + 0x10, count)
        put(party + 0x10c, old)
        cpu.mem_write(party + 0x114, bytes([kind]))
        for i, level in enumerate(levels):
            user = base + 0x30000 + i * 0x1000
            put(party + 0x18 + i * 8, 0 if level is None else user)
            cpu.mem_write(user + 0x136, struct.pack('<H', level or 0))
        call(0x444d80)
        result = struct.unpack('<I', cpu.mem_read(party + 0x10c, 4))[0]
        print('S', kind, old, count, result, *[-1 if x is None else x for x in levels])
    for maximum in (0, 1, 60, 65535, 0x7fffffff, 0x80000000, 0xffffffff):
        for difference in (-2147483648, -1, 0, 7, 8, 15, 16, 31, 32, 47, 48, 2147483647):
            level = (maximum - difference) & 0xffffffff
            put(party + 0x10c, maximum)
            print('F', maximum, level, call(0x444e40, level, True))
    for case in range(1024):
        life = (0, 1, 2, -1)[case % 4]
        state = (4, 0, 3, 5)[(case // 4) % 4]
        zone = (7, 8)[(case // 16) % 2]
        instance = (2, 3)[(case // 32) % 2]
        x, y, z = [(0., 1000., 0.), (3., 999., 4.), (6., 0., 8.), (float('nan'), 0., 0.), (float('inf'), 0., 0.), (0.1, 0., 0.1)][(case // 64) % 6]
        radius = (-1, 0, 4, 5, 10, 2147483647)[(case // 7) % 6]
        if case >= 768:
            life, state, zone, instance = 0, 4, 7, 2
            x = (4.999999523162842, 5., 5.000000476837158, 16777216., float('nan'), float('inf'), -3., 1e20)[case % 8]
            y, z = 10000., (0., 4.)[(case // 8) % 2]
            radius = (-1, 0, 5, 16777217)[(case // 16) % 4]
        for user, l, s, zo, inst, xyz in ((candidate, life, state, zone, instance, (x,y,z)), (reference, 1, 0, 7, 2, (0.,0.,0.))):
            put(user + 0x142c, l); put(user + 0x5758, s); put(user + 0xe0, zo)
            cpu.mem_write(user + 0x164, struct.pack('<H', inst))
            cpu.mem_write(user + 0xd0, struct.pack('<fff', *xyz))
        print('R', life, state, zone, instance, x, y, z, radius, call(0x45de60, radius))

if __name__ == '__main__':
    main()
