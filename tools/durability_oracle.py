#!/usr/bin/env python3
"""Execute original quality decrement/stat deltas and defense wear traversal."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
from equipment_oracle import definition


def main():
    cpu, digest = load_game()
    stack, user, items, info, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x10000000
    for address in (stack, user, items, info, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_write(info, definition(5, 1))
    cpu.mem_write(0x454280, b'\xc3')  # Final attack calculation has its own instruction oracle.
    recalculations = []

    def snapshot():
        return struct.unpack('<4i', cpu.mem_read(user + 0x12d8, 16))

    def hook(machine, address, size, context):
        recalculations.append(snapshot())

    cpu.hook_add(UC_HOOK_CODE, hook, begin=0x454280, end=0x454280)

    def initialize(present, qualities, tiers, values):
        cpu.mem_write(user, bytes(0x6000))
        cpu.mem_write(user + 0x19d, bytes(tiers))
        cpu.mem_write(user + 0x12d8, struct.pack('<4i', *values))
        for slot in range(13):
            item = items + slot * 256
            cpu.mem_write(item, bytes(160))
            cpu.mem_write(item + 0x30, struct.pack('<I', info))
            cpu.mem_write(item + 0x44, struct.pack('<H', qualities[slot]))
            cpu.mem_write(item + 0x96, struct.pack('<H', 56789))
            cpu.mem_write(user + 0x1c4 + 4 * slot, struct.pack('<I', item if present & (1 << slot) else 0))
        recalculations.clear()

    def run(entry, argument=0):
        cpu.mem_write(stack + 0x8000, struct.pack('<II', stop, argument & 0xffffffff))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.emu_start(entry, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original durability routine did not return')
        return cpu.reg_read(UC_X86_REG_EAX) & 255

    rng = random.Random(0x45b350)
    print(f'# ps_game.exe SHA256 {digest}; QualityDec 0045b350, QualityByDefense 0045b290, ItemEquipmentAttackRemQ/AddQ 004566d0/00456620')
    print('# Q: slot present quality tier decrement stats[4] -> bool quality tier stats[4] recalcCount snapshots[4]*; D: cursor noWear shape mask qualities[13] tiers[13] stats[4] -> cursor qualities[13] tiers[13] stats[4] recalcCount snapshots[4]*')
    cases = []
    for slot in range(13):
        for quality in (0, 1, 100, 101, 500, 501, 1000, 65535):
            for amount in (0, 1, 10, -1, 0x7fffffff, -0x80000000):
                cases.append((slot, 1, quality, rng.randrange(3), amount))
    for _ in range(512):
        cases.append((rng.randrange(13), rng.randrange(2), rng.randrange(65536), rng.randrange(3), rng.randrange(-0x80000000, 0x80000000)))
    for slot, present, quality, tier, amount in cases:
        values = tuple(rng.randrange(-0x80000000, 0x80000000) for _ in range(4))
        initialize((1 << slot) if present else 0, [quality] * 13, [tier] * 13, values)
        cpu.reg_write(UC_X86_REG_EAX, user)
        cpu.reg_write(UC_X86_REG_EDX, items + slot * 256 if present else 0)
        cpu.reg_write(UC_X86_REG_ESI, slot)
        result = run(0x45b350, amount)
        final_quality = struct.unpack('<H', cpu.mem_read(items + slot * 256 + 0x44, 2))[0]
        final_tier = cpu.mem_read(user + 0x19d + slot, 1)[0]
        print('Q', slot, present, quality, tier, amount, *values, result, final_quality, final_tier,
              *snapshot(), len(recalculations), *(v for entry in recalculations for v in entry), sep='\t')
    for case in range(1536):
        cursor = rng.choice((0, 1, 2, 3, 4, 5, 6, 7, 0x7fffffff))
        no_wear = rng.choice((0, 0, 0, 1, -1))
        shape = rng.choice((0, 0, 0, 13, 100, 255))
        mask = rng.randrange(8192)
        qualities = [rng.choice((0, 1, 100, 101, 500, 501, 1000)) for _ in range(13)]
        tiers = [rng.randrange(3) for _ in range(13)]
        values = tuple(rng.randrange(-0x80000000, 0x80000000) for _ in range(4))
        initialize(mask, qualities, tiers, values)
        cpu.mem_write(user + 0x144c, struct.pack('<i', cursor))
        cpu.mem_write(user + 0x58b8, struct.pack('<i', no_wear))
        cpu.mem_write(user + 0x1354, bytes([shape]))
        cpu.reg_write(UC_X86_REG_EDI, user)
        run(0x45b290)
        final_cursor = struct.unpack('<i', cpu.mem_read(user + 0x144c, 4))[0]
        final_qualities = [struct.unpack('<H', cpu.mem_read(items + slot * 256 + 0x44, 2))[0] for slot in range(13)]
        final_tiers = list(cpu.mem_read(user + 0x19d, 13))
        print('D', cursor, no_wear, shape, mask, *qualities, *tiers, *values, final_cursor, *final_qualities,
              *final_tiers, *snapshot(), len(recalculations), *(v for entry in recalculations for v in entry), sep='\t')


if __name__ == '__main__':
    main()
