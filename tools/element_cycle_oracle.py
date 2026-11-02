#!/usr/bin/env python3
"""Execute original elemental replacement, including nested InitApplySkill list iteration. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, info, nodes, party, incoming, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x10000000
    for address in (0, stack, user, info, nodes, party, incoming, stop):
        cpu.mem_map(address, 0x10000)
    for address, cleanup in ((0x4d4f10, 8), (0x4d4d20, 8), (0x445680, 4), (0x40a7c0, 0), (0x481990, 0)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    root = user + 0x8000
    context = dict(events=[], mutation=0, mutated=False, node_count=0)

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def flags():
        return sum(int(value == 1) << i for i, value in enumerate(cpu.mem_read(user + 0x12cc, 9)))

    def append_node(index, detail, effect, attribute, offline):
        node, definition = nodes + index * 0x80, info + index * 160
        data = bytearray(160)
        struct.pack_into('<HB', data, 0, 200 + index, 1)
        struct.pack_into('<H', data, 0x32, detail)
        data[0x31], data[0x4e], data[0x97] = effect, attribute, offline
        struct.pack_into('<BxH', data, 0x7c, 1, 1)
        cpu.mem_write(definition, bytes(data))
        last = number(root)
        cpu.mem_write(node, struct.pack('<4I', last, root, 1000 + index, 200 + index))
        cpu.mem_write(node + 0x10, struct.pack('<I', 1))
        cpu.mem_write(node + 0x30, struct.pack('<I', definition))
        cpu.mem_write(last + 4, struct.pack('<I', node));cpu.mem_write(root, struct.pack('<I', node))
        cpu.mem_write(user + 0xab4, struct.pack('<I', number(user + 0xab4) + 1))

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x4d4f10:
            ptr, length = struct.unpack('<2I', machine.mem_read(sp + 4, 8))
            data = bytes(machine.mem_read(ptr, length))
            if len(data) != 9 or struct.unpack_from('<HI', data) != (0x902, 9001):
                raise RuntimeError('Unexpected replica update')
            identity, level = struct.unpack_from('<HB', data, 6)
            context['events'].append((1, identity, level, 0))
        elif address == 0x4d4d20:
            ptr, length = struct.unpack('<2I', machine.mem_read(sp + 4, 8))
            data = bytes(machine.mem_read(ptr, length))
            if len(data) != 6 or struct.unpack_from('<H', data)[0] != 0x50e:
                raise RuntimeError('Unexpected self notification')
            context['events'].append((2, struct.unpack_from('<I', data, 2)[0], 0, 0))
        elif address == 0x481990:
            context['events'].append((3, number(user + 0x17c), 0, 0))
        elif address == 0x40a7c0:
            node = machine.reg_read(UC_X86_REG_EDI)
            context['events'].append((4, number(node + 8), number(user + 0x1280), flags()))
            if not context['mutated']:
                context['mutated'] = True
                if context['mutation'] == 1:
                    cpu.mem_write(user + 0x173c, bytes(4))
                elif context['mutation'] == 2:
                    cpu.mem_write(party + 0x114, b'\x01')
                elif context['mutation'] == 3:
                    append_node(context['node_count'], 94, 4, 3, 0)
        elif address == 0x445680:
            data = bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX)))
            opcode, cid, identity, level = struct.unpack('<HIHB', data)
            if opcode not in (0xc05, 0xc10) or cid != 9001:
                raise RuntimeError('Unexpected party notification')
            context['events'].append((5, identity, level, int(opcode == 0xc10)))

    cpu.hook_add(UC_HOOK_CODE, hook)
    e = lambda attribute=1, effect=4, offline=0: (94, effect, attribute, offline)
    b = (82, 4, 0, 0)
    many = [e(1 if i % 3 else 3, 4, i % 2) for i in range(70)]
    scenarios = [(0, -1, 1, 2, 0, []), (0, -1, 1, 2, 0, [e()]),
                 (0, 0, 1, 2, 0, [b, e(), e(2), e(3, 3), e(0), e(8, 4, 1)]),
                 (0, 0, 1, 2, 0, many), (0, 1, 1, 2, 0, many),
                 (0, 0, 1, 2, 1, many[:6]), (0, 0, 1, 2, 2, many[:6]), (0, 0, 1, 2, 3, many[:3]),
                 (1, -1, 0, 0, 0, [b, e(), e(2)]), (1, 0, 0, 0, 0, [e(), b, e(2), b]),
                 (1, 0, 0, 0, 0, [b, e(), e(2), e(3)]), (1, 1, 0, 0, 0, [e(), e(2), b, e(2, 4, 1)]),
                 (1, -1, 0, 0, 0, [b, b, b]), (0, 0, 1, 6, 0, [e(), e(2), e(3)]),
                 (0, 0, 0, 1, 0, [e(0), e()])]
    print(f'# ps_game.exe SHA256 {digest}; AddApplySkillBuff 00484cf0 detail 94; InitApplySkill 00487e50; SendSkillRemove 00482a70; RemApplySkill 00484c60')
    print('# scenario mode group current_attribute flags mutation node_count; nodes(detail,effect,attribute,offline); remaining_count HP attribute flags; remaining IDs; event_count; events(code,a,b,c): DB,self,maxHP,free(with attribute/flags),party')
    for scenario, (mode, group, attribute, mask, mutation, entries) in enumerate(scenarios):
        context.update(events=[], mutation=mutation, mutated=False, node_count=len(entries))
        cpu.mem_write(user, bytes(0x9000));cpu.mem_write(party, bytes(0x200))
        cpu.mem_write(root, struct.pack('<2I', root, root));cpu.mem_write(user + 0xa94, struct.pack('<I', root))
        cpu.mem_write(user + 0xdc, struct.pack('<I', 9001));cpu.mem_write(user + 0x578c, struct.pack('<I', 9001))
        cpu.mem_write(user + 0x17c, struct.pack('<I', 1000));cpu.mem_write(user + 0x1280, struct.pack('<I', attribute))
        cpu.mem_write(user + 0x12cc, bytes((mask >> i) & 1 for i in range(9)))
        cpu.mem_write(user + 0x173c, struct.pack('<I', party if group >= 0 else 0));cpu.mem_write(party + 0x114, bytes([max(0, group)]))
        for index, entry in enumerate(entries):
            append_node(index, *entry)
        cpu.mem_write(incoming, bytes(160));cpu.mem_write(incoming, struct.pack('<HB', 9999, 1))
        cpu.mem_write(incoming + 0x31, b'\x04');cpu.mem_write(incoming + 0x32, struct.pack('<H', 94));cpu.mem_write(incoming + 0x4e, b'\x02')
        cpu.mem_write(stack + 0x8000, struct.pack('<3I', stop, user, incoming))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_EAX, user)
        cpu.emu_start(0x487e50 if mode else 0x484cf0, stop, count=200000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original element replacement did not return')
        remaining, node = [], number(root + 4)
        while node != root:
            if len(remaining) > 256:
                raise RuntimeError('Original list did not terminate')
            remaining.append(number(node + 8));node = number(node + 4)
        print(scenario, mode, group, attribute, mask, mutation, len(entries), *[value for entry in entries for value in entry],
              len(remaining), number(user + 0x17c), number(user + 0x1280), flags(), *remaining,
              len(context['events']), *[value for event in context['events'] for value in event], sep='\t')


if __name__ == '__main__':
    main()
