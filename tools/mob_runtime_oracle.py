#!/usr/bin/env python3
"""Execute original LoadConstMob, CMob::Init, SetAttack and InitAttack.

Ghidra work/mob-definitions and work/world-object-startup. Full definition and
runtime normalization/attack bodies execute. SQL, allocator, clock/random,
verified path resets, compiled/Lua AI providers and OS helpers are ports.
"""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_original

# Canonical selected fields, retaining original byte widths/order. Pointers are
# compared separately as bindings rather than process-specific addresses.
FIELDS=((0x5c,4),(0x220,2),(0x224,4),(0x228,2),(0x22a,2),(0x22c,4),(0x230,4),(0x234,4),(0x238,4),(0x124,4),(0x128,4),(0x12c,4),(0x134,84),(0x188,48),(0x1b8,12),(0x23c,4),(0x240,4),(0x130,4),(0x250,4),(0x254,4),(0x258,1),(0x25c,4),(0x260,4),(0x264,1),(0x268,16),(0x278,4),(0xccc,4),(0xcc0,4),(0xcb4,4),(0xcb8,4),(0xcbc,4),(0xcc4,4),(0xcc8,4),(0xcac,1),(0xcb0,4),(0xcd4,12),(0xce0,12),(0xcec,4),(0xcf0,4),(0x248,4),(0x244,4),(0x24c,4),(0xcf4,32),(0xd14,32),(0x2b0,4),(0x298,4),(0x2b8,4),(0x50,12),(0xc88,12),(0xc94,12),(0xca0,12),(0x280,4),(0x284,4),(0x288,4),(0x28c,4),(0x290,4),(0x294,4),(0x29c,4),(0x2a0,4),(0x2a4,12),(0x2b4,4),(0xcd0,4),(0xa0,1),(0x64,8))


