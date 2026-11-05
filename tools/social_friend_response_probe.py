#!/usr/bin/env python3
"""Execute ps_game's 2202 branch with controlled world, zone, clock and transports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game()
stack,user,peer,packet,stop=0x20000000,0x30000000,0x31000000,0x33000000,0x10000000
for address in (stack,user,peer,packet,stop):cpu.mem_map(address,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*v))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]; found=peer; zone=peer; calls=[]
def capture(m,a,size,context):
    sp=m.reg_read(UC_X86_REG_ESP)
    if a in (0x4d4d20,0x4d4f10):events.append((a,m.reg_read(UC_X86_REG_ECX),bytes(m.mem_read(get(sp+4),get(sp+8)))))
    elif a==0x40d7e0:calls.append('world');m.reg_write(UC_X86_REG_EAX,found)
    elif a==0x415f30:calls.append('zone');m.reg_write(UC_X86_REG_EAX,zone)
    elif a==stop+0x200:m.reg_write(UC_X86_REG_EAX,0xfffffff0)
for a,n in ((0x4d4d20,8),(0x4d4f10,8),(0x40d7e0,4),(0x415f30,4),(0x505316,0),(stop+0x100,4),(stop+0x200,0)):
    cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,capture,begin=a,end=a)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x100);put(0x5210e4,stop+0x200)
put(0x546f38,stop+0x300);put(0x546f44,stop+0x400)
def reset():
    global found,zone
    found=peer;zone=peer;events.clear();calls.clear()
    for a in (user,peer,packet):cpu.mem_write(a,bytes(0x10000))
    put(user+0x578c,42);put(user+0xdc,123);put(user+0x128,124);put(user+0xe0,stop+0x500);cpu.mem_write(user+0x12d,b'\x01');cpu.mem_write(user+0x188,b'Owner\0')
    put(peer+0xdc,222);put(peer+0x128,223);cpu.mem_write(peer+0x12d,bytes([1,4,5]));cpu.mem_write(peer+0x134,b'\x06');cpu.mem_write(peer+0x188,b'Target\0')
    cpu.mem_write(packet,struct.pack('<HB',0x2203,1));put(user+0x5478,222);put(user+0x583c,555);put(peer+0x578c,43);cpu.mem_write(user+0x12e,bytes([7,8]));cpu.mem_write(user+0x134,b'\x09')
def call():
    put(stack+0xf000,stop,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,packet)
    cpu.emu_start(0x475870,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
reset();call()
assert get(user+0x5478)==0 and get(user+0x583c)==0 and get(user+0x1784)==223 and get(peer+0x1784)==124
assert [v[0] for v in events]==[0x4d4f10,0x4d4f10,0x4d4d20,0x4d4d20,0x4d4d20]
assert events[0][2][:20]==struct.pack('<HII3B',0xc02,42,222,4,5,6)+b'Target\0'
assert events[1][2][:19]==struct.pack('<HII3B',0xc02,43,123,4,5,9)+b'Owner\0'
assert events[2][1]==peer and events[2][2]==b'\x03\x22\x01'
assert events[3][1]==peer and events[3][2][:13]==struct.pack('<HIB',0x2204,123,9)+b'Owner\0'
assert events[4][1]==user and events[4][2][:14]==struct.pack('<HIB',0x2204,222,6)+b'Target\0'
for case in ('none','missing','zone','refuse','existing'):
    reset()
    if case=='none':put(user+0x5478,0)
    if case=='missing':found=0
    if case=='zone':zone=0
    if case=='refuse':cpu.mem_write(packet+2,b'\x00')
    if case=='existing':put(user+0x1780,1);put(user+0x1784,223)
    call()
    if case=='none':assert not calls and not events and get(user+0x583c)==555
    else:
        assert get(user+0x5478)==0 and get(user+0x583c)==0
        if case in ('refuse','existing'):assert len(events)==1 and events[0][1]==peer and events[0][2]==b'\x03\x22\x00'
        else:assert not events
print('6 original friend-response cases passed; ps_game.exe SHA256',digest)
