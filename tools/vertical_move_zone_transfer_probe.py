#!/usr/bin/env python3
"""Run the coordinator's real quest/session fixture with movement zone transfer."""
from pathlib import Path
import os
import shutil
import subprocess
import uuid
from parallel_lock import resource_lock

root = Path(__file__).resolve().parents[1]
coordinator = Path('/home/franco/Projects/Eternity-foundation')
real_zones = os.environ.get('ETERNITY_MOVEMENT_REAL_ZONES') == '1'
disconnect_probe = os.environ.get('ETERNITY_MOVEMENT_DISCONNECT') == '1'
if disconnect_probe and not real_zones: raise RuntimeError('disconnect probe requires real zone authority')
out = root / ('.local/movement-zone-transfer-disconnect' if disconnect_probe else '.local/movement-zone-transfer-authority' if real_zones else '.local/movement-zone-transfer')
base = '/src/.local/movement-zone-transfer-disconnect' if disconnect_probe else '/src/.local/movement-zone-transfer-authority' if real_zones else '/src/.local/movement-zone-transfer'

pool = r'''
struct TransferProbePool final:PartyMembershipSendWorld {
 PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t) override {throw std::logic_error("unexpected allocation");}
 void free_send_buffer(PartyMembershipSendBuffer&) override {throw std::logic_error("unexpected free");}
 void enqueue_send(PartyMembershipUser&,PartyMembershipSendBuffer&) override {throw std::logic_error("unexpected enqueue");}
 std::uint32_t tick() override {throw std::logic_error("unexpected tick");}
 void lock(PartyNormalMembership&) override {throw std::logic_error("unexpected lock");}
 void unlock(PartyNormalMembership&) noexcept override {std::terminate();}
 void lock_effects(PartyMembershipUser&) override {throw std::logic_error("unexpected effects lock");}
 void unlock_effects(PartyMembershipUser&) noexcept override {std::terminate();}
 void send_user(PartyMembershipUser&,const eternity::net::Packet&) override {throw std::logic_error("unexpected send");}
 void send_scene(PartyMembershipUser&,const eternity::net::Packet&) override {throw std::logic_error("unexpected scene");}
 void unregister_party(PartyNormalMembership&) override {throw std::logic_error("unexpected unregister");}
 void cleanup_guild_creation(PartyMembershipUser&) override {throw std::logic_error("unexpected guild cleanup");}
 void fill_log(PartyMembershipUser&,std::span<std::uint8_t>) override {throw std::logic_error("unexpected log");}
 void send_log(const eternity::net::Packet&) override {throw std::logic_error("unexpected log send");}
};
'''

