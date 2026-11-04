#!/usr/bin/env python3
"""Execute original mob/quest index construction and kill-counter delivery."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EDX, UC_X86_REG_ESI
from original_oracle import load_game, ROOT


def quest_block(records):
    regions = [(0x102, 14), (0x110, 7), (0x118, 9), (0x124, 37), (0x14a, 14), (0x15a, 1), (0x158, 2), (0x15b, 3)]
    for i in range(3):
        offset = 0x160 + i * 32
        regions += [(offset, 5), (offset + 8, 9), (offset + 20, 12)]
    block = struct.pack('<I', len(records))
    for record in records:
        block += bytes(record[:2]) + bytes(8)  # empty name/description
        block += b''.join(record[offset:offset + size] for offset, size in regions)
        block += bytes(28)  # seven empty strings
    return block


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--sql-mobs', type=Path, help='Optional read-only SQL definition snapshot for full distribution index')
    args = parser.parse_args()
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x40000)
    cpu.mem_map(0, 0x10000)
    stop, stack = base + 0x3f000, base + 0x3e000
    user, owner, mob, head, nodes, definitions, mobs = [base + i for i in (0, 0x6000, 0x6100, 0x7000, 0x8000, 0x10000, 0x18000)]
    state = {}

    def put(at, *values):
        cpu.mem_write(at, struct.pack('<' + 'I' * len(values), *[v & 0xffffffff for v in values]))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    for at, cleanup in ((0x503ae3, 0), (0x4d4f10, 8), (0x4d4d20, 8), (0x47ec40, 8), (stop + 16, 4), (stop + 32, 0)):
        cpu.mem_write(at, b'\xc2' + struct.pack('<H', cleanup))
    put(0x5210cc, stop + 16)
    put(0x5210d0, stop + 16)
    put(0x5210e4, stop + 32)

    def hook(machine, address, size, data):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503ae3:
            at = state['heap']
            state['heap'] += (get(sp + 4) + 15) & ~15
            if state['heap'] >= state['heap_limit']:
                raise RuntimeError('Quest oracle heap exhausted')
            machine.reg_write(UC_X86_REG_EAX, at)
        elif address in (0x4d4f10, 0x4d4d20):
            n = get(sp + 8)
            state['events'] += (b'B' if address == 0x4d4f10 else b'C') + struct.pack('<I', n) + bytes(machine.mem_read(get(sp + 4), n))
        elif address == 0x47ec40:
            node = machine.reg_read(UC_X86_REG_EDX)
            state['events'] += b'E' + struct.pack('<III', get(node + 8), get(sp + 4), get(sp + 8))
            if state['remove'] and get(node) and get(node + 4):
                previous, following = get(node), get(node + 4)
                put(previous + 4, following)
                put(following, previous)
                put(node, 0, 0)
                state['active'][(node - nodes) // 64] = 0
        elif address == stop + 32:
            machine.reg_write(UC_X86_REG_EAX, state['now'])

    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, *arguments):
        put(stack, stop, *arguments)
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.reg_write(UC_X86_REG_EAX, user)
        cpu.emu_start(address, stop, count=10000000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError(f'Quest function {address:08x} did not return')

    print(f'# ps_game.exe SHA256 {digest}; complete CMobQuestData Init 004c9ed0 with real constructors/AddEnd, and CUser QuestUpdateKillMOB 00480140 with real QuestFind 0047e4b0. Ports: allocation, locks, clock, DB/client transport and QuestEnd (optional unlink retains object).')
    print('# L scenario mob_count valid_mask quest_block_hex expected_index_hex')
    for scenario in range(128):
        cpu.mem_write(base, bytes(0x3d000))
        count = (0, 1, 2, 8, 16)[scenario % 5]
        mask = (0, 0x5555, 0xffff)[(scenario // 5) % 3]
        records = []
        for i in range(8):
            record = bytearray(464)
            struct.pack_into('<H', record, 0, 700 + i)
            struct.pack_into('<H', record, 0x102, (0, 1, 10, 1, 65535)[(i + scenario) % 5])
            record[0x147] = (0, 2, 2, 3)[(i + scenario) % 4]
            first = (0, 1, 2, 7, 8, 16, 65535)[(i + scenario // 3) % 7]
            second = first if i % 2 else (0, 1, 2, 7)[(scenario + i) % 4]
            struct.pack_into('<HH', record, 0x156, first, second)
            records.append(record)
        put(0xfcd2ac, count, mobs)
        put(0x3ed35c8, len(records), definitions)
        for i in range(count):
            cpu.mem_write(mobs + i * 268, struct.pack('<H', i))
            cpu.mem_write(mobs + i * 268 + 34, struct.pack('<H', 1 if mask >> i & 1 else 0))
            put(mobs + i * 268 + 48, 1)
        for i, record in enumerate(records):
            cpu.mem_write(definitions + i * 464, bytes(record))
        state.update(heap=base + 0x20000, heap_limit=base + 0x3d000)
        call(0x4c9ed0, owner)
        if cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
            raise RuntimeError('Original quest index failed')
        result = b''
        for i in range(count):
            at = get(owner + 4) + i * 16
            n = get(at + 8)
            result += struct.pack('<I', n) + (bytes(cpu.mem_read(get(at + 12), n * 2)) if n else b'')
        print('L', scenario, count, mask, quest_block(records).hex(), result.hex() or '-', sep='\t')

    print('# K scenario have_quests links_present mob_id now completion_unlinks quest_block_hex initial_state_hex final_state_hex events_hex')
    for scenario in range(512):
        cpu.mem_write(base, bytes(0x3d000))
        have = (-1, 0, 1, 3)[scenario % 4]
        linked = 0 if scenario % 11 == 0 else 1
        mob_id = (0, 1, 2, 7, 8, 65535)[(scenario // 4) % 6]
        now = (0, 1000, 0xffffffff)[(scenario // 7) % 3]
        remove = (scenario // 13) % 2
        put(user + 0x57c8, have)
        put(user + 0x578c, 7)
        put(user + 0x11f4, head, head)
        put(head, nodes + 3 * 64, nodes)
        put(0x3fd35d0, 8, owner if linked else 0)
        ids = [1, 2, 1, 999, 3, 4]
        for i in range(8):
            put(owner + i * 16 + 8, len(ids), base + 0x7100)
        cpu.mem_write(base + 0x7100, struct.pack('<6H', *ids))
        cpu.mem_write(mob + 0x220, struct.pack('<H', mob_id))
        records, initial = [], b''
        for i in range(4):
            record = bytearray(464)
            struct.pack_into('<H', record, 0, 700 + i)
            record[0x147] = (2, 2, 0, 3)[(i + scenario // 17) % 4]
            record[0x148] = (scenario // 19 + i) % 2
            struct.pack_into('<I', record, 0x124, (scenario // 23 + i) % 2)
            target = mob_id if scenario % 3 else 1
            struct.pack_into('<HH', record, 0x156, target, target if (scenario + i) % 2 else 2)
            record[0x15a:0x15c] = bytes([(0, 1, 254, 255)[(scenario // 29 + i) % 4], (0, 1, 254, 255)[(scenario // 31 + i) % 4]])
            records.append(record)
            cpu.mem_write(definitions + i * 464, bytes(record))
            node = nodes + i * 64
            put(node, nodes + (i - 1) * 64 if i else head, nodes + (i + 1) * 64 if i < 3 else head, i + 1)
            success = (0, 999, 1000, 0xffffffff)[(scenario // 37 + i) % 4]
            counters = bytes([(0, 1, 254, 255)[(scenario // 41 + i) % 4], (0, 1, 254, 255)[(scenario // 43 + i) % 4], 123])
            put(node + 0x18, success)
            cpu.mem_write(node + 0x1c, counters)
            put(node + 0x20, definitions + i * 464)
            initial += struct.pack('<I', success) + counters
        state.update(now=now, events=b'', remove=remove, active=[1] * 4)
        call(0x480140, mob)
        final = b''.join(bytes([state['active'][i]]) + bytes(cpu.mem_read(nodes + i * 64 + 0x18, 7)) for i in range(4))
        print('K', scenario, have, linked, mob_id, now, remove, quest_block(records).hex(), initial.hex(), final.hex(), state['events'].hex() or '-', sep='\t')

    if args.sql_mobs:
        original = (ROOT / '.local/server-data/NpcQuest.SData').read_bytes()
        source = next(line.split() for line in (ROOT / 'tests/data/quests-oracle.tsv').read_text().splitlines() if line.startswith('A\t'))
        if hashlib.sha256(original).hexdigest() != source[1]:
            raise RuntimeError('Quest distribution changed since original loader verification')
        data = original[int(source[2]):]
        cpu.mem_map(0x40000000, 0x4000000)
        cpu.mem_map(0x50000000, 0x1000000)
        state.update(heap=0x40000000, heap_limit=0x44000000, position=0)
        for at in (0x505316, 0x510785):
            cpu.mem_write(at, b'\xc3')

        def read_file(machine, address, size, context):
            sp = machine.reg_read(UC_X86_REG_ESP)
            target, amount = get(sp + 8), get(sp + 12)
            chunk = data[state['position']:state['position'] + amount]
            if len(chunk) != amount:
                raise RuntimeError('Original quest loader reached truncated input')
            machine.mem_write(target, chunk)
            state['position'] += amount
            machine.reg_write(UC_X86_REG_EAX, amount)

        cpu.hook_add(UC_HOOK_CODE, read_file, begin=0x510785, end=0x510785)
        put(base + 0x6500, 12)
        cpu.reg_write(UC_X86_REG_ESI, 0x3ed35c8)
        call(0x4ca5d0, base + 0x6500)
        if cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
            raise RuntimeError('Original full quest loader failed')
        snapshot = json.loads(args.sql_mobs.read_text())
        count = snapshot['count']
        put(0xfcd2ac, count, 0x50000000)
        for row in snapshot['rows']:
            identity = (row['MobID'] or 0) & 65535
            at = 0x50000000 + identity * 268
            cpu.mem_write(at, struct.pack('<H', identity))
            cpu.mem_write(at + 34, struct.pack('<H', (row['Level'] or 0) & 65535))
            put(at + 48, row['HP'] if row['HP'] else 1)
        call(0x4c9ed0, owner)
        if cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
            raise RuntimeError('Original full mob quest index failed')
        result = b''
        for i in range(count):
            at = get(owner + 4) + i * 16
            n = get(at + 8)
            result += struct.pack('<I', n) + (bytes(cpu.mem_read(get(at + 12), n * 2)) if n else b'')
        digest_index = 0xcbf29ce484222325
        for byte in result:
            digest_index = ((digest_index ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        print('A', count, get(0x3ed35c8), hashlib.sha256(original).hexdigest(), len(result), digest_index, sep='\t')


if __name__ == '__main__':
    main()
