#!/usr/bin/env python3
"""Execute original SendMove, SendSameMap and connection send with isolated pool/queue."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ESP, UC_X86_REG_ECX, UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,party,users,buffer,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
for base in (stack,party,users,buffer,stop):cpu.mem_map(base,0x10000)
def put(at,v):cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
def stub(at,n):cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
for at,n in ((0x4df9f0,0),(0x4d8950,8),(0x4df070,0)) :stub(at,n)
for off,at in ((0x5210cc,0x100),(0x5210d0,0x110),(0x5210f8,0x120),(0x5211f0,0x130)):
    put(off,stop+at);stub(stop+at,4)
events=[];state={}
def hook(machine,address,size,_):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address==0x4df9f0:
        payload=get(sp+4);length=get(sp+8);events.append('A'+bytes(cpu.mem_read(payload,length)).hex());put(buffer+8,1);machine.reg_write(UC_X86_REG_EAX,buffer if state['alloc'] else 0)
    elif address==stop+0x100:events.append('L')
    elif address==stop+0x110:events.append('U')
    elif address in (stop+0x120,stop+0x130):
        ptr=get(sp+4);value=(get(ptr)+(1 if address==stop+0x120 else -1))&0xffffffff;put(ptr,value);machine.reg_write(UC_X86_REG_EAX,value)
    elif address==0x4d8950:
        who=get(sp+4);events.append('E'+str(get(who+0xdc)));put(buffer+8,get(buffer+8)-1)
        if state['mutate']:put(party+0x10,2)
    elif address==0x4df070:events.append('F')
cpu.hook_add(UC_HOOK_CODE,hook)
print(f'# ps_game.exe SHA256 {digest}; SendMove 00445880, SendSameMap 004457f0, SConnection::Send 004d4cf0; pool, lock, queue, refcount isolated')
print('# kind allocated mutate events')
for kind,alloc,mutate in itertools.product((0,1),(0,1),(0,1)):
    cpu.mem_write(stack,bytes([0xa5])*0x10000);cpu.mem_write(party,bytes(0x1000));cpu.mem_write(users,bytes(0x6000));cpu.mem_write(buffer,bytes(0x1000));put(party+0x10,5);cpu.mem_write(party+0x114,bytes([kind]))
    for i in range(5):
        u=users+i*0x1000;put(party+0x18+i*8,u);put(u+0xdc,71+i);cpu.mem_write(u+0x164,struct.pack('<H',8 if i==2 else 7));put(u+0x68,0xffffffff if i==3 else 1)
    subject=users;cpu.mem_write(subject+0xd0,struct.pack('<3f',35,6,3));put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_EAX,subject);cpu.reg_write(UC_X86_REG_ECX,party);events.clear();state.update(alloc=alloc,mutate=mutate)
    cpu.emu_start(0x445880,stop,count=5000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('SendMove delivery did not return')
    print(kind,alloc,mutate,','.join(events),sep='\t')
