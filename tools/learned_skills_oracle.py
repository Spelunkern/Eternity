#!/usr/bin/env python3
"""Execute original learned skill reconstruction up to success or the invalid-definition error. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, packet, info, objects = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000
    for address in (stack, user, packet, info, objects):
        cpu.mem_map(address, 0x10000)
    for address in (0x402770, 0x40a780):
        cpu.mem_write(address, b'\xc3')
    context = dict(allocated=0, invalid=0)

    def hook(machine, address, size, _):
        if address == 0x402770:
            context['id'] = machine.reg_read(UC_X86_REG_EAX)
            machine.reg_write(UC_X86_REG_EAX, 0 if context['id'] == 999 else info)
        elif address == 0x40a780:
            machine.reg_write(UC_X86_REG_EAX, objects + context['allocated'] * 0x80)
            context['allocated'] += 1
        elif address in (0x46e216, 0x46e702):
            if address == 0x46e216:
                context['invalid'] = context['id']
            machine.emu_stop()

    cpu.hook_add(UC_HOOK_CODE, hook)
    print(f'# ps_game.exe SHA256 {digest}; PacketUserDBSetInfo 0046d780 case 0504; error branch stopped before logging/disconnection')
    print('# count,tick,invalid_index(-1=none); loaded_count,invalid_id; loaded (id,level,number,end_tick)')
    for count in (0, 1, 3, 255):
        for tick in (0, 1234567, 0x80000000, 0xffffffff):
            for bad in sorted(set((-1, 0, max(0, count - 1)))):
                context.update(allocated=0, invalid=0)
                cpu.mem_write(user, bytes(0x6000))
                entries = b''.join(struct.pack('<HBBH', 999 if i == bad else 1000 + i, i % 3 + 1, i, (0, 1, 65535)[i % 3]) for i in range(count))
                cpu.mem_write(packet, struct.pack('<HIB', 0x504, 7001, count) + entries)
                cpu.mem_write(stack + 0x8000, bytes(0x300))
                cpu.mem_write(stack + 0x8018, struct.pack('<I', tick))
                cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
                cpu.reg_write(UC_X86_REG_EBX, packet)
                cpu.reg_write(UC_X86_REG_EBP, user)
                cpu.emu_start(0x46e166, 0x46e20f, count=100000)
                if cpu.reg_read(UC_X86_REG_EIP) not in (0x46e20f, 0x46e216, 0x46e702):
                    raise RuntimeError('Original learned skill branch did not finish')
                loaded = struct.unpack('<I', cpu.mem_read(user + 0xac0, 4))[0]
                result = []
                for i in range(loaded):
                    pointer = struct.unpack('<I', cpu.mem_read(user + 0xac4 + i * 4, 4))[0]
                    identity, level, number = struct.unpack('<3I', cpu.mem_read(pointer + 0xc, 12))
                    end = struct.unpack('<I', cpu.mem_read(pointer + 0x1c, 4))[0]
                    result.extend((identity, level, number, end))
                print(count, tick, bad, loaded, context['invalid'], *result, sep='\t')


if __name__ == '__main__':
    main()
