#!/usr/bin/env python3
"""Execute original guard chase/return, breadcrumb trail and A* navigation. Requires Unicorn."""
import json
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game();stack, npc, target, zone, map_object, definition, source, points, tiles, heap, stop = (0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x36000000,0x40000000,0x42000000,0x10000000)
    for address in (0,stack,npc,target,zone,map_object,definition,source,points,stop):
        cpu.mem_map(address,0x10000)
    cpu.mem_map(tiles,0x10000);cpu.mem_map(heap,0x1000000);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
    cpu.mem_write(0x503a3f,b'\xc3');cpu.mem_write(0x416f30,b'\xc2\x14\x00')
    cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc3')
    for pointer in (0x5210cc,0x5210d0):
        cpu.mem_write(pointer,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210e4,struct.pack('<I',stop+0x200))
    context=dict(allocation=heap,clock=0,clocks=0,success=True,moves=b'')
    coverage={address:0 for address in (0x4bca14,0x4bca97,0x4bcae0,0x4bcb2d,0x4bcb67,0x4bcbcb,0x4bcbdf,0x4bcc0c,0x4bcc2c,0x4bcd6a,0x4bce89,0x4bcee7,0x4bcf41,0x4bcfa0,0x4bcfde,0x4bd058,0x4bd0e1,0x4bd1bd,0x4bd28d,0x4bd2bb,0x4bd2f7)}

    def number(address):
        return struct.unpack('<I',cpu.mem_read(address,4))[0]

    def hook(machine,address,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address in coverage:
            coverage[address]+=1
        elif address==0x503a3f:
            amount=number(sp+4);pointer=context['allocation'];context['allocation']+=(max(amount,1)+15)&~15
            if context['allocation']>heap+0x1000000:
                raise RuntimeError('Navigation oracle allocation limit')
            machine.mem_write(pointer,bytes(amount));machine.reg_write(UC_X86_REG_EAX,pointer)
        elif address==0x416f30:
            actual_zone,x,y,z,mode=struct.unpack('<5I',machine.mem_read(sp+4,20))
            if actual_zone!=zone or mode!=1:
                raise RuntimeError('Unexpected original guard navigation move')
            context['moves']+=struct.pack('<4I',x,y,z,mode)
            if context['success']:
                machine.mem_write(npc+8,struct.pack('<3I',x,y,z))
            machine.reg_write(UC_X86_REG_EAX,int(context['success']))
        elif address==stop+0x200:
            context['clocks']+=1;machine.reg_write(UC_X86_REG_EAX,context['clock'])
    for address in (0x503a3f,0x416f30,stop+0x200,*coverage):
        cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)

    def call(address,arguments=()):
        cpu.mem_write(stack+0x8000,struct.pack('<'+'I'*(len(arguments)+1),stop,*(value&0xffffffff for value in arguments)));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
        cpu.emu_start(address,stop,timeout=120000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError(f'Original navigation did not return from {address:x}; EIP {cpu.reg_read(UC_X86_REG_EIP):x}')
        return cpu.reg_read(UC_X86_REG_EAX)&255

    def snapshot(result):
        record=bytes([result])+bytes(cpu.mem_read(npc+8,12))+bytes(cpu.mem_read(npc+0x58,20))
        record+=bytes(cpu.mem_read(npc+0x90,1))+bytes(cpu.mem_read(npc+0x80,16))
        cursor=number(npc+0x7c);record+=bytes([bool(cursor)])
        if cursor:
            record+=bytes(cpu.mem_read(cursor+4,8))
        chain=b'';node=number(npc+0x74);seen=set()
        while node:
            if node in seen:
                raise RuntimeError('Original guard has cyclic path')
            seen.add(node);chain+=bytes(cpu.mem_read(node+4,8));node=number(node+0x1c)
        record+=struct.pack('<I',len(chain)//8)+chain
        chain=b'';node=number(npc+0x94)
        while node:
            chain+=bytes(cpu.mem_read(node,8));node=number(node+8)
        return record+struct.pack('<I',len(chain)//8)+chain+struct.pack('<2I',context['clocks'],len(context['moves'])//16)+context['moves']

    def initialize(extent,data,current,status,kind,uid,next_chase,disable,speed,distance,index,path_mode,path_point,trail):
        context.update(allocation=heap,moves=b'',clocks=0);cpu.mem_write(0x3ed34b8,bytes(12));cpu.mem_write(0x3ed34dc,bytes(12))
        cpu.mem_write(npc,bytes(152));cpu.mem_write(zone,bytes(256));cpu.mem_write(map_object,bytes(256));cpu.mem_write(definition,bytes(292));cpu.mem_write(source,bytes(32))
        cpu.mem_write(zone+0x24,struct.pack('<I',map_object));cpu.mem_write(map_object+4,struct.pack('<2I',extent,tiles));cpu.mem_write(tiles,bytes(data))
        cpu.mem_write(npc+8,struct.pack('<3f',*current));cpu.mem_write(npc+0x18,struct.pack('<I',zone));cpu.mem_write(npc+0x48,struct.pack('<I',index))
        cpu.mem_write(npc+0x50,struct.pack('<2I',definition,source));cpu.mem_write(npc+0x58,struct.pack('<2i3I',status,kind,uid,next_chase,disable))
        cpu.mem_write(definition+8,struct.pack('<2i',distance,speed));cpu.mem_write(source+8,struct.pack('<2I',3,points));cpu.mem_write(points,struct.pack('<9f',5,1,5,10,2,10,15,3,5))
        if path_mode==1:
            cpu.reg_write(UC_X86_REG_EDI,npc+0x6c);call(0x4a6b20,path_point)
        elif path_mode==2:
            cpu.reg_write(UC_X86_REG_ESI,npc+0x6c);cpu.reg_write(UC_X86_REG_EAX,path_point[0]);call(0x48eda0,(5,5,path_point[1],map_object))
        for x,z in trail:
            cpu.reg_write(UC_X86_REG_EDI,npc+0x94);cpu.reg_write(UC_X86_REG_EBX,z);cpu.reg_write(UC_X86_REG_EDX,map_object);call(0x48f660,(x,))

    def run(operation,position,reach,tick,clock,success):
        context.update(clock=clock,success=success,moves=b'',clocks=0);cpu.mem_write(target,struct.pack('<3f',*position))
        if operation==0:
            cpu.reg_write(UC_X86_REG_EAX,target);result=call(0x4bca00,(npc,tick,reach))
        else:
            result=call(0x4bcf30,(npc,tick,target))
        return snapshot(result)

    def bits(position):
        return struct.unpack('<3I',struct.pack('<3f',*position))
    print(f'# ps_game.exe SHA256 {digest}; Chase 004bca00, ReturnHome 004bcf30, ChasePathList::AddPath 0048f660/GetNextPath 0048f700, real A*/line/vector/CRT math. Allocation, movement, clock, locks and CRT TLS ports isolated.')
    print('# N map_id extent tile_hex. M op map_id current_bits target_bits reach tick clock speed distance status kind id next_chase disable move_success index path_mode path_X/Z trail_count trail_X/Z expected_hex. Result: BOOL,position,status/target/deadlines,path flag/bounds/cursor/full chain,trail,clock_calls,moves.')
    maps=[]
    for scenario in range(4):
        extent=32;data=bytearray((extent*extent+7)//8)
        for z in range(extent):
            for x in range(extent):
                blocked=(scenario==1 and x==12 and z!=12) or (scenario==2 and x==12) or (scenario==3 and (x,z) in ((5,5),(8,8),(10,10),(15,5)))
                if blocked:
                    at=z*extent+x;data[at//8]|=1<<(at%8)
        maps.append((extent,bytes(data)));print('N',scenario,extent,data.hex(),sep='\t')
    for operation in range(2):
        for scenario in range(768):
            map_id=scenario%4;extent,data=maps[map_id]
            values=(0,0.25,5,5.75,8,10,11.5,12,15,20,29,31.75,-0.25,float('nan'),float('inf'))
            current=(values[(scenario//4)%len(values)],3.25,values[(scenario//9)%len(values)])
            position=(values[(scenario//5+3)%len(values)],9.5,values[(scenario//11+5)%len(values)])
            reach=(2,0,-1,100)[(scenario//13)%4];tick=(0,100,0x7fffffff,0x80000000,0xffffffff)[(scenario//17)%5];clock=(tick+23)&0xffffffff
            speed=(0,500,-1,2147483647,-2147483648)[(scenario//19)%5];distance=(0,1,2,5,100,-1)[(scenario//23)%6]
            status=(scenario//3)%4;kind=2 if scenario&4 else 1;uid=70001;deadline=(0,100,0xffffffff)[(scenario//29)%3];disable=0x12345678
            success=scenario%7!=0;index=(scenario//31)%3;path_mode=(scenario//37)%3;path_point=((8,8),(20,20),(15,5))[(scenario//41)%3]
            trail=[(5+i*2,5+i) for i in range((scenario//43)%5)]
            initialize(extent,data,current,status,kind,uid,deadline,disable,speed,distance,index,path_mode,path_point,trail)
            record=run(operation,position,reach,tick,clock,success)
            print('M',operation,map_id,*bits(current),*bits(position),reach,tick,clock,speed,distance,status,kind,uid,deadline,disable,int(success),index,path_mode,*path_point,len(trail),*(value for point in trail for value in point),record.hex(),sep='\t')
    # Adjacent blocked targets and blocked current tiles distinguish frozen status
    # from ordinary attack-stop/return transitions and retain different state.
    for blocked_current in (False,True):
        for status in range(4):
            map_id=1;extent,data=maps[map_id];current=(12.25 if blocked_current else 11.25,3.25,5.25)
            position=(20.25 if blocked_current else 12.25,9.5,5.25);reach=2 if blocked_current else 0
            path_mode=int(blocked_current);path_point=(8,8);trail=[(5,5),(7,5)]
            initialize(extent,data,current,status,1,70001,0,0x12345678,500,2,0,path_mode,path_point,trail)
            record=run(0,position,reach,100,123,True)
            print('M',0,map_id,*bits(current),*bits(position),reach,100,123,500,2,status,1,70001,0,0x12345678,1,0,path_mode,*path_point,len(trail),*(value for point in trail for value in point),record.hex(),sep='\t')
    # Persistent movement uses fixed original waypoints, sampling each recorded deadline.
    for map_id in range(3):
        extent,data=maps[map_id];current=(5.25,3,5.75)
        initialize(extent,data,current,1,1,70001,0,0,500,2,0,0,(0,0),[])
        print('B',map_id,sep='\t')
        for step in range(36):
            operation=0 if step<20 else 1;position=(20.25,9,20.75) if operation==0 else (5.25,3,5.75)
            tick=number(npc+0x64);clock=(tick+7)&0xffffffff;success=step%13!=7
            print('T',operation,*bits(position),2,tick,clock,int(success),run(operation,position,2,tick,clock,success).hex(),sep='\t')

    if not all(coverage.values()):
        raise RuntimeError('Uncovered original navigation branches: '+str([hex(address) for address,count in coverage.items() if not count]))
    print('# Original navigation branch coverage '+json.dumps({f'{address:08x}':count for address,count in coverage.items()},sort_keys=True))


if __name__=='__main__':
    main()
