#!/usr/bin/env python3
"""Execute original map NPC initialization, entry and leave. Requires Unicorn."""
import hashlib
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game, ROOT


def main():
    cpu, digest = load_game()
    stack, obj, zone, file, pool, grid, heap, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x40000000, 0x10000000
    for address in (0, stack, obj, zone, file, pool, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(grid, 0x1000000);cpu.mem_map(heap, 0x4000000);cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    for address, cleanup in ((0x503ae3, 0), (0x510785, 0), (0x505afd, 0), (0x50c649, 0), (0x40acc0, 0), (0x4d9510, 0),
                             (0x416c10, 0), (0x416ca0, 0), (0x4208c0, 12)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    for pointer in (0x5210cc, 0x5210d0):
        cpu.mem_write(pointer, struct.pack('<I', stop + 0x100));cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    cpu.mem_write(0x5210f8, struct.pack('<I', stop + 0x200));cpu.mem_write(stop + 0x200, b'\xc2\x04\x00')
    context = dict(data=b'', position=0, allocation=heap, attempts=0, guards=0, events=b'', npcs=[], synthetic=False)

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503ae3:
            amount = number(sp + 4);pointer = context['allocation'];context['allocation'] += (max(amount, 1) + 15) & ~15
            if context['allocation'] > heap + 0x4000000:
                raise RuntimeError('NPC runtime oracle allocation limit')
            machine.mem_write(pointer, bytes(amount));machine.reg_write(UC_X86_REG_EAX, pointer)
        elif address == 0x505afd:
            machine.reg_write(UC_X86_REG_EAX, 12)
        elif address == 0x50c649:
            machine.reg_write(UC_X86_REG_EAX, 0)
        elif address == 0x510785:
            fd, pointer, amount = struct.unpack('<3I', machine.mem_read(sp + 4, 12));data = context['data'][context['position']:context['position'] + amount]
            if fd != 12 or len(data) != amount:
                raise RuntimeError('Original NPC runtime input changed')
            machine.mem_write(pointer, data);context['position'] += amount;machine.reg_write(UC_X86_REG_EAX, amount)
        elif address == 0x40acc0:
            attempt = context['attempts'];context['attempts'] += 1
            if context['synthetic'] and attempt % 5 == 4:
                machine.reg_write(UC_X86_REG_EAX, 0);return
            at = heap + 0x3000000 + 160 * attempt;machine.mem_write(at, bytes(152))
            machine.mem_write(at + 0x14, struct.pack('<I', 70000 + attempt if context['synthetic'] and attempt % 3 == 1 else 0))
            machine.mem_write(at + 0x68, struct.pack('<I', 0x12345678 if context['synthetic'] else 0))
            context['npcs'].append(at);machine.reg_write(UC_X86_REG_EAX, at)
        elif address == stop + 0x200:
            pointer = number(sp + 4);value = (number(pointer) + 1) & 0xffffffff;machine.mem_write(pointer, struct.pack('<I', value));machine.reg_write(UC_X86_REG_EAX, value)
        elif address == 0x4d9510:
            context['guards'] += 1
        elif address == 0x4208c0:
            actual_zone, pointer, length = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
            if actual_zone != zone:
                raise RuntimeError('Unexpected NPC zone route')
            context['events'] += struct.pack('<3I', machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX), length) + bytes(machine.mem_read(pointer, length))

    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, arguments=()):
        cpu.mem_write(stack + 0x8000, struct.pack('<' + 'I' * (len(arguments) + 1), stop, *arguments));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.emu_start(address, stop, count=15000000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original NPC runtime operation did not return')

    original = (ROOT / '.local/server-data/NpcQuest.SData').read_bytes();context['data'] = original;cpu.mem_write(file, struct.pack('<I', 12))
    cpu.reg_write(UC_X86_REG_ECX, 0x3ed3560);call(0x4c9410, (file,));definitions_end = context['allocation']
    if cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
        raise RuntimeError('Original NPC definitions failed')

    def prepare(data, synthetic):
        context.update(data=data, position=0, allocation=definitions_end, attempts=0, guards=0, events=b'', npcs=[], synthetic=synthetic)
        cpu.mem_write(obj, bytes(0x1000));cpu.mem_write(zone, bytes(0x1000));cpu.mem_write(pool, bytes(0x1000));cpu.mem_write(0x546f3c, struct.pack('<I', pool))
        cpu.mem_write(pool + 12, struct.pack('<I', 0xfffffffc if synthetic else 100))
        cpu.reg_write(UC_X86_REG_EDI, obj);cpu.reg_write(UC_X86_REG_ECX, file);call(0x425780)
        extent = number(obj + 4);count = extent // 40 + 1
        cpu.mem_write(zone + 0x28, struct.pack('<3I', count, count, grid));cpu.mem_write(zone + 0x24, struct.pack('<I', obj))
        at = grid + 0x1000;cpu.mem_write(grid, struct.pack('<' + 'I' * count, *(at + z * count * 292 for z in range(count))))
        cpu.mem_write(at, bytes(count * count * 292))
        for z in range(count):
            for x in range(count):
                cell = at + (z * count + x) * 292;head = cell + 0x80
                cpu.mem_write(cell + 0x58, struct.pack('<I', head));cpu.mem_write(head, struct.pack('<2I', head, head))
        cpu.reg_write(UC_X86_REG_EDI, obj);call(0x429130, (zone,))
        records = struct.pack('<4I', context['attempts'], context['guards'], number(pool + 12), len(context['npcs']))
        for at in context['npcs']:
            source_index = (number(at + 0x54) - number(obj + 0x44)) // 20
            records += struct.pack('<I', source_index) + bytes(cpu.mem_read(at + 8, 16)) + bytes(cpu.mem_read(at + 0x34, 12)) + bytes(cpu.mem_read(at + 0x68, 4)) + bytes(cpu.mem_read(at + 0x1c, 8))
        records += struct.pack('<I', len(context['events'])) + context['events']
        # Leave in reverse creation order, validating the real intrusive cell list too.
        context['events'] = b''
        for at in reversed(context['npcs']):
            cpu.reg_write(UC_X86_REG_EAX, zone);cpu.reg_write(UC_X86_REG_ESI, at);call(0x416e90)
            if number(at + 0x18) or number(at + 0x1c) or number(at + 0x20):
                raise RuntimeError('Original NPC leave did not clear ownership')
        records += struct.pack('<I', len(context['events'])) + context['events']
        for z in range(count):
            for x in range(count):
                cell = grid + 0x1000 + (z * count + x) * 292;head = cell + 0x80
                if number(cell + 0x78) != 0 or number(head) != head or number(head + 4) != head:
                    raise RuntimeError('Original NPC cell list was not emptied')
        return records

    print(f'# ps_game.exe SHA256 {digest}; InitNPC 00429130, EnterNpc 00416d70, LeaveNpc 00416e90. Definition SHA256 {hashlib.sha256(original).hexdigest()}.')
    print('# N minimal positional NPC definition fixture. File/allocation/log/atomic-ID/zone-index/transport ports isolated; original NPC lookup, position checks, tile bits and cell insertion/removal execute. A filename map_SHA256 canonical_bytes FNV1a64. S input_hex canonical_hex.')
    minimal = b''
    for group in range(13):
        count = int(number(0x3ed3560 + 8 * group) > 0);minimal += struct.pack('<I', count)
        if count:
            minimal += struct.pack('<BH', group + 1, 1) + (b'\0' if group == 0 else b'') + bytes(16) + bytes(8)
            if group == 0:
                minimal += bytes(4)
            elif group == 1:
                minimal += bytes(3 * 22)
            minimal += bytes(8)
    print('N', minimal.hex(), sep='\t')
    for path in sorted((ROOT / '.local/server-data/Map').glob('*.svmap'), key=lambda path: int(path.stem)):
        data = path.read_bytes();records = prepare(data, False);value = 0xcbf29ce484222325
        for byte in records:
            value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        print('A', path.name, hashlib.sha256(data).hexdigest(), len(records), value, sep='\t')
    for scenario in range(4):
        extent = (40, 81, 160, 320)[scenario];tiles = bytearray((extent * extent + 7) // 8);tiles[0] = 1
        data = struct.pack('<I', extent) + tiles + bytes(3 * 4);npcs = []
        for group in range(0, 15):
            for index, x in enumerate((-1, 0, 0.25, 39.99, extent - 0.25, extent)):
                positions = [(x, 9, x, (0, -0.5, 0.5, 8, 100000000, float('nan'))[index])]
                if index == 0:
                    positions = []
                elif index == 2:
                    positions += [(4, 5, 6, 0)]
                npc = struct.pack('<3i', group, 99999 if index == 3 else 1, len(positions))
                npc += b''.join(struct.pack('<4f', *position) for position in positions);npcs.append(npc)
        data += struct.pack('<I', len(npcs)) + b''.join(npcs) + bytes(3 * 4)
        records = prepare(data, True);print('S', data.hex(), records.hex(), sep='\t')


if __name__ == '__main__':
    main()
