#!/usr/bin/env python3
"""Complete SendUseSPMP, absent party; actual writes and self transport ordering."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000);c.mem_map(0,0x10000)
    user,stack,stop=base,base+0x60000,base+0x7f000;state={}
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def hook(cpu,at,size,data):
        if at==0x481dbb:state['requested']=(cpu.reg_read(UC_X86_REG_EBP),cpu.reg_read(UC_X86_REG_EDI))
        elif at==0x4d4d20:
            esp=cpu.reg_read(UC_X86_REG_ESP)
            state['packet']=bytes(cpu.mem_read(get(esp+4),get(esp+8))).hex()
            state['sent']=(get(user+0x1240),get(user+0x123c))
            cpu.reg_write(UC_X86_REG_EIP,get(esp));cpu.reg_write(UC_X86_REG_ESP,esp+12)
    c.hook_add(UC_HOOK_CODE,hook)
    def call(at,args=()):
        put(stack,stop,*args);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ESI,user)
        c.emu_start(at,stop,count=100000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4+4*len(args):raise RuntimeError('unbalanced original')
    c.reg_write(UC_X86_REG_FPCW,0x37f);call(0x51136e);control=c.reg_read(UC_X86_REG_FPCW)
    print(f'# ps_game.exe SHA256 {digest}; whole CUser::SendUseSPMP 00481ca0; actual CRT precision53; party null. Transport observes already committed resources.')
    print('# case sp mp sp_cost mp_cost sp_reduction mp_reduction final_sp final_mp requested_sp requested_mp packet')
    rng=random.Random(0x481ca0);resources=(0,1,29,65535,65536,0x7fffffff,0x80000000,0xffffffff);costs=(0,1,2,29,100,65535);mods=(0,100,101,-1,0x7fffffff,-0x80000000)
    for n in range(4096):
        sp=resources[n%8] if n<2048 else rng.getrandbits(32);mp=resources[(n//8)%8] if n<2048 else rng.getrandbits(32)
        sc=costs[(n//64)%6] if n<2048 else rng.randrange(65536);mc=costs[(n//3)%6] if n<2048 else rng.randrange(65536)
        sr=mods[(n//17)%6] if n<2048 else rng.randrange(-0x80000000,0x80000000);mr=mods[(n//31)%6] if n<2048 else rng.randrange(-0x80000000,0x80000000)
        c.mem_write(user,bytes(0x6000));put(user+0x123c,mp,sp);put(user+0x1278,mr,sr);state.clear();call(0x481ca0,(sc,mc))
        if c.reg_read(UC_X86_REG_FPCW)!=control:raise RuntimeError('original did not restore floating point control')
        ns,nm=get(user+0x1240),get(user+0x123c)
        if state['sent']!=(ns,nm):raise RuntimeError('write/send ordering differs')
        print(n,sp,mp,sc,mc,sr&0xffffffff,mr&0xffffffff,ns,nm,*state['requested'],state['packet'],sep='\t')
if __name__=='__main__':main()