transfer = r'''
            auto& transfer_entry=owner.zone_entry();transfer_entry.character_id=world_actor->id();transfer_entry.bound=true;
            admission.untrack(owner.admission()); // The upstream fixture projects location 4 directly.
            transfer_entry.map=static_cast<std::uint16_t>(movement_source_map.settings.id);transfer_entry.cell={0,0};transfer_entry.position={};
            queued(net::Packet{4,14,99,0,0,0});
            check(movement_resources->npc().npc==world_npc && world_npc->users_using==1,"transfer did not start with the real NPC interaction");
            NpcLeaveServices leave_services(*world_actor,movement_resources->npc(),*world_npc,0);
            bool suspended=false,resumed=false;std::vector<std::string> transfer_steps;
            using Transfer=VerticalMoveZoneTransfer<VerticalWorldSession>;
            auto transfer_ports=vertical_move_world_transfer_ports(world_registry,*packet_router,admission,
              [&](VerticalWorldSession& selected,RuntimeZone& zone){
                check(suspended && &selected==&owner && &zone==&world_zone,"wrong source owner");transfer_steps.push_back("leave");
                ZoneActorLeaveServices services;
                services.user_leave_lifecycle=[&](Actor& actor){check(&actor==world_actor.get(),"wrong leaving actor");pinned->packets().leave_interactions(movement_shop,leave_services);};
                services.send_view=[&](CellCoordinates,const net::Packet& packet){check(packet.size()==6 && packet[0]==2 && packet[1]==2,"wrong leave view packet");};
                return services;
              },[&](VerticalWorldSession& selected,RuntimeZone& zone){
                check(suspended && &selected==&owner && &zone==&destination_zone,"wrong destination owner");transfer_steps.push_back("queue");
                selected.admission().map=static_cast<std::uint16_t>(zone.map->settings.id);
              });
            auto actual_suspend=transfer_ports.suspend;transfer_ports.suspend=[&](VerticalWorldSession& selected){check(&selected==&owner && !suspended,"wrong suspend owner");actual_suspend(selected);suspended=true;transfer_steps.push_back("suspend");};
            auto actual_invalidate=transfer_ports.invalidate;transfer_ports.invalidate=[&](VerticalWorldSession& selected){check(suspended && &selected==&owner,"wrong invalidate owner");actual_invalidate(selected);transfer_steps.push_back("invalidate");};
            auto admit_destination=[&]{
                admission_services.transfer_select=[&](WorldAdmissionUser& user){check(&user==&owner.admission(),"wrong selected user");return &destination_zone;};
                admission_services.transfer_enter=[&](WorldAdmissionUser& user,RuntimeZone& zone){
                    check(&user==&owner.admission() && &zone==&destination_zone,"wrong entering user");destination_zone.users->attach(*world_actor,0,0,{});
                    owner.zone_entry().map=static_cast<std::uint16_t>(movement_destination_map.settings.id);owner.zone_entry().cell={0,0};owner.zone_entry().position={};owner.zone_entry().bound=true;return true;
                };
                admission.update_packets();check(owner.admission().location==3 && admission.zone_waiting_count(destination_zone)==1,"transfer did not reach actual zone wait");
                check(admission.world_enter(owner.admission()),"destination world admission failed");admission_services.transfer_select={};admission_services.transfer_enter={};
            };
            auto actual_rebind=transfer_ports.rebind_quests;transfer_ports.rebind_quests=[&](VerticalWorldSession& selected,RuntimeZone& old_zone,RuntimeZone& new_zone){
                check(suspended,"quest rebind escaped suspension");transfer_steps.push_back("rebind");actual_rebind(selected,old_zone,new_zone);
            };
            auto actual_resume=transfer_ports.resume;transfer_ports.resume=[&](VerticalWorldSession& selected){check(suspended && &selected==&owner,"wrong resume owner");actual_resume(selected);transfer_steps.push_back("resume");suspended=false;resumed=true;};
            VerticalNpcInteraction foreign_interaction;
            check(transfer_ports.quests_bound(owner,world_zone,movement_resources->npc()) && !transfer_ports.quests_bound(owner,world_zone,foreign_interaction) &&
                !transfer_ports.quests_bound(owner,destination_zone,movement_resources->npc()),"quest binding accepted foreign interaction or zone");
            transfer_entry.cell.x=1;Transfer wrong_entry(world_registry,owner.shared_from_this(),movement_lease,movement_resources,transfer_ports);
            bool entry_rejected=false;try{wrong_entry.begin(world_zone,destination_zone);}catch(const std::logic_error&){entry_rejected=true;}
            check(entry_rejected && !suspended && transfer_steps.empty(),"stale source cell was accepted");transfer_entry.cell.x=0;
            auto failing_ports=transfer_ports;failing_ports.leave=[&](VerticalWorldSession&,RuntimeZone&){transfer_steps.push_back("failed-leave");throw std::runtime_error("fixture leave failure");};
            Transfer failing(world_registry,owner.shared_from_this(),movement_lease,movement_resources,std::move(failing_ports));
            bool failed=false;try{failing.begin(world_zone,destination_zone);}catch(const std::runtime_error&){failed=true;}
            check(failed && suspended && !resumed && world_zone.users->find_actor(world_actor->id())==world_actor.get() && movement_resources->npc().npc==world_npc,
                "failed transfer resumed or mutated the retained source");
            packet_router->resume_gameplay(owner);suspended=false;transfer_steps.clear();
            auto failed_rebind_ports=transfer_ports;failed_rebind_ports.rebind_quests=[&](VerticalWorldSession&,RuntimeZone&,RuntimeZone&){throw std::runtime_error("fixture rebind failure");};
            Transfer failed_rebind(world_registry,owner.shared_from_this(),movement_lease,movement_resources,std::move(failed_rebind_ports));
            failed_rebind.begin(world_zone,destination_zone);admit_destination();
            bool rebind_failed=false;try{failed_rebind.complete();}catch(const std::runtime_error&){rebind_failed=true;}
            check(rebind_failed && suspended && !resumed && destination_zone.users->find_actor(world_actor->id())==world_actor.get() &&
                !world_zone.users->find_actor(world_actor->id()) && !movement_resources->npc().npc && world_npc->users_using==0 && pinned->quests().owns_npc_zone(*world_zone.npcs),
                "failed quest rebind resumed or lost its retained owners");
            check(destination_zone.users->detach(world_actor->id()),"fixture recovery destination detach failed");world_zone.users->attach(*world_actor,0,0,{});
            owner.admission().zone=&world_zone;transfer_entry.map=static_cast<std::uint16_t>(movement_source_map.settings.id);transfer_entry.cell={0,0};transfer_entry.position={};transfer_entry.bound=true;
            packet_router->resume_gameplay(owner);suspended=false;transfer_steps.clear();queued(net::Packet{4,14,99,0,0,0});check(movement_resources->npc().npc==world_npc && world_npc->users_using==1,"fixture recovery did not reacquire the source NPC");
            auto* retained_quest=&pinned->quests();Transfer actual(world_registry,owner.shared_from_this(),movement_lease,movement_resources,std::move(transfer_ports));
            actual.begin(world_zone,destination_zone);
            check(suspended && !resumed && owner.admission().location==2 && !owner.admission().zone && pinned->quests().owns_npc_zone(*world_zone.npcs),"transfer did not remain fenced during wait");
            check(packet_router->try_enqueue(owner,net::Packet{8,9,255,255}),"suspended transfer FIFO rejected packet");
            admit_destination();packet_router->update(owner);
            check(packet_router->pending(owner)==1 && packet_router->gameplay_suspended(owner) && pinned->quests().owns_npc_zone(*world_zone.npcs),"gameplay escaped before quest rebinding");
            actual.complete();packet_router->update(owner);check(packet_router->pending(owner)==0,"rebinding did not release queued gameplay");
            check(resumed && !suspended && transfer_steps==std::vector<std::string>({"suspend","invalidate","leave","queue","rebind","resume"}) &&
                leave_services.shutdowns==2 && !movement_resources->npc().npc && world_npc->users_using==0,"transfer order or original NPC cleanup differed");
            bool duplicate_transfer=false;try{actual.begin(world_zone,destination_zone);}catch(const std::logic_error&){duplicate_transfer=true;}check(duplicate_transfer,"transfer was reused");
            queued(net::Packet{4,14,102,0,0,0});check(movement_resources->npc().npc==destination_npc && destination_npc->users_using==1,"same-zone fixture did not acquire NPC");
            NpcLeaveServices same_leave(*world_actor,movement_resources->npc(),*destination_npc,0);
            auto same_ports=vertical_move_world_transfer_ports(world_registry,*packet_router,admission,
                [&](VerticalWorldSession& selected,RuntimeZone& zone){
                    check(&selected==&owner && &zone==&destination_zone,"same-zone leave used wrong owner");ZoneActorLeaveServices services;
                    services.user_leave_lifecycle=[&](Actor& actor){check(&actor==world_actor.get(),"wrong same-zone actor");pinned->packets().leave_interactions(movement_shop,same_leave);};
                    services.send_view=[&](CellCoordinates,const net::Packet& packet){check(packet.size()==6 && packet[0]==2 && packet[1]==2,"wrong same-zone leave packet");};return services;
                },[&](VerticalWorldSession& selected,RuntimeZone& zone){check(&selected==&owner && &zone==&destination_zone,"same-zone destination changed");selected.admission().map=static_cast<std::uint16_t>(zone.map->settings.id);});
            Transfer same_zone(world_registry,owner.shared_from_this(),movement_lease,movement_resources,std::move(same_ports));
            same_zone.begin(destination_zone,destination_zone);
            check(owner.admission().location==2 && !destination_zone.users->find_actor(world_actor->id()) && !movement_resources->npc().npc && destination_npc->users_using==0 && packet_router->gameplay_suspended(owner),"same-zone leave did not enter suspended wait");
            admit_destination();same_zone.complete();
            check(destination_zone.users->find_actor(world_actor->id())==world_actor.get() && pinned->quests().owns_npc_zone(*destination_zone.npcs) && !packet_router->gameplay_suspended(owner),"same-zone re-entry lost owners or remained suspended");
'''

