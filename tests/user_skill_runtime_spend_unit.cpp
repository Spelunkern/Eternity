#include "eternity/game/user_skill_runtime_spend.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("fixture required");
    std::ifstream in(argv[1]);if(!in)throw std::runtime_error("missing fixture");
    std::string line;unsigned count=0;
    while(std::getline(in,line)){
        if(line.empty()||line[0]=='#')continue;
        unsigned n,sc,mc;std::uint32_t sp,mp,sr,mr,ns,nm,rs,rm;std::string packet;
        std::istringstream row(line);row>>n>>sp>>mp>>sc>>mc>>sr>>mr>>ns>>nm>>rs>>rm>>packet;
        if(!row)throw std::runtime_error("invalid fixture");
        const auto result=user_skill_runtime_spend_transition(sp,mp,static_cast<std::uint16_t>(sc),static_cast<std::uint16_t>(mc),std::bit_cast<std::int32_t>(sr),std::bit_cast<std::int32_t>(mr));
        std::string hex;constexpr char digits[]="0123456789abcdef";
        for(auto b:result.self_packet){hex+=digits[b>>4];hex+=digits[b&15];}
        if(result.sp!=ns||result.mp!=nm||std::bit_cast<std::uint32_t>(result.requested_sp)!=rs||std::bit_cast<std::uint32_t>(result.requested_mp)!=rm||hex!=packet)throw std::runtime_error("original spend differs case "+std::to_string(n));
        ++count;
    }
    if(count!=4096)throw std::runtime_error("incomplete fixture");
    std::cout<<count<<" complete original no-party spend transitions passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
