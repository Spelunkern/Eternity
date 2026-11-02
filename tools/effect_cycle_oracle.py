#!/usr/bin/env python3
"""Execute original effect expiry over linked lists, recording DB-agent/client/group calls. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    machine, digest = load_game()
    stack, user, info, nodes, objects, party, stop = (0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x34000000, 0x10000000)
    for address in (0, stack, user, info, nodes, objects, party, stop):
        machine.mem_map(address, 0x10000)
    machine.reg_write(UC_X86_REG_FPCW, 0x27f)
    machine.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        machine.mem_write(iat, struct.pack('<I', stop + 0x100))
    for address, cleanup in ((0x4d4f10, 8), (0x4d4d20, 8), (0x481990, 0), (0x445680, 4), (0x4505a0, 0), (0x4bd8d0, 12)):
        machine.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    machine.mem_write(stop + 0x200, b'\xc3')
    machine.mem_write(0x5210e4, struct.pack('<I', stop + 0x200))
    machine.mem_write(0x546f3c, struct.pack('<I', objects))
    trace, current = [], {}
    root = user + 0x8000

    def append_node(index, realtime=False, end=12000, detail=82, effect=4):
        node, definition = nodes + index * 0x80, info + index * 160
        data = bytearray(160)
        struct.pack_into('<HB', data, 0, 200 + index, 1)
        data[0x31], data[0x97] = effect, int(realtime)
        struct.pack_into('<H', data, 0x32, detail)
        struct.pack_into('<BxH', data, 0x7c, 1, 1)
        struct.pack_into('<3H', data, 0x8e, 3, 7, 11)
        machine.mem_write(definition, bytes(data))
        last = struct.unpack('<I', machine.mem_read(root, 4))[0]
        machine.mem_write(node, struct.pack('<4I', last, root, 1000 + index, 200 + index))
        machine.mem_write(node + 0x10, struct.pack('<I', 1))
        machine.mem_write(node + 0x20, struct.pack('<4I', end, 10000, 1, 0))
        machine.mem_write(node + 0x30, struct.pack('<I', definition))
        machine.mem_write(last + 4, struct.pack('<I', node))
        machine.mem_write(root, struct.pack('<I', node))
        count = struct.unpack('<I', machine.mem_read(user + 0xab4, 4))[0]
        machine.mem_write(user + 0xab4, struct.pack('<I', count + 1))

    def hook(cpu, address, size, context):
        if address == 0x4d4f10:
            ptr = struct.unpack('<I', cpu.mem_read(cpu.reg_read(UC_X86_REG_ESP) + 4, 4))[0]
            data = cpu.mem_read(ptr, 9)
            if struct.unpack_from('<H', data)[0] != 0x902:
                raise RuntimeError('Unexpected database request')
            trace.append((1, *struct.unpack_from('<HB', data, 6), 0))
        elif address == 0x4d4d20:
            ptr = struct.unpack('<I', cpu.mem_read(cpu.reg_read(UC_X86_REG_ESP) + 4, 4))[0]
            data = cpu.mem_read(ptr, 6)
            if struct.unpack_from('<H', data)[0] != 0x50e:
                raise RuntimeError('Unexpected client notification')
            trace.append((2, struct.unpack_from('<I', data, 2)[0], 0, 0))
        elif address == 0x481990:
            trace.append((3, 0, 0, 0))
            if not current['changed']:
                current['changed'] = True
                if current['scenario'] == 5:
                    cpu.mem_write(user + 0x173c, bytes(4))
                elif current['scenario'] == 6:
                    cpu.mem_write(party + 0x114, b'\x01')
                elif current['scenario'] == 7:
                    append_node(3)
        elif address == 0x4867c8:
            node = cpu.reg_read(UC_X86_REG_ESI)
            identity = struct.unpack('<I', cpu.mem_read(node + 8, 4))[0]
            trace.append((4, identity, 0, 0))
        elif address == 0x445680:
            data = cpu.mem_read(cpu.reg_read(UC_X86_REG_ECX), 9)
            opcode = struct.unpack_from('<H', data)[0]
            cid, identity, level = struct.unpack_from('<IHB', data, 2)
            if opcode not in (0xc05, 0xc10) or cid != 9001:
                raise RuntimeError('Unexpected group notification')
            trace.append((5, identity, level, int(opcode == 0xc10)))
        elif address == stop + 0x200:
            cpu.reg_write(UC_X86_REG_EAX, 13001)
        elif address == 0x4505a0:
            trace.append((6, *struct.unpack('<3i', cpu.mem_read(cpu.reg_read(UC_X86_REG_EAX), 12))))

    machine.hook_add(UC_HOOK_CODE, hook)
    scenarios = [0, 1, 1, 8, 70, 6, 6, 3, 3, 1, 1]
    print(f'# ps_game.exe SHA256 {digest}; ChkEndTimeSkill 004865b0, RemApplySkill 00484c60, SendSkillRemoveParty 004829a0')
    print('# scenario; remaining_count,HP_max,hp_passive_applied,event_count; events(code,a,b,c); DB-agent/network/health operations recorded')
    for scenario, count in enumerate(scenarios):
        trace.clear();current.update(scenario=scenario, changed=False)
        machine.mem_write(user, bytes(0x9000));machine.mem_write(objects, bytes(0x1000));machine.mem_write(party, bytes(0x200))
        machine.mem_write(root, struct.pack('<II', root, root))
        machine.mem_write(user + 0xa94, struct.pack('<I', root))
        machine.mem_write(user + 0x173c, struct.pack('<I', party if scenario >= 3 else 0))
        machine.mem_write(user + 0xdc, struct.pack('<I', 9001));machine.mem_write(user + 0x578c, struct.pack('<I', 9001))
        machine.mem_write(user + 0x17c, struct.pack('<I', 10000));machine.mem_write(user + 0x57b5, b'\x01')
        machine.mem_write(user + 0x1368, b'\x01')
        for index in range(count):
            realtime = scenario == 2 or (scenario == 3 and index % 2 == 1)
            end = 12 if realtime else 12000
            if scenario == 3 and index % 3 == 0:
                end += 1
            if scenario == 8:
                end = (0, 1, 0xffffffff)[index]
            append_node(index, realtime, end, 6 if scenario == 9 else 81 if scenario == 10 else 82, 0 if scenario == 9 else 4)
        tick = 0 if scenario == 8 else 12000
        machine.mem_write(stack + 0x8000, struct.pack('<II', stop, tick))
        machine.reg_write(UC_X86_REG_ESP, stack + 0x8000);machine.reg_write(UC_X86_REG_EAX, user)
        machine.emu_start(0x4865b0, stop, count=200000)
        if machine.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original effect expiry did not return')
        remaining = struct.unpack('<I', machine.mem_read(user + 0xab4, 4))[0]
        hp = struct.unpack('<I', machine.mem_read(user + 0x17c, 4))[0]
        applied = machine.mem_read(user + 0x1368, 1)[0]
        print(scenario, remaining, hp, applied, len(trace), *[value for event in trace for value in event], sep='\t')


if __name__ == '__main__':
    main()