with resource_lock(root, 'build', 2):
    out.mkdir(parents=True, exist_ok=True)
    shutil.copytree(coordinator / 'include', out / 'include', dirs_exist_ok=True)
    for header in (root / 'include/eternity/game').glob('vertical_move*.hpp'):
        shutil.copy2(header, out / 'include/eternity/game' / header.name)
    for name in ('vertical_world_session_packets.hpp','vertical_world_session_packet_router.hpp','vertical_world_session_runtime.hpp'):
        shutil.copy2(root / 'include/eternity/game' / name, out / 'include/eternity/game' / name)
    shutil.copy2(root / 'include/eternity/game/world_admission.hpp', out / 'include/eternity/game/world_admission.hpp')
    quest_header = out / 'include/eternity/game/vertical_quests.hpp'
    quest_text = quest_header.read_text()
    anchor = '    void synchronize_interaction();'
    if quest_text.count(anchor) != 1:
        raise RuntimeError('quest interaction accessor anchor changed')
    quest_header.write_text(quest_text.replace(anchor, '    bool uses_interaction(const VerticalNpcInteraction& value) const noexcept {return &interaction_==&value;}\n'+anchor, 1))
    for header in (coordinator / 'tests').glob('*.hpp'):
        shutil.copy2(header, out / header.name)
    source = (coordinator / 'tests/vertical_quests_unit.cpp').read_text()
    select = 'RuntimeZone* select_zone(WorldAdmissionUser&) override {unexpected();}'
    enter = 'bool enter_zone(WorldAdmissionUser&,RuntimeZone&) override {unexpected();}'
    if source.count(select)!=1 or source.count(enter)!=1: raise RuntimeError('admission fixture changed')
    source = source.replace(select, 'std::function<RuntimeZone*(WorldAdmissionUser&)> transfer_select;RuntimeZone* select_zone(WorldAdmissionUser& user) override {if(!transfer_select)unexpected();return transfer_select(user);}', 1)
    source = source.replace(enter, 'std::function<bool(WorldAdmissionUser&,RuntimeZone&)> transfer_enter;bool enter_zone(WorldAdmissionUser& user,RuntimeZone& zone) override {if(!transfer_enter)unexpected();return transfer_enter(user,zone);}', 1)
    source = source.replace('#include "eternity/game/vertical_quests_world_session.hpp"', '#include "eternity/game/vertical_quests_world_session.hpp"\n#include "eternity/game/vertical_move_zone_transfer_world.hpp"\n#include "eternity/game/vertical_move_retained_world_transfer.hpp"', 1)
    if real_zones:
        authority_fixture = (coordinator / 'tests/mob_primary_zones_unit.cpp').read_text()
        start = authority_fixture.index('namespace {') + len('namespace {')
        end = authority_fixture.index('\n}\nint main(', start)
        source = source.replace('using namespace quest_actor_test;', '#include "eternity/game/mob_applied_skills_runtime.hpp"\n#include <source_location>\n#include <fstream>\nnamespace authority_fixture {using namespace eternity::game;'+authority_fixture[start:end]+'\n}\nusing namespace quest_actor_test;', 1)
        old = 'MapGeometry pickup_geometry(81);RuntimeZone world_zone;world_zone.users=std::make_unique<ZoneUsers>(1,1,0);world_zone.npcs=std::make_unique<ZoneNpcs>(pickup_geometry,npc_services);'
        if source.count(old)!=1:raise RuntimeError('source zone fixture changed')
        source = source.replace(old, '''MapGeometry pickup_geometry(81);authority_fixture::write_maps("'''+base+'''/maps");
        auto movement_config=MapConfiguration::decode("[BASE]\\nMapCount=2\\n");auto movement_maps=std::make_shared<ServerMaps>(ServerMaps::load(movement_config,"'''+base+'''/maps"));
        auto movement_frame=std::make_shared<authority_fixture::Frame>(movement_maps);auto movement_authority=authority_fixture::root_for(movement_frame);check(movement_authority->initialize(),"actual zone authority initialization failed");
        RuntimeZone *movement_source=nullptr,*movement_destination=nullptr;auto source_pin=movement_authority->lease(0),destination_pin=movement_authority->lease(1);
        source_pin.visit([&](RuntimeZone& zone){movement_source=&zone;});destination_pin.visit([&](RuntimeZone& zone){movement_destination=&zone;});source_pin.reset();destination_pin.reset();
        auto& world_zone=*movement_source;world_zone.users=std::make_unique<ZoneUsers>(1,1,0);world_zone.npcs=std::make_unique<ZoneNpcs>(pickup_geometry,npc_services);''',1)
    source = source.replace('struct PoolFactory final:', pool + '\nstruct PoolFactory final:', 1)
    zone_anchor = 'RuntimeZone destination_zone;destination_zone.users='
    if source.count(zone_anchor) != 1:
        raise RuntimeError('quest zone declaration changed')
    if real_zones:
        source = source.replace(zone_anchor, 'auto& movement_source_map=*world_zone.map;auto& movement_destination_map=*movement_destination->map;auto& destination_zone=*movement_destination;destination_zone.users=', 1)
    else:
        source = source.replace(zone_anchor, 'ConfiguredMap movement_source_map,movement_destination_map;movement_source_map.settings.id=20;movement_destination_map.settings.id=21;world_zone.map=&movement_source_map;RuntimeZone destination_zone;destination_zone.map=&movement_destination_map;destination_zone.users=', 1)
    declaration = 'PlayerCore world_player;VerticalNpcInteraction world_interaction;'
    if source.count(declaration) != 1:
        raise RuntimeError('quest interaction declaration changed')
    provider = 'auto movement_lease=VerticalMoveZoneLeases::create(movement_authority,world_zone,destination_zone,movement_frame);' if real_zones else 'auto movement_lease=std::make_shared<int>(1);'
    source = source.replace(declaration, 'PlayerCore world_player;TransferProbePool movement_pool;ZoneLeaveShopState movement_shop;'+provider+'std::shared_ptr<VerticalMoveSessionResources> movement_resources;', 1)
    source = source.replace('world_interaction', 'movement_resources->npc()')
    prepare = 'world_services.on_prepare=[&](VerticalWorldSession& owner){'
    if source.count(prepare) != 1:
        raise RuntimeError('quest preparation anchor changed')
    source = source.replace(prepare, prepare+'movement_resources=std::make_shared<VerticalMoveSessionResources>(movement_pool,owner.party_user(),movement_shop);', 1)
    begin = '            check(world_zone.users->detach(world_actor->id()),"world quest source Actor detach failed");'
    end = 'vertical_world_quests_rebind_zone(owner,world_zone,destination_zone,world_registry);'
    first = source.find(begin)
    last = source.find(end, first)
    if first < 0 or last < 0:
        raise RuntimeError('quest transfer anchor changed')
    transfer_body=transfer
    if real_zones:
        transfer_body=transfer_body.replace('Transfer actual(world_registry,owner.shared_from_this(),movement_lease,movement_resources,std::move(transfer_ports));','std::weak_ptr<VerticalMoveZoneLeases> weak_actual_lease;{auto actual_lease=VerticalMoveZoneLeases::create(movement_authority,world_zone,destination_zone,movement_frame);weak_actual_lease=actual_lease;VerticalMoveRetainedWorldTransfer actual(world_registry,owner.shared_from_this(),actual_lease,world_zone,destination_zone,movement_resources,std::move(transfer_ports));actual_lease.reset();')
        transfer_body=transfer_body.replace('actual.begin(world_zone,destination_zone)', 'actual.begin()')
        transfer_body=transfer_body.replace('actual.begin();\n            check(suspended', 'actual.begin();\n            check(!weak_actual_lease.expired() && movement_authority->drain(0,[]{return 100U;})==MobPrimaryZoneClose::deferred && world_zone.mobs,"source zone shutdown escaped transfer lease");\n            check(suspended', 1)
        transfer_body=transfer_body.replace('check(duplicate_transfer,"transfer was reused");','check(duplicate_transfer,"transfer was reused");}check(weak_actual_lease.expired(),"transfer retained its zone lease after destruction");',1)
        transfer_body=transfer_body.replace('Transfer same_zone(world_registry,owner.shared_from_this(),movement_lease,movement_resources,std::move(same_ports));','auto same_zone_lease=VerticalMoveZoneLeases::create(movement_authority,destination_zone,destination_zone,movement_frame);VerticalMoveRetainedWorldTransfer same_zone(world_registry,owner.shared_from_this(),same_zone_lease,destination_zone,destination_zone,movement_resources,std::move(same_ports));')
        transfer_body=transfer_body.replace('same_zone.begin(destination_zone,destination_zone)', 'same_zone.begin()')
        cleanup='npc_pool.release(*destination_npc);work.reset();database.join();worker.join();'
        if source.count(cleanup)!=1:raise RuntimeError('authority fixture cleanup changed')
        source=source.replace(cleanup,'npc_pool.release(*destination_npc);movement_lease.reset();check(movement_authority->drain_all([]{return 100U;}),"authority did not drain after transfer");work.reset();database.join();worker.join();',1)
        first = source.find(begin);last = source.find(end, first)
    source = source[:first] + transfer_body + source[last+len(end):]
    if disconnect_probe:
        source = source.replace('std::unique_ptr<SocialCatalog> catalog;bool unbound=false;', 'std::unique_ptr<SocialCatalog> catalog;std::function<void(VerticalWorldSession&)> disconnect_hook;bool unbound=false;', 1)
        old_disconnect='void disconnect(VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()> done) override {done();}'
        if source.count(old_disconnect)!=1:raise RuntimeError('successful session disconnect fixture changed')
        source=source.replace(old_disconnect, 'void disconnect(VerticalWorldSession& owner,std::optional<std::uint32_t>,std::function<void()> done) override {if(disconnect_hook)disconnect_hook(owner);done();}',1)
        old_interaction='PlayerCore success_player;VerticalNpcInteraction success_interaction;'
        if source.count(old_interaction)!=1:raise RuntimeError('successful interaction fixture changed')
        source=source.replace(old_interaction,'PlayerCore success_player;ZoneLeaveShopState success_shop;std::shared_ptr<VerticalMoveSessionResources> success_movement;',1)
        source=source.replace('success_interaction','success_movement->npc()')
        success_prepare='success_services.async_prepare=[&](VerticalWorldSession& owner,std::function<void(std::exception_ptr)> done) {'
        if source.count(success_prepare)!=1:raise RuntimeError('successful preparation fixture changed')
        source=source.replace(success_prepare,success_prepare+'success_movement=std::make_shared<VerticalMoveSessionResources>(movement_pool,owner.party_user(),success_shop);',1)
        close_anchor='success_session->close();check(world_registry.size()==0 && admission.waiting_count()==0 && success_services.unbound,"successful entry session did not drain");'
        if source.count(close_anchor)!=1:raise RuntimeError('successful close fixture changed')
        close_test=r'''
                admission.untrack(success_session->admission());auto* actor=success_session->world_user();check(actor,"success session lost world actor");
                destination_zone.users->attach(actor->actor,0,0,{});success_session->admission().location=4;success_session->admission().zone=&destination_zone;
                auto& entry=success_session->zone_entry();entry.character_id=actor->actor.id();entry.map=static_cast<std::uint16_t>(destination_zone.map->settings.id);entry.cell={0,0};entry.position={};entry.bound=true;
                vertical_world_quests_rebind_zone(*success_session,world_zone,destination_zone,world_registry);auto quests=success_session->quests_lease();check(bool(quests),"success session lost quests");
                check(quests->packets().receive(net::Packet{4,14,102,0,0,0})==VerticalQuestNpcPacketResult::handled && success_movement->npc().npc==destination_npc && destination_npc->users_using==1,"disconnect fixture did not acquire NPC");
                NpcLeaveServices leave(actor->actor,success_movement->npc(),*destination_npc,0);
                VerticalWorldSessionPacketRouter router(strand,world_registry,8,128,[](VerticalWorldSession&,RuntimeZone&)->std::shared_ptr<VerticalWorldSessionGameplay>{throw std::logic_error("unexpected gameplay");});router.attach(success_session);
                auto ports=vertical_move_world_transfer_ports(world_registry,router,admission,
                    [&](VerticalWorldSession&,RuntimeZone&){ZoneActorLeaveServices value;
                        value.user_leave_lifecycle=[&](Actor&){quests->packets().leave_interactions(success_shop,leave);};
                        value.send_view=[](CellCoordinates,const net::Packet&){};return value;},
                    [&](VerticalWorldSession& selected,RuntimeZone& target){selected.admission().map=static_cast<std::uint16_t>(target.map->settings.id);});
                auto lease=VerticalMoveZoneLeases::create(movement_authority,destination_zone,destination_zone,movement_frame);std::weak_ptr<VerticalMoveZoneLeases> weak_lease=lease;
                auto pending=std::make_unique<VerticalMoveRetainedWorldTransfer>(world_registry,success_session,lease,destination_zone,destination_zone,success_movement,std::move(ports));lease.reset();pending->begin();
                check(success_session->admission().location==2 && admission.waiting_count()==1 && !destination_zone.users->find_actor(actor->actor.id()) && !success_movement->npc().npc && destination_npc->users_using==0 && router.gameplay_suspended(*success_session),"disconnect transfer was not in fenced wait");
                check(router.try_enqueue(*success_session,net::Packet{8,9,255,255}),"disconnect fixture did not enqueue pending gameplay");unsigned disconnects=0;
                success_services.disconnect_hook=[&](VerticalWorldSession& selected){check(&selected==success_session.get() && selected.closing() && admission.waiting_count()==1 && !weak_lease.expired(),"disconnect lost transfer wait ownership");router.stop(selected);++disconnects;};
                success_session->close();check(disconnects==1 && success_session->closing() && world_registry.size()==0 && admission.waiting_count()==0 && success_services.unbound && !weak_lease.expired(),"disconnect did not drain retained world wait");
                bool rejected=false;try{pending->complete();}catch(const std::logic_error&){rejected=true;}check(rejected && !weak_lease.expired(),"closed transfer resumed gameplay");
                pending.reset();check(weak_lease.expired(),"closed transfer did not release its lease");quests.reset();success_services.disconnect_hook={};
'''
        source=source.replace(close_anchor,close_test,1)
    (out / 'vertical_quests_unit.cpp').write_text(source)
    shutil.copy2(coordinator / 'src/session/registry.cpp', out / 'registry.cpp')
    shutil.copy2(coordinator / 'src/game/vertical_quests.cpp', out / 'vertical_quests.cpp')
    shutil.copy2(root / 'src/game/world_admission.cpp', out / 'world_admission.cpp')
    for name in ('world', 'session', 'net', 'game', 'storage', 'quest_start_items_actor'):
        shutil.copy2(coordinator / 'build' / f'libeternity_{name}.a', out / f'libeternity_{name}.a')
    cmd = ['c++','-std=c++20','-DASIO_STANDALONE','-I'+base+'/include','-pthread',base+'/vertical_quests_unit.cpp',base+'/registry.cpp',base+'/vertical_quests.cpp',base+'/world_admission.cpp',
           base+'/libeternity_quest_start_items_actor.a',base+'/libeternity_world.a',base+'/libeternity_quest_start_items_actor.a',base+'/libeternity_game.a',
           base+'/libeternity_storage.a','-lodbc',base+'/libeternity_session.a',base+'/libeternity_net.a','-lcrypto','-o',base+'/probe']
    subprocess.run(['docker','run','--rm','-v',f'{root}:/src','-w','/src','eternity-dev:bookworm',*cmd],check=True)
    name='eternity-movement-transfer-'+uuid.uuid4().hex
    try:
        subprocess.run(['docker','run','--rm','--name',name,'-v',f'{root}:/src','-w','/src','eternity-dev:bookworm',base+'/probe'],check=True,timeout=120)
    except subprocess.TimeoutExpired:
        subprocess.run(['docker','rm','-f',name],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        raise
