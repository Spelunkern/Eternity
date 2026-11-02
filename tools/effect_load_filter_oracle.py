#!/usr/bin/env python3
"""Execute PacketUserDBSetInfo's persisted-effect filter before AddApplySkill. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, packet, info, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x10000000
    for address in (stack, user, packet, info, stop):
        cpu.mem_map(address, 0x10000)
    for address, cleanup in ((0x402770, 0), (0x487ce0, 12), (0x4d4f10, 8)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    context = dict(available=True, events=[])

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x402770:
            machine.reg_write(UC_X86_REG_EAX, info if context['available'] else 0)
        elif address == 0x487ce0:
            actor, level, remaining = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
            if (actor, level, remaining, machine.reg_read(UC_X86_REG_EAX)) != (user, 1, 30, 321):
                raise RuntimeError('Unexpected AddApplySkill delegation')
            context['events'].append(1)
        elif address == 0x4d4f10:
            pointer, length = struct.unpack('<2I', machine.mem_read(sp + 4, 8))
            data = bytes(machine.mem_read(pointer, length))
            if data != struct.pack('<HIHB', 0x902, 7001, 321, 1):
                raise RuntimeError('Unexpected DB-agent deletion')
            context['events'].append(2)

    cpu.hook_add(UC_HOOK_CODE, hook)
    print(f'# ps_game.exe SHA256 {digest}; PacketUserDBSetInfo 0046d780 case 0505, original branch 0046e2bb..0046e3b2')
    print('# available effect detail change_type; disposition(0=ignore,1=delegate AddApplySkill,2=delete replica)')
    for available in (0, 1):
        for effect in (0, 3, 4):
            for detail, shape in [(x, 0) for x in (73, 74, 75, 76, 77, 82)] + [(90, x) for x in (0, 13, 99, 100, 101, 102, 103, 356)]:
                context.update(available=bool(available), events=[])
                cpu.mem_write(info, bytes(160))
                cpu.mem_write(info, struct.pack('<HB', 321, 1))
                cpu.mem_write(info + 0x31, bytes([effect]))
                cpu.mem_write(info + 0x32, struct.pack('<H', detail))
                cpu.mem_write(info + 0x98, struct.pack('<H', shape))
                cpu.mem_write(user + 0x578c, struct.pack('<I', 7001))
                cpu.mem_write(packet, struct.pack('<HIBHBI', 0x505, 7001, 1, 321, 1, 30))
                cpu.mem_write(stack + 0x8000, bytes(1024))
                cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
                cpu.reg_write(UC_X86_REG_EBX, packet)
                cpu.reg_write(UC_X86_REG_EBP, user)
                cpu.emu_start(0x46e2bb, 0x46e3b2, count=10000)
                if cpu.reg_read(UC_X86_REG_EIP) != 0x46e3b2 or len(context['events']) > 1:
                    raise RuntimeError('Original filter did not finish')
                print(available, effect, detail, shape, context['events'][0] if context['events'] else 0, sep='\t')


if __name__ == '__main__':
    main()
