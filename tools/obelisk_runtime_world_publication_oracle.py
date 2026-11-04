#!/usr/bin/env python3
"""Real world persistence coordination and both original list serializers."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_ECX
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    world,maps,records,portals,stack,stop=[base+x for x in (0,0x1000,0x10000,0x60000,0xe0000,0xf0000)];state={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    cpu.mem_write(0x4d4f10,b'\xc2\x08\x00')
    def hook(machine,address,size,data):
        if machine.reg_read(UC_X86_REG_ECX)!=base+0x8000:raise RuntimeError('Unexpected DB connection identity')
        sp=machine.reg_read(UC_X86_REG_ESP);n=get(sp+8);state['trace'].extend((n,*cpu.mem_read(get(sp+4),n)));state['sends']+=1
        if state['scenario']//8%2:
            for mi,count in enumerate(state['counts']):
                for row in range(count):
                    at=records+mi*0x10000+row*0x70;put(at+4,(get(at+4)+1)%3);put(at+8,(get(at+8)+1)%3);put(at+0x10,get(at+0x10)+state['sends']);put(at+0x14,get(at+0x14)^0xa5a5a5a5)
            for i in range(state['portals']):put(portals+i*0x84+4,get(portals+i*0x84+4)^0x55555555)
    cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4d4f10,end=0x4d4f10)
    print(f'# ps_game.exe SHA256 {digest}; actual CWorld::SendDBObeliskList 00413b90 -> SendDBPortalLinkList 0042b460 -> per-map SendDBObeliskList 0042b370. Only shared DB transport observed. Every send mutates all live ordinary/portal values in alternate scenario blocks; no table/count changes.')
    print('# scenario followed by repeated packet length and bytes. Portal counts0,1,99,100,101,199,200,201. Map counts cycle0,1,2,99,100,101,199,200; map table length=scenario//16%5.')
    for scenario in range(128):
        cpu.mem_write(base,bytes(0xff000));count=scenario//16%5;counts=[(0,1,2,99,100,101,199,200)[(scenario//2+i)%8] for i in range(count)];p=(0,1,99,100,101,199,200,201)[scenario%8]
        state.update(scenario=scenario,counts=counts,portals=p,trace=[],sends=0);put(world+8,count,maps);put(0x5471c8,p,portals);put(0x546f44,base+0x8000)
        for mi,n in enumerate(counts):
            put(maps+mi*0x1f0+0x1e8,n,records+mi*0x10000)
            for row in range(n):put(records+mi*0x10000+row*0x70+4,(scenario+mi+row)%3,(scenario//3+mi+row)%3,0,0x12340000+mi*256+row,0x00010000+mi*256+row)
        for i in range(p):put(portals+i*0x84+4,0x10010000+i)
        put(stack,stop,world);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.emu_start(0x413b90,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original world list did not return')
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__':main()
