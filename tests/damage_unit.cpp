#include "eternity/game/resources.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Sink final:ResourceSink,DamageObserver {
    std::optional<bool> party;
    std::vector<std::string> events;
    DamageLedger ledger;
    bool in_zone() const noexcept override {return false;}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void packet(char route,const net::Packet& data) {
        static constexpr char hex[]="0123456789abcdef";
        std::string event(1,route);for(auto byte:data) {event+=hex[byte>>4];event+=hex[byte&15];}
        events.push_back(event);
    }
    void send_view(const net::Packet& data) override {packet('V',data);}
    void send_party(const net::Packet& data) override {packet('P',data);}
    void send_self(const net::Packet& data) override {packet('S',data);}
    void reset_damage() override {ledger.reset();events.emplace_back("X");}
    void defense_wear() override {events.emplace_back("W");}
    void record_damage(std::int32_t kind,std::uint32_t source,std::int32_t amount) override {
        ledger.add(kind,source,amount);
        events.push_back("R"+std::to_string(static_cast<std::uint32_t>(kind))+","+std::to_string(source)+","+std::to_string(static_cast<std::uint32_t>(amount)));
    }
    void check_hp_ability() override {events.emplace_back("H");}
};
DamageRecord read_record(std::istream& row) {
    unsigned int kind=0;DamageRecord result;row>>kind>>result.source>>result.amount;
    require(static_cast<bool>(row) && kind<=255,"Malformed damage record");result.kind=static_cast<std::uint8_t>(kind);return result;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected damage oracle file");std::ifstream input(argv[1]);require(input.good(),"Cannot open damage oracle");
        std::string line;unsigned int damage_cases=0,list_cases=0,empty_cases=0;DamageLedger ledger;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);char op=0;row>>op;
            const auto context=" at damage vector "+std::to_string(damage_cases)+" list vector "+std::to_string(list_cases);
            if(op=='D') {
                PlayerResources current{},expected{};ScalarDamageState state;
                std::int32_t kind=0,amount=0;std::uint32_t source=0;int party=-1,result=0;std::size_t event_count=0;
                row>>current.hp>>current.mp>>current.sp>>state.maximum_hp>>state.attack_mode>>state.protection_value>>kind>>source>>amount>>party;
                row>>result>>expected.hp>>expected.mp>>expected.sp>>event_count;
                require(static_cast<bool>(row),"Malformed scalar damage vector");
                Sink sink;if(party>=0) sink.party=party!=0;ActorResources actor(9001,current,sink);
                require(actor.damage(state,kind,source,amount,sink)==(result!=0),"Lethal result differs"+context);
                require(actor.current().hp==expected.hp && actor.current().mp==expected.mp && actor.current().sp==expected.sp,"Resources differ"+context);
                require(sink.events.size()==event_count,"Event count differs"+context);
                for(const auto& event:sink.events) {
                    std::string original;row>>original;require(event==original,"Event differs"+context+": "+event+" versus "+original);
                }
                ++damage_cases;
            } else if(op=='L') {
                int reset=0;std::int32_t kind=0,amount=0;std::uint32_t source=0;std::size_t count=0;
                row>>reset>>kind>>source>>amount>>count;require(static_cast<bool>(row),"Malformed damage list vector");
                if(reset) ledger.reset();ledger.add(kind,source,amount);
                require(ledger.entries().size()==count,"List size differs"+context);
                for(const auto& entry:ledger.entries()) require(entry==read_record(row),"List entry differs"+context);
                int present=0;row>>present;const auto maximum=ledger.maximum();require(maximum.has_value()==(present!=0),"Maximum presence differs"+context);
                if(present) require(*maximum==read_record(row),"Maximum record differs"+context);
                ++list_cases;
            } else if(op=='E') {
                int present=1;row>>present;ledger.reset();require(present==0 && !ledger.maximum(),"Reset left a maximum record");++empty_cases;
            } else throw std::runtime_error("Unknown damage vector operation");
            require(static_cast<bool>(row),"Truncated damage vector");std::string extra;require(!(row>>extra),"Trailing damage vector data");
        }
        require(damage_cases==1000 && list_cases==2048 && empty_cases==1,"Damage coverage changed");
        std::cout<<damage_cases<<" scalar damage cases and "<<list_cases<<" persistent ledger operations match original instructions\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
