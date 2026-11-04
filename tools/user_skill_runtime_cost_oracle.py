#!/usr/bin/env python3
"""Actual CRT precision setup and complete EnableUseSkill resource boundary."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
    user,definition,skill,weapon,item,stack,stop=base,base+0x10000,base+0x11000,base+0x12000,base+0x13000,base+0x60000,base+0x7f000
    state={}
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def hook(cpu,at,size,data):
        if at in (0x44eb9f,0x44eeaf):state['sp']=cpu.reg_read(UC_X86_REG_ECX)
        elif at in (0x44ebf5,0x44ef05):state['mp']=cpu.reg_read(UC_X86_REG_ECX)
        elif at==0x4d4d20:
            sp=cpu.reg_read(UC_X86_REG_ESP);state['packet']=bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex();cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+12)
    c.hook_add(UC_HOOK_CODE,hook)
    def call(at,args=(),regs=()):
        c.mem_write(stack-0x1000,bytes(0x1000));put(stack,stop,*args);c.reg_write(UC_X86_REG_ESP,stack)
        for r,v in regs:c.reg_write(r,v)
        c.emu_start(at,stop,count=100000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4+len(args)*4:raise RuntimeError('unbalanced complete original function')
    c.reg_write(UC_X86_REG_FPCW,0x37f);call(0x51136e);control=c.reg_read(UC_X86_REG_FPCW)
    if control&0xf00!=0x200:raise RuntimeError('CRT did not select precision53/nearest')
    c.mem_write(user,bytes(0x6000));put(user+0xdc,42);put(user+0x1d8,weapon);put(weapon+48,item);put(item+0xa0,1);put(skill+12,11,2);put(skill+48,definition);c.mem_write(definition+0x34,b'\x01')
    print(f'# ps_game.exe SHA256 {digest}; actual CRT helper 0051136e->_controlfp 0051a397 from _fpmath 00506081, control={control:04x}; WHOLE EnableUseSkillUser 0044eb40/Mob 0044ee50. Weapon valid, no required skill/block/target. Native compares resource prefix only, not remaining eligibility.')
    print('# case mob sp_cost mp_cost sp_reduction_bits mp_reduction_bits sp_bits mp_bits required_sp_bits required_mp_bits bool packet')
    rng=random.Random(0x44eb40);costs=(0,1,7,29,53,100,199,1000,32767,65535);reductions=(0,1,3,50,99,100,101,-1,-100,16777215,16777216,16777217,0x7fffffff,-0x80000000,0x40000001)
    for case in range(4096):
        mob=case%2;sp_cost=costs[(case//2)%len(costs)] if case<2048 else rng.randrange(65536);mp_cost=costs[(case//20)%len(costs)] if case<2048 else rng.randrange(65536)
        sr=reductions[(case//20)%len(reductions)] if case<2048 else rng.randrange(-0x80000000,0x80000000);mr=reductions[(case//7)%len(reductions)] if case<2048 else rng.randrange(-0x80000000,0x80000000)
        c.mem_write(definition+0x44,struct.pack('<HH',sp_cost,mp_cost));put(user+0x127c,sr);put(user+0x1278,mr);put(user+0x123c,0x7fffffff,0x7fffffff);state.clear()
        regs=((UC_X86_REG_ECX,user),(UC_X86_REG_EDI,skill),(UC_X86_REG_EDX,0));entry=0x44ee50 if mob else 0x44eb40
        call(entry,(100,),regs);sp_required,mp_required=state['sp'],state['mp']
        if c.reg_read(UC_X86_REG_EAX)&255!=1:raise RuntimeError('max signed resources failed')
        sp=(0,sp_required,(sp_required-1)&0xffffffff,0x80000000,0xffffffff,0x7fffffff)[(case//3)%6];mp=(0,mp_required,(mp_required-1)&0xffffffff,0x80000000,0xffffffff,0x7fffffff)[(case//5)%6]
        put(user+0x123c,mp,sp);state.clear();state['packet']='-';call(entry,(100,),regs)
        if c.reg_read(UC_X86_REG_FPCW)!=control:raise RuntimeError('resource path did not restore CRT control word')
        print(case,mob,sp_cost,mp_cost,sr&0xffffffff,mr&0xffffffff,sp,mp,sp_required,mp_required,c.reg_read(UC_X86_REG_EAX)&255,state['packet'],sep='\t')

if __name__=='__main__':main()
