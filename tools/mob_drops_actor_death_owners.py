#!/usr/bin/env python3
"""Read-only worker08 NPC-death bridge snapshot for this older worker base."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1];main=Path(__file__).resolve().parents[1];manifest={}
for name in ('include/eternity/game/mob_targeting_lifecycle_death.hpp','src/game/mob_targeting_lifecycle_death.cpp'):
    data=(main/name).read_bytes();p=root/'work/mob-drops-actor-death-owners'/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);manifest[name]=hashlib.sha256(data).hexdigest()
(root/'work/mob-drops-actor-death-owners.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Read-only NPC-death owner snapshot:',len(manifest))
