#!/usr/bin/env python3
"""Execute complete original CMob::GetExp, without dependency ports."""
import struct
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x10000);stop=base+0xf000
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    print(f'# ps_game.exe SHA256 {digest}; complete CMob GetExp 00491690; no ports, x87 precision/control 027f.')
    print('# mob_level base_exp user_level admin_mode event_active multiplier_bits expected')
    for scenario in range(1024):
        level=(0,1,38,60,65535)[scenario%5];exp=(0,1,17,1000,65535)[(scenario//5)%5];user=(level-10,level-9,level-1,level,level+1,level+5,level+6,0x7fffffff,-0x80000000)[(scenario//7)%9];mode=(scenario//11)%4;active=(scenario//13)%2;multiplier=(0.0,0.5,1.0,1.2,2.0,-1.0,1e20,float('inf'),float('nan'))[(scenario//17)%9]
        put(base+0xa8,base+0x2000);cpu.mem_write(base+0x2022,struct.pack('<HH',level,exp));put(0x546f60,mode);put(0x546f5c,active);cpu.mem_write(0x541b00,struct.pack('<d',multiplier));put(base+0xe000,stop)
        cpu.reg_write(UC_X86_REG_ESP,base+0xe000);cpu.reg_write(UC_X86_REG_EAX,base);cpu.reg_write(UC_X86_REG_ECX,user&0xffffffff);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x491690,stop,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Experience calculation did not return')
        print(level,exp,user,mode,active,struct.pack('<d',multiplier).hex(),cpu.reg_read(UC_X86_REG_EAX),sep='\t')
if __name__=='__main__':main()
