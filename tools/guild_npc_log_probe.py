#!/usr/bin/env python3
"""Probe ps_game CGuild::NpcUpgrade against executable tables and live slots."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,guild,stop=0x20000000,0x30000000,0x31000000,0x10000000
for a in (stack,user,guild,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v&0xffffffff))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[]
def hook(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x4d4d20:events.append(('self',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x42c050:events.append(('guild',bytes(m.mem_read(m.reg_read(UC_X86_REG_ECX),m.reg_read(UC_X86_REG_EAX)))))
 elif a==0x4d4f10:events.append(('db',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==0x4d4e50:events.append(('log',bytes(m.mem_read(get(sp+4),get(sp+8)))))
 elif a==stop+0x300:
  ptr,delta=get(sp+4),get(sp+8);old=get(ptr);put(ptr,old+delta);m.reg_write(UC_X86_REG_EAX,old)
for a,n in ((0x4d4d20,8),(0x42c050,4),(0x4d4f10,8),(0x4d4e50,8),(0x4cb8e0,0),(0x505316,0),(stop+0x100,4),(stop+0x200,4),(stop+0x300,8)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
cpu.mem_write(0x4cb8e0,b'\xb8\x78\x56\x34\x12\xc3')
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x200);put(0x52111c,stop+0x300);put(0x546f44,stop+0x400);put(0x546f4c,stop+0x500);put(0x541b14,2000)
def run(rank=1,use=1,buy=1,kind=2,level=1,etin=10000,keep=2000,slots=()):
 events.clear();cpu.mem_write(guild,bytes(0x10000));cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));put(guild+8,77);cpu.mem_write(guild+12,b'GuildName\0');put(user+0x578c,0x11223344);cpu.mem_write(user+0x5794,b'Account\0');put(user+0x128,0x55667788);cpu.mem_write(user+0x188,b'Character\0');cpu.mem_write(user+0x136,struct.pack('<H',0x1234));put(user+0x13c,0xabcdef01);cpu.mem_write(user+0x164,struct.pack('<H',0x4321));put(user+0xd0,0x01020304);put(user+0xd4,0x05060708);put(user+0xd8,0x090a0b0c);cpu.mem_write(guild+0x48,bytes([rank]));put(guild+0x4c,etin);put(guild+0x50,keep);cpu.mem_write(guild+0x54,bytes([use,buy]));
 for i,(t,l) in enumerate(slots):cpu.mem_write(guild+0x47c+i*4,struct.pack('<HH',t,l))
 put(stack+0xf000,stop);put(stack+0xf004,guild);put(stack+0xf008,kind);put(stack+0xf00c,level);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,user);cpu.emu_start(0x430640,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 return list(events),get(guild+0x4c),get(guild+0x50),bytes(cpu.mem_read(guild+0x47c,32))
events,etin,keep,slots=run()
packet=events[-1][1]
assert len(packet)==121 and packet[:2]==b"\x09\x0b"
for offset,fmt,value in ((2,"<I",0x11223344),(0x19,"<I",0x55667788),(0x32,"<H",0x1234),(0x34,"<I",0xabcdef01),(0x38,"<H",0x4321),(0x3a,"<I",0x01020304),(0x3e,"<I",0x05060708),(0x42,"<I",0x090a0b0c),(0x46,"<I",0x12345678),(0x4a,"<I",77),(0x6e,"<H",2),(0x70,"<B",1),(0x71,"<I",9000),(0x75,"<I",2200)):
 assert struct.unpack_from(fmt,packet,offset)[0]==value,(offset,packet.hex())
assert packet[6:14]==b"Account\0" and packet[0x1d:0x27]==b"Character\0" and packet[0x4e:0x58]==b"GuildName\0"
print('Original NPC upgrade game-log packet passed; ps_game.exe SHA256',digest)
