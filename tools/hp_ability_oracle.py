#!/usr/bin/env python3
"""Run original low-HP passive activation, ability mutations and send operations."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, user, info, node, party, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x10000000
    for address in (0, stack, user, info, node, party, stop):
        cpu.mem_map(address, 0x10000)
    cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    cpu.mem_write(stop + 0x100, b'\xc2\x04\x00')
    cpu.mem_write(stop + 0x200, b'\xc3')
    for iat in (0x5210cc, 0x5210d0):
        cpu.mem_write(iat, struct.pack('<I', stop + 0x100))
    cpu.mem_write(0x5210e4, struct.pack('<I', stop + 0x200))
    for address, cleanup in ((0x402770, 0), (0x40a780, 0), (0x454280, 0), (0x481990, 0), (0x4827b0, 0), (0x4d4f10, 8), (0x4d4d20, 8), (0x445680, 4)):
        cpu.mem_write(address, b'\xc2' + struct.pack('<H', cleanup))
    trace, state = [], {}

    def word(address):
        return struct.unpack('<I', cpu.mem_read(address, 4))[0]

    def hook(machine, address, size, context):
        sp = machine.reg_read(UC_X86_REG_ESP)
        if address == stop + 0x200:
            trace.append('T')
            machine.reg_write(UC_X86_REG_EAX, state['tick'])
        elif address == 0x402770:
            available = state['available'] and machine.reg_read(UC_X86_REG_EAX) == 200 and 1 <= machine.reg_read(UC_X86_REG_ESI) <= 3
            machine.reg_write(UC_X86_REG_EAX, info if available else 0)
        elif address == 0x40a780:
            trace.append('A')
            machine.reg_write(UC_X86_REG_EAX, node if state['allocate'] else 0)
        elif address in (0x4d4f10, 0x4d4d20):
            ptr, length = struct.unpack('<II', cpu.mem_read(sp + 4, 8))
            data = bytes(cpu.mem_read(ptr, length))
            if address == 0x4d4f10:
                if length != 13 or data[:2] != b'\x01\x09' or struct.unpack_from('<I', data, 2)[0] != 9001:
                    raise RuntimeError('Unexpected effect persistence request')
                identity, level, duration = struct.unpack_from('<HBI', data, 6)
                trace.append(f'R{identity},{level},{duration}')
            else:
                trace.append('S' + data.hex())
        elif address == 0x445680:
            if word(sp + 4) != user:
                raise RuntimeError('Unexpected group exclusion')
            trace.append('P' + bytes(cpu.mem_read(machine.reg_read(UC_X86_REG_ECX), machine.reg_read(UC_X86_REG_EAX))).hex())
        elif address in (0x481990, 0x4827b0):
            trace.append(('M' if address == 0x481990 else 'V') + str(cpu.mem_read(user + 0x1368, 1)[0]) + ',' + str(word(user + 0x1370)))

    for address in (stop + 0x200, 0x402770, 0x40a780, 0x4d4f10, 0x4d4d20, 0x445680, 0x481990, 0x4827b0):
        cpu.hook_add(UC_HOOK_CODE, hook, begin=address, end=address)
    rng = random.Random(0x487020)
    cases = []
    for maximum in (0, 1000, 0x7fffffff, 0x80000000, 0xffffffff):
        for limit in (0, 1, 25, 100, 255):
            product = struct.unpack('<i', struct.pack('<I', (maximum * limit) & 0xffffffff))[0]
            rounded = struct.unpack('<f', struct.pack('<f', product))[0]
            threshold = int(rounded / 100)
            for delta in (-1, 0, 1):
                for tick in (179999, 180000, 180001, 0xfffffff0):
                    cases.append(((threshold + delta) & 0xffffffff, maximum, limit, 0, tick, 200, 1, 0, 1, 1, rng.randrange(-1, 2)))
    for _ in range(1000):
        cases.append((rng.getrandbits(32), rng.getrandbits(32), rng.randrange(256), rng.getrandbits(32), rng.getrandbits(32),
                      rng.choice((0, 200, 200, 999)), rng.randrange(5), rng.randrange(2), rng.randrange(2), rng.randrange(2), rng.randrange(-1, 2)))
    for previous in (0xfffffff0, 0xfffd40e0, 1):
        deadline = (previous + 180000) & 0xffffffff
        for delta in (-1, 0, 1):
            for allocate in (0, 1):
                for group in (-1, 0, 1):
                    cases.append((250, 1000, 25, previous, (deadline + delta) & 0xffffffff, 200, 1, 0, 1, allocate, group))
    print(f'# ps_game.exe SHA256 {digest}; ChkHPAbilityApply 00487020, SendSkillApply 00482860, SetSkillAbility 00485e00, AddBack 00486200')
    print('# HP maxHP limit previous tick skill level active available allocate group values[3] keep -> active previous strength maxHP speed count end event_count events')
    for hp, maximum, limit, previous, tick, skill, level, active, available, allocate, group in cases:
        state.update(tick=tick, available=available, allocate=allocate)
        values = [rng.randrange(1, 65536) for _ in range(3)]
        keep = rng.getrandbits(32)
        cpu.mem_write(user, bytes(0x6000))
        for offset, value in ((0x17c, maximum), (0x1238, hp), (0x1370, previous), (0x1220, 100), (0x12ec, 2), (0xdc, 9001), (0x578c, 9001), (0x173c, party if group >= 0 else 0)):
            cpu.mem_write(user + offset, struct.pack('<I', value))
        cpu.mem_write(user + 0x1368, struct.pack('<BxHB', active, skill, level))
        cpu.mem_write(party + 0x114, bytes([max(0, group)]))
        root = user + 0x5000
        cpu.mem_write(root, struct.pack('<II', root, root))
        cpu.mem_write(user + 0xa94, struct.pack('<II', root, root))
        definition = bytearray(160)
        struct.pack_into('<H', definition, 0x32, 6)
        struct.pack_into('<I', definition, 0x5c, keep)
        definition[0x96] = limit
        for index, (kind, value) in enumerate(zip((4, 1, 16), values)):
            struct.pack_into('<BxH', definition, 0x7c + index * 4, kind, value)
        cpu.mem_write(info, bytes(definition))
        cpu.mem_write(node, bytes(64));cpu.mem_write(node + 8, struct.pack('<I', 1000))
        trace.clear()
        cpu.mem_write(stack + 0x8000, struct.pack('<II', stop, user))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000)
        cpu.emu_start(0x487020, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop:
            raise RuntimeError('Original activation did not return')
        count = word(user + 0xab4)
        print(hp, maximum, limit, previous, tick, skill, level, active, available, allocate, group, *values, keep,
              cpu.mem_read(user + 0x1368, 1)[0], word(user + 0x1370), struct.unpack('<i', cpu.mem_read(user + 0x1220, 4))[0],
              word(user + 0x17c), struct.unpack('<i', cpu.mem_read(user + 0x12ec, 4))[0], count, word(node + 0x20) if count else 0,
              len(trace), *trace, sep='\t')


if __name__ == '__main__':
    main()
