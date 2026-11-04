#include "eternity/game/party_runtime_requests.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;using Bytes=eternity::net::Packet;
namespace {
void check(bool ok) {if(!ok)throw std::runtime_error("Invalid request fixture/owner state");}
void append(Bytes& b,std::uint32_t n) {for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
Bytes unhex(const std::string& s) {Bytes b;if(s=="-")return b;for(std::size_t i=0;i<s.size();i+=2)b.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return b;}
struct Record {PartyMembershipUser member;PartyRequestState requests;};
struct World final:PartyRequestWorld,PartyRegistryPorts,PartyMembershipWorld {
    PartyRegistry reg;std::array<PartyRegistryParty,4> parties;std::array<std::shared_ptr<Record>,8> users;
    Bytes events;std::array<unsigned,6> depths{};
    unsigned mutation{},raw=2,root{},ticks{};std::uint32_t clock{};bool missing{},alloc_fail{},raid_success{},record{},mutated{},nested{};
    World(unsigned profile,unsigned flags,unsigned m,bool miss,bool fail,bool raid,std::uint32_t now):mutation(m),clock(now),missing(miss),alloc_fail(fail),raid_success(raid) {
        for(unsigned i=0;i<8;++i) {
            users[i]=std::make_shared<Record>();auto& u=users[i]->member;auto& s=users[i]->requests;u.id=100+i;u.level=static_cast<std::uint16_t>(10+i);u.job=static_cast<std::uint8_t>(i%6);u.name="user-"+std::string(14,'0')+std::to_string(i);u.pending_request=9;u.in_zone=true;s.zone=1;s.scene_state=4;
            u.hp=static_cast<std::uint16_t>(100+i);u.maximum_hp=static_cast<std::uint16_t>(200+i);u.mp=static_cast<std::uint16_t>(300+i);u.maximum_mp=static_cast<std::uint16_t>(400+i);u.sp=static_cast<std::uint16_t>(500+i);u.maximum_sp=static_cast<std::uint16_t>(600+i);u.instance=1;u.position={1.25F+static_cast<float>(i),-2.5F,3.75F};
        }
        initialize_party_registry(reg,*this);for(unsigned i=0;i<2;++i){construct_registry_party(parties[i],*this);parties[i].id=10+i*10;}
        std::vector<unsigned> members;
        if(profile==1 || profile==2 || profile==5 || profile==6)members={0,2};
        else if(profile==3)members={0,1,2,3,4,5,6};else if(profile==4)members={0,2,3,4,5,6};else if(profile==7)members={0,1};
        if(!members.empty() || profile==8) {
            auto& p=parties[0];p.count=static_cast<std::int32_t>(members.size());p.leader=profile==2?1:0;p.experience.experience_level=60;p.experience.kind=profile==5?1:0;
            for(std::size_t j=0;j<members.size();++j){p.slots[j]=&users[members[j]]->member;users[members[j]]->member.party=&p;}
            if(profile==8)users[0]->member.party=&p;
            reg.entries.emplace(p.id,&p);
        }
        if(profile==6){auto& p=parties[1];p.count=1;p.slots[0]=&users[1]->member;users[1]->member.party=&p;reg.entries.emplace(p.id,&p);}
        if(flags&1)users[0]->requests.activity=1;
        if(flags&2)users[1]->requests.activity=2;
        if(flags&4)users[1]->requests.country=1;
        if(flags&8)users[1]->requests.scene_state=0;
        if(flags&16)users[1]->requests.inviter_id=900;
        if(flags&128)users[1]->requests.zone=2;
        record=true;
    }
    unsigned pid(const PartyNormalMembership* p) const {for(unsigned i=0;i<4;++i)if(p==&parties[i])return i+1;return 0;}
    unsigned uid(const PartyMembershipUser* u) const {for(unsigned i=0;i<8;++i)if(u==&users[i]->member)return 100+i;return 0;}
    PartyRequestUser binding(unsigned i) {return {users[i]->member,users[i]->requests,std::static_pointer_cast<void>(users[i])};}
    Bytes state() const {
        Bytes b;append(b,reg.id_counter);append(b,reg.allocation_attempts);append(b,reg.available_count);append(b,static_cast<std::uint32_t>(reg.entries.size()));for(auto& [id,p]:reg.entries){append(b,id);append(b,pid(p));}
        for(auto& p:parties) {
            for(auto n:{p.id,static_cast<std::uint32_t>(p.leader),static_cast<std::uint32_t>(p.count),static_cast<std::uint32_t>(p.item_division),static_cast<std::uint32_t>(p.item_cursor),p.experience.experience_level,static_cast<std::uint32_t>(p.sub_leader),static_cast<std::uint32_t>(p.experience.kind)})append(b,n);
            for(auto n:p.slot_indices)append(b,n);
            for(auto n:p.extra_indices)append(b,n);
            for(auto* u:p.slots)append(b,uid(u));
            for(auto* u:p.extra_slots)append(b,uid(u));
        }
        for(auto& r:users){auto& u=r->member;auto& s=r->requests;for(auto n:{u.id,pid(u.party),s.inviter_id,s.invitation_deadline,static_cast<std::uint32_t>(s.activity),static_cast<std::uint32_t>(s.country),s.scene_state,static_cast<std::uint32_t>(s.zone),static_cast<std::uint32_t>(u.instance),static_cast<std::uint32_t>(u.pending_request)})append(b,n);auto at=b.size();b.resize(at+21,0);for(std::size_t i=0;i<u.name.size();++i)b[at+i]=static_cast<std::uint8_t>(u.name[i]);}
        return b;
    }
    void event(char tag,const Bytes& p={}) {if(!record)return;std::uint64_t h=14695981039346656037ULL;for(auto v:state())h=(h^v)*1099511628211ULL;events.push_back(static_cast<std::uint8_t>(tag));for(unsigned i=0;i<8;++i)events.push_back(static_cast<std::uint8_t>(h>>(8*i)));append(events,static_cast<std::uint32_t>(p.size()));events.insert(events.end(),p.begin(),p.end());}
    Bytes lock_data(PartyRegistryLock k,PartyRegistryParty* p) const {Bytes b;append(b,static_cast<unsigned>(k));append(b,pid(p));return b;}
    unsigned lock_index(PartyRegistryLock k,PartyRegistryParty* p) const {return k==PartyRegistryLock::party?pid(p)+1:static_cast<unsigned>(k);}
    void initialize_lock(PartyRegistryLock k,PartyRegistryParty* p,std::uint32_t) override {event('I',lock_data(k,p));}
    void destroy_lock(PartyRegistryLock k,PartyRegistryParty* p) override {event('D',lock_data(k,p));}
    void lock_registry(PartyRegistryLock k,PartyRegistryParty* p) override {event('[',lock_data(k,p));++depths[lock_index(k,p)];}
    void unlock_registry(PartyRegistryLock k,PartyRegistryParty* p) noexcept override {event(']',lock_data(k,p));--depths[lock_index(k,p)];}
    PartyRegistryParty* allocate_object() override {event('O');return alloc_fail || raw>=4?nullptr:&parties[raw++];}
    void deallocate_object(PartyRegistryParty&) override {throw std::logic_error("Unexpected request recycling");}
    PartyMembershipWorld& membership_world() override {return *this;}
    PartyRegistry& registry() override {return reg;}
    PartyRegistryPorts& registry_ports() override {return *this;}
    void lookup_mutation() {if(mutated)return;if(mutation==1){users[root]->member.party=nullptr;mutated=true;}else if(mutation==3){users[root]->requests.inviter_id=900;users[root]->requests.invitation_deadline=123;mutated=true;}}
    std::optional<PartyRequestUser> find_zone_user(const PartyRequestUser& source,std::uint32_t id) override {Bytes b;append(b,static_cast<std::uint32_t>(source.state.zone));append(b,id);event('Z',b);lookup_mutation();if(missing || id<100 || id>=108 || users[id-100]->requests.zone!=source.state.zone)return std::nullopt;return binding(id-100);}
    std::optional<PartyRequestUser> find_user_name(std::string_view name) override {event('N',Bytes(name.begin(),name.end()));lookup_mutation();if(!missing)for(unsigned i=0;i<8;++i)if(users[i]->member.name==name)return binding(i);return std::nullopt;}
    PartyRequestUser retain_member(PartyMembershipUser& m) override {const auto id=uid(&m);check(id>=100);return binding(id-100);}
    void close_connection(const PartyRequestUser&,std::int32_t reason,std::int32_t detail) override {Bytes b;append(b,static_cast<std::uint32_t>(reason));append(b,static_cast<std::uint32_t>(detail));event('C',b);}
    bool enter_raid(PartyRegistryParty& p,const PartyRequestUser& u) override {Bytes b;append(b,pid(&p));append(b,uid(&u.member));event('R',b);return raid_success;}
    void leave_raid(PartyRegistryParty& p,const PartyRequestUser& u) override {Bytes b;append(b,pid(&p));append(b,uid(&u.member));event('r',b);}
    void expel_user(PartyRegistryParty& p,const PartyRequestUser& u) override {Bytes b;append(b,pid(&p));append(b,uid(&u.member));event('X',b);}
    void raid_request(const PartyRequestUser&,Bytes&) override {throw std::logic_error("Required full raid dispatcher not supplied");}
    std::uint32_t tick() override {const auto n=clock+ticks++*17U;Bytes b;append(b,n);event('K',b);if(mutation==2 && !mutated){users[root]->member.id+=1000;users[root]->member.name="tick-name-0000000000";mutated=true;}return n;}
    void lock(PartyNormalMembership& p) override {lock_registry(PartyRegistryLock::party,static_cast<PartyRegistryParty*>(&p));}
    void unlock(PartyNormalMembership& p) noexcept override {unlock_registry(PartyRegistryLock::party,static_cast<PartyRegistryParty*>(&p));}
    void lock_effects(PartyMembershipUser& u) override {Bytes b;append(b,3);append(b,uid(&u));event('[',b);}
    void unlock_effects(PartyMembershipUser& u) noexcept override {Bytes b;append(b,3);append(b,uid(&u));event(']',b);}
    void send_party(PartyNormalMembership&,const Bytes& p) override {event('P',p);}
    void send_user(PartyMembershipUser& u,const Bytes& p) override {Bytes b;append(b,uid(&u));b.insert(b.end(),p.begin(),p.end());event('U',b);if(mutation==4 && !nested && p.size()>=2 && (p[0]==2 || p[0]==9) && p[1]==0xb){nested=true;Bytes n;append(n,uid(&u));event('V',n);Bytes accept{3,0xb,0};dispatch_party_request(binding(uid(&u)-100),accept,*this);}}
    void send_scene(PartyMembershipUser&,const Bytes& p) override {event('S',p);}
    void unregister_party(PartyNormalMembership& p) override {unregister_registry_membership(reg,p,*this);}
    void cleanup_guild_creation(PartyMembershipUser&) override {throw std::logic_error("Unexpected guild dependency");}
    void fill_log(PartyMembershipUser& u,std::span<std::uint8_t> p) override {auto id=uid(&u);Bytes b;append(b,id);event('F',b);for(std::size_t i=0;i<p.size();++i)p[i]=static_cast<std::uint8_t>(id+i);}
    void send_log(const Bytes& p) override {event('L',p);}
};
}
int main(int argc,char** argv) {
    try {
        check(argc==2);std::ifstream input(argv[1]);check(input.good());std::string line;unsigned cases=0,nested=0,requests=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#')continue;
            std::istringstream start(line);char tag;unsigned scenario,profile,flags,mutation,missing,alloc_fail,raid;std::uint32_t clock;start>>tag>>scenario>>profile>>flags>>mutation>>missing>>alloc_fail>>raid>>clock;check(tag=='S' && static_cast<bool>(start));World world(profile,flags,mutation,missing!=0,alloc_fail!=0,raid!=0,clock);
            while(std::getline(input,line) && line!="Z") {
                std::istringstream command(line);std::string packet;command>>tag>>world.root>>packet;check(tag=='T' && static_cast<bool>(command));auto p=unhex(packet);world.events.clear();
                if(p[0]==3 && p[1]==0xb){world.users[1]->requests.inviter_id=flags&32?0:100;world.users[1]->requests.invitation_deadline=0xfffffff0;}
                check(static_cast<bool>(std::getline(input,line)));std::istringstream result(line);std::string expected_packet,state,events;result>>tag>>expected_packet>>state>>events;check(tag=='R' && static_cast<bool>(result));
                dispatch_party_request(world.binding(world.root),p,world);
                if(p!=unhex(expected_packet) || world.state()!=unhex(state) || world.events!=unhex(events)) {std::size_t i=0;const auto e=unhex(events);while(i<e.size() && i<world.events.size() && e[i]==world.events[i])++i;throw std::runtime_error("Original PacketParty mismatch case "+std::to_string(scenario)+" state equal "+std::to_string(world.state()==unhex(state))+" event byte "+std::to_string(i));}
                for(auto depth:world.depths)check(depth==0);
                ++requests;
            }
            check(line=="Z");nested+=world.nested?1:0;++cases;
        }
        check(cases==1024 && nested>0);std::cout<<cases<<" cases / "<<requests<<" whole original normal requests / "<<nested<<" literal nested accepts passed\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
