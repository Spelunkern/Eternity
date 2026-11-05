#include "eternity/game/vertical_combat_scheduler_zone.hpp"
#include "eternity/game/vertical_combat_scheduler_equipment.hpp"
#include <stdexcept>
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include "eternity/game/ground_items.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/party_runtime_raid.hpp"
#include "eternity/game/vertical_move_user.hpp"
namespace eternity::game {
void VerticalCombatRecoveryEffectServices::expire_effects(Actor& actor,std::uint32_t tick){
 if(actor.effects().size()==0)return;
 if(!actor.effect_database_bound())throw std::logic_error("Effect expiry requires the actor DB-agent binding");
 EffectEquipment equipped;
 for(const auto& item:actor.inventory())if(item.bag==0){if(item.slot==5)equipped.weapon=&item;else if(item.slot==1)equipped.armor=&item;}
 const auto& definitions=effect_item_definitions(actor);
 actor.effects().expire(tick,actor.status(),equipped,definitions,actor,actor);
}
void VerticalCombatRecoveryRebirthServices::update_rebirth(Actor& actor,std::uint32_t tick){
 update_vehicle_prefix(actor,tick);
 if(actor.attack_state().mode!=1)return;
 if(!actor.death_metadata_bound())throw std::logic_error("Rebirth update requires the actor death metadata");
 if(actor.death_metadata().deadline>tick)return;
 log_rebirth_timeout(actor);
 if(actor.death_metadata().code==1 && rebirth_party_present(actor))expire_party_soul(actor);
 else expire_solo_soul(actor);
}
VerticalCombatRecoveryPartyServices::VerticalCombatRecoveryPartyServices(Actor& actor,PartyMembershipUser& member):actor_(actor),member_(member){
 if(actor.id()!=member.id)throw std::invalid_argument("Rebirth party membership requires its actor");
}
bool VerticalCombatRecoveryPartyServices::rebirth_party_present(Actor& actor){
 if(&actor!=&actor_ || actor.id()!=member_.id)throw std::invalid_argument("Rebirth party membership bound to another actor");
 return member_.party!=nullptr;
}
VerticalCombatRecoveryLogServices::VerticalCombatRecoveryLogServices(Actor& actor,PartyMembershipUser& member):VerticalCombatRecoveryPartyServices(actor,member),log_actor_(actor){}
void VerticalCombatRecoveryLogServices::log_rebirth_timeout(Actor& actor){
 if(&actor!=&log_actor_)throw std::invalid_argument("Rebirth log bound to another actor");
 const auto scratch=rebirth_log_scratch(actor);const auto subject=rebirth_log_subject(actor);
 if(subject.character_id!=actor.id())throw std::invalid_argument("Rebirth log subject requires the same actor");
 net::Packet packet(scratch.begin(),scratch.end());write_game_log_main(packet,subject,rebirth_log_calendar());
 packet[0]=0x05;packet[1]=0x03;for(std::size_t i=74;i<79;++i)packet[i]=0;packet[99]=5;packet[100]=0;
 send_rebirth_log(packet);
}
void vertical_combat_vehicle_prefix(Actor& actor,std::uint32_t tick,VerticalCombatVehicleItemRecords& records){
 auto& appearance=actor.appearance();auto& actions=actor.actions();
 if(appearance.vehicle_mode!=1 || actions.vehicle_request>tick)return;
 const auto held=actor.retained_inventory_item(0,13);const GroundItem* item=nullptr;
 if(!held){if(!actor.inventory_slot_empty(0,13))throw std::logic_error("Vehicle prefix requires retained slot-13 item ownership");}
 else{
  if(!held->item||!held->pool||held->item->item.bag!=0||held->item->item.slot!=13)throw std::logic_error("Vehicle item owner mismatch");
  const auto& inventory=actor.inventory();if(std::count_if(inventory.begin(),inventory.end(),[](const PlayerItem& entry){return entry.bag==0&&entry.slot==13;})!=1)throw std::logic_error("Vehicle item mirror mismatch");
  item=held->item;
 }
 const auto shape=actor.status().shape;
 if(!item || shape==10 || shape==11 || shape==12){
  actions.vehicle_request=0;appearance.vehicle_mode=0;actor.send_self(net::Packet{0x16,2,0,1});return;
 }
 if(!item->definition)throw std::logic_error("Vehicle prefix requires the retained item definition");
 const auto record=records.vehicle_item_record(*item);
 if(record.size()<0x44)throw std::logic_error("Vehicle prefix requires the retained original item record");
 const auto byte=[&](std::size_t index){return std::to_integer<std::uint8_t>(record[index]);};
 const auto model=static_cast<std::uint16_t>(byte(0x42)|(static_cast<unsigned>(byte(0x43))<<8));
 appearance.vehicle_addition=(actor.appearance().identity.grow>=2 && byte(0x30)>=2)?15:14;
 appearance.vehicle_shape=model<2?model*2:model+7;
 actions.vehicle_request=tick+static_cast<std::uint32_t>(byte(0x32))*1000u;appearance.vehicle_mode=2;
 actor.send_self(net::Packet{0x16,2,1,1});actor.shape_changed(actor.status());
}
VerticalCombatRecoveryVehicleServices::VerticalCombatRecoveryVehicleServices(Actor& actor,PartyMembershipUser& member):VerticalCombatRecoveryLogServices(actor,member),vehicle_actor_(actor){}
void VerticalCombatRecoveryVehicleServices::update_vehicle_prefix(Actor& actor,std::uint32_t tick){
 if(&actor!=&vehicle_actor_)throw std::invalid_argument("Vehicle prefix bound to another actor");
 vertical_combat_vehicle_prefix(actor,tick,*this);
}
VerticalCombatRecoveryPartySoulServices::VerticalCombatRecoveryPartySoulServices(Actor& actor,PartyMembershipUser& member,PartyRequestState& state,VerticalCombatPartySoulWorld& world)
 :VerticalCombatRecoveryVehicleServices(actor,member),soul_actor_(actor),soul_member_(member),soul_state_(state),soul_world_(world){}
void VerticalCombatRecoveryPartySoulServices::expire_party_soul(Actor& actor){
 if(&actor!=&soul_actor_ || actor.id()!=soul_member_.id)throw std::invalid_argument("Party soul bound to another actor");
 if(soul_state_.zone==0 || actor.attack_state().mode!=1)return;
 if(!actor.death_metadata_bound())throw std::logic_error("Party soul requires actor death metadata");
 if(actor.death_metadata().soul==2)return;
 if(actor.appearance().identity.grow==3 || !soul_member_.party){expire_solo_soul(actor);return;}
 auto* current=soul_member_.party;auto party=soul_world_.retain_party(*current);
 if(!party || static_cast<PartyNormalMembership*>(party.get())!=current || soul_member_.party!=current)throw std::logic_error("Party soul requires the exact retained party");
 auto& lock=soul_world_.membership_world();VerticalCombatSoulLeader selected;
 lock.lock(*party);
 try{
  if(party->count>0){
   auto* member=raid_membership_slot(*party,party->leader);
   if(member){selected=soul_world_.retain_leader(*member);if(!selected.actor||selected.member!=member||!selected.state||!selected.lifetime||selected.actor->id()!=member->id)throw std::logic_error("Party soul requires the exact retained leader");}
  }
 }catch(...){lock.unlock(*party);throw;}
 lock.unlock(*party);
 if(!selected.actor || selected.actor->attack_state().mode==1 || selected.state->scene_state!=4 || selected.state->zone!=soul_state_.zone || selected.member->instance!=soul_member_.instance){expire_solo_soul(actor);return;}
 soul_world_.drop_experience(actor,actor.death_metadata().penalty);
 const auto map=soul_member_.instance;const auto destination=soul_world_.leader_position(*selected.actor);
 mob_targeting_rebirth(actor,destination,5,map,true,soul_world_.rebirth_world(actor));
}
void VerticalCombatUltimateSoulWorld::ultimate_soul(Actor& actor){
 if(actor.status().skills.benefits.rebirth==0){ultimate_item_soul(actor);return;}
 const auto& maximum=actor.status().skills.equipment.base.maximum;
 const auto half=[](std::uint32_t v){return std::bit_cast<std::uint32_t>(std::bit_cast<std::int32_t>(v)/2);};
 actor.replace_resources_raw({half(maximum.hp),half(maximum.mp),half(maximum.sp)});
 auto [position,map]=bind_scratch(actor);bind_from_zone(actor,position,map);
 mob_targeting_rebirth(actor,position,4,map,false,rebirth_world(actor));
}
std::optional<VerticalCombatRebirthItem> vertical_combat_find_rebirth_item(Actor& actor,std::int32_t bags,bool zero_duration,VerticalCombatRebirthItemRecords& records){
 if(bags>9)throw std::logic_error("Rebirth search bag limit exceeds actor inventory representation");
 for(unsigned pass=0;pass!=2;++pass){
  for(int bag=1;bag<=bags;++bag)for(std::uint16_t slot=0;slot!=24;++slot){
   const auto held=actor.retained_inventory_item(static_cast<std::uint8_t>(bag),slot);
   if(!held){if(!actor.inventory_slot_empty(static_cast<std::uint8_t>(bag),slot))throw std::logic_error("Rebirth search requires retained item ownership");continue;}
   if(!held->item||!held->pool||!held->item->definition||held->item->item.bag!=bag||held->item->item.slot!=slot)throw std::logic_error("Rebirth search item owner mismatch");
   const auto record=records.rebirth_item_record(*held->item);
   if(record.size()<0xa4)throw std::logic_error("Rebirth search requires the original item record");
   const auto b=[&](unsigned i){return std::to_integer<std::uint8_t>(record[i]);};
   const auto effect=static_cast<std::uint32_t>(b(0xa0))|(static_cast<std::uint32_t>(b(0xa1))<<8)|(static_cast<std::uint32_t>(b(0xa2))<<16)|(static_cast<std::uint32_t>(b(0xa3))<<24);
   const auto duration=static_cast<unsigned>(b(0x34))|(static_cast<unsigned>(b(0x35))<<8);
   if(effect==26 && b(0x46)==37 && (pass==1 || (duration==0)==zero_duration))return VerticalCombatRebirthItem{static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),held->item};
  }
 }
 return {};
}
void VerticalCombatUltimateItemSoulWorld::ultimate_item_soul(Actor& actor){
 const auto item=vertical_combat_find_rebirth_item(actor,rebirth_bag_limit(actor),false,*this);
 if(item)ultimate_rebirth_item(actor,*item);else ultimate_no_rebirth_item(actor);
}
void vertical_combat_ultimate_item_rebirth(Actor& actor,const VerticalCombatRebirthItem& item,PartyMembershipUser& member,PartyRequestState& state,VerticalCombatSoloSoulWorld& world,VerticalCombatUltimateItemUse& use){
 if(member.id!=actor.id())throw std::invalid_argument("Rebirth item membership requires the same actor");
 const auto held=actor.retained_inventory_item(item.bag,item.slot);
 if(!item.item||!held||held->item!=item.item||!held->pool)throw std::logic_error("Rebirth item requires the selected retained item");
 use.use_rebirth_item(actor,item.bag,item.slot);
 const auto& maximum=actor.status().skills.equipment.base.maximum;
 const auto half=[](std::uint32_t v){return std::bit_cast<std::uint32_t>(std::bit_cast<std::int32_t>(v)/2);};
 actor.replace_resources_raw({half(maximum.hp),half(maximum.mp),half(maximum.sp)});
 if(state.zone!=0){
  auto [position,map]=world.bind_scratch(actor);world.bind_from_zone(actor,position,map);
  mob_targeting_rebirth(actor,position,4,map,false,world.rebirth_world(actor));return;
 }
 auto& lifecycle=world.rebirth_world(actor);const auto map=member.instance;
 if(lifecycle.map_present(map)){
  auto [position,unused]=world.bind_scratch(actor);(void)unused;world.bind_from_map(actor,map,position);
  mob_targeting_rebirth(actor,position,4,member.instance,false,lifecycle);
 }else{
  const auto position=world.actor_position(actor);mob_targeting_rebirth(actor,position,4,map,false,lifecycle);
 }
}
VerticalCombatUltimateItemRebirthWorld::VerticalCombatUltimateItemRebirthWorld(Actor& actor,PartyMembershipUser& member,PartyRequestState& state):item_actor_(actor),item_member_(member),item_state_(state){
 if(actor.id()!=member.id)throw std::invalid_argument("Rebirth item membership requires the same actor");
}
void VerticalCombatUltimateItemRebirthWorld::ultimate_rebirth_item(Actor& actor,const VerticalCombatRebirthItem& item){
 if(&actor!=&item_actor_)throw std::invalid_argument("Rebirth item bound to another actor");
 vertical_combat_ultimate_item_rebirth(actor,item,item_member_,item_state_,*this,*this);
}
void vertical_combat_ultimate_no_item(Actor& actor,PlayerCore& player,PartyMembershipUser& member,WorldAdmissionUser& admission,WorldCorpses& corpses,VerticalCombatSoloSoulWorld& world,VerticalCombatUltimateNoItemServices& services){
 if(actor.id()!=member.id)throw std::invalid_argument("Ultimate logout requires the same retained actor and memberships");
 if(!actor.death_metadata_bound())throw std::logic_error("Ultimate logout requires actor death metadata");
 world.drop_experience(actor,actor.death_metadata().penalty);
 net::Packet packet{0x06,0x04};
 const auto append=[&](std::uint32_t value){for(unsigned i=0;i!=4;++i)packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
 append(actor.account_id());append(player.character.id);services.send_ultimate_death_database(packet);
 if(admission.logout_tick==0 && member.send_socket!=-1){admission.logout_tick=0;admission.logout_type=1;services.logout_ultimate_user(actor);}
 (void)corpses.leave(actor.id());
}
VerticalCombatUltimateNoItemWorld::VerticalCombatUltimateNoItemWorld(Actor& actor,PlayerCore& player,PartyMembershipUser& member,PartyRequestState& state,WorldAdmissionUser& admission,WorldCorpses& corpses)
 :VerticalCombatUltimateItemRebirthWorld(actor,member,state),no_item_actor_(actor),no_item_player_(player),no_item_member_(member),no_item_admission_(admission),no_item_corpses_(corpses){
 if(actor.id()!=member.id)throw std::invalid_argument("Ultimate logout requires the same retained actor and memberships");
}
void VerticalCombatUltimateNoItemWorld::ultimate_no_rebirth_item(Actor& actor){
 if(&actor!=&no_item_actor_)throw std::invalid_argument("Ultimate logout bound to another actor");
 vertical_combat_ultimate_no_item(actor,no_item_player_,no_item_member_,no_item_admission_,no_item_corpses_,*this,*this);
}
void vertical_combat_ultimate_use_item(Actor& actor,std::uint8_t bag,std::uint8_t slot,VerticalCombatUltimateItemUseServices& services){
 const auto held=actor.retained_inventory_item(bag,slot);
 if(!held || !held->item || !held->pool || !held->item->definition)throw std::logic_error("Ultimate item use requires the retained item and pool");
 auto& item=*held->item;const auto id=item.definition->id;
 if(item.item.type==100 || (id>=0xaca1 && id<=0xaca7))services.log_ultimate_item_use(actor,item,bag,slot);
 const bool last=item.item.count<=1;
 if(last){
  services.detach_ultimate_item(actor,item,bag,slot);
  if(!actor.inventory_slot_empty(bag,slot))throw std::logic_error("Ultimate item detach did not remove the slot");
  item.item.count=static_cast<std::uint8_t>(item.item.count-1u);
 }else{
  if(!actor.set_inventory_item_count(bag,slot,static_cast<std::uint8_t>(item.item.count-1u),&item))throw std::logic_error("Ultimate item use lost its retained item");
 }
 net::Packet db{0x02,0x07};const auto append=[&](net::Packet& packet,std::uint32_t value){for(unsigned i=0;i!=4;++i)packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
 append(db,actor.account_id());db.push_back(bag);db.push_back(slot);db.push_back(1);services.send_ultimate_item_database(actor,db);
 net::Packet view{0x0a,0x05};append(view,actor.id());view.push_back(bag);view.push_back(slot);view.push_back(item.item.type);view.push_back(item.item.type_id);view.push_back(item.item.count);
 if(services.ultimate_item_zone_present(actor))services.send_ultimate_item_view(actor,view);
 if(last)held->pool->release(item);
}
void VerticalCombatUltimateItemUseWorld::use_rebirth_item(Actor& actor,std::uint8_t bag,std::uint8_t slot){vertical_combat_ultimate_use_item(actor,bag,slot,*this);}
net::Packet vertical_combat_ultimate_item_log(const GroundItem& item,std::string_view name,std::uint8_t bag,std::uint8_t slot,const GameLogSubject& subject,std::chrono::sys_seconds calendar,const std::array<std::uint8_t,152>& scratch){
 if(!item.definition || name.size()>31 || name.find('\0')!=name.npos)throw std::invalid_argument("Ultimate item log requires the retained item definition and bounded original name");
 net::Packet packet(scratch.begin(),scratch.end());write_game_log_main(packet,subject,calendar);
 const auto put=[&](std::size_t offset,std::uint64_t value,unsigned length){for(unsigned i=0;i<length;++i)packet[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};
 put(0,0x040e,2);put(74,item.item.uid,8);put(82,item.definition->id,4);
 std::copy(name.begin(),name.end(),packet.begin()+86);packet[86+name.size()]=0;
 std::copy(item.item.gems.begin(),item.item.gems.end(),packet.begin()+118);put(124,item.item.made_at,4);
 packet[128]=0;packet[149]=bag;packet[150]=slot;return packet;
}
void VerticalCombatUltimateItemLogWorld::log_ultimate_item_use(Actor& actor,const GroundItem& item,std::uint8_t bag,std::uint8_t slot){
 const auto subject=ultimate_item_log_subject(actor);
 const auto packet=vertical_combat_ultimate_item_log(item,ultimate_item_name(item),bag,slot,subject,ultimate_item_log_calendar(),ultimate_item_log_scratch(actor));send_ultimate_item_log(actor,packet);
}
VerticalCombatUltimateInventoryBindings::VerticalCombatUltimateInventoryBindings(Actor& actor,PartyRequestState& state,InventoryWorld& inventory,VerticalCombatView& view):actor_(actor),state_(state),inventory_(inventory),view_(view){}
void VerticalCombatUltimateInventoryBindings::require_actor(const Actor& actor) const{if(&actor!=&actor_)throw std::invalid_argument("Ultimate inventory bound to another actor");}
std::string VerticalCombatUltimateInventoryBindings::item_name(const GroundItem& item){
 const auto held=actor_.retained_inventory_item(item.item.bag,item.item.slot);
 if(!held || held->item!=&item || !held->pool || !item.definition)throw std::logic_error("Ultimate log requires the same retained inventory item");
 return inventory_.inventory_item_name(*item.definition);
}
GameLogSubject VerticalCombatUltimateInventoryBindings::subject(const Actor& actor){require_actor(actor);return inventory_.money_log_subject(actor);}
std::chrono::sys_seconds VerticalCombatUltimateInventoryBindings::calendar(){return inventory_.money_local_calendar();}
void VerticalCombatUltimateInventoryBindings::database(Actor& actor,const net::Packet& packet){require_actor(actor);inventory_.send_inventory_database(packet);}
void VerticalCombatUltimateInventoryBindings::log(Actor& actor,const net::Packet& packet){require_actor(actor);inventory_.send_money_log(packet);}
bool VerticalCombatUltimateInventoryBindings::zone_present(const Actor& actor) const{require_actor(actor);return state_.zone!=0;}
void VerticalCombatUltimateInventoryBindings::view(Actor& actor,const net::Packet& packet){require_actor(actor);view_.send(actor,packet);}
VerticalCombatUltimateInventoryWorld::VerticalCombatUltimateInventoryWorld(Actor& actor,PlayerCore& player,PartyMembershipUser& member,PartyRequestState& state,WorldAdmissionUser& admission,WorldCorpses& corpses,InventoryWorld& inventory,VerticalCombatView& view)
 :VerticalCombatUltimateItemLogWorld(actor,player,member,state,admission,corpses),bindings_(actor,state,inventory,view){}
std::string VerticalCombatUltimateInventoryWorld::ultimate_item_name(const GroundItem& item){return bindings_.item_name(item);}
GameLogSubject VerticalCombatUltimateInventoryWorld::ultimate_item_log_subject(const Actor& actor){return bindings_.subject(actor);}
std::chrono::sys_seconds VerticalCombatUltimateInventoryWorld::ultimate_item_log_calendar(){return bindings_.calendar();}
void VerticalCombatUltimateInventoryWorld::send_ultimate_item_log(Actor& actor,const net::Packet& packet){bindings_.log(actor,packet);}
void VerticalCombatUltimateInventoryWorld::send_ultimate_item_database(Actor& actor,const net::Packet& packet){bindings_.database(actor,packet);}
bool VerticalCombatUltimateInventoryWorld::ultimate_item_zone_present(const Actor& actor){return bindings_.zone_present(actor);}
void VerticalCombatUltimateInventoryWorld::send_ultimate_item_view(Actor& actor,const net::Packet& packet){bindings_.view(actor,packet);}
VerticalCombatUltimateActorInventoryBindings::VerticalCombatUltimateActorInventoryBindings(Actor& actor,VerticalCombatVehicleItemRecords& records):actor_(actor),records_(records){}
std::int32_t VerticalCombatUltimateActorInventoryBindings::bag_limit(const Actor& actor) const{
 if(&actor!=&actor_)throw std::invalid_argument("Ultimate bag limit bound to another actor");
 return actor.status().skills.max_bags;
}
std::span<const std::byte> VerticalCombatUltimateActorInventoryBindings::rebirth_item_record(const GroundItem& item){
 const auto held=actor_.retained_inventory_item(item.item.bag,item.item.slot);
 if(!held || held->item!=&item || !held->pool || !item.definition)throw std::logic_error("Ultimate item record requires the same retained inventory item");
 const auto record=records_.vehicle_item_record(item);
 if(record.size()<0xa4)throw std::logic_error("Ultimate rebirth requires the complete original item record");
 return record;
}
VerticalCombatUltimateActorInventoryWorld::VerticalCombatUltimateActorInventoryWorld(Actor& actor,PlayerCore& player,PartyMembershipUser& member,PartyRequestState& state,WorldAdmissionUser& admission,WorldCorpses& corpses,InventoryWorld& inventory,VerticalCombatView& view,VerticalCombatVehicleItemRecords& records)
 :VerticalCombatUltimateInventoryWorld(actor,player,member,state,admission,corpses,inventory,view),actor_inventory_(actor,records){}
std::int32_t VerticalCombatUltimateActorInventoryWorld::rebirth_bag_limit(const Actor& actor){return actor_inventory_.bag_limit(actor);}
std::span<const std::byte> VerticalCombatUltimateActorInventoryWorld::rebirth_item_record(const GroundItem& item){return actor_inventory_.rebirth_item_record(item);}
void vertical_combat_experience_drop(Actor& actor,PlayerCore& player,const Experience& curve,std::int32_t penalty,VerticalCombatExperienceDropServices& services){
 const auto level=actor.status().level;
 if(level<=10 || penalty<=0 || actor.status().skills.benefits.no_exp_loss!=0)return;
 if(std::fegetround()!=FE_TONEAREST)throw std::logic_error("Experience drop requires nearest floating rounding");
 const auto bounds=curve.bounds(actor.appearance().identity.grow,level);
 const auto signed_bits=[](std::uint32_t value){return std::bit_cast<std::int32_t>(value);};
 const auto fist=[](double value){if(!std::isfinite(value)||value<static_cast<double>(INT32_MIN)||value>=2147483648.0)return INT32_MIN;return static_cast<std::int32_t>(std::trunc(value));};
 volatile float difference=static_cast<float>(signed_bits(bounds.next-bounds.previous));
 volatile float percent=static_cast<float>(penalty);
 volatile double fraction=static_cast<double>(percent)/100.0;
 volatile double amount=fraction*static_cast<double>(difference);
 auto loss=fist(amount);
 const auto reduction=services.experience_drop_reduction(actor);
 if(reduction!=0){
  volatile float reduction_float=static_cast<float>(reduction);volatile float loss_float=static_cast<float>(loss);
  volatile double reduction_fraction=static_cast<double>(reduction_float)/100.0;
  volatile double saved_amount=reduction_fraction*static_cast<double>(loss_float);
  const auto saved=fist(saved_amount);
  loss=signed_bits(static_cast<std::uint32_t>(loss)-static_cast<std::uint32_t>(saved));
 }
 const auto available=signed_bits(player.experience-bounds.previous);
 if(available<1)loss=0;else if(loss>available)loss=available;
 player.experience-=static_cast<std::uint32_t>(loss);
 net::Packet packet{0x02,0x06};const auto append=[&](std::uint32_t value){for(unsigned i=0;i!=4;++i)packet.push_back(static_cast<std::uint8_t>(value>>(i*8)));};
 append(actor.account_id());append(player.experience);services.send_experience_drop_database(actor,packet);
}
VerticalCombatExperienceDropWorld::VerticalCombatExperienceDropWorld(Actor& actor,PlayerCore& player,PartyMembershipUser& member,PartyRequestState& state,WorldAdmissionUser& admission,WorldCorpses& corpses,InventoryWorld& inventory,VerticalCombatView& view,VerticalCombatVehicleItemRecords& records,const Experience& curve)
 :VerticalCombatUltimateActorInventoryWorld(actor,player,member,state,admission,corpses,inventory,view,records),exp_actor_(actor),exp_player_(player),exp_curve_(curve){}
void VerticalCombatExperienceDropWorld::drop_experience(Actor& actor,std::int32_t penalty){
 if(&actor!=&exp_actor_)throw std::invalid_argument("Experience drop bound to another actor");
 vertical_combat_experience_drop(actor,exp_player_,exp_curve_,penalty,*this);
}
VerticalCombatExperienceReplicaDelivery::VerticalCombatExperienceReplicaDelivery(Actor& actor,PlayerCore& player,CharacterReplicaDelivery& delivery):actor_(actor),player_(player),delivery_(delivery){}
void VerticalCombatExperienceReplicaDelivery::send(Actor& actor,const net::Packet& packet){
 if(&actor!=&actor_)throw std::invalid_argument("Experience replica bound to another actor");
 if(packet.size()!=10 || packet[0]!=0x02 || packet[1]!=0x06)throw std::invalid_argument("Experience replica requires a 0602 packet");
 const auto read=[&](std::size_t offset){std::uint32_t value=0;for(unsigned i=0;i!=4;++i)value|=static_cast<std::uint32_t>(packet[offset+i])<<(8*i);return value;};
 if(read(2)!=actor.account_id() || read(6)!=player_.experience)throw std::logic_error("Experience replica packet does not match the retained live owners");
 if(delivery_.apply(packet)!=InventoryReplicaResult::applied)throw std::runtime_error("Experience DB replica rejected 0602");
}
VerticalCombatExperienceReplicaWorld::VerticalCombatExperienceReplicaWorld(Actor& actor,PlayerCore& player,PartyMembershipUser& member,PartyRequestState& state,WorldAdmissionUser& admission,WorldCorpses& corpses,InventoryWorld& inventory,VerticalCombatView& view,VerticalCombatVehicleItemRecords& records,const Experience& curve,CharacterReplicaDelivery& delivery)
 :VerticalCombatExperienceDropWorld(actor,player,member,state,admission,corpses,inventory,view,records,curve),experience_delivery_(actor,player,delivery){}
void VerticalCombatExperienceReplicaWorld::send_experience_drop_database(Actor& actor,const net::Packet& packet){experience_delivery_.send(actor,packet);}
bool vertical_combat_map_bind(const MapDefinition& map,std::uint8_t country,const UserPosition& source,UserPosition& output,RandomSource& random){
 if(std::fegetround()!=FE_TONEAREST)throw std::logic_error("Map bind requires nearest floating rounding");
 if(!std::isfinite(source.x)||!std::isfinite(source.y)||!std::isfinite(source.z))throw std::invalid_argument("Map bind requires finite source coordinates");
 if(map.binds.empty()){
  output=source;const auto size=static_cast<float>(map.size);
  if(output.x<0 || output.x>=size || output.z<0 || output.z>=size){output.x=0;output.z=0;}return false;
 }
 std::size_t selected=0;float nearest=9999.0f;
 for(std::size_t i=0;i<map.binds.size();++i){
  const auto& bind=map.binds[i];if(bind.boxes.empty())continue;
  if(bind.country!=0 && bind.country!=3 && bind.country!=(country==0?1:2))continue;
  const auto& first=bind.boxes.front().minimum;
  if(!std::isfinite(first.x)||!std::isfinite(first.z))throw std::invalid_argument("Map bind requires finite box coordinates");
  volatile float dx=source.x-first.x,dz=source.z-first.z;
  const double square=static_cast<double>(dx)*dx+static_cast<double>(dz)*dz;
  const float distance=static_cast<float>(std::sqrt(square));
  if(distance<nearest){nearest=distance;selected=i;}
 }
 const auto next=[&](){const auto value=random.next();if(value<0 || value>32767)throw std::logic_error("Map bind requires MSVCRT rand values");return value;};
 const auto choose=next();const auto& boxes=map.binds[selected].boxes;
 if(boxes.empty()||boxes.size()>static_cast<std::size_t>(INT32_MAX))throw std::logic_error("Original map bind would divide by zero or overflow its count");
 const auto& box=boxes[static_cast<std::size_t>(choose)%boxes.size()];
 const auto divisor=[](float low,float high){if(!std::isfinite(low)||!std::isfinite(high))throw std::invalid_argument("Map bind requires finite box coordinates");const auto width=static_cast<double>(high)-static_cast<double>(low);if(width<static_cast<double>(INT32_MIN)||width>=2147483648.0)return INT32_MIN;const auto value=static_cast<std::int32_t>(std::trunc(width));if(value==0)throw std::logic_error("Original map bind would divide by zero");return value;};
 const auto z_random=next();const auto z_divisor=divisor(box.minimum.z,box.maximum.z);
 const auto z=static_cast<float>(z_random%z_divisor)+box.minimum.z;
 const auto y=box.minimum.y+0.9f;
 const auto x_random=next();const auto x_divisor=divisor(box.minimum.x,box.maximum.x);
 output={static_cast<float>(x_random%x_divisor)+box.minimum.x,y,z};return true;
}
bool vertical_combat_zone_bind(const ConfiguredMap& current,std::span<const ConfiguredMap> maps,std::uint8_t country,const UserPosition& source,UserPosition& output,std::int32_t& map,RandomSource& random){
 if(std::fegetround()!=FE_TONEAREST)throw std::logic_error("Zone bind requires nearest floating rounding");
 if(country>=current.settings.rebirth.size())throw std::out_of_range("Original zone bind country exceeds retained map settings");
 const auto& override=current.settings.rebirth[country];
 if(override.map>=0 && static_cast<std::size_t>(override.map)<maps.size()){
  const auto& target=maps[static_cast<std::size_t>(override.map)].definition;
  const auto size=static_cast<float>(target.size);const auto& p=override.position;
  if(p.x>=0 && p.x<size && p.z>=0 && p.z<size){output={p.x,p.y+0.9f,p.z};map=override.map;return true;}
 }
 UserPosition destination{};const auto found=vertical_combat_map_bind(current.definition,country,source,destination,random);
 output=destination;map=current.settings.id;return found;
}
VerticalCombatMapBindWorld::VerticalCombatMapBindWorld(Actor& actor,PlayerCore& player,PartyMembershipUser& member,PartyRequestState& state,WorldAdmissionUser& admission,WorldCorpses& corpses,InventoryWorld& inventory,VerticalCombatView& view,VerticalCombatVehicleItemRecords& records,const Experience& curve,CharacterReplicaDelivery& delivery,RandomSource& random,const ServerMaps& maps)
 :VerticalCombatExperienceReplicaWorld(actor,player,member,state,admission,corpses,inventory,view,records,curve,delivery),map_actor_(actor),map_random_(random),maps_(maps){}
void VerticalCombatMapBindWorld::bind_from_map(Actor& actor,std::uint16_t map,UserPosition& output){
 if(&actor!=&map_actor_)throw std::invalid_argument("Map bind bound to another actor");
 const auto* configured=maps_.find(map);if(!configured)throw std::logic_error("Map bind requires the exact live configured map");
 const auto source=actor_position(actor);(void)vertical_combat_map_bind(configured->definition,actor.appearance().identity.country,source,output,map_random_);
}
void VerticalCombatMapBindWorld::bind_from_zone(Actor& actor,UserPosition& output,std::int32_t& map){
 if(&actor!=&map_actor_)throw std::invalid_argument("Zone bind bound to another actor");
 const auto& current=rebirth_zone_map(actor);const auto catalog=maps_.all();
 if(std::none_of(catalog.begin(),catalog.end(),[&](const ConfiguredMap& entry){return &entry==&current;}))throw std::logic_error("Zone bind requires the retained current world map");
 const auto source=actor_position(actor);(void)vertical_combat_zone_bind(current,catalog,actor.appearance().identity.country,source,output,map,map_random_);
}
VerticalCombatLiveMapBindings::VerticalCombatLiveMapBindings(Actor& actor,ZoneEntryState& entry,WorldAdmissionUser& admission):actor_(actor),entry_(entry),admission_(admission){
 if(entry_.character_id!=actor_.id())throw std::invalid_argument("Live map position requires the same actor admission");
}
void VerticalCombatLiveMapBindings::require_actor(const Actor& actor) const{
 if(&actor!=&actor_ || entry_.character_id!=actor_.id())throw std::invalid_argument("Live map binding refers to another actor");
}
UserPosition VerticalCombatLiveMapBindings::position(const Actor& actor) const{
 require_actor(actor);
 if(admission_.zone && admission_.zone->users){
  const auto* current=admission_.zone->users->find_actor(actor.id());
  if(current && current!=&actor_)throw std::logic_error("Live bind zone contains another actor with the same ID");
  if(current){
   const auto linked=admission_.zone->users->position(actor.id());
   if(!linked)throw std::logic_error("Live bind actor has no zone position");
   const auto equal=[](float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);};
   if(!equal(linked->x,entry_.position.x)||!equal(linked->y,entry_.position.y)||!equal(linked->z,entry_.position.z))throw std::logic_error("Live bind movement and zone positions diverged");
  }
 }
 return entry_.position;
}
const ConfiguredMap& VerticalCombatLiveMapBindings::zone_map(const Actor& actor) const{
 require_actor(actor);if(!admission_.zone || !admission_.zone->map)throw std::logic_error("Live zone bind requires the retained zone and map");
 return *admission_.zone->map;
}
VerticalCombatLiveMapBindWorld::VerticalCombatLiveMapBindWorld(Actor& actor,PlayerCore& player,PartyMembershipUser& member,PartyRequestState& state,WorldAdmissionUser& admission,WorldCorpses& corpses,InventoryWorld& inventory,VerticalCombatView& view,VerticalCombatVehicleItemRecords& records,const Experience& curve,CharacterReplicaDelivery& delivery,RandomSource& random,const ServerMaps& maps,ZoneEntryState& entry)
 :VerticalCombatMapBindWorld(actor,player,member,state,admission,corpses,inventory,view,records,curve,delivery,random,maps),live_map_(actor,entry,admission){}
UserPosition VerticalCombatLiveMapBindWorld::actor_position(const Actor& actor){return live_map_.position(actor);}
const ConfiguredMap& VerticalCombatLiveMapBindWorld::rebirth_zone_map(Actor& actor){return live_map_.zone_map(actor);}
VerticalCombatRebirthMoveGameServices::VerticalCombatRebirthMoveGameServices(Actor& actor,ZoneEntryState& admission):actor_(actor),admission_(admission){
 if(admission.character_id!=actor.id())throw std::invalid_argument("MoveByGame requires the actor's retained position");
}
void vertical_combat_rebirth_party_move(Actor& actor,PartyMembershipUser& subject,PartyRegistryParty& party,const ZoneEntryState& admission,PartyMembershipSendWorld& world,std::array<std::uint8_t,2> scratch){
 if(subject.id!=actor.id() || admission.character_id!=actor.id() || subject.party!=&party)throw std::invalid_argument("Party move requires retained actor membership and position");
 net::Packet packet{scratch[0],scratch[1]};
 if(party.experience.kind==0){packet[0]=0x07;packet[1]=0x0c;}else if(party.experience.kind==1){packet[0]=0x12;packet[1]=0x0c;}
 const auto append=[&](std::uint32_t value,unsigned width){for(unsigned i=0;i<width;++i)packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
 append(actor.id(),4);append(subject.instance,2);append(std::bit_cast<std::uint32_t>(admission.position.x),4);append(std::bit_cast<std::uint32_t>(admission.position.y),4);append(std::bit_cast<std::uint32_t>(admission.position.z),4);
 auto* buffer=allocate_party_membership_send_buffer(packet,world);if(!buffer)return;
 world.lock(party);
 try {
  for(std::int32_t i=0;i<party.count;++i){
   if(i>=30)throw std::logic_error("Party move exceeds retained registry slots");
   auto* peer=raid_membership_slot(party,i);
   if(peer && peer!=&subject && peer->instance==subject.instance && peer->send_socket!=-1){buffer->references.fetch_add(1,std::memory_order_seq_cst);world.enqueue_send(*peer,*buffer);}
  }
 } catch(...) {world.unlock(party);release_party_membership_buffer(*buffer,world);throw;}
 world.unlock(party);release_party_membership_buffer(*buffer,world);
}
VerticalCombatRebirthPartyMoveServices::VerticalCombatRebirthPartyMoveServices(Actor& actor,ZoneEntryState& entry,PartyMembershipUser& member,PartyMembershipSendWorld& world):VerticalCombatRebirthMoveGameServices(actor,entry),actor_(actor),entry_(entry),member_(member),party_world_(world){if(member.id!=actor.id())throw std::invalid_argument("Party move requires the actor's membership");}
bool VerticalCombatRebirthPartyMoveServices::rebirth_party_present(const Actor& actor){if(&actor!=&actor_ || member_.id!=actor.id())throw std::invalid_argument("Party move bound to another actor");return member_.party!=nullptr;}
void VerticalCombatRebirthPartyMoveServices::send_party_move(Actor& actor){
 if(&actor!=&actor_ || member_.id!=actor.id() || !member_.party)throw std::logic_error("Party move requires live membership");
 auto* selected=member_.party;auto retained=retain_rebirth_move_party(*selected);if(!retained || retained.get()!=selected || member_.party!=selected)throw std::logic_error("Party move requires the selected retained registry party");
 vertical_combat_rebirth_party_move(actor,member_,*retained,entry_,party_world_,party_move_opcode_scratch());
}
void VerticalCombatRebirthMoveGameServices::move_by_game(Actor& actor){
 if(&actor!=&actor_ || admission_.character_id!=actor.id())throw std::invalid_argument("MoveByGame bound to another actor");
 const auto tick=rebirth_move_tick();auto& check=actor.movement_check();
 check.previous_time=tick;check.previous=admission_.position;check.total_distance=0;check.maximum_speed=0;
 check.back_time=check.previous_time;check.check_time=check.previous_time+5000U;check.back=admission_.position;
}
bool vertical_combat_rebirth_move_local(Actor& actor,RuntimeZone& zone,ZoneEntryState& admission,std::uint32_t location,UserPosition position,UserViewActorTransport& view,VerticalCombatRebirthLocalServices& services){
 if(!zone.map || !zone.users || !admission.bound || admission.character_id!=actor.id() || zone.map->settings.id<0 || zone.map->settings.id>65535 || admission.map!=zone.map->settings.id || zone.users->find_actor(actor.id())!=&actor)throw std::logic_error("Rebirth local move requires retained zone admission");
 if(location!=4)return false;
 if(!zone.map->definition.geometry().safe_position(position.x,position.z)){services.close_rebirth_move(actor,9);return false;}
 if(!vertical_move_user(actor,zone,admission,location,position,view,[&](auto reason){services.close_rebirth_move(actor,reason);}))return false;
 services.move_by_game(actor);
 if(services.rebirth_party_present(actor))services.send_party_move(actor);
 return true;
}
VerticalCombatRebirthMoveLocalServices::VerticalCombatRebirthMoveLocalServices(Actor& actor,WorldAdmissionUser& admission,ZoneEntryState& entry,PartyRequestState& state,UserViewActorTransport& view,VerticalCombatRebirthLocalServices& local):actor_(actor),admission_(admission),entry_(entry),state_(state),view_(view),local_(local){if(entry.character_id!=actor.id())throw std::invalid_argument("Rebirth move requires retained actor admission");}
bool VerticalCombatRebirthMoveLocalServices::move_rebirth_local(Actor& actor,std::uint16_t,const UserPosition& position){
 if(&actor!=&actor_ || entry_.character_id!=actor.id())throw std::invalid_argument("Rebirth local move bound to another actor");
 if(!admission_.zone)throw std::logic_error("Rebirth local move requires the current retained zone");
 return vertical_combat_rebirth_move_local(actor,*admission_.zone,entry_,state_.scene_state,position,view_,local_);
}
bool vertical_combat_rebirth_move(Actor& actor,std::uint16_t instance,const UserPosition& position,ActorActionWorld& actions,VerticalCombatRebirthMoveServices& services){
 const auto partner=actor.appearance().vehicle_partner;const auto mode=actor.appearance().vehicle_mode;
 if(partner!=0 && (mode==2 || mode==3)){
  if(auto* peer=actions.find_user(actor,partner)){
   if(peer->id()!=partner)throw std::logic_error("Rebirth move found a foreign vehicle peer");
   const auto lease=services.retain_move_peer(*peer);if(!lease)throw std::logic_error("Rebirth move requires the retained vehicle peer");
   if(mode==2){actor.appearance().vehicle_partner=0;peer->appearance().vehicle_mode=0;peer->appearance().vehicle_partner=0;peer->send_vehicle_partner();}
   else{peer->appearance().vehicle_partner=0;actor.appearance().vehicle_mode=0;actor.appearance().vehicle_partner=0;actor.send_vehicle_partner();}
  }
 }
 return services.move_rebirth_local(actor,instance,position);
}
VerticalCombatRebirthMoveWorld::VerticalCombatRebirthMoveWorld(Actor& actor,MobTargetingLifecycleWorld& world,ActorActionWorld& actions,VerticalCombatRebirthMoveServices& services):actor_(actor),world_(world),actions_(actions),services_(services){}
GameLogSubject VerticalCombatRebirthMoveWorld::subject(const Actor& actor) const{return world_.subject(actor);}
std::uint16_t VerticalCombatRebirthMoveWorld::instance(const Actor& actor) const{return world_.instance(actor);}
std::uint8_t VerticalCombatRebirthMoveWorld::presentation_motion(const Actor& actor) const{return world_.presentation_motion(actor);}
void VerticalCombatRebirthMoveWorld::presentation_motion(Actor& actor,std::uint8_t value){world_.presentation_motion(actor,value);}
void VerticalCombatRebirthMoveWorld::replace_resources(Actor& actor,PlayerResources value){world_.replace_resources(actor,value);}
std::optional<std::uint32_t> VerticalCombatRebirthMoveWorld::find_dead(Actor& actor,std::uint32_t id){return world_.find_dead(actor,id);}
void VerticalCombatRebirthMoveWorld::leave_dead(Actor& actor,std::uint32_t id){world_.leave_dead(actor,id);}
bool VerticalCombatRebirthMoveWorld::move_user(Actor& actor,std::uint16_t instance,const UserPosition& position){if(&actor!=&actor_)throw std::invalid_argument("Rebirth move bound to another actor");return vertical_combat_rebirth_move(actor,instance,position,actions_,services_);}
bool VerticalCombatRebirthMoveWorld::map_present(std::int32_t map){return world_.map_present(map);}
void VerticalCombatRebirthMoveWorld::zone_leave_user_move(Actor& actor,std::int32_t map,const UserPosition& position){world_.zone_leave_user_move(actor,map,position);}
void VerticalCombatRebirthMoveWorld::send_cell_view(Actor& actor,const net::Packet& packet){world_.send_cell_view(actor,packet);}
void VerticalCombatRebirthMoveWorld::send_recover_change(Actor& actor,PlayerResources resources){world_.send_recover_change(actor,resources);}
std::chrono::sys_seconds VerticalCombatRebirthMoveWorld::local_calendar(){return world_.local_calendar();}
void VerticalCombatRebirthMoveWorld::send_game_log(const net::Packet& packet){world_.send_game_log(packet);}
VerticalCombatRecoverySoloSoulServices::VerticalCombatRecoverySoloSoulServices(Actor& actor,PartyMembershipUser& member,PartyRequestState& state,VerticalCombatSoloSoulWorld& world)
 :VerticalCombatRecoveryPartySoulServices(actor,member,state,world),solo_actor_(actor),solo_member_(member),solo_state_(state),solo_world_(world){}
void VerticalCombatRecoverySoloSoulServices::expire_solo_soul(Actor& actor){
 if(&actor!=&solo_actor_ || actor.id()!=solo_member_.id)throw std::invalid_argument("Solo soul bound to another actor");
 if(solo_state_.zone==0 || actor.attack_state().mode!=1)return;
 if(!actor.death_metadata_bound())throw std::logic_error("Solo soul requires actor death metadata");
 if(actor.death_metadata().soul==2)return;
 if(actor.appearance().identity.grow==3 && actor.death_metadata().penalty!=-1){solo_world_.ultimate_soul(actor);return;}
 solo_world_.drop_experience(actor,actor.death_metadata().penalty);
 if(solo_state_.zone!=0){
  auto [position,map]=solo_world_.bind_scratch(actor);solo_world_.bind_from_zone(actor,position,map);
  mob_targeting_rebirth(actor,position,4,map,true,solo_world_.rebirth_world(actor));return;
 }
 auto& world=solo_world_.rebirth_world(actor);const auto map=solo_member_.instance;
 if(world.map_present(map)){
  auto [position,unused]=solo_world_.bind_scratch(actor);(void)unused;solo_world_.bind_from_map(actor,map,position);
  mob_targeting_rebirth(actor,position,4,solo_member_.instance,true,world);
 }else{
  const auto position=solo_world_.actor_position(actor);mob_targeting_rebirth(actor,position,4,map,true,world);
 }
}
void vertical_combat_recovery_zone(ZoneUsers& zone,std::uint32_t tick,VerticalCombatRecoveryZoneServices& services){
 zone.visit_all([&](Actor& actor){return services.retain(actor);},[&](Actor& actor){
  const auto id=actor.id();auto& loaded=services.loaded(actor);
  if(loaded.actor.get()!=&actor)throw std::logic_error("Recovery zone requires the same retained actor");
  auto& next=services.next_recovery(actor);
  const auto validate=[&](){if(zone.find_actor(id)!=&actor)throw std::logic_error("Recovery zone actor membership changed during continuation");};
  validate();static_cast<void>(vertical_combat_recovery(loaded,tick,next));validate();
  services.expire_effects(actor,tick);validate();services.update_duel(actor,tick);validate();services.update_rebirth(actor,tick);
 });
}
void vertical_combat_scheduler_zone(ZoneUsers& zone,std::uint32_t tick,VerticalCombatSchedulerZoneServices& services){
 zone.visit_all([&](Actor& actor){return services.retain(actor);},[&](Actor& actor){
  auto& world=services.world(actor);
  static_cast<void>(vertical_combat_scheduler_equipment(actor,tick,world));
 });
}
}
