#!/usr/bin/env python3
"""Trace Chase deadline, in-range reset, and immobilization using real sqrt."""
import math,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,target,definition,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,mob,target,definition,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
state={}
def hook(machine,address,size,context):
    if address==0x48ec70:state['reset']+=1
    elif address==0x4a6ea1:state['movement']+=1;machine.reg_write(UC_X86_REG_EIP,0x4a7659)
cpu.mem_write(0x48ec70,b'\xc2\x04\x00');cpu.hook_add(UC_HOOK_CODE,hook,begin=0x48ec70,end=0x48ec70);cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4a6ea1,end=0x4a6ea1);cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::Chase 004a6de0 prefix; real sqrt, ResetPath and movement intercepted')
print('# now deadline x z size range stop sleep stun | result reset movement found (float x/z as DWORD bits)')
cases=((49,50,0,0,0,0,0,0,0),(50,50,0,0,0,0,1,1,1),(50,50,1.49999,0,0,0,0,0,0),(50,50,1.5,0,0,0,0,0,0),(50,50,2.5,0,1,0,0,0,0),(50,50,3,4,0,4,0,0,0),(50,50,20,0,0,0,1,0,0),(50,50,20,0,0,0,0,1,0),(50,50,20,0,0,0,0,0,1),(50,50,20,0,0,0,0,0,0),(0,0xffffffff,0,0,0,0,0,0,0),(50,50,math.nan,0,0,0,0,0,0))
for now,deadline,x,z,size,range_,stopped,sleep,stun in cases:
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(target,bytes(0x1000));cpu.mem_write(definition,bytes(0x1000));put(mob+0xa8,definition);put(mob+0xcb8,deadline);put(mob+0x244,stopped);put(mob+0x24c,sleep);put(mob+0x248,stun);cpu.mem_write(mob+0xa0,b'\x01');cpu.mem_write(definition+0x3f,bytes([size]));put(target,bits(x),0,bits(z));state.update(reset=0,movement=0)
    put(stack+0x8000,stop,mob,now,target,range_);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a6de0,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Chase prefix did not return')
    print(now,deadline,bits(x),bits(z),size,range_,stopped,sleep,stun,cpu.reg_read(UC_X86_REG_EAX)&255,state['reset'],state['movement'],cpu.mem_read(mob+0xa0,1)[0],sep='\t')
