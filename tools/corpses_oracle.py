#!/usr/bin/env python3
"""Execute original corpse capture/cell lists and registry control with isolated map/IO ports."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, world, objects, bodies, zones, cells, roots, items, stop = (0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x35000000, 0x36000000, 0x37000000, 0x10000000)
    for address in (stack, user, world, objects, bodies, items, stop):
        cpu.mem_map(address, 0x10000)
    for address in (zones, cells, roots):
        cpu.mem_map(address, 0x20000)
    cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00');cpu.mem_write(stop + 0x200, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    cpu.mem_write(0x521120, struct.pack('<I', stop + 0x200))
    cpu.mem_write(0x546f38, struct.pack('<I', world));cpu.mem_write(0x546f3c, struct.pack('<I', objects))
    cpu.mem_write(world + 0x1a0, struct.pack('<I', stop + 0x700))
    for address, cleanup in ((0x503a3f, 0), (0x40fc60, 0), (0x418210, 8), (0x418250, 4), (0x410980, 0), (0x4d9510, 0), (0x4208c0, 12), (0x4d4d20, 8)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    counts = (5, 9)

    def write(address, *values):
        cpu.mem_write(address, struct.pack('<' + 'I' * len(values), *(value & 0xffffffff for value in values)))

    def word(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def cell(zone, x, z):
        return cells + zone * 0x10000 + (z * counts[zone] + x) * 292

    for zone, count in enumerate(counts):
        base = zones + zone * 0x10000;write(base + 0x28, count, count, base + 0x1000)
        for z in range(count):
            write(base + 0x1000 + 4 * z, cell(zone, 0, z))
            for x in range(count):
                root = roots + zone * 0x10000 + (z * count + x) * 16
                write(root, root, root);write(cell(zone, x, z) + 0xb0, root, root)
    registry, allocated, events, current = {}, [], [], {}

    def index(address):
        return allocated.index(address) if address else -1

    def hook(machine, address, size, context):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503a3f:
            if word(sp + 4) != 96 or len(allocated) >= 250:
                raise RuntimeError('Unexpected corpse allocation')
            body = bodies + len(allocated) * 256;allocated.append(body);cpu.mem_write(body, bytes(96));machine.reg_write(UC_X86_REG_EAX, body)
        elif address == 0x4584e2:
            current['created'] = machine.reg_read(UC_X86_REG_EAX)
        elif address == 0x40fc60:
            machine.reg_write(UC_X86_REG_EAX, registry.get(word(machine.reg_read(UC_X86_REG_EBX)), 0))
        elif address == 0x418210:
            registry[word(word(sp + 4))] = word(sp + 8)
        elif address == 0x418250:
            machine.reg_write(UC_X86_REG_EAX, registry.pop(word(word(sp + 4)), 0))
        elif address == 0x410980:
            output = machine.reg_read(UC_X86_REG_EAX);body = registry.get(word(machine.reg_read(UC_X86_REG_EBX)), 0)
            write(stop + 0x510, body);write(output, stop + 0x500 if body else stop + 0x700)
        elif address == 0x4d9510:
            kind = {0x530cec: 'D', 0x530cc4: 'M', 0x530cf8: 'W'}[word(sp + 8)]
            events.append(kind + str(word(sp + 12)))
        elif address == 0x4208c0:
            zone, ptr, length = (word(sp + offset) for offset in (4, 8, 12))
            identity = word(ptr + 2)
            body = current['leaving']
            if identity in registry or word(body + 0x18) != 0:
                raise RuntimeError('Original corpse removal broadcast preceded unlink')
            events.append(f'V{(zone-zones)//0x10000},{machine.reg_read(UC_X86_REG_ECX)},{machine.reg_read(UC_X86_REG_EAX)}:' + bytes(cpu.mem_read(ptr, length)).hex())
        elif address == 0x40a6a0:
            body = machine.reg_read(UC_X86_REG_EDI)
            if any(word(body + offset) for offset in (0x18, 0x1c, 0x20)):
                raise RuntimeError('Original corpse returned to heap before detach')
            events.append('F' + str(index(body)))
        elif address == 0x4d4d20:
            ptr, length = word(sp + 4), word(sp + 8);events.append('S' + bytes(cpu.mem_read(ptr, length)).hex())
        elif address == stop + 0x200:
            ptr = word(sp + 4);length = 0
            while cpu.mem_read(ptr + length, 1)[0]:
                length += 1
            machine.reg_write(UC_X86_REG_EAX, length)

    for address in (0x503a3f, 0x4584e2, 0x40fc60, 0x418210, 0x418250, 0x410980, 0x4d9510, 0x4208c0, 0x40a6a0, 0x4d4d20, stop + 0x200):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)

    def call(entry, args=(), regs=(), end=stop):
        write(stack + 0x8000, stop, *args);cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        for reg, value in regs:
            cpu.reg_write(reg, value)
        try:
            cpu.emu_start(entry, end, count=100000)
        except Exception as error:
            raise RuntimeError(f'Corpse oracle fault at {cpu.reg_read(UC_X86_REG_EIP):08x}, entry {entry:08x}') from error
        if cpu.reg_read(UC_X86_REG_EIP) != end:
            raise RuntimeError('Original corpse operation did not return')
        return cpu.reg_read(UC_X86_REG_EAX)

    rng = random.Random(0x417720)
    def creation(zone, identity, mask, name, x=10.0, z=10.0):
        bits = [struct.unpack('<I', struct.pack('<f', value))[0] for value in (x, rng.uniform(-50, 50), z)]
        return ['C', zone, identity, *bits, rng.randrange(65536), *[rng.randrange(256) for _ in range(7)], mask,
                *[rng.randrange(256) for _ in range(16)], name.hex() or '-']

    fixed = [creation(0, 9001, 255, b'ABCDEFGHIJKLMNOPQRST'), ['E', 0, 0, 0], ['F', 1, 9001], ['Q', 0, 0, 0, 9001],
             creation(1, 9001, 0, b'duplicate'), ['R', 9001], creation(1, 9002, 1, b'x'), ['E', 1, 0, 0], ['O', 1, 0, 0], ['L', 0, 1],
             creation(0, 9003, 0, b'outside', 200), ['R', 9002]]
    print(f'# ps_game.exe SHA256 {digest}; EnterDead 004584d0, corpse zone/cell find/enter/leave, FreeDead and actual LIFO allocation; map/transport/heap-allocation ports isolated')
    print('# operation and input; result; allocated count and bodies(zone cellX cellZ snapshotHex); global count and (ID handle)*; event count and events')
    for step in range(396):
        if step < len(fixed):
            command = fixed[step]
        else:
            op = rng.choice(('C', 'C', 'F', 'W', 'R', 'L', 'E', 'O', 'Q'))
            zone, identity = rng.randrange(2), rng.randrange(9000, 9012)
            if op == 'C':
                command = creation(zone, identity, rng.randrange(256), rng.choice((b'', b'long-name-0123456789', b'\x81\xa1\x82\xa2', b'short')),
                                   rng.choice((-0.25, 10.0, 39.9, 40.0, 80.0, 159.9, 199.9, 200.0, 359.9, 360.0)), rng.choice((10.0, 39.9, 40.0, 80.0)))
            elif op == 'L':
                candidates = [body for body in allocated if registry.get(word(body + 0x14), body) == body]
                body = rng.choice(candidates);owner = word(body + 0x18);command = ['L', (owner-zones)//0x10000 if owner else zone, index(body)]
            elif op in ('E', 'O', 'Q'):
                command = [op, zone, rng.randrange(3), rng.randrange(3)] + ([identity] if op == 'Q' else [])
            else:
                command = [op, zone, identity] if op == 'F' else [op, identity]
        events.clear();op = command[0];result = 0
        if op == 'C':
            _, zone, identity, x, y, z, direction, *rest = command
            appearance, mask, types, ids, name = rest[:7], rest[7], rest[8:16], rest[16:24], bytes.fromhex(rest[24]) if rest[24] != '-' else b''
            cpu.mem_write(user, bytes(0x6000));write(user + 0xd0, x, y, z, identity);write(user + 0xe0, zones + zone * 0x10000)
            cpu.mem_write(user + 0x166, struct.pack('<H', direction));cpu.mem_write(user + 0x188, name + b'\0')
            for offset, value in zip((0x12d, 0x12e, 0x131, 0x132, 0x133, 0x134, 0x135), appearance):
                cpu.mem_write(user + offset, bytes([value]))
            for slot in range(8):
                if mask & (1 << slot):
                    write(user + 0x1c4 + slot * 4, items + slot * 256);cpu.mem_write(items + slot * 256 + 0x40, bytes((types[slot], ids[slot])))
            call(0x4584d0, regs=((UC_X86_REG_EDI, user),));result = index(current['created'])
        elif op in ('L', 'R'):
            body = allocated[command[2]] if op == 'L' else registry.get(command[1], 0);current['leaving'] = body
            if op == 'L':
                result = call(0x417820, (zones + command[1] * 0x10000,), ((UC_X86_REG_EBX, body),)) & 255
            else:
                result = int(body != 0);call(0x40d520, (command[1],), ((UC_X86_REG_EAX, world),))
        elif op in ('F', 'W', 'Q'):
            if op == 'F':
                value = call(0x417640, (zones + command[1] * 0x10000, command[2]))
            elif op == 'W':
                value = call(0x4176c0, (command[1],))
            else:
                value = call(0x4239c0, (command[4],), ((UC_X86_REG_EDI, cell(*command[1:4])),))
            result = index(value)
        else:
            target = cell(*command[1:4]);write(stack + 0x800c, user)
            if op == 'E':
                call(0x423ccb, regs=((UC_X86_REG_EDI, target), (UC_X86_REG_EBP, 0)), end=0x423ea7)
            else:
                call(0x4241aa, regs=((UC_X86_REG_ESI, target), (UC_X86_REG_EBP, user)), end=0x42420e)
        snapshots = []
        for body in allocated:
            owner = word(body + 0x18)
            raw = bytes(cpu.mem_read(body + 0x14, 4)) + bytes(cpu.mem_read(body + 8, 12)) + bytes(cpu.mem_read(body + 0x30, 62-16))
            snapshots.extend(((owner-zones)//0x10000 if owner else -1, word(body + 0x1c), word(body + 0x20), raw.hex()))
        print(*command, result, len(allocated), *snapshots, len(registry), *(value for identity, body in sorted(registry.items()) for value in (identity, index(body))), len(events), *events, sep='\t')


if __name__ == '__main__':
    main()
