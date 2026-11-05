#!/usr/bin/env python3
"""Execute all ignored ps_game PacketGuild opcodes through the real switch."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu,digest=load_game();stack,user,packet,stop=0x20000000,0x30000000,0x33000000,0x10000000
for address in (stack,user,packet,stop):cpu.mem_map(address,0x10000)
cpu.mem_map(0,0x1000)  # x86 SEH chain accessed through FS:0
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*values))
cpu.mem_write(0x505316,b'\xc3')
external=[]
def capture(machine,address,size,context):
    if not 0x4764a0<=address<0x477740 and address not in (0x505316,stop):external.append(address);machine.emu_stop()
cpu.hook_add(UC_HOOK_CODE,capture)
implemented=set(range(3,11))|{0x0c,0x20,0x21,0x22,0x23,0x25,0x34,0x35}
for opcode in range(256):
    if opcode in implemented:continue
    original_user=bytes([0x5a])*0x10000;original_packet=struct.pack('<H',0x0d00|opcode)+bytes([0xa5])*254
    cpu.mem_write(user,original_user);cpu.mem_write(packet,original_packet);cpu.mem_write(stack,bytes(0x10000));external.clear()
    put(stack+0xf000,stop,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,packet)
    cpu.emu_start(0x4764a0,stop,count=100000)
    assert cpu.reg_read(UC_X86_REG_EIP)==stop and not external,(opcode,external)
    assert cpu.mem_read(user,len(original_user))==original_user and cpu.mem_read(packet,len(original_packet))==original_packet,opcode
print('240 original ignored guild opcodes passed; ps_game.exe SHA256',digest)
