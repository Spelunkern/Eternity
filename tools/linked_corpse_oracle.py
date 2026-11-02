#!/usr/bin/env python3
"""Execute linked-principal corpse timer instructions, including actual SRandom."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x20000);stop=base+0x1f000
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    context={};cpu.mem_write(0x504697,b'\xc3')
    def hook(machine,address,size,data):
        if address==0x504697:context['calls']+=1;machine.reg_write(UC_X86_REG_EAX,context['rand'])
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; UpdateMobDeath linked timer 00421c16..00421d62, real SRandom seed/draw; rand port. No corpse traversal/unlink.')
    print('# phase randomized minimum extra group_delay mob_delay old_next now seed rand expected_next current calls mt_index mt_word0')
    for scenario in range(256):
        cpu.mem_write(base,bytes(0x1e000));mob,respawn,definition,zone,group=base,base+0x2000,base+0x3000,base+0x4000,base+0x6000
        phase=scenario%3;randomized=(scenario//3)%2;minimum=(0,1,0xffffffff)[(scenario//17)%3];extra=(0,1,5)[(scenario//7)%3];group_delay=(0,180000,7200000)[(scenario//11)%3];mob_delay=(0,5000)[scenario%2];now=(0,1000,0xfffffff0)[(scenario//5)%3];old=(0,now,0xffffffff,(now+1)&0xffffffff)[(scenario//13)%4];seed=scenario*1234567&0xffffffff;rand=(0,60,119,32767)[scenario%4]
        put(base+0x1e000,stop);cpu.reg_write(UC_X86_REG_ESP,base+0x1e000);cpu.reg_write(UC_X86_REG_EAX,mob+0x2bc);cpu.reg_write(UC_X86_REG_EDI,seed);cpu.emu_start(0x4a8980,stop,count=100000)
        put(mob+0xc84,respawn);put(mob+0xa8,definition);put(definition+108,mob_delay);put(respawn,0,1,7,old);put(zone+0x270,1,group);put(group+8,group_delay);put(group+0x26c8,phase);put(group+0x118,randomized,minimum,extra)
        sp=base+0x1e000;put(sp+0x20,zone,now);context.update(calls=0,rand=rand)
        for reg,value in ((UC_X86_REG_EAX,respawn),(UC_X86_REG_ESI,mob),(UC_X86_REG_EBP,0),(UC_X86_REG_EBX,now),(UC_X86_REG_ESP,sp)):cpu.reg_write(reg,value)
        cpu.emu_start(0x421c16,0x421d62,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x421d62:raise RuntimeError('Linked corpse timer did not finish')
        print(phase,randomized,minimum,extra,group_delay,mob_delay,old,now,seed,rand,get(respawn+12),get(respawn+8),context['calls'],get(mob+0x2bc+2500),get(mob+0x2bc+4),sep='\t')
if __name__=='__main__':main()
