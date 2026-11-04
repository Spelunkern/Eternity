#!/usr/bin/env python3
"""Original SendRecoverChange and SendEnergyAllToParty with live callback probes."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x20000)
    user,zone,party=base,base+0x6000,base+0x7000;stack,child_stack,stop=base+0x10000,base+0x14000,base+0x1f000;state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def resources(values):
        for at,value in zip((0x1238,0x123c,0x1240),values):put(user+at,value)
    def group(kind):put(user+0x173c,party if kind>=0 else 0);cpu.mem_write(party+0x114,bytes([max(0,kind)]))
    def invoke(args,sp):
        put(sp,stop)
        # Original stack order HP,SP,MP; public native API uses PlayerResources HP,MP,SP.
        for i,value in enumerate((args[0],args[2],args[1])):put(sp+4+i*4,value)
        cpu.reg_write(UC_X86_REG_ESP,sp);cpu.reg_write(UC_X86_REG_ESI,user);cpu.emu_start(0x482230,stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Incomplete original SendRecoverChange')
    def hook(machine,at,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if at==0x421220:
            args=struct.unpack('<8I',machine.mem_read(sp+4,32));actual_zone,pointer,length,position,radius,identity,zero,channel=args
            if (actual_zone,length,position,radius,identity,zero,channel)!=(zone,12,user+0xd0,0x42700000,state['identity'],0,5):raise RuntimeError('Unexpected original view route')
            state['events'].append('V'+bytes(machine.mem_read(pointer,length)).hex())
            if state['depth']==0:
                if state['mutate']:
                    resources(state['changed']);group(state['after']);state['events'].append('X')
                if state['nested']:
                    state['events'].append('N');saved=machine.context_save();state['depth']=1
                    invoke(state['child'],child_stack);machine.context_restore(saved);state['depth']=0;state['events'].append('n')
        else:
            if machine.reg_read(UC_X86_REG_EAX)!=12 or machine.reg_read(UC_X86_REG_EBX)!=party or get(sp+4)!=user:raise RuntimeError('Unexpected original party sender/exclusion')
            pointer=machine.reg_read(UC_X86_REG_ECX);state['events'].append('P'+bytes(machine.mem_read(pointer,12)).hex())
            if state['depth']==0 and state['party_mutate']:
                resources(state['last']);group(-1);state['events'].append('Y')
    for at,cleanup in ((0x421220,32),(0x445680,4)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    print(f'# ps_game.exe SHA256 {sha}; complete SendRecoverChange00482230 + SendEnergyAllToParty00482360, actual packet/control bodies; only view/party dispatch are observed ports. V=radius60/channel5; P=existing group sender excluding affected user. X/Y mutate live owner at send boundary; N/n execute actual nested original routine. No full root world/provider equivalence claim.')
    values=(0,1,65535,65536,0x7fffffff,0x80000000,0xffffffff,0x12345678)
    for case in range(768):
        cpu.mem_write(base,bytes(0x8000));identity=(0,9001,0xffffffff)[case%3]
        initial=tuple(values[(case//(8**i))%8] for i in range(3));view=tuple(values[(case//(8**i)+3+i)%8] for i in range(3));changed=tuple((x^0xa5a55a5a)&0xffffffff for x in initial);last=tuple((x+7919)&0xffffffff for x in initial)
        child=tuple((x+0x12345)&0xffffffff for x in view);have_zone=case%4!=0;before=(-1,0,1)[(case//4)%3];after=(-1,0,1)[(case//12)%3]
        put(user+0xdc,identity);put(user+0xe0,zone if have_zone else 0);resources(initial);group(before)
        state.update(identity=identity,changed=changed,last=last,after=after,mutate=int(case%2==1),party_mutate=int(case%5==0),nested=int(case%17==0),child=child,depth=0,events=[])
        invoke(view,stack);final=tuple(get(user+at) for at in (0x1238,0x123c,0x1240));kind=-1 if get(user+0x173c)==0 else cpu.mem_read(party+0x114,1)[0]
        print(case,identity,int(have_zone),before,after,state['mutate'],state['party_mutate'],state['nested'],*initial,*view,*changed,*last,*child,*final,kind,','.join(state['events']) or '-',sep='\t')
if __name__=='__main__':main()
