#!/usr/bin/env python3
"""Execute ps_game's movement check with real Reset and CRT sqrt under Unicorn."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game

def f(x): return struct.unpack('<I', struct.pack('<f', x))[0]
def main():
    cpu, digest = load_game()
    stack, state, pos, stop = 0x20000000, 0x30000000, 0x31000000, 0x10000000
    for address in (stack, state, pos, stop): cpu.mem_map(address, 0x10000)
    cpu.reg_write(UC_X86_REG_FPCW, 0x27f)
    cpu.mem_write(0x50d0ca, b'\xb8' + struct.pack('<I', stop + 0x1000) + b'\xc3')
    cpu.mem_write(stop + 0x100, b'\xc3')
    cpu.mem_write(0x5210e4, struct.pack('<I', stop + 0x100))
    ticks = []
    def hook(machine, address, size, _):
        if address == stop + 0x100:
            machine.reg_write(UC_X86_REG_EAX, ticks.pop(0))
    cpu.hook_add(UC_HOOK_CODE, hook)
    rng = random.Random(0x46a120)
    print(f'# ps_game.exe SHA256 {digest}; stUserMoveChk::Update 0046a120, Reset 0046a0b0, original CRT sqrt; FPCW 027f')
    print('# input state 11 DWORDs, destination 3 DWORDs, speed, clock samples 2; output state 11 DWORDs, result, clock call count; hex')
    for i in range(4096):
        now = [0, 1, 4999, 5000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff, rng.getrandbits(32)][i % 9]
        deadline = [now, (now + 1) & 0xffffffff, (now - 1) & 0xffffffff, rng.getrandbits(32)][(i // 9) % 4]
        back_time = [now, (now - 5000) & 0xffffffff, (now + 1) & 0xffffffff, rng.getrandbits(32)][(i // 36) % 4]
        values = [0.0, 0.125, 1.0, 5.0, 49.999996185302734, 50.0, 50.000003814697266, 100.0, -3.0, 10000.0]
        prev = [rng.choice(values), rng.choice(values), rng.choice(values)]
        dest = [rng.choice(values), rng.choice(values), rng.choice(values)]
        if i % 4 == 0: prev = [0.0, 0.0, 0.0]; dest = [values[(i // 4) % len(values)], 0.0, 0.0]
        back = [rng.choice(values) for _ in range(3)]
        words = [back_time, deadline, *map(f, back), rng.getrandbits(32), *map(f, prev), f(rng.choice(values)), f(rng.choice(values))]
        speed = f(rng.choice(values)); second = (now + (i % 7)) & 0xffffffff
        if i < 3:
            dest = [values[4 + i], 0.0, 0.0]
            words = [now, now, *map(f, back), 0, f(0), f(0), f(0), f(0), f(0)]
            speed = f(0)
        if 3 <= i < 6:
            dest = [[54.999996185302734, 55.0, 55.000003814697266][i-3], 0.0, 0.0]
            words = [(now - 1000) & 0xffffffff, now, *map(f, back), 0, f(0), f(0), f(0), f(0), f(0)]
            speed = f(5)
        if 6 <= i < 14:
            tiny = [1, 2, 0x007fffff, 0x00800000][(i-6) % 4]
            words = [now, now if i < 10 else (now+1)&0xffffffff, *map(f, back), 0, 0, 0, 0, tiny, tiny]
            dest = [struct.unpack('<f', struct.pack('<I', tiny))[0], 0.0, 0.0]
            speed = tiny
        cpu.mem_write(state, struct.pack('<11I', *words));cpu.mem_write(pos, struct.pack('<3I', *map(f, dest)))
        cpu.mem_write(stack + 0x8000, struct.pack('<2I', stop, speed))
        cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000);cpu.reg_write(UC_X86_REG_ECX, state);cpu.reg_write(UC_X86_REG_EAX, pos)
        ticks[:] = [now, second]
        cpu.emu_start(0x46a120, stop, count=100000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop: raise RuntimeError('Movement check did not return')
        output = struct.unpack('<11I', cpu.mem_read(state, 44))
        print(' '.join(f'{x:08x}' for x in [*words, *map(f, dest), speed, now, second, *output, cpu.reg_read(UC_X86_REG_EAX)&255, 2-len(ticks)]))
if __name__ == '__main__': main()
