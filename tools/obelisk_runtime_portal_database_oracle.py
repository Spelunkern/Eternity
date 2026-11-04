#!/usr/bin/env python3
"""Execute original initial portal DB list, including live chunk callback reads."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    links=base;stack=base+0xe0000;stop=base+0xf0000;state={}
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[n&0xffffffff for n in values]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    cpu.mem_write(0x4d4f10,b'\xc2\x08\x00')
    def hook(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP);n=get(sp+8);p=get(sp+4)
        state['trace'].extend((n,*cpu.mem_read(p,n)));state['sends']+=1
        if state['scenario']//12%2 and state['sends']==1:
            for i in range(100,state['count']):
                at=links+i*0x84;put(at+4,get(at+4)^0xa5a5a5a5);put(at+0x14,0xdeadbeef)
                cpu.mem_write(at+0x18,b'\x01\x01')
    cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4d4f10,end=0x4d4f10)
    print(f'# ps_game.exe SHA256 {digest}; actual CMap::SendDBPortalLinkList 0042b460. Initial persistence always country3/time0, independent of target country/pointer/open flags. Only DB transport observed. Full chunk callback mutates later base IDs in alternate blocks.')
    print('# scenario followed by repeated length and raw packet bytes. Count cycles0,1,2,12,99,100,101,199,200,201,255,301. Initial target pointers include null and invalid values, proving no target dereference.')
    counts=(0,1,2,12,99,100,101,199,200,201,255,301)
    for scenario in range(96):
        cpu.mem_write(base,bytes(0xff000));count=counts[scenario%12];state.update(scenario=scenario,count=count,sends=0,trace=[])
        put(0x5471c8,count,links)
        for i in range(count):
            at=links+i*0x84;put(at,0xf1230000+i,((i*0x10203)^0x10000000^scenario)&0xffffffff)
            put(at+0x14,(0,0xdeadbeef,0xffffffff)[(scenario//12+i)%3]);cpu.mem_write(at+0x18,bytes([(scenario+i)%2,(scenario//2+i)%2]))
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.emu_start(0x42b460,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original portal list did not return')
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__':main()
