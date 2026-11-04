#include "eternity/game/mob_targeting_bindings.hpp"
#include "eternity/game/mobs.hpp"
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool b,const char* message) {if(!b) throw std::runtime_error(message);}
eternity::storage::Row read_row(std::istringstream& row) {
    eternity::storage::Row result;std::string value;while(row>>value) result.push_back(value=="N"?eternity::storage::Cell{}:eternity::storage::Cell{value});return result;
}
void negative_contracts() {
    eternity::storage::ResultSet mobs,skills;mobs.rows.resize(1);mobs.rows[0].resize(69,std::string("0"));skills.rows.resize(1);skills.rows[0].resize(82,std::string("0"));
    mobs.rows[0][0]="1";skills.rows[0][0]="1";skills.rows[0][16]="1";skills.rows[0][37]="65";mobs.rows[0][45]="1";mobs.rows[0][52]="255";
    auto original=MobTargetingBindingCatalog::load(1,1,mobs,skills);const auto* pointer=original.mob(1)->attacks[0].skill;
    auto moved=std::move(original);check(moved.mob(1)->attacks[0].skill==pointer && pointer==moved.skill(1),"Move invalidated owned skill references");
    check(!moved.skill(0) && !moved.skill(2) && !moved.mob(2),"Sparse/boundary lookup guard differs");
    auto fail=[&] {bool caught=false;try {(void)MobTargetingBindingCatalog::load(1,1,mobs,skills);}catch(const std::runtime_error&) {caught=true;}check(caught,"Invalid positional input accepted");};
    mobs.rows[0][44]="256";fail();mobs.rows[0][44]="0";
    mobs.rows[0].resize(68);fail();mobs.rows[0].resize(69,std::string("0"));
    skills.rows[0][0]="2";fail();skills.rows[0][0]="1";
    skills.rows[0][37]="-32769";fail();skills.rows[0][37]="0";
    skills.rows[0].resize(81);fail();
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected binding oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot open binding oracle");
        eternity::storage::ResultSet mobs,skills;std::optional<MobTargetingBindingCatalog> catalog;
        unsigned mobmax=0,skillmax=0,scenarios=0,definitions=0,skillchecks=0,inits=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char op;row>>op;
            if(op=='S') {
                unsigned scenario;row>>scenario>>mobmax>>skillmax;mobs={};skills={};catalog.reset();
                // Deliberately misleading duplicate labels: only procedure positions matter.
                mobs.columns.resize(69,"ignored");skills.columns.resize(82,"ignored");++scenarios;
            } else if(op=='K' || op=='M') {(op=='K'?skills:mobs).rows.push_back(read_row(row));continue;}
            else if(op=='P' || op=='D') {
                if(!catalog) catalog.emplace(MobTargetingBindingCatalog::load(static_cast<std::uint16_t>(mobmax),static_cast<std::uint16_t>(skillmax),mobs,skills));
                check(catalog->mob_slots()==mobmax+1 && catalog->skill_slots()==skillmax+1,"MaxID+1 allocation differs");
                if(op=='P') {
                    unsigned slot,id,effect,detail,cooldown,level;row>>slot>>id>>effect>>detail>>cooldown>>level;const auto* s=catalog->skill(slot);
                    if(id==0) check(!s,"Original sparse skill unexpectedly present");
                    else check(s && s->id==id && s->effect==effect && s->detail==detail && s->cooldown_seconds==cooldown && s->level==level,"Original skill loader projection differs");
                    ++skillchecks;
                } else {
                    unsigned id,ai,range,country;row>>id>>ai>>range>>country;const auto* d=catalog->mob(id);
                    check(d && d->ai==ai && d->chase_range==range && d->country==static_cast<std::int32_t>(country),"Original definition country/range differs");
                    const auto projection=d->targeting();check(projection.ai==ai,"Targeting AI binding differs");
                    for(unsigned i=0;i<3;++i) {
                        std::array<std::uint32_t,11> e{};for(auto& v:e) row>>v;const auto& a=d->attacks[i];const auto* s=a.skill;
                        const std::array<std::uint32_t,11> actual{a.type,a.enabled,a.time,a.range,a.damage,a.damage_add,a.attribute,a.special,s?s->id:0U,s?s->cooldown_seconds:0U,s?s->level:0U};
                        if(actual!=e) throw std::runtime_error("Original attack skill binding differs at scenario "+std::to_string(scenarios-1)+" mob "+std::to_string(id)+" slot "+std::to_string(i));
                        check(projection.attacks[i].enabled==(a.enabled!=0) && projection.attacks[i].has_skill==(s!=nullptr) && projection.attacks[i].cooldown_seconds==(s?s->cooldown_seconds:0),"Targeting enabled/pointer/cooldown conflated");
                        if(s) check(s==catalog->skill(a.type),"Skill pointer not bound to owner slot");
                    }
                    ++definitions;
                }
            } else if(op=='I') {
                unsigned n;row>>n;for(unsigned id=1;id<=n;++id) {
                    const auto* d=catalog->mob(id);const auto before=d->targeting();MobRuntimeTemplate canonical;canonical.id=d->id;canonical.ai=d->ai;canonical.level=1;canonical.hp=100;
                    MobRuntime m;m.definition=&canonical;m.attack_times={111,222,333};MicrosoftRandom random;unsigned ai_calls=0;
                    MobInitPorts ports{random,[]{return 123U;},[&](MobRuntime&,std::uint8_t){++ai_calls;return false;},[&](MobRuntime&){++ai_calls;}};
                    initialize_mob(m,id,nullptr,ports);
                    check(m.definition==&canonical && m.attack_times==std::array<std::uint32_t,3>{},"Native Init definition retention/timer reset differs");
                    check(catalog->mob(id)==d && d->targeting().ai==before.ai,"Init moved catalog definition");
                    for(unsigned i=0;i<3;++i) check(d->targeting().attacks[i].cooldown_seconds==before.attacks[i].cooldown_seconds,"Init rewrote definition cooldown");
                    check(ai_calls==((d->ai>=10)?1U:0U),"Init AI provider route differs");++inits;
                }
            } else if(op!='E') throw std::runtime_error("Unknown binding oracle operation");
            check(static_cast<bool>(row),"Malformed binding oracle");
        }
        negative_contracts();check(scenarios==256 && definitions==1024 && skillchecks==2048 && inits==1024,"Binding coverage changed");
        std::cout<<scenarios<<" complete original skill/mob loads; "<<skillchecks<<" skill slots, "<<definitions<<" definitions and "<<inits<<" Init binding checks passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
