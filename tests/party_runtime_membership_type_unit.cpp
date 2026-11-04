#include "eternity/game/party_runtime_membership_type.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
using Bytes=eternity::net::Packet;
namespace {
void check(bool ok) {if(!ok)throw std::runtime_error("Invalid party type fixture/lock state");}
void append(Bytes& bytes,std::uint32_t value) {for(unsigned i=0;i<4;++i)bytes.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes bytes;if(text=="-")return bytes;for(std::size_t i=0;i<text.size();i+=2)bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return bytes;}
struct Action {unsigned membership{},target{};std::int32_t count{};unsigned leader{},match{};};
struct World final:PartyMembershipWorld {
    std::array<PartyNormalMembership,2> parties;
    PartyMembershipUser user,other;
    std::array<unsigned,2> depth{};
    std::array<Action,4> actions;
    unsigned ordinal{};
    Bytes events;
    unsigned identity(PartyNormalMembership* party) const {return !party?0:party==&parties[0]?1:2;}
    Bytes state() {
        Bytes bytes;append(bytes,identity(user.party));for(auto value:depth)append(bytes,value);
        for(auto& party:parties)append(bytes,static_cast<std::uint32_t>(party.count));
        for(auto& party:parties)append(bytes,static_cast<std::uint32_t>(party.leader));
        for(auto& party:parties)append(bytes,party.slots[static_cast<std::size_t>(party.leader)]==&user?1:0);
        return bytes;
    }
    void callback(PartyNormalMembership& party,bool locking) {
        const auto index=identity(&party)-1;check(ordinal<actions.size());
        if(locking)++depth[index];else {check(depth[index]>0);--depth[index];}
        const auto& action=actions[ordinal++];
        if(action.membership!=255)user.party=action.membership?&parties[action.membership-1]:nullptr;
        if(action.target) {
            auto& changed=parties[action.target-1];changed.count=action.count;changed.leader=static_cast<std::int32_t>(action.leader);
            changed.slots[action.leader]=action.match?&user:&other;
        }
        append(events,index+1);append(events,locking?1:0);const auto bytes=state();events.insert(events.end(),bytes.begin(),bytes.end());
    }
    void lock(PartyNormalMembership& party) override {callback(party,true);}
    void unlock(PartyNormalMembership& party) noexcept override {callback(party,false);}
    // The original function uses only locks. All unrelated providers fail if
    // reached, so this suite cannot silently mask an added world dependency.
    [[noreturn]] static void unexpected() {throw std::logic_error("Unexpected party type provider");}
    std::uint32_t tick() override {unexpected();}
    void lock_effects(PartyMembershipUser&) override {unexpected();}
    void unlock_effects(PartyMembershipUser&) noexcept override {std::terminate();}
    void send_party(PartyNormalMembership&,const Bytes&) override {unexpected();}
    void send_user(PartyMembershipUser&,const Bytes&) override {unexpected();}
    std::uint8_t party_type(PartyMembershipUser&) override {unexpected();}
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
            World world;std::istringstream row(line);unsigned initial;row>>initial;
            world.user.party=initial?&world.parties[initial-1]:nullptr;
            for(auto& party:world.parties) {
                unsigned match,kind;row>>party.count>>party.leader>>match>>kind;party.experience.kind=static_cast<std::uint8_t>(kind);
                party.slots.fill(&world.other);party.slots[static_cast<std::size_t>(party.leader)]=match?&world.user:&world.other;
            }
            for(auto& action:world.actions)row>>action.membership>>action.target>>action.count>>action.leader>>action.match;
            unsigned result;std::string state,events;row>>result>>state>>events;check(static_cast<bool>(row));
            const auto actual=party_membership_type(world.user,world);
            if(actual!=result || world.state()!=unhex(state) || world.events!=unhex(events))throw std::runtime_error("Original GetPartyType mismatch case "+std::to_string(cases));
            check(world.depth==std::array<unsigned,2>{0,0});++cases;
        }
        check(cases==1024);std::cout<<cases<<" complete original GetPartyType comparisons passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
