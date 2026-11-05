#!/usr/bin/env python3
"""Trace the original FindItemRebirth inventory scan."""
import itertools
import struct
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    user, memory, stack, stop = 0x30000000, 0x31000000, 0x20000000, 0x10000000
    for base in (user, memory, stack, stop):
        cpu.mem_map(base, 0x10000)
    def put(address, value):
        cpu.mem_write(address, struct.pack('<I', value & 0xffffffff))
    def get(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]
    print(f'# ps_game.exe SHA256 {digest}; CUser::FindItemRebirth 0045f740')
    print('# bag limit, flag, first duration, second duration, first match, second match, found, bag, slot')
    for limit, flag, first_duration, second_duration, first_match, second_match in itertools.product((0,1,2), (0,1), (0,7), (0,7), (0,1), (0,1)):
        cpu.mem_write(user, bytes(0x2000)); cpu.mem_write(memory, bytes(0x2000))
        put(user + 0x1304, limit)
        for i, (bag, slot, duration, match) in enumerate(((1,5,first_duration,first_match),(2,3,second_duration,second_match))):
            item, info = memory + 0x100 + i * 0x200, memory + 0x200 + i * 0x200
            put(user + 0x1c4 + (bag * 24 + slot) * 4, item)
            put(item + 0x30, info); put(info + 0xa0, 26 if match else 25)
            cpu.mem_write(info + 0x46, bytes((37,))); cpu.mem_write(info + 0x34, struct.pack('<H', duration))
        out_bag, out_slot = memory + 0x1000, memory + 0x1004
        put(out_bag, 0xdead); put(out_slot, 0xbeef); put(stack + 0xf000, stop); put(stack + 0xf004, flag)
        cpu.reg_write(UC_X86_REG_EBX, user); cpu.reg_write(UC_X86_REG_ECX, out_slot); cpu.reg_write(UC_X86_REG_EDX, out_bag); cpu.reg_write(UC_X86_REG_ESP, stack + 0xf000)
        cpu.emu_start(0x45f740, stop, count=10000)
        found = cpu.reg_read(UC_X86_REG_EAX) & 0xff
        print(limit, flag, first_duration, second_duration, first_match, second_match, found, get(out_bag) if found else 0, get(out_slot) if found else 0, sep='\t')

if __name__ == '__main__':
    main()
