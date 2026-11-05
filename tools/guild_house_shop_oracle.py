#!/usr/bin/env python3
"""Execute the original CGuild::GetGuildHouseShopBuyRate over generated guild state."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game

ENTRY, BUY, POTION, IDS, ENTER, LEAVE = 0x430ac0, 0x541c18, 0x541c38, 0x541cb0, 0x5210cc, 0x5210d0
STACK, GUILD, STOP, STUB = 0x20000000, 0x30000000, 0x10000000, 0x11000000


def main():
    cpu, digest = load_game()
    for address in (STACK, GUILD, STOP, STUB):
        cpu.mem_map(address, 0x10000)
    cpu.mem_write(STUB, b"\xc2\x04\x00")
    cpu.mem_write(ENTER, struct.pack("<I", STUB))
    cpu.mem_write(LEAVE, struct.pack("<I", STUB))
    cpu.hook_add(UC_HOOK_CODE, lambda machine, *_: machine.emu_stop(), begin=STOP, end=STOP)
    rng = random.Random(0x430ac0)
    print(f"# ps_game.exe SHA256 {digest}; CGuild::GetGuildHouseShopBuyRate 00430ac0 executes unmodified with generated tables")
    print("# country rank npc type rates(8) ids(96) slots(16) result rate detail country_after slots_after(8 levels)")
    for _ in range(4096):
        rates = [rng.choice((.99, .95, .9, .85, .8, .75, .7, .6, rng.random())) for _ in range(8)]
        ids = [rng.randint(1, 40) if rng.random() < .3 else 0 for _ in range(96)]
        slots = []
        for _ in range(8):
            slots += [rng.choice((rng.randint(1, 4) << 8 | 1, rng.randint(0, 6) << 8 | 1, 0x202, 0)), rng.choice((0, 1, 2, 3, 3, 4, 9, 65535))]
        country, rank = rng.randint(0, 3), rng.choice((0, 1, 6, 7, 30, 31, 255, rng.randint(0, 255)))
        npc, shop_type = rng.choice((0, rng.randint(1, 40))), rng.choice((0, 1, 2, 3, 3, 4, 4, 5, 200, rng.randint(0, 255)))
        cpu.mem_write(GUILD, bytes(0x10000))
        cpu.mem_write(BUY, struct.pack("<4d", *rates[:4]))
        cpu.mem_write(POTION, struct.pack("<4d", *rates[4:]))
        cpu.mem_write(IDS, struct.pack("<96H", *ids))
        cpu.mem_write(GUILD + 0x40, struct.pack("<i", country))
        cpu.mem_write(GUILD + 0x48, bytes([rank]))
        cpu.mem_write(GUILD + 0x47c, struct.pack("<16H", *slots))
        cpu.mem_write(STACK + 0x8000, struct.pack("<IIIII", STOP, 0xffff0000 | npc, 0xffffff00 | shop_type, STACK + 0x100, STACK + 0x108))
        cpu.mem_write(STACK + 0x100, bytes(16))
        cpu.reg_write(UC_X86_REG_ESP, STACK + 0x8000)
        cpu.reg_write(UC_X86_REG_EAX, GUILD)
        cpu.emu_start(ENTRY, STOP, count=20000)
        if cpu.reg_read(UC_X86_REG_EIP) != STOP:
            raise RuntimeError("Rate did not return")
        result = struct.unpack("<i", bytes(cpu.reg_read(UC_X86_REG_EAX).to_bytes(4, "little")))[0]
        rate = struct.unpack("<d", bytes(cpu.mem_read(STACK + 0x100, 8)))[0]
        detail = bytes(cpu.mem_read(STACK + 0x108, 1))[0]
        after = struct.unpack("<i", bytes(cpu.mem_read(GUILD + 0x40, 4)))[0]
        levels = struct.unpack("<16H", bytes(cpu.mem_read(GUILD + 0x47c, 32)))[1::2]
        print(country, rank, npc, shop_type, *(r.hex() for r in rates), *ids, *slots, result, rate.hex(), detail, after, *levels, sep="\t")


if __name__ == "__main__":
    main()
