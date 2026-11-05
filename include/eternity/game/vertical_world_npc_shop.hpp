#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/guild_house_shop.hpp"
#include "eternity/game/item_ids.hpp"
#include "eternity/game/npc_shop.hpp"
#include "eternity/game/vertical_world_inventory_pickup.hpp"

namespace eternity::game {
// Read-only world definitions and the shared item owners used by this shop.
struct VerticalWorldNpcShopOwners {
    const NpcDefinitions& npcs;
    const ItemDefinitions& items;
    const GuildHouseShopConfig& rates;
    GuildCatalog& guilds;
    std::shared_ptr<GroundItemPool> pool;
    std::shared_ptr<ItemIds> ids;
    std::shared_ptr<ItemIdClock> clock;
};
// One session's opcode group 07 (ps_game.exe CUser::PacketProcessing to
// PacketShop 0046af10), chained into the retained gameplay dispatcher. Owners,
// services, actions and the zone are the actual live ones and outlive dispatch.
template<class Store=CharacterSaveStore>
class VerticalWorldNpcShop final:public NpcShopWorld {
public:
    using Binding=VerticalWorldSessionSaveBinding<Store>;
    using Other=VerticalWorldSessionGameplay::Other;
    VerticalWorldNpcShop(VerticalWorldSession::Strand strand,std::shared_ptr<VerticalWorldSession> session,RuntimeZone& zone,
        std::shared_ptr<Binding> save,std::shared_ptr<ItemPickupWorld> services,std::shared_ptr<VerticalInventoryActions> actions,
        VerticalWorldNpcShopOwners owners,Other next)
        :strand_(std::move(strand)),session_(session),zone_(zone),save_(std::move(save)),services_(std::move(services)),
         actions_(std::move(actions)),owners_(std::move(owners)),next_(std::move(next)) {
        if(!strand_.running_in_this_thread() || !session || !save_ || !services_ || !actions_ || !owners_.pool || !owners_.ids ||
           !owners_.clock || !next_ || !save_->owns_session(*session) || !zone_.map)
            throw std::invalid_argument("Incomplete session NPC shop owners");
    }
    void dispatch(VerticalWorldSession& session,const net::Packet& packet) {
        auto owner=checked(session);
        if(packet.size()<2)throw std::invalid_argument("Malformed NPC shop dispatch");
        if(packet[1]!=7) {next_(session,packet);return;}
        if(active_)throw std::logic_error("Reentrant NPC shop dispatch");
        struct Guard {bool& value;explicit Guard(bool& v):value(v){value=true;}~Guard(){value=false;}} guard(active_);
        owner->begin_admission_pass();
        try {(void)npc_shop_packet(owner->world_user()->actor,packet,*this);}
        catch(...) {owner->end_admission_pass();throw;}
        owner->end_admission_pass();
    }
    static Other bind(std::shared_ptr<VerticalWorldNpcShop> owner) {
        if(!owner)throw std::invalid_argument("Missing NPC shop owner");
        return [owner=std::move(owner)](VerticalWorldSession& session,const net::Packet& packet){owner->dispatch(session,packet);};
    }
    std::optional<NpcShopEntity> find_zone_npc(Actor&,std::uint32_t id) override {
        if(!zone_.npcs)throw std::logic_error("NPC shop zone has no NPC owner");
        const auto* npc=zone_.npcs->find(id);if(!npc)return std::nullopt;
        return NpcShopEntity{static_cast<std::uint32_t>(npc->type),static_cast<std::uint32_t>(npc->type_id)};
    }
    // m_listShop is the NpcQuest.SData list of type 1, indexed by one-based row.
    const NpcDefinition* find_shop_npc(std::uint32_t type_id) override {
        return owners_.npcs.find(1,static_cast<std::int32_t>(type_id));
    }
    const ItemDefinition* find_shop_item(std::uint8_t type,std::uint8_t type_id) override {return owners_.items.find(type,type_id);}
    void cancel_action_exchange(Actor& actor) override {actions_->cancel_action_exchange(actor);}
    void end_my_shop(Actor& actor) override {actions_->end_shop(actor);}
    bool guild_house_zone(Actor&) override {return zone_.map->settings.create_type==6;} // CreateType H.
    std::optional<NpcShopBuyRate> guild_house_buy_rate(Actor&,std::uint16_t npc_type_id,std::uint8_t shop_type) override {
        auto owner=live();
        auto* guild=owners_.guilds.find_membership(owner->guild_user().guild);if(!guild)return std::nullopt;
        std::lock_guard lock(guild->mutex); // CGuildLock 0042b800.
        auto& listing=guild->metadata.listing;auto country=static_cast<std::int32_t>(listing.country);
        const auto rate=guild_house_shop_buy_rate(guild->npcs,country,listing.rank,owners_.rates,npc_type_id,shop_type);
        listing.country=static_cast<std::uint32_t>(country);return rate;
    }
    GroundItemPool& shop_item_pool() override {return *owners_.pool;}
    std::uint32_t shop_system_time() override {return owners_.clock->packed_local_calendar();}
    std::uint64_t next_shop_item_uid() override {return owners_.ids->next();}
    void send_shop_database(const net::Packet& packet) override {apply(packet);}
    std::string shop_item_name(const ItemDefinition& item) override {return services_->inventory_item_name(item);}
    void close_shop_connection(Actor&,std::uint32_t reason) override {live()->channel().disconnect_with_reason(reason);}
    std::uint32_t shop_interaction_npc_type_id(Actor&) override {
        auto quests=live()->quests_lease();if(!quests)return 0;
        const auto* npc=quests->packets().interaction().npc;return npc?static_cast<std::uint32_t>(npc->type_id):0;
    }
    void send_money_database(const net::Packet& packet) override {apply(packet);}
    GameLogSubject money_log_subject(const Actor& actor) override {return services_->money_log_subject(actor);}
    std::chrono::sys_seconds money_local_calendar() override {return services_->money_local_calendar();}
    void send_money_log(const net::Packet& packet) override {services_->send_money_log(packet);}
private:
    std::shared_ptr<VerticalWorldSession> live() {
        auto owner=session_.lock();if(!owner)throw std::logic_error("NPC shop lost session");return checked(*owner);
    }
    std::shared_ptr<VerticalWorldSession> checked(VerticalWorldSession& session) {
        if(!strand_.running_in_this_thread())throw std::logic_error("NPC shop left the world strand");
        auto owner=session_.lock();
        if(owner.get()!=&session || !owner->world_user() || owner->admission().zone!=&zone_ || owner->closing())
            throw std::logic_error("NPC shop lost its zone session");
        return owner;
    }
    void apply(const net::Packet& packet) {
        auto owner=live();std::lock_guard lock(save_->save_mutex());
        const auto result=save_->apply(packet);
        if(result!=InventoryReplicaResult::applied)throw VerticalInventoryDeliveryError(result);
    }
    VerticalWorldSession::Strand strand_;std::weak_ptr<VerticalWorldSession> session_;RuntimeZone& zone_;
    std::shared_ptr<Binding> save_;std::shared_ptr<ItemPickupWorld> services_;std::shared_ptr<VerticalInventoryActions> actions_;
    VerticalWorldNpcShopOwners owners_;Other next_;bool active_=false;
};
}
