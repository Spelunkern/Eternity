#!/usr/bin/env python3
"""Execute original active/finished quest removal, shared cursor and CQuest heap."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
from mob_quest_oracle import quest_block


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x40000)
    user, active_head, finished_head, manager, nodes, definitions = [base + n for n in (0, 0x7000, 0x7100, 0x8000, 0x9000, 0x10000)]
    stack, stop = base + 0x3e000, base + 0x3f000
    state = {}

    def put(at, *values):
        cpu.mem_write(at, struct.pack('<' + 'I' * len(values), *[v & 0xffffffff for v in values]))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    for at, cleanup in ((0x503a3f, 0), (0x4d4f10, 8), (stop + 16, 4)):
        cpu.mem_write(at, b'\xc2' + struct.pack('<H', cleanup))
    for at in (0x5210cc, 0x5210d0):
        put(at, stop + 16)
    put(0x546f3c, manager)

    def identity(pointer):
        return (pointer - nodes) // 64 if pointer else -1

    def payload(pointer):
        definition = get(pointer + 0x20)
        index = (definition - definitions) // 464 + 1 if definition else 0
        return struct.pack('<HBIII3BH', get(pointer + 8) & 65535, cpu.mem_read(pointer + 12, 1)[0],
                           get(pointer + 16), get(pointer + 20), get(pointer + 24),
                           *cpu.mem_read(pointer + 28, 3), index)

    def sequence(head):
        result, node = [], get(head + 4)
        while node != head:
            result.append(identity(node))
            node = get(node + 4)
            if len(result) > 32:
                raise RuntimeError('Quest list did not terminate')
        return result

    def hook(machine, address, size, data):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503a3f:
            if get(sp + 4) != 36:
                raise RuntimeError('Unexpected CQuest allocation size')
            if not state['allocate']:
                machine.reg_write(UC_X86_REG_EAX, 0)
                return
            pointer = nodes + state['allocated'] * 64
            state['allocated'] += 1
            machine.mem_write(pointer, bytes(36))
            machine.reg_write(UC_X86_REG_EAX, pointer)
        elif address == 0x4d4f10:
            n = get(sp + 8)
            state['events'] += b'B' + struct.pack('<I', n) + bytes(machine.mem_read(get(sp + 4), n))
            if state['reentry']:
                selected = active_head
                for index in sequence(active_head):
                    node = nodes + index * 64
                    if get(node + 8) == state['reentry']:
                        selected = node
                        break
                put(user + 0x11f8, selected)
                state['events'] += b'F' + struct.pack('<i', identity(selected) if selected != active_head else -1)

    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, *arguments, eax=0, ecx=0, edx=0, esi=0, edi=0):
        put(stack, stop, *arguments)
        for register, value in ((UC_X86_REG_ESP, stack), (UC_X86_REG_EAX, eax), (UC_X86_REG_ECX, ecx),
                                (UC_X86_REG_EDX, edx), (UC_X86_REG_ESI, esi), (UC_X86_REG_EDI, edi)):
            cpu.reg_write(register, value)
        cpu.emu_start(address, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Quest ownership operation did not return')
        return cpu.reg_read(UC_X86_REG_EAX)

    def text_ids(values):
        return ','.join(str(value) for value in values) or '-'

    def snapshot():
        if get(user + 0x1214) != len(sequence(active_head)) or get(user + 0x11e8) != len(sequence(finished_head)):
            raise RuntimeError('Original quest list count differs from linked membership')
        free, current = [], get(manager + 0xf0)
        while current:
            free.append(identity(current))
            current = get(current + 4)
            if len(free) > 32:
                raise RuntimeError('Quest heap did not terminate')
        cursor = get(user + 0x11f8)
        print('S', text_ids(sequence(active_head)), text_ids(sequence(finished_head)), text_ids(free),
              identity(cursor) if cursor != active_head else -1, get(user + 0x57c8), get(user + 0x57c4),
              get(manager + 0xf4), b''.join(payload(nodes + i * 64) for i in range(state['allocated'])).hex(),
              state['events'].hex() or '-', sep='\t')

    rng = random.Random(0x47e150)
    print(f'# ps_game.exe SHA256 {digest}; complete QuestRemove 0047e150, QuestFind 0047e4b0, list Front/Next 0044bbc0/0044bbe0, actual FreeQuest 0040a820 and heap Alloc 0040b0b0. Ports: zeroed raw allocation, locks and DB transport. Transport callback optionally performs a reentrant active lookup; seeded admission/payload is explicit.')
    print('# I episode account initial_mob_count initial_pc_count quest_block initial_payloads; R identity success reentrant_id; F id result; T/N result; A allocation_allowed allocated_identity retained_payload; S active,finished,free_head_order,cursor,mob,pc,attempts,all_payloads,events')
    for episode in range(64):
        cpu.mem_write(base, bytes(0x3d000))
        state.update(allocated=7, events=b'', reentry=0, allocate=True)
        account = (7, 0x80000000, 0xffffffff)[episode % 3]
        mob_count = (0, 1, 0x80000000, 0xffffffff)[episode % 4]
        pc_count = (0, 1, 0x80000000, 0xffffffff)[(episode // 4) % 4]
        records = []
        for i in range(7):
            record = bytearray(464)
            struct.pack_into('<H', record, 0, 700 + i)
            struct.pack_into('<I', record, 0x134, (0, 1, 0x80000000)[(episode + i) % 3])
            record[0x147] = (0, 1, 2, 3, 255)[(episode + i) % 5]
            records.append(record)
            cpu.mem_write(definitions + i * 464, bytes(record))
        put(user + 0x578c, account)
        put(user + 0x57c8, mob_count)
        put(user + 0x57c4, pc_count)
        put(user + 0x11f4, active_head, active_head)
        put(user + 0x1214, 5)
        put(user + 0x11c8, finished_head, finished_head)
        put(user + 0x11e8, 2)
        put(manager + 0xf0, 0, 7, 0)
        for head, first, count in ((active_head, 0, 5), (finished_head, 5, 2)):
            put(head, nodes + (first + count - 1) * 64, nodes + first * 64)
            for i in range(first, first + count):
                node = nodes + i * 64
                put(node, nodes + (i - 1) * 64 if i > first else head,
                    nodes + (i + 1) * 64 if i + 1 < first + count else head)
                identity_id = 1 if episode % 7 == 0 and i in (0, 1) else i + 1
                put(node + 8, identity_id)
                cpu.mem_write(node + 12, bytes([(episode * 17 + i) & 255]))
                put(node + 16, 123 + i, 456 + i, 789 + i)
                cpu.mem_write(node + 28, bytes([(i * 17) & 255, (i * 31) & 255, 123]))
                put(node + 32, definitions + i * 464)
        print('I', episode, account, mob_count, pc_count, quest_block(records).hex(),
              b''.join(payload(nodes + i * 64) for i in range(7)).hex(), sep='\t')
        snapshot()
        remaining = list(range(5))
        rng.shuffle(remaining)
        for step, index in enumerate(remaining):
            wanted = rng.choice((1, 3, 5, 999))
            result = call(0x47e4b0, wanted, edi=user)
            print('F', wanted, identity(result), sep='\t')
            state['events'] = b''
            snapshot()
            result = call(0x44bbc0, eax=user + 0x11f4)
            print('T', identity(result), sep='\t')
            snapshot()
            for _ in range((episode + step) % 3):
                result = call(0x44bbe0, edx=user + 0x11f4)
                print('N', identity(result), sep='\t')
                snapshot()
            success = (0, 1, 2, 255)[(episode + step) % 4]
            reentry = (0, 1, 3, 999)[(episode // 3 + step) % 4]
            state.update(events=b'', reentry=reentry)
            call(0x47e150, success, eax=nodes + index * 64, ecx=user)
            print('R', index, success, reentry, sep='\t')
            snapshot()
            state.update(events=b'', reentry=0, allocate=(episode + step) % 3 != 0)
            allocated = call(0x40b0b0, esi=manager + 0xf0)
            retained = payload(allocated).hex() if allocated else '-'
            print('A', int(state['allocate']), identity(allocated), retained, sep='\t')
            call(0x40a820, esi=manager, edi=allocated)
            snapshot()
        if get(user + 0x1214) != 0:
            raise RuntimeError('Original active count did not reach zero')


if __name__ == '__main__':
    main()
