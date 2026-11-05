#!/usr/bin/env python3
"""Execute original accepted-login reply checks at boundary and wrap ticks."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_original

cpu,digest=load_original('ps_login');stack,user,agent,reply,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,user,agent,reply,stop):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values]))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
put(0x4410cc,stop+0x100);cpu.mem_write(stop+0x100,b'\xc3');cpu.mem_write(0x411160,b'\xc2\x08\x00')
reasons=[];current=0
def clock(machine,address,size,data):machine.reg_write(UC_X86_REG_EAX,current)
def close(machine,address,size,data):
    sp=machine.reg_read(UC_X86_REG_ESP);reasons.append((get(sp+4),get(sp+8)))
cpu.hook_add(UC_HOOK_CODE,clock,begin=stop+0x100,end=stop+0x100)
cpu.hook_add(UC_HOOK_CODE,close,begin=0x411160,end=0x411160)
def call(address,*args):
    put(stack+0x8000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,reply)
    cpu.emu_start(address,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop or len(reasons)>1:raise RuntimeError('Original check did not return')
print(f'# ps_login.exe SHA256 {digest}; SReply::CheckRecv 00417d90, CheckTime 00417d40; clock and close are ports')
print('kind\tlast\tnow\tstored\treason')
for last,now in [(0,1),(1,110000),(1,110001),(1,720001),(1,720002),(1000,999),(0xfffffff0,0x1ada0),(0x100,0x80000100)]:
    cpu.mem_write(user,bytes(0x100));cpu.mem_write(agent,bytes(0x100));put(user+0xc8,agent);put(user+0x4c,last)
    cpu.mem_write(agent+0x2a,b'\x01');put(agent+0x30,120000,10000,600000)
    current=now;reasons.clear();call(0x417d90,user)
    print('recv',last,now,get(user+0x4c),reasons[0][0] if reasons else 0,sep='\t')
for last,now in [(0,720000),(0,720001),(1,720001),(1,720002),(1000,999),(0xfffffff0,20)]:
    cpu.mem_write(user,bytes(0x100));cpu.mem_write(agent,bytes(0x100));put(user+0xc8,agent);put(user+0x4c,last);put(agent+0x30,120000,10000,600000)
    reasons.clear();call(0x417d40,user,now)
    print('scan',last,now,get(user+0x4c),reasons[0][0] if reasons else 0,sep='\t')
