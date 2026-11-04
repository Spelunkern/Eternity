#!/usr/bin/env python3
"""Execute original request -> check -> item consumption, observing ownership/QuestEnd."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
from mob_quest_oracle import quest_block

cpu, digest = load_game()
base = 0x30000000
cpu.mem_map(base, 0x40000)
user, node, definition, npc, items, info = [base + n for n in (0, 0x6000, 0x7000, 0x8000, 0x9000, 0x13000)]
stack, stop = base + 0x3e000, base + 0x3f000
state = {}
def put(at, *values):
    cpu.mem_write(at, struct.pack('<' + 'I' * len(values), *values))
def get(at):
    return struct.unpack('<I', cpu.mem_read(at, 4))[0]
for at, cleanup in ((0x47e4b0, 4), (0x416d10, 4), (0x47ec40, 8), (0x4d4f10, 8), (0x4d4d20, 8), (0x40a590, 0), (stop + 16, 4)):
    cpu.mem_write(at, b'\xc2' + struct.pack('<H', cleanup))
put(0x5210cc, stop + 16)
put(0x5210d0, stop + 16)
def hook(machine, at, size, data):
    sp = machine.reg_read(UC_X86_REG_ESP)
    if at == 0x47e4b0:
        state['events'] += b'A' + struct.pack('<I', get(sp + 4))
        machine.reg_write(UC_X86_REG_EAX, node if state['active'] else 0)
    elif at == 0x416d10:
        state['events'] += b'N' + struct.pack('<I', get(sp + 4))
        machine.reg_write(UC_X86_REG_EAX, npc if state['found'] else 0)
    elif at in (0x4d4f10, 0x4d4d20):
        state['events'] += (b'B' if at == 0x4d4f10 else b'C') + bytes(machine.mem_read(get(sp + 4), get(sp + 8)))
    elif at in (0x40a590, 0x47fccd):
        item = machine.reg_read(UC_X86_REG_EDI)
        slot = (item - items) // 128
        assert get(user + 0x224 + 4 * slot) == item
        state['events'] += b'F' + bytes((slot // 24 + 1, slot % 24, machine.mem_read(item + 0x42, 1)[0]))
    elif at == 0x47ec40:
        assert machine.reg_read(UC_X86_REG_EDX) == node
        state['events'] += b'E' + struct.pack('<II', get(sp + 4), get(sp + 8)) + bytes(machine.mem_read(node + 0x1c, 3))
cpu.hook_add(UC_HOOK_CODE, hook)
rng = random.Random(0x47f2a0)
h = lambda b: bytes(b).hex() or '-'
print('# ps_game.exe SHA256', digest, 'real QuestEndRequest 0047f2a0, checks 0047f630/670/6e0/740 and QuestFindItem 0047f8e0. Ports QuestFind, FindNpc, transport, locks, release and QuestEnd; F observes original retained item count before release, including inline effect-27 release 0047fccd.')
print('# case id npc active found npc_type npc_id bags definition_block input_inventory input_counters final_inventory final_counters events')
for case in range(512):
    cpu.mem_write(base, bytes(0x30000))
    state.update(active=case % 17 != 0, found=case % 13 != 0, events=b'')
    d = bytearray(464)
    struct.pack_into('<H', d, 0, 1)
    d[0x147] = 1 if case % 8 < 6 else (0, 2)[case % 2]
    d[0x148] = (case // 2) % 2
    struct.pack_into('<H', d, 0x14a, 12)
    d[0x15c] = (case // 4) % 3
    d[0x15d] = (case // 12) % 2
    d[0x15a:0x15c] = bytes((2, 3))
    for i in range(3):
        d[0x14c + 3*i:0x14f + 3*i] = bytes((1, i + 1 if case % 11 else 1, rng.choice((0, 1, 2, 7, 250))))
        d[0x163 + 32*i:0x165 + 32*i] = bytes((rng.randrange(4), rng.randrange(10)))
    counters = bytes(rng.choice((0, 1, 7, 255)) for _ in range(3))
    bags = (0, 1, 2, 5)[case % 4]
    requested = 0xabcdef01 if case % 3 else 0
    nt, ni = d[0x148] if case % 19 else 99, 12 if case % 23 else 13
    put(npc + 0x34, nt, ni)
    put(node + 0x20, definition)
    cpu.mem_write(node + 0x1c, counters)
    put(0x3ed35c8, 1, definition)
    cpu.mem_write(definition, bytes(d))
    put(user + 0x1304, bags)
    put(user + 0x578c, 0x12345678)
    put(0x546f3c, base + 0x22000)
    inventory = bytearray()
    for slot in range(bags * 24):
        present = slot < 8 or rng.randrange(5) == 0
        record = bytes((1, 1, slot % 4 + 1, rng.choice((1, 2, 7, 255)), rng.choice((0, 0, 27)))) if present else bytes(5)
        inventory += record
        if present:
            item = items + slot * 128
            put(user + 0x224 + 4 * slot, item)
            put(item + 0x30, info + slot * 256)
            put(info + slot * 256 + 0xa0, record[4])
            cpu.mem_write(item + 0x40, record[1:4])
    ident = 0 if case % 29 == 0 else 1
    put(stack, stop, user, requested)
    cpu.reg_write(UC_X86_REG_ESP, stack)
    cpu.reg_write(UC_X86_REG_EAX, ident)
    cpu.emu_start(0x47f2a0, stop, count=1000000)
    assert cpu.reg_read(UC_X86_REG_EIP) == stop
    final = bytearray()
    for slot in range(bags * 24):
        item = get(user + 0x224 + 4 * slot)
        final += bytes((1,)) + bytes(cpu.mem_read(item + 0x40, 3)) + bytes((inventory[slot*5 + 4],)) if item else bytes(5)
    print(case, ident, requested, int(state['active']), int(state['found']), nt, ni, bags, h(quest_block([d])), h(inventory), h(counters), h(final), h(cpu.mem_read(node + 0x1c, 3)), h(state['events']))
