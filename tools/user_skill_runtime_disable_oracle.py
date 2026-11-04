#!/usr/bin/env python3
"""Whole ChkKeepSkill with actual list/disable/sleep/speed tail, explicit prefix ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
    user,head,node,info,zone,stack,stop=base,base+0x8000,base+0x9000,base+0xa000,base+0xb000,base+0x60000,base+0x7f000
    state={};events=[]
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def snapshot():return ','.join(map(str,(get(user+0x1428),get(user+0x1344),get(user+0x142c))))
    def event(tag):events.append(tag+':'+snapshot())
    def hook(cpu,at,size,data):
        pop=0;result=None;sp=cpu.reg_read(UC_X86_REG_ESP)
        if at in (stop+16,stop+32):
            if get(sp+4)==user+0xa9c:state['depth']+=1 if at==stop+16 else -1
            pop=4
        elif at==0x486290:
            cpu.mem_write(cpu.reg_read(UC_X86_REG_ESI),bytes(12));pop=4;state['periodic']+=1
        elif at==0x44c3f0:result=0;pop=4 # actual periodic damage service: zero damage/no death
        elif at==0x487750:
            if state['depth']!=0:raise RuntimeError('disable stage retained applied lock')
            state['boundary']=snapshot();state['damage']=cpu.mem_read(sp+0x1f,1)[0];state['tail']=True;return
        elif at==stop+48:
            if not state['tail'] or state['depth']!=0:raise RuntimeError('fresh clock before unlocked tail')
            event('T');result=state['fresh']
            if state['mutation']==1:put(user+0x1428,0)
            if state['mutation']==2:put(user+0x1428,(state['fresh']+1)&0xffffffff)
            if state['mutation']==4:put(user+0x1428,state['fresh'])
        elif at==0x489560:state['cleanup'].append('M');return # execute actual Actor shape boundary
        elif at==0x4893a0:state['cleanup'].append('S');return # execute actual sleep owner, no detail66 node
        elif at==0x421220:
            z,ptr,length,pos,radius,id,unused,channel=struct.unpack('<8I',cpu.mem_read(sp+4,32));wire=bytes(cpu.mem_read(ptr,length))
            if (z,pos,radius,id,unused,channel)!=(zone,user+0xd0,0x42700000,42,0,5):raise RuntimeError('wrong actual view route')
            if state['tail']:
                event('V'+wire.hex())
                if state['mutation']==3:put(user+0x142c,1)
                if state['mutation']==5:put(user+0x1428,777)
            elif wire[:2]!=b'\x0f\x05':raise RuntimeError('unexpected periodic prefix packet')
            pop=32
        else:return
        ret=get(sp);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
        cpu.reg_write(UC_X86_REG_EIP,ret)
    c.hook_add(UC_HOOK_CODE,hook)
    for p,v in ((0x5210cc,stop+16),(0x5210d0,stop+32),(0x5210e4,stop+48)):put(p,v)
    print(f'# ps_game.exe SHA256 {digest}; WHOLE ChkKeepSkill00487190 with actual applied list, RemApplySkillToMOB00489560/RemApplySkillSleep004893a0/SendSpeed004827b0. TimeDamage/damage/view/OS ports; native compares unlocked tail from observed periodic boundary, not a replacement periodic list owner.')
    print('# case kind initial_deadline fresh caller_tick mutation in_zone initializing initial_sleep initial_mode pre_tail damage periodic cleanup final events')
    for case in range(512):
        c.mem_write(base,bytes(0x50000));kind=case%3;deadline=(0,1,99,100,101,0x7fffffff,0x80000000,0xffffffff)[(case//3)%8];fresh=(0,100,101,0xfffffff0,0xffffffff)[(case//24)%5];caller=(0,3000,0xfffffff0)[(case//120)%3];mutation=(case//8)%6;in_zone=int(case%7!=0);initializing=int(case%13==0);sleep=case%2;mode=int(case%11==0)
        state=dict(depth=0,tail=False,periodic=0,damage=0,boundary=None,cleanup=[],fresh=fresh,mutation=mutation);events=[]
        put(user+0xdc,42);put(user+0xe0,zone if in_zone else 0);put(user+0x12ec,4,2);put(user+0x1428,deadline,mode);put(user+0x1344,sleep);c.mem_write(user+0x57b5,bytes((initializing,)));put(user+0xa94,head,head);put(user+0xab4,int(kind!=0));put(head,head,head)
        if kind:
            put(head,node,node);put(node,head,head,123,11,1);put(node+0x24,17,0,0);put(node+0x30,info);c.mem_write(info+0x32,struct.pack('<H',61 if kind==2 else 0))
        c.mem_write(stack-0x1000,bytes(0x1000));put(stack,stop,caller);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,user);c.emu_start(0x487190,stop,count=300000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+8 or state['depth']!=0:raise RuntimeError('unbalanced complete keep frame')
        if state['damage']!=int(kind==2) or state['periodic']!=int(kind==2):raise RuntimeError('original periodic flag differs')
        if state['cleanup'] not in ([],['M','S']):raise RuntimeError('wrong actual cleanup order')
        print(case,kind,deadline,fresh,caller,mutation,in_zone,initializing,sleep,mode,state['boundary'],state['damage'],state['periodic'],''.join(state['cleanup']) or '-',snapshot(),'|'.join(events) or '-',sep='\t')

if __name__=='__main__':main()
