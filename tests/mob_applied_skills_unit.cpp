#include "eternity/game/mob_applied_skills.hpp"
#include "eternity/game/skills.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

using namespace eternity::game;
namespace {
void require(bool value) {if (!value) throw std::runtime_error("Applied skill comparison failed");}
struct Services final:MobAppliedSkillPoolServices,MobAppliedSkillLogServices {
    bool fail=false;
    MobAppliedSkillPool* pool=nullptr;
    MobAppliedSkills* current=nullptr;
    std::vector<MobAppliedSkill*> objects;
    std::vector<std::unique_ptr<SkillDefinition>> definitions;
    std::vector<std::size_t> logs;
    std::size_t index(const MobAppliedSkill* skill) const {
        auto found=std::find(objects.begin(),objects.end(),skill);
        require(found!=objects.end());return static_cast<std::size_t>(found-objects.begin());
    }
    std::unique_ptr<MobAppliedSkill> construct() override {
        if(fail)return nullptr;
        auto skill=std::make_unique<MobAppliedSkill>();objects.push_back(skill.get());
        definitions.push_back(std::make_unique<SkillDefinition>());return skill;
    }
    void removed(const MobAppliedSkill& skill,std::uint8_t action) noexcept override {
        // Port observes the same pre-unlink ordering as original execution.
        const auto records=current->records();const auto available=pool->available();
        require(action==6 && !records.empty() && records.front()==&skill);
        require(std::find(available.begin(),available.end(),&skill)==available.end());
        const auto id=index(&skill);
        require(skill.name=="Skill"+std::to_string(id));
        logs.push_back(id);
    }
};
std::string indices(const std::vector<std::size_t>& values) {
    std::ostringstream out;
    for(std::size_t i=0;i<values.size();++i){if(i)out<<',';out<<values[i];}
    return values.empty()?"-":out.str();
}
std::string pointers(const Services& services,const std::vector<MobAppliedSkill*>& values) {
    std::vector<std::size_t> ids;for(auto* skill:values)ids.push_back(services.index(skill));return indices(ids);
}
std::string snapshot(const Services& services,const MobAppliedSkillPool& pool,
                     const std::array<std::unique_ptr<MobAppliedSkills>,2>& owners,
                     const std::vector<std::size_t>& deleted) {
    require(pool.owned_count()==services.objects.size()-deleted.size());
    std::ostringstream out;
    out<<pool.construction_attempts()<<'|'<<pointers(services,pool.available());
    for(const auto& owner:owners)out<<'|'<<(owner?pointers(services,owner->records()):"-");
    out<<'|'<<indices(services.logs)<<'|'<<indices(deleted)<<'|';
    bool any=false;
    for(std::size_t i=0;i<services.objects.size();++i) {
        if(std::find(deleted.begin(),deleted.end(),i)!=deleted.end())continue;
        if(any)out<<';';any=true;
        const auto& s=*services.objects[i];
        out<<i<<':'<<s.instance_id<<':'<<s.skill_id<<':'<<s.level<<':'<<s.number<<':'<<s.attack_tick
           <<':'<<s.reset_tick<<':'<<s.end_tick<<':'<<s.previous_tick<<':'<<s.target_type<<':'<<s.target_id;
        auto definition=std::find_if(services.definitions.begin(),services.definitions.end(),
            [&s](const auto& item){return item.get()==s.definition;});
        require(definition!=services.definitions.end());
        out<<':'<<static_cast<std::size_t>(definition-services.definitions.begin());
    }
    if(!any)out<<'-';return out.str();
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2);std::ifstream input(argv[1]);require(input.good());std::string line;
        std::unique_ptr<Services> services;
        std::unique_ptr<MobAppliedSkillPool> pool;
        std::array<std::unique_ptr<MobAppliedSkills>,2> owners;
        std::vector<std::size_t> deleted;std::size_t cases=0,operations=0,line_number=0;
        while(std::getline(input,line)) {
            ++line_number;if(line.empty() || line[0]=='#')continue;
            std::istringstream row(line);char op;unsigned owner;row>>op>>owner;
            if(op=='S') {
                for(auto& item:owners)item.reset();pool.reset();services=std::make_unique<Services>();
                pool=std::make_unique<MobAppliedSkillPool>(*services);services->pool=pool.get();
                for(auto& item:owners)item=std::make_unique<MobAppliedSkills>(*pool,*services);
                deleted.clear();++cases;continue;
            }
            services->current=owners.at(owner).get();
            if(op=='A') {
                unsigned fail;std::uint32_t stamp;row>>fail>>stamp;services->fail=fail!=0;
                auto* skill=owners[owner]->append_from_pool();
                if(skill && stamp) {
                    skill->instance_id=stamp;skill->skill_id=stamp+1;skill->level=stamp+2;
                    skill->number=stamp+3;skill->attack_tick=stamp+4;skill->reset_tick=stamp+5;
                    skill->end_tick=stamp+6;skill->previous_tick=stamp+7;
                    skill->target_type=stamp+8;skill->target_id=stamp+9;
                }
                if(skill) {
                    const auto id=services->index(skill);
                    skill->name="Skill"+std::to_string(id);
                    skill->definition=services->definitions[id].get();
                }
            } else if(op=='C') {
                unsigned flags;row>>flags;
                owners[owner]->clear({(flags&1)!=0,(flags&2)!=0,(flags&4)!=0});
            } else if(op=='D') {
                for(auto* skill:owners[owner]->records())deleted.push_back(services->index(skill));
                owners[owner].reset();
            } else require(false);
            std::string expected;row>>expected;auto actual=snapshot(*services,*pool,owners,deleted);
            if(actual!=expected) {
                std::cerr<<"line "<<line_number<<"\nexpected "<<expected<<"\nactual   "<<actual<<'\n';return 1;
            }
            ++operations;
        }
        require(cases==64 && operations==956);
        std::cout<<cases<<" original scenarios, "<<operations<<" ownership operations matched\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
