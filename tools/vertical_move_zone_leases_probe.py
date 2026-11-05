#!/usr/bin/env python3
"""Exercise transfer zone leases against the coordinator's actual zone authority."""
from pathlib import Path
import shutil
import subprocess
import uuid
from parallel_lock import resource_lock

root = Path(__file__).resolve().parents[1]
coordinator = Path('/home/franco/Projects/Eternity-foundation')
out = root / '.local/movement-zone-leases'
with resource_lock(root, 'build', 2):
    out.mkdir(parents=True, exist_ok=True)
    shutil.copytree(coordinator / 'include', out / 'include', dirs_exist_ok=True)
    shutil.copy2(root / 'include/eternity/game/vertical_move_zone_leases.hpp', out / 'include/eternity/game/vertical_move_zone_leases.hpp')
    for header in (coordinator / 'tests').glob('*.hpp'):
        shutil.copy2(header, out / header.name)
    text = (coordinator / 'tests/mob_primary_zones_unit.cpp').read_text()
    text = text.replace('#include "eternity/game/mob_primary_zones.hpp"', '#include "eternity/game/mob_primary_zones.hpp"\n#include "eternity/game/vertical_move_zone_leases.hpp"', 1)
    anchor = '    auto lease=root->lease(0);auto generation=lease.generation();'
    if text.count(anchor)!=1: raise RuntimeError('zone lease fixture anchor changed')
    insert = '''    RuntimeZone *first_zone=nullptr,*second_zone=nullptr;auto first_pin=root->lease(0),second_pin=root->lease(1);
    first_pin.visit([&](RuntimeZone& zone){first_zone=&zone;});second_pin.visit([&](RuntimeZone& zone){second_zone=&zone;});first_pin.reset();second_pin.reset();
    auto providers=std::make_shared<int>(7);std::weak_ptr<void> weak_providers=providers;
    auto transfer_lease=VerticalMoveZoneLeases::create(root,*first_zone,*second_zone,providers);providers.reset();
    transfer_lease->validate(*first_zone,*second_zone);check(!weak_providers.expired());
    bool rejected_identity=false;try{transfer_lease->validate(*second_zone,*first_zone);}catch(const std::logic_error&){rejected_identity=true;}check(rejected_identity);
    auto same=VerticalMoveZoneLeases::create(root,*first_zone,*first_zone,std::make_shared<int>(9));same->validate(*first_zone,*first_zone);same.reset();
    ConfiguredMap foreign_map;RuntimeZone foreign_zone;foreign_zone.map=&foreign_map;bool rejected_foreign=false;
    try{(void)VerticalMoveZoneLeases::create(root,*first_zone,foreign_zone,std::make_shared<int>(1));}catch(const std::logic_error&){rejected_foreign=true;}check(rejected_foreign);
'''
    text=text.replace(anchor,insert+anchor,1)
    anchor2='    if(scenario&2){frame->backend.fail_release=true;'
    if text.count(anchor2)!=1: raise RuntimeError('zone drain fixture anchor changed')
    text=text.replace(anchor2,'    check(root->drain(0,clock)==MobPrimaryZoneClose::deferred && !weak_providers.expired());transfer_lease.reset();check(weak_providers.expired());\n'+anchor2,1)
    (out/'probe.cpp').write_text(text)
    for name in ('game','storage','session','net'):
        shutil.copy2(coordinator/'build'/f'libeternity_{name}.a',out/f'libeternity_{name}.a')
    shutil.copy2(coordinator/'src/game/mob_primary_zones.cpp',out/'mob_primary_zones.cpp')
    base='/src/.local/movement-zone-leases'
    cmd=['c++','-std=c++20','-DASIO_STANDALONE','-I'+base+'/include','-I'+base,'-pthread',base+'/probe.cpp',base+'/mob_primary_zones.cpp',base+'/libeternity_game.a',base+'/libeternity_storage.a',base+'/libeternity_session.a',base+'/libeternity_net.a','-lodbc','-lcrypto','-o',base+'/probe']
    subprocess.run(['docker','run','--rm','-v',f'{root}:/src','-w','/src','eternity-dev:bookworm',*cmd],check=True)
    name='eternity-movement-zone-leases-'+uuid.uuid4().hex
    try:
        subprocess.run(['docker','run','--rm','--name',name,'-v',f'{root}:/src','-w','/src','eternity-dev:bookworm',base+'/probe',base+'/maps'],check=True,timeout=120)
    except subprocess.TimeoutExpired:
        subprocess.run(['docker','rm','-f',name],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        raise
