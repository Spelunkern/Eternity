#include "eternity/game/user_view_difference.hpp"
#include "eternity/game/user_view_publication.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace eternity::game;
struct Buffer final:UserViewBuffer {explicit Buffer(UserViewBufferKind k):kind(k){}UserViewBufferKind kind;};
struct Services final:UserViewPublicationServices {
    unsigned mask,attempts=0;bool shape=true,hide_after_enter=false;std::vector<std::string> events;
    explicit Services(unsigned m):mask(m){}
    std::unique_ptr<UserViewBuffer> allocate(UserViewBufferKind kind) override {auto i=attempts++;if(mask&(1U<<i))return {};return std::make_unique<Buffer>(kind);}
    void broadcast(CellCoordinates c,UserViewBuffer& b) override {events.push_back("B"+std::to_string(static_cast<unsigned>(static_cast<Buffer&>(b).kind))+":"+std::to_string(c.x)+":"+std::to_string(c.z));}
    void leave_snapshot(CellCoordinates c) override {events.push_back("L:"+std::to_string(c.x)+":"+std::to_string(c.z));}
    void enter_snapshot(CellCoordinates c) override {events.push_back("E:"+std::to_string(c.x)+":"+std::to_string(c.z));if(hide_after_enter)shape=false;}
    bool publish_shape() override{return shape;}
};
static std::string encode(const std::vector<CellCoordinates>& cells){
    if(cells.empty())return "-";std::string result;
    for(auto cell:cells){if(!result.empty())result+=',';result+=std::to_string(cell.x)+':'+std::to_string(cell.z);}return result;
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("Expected original view oracle");std::ifstream input(argv[1]);
    if(!input)throw std::runtime_error("Cannot open original view oracle");std::string line;unsigned cases=0;
    while(std::getline(input,line)){if(line.empty() || line[0]=='#')continue;
        std::istringstream row(line);CellCoordinates old_cell,new_cell;std::string old_expected,new_expected,extra;
        if(!(row>>old_cell.x>>old_cell.z>>new_cell.x>>new_cell.z>>old_expected>>new_expected) || row>>extra)throw std::runtime_error("Malformed view oracle");
        std::vector<CellCoordinates> leaving,entering;bool entered=false;
        user_view_difference(10,10,old_cell,new_cell,[&](auto cell){if(entered)throw std::runtime_error("Leave after enter");leaving.push_back(cell);},[&](auto cell){entered=true;entering.push_back(cell);});
        if(encode(leaving)!=old_expected || encode(entering)!=new_expected)throw std::runtime_error("Original user view traversal differs at "+std::to_string(cases));++cases;
    }
    if(cases!=625)throw std::runtime_error("Missing original cases");
    bool rejected=false;try{user_view_difference(10,10,{-1,0},{0,0},[](auto){},[](auto){});}catch(const std::invalid_argument&){rejected=true;}
    if(!rejected)throw std::runtime_error("Invalid cell accepted");
    for(unsigned mask=0;mask<16;++mask){Services services(mask);publish_user_view_difference(10,10,{3,3},{4,3},services);std::vector<std::string> expected;
        for(int z=1;z<=5;++z)if(!(mask&1)){expected.push_back("B0:1:"+std::to_string(z));expected.push_back("L:1:"+std::to_string(z));}
        for(int z=1;z<=5;++z)if(!(mask&6)){expected.push_back("B1:6:"+std::to_string(z));expected.push_back("B2:6:"+std::to_string(z));expected.push_back("E:6:"+std::to_string(z));}
        for(int z=1;z<=5;++z)if(!(mask&8))expected.push_back("B3:6:"+std::to_string(z));
        if(services.attempts!=4 || services.events!=expected)throw std::runtime_error("Original buffer failure trace differs");
    }
    Services hidden(0);hidden.shape=false;publish_user_view_difference(10,10,{3,3},{4,3},hidden);
    if(hidden.attempts!=3)throw std::runtime_error("Suppressed shape allocated a buffer");
    Services changed(0);changed.hide_after_enter=true;publish_user_view_difference(10,10,{3,3},{4,3},changed);
    if(changed.attempts!=3)throw std::runtime_error("Shape state was sampled before entry callbacks");
    std::cout<<cases<<" original user view traversal cases passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
