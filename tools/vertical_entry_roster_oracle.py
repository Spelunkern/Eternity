#!/usr/bin/env python3
"""Execute original DB-agent 0407 roster scan up to its selected branch."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EIP
from original_oracle import load_original

def main():
    cpu,digest=load_original('ps_dbAgent')
    user,request=0x30000000,0x31000000
    cpu.mem_map(user,0x10000);cpu.mem_map(request,0x1000)
    for address in (0x40591f,0x405a2b):
        cpu.hook_add(UC_HOOK_CODE,lambda m,a,s,c:m.emu_stop(),begin=address,end=address)
    rng=random.Random(0x4059e2)
    print(f'# ps_dbAgent.exe SHA256 {digest}; PacketUserChar 004059e2..00405a2b; retained populated slots, original branch execution')
    for _ in range(2048):
        cpu.mem_write(user,bytes(0x10000));ids=[rng.choice((0,1,2,3,4,0xffffffff)) for i in range(5)]
        target=rng.choice((1,2,3,4,5,0xffffffff))
        fields=[]
        for i,ident in enumerate(ids):
            rename=rng.choice((0,1,127,255));remain=rng.choice((0,1,-1,-2147483648,2147483647))
            cpu.mem_write(user+0x3b6c+i*0x5c,struct.pack('<I',ident))
            cpu.mem_write(user+0x3bc4+i*0x5c,struct.pack('<i',rename))
            cpu.mem_write(user+0x3b94+i*0x5c,struct.pack('<i',remain))
            fields.extend((ident,int(rename!=0),int(remain!=0)))
        cpu.mem_write(request+6,struct.pack('<I',target));cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_EDI,request)
        cpu.emu_start(0x4059e2,0,count=1000);end=cpu.reg_read(UC_X86_REG_EIP)
        if end not in (0x40591f,0x405a2b):raise RuntimeError('Original roster gate failed to reach branch')
        print(target,*fields,int(end==0x405a2b))
if __name__=='__main__': main()
