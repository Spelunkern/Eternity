#include "eternity/game/mob_targeting_lifecycle_pvp.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
using namespace eternity::game;
int main(int argc,char**argv) {
    if(argc!=2)throw std::runtime_error("Expected original fixture path");
    std::ifstream file(argv[1]);if(!file)throw std::runtime_error("Missing fixture");
    std::string line;unsigned count=0,duels=0,guilds=0,self=0;
    while(std::getline(file,line)) {
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);unsigned same,result;
        MobTargetingPvpRelation a{},b{};
        if(!(row>>same>>a.id>>a.duel_state>>a.duel_peer>>a.guild>>a.opposing_guild
             >>b.id>>b.duel_state>>b.duel_peer>>b.guild>>b.opposing_guild>>result))
            throw std::runtime_error("Invalid original record");
        const auto actual=static_cast<unsigned>(mob_targeting_pvp_death_type(a,b,same!=0));
        if(actual!=result)throw std::runtime_error("Original PvP type mismatch at "+std::to_string(count));
        ++count;duels+=result==4;guilds+=result==5;self+=same!=0;
    }
    if(count!=8192||duels==0||guilds==0||self!=4096)throw std::runtime_error("Incomplete branch coverage");
    // The helper does NOT require reciprocal duel state/ID on peer.
    MobTargetingPvpRelation a{1,4,2,7,8},b{2,0,999,8,7};
    if(mob_targeting_pvp_death_type(a,b,false)!=MobTargetingPvpDeathType::duel ||
       mob_targeting_pvp_death_type(a,b,true)!=MobTargetingPvpDeathType::ordinary)
        throw std::runtime_error("Identity or duel precedence lost");
    std::cout<<count<<" complete original PvP death classifications; duel="<<duels<<" guild="<<guilds<<" self="<<self<<'\n';
}
