#!/usr/bin/env python3
"""Execute full UpdateMobDeath with real zone registry and recycling; requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,zones,grids,heap,manager,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
    for at,n in ((0,0x10000),(stack,0x10000),(zones,0x10000),(grids,0x100000),(heap,0x400000),(manager,0x10000),(stop,0x10000)):cpu.mem_map(at,n)
    context={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def ident(m):return 255 if not m else context['objects'].index(m)
    def zid(z):return 255 if not z else (z-zones)//0x1000
    def cell(z,i):return grids+z*0x20000+0x1000+i*292
    def order(z):
        head=get(zones+z*0x1000+0x13c);at=get(head+4);out=[]
        while at!=head:
            out.append(ident(at-0x6c));at=get(at+4)
            if len(out)>3:raise RuntimeError('Zone list cycle')
        return out
    def observer(tag,m):
        out=tag.encode()+bytes([ident(m)])
        for z in range(2):
            out+=struct.pack('<4I',get(zones+z*0x1000+0x15c),get(zones+z*0x1000+0xac),sum(get(cell(z,i)+0x4c) for i in range(context['cells']**2)),get(zones+z*0x1000+0x218))
        out+=bytes([zid(get(m+0x60))])+bytes(cpu.mem_read(m+0x64,8))+bytes(cpu.mem_read(m+0x284,4))+bytes(cpu.mem_read(m+0x290,4))+bytes(cpu.mem_read(m+0x27c,4))+bytes(cpu.mem_read(m+0x44,4))
        context['events']+=out
    for at,cleanup in ((0x503a3f,0),(0x503ba1,0),(0x5046b9,0),(0x4f55b0,0),(0x4208c0,12),(0x4b1010,4),(stop+0x100,4),(stop+0x200,8),(stop+0x300,4)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    for iat in (0x5210cc,0x5210d0):put(iat,stop+0x100)
    put(0x5210c8,stop+0x200);put(0x5210f8,stop+0x300);put(0x546f3c,manager)
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x503a3f:
            n=get(sp+4)
            if n==3380:
                context['events']+=b'N'+bytes([not context['fail']])
                if context['fail']:machine.reg_write(UC_X86_REG_EAX,0);return
            at=context['heap'];context['heap']+=(n+15)&~15;machine.mem_write(at,bytes(n));machine.reg_write(UC_X86_REG_EAX,at)
            if n==3380:context['objects'].append(at)
        elif address==0x5046b9:context['events']+=b'T';machine.reg_write(UC_X86_REG_EAX,context['time'])
        elif address==stop+0x300:
            at=get(sp+4);v=(get(at)+1)&0xffffffff;put(at,v);context['events']+=b'I';machine.reg_write(UC_X86_REG_EAX,v)
        elif address==0x4953f0:observer('C',machine.reg_read(UC_X86_REG_EDI)-0xac)
        elif address==0x492b90:observer('S',machine.reg_read(UC_X86_REG_EDI)-4)
        elif address==0x4f55b0:
            handle=get(sp+4);observer('L',context['objects'][(handle-0x70000000)//16])
        elif address==0x4208c0:
            z,p,n=struct.unpack('<3I',machine.mem_read(sp+4,12));context['events']+=b'V'+bytes([zid(z)])+struct.pack('<I',n)+bytes(machine.mem_read(p,n))
        elif address==0x4b1010:context['events']+=b'Q'+bytes([ident(get(sp+4))])
    cpu.hook_add(UC_HOOK_CODE,hook)
    def invalid(machine,access,address,size,value,_):raise RuntimeError(f'Invalid release access {address:08x} at {machine.reg_read(UC_X86_REG_EIP):08x}')
    cpu.hook_add(UC_HOOK_MEM_INVALID,invalid)
    def call(at,args=()):
        put(stack+0x8000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(at,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original zone release call did not return')
        return cpu.reg_read(UC_X86_REG_EAX)
    def snapshot():
        out=struct.pack('<3I',get(manager+8),get(manager+0x40),get(manager+0x44));first=get(manager+0x3c);free=[]
        while first:
            free.append(ident(first));nxt=get(first+0x4c);first=nxt-0x48 if nxt else 0
            if len(free)>3:raise RuntimeError('Pool cycle')
        out+=bytes([len(free),*free])
        for z in range(2):
            out+=bytes(cpu.mem_read(zones+z*0x1000+0x218,80));items=order(z);out+=bytes([len(items),*items])+struct.pack('<I',get(zones+z*0x1000+0xac))
            for i in range(context['cells']**2):out+=struct.pack('<I',get(cell(z,i)+0x4c))
        out+=bytes([len(context['objects'])])
        for m in context['objects']:
            out+=bytes([zid(get(m+0x60))])+bytes(cpu.mem_read(m+0x64,8))+bytes(cpu.mem_read(m+0x50,12))+bytes(cpu.mem_read(m+0xc88,12))+bytes(cpu.mem_read(m+0x5c,4))+bytes(cpu.mem_read(m+0x124,8))+bytes(cpu.mem_read(m+0x44,4))+bytes(cpu.mem_read(m+0x284,4))+bytes(cpu.mem_read(m+0x290,4))+bytes(cpu.mem_read(m+0x27c,4))+bytes(cpu.mem_read(m+0x2bc,8))
        return out
    print(f'# ps_game.exe SHA256 {digest}; UpdateMobDeath 00421a80, EnterMob 00416860, real constructor/pool/FSM release/tree erasure/cell clear and natural corpse unlink/recycling. Ports: heap/locks/time/atomic increment/Lua close and transport.')
    print('# S scenario extent counter; A fail time; C object config ID; E object zone x y z float bits; H object status death_time, D zone tick. Expected result index/BOOL BYTE, events length/events, pool/portal/index/cell/object snapshot. Cleanup events capture registry and raw binding state before each actual cleanup phase.')
    for scenario in range(128):
        extent=(81,160)[scenario%2];n=extent//40+1;counter=(1,0xfffffffe,0xffffffff)[scenario%3]
        context.update(heap=heap+0x10000,objects=[],events=b'',fail=False,time=0,cells=n)
        for at,size in ((zones,0x3000),(manager,0x1000),(heap,0x10000),(grids,0x50000)):cpu.mem_write(at,bytes(size))
        put(manager+8,counter);print('S',scenario,extent,counter,sep='\t')
        for z in range(2):
            at=zones+z*0x1000;grid=grids+z*0x20000;put(at+0x28,n,n,grid);put(grid,*(cell(z,iz*n) for iz in range(n)))
            head=heap+z*0x4000;put(at+0xa4,0,head,0);put(head,head,head,head);cpu.mem_write(head+20,b'\x01\x01');head+=0x100;put(at+0x13c,head,head);put(head,head,head)
            for i in range(n*n):
                head=heap+z*0x4000+0x1000+i*0x100;c=cell(z,i);put(c+0x2c,head);put(head+0x48,head+0x48,head+0x48)
            put(at+0x218,*(scenario*100+z*20+i+1 for i in range(20)))
        bits=lambda values:struct.unpack('<3I',struct.pack('<3f',*values))
        ops=[('A',0,1),('A',0,2),('A',0,3),('E',0,0,*bits((1.25,3,2.25))),('E',1,0,*bits((41.25,5,41.25))),('E',2,0,*bits((41.25,7,1.25))),('H',0,2,100),('H',1,2,100),('H',2,1,0),('D',0,100),('D',0,101),('D',0,101),('H',2,2,200),('D',0,200),('D',0,201),('D',0,0xffffffff)]
        for op in ops:
            context['events']=b'';result=255
            if op[0]=='A':
                context.update(fail=bool(op[1]),time=op[2]);cpu.reg_write(UC_X86_REG_EAX,manager);result=ident(call(0x40a5e0))
            elif op[0]=='C':
                _,i,config,mid=op;m=context['objects'][i];put(m+0x5c,mid);put(m+0x124,100+config,200+config);put(m+0x44,config&1);put(m+0x284,(config>>1)&1);put(m+0x290,300+config);put(m+0x27c,0x70000000+i*16 if config&4 else 0)
            elif op[0]=='H':
                _,i,status,deadline=op;m=context['objects'][i];put(m+0x124,status);put(m+0xcb0,deadline);put(m+0xc84,0)
            elif op[0]=='D':call(0x421a80,(zones+op[1]*0x1000,op[2]))
            elif op[0]=='E':
                _,i,z,*position=op;m=context['objects'][i];put(m+0x50,*position);cpu.reg_write(UC_X86_REG_EAX,m);result=call(0x416860,(zones+z*0x1000,))&255
            else:call(0x41c3b0,(zones+op[1]*0x1000,))
            out=bytes([result])+struct.pack('<I',len(context['events']))+context['events']+snapshot();print(*op,out.hex(),sep='\t')


if __name__=='__main__':main()
