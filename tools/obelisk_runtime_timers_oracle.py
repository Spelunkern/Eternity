#!/usr/bin/env python3
"""Execute original CMap::UpdateRespawnObelisk and its real complete-respawn path."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
                              UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP)
from original_oracle import load_game


def main(initialization=False):
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x200000)
    map_, ob, groups, bitmap, definitions, zone, manager = [base+x for x in (0,0x1000,0x2000,0x3000,0x4000,0x6000,0x8000)]
    stack, stop = base+0x1e0000, base+0x1f0000
    state = {}
    def put(at,*values): cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def get(at): return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def event(*values): state['trace'].extend(v&0xffffffff for v in values)
    def identity(pointer): return (pointer-(base+0x10000))//0x1000+100 if pointer else 0
    ports = {0x40a5e0:0,0x40ab80:0,0x490040:8,0x416860:4,0x4d4e50:8,
             0x504697:0,0x5046b9:0,0x4a8980:0,0x505316:0}
    for address,cleanup in ports.items(): cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210f8,stop+16)
    def hook(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP);eax=machine.reg_read(UC_X86_REG_EAX)
        if address in (0x40a5e0,0x40ab80):
            event(9,int(address==0x40a5e0));attempt=state['alloc'];state['alloc']+=1
            failed=(state['scenario']//17%3==1 and attempt%2==0) or state['scenario']//17%3==2
            machine.reg_write(UC_X86_REG_EAX,0 if failed else base+0x10000+attempt*0x1000)
        elif address==stop+16:
            state['id']+=1;event(10,state['id']);machine.reg_write(UC_X86_REG_EAX,state['id'])
        elif address==0x5046b9: machine.reg_write(UC_X86_REG_EAX,123456)
        elif address==0x490040:
            mob=machine.reg_read(UC_X86_REG_ESI);respawn=get(sp+8)
            cpu.mem_write(mob+0x220,struct.pack('<H',get(sp+4)))
            state['bindings'][mob]=respawn
            offset=respawn-ob if respawn<groups else respawn-groups+1000
            event(11,identity(mob),get(sp+4),offset)
            # Observe the original's post-Init definition lookup, rather than
            # assuming the selected template is still the mob's live template.
            if state['scenario']%11==0 and respawn>=groups: put(mob+0xa8,definitions+0x400)
        elif address==0x416860:
            respawn=state['bindings'][eax]
            event(12,identity(eax),get(eax+0x50),get(eax+0x54),get(eax+0x58),get(respawn+8),get(respawn+12))
            if initialization and state['scenario']%5==0 and respawn>=groups:
                put(respawn+8,get(respawn+8)-1)
        elif address==0x4d4e50:
            length=get(sp+8);event(6,length,*cpu.mem_read(get(sp+4),length))
        elif address==0x504697:
            value=(state['random']*17+state['scenario']*13)%32768;state['random']+=1
            event(13,value);machine.reg_write(UC_X86_REG_EAX,value)
    cpu.hook_add(UC_HOOK_CODE,hook)
    if initialization: print('# InitObelisk 00429a60 followed by three real scheduler ticks; tag 16=startup. Controlled EnterMob callback decrements companion current in scenarios divisible by 5.')
    print(f'# ps_game.exe SHA256 {digest}; UpdateRespawnObelisk 00429da0, real ObeliskRespawn 0042a6f0, CBitArray::GetBit 0041b3a0, AddMob 00429930. External allocation, Init, enter, random/time/seed/atomic and transport are observed ports.')
    print('# scenario followed by trace uint32 words. 14=tick, 9=allocation(manager flag), 10=heap ID, 11=Init(identity,type,respawn offset), 12=enter(identity,float bits x/y/z,current,deadline), 13=rand, 6=log(length,bytes), 99=principal snapshot, 98=spawn snapshot. Three successive ticks retain state.')
    for scenario in range(768):
        cpu.mem_write(base,bytes(0x1ff000));cpu.mem_write(stop+16,b'\xc2\x04\x00')
        state.update(scenario=scenario,trace=[],alloc=0,random=0,id=500,bindings={})
        now=(0,1,100,0x7fffffff,0x80000000,0xfffffffe,0xffffffff)[scenario%7]
        deadlines=(0,(now-1)&0xffffffff,now,(now+1)&0xffffffff,0xffffffff,0x80000000)
        put(map_,zone if scenario%19 else 0,8,bitmap);put(map_+0x78,42);put(map_+0x1e8,2,ob);put(0x546f3c,manager)
        cpu.mem_write(bitmap,bytes([0xaa if scenario//3%2 else 0]*8))
        for d,type_id in enumerate((321,322,401,402,999)):
            cpu.mem_write(definitions+d*0x100,struct.pack('<H',type_id)+b'ABCDEFGHIJKLMNOPQRSTUVWXYZ12345\0')
            put(definitions+d*0x100+0x6c,(0,1,20,0xffffffff)[(scenario//13+d)%4] if d!=4 else 0xfffffffc)
        for oi in range(2):
            at=ob+oi*0x70;country=(scenario+oi)%3;put(at+4,country)
            cpu.mem_write(at+0x1c,struct.pack('<fff',2.5+oi,3.5,4.5))
            put(at+0x34+country*4,0 if (scenario+oi)%9==0 else definitions+oi*0x100)
            put(at+0x48,(-1,0,1,2)[(scenario//7+oi)%4],deadlines[(scenario//3+oi)%6])
            put(at+0x64,2,groups+oi*0x100,0)
            for g,count in enumerate((3,1)):
                spawn=groups+oi*0x100+g*0x58
                cpu.mem_write(spawn,struct.pack('<ffffii',0.25+g*6,1.5+oi,0.75+g*6,1.5,4,count))
                put(spawn+0x24+country*4,0 if (scenario//2+oi*3+g)%5==0 else definitions+(2+g)*0x100)
                put(spawn+0x38,(-1,0,1,3)[(scenario//11+oi*2+g)%4],deadlines[(scenario//5+oi*2+g)%6])
                put(spawn+0x54,base+0x9000+oi*0x200+g*0x100)
        def snapshot():
            for oi in range(2):
                at=ob+oi*0x70;event(99,get(at+0x48),get(at+0x4c),identity(get(at+0x6c)))
                for g,count in enumerate((3,1)):
                    spawn=groups+oi*0x100+g*0x58;members=get(spawn+0x54)
                    event(98,get(spawn+0x38),get(spawn+0x3c),*[identity(get(members+n*4)) for n in range(count)])
        if initialization:
            event(16);put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EBX,map_)
            cpu.emu_start(0x429a60,stop,count=100000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('Original initialization instruction limit exceeded')
            snapshot()
        for tick in (now,(now+1)&0xffffffff,(now+20)&0xffffffff):
            event(14,tick);put(stack,stop,tick);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EBX,map_)
            cpu.emu_start(0x429da0,stop,count=100000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('Original instruction limit exceeded')
            snapshot()
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__': main()
