#!/usr/bin/env python3
"""Execute complete original CMob::SendLogBothMob with observed transport."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x10000)
    mob, definition, zone, map_object, user, detail, stack, stop = [base + n for n in (0, 0x2000, 0x3000, 0x4000, 0x5000, 0x5100, 0xe000, 0xf000)]
    cpu.mem_write(0x4d4e50, b'\xc2\x08\x00')
    cpu.mem_write(0x505316, b'\xc3')
    state = {}

    def put(at, *values):
        cpu.mem_write(at, struct.pack('<' + 'I' * len(values), *[v & 0xffffffff for v in values]))

    def hook(machine, address, size, data):
        if address == 0x4d4e50:
            sp = machine.reg_read(UC_X86_REG_ESP)
            packet, count = struct.unpack('<II', machine.mem_read(sp + 4, 8))
            if count != 123:
                raise RuntimeError('Original mob log length changed')
            state['packets'].append(bytes(machine.mem_read(packet, count)))

    cpu.hook_add(UC_HOOK_CODE, hook, begin=0x4d4e50, end=0x4d4e50)
    rng = random.Random(0x4a1810)
    names = (b'', b'Mob', bytes(range(128, 159)), b'0123456789012345678901234567890', b'A\0ignored suffix')
    users = (None, b'', b'Player', bytes(range(160, 191)), b'User\0ignored suffix')
    details = (None, b'', b'Skill', bytes(range(192, 223)), b'Skill\0ignored suffix')
    encode = lambda value: 'N' if value is None else 'E' if not value else value.hex()
    print(f'# ps_game.exe SHA256 {digest}; complete CMob SendLogBothMob 004a1810; only game-log transport and security-cookie ports. Initialized stack storage exposes preserved C-string tails. All packet bytes compared.')
    print('# scenario scene map definition event value hp position_bits mob_name user_name detail seed expected_packet_or_N zero_packet_or_N')
    for scenario in range(512):
        scene = scenario % 5 != 0
        map_id = (0, 1, 65535, 0x1234)[scenario % 4]
        identity = (0, 1, 65535, 0xabcd)[(scenario // 3) % 4]
        event = (0, 1, 4, 6, 255)[(scenario // 7) % 5]
        value = (0, 1, 0xffffffff, 0x80000000)[(scenario // 11) % 4]
        hp = (0, 1, 0xffffffff, 0x80000000)[(scenario // 13) % 4]
        position = tuple((0, 0x80000000, 0x3f800000, 0x7f800000, 0x7fc12345, rng.randrange(1 << 32))[(scenario + i * 3) % 6] for i in range(3))
        name = names[(scenario // 2) % len(names)]
        user_name = users[(scenario // 17) % len(users)]
        description = details[(scenario // 19) % len(details)]
        seed = bytes(rng.randrange(256) for _ in range(123))
        cpu.mem_write(base, bytes(0xd000))
        put(mob + 0x50, *position)
        put(mob + 0x60, zone if scene else 0)
        put(mob + 0xa8, definition)
        put(mob + 0x224, hp)
        put(zone + 0x24, map_object)
        cpu.mem_write(map_object + 0x78, struct.pack('<H', map_id))
        cpu.mem_write(definition, struct.pack('<H', identity) + name + b'\0')
        if user_name is not None:
            cpu.mem_write(user, user_name + b'\0')
        if description is not None:
            cpu.mem_write(detail, description + b'\0')
        results = []
        for initialized in (seed, bytes(123)):
            cpu.mem_write(stack - 0x80, initialized + bytes(5))
            put(stack, stop, event, detail if description is not None else 0, value)
            cpu.reg_write(UC_X86_REG_ESP, stack)
            cpu.reg_write(UC_X86_REG_EDX, mob)
            cpu.reg_write(UC_X86_REG_EDI, user if user_name is not None else 0)
            state['packets'] = []
            cpu.emu_start(0x4a1810, stop, count=10000)
            if cpu.reg_read(UC_X86_REG_EIP) != stop or len(state['packets']) != int(scene):
                raise RuntimeError('Original mob log did not return/deliver as expected')
            results.append(state['packets'][0].hex() if scene else 'N')
        print(scenario, int(scene), map_id, identity, event, value, hp, struct.pack('<III', *position).hex(), encode(name), encode(user_name), encode(description), seed.hex(), *results, sep='\t')


if __name__ == '__main__':
    main()
