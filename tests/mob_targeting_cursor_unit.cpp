#include "eternity/game/mob_targeting_help.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <array>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok,const char* message) {if(!ok) throw std::runtime_error(message);}
// Test-only cursor probe. send_leave already traverses ZoneMobs' actual shared
// cell cursor; packets are consumed locally to observe visits, never transported.
// Production integration needs a direct lock-held visitor API, not this probe.
struct Case final:MobTargetingHelpServices,ZoneMobServices {
    std::array<MobRuntime,6> mobs;
    ZoneMobs zone{MapGeometry(1),*this};
    unsigned count=0,mode=0,trigger=0,calls=0,current=0;
    std::string events;
    std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected clock");}
    void send_cell_view(CellCoordinates,const eternity::net::Packet& p) override {check(p.size()==17 && p[0]==1 && p[1]==6,"Unexpected entry delivery");}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected movement");}
    void lua_init(MobRuntime&) override {throw std::runtime_error("Unexpected Lua");}
    struct Viewer final:MobViewer {
        Case& owner;const std::function<void(const MobTargetingHelpCandidate&)>& visit;
        Viewer(Case& c,const std::function<void(const MobTargetingHelpCandidate&)>& f):owner(c),visit(f) {}
        void send_self(const eternity::net::Packet& p) override {
            check(p.size()==6 && p[0]==2 && p[1]==6,"Unexpected cursor probe packet");
            std::uint32_t id=0;for(unsigned i=0;i<4;++i) id|=static_cast<std::uint32_t>(p[2+i])<<(8*i);
            auto* m=owner.zone.find(id);check(m!=nullptr,"Cursor delivered detached mob");owner.current=id;
            visit({mob_status(*m),m->type,1,m->position.x,m->position.z});
        }
    };
    void visit_cell(std::int32_t x,std::int32_t z,const std::function<void(const MobTargetingHelpCandidate&)>& f) override {
        check(x==0 && z==0,"Unexpected cell");events+="L ";Viewer v(*this,f);zone.send_leave({0,0},v);events+="U ";
    }
    void remove(unsigned id) {
        if(id<1 || id>count) return;
        events+="L ";check(zone.leave(mobs.at(id-1)),"Removal failed");events+="U ";
    }
    void add(unsigned id) {events+="L ";check(zone.enter(mobs.at(id-1)),"Append failed");events+="U ";}
    void find(unsigned id) {events+="L ";const auto* m=zone.find_cell({0,0},id);events+="U F"+std::to_string(m?m->id:0)+" ";}
    void set_target_without_help() override {
        events+="A"+std::to_string(current)+" ";if(calls++!=trigger) return;
        switch(mode) {
        case 0:break;
        case 1:remove(current);break;
        case 2:remove(current+1);break;
        case 3:remove(current-1);break;
        case 4:add(count+1);break;
        case 5:find(count);break;
        case 6:find(99);break;
        case 7:find(current);break;
        case 8:remove(current);add(current);break;
        case 9:remove(current+1);find(99);break;
        default:throw std::runtime_error("Unknown cursor mutation");
        }
    }
    void initialize(unsigned fsm) {
        for(unsigned i=0;i<count+1;++i) {
            auto& m=mobs.at(i);m.id=i+1;m.type=7;
            if(fsm) {m.status=42;m.state_stack=std::vector<std::int32_t>{};if(fsm==1) m.state_stack->push_back(0);}
            if(i<count) check(zone.enter(m),"Cannot initialize cursor fixture");
        }
    }
};
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected cursor oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot read cursor oracle");std::string line;unsigned scenarios=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            Case c;unsigned kind=0,fsm=0;std::istringstream row(line);row>>kind>>c.count>>c.mode>>c.trigger>>fsm;char separator=0;row>>separator;check(separator=='|',"Malformed cursor header");
            std::string expected_events;std::getline(row,expected_events,'|');std::vector<unsigned> expected_ids;unsigned id=0;while(row>>id) expected_ids.push_back(id);
            c.initialize(fsm);MobTargetingHelpSource source;source.zone_user_count=1;source.cells_x=source.cells_z=1;source.country=1;source.mob_type=7;source.radius=255;
            MobTargetingHelpUser user;user.attackable=user.visible=true;
            mob_find_targeting_help(static_cast<MobTargetingHelpKind>(kind),source,user,c);
            std::istringstream expected(expected_events),actual(c.events);std::string a,e;bool equal=true;
            while(expected>>e) if(!(actual>>a) || a!=e) equal=false;
            if(actual>>a) equal=false;
            if(!equal) throw std::runtime_error("Original live-cursor trace differs at "+std::to_string(scenarios)+": "+c.events);
            std::vector<unsigned> ids;for(auto* m:c.zone.in_cell({0,0})) {ids.push_back(m->id);check(c.zone.find(m->id)==m,"Global registry diverged");}
            check(ids==expected_ids && ids.size()==c.zone.size(),"Original final membership/order differs");
            ++scenarios;
        }
        check(scenarios==450,"Cursor coverage changed");std::cout<<scenarios<<" original helper mutation traces match real ZoneMobs shared cursors\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
