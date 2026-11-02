#!/usr/bin/env python3
"""Execute original obelisk corpse deadline instructions, without traversal."""
import struct
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x10000)
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    print(f'# ps_game.exe SHA256 {digest}; UpdateMobDeath obelisk deadline 00421bdd..00421d62; no dependency ports, traversal/unlink excluded.')
    print('# old_next now delay count expected_next expected_count')
    for now in (0,1,1000,0xfffffff0,0xffffffff):
        for old in (0,now,(now+1)&0xffffffff,0xffffffff):
            for delay in (0,5000,604800000):
                mob,respawn,definition=base,base+0x2000,base+0x3000;put(mob+0xa8,definition);put(definition+108,delay);put(respawn+8,7);put(respawn+12,old)
                for reg,value in ((UC_X86_REG_EAX,respawn),(UC_X86_REG_ESI,mob),(UC_X86_REG_EBP,0),(UC_X86_REG_EBX,now)):cpu.reg_write(reg,value)
                cpu.emu_start(0x421bdd,0x421d62,count=100)
                if cpu.reg_read(UC_X86_REG_EIP)!=0x421d62:raise RuntimeError('Obelisk deadline did not finish')
                print(old,now,delay,7,get(respawn+12),get(respawn+8),sep='\t')
if __name__=='__main__':main()
