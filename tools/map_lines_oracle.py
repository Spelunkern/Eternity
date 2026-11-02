#!/usr/bin/env python3
"""Execute both original map line checks and safe collision queries. Requires Unicorn."""
import hashlib
import struct
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game, ROOT


def main():
    cpu, digest = load_game();stack, obj, output, tiles, stop = 0x20000000, 0x30000000, 0x31000000, 0x40000000, 0x10000000
    for address in (stack, obj, output, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(tiles, 0x1000000);cpu.reg_write(UC_X86_REG_FPCW, 0x27f)

    def call(address, arguments=()):
        cpu.mem_write(stack + 0x8000, struct.pack('<' + 'I' * (len(arguments) + 1), stop, *(value & 0xffffffff for value in arguments)));cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.emu_start(address, stop, count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original map line query did not return')
        return cpu.reg_read(UC_X86_REG_EAX) & 255

    def prepare(size, data):
        cpu.mem_write(obj, bytes(256));cpu.mem_write(obj + 4, struct.pack('<2I', size, tiles));cpu.mem_write(tiles, data)

    def query(start, end):
        cpu.mem_write(output, struct.pack('<2i', -777, 888));cpu.reg_write(UC_X86_REG_ECX, obj);cpu.reg_write(UC_X86_REG_EAX, end[1] & 0xffffffff)
        result = call(0x42ab80, (*start, end[0], output, output + 4));last = bytes(cpu.mem_read(output, 8))
        cpu.reg_write(UC_X86_REG_ECX, end[0] & 0xffffffff);cpu.reg_write(UC_X86_REG_EAX, end[1] & 0xffffffff);cpu.reg_write(UC_X86_REG_EDX, obj)
        simple = call(0x42ade0, start)
        blocked = []
        for point in (start, end):
            cpu.reg_write(UC_X86_REG_ECX, obj);cpu.reg_write(UC_X86_REG_EAX, point[0] & 0xffffffff);cpu.reg_write(UC_X86_REG_EDX, point[1] & 0xffffffff)
            blocked.append(call(0x41b420))
        if result != simple:
            raise RuntimeError('Original line overloads differ')
        return bytes([result]) + last + bytes([simple, *blocked])

    def fingerprint(data):
        value = 0xcbf29ce484222325
        for byte in data:
            value = ((value ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        return value

    def endpoints(size, scenario):
        values = (-1, 0, 1, size - 1, size, -2147483648, 2147483647)
        if scenario < 32:
            return (values[scenario % 7], values[(scenario // 3) % 7]), (values[(scenario // 5) % 7], values[(scenario + 1) % 7])
        if size == 0:
            return (0, 0), (0, 0)
        start = ((scenario * 31 + 7) % size, (scenario * 17 + 3) % size);end = ((scenario * 13 + 1) % size, (scenario * 41 + 5) % size)
        if scenario % 7 == 0:
            end = start
        elif scenario % 7 == 1:
            end = (start[0], end[1])
        elif scenario % 7 == 2:
            end = (end[0], start[1])
        return start, end

    print(f'# ps_game.exe SHA256 {digest}; CMap::EnableLine 0042ab80/0042ade0, IsBlockSafe 0041b420, IsBlock 0042ab40 and real bit queries. No function ports replaced.')
    print('# N id extent tile_hex (dash means empty). L id start/end expected_hex: BOOL,previous_X/Z,BOOL no-output overload,blocked_start/end. A filename SHA256 canonical_bytes FNV1a64 for 128 deterministic queries.')
    for index in range(16):
        size = (0, 1, 4, 16, 37, 81, 160, 320)[index % 8];data = bytearray((size * size + 7) // 8)
        for z in range(size):
            for x in range(size):
                blocked = ((x * 11 + z * 19 + index) % 17 == 0) if index < 8 else (x == size // 2 and z != size // 3) or (z == size // 2 and x != size // 3)
                if blocked:
                    at = z * size + x;data[at // 8] |= 1 << (at % 8)
        prepare(size, bytes(data));print('N', index, size, data.hex() or '-', sep='\t')
        for scenario in range(256):
            start, end = endpoints(size, scenario);print('L', index, *start, *end, query(start, end).hex(), sep='\t')
    for path in sorted((ROOT / '.local/server-data/Map').glob('*.svmap'), key=lambda path: int(path.stem)):
        data = path.read_bytes();size = struct.unpack_from('<I', data)[0];prepare(size, data[4:4 + (size * size + 7) // 8])
        records = b''.join(query(*endpoints(size, scenario)) for scenario in range(128))
        print('A', path.name, hashlib.sha256(data).hexdigest(), len(records), fingerprint(records), sep='\t')


if __name__ == '__main__':
    main()
