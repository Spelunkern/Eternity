#!/usr/bin/env python3
"""Trace original 0206 dispatch with observed cancellation/shop/drop ports."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, packet, stop = 0x20000000, 0x30000000, 0x31000000, 0x10000000
    for address in (0, stack, user, packet, stop):
        cpu.mem_map(address, 0x10000)
    for address in (0x44b3d0, 0x478520):
        cpu.mem_write(address, b'\xc3')
    events = []
    current = {}

    def hook(machine, address, size, data):
        if address == 0x469c37:
            machine.emu_stop()
        elif address == 0x44b3d0:
            if machine.reg_read(UC_X86_REG_EDI) != user:
                raise RuntimeError('Wrong cancellation subject')
            events.append('C')
            if current['mutation'] == 1:
                machine.mem_write(packet + 2, current['replacement'])
        elif address == 0x478520:
            if machine.reg_read(UC_X86_REG_ECX) != user + 0x15dc:
                raise RuntimeError('Wrong shop subject')
            events.append('S')
            if current['mutation'] == 2:
                machine.mem_write(packet + 2, current['replacement'])
        else:
            if machine.reg_read(UC_X86_REG_ECX) != user:
                raise RuntimeError('Wrong drop subject')
            esp = machine.reg_read(UC_X86_REG_ESP)
            bag, slot, count = struct.unpack('<III', machine.mem_read(esp + 4, 12))
            events.append(f'D{bag}:{slot}:{count}')
            machine.emu_stop()

    for address in (0x469c37, 0x44b3d0, 0x478520, 0x45cd10):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    rng = random.Random(0x206)
    print(f'# ps_game.exe SHA256 {digest}; CUser::PacketMaininterface 00468420 opcode 0206 004686f7..00468758; CUser::IsBag 0045b3d0; observed cancellation/shop/drop ports')
    print('# mode max_bags bag slot count mutation replacement_bag replacement_slot replacement_count events')
    for case in range(1024):
        mode = [0, 1, 2, 0xffffffff][case % 4]
        maximum = [-1, 0, 1, 3, 9][(case // 4) % 5]
        bag = [0, 1, 3, 9, 10, 255][(case // 20) % 6]
        slot = [0, 12, 23, 24, 255][(case // 120) % 5]
        count = [0, 1, 255][(case // 17) % 3]
        mutation = case % 3
        replacement_bag = rng.randrange(10)
        replacement = bytes((replacement_bag, rng.randrange(13 if replacement_bag == 0 else 24), rng.randrange(256)))
        for address in (stack, user, packet):
            cpu.mem_write(address, bytes(0x10000))
        cpu.mem_write(user + 0x142c, struct.pack('<I', mode))
        cpu.mem_write(user + 0x1304, struct.pack('<i', maximum))
        cpu.mem_write(packet, struct.pack('<HBBB', 0x206, bag, slot, count))
        cpu.mem_write(stack + 0x8000, struct.pack('<II', stop, user))
        current.update(mutation=mutation, replacement=replacement)
        events.clear()
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.reg_write(UC_X86_REG_ECX, packet)
        cpu.emu_start(0x468420, stop, count=10000)
        if cpu.reg_read(UC_X86_REG_EIP) not in (0x469c37, 0x45cd10):
            raise RuntimeError('Handler failed to reach observed exit')
        print(mode, maximum, bag, slot, count, mutation, *replacement, ','.join(events) or '-', sep='\t')


if __name__ == '__main__':
    main()
