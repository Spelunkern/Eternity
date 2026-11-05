#!/usr/bin/env python3
"""Execute the original NProtect receive absent/invalid-location branches."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP
from original_oracle import load_game

cpu, digest = load_game()
stack, user, context, stop = 0x20000000, 0x30000000, 0x31000000, 0x10000000
for address in (stack, user, context, stop):
    cpu.mem_map(address, 0x10000)
def put(address, *values):
    cpu.mem_write(address, struct.pack('<' + 'I' * len(values), *values))
def get(address):
    return struct.unpack('<I', cpu.mem_read(address, 4))[0]
# Only socket close is ported; the tested branches call no NProtect backend.
cpu.mem_write(0x4d43a0, b'\xc2\x08\x00')
closed = []
def hook(machine, address, size, data):
    sp = machine.reg_read(UC_X86_REG_ESP)
    closed.append((get(sp + 4), get(sp + 8)))
cpu.hook_add(UC_HOOK_CODE, hook, begin=0x4d43a0, end=0x4d43a0)
print(f'# ps_game.exe SHA256 {digest}; CUser::NProtectCSARecv 0044b7e0; socket close is ported')
print('object_present\tlocation\treturn\tclose_reason')
for present in (False, True):
    cpu.mem_write(user, bytes(0x6000)); put(user + 0x5520, context if present else 0)
    put(user + 0x5758, 0); put(stack + 0x8000, stop, 2, 3, 4)
    cpu.reg_write(UC_X86_REG_ESP, stack + 0x8000); cpu.reg_write(UC_X86_REG_ESI, user)
    cpu.reg_write(UC_X86_REG_ECX, 1); cpu.reg_write(UC_X86_REG_EAX, 0x12345678)
    closed.clear(); cpu.emu_start(0x44b7e0, stop, count=1000)
    reason = closed[0][0] if closed else 0
    if cpu.reg_read(UC_X86_REG_EIP) != stop or cpu.reg_read(UC_X86_REG_EAX) & 0xff or reason != (111 if present else 0):
        raise RuntimeError('Unexpected original NProtect absent/location-zero behavior')
    print(f'{int(present)}\t0\t0\t{reason}')
