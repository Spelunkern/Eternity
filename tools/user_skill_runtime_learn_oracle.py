#!/usr/bin/env python3
"""Whole LearnSkill/EnableLearnSkill/SendGetSkill/SetGameLogMain, actual allocation."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000);c.mem_map(0,0x10000)
    user,manager,heap,defs,packet,stack,stop=base,base+0x10000,base+0x20000,base+0x30000,base+0x40000,base+0x60000,base+0x7f000
    state={};events=[]
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def word(p,v):c.mem_write(p,struct.pack('<H',v&65535))
    def byte(p,v):c.mem_write(p,bytes((v&255,)))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def info(p):return (p-defs)//0x100 if p else 9
    def snapshot():
        out=[str(struct.unpack('<H',c.mem_read(user+0x13a,2))[0]),str(get(user+0xac0)),str(get(user+0x1444))]
        for i in range(3):
            p=get(user+0xac4+4*i)
            out.append(','.join(str(x) for x in ([get(p+x) for x in (8,12,16,20,24,28,32,36,40,44)]+[info(get(p+48))])) if p else '-')
        return '/'.join(out)
    def event(tag):events.append(tag+':'+snapshot())
    def target():return get(user+0xac4) if state['upgrade'] else state['last']
    def hook(cpu,at,size,data):
        result=None;pop=0
        if at in (stop+16,stop+32):pop=4
        elif at==stop+48:
            if not state['seed']:
                event('T')
                if state['profile']==22:put(user+0xac0,0);put(user+0x1444,0)
            result=state['tick']
        elif at==stop+64:
            p=get(cpu.reg_read(UC_X86_REG_ESP)+4);put(p,get(p)+1);result=get(p);pop=4
        elif at==0x503a3f:
            if not state['seed']:event('H')
            if not state['seed'] and state['profile']==7:result=0
            else:
                result=heap+state['allocated']*0x100;state['allocated']+=1;state['last']=result;put(result,*([0x11223344]*13))
        elif at==0x402770:
            if state['seed']:result=defs+0x200
            else:
                state['queries']+=1;event('Q'+str(state['queries']))
                result=0 if state['profile']==4 or (state['profile']==21 and state['queries']==2) else defs+(0x100 if state['profile']==20 and state['queries']==2 else 0)
        elif at==0x489a40:
            event('B')
            if state['profile']==18:
                p=target();put(p+12,99,3,9);put(p+48,defs+0x300);word(user+0x13a,55)
        elif at in (0x4d4f10,0x4d4d20,0x4d4e50):
            sp=cpu.reg_read(UC_X86_REG_ESP);raw=bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex();event(('D' if at==0x4d4f10 else 'S' if at==0x4d4d20 else 'G')+raw);pop=8
            if (at==0x4d4f10 and state['profile']==19) or (at==0x4d4d20 and state['profile']==24):
                p=target();put(p+48,defs+0x300)
                if at==0x4d4f10:word(user+0x13a,55)
        elif at==0x45b0d0:
            event('L')
            if state['profile']==23:put(target()+48,defs+0x400)
            return # execute whole SetGameLogMain
        elif at==0x4cb8e0:result=0x12345678
        else:return
        sp=cpu.reg_read(UC_X86_REG_ESP);ret=get(sp);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
        cpu.reg_write(UC_X86_REG_EIP,ret)
    c.hook_add(UC_HOOK_CODE,hook)
    for imported,value in ((0x5210cc,stop+16),(0x5210d0,stop+32),(0x5210e4,stop+48),(0x5210f8,stop+64)):put(imported,value)
    def call(at,args,regs=()):
        c.mem_write(stack-0x1000,bytes(0x1000));put(stack,stop,*args);c.reg_write(UC_X86_REG_ESP,stack)
        for r,v in regs:c.reg_write(r,v)
        try:c.emu_start(at,stop,count=300000)
        except Exception as e:raise RuntimeError(f'case {state["case"]} PC {c.reg_read(UC_X86_REG_EIP):x} events {events}') from e
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4+4*len(args):raise RuntimeError('unbalanced full function')
    print(f'# ps_game.exe SHA256 {digest}; whole LearnSkill 004842e0, EnableLearnSkill 00484080, SendGetSkill 00484260, SetGameLogMain 0045b0d0, CreateSkill 0040a780. Passive/definition/allocator/OS/transport/time ports; snapshots before callback mutations.')
    print('# case profile existing requested_level tick final counter events')
    for case in range(192):
        c.mem_write(base,bytes(0x50000));profile=case%32;existing=(case//32)%3;tick=(100,0xfffffff0)[case//96];state=dict(case=case,profile=profile,tick=tick,seed=True,allocated=0,queries=0,upgrade=existing==1,last=0);events=[]
        put(0x546f3c,manager);put(manager+16,500);put(user+0xdc,42);put(user+0x578c,42);put(user+0x128,99);word(user+0x136,10);word(user+0x164,7);put(user+0x13c,1234);put(user+0xd0,0x3f800000,0x40000000,0x40400000)
        c.mem_write(user+0x5794,b'Account\0');c.mem_write(user+0x188,b'Hero\0');byte(user+0x134,2);byte(user+0x12e,0);byte(user+0x12f,1);word(user+0x13a,1 if profile in (8,20) else 20)
        for i,name in enumerate((b'Requested\0',b'Validation\0',b'Old\0',b'After\0',b'Context\0')):
            p=defs+i*0x100;word(p,11);byte(p+2,2);c.mem_write(p+3,name);word(p+0x24,10);byte(p+0x26,0);byte(p+0x29,1);byte(p+0x2d,1);byte(p+0x2e,0 if i==1 else 7);byte(p+0x2f,1)
        if existing:
            c.mem_write(packet,struct.pack('<HIBHBBH',0x504,42,1,11 if existing==1 else 12,1,0,0));call(0x46d780,(user,),((UC_X86_REG_ECX,packet),));p=get(user+0xac4);put(p+0x14,0xf1000005);put(p+0x18,71,72,73,74,75,76)
            if profile==31:put(p+0x10,0xffffffff)
        put(user+0x1444,17);state['seed']=False;events=[];level=2 if existing==1 else 1
        if profile in (1,2,3):level=(0,4,-1)[profile-1]
        if profile==5:level=1 if existing==1 else 2
        if profile==6:level=3
        if profile==9:word(defs+0x24,11)
        if profile==10:byte(defs+0x26,1)
        if profile==11:byte(defs+0x29,0)
        if profile==12:byte(defs+0x2d,2)
        if profile==13:byte(user+0x12f,0)
        if profile==14:byte(user+0x12f,2);byte(defs+0x2d,3)
        if profile==15:byte(defs+0x2f,0)
        if profile==16:byte(user+0x12e,1);byte(defs+0x26,2)
        if profile==17:byte(user+0x12f,3);byte(defs+0x2d,3)
        if profile==25:byte(defs+0x26,6)
        if profile==26:byte(defs+0x26,3);byte(user+0x12e,3)
        if profile==27:byte(defs+0x26,4);byte(user+0x12e,2)
        if profile==28:byte(defs+0x26,5);byte(user+0x12e,3)
        if profile==29:byte(defs+0x26,5);byte(user+0x12e,1)
        if profile==30:word(defs+0x24,65535);word(user+0x136,65535)
        call(0x4842e0,(user,11,level))
        print(case,profile,existing,level,tick,snapshot(),get(manager+16),'|'.join(events) or '-',sep='\t')

if __name__=='__main__':main()
