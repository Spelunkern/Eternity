#include "eternity/game/mob_combat.hpp"
#include <bit>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace eternity::game;
namespace {
struct Pool final:MobDamagePoolServices {
    std::function<void()> observe;
    std::unique_ptr<MobDamageRecord> construct() override {observe();return std::make_unique<MobDamageRecord>();}
};
struct Ports final:MobCombatServices {
    unsigned mask=0;int current=0;
    MobCombatState* mob=nullptr;
    MobDamageLedger* ledger=nullptr;
    std::array<std::int32_t,3> values{};
    std::string events;
    std::string snapshot(char tag,std::uint32_t argument=0) const {
        const auto& m=*mob;std::ostringstream out;out<<tag<<':';
        const std::uint32_t fields[]={m.hp,m.mp,m.sp,static_cast<std::uint32_t>(m.state),m.secondary_state,m.death_tick,
            m.long_corpse_delay,m.stacked_fsm,m.log_enabled,static_cast<std::uint32_t>(values[0]),
            static_cast<std::uint32_t>(values[1]),static_cast<std::uint32_t>(values[2]),
            static_cast<std::uint32_t>(ledger->records().size()),argument};
        bool first=true;for(auto value:fields){if(!first)out<<',';out<<value;first=false;}
        return out.str();
    }
    void observe(char tag,std::uint32_t value=0){if(!events.empty())events+='/';events+=snapshot(tag,value);}
    std::int32_t status(const MobCombatState&) override{return current;}
    void dispatch_death_event(MobCombatState& m) override {
        observe('E');
        if(mask&1){m.hp=17;m.mp=60;m.sp=70;m.state=9;m.secondary_state=99;
            m.long_corpse_delay=!m.long_corpse_delay;m.log_enabled=true;values={31,-4,9};current=1;}
    }
    std::uint32_t tick() override {
        observe('T');if(mask&4){mob->long_corpse_delay=!mob->long_corpse_delay;mob->secondary_state=88;}
        return 0xfffffff0;
    }
    void log_damage(MobCombatState& m,std::uint32_t value) override {
        observe('L',value);
        if(mask&2){m.hp=23;m.mp=5;m.sp=7;m.state=6;m.secondary_state=66;
            values={11,3,-2};m.stacked_fsm=!m.stacked_fsm;current=1;}
    }
    void retarget(MobCombatState&,bool stacked) override{observe(stacked?'X':'R');}
};
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");
        std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture unavailable");
        std::string line;unsigned count=0;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            unsigned mode,mask,status,flags,result;std::uint32_t hp,mp,sp,d0,d1,d2;
            std::string final,events,credit;std::istringstream row(line);
            row>>mode>>mask>>status>>hp>>mp>>sp>>flags>>d0>>d1>>d2>>result>>final>>events>>credit;
            if(!row)throw std::runtime_error("invalid fixture");
            Pool allocator;MobDamagePool pool(allocator);MobDamageLedger ledger(pool);
            MobCombatState m;m.hp=hp;m.mp=static_cast<std::uint16_t>(mp);m.sp=static_cast<std::uint16_t>(sp);m.state=7;m.secondary_state=8;
            m.stacked_fsm=flags&1;m.long_corpse_delay=flags&2;m.log_enabled=flags&4;
            Ports ports;ports.mask=mask;ports.current=static_cast<int>(status);ports.mob=&m;ports.ledger=&ledger;
            ports.values={std::bit_cast<std::int32_t>(d0),std::bit_cast<std::int32_t>(d1),std::bit_cast<std::int32_t>(d2)};
            allocator.observe=[&]{ports.observe('A',mode?static_cast<std::uint32_t>(ports.values[0]):d0);};
            MobCombatCredit c{1,42,77,3,4};
            const bool actual=mode?mob_combat_damage(m,ledger,ports,ports.values,c):mob_combat_damage(m,ledger,ports,d0,c);
            std::string actual_credit="-";
            if(!ledger.records().empty()) {
                const auto& r=*ledger.records()[0];std::ostringstream out;
                out<<static_cast<unsigned>(r.type)<<','<<r.id<<','<<static_cast<std::uint32_t>(r.damage)<<','<<static_cast<std::uint32_t>(r.aggro)<<','<<r.party;actual_credit=out.str();
            }
            if(events=="-")events.clear();
            if(actual!=static_cast<bool>(result)||ports.snapshot('F')!=final||ports.events!=events||actual_credit!=credit)
                throw std::runtime_error("callback case "+std::to_string(count)+"\nactual "+ports.snapshot('F')+"\nexpected "+final+"\nactual events "+ports.events+"\nexpected events "+events);
            ++count;
        }
        if(count!=512)throw std::runtime_error("incomplete fixture");
        std::cout<<count<<" original callback comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
