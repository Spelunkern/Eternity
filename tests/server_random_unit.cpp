#include "eternity/game/random.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
using Bytes=std::vector<std::uint8_t>;
void number(Bytes& b,std::uint32_t v) {for(unsigned int i=0;i<4;++i) b.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
Bytes unhex(const std::string& text) {Bytes b;for(std::size_t i=0;i<text.size();i+=2) b.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));return b;}
void require(bool v,const char* s) {if(!v) throw std::runtime_error(s);}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected SRandom oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open SRandom oracle");std::string line;unsigned int scenarios=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char tag=0;unsigned int scenario=0,lazy=0;std::uint32_t seed=0;std::string outputs,expected_state;row>>tag>>scenario>>seed>>lazy>>outputs>>expected_state;require(static_cast<bool>(row) && tag=='R',"Malformed SRandom row");
            eternity::game::ServerRandom random(seed);if(!lazy) random.seed(seed);Bytes actual;
            for(unsigned int step=0;step<1300;++step) {if(step==650 && scenario%3==0) random.seed(seed^0xdeadbeefU);const std::array<std::uint32_t,9> bounds{0,1,2,3,100,10000,0xffffffffU,0x80000000U,65535};const auto bound=bounds[(scenario+step)%bounds.size()];const auto index=random.index();const auto words=random.words();number(actual,random.next(bound));if(bound==0) require(random.index()==index && random.words()==words,"Zero bound changed random state");}
            if(actual!=unhex(outputs)) throw std::runtime_error("Original SRandom stream differs at scenario "+std::to_string(scenario));Bytes state;number(state,random.seed());for(auto word:random.words()) number(state,word);number(state,random.index());if(state!=unhex(expected_state)) throw std::runtime_error("Original SRandom state differs at scenario "+std::to_string(scenario));++scenarios;
        }
        require(scenarios==128,"SRandom coverage changed");std::cout<<scenarios<<" original streams, 166400 draws and complete final states passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
