#include "eternity/game/vertical_persistence.hpp"
#include "eternity/game/vertical_entry.hpp"
#include "eternity/game/creation.hpp"
#include "eternity/game/roster_world_handoff.hpp"
#include "eternity/game/vertical_selection_sql_loader.hpp"
#include "eternity/game/vertical_owned_selection.hpp"
#include "eternity/game/vertical_world_session_sql_save.hpp"
#include <asio/thread_pool.hpp>
#include <future>
#include <thread>
#include <iostream>

using namespace eternity;
using namespace eternity::game;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void put(net::Packet& p,std::uint32_t value,unsigned n) {for(unsigned i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
net::Packet packet(std::uint16_t op) {net::Packet p;put(p,op,2);put(p,9951,4);return p;}
struct SelectionOwner {std::shared_ptr<VerticalOwnedSelection<SelectionOwner>> selection;};
struct SelectionApplication final:net::WorldApplication {
    std::shared_ptr<SelectionOwner> owner;VerticalSessionRegistry<SelectionOwner>& registry;
    SelectionApplication(std::shared_ptr<SelectionOwner> value,VerticalSessionRegistry<SelectionOwner>& owners):owner(std::move(value)),registry(owners){}
    void open() override {}
    void receive(const net::Packet& packet) override {(void)owner->selection->receive(packet);}
    void close() override {auto retained=owner;auto* owners=&registry;owner->selection->cancel_and_drain([retained,owners]{require(owners->release(9951,*retained),"Selection fence prevented release");});}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2 && std::string_view(argv[1]).starts_with("Eternity_Test_Creation_"),"Disposable creation catalog required");
        const std::string catalog=argv[1],prefix="["+catalog+"].dbo.";
        const auto config=storage::SqlServerConfig::from_environment();storage::SqlServer db(config);
        const auto definitions=Definitions::load(db);const auto rules=CreationRules::load(definitions);
        const auto date=std::chrono::sys_days{std::chrono::year{2024}/1/1};Characters characters(db,1,date,catalog);
        require(characters.save_country(9951,0,true),"Cannot create fixture account");
        auto draft=rules.prepare({0,0,0,0,0,0,0,0,"PersistHero"},0);draft.items.clear();
        const auto created=characters.create(9951,"persistuser",draft,date);require(created.result==0,"Cannot create fixture character");
        const auto cid=std::to_string(created.character_id);
        db.query("INSERT "+prefix+"Users_Master(UserUID,UserID,Pw,Admin,AdminLevel,UseQueue,Status,[Leave],UserType,Enpassword,Point) VALUES(9951,'persistuser','testpass',0,0,0,0,0,'N','',500); "
            "INSERT "+prefix+"Users_Product(UserUID,Slot,ItemID,ItemCount,ProductCode,OrderNumber,VerifyCode,BuyDate) VALUES(9951,4,5001,3,'persist_product',99,100,'20240101'); "
            "INSERT "+prefix+"PointGiftNotify(UserUID,ProductCode,SendCharName,RecvDate) VALUES(9951,'persist_gift','GiftSender','20240101');");
        // Two valid rows share a slot. The original retains both and marks the
        // relocated row for the next character save, without an immediate write.
        db.query("INSERT "+prefix+"CharItems(CharID,ItemID,ItemUID,Type,TypeID,Bag,Slot,Quality,Gem1,Gem2,Gem3,Gem4,Gem5,Gem6,Craftname,[Count],MakeTime,MakeType) VALUES("+cid+",1001,9951001,1,1,1,0,100,0,0,0,0,0,0,'',1,'20240101','N'),("+cid+",1001,9951002,1,1,1,0,100,0,0,0,0,0,0,'',1,'20240101','N');");
        Players players(db,1,catalog);PointItemStore products(db,1,catalog);auto pool=std::make_shared<InventoryReplicaPool>();
        auto loader=std::make_shared<VerticalSelectionSqlLoader>(config,1,catalog,"persistuser",2,pool);
        asio::io_context world;auto guard=asio::make_work_guard(world);auto strand=asio::make_strand(world);asio::thread_pool database_worker(1);std::jthread runner([&]{world.run();});
        std::optional<VerticalPersistenceSelection> selected;
        std::promise<void> loaded;auto loaded_result=loaded.get_future();auto owners=std::make_shared<VerticalSessionRegistry<SelectionOwner>>();std::shared_ptr<SelectionOwner> selected_owner;
        struct StopRuntime {asio::io_context& world;decltype(guard)& work;asio::thread_pool& worker;std::jthread& runner;~StopRuntime(){worker.join();work.reset();world.stop();if(runner.joinable())runner.join();}} stop_runtime{world,guard,database_worker,runner};
        net::Packet request{0x04,0x01};put(request,created.character_id,4);
        std::vector<net::Packet> roster_packets;net::WorldChannel channel;channel.send=[&](net::Packet p){roster_packets.push_back(std::move(p));};channel.disconnect=[] {throw std::runtime_error("Unexpected roster disconnect");};
        auto handoff_factory=roster_world_serialized_factory(strand,[&](const session::LoginSession& owner){require(owner.user_id==9951,"Wrong roster owner");return characters.load(owner.user_id);},
            [](const session::LoginSession&,CharacterRoster&,const net::Packet&){return CharacterReply{{},true};},
            [&](const session::LoginSession& owner,net::WorldChannel,CharacterRoster transferred){
                require(owner.user_id==9951 && transferred.slots[0] && transferred.slots[0]->id==created.character_id,"Roster handoff identity differs");
                selected_owner=std::make_shared<SelectionOwner>();owners->reserve(owner.user_id,selected_owner);
                selected_owner->selection=std::make_shared<VerticalOwnedSelection<SelectionOwner>>(strand,database_worker.get_executor(),*owners,selected_owner,CharacterSelectionState{9951,transferred.country,true},std::move(transferred),VerticalSelectionSqlLoader::bind(loader),
                    [&](SelectionOwner&,VerticalPersistenceSelection value){require(owners->pending_database_work(9951)==1,"SQL delivery lost its owner fence");selected=std::move(value);loaded.set_value();},
                    [&](SelectionOwner&){loaded.set_exception(std::make_exception_ptr(std::runtime_error("Selection unavailable")));},
                    [&](SelectionOwner&,std::uint32_t){loaded.set_exception(std::make_exception_ptr(std::runtime_error("Selection closed")));},
                    [&](SelectionOwner&,std::exception_ptr error){loaded.set_exception(error);});
                return std::make_unique<SelectionApplication>(selected_owner,*owners);
            });
        session::LoginSession account{};account.user_id=9951;auto handoff=handoff_factory(account,channel);handoff->open();handoff->receive(request);
        loaded_result.get();require(roster_packets.size()==6 && selected.has_value(),"Roster handoff did not reach asynchronous SQL selection");
        auto snapshot=std::make_optional(std::move(selected->snapshot));auto cache=std::move(selected->replica);
        require(snapshot->catalogs.at("GiftNotifications").rows.size()==1,"Selection did not retain consumed gift");
        const auto loaded_items=cache->inventory().inventory();require(loaded_items.size()==2 && loaded_items[0].save_type+loaded_items[1].save_type==2 && db.query("SELECT COUNT(*) FROM "+prefix+"CharItems WHERE CharID="+cid+" AND Bag=1 AND Slot=0;")[0].rows[0][0]==storage::Cell{"2"},"Duplicate placement wrote SQL early or lost its save marker");
        db.query("INSERT "+prefix+"PointGiftNotify(UserUID,ProductCode,SendCharName,RecvDate) VALUES(9951,'after_select','GiftSender','20240101');");
        std::promise<void> ignored;auto ignored_result=ignored.get_future();asio::post(strand,[&]{try{require(selected_owner->selection->receive(request)==CharacterSelectionResult::ignored,"Consumed selection latch loaded again");ignored.set_value();}catch(...){ignored.set_exception(std::current_exception());}});ignored_result.get();
        require(snapshot->catalogs.at("GiftNotifications").rows.size()==1 && cache->points().products()[4].count==3,"Snapshot or product load changed");
        require(db.query("SELECT COUNT(*) FROM "+prefix+"PointGiftNotify WHERE UserUID=9951;")[0].rows[0][0]==storage::Cell{"1"},"Replica construction consumed a second notification");
        auto money=packet(0x603);put(money,1234,4);auto resources=packet(0x606);put(resources,101,2);put(resources,102,2);put(resources,103,2);
        auto skill=packet(0x801);put(skill,4,2);put(skill,10,2);skill.push_back(1);
        auto quest=packet(0xa01);put(quest,5,2);put(quest,10,2);
        for(const auto* p:{&money,&resources,&skill,&quest}) require(cache->apply(*p)==InventoryReplicaResult::applied,"DB mutation rejected");
        const auto path=std::filesystem::temp_directory_path()/("eternity-vertical-persistence-"+catalog+".log");
        struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code e;std::filesystem::remove(path,e);}} cleanup{path};
        auto sql_save=VerticalWorldSessionSqlSave::open(config,1,path,catalog);std::weak_ptr<VerticalWorldSessionSqlSave> save_lifetime=sql_save;
        auto store=sql_save->store();auto mutex=sql_save->mutex();sql_save.reset();require(!save_lifetime.expired(),"Save bundle was not retained by aliases");
        std::vector<std::string> events;CharacterSaveCallbacks callbacks{[&]{events.push_back("friends");},[&]{events.push_back("blocks");},[&]{events.push_back("queue");},[&]{events.push_back("exit");}};
        std::vector<std::string> errors;auto saved=store->save_exit(*cache,false,"persistuser",callbacks,[&](std::string_view message){errors.emplace_back(message);});
        require(saved.legacy_success && saved.attempts==1 && events==std::vector<std::string>{"friends","blocks","exit"} && errors.empty(),"Disconnect save failed");
        require(save_lifetime.lock()->connections_clean(),"Save leaked transaction state");
        require(db.query("SELECT COUNT(DISTINCT Slot) FROM "+prefix+"CharItems WHERE CharID="+cid+" AND Bag=1;")[0].rows[0][0]==storage::Cell{"2"},"Relocated duplicate was not saved to its new slot");
        cache.reset();snapshot.reset();
        auto reloaded=players.load(9951,created.character_id,"persistuser");require(reloaded.has_value(),"Reconnect selection failed");
        require(reloaded->catalogs.at("GiftNotifications").rows.size()==1,"Reconnect did not consume precisely the later gift");
        auto next=vertical_persistence_restore_with_products(*reloaded,products,2,*pool);
        const auto core=next->core();require(core.money==1234 && core.character.hp==101 && core.character.mp==102 && core.character.sp==103 && core.skill_points==4,"Core mutation did not survive reconnect");
        require(next->skills().learned().size()==1 && next->skills().learned()[0].id==10 && next->quests().active().size()==1 && next->quests().active()[0].id==5 && next->points().products()[4].count==3,"Skill, quest or account product did not survive reconnect");
        require(!std::filesystem::exists(path),"Successful save unexpectedly wrote recovery file");
        store.reset();mutex.reset();require(save_lifetime.expired(),"Save bundle survived its aliases");
        handoff->close();require(owners->size()==0,"Selection owner was not released");database_worker.join();selected_owner.reset();guard.reset();runner.join();
        std::cout<<"Vertical persistence SQL selection, mutation, exit save and reload passed\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
