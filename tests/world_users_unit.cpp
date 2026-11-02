#include "eternity/game/world_users.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Scene final:ActorScene {
    void unexpected() const {throw std::runtime_error("World index invoked unrelated actor service");}
    void send_self(const net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const net::Packet&) override {unexpected();}
    void send_cell_view(const net::Packet&) override {unexpected();}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return {};}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 0;}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
std::string decode(std::string value) {
    if(value=="-") return {};std::string result;
    for(std::size_t i=0;i<value.size();i+=2) result+=static_cast<char>(std::stoul(value.substr(i,2),nullptr,16));return result;
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original world index oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open world index oracle");
        ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;std::array<WorldAdmissionUser,3> states;
        std::array<std::unique_ptr<Actor>,3> actors;std::array<std::unique_ptr<WorldUser>,3> users;
        std::array<std::uint32_t,3> ids{};std::array<std::string,3> names;std::unique_ptr<WorldUsers> index;
        std::vector<std::string> events;std::size_t next=0,cases=0,steps=0;std::string line;
        const auto number=[&](WorldUser* user){for(unsigned int i=0;i<3;++i) if(user==users[i].get()) return i+1;return 0U;};
        const auto notify=[&](const char* kind,WorldUser& user){events.push_back(std::string(kind)+' '+std::to_string(number(&user)-1)+' '+std::to_string(index->id_count())+' '+std::to_string(index->name_count()));};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {
                index.reset();for(auto& id:ids) row>>id;for(auto& name:names) {row>>name;name=decode(name);}
                for(unsigned int i=0;i<3;++i) {
                    states[i].location=0;states[i].logout_tick=0;
                    actors[i]=std::make_unique<Actor>(ids[i],EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{items,enchant,mobs});
                    actors[i]->appearance().identity.name=names[i];users[i]=std::make_unique<WorldUser>(WorldUser{*actors[i],states[i]});
                }
                index=std::make_unique<WorldUsers>(WorldUserPorts{[&](WorldUser& user){notify("ENTER",user);},[&](WorldUser& user){notify("LEAVE",user);}});++cases;
            } else if(tag=="STEP") {
                require(next==events.size(),"Unconsumed world lifecycle callback");std::string op;unsigned int i=0;std::uint32_t location=0,logout=0;row>>op>>i>>location>>logout;events.clear();next=0;
                if(op=="ENTER") index->enter(*users.at(i));else if(op=="LEAVE") index->leave(*users.at(i));else {states.at(i).location=location;states.at(i).logout_tick=logout;}++steps;
            } else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Original world lifecycle callback/order differs at step "+std::to_string(steps));++next;}
            else if(tag=="RESULT") {
                std::size_t id_count=0,name_count=0;row>>id_count>>name_count;
                require(index->id_count()==id_count && index->name_count()==name_count,"Original duplicate-key index counts differ");
                for(unsigned int mode=0;mode<3;++mode) for(const auto key:{ids[0],ids[1],ids[2],123U}) {
                    unsigned int expected=0;row>>expected;const auto result=mode==0?index->find(key):mode==1?index->find_zone(key):index->find_zone_move(key);
                    require(number(result)==expected,"Original numeric lookup/filter differs at step "+std::to_string(steps));
                }
                for(unsigned int mode=0;mode<3;++mode) for(const auto& key:{names[0],names[1],names[2],std::string("Missing")}) {
                    unsigned int expected=0;row>>expected;const auto result=mode==0?index->find(key):mode==1?index->find_zone(key):index->find_zone_move(key);
                    require(number(result)==expected,"Original name lookup/filter differs at step "+std::to_string(steps));
                }
            } else if(tag=="END") require(next==events.size(),"Extra world lifecycle callback");else throw std::runtime_error("Unknown world index fixture row");
        }
        require(cases==192 && steps==4608,"World index fixture coverage changed");
        std::cout<<cases<<" original world tree sequences, "<<steps<<" entry/leave/state operations and ID/name/zone/move queries passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
