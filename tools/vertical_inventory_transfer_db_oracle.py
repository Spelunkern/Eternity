#!/usr/bin/env python3
"""Execute original exchange item DB add/remove instruction slices."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,source,transferred,definition,offer,users,stop=0x20000000,0x30000000,0x30010000,0x31000000,0x32000000,0x33000000,0x10000000
    for address,size in ((stack,0x10000),(source,0x20000),(definition,0x10000),(offer,0x10000),(users,0x20000),(stop,0x1000)):cpu.mem_map(address,size)
    cpu.mem_write(0x4d4f10,b'\xc2\x08\x00');events=[]
    def hook(machine,address,size,data):
        if address==0x46fdf7:machine.emu_stop();return
        sp=machine.reg_read(UC_X86_REG_ESP);ptr,length=struct.unpack('<II',machine.mem_read(sp+4,8));events.append(bytes(machine.mem_read(ptr,length)).hex())
    for address in (0x4d4f10,0x46fdf7):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
    rng=random.Random(0x46fa10)
    print(f'# ps_game.exe SHA256 {digest}; CUser::ExchangeItemAdd 0046f910 whole DB sends 0046fa10..0046fb1c and split DB sends 0046fceb..0046fdf7; original packet instructions')
    print('# split source_bag source_slot offered_count source_remaining destination_bag destination_slot source_uid transferred_uid item_id item_payload made_at make_type scratch add_packet remove_packet')
    for case in range(1024):
        split=case%2;source_bag=rng.randrange(1,6);source_slot=rng.randrange(24);destination_bag=rng.randrange(1,6);destination_slot=rng.randrange(24);remaining=rng.randrange(1,200);offered=rng.randrange(1,256-remaining) if split else rng.randrange(1,256);count=offered if split else remaining
        source_uid=rng.getrandbits(64);transfer_uid=(source_uid^0x8000000000000000) if split else source_uid;item_id=rng.getrandbits(32);kind=rng.randrange(1,256);type_id=rng.randrange(1,256);quality=rng.randrange(65536);gems=bytes(rng.randrange(256) for _ in range(6));length=rng.randrange(21);craft=bytes(rng.randrange(1,256) for _ in range(length))+b'\0'+bytes(rng.randrange(256) for _ in range(20-length));payload=bytes((kind,type_id,count,0))+struct.pack('<H',quality)+gems+craft;made_at=rng.getrandbits(32);make_type=rng.randrange(256);scratch=bytes(rng.randrange(256) for _ in range(57));item=transferred if split else source
        for address,size in ((stack,0x10000),(source,0x20000),(definition,0x10000),(offer,0x10000),(users,0x20000)):cpu.mem_write(address,bytes(size))
        cpu.mem_write(definition,struct.pack('<I',item_id));cpu.mem_write(item+0x30,struct.pack('<I',definition));cpu.mem_write(item+0x38,struct.pack('<Q',transfer_uid));cpu.mem_write(item+0x40,payload);cpu.mem_write(item+0x64,struct.pack('<I',made_at));cpu.mem_write(item+0x68,bytes((make_type,)))
        if split:cpu.mem_write(source+0x42,bytes((remaining,)))
        cpu.mem_write(offer,bytes((source_bag,source_slot,offered)));cpu.mem_write(users+0x578c,struct.pack('<I',12345));cpu.mem_write(users+0x10000+0x578c,struct.pack('<I',12346));sp=stack+0x8000;cpu.mem_write(sp+0x18,struct.pack('<I',users));cpu.mem_write(sp+0x14,struct.pack('<I',offer));cpu.mem_write(sp+(0x98 if split else 0xd4),scratch)
        cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_EBP,source);cpu.reg_write(UC_X86_REG_EBX,destination_slot);cpu.reg_write(UC_X86_REG_EDI,offer)
        if split:cpu.reg_write(UC_X86_REG_ESI,transferred);cpu.reg_write(UC_X86_REG_EAX,destination_bag);cpu.reg_write(UC_X86_REG_ECX,users+0x10000)
        else:cpu.reg_write(UC_X86_REG_ESI,users+0x10000);cpu.reg_write(UC_X86_REG_ECX,destination_bag)
        events.clear();cpu.emu_start(0x46fceb if split else 0x46fa10,stop,count=20000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x46fdf7 or len(events)!=2:raise RuntimeError('original exchange DB packet slices did not finish')
        print(split,source_bag,source_slot,offered,remaining,destination_bag,destination_slot,source_uid,transfer_uid,item_id,payload.hex(),made_at,make_type,scratch.hex(),events[0],events[1],sep='\t')

if __name__=='__main__':main()
