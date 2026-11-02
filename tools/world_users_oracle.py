#!/usr/bin/env python3
"""Execute original world ID/name trees and lookup filters.

Ghidra work/world-live-index, work/world-user-lifecycle and work/world-index-trees.
Original insert/find/erase, string comparison and balancing run. Allocator, locks
and CUser::EnterWorld/LeaveWorld bodies are ports, not reconstructed by this oracle.
"""
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();stack,world,users,heap,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
    for at in (0,stack,world,data,stop):cpu.mem_map(at,0x10000)
    cpu.mem_map(users,0x30000);cpu.mem_map(heap,0x100000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    pointers=[users+i*0x10000 for i in range(3)];events=[];state={}
    def hook(machine,address,size,context):
        if address==0x503a3f:
            size=word(machine.reg_read(UC_X86_REG_ESP)+4);at=state['allocation'];state['allocation']+=(size+15)&~15;machine.reg_write(UC_X86_REG_EAX,at)
        elif address in (0x44a820,0x44aad0):
            # Both callbacks receive the actual user in ECX.
            from unicorn.x86_const import UC_X86_REG_ECX
            events.append(('ENTER ' if address==0x44a820 else 'LEAVE ')+str(pointers.index(machine.reg_read(UC_X86_REG_ECX)))+' '+str(word(world+0x30))+' '+str(word(world+0x5c)))
    for at,cleanup in ((0x503a3f,0),(0x503ba1,0),(stop+0x100,4),(0x44a820,0),(0x44aad0,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    for iat in (0x5210cc,0x5210d0):put(iat,stop+0x100)
    def call(address,args=(),regs=()):
        put(stack+0xf000,stop,*args);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000)
        for reg,value in regs:cpu.reg_write(reg,value)
        try:cpu.emu_start(address,stop,count=1000000)
        except Exception:
            raise RuntimeError(f"World index fault at {cpu.reg_read(UC_X86_REG_EIP):08x}, stack {bytes(cpu.mem_read(cpu.reg_read(UC_X86_REG_ESP),32)).hex()}")
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError(f'Original world index did not return {address:08x}')
        result=cpu.reg_read(UC_X86_REG_EAX);return pointers.index(result)+1 if result in pointers else 0
    rng=random.Random(0x40d450)
    print(f'# ps_game.exe SHA256 {digest}; EnterUserWorld 0040d450, LeaveUserWorld 0040d4d0, FindUser/Zone/ZoneMove 0040d640..0040d840; original trees execute; allocator/locks/user lifecycle ports')
    for scenario in range(192):
        cpu.mem_write(world,bytes(0x10000));cpu.mem_write(users,bytes(0x30000));cpu.mem_write(heap,bytes(0x100000));state['allocation']=heap+0x1000
        for offset,head,size in ((0x28,heap,0x15),(0x54,heap+0x100,0x31)):
            put(world+offset,0,head,0);put(head,head,head,head);cpu.mem_write(head+size-1,b'\x01\x01')
        ids=[0x80000000+scenario if scenario%4 else 0,0xffffffff,0x80000000+scenario if scenario%3 else 0xffffffff]
        names=[b'Hero',b'hero' if scenario%2 else b'Hero',rng.choice((b'Other',b'Hero',b'\x80X',b'\xffX',b'',b'A'*31,b'Hero\0tail'))]
        for i,at in enumerate(pointers):put(at+0x128,ids[i]);cpu.mem_write(at+0x188,names[i]+b'\0')
        print('CASE',*ids,*[name.hex() if name else '-' for name in names])
        operations=[('ENTER',i,0,0) for i in (0,1,2,0)]+[('STATE',i,4,0) for i in range(3)]
        for _ in range(17):operations.append((rng.choice(('ENTER','LEAVE','STATE')),rng.randrange(3),rng.choice((0,1,2,3,4,5,0xffffffff)),rng.choice((0,1,0xffffffff))))
        for op,i,location,logout in operations:
            events.clear();print('STEP',op,i,location,logout)
            if op=='ENTER':call(0x40d450,(world,),((UC_X86_REG_EDI,pointers[i]),))
            elif op=='LEAVE':call(0x40d4d0,regs=((UC_X86_REG_EBX,world),(UC_X86_REG_EDI,pointers[i])))
            else:put(pointers[i]+0x5758,location);put(pointers[i]+0x57dc,logout)
            for event in events:print('EVENT',event)
            results=[]
            for address in (0x40d640,0x40d700,0x40d7e0):
                for key in ids+[123]:results.append(call(address,(key,),((UC_X86_REG_EAX,world),)))
            for address in (0x40d690,0x40d760,0x40d840):
                for key in names+[b'Missing']:
                    cpu.mem_write(data,key+b'\0');results.append(call(address,(world,),((UC_X86_REG_EAX,data),)))
            print('RESULT',word(world+0x30),word(world+0x5c),*results)
        print('END')


if __name__=='__main__':main()
