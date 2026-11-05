#!/usr/bin/env python3
"""Trace real CUser::Damage reduction and party-energy packet bodies."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game
c,digest=load_game();actor,damage,party,stack,stop=(0x30000000,0x31000000,0x32000000,0x33000000,0x34000000)
for a in (actor,damage,party,stack,stop):c.mem_map(a,0x10000)
def put(a,*v):c.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={}
def hook(m,a,size,ctx):
    sp=m.reg_read(UC_X86_REG_ESP)
    if a==0x4d4d20:
        assert m.reg_read(UC_X86_REG_ECX)==actor and get(sp+8)==4
        state['events'].append('S'+bytes(c.mem_read(get(sp+4),4)).hex())
    elif a==0x445680:
        assert m.reg_read(UC_X86_REG_EAX)==12 and m.reg_read(UC_X86_REG_EBX)==party and get(sp+4)==actor
        state['events'].append('P'+bytes(c.mem_read(m.reg_read(UC_X86_REG_ECX),12)).hex())
for a,ret in ((0x45b290,b'\xc3'),(0x4b8b60,b'\xc2\x08\x00'),(0x4d4d20,b'\xc2\x08\x00'),(0x445680,b'\xc2\x04\x00'),(0x487020,b'\xc2\x04\x00')):
    c.mem_write(a,ret);c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
print('# ps_game.exe SHA256 '+digest+'; CUser::Damage 0044c1e0 and SendEnergyAllToParty 00482360; wear, ledger, passive, self/group sends intercepted')
print('# party hp protection events killed hp mp sp')
for group in (-1,0,1):
 for hp in (3,50):
  for protection in (0,2):
    c.mem_write(actor,bytes(0x6000));c.mem_write(party,bytes(0x200));put(actor+0xdc,42);put(actor+0x1238,hp,0x12345,0x23456);put(actor+0x12fc,protection)
    if group>=0:put(actor+0x173c,party);c.mem_write(party+0x114,bytes([group]))
    put(damage,5,1,2);put(stack+0x8000,stop,2,damage);state['events']=[];c.reg_write(UC_X86_REG_EAX,actor);c.reg_write(UC_X86_REG_EBX,77);c.reg_write(UC_X86_REG_ESP,stack+0x8000)
    c.emu_start(0x44c1e0,stop,count=10000);assert c.reg_read(UC_X86_REG_EIP)==stop
    print(group,hp,protection,','.join(state['events']) or '-',c.reg_read(UC_X86_REG_EAX)&255,get(actor+0x1238),get(actor+0x123c),get(actor+0x1240),sep='\t')
