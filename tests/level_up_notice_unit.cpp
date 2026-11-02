#include "eternity/game/character_replica.hpp"
#include "eternity/game/level_up_notice.hpp"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iostream>
#include <stdexcept>
using namespace eternity::game;using Bytes=eternity::net::Packet;
void check(bool v) {if(!v) throw std::runtime_error("Original level notice mismatch");}
void number(Bytes& b,std::uint32_t value) {for(unsigned i=0;i<4;++i) b.push_back(static_cast<std::uint8_t>(value>>(i*8)));}
Bytes unhex(const std::string& hex) {Bytes b;for(unsigned i=0;i<hex.size();i+=2) b.push_back(static_cast<std::uint8_t>(std::stoul(hex.substr(i,2),nullptr,16)));return b;}
struct World final:LevelUpNoticeWorld {
    CharacterReplica* replica=nullptr;Bytes events;bool scene=false,guild=false,locked=false;int party=-1;
    void packet(char tag,const Bytes& p) {events.push_back(static_cast<std::uint8_t>(tag));number(events,static_cast<std::uint32_t>(p.size()));events.insert(events.end(),p.begin(),p.end());}
    void rebuild_attack() override {events.push_back('A');}
    void send_database(const Bytes& p) override {check(replica && replica->apply(p)==InventoryReplicaResult::applied);packet('B',p);}
    void send_self(const Bytes& p) override {packet('C',p);}
    bool in_zone() const override {return scene;}
    void send_scene(const Bytes& p,float radius,std::uint8_t priority) override {check(scene && radius==60 && priority==5);packet('V',p);}
    bool acquire_party() override {check(!locked);locked=party>=0;return locked;}
    std::uint8_t party_type() const override {check(locked);return static_cast<std::uint8_t>(party);}
    void update_party_experience() override {check(locked);events.push_back('P');}
    void send_party(const Bytes& p) override {check(locked);packet('T',p);}
    void release_party() override {check(locked);locked=false;}
    void energy_all_max() override {check(!locked);events.push_back('E');}
    void recover_add(PlayerResources p) override {events.push_back('R');number(events,p.hp);number(events,p.sp);number(events,p.mp);}
    void fill_game_log_main(std::span<std::uint8_t> p) override {check(p.size()==74);std::fill(p.begin(),p.end(),0);events.push_back('M');}
    void send_game_log(const Bytes& p) override {packet('G',p);}
    void update_guild_level(std::uint32_t id,std::uint16_t level) override {if(guild) {events.push_back('U');number(events,id);number(events,level);}}
};
int main(int argc,char** argv) {try {
    check(argc==2);std::ifstream input(argv[1]);check(input.good());eternity::storage::ResultSet rows;rows.columns={"Level","Grow","Exp"};for(unsigned g=1;g<=4;++g) for(unsigned l=1;l<=60;++l) rows.rows.push_back({std::to_string(l),std::to_string(g),"1000"});const auto curve=Experience::load(rows);std::string line;unsigned cases=0;
    while(std::getline(input,line)) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned scenario,grow,level,scene,guild;std::uint32_t gain,source;World world;std::string expected;row>>scenario>>grow>>level>>gain>>source>>scene>>world.party>>guild>>expected;check(static_cast<bool>(row));world.scene=scene!=0;world.guild=guild!=0;
        PlayerCore p;p.user_id=123;p.character.id=321;p.character.grow=static_cast<std::uint8_t>(grow);p.character.level=static_cast<std::uint16_t>(level);p.experience=12345;p.stat_points=11;p.skill_points=13;auto& c=p.character;c.strength=14;c.dexterity=12;c.intelligence=8;c.wisdom=7;c.recovery=9;c.luck=15;
        InventoryReplicaPool pool;CharacterReplica replica(p,pool);world.replica=&replica;
        notify_level_up(p,42,{65537,65538,65539},{1001,1002,1003},curve,gain,source,world);
        if(world.events!=unhex(expected)) throw std::runtime_error("Original level notice differs at scenario "+std::to_string(scenario));check(!world.locked);const auto persisted=replica.core();check(persisted.character.id==321 && persisted.character.intelligence==8 && persisted.character.wisdom==7 && persisted.character.recovery==9 && persisted.character.hp==1 && persisted.character.mp==2 && persisted.character.sp==3);++cases;
    }
    check(cases==64);std::cout<<cases<<" original level notification coordinators passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
