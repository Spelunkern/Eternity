#!/usr/bin/env python3
"""Trace original trade offer removal and money updates with observed sends."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, actor, peer, packet, stop = 0x20000000, 0x30000000, 0x30010000, 0x31000000, 0x10000000
    for address, size in ((0, 0x10000), (stack, 0x10000), (actor, 0x20000), (packet, 0x10000), (stop, 0x1000)):
        cpu.mem_map(address, size)
    for address, ret in ((0x478520, b'\xc3'), (0x4d4d20, b'\xc2\x08\x00'), (0x505316, b'\xc3')):
        cpu.mem_write(address, ret)
    events = []
    def hook(machine, address, size, data):
        if address == 0x478520:
            if machine.reg_read(UC_X86_REG_ECX) != actor + 0x15dc:
                raise RuntimeError('wrong shop owner')
            events.append('E')
        else:
            sp = machine.reg_read(UC_X86_REG_ESP)
            ptr, length = struct.unpack('<II', machine.mem_read(sp + 4, 8))
            target = machine.reg_read(UC_X86_REG_ECX)
            if target not in (actor, peer):
                raise RuntimeError('wrong send owner')
            events.append(('A' if target == actor else 'B') + bytes(machine.mem_read(ptr, length)).hex())
    for address in (0x478520, 0x4d4d20):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    rng = random.Random(0xa0708)
    print(f'# ps_game.exe SHA256 {digest}; CUser::PacketExchange 0046ec90, 0a07 0046f1c3..0046f259, 0a08 0046f25a..0046f2c6; CUser::ExchangeCancleReady 0046f450; shop and sends observed')
    print('# op mode peer ready peer_ready money value slot_bag slot_slot slot_count events final_ready final_peer_ready final_money final_bag')
    for case in range(1024):
        op = 7 + case % 2
        mode = [0, 1, 2, 3, 4][(case // 2) % 5]
        present = (case // 10) % 2
        ready = [0, 1, 255][(case // 20) % 3]
        peer_ready = [0, 1, 255][(case // 60) % 3]
        money = rng.choice((0, 1, 100, 0xffffffff))
        index = rng.choice((0, 3, 7, 8, 255))
        value = index if op == 7 else rng.choice((0, 1, money, 0xffffffff))
        bag, slot, count = rng.choice((0, 1, 9)), rng.randrange(24), rng.randrange(256)
        for base, size in ((stack, 0x10000), (actor, 0x20000), (packet, 0x10000)):
            cpu.mem_write(base, bytes(size))
        cpu.mem_write(actor + 0x1568, struct.pack('<II', mode, peer if present else 0))
        cpu.mem_write(actor + 0x140, struct.pack('<I', money))
        cpu.mem_write(actor + 0x1570, struct.pack('<I', 0x12345678))
        cpu.mem_write(actor + 0x158c, bytes((ready,)))
        cpu.mem_write(peer + 0x158c, bytes((peer_ready,)))
        if index < 8:
            cpu.mem_write(actor + 0x1574 + 3 * index, bytes((bag, slot, count)))
        cpu.mem_write(packet, struct.pack('<HI', 0xa00 + op, value))
        cpu.mem_write(stack + 0x8000, struct.pack('<I', stop))
        events.clear()
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.reg_write(UC_X86_REG_ECX, actor)
        cpu.reg_write(UC_X86_REG_EDX, packet)
        cpu.emu_start(0x46ec90, stop, count=10000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('original handler did not return')
        final_money = struct.unpack('<I', cpu.mem_read(actor + 0x1570, 4))[0]
        final_bag = cpu.mem_read(actor + 0x1574 + 3 * index, 1)[0] if index < 8 else bag
        print(op, mode, present, ready, peer_ready, money, value, bag, slot, count, ','.join(events), cpu.mem_read(actor + 0x158c, 1)[0], cpu.mem_read(peer + 0x158c, 1)[0], final_money, final_bag, sep='\t')

if __name__ == '__main__':
    main()
