#include "eternity/game/mob_experience.hpp"
#include <bit>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
int main(int argc,char** argv) {try {
    if(argc!=2) throw std::runtime_error("Expected mob experience oracle");std::ifstream input(argv[1]);if(!input.good()) throw std::runtime_error("Cannot read experience oracle");std::string line;unsigned cases=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned level,base;std::int32_t user,mode,active;std::string bits;std::uint32_t expected;
        row>>level>>base>>user>>mode>>active>>bits>>expected;if(!row || bits.size()!=16) throw std::runtime_error("Malformed experience vector");
        std::uint64_t raw=0;for(unsigned i=0;i<8;++i) raw|=std::stoull(bits.substr(i*2,2),nullptr,16)<<(i*8);
        const auto result=eternity::game::mob_experience(static_cast<std::uint16_t>(level),static_cast<std::uint16_t>(base),user,mode,active,std::bit_cast<double>(raw));
        if(static_cast<std::uint32_t>(result)!=expected) throw std::runtime_error("Original experience differs at case "+std::to_string(cases)+": "+std::to_string(result)+"/"+std::to_string(expected));++cases;
    }
    if(cases!=1024) throw std::runtime_error("Experience coverage changed");std::cout<<cases<<" original mob experience calculations passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
