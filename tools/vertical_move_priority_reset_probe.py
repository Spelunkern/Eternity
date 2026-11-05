#!/usr/bin/env python3
"""Check original priority reset preserves extracted send references."""
import struct
from unicorn.x86_const import UC_X86_REG_ECX,UC_X86_REG_EIP,UC_X86_REG_ESP
import user_priority_queue_probe as q

q.cpu.mem_write(q.user,bytes(0x1000));q.cpu.mem_write(q.nodes,bytes(0x10000));q.state['next']=0
q.put(q.buf+8,0);q.put(q.user+0x68,5);q.cpu.mem_write(q.frame,struct.pack('<H',2))
for _ in range(130):q.send(7,10)
assert q.extract()==q.batch and q.get(q.user+0x120)==2 and q.get(q.buf+8)==130
q.put(q.stack+0x8000,q.stop);q.cpu.reg_write(UC_X86_REG_ESP,q.stack+0x8000);q.cpu.reg_write(UC_X86_REG_ECX,q.user+0xfc)
q.cpu.emu_start(0x4dfe60,q.stop,count=100000)
assert q.cpu.reg_read(UC_X86_REG_EIP)==q.stop
assert q.get(q.user+0xfc)==0 and q.get(q.user+0x100)==0 and q.get(q.user+0x104)==0 and q.get(q.user+0x120)==0
assert q.get(q.buf+8)==128
print('ps_game.exe',q.digest,'SSyncQueueBufferPriority::Reset 004dfe60: 2 queued references released, 128 transferred references retained')
