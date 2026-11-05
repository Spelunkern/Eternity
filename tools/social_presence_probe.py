#!/usr/bin/env python3
"""Execute ps_game InitFriend/ReleaseFriend with controlled world lookup."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,peer,world,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for address in (stack,user,peer,world,data,stop):cpu.mem_map(address,0x10000)
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*v))
events=[];present={5,9};last=0;mutation=None;mutated=False
def hook(m,a,size,context):
    global last,mutated
    sp=m.reg_read(UC_X86_REG_ESP)
    if a==0x410320:
        last=get(m.reg_read(UC_X86_REG_EBX));events.append(('lookup',last))
        out=m.reg_read(UC_X86_REG_EAX);put(out,data+0x100 if last in present else data);m.reg_write(UC_X86_REG_EAX,out)
        if mutation=='lookup_remove' and not mutated:put(user+0x1784,get(user+0x1784+2*80));put(user+0x1780,2);mutated=True
    elif a==0x4d4d20:
        events.append(('send',last,bytes(m.mem_read(get(sp+4),get(sp+8)))))
        if mutation=='send_remove' and not mutated:put(user+0x1784,get(user+0x1784+2*80));put(user+0x1780,2);mutated=True
        if mutation=='send_append' and not mutated:put(user+0x1784+2*80,9);put(user+0x1780,3);mutated=True
for a,n in ((0x4d4d20,8),(0x410320,0),(0x505316,0),(stop+0x100,4)):
    cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x100);put(0x546f38,world);put(world+0x2c,data);put(data+0x110,peer)
def run(address,ids,change=None):
    global mutation,mutated
    mutation=change;mutated=False;events.clear();cpu.mem_write(user,bytes(0x10000));put(user+0xdc,0x12345678);put(user+0x1780,len(ids))
    for i,id in enumerate(ids):put(user+0x1784+i*80,id)
    put(stack+0xf000,stop,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.emu_start(address,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    return list(events)
for function,status in ((0x4756d0,1),(0x4757a0,0)):
    assert run(function,[])==[]
    expected=[]
    for id in (0,5,8,5,9,0):
        if id:
            expected.append(('lookup',id))
            if id in present:expected.append(('send',id,struct.pack('<HIB',0x2207,0x12345678,status)))
    assert run(function,[0,5,8,5,9,0])==expected
    assert get(user+0x1780)==6
    first=[('lookup',5),('send',5,struct.pack('<HIB',0x2207,0x12345678,status)),('lookup',8)]
    assert run(function,[5,8,9],'lookup_remove')==first
    assert run(function,[5,8,9],'send_remove')==first
    assert run(function,[5,8],'send_append')==first+[('lookup',9),('send',9,struct.pack('<HIB',0x2207,0x12345678,status))]
print('10 original social presence cases passed; ps_game.exe SHA256',digest)
