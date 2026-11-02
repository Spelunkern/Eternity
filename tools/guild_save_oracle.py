#!/usr/bin/env python3
"""Execute full GuildMgrSave::DBSave (00410190), preserving its real ID tree.

Ghidra work/guild-save-owner. Original metadata formatting/markers, timeout
retry limits, unsigned map traversal and bank/NPC retry ordering execute.
Child save owners (separately verified), SQL/pool, CRT, locks and log are ports.
"""
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,guilds,tree,database,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x10000000;manager=0x4710d8
    for at in (stack,guilds,tree,database,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def string(at):
        out=bytearray()
        while (b:=cpu.mem_read(at+len(out),1))!=b'\0':out.extend(b)
        return out.decode('ascii')
    def operation(query):
        match=re.fullmatch(r"EXEC (\w+) (.*) ",query)
        if not match:raise RuntimeError('Unexpected metadata SQL: '+repr(query))
        proc,args=match.groups();fields=[]
        for raw in re.findall(r"'[^']*'|[^,]+",args):
            raw=raw.strip();fields.append('s'+raw[1:-1].encode().hex() if raw.startswith("'") else 'i'+str(int(raw)))
        return proc.lower()+' '+' '.join(fields)
    events=[];state={}
    def result(mode,attempt):
        if mode in (2,):return False,'HYT00',1222
        if mode==3 and attempt<2:return False,'HYT00',1222
        if mode in (4,5):return False,'42000',105 if mode==4 else 2627
        if mode==6:return False,'HYT01',105
        if mode in (7,8):return (False,'HYT00',1222) if attempt==0 else (False,'42000',105 if mode==7 else 2627)
        if mode==9:return (False,'HYT00',1222) if attempt<2 else (False,'HYT01',999)
        return True,'-',0
    def hook(m,address,size,context):
        sp=m.reg_read(UC_X86_REG_ESP)
        if address==0x44447f:
            dest,fmt=word(sp+4),string(word(sp+8));cursor=sp+12;values=[]
            for match in re.finditer(r'%[ds]',fmt):
                value=word(cursor);cursor+=4;values.append(string(value) if match[0]=='%s' else value if value<0x80000000 else value-0x100000000)
            rendered=fmt%tuple(values);m.mem_write(dest,rendered.encode()+b'\0');m.reg_write(UC_X86_REG_EAX,len(rendered))
        elif address==0x43ccc0:
            ok=state['mode']!=1;events.append('ACQUIRE '+str(int(ok)));m.reg_write(UC_X86_REG_EAX,database if ok else 0)
        elif address==0x43cd80:events.append('RELEASE')
        elif address==0x43e6f0:events.append('PREPARE')
        elif address==0x43f350:
            query=string(word(sp+4));gid=int(query.split()[2].split(',')[0])&0xffffffff;attempt=state['queries'].get(gid,0);state['queries'][gid]=attempt+1;ok,sqlstate,native=result(state['mode'],attempt)
            if not ok:put(database+0x10,native);m.mem_write(database+0x14,sqlstate.encode()+b'\0')
            events.append('EXEC '+operation(query)+' | '+str(int(ok))+' '+sqlstate+' '+str(native));m.reg_write(UC_X86_REG_EAX,0 if ok else 0xffffffff)
        elif address in (0x4105b0,0x4103e0):
            gid=word(word(sp+4)+12);kind='BANK' if address==0x4105b0 else 'NPC';key=(kind,gid);attempt=state['children'].get(key,0);state['children'][key]=attempt+1;profile=(state['case']+state['indexes'][gid]+(kind=='NPC'))%3;returned=0 if profile==0 or profile==1 and attempt>0 else profile
            events.append(kind+' '+str(gid)+' '+str(returned));m.reg_write(UC_X86_REG_EAX,returned)
        elif address==0x432d80:events.append('ERROR')
    for at,cleanup in ((0x44447f,0),(0x43ccc0,0),(0x43cd80,4),(0x43e6f0,0),(0x43f350,4),(0x4105b0,4),(0x4103e0,4),(0x432d80,0),(0x444dc3,0),(stop+0x100,4)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    rng=random.Random(0x410190)
    print(f'# ps_dbAgent.exe SHA256 {digest}; GuildMgrSave::DBSave 00410190; original tree/metadata/timeout/child retry loops; verified child owners/SQL/CRT/lease/OS ports')
    for case in range(160):
        for at in (guilds,tree,database):cpu.mem_write(at,bytes(0x10000))
        cpu.mem_write(manager,bytes(0x100));events.clear();ids=sorted(rng.sample((0,42,0x80000000,0xffffffff),(case//10)%5));head=tree;nodes=[tree+32+i*32 for i in range(len(ids))];records={}
        put(manager+0x1c,0,head,len(ids));cpu.mem_write(head+20,b'\x01\x01')
        def build(lo,hi,parent):
            if lo>=hi:return head
            mid=(lo+hi)//2;at=nodes[mid];put(at,build(lo,mid,at),parent,build(mid+1,hi,at),ids[mid],records[ids[mid]]);cpu.mem_write(at+20,b'\x01\x00');return at
        print('CASE',case,case%10)
        for i,gid in enumerate(ids):
            at=guilds+i*0x1000;records[gid]=at;points,etin,returned,keep=[rng.getrandbits(32) for _ in range(4)];buy,rank,use=[rng.randrange(256) for _ in range(3)];remark=rng.choice(('', 'Notice'+str(i),'R'*64));marker=2 if i<2 or case%3==0 else rng.choice((0,2,17))
            put(at+12,gid,points,etin);cpu.mem_write(at+24,remark.encode()+b'\0');cpu.mem_write(at+0x59,bytes([rank]));put(at+0x5c,marker);cpu.mem_write(at+0x60,bytes([buy,use]));put(at+0x64,returned,keep)
            print('GUILD',gid,points,etin,returned,keep,buy,rank,use,remark.encode().hex() or '-',marker)
        root=build(0,len(ids),head);put(head,nodes[0] if nodes else head,root,nodes[-1] if nodes else head)
        state.update(case=case,mode=case%10,queries={},children={},indexes={gid:i for i,gid in enumerate(ids)})
        cpu.mem_write(stack,bytes(0x10000));put(stack+0xf000,stop);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,manager);cpu.emu_start(0x410190,stop,count=300000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original guild save coordinator did not return')
        print('RUN')
        for event in events:print('TRACE',event)
        print('RETURN',cpu.reg_read(UC_X86_REG_EAX)&255)
        for gid,at in records.items():print('STATE',gid,word(at+0x5c),string(at+24).encode().hex() or '-')
        print('END')


if __name__=='__main__':main()
