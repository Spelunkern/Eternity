#include "eternity/game/zones.hpp"
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
struct Services final:MobPoolServices,ZoneMobServices,RandomSource,LinkedMobWorld {
    Bytes events;std::vector<MobRuntime*> objects;std::uint32_t* counter=nullptr;ZoneMobs* zone=nullptr;
    RuntimeZone* runtime=nullptr;LinkedMobPopulation* population=nullptr;MapPortal portal;std::array<MobRuntime,3> dummies;
    unsigned int fail=0,attempts=0;std::uint32_t time=0,tick=0,ticks=0;bool constructing=false,ai_ok=false;
    MicrosoftRandom random;
    void event(char tag) {events.push_back(static_cast<std::uint8_t>(tag));}
    void event(char tag,std::uint32_t value) {event(tag);number(events,value);}
    std::uint32_t canonical(MobRuntime* pointer) const {
        if(!pointer) return 0;for(std::size_t i=0;i<dummies.size();++i) if(pointer==&dummies[i]) return static_cast<std::uint32_t>(0x70000001U+i);
        const auto at=std::find(objects.begin(),objects.end(),pointer);require(at!=objects.end(),"Unknown linked member");return static_cast<std::uint32_t>(at-objects.begin())+1;
    }
    void observe() {
        for(auto& group:population->groups()) number(events,static_cast<std::uint32_t>(group.current_index));
        for(auto& group:population->groups()) for(int i=-1;i<group.spawn_count;++i) {auto& spawn=i<0?group.leader:group.spawns.at(static_cast<std::size_t>(i));number(events,static_cast<std::uint32_t>(spawn.respawn.current));number(events,spawn.respawn.next);for(auto* m:spawn.members) number(events,canonical(m));}
    }
    void remove_by_id(std::uint32_t id) override {event('M',id);observe();}
    void set_portal_country(const MapPortal& p,std::int32_t country) override {require(&p==&portal,"Wrong shared portal");portal.country=country;event('P',static_cast<std::uint32_t>(p.id));number(events,static_cast<std::uint32_t>(country));number(events,static_cast<std::uint32_t>(portal.country));observe();runtime->update_instance_portal_country(p.id,country);}
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
    MapDefinition map;ConfiguredMap configured;eternity::storage::ResultSet rows;MobRuntimeTemplates definitions;std::uint32_t counter=0;Services services;
    std::vector<LinkedMobGroup> source_groups;
    std::unique_ptr<LinkedMobPopulation> population;std::unique_ptr<MobPool> pool;ZoneMobs* zone=nullptr;RuntimeZone runtime;unsigned int scenario=0;
    unsigned int next_spawn=0;
    LinkedMobSpawn& add_spawn() {auto& group=source_groups.back();return next_spawn++==0?group.leader:group.spawns.at(next_spawn-2);}
    void prepare() {population=std::make_unique<LinkedMobPopulation>(source_groups);pool=std::make_unique<MobPool>(counter,services);runtime.map=&configured;runtime.mobs=std::make_unique<ZoneMobs>(map.geometry(),services);zone=runtime.mobs.get();services.zone=zone;services.runtime=&runtime;services.population=population.get();}
    Bytes snapshot() {
        Bytes b;number(b,counter);number(b,pool->construction_attempts());number(b,static_cast<std::uint32_t>(zone->size()));number(b,services.random.state());number(b,services.ticks);
        for(auto& group:population->groups()) number(b,static_cast<std::uint32_t>(group.current_index));
        std::vector<MobRespawn*> respawns;
        for(auto& group:population->groups()) for(int i=-1;i<group.spawn_count;++i) {
            auto& spawn=i<0?group.leader:group.spawns.at(static_cast<std::size_t>(i));respawns.push_back(&spawn.respawn);require(spawn.respawn.both_source==&spawn,"Copied linked respawn lost its self binding");number(b,static_cast<std::uint32_t>(spawn.respawn.current));number(b,spawn.respawn.next);for(auto* member:spawn.members) number(b,services.canonical(member));
        }
        number(b,static_cast<std::uint32_t>(services.objects.size()));
        for(auto* m:services.objects) {
            number(b,m->definition->id-1U);number(b,m->id);number(b,m->type,2);for(auto f:{m->position.x,m->position.y,m->position.z,m->respawn_position.x,m->respawn_position.y,m->respawn_position.z}) number(b,std::bit_cast<std::uint32_t>(f));
            if(m->zone) {require(m->zone==zone && zone->find(m->id)==m,"Linked member ID binding differs");const auto cell=zone->in_cell(*m->cell);require(std::find(cell.begin(),cell.end(),m)!=cell.end(),"Linked member cell binding differs");}
            number(b,m->cell?static_cast<std::uint32_t>(m->cell->x):0);number(b,m->cell?static_cast<std::uint32_t>(m->cell->z):0);const auto at=std::find(respawns.begin(),respawns.end(),m->respawn);require(at!=respawns.end(),"Linked respawn binding lost");number(b,static_cast<std::uint32_t>(at-respawns.begin()));number(b,static_cast<std::uint32_t>(m->status));number(b,m->hp);number(b,m->sp,2);number(b,m->mp,2);number(b,m->next_target);number(b,m->next_new_target);number(b,m->drop_random.seed());number(b,m->drop_random.words()[0]);number(b,m->drop_random.index());number(b,static_cast<std::uint32_t>(m->lua_ai));number(b,m->lock_on_tick);number(b,static_cast<std::uint32_t>(m->unbeatable));number(b,m->vision_fixed_user);number(b,m->zone?1:0,1);
        }
        number(b,static_cast<std::uint32_t>(services.portal.country));for(auto portal:runtime.instance_portals) {number(b,static_cast<std::uint32_t>(portal.id));number(b,static_cast<std::uint32_t>(portal.country));}
        return b;
    }
    Bytes run(char operation,std::uint32_t argument) {
        if(!population) prepare();services.events.clear();MobInitPorts ports{services,[&]{return services.tick_ms();},[&](MobRuntime& m,std::uint8_t ai){return services.compiled(m,ai);},[&](MobRuntime& m){services.lua(m);}};
        if(operation=='R') population->respawn(argument,map,*zone,*pool,ports,services);
        else if(operation=='X') population->destroyed(argument,map,*zone,*pool,ports,services);
        else if(operation=='U') population->update(map,*zone,*pool,ports,argument,services);
        else require(operation=='C',"Unknown lifecycle method");
        Bytes b;number(b,1,1);number(b,static_cast<std::uint32_t>(services.events.size()));b.insert(b.end(),services.events.begin(),services.events.end());const auto state=snapshot();b.insert(b.end(),state.begin(),state.end());return b;
    }
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected linked mob oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open linked mob oracle");std::unique_ptr<State> state;std::string line;unsigned int scenarios=0,operations=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string op;row>>op;
            if(op=="S") {
                state=std::make_unique<State>();unsigned int ai_ok=0;std::uint32_t random=0;row>>state->scenario>>state->map.size>>state->counter>>state->services.fail>>random>>state->services.tick>>state->services.time>>ai_ok>>state->configured.settings.create_type;state->services.ai_ok=ai_ok!=0;state->services.random=MicrosoftRandom(random);state->services.counter=&state->counter;state->map.tiles.resize((static_cast<std::size_t>(state->map.size)*static_cast<std::size_t>(state->map.size)+7)/8);
                for(std::int32_t z=0;z<state->map.size;++z) for(std::int32_t x=0;x<state->map.size;++x) if((static_cast<unsigned int>(x+z)+state->scenario)%7==0) {const auto i=static_cast<std::size_t>(z*state->map.size+x);state->map.tiles[i/8]|=static_cast<std::uint8_t>(1U<<(i%8));}
                state->services.portal.id=1010;state->services.portal.country=2;for(std::size_t i=0;i<3;++i) state->services.dummies[i].id=static_cast<std::uint32_t>(0xf0000000U+i);state->rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};++scenarios;
            } else if(op=="D") {unsigned int id=0,level=0,ai=0,selector=0;row>>id>>level>>ai>>selector;state->rows.rows.push_back({std::to_string(id),std::to_string(level),std::to_string(ai),"2765","874","1578","60","40","53","4","38","31","13",std::to_string(selector),"0"});}
            else if(op=="G") {if(state->source_groups.empty()) state->definitions=MobRuntimeTemplates::load(state->rows);LinkedMobGroup group;unsigned int has_portal=0;row>>group.spawn_count>>group.change_count>>group.current_index>>has_portal;group.portal=has_portal?&state->services.portal:nullptr;state->source_groups.push_back(group);state->next_spawn=0;}
            else if(op=="P") {
                auto& spawn=state->add_spawn();std::uint32_t x=0,y=0,z=0,radius=0;unsigned int info=0;row>>x>>y>>z>>radius>>spawn.radius_twice>>spawn.count>>info>>spawn.randomized>>spawn.minimum_minutes>>spawn.random_minutes;spawn.center={std::bit_cast<float>(x),std::bit_cast<float>(y),std::bit_cast<float>(z)};spawn.radius=std::bit_cast<float>(radius);for(std::size_t stage=0;stage<16;++stage) spawn.definitions[stage]=state->definitions.find(static_cast<std::uint32_t>((info+stage)%4+1));spawn.respawn.type=static_cast<std::int32_t>(info+1);spawn.respawn.count=spawn.count;spawn.respawn.current=state->next_spawn==1?0:spawn.count-1;spawn.respawn.next=0;spawn.respawn.definition=spawn.definitions[0];spawn.respawn.both=true;
                for(std::size_t i=0;i<16;++i) spawn.members[i]=i<static_cast<std::size_t>(spawn.count) && i%2==0?&state->services.dummies[i%3]:nullptr;for(std::size_t i=0;i<3;++i) spawn.random_centers[i]={1.25F+static_cast<float>(i)*40,9.0F+static_cast<float>(i),1.25F};
            }
            else {
                require(op=="R" || op=="X" || op=="U" || op=="C","Unknown linked lifecycle vector");std::uint32_t argument=0;row>>argument;
                if(op=="C") {if(!state->population) state->prepare();auto& group=state->population->groups()[argument/3];auto& spawn=argument%3==0?group.leader:group.spawns[argument%3-1];row>>spawn.respawn.current>>spawn.respawn.next;}
                std::string expected;row>>expected;const auto actual=state->run(op[0],argument),original=unhex(expected);if(actual!=original) {const auto at=std::mismatch(actual.begin(),actual.end(),original.begin(),original.end());throw std::runtime_error("Original linked lifecycle differs at operation "+std::to_string(operations)+" byte "+std::to_string(at.first-actual.begin())+" sizes "+std::to_string(actual.size())+"/"+std::to_string(original.size()));}++operations;
            }
            require(static_cast<bool>(row),"Malformed linked mob vector");
        }
        require(scenarios==128 && operations==2048,"Linked mob coverage changed");std::cout<<scenarios<<" original linked populations, "<<operations<<" linked lifecycle operations passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
