#include "eternity/game/mob_targeting_help.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
struct Services final:MobTargetingHelpServices {
    MobTargetingHelpSource source;
    std::array<MobTargetingHelpCandidate,9> candidates{};
    bool mutate=false;std::size_t cursor=0;std::string events;
    void visit_cell(std::int32_t x,std::int32_t z,const std::function<void(const MobTargetingHelpCandidate&)>& visitor) override {
        cursor=static_cast<std::size_t>(z*3+x);events+="L"+std::to_string(cursor)+" ";visitor(candidates.at(cursor));events+="U"+std::to_string(cursor)+" ";
    }
    void set_target_without_help() override {
        events+="A"+std::to_string(cursor)+" ";candidates.at(cursor).status=1;
        if(mutate) {source.x=8;source.z=0;source.mob_type=9;source.radius=0;}
    }
};
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Expected help oracle");
        std::ifstream input(argv[1]);if(!input) throw std::runtime_error("Cannot read help oracle");
        Services s;MobTargetingHelpUser user;unsigned kind=0,count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);char op;row>>op;
            if(op=='S') {
                s=Services{};user={};unsigned country,hp,attackable,visible,radius,mutate;
                row>>kind>>s.source.zone_user_count>>s.source.cells_x>>s.source.cells_z>>s.source.cell_x>>s.source.cell_z>>s.source.country>>country>>user.life_status>>hp>>attackable>>visible>>radius>>mutate;
                user.country=static_cast<std::uint8_t>(country);user.hp_ability=static_cast<std::uint8_t>(hp);user.attackable=attackable!=0;user.visible=visible!=0;
                s.source.mob_type=7;s.source.radius=static_cast<std::uint8_t>(radius);s.mutate=mutate!=0;
            } else if(op=='C') {
                unsigned i,status,fsm,type,ai;std::uint32_t x,z;row>>i>>status>>fsm>>type>>ai>>x>>z;
                s.candidates.at(i)={fsm==2?0:static_cast<std::int32_t>(status),static_cast<std::uint16_t>(type),static_cast<std::uint8_t>(ai),std::bit_cast<float>(x),std::bit_cast<float>(z)};
            } else if(op=='O') {
                mob_find_targeting_help(static_cast<MobTargetingHelpKind>(kind),s.source,user,s);
                std::istringstream actual(s.events);std::string a,e;bool equal=true;
                while(row>>e) {if(!(actual>>a) || a!=e) equal=false;}
                if(actual>>a) equal=false;
                if(!equal) throw std::runtime_error("Original help selection differs at case "+std::to_string(count)+": "+s.events);
                ++count;continue;
            } else throw std::runtime_error("Unknown help fixture operation");
            if(!row) throw std::runtime_error("Malformed help input");
        }
        if(count!=1536) throw std::runtime_error("Help coverage changed");
        std::cout<<count<<" original help selections and cell lock sequences passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
