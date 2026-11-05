#!/usr/bin/env python3
"""Execute ExchangeItemAdd's successful split through destination insertion."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,source,new,definition,offer,recipient,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x10000000
    for address,size in ((stack,0x10000),(source,0x10000),(new,0x10000),(definition,0x10000),(offer,0x10000),(recipient,0x10000),(stop,0x1000)):cpu.mem_map(address,size)
    cpu.mem_write(0x40a510,b'\xb8'+struct.pack('<I',new)+b'\xc3')
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc2\x04\x00');cpu.mem_write(stop+0x300,b'\xb8\x64\x00\x00\x00\xc3')
    for iat in (0x5210cc,0x5210d0):cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210dc,struct.pack('<I',stop+0x200));cpu.mem_write(0x5210e4,struct.pack('<I',stop+0x300));cpu.mem_write(0x542ca0,b'\x07')
    from unicorn import UC_HOOK_CODE
    def localtime(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP);ptr=struct.unpack('<I',machine.mem_read(sp+4,4))[0];machine.mem_write(ptr,struct.pack('<8H',2026,11,0,2,13,47,59,0))
    cpu.hook_add(UC_HOOK_CODE,localtime,begin=stop+0x200,end=stop+0x200)
    rng=random.Random(0x46fb21)
    print(f'# ps_game.exe SHA256 {digest}; CUser::ExchangeItemAdd 0046fb21..0046fceb successful split')
    print('# count offered type typeid server wisdom craft_tail bag slot uid made final_count payload make_type')
    for case in range(128):
        for address in (stack,source,new,definition,offer,recipient):cpu.mem_write(address,bytes(0x10000))
        cpu.mem_write(0x547ce4,bytes(16));count=rng.randrange(2,256);offered=rng.randrange(1,count);kind=rng.randrange(256);type_id=rng.randrange(256);server=rng.randrange(2);wisdom=rng.choice([0,1,50,99,100,101]);tail=bytes(rng.randrange(256) for _ in range(20));bag=rng.randrange(1,10);slot=rng.randrange(24)
        cpu.mem_write(source+0x30,struct.pack('<I',definition));cpu.mem_write(source+0x40,bytes((kind,type_id,count)));cpu.mem_write(definition+0x3e,struct.pack('<H',wisdom));cpu.mem_write(definition+0x4a,bytes((server,)));cpu.mem_write(offer,bytes((1,2,offered)));cpu.mem_write(new+0x4c,b'\0'+tail)
        sp=stack+0x8000;cpu.mem_write(sp+0x14,struct.pack('<I',offer));cpu.mem_write(sp+0x24,struct.pack('<I',bag));cpu.mem_write(sp+0x28,struct.pack('<I',recipient));cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,source);cpu.reg_write(UC_X86_REG_EBX,slot);cpu.reg_write(UC_X86_REG_EDI,offer);cpu.emu_start(0x46fb21,0x46fceb,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x46fceb or struct.unpack('<I',cpu.mem_read(recipient+0x1c4+(bag*24+slot)*4,4))[0]!=new:raise RuntimeError('split did not reach insertion')
        uid=struct.unpack('<Q',cpu.mem_read(new+0x38,8))[0];made=struct.unpack('<I',cpu.mem_read(new+0x64,4))[0];remaining=cpu.mem_read(source+0x42,1)[0];payload=bytes(cpu.mem_read(new+0x40,33)).hex();make=cpu.mem_read(new+0x68,1)[0]
        print(count,offered,kind,type_id,server,wisdom,tail.hex(),bag,slot,uid,made,remaining,payload,make,sep='\t')


if __name__=='__main__':main()
