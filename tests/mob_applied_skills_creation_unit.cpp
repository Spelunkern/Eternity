#include "eternity/game/mob_applied_skills.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <set>
#include <thread>
#include <stdexcept>

using namespace eternity::game;
namespace {
void require(bool value) {if(!value) throw std::runtime_error("Skill creation comparison failed");}
struct Services final:MobAppliedSkillPoolServices,MobAppliedSkillLogServices {
    bool fail=false;
    std::vector<MobAppliedSkill*> objects;
    std::unique_ptr<MobAppliedSkill> construct() override {
        if(fail) return nullptr;
        auto skill=std::make_unique<MobAppliedSkill>();
        skill->instance_id=skill->skill_id=skill->level=skill->number=0xa5a5a5a5U;
        skill->attack_tick=skill->reset_tick=skill->end_tick=skill->previous_tick=0xa5a5a5a5U;
        skill->target_type=skill->target_id=0xa5a5a5a5U;
        objects.push_back(skill.get());return skill;
    }
    void removed(const MobAppliedSkill&,std::uint8_t) noexcept override {std::terminate();}
    int index(const MobAppliedSkill* skill) const {
        if(!skill) return -1;
        const auto found=std::find(objects.begin(),objects.end(),skill);
        require(found!=objects.end());return static_cast<int>(found-objects.begin());
    }
};
std::string pointers(const Services& services,const std::vector<MobAppliedSkill*>& records) {
    if(records.empty()) return "-";
    std::ostringstream out;
    for(std::size_t i=0;i<records.size();++i) {if(i)out<<',';out<<services.index(records[i]);}
    return out.str();
}
std::string snapshot(const Services& services,const MobAppliedSkillPool& pool,
                     const std::array<std::unique_ptr<MobAppliedSkills>,2>& owners,
                     const std::atomic<std::uint32_t>& counter) {
    require(pool.owned_count()==services.objects.size());
    std::ostringstream out;
    out<<counter.load()<<'|'<<pool.construction_attempts()<<'|'<<pointers(services,pool.available());
    for(const auto& owner:owners) out<<'|'<<pointers(services,owner->records());
    out<<'|';
    for(std::size_t i=0;i<services.objects.size();++i) {
        if(i)out<<';';
        const auto& s=*services.objects[i];
        out<<i<<':'<<s.instance_id<<':'<<s.skill_id<<':'<<s.level<<':'<<s.number<<':'<<s.attack_tick
           <<':'<<s.reset_tick<<':'<<s.end_tick<<':'<<s.previous_tick<<':'<<s.target_type<<':'<<s.target_id;
    }
    if(services.objects.empty())out<<'-';
    return out.str();
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2);std::ifstream input(argv[1]);require(input.good());
        std::unique_ptr<Services> services;
        std::unique_ptr<MobAppliedSkillPool> pool;
        std::array<std::unique_ptr<MobAppliedSkills>,2> owners;
        std::atomic<std::uint32_t> counter{0};
        std::size_t cases=0,operations=0,line_number=0,zero_ids=0;
        for(std::string line;std::getline(input,line);) {
            ++line_number;if(line.empty() || line[0]=='#')continue;
            std::istringstream row(line);char op;std::uint32_t arg;row>>op>>arg;
            if(op=='S') {
                for(auto& owner:owners)owner.reset();pool.reset();
                services=std::make_unique<Services>();pool=std::make_unique<MobAppliedSkillPool>(*services);
                for(auto& owner:owners)owner=std::make_unique<MobAppliedSkills>(*pool,*services);
                std::uint32_t initial;row>>initial;counter.store(initial);++cases;continue;
            }
            if(op=='A') {
                unsigned fail;std::uint32_t stamp;int expected_index;
                row>>fail>>stamp>>expected_index;services->fail=fail!=0;
                auto* skill=owners.at(arg)->append_created(counter);
                require(services->index(skill)==expected_index);
                if(skill) {
                    zero_ids+=skill->instance_id==0?1U:0U;
                    if(stamp) {
                        skill->skill_id=stamp;skill->level=stamp+1;skill->number=stamp+2;
                        skill->attack_tick=stamp+3;skill->reset_tick=stamp+4;skill->end_tick=stamp+5;
                        skill->previous_tick=stamp+6;skill->target_type=stamp+7;skill->target_id=stamp+8;
                    }
                }
            } else if(op=='C') owners.at(arg)->clear({});
            else if(op=='X') counter.store(arg);
            else require(false);
            std::string expected;row>>expected;require(!row.fail());
            const auto actual=snapshot(*services,*pool,owners,counter);
            if(actual!=expected) {
                std::cerr<<"line "<<line_number<<"\nexpected "<<expected<<"\nactual   "<<actual<<'\n';return 1;
            }
            ++operations;
        }
        require(cases==64 && operations==1088 && zero_ids>0);
        // Concurrent native owners share the original manager's atomic sequence.
        // Starting near wrap includes zero without inventing a zero-skipping rule.
        {
            Services concurrent;
            MobAppliedSkillPool shared_pool(concurrent);
            MobAppliedSkills first(shared_pool,concurrent),second(shared_pool,concurrent);
            std::atomic<std::uint32_t> shared_counter{0xffffff00U};
            auto fill=[&shared_counter](MobAppliedSkills& owner) {
                for(unsigned i=0;i<256;++i)require(owner.append_created(shared_counter)!=nullptr);
            };
            std::thread left(fill,std::ref(first)),right(fill,std::ref(second));
            left.join();right.join();
            std::set<std::uint32_t> ids;
            for(auto* skill:concurrent.objects)ids.insert(skill->instance_id);
            require(ids.size()==512 && ids.contains(0) && shared_counter.load()==256);
            first.clear({});second.clear({});concurrent.fail=true;
            for(unsigned i=0;i<512;++i)require(first.append_created(shared_counter)!=nullptr);
            require(shared_counter.load()==257 && shared_pool.construction_attempts()==512);
            ids.clear();
            for(auto* skill:first.records())ids.insert(skill->instance_id);
            require(ids.size()==512 && !ids.contains(0) && ids.contains(257));
        }
        std::cout<<cases<<" original creation scenarios, "<<operations<<" operations matched\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
