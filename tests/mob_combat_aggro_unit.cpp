#include "eternity/game/mob_combat_aggro.hpp"
#include "eternity/game/mobs.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
using namespace eternity::game;
namespace {
struct Pool final:MobDamagePoolServices {
    bool fail=false;std::function<void()> observe;std::vector<MobDamageRecord*> objects;
    std::unique_ptr<MobDamageRecord> construct() override {
        observe();if(fail)return {};
        auto record=std::make_unique<MobDamageRecord>();objects.push_back(record.get());return record;
    }
    std::size_t identity(MobDamageRecord* record) const {
        const auto found=std::find(objects.begin(),objects.end(),record);
        if(found==objects.end())throw std::runtime_error("unknown heap record");
        return static_cast<std::size_t>(found-objects.begin());
    }
};
struct Ports final:MobCombatAggroServices,MobCombatAggroWorld {
    MobRuntime* live=nullptr;
    MobCombatState* mob=nullptr;MobDamageLedger* first=nullptr;MobDamageLedger* second=nullptr;
    MobDamagePool* pool=nullptr;Pool* allocator=nullptr;
    bool active=false,empty=false;std::int32_t scene=0;unsigned mutation=0;
    std::string events;
    std::string records(const MobDamageLedger& ledger) const {
        std::ostringstream out;bool initial=true;
        for(auto* r:ledger.records()) {
            if(!initial)out<<'/';initial=false;
            out<<allocator->identity(r)<<','<<static_cast<unsigned>(r->type)<<','<<r->id<<','<<static_cast<std::uint32_t>(r->damage)<<','<<static_cast<std::uint32_t>(r->aggro)<<','<<r->party;
        }
        return initial?"-":out.str();
    }
    std::string snapshot(char tag) const {
        if(live){mob->hp=live->hp;mob->state=live->status;mob->stacked_fsm=live->state_stack.has_value();}
        std::ostringstream out;
        out<<tag<<':'<<mob->hp<<','<<static_cast<std::uint32_t>(mob->state)<<','<<mob->stacked_fsm<<','<<static_cast<std::uint32_t>(scene)<<','<<empty<<','<<pool->construction_attempts()<<','<<pool->available().size()<<','<<first->fair_combat<<','<<second->fair_combat<<';'<<records(*first)<<';'<<records(*second);
        return out.str();
    }
    void observe(char tag){if(!events.empty())events+='|';events+=snapshot(tag);}
    std::int32_t status(const MobCombatState& m) override {observe('S');return m.stacked_fsm?(empty?0:scene):m.state;}
    void retarget(MobCombatState&,bool stacked) override {observe(stacked?'X':'R');if(mutation==4)first->reset();}
    void retarget(MobRuntime&,bool stacked) override {retarget(*mob,stacked);}
    void allocation() {
        if(!active)return;
        observe('N');
        if(mutation==1){mob->state=2;scene=2;}
        else if(mutation==2){mob->state=1;scene=1;mob->stacked_fsm=!mob->stacked_fsm;empty=false;}
        else if(mutation==3){mob->hp=0;mob->state=1;scene=1;empty=false;}
        if(live){live->hp=mob->hp;live->status=mob->state;
            if(mob->stacked_fsm)live->state_stack=empty?std::vector<std::int32_t>{}:std::vector<std::int32_t>{scene};
            else live->state_stack.reset();}
    }
};
std::string maximum(const MobDamageLedger& ledger) {
    MobDamageRecord result{0xa5,0xa5a5a5a5U,std::bit_cast<std::int32_t>(0xa5a5a5a5U),std::bit_cast<std::int32_t>(0xa5a5a5a5U),0xa5a5a5a5U};
    std::array<unsigned char,29> bytes;bytes.fill(0xa5);bytes[0]=ledger.maximum_aggro(result)?1:0;
    bytes[9]=result.type;
    const auto put=[&](unsigned offset,std::uint32_t value){for(unsigned i=0;i<4;++i)bytes[offset+i]=static_cast<unsigned char>(value>>(8*i));};
    put(13,result.id);put(17,static_cast<std::uint32_t>(result.damage));put(21,static_cast<std::uint32_t>(result.aggro));put(25,result.party);
    const char* digits="0123456789abcdef";std::string text;
    for(auto byte:bytes){text+=digits[byte>>4];text+=digits[byte&15];}return text;
}
}
int main(int argc,char**argv) {
    try {
        if(argc!=2)throw std::runtime_error("fixture required");
        std::ifstream input(argv[1]);if(!input)throw std::runtime_error("fixture unavailable");
        unsigned count=0;std::string line;
        for(unsigned representation=0;representation<2;++representation) {
        input.clear();input.seekg(0);
        while(std::getline(input,line)) {
            if(line.empty()||line[0]=='#')continue;
            unsigned seed,stack_mode,fail,mutation;std::uint32_t amount,id,hp,state,scene,damage,aggro;
            std::string final,events,selected;std::istringstream row(line);
            row>>seed>>amount>>id>>hp>>state>>stack_mode>>scene>>fail>>mutation>>damage>>aggro>>final>>events>>selected;
            if(!row)throw std::runtime_error("invalid fixture");
            Pool allocator;MobDamagePool pool(allocator);MobRuntime live;
            live.damage=std::make_unique<MobDamageLedger>(pool);
            MobDamageLedger& first=*live.damage;MobDamageLedger second(pool);MobCombatState mob;
            first.fair_combat=false;second.fair_combat=false;
            Ports ports;ports.mob=&mob;ports.first=&first;ports.second=&second;ports.pool=&pool;ports.allocator=&allocator;
            allocator.observe=[&]{ports.allocation();};
            second.add(1,id,7,9,55);
            if(seed==1||seed==4)first.add(1,id,std::bit_cast<std::int32_t>(damage),std::bit_cast<std::int32_t>(aggro),77);
            else if(seed==2){first.add(2,id,std::bit_cast<std::int32_t>(damage),std::bit_cast<std::int32_t>(aggro),77);first.add(5,id,std::bit_cast<std::int32_t>(damage),std::bit_cast<std::int32_t>(aggro),77);}
            else if(seed==3)first.add(1,id^1,std::bit_cast<std::int32_t>(damage),std::bit_cast<std::int32_t>(aggro),77);
            if(seed==4)first.reset();
            mob.hp=hp;mob.state=std::bit_cast<std::int32_t>(state);mob.stacked_fsm=stack_mode!=0;
            mob.mp=17;mob.sp=19;mob.hit_count=123;mob.death_tick=999;mob.secondary_state=87;
            ports.empty=stack_mode==2;ports.scene=std::bit_cast<std::int32_t>(scene);ports.mutation=mutation;
            live.hp=hp;live.status=std::bit_cast<std::int32_t>(state);
            live.mp=17;live.sp=19;live.attacked_count=123;live.death_time=999;
            if(stack_mode)live.state_stack=ports.empty?std::vector<std::int32_t>{}:std::vector<std::int32_t>{ports.scene};
            allocator.fail=fail!=0;ports.active=true;
            if(representation) {
                ports.live=&live;
                mob_combat_aggro(live,ports,id,std::bit_cast<std::int32_t>(amount));
                // GetStatus is native bridge code, not an external world port.
                // Compare all other original observations and final state.
                std::istringstream observations(events);std::string event,filtered;
                while(std::getline(observations,event,'|'))if(!event.starts_with("S:")){
                    if(!filtered.empty())filtered+='|';filtered+=event;
                }
                events=filtered.empty()?"-":filtered;
            } else mob_combat_aggro(mob,first,ports,id,std::bit_cast<std::int32_t>(amount));
            if(events=="-")events.clear();
            if(ports.snapshot('F')!=final||ports.events!=events||maximum(first)!=selected||mob.mp!=17||mob.sp!=19||mob.hit_count!=123||mob.death_tick!=999||mob.secondary_state!=87||live.mp!=17||live.sp!=19||live.attacked_count!=123||live.death_time!=999)
                throw std::runtime_error("Aggro mismatch case "+std::to_string(count)+"\nactual "+ports.snapshot('F')+"\nexpected "+final+"\nactual events "+ports.events+"\nexpected events "+events+"\nselection "+maximum(first)+" expected "+selected);
            ++count;
        }
        }
        // An absent ledger is a broken ownership invariant, never an empty
        // substitute ledger or a silently disabled world operation.
        MobRuntime unowned;Ports unused;bool rejected=false;
        try{mob_combat_aggro(unowned,unused,1,1);}catch(const std::invalid_argument&){rejected=true;}
        if(!rejected)throw std::runtime_error("missing runtime damage owner accepted");
        if(count!=4096)throw std::runtime_error("incomplete fixture");
        std::cout<<count<<" complete original Aggro comparisons passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
