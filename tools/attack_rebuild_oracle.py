#!/usr/bin/env python3
"""Execute original attack-only equipment/transform rebuilding with cached quality tiers. Requires Unicorn."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game
from equipment_oracle import definition


def main():
    cpu, digest = load_game()
    stack, user, items, infos, text, mobs, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x35000000, 0x10000000
    for address in (stack, user, items, infos, text, mobs, stop):
        cpu.mem_map(address, 0x10000)
    cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    for step in range(21):
        cpu.mem_write(0x542100 + step * 4, struct.pack('<HH', step * 7, step * 11))
    for gem in range(1, 5):
        cpu.mem_write(0x6882ac + 168 * gem, definition(30, gem, True))
    cpu.mem_write(0xfcd2ac, struct.pack('<2I', 4, mobs))
    for index in range(4):
        cpu.mem_write(mobs + index * 268, struct.pack('<H', index))
        cpu.mem_write(mobs + index * 268 + 0x22, struct.pack('<H', 0 if index in (2, 3) else 2))
        cpu.mem_write(mobs + index * 268 + 0x30, struct.pack('<I', 0 if index == 2 else 0xffffffff))
        cpu.mem_write(mobs + index * 268 + 0x8a, struct.pack('<2H', 34123, 54234))
        cpu.mem_write(mobs + index * 268 + 0x42, struct.pack('<2H', 65535, 12345))
    cases = [(1, slot, quality, 5 if slot == 5 else 16 if slot < 8 else 21, shape, '01020304050607080903', [1, 2, 3, 4, 5, 0], 1)
             for slot in range(13) for quality in range(3) for shape in (0, 1)]
    rng = random.Random(0x454ba0)
    for _ in range(100):
        cases.append((1, rng.choice((-2, -1, *range(13))), rng.randrange(3), rng.choice((5, 16, 21, 45)), rng.randrange(2),
                      rng.choice(('short', '99999999999999999970', '01020304050607080951', '01020304050607080903')),
                      [rng.randrange(6) for _ in range(6)], 1))
    cases += [(0, -2, 0, 5, shape, 'short', [1, 2, 3, 4, 5, 0], index)
              for shape in (0, 1) for index in (0, 1, 2, 3, 4, 65535, 0xffffffff)]
    offsets = (0x12d8, 0x12dc, 0x12e0, 0x12e4, 0x12e8, 0x12ec, 0x12f0, 0x12fc, 0x1280, 0x1284,
               0x1220, 0x1224, 0x1230, 0x1228, 0x122c, 0x1234, 0x17c, 0x180, 0x184)
    print(f'# ps_game.exe SHA256 {digest}; InitEquipment(bool) 00454ba0, ItemEquipmentAttackAdd 00456450, GemEquipmentAttackAdd 004565b0, SetAttack 00454280')
    print('# equipment_mode slot(-1=empty,-2=all) cached_quality type shape craft gems[6] mob_index; result fields ' + ' '.join(hex(x) for x in offsets) + '; 3*(accuracy,attack,evasion,defense,critical_attack,critical_defense)')
    for mode, slot, quality, item_type, shape, craft, gems, mob_index in cases:
        memory = bytearray(0x6000)
        for offset, value in zip((0x1220, 0x1224, 0x1230, 0x1228, 0x122c, 0x1234), (100, 200, 300, 400, 500, 600)):
            struct.pack_into('<i', memory, offset, value)
        struct.pack_into('<3I', memory, 0x17c, 10000, 20000, 30000)
        struct.pack_into('<4i', memory, 0x12d8, -2147483648, 2147483647, -1, 23)
        struct.pack_into('<3i', memory, 0x12e8, 50, 60, 2)
        struct.pack_into('<i', memory, 0x12fc, 70)
        struct.pack_into('<2I', memory, 0x1280, 7, 8)
        struct.pack_into('<12i', memory, 0x1374, *range(31, 43))
        struct.pack_into('<H', memory, 0x136, 30)
        memory[0x1334] = 9
        memory[0x131c:0x1330] = bytes([7] * 20)
        memory[0x1354] = shape
        struct.pack_into('<I', memory, 0x1358, mob_index)
        cpu.mem_write(user, bytes(memory))
        positions = list(range(13)) if slot == -2 else [] if slot == -1 else [slot]
        positions += [24 + 5]  # A bag item must never contribute to rebuilding equipment.
        for position in positions:
            item, info = items + position * 256, infos + position * 256
            kind = (5 if position == 5 else 16 if position < 8 else 21) if slot == -2 else item_type
            cpu.mem_write(info, definition(kind, 1))
            cpu.mem_write(item, bytes(192))
            cpu.mem_write(item + 0x30, struct.pack('<I', info))
            cpu.mem_write(item + 0x44, struct.pack('<H6B', 0, *gems))  # Deliberately unrelated to cached quality.
            cpu.mem_write(text, craft.encode() + b'\0')
            cpu.mem_write(stack + 0x8000, struct.pack('<2I', stop, position))
            cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
            cpu.reg_write(UC_X86_REG_EDI, item)
            cpu.reg_write(UC_X86_REG_EAX, text)
            cpu.emu_start(0x4ba670, stop, count=10000)
            cpu.mem_write(user + 0x1c4 + position * 4, struct.pack('<I', item))
            if position < 13:
                cpu.mem_write(user + 0x19d + position, bytes([(quality + position) % 3 if slot == -2 else quality]))
        cpu.mem_write(stack + 0x8000, struct.pack('<2I', stop, mode))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.reg_write(UC_X86_REG_ECX, user)
        cpu.emu_start(0x454ba0, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original attack rebuild did not return')
        result = [struct.unpack('<i', cpu.mem_read(user + offset, 4))[0] for offset in offsets]
        for category in range(3):
            result.extend(struct.unpack('<6i', cpu.mem_read(user + 0x13b8 + 44 * category, 24)))
        print(mode, slot, quality, item_type, shape, craft, *gems, mob_index, *result, sep='\t')


if __name__ == '__main__':
    main()
