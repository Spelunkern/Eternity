#!/usr/bin/env python3
"""Trace ItemUseNSend for the no-self-notify ultimate rebirth call."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EIP, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu,digest=load_game();user,zone,item,info,stack,stop=(0x30000000,0x31000000,0x32000000,0x33000000,0x20000000,0x10000000)
    for base in (user,zone,item,info,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    slot_addr=user+0x1c4+(24+5)*4
    events=[]
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x465181:
            events.append('L');machine.reg_write(UC_X86_REG_EIP,0x465223)
        elif address==0x4d4f10:
            if get(sp+8)!=9:raise RuntimeError('bad DB length')
            events.append('D'+bytes(cpu.mem_read(get(sp+4),9)).hex()+':'+str(int(get(slot_addr)!=0))+':'+str(cpu.mem_read(item+0x42,1)[0]))
        elif address==0x421220:
            if get(sp+12)!=11:raise RuntimeError('bad view length')
            events.append('V'+bytes(cpu.mem_read(get(sp+8),11)).hex())
        elif address==0x40a590:events.append('F')
    cpu.hook_add(UC_HOOK_CODE,hook)
    for address,cleanup in ((0x4d4f10,8),(0x421220,32),(0x40a590,0),(0x505316,0)):cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    print(f'# ps_game.exe SHA256 {digest}; CUser::ItemUseNSend 00465120 with notify=false; log body and transports intercepted')
    print('# count type definition zone events')
    for count,item_type,definition,has_zone in itertools.product((0,1,2,255),(5,100),(1,0xaca1),(0,1)):
        cpu.mem_write(user,bytes(0x7000));cpu.mem_write(item,bytes(0x100));cpu.mem_write(info,bytes(0x200));put(slot_addr,item);put(item+0x30,info);put(info,definition);cpu.mem_write(item+0x40,bytes((item_type,9,count)));put(user+0xe0,zone if has_zone else 0);put(user+0xdc,60);put(user+0x578c,123)
        put(stack+0xf000,stop);put(stack+0xf004,5);put(stack+0xf008,0);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_EBX,1);events.clear()
        cpu.emu_start(0x465120,stop,count=2000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('original did not return')
        print(count,item_type,definition,has_zone,','.join(events),sep='\t')

if __name__=='__main__':main()
