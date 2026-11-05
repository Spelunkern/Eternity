#!/usr/bin/env python3
"""Execute ps_game PacketGuild 0d23 with real GuildLock and controlled ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EBP, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu,digest=load_game();stack,user,guild,other,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,user,guild,other,stop):cpu.mem_map(address,0x10000)
def put(address,value):cpu.mem_write(address,struct.pack('<I',value))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
events=[];mode='normal'
def capture(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==stop+0x100:
        events.append(('lock',get(sp+4)))
        if mode=='clear':put(user+0x1758,0)
        if mode=='swap':put(user+0x1758,other)
    elif address==stop+0x200:events.append(('unlock',get(sp+4)))
    elif address==0x4d4d20:events.append(('send',machine.reg_read(UC_X86_REG_ECX),bytes(machine.mem_read(get(sp+4),get(sp+8)))))
for address,cleanup in ((stop+0x100,4),(stop+0x200,4),(0x4d4d20,8)):
    cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,capture,begin=address,end=address)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200)
def run(current,value,action='normal'):
    global mode
    mode=action;events.clear();cpu.mem_write(user,bytes(0x10000));cpu.mem_write(guild,bytes(0x10000));cpu.mem_write(other,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000))
    put(user+0x1758,guild if current else 0);put(guild+0x4c,value);put(other+0x4c,0xdeadbeef)
    cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBP,user)
    cpu.emu_start(0x477185,0x47768c,count=100000)
    assert cpu.reg_read(UC_X86_REG_EIP)==0x47768c
    return list(events)
assert run(False,123)==[]
for value in (0,1,0x12345678,0xffffffff):
    assert run(True,value)==[('lock',guild+0x578),('send',user,struct.pack('<HI',0xd23,value)),('unlock',guild+0x578)]
assert run(True,1,'clear')==[('lock',guild+0x578),('unlock',guild+0x578)]
assert run(True,1,'swap')==[('lock',guild+0x578),('send',user,struct.pack('<HI',0xd23,0xdeadbeef)),('unlock',guild+0x578)]
print('7 original guild etin query cases passed; ps_game.exe SHA256',digest)
