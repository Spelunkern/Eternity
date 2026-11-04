#!/usr/bin/env python3
"""Execute actual ownership change/list packet assembly in ps_game.exe."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDX,
                              UC_X86_REG_EDI, UC_X86_REG_ESP, UC_X86_REG_EIP)
from original_oracle import load_game


def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    map_,ob,zone=base,base+0x1000,base+0x10000
    stack,stop=base+0xe0000,base+0xf0000
    state={}
    def put(at,*values): cpu.mem_write(at,struct.pack('<'+'I'*len(values),*[v&0xffffffff for v in values]))
    def get(at): return struct.unpack('<I',cpu.mem_read(at,4))[0]
    for at,cleanup in ((0x4207f0,0),(0x4d4f10,8)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    def hook(machine,address,size,data):
        if address not in (0x4207f0,0x4d4f10): return
        sp=machine.reg_read(UC_X86_REG_ESP)
        length=machine.reg_read(UC_X86_REG_EAX) if address==0x4207f0 else get(sp+8)
        pointer=machine.reg_read(UC_X86_REG_ECX) if address==0x4207f0 else get(sp+4)
        state['trace'].extend([1 if address==0x4207f0 else 2,length,*cpu.mem_read(pointer,length)])
        state['sends']+=1
        if state['op']==2 and state['scenario']//36%2 and state['sends']==1:
            # An explicit transport callback updates later records after the
            # first full chunk, testing that the original rereads live storage.
            for row in range(100,state['count']):
                at=ob+row*0x70;put(at+4,2,-1);put(at+0x14,get(at+0x14)^0xa5a5a5a5);put(at+0x10,get(at+0x10)+99)
    cpu.hook_add(UC_HOOK_CODE,hook)
    print(f'# ps_game.exe SHA256 {digest}; CMap::SendObeliskChange 0042b200, SendDBObeliskChange 0042b260, SendDBObeliskList 0042b370. Only zone/DB transport are observed; actual serializer/loop executes. Countries include signed/extreme values to check original byte packing; list callbacks mutate later records.')
    print('# scenario then trace words: tag 1 zone / 2 DB, byte length, packet bytes. Counts cycle 0,1,2,99,100,101,199,200,201,255,256,301; op=scenario//12%3; mutation=scenario//36%2.')
    for scenario in range(216):
        cpu.mem_write(base,bytes(0xff000))
        count=(0,1,2,99,100,101,199,200,201,255,256,301)[scenario%12];op=scenario//12%3
        state.update(scenario=scenario,count=count,op=op,trace=[],sends=0)
        put(map_,zone if scenario%7 else 0);put(map_+0x1e8,count,ob)
        cpu.mem_write(map_+0x1e4,bytes([scenario//3%2]))
        for row in range(max(count,1)):
            at=ob+row*0x70
            put(at,0x81000000+row*65537+scenario,(-1,0,1,2,15,0x7fffffff,-0x80000000)[(scenario+row)%7],
                (-1,0,1,2,15,31,0x7fffffff,-0x80000000)[(scenario//2+row)%8])
            put(at+0x10,0xffffffff-(scenario*65539+row*7),0xf1230000+row*37+scenario)
        index=scenario//5%max(count,1)
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack)
        cpu.reg_write(UC_X86_REG_EAX,index);cpu.reg_write(UC_X86_REG_ECX,map_)
        cpu.reg_write(UC_X86_REG_EDX,map_);cpu.reg_write(UC_X86_REG_EDI,map_)
        cpu.emu_start((0x42b200,0x42b260,0x42b370)[op],stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop: raise RuntimeError('Original instruction limit exceeded')
        print(scenario,' '.join(map(str,state['trace'])),sep='\t')

if __name__=='__main__': main()
