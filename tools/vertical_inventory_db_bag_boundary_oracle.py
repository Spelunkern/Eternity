#!/usr/bin/env python3
"""Trace original DB-agent 0701 bag positions that overlap native bank slots."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent')
    stack,user,item,packet,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for address in (stack,user,item,packet,stop):cpu.mem_map(address,0x10000)
    rng=random.Random(0x408480)
    print(f'# ps_dbAgent.exe SHA256 {digest}; CUser::PacketUserItem 00408480 case 0701; CUser inventory +00e0 [6][24], bank +0320 [240]')
    print('# bag slot bank_index old_count add_count old_save new_count new_save packet')
    for case in range(128):
        for address in (stack,user,item,packet):cpu.mem_write(address,bytes(0x10000))
        bag=6+case%4;slot=rng.randrange(24);index=(bag-6)*24+slot;old=rng.randrange(256);added=rng.randrange(256);save=rng.choice((0,1,2));kind=rng.randrange(256);type_id=rng.randrange(256)
        cpu.mem_write(user+0x320+index*4,struct.pack('<I',item));cpu.mem_write(item+8,struct.pack('<i',save));cpu.mem_write(item+0x1c,bytes((kind,type_id,old)))
        data=struct.pack('<HIQI5BH',0x701,12345,0x123456789abcdef0,5001,bag,slot,kind,type_id,added,0)+bytes(6)+bytes(21)+struct.pack('<IB',0,0)
        cpu.mem_write(packet,data);cpu.mem_write(stack+0x8000,struct.pack('<II',stop,packet));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,user);cpu.emu_start(0x408480,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('DB-agent item handler did not return')
        count=cpu.mem_read(item+0x1e,1)[0];new_save=struct.unpack('<i',cpu.mem_read(item+8,4))[0]
        if count!=(old+added)%256 or new_save!=(2 if save==0 else save):raise RuntimeError('unexpected native bank alias')
        print(bag,slot,index,old,added,save,count,new_save,data.hex(),sep='\t')


if __name__=='__main__':main()
