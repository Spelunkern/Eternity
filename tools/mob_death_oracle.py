#!/usr/bin/env python3
"""Execute original CMob::Death, damage selection/reset and experience arithmetic."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x100000)
    stop = base + 0xff000
    mob, zone, respawn, definition, map_object = [base + x for x in (0, 0x2000, 0x3000, 0x4000, 0x5000)]
    party, head = base + 0x6000, base + 0x7000
    users = [base + 0x10000 + i * 0x6000 for i in range(31)]
    context = {}
    original_rates = bytes(cpu.mem_read(0x5423a8, 39 * 8))

    def put(at, *values):
        cpu.mem_write(at, struct.pack('<' + 'I' * len(values), *[v & 0xffffffff for v in values]))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    # Actual selection/reset, GetExp, GetPartyExpByLevel and floating-point CRT
    # remain intact. World services, reward owners and locks are observed ports.
    ports = {0x421120: 24, 0x415f30: 4, 0x458240: 12, 0x4a5ea0: 12,
             0x480140: 4, 0x45de60: 4, 0x42a410: 16, 0x41bad0: 4,
             0x41d700: 0, 0x41d730: 0, 0x4a1810: 12, 0x4a0990: 0,
             stop + 16: 4, stop + 32: 4}
    for at, cleanup in ports.items():
        cpu.mem_write(at, b'\xc2' + struct.pack('<H', cleanup))
    put(0x5210cc, stop + 16)
    put(0x5210d0, stop + 32)

    def event(tag, *values):
        context['events'] += tag.encode() + struct.pack('<' + 'I' * len(values), *[v & 0xffffffff for v in values])

    def identity(address):
        return users.index(address) + 100 if address in users else 0

    def hook(machine, address, size, data):
        sp = machine.reg_read(UC_X86_REG_ESP)
        eax = machine.reg_read(UC_X86_REG_EAX)
        if address == 0x421120:
            event('V', get(sp + 12), get(sp + 24))
            context['events'] += bytes(machine.mem_read(get(sp + 8), get(sp + 12)))
        elif address == 0x415f30:
            event('F', get(sp + 4))
            index = get(sp + 4) - 100
            machine.reg_write(UC_X86_REG_EAX, users[index] if context['found'] and 0 <= index < len(users) else 0)
        elif address == 0x458240:
            event('E', identity(eax), get(sp + 4), get(sp + 8), get(sp + 12))
            if context['updates']:
                cpu.mem_write(eax + 0x136, struct.pack('<H', 60))
                put(party + 0x10c, 60)
                cpu.mem_write(party + 0x114, bytes([1 - (cpu.mem_read(party + 0x114, 1)[0] & 1)]))
        elif address == 0x4a5ea0:
            event('D', identity(get(sp + 8)), int(get(sp + 12) != 0))
        elif address == 0x480140:
            event('Q', identity(eax))
        elif address == 0x45de60:
            index = users.index(machine.reg_read(UC_X86_REG_ECX))
            event('R', identity(eax), index + 100, get(sp + 4))
            machine.reg_write(UC_X86_REG_EAX, int((context['eligible'] >> index) & 1))
        elif address == 0x42a410:
            event('O', get(sp + 8), get(sp + 12), get(sp + 16))
        elif address == 0x41bad0:
            event('B', machine.reg_read(UC_X86_REG_EDX))
        elif address == 0x41d700:
            event('o')
        elif address == 0x41d730:
            event('b')
        elif address == 0x4a1810:
            event('L', get(sp + 4), int(machine.reg_read(UC_X86_REG_EDI) != 0), get(sp + 12))
        elif address == 0x4a0990:
            if get(mob + 0x1f4) != head or get(head + 4) != head or cpu.mem_read(mob + 0x1f0, 1) != b'\x01':
                raise RuntimeError('Skills cleared before real damage reset')
            event('C')
        elif address in (stop + 16, stop + 32) and get(sp + 4) == party + 0x118:
            event('[' if address == stop + 16 else ']')

    cpu.hook_add(UC_HOOK_CODE, hook)
    print(f'# ps_game.exe SHA256 {digest}; complete CMob Death 004910f0, actual damage selection/reset, GetExp 00491690 and GetPartyExpByLevel 00444e40. Ports: transport, FindUser, range, experience delivery, drops, quests, obelisk/linked owners, logs, skill cleanup, locks. Deterministic explicit party globals.')
    print('# S scenario branch fair credit found scene lua party_kind party_count party_level eligible mob_level base_exp admin active multiplier_bits obelisk_index obelisk_country rate_override reward_updates events current deadline free_count')
    print('# N type id damage party')
    for scenario in range(1536):
        cpu.mem_write(base, bytes(0xfd000))
        branch = scenario % 7
        fair = (scenario // 7) % 2
        credit = (scenario // 14) % 5
        found = (scenario // 70) % 2
        scene = 0 if credit in (0, 3) and scenario % 11 == 0 and branch == 0 else 1
        lua = (scenario // 9) % 2
        kind = (-1, 0, 1, 2)[(scenario // 5) % 4]
        count = (0, 1, 3, 7, 16, 31)[(scenario // 13) % 6]
        party_level = (1, 8, 16, 32, 48, 64, 0x80000000)[(scenario // 17) % 7]
        eligible = (0, 1, 0x55555555, 0xffffffff)[(scenario // 19) % 4]
        level = (1, 8, 30, 60, 65535)[(scenario // 23) % 5]
        exp = (0, 1, 17, 704, 65535)[(scenario // 29) % 5]
        admin, active = (scenario // 31) % 3, (scenario // 37) % 2
        multiplier = (0.5, 1.0, 1.2, 2.0, 1e20)[(scenario // 41) % 5]
        obelisk_index = (-1, 0, 1, 2)[(scenario // 43) % 4]
        country = (scenario // 47) % 4
        rate_override = (scenario // 53) % 2
        updates = (scenario // 59) % 2
        records = [] if credit == 0 else [(1 if credit in (1, 2, 4) else 2, 100, 41, 7)]
        if credit == 2:
            records += [(5, 7, 1000, 7), (1, 101, 57, 7)]
        if credit == 4:
            records += [(1, 101, 41, 7)]  # first equal winner is retained
        put(mob + 0x5c, 77, zone if scene else 0, 2, 3)
        put(mob + 0xa8, definition)
        put(mob + 0x284, lua)
        put(mob + 0xc84, respawn if branch else 0)
        put(zone + 0x24, map_object)
        put(map_object + 0x1e8, 1, base + 0x8000)
        for i in range(2):
            put(base + 0x8000 + i * 112 + 4, country)
        put(respawn, obelisk_index if branch in (1, 6) else 0, 3, 3, 123)
        cpu.mem_write(respawn + 0x14, bytes([branch in (1, 6)]))
        put(respawn + 0x18, base + 0xa000 if branch == 2 else 0)
        cpu.mem_write(respawn + 0x1c, bytes([branch in (2, 3, 6)]))
        put(respawn + 0x20, base + 0xb000 if branch in (2, 4) else 0)
        cpu.mem_write(mob + 0x1f0, bytes([fair]))
        put(mob + 0x1f4, head, head)
        nodes = [base + 0x9000 + 32 * i for i in range(len(records))]
        put(head, nodes[-1] if nodes else head, nodes[0] if nodes else head)
        for i, (record_type, identity_id, damage, group) in enumerate(records):
            node = nodes[i]
            put(node, nodes[i - 1] if i else head, nodes[i + 1] if i + 1 < len(nodes) else head)
            cpu.mem_write(node + 8, bytes([record_type]))
            put(node + 12, identity_id, damage, 0, group)
        put(0x3ed353c, 0, 0, 0)
        cpu.mem_write(definition + 0x22, struct.pack('<HH', level, exp))
        put(0x546f60, admin)
        put(0x546f5c, active)
        cpu.mem_write(0x541b00, struct.pack('<d', multiplier))
        # Tables overlap in the original image (8-entry normal table followed
        # by raid data). Use one explicit 39-double backing region.
        cpu.mem_write(0x5423a8, original_rates)
        if rate_override:
            for i in range(39):
                cpu.mem_write(0x5423a8 + i * 8, struct.pack('<d', (i + 1) / 10))
        put(party + 0x10, count)
        put(party + 0x10c, party_level)
        cpu.mem_write(party + 0x114, bytes([max(kind, 0)]))
        for i, user in enumerate(users):
            put(party + 0x18 + i * 8, user)
            cpu.mem_write(user + 0x136, struct.pack('<H', (i * 7 + 1) & 65535))
            cpu.mem_write(user + 0x12d, bytes([i % 3]))
            put(user + 0x173c, party if kind >= 0 else 0)
        context.update(events=b'', found=found, eligible=eligible, updates=updates)
        put(base + 0xfe000, stop, 2, 0xfedcba98)
        cpu.reg_write(UC_X86_REG_ESP, base + 0xfe000)
        cpu.reg_write(UC_X86_REG_ECX, mob)
        cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
        cpu.emu_start(0x4910f0, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Death did not return')
        print('S', scenario, branch, fair, credit, found, scene, lua, kind, count, party_level, eligible,
              level, exp, admin, active, struct.pack('<d', multiplier).hex(), obelisk_index, country, rate_override, updates,
              context['events'].hex(), get(respawn + 8), get(respawn + 12), get(0x3ed353c + 8), sep='\t')
        for record in records:
            print('N', *record, sep='\t')


if __name__ == '__main__':
    main()
