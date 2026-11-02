#!/usr/bin/env python3
"""Original guild NPC upgrade/reset and DBSaveNpcLv.

Ghidra work/guild-save-updates, work/guild-save-owner. Original one-node map
lookup, slot/marker changes, metadata writes and save/error loops execute.
Lease, SQL, CRT, interlocked exchange, locks and logging are ports.
"""
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,guild,tree,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000;manager=0x4710d8
    for at in (stack,guild,tree,data,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def string(at):
        out=bytearray()
        while (b:=cpu.mem_read(at+len(out),1))!=b'\0':out.extend(b)
        return out.decode('ascii')
    state={};events=[]
    def hook(m,address,size,context):
        sp=m.reg_read(UC_X86_REG_ESP)
        if address==0x44447f:
            dest,fmt=word(sp+4),string(word(sp+8));values=[];cursor=sp+12
            for match in re.finditer(r'%d',fmt):
                value=word(cursor);cursor+=4;values.append(value if value<0x80000000 else value-0x100000000)
            rendered=fmt%tuple(values);m.mem_write(dest,rendered.encode()+b'\0');m.reg_write(UC_X86_REG_EAX,len(rendered))
        elif address==0x43ccc0:
            failed=state['pass']==0 and state['mode']==1;events.append('ACQUIRE '+str(int(not failed)));m.reg_write(UC_X86_REG_EAX,0 if failed else data)
        elif address==0x43cd80:events.append('RELEASE')
        elif address==0x43e6f0:events.append('PREPARE')
        elif address==0x43f350:
            query=string(word(sp+4));failed=state['pass']==0 and (state['mode']==3 or state['mode']==2 and state['attempt']==state['fail_at']);state['attempt']+=1
            events.append('EXEC '+query.lower()+' | '+str(int(not failed)));m.reg_write(UC_X86_REG_EAX,0xffffffff if failed else 0)
        elif address==0x432d80:events.append('ERROR')
        elif address==stop+0x200:
            at,value=word(sp+4),word(sp+8);previous=word(at);put(at,value);m.reg_write(UC_X86_REG_EAX,previous)
    for at,cleanup in ((0x44447f,0),(0x43ccc0,0),(0x43cd80,4),(0x43e6f0,0),(0x43f350,4),(0x432d80,0),(0x444dc3,0),(stop+0x100,4),(stop+0x200,8)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    put(0x4600d0,stop+0x200)
    def call(at,args,reg=None):
        cpu.mem_write(stack,bytes(0x10000));put(stack+0xf000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
        if reg is not None:cpu.reg_write(UC_X86_REG_EAX,reg)
        cpu.emu_start(at,stop,count=200000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original guild NPC owner did not return')
        return cpu.reg_read(UC_X86_REG_EAX)
    def snapshot():print('STATE',word(guild+0x14),word(guild+0x68),word(guild+0x5c),*struct.unpack('<24H',cpu.mem_read(guild+0x470,48)))
    rng=random.Random(0x4103e0)
    print(f'# ps_dbAgent.exe SHA256 {digest}; UpdateNpcUpgrade 00410010, ResetNpcLevel 004100f0, DBSaveNpcLv 004103e0; actual tree/slots/markers/save loops; SQL/CRT/lease/OS ports')
    for case in range(256):
        cpu.mem_write(guild,bytes(0x10000));cpu.mem_write(tree,bytes(0x10000));cpu.mem_write(manager,bytes(0x100));gid=rng.getrandbits(32);initial=[rng.choice((0,1,2,3,65535,rng.getrandbits(16))) for _ in range(24)];etin,keep=rng.getrandbits(32),rng.getrandbits(32);marker=rng.choice((0,2,17));mode=case%4;fail_at=(case//4)%8
        cpu.mem_write(guild+0x470,struct.pack('<24H',*initial));put(guild+12,gid);put(guild+0x14,etin);put(guild+0x68,keep);put(guild+0x5c,marker)
        head,node=tree,tree+32;put(manager+0x1c,0,head,1);put(head,node,node,node);cpu.mem_write(head+20,b'\x01\x01');put(node,head,head,head,gid,guild);cpu.mem_write(node+20,b'\x01\x00')
        print('CASE',gid,mode,fail_at,etin,keep,marker,*initial)
        state.update(mode=mode,fail_at=fail_at,attempt=0,**{'pass':0})
        for step in range(case%7):
            events.clear()
            if step%3==2:
                keep=rng.getrandbits(32);print('RESET',keep);call(0x4100f0,(gid,keep),manager)
            else:
                slot=rng.randrange(8);kind=rng.choice((0,1,32767,32768,65535));level=rng.choice((0,1,255));etin,keep=rng.getrandbits(32),rng.getrandbits(32);print('UPGRADE',slot,kind,level,etin,keep);call(0x410010,(gid,kind,level,slot,etin,keep),manager)
            snapshot()
        for turn in range(2):
            state.update(attempt=0,**{'pass':turn});events.clear();result=call(0x4103e0,(guild,));print('PASS',turn)
            for event in events:print('TRACE',event)
            print('RETURN',result);snapshot();print('ENDPASS')
        print('END')


if __name__=='__main__':main()
