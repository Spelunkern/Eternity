#include "eternity/game/mob_combat_attack.hpp"
#include <bit>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace eternity::game;
namespace {
std::string record_text(const MobDamageRecord& r) {
    std::ostringstream out;out<<static_cast<unsigned>(r.type)<<','<<r.id<<','<<static_cast<std::uint32_t>(r.damage)<<','<<static_cast<std::uint32_t>(r.aggro)<<','<<r.party;return out.str();
}
struct Pool final:MobDamagePoolServices {
    std::function<void(MobDamageRecord*)> allocated;
    std::unique_ptr<MobDamageRecord> construct() override {auto r=std::make_unique<MobDamageRecord>();allocated(r.get());return r;}
};
struct Ports final:MobCombatAttackServices {
    MobCombatState* mob=nullptr;MobDamageLedger* ledger=nullptr;
    bool mutate=false;std::string events,pending_snapshot;MobDamageRecord* pending=nullptr;
    std::string snapshot(char tag) const {
        std::ostringstream out;out<<tag<<':'<<mob->hp<<','<<mob->mp<<','<<mob->sp<<','<<ledger->records().size();return out.str();
    }
    void append(const std::string& value){if(!events.empty())events+='/';events+=value;}
    void flush(){if(pending){append(pending_snapshot+','+record_text(*pending));pending=nullptr;}}
    void allocation(MobDamageRecord* r){flush();pending=r;pending_snapshot=snapshot('A');}
    std::int32_t status(const MobCombatState& m) override{flush();return m.state;}
    void dispatch_death_event(MobCombatState&) override{flush();append(snapshot('E'));}
    std::uint32_t tick() override{return 123;}
    void log_damage(MobCombatState&,std::uint32_t) override{throw std::runtime_error("unexpected logging");}
    void retarget(MobCombatState&,bool stacked) override{flush();append(snapshot(stacked?'X':'R'));}
    void stop_attack(MobCombatAttacker& user) override{flush();append(snapshot('S')+','+std::to_string(user.id));if(mutate)user.id=99;}
    void death(MobCombatState&,std::int32_t type,std::uint32_t id) override{flush();append(snapshot('D')+','+std::to_string(type)+','+std::to_string(id));}
};
}
int main(int argc,char** argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");
        std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture unavailable");
        unsigned count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            std::istringstream row(line);unsigned path,status,has_party,mutate,result;
            std::uint32_t hp,mp,sp,party_id,aggro,bonus,ohp,omp,osp,state,tick,uid;
            std::array<std::uint32_t,3> primary{},secondary{};std::string events,records;
            row>>path>>status>>hp>>mp>>sp>>has_party>>party_id>>aggro>>bonus;
            for(auto& v:primary)row>>v;for(auto& v:secondary)row>>v;
            row>>mutate>>result>>ohp>>omp>>osp>>state>>tick>>uid>>events>>records;
            if(!row)throw std::runtime_error("invalid fixture");
            Pool allocator;MobDamagePool pool(allocator);MobDamageLedger ledger(pool);
            MobCombatState mob;mob.hp=hp;mob.mp=static_cast<std::uint16_t>(mp);mob.sp=static_cast<std::uint16_t>(sp);mob.state=static_cast<std::int32_t>(status);
            MobCombatAttacker user;user.id=42;user.aggro_bonus=std::bit_cast<std::int32_t>(aggro);if(has_party)user.party=party_id;
            Ports ports;ports.mob=&mob;ports.ledger=&ledger;ports.mutate=mutate!=0;
            allocator.allocated=[&](MobDamageRecord* r){ports.allocation(r);};
            const auto& chosen=(path==3||path==5)?secondary:primary;
            const std::array damage={std::bit_cast<std::int32_t>(chosen[0]),std::bit_cast<std::int32_t>(chosen[1]),std::bit_cast<std::int32_t>(chosen[2])};
            const bool actual=mob_combat_user_hit(mob,ledger,ports,user,damage,std::bit_cast<std::int32_t>(primary[0]),path==0?0:static_cast<std::int32_t>(bonus));ports.flush();
            std::string actual_records;
            for(auto* r:ledger.records()){if(!actual_records.empty())actual_records+='/';actual_records+=record_text(*r);}
            if(events=="-")events.clear();if(records=="-")records.clear();
            if(actual!=static_cast<bool>(result)||mob.hp!=ohp||mob.mp!=omp||mob.sp!=osp||static_cast<std::uint32_t>(mob.state)!=state||mob.death_tick!=tick||user.id!=uid||events!=ports.events||records!=actual_records)
                throw std::runtime_error("caller mismatch case "+std::to_string(count)+" path "+std::to_string(path)+"\nactual "+ports.events+"\nexpected "+events+"\nrecords "+actual_records+" expected "+records);
            ++count;
        }
        if(count!=768)throw std::runtime_error("incomplete caller fixture");
        std::cout<<count<<" original caller-slice comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
