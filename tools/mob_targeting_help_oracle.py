#!/usr/bin/env python3
"""Execute original help selection and cell cursor loops; SetTarget is observed."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x20000000;stack=base;source=base+0x10000;user=base+0x20000;zone=base+0x30000;grid=base+0x40000;data=base+0x50000;stop=base+0x80000
    cpu.mem_map(base,0x90000)
    # Keep real CRT sqrt/error paths; supply isolated Windows thread data.
    cpu.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def floats(at,*v):cpu.mem_write(at,struct.pack('<'+'f'*len(v),*v))
    for at,n in ((stop+16,4),(stop+32,4),(0x49fad0,8)):cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
    put(0x5210cc,stop+16);put(0x5210d0,stop+32)
    ctx={}
    def hook(c,at,size,_):
        sp=c.reg_read(UC_X86_REG_ESP)
        if at in (stop+16,stop+32):
            offset=get(sp+4)-grid-0x100-0x34;z=offset//0x1000;x=(offset%0x1000)//292
            ctx['events']+=[('L' if at==stop+16 else 'U')+str(z*3+x)]
        elif at==0x49fad0:
            target=c.reg_read(UC_X86_REG_ECX);i=(target-data)//0x1000
            if get(sp+4)!=user or get(sp+8)!=0:raise RuntimeError('Wrong SetTarget arguments')
            ctx['events']+=['A'+str(i)];put(target+0x124,1)
            if ctx['mutate']:
                floats(source+0x50,8,0,0);cpu.mem_write(source+0x220,struct.pack('<H',9));cpu.mem_write(data+0x9000+0x69,b'\x00')
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; FindHelpNormal 0049ff70; FindHelpNormal2 004a0200; FindHelpAttack 004a0470. Original cell loops/status/distance/sqrt. Ports: locks, CRT TLS and observed SetTarget(false).')
    print('# S kind zone_user_count width height cell_x cell_z country user_country life hp attackable visible radius mutate; C index status fsm type ai x_bits z_bits; O lock/target events')
    for case in range(1536):
        kind=(2,3,5)[case%3];world=(0,1,-1,2)[(case//3)%4] if case<192 else 1
        width=(1,2,3)[(case//5)%3];height=(1,2,3)[(case//7)%3];cx=(case//11)%width;cz=(case//13)%height
        country=(case//17)%3;ucountry=(case//19)%3;life=(case//23)%3 if case<768 else 0;hp=(case//29)%2 if case<768 else 0
        attackable=(case//31)%2 if case<768 else 1;visible=(case//37)%2 if case<768 else 1;radius=(0,1,5,8,255)[(case//41)%5];mutate=(case//43)%2
        cpu.mem_write(source,bytes(0x1000));cpu.mem_write(zone,bytes(0x1000));cpu.mem_write(data,bytes(0x10000));cpu.mem_write(grid,bytes(0x4000));put(source+0x60,zone,cx,cz);put(source+0xa8,data+0x9000);put(data+0x9078,country);cpu.mem_write(data+0x9069,bytes([radius]));cpu.mem_write(source+0x220,struct.pack('<H',7));floats(source+0x50,0,0,0)
        put(zone+0x130,world);put(zone+0x28,width,height,grid);put(grid,grid+0x100,grid+0x1100,grid+0x2100)
        cpu.mem_write(user+0x12d,bytes([ucountry]));put(user+0x142c,life);cpu.mem_write(user+0x1366,bytes([hp]));cpu.mem_write(user+0x5775,bytes([attackable]));cpu.mem_write(user+0x5774,bytes([visible]))
        print('S',kind,world,width,height,cx,cz,country,ucountry,life,hp,attackable,visible,radius,mutate,sep='\t')
        for i in range(9):
            c=data+i*0x1000;info=c+0x400;status=(0,0,1,4,100)[(case+i)//3%5];fsm=(case+i)%3;mobtype=(7,9,8)[(case+i)//5%3];ai=(0,1,2,3,5)[(case+i)//7%5]
            x,z=((0,0),(3,4),(5,0),(8,0),(float('nan'),0),(float('inf'),0),(-3,-4),(0,1),(0,255))[(case+i)%9]
            cell=grid+0x100+(i//3)*0x1000+(i%3)*292;head=grid+0x3300+i*128
            put(cell+0x2c,head);put(head+0x4c,c+0x48);put(c+0x4c,head+0x48)
            put(c+0x124,status if fsm==0 else 42);put(c+0xa8,info);cpu.mem_write(c+0x220,struct.pack('<H',mobtype));cpu.mem_write(info+0x26,bytes([ai]));floats(c+0x50,x,0,z)
            if fsm:
                put(c+0x44,1);put(c+0x34,info+0x100);put(c+0x38,0,0,1 if fsm==1 else 0);put(info+0x100,info+0x110);put(info+0x110,info+0x120);put(info+0x148,status)
            bits=struct.unpack('<2I',struct.pack('<2f',x,z));print('C',i,status,fsm,mobtype,ai,*bits,sep='\t')
        ctx.update(events=[],mutate=mutate)
        if kind==2:put(stack+0xf000,stop,0,user)
        else:put(stack+0xf000,stop,source,user)
        cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,source);cpu.reg_write(UC_X86_REG_FPCW,0x27f)
        try:cpu.emu_start({2:0x49ff70,3:0x4a0470,5:0x4a0200}[kind],stop,count=50000)
        except Exception as e:raise RuntimeError(f'Case {case} EIP {cpu.reg_read(UC_X86_REG_EIP):08x}: {e}') from e
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original did not return')
        print('O',' '.join(ctx['events']),sep='\t')
if __name__=='__main__':main()
