#include "eternity/game/stats.hpp"
#include "eternity/game/creation.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main(int argc,char** argv) {
    using namespace eternity;using namespace eternity::game;
    try {
        require(argc==2,"Expected original attack regression vectors");
        Definitions defs;
        auto& table=defs.catalogs["StatusDefs"];table.columns={"Level","Job","HP","SP","MP"};
        const std::array<std::array<unsigned int,3>,6> resources={{{205,125,50},{250,65,65},{170,150,60},{175,135,70},{150,60,170},{160,70,150}}};
        for(unsigned int job=0;job<6;++job) table.rows.push_back({"1",std::to_string(job),std::to_string(resources[job][0]),
            std::to_string(resources[job][1]),std::to_string(resources[job][2])});
        table.rows.push_back({"60","0","9001","3001","1001"});
        defs.catalogs["Items"].columns={"Type","TypeID","Quality"};
        defs.catalogs["BaseItemDefs"].columns={"Family","Job","Type","TypeID"};
        const auto status=StatusRules::load(table);const auto creation=CreationRules::load(defs);
        for(std::uint8_t family=0;family<4;++family) for(std::uint8_t job=0;job<6;++job) for(std::uint8_t grow=0;grow<4;++grow) {
            constexpr std::array<unsigned int,4> allowed={0x23,0x1c,0x34,0x0b};
            if((allowed[family]&(1U<<job))==0) continue;
            CreateRequest request;request.family=family;request.job=job;request.grow=grow;
            auto character=creation.prepare(request,0).character;const auto loaded=status.base(character);
            require(loaded.maximum.hp==character.hp && loaded.maximum.mp==character.mp && loaded.maximum.sp==character.sp,
                "Creation and level-one initialization resources disagree");
            character.hp=1;character.mp=2;character.sp=3;
            const auto damaged=status.base(character);
            require(damaged.maximum.hp==loaded.maximum.hp && damaged.maximum.mp==loaded.maximum.mp && damaged.maximum.sp==loaded.maximum.sp,
                "Current stored resources changed calculated maximum");
            require(loaded.attributes.strength==character.strength && loaded.attributes.dexterity==character.dexterity &&
                loaded.attributes.recovery==character.recovery && loaded.attributes.intelligence==character.intelligence &&
                loaded.attributes.wisdom==character.wisdom && loaded.attributes.luck==character.luck,"Base attributes changed on initialization");
        }
        CharacterSummary fallback;fallback.level=61;fallback.job=5;fallback.grow=2;fallback.family=3;fallback.recovery=2;
        const auto loaded=status.base(fallback);
        require(loaded.maximum.hp==9921 && loaded.maximum.mp==1101 && loaded.maximum.sp==3301,"Original level-60 fallback or truncation changed");
        fallback.level=1;fallback.job=6;require(status.base(fallback).maximum.hp==9921,"Invalid job did not use original fallback");
        auto invalid=table;invalid.rows.push_back({"61","0","1","1","1"});
        bool rejected=false;try{StatusRules::load(invalid);}catch(const std::runtime_error&){rejected=true;}
        require(rejected,"Out-of-bounds original definitions accepted");

        std::ifstream vectors(argv[1]);require(vectors.good(),"Cannot open attack vectors");std::string line;unsigned int count=0;
        while(std::getline(vectors,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream input(line);unsigned int level=0;Attributes attributes;AttackModifiers modifiers;
            require(static_cast<bool>(input>>level>>attributes.strength>>attributes.dexterity>>attributes.recovery>>attributes.intelligence>>attributes.wisdom>>attributes.luck
                >>modifiers.item_attack>>modifiers.physical_defense>>modifiers.magic_defense>>modifiers.critical),"Malformed original vector");
            for(auto& add:modifiers.categories) require(static_cast<bool>(input>>add.accuracy>>add.attack>>add.evasion>>add.defense),"Malformed attack additions");
            const auto calculated=calculate_attack(static_cast<std::uint16_t>(level),attributes,modifiers);
            for(const auto& result:calculated) {
                for(const auto value:{result.accuracy,result.evasion,result.attack,result.defense,result.critical_attack,result.critical_defense}) {
                    std::int32_t expected=0;require(static_cast<bool>(input>>expected),"Missing original result");
                    if(value!=expected) throw std::runtime_error("Original SetAttack mismatch in vector "+std::to_string(count)+": "+std::to_string(value)+" != "+std::to_string(expected));
                }
            }
            std::string extra;require(!(input>>extra),"Trailing original vector data");++count;
        }
        require(count==84,"Original attack vector count changed");return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
