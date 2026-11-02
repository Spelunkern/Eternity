#!/usr/bin/env python3
"""Execute linked transitions/update through real pool, initialization and zone insertion."""
import struct
import math
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,zone,grid,heap,data,manager,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x34000000,0x35000000,0x10000000
    for at,n in ((0,0x10000),(stack,0x10000),(zone,0x10000),(grid,0x100000),(heap,0x800000),(data,0x100000),(manager,0x10000),(stop,0x10000)):cpu.mem_map(at,n)
    context={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def event(tag,*values):context['events']+=tag.encode()+struct.pack('<'+'I'*len(values),*values)
    for at,cleanup in ((0x503a3f,0),(0x503ba1,0),(0x5046b9,0),(0x504697,0),(0x4a9130,4),(0x4b4c20,0),(0x4208c0,12),(0x4b1010,4),(0x41e910,4),(stop+0x100,4),(stop+0x200,8),(stop+0x300,4),(stop+0x400,0)):
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
        elif address==0x4b1010:event('L',get(get(sp+4)+0x5c))
        elif address in (0x41e910,0x417a10):
            if address==0x41e910:event('M',get(sp+4))
            else:event('P',machine.reg_read(UC_X86_REG_EDI),machine.reg_read(UC_X86_REG_EBX),get(data+0x6000+20))
            for group in context['groups']:context['events']+=bytes(machine.mem_read(group+0x26c8,4))
            for spawn in context['spawns']:
                context['events']+=bytes(machine.mem_read(spawn+0xa0,8))+struct.pack('<16I',*(canonical(get(spawn+0xbc+i*4)) for i in range(16)))

    cpu.hook_add(UC_HOOK_CODE,hook)
    def invalid(machine,access,address,size,value,_):raise RuntimeError(f'Invalid population access {address:08x} at {machine.reg_read(UC_X86_REG_EIP):08x}')
    cpu.hook_add(UC_HOOK_MEM_INVALID,invalid)
    def call(at,args=(zone,),regs=()):
        put(stack+0xf000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EDI,zone);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        for reg,value in regs:cpu.reg_write(reg,value)
        cpu.emu_start(at,stop,count=20000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original population did not return')
        return cpu.reg_read(UC_X86_REG_EAX)&255
    def bits(values):return struct.unpack('<3I',struct.pack('<3f',*values))
    def canonical(pointer):
        if data+0x7000<=pointer<data+0x7300:return 0x70000001+(pointer-data-0x7000)//256
        if pointer in context['objects']:return context['objects'].index(pointer)+1
        return pointer
    def snapshot():
        out=struct.pack('<5I',get(manager+8),get(manager+0x40),get(zone+0x15c),context['random'],context['ticks'])
        for group in context['groups']:out+=bytes(cpu.mem_read(group+0x26c8,4))
        for spawn in context['spawns']:
            out+=bytes(cpu.mem_read(spawn+0xa0,8))
            out+=struct.pack('<16I',*(canonical(get(spawn+0xbc+i*4)) for i in range(16)))
        out+=struct.pack('<I',len(context['objects']))
        for m in context['objects']:
            out+=struct.pack('<I',context['infos'].index(get(m+0xa8)))+bytes(cpu.mem_read(m+0x5c,4))+bytes(cpu.mem_read(m+0x220,2))
            out+=bytes(cpu.mem_read(m+0x50,12))+bytes(cpu.mem_read(m+0xc88,12))+bytes(cpu.mem_read(m+0x64,8))
            out+=struct.pack('<I',context['spawns'].index(get(m+0xc84)-0x98))+bytes(cpu.mem_read(m+0x124,4))+bytes(cpu.mem_read(m+0x224,8))
            out+=bytes(cpu.mem_read(m+0xcc4,4))+bytes(cpu.mem_read(m+0xcc8,4))+bytes(cpu.mem_read(m+0x2bc,8))+bytes(cpu.mem_read(m+0xc80,4))
            out+=bytes(cpu.mem_read(m+0x284,4))+bytes(cpu.mem_read(m+0x2b0,4))+bytes(cpu.mem_read(m+0x298,4))+bytes(cpu.mem_read(m+0x2b8,4))
            out+=bytes([get(m+0x60)==zone])
        out+=bytes(cpu.mem_read(data+0x6000+20,4))+bytes(cpu.mem_read(zone+0x218,80))
        return out
    print(f'# ps_game.exe SHA256 {digest}; BothMobDestroy 0041bad0, BothMobRespawn 0041be50, UpdateBothMobRespawn 0041b870; real Alloc/CMob constructor, CMob::Init/attack, EnterMob/global index/cell lists and inline random seeding. Ports: allocator failure, locks, time/tick/random, AI/Lua, ID removal boundary and transport. Actual instance portal updates and drop-random seeding execute.')
    print('# S scenario size counter allocation_failure random_seed tick time ai_ok create_type; D id level ai respawn_selector; G support_count change_count stage portal_bound; P center/radius bits radius_twice count first_definition_slot random min/random minutes. R group, X group, U tick, C spawn_ordinal current deadline; expected event and state bytes. Removal is an observer-only port; dummy members use canonical 70000001..3.')
    for scenario in range(128):
        extent=(81,160)[scenario%2];cells=extent//40+1;counter=(1,0xfffffffe,0xffffffff)[scenario%3];randseed=(scenario*0x112233+1)&0xffffffff;tick=(0xfffffff0+scenario*91)&0xffffffff;timestamp=(scenario*0x1234567)&0xffffffff
        context.update(heap=heap+0x10000,objects=[],spawns=[],groups=[],infos=[],events=b'',attempts=0,fail=scenario%4,time=timestamp,tick=tick,ticks=0,random=randseed,ai_ok=scenario%2,cells=cells)
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
        print('S',scenario,extent,counter,scenario%4,randseed,tick,timestamp,scenario%2,scenario%3,sep='\t')
        for i in range(3):
            info=data+0x8000+i*268;context['infos'].append(info);level=(38,1,0)[i];ai=(0,9,10,99,100)[(scenario+i)%5]
            cpu.mem_write(info,struct.pack('<H',i+1));cpu.mem_write(info+34,struct.pack('<H',level));cpu.mem_write(info+38,bytes([ai]));put(info+48,2765);cpu.mem_write(info+52,struct.pack('<9H',874,1578,60,40,53,0,4,38,31));put(info+112,13);put(info+108,(0,5000,180000)[i])
            print('D',i+1,level,ai,(14,0,3)[i],sep='\t')
        put(zone+0x270,2,data+0x10000);put(map_object+0x1a4,scenario%3);put(data+0x6000,1010);put(data+0x6000+20,2)
        for i in range(3):put(data+0x7000+i*256+0x5c,0xf0000000+i)
        values=(-0.25,0.25,39.75,80.75,159.75,float('nan'),float('inf'))
        for group_index in range(2):
            group=data+0x10000+group_index*9932;context['groups'].append(group);change=(0,1,2,15)[(scenario//5)%4];stage=(scenario//7+group_index)%max(change,1);portal=int((scenario+group_index)%3!=0);put(group+4,change);put(group+20,data+0x6000 if portal else 0);put(group+24,2);put(group+0x26c8,stage);print('G',2,change,stage,portal,sep='\t')
            for spawn_index in range(3):
                spawn=group+(28 if spawn_index==0 else 328+(spawn_index-1)*300);context['spawns'].append(spawn)
                center=(values[(scenario//4+spawn_index+group_index)%len(values)],-3.5+spawn_index,values[(scenario//9+spawn_index)%5]);radius=(0.25,1,20,40,16777216.0)[(scenario+spawn_index)%5];radius2=(1,2,40,80,33554432)[(scenario+spawn_index)%5];n=(0,1,3,16)[(scenario//3+spawn_index)%4]
                if spawn_index==0 and not math.isfinite(center[0]):center=(1.25,center[1],center[2]) # Avoid original unchecked negative cell indexing on NaN/Inf entry.
                info_slot=(scenario+spawn_index+group_index)%4;info=context['infos'][info_slot] if info_slot<2 else 0;randomized=int(spawn_index==0 and (scenario+group_index)%3==0);minimum=(0,1,0xffffffff)[scenario%3];extra=(0,1,5)[(scenario//3)%3]
                cpu.mem_write(spawn,struct.pack('<4f',*center,radius));put(spawn+16,radius2,n);put(spawn+88,info);put(spawn+0x98,info_slot+1,n,0 if spawn_index==0 else n-1,0,info,0,0,1,spawn)
                for stage_index in range(16):
                    slot=(scenario+spawn_index+group_index+stage_index)%4;put(spawn+88+stage_index*4,context['infos'][slot] if slot<2 else 0)
                put(spawn+0xfc,randomized,minimum,extra);put(spawn+0xbc,*(data+0x7000+(i%3)*256 if i<n and i%2==0 else 0 for i in range(16)))
                for center_index in range(3):cpu.mem_write(spawn+264+center_index*12,struct.pack('<3f',1.25+center_index*40,9+center_index,1.25))
                print('P',*bits(center),struct.unpack('<I',struct.pack('<f',radius))[0],radius2,n,info_slot,randomized,minimum,extra,sep='\t')
        for op in (('R',0),('R',1),('U',0),('U',100),('U',0xfffffff0),('X',0),('R',0),('X',0),('R',0),('C',0,0,0),('C',1,-1,0),('U',0),('U',0xfffffff0),('U',0),('X',1),('R',1)):
            context['events']=b''
            if op[0]=='R':call(0x41be50,(zone,),((UC_X86_REG_EAX,op[1]),))
            elif op[0]=='X':call(0x41bad0,(zone,),((UC_X86_REG_EDX,op[1]),))
            elif op[0]=='U':call(0x41b870,(zone,op[1]))
            else:put(context['spawns'][op[1]]+0xa0,op[2],op[3])
            out=b'\x01'+struct.pack('<I',len(context['events']))+context['events']+snapshot();print(*op,out.hex(),sep='\t')



if __name__=='__main__':main()
