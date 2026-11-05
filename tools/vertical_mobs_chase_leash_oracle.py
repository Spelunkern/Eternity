#!/usr/bin/env python3
"""Trace Chase's home/target leash branches with original distance code."""
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
    if address==0x48ec70:state['trace']+='P'
    elif address==0x492310:state['trace']+='F';put(mob+0x124,100,0,0)
    elif address==0x4a6f65:
        state['trace']+='M';machine.reg_write(UC_X86_REG_EAX,0);machine.reg_write(UC_X86_REG_ESP,stack+0x8014);machine.reg_write(UC_X86_REG_EIP,stop)
for address,code in ((0x48ec70,b'\xc2\x04\x00'),(0x492310,b'\xc3')):
    cpu.mem_write(address,code);cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4a6f65,end=0x4a6f65);cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
print('# ps_game.exe SHA256 '+digest+'; CMob::Chase 004a6de0 leash; real distance/GetStatus, reset and movement intercepted')
print('# mob_x target_x home_x fixed pais status | trace result status target_kind target_id found (float x values as DWORD bits)')
cases=((0,46,0,0,0,1),(0,46,0,0,0,2),(0,45,0,0,0,1),(46,0,0,0,0,1),(46,0,0,0,0,2),(46,0,0,0,1,1),(46,0,0,0,1,2),(0,60,0,1,0,1),(0,math.nan,0,0,0,1))
for mx,tx,hx,fixed,pais,status in cases:
    cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(target,bytes(0x1000));cpu.mem_write(definition,bytes(0x1000));put(mob+0xa8,definition);put(mob+0x50,bits(mx));put(mob+0xc88,bits(hx));put(target,bits(tx));put(mob+0x124,status,7,42);put(mob+0x2b8,fixed);cpu.mem_write(mob+0x250,bytes([pais]));cpu.mem_write(mob+0xa0,b'\x01');state['trace']=''
    put(stack+0x8000,stop,mob,50,target,0);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x4a6de0,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Chase leash did not return')
    print(bits(mx),bits(tx),bits(hx),fixed,pais,status,state['trace'] or '-',cpu.reg_read(UC_X86_REG_EAX)&255,get(mob+0x124),get(mob+0x128),get(mob+0x12c),cpu.mem_read(mob+0xa0,1)[0],sep='\t')
