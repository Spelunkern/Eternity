#!/usr/bin/env python3
"""Run original defensive item-option dispatch with skill execution observed."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
c,digest=load_game();stack,mob,user,items,skills,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000
for address in (stack,mob,user,items,skills,stop):c.mem_map(address,0x10000)
def put(a,*v):c.mem_write(a,struct.pack('<'+'I'*len(v),*v))
def byte(a,n):c.mem_write(a,bytes([n]))
state={}
def hook(m,a,size,ctx):
    if a==0x41d760:m.reg_write(UC_X86_REG_EAX,state['status'])
    elif a==0x44c610:m.reg_write(UC_X86_REG_EAX,0)
    elif a==0x44c940:
        assert m.reg_read(UC_X86_REG_ECX)==user
        state['calls'].append((m.reg_read(UC_X86_REG_EDX)-skills)//160)
    elif a==0x505316:pass
for a,ret in ((0x41d760,b'\xc3'),(0x44c610,b'\xc3'),(0x44c940,b'\xc3'),(0x505316,b'\xc3')):
    c.mem_write(a,ret);c.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0xfcd2c4,64);put(0xfcd2c8,skills)
for n in range(1,20):
    jewel=0x6882ac+n*168;byte(jewel+0x24,1);byte(jewel+0x25,1);byte(jewel+0x46,n);c.mem_write(skills+n*160,b'\x01\x00')
print('# ps_game.exe SHA256 '+digest+'; 0044cae0 defensive option dispatch; 0044c940 observed')
print('# guard mob_status user_mode slots calls final_guard')
for guard,status,mode,slots in ((0,1,0,1),(1,1,0,1),(0,2,0,1),(0,1,1,1),(0,1,0,0)):
    c.mem_write(mob,bytes(0x1000));c.mem_write(user,bytes(0x6000));c.mem_write(items,bytes(0x1000));byte(user+0x57b4,guard);put(user+0x142c,mode)
    if slots:
        for slot in range(7):
            item=items+slot*0x100;put(user+0x1c4+slot*4,item);byte(item+0x46,slot+1);byte(item+0x47,slot+8)
    state.update(status=status,calls=[]);put(stack+0x8000,stop,user);c.reg_write(UC_X86_REG_ESP,stack+0x8000);c.reg_write(UC_X86_REG_ECX,mob)
    c.emu_start(0x44cae0,stop,count=100000)
    assert c.reg_read(UC_X86_REG_EIP)==stop
    print(guard,status,mode,slots,','.join(map(str,state['calls'])) or '-',c.mem_read(user+0x57b4,1)[0],sep='\t')
