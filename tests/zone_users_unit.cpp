#include "eternity/game/actor.hpp"
#include "eternity/game/maps.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Scene final:ActorScene,ActorEffectDatabase {
    std::vector<net::Packet> effect_database_packets;
    void send_effect_database(const net::Packet& packet) noexcept override {effect_database_packets.push_back(packet);}
    std::uint32_t id=0;ZoneUsers* zone=nullptr;std::vector<std::uint32_t>* reveals=nullptr;std::function<void()> on_reveal;
    void send_self(const net::Packet& packet) noexcept override {
        if(packet.size()==6 && packet[0]==0x0e && packet[1]==5) {reveals->push_back(id);if(on_reveal) on_reveal();}
        else if(packet!=net::Packet{0x10,2,1}) std::terminate();
    }
    void unexpected() {throw std::runtime_error("Unexpected zone-user scene operation");}
    void send_nearby(const net::Packet&) override {unexpected();}
    void send_cell_view(const net::Packet&) override {}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return {};}
    void release_effect_id(std::uint32_t) noexcept override {}
    std::uint32_t tick_ms() const noexcept override {return 0;}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {return zone;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected zone-users oracle path");std::ifstream input(argv[1]);require(input.good(),"Cannot open zone-users oracle");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;
        SkillDefinition hide;hide.id=200;hide.level=1;hide.effect=4;hide.detail=88;
        const double nan=std::numeric_limits<double>::quiet_NaN(),infinity=std::numeric_limits<double>::infinity();
        const std::array<std::array<double,2>,12> offsets={{{0,0},{3,4},{5,0},{5.000007629394531,0},{4.999992370605469,0},
            {nan,0},{infinity,0},{1e20,1e20},{-3,-4},{0,5},{0,0},{255,0}}};
        unsigned int checked=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);
            unsigned int s=0,range=0,count=0;std::int32_t width=0,height=0,mode=0,sx=0,sz=0;
            require(static_cast<bool>(row>>s>>width>>height>>mode>>sx>>sz>>range>>count),"Malformed zone-users oracle");
            std::vector<std::uint32_t> expected(count);for(auto& id:expected) row>>id;
            std::vector<std::uint32_t> revealed;std::array<Scene,24> scenes;std::vector<std::unique_ptr<Actor>> actors;
            for(unsigned int i=0;i<24;++i) {
                scenes[i].id=9001+i;scenes[i].reveals=&revealed;
                EffectStatus status;status.shape=13;status.skills.equipment.transformed=true;
                actors.push_back(std::make_unique<Actor>(9001+i,status,PlayerResources{0,0,0},std::vector<PersistedEffect>{{200,1,30}},scenes[i],ActorEquipmentRules{items,enchant,mobs}));
                auto& actor=*actors.back();actor.effects().append(1000+i,{{200,1,30},10000,12000,false},hide);
                actor.appearance().identity.country=static_cast<std::uint8_t>((s+i)%3);actor.appearance().visible=(s+i)%13!=0;actor.appearance().attackable=(s+i)%7!=0;
                actor.attack_state().mode=(s+i)%11==0;
                auto& relation=actor.relations();relation.duel_state=(s+i)%5==0?4:static_cast<std::int32_t>((s+i)%4);relation.duel_opponent=(s+i)%2?9001:9999;
                relation.guild_id=i==0?1:i%4==0?2:3;relation.opposing_guild_id=i==0?2:i%4==0?1:5;relation.guild_duel_ready=i==0?(s&8)!=0:(s+i)%2!=0;
            }
            ZoneUsers zone(width,height,mode);
            for(unsigned int i=0;i<24;++i) {
                scenes[i].zone=&zone;
                const auto x=i==0?sx:static_cast<std::int32_t>((s+i*2)%static_cast<unsigned int>(width));
                const auto z=i==0?sz:static_cast<std::int32_t>((s*3+i)%static_cast<unsigned int>(height));
                const auto& delta=offsets[(s+i)%offsets.size()];
                const UserPosition position=i==0?UserPosition{100.25F,-9000,200.75F}:
                    UserPosition{static_cast<float>(100.25+delta[0]),9000,static_cast<float>(200.75+delta[1])};
                zone.attach(*actors[i],x,z,position);
            }
            SkillDefinition detector;detector.effect=4;detector.detail=93;detector.apply_range=static_cast<std::uint8_t>(range);
            apply_active_effect(actors[0]->status(),detector,0,*actors[0]);const auto context=" at zone-user vector "+std::to_string(s);
            require(revealed==expected,"Hidden-user query order/filter differs"+context);
            for(const auto& actor:actors) {
                const bool was_revealed=std::find(expected.begin(),expected.end(),actor->id())!=expected.end();
                require(actor->status().shape==(was_revealed?0:13) && actor->effects().size()==(was_revealed?0:1) && actor->replica().snapshot().size()==(was_revealed?0:1),"Hidden-user query did not update actual actor/replica"+context);
            }
            std::string extra;require(!(row>>extra),"Unexpected zone-user oracle data");++checked;
        }
        require(checked==128,"Zone-user oracle coverage changed");
        std::array<Scene,3> scenes;std::vector<std::unique_ptr<Actor>> actors;std::vector<std::uint32_t> revealed;
        for(std::uint32_t i=0;i<3;++i) {
            scenes[i].id=9001+i;scenes[i].reveals=&revealed;EffectStatus status;status.shape=13;
            actors.push_back(std::make_unique<Actor>(9001+i,status,PlayerResources{0,0,0},std::vector<PersistedEffect>{{200,1,30}},scenes[i],ActorEquipmentRules{items,enchant,mobs}));
            actors.back()->appearance().identity.country=i==0?0:1;actors.back()->effects().append(1000+i,{{200,1,30},10000,12000,false},hide);
        }
        {
            ZoneUsers zone(4,4,0);for(auto& scene:scenes) scene.zone=&zone;
            zone.attach(*actors[0],0,0,{0,0,0});zone.attach(*actors[1],3,3,{0,0,0});zone.attach(*actors[2],1,1,{3,0,4});
            zone.move(9002,1,1,{0,0,0});
            scenes[2].on_reveal=[&]{require(zone.detach(9003),"Cannot detach current queried user");};
            zone.detect_hidden(9001,5);
            require(revealed==std::vector<std::uint32_t>{9003,9002} && zone.size()==2 && !actors[2]->status().in_zone,"Zone movement/current-node removal changed traversal");
            require(!zone.detach(9999),"Detached absent user");zone.move(9001,3,3,{10,0,20});
            require(same_team(*actors[0],*actors[0],0),"Self stopped being same team");
        }
        require(!actors[0]->status().in_zone && !actors[1]->status().in_zone,"Zone teardown retained actor membership");
        revealed.clear();for(auto& scene:scenes) scene.on_reveal={};
        EffectStatus hidden;hidden.shape=13;hidden.skills.equipment.transformed=true;
        actors[1]=std::make_unique<Actor>(9002,hidden,PlayerResources{0,0,0},std::vector<PersistedEffect>{{200,1,30}},scenes[1],ActorEquipmentRules{items,enchant,mobs});
        actors[1]->appearance().identity.country=1;actors[1]->effects().append(2001,{{200,1,30},10000,12000,false},hide);
        {
            ZoneUsers zone(MapGeometry(121),0);for(auto& scene:scenes) scene.zone=&zone;
            require(zone.attach_at_position(*actors[0],{39,0,0}) && zone.attach_at_position(*actors[1],{120,0,0}),"Resolved-position attachment failed");
            require(!zone.attach_at_position(*actors[2],{121,0,0}) && !actors[2]->status().in_zone,"Map boundary admitted an actor");
            require(zone.move_at_position(9002,{90,0,0}) && zone.move_at_position(9002,{79,0,0}),"Valid map movement failed");
            zone.detect_hidden(9001,255);require(revealed.empty(),"Movement ignored the original cell view overlap");
            require(zone.move_at_position(9002,{69,0,0}),"Movement did not cross the old view boundary");zone.detect_hidden(9001,255);
            require(revealed==std::vector<std::uint32_t>{9002},"Resolved movement did not change query membership");
            require(!zone.move_at_position(9002,{-1,999,0}) && zone.position(9002)->x==69 && zone.position(9002)->y==0,"Rejected movement changed stored position");
        }
        std::cout<<checked<<" original hidden-user queries plus native movement and live unlink passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
