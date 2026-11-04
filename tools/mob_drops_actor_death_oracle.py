#!/usr/bin/env python3
"""Complete original CDamage::Reset, observing only Windows critical sections."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_ESI,UC_X86_REG_EIP
from original_oracle import load_game
cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x20000)
owner=base;sentinel=base+0x100;stack=base+0x10000;stop=base+0x1f000;enter=stop+16;leave=stop+32;heap=0x3ed3518
put=lambda at,value:cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
get=lambda at:struct.unpack('<I',cpu.mem_read(at,4))[0]
for iat,stub in ((0x5210cc,enter),(0x5210d0,leave)):put(iat,stub);cpu.mem_write(stub,b'\xc2\x04\x00')
events=[]
def hook(machine,at,size,data):
    lock=get(machine.reg_read(UC_X86_REG_ESP)+4)
    if lock not in (owner+8,heap+12):raise RuntimeError('Unexpected critical section')
    events.append(('L' if at==enter else 'U')+('D' if lock==owner+8 else 'H'))
for stub in (enter,leave):cpu.hook_add(UC_HOOK_CODE,hook,begin=stub,end=stub)
print(f'# ps_game.exe {sha}; full CDamage::Reset004b8aa0; only critical-section imports observed. count before/after; released heap count/order; repeat count; lock trace')
for count in range(65):
    cpu.mem_write(base,bytes(0x8000));put(heap,0);put(heap+8,0)
    nodes=[base+0x200+i*32 for i in range(count)];ring=[sentinel]+nodes
    for i,node in enumerate(ring):put(node,ring[(i+1)%len(ring)]);put(node+4,ring[(i-1)%len(ring)])
    put(owner,sentinel);put(owner+0x20,count);events.clear()
    def run():
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ESI,owner);cpu.emu_start(0x4b8aa0,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Incomplete reset')
    run();after=get(owner+0x20);released=[];node=get(heap)
    while node:
        released.append(nodes.index(node));node=get(node+4)
        if len(released)>count:raise RuntimeError('Heap cycle')
    if get(sentinel)!=sentinel or get(sentinel+4)!=sentinel or released!=list(range(count)):raise RuntimeError('List/heap corruption')
    run()
    print(count,after,get(heap+8),','.join(map(str,released)) or '-',get(owner+0x20),','.join(events),sep='\t')
