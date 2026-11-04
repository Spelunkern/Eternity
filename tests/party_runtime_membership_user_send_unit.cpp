#include "eternity/game/party_runtime_membership_user_send.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Bytes=eternity::net::Packet;
namespace {
void check(bool ok) {if(!ok)throw std::runtime_error("Invalid direct-send ownership/fixture");}
void append(Bytes& bytes,std::uint32_t n) {for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
Bytes unhex(const std::string& s) {Bytes p;if(s=="-")return p;for(std::size_t i=0;i<s.size();i+=2)p.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return p;}
struct World final:PartyMembershipUserSendWorld {
    PartyMembershipUser user;
    PartyMembershipSendBuffer buffer;
    Bytes payload,events;
    bool allocated{},completion{},close_mutation{},freed{},used{};
    unsigned mutation{},frees{};
    World() {buffer.references=17;buffer.custom_references=19;}
    Bytes state() {Bytes bytes;for(auto n:{user.send_error,user.send_error_detail,static_cast<std::uint32_t>(user.send_socket),buffer.references.load(),buffer.custom_references,freed?1U:0U})append(bytes,n);return bytes;}
    void event(char tag,const Bytes& payload={}) {events.push_back(static_cast<std::uint8_t>(tag));const auto bytes=state();events.insert(events.end(),bytes.begin(),bytes.end());append(events,static_cast<std::uint32_t>(payload.size()));events.insert(events.end(),payload.begin(),payload.end());}
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t capacity) override {
        Bytes bytes;append(bytes,capacity);event('A',bytes);
        if(mutation==1 || mutation==4)user.send_socket=mutation==1?-1:0;
        if(mutation==2 || mutation==4)std::fill(payload.begin(),payload.end(),0);
        if(mutation==3) {user.send_error=99;user.send_error_detail=77;}
        used=allocated;return allocated?&buffer:nullptr;
    }
    void free_send_buffer(PartyMembershipSendBuffer& value) override {check(&value==&buffer && !freed && buffer.references==0);freed=true;++frees;event('B');}
    void enqueue_send(PartyMembershipUser& target,PartyMembershipSendBuffer& value) override {check(&target==&user && &value==&buffer);Bytes bytes;append(bytes,value.references);bytes.insert(bytes.end(),value.frame.begin(),value.frame.end());event('E',bytes);if(completion)release_party_membership_buffer(value,*this);}
    void close_send_socket(PartyMembershipUser& target,std::int32_t socket) override {check(&target==&user);Bytes bytes;append(bytes,static_cast<std::uint32_t>(socket));event('C',bytes);if(close_mutation){user.send_socket=123;user.send_error=44;user.send_error_detail=55;}}
    [[noreturn]] static void unexpected() {throw std::logic_error("Unexpected direct roster provider");}
    std::uint32_t tick() override {unexpected();}
    void lock(PartyNormalMembership&) override {unexpected();}
    void unlock(PartyNormalMembership&) noexcept override {std::terminate();}
    void lock_effects(PartyMembershipUser&) override {unexpected();}
    void unlock_effects(PartyMembershipUser&) noexcept override {std::terminate();}
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
            World world;unsigned allocated,completion,close_mutation;std::string payload;
            std::istringstream row(line);row>>world.user.send_socket>>world.user.send_error>>world.user.send_error_detail>>allocated>>completion>>world.mutation>>close_mutation>>payload;check(static_cast<bool>(row));world.allocated=allocated!=0;world.completion=completion!=0;world.close_mutation=close_mutation!=0;world.payload=unhex(payload);
            check(static_cast<bool>(std::getline(input,line)));std::istringstream result(line);char tag;std::string state,frame,events;result>>tag>>state>>frame>>events;check(tag=='R' && static_cast<bool>(result));
            world.send_user(world.user,world.payload);
            if(world.state()!=unhex(state) || world.buffer.frame!=unhex(frame) || world.events!=unhex(events))throw std::runtime_error("Original direct roster send differs at case "+std::to_string(cases));
            if(world.used && !world.freed) {check(world.buffer.references==1);release_party_membership_buffer(world.buffer,world);}
            check(world.frees==(world.used?1U:0U));++cases;
        }
        check(cases==1024);std::cout<<cases<<" complete original direct-roster send comparisons passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
