#!/usr/bin/env python3
"""Read-only pinned Actor dependency snapshot plus existing canonical quest owners."""
import hashlib
import json
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
out=root/'work/quest-requests-actor-owners'
commit='c02b69c227951a9d1961170385176b443237e863'
manifest={}
for path in ('include/eternity/game/actor.hpp','src/game/inventory.cpp'):
    data=subprocess.check_output(['git','show',f'{commit}:{path}'],cwd=root)
    target=out/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
    manifest[path]={'commit':commit,'sha256':hashlib.sha256(data).hexdigest()}
for stem in ('quest_runtime','quest_rewards'):
    for path in (f'include/eternity/game/{stem}.hpp',f'src/game/{stem}.cpp'):
        data=(Path(__file__).resolve().parents[1]/path).read_bytes()
        target=out/path;target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data)
        manifest[path]={'source':'coordinator read-only','sha256':hashlib.sha256(data).hexdigest()}
(out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(manifest,indent=2))
