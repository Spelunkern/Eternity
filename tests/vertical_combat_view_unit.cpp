#include "eternity/game/vertical_combat_view.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
struct Scene:ActorScene {
 void send_self(const net::Packet&) noexcept override{} void send_cell_view(const net::Packet&) override{} void release_effect_id(std::uint32_t) noexcept override{}
 std::optional<bool> party_kind() const noexcept override{return {};} void send_nearby(const net::Packet&) override{} void send_party(const net::Packet&) noexcept override{} void reset_damage() override{}
 std::optional<std::uint32_t> allocate_effect_id() noexcept override{return {};} std::uint32_t tick_ms() const noexcept override{return 0;}
 void stop_target_mob(std::uint32_t) override{} void taunt(std::uint16_t,float) override{} ZoneUsers* user_zone() override{return nullptr;} UserShapeSocial user_shape_social() override{return {};}
 void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override{}
};
struct Area:UserPriorityAreaServices {
 struct Buffer:UserViewBuffer{net::Packet packet;explicit Buffer(const net::Packet& p):packet(p){}};
 Actor& source;ZoneUsers& zone;bool fail=false;unsigned allocations=0;std::vector<std::uint32_t> immediate_ids,priority_ids;
 Area(Actor& a,ZoneUsers& z):source(a),zone(z){}
 std::unique_ptr<UserViewBuffer> allocate(const net::Packet& p)override{++allocations;if(fail)return {};return std::make_unique<Buffer>(p);}
 std::shared_ptr<void> retain(Actor& a)override{return {&a,[](void*){}};}
 UserPosition origin()override{const auto p=zone.position(source.id());if(!p)throw std::runtime_error("lost origin");return *p;}
 void immediate(Actor& actor,UserViewBuffer& buffer)override{if(dynamic_cast<Buffer&>(buffer).packet!=net::Packet({3,5,7}))throw std::runtime_error("immediate payload");immediate_ids.push_back(actor.id());}
 void priority(Actor& actor,UserViewBuffer& buffer,std::uint8_t priority,float distance)override{if(dynamic_cast<Buffer&>(buffer).packet!=net::Packet({3,5,7})||priority!=5||distance>60.0f)throw std::runtime_error("priority arguments");priority_ids.push_back(actor.id());}
};
}
int main(){try{
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};Scene scene;
 Actor source(42,{}, {},{},scene,rules),near(43,{}, {},{},scene,rules),edge(44,{}, {},{},scene,rules),far(45,{}, {},{},scene,rules),foreign(46,{}, {},{},scene,rules),replacement(42,{}, {},{},scene,rules);
 ZoneUsers zone(5,5,0);zone.attach(source,2,2,{80,0,80});zone.attach(near,2,2,{90,0,80});zone.attach(edge,3,2,{140,0,80});zone.attach(far,3,2,{141,0,80});Area area(source,zone);VerticalCombatView view(source,zone,5,5,area);
 const net::Packet packet{3,5,7};view.send(source,packet);
 if(area.allocations!=1||area.immediate_ids!=std::vector<std::uint32_t>{42}||area.priority_ids!=std::vector<std::uint32_t>({43,44}))throw std::runtime_error("actual priority area recipients");
 area.fail=true;view.send(source,packet);if(area.allocations!=2||area.immediate_ids.size()!=1||area.priority_ids.size()!=2)throw std::runtime_error("allocation failure delivery");
 area.fail=false;area.immediate_ids.clear();area.priority_ids.clear();view.send(source,packet,43);
 if(area.allocations!=3||area.immediate_ids!=std::vector<std::uint32_t>({42,43})||area.priority_ids!=std::vector<std::uint32_t>({44}))throw std::runtime_error("source and target immediate delivery");
 bool rejected=false;try{view.send(foreign,packet);}catch(const std::invalid_argument&){rejected=true;}if(!rejected||area.allocations!=3)throw std::runtime_error("foreign actor accepted");
 zone.detach(42);view.send(source,packet);if(area.allocations!=3)throw std::runtime_error("detached source delivered");
 source.status().in_zone=true;rejected=false;try{view.send(source,packet);}catch(const std::logic_error&){rejected=true;}source.status().in_zone=false;if(!rejected)throw std::runtime_error("foreign zone accepted");
 zone.attach(replacement,2,2,{80,0,80});rejected=false;try{view.send(source,packet);}catch(const std::logic_error&){rejected=true;}if(!rejected)throw std::runtime_error("recycled identity accepted");
 std::cout<<"combat priority view: immediate source, radius, allocation and identity guards passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
