#!/usr/bin/env python3
"""Complete original Aggro/GetStatus/Add/Reset/GetMaxAggro with shared heap."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    root = 0x30000000
    cpu.mem_map(root, 0x40000)
    mobs = [root, root+0x2000]
    heads = [root+0x4000, root+0x4040]
    scene, table, block = root+0x5000, root+0x5100, root+0x5200
    stack, stop, output = root+0x30000, root+0x3f000, root+0x3e000
    pool = 0x3ed353c
    active = False
    events, objects = [], []
    fail = False
    mutation = 0

    def put(at, *numbers):
        cpu.mem_write(at, struct.pack('<'+'I'*len(numbers), *[n & 0xffffffff for n in numbers]))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    def records(owner):
        at, result = get(heads[owner]+4), []
        while at != heads[owner]:
            result.append([objects.index(at), cpu.mem_read(at+8, 1)[0], *[get(at+i) for i in (12, 16, 20, 24)]])
            at = get(at+4)
        return '/'.join(','.join(map(str, row)) for row in result) or '-'

    def snapshot(tag):
        mob = mobs[0]
        fields = [get(mob+0x224), get(mob+0x124), int(get(mob+0x44) != 0), get(scene+0x28), int(get(mob+0x40) == 0), get(pool+4), get(pool+8), cpu.mem_read(mob+0x1f0, 1)[0], cpu.mem_read(mobs[1]+0x1f0, 1)[0]]
        return tag+':'+','.join(map(str, fields))+';'+records(0)+';'+records(1)

    def mutate_allocation():
        if mutation == 1:
            put(mobs[0]+0x124, 2); put(scene+0x28, 2)
        elif mutation == 2:
            put(mobs[0]+0x124, 1); put(scene+0x28, 1)
            put(mobs[0]+0x44, not get(mobs[0]+0x44)); put(mobs[0]+0x40, 1)
        elif mutation == 3:
            put(mobs[0]+0x224, 0); put(mobs[0]+0x124, 1); put(scene+0x28, 1)
            put(mobs[0]+0x40, 1)

    def hook(c, at, size, data):
        if at == 0x503a3f:
            if active:
                events.append(snapshot('N')); mutate_allocation()
            if fail:
                c.reg_write(UC_X86_REG_EAX, 0)
            else:
                address = root+0x10000+len(objects)*32
                objects.append(address); cpu.mem_write(address, bytes(28)); c.reg_write(UC_X86_REG_EAX, address)
        elif active and at == 0x41d760:
            events.append(snapshot('S'))  # observer only: actual GetStatus executes
        elif active and at in (0x490ab0, 0x4b7490):
            events.append(snapshot('R' if at == 0x490ab0 else 'X'))
            if mutation == 4:
                # Synthetic provider executes the ORIGINAL Reset, returning to
                # Aggro through the retarget call's existing return address.
                c.reg_write(UC_X86_REG_ESI, mobs[0]+0x1f0)
                c.reg_write(UC_X86_REG_EIP, 0x4b8d10)
            else:
                esp = c.reg_read(UC_X86_REG_ESP)
                c.reg_write(UC_X86_REG_EIP, get(esp)); c.reg_write(UC_X86_REG_ESP, esp+4)

    cpu.hook_add(UC_HOOK_CODE, hook)
    cpu.mem_write(stop+16, b'\xc2\x04\x00')
    put(0x5210cc, stop+16); put(0x5210d0, stop+16)
    cpu.mem_write(0x503a3f, b'\xc3')  # allocation port; actual Add/pool run

    def call(at, owner, args=(), kind=1):
        put(stack, stop, *args)
        cpu.reg_write(UC_X86_REG_ESP, stack)
        cpu.reg_write(UC_X86_REG_ESI, mobs[owner]+0x1f0)
        cpu.reg_write(UC_X86_REG_EBX, kind)
        cpu.emu_start(at, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop or cpu.reg_read(UC_X86_REG_ESP) != stack+4+4*len(args):
            raise RuntimeError('original did not return with balanced stack')

    rng = random.Random(0x491080)
    print(f'# ps_game.exe SHA256 {digest}; CMob::Aggro 00491080, GetStatus 0041d760, CDamageMob Add 004b8dd0/Reset 004b8d10/GetMaxAggro 004b9000 all execute; OS lock/allocation/retarget ports only')
    print('# seed amount id hp ordinary stack_mode scene_status fail mutation seeded_damage seeded_aggro final events maximum_aggro_bool_and_28_bytes')
    for case in range(2048):
        active = False; fail = False; events = []; objects = []
        cpu.mem_write(root, bytes(0x2f000)); put(pool, 0, 0, 0)
        for i in range(2):
            put(mobs[i]+0x1f4, heads[i], heads[i]); put(heads[i], heads[i], heads[i])
        seed = case % 5
        identity = rng.choice((0, 1, 77, 0xffffffff))
        initial_damage = rng.choice((1, 99, 0x80000000, 0xffffffff))
        initial_aggro = rng.choice((0, 1, 0x7ffffffe, 0x7fffffff, 0x80000000, 0xffffffff))
        # A second mob always retains its independent payload across this call.
        call(0x4b8dd0, 1, (identity, 7, 9, 55))
        if seed in (1, 4):
            call(0x4b8dd0, 0, (identity, initial_damage, initial_aggro, 77))
        elif seed == 2:
            call(0x4b8dd0, 0, (identity, initial_damage, initial_aggro, 77), kind=2)
            call(0x4b8dd0, 0, (identity, initial_damage, initial_aggro, 77), kind=5)
        elif seed == 3:
            call(0x4b8dd0, 0, (identity ^ 1, initial_damage, initial_aggro, 77))
        if seed == 4:
            call(0x4b8d10, 0)
        amount = (0, 1, 2, 100, 0x7fffffff, 0x80000000, 0xffffffff, 0x40000000)[case % 8]
        hp, ordinary, stack_mode, scene_status = rng.choice((0, 1, 0xffffffff)), rng.randrange(4), rng.randrange(3), rng.randrange(4)
        mutation, fail = rng.randrange(5), bool(rng.randrange(2))
        mob = mobs[0]
        put(mob+0x224, hp); put(mob+0x124, ordinary)
        cpu.mem_write(mob+0x228, struct.pack('<HH', 17, 19))
        put(mob+0x25c, 123); put(mob+0xcb0, 999); put(mob+0x128, 87)
        put(mob+0x44, stack_mode != 0); put(mob+0x40, stack_mode != 2)
        put(mob+0x34, table); put(table, block); put(block, scene); put(scene+0x28, scene_status)
        active = True
        put(stack, stop, identity)
        cpu.reg_write(UC_X86_REG_ESP, stack); cpu.reg_write(UC_X86_REG_EAX, amount); cpu.reg_write(UC_X86_REG_ECX, mob)
        cpu.emu_start(0x491080, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop or cpu.reg_read(UC_X86_REG_ESP) != stack+8:
            raise RuntimeError('Aggro did not return with balanced stack')
        active = False
        if bytes(cpu.mem_read(mob+0x228, 4)) != struct.pack('<HH', 17, 19) or get(mob+0x25c) != 123 or get(mob+0xcb0) != 999 or get(mob+0x128) != 87:
            raise RuntimeError('Aggro changed unrelated resources/counters')
        final = snapshot('F')
        cpu.mem_write(output, b'\xa5'*28)
        put(stack, stop); cpu.reg_write(UC_X86_REG_ESP, stack); cpu.reg_write(UC_X86_REG_ESI, mob+0x1f0); cpu.reg_write(UC_X86_REG_EBX, output)
        cpu.emu_start(0x4b9000, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('GetMaxAggro did not return')
        maximum = bytes([cpu.reg_read(UC_X86_REG_EAX)&255])+bytes(cpu.mem_read(output, 28))
        print(seed, amount, identity, hp, ordinary, stack_mode, scene_status, int(fail), mutation, initial_damage, initial_aggro,
              final, '|'.join(events) or '-', maximum.hex(), sep='\t')


if __name__ == '__main__':
    main()
