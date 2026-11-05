#!/usr/bin/env python3
"""Trace the original exchange tail, recording transfer and completion calls."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, actor, peer = 0x20000000, 0x30000000, 0x30010000
    cpu.mem_map(stack, 0x10000)
    cpu.mem_map(actor, 0x20000)
    cpu.mem_write(0x46f910, b'\xc2\x04\x00')
    cpu.mem_write(0x4d4d20, b'\xc2\x08\x00')
    events = []

    def hook(machine, address, size, data):
        if address == 0x46f767:
            machine.emu_stop()
            return
        sp = machine.reg_read(UC_X86_REG_ESP)
        target = machine.reg_read(UC_X86_REG_ECX)
        label = 'A' if target == actor else 'B' if target == peer else '?'
        if address == 0x46f910:
            source = struct.unpack('<I', machine.mem_read(sp + 4, 4))[0]
            offer = machine.reg_read(UC_X86_REG_EDX)
            source_label = 'A' if source == actor else 'B' if source == peer else '?'
            if offer != source + 0x1568:
                raise RuntimeError('transfer did not use source exchange owner')
            events.append('T' + label + source_label)
        else:
            ptr, length = struct.unpack('<II', machine.mem_read(sp + 4, 8))
            events.append(label + bytes(machine.mem_read(ptr, length)).hex())

    for address in (0x46f910, 0x4d4d20, 0x46f767):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    sp = stack + 0x8000
    cpu.mem_write(sp + 0x0c, b'\x04\x0a\xff')
    cpu.reg_write(UC_X86_REG_ESP, sp)
    cpu.reg_write(UC_X86_REG_EBX, actor)
    cpu.reg_write(UC_X86_REG_EBP, peer)
    cpu.emu_start(0x46f72a, 0x46f767, count=10000)
    if events != ['TAB', 'TBA', 'A040a00', 'B040a00']:
        raise RuntimeError(f'unexpected original exchange tail: {events}')
    print(f'# ps_game.exe SHA256 {digest}; CUser::Exchange 0046f72a..0046f767')
    print(','.join(events))


if __name__ == '__main__':
    main()
