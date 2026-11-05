#!/usr/bin/env python3
"""Trace same-cell CZone::MoveUserLocal failure and continuation order."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu, digest = load_game()
stack, zone, map_ptr, user, grid, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x10000000
for base in (stack, zone, map_ptr, user, grid, stop):
    cpu.mem_map(base, 0x10000)
def put(address, value):
    cpu.mem_write(address, struct.pack('<I', value))
def bits(value):
    return struct.unpack('<I', struct.pack('<f', value))[0]
for address, cleanup in ((0x44adb0, 0), (0x445880, 0), (0x4d43a0, 8)):
    cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
for address in (0x5210cc, 0x5210d0):
    put(address, stop + 0x100)
put(zone + 0x24, map_ptr); put(map_ptr + 4, 120); put(zone + 0x30, grid); put(grid, grid + 0x100)
cpu.mem_write(grid + 0x100 + 0x114, struct.pack('<4f', -10, 50, -10, 50))
events = []
def hook(machine, address, size, _):
    if address == 0x44adb0: events.append('G')
    if address == 0x445880: events.append('S')
    if address == 0x4d43a0: events.append('C')
cpu.hook_add(UC_HOOK_CODE, hook)
print(f'# ps_game.exe SHA256 {digest}; CZone::MoveUserLocal 00416370; lock, close, MoveByGame and SendMove intercepted')
print('# location invalid party result x_bits events')
for location, invalid, party in itertools.product((3, 4), (0, 1), (0, 1)):
    cpu.mem_write(user, bytes(0x6000)); put(user + 0x5758, location); put(user + 0x173c, 1 if party else 0)
    put(user + 0xd0, bits(34)); put(user + 0xd4, bits(2)); put(user + 0xd8, bits(3))
    cpu.mem_write(stack + 0x8000, struct.pack('<5I', stop, zone, bits(-1 if invalid else 35), bits(2), bits(3)))
    cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000); cpu.reg_write(UC_X86_REG_EAX, user); events.clear()
    cpu.emu_start(0x416370, stop, count=10000)
    if cpu.reg_read(UC_X86_REG_EIP) != stop: raise RuntimeError('local move did not return')
    x = struct.unpack('<I', cpu.mem_read(user + 0xd0, 4))[0]
    print(location, invalid, party, cpu.reg_read(UC_X86_REG_EAX) & 255, x, ''.join(events) or '-', sep='\t')
