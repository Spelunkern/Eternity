#!/usr/bin/env python3
"""Execute original non-damage RemApplySkill and complete SetSkillAbility dispatch."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x10000)
    mob, info, skill, mob_info, stop = base, base+0x2000, base+0x3000, base+0x4000, base+0xf000
    fields = [0x230, 0x22c, 0x234, 0x240, 0x23c, *range(0x188, 0x1b8, 4),
              0x134, 0x150, 0x16c, 0x248, 0x244, 0x24c, 0x238, 0x2b8, 0x224, 0x124, 0x12c]
    state = {}

    def put(at, value):
        cpu.mem_write(at, struct.pack('<I', value & 0xffffffff))

    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]

    def snapshot():
        words = [get(mob+offset) for offset in fields]
        for slot in range(3):
            words += [cpu.mem_read(info+0x7c+4*slot, 1)[0],
                      struct.unpack('<H', cpu.mem_read(info+0x7e+4*slot, 2))[0]]
        return struct.pack('<34I', *words).hex()

    for address in (0x4902f0, 0x4906d0, 0x505316):
        cpu.mem_write(address, b'\xc3')

    def hook(machine, address, size, data):
        if address in (0x4902f0, 0x4906d0):
            register = UC_X86_REG_ECX if address == 0x4902f0 else UC_X86_REG_EAX
            assert machine.reg_read(register) == mob
            state['events'].append(('A' if address == 0x4902f0 else 'S')+snapshot())
            if state['mutate']:
                put(mob+0x230, get(mob+0x230) ^ 0x11111111)
                cpu.mem_write(info+0x80, b'\x08')
                cpu.mem_write(info+0x82, b'\xff\xff')
    cpu.hook_add(UC_HOOK_CODE, hook)

    print(f'# ps_game.exe SHA256 {digest}; RemApplySkill 004a0da0 non-damage branches and real SetSkillAbility 004a1420. SetAttack 004902f0 / SendSpeed 004906d0 are observed required ports. No original damage branches are represented as executed effects.')
    print('# kind type_or_detail delta base_attribute mutate ability_type/value x3 initial28wordhex final34wordhex callback_events(A attack/S speed + live34wordhex). Mutating ports XOR wisdom and replace ability slot 2 with type 8/value 65535 after observing state.')
    boundaries = [0, 1, 0xffffffff, 0x7fffffff, 0x80000000, 65535, 0xffff0000]
    patterns = [(7, 8, 9), (16, 17, 32), (20, 21, 22), (23, 24, 25),
                (26, 27, 28), (29, 30, 31), (32, 33, 34), (0, 255, 10)]
    details = [0x3e, 0x43, 0x40, 0x41, 0x42, 0x44, 0x48, 0x45,
               0x46, 0x47, 0, 0x3c, 0x49, 0xffff, 0x100, 7]
    cases = [('A', i % 256, boundaries[i % 7], i) for i in range(768)]
    cases += [('A', 256, 1, 768), ('A', 0xffffffff, 0xffffffff, 769)]
    cases += [('R', details[i % 16], 0, 770+i) for i in range(512)]
    for kind, selector, delta, case in cases:
        cpu.mem_write(base, bytes(0xe000))
        values = [boundaries[(case+i) % 7] for i in range(28)]
        for offset, value in zip(fields, values):
            put(mob+offset, value)
        put(mob+0xa8, mob_info)
        base_attribute = case % 256
        cpu.mem_write(mob_info+0x40, bytes([base_attribute]))
        put(skill+0x30, info)
        cpu.mem_write(info+0x32, struct.pack('<H', selector & 0xffff))
        ability_types = patterns[(case//16) % 8]
        abilities = []
        for slot, ability_type in enumerate(ability_types):
            value = [0, 1, 65535][(case+slot) % 3]
            abilities += [ability_type, value]
            cpu.mem_write(info+0x7c+slot*4, bytes([ability_type]))
            cpu.mem_write(info+0x7e+slot*4, struct.pack('<H', value))
        mutate = (case//16) % 2
        state.update(events=[], mutate=mutate)
        put(base+0xe000, stop)
        cpu.reg_write(UC_X86_REG_ESP, base+0xe000)
        cpu.reg_write(UC_X86_REG_ECX, mob)
        cpu.reg_write(UC_X86_REG_EBX, skill)
        cpu.reg_write(UC_X86_REG_EAX, delta)
        cpu.reg_write(UC_X86_REG_EDX, selector)
        cpu.emu_start(0x4a1420 if kind == 'A' else 0x4a0da0, stop, count=10000)
        assert cpu.reg_read(UC_X86_REG_EIP) == stop
        initial = struct.pack('<28I', *values).hex()
        print(kind, selector, delta, base_attribute, mutate, *abilities, initial, snapshot(),
              ','.join(state['events']) or '-', sep='\t')


if __name__ == '__main__':
    main()
