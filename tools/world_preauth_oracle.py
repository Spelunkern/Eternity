#!/usr/bin/env python3
"""Execute original pre-authentication packet-count and close-reason branches."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game

cpu,digest=load_game()
stack,user,packet,stop=0x20000000,0x30000000,0x31000000,0x10000000
for address in (stack,user,packet,stop): cpu.mem_map(address,0x10000)
def put(address,value): cpu.mem_write(address,struct.pack('<I',value))
def get(address): return struct.unpack('<I',cpu.mem_read(address,4))[0]
# Logging, close and compiler cookie checks are external ports. PacketProcessing
# and its undefined counter are the original executable instructions.
for address,code in ((0x4d9510,b'\xc3'),(0x4d43a0,b'\xc2\x08\x00'),(0x505316,b'\xc3')):
    cpu.mem_write(address,code)
closed=[]
def close_hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    closed.append((get(sp+4),get(sp+8)))
cpu.hook_add(UC_HOOK_CODE,close_hook,begin=0x4d43a0,end=0x4d43a0)
print(f'# ps_game.exe SHA256 {digest}; CUser::PacketProcessing 00466da0; logging, close and cookie are ports')
for opcode in (0x0000,0xa301):
    cpu.mem_write(user,bytes(0x6000));cpu.mem_write(packet,struct.pack('<HI',opcode,0)+bytes(16))
    closed.clear()
    for count in range(11):
        put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
        cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_EDX,packet)
        cpu.emu_start(0x466da0,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('Original did not return')
        expected=min(count+1,10)
        if get(user+0x5810)!=expected: raise RuntimeError('Unexpected undefined count')
        if closed != ([(0x65,0)] if count==10 else []): raise RuntimeError('Unexpected close reason')
        print(f'{opcode:04x} {count+1} {expected} {closed[-1][0] if closed else 0}')
