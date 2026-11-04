#!/usr/bin/env python3
"""Execute original CSkill heap, list admission, cleanup and list destruction."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_EBX
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x40000)
    stop = base + 0x3f000
    pool = base + 0x10114
    owners = [base, base + 0x2000]
    heads = [base + 0x5000, base + 0x5100]
    state = {}

    def put(at, *values):
        cpu.mem_write(at, struct.pack('<'+'I'*len(values), *[x & 0xffffffff for x in values]))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    def idx(at):
        return state['objects'].index(at)

    def seq(values):
        return ','.join(map(str, values)) or '-'

    def nodes(head, sentinel=0):
        result = []
        while head != sentinel:
            if len(result) > 100:
                raise RuntimeError('Cyclic original list')
            result.append(idx(head))
            head = get(head + 4)
        return result

    def snapshot():
        lists = [seq(nodes(get(h+4), h)) for h in heads]
        for owner, h in zip(owners, heads):
            assert get(owner+0x1e4) == len(nodes(get(h+4), h))
        free = nodes(get(pool))
        assert get(pool+8) == len(free)
        payloads = []
        for i, at in enumerate(state['objects']):
            if i not in state['deleted']:
                payloads.append(':'.join(map(str, [i, *struct.unpack('<10I', cpu.mem_read(at+8, 40)), (get(at+48)-base-0x30000)//64])))
        return '|'.join([str(get(pool+4)), seq(free), *lists,
                         seq(state['logs']), seq(state['deleted']), ';'.join(payloads) or '-'])

    cpu.mem_write(stop+16, b'\xc2\x04\x00')
    put(0x5210cc, stop+16)
    put(0x5210d0, stop+16)
    cpu.mem_write(0x503a3f, b'\xc3')
    cpu.mem_write(0x503ba1, b'\xc3')
    cpu.mem_write(0x4a1810, b'\xc2\x0c\x00')

    def hook(machine, address, size, data):
        if address == 0x503a3f:
            if state['fail']:
                machine.reg_write(UC_X86_REG_EAX, 0)
            else:
                at = base+0x20000+len(state['objects'])*0x100
                state['objects'].append(at)
                machine.mem_write(at, bytes(52))
                machine.reg_write(UC_X86_REG_EAX, at)
        elif address == 0x503ba1:
            state['deleted'].append(idx(get(machine.reg_read(UC_X86_REG_ESP)+4)))
        elif address == 0x4a1810:
            esp = machine.reg_read(UC_X86_REG_ESP)
            at = machine.reg_read(UC_X86_REG_EBX)
            # Real call arguments: action, skill name, level. Still linked here.
            assert get(esp+4) == 6 and get(esp+12) == get(at+0x10)
            assert machine.reg_read(UC_X86_REG_EDI) == owners[state['owner']]+0xcf4
            assert get(esp+8) == get(at+0x30)+3
            name = ('Skill'+str(idx(at))).encode() + b'\0'
            assert bytes(cpu.mem_read(get(esp+8), len(name))) == name
            assert idx(at) in nodes(get(heads[state['owner']]+4), heads[state['owner']])
            assert idx(at) not in nodes(get(pool))
            state['logs'].append(idx(at))
    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, esi=0, edi=0):
        put(base+0x3e000, stop)
        cpu.reg_write(UC_X86_REG_ESP, base+0x3e000)
        cpu.reg_write(UC_X86_REG_ESI, esi)
        cpu.reg_write(UC_X86_REG_EDI, edi)
        cpu.emu_start(address, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original call did not return')
        return cpu.reg_read(UC_X86_REG_EAX)

    print(f'# ps_game.exe SHA256 {digest}; Alloc 0040b180, AddBack 00486200, ClearApplySkill 004a0990, Release 0044bc00. Real list/pool instructions; allocation, logging, locks and delete are observed ports.')
    print('# S scenario; A owner fail stamp expected; C owner flags expected; D owner expected. Snapshot: attempts|free|owner0|owner1|logs|deleted|retained10wordpayloads+definition_index. flags: Lua AI, respawn pointer, respawn logging.')
    for scenario in range(64):
        cpu.mem_write(base, bytes(0x3e000))
        put(0x546f3c, base+0x10000)
        state.update(objects=[], logs=[], deleted=[], fail=False, owner=0)
        for owner, head in zip(owners, heads):
            put(owner+0x1c4, head, head)
            put(head, head, head)
        print('S', scenario, sep='\t')
        operations = [('A', 0, 1, 0)]
        operations += [('A', i % 2, 0, (scenario+1)*100+i) for i in range(scenario % 9)]
        operations += [('C', 0, scenario % 8), ('C', 0, scenario % 8), ('A', 1, 1, 0),
                       ('A', 0, 0, 0), ('C', 1, (scenario//8) % 8), ('A', 0, 0, 0),
                       ('A', 1, 0, 0xffffffff), ('D', 0), ('C', 1, scenario % 8), ('D', 1)]
        for op in operations:
            state['owner'] = op[1]
            owner = owners[op[1]]
            if op[0] == 'A':
                state['fail'] = bool(op[2])
                at = call(0x40b180, pool)
                if at:
                    if op[3]:
                        put(at+8, *[(op[3]+i) & 0xffffffff for i in range(10)])
                    info = base+0x30000+idx(at)*64
                    put(at+0x30, info)
                    cpu.mem_write(info+3, ('Skill'+str(idx(at))).encode()+b'\0')
                    call(0x486200, owner+0x1c4, at)
            elif op[0] == 'C':
                flags = op[2]
                put(owner+0x284, 1 if flags & 1 else 0)
                put(owner+0xc84, base+0x6000 if flags & 2 else 0)
                cpu.mem_write(base+0x601c, bytes([bool(flags & 4)]))
                call(0x4a0990, owner)
            else:
                call(0x44bc00, edi=owner+0x1c4)
            print(*op, snapshot(), sep='\t')


if __name__ == '__main__':
    main()
