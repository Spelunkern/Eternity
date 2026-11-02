#!/usr/bin/env python3
"""Execute original ability/passive initialization; choose 'ability' or 'passive'. Requires Unicorn."""
import argparse
import struct
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

OFFSETS = (0x1220, 0x1224, 0x1230, 0x1228, 0x122c, 0x1234, 0x17c, 0x180, 0x184,
           0x125c, 0x1260, 0x1264, 0x1278, 0x127c, 0x12e8, 0x12ec, 0x12f0, 0x12f4, 0x12f8,
           *range(0x1374, 0x13a4, 4), 0x13a4, 0x13d0, 0x13fc,
           *range(0x58b0, 0x58dc, 4), 0x1300, 0x1304, 0x1330, 0x12d8, 0x12e0)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("mode", choices=("ability", "passive"))
    mode = parser.parse_args().mode
    machine, digest = load_game()
    stack, user, skill, weapon, shield, info, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x10000000
    for address in (0, stack, user, skill, weapon, shield, info, stop):
        machine.mem_map(address, 0x10000)
    print(f"# ps_game.exe SHA256 {digest}; SetSkillAbility 00485e00; AddSkillPassive 00489a80; no gameplay/SQL/network calls")
    print("# inputs followed by state fields at offsets: " + " ".join(hex(x) for x in OFFSETS) + "; weapon_speed[20],weapon_damage[20],shield byte,hp ability applied/id/level/previous")
    if mode == "ability":
        cases = [(kind, effect, amount, amulet) for kind in range(48) for effect in (0, 3, 4)
                 for amount in (-23, 0, 17, 2147483647) for amulet in (1, 3)]
    else:
        cases = [(detail, weapon_type, shield_type, first, second, amount) for detail in range(12)
                 for weapon_type, shield_type in ((0, 0), (5, 15), (1, 0), (0, 15))
                 for first, second, amount in ((5, 1, 17), (5, 5, 200), (0, 0, 0))]
    for case in cases:
        memory = bytearray(0x6000)
        for offset, value in zip((0x1220, 0x1224, 0x1230, 0x1228, 0x122c, 0x1234), (100, 200, 300, 400, 500, 600)):
            struct.pack_into("<i", memory, offset, value)
        for offset, value in zip((0x17c, 0x180, 0x184, 0x1304, 0x12f0), (10000, 20000, 30000, 2, 2)):
            struct.pack_into("<i", memory, offset, value)
        memory[0x57b5] = 1
        machine.mem_write(user, bytes(memory))
        machine.mem_write(stack + 0x8000, struct.pack("<II", stop, user))
        machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        if mode == "ability":
            kind, effect, amount, amulet = case
            machine.mem_write(user + 0x58b0, struct.pack("<i", amulet))
            machine.reg_write(UC_X86_REG_ECX, kind)
            machine.reg_write(UC_X86_REG_EDX, effect)
            machine.reg_write(UC_X86_REG_EAX, amount & 0xffffffff)
            entry = 0x485e00
        else:
            detail, weapon_type, shield_type, first, second, amount = case
            definition = bytearray(160)
            struct.pack_into("<HB", definition, 0, 1234, 3)
            struct.pack_into("<H", definition, 0x32, detail)
            definition[0x60:0x64] = bytes((first, second, amount, 5))
            for offset, kind, value in ((0x7c, 5, 13), (0x80, 22, 19), (0x84, 40, 1)):
                struct.pack_into("<BxH", definition, offset, kind, value)
            machine.mem_write(skill, bytes(definition))
            for pointer, item, item_info, real_type in ((0x1d8, weapon, info, weapon_type), (0x1dc, shield, info + 168, shield_type)):
                if real_type:
                    machine.mem_write(user + pointer, struct.pack("<I", item))
                    machine.mem_write(item + 0x30, struct.pack("<I", item_info))
                    machine.mem_write(item_info + 0xa0, struct.pack("<I", real_type))
            machine.reg_write(UC_X86_REG_ESI, skill)
            machine.reg_write(UC_X86_REG_EDI, user)
            entry = 0x489a80
        machine.emu_start(entry, stop, count=10000)
        if machine.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError("Original skill function did not return")
        values = [struct.unpack("<i", machine.mem_read(user + offset, 4))[0] for offset in OFFSETS]
        # Resources are unsigned in native state; compare their raw original bit patterns.
        for i in (6, 7, 8):
            values[i] &= 0xffffffff
        values.extend(machine.mem_read(user + 0x1308, 40))
        values.append(machine.mem_read(user + 0x1334, 1)[0])
        values.extend((machine.mem_read(user + 0x1368, 1)[0], *struct.unpack("<H", machine.mem_read(user + 0x136a, 2)),
                       machine.mem_read(user + 0x136c, 1)[0], *struct.unpack("<I", machine.mem_read(user + 0x1370, 4))))
        print(*case, *values, sep="\t")


if __name__ == "__main__":
    main()
