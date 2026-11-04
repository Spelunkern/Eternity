#!/usr/bin/env python3
"""Original first-user search and AI gate; real GetStatus/SetTarget for acquisitions."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    c,digest=load_game();b=0x20000000;stack=b;mob=b+0x10000;zone=b+0x20000;grid=b+0x30000;info=b+0x40000;users=b+0x50000;stop=b+0xa0000
    c.mem_map(b,0xb0000)
    def put(at,*v):c.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',c.mem_read(at,4))[0]
    def floats(at,*v):c.mem_write(at,struct.pack('<'+'f'*len(v),*v))
    c.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
    for at in (stop+16,stop+32):c.mem_write(at,b'\xc2\x04\x00')
    put(0x5210cc,stop+16);put(0x5210d0,stop+32)
    for at in (0x49ff70,0x4a0470,0x4a0200):c.mem_write(at,b'\xc2\x08\x00')
    ctx={}
    def hook(cpu,at,size,_):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if at in (stop+16,stop+32):
            off=get(sp+4)-grid-0x100-8;z=off//0x1000;x=off%0x1000//292;ctx['events'].append(('L' if at==stop+16 else 'U')+str(z*3+x))
            if at==stop+16 and ctx['mutate']:
                # World-lock port makes a live definition/position update before visiting.
                c.mem_write(info+0x69,b'\x05');floats(mob+0x50,3,0,4)
        elif at in (0x49ff70,0x4a0470,0x4a0200):ctx['events'].append('H'+str({0x49ff70:2,0x4a0470:3,0x4a0200:5}[at]))
        elif at==0x49fad0:
            if cpu.reg_read(UC_X86_REG_ECX)!=mob or get(sp+8)!=1:raise RuntimeError('Wrong SetTarget owner/initialize flag')
            p=get(sp+4);ctx['events'].append('A'+str((p-users)//0x6000+1))
    c.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; Targeting 0049fcf0, FindTarget 0049fd20; real SetTarget 0049fad0/GetStatus 0041d760 and CRT sqrt. Ports: user cell locks, CRT TLS and help bodies (observed). Attacks disabled.')
    print('# S entry ai count width height cx cz radius status fixed mutate; C index cell life shape hp attackable visible country x_bits z_bits; O return_token_or_bool target_kind target_id status nextmove | events')
    for case in range(2048):
        entry=case%2;ai=(0,1,2,3,5)[case//2%5];count=(0,1,2)[case//10%3];width=(1,2,3)[case//30%3];height=(1,2,3)[case//90%3];cx=(case//11)%width;cz=(case//13)%height;radius=(0,1,5,255)[case//7%4];status=(0,1,4,100)[case//17%4];fixed=(case//19)%2;mutate=(case//23)%2
        c.mem_write(mob,bytes(0x1000));c.mem_write(zone,bytes(0x1000));c.mem_write(grid,bytes(0x4000));c.mem_write(info,bytes(0x1000));c.mem_write(users,bytes(12*0x6000))
        put(mob+0x60,zone,cx,cz);put(mob+0xa8,info);put(mob+0x124,status,7,12345);put(mob+0x2b8,fixed);put(mob+0xcbc,777);c.mem_write(info+0x26,bytes([ai]));c.mem_write(info+0x69,bytes([radius]));put(info+0x78,0)
        put(zone+0x130,count);put(zone+0x28,width,height,grid);put(grid,grid+0x100,grid+0x1100,grid+0x2100)
        # Two users in each of six cell slots, with all nine cell heads valid.
        for cellindex in range(9):
            cell=grid+0x100+(cellindex//3)*0x1000+(cellindex%3)*292;head=grid+0x3300+cellindex*64;put(cell,head,head);put(head+8,head+8,head+8)
        print('S',entry,ai,count,width,height,cx,cz,radius,status,fixed,mutate,sep='\t')
        bins={i:[] for i in range(9)}
        for i in range(12):
            cellindex=(i//2+case//31)%9;p=users+i*0x6000;bins[cellindex].append(p)
            life=(0,1,2)[(case+i)//3%3] if case<1024 else 0;shape=(case+i)//5%2 if case<1024 else 0;hp=(case+i)//7%2 if case<1024 else 0;attack=(case+i)//11%2 if case<1024 else 1;visible=(case+i)//13%2 if case<1024 else 1;country=(case+i)%2
            x,z=((0,0),(3,4),(5,0),(5.000000476837158,0),(4.999999523162842,0),(float('nan'),0),(float('inf'),0),(0,255),(-3,-4),(1,0),(1e20,1e20),(0,1))[(case+i)%12]
            put(p+0x142c,life);put(p+0xdc,100+i);c.mem_write(p+0x1354,bytes([shape]));c.mem_write(p+0x1366,bytes([hp]));c.mem_write(p+0x5774,bytes([visible,attack]));c.mem_write(p+0x12d,bytes([country]));floats(p+0xd0,x,0,z)
            bits=struct.unpack('<2I',struct.pack('<2f',x,z));print('C',i,cellindex,life,shape,hp,attack,visible,country,*bits,sep='\t')
        for ci,items in bins.items():
            head=grid+0x3300+ci*64;links=[head+8]+[p+8 for p in items]+[head+8]
            put(head+8,links[-2],links[1])
            for j,p in enumerate(items):put(p+8,links[j],links[j+2])
        ctx.update(events=[],mutate=mutate)
        put(stack+0xf000,stop);c.reg_write(UC_X86_REG_ESP,stack+0xf000);c.reg_write(UC_X86_REG_EAX,mob);c.reg_write(UC_X86_REG_EDI,mob);c.reg_write(UC_X86_REG_FPCW,0x27f)
        c.emu_start(0x49fcf0 if entry else 0x49fd20,stop,count=50000)
        if c.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original find did not return')
        result=c.reg_read(UC_X86_REG_EAX)
        result=(result&255) if entry else (result-users)//0x6000+1 if result else 0
        print('O',result,get(mob+0x128),get(mob+0x12c),get(mob+0x124),get(mob+0xcbc),'|',' '.join(ctx['events']),sep='\t')
if __name__=='__main__':main()
