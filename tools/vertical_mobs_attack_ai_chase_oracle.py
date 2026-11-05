#!/usr/bin/env python3
"""Trace AttackAI chase routing; real dispatch with chase/attack/skill intercepted."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,definition,skill,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for address in (stack,mob,user,definition,skill,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
state={}
def hook(machine,address,size,context):
    if address==0x490ab0:state['trace']+='R'
    elif address==0x4a6de0:
        state['trace']+='C';machine.reg_write(UC_X86_REG_EAX,state['reached'])
        if state['chase_clear']:put(mob+0x128,0)
    elif address==0x4a2000:
        state['trace']+='A';state['during']=get(mob+0x130)
        if state['mutation']:put(mob+0x130,2);put(mob+0x240,6);put(definition+0xac,777)
    elif address==0x4a48e0:
        state['trace']+='U';state['during']=get(mob+0x130)
        if state['mutation']:cpu.mem_write(skill+0x4a,struct.pack('<H',7));put(mob+0x130,77)
for address,code in ((0x490ab0,b'\xc3'),(0x4a6de0,b'\xc2\x10\x00'),(0x4a2000,b'\xc2\x04\x00'),(0x4a48e0,b'\xc2\x04\x00')):
    cpu.mem_write(address,code);cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
print('# ps_game.exe SHA256 '+digest+'; CMob::AttackAI 004a1ab0 chase routing; retarget/Chase/Attack/UseSkill intercepted')
print('# now slot skill reached target_kind chase_clear mutation | trace during deadline current target_kind')
cases=((100,0,0,0,1,0,0),(100,1,0,1,1,0,0),(100,2,0,1,1,0,1),(100,1,1,0,1,0,0),(100,1,1,0,1,1,0),(100,1,1,0,0,0,0),(100,0,1,1,1,0,0),(100,2,1,1,1,0,1),(0xfffffffe,1,1,0,0,0,0))
for now,slot,is_skill,reached,kind,clear,mutation in cases:
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(user,bytes(0x6000));cpu.mem_write(definition,bytes(0x1000));cpu.mem_write(skill,bytes(0x1000));put(mob+0xa8,definition);put(definition+0x78,2);cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(user+0x5774,b'\x01\x01');put(mob+0x124,1,kind,42);put(mob+0x240,3);put(mob+0x130,99);put(mob+0x1b8,10,11,12);cpu.mem_write(definition+0x68,b'\x02');cpu.mem_write(definition+0x80+slot*20,struct.pack('<H',is_skill));cpu.mem_write(definition+0x82+slot*20,b'\x01');cpu.mem_write(definition+0x88+slot*20,b'\x05');put(definition+0x84+slot*20,2000)
    if is_skill:put(definition+0x90+slot*20,skill);cpu.mem_write(skill+0x31,b'\x01');cpu.mem_write(skill+0x4a,struct.pack('<H',5));cpu.mem_write(skill+0x4c,b'\x06')
    state.update(trace='',during=999,reached=reached,chase_clear=clear,mutation=mutation);put(stack+0x8000,stop,now,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob);cpu.emu_start(0x4a1ab0,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('AttackAI did not return')
    print(now,slot,is_skill,reached,kind,clear,mutation,state['trace'],state['during'],get(mob+0x1b8+slot*4),get(mob+0x130),get(mob+0x128),sep='\t')
