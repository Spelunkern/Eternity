#!/usr/bin/env python3
"""Read-only coherent canonical-archive checks for lifecycle Actor API bindings.

All copies/executables/manifests stay in ignored work/. No canonical source,
SQL state or original binary is written. Fails if source/archive moves during
the snapshot; does not retry or use a second runtime owner.
"""
from pathlib import Path
import argparse,hashlib,json,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from parallel_lock import resource_lock
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--canonical',type=Path,required=True,help='Read-only canonical checkout with completed matching build archives')
parser.add_argument('--suite',choices=['ordinary','death','lose'],action='append',help='Run only selected suites; default is ordinary/death')
args=parser.parse_args()
root=ROOT;main=args.canonical.resolve();out=root/'work/mob-targeting-lifecycle-canonical-check'
with resource_lock(root,'build',2):
    source=subprocess.check_output(['git','-C',str(main),'rev-parse','HEAD'],text=True).strip()
    out.mkdir(parents=True,exist_ok=True)
    shutil.copytree(main/'include',out/'include',dirs_exist_ok=True)
    overlay=out/'overlay/eternity/game';overlay.mkdir(parents=True,exist_ok=True)
    for h in (root/'include/eternity/game').glob('mob_targeting_lifecycle*.hpp'):shutil.copy2(h,overlay/h.name)
    manifest={'main_source':source,'libraries':{}}
    for name in ['libeternity_game.a','libeternity_storage.a']:
        src=main/'build'/name
        before=hashlib.sha256(src.read_bytes()).hexdigest();shutil.copy2(src,out/name)
        after=hashlib.sha256(src.read_bytes()).hexdigest();copied=hashlib.sha256((out/name).read_bytes()).hexdigest()
        if not before==after==copied:raise RuntimeError('Canonical archive changed during read-only snapshot; validation not run')
        manifest['libraries'][name]=before
    if source!=subprocess.check_output(['git','-C',str(main),'rev-parse','HEAD'],text=True).strip():raise RuntimeError('Canonical source changed during read-only evidence snapshot')
    (out/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Read-only canonical evidence source',source,flush=True)
    relative=out.relative_to(root)
    base=['docker','run','--rm','--mount',f'type=bind,source={root},target=/src','eternity-dev:bookworm']
    probe=out/'actor-api-probe.cpp'
    probe.write_text('#include "eternity/game/actor.hpp"\nusing namespace eternity::game;\nstatic_assert(requires(Actor& a,PlayerResources p) {a.replace_resources_raw(p);a.reset_damage_ledger();a.presentation_motion();a.effect_database_bound();});\n')
    subprocess.run(base+['g++','-std=c++20','-fsyntax-only','-I'+str(relative/'include'),str(relative/'actor-api-probe.cpp')],check=True)
    for stem in (args.suite or ['ordinary','death']):
        data=stem
        exe=str(relative/('mob-targeting-lifecycle-'+stem+'-unit'))
        sources=['src/game/mob_targeting_lifecycle_lose.cpp'] if stem=='lose' else ['src/game/mob_targeting_lifecycle_death.cpp']
        if stem=='ordinary':sources+=['src/game/mob_targeting_lifecycle_ordinary.cpp']
        subprocess.run(base+['g++','-std=c++20','-g','-O0','-fno-pie','-no-pie','-I'+str(relative/'overlay'),'-I'+str(relative/'include'),f'tests/mob_targeting_lifecycle_{stem}_unit.cpp',*sources,str(relative/'libeternity_game.a'),str(relative/'libeternity_storage.a'),'-lodbc','-o',exe],cwd=root,check=True)
        subprocess.run(base+[exe,f'tests/data/mob-targeting-lifecycle-{data}-oracle.tsv'],cwd=root,check=True)
