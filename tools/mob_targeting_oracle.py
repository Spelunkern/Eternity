#!/usr/bin/env python3
"""Original SetTarget/SetTargetEx instructions; required help/FSM boundaries observed."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_ESI
from original_oracle import load_game


def main():
    cpu,digest=load_game()
    base=0x20000000; stack=base; mob=base+0x10000; user=base+0x20000; info=base+0x30000; skill=base+0x40000; stop=base+0x50000
    cpu.mem_map(base,0x60000)
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    state={}
    for at,n in ((0x504697,0),(stop+16,0),(0x49ff70,8),(0x4a0470,8),(0x4a0200,8),(0x4925f0,4)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',n))
    put(0x5210e4,stop+16)
    def hook(c,at,size,_):
        sp=c.reg_read(UC_X86_REG_ESP)
        if at==0x504697:
            state['seed']=(state['seed']*214013+2531011)&0xffffffff
            v=(state['seed']>>16)&32767;state['events']+=['R',str(v)];c.reg_write(UC_X86_REG_EAX,v)
        elif at==stop+16:
            v=(state['tick']+state['ticks']*11)&0xffffffff;state['ticks']+=1
            state['events']+=['T',str(v)];c.reg_write(UC_X86_REG_EAX,v)
        elif at==0x4925f0:
            if c.reg_read(UC_X86_REG_ESI)!=mob+4:raise RuntimeError('Wrong FSM owner')
            state['events']+=['D',str(get(sp+4)),str(c.reg_read(UC_X86_REG_ECX))]
        elif at in (0x49ff70,0x4a0470,0x4a0200):
            if get(sp+8)!=user:raise RuntimeError('Wrong help user')
            if at==0x49ff70:
                if get(sp+4)!=0 or c.reg_read(UC_X86_REG_ECX)!=mob:raise RuntimeError('Wrong normal help owner/mode')
            elif get(sp+4)!=mob:raise RuntimeError('Wrong help owner')
            state['events']+=['H',str({0x49ff70:2,0x4a0470:3,0x4a0200:5}[at]),str(get(mob+0x124)),str(get(mob+0x12c)),str(get(mob+0x280))]
            if state['mutate']:
                put(mob+0x12c,987654321);cpu.mem_write(info+0x96,bytes([1]));put(info+0xa4,skill+0x100);cpu.mem_write(skill+0x14a,struct.pack('<H',65))
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; CMob::SetTarget 0049fad0; SetTargetEx 004b65e0; real GetStatus 0041d760. Help/dispatch required external ports.')
    print('# op fsm status move life fixed initialize ai attackmask skillmask cooldown0 cooldown1 cooldown2 mutate seed tick raw_status | status targettype targetid timeout nextmove times0..2 | events')
    for case in range(4096):
        op=case%2;fsm=(case//2)%2;status=(0,1,2,4,100,-1,5,0)[(case//4)%8];move=(case//32)%3;life=(case//96)%3;fixed=(case//288)%2;init=(case//576)%2;ai=(0,2,3,5,9)[(case//7)%5];mask=(case//3)%8;skills=(case//11)%8;cool=[(0,1,65,65535)[(case//(13+i*4))%4] for i in range(3)];mutate=(case//23)%2;seed=case*979+1;tick=(0xfffffff0+case*19)&0xffffffff
        raw_status=(case*17+42)%101 if fsm else status
        cpu.mem_write(mob,bytes(0x1000));cpu.mem_write(info,bytes(0x200));cpu.mem_write(skill,bytes(0x400))
        put(mob+0x44,fsm);put(mob+0x124,raw_status,7,12345);put(mob+0x280,8765);put(mob+0x2a0,move);put(mob+0x2b8,fixed);put(mob+0xcbc,777);put(mob+0x1b8,111,222,333);put(mob+0xa8,info);put(user+0xdc,0xf1234567);put(user+0x142c,life)
        # Real GetStatus scene deque containing one scene.
        put(mob+0x34,info+0x100);put(mob+0x38,0,0,1);put(info+0x100,info+0x110);put(info+0x110,info+0x120);put(info+0x148,status)
        cpu.mem_write(info+0x26,bytes([ai]))
        for i in range(3):
            cpu.mem_write(info+0x82+i*20,bytes([(mask>>i)&1]));put(info+0x90+i*20,skill+i*0x100 if skills&(1<<i) else 0);cpu.mem_write(skill+i*0x100+0x4a,struct.pack('<H',cool[i]))
        state.update(events=[],seed=seed,tick=tick,ticks=0,mutate=mutate)
        put(stack+0xf000,stop,user,init);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.reg_write(UC_X86_REG_ECX,mob)
        cpu.emu_start(0x4b65e0 if op else 0x49fad0,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original did not return')
        inputs=[op,fsm,status,move,life,fixed,init,ai,mask,skills,*cool,mutate,seed,tick,raw_status]
        out=[get(mob+x) for x in (0x124,0x128,0x12c,0x280,0xcbc,0x1b8,0x1bc,0x1c0)]
        print(' '.join(map(str,inputs))+' | '+' '.join(map(str,out))+' | '+' '.join(state['events']))
if __name__=='__main__':main()
