#include "eternity/game/mob_applied_skills_expiration.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity::game;
namespace {
void require(bool value) {if(!value)throw std::runtime_error("Skill expiration comparison failed");}
struct Services final:MobAppliedSkillPoolServices,MobAppliedSkillLogServices,MobAppliedSkillRemovalServices {
    MobAppliedSkills* owner=nullptr;
    MobAppliedSkillPool* pool=nullptr;
    MobAppliedSkillLogProjection projection;
    unsigned mode=0;
    std::vector<MobAppliedSkill*> objects;
    std::vector<std::string> events;
    std::unique_ptr<MobAppliedSkill> construct() override {
        auto object=std::make_unique<MobAppliedSkill>();objects.push_back(object.get());return object;
    }
    std::size_t index(const MobAppliedSkill* skill) const {
        const auto found=std::find(objects.begin(),objects.end(),skill);
        require(found!=objects.end());return static_cast<std::size_t>(found-objects.begin());
    }
    void removed_log(const MobAppliedSkill& skill) {
        const auto active=owner->records();const auto available=pool->available();
        require(std::find(active.begin(),active.end(),&skill)!=active.end());
        require(std::find(available.begin(),available.end(),&skill)==available.end());
        events.push_back("L"+std::to_string(index(&skill)));
    }
    void removed(const MobAppliedSkill& skill,std::uint8_t action) noexcept override {
        require(action==6);removed_log(skill);
    }
    void remove_effects(MobAppliedSkill& skill) noexcept override {
        const auto active=owner->records();const auto available=pool->available();
        require(std::find(active.begin(),active.end(),&skill)==active.end());
        require(std::find(available.begin(),available.end(),&skill)==available.end());
        events.push_back("R"+std::to_string(index(&skill)));
        skill.number^=0x55aa55aaU;
        const auto callback_mode=removed_count++==0?mode:0;
        if(callback_mode==2 && !active.empty())active.back()->end_tick=0;
        else if(callback_mode==1 || callback_mode==3) {
            owner->clear(projection);
            if(callback_mode==3 && !active.empty()) {
                auto* recycled=owner->append_from_pool();require(recycled!=nullptr);
                recycled->end_tick=0xffffffffU;
            }
        }
    }
    unsigned removed_count=0;
};
std::string pointers(const Services& services,const std::vector<MobAppliedSkill*>& records) {
    if(records.empty())return "-";
    std::ostringstream out;
    for(std::size_t i=0;i<records.size();++i){if(i)out<<',';out<<services.index(records[i]);}
    return out.str();
}
std::string snapshot(const Services& services,const MobAppliedSkillPool& pool,
                     const std::array<std::unique_ptr<MobAppliedSkills>,2>& owners) {
    std::ostringstream out;
    out<<pool.construction_attempts()<<'|'<<pointers(services,pool.available());
    for(const auto& owner:owners)out<<'|'<<pointers(services,owner->records());
    out<<'|';
    if(services.events.empty())out<<'-';
    for(std::size_t i=0;i<services.events.size();++i){if(i)out<<',';out<<services.events[i];}
    out<<'|';
    for(std::size_t i=0;i<services.objects.size();++i) {
        if(i)out<<';';const auto& s=*services.objects[i];
        out<<i<<':'<<s.instance_id<<':'<<s.skill_id<<':'<<s.level<<':'<<s.number<<':'<<s.attack_tick
           <<':'<<s.reset_tick<<':'<<s.end_tick<<':'<<s.previous_tick<<':'<<s.target_type<<':'<<s.target_id;
    }
    if(services.objects.empty())out<<'-';return out.str();
}
MobAppliedSkillLogProjection flags(unsigned bits) {return {(bits&1)!=0,(bits&2)!=0,(bits&4)!=0};}
}
int main(int argc,char** argv) {
    try {
        require(argc==2);std::ifstream input(argv[1]);require(input.good());
        std::unique_ptr<Services> services;std::unique_ptr<MobAppliedSkillPool> pool;
        std::array<std::unique_ptr<MobAppliedSkills>,2> owners;
        std::size_t cases=0,operations=0,line_number=0;
        for(std::string line;std::getline(input,line);) {
            ++line_number;if(line.empty() || line[0]=='#')continue;
            std::istringstream row(line);char op;unsigned owner;row>>op>>owner;
            if(op=='S') {
                for(auto& item:owners)item.reset();pool.reset();services=std::make_unique<Services>();
                pool=std::make_unique<MobAppliedSkillPool>(*services);services->pool=pool.get();
                for(auto& item:owners)item=std::make_unique<MobAppliedSkills>(*pool,*services);
                ++cases;continue;
            }
            services->owner=owners.at(owner).get();
            if(op=='A') {
                std::uint32_t end;std::size_t expected;row>>end>>expected;
                auto* skill=owners[owner]->append_from_pool();require(skill!=nullptr);
                const auto i=static_cast<std::uint32_t>(services->index(skill));require(i==expected);
                skill->instance_id=i+1;skill->skill_id=100+i;skill->level=i%5+1;
                skill->number=200+i;skill->attack_tick=300+i;skill->reset_tick=400+i;
                skill->end_tick=end;skill->previous_tick=500+i;skill->target_type=i%3;skill->target_id=600+i;
            } else if(op=='E') {
                std::uint32_t now;unsigned bits;row>>now>>bits>>services->mode;
                services->removed_count=0;services->projection=flags(bits);
                owners[owner]->expire(now,services->projection,*services);
            } else if(op=='C') {
                unsigned bits;row>>bits;services->projection=flags(bits);
                owners[owner]->clear(services->projection);
            } else require(false);
            std::string expected;row>>expected;require(!row.fail());
            const auto actual=snapshot(*services,*pool,owners);
            if(actual!=expected) {
                std::cerr<<"line "<<line_number<<"\nexpected "<<expected<<"\nactual   "<<actual<<'\n';return 1;
            }
            ++operations;
        }
        require(cases==128 && operations==1600);
        std::cout<<cases<<" original expiration scenarios, "<<operations<<" operations matched\n";
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
