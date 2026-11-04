#!/usr/bin/env python3
"""Execute original portal coordination and publication with observed transports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    links,portals,maps,obs=base,base+0x1000,base+0x2000,base+0x4000
    stack,stop=base+0xe0000,base+0xf0000;state={}
    def put(at,*values): cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def get(at): return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def event(*words): state['trace'].extend(v&0xffffffff for v in words)
    for address,cleanup in ((0x4d4f10,8),(0x4207f0,0),(0x412ec0,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    def hook(machine,address,size,data):
        if address not in (0x4d4f10,0x4207f0,0x412ec0): return
        sp=machine.reg_read(UC_X86_REG_ESP)
        pointer=get(sp+4) if address==0x4d4f10 else machine.reg_read(UC_X86_REG_ECX)
        length=get(sp+8) if address==0x4d4f10 else machine.reg_read(UC_X86_REG_EAX)
        event({0x4d4f10:1,0x4207f0:2,0x412ec0:3}[address],length,*cpu.mem_read(pointer,length))
        state['sends']+=1
        if state['scenario']%7==0 and state['sends']==1:
            # Reentrant service changes target countries and later ownership.
            for i in range(state['count']): put(portals+i*0x40+0x14,(i+state['scenario'])%5-1)
            for i in range(8): put(obs+i*0x70+8,(i+1)%3)
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; UpdatePortalLink 0042a000 and actual SendPortalLinkChange 0042b2c0. Only DB/map/world transports are observed. Initial structures are explicit projections.')
    print('# scenario; trace words 1=DB/2=map/3=world then length and bytes, 99=final(country,open0,open1). First transport mutates countries/last-destroy fields when scenario%7==0. Country-one scan starts at flat pLink[4], as original instructions 0042a06f-0042a07b.')
    for scenario in range(1024):
        cpu.mem_write(base,bytes(0xff000));count=(0,1,2,3,4,6,11,12)[scenario%8]
        state.update(scenario=scenario,count=count,trace=[],sends=0)
        put(0x5471c8,count,links)
        for j in range(8): put(obs+j*0x70+4,99,(scenario//8+j)%4-1)
        for i in range(count):
            at=links+i*0x84;put(at,0xf1230000+i,0x10000000+i,0,0,maps+i*0x1f0,0 if (scenario//9+i)%7==0 else portals+i*0x40)
            put(portals+i*0x40,0xabc00000+i);put(portals+i*0x40+0x14,(scenario//3+i)%6-1)
            put(maps+i*0x1f0,0 if (scenario//5+i)%3==0 else 0x1234)
            cpu.mem_write(at+0x18,bytes([(scenario//2+i)%2,(scenario//4+i)%2]))
            put(at+0x1c,(scenario//11+i)%6-1,(scenario//13+i)%6-1)
            for j in range(8):
                mode=scenario//17%5
                idx=(j+i)%8
                if mode in (1,2): put(obs+idx*0x70+8,mode-1)
                put(at+0x2c+j*12,0 if (scenario//19+i+j)%7==0 else obs+idx*0x70)
        # Dedicated all-open transitions force both first-pass and neighbor
        # second-pass publications for the same portal, in both directions.
        forced=scenario//64%4
        if forced in (2,3):
            side=0 if forced==3 else 1
            for j in range(8): put(obs+j*0x70+8,side)
            for i in range(count):
                at=links+i*0x84;put(at+0x14,portals+i*0x40)
                put(portals+i*0x40+0x14,3);cpu.mem_write(at+0x18,b'\x00\x00')
                put(at+0x1c,2 if side==0 else 0,2 if side==1 else 0)
                for j in range(8): put(at+0x2c+j*12,obs+j*0x70)
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.emu_start(0x42a000,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('Original instruction limit exceeded')
        for i in range(count): event(99,get(portals+i*0x40+0x14),*cpu.mem_read(links+i*0x84+0x18,2))
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__': main()
