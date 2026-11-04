#!/usr/bin/env python3
"""Original full ResetAttack coordination; actual ClearApplySkill, observed world ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x10000)
    mob,info,head,stop=base,base+0x2000,base+0x3000,base+0xf000
    def put(at,*v):cpu.mem_write(at,struct.pack('<'+'I'*len(v),*[n&0xffffffff for n in v]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def core():
        values=[get(mob+0x224),*struct.unpack('<2H',cpu.mem_read(mob+0x228,4))]
        values += [get(mob+x) for x in (0x22c,0x230,0x234,0x238,0x248,0x244,0x24c,0x23c,0x240,0x130,0x1b8,0x1bc,0x1c0,0x124,0x2b8,0x128,0x12c)]
        return struct.pack('<20I',*values).hex()
    state={}
    for at,code in ((0x4906d0,b'\xc3'),(0x4902f0,b'\xc3'),(0x421220,b'\xc2\x20\x00'),(stop+16,b'\xc2\x04\x00')):cpu.mem_write(at,code)
    put(0x5210cc,stop+16);put(0x5210d0,stop+16)
    def hook(machine,at,size,data):
        if at not in (0x4a0990,0x421220,0x4906d0,0x4902f0):return
        assert bytes(cpu.mem_read(mob+0x134,84))==bytes(84) and bytes(cpu.mem_read(mob+0x188,48))==bytes(48)
        if at==0x4a0990:return
        tag={0x4a0990:'C',0x421220:'V',0x4906d0:'S',0x4902f0:'A'}[at]
        if at==0x421220:
            esp=machine.reg_read(UC_X86_REG_ESP);args=struct.unpack('<8I',cpu.mem_read(esp+4,32))
            assert args[0]==base+0x4000 and args[2:]==(10,mob+0x50,0x42700000,0,0,6)
            tag+=':'+bytes(cpu.mem_read(args[1],10)).hex()
        state['events'].append(tag+':'+core())
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; CMob::ResetAttack 004905c0 with actual ClearApplySkill 004a0990. Empty list; world HP view/SendSpeed/SetAttack are observed ports. Resources and controls sampled at every boundary; attack buffers asserted zero before SetAttack.')
    for case in range(64):
        cpu.mem_write(mob,b'\xa5'*0x1000);cpu.mem_write(info,bytes(0x100));put(mob+0xa8,info);put(mob+0x5c,100+case);put(mob+0x60,base+0x4000 if case&1 else 0)
        put(mob+0x1c4,head,head);put(head,head,head);put(mob+0x1e4,0)
        put(info+0x30,0xffffffff-case);cpu.mem_write(info+0x34,struct.pack('<5H',65535-case,32768+case,60000+case,50000+case,40000+case));cpu.mem_write(info+0x40,bytes([case%9]))
        put(base+0xe000,stop);cpu.reg_write(UC_X86_REG_ESP,base+0xe000);cpu.reg_write(UC_X86_REG_EAX,mob);state['events']=[]
        cpu.emu_start(0x4905c0,stop,count=10000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
        print(case,';'.join(state['events']),core(),sep='\t')
if __name__=='__main__':main()
