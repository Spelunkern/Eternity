#!/usr/bin/env python3
"""Execute original effect dispatch with recorded world-service calls. Requires Unicorn."""
import argparse
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--remove', action='store_true')
    parser.add_argument('--precision64', action='store_true', help='Cross-check x87 extended precision')
    args = parser.parse_args()
    removal = args.remove
    machine, digest = load_game()
    machine.reg_write(UC_X86_REG_FPCW, 0x37f if args.precision64 else 0x27f)
    stack, user, skill, source, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x10000000
    for address in (0, stack, user, skill, source, stop):
        machine.mem_map(address, 0x10000)
    events = []
    current = {}
    offsets = (0x1220, 0x1224, 0x1230, 0x1228, 0x122c, 0x1234, 0x17c, 0x180, 0x184,
               0x12ec, 0x12f0, 0x1280, 0x1284,
               *[0x13a4 + 44 * i + field for i in range(3) for field in (4, 8, 12, 16)],
               0x1338, 0x133c, 0x1340, 0x1344, 0x1348, 0x1350, 0x1358, 0x1360)
    patches = {0x481990: 0, 0x481aa0: 0, 0x481ba0: 0, 0x4827b0: 0, 0x482590: 0, 0x4826c0: 8,
               0x489ed0: 0, 0x416800: 4, 0x490980: 0, 0x4877f0: 12, 0x486e00: 4, 0x454ba0: 4, 0x482ad0: 4,
               0x44bad0: 0, 0x482a70: 4, 0x484c60: 0, 0x40a7c0: 0}
    if removal:
        patches.update({0x4505a0: 0, 0x4bd8d0: 12})
    for address, cleanup in patches.items():
        machine.mem_write(address, b"\xc2" + struct.pack("<H", cleanup))
    machine.mem_write(stop + 0x100, b"\xc2\x04\x00")
    for iat in (0x5210cc, 0x5210d0):
        machine.mem_write(iat, struct.pack("<I", stop + 0x100))
    machine.mem_write(stop + 0x300, b'\xc3')
    machine.mem_write(0x5210e4, struct.pack('<I', stop + 0x300))

    def hook(cpu, address, size, context):
        if address == stop + 0x300:
            cpu.reg_write(UC_X86_REG_EAX, 10000)
            return
        if address not in patches:
            return
        esp = cpu.reg_read(UC_X86_REG_ESP)
        args = struct.unpack("<4I", cpu.mem_read(esp, 16))
        if address in (0x481990, 0x481ba0, 0x481aa0):
            events.append((1, {0x481990: 0, 0x481ba0: 1, 0x481aa0: 2}[address], 0))
        elif address == 0x4827b0:
            events.append((2, 0, 0))
        elif address == 0x482590:
            events.append((3, 0, 0))
        elif address == 0x4826c0:
            events.append((4, args[2], 0))
        elif address == 0x489ed0:
            events.append((5, 0, 0))
        elif address == 0x416800:
            cpu.reg_write(UC_X86_REG_EAX, source + 0x4000)
        elif address == 0x490980:
            events.append((6, 777, 0))
        elif address == 0x4877f0:
            events.append((7, args[3], int(struct.unpack("<f", struct.pack("<I", args[2]))[0])))
        elif address == 0x486e00:
            events.append((8, 1234, 0))
        elif address == 0x454ba0:
            events.append((9, args[1], 0))
        elif address == 0x482ad0:
            events.append((10, 0, 0))
        elif address == 0x484c60:
            # Replacement delegates removal to world services; its implementation is not verified here.
            events.append((11, current['attribute'], 0))
            cpu.mem_write(user + 0x12cd, b"\0")
            cpu.mem_write(user + 0x1280, bytes(4))
        elif address == 0x4505a0:
            amounts = struct.unpack('<3i', cpu.mem_read(cpu.reg_read(UC_X86_REG_EAX), 12))
            events.extend(((12, amounts[0], amounts[1]), (13, amounts[2], 0)))

    machine.hook_add(UC_HOOK_CODE, hook)
    functions = 'RemApplySkillBuff 00485130 / Debuff 00485720' if removal else 'AddApplySkillBuff 00484cf0 / Debuff 004855b0'
    print(f"# ps_game.exe SHA256 {digest}; {functions}; world operations recorded, not executed")
    print("# detail effect attribute change_type scenario ability_set; state offsets " + " ".join(hex(x) for x in offsets) + "; shape/form/no-mob,element flags[9],speeds[2],calculated attacks[3*(accuracy,attack,evasion,defense,critical_attack,critical_defense)]; events(count,code,arg1,arg2)")
    for detail in range(60, 97):
        for effect in (3, 4):
            for scenario in range(4):
                if removal and effect == 3 and detail in (61, 63):
                    continue  # Periodic damage/reflection/death belongs to the pending combat implementation.
                events.clear()
                attribute = (0, 2, 1, 8)[scenario]
                change = (100, 101, 102, 7)[scenario]
                ability_set = scenario % 2
                current['attribute'] = attribute
                memory = bytearray(0x9000)
                for offset, value in zip((0x1220, 0x1224, 0x1230, 0x1228, 0x122c, 0x1234), (100, 200, 300, 400, 500, 600)):
                    struct.pack_into("<i", memory, offset, value)
                struct.pack_into("<3I", memory, 0x17c, 10000, 20000, 30000)
                struct.pack_into("<II", memory, 0x1280, 1, 7)
                struct.pack_into("<i", memory, 0x12f0, 2)
                struct.pack_into("<H", memory, 0x134c, 999)
                memory[0x57b5] = 1
                if removal:
                    count = (0, 1, -1, -2147483648)[scenario]
                    for offset in (0x1338, 0x133c, 0x1340, 0x1344, 0x1348, 0x1360):
                        struct.pack_into('<i', memory, offset, count)
                    memory[0x1354], memory[0x1366] = change & 255, 1
                    struct.pack_into('<I', memory, 0x1350, 888)
                    struct.pack_into('<I', memory, 0x1358, 123)
                    for i in range(3):
                        struct.pack_into('<4i', memory, 0x13a8 + 44 * i, 2, 19, 4321 if i == 1 else 1234, 3 - i)
                    if scenario:
                        for offset, item, item_info, item_type, element in ((0x1d8, source + 0x5000, source + 0x5200, 5, 1),
                                                                         (0x1c8, source + 0x5100, source + 0x5300, 16, 2)):
                            struct.pack_into('<I', memory, offset, item)
                            machine.mem_write(item, bytes(160))
                            machine.mem_write(item + 0x30, struct.pack('<I', item_info))
                            machine.mem_write(item_info, bytes(168))
                            machine.mem_write(item_info + 0x24, bytes((item_type, 1)))
                            machine.mem_write(item_info + 0x45, bytes((element,)))
                        if scenario == 2:
                            machine.mem_write(source + 0x5000 + 0x46, b'\x01')
                        if scenario == 3:
                            machine.mem_write(source + 0x5100 + 0x46, b'\x01')
                        gem = bytearray(168);gem[0x24:0x26] = bytes((30, 1));gem[0x45] = 3
                        machine.mem_write(0x6882ac + 168, bytes(gem))
                if scenario in (1, 2):
                    memory[0x12cd] = 1
                if scenario == 3:
                    struct.pack_into("<II", memory, 0x1528, 2, 777)
                    struct.pack_into("<I", memory, 0xe0, source + 0x4000)
                # One supplied previous element effect, so replacement delegates exactly one removal.
                struct.pack_into("<I", memory, 0xa94, user + 0x8000)
                struct.pack_into("<I", memory, 0x8004, user + 0x8100)
                struct.pack_into("<I", memory, 0x8104, user + 0x8000)
                struct.pack_into("<I", memory, 0x8130, user + 0x8200)
                struct.pack_into("<H", memory, 0x8232, 94)
                memory[0x824e] = 1
                machine.mem_write(user, bytes(memory))
                definition = bytearray(160)
                struct.pack_into("<HB", definition, 0, 1234, 3)
                definition[0x31], definition[0x4e], definition[0x57] = effect, attribute, 12
                struct.pack_into("<H", definition, 0x32, detail)
                struct.pack_into("<H", definition, 0x64, 37)
                definition[0x94:0x96] = bytes((scenario % 3, 19))
                struct.pack_into("<HH", definition, 0x98, change, 123)
                abilities = ((5, 13), (16, 17), (17, 19)) if ability_set == 0 else ((1, 0), (2, 23), (3, 29))
                for i, (kind, value) in enumerate(abilities):
                    struct.pack_into("<BxH", definition, 0x7c + 4 * i, kind, value)
                if removal:
                    struct.pack_into('<3H', definition, 0x8e, 3, 7, 65535)
                machine.mem_write(skill, bytes(definition))
                machine.mem_write(source + 0x2c, struct.pack("<I", 888))
                machine.mem_write(source + 0xc, struct.pack('<II', 1234, 3))
                if removal:
                    age = (0, 1001, 3000, 0xffffffff)[scenario]
                    machine.mem_write(source + 0x24, struct.pack('<I', (10000 - age) & 0xffffffff))
                machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
                if removal:
                    machine.mem_write(stack + 0x8000, struct.pack('<II', stop, source))
                    machine.reg_write(UC_X86_REG_ECX, user)
                    machine.reg_write(UC_X86_REG_EDX, skill)
                    machine.reg_write(UC_X86_REG_EBX, source)
                    entry = 0x485130 if effect == 4 else 0x485720
                elif effect == 4:
                    machine.mem_write(stack + 0x8000, struct.pack("<III", stop, user, skill))
                    entry = 0x484cf0
                else:
                    machine.mem_write(stack + 0x8000, struct.pack("<II", stop, source))
                    machine.reg_write(UC_X86_REG_ESI, skill)
                    machine.reg_write(UC_X86_REG_EDI, user)
                    entry = 0x4855b0
                machine.emu_start(entry, stop, count=20000)
                if machine.reg_read(UC_X86_REG_EIP) != stop:
                    raise RuntimeError("Original effect dispatch did not return")
                values = [struct.unpack("<i", machine.mem_read(user + offset, 4))[0] for offset in offsets]
                values.extend((machine.mem_read(user + 0x1354, 1)[0], *struct.unpack("<H", machine.mem_read(user + 0x134c, 2)), machine.mem_read(user + 0x1366, 1)[0]))
                values.extend(machine.mem_read(user + 0x12cc, 9))
                for function in (0x48a460, 0x48a4d0):
                    machine.mem_write(stack + 0x8000, struct.pack("<I", stop))
                    machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
                    machine.reg_write(UC_X86_REG_EAX, user)
                    machine.emu_start(function, stop, count=1000)
                    values.append(machine.reg_read(UC_X86_REG_EAX) & 0xffffffff)
                for category in range(3):
                    values.extend(struct.unpack('<6i', machine.mem_read(user + 0x13b8 + 44 * category, 24)))
                print(detail, effect, attribute, change, scenario, ability_set, *values, len(events), *[x for event in events for x in event], sep="\t")


if __name__ == "__main__":
    main()
