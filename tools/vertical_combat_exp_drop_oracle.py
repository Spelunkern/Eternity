#!/usr/bin/env python3
"""Execute original ExpDrop with a synthetic cumulative experience catalog."""
import itertools
import random
import struct
import sys
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();user,world,stack,stop=(0x30000000,0x31000000,0x20000000,0x10000000)
    for base in (user,world,stack,stop):cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    calls=[]
    def hook(machine,address,size,_):
        if address==0x4d4f10:
            sp=machine.reg_read(UC_X86_REG_ESP)
            if get(sp+8)!=10:raise RuntimeError('wrong DB length')
            calls.append(bytes(cpu.mem_read(get(sp+4),10)).hex())
    cpu.hook_add(UC_HOOK_CODE,hook);cpu.mem_write(0x4d4f10,b'\xc2\x08\x00');put(0x546f38,world)
    wide=len(sys.argv)>1 and sys.argv[1]=='--wide'
    for grow in range(4):
        total=0
        for level in range(61):
            if level:
                increment=(((grow+1)*level*0x1f00f0f)&0xffffffff) if wide else (grow+1)*1000*level
                total=(total+increment)&0xffffffff
            put(0x54cedc+(grow*61+level)*4,total)
    print(f'# ps_game.exe SHA256 {digest}; CUser::ExpDrop 0045ad50, SendDBExp 0046c6c0, DB transport intercepted; x87 control=027f; catalog={"wide" if wide else "ordinary"}')
    print('# grow level penalty protect reduction exp output packet')
    randomizer=random.Random(0x45ad50)
    edges=(0,1,50,99,100,101,199,0x7fffffff,0x80000000,0xffffffff)
    random_cases=((randomizer.randrange(6),randomizer.choice((11,12,33,59,60,61,200)),randomizer.choice(edges),0,randomizer.choice(edges),randomizer.choice((2000000,0x7fffffff,0x80000000,0xffffffff,randomizer.getrandbits(32)))) for _ in range(512))
    standard=itertools.product((0,2,4),(10,11,60,61,200),(0,1,20,100),(0,1),(0,25,101),(0,55000,2000000,0xffffffff))
    for grow,level,penalty,protect,reduction,exp in itertools.chain(standard,random_cases):
        cpu.mem_write(user,bytes(0x7000));cpu.mem_write(user+0x12f,bytes((grow,)));cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(user+0x136,struct.pack('<H',level));put(user+0x58bc,protect);put(user+0x13c,exp);put(user+0x578c,123);put(world+0x274+132,reduction)
        put(stack+0xf000,stop);put(stack+0xf004,penalty);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_EAX,user);cpu.reg_write(UC_X86_REG_FPCW,0x027f);calls.clear();cpu.emu_start(0x45ad50,stop,count=1000)
        if len(calls)>1:raise RuntimeError('unexpected DB sends')
        print(grow,level,penalty,protect,reduction,exp,get(user+0x13c),calls[0] if calls else '-',sep='\t')

if __name__=='__main__':main()
