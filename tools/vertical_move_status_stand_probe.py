#!/usr/bin/env python3
"""Execute original StatusStand state changes with shop shutdown recorded."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EIP,UC_X86_REG_ESP
from original_oracle import load_game

def main():
    cpu,digest=load_game();base=0x30000000;cpu.mem_map(base,0x20000)
    user,stack,stop=base,base+0x18000,base+0x1f000;calls=[]
    def put(offset,value):cpu.mem_write(user+offset,struct.pack('<I',value&0xffffffff))
    def get(offset):return struct.unpack('<I',cpu.mem_read(user+offset,4))[0]
    def hook(machine,address,size,_):
        if address!=0x478520:return
        calls.append((get(0x57d8),get(0x57dc),cpu.mem_read(user+0x1434,1)[0],get(0x1274)))
        sp=machine.reg_read(UC_X86_REG_ESP);machine.reg_write(UC_X86_REG_EIP,struct.unpack('<I',machine.mem_read(sp,4))[0]);machine.reg_write(UC_X86_REG_ESP,sp+4)
    cpu.hook_add(UC_HOOK_CODE,hook)
    count=0
    for motion in (0,1,2):
        for logout in (0,2):
            for connected in (False,True):
                for transfer in (0,1):
                    for recovery in (-3,0,1,7):
                        cpu.mem_write(user,bytes(0x6000));calls.clear();cpu.mem_write(user+0x1434,bytes([motion]))
                        put(0x57d8,logout);put(0x57dc,55);put(0x68,0 if connected else 0xffffffff);put(0x57e0,transfer);put(0x1274,recovery)
                        cpu.mem_write(stack,struct.pack('<I',stop));cpu.reg_write(UC_X86_REG_EAX,user);cpu.reg_write(UC_X86_REG_ESP,stack)
                        cpu.emu_start(0x48a100,stop,count=1000)
                        clear=motion==1 and logout!=0 and connected and transfer!=1
                        expected=(0 if clear else logout,0 if clear else 55,0 if motion==1 else motion,max(1,recovery) if motion==1 else recovery)
                        actual=(get(0x57d8),get(0x57dc),cpu.mem_read(user+0x1434,1)[0],struct.unpack('<i',cpu.mem_read(user+0x1274,4))[0])
                        assert actual==expected,(motion,logout,connected,transfer,recovery,actual,expected)
                        assert len(calls)==(1 if motion==1 else 0)
                        if calls:assert calls[0]==(expected[0],expected[1],expected[2],expected[3]&0xffffffff)
                        count+=1
    print(f'ps_game.exe {digest} CUser::StatusStand 0048a100: {count} cases passed; CUserShop::Ended 00478520 ported')
if __name__=='__main__':main()
