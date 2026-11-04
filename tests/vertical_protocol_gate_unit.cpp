#include "eternity/game/vertical_protocol.hpp"
#include "eternity/game/zone_entry.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
void check(bool ok,const char* msg) {if(!ok) throw std::runtime_error(msg);}
struct Scene final:ActorScene {
    void send_self(const net::Packet&) noexcept override {} void send_nearby(const net::Packet&) override {} void send_cell_view(const net::Packet&) override {}
    std::optional<bool> party_kind() const noexcept override {return {};} void send_party(const net::Packet&) noexcept override {} void reset_damage() override {}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return {};} void release_effect_id(std::uint32_t) noexcept override {}
    std::uint32_t tick_ms() const noexcept override {return 1;} void stop_target_mob(std::uint32_t) override {} void taunt(std::uint16_t,float) override {}
    ZoneUsers* user_zone() override {return nullptr;} UserShapeSocial user_shape_social() override {return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {}
};
struct Services final:WorldAdmissionServices {
    RuntimeZone& zone;Actor& actor;ZoneEntryState& state;ZoneActorEntryServices& entry;std::vector<std::string> events;
    Services(RuntimeZone& z,Actor& a,ZoneEntryState& s,ZoneActorEntryServices& e):zone(z),actor(a),state(s),entry(e) {}
    void packets(WorldAdmissionUser&) override {events.emplace_back("packets");} RuntimeZone* select_zone(WorldAdmissionUser&) override {events.emplace_back("select");return &zone;}
    void register_world(WorldAdmissionUser&) override {events.emplace_back("register");} bool enter_zone(WorldAdmissionUser&,RuntimeZone& z) override {events.emplace_back("enter");return enter_zone_actor(z,actor,state,entry);}
    void close(WorldAdmissionUser&,std::uint32_t) override {events.emplace_back("close");} void diagnostic(WorldAdmissionUser&,WorldAdmissionError) override {events.emplace_back("diagnostic");}
    std::optional<std::uint32_t> rank_window(RuntimeZone&) override {throw std::logic_error("unexpected rank");} bool lock_guild(WorldAdmissionUser&) override {throw std::logic_error("unexpected guild");} void unlock_guild(WorldAdmissionUser&) override {throw std::logic_error("unexpected guild");}
};
int main(int argc,char** argv) {try {
    check(argc==2,"missing original gate fixture");std::ifstream in(argv[1]);check(in.good(),"fixture missing");
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;EffectStatus status;Actor actor(42,status,{1,1,1},{},scene,{items,enchant,mobs});
    ConfiguredMap map;map.settings.id=20;map.definition.size=81;RuntimeZone zone;zone.map=&map;zone.users=std::make_unique<ZoneUsers>(map.definition.geometry(),0);zone.environment=ZoneEnvironment::prepare(map);
    ZoneEntryState position;position.character_id=42;position.position={40,0,40};ZoneActorEntryServices entry;
    entry.send_view=[](CellCoordinates,net::Packet){};entry.cell_info=[](CellCoordinates){};entry.obelisks=[](){};entry.nprotect=[](){};entry.diagnostic=[](){};entry.close=[](std::uint32_t){};
    Services services(zone,actor,position,entry);WorldAdmission world(services);WorldAdmissionUser user;user.user_id=11;user.map=20;world.track_selection(user);VerticalEntrySession session(world,user);
    check(session.wait(),"wait failed");check(user.location==2 && world.waiting_count()==1,"wait ownership failed");update_vertical_admission(world);check(user.location==3 && world.zone_waiting_count(zone)==1,"world update failed");
    std::string line;unsigned cases=0;while(std::getline(in,line)){if(line.empty()||line[0]=='#')continue;std::istringstream row(line);std::uint32_t location,opcode,suffix;std::string expected;row>>location>>opcode>>suffix>>expected;
        user.location=location;net::Packet packet{static_cast<std::uint8_t>(opcode),static_cast<std::uint8_t>(opcode>>8),static_cast<std::uint8_t>(suffix),static_cast<std::uint8_t>(suffix>>8),static_cast<std::uint8_t>(suffix>>16),static_cast<std::uint8_t>(suffix>>24)};
        if(expected=="ENTER") {position.position.x=81;check(session.receive(packet)==VerticalEntryRequest::rejected && world.zone_waiting_count(zone)==1,"original gate did not invoke real failed entry");} else check(session.receive(packet)==VerticalEntryRequest::unhandled,"original gate accepted unrelated request");++cases;}
    check(cases==60,"gate fixture coverage changed");user.location=3;position.position.x=40;check(session.receive({})==VerticalEntryRequest::unhandled,"short packet accepted");
    check(session.receive(net::Packet{1,2,0xaa})==VerticalEntryRequest::entered,"0201 did not enter");check(user.location==4 && zone.users->size()==1 && actor.status().in_zone && world.zone_waiting_count(zone)==0,"actor membership not retained");
    check(services.events==std::vector<std::string>{"register","select","enter","enter","enter"},"admission event order changed");check(zone.users->detach(42),"detach failed");
    std::cout<<cases<<" original gate vectors and actual admission composition passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
