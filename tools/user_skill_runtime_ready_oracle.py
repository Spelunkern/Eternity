#!/usr/bin/env python3
"""Whole original EnableUseSkill overloads; cost/equipment prefix valid inputs."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
    user,target,manager,defs,packet,heap,weapon,item_info,stack,stop=base,base+0x8000,base+0x10000,base+0x11000,base+0x12000,base+0x20000,base+0x30000,base+0x31000,base+0x60000,base+0x7f000
    state={};events=[]
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def snapshot():
        p=get(user+0xac4);q=get(user+0xac8);return ','.join(str(x) for x in (get(user+0xac0),get(user+0x1444),get(p+12),get(p+16),get(q+28)))
    def event(tag):events.append(tag+':'+snapshot())
    def mutate(id):put(user+0xac0,0);put(user+0x1444,0);put(get(user+0xac4)+12,id,3)
    def hook(cpu,at,size,data):
        pop=0;result=None
        if at in (stop+16,stop+32):pop=4
        elif at==stop+48:
            result=state['now']
            if not state['seed']:
                event('T')
                if state['profile']==8:put(get(user+0xac8)+28,state['now'])
                if state['profile']==9:put(get(user+0xac8)+28,(state['now']+1)&0xffffffff)
                if state['profile']==10:mutate(99);put(get(user+0xac8)+28,state['now'])
                if state['profile']==15:result=(state['now']+2)&0xffffffff
        elif at==stop+64:
            p=get(cpu.reg_read(UC_X86_REG_ESP)+4);put(p,get(p)+1);result=get(p);pop=4
        elif at==0x503a3f:result=heap+state['allocated']*64;state['allocated']+=1;cpu.mem_write(result,bytes(52))
        elif at==0x402770:result=defs
        elif at in (0x44bf90,0x44c030):
            sp=cpu.reg_read(UC_X86_REG_ESP);event('R'+str(get(sp+(4 if at==0x44bf90 else 8))));pop=4 if at==0x44bf90 else 8;result=int(state['profile']!=12)
            if state['mutate']:mutate(77)
        elif at==0x4d4d20:
            sp=cpu.reg_read(UC_X86_REG_ESP);event('S'+bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex());pop=8
            if state['mutate']:mutate(88)
        else:return
        sp=cpu.reg_read(UC_X86_REG_ESP);ret=get(sp);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
        cpu.reg_write(UC_X86_REG_EIP,ret)
    c.hook_add(UC_HOOK_CODE,hook)
    for p,v in ((0x5210cc,stop+16),(0x5210d0,stop+32),(0x5210e4,stop+48),(0x5210f8,stop+64)):put(p,v)
    def call(at,args,regs):
        c.mem_write(stack-0x1000,bytes(0x1000));put(stack,stop,*args);c.reg_write(UC_X86_REG_ESP,stack)
        for r,v in regs:c.reg_write(r,v)
        c.emu_start(at,stop,count=200000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4+len(args)*4:raise RuntimeError('unbalanced complete original function')
    print(f'# ps_game.exe SHA256 {digest}; whole EnableUseSkillUser 0044eb40 / Mob 0044ee50, actual SendUseSkillError 0044ea30 and admission/CreateSkill. Cost0/SP/MP100000 and compatible retained weapon are valid original prefix inputs; native compares suffix only.')
    print('# case mob profile now mutate result final events')
    for case in range(256):
        c.mem_write(base,bytes(0x50000));mob=case%2;profile=(case//2)%16;now=(0,100,0xfffffff0,0xffffffff)[(case//32)%4];state=dict(seed=True,allocated=0,profile=profile,now=now,mutate=case>=128);events=[]
        put(0x546f3c,manager);put(manager+16,500);put(user+0xdc,42);put(user+0x123c,100000,100000);put(user+0x1d8,weapon);put(weapon+48,item_info);put(item_info+0xa0,1);c.mem_write(defs+0x34,b'\x01');c.mem_write(defs+0x30,b'\x01');c.mem_write(defs+0x4c,b'\x07')
        required=22 if profile in (5,6,8,9,10,14,15) else 33 if profile==7 else 0;c.mem_write(defs+0x52,struct.pack('<H',required))
        rows=[(11,2,0,0),(22,1,1,0)]+([(22,1,2,0)] if profile==14 else [])
        wire=struct.pack('<HIB',0x504,42,len(rows))+b''.join(struct.pack('<HBBH',*r) for r in rows);c.mem_write(packet,wire);call(0x46d780,(user,),((UC_X86_REG_ECX,packet),));put(user+0x1444,17)
        active=get(user+0xac4);prereq=get(user+0xac8);deadline=0;reduction=0
        if profile in (1,3):deadline=(now+1)&0xffffffff
        if profile in (2,4):deadline=(now+1000)&0xffffffff;reduction=1 if profile==2 else 0x7fffffff
        if profile==3:reduction=-1
        put(active+28,deadline);put(user+0x12f8,reduction);put(prereq+28,(now+1)&0xffffffff if profile not in (6,9,14) else now)
        if profile==14:put(get(user+0xacc)+28,(now+1)&0xffffffff)
        if profile==11:put(user+0x13d0,0xffffffff)
        state['seed']=False;events=[]
        call(0x44ee50 if mob else 0x44eb40,(now,),((UC_X86_REG_ECX,user),(UC_X86_REG_EDI,active),(UC_X86_REG_EDX,0 if profile==13 else target)))
        print(case,mob,profile,now,int(state['mutate']),c.reg_read(UC_X86_REG_EAX)&255,snapshot(),'|'.join(events) or '-',sep='\t')

if __name__=='__main__':main()
