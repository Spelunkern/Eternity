#!/usr/bin/env python3
"""Execute GuildMgrSave::DBSaveItem 004105b0 in the immutable DB-agent.

Ghidra: work/guild-bank-save and work/item-save-time. Actual slot/journal loops,
marker changes, error/recovery paths and time conversion execute. Database lease,
ODBC, CRT formatting, delete-node allocation, locks and logging are ports.
"""
import random
import re
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent')
    stack,guild,items,nodes,database,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
    for at in (stack,guild,items,nodes,database,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def string(at):
        result=bytearray()
        while (byte:=cpu.mem_read(at+len(result),1))!=b'\0':result.extend(byte)
        return result.decode('ascii')
    state={};events=[]
    def operation(text):
        match=re.fullmatch(r'\{\?=call (\w+)\((.*)\)\}',text)
        if not match:raise RuntimeError('Unexpected original SQL: '+repr(text))
        procedure,args=match.groups();parameters=[]
        for field in re.findall(r"'[^']*'|[^,]+",args):
            field=field.strip();parameters.append('s'+field[1:-1].encode().hex() if field.startswith("'") else 'i'+str(int(field)))
        return procedure.lower()+' '+' '.join(parameters)
    def hook(m,address,size,context):
        sp=m.reg_read(UC_X86_REG_ESP)
        if address==0x44447f:
            dest,fmt=word(sp+4),string(word(sp+8));cursor=sp+12;values=[]
            for match in re.finditer(r'%(I64d|(?:\.\d+)?[ducs])',fmt):
                kind=match[1];value=word(cursor);cursor+=4
                if kind=='I64d':value=struct.unpack('<q',m.mem_read(cursor-4,8))[0];cursor+=4
                elif kind.endswith('d') and value>=0x80000000:value-=0x100000000
                elif kind=='s':value=string(value)
                elif kind=='c':value=chr(value&255)
                values.append(value)
            rendered=fmt.replace('%I64d','%d')%tuple(values);m.mem_write(dest,rendered.encode()+b'\0');m.reg_write(UC_X86_REG_EAX,len(rendered))
        elif address==0x43ccc0:
            failed=state['pass']==0 and state['mode']==1;events.append('ACQUIRE '+str(int(not failed)));m.reg_write(UC_X86_REG_EAX,0 if failed else database)
        elif address==0x43cd80:events.append('RELEASE')
        elif address==0x43e6f0:events.append('PREPARE')
        elif address==0x43f410:
            current=state['attempt'];state['attempt']+=1
            result=0
            if state['pass']==0:
                if state['mode']>=6:result=state['mode']-5
                elif state['mode']>=2 and current==state['fail_at']:result=state['mode']-1
            state['result']=result;events.append('EXEC '+operation(string(sp+0x240))+' | '+str(result));put(word(sp+20),0xffffffff if result==4 else 1)
            m.reg_write(UC_X86_REG_EAX,0xffffffff if result==1 else 0)
        elif address==0x43f350:m.reg_write(UC_X86_REG_EAX,0xffffffff if state['result']==2 else 0)
        elif address==0x43f3d0:m.reg_write(UC_X86_REG_EAX,0xffffffff if state['result']==3 else 0)
        elif address==0x432d80:events.append('ERROR')
        elif address==0x40e0d0:
            at=nodes+0x8000+state['alloc']*16;state['alloc']+=1;m.mem_write(at,bytes(16));m.reg_write(UC_X86_REG_EAX,at)
    for at,cleanup in ((0x44447f,0),(0x43ccc0,0),(0x43cd80,4),(0x43e6f0,0),(0x43f410,28),(0x43f350,4),(0x43f3d0,0),(0x432d80,0),(0x40e0d0,0),(0x444dc3,0),(stop+0x100,4)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    for at in (0x4600d4,0x4600d8):put(at,stop+0x100)
    rng=random.Random(0x4105b0)
    print(f'# ps_dbAgent.exe SHA256 {digest}; GuildMgrSave::DBSaveItem 004105b0; actual loops/markers/journals/time; ODBC/CRT/lease/OS ports')
    for case in range(144):
        for at in (guild,items,nodes,database):cpu.mem_write(at,bytes(0x10000))
        put(0x470b6c,0,0,0);gid=rng.getrandbits(32);mode=case%10;fail_at=(case//10)%5;count=(0,1,39,40,41,80,81)[case%7]
        put(guild+12,gid);root=nodes;links=[nodes+32+i*16 for i in range(count)];put(root,links[-1] if links else root,links[0] if links else root);put(guild+0x444,root);put(guild+0x464,count)
        state.update(mode=mode,fail_at=fail_at,alloc=0);print('CASE',gid,mode,fail_at)
        for i,at in enumerate(links):
            uid=rng.getrandbits(64);put(at,root if i==0 else links[i-1],root if i+1==len(links) else links[i+1]);cpu.mem_write(at+8,struct.pack('<Q',uid));print('DEL',uid)
        pointers={}
        for i,slot in enumerate(sorted(rng.sample(range(240),12))):
            at=items+i*128;pointers[slot]=at;put(guild+0x6c+slot*4,at)
            uid=rng.getrandbits(64);iid=rng.getrandbits(32);kind,tid,n=rng.randrange(256),rng.randrange(256),rng.randrange(256);quality=rng.choice((0,32767,32768,65535));gems=bytes(rng.randrange(256) for _ in range(6));craft=rng.choice(('', '01020304050607080903','12345'));made=rng.choice((0,0xffffffff,0x6154c34e,rng.getrandbits(32)));make=rng.choice((77,85,83));save=rng.choice((-1,0,1,1,2,2,17))
            put(at+8,save,0);cpu.mem_write(at+16,struct.pack('<QI3BxH',uid,iid,kind,tid,n,quality));cpu.mem_write(at+34,gems);cpu.mem_write(at+40,craft.encode()+b'\0');put(at+64,made);cpu.mem_write(at+68,bytes([make]))
            print('ITEM',slot,uid,iid,kind,tid,n,quality,gems.hex(),craft or '-',made,make,save)
        for turn in range(2):
            state.update(**{'pass':turn,'attempt':0,'result':0});events.clear();cpu.mem_write(stack,bytes(0x10000));sp=stack+0xf000;put(sp,stop,guild);cpu.reg_write(UC_X86_REG_ESP,sp)
            cpu.emu_start(0x4105b0,stop,count=3000000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original guild bank save did not return')
            print('PASS',turn)
            for event in events:print('TRACE',event)
            print('RETURN',cpu.reg_read(UC_X86_REG_EAX))
            for slot,at in pointers.items():print('FLAG',slot,word(at+8))
            deleted=[];at=word(root+4)
            while at!=root:deleted.append(struct.unpack('<Q',cpu.mem_read(at+8,8))[0]);at=word(at+4)
            print('JOURNAL',*deleted);print('ENDPASS')
        print('END')


if __name__=='__main__':main()
