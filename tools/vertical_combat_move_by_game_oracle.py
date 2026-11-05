#!/usr/bin/env python3
"""Execute CUser::MoveByGame with an intercepted server clock."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu, digest = load_game()
user, stack, stop = 0x30000000, 0x20000000, 0x10000000
for base in (user, stack, stop): cpu.mem_map(base, 0x10000)
def put(address, value): cpu.mem_write(address, struct.pack('<I', value))
put(0x5210e4, stop + 0x100); cpu.mem_write(stop + 0x100, b'\xc3')
state = {'tick': 0}
def hook(machine, address, size, _):
    if address == stop + 0x100: machine.reg_write(UC_X86_REG_EAX, state['tick'])
cpu.hook_add(UC_HOOK_CODE, hook)
print(f'# ps_game.exe SHA256 {digest}; CUser::MoveByGame 0044adb0; clock intercepted')
print('# tick x y z back_time check_time back_x back_y back_z previous_time previous_x previous_y previous_z distance speed')
for tick in (0, 1, 0xfffffff0):
    for position in ((0x3f800000, 0x40000000, 0x40400000), (0x80000000, 0x7fc00001, 0xff800000)):
        cpu.mem_write(user, bytes([0x55])*0x6000)
        cpu.mem_write(user + 0xd0, struct.pack('<3I', *position)); put(stack + 0x8000, stop)
        cpu.reg_write(UC_X86_REG_ESI, user); cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000); state['tick'] = tick
        cpu.emu_start(0x44adb0, stop, count=1000)
        if cpu.reg_read(UC_X86_REG_EIP) != stop: raise RuntimeError('MoveByGame did not return')
        result=struct.unpack('<11I', cpu.mem_read(user + 0x5494, 44))
        print(tick, *position, *result, sep='\t')
