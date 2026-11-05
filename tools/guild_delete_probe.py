#!/usr/bin/env python3
"""Probe ps_dbAgent PacketGuild 0b05 retry calls using a controlled DB function."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_original
cpu,digest=load_original('ps_dbAgent');stack,packet,stop=0x20000000,0x30000000,0x10000000
for a in (stack,packet,stop):cpu.mem_map(a,0x10000)
def put(a,v):cpu.mem_write(a,struct.pack('<I',v))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
events=[];responses=[]
def capture(m,a,size,ctx):
 sp=m.reg_read(UC_X86_REG_ESP);guild,flag=get(sp+4),get(sp+8);mode=responses[len(events)];events.append((guild,get(flag)))
 if mode=='timeout' and get(flag)==0:put(flag,1)
 m.reg_write(UC_X86_REG_EAX,int(mode=='ok'))
cpu.mem_write(0x4282a0,b'\xc3');cpu.hook_add(UC_HOOK_CODE,capture,begin=0x4282a0,end=0x4282a0)
for responses,flags in ((['ok'],[0]),(['timeout','ok'],[0,1]),(['timeout','timeout','ok'],[0,1,1]),(['timeout']*3,[0,1,1]),(['error','ok'],[0,0]),(['error','timeout'],[0,0]),(['timeout','error','ok'],[0,1,1])):
 events.clear();cpu.mem_write(stack,bytes(0x10000));cpu.mem_write(packet,struct.pack('<HI',0xb05,0x12345678));put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EAX,packet)
 cpu.emu_start(0x40ba00,stop,count=100000);assert cpu.reg_read(UC_X86_REG_EIP)==stop and events==[(0x12345678,f) for f in flags],(responses,events)
print('7 original guild delete retry cases passed; ps_dbAgent.exe SHA256',digest)
