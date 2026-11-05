#!/usr/bin/env python3
"""Trace real CUser::Damage triple arithmetic with owned callbacks intercepted."""
import itertools,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
c,digest=load_game();actor,damage,stack,stop=0x30000000,0x31000000,0x32000000,0x33000000
for a in (actor,damage,stack,stop):c.mem_map(a,0x10000)
def put(a,*v):c.mem_write(a,struct.pack('<'+'I'*len(v),*(x&0xffffffff for x in v)))
def get(a):return struct.unpack('<I',c.mem_read(a,4))[0]
state={}
def hook(m,a,size,ctx):
    sp=m.reg_read(UC_X86_REG_ESP)
    if a==0x45b290:state['trace']+='W'
    elif a==0x4b8b60:
        assert get(sp+4)==2 and get(sp+8)==state['original_hp'] and m.reg_read(UC_X86_REG_EBX)==77
        state['trace']+='L'
    elif a==0x4d4d20:
        packet=bytes(c.mem_read(get(sp+4),4));assert packet[:2]==b'\x25\x05';state['amount']=int.from_bytes(packet[2:],'little');state['trace']+='R'
    elif a==0x482360:state['trace']+='E'
    elif a==0x487020:state['trace']+='H'
for a,ret in ((0x45b290,b'\xc3'),(0x4b8b60,b'\xc2\x08\x00'),(0x4d4d20,b'\xc2\x08\x00'),(0x482360,b'\xc3'),(0x487020,b'\xc2\x04\x00')):
    c.mem_write(a,ret);c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
print('# ps_game.exe SHA256 '+digest+'; CUser::Damage 0044c1e0; wear, ledger, send, party, HP passive intercepted')
print('# hp mp sp mode absorb protection dh ds dm | trace reduced result hp mp sp dh ds dm')
cases=list(itertools.product((1,50),(0,20),(0,15),(0,1),(0,1,2,3,4),(0,3),(0,1,10,60),(0,4),(0,5)))
for hp,mp,sp,mode,absorb,protection,dh,ds,dm in cases:
    c.mem_write(actor,bytes(0x6000));put(actor+0x1238,hp,mp,sp);put(actor+0x142c,mode);put(actor+0x1338,absorb);put(actor+0x12fc,protection);put(damage,dh,ds,dm);put(stack+0x8000,stop,2,damage)
    state.update(trace='',amount=0,original_hp=dh);c.reg_write(UC_X86_REG_EAX,actor);c.reg_write(UC_X86_REG_EBX,77);c.reg_write(UC_X86_REG_ESP,stack+0x8000)
    c.emu_start(0x44c1e0,stop,count=10000);assert c.reg_read(UC_X86_REG_EIP)==stop
    print(hp,mp,sp,mode,absorb,protection,dh,ds,dm,state['trace'] or '-',state['amount'],c.reg_read(UC_X86_REG_EAX)&255,get(actor+0x1238),get(actor+0x123c),get(actor+0x1240),get(damage),get(damage+4),get(damage+8),sep='\t')
