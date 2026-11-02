#include "eternity/game/effect_cycle.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::uint32_t get(const net::Packet& packet,std::size_t offset,unsigned int length) {std::uint32_t value=0;for(unsigned int i=0;i<length;++i) value|=static_cast<std::uint32_t>(packet.at(offset+i))<<(8*i);return value;}
std::uint32_t flags(const EffectStatus& status) {std::uint32_t mask=status.zero_attack_attribute_buff?1U:0U;for(std::size_t i=0;i<8;++i) if(status.skills.equipment.attack_attribute_buffs[i]==1) mask|=1U<<(i+1);return mask;}
struct World final:EffectRemovalWorld,EffectObserver {
    EffectCycle& cycle;EffectStatus& status;std::vector<SkillDefinition>& definitions;const ItemDefinitions items;
    std::vector<std::array<std::int64_t,4>> events;std::optional<bool> party;unsigned int mutation=0,original_count=0;bool mutated=false,valid=true;
    World(EffectCycle& cycle_,EffectStatus& status_,std::vector<SkillDefinition>& definitions_):cycle(cycle_),status(status_),definitions(definitions_) {}
    std::optional<std::uint32_t> allocate_id() noexcept override {return std::nullopt;}
    void release_id(std::uint32_t id) noexcept override {
        events.push_back({4,id,status.skills.equipment.attack_attribute,flags(status)});
        if(!mutated) {mutated=true;if(mutation==1) party.reset();else if(mutation==2 && party) party=true;else if(mutation==3) {
            const auto& definition=definitions[original_count];cycle.append(1000+original_count,{{definition.id,1,30},10000,12000,false},definition);
        }}
    }
    std::uint32_t tick_ms() const noexcept override {return 13001;}
    void erase_persisted(const PersistedEffect& effect) noexcept override {events.push_back({1,effect.skill_id,effect.level,0});}
    void send_self(const net::Packet& packet) noexcept override {if(packet.size()!=6 || get(packet,0,2)!=0x50e) valid=false;else events.push_back({2,get(packet,2,4),0,0});}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void send_party(const net::Packet& packet) noexcept override {
        if(packet.size()!=9 || get(packet,2,4)!=9001 || (get(packet,0,2)!=0xc05 && get(packet,0,2)!=0xc10)) valid=false;
        else events.push_back({5,get(packet,6,2),packet[8],get(packet,0,2)==0xc10});
    }
    void maximum_changed(EffectStatus& value,std::uint8_t resource) override {require(resource==0,"Unexpected resource in element fixture");events.push_back({3,value.skills.equipment.base.maximum.hp,0,0});}
    void replace_attack_elements(EffectStatus& value,std::uint8_t attribute) override {cycle.replace_attack_elements(attribute,value,{},items,*this,*this);}
    void unexpected() {throw std::runtime_error("Unexpected delegated world operation");}
    void speed_changed(EffectStatus&) override {unexpected();}
    void shape_changed(EffectStatus&) override {unexpected();}
    void run_mode_changed(EffectStatus&,bool) override {unexpected();}
    void stop_attack(EffectStatus&) override {unexpected();}
    void stop_target_mob(EffectStatus&,std::uint32_t) override {unexpected();}
    void taunt(EffectStatus&,std::uint16_t,float) override {unexpected();}
    void detect_hidden(EffectStatus&,const SkillDefinition&) override {unexpected();}
    void rebuild_attack(EffectStatus&,bool) override {unexpected();}
    void user_shape_changed(EffectStatus&) override {unexpected();}
    void finish_periodic_heal(EffectStatus&,const SkillDefinition&,const EffectSource&,const std::array<std::int32_t,3>&) override {unexpected();}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected element replacement oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open element oracle");std::string line;unsigned int checked=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);
            unsigned int scenario=0,mode=0,attribute=0,mask=0,mutation=0,count=0;int group=-1;
            require(static_cast<bool>(row>>scenario>>mode>>group>>attribute>>mask>>mutation>>count),"Malformed element vector");
            std::vector<SkillDefinition> definitions(count+1);EffectStatus status;status.skills.equipment.base.maximum.hp=1000;status.skills.equipment.attack_attribute=attribute;
            status.zero_attack_attribute_buff=(mask&1)!=0;for(std::size_t i=0;i<8;++i) status.skills.equipment.attack_attribute_buffs[i]=static_cast<std::uint8_t>((mask>>(i+1))&1);
            EffectCycle cycle(9001);World world(cycle,status,definitions);world.original_count=count;world.mutation=mutation;if(group>=0) world.party=group!=0;
            for(unsigned int i=0;i<count;++i) {
                auto& definition=definitions[i];unsigned int detail=0,effect=0,element=0,offline=0;row>>detail>>effect>>element>>offline;
                definition.id=static_cast<std::uint16_t>(200+i);definition.level=1;definition.detail=static_cast<std::uint16_t>(detail);definition.effect=static_cast<std::uint8_t>(effect);
                definition.attribute=static_cast<std::uint8_t>(element);definition.offline_countdown=offline!=0;definition.abilities[0]={1,1};
                cycle.append(1000+i,{{definition.id,1,30},10000,12000,definition.offline_countdown},definition);
            }
            auto& appended=definitions[count];appended.id=static_cast<std::uint16_t>(200+count);appended.level=1;appended.detail=94;appended.effect=4;appended.attribute=3;
            if(mode==2) {cycle.remove_weapon_elements(status,{},world.items,world,world);status.skills.equipment.attack_attribute=0;}else if(mode) cycle.initialize(status,world);else {SkillDefinition incoming;incoming.id=9999;incoming.level=1;incoming.detail=94;incoming.effect=4;incoming.attribute=2;apply_active_effect(status,incoming,0,world);}
            unsigned int remaining=0,hp=0,expected_attribute=0,expected_mask=0;row>>remaining>>hp>>expected_attribute>>expected_mask;
            const auto context=" at element scenario "+std::to_string(scenario);
            require(cycle.size()==remaining && status.skills.equipment.base.maximum.hp==hp && status.skills.equipment.attack_attribute==expected_attribute && flags(status)==expected_mask,"Element state differs"+context);
            std::vector<std::uint32_t> expected_ids(remaining);for(auto& id:expected_ids) row>>id;std::vector<std::uint32_t> actual_ids;
            for(const auto& packet:cycle.initial_packets(10000)) for(std::size_t offset=3;offset<packet.size();offset+=11) actual_ids.push_back(get(packet,offset,4));
            require(actual_ids==expected_ids,"Surviving element list order differs"+context);
            std::size_t events=0;row>>events;require(world.valid && world.events.size()==events,"Element notification count differs"+context+": "+std::to_string(world.events.size())+" != "+std::to_string(events));
            for(const auto& event:world.events) for(const auto value:event) {std::int64_t original=0;require(static_cast<bool>(row>>original) && value==original,"Element notification order/state differs"+context);}
            std::string extra;require(!(row>>extra),"Unexpected trailing element oracle data");++checked;
        }
        require(checked==30,"Element replacement coverage changed");std::cout<<checked<<" original replacement and nested initialization scenarios passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
