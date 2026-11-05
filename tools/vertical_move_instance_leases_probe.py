#!/usr/bin/env python3
"""Check a real transfer lease against the coordinator instance shutdown path."""
from pathlib import Path
import shutil
import subprocess
import uuid
from parallel_lock import resource_lock

root=Path(__file__).resolve().parents[1]
coordinator=Path(__file__).resolve().parents[1]
out=root/'.local/movement-instance-leases'
with resource_lock(root,'build',2):
    out.mkdir(parents=True,exist_ok=True)
    shutil.copytree(coordinator/'include',out/'include',dirs_exist_ok=True)
    shutil.copy2(root/'include/eternity/game/vertical_move_zone_leases.hpp',out/'include/eternity/game/vertical_move_zone_leases.hpp')
    for header in (coordinator/'tests').glob('*.hpp'):shutil.copy2(header,out/header.name)
    text=(coordinator/'tests/vertical_world_unit.cpp').read_text()
    text=text.replace('#include "eternity/game/mob_primary_zones.hpp"','#include "eternity/game/mob_primary_zones.hpp"\n#include "eternity/game/vertical_move_zone_leases.hpp"',1)
    anchor='            auto held_during_drain=authority23->lease_instance(*selected);check(bool(held_during_drain));'
    if text.count(anchor)!=1:raise RuntimeError('instance shutdown fixture anchor changed')
    text=text.replace(anchor,'''            auto provider23=std::make_shared<int>(1);std::weak_ptr<void> weak_provider23=provider23;
            auto held_during_drain=VerticalMoveZoneLeases::create(authority23,*selected,*selected,provider23);provider23.reset();
            held_during_drain->validate(*selected,*selected);check(!weak_provider23.expired());''',1)
    anchor2='check(!world23->poll_shutdown(183200) && world23->world().running() && authority23->instance_count()==1);held_during_drain.reset();'
    if text.count(anchor2)!=1:raise RuntimeError('instance lease release anchor changed')
    text=text.replace(anchor2,anchor2+'check(weak_provider23.expired());',1)
    (out/'probe.cpp').write_text(text)
    shutil.copy2(coordinator/'src/game/mob_primary_zones.cpp',out/'mob_primary_zones.cpp')
    for name in ('world','game','storage','session','net','quest_start_items_actor'):
        shutil.copy2(coordinator/'build'/f'libeternity_{name}.a',out/f'libeternity_{name}.a')
    base='/src/.local/movement-instance-leases'
    cmd=['c++','-std=c++20','-DASIO_STANDALONE','-I'+base+'/include','-I'+base,'-pthread',base+'/probe.cpp',base+'/mob_primary_zones.cpp','-Wl,--start-group']+[base+f'/libeternity_{name}.a' for name in ('world','game','storage','session','net','quest_start_items_actor')]+['-Wl,--end-group','-lodbc','-lcrypto','-o',base+'/probe']
    subprocess.run(['docker','run','--rm','-v',f'{root}:/src','-w','/src','eternity-dev:bookworm',*cmd],check=True)
    name='eternity-movement-instance-leases-'+uuid.uuid4().hex
    try:subprocess.run(['docker','run','--rm','--name',name,'-v',f'{root}:/src','-w','/src','eternity-dev:bookworm',base+'/probe',base+'/maps'],check=True,timeout=120)
    except subprocess.TimeoutExpired:
        subprocess.run(['docker','rm','-f',name],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        raise
