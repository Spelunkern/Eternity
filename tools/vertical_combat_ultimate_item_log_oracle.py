#!/usr/bin/env python3
"""Record original ItemUseNSend's 040e body and untouched scratch bytes."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESP
from original_oracle import load_game


def main():
    cpu,digest=load_game();user,item,info,stack,stop=(0x30000000,0x31000000,0x32000000,0x20000000,0x10000000)
    for base in (user,item,info,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    captured=[]
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x4d4e50:
            if get(sp+8)!=152:raise RuntimeError('bad log size')
            captured.append(bytes(cpu.mem_read(get(sp+4),152)));machine.emu_stop()
    cpu.hook_add(UC_HOOK_CODE,hook)
    cpu.mem_write(0x45b0d0,b'\xc3')
    print(f'# ps_game.exe SHA256 {digest}; ItemUseNSend 00465120, SetGameLogMain intercepted, log body 00465181')
    print('# scratch name uid id gems made bag slot body')
    for seed,name in itertools.product((0,0x55,0xa5),('', 'A', 'Rebirth_Token', 'X'*31)):
        cpu.mem_write(user,bytes(0x7000));cpu.mem_write(item,bytes(0x100));cpu.mem_write(info,bytes(0x200));cpu.mem_write(stack,bytes([seed])*0x10000)
        put(user+0x1c4+(24+5)*4,item);put(item+0x30,info);put(item+0x38,0x55667788);put(item+0x3c,0x11223344);put(item+0x64,0x98765432);cpu.mem_write(item+0x40,bytes((100,9,2)));cpu.mem_write(item+0x46,bytes((1,2,3,4,5,6)));put(info,0xaca1);cpu.mem_write(info+4,name.encode()+b'\0')
        put(stack+0xf000,stop);put(stack+0xf004,5);put(stack+0xf008,0);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_EBX,1);captured.clear();cpu.emu_start(0x465120,stop,count=2000)
        if len(captured)!=1:raise RuntimeError('original log was not sent')
        print(seed,name or '-',0x1122334455667788,0xaca1,'010203040506',0x98765432,1,5,captured[0][74:].hex(),sep='\t')

if __name__=='__main__':main()
