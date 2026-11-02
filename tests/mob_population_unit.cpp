#include "eternity/game/mob_population.hpp"
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
    std::unique_ptr<MobPopulation> population;std::unique_ptr<MobPool> pool;std::unique_ptr<ZoneMobs> zone;std::uint32_t counter=0;
    std::vector<std::pair<std::int32_t,std::uint32_t>> respawn_seeds;
    void prepare() {
        definitions=MobRuntimeTemplates::load(rows);population=std::make_unique<MobPopulation>(map,definitions);pool=std::make_unique<MobPool>(counter,services);zone=std::make_unique<ZoneMobs>(map.geometry(),services);services.zone=zone.get();std::size_t i=0;
        for(auto& area:population->areas()) for(auto& spawn:area.spawns) {spawn.current=respawn_seeds.at(i).first;spawn.next=respawn_seeds.at(i++).second;}
    }
    Bytes snapshot() {
        Bytes b;number(b,counter);number(b,pool->construction_attempts());number(b,static_cast<std::uint32_t>(zone->size()));number(b,services.random.state());number(b,services.ticks);
        std::vector<MobRespawn*> respawns;for(auto& area:population->areas()) for(auto& spawn:area.spawns) {respawns.push_back(&spawn);number(b,static_cast<std::uint32_t>(spawn.current));number(b,spawn.next);}
        number(b,static_cast<std::uint32_t>(services.objects.size()));
        for(auto* m:services.objects) {
            number(b,m->definition->id-1U);number(b,m->id);number(b,m->type,2);for(auto f:{m->position.x,m->position.y,m->position.z,m->respawn_position.x,m->respawn_position.y,m->respawn_position.z}) number(b,std::bit_cast<std::uint32_t>(f));
            require(m->zone==zone.get() && m->cell.has_value(),"Population object is detached");require(zone->find(m->id)==m,"Population object missing from ID index");const auto cell=zone->in_cell(*m->cell);require(std::find(cell.begin(),cell.end(),m)!=cell.end(),"Population object missing from its cell");number(b,static_cast<std::uint32_t>(m->cell->x));number(b,static_cast<std::uint32_t>(m->cell->z));const auto at=std::find(respawns.begin(),respawns.end(),m->respawn);require(at!=respawns.end(),"Population respawn binding lost");number(b,static_cast<std::uint32_t>(at-respawns.begin()));number(b,static_cast<std::uint32_t>(m->status));number(b,m->hp);number(b,m->sp,2);number(b,m->mp,2);number(b,m->next_target);number(b,m->next_new_target);number(b,m->drop_random.seed());number(b,m->drop_random.words()[0]);number(b,m->drop_random.index());number(b,static_cast<std::uint32_t>(m->lua_ai));number(b,m->lock_on_tick);number(b,static_cast<std::uint32_t>(m->unbeatable));number(b,m->vision_fixed_user);
        }
        require(zone->size()==services.objects.size(),"Population member count changed");return b;
    }
    Bytes run() {
        if(!population) prepare();services.events.clear();MobInitPorts ports{services,[&]{return services.tick_ms();},[&](MobRuntime& m,std::uint8_t ai){return services.compiled(m,ai);},[&](MobRuntime& m){services.lua(m);}};
        const auto success=population->initialize(*zone,*pool,ports,[&]{services.event('B',static_cast<std::uint32_t>(zone->size()));return services.ai_ok;});Bytes b;number(b,success?1:0,1);number(b,static_cast<std::uint32_t>(services.events.size()));b.insert(b.end(),services.events.begin(),services.events.end());const auto state=snapshot();b.insert(b.end(),state.begin(),state.end());return b;
    }
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected mob population oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open mob population oracle");std::unique_ptr<State> state;std::string line;unsigned int scenarios=0,runs=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string op;row>>op;
            if(op=="S") {
                state=std::make_unique<State>();unsigned int scenario=0,ai_ok=0;std::uint32_t random=0;row>>scenario>>state->map.size>>state->counter>>state->services.fail>>random>>state->services.tick>>state->services.time>>ai_ok;state->services.ai_ok=ai_ok!=0;state->services.random=MicrosoftRandom(random);state->services.counter=&state->counter;state->map.tiles.resize((static_cast<std::size_t>(state->map.size)*static_cast<std::size_t>(state->map.size)+7)/8);
                for(std::int32_t z=0;z<state->map.size;++z) for(std::int32_t x=0;x<state->map.size;++x) if((static_cast<unsigned int>(x+z)+scenario)%7==0) {const auto i=static_cast<std::size_t>(z*state->map.size+x);state->map.tiles[i/8]|=static_cast<std::uint8_t>(1U<<(i%8));}
                state->rows.columns={"MobID","Level","AI","HP","SP","MP","Dex","Wis","Luc","Attrib","Defense","Magic","Attacktime3","Attackspecial3","Exp"};++scenarios;
            } else if(op=="D") {unsigned int id=0,level=0,ai=0;row>>id>>level>>ai;state->rows.rows.push_back({std::to_string(id),std::to_string(level),std::to_string(ai),"2765","874","1578","60","40","53","4","38","31","13","0","0"});}
            else if(op=="A") {MapMobArea area;std::uint32_t x=0,y=0,z=0;row>>x>>y>>z>>area.delta_x>>area.delta_z;area.box.minimum={std::bit_cast<float>(x),std::bit_cast<float>(y),std::bit_cast<float>(z)};area.box.maximum={200,3,200};state->map.mobs.push_back(std::move(area));}
            else if(op=="P") {MapMobSpawn spawn;std::int32_t current=0;std::uint32_t next=0;row>>spawn.mob_id>>spawn.count>>current>>next;state->map.mobs.back().spawns.push_back(spawn);state->respawn_seeds.emplace_back(current,next);}
            else if(op=="RUN") {std::string hex;row>>hex;const auto actual=state->run(),expected=unhex(hex);if(actual!=expected) {const auto at=std::mismatch(actual.begin(),actual.end(),expected.begin(),expected.end());throw std::runtime_error("Original mob population differs at run "+std::to_string(runs)+" byte "+std::to_string(at.first-actual.begin()));}++runs;}
            else require(false,"Unknown population vector");require(static_cast<bool>(row),"Malformed mob population vector");
        }
        require(scenarios==128 && runs==256,"Mob population coverage changed");std::cout<<scenarios<<" original populations, "<<runs<<" sustained initialization runs passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
