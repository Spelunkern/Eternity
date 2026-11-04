#include "eternity/game/vertical_persistence.hpp"
#include "eternity/game/vertical_entry.hpp"
#include "eternity/game/creation.hpp"
#include <iostream>

using namespace eternity;
using namespace eternity::game;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void put(net::Packet& p,std::uint32_t value,unsigned n) {for(unsigned i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
net::Packet packet(std::uint16_t op) {net::Packet p;put(p,op,2);put(p,9951,4);return p;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2 && std::string_view(argv[1]).starts_with("Eternity_Test_Creation_"),"Disposable creation catalog required");
        const std::string catalog=argv[1],prefix="["+catalog+"].dbo.";
        const auto config=storage::SqlServerConfig::from_environment();storage::SqlServer db(config),aux(config);
        const auto definitions=Definitions::load(db);const auto rules=CreationRules::load(definitions);
        const auto date=std::chrono::sys_days{std::chrono::year{2024}/1/1};Characters characters(db,1,date,catalog);
        require(characters.save_country(9951,0,true),"Cannot create fixture account");
        auto draft=rules.prepare({0,0,0,0,0,0,0,0,"PersistHero"},0);draft.items.clear();
        const auto created=characters.create(9951,"persistuser",draft,date);require(created.result==0,"Cannot create fixture character");
        const auto cid=std::to_string(created.character_id);
        db.query("INSERT "+prefix+"Users_Master(UserUID,UserID,Pw,Admin,AdminLevel,UseQueue,Status,[Leave],UserType,Enpassword,Point) VALUES(9951,'persistuser','testpass',0,0,0,0,0,'N','',500); "
            "INSERT "+prefix+"Users_Product(UserUID,Slot,ItemID,ItemCount,ProductCode,OrderNumber,VerifyCode,BuyDate) VALUES(9951,4,5001,3,'persist_product',99,100,'20240101'); "
            "INSERT "+prefix+"PointGiftNotify(UserUID,ProductCode,SendCharName,RecvDate) VALUES(9951,'persist_gift','GiftSender','20240101');");
        Players players(db,1,catalog);PointItemStore products(db,1,catalog);InventoryReplicaPool pool;
        auto roster=characters.load(9951);CharacterSelectionState selection{9951,0,true};
        std::optional<VerticalPersistenceSelection> selected;
        VerticalEntryPersistencePorts entry{[&](VerticalPersistenceSelection value){selected=std::move(value);},
            []{throw std::runtime_error("Retained selection unavailable");},
            [](std::uint32_t){throw std::runtime_error("Retained selection closed");}};
        net::Packet request{0x04,0x01};put(request,created.character_id,4);
        require(vertical_entry_select_persisted(selection,request,roster,players,products,"persistuser",2,pool,entry)==CharacterSelectionResult::requested && !selection.enabled && selected.has_value(),"Retained 0104 selection failed");
        auto snapshot=std::make_optional(std::move(selected->snapshot));auto cache=std::move(selected->replica);
        require(snapshot->catalogs.at("GiftNotifications").rows.size()==1,"Selection did not retain consumed gift");
        db.query("INSERT "+prefix+"PointGiftNotify(UserUID,ProductCode,SendCharName,RecvDate) VALUES(9951,'after_select','GiftSender','20240101');");
        require(vertical_entry_select_persisted(selection,request,roster,players,products,"persistuser",2,pool,entry)==CharacterSelectionResult::ignored,"Consumed selection latch loaded again");
        require(snapshot->catalogs.at("GiftNotifications").rows.size()==1 && cache->points().products()[4].count==3,"Snapshot or product load changed");
        require(db.query("SELECT COUNT(*) FROM "+prefix+"PointGiftNotify WHERE UserUID=9951;")[0].rows[0][0]==storage::Cell{"1"},"Replica construction consumed a second notification");
        auto money=packet(0x603);put(money,1234,4);auto resources=packet(0x606);put(resources,101,2);put(resources,102,2);put(resources,103,2);
        auto skill=packet(0x801);put(skill,4,2);put(skill,10,2);skill.push_back(1);
        auto quest=packet(0xa01);put(quest,5,2);put(quest,10,2);
        for(const auto* p:{&money,&resources,&skill,&quest}) require(cache->apply(*p)==InventoryReplicaResult::applied,"DB mutation rejected");
        const auto path=std::filesystem::temp_directory_path()/("eternity-vertical-persistence-"+catalog+".log");
        struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove(path,e);}} cleanup{path};
        CharacterRecoveryFile recovery(path);std::recursive_mutex mutex;CharacterSaveStore store(db,aux,1,mutex,recovery,catalog);
        std::vector<std::string> events;CharacterSaveCallbacks callbacks{[&]{events.push_back("friends");},[&]{events.push_back("blocks");},[&]{events.push_back("queue");},[&]{events.push_back("exit");}};
        std::vector<std::string> errors;auto saved=store.save_exit(*cache,false,"persistuser",callbacks,[&](std::string_view message){errors.emplace_back(message);});
        require(saved.legacy_success && saved.attempts==1 && events==std::vector<std::string>{"friends","blocks","exit"} && errors.empty(),"Disconnect save failed");
        require(!db.transaction_active() && !db.character_write_context_active() && !aux.character_write_context_active(),"Save leaked transaction state");
        cache.reset();snapshot.reset();
        auto reloaded=players.load(9951,created.character_id,"persistuser");require(reloaded.has_value(),"Reconnect selection failed");
        require(reloaded->catalogs.at("GiftNotifications").rows.size()==1,"Reconnect did not consume precisely the later gift");
        auto next=vertical_persistence_restore_with_products(*reloaded,products,2,pool);
        const auto core=next->core();require(core.money==1234 && core.character.hp==101 && core.character.mp==102 && core.character.sp==103 && core.skill_points==4,"Core mutation did not survive reconnect");
        require(next->skills().learned().size()==1 && next->skills().learned()[0].id==10 && next->quests().active().size()==1 && next->quests().active()[0].id==5 && next->points().products()[4].count==3,"Skill, quest or account product did not survive reconnect");
        require(!std::filesystem::exists(path),"Successful save unexpectedly wrote recovery file");
        std::cout<<"Vertical persistence SQL selection, mutation, exit save and reload passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
