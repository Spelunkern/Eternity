#include "eternity/game/party_runtime_membership_user_send.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Bytes=eternity::net::Packet;
namespace {
void check(bool ok) {if(!ok) throw std::runtime_error("Invalid membership fixture or lock state");}
void append(Bytes& p,std::uint32_t n) {for(unsigned i=0;i<4;++i) p.push_back(static_cast<std::uint8_t>(n>>(i*8)));}
Bytes unhex(const std::string& s) {Bytes p;if(s=="-")return p;for(std::size_t i=0;i<s.size();i+=2)p.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return p;}
struct World final:PartyMembershipUserSendWorld {
    PartyNormalMembership p;
    std::array<PartyMembershipUser,8> users;
    Bytes events;
    unsigned depth{};
    bool real_type{},real_send{},real_user_send{},buffer_active{};
    PartyMembershipSendBuffer send_buffer;
    std::array<bool,8> effect_locks{};
    std::uint32_t now{};
    Bytes state() {
        Bytes b;append(b,static_cast<std::uint32_t>(p.count));append(b,static_cast<std::uint32_t>(p.leader));append(b,static_cast<std::uint32_t>(p.item_cursor));append(b,p.experience.experience_level);
        std::uint32_t mask=0;for(unsigned i=0;i<8;++i) if(users[i].party==&p)mask|=1U<<i;
        append(b,mask);for(auto* user:p.slots)append(b,user?user->id:0);
        for(const auto& user:users)append(b,user.pending_request);
        for(const auto& user:users)append(b,static_cast<std::uint32_t>(user.effect_cursor));
        return b;
    }
    void event(char tag,const Bytes& payload={}) {
        events.push_back(static_cast<std::uint8_t>(tag));const auto s=state();events.insert(events.end(),s.begin(),s.end());append(events,static_cast<std::uint32_t>(payload.size()));events.insert(events.end(),payload.begin(),payload.end());
    }
    static Bytes id(PartyMembershipUser& user) {Bytes b;append(b,user.id);return b;}
    std::uint32_t tick() override {event('K');return now;}
    void lock(PartyNormalMembership& party) override {check(&party==&p);event('[');++depth;}
    void unlock(PartyNormalMembership&) noexcept override {event(']');--depth;}
    void lock_effects(PartyMembershipUser& user) override {check(depth>0);const auto i=static_cast<std::size_t>(&user-users.data());check(!effect_locks[i]);event('{',id(user));effect_locks[i]=true;}
    void unlock_effects(PartyMembershipUser& user) noexcept override {event('}',id(user));effect_locks[static_cast<std::size_t>(&user-users.data())]=false;}
    void send_party(PartyNormalMembership& party,const Bytes& packet) override {check(depth>0);event('P',packet);if(real_send)PartyMembershipSendWorld::send_party(party,packet);}
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t capacity) override {check(real_send && !buffer_active);buffer_active=true;Bytes bytes;append(bytes,capacity);event('A',bytes);return &send_buffer;}
    void free_send_buffer(PartyMembershipSendBuffer& buffer) override {check(buffer_active && &buffer==&send_buffer && buffer.references==0);buffer_active=false;Bytes bytes;append(bytes,0);event('B',bytes);}
    void enqueue_send(PartyMembershipUser& user,PartyMembershipSendBuffer& buffer) override {check(buffer_active && &buffer==&send_buffer && depth>0);auto bytes=id(user);append(bytes,buffer.references);bytes.insert(bytes.end(),buffer.frame.begin(),buffer.frame.end());event('E',bytes);release_party_membership_buffer(buffer,*this);}
    void send_user(PartyMembershipUser& user,const Bytes& packet) override {check(depth>0);auto b=id(user);b.insert(b.end(),packet.begin(),packet.end());event('U',b);if(real_user_send)PartyMembershipUserSendWorld::send_user(user,packet);}
    void close_send_socket(PartyMembershipUser&,std::int32_t) override {check(false);}
    std::uint8_t party_type(PartyMembershipUser& user) override {
        const std::uint8_t type=real_type?PartyMembershipWorld::party_type(user):!user.party?0:(p.count>0 && p.slots[static_cast<std::size_t>(p.leader)]==&user)?2:1;
        auto b=id(user);append(b,type);event('Y',b);return type;
    }
    void send_scene(PartyMembershipUser& user,const Bytes& packet) override {check(user.in_zone);event('S',packet);}
    void unregister_party(PartyNormalMembership&) override {check(depth>0 && p.count==0);event('D');}
    void cleanup_guild_creation(PartyMembershipUser& user) override {check(depth>0 && user.guild_creation);event('G',id(user));user.guild_creation=false;}
    void fill_log(PartyMembershipUser& user,std::span<std::uint8_t> bytes) override {event('F',id(user));check(bytes.size()==74);for(std::size_t i=0;i<bytes.size();++i)bytes[i]=static_cast<std::uint8_t>(user.id+i);}
    void send_log(const Bytes& packet) override {event('L',packet);}
};
}
int main(int argc,char** argv) {
    try {
        check(argc>=2 && argc<=5);bool real_type=false,real_send=false,real_user_send=false;
        for(int i=2;i<argc;++i) {const std::string flag=argv[i];check(flag=="--real-party-type" || flag=="--real-party-send" || flag=="--real-user-send");if(flag=="--real-party-type")real_type=true;else if(flag=="--real-party-send")real_send=true;else real_user_send=true;}
        real_send=real_send || real_user_send;real_type=real_type || real_send;std::ifstream input(argv[1]);check(input.good());std::string line;unsigned cases=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#')continue;
            World world;world.real_type=real_type;world.real_send=real_send;world.real_user_send=real_user_send;unsigned operation,target;char tag;
            std::istringstream row(line);row>>tag>>operation>>world.p.count>>world.p.leader>>world.p.item_cursor>>world.p.experience.experience_level>>world.p.id>>world.now>>target;check(tag=='C' && static_cast<bool>(row));
            for(unsigned i=0;i<8;++i) {
                check(static_cast<bool>(std::getline(input,line)));std::istringstream user_row(line);auto& user=world.users[i];unsigned job,zone,guild,count;
                user_row>>tag>>user.id>>user.level>>job>>user.hp>>user.maximum_hp>>user.mp>>user.maximum_mp>>user.sp>>user.maximum_sp>>user.instance;
                for(auto& value:user.position)user_row>>value;
                user_row>>user.name>>zone>>guild;if(real_send)user_row>>user.send_socket;user_row>>count;user.job=static_cast<std::uint8_t>(job);user.in_zone=zone!=0;user.guild_creation=guild!=0;
                user.party=static_cast<std::int32_t>(i)<world.p.count?&world.p:nullptr;user.pending_request=9;user.effect_cursor=count;
                for(unsigned j=0;j<count;++j) {PartyMembershipEffect effect;unsigned level,visibility;user_row>>effect.skill>>level>>visibility>>effect.deadline;effect.level=static_cast<std::uint8_t>(level);effect.visibility=static_cast<std::uint8_t>(visibility);user.effects.push_back(effect);}
                check(tag=='U' && static_cast<bool>(user_row));
                if(i<7) {world.p.slots[i]=&user;world.p.slot_indices[i]=900+i;}
            }
            check(static_cast<bool>(std::getline(input,line)));std::istringstream result(line);unsigned expected;std::string state,events;result>>tag>>expected>>state>>events;check(tag=='R' && static_cast<bool>(result));
            const auto success=operation==0?enter_normal_party(world.p,world.users[target],world):leave_normal_party(world.p,world.users[target],world);
            const auto actual=world.state(), wanted=unhex(state), expected_events=unhex(events);
            if(success!=(expected!=0) || actual!=wanted || world.events!=expected_events) {
                std::size_t mismatch=0;while(mismatch<world.events.size() && mismatch<expected_events.size() && world.events[mismatch]==expected_events[mismatch])++mismatch;
                throw std::runtime_error("Original membership mismatch case "+std::to_string(cases)+" event byte "+std::to_string(mismatch)+" state equal "+std::to_string(actual==wanted));
            }
            check(world.depth==0 && !world.buffer_active);for(unsigned i=0;i<7;++i)check(world.p.slot_indices[i]==900+i);
            ++cases;
        }
        check(cases==512);std::cout<<cases<<" complete original normal-party membership comparisons passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
