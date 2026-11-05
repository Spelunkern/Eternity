#!/usr/bin/env python3
"""Trace ExchangeItemAdd until its first whole/split item decision."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, recipient, source, exchange, items, definitions, stop = 0x20000000, 0x30000000, 0x30010000, 0x31000000, 0x32000000, 0x33000000, 0x10000000
    for address, size in ((stack, 0x10000), (recipient, 0x20000), (exchange, 0x10000), (items, 0x10000), (definitions, 0x10000), (stop, 0x1000)):
        cpu.mem_map(address, size)
    cpu.mem_write(0x505316, b'\xc3')
    rng = random.Random(0x46f910)
    print(f'# ps_game.exe SHA256 {digest}; CUser::ExchangeItemAdd 0046f910..0046f9e7 / 0046fb21')
    print('# max_bags recipient_mask1 recipient_mask2 source_mask offers counts stacks index bag slot whole')
    for case in range(512):
        for address, size in ((stack, 0x10000), (recipient, 0x20000), (exchange, 0x10000), (items, 0x10000), (definitions, 0x10000)):
            cpu.mem_write(address, bytes(size))
        maximum = rng.randrange(3)
        masks = [rng.choice([0xffffff, rng.getrandbits(24)]) for _ in range(2)]
        source_mask = rng.randrange(256)
        offers = bytearray(24)
        counts = bytes(rng.randrange(1, 6) for _ in range(8))
        stacks = bytes(rng.choice([1, 20]) for _ in range(8))
        for i in range(8):
            offers[i*3:i*3+3] = bytes((rng.choice([0, 1, 1]), i, rng.randrange(1, 6)))
            if source_mask & (1 << i):
                item, definition = items + i*0x100, definitions + i*0x100
                cpu.mem_write(source + 0x1c4 + (24+i)*4, struct.pack('<I', item))
                cpu.mem_write(item+0x30, struct.pack('<I', definition))
                cpu.mem_write(item+0x42, bytes((counts[i],)))
                cpu.mem_write(definition+0x4b, bytes((stacks[i],)))
        for bag in range(1, 3):
            for slot in range(24):
                if masks[bag-1] & (1 << slot):
                    cpu.mem_write(recipient+0x1c4+(bag*24+slot)*4, struct.pack('<I', 1))
        cpu.mem_write(recipient+0x1304, struct.pack('<i', maximum))
        cpu.mem_write(exchange+12, bytes(offers))
        sp = stack+0x8000
        cpu.mem_write(sp, struct.pack('<II', stop, source))
        cpu.reg_write(UC_X86_REG_ESP, sp)
        cpu.reg_write(UC_X86_REG_ECX, recipient)
        cpu.reg_write(UC_X86_REG_EDX, exchange)
        # Stop at the whole/split branch, or just before the original return.
        from unicorn import UC_HOOK_CODE
        def hook(machine, address, size, data):
            machine.emu_stop()
        handles = [cpu.hook_add(UC_HOOK_CODE, hook, begin=a, end=a) for a in (0x46f9e7, 0x46fb21, 0x4700f8)]
        cpu.emu_start(0x46f910, stop, count=10000)
        ip = cpu.reg_read(UC_X86_REG_EIP)
        if ip == 0x4700f8:
            index, bag, slot, whole = 255, 0, 0, 0
        elif ip in (0x46f9e7, 0x46fb21):
            index = (cpu.reg_read(UC_X86_REG_EDI) - exchange - 12)//3
            bag, slot, whole = cpu.reg_read(UC_X86_REG_ECX), cpu.reg_read(UC_X86_REG_EBX), int(ip == 0x46f9e7)
        else:
            raise RuntimeError(f'unexpected selection stop {ip:x}')
        for handle in handles:
            cpu.hook_del(handle)
        print(maximum, *masks, source_mask, offers.hex(), counts.hex(), stacks.hex(), index, bag, slot, whole, sep='\t')


if __name__ == '__main__':
    main()
