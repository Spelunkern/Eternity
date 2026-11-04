#!/usr/bin/env python3
"""Run full original LoadObelisk; observe obelisk materialization from parsed records."""
import hashlib
import struct
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game,ROOT


def main():
    cpu,digest=load_game();stack,world,maps,infos,heap,file,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x34000000,0x38000000,0x10000000
    for at,n in ((0,0x10000),(stack,0x10000),(world,0x10000),(maps,0x100000),(infos,0x200000),(heap,0x3000000),(file,0x10000),(stop,0x10000)):cpu.mem_map(at,n)
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    for at in (0x505316,0x50b152,0x50b21f,0x5043e5,0x504303,0x503ae3,0x503ba1,0x504444):cpu.mem_write(at,b'\xc3')
    cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
    put(0x546f38,world);put(0xfcd2ac,4096,infos)
    for i in range(1,4096):cpu.mem_write(infos+i*268,struct.pack('<H',i));cpu.mem_write(infos+i*268+34,struct.pack('<H',0 if i%7==0 else 1));put(infos+i*268+48,1)
    context={}
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x5043e5:machine.reg_write(UC_X86_REG_EAX,file)
        elif address==0x504303:
            target,n,stream=struct.unpack('<3I',machine.mem_read(sp+4,12))
            if stream!=file:raise RuntimeError('Unexpected original stream')
            position=context['position'];text=context['text'];end=text.find(b'\n',position);end=len(text) if end<0 else end+1;end=min(end,position+n-1);line=text[position:end];context['position']=end
            if line:machine.mem_write(target,line+b'\0')
            if not line or (end==len(text) and not line.endswith(b'\n')):put(file+12,0x10)
            machine.reg_write(UC_X86_REG_EAX,target if line else 0)
        elif address==0x503ae3:
            n=get(sp+4);at=context['heap'];context['heap']+=(n+15)&~15;machine.mem_write(at,bytes(n));machine.reg_write(UC_X86_REG_EAX,at)
        elif address in (0x42733d,0x427349,0x427377):
            # Exact temporary-array reset, avoiding millions of REP callbacks.
            count=machine.reg_read(UC_X86_REG_ECX);at=machine.reg_read(UC_X86_REG_EDI);machine.mem_write(at,bytes(count*4));machine.reg_write(UC_X86_REG_EDI,at+count*4);machine.reg_write(UC_X86_REG_ECX,0);machine.reg_write(UC_X86_REG_EIP,address+2)
    for at in (0x5043e5,0x504303,0x503ae3,0x42733d,0x427349,0x427377):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    def invalid(machine,access,address,size,value,_):raise RuntimeError(f'Invalid parser access {address:08x} at {machine.reg_read(UC_X86_REG_EIP):08x}')
    cpu.hook_add(UC_HOOK_MEM_INVALID,invalid)
    def definition(pointer):return (pointer-infos)//268 if pointer else 0
    from unicorn.x86_const import UC_X86_REG_EBX,UC_X86_REG_ESI
    state={'capture':False}
    def event(*values):
        if state['capture']:state['trace'].extend(v&0xffffffff for v in values)
    for address,cleanup in ((0x4d4f10,8),(0x4d4d20,8),(0x4207f0,0),(0x412ec0,0)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210e4,stop+0x2000);cpu.mem_write(stop+0x2000,b'\xc3')
    def transport(machine,address,size,data):
        if not state['capture']:return
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+0x2000:event(6,state['tick']);machine.reg_write(UC_X86_REG_EAX,state['tick']);return
        if address in (0x4d4f10,0x4d4d20):n=get(sp+8);pointer=get(sp+4)
        else:n=machine.reg_read(UC_X86_REG_EAX);pointer=machine.reg_read(UC_X86_REG_ECX)
        if address==0x4d4f10:
            event(3,n,*cpu.mem_read(pointer,n))
            if state['reset'] and state['scenario']%11==0 and not state['dropped']:
                state['dropped']=True;put(maps,0)
        elif address==0x4d4d20:event(5,state['map'],n,*cpu.mem_read(pointer,n))
        elif address==0x412ec0:event(4,n,*cpu.mem_read(pointer,n))
        else:
            zone=machine.reg_read(UC_X86_REG_EBX);mi=0 if zone==world+0x8000 else 1
            if zone!=world+0x8000+mi*0x1000:raise RuntimeError('Unexpected original primary zone route')
            event(2,mi,n,*cpu.mem_read(pointer,n))
    for address in (0x4d4f10,0x4d4d20,0x4207f0,0x412ec0,stop+0x2000):cpu.hook_add(UC_HOOK_CODE,transport,begin=address,end=address)
    def call(address,*args):
        put(stack+0xf000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.emu_start(address,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original map composition did not return')
    def snapshot():
        event(90,get(0x541b0c),get(0x5471bc),get(0x5471c0),get(0x5471c4))
        for mi in range(2):
            for j in range(3):event(get(maps+0x20000+mi*3*48+j*48+20))
        for i in range(get(0x5471c8)):event(*cpu.mem_read(get(0x5471cc)+i*0x84+24,2))
        for mi in range(2):
            for row in range(2):
                at=get(maps+mi*496+0x1ec)+row*112;event(get(at+4),get(at+8),get(at+0x4c))
        event(*(cpu.mem_read(infos+i*268+0x7c,1)[0] for i in (1,2,3)))
    print(f'# ps_game.exe SHA256 {digest}; real LoadObelisk 00427300, SetKeepObelisk 00413bd0, SendObeliskList 0042b040, ObeliskReset 0042aa40, UpdatePortalLink 0042a000 and world DB list00413b90. Native comparison uses actual ServerMaps loading/retained portal country/position, full catalog and required primary-zone routing. Initial map definitions/SQL records are explicit fixture inputs. Ports:file/heap/locks/CRT/transports/tick.')
    print('# trace:2 map(index,length,bytes);3 DB(length,bytes);4 world(length,bytes);5 user(map,length,bytes);6 tick;90 snapshot(nextID,total,KC0,KC1,6 actual portal countries,4 open flags,4 record country/last/next triples,3 raw template flags).')
    for scenario in range(64):
        text=''
        for mi in range(2):
            text+=f'[zone_{mi}]\n'
            for row in range(2):text+=f'[Obelisk_{row+1}]=({row+1.25},2.5,{row+3.75}),(1,2,3),{1+(scenario+mi+row)%3}\n'
        text+='[Portal_10,0,1001]\n[light]=0,Obelisk_1,1,Obelisk_2\n[fury]=0,Obelisk_2\n[Portal_11,1,1002]\n[light]=1,Obelisk_1\n[fury]=0,Obelisk_2,1,Obelisk_2\n'
        context.update(text=text.encode(),position=0,heap=heap);cpu.mem_write(file,bytes(32));cpu.mem_write(maps,bytes(0x30000));put(world+8,2,maps)
        for mi in range(2):
            at=maps+mi*496;put(at,world+0x8000+mi*0x1000 if scenario%(3+mi) else 0,81);portals=maps+0x20000+mi*3*48;put(at+80,3,portals)
            for j,identifier in enumerate((1001,1001,1002)):
                put(portals+j*48,identifier);cpu.mem_write(portals+j*48+4,struct.pack('<fff',20.25+j*10,99,30.5+j*10));put(portals+j*48+20,identifier)
        for i in (1,2,3):cpu.mem_write(infos+i*268+0x7c,b'\0')
        put(0x541b0c,0xfffffffd if scenario%2 else 0x123);put(0x5471bc,0);put(0xfcd2cc,0,0);put(0xfcd25c,1000);put(0xfd4e5c,-45)
        state.update(scenario=scenario,trace=[],capture=False,reset=False,dropped=False,tick=(0xffffff00 if scenario%2 else 100)+scenario)
        cpu.reg_write(UC_X86_REG_FPCW,0x27f);call(0x427300);state['capture']=True;snapshot()
        cpu.reg_write(UC_X86_REG_EAX,world);call(0x413bd0,0x1000000a+scenario%2,scenario//2%5-1,0);snapshot()
        for mi in range(2):state['map']=mi;cpu.reg_write(UC_X86_REG_EBX,maps+mi*496);call(0x42b040,world+0x7000)
        cpu.reg_write(UC_X86_REG_EAX,world);call(0x413bd0,(scenario//5%2)*65536+scenario//7%2+1,(scenario%3)|((scenario//3%3+1)<<4),0)
        state['reset']=True;cpu.reg_write(UC_X86_REG_ESI,maps+scenario//9%2*496);call(0x42aa40);state['reset']=False;snapshot()
        for mi in range(2):state['map']=mi;cpu.reg_write(UC_X86_REG_EBX,maps+mi*496);call(0x42b040,world+0x7000)
        call(0x413b90,world);snapshot();print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__':main()
