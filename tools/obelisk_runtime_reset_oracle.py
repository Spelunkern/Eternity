#!/usr/bin/env python3
"""Run original ObeliskReset with observed removal ports and real nested Destroy."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESI,
                              UC_X86_REG_ESP, UC_X86_REG_EIP)
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x200000)
    map_,ob,groups,definition,zone=[base+x for x in (0,0x1000,0x2000,0x4000,0x6000)]
    stack,stop=base+0x1e0000,base+0x1f0000
    state={}
    def put(at,*values): cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def get(at): return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def event(*values): state['trace'].extend(v&0xffffffff for v in values)
    ports={0x41e910:4,0x42b200:0,0x4d4e50:8,0x4d4f10:8,0x42a000:0,0x505316:0}
    for address,cleanup in ports.items(): cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210e4,stop+16)
    def hook(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP);eax=machine.reg_read(UC_X86_REG_EAX)
        if address==0x42a410:
            index=get(sp+12)
            if get(sp+8)!=get(ob+index*0x70+0x18) or get(sp+16)!=1:
                raise RuntimeError('Removal trampoline passed stale country/reset arguments')
        elif address==stop+16:
            event(15,state['now']);machine.reg_write(UC_X86_REG_EAX,state['now'])
        elif address==0x41e910:
            mob_id=get(sp+4);event(1,mob_id,cpu.mem_read(map_+0x1e4,1)[0])
            mode=state['scenario']//9%3
            if 100<=mob_id<103 and mode:
                oi=mob_id-100;at=ob+oi*0x70
                # Explicit removal-provider projection, not a reconstruction of
                # CZone::MobRemoveByID. Its nested Destroy is executed unmodified.
                put(at+0x48,get(at+0x48)-1,0x12340000+mob_id)
                if mode==2:
                    put(at+0x18,(get(at+0x18)+1)%3);put(at+0x10,get(at+0x10)^0x55aa55aa);put(at+8,-1)
                trampoline=stop+0x100+oi*0x100
                # Preserve the reset caller's callee-saved registers around the
                # real Destroy(this=map, mob, live base country, index, reset=1).
                code=b'\x53\x55\x56\x57\x6a\x01\x6a'+bytes([oi])
                code+=b'\xff\x35'+struct.pack('<I',at+0x18)
                code+=b'\x68'+struct.pack('<I',base+0x10000+oi*0x1000)
                code+=b'\xb9'+struct.pack('<I',map_)+b'\xb8'+struct.pack('<I',0x42a410)+b'\xff\xd0'
                code+=b'\x5f\x5e\x5d\x5b\xc2\x04\x00'
                cpu.mem_write(trampoline,code)
                # Reused emulator code pages must not retain a previous case's
                # translated trampoline. Read live country from memory as well.
                cpu.ctl_remove_cache(trampoline,trampoline+len(code))
                machine.reg_write(UC_X86_REG_EIP,trampoline)
        elif address==0x42b200:
            at=ob+eax*0x70;event(2,eax,cpu.mem_read(map_+0x1e4,1)[0],get(at+4),get(at+8),get(at+0x48),get(at+0x4c))
        elif address in (0x4d4e50,0x4d4f10):
            length=get(sp+8);event(6 if address==0x4d4e50 else 7,length,*cpu.mem_read(get(sp+4),length))
        elif address==0x42a000: event(8,cpu.mem_read(map_+0x1e4,1)[0])
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; ObeliskReset 0042aa40 and real nested ObeliskDestroy 0042a410. Clock, removal and transports observed. Removal provider variants: inert, counter/deadline update + nested Destroy, live base/last/change-time updates + nested Destroy. No claim to execute CZone::MobRemoveByID body.')
    print('# scenario then uint32 trace: 15=clock(tick); 1=remove(id,reset flag); 2=change(index,flag,country,last,current,deadline); 6/7=log/DB(length,bytes); 8=portal(flag); 97=map flag; 99=principal(country,last,base,change,current,deadline,present); 98=spawn(current,deadline,member flags).')
    for scenario in range(648):
        cpu.mem_write(base,bytes(0x1ff000));cpu.mem_write(stop+16,b'\xc3')
        count=scenario%4;now=(0,100,0x7fffffff,0xfff6d83f,0xffffffff)[scenario//7%5]
        state.update(scenario=scenario,now=now,trace=[])
        put(map_,zone if scenario%17 else 0);put(map_+0x78,42);put(map_+0x1e8,count,ob)
        cpu.mem_write(map_+0x1e4,bytes([scenario//5%2]))
        for oi in range(count):
            at=ob+oi*0x70;mob=base+0x10000+oi*0x1000;spawn=groups+oi*0x58
            country=(scenario//4+oi)%3;base_country=(scenario//12+oi)%3;last=(-1,0,1,2)[(scenario//36+oi)%4]
            put(at+4,country,last,0,0xfedcba98^oi,0x12345678+oi,base_country)
            put(at+0x48,(-1,0,1,2)[(scenario//11+oi)%4],0xf0000000+oi)
            put(at+0x64,1,spawn,mob if (scenario//2+oi)%3 else 0)
            cpu.mem_write(definition+oi*0x100,struct.pack('<H',321+oi)+b'ABCDEFGHIJKLMNOPQRSTUVWXYZ12345\0')
            put(mob+0x5c,100+oi);put(mob+0xa8,definition+oi*0x100)
            cpu.mem_write(mob+0x220,struct.pack('<H',654+oi));cpu.mem_write(mob+0x50,struct.pack('<fff',1.25+oi,2.5,3.75))
            members=base+0x9000+oi*0x100;put(spawn+0x14,2);put(spawn+0x38,1,2345);put(spawn+0x54,members)
            for n in range(2):
                member=base+0x20000+(oi*2+n)*0x1000;put(member+0x5c,700+oi*2+n)
                put(members+n*4,member if (scenario+oi+n)%2==0 else 0)
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ESI,map_)
        cpu.emu_start(0x42aa40,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('Original instruction limit exceeded')
        event(97,cpu.mem_read(map_+0x1e4,1)[0])
        for oi in range(count):
            at=ob+oi*0x70;spawn=groups+oi*0x58;members=get(spawn+0x54)
            event(99,get(at+4),get(at+8),get(at+0x18),get(at+0x10),get(at+0x48),get(at+0x4c),int(get(at+0x6c)!=0))
            event(98,get(spawn+0x38),get(spawn+0x3c),*[int(get(members+n*4)!=0) for n in range(2)])
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__': main()
