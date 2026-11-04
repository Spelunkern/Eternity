#!/usr/bin/env python3
"""Whole original eligibility overloads, actual raw item/definition-byte lookup."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
    user,info,skill,weapon,item,shield,stack,stop=base,base+0x10000,base+0x11000,base+0x12000,base+0x13000,base+0x14000,base+0x60000,base+0x7f000
    events=[]
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def word(p,v):c.mem_write(p,struct.pack('<H',v))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def hook(cpu,at,size,data):
        if at in (0x44ec0e,0x44ef1b):events.append('B6')
        elif at in (0x44ec77,0x44ef24):events.append('B5')
        elif at==0x4d4d20:
            sp=cpu.reg_read(UC_X86_REG_ESP);events.append('S'+bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex());cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+12)
    c.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; WHOLE EnableUseSkillUser0044eb40/Mob0044ee50 with actual weapon pointer/info+a0 and raw stSkillInfo+33+type lookup; only self transport port. Resource-max/zero reduction and valid suffix inputs; native compares equipment middle.')
    print('# case mob real_type weapon shield needs_shield pattern detail sp mp ready padding reset range state attribute result events')
    for case in range(1344):
        c.mem_write(base,bytes(0x50000));mob=case%2;kind=(case//2)%28;has_weapon=(case//56)%2;has_shield=(case//112)%2;needs_shield=(case//224)%2;pattern=case//448;detail=0x100 if case%3==0 else 0;sp=(0x100,1,0,0)[case%4];mp=(0x100,1,0,0,0)[case%5];ready=2 if case%7==0 else 0;padding=0xa5 if case%11==0 else 0;reset=(0x100,1,0,0,0,0)[case%6];radius=7 if case%9==0 else 0;state=15 if case%13==0 else 0;attribute=3 if case%17==0 else 0;events=[]
        put(user+0xdc,42);put(user+0x123c,0x7fffffff,0x7fffffff);put(user+0x1d8,weapon if has_weapon else 0,shield if has_shield else 0);put(weapon+48,item);put(item+0xa0,kind);put(skill+12,11,2);put(skill+48,info)
        word(info+0x32,detail);c.mem_write(info+0x34,bytes(int((i+pattern)%3==0) for i in range(15))+bytes((needs_shield,)));word(info+0x44,sp);word(info+0x46,mp);c.mem_write(info+0x48,bytes((ready,padding)));word(info+0x4a,reset);c.mem_write(info+0x4c,bytes((radius,state,attribute)))
        c.mem_write(stack-0x1000,bytes(0x1000));put(stack,stop,100);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,user);c.reg_write(UC_X86_REG_EDI,skill);c.reg_write(UC_X86_REG_EDX,0)
        c.emu_start(0x44ee50 if mob else 0x44eb40,stop,count=100000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+8:raise RuntimeError('unbalanced whole eligibility')
        print(case,mob,kind,has_weapon,has_shield,needs_shield,pattern,detail,sp,mp,ready,padding,reset,radius,state,attribute,c.reg_read(UC_X86_REG_EAX)&255,'|'.join(events) or '-',sep='\t')

if __name__=='__main__':main()
