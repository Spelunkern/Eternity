#!/usr/bin/env python3
"""Execute original QuestStart AND SetGameLogMain; observe exact log/client bytes.
Only OS GetLocalTime, transports and security-cookie check are ports.
Client transport mutates live user fields to verify the log's earlier snapshot.
"""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,user,record,npc,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000
    for p in (stack,user,record,npc,stop):cpu.mem_map(p,0x10000)
    def put(p,v):cpu.mem_write(p,struct.pack('<I',v))
    def word(p):return struct.unpack('<I',cpu.mem_read(p,4))[0]
    state={}
    def port(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==stop+0x100:
            year,month,day,hour,minute,second=state['calendar']
            cpu.mem_write(word(sp+4),struct.pack('<8H',year,month,0,day,hour,minute,second,999))
        elif address==0x4d4d20:
            state['client']=bytes(cpu.mem_read(word(sp+4),word(sp+8)))
            cpu.mem_write(user+0x128,bytes([0xed])*0x80);put(user+0x578c,0xfefefefe)
        elif address==0x4d4e50:state['log']=bytes(cpu.mem_read(word(sp+4),word(sp+8)))
    for p,n in ((stop+0x100,4),(0x4d4d20,8),(0x4d4e50,8),(0x505316,0)):
        cpu.mem_write(p,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,port,begin=p,end=p)
    put(0x5210dc,stop+0x100)
    rng=random.Random(0x47ea70)
    print(f'# ps_game.exe SHA256 {digest}; QuestStart 0047ea70; SetGameLogMain 0045b0d0; GetSystemTime 004cb8e0; 144-byte log and 8-byte client')
    def string(n):return bytes(rng.choice(b'AbcXYZ09_') for _ in range(n))
    for case in range(256):
        fill=rng.randrange(256);cpu.mem_write(stack,bytes([fill])*0x10000);cpu.mem_write(user,bytes(0x10000))
        q=bytearray(464);struct.pack_into('<H',q,0,rng.randrange(65536));qname=string(case%32);q[2:2+len(qname)]=qname
        # Bytes after first NUL are deliberately nonzero in selected inputs.
        if case%3==0:q[3+len(qname):34]=bytes([0xa7])*max(0,31-len(qname))
        c=bytearray(74)
        for p in (2,25,52,58,62,66,70):struct.pack_into('<I',c,p,rng.randrange(1<<32))
        for p in (50,56):struct.pack_into('<H',c,p,rng.randrange(65536))
        account=string(case%19);character=string(case%21);c[6:6+len(account)]=account;c[29:29+len(character)]=character
        for dst,src,size in ((0x578c,2,4),(0x128,25,4),(0x136,50,2),(0x13c,52,4),(0x164,56,2),(0xd0,58,12)):
            cpu.mem_write(user+dst,bytes(c[src:src+size]))
        cpu.mem_write(user+0x5794,account+b'\0');cpu.mem_write(user+0x188,character+b'\0');cpu.mem_write(record,bytes(q))
        present=case%2;kind=rng.randrange(1<<32);name=string(case%32);handle=rng.randrange(1<<32)
        put(npc+0x14,handle);put(npc+0x34,kind);put(npc+0x50,npc+0x100);cpu.mem_write(npc+0x114,name+b'\0')
        state.clear();state['calendar']=(rng.choice((0,15,16,1999,2000,2026,65535)),rng.randrange(1,13),rng.randrange(1,32),rng.randrange(24),rng.randrange(60),rng.randrange(60))
        sp=stack+0x8000;put(sp,stop);put(sp+4,record)
        for reg,v in ((UC_X86_REG_ESP,sp),(UC_X86_REG_ECX,user),(UC_X86_REG_EDX,npc if present else 0)):cpu.reg_write(reg,v)
        cpu.emu_start(0x47ea70,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original QuestStart did not return')
        if len(state['log'])!=144 or len(state['client'])!=8:raise RuntimeError('Unexpected original packet length')
        c[70:74]=state['log'][70:74]
        print(case,fill,bytes(q).hex(),bytes(c).hex(),present,kind,name.hex() or '-',handle,*state['calendar'],state['client'].hex(),state['log'].hex())

if __name__=='__main__':main()
