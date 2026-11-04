#!/usr/bin/env python3
"""Real zone/index/cell removal, nested obelisk destruction and pool recycling."""
import struct
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP)
from original_oracle import load_game


def main():
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
        (0x4d4e50,8),(0x4d4f10,8),(0x42a000,0),(0x4953f0,0),(0x5046b9,0),(0x505316,0)):
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
            put(mob+0x5c,mid);put(mob+0xa8,definition);put(mob+0xc84,ob+0x40 if i==0 else group+0x30)
            cpu.mem_write(mob+0x220,struct.pack('<H',654+i));cpu.mem_write(mob+0x50,struct.pack('<fff',(1.25,41.25,80.25)[i],2.5,3.25))
            cpu.reg_write(UC_X86_REG_EAX,mob);call(0x416860,zone)
        state['capture']=True;snapshot();mode=scenario//5%2;event(10,mode)
        if mode:
            cpu.reg_write(UC_X86_REG_ESI,map_);call(0x42aa40);event(11,255)
        else:
            cpu.reg_write(UC_X86_REG_ECX,zone);event(11,call(0x41e910,ids[0])&255)
        snapshot()
        for op,index in ((2,0),(3,1),(4,2)):
            event(10,op);cpu.reg_write(UC_X86_REG_ECX,zone);event(11,call(0x41e910,ids[index])&255);snapshot()
        for op in (5,6):
            event(10,op);cpu.reg_write(UC_X86_REG_EAX,manager);mob=call(0x40a5e0)
            event(8,identity(mob),get(mob+0x5c),get(manager+0x44),get(manager+8),get(mob+0x2bc),get(mob+0x2c0));snapshot()
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__': main()
