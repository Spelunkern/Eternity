#!/usr/bin/env python3
"""Execute immutable ps_game.exe event handlers; observe tick/Lua boundaries."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EBX, UC_X86_REG_EDI, UC_X86_REG_EIP
from original_oracle import load_game

cpu, digest = load_game()
base = 0x30000000
cpu.mem_map(base, 0x10000)
mob, event, stack, stop = base, base+0x2000, base+0x8000, base+0xf000
addresses = [0x4b60d0, 0x4b60e0, 0x4b60f0, 0x4b6120, 0x4b6170, 0x4b61c0, 0x4b6220, 0x4b6270]
offsets = [0x284, 0x290, 0x280, 0x130, 0x2a0, 0x2b4, 0x28c]
def put(at, value): cpu.mem_write(at, struct.pack('<I', value & 0xffffffff))
def get(at): return struct.unpack('<I', cpu.mem_read(at, 4))[0]
def fields(): return [get(mob+x) for x in offsets]
put(0x5210e4, stop+16)
cpu.mem_write(stop+16, b'\xc3')
for address, count in [(0x4b1090, 8), (0x4b1140, 12)]:
    cpu.mem_write(address, b'\xc2'+struct.pack('<H', count))
context = {}
def hook(machine, address, size, data):
    if address == stop+16:
        context['ticks'] += 1
        machine.reg_write(UC_X86_REG_EAX, context['tick'])
    if address in (0x4b1090, 0x4b1140):
        sp = machine.reg_read(UC_X86_REG_ESP)
        name_at = machine.reg_read(UC_X86_REG_EBX)
        name = bytes(machine.mem_read(name_at, 32)).split(b'\0')[0].decode()
        assert get(sp+4) == mob and machine.reg_read(UC_X86_REG_EDI) == get(mob+0x27c)
        context['call'] = [name, get(sp+8), int(address == 0x4b1140), get(sp+12) if address == 0x4b1140 else 0] + fields()
        if context['mutate']:
            for i, offset in enumerate(offsets): put(mob+offset, 100+i)
cpu.hook_add(UC_HOOK_CODE, hook)
print(f'# ps_game.exe SHA256 {digest}; CMob event handlers 004b60d0..004b6270; tick and LuaCall are observed providers.')
print('# event mutate argument tick input[7] ticks function call_tick has_arg argument observed[7] final[7]')
for kind, address in enumerate(addresses):
    for scenario in range(64):
        values = [ [0,1,0xffffffff,2][scenario%4], scenario%3, 0x80000000+scenario, scenario+1, scenario+2, scenario+3, 0xffffffff-scenario ]
        for offset, value in zip(offsets, values): put(mob+offset, value)
        put(mob+0x27c, base+0x3000)
        argument, tick = 0x80000000+scenario, (0xfffffff0+scenario)&0xffffffff
        put(event+8, argument)
        put(stack, stop); put(stack+4, event)
        for reg, value in [(UC_X86_REG_ECX,mob),(UC_X86_REG_ESI,mob),(UC_X86_REG_ESP,stack)]: cpu.reg_write(reg,value)
        context.update(ticks=0, tick=tick, mutate=scenario%2, call=None)
        cpu.emu_start(address, stop, count=1000)
        assert cpu.reg_read(UC_X86_REG_EIP) == stop
        assert cpu.reg_read(UC_X86_REG_ESP) == stack + (4 if kind == 2 else 8)
        call = context['call'] or ['-', 0, 0, 0]+[0]*7
        print(kind, scenario%2, argument, tick, *values, context['ticks'], *call, *fields())
