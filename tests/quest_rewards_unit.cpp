#include "eternity/game/quest_rewards.hpp"
#include "eternity/game/money.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet decode(const std::string& text) {net::Packet bytes;check(text.size()%2==0,"Malformed reward fixture hex");for(std::size_t i=0;i<text.size();i+=2) bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return bytes;}
void append(net::Packet& packet,std::uint32_t value) {for(unsigned i=0;i<4;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
struct Case {
    unsigned scenario=0,variant=0;std::uint32_t money=0;unsigned mutate_exp=0,mutate_db=0,header_alt=0,item_success=0,bag=0,slot=0;std::uint32_t context=0;
};
struct World final:QuestRewardWorld {
    const Case& c;const QuestDefinitions& definitions;RestoredQuest& quest;std::uint32_t& money;
    ItemDefinition item;net::Packet events;MoneyReplica replica;bool money_delivered=false;
    World(const Case& input,const QuestDefinitions& defs,RestoredQuest& q,std::uint32_t& current):c(input),definitions(defs),quest(q),money(current),replica(7,current) {item.id=0x12345678;item.type=17;item.type_id=19;}
    void event(char tag,std::initializer_list<std::uint32_t> values={}) {events.push_back(static_cast<std::uint8_t>(tag));for(auto value:values) append(events,value);}
    void packet(char tag,const net::Packet& p) {event(tag,{static_cast<std::uint32_t>(p.size())});events.insert(events.end(),p.begin(),p.end());}
    void add_experience(std::uint32_t source,std::int32_t amount,bool bypass) override {
        event('E',{source,static_cast<std::uint32_t>(amount),bypass?1U:0U});
        if(c.mutate_exp) {quest.packet.id=321;quest.definition=definitions.find(2);money^=0x11111111;}
    }
    void send_database(const net::Packet& p) override {packet('B',p);check(replica.apply(p),"Quest money update rejected by actual DB-agent replica");money_delivered=true;if(c.mutate_db) quest.definition=definitions.find(1);}
    void fill_game_log_main(std::span<std::uint8_t> header) override {
        check(header.size()==74,"Quest log header size changed");for(unsigned i=2;i<74;++i) header[i]=static_cast<std::uint8_t>(i*7+c.scenario);
        quest.definition=definitions.find(c.header_alt?2:1);event('M');
    }
    std::optional<QuestRewardItemReceipt> receive_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) override {
        event('I',{type,id,count});if(!c.item_success) return std::nullopt;
        return QuestRewardItemReceipt{static_cast<std::uint8_t>(c.bag),static_cast<std::uint8_t>(c.slot),&item,"Reward item"};
    }
    void send_self(const net::Packet& p) override {packet('C',p);}
    void remove_quest(RestoredQuest& q,bool success) override {check(&q==&quest,"Quest reward removed a different owner");event('R',{q.packet.id,success?1U:0U});}
    void send_game_log(const net::Packet& p) override {packet('G',p);}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original quest reward oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot read quest reward oracle");unsigned cases=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);char tag;Case c;std::string block,events;std::uint32_t expected_money;unsigned expected_id,expected_binding;
        row>>tag>>c.scenario>>c.variant>>c.money>>c.mutate_exp>>c.mutate_db>>c.header_alt>>c.item_success>>c.bag>>c.slot>>c.context>>block>>events>>expected_money>>expected_id>>expected_binding;
        check(static_cast<bool>(row) && tag=='S',"Malformed quest reward fixture");std::string extra;check(!(row>>extra),"Trailing quest reward fixture data");
        const auto definitions=QuestDefinitions::decode(decode(block));RestoredQuest quest;quest.packet.id=123;quest.definition=definitions.find(1);auto money=c.money;World world(c,definitions,quest,money);
        award_quest_reward(quest,static_cast<std::uint8_t>(c.variant),c.context,7,money,world);
        const auto expected=decode(events);
        if(world.events!=expected) {
            const auto at=std::mismatch(world.events.begin(),world.events.end(),expected.begin(),expected.end()).first-world.events.begin();
            throw std::runtime_error("Original quest reward packet/order differs in scenario "+std::to_string(c.scenario)+" at byte "+std::to_string(at));
        }
        check(money==expected_money && quest.packet.id==expected_id && quest.definition==definitions.find(expected_binding?2:1),"Quest reward live state differs in scenario "+std::to_string(c.scenario));
        if(world.money_delivered) check(world.replica.money()==money,"Quest reward money and real DB-agent replica diverged");
        ++cases;
    }
    check(cases==512,"Quest reward original coverage changed");std::cout<<cases<<" complete original quest reward results and concrete money replica deliveries passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
