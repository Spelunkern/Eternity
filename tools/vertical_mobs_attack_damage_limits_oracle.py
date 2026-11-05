#!/usr/bin/env python3
"""Trace the original normal mob damage cap and minimum-damage roll."""
import random
import struct

from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

cpu, digest = load_game()
stack, damage, stop = 0x20000000, 0x30000000, 0x10000000
for address in (stack, damage, stop):
    cpu.mem_map(address, 0x10000)
state = {}

def hook(machine, address, size, context):
    if address == 0x504697:
        state['draws'] += 1
        sp = machine.reg_read(UC_X86_REG_ESP)
        ret, = struct.unpack('<I', machine.mem_read(sp, 4))
        machine.reg_write(UC_X86_REG_EAX, state['roll'])
        machine.reg_write(UC_X86_REG_ESP, sp + 4)
        machine.reg_write(UC_X86_REG_EIP, ret)

cpu.hook_add(UC_HOOK_CODE, hook, begin=0x504697, end=0x504697)
rng = random.Random(0x4a2fe9)
boundary = (-2147483648, -11, -1, 0, 1, 9, 10, 11, 29999, 30000, 30001, 2147483647)
print('# ps_game.exe SHA256 ' + digest + '; CMob::AttackDamage 004a2fe9..004a3026; CRT random intercepted')
print('# primary secondary tertiary roll | primary secondary tertiary draws')
for i in range(256):
    values = [rng.choice(boundary) for _ in range(3)]
    roll = rng.randrange(32768)
    cpu.mem_write(damage, struct.pack('<iii', *values))
    cpu.reg_write(UC_X86_REG_EBP, damage)
    cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
    state.update(roll=roll, draws=0)
    cpu.emu_start(0x4a2fe9, 0x4a3026, count=1000)
    result = struct.unpack('<iii', cpu.mem_read(damage, 12))
    print(*values, roll, *result, state['draws'], sep='\t')
