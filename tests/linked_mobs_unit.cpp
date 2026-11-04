#include "mob_damage_fixture.hpp"
#include "eternity/game/linked_mobs.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;using Bytes=eternity::net::Packet;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void number(Bytes& b,std::uint32_t v,unsigned int n=4) {for(unsigned int i=0;i<n;++i) b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes b;for(std::size_t i=0;i<text.size();i+=2) b.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return b;}
struct Services final:MobPoolServices,ZoneMobServices,RandomSource {
    MobDamageFixture damage_fixture;MobDamagePool& damage_pool() override {return damage_fixture.pool;}

    Bytes events;std::vector<MobRuntime*> objects;std::uint32_t* counter=nullptr;ZoneMobs* zone=nullptr;
    unsigned int fail=0,attempts=0;std::uint32_t time=0,tick=0,ticks=0;bool constructing=false,ai_ok=false;
    MicrosoftRandom random;
    void event(char tag) {events.push_back(static_cast<std::uint8_t>(tag));}
    void event(char tag,std::uint32_t value) {event(tag);number(events,value);}
    std::int32_t next() override {const auto value=random.next();event('R',static_cast<std::uint32_t>(value));return value;}
    std::uint32_t unix_time() override {if(!constructing) event('I',*counter);event('T',time);return time;}
    std::uint32_t tick_ms() override {const auto value=tick+ticks++*11U;event('K',value);return value;}
    std::unique_ptr<MobRuntime> construct() override {
        const auto attempt=attempts++;const bool failed=fail==1 || (fail==2 && attempt%3==0) || (fail==3 && attempt%2==0);event('N',failed?0:1);if(failed) return {};
        auto mob=std::make_unique<MobRuntime>();constructing=true;mob->drop_random=ServerRandom(unix_time());constructing=false;objects.push_back(mob.get());return mob;
    }
    void release_compiled_ai(MobRuntime&) override {throw std::runtime_error("Unexpected pool release");}
    void release_stack_ai(MobRuntime&) override {throw std::runtime_error("Unexpected pool release");}
    void close_lua(void*) override {throw std::runtime_error("Unexpected Lua close");}
    void send_cell_view(CellCoordinates cell,const Bytes& packet) override {require(!zone->in_cell(cell).empty(),"Entry sent before membership");event('V',static_cast<std::uint32_t>(packet.size()));events.insert(events.end(),packet.begin(),packet.end());}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const Bytes&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected movement");}
    void lua_init(MobRuntime& mob) override {event('L',mob.id);}
    bool compiled(MobRuntime& mob,std::uint8_t ai) {event('A',ai);mob.status=17;mob.lock_on_tick=0x1234;mob.unbeatable=33;mob.vision_fixed_user=99;return ai_ok;}
    void lua(MobRuntime& mob) {event('U');mob.lua_ai=1;mob.only_lua_attack=1;mob.lock_on_tick=0x76543210;mob.unbeatable=5;mob.vision_fixed_user=99;}
};
struct State {
    MapDefinition map;eternity::storage::ResultSet rows;MobRuntimeTemplates definitions;Services services;
    std::vector<LinkedMobGroup> source_groups;
    std::unique_ptr<LinkedMobPopulation> population;std::unique_ptr<MobPool> pool;std::unique_ptr<ZoneMobs> zone;std::uint32_t counter=0;
    unsigned int next_spawn=0;
    LinkedMobSpawn& add_spawn() {auto& group=source_groups.back();return next_spawn++==0?group.leader:group.spawns.at(next_spawn-2);}
    void prepare() {population=std::make_unique<LinkedMobPopulation>(source_groups);pool=std::make_unique<MobPool>(counter,services);zone=std::make_unique<ZoneMobs>(map.geometry(),services);services.zone=zone.get();}
    std::uint32_t canonical(MobRuntime* pointer) const {
        if(!pointer) return 0;const auto at=std::find(services.objects.begin(),services.objects.end(),pointer);if(at!=services.objects.end()) return static_cast<std::uint32_t>(at-services.objects.begin())+1;
        return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer));
    }
    Bytes snapshot() {
        Bytes b;number(b,counter);number(b,pool->construction_attempts());number(b,static_cast<std::uint32_t>(zone->size()));number(b,services.random.state());number(b,services.ticks);
        for(auto& group:population->groups()) number(b,static_cast<std::uint32_t>(group.current_index));
        std::vector<MobRespawn*> respawns;
        for(auto& group:population->groups()) for(int i=-1;i<group.spawn_count;++i) {
            auto& spawn=i<0?group.leader:group.spawns.at(static_cast<std::size_t>(i));respawns.push_back(&spawn.respawn);require(spawn.respawn.both_source==&spawn,"Copied linked respawn lost its self binding");number(b,static_cast<std::uint32_t>(spawn.respawn.current));number(b,spawn.respawn.next);for(auto* member:spawn.members) number(b,canonical(member));
        }
        number(b,static_cast<std::uint32_t>(services.objects.size()));
        for(auto* m:services.objects) {
            number(b,m->definition->id-1U);number(b,m->id);number(b,m->type,2);for(auto f:{m->position.x,m->position.y,m->position.z,m->respawn_position.x,m->respawn_position.y,m->respawn_position.z}) number(b,std::bit_cast<std::uint32_t>(f));
            if(m->zone) {require(m->zone==zone.get() && zone->find(m->id)==m,"Linked member ID binding differs");const auto cell=zone->in_cell(*m->cell);require(std::find(cell.begin(),cell.end(),m)!=cell.end(),"Linked member cell binding differs");}
            number(b,m->cell?static_cast<std::uint32_t>(m->cell->x):0);number(b,m->cell?static_cast<std::uint32_t>(m->cell->z):0);const auto at=std::find(respawns.begin(),respawns.end(),m->respawn);require(at!=respawns.end(),"Linked respawn binding lost");number(b,static_cast<std::uint32_t>(at-respawns.begin()));number(b,static_cast<std::uint32_t>(m->status));number(b,m->hp);number(b,m->sp,2);number(b,m->mp,2);number(b,m->next_target);number(b,m->next_new_target);number(b,m->drop_random.seed());number(b,m->drop_random.words()[0]);number(b,m->drop_random.index());number(b,static_cast<std::uint32_t>(m->lua_ai));number(b,m->lock_on_tick);number(b,static_cast<std::uint32_t>(m->unbeatable));number(b,m->vision_fixed_user);number(b,m->zone?1:0,1);
        }
        return b;
    }
    Bytes run() {
        if(!population) prepare();services.events.clear();MobInitPorts ports{services,[&]{return services.tick_ms();},[&](MobRuntime& m,std::uint8_t ai){return services.compiled(m,ai);},[&](MobRuntime& m){services.lua(m);}};
        const auto success=population->initialize(map,*zone,*pool,ports,[&]{services.event('T',services.time);return services.time;});Bytes b;number(b,success?1:0,1);number(b,static_cast<std::uint32_t>(services.events.size()));b.insert(b.end(),services.events.begin(),services.events.end());const auto state=snapshot();b.insert(b.end(),state.begin(),state.end());return b;
    }
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected linked mob oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open linked mob oracle");std::unique_ptr<State> state;std::string line;unsigned int scenarios=0,runs=0,helpers=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string op;row>>op;
            if(op=="S") {
                state=std::make_unique<State>();unsigned int scenario=0,ai_ok=0;std::uint32_t random=0;row>>scenario>>state->map.size>>state->counter>>state->services.fail>>random>>state->services.tick>>state->services.time>>ai_ok;state->services.ai_ok=ai_ok!=0;state->services.random=MicrosoftRandom(random);state->services.counter=&state->counter;state->map.tiles.resize((static_cast<std::size_t>(state->map.size)*static_cast<std::size_t>(state->map.size)+7)/8);
                for(std::int32_t z=0;z<state->map.size;++z) for(std::int32_t x=0;x<state->map.size;++x) if((static_cast<unsigned int>(x+z)+scenario)%7==0) {const auto i=static_cast<std::size_t>(z*state->map.size+x);state->map.tiles[i/8]|=static_cast<std::uint8_t>(1U<<(i%8));}
                state->rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};++scenarios;
            } else if(op=="D") {unsigned int id=0,level=0,ai=0;row>>id>>level>>ai;state->rows.rows.push_back({std::to_string(id),std::to_string(level),std::to_string(ai),"2765","874","1578","60","40","53","4","38","31","13","0","0"});}
            else if(op=="G") {if(state->source_groups.empty()) state->definitions=MobRuntimeTemplates::load(state->rows);LinkedMobGroup group;row>>group.spawn_count;group.current_index=99;state->source_groups.push_back(group);state->next_spawn=0;}
            else if(op=="P") {
                auto& spawn=state->add_spawn();std::uint32_t x=0,y=0,z=0,radius=0;unsigned int info=0;row>>x>>y>>z>>radius>>spawn.radius_twice>>spawn.count>>info>>spawn.randomized>>spawn.minimum_minutes>>spawn.random_minutes;spawn.center={std::bit_cast<float>(x),std::bit_cast<float>(y),std::bit_cast<float>(z)};spawn.radius=std::bit_cast<float>(radius);spawn.definitions[0]=state->definitions.find(info+1);spawn.respawn.type=static_cast<std::int32_t>(info+1);spawn.respawn.count=spawn.count;spawn.respawn.current=-17;spawn.respawn.next=0xabcdef01;spawn.respawn.definition=spawn.definitions[0];spawn.respawn.both=true;
                const auto ordinal=(state->source_groups.size()-1)*3+state->next_spawn;for(std::size_t i=0;i<16;++i) spawn.members[i]=reinterpret_cast<MobRuntime*>(static_cast<std::uintptr_t>(0x71000000U+ordinal*256+i));
            }
            else if(op=="H") {char command=0;unsigned int index=0;std::uint32_t pointer=0;std::string expected;row>>command>>index>>pointer>>expected;auto& group=state->population->groups()[index/3];auto& spawn=index%3==0?group.leader:group.spawns[index%3-1];auto* mob=reinterpret_cast<MobRuntime*>(static_cast<std::uintptr_t>(pointer));if(command=='A') spawn.add(mob);else if(command=='R') spawn.remove(mob);else {require(command=='Z',"Unknown linked helper");spawn.reset_members();}Bytes actual{1,0,0,0,0};const auto snapshot=state->snapshot();actual.insert(actual.end(),snapshot.begin(),snapshot.end());if(actual!=unhex(expected)) throw std::runtime_error("Original linked helper differs at "+std::to_string(helpers));++helpers;}
            else if(op=="RUN") {std::string hex;row>>hex;const auto actual=state->run(),expected=unhex(hex);if(actual!=expected) {const auto at=std::mismatch(actual.begin(),actual.end(),expected.begin(),expected.end());throw std::runtime_error("Original linked mob population differs at run "+std::to_string(runs)+" byte "+std::to_string(at.first-actual.begin()));}++runs;}
            else require(false,"Unknown population vector");require(static_cast<bool>(row),"Malformed linked mob vector");
        }
        require(scenarios==128 && runs==256 && helpers==1280,"Linked mob coverage changed");std::cout<<scenarios<<" original linked populations, "<<runs<<" sustained linked initialization runs passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
