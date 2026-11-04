#!/usr/bin/env python3
"""Direct displacement audit; original symbol names are not trusted field names."""
from pathlib import Path
import bisect,re,subprocess
root=Path(__file__).resolve().parents[1]
functions=[]
for line in (Path('/home/franco/Projects/Eternity/work/reverse/ps_game.exe/functions.tsv')).read_text().splitlines():
    parts=line.split('\t')
    if re.fullmatch('[0-9a-fA-F]{8}',parts[0]):functions.append((int(parts[0],16),parts[1]))
functions.sort();starts=[f[0] for f in functions]
print('# ps_game.exe direct instruction operands +1430/+1454/+1458/+145c. Constructor/reset bulk writes separately executed. Displacements alone do not prove object type; no exhaustive dynamic alias claim.')
for line in subprocess.check_output(['objdump','-d','-Mintel',str(root/'originals/ps_game/ps_game.exe')],text=True).splitlines():
    if not re.search(r'0x14(30|54|58|5c)\]',line):continue
    address=int(line.split(':')[0],16);function=functions[bisect.bisect_right(starts,address)-1]
    print(f'{address:08x}',function[1],line.split('\t')[-1].strip(),sep='\t')
