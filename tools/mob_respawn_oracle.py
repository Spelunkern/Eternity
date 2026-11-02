#!/usr/bin/env python3
"""Execute periodic UpdateMobRespawn through real pool, initialization and zone insertion."""
import struct
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,zone,grid,heap,data,manager,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x34000000,0x35000000,0x10000000
    for at,n in ((0,0x10000),(stack,0x10000),(zone,0x10000),(grid,0x100000),(heap,0x800000),(data,0x100000),(manager,0x10000),(stop,0x10000)):cpu.mem_map(at,n)
    context={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def event(tag,*values):context['events']+=tag.encode()+struct.pack('<'+'I'*len(values),*values)
    for at,cleanup in ((0x503a3f,0),(0x503ba1,0),(0x5046b9,0),(0x504697,0),(0x4a9130,4),(0x4b4c20,0),(0x4208c0,12),(0x4b1010,4),(0x41b870,8),(0x429da0,4),(stop+0x100,4),(stop+0x200,8),(stop+0x300,4),(stop+0x400,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    for iat in (0x5210cc,0x5210d0):put(iat,stop+0x100)
    put(0x5210c8,stop+0x200);put(0x5210f8,stop+0x300);put(0x5210e4,stop+0x400);put(0x546f3c,manager)
    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x503a3f:
            n=get(sp+4)
            if n==3380:
                attempt=context['attempts'];context['attempts']+=1;mode=context['fail'];failed=mode==1 or (mode==2 and attempt%3==0) or (mode==3 and attempt%2==0);event('N',int(not failed))
                if failed:machine.reg_write(UC_X86_REG_EAX,0);return
            at=context['heap'];context['heap']+=(n+15)&~15;machine.mem_write(at,bytes(n));machine.reg_write(UC_X86_REG_EAX,at)
            if n==3380:context['objects'].append(at)
        elif address==0x5046b9:event('T',context['time']);machine.reg_write(UC_X86_REG_EAX,context['time'])
        elif address==stop+0x300:
            at=get(sp+4);v=(get(at)+1)&0xffffffff;put(at,v);event('I',v);machine.reg_write(UC_X86_REG_EAX,v)
        elif address==stop+0x400:
            v=(context['tick']+context['ticks']*11)&0xffffffff;context['ticks']+=1;event('K',v);machine.reg_write(UC_X86_REG_EAX,v)
        elif address==0x504697:
            context['random']=(context['random']*214013+2531011)&0xffffffff;v=(context['random']>>16)&32767;event('R',v);machine.reg_write(UC_X86_REG_EAX,v)
        elif address==0x4a9130:
            m=machine.reg_read(UC_X86_REG_ECX);event('A',get(sp+4));put(m+0x124,17);put(m+0x2b0,0x1234);put(m+0x298,33);put(m+0x2b8,99);machine.reg_write(UC_X86_REG_EAX,context['ai_ok'])
        elif address==0x4b4c20:
            m=machine.reg_read(UC_X86_REG_EAX);event('U');put(m+0x284,1);put(m+0x290,1);put(m+0x2b0,0x76543210);put(m+0x298,5);put(m+0x2b8,99)
        elif address==0x4208c0:
            owner,packet,length=struct.unpack('<3I',machine.mem_read(sp+4,12))
            if owner!=zone:raise RuntimeError('Unexpected population view owner')
            event('V',length);context['events']+=bytes(machine.mem_read(packet,length))
            mob=context['objects'][-1];context['events']+=bytes(machine.mem_read(get(mob+0xc84)+8,8))
        elif address==0x4b1010:event('L',get(get(sp+4)+0x5c))
        elif address==0x429da0:event('O',get(sp+4),get(zone+0x15c))
        elif address==0x41b870:
            if get(sp+4)!=zone:raise RuntimeError('Wrong linked update owner')
            event('B',get(sp+8),get(zone+0x15c))
    cpu.hook_add(UC_HOOK_CODE,hook)
    def invalid(machine,access,address,size,value,_):raise RuntimeError(f'Invalid population access {address:08x} at {machine.reg_read(UC_X86_REG_EIP):08x}')
    cpu.hook_add(UC_HOOK_MEM_INVALID,invalid)
    def call(at,now):
        put(stack+0xf000,stop,zone,now);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EDI,zone);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        cpu.emu_start(at,stop,count=20000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original population did not return')
        return cpu.reg_read(UC_X86_REG_EAX)&255
    def bits(values):return struct.unpack('<3I',struct.pack('<3f',*values))
    def snapshot():
        out=struct.pack('<5I',get(manager+8),get(manager+0x40),get(zone+0x15c),context['random'],context['ticks'])
        for r in context['spawns']:out+=bytes(cpu.mem_read(r+8,8))
        out+=struct.pack('<I',len(context['objects']))
        for m in context['objects']:
            out+=struct.pack('<I',context['infos'].index(get(m+0xa8)))+bytes(cpu.mem_read(m+0x5c,4))+bytes(cpu.mem_read(m+0x220,2))
            out+=bytes(cpu.mem_read(m+0x50,12))+bytes(cpu.mem_read(m+0xc88,12))+bytes(cpu.mem_read(m+0x64,8))
            out+=struct.pack('<I',context['spawns'].index(get(m+0xc84)))+bytes(cpu.mem_read(m+0x124,4))+bytes(cpu.mem_read(m+0x224,8))
            out+=bytes(cpu.mem_read(m+0xcc4,4))+bytes(cpu.mem_read(m+0xcc8,4))+bytes(cpu.mem_read(m+0x2bc,8))+bytes(cpu.mem_read(m+0xc80,4))
            out+=bytes(cpu.mem_read(m+0x284,4))+bytes(cpu.mem_read(m+0x2b0,4))+bytes(cpu.mem_read(m+0x298,4))+bytes(cpu.mem_read(m+0x2b8,4))
        # Verify actual cell/global list counts and each real ID tree lookup externally in other fixtures.
        total=sum(get(grid+0x1000+i*292+0x4c) for i in range(context['cells']**2))
        if total!=len(context['objects']) or get(zone+0x15c)!=total:raise RuntimeError('Original population membership counts differ')
        return out
    print(f'# ps_game.exe SHA256 {digest}; UpdateMobRespawn 0041d240; real Alloc/CMob constructor, CMob::Init/attack, EnterMob/global index/cell lists and inline random seeding. Ports: allocator failure, locks, time/tick/random, AI/Lua, obelisk/linked updates and transport.')
    print('# S scenario map_size initial_counter fail_mode random_seed tick time ai_ok; D id level ai selector; A minimum float bits delta_x delta_z; P type count current next; U now expected hex. Nine sustained ticks per scenario; expected result/events then counters, respawns and object snapshots.')
    for scenario in range(256):
        extent=(81,160)[scenario%2];cells=extent//40+1;counter=(1,0xfffffffe,0xffffffff)[scenario%3];randseed=(scenario*0x112233+1)&0xffffffff;tick=(0xfffffff0+scenario*91)&0xffffffff;timestamp=(scenario*0x1234567)&0xffffffff
        context.update(heap=heap+0x10000,objects=[],spawns=[],infos=[],events=b'',attempts=0,fail=scenario%4,time=timestamp,tick=tick,ticks=0,random=randseed,ai_ok=scenario%2,cells=cells)
        cpu.mem_write(zone,bytes(0x1000));cpu.mem_write(manager,bytes(0x1000));cpu.mem_write(data,bytes(0x10000));cpu.mem_write(grid,bytes(0x10000));cpu.mem_write(heap,bytes(0x10000));put(manager+8,counter)
        map_object=data;tiles=data+0x1000;area_table=data+0x2000;put(map_object+4,extent,tiles);put(zone+0x24,map_object,cells,cells,grid)
        for z in range(extent):
            for x in range(extent):
                if (x+z+scenario)%7==0:
                    i=z*extent+x;cpu.mem_write(tiles+i//8,bytes([cpu.mem_read(tiles+i//8,1)[0]|1<<(i%8)]))
        put(grid,*(grid+0x1000+z*cells*292 for z in range(cells)))
        for z in range(cells):
            for x in range(cells):
                c=grid+0x1000+(z*cells+x)*292;head=heap+0x1000+(z*cells+x)*0x100;put(c+0x2c,head);put(head+0x48,head+0x48,head+0x48)
        head=heap;put(zone+0xa4,0,head,0);put(head,head,head,head);cpu.mem_write(head+20,b'\x01\x01');head=heap+0x100;put(zone+0x13c,head,head);put(head,head,head)
        create_type=(0,1,3,-1)[(scenario//4)%4];put(map_object+0x1a4,create_type)
        print('S',scenario,extent,counter,scenario%4,randseed,tick,timestamp,scenario%2,create_type,sep='\t')
        for i in range(3):
            info=data+0x8000+i*268;context['infos'].append(info);level=(38,1,0)[i];ai=(0,9,10,99,100)[(scenario+i)%5]
            cpu.mem_write(info,struct.pack('<H',i+1));cpu.mem_write(info+34,struct.pack('<H',level));cpu.mem_write(info+38,bytes([ai]));put(info+48,2765);cpu.mem_write(info+52,struct.pack('<9H',874,1578,60,40,53,0,4,38,31));put(info+112,13)
            selector=(scenario+i)%18;timers=(5000,30000,60000,180000,300000,600000,900000,1800000,3600000,14400000,43200000,86400000,259200000,7200000,0,604800000);put(info+108,timers[selector if selector<16 else 0])
            print('D',i+1,level,ai,selector,sep='\t')
        put(zone+0x268,3,area_table)
        values=(-0.25,0.25,39.75,80.75,159.75,float('nan'),float('inf'),16777216.0)
        for i in range(3):
            area=area_table+i*40;minimum=(values[(scenario//4+i)%len(values)],-3.5+i,values[(scenario//9+i)%5]);dx=(1,2,40,200)[(scenario+i)%4];dz=(1,2,40,200)[(scenario//3+i)%4];respawns=data+0x3000+i*0x200
            cpu.mem_write(area,struct.pack('<6f',*minimum,200,3,200));put(area+24,dx,dz,3,respawns);print('A',*bits(minimum),dx,dz,sep='\t')
            for j in range(3):
                r=respawns+j*36;mid=(1,2,3,-1,65536)[(scenario+i+j)%5];count=(-2,0,1,3,5)[(scenario//2+i+j)%5];current=(-3,-1,0,count,count+1)[(scenario//5+i+j)%5];deadline=(0,1,0xffffffff,5000)[(scenario//11+i+j)%4]
                put(r,mid,count,current,deadline,context['infos'][mid-1] if mid in (1,2) else 0);context['spawns'].append(r);print('P',mid,count,current,deadline,sep='\t')
        for now in (0,1,4999,5000,0xfffffffe,0xffffffff,0,180000,180001):
            context['events']=b'';call(0x41d240,now);out=b'\x01'+struct.pack('<I',len(context['events']))+context['events']+snapshot();print('U',now,out.hex(),sep='\t')


if __name__=='__main__':main()
