#include "eternity/game/mob_combat.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace eternity::game;
struct Pool final:MobDamagePoolServices {
    std::string* events=nullptr;
    std::unique_ptr<MobDamageRecord> construct() override {if(events)*events+='A';return std::make_unique<MobDamageRecord>();}
};
struct Ports final:MobCombatServices {
    int current=0;std::string events;std::uint32_t logged_damage=0;
    std::int32_t status(const MobCombatState&) override{return current;}
    void dispatch_death_event(MobCombatState&) override{events+='E';}
    std::uint32_t tick() override{events+='T';return 0xfffffff0;}
    void log_damage(MobCombatState&,std::uint32_t value) override{events+='L';logged_damage=value;}
    void retarget(MobCombatState&,bool stacked) override{events+=stacked?'X':'R';}
};
int main(int argc,char**argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");
        std::ifstream in(argv[1]);if(!in)throw std::runtime_error("cannot open fixture");
        std::string line;unsigned count=0;
        while(std::getline(in,line)) {
            if(line.empty()||line[0]=='#')continue;
            std::istringstream row(line);unsigned mode,status,flags;std::uint32_t hp,mp,sp,d0,d1,d2,bonus,base;
            unsigned result;std::uint32_t ohp,omp,osp,hits,state,secondary,tick;std::string events;
            row>>mode>>status>>hp>>mp>>sp>>flags>>d0>>d1>>d2>>bonus>>base>>result>>ohp>>omp>>osp>>hits>>state>>secondary>>tick>>events;
            if(!row)throw std::runtime_error("invalid fixture");
            Pool allocator;MobDamagePool pool(allocator);MobDamageLedger ledger(pool);Ports ports;ports.current=static_cast<int>(status);allocator.events=&ports.events;
            MobCombatState m;m.hp=hp;m.mp=static_cast<std::uint16_t>(mp);m.sp=static_cast<std::uint16_t>(sp);m.hit_count=0xffffffff;m.state=7;m.secondary_state=8;
            m.count_hits=flags&1;m.stacked_fsm=flags&2;m.long_corpse_delay=flags&4;m.log_enabled=flags&8;m.linked_log_enabled=(flags&16)&&(flags&32);
            MobCombatCredit c{1,42,77,std::bit_cast<std::int32_t>(bonus),std::bit_cast<std::int32_t>(base)};
            bool actual;
            if(mode==0)actual=mob_combat_damage(m,ledger,ports,d0,c);
            else if(mode==1)actual=mob_combat_damage(m,ledger,ports,std::array{std::bit_cast<std::int32_t>(d0),std::bit_cast<std::int32_t>(d1),std::bit_cast<std::int32_t>(d2)},c);
            else actual=mob_combat_party(m,ledger,ports,42,std::bit_cast<std::int32_t>(d0));
            if(events=="-")events.clear();
            bool ok=actual==static_cast<bool>(result)&&m.hp==ohp&&m.mp==omp&&m.sp==osp&&m.hit_count==hits&&static_cast<std::uint32_t>(m.state)==state&&m.secondary_state==secondary&&m.death_tick==tick&&events==ports.events;
            if(events.find('L')!=std::string::npos)ok=ok&&ports.logged_damage==d0;
            std::uint32_t kind,id,damage,aggro,party;
            if(row>>kind>>id>>damage>>aggro>>party) {
                ok=ok&&ledger.records().size()==1;
                if(!ledger.records().empty()){auto& r=*ledger.records()[0];ok=ok&&r.type==kind&&r.id==id&&static_cast<std::uint32_t>(r.damage)==damage&&static_cast<std::uint32_t>(r.aggro)==aggro&&r.party==party;}
            } else ok=ok&&ledger.records().empty();
            if(!ok)throw std::runtime_error("original mismatch case "+std::to_string(count)+" events="+ports.events+" expected="+events);
            ++count;
        }
        if(count!=768)throw std::runtime_error("incomplete fixture");
        // Compose recovered damage with the existing pooled ledger and party
        // winner selection, retaining individual credit beneath aggregate credit.
        Pool allocator;MobDamagePool pool(allocator);MobDamageLedger ledger(pool);Ports ports;
        MobCombatState mob;mob.hp=1000;
        MobCombatCredit first{1,42,77,0,0},second{1,43,77,0,0};
        mob_combat_damage(mob,ledger,ports,20,first);
        mob_combat_party(mob,ledger,ports,77,20);
        mob_combat_damage(mob,ledger,ports,40,second);
        mob_combat_party(mob,ledger,ports,77,40);
        MobDamageRecord winner;
        if(!ledger.maximum_damage(winner)||winner.id!=43||winner.party!=77||mob.hp!=940)
            throw std::runtime_error("party credit composition failed");
        std::cout<<count<<" original mob combat comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
