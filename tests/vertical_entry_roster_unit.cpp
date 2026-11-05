#include "eternity/game/vertical_entry.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
int main(int argc,char** argv) {try {
    if(argc!=2) throw std::runtime_error("Missing original roster oracle");
    std::ifstream in(argv[1]);if(!in) throw std::runtime_error("Cannot open original roster oracle");
    std::string line;unsigned count=0;
    while(std::getline(in,line)) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);std::uint32_t target;row>>target;CharacterRoster roster;
        for(auto& slot:roster.slots) {std::uint32_t id;unsigned rename;std::int32_t remaining;row>>id>>rename>>remaining;if(id) {CharacterSummary value;value.id=id;value.rename=rename;value.remaining_time=remaining;slot=value;}}
        unsigned expected;row>>expected;if(!row || vertical_entry_roster_allows(roster,target)!=bool(expected)) throw std::runtime_error("Original DB-agent roster branch differs");++count;
    }
    CharacterRoster wire;CharacterSummary raw;raw.id=123;raw.remaining_time=-1;raw.rename=255;wire.slots[0]=raw;
    const auto packets=character_list(wire,false);const auto& populated=packets.back();
    if(populated.size()!=83 || populated[75]!=1 || populated[76]!=1) throw std::runtime_error("Raw roster status changed client boolean flags");
    CharacterRoster empty;bool ambiguous=false;try{(void)vertical_entry_roster_allows(empty,0);}catch(const std::logic_error&){ambiguous=true;}
    if(!ambiguous || count!=2048) throw std::runtime_error("Missing cases or silently resolved empty-slot residue");
    std::cout<<count<<" original DB-agent roster branches passed\n";return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
