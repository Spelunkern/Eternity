#include "eternity/game/party_runtime_registry_break.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Bytes=eternity::net::Packet;
namespace {
void check(bool ok) {if(!ok)throw std::runtime_error("Invalid registry fixture/owner state");}
void append(Bytes& b,std::uint32_t v) {for(unsigned i=0;i<4;++i)b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
Bytes unhex(const std::string& s) {Bytes b;if(s=="-")return b;for(std::size_t i=0;i<s.size();i+=2)b.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return b;}
struct World final:PartyRegistryPorts,PartyMembershipWorld {
    PartyRegistry registry;
    std::array<PartyRegistryParty,6> parties;
    std::array<PartyMembershipUser,2> users;
    std::array<unsigned,8> depths{};
    Bytes events;
    unsigned scenario{},raw=2,attempt{},freed{};
    bool fail{},breaking{},mutated{};
    unsigned break_mutation{};
    PartyRegistryParty* subject{};
    World(unsigned s,bool f):scenario(s),fail(f) {
        registry.id_counter=9;registry.allocation_attempts=17;registry.available_count=19;
        for(unsigned i=0;i<6;++i) {
            auto& p=parties[i];p.id=(i+1)*100+s;p.leader=1;p.count=2;p.item_division=-7;p.item_cursor=3;p.experience.experience_level=99;p.sub_leader=9;p.experience.kind=1;p.auto_join=true;
            p.previous=i?&parties[i-1]:nullptr;p.next=i<5?&parties[i+1]:nullptr;
            for(unsigned j=0;j<30;++j) {if(j<7){p.slot_indices[j]=j+10+i;p.slots[j]=&users[(j+i)%2];}else{p.extra_indices[j-7]=j+10+i;p.extra_slots[j-7]=&users[(j+i)%2];}}
        }
        for(unsigned i=0;i<2;++i) {
            auto& u=users[i];u.id=100+i;u.level=static_cast<std::uint16_t>(20+i);u.job=static_cast<std::uint8_t>(1+i);
            u.name="registry-"+std::string(10,'0')+std::to_string(i);u.pending_request=9;u.in_zone=(s+i)%2!=0;
            u.hp=static_cast<std::uint16_t>(100+i);u.maximum_hp=static_cast<std::uint16_t>(200+i);u.mp=static_cast<std::uint16_t>(300+i);u.maximum_mp=static_cast<std::uint16_t>(400+i);u.sp=static_cast<std::uint16_t>(500+i);u.maximum_sp=static_cast<std::uint16_t>(600+i);u.instance=1;u.position={1.25F+static_cast<float>(i),-2.5F,3.75F};
        }
    }
    unsigned identity(const PartyNormalMembership* p) const {for(unsigned i=0;i<6;++i)if(p==&parties[i])return i+1;return 0;}
    unsigned user_id(const PartyMembershipUser* u) const {for(unsigned i=0;i<2;++i)if(u==&users[i])return 100+i;return 0;}
    Bytes state() const {
        Bytes b;append(b,registry.id_counter);append(b,identity(registry.available));append(b,registry.allocation_attempts);append(b,registry.available_count);append(b,freed);append(b,static_cast<std::uint32_t>(registry.entries.size()));
        for(const auto& [id,p]:registry.entries){append(b,id);append(b,identity(p));}
        for(const auto& p:parties) {
            for(auto v:{p.id,static_cast<std::uint32_t>(p.leader),static_cast<std::uint32_t>(p.count),static_cast<std::uint32_t>(p.item_division),static_cast<std::uint32_t>(p.item_cursor),p.experience.experience_level,static_cast<std::uint32_t>(p.sub_leader)})append(b,v);
            append(b,p.experience.kind);append(b,p.auto_join?1:0);append(b,identity(p.previous));append(b,identity(p.next));
            for(auto v:p.slot_indices)append(b,v);
            for(auto v:p.extra_indices)append(b,v);
            for(auto* u:p.slots)append(b,user_id(u));
            for(auto* u:p.extra_slots)append(b,user_id(u));
        }
        for(const auto& u:users){append(b,identity(u.party));append(b,u.pending_request);}return b;
    }
    void event(char tag,const Bytes& payload={}) {
        std::uint64_t hash=14695981039346656037ULL;for(auto byte:state())hash=(hash^byte)*1099511628211ULL;
        events.push_back(static_cast<std::uint8_t>(tag));for(unsigned i=0;i<8;++i)events.push_back(static_cast<std::uint8_t>(hash>>(8*i)));append(events,static_cast<std::uint32_t>(payload.size()));events.insert(events.end(),payload.begin(),payload.end());
    }
    Bytes lock_data(PartyRegistryLock kind,PartyRegistryParty* p) const {Bytes b;append(b,static_cast<unsigned>(kind));append(b,identity(p));return b;}
    unsigned lock_index(PartyRegistryLock kind,PartyRegistryParty* p) const {return kind==PartyRegistryLock::party?identity(p)+1:static_cast<unsigned>(kind);}
    void initialize_lock(PartyRegistryLock kind,PartyRegistryParty* p,std::uint32_t spin) override {auto b=lock_data(kind,p);append(b,spin);event('I',b);}
    void destroy_lock(PartyRegistryLock kind,PartyRegistryParty* p) override {event('D',lock_data(kind,p));}
    void lock_registry(PartyRegistryLock kind,PartyRegistryParty* p) override {event('[',lock_data(kind,p));++depths[lock_index(kind,p)];}
    void unlock_registry(PartyRegistryLock kind,PartyRegistryParty* p) noexcept override {event(']',lock_data(kind,p));--depths[lock_index(kind,p)];}
    PartyRegistryParty* allocate_object() override {event('O');++attempt;if((fail && attempt==1) || raw>=6)return nullptr;return &parties[raw++];}
    void deallocate_object(PartyRegistryParty& p) override {const auto id=identity(&p);freed|=1U<<(id-1);Bytes b;append(b,id);event('V',b);}
    std::uint32_t tick() override {event('K');return 2000+scenario;}
    void lock(PartyNormalMembership& p) override {lock_registry(PartyRegistryLock::party,static_cast<PartyRegistryParty*>(&p));}
    void unlock(PartyNormalMembership& p) noexcept override {unlock_registry(PartyRegistryLock::party,static_cast<PartyRegistryParty*>(&p));}
    void lock_effects(PartyMembershipUser& u) override {Bytes b;append(b,3);append(b,u.id);event('[',b);}
    void unlock_effects(PartyMembershipUser& u) noexcept override {Bytes b;append(b,3);append(b,u.id);event(']',b);}
    void send_party(PartyNormalMembership&,const Bytes& p) override {event('P',p);}
    void send_user(PartyMembershipUser& u,const Bytes& p) override {Bytes b;append(b,u.id);b.insert(b.end(),p.begin(),p.end());event('U',b);}
    PartyMembershipUser*& projected_slot(PartyRegistryParty& p,std::size_t i) {return i<7?p.slots[i]:p.extra_slots[i-7];}
    std::size_t first_slot(PartyMembershipUser& user) {for(std::size_t i=0;i<30;++i)if(projected_slot(*subject,i)==&user)return i;throw std::logic_error("Missing reentry slot");}
    void send_scene(PartyMembershipUser& user,const Bytes& p) override {
        event('S',p);
        if(breaking && !mutated && break_mutation>=1 && break_mutation<=4) {
            mutated=true;const auto i=first_slot(user);
            if(break_mutation==1)subject->count=0;
            else if(break_mutation==2)subject->count=30;
            else if(break_mutation==3)projected_slot(*subject,i)=&users[&user==&users[0]?1:0];
            else if(i+1<30)projected_slot(*subject,i+1)=nullptr;
        }
    }
    void unregister_party(PartyNormalMembership& p) override {unregister_registry_membership(registry,p,*this);}
    void cleanup_guild_creation(PartyMembershipUser&) override {throw std::logic_error("Unexpected guild dependency");}
    void fill_log(PartyMembershipUser& u,std::span<std::uint8_t> p) override {Bytes b;append(b,u.id);event('F',b);for(std::size_t i=0;i<p.size();++i)p[i]=static_cast<std::uint8_t>(u.id+i);if(breaking && !mutated && (break_mutation==5 || break_mutation==6)){mutated=true;const auto i=first_slot(u);if(break_mutation==5)++subject->id;else if(i+1<30){projected_slot(*subject,i+1)=nullptr;--subject->count;}}}
    void send_log(const Bytes& p) override {event('L',p);}
};
}
int main(int argc,char** argv) {
    try {
        check(argc==2 || (argc==3 && std::string(argv[2])=="--break"));const bool break_mode=argc==3;std::ifstream input(argv[1]);check(input.good());std::string line;unsigned scenarios=0,operations=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#')continue;
            std::istringstream start(line);char tag;unsigned scenario,fail;start>>tag>>scenario>>fail;check(tag=='S' && static_cast<bool>(start));World w(scenario,fail!=0);if(break_mode){start>>w.break_mutation;check(static_cast<bool>(start));}
            while(std::getline(input,line) && line!="Z") {
                std::istringstream command(line);char op;command>>op;w.events.clear();unsigned actual=0;
                if(op=='I')initialize_party_registry(w.registry,w);
                else if(op=='T' || op=='X') {unsigned i;command>>i;if(op=='T')construct_registry_party(w.parties[i],w);else reset_registry_party(w.parties[i]);}
                else if(op=='H') {
                    std::uint32_t counter,id0,id1;unsigned count;command>>counter>>count>>id0>>id1;w.registry.id_counter=counter;w.registry.available=count?&w.parties[0]:nullptr;w.registry.allocation_attempts=7;w.registry.available_count=count;w.parties[0].id=id0;w.parties[1].id=id1;w.parties[0].next=count==2?&w.parties[1]:nullptr;w.parties[1].next=nullptr;
                }
                else if(op=='W') {
                    unsigned i,kind;std::int32_t count;std::uint32_t mask;command>>i>>count>>mask>>kind;auto& p=w.parties[i];p.count=count;p.leader=0;p.item_division=7;p.item_cursor=9;p.experience.experience_level=77;p.sub_leader=2;p.experience.kind=static_cast<std::uint8_t>(kind);p.auto_join=true;
                    for(unsigned j=0;j<30;++j){if(j<7)p.slot_indices[j]=0xbeef+j;else p.extra_indices[j-7]=0xbeef+j;w.projected_slot(p,j)=(mask>>j)&1?&w.users[j%2]:nullptr;}
                    for(unsigned u=0;u<2;++u){w.users[u].party=nullptr;w.users[u].guild_creation=true;for(std::int32_t j=0;j<count;++j)if((mask>>static_cast<unsigned>(j))&1 && static_cast<unsigned>(j)%2==u)w.users[u].party=&p;}
                }
                else if(op=='B') {unsigned i;command>>i;w.breaking=true;w.mutated=false;w.subject=&w.parties[i];actual=break_registry_party(w.registry,w.parties[i],w,w);w.breaking=false;}
                else if(op=='C') {int u;std::int32_t div;command>>u>>div;actual=w.identity(create_registry_party(w.registry,u<0?nullptr:&w.users[static_cast<unsigned>(u)],div,w,w));}
                else if(op=='F') {std::uint32_t id;command>>id;actual=w.identity(find_registry_party(w.registry,id,w));}
                else if(op=='R') {int i;command>>i;remove_registry_party(w.registry,i<0?nullptr:&w.parties[static_cast<unsigned>(i)],w);}
                else if(op=='J' || op=='L') {unsigned i,u;command>>i>>u;actual=op=='J'?enter_normal_party(w.parties[i],w.users[u],w):leave_normal_party(w.parties[i],w.users[u],w);}
                else if(op=='K') {std::uint32_t counter;command>>counter;w.registry.id_counter=counter;}
                else if(op=='Q') {unsigned i;int tail;command>>i>>tail;w.registry.available=&w.parties[i];w.registry.available_count=tail<0?1U:2U;w.parties[i].next=tail<0?nullptr:&w.parties[static_cast<unsigned>(tail)];if(tail>=0)w.parties[static_cast<unsigned>(tail)].next=nullptr;}
                else if(op=='A')actual=w.identity(allocate_registry_party(w.registry,w));
                else if(op=='D')destroy_party_registry(w.registry,w);
                else check(false);
                check(static_cast<bool>(command));check(static_cast<bool>(std::getline(input,line)));std::istringstream result(line);unsigned expected;std::string state,events;result>>tag>>expected>>state>>events;check(tag=='R' && static_cast<bool>(result));
                if(actual!=expected || w.state()!=unhex(state) || w.events!=unhex(events))throw std::runtime_error("Original registry mismatch scenario "+std::to_string(scenario)+" operation "+std::string(1,op)+" number "+std::to_string(operations)+" state equal "+std::to_string(w.state()==unhex(state)));
                for(auto depth:w.depths)check(depth==0);
                ++operations;
            }
            check(line=="Z");++scenarios;
        }
        check(scenarios==(break_mode?256U:64U));std::cout<<scenarios<<" original registry lifetimes / "<<operations<<" state/action comparisons passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
