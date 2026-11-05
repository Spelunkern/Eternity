#!/usr/bin/env python3
"""Execute PacketComm's social delete/memo cases with controlled transport ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game

cpu, digest = load_game()
stack,user,packet,stop = 0x20000000,0x30000000,0x33000000,0x10000000
for address in (stack,user,packet,stop): cpu.mem_map(address,0x10000)
def put(address,*values): cpu.mem_write(address,struct.pack('<'+'I'*len(values),*values))
def get(address): return struct.unpack('<I',cpu.mem_read(address,4))[0]
events=[]
def capture(machine,address,size,context):
    sp=machine.reg_read(UC_X86_REG_ESP)
    if address in (0x4d4d20,0x4d4f10):
        events.append((address,bytes(machine.mem_read(get(sp+4),get(sp+8)))))
    elif address==0x40d7e0: machine.reg_write(UC_X86_REG_EAX,0)
for address,cleanup in ((0x4d4d20,8),(0x4d4f10,8),(0x40d7e0,4),(0x505316,0),(stop+0x100,4)):
    cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,capture,begin=address,end=address)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x100)
put(0x546f38,stop+0x200);put(0x546f44,stop+0x300)
def call(payload):
    events.clear();cpu.mem_write(packet,bytes(128));cpu.mem_write(packet,payload)
    put(stack+0xf000,stop,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,packet)
    cpu.emu_start(0x475870,stop,count=1000000)
    assert cpu.reg_read(UC_X86_REG_EIP)==stop
def reset():
    cpu.mem_write(user,bytes(0x10000));put(user+0x578c,42);put(user+0xdc,123)
    put(user+0x1780,2);put(user+0x1784,55);put(user+0x17d4,56)
    cpu.mem_write(user+0x1788,bytes([1,2,3])+b'First\0');cpu.mem_write(user+0x17d8,bytes([4,5,6])+b'Second\0')
    put(user+0x36c4,1);put(user+0x36c8,57);cpu.mem_write(user+0x36cc,b'Block\0')
cases=0
for friend in (True,False):
    for text in (b'',b"x'y",b'Z'*50):
        reset();ident=55 if friend else 57;op=0x2206 if friend else 0x220b
        call(struct.pack('<HIB',op,ident,len(text))+text+b'\0')
        assert len(events)==2 and events[0][0]==0x4d4f10 and events[1][0]==0x4d4d20
        db=events[0][1];response=events[1][1]
        assert db[:10]==struct.pack('<HII',0xc04 if friend else 0xc08,42,ident) and db[10:11+len(text)]==text+b'\0'
        assert response==struct.pack('<HIB',op,ident,len(text))+text+b'\0'
        cases+=1
    reset();ident=55 if friend else 57;op=0x2205 if friend else 0x220a
    call(struct.pack('<HI',op,ident))
    assert [kind for kind,_ in events]==[0x4d4f10,0x4d4d20]
    assert events[0][1]==struct.pack('<HII',0xc03 if friend else 0xc07,42,ident)
    assert events[1][1]==struct.pack('<HI',op,ident)
    if friend:
        assert get(user+0x1780)==1 and get(user+0x1784)==56 and bytes(cpu.mem_read(user+0x1788,3))==bytes([1,2,6])
    else: assert get(user+0x36c4)==0
    cases+=1
# Original checks the declared byte, then forces the memo's 51st byte to NUL.
# Friend permits declared 51; block permits only 50. Oversized requests are
# silently ignored without touching the retained friend/block data.
for friend,allowed,ignored in ((True,51,52),(False,50,51)):
    ident=55 if friend else 57;op=0x2206 if friend else 0x220b
    reset();call(struct.pack('<HIB',op,ident,allowed)+b'Z'*55+b'\0')
    assert len(events)==2 and events[1][1]==struct.pack('<HIB',op,ident,50)+b'Z'*50+b'\0'
    cases+=1
    reset();before=bytes(cpu.mem_read(user+0x1780,0x3000));call(struct.pack('<HIB',op,ident,ignored)+b'Z'*55+b'\0')
    assert not events and bytes(cpu.mem_read(user+0x1780,0x3000))==before
    cases+=1
print(cases,'original social client mutation cases passed; peer lookup and transports intercepted; ps_game.exe SHA256',digest)
