#!/usr/bin/env python3
"""Execute the original SetAttack function to generate regression vectors (optional Unicorn dependency)."""
import argparse
import random
import struct
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_FPCW, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--precision64', action='store_true', help='Inspect the alternative x87 precision mode')
    precision = 0x37f if parser.parse_args().precision64 else 0x27f
    machine, digest = load_game()
    stack, user, stop = 0x20000000, 0x30000000, 0x10000000
    for address in (stack, user, stop):
        machine.mem_map(address, 0x10000)
    rng = random.Random(454280)
    print(f"# ps_game.exe SHA256 {digest}; CUser::SetAttack 00454280; Ghidra disassembly")
    print("# level str dex rec int wis luck item_atk phys_def magic_def critical; 3*(accuracy attack evasion defense) additions; 3*(accuracy evasion attack defense critical_attack critical_defense) outputs")
    cases = [(1, [14, 12, 9, 8, 7, 15], [0] * 16)]
    for _ in range(80):
        cases.append((rng.randrange(1, 61), [rng.randrange(-500, 5000) for _ in range(6)],
                      [rng.randrange(-200, 2000) for _ in range(16)]))
    # Precision boundaries and original signed-integer wrap/indefinite behavior.
    for attributes in ([2**24 + 1] * 6, [2**31 - 1] * 6, [-2**31] * 6):
        cases.append((60, attributes, [2**30] * 16))
    for _ in range(160):
        cases.append((rng.randrange(1, 61), [rng.randrange(-2**31, 2**31) for _ in range(6)],
                      [rng.randrange(-2**30, 2**30) for _ in range(16)]))
    for level, attributes, modifiers in cases:
        memory = bytearray(0x6000)
        struct.pack_into("<H", memory, 0x136, level)
        for offset, value in zip((0x1220, 0x1224, 0x1230, 0x1228, 0x122c, 0x1234), attributes):
            struct.pack_into("<i", memory, offset, value)
        for offset, value in zip((0x12d8, 0x12e0, 0x12e4, 0x12f4), modifiers[:4]):
            struct.pack_into("<i", memory, offset, value)
        struct.pack_into("<12i", memory, 0x1374, *modifiers[4:])
        machine.mem_write(user, bytes(memory))
        machine.mem_write(stack + 0x8000, struct.pack("<I", stop))
        machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        machine.reg_write(UC_X86_REG_ECX, user)
        machine.reg_write(UC_X86_REG_FPCW, precision)
        machine.emu_start(0x454280, stop, count=10000)
        if machine.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError("Original attack calculation did not return")
        values = []
        for category in range(3):
            values.extend(struct.unpack("<6i", machine.mem_read(user + 0x13b8 + 44 * category, 24)))
        # Machine order: accuracy, attack, evasion, defense, critical attack, critical defense.
        result = [values[i + j] for i in (0, 6, 12) for j in (0, 2, 1, 3, 4, 5)]
        print(level, *attributes, *modifiers, *result, sep="\t")


if __name__ == "__main__":
    main()
