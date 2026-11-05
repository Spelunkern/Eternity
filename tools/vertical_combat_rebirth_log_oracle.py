#!/usr/bin/env python3
"""Trace the original UpdateRebirth timeout log scratch writes and send."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_EDI, UC_X86_REG_EIP, UC_X86_REG_ESI, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu,digest=load_game();user,stack,stop=0x30000000,0x20000000,0x10000000
    for base in (user,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    logs=[]
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x45b0d0:
            ptr=machine.reg_read(UC_X86_REG_ESI)
            machine.mem_write(ptr+2,bytes((i*7+3)&255 for i in range(72)))
        elif address==0x4d4e50:
            ptr,n=get(sp+4),get(sp+8)
            logs.append(bytes(machine.mem_read(ptr,n)))
    cpu.hook_add(UC_HOOK_CODE,hook)
    for address,cleanup in ((0x45b0d0,0),(0x4d4e50,8),(0x4599e0,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    put(0x546f4c,stop)
    print(f'# ps_game.exe SHA256 {digest}; CUser::UpdateRebirth 0048d240 timeout log at 0048d39a; SetGameLogMain 0045b0d0 boundary intercepted')
    print('# scratch byte; emitted 101-byte packet (header bytes 2..73 set by a recording stub)')
    for seed in (0,0x5a,0xff):
        cpu.mem_write(user,bytes(0x7000));cpu.mem_write(stack,bytes([seed])*0x10000)
        put(user+0x142c,1);put(user+0x1454,0);put(user+0x1458,0)
        cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EDI,user);cpu.reg_write(UC_X86_REG_ESI,100);cpu.reg_write(UC_X86_REG_EBX,1);cpu.reg_write(UC_X86_REG_EBP,0)
        logs.clear();cpu.emu_start(0x48d38a,0x48d3f9,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x48d3f9 or len(logs)!=1 or len(logs[0])!=101:raise RuntimeError('Original timeout log not emitted')
        print(seed,logs[0].hex(),sep='\t')

if __name__=='__main__':main()
