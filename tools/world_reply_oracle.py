#!/usr/bin/env python3
"""Execute the original accepted-game-connection reply timing check."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game

cpu,digest=load_game();stack,user,agent,reply,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,user,agent,reply,stop):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values]))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
# GetTickCount and connection close are the only ports; CheckRecv runs unchanged.
put(0x5210e4,stop+0x100);cpu.mem_write(stop+0x100,b'\xc3');cpu.mem_write(0x4d43a0,b'\xc2\x08\x00')
reasons=[];current_tick=0
def clock_hook(machine,address,size,context):machine.reg_write(UC_X86_REG_EAX,current_tick)
cpu.hook_add(UC_HOOK_CODE,clock_hook,begin=stop+0x100,end=stop+0x100)
def close_hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP);reasons.append((get(sp+4),get(sp+8)))
cpu.hook_add(UC_HOOK_CODE,close_hook,begin=0x4d43a0,end=0x4d43a0)
print(f'# ps_game.exe SHA256 {digest}; SReply::CheckRecv 004dd6c0; accepted game parameters 00405a30; clock and close are ports')
cases=[(0,1),(0,0),(1,1),(1,900001),(1,900002),(1000,999),(0xfffffff0,20),(0x100,0x80000100),(0x100,0x800000ff),(0xffff0000,0x000cbb20)]
for last,now in cases:
    cpu.mem_write(user,bytes(0x100));cpu.mem_write(agent,bytes(0x100));put(user+0xc8,agent);put(user+0x4c,last)
    cpu.mem_write(agent+0x2a,b'\x01');put(agent+0x30,300000,300000,600000)
    current_tick=now;put(stack+0x8000,stop,user)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,reply);reasons.clear();cpu.emu_start(0x4dd6c0,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop or len(reasons)>1:raise RuntimeError('Unexpected original reply result')
    print(last,now,get(user+0x4c),reasons[0][0] if reasons else 0)
