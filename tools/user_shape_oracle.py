#!/usr/bin/env python3
"""Execute original SendUserShape with isolated party/guild/transport ports. Requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, objects, copied, zone, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x10000000
    for address in (stack, user, objects, copied, zone, stop):
        cpu.mem_map(address, 0x10000)
    for address, cleanup in ((0x4208c0, 12), (0x48a330, 4), (0x476430, 4), (0x505316, 0)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    context = {}

    def hook(machine, address, size, _):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == 0x4208c0:
            actual_zone, ptr, length = struct.unpack('<3I', machine.mem_read(sp + 4, 12))
            if (actual_zone, machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX)) != (zone, 17, 23):
                raise RuntimeError('Unexpected user-shape cell route')
            context['packet'] = bytes(machine.mem_read(ptr, length)).hex()
        elif address == 0x48a330:
            context['social'] += 1
            machine.reg_write(UC_X86_REG_EAX, context['scenario'] % 3)
        elif address == 0x476430:
            if machine.reg_read(UC_X86_REG_EBX) != user:
                raise RuntimeError('Unexpected guild owner')
            ptr = struct.unpack('<I', machine.mem_read(sp + 4, 4))[0]
            machine.mem_write(ptr, context['guild'] + b'\0')

    cpu.hook_add(UC_HOOK_CODE, hook)
    crafts = (b'', b'1234567890123456789', b'01234567890123456789', b'01234567890123456799',
              b'012345678901234567x1', b'0123456789012345671/', b'012345678901234567890')
    print(f'# ps_game.exe SHA256 {digest}; CUser::SendUserShape 00482ad0. Ghidra work/actor-spatial-actions; GetPartyType/GetGuildName isolated inputs.')
    print('# scenario; social lookups; hex payload or - when not in a zone. Original unused normal-name padding is zero in the input.')
    for s in range(256):
        guild = (b'', b'G', b'ABCDEFGHIJKLMNOPQRSTUVWX', b'Eternity')[(s >> 4) % 4]
        context.update(scenario=s, guild=guild, social=0, packet='-')
        for address in (user, objects, copied, stack):
            cpu.mem_write(address, bytes(0x10000))
        cpu.mem_write(user + 0xdc, struct.pack('<I', 9001))
        cpu.mem_write(user + 0xe0, struct.pack('<3I', zone if s & 1 else 0, 17, 23))
        cpu.mem_write(user + 0x1354, bytes([100 if s & 2 else 0]))
        cpu.mem_write(user + 0x135c, struct.pack('<I', copied if s & 4 else 0))
        cpu.mem_write(user + 0x142c, struct.pack('<I', s % 4))
        cpu.mem_write(user + 0x1434, bytes([(s * 7) & 255]))
        identity = (s % 3, (s >> 2) % 4, (s * 3) & 255, (s * 5) & 255, s % 5, s % 6, s % 2, s % 4)
        for offset, value in zip((0x12d, 0x12e, 0x131, 0x132, 0x133, 0x134, 0x135, 0x12f), identity):
            cpu.mem_write(user + offset, bytes([value]))
        cpu.mem_write(user + 0x14c, struct.pack('<I', 0xfedcba98 + s))
        name = (b'Actor', b'ABCDEFGHIJKLMNOPQRST', b'\xb0\xa1\xb3\xaa')[(s >> 3) % 3]
        cpu.mem_write(user + 0x188, name + b'\0')
        for slot in range(13):
            present = bool(s & 8) if slot == 7 else (s + slot) % 3 != 0
            if not present:
                continue
            item = objects + slot * 0x100
            cpu.mem_write(user + 0x1c4 + slot * 4, struct.pack('<I', item))
            cpu.mem_write(item + 0x40, bytes(((slot * 7 + s) & 255, (slot * 3 + s) & 255)))
            cpu.mem_write(item + 0x46, bytes((s + j * 17) & 255 for j in range(6)))
            cpu.mem_write(item + 0x4c, crafts[(slot + s) % len(crafts)] + b'\0')
        cpu.mem_write(copied + 8, bytes((s + j * 11) & 255 for j in range(11)))
        cpu.mem_write(copied + 0x14, struct.pack('<I', 0x12345678 + s))
        cpu.mem_write(copied + 0x18, bytes((s + j * 13) & 255 for j in range(24)))
        cpu.mem_write(copied + 0x30, bytes((s + j * 7) & 255 for j in range(21)))
        cpu.mem_write(copied + 0x45, bytes((s + j * 19) & 255 for j in range(31)))
        cpu.mem_write(copied + 0x64, bytes([(67, 73, 91, 97)[s % 4]]))
        cpu.mem_write(stack + 0x8000, struct.pack('<2I', stop, user))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.reg_write(UC_X86_REG_EBX, 0)
        cpu.reg_write(UC_X86_REG_EDX, 0)
        cpu.emu_start(0x482ad0, stop, count=10000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original SendUserShape did not return')
        print(s, context['social'], context['packet'], sep='\t')


if __name__ == '__main__':
    main()
