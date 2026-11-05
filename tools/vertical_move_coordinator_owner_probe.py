#!/usr/bin/env python3
"""Exercise movement cleanup in a read-only coordinator runtime snapshot.

Requires the coordinator's already-built static libraries; does not build or
modify the coordinator. The generated snapshot and executable are local.
"""
from pathlib import Path
import shutil
import os
import subprocess
import sys
import uuid

from parallel_lock import resource_lock

root = Path(__file__).resolve().parents[1]
coordinator = Path(__file__).resolve().parents[1]
out = root / '.local/movement-coordinator-owner'

pool = r'''
struct MovementProbePool final:PartyMembershipSendWorld {
 unsigned frees=0;VerticalWorldSendBufferPool storage;
 PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t capacity) override {return storage.allocate_send_buffer(capacity);}
 void free_send_buffer(PartyMembershipSendBuffer& b) override {++frees;storage.free_send_buffer(b);}
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

zone_services = r'''
struct MovementProbeDuel final:DuelWorld {
 Actor* find_duel_user(Actor&,std::uint32_t) override {throw std::logic_error("unexpected duel lookup");}
 UserPosition duel_position(const Actor&) override {throw std::logic_error("unexpected duel position");}
 std::int32_t drop_random() override {throw std::logic_error("unexpected duel random");}
 DuelDropItem* allocate_drop_item() override {throw std::logic_error("unexpected duel allocation");}
 bool enter_duel_item(Actor&,DuelDropItem&,bool) override {throw std::logic_error("unexpected duel drop");}
 GameLogSubject duel_log_subject(const Actor&) override {throw std::logic_error("unexpected duel log");}
 std::chrono::sys_seconds local_calendar() override {throw std::logic_error("unexpected duel clock");}
 void send_duel_log(const eternity::net::Packet&) override {throw std::logic_error("unexpected duel logging");}
};
struct MovementProbeGuild final:VerticalGuildDuelMovementServices {
 std::shared_ptr<void> capture_guild_lock(Actor&) override {throw std::logic_error("unexpected guild lock");}
 void* current_guild(Actor&) override {throw std::logic_error("unexpected guild lookup");}
 std::int32_t guild_duel_status(void*) override {throw std::logic_error("unexpected guild duel");}
 void leave_guild_duel(void*,Actor&,std::uint8_t) override {throw std::logic_error("unexpected guild leave");}
};
struct MovementZoneOwners {
 ZoneLeaveShopState shop;MovementProbeDuel duel;MovementProbeGuild guild;std::array<float,5> speeds{3,4,5,6,7};
 VerticalWorldPartyRuntime party;
 static VerticalWorldPartyMembership::Services required_services() {
  VerticalWorldPartyMembership::Services s;
  s.tick=[]()->std::uint32_t{throw std::logic_error("unexpected party tick");};
  s.lock_effects=[](PartyMembershipUser&){throw std::logic_error("unexpected effect lock");};
  s.unlock_effects=[](PartyMembershipUser&){std::terminate();};
  s.cleanup_guild_creation=[](PartyMembershipUser&){throw std::logic_error("unexpected guild cleanup");};
  s.send_scene=[](PartyMembershipUser&,const eternity::net::Packet&){throw std::logic_error("unexpected party scene");};
  s.fill_log=[](PartyMembershipUser&,std::span<std::uint8_t>){throw std::logic_error("unexpected party log");};
  s.send_log=[](const eternity::net::Packet&){throw std::logic_error("unexpected party log send");};
  s.close_socket=[](PartyMembershipUser&,std::int32_t){throw std::logic_error("unexpected party close");};
  s.send_failure=[](PartyMembershipUser&,std::exception_ptr){std::terminate();};return s;
 }
 MovementZoneOwners(VerticalWorldSession::Strand strand,WorldUsers& users,VerticalWorldSession::Registry& registry)
  :party(strand,registry,users,required_services(),
    [](const PartyRequestUser&,std::int32_t,std::int32_t){throw std::logic_error("unexpected party request close");},
    [](const PartyRequestUser&,eternity::net::Packet&){throw std::logic_error("unexpected party fallback");}) {}
};
using AdmittedMovement=VerticalMoveRuntimeComposition<VerticalWorldSessionGameplay,VerticalWorldSession::Strand,VerticalWorldSession>;
struct MovementProbePorts {
 std::function<std::shared_ptr<VerticalWorldSessionGameplay>(VerticalWorldSession&,RuntimeZone&)> gameplay;
 std::function<void(VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()>)> disconnect;
};
'''

waiting_test = r'''
  {
   check(live->admission().location==3 && live_world.zone_waiting_count(live_zone)==1 && !live_zone.users->find_actor(43),"session is not in the real zone-wait list");
   admitted_owners=std::make_shared<MovementZoneOwners>(strand,world_users,sessions);auto* connection=&live->party_user();
   admitted_original_socket=connection->send_socket;connection->send_socket=1;check(connection->id==43,"session party connection identity differs");
   auto underlying_zone_disconnect=std::move(live_services.disconnect_override);
   admitted_zone_ports=std::make_shared<MovementProbePorts>();
   admitted_zone_ports->disconnect=[underlying_zone_disconnect](VerticalWorldSession& owner,std::optional<std::uint32_t>,std::function<void()> done){underlying_zone_disconnect(owner,std::move(done));};
   admitted_shared_world=VerticalMoveSharedWorld<VerticalWorldSession>::create(admitted_owners,sessions,world_users,admitted_owners->party.membership(),
      [owners=admitted_owners](VerticalWorldSession&)->ZoneLeaveShopState&{return owners->shop;},admitted_owners->duel,admitted_owners->guild,
      [](Actor&){throw std::logic_error("unexpected guild refresh");},[]{return std::array<std::uint8_t,100>{};},admitted_owners->speeds,
      []{return 200U;},[]{return std::array<std::uint8_t,4>{};});
   admitted_movement=admitted_shared_world->composition<VerticalWorldSessionGameplay>(VerticalWorldSession::Strand(strand),
      [](VerticalWorldSession&,const eternity::net::Packet&){throw std::logic_error("unexpected other gameplay");});
   admitted_movement->install(*admitted_zone_ports);
   live_services.disconnect_override=[ports=admitted_zone_ports](VerticalWorldSession& owner,std::function<void()> done){ports->disconnect(owner,{},std::move(done));};
   auto waiting_resources=admitted_movement->resources(*live);std::vector<eternity::net::Packet> frames;
   auto previous_send=live->channel().send;live->channel().send=[&](eternity::net::Packet frame){frames.push_back(std::move(frame));};
   unsigned capacity_calls=0;using RetainedPorts=WorldRetainedSendPorts<VerticalWorldSession,VerticalMoveSessionResources>;
   using NativePorts=VerticalMoveWorldSendPorts<WorldNativeSendPorts,AdmittedMovement,VerticalWorldSession,RetainedPorts>;
   auto ports=NativePorts::create(sessions,admitted_shared_world,admitted_movement,[&](VerticalWorldSession& owner){check(&owner==live.get(),"waiting capacity checked foreign session");++capacity_calls;return true;});
   VerticalMoveWorldSendServices<WorldSendServices,WorldNativeSendServices,NativePorts> sends(ports);
   auto buffer=admitted_shared_world->buffers().allocate(eternity::net::Packet{0x72,0x76});
   check(buffer && admitted_shared_world->buffers().enqueue_priority(*connection,*buffer,waiting_resources->queue(),7,0),"real zone-wait priority enqueue failed");buffer.reset();
   unsigned visits=0;live_world.visit_zone_waiting(live_zone,[&](WorldAdmissionUser& user){return sends.retain_waiting(user);},
    [&](WorldAdmissionUser& user){check(&user==&live->admission(),"zone-wait visit lost its session");++visits;sends.flush_waiting(user);});
   check(visits==1 && capacity_calls==1 && frames==std::vector<eternity::net::Packet>{{0x72,0x76}} && waiting_resources->queue().size()==0 && admitted_owners->party.buffers().outstanding()==0,
    "real zone-wait flush lost canonical owners");
   WorldAdmissionUser foreign;foreign.user_id=live->account_id();foreign.location=3;bool rejected=false;
   try{(void)sends.retain_waiting(foreign);}catch(const std::logic_error&){rejected=true;}check(rejected,"world send accepted foreign waiting user");
   live->channel().send=std::move(previous_send);
  }
