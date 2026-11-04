#pragma once
#include "eternity/game/quest_start_items_actor.hpp"
#include "eternity/game/quest_start_log.hpp"
#include "eternity/game/inventory_replica.hpp"
#include "eternity/game/quest_replica.hpp"
#include "equipment_fixture.hpp"
#include <functional>
#include <sstream>
#include <stdexcept>
namespace quest_actor_test {
using namespace eternity;using namespace eternity::game;
inline void check(bool b,const std::string& s) {if(!b) throw std::runtime_error(s);}
inline net::Packet decode(const std::string& s) {net::Packet p;if(s=="-") return p;for(std::size_t i=0;i<s.size();i+=2) p.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return p;}
inline std::vector<std::uint64_t> numbers(const std::string& s) {std::vector<std::uint64_t> r;if(s=="-") return r;std::istringstream row(s);for(std::string v;std::getline(row,v,',');) r.push_back(std::stoull(v));return r;}
inline void append(net::Packet& p,std::uint64_t v,unsigned n=4) {for(unsigned i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(v>>(8*i)));}
inline std::uint32_t word(const net::Packet& p,std::size_t at,unsigned n=4) {std::uint32_t v=0;for(unsigned i=0;i<n;++i) v|=std::uint32_t(p.at(at+i))<<(8*i);return v;}
inline std::shared_ptr<ItemDefinitions> catalog(std::uint8_t type,const std::vector<std::uint64_t>& ids) {
    auto table=item_definitions();table.rows.clear();
    for(auto id:ids) {
        std::vector<std::optional<std::string>> row(table.columns.size(),std::string("0"));
        const auto put=[&](const std::string& key,std::uint64_t v) {for(std::size_t i=0;i<table.columns.size();++i) if(table.columns[i]==key) row[i]=std::to_string(v);};
        put("ItemID",std::uint32_t(type)*1000+id);put("Type",type);put("TypeID",id);put("ReqWis",99);put("Server",1);put("Quality",256+id);put("Count",1);table.rows.push_back(std::move(row));
    }
    return std::make_shared<ItemDefinitions>(ItemDefinitions::load(table));
}
struct Scene final:ActorScene {
    std::vector<net::Packet> packets;
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected Actor scene effect");}
    void send_self(const net::Packet& p) noexcept override {packets.push_back(p);}
    void send_nearby(const net::Packet&) override {unexpected();}void send_cell_view(const net::Packet&) override {unexpected();}
    std::optional<bool> party_kind() const noexcept override {std::terminate();}void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}void stop_target_mob(std::uint32_t) override {unexpected();}void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();}UserShapeSocial user_shape_social() override {unexpected();}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
struct Clock final:ItemIdClock {
    std::uint32_t tick=0;unsigned ticks=0,calendars=0;
    std::uint32_t ticks_ms() override {++ticks;return tick++;}
    std::uint32_t packed_local_calendar() override {++calendars;return quest_start_log_time(2026,11,3,15,30,40);}
};
inline void mirror(Actor& actor,const net::Packet& p) {
    const auto bag=p.at(18);const auto slot=p.at(19);auto held=actor.retained_inventory_item(bag,slot);check(held.has_value(),"Missing retained grant at DB boundary");
    const auto at=std::find_if(actor.inventory().begin(),actor.inventory().end(),[&](const auto& i){return i.bag==bag && i.slot==slot;});
    check(at!=actor.inventory().end() && at->uid==held->item->item.uid && at->craft==held->item->item.craft && at->count==held->item->item.count && at->quality==held->item->item.quality,"Actor mirror was not synchronized before real transport");
}
struct Ports:QuestStartItemsActorPorts {
    std::int32_t& bags;std::int32_t new_bags=-99;unsigned db_count=0;std::uint32_t now=0;net::Packet packets;
    InventoryReplicaPool cache_pool;InventoryReplica cache;QuestReplicaPool quest_cache_pool;QuestReplica quest_cache;
    bool fail_calendar=false,fail_database=false;std::function<void(Actor&,const net::Packet&)> database_callback;
    Ports(std::int32_t& b,std::uint32_t account):bags(b),cache(account,0,cache_pool),quest_cache(account,quest_cache_pool) {}
    std::uint32_t local_make_time(Actor&) override {if(fail_calendar) throw std::runtime_error("Calendar unavailable");return quest_start_log_time(2026,11,3,15,30,40);}
    std::uint32_t tick_ms(Actor&) override {return now++;}
    void send_database(Actor& actor,const net::Packet& p) override {
        const auto op=word(p,0,2);
        if(op==0x701) {mirror(actor,p);check(cache.apply(p)==InventoryReplicaResult::applied,"Actual inventory replica rejected grant");++db_count;}
        else if(op==0xa01) check(quest_cache.apply(p)==InventoryReplicaResult::applied,"Actual quest replica rejected add");
        else check(false,"Unexpected DB operation");
        packets.push_back('B');packets.insert(packets.end(),p.begin(),p.end());
        if(db_count==1 && new_bags!=-99) bags=new_bags;
        if(database_callback) database_callback(actor,p);if(fail_database) throw std::runtime_error("DB transport unavailable");
    }
    void send_client(Actor& actor,const net::Packet& p) override {packets.push_back('C');packets.insert(packets.end(),p.begin(),p.end());actor.send_self(p);}
};
inline net::Packet snapshot(const std::vector<GroundItem*>& stored) {
    net::Packet p;append(p,stored.size());for(const auto* i:stored) {append(p,i->item.bag,1);append(p,i->item.slot,1);append(p,i->item.uid,8);append(p,i->item.type,1);append(p,i->item.type_id,1);append(p,i->item.count,1);append(p,i->item.quality,2);for(auto c:i->item.gems) append(p,c,1);for(auto c:i->craft_storage) append(p,c,1);append(p,i->item.made_at);append(p,i->item.make_type.empty()?0:static_cast<unsigned char>(i->item.make_type[0]),1);for(auto c:i->craft_additions) append(p,c,2);append(p,i->definition->id);}return p;
}
}
