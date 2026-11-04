#!/usr/bin/env python3
"""Real zone/index/cell removal, nested obelisk destruction and pool recycling."""
import struct
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW)
from original_oracle import load_game


def main(combat=False):
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x400000);cpu.mem_map(0,0x10000)
    zone,map_,ob,group,definition,manager= [base+x for x in (0,0x1000,0x2000,0x3000,0x4000,0x5000)]
    mobs,grid,heap,stack,stop= [base+x for x in (0x10000,0x20000,0x40000,0x3e0000,0x3f0000)]
    state={}
    def put(at,*values): cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def get(at): return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def event(*values):
        if state['capture']: state['trace'].extend(v&0xffffffff for v in values)
    def identity(pointer): return 255 if not pointer else (pointer-mobs)//0x2000
    def cell(x,z): return grid+0x1000+(z*3+x)*292
    for at,cleanup in ((0x503a3f,0),(0x503ba1,0),(0x4208c0,12),(0x4207f0,0),
        (0x4d4e50,8),(0x4d4f10,8),(0x42a000,0),(0x4953f0,0),(0x5046b9,0),(0x505316,0),
        (0x421120,24),(0x415f30,4),(0x458240,12),(0x4a5ea0,12),(0x480140,4),
        (0x4a0990,0),(0x413600,8),(0x413750,8),(0x412ec0,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    put(0x546f3c,manager)
    for iat in (0x5210cc,0x5210d0): put(iat,stop+16)
    put(0x5210e4,stop+32);put(0x5210f8,stop+48)
    def hook(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP);eax=machine.reg_read(UC_X86_REG_EAX)
        if address==0x503a3f:
            at=state['heap'];state['heap']+=(get(sp+4)+15)&~15;machine.mem_write(at,bytes(get(sp+4)));machine.reg_write(UC_X86_REG_EAX,at)
        elif address==stop+32:
            tick=(state['tick']+17*state['ticks'])&0xffffffff;state['ticks']+=1;event(1,tick);machine.reg_write(UC_X86_REG_EAX,tick)
        elif address==stop+48:
            at=get(sp+4);value=(get(at)+1)&0xffffffff;put(at,value);machine.reg_write(UC_X86_REG_EAX,value)
        elif address==0x5046b9: event(12,state['unix']);machine.reg_write(UC_X86_REG_EAX,state['unix'])
        elif address==0x4208c0:
            pointer,length=get(sp+8),get(sp+12)
            if state['capture']:
                mob_id=get(pointer+2);mob=next(mobs+i*0x2000 for i in range(3) if get(mobs+i*0x2000+0x5c)==mob_id)
                event(2,length,*cpu.mem_read(pointer,length),get(zone+0x15c),int(get(mob+0x60)!=0),get(mob+0x64),get(mob+0x68))
        elif address==0x4953f0:
            mob=machine.reg_read(UC_X86_REG_EDI)-0xac
            event(3,identity(mob),get(mob+0x5c),get(zone+0x15c),get(manager+0x44),cpu.mem_read(map_+0x1e4,1)[0])
        elif address==0x4207f0:
            event(4,eax,*cpu.mem_read(machine.reg_read(UC_X86_REG_ECX),eax),cpu.mem_read(map_+0x1e4,1)[0])
        elif address in (0x4d4e50,0x4d4f10):
            length=get(sp+8);event(5 if address==0x4d4e50 else 6,length,*cpu.mem_read(get(sp+4),length))
        elif address==0x42a000: event(7,cpu.mem_read(map_+0x1e4,1)[0])
        elif address==0x421120:
            n=get(sp+12);event(24,n,*cpu.mem_read(get(sp+8),n),get(sp+24),get(zone+0x15c))
        elif address==0x415f30:
            event(15,get(sp+4));machine.reg_write(UC_X86_REG_EAX,base+0x30000 if state['credit'] in (1,4) else 0)
        elif address==0x458240: event(16,100,get(sp+4),get(sp+8))
        elif address==0x4a5ea0:
            event(13,100 if get(sp+8) else 0)
            if state['scenario']%13==0: put(ob+4,(get(ob+4)+1)%3)
        elif address==0x480140:
            event(14,100)
            if state['scenario']%11==0:
                at=base+0x30000+0x12d;cpu.mem_write(at,bytes([(cpu.mem_read(at,1)[0]+1)%3]))
        elif address==0x4a0990:
            head=base+0xc000
            event(25,get(ob+0x48),get(ob+0x4c),int(get(head)==head and get(head+4)==head),
                  cpu.mem_read(mobs+0x1f0,1)[0],get(zone+0x15c),get(0x3ed353c+8))
        elif address in (0x413600,0x413750): event(20 if address==0x413600 else 21,machine.reg_read(UC_X86_REG_EBX),get(sp+8))
        elif address==0x412ec0: event(23,eax,*cpu.mem_read(machine.reg_read(UC_X86_REG_ECX),eax))

    cpu.hook_add(UC_HOOK_CODE,hook)
    def invalid(machine,access,address,size,value,data): raise RuntimeError(f'Invalid zone access {address:x} at {machine.reg_read(UC_X86_REG_EIP):x}')
    cpu.hook_add(UC_HOOK_MEM_INVALID,invalid)
    def call(at,*args):
        put(stack,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.emu_start(at,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('Original zone call instruction limit exceeded')
        return cpu.reg_read(UC_X86_REG_EAX)
    def snapshot():
        event(9,get(zone+0x15c),get(manager+0x44),get(manager+8),cpu.mem_read(map_+0x1e4,1)[0],int(get(ob+0x6c)!=0),
              get(ob+4),get(ob+8),get(ob+0x48),get(ob+0x4c),get(group+0x38),get(group+0x3c),identity(get(group+0x54) and get(get(group+0x54))),identity(get(get(group+0x54)+4)))
    if combat: print('# Full CMob::Death 004910f0 with real selection/reset, obelisk ownership, nested zone removal and real corpse cleanup 00421a80. Rewards/user lookup/skill effects and KC/world broadcasts are observed ports. Explicit caller sets death state/deadline after Death. Tags 13 drop,14 quest,15 lookup,16 experience,20/21 KC add/sub,23 broadcast,24 death,25 skill-clear boundary.')
    print(f'# ps_game.exe SHA256 {digest}; real MobRemoveByID 0041e910, FindMob/RemoveMob/cell unlink, ObeliskDestroy 0042a410, ObeliskReset 0042aa40, RemoveMob 0041d700, FreeMob 0040a630 and pooled CreateMob 0040a5e0/seed. Initial actors are explicit inputs. Ports: heap, locks, tick/Unix/atomic, compiled-AI cleanup, transport and portal updates.')
    print('# scenario then trace uint32 words. 1=tick; 2=cell packet(length,bytes,zone size,raw zone/cell); 3=compiled cleanup(identity,id,zone size,free count,reset flag); 4=zone packet(length,bytes,flag); 5/6=log/DB(length,bytes); 7=portal flag; 8=reuse(identity,id,free count,counter,seed,word0); 9=snapshot; 10=operation; 11=result; 12=Unix time.')
    for scenario in range(256):
        cpu.mem_write(base,bytes(0x3ff000));state.update(scenario=scenario,heap=heap+0x10000,trace=[],capture=False,ticks=0,
            tick=(0,100,0xfffffff0,0xffffffff)[scenario//11%4],unix=123456+scenario)
        for at,code in ((stop+16,b'\xc2\x04\x00'),(stop+32,b'\xc3'),(stop+48,b'\xc2\x04\x00')): cpu.mem_write(at,code)
        counter=(0,0xfffffffd,0xffffffff)[scenario%3];put(manager+8,(counter+3)&0xffffffff)
        put(zone+0x24,map_,3,3,grid);put(grid,*(cell(0,z) for z in range(3)))
        head=heap;put(zone+0xa4,0,head,0);put(head,head,head,head);cpu.mem_write(head+20,b'\x01\x01')
        head+=0x100;put(zone+0x13c,head,head);put(head,head,head)
        for z in range(3):
            for x in range(3):
                c=cell(x,z);head=heap+0x1000+(z*3+x)*0x100;put(c+0x2c,head);put(head+0x48,head+0x48,head+0x48)
        put(map_,zone);put(map_+0x78,42);put(map_+0x1e8,1,ob);cpu.mem_write(map_+0x1e4,bytes([scenario//7%2]))
        put(ob,0x81001234,scenario%3,(-1,0,1,2)[scenario//9%4],0,0xfedcba98,0x01234567,scenario//3%3)
        put(ob+0x40,0,1,(1,0,-1)[scenario//5%3],123);cpu.mem_write(ob+0x54,b'\x01');put(ob+0x64,1,group,mobs)
        put(group+0x14,2);put(group+0x30,0,2,(2,1,0,-1)[scenario//7%4],2345);put(group+0x48,group);put(group+0x54,base+0x9000)
        slots=((1,1),(255,2),(1,2),(255,255),(2,1))[scenario%5]
        put(base+0x9000,*[0 if i==255 else mobs+i*0x2000 for i in slots])
        cpu.mem_write(definition,struct.pack('<H',321)+b'ABCDEFGHIJKLMNOPQRSTUVWXYZ12345\0');put(definition+0x6c,(0,1,20,0xffffffff,0x80000000)[scenario//3%5])
        ids=[]
        for i in range(3):
            mob=mobs+i*0x2000;mid=(counter+i+1)&0xffffffff;ids.append(mid)
            put(mob+0x5c,mid);put(mob+0x74,mob+0x50);put(mob+0xa8,definition);put(mob+0xc84,ob+0x40 if i==0 else group+0x30)
            cpu.mem_write(mob+0x220,struct.pack('<H',654+i));cpu.mem_write(mob+0x50,struct.pack('<fff',(1.25,41.25,80.25)[i],2.5,3.25))
            cpu.reg_write(UC_X86_REG_EAX,mob);call(0x416860,zone)
        if combat:
            credit=scenario//3%5;state['credit']=credit
            head=base+0xc000;node=base+0xc100;put(mobs+0x1f4,head,head)
            put(head,node if credit else head,node if credit else head)
            if credit:
                put(node,head,head);cpu.mem_write(node+8,bytes([2 if credit==3 else 1]));put(node+12,100,41,0,0)
            cpu.mem_write(mobs+0x1f0,bytes([scenario//7%2]));put(0x3ed353c,0,0,0)
            cpu.mem_write(definition+0x22,struct.pack('<HH',1,0));user=base+0x30000
            cpu.mem_write(user+0x136,struct.pack('<H',1));cpu.mem_write(user+0x12d,bytes([scenario//15%3]))
            put(0x5471c0,0 if scenario//6%2 else 30,-15 if scenario//7%2 else 0)
            put(0x546f38,base+0xb000)
            for country in range(3): put(base+0xb000+0x208+country*0x84,scenario//4%2)
        state['capture']=True;snapshot();mode=7 if combat else scenario//5%2;event(10,mode)
        if combat:
            cpu.reg_write(UC_X86_REG_ECX,mobs);cpu.reg_write(UC_X86_REG_FPCW,0x27f);call(0x4910f0,2,0xfedcba98);event(11,255)
        elif mode:
            cpu.reg_write(UC_X86_REG_ESI,map_);call(0x42aa40);event(11,255)
        else:
            cpu.reg_write(UC_X86_REG_ECX,zone);event(11,call(0x41e910,ids[0])&255)
        snapshot()
        if combat:
            # CMob constructor binds its global node payload to vPos (+0050).
            # UpdateMobDeath recovers CMob from node->payload - 0050.
            if get(mobs+0x74)!=mobs+0x50: raise RuntimeError('Unbound original zone node payload')
            put(mobs+0x124,2);put(mobs+0xcb0,10)
            for op,tick in ((8,10),(9,11)):
                event(10,op);call(0x421a80,zone,tick);event(11,255);snapshot()
        for op,index in ((2,0),(3,1),(4,2)):
            event(10,op);cpu.reg_write(UC_X86_REG_ECX,zone);event(11,call(0x41e910,ids[index])&255);snapshot()
        for op in (5,6):
            event(10,op);cpu.reg_write(UC_X86_REG_EAX,manager);mob=call(0x40a5e0)
            event(8,identity(mob),get(mob+0x5c),get(manager+0x44),get(manager+8),get(mob+0x2bc),get(mob+0x2c0));snapshot()
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__': main()
