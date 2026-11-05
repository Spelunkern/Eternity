#!/usr/bin/env python3
"""Execute original pre-authentication duplicate billing-user handling."""
import struct
import sys
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game

cpu,digest=load_game();stack,user,other,packet,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for address in (stack,user,other,packet,stop):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values]))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
# Only the existing-user lookup, logging, socket close and cookie are ports.
for address,code in ((0x40d570,b'\xc2\x04\x00'),(0x4d9510,b'\xc3'),(0x4d43a0,b'\xc2\x08\x00'),(0x505316,b'\xc3')):cpu.mem_write(address,code)
closed=[];lookups=[];token_reads=[]
def hook(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==0x40d570:
        lookups.append(get(sp+4));machine.reg_write(UC_X86_REG_EAX,other)
    elif address==0x4d43a0:closed.append((get(sp+4),get(sp+8)))
for address in (0x40d570,0x4d43a0):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
short=sys.argv[1:]==['--short']
if sys.argv[1:] and not short:raise ValueError('Expected optional --short')
if short:
    def token_read(machine,access,address,size,value,data): token_reads.append((address,size))
    cpu.hook_add(UC_HOOK_MEM_READ,token_read,begin=packet+6,end=packet+21)
print(f'# ps_game.exe SHA256 {digest}; CUser::PacketProcessing 00466da0; existing billing-user lookup and transport calls are ports')
if short: print('# Logical payload contains only opcode and account ID; token-area reads are forbidden')
cpu.mem_write(user,bytes(0x6000));cpu.mem_write(packet,struct.pack('<HI',0xa301,42)+(b'' if short else bytes(16)))
for index in range(11):
    put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,user);cpu.reg_write(UC_X86_REG_EDX,packet)
    cpu.emu_start(0x466da0,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop or lookups[-1:]!=[42]:raise RuntimeError('Duplicate lookup did not complete')
    expected=min(index+1,10)
    if get(user+0x5810)!=expected or closed!=([(101,0)] if index==10 else []):raise RuntimeError('Unexpected duplicate limit')
    if token_reads:raise RuntimeError('Original duplicate path read the token area')
    print(index+1,expected,closed[-1][0] if closed else 0)
