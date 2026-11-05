#include "eternity/game/actor.hpp"
#include "eternity/game/inventory.hpp"
#include "eternity/game/inventory_replica.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/item_ids.hpp"
#include "eternity/game/vertical_session_character.hpp"
#include "eternity/game/character_replica_delivery.hpp"
#include "equipment_fixture.hpp"
#include <algorithm>
#include <iostream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
std::vector<std::uint8_t> unhex(const std::string& text) {
    require(text.size()%2==0,"Invalid hex size");std::vector<std::uint8_t> result;
    for(std::size_t i=0;i<text.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));
    return result;
}
struct World final:ActorScene,ItemDropWorld,ItemPickupWorld,ZoneItemServices {
    InventoryReplica& replica;GroundItemPool& pool;ZoneItems* zone=nullptr;CharacterReplicaDelivery* delivery=nullptr;std::uint64_t next_uid=3;std::vector<net::Packet> client,view,database,logs;std::string* exchange_trace=nullptr;char exchange_tag='A';std::uint32_t test_tick=100;bool trace_tick=false;std::vector<std::uint8_t>* mutate_offer=nullptr;std::array<std::uint8_t,4> mutate_value{};
    World(InventoryReplica& cache,GroundItemPool& items):replica(cache),pool(items) {}
    void unexpected() {throw std::runtime_error("Unexpected inventory flow operation");}
    void send_self(const net::Packet& packet) noexcept override {
        client.push_back(packet);
        if(mutate_offer) {for(unsigned i=0;i<mutate_value.size() && i+2<mutate_offer->size();++i) (*mutate_offer)[i+2]=mutate_value[i];mutate_offer=nullptr;}
        if(exchange_trace) {static constexpr char digits[]="0123456789abcdef";*exchange_trace+=",";*exchange_trace+=exchange_tag;for(auto byte:packet) {*exchange_trace+=digits[byte>>4];*exchange_trace+=digits[byte&15];}}
    }
    void send_nearby(const net::Packet& packet) override {view.push_back(packet);}void send_cell_view(const net::Packet& packet) override {view.push_back(packet);}
    std::optional<bool> party_kind() const noexcept override {return std::nullopt;}void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {if(trace_tick && exchange_trace)*exchange_trace+=",T";return test_tick;}std::uint32_t tick_ms() override {return test_tick;}
    void stop_target_mob(std::uint32_t) override {unexpected();}void taunt(std::uint16_t,float) override {unexpected();}ZoneUsers* user_zone() override {unexpected();return nullptr;}UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
    std::uint32_t lock_inventory_party(Actor&) override {return 0;}void unlock_inventory_party(std::uint32_t) noexcept override {std::terminate();}
    std::optional<bool> inventory_party_kind(const Actor&) override {return std::nullopt;}void send_inventory_party(Actor&,const net::Packet&) override {unexpected();}
    void send_inventory_database(const net::Packet& packet) override {database.push_back(packet);require((delivery?delivery->apply(packet):replica.apply(packet))==InventoryReplicaResult::applied,"Actor inventory message failed in DB-agent cache");}
    void send_money_database(const net::Packet& packet) override {database.push_back(packet);require(delivery?delivery->apply(packet)==InventoryReplicaResult::applied:replica.apply_money(packet),"Actor money message failed in shared DB-agent cache");}
    GameLogSubject money_log_subject(const Actor& actor) override {GameLogSubject subject;subject.account_id=actor.account_id();subject.character_id=actor.id();subject.character_name="Flow";subject.account_name="flow";subject.position=drop_position(actor);return subject;}
    std::chrono::sys_seconds money_local_calendar() override {return std::chrono::sys_days{std::chrono::year{2026}/11/2};}void send_money_log(const net::Packet& packet) override {logs.push_back(packet);}
    std::string inventory_item_name(const ItemDefinition&) override {return "Flow item";}std::string inventory_drop_user_name(Actor&,std::uint32_t,bool) override {unexpected();return {};}
    bool return_inventory_item(Actor&,GroundItem& item) override {return zone->enter(item,1);}
    Actor* find_exchange_user(Actor&,std::uint32_t) override {unexpected();return nullptr;}void negative_craft_reduction(const PlayerItem&,std::int32_t) override {unexpected();}
    UserPosition drop_position(const Actor&) override {return {10,0,10};}std::uint64_t next_drop_uid() override {return next_uid++;}std::uint32_t drop_make_time() override {return pack_world_time(money_local_calendar());}
    bool enter_dropped_item(Actor&,GroundItem& item) override {return zone->enter(item,1);}
    std::uint32_t pick_tick_ms() override {return 100;}std::uint32_t pick_party_id(Actor&) override {return 0;}UserPosition pick_position(const Actor& actor) override {return drop_position(actor);}bool pick_quest_active(Actor&,std::uint16_t) override {unexpected();return false;}
    void send_cell_view(CellCoordinates,const net::Packet& packet) override {view.push_back(packet);}void release(GroundItem& item) override {pool.release(item);}
};
struct ExchangeMoneyWorld final:MoneyWorld {
    InventoryReplica& first;InventoryReplica& second;std::vector<std::string> events;Actor* mutate_actor=nullptr;Actor* mutate_peer=nullptr;std::uint32_t* mutate_field=nullptr;
    ExchangeMoneyWorld(InventoryReplica& a,InventoryReplica& b):first(a),second(b) {}
    static std::string encode(const net::Packet& packet) {static constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:packet) {result+=digits[byte>>4];result+=digits[byte&15];}return result;}
    void send_money_database(const net::Packet& packet) override {
        require(packet.size()==10,"Bad exchange DB packet");const auto account=std::uint32_t(packet[2])|(std::uint32_t(packet[3])<<8)|(std::uint32_t(packet[4])<<16)|(std::uint32_t(packet[5])<<24);
        require(account==12345?first.apply_money(packet):account==12346 && second.apply_money(packet),"Exchange money did not reach its retained replica");events.push_back("D"+encode(packet));
    }
    GameLogSubject money_log_subject(const Actor& actor) override {
        GameLogSubject subject;
        if(actor.id()==1001) {subject.account_id=12345;subject.account_name="account-a";subject.character_id=1001;subject.character_name="Actor";subject.level=30;subject.experience=123;subject.map=7;subject.position={10,-1,20};}
        else {require(actor.id()==1002,"Unexpected exchange log subject");subject.account_id=12346;subject.account_name="account-b";subject.character_id=1002;subject.character_name="Peer";subject.level=31;subject.experience=456;subject.map=8;subject.position={30,-2,40};}return subject;
    }
    std::chrono::sys_seconds money_local_calendar() override {events.push_back("T");return std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59};}
    void send_money_log(const net::Packet& packet) override {
        events.push_back("G"+encode(packet));if(mutate_actor) {mutate_actor->exchange().money=7;mutate_peer->exchange().money=9;*mutate_field=0xdeadbeef;mutate_actor=nullptr;}
    }
};
}
#include "eternity/game/vertical_inventory.hpp"
#include <bit>
#include <fstream>
#include <sstream>

