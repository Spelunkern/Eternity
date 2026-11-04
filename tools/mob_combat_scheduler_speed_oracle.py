#!/usr/bin/env python3
"""Actual speed getters and complete UpdateAttack with live effect counters."""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();root=0x30000000;cpu.mem_map(root,0x20000)
    user,mob,stack,stop=root,root+0x8000,root+0x18000,root+0x1f000
    standalone=False;events=[]
    offsets=(0x12ec,0x1340,0x1344,0x1428,0x1348,0x133c,0x5778)
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def snapshot():
        fields=[get(user+x) for x in (0x142c,0x1274,0x1528,0x152c,0x143c,0x1440,0x1450)]+[cpu.mem_read(user+0x1448,1)[0]]+[get(user+x) for x in offsets]+[get(mob+0x124)]
        return ','.join(map(str,fields))
    def event(tag,arg=''):events.append(tag+':'+snapshot()+(','+arg if arg else ''))
    def hook(c,at,size,data):
        if standalone:
            if not(0x48a460<=at<=0x48a4a1 or 0x48a4b0<=at<=0x48a4c7):raise RuntimeError('speed getter called external code')
            return
        pop,result=0,None
        if at==0x489560:event('T');pop=4
        elif at==0x44cd80:event('E');result=mob
        elif at==0x44c030:
            event('R');pop=8;result=1
            if mutation&1:
                for x in (0x1340,0x1344,0x1428,0x1348):put(user+x,0xffffffff)
                put(user+0x12ec,9)
        elif at==0x44c610:event('B');result=0
        elif at==0x44ce10:event('A');pop=4;put(mob+0x124,2 if mutation&2 else 0)
        elif at==0x48a010:event('C');return
        elif at==0x4d4d20:
            sp=c.reg_read(UC_X86_REG_ESP);event('P',bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex());pop=8
        else:return
        sp=c.reg_read(UC_X86_REG_ESP);ret=get(sp);c.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:c.reg_write(UC_X86_REG_EAX,result)
        c.reg_write(UC_X86_REG_EIP,ret)
    cpu.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x48a460)
    print(f'# ps_game.exe SHA256 {digest}; actual GetAttackSpeed 0048a460/GetAttackSpeedNow 0048a4b0, plus complete UpdateAttack 0048ca50 with real getters/Stop/CancelMove/cookie. No speed-query port.')
    print('# G raw_speed action_a action_b casting fixed movement restricted -> gate now; F same inputs now resume mutation -> bool final events')
    for case in range(3072):
        speed=(0xffffffff,0xfffffffe,0,1,8,9,10,255,0x7fffffff,0x80000000)[(case//16)%10]
        mask=case%16
        if case>=2048 and case%2==0:mask=0
        blockers=[rng.choice((1,0xffffffff,0x80000000)) if mask&(1<<i) else 0 for i in range(4)]
        values=[speed,*blockers,rng.choice((0,1,0xffffffff)),rng.choice((0,1,0xffffffff))]
        cpu.mem_write(user,bytes(0x6000));cpu.mem_write(mob,bytes(0x1000))
        for offset,value in zip(offsets,values):put(user+offset,value)
        if case<2048:
            standalone=True;before=bytes(cpu.mem_read(user,0x6000));results=[]
            for entry in (0x48a460,0x48a4b0):
                put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EAX,user);cpu.emu_start(entry,stop,count=1000)
                if cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+4 or bytes(cpu.mem_read(user,0x6000))!=before:raise RuntimeError('getter mutated user or failed stack balance')
                results.append(cpu.reg_read(UC_X86_REG_EAX))
            print('G',*values,*results,sep='\t')
        else:
            standalone=False;now=rng.choice((100,0xfffffff0,0xffffffff));resume=0 if case%13 else ((now+1)&0xffffffff);mutation=(case//2)%4
            put(user+0xdc,42);put(user+0x142c,2);put(user+0x1274,0);put(user+0x1528,2,77);put(user+0x143c,55,1);put(user+0x1450,resume);put(user+0x1d8,1);cpu.mem_write(user+0x1448,b'\xa5')
            events=[];put(stack,stop,now);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,user);cpu.emu_start(0x48ca50,stop,count=100000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+8:raise RuntimeError('poll did not return balanced')
            print('F',*values,now,resume,mutation,cpu.reg_read(UC_X86_REG_EAX)&255,snapshot(),'|'.join(events) or '-',sep='\t')


if __name__=='__main__':main()
