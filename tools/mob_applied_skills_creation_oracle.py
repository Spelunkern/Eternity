#!/usr/bin/env python3
"""Execute original manager skill creation and ID reuse across two live lists."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x40000)
    manager = base+0x10000
    pool = manager+0x114
    stop = base+0x3f000
    owners = [base, base+0x2000]
    heads = [base+0x5000, base+0x5100]
    state = {}

    def put(at, *values):
        cpu.mem_write(at, struct.pack('<'+'I'*len(values), *[x & 0xffffffff for x in values]))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    def idx(at):
        return state['objects'].index(at)

    def seq(values):
        return ','.join(map(str, values)) or '-'

    def nodes(at, sentinel=0):
        result = []
        while at != sentinel:
            assert len(result) < 100
            result.append(idx(at))
            at = get(at+4)
        return result

    def snapshot():
        free = nodes(get(pool))
        assert len(free) == get(pool+8)
        lists = [nodes(get(h+4), h) for h in heads]
        for owner, records in zip(owners, lists):
            assert len(records) == get(owner+0x1e4)
        payload = ['%d:%s' % (i, ':'.join(map(str, struct.unpack('<10I', cpu.mem_read(at+8, 40)))))
                   for i, at in enumerate(state['objects'])]
        return '|'.join([str(get(manager+0x10)), str(get(pool+4)), seq(free),
                         *map(seq, lists), ';'.join(payload) or '-'])

    cpu.mem_write(stop+16, b'\xc2\x04\x00')
    put(0x5210cc, stop+16)
    put(0x5210d0, stop+16)
    # stdcall InterlockedIncrement: execute locked xadd on the real input word.
    cpu.mem_write(stop+32, bytes.fromhex('8b4c2404b801000000f00fc10140c20400'))
    put(0x5210f8, stop+32)
    cpu.mem_write(0x503a3f, b'\xc3')

    def hook(machine, address, size, data):
        if address == 0x503a3f:
            if state['fail']:
                machine.reg_write(UC_X86_REG_EAX, 0)
            else:
                at = base+0x20000+len(state['objects'])*64
                state['objects'].append(at)
                # Deliberately nonzero initial ID verifies the original constructor.
                machine.mem_write(at, b'\xa5'*52)
                machine.reg_write(UC_X86_REG_EAX, at)
    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, esi=0, edi=0, eax=0):
        put(base+0x3e000, stop)
        cpu.reg_write(UC_X86_REG_ESP, base+0x3e000)
        cpu.reg_write(UC_X86_REG_ESI, esi)
        cpu.reg_write(UC_X86_REG_EDI, edi)
        cpu.reg_write(UC_X86_REG_EAX, eax)
        cpu.emu_start(address, stop, count=100000)
        assert cpu.reg_read(UC_X86_REG_EIP) == stop
        return cpu.reg_read(UC_X86_REG_EAX)

    print(f'# ps_game.exe SHA256 {digest}; CreateSkill 0040a780, Alloc 0040b180, AddBack 00486200, ClearApplySkill 004a0990. Original manager/heap/list instructions; allocation and locks ports; locked xadd implements imported InterlockedIncrement.')
    print('# S scenario initial_counter; A owner fail stamp result_index snapshot; C owner snapshot; X counter snapshot. Snapshot counter|attempts|free|owner0|owner1|10wordpayloads. Every fresh payload initialized after creation; recycled payload retained.')
    counters = [0, 1, 0x7ffffffe, 0x7fffffff, 0x80000000, 0xfffffffd, 0xfffffffe, 0xffffffff]
    for scenario in range(64):
        cpu.mem_write(base, bytes(0x3e000))
        put(0x546f3c, manager)
        initial = counters[scenario % len(counters)]
        put(manager+0x10, initial)
        state.update(objects=[], fail=False)
        for owner, head in zip(owners, heads):
            put(owner+0x1c4, head, head)
            put(head, head, head)
        print('S', scenario, initial, sep='\t')
        operations = [('A', 0, 1, 0), ('A', 0, 0, scenario+1),
                      ('C', 0), ('A', 1, 1, 0), ('A', 0, 1, 0),
                      ('A', 0, 0, scenario+100), ('C', 1), ('C', 0),
                      ('X', counters[(scenario//8) % len(counters)]),
                      ('A', 1, 1, 0), ('A', 0, 1, 0),
                      ('A', 1, 0, scenario+200), ('C', 1), ('C', 0),
                      ('A', 0, 1, 0), ('C', 0), ('C', 1)]
        for op in operations:
            if op[0] == 'A':
                state['fail'] = bool(op[2])
                at = call(0x40a780, eax=manager)
                if at:
                    if op[3]:
                        put(at+12, *[op[3]+i for i in range(9)])
                    call(0x486200, esi=owners[op[1]]+0x1c4, edi=at)
                print(*op, idx(at) if at else -1, snapshot(), sep='\t')
            elif op[0] == 'C':
                call(0x4a0990, esi=owners[op[1]])
                print(*op, snapshot(), sep='\t')
            else:
                put(manager+0x10, op[1])
                print(*op, snapshot(), sep='\t')


if __name__ == '__main__':
    main()
