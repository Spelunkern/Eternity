#!/usr/bin/env python3
"""Original receive FIFO + OnRecv + location-sensitive packet dispatch.

AES, DB transport, zone entry, close, free, locks and security checks are ports.
The original FIFO, snapshot loop, OnRecv and PacketProcessing instructions run.
"""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stack,user,agent,mem,vt,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for a,size in ((stack,0x10000),(user,0x10000),(agent,0x10000),(mem,0x10000),(vt,0x10000),(stop,0x10000)):cpu.mem_map(a,size)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
for a in (stop+0x100,stop+0x120):cpu.mem_write(a,b'\xc2\x04\x00')
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x120);put(vt,0x466d20)
for a,b in ((0x4d4f10,b'\xc2\x08\x00'),(0x44b8a0,b'\xc3'),(0x4df070,b'\xc3'),(0x505316,b'\xc3')):cpu.mem_write(a,b)
events=[]
def hook(m,a,size,ctx):
    if a==0x4d4f10:
        sp=m.reg_read(UC_X86_REG_ESP);packet=bytes(m.mem_read(get(sp+4),get(sp+8)))
        assert packet==bytes.fromhex('07040b00000000000000');events.append('DB_0407')
    elif a==0x44b8a0:events.append('ENTER')
    elif a==0x4674f8:events.append('IGNORE')
for a in (0x4d4f10,0x44b8a0,0x4674f8):cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
def call(a,arg=None):
    put(stack+0x8000,stop,*([] if arg is None else [arg]));cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,user+0x7c if a==0x4e0740 else user);cpu.reg_write(UC_X86_REG_ESI,user);cpu.emu_start(a,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original did not return')
def init():
    cpu.mem_write(user,bytes(0x10000));cpu.mem_write(agent,bytes(0x10000));cpu.mem_write(mem,bytes(0x10000));put(user,vt);put(user+0x68,42);put(user+0xc8,agent);put(user+0x578c,11);cpu.mem_write(user+0x57c0,b'\x01');events.clear()
def enqueue(i,opcode):
    record=mem+i*64;frame=mem+0x4000+i*64;cpu.mem_write(frame,struct.pack('<HHI',8,opcode,0));put(record+4,frame);call(0x4e0740,record)
def update(location):put(user+0x5758,location);call(0x466cc0)
print(f'# ps_game.exe SHA256 {digest}; FIFO Push/Pop 004e0740/004e07a0; UpdatePackets 00466cc0; OnRecv 00466d20; PacketProcessing 00466da0; PacketCharacter 0046bf20; DB/zone/crypto/OS ports')
init();enqueue(0,0x104);enqueue(1,0x201);update(1);assert events==['DB_0407','IGNORE'] and get(user+0x9c)==0 and cpu.mem_read(user+0x57c0,1)==b'\x00';print('SELECTION_BATCH',','.join(events),get(user+0x9c))
init();enqueue(0,0x104);update(1);enqueue(1,0x201);put(user+0x5758,2);assert get(user+0x9c)==1 and events==['DB_0407'];print('WORLD_WAIT',','.join(events),get(user+0x9c));update(3);assert events==['DB_0407','ENTER'] and get(user+0x9c)==0;print('ZONE_WAIT',','.join(events),get(user+0x9c))
init();enqueue(0,0x201);update(2);assert events==['IGNORE'] and get(user+0x9c)==0;print('PUBLICATION_GAP',','.join(events),get(user+0x9c))
