#!/usr/bin/env python3
"""Execute original NPC movement, visibility changes and prioritized movement routes. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, zone, npc, map_object, grid, buffers, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x10000000
    for address in (0, stack, zone, npc, map_object, buffers, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(grid, 0x100000)
    cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    for address, cleanup in ((0x416c10, 0), (0x4208c0, 12), (0x4df9f0, 0), (0x4df070, 0), (0x4243d0, 4), (0x4246c0, 12)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    for pointer in (0x5210cc, 0x5210d0, 0x5211f0):
        cpu.mem_write(pointer, struct.pack('<I', stop + 0x100))
    cpu.mem_write(stop + 0x100, b'\xb8\x01\x00\x00\x00\xc2\x04\x00')
    context = dict(count=0, events=b'', allocations=0, fail=0, packets={})

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def cell_coords(pointer):
        index, remainder = divmod(pointer - grid - 0x1000, 292)
        if remainder or index < 0 or index >= context['count'] ** 2:
            raise RuntimeError('Invalid original movement cell pointer')
        return index % context['count'], index // context['count']

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x4df9f0:
            pointer, length = struct.unpack('<2I', machine.mem_read(sp + 4, 8));index = context['allocations'];context['allocations'] += 1
            if context['fail'] & (1 << index):
                machine.reg_write(UC_X86_REG_EAX, 0);return
            at = buffers + 256 * index;machine.mem_write(at, bytes(32));machine.mem_write(at + 8, struct.pack('<I', 1))
            context['packets'][at] = bytes(machine.mem_read(pointer, length));machine.reg_write(UC_X86_REG_EAX, at)
        elif address in (0x4243d0, 0x4246c0):
            if address == 0x4243d0:
                cell = machine.reg_read(UC_X86_REG_EDI);buffer = number(sp + 4);priority = 255
            else:
                cell = machine.reg_read(UC_X86_REG_EBX);buffer = machine.reg_read(UC_X86_REG_EDI)
                forced1, forced2, priority = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
                if (forced1, forced2) != (0, 0):
                    raise RuntimeError('Unexpected original NPC forced recipients')
            x, z = cell_coords(cell);packet = context['packets'][buffer]
            context['events'] += struct.pack('<4I', x, z, priority, len(packet)) + packet
            # Capture ordering: position/cell fields at each delivery, including
            # transition announcements before replacement position/cell assignment.
            context['events'] += bytes(machine.mem_read(npc + 8, 12)) + bytes(machine.mem_read(npc + 0x1c, 8))

    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, arguments=()):
        cpu.mem_write(stack + 0x8000, struct.pack('<' + 'I' * (len(arguments) + 1), stop, *arguments));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.emu_start(address, stop, count=200000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original NPC movement did not return')

    def initialize(extent, cell, position):
        count = extent // 40 + 1;context['count'] = count
        cpu.mem_write(zone, bytes(0x1000));cpu.mem_write(npc, bytes(152));cpu.mem_write(map_object, bytes(256))
        cpu.mem_write(map_object + 4, struct.pack('<I', extent));cpu.mem_write(zone + 0x24, struct.pack('<4I', map_object, count, count, grid))
        cpu.mem_write(grid, struct.pack('<' + 'I' * count, *(grid + 0x1000 + z * count * 292 for z in range(count))))
        cpu.mem_write(grid + 0x1000, bytes(count * count * 292))
        for z in range(count):
            for x in range(count):
                at = grid + 0x1000 + (z * count + x) * 292;head = at + 0x80
                cpu.mem_write(at + 0x58, struct.pack('<I', head));cpu.mem_write(head, struct.pack('<2I', head, head))
                cpu.mem_write(at + 0x114, struct.pack('<4f', x * 40 - 10, x * 40 + 50, z * 40 - 10, z * 40 + 50))
        # Real EnterNpc links the supplied initial cell, then stored position is
        # deliberately independent to cover the original delayed-cell behavior.
        cpu.mem_write(npc + 8, struct.pack('<3f', cell[0] * 40 + 1, 7, cell[1] * 40 + 1))
        cpu.mem_write(npc + 0x14, struct.pack('<I', 0xfedcba98));cpu.mem_write(npc + 0x34, struct.pack('<3i', 257, 70000, -3))
        cpu.reg_write(UC_X86_REG_ESI, npc);call(0x416d70, (zone,));cpu.mem_write(npc + 8, struct.pack('<3f', *position))

    def move(destination, mode, fail, detached):
        context.update(events=b'', allocations=0, fail=fail, packets={})
        if detached:
            cpu.mem_write(npc + 0x18, bytes(4))
        cpu.reg_write(UC_X86_REG_EBX, npc);call(0x416f30, (zone, *struct.unpack('<3I', struct.pack('<3f', *destination)), mode))
        result = bytes([cpu.reg_read(UC_X86_REG_EAX) & 255]) + bytes(cpu.mem_read(npc + 8, 12)) + bytes(cpu.mem_read(npc + 0x1c, 8)) + struct.pack('<I', context['allocations']) + context['events']
        # Both real list primitives must move the NPC exactly once to its reported cell.
        x, z = struct.unpack('<2I', cpu.mem_read(npc + 0x1c, 8))
        for iz in range(context['count']):
            for ix in range(context['count']):
                at = grid + 0x1000 + (iz * context['count'] + ix) * 292;head = at + 0x80
                expected = int((ix, iz) == (x, z))
                if number(at + 0x78) != expected or (number(head) == head) != (not expected):
                    raise RuntimeError('Original movement cell membership differs')
        return result

    print(f'# ps_game.exe SHA256 {digest}; MoveNpc 00416f30, SendMoveNpc 0041faa0, PSendView 00421120, EnterNpc 00416d70; real cell insert/remove and geometry.')
    print('# Transport/buffer/reference/zone-index ports isolated. M size initial_cell stored_position destination mode failure_mask detached expected_hex. Positions use float bit DWORDs. Expected: success BYTE,position,cell,allocation_count,ordered per-cell deliveries with priority (255=immediate),packet and position/cell at delivery.')
    def bits(values):
        return struct.unpack('<3I', struct.pack('<3f', *values))
    for scenario in range(1536):
        extent = (81, 160, 400)[scenario % 3];count = extent // 40 + 1
        cell = ((scenario // 3) % count, (scenario // 13) % count)
        values = (-0.25, 0, 39.99, 40, 60, 79.99, 80, 159.99, 240, 399.99, float('nan'), float('inf'))
        stored = (values[(scenario // 7) % len(values)], 7.25, values[(scenario // 11) % len(values)])
        targets = (-1, 0, 39.99, 40, cell[0] * 40 + 50, cell[0] * 40 + 50.001, extent - 0.001, extent, float('nan'))
        destination = (targets[(scenario // 5) % len(targets)], -scenario / 4, targets[(scenario // 17) % len(targets)])
        mode, fail, detached = (0, 1, 2, 255)[scenario % 4], (scenario // 19) % 8, int(scenario % 23 == 0)
        initialize(extent, cell, stored);result = move(destination, mode, fail, detached)
        print('M', extent, *cell, *bits(stored), *bits(destination), mode, fail, detached, result.hex(), sep='\t')
    # Sustained paths exercise the lag across calls with no injected stored-position edits.
    for scenario in range(12):
        extent = 400;cell = (scenario % 4, scenario // 4);stored = (cell[0] * 40 + 1, 3, cell[1] * 40 + 1);initialize(extent, cell, stored)
        print('B', extent, *cell, *bits(stored), sep='\t')
        for step in range(12):
            destination = ((step * 41 + scenario * 23) % 400 + 0.25, step - 3, (step * 71 + scenario * 17) % 400 + 0.75)
            mode, fail = step % 4, 0 if step < 8 else step % 8;result = move(destination, mode, fail, False)
            print('P', *bits(destination), mode, fail, result.hex(), sep='\t')


if __name__ == '__main__':
    main()
