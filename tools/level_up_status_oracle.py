#!/usr/bin/env python3
"""Execute complete original LevelUpStatus, including CRT float conversion."""
import struct
from unicorn.x86_const import UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x10000);stop=base+0xf000
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    fields=((0x136,6),(0x170,24),(0x1220,24))
    def snapshot():return b''.join(bytes(cpu.mem_read(base+offset,n)) for offset,n in fields).hex()
    # Deterministic non-linear status table; actual source lookup and math run.
    for i in range(-1,366):cpu.mem_write(0x54c648+i*6,struct.pack('<3H',*((i*i*17+j*123+i*91+1000)&65535 for j in range(3))))
    print(f'# ps_game.exe SHA256 {digest}; complete CUser LevelUpStatus 0048a5e0; no ports; actual CRT ftol2; synthetic status table formula i*i*17+j*123+i*91+1000 modulo WORD, i=-1..365.')
    print('# scenario class mode initial_projection expected_projection; fields level/statpoints/skillpoints WORDs, six attributes WORDs, HP/MP/SP DWORDs, six runtime attributes DWORDs')
    for scenario in range(384):
        cpu.mem_write(base,bytes([scenario&255])*0x6000);job=(0,1,2,3,4,5,6,255)[scenario%8];mode=(scenario//8)%6;level=(0,1,59,60,61,100)[(scenario//13)%6]
        cpu.mem_write(base+0x12f,bytes([mode]));cpu.mem_write(base+0x134,bytes([job]));cpu.mem_write(base+0x136,struct.pack('<3H',level,(scenario*991)&65535,(65535-scenario*39)&65535));cpu.mem_write(base+0x170,struct.pack('<6H',*((scenario*123+j*999)&65535 for j in range(6))))
        put(base+0x17c,*((0,1,10000,0x7fffffff,0x80000000,0xffffffff)[(scenario+j)%6] for j in range(3)));put(base+0x1220,*((scenario*0x1234567+j)&0xffffffff for j in range(6)))
        before=snapshot();put(base+0xe000,stop);cpu.reg_write(UC_X86_REG_ESP,base+0xe000);cpu.reg_write(UC_X86_REG_EDI,base);cpu.reg_write(UC_X86_REG_FPCW,0x27f);cpu.emu_start(0x48a5e0,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('LevelUpStatus did not return')
        print(scenario,job,mode,before,snapshot(),sep='\t')
if __name__=='__main__':main()
