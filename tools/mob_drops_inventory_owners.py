#!/usr/bin/env python3
"""Read-only quest evidence snapshots for worker bases predating canonical owners."""
from pathlib import Path
import hashlib
import json
ROOT=Path(__file__).resolve().parents[1]
MAIN=Path('/home/franco/Projects/Eternity')
FILES=['include/eternity/game/quest_requests.hpp','src/game/quest_requests.cpp',
       'include/eternity/game/quest_start_items.hpp','src/game/quest_start_items.cpp',
       'tests/data/quest-requests-flow-oracle.tsv','tests/data/quest-start-items-oracle.tsv']
def main():
    target=ROOT/'work/mob-drops-inventory-owners';manifest={}
    for name in FILES:
        data=(MAIN/name).read_bytes();path=target/name;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        manifest[name]=hashlib.sha256(data).hexdigest()
    (ROOT/'work/mob-drops-inventory-owners.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Read-only canonical quest source/fixture snapshots:',len(FILES))
if __name__=='__main__':main()
