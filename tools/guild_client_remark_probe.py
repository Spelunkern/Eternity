#!/usr/bin/env python3
"""Execute ps_game PacketGuild 0d20 with real lock and controlled transports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_EBP, UC_X86_REG_EBX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu,digest=load_game();stack,user,guild,packet,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,user,guild,packet,stop):cpu.mem_map(address,0x10000)
def put(address,value):cpu.mem_write(address,struct.pack('<I',value))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
events=[];clear_on_lock=False
def capture(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==stop+0x100:
        events.append(('lock',get(sp+4)))
        if clear_on_lock:put(user+0x1758,0)
    elif address==stop+0x200:events.append(('unlock',get(sp+4)))
    elif address==0x4d4d20:events.append(('self',machine.reg_read(UC_X86_REG_ECX),bytes(machine.mem_read(get(sp+4),get(sp+8)))))
    elif address==0x42c050:events.append(('guild',get(sp+4),bytes(machine.mem_read(machine.reg_read(UC_X86_REG_ECX),machine.reg_read(UC_X86_REG_EAX)))))
    elif address==0x4d4f10:events.append(('db',bytes(machine.mem_read(get(sp+4),get(sp+8)))))
for address,cleanup in ((stop+0x100,4),(stop+0x200,4),(0x4d4d20,8),(0x42c050,4),(0x4d4f10,8),(0x505316,0)):
    cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,capture,begin=address,end=address)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200);put(0x546f44,stop+0x300)
def run(rank,has_guild,text,clear=False):
    global clear_on_lock
    clear_on_lock=clear;events.clear();cpu.mem_write(user,bytes(0x10000));cpu.mem_write(guild,bytes(0x10000));cpu.mem_write(packet,bytes(0x10000));cpu.mem_write(stack,b'\xa5'*0x10000)
    put(user+0x174c,rank);put(user+0x1758,guild if has_guild else 0);put(guild+8,77);cpu.mem_write(guild+0x56,b'Old\0')
    cpu.mem_write(packet,struct.pack('<H',0xd20)+text+b'\0');cpu.reg_write(UC_X86_REG_EBX,packet);cpu.reg_write(UC_X86_REG_EBP,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
    cpu.emu_start(0x4770af,0x47768c,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==0x47768c
    return list(events),bytes(cpu.mem_read(guild+0x56,65))
for rank,text in ((0,b'Hi'),(2,b'Hi'),(1,b"O'Brien")):
    e,state=run(rank,True,text);assert len(e)==1 and e[0][0:2]==('self',user) and len(e[0][2])==68 and e[0][2][:3+len(text)+1]==b'\x20\x0d\0'+text+b'\0' and state[:4]==b'Old\0'
e,state=run(1,False,b'Hi');assert not e and state[:4]==b'Old\0'
for text in (b'',b'Hello',b'Z'*64,b'Z'*64+b"'ignored"):
    e,state=run(1,True,text);actual=text[:64];assert [x[0] for x in e]==['lock','guild','db','unlock'] and e[0][1]==guild+0x578 and e[-1][1]==guild+0x578
    assert e[1][1]==guild and len(e[1][2])==68 and e[1][2][:3+len(actual)+1]==b'\x20\x0d\x01'+actual+b'\0'
    assert len(e[2][1])==71 and e[2][1][:6+len(actual)+1]==struct.pack('<HI',0xb0a,77)+actual+b'\0' and state[:len(actual)+1]==actual+b'\0'
e,state=run(1,True,b'Hi',True);assert e==[('lock',guild+0x578),('unlock',guild+0x578)] and state[:4]==b'Old\0'
print('9 original guild remark cases passed; ps_game.exe SHA256',digest)
