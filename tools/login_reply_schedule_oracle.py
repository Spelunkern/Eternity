#!/usr/bin/env python3
"""Execute the original accepted-login periodic reply-scan gate."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_original

cpu,digest=load_original('ps_login');stack,agent,stop=0x20000000,0x30000000,0x10000000
for address in (stack,agent,stop):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values]))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
events=[]
def due_hook(machine,address,size,data):events.append('due')
cpu.hook_add(UC_HOOK_CODE,due_hook,begin=0x412f4c,end=0x412f4c)
print(f'# ps_login.exe SHA256 {digest}; SAgent::UpdateReply 00412e40; disabled scan/send isolate next-tick gate')
print('previous\tnow\tdue\tnext')
for previous,now in [(0,0),(120000,119999),(120000,120000),(0xffffff00,0xfffffeff),(0xffffff00,0xffffff00),(0xffffff00,0),(120000,0xffffff00),(119744,119744)]:
    cpu.mem_write(agent,bytes(0x200));put(agent+0x2c,previous);put(agent+0x30,120000);put(stack+0x8000,stop,now)
    cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,agent);events.clear();cpu.emu_start(0x412e40,stop,count=10000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop or len(events)>1:raise RuntimeError('Original scan gate did not terminate')
    print(previous,now,int(bool(events)),get(agent+0x2c),sep='\t')
