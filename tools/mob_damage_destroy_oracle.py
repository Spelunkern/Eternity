#!/usr/bin/env python3
"""Execute actual damage owner destruction versus reset and subsequent reuse."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x10000);stop=base+0xf000;pool=0x3ed353c;state={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    for at,n in ((0x503a3f,0),(0x503ba1,0),(stop+16,4)):cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
    for iat in (0x5210cc,0x5210d0,0x5210d8):put(iat,stop+16)
    def hook(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x503a3f:
            at=base+0x1000+state['allocated']*32;state['allocated']+=1;machine.mem_write(at,bytes(28));machine.reg_write(UC_X86_REG_EAX,at)
        elif address==0x503ba1:state['deleted'].append((get(sp+4)-base-0x1000)//32)
    cpu.hook_add(UC_HOOK_CODE,hook)
    def call(address,owner,args=(),kind=1):
        put(base+0xe000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,base+0xe000);cpu.reg_write(UC_X86_REG_ESI,owner);cpu.reg_write(UC_X86_REG_EAX,owner);cpu.reg_write(UC_X86_REG_EBX,kind);cpu.emu_start(address,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Damage lifecycle did not return')
    print(f'# ps_game.exe SHA256 {digest}; Add 004b8dd0, Reset 004b8d10, destructor 004b8cf0 and real list Release 004b9480. Allocator/delete and locks ports; actual loop beyond Ghidra noreturn annotation executes.')
    print('# initial_records reset_first allocated_attempts free_count deleted_count next_owner_record_id')
    for n in (0,1,3,16):
        for reset in (0,1):
            cpu.mem_write(base,bytes(0xd000));put(pool,0,0,0);state.update(allocated=0,deleted=[])
            for i in range(2):
                owner=base+i*256;head=base+0x200+i*64;put(owner+4,head,head);put(head,head,head)
            for i in range(n):call(0x4b8dd0,base,(i,1,1,0))
            if reset:call(0x4b8d10,base)
            call(0x4b8cf0,base);call(0x4b8dd0,base+256,(999,1,1,0))
            node=get(base+0x240+4);identity=(node-base-0x1000)//32
            print(n,reset,get(pool+4),get(pool+8),len(state['deleted']),identity,sep='\t')
if __name__=='__main__':main()
