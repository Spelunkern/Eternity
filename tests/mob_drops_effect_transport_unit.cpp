#include "eternity/game/actor.hpp"
#include "eternity/game/skill_save.hpp"
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void check(bool ok,const std::string& why) {if(!ok) throw std::runtime_error(why);}
std::string hex(const net::Packet& packet) {std::string result;for(auto byte:packet) {result+="0123456789abcdef"[byte>>4];result+="0123456789abcdef"[byte&15];}return result;}
struct Scene:ActorScene {
    std::vector<std::string> events;std::vector<net::Packet> clients;unsigned freed=0,next=100;
    std::function<void(const net::Packet&)> on_client;
    void send_self(const net::Packet& packet) noexcept override {if(on_client) on_client(packet);clients.push_back(packet);events.push_back("S"+hex(packet));}
    void send_nearby(const net::Packet& packet) override {events.push_back("N"+hex(packet));}
    void send_cell_view(const net::Packet& packet) override {events.push_back("V"+hex(packet));}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::runtime_error("Unexpected damage reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return next++;}
    void release_effect_id(std::uint32_t id) noexcept override {++freed;events.push_back("F"+std::to_string(id));}
    std::uint32_t tick_ms() const noexcept override {return 1000;}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected target action");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected taunt");}
    ZoneUsers* user_zone() override {return nullptr;}
    UserShapeSocial user_shape_social() override {return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected periodic damage");}
};
struct Database:ActorEffectDatabase {
    ActorEffectReplicaDelivery* receiver=nullptr;Scene& scene;std::vector<net::Packet> packets;std::function<void(const net::Packet&)> on_send;
    explicit Database(Scene& value):scene(value) {}
    void send_effect_database(const net::Packet& packet) noexcept override {if(on_send) on_send(packet);if(receiver) receiver->send_effect_database(packet);packets.push_back(packet);scene.events.push_back("B"+hex(packet));}
};
struct ComposedScene final:Scene,ActorEffectDatabase {
    std::vector<net::Packet> packets;
    void send_effect_database(const net::Packet& packet) noexcept override {packets.push_back(packet);events.push_back("B"+hex(packet));}
};
struct Diagnostics:ActorEffectDatabaseDiagnostics {
    unsigned rejected=0,failed=0;
    void rejected_effect_database(const net::Packet&,InventoryReplicaResult) noexcept override {++rejected;}
    void failed_effect_database(const net::Packet&) noexcept override {++failed;}
};
SkillDefinitions restoration_skills() {
    storage::ResultSet table;table.columns={"SkillID","SkillLevel","TypeDetail","TypeEffect","Weapon1","Weapon2","Weaponvalue","Bag","AbilityType1","AbilityValue1","AbilityType2","AbilityValue2","AbilityType3","AbilityValue3","AttrType","DefenceType","DefenceValue","ApplyRange","Arrow","ChangeType","ChangeLevel","FixRange","KeepTime","TimeHealHP","TimeHealSP","TimeHealMP","LimitHP"};
    for(unsigned id:{400,401,402,403,124}) {
        storage::Row row(table.columns.size(),storage::Cell{"0"});row[0]=std::to_string(id);row[1]=id==124?"2":"1";row[2]=id==400?"74":"82";row[3]=id==403?"2":"4";row[21]=(id==401||id==402)?"1":"0";row[22]="1";table.rows.push_back(row);
    }
    return SkillDefinitions::load(table);
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original SendSkillRemove fixture");std::ifstream input(argv[1]);check(input.good(),"Missing original send fixture");
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;unsigned cases=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned scenario,skill,level,accepted;std::uint32_t account,instance;std::string expected;
        row>>scenario>>account>>skill>>level>>instance>>accepted>>expected;check(!row.fail(),"Bad original send fixture");
        Scene scene;Database database(scene);SkillDefinition definition;definition.id=static_cast<std::uint16_t>(skill);definition.level=static_cast<std::uint8_t>(level);definition.detail=66;definition.effect=3;
        SkillReplicaPool pool;std::uint16_t points=0;SkillReplica backend(account,points,pool);
        const std::vector<PersistedEffect> saved{{definition.id,definition.level,1},{definition.id,definition.level,9}};
        check(backend.load({},saved),"Actual DB-agent cache load failed");Diagnostics diagnostics;std::recursive_mutex mutex;
        ActorEffectReplicaDelivery delivery(backend,mutex,diagnostics);database.receiver=&delivery;
        Actor actor(0x0badf00d,{}, {},{{definition.id,definition.level,1},{definition.id,definition.level,9}},scene,{items,enchant,mobs},database);actor.account_id()=account;actor.status().action_block_b=2;
        actor.effects().append(instance,{{definition.id,definition.level,1},0,1,false},definition);
        database.on_send=[&](const net::Packet&) {check(!actor.effects().contains(instance) && actor.replica().snapshot().size()==1 && actor.replica().snapshot()[0].remaining==9 && scene.freed==0 && scene.clients.empty() && actor.status().action_block_b==2,"DB callback did not observe unlink/first replica erase before client/reversal/free");};
        scene.on_client=[&](const net::Packet& packet) {if(packet.size()==6 && packet[0]==14 && packet[1]==5) check(database.packets.size()==1 && scene.freed==0 && actor.status().action_block_b==2,"Client preceded DB or followed release/reversal");};
        actor.remove_sleep();
        std::string wire;for(const auto& event:scene.events) if(event.starts_with("B") || event.starts_with("S0e05")) {if(!wire.empty()) wire+=',';wire+=event;}
        check(wire==expected && database.packets.size()==1 && scene.freed==1 && scene.events.size()>=3 && scene.events[2]=="F"+std::to_string(instance),"Original immediate DB/client/free ordering or packet differs");
        check(backend.effects().size()==1 && backend.effects()[0].delay==9 && pool.statistics().stranded==1 && pool.statistics().available==0 && diagnostics.rejected==0 && diagnostics.failed==0,"Actual DB-agent first-match unlink/pool stranding differs");
        const auto plan=skill_save_plan(backend,actor.id());check(plan.size()==2 && plan[0].procedure=="usp_Save_Char_ApplySkill_Del_E" && plan[1].procedure=="usp_Save_Char_ApplySkill_Add_E2" && std::get<std::int64_t>(plan[1].parameters[3])==9,"Immediate removal changed later save plan");
        check(actor.status().action_block_b==0 && actor.effects().size()==0,"Actual Actor sleep cleanup incomplete");(void)accepted;++cases;
    }
    check(cases==512,"Incomplete packet boundary coverage");
    // Missing required transport must not look like a successful action.
    {
        Scene scene;SkillDefinition skill;skill.id=1;skill.level=1;skill.detail=88;skill.effect=4;
        Actor actor(1,{}, {},{{1,1,30}},scene,{items,enchant,mobs});actor.status().shape=13;actor.effects().append(77,{{1,1,30},0,1,false},skill);
        check(!actor.effect_database_bound(),"Legacy constructor invented a transport");bool rejected=false;
        try {actor.cancel_shape_on_attack();}catch(const std::logic_error&) {rejected=true;}
        check(rejected && actor.effects().contains(77) && actor.replica().snapshot().size()==1 && scene.clients.empty(),"Unbound action mutated state");
    }
    // Restoration sends only the original filtered/expired removals. No client
    // removal for skipped definitions or restoration cleanup; shutdown only frees.
    {
        ComposedScene scene;const auto skills=restoration_skills();const std::vector<PersistedEffect> rows{{400,1,30},{401,1,1},{402,1,100},{403,1,30},{124,2,30},{999,1,30}};
        {
            Actor actor(1,{}, {},rows,scene,{items,enchant,mobs});actor.account_id()=0x12345678;
            check(actor.effect_database_bound(),"Scene/DB adapter did not bind before restoration");
            actor.effects().restore(rows,skills,18,10,1000,actor);
            check(scene.packets==std::vector<net::Packet>{effect_database_removed_packet(0x12345678,400,1),effect_database_removed_packet(0x12345678,401,1)} && scene.clients.empty() && actor.replica().snapshot().size()==4 && actor.effects().size()==1,"Restore filtering/expiry emits incorrect removals");
        }
        check(scene.packets.size()==2 && scene.freed==1,"Shutdown generated DB removal or lost runtime release");
    }
    // Death and duel cleanup have original immediate DB delivery but no 050e.
    for(bool death:{false,true}) {
        ComposedScene scene;SkillDefinition skill;skill.id=321;skill.level=1;skill.effect=3;skill.detail=66;
        Actor actor(1,{}, {},{{321,1,30}},scene,{items,enchant,mobs});actor.account_id()=9;actor.effects().append(99,{{321,1,30},0,1000,false},skill);
        if(death) actor.clear_effects_on_death(0);else actor.clear_duel_debuffs();
        check(scene.packets==std::vector<net::Packet>{effect_database_removed_packet(9,321,1)} && std::none_of(scene.clients.begin(),scene.clients.end(),[](const auto& packet){return packet.size()==6 && packet[0]==14 && packet[1]==5;}) && scene.freed==1,"Death/duel immediate DB path invented client removal");
    }
    {
        SkillReplicaPool pool;std::uint16_t points=0;SkillReplica receiver(9,points,pool);const std::vector<PersistedEffect> saved{{1,1,30}};check(receiver.load({},saved),"Receiver load failed");
        Diagnostics diagnostics;std::recursive_mutex mutex;ActorEffectReplicaDelivery delivery(receiver,mutex,diagnostics);
        delivery.send_effect_database(effect_database_removed_packet(10,1,1));delivery.send_effect_database({1,9});
        check(diagnostics.rejected==2 && diagnostics.failed==0 && receiver.effects().size()==1,"Wrong owner/malformed delivery silently succeeded");
    }
    EffectReplica cache({{1,1,30},{1,1,40}});check(cache.erase(1,1) && cache.snapshot().size()==1 && cache.snapshot()[0].remaining==40,"Save/replica first-match behavior changed");
    std::cout<<cases<<" complete original DB/client packets through actual Actor, first-match Actor/DB-agent replica, actual cache pool stranding/save plan and reversal/release ordering; required binding, restore/death/duel/shutdown distinctions passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
