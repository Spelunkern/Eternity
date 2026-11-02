#!/usr/bin/env python3
"""Execute sustained two-owner damage accumulation and shared heap recycling."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x20000);stop=base+0x1f000;pool=0x3ed353c;state={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def ident(pointer):return 255 if not pointer else state['objects'].index(pointer)
    cpu.mem_write(stop+16,b'\xc2\x04\x00');put(0x5210cc,stop+16);put(0x5210d0,stop+16);cpu.mem_write(0x503a3f,b'\xc3')
    def hook(machine,address,size,data):
        if address==0x503a3f:
            if state['fail']:machine.reg_write(UC_X86_REG_EAX,0);return
            at=base+0x1000+len(state['objects'])*32;state['objects'].append(at);machine.mem_write(at,bytes(28));machine.reg_write(UC_X86_REG_EAX,at)
    cpu.hook_add(UC_HOOK_CODE,hook)
    def snapshot():
        out=struct.pack('<2I',get(pool+4),get(pool+8));free=[];at=get(pool)
        while at:
            free.append(ident(at));at=get(at+4)
        out+=bytes([len(free),*free])
        for i in range(2):
            owner=base+i*256;head=base+0x200+i*64;at=get(head+4);nodes=[]
            while at!=head:nodes.append(ident(at));at=get(at+4)
            out+=bytes(cpu.mem_read(owner,1))+struct.pack('<I',get(owner+0x24))+bytes([len(nodes),*nodes])
        out+=bytes([len(state['objects'])])
        for at in state['objects']:out+=bytes(cpu.mem_read(at+8,20))
        return out.hex()
    print(f'# ps_game.exe SHA256 {digest}; CDamageMob Add 004b8dd0, Reset 004b8d10, GetMaxAggro 004b9000; real shared heap/list operations; allocation/failure and locks ports. Explicit empty list heads.')
    print('# S scenario; A owner type id damage aggro party fail expected; R owner expected; Q owner bool/output28bytes. Mutation expected: allocation attempts/free count/order, two owners fair/count/list, every record retained payload.')
    for scenario in range(64):
        cpu.mem_write(base,bytes(0x1e000));put(pool,0,0,0);state.update(objects=[],fail=False);print('S',scenario,sep='\t')
        for i in range(2):
            owner=base+i*256;head=base+0x200+i*64;put(owner+4,head,head);put(head,head,head)
        large=(0x7fffffff,0x80000000,0xffffffff,100)[scenario%4]
        ops=[('A',0,1,1,large,large,2,0),('A',0,1,1,100,100,3,1),('A',0,5,2,large,100,2,0),('A',0,5,2,1,100,3,0),('A',1,2,3,1,2,0,0),('A',1,1,4,1,1,0,1),('R',0),('A',1,1,4,large,large,2,1),('A',0,1,5,1,1,0,1),('R',1),('R',1),('A',0,5,6,1,100,6,1),('R',0)]
        for op in ops:
            state['fail']=op[-1] if op[0]=='A' else False;owner=base+op[1]*256;args=op[3:7] if op[0]=='A' else ();put(base+0x1e000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,base+0x1e000);cpu.reg_write(UC_X86_REG_ESI,owner)
            if op[0]=='A':cpu.reg_write(UC_X86_REG_EBX,op[2])
            cpu.emu_start(0x4b8dd0 if op[0]=='A' else 0x4b8d10,stop,count=100000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Damage mutation did not return')
            print(*op,snapshot(),sep='\t')
            for query_owner in range(2):
                output=base+0x1d000;cpu.mem_write(output,b'\xa5'*28);put(base+0x1e000,stop);cpu.reg_write(UC_X86_REG_ESP,base+0x1e000);cpu.reg_write(UC_X86_REG_ESI,base+query_owner*256);cpu.reg_write(UC_X86_REG_EBX,output)
                cpu.emu_start(0x4b9000,stop,count=100000)
                if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Live aggro selection did not return')
                result=bytes([cpu.reg_read(UC_X86_REG_EAX)&255])+bytes(cpu.mem_read(output,28));print('Q',query_owner,result.hex(),sep='\t')
if __name__=='__main__':main()
