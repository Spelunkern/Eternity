#include "eternity/game/vertical_movement_dispatch.hpp"
#include <bit>
#include <iostream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct Scene final:ActorScene {
    ZoneUsers* zone=nullptr;unsigned clocks=0;std::vector<net::Packet> sent;
    void send_self(const net::Packet& p) noexcept override {sent.push_back(p);}
    void send_nearby(const net::Packet&) override {}
    void send_cell_view(const net::Packet&) override {}
    std::optional<bool> party_kind() const noexcept override{return {};}
    void send_party(const net::Packet&) noexcept override {}
    void reset_damage() override {}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override{return {};}
    void release_effect_id(std::uint32_t) noexcept override {}
    std::uint32_t tick_ms() const noexcept override{return 100;}
    void stop_target_mob(std::uint32_t) override {}
    void taunt(std::uint16_t,float) override {}
    ZoneUsers* user_zone() override{return zone;}
    UserShapeSocial user_shape_social() override{return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {}
};
void append(net::Packet& p,std::uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i)p.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
net::Packet packet(UserPosition p){net::Packet out{1,5,0x81,0x34,0x12};for(float v:{p.x,p.y,p.z})append(out,std::bit_cast<std::uint32_t>(v));return out;}
}
int main(){
 try {
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;
    Actor actor(101,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,{items,enchant,mobs});
    ConfiguredMap map;map.settings.id=20;map.definition.size=120;
    RuntimeZone zone;zone.map=&map;zone.users=std::make_unique<ZoneUsers>(map.definition.geometry(),0);zone.environment=ZoneEnvironment::prepare(map);scene.zone=zone.users.get();
    ZoneEntryState admission;admission.character_id=101;admission.position={10,2,10};
    ZoneActorEntryServices services;services.send_view=[](auto,auto){};services.cell_info=[](auto){};services.obelisks=[]{};services.nprotect=[]{};services.diagnostic=[]{};services.close=[](auto){throw std::runtime_error("Entry unexpectedly closed");};
    require(enter_zone_actor(zone,actor,admission,services),"actual admission failed");
    // Test-only bridge: Actor is non-const and owns this object. Production needs
    // an explicit mutable owner API; this cast is not an integration solution.
    auto& check=const_cast<ZoneMovementCheck&>(actor.movement_check());
    unsigned calls=0;auto clock=[&]{++calls;return 200U;};
    const auto result=vertical_movement_packet_check_after_gate(actor,zone,admission,check,packet({20,7,10}),5,clock);
    require(result.motion==0x81 && result.direction==0x1234 && result.destination.x==20 && result.destination.y==7 && result.position_before_check.x==10 && result.accepted_by_speed_check,"0501 decoding or check failed");
    require(calls==1 && actor.movement_check().previous.x==20 && zone.users->position(101)->x==10,"check did not use owner or moved before continuation");
    check.total_distance=51;check.maximum_speed=0;check.back_time=5100;check.check_time=5100;check.back={9,3,9};
    calls=0;auto due=[&]{++calls;return 5100U;};
    const auto rejected=vertical_movement_packet_check_after_gate(actor,zone,admission,check,packet({20,7,10}),0,due);
    require(!rejected.accepted_by_speed_check && calls==2 && check.previous.x==9 && zone.users->position(101)->x==10,"rejected check reset/membership changed");
    ZoneMovementCheck copy=check;calls=0;bool failed=false;
    try{vertical_movement_packet_check_after_gate(actor,zone,admission,copy,packet({20,7,10}),5,clock);}catch(const std::logic_error&){failed=true;}
    require(failed && calls==0,"copied movement owner accepted");
    auto invalid=packet({20,7,10});invalid.resize(16);failed=false;
    try{vertical_movement_packet_check_after_gate(actor,zone,admission,check,invalid,5,clock);}catch(const std::invalid_argument&){failed=true;}
    require(failed && calls==0,"short packet reached clock");invalid=packet({20,7,10});invalid[0]=2;failed=false;
    try{vertical_movement_packet_check_after_gate(actor,zone,admission,check,invalid,5,clock);}catch(const std::invalid_argument&){failed=true;}
    require(failed && calls==0,"wrong opcode reached clock");
    require(zone.users->detach(101),"detach failed");failed=false;
    try{vertical_movement_packet_check_after_gate(actor,zone,admission,check,packet({20,7,10}),5,clock);}catch(const std::logic_error&){failed=true;}
    require(failed && calls==0,"stale admission reached clock");
    std::cout<<"0501 speed-check stage with admitted actor, zone and actual movement owner passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
