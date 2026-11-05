#!/usr/bin/env python3
"""Trace AttackAI HP-gated slot selection through original x87 operations."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,definition,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,mob,user,definition,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):state['slot']=machine.reg_read(UC_X86_REG_EBX);machine.reg_write(UC_X86_REG_EIP,0x4a1fdb)
cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4a1bde,end=0x4a1bde);cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4a1d22,end=0x4a1d22)
print('# ps_game.exe SHA256 '+digest+'; CMob::AttackAI 004a1ab0 HP gates; retarget and type 6 intercepted')
print('# maxhp hp type0 type1 type2 | slot next_attack')
cases=[(100,h,0,0,t) for t in (3,4,5) for h in (0,4,5,6,19,20,21,39,40,41,100)]+[(100,10,1,0,3),(100,50,0,1,3),(100,50,0,0,6),(0xffffffff,0x80000000,0,0,3),(0xffffffff,0xffffffff,0,0,4),(16777217,3355443,0,0,5)]
for maxhp,hp,t0,t1,t2 in cases:
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(user,bytes(0x6000));cpu.mem_write(definition,bytes(0x1000));put(mob+0xa8,definition);put(definition+0x78,2);put(definition+0x30,maxhp);put(mob+0x224,hp);cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(user+0x5774,b'\x01\x01');put(mob+0x240,3);put(mob+0xcb4,12345)
    for i,v in enumerate((t0,t1,t2)):cpu.mem_write(definition+0x82+i*20,bytes([v]))
    state['slot']=0xffffffff;put(stack+0x8000,stop,100,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob);cpu.emu_start(0x4a1ab0,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('AttackAI did not return')
    print(maxhp,hp,t0,t1,t2,state['slot'],get(mob+0xcb4),sep='\t')
