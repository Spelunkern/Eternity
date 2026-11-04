#!/usr/bin/env python3
"""Execute six original caller slices with real DamageParty and Damage bodies."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

# Ranges end immediately after the conditional stop/death sequence. Earlier
# attack validation/calculation and later packets/secondary guards are excluded.
SLICES = ((0x44cf04, 0x44cf61), (0x44ff7c, 0x44ffe8),
          (0x45009a, 0x450108), (0x4501a2, 0x450223),
          (0x450359, 0x4503c1), (0x45045b, 0x4504d6))


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x30000)
    mob, user, skill, party, definition, stack, port = [base + x for x in (0, 0x2000, 0x8000, 0x9000, 0xa000, 0x18000, 0x28000)]

    def put(at, *numbers):
        cpu.mem_write(at, struct.pack('<'+'I'*len(numbers), *[v & 0xffffffff for v in numbers]))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    def snapshot(tag, *args):
        mp, sp = struct.unpack('<HH', cpu.mem_read(mob+0x228, 4))
        fields = [get(mob+0x224), mp, sp, len(records), *args]
        return tag+':'+','.join(map(str, fields))

    def hook(c, at, size, data):
        pop, result = 0, None
        if at == 0x41d760:
            result = get(mob+0x124)
        elif at == 0x4925f0:
            events.append(snapshot('E')); pop = 4
        elif at == port:
            result = 123
        elif at == 0x4b8dd0:
            esp = c.reg_read(UC_X86_REG_ESP)
            record = [c.reg_read(UC_X86_REG_EBX)] + [get(esp+i) for i in (4, 8, 12, 16)]
            events.append(snapshot('A', *record))
            for old in records:
                if old[:2] == record[:2]:
                    old[2] = (old[2]+record[2]) & 0xffffffff
                    if old[0] != 5:
                        old[3] = (old[3]+record[3]) & 0xffffffff
                    old[4] = record[4]
                    break
            else:
                records.append(record)
            pop = 16
        elif at in (0x490ab0, 0x4b7490):
            events.append(snapshot('R' if at == 0x490ab0 else 'X'))
        elif at == 0x489ed0:
            events.append(snapshot('S', get(user+0xdc)))
            if mutate:
                put(user+0xdc, 99)
        elif at == 0x4910f0:
            esp = c.reg_read(UC_X86_REG_ESP)
            events.append(snapshot('D', get(esp+4), get(esp+8)))
            pop = 8
        else:
            return
        esp = c.reg_read(UC_X86_REG_ESP)
        ret = get(esp)
        c.reg_write(UC_X86_REG_ESP, esp+4+pop)
        if result is not None:
            c.reg_write(UC_X86_REG_EAX, result)
        c.reg_write(UC_X86_REG_EIP, ret)

    cpu.hook_add(UC_HOOK_CODE, hook)
    put(0x5210e4, port)
    rng = random.Random(0x44cf19)
    print(f'# ps_game.exe SHA256 {digest}; CUser::AttackMob 0044ce10/0044fd90, six caller slices; real DamageParty/Damage, observed Add/status/FSM/clock/stop/death ports')
    print('# path status hp mp sp has_party party_id user_aggro_bonus skill_base_aggro primary_hp primary_mp primary_sp secondary_hp secondary_mp secondary_sp mutate_stop result hp mp sp state tick user_id events records')
    for case in range(768):
        path = case % 6
        status = rng.choice((0, 1, 2))
        hp, mp, sp = rng.choice((0, 1, 100, 1000)), rng.randrange(80), rng.randrange(80)
        has_party, party_id = rng.randrange(2), rng.choice((0, 7, 77))
        aggro, bonus = rng.choice((0, 9, 0xffffffff)), rng.choice((0, 3, 65535))
        primary = [rng.choice((0, 1, 99, 100, 65535, 0xffffffff)) for _ in range(3)]
        secondary = [rng.randrange(200) for _ in range(3)]
        mutate = (case // 6) % 2
        cpu.mem_write(mob, b'\0'*0x1000)
        cpu.mem_write(stack, b'\0'*0x100)
        put(mob+0x124, status)
        put(mob+0x224, hp)
        cpu.mem_write(mob+0x228, struct.pack('<HH', mp, sp))
        put(mob+0xa8, definition)
        put(definition+0x6c, 0)
        put(user+0xdc, 42)
        put(user+0x1230, aggro)
        put(user+0x173c, party if has_party else 0)
        put(party+8, party_id)
        cpu.mem_write(skill+0x64, struct.pack('<H', bonus))
        cpu.mem_write(skill+0x76, struct.pack('<HHH', *secondary))
        # Normal attack's primary lives at +14; physical/direct skill +18;
        # alternate skill +24. Secondary slices consume the OTHER vector.
        primary_offset = 0x14 if path == 0 else (0x24 if path in (4, 5) else 0x18)
        put(stack+primary_offset, *primary)
        if path != 0:
            put(stack+0x14, 0)  # caller's default party identity
        events, records = [], []
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.reg_write(UC_X86_REG_EDI, user)
        cpu.reg_write(UC_X86_REG_EBP, mob if path == 0 else skill)
        cpu.reg_write(UC_X86_REG_EBX, 0 if path == 0 else mob)
        cpu.reg_write(UC_X86_REG_EAX, secondary[0])
        start, stop = SLICES[path]
        cpu.emu_start(start, stop, count=30000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop or cpu.reg_read(UC_X86_REG_ESP) != stack:
            raise RuntimeError('caller slice did not finish with balanced stack')
        killed = bool(cpu.reg_read(UC_X86_REG_EBX)&255) if path == 0 else bool(cpu.mem_read(stack+0x12, 1)[0])
        out_mp, out_sp = struct.unpack('<HH', cpu.mem_read(mob+0x228, 4))
        encoded = '/'.join(','.join(map(str, r)) for r in records) or '-'
        print(path, status, hp, mp, sp, has_party, party_id, aggro, bonus, *primary, *secondary, mutate,
              int(killed), get(mob+0x224), out_mp, out_sp, get(mob+0x124), get(mob+0xcb0),
              get(user+0xdc), '/'.join(events) or '-', encoded, sep='\t')


if __name__ == '__main__':
    main()