def main():
    cpu,digest=load_original('ps_game');stack,mob,infos,database,stop,game=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000,0x35000000
    for at in (stack,mob,infos,database,stop):cpu.mem_map(at,0x10000)
    cpu.mem_map(game,0xb00000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    state={};events=[]
    columns=(1,3,5,9,10,11,12,13,14,17,18,19,63,68,4)
    def hook(m,address,size,context):
        sp=m.reg_read(UC_X86_REG_ESP)
        if address==0x4d3c40:m.reg_write(UC_X86_REG_EAX,database)
        elif address==0x503ae3:m.reg_write(UC_X86_REG_EAX,infos)
        elif address==0x4e2050:state['query']+=1;state['row']=-1;m.reg_write(UC_X86_REG_EAX,0)
        elif address==0x4e2090:state['row']+=1;m.reg_write(UC_X86_REG_EAX,0 if state['row']==0 else 100)
        elif address==0x4e15b0:
            column,n,kind,at=word(sp+4),word(sp+8),word(sp+12),word(sp+16);kind=kind if kind<0x80000000 else kind-0x100000000
            value=63 if state['query']==0 else state['values'].get(column,0)
            if value is not None:
                if kind==1:m.mem_write(at,b'Mob\0')
                else:
                    width={4:4,5:2,-6:1}[kind];m.mem_write(at,(value&((1<<(width*8))-1)).to_bytes(width,'little'))
            m.reg_write(UC_X86_REG_EAX,0)
        elif address==0x4027e0:m.reg_write(UC_X86_REG_EAX,0)
        elif address==0x504697:events.append('RANDOM '+str(state['random']));m.reg_write(UC_X86_REG_EAX,state['random']&0xffffffff)
        elif address==stop+0x100:
            value=state['ticks'][state['clock']];state['clock']+=1;events.append('TICK '+str(value));m.reg_write(UC_X86_REG_EAX,value)
        elif address==0x4a9130:
            events.append('AI '+str(word(sp+4))+' '+str(state['ai_ok']));put(mob+0x124,17);put(mob+0x2b0,0x1234);put(mob+0x298,33);put(mob+0x2b8,99);m.reg_write(UC_X86_REG_EAX,state['ai_ok'])
        elif address==0x4b4c20:
            events.append('LUA');put(mob+0x284,1);put(mob+0x290,1);put(mob+0x2b0,0x76543210);put(mob+0x298,5);put(mob+0x2b8,99)
    for at,cleanup in ((0x4d3c40,0),(0x4d3d00,4),(0x503ae3,0),(0x4e13f0,0),(0x4e2050,4),(0x4e2090,0),(0x4e15b0,20),(0x4027e0,0),(0x4d9510,0),(0x505316,0),(0x48ec70,4),(0x48f600,0),(0x504697,0),(stop+0x100,0),(0x4a9130,4),(0x4b4c20,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    put(0x5210e4,stop+0x100)
    def call(at,args=(),regs=()):
        cpu.mem_write(stack,bytes(0x10000));put(stack+0xf000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_FPCW,0x027f)
        for reg,value in regs:cpu.reg_write(reg,value)
        cpu.emu_start(at,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError(f'Original mob owner did not return {at:08x}')
        return cpu.reg_read(UC_X86_REG_EAX)
    def snapshot():
        print('STATE',b''.join(bytes(cpu.mem_read(mob+offset,n)) for offset,n in FIELDS).hex());print('BIND',int(word(mob+0xc84)!=0),int(word(mob+0x60)!=0))
    rng=random.Random(0x490040)
    print(f'# ps_game.exe SHA256 {digest}; LoadConstMob 004c1720, CMob::Init 00490040, SetAttack 004902f0, InitAttack 00490290; SQL/clock/random/path/AI ports')
    for case in range(384):
        raw=[case%63+1,rng.choice((1,38,65535)),(0,9,10,99,100,255)[case%6],rng.choice((0,-1,2765,rng.randrange(1,2147483648))),*[rng.choice((0,1,65535,rng.randrange(65536))) for _ in range(5)],rng.choice((0,4,5,8,255)),rng.randrange(65536),rng.randrange(65536),rng.choice((0,-1,2147483647)),(case%18 if case%18<17 else 255)]
        raw.append(rng.choice((0,1,704,65535,None)))
        if case%17==0:raw[rng.randrange(2,len(raw))]=None
        state.update(query=-1,row=-1,values=dict(zip(columns,raw)),random=rng.choice((0,1,32767,59999,60000,-1,-2147483648)),ticks=[rng.getrandbits(32),rng.getrandbits(32)],clock=0,ai_ok=case%2)
        events.clear();returned=call(0x4c1720,regs=((UC_X86_REG_ECX,game),))
        if not returned&255:raise RuntimeError('Original full mob loader rejected synthetic row')
        info=word(game+0xa81808)+raw[0]*268;projection=[int.from_bytes(cpu.mem_read(info+o,n),'little') for o,n in ((0,2),(34,2),(38,1),(48,4),(52,2),(54,2),(56,2),(58,2),(60,2),(64,1),(66,2),(68,2),(112,4),(108,4),(183,1),(36,2))]
        seed=(case*17+0xa5)&255;cpu.mem_write(mob,bytes([seed])*3380);put(mob+0xa8,info);type_id=rng.getrandbits(32);respawn=database+0x4000 if case%2 else 0
        print('CASE',case,seed,type_id,int(bool(respawn)),state['random'],*state['ticks'],state['ai_ok']);print('ROW',*['N' if v is None else v for v in raw]);print('DEF',*projection)
        events.clear();call(0x490040,(type_id,respawn),((UC_X86_REG_ESI,mob),))
        for event in events:print('EVENT',event)
        snapshot()
        additions=[rng.choice((0,-1,2147483647,-2147483648,16777217,rng.randrange(-100000,100000))) for _ in range(12)];wis,luc=[rng.choice((0,-1,16777217,2147483647,-2147483648,rng.randrange(-100000,100000))) for _ in range(2)];scale=rng.choice((0,0x3f800000,0x3f000000,0x3dcccccd,0xbf800000,0x7f800000,0x7fc00000));disabled=[case*3+i for i in range(3)]
        cpu.mem_write(mob+0x188,struct.pack('<12i',*additions));put(mob+0x230,wis);put(mob+0x234,luc);put(mob+0x270,scale)
        for i,value in enumerate(disabled):put(mob+0x134+28*i,value)
        print('ATTACK',wis,luc,scale,*additions,*disabled);call(0x4902f0,regs=((UC_X86_REG_ECX,mob),));snapshot();print('RESET_ATTACK');call(0x490290,regs=((UC_X86_REG_EDX,mob),));snapshot();print('END')


if __name__=='__main__':main()
