#!/usr/bin/env python3
"""Original skill AttackSuccess with actual immunity and hit-mode bytes."""
import random,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x30000000;c.mem_map(base,0x20000)
u,m,info,skill,stack,end=(base+x for x in (0,0x3000,0x5000,0x6000,0x10000,0x18000))
def put(a,v):c.mem_write(a,struct.pack('<I',v&0xffffffff))
def word(a,v):c.mem_write(a,struct.pack('<H',v))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
def hook(c,a,size,_):
 global draws
 if a!=0x504697:return
 draws+=1;sp=c.reg_read(UC_X86_REG_ESP);c.reg_write(UC_X86_REG_EAX,roll);c.reg_write(UC_X86_REG_EIP,get(sp));c.reg_write(UC_X86_REG_ESP,sp+4)
c.hook_add(UC_HOOK_CODE,hook);rng=random.Random(0x44d088)
print('# SHA256',digest,'CUser::AttackSuccess 0044d010 skill hit modes 1..3, real GetStatus; CRT rand observed')
print('# kind unbeatable status state_type state_immune skill_id skill_immune hit_type hit_value dex wis mob_dex mob_wis roll result draws')
for i in range(4096):
 kind=i%3;unbeatable=int(i%31==0);status=3 if i%37==0 else i%3;state_type=rng.choice((0,1,2,5));state_immune=rng.randrange(2);skill_id=rng.choice((1,5,100,256));skill_immune=rng.randrange(2);hit_type=1+i%3;hit_value=rng.randrange(256);dex=rng.randrange(1,1000);wis=rng.randrange(1,1000);md=rng.randrange(1,1000);mw=rng.randrange(1,1000);roll=rng.randrange(32768)
 c.mem_write(u,b'\0'*0x2000);c.mem_write(m,b'\0'*0x1000);c.mem_write(info,b'\0'*0x1000);c.mem_write(skill,b'\0'*0x1000)
 put(m+0xa8,info);put(m+0x298,unbeatable);put(m+0x124,status);put(u+0x122c,dex);put(u+0x1234,wis);word(info+0x3a,md);word(info+0x3c,mw);word(skill,skill_id);c.mem_write(skill+0x4d,bytes([state_type]));c.mem_write(skill+0x54,bytes([hit_type,hit_value]));
 if state_type:c.mem_write(info+0x45+state_type,bytes([state_immune]))
 if skill_immune and skill_id<256:c.mem_write(info+0x55,bytes([skill_id]))
 sp=stack+0x800;put(sp,end);put(sp+4,kind);c.reg_write(UC_X86_REG_ESP,sp);c.reg_write(UC_X86_REG_ECX,m);c.reg_write(UC_X86_REG_EDI,u);c.reg_write(UC_X86_REG_EAX,skill);c.reg_write(UC_X86_REG_FPCW,0x27f);c.reg_write(UC_X86_REG_FPSW,0);c.reg_write(UC_X86_REG_FPTAG,0xffff);draws=0
 c.emu_start(0x44d010,end,count=5000)
 print(kind,unbeatable,status,state_type,state_immune,skill_id,skill_immune,hit_type,hit_value,dex,wis,md,mw,roll,c.reg_read(UC_X86_REG_EAX)&255,draws,sep='\t')
