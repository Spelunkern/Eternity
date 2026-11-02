#!/usr/bin/env python3
"""Execute complete forced-removal coordinator with observed dependency ports."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;stop=base+0xf000
    cpu.mem_map(base,0x10000)
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    zone,mob,respawn,definition,map_object=base,base+0x1000,base+0x3000,base+0x4000,base+0x5000
    context={}
    ports={0x416800:('F',4),0x416790:('G',0),0x424ac0:('C',0),0x4208c0:('V',12),0x42a410:('O',16),0x41d700:('o',0),0x41bad0:('B',4),0x41d730:('b',0),0x40a630:('P',0),stop+16:('T',0)}
    for at,(_,cleanup) in ports.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210e4,stop+16)
    def hook(machine,address,size,data):
        if address not in ports:return
        tag=ports[address][0];context['events']+=tag
        if tag=='F':machine.reg_write(UC_X86_REG_EAX,mob if context['found'] else 0)
        elif tag=='T':machine.reg_write(UC_X86_REG_EAX,0xfffffff0)
        elif tag=='V':
            sp=machine.reg_read(UC_X86_REG_ESP);packet=get(sp+8)
            if bytes(machine.mem_read(packet,6))!=struct.pack('<HI',0x602,77):raise RuntimeError('Removal packet differs')
            if get(mob+0x60)!=0 or get(mob+0x64)!=1 or get(mob+0x68)!=2:raise RuntimeError('Removal delivery state differs')
        elif tag in 'OoBbP':
            if get(mob+0x60) or get(mob+0x64) or get(mob+0x68):raise RuntimeError('Owner/recycle before clearing bindings')
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; complete MobRemoveByID 0041e910. Ports: FindMob, global/cell unlink, SendView, obelisk/linked callbacks, FreeMob, clock. Container implementation and callback bodies are not executed.')
    print('# branch found full returned events current deadline')
    for branch in range(6):
        for found in (0,1):
            for full in (0,1):
                cpu.mem_write(base,bytes(0xe000));put(zone+0x24,map_object);put(zone+0x30,base+0x6000);put(base+0x6000+8,base+0x7000)
                put(mob+0x5c,77,zone,1,2);put(mob+0xa8,definition);put(definition+108,32)
                put(mob+0xc84,respawn if branch else 0);put(respawn,0,3,3 if full else 0,123)
                cpu.mem_write(respawn+0x14,bytes([branch==1]));put(respawn+0x18,base+0x8000 if branch in (1,2) else 0)
                cpu.mem_write(respawn+0x1c,bytes([branch in (1,2,3)]));put(respawn+0x20,base+0x9000 if branch in (1,2,3,4) else 0)
                put(map_object+0x1e8,1,base+0xa000);put(base+0xa000+24,2)
                context.update(events='',found=found);put(base+0xe000,stop,77)
                cpu.reg_write(UC_X86_REG_ESP,base+0xe000);cpu.reg_write(UC_X86_REG_ECX,zone)
                cpu.emu_start(0x41e910,stop,count=1000)
                if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Removal did not return')
                print(branch,found,full,cpu.reg_read(UC_X86_REG_EAX)&255,context['events'],get(respawn+8),get(respawn+12),sep='\t')
if __name__=='__main__':main()
