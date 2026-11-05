#!/usr/bin/env python3
"""Probe DBCharacter bank duplicate placement, ps_dbAgent.exe 00421df7..00421e81."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EDI,UC_X86_REG_ESP
from original_oracle import load_original
cpu,digest=load_original('ps_dbAgent')
stack,user,nodes,root=0x20000000,0x30000000,0x31000000,0x32000000
for address in (stack,user,nodes,root):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*values))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
freed=[]
cpu.mem_write(0x40df10,b'\xc3')
def hook(machine,address,size,context):
 if address==0x40df10:freed.append(machine.reg_read(UC_X86_REG_EDI))
cpu.hook_add(UC_HOOK_CODE,hook,begin=0x40df10,end=0x40df10)
cases=0
for mask in range(64):
 for count in range(1,9):
  cpu.mem_write(stack,bytes(0x10000));cpu.mem_write(user,bytes(0x10000));cpu.mem_write(nodes,bytes(0x10000));cpu.mem_write(root,bytes(0x10000));freed.clear()
  sp=stack+0x8000;put(sp+0x14,user);put(sp+0x1c,root);put(sp+0x24,count)
  addresses=[nodes+i*0x80 for i in range(count)]
  put(root,addresses[-1],addresses[0])
  for i,p in enumerate(addresses):put(p,addresses[i-1] if i else root,addresses[i+1] if i+1<count else root)
  for i in range(6):
   if mask & (1<<i):put(user+0x320+i*4,0xabcdef00+i)
  cpu.reg_write(UC_X86_REG_ESP,sp);cpu.emu_start(0x421df7,0x421e81,count=50000)
  slots=[get(user+0x320+i*4) for i in range(6)]
  empty=[i for i in range(6) if not mask & (1<<i)]
  placed=min(len(empty),count)
  expected=[0xabcdef00+i if mask & (1<<i) else 0 for i in range(6)]
  for i,p in zip(empty,addresses[:placed]):expected[i]=p
  assert slots==expected,(mask,count,slots,expected)
  assert [p for p in freed if p]==addresses[placed:],(mask,count,freed,addresses[placed:])
  cases+=1
print(cases,'original bank duplicate placement cases passed; ps_dbAgent.exe SHA256',digest)
