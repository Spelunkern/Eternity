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
initial = bytes(cpu.mem_read(0x541b10, 0xd8))
cpu.mem_write(0x4d9510, b"\xc3")
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
    if address == 0x4d9510:
        context['diagnostics'] += 1
    elif address == stop + 0x100:
        section, key, default, target, capacity, path = struct.unpack('<6I', machine.mem_read(sp + 4, 24))
        value = context['profile'].get((string(section).lower(), string(key).lower()), string(default))[:capacity-1]
        machine.mem_write(target, value + b'\0'); machine.reg_write(UC_X86_REG_EAX, len(value))
    elif address == stop + 0x200:
        section, key, default, path = struct.unpack('<4I', machine.mem_read(sp + 4, 16))
        name = (string(section).lower(), string(key).lower())
        value = context['profile'].get(name)
        # Only ordinary decimal values are supplied for the integer key in this probe.
        result = int(value) & 0xffffffff if value else default
        machine.reg_write(UC_X86_REG_EAX, result)
cpu.hook_add(UC_HOOK_CODE, hook)
def run(data):
    context['profile'] = {(a.encode(), b.encode()): v for (a,b),v in profile(data).items()}
    context['diagnostics'] = 0
    cpu.mem_write(0x541b10, initial)
    cpu.mem_write(stack + 0x8000, struct.pack('<I', stop))
    cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
    cpu.emu_start(0x4319a0, 0x431ea7, count=1000000)
    if cpu.reg_read(UC_X86_REG_EIP) != 0x431ea7: raise RuntimeError('Original loader did not reach NPC-level loop')
    return (struct.unpack('<I', cpu.mem_read(0x541b10,4))[0], struct.unpack('<I', cpu.mem_read(0x541b14,4))[0], context['diagnostics'])

for data,expected in ((b'',(50000000,2000,1)),(b'[DefaultSet]\nHouseBuyMoney=12345\nHouseKeepEtin=4321\n',(12345,4321,0)),(b'[DefaultSet]\nHouseBuyMoney=0\n',(0,2000,1)),(b'[DefaultSet]\nHouseBuyMoney=-1\n',(0xffffffff,2000,1)),(b'[DefaultSet]\nHouseBuyMoney=2147483647\n',(2147483647,2000,0))):
    result=run(data);assert result==expected,(data,result,expected)
print(f'5 original guild house purchase settings cases passed; ps_game.exe SHA256 {digest}')
