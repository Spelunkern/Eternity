#!/usr/bin/env python3
"""Execute complete original sequence configuration initialization/reload."""
import argparse
import configparser
import hashlib
from pathlib import Path
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--ini',type=Path);args=parser.parse_args()
    cpu,sha=load_game();base=0x30000000;cpu.mem_map(base,0x100000)
    catalog,stack,stop=base,base+0xf0000,base+0xff000
    state={}
    def put(at,value):cpu.mem_write(at,struct.pack('<I',value&0xffffffff))
    def get(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def string(at):
        value=bytearray()
        while at and cpu.mem_read(at,1)!=b'\0':value.extend(cpu.mem_read(at,1));at+=1
        return value.decode('ascii')
    for at,cleanup in ((stop+16,16),(0x504495,0),(0x4d9510,0),(0x505316,0)):
        cpu.mem_write(at,b'\xc2'+struct.pack('<H',cleanup))
    put(0x5210f0,stop+16)
    diagnostics={0x539a1c:'I0',0x5399e4:'I1',0x53998c:'I2',0x539944:'R0',0x539910:'R1',0x5398d8:'R2'}
    def hook(machine,at,size,_):
        sp=machine.reg_read(UC_X86_REG_ESP)
        if at==stop+16:
            group,key=string(get(sp+4)),string(get(sp+8))
            if get(sp+12)!=0 or string(get(sp+16))!='.\\Data\\SequenceItemCreate.ini':raise RuntimeError('Unexpected profile boundary')
            if group=='SEQUENCE_GROUP_COUNT' and key=='COUNT':value=state['values'][0]
            else:
                i=int(group.removeprefix('SEQUENCE_GROUP_'))-1
                j=0 if key=='SEQUENCE' else int(key.removeprefix('GRADE_'))+1
                value=state['values'][1+i*11+j]
            state['events'].append(f'P{group}:{key}={value}')
            machine.reg_write(UC_X86_REG_EAX,value)
        elif at==0x504495:
            fmt=string(get(sp+8));number=get(sp+12)
            if fmt not in ('SEQUENCE_GROUP_%d','GRADE_%d'):raise RuntimeError('Unexpected format')
            result=(fmt%number).encode();cpu.mem_write(get(sp+4),result+b'\0');machine.reg_write(UC_X86_REG_EAX,len(result))
        elif at==0x4d9510:state['events'].append(diagnostics[get(sp+8)])
    for at in (stop+16,0x504495,0x4d9510):cpu.hook_add(UC_HOOK_CODE,hook,begin=at,end=at)
    def words(values):return struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values])
    print(f'# ps_game.exe SHA256 {sha}; complete InitSequenceItemCreate 004be520 and ReSequenceItemCreate 004be6a0. Ports: Windows integer profile reads, known decimal key formatting, diagnostics and stack cookie. All count clamping, retained state and partial writes execute unchanged.')
    cases=[]
    for case in range(256):
        initial=[(0,3,8,0xffffffff)[(case//4)%4]]
        for i in range(8):initial.extend([1000+i,*[case*100+i*10+j for j in range(10)]])
        values=[(0,1,3,8,9,0xffffffff,0x80000000,100)[case%8]]
        zero=(case//8)%10-1
        for i in range(8):values.extend([0 if i==zero else (i+1)*37+case,*[(0,1,1024,1025,65536,0xffffffff)[(case+i+j)%6] for j in range(10)]])
        cases.append(('S',case,(case//64)%2,initial,values))
    if args.ini:
        raw=args.ini.read_bytes();profile=configparser.ConfigParser();profile.read_string(raw.decode('cp949'))
        values=[int(profile.get('SEQUENCE_GROUP_COUNT','COUNT',fallback='0'))&0xffffffff]
        for i in range(8):values.extend([int(profile.get(f'SEQUENCE_GROUP_{i+1}','SEQUENCE',fallback='0'))&0xffffffff,*[int(profile.get(f'SEQUENCE_GROUP_{i+1}',f'GRADE_{j}',fallback='0'))&0xffffffff for j in range(10)]])
        print('# Asset SHA256',hashlib.sha256(raw).hexdigest())
        cases.append(('A',0,0,[0]*89,values))
    for tag,case,reload,initial,values in cases:
        put(catalog+0x3184,initial[0]);cpu.mem_write(catalog+0x3024,words(initial[1:]))
        state.update(values=values,events=[])
        put(stack,stop);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,catalog)
        cpu.emu_start(0x4be6a0 if reload else 0x4be520,stop,count=100000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Sequence configuration did not return')
        result=[get(catalog+0x3184),*struct.unpack('<'+'I'*88,cpu.mem_read(catalog+0x3024,352))]
        success=int(cpu.reg_read(UC_X86_REG_EAX)==1) if reload else 0
        print(tag,case,reload,words(initial).hex(),words(values).hex(),words(result).hex(),success,','.join(state['events']).encode().hex(),sep='\t')


    for grade in (1025,65535,65536,65537,131073,0x7fffffff):
        put(catalog+0x3184,1);put(catalog+0x3024,2);put(catalog+0x3028+4,grade)
        put(stack,stop);put(stack+4,10);put(stack+8,2);cpu.reg_write(UC_X86_REG_ESP,stack);cpu.reg_write(UC_X86_REG_ECX,catalog)
        cpu.emu_start(0x4be490,stop,count=1000)
        if cpu.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('Wide sequence grade lookup did not return')
        print('Q',grade,cpu.reg_read(UC_X86_REG_EAX),sep='\t')

if __name__=='__main__':main()
