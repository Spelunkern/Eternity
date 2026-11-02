#!/usr/bin/env python3
"""Execute original SDBDispatchClient queue/counter/completion control.

Ghidra work/db-dispatch and work/db-dispatch-start. FIFO heap allocation, atomics,
completion delivery and the virtual job body are explicit ports. Handler-state
injections model producer/reset interleavings after the first dispatched job.
"""
import itertools
import random
import struct
from collections import deque
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,client,dispatcher,stop=0x20000000,0x30000000,0x31000000,0x10000000
    for at in (stack,client,dispatcher,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    queue=deque();events=[];state={}
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x431710:
            job=word(sp+4);events.append('PUSH '+str(job))
            if not state['fail']:queue.append(job)
        elif address==0x431510:
            job=queue.popleft() if queue else 0;events.append('POP '+str(job));machine.reg_write(UC_X86_REG_EAX,job)
        elif address==0x4314a0:events.append('CLEAR');queue.clear()
        elif address==stop+0x100:
            at=word(sp+4);value=(word(at)+1)&0xffffffff;put(at,value);events.append('INC '+str(value));machine.reg_write(UC_X86_REG_EAX,value)
        elif address==stop+0x120:
            at,delta=word(sp+4),word(sp+8);old=word(at);put(at,old+delta);events.append('SUB '+str((-delta)&0xffffffff)+' '+str(old));machine.reg_write(UC_X86_REG_EAX,old)
        elif address==stop+0x140:
            at,value=word(sp+4),word(sp+8);old=word(at);put(at,value);events.append('EXCHANGE '+str(value));machine.reg_write(UC_X86_REG_EAX,old)
        elif address in (stop+0x160,0x431a00):events.append('WAKE')
        elif address==stop+0x180:
            job=word(sp+4);events.append('JOB '+str(job))
            if not state['injected'] and state['inject']:
                mode=state['inject'];state['injected']=True;events.append('INJECT '+str(mode))
                if mode==1:put(client+0x2c,0)
                elif mode==2:put(client+0x2c,0xffffffff)
                elif mode==3:queue.append(8);put(client+0x2c,word(client+0x2c)+1)
                elif mode==4:queue.clear();put(client+0x2c,0)
                elif mode==5:queue.append(0);put(client+0x2c,word(client+0x2c)+1)
    for at,cleanup in ((0x431710,4),(0x431510,0),(0x4314a0,0),(stop+0x100,4),(stop+0x120,8),(stop+0x140,8),(stop+0x160,16),(stop+0x180,4),(0x431a00,4)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    for iat,target in ((0x4600e4,stop+0x100),(0x460110,stop+0x120),(0x4600d0,stop+0x140),(0x460108,stop+0x160)):put(iat,target)
    put(client,stop+0x200);put(stop+0x200,stop+0x180)
    print(f'# ps_dbAgent.exe SHA256 {digest}; PostDispatchDB 00431a60, DispatchDB 00431ac0, Reset 00403c30; FIFO/heap, atomics, completion and virtual handler are ports')
    rng=random.Random(0x431ac0)
    jobs=((),(0,),(8,),(8,9,10),(8,0,22),(1,1,1,1))
    counts=(0,1,2,4,0x7fffffff,0x80000000,0xfffffffe,0xffffffff)
    for attached,sync,count,initial,fail,inject in itertools.product((False,True),(False,True),counts,jobs,(False,True),range(6)):
        queue.clear();queue.extend(initial);put(client+0x28,dispatcher if attached else 0);put(client+0x2c,count);cpu.mem_write(dispatcher+0x414,bytes([sync]));state.update(fail=fail,inject=inject)
        print('CASE',int(attached),int(sync),count,int(fail),inject,len(initial),*initial)
        # Direct dispatch probes malformed/stale counters before a fresh enqueue.
        steps=[('D',0),('P',rng.choice((0,8,9,22,0xffffffff))),('P',8),('D',0),('D',0),('R',0),('P',9),('D',0),('D',0)]
        for kind,job in steps:
            events.clear();state['injected']=False;put(stack+0x8000,stop,job);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.reg_write(UC_X86_REG_ECX,client);cpu.reg_write(UC_X86_REG_EAX,client)
            cpu.emu_start({'P':0x431a60,'D':0x431ac0,'R':0x403c30}[kind],stop,count=10000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original DB dispatch did not return')
            print('STEP',kind,job)
            for event in events:print('EVENT',event)
            print('STATE',word(client+0x2c),len(queue),*queue)
        print('END')
    # Two stale completion wakes can overlap while a producer is between Push
    # and InterlockedIncrement. Cooperative instruction checkpoints preserve
    # each thread's registers/stack and share the actual original memory.
    queue.clear();queue.append(8);put(client+0x28,dispatcher);put(client+0x2c,1);cpu.mem_write(dispatcher+0x414,b'\x01');state.update(fail=False,inject=0,injected=True);events.clear()
    def checkpoint(machine,address,size,context):
        if state.get('checkpoint')==address:machine.emu_stop()
    for at in (0x431a80,0x431af7):cpu.hook_add(UC_HOOK_CODE,checkpoint,begin=at,end=at)
    def start(address,sp,end,job=0):
        put(sp,stop,job);cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_ECX,client)
        state['checkpoint']=end;cpu.emu_start(address,stop,count=10000);state['checkpoint']=None
        if cpu.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError(f'Original dispatch checkpoint did not finish: {cpu.reg_read(UC_X86_REG_EIP):08x} expected {end:08x}, effects={events}')
        return cpu.context_save()
    producer=start(0x431a60,stack+0x6000,0x431a80,9)
    first=start(0x431ac0,stack+0x8000,0x431af7)
    start(0x431ac0,stack+0xa000,stop)
    for context in (first,producer):
        cpu.context_restore(context);cpu.emu_start(cpu.reg_read(UC_X86_REG_EIP),stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original interleaved dispatcher did not return')
    start(0x431ac0,stack+0xa000,stop)
    print('RACE')
    for event in events:print('EVENT',event)
    print('STATE',word(client+0x2c),len(queue),*queue)
    print('END')


if __name__=='__main__':main()
