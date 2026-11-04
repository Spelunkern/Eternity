#!/usr/bin/env python3
"""Whole SkillAttack including actual selected coordinator, explicit action ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x80000)
    source,target,info,stack,stop=base,base+0x10000,base+0x20000,base+0x60000,base+0x7f000;state={}
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def ret(cpu,n):
        sp=cpu.reg_read(UC_X86_REG_ESP);cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+n)
    def hook(cpu,at,size,data):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        if at==0x44e900:
            if cpu.reg_read(UC_X86_REG_ECX)!=info or cpu.reg_read(UC_X86_REG_EDX)&255!=4 or [get(sp+i) for i in (4,8,12)]!=[source,77,0]:raise RuntimeError('announce ABI')
            state['trace'].append('C');c.mem_write(info+0x58,bytes([state['announced']]));ret(cpu,12)
        elif at==0x44f4c0:
            if cpu.reg_read(UC_X86_REG_ECX)!=source or get(sp+4)!=target or get(sp+8)!=info:raise RuntimeError('hit ABI')
            state['trace'].append('H'+str(get(sp+12))+str(get(sp+16)));state['hits']+=1
            if state['hits']==1:
                c.mem_write(info+0x58,bytes([state['changed']]));put(source+0x142c,state['source_dead']);put(target+0x142c,state['target_dead']);put(source+0x1238,0x80000011)
                put(source+0x54cc,0);put(source+0x54d4,0);put(source+0x54e4,0)
            ret(cpu,16)
        elif at==0x44c120:
            if cpu.reg_read(UC_X86_REG_ECX)!=source or cpu.reg_read(UC_X86_REG_EAX)!=1 or cpu.reg_read(UC_X86_REG_EBX)!=42:raise RuntimeError('damage ABI')
            state['trace'].append('D'+str(get(sp+4)));cpu.reg_write(UC_X86_REG_EAX,state['killed']);ret(cpu,4)
        elif at==0x4586a0:
            if cpu.reg_read(UC_X86_REG_ECX)!=source or [get(sp+i) for i in (4,8)]!=[1,42]:raise RuntimeError('death ABI')
            state['trace'].append('F'+str(get(sp+12)));ret(cpu,12)
        elif at in (0x451f80,0x452560):
            if at==0x451f80:
                if cpu.reg_read(UC_X86_REG_EDX)!=info or cpu.reg_read(UC_X86_REG_EDI)!=source or cpu.reg_read(UC_X86_REG_ECX)!=target:raise RuntimeError('range ABI')
                state['trace'].append('R');ret(cpu,0)
            else:
                if cpu.reg_read(UC_X86_REG_ECX)!=info or cpu.reg_read(UC_X86_REG_EDI)!=source or get(sp+4)!=target:raise RuntimeError('range-select ABI')
                state['trace'].append('S');ret(cpu,4)
    c.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; WHOLE SkillAttack00451d90 + SkillAttackSelect00451dd0. Actual damage/death/hit/announcement/range boundaries ported; no damage body equivalence claim.')
    print('# case target_kind detail initial_count announced_count changed_count source_dead target_dead killed relation trace')
    n=0;coverage=set()
    for kind in (0,3,6,7,255):
     for detail in (20,21,22,23,24,25,26,27,28,65535):
      for variant in range(128):
        initial=variant%4;announced=(variant//2)%4;changed=(variant//4)%4;sd=(variant>>5)&1;td=(variant>>6)&1;killed=variant&1;relation=(variant//4)%8
        c.mem_write(source,bytes(0x6000));c.mem_write(target,bytes(0x6000));c.mem_write(info,bytes(160));put(source+0xdc,42);put(target+0xdc,77);put(source+0x1238,13)
        put(source+0x54cc,4 if relation&1 else 0);put(source+0x54d4,77 if relation&2 else 9);put(target+0x54d4,42 if relation&4 else 9);put(source+0x54e4,5 if relation==6 else 0)
        c.mem_write(info+0x32,struct.pack('<H',detail));c.mem_write(info+0x56,bytes([kind]));c.mem_write(info+0x58,bytes([initial]))
        state.update(trace=[],hits=0,announced=announced,changed=changed,source_dead=sd,target_dead=td,killed=killed)
        put(stack,stop);c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EAX,source);c.reg_write(UC_X86_REG_ECX,target);c.reg_write(UC_X86_REG_EDX,info)
        c.emu_start(0x451d90,stop,count=100000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4:raise RuntimeError('unbalanced original')
        if state['hits']>1:coverage.add('multi')
        coverage.update(x for x in state['trace'] if x in ('F0','F1'))
        print(n,kind,detail,initial,announced,changed,sd,td,killed,relation,','.join(state['trace']) or '-',sep='\t');n+=1
    if coverage!={'multi','F0','F1'}:raise RuntimeError('incomplete multihit/death coverage')
if __name__=='__main__':main()
