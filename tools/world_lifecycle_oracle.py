#!/usr/bin/env python3
"""Original CUser EnterWorld/LeaveWorld control, log packets and call order.

Ghidra work/world-user-lifecycle and work/world-lifecycle-details. Gameplay/admin,
calendar, offline-money hash cache, CRT formatting and transport are explicit ports.
SetGameLogMain executes; only undefined string/manager padding is normalized.
"""
import ctypes
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,user,party,world,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for at in (stack,user,party,world,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def string(at):return bytes(cpu.mem_read(at,256)).split(b'\0')[0]
    events=[];state={};libc=ctypes.CDLL(None)
    steps={0x432000:0,0x446960:1,0x446510:2,0x476300:3,0x4757a0:4,0x40d520:5,0x4703f0:6,0x448d10:7,0x448990:8,0x44b2c0:9,0x44b330:10}
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address in steps:events.append('STEP '+str(steps[address]))
        elif address==0x448920:events.append('ADMIN');machine.reg_write(UC_X86_REG_EAX,state['admin'])
        elif address==0x4d43a0:events.append('CLOSE '+str(word(sp+4)))
        elif address==0x4cb8e0:events.append('CALENDAR');machine.reg_write(UC_X86_REG_EAX,0x61510b9c)
        elif address==0x40d8c0:
            key=word(sp+4);old=state['cache'].pop(key,None);events.append('TAKE '+str(key)+' '+str(old if old is not None else -1));machine.reg_write(UC_X86_REG_EAX,int(old is not None))
            if old is not None:put(word(sp+8),old)
        elif address==0x40d920:
            key=word(sp+4);value=machine.reg_read(UC_X86_REG_EAX);state['cache'][key]=value;events.append('SAVE '+str(key)+' '+str(value))
        elif address==0x504495:
            dest,fmt=word(sp+4),string(word(sp+8));out=ctypes.create_string_buffer(512);name=string(word(sp+12))
            if b'Old=' in fmt:libc.sprintf(out,fmt,ctypes.c_char_p(name),ctypes.c_uint(word(sp+16)),ctypes.c_uint(word(sp+20)))
            else:
                x,z=struct.unpack('<dd',cpu.mem_read(sp+24,16));libc.sprintf(out,fmt,ctypes.c_char_p(name),ctypes.c_int(word(sp+16)),ctypes.c_int(word(sp+20)),ctypes.c_double(x),ctypes.c_double(z))
            cpu.mem_write(dest,out.value+b'\0');machine.reg_write(UC_X86_REG_EAX,len(out.value))
        elif address in (0x4d4e50,0x4d4f10):
            packet=bytearray(cpu.mem_read(word(sp+4),word(sp+8)))
            if address==0x4d4e50:
                for offset,width in ((6,19),(29,21)):
                    end=packet.index(0,offset,offset+width);packet[end+1:offset+width]=bytes(offset+width-end-1)
                events.append('LOG '+packet.hex())
            else:
                end=packet.index(0,3);packet[end+1:]=bytes(len(packet)-end-1);events.append('MANAGER '+packet.hex())
    targets=[(at,4 if at in (0x446960,0x446510,0x4757a0,0x40d520,0x448990) else 0) for at in steps]
    targets += [(0x448920,0),(0x4d43a0,8),(0x4cb8e0,0),(0x40d8c0,8),(0x40d920,4),(0x504495,0),(0x4d4e50,8),(0x4d4f10,8),(0x505316,0)]
    for at,cleanup in targets:cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    put(0x546f38,world)
    def call(address):
        put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,user)
        cpu.emu_start(address,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original world lifecycle did not return')
    rng=random.Random(0x44a820)
    print(f'# ps_game.exe SHA256 {digest}; EnterWorld 0044a820, LeaveWorld 0044aad0, SetGameLogMain 0045b0d0; gameplay/admin/calendar/cache/CRT/transport ports; undefined string tails normalized')
    for case in range(1024):
        cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));permission=(0,10,11,255)[case%4];admin=(case//4)%2;party_kind=(case//8)%3;vehicle=(0,1,2,3,0xffffffff)[(case//24)%5];auto=(case//120)%2;reconnect=(0,1,2)[(case//240)%3];entry=(-1,3,4,0x7fffffff)[(case//3)%4];question=case%2
        old=rng.choice((None,0,1,10000,0xffffffff));money=rng.choice((0,9999,10000,10001,999999999,1000000000,1000000001,0xffffffff));initial=rng.choice((0,1,10000,0xffffffff));cash,ip,disconnected=(rng.getrandbits(32) for _ in range(3));stats=[rng.getrandbits(16) for _ in range(8)];mode=case%256
        put(user+0x128,9001);put(user+0x578c,42);put(user+0x13c,0x12345678);put(user+0x140,money,initial);put(user+0x148,0xaabbccdd);put(user+0x70,ip);put(user+0x5858,disconnected);cpu.mem_write(user+0x5854,bytes([mode]));put(user+0x5a20,cash);cpu.mem_write(user+0x136,struct.pack('<HHH',30,stats[1],stats[0]));cpu.mem_write(user+0x170,struct.pack('<6H',*stats[2:]));cpu.mem_write(user+0x164,struct.pack('<H',20));put(user+0xd0,0x41a40000,0xc1800000,0xc2000000);cpu.mem_write(user+0x5794,b'account\0');cpu.mem_write(user+0x188,b'Hero\0');cpu.mem_write(user+0x5768,bytes([permission]));put(user+0x576c,question);put(user+0x173c,party if party_kind else 0);cpu.mem_write(party+0x114,bytes([party_kind==2]));put(user+0x1460,vehicle);put(user+0x57e8,auto);put(user+0x57e0,reconnect);put(user+0x5814,entry)
        state.update(admin=admin,cache={} if old is None else {9001:old});print('CASE',permission,admin,party_kind,vehicle,auto,reconnect,entry,question,money,initial,cash,ip,disconnected,mode,-1 if old is None else old,*stats)
        for operation,address in (('ENTER',0x44a820),('LEAVE',0x44aad0),('ENTER',0x44a820)):
            events.clear();call(address);print('OP',operation)
            for event in events:print('EVENT',event)
            print('ENDOP')
        print('END')


if __name__=='__main__':main()
