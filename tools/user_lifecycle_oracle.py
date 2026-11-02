#!/usr/bin/env python3
"""Execute original ps_dbAgent user exit/select/save-job dispatch instructions.

Ghidra exports: work/user-lifecycle and work/user-dispatch. Saving, ResetData,
registry removal, roster load and transport are explicit boundary ports.
"""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_original


def main():
    cpu,digest=load_original('ps_dbAgent');stack,user,stop=0x20000000,0x30000000,0x10000000
    for at in (stack,user,stop):cpu.mem_map(at,0x10000)
    def word(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def put(at,*values):cpu.mem_write(at,struct.pack('<'+'I'*len(values),*values))
    events=[];state={}
    def hook(machine,address,size,context):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if address==0x431a60:events.append('POST '+str(word(sp+4)))
        elif address==0x4327a0:events.append('SEND '+bytes(cpu.mem_read(word(sp+4),word(sp+8))).hex())
        elif address==0x41bf40:events.append('LEAVE '+str(word(sp+4)))
        elif address==0x404f30:
            events.append('SAVE 0 '+str(int(bool(word(sp+4)))));machine.reg_write(UC_X86_REG_EAX,state['success'])
        elif address==0x405000:events.append('SAVE 1 0');machine.reg_write(UC_X86_REG_EAX,state['success'])
        elif address==0x4050b0:events.append('SAVE_EXIT 0')
        elif address==0x405140:events.append('SAVE_EXIT 1')
        elif address==0x402590:events.append('RESET');put(user+0x87f4,1);put(user+0x87fc,0)
        elif address==0x404220:events.append('ROSTER '+str(machine.reg_read(UC_X86_REG_EBX)&255))
    for at,cleanup in ((0x431a60,4),(0x4327a0,8),(0x41bf40,4),(0x404f30,4),(0x405000,0),(0x4050b0,0),(0x405140,0),(0x402590,0),(0x404220,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup));cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    print(f'# ps_dbAgent.exe SHA256 {digest}; ExitUser 00402f70, DisconnectByGameServer 00402fe0, LogoutSelect 00403010, OnDispatchDB 00403dd0; boundary bodies are ports')
    operations=(('EXIT',0),('DISCONNECT',0),('LOGOUT',0),*[('JOB',n) for n in (8,9,10,22)])
    for (kind,job),status,connected,success,uid in itertools.product(operations,(0,1,2,3,0xffffffff),(False,True),(False,True),(0,1,0x7fffffff,0xffffffff)):
        cpu.mem_write(user,bytes(0x10000));cpu.mem_write(stack,bytes(0x10000));put(user+0x38,uid);put(user+0x87f4,status);put(user+0x87d8,0x31000000 if connected else 0);put(user+0x87fc,0xf0000000);put(stack+0x8000,stop,job);events.clear();state['success']=int(success)
        for reg in (UC_X86_REG_ESI,UC_X86_REG_ECX,UC_X86_REG_EAX):cpu.reg_write(reg,user)
        cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
        cpu.emu_start({'EXIT':0x402f70,'DISCONNECT':0x402fe0,'LOGOUT':0x403010,'JOB':0x403dd0}[kind],stop,count=10000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original lifecycle did not return')
        print('CASE',kind,job,status,int(connected),int(success),uid)
        print('STATE',word(user+0x87f4),word(user+0x87fc))
        for event in events:print('EVENT',event)
        print('END')
    for time_id,last in itertools.product((0,1,899,900,901,1799,0x7fffffff,0x80000000,0xffffffff),(0,1,899999,900000,0xffff0000,0xffffffff)):
        put(user+0x8800,time_id);put(0x470894,last);cpu.reg_write(UC_X86_REG_ESI,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000)
        cpu.emu_start(0x404b51,0x404b87,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=0x404b87:raise RuntimeError('Original load clock did not finish')
        print('LOAD_CLOCK',time_id,last,word(user+0x87f4),word(user+0x87fc))


if __name__=='__main__':main()
