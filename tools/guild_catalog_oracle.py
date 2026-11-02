#!/usr/bin/env python3
"""Execute original game guild metadata/member load owners.

Ghidra work/guild-global-init. Real ID/member/directory trees and master/rank
correction execute; OS, heap pool/reset and case-insensitive name insertion are
ports. Guild reset port initializes previously constructed empty native trees.
"""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,guilds,members,heap,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000;manager=0x5476e0
    for at in (0,stack,data,stop):cpu.mem_map(at,0x10000)
    cpu.mem_map(guilds,0x100000);cpu.mem_map(members,0x100000);cpu.mem_map(heap,0x200000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def string(at,n):return bytes(cpu.mem_read(at,n)).split(b'\0')[0]
    state={};events=[]
    def allocate(n):at=state['heap'];state['heap']+=(n+15)&~15;return at
    def tree(at):
        head=allocate(24);put(at,0,head,0);put(head,head,head,head);cpu.mem_write(head+20,b'\x01\x01')
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x503a3f:machine.reg_write(UC_X86_REG_EAX,allocate(word(sp+4)))
        elif address==0x434cd0:
            state['ga']+=1;fail=state['fail_guild'] and state['ga']%3==0;events.append('GALLOC '+str(int(not fail)));at=0 if fail else guilds+len(state['guilds'])*0x1000
            if at:state['guilds'].append(at)
            machine.reg_write(UC_X86_REG_EAX,at)
        elif address==0x42bcd0:
            at=word(sp+4);cpu.mem_write(at,bytes(0x1000))
            for offset in (0x50c,0x538,0x564):tree(at+offset)
        elif address==0x434570:
            state['ma']+=1;fail=state['fail_member'] and state['ma']%4==0;events.append('MALLOC '+str(int(not fail)));at=members+state['ma']*64;cpu.mem_write(at,bytes(64));machine.reg_write(UC_X86_REG_EAX,0 if fail else at)
        elif address==0x4349e0:pass # Name index insertion is a separate boundary.
    for at,cleanup in ((0x503a3f,0),(0x503ba1,0),(0x434cd0,0),(0x42bcd0,4),(0x434570,0),(0x4349e0,12),(stop+0x100,4),(0x505316,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    for iat in (0x5210cc,0x5210d0):put(iat,stop+0x100)
    def nodes(at):
        head=word(at+4)
        def walk(p):
            if p==head:return []
            return walk(word(p))+[(word(p+12),word(p+16))]+walk(word(p+8))
        return walk(word(head+4))
    def text(at,n):raw=string(at,n);return raw.hex() if raw else '-'
    def snapshot():
        print('MAX',word(manager))
        for key,at in nodes(manager+0x1c):
            print('GUILD',key,text(at+12,25),text(at+37,21),word(at+64),word(at+68),cpu.mem_read(at+72,1)[0],word(at+76),cpu.mem_read(at+84,1)[0],text(at+86,65),cpu.mem_read(at+85,1)[0],word(at+80),word(at+156),word(at+60))
            for phase,offset in (('OFFLINE',0x538),('APPLICATION',0x564)):
                for mid,member in nodes(at+offset):print('MEMBER',key,phase,mid,word(member+8),word(member+40),word(member+36),word(member+44),text(member+12,21),word(member+52),word(member+56))
        print('ENDSTATE')
    def send(packet):
        events.clear();cpu.mem_write(data,packet);put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_EBX,data);cpu.emu_start(0x408e00,stop,count=2000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original guild catalog did not return')
        print('PACKET',packet.hex())
        for event in events:print('EVENT',event)
        snapshot()
    def fixed(raw,n):return raw+b'\0'+bytes(n-len(raw)-1)
    rng=random.Random(0x431870)
    print(f'# ps_game.exe SHA256 {digest}; PacketGuild 00408e00, InitAddGuild 00431870, InitAddUser 0042cf70; actual ID/member trees; reset/pool/name-index/OS ports')
    for case in range(96):
        cpu.mem_write(manager,bytes(0x200));state.update(heap=heap,guilds=[],ga=0,ma=0,fail_guild=case%3==0,fail_member=case%4==0);tree(manager+0x1c);tree(manager+0xa0);print('CASE',case)
        send(struct.pack('<HI',0xb01,rng.getrandbits(32)))
        rows=[]
        for i in range(5):
            gid=(1,2,1,0xffffffff,0)[i];rows.append(struct.pack('<I',gid)+fixed(b'Guild'+str(i).encode(),25)+fixed(b'Master',21)+struct.pack('<BIBIB',i%3,rng.getrandbits(32),i+1,rng.getrandbits(32),i%2)+fixed(b'Notice'+str(i).encode(),65)+struct.pack('<BII',i%2,rng.getrandbits(32),rng.getrandbits(32)))
        send(struct.pack('<HB',0xb02,len(rows))+b''.join(rows))
        for gid in (1,2,99,0xffffffff,0):
            entries=[]
            for i in range(9):
                mid=(0,1,2,1,3,4,5,6,7)[i];name=b'Master' if i in (1,3,7) else b'User'+str(i).encode();entries.append(struct.pack('<IHBB',mid,65530+i%6,i%6,(0,1,2,3,255)[(case+i)%5])+fixed(name,21))
            send(struct.pack('<HIB',0xb03,gid,len(entries))+b''.join(entries))
        send(struct.pack('<HI',0xb01,3));print('END')


if __name__=='__main__':main()
