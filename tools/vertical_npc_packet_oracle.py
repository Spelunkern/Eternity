#!/usr/bin/env python3
"""Observe original PacketNpc and its required NPC lookup/close boundaries."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESI,
                               UC_X86_REG_ESP, UC_X86_REG_EIP)
from original_oracle import load_game

cpu, digest = load_game()
base = 0x30000000
cpu.mem_map(base, 0x20000)
user, zone, old, target, packet, stack, stop = (base, base+0x4000, base+0x5000,
                                                  base+0x6000, base+0x7000,
                                                  base+0x1e000, base+0x1f000)
state = {}

def put(at, value):
    cpu.mem_write(at, struct.pack('<I', value & 0xffffffff))

def get(at):
    return struct.unpack('<I', cpu.mem_read(at, 4))[0]

def hook(machine, at, size, data):
    sp = machine.reg_read(UC_X86_REG_ESP)
    if at == 0x416d10:
        state['events'].append(('find', machine.reg_read(UC_X86_REG_EAX), get(sp+4)))
        machine.reg_write(UC_X86_REG_EAX, state['result'])
    elif at == 0x4d43a0:
        state['events'].append(('close', machine.reg_read(UC_X86_REG_ECX), get(sp+4), get(sp+8)))

for address, cleanup in ((0x416d10, 4), (0x4d43a0, 8)):
    cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
cpu.hook_add(UC_HOOK_CODE, hook)
print('# ps_game.exe SHA256', digest, 'CUser::PacketNpc 00467510')
for opcode, previous, result, old_count, target_count in (
        (0xe04, 0, target, 0, 0),
        (0xe04, old, target, 1, 0),
        (0xe04, target, target, 0, 7),
        (0xe04, old, 0, 1, 0),
        (0xe04, old, target, 0x80000000, 0x7fffffff),
        (0xe05, old, target, 1, 2)):
    cpu.mem_write(packet, struct.pack('<HI', opcode, 0x78563412))
    put(user+0xe0, zone);put(user+0x1564, previous)
    put(old+0x40, old_count);put(target+0x40, target_count);put(stack, stop)
    state.update(events=[], result=result)
    for reg, value in ((UC_X86_REG_ESP, stack), (UC_X86_REG_EAX, packet), (UC_X86_REG_ESI, user)):
        cpu.reg_write(reg, value)
    cpu.emu_start(0x467510, stop, count=1000)
    assert cpu.reg_read(UC_X86_REG_EIP) == stop
    if opcode != 0xe04:
        expected_events=[('close', user, 9, 0)];new_previous=previous
    else:
        expected_events=[('find', zone, 0x78563412)]
        new_previous=result if result and result!=previous else previous
        if result and result!=previous:
            if previous==old:old_count=(old_count-1)&0xffffffff
            if previous==target:target_count=(target_count-1)&0xffffffff
            if result==old:old_count=(old_count+1)&0xffffffff
            if result==target:target_count=(target_count+1)&0xffffffff
    assert state['events']==expected_events
    assert (get(user+0x1564),get(old+0x40),get(target+0x40))==(new_previous,old_count,target_count)
    print(f'{opcode:04x} previous={previous:08x} result={result:08x}', state['events'],
          f'interaction={new_previous:08x} old={old_count:08x} target={target_count:08x}')
