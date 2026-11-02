#!/usr/bin/env python3
"""Execute original complete CZone::EnterUser admission/packet control.

Ghidra work/zone-admission and work/map-admission. Cell/list membership, view,
shape/speed, cell contents, weather, obelisks and EnterZone are boundary ports.
Original IsSafePos, cell arithmetic, field writes, 0201 bytes and ordering run.
"""
import itertools
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,user,zone,map_ptr,rows,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
    for at in (stack,user,zone,map_ptr,rows,stop):cpu.mem_map(at,0x10000)
    cpu.reg_write(UC_X86_REG_FPCW,0x27f)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
    def signed(value):return value if value<0x80000000 else value-0x100000000
    state={};events=[]
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x423560:events.append('ATTACH '+str(signed(word(user+0xe4)))+' '+str(signed(word(user+0xe8))))
        elif address==0x4208c0:
            ptr,length=word(sp+8),word(sp+12);events.append('VIEW '+str(signed(machine.reg_read(UC_X86_REG_ECX)))+' '+str(signed(machine.reg_read(UC_X86_REG_EAX)))+' '+bytes(cpu.mem_read(ptr,length)).hex())
        elif address==0x48a300:events.append('SHAPE');machine.reg_write(UC_X86_REG_EAX,state['shape']&0xffffffff)
        elif address==0x482590:events.append('SEND_SHAPE')
        elif address==0x4827b0:events.append('SEND_SPEED')
        elif address==0x41f290:events.append('CELL_INFO '+str(signed(word(user+0xe4)))+' '+str(signed(word(user+0xe8))))
        elif address==0x4201b0:events.append('WEATHER '+str(word(sp+8)))
        elif address==0x42b040:events.append('OBELISKS')
        elif address==0x44ae30:events.append('ENTER_ZONE')
        elif address==0x4d9510:events.append('DIAGNOSTIC')
        elif address==0x4d43a0:events.append('CLOSE '+str(word(sp+4)))
    for at,cleanup in ((0x423560,0),(0x415e30,0),(0x4208c0,12),(0x48a300,0),(0x482590,0),(0x4827b0,0),(0x41f290,8),(0x4201b0,8),(0x42b040,4),(0x44ae30,0),(0x4d9510,0),(0x4d43a0,8)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    put(zone+0x24,map_ptr);put(zone+0x30,rows)
    for i in range(128):put(rows+4*i,rows+0x1000)
    rng=random.Random(0x415f80);cases=[]
    for size in (0,1,40,81,1024):
        points=(0,-0.01,size-0.000244140625,float(size),40,float('nan'),float('inf'),float('-inf'))
        for x,z in itertools.product(points,repeat=2):cases.append((size,bits(x),bits(z),size//40+1,size//40+1,rng.choice((0,1,2)),rng.choice((0,2,3,-1)),rng.choice((0,10,11,255)),rng.choice((0,13,102))))
    for dungeon,mode,permission,shape in itertools.product((0,1,2),(0,1,2,3,-1),(0,10,11,255),(0,-1,13,102)):
        cases.append((81,bits(40),bits(79),3,3,dungeon,mode,permission,shape))
    for _ in range(1024):
        size=rng.choice((1,40,81,1024));width,height=(rng.choice((0,1,size//40+1,-1)) for _ in range(2));cases.append((size,bits(rng.uniform(-5,size+5)),bits(rng.uniform(-5,size+5)),width,height,rng.choice((0,1,2)),rng.choice((0,1,2,3,-1)),rng.randrange(256),rng.choice((0,13,102))))
    print(f'# ps_game.exe SHA256 {digest}; CZone::EnterUser 00415f80..0041619e, CMap::IsSafePos 00415360; membership/scene/time/transport are ports')
    for size,x,z,width,height,dungeon,mode,permission,shape in cases:
        cid,partner=(rng.getrandbits(32) for _ in range(2));direction,map_id=(rng.getrandbits(16) for _ in range(2));level=signed(rng.getrandbits(32));y=rng.choice((bits(-17.5),0x7fa01234,0x80000000));bound=rng.choice((False,True));cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));put(map_ptr+4,size);put(map_ptr+0x78,map_id);put(map_ptr+0x80,dungeon);put(zone+0x28,width,height);put(user+0xd0,x,y,z);put(user+0x128,cid);cpu.mem_write(user+0x5768,bytes([permission]));cpu.mem_write(user+0x164,struct.pack('<HH',31337,direction));put(user+0xe4,-7,-9);put(user+0xe0,0x87654321 if bound else 0);put(user+0x1460,mode);put(user+0x1470,partner);put(user+0x1748,level);put(stack+0x8000,stop,zone);events.clear();state['shape']=shape
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EDI,user);cpu.emu_start(0x415f80,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original zone entry did not return')
        print('CASE',size,x,y,z,width,height,map_id,dungeon,cid,direction,permission,mode,partner,level,shape,int(bound))
        print('RETURN',cpu.reg_read(UC_X86_REG_EAX)&255,struct.unpack('<H',cpu.mem_read(user+0x164,2))[0],signed(word(user+0xe4)),signed(word(user+0xe8)),signed(word(user+0x1460)),int(bool(word(user+0xe0))))
        for event in events:print('EVENT',event)
        print('END')


if __name__=='__main__':main()
