#!/usr/bin/env python3
"""Original ps_game NProtectCSARecv with auth, diagnostic and close ports."""
import itertools,struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESI,UC_X86_REG_ESP
from original_oracle import load_game
cpu,digest=load_game();stack,user,auth,stop=0x20000000,0x30000000,0x31000000,0x10000000
for a in (stack,user,auth,stop):cpu.mem_map(a,0x10000)
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
for address,body in ((0x51c0e0,b'\xc3'),(0x4d9510,b'\xc3'),(0x4d43a0,b'\xc2\x08\x00')):cpu.mem_write(address,body)
events=[];context={}
def hook(m,address,size,ctx):
    sp=m.reg_read(UC_X86_REG_ESP)
    if address==0x51c0e0:
        events.append('CHECK');assert [get(auth+0x1c+i*4) for i in range(4)]==context['answer'];m.reg_write(UC_X86_REG_EAX,context['error'])
    elif address==0x4d9510:events.append('LOG')
    elif address==0x4d43a0:events.append('CLOSE'+str(get(sp+4)))
for address in (0x51c0e0,0x4d9510,0x4d43a0):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
print(f'# ps_game.exe SHA256 {digest}; CUser::NProtectCSARecv 0044b7e0; auth/log/close ports')
for present,location,received,error in itertools.product((0,1),(0,1,3,4),(0,1),(0,17)):
    cpu.mem_write(user,bytes(0x10000));cpu.mem_write(auth,bytes(0x10000));cpu.mem_write(user+0x5518,bytes([received]));put(user+0x5758,location);put(user+0x5520,auth if present else 0)
    answer=[0x11223344,0x55667788,0x89abcdef,0xffeeddcc];put(stack+0x8000,stop,*answer[1:]);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_ECX,answer[0]);events.clear();context.update(answer=answer,error=error)
    cpu.emu_start(0x44b7e0,stop,count=10000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
    actual=cpu.mem_read(user+0x5518,1)[0];result=cpu.reg_read(UC_X86_REG_EAX)&0xff
    expected=[] if not present else ['CLOSE111'] if not location else ['CHECK'] if not error else ['CHECK','LOG','CLOSE111']
    assert events==expected and actual==(1 if present and location and not error else received) and result==(1 if present and location and not error else 0)
    print(present,location,received,error,result,actual,','.join(events) or '-')
