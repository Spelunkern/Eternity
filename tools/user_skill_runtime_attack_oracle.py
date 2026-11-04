#!/usr/bin/env python3
"""Whole resolved skill attack overloads with captured definition mutations."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
    user,target,skill,info,alternate,zone,stack,stop=base,base+0x8000,base+0x10000,base+0x11000,base+0x12000,base+0x20000,base+0x60000,base+0x7f000
    state={};events=[]
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def word(p,v):c.mem_write(p,struct.pack('<H',v))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def snapshot():return ','.join(map(str,(get(skill+28),get(user+0x1428),int(get(skill+48)==alternate),struct.unpack('<H',c.mem_read(info+0x44,2))[0],struct.unpack('<H',c.mem_read(info+0x46,2))[0],struct.unpack('<H',c.mem_read(info+0x50,2))[0])))
    def event(tag):events.append(tag+':'+snapshot())
    effects={0x450e20:8,0x450f70:4,0x44fd90:16,0x451980:0,0x451570:4,0x451d90:0,0x452b50:0,0x452bf0:0,0x453470:0,0x4534e0:4,0x453610:4}
    def hook(cpu,at,size,data):
        pop=0;result=None
        if at in effects:
            sp=cpu.reg_read(UC_X86_REG_ESP);eax=cpu.reg_read(UC_X86_REG_EAX);ecx=cpu.reg_read(UC_X86_REG_ECX);edx=cpu.reg_read(UC_X86_REG_EDX);ebx=cpu.reg_read(UC_X86_REG_EBX);esi=cpu.reg_read(UC_X86_REG_ESI);edi=cpu.reg_read(UC_X86_REG_EDI)
            actual={0x450e20:(ecx,get(sp+4),get(sp+8)),0x450f70:(get(sp+4),ecx,edx),0x44fd90:(ecx,get(sp+4),get(sp+8)),0x451980:(ebx,0,ecx),0x451570:(get(sp+4),edx,ecx),0x451d90:(eax,ecx,edx),0x452b50:(ecx,edx,eax),0x452bf0:(eax,edx,ecx),0x453470:(edx,ecx,eax),0x4534e0:(get(sp+4),edi,ecx),0x453610:(esi,get(sp+4),ebx)}[at]
            if actual!=(user,0 if at==0x451980 else target,info):raise RuntimeError('incorrect actual effect calling convention')
            if at==0x44fd90 and (get(sp+12),get(sp+16))!=(1,1):raise RuntimeError('debuff flags differ')
            event('E'+f'{at:08x}');pop=effects[at]
            if state['mutation']&1:put(skill+28,77);put(skill+48,alternate);word(info+0x44,3);word(info+0x46,4);word(info+0x50,2);put(user+0x12f8,0xffffffff)
        elif at==0x481ca0:
            sp=cpu.reg_read(UC_X86_REG_ESP);event('C'+str(get(sp+4))+','+str(get(sp+8)));pop=8
            if state['mutation']&2:word(info+0x50,3)
        elif at==stop+16:
            event('T');result=(state['now']+7)&0xffffffff
            if state['mutation']&4:word(info+0x50,0);put(user+0x1428,123)
        elif at==0x421220:
            sp=cpu.reg_read(UC_X86_REG_ESP);zone_arg,ptr,length,pos,radius,id,unknown,channel=struct.unpack('<8I',cpu.mem_read(sp+4,32))
            if (zone_arg,length,pos,radius,id,unknown,channel)!=(zone,8,user+0xd0,0x42700000,42,0,5):raise RuntimeError('wrong actual SendSpeed route')
            event('V'+bytes(cpu.mem_read(ptr,length)).hex());pop=32
            if state['mutation']&8:put(user+0x1428,0);put(skill+28,88)
        else:return
        sp=cpu.reg_read(UC_X86_REG_ESP);ret=get(sp);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
        cpu.reg_write(UC_X86_REG_EIP,ret)
    c.hook_add(UC_HOOK_CODE,hook);put(0x5210e4,stop+16)
    print(f'# ps_game.exe SHA256 {digest}; WHOLE AttackMob(CSkill*)00450cf0/AttackUser(CSkill*)00451c80 and SendSpeed004827b0/GetAttackSpeed/GetMoveSpeed. Only actual effect/SendUseSPMP/time/PSendView ports; callbacks mutate captured info/runtime independently.')
    print('# case mob effect target_kind seconds cool_time_bits now mutation sp mp disable in_zone initializing final events')
    for case in range(768):
        c.mem_write(base,bytes(0x50000));mob=case%2;effect=(case//2)%8;kind=(3,6,7,0)[(case//16)%4];seconds=(0,500,501,65535,499,1)[(case//64)%6];cool=(-1,0,1,1000)[(case//8)%4];now=(0,100,0xfffffff0,0xffffffff,0xfffffff9)[(case//4)%5];mutation=0 if case<384 else (case//3)%16;sp=0 if case%3==0 else 2;mp=0 if case%5==0 else 5;disable=0 if case%4==0 else 1;in_zone=int(case%7!=0);initializing=int(case%11==0);state=dict(now=now,mutation=mutation);events=[]
        put(user+0xdc,42);put(user+0xe0,zone if in_zone else 0);put(user+0x12f8,cool);put(user+0x12ec,4,2);c.mem_write(user+0x57b5,bytes((initializing,)));put(skill+28,999);put(skill+48,info)
        c.mem_write(info+0x31,bytes((effect,)));c.mem_write(info+0x56,bytes((kind,)));word(info+0x4a,seconds);word(info+0x44,sp);word(info+0x46,mp);word(info+0x50,disable);word(alternate+0x44,9);word(alternate+0x46,9);word(alternate+0x50,9)
        c.mem_write(stack-0x1000,bytes(0x1000));args=(user,) if mob else ();put(stack,stop,*args);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,skill);c.reg_write(UC_X86_REG_EDX,now);c.reg_write(UC_X86_REG_EAX,target if mob else user);c.reg_write(UC_X86_REG_EDI,target)
        c.emu_start(0x450cf0 if mob else 0x451c80,stop,count=100000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4+4*len(args):raise RuntimeError('unbalanced complete resolved attack')
        print(case,mob,effect,kind,seconds,cool&0xffffffff,now,mutation,sp,mp,disable,in_zone,initializing,snapshot(),'|'.join(events) or '-',sep='\t')

if __name__=='__main__':main()
