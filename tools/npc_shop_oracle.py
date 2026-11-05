#!/usr/bin/env python3
"""Execute the original CUser::PacketShop with observed world ports and record each outcome."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_EIP,
                               UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_FPCW)
from original_oracle import load_game

ENTRY, EXIT = 0x46af10, 0x46b6f6
STACK, USER, PACKET, STOP, ZONE, MAP, NPC, SHOP, ENTRIES, DEFS, ITEMS = (
    0x20000000, 0x30000000, 0x31000000, 0x10000000, 0x32000000, 0x33000000, 0x34000000, 0x35000000,
    0x36000000, 0x37000000, 0x38000000)
SLOT_BASE = 0x1c4
ITEM_SIZE = 0x100
CASES = 4096


def write(cpu, address, form, *values):
    cpu.mem_write(address, struct.pack(form, *values))


def read(cpu, address, form):
    return struct.unpack(form, bytes(cpu.mem_read(address, struct.calcsize(form))))[0]


class Case:
    def __init__(self, rng, index):
        self.index = index
        self.opcode = rng.choices((0x702, 0x703, 0x704), (10, 10, 1))[0]
        self.life = 1 if rng.random() < .03 else rng.choice((0, 2))
        self.bags = rng.choice((0, 1, 1, 2, 5))
        self.money = rng.choice((0, 1, 500, 1000, 100000, 0xfffffff0, 0xffffffff, rng.getrandbits(32)))
        self.stack = rng.choice((1, 1, 5, 50, 255))
        self.buy = rng.choice((0, 1, 7, 100, 3000, 0x7fffffff, rng.getrandbits(31)))
        self.sell = rng.choice((0, 1, 7, 100, 3000, 0x7fffffff, rng.getrandbits(31)))
        self.real = rng.choice((0, 3, 0x1b, 0x1b, 26))
        self.type, self.type_id = rng.randint(1, 100), rng.randint(1, 200)
        self.npc = int(rng.random() > .05)
        self.kind = 1 if rng.random() > .05 else rng.choice((0, 2))
        self.shop = int(rng.random() > .05)
        self.shop_type = rng.randint(1, 4)
        self.shop_type_id = rng.randint(1, 900)
        self.shop_items = rng.choice((0, 1, 3))
        self.definition = int(rng.random() > .05)
        self.guild_zone = int(rng.random() < .3)
        self.guild = int(rng.random() > .2)
        self.rate_result = rng.choice((0, 0, 0, 1, 2))
        self.rate = rng.choice((1.0, .95, .9, .85, .8, .75, .7, .333))
        self.detail = rng.randint(0, 255)
        self.create = int(rng.random() > .1)
        self.interaction = rng.choice((0, rng.randint(1, 900)))
        self.quality = rng.randint(0, 2000)
        self.server = rng.choice((0, 1, 1, 9))
        self.wisdom = rng.choice((0, 1, 50, 99, 100, 101))
        self.items = {}
        density = rng.choice((0, .05, .3, 1.0))
        for bag in range(1, 6):
            for slot in range(24):
                if rng.random() < density and bag <= 2:
                    same = rng.random() < .7
                    count = rng.choice((1, 2, 5, 49, 50, 200, 255))
                    self.items[bag, slot] = {
                        "same": int(same), "count": count, "uid": rng.getrandbits(63), "gems": rng.randbytes(6),
                        "made": rng.getrandbits(32), "craft": rng.choice(("", "A", "Craftname")), "quality": rng.randint(0, 2000)}
        if self.opcode == 0x702:
            shop_index, count = rng.choice((0, 1, 2, 7)), rng.choice((0, 1, 1, 2, 5, 50, 255))
            if rng.random() < .75:
                self.npc = self.kind = self.shop = self.definition = 1
                self.shop_items = max(self.shop_items, 1)
                self.bags = max(self.bags, 1)
                shop_index, count = 0, rng.randint(1, self.stack)
                self.money = rng.choice((self.money, 0xffffffff, count * self.buy & 0xffffffff))
            self.body = struct.pack("<IBB", rng.randint(1, 1 << 30), shop_index, count)
        else:
            if self.items and rng.random() < .85:
                occupied = rng.choice(sorted(self.items))
                self.bags = max(self.bags, occupied[0])
            else:
                occupied = (rng.randint(0, 6), rng.randint(0, 25))
            held = self.items.get(occupied, {}).get("count", 1)
            count = rng.choice((0, 1, held, held, max(held - 1, 0), min(held + 1, 255)))
            self.body = struct.pack("<BBB", occupied[0], occupied[1], count) + bytes(3)


def main():
    cpu, digest = load_game()
    cpu.mem_map(0, 0x10000)
    for address, size in ((STACK, 0x10000), (USER, 0x10000), (PACKET, 0x1000), (STOP, 0x1000), (ZONE, 0x1000), (MAP, 0x1000),
                          (NPC, 0x1000), (SHOP, 0x1000), (ENTRIES, 0x1000), (DEFS, 0x1000), (ITEMS, 0x100000)):
        cpu.mem_map(address, size)
    state = {}
    events = []

    def result(machine, value, cleanup=0, high=None):
        esp = machine.reg_read(UC_X86_REG_ESP)
        back = read(machine, esp, "<I")
        machine.reg_write(UC_X86_REG_EAX, value)
        if high is not None:
            machine.reg_write(UC_X86_REG_EDX, high)
        machine.reg_write(UC_X86_REG_ESP, esp + 4 + cleanup)
        machine.reg_write(UC_X86_REG_EIP, back)

    def argument(machine, index):
        return read(machine, machine.reg_read(UC_X86_REG_ESP) + 4 + 4 * index, "<I")

    def send(tag):
        def port(machine):
            size = argument(machine, 1)
            events.append(tag + bytes(machine.mem_read(argument(machine, 0), size)).hex())
            result(machine, 1, 8)
        return port

    def find_npc(machine):
        case = state["case"]
        if machine.reg_read(UC_X86_REG_EAX) != ZONE:
            raise RuntimeError("FindNpc received the wrong zone")
        result(machine, NPC if case.npc else 0, 4)

    def shop(machine):
        case = state["case"]
        if machine.reg_read(UC_X86_REG_EAX) != case.shop_type_id:
            raise RuntimeError("GetShopNpc received the wrong type")
        result(machine, SHOP if case.shop else 0)

    def item_info(machine):
        case = state["case"]
        if (machine.reg_read(UC_X86_REG_EAX), machine.reg_read(UC_X86_REG_ECX)) != (case.type, case.type_id):
            raise RuntimeError("GetItemInfo received the wrong identity")
        result(machine, DEFS if case.definition else 0, 4)

    def guild_lock(machine):
        write(machine, machine.reg_read(UC_X86_REG_ESI), "<I", 0)
        write(machine, USER + 0x1758, "<I", 0x1000 if state["case"].guild else 0)
        result(machine, machine.reg_read(UC_X86_REG_ESI))

    def rate(machine):
        case = state["case"]
        events.append(f"R{argument(machine, 0) & 0xffff}:{argument(machine, 1) & 0xff}")
        write(machine, argument(machine, 2), "<d", case.rate)
        write(machine, argument(machine, 3), "<B", case.detail)
        result(machine, case.rate_result, 16)

    def create(machine):
        case = state["case"]
        if not case.create:
            result(machine, 0)
            return
        state["allocated"] += 1
        address = ITEMS + ITEM_SIZE * (120 + state["allocated"])
        machine.mem_write(address, bytes(ITEM_SIZE))
        result(machine, address)

    def free(machine):
        item = machine.reg_read(UC_X86_REG_EDI)
        events.append(f"X{(item - ITEMS) // ITEM_SIZE}")
        result(machine, 0)

    def plain(tag):
        def port(machine):
            events.append(tag)
            result(machine, 0)
        return port

    def money(machine):
        if machine.reg_read(UC_X86_REG_EAX) != USER:
            raise RuntimeError("SendDBMoney received the wrong user")
        events.append(f"M{read(machine, USER + 0x140, '<I')}")
        result(machine, 0)

    def close(machine):
        events.append(f"F{argument(machine, 0)}")
        result(machine, 0, 8)

    ports = {
        0x416d10: find_npc, 0x466c30: shop, 0x402710: item_info, 0x44b3d0: plain("C"), 0x478520: plain("E"),
        0x42b800: guild_lock, 0x430ac0: rate, 0x40a510: create, 0x40a590: free, 0x45b0d0: lambda machine: result(machine, 0),
        0x4cb8e0: lambda machine: result(machine, 0x1234abcd),
        0x457a50: lambda machine: result(machine, 0x55667788, 0, 0x11223344),
        0x46c700: money, 0x4d4d20: send("S"), 0x4d4f10: send("D"), 0x4d4e50: send("L"), 0x4d43a0: close,
        0x505316: lambda machine: result(machine, 0)}

    def hook(machine, address, _size, _data):
        if address == EXIT:
            machine.emu_stop()
        else:
            ports[address](machine)
    for address in (*ports, EXIT):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)

    rng = random.Random(0x46af10)
    print(f"# ps_game.exe SHA256 {digest}; CUser::PacketShop 0046af10, opcodes 0702/0703; real handler, CItem::Init 004b9f80 and InitCraftExpansion 004ba280 "
          "execute, world lookups/transports are observed ports; SetGameLogMain 0045b0d0 header bytes stay zero")
    print("# opcode life bags money stack buy sell real type type_id npc kind shop shop_type shop_type_id shop_items definition "
          "guild_zone guild rate_result rate detail create interaction quality server wisdom body item_count items... events money final_count final...")
    for index in range(CASES):
        case = Case(rng, index)
        state.update(case=case, allocated=0)
        events.clear()
        cpu.mem_write(STACK, bytes(0x10000))
        cpu.mem_write(USER, bytes(0x10000))
        cpu.mem_write(PACKET, bytes(0x1000))
        cpu.mem_write(ITEMS, bytes(0x100000))
        write(cpu, USER + 0x142c, "<I", case.life)
        write(cpu, USER + 0x140, "<I", case.money)
        write(cpu, USER + 0x1304, "<i", case.bags)
        write(cpu, USER + 0x578c, "<I", 12345)
        write(cpu, USER + 0xe0, "<I", ZONE)
        write(cpu, ZONE + 0x24, "<I", MAP)
        write(cpu, MAP + 0x1a4, "<I", 6 if case.guild_zone else 0)
        write(cpu, USER + 0x1564, "<I", (NPC + 0x800) if case.interaction else 0)
        write(cpu, NPC + 0x838, "<I", case.interaction)
        write(cpu, NPC + 0x34, "<I", case.kind)
        write(cpu, NPC + 0x38, "<I", case.shop_type_id)
        write(cpu, SHOP + 2, "<H", case.shop_type_id)
        write(cpu, SHOP + 0x124, "<B", case.shop_type)
        write(cpu, SHOP + 0x128, "<i", case.shop_items)
        write(cpu, SHOP + 0x12c, "<I", ENTRIES)
        for entry in range(max(case.shop_items, 8)):
            write(cpu, ENTRIES + 2 * entry, "<BB", case.type, case.type_id)
        cpu.mem_write(DEFS, bytes(0x1000))
        write(cpu, DEFS, "<I", 50000 + case.index)
        cpu.mem_write(DEFS + 4, b"Shop item\0")
        write(cpu, DEFS + 0x24, "<BB", case.type, case.type_id)
        write(cpu, DEFS + 0x4b, "<B", case.stack)
        write(cpu, DEFS + 0x4c, "<H", case.quality)
        write(cpu, DEFS + 0x3e, "<H", case.wisdom)
        write(cpu, DEFS + 0x4a, "<B", case.server)
        write(cpu, DEFS + 0x80, "<II", case.buy, case.sell)
        write(cpu, DEFS + 0xa0, "<i", case.real)
        other = DEFS + 0x400
        write(cpu, other, "<I", 40000)
        cpu.mem_write(other + 4, b"Other item\0")
        write(cpu, other + 0x24, "<BB", (case.type % 100) + 1, case.type_id)
        write(cpu, other + 0x4b, "<B", case.stack)
        write(cpu, other + 0x84, "<I", case.sell)
        write(cpu, other + 0xa0, "<i", case.real)
        position = {}
        for number, ((bag, slot), item) in enumerate(sorted(case.items.items())):
            address = ITEMS + ITEM_SIZE * number
            position[bag, slot] = number
            write(cpu, USER + SLOT_BASE + 4 * (bag * 24 + slot), "<I", address)
            write(cpu, address + 0x30, "<I", DEFS if item["same"] else other)
            write(cpu, address + 0x38, "<Q", item["uid"])
            write(cpu, address + 0x40, "<BBB", case.type if item["same"] else (case.type % 100) + 1, case.type_id, item["count"])
            write(cpu, address + 0x44, "<H", item["quality"])
            cpu.mem_write(address + 0x46, item["gems"])
            cpu.mem_write(address + 0x4c, item["craft"].encode() + b"\0")
            write(cpu, address + 0x64, "<I", item["made"])
        write(cpu, PACKET, "<H", case.opcode)
        cpu.mem_write(PACKET + 2, case.body)
        cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
        cpu.reg_write(UC_X86_REG_ESP, STACK + 0x8000)
        write(cpu, STACK + 0x8000, "<I", STOP)
        cpu.reg_write(UC_X86_REG_ECX, USER)
        cpu.reg_write(UC_X86_REG_EDX, PACKET)
        cpu.emu_start(ENTRY, STOP, count=200000)
        if cpu.reg_read(UC_X86_REG_EIP) != EXIT:
            raise RuntimeError(f"Case {index} did not reach the handler exit")
        final = []
        for bag in range(1, 6):
            for slot in range(24):
                address = read(cpu, USER + SLOT_BASE + 4 * (bag * 24 + slot), "<I")
                if address:
                    final.append(f"{bag}:{slot}:{read(cpu, address + 0x42, '<B')}:{read(cpu, address + 0x38, '<Q')}:{read(cpu, address + 0x44, '<H')}:{read(cpu, address + 0x64, '<I')}")
        items = [f"{bag}:{slot}:{item['same']}:{item['count']}:{item['uid']}:{item['gems'].hex()}:{item['made']}:{item['craft'] or '-'}:{item['quality']}"
                 for (bag, slot), item in sorted(case.items.items())]
        fields = [case.opcode, case.life, case.bags, case.money, case.stack, case.buy, case.sell, case.real, case.type, case.type_id, case.npc,
                  case.kind, case.shop, case.shop_type, case.shop_type_id, case.shop_items, case.definition, case.guild_zone, case.guild,
                  case.rate_result, repr(case.rate), case.detail, case.create, case.interaction, case.quality, case.server, case.wisdom, case.body.hex(), len(items), *items,
                  ",".join(events) or "-", read(cpu, USER + 0x140, "<I"), len(final), *final]
        print("\t".join(str(field) for field in fields))


if __name__ == "__main__":
    main()
