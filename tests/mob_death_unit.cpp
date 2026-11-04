#include "eternity/game/mob_death.hpp"
#include "mob_damage_fixture.hpp"
#include <bit>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
void append(eternity::net::Packet& packet,std::uint32_t value) {for(unsigned i=0;i<4;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
eternity::net::Packet decode(const std::string& text) {
    check(text.size()%2==0,"Malformed hex");eternity::net::Packet bytes;
    for(std::size_t i=0;i<text.size();i+=2) bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return bytes;
}
struct Case {
    unsigned scenario=0,branch=0,fair=0,credit=0,found=0,scene=0,lua=0,count=0,maximum=0,eligible=0,level=0,base=0,country=0,rate_override=0,updates=0,current=0,deadline=0,free=0;
    int kind=0,admin=0,active=0,index=0;std::string multiplier,events;
    std::vector<MobDamageRecord> records;
};
struct World final:MobDeathWorld {
    const Case& c;MobRuntime& mob;MobDamageFixture& heap;
    std::array<MobDeathUser,31> users{};MobDeathParty group;eternity::net::Packet events;bool locked=false,fail_reward=false;
    World(const Case& input,MobRuntime& subject,MobDamageFixture& fixture):c(input),mob(subject),heap(fixture) {
        for(unsigned i=0;i<users.size();++i) users[i]={100+i,static_cast<std::uint16_t>(i*7+1),static_cast<std::uint8_t>(i%3)};
        group.type=static_cast<std::uint8_t>(std::max(c.kind,0));group.maximum_level=std::bit_cast<std::int32_t>(c.maximum);
        for(unsigned i=0;i<c.count;++i) group.members.push_back(&users[i]);
    }
    void event(char tag,std::initializer_list<std::uint32_t> values={}) {events.push_back(static_cast<std::uint8_t>(tag));for(auto value:values) append(events,value);}
    void send_death(MobRuntime& subject,const eternity::net::Packet& packet,std::uint8_t priority) override {check(&subject==&mob,"Wrong death subject");event('V',{static_cast<std::uint32_t>(packet.size()),priority});events.insert(events.end(),packet.begin(),packet.end());}
    MobDeathUser* find_user(MobRuntime&,std::uint32_t id) override {event('F',{id});return c.found && id>=100 && id<100+users.size()?&users[id-100]:nullptr;}
    MobDeathParty* party(MobDeathUser&) override {return c.kind<0?nullptr:&group;}
    void lock_party(MobDeathParty&) override {check(!locked,"Party locked twice");locked=true;event('[');}
    void unlock_party(MobDeathParty&) noexcept override {locked=false;event(']');}
    bool in_reward_range(MobDeathUser& credit,MobDeathUser& member,std::int32_t radius) override {check(locked,"Range before lock");event('R',{credit.id,member.id,static_cast<std::uint32_t>(radius)});return (c.eligible>>(member.id-100))&1;}
    void add_experience(MobDeathUser& user,std::uint32_t source,std::int32_t amount) override {
        if(fail_reward) throw std::runtime_error("Reward failure");event('E',{user.id,source,static_cast<std::uint32_t>(amount),0});
        if(c.updates) {user.level=60;group.maximum_level=60;group.type=static_cast<std::uint8_t>(1-(group.type&1));}
    }
    void drop_items(MobRuntime&,MobDeathUser* user,MobDeathParty* party) override {check(!party || locked,"Party drops after unlock");event('D',{user?user->id:0,party?1U:0U});}
    void quest_kill(MobDeathUser& user,MobRuntime&) override {event('Q',{user.id});}
    std::optional<std::int32_t> obelisk_country(MobRuntime&,std::int32_t type) override {if(type<0 || type>1) return std::nullopt;return static_cast<std::int32_t>(c.country);}
    void destroy_obelisk(MobRuntime&,std::int32_t country,std::int32_t type) override {event('O',{static_cast<std::uint32_t>(country),static_cast<std::uint32_t>(type),0});}
    void destroy_linked_group(MobRuntime&,std::int32_t type) override {event('B',{static_cast<std::uint32_t>(type)});}
    void remove_obelisk_member(MobRuntime&,void*) override {event('o');}
    void remove_linked_member(MobRuntime&,void*) override {event('b');}
    void log_death(MobRuntime&,MobDeathUser* user,bool fair) override {event('L',{4,user?1U:0U,fair?1U:0U});}
    void clear_applied_skills(MobRuntime& subject) override {check(subject.damage->fair_combat && subject.damage->records().empty() && heap.pool.available().size()==c.records.size(),"Skills cleared before damage reset");event('C');}
};
void run(const Case& c) {
    MobDamageFixture heap;MobRuntime mob;MobRuntimeTemplate definition;definition.level=static_cast<std::uint16_t>(c.level);definition.experience=static_cast<std::uint16_t>(c.base);
    MobRespawn respawn;respawn.type=c.branch==1 || c.branch==6?c.index:0;respawn.count=3;respawn.current=3;respawn.next=123;respawn.obelisk=c.branch==1 || c.branch==6;respawn.both=c.branch==2 || c.branch==3 || c.branch==6;
    // Only presence is consumed by this coordinator; no ZoneMobs methods execute.
    std::uint32_t zone_tag=0,obelisk_tag=0,linked_tag=0;
    respawn.obelisk_source=c.branch==2?&obelisk_tag:nullptr;respawn.both_source=c.branch==2 || c.branch==4?&linked_tag:nullptr;
    mob.id=77;mob.definition=&definition;mob.zone=c.scene?reinterpret_cast<ZoneMobs*>(&zone_tag):nullptr;mob.respawn=c.branch?&respawn:nullptr;mob.lua_ai=static_cast<std::int32_t>(c.lua);
    mob.damage=std::make_unique<MobDamageLedger>(heap.pool);mob.damage->fair_combat=c.fair;
    for(const auto& record:c.records) mob.damage->add(record.type,record.id,record.damage,0,record.party);
    MobDeathExperience rules;rules.admin_mode=c.admin;rules.event_active=c.active;
    const auto bits=decode(c.multiplier);std::uint64_t raw=0;for(unsigned i=0;i<8;++i) raw|=std::uint64_t(bits[i])<<(8*i);rules.multiplier=std::bit_cast<double>(raw);
    if(c.rate_override) for(unsigned i=0;i<31;++i) {rules.party[i]=static_cast<double>(i+1)/10;rules.raid[i]=static_cast<double>(i+9)/10;}
    World world(c,mob,heap);mob_death(mob,2,0xfedcba98,rules,world);
    const auto expected=decode(c.events);
    if(world.events!=expected) {
        std::cerr<<"Observed:";for(auto b:world.events) std::cerr<<' '<<std::hex<<unsigned(b);std::cerr<<"\nExpected:";for(auto b:expected) std::cerr<<' '<<std::hex<<unsigned(b);std::cerr<<std::dec<<'\n';
        throw std::runtime_error("Original death callback/packet mismatch in scenario "+std::to_string(c.scenario));
    }
    check(!world.locked && static_cast<std::uint32_t>(respawn.current)==c.current && respawn.next==c.deadline && heap.pool.available().size()==c.free,"Original death ownership mismatch in scenario "+std::to_string(c.scenario));
    check(mob.id==77 && mob.definition==&definition && mob.status==0 && mob.hp==0,"Death coordinator changed FSM/resources");
}
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original mob death oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot read mob death oracle");
    std::string line;Case c;unsigned cases=0;bool pending=false;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char tag;row>>tag;
        if(tag=='S') {
            if(pending) {run(c);++cases;}c={};pending=true;
            row>>c.scenario>>c.branch>>c.fair>>c.credit>>c.found>>c.scene>>c.lua>>c.kind>>c.count>>c.maximum>>c.eligible>>c.level>>c.base>>c.admin>>c.active>>c.multiplier>>c.index>>c.country>>c.rate_override>>c.updates>>c.events>>c.current>>c.deadline>>c.free;
        } else if(tag=='N') {unsigned type;MobDamageRecord record;row>>type>>record.id>>record.damage>>record.party;record.type=static_cast<std::uint8_t>(type);c.records.push_back(record);}
        else throw std::runtime_error("Unknown death oracle row");check(static_cast<bool>(row),"Malformed death oracle row");
    }
    if(pending) {run(c);++cases;}check(cases==1536,"Death coverage changed");std::cout<<cases<<" complete original mob death coordination cases passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
