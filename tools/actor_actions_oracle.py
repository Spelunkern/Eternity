#!/usr/bin/env python3
"""Execute original move/vehicle/casting cancellation over two live actor layouts."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

OFFSETS = (0x1460, 0x1464, 0x1468, 0x146c, 0x1470, 0x1474, 0x1478, 0x5818, 0x581c, 0x586c,
           *range(0x1488, 0x14b4, 4), 0x142c, 0x1274, 0x143c, 0x1440, 0x1450,
           0x1528, 0x152c, 0x1530, 0x1534, 0x1344, 0x1348, 0x1428, 0x1358)


def main():
    cpu, digest = load_game()
    stack, users, zones, stop = 0x20000000, 0x30000000, 0x31000000, 0x10000000
    for address in (stack, zones, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(users, 0x20000)
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00');cpu.mem_write(stop + 0x200, b'\xc3')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    cpu.mem_write(0x5210e4, struct.pack('<I', stop + 0x200))
    for address, cleanup in ((0x415f30, 4), (0x4209a0, 20), (0x4208c0, 12), (0x421220, 32), (0x4d4d20, 8)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    trace, state = [], {}

    def words(address, count=1):
        return struct.unpack('<' + 'I' * count, cpu.mem_read(address, 4 * count))

    def hook(machine, address, size, context):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x415f30:
            identity = words(sp + 4)[0];trace.append('Q' + str(identity))
            if machine.reg_read(UC_X86_REG_EAX) != words(users + 0xe0)[0]:
                raise RuntimeError('Unexpected vehicle requester zone')
            target = users if identity == 9001 else users + 0x10000 if identity == 9002 and state['available'] else 0
            if target and words(target + 0xe0)[0] != words(users + 0xe0)[0]:
                target = 0
            machine.reg_write(UC_X86_REG_EAX, target)
        elif address == stop + 0x200:
            trace.append('T');machine.reg_write(UC_X86_REG_EAX, state['tick'])
        elif address == 0x4209a0:
            zone, ptr, length, position, radius = words(sp + 4, 5)
            if (zone, position, radius) != (words(users + 0xe0)[0], users + 0xd0, 0x42700000):
                raise RuntimeError('Unexpected ordinary ranged move-cancel route')
            trace.append('M' + bytes(cpu.mem_read(ptr, length)).hex())
        elif address == 0x421220:
            zone, ptr, length, position, radius, identity, unknown, channel = words(sp + 4, 8)
            actor = users if identity == 9001 else users + 0x10000
            if (zone, position, radius, unknown, channel) != (words(actor + 0xe0)[0], actor + 0xd0, 0x42700000, 0, 5):
                raise RuntimeError('Unexpected prioritized speed route')
            trace.append('N' + bytes(cpu.mem_read(ptr, length)).hex())
        elif address == 0x4208c0:
            zone, ptr, length = words(sp + 4, 3);identity = words(ptr + 2)[0];index = identity - 9001
            if index not in (0, 1) or (zone, machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX)) != (zones, 17 + index, 23 + index):
                raise RuntimeError('Unexpected vehicle/shape cell route')
            trace.append('C' + bytes(cpu.mem_read(ptr, length)).hex())
        elif address == 0x4d4d20:
            ptr, length = words(sp + 4, 2);trace.append('S' + bytes(cpu.mem_read(ptr, length)).hex())

    for address in (0x415f30, stop + 0x200, 0x4209a0, 0x4208c0, 0x421220, 0x4d4d20):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    rng = random.Random(0x489f10)

    def initial(index, scenario):
        flags = [rng.randrange(2), rng.randrange(2), rng.choice((0, 13, 100, 101, 102)), 42 + index]
        values = [rng.choice((-1, 0, 1, 2, 3)), rng.getrandbits(32), rng.randrange(-100, 100), rng.randrange(-100, 100),
                  rng.choice((0, 9001, 9002, 9999)), rng.choice((0, 9001, 9002, 9999)), rng.getrandbits(32),
                  rng.choice((-0x80000000, -1, 0, 1, 2, 3)), rng.choice((0, 1, 0xffffffff)), rng.choice((0, 10, 11, 0xffffffff)),
                  *[rng.getrandbits(32) for _ in range(11)], rng.choice((-1, 0, 1, 2, 3)), rng.choice((-2, 0, 1, 7)),
                  rng.choice((0, 123, 0xffffffff)), rng.getrandbits(32), rng.getrandbits(32), rng.choice((0, 1, 2)), rng.choice((9001, 9002, 9999)),
                  rng.choice((0, 1, 2)), rng.getrandbits(32), rng.choice((-1, 0, 1, 3)), rng.choice((-1, 0, 1)), rng.randrange(2), 321]
        # Deliberate coverage for cast-to-self restoration and both resume branches.
        if index == 0 and scenario % 4 == 0:
            values[21] = 3;values[26] = 1;values[27] = 9001;values[23] = 0 if scenario % 8 == 0 else 123
        return flags + [value & 0xffffffff for value in values]

    def initialize(index, values):
        actor = users + index * 0x10000;zone, visible, shape, form = values[:4]
        cpu.mem_write(actor, bytes(0x9000))
        for offset, value in zip(OFFSETS, values[4:]):
            cpu.mem_write(actor + offset, struct.pack('<I', value))
        cpu.mem_write(actor + 0xdc, struct.pack('<I', 9001 + index));cpu.mem_write(actor + 0xe0, struct.pack('<III', zones if zone else 0, 17 + index, 23 + index))
        cpu.mem_write(actor + 0xd0, struct.pack('<3f', 10 + index, 20 + index, 30 + index))
        cpu.mem_write(actor + 0x5774, bytes([visible]));cpu.mem_write(actor + 0x1354, bytes([shape]));cpu.mem_write(actor + 0x134c, struct.pack('<H', form))
        cpu.mem_write(actor + 0x12ec, struct.pack('<II', 2, 2));cpu.mem_write(actor + 0x135c, struct.pack('<I', 0x778899aa))
        root = actor + 0x8000;cpu.mem_write(root, struct.pack('<II', root, root));cpu.mem_write(actor + 0xa94, struct.pack('<II', root, root))

    def snapshot(index):
        actor = users + index * 0x10000
        if words(actor + 0x135c)[0] != 0x778899aa:
            raise RuntimeError('Action cancellation changed cached copied-user pointer')
        return [int(words(actor + 0xe0)[0] != 0), cpu.mem_read(actor + 0x5774, 1)[0], cpu.mem_read(actor + 0x1354, 1)[0], struct.unpack('<H', cpu.mem_read(actor + 0x134c, 2))[0],
                *[words(actor + offset)[0] for offset in OFFSETS]]

    print(f'# ps_game.exe SHA256 {digest}; CancleMove 0048a010, CancleActionVehicle 0044b2c0, StatusCastingStop 00489f10; original nested effect/shape/speed/attack-stop code')
    print('# op tick peerAvailable; each actor: zone visible shape form, DWORDs ' + ' '.join(hex(value) for value in OFFSETS) + '; final same fields for both actors; eventCount events(Q lookup,M ordinary range,N priority range,C cell,S self,T clock)')
    for op, entry in enumerate((0x48a010, 0x44b2c0, 0x489f10)):
        for scenario in range(512):
            first, second = initial(0, scenario), initial(1, scenario)
            state.update(tick=rng.choice((0, 123456, 0xfffffff0, rng.getrandbits(32))), available=rng.randrange(2))
            initialize(0, first);initialize(1, second);trace.clear()
            cpu.mem_write(stack + 0x8000, struct.pack('<I', stop));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
            cpu.reg_write(UC_X86_REG_EAX, users);cpu.reg_write(UC_X86_REG_ESI, users)
            cpu.emu_start(entry, stop, count=100000)
            if cpu.reg_read(UC_X86_REG_EIP) != stop:
                raise RuntimeError('Original action cancellation did not return')
            print(op, state['tick'], state['available'], *first, *second, *snapshot(0), *snapshot(1), len(trace), *trace, sep='\t')


if __name__ == '__main__':
    main()
