#!/usr/bin/env python3
"""Read-only exact canonical owner snapshots for the older worker base only."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
ROOT=Path(__file__).resolve().parents[1]
CANONICAL='48f2c32fe2a625b7af0a40c8654d013f875f97a2'
INVENTORY='c02b69c227951a9d1961170385176b443237e863'

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--revision',default=CANONICAL)
    revision=parser.parse_args().revision
    target=ROOT/'work/quest-start-items-actor-owners';manifest={}
    files={INVENTORY:['include/eternity/game/actor.hpp','src/game/inventory.cpp'],revision:['include/eternity/game/item_ids.hpp','src/game/item_ids.cpp','include/eternity/game/quest_runtime.hpp','src/game/quest_runtime.cpp']}
    for ref,names in files.items():
        for name in names:
            data=subprocess.check_output(['git','show',f'{ref}:{name}'],cwd=ROOT)
            path=target/name;path.parent.mkdir(parents=True,exist_ok=True)
            if path.exists():path.chmod(0o600)
            path.write_bytes(data);path.chmod(0o444)
            manifest[name]={'commit':ref,'sha256':hashlib.sha256(data).hexdigest()}
    (target/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Exact read-only owner snapshot files:',len(manifest),'main revision:',revision)
if __name__=='__main__':main()
