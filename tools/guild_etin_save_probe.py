#!/usr/bin/env python3
"""Execute original PacketGuild -> UpdateGuildEtin/KeepEtin -> DBSave SQL formatting.

Ghidra work/guild-bank-save, work/guild-save-updates, work/guild-save-owner.
Real guild lookup, forced terminators, copying and formatting execute. Query,
verified child save owners, CRT, lease, locks and logging are ports.
"""
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,guild,tree,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000;manager=0x4710d8
    for at in (stack,guild,tree,data,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def string(at):
        out=bytearray()
        while (b:=cpu.mem_read(at+len(out),1))!=b'\0':out.extend(b)
        return bytes(out)
    queries=[]
    def hook(m,address,size,context):
        sp=m.reg_read(UC_X86_REG_ESP)
        if address==0x44447f:
            dest,fmt=word(sp+4),string(word(sp+8)).decode();values=[];cursor=sp+12
            for match in re.finditer(r'%[ds]',fmt):
                value=word(cursor);cursor+=4;values.append(string(value).decode() if match[0]=='%s' else value if value<0x80000000 else value-0x100000000)
            rendered=fmt%tuple(values);m.mem_write(dest,rendered.encode()+b'\0');m.reg_write(UC_X86_REG_EAX,len(rendered))
        elif address==stop+0x200:
            ptr,value=word(sp+4),word(sp+8);old=word(ptr);put(ptr,value);m.reg_write(UC_X86_REG_EAX,old)
        elif address==0x43ccc0:m.reg_write(UC_X86_REG_EAX,data+0x8000)
        elif address==0x43f350:queries.append(string(word(sp+4)));m.reg_write(UC_X86_REG_EAX,0)
        elif address in (0x4105b0,0x4103e0):m.reg_write(UC_X86_REG_EAX,0)
    for at,cleanup in ((0x44447f,0),(0x43ccc0,0),(0x43cd80,4),(0x43e6f0,0),(0x43f350,4),(0x4105b0,4),(0x4103e0,4),(0x432d80,0),(0x444dc3,0),(stop+0x100,4),(stop+0x200,8)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    put(0x4600d0,stop+0x200)
    def call(at,register,value):
        cpu.mem_write(stack,bytes(0x10000));put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(register,value);cpu.emu_start(at,stop,count=200000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original guild remark path did not return')
    print(f'ps_dbAgent.exe SHA256 {digest}; PacketGuild 0040ba00, UpdateGuildEtinAndReturnCnt 0040fd80, UpdateGuildKeepEtin 0040fec0, DBSave 00410190')
    for op,present,value,count in ((0xb12,True,0x87654321,0xabcdef01),(0xb12,False,0,0),(0xb13,True,0x80000001,0),(0xb13,False,0,0)):
        for at in (guild,tree,data):cpu.mem_write(at,bytes(0x10000))
        cpu.mem_write(manager,bytes(0x100));gid=10
        put(guild+12,gid,123,456);cpu.mem_write(guild+24,b'Previous\0');put(guild+0x5c,1);cpu.mem_write(guild+0x60,bytes([7,3]));put(guild+0x64,9,111)
        head,node=tree,tree+32;put(manager+0x1c,0,head,1);put(head,node,node,node);cpu.mem_write(head+20,b'\x01\x01');put(node,head,head,head,gid,guild);cpu.mem_write(node+20,b'\x01\x00')
        packet=(struct.pack('<HIIIII',op,gid if present else 11,value,99,88,count) if op==0xb12 else struct.pack('<HII',op,gid if present else 11,value))
        cpu.mem_write(data,packet);queries.clear();call(0x40ba00,UC_X86_REG_EAX,data)
        assert word(guild+0x14)==(value if present and op==0xb12 else 456)
        assert word(guild+0x64)==(count if present and op==0xb12 else 9)
        assert word(guild+0x68)==(value if present and op==0xb13 else 111)
        assert word(guild+0x5c)==(2 if present else 1)
        if present:
            call(0x410190,UC_X86_REG_ECX,manager)
            assert len(queries)==1 and queries[0].startswith(b'EXEC usp_Save_GuildInfo_E2 ') and word(guild+0x5c)==0
    print('4 original guild etin replica and save cases passed')

if __name__=='__main__':main()
