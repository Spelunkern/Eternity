#!/usr/bin/env python3
"""Whole SendUseSPMP with actual lock/transport boundaries and reentry mutations."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000);c.mem_map(0,0x10000)
    user,stack,stop=base,base+0x60000,base+0x7f000
    groups=[0,base+0x10000,base+0x11000,base+0x12000];state={}
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    enter,leave=stop+16,stop+32;put(0x5210cc,enter);put(0x5210d0,leave)
    def ret(cpu,n):
        sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+n)
    def hook(cpu,at,size,data):
        esp=cpu.reg_read(UC_X86_REG_ESP)
        if at==0x4d4d20:
            state['trace'].append('S'+bytes(cpu.mem_read(get(esp+4),get(esp+8))).hex())
            put(user+0x173c,groups[state['self']]);ret(cpu,8)
        elif at in (enter,leave):
            group=groups.index(get(esp+4)-0x118)
            state['trace'].append(('L' if at==enter else 'U')+str(group))
            if at==enter:put(user+0x173c,groups[state['lock']])
            ret(cpu,4)
        elif at==0x445680:
            group=groups.index(cpu.reg_read(UC_X86_REG_EBX));packet=bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_ECX),9))
            if cpu.reg_read(UC_X86_REG_EAX)!=9 or get(esp+4)!=user:raise RuntimeError('wrong group ABI')
            state['trace'].append('P'+str(group)+':'+packet.hex())
            if packet[6]==1:
                put(user+0x173c,groups[state['after']]);put(user+0x123c,0x12345678)
            ret(cpu,4)
    c.hook_add(UC_HOOK_CODE,hook)
    def call(at,args=()):
        put(stack,stop,*args);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ESI,user)
        c.emu_start(at,stop,count=100000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4+4*len(args):raise RuntimeError('unbalanced original')
    c.reg_write(UC_X86_REG_FPCW,0x37f);call(0x51136e)
    print(f'# ps_game.exe SHA256 {digest}; WHOLE SendUseSPMP 00481ca0; lock imports/self/group transport ported. Actual group enumeration is outside this coordinator.')
    print('# case kind self_membership lock_membership after_sp_membership sp_cost mp_cost trace')
    n=0
    for kind in range(2):
      for self_group in range(4):
       for lock_group in range(4):
        for after in range(1,4):
         for sc,mc in ((0,0),(2,0),(0,3),(2,3),(1,1)):
            c.mem_write(user,bytes(0x6000));put(user+0xdc,42);put(user+0x123c,100,100)
            for i,g in enumerate(groups[1:],1):c.mem_write(g+0x114,bytes([(kind+i-1)%2]))
            state.update(trace=[],self=self_group,lock=lock_group,after=after)
            call(0x481ca0,(sc,mc))
            print(n,kind,self_group,lock_group,after,sc,mc,','.join(state['trace']),sep='\t');n+=1
if __name__=='__main__':main()
