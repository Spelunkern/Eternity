#!/usr/bin/env python3
"""Original construction and ResetCharacter prefix with explicit inherited bytes."""
import struct
from unicorn.x86_const import UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x20000);stack=base+0x18000;stop=base+0x1f000
put=lambda at,v:cpu.mem_write(at,struct.pack('<I',v&0xffffffff))
get=lambda at:struct.unpack('<I',cpu.mem_read(at,4))[0]
# Base connection constructor and friend constructor operate outside these fields.
# Execute actual stUserInfo constructor; only Win32 CS initialization is returned.
for at in (0x449540,0x4b9690):cpu.mem_write(at,b'\xc3')
cs=stop+16;cpu.mem_write(cs,b'\xc2\x08\x00');put(0x5210c8,cs)
# Reset prefix stops BEFORE CDamage reset, a separately verified owner.
print(f'# ps_game.exe {sha}; CUser004499d0 complete owned body, actual stUserInfo00449eb0; base/friend constructors and CS init returned. ResetCharacter0044a050 prefix to0044a187 (before damage reset). No whole character-reset/world equivalence claim.')
values=(0,1,0x7fffffff,0x80000000,0xffffffff,0xa5a55a5a)
for case in range(216):
    initial=(values[case%6],values[case//6%6],values[case//36%6],values[(case+3)%6])
    outputs=[]
    for mode in range(2):
        cpu.mem_write(base,bytes([0xa5])*0x8000)
        for at,value in zip((0x145c,0x1430,0x1454,0x1458),initial):put(base+at,value)
        put(stack,stop);put(stack+4,base);cpu.reg_write(UC_X86_REG_ESP,stack)
        end=stop if mode==0 else 0x44a187
        cpu.emu_start(0x4499d0 if mode==0 else 0x44a050,end,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=end:raise RuntimeError('Incomplete initialization projection')
        outputs.extend(get(base+at) for at in (0x145c,0x1430,0x1454,0x1458))
    print(case,*initial,*outputs,sep='\t')
