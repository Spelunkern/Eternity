#!/usr/bin/env python3
"""Execute original DB effect restoration and stop at the original expiry decisions. Requires Unicorn."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    machine, digest = load_game()
    stack, user, info, node, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x10000000
    for address in (stack, user, info, node, stop):
        machine.mem_map(address, 0x10000)
    for address, cleanup in ((0x402770, 0), (0x40a780, 0), (0x486200, 0), (0x4d4f10, 8)):
        machine.mem_write(address, b"\xc2" + struct.pack("<H", cleanup))
    machine.mem_write(stop + 0x100, b"\xc3")
    machine.mem_write(stop + 0x200, b"\xc2\x04\x00")
    machine.mem_write(0x5210e4, struct.pack("<I", stop + 0x100))
    machine.mem_write(0x5210cc, struct.pack("<I", stop + 0x200))
    current = {}

    def hook(cpu, address, size, context):
        if address == 0x402770:
            cpu.reg_write(UC_X86_REG_EAX, info if current['available'] else 0)
        elif address == 0x40a780:
            cpu.reg_write(UC_X86_REG_EAX, node)
        elif address == stop + 0x100:
            cpu.reg_write(UC_X86_REG_EAX, current['tick'])
        elif address == 0x486200:
            current['disposition'] = 1
        elif address == 0x4d4f10:
            current['disposition'] = 2
        elif current.get('checking') and address in (0x486625, 0x4866f5, 0x4867d7):
            current['expired'] = int(address != 0x4867d7)
            cpu.emu_stop()

    machine.hook_add(UC_HOOK_CODE, hook)
    cases = []
    for offline in (0, 1):
        for remaining in (0, 10, 11, 100, 0xffffffff):
            for tick in (0, 999, 1000, 0xfffffe00, 0xffffffff):
                cases.append((137, 1, 4, offline, 1 if offline else 100, remaining, 1, 10, tick, 1))
    for identity in (111, 112, 114, 116, 117, 120, 121, 123, 124, 127, 175, 181, 185, 137):
        for level in (1, 2):
            cases.append((identity, level, 4, 1, 1, 100, 18, 10, 1234567, 1))
    rng = random.Random(0x487ce0)
    for _ in range(100):
        cases.append((137, 1, rng.choice((0, 3, 4, 6)), rng.randrange(2), rng.choice((0, 1, 100, 0xffffffff)),
                      rng.choice((0, 10, 11, 100, 0xffffffff)), rng.choice((1, 18)), rng.choice((0, 10, 99, 100, 0xffffffff)),
                      rng.randrange(0x100000000), rng.choice((0, 1, 1, 1))))
    print(f"# ps_game.exe SHA256 {digest}; DB AddApplySkill 00487ce0, EnableApplySkillByMap 004845f0, expiry decisions in ChkEndTimeSkill 004865b0")
    print("# id level effect offline raw_keep_time saved_remaining map disconnected_seconds tick_ms definition_present; disposition(0 ignored/1 restored/2 erase), previous_tick,end,attacker_kind; expired at tick, tick+1, tick-1")
    for identity, level, effect, offline, keep, remaining, map_id, disconnected, tick, available in cases:
        current.clear();current.update(available=available, tick=tick, disposition=0, checking=False)
        machine.mem_write(user, bytes(0x9000));machine.mem_write(node, bytes(0x100))
        machine.mem_write(user + 0x164, struct.pack("<H", map_id))
        machine.mem_write(user + 0x585c, struct.pack("<I", disconnected))
        definition = bytearray(160)
        struct.pack_into("<HB", definition, 0, identity, level)
        definition[0x31], definition[0x97] = effect, offline
        struct.pack_into("<I", definition, 0x5c, (keep * (3600 if offline else 1)) & 0xffffffff)
        machine.mem_write(info, bytes(definition))
        machine.mem_write(stack + 0x8000, struct.pack("<4I", stop, user, level, remaining))
        machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        machine.reg_write(UC_X86_REG_EAX, identity)
        machine.emu_start(0x487ce0, stop, count=20000)
        if machine.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError("Original effect restoration did not return")
        disposition = current['disposition']
        previous = end = attacker = 0
        expired = [-1, -1, -1]
        if disposition == 1:
            end, previous, attacker = struct.unpack("<3I", machine.mem_read(node + 0x20, 12))
            # Inspect the original comparison branch; stop before list removal/network/SQL/free operations.
            for i, check_tick in enumerate((tick, (tick + 1) & 0xffffffff, (tick - 1) & 0xffffffff)):
                current.update(checking=True, expired=None)
                machine.mem_write(user + 0xa94, struct.pack("<I", user + 0x8000))
                machine.mem_write(user + 0x8004, struct.pack("<I", node))
                machine.mem_write(node + 4, struct.pack("<I", user + 0x8000))
                machine.mem_write(stack + 0x8000, struct.pack("<II", stop, check_tick))
                machine.reg_write(UC_X86_REG_ESP, stack + 0x8000)
                machine.reg_write(UC_X86_REG_EAX, user)
                machine.emu_start(0x4865b0, stop, count=20000)
                if current['expired'] is None:
                    raise RuntimeError("Original effect expiry branch was not reached")
                expired[i] = current['expired']
        print(identity, level, effect, offline, keep, remaining, map_id, disconnected, tick, available,
              disposition, previous, end, attacker, *expired, sep="\t")


if __name__ == "__main__":
    main()
