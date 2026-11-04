#include "eternity/game/party_runtime_raid.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Bytes=eternity::net::Packet;
namespace {
void check(bool ok) {if(!ok)throw std::runtime_error("Invalid party send fixture/ownership state");}
void append(Bytes& bytes,std::uint32_t value) {for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
Bytes unhex(const std::string& s) {Bytes p;if(s=="-")return p;for(std::size_t i=0;i<s.size();i+=2)p.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return p;}
struct World final:PartyRaidMembershipWorld {
    PartyRegistryParty party;
    std::array<PartyMembershipUser,30> users;
    PartyMembershipSendBuffer buffer;
    Bytes payload,events;
    bool allocated{},freed{},mutate_input{};
    unsigned completion{},mutation{},queues{},depth{},frees{};
    World() {buffer.references=17;buffer.custom_references=19;}
    Bytes state() {
        Bytes bytes;append(bytes,static_cast<std::uint32_t>(party.count));append(bytes,party.experience.kind);append(bytes,buffer.references.load());append(bytes,buffer.custom_references);append(bytes,freed?1:0);
        for(std::int32_t i=0;i<30;++i){auto* user=raid_membership_slot(party,i);append(bytes,user?user->id:0);}
        for(auto& user:users)append(bytes,static_cast<std::uint32_t>(user.send_socket));
        return bytes;
    }
    void event(char tag,const Bytes& data={}) {events.push_back(static_cast<std::uint8_t>(tag));const auto s=state();events.insert(events.end(),s.begin(),s.end());append(events,static_cast<std::uint32_t>(data.size()));events.insert(events.end(),data.begin(),data.end());}
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t capacity) override {Bytes bytes;append(bytes,capacity);event('A',bytes);return allocated?&buffer:nullptr;}
    void free_send_buffer(PartyMembershipSendBuffer& value) override {check(&value==&buffer && !freed && value.references==0);freed=true;++frees;event('B');}
    void enqueue_send(PartyMembershipUser& user,PartyMembershipSendBuffer& value) override {
        check(&value==&buffer && depth==1 && user.send_socket!=-1);Bytes bytes;append(bytes,user.id);event('E',bytes);++queues;
        if(queues==1) {
            std::int32_t index=0;while(index<30 && raid_membership_slot(party,index)!=&user)++index;check(index<30);
            if(mutation==1)party.count=0;
            else if(mutation==2)party.count=30;
            else if(mutation==3 && index+1<30) {raid_membership_slot(party,index+1)=&users[29];users[29].send_socket=0;}
            else if(mutation==4 && index+1<30)raid_membership_slot(party,index+1)=nullptr;
            else if(mutation==5)party.experience.kind=2;
            else if(mutation==6 && index+1<30 && raid_membership_slot(party,index+1))raid_membership_slot(party,index+1)->send_socket=-1;
        }
        if(completion==1 || (completion==2 && queues%2==1))release_party_membership_buffer(buffer,*this);
    }
    void lock(PartyNormalMembership& value) override {check(&value==&party && depth==0);++depth;event('[');if(mutate_input)std::fill(payload.begin(),payload.end(),0);}
    void unlock(PartyNormalMembership&) noexcept override {event(']');--depth;}
    [[noreturn]] static void unexpected() {throw std::logic_error("Unexpected party send provider");}
    std::uint32_t tick() override {unexpected();}
    void lock_effects(PartyMembershipUser&) override {unexpected();}
    void unlock_effects(PartyMembershipUser&) noexcept override {std::terminate();}
    void send_user(PartyMembershipUser&,const Bytes&) override {unexpected();}
    void close_send_socket(PartyMembershipUser&,std::int32_t) override {unexpected();}
    void send_scene(PartyMembershipUser&,const Bytes&) override {unexpected();}
    void unregister_party(PartyNormalMembership&) override {unexpected();}
    void cleanup_guild_creation(PartyMembershipUser&) override {unexpected();}
    void fill_log(PartyMembershipUser&,std::span<std::uint8_t>) override {unexpected();}
    void send_log(const Bytes&) override {unexpected();}
};
}
int main(int argc,char** argv) {
    try {
        check(argc==2);std::ifstream input(argv[1]);check(input.good());std::string line;unsigned cases=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#')continue;
            World world;std::istringstream row(line);unsigned kind,mask,allocated,mutate;std::string payload;
            row>>kind>>world.party.count>>mask>>allocated>>world.completion>>world.mutation>>mutate;world.party.experience.kind=static_cast<std::uint8_t>(kind);world.allocated=allocated!=0;world.mutate_input=mutate!=0;
            for(unsigned i=0;i<30;++i) {auto& user=world.users[i];user.id=100+i;row>>user.send_socket;raid_membership_slot(world.party,static_cast<std::int32_t>(i))=(mask>>i)&1?&user:nullptr;}
            row>>payload;check(static_cast<bool>(row));world.payload=unhex(payload);
            check(static_cast<bool>(std::getline(input,line)));std::istringstream result(line);char tag;std::string state,frame,events;result>>tag>>state>>frame>>events;check(tag=='R' && static_cast<bool>(result));
            world.send_party(world.party,world.payload);
            if(world.state()!=unhex(state) || world.buffer.frame!=unhex(frame) || world.events!=unhex(events))throw std::runtime_error("Original party send mismatch case "+std::to_string(cases));
            check(world.depth==0);
            // Simulate asynchronous consumers releasing their retained shares
            // after the original-function comparison, checking one recycle only.
            if(world.allocated && !world.freed) {
                const auto retained=world.buffer.references.load();check(retained>0);
                for(std::uint32_t i=0;i<retained;++i)release_party_membership_buffer(world.buffer,world);
            }
            check(world.frees==(world.allocated?1U:0U));++cases;
        }
        check(cases==512);std::cout<<cases<<" complete original party send/ownership comparisons passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
