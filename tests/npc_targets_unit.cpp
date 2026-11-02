#include "eternity/game/npc_targets.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;using Bytes=eternity::net::Packet;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void number(Bytes& bytes,std::uint32_t value,unsigned int size=4) {for(unsigned int i=0;i<size;++i) bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes result;for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;}
struct Scene final:ActorScene {
    void send_self(const Bytes&) noexcept override {std::terminate();}
    void send_nearby(const Bytes&) override {throw std::logic_error("Unexpected target-query broadcast");}
    void send_cell_view(const Bytes&) override {throw std::logic_error("Unexpected target-query view");}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const Bytes&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::logic_error("Unexpected target-query damage");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return {};}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 0;}
    void stop_target_mob(std::uint32_t) override {throw std::logic_error("Unexpected target-query mob stop");}
    void taunt(std::uint16_t,float) override {throw std::logic_error("Unexpected target-query taunt");}
    ZoneUsers* user_zone() override {return nullptr;}
    UserShapeSocial user_shape_social() override {throw std::logic_error("Unexpected target-query shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::logic_error("Unexpected target-query periodic damage");}
};
struct Mob final:NpcMobTarget {
    std::uint32_t uid=0,after=0;UserPosition at;std::int32_t faction=0,state=0;unsigned int mode=0;
    std::uint32_t id() const noexcept override {return uid;}
    UserPosition position() const noexcept override {return at;}
    std::int32_t country() const noexcept override {return faction;}
    std::int32_t status() const noexcept override {return mode==1?0:state;}
    std::uint32_t& guard_target_after() noexcept override {return after;}
};
struct Mobs final:NpcMobCells {
    std::map<std::pair<std::int32_t,std::int32_t>,std::vector<Mob*>> cells;
    bool visit_cell(CellCoordinates cell,const std::function<bool(NpcMobTarget&)>& visitor) override {
        const auto found=cells.find({cell.x,cell.z});if(found==cells.end()) return false;
        for(auto* mob:found->second) if(visitor(*mob)) return true;return false;
    }
};
struct Clock final:NpcClock {
    Npc* npc=nullptr;std::vector<std::unique_ptr<Actor>>* actors=nullptr;std::array<Mob,12>* mobs=nullptr;
    std::uint32_t base=0;mutable std::uint32_t calls=0;unsigned int mutation=0;
    std::uint32_t tick_ms() const noexcept override {
        const auto sample=base+calls*17U;++calls;
        if(calls==1) {
            if(mutation==1) {for(auto& actor:*actors) actor->attack_state().mode=1;for(auto& mob:*mobs) {mob.mode=0;mob.state=2;}}
            else if(mutation==2 || mutation==3) npc->status=mutation==2?1:2;
            else if(mutation==4) for(auto& actor:*actors) {actor->status().no_mob_targeting=true;actor->appearance().visible=false;actor->appearance().attackable=false;}
            else if(mutation==5) {for(auto& actor:*actors) actor->guard_target_after()=sample+1;for(auto& mob:*mobs) mob.after=sample+1;}
        }
        return sample;
    }
};
struct State {
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates templates;std::array<Scene,12> scenes;std::vector<std::unique_ptr<Actor>> actors;
    std::array<Mob,12> mob;std::unique_ptr<ZoneUsers> users;Mobs mobs;Npc npc;NpcDefinition definition;MapNpc source;MapDefinition map;Clock clock;
    void initialize(unsigned int s) {
        users.reset();actors.clear();mobs.cells.clear();npc={};definition={};source={};map={};clock={};
        map.size=std::array<std::int32_t,3>{40,81,160}[s%3];const auto count=map.geometry().cell_count();const float center=static_cast<float>(map.size)/2;
        clock.base=std::array<std::uint32_t,4>{0,100,5000,0xffffffff}[(s/7)%4];clock.mutation=(s/17)%6;clock.npc=&npc;clock.actors=&actors;clock.mobs=&mob;
        npc.position={center-0.75F,3,center-0.25F};if(s%41==0) npc.position.x=-0.25F;if(s%43==0) npc.position.x=-1;if(s%47==0) npc.position.z=std::numeric_limits<float>::quiet_NaN();
        map.tiles.resize((static_cast<std::size_t>(map.size)*static_cast<std::size_t>(map.size)+7)/8);
        if(s%37==0 && std::isfinite(npc.position.x) && std::isfinite(npc.position.z)) {const auto x=static_cast<std::int32_t>(npc.position.x),z=static_cast<std::int32_t>(npc.position.z);if(x>=0 && z>=0 && x<map.size && z<map.size) {const auto at=static_cast<std::size_t>(z*map.size+x);map.tiles[at/8]|=static_cast<std::uint8_t>(1U<<(at%8));}}
        npc.cell=CellCoordinates{static_cast<std::int32_t>((s/3)%static_cast<unsigned int>(count)),static_cast<std::int32_t>((s/5)%static_cast<unsigned int>(count))};npc.next_position=static_cast<std::int32_t>((s/11)%3);
        npc.definition=&definition;npc.source=&source;npc.status=static_cast<std::int32_t>((s/13)%4);npc.target_kind=2;npc.target_id=777;definition.record[16]=std::array<std::uint8_t,4>{0,1,2,255}[(s/19)%4];
        source.positions={{{center,0,center},0},{{center-10,2,center-10},0},{{center+10,4,center+10},0}};
        users=std::make_unique<ZoneUsers>(map.geometry(),0);
        const auto nan=std::numeric_limits<float>::quiet_NaN(),infinity=std::numeric_limits<float>::infinity();
        const std::array<std::array<float,2>,12> offsets{{{0,0},{3,4},{20,0},{20.000003814697266F,0},{19.999996185302734F,0},{30,0},{nan,0},{infinity,0},{-3,-4},{10,0},{0,20},{0,30}}};
        for(unsigned int i=0;i<12;++i) {
            EffectStatus status;status.shape=(s+i)%7==0?13:0;status.no_mob_targeting=(s+i)%13==0;
            actors.push_back(std::make_unique<Actor>(9001+i,status,PlayerResources{},std::vector<PersistedEffect>{},scenes[i],ActorEquipmentRules{items,enchant,templates}));auto& actor=*actors.back();
            actor.appearance().identity.country=static_cast<std::uint8_t>((s+i)%3);actor.attack_state().mode=(s+i)%11==0;actor.appearance().visible=(s+i)%17!=0;actor.appearance().attackable=(s+i)%19!=0;
            const auto cooldown=std::array<std::uint32_t,4>{clock.base-1,clock.base,clock.base+1,0xffffffff}[(s+i)%4];actor.guard_target_after()=cooldown;
            const auto& delta=offsets[(s+i)%offsets.size()];users->attach(actor,static_cast<std::int32_t>((s+i*2)%static_cast<unsigned int>(count)),static_cast<std::int32_t>((s*3+i)%static_cast<unsigned int>(count)),{npc.position.x+delta[0],9000,npc.position.z+delta[1]});
            auto& object=mob[i];object.uid=10001+i;object.after=cooldown;object.faction=static_cast<std::int32_t>((s+i)%4);object.state=static_cast<std::int32_t>((s+i)%4);object.mode=(s+i)/3%3;
            const auto& md=offsets[(s+i+3)%offsets.size()];object.at={npc.position.x+md[0],-9000,npc.position.z+md[1]};mobs.cells[{static_cast<std::int32_t>((s+i*3+1)%static_cast<unsigned int>(count)),static_cast<std::int32_t>((s*2+i)%static_cast<unsigned int>(count))}].push_back(&object);
        }
    }
    Bytes snapshot(bool result) {
        Bytes bytes;number(bytes,result?1:0,1);number(bytes,static_cast<std::uint32_t>(npc.status));number(bytes,static_cast<std::uint32_t>(npc.target_kind));number(bytes,npc.target_id);number(bytes,clock.calls);
        for(const auto& actor:actors) {number(bytes,actor->guard_target_after());number(bytes,static_cast<std::uint32_t>(actor->attack_state().mode));}
        for(auto& object:mob) {number(bytes,object.after);number(bytes,static_cast<std::uint32_t>(object.status()));}return bytes;
    }
    Bytes query() {return snapshot(find_npc_target(npc,map,*users,mobs,clock));}
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected guard target oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open guard target oracle");State state;std::string line;unsigned int cases=0,direct=0,sequences=0,rounds=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;unsigned int scenario=0;std::string hex;Bytes bytes;row>>kind;
            if(kind=='S') {row>>scenario>>hex;state.initialize(scenario);bytes=state.query();++cases;}
            else if(kind=='D') {unsigned int target_kind=0;row>>target_kind>>scenario>>hex;state.initialize(scenario);if(target_kind==0) set_npc_target(state.npc,*state.actors[0],state.clock);else set_npc_target(state.npc,state.mob[0],state.clock);bytes=state.snapshot(false);++direct;}
            else if(kind=='R') {row>>scenario;state.initialize(scenario);++sequences;}
            else if(kind=='G') {row>>hex;state.npc.status=0;state.npc.target_kind=2;state.npc.target_id=777;bytes=state.query();++rounds;}
            else require(false,"Unknown guard target vector type");
            if(kind!='R' && bytes!=unhex(hex)) throw std::runtime_error("Original guard target state differs: kind "+std::string(1,kind)+" case "+std::to_string(cases)+" direct "+std::to_string(direct)+" round "+std::to_string(rounds));
            require(static_cast<bool>(row),"Malformed guard target vector");std::string extra;require(!(row>>extra),"Trailing guard target vector data");
        }
        require(cases==1024 && direct==256 && sequences==32 && rounds==128,"Guard target coverage changed");
        std::cout<<cases<<" original guard queries, "<<direct<<" setters and "<<rounds<<" shared-cooldown rounds passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
