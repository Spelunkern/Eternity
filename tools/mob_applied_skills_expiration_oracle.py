#!/usr/bin/env python3
"""Execute original mob expiration, including removal-driven recursive cleanup."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x40000)
    pool = base+0x10114
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
        return '|'.join([str(get(pool+4)), seq(free), *map(seq, lists),
                         seq(state['events']), ';'.join(payload) or '-'])

    cpu.mem_write(stop+16, b'\xc2\x04\x00')
    put(0x5210cc, stop+16)
    put(0x5210d0, stop+16)
    cpu.mem_write(0x503a3f, b'\xc3')
    cpu.mem_write(0x4a1810, b'\xc2\x0c\x00')
    cpu.mem_write(0x4a0da0, b'\xc3')

    def call_bytes(start, target):
        return b'\xe8'+struct.pack('<i', target-start-5)

    def hook(machine, address, size, data):
        if address == 0x503a3f:
            at = base+0x20000+len(state['objects'])*64
            state['objects'].append(at)
            machine.mem_write(at, bytes(52))
            machine.reg_write(UC_X86_REG_EAX, at)
        elif address == 0x486200 and state['expected_append']:
            assert get(machine.reg_read(UC_X86_REG_EDI)+0x20) == 0xffffffff
            state['expected_append'] = False
        elif address in (0x4a1810, 0x4a0da0):
            at = machine.reg_read(UC_X86_REG_EBX)
            owner = owners[state['owner']]
            active = nodes(get(heads[state['owner']]+4), heads[state['owner']])
            assert idx(at) not in nodes(get(pool))
            if address == 0x4a1810:
                esp = machine.reg_read(UC_X86_REG_ESP)
                assert idx(at) in active and get(esp+4) == 6
                assert get(esp+8) == get(at+48)+3 and get(esp+12) == get(at+16)
                assert machine.reg_read(UC_X86_REG_EDI) == owner+0xcf4
                state['events'].append('L'+str(idx(at)))
                return
            assert idx(at) not in active and machine.reg_read(UC_X86_REG_ECX) == owner
            state['events'].append('R'+str(idx(at)))
            put(at+0x14, get(at+0x14) ^ 0x55aa55aa)
            mode = state['mode'] if state['removed'] == 0 else 0
            state['removed'] += 1
            if mode == 2 and active:
                put(state['objects'][active[-1]]+0x20, 0)
            elif mode in (1, 3):
                # Execute real recursive Clear. Mode 3 additionally appends one
                # recycled node, testing that expiration resumes from live cursor.
                start = stop+64
                code = b'\x56\x57\xbe'+struct.pack('<I', owner)
                code += call_bytes(start+len(code), 0x4a0990)
                if mode == 3 and active:
                    state['expected_append'] = True
                    code += b'\xbe'+struct.pack('<I', pool)
                    code += call_bytes(start+len(code), 0x40b180)
                    code += b'\x89\xc7\xc7\x47\x20\xff\xff\xff\xff'
                    code += b'\xbe'+struct.pack('<I', owner+0x1c4)
                    code += call_bytes(start+len(code), 0x486200)
                code += b'\x5f\x5e\xc3'
                machine.mem_write(start, code)
                # External writes do not invalidate Unicorn translated blocks.
                machine.ctl_remove_cache(start, start+256)
                machine.reg_write(UC_X86_REG_EIP, start)
    cpu.hook_add(UC_HOOK_CODE, hook)

    def call(address, esi=0, edi=0, args=()):
        put(base+0x3e000, stop, *args)
        cpu.reg_write(UC_X86_REG_ESP, base+0x3e000)
        cpu.reg_write(UC_X86_REG_ESI, esi)
        cpu.reg_write(UC_X86_REG_EDI, edi)
        cpu.emu_start(address, stop, count=100000)
        assert cpu.reg_read(UC_X86_REG_EIP) == stop
        assert not state['expected_append']
        return cpu.reg_read(UC_X86_REG_EAX)

    def flags(owner, bits):
        put(owner+0x284, bits & 1)
        put(owner+0xc84, base+0x6000 if bits & 2 else 0)
        cpu.mem_write(base+0x601c, bytes([bool(bits & 4)]))

    print(f'# ps_game.exe SHA256 {digest}; ChkEndTimeSkill 004a12f0, ClearApplySkill 004a0990, Alloc 0040b180, AddBack 00486200. Real list/cursor/pool; required RemApplySkill 004a0da0 is an observed port with explicit callback mutation modes.')
    print('# S scenario; A owner end index snapshot; E owner now flags mode snapshot; C owner flags snapshot. mode 1 recursive clear, 2 last active end=0, 3 recursive clear+recycled append. Snapshot attempts|free|owner0|owner1|L-log/R-removal events|ten-word payloads.')
    ticks = [0, 1, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff, 2, 1000]
    for scenario in range(128):
        cpu.mem_write(base, bytes(0x3e000))
        put(0x546f3c, base+0x10000)
        state.update(objects=[], events=[], owner=0, mode=0, removed=0, expected_append=False)
        for owner, head in zip(owners, heads):
            put(owner+0x1c4, head, head)
            put(head, head, head)
        now = ticks[(scenario//8) % 8]
        ends = [0xffffffff, now, (now+1) & 0xffffffff, 0, 0x80000000, 1, 0x7fffffff]
        if scenario & 64:
            ends.reverse()
        print('S', scenario, sep='\t')
        operations = [('A', 0, end) for end in ends[:scenario % 8]]
        operations += [('A', 1, now), ('A', 1, (now+1) & 0xffffffff),
                       ('E', 0, now, scenario % 8, (scenario//4) % 4),
                       ('E', 0, now, scenario % 8, 0), ('C', 0, scenario % 8),
                       ('E', 1, now, (scenario//8) % 8, 0),
                       ('C', 1, 0), ('A', 0, now), ('E', 0, now, scenario % 8, 0)]
        for op in operations:
            state['owner'] = op[1]
            owner = owners[op[1]]
            if op[0] == 'A':
                at = call(0x40b180, esi=pool)
                i = idx(at)
                put(at+8, i+1, 100+i, i % 5+1, 200+i, 300+i, 400+i, op[2], 500+i, i % 3, 600+i)
                put(at+48, base+0x30000+i*64)
                call(0x486200, esi=owner+0x1c4, edi=at)
                print(*op, i, snapshot(), sep='\t')
            elif op[0] == 'E':
                state.update(mode=op[4], removed=0)
                flags(owner, op[3])
                call(0x4a12f0, args=(owner, op[2]))
                print(*op, snapshot(), sep='\t')
            else:
                flags(owner, op[2])
                call(0x4a0990, esi=owner)
                print(*op, snapshot(), sep='\t')


if __name__ == '__main__':
    main()
