#!/usr/bin/env python3
"""Complete PacketPC selection followed by complete UpdateAttack binding probes.

No native active CSkill owner exists yet. This fixture establishes its required
live access contract without replacing it with IDs, passive definitions or a
second skill store. All mutable skill/weapon objects here are original PE input.
"""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX, UC_X86_REG_ESP, UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x40000);cpu.mem_map(0,0x10000)
    user,mob,packet,skill,definition,stack,stop=base,base+0x8000,base+0xa000,base+0x10000,base+0x12000,base+0x30000,base+0x3f000
    events=[];state={}
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[x&0xffffffff for x in values]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def event(tag):events.append(tag)
    def snapshot():return ','.join(str(get(user+x)) for x in (0x142c,0x1274,0x143c,0x1440,0x1450,0x1444))+','+str(cpu.mem_read(user+0x1448,1)[0])
    def hook(c,at,size,data):
        result=None;pop=0
        if at==0x48a460:result=255 if state['profile']==4 else 4;event('speed')
        elif at in (0x44e140,0x44e6a0):
            event('target');result=int(state['profile']!=14);pop=8 if at==0x44e140 else 4
            if state['profile']==17:cpu.mem_write(packet+2,b'\xff')
            if state['profile']==18:put(user+0xac4,0)
        elif at==0x484b10:event('remove');pop=4
        elif at==0x4826f0:event('start');pop=0
        elif at==0x48a010:event('cancel');return # execute the real CancelMove
        elif at==0x489560:
            event('transform');pop=4
            if state['profile']==19:put(user+0x1444,17)
            if state['profile'] in (20,24):put(user+0x1d8,0)
        elif at==0x44cd80:event('mob');result=mob
        elif at==0x44c030:result=1;pop=8;event('range')
        elif at==0x44c610:result=0
        elif at in (0x44f2d0,0x450cf0):
            selected=c.reg_read(UC_X86_REG_ECX if at==0x450cf0 else UC_X86_REG_EAX);index=(selected-skill)//0x100
            if selected not in (skill,skill+0x100):raise RuntimeError('unexpected runtime pointer')
            event(('cast' if at==0x450cf0 else 'use')+str(index));pop=4
        elif at==0x44ce10:event('normal');pop=4
        elif at==0x4d4d20:
            sp=c.reg_read(UC_X86_REG_ESP);event('send'+bytes(c.mem_read(get(sp+4),get(sp+8))).hex());pop=8
        else:return
        sp=c.reg_read(UC_X86_REG_ESP);ret=get(sp);c.reg_write(UC_X86_REG_ESP,sp+4+pop)
        if result is not None:c.reg_write(UC_X86_REG_EAX,result)
        c.reg_write(UC_X86_REG_EIP,ret)
    cpu.hook_add(UC_HOOK_CODE,hook)
    def call(address,argument):
        put(stack,stop,argument);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,user)
        try:cpu.emu_start(address,stop,count=100000)
        except Exception as error:raise RuntimeError(f'profile {state["profile"]}, PC {cpu.reg_read(UC_X86_REG_EIP):x}, events={events}') from error
        if cpu.reg_read(UC_X86_REG_EIP)!=stop or cpu.reg_read(UC_X86_REG_ESP)!=stack+8:raise RuntimeError('unbalanced complete function')
    print(f'# ps_game.exe SHA256 {digest}; complete PacketPC 0046a230 opcodes 0511/0517 then complete UpdateAttack 0048ca50. Target/action/effect/send boundaries ported; actual selection/stop/continuation/cancel/cookie execute.')
    print('# opcode profile initial_mode request_slot count selected_after_request final_selected final_mode phase request_events poll_input poll_output poll_events')
    rows=0
    for opcode in (0x511,0x517):
        for profile in range(28):
            for mode in (0,2):
                cpu.mem_write(base,bytes(0x20000));events=[];state['profile']=profile
                request=255 if profile in (15,16) else (1 if profile==21 else 0)
                count=-1 if profile==8 else (0 if profile==7 else 2)
                put(user+0xdc,42);put(user+0x142c,1 if profile==1 else 3 if profile==2 else mode)
                put(user+0xac0,count);put(user+0xac4,0 if profile==9 else skill,skill+0x100)
                put(user+0xac4+255*4,skill+0x100);put(user+0xac4+17*4,skill+0x100)
                put(user+0x1444,33);put(user+0x1d8,base+0x18000);put(user+0x12ec,4)
                put(user+0x1528,2,77);put(user+0x143c,0,0)
                if profile==3:cpu.mem_write(user+0x1354,b'\x02')
                if profile==5:cpu.mem_write(user+0x1434,b'\x01')
                if profile==6:put(user+0x1460,1)
                cpu.mem_write(user+0x136,struct.pack('<H',10));cpu.mem_write(user+0x134,b'\x02');cpu.mem_write(user+0x12f,b'\x02')
                for i in range(2):
                    put(skill+i*0x100+0x30,definition+i*0x100)
                    cpu.mem_write(definition+i*0x100+3,b'Runtime\0')
                    cpu.mem_write(definition+i*0x100+0x2f,bytes((0 if profile==10 else 1,)))
                    cpu.mem_write(definition+i*0x100+0x24,struct.pack('<H',11 if profile==11 else 10))
                    cpu.mem_write(definition+i*0x100+0x29,bytes((0 if profile==12 else 1,)))
                    cpu.mem_write(definition+i*0x100+0x2d,bytes((3 if profile==13 else 2,)))
                put(packet,opcode | request<<16);put(packet+3,0 if profile==16 else 77)
                call(0x46a230,packet);selected=get(user+0x1444)
                accepted=profile in (0,17,18,19,20,21,22,23,24,25,26,27)
                expected=255 if profile==17 else request if accepted else 33
                if selected!=expected:raise RuntimeError(f'wrong selection profile {profile}: {selected}!={expected}, {events}')
                request_events='|'.join(events) or '-';pre=post=poll_events='-'
                if accepted:
                    if get(user+0x1440)!=2:raise RuntimeError('phase was not selected skill')
                    # Force normal/casting poll independently of request publication.
                    # Poll must use the live slot even beyond the learned count.
                    if profile in (22,26,27):put(user+0x142c,3)
                    if profile==26:put(user+0xac4,0)
                    if profile in (25,27):put(user+0x1d8,0)
                    if profile in (23,24,25):put(user+0x1440,1)
                    pre=snapshot();events=[]
                    call(0x48ca50,100)
                    if profile in (17,19,21) and 'use1' not in events:raise RuntimeError('live/forced slot was not dispatched')
                    if profile in (18,26) and not any(x.startswith('send02050a') for x in events):raise RuntimeError('missing slot did not report reason 10')
                    if profile in (24,25) and not any(x.startswith('send020507') for x in events):raise RuntimeError('missing weapon did not report reason 7')
                    post=snapshot();poll_events='|'.join(events) or '-'
                print(hex(opcode),profile,mode,request,count,selected,get(user+0x1444),get(user+0x142c),get(user+0x1440),request_events,pre,post,poll_events,sep='\t');rows+=1
    if rows!=112:raise RuntimeError('incomplete binding matrix')


if __name__=='__main__':main()