'''

zone_test = r'''
  try {
   std::cerr<<"zone movement probe: starting"<<std::endl;
   auto geometry=live_map.definition.geometry();CorpseServices corpse_services;MobServices mob_services;NpcServices npc_services;ItemServices item_services;WorldCorpses zone_corpses;
   live_zone.corpses=std::make_unique<ZoneCorpses>(geometry,zone_corpses,corpse_services);
   live_zone.mobs=std::make_unique<ZoneMobs>(geometry,mob_services);live_zone.npcs=std::make_unique<ZoneNpcs>(geometry,npc_services);live_zone.items=std::make_unique<ZoneItems>(geometry,item_services);
   Npc entering_npc;entering_npc.id=501;entering_npc.position={161,0,40};check(live_zone.npcs->enter(entering_npc),"zone probe NPC did not enter");
   auto* connection=&live->party_user();const auto original_socket=admitted_original_socket;
   auto zone_ports=admitted_zone_ports;auto shared_world=admitted_shared_world;
   bool duplicate_composition=false;
   try{(void)shared_world->composition<VerticalWorldSessionGameplay>(VerticalWorldSession::Strand(strand),[](VerticalWorldSession&,const eternity::net::Packet&){throw std::logic_error("unexpected other gameplay");});}
   catch(const std::logic_error&){duplicate_composition=true;}
   check(duplicate_composition,"shared world accepted multiple coordinator compositions");
   std::vector<eternity::net::Packet> movement_frames;
   auto previous_channel_send=live->channel().send;live->channel().send=[&](eternity::net::Packet frame){movement_frames.push_back(std::move(frame));};
   auto& checks=live->world_user()->actor.movement_check();checks={};checks.previous=checks.back=live->zone_entry().position;checks.check_time=5000;
   std::cerr<<"zone movement probe: constructing"<<std::endl;
   auto bound=zone_ports->gameplay(*live,live_zone);
   auto third_move=move;const auto third_bits=std::bit_cast<std::uint32_t>(47.F);for(unsigned i=0;i<4;++i)third_move[5+i]=static_cast<std::uint8_t>(third_bits>>(i*8));
   std::cerr<<"zone movement probe: dispatching"<<std::endl;
   const auto result=bound->dispatch(*live,third_move);
   std::cerr<<"zone movement probe: dispatched"<<std::endl;
   check(result && *result==VerticalMoveResult::moved && live->zone_entry().position.x==47 && live_zone.users->position(43)->x==47,"directory gameplay lost admitted movement");
   check(!movement_frames.empty() && movement_frames.back()[0]==1 && movement_frames.back()[1]==5,"directory gameplay did not deliver 0501");
   auto dispatch_x=[&](float x) {
    checks={};checks.previous=checks.back=live->zone_entry().position;checks.check_time=5000;
    auto packet=third_move;const auto bits=std::bit_cast<std::uint32_t>(x);for(unsigned i=0;i<4;++i)packet[5+i]=static_cast<std::uint8_t>(bits>>(8*i));
    const auto step=bound->dispatch(*live,packet);check(step && *step==VerticalMoveResult::moved,"cell-crossing movement rejected");
   };
   dispatch_x(70.F);movement_frames.clear();dispatch_x(91.F);
   const auto cell=live_zone.users->cell(43);check(cell && cell->x==2 && live->zone_entry().cell.x==2 && live_zone.users->position(43)->x==91,"coordinator actor did not change cells");
   const auto npc_packet=npc_appearance(entering_npc);check(std::find(movement_frames.begin(),movement_frames.end(),npc_packet)!=movement_frames.end(),"entering cell NPC snapshot was not delivered");
   check(!movement_frames.empty() && movement_frames.back()[0]==1 && movement_frames.back()[1]==5,"cell-crossing 0501 was not delivered");
   auto batch_resources=admitted_movement->resources(*live);movement_frames.clear();
   bool capacity=false;unsigned capacity_calls=0;
   using RetainedPorts=WorldRetainedSendPorts<VerticalWorldSession,VerticalMoveSessionResources>;
   using NativePorts=VerticalMoveWorldSendPorts<WorldNativeSendPorts,AdmittedMovement,VerticalWorldSession,RetainedPorts>;
   auto native_ports=NativePorts::create(sessions,shared_world,admitted_movement,[&](VerticalWorldSession& owner){check(&owner==live.get(),"capacity checked foreign session");++capacity_calls;return capacity;});
   const auto session_refs=live.use_count(),resource_refs=batch_resources.use_count();auto send_pin=native_ports->retain_actor(live->world_user()->actor);
   check(send_pin && live.use_count()>session_refs && batch_resources.use_count()>resource_refs,"world send did not pin session and movement resources together");
   send_pin.reset();check(live.use_count()==session_refs && batch_resources.use_count()==resource_refs,"world send pin did not release its owners");
   VerticalMoveWorldSendServices<WorldSendServices,WorldNativeSendServices,NativePorts> native_send(native_ports);
   auto payload=std::make_shared<const eternity::net::Packet>(eternity::net::Packet{0x71,0x76});
   WorldZoneSendMessage world_priority{WorldZoneSendKind::priority,payload,0,0,0,7};
   native_send.deliver(live_zone,world_priority);check(batch_resources->queue().size()==1 && admitted_owners->party.buffers().outstanding()==1,"world send did not use movement queue and pool");
   native_send.flush_actor(live->world_user()->actor);check(capacity_calls==1 && movement_frames.empty() && batch_resources->queue().size()==0 && admitted_owners->party.buffers().outstanding()==0,"world send capacity rejection did not reset queue");
   bool nested_world_flush=false;live->channel().send=[&](eternity::net::Packet frame){
    try{native_send.flush_actor(live->world_user()->actor);}catch(const std::logic_error& error){nested_world_flush=std::string(error.what())=="Movement priority drain is already active";}
    movement_frames.push_back(std::move(frame));};
   capacity=true;native_send.deliver(live_zone,world_priority);native_send.flush_actor(live->world_user()->actor);
   check(nested_world_flush && capacity_calls==2 && movement_frames==std::vector<eternity::net::Packet>{{0x71,0x76}} && admitted_owners->party.buffers().outstanding()==0,"world send did not fence retained actor channel");
   live->channel().send=[&](eternity::net::Packet frame){movement_frames.push_back(std::move(frame));};
   movement_frames.clear();native_ports.reset();
   for(unsigned i=0;i<130;++i) {
    const eternity::net::Packet frame{0x71,0x77,static_cast<std::uint8_t>(i)};
    auto* buffer=allocate_party_membership_send_buffer(frame,shared_world->send_world());
    check(buffer && batch_resources->queue().enqueue(connection->send_socket,*buffer,7,0),"admitted batch queue setup failed");
    release_party_membership_buffer(*buffer,shared_world->send_world());
   }
   check(admitted_owners->party.buffers().outstanding()==130,"coordinator pool did not retain queued buffers");
   bool nested_drain_rejected=false;unsigned sends=0;
   live->channel().send=[&](eternity::net::Packet frame) {
    if(++sends==1) {
     try{(void)admitted_movement->drain(*live);}catch(const std::logic_error& error){nested_drain_rejected=std::string(error.what())=="Movement priority drain is already active";}
    }
    movement_frames.push_back(std::move(frame));
   };
   check(admitted_movement->drain(*live)==128 && batch_resources->queue().size()==2 && movement_frames.size()==128 && admitted_owners->party.buffers().outstanding()==2,"first priority batch differed");
   check(nested_drain_rejected && admitted_movement->drain(*live)==2 && batch_resources->queue().size()==0 && movement_frames.size()==130,"second priority batch or nested drain guard differed");
   for(unsigned i=0;i<130;++i)check(movement_frames[i]==eternity::net::Packet({0x71,0x77,static_cast<std::uint8_t>(i)}),"priority channel order differed");
   check(admitted_owners->party.buffers().outstanding()==0,"priority channel buffers leaked");
   for(unsigned i=0;i<130;++i) {
    auto* buffer=allocate_party_membership_send_buffer(eternity::net::Packet{0x72,0x77,static_cast<std::uint8_t>(i)},shared_world->send_world());
    check(buffer && batch_resources->queue().enqueue(connection->send_socket,*buffer,7,0),"failing batch queue setup failed");
    release_party_membership_buffer(*buffer,shared_world->send_world());
   }
   movement_frames.clear();unsigned attempted=0;
   live->channel().send=[&](eternity::net::Packet frame){if(++attempted==2)throw std::runtime_error("injected send failure");movement_frames.push_back(std::move(frame));};
   bool send_failed=false;try{(void)admitted_movement->drain(*live);}catch(const std::runtime_error&){send_failed=true;}
   check(send_failed && attempted==2 && movement_frames==std::vector<eternity::net::Packet>{{0x72,0x77,0}} &&
    batch_resources->queue().size()==2 && admitted_owners->party.buffers().outstanding()==2,"partial priority failure lost reference ownership");
   live->channel().send=[&](eternity::net::Packet frame){movement_frames.push_back(std::move(frame));};
   check(admitted_movement->drain(*live)==2 && movement_frames==std::vector<eternity::net::Packet>({{0x72,0x77,0},{0x72,0x77,128},{0x72,0x77,129}}) &&
    admitted_owners->party.buffers().outstanding()==0,"priority queue did not retain its unextracted tail after failure");
   batch_resources.reset();
   bound.reset();live->channel().send=std::move(previous_channel_send);
   auto resources=admitted_movement->resources(*live);
   auto* pending=allocate_party_membership_send_buffer(eternity::net::Packet{1,5},shared_world->send_world());
   check(pending && resources->queue().enqueue(connection->send_socket,*pending,7,0),"admitted priority queue setup failed");
   release_party_membership_buffer(*pending,shared_world->send_world());admitted_stale=resources;resources.reset();connection->send_socket=original_socket;
   check(admitted_movement->directory().size()==1 && admitted_owners->party.buffers().outstanding()==1,"admitted queue not retained until disconnect");
   bool early_shutdown=false;try{admitted_owners->party.shutdown();}catch(const std::logic_error&){early_shutdown=true;}
   check(early_shutdown && admitted_owners->party.buffers().outstanding()==1,"party runtime shut down while movement held its pool");
   std::cerr<<"zone movement probe: cleanup"<<std::endl;
   live_zone.items.reset();live_zone.npcs.reset();live_zone.mobs.reset();live_zone.corpses.reset();
  } catch(const std::exception& error) {std::cerr<<"zone movement probe: "<<error.what()<<std::endl;throw;}
