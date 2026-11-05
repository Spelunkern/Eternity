#!/usr/bin/env python3
"""Trace named defensive item options, including live callback mutations."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
c,digest=load_game();stack,mob,user,items,skills,stop,respawn=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000)
for address in (stack,mob,user,items,skills,stop,respawn):c.mem_map(address,0x10000)
def put(a,*v):c.mem_write(a,struct.pack('<'+'I'*len(v),*v))
def byte(a,n):c.mem_write(a,bytes([n]))
def name(a):return bytes(c.mem_read(a,32)).split(b'\0',1)[0].decode('ascii')
state={}
def hook(m,a,size,ctx):
    if a==0x41d760:m.reg_write(UC_X86_REG_EAX,1)
    elif a==0x44c940:
        assert m.reg_read(UC_X86_REG_ECX)==user and m.reg_read(UC_X86_REG_EDX)==skills+160
        state['seen']=(name(mob+0xcf4),name(mob+0xd14),c.mem_read(user+0x57b4,1)[0])
        c.mem_write(mob+0xcf4,b'changed-user\0');c.mem_write(mob+0xd14,b'changed-skill\0')
for a in (0x41d760,0x44c940,0x505316):
    c.mem_write(a,b'\xc3');c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0xfcd2c4,2);put(0xfcd2c8,skills);c.mem_write(skills+160,b'\x01\0\x01option\0');jewel=0x6882ac+168
byte(jewel+0x24,1);byte(jewel+0x25,1);byte(jewel+0x46,1)
print('# ps_game.exe SHA256 '+digest+'; CUser::BeAttackedItemOption 0044cae0; actual CMob::IsBothmob 0044c610; AttackItemMob intercepted')
print('# lua both seen_user seen_skill seen_guard final_user final_skill final_guard')
for lua,both in ((0,0),(1,0),(0,1)):
    c.mem_write(mob,bytes(0x1000));c.mem_write(user,bytes(0x6000));c.mem_write(items,bytes(0x1000));c.mem_write(respawn,bytes(0x100));put(mob+0x284,lua)
    if both:put(mob+0xc84,respawn);byte(respawn+0x1c,1)
    c.mem_write(mob+0xcf4,b'prioruser\0');c.mem_write(mob+0xd14,b'priorskill\0');put(user+0x1c4+6*4,items);byte(items+0x46,1)
    state.clear();put(stack+0x8000,stop,user);c.reg_write(UC_X86_REG_ESP,stack+0x8000);c.reg_write(UC_X86_REG_ECX,mob);c.emu_start(0x44cae0,stop,count=100000)
    assert c.reg_read(UC_X86_REG_EIP)==stop and 'seen' in state
    print(lua,both,*state['seen'],name(mob+0xcf4),name(mob+0xd14),c.mem_read(user+0x57b4,1)[0],sep='\t')
