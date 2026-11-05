#!/usr/bin/env python3
"""Run ps_game PacketComm block-add against a controlled world and transports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu,digest=load_game()
stack,user,peer,packet,stop=0x20000000,0x30000000,0x31000000,0x33000000,0x10000000
for address in (stack,user,peer,packet,stop): cpu.mem_map(address,0x10000)
def put(address,*values): cpu.mem_write(address,struct.pack('<'+'I'*len(values),*values))
def get(address): return struct.unpack('<I',cpu.mem_read(address,4))[0]
events=[]; found=peer; lookups=0
def capture(machine,address,size,context):
    global lookups
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address in (0x4d4d20,0x4d4f10): events.append((address,bytes(machine.mem_read(get(sp+4),get(sp+8)))))
    elif address==0x40d690:
        lookups+=1;machine.reg_write(UC_X86_REG_EAX,found)
for address,cleanup in ((0x4d4d20,8),(0x4d4f10,8),(0x40d690,4),(0x505316,0),(stop+0x100,4)):
    cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,capture,begin=address,end=address)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x100)
put(0x546f38,stop+0x200);put(0x546f44,stop+0x300)
def reset():
    global found,lookups
    found=peer;lookups=0;events.clear()
    for address in (user,peer,packet):cpu.mem_write(address,bytes(0x10000))
    put(user+0x578c,42);put(user+0xdc,123);cpu.mem_write(user+0x12d,b'\x01')
    put(peer+0x128,88);put(peer+0xdc,99);cpu.mem_write(peer+0x12d,b'\x01');cpu.mem_write(peer+0x188,b'Target\0')
    cpu.mem_write(packet,struct.pack('<H',0x2209)+b'Target\0'+bytes(14))
def call():
    put(stack+0xf000,stop,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,packet)
    cpu.emu_start(0x475870,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
reset();call()
assert get(user+0x36c4)==1 and get(user+0x36c8)==88
assert [kind for kind,_ in events]==[0x4d4f10,0x4d4d20,0x4d4d20]
assert events[0][1][:17]==struct.pack('<HII',0xc06,42,99)+b'Target\0'
assert events[1][1]==b'\x0e\x22\x01' and events[2][1][:13]==struct.pack('<HI',0x2209,99)+b'Target\0'
for case in ('missing','self','country','name','id','full'):
    reset()
    if case=='missing':found=0
    if case=='self':found=user
    if case=='country':cpu.mem_write(peer+0x12d,b'\x02')
    if case=='name':put(user+0x36c4,1);cpu.mem_write(user+0x36cc,b'Target\0')
    if case=='id':put(user+0x36c4,1);put(user+0x36c8,88);cpu.mem_write(user+0x36cc,b'Other\0')
    if case=='full':
        put(user+0x36c4,100)
        for i in range(100):put(user+0x36c8+76*i,1000+i);cpu.mem_write(user+0x36cc+76*i,b'Other\0')
    call()
    if case=='full':assert get(user+0x36c4)==100 and len(events)==3 and events[1][1]==b'\x0e\x22\x01'
    else:assert len(events)==1 and events[0][1]==b'\x0e\x22\x00'
    if case=='name':assert lookups==0
print('7 original block-add cases passed; distinct live list ID and response/DB ID; ps_game.exe SHA256',digest)
