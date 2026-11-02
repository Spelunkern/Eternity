#!/usr/bin/env python3
"""Execute original map environment preparation, doors, weather and MSVCRT random. Requires Unicorn."""
import hashlib
import math
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game, ROOT


def main():
    cpu, digest = load_game()
    stack, obj, zone, door, user, heap, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x40000000, 0x10000000
    for address in (0, stack, obj, zone, door, user, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(heap, 0x4000000);cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    original_rand = bytes(cpu.mem_read(0x504697, 34))
    for address, cleanup in ((0x503ae3, 0), (0x505afd, 0), (0x510785, 0), (0x50c649, 0),
                             (0x4233d0, 0), (0x423a30, 0), (0x429130, 4), (0x429a60, 0),
                             (0x4208c0, 12), (0x4207f0, 0), (0x4d4d20, 8), (0x504697, 0)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    cpu.mem_write(0x50d0ca, b'\xb8' + struct.pack('<I', stop + 0x1000) + b'\xc3')
    context = dict(data=b'', position=0, allocation=heap, events=[], random=0, calls=0, phase='prepare')

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503ae3:
            amount = number(sp + 4);pointer = context['allocation'];context['allocation'] += (max(amount, 1) + 15) & ~15
            if context['allocation'] > heap + 0x4000000:
                raise RuntimeError('Environment oracle allocation limit')
            machine.mem_write(pointer, bytes(amount));machine.reg_write(UC_X86_REG_EAX, pointer)
        elif address == 0x505afd:
            machine.reg_write(UC_X86_REG_EAX, 12)
        elif address == 0x50c649:
            machine.reg_write(UC_X86_REG_EAX, 0)
        elif address == 0x510785:
            fd, pointer, amount = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
            data = context['data'][context['position']:context['position'] + amount]
            if fd != 12 or len(data) != amount:
                raise RuntimeError('Original environment loader input changed')
            machine.mem_write(pointer, data);context['position'] += amount;machine.reg_write(UC_X86_REG_EAX, amount)
        elif address == 0x4208c0:
            actual_zone, pointer, length = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
            if (actual_zone, machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX)) != (zone, 17, 23):
                raise RuntimeError('Unexpected door grid route')
            context['events'].append('C' + bytes(machine.mem_read(pointer, length)).hex())
        elif address == 0x4207f0:
            context['events'].append('A' + bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX))).hex())
        elif address == 0x4d4d20:
            pointer, length = struct.unpack('<2I', machine.mem_read(sp + 4, 8))
            context['events'].append('S' + bytes(machine.mem_read(pointer, length)).hex())
        elif address == 0x504697 and context['phase'] != 'random':
            context['calls'] += 1;machine.reg_write(UC_X86_REG_EAX, context['random'])

    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, arguments=()):
        cpu.mem_write(stack + 0x8000, struct.pack('<' + 'I' * (len(arguments) + 1), stop, *arguments));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.emu_start(address, stop, count=10000000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original environment operation did not return')

    def prepare(data):
        context.update(data=data, position=0, allocation=heap, phase='prepare');cpu.mem_write(obj, bytes(0x1000));cpu.mem_write(zone, bytes(0x1000))
        cpu.reg_write(UC_X86_REG_EDI, obj);cpu.reg_write(UC_X86_REG_ECX, user);call(0x425780)
        cpu.mem_write(obj + 0x24, struct.pack('<5I', 0, 2, 30, 180000, 1800000));call(0x41c8a0, (zone, obj))
        if cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
            raise RuntimeError('Original environment preparation failed')
        records = bytes(cpu.mem_read(zone + 0x278, 20)) + bytes(cpu.mem_read(zone + 0x28c, 1)) + bytes(cpu.mem_read(zone + 0x290, 4))
        for count_offset, pointer_offset, width, regions in ((0x294, 0x298, 68, False), (0x29c, 0x2a0, 80, True)):
            count, pointer = number(zone + count_offset), number(zone + pointer_offset);records += struct.pack('<I', count)
            for i in range(count):
                at = pointer + i * width
                records += bytes(cpu.mem_read(at, 16)) + bytes(cpu.mem_read(at + 0x14, 8))
                if regions:
                    records += bytes(cpu.mem_read(at + 0x28, 40))
                else:
                    records += bytes(cpu.mem_read(at + 0x30, 1)) + bytes(cpu.mem_read(at + 0x34, 16))
        return records

    def fingerprint(data):
        value = 0xcbf29ce484222325
        for byte in data:
            value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        return value

    print(f'# ps_game.exe SHA256 {digest}; MapInit 0041c8a0 environment fields, Use 004b9590, AutoClose 004b95f0, UpdateDoor 0041d530, UpdateWeather 0041d4d0, SendWeather 004201b0, rand 00504697.')
    print('# A filename SHA256 canonical_bytes FNV1a64; S map_hex environment_hex. D operation/scenario and ordered packets. W scenario and weather state/RNG/packets. R seed sequence (value,state). NPC/obelisk/door-cell insertion and transport/file/allocation ports isolated.')
    for path in sorted((ROOT / '.local/server-data/Map').glob('*.svmap'), key=lambda path: int(path.stem)):
        data = path.read_bytes();records = prepare(data)
        print('A', path.name, hashlib.sha256(data).hexdigest(), len(records), fingerprint(records), sep='\t')
    for extent in (0, 40, 81, 160):
        points = (-41, -40, -39.99, -0.25, 0, 39.99, 40, 49.99, 50, extent - 1, extent, (extent // 40 + 1) * 40 - 0.01, (extent // 40 + 1) * 40, math.inf, math.nan)
        data = struct.pack('<I', extent) + bytes((extent * extent + 7) // 8) + struct.pack('<I', len(points))
        for i, x in enumerate(points):
            data += struct.pack('<3fii', x, i, points[(i + 3) % len(points)], i % 3 - 1, (0, 1, -1, 2147483647)[i % 4])
        data += bytes(5 * 4) + struct.pack('<I', len(points))
        for i, x in enumerate(points):
            data += struct.pack('<6fiBBH', x, 1, 0, x + 0.5, 3, 2, 1 if i % 3 else 2, i, 255 - i, 65000 + i)
        records = prepare(data);print('S', data.hex(), records.hex(), sep='\t')
    context['phase'] = 'runtime'
    for operation, entry in enumerate((0x4b9590, 0x4b95f0, 0x41d530)):
        for s in range(128):
            context['events'] = [];cpu.mem_write(door, bytes(68));cpu.mem_write(zone, bytes(0x1000))
            cpu.mem_write(door + 0xc, struct.pack('<4I', (s * 7) & 511, zone if s & 1 else 0, 17, 23))
            cpu.mem_write(door + 0x30, bytes([int(bool(s & 2))]))
            close_tick = (0, 100, 2000, 0xffffffff)[(s // 3) % 4];use_tick = (0, 100, 2000, 0xffffffff)[(s // 5) % 4]
            auto_close, delay = (0, 1, -1)[(s // 7) % 3], (0, 1, 2000, 0xffffffff)[(s // 11) % 4]
            cpu.mem_write(door + 0x34, struct.pack('<2IiI', close_tick, use_tick, auto_close, delay))
            tick = (0, 99, 100, 1999, 2000, 0xffffffff)[s % 6]
            cpu.reg_write(UC_X86_REG_ECX, door);cpu.reg_write(UC_X86_REG_EAX, tick)
            cpu.mem_write(zone + 0x294, struct.pack('<2I', 1, door));cpu.reg_write(UC_X86_REG_ESI, zone)
            call(entry, (tick,) if operation == 2 else ())
            print('D', operation, s, cpu.mem_read(door + 0x30, 1)[0], number(door + 0x34), number(door + 0x38), len(context['events']), *context['events'], sep='\t')
    for s in range(512):
        context.update(events=[], calls=0, random=(0, 1, 29, 30, 99, 100, 32767)[s % 7])
        state, power, rate = (0, 1, 2, -1, 255)[s % 5], (0, 2, 257, -1)[(s // 3) % 4], (-1, 0, 1, 30, 99, 100, 101)[(s // 5) % 7]
        delay, none = (0, 1, 1000, 0xffffffff)[(s // 11) % 4], (0, 5000, 0xffffffff)[(s // 13) % 3]
        active, following, tick = (s // 7) % 3, (0, 100, 0xffffffff)[(s // 17) % 3], (0, 99, 100, 1999, 2000, 0xffffffff)[s % 6]
        cpu.mem_write(zone, bytes(0x1000));cpu.mem_write(zone + 0x278, struct.pack('<3i2IB3xI', state, power, rate, delay, none, active, following))
        cpu.reg_write(UC_X86_REG_EAX, zone);cpu.reg_write(UC_X86_REG_EDI, tick);call(0x41d4d0)
        cpu.reg_write(UC_X86_REG_EAX, zone);call(0x4201b0, (user, (0, 1, 255)[s % 3]))
        print('W', s, cpu.mem_read(zone + 0x28c, 1)[0], number(zone + 0x290), context['calls'], len(context['events']), *context['events'], sep='\t')
    context['phase'] = 'random';cpu.mem_write(0x504697, original_rand)
    cpu.ctl_remove_cache(0x504697, 0x504697 + len(original_rand))
    for seed in (0, 1, 5489, 0x80000000, 0xffffffff):
        cpu.mem_write(stop + 0x1014, struct.pack('<I', seed));values = []
        for _ in range(32):
            call(0x504697);values.extend((cpu.reg_read(UC_X86_REG_EAX), number(stop + 0x1014)))
        print('R', seed, *values, sep='\t')


if __name__ == '__main__':
    main()
