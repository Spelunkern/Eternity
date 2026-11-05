#!/usr/bin/env python3
"""Trace stationary AttackAI normal attacks with real range and intercepted attack."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_FPCW
from original_oracle import load_game
cpu,digest=load_game();stack,mob,user,definition,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,mob,user,definition,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
state={}
def hook(machine,address,size,context):
    if address==0x490ab0:state['trace']+='R'
    elif address==0x4a2000:
        state['trace']+='A';state['during']=get(mob+0x130)
        if state['mutation']:
            put(mob+0x130,state['mutation']);put(mob+0x240,6);put(definition+0x84+state['mutation']*20,777)
for address,code in ((0x490ab0,b'\xc3'),(0x4a2000,b'\xc2\x04\x00')):
    cpu.mem_write(address,code);cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::AttackAI 004a1ab0 stationary normal; real range, aggro/Attack intercepted')
print('# now slot ai speed obelisk range distance status mutation | trace during deadline current status target_kind')
cases=((100,0,4,0,0,5,1,1,0),(100,1,0,0,0,5,1,1,0),(100,2,0,2,1,5,1,1,0),(100,1,4,0,0,5,5.3,1,0),(100,2,4,0,0,5,20,2,0),(100,1,4,0,0,5,1,1,2),(0xfffffffe,2,4,0,0,5,1,1,0))
for now,slot,ai,speed,obelisk,range_,distance,status,mutation in cases:
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(user,bytes(0x6000));cpu.mem_write(definition,bytes(0x1000));put(mob+0xa8,definition);put(definition+0x78,2);cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(user+0x5774,b'\x01\x01');put(user+0xd0,bits(distance));put(mob+0x124,status,7,42);put(mob+0x240,3);put(mob+0x130,99);put(mob+0x1b8,10,11,12);cpu.mem_write(definition+0x26,bytes([ai]));cpu.mem_write(definition+0x68,bytes([speed]));cpu.mem_write(definition+0x7c,bytes([obelisk]));cpu.mem_write(definition+0x82+slot*20,b'\x01');cpu.mem_write(definition+0x88+slot*20,bytes([range_]));put(definition+0x84+slot*20,2000);state.update(trace='',during=999,mutation=mutation)
    put(stack+0x8000,stop,now,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,mob);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a1ab0,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('AttackAI did not return')
    print(now,slot,ai,speed,obelisk,range_,bits(distance),status,mutation,state['trace'],state['during'],get(mob+0x1b8+slot*4),get(mob+0x130),get(mob+0x124),get(mob+0x128),sep='\t')
