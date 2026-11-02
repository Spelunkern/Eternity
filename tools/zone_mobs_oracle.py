#!/usr/bin/env python3
"""Original mob index/list membership and whole-cell snapshots; requires Unicorn."""
import struct
from unicorn import UC_HOOK_CODE, UC_HOOK_MEM_INVALID
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack, zone, mobs, grid, heap, stop = 0x20000000, 0x30000000, 0x31000000, 0x32000000, 0x33000000, 0x10000000
    for at in (stack, zone, mobs, grid, heap, stop):
        cpu.mem_map(at, 0x100000)
    cpu.mem_map(0,0x10000)
    cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
    def invalid(machine,access,address,size,value,data):
        raise RuntimeError(f'Invalid original access {address:08x} at {machine.reg_read(UC_X86_REG_EIP):08x}')
    cpu.hook_add(UC_HOOK_MEM_INVALID,invalid)
    context = dict(heap=heap+0x10000, events=b'', reenter=False, cell=0)
    def put(at, *values):
        cpu.mem_write(at, struct.pack('<'+'I'*len(values), *values))
    def get(at):
        return struct.unpack('<I', cpu.mem_read(at, 4))[0]
    def ident(pointer):
        if not pointer:
            return 255
        index, rem = divmod(pointer-mobs, 0x2000)
        if rem or not 0 <= index < 4:
            raise RuntimeError(f'Unexpected original mob pointer {pointer:08x}')
        return index
    def event(tag, packet):
        context['events'] += bytes([tag])+struct.pack('<I', len(packet))+packet
    for at, cleanup in ((0x4208c0,12),(0x4b1010,4),(0x4d4d20,8),(0x503a3f,0),(0x503ba1,0),(0x505316,0)):
        cpu.mem_write(at, b'\xc2'+struct.pack('<H', cleanup))
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00')
    for at in (0x5210cc,0x5210d0):
        put(at,stop+0x100)
    def hook(machine, address, size, _):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address == 0x503a3f:
            size=get(sp+4);at=context['heap'];context['heap']+=(size+15)&~15
            machine.mem_write(at,bytes(size));machine.reg_write(UC_X86_REG_EAX,at)
        elif address == 0x4208c0:
            owner,packet,length=struct.unpack('<3I',machine.mem_read(sp+4,12))
            if owner != zone:
                raise RuntimeError('Unexpected SendView owner')
            event(1, bytes(machine.mem_read(packet,length)))
        elif address == 0x4b1010:
            event(2, bytes([ident(get(sp+4))]))
        elif address == 0x4d4d20:
            packet,length=struct.unpack('<2I',machine.mem_read(sp+4,8))
            event(3,bytes(machine.mem_read(packet,length)))
            if context['reenter']:
                context['reenter']=False
                # A failed reentrant cell lookup leaves the shared cursor at its sentinel.
                put(context['cell']+0x30,get(context['cell']+0x2c))
    cpu.hook_add(UC_HOOK_CODE,hook)
    def call(at,args=()):
        put(stack+0x8000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
        cpu.emu_start(at,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:
            raise RuntimeError('Original mob registry call did not return')
        return cpu.reg_read(UC_X86_REG_EAX)
    def cell(x,z):
        return grid+0x1000+(z*3+x)*292
    def initialize(scenario):
        context.update(heap=heap+0x10000,events=b'',reenter=False)
        cpu.mem_write(zone,bytes(0x1000));cpu.mem_write(mobs,bytes(0x10000));cpu.mem_write(grid,bytes(0x10000));cpu.mem_write(heap,bytes(0x20000))
        put(zone+0x28,3,3,grid);put(grid,*(cell(0,z) for z in range(3)))
        head=heap;put(zone+0xa4,0,head,0);put(head,head,head,head);cpu.mem_write(head+20,b'\x01\x01')
        head=heap+0x100;put(zone+0x13c,head,head);put(head,head,head)
        for z in range(3):
            for x in range(3):
                c=cell(x,z);base=heap+0x1000+(z*3+x)*0x500
                for offset,links in ((0,8),(0x2c,0x48),(0x58,0),(0x84,0),(0xb0,0),(0xdc,0x28)):
                    head=base+offset*4;put(c+offset,head);put(head+links,head+links,head+links)
        ids=(0,0x80000000,0xffffffff,1234)
        for i in range(4):
            m=mobs+i*0x2000;mid=ids[scenario%4] if i<2 else ids[(scenario+i)%4]
            x=-0.25 if scenario%7==0 else 1.25
            if i==2 and scenario%2:
                x=81.25
            if i==3:
                x=121.25
            cpu.mem_write(m+0x50,struct.pack('<3f',x,-i-0.5,3.25));put(m+0x5c,mid)
            cpu.mem_write(m+0x220,struct.pack('<H',(scenario*1000+i)&65535));put(m+0x284,int((scenario+i)%3==0));put(m+0x124,(0,2,100)[(scenario+i)%3])
            mode=(scenario+i)%5
            if mode:
                put(m+0x44,1)
                if mode>1:
                    table=heap+0x8000+i*0x400;bucket=table+0x40
                    put(m+0x34,table,1,0,mode-1);put(table,bucket)
                    for j in range(mode-1):
                        state=table+0x100+j*0x40;put(bucket+j*4,state);put(state+0x28,2 if (scenario+i)%2 else 100)
    def snapshot():
        out=struct.pack('<I',get(zone+0x15c))
        head=get(zone+0x13c);current=get(head+4);order=[]
        while current!=head:
            order.append(ident(current-0x6c));current=get(current+4)
            if len(order)>4: raise RuntimeError('Bad original zone list')
        out+=bytes([len(order),*order])
        for i in range(4):
            m=mobs+i*0x2000;bound=get(m+0x60)!=0
            out+=bytes([bound])+bytes(cpu.mem_read(m+0x64,8))+bytes(cpu.mem_read(m+0xc88,12))
            cpu.reg_write(UC_X86_REG_EAX,zone);out+=bytes([ident(call(0x416800,(get(m+0x5c),)))])
        for z in range(3):
            for x in range(3):
                c=cell(x,z);head=get(c+0x2c);current=get(head+0x4c);order=[]
                while current!=head+0x48:
                    order.append(ident(current-0x48));current=get(current+4)
                    if len(order)>4: raise RuntimeError('Bad original cell list')
                if get(c+0x4c)!=len(order): raise RuntimeError('Original cell count mismatch')
                out+=bytes([len(order),*order])
                for i in range(4):
                    cpu.reg_write(UC_X86_REG_EDI,c);out+=bytes([ident(call(0x423680,(get(mobs+i*0x2000+0x5c),)))])
        return out
    print(f'# ps_game.exe SHA256 {digest}; real EnterMob 00416860, LeaveMob 00416990, AddMob 00416700, RemoveMob 00416790, FindMob 00416800, CCell::FindMob 00423680, SendCellInfoEnter 00423aa0, SendCellInfoLeave 00424130. Only allocator, lock, Lua and transport ports.')
    print('# S scenario initializes four mobs; E/L object membership; V/W x z reentrant snapshots; expected: result BYTE, event length/events(tag BYTE,length DWORD,payload), registry/list/lookup/binding snapshot.')
    for scenario in range(128):
        initialize(scenario);print('S',scenario,sep='\t')
        ops=[('E',i) for i in range(4)]+[('V',0,0,0),('W',0,0,0),('V',0,0,1),('W',0,0,1),('L',1),('V',0,0,0),('L',0),('E',1),('V',0,0,0),('W',2,0,0),('L',1),('L',2)]
        for op in ops:
            context['events']=b''
            if op[0] in ('E','L'):
                m=mobs+op[1]*0x2000;cpu.reg_write(UC_X86_REG_EAX,m)
                if op[0]=='E': result=call(0x416860,(zone,))&255
                else:
                    cpu.reg_write(UC_X86_REG_ECX,zone);result=call(0x416990)&255
            else:
                c=cell(op[1],op[2]);context.update(cell=c,reenter=bool(op[3]))
                if op[0]=='V':
                    cpu.reg_write(UC_X86_REG_EDI,c);cpu.reg_write(UC_X86_REG_ECX,heap+0xf000);call(0x423aa0)
                else:
                    cpu.reg_write(UC_X86_REG_ESI,c);call(0x424130,(heap+0xf000,))
                result=0
            output=bytes([result])+struct.pack('<I',len(context['events']))+context['events']+snapshot()
            print(*op,output.hex(),sep='\t')


if __name__=='__main__':
    main()
