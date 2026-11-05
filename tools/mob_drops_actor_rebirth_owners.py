#!/usr/bin/env python3
"""Snapshot the read-only canonical worker08 bridge for older worker bases."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
MAIN=Path(__file__).resolve().parents[1]
def main():
    manifest={};target=ROOT/'work/mob-drops-actor-rebirth-owners'
    for name in ('include/eternity/game/mob_targeting_lifecycle.hpp','src/game/mob_targeting_lifecycle.cpp'):
        data=(MAIN/name).read_bytes();p=target/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);manifest[name]=hashlib.sha256(data).hexdigest()
    (ROOT/'work/mob-drops-actor-rebirth-owners.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Read-only retained Rebirth bridge snapshot:',len(manifest))
if __name__=='__main__':main()
