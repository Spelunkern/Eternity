#!/usr/bin/env python3
"""Execute original damage/aggro selection on explicit intrusive records."""
import random
import struct
from unicorn.x86_const import UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EAX,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x10000);stop=base+0xf000;owner=base;head=base+0x100;out=base+0x200
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    cpu.mem_write(stop+16,b'\xc2\x04\x00');put(0x5210cc,stop+16);put(0x5210d0,stop+16)
    rng=random.Random(0x4b8ef0)
    print(f'# ps_game.exe SHA256 {digest}; CDamageMob GetMaxDamage 004b8ef0 / GetMaxAggro 004b9000; lock ports, intrusive records explicit, allocation/Add/Reset not executed.')
    print('# S scenario; N type id damage aggro party; D/A bool output28bytes')
    for scenario in range(128):
        n=scenario%9;nodes=[base+0x1000+i*32 for i in range(n)];put(owner+4,head,head);put(head,nodes[-1] if n else head,nodes[0] if n else head);print('S',scenario,sep='\t')
        for i,node in enumerate(nodes):
            kind=1 if i==0 else (1,2,5)[rng.randrange(3)];identity=rng.randrange(4);damage=rng.choice((0,1,100,0x7fffffff,0x80000000,0xffffffff));aggro=rng.choice((0,1,100,0x7fffffff,0x80000000,0xffffffff));party=rng.randrange(4)
            put(node,nodes[i-1] if i else head,nodes[i+1] if i+1<n else head);cpu.mem_write(node+8,bytes([kind]));put(node+12,identity,damage,aggro,party);print('N',kind,identity,damage,aggro,party,sep='\t')
        for tag,address,output_reg in (('D',0x4b8ef0,UC_X86_REG_EDI),('A',0x4b9000,UC_X86_REG_EBX)):
            cpu.mem_write(out,b'\xa5'*28);put(base+0xe000,stop);cpu.reg_write(UC_X86_REG_ESP,base+0xe000);cpu.reg_write(UC_X86_REG_ESI,owner);cpu.reg_write(output_reg,out);cpu.emu_start(address,stop,count=100000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Selection did not return')
            print(tag,cpu.reg_read(UC_X86_REG_EAX)&255,bytes(cpu.mem_read(out,28)).hex(),sep='\t')
if __name__=='__main__':main()
