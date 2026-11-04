#include "eternity/game/vertical_movement_prefix.hpp"
#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
namespace {
void require(bool v,const std::string& m){if(!v)throw std::runtime_error(m);}
std::string hex(const net::Packet& p){std::ostringstream s;s<<std::hex<<std::setfill('0');for(auto x:p)s<<std::setw(2)<<unsigned(x);return s.str();}
struct Scene final:ActorScene {
    ZoneUsers* zone=nullptr;std::vector<std::string> events;
    void send_self(const net::Packet& p) noexcept override {events.push_back("S"+hex(p));}
    void send_nearby(const net::Packet&) override {} void send_cell_view(const net::Packet&) override {}
    std::optional<bool> party_kind() const noexcept override{return {};}
    void send_party(const net::Packet&) noexcept override {} void reset_damage() override {}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override{return {};}
    void release_effect_id(std::uint32_t) noexcept override {} std::uint32_t tick_ms() const noexcept override{return 100;}
    void stop_target_mob(std::uint32_t) override {} void taunt(std::uint16_t,float) override {}
    ZoneUsers* user_zone() override{return zone;} UserShapeSocial user_shape_social() override{return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {}
};
struct Services final:VerticalMovementPrefixServices {
    Scene& scene;std::uint16_t dir;unsigned stands=0;
    Services(Scene& s,std::uint16_t d):scene(s),dir(d){}
    void status_stand(Actor& a) override {++stands;require(a.presentation_motion()==0,"fixture requires actual no-op stand path");}
    std::uint16_t direction(const Actor&) override{return dir;}
    void send_view_action(Actor&,const net::Packet& p) override {scene.events.push_back("V"+hex(p));}
};
void put(net::Packet& p,std::uint32_t n,unsigned size){for(unsigned i=0;i<size;++i)p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
std::uint32_t fromhex(const std::string& s){if(s.size()!=8)throw std::runtime_error("bad oracle float");std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=static_cast<std::uint32_t>(std::stoul(s.substr(i*2,2),nullptr,16))<<(8*i);return n;}
}
int main(int argc,char** argv) {
 try {
    require(argc==2,"oracle path required");std::ifstream in(argv[1]);require(bool(in),"oracle unavailable");
    ItemDefinitions items;EnchantValues ench;MobAttackTemplates mobs;std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);unsigned index,life,soul,total,dir,final_life,target,prev,next,recovery;std::string distance,events,request_x;
        require(bool(row>>index>>life>>soul>>distance>>total>>dir>>events>>final_life>>target>>prev>>next>>recovery>>request_x),"bad oracle row");
        Scene scene;Actor actor(0x11223344,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,{items,ench,mobs});
        actor.initialize_death_metadata({0,soul,0,0});ConfiguredMap map;map.settings.id=20;map.definition.size=120;
        RuntimeZone zone;zone.map=&map;zone.users=std::make_unique<ZoneUsers>(map.definition.geometry(),0);zone.environment=ZoneEnvironment::prepare(map);scene.zone=zone.users.get();
        ZoneEntryState entry;entry.character_id=actor.id();entry.position={10,2,10};ZoneActorEntryServices ep;
        ep.send_view=[](auto,auto){};ep.cell_info=[](auto){};ep.obelisks=[]{};ep.nprotect=[]{};ep.diagnostic=[]{};ep.close=[](auto){throw std::runtime_error("entry close");};
        require(enter_zone_actor(zone,actor,entry,ep),"entry failed");scene.events.clear();
        actor.attack_state().mode=static_cast<std::int32_t>(life);actor.attack_state().recovery=0;actor.attack_state().previous_tick=11;actor.attack_state().next_tick=12;actor.status().current_target_type=7;
        net::Packet packet{1,5,0x81,0x34,0x12};const auto dx=std::bit_cast<float>(fromhex(distance));for(float v:{10.0F+dx,9.0F,10.0F})put(packet,std::bit_cast<std::uint32_t>(v),4);
        Services service(scene,static_cast<std::uint16_t>(dir));const auto result=vertical_movement_prefix(actor,zone,entry,packet,service);
        const bool speed_rejected=events.rfind("T|T|V",0)==0;
        if(speed_rejected){require(result==VerticalMovementPrefixResult::check_required,"speed correction skipped gate");vertical_movement_speed_correction(actor,zone,entry,packet,service);}
        const bool life_blocked=events=="-",distance_corrected=events.rfind("V",0)==0;
        require(service.stands==1 && (life_blocked?result==VerticalMovementPrefixResult::life_blocked:distance_corrected?result==VerticalMovementPrefixResult::distance_corrected:result==VerticalMovementPrefixResult::check_required),"gate mismatch case "+std::to_string(index));
        const auto first=events.find('V');std::string actual=scene.events.empty()?"-":scene.events[0]+"|"+scene.events.at(1);
        require(actual==(first==std::string::npos?"-":events.substr(first)),"correction packet mismatch case "+std::to_string(index));
        require(actor.attack_state().mode==static_cast<std::int32_t>(final_life) && actor.status().current_target_type==target && actor.attack_state().previous_tick==prev && actor.attack_state().next_tick==next && actor.attack_state().recovery==static_cast<std::int32_t>(recovery),"attack state mismatch case "+std::to_string(index));
        require(hex(net::Packet(packet.begin()+5,packet.begin()+9))==request_x && zone.users->position(actor.id())->x==10,"request or zone position mismatch case "+std::to_string(index));++count;
    }
    require(count==576,"oracle count mismatch");std::cout<<count<<" original PacketPC gate/correction cases passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
