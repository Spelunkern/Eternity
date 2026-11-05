#!/usr/bin/env python3
"""Probe original SendCharacterFriend under controlled synchronous mutations."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ESP,UC_X86_REG_EIP
from original_oracle import load_game
cpu,digest=load_game();stack,user,peer,world,data,stop=0x20000000,0x30000000,0x31000000,0x32000000,0x33000000,0x10000000
for a in (stack,user,peer,world,data,stop):cpu.mem_map(a,0x10000)
def get(a):return struct.unpack('<I',cpu.mem_read(a,4))[0]
def put(a,*v):cpu.mem_write(a,struct.pack('<'+'I'*len(v),*v))
def friend(i,id):
 a=user+0x1784+80*i;put(a,id);cpu.mem_write(a+6,bytes([id&255]));cpu.mem_write(a+7,('F'+str(id)).encode()+b'\0');cpu.mem_write(a+28,b'\0')
def block(i,id):
 a=user+0x36c8+76*i;put(a,id);cpu.mem_write(a+4,('B'+str(id)).encode()+b'\0');cpu.mem_write(a+25,b'\0')
def remove_first():
 n=get(user+0x1780);a=user+0x1784;b=a+(n-1)*80
 for off,nbytes in ((0,4),(6,1),(7,21),(28,51)):cpu.mem_write(a+off,bytes(cpu.mem_read(b+off,nbytes)))
 put(user+0x1780,n-1);put(b,0);cpu.mem_write(b+6,b'\0\0');cpu.mem_write(b+28,b'\0')
def normalize(p):
 op=struct.unpack_from('<H',p)[0];at=3
 for _ in range(p[2]):
  off,width,memo=(6,21,27) if op==0x2201 else (4,21,25);end=p.index(0,at+off,at+off+width);p[end+1:at+off+width]=bytes(at+off+width-end-1);at+=memo+1+p[at+memo]
 return bytes(p)
events=[];mode='';changed=False
def hook(m,a,size,ctx):
 global changed
 sp=m.reg_read(UC_X86_REG_ESP)
 if a==0x410320:
  key=get(m.reg_read(UC_X86_REG_EBX));events.append(('lookup',key));out=m.reg_read(UC_X86_REG_EAX);put(out,data+0x100 if mode=='lookup_remove_online' else data);m.reg_write(UC_X86_REG_EAX,out)
  if mode in ('lookup_remove','lookup_remove_online') and not changed:changed=True;remove_first()
 elif a==0x4d4d20:
  p=normalize(bytearray(m.mem_read(get(sp+4),get(sp+8))));events.append(('send',p))
  if not changed and mode=='friend_append':changed=True;friend(get(user+0x1780),26);put(user+0x1780,get(user+0x1780)+1)
  elif not changed and mode=='friend_remove':changed=True;remove_first()
  elif not changed and mode=='block_append' and p[:2]==b'\x08\x22':changed=True;block(get(user+0x36c4),26);put(user+0x36c4,get(user+0x36c4)+1)
 elif a==0x505bb0:
  amount=m.reg_read(UC_X86_REG_EAX);ret=get(sp);put(sp-amount,ret);m.reg_write(UC_X86_REG_ESP,sp-amount)
for a,n in ((0x4d4d20,8),(0x410320,0),(0x505bb0,0),(0x505316,0),(stop+0x100,4)):
 cpu.mem_write(a,b'\xc2'+struct.pack('<H',n));cpu.hook_add(UC_HOOK_CODE,hook,begin=a,end=a)
put(0x5210cc,stop+0x100);put(0x5210d0,stop+0x100);put(0x546f38,world);put(world+0x2c,data);put(data+0x110,peer);put(peer+0x1780,1);put(peer+0x1784,101)
def run(friends,blocks,change):
 global mode,changed
 mode=change;changed=False;events.clear();cpu.mem_write(user,bytes(0x10000));put(user+0xdc,101);put(user+0x1780,friends);put(user+0x36c4,blocks)
 for i in range(friends):friend(i,i+1)
 for i in range(blocks):block(i,i+1)
 put(stack+0xf000,stop,user);cpu.reg_write(UC_X86_REG_ESP,stack+0xf000);cpu.emu_start(0x483ac0,stop,count=1000000);assert cpu.reg_read(UC_X86_REG_EIP)==stop
 return list(events)
def sends(e):return [v for kind,v in e if kind=='send']
def ids(p,op):
 a=3;out=[]
 for i in range(p[2]):
  out.append(struct.unpack_from('<I',p,a)[0]);a+=(28+p[a+27] if op==0x2201 else 26+p[a+25])
 return out
p=sends(run(1,0,'lookup_remove'));assert len(p)==1 and ids(p[0],0x2201)==[0] and p[0][7]==0 and p[0][9]==0
p=sends(run(1,0,'lookup_remove_online'));assert len(p)==1 and ids(p[0],0x2201)==[0] and p[0][8]==1
p=sends(run(2,0,'lookup_remove'));assert len(p)==1 and ids(p[0],0x2201)==[2]
p=sends(run(25,0,'friend_append'));assert [x[2] for x in p]==[25,1] and ids(p[1],0x2201)==[26]
p=sends(run(26,0,'friend_remove'));assert [x[2] for x in p]==[25]
p=sends(run(0,25,'block_append'));assert [x[2] for x in p]==[25,1] and ids(p[1],0x2208)==[26]
p=sends(run(1,0,'friend_append'));assert [x[2] for x in p]==[1] and get(user+0x1780)==2
print('7 original reentrant social list cases passed; ps_game.exe SHA256',digest)
