#!/usr/bin/env python3
"""Execute the original LevelUp party slice with real maximum recomputation."""
import struct
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x100000)
    party, user, stack, lock, unlock = [base + n for n in (0, 0x10000, 0xf0000, 0xff000, 0xff010)]
    def put(at, value):
        cpu.mem_write(at, struct.pack('<I', value & 0xffffffff))
    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]
    cpu.mem_write(lock, b'\xc2\x04\x00')
    cpu.mem_write(unlock, b'\xc2\x04\x00')
    put(0x5210cc, lock); put(0x5210d0, unlock)
    cpu.mem_write(0x4455d0, b'\xc3')  # Observe SendParty transport boundary.
    context = {}
    def hook(machine, address, size, data):
        if address == lock:
            context['events'] += '['
            if context['detach']:
                put(user + 0x173c, 0)
        elif address == unlock:
            context['events'] += ']'
        elif address == 0x444d80:
            context['events'] += 'R'
        elif address == 0x4455d0:
            assert machine.reg_read(UC_X86_REG_EAX) == 8
            context['events'] += 'T' + bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX), 8)).hex()
            context['observed'] = get(party + 0x10c)
    cpu.hook_add(UC_HOOK_CODE, hook)
    print(f'# ps_game.exe SHA256 {digest}; CUser::LevelUp 0048ad70 slice 0048af5c..0048b006; real ChkPartyExpByLevel 00444d80; lock/unlock and SendParty are observed ports.')
    print('# kind old_level membership(0 absent/1 present/2 detached on lock) actor_id actor_level count slots... final_max observed_max events')
    for case in range(256):
        kind = case % 2
        membership = (case // 2) % 3
        count = (1, 2, 3, 7, 16, 30)[(case // 6) % 6]
        old = (0, 60, 65535, 0x80000000)[(case // 36) % 4]
        actor_id = 0xf1234567 + case
        level = (1, 8, 30, 59, 60)[(case // 5) % 5]
        levels = [level] + [None if kind and (i + case) % 3 == 0 else (i * 17 + case) % 61 for i in range(1, count)]
        put(party + 0x10, count); put(party + 0x10c, old)
        cpu.mem_write(party + 0x114, bytes([kind]))
        put(user + 0xdc, actor_id); cpu.mem_write(user + 0x136, struct.pack('<H', level))
        put(user + 0x173c, party if membership else 0)
        for i, member_level in enumerate(levels):
            member = user if i == 0 else base + 0x20000 + i * 0x1000
            put(party + 0x18 + i * 8, member if member_level is not None else 0)
            cpu.mem_write(member + 0x136, struct.pack('<H', member_level or 0))
        context.update(events='', observed=-1, detach=membership == 2)
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.reg_write(UC_X86_REG_EDI, 0); cpu.reg_write(UC_X86_REG_EBP, user)
        cpu.emu_start(0x48af5c, 0x48b006, count=10000)
        print(kind, old, membership, actor_id, level, count, *[-1 if x is None else x for x in levels], get(party + 0x10c), context['observed'], context['events'] or '-')

if __name__ == '__main__':
    main()
