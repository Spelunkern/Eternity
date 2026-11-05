#!/usr/bin/env python3
"""Compile the coordinator session owner with the movement disconnect fence."""
from pathlib import Path
import shutil
import subprocess
import time
import uuid
from parallel_lock import resource_lock

root=Path(__file__).resolve().parents[1]
coordinator=Path('/home/franco/Projects/Eternity-foundation')
out=root/'.local/movement-session-owner'
with resource_lock(root,'build',2):
    out.mkdir(parents=True,exist_ok=True)
    shutil.copytree(coordinator/'include',out/'include',dirs_exist_ok=True)
    for header in (root/'include/eternity/game').glob('vertical_move*.hpp'):shutil.copy2(header,out/'include/eternity/game'/header.name)
    for name in ('vertical_world_session_owner.hpp','vertical_world_session_runtime.hpp','vertical_world_session_packets.hpp','vertical_world_session_packet_router.hpp','world_admission.hpp'):
        shutil.copy2(root/'include/eternity/game'/name,out/'include/eternity/game'/name)
    quest=out/'include/eternity/game/vertical_quests.hpp';text=quest.read_text();anchor='    void synchronize_interaction();'
    if text.count(anchor)!=1:raise RuntimeError('quest accessor anchor changed')
    if 'bool uses_interaction(' not in text:quest.write_text(text.replace(anchor,'    bool uses_interaction(const VerticalNpcInteraction& value) const noexcept {return &interaction_==&value;}\n'+anchor,1))
    for header in (coordinator/'tests').glob('*.hpp'):shutil.copy2(header,out/header.name)
    shutil.copy2(coordinator/'tests/vertical_protocol_gate_unit.cpp',out/'vertical_protocol_gate_unit.cpp')
    (out/'data').mkdir(exist_ok=True);shutil.copy2(coordinator/'tests/data/quests-oracle.tsv',out/'data/quests-oracle.tsv')
    source=(coordinator/'tests/vertical_session_registry_unit.cpp').read_text()
    anchor='  auto root_bindings=ComposedOwner::root_bindings(composed_owner);'
    if source.count(anchor)!=1:raise RuntimeError('composed owner anchor changed')
    source=source.replace(anchor,'  composed_owner->install_transfer_directory();check(composed_owner->transfers().size()==0,"installed transfer directory not empty");bool duplicate_transfer_install=false;try{composed_owner->install_transfer_directory();}catch(const std::logic_error&){duplicate_transfer_install=true;}check(duplicate_transfer_install,"duplicate transfer directory accepted");\n'+anchor,1)
    runtime_anchor='auto foreign_runtime_ports=runtime_ports;TestRuntime runtime(strand,sessions,2,64,std::move(runtime_ports));'
    if source.count(runtime_anchor)!=1:raise RuntimeError('runtime fence anchor changed')
    source=source.replace(runtime_anchor,'''auto foreign_runtime_ports=runtime_ports;unsigned outer_fence_completed=0;
 runtime_ports.disconnect_fence=[&](VerticalWorldSession& owner,std::optional<std::uint32_t> reason,std::function<void()> done,TestRuntime::Ports::DisconnectService actual){
  auto retained=owner.shared_from_this();actual(owner,reason,[&,retained,done=std::move(done)]() mutable {check(sessions.pending_database_work(retained->account_id())==0,"outer fence completed before database drain");++outer_fence_completed;done();});};
 TestRuntime runtime(strand,sessions,2,64,std::move(runtime_ports));''',1)
    finish_anchor='runtime_cleanup_future.get();runtime_dispatcher.shutdown();'
    if source.count(finish_anchor)!=1:raise RuntimeError('runtime cleanup anchor changed')
    source=source.replace(finish_anchor,'runtime_cleanup_future.get();check(outer_fence_completed>=2,"runtime disconnect did not complete outer fences");runtime_dispatcher.shutdown();',1)
    (out/'probe.cpp').write_text(source)
    for name,path in (('registry.cpp','src/session/registry.cpp'),('quests.cpp','src/game/vertical_quests.cpp')):shutil.copy2(coordinator/path,out/name)
    shutil.copy2(root/'src/game/world_admission.cpp',out/'world_admission.cpp')
    for name in ('world','game','storage','session','net','quest_start_items_actor'):
        archive=coordinator/'build'/f'libeternity_{name}.a';snapshot=out/archive.name
        for attempt in range(60):
            before=archive.stat()
            if before.st_size>1024:
                shutil.copy2(archive,snapshot);after=archive.stat()
                if (before.st_ino,before.st_size,before.st_mtime_ns)==(after.st_ino,after.st_size,after.st_mtime_ns):break
            time.sleep(1)
        else:raise RuntimeError(f'coordinator archive remained unstable: {name}')
    base='/src/.local/movement-session-owner'
    cmd=['c++','-std=c++20','-DASIO_STANDALONE','-I'+base+'/include','-I'+base,'-pthread',base+'/probe.cpp',base+'/registry.cpp',base+'/quests.cpp',base+'/world_admission.cpp','-Wl,--start-group']+[base+f'/libeternity_{name}.a' for name in ('world','game','storage','session','net','quest_start_items_actor')]+['-Wl,--end-group','-lodbc','-lcrypto','-o',base+'/probe']
    compiler='eternity-movement-owner-compile-'+uuid.uuid4().hex
    try:subprocess.run(['docker','run','--rm','--name',compiler,'-v',f'{root}:/src','-w','/src','eternity-dev:bookworm',*cmd],check=True)
    except BaseException:
        subprocess.run(['docker','rm','-f',compiler],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);raise
    name='eternity-movement-owner-'+uuid.uuid4().hex
    try:subprocess.run(['docker','run','--rm','--name',name,'-v',f'{root}:/src','-w','/src','eternity-dev:bookworm',base+'/probe'],check=True,timeout=120)
    except subprocess.TimeoutExpired:
        subprocess.run(['docker','rm','-f',name],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL);raise
