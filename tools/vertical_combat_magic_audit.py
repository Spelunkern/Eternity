#!/usr/bin/env python3
"""Independent Python arithmetic audit of original magic AttackDamage fixtures."""
import math,struct
from pathlib import Path
from original_oracle import load_game
ROOT=Path(__file__).resolve().parents[1]
c,_=load_game();TABLE=struct.unpack('<81f',c.mem_read(0x542260,324))
def f32(v):return struct.unpack('<f',struct.pack('<f',v))[0]
def signed(v):return (v+2**31)%2**32-2**31
def fist(v):
 if not math.isfinite(v):return -2**31
 n=math.trunc(v)
 return n if -2**31<=n<2**31 else -2**31
def check(path,skill):
 count=0
 for line in open(path):
  if not line.strip() or line.startswith('#'):continue
  row=line.split();i=0
  def number():
   nonlocal i
   v=int(row[i]);i+=1;return v
  kind=number();assert kind==2
  if skill:
   mode,detail,override=number(),number(),number();values=[number() for _ in range(3)];recover=[number() for _ in range(3)];maximum=[number() for _ in range(3)];current=[number() for _ in range(3)]
  else:mode,detail,override=99,0,0;values=[0]*3
  ul,ml,attack,variance,wis,critical,luck,defense,ae,de,passive=[number() for _ in range(11)];distance=float(row[i]);i+=1;rolls=[number() for _ in range(5)];expected_critical=number();expected=[number() for _ in range(3 if skill else 1)];expected_draws=number();expected_distance=number();assert expected_distance==0
  draws=0
  def draw():
   nonlocal draws
   result=rolls[draws];draws+=1;return result
  result=[0,0,0];is_critical=False
  if skill and mode==0:result=values.copy()
  elif skill and mode==3:
   for n in range(3):
    if values[n]:result[n]=min(30000,max(1,signed(recover[n]*values[n])))
  elif skill and mode in (4,5):
   source=maximum if mode==4 else current
   for n in range(3):
    if values[n]:result[n]=min(30000,max(1,signed(math.trunc((f32(values[n])/100.0)*f32(source[n])))))
  else:
   if skill and mode==1:result=values.copy()
   level=max(1.0,min(95.0,f32(((f32(ml-ul-1)*100.0+f32(ml))/100.0)*10.0)))
   attack=f32(signed(attack+result[0]));attack=f32(attack+f32(draw()%variance)) if variance>0 else attack
   a=override if override else ae;d=0 if skill and detail==24 else de;element=TABLE[a*9+d] if a<9 and d<9 else 1.0
   first=draw()%abs(wis)+1 if wis else 0
   defense=f32(defense);value=f32((attack-attack/100.0*level)*element-(defense+defense/100.0*level)+f32(first)*f32(.1));value=max(value,0.0)
   rate=max(5,min(99,fist(f32(critical)/100.0*100.0-f32(luck)*f32(.034))))
   is_critical=draw()%100<rate
   if is_critical:
    bonus=f32(f32(draw()%abs(wis)+1 if wis else 0)*f32(1.4));value=f32(value*1.5+bonus-bonus/100.0*level)
   result[0]=fist(value)
   if skill and mode==2:
    for n in range(3):
     if values[n]:result[n]=signed(result[n]*values[n])
   result=[min(v,30000) for v in result]
   if result[0]<11:result[0]=draw()%10+1
  compared=result if skill else result[:1]
  if compared!=expected or int(is_critical)!=expected_critical or draws!=expected_draws:
   raise RuntimeError(f'{path.name} row {count}: {compared}, {is_critical}, {draws} != {expected}, {expected_critical}, {expected_draws}')
  count+=1
 print(path.name,count,'independent comparisons passed')
check(ROOT/'tests/data/vertical_combat_magic_damage_oracle.tsv',False)
check(ROOT/'tests/data/vertical_combat_magic_skill_damage_oracle.tsv',True)
