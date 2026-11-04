#!/usr/bin/env python3
"""Execute complete original ReLoadConstMobItem over explicit row/error streams."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EDI,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    table,pointer,counter,database=base+0x10000,base+0x70000,base+0x71000,base+0x72000
    stack,stop=base+0xf0000,base+0xff000;state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def string(at):
        result=bytearray()
        while cpu.mem_read(at,1)!=b'\0':result.extend(cpu.mem_read(at,1));at+=1
        return result.decode('ascii')
    ports={stop+16:24,0x4cb4e0:0,0x4d3840:28,0x4d3c40:0,0x504495:0,0x4e2050:4,0x4e2090:0,0x4e15b0:20,0x4d9510:0,0x4d3d00:4,0x4d33e0:0,0x4d35e0:0,0x505316:0}
    for at,cleanup in ports.items():cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210ec,stop+16)
    def hook(machine,at,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if at in (stop+16,0x4d3840):machine.reg_write(UC_X86_REG_EAX,1)
        elif at==0x4d3c40:machine.reg_write(UC_X86_REG_EAX,database)
        elif at==0x504495:
            if string(get(sp+8))!='EXEC usp_Read_MobItems_R2 %d' or get(sp+12)!=state['request']:raise RuntimeError('Original selective query changed')
            result=f'EXEC usp_Read_MobItems_R2 {state["request"]}'.encode();cpu.mem_write(get(sp+4),result+b'\0');machine.reg_write(UC_X86_REG_EAX,len(result))
        elif at==0x4e2050:
            if string(get(sp+4))!=f'EXEC usp_Read_MobItems_R2 {state["request"]}':raise RuntimeError('Original reload query differs')
            machine.reg_write(UC_X86_REG_EAX,state['query_fail'])
        elif at==0x4e2090:
            state['index']+=1;machine.reg_write(UC_X86_REG_EAX,100 if state['index']>=len(state['rows']) else 0)
        elif at==0x4e15b0:
            column=get(sp+4);width=get(sp+8);ctype=get(sp+12);dest=get(sp+16)
            if width!=(2,1,2,4)[column-1] or ctype!=(5,0xfffffffa,5,4)[column-1]:raise RuntimeError('Original ODBC projection differs')
            fail=state['index']==state['fail_row'] and column==state['fail_col']
            if not fail:
                value=state['rows'][state['index']][column-1];cpu.mem_write(dest,(value&((1<<(width*8))-1)).to_bytes(width,'little'))
            machine.reg_write(UC_X86_REG_EAX,int(fail))
        elif at==0x4d9510:state['events']+='X' if get(sp+8)==0x5394bc else 'E'
    for at in ports:cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    def snapshot():return b''.join(bytes(cpu.mem_read(table+m*0x10c+0xbc+i*8,2))+bytes(cpu.mem_read(table+m*0x10c+0xc0+i*8,4)) for m in range(4) for i in range(10))
    print(f'# ps_game.exe SHA256 {sha}; complete CGameData::ReLoadConstMobItem 004c2e70. Ports: profile/credential parsing, dedicated DB pool, exact query and ODBC row/error stream, diagnostics, cookie. Actual in-place comparisons/writes, wrapping counter, rejection/partial-failure control and cleanup execute. Native uses supplied SQL owner rather than original global pool lifecycle.')
    for case in range(512):
        cpu.mem_write(base,bytes(0x80000));put(pointer,4);put(pointer+4,table)
        initial=[]
        for m in range(4):
            for i in range(10):
                grade=(case+m*10+i)%65536;rate=(case*100+m*10+i)&0xffffffff
                cpu.mem_write(table+m*0x10c+0xbc+i*8,struct.pack('<H2xI',grade,rate));initial.append((grade,rate))
        rows=[]
        for i in range(8):
            mob=(0,1,3,4,65535)[(case+i)%5];order=(0,1,9,10,255)[(case//5+i)%5]
            grade=(0,65535,case,initial[(mob%4)*10+(order%10)][0])[(case//7+i)%4];rate=(0,0xffffffff,case*100,initial[(mob%4)*10+(order%10)][1])[(case//11+i)%4]
            rows.append((mob,order,grade,rate))
        if case%3==0:rows[1]=rows[0]
        if case%13==0:rows=[]
        failure=(case//17)%10-1;column=case%4+1;query_fail=int(case%29==0);request=(case*4093)&65535
        initial_counter=(0,17,0x7ffffffe,0xfffffffe)[(case//19)%4];put(counter,initial_counter)
        before=snapshot();state.update(rows=rows,index=-1,fail_row=failure,fail_col=column,query_fail=query_fail,request=request,events='')
        put(stack,stop);put(stack+4,pointer-0xa81804);put(stack+8,request);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_EDI,counter)
        cpu.emu_start(0x4c2e70,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original MobItems reload did not return')
        print('S',case,request,initial_counter,failure,column,query_fail,len(rows),cpu.reg_read(UC_X86_REG_EAX)&255,get(counter),before.hex(),snapshot().hex(),state['events'] or '-',sep='\t')
        for row in rows:print('R',*row,sep='\t')


    # Separate original loot execution observes later slot reads after a delivery
    # callback installs the exact two-field reload effect. Native callback uses
    # the actual reload engine; complete reload bodies are compared above.
    cpu.mem_map(0,0x1000);cpu.mem_write(0x4bdd10,b'\xc3');cpu.mem_write(0x4a65a0,b'\xc2\x10\x00');cpu.mem_write(0x504697,b'\xc3')
    mob,zone,map_info=base+0x20000,base+0x30000,base+0x31000;definition=table+0x10c
    cpu.mem_write(base,bytes(0x80000));put(mob+0xa8,definition);put(mob+0x60,zone);put(zone+0x24,map_info)
    for i in range(1,4):cpu.mem_write(definition+0xbc+i*8,struct.pack('<H2xI',i,100))
    loot={'events':[],'calls':0,'mutated':False}
    def observed(machine,at,size,_):
        if at==0x504697:loot['calls']+=1;machine.reg_write(UC_X86_REG_EAX,0)
        elif at==0x4bdd10:
            grade=machine.reg_read(UC_X86_REG_EAX);loot['events'].append('S'+str(grade));machine.reg_write(UC_X86_REG_EAX,grade+1000)
        elif at==0x4a65a0:
            loot['events'].append('D'+str(get(machine.reg_read(UC_X86_REG_ESP)+4)))
            if not loot['mutated']:
                cpu.mem_write(definition+0xbc+2*8,struct.pack('<H',9));put(definition+0xc0+3*8,0);loot['mutated']=True
    for at in (0x504697,0x4bdd10,0x4a65a0):cpu.hook_add(UC_HOOK_CODE,observed,begin=at,end=at)
    put(stack,stop);put(stack+4,mob);put(stack+8,0);put(stack+12,0);cpu.reg_write(UC_X86_REG_ESP,stack)
    cpu.emu_start(0x4a5ea0,stop,count=100000)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Original live loot read did not return')
    print('L',','.join(loot['events']),loot['calls'],sep='\t')

if __name__=='__main__':main()
