#!/usr/bin/env python3
"""Complete StatusAttackStop and original user-hit/stop/death caller traces."""
import random
import struct
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_WRITE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x30000)
    user, mob, party, definition, stack, stop = [base+x for x in (0, 0x8000, 0xa000, 0xb000, 0x20000, 0x2f000)]
    offsets = (0x142c, 0x1274, 0x1528, 0x143c, 0x1440, 0x152c)
    active_stop = False
    writes, events, credits = [], [], []
    standalone = False

    def put(at, *numbers):
        cpu.mem_write(at, struct.pack('<'+'I'*len(numbers), *[n & 0xffffffff for n in numbers]))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    def state():
        return ','.join(str(get(user+x)) for x in offsets)

    def write(c, access, at, size, value, data):
        if active_stop:
            writes.append(f'{at-user:x}:{size}:{value & 0xffffffff}')

    def hook(c, at, size, data):
        nonlocal active_stop
        if at == 0x489ed0:
            active_stop = True
        if active_stop:
            if not 0x489ed0 <= at <= 0x489f0c:
                raise RuntimeError('StatusAttackStop unexpectedly called external code')
            if at == 0x489f0c:
                active_stop = False
            return
        if standalone:
            raise RuntimeError('Standalone stop executed code outside function')
        pop, result = 0, None
        if at == 0x41d760:
            result = get(mob+0x124)
        elif at == stop+16:
            result = 123
        elif at == 0x4b8dd0:
            esp = c.reg_read(UC_X86_REG_ESP)
            record = [c.reg_read(UC_X86_REG_EBX)] + [get(esp+x) for x in (4, 8, 12, 16)]
            events.append('A:'+','.join(map(str, record)))
            credits.append(record); pop = 16
        elif at in (0x490ab0, 0x4b7490):
            events.append('R')
        elif at == 0x4910f0:
            esp = c.reg_read(UC_X86_REG_ESP)
            events.append('D:'+state()+','+str(get(esp+4))+','+str(get(esp+8))); pop = 8
        else:
            return
        esp = c.reg_read(UC_X86_REG_ESP)
        ret = get(esp)
        c.reg_write(UC_X86_REG_ESP, esp+4+pop)
        if result is not None:
            c.reg_write(UC_X86_REG_EAX, result)
        c.reg_write(UC_X86_REG_EIP, ret)

    cpu.hook_add(UC_HOOK_CODE, hook)
    cpu.hook_add(UC_HOOK_MEM_WRITE, write)
    put(0x5210e4, stop+16)
    rng = random.Random(0x489ed0)
    print(f'# ps_game.exe SHA256 {digest}; complete CUser::StatusAttackStop 00489ed0, no function-body ports; CUser::AttackMob caller slice 0044cf04..0044cf61 includes real DamageParty/Damage/Stop, observed Add/status/clock/retarget/Death ports')
    print('# mode recovery target_type previous next target_id; S standalone, C composed inputs hp damage has_party; final_stop_state ordered_writes ordered_external_events')
    values = (0, 1, 2, 3, 0xffffffff, 0x80000000, 0x7fffffff, 99)
    allowed = {user+offset+i for offset in offsets[:-1] for i in range(4)}
    for case in range(1024):
        composed = case >= 512
        mode, recovery = values[case % 8], values[(case//8) % 8]
        initial = [mode, recovery, rng.choice(values), rng.getrandbits(32), rng.getrandbits(32), rng.choice(values)]
        cpu.mem_write(user, b'\xa5'*0x2000)
        for offset, value in zip(offsets, initial):
            put(user+offset, value)
        writes, events, credits = [], [], []
        if not composed:
            standalone = True
            before = bytes(cpu.mem_read(user, 0x2000))
            put(stack, stop); cpu.reg_write(UC_X86_REG_ESP, stack); cpu.reg_write(UC_X86_REG_EAX, user)
            cpu.emu_start(0x489ed0, stop, count=1000)
            after = bytes(cpu.mem_read(user, 0x2000))
            if any(a != b and user+i not in allowed for i, (a, b) in enumerate(zip(before, after))):
                raise RuntimeError('stop mutated unexpected user storage')
            if cpu.reg_read(UC_X86_REG_ESP) != stack+4:
                raise RuntimeError('unbalanced stop return')
            extra = []
        else:
            standalone = False
            hp, damage, has_party = rng.choice((0, 1, 100, 1000)), rng.choice((0, 1, 99, 100, 0xffffffff)), rng.randrange(2)
            cpu.mem_write(mob, bytes(0x1000)); cpu.mem_write(stack, bytes(0x100))
            put(mob+0x224, hp); put(mob+0xa8, definition); put(definition+0x6c, 0)
            put(user+0xdc, 42); put(user+0x1230, 3); put(user+0x173c, party if has_party else 0); put(party+8, 77)
            put(stack+0x14, damage, 0, 0)
            cpu.reg_write(UC_X86_REG_ESP, stack); cpu.reg_write(UC_X86_REG_EDI, user); cpu.reg_write(UC_X86_REG_EBP, mob); cpu.reg_write(UC_X86_REG_EBX, 0)
            cpu.emu_start(0x44cf04, 0x44cf61, count=10000)
            if cpu.reg_read(UC_X86_REG_EIP) != 0x44cf61 or cpu.reg_read(UC_X86_REG_ESP) != stack:
                raise RuntimeError('unbalanced caller slice')
            extra = [hp, damage, has_party, int(bool(cpu.reg_read(UC_X86_REG_EBX)&255)), get(mob+0x224)]
        print('C' if composed else 'S', *initial, *extra, state(), '/'.join(writes) or '-', '/'.join(events) or '-', sep='\t')


if __name__ == '__main__':
    main()
