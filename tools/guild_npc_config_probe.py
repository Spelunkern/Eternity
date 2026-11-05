#!/usr/bin/env python3
"""Probe original guild-house cost and upkeep loading with isolated profile APIs."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW
from original_oracle import load_game
from map_config_oracle import profile

cpu, digest = load_game()
stack, stop = 0x20000000, 0x10000000
for address in (0, stack, stop):
    cpu.mem_map(address, 0x10000)
cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
cpu.mem_write(0x505316, b'\xc3')
for address in (0x50b152, 0x50b21f):
    cpu.mem_write(address, b'\xc3')
cpu.mem_write(0x50d0ca, b'\xb8' + struct.pack('<I', stop + 0x1000) + b'\xc3')
cpu.mem_write(stop + 0x100, b'\xc2\x18\x00')
cpu.mem_write(stop + 0x200, b'\xc2\x10\x00')
cpu.mem_write(0x5210ec, struct.pack('<I', stop + 0x100))
cpu.mem_write(0x5210f0, struct.pack('<I', stop + 0x200))
initial = bytes(cpu.mem_read(0x541b14, 0xd4))
context = {}
def string(address):
    data = bytearray()
    while address:
        value = cpu.mem_read(address, 1)[0]
        if not value: break
        data.append(value); address += 1
    return bytes(data)
def hook(machine, address, size, user):
    sp = machine.reg_read(UC_X86_REG_ESP)
    if address == stop + 0x100:
        section, key, default, target, capacity, path = struct.unpack('<6I', machine.mem_read(sp + 4, 24))
        value = context['profile'].get((string(section).lower(), string(key).lower()), string(default))[:capacity-1]
        machine.mem_write(target, value + b'\0'); machine.reg_write(UC_X86_REG_EAX, len(value))
    elif address == stop + 0x200:
        section, key, default, path = struct.unpack('<4I', machine.mem_read(sp + 4, 16))
        name = (string(section).lower(), string(key).lower())
        value = context['profile'].get(name)
        # Only ordinary decimal values are supplied for the integer key in this probe.
        result = int(value) & 0xffffffff if value else default
        if name == (b'defaultset', b'housebuymoney'): result = 1
        machine.reg_write(UC_X86_REG_EAX, result)
cpu.hook_add(UC_HOOK_CODE, hook)
def run(data):
    context['profile'] = {(a.encode(), b.encode()): v for (a,b),v in profile(data).items()}
    cpu.mem_write(0x541b14, initial)
    cpu.mem_write(stack + 0x8000, struct.pack('<I', stop))
    cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
    cpu.emu_start(0x4319a0, 0x431ea7, count=1000000)
    if cpu.reg_read(UC_X86_REG_EIP) != 0x431ea7: raise RuntimeError('Original loader did not reach NPC-level loop')
    return (struct.unpack('<I', cpu.mem_read(0x541b14,4))[0],
            struct.unpack('<56H', cpu.mem_read(0x541b78,112)),
            struct.unpack('<48H', cpu.mem_read(0x541b18,96)))

base = run(b'')
assert base[0] == 2000 and base[1][1:4] == (1000,1500,2000) and base[2][1:4] == (200,500,800), base
sample = b'''[DefaultSet]\nHouseKeepEtin=3000\n[GateKeeper]\nUpgradeEtin=1250,77\nKeepEtin=321\n[ShopWeapon]\nUpgradeEtin=1,2,3\n[ShopDefen]\nUpgradeEtin=4,5,6\n[ShopPosion]\nUpgradeEtin=7,8,9\nKeepEtin=10,11,12\n[Bank]\nUpgradeEtin=13,14,15,16,17\nKeepEtin=18,19,20,21,22\n[General]\nUpgradeEtin=150,200,400\n[Smith]\nUpgradeEtin=23,24,25,26,27\nKeepEtin=100,200,500,800,1500\n'''
value = run(sample)
assert value[0] == 3000
for row, expected in ((0,(1250,77,2000)),(1,(1,2,3)),(2,(4,5,6)),(3,(150,200,400)),(4,(23,24,25,26,27)),(5,(7,8,9)),(6,(13,14,15,16,17))):
    assert value[1][row*7+1:row*7+1+len(expected)] == expected, (row,value[1])
for row, expected in ((0,(321,500,800)),(4,(100,200,500,800,1500)),(5,(10,11,12)),(6,(18,19,20,21,22))):
    assert value[2][row*6+1:row*6+1+len(expected)] == expected, (row,value[2])
value = run(b'[GateKeeper]\nUpgradeEtin=-1,65537,bad,99\nKeepEtin={7=8} 9\n')
assert value[1][1:4] == (65535,1,0) and value[2][1:4] == (7,8,9), value
print(f'Original guild house loader and CRT parser: 3 cases passed; ps_game.exe SHA256 {digest}')
