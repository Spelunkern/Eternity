#include "eternity/game/effect_cycle.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
std::uint32_t get(const net::Packet& packet,std::size_t offset,std::size_t length) {
    std::uint32_t value=0;for(std::size_t i=0;i<length;++i) value|=static_cast<std::uint32_t>(packet[offset+i])<<(8*i);return value;
}
struct Harness final:EffectRemovalWorld,EffectObserver {
    EffectCycle& cycle;std::vector<SkillDefinition>& definitions;
    std::vector<std::array<std::int64_t,4>> trace;
    unsigned int scenario=0;bool changed=false,valid=true,allocation_failure=false;
    std::optional<bool> party;std::uint32_t next_id=2000;
    Harness(EffectCycle& value,std::vector<SkillDefinition>& skills):cycle(value),definitions(skills) {}
    std::optional<std::uint32_t> allocate_id() noexcept override {return allocation_failure?std::nullopt:std::optional{next_id++};}
    void release_id(std::uint32_t id) noexcept override {trace.push_back({4,id,0,0});}
    std::uint32_t tick_ms() const noexcept override {return 13001;}
    void erase_persisted(const PersistedEffect& effect) noexcept override {trace.push_back({1,effect.skill_id,effect.level,0});}
    void send_self(const net::Packet& packet) noexcept override {
        if(packet.size()!=6 || get(packet,0,2)!=0x50e) {valid=false;return;}trace.push_back({2,get(packet,2,4),0,0});
    }
    std::optional<bool> party_kind() const noexcept override {return party;}
    void send_party(const net::Packet& packet) noexcept override {
        if(packet.size()!=9 || get(packet,2,4)!=9001 || (get(packet,0,2)!=0xc05 && get(packet,0,2)!=0xc10)) {valid=false;return;}
        trace.push_back({5,get(packet,6,2),packet[8],get(packet,0,2)==0xc10});
    }
    void maximum_changed(EffectStatus&,std::uint8_t resource) override {
        trace.push_back({3,resource,0,0});
        if(!changed) {
            changed=true;
            if(scenario==5) party.reset();else if(scenario==6) party=true;
            else if(scenario==7) cycle.append(1003,{{203,1,0},10000,12000,false},definitions[3]);
        }
    }
    void finish_periodic_heal(EffectStatus&,const SkillDefinition&,const EffectSource&,const std::array<std::int32_t,3>& amounts) override {trace.push_back({6,amounts[0],amounts[1],amounts[2]});}
    void unexpected() {throw std::runtime_error("Unexpected world operation in expiry fixture");}
    void speed_changed(EffectStatus&) override {unexpected();}
    void shape_changed(EffectStatus&) override {unexpected();}
    void run_mode_changed(EffectStatus&,bool) override {unexpected();}
    void stop_attack(EffectStatus&) override {unexpected();}
    void stop_target_mob(EffectStatus&,std::uint32_t) override {unexpected();}
    void taunt(EffectStatus&,std::uint16_t,float) override {unexpected();}
    void detect_hidden(EffectStatus&,const SkillDefinition&) override {unexpected();}
    void rebuild_attack(EffectStatus&,bool) override {unexpected();}
    void user_shape_changed(EffectStatus&) override {unexpected();}
    void replace_attack_elements(EffectStatus&,std::uint8_t) override {unexpected();}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
int main(int argc,char** argv) {
    try {
        require(argc==3,"Expected expiry cycle and load filter oracle files");
        require(effect_removed_packet(0x78563412)==net::Packet{0x0e,5,0x12,0x34,0x56,0x78},"Effect removal DWORD packet changed");
        require(party_effect_removed_packet(0x78563412,0x2345,3,true)==net::Packet{0x10,0x0c,0x12,0x34,0x56,0x78,0x45,0x23,3},"Raid effect removal packet changed");
        std::ifstream input(argv[1]);require(input.good(),"Cannot open expiry cycle vectors");std::string line;unsigned int checked=0;
        constexpr std::array<unsigned int,11> counts={0,1,1,8,70,6,6,3,3,1,1};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream vector(line);unsigned int scenario=0,remaining=0,applied=0,event_count=0;std::uint32_t hp=0;
            require(static_cast<bool>(vector>>scenario>>remaining>>hp>>applied>>event_count) && scenario<counts.size(),"Malformed expiry cycle vector");
            EffectCycle cycle(9001);std::vector<SkillDefinition> definitions(counts[scenario]+1);
            Harness world(cycle,definitions);world.scenario=scenario;if(scenario>=3) world.party=false;
            for(std::size_t i=0;i<definitions.size();++i) {
                auto& skill=definitions[i];skill.id=static_cast<std::uint16_t>(200+i);skill.level=1;skill.effect=scenario==9?0:4;
                skill.detail=scenario==9?6:scenario==10?81:82;skill.abilities[0]={1,1};skill.time_heal={3,7,11};
                skill.offline_countdown=scenario==2 || (scenario==3 && i%2==1);
            }
            for(unsigned int i=0;i<counts[scenario];++i) {
                const auto& skill=definitions[i];auto end=skill.offline_countdown?12U:12000U;
                if(scenario==3 && i%3==0) ++end;if(scenario==8) end=std::array<std::uint32_t,3>{0,1,UINT32_MAX}[i];
                cycle.append(1000+i,{{skill.id,1,0},10000,end,skill.offline_countdown},skill);
            }
            EffectStatus status;status.skills.equipment.base.maximum.hp=10000;status.skills.hp_ability_applied=true;
            cycle.expire(scenario==8?0U:12000U,status,{},ItemDefinitions{},world,world);
            require(world.valid && cycle.size()==remaining && status.skills.equipment.base.maximum.hp==hp && status.skills.hp_ability_applied==(applied!=0),"Original expiry cycle state differs");
            if(world.trace.size()!=event_count) throw std::runtime_error("Expiry event count mismatch in scenario "+std::to_string(scenario));
            for(const auto& event:world.trace) for(auto value:event) {
                std::int64_t expected=0;require(static_cast<bool>(vector>>expected),"Missing original expiry event");
                if(value!=expected) throw std::runtime_error("Expiry event mismatch in scenario "+std::to_string(scenario)+": "+std::to_string(value)+" != "+std::to_string(expected));
            }
            std::string extra;require(!(vector>>extra),"Trailing expiry cycle data");++checked;
        }
        require(checked==11,"Expiry cycle scenario count changed");
        EffectCycle cycle(9001);std::vector<SkillDefinition> skills(1);auto& skill=skills[0];skill.id=10;skill.level=1;skill.effect=4;skill.detail=82;skill.abilities[0]={1,1};skill.offline_countdown=false;
        cycle.append(1,{{10,1,0},0,12,false},skill);skill.offline_countdown=true;
        Harness world(cycle,skills);EffectStatus status;status.skills.equipment.base.maximum.hp=10;
        cycle.expire(12,status,{},ItemDefinitions{},world,world);require(cycle.size()==1,"Expiry ignored live definition time units");
        cycle.expire(12000,status,{},ItemDefinitions{},world,world);require(cycle.size()==0 && status.skills.equipment.base.maximum.hp==9,"Reloaded effect did not expire once");
        cycle.expire(12000,status,{},ItemDefinitions{},world,world);require(world.trace.size()==4,"Expired effect was notified twice");
        EffectReplica replica({{10,1,31},{10,1,40},{10,2,50}});
        require(replica.erase(10,1) && replica.snapshot().size()==2 && replica.snapshot()[0].remaining==40 && !replica.erase(99,1),
            "DB-agent effect deletion did not retain ordered first-match semantics");
        EffectReplica applied({{10,1,31},{10,1,40},{10,2,50}});applied.apply({10,1,UINT32_MAX});applied.apply({11,1,60});
        require(applied.snapshot().size()==4 && applied.snapshot()[0].remaining==UINT32_MAX && applied.snapshot()[1].remaining==40 && applied.snapshot()[2].level==2 && applied.snapshot()[3].skill_id==11,"DB-agent effect application did not retain first-match update and tail insertion semantics");
        storage::ResultSet table;table.columns={"SkillID","SkillLevel","TypeDetail","TypeEffect","Weapon1","Weapon2","Weaponvalue","Bag",
            "AbilityType1","AbilityValue1","AbilityType2","AbilityValue2","AbilityType3","AbilityValue3",
            "AttrType","DefenceType","DefenceValue","ApplyRange","Arrow","ChangeType","ChangeLevel","FixRange","KeepTime","TimeHealHP","TimeHealSP","TimeHealMP","LimitHP"};
        for(auto id:{10,11,124}) {
            storage::Row row(table.columns.size(),storage::Cell{"0"});row[0]=std::to_string(id);row[1]="1";row[2]="82";row[3]="4";
            row[8]="1";row[9]="1";row[21]=id==11?"1":"0";row[22]="100";table.rows.push_back(std::move(row));
        }
        const auto registry=SkillDefinitions::load(table);EffectCycle loaded(9001);std::vector<SkillDefinition> unused;Harness restored(loaded,unused);
        loaded.restore({{11,1,1},{10,1,1},{11,1,10},{124,1,1},{999,1,1}},registry,18,10,10000,restored);
        require(loaded.size()==2 && loaded.contains(2000) && loaded.contains(2001) && restored.trace==std::vector<std::array<std::int64_t,4>>{{1,11,1,0}},
            "Restoration did not keep ordered IDs, spent-timer replica deletion or map/definition filtering");
        EffectStatus loaded_status;loaded_status.skills.equipment.base.maximum.hp=10;loaded.initialize(loaded_status,restored);
        require(loaded_status.skills.equipment.base.maximum.hp==12,"Restored effects did not apply state before ticking");
        loaded.expire(20000,loaded_status,{},ItemDefinitions{},restored,restored);
        require(loaded.size()==1 && loaded.contains(2000) && loaded_status.skills.equipment.base.maximum.hp==11,"Restored real-time expiry did not reverse the correct effect");
        loaded.expire(21000,loaded_status,{},ItemDefinitions{},restored,restored);
        require(loaded.size()==0 && loaded_status.skills.equipment.base.maximum.hp==10,"Restored ordinary expiry did not reverse its grace-period deadline");
        EffectCycle failed(9001);Harness allocation(failed,unused);allocation.allocation_failure=true;
        failed.restore({{10,1,1}},registry,1,0,10000,allocation);
        require(failed.size()==0 && allocation.trace.empty(),"Failed skill allocation changed persistence or attached an effect");
        std::ifstream filter_input(argv[2]);require(filter_input.good(),"Cannot open persisted effect filter oracle");unsigned int filtered=0;
        while(std::getline(filter_input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned int available=0,effect=0,detail=0,shape=0,disposition=0;
            require(static_cast<bool>(row>>available>>effect>>detail>>shape>>disposition),"Malformed effect load filter vector");
            auto data=table;data.rows.clear();
            if(available) {storage::Row values(data.columns.size(),storage::Cell{"0"});values[0]="321";values[1]="1";values[2]=std::to_string(detail);values[3]=std::to_string(effect);values[19]=std::to_string(shape);values[22]="100";data.rows.push_back(std::move(values));}
            const auto skills=SkillDefinitions::load(data);EffectCycle actor(9001);Harness observer(actor,unused);
            actor.restore({{321,1,30}},skills,1,0,10000,observer);
            require(actor.size()==(disposition==1 && (effect==3 || effect==4)?1U:0U),"Original load filter attached a different effect");
            require(observer.trace==(disposition==2?std::vector<std::array<std::int64_t,4>>{{1,321,1,0}}:std::vector<std::array<std::int64_t,4>>{}),"Original load filter replica deletion differs");
            ++filtered;
        }
        require(filtered==84,"Effect load filter coverage changed");
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
