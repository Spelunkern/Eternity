#!/usr/bin/env python3
"""Whole original zone loop plus real insertion/removal/ID tree and nested loops."""
import struct
from unicorn import UC_HOOK_CODE, UC_PROT_ALL
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x200000);cpu.mem_map(0,0x10000)
    zone,head,tree,heap,users,stack,stop,code=base,base+0x3000,base+0x4000,base+0x5000,base+0x20000,base+0x80000,base+0x8f000,base+0x100000
    ids=(90,10,70,20,60,30);ptrs=[users+i*0x8000 for i in range(6)];state={};events=[];checkpoints={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def identity(at):return ids[ptrs.index(at)] if at in ptrs else 0
    def emit_stub(operations,return_pc,return_esp):
        address=code+state['case']*0x400;data=bytearray(b'\x60') # PUSHAD
        def mov(op,value):data.extend(bytes((op,))+struct.pack('<I',value&0xffffffff))
        def call(target):
            at=address+len(data);data.extend(b'\xe8'+struct.pack('<i',target-(at+5)))
        def mark(value):checkpoints[address+len(data)]=value;data.extend(b'\x90')
        for op,i in operations:
            if op=='R':
                mov(0xb8,ptrs[i]);mov(0xbf,zone);call(0x415ec0);mark(('R',i))
            elif op=='A':
                mov(0xb8,ptrs[i]);mov(0xb9,zone);call(0x415e30);mark(('A',i))
            elif op=='Q':
                data.extend(b'\x68'+struct.pack('<I',ids[i]));mov(0xb8,zone);call(0x415f30);mark(('Q',i))
            elif op=='M':
                mov(0xb8,ptrs[i]);data.extend(b'\xc7\x80'+struct.pack('<II',0xe4,1));mark(('M',i))
            elif op=='N':
                mov(0xbe,zone);mov(0xbf,(state['tick']+7)&0xffffffff);call(0x421930)
        data.extend(b'\x61\x30\xc0\xc2\x04\x00') # POPAD, false poll, RET 4
        cpu.mem_write(address,bytes(data));cpu.reg_write(UC_X86_REG_EIP,address)
    def hook(c,at,size,data):
        if at==0x503a3f:
            length=get(c.reg_read(UC_X86_REG_ESP)+4);address=state['heap'];state['heap']+=(length+15)&~15;c.reg_write(UC_X86_REG_EAX,address)
        elif at in (stop+16,stop+32):
            lock=get(c.reg_read(UC_X86_REG_ESP)+4)
            if lock==zone+0x118:state['depth']+=1 if at==stop+16 else -1
        elif at in checkpoints:
            op,i=checkpoints[at]
            if op=='Q':events.append(f'Q,{ids[i]},{identity(c.reg_read(UC_X86_REG_EAX))}')
            else:
                events.append(f'{op},{ids[i]}')
                if op=='R':cpu.mem_protect(ptrs[i],0x6000,0) # no post-callback dereference allowed
        elif at==0x48ca50:
            sp=c.reg_read(UC_X86_REG_ESP);actor=c.reg_read(UC_X86_REG_ECX);i=ptrs.index(actor);tick=get(sp+4)
            events.append(f'P,{ids[i]},{tick},{state["depth"]}')
            if not state['mutated']:
                state['mutated']=True;p=state['pattern'];n=state['n']
                ops={0:[('Q',n-1)],1:[('R',i)],2:[('R',1)],3:[('A',n)],4:[('M',i)],5:[('N',0)],6:[('R',i),('A',n)],7:[('R',1),('Q',1),('A',n)]}[p]
                emit_stub(ops,get(sp),sp);return
            ret=get(sp);c.reg_write(UC_X86_REG_ESP,sp+8);c.reg_write(UC_X86_REG_EAX,0);c.reg_write(UC_X86_REG_EIP,ret)
    cpu.hook_add(UC_HOOK_CODE,hook)
    for at,cleanup in ((0x503a3f,0),(0x503ba1,0),(stop+16,4),(stop+32,4)):cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210cc,stop+16);put(0x5210d0,stop+32)
    def call(at,regs):
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack)
        for reg,value in regs:cpu.reg_write(reg,value)
        try:cpu.emu_start(at,stop,count=1000000)
        except Exception as error:raise RuntimeError(f"fault case={state['case']} entry={at:x} pc={cpu.reg_read(UC_X86_REG_EIP):x} eax={cpu.reg_read(UC_X86_REG_EAX):x} ecx={cpu.reg_read(UC_X86_REG_ECX):x} esp={cpu.reg_read(UC_X86_REG_ESP):x}") from error
        if cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+4:raise RuntimeError('zone operation did not return balanced')
    print(f'# ps_game.exe SHA256 {digest}; whole UpdateUserAttack 00421930, AddUser 00415e30, RemoveUser 00415ec0, FindUser 00415f30 with actual ID trees; poll/locks/allocator ports. Removed actor memory made inaccessible.')
    print('# case pattern initial_count tick events final_insertion_order')
    for case in range(256):
        for actor in ptrs:cpu.mem_protect(actor,0x6000,UC_PROT_ALL)
        cpu.mem_write(base,bytes(0x80000));state.update(case=case,heap=heap,depth=0,mutated=False,pattern=case%8,n=3+(case//8)%2,tick=(100,0xfffffff0,0xffffffff)[(case//16)%3]);events=[];checkpoints={}
        put(zone+0x110,head,head);put(head,head,head);put(zone+0x78,0,tree,0);put(tree,tree,tree,tree);cpu.mem_write(tree+0x14,b'\x01\x01')
        for i,actor in enumerate(ptrs):put(actor+0xdc,ids[i]);put(actor+0xf4,actor+0xd0)
        for i in range(state['n']):call(0x415e30,((UC_X86_REG_EAX,ptrs[i]),(UC_X86_REG_ECX,zone)))
        call(0x421930,((UC_X86_REG_ESI,zone),(UC_X86_REG_EDI,state['tick'])))
        if state['depth']!=0 or get(zone+0x114)!=head:raise RuntimeError('global lock/cursor not released at end')
        members=[];at=get(head+4)
        while at!=head:members.append(str(get(get(at+8)-0xd0+0xdc)));at=get(at+4)
        if len(members)!=get(zone+0x130):raise RuntimeError('original membership count mismatch')
        print(case,state['pattern'],state['n'],state['tick'],'|'.join(events),','.join(members),sep='\t')


if __name__=='__main__':main()
