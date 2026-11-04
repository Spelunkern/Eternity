#include "eternity/game/mob_targeting_find.hpp"
#include "eternity/game/mob_targeting.hpp"
#include <array>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <vector>
using namespace eternity::game;
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
struct Services final:MobTargetingFindServices,MobTargetingServices {
    MobTargetingFindSource source;MobTargetingState mob;
    std::array<MobTargetingFindUser,12> users{};
    std::array<std::vector<unsigned>,9> cells;
    bool mutate=false,locked=false;std::string events;
    void visit_cell_users(std::int32_t x,std::int32_t z,const std::function<bool(const MobTargetingFindUser&)>& visitor) override {
        const auto cell=static_cast<std::size_t>(z*3+x);check(!locked,"Nested cell acquisition");locked=true;
        events+="L"+std::to_string(cell)+" ";if(mutate) {source.radius=5;source.x=3;source.z=4;}
        for(auto i:cells.at(cell)) if(!visitor(users.at(i))) break;
        events+="U"+std::to_string(cell)+" ";locked=false;
    }
    void set_target_with_help(std::uintptr_t token) override {
        check(locked && token>0 && token<=users.size(),"Acquisition outside selected cell lock");
        events+="A"+std::to_string(token)+" ";const auto& user=users.at(token-1);
        mob_set_target(mob,{static_cast<std::uint32_t>(99+token),user.life_status},true,*this);
    }
    std::int32_t random() override {throw std::runtime_error("Unexpected attack random");}
    std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected attack timer");}
    void help(MobTargetingState&,const MobTargetingUser&,std::uint8_t kind) override {check(locked,"Help outside selected cell lock");events+="H"+std::to_string(kind)+" ";}
    void dispatch(MobTargetingState&,std::uint32_t,std::uint32_t) override {throw std::runtime_error("Unexpected FSM");}
};
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected find oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot read find oracle");
        std::string line;Services s;unsigned entry=0,count=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char op;row>>op;
            if(op=='S') {
                s=Services{};unsigned ai,radius,fixed,mutate;
                row>>entry>>ai>>s.source.zone_user_count>>s.source.cells_x>>s.source.cells_z>>s.source.cell_x>>s.source.cell_z>>radius>>s.mob.status>>fixed>>mutate;
                s.source.ai=static_cast<std::uint8_t>(ai);s.source.radius=static_cast<std::uint8_t>(radius);s.mutate=mutate!=0;
                s.mob.ai=static_cast<std::uint8_t>(ai);s.mob.vision_fixed_user=fixed;s.mob.target_type=7;s.mob.target_id=12345;s.mob.next_move_time=777;
            } else if(op=='C') {
                unsigned i,cell,shape,hp,attack,visible,country;std::uint32_t x,z;std::int32_t life;
                row>>i>>cell>>life>>shape>>hp>>attack>>visible>>country>>x>>z;
                s.cells.at(cell).push_back(i);s.users.at(i)={i+1,life,static_cast<std::uint8_t>(shape),static_cast<std::uint8_t>(hp),attack!=0,visible!=0,std::bit_cast<float>(x),std::bit_cast<float>(z)};
                // Country is intentionally absent from the search projection:
                // original FindTarget has no country check, including same side.
                (void)country;
            } else if(op=='O') {
                std::array<std::uint32_t,5> expected{};for(auto& v:expected) row>>v;
                char separator=0;row>>separator;check(separator=='|',"Malformed find result");
                const auto result=entry?static_cast<std::uintptr_t>(mob_targeting(s.source,s)):mob_find_target(s.source,s);
                const std::array<std::uint32_t,5> actual{static_cast<std::uint32_t>(result),s.mob.target_type,s.mob.target_id,static_cast<std::uint32_t>(s.mob.status),s.mob.next_move_time};
                std::istringstream events(s.events);std::string a,e;bool equal=true;
                while(row>>e) if(!(events>>a) || a!=e) equal=false;
                if(events>>a) equal=false;
                if(actual!=expected || !equal) throw std::runtime_error("Original user search differs at case "+std::to_string(count)+": "+s.events);
                check(!s.locked,"Cell lock retained after acquisition");++count;continue;
            } else throw std::runtime_error("Unknown find fixture operation");
            check(static_cast<bool>(row),"Malformed find fixture");
        }
        check(count==2048,"Find coverage changed");std::cout<<count<<" original first-user searches with real native SetTarget comparisons passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
