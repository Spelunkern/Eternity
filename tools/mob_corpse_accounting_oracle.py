#!/usr/bin/env python3
"""Execute regular corpse accounting instructions from UpdateMobDeath."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x10000)
    def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    cpu.mem_write(0x504697,b'\xc3');context={}
    def hook(machine,address,size,data):
        if address==0x504697:context['calls']+=1;machine.reg_write(UC_X86_REG_EAX,context['random'])
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; UpdateMobDeath regular accounting 00421d02..00421d62; rand is a port; no traversal/unlink executed.')
    print('# current count deadline duration selector now random expected_current expected_next calls')
    for current in (0,3,0x80000000):
        for count in (0,3,0x80000000):
            for duration in (0,5000,14400000):
                for selector in (0,8,9,15):
                    for random in (0,59,60,119,32767):
                        mob,respawn,definition=base,base+0x2000,base+0x3000;now=0xfffffff0
                        put(respawn+4,count);put(respawn+8,current);put(respawn+12,123);put(mob+0xc84,respawn);put(mob+0xa8,definition);put(definition+108,duration);cpu.mem_write(definition+0xb7,bytes([selector]))
                        context.update(random=random,calls=0)
                        for reg,value in ((UC_X86_REG_EAX,respawn),(UC_X86_REG_ESI,mob),(UC_X86_REG_EBP,0),(UC_X86_REG_EBX,now),(UC_X86_REG_ESP,base+0xf000)):cpu.reg_write(reg,value)
                        cpu.emu_start(0x421d02,0x421d62,count=100)
                        if cpu.reg_read(UC_X86_REG_EIP)!=0x421d62:raise RuntimeError('Corpse accounting did not return')
                        print(current,count,123,duration,selector,now,random,get(respawn+8),get(respawn+12),context['calls'],sep='\t')
if __name__=='__main__':main()
