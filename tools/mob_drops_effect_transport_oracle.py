#!/usr/bin/env python3
"""Execute original SendSkillRemove including DB/client delivery and ignored send result."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x10000);stack,stop=base+0x8000,base+0xf000;state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def hook(machine,at,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP);ptr,length=struct.unpack('<II',machine.mem_read(sp+4,8))
        state['events'].append(('B' if at==0x4d4f10 else 'S')+bytes(machine.mem_read(ptr,length)).hex());machine.reg_write(UC_X86_REG_EAX,state['accepted'])
    for at in (0x4d4f10,0x4d4d20):cpu.mem_write(at,b'\xc2\x08\x00');cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    print(f'# ps_game.exe SHA256 {sha}; complete CUser::SendSkillRemove 00482a70; actual 0902/050e construction and ordering. DB/client sends are observed ports with alternating return values, ignored by original.')
    for case in range(512):
        account=(0,1,0x12345678,0xffffffff)[case%4];skill=(0,1,127,321,65535)[case%5];level=(0,1,100,255)[(case//5)%4];instance=(case*7919)&0xffffffff
        put(base+0x578c,account);put(stack,stop);put(stack+4,instance);state.update(events=[],accepted=case%2)
        cpu.reg_write(UC_X86_REG_ESI,base);cpu.reg_write(UC_X86_REG_ECX,skill);cpu.reg_write(UC_X86_REG_EDX,level);cpu.reg_write(UC_X86_REG_ESP,stack)
        cpu.emu_start(0x482a70,stop,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Incomplete original send')
        print(case,account,skill,level,instance,state['accepted'],','.join(state['events']),sep='\t')
if __name__=='__main__':main()
