#!/usr/bin/env python3
"""Execute complete original quest reward result with observed delivery owners."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def quest_block(records):
    regions = [(0x102, 14), (0x110, 7), (0x118, 9), (0x124, 37), (0x14a, 14), (0x15a, 1), (0x158, 2), (0x15b, 3)]
    for i in range(3):
        at = 0x160 + i * 32
        regions += [(at, 5), (at + 8, 9), (at + 20, 12)]
    result = struct.pack('<I', len(records))
    for record in records:
        name = bytes(record[2:258]).split(b'\0')[0]
        result += bytes(record[:2]) + struct.pack('<I', len(name)) + name + bytes(4)
        result += b''.join(record[at:at + size] for at, size in regions) + bytes(28)
    return result


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x10000)
    user, quest, first, second, item = [base + i for i in (0, 0x7000, 0x8000, 0x8400, 0x9000)]
    stop, stack = base + 0xf000, base + 0xe000
    state = {}

    def put(at, *values):
        cpu.mem_write(at, struct.pack('<' + 'I' * len(values), *[v & 0xffffffff for v in values]))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    ports = {0x458240: 12, 0x4d4f10: 8, 0x45b0d0: 0, 0x47e6f0: 20, 0x4d4d20: 8, 0x47e150: 4, 0x4d4e50: 8, 0x505316: 0}
    for at, cleanup in ports.items():
        cpu.mem_write(at, b'\xc2' + struct.pack('<H', cleanup))

    def event(tag, *values):
        state['events'] += tag.encode() + struct.pack('<' + 'I' * len(values), *[v & 0xffffffff for v in values])

    def hook(machine, address, size, data):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x458240:
            event('E', get(sp + 4), get(sp + 8), get(sp + 12))
            if state['mutate_exp']:
                put(quest + 8, 321)
                put(quest + 0x20, second)
                put(user + 0x140, get(user + 0x140) ^ 0x11111111)
        elif address in (0x4d4f10, 0x4d4d20, 0x4d4e50):
            n, pointer = get(sp + 8), get(sp + 4)
            event({0x4d4f10: 'B', 0x4d4d20: 'C', 0x4d4e50: 'G'}[address], n)
            state['events'] += bytes(machine.mem_read(pointer, n))
            if address == 0x4d4f10 and state['mutate_db']:
                put(quest + 0x20, first)
        elif address == 0x45b0d0:
            pointer = machine.reg_read(UC_X86_REG_ESI)
            machine.mem_write(pointer + 2, bytes((i * 7 + state['scenario']) & 255 for i in range(2, 74)))
            put(quest + 0x20, second if state['header_alt'] else first)
            event('M')
        elif address == 0x47e6f0:
            event('I', get(sp + 4), machine.reg_read(UC_X86_REG_ECX), get(sp + 8))
            if state['item_success']:
                put(get(sp + 12), state['bag'])
                put(get(sp + 16), state['slot'])
                put(get(sp + 20), item)
            machine.reg_write(UC_X86_REG_EAX, state['item_success'])
        elif address == 0x47e150:
            event('R', get(machine.reg_read(UC_X86_REG_EAX) + 8), get(sp + 4))

    cpu.hook_add(UC_HOOK_CODE, hook)
    print(f'# ps_game.exe SHA256 {digest}; complete CUser QuestSuccessResult 0047ee40. Ports: AddExpFromUser, DB/client/game-log transport, log-main fields, QuestAddItem, QuestRemove and cookie. Source stack starts zero; undefined string tails therefore compare as initialized padding.')
    print('# S scenario variant money mutate_exp mutate_db header_alt item_success bag slot context definitions_hex events_hex final_money final_quest_id final_binding')
    for scenario in range(512):
        cpu.mem_write(base, bytes(0xe000))
        variant = scenario % 3
        money = (0, 11, 0xfffffff0, 0xffffffff)[(scenario // 3) % 4]
        mutate_exp, mutate_db, header_alt = (scenario // 7) % 2, (scenario // 11) % 2, (scenario // 13) % 2
        success, bag, slot = (scenario // 17) % 2, scenario % 8, scenario % 240
        context = (0, 777, 0xffffffff)[(scenario // 19) % 3]
        records = []
        for index in range(2):
            record = bytearray(464)
            struct.pack_into('<H', record, 0, 700 + index)
            name = (b'Quest A', b'', b'Q' * 29)[(scenario + index) % 3]
            record[2:2 + len(name)] = name
            record[0x1f], record[0x20] = 32, 0
            record[0x107] = (scenario // 23 + index) % 5
            for choice in range(3):
                experience = (0, 1, 0x7fffffff, 0x80000000, 0xffffffff)[(scenario // 29 + choice + index) % 5]
                reward_money = (0, 1, 100, 0xfffffff0, 0xffffffff)[(scenario // 31 + choice + index) % 5]
                struct.pack_into('<II', record, 0x174 + choice * 32, experience, reward_money)
                record[0x17c + choice * 32] = 3 + index + choice
                record[0x17d + choice * 32] = 9 + index + choice
            records.append(record)
            cpu.mem_write((first, second)[index], bytes(record))
        put(quest + 8, 123)
        put(quest + 0x20, first)
        put(user + 0x140, money)
        put(user + 0x578c, 7)
        put(item, 0x12345678)
        cpu.mem_write(item + 4, b'Reward item\0')
        cpu.mem_write(item + 0x24, bytes([17, 19]))
        state.update(scenario=scenario, mutate_exp=mutate_exp, mutate_db=mutate_db, header_alt=header_alt, item_success=success, bag=bag, slot=slot, events=b'')
        put(stack, stop, context)
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.reg_write(UC_X86_REG_EBX, quest)
        cpu.reg_write(UC_X86_REG_ECX, variant)
        cpu.reg_write(UC_X86_REG_EDX, user)
        cpu.emu_start(0x47ee40, stop, count=10000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Quest reward coordinator did not return')
        print('S', scenario, variant, money, mutate_exp, mutate_db, header_alt, success, bag, slot, context,
              quest_block(records).hex(), state['events'].hex(), get(user + 0x140), get(quest + 8), int(get(quest + 0x20) == second), sep='\t')


if __name__ == '__main__':
    main()
