#!/usr/bin/env python3
"""Execute original exchange item sender and receiver packet slices."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,item,offer,users,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for address,size in ((stack,0x10000),(item,0x10000),(offer,0x10000),(users,0x20000),(stop,0x1000)):cpu.mem_map(address,size)
    cpu.mem_write(0x4d4d20,b'\xc2\x08\x00');events=[]
    def hook(machine,address,size,data):
        if address in (0x46fe48,0x46ffec):machine.emu_stop();return
        sp=machine.reg_read(UC_X86_REG_ESP);ptr,length=struct.unpack('<II',machine.mem_read(sp+4,8));events.append(bytes(machine.mem_read(ptr,length)).hex())
    for address in (0x4d4d20,0x46fe48,0x46ffec):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0x46ff56)
    print(f'# ps_game.exe SHA256 {digest}; CUser::ExchangeItemAdd 0046f910 sender packet 0046fdf7..0046fe48 and receiver packet 0046ff56..0046ffec; real original packet instructions')
    print('# source_bag source_slot offered_count destination_bag destination_slot remains item_payload receiver_scratch sender_packet receiver_packet')
    for case in range(1024):
        source_bag=rng.randrange(1,10);source_slot=rng.randrange(24);offered=rng.randrange(256);destination_bag=rng.randrange(1,10);destination_slot=rng.randrange(24);remains=case%2
        kind=rng.randrange(256);type_id=rng.randrange(256);count=rng.randrange(256);quality=rng.randrange(65536);gems=bytes(rng.randrange(256) for _ in range(6));length=rng.randrange(21);craft=bytes(rng.randrange(1,256) for _ in range(length))+b'\0'+bytes(rng.randrange(256) for _ in range(20-length));payload=bytes((kind,type_id,count,0))+struct.pack('<H',quality)+gems+craft;scratch=bytes(rng.randrange(256) for _ in range(36))
        for address,size in ((stack,0x10000),(item,0x10000),(offer,0x10000)):cpu.mem_write(address,bytes(size))
        cpu.mem_write(item+0x40,payload);cpu.mem_write(offer,bytes((source_bag,source_slot,offered)));sp=stack+0x8000;cpu.mem_write(sp+0x13,bytes((remains,)));cpu.mem_write(sp+0x18,struct.pack('<I',users));cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,item);cpu.reg_write(UC_X86_REG_EDI,offer);events.clear();cpu.emu_start(0x46fdf7,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x46fe48 or len(events)!=1:raise RuntimeError('original sender packet did not finish')
        sender=events[0];cpu.mem_write(stack,bytes(0x10000));cpu.mem_write(sp+0x24,bytes((destination_bag,)));cpu.mem_write(sp+0x14,struct.pack('<I',offer));cpu.mem_write(sp+0x74,scratch);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,item);cpu.reg_write(UC_X86_REG_EDI,users+0x10000);cpu.reg_write(UC_X86_REG_EBX,destination_slot);cpu.reg_write(UC_X86_REG_ESI,(-(item+0x4c))&0xffffffff);events.clear();cpu.emu_start(0x46ff56,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x46ffec or len(events)!=1:raise RuntimeError('original receiver packet did not finish')
        print(source_bag,source_slot,offered,destination_bag,destination_slot,remains,payload.hex(),scratch.hex(),sender,events[0],sep='\t')

if __name__=='__main__':main()
