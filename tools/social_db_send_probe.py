#!/usr/bin/env python3
"""Probe ps_dbAgent SendCharacterFriend 0040a7f0 with retained social slots."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP
from original_oracle import load_original
cpu,digest=load_original('ps_dbAgent');stack,user,stop=0x20000000,0x30000000,0x10000000
for address in (stack,user,stop):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*values))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
cpu.mem_write(0x4327a0,b'\xc2\x08\x00');cpu.mem_write(0x444dc3,b'\xc3');packets=[]
def capture(machine,address,size,context):
 sp=machine.reg_read(UC_X86_REG_ESP);packets.append(bytes(machine.mem_read(get(sp+4),get(sp+8))))
cpu.hook_add(UC_HOOK_CODE,capture,begin=0x4327a0,end=0x4327a0)
cases=0
for count in (0,1,24,25,26,49,50,51,99,100):
 for sparse in (False,True):
  cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));packets.clear();put(user+0x38,42);put(user+0x87d8,1)
  expected=[]
  for i in range(count):
   index=(i*37)%100 if sparse else i
   f=user+0x4064+index*84;b=user+0x6134+index*80
   put(f,index+1);cpu.mem_write(f+4,bytes([3]));cpu.mem_write(f+5,b'Name\0');cpu.mem_write(f+26,b'Memo\0');cpu.mem_write(f+77,bytes([7,8]))
   put(b,index+101);cpu.mem_write(b+4,b'Block\0');cpu.mem_write(b+25,b'Note\0')
  for op,width,base,stride in ((1,79,user+0x4064,84),(5,76,user+0x6134,80)):
   rows=[]
   for i in range(100):
    address=base+i*stride;ident=get(address)
    if not ident:continue
    if op==1:rows.append(struct.pack('<IBBB',ident,7,8,3)+(b'Name\0').ljust(21,b'\0')+(b'Memo\0').ljust(51,b'\0'))
    else:rows.append(struct.pack('<I',ident)+(b'Block\0').ljust(21,b'\0')+(b'Note\0').ljust(51,b'\0'))
   for start in range(0,len(rows),25):expected.append((op,rows[start:start+25]))
  sp=stack+0x8000;put(sp,stop,user);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.emu_start(0x40a7f0,stop,count=1000000)
  assert len(packets)==len(expected),(count,sparse,len(packets),len(expected))
  for packet,(op,rows) in zip(packets,expected):
   width=79 if op==1 else 76;assert packet[:7]==bytes([op,12,42,0,0,0,len(rows)]) and len(packet)==7+width*len(rows)
   for i,row in enumerate(rows):
    actual=packet[7+i*width:7+(i+1)*width]
    # Original stack bytes after NUL are unspecified; compare meaningful fields.
    if op==1:assert actual[:7]==row[:7] and actual[7:12]==row[7:12] and actual[28:33]==row[28:33]
    else:assert actual[:4]==row[:4] and actual[4:10]==row[4:10] and actual[25:30]==row[25:30]
  cases+=1
print(cases,'original DB social batch cases passed; ps_dbAgent.exe SHA256',digest)
