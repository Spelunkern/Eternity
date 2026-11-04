#!/usr/bin/env python3
"""Execute original mixed ordinary/portal scene list sent directly to one user."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    map_,ob,other,user,links,targets,stack,stop=[base+x for x in (0,0x1000,0x8000,0x9000,0x20000,0x30000,0xe0000,0xf0000)];state={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def position(at,x,z):cpu.mem_write(at,struct.pack('<fff',x,12345.5,z))
    cpu.mem_write(0x4d4d20,b'\xc2\x08\x00')
    def hook(machine,address,size,data):
        if machine.reg_read(UC_X86_REG_ECX)!=user:raise RuntimeError('Original selected wrong user connection')
        sp=machine.reg_read(UC_X86_REG_ESP);n=get(sp+8);state['trace'].extend((n,*cpu.mem_read(get(sp+4),n)));state['sends']+=1
        if state['scenario']//48%2 and state['sends']==1:
            for i in range(100,state['ordinary']):
                at=ob+i*0x70;put(at,get(at)^0xa5a5a5a5,2);position(at+0x1c,i+99.5,-i-99.75)
            for i in range(state['portals']):
                at=links+i*0x84;put(at,get(at)^0x55555555);put(at+0x10,map_ if (state['scenario']+i)%3 else other)
                put(targets+i*0x40+0x14,(state['scenario']+i)%6-1);position(targets+i*0x40+4,i+200.25,-i-200.5)
    cpu.hook_add(UC_HOOK_CODE,hook,begin=0x4d4d20,end=0x4d4d20)
    print(f'# ps_game.exe SHA256 {digest}; actual CMap::SendObeliskList 0042b040, only user connection Send 004d4d20 observed. No map-zone guard. Ordinary then matching portal rows share100-record chunks; callback mutates later ordinary and live portal identity/country/position/map selection.')
    print('# scenario followed by repeated packet length/bytes. Ordinary counts0/1/2/99/100/101/199/200; portals0/1/2/12/100/101. Mutation=scenario//48%2, initial matching pattern=scenario//96%2.')
    for scenario in range(192):
        cpu.mem_write(base,bytes(0xff000));n=(0,1,2,99,100,101,199,200)[scenario%8];p=(0,1,2,12,100,101)[scenario//8%6]
        state.update(scenario=scenario,ordinary=n,portals=p,sends=0,trace=[]);put(map_+0x1e8,n,ob);put(0x5471c8,p,links)
        for i in range(n):
            at=ob+i*0x70;put(at,0xf1230000+i,(-1,0,1,2,3,0x80000000,0x7fffffff)[(scenario+i)%7]);position(at+0x1c,i+0.25,-i-0.5)
        for i in range(p):
            at=links+i*0x84;put(at,0x81230000+i);put(at+0x10,map_ if scenario//96%2==0 or i%2 else other,targets+i*0x40)
            put(targets+i*0x40+0x14,(-1,0,1,2,3,0x80000000,0x7fffffff)[(scenario+i)%7]);position(targets+i*0x40+4,i+20.25,-i-20.5)
        put(stack,stop,user);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EBX,map_);cpu.emu_start(0x42b040,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original user list did not return')
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__':main()