'''

install = r'''
 using MovementComposition=VerticalMoveRuntimeComposition<VerticalWorldSessionGameplay,VerticalWorldSession::Strand,VerticalWorldSession>;
 auto movement_pool=std::make_shared<MovementProbePool>();
 auto movement_shop=std::make_shared<ZoneLeaveShopState>();auto movement_lease=std::make_shared<std::tuple<std::shared_ptr<MovementProbePool>,std::shared_ptr<ZoneLeaveShopState>>>(movement_pool,movement_shop);
 auto movement=MovementComposition::create(strand,sessions,
  [movement_lease](VerticalWorldSession& session)->MovementComposition::Resources{return {movement_lease,*std::get<0>(*movement_lease),session.party_user(),*std::get<1>(*movement_lease)};},
  [](VerticalWorldSession&,RuntimeZone&,MovementComposition::Directory&)->VerticalMoveDirectoryServices<VerticalWorldSession>{throw std::logic_error("unexpected movement gameplay");},
  [](VerticalWorldSession&,const eternity::net::Packet&){throw std::logic_error("unexpected other gameplay");});
 auto underlying_configure=std::move(runtime_ports.configure_admission);
 runtime_ports.configure_admission=[underlying_configure=std::move(underlying_configure)](VerticalWorldSession& session,WorldAdmissionUser& user,const PlayerSnapshot& snapshot){
  underlying_configure(session,user,snapshot);
  check(session.party_user().send_socket==-1,"fixture unexpectedly supplied a connected party projection");
  session.party_user().send_socket=1; // Explicit fixture projection; production needs the real connection owner.
 };
 auto underlying_disconnect=std::move(runtime_ports.disconnect);
 runtime_ports.disconnect=[&,underlying_disconnect=std::move(underlying_disconnect)](VerticalWorldSession& session,std::optional<std::uint32_t> reason,std::function<void()> done) mutable {
  check(session.party_user().send_socket==-1,"session did not close its canonical party projection");underlying_disconnect(session,reason,std::move(done));};
 movement->install(runtime_ports);
