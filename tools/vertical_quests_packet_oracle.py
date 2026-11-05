#!/usr/bin/env python3
"""Observe original ps_game PacketQuest dispatch and its synchronous boundaries."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                               UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP)
from original_oracle import load_game

cpu, digest = load_game()
base = 0x30000000
cpu.mem_map(base, 0x20000)
user, packet, quest, stack, stop = (base, base + 0x8000, base + 0x9000,
                                   base + 0x1e000, base + 0x1f000)
state = {}

def word(at):
    return struct.unpack('<I', cpu.mem_read(at, 4))[0]

def hook(machine, at, size, data):
    sp = machine.reg_read(UC_X86_REG_ESP)
    a = machine.reg_read(UC_X86_REG_EAX)
    c = machine.reg_read(UC_X86_REG_ECX)
    d = machine.reg_read(UC_X86_REG_EDX)
    if at == 0x44b3d0:
        state['events'].append(('cancel', a))
    elif at == 0x478520:
        state['events'].append(('shop', c))
        if state['mutate']:
            cpu.mem_write(packet, struct.pack('<H', state['mutate']))
    elif at == 0x47f220:
        state['events'].append(('start', c, a, d & 0xffff))
    elif at == 0x47f2a0:
        state['events'].append(('end', word(sp+4), word(sp+8), a & 0xffff))
    elif at == 0x47ebe0:
        state['events'].append(('select', d, a & 0xffff, c & 0xff))
    elif at == 0x47e4b0:
        state['events'].append(('find', word(sp+4)))
        machine.reg_write(UC_X86_REG_EAX, quest if state['active'] else 0)
    elif at == 0x47ec40:
        state['events'].append(('failure', c, d, word(sp+4), word(sp+8)))
    elif at == 0x4d43a0:
        state['events'].append(('close', c, word(sp+4), word(sp+8)))

# Only external calls are intercepted; the original PacketQuest jump table and
# instruction bodies run unchanged in the emulator's private copy of the PE.
for address, cleanup in ((0x44b3d0, 0), (0x478520, 0), (0x47f220, 0),
                         (0x47f2a0, 8), (0x47ebe0, 0), (0x47e4b0, 4),
                         (0x47ec40, 8), (0x4d43a0, 8)):
    cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
cpu.hook_add(UC_HOOK_CODE, hook)
print('# ps_game.exe SHA256', digest, 'CUser::PacketQuest 0046be10')
for opcode, active, mutate in ((0x902, 0, 0), (0x904, 0, 0), (0x903, 0, 0),
                               (0x907, 0, 0), (0x908, 1, 0), (0x908, 0, 0),
                               (0x900, 0, 0), (0x901, 0, 0), (0x905, 0, 0),
                               (0x906, 0, 0), (0x909, 0, 0), (0x9ff, 0, 0),
                               (0x906, 0, 0x908)):
    cpu.mem_write(packet, struct.pack('<HIH', opcode, 0x78563412, 0xabcd))
    cpu.mem_write(stack, struct.pack('<I', stop))
    state.update(events=[], active=active, mutate=mutate)
    for reg, value in ((UC_X86_REG_ESP, stack), (UC_X86_REG_EAX, user), (UC_X86_REG_ESI, packet)):
        cpu.reg_write(reg, value)
    cpu.emu_start(0x46be10, stop, count=1000)
    assert cpu.reg_read(UC_X86_REG_EIP) == stop
    expected = [('cancel', user), ('shop', user + 0x15dc)]
    effective = mutate or opcode
    if effective in (0x902, 0x904):
        expected.append(('start', user, 0x78563412, 0xabcd))
    elif effective == 0x903:
        expected.append(('end', user, 0x78563412, 0xabcd))
    elif effective == 0x907:
        expected.append(('select', user, 0x3412, 0x56))
    elif effective == 0x908:
        expected.append(('find', 0x3412))
        if active:
            expected.append(('failure', user, quest, 0, 0))
    else:
        expected.append(('close', user, 9, 0))
    assert state['events'] == expected, (hex(opcode), state['events'], expected)
    print(f'{opcode:04x} active={active} mutate={mutate:04x}', state['events'])
