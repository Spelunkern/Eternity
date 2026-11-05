#!/usr/bin/env python3
"""Execute the game receive worker's malformed-length close branch."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game

cpu,digest=load_game()
stack,worker,connection,agent,context,holder,frame,dispatcher,stop=(0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x34000000,0x35000000,0x36000000,0x10000000)
for address in (stack,worker,connection,agent,context,holder,frame,dispatcher,stop): cpu.mem_map(address,0x10000)
def put(address,*values): cpu.mem_write(address,struct.pack('<'+'I'*len(values),*values))
def get(address): return struct.unpack('<I',cpu.mem_read(address,4))[0]
# The receive worker executes unchanged; close, disconnect, dispatch and context
# freeing are ports. Capture arguments to the original close call.
for address,code in ((0x4d43a0,b'\xc2\x08\x00'),(0x4d43e0,b'\xc3'),(0x4dd300,b'\xc2\x04\x00'),(0x4dbcf0,b'\xc2\x04\x00')):
    cpu.mem_write(address,code)
events=[]
def hook(machine,address,size,context_value):
    if address==0x4d43a0:
        sp=machine.reg_read(UC_X86_REG_ESP);events.append(('close',get(sp+4),get(sp+8)))
    elif address==0x4d43e0: events.append(('disconnect',))
    elif address==0x4dd300: events.append(('dispatch',))
    else: events.append(('free',))
for address in (0x4d43a0,0x4d43e0,0x4dd300,0x4dbcf0):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
print(f'# ps_game.exe SHA256 {digest}; SWorker::IORecv 004dc690; close and cleanup calls are ports')
for length in (0,1,2049,65535):
    for dispatched in (0,1):
        cpu.mem_write(context,bytes(0x100));cpu.mem_write(agent,bytes(0x200));put(connection+0xc8,agent);put(context+0x28,holder);put(holder+4,frame)
        if dispatched:put(agent+0x120,dispatcher)
        cpu.mem_write(frame,struct.pack('<H',length));put(stack+0x8000,stop,connection,2,context)
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,worker);events.clear();cpu.emu_start(0x4dc690,stop,count=10000)
        expected=[('close',13,0),(('dispatch',) if dispatched else ('disconnect',)),('free',)]
        if cpu.reg_read(UC_X86_REG_EIP)!=stop or events!=expected:raise RuntimeError(f'Unexpected original receive result: {events}')
        print(length,dispatched,13,events[1][0])
