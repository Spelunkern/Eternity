#!/usr/bin/env python3
"""Execute original zone grid creation, map bounds, cell views and entry coordinate arithmetic. Requires Unicorn."""
import math
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, zone, map_ptr, user, heap, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x40000000, 0x10000000
    for address in (0, stack, zone, map_ptr, user, stop):
        cpu.mem_map(address, 0x10000)
    cpu.mem_map(heap, 0x1000000);cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    cpu.mem_write(0x503ae3, b'\xc3');cpu.mem_write(0x4233d0, b'\xc3')
    for address, cleanup in ((0x40f200, 0), (0x423560, 0), (0x41f350, 12), (0x44adb0, 0), (0x4d43a0, 8)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    context = dict(allocation=heap, phase='create')

    def hook(machine, address, size, _):
        if address == 0x503ae3:
            sp = machine.reg_read(UC_X86_REG_ESP)
            amount = struct.unpack('<I', machine.mem_read(sp + 4, 4))[0]
            pointer = context['allocation'];context['allocation'] += (max(amount, 1) + 15) & ~15
            if context['allocation'] > heap + 0x1000000:
                raise RuntimeError('Geometry oracle allocation limit')
            machine.mem_write(pointer, bytes(amount));machine.reg_write(UC_X86_REG_EAX, pointer)
        elif context['phase'] == 'cell' and address in (0x416022, 0x416151):
            machine.emu_stop()

    cpu.hook_add(UC_HOOK_CODE, hook)

    def number(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def fingerprint(data):
        result = 0xcbf29ce484222325
        for byte in data:
            result = ((result ^ byte) * 0x100000001b3) & 0xffffffffffffffff
        return result

    def bits(value):
        return struct.unpack('<I', struct.pack('<f', value))[0]

    print(f'# ps_game.exe SHA256 {digest}; CZone::Create 00415c40, CMap::IsSafePos 00415360, CCell::IsInnerCellView 004153c0, EnterUser coordinate block 00415fa8..00416022.')
    print('# G size cell_count_x cell_count_z FNV1a64(view bounds in row order); P size x_bits z_bits safe cell_x cell_z; C size cell_x cell_z x_bits z_bits contains; M size old_cell_x old_cell_z x_bits z_bits success final_cell_x final_cell_z. Cell constructors/allocation, movement scene/transport ports isolated.')
    for size in (0, 1, 39, 40, 41, 79, 81, 1024, 2048):
        context.update(allocation=heap, phase='create');cpu.mem_write(zone, bytes(0x1000))
        cpu.mem_write(stack + 0x8000, struct.pack('<3I', stop, size, size))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_ESI, zone)
        cpu.emu_start(0x415c40, stop, count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop or cpu.reg_read(UC_X86_REG_EAX) & 255 != 1:
            raise RuntimeError('Original zone creation failed')
        width, height = number(zone + 0x28), number(zone + 0x2c)
        cpu.mem_write(zone + 0x24, struct.pack('<I', map_ptr))
        records = b''
        for z in range(height):
            row = number(number(zone + 0x30) + z * 4)
            for x in range(width):
                records += bytes(cpu.mem_read(row + x * 0x124 + 0x114, 16))
        print('G', size, width, height, fingerprint(records), sep='\t')
        cpu.mem_write(map_ptr + 4, struct.pack('<I', size))
        points = [0, -0.0, -0.01, 1, size - 0.000244140625, size, size + 0.000244140625,
                  39.999996185302734, 40, 40.000003814697266, math.nan, math.inf, -math.inf]
        for x in points:
            for z in points:
                context['phase'] = 'safe'
                cpu.mem_write(stack + 0x8000, struct.pack('<4I', stop, map_ptr, bits(x), bits(z)))
                cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
                cpu.emu_start(0x415360, stop, count=1000)
                if cpu.reg_read(UC_X86_REG_EIP) != stop:
                    raise RuntimeError('Original map-bound check did not return')
                safe, cx, cz = cpu.reg_read(UC_X86_REG_EAX) & 255, -1, -1
                if safe:
                    context['phase'] = 'cell';cpu.mem_write(user + 0xd0, struct.pack('<3f', x, 0, z))
                    cpu.reg_write(UC_X86_REG_EDI, user);cpu.reg_write(UC_X86_REG_EBP, zone);cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
                    cpu.emu_start(0x415fa8, stop, count=1000)
                    pc = cpu.reg_read(UC_X86_REG_EIP)
                    if pc == 0x416022:
                        cx, cz = number(stack + 0x800c), number(stack + 0x8008)
                    elif pc != 0x416151:
                        raise RuntimeError('Original entry coordinate check did not finish')
                print('P', size, bits(x), bits(z), safe, cx, cz, sep='\t')
        context['phase'] = 'view'
        for cx, cz in sorted(set(((0, 0), (width - 1, height - 1)))):
            cell = number(number(zone + 0x30) + cz * 4) + cx * 0x124
            left, right, bottom, top = struct.unpack('<4f', cpu.mem_read(cell + 0x114, 16))
            for x in (left - 0.01, left, right, right + 0.01, math.nan):
                for z in (bottom - 0.01, bottom, top, top + 0.01, math.nan):
                    cpu.mem_write(stack + 0x8000, struct.pack('<3I', stop, bits(x), bits(z)))
                    cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_ECX, cell)
                    cpu.emu_start(0x4153c0, stop, count=1000)
                    if cpu.reg_read(UC_X86_REG_EIP) != stop:
                        raise RuntimeError('Original cell-view test did not return')
                    print('C', size, cx, cz, bits(x), bits(z), cpu.reg_read(UC_X86_REG_EAX) & 255, sep='\t')
            context['phase'] = 'move'
            for x in points:
                for z in points:
                    cpu.mem_write(user, bytes(0x6000));cpu.mem_write(user + 0x5758, struct.pack('<I', 4))
                    cpu.mem_write(user + 0xe4, struct.pack('<2I', cx, cz))
                    cpu.mem_write(stack + 0x8000, struct.pack('<5I', stop, zone, bits(x), bits(123), bits(z)))
                    cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_EAX, user)
                    cpu.emu_start(0x416370, stop, count=10000)
                    if cpu.reg_read(UC_X86_REG_EIP) != stop:
                        raise RuntimeError('Original local movement did not return')
                    print('M', size, cx, cz, bits(x), bits(z), cpu.reg_read(UC_X86_REG_EAX) & 255, number(user + 0xe4), number(user + 0xe8), sep='\t')


if __name__ == '__main__':
    main()
