#include "eternity/game/blessing.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet unhex(const std::string& text) {
    require(text.size()%2==0,"Odd hex length");net::Packet result;
    for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return result;
}
std::vector<std::int32_t*> fields(SkillStatus& status,std::array<std::int32_t,3>& recovery) {
    auto& equipment=status.equipment;auto& a=equipment.base.attributes;
    std::vector<std::int32_t*> result{&a.strength,&a.dexterity,&a.recovery,&a.intelligence,&a.wisdom,&a.luck};
    for(auto& value:equipment.standing_recovery) result.push_back(&value);
    for(auto& value:equipment.sitting_recovery) result.push_back(&value);
    for(auto& value:recovery) result.push_back(&value);result.push_back(&equipment.attack.critical);
    for(auto& category:equipment.attack.categories) for(auto* value:{&category.accuracy,&category.attack,&category.evasion,&category.defense}) result.push_back(value);
    return result;
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected blessing oracle file");std::ifstream input(argv[1]);require(input.good(),"Cannot open blessing oracle");std::string line;
        unsigned int loads=0,initializations=0,packets=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char kind=0;row>>kind;
            if(kind=='L') {
                std::string hex;row>>hex;auto data=unhex(hex);const auto definitions=BlessingDefinitions::decode(data);
                std::array<std::size_t,2> sizes{};row>>sizes[0]>>sizes[1];
                for(std::uint8_t country=0;country<2;++country) {
                    require(definitions.size(country)==sizes[country],"Blessing country count differs");
                    for(std::size_t i=0;i<sizes[country];++i) {
                        BlessingDefinition expected;row>>expected.index>>expected.kill_count>>expected.kill_value;for(auto& value:expected.modifiers) row>>value;
                        const auto* result=definitions.find(country,static_cast<std::int32_t>(i));
                        require(result && result->index==expected.index && result->kill_count==expected.kill_count && result->kill_value==expected.kill_value && result->modifiers==expected.modifiers,"Original blessing record differs");
                    }
                    for(const auto kills:{0U,49U,50U,99U,100U,1000U,0xffffffffU}) {
                        const BlessingDefinition* expected=nullptr;
                        for(std::size_t i=0;i<sizes[country];++i) {const auto* candidate=definitions.find(country,static_cast<std::int32_t>(i));if(candidate->kill_count<=kills) expected=candidate;}
                        require(definitions.by_count(country,kills)==expected,"Blessing lookup changed insertion order");
                    }
                    require(!definitions.find(country,-1) && !definitions.find(country,static_cast<std::int32_t>(sizes[country])),"Invalid blessing index accepted");
                }
                require(!definitions.find(2,0) && !definitions.by_count(2,0) && definitions.size(2)==0,"Invalid blessing country accepted");
                data.pop_back();bool rejected=false;try{BlessingDefinitions::decode(data);}catch(const std::runtime_error&){rejected=true;}require(rejected,"Truncated blessing data accepted");++loads;
            } else if(kind=='I') {
                std::array<std::int32_t,31> initial{},expected{};std::array<std::int32_t,2> defenses{};BlessingDefinition definition;
                for(auto& value:initial) row>>value;for(auto& value:defenses) row>>value;for(auto& value:definition.modifiers) row>>value;
                for(auto& value:expected) row>>value;std::array<std::int32_t,28> expected_applied{};for(auto& value:expected_applied) row>>value;
                SkillStatus status;std::array<std::int32_t,3> recovery{};const auto pointers=fields(status,recovery);
                for(std::size_t i=0;i<pointers.size();++i) *pointers[i]=initial[i<6?i:i+3];
                status.equipment.base.maximum={static_cast<std::uint32_t>(initial[6]),static_cast<std::uint32_t>(initial[7]),static_cast<std::uint32_t>(initial[8])};
                const auto applied=initialize_blessing(status,recovery,definition,defenses);
                const auto context=" at blessing initialization "+std::to_string(initializations);
                require(applied==expected_applied,"Applied blessing modifiers differ"+context);
                for(std::size_t i=0;i<pointers.size();++i) require(*pointers[i]==expected[i<6?i:i+3],"Blessed field "+std::to_string(i)+" differs"+context);
                const auto maximum=status.equipment.base.maximum;
                require(std::bit_cast<std::int32_t>(maximum.hp)==expected[6] && std::bit_cast<std::int32_t>(maximum.mp)==expected[7] && std::bit_cast<std::int32_t>(maximum.sp)==expected[8],"Blessed resources differ"+context);++initializations;
            } else if(kind=='P') {
                unsigned int country=0;std::uint32_t kills=0,end=0,tick=0;std::string hex;row>>country>>kills>>end>>tick>>hex;
                require(blessing_packet(static_cast<std::uint8_t>(country),kills,end,tick)==unhex(hex),"Blessing notification differs");++packets;
            } else require(false,"Unknown blessing vector kind");
            require(static_cast<bool>(row),"Malformed blessing vector");std::string extra;require(!(row>>extra),"Trailing blessing vector data");
        }
        require(loads==3 && initializations==240 && packets==14,"Missing blessing coverage");
        std::cout<<loads<<" original data loads, "<<initializations<<" actor initializations and "<<packets<<" notifications passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
