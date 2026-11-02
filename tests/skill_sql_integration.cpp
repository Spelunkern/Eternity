#include "eternity/game/skill_save.hpp"
#include "eternity/game/character_core_save.hpp"
#include "eternity/game/inventory_save.hpp"
#include "eternity/game/creation.hpp"
#include <iostream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void put(net::Packet& p,std::uint32_t value,unsigned int bytes) {for(unsigned int i=0;i<bytes;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
net::Packet packet(std::uint16_t op) {net::Packet p;put(p,op,2);put(p,9201,4);return p;}
net::Packet add(std::uint16_t id,std::uint8_t level,std::uint16_t points) {auto p=packet(0x801);put(p,points,2);put(p,id,2);p.push_back(level);return p;}
net::Packet effect(std::uint16_t op,std::uint16_t id,std::uint8_t level,std::uint32_t delay=0) {auto p=packet(op);put(p,id,2);p.push_back(level);if(op!=0x902) put(p,delay,4);return p;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2 && std::string_view(argv[1]).starts_with("Eternity_Test_Creation_"),"Skill SQL test requires disposable catalog");const std::string catalog=argv[1],prefix="["+catalog+"].dbo.";
        storage::SqlServer db(storage::SqlServerConfig::from_environment());const auto definitions=Definitions::load(db);const auto rules=CreationRules::load(definitions);const auto time=std::chrono::sys_days{std::chrono::year{2024}/1/1};Characters characters(db,1,time,catalog);
        require(characters.save_country(9201,0,true) && characters.save_country(9202,0,true),"Cannot initialize skill fixture factions");auto draft=rules.prepare({0,0,0,0,0,0,0,0,"SkillHero"},0);draft.items.clear();const auto created=characters.create(9201,"skill_user",draft,time);draft.request.name="SkillOther";const auto other=characters.create(9202,"skill_other",draft,time);require(created.result==0 && other.result==0,"Cannot create skill fixture characters");const auto cid=std::to_string(created.character_id),other_id=std::to_string(other.character_id);
        db.query("EXEC "+prefix+"usp_Save_Char_Skill_Add_E "+cid+",10,1,12,9; EXEC "+prefix+"usp_Save_Char_Skill_Add_E "+cid+",11,2,22,17; EXEC "+prefix+"usp_Save_Char_ApplySkill_Add_E2 "+cid+",50,1,30; EXEC "+prefix+"usp_Save_Char_ApplySkill_Add_E2 "+other_id+",59,1,60; EXEC "+prefix+"usp_Save_Char_Skill_Add_E "+other_id+",99,1,77,33;");
        InventoryReplicaPool item_pool;SkillReplicaPool skill_pool;PlayerCore core;core.user_id=9201;core.character=draft.character;core.character.id=created.character_id;core.skill_points=20;CharacterReplica cache(core,item_pool,&skill_pool);
        const std::vector<PlayerSkill> learned={{10,9,1,0,12},{11,17,2,1,22}};const std::vector<PersistedEffect> effects={{50,1,30},{51,2,0}};require(cache.skills().load(learned,effects),"Cannot load original skill cache");
        CharacterCoreStore core_store(db,catalog);InventoryStore item_store(db,1,catalog);SkillStore skill_store(db,catalog);const auto apply=[&](const net::Packet& p){require(cache.apply(p)==InventoryReplicaResult::applied,"Skill fixture command failed");};
        bool rejected=false;try{skill_store.save_in_transaction(cache.skills(),created.character_id);}catch(const std::logic_error&){rejected=true;}require(rejected,"Skill phase ran outside complete character transaction");
        const auto save=[&](){core_store.save_in_transaction(cache);item_store.save_in_transaction(cache.inventory(),created.character_id);skill_store.save_in_transaction(cache.skills(),created.character_id);};
        const auto ack=[&](){cache.core_save_success();cache.inventory().inventory_save_success();cache.skills().save_success();};
        const auto read_skills=[&](){return db.query("SELECT SkillID,SkillLevel,Number,[Delay] FROM "+prefix+"CharSkills WHERE CharID="+cid+" ORDER BY SkillID;")[0].rows;};
        const auto read_effects=[&](){return db.query("SELECT SkillID,SkillLevel,LeftResetTime FROM "+prefix+"CharApplySkills WHERE CharID="+cid+" ORDER BY SkillID;")[0].rows;};
        apply(add(10,3,15));apply(add(12,1,11));auto delay=packet(0x803);delay.push_back(1);put(delay,123,2);apply(delay);auto batch=packet(0x804);batch.insert(batch.end(),{3,255,0xff,0xff,0,7,0,2,9,0});apply(batch);
        apply(effect(0x901,50,1,40));apply(effect(0x903,99,1,77));auto applied_batch=packet(0x904);applied_batch.push_back(2);put(applied_batch,50,2);applied_batch.push_back(1);put(applied_batch,0,4);put(applied_batch,51,2);applied_batch.push_back(2);put(applied_batch,UINT32_MAX,4);apply(applied_batch);apply(effect(0x901,52,2,0x80000000U));
        PlayerItem item;item.uid=0x7777000000000400ULL;item.id=5001;item.type=5;item.type_id=1;item.bag=1;item.count=10;item.made_at=pack_world_time(time);item.make_type="U";require(cache.inventory().load(item,false,1),"Cannot seed composite inventory");
        db.begin();save();require(cache.skills().saved_count()==2 && cache.skills().learned().size()==3,"SQL phase acknowledged learned skills before commit");db.commit();ack();
        const auto first_skills=read_skills(),first_effects=read_effects();const std::vector<storage::Row> expected_skills={{storage::Cell{"10"},storage::Cell{"3"},storage::Cell{"12"},storage::Cell{"7"}},{storage::Cell{"11"},storage::Cell{"2"},storage::Cell{"22"},storage::Cell{"123"}},{storage::Cell{"12"},storage::Cell{"1"},storage::Cell{"2"},storage::Cell{"9"}}};
        require(first_skills==expected_skills,"Saved/new skill partition or renumbering changed");require(first_effects==std::vector<storage::Row>{{storage::Cell{"51"},storage::Cell{"2"},storage::Cell{"-1"}},{storage::Cell{"52"},storage::Cell{"2"},storage::Cell{"-2147483648"}}},"Effect replacement did not skip zero timers or preserve DWORD bits");
        auto rows=db.query("SELECT SkillPoint FROM "+prefix+"Chars WHERE CharID="+cid+"; SELECT [Count] FROM "+prefix+"CharItems WHERE CharID="+cid+";");require(rows[0].rows[0][0]==storage::Cell{"11"} && rows[1].rows[0][0]==storage::Cell{"10"},"Skills did not share core points/inventory transaction");
        // Reset is a completed DB job; 0802 itself remains a no-op.
        auto noop=packet(0x802);put(noop,0,2);apply(noop);require(cache.core().skill_points==11 && cache.skills().learned().size()==3,"0802 unexpectedly reset skills");
        auto reset=cache.skills().finish_reset(99);auto expected_packet=packet(0x50a);require(reset==expected_packet,"Non-event reset completion packet changed");require(cache.core().skill_points==99 && cache.skills().saved_count()==3 && cache.skills().reset_pending(),"Reset lost its saved-count boundary/shared points");
        apply(add(20,1,90));apply(add(21,1,80));auto count=packet(0x707);count.insert(count.end(),{1,0,4});apply(count);apply(effect(0x901,32760,1,61));
        db.begin();bool failed=false;try{save();}catch(const std::runtime_error&){failed=true;}require(failed && db.transaction_active(),"Rejected later effect insert lost caller transaction");db.rollback();
        require(read_skills()==first_skills && read_effects()==first_effects,"Effect failure retained partial learned reset or effect replacement");rows=db.query("SELECT SkillPoint FROM "+prefix+"Chars WHERE CharID="+cid+"; SELECT [Count] FROM "+prefix+"CharItems WHERE CharID="+cid+";");require(rows[0].rows[0][0]==storage::Cell{"11"} && rows[1].rows[0][0]==storage::Cell{"10"},"Later skill failure retained partial core/inventory writes");require(cache.skills().reset_pending() && cache.skills().saved_count()==3 && cache.skills().learned().size()==2 && cache.core().skill_points==80 && cache.inventory().inventory()[0].save_type==2,"Rollback cleared pending reset/core/inventory changes");
        apply(effect(0x902,32760,1));db.begin();save();db.commit();ack();const auto reset_skills=read_skills();require(reset_skills==std::vector<storage::Row>{{storage::Cell{"20"},storage::Cell{"1"},storage::Cell{"0"},storage::Cell{"0"}},{storage::Cell{"21"},storage::Cell{"1"},storage::Cell{"1"},storage::Cell{"0"}}} && read_effects()==first_effects,"Reset retry did not delete all/reinsert with new numbers");require(!cache.skills().reset_pending() && cache.skills().saved_count()==2,"Commit did not acknowledge reset");
        rows=db.query("SELECT SkillPoint FROM "+prefix+"Chars WHERE CharID="+cid+"; SELECT [Count] FROM "+prefix+"CharItems WHERE CharID="+cid+"; SELECT SkillID,Number,[Delay] FROM "+prefix+"CharSkills WHERE CharID="+other_id+"; SELECT SkillID,LeftResetTime FROM "+prefix+"CharApplySkills WHERE CharID="+other_id+";");require(rows[0].rows[0][0]==storage::Cell{"80"} && rows[1].rows[0][0]==storage::Cell{"4"} && rows[2].rows[0]==storage::Row{storage::Cell{"99"},storage::Cell{"77"},storage::Cell{"33"}} && rows[3].rows[0]==storage::Row{storage::Cell{"59"},storage::Cell{"60"}},"Composite reset failed or changed another character");
        // WORD delays/IDs are printed positive by the original, so SQL SMALLINT rejects 32768/65535.
        delay=packet(0x803);delay.push_back(0);put(delay,32768,2);apply(delay);db.begin();failed=false;try{save();}catch(const std::runtime_error&){failed=true;}db.rollback();require(failed && read_skills()==reset_skills,"Out-of-range learned delay wrapped instead of failing");
        delay=packet(0x803);delay.push_back(0);put(delay,0,2);apply(delay);apply(add(65535,1,60));db.begin();failed=false;try{save();}catch(const std::runtime_error&){failed=true;}db.rollback();require(failed && read_skills()==reset_skills && read_effects()==first_effects,"Out-of-range learned ID became a negative SQL identifier");
        std::cout<<"Original skill SQL passed: saved/new partition, timers, shared points, reset, composite rollback/retry and SMALLINT failures\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
