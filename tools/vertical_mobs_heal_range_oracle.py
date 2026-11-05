#!/usr/bin/env python3
"""Run original SkillHealRange cell traversal with HealUser observed."""
import itertools,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();base=0x20000000;stack=base;caster=base+0x10000;user=base+0x20000;zone=base+0x30000;grid=base+0x40000;data=base+0x50000;skill=base+0x70000;stop=base+0x80000
c.mem_map(base,0x90000);c.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
def put(a,*v):c.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
def pos(a,x,z):c.mem_write(a+0x50,struct.pack('<fff',x,0,z))
for at,n in ((stop+16,4),(stop+32,4),(0x4a3f50,4),(0x4a5780,12)):
    c.mem_write(at,b'\xc2'+struct.pack('<H',n))
put(0x5210cc,stop+16);put(0x5210d0,stop+32);state={}
def hook(m,a,size,ctx):
    sp=m.reg_read(UC_X86_REG_ESP)
    if a==0x4a3f50:assert get(sp+4)==caster and m.reg_read(UC_X86_REG_ESI)==0;state['trace']+='S'
    elif a==0x4a5780:
        target=m.reg_read(UC_X86_REG_EDX);assert m.reg_read(UC_X86_REG_ECX)==caster and [get(sp+i) for i in (4,8,12)]==[user,skill,0]
        state['targets'].append(get(target+0x5c))
for at in (0x4a3f50,0x4a5780):c.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
print('# ps_game.exe SHA256 '+digest+'; CMob::SkillHealRange 004a5520; original cell loops and eligibility; locks, SendUseSkill and HealUser intercepted')
print('# country radius deadmask | selected mob IDs')
for country,radius,deadmask in itertools.product((0,2),(0,34,35,49,50,255),(0,0x15)):
    for a in (caster,user,zone,skill):c.mem_write(a,bytes(0x1000))
    c.mem_write(grid,bytes(0x5000));c.mem_write(data,bytes(0x10000));put(caster+0x60,zone,1,1);put(caster+0xa8,data+0x9000);put(caster+0x5c,104);put(data+0x9078,country);pos(caster,45,45)
    put(zone+0x28,3,3,grid);put(grid,grid+0x100,grid+0x1100,grid+0x2100);c.mem_write(skill+0x56,b'\x03');c.mem_write(skill+0x57,bytes([radius]));coords=(10,45,80)
    for i in range(9):
        subject=caster if i==4 else data+i*0x1000;info=data+0x9000 if i==4 else subject+0x400;cell=grid+0x100+(i//3)*0x1000+(i%3)*292;head=grid+0x3300+i*128
        put(cell+0x2c,head);put(head+0x4c,subject+0x48);put(subject+0x4c,head+0x48);put(subject+0x5c,100+i);put(subject+0x124,2 if i!=4 and (deadmask>>i)&1 else 1);put(subject+0xa8,info);put(info+0x78,country if i==4 else i%2);pos(subject,coords[i%3],coords[i//3])
    put(stack+0xf000,stop);state.update(trace='',targets=[]);c.reg_write(UC_X86_REG_ESP,stack+0xf000);c.reg_write(UC_X86_REG_ECX,caster);c.reg_write(UC_X86_REG_EDX,user);c.reg_write(UC_X86_REG_ESI,skill);c.reg_write(UC_X86_REG_FPCW,0x27f)
    c.emu_start(0x4a54e0,stop,count=100000);assert c.reg_read(UC_X86_REG_EIP)==stop and state['trace']=='S'
    print(country,radius,deadmask,','.join(map(str,state['targets'])) or '-',sep='\t')
