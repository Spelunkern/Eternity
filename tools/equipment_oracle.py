#!/usr/bin/env python3
"""Execute original equipment/gem/craft code to generate regression vectors; requires Unicorn."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EDX
from original_oracle import load_game


def definition(item_type, item_id, gem=False):
    data = bytearray(168)
    data[0x24:0x26] = bytes((item_type, item_id))
    if not gem:
        struct.pack_into("<H", data, 0x3e, 50)
        struct.pack_into("<H", data, 0x42, 4)
        data[0x44:0x4b] = bytes((2, 1, 0, 6, 1, 0, 1))
        struct.pack_into("<3H", data, 0x4c, 1000, 500, 100)
        for i in range(4):
            struct.pack_into("<3H", data, 0x52 + 6 * i, 1001 + i, (1001 + i) // 2, 0)
        struct.pack_into("<9H", data, 0x6a, 101, 202, 303, 1, 2, 3, 4, 5, 6)
        struct.pack_into("<I", data, 0xa0, {5: 5, 16: 16, 21: 21, 45: 1}[item_type])
    elif item_id == 1:
        struct.pack_into("<H", data, 0x42, 2)
        data[0x44:0x4a] = bytes((1, 1, 4, 0, 3, 5))
        for i in range(4):
            struct.pack_into("<H", data, 0x52 + 6 * i, 31 + i)
        struct.pack_into("<9H", data, 0x6a, 21, 22, 23, 11, 12, 13, 14, 15, 16)
    elif item_id == 2:
        data[0x46] = 16
        struct.pack_into("<9H", data, 0x6a, 7, 8, 9, 1, 2, 3, 4, 5, 6)
    elif item_id == 3:
        data[0x45] = 2
        data[0x46] = 1
    elif item_id == 4:
        data[0x45] = 1
    return bytes(data)


def main():
    machine, digest = load_game()
    stack, user, item, info, text, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x10000000
    for address in (stack, user, item, info, text, stop):
        machine.mem_map(address, 0x10000)
    # Critical sections protect an explicitly supplied local skill list; no OS/network/SQL calls execute.
    machine.mem_write(stop + 0x100, b"\xc2\x04\x00")
    for iat in (0x5210cc, 0x5210d0):
        machine.mem_write(iat, struct.pack("<I", stop + 0x100))
    for step in range(21):
        machine.mem_write(0x542100 + step * 4, struct.pack("<HH", step * 7, step * 11))
    for gem in range(1, 5):
        machine.mem_write(0x6882ac + 168 * gem, definition(30, gem, True))
    rng = random.Random(0x454c70)
    cases = []
    for slot in range(13):
        for quality in (0, 100, 101, 500, 501, 1000):
            cases.append((slot, 5 if slot == 5 else 16 if slot < 8 else 21, quality, 0, 0, 0,
                          "01020304050607080903", [1, 2, 3, 4, 5, 0]))
    for _ in range(60):
        cases.append((rng.randrange(13), rng.choice((5, 16, 21, 45)), rng.randrange(1200), rng.randrange(2), rng.randrange(2), rng.randrange(2),
                      rng.choice(("short", "00000000000000000000", "99999999999999999970", "01020304050607080951", "01020304050607080921", "0102030405060708xx99")),
                      [rng.randrange(6) for _ in range(6)]))
    print(f"# ps_game.exe SHA256 {digest}; ItemEquipmentAdd 00454c70, CopyCraftExpassion 004ba670, GemEquipmentAdd 004560c0")
    print("# slot type quality transformed skill95 atk_buff craft gem[6]; output attributes[6], HP/MP/SP, attack/variance/physical/magic/range/aspeed/mspeed/reduction, elements[2], recoveries[6], state_blocks[17], quality, craft_values[11], enchant_step")
    for slot, item_type, quality, transformed, skill95, buff, craft, gems in cases:
        memory = bytearray(0x9000)
        for offset, value in zip((0x1220, 0x1224, 0x1230, 0x1228, 0x122c, 0x1234), (100, 200, 300, 400, 500, 600)):
            struct.pack_into("<i", memory, offset, value)
        for offset, value in zip((0x17c, 0x180, 0x184), (10000, 20000, 30000)):
            struct.pack_into("<I", memory, offset, value)
        for offset, value in zip((0x12d8, 0x12dc, 0x12e0, 0x12e4, 0x12e8, 0x12ec, 0x12f0, 0x12fc), (10, 20, 30, 40, 50, 60, 2, 70)):
            struct.pack_into("<i", memory, offset, value)
        struct.pack_into("<II", memory, 0x1280, 7, 8)
        struct.pack_into("<3i", memory, 0x1250, 28, 28, 28)
        struct.pack_into("<3i", memory, 0x1268, 4, 4, 4)
        memory[0x1354], memory[0x57b5], memory[0x1334], memory[0x12cd] = transformed, 1, 9, buff
        memory[0x1308:0x131c] = bytes([3] * 20)
        memory[0x131c:0x1330] = bytes([7] * 20)
        struct.pack_into("<I", memory, 0xa94, user + 0x8000)
        struct.pack_into("<I", memory, 0x8004, user + (0x8100 if skill95 else 0x8000))
        struct.pack_into("<I", memory, 0x8104, user + 0x8000)
        struct.pack_into("<I", memory, 0x8130, user + 0x8200)
        struct.pack_into("<H", memory, 0x8232, 95)
        machine.mem_write(user, bytes(memory))
        machine.mem_write(info, definition(item_type, 1))
        machine.mem_write(item, bytes(160))
        machine.mem_write(item + 0x30, struct.pack("<I", info))
        machine.mem_write(item + 0x44, struct.pack("<H6B", quality, *gems))
        machine.mem_write(text, craft.encode() + b"\0")
        machine.mem_write(stack + 0x8000, struct.pack("<II", stop, slot))
        machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        machine.reg_write(UC_X86_REG_EDI, item)
        machine.reg_write(UC_X86_REG_EAX, text)
        machine.emu_start(0x4ba670, stop, count=10000)
        if machine.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError("Craft function did not return")
        expansion = struct.unpack("<11H", machine.mem_read(item + 0x84, 22))
        machine.mem_write(stack + 0x8000, struct.pack("<II", stop, slot))
        machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        machine.reg_write(UC_X86_REG_EDI, user)
        machine.reg_write(UC_X86_REG_EAX, item)
        machine.emu_start(0x454c70, stop, count=10000)
        if machine.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError("Equipment function did not return")
        offsets = (0x1220, 0x1224, 0x1230, 0x1228, 0x122c, 0x1234, 0x17c, 0x180, 0x184,
                   0x12d8, 0x12dc, 0x12e0, 0x12e4, 0x12e8, 0x12ec, 0x12f0, 0x12fc, 0x1280, 0x1284,
                   0x1268, 0x126c, 0x1270, 0x1250, 0x1254, 0x1258, *range(0x1288, 0x12cc, 4))
        values = [struct.unpack("<i", machine.mem_read(user + offset, 4))[0] for offset in offsets]
        stage = machine.mem_read(user + 0x19d + slot, 1)[0]
        machine.reg_write(UC_X86_REG_EDI, user)
        # GetEnchantStep takes EDX, independently of item eligibility.
        machine.reg_write(UC_X86_REG_EDX, item)
        machine.mem_write(stack + 0x8000, struct.pack("<I", stop))
        machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        machine.emu_start(0x4ba9b0, stop, count=10000)
        enchant_step = machine.reg_read(UC_X86_REG_EAX) & 255
        print(slot, item_type, quality, transformed, skill95, buff, craft, *gems, *values, stage, *expansion, enchant_step, sep="\t")


if __name__ == "__main__":
    main()
