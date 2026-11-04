#!/usr/bin/env python3
"""Original Death post-lookup branch and complete PvPGetDeathType, no patches.
Only this bounded decision region is executed; reward/defeat/tail are exits.
"""
import struct
from original_oracle import load_game
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP

def main():
    c,digest=load_game();victim=0x30000000;peer=victim+0x8000;stack=victim+0x10000
    c.mem_map(victim,0x12000)
    def put(at,n):c.mem_write(at,struct.pack('<I',n&0xffffffff))
    def get(at):return struct.unpack('<I',c.mem_read(at,4))[0]
    exits={0x4588c3:3,0x458cb7:None,0x458cbe:0}
    found={}
    def hook(cpu,at,size,data):
        if at in exits:
            found['route']=get(cpu.reg_read(UC_X86_REG_ESP)) if at==0x458cb7 else exits[at]
            if at==0x458cb7 and get(cpu.reg_read(UC_X86_REG_ESP)+4)!=found['peer']:raise RuntimeError('Wrong defeat peer')
            cpu.emu_stop()
    c.hook_add(UC_HOOK_CODE,hook)
    print('# ps_game.exe SHA256 '+digest+'; Death decision 0045885c..004588bd plus 00458cab..00458cb6 and complete helper 0047b270; exit before rewards/defeat/tail; no patched instructions')
    print('# same forced penalty | victim id/state/peer/guild/opposing | attacker id/state/peer/guild/opposing | route final_penalty')
    raw=(0,1,3,4,5,0xffffffff,0x80000000,0x7fffffff)
    for case in range(16384):
        same=case%2;flag=(0,1,2,128,255)[(case//2)%5];penalty=raw[(case//10)%8]
        a=[raw[(case//13)%8],raw[(case//7)%8],raw[(case//17)%8],raw[(case//19)%8],raw[(case//23)%8]]
        b=[raw[(case//29)%8],raw[(case//31)%8],raw[(case//37)%8],raw[(case//41)%8],raw[(case//43)%8]]
        if case%3==0:a[2]=b[0]
        if case%5==0:b[2]=a[0]
        if case%7==0:a[4]=b[3];b[4]=a[3]
        if same:b=a.copy()
        for addr,values in ((victim,a),(peer,b)):
            for offset,value in zip((0xdc,0x54cc,0x54d4,0x1748,0x54e4),values):put(addr+offset,value)
        put(victim+0x145c,penalty);c.mem_write(stack+0x16c,bytes([flag]))
        c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,0) # CL is NUL from completed name copy.
        c.reg_write(UC_X86_REG_EDI,victim);c.reg_write(UC_X86_REG_EBP,victim if same else peer)
        found.clear();found['peer']=victim if same else peer
        c.emu_start(0x45885c,0,count=200)
        if 'route' not in found:raise RuntimeError('Decision did not reach a known exit')
        print(same,flag,penalty,*a,*b,found['route'],get(victim+0x145c))
if __name__=='__main__':main()
