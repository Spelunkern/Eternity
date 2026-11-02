#!/usr/bin/env python3
"""Execute original ChkFindHide with real IsTeam and cell traversal; record reveal calls. Requires Unicorn."""
import math
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, users, zone, skill, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x10000000
    for address in (0, stack, zone, skill, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(users, 0x100000)
    cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    cpu.mem_write(0x489290, b'\xc3')
    # CRT thread-local storage normally comes from Windows TLS. Keep sqrt/error handling
    # intact and supply a zeroed isolated _tiddata block for its errno path.
    cpu.mem_write(0x50d0ca, b'\xb8' + struct.pack('<I', stop + 0x1000) + b'\xc3')
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    revealed = []

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def hook(machine, address, size, _):
        if address == 0x489290:
            revealed.append(number(machine.reg_read(UC_X86_REG_EBX) + 0xdc))

    cpu.hook_add(UC_HOOK_CODE, hook)
    offsets = ((0, 0), (3, 4), (5, 0), (5.000007629394531, 0), (4.999992370605469, 0),
               (math.nan, 0), (math.inf, 0), (1e20, 1e20), (-3, -4), (0, 5), (0, 0), (255, 0))
    print(f'# ps_game.exe SHA256 {digest}; ChkFindHide 00486e00, IsTeam(CUser*) 0044bdf0, real CRT sqrt; Ghidra work/actor-spatial-actions and work/actor-hide-removal.')
    print('# scenario width height team_mode source_cell_x source_cell_z range; revealed_count; revealed IDs in original order. Inputs specified in tools/zone_users_oracle.py.')
    for s in range(128):
        revealed.clear();cpu.mem_write(users, bytes(0x100000));cpu.mem_write(zone, bytes(0x10000))
        width, height = ((1, 1), (3, 3), (5, 4))[s % 3]
        sx, sz = (s // 3) % width, (s // 5) % height
        mode, radius = (s // 7) % 3, (0, 5, 255)[(s // 11) % 3]
        cpu.mem_write(zone + 0x24, struct.pack('<3I', zone + 0x100, width, height))
        cpu.mem_write(zone + 0x100 + 0x7c, struct.pack('<I', mode))
        cpu.mem_write(zone + 0x30, struct.pack('<I', zone + 0x1000))
        for z in range(height):
            row = zone + 0x2000 + z * width * 0x124
            cpu.mem_write(zone + 0x1000 + z * 4, struct.pack('<I', row))
            for x in range(width):
                head = zone + 0x8000 + (z * width + x) * 0x40
                cpu.mem_write(row + x * 0x124, struct.pack('<I', head))
                cpu.mem_write(head + 8, struct.pack('<2I', head + 8, head + 8))
        for i in range(24):
            user = users + i * 0x6000
            x, z = (sx, sz) if i == 0 else ((s + i * 2) % width, (s * 3 + i) % height)
            cpu.mem_write(user + 0xdc, struct.pack('<I', 9001 + i))
            cpu.mem_write(user + 0xe0, struct.pack('<3I', zone, x, z))
            cpu.mem_write(user + 0x12d, bytes([(s + i) % 3]))
            dx, dz = offsets[(s + i) % len(offsets)]
            position = (100.25, -9000, 200.75) if i == 0 else (100.25 + dx, 9000, 200.75 + dz)
            cpu.mem_write(user + 0xd0, struct.pack('<3f', *position))
            cpu.mem_write(user + 0x142c, struct.pack('<I', int((s + i) % 11 == 0)))
            cpu.mem_write(user + 0x5774, bytes([int((s + i) % 13 != 0), int((s + i) % 7 != 0)]))
            cpu.mem_write(user + 0x54cc, struct.pack('<I', 4 if (s + i) % 5 == 0 else (s + i) % 4))
            cpu.mem_write(user + 0x54d4, struct.pack('<I', 9001 if (s + i) % 2 else 9999))
            own_guild, enemy_guild, active = (1, 2, bool(s & 8)) if i == 0 else (2 if i % 4 == 0 else 3, 1 if i % 4 == 0 else 5, bool((s + i) % 2))
            cpu.mem_write(user + 0x1748, struct.pack('<I', own_guild))
            cpu.mem_write(user + 0x54e4, struct.pack('<I', enemy_guild));cpu.mem_write(user + 0x54fc, struct.pack('<I', active))
            head = zone + 0x8000 + (z * width + x) * 0x40
            previous = number(head + 8)
            cpu.mem_write(user + 8, struct.pack('<2I', previous, head + 8))
            cpu.mem_write(previous + 4, struct.pack('<I', user + 8));cpu.mem_write(head + 8, struct.pack('<I', user + 8))
        cpu.mem_write(skill + 0x57, bytes([radius]))
        cpu.mem_write(stack + 0x8000, struct.pack('<2I', stop, skill))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_ESI, users)
        cpu.emu_start(0x486e00, stop, count=200000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original hidden-user query did not return')
        print(s, width, height, mode, sx, sz, radius, len(revealed), *revealed, sep='\t')


if __name__ == '__main__':
    main()
