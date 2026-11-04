#!/usr/bin/env python3
"""Execute original RemApplySkill damage coordination; observe required world ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x10000)
    mob, info, skill, respawn, zone, stop = [base+x for x in (0, 0x2000, 0x3000, 0x4000, 0x5000, 0xf000)]
    state = {}

    def pack(*values):
        return struct.pack('<'+'I'*len(values), *[x & 0xffffffff for x in values])

    def put(at, *values):
        cpu.mem_write(at, pack(*values))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    def cstring(at, name):
        cpu.mem_write(at, name.encode()+b'\0')

    def snapshot():
        values = [get(mob+0x5c), get(mob+0x224), get(mob+0x284), int(get(mob+0xc84)!=0),
                  cpu.mem_read(respawn+0x1c, 1)[0], int(get(mob+0x60)!=0),
                  get(skill+0xc), get(skill+0x10), get(skill+0x24), get(skill+0x28), get(skill+0x2c),
                  struct.unpack('<H', cpu.mem_read(info+0x32, 2))[0]]
        return (pack(*values)+bytes(cpu.mem_read(mob+0xcf4, 64))+bytes(cpu.mem_read(info+3, 32))).hex()

    def toggle_zone():
        put(mob+0x60, 0 if get(mob+0x60) else zone)

    for at, code in [(0x503a3f, b'\xc3'), (0x505316, b'\xc3'),
                     (0x4a3be0, b'\xc2\x04\x00'), (0x490ee0, b'\xc2\x18\x00'),
                     (0x490cf0, b'\xc2\x18\x00'), (0x4910f0, b'\xc2\x08\x00'),
                     (0x421220, b'\xc2\x20\x00'), (stop+16, b'\xc3')]:
        cpu.mem_write(at, code)
    put(0x5210e4, stop+16)

    def hook(machine, address, size, data):
        esp = machine.reg_read(UC_X86_REG_ESP)
        mode = state['mode']
        if address == stop+16:
            state['events'].append('K'+snapshot())
            if mode & 8:
                put(skill+0x24, get(skill+0x24) ^ 0x80000000)
            machine.reg_write(UC_X86_REG_EAX, state['now'])
        elif address == 0x4a3be0:
            assert machine.reg_read(UC_X86_REG_EAX) == info
            elapsed = get(esp+4)
            state['events'].append('T'+pack(elapsed).hex()+snapshot())
            cpu.mem_write(machine.reg_read(UC_X86_REG_ESI), pack(*state['amounts']))
            if mode & 1:
                put(mob+0x284, 1)
                put(mob+0x224, 0x76543210)
                put(skill+0x24, 0x7fffffff)
                put(skill+0x28, 5, 0x12345678)
                cpu.mem_write(info+3, b'TimeChanged\0'.ljust(32, b'\0'))
        elif address in (0x490ee0, 0x490cf0):
            assert get(esp+4) == mob and [get(esp+x) for x in (16, 20, 24)] == [0, 0, 0]
            kind, ident = get(esp+8), get(esp+12)
            value = machine.reg_read(UC_X86_REG_EAX)
            amounts = bytes(cpu.mem_read(value, 12)) if address == 0x490ee0 else pack(value)
            state['events'].append(('P' if address == 0x490ee0 else 'H')+pack(kind, ident).hex()+amounts.hex()+snapshot())
            if mode & 2:
                put(mob+0x5c, get(mob+0x5c) ^ 0x10203040)
                put(mob+0x224, 0x12340000+state['case'])
                put(mob+0x284, 0)
                put(skill+0xc, get(skill+0xc) ^ 0xabcd0000, get(skill+0x10)+0x101)
                put(skill+0x28, 9, 0x89abcdef)
                cstring(mob+0xcf4, 'DamageUser')
                cstring(mob+0xd14, 'DamageSkill')
                toggle_zone()
                if address == 0x490ee0:
                    put(value, get(value)^0xffff, get(value+4)^0x80000000, get(value+8)^0xffff0000)
            machine.reg_write(UC_X86_REG_EAX, state['killed'])
        elif address == 0x4910f0:
            assert machine.reg_read(UC_X86_REG_ECX) == mob
            state['events'].append('X'+pack(get(esp+4), get(esp+8)).hex()+snapshot())
            if mode & 4:
                put(mob+0x5c, 0x99887766)
                put(mob+0x224, 0)
                put(skill+0xc, 0xffff1234, 0xffffff80)
                cstring(mob+0xcf4, 'DeathUser')
                cstring(mob+0xd14, 'DeathSkill')
                toggle_zone()
        elif address == 0x421220:
            assert get(esp+4) == get(mob+0x60) != 0
            assert get(esp+12) == 15 and get(esp+16) == mob+0x50
            assert [get(esp+x) for x in (20, 24, 28, 32)] == [0x42700000, 0, 0, 6]
            state['events'].append('V'+bytes(cpu.mem_read(get(esp+8), 15)).hex()+snapshot())
    cpu.hook_add(UC_HOOK_CODE, hook)

    print(f'# ps_game.exe SHA256 {digest}; real CMob::RemApplySkill 004a0da0 damage branches and IsBothmob 0044c610; tick, TimeDamage 004a3be0, Damage 00490ee0/00490cf0, Death 004910f0 and PSendView 00421220 are observed required ports, not their reconstructed bodies.')
    print('# case now mode killed initial144bytehex amounts3wordhex events final144bytehex. State: id/hp/lua/respawn/both/zone/skillid/level/prev/type/target/detail then previous-user32/skill32/definition-name32. Events K clock, T time calculation, P periodic damage, H captured-HP damage, X death, V exact15byte result; each includes live state. Mutation bits 1 time, 2 damage, 4 death, 8 clock.')
    boundaries = [0, 1, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff, 1000, 0x12345678]
    names = ['', 'Burn', 'A'*31, 'Ice']
    for case in range(1024):
        cpu.mem_write(base, bytes(0xe000))
        detail = 0x3d if case % 2 == 0 else 0x3f
        flags = (case//64) % 16
        put(mob+0x5c, 0xabcdef00+case)
        put(mob+0x224, boundaries[(case//5) % 8])
        put(mob+0x284, 0xffffffff if flags & 1 else 0)
        put(mob+0xc84, respawn if flags & 2 else 0)
        cpu.mem_write(respawn+0x1c, bytes([bool(flags & 4)]))
        put(mob+0x60, zone if flags & 8 else 0)
        put(skill+0xc, boundaries[(case//3) % 8], boundaries[(case//9) % 8])
        put(skill+0x24, boundaries[(case//7) % 8], boundaries[(case//11) % 8], 0x87650000+case)
        put(skill+0x30, info)
        cpu.mem_write(info+0x32, struct.pack('<H', detail))
        cpu.mem_write(info+3, names[(case//13) % 4].encode().ljust(32, b'\0'))
        cpu.mem_write(mob+0xcf4, b'U'*32+b'S'*32)
        cstring(mob+0xcf4, 'BeforeUser')
        cstring(mob+0xd14, 'BeforeSkill')
        now = boundaries[(case//8) % 8]
        amounts = [boundaries[(case+i) % 8] for i in range(3)]
        mode, killed = (case//2) % 16, (case//32) % 2
        state.update(case=case, mode=mode, killed=killed, now=now, amounts=amounts, events=[])
        initial = snapshot()
        put(base+0xe000, stop)
        cpu.reg_write(UC_X86_REG_ESP, base+0xe000)
        cpu.reg_write(UC_X86_REG_ECX, mob)
        cpu.reg_write(UC_X86_REG_EBX, skill)
        cpu.emu_start(0x4a0da0, stop, count=10000)
        assert cpu.reg_read(UC_X86_REG_EIP) == stop
        print(case, now, mode, killed, initial, pack(*amounts).hex(), ','.join(state['events']) or '-', snapshot(), sep='\t')


if __name__ == '__main__':
    main()
