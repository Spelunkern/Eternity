#include "eternity/game/vertical_protocol.hpp"
#include <bit>
#include <iostream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Scene final:ActorScene ,public ActorEffectDatabase {
    std::vector<net::Packet> effect_database_packets;
    void send_effect_database(const net::Packet& packet) noexcept override {effect_database_packets.push_back(packet);}
    std::vector<std::string> events;std::optional<bool> party;std::uint32_t next=100;
    void record(char route,const net::Packet& packet) {static constexpr char hex[]="0123456789abcdef";std::string result(1,route);for(auto byte:packet){result+=hex[byte>>4];result+=hex[byte&15];}events.push_back(std::move(result));}
    void send_self(const net::Packet& packet) noexcept override {record('S',packet);}
    void send_nearby(const net::Packet& packet) override {record('N',packet);}
    void send_cell_view(const net::Packet& packet) override {record('C',packet);}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void send_party(const net::Packet& packet) noexcept override {record('P',packet);}
    void reset_damage() override {events.emplace_back("D");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return next++;}
    void release_effect_id(std::uint32_t id) noexcept override {events.push_back("F"+std::to_string(id));}
    std::uint32_t tick_ms() const noexcept override {return 13001;}
    void unexpected() {throw std::runtime_error("Unexpected scene operation");}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
int main() {try {
        const MobAttackTemplates default_mobs;
        storage::ResultSet skill_table;skill_table.columns={"SkillID","SkillLevel","TypeDetail","TypeEffect","Weapon1","Weapon2","Weaponvalue","Bag",
            "AbilityType1","AbilityValue1","AbilityType2","AbilityValue2","AbilityType3","AbilityValue3",
            "AttrType","DefenceType","DefenceValue","ApplyRange","Arrow","ChangeType","ChangeLevel","FixRange","KeepTime","TimeHealHP","TimeHealSP","TimeHealMP","LimitHP"};
        for(unsigned int id:{100,101,102}) {
            storage::Row row(skill_table.columns.size(),storage::Cell{"0"});row[0]=std::to_string(id);row[1]="1";row[2]=id==100?"4":"82";row[3]=id==100?"0":"4";
            row[8]="1";row[9]=id==100?"40":"100";row[10]="2";row[11]=id==100?"60":"100";row[12]="4";row[13]=id==100?"7":"5";row[22]="100";
            if(id==102) row[2]="93";skill_table.rows.push_back(row);
        }
        const auto skills=SkillDefinitions::load(skill_table);storage::ResultSet status_table{{"Level","Job","HP","MP","SP"},{{"1","0","1000","2000","3000"}}};
        const auto statuses=StatusRules::load(status_table);const ItemDefinitions items;const EnchantValues enchant;
        const auto quests=QuestDefinitions::decode(net::Packet{0,0,0,0});const ActorLoadRules rules{statuses,items,enchant,skills,quests,default_mobs};
        PlayerSnapshot player;auto& character=player.core.character;character.id=9001;character.level=1;character.map=1;character.strength=10;character.dexterity=20;
        character.recovery=30;character.intelligence=40;character.wisdom=50;character.luck=60;character.hp=2000;character.mp=3000;character.sp=4000;
        player.skills={{100,5,1,0,0}};player.effects={{101,1,30}};player.core.user_id=12345;player.core.money=0xfffffff0U;
        Scene scene;ActorLoadPipeline pipeline(player,rules,{10000,0,0,nullptr},scene);
        SocialCatalog social(player.core.user_id,character.id);EntryCatalogs catalogs(player.core.user_id);
        CharacterEntryState state;state.user_id=player.core.user_id;state.character_id=character.id;state.level=character.level;
        VerticalProtocolServices services;services.delivery.send_self=[&](net::Packet p){scene.record('S',p);};
        services.delivery.send_peer=[](std::uint32_t,net::Packet){};services.delivery.social=[](std::uint32_t)->const SocialCatalog*{return nullptr;};
        services.delivery.tick=[](){return 10000U;};services.delivery.kill_count=[](std::uint8_t){return EntryKillCount{};};
        services.send_manager=[&](net::Packet p){scene.record('M',p);};services.world_day=[](){return 1234U;};services.system_time=[](){return 5678U;};
        services.lock_guild=[](){return false;};services.unlock_guild=[](){};
        std::vector<CharacterEntryStep> lifecycle;services.lifecycle=[&](CharacterEntryStep step,Actor& actor){require(&actor==pipeline.result().actor.get(),"wrong actor");lifecycle.push_back(step);};
        deliver_vertical_entry(state,pipeline,player,social,catalogs,{0,100},"",services);
        require(state.ready && state.runtime_id==character.id,"entry did not finish");
        require(lifecycle==std::vector<CharacterEntryStep>{CharacterEntryStep::wait_user,CharacterEntryStep::init_guild,CharacterEntryStep::pvp_reset,CharacterEntryStep::flush},"lifecycle order differs");
        require(scene.events.size()>5 && scene.events[0]=="M030839300000" && scene.events[1].starts_with("S0b05") && scene.events[2]=="S04010029230000" && scene.events[3]=="S0404d2040000", "original 0407 initialization/ACK order differs");
        auto loaded=pipeline.release();require(loaded.actor && loaded.actor->id()==character.id,"pipeline ownership lost");
        Scene other_scene;ActorLoadPipeline other(player,rules,{10000,0,0,nullptr},other_scene);state.user_id++;
        bool rejected=false;try{deliver_vertical_entry(state,other,player,social,catalogs,{0,100},"",services);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected && other_scene.events.empty(),"foreign state caused side effects");
        std::cout<<"vertical entry composition passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
