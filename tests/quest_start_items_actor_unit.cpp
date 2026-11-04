#include "quest_start_items_actor_fixture.hpp"
#include "eternity/game/quest_start.hpp"
#include <fstream>
#include <iostream>
using namespace quest_actor_test;
namespace {
struct Sequence final:QuestStartWorld {
    QuestStartItemsActor& adapter;unsigned phase=0;
    explicit Sequence(QuestStartItemsActor& a):adapter(a) {}
    void prepare_start_log(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override {check(phase++==0,"Prelude capture order");}
    void send_self(const std::array<std::uint8_t,8>&) override {check(phase++==1,"Prelude client order");}
    void log_start(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override {check(phase++==2,"Prelude log order");}
    bool give_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) override {check(phase==3,"Grant before start log");return adapter.grant(type,id,count);}
    std::optional<QuestStartNpc> find_npc(std::uint32_t) override {throw std::runtime_error("Unexpected NPC lookup");}
    bool active(std::uint16_t) override {throw std::runtime_error("Unexpected active lookup");}bool finished(std::uint16_t) override {throw std::runtime_error("Unexpected finished lookup");}
    bool find_item(std::uint8_t,std::uint8_t,bool) override {throw std::runtime_error("Unexpected requirement");}std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected quest clock");}bool add(const QuestStartRecord&) override {throw std::runtime_error("Unexpected quest add");}
};
struct Root {
    Scene scene;EnchantValues enchant;MobAttackTemplates mobs;Clock clock;
    std::shared_ptr<ItemDefinitions> definitions=catalog(5,{1,2,3});std::shared_ptr<GroundItemPool> pool=std::make_shared<GroundItemPool>();std::shared_ptr<ItemIds> ids=std::make_shared<ItemIds>(7,clock);
    std::shared_ptr<Actor> actor=std::make_shared<Actor>(9001,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{*definitions,enchant,mobs});
    std::int32_t& bags=actor->status().skills.max_bags;Ports ports{bags,77};
    Root() {actor->account_id()=77;bags=1;}
    QuestStartItemsActorOwners owners() {return {definitions,pool,ids,actor};}
};
void failures() {
    Root root;QuestStartItemsActor adapter(root.owners(),root.ports);
    root.ports.fail_calendar=true;bool failed=false;try {(void)adapter.grant(5,1,1);}catch(const std::runtime_error&) {failed=true;}
    check(failed && root.actor->inventory().empty() && root.pool->statistics().available==1 && root.clock.ticks==0,"Pre-claim failure stranded item or advanced shared UID");
    root.ports.fail_calendar=false;check(adapter.grant(5,1,1),"Actual pool reuse after failure failed");check(root.pool->statistics().fresh_attempts==1,"Failure recovery allocated a competing item");
    root.ports.fail_database=true;failed=false;const auto clients=root.scene.packets.size();try {(void)adapter.grant(5,2,2);}catch(const std::runtime_error&) {failed=true;}
    check(failed && root.actor->inventory().size()==2 && root.scene.packets.size()==clients,"Post-claim transport failure rolled back ownership or sent client");
    root.ports.fail_database=false;check(adapter.grant(5,3,3),"Frame was not restored after transport failure");
    auto reused=root.actor->retained_inventory_item(1,1);check(reused && reused->pool==root.pool.get(),"Wrong retained pool");
    Root nested;QuestStartItemsActor recursive(nested.owners(),nested.ports);bool recursed=false;
    nested.ports.database_callback=[&](Actor&,const net::Packet& p) {if(word(p,0,2)==0x701 && !recursed) {recursed=true;check(recursive.grant(5,2,2),"Nested grant failed");}};
    check(recursive.grant(5,1,1) && nested.actor->inventory().size()==2 && nested.ports.cache.inventory().size()==2,"Recursive actual owner grant lost a frame");
    check(nested.scene.packets.size()==2 && nested.scene.packets[0][5]==2 && nested.scene.packets[1][5]==1,"Nested DB/client order changed");
}
void leases() {
    Scene scene;EnchantValues enchant;MobAttackTemplates mobs;Clock clock;
    auto definitions=catalog(5,{1});auto pool=std::make_shared<GroundItemPool>();auto ids=std::make_shared<ItemIds>(3,clock);
    auto actor=std::make_shared<Actor>(9001,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{*definitions,enchant,mobs});actor->account_id()=77;actor->status().skills.max_bags=1;Ports ports(actor->status().skills.max_bags,77);
    std::weak_ptr<Actor> weak_actor=actor;std::weak_ptr<GroundItemPool> weak_pool=pool;std::weak_ptr<const ItemDefinitions> weak_definitions=definitions;std::weak_ptr<ItemIds> weak_ids=ids;
    {
        QuestStartItemsActor adapter({definitions,pool,ids,actor},ports);actor.reset();definitions.reset();pool.reset();ids.reset();
        ports.database_callback=[&](Actor& actual,const net::Packet&) {check(weak_actor.lock().get()==&actual && !weak_pool.expired() && !weak_definitions.expired() && !weak_ids.expired(),"Actual owner lease expired in delivery callback");};
        check(adapter.grant(5,1,1),"Leased grant failed");
    }
    check(weak_actor.expired() && weak_pool.expired() && weak_definitions.expired() && weak_ids.expired(),"Adapter retained root owners after destruction");
}
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected actual owner sequence fixture");std::ifstream input(argv[1]);check(input.good(),"Missing fixture");unsigned cases=0,total=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;unsigned id,mask,lookup,server,expected_attempts;std::int32_t bags,new_bags;std::uint32_t billing,tick,baseline,final_bags;std::uint64_t before,after;std::string record,holes,grid,state,audit,packets;
        std::istringstream row(line);row>>id>>record>>bags>>holes>>mask>>lookup>>new_bags>>billing>>server>>tick>>baseline>>before>>after>>final_bags>>grid>>state>>expected_attempts>>audit>>packets;check(!row.fail(),"Malformed Actor fixture");
        std::vector<std::uint64_t> ids;for(unsigned i=0;i<3;++i) if(lookup&(1u<<i)) ids.push_back(i+1);auto definitions=catalog(5,ids);
        Scene scene;EnchantValues enchant;MobAttackTemplates mobs;Clock clock;clock.tick=tick;
        unsigned attempts=0;auto pool=std::make_shared<GroundItemPool>(1,[&]()->std::unique_ptr<GroundItem> {const bool success=(mask&(1u<<attempts++))!=0;if(!success) return {};auto p=std::make_unique<GroundItem>();p->item.uid=0xface;p->craft_storage.fill(0xa5);return p;});
        auto uid=std::make_shared<ItemIds>(static_cast<std::uint8_t>(server),clock);uid->set_start_id(baseline);check(uid->next()==before,"External original UID before grants mismatch "+std::to_string(id));
        const auto empty=numbers(holes);std::vector<PlayerItem> occupied;
        for(unsigned cell=0;cell<48;++cell) if(std::find(empty.begin(),empty.end(),cell)==empty.end()) {PlayerItem p;p.bag=static_cast<std::uint8_t>(1+cell/24);p.slot=static_cast<std::uint16_t>(cell%24);occupied.push_back(p);}
        auto actor=std::make_shared<Actor>(9001,EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scene,ActorEquipmentRules{*definitions,enchant,mobs},occupied);actor->account_id()=billing;actor->status().skills.max_bags=bags;
        Ports ports(actor->status().skills.max_bags,billing);ports.new_bags=new_bags;std::vector<GroundItem*> stored;
        ports.database_callback=[&](Actor& actual,const net::Packet& p) {auto held=actual.retained_inventory_item(p[18],p[19]);check(held && held->pool==pool.get(),"Adapter used a second pool");stored.push_back(held->item);};
        {
            QuestStartItemsActor adapter({definitions,pool,uid,actor},ports);Sequence start(adapter);const auto qbytes=decode(record);QuestStartRecord q;check(qbytes.size()==q.size(),"Invalid quest record");std::copy(qbytes.begin(),qbytes.end(),q.begin());quest_start(q,{},start);
            check(uid->next()==after,"External original UID after grants mismatch "+std::to_string(id));
            net::Packet occupancy;for(unsigned cell=0;cell<48;++cell) {auto held=actor->retained_inventory_item(static_cast<std::uint8_t>(1+cell/24),static_cast<std::uint16_t>(cell%24));occupancy.push_back(held?2:actor->inventory_slot_empty(static_cast<std::uint8_t>(1+cell/24),static_cast<std::uint16_t>(cell%24))?0:1);}
            check(occupancy==decode(grid),"Actual Actor occupancy mismatch "+std::to_string(id));check(snapshot(stored)==decode(state),"Actual retained item/UID payload mismatch "+std::to_string(id));check(ports.packets==decode(packets),"Actual owner transport bytes/order mismatch "+std::to_string(id));
            check(attempts==expected_attempts && pool->statistics().owned==stored.size(),"Raw Actor claim allocated another item");check(ports.cache.inventory().size()==stored.size() && scene.packets.size()==stored.size(),"Actual persistent replica/client delivery mismatch");check(static_cast<std::uint32_t>(actor->status().skills.max_bags)==final_bags,"Live bag update mismatch");
        }
        total+=static_cast<unsigned>(stored.size());actor.reset();check(pool->statistics().available==pool->statistics().owned,"Actual Actor destruction did not return shared pool items");++cases;
    }
    failures();leases();check(cases==240 && total>200,"Insufficient real owner sequence coverage");std::cout<<cases<<" original sequences / "<<total<<" grants through concrete Actor + live shared UID; external UID continuity, leases and failure recovery passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
