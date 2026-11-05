#!/usr/bin/env python3
"""Trace mob-to-player normal AttackSuccess with deterministic CRT draws."""
import random,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x20000);u,m,info,stack,end=(base+x for x in (0,0x6000,0x8000,0x10000,0x18000))
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def word(a,v):c.mem_write(a,struct.pack('<H',v))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
def ret(result):
 sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_EIP,get(sp));c.reg_write(UC_X86_REG_ESP,sp+4);c.reg_write(UC_X86_REG_EAX,result)
def hook(c,a,size,_):
 global draws,blocks
 if a==0x44c5a0:blocks+=1;ret(blocked)
 elif a==0x504697:draws+=1;ret(roll)
for a in (0x44c5a0,0x504697):c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
rng=random.Random(0x4a23f0)
print('# SHA256',digest,'CMob::AttackSuccess 004a23f0 null skill; actor block and CRT rand intercepted')
print('# kind blocked user_level mob_level user_accuracy user_evasion mob_accuracy mob_evasion user_dex mob_dex roll | result blocks draws')
for i in range(1024):
 kind=i%4;blocked=int(i%29==0);ul=rng.choice((0,1,10,30,60,100,65535));ml=rng.choice((0,1,10,30,60,100,65535));dex=rng.randrange(0,1000);mobdex=rng.randrange(0,1000)
 ua,ue,ma,me=[rng.choice((0,1,-1,10,50,100,1000,32767,-32768,2147483647,-2147483648,rng.randrange(-5000,5001))) for _ in range(4)];roll=rng.randrange(32768)
 c.mem_write(u,bytes(0x6000));c.mem_write(m,bytes(0x2000));c.mem_write(info,bytes(0x1000));put(m+0xa8,info);word(u+0x136,ul);word(info+0x22,ml)
 if kind<3:put(u+0x13b8+44*kind,ua);put(u+0x13c0+44*kind,ue);put(m+0x138+28*kind,ma);put(m+0x140+28*kind,me)
 put(u+0x122c,dex);word(info+0x3a,mobdex);sp=stack+0x800;put(sp,end);put(sp+4,kind);c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_ECX,u);c.reg_write(UC_X86_REG_EDI,m);c.reg_write(UC_X86_REG_EAX,0);c.reg_write(UC_X86_REG_FPCW,0x27f);c.reg_write(UC_X86_REG_FPSW,0);c.reg_write(UC_X86_REG_FPTAG,0xffff);draws=blocks=0
 c.emu_start(0x4a23f0,end,count=5000)
 if c.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError('Mob AttackSuccess did not return')
 print(kind,blocked,ul,ml,ua,ue,ma,me,dex,mobdex,roll,c.reg_read(UC_X86_REG_EAX)&255,blocks,draws,sep='\t')
