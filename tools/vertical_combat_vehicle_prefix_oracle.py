#!/usr/bin/env python3
"""Trace UpdateRebirth's original vehicle prefix with transport boundaries."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu,digest=load_game();user,item,info,stack=(0x30000000,0x31000000,0x32000000,0x20000000)
    for base in (user,item,info,stack):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def word(at,v):cpu.mem_write(at,struct.pack('<H',v))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    events=[]
    def hook(machine,address,size,_):
        if address==0x4d4d20:
            sp=machine.reg_read(UC_X86_REG_ESP);ptr=get(sp+4);length=get(sp+8)
            if machine.reg_read(UC_X86_REG_ECX)!=user:raise RuntimeError('Foreign send actor')
            events.append('S'+bytes(machine.mem_read(ptr,length)).hex())
        elif address==0x482590:
            if machine.reg_read(UC_X86_REG_ECX)!=user:raise RuntimeError('Foreign shape actor')
            events.append('V')
    cpu.hook_add(UC_HOOK_CODE,hook)
    for address,cleanup in ((0x4d4d20,8),(0x482590,0)):cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    print(f'# ps_game.exe SHA256 {digest}; CUser::UpdateRebirth 0048d240 vehicle prefix; SendShape 00482590')
    print('# now mode item shape grow count required_grow speed model; final mode timer addition shape; events')
    cases=itertools.product((99,100,0xffffffff),(0,1,2),(0,1),(0,10,11,12,13),(1,2),(1,2),(1,2))
    for case,(now,mode,present,shape,grow,count,required) in enumerate(cases):
        speed=(0,1,255)[case%3];model=(0,1,2,65535)[case%4]
        cpu.mem_write(user,bytes(0x7000));cpu.mem_write(item,bytes(0x100));cpu.mem_write(info,bytes(0x100));cpu.mem_write(stack,bytes(0x10000))
        put(user+0x1460,mode);put(user+0x1464,100);put(user+0x1468,123);put(user+0x146c,321);put(user+0x1f8,item if present else 0)
        cpu.mem_write(user+0x1354,bytes([shape]));cpu.mem_write(user+0x12f,bytes([grow]));put(item+0x30,info);cpu.mem_write(item+0x42,bytes([count]));cpu.mem_write(info+0x30,bytes([required]));cpu.mem_write(info+0x32,bytes([speed]));word(info+0x42,model)
        cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,now);cpu.reg_write(UC_X86_REG_EDX,user);events.clear()
        cpu.emu_start(0x48d240,0x48d38a,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x48d38a:raise RuntimeError('Vehicle prefix did not reach continuation')
        print(now,mode,present,shape,grow,count,required,speed,model,get(user+0x1460),get(user+0x1464),get(user+0x1468),get(user+0x146c),','.join(events) or '-',sep='\t')

if __name__=='__main__':main()
