#include "eternity/game/world_admission.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Services final:WorldAdmissionServices {
    WorldAdmission* world=nullptr;RuntimeZone zone;ConfiguredMap map;std::atomic<std::uint32_t> counter{0};std::vector<std::string> events;
    unsigned int fail=0,wait_mask=0,guild_mask=0;bool success=false,open=false,vanish=false;
    std::function<void(WorldAdmissionUser&)> on_packets;
    Services() {zone.map=&map;}
    static unsigned int index(const WorldAdmissionUser& u) {return u.user_id-1;}
    void packets(WorldAdmissionUser& u) override {const auto i=index(u);events.push_back("PACKETS "+std::to_string(i));if(i<2 && (wait_mask&(1U<<i))) {wait_mask&=~(1U<<i);world->wait(u);}if(on_packets) on_packets(u);}
    RuntimeZone* select_zone(WorldAdmissionUser& u) override {const auto i=index(u);events.push_back("SELECT "+std::to_string(i));return fail&(1U<<i)?nullptr:&zone;}
    void register_world(WorldAdmissionUser& u) override {events.push_back("REGISTER "+std::to_string(index(u)));}
    bool enter_zone(WorldAdmissionUser& u,RuntimeZone&) override {events.push_back("ENTER "+std::to_string(index(u)));return success;}
    void close(WorldAdmissionUser& u,std::uint32_t reason) override {events.push_back("CLOSE "+std::to_string(index(u))+' '+std::to_string(reason));}
    void diagnostic(WorldAdmissionUser& u,WorldAdmissionError error) override {events.push_back("DIAGNOSTIC "+std::to_string(index(u))+' '+std::to_string(static_cast<unsigned int>(error)));}
    std::optional<std::uint32_t> rank_window(RuntimeZone&) override {events.push_back("WINDOW "+std::to_string(active));return open?std::optional<std::uint32_t>{0x1501010b}:std::nullopt;}
    bool lock_guild(WorldAdmissionUser& u) override {if(!u.guild_rank_entries) return false;events.push_back("GUILD_LOCK "+std::to_string(index(u)));if(vanish) {u.guild_rank_entries=nullptr;events.push_back("GUILD_UNLOCK "+std::to_string(index(u)));return false;}return true;}
    void unlock_guild(WorldAdmissionUser& u) override {events.push_back("GUILD_UNLOCK "+std::to_string(index(u)));}
    unsigned int active=0;
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original world admission oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open admission oracle");std::array<WorldAdmissionUser,3> users;Services services;std::unique_ptr<WorldAdmission> world;std::string line;std::size_t cases=0,steps=0,next=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {world.reset();std::uint32_t location=0;unsigned int permission=0;row>>location>>services.fail>>services.wait_mask>>services.success>>services.map.settings.create_type>>permission>>services.open>>services.guild_mask>>services.vanish;services.counter=0xfffffffe;world=std::make_unique<WorldAdmission>(services);services.world=world.get();for(unsigned int i=0;i<3;++i) {auto& u=users[i];u.user_id=i+1;u.location=i<2?1:0;u.logout_type=7;u.logout_tick=8;u.zone=nullptr;u.map=20;u.access={};u.access.permission=static_cast<std::uint8_t>(permission);u.access.instance_owner=77;u.access.last_rank_entry=88;u.guild_rank_entries=services.guild_mask&(1U<<i)?&services.counter:nullptr;if(i<2) world->track_selection(u);else world->track_connected(u);}users[0].location=location;++cases;}
            else if(tag=="STEP") {std::string op;unsigned int i=0;row>>op>>i;services.events.clear();next=0;services.active=i;const auto before=services.counter.load();if(op=="WAIT") world->wait(users[i]);else if(op=="UPDATE") world->update_packets();else if(op=="CLIENT") world->client_enter(users[i]);else if(op=="WORLD") world->world_enter(users[i]);else throw std::runtime_error("Unknown admission step");if(services.counter.load()!=before) {const auto unlock=std::find(services.events.begin(),services.events.end(),"GUILD_UNLOCK "+std::to_string(i));services.events.insert(unlock,"GUILD_INCREMENT "+std::to_string(i));}++steps;}
            else if(tag=="EVENT") {require(next<services.events.size() && services.events[next]==line.substr(6),"World admission effects differ at case "+std::to_string(cases)+" step "+std::to_string(steps)+" effect "+std::to_string(next)+": "+(next<services.events.size()?services.events[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="STATE") {std::vector<std::uint64_t> values{world->connected_count(),world->selection_count(),world->waiting_count(),world->zone_waiting_count(services.zone),services.counter.load()};for(const auto& u:users) values.insert(values.end(),{u.location,u.logout_type,u.logout_tick,u.zone?1U:0U,u.access.instance_owner,u.access.last_rank_entry,u.guild_rank_entries?1U:0U});for(auto value:values) {std::uint64_t expected=0;row>>expected;require(value==expected,"World admission state differs at case "+std::to_string(cases)+" step "+std::to_string(steps)+": "+std::to_string(value)+" vs "+std::to_string(expected));}require(next==services.events.size(),"Native world admission emitted extra effects");}
            else if(tag!="END") throw std::runtime_error("Unknown admission oracle row");
        }
        require(cases==1024 && steps==7168,"World admission oracle coverage changed");
        // The world-wait list is not visited until world transfer; once in the
        // zone-wait list, a successful client entry removes the current node.
        world.reset();Services s;WorldAdmission combined(s);s.world=&combined;
        WorldAdmissionUser first,second;first.user_id=1;second.user_id=2;
        combined.track_selection(first);combined.track_selection(second);
        require(combined.wait(first) && combined.wait(second),"Cannot queue zone users");
        s.events.clear();combined.update_zone_wait_packets(s.zone);
        require(s.events.empty(),"World-wait users dispatched by zone");
        combined.update_packets();s.events.clear();s.success=true;
        s.on_packets=[&](WorldAdmissionUser& u){require(combined.client_enter(u),"Zone entry failed");};
        combined.update_zone_wait_packets(s.zone);
        require(s.events==std::vector<std::string>{"PACKETS 0","ENTER 0","PACKETS 1","ENTER 1"},"Zone cursor lost a user after current removal");
        require(first.location==4 && second.location==4 && combined.zone_waiting_count(s.zone)==0,"Zone wait not drained");
        first.zone=nullptr;first.map=42;const auto registration_events=s.events.size();
        combined.wait_after_zone_leave(first);
        require(first.location==2 && combined.waiting_count()==1 && first.map==42 && s.events.size()==registration_events,"Transfer did not enqueue detached user without registration");
        bool duplicate=false;try {combined.wait_after_zone_leave(first);}catch(const std::logic_error&) {duplicate=true;}
        require(duplicate && combined.waiting_count()==1,"Duplicate transfer changed waiting membership");
        combined.update_packets();require(first.location==3 && first.zone==&s.zone && combined.zone_waiting_count(s.zone)==1,"Transfer did not reach destination zone wait");
        require(combined.world_enter(first) && first.location==4 && combined.zone_waiting_count(s.zone)==0,"Transfer did not complete real world entry");
        bool still_attached=false;try {combined.wait_after_zone_leave(first);}catch(const std::logic_error&) {still_attached=true;}
        require(still_attached && first.location==4 && first.zone==&s.zone,"Transfer accepted user before zone leave");
        std::cout<<cases<<" original world admission sequences / "<<steps<<" list/wait/client/world entry operations passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