namespace {
struct Actions final:VerticalInventoryActions {
    std::vector<std::uint8_t>& packet;ZoneItems& zone;std::uint32_t replacement;unsigned mutation;bool flip;std::string events;
    Actions(std::vector<std::uint8_t>& p,ZoneItems& z,std::uint32_t r,unsigned m,bool f):packet(p),zone(z),replacement(r),mutation(m),flip(f) {}
    bool pickup_checks() override {require(events=="C,S","Pickup checks sampled before callbacks");return true;}
    void change() {for(unsigned i=0;i<4;++i) packet[2+i]=static_cast<std::uint8_t>(replacement>>(i*8));}
    void cancel_action_exchange(Actor& actor) override {require(zone.size()==1,"Pickup before action cancellation");events="C";if(mutation==1) change();if(flip) actor.attack_state().mode=1;}
    void end_shop(Actor&) override {require(events=="C" && zone.size()==1,"Shop ended out of order");events+=",S";if(mutation==2) change();}
};
struct SplitDelivery final:VerticalInventoryExchangeWorld {
    InventoryReplica& source;InventoryReplica& recipient;std::string& events;
    SplitDelivery(InventoryReplica& s,InventoryReplica& r,std::string& e):source(s),recipient(r),events(e) {}
    static std::string encode(const net::Packet& packet) {static constexpr char d[]="0123456789abcdef";std::string out;for(auto b:packet) {out+=d[b>>4];out+=d[b&15];}return out;}
    void send_inventory_database(const net::Packet& packet) override {const auto account=std::uint32_t(packet[2])|(std::uint32_t(packet[3])<<8)|(std::uint32_t(packet[4])<<16)|(std::uint32_t(packet[5])<<24);require((account==12345?source:recipient).apply(packet)==InventoryReplicaResult::applied,"Split DB packet was not applied to actual replica");events+=",D"+encode(packet);}
    void send_exchange_insertion(const net::Packet& packet) override {const auto account=std::uint32_t(packet[2])|(std::uint32_t(packet[3])<<8)|(std::uint32_t(packet[4])<<16)|(std::uint32_t(packet[5])<<24);const auto rows=(account==12345?source:recipient).inventory();if(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==packet[18] && row.item.slot==packet[19];})) throw VerticalInventoryDestinationOccupied();send_inventory_database(packet);}
    void validate_exchange_destination(std::uint32_t account,std::uint8_t bag,std::uint8_t slot) override {const auto rows=(account==12345?source:recipient).inventory();if(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==bag && row.item.slot==slot;})) throw VerticalInventoryDestinationOccupied();}
    void validate_exchange_source(std::uint32_t account,std::uint8_t bag,std::uint8_t slot,std::uint64_t uid,std::uint8_t count) override {const auto rows=(account==12345?source:recipient).inventory();const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==bag && row.item.slot==slot;});if(found==rows.end()) throw VerticalInventoryRemovalError();if(found->item.uid!=uid || found->item.count!=count) throw VerticalInventorySourceMismatch();}
    void send_exchange_removal(const net::Packet& packet,std::uint64_t uid,std::uint8_t count) override {const auto account=std::uint32_t(packet[2])|(std::uint32_t(packet[3])<<8)|(std::uint32_t(packet[4])<<16)|(std::uint32_t(packet[5])<<24);const auto rows=(account==12345?source:recipient).inventory();const auto found=std::find_if(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==packet[6] && row.item.slot==packet[7];});if(found==rows.end()) throw VerticalInventoryRemovalError();if(found->item.uid!=uid || found->item.count!=count) throw VerticalInventorySourceMismatch();send_inventory_database(packet);}
    void send_money_log(const net::Packet& packet) override {events+=",G"+encode(packet);}
    GameLogSubject money_log_subject(const Actor& actor) override {GameLogSubject s;if(actor.id()==1001) {s.account_id=12345;s.account_name="account-a";s.character_id=1001;s.character_name="Actor";s.level=30;s.experience=123;s.map=7;s.position={10,-1,20};}else {s.account_id=12346;s.account_name="account-b";s.character_id=1002;s.character_name="Peer";s.level=31;s.experience=456;s.map=8;s.position={30,-2,40};}return s;}
    std::chrono::sys_seconds money_local_calendar() override {return std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59};}
    std::string inventory_item_name(const ItemDefinition&) override {return "Strange item";}
    void send_money_database(const net::Packet& packet) override {const auto account=std::uint32_t(packet[2])|(std::uint32_t(packet[3])<<8)|(std::uint32_t(packet[4])<<16)|(std::uint32_t(packet[5])<<24);require((account==12345?source:recipient).apply_money(packet),"Settlement money did not reach real replica");events+=",M"+encode(packet);}
    std::uint32_t lock_inventory_party(Actor&) override {throw std::runtime_error("Unexpected party lock");}void unlock_inventory_party(std::uint32_t) noexcept override {std::terminate();}
    std::optional<bool> inventory_party_kind(const Actor&) override {throw std::runtime_error("Unexpected party query");}void send_inventory_party(Actor&,const net::Packet&) override {throw std::runtime_error("Unexpected party send");}
    std::string inventory_drop_user_name(Actor&,std::uint32_t,bool) override {throw std::runtime_error("Unexpected drop name");}bool return_inventory_item(Actor&,GroundItem&) override {throw std::runtime_error("Unexpected item return");}
};
struct NoWholeExchange final:VerticalExchangeWholeItemTransfer {
    unsigned validations=0,transfers=0;
    void validate_whole_exchange_source(Actor&,Actor&,const ExchangeItemReference&,GroundItem&,GroundItemPool&) override {++validations;throw std::runtime_error("Fixture cannot transfer a whole exchange item");}
    void transfer_whole_exchange_item(Actor&,Actor&,const VerticalExchangeItemSelection&) override {++transfers;throw std::runtime_error("Fixture unexpectedly required a real whole-item transfer");}
};
struct EmptyExchangeTransfer final:VerticalExchangeItemTransfer {
    Actor& actor;Actor& peer;std::string& events;
    EmptyExchangeTransfer(Actor& a,Actor& p,std::string& e):actor(a),peer(p),events(e) {}
    void validate_exchange_sources(Actor& a,Actor& b) override {require(&a==&actor && &b==&peer && std::all_of(a.exchange().items.begin(),a.exchange().items.end(),[](const auto& item){return item.bag==0;}) && std::all_of(b.exchange().items.begin(),b.exchange().items.end(),[](const auto& item){return item.bag==0;}),"Empty transfer validator received items");}
    void transfer_exchange_items(Actor& recipient,Actor& source) override {
        require((&recipient==&actor && &source==&peer) || (&recipient==&peer && &source==&actor),"Unexpected transfer actors");
        require(std::all_of(source.exchange().items.begin(),source.exchange().items.end(),[](const auto& item){return item.bag==0;}),"Empty transfer fixture received items");
        events+=(&recipient==&actor?",TAB":",TBA");
    }
};
struct RejectingSettlement final:VerticalExchangeSettlement {
    unsigned calls=0;
    void validate_before_ready(Actor& actor,Actor& peer) override {require(!vertical_inventory_exchange_money_valid(peer,actor,peer.exchange().money),"Fixture requires a real preflight rejection");}
    void settle_exchange(Actor& actor,Actor& peer) override {
        ++calls;require(actor.exchange().mode==4 && peer.exchange().mode==4,"Settlement called before ready transition");
        // This fixture uses real preflight and only supports a real rejection;
        // it never substitutes a successful item or money transfer.
        require(vertical_inventory_exchange_preflight(actor,peer)==VerticalExchangePreflight::peer_money,"Unexpected settlement requires a real transfer owner");
    }
};
struct ExchangeActions final:VerticalInventoryActions {
    std::string& events;explicit ExchangeActions(std::string& e):events(e) {}
    bool pickup_checks() override {throw std::runtime_error("Exchange sampled pickup setting");}
    void cancel_action_exchange(Actor&) override {throw std::runtime_error("Exchange cancelled action");}
    void end_shop(Actor&) override {events="E";}
};
struct MutatingControlActions final:VerticalInventoryActions {
    std::vector<std::uint8_t>& packet;explicit MutatingControlActions(std::vector<std::uint8_t>& p):packet(p) {}
    bool pickup_checks() override {throw std::runtime_error("Control sampled pickup setting");}
    void cancel_action_exchange(Actor&) override {throw std::runtime_error("Control cancelled action");}
    void end_shop(Actor&) override {packet[2]=0;}
};
struct DropActions final:VerticalInventoryActions {
    std::vector<std::uint8_t>& packet;unsigned mutation;std::array<std::uint8_t,3> replacement;std::string events;
    DropActions(std::vector<std::uint8_t>& p,unsigned m,std::array<std::uint8_t,3> r):packet(p),mutation(m),replacement(r) {}
    bool pickup_checks() override {throw std::runtime_error("Drop sampled pickup setting");}
    void change() {std::copy(replacement.begin(),replacement.end(),packet.begin()+2);}
    void cancel_action_exchange(Actor&) override {events="C";if(mutation==1) change();}
    void end_shop(Actor&) override {require(events=="C","Drop callback order changed");events+=",S";if(mutation==2) change();}
};
}
int main(int argc,char** argv) {try {
    require(argc==25,"Expected original request oracles");std::ifstream input(argv[1]);require(input.good(),"Cannot open request oracle");
    const ItemDefinitions definitions;const EnchantValues enchant;const MobAttackTemplates mobs;const MapGeometry geometry(160);const ItemQuestLinks links;
    unsigned count=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);std::uint32_t mode,id,replacement;unsigned mutation,flip;std::string expected;row>>mode>>id>>mutation>>replacement>>flip>>expected;require(!row.fail(),"Invalid request oracle");
        GroundItemPool pool;InventoryReplicaPool cached;InventoryReplica replica(12345,0,cached);World world(replica,pool);ZoneItems zone(geometry,world);world.zone=&zone;
        Actor actor(9001,{}, {},{},world,{definitions,enchant,mobs});actor.account_id()=12345;actor.attack_state().mode=std::bit_cast<std::int32_t>(mode);
        auto* item=pool.allocate();require(item,"Item allocation failed");item->id=mutation?replacement:id;item->item.type=26;item->item.count=1;item->money=1;item->position={10,0,10};require(zone.enter(*item,1),"Cannot enter item");
        std::vector<std::uint8_t> packet{5,2,0,0,0,0};for(unsigned i=0;i<4;++i) packet[2+i]=static_cast<std::uint8_t>(id>>(8*i));Actions actions(packet,zone,replacement,mutation,flip);
        const auto result=vertical_inventory_request(actor,packet,zone,links,world,pool,actions);
        std::string actual=actions.events;
        if(result.request==VerticalInventoryRequest::pickup) {
            require(result.receipt==InventoryReceipt::money && zone.size()==0 && actor.money()==1 && replica.money()==1,"Real Actor pickup and replica failed");
            actual+=",P"+std::to_string(mutation?replacement:id);
        } else {require(result.request==VerticalInventoryRequest::ignored && !result.receipt && zone.size()==1,"Gate failed");require(zone.leave(*item),"Cannot detach gated item");pool.release(*item);}
        require((actual.empty()?"-":actual)==expected,"Original request trace differs");++count;
    }
    require(count==512,"Missing original request cases");
    GroundItemPool pool;InventoryReplicaPool cached;InventoryReplica replica(12345,0,cached);World world(replica,pool);ZoneItems zone(geometry,world);world.zone=&zone;Actor actor(9001,{}, {},{},world,{definitions,enchant,mobs});std::vector<std::uint8_t> packet{5,2,0,0,0,0};Actions actions(packet,zone,0,0,false);
    for(unsigned length=0;length<6;++length) {const auto result=vertical_inventory_request(actor,std::span(packet).first(length),zone,links,world,pool,actions);require(result.request==VerticalInventoryRequest::malformed && !result.receipt && actions.events.empty(),"Truncated request caused effects");}
    packet[0]=6;require(vertical_inventory_request(actor,packet,zone,links,world,pool,actions).request==VerticalInventoryRequest::unhandled && actions.events.empty(),"Unrelated opcode caused effects");
    auto* other=pool.allocate();other->id=123;other->position={10,0,10};require(zone.enter(*other,1),"Cannot enter rejection item");
    packet={5,2,124,0,0,0};actor.account_id()=12345;
    actor.status().skills.max_bags=6;
    const auto missing=vertical_inventory_guarded_pickup_request(actor,packet,zone,links,world,pool,actions);
    require(missing.request==VerticalInventoryRequest::pickup && missing.receipt==InventoryReceipt::rejected && actions.events=="C,S" && zone.size()==1,"Missing-item rejection or prefix ordering changed");
    require(!world.client.empty() && world.client.back()==inventory_failure_packet(3),"Missing-item failure transport changed");
    require(zone.leave(*other),"Cannot detach rejection item");pool.release(*other);
    {
        GroundItemPool item_pool;InventoryReplicaPool cache_pool;PlayerSnapshot selected;
        selected.core.user_id=12345;selected.core.character.id=9001;selected.core.money=10;
        auto cache=std::make_unique<CharacterReplica>(selected.core,cache_pool);
        World retained_world(cache->inventory(),item_pool);ZoneItems retained_zone(geometry,retained_world);retained_world.zone=&retained_zone;
        const StatusRules statuses;const SkillDefinitions skills;const auto quests=QuestDefinitions::decode(net::Packet{0,0,0,0});
        const ActorLoadRules rules{statuses,definitions,enchant,skills,quests,mobs};
        ActorLoadOptions options;options.inventory_pool=&item_pool;
        VerticalSessionCharacter session({std::move(selected),std::move(cache)},rules,options,retained_world);
        std::recursive_mutex cache_mutex;CharacterReplicaDelivery delivery(session.replica(),cache_mutex);retained_world.delivery=&delivery;
        require(delivery.apply(net::Packet{3,6,0,0,0,0,5,0,0,0})==InventoryReplicaResult::rejected && session.replica().core().money==10,"Foreign account changed the retained cache");
        const net::Packet unknown{0xff,0xff,0x39,0x30,0,0};
        require(delivery.apply(unknown)==InventoryReplicaResult::unsupported && delivery.apply(net::Packet{3,6})==InventoryReplicaResult::rejected,"Delivery hid unsupported or malformed status");
        PlayerItem cached_item;cached_item.bag=1;cached_item.slot=0;cached_item.type=5;cached_item.type_id=1;cached_item.count=2;
        require(session.replica().inventory().load(cached_item),"Cannot prepare collision test");
        net::Packet collision(57,0);collision[0]=1;collision[1]=7;collision[2]=0x39;collision[3]=0x30;collision[18]=1;collision[20]=6;collision[21]=1;collision[22]=1;
        {std::lock_guard held(cache_mutex);require(delivery.apply(collision)==InventoryReplicaResult::disconnect,"Delivery hid inventory collision disconnect");}
        const auto unchanged=session.replica().inventory().inventory();require(unchanged.size()==1 && unchanged[0].item.type==5 && unchanged[0].item.count==2,"Collision changed retained inventory");
        Actor& loaded=*session.pipeline().result().actor;loaded.status().skills.max_bags=6;
        auto* money=item_pool.allocate();require(money,"Cannot allocate session pickup");money->id=456;money->item.type=26;money->item.count=1;money->money=7;money->position={10,0,10};require(retained_zone.enter(*money,1),"Cannot place session pickup");
        std::vector<std::uint8_t> request{5,2,0xc8,1,0,0};Actions session_actions(request,retained_zone,0,0,false);
        const auto result=vertical_inventory_guarded_pickup_request(loaded,request,retained_zone,links,retained_world,item_pool,session_actions);
        require(result.receipt==InventoryReceipt::money && loaded.money()==17 && session.replica().core().money==17,"Session actor and retained DB replica diverged during pickup");
    }
    {
        std::ifstream drops(argv[2]);require(drops.good(),"Cannot open drop request oracle");unsigned cases=0;
        for(std::string line;std::getline(drops,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);std::uint32_t mode;std::int32_t maximum;unsigned bag,slot,amount,mutation,rb,rs,rc;std::string expected;
            row>>mode>>maximum>>bag>>slot>>amount>>mutation>>rb>>rs>>rc>>expected;require(!row.fail(),"Invalid drop request oracle");
            GroundItemPool item_pool;InventoryReplicaPool cache_pool;InventoryReplica cache(12345,0,cache_pool);World drop_world(cache,item_pool);
            Actor subject(9001,{}, {},{},drop_world,{definitions,enchant,mobs});subject.status().skills.max_bags=maximum;subject.attack_state().mode=std::bit_cast<std::int32_t>(mode);
            std::vector<std::uint8_t> request{6,2,static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),static_cast<std::uint8_t>(amount)};
            DropActions callbacks(request,mutation,{static_cast<std::uint8_t>(rb),static_cast<std::uint8_t>(rs),static_cast<std::uint8_t>(rc)});
            const auto result=vertical_inventory_drop_request(subject,request,drop_world,item_pool,callbacks);auto actual=callbacks.events;
            if(result.request==VerticalInventoryRequest::drop) {
                require(result.drop_result==ItemDropResult::ignored && !result.receipt,"Empty slot did not reach actual drop owner");
                actual+=",D"+std::to_string(request[2])+":"+std::to_string(request[3])+":"+std::to_string(request[4]);
            } else require(result.request==VerticalInventoryRequest::ignored && !result.drop_result,"Drop request gate changed");
            require((actual.empty()?"-":actual)==expected,"Original drop request trace differs");++cases;
        }
        require(cases==1024,"Missing original drop request cases");
        GroundItemPool item_pool;InventoryReplicaPool cache_pool;InventoryReplica cache(12345,0,cache_pool);World drop_world(cache,item_pool);ZoneItems drop_zone(geometry,drop_world);drop_world.zone=&drop_zone;
        auto table=item_definitions();const auto live_definitions=ItemDefinitions::load(table);Actor subject(9001,{}, {},{},drop_world,{live_definitions,enchant,mobs});subject.account_id()=12345;
        PlayerItem held;held.bag=1;held.slot=0;held.type=5;held.type_id=1;held.count=1;held.uid=42;
        require(subject.load_inventory_item(held,item_pool) && cache.load(held),"Cannot prepare drop fixture");
        const auto retained=subject.retained_inventory_item(1,0);require(retained && retained->item,"Loaded item was not retained");
        auto* const original_item=retained->item;const auto runtime_id=original_item->id;
        std::vector<std::uint8_t> request{6,2,1,0,1};DropActions callbacks(request,0,{1,0,1});
        for(unsigned size=0;size<5;++size) require(vertical_inventory_drop_request(subject,std::span(request).first(size),drop_world,item_pool,callbacks).request==VerticalInventoryRequest::malformed && callbacks.events.empty(),"Truncated drop caused effects");
        request[0]=7;require(vertical_inventory_drop_request(subject,request,drop_world,item_pool,callbacks).request==VerticalInventoryRequest::unhandled && callbacks.events.empty(),"Unrelated opcode reached drop handler");request[0]=6;
        const auto result=vertical_inventory_guarded_drop_request(subject,request,drop_world,item_pool,callbacks);
        require(result.drop_result==ItemDropResult::whole && callbacks.events=="C,S" && subject.inventory().empty() && drop_zone.size()==1 && cache.inventory().empty() && cache.deleted_inventory()==std::vector<std::uint64_t>{42},"Drop did not reach actual actor, zone and DB replica");
        auto* dropped=drop_zone.find(runtime_id);require(dropped==original_item,"Dropped item missing from zone index or replaced");
        require(dropped->item.uid==42,"Dropped item changed its persistent UID");
        require(drop_zone.leave(*dropped),"Dropped item could not leave its zone");item_pool.release(*dropped);
    }
    {
        std::ifstream offers(argv[3]);require(offers.good(),"Cannot open exchange request oracle");unsigned cases=0;
        for(std::string line;std::getline(offers,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned op,mode,present,ready,peer_ready,bag,slot,item_count,final_ready,final_peer_ready,final_bag;std::uint32_t money,value,final_money;std::string expected;
            row>>op>>mode>>present>>ready>>peer_ready>>money>>value>>bag>>slot>>item_count>>expected>>final_ready>>final_peer_ready>>final_money>>final_bag;require(!row.fail(),"Invalid exchange request oracle");
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(1,0,cache_pool),second_cache(2,0,cache_pool);World first_world(first_cache,items),second_world(second_cache,items);
            std::string trace;first_world.exchange_trace=&trace;second_world.exchange_trace=&trace;second_world.exchange_tag='B';
            Actor actor(1,{}, {},{},first_world,{definitions,enchant,mobs}),peer(2,{}, {},{},second_world,{definitions,enchant,mobs});
            actor.money()=money;actor.exchange().mode=static_cast<std::int32_t>(mode);actor.exchange().peer=present?&peer:nullptr;actor.exchange().money=0x12345678;actor.exchange().ready=static_cast<std::uint8_t>(ready);peer.exchange().ready=static_cast<std::uint8_t>(peer_ready);
            if(op==7 && value<8) actor.exchange().items[value]={static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),static_cast<std::uint8_t>(item_count)};
            std::vector<std::uint8_t> request{static_cast<std::uint8_t>(op),0x0a,0,0,0,0};for(unsigned i=0;i<4;++i) request[2+i]=static_cast<std::uint8_t>(value>>(8*i));ExchangeActions actions(trace);
            const auto result=vertical_inventory_exchange_request(actor,request,actions);
            require(result==VerticalExchangeRequest::ignored || result==VerticalExchangeRequest::updated,"Unexpected exchange result");
            require(trace==expected && actor.exchange().ready==final_ready && peer.exchange().ready==final_peer_ready && actor.exchange().money==final_money,"Original exchange offer trace differs");
            if(op==7 && value<8) require(actor.exchange().items[value].bag==final_bag && actor.exchange().items[value].slot==slot && actor.exchange().items[value].count==item_count,"Exchange offer removal changed payload");
            ++cases;
        }
        require(cases==1024,"Missing exchange request traces");
        {
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(1,0,cache_pool),second_cache(2,0,cache_pool);World first(first_cache,items),second(second_cache,items);
            Actor actor(1,{}, {},{},first,{definitions,enchant,mobs}),peer(2,{}, {},{},second,{definitions,enchant,mobs});actor.exchange().mode=3;actor.exchange().peer=&peer;actor.exchange().ready=1;actor.exchange().items[1]={1,3,5};actor.exchange().items[2]={2,4,6};
            std::vector<std::uint8_t> request{7,0x0a,1};first.mutate_offer=&request;first.mutate_value={2,0,0,0};std::string trace;ExchangeActions actions(trace);
            require(vertical_inventory_exchange_request(actor,request,actions)==VerticalExchangeRequest::updated && actor.exchange().items[1].bag==1 && actor.exchange().items[2].bag==0 && first.client.back()==net::Packet({7,0x0a,1,2}) && second.client.back()==net::Packet({7,0x0a,2,2}),"Exchange failed to reload offer index after readiness sends");
            actor.money()=100;actor.exchange().ready=1;request={8,0x0a,10,0,0,0};first.mutate_offer=&request;first.mutate_value={200,0,0,0};
            require(vertical_inventory_exchange_request(actor,request,actions)==VerticalExchangeRequest::updated && actor.exchange().money==200 && first.client.back()==net::Packet({8,0x0a,1,200,0,0,0}),"Exchange failed to reload money after readiness sends");
        }
        GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica cache(1,0,cache_pool);World world(cache,items);Actor actor(1,{}, {},{},world,{definitions,enchant,mobs});std::string trace;ExchangeActions actions(trace);
        const std::vector<std::uint8_t> request{8,0x0a,0,0,0,0};
        for(unsigned size=0;size<6;++size) require(vertical_inventory_exchange_request(actor,std::span(request).first(size),actions)==VerticalExchangeRequest::malformed && trace.empty(),"Truncated exchange caused effects");
        require(vertical_inventory_exchange_request(actor,net::Packet{9,0x0a},actions)==VerticalExchangeRequest::unhandled && trace.empty(),"Unowned exchange opcode caused effects");
    }
    {
        std::ifstream offers(argv[4]);require(offers.good(),"Cannot open item-offer oracle");unsigned cases=0;
        for(std::string line;std::getline(offers,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned mode,present,ready,peer_ready,maximum,bag,slot,count,index,item_present,real_type,item_count,final_ready,final_peer_ready;std::string initial_hex,raw_hex,expected,final_hex;
            row>>mode>>present>>ready>>peer_ready>>maximum>>bag>>slot>>count>>index>>item_present>>real_type>>item_count>>initial_hex>>raw_hex>>expected>>final_ready>>final_peer_ready>>final_hex;require(!row.fail(),"Invalid item-offer oracle");
            const auto initial=unhex(initial_hex),raw=unhex(raw_hex),final=unhex(final_hex);require(initial.size()==24 && final.size()==24 && raw.size()==33,"Invalid item-offer fixture");
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(1,0,cache_pool),second_cache(2,0,cache_pool);World first(first_cache,items),second(second_cache,items);std::string trace;first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';
            ItemDefinition definition;definition.id=5001;definition.type=5;definition.type_id=1;definition.real_type=static_cast<std::uint8_t>(real_type);
            Actor actor(1,{}, {},{},first,{definitions,enchant,mobs}),peer(2,{}, {},{},second,{definitions,enchant,mobs});actor.status().skills.max_bags=static_cast<std::int32_t>(maximum);actor.exchange().mode=static_cast<std::int32_t>(mode);actor.exchange().peer=present?&peer:nullptr;actor.exchange().ready=static_cast<std::uint8_t>(ready);peer.exchange().ready=static_cast<std::uint8_t>(peer_ready);
            for(unsigned i=0;i<8;++i) actor.exchange().items[i]={initial[i*3],initial[i*3+1],initial[i*3+2]};
            if(item_present && bag>0 && bag<=9 && slot<24) {auto* item=items.allocate();require(item,"Cannot allocate offered item");item->definition=&definition;item->item.type=raw[0];item->item.type_id=raw[1];item->item.count=static_cast<std::uint8_t>(item_count);item->item.quality=static_cast<std::uint16_t>(raw[4]|(raw[5]<<8));std::copy_n(raw.begin()+6,6,item->item.gems.begin());std::copy_n(raw.begin()+12,21,item->craft_storage.begin());require(actor.claim_inventory_item(static_cast<std::uint8_t>(bag),static_cast<std::uint16_t>(slot),*item,items),"Cannot claim offered item");}
            std::vector<std::uint8_t> request{6,0x0a,static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),static_cast<std::uint8_t>(count),static_cast<std::uint8_t>(index)};ExchangeActions actions(trace);
            const auto result=vertical_inventory_exchange_request(actor,request,actions);require(result==VerticalExchangeRequest::ignored || result==VerticalExchangeRequest::updated,"Unexpected item-offer result");
            require(trace==expected && actor.exchange().ready==final_ready && peer.exchange().ready==final_peer_ready,"Original item-offer trace differs");
            for(unsigned i=0;i<8;++i) {const auto& actual=actor.exchange().items[i];require(actual.bag==final[i*3] && actual.slot==final[i*3+1] && actual.count==final[i*3+2],"Original item-offer state differs");}++cases;
        }
        require(cases==1024,"Missing original item-offer traces");
        {
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(1,0,cache_pool),second_cache(2,0,cache_pool);World first(first_cache,items),second(second_cache,items);
            ItemDefinition definition;definition.id=5001;definition.type=5;definition.type_id=1;Actor actor(1,{}, {},{},first,{definitions,enchant,mobs}),peer(2,{}, {},{},second,{definitions,enchant,mobs});actor.exchange().mode=3;actor.exchange().peer=&peer;
            for(unsigned slot=2;slot<=3;++slot) {auto* item=items.allocate();require(item,"Cannot allocate reloaded offer item");item->definition=&definition;item->item.type=5;item->item.type_id=static_cast<std::uint8_t>(slot);item->item.count=5;item->craft_storage[0]=0;item->craft_storage[20]=static_cast<std::uint8_t>(0xa0+slot);require(actor.claim_inventory_item(1,static_cast<std::uint16_t>(slot),*item,items),"Cannot retain reloaded offer item");}
            std::vector<std::uint8_t> request{6,0x0a,1,2,3,0};first.mutate_offer=&request;first.mutate_value={1,3,4,0};std::string trace;ExchangeActions actions(trace);
            require(vertical_inventory_exchange_request(actor,request,actions)==VerticalExchangeRequest::updated && actor.exchange().items[0].slot==2 && actor.exchange().items[0].count==3,"Original offer was not recorded before sender callback");
            require(second.client.size()==1 && second.client[0].size()==35 && second.client[0][4]==3 && second.client[0][5]==4 && second.client[0][34]==0xa3,"Peer offer did not reload pooled payload and trailing craft byte");
        }
    }
    for(const unsigned opcode:{2U,5U,10U}) {
        std::ifstream responses(argv[opcode==2?5:opcode==5?7:10]);require(responses.good(),"Cannot open exchange control oracle");unsigned cases=0;
        for(std::string line;std::getline(responses,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned mode,peer_mode,present,response,ready,peer_ready,final_mode,final_peer_mode,final_present,final_remote,final_ready,final_peer_ready;std::uint32_t money,peer_money,final_money,final_peer_money;std::string initial_hex,remote_hex,expected,final_hex,final_remote_hex;
            row>>mode>>peer_mode>>present>>response>>ready>>peer_ready>>money>>peer_money>>initial_hex>>remote_hex>>expected>>final_mode>>final_peer_mode>>final_present>>final_remote>>final_ready>>final_peer_ready>>final_money>>final_peer_money>>final_hex>>final_remote_hex;require(!row.fail(),"Invalid exchange response oracle");
            const auto initial=unhex(initial_hex),remote=unhex(remote_hex),final=unhex(final_hex),remote_final=unhex(final_remote_hex);require(initial.size()==24 && remote.size()==24 && final.size()==24 && remote_final.size()==24,"Invalid exchange response fixture");
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(1,0,cache_pool),second_cache(2,0,cache_pool);World first(first_cache,items),second(second_cache,items);std::string trace;first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';
            Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});auto& a=actor.exchange();auto& b=peer.exchange();a.mode=static_cast<std::int32_t>(mode);b.mode=static_cast<std::int32_t>(peer_mode);a.peer=present?&peer:nullptr;b.peer=&actor;a.ready=static_cast<std::uint8_t>(ready);b.ready=static_cast<std::uint8_t>(peer_ready);a.money=money;b.money=peer_money;
            for(unsigned i=0;i<8;++i) {a.items[i]={initial[i*3],initial[i*3+1],initial[i*3+2]};b.items[i]={remote[i*3],remote[i*3+1],remote[i*3+2]};}
            std::vector<std::uint8_t> request{static_cast<std::uint8_t>(opcode==10?5:opcode),0x0a,static_cast<std::uint8_t>(response)};ExchangeActions actions(trace);RejectingSettlement settlement;
            const auto result=opcode==10?vertical_inventory_exchange_ready(actor,request,actions,settlement):vertical_inventory_exchange_request(actor,request,actions);
            const bool handled=opcode==2?mode==2:opcode==10?(mode==3 && present):(mode==3 && present && (response==1 || response==2));require(result==(handled?VerticalExchangeRequest::updated:VerticalExchangeRequest::ignored),"Exchange control status differs");
            require(settlement.calls==(opcode==10 && expected.find("A040a06")!=std::string::npos?1U:0U),"Settlement was not invoked exactly for the original rejection path");
            require(trace==expected && a.mode==static_cast<std::int32_t>(final_mode) && b.mode==static_cast<std::int32_t>(final_peer_mode) && (a.peer!=nullptr)==(final_present!=0) && (b.peer!=nullptr)==(final_remote!=0) && a.ready==final_ready && b.ready==final_peer_ready && a.money==final_money && b.money==final_peer_money,"Original exchange response differs");
            for(unsigned i=0;i<8;++i) require(a.items[i].bag==final[i*3] && a.items[i].slot==final[i*3+1] && a.items[i].count==final[i*3+2] && b.items[i].bag==remote_final[i*3] && b.items[i].slot==remote_final[i*3+1] && b.items[i].count==remote_final[i*3+2],"Original response offer state differs");++cases;
        }
        require(cases==512,"Missing original exchange control traces");
    }
    {
        GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica cache(1,0,cache_pool);World world(cache,items);Actor actor(1,{}, {},{},world,{definitions,enchant,mobs});std::string trace;ExchangeActions actions(trace);
        require(vertical_inventory_exchange_request(actor,net::Packet{5,0x0a},actions)==VerticalExchangeRequest::malformed && trace.empty(),"Truncated exchange control caused effects");
        require(vertical_inventory_exchange_request(actor,net::Packet{5,0x0a,0},actions)==VerticalExchangeRequest::unhandled && trace.empty(),"Settlement request caused partial effects");
        RejectingSettlement settlement;require(vertical_inventory_exchange_ready(actor,net::Packet{5,0x0a},actions,settlement)==VerticalExchangeRequest::malformed && trace.empty(),"Truncated ready caused effects");require(vertical_inventory_exchange_ready(actor,net::Packet{5,0x0a,1},actions,settlement)==VerticalExchangeRequest::unhandled && trace.empty(),"Non-ready control caused ready effects");
        std::vector<std::uint8_t> changing{5,0x0a,1};MutatingControlActions mutating(changing);bool blocked=false;try {vertical_inventory_exchange_request(actor,changing,mutating);}catch(const std::logic_error&) {blocked=true;}require(blocked && actor.exchange().mode==0 && world.client.empty(),"Mutated settlement control caused partial effects");
    }
    {
        std::ifstream invites(argv[6]);require(invites.good(),"Cannot open exchange invite oracle");unsigned cases=0;
        for(std::string line;std::getline(invites,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned mode,peer_mode,country,peer_country,target,final_mode,final_peer_mode,final_peer,final_remote;std::uint32_t tick,timeout,final_timeout;std::string expected;
            row>>mode>>peer_mode>>country>>peer_country>>target>>tick>>timeout>>expected>>final_mode>>final_peer_mode>>final_peer>>final_remote>>final_timeout;require(!row.fail(),"Invalid exchange invite oracle");
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(1,0,cache_pool),second_cache(2,0,cache_pool);World first(first_cache,items),second(second_cache,items);std::string trace;first.exchange_trace=&trace;first.trace_tick=true;first.test_tick=tick;second.exchange_trace=&trace;second.exchange_tag='B';
            Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.exchange().mode=static_cast<std::int32_t>(mode);peer.exchange().mode=static_cast<std::int32_t>(peer_mode);actor.appearance().identity.country=static_cast<std::uint8_t>(country);peer.appearance().identity.country=static_cast<std::uint8_t>(peer_country);
            ZoneUsers zone(1,1,0);zone.attach(actor,0,0,{});zone.attach(peer,0,0,{});
            std::vector<std::uint8_t> request{1,0x0a,0,0,0,0};for(unsigned i=0;i<4;++i) request[2+i]=static_cast<std::uint8_t>(target>>(8*i));ExchangeActions actions(trace);
            const auto result=vertical_inventory_exchange_invite(actor,request,zone,timeout,actions);const auto pointer=[&](const Actor* value){return value==&actor?1U:value==&peer?2U:0U;};
            require(result==(expected.find(",T")!=std::string::npos?VerticalExchangeRequest::updated:VerticalExchangeRequest::ignored),"Exchange invite status differs");
            require(trace==expected && actor.exchange().mode==static_cast<std::int32_t>(final_mode) && peer.exchange().mode==static_cast<std::int32_t>(final_peer_mode) && pointer(actor.exchange().peer)==final_peer && pointer(peer.exchange().peer)==final_remote && timeout==final_timeout,"Original exchange invite differs");++cases;
        }
        require(cases==512,"Missing original exchange invite traces");
        GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica cache(1,0,cache_pool);World world(cache,items);Actor actor(1,{}, {},{},world,{definitions,enchant,mobs});ZoneUsers zone(1,1,0);zone.attach(actor,0,0,{});std::uint32_t timeout=12;std::string trace;ExchangeActions actions(trace);std::vector<std::uint8_t> request{1,0x0a,0,0,0,0};
        for(unsigned size=0;size<6;++size) require(vertical_inventory_exchange_invite(actor,std::span(request).first(size),zone,timeout,actions)==VerticalExchangeRequest::malformed && trace.empty() && timeout==12,"Truncated exchange invite caused effects");
        request[0]=2;require(vertical_inventory_exchange_invite(actor,request,zone,timeout,actions)==VerticalExchangeRequest::unhandled && trace.empty(),"Unowned exchange opcode reached invite handler");
    }
    {
        GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(12345,0,cache_pool),second_cache(12346,0,cache_pool);World first(first_cache,items),second(second_cache,items);
        auto table=item_definitions();const auto live=ItemDefinitions::load(table);Actor actor(1001,{}, {},{},first,{live,enchant,mobs}),peer(1002,{}, {},{},second,{live,enchant,mobs});actor.account_id()=12345;
        PlayerItem held;held.uid=42;held.bag=1;held.slot=2;held.type=5;held.type_id=1;held.count=5;require(actor.load_inventory_item(held,items) && first_cache.load(held),"Cannot prepare retained exchange flow");
        ZoneUsers zone(1,1,0);zone.attach(actor,0,0,{});zone.attach(peer,0,0,{});std::uint32_t timeout=0;std::string trace;ExchangeActions actions(trace);
        require(vertical_inventory_exchange_invite(actor,net::Packet{1,0x0a,0xea,3,0,0},zone,timeout,actions)==VerticalExchangeRequest::updated && timeout==30100 && actor.exchange().peer==&peer && peer.exchange().peer==&actor,"Real zone invitation failed");
        require(vertical_inventory_exchange_request(peer,net::Packet{2,0x0a,0},actions)==VerticalExchangeRequest::updated && actor.exchange().mode==3 && peer.exchange().mode==3,"Trade response did not open both retained states");
        require(vertical_inventory_exchange_request(actor,net::Packet{6,0x0a,1,2,3,0},actions)==VerticalExchangeRequest::updated && actor.exchange().items[0].bag==1 && actor.exchange().items[0].count==3,"Retained item was not offered");
        require(vertical_inventory_exchange_request(peer,net::Packet{5,0x0a,2},actions)==VerticalExchangeRequest::updated && actor.exchange().mode==0 && peer.exchange().mode==0,"Trade abort did not reset both owners");
        require(actor.retained_inventory_item(1,2) && actor.inventory()[0].uid==42 && first_cache.inventory().size()==1 && first_cache.inventory()[0].item.uid==42 && first_cache.inventory()[0].item.count==5 && first.database.empty() && second.database.empty(),"Canceled offer changed inventory or DB replica");
        require(vertical_inventory_exchange_invite(actor,net::Packet{1,0x0a,0xea,3,0,0},zone,timeout,actions)==VerticalExchangeRequest::updated && timeout==30100,"Cannot prepare actual invitation expiry");
        std::uint32_t peer_timeout=0;require(!vertical_inventory_exchange_expire(actor,30099,timeout,&peer_timeout) && actor.exchange().mode==1,"Invitation expired too early");
        require(vertical_inventory_exchange_expire(actor,30100,timeout,&peer_timeout) && actor.exchange().mode==0 && peer.exchange().mode==2 && timeout==30100 && peer_timeout==0,"Invitation expiry did not preserve original asymmetric state");
    }
    {
        std::ifstream checks(argv[8]);require(checks.good(),"Cannot open exchange capacity oracle");unsigned cases=0;
        for(std::string line;std::getline(checks,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned operation,expected;std::int32_t maximum;std::uint32_t payer_money,recipient_money,amount;std::string offer_hex,masks;
            row>>operation>>maximum>>offer_hex>>masks>>payer_money>>recipient_money>>amount>>expected;require(!row.fail(),"Invalid exchange capacity oracle");const auto raw=unhex(offer_hex);require(raw.size()==24,"Invalid capacity offer");ItemExchangeState offer;for(unsigned i=0;i<8;++i) offer.items[i]={raw[i*3],raw[i*3+1],raw[i*3+2]};
            std::vector<PlayerItem> inventory;std::istringstream mask_row(masks);for(unsigned bag=1;bag<=9;++bag) {std::string value;require(static_cast<bool>(std::getline(mask_row,value,',')),"Missing capacity bag mask");const auto mask=std::stoul(value,nullptr,16);if(operation==1) for(unsigned slot=0;slot<24;++slot) if(mask&(1U<<slot)) {PlayerItem item;item.bag=static_cast<std::uint8_t>(bag);item.slot=static_cast<std::uint16_t>(slot);inventory.push_back(item);}}
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica cache(1,0,cache_pool);World world(cache,items);Actor actor(1,{}, {},{},world,{definitions,enchant,mobs},std::move(inventory)),recipient(2,{}, {},{},world,{definitions,enchant,mobs});actor.status().skills.max_bags=maximum;actor.money()=payer_money;recipient.money()=recipient_money;
            const auto result=operation==0?vertical_inventory_exchange_money_valid(actor,recipient,amount):vertical_inventory_exchange_items_fit(actor,offer);require(result==(expected!=0),"Original exchange capacity check differs");++cases;
        }
        require(cases==2048,"Missing original exchange capacity cases");
    }
    {
        std::ifstream checks(argv[9]);require(checks.good(),"Cannot open exchange preflight oracle");unsigned cases=0;
        for(std::string line;std::getline(checks,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned scenario,expected;std::int32_t a_max,p_max;std::uint32_t a_money,p_money,a_offer,p_offer;std::string a_mask,p_mask,a_hex,p_hex,events;
            row>>scenario>>a_money>>p_money>>a_offer>>p_offer>>a_max>>p_max>>a_mask>>p_mask>>a_hex>>p_hex>>events>>expected;require(!row.fail(),"Invalid exchange preflight oracle");
            const auto a_raw=unhex(a_hex),p_raw=unhex(p_hex);require(a_raw.size()==24 && p_raw.size()==24,"Invalid preflight offer bytes");
            const auto inventory=[](const std::string& value) {std::vector<PlayerItem> result;const auto mask=std::stoul(value,nullptr,16);for(unsigned slot=0;slot<24;++slot) if(mask&(1U<<slot)) {PlayerItem item;item.bag=1;item.slot=static_cast<std::uint16_t>(slot);result.push_back(item);}return result;};
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(1,0,cache_pool),second_cache(2,0,cache_pool);World first(first_cache,items),second(second_cache,items);std::string trace;first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';
            Actor actor(1,{}, {},{},first,{definitions,enchant,mobs},inventory(a_mask)),peer(2,{}, {},{},second,{definitions,enchant,mobs},inventory(p_mask));actor.money()=a_money;peer.money()=p_money;actor.status().skills.max_bags=a_max;peer.status().skills.max_bags=p_max;actor.exchange().money=a_offer;peer.exchange().money=p_offer;
            for(unsigned i=0;i<8;++i) {actor.exchange().items[i]={a_raw[i*3],a_raw[i*3+1],a_raw[i*3+2]};peer.exchange().items[i]={p_raw[i*3],p_raw[i*3+1],p_raw[i*3+2]};}
            const auto result=vertical_inventory_exchange_preflight(actor,peer);require(static_cast<unsigned>(result)==expected && (trace.empty()?"-":trace.substr(1))==events,"Original ordered exchange preflight differs");
            require(actor.money()==a_money && peer.money()==p_money && actor.exchange().money==a_offer && peer.exchange().money==p_offer && first.database.empty() && second.database.empty(),"Exchange preflight changed transfer state");++cases;
        }
        require(cases==1024,"Missing original ordered exchange preflight cases");
    }
    {
        std::ifstream expiries(argv[11]);require(expiries.good(),"Cannot open exchange expiry oracle");unsigned cases=0;
        for(std::string line;std::getline(expiries,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned mode,peer_mode,kind,ready,peer_ready,final_mode,final_peer_mode,final_peer,final_remote,final_ready,final_peer_ready;std::uint32_t timeout,peer_timeout,now,money,peer_money,final_timeout,final_peer_timeout,final_money,final_peer_money;std::string offers,remote,final_offers,final_remote_offers;
            row>>mode>>peer_mode>>kind>>timeout>>peer_timeout>>now>>money>>peer_money>>ready>>peer_ready>>offers>>remote>>final_mode>>final_peer_mode>>final_peer>>final_remote>>final_timeout>>final_peer_timeout>>final_money>>final_peer_money>>final_ready>>final_peer_ready>>final_offers>>final_remote_offers;require(!row.fail(),"Invalid exchange expiry oracle");
            const auto a_raw=unhex(offers),b_raw=unhex(remote),a_final=unhex(final_offers),b_final=unhex(final_remote_offers);require(a_raw.size()==24 && b_raw.size()==24 && a_final.size()==24 && b_final.size()==24,"Invalid expiry offer bytes");
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(1,0,cache_pool),second_cache(2,0,cache_pool);World first(first_cache,items),second(second_cache,items);
            Actor actor(1,{}, {},{},first,{definitions,enchant,mobs}),peer(2,{}, {},{},second,{definitions,enchant,mobs});auto& a=actor.exchange();auto& b=peer.exchange();a.mode=static_cast<std::int32_t>(mode);b.mode=static_cast<std::int32_t>(peer_mode);a.peer=kind==0?nullptr:kind==1?&actor:&peer;b.peer=&actor;a.money=money;b.money=peer_money;a.ready=static_cast<std::uint8_t>(ready);b.ready=static_cast<std::uint8_t>(peer_ready);
            for(unsigned i=0;i<8;++i) {a.items[i]={a_raw[i*3],a_raw[i*3+1],a_raw[i*3+2]};b.items[i]={b_raw[i*3],b_raw[i*3+1],b_raw[i*3+2]};}
            const bool expected=timeout && timeout<=now && (mode==1 || mode==2);auto* other_timer=kind==1?&timeout:kind==2?&peer_timeout:nullptr;const auto result=vertical_inventory_exchange_expire(actor,now,timeout,other_timer);const auto pointer=[&](const Actor* value){return value==&actor?1U:value==&peer?2U:0U;};
            require(result==expected && a.mode==static_cast<std::int32_t>(final_mode) && b.mode==static_cast<std::int32_t>(final_peer_mode) && pointer(a.peer)==final_peer && pointer(b.peer)==final_remote && timeout==final_timeout && peer_timeout==final_peer_timeout && a.money==final_money && b.money==final_peer_money && a.ready==final_ready && b.ready==final_peer_ready,"Original exchange expiry differs");
            for(unsigned i=0;i<8;++i) require(a.items[i].bag==a_final[i*3] && a.items[i].slot==a_final[i*3+1] && a.items[i].count==a_final[i*3+2] && b.items[i].bag==b_final[i*3] && b.items[i].slot==b_final[i*3+1] && b.items[i].count==b_final[i*3+2],"Original expiry offers differ");
            require(first.client.empty() && second.client.empty(),"Exchange expiry sent unexpected packets");++cases;
        }
        require(cases==1024,"Missing original exchange expiry traces");
        GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica cache(1,0,cache_pool);World world(cache,items);Actor actor(1,{}, {},{},world,{definitions,enchant,mobs}),peer(2,{}, {},{},world,{definitions,enchant,mobs});actor.exchange().mode=1;actor.exchange().peer=&peer;peer.exchange().mode=1;std::uint32_t timeout=1;bool rejected=false;
        try {vertical_inventory_exchange_expire(actor,1,timeout,nullptr);} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected && timeout==1 && actor.exchange().mode==1 && peer.exchange().mode==1,"Missing peer timer changed exchange state");
    }
    {
        std::ifstream logs(argv[12]);require(logs.good(),"Cannot open exchange money-log oracle");unsigned cases=0;
        GameLogSubject actor;actor.account_id=12345;actor.account_name="account-a";actor.character_id=1001;actor.character_name="Actor";actor.level=30;actor.experience=123;actor.map=7;actor.position={10,-1,20};
        GameLogSubject peer;peer.account_id=12346;peer.account_name="account-b";peer.character_id=1002;peer.character_name="Peer";peer.level=31;peer.experience=456;peer.map=8;peer.position={30,-2,40};
        const auto calendar=std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59};
        for(std::string line;std::getline(logs,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned kind,subject;std::uint32_t amount,balance;std::string before,expected;row>>kind>>subject>>amount>>balance>>before>>expected;require(!row.fail() && kind<2 && subject<2,"Invalid money-log oracle");
            auto scratch=unhex(before);const auto target=unhex(expected);vertical_inventory_exchange_money_log(scratch,kind?VerticalExchangeMoneyLog::incoming:VerticalExchangeMoneyLog::outgoing,subject?peer:actor,subject?actor:peer,amount,balance,0x12345678,calendar);
            require(scratch==target,"Original exchange money log differs");++cases;
        }
        require(cases==492,"Missing original exchange money-log cases");
        std::vector<std::uint8_t> invalid(110,0xa5);bool rejected=false;try {vertical_inventory_exchange_money_log(invalid,VerticalExchangeMoneyLog::incoming,actor,peer,1,1,0,calendar);}catch(const std::invalid_argument&) {rejected=true;}require(rejected && std::all_of(invalid.begin(),invalid.end(),[](auto byte){return byte==0xa5;}),"Invalid money log mutated scratch");
    }
    {
        std::ifstream phases(argv[13]);require(phases.good(),"Cannot open exchange money-phase oracle");unsigned cases=0;
        for(std::string line;std::getline(phases,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);std::uint32_t actor_money,peer_money,actor_offer,peer_offer,final_actor,final_peer,final_actor_offer,final_peer_offer,final_field;unsigned mutation,event_count;std::string out_hex,in_hex;
            row>>actor_money>>peer_money>>actor_offer>>peer_offer>>mutation>>final_actor>>final_peer>>final_actor_offer>>final_peer_offer>>final_field>>out_hex>>in_hex>>event_count;require(!row.fail(),"Invalid exchange money-phase oracle");std::vector<std::string> events;for(unsigned i=0;i<event_count;++i) {std::string event;row>>event;require(!row.fail(),"Missing exchange money-phase event");events.push_back(event);}
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(12345,actor_money,cache_pool),second_cache(12346,peer_money,cache_pool);World first(first_cache,items),second(second_cache,items);ExchangeMoneyWorld money_world(first_cache,second_cache);
            Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.account_id()=12345;peer.account_id()=12346;actor.money()=actor_money;peer.money()=peer_money;actor.exchange().money=actor_offer;peer.exchange().money=peer_offer;
            require(vertical_inventory_exchange_preflight(actor,peer)==VerticalExchangePreflight::transfer_required,"Money fixture failed original preflight");
            auto outgoing=unhex(out_hex),incoming=unhex(in_hex);std::uint32_t initiator_field=0x12345678;if(mutation) {money_world.mutate_actor=&actor;money_world.mutate_peer=&peer;money_world.mutate_field=&initiator_field;}vertical_inventory_exchange_money_phase(actor,peer,money_world,outgoing,incoming,initiator_field);
            require(actor.money()==final_actor && peer.money()==final_peer && first_cache.money()==final_actor && second_cache.money()==final_peer && actor.exchange().money==final_actor_offer && peer.exchange().money==final_peer_offer && initiator_field==final_field,"Exchange money phase and retained replicas diverged");require(money_world.events==events,"Original exchange money-phase events differ");require(first.client.empty() && second.client.empty(),"Money phase prematurely completed exchange");++cases;
        }
        require(cases==256,"Missing original exchange money-phase cases");
        GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica first_cache(12345,10,cache_pool),second_cache(12346,20,cache_pool);World first(first_cache,items),second(second_cache,items);ExchangeMoneyWorld money_world(first_cache,second_cache);Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.money()=10;peer.money()=20;actor.exchange().money=3;net::Packet bad(125),incoming(111);const std::uint32_t field=0;bool rejected=false;
        try {vertical_inventory_exchange_money_phase(actor,peer,money_world,bad,incoming,field);} catch(const std::invalid_argument&) {rejected=true;}
        require(rejected && actor.money()==10 && peer.money()==20 && first_cache.money()==10 && second_cache.money()==20 && money_world.events.empty(),"Invalid money scratch caused partial transfer");
    }
    {
        std::ifstream packets(argv[14]);require(packets.good(),"Cannot open exchange transfer-packet oracle");unsigned cases=0;
        for(std::string line;std::getline(packets,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned source_bag,source_slot,offered,destination_bag,destination_slot,remains;std::string payload_hex,scratch_hex,sender_hex,receiver_hex;
            row>>source_bag>>source_slot>>offered>>destination_bag>>destination_slot>>remains>>payload_hex>>scratch_hex>>sender_hex>>receiver_hex;require(!row.fail(),"Invalid exchange transfer-packet oracle");
            const auto raw=unhex(payload_hex);require(raw.size()==33,"Invalid source item bytes");GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica cache(1,0,cache_pool);World world(cache,items);ItemDefinition definition;definition.id=5001;
            Actor actor(1,{}, {},{},world,{definitions,enchant,mobs});auto* source=items.allocate();require(source,"Cannot allocate source item");source->definition=&definition;source->item.type=raw[0];source->item.type_id=raw[1];source->item.count=raw[2];source->item.quality=static_cast<std::uint16_t>(raw[4]|(raw[5]<<8));std::copy_n(raw.begin()+6,6,source->item.gems.begin());std::copy_n(raw.begin()+12,21,source->craft_storage.begin());
            require(actor.claim_inventory_item(static_cast<std::uint8_t>(source_bag),static_cast<std::uint16_t>(source_slot),*source,items),"Cannot retain exchange source");const ExchangeItemReference offer{static_cast<std::uint8_t>(source_bag),static_cast<std::uint8_t>(source_slot),static_cast<std::uint8_t>(offered)};
            const auto sender=vertical_inventory_exchange_sender_packet(offer,*source,remains!=0);auto receiver=unhex(scratch_hex);vertical_inventory_exchange_receiver_packet(receiver,static_cast<std::uint8_t>(destination_bag),static_cast<std::uint8_t>(destination_slot),static_cast<std::uint8_t>(offered),*source);
            require(sender==unhex(sender_hex) && receiver==unhex(receiver_hex),"Original exchange item packet differs");require(actor.retained_inventory_item(offer.bag,offer.slot)->item==source,"Packet builders changed pooled ownership");++cases;
        }
        require(cases==1024,"Missing original exchange transfer-packet traces");GroundItem invalid;invalid.craft_storage.fill(0xff);std::vector<std::uint8_t> scratch(36,0xa5);bool rejected=false;try {vertical_inventory_exchange_receiver_packet(scratch,1,0,1,invalid);}catch(const std::invalid_argument&) {rejected=true;}require(rejected && std::all_of(scratch.begin(),scratch.end(),[](auto byte){return byte==0xa5;}),"Invalid craft changed receipt scratch");
    }
    {
        std::ifstream packets(argv[15]);require(packets.good(),"Cannot open exchange transfer DB oracle");unsigned cases=0;
        for(std::string line;std::getline(packets,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned split,source_bag,source_slot,offered,remaining,destination_bag,destination_slot,make_type;std::uint64_t source_uid,transfer_uid;std::uint32_t item_id,made_at;std::string payload_hex,scratch_hex,add_hex,remove_hex;
            row>>split>>source_bag>>source_slot>>offered>>remaining>>destination_bag>>destination_slot>>source_uid>>transfer_uid>>item_id>>payload_hex>>made_at>>make_type>>scratch_hex>>add_hex>>remove_hex;require(!row.fail(),"Invalid exchange transfer DB oracle");const auto raw=unhex(payload_hex);require(raw.size()==33,"Invalid transfer DB item bytes");
            GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica source_cache(12345,0,cache_pool),destination_cache(12346,0,cache_pool);World world(source_cache,items);ItemDefinition definition;definition.id=item_id;Actor actor(1,{}, {},{},world,{definitions,enchant,mobs});
            auto* source=items.allocate();require(source,"Cannot allocate DB source item");auto* transferred=split?items.allocate():source;require(transferred,"Cannot allocate DB transfer item");
            const auto fill=[&](GroundItem& item,std::uint64_t uid) {item.definition=&definition;item.item.uid=uid;item.item.type=raw[0];item.item.type_id=raw[1];item.item.count=raw[2];item.item.quality=static_cast<std::uint16_t>(raw[4]|(raw[5]<<8));std::copy_n(raw.begin()+6,6,item.item.gems.begin());std::copy_n(raw.begin()+12,21,item.craft_storage.begin());item.item.made_at=made_at;if(make_type)item.item.make_type.assign(1,static_cast<char>(make_type));};
            fill(*source,source_uid);source->item.count=static_cast<std::uint8_t>(remaining);if(split) fill(*transferred,transfer_uid);
            require(actor.claim_inventory_item(static_cast<std::uint8_t>(source_bag),static_cast<std::uint16_t>(source_slot),*source,items),"Cannot retain DB source item");auto cached=source->item;cached.count=static_cast<std::uint8_t>(remaining+(split?offered:0));require(source_cache.load(cached),"Cannot load source DB replica");
            const ExchangeItemReference offer{static_cast<std::uint8_t>(source_bag),static_cast<std::uint8_t>(source_slot),static_cast<std::uint8_t>(offered)};auto add=unhex(scratch_hex);vertical_inventory_exchange_db_add_packet(add,12346,static_cast<std::uint8_t>(destination_bag),static_cast<std::uint8_t>(destination_slot),*transferred);const auto remove=vertical_inventory_exchange_db_remove_packet(12345,offer,static_cast<std::uint8_t>(split?offered:remaining));
            require(add==unhex(add_hex) && remove==unhex(remove_hex),"Original exchange DB packet differs");require(destination_cache.apply(add)==InventoryReplicaResult::applied && source_cache.apply(remove)==InventoryReplicaResult::applied,"Exchange DB packets did not reach retained replicas");
            const auto destination=destination_cache.inventory(),remaining_items=source_cache.inventory();require(destination.size()==1 && destination[0].item.uid==transfer_uid && destination[0].item.id==item_id && destination[0].item.bag==destination_bag && destination[0].item.slot==destination_slot && destination[0].item.count==raw[2],"Destination DB replica differs after exchange add");
            if(split) {require(remaining_items.size()==1 && remaining_items[0].item.uid==source_uid && remaining_items[0].item.count==remaining && source_cache.deleted_inventory().empty(),"Split DB removal lost source UID or count");items.release(*transferred);}else require(remaining_items.empty() && source_cache.deleted_inventory()==std::vector<std::uint64_t>{source_uid},"Whole DB removal did not journal source UID");
            require(actor.retained_inventory_item(offer.bag,offer.slot)->item==source,"DB packet builders changed actor ownership");++cases;
        }
        require(cases==1024,"Missing original exchange transfer DB traces");GroundItem invalid;invalid.craft_storage.fill(0xff);std::vector<std::uint8_t> scratch(57,0xa5);bool rejected=false;try {vertical_inventory_exchange_db_add_packet(scratch,1,1,0,invalid);}catch(const std::invalid_argument&) {rejected=true;}require(rejected && std::all_of(scratch.begin(),scratch.end(),[](auto byte){return byte==0xa5;}),"Invalid DB item changed scratch");
    }
    {
        std::ifstream logs(argv[16]);require(logs.good(),"Cannot open exchange item-log oracle");unsigned cases=0;
        GameLogSubject actor;actor.account_id=12345;actor.account_name="account-a";actor.character_id=1001;actor.character_name="Actor";actor.level=30;actor.experience=123;actor.map=7;actor.position={10,-1,20};
        GameLogSubject peer;peer.account_id=12346;peer.account_name="account-b";peer.character_id=1002;peer.character_name="Peer";peer.level=31;peer.experience=456;peer.map=8;peer.position={30,-2,40};
        const auto calendar=std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59};
        for(std::string line;std::getline(logs,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned kind,offered,bag,slot;std::uint64_t uid;std::uint32_t item_id,made_at;std::string payload,before,expected;
            row>>kind>>uid>>item_id>>offered>>bag>>slot>>payload>>made_at>>before>>expected;require(!row.fail() && kind<2,"Invalid exchange item-log oracle");
            const auto raw=unhex(payload);require(raw.size()==33,"Invalid item-log source bytes");GroundItemPool items;InventoryReplicaPool cache_pool;InventoryReplica cache(1,0,cache_pool);World world(cache,items);ItemDefinition definition;definition.id=item_id;
            Actor owner(1,{}, {},{},world,{definitions,enchant,mobs});auto* source=items.allocate();require(source,"Cannot allocate item-log source");source->definition=&definition;source->item.uid=uid;source->item.made_at=made_at;std::copy_n(raw.begin()+6,6,source->item.gems.begin());std::copy_n(raw.begin()+12,21,source->craft_storage.begin());
            require(owner.claim_inventory_item(1,2,*source,items),"Cannot retain item-log source");auto scratch=unhex(before);
            vertical_inventory_exchange_item_log(scratch,kind?VerticalExchangeItemLog::incoming:VerticalExchangeItemLog::outgoing,kind?peer:actor,kind?actor:peer,*source,"Strange item",static_cast<std::uint8_t>(offered),static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),calendar);
            require(scratch==unhex(expected),"Original exchange item log differs");require(owner.retained_inventory_item(1,2)->item==source,"Item log changed source ownership");++cases;
        }
        require(cases==1024,"Missing original exchange item-log traces");GroundItem invalid;std::vector<std::uint8_t> scratch(181,0xa5);bool rejected=false;
        try {vertical_inventory_exchange_item_log(scratch,VerticalExchangeItemLog::outgoing,actor,peer,invalid,"x",1,1,1,calendar);}catch(const std::invalid_argument&) {rejected=true;}
        require(rejected && std::all_of(scratch.begin(),scratch.end(),[](auto byte){return byte==0xa5;}),"Invalid item log changed scratch");
    }
    {
        std::ifstream oracle(argv[17]);require(oracle.good(),"Cannot open exchange item-phase oracle");std::string line,expected;
        while(std::getline(oracle,line)) if(!line.empty() && line[0]!='#') expected=line;
        GroundItemPool items;InventoryReplicaPool pool;InventoryReplica first_cache(1,0,pool),second_cache(2,0,pool);World first(first_cache,items),second(second_cache,items);std::string events;first.exchange_trace=&events;second.exchange_trace=&events;second.exchange_tag='B';
        Actor actor(1,{}, {},{},first,{definitions,enchant,mobs}),peer(2,{}, {},{},second,{definitions,enchant,mobs});EmptyExchangeTransfer transfer(actor,peer,events);
        vertical_inventory_exchange_item_phase(actor,peer,transfer);require(!events.empty() && events.substr(1)==expected,"Original exchange item phase differs");
        require(first.client==std::vector<net::Packet>{{4,0x0a,0}} && second.client==first.client,"Exchange completion packet differs");
    }
    {
        std::ifstream selections(argv[18]);require(selections.good(),"Cannot open exchange item-selection oracle");unsigned cases=0;
        for(std::string line;std::getline(selections,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned maximum,mask1,mask2,source_mask,index,bag,slot,whole;std::string offers_hex,counts_hex,stacks_hex;
            row>>maximum>>mask1>>mask2>>source_mask>>offers_hex>>counts_hex>>stacks_hex>>index>>bag>>slot>>whole;require(!row.fail(),"Invalid item-selection trace");
            const auto offers=unhex(offers_hex),counts=unhex(counts_hex),stacks=unhex(stacks_hex);require(offers.size()==24 && counts.size()==8 && stacks.size()==8,"Invalid item-selection bytes");
            GroundItemPool items;InventoryReplicaPool pool;InventoryReplica first_cache(1,0,pool),second_cache(2,0,pool);World first(first_cache,items),second(second_cache,items);
            Actor recipient(1,{}, {},{},first,{definitions,enchant,mobs}),source(2,{}, {},{},second,{definitions,enchant,mobs});recipient.status().skills.max_bags=static_cast<std::int32_t>(maximum);std::array<ItemDefinition,8> item_definitions{};ItemDefinition filler_definition;
            for(unsigned i=0;i<8;++i) {
                source.exchange().items[i]={offers[i*3],offers[i*3+1],offers[i*3+2]};item_definitions[i].stack_count=stacks[i];
                if(source_mask&(1U<<i)) {auto* item=items.allocate();require(item,"Cannot allocate source item");item->definition=&item_definitions[i];item->item.count=counts[i];require(source.claim_inventory_item(1,i,*item,items),"Cannot retain source item");}
            }
            for(unsigned b=1;b<=2;++b) for(unsigned n=0;n<24;++n) if((b==1?mask1:mask2)&(1U<<n)) {auto* item=items.allocate();require(item,"Cannot allocate destination item");item->definition=&filler_definition;require(recipient.claim_inventory_item(b,n,*item,items),"Cannot retain destination item");}
            const auto selected=vertical_inventory_exchange_next_item(recipient,source);
            if(index==255) require(!selected,"Unexpected exchange item selection");
            else {const auto held=source.retained_inventory_item(1,index);require(selected && held && selected->offer_index==index && selected->destination_bag==bag && selected->destination_slot==slot && selected->whole==(whole!=0) && selected->source_item==held->item && selected->source_pool==held->pool,"Original exchange item selection differs");}
            require(first.client.empty() && second.client.empty(),"Exchange selection sent packets");++cases;
        }
        require(cases==512,"Missing original exchange item-selection traces");
    }
    {
        std::ifstream traces(argv[19]);require(traces.good(),"Cannot open exchange split-failure oracle");unsigned cases=0;
        GameLogSubject actor;actor.account_id=12345;actor.account_name="account-a";actor.character_id=1001;actor.character_name="Actor";actor.level=30;actor.experience=123;actor.map=7;actor.position={10,-1,20};
        GameLogSubject peer;peer.account_id=12346;peer.account_name="account-b";peer.character_id=1002;peer.character_name="Peer";peer.level=31;peer.experience=456;peer.map=8;peer.position={30,-2,40};
        const auto calendar=std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59};
        for(std::string line;std::getline(traces,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);std::uint64_t uid;std::uint32_t item_id,made;unsigned offered,bag,slot,final_count;std::string payload,out_before,receiver_before,in_before,sender_expected,out_expected,receiver_expected,in_expected;
            row>>uid>>item_id>>offered>>bag>>slot>>payload>>made>>out_before>>receiver_before>>in_before>>sender_expected>>out_expected>>receiver_expected>>in_expected>>final_count;require(!row.fail(),"Invalid split-failure trace");
            const auto raw=unhex(payload);require(raw.size()==33,"Invalid split-failure item");GroundItemPool items;GroundItemPool failing(1,[]{return std::unique_ptr<GroundItem>{};});InventoryReplicaPool pool;InventoryReplica cache(1,0,pool);World world(cache,items);ItemDefinition definition;definition.id=item_id;
            Actor owner(1,{}, {},{},world,{definitions,enchant,mobs});auto* source=items.allocate();require(source,"Cannot allocate split source");source->definition=&definition;source->item.uid=uid;source->item.type=raw[0];source->item.type_id=raw[1];source->item.count=raw[2];source->item.quality=static_cast<std::uint16_t>(raw[4]|(raw[5]<<8));source->item.made_at=made;std::copy_n(raw.begin()+6,6,source->item.gems.begin());std::copy_n(raw.begin()+12,21,source->craft_storage.begin());require(owner.claim_inventory_item(1,2,*source,items),"Cannot retain split source");
            require(!failing.allocate(),"Split failure fixture unexpectedly allocated");const ExchangeItemReference offer{1,2,static_cast<std::uint8_t>(offered)};auto outgoing=unhex(out_before),receiver=unhex(receiver_before),incoming=unhex(in_before);
            const auto sender=vertical_inventory_exchange_sender_packet(offer,*source,false);vertical_inventory_exchange_item_log(outgoing,VerticalExchangeItemLog::outgoing,actor,peer,*source,"Strange item",offer.count,bag,slot,calendar);vertical_inventory_exchange_receiver_packet(receiver,bag,slot,offer.count,*source);vertical_inventory_exchange_item_log(incoming,VerticalExchangeItemLog::incoming,peer,actor,*source,"Strange item",offer.count,bag,slot,calendar);
            require(sender==unhex(sender_expected) && outgoing==unhex(out_expected) && receiver==unhex(receiver_expected) && incoming==unhex(in_expected),"Original split allocation failure packets differ");
            require(source->item.count==final_count && owner.retained_inventory_item(1,2)->item==source && world.database.empty(),"Failed split changed actual ownership, count or DB");++cases;
        }
        require(cases==64,"Missing original exchange split-failure traces");
    }
    {
        std::ifstream traces(argv[20]);require(traces.good(),"Cannot open exchange split-ownership oracle");unsigned cases=0;
        struct Clock final:ItemIdClock {unsigned times=0,ticks=0;std::uint32_t ticks_ms() override {++ticks;return 100;}std::uint32_t packed_local_calendar() override {++times;return pack_item_id_time(std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59});}};
        for(std::string line;std::getline(traces,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned count,offered,type,type_id,server,wisdom,bag,slot,remaining,make_type;std::uint64_t uid;std::uint32_t made;std::string tail_hex,payload_hex;
            row>>count>>offered>>type>>type_id>>server>>wisdom>>tail_hex>>bag>>slot>>uid>>made>>remaining>>payload_hex>>make_type;require(!row.fail(),"Invalid split-ownership trace");const auto tail=unhex(tail_hex),payload=unhex(payload_hex);require(tail.size()==20 && payload.size()==33,"Invalid split payload");
            GroundItemPool items;InventoryReplicaPool pool;InventoryReplica first_cache(1,0,pool),second_cache(2,0,pool);World first(first_cache,items),second(second_cache,items);Actor recipient(1,{}, {},{},first,{definitions,enchant,mobs}),source(2,{}, {},{},second,{definitions,enchant,mobs});recipient.status().skills.max_bags=9;ItemDefinition definition;definition.id=5001;definition.server=server;definition.required_wisdom=wisdom;
            auto* old=items.allocate();require(old,"Cannot allocate split source");old->definition=&definition;old->item.count=count;old->item.type=type;old->item.type_id=type_id;require(source.claim_inventory_item(1,2,*old,items),"Cannot retain split source");auto* recycled=items.allocate();require(recycled,"Cannot allocate recycled split item");std::copy(tail.begin(),tail.end(),recycled->craft_storage.begin()+1);items.release(*recycled);
            source.exchange().items[0]={1,2,static_cast<std::uint8_t>(offered)};VerticalExchangeItemSelection selection{0,static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),old,&items,false};Clock clock;ItemIds ids(7,clock);auto* item=vertical_inventory_exchange_split_item(recipient,source,selection,items,ids,clock);require(item==recycled,"Split did not reuse pool owner");
            const auto held=recipient.retained_inventory_item(bag,slot);require(held && held->item==item && held->pool==&items && item->definition==&definition && item->item.uid==uid && item->item.made_at==made && item->item.make_type==std::string(1,static_cast<char>(make_type)),"Original split ownership or identity differs");
            require(old->item.count==remaining && source.retained_inventory_item(1,2)->item==old && item->item.type==payload[0] && item->item.type_id==payload[1] && item->item.count==payload[2] && item->item.quality==(payload[4]|(payload[5]<<8)) && std::equal(item->item.gems.begin(),item->item.gems.end(),payload.begin()+6) && std::equal(item->craft_storage.begin(),item->craft_storage.end(),payload.begin()+12),"Original split fields differ");
            require(first.database.empty() && second.database.empty() && first.client.empty() && second.client.empty(),"Split ownership emitted premature messages");++cases;
        }
        require(cases==128,"Missing original split-ownership traces");
        GroundItemPool items;GroundItemPool failing(1,[]{return std::unique_ptr<GroundItem>{};});InventoryReplicaPool pool;InventoryReplica first_cache(1,0,pool),second_cache(2,0,pool);World first(first_cache,items),second(second_cache,items);Actor recipient(1,{}, {},{},first,{definitions,enchant,mobs}),source(2,{}, {},{},second,{definitions,enchant,mobs});recipient.status().skills.max_bags=1;ItemDefinition definition;auto* old=items.allocate();require(old,"Cannot allocate failure source");old->definition=&definition;old->item.count=5;require(source.claim_inventory_item(1,2,*old,items),"Cannot retain failure source");source.exchange().items[0]={1,2,2};VerticalExchangeItemSelection selection{0,1,3,old,&items,false};Clock clock;ItemIds ids(7,clock);const auto initial_times=clock.times;
        require(!vertical_inventory_exchange_split_item(recipient,source,selection,failing,ids,clock) && old->item.count==5 && source.retained_inventory_item(1,2)->item==old && recipient.inventory_slot_empty(1,3) && clock.ticks==0 && clock.times==initial_times,"Allocation failure mutated split or consumed UID/time");
        struct ThrowClock final:ItemIdClock {unsigned calls=0;std::uint32_t ticks_ms() override {return 100;}std::uint32_t packed_local_calendar() override {if(++calls==3) throw std::runtime_error("injected clock failure");return 0x12345678;}} throwing_clock;
        ItemIds throwing_ids(7,throwing_clock);bool threw=false;try {vertical_inventory_exchange_split_item(recipient,source,selection,items,throwing_ids,throwing_clock);}catch(const std::runtime_error&) {threw=true;}
        const auto originals=source.inventory();require(threw && old->item.count==5 && originals.size()==1 && originals[0].count==5 && source.retained_inventory_item(1,2)->item==old && recipient.inventory_slot_empty(1,3) && items.statistics().available==1 && first.client.empty() && second.client.empty(),"Native split exception leaked ownership or changed source mirror");
        auto* recycled=items.allocate();require(recycled && recycled!=old,"Native split exception did not return allocation to pool");items.release(*recycled);
    }
    {
        std::ifstream traces(argv[21]);require(traces.good(),"Cannot open exchange split-flow oracle");unsigned cases=0;
        struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override {return 100;}std::uint32_t packed_local_calendar() override {return pack_item_id_time(std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59});}};
        for(std::string line;std::getline(traces,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);std::uint64_t uid;std::uint32_t item_id,made;unsigned offered,bag,slot,remaining;std::string payload,db_before,out_before,receiver_before,in_before,db_add,db_remove,sender,out_log,receiver,in_log;
            row>>uid>>item_id>>offered>>bag>>slot>>payload>>made>>db_before>>out_before>>receiver_before>>in_before>>db_add>>db_remove>>sender>>out_log>>receiver>>in_log>>remaining;require(!row.fail(),"Invalid split-flow trace");const auto raw=unhex(payload);require(raw.size()==33,"Invalid split-flow item");
            GroundItemPool items;InventoryReplicaPool pool;InventoryReplica source_cache(12345,0,pool),recipient_cache(12346,0,pool);World first(source_cache,items),second(recipient_cache,items);std::string events;first.exchange_trace=&events;second.exchange_trace=&events;second.exchange_tag='B';SplitDelivery world(source_cache,recipient_cache,events);Actor source(1001,{}, {},{},first,{definitions,enchant,mobs}),recipient(1002,{}, {},{},second,{definitions,enchant,mobs});source.account_id()=12345;recipient.account_id()=12346;recipient.status().skills.max_bags=9;ItemDefinition definition;definition.id=item_id;definition.stack_count=20;
            auto* old=items.allocate();require(old,"Cannot allocate split-flow source");old->definition=&definition;old->item.uid=uid;old->item.made_at=made;old->item.type=raw[0];old->item.type_id=raw[1];old->item.count=raw[2];old->item.quality=static_cast<std::uint16_t>(raw[4]|(raw[5]<<8));std::copy_n(raw.begin()+6,6,old->item.gems.begin());std::copy_n(raw.begin()+12,21,old->craft_storage.begin());require(source.claim_inventory_item(1,2,*old,items),"Cannot retain split-flow source");require(source_cache.load(old->item),"Cannot seed source replica");source.exchange().items[0]={1,2,static_cast<std::uint8_t>(offered)};
            VerticalExchangeItemSelection selection{0,static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),old,&items,false};auto db=unhex(db_before),out=unhex(out_before),receive=unhex(receiver_before),in=unhex(in_before);Clock clock;ItemIds ids(7,clock);
            require(vertical_inventory_exchange_split_entry(recipient,source,selection,items,ids,clock,world,db,out,receive,in),"Split flow failed allocation");
            require(events==",D"+db_add+",D"+db_remove+",A"+sender+",G"+out_log+",B"+receiver+",G"+in_log,"Original split flow differs");const auto held=recipient.retained_inventory_item(bag,slot);const auto source_items=source_cache.inventory(),recipient_items=recipient_cache.inventory();require(old->item.count==remaining && held && source_items.size()==1 && source_items[0].item.count==remaining && recipient_items.size()==1 && recipient_items[0].item.uid==held->item->item.uid && recipient_items[0].item.count==offered,"Split actors and DB replicas diverged");++cases;
        }
        require(cases==64,"Missing original split-flow traces");
        std::ifstream failures(argv[19]);require(failures.good(),"Cannot reopen split-failure traces");unsigned failures_checked=0,guarded=0;
        for(std::string line;std::getline(failures,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);std::uint64_t uid;std::uint32_t item_id,made;unsigned offered,bag,slot,remaining;std::string payload,out_before,receiver_before,in_before,sender,out_log,receiver,in_log;
            row>>uid>>item_id>>offered>>bag>>slot>>payload>>made>>out_before>>receiver_before>>in_before>>sender>>out_log>>receiver>>in_log>>remaining;require(!row.fail(),"Invalid integrated failure trace");const auto raw=unhex(payload);
            GroundItemPool items;GroundItemPool failing(1,[]{return std::unique_ptr<GroundItem>{};});InventoryReplicaPool pool;InventoryReplica source_cache(12345,0,pool),recipient_cache(12346,0,pool);World first(source_cache,items),second(recipient_cache,items);std::string events;first.exchange_trace=&events;second.exchange_trace=&events;second.exchange_tag='B';SplitDelivery world(source_cache,recipient_cache,events);Actor source(1001,{}, {},{},first,{definitions,enchant,mobs}),recipient(1002,{}, {},{},second,{definitions,enchant,mobs});source.account_id()=12345;recipient.account_id()=12346;recipient.status().skills.max_bags=9;ItemDefinition definition;definition.id=item_id;definition.stack_count=20;
            auto* old=items.allocate();require(old,"Cannot allocate failure-flow source");old->definition=&definition;old->item.uid=uid;old->item.made_at=made;old->item.type=raw[0];old->item.type_id=raw[1];old->item.count=raw[2];old->item.quality=static_cast<std::uint16_t>(raw[4]|(raw[5]<<8));std::copy_n(raw.begin()+6,6,old->item.gems.begin());std::copy_n(raw.begin()+12,21,old->craft_storage.begin());require(source.claim_inventory_item(1,2,*old,items),"Cannot retain failure-flow source");require(source_cache.load(old->item),"Cannot seed failure-flow replica");source.exchange().items[0]={1,2,static_cast<std::uint8_t>(offered)};
            VerticalExchangeItemSelection selection{0,static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),old,&items,false};net::Packet db(57,0x5a);auto out=unhex(out_before),receive=unhex(receiver_before),in=unhex(in_before);Clock clock;ItemIds ids(7,clock);
            if(bag>=6) {bool rejected=false;try {vertical_inventory_exchange_split_entry(recipient,source,selection,failing,ids,clock,world,db,out,receive,in);}catch(const std::out_of_range&) {rejected=true;}require(rejected && events.empty() && old->item.count==remaining && source_cache.inventory()[0].item.count==remaining && recipient_cache.inventory().empty() && failing.statistics().fresh_attempts==0,"Unsupported destination mutated split owners");++guarded;continue;}
            require(!vertical_inventory_exchange_split_entry(recipient,source,selection,failing,ids,clock,world,db,out,receive,in),"Failure-flow allocation unexpectedly succeeded");require(events==",A"+sender+",G"+out_log+",B"+receiver+",G"+in_log,"Original allocation-failure flow differs");require(old->item.count==remaining && recipient.inventory_slot_empty(bag,slot) && source_cache.inventory().size()==1 && source_cache.inventory()[0].item.count==remaining && recipient_cache.inventory().empty() && std::all_of(db.begin(),db.end(),[](auto b){return b==0x5a;}),"Failed split changed actors, DB replicas or DB scratch");++failures_checked;
        }
        require(failures_checked+guarded==64 && guarded>0 && failures_checked>0,"Missing integrated allocation-failure or guarded traces");
    }
    {
        struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override {return 100;}std::uint32_t packed_local_calendar() override {return pack_item_id_time(std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59});}};
        GroundItemPool items;InventoryReplicaPool pool;InventoryReplica source_cache(12345,0,pool),recipient_cache(12346,0,pool);World first(source_cache,items),second(recipient_cache,items);std::string events;first.exchange_trace=&events;second.exchange_trace=&events;second.exchange_tag='B';SplitDelivery world(source_cache,recipient_cache,events);Actor source(1001,{}, {},{},first,{definitions,enchant,mobs}),recipient(1002,{}, {},{},second,{definitions,enchant,mobs});source.account_id()=12345;recipient.account_id()=12346;recipient.status().skills.max_bags=1;ItemDefinition definition;definition.id=5001;definition.stack_count=20;
        for(unsigned i=0;i<2;++i) {auto* item=items.allocate();require(item,"Cannot allocate loop source");item->definition=&definition;item->item.uid=100+i;item->item.type=1;item->item.type_id=2+i;item->item.count=10;require(source.claim_inventory_item(1,i,*item,items),"Cannot retain loop source");require(source_cache.load(item->item),"Cannot seed loop replica");source.exchange().items[i*2]={1,static_cast<std::uint8_t>(i),static_cast<std::uint8_t>(2+i)};}
        source.exchange().items[1]={1,7,1};Clock clock;ItemIds ids(7,clock);NoWholeExchange whole;net::Packet db(57,0),out(181,0),receive(36,0),in(179,0);
        require(vertical_inventory_exchange_db_compatible(recipient,source),"Supported DB bags incorrectly rejected");recipient.status().skills.max_bags=6;require(!vertical_inventory_exchange_db_compatible(recipient,source),"Unsupported bag maximum accepted");bool rejected=false;try {vertical_inventory_exchange_transfer_items(recipient,source,items,ids,clock,world,whole,db,out,receive,in);}catch(const std::out_of_range&) {rejected=true;}require(rejected && events.empty() && source_cache.inventory()[0].item.count==10 && recipient_cache.inventory().empty(),"Unsupported maximum caused a partial exchange");recipient.status().skills.max_bags=1;source.exchange().items[7]={6,0,1};require(!vertical_inventory_exchange_db_compatible(recipient,source),"Unsupported offered bag accepted");source.exchange().items[7]={};
        vertical_inventory_exchange_transfer_items(recipient,source,items,ids,clock,world,whole,db,out,receive,in);
        const auto originals=source_cache.inventory(),received=recipient_cache.inventory();require(originals.size()==2 && originals[0].item.count==8 && originals[1].item.count==7 && received.size()==2 && received[0].item.count==2 && received[1].item.count==3 && recipient.retained_inventory_item(1,0) && recipient.retained_inventory_item(1,1),"Ordered split loop did not update real owners");
        require(std::count(events.begin(),events.end(),',')==12 && first.client.size()==2 && second.client.size()==2,"Ordered split loop sent incorrect event count");
    }
    {
        std::ifstream traces(argv[22]);require(traces.good(),"Cannot open DB bag-boundary oracle");unsigned cases=0;
        for(std::string line;std::getline(traces,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned bag,slot,index,old_count,add_count,old_save,original_count,original_save;std::string packet_hex;
            row>>bag>>slot>>index>>old_count>>add_count>>old_save>>original_count>>original_save>>packet_hex;require(!row.fail(),"Invalid DB bag-boundary trace");const auto packet=unhex(packet_hex);require(packet.size()==57 && bag>=6 && bag<=9 && index==(bag-6)*24+slot && original_count==static_cast<std::uint8_t>(old_count+add_count) && original_save==(old_save==0?2U:old_save),"Original DB bank alias differs");
            InventoryReplicaPool pool;InventoryReplica replica(12345,0,pool);PlayerItem bank;bank.uid=17;bank.id=5001;bank.slot=index;bank.type=packet[20];bank.type_id=packet[21];bank.count=old_count;require(replica.load(bank,true,static_cast<std::int32_t>(old_save)),"Cannot load bank boundary item");
            require(replica.apply(packet)==InventoryReplicaResult::rejected,"DB replica accepted native bank alias");const auto retained=replica.bank();require(retained.size()==1 && retained[0].item.slot==index && retained[0].item.uid==17 && retained[0].item.count==old_count && retained[0].save_type==static_cast<std::int32_t>(old_save),"Rejected bag corrupted retained bank");++cases;
        }
        require(cases==128,"Missing DB bag-boundary original traces");
    }
    {
        GroundItemPool items;InventoryReplicaPool pool;InventoryReplica first_cache(12345,10,pool),second_cache(12346,20,pool);World first(first_cache,items),second(second_cache,items);std::string trace;first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.account_id()=12345;peer.account_id()=12346;actor.money()=10;peer.money()=20;actor.exchange().money=3;actor.status().skills.max_bags=1;peer.status().skills.max_bags=1;
        ExchangeMoneyWorld money(first_cache,second_cache);EmptyExchangeTransfer transfers(actor,peer,trace);net::Packet outgoing(126,0),incoming(111,0);const std::uint32_t field=0x12345678;VerticalExchangeSettlementExecutor executor(money,transfers,outgoing,incoming,field);
        executor.settle_exchange(actor,peer);require(actor.money()==7 && peer.money()==23 && first_cache.money()==7 && second_cache.money()==23,"Settlement money did not reach actor and DB owners");
        require(money.events.size()==6 && money.events[0]=="T" && money.events[1].starts_with("G0408") && money.events[2]=="T" && money.events[3].starts_with("G0308") && money.events[4].starts_with("D0306") && money.events[5].starts_with("D0306"),"Settlement money events differ");require(trace==",TAB,TBA,A040a00,B040a00","Settlement item/completion ordering differs");
    }
    {
        GroundItemPool items;InventoryReplicaPool pool;InventoryReplica first_cache(12345,10,pool),second_cache(12346,20,pool);World first(first_cache,items),second(second_cache,items);std::string trace;first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.account_id()=12345;peer.account_id()=12346;actor.money()=10;peer.money()=20;peer.exchange().money=21;actor.status().skills.max_bags=1;peer.status().skills.max_bags=1;
        ExchangeMoneyWorld money(first_cache,second_cache);EmptyExchangeTransfer transfers(actor,peer,trace);net::Packet outgoing(126,0),incoming(111,0);const std::uint32_t field=0;VerticalExchangeSettlementExecutor executor(money,transfers,outgoing,incoming,field);
        actor.status().skills.max_bags=6;bool rejected=false;try {executor.settle_exchange(actor,peer);}catch(const std::out_of_range&) {rejected=true;}require(rejected && trace.empty() && money.events.empty() && actor.money()==10 && peer.money()==20,"Unsafe bag changed settlement state");actor.status().skills.max_bags=1;
        executor.settle_exchange(actor,peer);require(trace==",A040a06,B040a07" && money.events.empty() && actor.money()==10 && peer.money()==20 && first_cache.money()==10 && second_cache.money()==20,"Preflight rejection reached settlement services");
    }
    {
        struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override {return 100;}std::uint32_t packed_local_calendar() override {return pack_item_id_time(std::chrono::sys_days{std::chrono::year{2026}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59});}};
        GroundItemPool items;InventoryReplicaPool pool;PlayerCore actor_core,peer_core;actor_core.user_id=12345;actor_core.money=10;peer_core.user_id=12346;peer_core.money=20;CharacterReplica actor_replica(actor_core,pool),peer_replica(peer_core,pool);auto& actor_cache=actor_replica.inventory();auto& peer_cache=peer_replica.inventory();std::recursive_mutex actor_mutex,peer_mutex;World first(actor_cache,items),second(peer_cache,items);std::string trace,shop;first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';SplitDelivery world(actor_cache,peer_cache,trace);VerticalInventoryReplicaWorld delivery(world,12345,actor_replica,actor_mutex,12346,peer_replica,peer_mutex);Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.account_id()=12345;peer.account_id()=12346;actor.money()=10;peer.money()=20;actor.status().skills.max_bags=1;peer.status().skills.max_bags=1;
        ItemDefinition actor_def,peer_def;actor_def.id=5001;peer_def.id=5002;actor_def.stack_count=20;peer_def.stack_count=20;auto* actor_item=items.allocate();auto* peer_item=items.allocate();require(actor_item && peer_item,"Cannot allocate bidirectional source items");actor_item->definition=&actor_def;actor_item->item.uid=101;actor_item->item.type=1;actor_item->item.type_id=1;actor_item->item.count=10;peer_item->definition=&peer_def;peer_item->item.uid=102;peer_item->item.type=1;peer_item->item.type_id=2;peer_item->item.count=8;
        require(actor.claim_inventory_item(1,2,*actor_item,items) && peer.claim_inventory_item(1,3,*peer_item,items) && actor_cache.load(actor_item->item) && peer_cache.load(peer_item->item),"Cannot retain bidirectional items and replicas");actor.exchange().mode=3;peer.exchange().mode=3;actor.exchange().peer=&peer;peer.exchange().peer=&actor;peer.exchange().ready=1;actor.exchange().money=3;peer.exchange().money=2;actor.exchange().items[0]={1,2,3};peer.exchange().items[0]={1,3,2};actor.exchange().items[1]={1,7,1};
        const auto expect_delivery=[&](const net::Packet& packet,InventoryReplicaResult expected) {bool matched=false;try {delivery.send_inventory_database(packet);}catch(const VerticalInventoryDeliveryError& e) {matched=e.result()==expected;}require(matched,"Replica delivery hid real failure");};
        expect_delivery(net::Packet{1,7},InventoryReplicaResult::rejected);expect_delivery(money_database_packet(999,1),InventoryReplicaResult::rejected);expect_delivery(net::Packet{0xff,0xff,0x39,0x30,0,0},InventoryReplicaResult::unsupported);
        GroundItem collision;collision.definition=&actor_def;collision.item.uid=777;collision.item.type=9;collision.item.type_id=1;collision.item.count=1;expect_delivery(inventory_add_packet(12345,1,2,collision),InventoryReplicaResult::disconnect);require(actor_cache.inventory().size()==1 && actor_cache.inventory()[0].item.uid==101 && actor_cache.inventory()[0].item.count==10 && peer_cache.inventory().size()==1 && trace.empty(),"Rejected delivery changed actual replicas or emitted events");
        Clock clock;ItemIds ids(7,clock);NoWholeExchange whole;net::Packet db(57,0),out_item(181,0),receipt(36,0),in_item(179,0),out_money(126,0),in_money(111,0);VerticalExchangeItemTransferExecutor transfers(items,ids,clock,delivery,whole,db,out_item,receipt,in_item);const std::uint32_t field=0x12345678;VerticalExchangeSettlementExecutor settlement(delivery,transfers,out_money,in_money,field);ExchangeActions actions(shop);const net::Packet request{5,0x0a,0};
        require(vertical_inventory_exchange_guarded_ready(actor,request,actions,settlement)==VerticalExchangeRequest::updated && shop=="E","Guarded bidirectional ready request did not settle");const auto actor_received=actor.retained_inventory_item(1,0),peer_received=peer.retained_inventory_item(1,0);const auto a=actor_cache.inventory(),b=peer_cache.inventory();
        require(actor.money()==9 && peer.money()==21 && actor_cache.money()==9 && peer_cache.money()==21 && actor_item->item.count==7 && peer_item->item.count==6 && actor_received && peer_received && actor_received->item->item.count==2 && peer_received->item->item.count==3 && a.size()==2 && b.size()==2 && a[0].item.uid==actor_received->item->item.uid && a[1].item.uid==101 && a[1].item.count==7 && b[0].item.uid==peer_received->item->item.uid && b[1].item.uid==102 && b[1].item.count==6,"Bidirectional split actor and replica states differ");
        require(actor.exchange().mode==0 && peer.exchange().mode==0 && first.client.front()==net::Packet({5,0x0a,1,0}) && second.client.front()==net::Packet({5,0x0a,2,0}) && first.client.back()==net::Packet({4,0x0a,0}) && second.client.back()==net::Packet({4,0x0a,0}),"Bidirectional settlement did not complete original ready sequence");
    }
    {
        struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override{return 100;}std::uint32_t packed_local_calendar() override{return 0x12345678;}} clock;
        GroundItemPool items;InventoryReplicaPool pool;PlayerCore ac,bc;ac.user_id=12345;ac.money=10;bc.user_id=12346;bc.money=20;
        CharacterReplica a(ac,pool),b(bc,pool);std::recursive_mutex am,bm;World first(a.inventory(),items),second(b.inventory(),items);std::string trace,shop;
        first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';SplitDelivery services(a.inventory(),b.inventory(),trace);VerticalInventoryReplicaWorld delivery(services,12345,a,am,12346,b,bm);
        Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.account_id()=12345;peer.account_id()=12346;actor.money()=10;peer.money()=20;actor.status().skills.max_bags=peer.status().skills.max_bags=1;
        ItemDefinition whole_def,split_def;whole_def.id=5001;whole_def.stack_count=20;split_def.id=5002;split_def.stack_count=20;
        auto* whole_item=items.allocate();auto* split_item=items.allocate();require(whole_item && split_item,"Cannot allocate mixed settlement items");
        whole_item->definition=&whole_def;whole_item->item.uid=101;whole_item->item.type=1;whole_item->item.type_id=1;whole_item->item.count=10;split_item->definition=&split_def;split_item->item.uid=102;split_item->item.type=1;split_item->item.type_id=2;split_item->item.count=8;
        require(actor.claim_inventory_item(1,2,*whole_item,items) && peer.claim_inventory_item(1,3,*split_item,items) && a.inventory().load(whole_item->item) && b.inventory().load(split_item->item),"Cannot retain mixed settlement items");
        actor.exchange().mode=peer.exchange().mode=3;actor.exchange().peer=&peer;peer.exchange().peer=&actor;peer.exchange().ready=1;actor.exchange().money=3;peer.exchange().money=2;actor.exchange().items[0]={1,2,10};peer.exchange().items[0]={1,3,2};
        ItemIds ids(7,clock);net::Packet db(57),out(181),receive(36),in(179),money_out(126),money_in(111);VerticalExchangeWholeItemTransferExecutor whole(delivery,db,out,receive,in);
        VerticalExchangeItemTransferExecutor transfers(items,ids,clock,delivery,whole,db,out,receive,in);const std::uint32_t field=0x12345678;VerticalExchangeSettlementExecutor settlement(delivery,transfers,money_out,money_in,field);ExchangeActions actions(shop);
        require(vertical_inventory_exchange_guarded_ready(actor,net::Packet{5,0x0a,0},actions,settlement)==VerticalExchangeRequest::updated && shop=="E","Mixed settlement did not execute");
        const auto received_split=actor.retained_inventory_item(1,0),received_whole=peer.retained_inventory_item(1,0);const auto ar=a.inventory().inventory(),br=b.inventory().inventory();
        require(actor.money()==9 && peer.money()==21 && a.core().money==9 && b.core().money==21 && actor.inventory_slot_empty(1,2) && received_split && received_split->item!=split_item && received_split->item->item.count==2 && received_whole && received_whole->item==whole_item && whole_item->item.uid==101 && split_item->item.count==6,"Mixed settlement actor ownership differs");
        require(ar.size()==1 && ar[0].item.uid==received_split->item->item.uid && ar[0].item.count==2 && br.size()==2 && br[0].item.uid==101 && br[1].item.uid==102 && br[1].item.count==6 && items.statistics().owned==3 && actor.exchange().mode==0 && peer.exchange().mode==0,"Mixed settlement replicas or exchange state differ");
        require(first.client.front()==net::Packet({5,0x0a,1,0}) && second.client.front()==net::Packet({5,0x0a,2,0}) && first.client.back()==net::Packet({4,0x0a,0}) && second.client.back()==net::Packet({4,0x0a,0}),"Mixed settlement client ready/completion differs");
    }
    {
        GroundItemPool items;InventoryReplicaPool pool;InventoryReplica actor_cache(12345,0,pool),peer_cache(12346,0,pool);World first(actor_cache,items),second(peer_cache,items);Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.exchange().mode=3;peer.exchange().mode=3;actor.exchange().peer=&peer;peer.exchange().peer=&actor;actor.status().skills.max_bags=6;peer.status().skills.max_bags=1;std::string shop;ExchangeActions actions(shop);RejectingSettlement settlement;const net::Packet request{5,0x0a,0};
        for(unsigned peer_ready:{0U,1U}) {peer.exchange().ready=peer_ready;bool rejected=false;try {vertical_inventory_exchange_guarded_ready(actor,request,actions,settlement);}catch(const std::out_of_range&) {rejected=true;}require(rejected && shop.empty() && first.client.empty() && second.client.empty() && actor.exchange().ready==0 && actor.exchange().mode==3 && peer.exchange().ready==peer_ready && settlement.calls==0,"Incompatible ready caused observable effects");}
        struct ChangingBag final:VerticalInventoryActions {bool called=false;void cancel_action_exchange(Actor&) override {throw std::runtime_error("Unexpected cancellation");}bool pickup_checks() override {throw std::runtime_error("Unexpected pickup");}void end_shop(Actor& value) override {called=true;value.status().skills.max_bags=6;}} changing;
        actor.status().skills.max_bags=1;peer.exchange().ready=1;peer.money()=0;peer.exchange().money=1;bool rejected=false;try {vertical_inventory_exchange_guarded_ready(actor,request,changing,settlement);}catch(const std::out_of_range&) {rejected=true;}require(rejected && changing.called && actor.exchange().ready==0 && actor.exchange().mode==3 && first.client.empty() && second.client.empty() && settlement.calls==0,"Shop mutation bypassed ready DB guard");
        actor.status().skills.max_bags=1;peer.money()=0;peer.exchange().money=1;require(vertical_inventory_exchange_guarded_ready(actor,request,actions,settlement)==VerticalExchangeRequest::updated && settlement.calls==1 && actor.exchange().mode==0 && peer.exchange().mode==0,"Compatible guarded ready did not execute real rejection");
    }
    {
        std::ifstream traces(argv[23]);require(traces.good(),"Cannot open exchange IPv4 oracle");unsigned cases=0;
        GameLogSubject actor;actor.account_name="account-a";actor.character_name="Actor";GameLogSubject peer;peer.account_name="account-b";peer.character_name="Peer";
        const auto calendar=std::chrono::sys_days{std::chrono::year{2026}/11/2};
        for(std::string line;std::getline(traces,line);) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);std::string bytes;std::uint32_t expected;row>>bytes>>expected;require(!row.fail(),"Invalid IPv4 oracle");const auto raw=unhex(bytes);require(raw.size()==4,"Invalid IPv4 octets");std::array<std::uint8_t,4> octets{};std::copy(raw.begin(),raw.end(),octets.begin());const auto field=vertical_inventory_exchange_ipv4_field(octets);require(field==expected,"Original SInternetAddress::GetIP differs");
            net::Packet incoming(111,0xa5);vertical_inventory_exchange_money_log(incoming,VerticalExchangeMoneyLog::incoming,actor,peer,1,2,field,calendar);require(std::equal(octets.begin(),octets.end(),incoming.begin()+107),"Money log did not preserve peer IPv4 octets");++cases;
        }
        require(cases==256,"Missing original exchange IPv4 traces");
    }
    {
        GroundItemPool items;InventoryReplicaPool pool;InventoryReplica cache(12345,0,pool);World world(cache,items);Actor actor(9001,{}, {},{},world,{definitions,enchant,mobs});actor.account_id()=12345;actor.status().skills.max_bags=6;ItemDefinition definition;definition.id=5001;auto* item=items.allocate();require(item,"Cannot allocate unsupported-bag item");item->definition=&definition;item->item.uid=42;item->item.count=1;require(actor.claim_inventory_item(6,0,*item,items),"Cannot retain unsupported-bag item");PlayerItem bank;bank.uid=77;bank.slot=0;bank.count=9;require(cache.load(bank,true),"Cannot retain bank boundary item");
        std::vector<std::uint8_t> request{6,2,6,0,1};DropActions direct(request,0,{6,0,1});bool rejected=false;try {vertical_inventory_guarded_drop_request(actor,request,world,items,direct);}catch(const std::out_of_range&) {rejected=true;}require(rejected && direct.events.empty() && actor.retained_inventory_item(6,0)->item==item && cache.bank().size()==1 && cache.bank()[0].item.uid==77 && cache.bank()[0].item.count==9 && world.database.empty() && world.client.empty(),"Unsupported drop changed actor, bank or callbacks");
        actor.attack_state().mode=1;require(vertical_inventory_guarded_drop_request(actor,request,world,items,direct).request==VerticalInventoryRequest::ignored && direct.events.empty(),"Ignored original drop was blocked");actor.attack_state().mode=0;
        for(unsigned mutation:{1U,2U}) {request={6,2,1,0,1};DropActions changing(request,mutation,{6,0,1});rejected=false;try {vertical_inventory_guarded_drop_request(actor,request,world,items,changing);}catch(const std::out_of_range&) {rejected=true;}require(rejected && changing.events=="C,S" && actor.retained_inventory_item(6,0)->item==item && cache.bank().size()==1 && cache.bank()[0].item.uid==77 && cache.bank()[0].item.count==9 && world.database.empty() && world.client.empty(),"Reloaded unsupported drop changed actor or bank");}
    }
    {
        GroundItemPool items;InventoryReplicaPool pool;InventoryReplica cache(12345,0,pool);World world(cache,items);ZoneItems zone(geometry,world);world.zone=&zone;Actor actor(9001,{}, {},{},world,{definitions,enchant,mobs});actor.account_id()=12345;actor.status().skills.max_bags=6;ItemDefinition definition;definition.id=5001;definition.type=5;definition.type_id=1;auto* item=items.allocate();require(item,"Cannot allocate guarded pickup item");item->id=42;item->definition=&definition;item->item.uid=88;item->item.type=5;item->item.type_id=1;item->item.count=1;item->position={10,0,10};require(zone.enter(*item,1),"Cannot enter guarded pickup item");PlayerItem bank;bank.uid=77;bank.slot=0;bank.count=9;require(cache.load(bank,true),"Cannot retain guarded pickup bank item");
        std::vector<std::uint8_t> request{5,2,42,0,0,0};Actions direct(request,zone,0,0,false);bool rejected=false;try {vertical_inventory_guarded_pickup_request(actor,request,zone,links,world,items,direct);}catch(const std::out_of_range&) {rejected=true;}require(rejected && direct.events.empty() && zone.find(42)==item && actor.inventory().empty() && cache.bank().size()==1 && cache.bank()[0].item.count==9 && world.database.empty() && world.client.empty(),"Unsupported pickup changed zone, actor, bank or callbacks");
        struct ChangingPickup final:VerticalInventoryActions {Actor& actor;std::vector<std::uint8_t>& packet;unsigned step;std::string events;ChangingPickup(Actor& a,std::vector<std::uint8_t>& p,unsigned s):actor(a),packet(p),step(s) {}void cancel_action_exchange(Actor&) override {events="C";}void end_shop(Actor&) override {events+=",S";if(step==1) actor.status().skills.max_bags=6;}bool pickup_checks() override {events+=",P";if(step>=2) actor.status().skills.max_bags=6;if(step==3) packet[2]=0;return true;}};
        for(unsigned step:{1U,2U,3U}) {actor.status().skills.max_bags=5;request={5,2,42,0,0,0};ChangingPickup changing(actor,request,step);rejected=false;try {vertical_inventory_guarded_pickup_request(actor,request,zone,links,world,items,changing);}catch(const std::out_of_range&) {rejected=true;}require(rejected && changing.events=="C,S,P" && zone.find(42)==item && actor.inventory().empty() && cache.bank()[0].item.count==9 && world.database.empty() && world.client.empty(),"Pickup callback bypassed selected-item DB guard");}
        actor.status().skills.max_bags=5;request={5,2,42,0,0,0};Actions valid(request,zone,0,0,false);const auto result=vertical_inventory_guarded_pickup_request(actor,request,zone,links,world,items,valid);const auto held=actor.retained_inventory_item(1,0);require(result.receipt==InventoryReceipt::stored && zone.size()==0 && held && held->item==item && cache.inventory().size()==1 && cache.inventory()[0].item.uid==88 && cache.bank()[0].item.count==9,"Compatible guarded pickup did not reach real actor and replica");
    }
    {
        GroundItemPool items;InventoryReplicaPool failing([] {return std::unique_ptr<CachedInventoryItem>{};});PlayerCore a_core,b_core;a_core.user_id=12345;b_core.user_id=12346;CharacterReplica a(a_core,failing),b(b_core,failing);std::recursive_mutex a_mutex,b_mutex;World services(a.inventory(),items);VerticalInventoryReplicaWorld delivery(static_cast<ItemPickupWorld&>(services),12345,a,a_mutex,12346,b,b_mutex);
        ItemDefinition definition;definition.id=5001;GroundItem item;item.definition=&definition;item.item.uid=42;item.item.type=5;item.item.type_id=1;item.item.count=1;const auto add=inventory_add_packet(12345,1,2,item);bool rejected=false;
        try {delivery.send_inventory_database(add);}catch(const VerticalInventoryInsertionError&) {rejected=true;}require(rejected && a.inventory().inventory().empty() && b.inventory().inventory().empty() && failing.statistics().attempts==1 && services.database.empty(),"Silent DB item allocation failure was reported as success");
        bool mismatch=false;try {VerticalInventoryReplicaWorld invalid(static_cast<ItemPickupWorld&>(services),999,a,a_mutex,12346,b,b_mutex);}catch(const std::invalid_argument&) {mismatch=true;}require(mismatch,"Replica account binding mismatch was accepted");
        struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override {return 100;}std::uint32_t packed_local_calendar() override {return 0x12345678;}} clock;ItemIds ids(7,clock);Actor source(1001,{}, {},{},services,{definitions,enchant,mobs}),recipient(1002,{}, {},{},services,{definitions,enchant,mobs});source.account_id()=12345;recipient.account_id()=12346;recipient.status().skills.max_bags=1;definition.stack_count=20;auto* original=items.allocate();require(original,"Cannot allocate partial-failure source");original->definition=&definition;original->item.uid=99;original->item.type=5;original->item.type_id=1;original->item.count=10;require(source.claim_inventory_item(1,2,*original,items),"Cannot retain partial-failure source");source.exchange().items[0]={1,2,3};VerticalExchangeItemSelection selection{0,1,0,original,&items,false};net::Packet db(57),out(181),receiver(36),in(179);rejected=false;
        try {vertical_inventory_exchange_split_entry(recipient,source,selection,items,ids,clock,delivery,db,out,receiver,in);}catch(const VerticalInventoryInsertionError&) {rejected=true;}const auto retained=recipient.retained_inventory_item(1,0);require(rejected && original->item.count==7 && retained && retained->item->item.count==3 && a.inventory().inventory().empty() && b.inventory().inventory().empty() && services.database.empty() && services.client.empty() && services.logs.empty(),"Split insertion failure was hidden or emitted later success effects");
    }
    {
        GroundItemPool items;InventoryReplicaPool pool;PlayerCore a_core,b_core;a_core.user_id=12345;b_core.user_id=12346;CharacterReplica a(a_core,pool),b(b_core,pool);std::recursive_mutex a_mutex,b_mutex;World services(a.inventory(),items);VerticalInventoryReplicaWorld delivery(static_cast<ItemPickupWorld&>(services),12345,a,a_mutex,12346,b,b_mutex);
        PlayerItem item;item.uid=42;item.id=5001;item.bag=1;item.slot=0;item.type=5;item.type_id=1;item.count=5;require(a.inventory().load(item),"Cannot seed removal replica");const ExchangeItemReference offer{1,0,2};const auto removal=vertical_inventory_exchange_db_remove_packet(12345,offer,2);for(const auto [uid,count]:{std::pair<std::uint64_t,std::uint8_t>{43,5},std::pair<std::uint64_t,std::uint8_t>{42,4}}) {bool mismatch=false;try {delivery.send_exchange_removal(removal,uid,count);}catch(const VerticalInventorySourceMismatch&) {mismatch=true;}require(mismatch && a.inventory().inventory()[0].item.uid==42 && a.inventory().inventory()[0].item.count==5,"Mismatched DB source was mutated");}delivery.send_exchange_removal(removal,42,5);require(a.inventory().inventory().size()==1 && a.inventory().inventory()[0].item.count==3,"Real partial DB removal failed");delivery.send_exchange_removal(vertical_inventory_exchange_db_remove_packet(12345,offer,3),42,3);require(a.inventory().inventory().empty() && a.inventory().deleted_inventory()==std::vector<std::uint64_t>{42},"Real whole DB removal failed");
        bool absent=false;try {delivery.send_exchange_removal(vertical_inventory_exchange_db_remove_packet(12345,offer,1),42,3);}catch(const VerticalInventoryRemovalError&) {absent=true;}require(absent && a.inventory().inventory().empty() && a.inventory().deleted_inventory()==std::vector<std::uint64_t>{42},"Missing DB source was treated as a successful removal");
        item.uid=44;item.count=5;require(a.inventory().load(item),"Cannot seed occupied exchange destination");net::Packet insertion(57);insertion[0]=1;insertion[1]=7;insertion[2]=0x39;insertion[3]=0x30;insertion[6]=55;insertion[18]=1;insertion[19]=0;insertion[20]=5;insertion[21]=1;insertion[22]=3;bool occupied=false;try {delivery.send_exchange_insertion(insertion);}catch(const VerticalInventoryDestinationOccupied&) {occupied=true;}auto rows=a.inventory().inventory();require(occupied && rows.size()==1 && rows[0].item.uid==44 && rows[0].item.count==5,"Occupied exchange destination was merged or replaced");
        insertion[19]=1;delivery.send_exchange_insertion(insertion);rows=a.inventory().inventory();require(rows.size()==2 && std::any_of(rows.begin(),rows.end(),[](const auto& row){return row.item.bag==1 && row.item.slot==1 && row.item.uid==55 && row.item.count==3;}),"Empty exchange destination did not retain requested UID");
        const ExchangeItemReference invalid{6,0,1};bool rejected=false;try {delivery.send_inventory_database(vertical_inventory_exchange_db_remove_packet(12345,invalid,1));}catch(const VerticalInventoryDeliveryError& e) {rejected=e.result()==InventoryReplicaResult::rejected;}require(rejected,"Malformed DB bag lost its real rejection result");
    }
    {
        struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override {return 100;}std::uint32_t packed_local_calendar() override {return 0x12345678;}} clock;
        GroundItemPool items;InventoryReplicaPool pool;PlayerCore ac,bc;ac.user_id=12345;bc.user_id=12346;CharacterReplica a(ac,pool),b(bc,pool);std::recursive_mutex am,bm;World first(a.inventory(),items),second(b.inventory(),items);VerticalInventoryReplicaWorld delivery(static_cast<ItemPickupWorld&>(first),12345,a,am,12346,b,bm);Actor source(1001,{}, {},{},first,{definitions,enchant,mobs}),recipient(1002,{}, {},{},second,{definitions,enchant,mobs});source.account_id()=12345;recipient.account_id()=12346;recipient.status().skills.max_bags=1;ItemDefinition definition;definition.id=5001;definition.stack_count=20;auto* original=items.allocate();require(original,"Cannot allocate occupied-destination source");original->definition=&definition;original->item.uid=99;original->item.type=5;original->item.type_id=1;original->item.count=10;require(source.claim_inventory_item(1,2,*original,items) && a.inventory().load(original->item),"Cannot retain occupied-destination source");PlayerItem stale=original->item;stale.uid=123;stale.bag=1;stale.slot=0;stale.count=5;require(b.inventory().load(stale),"Cannot retain occupied destination replica");source.exchange().items[0]={1,2,3};VerticalExchangeItemSelection selection{0,1,0,original,&items,false};net::Packet db(57),out(181),receiver(36),in(179);ItemIds ids(7,clock);bool occupied=false;try {vertical_inventory_exchange_split_entry(recipient,source,selection,items,ids,clock,delivery,db,out,receiver,in);}catch(const VerticalInventoryDestinationOccupied&) {occupied=true;}const auto rows=b.inventory().inventory();require(occupied && rows.size()==1 && rows[0].item.uid==123 && rows[0].item.count==5 && a.inventory().inventory()[0].item.count==10 && original->item.count==10 && recipient.inventory_slot_empty(1,0) && items.statistics().owned==1 && first.client.empty() && second.client.empty(),"Occupied replica destination changed actor ownership or emitted effects");
    }
    {
        struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override {return 100;}std::uint32_t packed_local_calendar() override {return 0x12345678;}} clock;
        GroundItemPool items;InventoryReplicaPool pool;PlayerCore a_core,b_core;a_core.user_id=12345;b_core.user_id=12346;CharacterReplica a(a_core,pool),b(b_core,pool);std::recursive_mutex a_mutex,b_mutex;World first(a.inventory(),items),second(b.inventory(),items);std::string trace;first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';SplitDelivery services(a.inventory(),b.inventory(),trace);VerticalInventoryReplicaWorld delivery(services,12345,a,a_mutex,12346,b,b_mutex);Actor source(1001,{}, {},{},first,{definitions,enchant,mobs}),recipient(1002,{}, {},{},second,{definitions,enchant,mobs});source.account_id()=12345;recipient.account_id()=12346;recipient.status().skills.max_bags=1;ItemDefinition definition;definition.id=5001;definition.stack_count=20;auto* original=items.allocate();require(original,"Cannot allocate missing-replica source");original->definition=&definition;original->item.uid=99;original->item.type=5;original->item.type_id=1;original->item.count=10;require(source.claim_inventory_item(1,2,*original,items),"Cannot retain missing-replica source");source.exchange().items[0]={1,2,3};VerticalExchangeItemSelection selection{0,1,0,original,&items,false};net::Packet db(57),out(181),receiver(36),in(179);ItemIds ids(7,clock);bool absent=false;
        try {vertical_inventory_exchange_split_entry(recipient,source,selection,items,ids,clock,delivery,db,out,receiver,in);}catch(const VerticalInventoryRemovalError&) {absent=true;}const auto transferred=recipient.retained_inventory_item(1,0);const auto cached=b.inventory().inventory();require(absent && original->item.count==7 && transferred && transferred->item->item.count==3 && a.inventory().inventory().empty() && cached.size()==1 && cached[0].item.uid==transferred->item->item.uid && cached[0].item.count==3 && trace.empty() && first.client.empty() && second.client.empty(),"Missing source replica was hidden or emitted later success effects");
    }
    {
        std::ifstream traces(argv[24]);require(traces.good(),"Cannot open whole-exchange flow oracle");unsigned cases=0;
        for(std::string line;std::getline(traces,line);) {
            if(line.empty() || line[0]=='#')continue;
            std::istringstream row(line);std::uint64_t uid;std::uint32_t item_id,made;unsigned offered,bag,slot,final_count;
            std::string payload,db_before,out_before,receiver_before,in_before,db_add,db_remove,sender,out_log,receiver,in_log;
            row>>uid>>item_id>>offered>>bag>>slot>>payload>>made>>db_before>>out_before>>receiver_before>>in_before>>db_add>>db_remove>>sender>>out_log>>receiver>>in_log>>final_count;require(!row.fail(),"Invalid whole-exchange flow trace");
            const auto raw=unhex(payload);require(raw.size()==33,"Invalid whole-exchange item payload");
            GroundItemPool items;InventoryReplicaPool pool;InventoryReplica source_cache(12345,0,pool),recipient_cache(12346,0,pool);World first(source_cache,items),second(recipient_cache,items);std::string events;first.exchange_trace=&events;second.exchange_trace=&events;second.exchange_tag='B';SplitDelivery world(source_cache,recipient_cache,events);
            Actor source(1001,{}, {},{},first,{definitions,enchant,mobs}),recipient(1002,{}, {},{},second,{definitions,enchant,mobs});source.account_id()=12345;recipient.account_id()=12346;recipient.status().skills.max_bags=5;
            ItemDefinition definition;definition.id=item_id;definition.stack_count=20;auto* item=items.allocate();require(item,"Cannot allocate whole-flow source");item->definition=&definition;item->item.uid=uid;item->item.made_at=made;item->item.type=raw[0];item->item.type_id=raw[1];item->item.count=raw[2];item->item.quality=static_cast<std::uint16_t>(raw[4]|(raw[5]<<8));std::copy_n(raw.begin()+6,6,item->item.gems.begin());std::copy_n(raw.begin()+12,21,item->craft_storage.begin());
            require(source.claim_inventory_item(1,2,*item,items) && source_cache.load(item->item),"Cannot seed whole-flow source");source.exchange().items[0]={1,2,static_cast<std::uint8_t>(offered)};
            auto db=unhex(db_before),out=unhex(out_before),receive=unhex(receiver_before),in=unhex(in_before);VerticalExchangeWholeItemTransferExecutor whole(world,db,out,receive,in);
            VerticalExchangeItemSelection selection{0,static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),item,&items,true};whole.transfer_whole_exchange_item(recipient,source,selection);
            require(events==",D"+db_add+",D"+db_remove+",A"+sender+",G"+out_log+",B"+receiver+",G"+in_log,"Original whole exchange flow differs");
            const auto held=recipient.retained_inventory_item(bag,slot);const auto rows=recipient_cache.inventory();require(source.inventory_slot_empty(1,2) && held && held->item==item && held->pool==&items && item->item.count==final_count && source_cache.inventory().empty() && rows.size()==1 && rows[0].item.uid==uid,"Whole flow lost actor or replica ownership");++cases;
        }
        require(cases==64,"Missing original whole-flow traces");
    }
    {
        GroundItemPool items;InventoryReplicaPool pool;PlayerCore ac,bc;ac.user_id=12345;bc.user_id=12346;CharacterReplica a(ac,pool),b(bc,pool);std::recursive_mutex am,bm;
        World first(a.inventory(),items),second(b.inventory(),items);std::string trace;first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';
        SplitDelivery services(a.inventory(),b.inventory(),trace);VerticalInventoryReplicaWorld delivery(services,12345,a,am,12346,b,bm);
        Actor source(1001,{}, {},{},first,{definitions,enchant,mobs}),recipient(1002,{}, {},{},second,{definitions,enchant,mobs});source.account_id()=12345;recipient.account_id()=12346;recipient.status().skills.max_bags=1;
        ItemDefinition definition;definition.id=5001;definition.stack_count=1;auto* item=items.allocate();require(item,"Cannot allocate whole exchange item");item->definition=&definition;item->item.uid=99;item->item.type=5;item->item.type_id=1;item->item.count=1;item->item.quality=77;
        require(source.claim_inventory_item(1,2,*item,items),"Cannot retain whole exchange source");source.exchange().items[0]={1,2,1};
        net::Packet db(57),out(181),receive(36),in(179);VerticalExchangeWholeItemTransferExecutor whole(delivery,db,out,receive,in);
        VerticalExchangeItemSelection selection{0,1,0,item,&items,true};bool missing=false;
        try{whole.transfer_whole_exchange_item(recipient,source,selection);}catch(const VerticalInventoryRemovalError&){missing=true;}
        require(missing && source.retained_inventory_item(1,2)->item==item && recipient.inventory_slot_empty(1,0) && trace.empty(),"Missing whole source replica changed ownership or emitted effects");
        require(a.inventory().load(item->item),"Cannot seed whole exchange source replica");whole.transfer_whole_exchange_item(recipient,source,selection);
        const auto moved=recipient.retained_inventory_item(1,0);const auto rows=b.inventory().inventory();
        require(source.inventory_slot_empty(1,2) && moved && moved->item==item && moved->pool==&items && items.is_allocated(*item) && item->item.uid==99 && item->item.quality==77 && a.inventory().inventory().empty() && rows.size()==1 && rows[0].item.uid==99 && rows[0].item.quality==77,"Whole exchange lost actor, pool or replica ownership");
        require(first.client.size()==1 && first.client[0]==net::Packet({6,2,1,2,0,0,0}) && second.client.size()==1 && second.client[0].size()==36 && second.client[0][0]==5 && second.client[0][1]==2 && std::count(trace.begin(),trace.end(),',')==4,"Whole exchange lost client or log delivery");
    }
    {
        struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override {return 100;}std::uint32_t packed_local_calendar() override {return 0x12345678;}};
        for(unsigned scenario=0;scenario<3;++scenario) {
            GroundItemPool items;InventoryReplicaPool pool;PlayerCore a_core,b_core;a_core.user_id=12345;a_core.money=10;b_core.user_id=12346;b_core.money=20;CharacterReplica a(a_core,pool),b(b_core,pool);std::recursive_mutex a_mutex,b_mutex;World first(a.inventory(),items),second(b.inventory(),items);std::string trace;first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';SplitDelivery services(a.inventory(),b.inventory(),trace);VerticalInventoryReplicaWorld delivery(services,12345,a,a_mutex,12346,b,b_mutex);Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.account_id()=12345;peer.account_id()=12346;actor.money()=10;peer.money()=20;actor.status().skills.max_bags=1;peer.status().skills.max_bags=1;ItemDefinition definition;definition.id=5001;definition.stack_count=20;auto* source=items.allocate();require(source,"Cannot allocate validation source");source->definition=&definition;source->item.uid=99;source->item.type=5;source->item.type_id=1;source->item.count=10;require(actor.claim_inventory_item(1,2,*source,items),"Cannot retain validation source");actor.exchange().items[0]={1,2,3};actor.exchange().items[1]={1,7,1};actor.exchange().money=3;peer.exchange().money=2;
            if(scenario) {auto row=source->item;if(scenario==1) row.uid=100;else row.count=9;require(a.inventory().load(row),"Cannot seed divergent source replica");}
            Clock clock;ItemIds ids(7,clock);NoWholeExchange whole;net::Packet db(57),out(181),receive(36),in(179),out_money(126),in_money(111);VerticalExchangeItemTransferExecutor transfers(items,ids,clock,delivery,whole,db,out,receive,in);const std::uint32_t field=0;VerticalExchangeSettlementExecutor settlement(delivery,transfers,out_money,in_money,field);bool blocked=false;
            try {settlement.settle_exchange(actor,peer);}catch(const VerticalInventoryRemovalError&) {blocked=scenario==0;}catch(const VerticalInventorySourceMismatch&) {blocked=scenario!=0;}
            const auto rows=a.inventory().inventory();require(blocked && actor.money()==10 && peer.money()==20 && a.core().money==10 && b.core().money==20 && source->item.count==10 && actor.retained_inventory_item(1,2)->item==source && peer.inventory().empty() && b.inventory().inventory().empty() && items.statistics().owned==1 && trace.empty() && first.client.empty() && second.client.empty(),"Source prevalidation allowed partial settlement effects");
            require((scenario==0 && rows.empty()) || (scenario==1 && rows.size()==1 && rows[0].item.uid==100 && rows[0].item.count==10) || (scenario==2 && rows.size()==1 && rows[0].item.uid==99 && rows[0].item.count==9),"Source prevalidation changed divergent replica");
        }
    }
    {
        struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override {return 100;}std::uint32_t packed_local_calendar() override {return 0x12345678;}};
        for(unsigned reverse=0;reverse<2;++reverse) {
            GroundItemPool items;InventoryReplicaPool pool;PlayerCore ac,bc;ac.user_id=12345;bc.user_id=12346;ac.money=10;bc.money=20;CharacterReplica a(ac,pool),b(bc,pool);std::recursive_mutex am,bm;World first(a.inventory(),items),second(b.inventory(),items);std::string trace,shop;SplitDelivery services(a.inventory(),b.inventory(),trace);VerticalInventoryReplicaWorld delivery(services,12345,a,am,12346,b,bm);Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.account_id()=12345;peer.account_id()=12346;actor.money()=10;peer.money()=20;actor.status().skills.max_bags=peer.status().skills.max_bags=1;actor.exchange().mode=peer.exchange().mode=3;actor.exchange().peer=&peer;peer.exchange().peer=&actor;peer.exchange().ready=1;actor.exchange().money=3;peer.exchange().money=2;
            auto& source=reverse?actor:peer;auto& source_replica=reverse?a:b;auto& target_replica=reverse?b:a;ItemDefinition definition;definition.id=5001;definition.stack_count=20;auto* item=items.allocate();require(item,"Cannot allocate destination-preflight source");item->definition=&definition;item->item.uid=99;item->item.type=5;item->item.type_id=1;item->item.count=10;require(source.claim_inventory_item(1,2,*item,items) && source_replica.inventory().load(item->item),"Cannot retain destination-preflight source");source.exchange().items[0]={1,2,3};auto stale=item->item;stale.uid=123;stale.bag=1;stale.slot=0;require(target_replica.inventory().load(stale),"Cannot load divergent destination");Clock clock;ItemIds ids(7,clock);NoWholeExchange whole;net::Packet db(57),out(181),receipt(36),in(179),money_out(126),money_in(111);VerticalExchangeItemTransferExecutor transfers(items,ids,clock,delivery,whole,db,out,receipt,in);const std::uint32_t field=0;VerticalExchangeSettlementExecutor settlement(delivery,transfers,money_out,money_in,field);ExchangeActions actions(shop);bool blocked=false;try {vertical_inventory_exchange_guarded_ready(actor,net::Packet{5,0x0a,0},actions,settlement);}catch(const VerticalInventoryDestinationOccupied&) {blocked=true;}require(blocked && shop.empty() && actor.exchange().ready==0 && actor.exchange().mode==3 && peer.exchange().mode==3 && actor.money()==10 && peer.money()==20 && a.core().money==10 && b.core().money==20 && item->item.count==10 && items.statistics().owned==1 && first.client.empty() && second.client.empty() && trace.empty(),"First destination divergence caused readiness or settlement effects");
        }
    }
    {
        struct Clock final:ItemIdClock {std::uint32_t ticks_ms() override {return 100;}std::uint32_t packed_local_calendar() override {return 0x12345678;}};
        for(unsigned scenario=0;scenario<2;++scenario) {
            GroundItemPool items;InventoryReplicaPool pool;PlayerCore a_core,b_core;a_core.user_id=12345;a_core.money=10;b_core.user_id=12346;b_core.money=20;CharacterReplica a(a_core,pool),b(b_core,pool);std::recursive_mutex a_mutex,b_mutex;World first(a.inventory(),items),second(b.inventory(),items);std::string trace;first.exchange_trace=&trace;second.exchange_trace=&trace;second.exchange_tag='B';SplitDelivery services(a.inventory(),b.inventory(),trace);VerticalInventoryReplicaWorld delivery(services,12345,a,a_mutex,12346,b,b_mutex);Actor actor(1001,{}, {},{},first,{definitions,enchant,mobs}),peer(1002,{}, {},{},second,{definitions,enchant,mobs});actor.account_id()=12345;peer.account_id()=12346;actor.money()=10;peer.money()=20;actor.status().skills.max_bags=1;peer.status().skills.max_bags=1;
            ItemDefinition definition;definition.id=5001;definition.stack_count=scenario?20:1;auto* source=items.allocate();require(source,"Cannot allocate whole preflight source");source->definition=&definition;source->item.uid=99;source->item.type=5;source->item.type_id=1;source->item.count=scenario?10:1;require(actor.claim_inventory_item(1,2,*source,items) && a.inventory().load(source->item),"Cannot retain whole preflight source");actor.exchange().items[0]={1,2,static_cast<std::uint8_t>(scenario?10:7)};actor.exchange().money=3;peer.exchange().money=2;
            Clock clock;ItemIds ids(7,clock);NoWholeExchange whole;net::Packet db(57),out(181),receive(36),in(179),out_money(126),in_money(111);VerticalExchangeItemTransferExecutor transfers(items,ids,clock,delivery,whole,db,out,receive,in);const std::uint32_t field=0;VerticalExchangeSettlementExecutor settlement(delivery,transfers,out_money,in_money,field);bool blocked=false;try {settlement.settle_exchange(actor,peer);}catch(const std::runtime_error&) {blocked=true;}
            require(blocked && whole.validations==1 && whole.transfers==0 && actor.money()==10 && peer.money()==20 && a.core().money==10 && b.core().money==20 && actor.retained_inventory_item(1,2)->item==source && source->item.count==(scenario?10:1) && a.inventory().inventory().size()==1 && b.inventory().inventory().empty() && peer.inventory().empty() && items.statistics().owned==1 && trace.empty() && first.client.empty() && second.client.empty(),"Unavailable whole owner allowed partial settlement");
            actor.exchange().mode=3;peer.exchange().mode=3;actor.exchange().peer=&peer;peer.exchange().peer=&actor;peer.exchange().ready=1;whole.validations=0;std::string shop;ExchangeActions actions(shop);blocked=false;try {vertical_inventory_exchange_guarded_ready(actor,net::Packet{5,0x0a,0},actions,settlement);}catch(const std::runtime_error&) {blocked=true;}
            require(blocked && whole.validations==1 && whole.transfers==0 && shop.empty() && actor.exchange().ready==0 && actor.exchange().mode==3 && peer.exchange().mode==3 && actor.money()==10 && peer.money()==20 && trace.empty() && first.client.empty() && second.client.empty(),"Unavailable whole owner produced ready effects");
            if(scenario) {struct ChangingOffer final:VerticalInventoryActions {bool called=false;void cancel_action_exchange(Actor&) override {throw std::runtime_error("Unexpected cancellation");}bool pickup_checks() override {throw std::runtime_error("Unexpected pickup");}void end_shop(Actor& value) override {called=true;value.exchange().items[0].count=10;}} changing;actor.exchange().items[0].count=3;whole.validations=0;blocked=false;try {vertical_inventory_exchange_guarded_ready(actor,net::Packet{5,0x0a,0},changing,settlement);}catch(const std::runtime_error&) {blocked=true;}require(blocked && changing.called && whole.validations==1 && whole.transfers==0 && actor.exchange().ready==0 && actor.exchange().mode==3 && actor.money()==10 && peer.money()==20 && trace.empty() && first.client.empty() && second.client.empty(),"Shop mutation bypassed whole-owner readiness validation");}
        }
    }
    std::cout<<count<<" pickup, 1024 drop, 1024 exchange, 1024 item-offer, 512 response, 512 control, 512 invite, 512 ready, 2048 capacity, 1024 preflight, 1024 expiry, 492 money-log, 256 money-phase, 1024 item packet and 1024 transfer DB and 1024 item-log traces passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