'''
attach = r'''
  auto& movement_connection=live->party_user();check(movement_connection.id==live->world_user()->actor.id(),"canonical party connection identity differed");movement_connection.send_socket=1;
  check(movement->directory().size()==1 && movement->directory().find(live->world_user()->actor),"admission did not attach movement resources before zone wait");
  auto movement_resources=movement->resources(*live);check(&movement_resources->connection()==&movement_connection,"movement did not retain canonical party connection");
  auto* pending=allocate_party_membership_send_buffer(eternity::net::Packet{1,5},*movement_pool);
  check(pending && movement_resources->queue().enqueue(movement_connection.send_socket,*pending,7,0),"movement queue setup failed");
  release_party_membership_buffer(*pending,*movement_pool);movement_resources.reset();
  check(movement->directory().size()==1 && movement_pool->frees==0,"movement owner was not retained");
'''
assertion = 'check(movement->directory().size()==0 && movement_pool->frees==1 && movement_pool->storage.outstanding()==0,"movement cleanup escaped runtime fences");'

with resource_lock(root, 'build', 2):
    out.mkdir(parents=True, exist_ok=True)
    shutil.copytree(coordinator / 'include', out / 'include', dirs_exist_ok=True)
    for header in (root / 'include/eternity/game').glob('vertical_move*.hpp'):
        shutil.copy2(header, out / 'include/eternity/game' / header.name)
    shutil.copy2(root / 'include/eternity/game/user_priority_shared_cell.hpp', out / 'include/eternity/game/user_priority_shared_cell.hpp')
    for name in ('vertical_session_registry_unit.cpp', 'vertical_protocol_gate_unit.cpp'):
        shutil.copy2(coordinator / 'tests' / name, out / name)
    for header in (coordinator / 'tests').glob('*.hpp'):
        shutil.copy2(header, out / header.name)
    (out / 'data').mkdir(exist_ok=True)
    shutil.copy2(coordinator / 'tests/data/quests-oracle.tsv', out / 'data/quests-oracle.tsv')
    shutil.copy2(coordinator / 'src/session/registry.cpp', out / 'coordinator_session_registry.cpp')
    shutil.copy2(coordinator / 'src/game/vertical_quests.cpp', out / 'coordinator_vertical_quests.cpp')
    for name in ('world', 'session', 'net', 'game', 'storage', 'quest_start_items_actor'):
        shutil.copy2(coordinator / 'build' / f'libeternity_{name}.a', out / f'libeternity_{name}.a')
    path = out / 'vertical_session_registry_unit.cpp'
    text = path.read_text()
    text = text.replace('#include "eternity/game/vertical_world_session_runtime.hpp"', '#include "eternity/game/vertical_world_session_runtime.hpp"\n#include "eternity/game/vertical_move_runtime_composition.hpp"\n#include "eternity/game/vertical_move_channel_send_world.hpp"\n#include "eternity/game/vertical_move_shared_world.hpp"\n#include "eternity/game/vertical_move_world_send_ports.hpp"\n#include "eternity/game/vertical_move_world_send_services.hpp"\n#include "eternity/game/world_native_send_services.hpp"\n#include "eternity/game/world_retained_send_ports.hpp"\n#include "eternity/game/vertical_world_send_buffer_pool.hpp"', 1)
    text = text.replace('struct Owner {', pool + zone_services + '\nstruct Owner {', 1)
    anchor = 'TestRuntime runtime(strand,sessions,2,64,std::move(runtime_ports));runtime_ptr=&runtime;'
    if text.count(anchor) != 1: raise RuntimeError('runtime anchor changed')
    text = text.replace(anchor, install + anchor, 1)
    anchor = '  live->close();check(sessions.size()==1,"runtime released before save binding teardown");runtime_checked.set_value();'
    if text.count(anchor) != 1: raise RuntimeError('disconnect anchor changed')
    text = text.replace(anchor, attach + anchor, 1)
    anchor = 'runtime_drained_future.get();poll.reset();'
    if text.count(anchor) != 1: raise RuntimeError('drain anchor changed')
    text = text.replace(anchor, anchor + assertion, 1)
    anchor = '  auto ignored_gameplay=VerticalWorldSessionGameplay::create_retained(strand,sessions,live,live_zone,live->zone_entry(),'
    if text.count(anchor) != 1: raise RuntimeError('zone gameplay anchor changed')
    if os.environ.get('ETERNITY_MOVEMENT_ZONE_PROBE') == '1':
        declaration = ' SessionMoveView move_view;SessionMoveServices move_services;'
        if text.count(declaration) != 1: raise RuntimeError('zone declaration anchor changed')
        text = text.replace(declaration, ' std::shared_ptr<AdmittedMovement> admitted_movement;std::shared_ptr<MovementZoneOwners> admitted_owners;std::shared_ptr<VerticalMoveSharedWorld<VerticalWorldSession>> admitted_shared_world;std::shared_ptr<MovementProbePorts> admitted_zone_ports;std::shared_ptr<VerticalMoveSessionResources> admitted_stale;std::int32_t admitted_original_socket=-1;\n'+declaration, 1)
        waiting_anchor = '"world wait did not preserve queued input");live->zone_ready();'
        if text.count(waiting_anchor) != 1: raise RuntimeError('zone-wait anchor changed')
        text = text.replace(waiting_anchor, '"world wait did not preserve queued input");'+waiting_test+'live->zone_ready();', 1)
        close_anchor = 'live->close();bool gameplay_stopped=false;'
        if text.count(close_anchor) != 1: raise RuntimeError('zone close anchor changed')
        close_inside_visit = r'''live->party_user().send_socket=1;
  {
   using RetainedPorts=WorldRetainedSendPorts<VerticalWorldSession,VerticalMoveSessionResources>;
   using NativePorts=VerticalMoveWorldSendPorts<WorldNativeSendPorts,AdmittedMovement,VerticalWorldSession,RetainedPorts>;
   auto ports=NativePorts::create(sessions,admitted_shared_world,admitted_movement,
    [](VerticalWorldSession&)->bool{throw std::logic_error("unexpected close-visit capacity query");});
   VerticalMoveWorldSendServices<WorldSendServices,WorldNativeSendServices,NativePorts> sends(ports);
   auto prior=live->channel().send;bool callback=false;
   live->channel().send=[&](eternity::net::Packet packet){
    check(packet==eternity::net::Packet({0x73,0x76}),"close-visit packet changed");
    live->close();callback=true;
    check(live->closing() && live->party_user().send_socket==-1 && admitted_movement->directory().size()==1 && sessions.size()==1,
     "close during world visit released retained owners");
   };
   auto payload=std::make_shared<const eternity::net::Packet>(eternity::net::Packet{0x73,0x76});
   sends.deliver(live_zone,WorldZoneSendMessage{WorldZoneSendKind::all,payload});
   live->channel().send=std::move(prior);check(callback,"world send did not reach closing session");
  }
  bool gameplay_stopped=false;'''
        text = text.replace(close_anchor, close_inside_visit, 1)
        deferred = 'zone_entered.set_value();'
        if text.count(deferred) != 1: raise RuntimeError('zone deferred anchor changed')
        text = text.replace(deferred, 'check(live->party_user().send_socket==-1 && admitted_movement && admitted_movement->directory().size()==1 && admitted_owners->party.buffers().outstanding()==1,"session close failed to fence the admitted queue");'+deferred, 1)
        completion = 'disconnect_done();disconnect_done={};check(sessions.size()==0,"disconnect completion retained session");'
        if text.count(completion) != 1: raise RuntimeError('zone completion anchor changed')
        text = text.replace(completion, completion+'check(admitted_movement && admitted_movement->directory().size()==0 && admitted_owners->party.buffers().outstanding()==0,"admitted movement queue escaped deferred disconnect");admitted_owners->party.shutdown();', 1)
        destruction = 'check(retained.expired(),"drained save preparation leaked its session");'
        if text.count(destruction) != 1: raise RuntimeError('zone session destruction anchor changed')
        retired = r'''check(bool(admitted_stale),"retired movement handle was not retained");
 unsigned rejected=0;auto rejects=[&](auto access){try{access();}catch(const std::logic_error&){++rejected;}};
 rejects([&]{(void)admitted_stale->pool();});rejects([&]{(void)admitted_stale->queue();});rejects([&]{(void)admitted_stale->connection();});
 rejects([&]{(void)admitted_stale->shop();});rejects([&]{(void)admitted_stale->cadence();});rejects([&]{(void)admitted_stale->npc();});
 rejects([&]{(void)admitted_stale->duel();});rejects([&]{(void)admitted_stale->guild_duel();});rejects([&]{(void)admitted_stale->close_queue();});
 check(rejected==9,"retired movement handle exposed destroyed session resources");admitted_stale.reset();'''
        text = text.replace(destruction, destruction+retired, 1)
        text = text.replace(anchor, zone_test + anchor, 1)
        text = text.replace('live_map.definition.size=81;RuntimeZone live_zone;', 'live_map.definition.size=200;RuntimeZone live_zone;', 1)
        text = text.replace('party_view.member.position[0]==46', 'party_view.member.position[0]==91', 1)
        strand_anchor = 'zone_entered_future.get();'
        if text.count(strand_anchor) != 1: raise RuntimeError('zone strand anchor changed')
        strand_check = r'''bool off_resources=false,off_drain=false;
 try{admitted_movement->resources(*live);}catch(const std::logic_error& e){off_resources=std::string(e.what())=="Movement runtime outside world strand";}
 try{admitted_movement->drain(*live);}catch(const std::logic_error& e){off_drain=std::string(e.what())=="Movement runtime outside world strand";}
 if(!off_resources || !off_drain)std::cerr<<"movement probe: off-strand guard mismatch"<<std::endl;
 check(off_resources && off_drain,"movement runtime accepted calls outside world strand");std::cerr<<"movement probe: off-strand guards passed"<<std::endl;'''
        text = text.replace(strand_anchor, strand_anchor + strand_check, 1)
        for future in ('entered_success_future','bound_open_disposal_future','bound_closed_future','entered_zone_future','zone_entered_future','zone_finished_future','save_cleaned'):
            text = text.replace(future + '.get();', 'std::cerr<<"waiting '+future+'"<<std::endl;'+future+'.get();std::cerr<<"completed '+future+'"<<std::endl;', 1)
        text = text.replace('catch(...){save_cleanup.set_exception(std::current_exception());}',
            'catch(...){auto error=std::current_exception();try{std::rethrow_exception(error);}catch(const std::exception& e){std::cerr<<"save cleanup probe: "<<e.what()<<std::endl;}catch(...){}save_cleanup.set_exception(error);}', 1)
    text = text.replace('catch(...){zone_finished.set_exception(std::current_exception());}',
        'catch(...){auto error=std::current_exception();try{std::rethrow_exception(error);}catch(const std::exception& e){std::cerr<<"zone teardown probe: "<<e.what()<<std::endl;}catch(...){}zone_finished.set_exception(error);}')
    path.write_text(text)
    base = '/src/.local/movement-coordinator-owner'
    command = ['c++', '-std=c++20', '-DASIO_STANDALONE', '-I'+base+'/include', '-pthread', base+'/vertical_session_registry_unit.cpp', base+'/coordinator_session_registry.cpp', base+'/coordinator_vertical_quests.cpp',
               base+'/libeternity_world.a', base+'/libeternity_session.a', base+'/libeternity_net.a', '-lcrypto',
               base+'/libeternity_quest_start_items_actor.a', base+'/libeternity_game.a', base+'/libeternity_storage.a', '-lodbc', '-o', base+'/probe']
    subprocess.run(['docker','run','--rm','-v',f'{root}:/src','-w','/src','eternity-dev:bookworm',*command],check=True)
    name = 'eternity-movement-probe-' + uuid.uuid4().hex
    try:
        subprocess.run(['docker','run','--rm','--name',name,'-v',f'{root}:/src','-w','/src','eternity-dev:bookworm',base+'/probe'],check=True,timeout=90)
    except subprocess.TimeoutExpired:
        subprocess.run(['docker','rm','-f',name],check=False,stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
        raise
