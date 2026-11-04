#!/usr/bin/env python3
"""Complete admission and SendDBAgentCharGetInfo, isolated learned phase comparisons."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
    c,digest=load_game();base=0x30000000;c.mem_map(base,0x100000)
    user,manager,definition,packet,heap,stack,stop=base,base+0x10000,base+0x11000,base+0x12000,base+0x20000,base+0x80000,base+0xff000
    state={};events=[]
    def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
    def hook(cpu,at,size,data):
        pop=0;result=None
        if at in (stop+16,stop+32):pop=4
        elif at==stop+48:
            result=state['tick']
            if not state['seed']:
                events.append('T,'+bytes(cpu.mem_read(stack-0x14a4,7)).hex())
                if state['flags']&1:put(user+0xac0,0);put(user+0x1444,0)
                if state['flags']&4:put(user+0x578c,55)
        elif at==stop+64:
            p=get(cpu.reg_read(UC_X86_REG_ESP)+4);put(p,get(p)+1);result=get(p);pop=4
        elif at==0x503a3f:
            result=heap+state['allocated']*64;state['allocated']+=1;cpu.mem_write(result,bytes(52))
        elif at==0x402770:result=definition
        elif at==0x46cb63:
            state['scratch']=bytes(cpu.mem_read(stack-0x14a4,775));return
        elif at==0x4d4f10:
            sp=cpu.reg_read(UC_X86_REG_ESP);wire=bytes(cpu.mem_read(get(sp+4),get(sp+8)));op=struct.unpack_from('<H',wire)[0]
            if op==0x804:
                state['wire']=wire.hex();events.append('D')
            else:state['other'].append(op)
            if op==0x804 and state['flags']&2:put(user+0xac0,0);put(user+0x1444,0);put(user+0x578c,99)
            pop=8
        else:return
        sp=cpu.reg_read(UC_X86_REG_ESP);ret=get(sp);cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:cpu.reg_write(UC_X86_REG_EAX,result)
        cpu.reg_write(UC_X86_REG_EIP,ret)
    c.hook_add(UC_HOOK_CODE,hook)
    for p,v in ((0x5210cc,stop+16),(0x5210d0,stop+32),(0x5210e4,stop+48),(0x5210f8,stop+64)):put(p,v)
    def call(at,args,regs=()):
        c.mem_write(stack-0x3000,bytes((state['fill'],))*0x3000);put(stack,stop,*args);c.reg_write(UC_X86_REG_ESP,stack)
        for r,v in regs:c.reg_write(r,v)
        c.emu_start(at,stop,count=500000)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_ESP)!=stack+4+len(args)*4:raise RuntimeError('unbalanced complete original function')
    print(f'# ps_game.exe SHA256 {digest}; complete PacketUserDBSetInfo 0046d780 admission and WHOLE SendDBAgentCharGetInfo 0046c980 (empty unrelated owner lists); actual CreateSkill shared allocation/ID logic. Initialized stack input exposes untouched sparse rows; clock/send mutations are explicit ports; only 0804 phase compared natively.')
    print('# case count pattern tick flags fill final_count final_selection final_account scratch_fnv packet events')
    for case in range(192):
        c.mem_write(base,bytes(0x70000));n=(0,1,4,16,255,256)[case%6];pattern=(case//6)%4;tick=(0,100,0xfffffff0,0xffffffff)[(case//24)%4];flags=(int(case%11==0)|2*int(case%13==0)|4*int(case%7==0));fill=(0xa5,0x3c)[case//96]
        state=dict(seed=True,allocated=0,tick=tick,flags=flags,fill=fill,wire='-',other=[],scratch=None);events=[];put(0x546f3c,manager);put(manager+16,500);put(user+0x578c,42)
        for i,offset in enumerate((0xa94,0x11f4)):
            head=base+0x18000+i*0x100;put(head,head,head);put(user+offset,head)
        for i,cap in enumerate((0,1,7,65535)):c.mem_write(definition+i*0x100+0x4a,struct.pack('<H',cap))
        records=[]
        for i in range(n):
            slot=(i,(i*3+7)%256,255,i%3)[pattern];records.append(struct.pack('<HBBH',11,1,slot,0))
        for start in range(0,n,255):
            rows=records[start:start+255];wire=struct.pack('<HIB',0x504,42,len(rows))+b''.join(rows);c.mem_write(packet,wire);call(0x46d780,(user,),((UC_X86_REG_ECX,packet),))
        put(user+0x1444,17)
        deltas=(0,999,1000,0xffffffff,0x7fffffff,0x80000000,0x800003e8,0xfffffc18)
        for i in range(256):
            p=get(user+0xac4+4*i)
            if p:put(p+12,0x12340000+i,0xff000203,0x1fe+i);put(p+28,(tick+deltas[(i+case)%8])&0xffffffff);put(p+48,definition+((i+case)%4)*0x100 if get(p+28)>tick or i%17 else 0)
        state['seed']=False;call(0x46c980,(),((UC_X86_REG_ECX,user),))
        scratch=state['scratch'];checksum=2166136261
        for b in scratch:checksum=((checksum^b)*16777619)&0xffffffff
        if state['other']!=[0x606,0x607]:raise RuntimeError('unexpected original surrounding phases')
        if scratch is None:raise RuntimeError('whole original function missed learned checkpoint')
        print(case,n,pattern,tick,flags,fill,get(user+0xac0),get(user+0x1444),get(user+0x578c),checksum,state['wire'],'|'.join(events),sep='\t')

if __name__=='__main__':main()
