#!/usr/bin/env python3
"""Trace the original UpdateIdle ordinary targeting and movement gates."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game

cpu, digest = load_game()
stack, mob, definition, stop = 0x20000000, 0x30000000, 0x31000000, 0x10000000
for address in (stack, mob, definition, stop):
    cpu.mem_map(address, 0x10000)
def put(address, *values):
    cpu.mem_write(address, struct.pack('<' + 'I' * len(values), *values))
def get(address):
    return struct.unpack('<I', cpu.mem_read(address, 4))[0]
state = {}
def hook(machine, address, size, context):
    if address == 0x41d760:
        machine.reg_write(UC_X86_REG_EAX, 0)
    elif address == 0x49fcf0:
        state['target_calls'] += 1
        machine.reg_write(UC_X86_REG_EAX, state['found'])
    elif address == 0x4a7f77:
        state['moves'] += 1
        machine.reg_write(UC_X86_REG_EIP, 0x4a81fa)
for address in (0x41d760, 0x49fcf0):
    cpu.mem_write(address, b'\xc3')
    cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
cpu.hook_add(UC_HOOK_CODE, hook, begin=0x4a7f77, end=0x4a7f77)
print('# ps_game.exe SHA256 ' + digest + '; CMob::UpdateIdle 004a7e60; status/Targeting stubbed, movement intercepted')
print('# now next_target next_move obelisk found | target_calls moves resulting_next_target')
for now, target, move, obelisk, found in ((50,51,50,0,0),(50,50,50,0,1),(50,0,50,1,0),(50,0,51,0,0),(50,0,50,0,0),(50,50,50,0,0)):
    cpu.mem_write(mob, bytes(0x1000))
    cpu.mem_write(definition, bytes(0x1000))
    put(mob+0xa8, definition)
    put(mob+0xcc8, target)
    put(mob+0xcbc, move)
    cpu.mem_write(definition+0x7c, bytes([obelisk]))
    state.update(target_calls=0,moves=0,found=found)
    put(stack+0x8000, stop, mob, now)
    cpu.reg_write(UC_X86_REG_ESP, stack+0x8000)
    cpu.emu_start(0x4a7e60, stop, count=10000)
    if cpu.reg_read(UC_X86_REG_EIP) != stop:
        raise RuntimeError('UpdateIdle did not return')
    print(now,target,move,obelisk,found,state['target_calls'],state['moves'],get(mob+0xcc8),sep='\t')
