#!/usr/bin/env python3
"""Original CMob::Damage callbacks: live state, input reloads and credit timing."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    root = 0x30000000
    cpu.mem_map(root, 0x20000)
    mob, values, definition, stack, stop = [root + x for x in (0, 0x2000, 0x3000, 0x10000, 0x1f000)]

    def put(at, *numbers):
        cpu.mem_write(at, struct.pack('<' + 'I' * len(numbers), *[n & 0xffffffff for n in numbers]))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    def snapshot(tag, argument=0):
        mp, sp = struct.unpack('<HH', cpu.mem_read(mob + 0x228, 4))
        numbers = [get(mob + 0x224), mp, sp, get(mob + 0x124), get(mob + 0x128),
                   get(mob + 0xcb0), int(get(definition + 0x6c) != 0),
                   int(get(mob + 0x44) != 0), int(get(mob + 0x284) != 0),
                   *[get(values + i) for i in (0, 4, 8)], len(credits), argument]
        return tag + ':' + ','.join(map(str, numbers))

    def hook(c, at, size, data):
        nonlocal status
        pop, result = 0, None
        if at == 0x41d760:
            result = status
        elif at == 0x4925f0:
            events.append(snapshot('E'))
            if mask & 1:
                put(mob + 0x224, 17)
                cpu.mem_write(mob + 0x228, struct.pack('<HH', 60, 70))
                put(mob + 0x124, 9, 99)
                put(definition + 0x6c, not get(definition + 0x6c))
                put(mob + 0x284, 1)
                put(values, 31, -4, 9)
                status = 1
            pop = 4
        elif at == stop + 16:
            events.append(snapshot('T'))
            if mask & 4:
                put(definition + 0x6c, not get(definition + 0x6c))
                put(mob + 0x128, 88)
            result = 0xfffffff0
        elif at == 0x4a1810:
            events.append(snapshot('L', get(c.reg_read(UC_X86_REG_ESP) + 12)))
            if mask & 2:
                put(mob + 0x224, 23)
                cpu.mem_write(mob + 0x228, struct.pack('<HH', 5, 7))
                put(mob + 0x124, 6, 66)
                put(values, 11, 3, -2)
                put(mob + 0x44, not get(mob + 0x44))
                status = 1
            pop = 12
        elif at == 0x4b8dd0:
            esp = c.reg_read(UC_X86_REG_ESP)
            events.append(snapshot('A', get(esp + 8)))
            credits.append([c.reg_read(UC_X86_REG_EBX)] + [get(esp + i) for i in (4, 8, 12, 16)])
            pop = 16
        elif at in (0x4b7490, 0x490ab0):
            events.append(snapshot('X' if at == 0x4b7490 else 'R'))
        else:
            return
        esp = c.reg_read(UC_X86_REG_ESP)
        ret = get(esp)
        c.reg_write(UC_X86_REG_ESP, esp + 4 + pop)
        if result is not None:
            c.reg_write(UC_X86_REG_EAX, result)
        c.reg_write(UC_X86_REG_EIP, ret)

    cpu.hook_add(UC_HOOK_CODE, hook)
    put(0x5210e4, stop + 16)
    rng = random.Random(0x490f4b)
    print(f'# ps_game.exe SHA256 {digest}; CMob::Damage 00490cf0/00490ee0; observed service ports with explicit callback mutations')
    print('# mode mask status hp mp sp flags d0 d1 d2 result final_snapshot callback_snapshots credit_or_dash')
    for case in range(512):
        mode, mask = case % 2, (case // 2) % 8
        initial_status = status = rng.choice((0, 1, 2, 3))
        hp, mp, sp = rng.choice((0, 10, 100)), rng.randrange(80), rng.randrange(80)
        flags = rng.randrange(8)  # stacked, long delay, direct logging
        damages = [rng.choice((0, 9, 10, 100, 0xffffffff)) for _ in range(3)]
        cpu.mem_write(mob, b'\0' * 0x1000)
        put(mob + 0x224, hp)
        cpu.mem_write(mob + 0x228, struct.pack('<HH', mp, sp))
        put(mob + 0x124, 7, 8)
        put(mob + 0x44, flags & 1)
        put(mob + 0xa8, definition)
        put(definition + 0x6c, flags & 2)
        put(mob + 0x284, flags & 4)
        put(values, *damages)
        events, credits = [], []
        put(stack, stop, mob, 1, 42, 3, 4, 77)
        cpu.reg_write(UC_X86_REG_EAX, values if mode else damages[0])
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.emu_start(0x490ee0 if mode else 0x490cf0, stop, count=10000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Damage did not return')
        credit = ','.join(map(str, credits[0])) if credits else '-'
        print(mode, mask, initial_status, hp, mp, sp, flags, *damages,
              cpu.reg_read(UC_X86_REG_EAX) & 255, snapshot('F'), '/'.join(events) or '-', credit, sep='\t')


if __name__ == '__main__':
    main()
