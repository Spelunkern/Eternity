#!/usr/bin/env python3
"""Execute PacketPC 0501 through the correction branches; port only I/O and clocks."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from original_oracle import load_game

def main():
 c,digest=load_game();base=0x30000000;c.mem_map(base,0x40000);c.mem_map(0,0x10000)
 user,pkt,stack,stop=base,base+0x8000,base+0x30000,base+0x3f000
 c.reg_write(UC_X86_REG_FPCW,0x27f)
 c.mem_write(0x50d0ca,b'\xb8'+struct.pack('<I',stop+0x1000)+b'\xc3')
 c.mem_write(0x5210e4,struct.pack('<I',stop+0x100));c.mem_write(stop+0x100,b'\xc3')
 events=[]
 def put(p,*v):c.mem_write(p,struct.pack('<'+'I'*len(v),*[x&0xffffffff for x in v]))
 def get(p):return struct.unpack('<I',c.mem_read(p,4))[0]
 def hook(cpu,at,size,data):
  sp=cpu.reg_read(UC_X86_REG_ESP)
  if at==stop+0x100:cpu.reg_write(UC_X86_REG_EAX,1000);events.append('T');return
  if at==0x46a47b:events.append('A');cpu.reg_write(UC_X86_REG_EIP,stop);return
  if at==0x4bd890:
   events.append('V'+bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex());pop=12
  elif at==0x4d4d20:
   events.append('S'+bytes(cpu.mem_read(get(sp+4),get(sp+8))).hex());pop=8
  else:return
  cpu.reg_write(UC_X86_REG_EIP,get(sp));cpu.reg_write(UC_X86_REG_ESP,sp+4+pop)
 c.hook_add(UC_HOOK_CODE,hook)
 print(f'# ps_game.exe SHA256 {digest}; PacketPC 0046a230, real StatusStand 0048a100, Distance2 0041b1f0, Update 0046a120, StatusAttackStop 00489ed0 and CRT sqrt; view/self and tick isolated; stop before accepted MoveUserByPacket.')
 print('# case life soul distance total direction events final_life target previous_tick next_tick recovery request_x')
 values=(0.0,34.999996185302734,35.0,35.000003814697266,40.0,100.0)
 n=0
 for life in (0,1,2,3):
  for soul in (0,1):
   for distance in values:
    for total in (0,20,51,100):
     for direction in (0,0x1234,0xffff):
      c.mem_write(user,bytes(0x6000));c.mem_write(pkt,bytes(32));events.clear()
      put(user+0xdc,0x11223344);put(user+0xd0,*[struct.unpack('<I',struct.pack('<f',x))[0] for x in (10.,2.,10.)]);put(user+0x142c,life,soul);put(user+0x1274,0);put(user+0x1528,7);put(user+0x143c,11,12);put(user+0x1460,2)
      c.mem_write(user+0x164,struct.pack('<H',direction));put(user+0x5494,1000,1000,*[struct.unpack('<I',struct.pack('<f',x))[0] for x in (10.,2.,10.)],999,*[struct.unpack('<I',struct.pack('<f',x))[0] for x in (10.,2.,10.)],struct.unpack('<I',struct.pack('<f',float(total)))[0],0)
      c.mem_write(pkt,struct.pack('<HBHfff',0x501,0x81,0x1234,10.+distance,9.,10.));put(stack,stop,pkt)
      c.reg_write(UC_X86_REG_ESP,stack);c.reg_write(UC_X86_REG_ECX,user)
      c.emu_start(0x46a230,stop,count=100000)
      if c.reg_read(UC_X86_REG_EIP)!=stop:raise RuntimeError('PacketPC failed')
      print(n,life,soul,struct.pack('<f',distance).hex(),total,direction,'|'.join(events) or '-',*[get(user+x) for x in (0x142c,0x1528,0x143c,0x1440,0x1274)],bytes(c.mem_read(pkt+5,4)).hex(),sep='\t');n+=1
if __name__=='__main__':main()
