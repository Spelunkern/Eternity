#!/usr/bin/env python3
"""Execute original blessing SData loading, actor initialization and notification. Requires Unicorn."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game, ROOT

OFFSETS = (0x1220, 0x1224, 0x1230, 0x1228, 0x122c, 0x1234, 0x17c, 0x180, 0x184,
           0x1268, 0x126c, 0x1270, 0x1250, 0x1254, 0x1258, 0x1244, 0x1248, 0x124c, 0x12f4, *range(0x1374, 0x13a4, 4))


def main():
    cpu, digest = load_game()
    stack, user, world, stop = 0x20000000, 0x30000000, 0x31000000, 0x10000000
    for address in (0, stack, user, world, stop):
        cpu.mem_map(address, 0x10000)
    cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    cpu.mem_write(0x546f38, struct.pack('<I', world))
    for address, cleanup in ((0x505afd, 0), (0x510785, 0), (0x50c649, 0), (0x4d4d20, 8)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    cpu.mem_write(stop + 0x100, b'\xc3')
    cpu.mem_write(0x5210e4, struct.pack('<I', stop + 0x100))
    context = dict(data=b'', position=0, tick=0, packets=[])

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x505afd:
            context['position'] = 0
            machine.reg_write(UC_X86_REG_EAX, 12)
        elif address == 0x510785:
            fd, ptr, size = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
            data = context['data'][context['position']:context['position'] + size]
            machine.mem_write(ptr, data)
            context['position'] += len(data)
            machine.reg_write(UC_X86_REG_EAX, len(data))
        elif address == 0x4d4d20:
            ptr, size = struct.unpack('<2I', machine.mem_read(sp + 4, 8))
            context['packets'].append(bytes(machine.mem_read(ptr, size)).hex())
        elif address == stop + 0x100:
            machine.reg_write(UC_X86_REG_EAX, context['tick'])

    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(entry, args=()):
        cpu.mem_write(stack + 0xf000, struct.pack('<' + 'I' * (1 + len(args)), stop, *args))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0xf000)
        cpu.reg_write(UC_X86_REG_ESI, user)
        cpu.reg_write(UC_X86_REG_EDI, user)
        cpu.emu_start(entry, stop, count=200000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original blessing routine did not return')

    print(f'# ps_game.exe SHA256 {digest}; LoadKCStatus 004bf4a0; InitKCStatus 0048be90; SendKCStatus 00481920')
    print('# L input_hex; country sizes, records(index,kills,value,28 modifiers). I 31 initial fields,2 calculated defenses,28 modifiers; 31 final fields,28 applied. P country,kills,end,tick; hex packet')
    print('# I field offsets: ' + ' '.join(hex(x) for x in OFFSETS))
    synthetic = [(0, 100, 7, [(4, 1), (4, 9), (28, 1), (255, 1), (0, 12), (1, 13)]),
                 (1, 100, 8, [(1, 15)] * 6), (0, 50, 10, [(4, 3), (8, 20)] + [(0, 0)] * 4)]
    data = struct.pack('<I', len(synthetic))
    for country, kills, value, modifiers in synthetic:
        data += struct.pack('<BIH', country, kills, value) + b''.join(struct.pack('<BH', *pair) for pair in modifiers)
    for data in (struct.pack('<I', 0), data, (ROOT / '.local/server-data/KillStatus.SData').read_bytes()):
        cpu.mem_write(0xfcd2cc, bytes(8 + 512 * 124))
        context['data'] = data
        call(0x4bf4a0, (0x54baa8,))
        if cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
            raise RuntimeError('Original blessing load failed')
        sizes = struct.unpack('<2I', cpu.mem_read(0xfcd2cc, 8))
        records = []
        for country, count in enumerate(sizes):
            for index in range(count):
                records.extend(struct.unpack('<31i', cpu.mem_read(0xfcd2d4 + (country * 256 + index) * 124, 124)))
        print('L', data.hex(), *sizes, *records, sep='\t')
    rng = random.Random(0x48be90)
    for scenario in range(240):
        initial = [rng.randrange(-2**31, 2**31) if scenario >= 80 else rng.randrange(0, 60000) for _ in OFFSETS]
        defenses = [rng.randrange(-2**31, 2**31) if scenario >= 80 else rng.randrange(0, 60000) for _ in range(2)]
        modifiers = [rng.randrange(-2**31, 2**31) if scenario >= 160 else rng.randrange(-100, 101) for _ in range(28)]
        if scenario < 28:
            modifiers = [int(i == scenario) * 17 for i in range(28)]
        cpu.mem_write(user, bytes(0x6000))
        cpu.mem_write(0xfcd2cc, struct.pack('<2I', 1, 1))
        country = scenario % 2
        cpu.mem_write(user + 0x12d, bytes([country]))
        cpu.mem_write(world + 0x1fc + 0x84 * country, bytes(4))
        for offset, value in zip(OFFSETS, initial):
            cpu.mem_write(user + offset, struct.pack('<i', value))
        for offset, value in zip((0x13c4, 0x13f0), defenses):
            cpu.mem_write(user + offset, struct.pack('<i', value))
        cpu.mem_write(0xfcd2d4 + country * 0x7c00 + 12, struct.pack('<28i', *modifiers))
        call(0x48be90)
        final = [struct.unpack('<i', cpu.mem_read(user + offset, 4))[0] for offset in OFFSETS]
        applied = struct.unpack('<28i', cpu.mem_read(user + 0x14b8, 112))
        print('I', *initial, *defenses, *modifiers, *final, *applied, sep='\t')
    for country in (0, 1):
        for end, tick in ((0, 10), (1000, 100), (1000, 1000), (1000, 1001), (0x7fffffff, 0xffffffff), (0x100, 0xffffff00), (0xffffffff, 0)):
            cpu.mem_write(user + 0x12d, bytes([country]))
            cpu.mem_write(world + 0x200 + 0x84 * country, struct.pack('<I', 0x12345678))
            cpu.mem_write(world + 0x208 + 0x84 * country, struct.pack('<I', end))
            context.update(tick=tick, packets=[])
            call(0x481920)
            if len(context['packets']) != 1:
                raise RuntimeError('Unexpected blessing notification count')
            print('P', country, 0x12345678, end, tick, context['packets'][0], sep='\t')


if __name__ == '__main__':
    main()
