#!/usr/bin/env python3
"""Execute ps_game.exe ObeliskDestroy/Respawn; observe required external services."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EBX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    base = 0x30000000
    cpu.mem_map(base, 0x200000)
    map_, ob, groups, bitmap, definition, dead, zone, world, manager = [base+x for x in (0,0x1000,0x2000,0x3000,0x4000,0x5000,0x6000,0x7000,0x8000)]
    stop, stack = base+0x1f0000,base+0x1e0000
    state = {}
    def put(at,*values): cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def get(at): return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def event(*values): state['trace'].extend(v&0xffffffff for v in values)
    def identity(m): return (m-(base+0x10000))//0x1000+100 if m>=base+0x10000 else 77
    ports={0x41e910:4,0x42b200:0,0x413600:8,0x413750:8,0x412ec0:0,0x4d4e50:8,0x4d4f10:8,0x42a000:0,
           0x40a5e0:0,0x40ab80:0,0x490040:8,0x416860:4,0x504697:0,0x5046b9:0,0x4a8980:0,0x505316:0,stop+16:4}
    for at,cleanup in ports.items(): cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210f8,stop+16)
    def hook(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP);eax=machine.reg_read(UC_X86_REG_EAX);ecx=machine.reg_read(UC_X86_REG_ECX)
        if address==0x41e910: event(1,get(sp+4))
        elif address==0x42b200: event(2,eax)
        elif address==0x413600: event(3,machine.reg_read(UC_X86_REG_EBX),get(sp+8))
        elif address==0x413750: event(4,machine.reg_read(UC_X86_REG_EBX),get(sp+8))
        elif address==0x412ec0: event(5,eax,*cpu.mem_read(ecx,eax))
        elif address in (0x4d4e50,0x4d4f10):
            n=get(sp+8);event(6 if address==0x4d4e50 else 7,n,*cpu.mem_read(get(sp+4),n))
        elif address==0x42a000: event(8)
        elif address in (0x40a5e0,0x40ab80):
            principal=address==0x40a5e0;attempt=state['alloc'];state['alloc']+=1
            event(9,int(principal))
            ptr=base+0x10000+attempt*0x1000 if not (state['scenario']//9%4==1 and attempt==0) and not (state['scenario']//9%4==2 and attempt%2==1) else 0
            machine.reg_write(UC_X86_REG_EAX,ptr)
        elif address==stop+16: state['id']+=1;machine.reg_write(UC_X86_REG_EAX,state['id']);event(10,state['id'])
        elif address==0x5046b9: machine.reg_write(UC_X86_REG_EAX,123456)
        elif address==0x490040:
            # Original Init uses ESI for mob, passed definition ID and respawn.
            from unicorn.x86_const import UC_X86_REG_ESI
            mob=machine.reg_read(UC_X86_REG_ESI);cpu.mem_write(mob+0x220,struct.pack('<H',get(sp+4)))
            event(11,identity(mob),get(sp+4), (get(sp+8)-ob) if get(sp+8)<groups else (get(sp+8)-groups+1000))
        elif address==0x416860:
            event(12,identity(eax),get(eax+0x50),get(eax+0x54),get(eax+0x58))
        elif address==0x504697:
            value=(state['random']*17+state['scenario']*13)%32768;state['random']+=1;event(13,value);machine.reg_write(UC_X86_REG_EAX,value)
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; ObeliskDestroy 0042a410, ObeliskRespawn 0042a6f0. Real group membership AddMob, x87 coordinate/terrain branches and packet assembly. External world, allocation, init, random/time/atomic/seed and transport observed ports. Name fills 31 bytes to avoid undefined stack padding.')
    for scenario in range(432):
        cpu.mem_write(base,bytes(0x1ff000));cpu.mem_write(stop+16,b'\xc2\x04\x00');state.update(trace=[],scenario=scenario,alloc=0,id=500,random=0)
        previous=scenario%3;requested=scenario//3%3;base_country=scenario//9%3;last=scenario//27%3
        put(map_,zone if scenario%17 else 0,8,bitmap);put(map_+0x78,42);put(map_+0x1ec,ob);cpu.mem_write(map_+0x1e4,bytes([scenario//2%2]))
        put(ob+4,previous,last,0,0xfedcba98,0x12345678,base_country)
        cpu.mem_write(ob+0x1c,struct.pack('<fff',2.5,3.5,4.5))
        for c in range(3): put(ob+0x34+c*4,0 if scenario%11==0 else definition)
        put(ob+0x48,9,1234);put(ob+0x64,2,groups,dead)
        cpu.mem_write(definition,struct.pack('<H',321)+b'ABCDEFGHIJKLMNOPQRSTUVWXYZ12345\0')
        put(dead+0xa8,definition);cpu.mem_write(dead+0x220,struct.pack('<H',654));cpu.mem_write(dead+0x50,struct.pack('<fff',1.25,2.5,3.75))
        cpu.mem_write(bitmap,bytes([0xaa if scenario//7%2 else 0]*8))
        for g,count in enumerate((3,1)):
            at=groups+g*0x58;cpu.mem_write(at,struct.pack('<ffffii',0.25+g*6,1.5,0.75+g*6,1.5,4,count))
            for c in range(3): put(at+0x24+c*4,0 if (scenario+g)%7==0 else definition)
            put(at+0x38,8,2345);put(at+0x54,base+0x9000+g*0x100)
            for n in range(count):
                ptr=base+0xa000+(g*3+n)*0x400;put(ptr+0x5c,700+g*3+n)
                put(base+0x9000+g*0x100+n*4,ptr if n%2==0 else 0)
        kc=(0 if scenario//5%2 else 30,-15 if scenario//6%2 else 0);put(0x5471c0,*kc)
        put(0x546f38,world);put(0x546f3c,manager)
        for c in range(3): put(world+0x208+c*0x84,scenario//4%2)
        mode=scenario//216
        args=[dead,requested,0,scenario//8%2] if mode==0 else [map_,0]
        put(stack,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,map_)
        try:
            cpu.emu_start(0x42a410 if mode==0 else 0x42a6f0,stop,count=100000)
        except Exception:
            import sys
            print(f"scenario={scenario} eip={cpu.reg_read(UC_X86_REG_EIP):x} trace={state['trace']}",file=sys.stderr)
            raise
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError("Original instruction limit exceeded")
        # Persisted and membership state, including stale slots when country definition is absent.
        event(99,get(ob+4),get(ob+8),get(ob+0x48),get(ob+0x4c),int(get(ob+0x6c)!=0))
        for g,count in enumerate((3,1)):
            at=groups+g*0x58;event(98,get(at+0x38),get(at+0x3c),*[int(get(base+0x9000+g*0x100+n*4)!=0) for n in range(count)])
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__': main()
