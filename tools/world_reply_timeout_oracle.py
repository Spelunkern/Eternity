#!/usr/bin/env python3
"""Execute original game reply timeout checks with captured close calls."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game

cpu,digest=load_game();stack,user,agent,reply,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,user,agent,reply,stop):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values]))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
cpu.mem_write(0x4d43a0,b'\xc2\x08\x00');reasons=[]
def close_hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP);reasons.append((get(sp+4),get(sp+8)))
cpu.hook_add(UC_HOOK_CODE,close_hook,begin=0x4d43a0,end=0x4d43a0)
print(f'# ps_game.exe SHA256 {digest}; SReply::CheckTime 004dd670; accepted parameters 00405a30; close is a port')
for last,connected,now in [(0,1,900001),(0,1,900002),(0,0xfffffff0,20),(1,0,900001),(1,0,900002),(1000,0,999),(0xfffffff0,0,20),(0x100,0,0x80000100),(0x100,0,0x800000ff)]:
    cpu.mem_write(user,bytes(0x100));cpu.mem_write(agent,bytes(0x100));put(user+0xc8,agent);put(user+0x4c,last);put(user+0x40,connected);put(agent+0x30,300000,300000,600000)
    put(stack+0x8000,stop,user,now);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,reply);reasons.clear();cpu.emu_start(0x4dd670,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop or len(reasons)>1:raise RuntimeError('Unexpected original timeout result')
    print(last,connected,now,reasons[0][0] if reasons else 0)
