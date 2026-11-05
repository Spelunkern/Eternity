#!/usr/bin/env python3
"""Exercise ps_game.exe CUser::CancleActionExc against the original code."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EDI, UC_X86_REG_ESI, UC_X86_REG_ESP
from original_oracle import load_game

cpu,digest=load_game();stop,stack,user,peer=(0x10000000,0x20000000,0x30000000,0x31000000)
for address in (stop,stack,user,peer):cpu.mem_map(address,0x10000)
def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*[value&0xffffffff for value in values]))
def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
for address,cleanup in ((0x4d4d20,8),(0x415f30,4),(0x47c610,4),(0x4b8aa0,0)):
 cpu.mem_write(address,b'\xc2'+struct.pack('<H',cleanup))
events=[];lookup=0
def hook(machine,address,size,data):
 sp=machine.reg_read(UC_X86_REG_ESP)
 if address==0x4d4d20:events.append(('send',machine.reg_read(UC_X86_REG_ECX),bytes(machine.mem_read(get(sp+4),get(sp+8))).hex()))
 elif address==0x415f30:events.append(('lookup',get(sp+4)));machine.reg_write(UC_X86_REG_EAX,lookup)
 elif address==0x47c610:events.append(('debuff',get(sp+4)))
 elif address==0x4b8aa0:events.append(('ledger',machine.reg_read(UC_X86_REG_ESI)-0x1538))
for address in (0x4d4d20,0x415f30,0x47c610,0x4b8aa0):cpu.hook_add(UC_HOOK_CODE,hook,begin=address,end=address)
def run():
 put(stack+0x8000,stop);cpu.reg_write(UC_X86_REG_EDI,user);cpu.reg_write(UC_X86_REG_ESP,stack+0x8000);cpu.emu_start(0x44b3d0,stop,count=10000)
count=0
for mode in (0,1,2,3,7):
 for has_peer in (False,True):
  for other_mode in (0,1,2,3):
   cpu.mem_write(user,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));events.clear()
   put(user+0xdc,0x12345678);put(user+0x1568,mode,peer if has_peer else 0,999);put(peer+0x1568,other_mode,user,888);put(user+0x5830,77);put(peer+0x5830,88)
   run();local_reset=mode in (1,2) or (mode not in (0,1,2) and has_peer);peer_reset=has_peer and ((mode in (1,2) and other_mode==1) or mode not in (0,1,2))
   expected=[]
   if mode==2 and has_peer and other_mode==1:expected=[('send',peer,'030a7856341201')]
   elif mode not in (0,1,2) and has_peer:expected=[('send',user,'040a02'),('send',peer,'040a03')]
   assert events==expected,(mode,has_peer,other_mode,events)
   assert get(user+0x1568)==(0 if local_reset else mode) and get(peer+0x1568)==(0 if peer_reset else other_mode)
   assert get(user+0x5830)==(0 if mode==2 else 77) and get(peer+0x5830)==(0 if mode==1 and peer_reset else 88)
   count+=1
for available in (False,True):
 cpu.mem_write(user,bytes(0x7000));cpu.mem_write(peer,bytes(0x7000));events.clear();lookup=peer if available else 0
 put(user+0x54cc,5,3,123);put(peer+0x54cc,4,2,456);put(user+0x1528,2);put(peer+0x1528,2);run()
 expected=[('lookup',123)]
 if available:expected += [('debuff',peer),('ledger',peer),('debuff',user),('ledger',user),('send',user,'082402'),('send',peer,'082403')]
 assert events==expected,(available,events)
 assert get(user+0x54cc)==(0 if available else 5) and get(peer+0x54cc)==(0 if available else 4)
 assert get(user+0x54d0)==(0xffffffff if available else 3) and get(user+0x1528)==(0 if available else 2)
print(f'{count} original exchange cases and 2 duel cases passed; ps_game.exe SHA256 {digest}')
