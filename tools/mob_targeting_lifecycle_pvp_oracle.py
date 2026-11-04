#!/usr/bin/env python3
"""Execute complete unpatched ps_game.exe PvPGetDeathType 0047b270."""
import struct
from original_oracle import load_game
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP

def main():
    cpu,digest=load_game()
    base=0x30000000; peer=base+0x8000; stack=base+0x10000; stop=stack+0x1000
    cpu.mem_map(base,0x12000)
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value))
    print('# ps_game.exe SHA256 '+digest+'; complete CUser::PvPGetDeathType 0047b270; no hooks or patched instructions')
    print('# same | subject id/state/peer/guild/opposing | peer id/state/peer/guild/opposing | result')
    raw=(0,1,3,4,5,0xffffffff,0x80000000,0x7fffffff)
    for case in range(8192):
        same=case%2
        state=raw[(case//2)%8]
        a=[raw[(case//16)%8],state,raw[(case//128)%8],raw[(case//1024)%8],raw[(case//37)%8]]
        b=[raw[(case//29)%8],raw[(case//11)%8],raw[(case//17)%8],raw[(case//23)%8],raw[(case//31)%8]]
        # Deliberate correlated matches exercise precedence and asymmetric tests.
        if case%3==0:a[2]=b[0]
        if case%5==0:a[4]=b[3]
        if case%7==0:b[4]=a[3]
        if same:b=a.copy()
        for address,values in ((base,a),(peer,b)):
            for offset,value in zip((0xdc,0x54cc,0x54d4,0x1748,0x54e4),values):put(address+offset,value)
        put(stack,stop)
        cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EAX,base)
        cpu.reg_write(UC_X86_REG_ESI,base if same else peer)
        cpu.emu_start(0x47b270,stop,count=100)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Incomplete helper execution')
        print(same,*a,*b,cpu.reg_read(UC_X86_REG_EAX))
if __name__=='__main__':main()
