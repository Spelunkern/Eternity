#!/usr/bin/env python3
"""Execute original actor appearance, speed, attack-stop and periodic healing operations. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, party, info, source, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x10000000
    for address in (0, stack, user, party, info, source, stop):
        cpu.mem_map(address, 0x10000)
    cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    for address, cleanup in ((0x4d4d20, 8), (0x421220, 32), (0x4208c0, 12), (0x445680, 4), (0x4b8aa0, 0), (0x505316, 0)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    cpu.mem_write(stop + 0x100, b'\xc3')
    cpu.mem_write(0x5210e4, struct.pack('<I', stop + 0x100))
    context = dict(events=[])

    def words(address, count):
        return struct.unpack('<' + 'I' * count, cpu.mem_read(address, count * 4))

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x4d4d20:
            ptr, length = words(sp + 4, 2)
            context['events'].append('S' + bytes(machine.mem_read(ptr, length)).hex())
        elif address == 0x421220:
            zone, ptr, length, position, radius, cid, exclusion, channel = words(sp + 4, 8)
            if (zone, position, radius, cid, exclusion, channel) != (party + 0x1000, user + 0xd0, 0x42700000, 9001, 0, 5):
                raise RuntimeError('Unexpected proximity route')
            context['events'].append('N' + bytes(machine.mem_read(ptr, length)).hex())
        elif address == 0x4208c0:
            zone, ptr, length = words(sp + 4, 3)
            if (zone, machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX)) != (party + 0x1000, 17, 23):
                raise RuntimeError('Unexpected cell route')
            context['events'].append('C' + bytes(machine.mem_read(ptr, length)).hex())
        elif address == 0x445680:
            context['events'].append('P' + bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX))).hex())
        elif address == 0x4b8aa0:
            context['events'].append('D')
        elif address == stop + 0x100:
            machine.reg_write(UC_X86_REG_EAX, 13001)

    cpu.hook_add(UC_HOOK_CODE, hook)
    print(f'# ps_game.exe SHA256 {digest}; SendShapeMe 00482510, SendShape 00482590, SendShapeVehicle2 00482660, SendSpeed 004827b0, SendRunMode 004826c0, StatusAttackStop 00489ed0, RemApplySkillBuff 00485130')
    print('# operation scenario; final mode,recovery,target_type,previous,next,HP,MP,SP; ordered network/damage events')
    entries = (0x482510, 0x482590, 0x482660, 0x4827b0, 0x4826c0, 0x489ed0, 0x485130)
    for op, entry in enumerate(entries):
        for scenario in range(128):
            context['events'].clear()
            cpu.mem_write(user, bytes(0x6000))
            cpu.mem_write(user + 0xdc, struct.pack('<I', 9001))
            cpu.mem_write(user + 0xe0, struct.pack('<3I', party + 0x1000 if scenario & 1 else 0, 17, 23))
            cpu.mem_write(user + 0x5774, bytes([int(bool(scenario & 2))]))
            cpu.mem_write(user + 0x57b5, bytes([int(bool(scenario & 4))]))
            cpu.mem_write(user + 0x1460, struct.pack('<I', scenario % 4))
            cpu.mem_write(user + 0x1468, struct.pack('<3I', 77, 25, 0x78563412))
            cpu.mem_write(user + 0x1354, bytes([(0, 13, 100, 101, 102, 255)[scenario % 6]]))
            cpu.mem_write(user + 0x134c, struct.pack('<H', 0x1234))
            for i, offset in enumerate((0x133c, 0x1340, 0x1344, 0x1428, 0x1348, 0x5778)):
                cpu.mem_write(user + offset, struct.pack('<I', (scenario >> i) & 1))
            cpu.mem_write(user + 0x12ec, struct.pack('<i', scenario % 13 - 2))
            cpu.mem_write(user + 0x12f0, struct.pack('<i', scenario % 8 - 2))
            mode = (0, 1, 2, -1)[scenario % 4]
            recovery = (-2, 0, 1, 7)[scenario % 4]
            cpu.mem_write(user + 0x142c, struct.pack('<i', mode))
            cpu.mem_write(user + 0x1274, struct.pack('<i', recovery))
            cpu.mem_write(user + 0x1528, struct.pack('<I', 2))
            cpu.mem_write(user + 0x143c, struct.pack('<2I', 123, 456))
            current = ((100, 200, 300), (999, 1999, 2999), (1000, 2000, 3000), (70000, 80000, 90000))[scenario % 4]
            cpu.mem_write(user + 0x1238, struct.pack('<3I', *current))
            cpu.mem_write(user + 0x17c, struct.pack('<3I', 1000, 2000, 3000))
            cpu.mem_write(user + 0x173c, struct.pack('<I', party if scenario & 8 else 0))
            cpu.mem_write(party + 0x114, bytes([int(bool(scenario & 16))]))
            cpu.mem_write(info, bytes(160))
            cpu.mem_write(info, struct.pack('<HB', 321, 2))
            cpu.mem_write(info + 0x32, struct.pack('<H', 81))
            cpu.mem_write(info + 0x8e, struct.pack('<3H', 3, 7, 65535))
            cpu.mem_write(source + 0xc, struct.pack('<2I', 321, 2))
            cpu.mem_write(source + 0x24, struct.pack('<I', (13001 - (0, 1001, 3000, 0xffffffff)[scenario % 4]) & 0xffffffff))
            cpu.mem_write(stack + 0x8000, struct.pack('<3I', stop, user, scenario & 1))
            cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
            for reg in (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI):
                cpu.reg_write(reg, user)
            cpu.reg_write(UC_X86_REG_EDX, info)
            cpu.reg_write(UC_X86_REG_EBX, source)
            cpu.emu_start(entry, stop, count=10000)
            if cpu.reg_read(UC_X86_REG_EIP) != stop:
                raise RuntimeError('Original actor operation did not return')
            final_mode = struct.unpack('<i', cpu.mem_read(user + 0x142c, 4))[0]
            final_recovery = struct.unpack('<i', cpu.mem_read(user + 0x1274, 4))[0]
            target_type = words(user + 0x1528, 1)[0]
            previous, following = words(user + 0x143c, 2)
            resources = words(user + 0x1238, 3)
            print(op, scenario, final_mode, final_recovery, target_type, previous, following, *resources, len(context['events']), *context['events'], sep='\t')


if __name__ == '__main__':
    main()
