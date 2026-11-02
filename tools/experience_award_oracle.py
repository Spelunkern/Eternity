#!/usr/bin/env python3
"""Execute complete AddExpFromUser with real LevelUpStatus and SendDBExp."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP,UC_X86_REG_FPCW
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x10000);stop=base+0xf000;context={}
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    for at,cleanup in ((0x4d4f10,8),(0x4d4d20,8),(0x48ad70,4)):cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    def hook(machine,address,size,data):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address in (0x4d4f10,0x4d4d20):
            packet,n=struct.unpack('<2I',machine.mem_read(sp+4,8));context['events']+=bytes([ord('B') if address==0x4d4f10 else ord('C')])+struct.pack('<I',n)+bytes(machine.mem_read(packet,n))
        elif address==0x48ad70:
            context['events']+=b'L'+struct.pack('<4I',machine.reg_read(UC_X86_REG_ECX),machine.reg_read(UC_X86_REG_EDX),get(base+0x13c),int.from_bytes(machine.mem_read(base+0x136,2),'little'))
    cpu.hook_add(UC_HOOK_CODE,hook)
    cpu.mem_write(0x54c648,bytes(366*6))
    for grow in range(4):
        for level in range(61):put(0x54cedc+(grow*61+level)*4,level*1000)
    print(f'# ps_game.exe SHA256 {digest}; complete AddExpFromUser 00458240, actual LevelUpStatus 0048a5e0, GetExp 00458180 and SendDBExp 0046c6c0. LevelUp notice and DB/client transport ports. Zero status table; experience table level*1000 for all modes.')
    print('# scenario grow level old_exp award bypass double half bonus party7 expected_events expected_core_hex')
    for scenario in range(512):
        cpu.mem_write(base,bytes(0xd000));grow=scenario%5;level=(0,1,29,30,59,60,61)[(scenario//5)%7];old=(0,999,59000,0xfffffff0)[(scenario//7)%4];award=(0,1,100,1000,50000,0xffffffff,0x80000000,0x7fffffff)[(scenario//11)%8];bypass=(scenario//13)%2;double=(scenario//17)%2;half=(scenario//19)%2;bonus=(scenario//23)%5;party=(scenario//29)%2
        cpu.mem_write(base+0x12f,bytes([grow]));cpu.mem_write(base+0x136,struct.pack('<H',level));put(base+0x13c,old);put(base+0x578c,123);put(base+0x58d4,double);put(base+0x58d0,half);put(base+0x58b0,bonus)
        if party:put(base+0x173c,base+0x8000);put(base+0x8010,7)
        put(base+0xe000,stop,777,award,bypass);cpu.reg_write(UC_X86_REG_ESP,base+0xe000);cpu.reg_write(UC_X86_REG_EAX,base);cpu.reg_write(UC_X86_REG_FPCW,0x27f);context['events']=b'';cpu.emu_start(0x458240,stop,count=1000000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Experience award did not return')
        core=bytes(cpu.mem_read(base+0x136,10))+bytes(cpu.mem_read(base+0x170,24))+bytes(cpu.mem_read(base+0x1220,24))
        print(scenario,grow,level,old,award,bypass,double,half,bonus,party,context['events'].hex() or '-',core.hex(),sep='\t')
if __name__=='__main__':main()
