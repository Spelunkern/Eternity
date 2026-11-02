#!/usr/bin/env python3
"""Execute original guard target queries/setters with live player and mob lists. Requires Unicorn."""
import json
import math
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,npc,zone,map_object,definition,source,points,grid,users,mobs,tiles,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x36000000,0x37000000,0x38000000,0x40000000,0x10000000)
    for address in (0,stack,npc,zone,map_object,definition,source,points,grid,mobs,tiles,stop):
        cpu.mem_map(address,0x10000)
    cpu.mem_map(users,0x100000);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
    cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc3')
    for pointer in (0x5210cc,0x5210d0):
        cpu.mem_write(pointer,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210e4,struct.pack('<I',stop+0x200))
    context=dict(base=0,calls=0,mutation=0)
    coverage={address:0 for address in (0x4bc627,0x4bc62f,0x4bc692,0x4bc64b,0x4bc6f9,0x4bc75a,0x4bc797)}

    def number(address):
        return struct.unpack('<I',cpu.mem_read(address,4))[0]

    def hook(machine,address,size,_):
        if address in coverage:
            coverage[address]+=1;return
        sample=(context['base']+context['calls']*17)&0xffffffff;context['calls']+=1
        if context['calls']==1:
            mutation=context['mutation']
            if mutation==1:
                for i in range(12):
                    machine.mem_write(users+i*0x6000+0x142c,struct.pack('<I',1));machine.mem_write(mobs+i*0x1000+0x44,bytes(4));machine.mem_write(mobs+i*0x1000+0x124,struct.pack('<I',2))
            elif mutation in (2,3):
                machine.mem_write(npc+0x58,struct.pack('<I',1 if mutation==2 else 2))
            elif mutation==4:
                for i in range(12):
                    machine.mem_write(users+i*0x6000+0x1366,b'\1');machine.mem_write(users+i*0x6000+0x5774,b'\0\0')
            elif mutation==5:
                for i in range(12):
                    machine.mem_write(users+i*0x6000+0x5850,struct.pack('<I',(sample+1)&0xffffffff));machine.mem_write(mobs+i*0x1000+0x2b0,struct.pack('<I',(sample+1)&0xffffffff))
        machine.reg_write(UC_X86_REG_EAX,sample)
    for address in (stop+0x200,*coverage):
        cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)

    def call(address):
        cpu.mem_write(stack+0x8000,struct.pack('<I',stop));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(address,stop,timeout=30000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError('Original guard target operation did not return')
        return cpu.reg_read(UC_X86_REG_EAX)&255

    offsets=((0,0),(3,4),(20,0),(20.000003814697266,0),(19.999996185302734,0),(30,0),(math.nan,0),(math.inf,0),(-3,-4),(10,0),(0,20),(0,30))
    def initialize(s):
        extent=(40,81,160)[s%3];count=extent//40+1;center=extent/2
        context.update(base=(0,100,5000,0xffffffff)[(s//7)%4],calls=0,mutation=(s//17)%6)
        cpu.mem_write(npc,bytes(152));cpu.mem_write(zone,bytes(256));cpu.mem_write(map_object,bytes(256));cpu.mem_write(definition,bytes(292));cpu.mem_write(source,bytes(32));cpu.mem_write(grid,bytes(0x10000));cpu.mem_write(users,bytes(12*0x6000));cpu.mem_write(mobs,bytes(12*0x1000))
        position=[center-0.75,3,center-0.25]
        if s%41==0:
            position[0]=-0.25
        if s%43==0:
            position[0]=-1
        if s%47==0:
            position[2]=math.nan
        data=bytearray((extent*extent+7)//8)
        if s%37==0 and all(math.isfinite(value) for value in (position[0],position[2])):
            x,z=int(position[0]),int(position[2])
            if 0<=x<extent and 0<=z<extent:
                at=z*extent+x;data[at//8]|=1<<(at%8)
        cpu.mem_write(tiles,bytes(data));cpu.mem_write(map_object+4,struct.pack('<2I',extent,tiles))
        cx,cz=(s//3)%count,(s//5)%count;cpu.mem_write(npc+8,struct.pack('<3f',*position));cpu.mem_write(npc+0x18,struct.pack('<3I',zone,cx,cz))
        cpu.mem_write(npc+0x48,struct.pack('<I',(s//11)%3));cpu.mem_write(npc+0x50,struct.pack('<2I',definition,source));cpu.mem_write(npc+0x58,struct.pack('<3I',(s//13)%4,2,777))
        cpu.mem_write(definition+16,struct.pack('<I',(0,1,2,255)[(s//19)%4]));cpu.mem_write(source+8,struct.pack('<2I',3,points));cpu.mem_write(points,struct.pack('<9f',center,0,center,center-10,2,center-10,center+10,4,center+10))
        cpu.mem_write(zone+0x24,struct.pack('<4I',map_object,count,count,grid));cpu.mem_write(grid,struct.pack('<'+'I'*count,*(grid+0x1000+z*count*292 for z in range(count))))
        for z in range(count):
            for x in range(count):
                cell=grid+0x1000+(z*count+x)*292;head=grid+0x4000+(z*count+x)*128
                cpu.mem_write(cell,struct.pack('<I',head));cpu.mem_write(head+8,struct.pack('<2I',head+8,head+8))
                head+=32;cpu.mem_write(cell+0x2c,struct.pack('<I',head));cpu.mem_write(head+0x48,struct.pack('<2I',head+0x48,head+0x48))
        for i in range(12):
            user=users+i*0x6000;mob=mobs+i*0x1000;ux,uz=(s+i*2)%count,(s*3+i)%count;mx,mz=(s+i*3+1)%count,(s*2+i)%count
            dx,dz=offsets[(s+i)%len(offsets)];cpu.mem_write(user+0xd0,struct.pack('<3f',position[0]+dx,9000,position[2]+dz));cpu.mem_write(user+0xdc,struct.pack('<I',9001+i));cpu.mem_write(user+0x12d,bytes([(s+i)%3]))
            cpu.mem_write(user+0x142c,struct.pack('<I',int((s+i)%11==0)));cpu.mem_write(user+0x1354,bytes([13 if (s+i)%7==0 else 0]));cpu.mem_write(user+0x1366,bytes([int((s+i)%13==0)]));cpu.mem_write(user+0x5774,bytes([int((s+i)%17!=0),int((s+i)%19!=0)]))
            cooldown=((context['base']-1)&0xffffffff,context['base'],(context['base']+1)&0xffffffff,0xffffffff)[(s+i)%4];cpu.mem_write(user+0x5850,struct.pack('<I',cooldown))
            head=grid+0x4000+(uz*count+ux)*128;previous=number(head+8);cpu.mem_write(user+8,struct.pack('<2I',previous,head+8));cpu.mem_write(previous+4,struct.pack('<I',user+8));cpu.mem_write(head+8,struct.pack('<I',user+8))
            dx,dz=offsets[(s+i+3)%len(offsets)];cpu.mem_write(mob+0x50,struct.pack('<3f',position[0]+dx,-9000,position[2]+dz));cpu.mem_write(mob+0x5c,struct.pack('<I',10001+i));cpu.mem_write(mob+0xa8,struct.pack('<I',mob+0x400));cpu.mem_write(mob+0x478,struct.pack('<I',(s+i)%4))
            state=(s+i)%4;mode=(s+i)//3%3;cpu.mem_write(mob+0x124,struct.pack('<I',state))
            if mode:
                cpu.mem_write(mob+0x44,struct.pack('<I',1));cpu.mem_write(mob+0x34,struct.pack('<4I',mob+0x500,1,0,0 if mode==1 else 1));cpu.mem_write(mob+0x500,struct.pack('<I',mob+0x600));cpu.mem_write(mob+0x600,struct.pack('<I',mob+0x700));cpu.mem_write(mob+0x728,struct.pack('<I',state))
            cpu.mem_write(mob+0x2b0,struct.pack('<I',cooldown));head=grid+0x4000+(mz*count+mx)*128+32;previous=number(head+0x48);cpu.mem_write(mob+0x48,struct.pack('<2I',previous,head+0x48));cpu.mem_write(previous+4,struct.pack('<I',mob+0x48));cpu.mem_write(head+0x48,struct.pack('<I',mob+0x48))

    def snapshot(result):
        record=bytes([result])+bytes(cpu.mem_read(npc+0x58,12))+struct.pack('<I',context['calls'])
        for i in range(12):
            user=users+i*0x6000;record+=bytes(cpu.mem_read(user+0x5850,4))+bytes(cpu.mem_read(user+0x142c,4))
        for i in range(12):
            mob=mobs+i*0x1000
            state=number(mob+0x124) if number(mob+0x44)==0 else 0 if number(mob+0x40)==0 else number(mob+0x728)
            record+=bytes(cpu.mem_read(mob+0x2b0,4))+struct.pack('<I',state)
        return record

    def query():
        cpu.reg_write(UC_X86_REG_EDI,npc);return snapshot(call(0x4bc180))
    print(f'# ps_game.exe SHA256 {digest}; FindTarget 004bc180, SetTarget(CUser*) 004bc750, SetTarget(CMob*) 004bc790, real intrusive lists/FSM lookup/CRT sqrt. Clock/critical-section/CRT TLS ports isolated.')
    print('# S scenario expected_hex. D kind scenario expected_hex (return fixed zero). R scenario starts shared-cooldown sequence; G expected_hex resets NPC state only. Input formulas in tools/npc_targets_oracle.py. Result: BOOL,status,kind,id,clock_calls,12 player cooldown/mode pairs,12 mob cooldown/derived-state pairs.')
    for s in range(1024):
        initialize(s);print('S',s,query().hex(),sep='\t')
    for kind in range(2):
        for s in range(128):
            initialize(s)
            if kind==0:
                cpu.reg_write(UC_X86_REG_EAX,npc);cpu.reg_write(UC_X86_REG_ESI,users);call(0x4bc750)
            else:
                cpu.reg_write(UC_X86_REG_ESI,npc);cpu.reg_write(UC_X86_REG_EBX,mobs);call(0x4bc790)
            print('D',kind,s,snapshot(0).hex(),sep='\t')
    for s in range(1000,1032):
        initialize(s);print('R',s,sep='\t')
        for _ in range(4):
            cpu.mem_write(npc+0x58,struct.pack('<3I',0,2,777));print('G',query().hex(),sep='\t')
    print('# Original target branch coverage '+json.dumps({f'{address:08x}':count for address,count in coverage.items()},sort_keys=True))
    if not coverage[0x4bc62f] or not coverage[0x4bc692]:
        raise RuntimeError('Missing original player/mob selection coverage')


if __name__=='__main__':
    main()
