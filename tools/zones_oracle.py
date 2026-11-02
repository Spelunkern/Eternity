#!/usr/bin/env python3
"""Execute original zone startup/selection with isolated object/allocator/index/queue ports. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, world, map_ptr, user, guild, party, heap, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x40000000, 0x10000000
    for address in (0, stack, world, map_ptr, user, guild, party, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(heap, 0x1000000)
    ports = ((0x503ae3, 0), (0x503a3f, 0), (0x415410, 4), (0x414b10, 4), (0x414bc0, 0),
             (0x414b80, 8), (0x414c80, 0), (0x407ff0, 0), (0x41c8a0, 8), (0x41c5f0, 0),
             (0x41c3b0, 4), (0x406880, 4), (0x415660, 4), (0x503ba1, 0))
    for address, cleanup in ports:
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    for offset, iat, cleanup in ((0x100, 0x5210cc, 4), (0x110, 0x5210d0, 4), (0x120, 0x5210c8, 8),
                                (0x130, 0x5210f8, 4), (0x140, 0x5210dc, 4)):
        cpu.mem_write(stop + offset, b'\xc2' + struct.pack('<H', cleanup));cpu.mem_write(iat, struct.pack('<I', stop + offset))
    context = {}

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def allocate(amount):
        pointer = context['allocation'];context['allocation'] += (max(amount, 1) + 15) & ~15
        if context['allocation'] > heap + 0x1000000:
            raise RuntimeError('Zone oracle allocation limit')
        cpu.mem_write(pointer, bytes(amount));return pointer

    def new_zone():
        return allocate(0x2a4)

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503ae3:
            machine.reg_write(UC_X86_REG_EAX, allocate(number(sp + 4)))
        elif address == 0x503a3f:
            machine.reg_write(UC_X86_REG_EAX, 0 if context['failure'] == 1 else new_zone())
        elif address == 0x415410:
            machine.reg_write(UC_X86_REG_EAX, number(sp + 4))
        elif address == 0x414c80:
            pool = context['pools'].setdefault(machine.reg_read(UC_X86_REG_ESI), [])
            pointer = pool.pop() if pool else 0 if context['failure'] == 6 else new_zone()
            machine.reg_write(UC_X86_REG_EAX, pointer)
        elif address == 0x407ff0:
            context['pools'].setdefault(machine.reg_read(UC_X86_REG_ESI), []).append(machine.reg_read(UC_X86_REG_EDI))
        elif address == 0x414bc0:
            key = number(machine.reg_read(UC_X86_REG_EBX));index = machine.reg_read(UC_X86_REG_ESI)
            machine.reg_write(UC_X86_REG_EAX, context['owners'].get((index, key), 0))
        elif address == 0x414b80:
            index = machine.reg_read(UC_X86_REG_ESI);key, pointer = number(number(sp + 4)), number(sp + 8)
            context['owners'][index, key] = pointer
        elif address == 0x41c8a0:
            zone, definition = number(sp + 4), number(sp + 8)
            if definition != map_ptr:
                raise RuntimeError('Unexpected original map binding')
            machine.mem_write(zone + 0x24, struct.pack('<I', definition));context['events'].append('M' + str(number(zone + 0x18)))
            machine.reg_write(UC_X86_REG_EAX, int(context['failure'] not in (2, 4)))
        elif address == 0x41c5f0:
            zone = machine.reg_read(UC_X86_REG_EDI);context['events'].append('O' + str(number(zone + 0x18)))
            machine.mem_write(zone + 0x20, struct.pack('<I', 12345)) # Stimulate an object-port write to verify later expiry clearing.
            machine.reg_write(UC_X86_REG_EAX, int(context['failure'] not in (3, 5)))
        elif address == 0x41c3b0:
            context['events'].append('R' + str(number(number(sp + 4) + 0x18)))
        elif address == 0x406880:
            pointer = number(sp + 4)
            context['events'].append('Q' + ':'.join(str(number(pointer + offset)) for offset in (0x18, 0x1c, 0x20)))
        elif address == 0x415660:
            context['events'].append('D' + str(number(number(sp + 4) + 0x18)))
        elif address == stop + 0x130:
            pointer = number(sp + 4);value = (number(pointer) + 1) & 0xffffffff
            machine.mem_write(pointer, struct.pack('<I', value));machine.reg_write(UC_X86_REG_EAX, value)
        elif address == stop + 0x140:
            pointer = number(sp + 4);month, day, weekday, hour = context['clock']
            machine.mem_write(pointer, struct.pack('<8H', 2026, month, weekday, day, hour, 0, 0, 0));context['events'].append('T')

    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, arguments):
        cpu.mem_write(stack + 0x8000, struct.pack('<' + 'I' * (len(arguments) + 1), stop, *arguments))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.emu_start(address, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original zone operation did not return')
        return cpu.reg_read(UC_X86_REG_EAX)

    stats_offsets = (0x31c, 0x320, 0x324, 0x328, 0x32c, 0x330, 0x334)
    cases = [(mode, flags, 0) for mode in range(8) for flags in range(128)]
    cases += [(mode, 31, failure) for mode in (0, 3) for failure in (1, 2, 3)]
    cases += [(mode, 31, failure) for mode in range(1, 7) for failure in (4, 5, 6)]
    cases += [(5, 147, 0), (3, 283, 0), (4, 283, 0), (5, 283, 0), (6, 315, 0)]
    print(f'# ps_game.exe SHA256 {digest}; InitZones 00414870, ChkZoneID 004146f0, GetZoneByMapID 00413df0. Object/allocator/index/queue ports isolated; original list, locks and access predicates execute.')
    print('# mode flags failure; boot_success serial statistics[house,rank,house_peak,rank_peak,party,party_peak,total]; 3 results(present,id,owner,expiry); event_count; ordered M map-init/O objects/R release/Q queue/T local-time/D destroy events')
    for mode, flags, failure in cases:
        context.update(allocation=heap, pools={}, owners={}, events=[], failure=failure if failure < 4 else 0,
                       clock=(11, 1, 2 if flags & 32 else 1, 21))
        for address in (world, map_ptr, user, guild, party):
            cpu.mem_write(address, bytes(0x10000))
        seed = 0xffffffff if flags & 64 else 0
        cpu.mem_write(world, struct.pack('<4I', seed, 0, 1, map_ptr))
        for offset in stats_offsets:
            cpu.mem_write(world + offset, struct.pack('<I', seed))
        cpu.mem_write(map_ptr + 0x1a4, struct.pack('<I', mode))
        cpu.mem_write(map_ptr + 0x1a8, struct.pack('<21H', 1, 21, 1, *([65535, 0, 0] * 6)))
        permission = (0, 10, 11, 255)[flags & 3]
        special = 77 if flags & 4 else 0
        party_id = (0 if flags & 256 else 42) if flags & 8 else None
        guild_id = (0 if flags & 256 else 91) if flags & 16 else None
        last_entry = 0x1501010b if flags & (32 | 128) else 0
        cpu.mem_write(user + 0x5768, bytes([permission]));cpu.mem_write(user + 0x54c8, struct.pack('<I', special))
        cpu.mem_write(user + 0x54c4, struct.pack('<I', last_entry))
        cpu.mem_write(user + 0x173c, struct.pack('<I', party if party_id is not None else 0));cpu.mem_write(party + 8, struct.pack('<I', party_id or 0))
        cpu.mem_write(user + 0x1758, struct.pack('<I', guild if guild_id is not None else 0));cpu.mem_write(guild + 8, struct.pack('<I', guild_id or 0))
        cpu.mem_write(guild + 0x48, bytes([30 if flags & 32 else 31]));cpu.mem_write(guild + 0x54, bytes([1, 1]))
        boot = call(0x414870, (world,)) & 255
        results = []
        if boot:
            context['failure'] = failure
            for query in range(3):
                if query == 2:
                    cpu.mem_write(user + 0x54c8, struct.pack('<I', special + 1 if special else 0))
                    cpu.mem_write(party + 8, struct.pack('<I', (party_id or 0) + 1));cpu.mem_write(guild + 8, struct.pack('<I', (guild_id or 0) + 1))
                pointer = call(0x413df0, (world, 0, user))
                results.extend((1, *[number(pointer + offset) for offset in (0x18, 0x1c, 0x20)]) if pointer else (0, 0, 0, 0))
        else:
            results = [0] * 12
        print(mode, flags, failure, boot, number(world), *[number(world + offset) for offset in stats_offsets],
              *results, len(context['events']), *context['events'], sep='\t')
    schedules = [[], [(1, 21, 1)], [(6, 23, 2)], [(0, 0, 0), (2, 10, 2)],
                 [(65535, 0, 0), (1, 21, 1)], [(1, 21, 1), (2, 21, 1), (3, 21, 1)],
                 [(1, 65535, 65535)], [(1, 21, 0)]]
    for schedule in schedules:
        entries = schedule + [(65535, 0, 0)] * (7 - len(schedule))
        cpu.mem_write(map_ptr + 0x1a8, struct.pack('<21H', *[v for entry in entries for v in entry]))
        for month in (1, 11):
            for day in (1, 31):
                for weekday in range(7):
                    for hour in (0, 1, 9, 10, 11, 12, 20, 21, 22, 23):
                        context.update(clock=(month, day, weekday, hour), events=[])
                        cpu.mem_write(stop + 0x2000, bytes(4))
                        opened = call(0x428d20, (map_ptr, stop + 0x2000))
                        print('W', *[v for entry in entries for v in entry], month, day, weekday, hour,
                              opened, number(stop + 0x2000) if opened else 0, sep='\t')
    for rank in (0, 30, 31, 255):
        for acquired in (0, 1, 2):
            for enabled in (0, 1, 255):
                cpu.mem_write(guild + 0x48, bytes([rank]));cpu.mem_write(guild + 0x54, bytes([acquired, enabled]))
                cpu.reg_write(UC_X86_REG_EAX, guild)
                print('H', rank, acquired, enabled, call(0x42f0c0, ()), sep='\t')


if __name__ == '__main__':
    main()
