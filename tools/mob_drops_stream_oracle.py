#!/usr/bin/env python3
"""Execute original ODBC wrapper return normalization, including warnings."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x10000)
    stack,stop=base+0x8000,base+0xf000;state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def hook(machine,at,size,_):
        if at==0x4e1a60:state['diagnostics']+=1
        else:machine.reg_write(UC_X86_REG_EAX,state['raw']&0xffff)
    for at,cleanup in ((0x4f0458,24),(0x4f04b2,4),(0x4e1a60,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    print(f'# ps_game.exe SHA256 {sha}; complete SDatabase::GetData 004e15b0 and Fetch 004e2090; raw ODBC return/diagnostic lookup are ports. Success-with-info normalizes to zero; raw signed return remains in original +0c.')
    for stage in ('G','F'):
        for raw in (0,1,100,-1,-2,2):
            state.update(raw=raw,diagnostics=0);put(stack,stop)
            for i,value in enumerate((1,2,5,base+0x100,base+0x200)):put(stack+4+i*4,value)
            cpu.reg_write(UC_X86_REG_ECX,base);cpu.reg_write(UC_X86_REG_ESP,stack)
            cpu.emu_start(0x4e15b0 if stage=='G' else 0x4e2090,stop,count=1000)
            if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Incomplete original ODBC wrapper')
            result=cpu.reg_read(UC_X86_REG_EAX)&65535;result=result-65536 if result>=32768 else result
            stored=struct.unpack('<i',cpu.mem_read(base+12,4))[0]
            print(stage,raw,result,stored,state['diagnostics'],sep='\t')

if __name__=='__main__':main()
