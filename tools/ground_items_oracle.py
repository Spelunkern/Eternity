#!/usr/bin/env python3
"""Original zone/cell item admission, lookup, removal, strict expiry and visibility."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu, digest = load_game()
    stack,user,objects,items,zones,cells,roots,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x10000000)
    for address in (stack,user,objects,items,stop):
        cpu.mem_map(address,0x10000)
    for address in (zones,cells,roots):
        cpu.mem_map(address,0x20000)
    cpu.reg_write(UC_X86_REG_FPCW,0x27f)
    cpu.mem_write(stop+0x100,b'\xc2\x04\x00');cpu.mem_write(stop+0x200,b'\xc3')
    for iat in (0x5210cc,0x5210d0):
        cpu.mem_write(iat,struct.pack('<I',stop+0x100))
    cpu.mem_write(0x5210e4,struct.pack('<I',stop+0x200));cpu.mem_write(0x546f3c,struct.pack('<I',objects))
    for address,cleanup in ((0x418ca0,4),(0x417f90,4),(0x419010,0),(0x4208c0,12),(0x4d4d20,8)):
        cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
    counts=(5,9);registry=[{},{}];events=[];state={'tick':0};released=set();seen=0

    def write(address,*values):
        cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))

    def word(address):
        return struct.unpack('<I',cpu.mem_read(address,4))[0]

    def handle(pointer):
        return (pointer-items)//256 if pointer else -1

    def cell(zone,x,z):
        return cells+zone*0x10000+(z*counts[zone]+x)*292

    for zone,count in enumerate(counts):
        base=zones+zone*0x10000;root=roots+zone*0x10000+0xe000
        write(base+0x28,count,count,base+0x1000);write(root,root,root);write(base+0x194,root,root);write(base+0x100,roots+zone*0x10000+0xf000)
        for z in range(count):
            write(base+0x1000+4*z,cell(zone,0,z))
            for x in range(count):
                root=roots+zone*0x10000+(z*count+x)*16;write(root,root,root);write(cell(zone,x,z)+0x84,root,root)
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+0x200:
            events.append('T');machine.reg_write(UC_X86_REG_EAX,state['tick'])
        elif address==0x418ca0:
            zone=(machine.reg_read(UC_X86_REG_EAX)-zones-0xfc)//0x10000;pair=machine.reg_read(UC_X86_REG_EBX)
            registry[zone].setdefault(word(pair),word(pair+4))
        elif address==0x417f90:
            zone=(machine.reg_read(UC_X86_REG_ESI)-zones-0xe4)//0x10000;registry[zone].pop(word(word(sp+4)),None)
        elif address==0x419010:
            zone=(machine.reg_read(UC_X86_REG_ECX)-zones-0xfc)//0x10000;identity=word(machine.reg_read(UC_X86_REG_EBX));output=machine.reg_read(UC_X86_REG_EAX)
            target=registry[zone].get(identity,0);node=roots+zone*0x10000+(0xf100 if target else 0xf000);write(node+0x10,target);write(output,node)
        elif address==0x4208c0:
            zone=(word(sp+4)-zones)//0x10000;ptr,length=word(sp+8),word(sp+12);identity=word(ptr+2)
            item=state['active'] if length==22 else next(items+i*256 for i in range(seen) if word(items+i*256+0x14)==identity and word(items+i*256+0x18)==0 and word(items+i*256+0x1c)==machine.reg_read(UC_X86_REG_ECX) and word(items+i*256+0x20)==machine.reg_read(UC_X86_REG_EAX))
            events.append(f'V{zone},{machine.reg_read(UC_X86_REG_ECX)},{machine.reg_read(UC_X86_REG_EAX)},{int(word(item+0x18)!=0)},{handle(registry[zone].get(identity,0))}:'+bytes(cpu.mem_read(ptr,length)).hex())
        elif address==0x421f14:
            item=machine.reg_read(UC_X86_REG_ESI)
            if any(word(item+offset) for offset in (0x18,0x1c,0x20)):
                raise RuntimeError('Expired item released before detach')
            released.add(handle(item));events.append('F'+str(handle(item)))
        elif address==0x4d4d20:
            ptr,length=word(sp+4),word(sp+8);events.append('S'+bytes(cpu.mem_read(ptr,length)).hex())
    for address in (stop+0x200,0x418ca0,0x417f90,0x419010,0x4208c0,0x421f14,0x4d4d20):
        cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)

    def call(entry,args=(),regs=(),end=stop):
        write(stack+0x8000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
        for reg,value in regs:
            cpu.reg_write(reg,value)
        try:
            cpu.emu_start(entry,end,count=100000)
        except Exception as error:
            raise RuntimeError(f'Item oracle fault at {cpu.reg_read(UC_X86_REG_EIP):08x}, entry {entry:08x}') from error
        if cpu.reg_read(UC_X86_REG_EIP)!=end:
            raise RuntimeError('Original item operation did not return')
        return cpu.reg_read(UC_X86_REG_EAX)
    rng=random.Random(0x417320)
    def enter(zone,index,identity,x=10.0,z=10.0,tick=123,duration=60000,custom=0):
        bits=[struct.unpack('<I',struct.pack('<f',value))[0] for value in (x,rng.uniform(-20,20),z)]
        return ['E',zone,index,identity,*bits,rng.randrange(256),rng.randrange(256),rng.randrange(256),rng.choice((0,1,2,255)),tick,custom,duration]
    fixed=[enter(0,0,9001,tick=0),enter(0,1,9001,tick=0),['F',0,9001],['L',0,1],['F',0,9001],['Q',0,0,0,9001],['I',0,0,0],['O',0,0,0],['U',0,60000],['U',0,60001],
           enter(1,2,9002,x=359.9,z=359.9,tick=0xfffffff0),['U',1,0xfffffff0],enter(0,3,9003,x=200.0),enter(0,3,9003,x=-0.25,z=-0.25,tick=100,duration=0,custom=1),['U',0,100],['U',0,101]]
    print(f'# ps_game.exe SHA256 {digest}; zone AddItem/RemoveItem/FindItem, both EnterItem overloads, LeaveItem, UpdateItem, cell lists/find and visibility; original intrusive code and packets, tree/transport/clock ports isolated')
    print('# command; result; seen item count, each item(owner cellX cellZ expiry id XYZbits type typeID count freed); two zone indexes(count,ID/handle pairs); events')
    for step in range(640):
        if step<len(fixed):
            command=fixed[step]
        else:
            op=rng.choice(('E','E','L','F','Q','I','O','U'));zone=rng.randrange(2)
            attached=[i for i in range(seen) if word(items+i*256+0x18)]
            if op=='L' and not attached:
                op='E'
            if op=='E':
                candidates=[i for i in range(seen) if not word(items+i*256+0x18) and i not in released]
                index=rng.choice(candidates) if candidates and rng.randrange(2) else seen
                command=enter(zone,index,rng.randrange(9000,9012),rng.choice((-0.25,0.0,39.9,40.0,159.9,199.9,200.0,359.9,360.0)),rng.choice((-0.25,10.0,40.0,199.9)),rng.choice((0,100,60000,60001,0xfffffff0)),rng.choice((0,1,30000,60000,0xffffffff)),rng.randrange(2))
            elif op=='L':
                index=rng.choice(attached);command=['L',(word(items+index*256+0x18)-zones)//0x10000,index]
            elif op=='U':
                command=['U',zone,rng.choice((0,100,60000,60001,120000,0xfffffff0,0xffffffff))]
            elif op=='F':
                command=['F',zone,rng.randrange(9000,9012)]
            else:
                command=[op,zone,rng.randrange(3),rng.randrange(3)]+([rng.randrange(9000,9012)] if op=='Q' else [])
        events.clear();op=command[0];result=0
        if op=='E':
            _,zone,index,identity,x,y,z,kind,type_id,count,flag,tick,custom,duration=command
            if index==seen:
                seen+=1;write(items+index*256+0x2c,items+index*256+8)
            item=items+index*256;state.update(active=item,tick=tick);write(item+8,x,y,z,identity);cpu.mem_write(item+0x40,bytes((kind,type_id,count)))
            result=call(0x417210 if custom else 0x417320,(zones+zone*0x10000,flag,*([duration] if custom else [])),((UC_X86_REG_EAX,item),))&255
        elif op=='L':
            _,zone,index=command;item=items+index*256;state['active']=item
            result=call(0x417440,regs=((UC_X86_REG_EAX,item),(UC_X86_REG_EBX,zones+zone*0x10000)))&255
        elif op=='U':
            _,zone,tick=command;call(0x421e20,(zones+zone*0x10000,tick))
        elif op=='F':
            _,zone,identity=command;result=handle(call(0x4171b0,(identity,),((UC_X86_REG_EAX,zones+zone*0x10000),)))
        elif op=='Q':
            _,zone,x,z,identity=command;result=handle(call(0x4238a0,(identity,),((UC_X86_REG_EDI,cell(zone,x,z)),)))
        elif op=='I':
            _,zone,x,z=command;call(0x424018,regs=((UC_X86_REG_EDI,cell(zone,x,z)),(UC_X86_REG_EBX,user),(UC_X86_REG_EBP,stop+0x100)),end=0x4240a7)
        else:
            _,zone,x,z=command;call(0x4242be,regs=((UC_X86_REG_ESI,cell(zone,x,z)),(UC_X86_REG_EBP,user),(UC_X86_REG_EBX,stop+0x100)),end=0x42431c)
        snapshots=[]
        for index in range(seen):
            item=items+index*256;owner=word(item+0x18)
            snapshots.extend(((owner-zones)//0x10000 if owner else -1,word(item+0x1c),word(item+0x20),word(item+0x6c),word(item+0x14),*struct.unpack('<3I',cpu.mem_read(item+8,12)),*cpu.mem_read(item+0x40,3),int(index in released)))
        print(*command,result,seen,*snapshots,*[v for zone in range(2) for v in (len(registry[zone]),*[v for identity,pointer in sorted(registry[zone].items()) for v in (identity,handle(pointer))])],len(events),*events,sep='\t')


if __name__=='__main__':
    main()
