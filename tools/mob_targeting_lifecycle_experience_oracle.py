#!/usr/bin/env python3
"""Complete original CUser::GetExp, including raw int level wrap and x87 stores."""
import struct
from original_oracle import load_game
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW

def main():
    c,digest=load_game();base=0x30000000;stack=base+0x2000;stop=base+0x3000;c.mem_map(base,0x4000)
    print('# ps_game.exe SHA256 '+digest+'; complete CUser::GetExp 004581b0; no ports/hooks/patches; x87 control 027f restored')
    print('# victim_word recipient_raw_dword expected_raw_dword')
    for case in range(8192):
        level=(0,1,9,10,29,60,65535,(case*37)&65535)[case%8]
        recipient=(level-10,level-9,level-1,level,level+1,level+5,level+6,level+7,0x7fffffff,-0x80000000,-1,-10,(case*15485863)&0xffffffff)[(case//8)%13]&0xffffffff
        c.mem_write(base+0x136,struct.pack('<H',level));c.mem_write(stack,struct.pack('<I',stop))
        c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_EAX,base);c.reg_write(UC_X86_REG_ECX,recipient);c.reg_write(UC_X86_REG_FPCW,0x27f)
        c.emu_start(0x4581b0,stop,count=200)
        if c.reg_read(UC_X86_REG_EIP)!=stop or c.reg_read(UC_X86_REG_FPCW)!=0x27f:raise RuntimeError('Incomplete GetExp or changed control word')
        print(level,recipient,c.reg_read(UC_X86_REG_EAX))
if __name__=='__main__':main()
