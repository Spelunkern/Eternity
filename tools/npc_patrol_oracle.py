#!/usr/bin/env python3
"""Execute original NPC waypoint patrol and vector math. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, npc, source, definition, points, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x10000000
    for address in (0, stack, npc, source, definition, points, stop):
        cpu.mem_map(address, 0x10000)
    cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    cpu.mem_write(0x50d0ca, b'\xb8' + struct.pack('<I', stop + 0x1000) + b'\xc3')
    cpu.mem_write(0x416f30, b'\xc2\x14\x00');cpu.mem_write(0x504697, b'\xc3')
    context = dict(random=0, calls=0, success=False, moves=b'')

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x416f30:
            zone, x, y, z, mode = struct.unpack('<5I', machine.mem_read(sp + 4, 20))
            if zone != 0x12345678 or mode != 0:
                raise RuntimeError('Unexpected original NPC patrol move')
            context['moves'] += struct.pack('<4I', x, y, z, mode)
            if context['success']:
                machine.mem_write(npc + 8, struct.pack('<3I', x, y, z))
            machine.reg_write(UC_X86_REG_EAX, int(context['success']))
        elif address == 0x504697:
            context['calls'] += 1;machine.reg_write(UC_X86_REG_EAX, context['random'] & 0xffffffff)

    cpu.hook_add(UC_HOOK_CODE, hook)

    def initialize(position, path, index, forward, users, deadline, distance, speed, present=True):
        cpu.mem_write(npc, bytes(152));cpu.mem_write(source, bytes(32));cpu.mem_write(definition, bytes(292))
        cpu.mem_write(npc + 8, struct.pack('<3f', *position));cpu.mem_write(npc + 0x18, struct.pack('<I', 0x12345678))
        cpu.mem_write(npc + 0x40, struct.pack('<iB3xiI2I', users, forward, index, deadline, definition, source if present else 0))
        cpu.mem_write(source + 8, struct.pack('<2I', len(path), points));cpu.mem_write(points, b''.join(struct.pack('<3f', *point) for point in path))
        cpu.mem_write(definition + 8, struct.pack('<2i', distance, speed))

    def update(tick, random, success):
        context.update(random=random, success=success, calls=0, moves=b'')
        cpu.mem_write(stack + 0x8000, struct.pack('<2I', stop, tick));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_EAX, npc)
        cpu.emu_start(0x4bbeb0, stop, count=200000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original NPC patrol did not return')
        return (bytes(cpu.mem_read(npc + 8, 12)) + bytes(cpu.mem_read(npc + 0x44, 1)) + bytes(cpu.mem_read(npc + 0x48, 8))
                + struct.pack('<2I', context['calls'], len(context['moves']) // 16) + context['moves'])

    def bits(position):
        return struct.unpack('<3I', struct.pack('<3f', *position))
    print(f'# ps_game.exe SHA256 {digest}; CNpc::UpdateMove 004bbeb0, Normalize 0041b250, Length 0041b2e0, Length2 004a7c30, vector multiply/add, real CRT sqrt. Movement/rand/TLS ports isolated.')
    print('# M users deadline tick source_present point_count index forward distance speed random move_success position_bits path_bits expected_hex. B path initial state; T tick random success expected_hex. Result: position,forward BYTE,index,next_move,random_calls,move_count,XYZ/mode per move.')
    for scenario in range(1536):
        count = (0, 1, 2, 3, 4)[scenario % 5] if scenario < 256 else 2 + scenario % 3
        present = scenario % 31 != 0;index = (scenario // 3) % max(count, 1);forward = scenario % 2
        users = (0, -1, 1, 3)[(scenario // 11) % 4] if scenario < 256 else 0
        tick = (0, 100, 0x7fffffff, 0xffffffff)[(scenario // 7) % 4];deadline = (0, 100, 101, 0xffffffff)[(scenario // 13) % 4] if scenario < 256 else tick
        distance = (0, 1, 2, 3, 5, 100, -1, 16777217, 2147483647)[(scenario // 5) % 9];speed = (0, 1, 500, -1, 2147483647, -2147483648)[(scenario // 17) % 6]
        random = (0, 9999, 10000, 32767, -1, -2147483648)[(scenario // 19) % 6];success = scenario % 7 != 0
        coordinates = (-0.25, 0, 0.25, 1, 3, 5, 60.75, -100.75, 16777216, 1e20, float('inf'), float('nan'))
        position = (coordinates[(scenario // 23) % len(coordinates)], -3.25, coordinates[(scenario // 29) % len(coordinates)])
        path = [(coordinates[(scenario + i) % len(coordinates)], i * 2 + 1.5, coordinates[(scenario // 2 + i) % len(coordinates)]) for i in range(count)]
        initialize(position, path, index, forward, users, deadline, distance, speed, present);result = update(tick, random, success)
        print('M', users, deadline, tick, int(present), count, index, forward, distance, speed, random, int(success), *bits(position), *(v for point in path for v in bits(point)), result.hex(), sep='\t')
    for scenario in range(12):
        path = [(3.25, 2, 4.75), (45.5, 9, 60.25), (150.75, 3, 180.5)];position = path[0]
        distance, speed = (1, 5, 100)[scenario % 3], (0, 500, -1, 2147483647)[scenario // 3]
        initialize(position, path, 0, True, 0, 0, distance, speed)
        print('B', distance, speed, *bits(position), len(path), *(v for point in path for v in bits(point)), sep='\t')
        for step in range(48):
            tick = number(npc + 0x4c);random = (step * 701) % 32768;success = step % 11 != 4
            print('T', tick, random, int(success), update(tick, random, success).hex(), sep='\t')


if __name__ == '__main__':
    main()
