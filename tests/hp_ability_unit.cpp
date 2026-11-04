#include "eternity/game/actor.hpp"
#include "eternity/game/entry_packets.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
SkillDefinitions definitions(unsigned int limit,std::array<std::uint16_t,3> values,std::uint32_t keep,bool available=true) {
    storage::ResultSet table;table.columns={"SkillID","SkillLevel","TypeDetail","TypeEffect","Weapon1","Weapon2","Weaponvalue","Bag",
        "AbilityType1","AbilityValue1","AbilityType2","AbilityValue2","AbilityType3","AbilityValue3",
        "AttrType","DefenceType","DefenceValue","ApplyRange","Arrow","ChangeType","ChangeLevel","FixRange","KeepTime","TimeHealHP","TimeHealSP","TimeHealMP","LimitHP"};
    if(available) for(unsigned int level=1;level<=3;++level) {
        storage::Row row(table.columns.size(),storage::Cell{"0"});row[0]="200";row[1]=std::to_string(level);row[2]="6";
        row[8]="4";row[9]=std::to_string(values[0]);row[10]="1";row[11]=std::to_string(values[1]);row[12]="16";row[13]=std::to_string(values[2]);
        row[22]=std::to_string(keep);row[26]=std::to_string(limit);table.rows.push_back(std::move(row));
    }
    return SkillDefinitions::load(table);
}
std::string packet(char route,const net::Packet& data) {
    static constexpr char hex[]="0123456789abcdef";std::string result(1,route);
    for(auto byte:data) {result+=hex[byte>>4];result+=hex[byte&15];}return result;
}
struct Harness final:EffectWorld,EffectActivationObserver {
    std::uint32_t tick=0;bool allocate=true;std::optional<bool> party;EffectStatus& status;
    mutable std::vector<std::string> events;EffectReplica replica{{}};
    explicit Harness(EffectStatus& value):status(value) {}
    std::optional<std::uint32_t> allocate_id() noexcept override {events.emplace_back("A");return allocate?std::optional<std::uint32_t>{1000}:std::nullopt;}
    void release_id(std::uint32_t) noexcept override {events.emplace_back("F");}
    std::uint32_t tick_ms() const noexcept override {events.emplace_back("T");return tick;}
    void erase_persisted(const PersistedEffect& effect) noexcept override {replica.erase(effect.skill_id,effect.level);}
    void persist_applied(const PersistedEffect& effect) noexcept override {
        replica.apply(effect);events.push_back("R"+std::to_string(effect.skill_id)+","+std::to_string(effect.level)+","+std::to_string(effect.remaining));
    }
    void send_self(const net::Packet& data) noexcept override {events.push_back(packet('S',data));}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void send_party(const net::Packet& data) noexcept override {events.push_back(packet('P',data));}
    void notification(char kind) {events.push_back(std::string(1,kind)+std::to_string(status.skills.hp_ability_applied?1:0)+","+std::to_string(status.skills.hp_ability_previous));}
    void maximum_changed(EffectStatus&,std::uint8_t resource) override {require(resource==0,"Unexpected passive resource");notification('M');}
    void speed_changed(EffectStatus&) override {notification('V');}
    void unexpected() {throw std::runtime_error("Unexpected low-HP world operation");}
    void shape_changed(EffectStatus&) override {unexpected();}
    void run_mode_changed(EffectStatus&,bool) override {unexpected();}
    void stop_attack(EffectStatus&) override {unexpected();}
    void stop_target_mob(EffectStatus&,std::uint32_t) override {unexpected();}
    void taunt(EffectStatus&,std::uint16_t,float) override {unexpected();}
    void detect_hidden(EffectStatus&,const SkillDefinition&) override {unexpected();}
    void rebuild_attack(EffectStatus&,bool) override {unexpected();}
    void user_shape_changed(EffectStatus&) override {unexpected();}
    void replace_attack_elements(EffectStatus&,std::uint8_t) override {unexpected();}
};
struct Scene final:ActorScene ,public ActorEffectDatabase {
    std::vector<net::Packet> effect_database_packets;
    void send_effect_database(const net::Packet& packet) noexcept override {effect_database_packets.push_back(packet);}
    std::uint32_t tick=180000;std::vector<std::string> events;
    void send_self(const net::Packet& data) noexcept override {events.push_back(packet('S',data));}
    void send_nearby(const net::Packet& data) override {events.push_back(packet('V',data));}
    void send_cell_view(const net::Packet& data) override {events.push_back(packet('C',data));}
    std::optional<bool> party_kind() const noexcept override {return false;}
    void send_party(const net::Packet& data) noexcept override {events.push_back(packet('P',data));}
    void reset_damage() override {events.emplace_back("D");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return 1000;}
    void release_effect_id(std::uint32_t id) noexcept override {events.push_back("F"+std::to_string(id));}
    std::uint32_t tick_ms() const noexcept override {return tick;}
    void unexpected() {throw std::runtime_error("Unexpected actor scene operation");}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected low-HP oracle file");std::ifstream input(argv[1]);require(input.good(),"Cannot open low-HP oracle");
        std::string line;unsigned int count=0,activated=0,allocation_failures=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);PlayerResources current{};EffectStatus status;auto& skills=status.skills;auto& equipment=skills.equipment;
            unsigned int limit=0,id=0,level=0,applied=0,available=0,allocate=0;int party=-1;std::uint32_t tick=0,keep=0;
            std::array<std::uint16_t,3> values{};
            row>>current.hp>>equipment.base.maximum.hp>>limit>>skills.hp_ability_previous>>tick>>id>>level>>applied>>available>>allocate>>party;
            for(auto& value:values) row>>value;row>>keep;require(static_cast<bool>(row),"Malformed low-HP input");
            skills.hp_ability_id=static_cast<std::uint16_t>(id);skills.hp_ability_level=static_cast<std::uint8_t>(level);skills.hp_ability_applied=applied!=0;
            equipment.base.attributes.strength=100;equipment.attack_speed=2;
            const auto registry=definitions(limit,values,keep,available!=0);EffectCycle cycle(9001);Harness world(status);
            world.tick=tick;world.allocate=allocate!=0;if(party>=0) world.party=party!=0;
            const bool result=cycle.activate_hp(status,current,registry,world,world);
            unsigned int final_applied=0,entries=0;std::uint32_t previous=0,maximum=0,end=0;std::int32_t strength=0,speed=0;std::size_t events=0;
            row>>final_applied>>previous>>strength>>maximum>>speed>>entries>>end>>events;
            const auto context=" at low-HP vector "+std::to_string(count);
            require(skills.hp_ability_applied==(final_applied!=0) && skills.hp_ability_previous==previous,"Passive flags/cooldown differ"+context);
            require(equipment.base.attributes.strength==strength && equipment.base.maximum.hp==maximum && equipment.attack_speed==speed,"Passive ability mutation differs"+context);
            require(cycle.size()==entries && result==(entries!=0),"Passive insertion differs"+context);
            if(result) {
                const std::array<ActiveEffectPacketEntry,1> expected{{{1000,static_cast<std::uint16_t>(id),static_cast<std::uint8_t>(level),end,false}}};
                require(cycle.initial_packets(tick)==character_effects(expected,tick),"Passive runtime lifetime differs"+context);
                require(world.replica.snapshot().size()==1 && world.replica.snapshot()[0].remaining==keep,"Passive persistence duration differs"+context);++activated;
            } else require(world.replica.snapshot().empty(),"Skipped passive changed replica"+context);
            require(world.events.size()==events,"Passive event count differs"+context);
            for(const auto& event:world.events) {std::string original;row>>original;require(event==original,"Passive event differs"+context+": "+event+" versus "+original);if(event=="A" && !allocate) ++allocation_failures;}
            require(static_cast<bool>(row),"Truncated low-HP vector");std::string extra;require(!(row>>extra),"Trailing low-HP vector data");++count;
        }
        require(count==1354 && activated>100 && allocation_failures>0,"Low-HP coverage changed");
        const auto registry=definitions(25,{10,100,1},120);EffectStatus status;status.skills.hp_ability_id=200;status.skills.hp_ability_level=1;status.skills.equipment.base.maximum.hp=1000;
        Scene scene;const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;
        Actor actor(9001,status,{1000,0,0},{{200,1,31},{200,1,40}},scene,{items,enchant,mobs});
        require(!actor.damage(3,123,750,registry) && actor.resources().hp==250 && actor.status().skills.hp_ability_applied,"Actor damage did not activate passive");
        require(actor.damage_ledger().maximum()==std::optional<DamageRecord>{{3,123,750}},"Actor damage did not own attribution");
        require(actor.status().skills.equipment.base.maximum.hp==1100 && actor.status().skills.equipment.base.attributes.strength==10 && actor.status().skills.equipment.attack_speed==1,"Actor passive did not apply abilities");
        require(actor.replica().snapshot().size()==2 && actor.replica().snapshot()[0].remaining==120 && actor.replica().snapshot()[1].remaining==40,"Actor passive did not update only first matching replica");
        require(scene.events==std::vector<std::string>{"P010c2923000000fa00","S0d05e8030000c80001","P040c29230000c80001","P020c29230000004c04"},"Actor damage/passive notification ordering changed");
        actor.effects().expire(209999,actor.status(),{},items,actor,actor);require(actor.effects().size()==1,"Passive expired before fixed deadline");
        actor.effects().expire(210000,actor.status(),{},items,actor,actor);
        require(actor.effects().size()==0 && !actor.status().skills.hp_ability_applied && actor.status().skills.hp_ability_previous==180000 && actor.status().skills.equipment.base.maximum.hp==1000 && actor.replica().snapshot().size()==1 && actor.replica().snapshot()[0].remaining==40,"Actor passive expiry did not reverse state and retain duplicate replica");
        scene.tick=359999;require(!actor.check_hp_ability(registry),"Passive reactivated before three-minute cooldown");
        scene.tick=360000;require(actor.check_hp_ability(registry),"Passive failed to reactivate at inclusive cooldown");
        actor.heal({10000,0,0});require(actor.damage_ledger().entries().empty(),"Full healing retained actor damage attribution");
        actor.clear_effects_on_death(5);
        require(actor.effects().size()==0 && actor.replica().snapshot().empty() && !actor.status().skills.hp_ability_applied && actor.status().skills.hp_ability_previous==360000 && actor.status().skills.equipment.base.maximum.hp==1000 && actor.resources().hp==1000,"Actor death cleanup did not remove activated passive, preserve cooldown or clamp resources");
        EffectStatus protected_status;protected_status.skills.equipment.base.maximum.hp=1000;protected_status.skills.equipment.damage_reduction=0x12345678;
        Scene protected_scene;Actor protected_actor(9001,protected_status,{1000,0,0},{},protected_scene,{items,enchant,mobs});
        require(!protected_actor.damage(3,456,1000,registry) && protected_actor.resources().hp==1 && protected_actor.damage_ledger().maximum()->amount==999,"Guard protection did not leave one HP or record adjusted damage");
        require(protected_scene.events==std::vector<std::string>{"S25057856","P010c29230000000100"},"Guard protection notification ordering changed");
        protected_scene.events.clear();protected_actor.attack_state().mode=1;
        require(!protected_actor.damage(3,456,1000,registry) && protected_actor.resources().hp==1 && protected_scene.events.empty(),"Dead actor processed scalar damage");
        protected_actor.attack_state().mode=0;
        require(protected_actor.damage(3,456,1,registry) && protected_actor.resources().hp==0 && protected_actor.attack_state().mode==0 && protected_actor.damage_ledger().maximum()->amount==1000,"Lethal actor damage changed life mode or attribution incorrectly");
        std::cout<<count<<" original low-HP activations and native actor damage/expiry/persistence integration passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
