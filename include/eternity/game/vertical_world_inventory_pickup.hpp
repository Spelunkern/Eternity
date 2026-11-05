#pragma once
#include "eternity/game/vertical_inventory.hpp"
#include "eternity/game/item_pool.hpp"
#include "eternity/game/vertical_world_session_gameplay.hpp"
#include "eternity/game/vertical_world_session_save_binding.hpp"
#include <algorithm>
#include <bit>
#include <optional>

namespace eternity::game {
class VerticalInventoryMoneyMismatch final:public std::runtime_error {
public:
    VerticalInventoryMoneyMismatch():std::runtime_error("Pickup money differs from the retained actor or replica") {}
};
// One session's original 0205 pickup, chained into the retained gameplay
// dispatcher. The supplied live services, quest links, actions and item pool
// must be the same owners used by this zone, and must outlive dispatch. Quest
// authorization uses the session's actual quest lease, never the delegate's
// quest callback. Their
// callbacks must not close/move the session or mutate its selected item
// reentrantly. The runtime stops this owner before save/zone teardown.
template<class Store=CharacterSaveStore>
class VerticalWorldInventoryPickup final:public ItemPickupWorld,public ItemPickDetachFence,public std::enable_shared_from_this<VerticalWorldInventoryPickup<Store>> {
public:
    using Binding=VerticalWorldSessionSaveBinding<Store>;
    using Other=VerticalWorldSessionGameplay::Other;
    static std::shared_ptr<VerticalWorldInventoryPickup> create(VerticalWorldSession::Strand strand,
        VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalWorldSession> session,RuntimeZone& zone,
        std::shared_ptr<Binding> save,std::shared_ptr<ItemPickupWorld> services,
        std::shared_ptr<VerticalInventoryActions> actions,std::shared_ptr<const ItemQuestLinks> links,
        std::shared_ptr<GroundItemPool> pool,Other next) {
        if(!strand.running_in_this_thread() || !session || !save || !services || !actions || !links || !pool || !next || !save->owns_session(*session))
            throw std::invalid_argument("Incomplete session pickup owners");
        auto result=std::shared_ptr<VerticalWorldInventoryPickup>(new VerticalWorldInventoryPickup(std::move(strand),registry,session,zone,std::move(save),std::move(services),std::move(actions),std::move(links),std::move(pool),std::move(next)));
        result->checked(*session);return result;
    }
    void dispatch(VerticalWorldSession& session,const net::Packet& packet) {
        auto self=this->shared_from_this();auto owner=checked(session);
        if(packet.size()<2)throw std::invalid_argument("Malformed pickup dispatch");
        if(packet[0]!=5 || packet[1]!=2) {next_(session,packet);return;}
        if(active_)throw std::logic_error("Reentrant pickup dispatch");
        struct Guard {VerticalWorldInventoryPickup& owner;explicit Guard(VerticalWorldInventoryPickup& value):owner(value){owner.active_=true;}~Guard(){if(owner.pinned_)owner.pool_->unpin(*owner.selected_);owner.pinned_=false;owner.selected_snapshot_.reset();owner.selected_=nullptr;owner.active_=false;}} guard(*this);
        struct CheckedActions final:VerticalInventoryActions {
            VerticalWorldInventoryPickup& parent;VerticalInventoryActions& owner;const net::Packet& packet;std::uint32_t selected_id=0;
            CheckedActions(VerticalWorldInventoryPickup& p,VerticalInventoryActions& a,const net::Packet& bytes):parent(p),owner(a),packet(bytes) {}
            void cancel_action_exchange(Actor& actor) override {owner.cancel_action_exchange(actor);parent.check_live();}
            void end_shop(Actor& actor) override {
                owner.end_shop(actor);parent.check_live();selected_id=0;
                for(unsigned i=0;i<4;++i)selected_id|=std::uint32_t(packet[2+i])<<(8*i);
            }
            bool pickup_checks() override {const auto result=owner.pickup_checks();parent.check_live();parent.prepare_selected(selected_id);return result;}
        } actions(*this,*actions_,packet);
        owner->begin_admission_pass();
        try {(void)vertical_inventory_guarded_pickup_request(owner->world_user()->actor,packet,*zone_.items,*links_,*this,*pool_,actions);check_live();}
        catch(...) {owner->end_admission_pass();throw;}
        owner->end_admission_pass();
    }
    static Other bind(std::shared_ptr<VerticalWorldInventoryPickup> owner) {
        if(!owner)throw std::invalid_argument("Missing pickup owner");
        return [owner=std::move(owner)](VerticalWorldSession& session,const net::Packet& packet){owner->dispatch(session,packet);};
    }
    void stop() {require_strand();if(active_)throw std::logic_error("Pickup stopped during dispatch");stopped_=true;}
    std::uint32_t pick_tick_ms() override {check_live();const auto value=zone_.items->current_tick_ms();check_live();check_selected();return value;}
    std::uint32_t pick_party_id(Actor& a) override {check_actor(a);const auto value=services_->pick_party_id(a);check_actor(a);check_selected();return value;}
    UserPosition pick_position(const Actor& a) override {
        check_actor(a);const auto position=zone_.users->position(a.id());
        if(!position)throw std::logic_error("Pickup lost its zone position");
        check_selected();return *position;
    }
    bool pick_quest_active(Actor& a,std::uint16_t id) override {
        auto owner=session_.lock();if(!owner)throw std::logic_error("Pickup lost session");checked(*owner);
        if(&owner->world_user()->actor!=&a)throw std::logic_error("Pickup quest actor changed");
        auto quests=owner->quests_lease();
        if(!quests || !quests->quests().owns_actor(a))throw std::logic_error("Pickup quest session unavailable");
        const auto active=quests->quests().active(id);check_actor(a);check_selected();return active;
    }
    void item_pick_detached(Actor& actor,GroundItem& item) override {
        check_actor(actor);if(&item!=selected_)throw std::logic_error("Pickup detached a different item");check_detached();
    }
    std::uint32_t lock_inventory_party(Actor& a) override {
        check_actor(a);check_detached();const auto token=services_->lock_inventory_party(a);
        try {check_actor(a);check_detached();}catch(...) {if(token)services_->unlock_inventory_party(token);throw;}
        return token;
    }
    void unlock_inventory_party(std::uint32_t token) noexcept override {services_->unlock_inventory_party(token);}
    std::optional<bool> inventory_party_kind(const Actor& a) override {check_actor(a);const auto value=services_->inventory_party_kind(a);check_actor(a);return value;}
    void send_inventory_party(Actor& a,const net::Packet& p) override {check_actor(a);services_->send_inventory_party(a,p);check_actor(a);}
    void send_inventory_database(const net::Packet& p) override {
        if(p.size()<2 || p[1]!=7 || (p[0]!=1 && p[0]!=7))throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
        apply(p);
    }
    void send_money_database(const net::Packet& p) override {
        if(p.size()!=10 || p[0]!=3 || p[1]!=6)throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
        auto owner=session_.lock();if(!owner)throw std::logic_error("Pickup lost session");checked(*owner);
        std::lock_guard lock(save_->save_mutex());
        std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(p[6+i])<<(8*i);
        if(value!=owner->world_user()->actor.money())throw VerticalInventoryMoneyMismatch();
        apply(p);
        if(owner->character()->replica().core().money!=value || owner->world_user()->actor.money()!=value)
            throw VerticalInventoryMoneyMismatch();
        check_live();
    }
    std::string inventory_item_name(const ItemDefinition& i) override {check_live();auto value=services_->inventory_item_name(i);check_live();return value;}
    std::string inventory_drop_user_name(Actor& a,std::uint32_t id,bool lookup) override {check_actor(a);auto value=services_->inventory_drop_user_name(a,id,lookup);check_actor(a);return value;}
    bool return_inventory_item(Actor& a,GroundItem& i) override {
        check_actor(a);const auto entered=zone_.items->enter(i,1);check_actor(a);return entered;
    }
    GameLogSubject money_log_subject(const Actor& a) override {check_actor(a);auto value=services_->money_log_subject(a);check_actor(a);return value;}
    std::chrono::sys_seconds money_local_calendar() override {check_live();const auto value=services_->money_local_calendar();check_live();return value;}
    void send_money_log(const net::Packet& p) override {check_live();services_->send_money_log(p);check_live();}
private:
    VerticalWorldInventoryPickup(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        const std::shared_ptr<VerticalWorldSession>& session,RuntimeZone& zone,std::shared_ptr<Binding> save,
        std::shared_ptr<ItemPickupWorld> services,std::shared_ptr<VerticalInventoryActions> actions,
        std::shared_ptr<const ItemQuestLinks> links,std::shared_ptr<GroundItemPool> pool,Other next)
        :strand_(std::move(strand)),registry_(registry),session_(session),zone_(zone),save_(std::move(save)),services_(std::move(services)),actions_(std::move(actions)),links_(std::move(links)),pool_(std::move(pool)),next_(std::move(next)) {}
    void prepare_selected(std::uint32_t id) {
        selected_=zone_.items->find(id);selected_id_=id;
        if(selected_) {
            if(!pool_->pin(*selected_))throw std::logic_error("Pickup item is not allocated by its pool");
            pinned_=true;selected_snapshot_=*selected_;
        }
    }
    bool selected_fields_match() const {
        if(!selected_ || !selected_snapshot_)return false;
        const auto& a=*selected_;const auto& b=*selected_snapshot_;const auto& x=a.item;const auto& y=b.item;
        const auto same_float=[](float left,float right){return std::bit_cast<std::uint32_t>(left)==std::bit_cast<std::uint32_t>(right);};
        return a.id==b.id && a.definition==b.definition && a.user_id==b.user_id && a.party_id==b.party_id &&
            a.drop_id==b.drop_id && a.money==b.money && a.drop_type==b.drop_type && a.expires_at==b.expires_at &&
            same_float(a.position.x,b.position.x) && same_float(a.position.y,b.position.y) && same_float(a.position.z,b.position.z) &&
            a.craft_storage==b.craft_storage && a.craft_additions==b.craft_additions && x.id==y.id && x.uid==y.uid &&
            x.type==y.type && x.type_id==y.type_id && x.bag==y.bag && x.slot==y.slot && x.count==y.count &&
            x.quality==y.quality && x.gems==y.gems && x.craft==y.craft && x.made_at==y.made_at &&
            x.make_type==y.make_type && x.relocated==y.relocated && x.source_order==y.source_order;
    }
    // These checks run only while ZoneItems::pick still owns the captured item.
    // After leave(), the admission path intentionally works on a detached item.
    void check_selected() {
        if(selected_ && (!selected_fields_match() || selected_->id!=selected_id_ || zone_.items->find(selected_id_)!=selected_ || selected_->zone!=zone_.items.get() ||
           !selected_->cell || !pool_->is_allocated(*selected_)))
            throw std::logic_error("Pickup selected item changed during authorization");
    }
    void check_detached() {
        if(selected_ && (!selected_fields_match() || selected_->id!=selected_id_ || selected_->zone || selected_->cell || !pool_->is_allocated(*selected_)))
            throw std::logic_error("Pickup detached item changed during admission");
    }
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Pickup outside world strand");}
    std::shared_ptr<VerticalWorldSession> checked(VerticalWorldSession& supplied) {
        require_strand();auto owner=session_.lock();
        if(stopped_ || !owner || owner.get()!=&supplied || owner->closing() || !owner->character() || !owner->world_user() ||
           owner->character()->pipeline().result().actor.get()!=&owner->world_user()->actor ||
           owner->admission().location!=4 || owner->admission().zone!=&zone_ || !zone_.items || !zone_.users ||
           zone_.users->find_actor(owner->world_user()->actor.id())!=&owner->world_user()->actor ||
           registry_.retain_actor(owner->world_user()->actor)!=owner || !save_->owns_session(*owner))
            throw std::logic_error("Pickup lost its admitted session");
        return owner;
    }
    void check_live() {auto owner=session_.lock();if(!owner)throw std::logic_error("Pickup lost session");checked(*owner);}
    void check_actor(const Actor& actor) {auto owner=session_.lock();if(!owner)throw std::logic_error("Pickup lost session");checked(*owner);if(&owner->world_user()->actor!=&actor)throw std::logic_error("Pickup service actor changed");}
    void apply(const net::Packet& packet) {
        auto owner=session_.lock();if(!owner)throw std::logic_error("Pickup lost session");checked(*owner);
        if(packet.size()<6)throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
        std::uint32_t account=0;for(unsigned i=0;i<4;++i)account|=std::uint32_t(packet[2+i])<<(8*i);
        if(account!=owner->account_id())throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
        std::lock_guard lock(save_->save_mutex());
        const bool insertion=packet.size()==57 && packet[0]==1 && packet[1]==7;
        const bool count_update=packet.size()==9 && packet[0]==7 && packet[1]==7;
        std::uint64_t expected_uid=0;
        if(insertion) {
            if(packet[18]==0 || packet[18]>=6 || packet[19]>=24)
                throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
            const auto rows=owner->character()->replica().inventory().inventory();
            if(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==packet[18] && row.item.slot==packet[19];}))
                throw VerticalInventoryDestinationOccupied();
            auto& actor=owner->world_user()->actor;
            const auto held=actor.retained_inventory_item(packet[18],packet[19]);
            if(!held || !held->item || held->pool!=pool_.get() || !held->item->definition ||
               inventory_add_packet(owner->account_id(),packet[18],packet[19],*held->item)!=packet)
                throw VerticalInventorySourceMismatch();
        }
        if(count_update) {
            if(packet[6]==0 || packet[6]>=6 || packet[7]>=24)
                throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
            auto& actor=owner->world_user()->actor;
            const auto held=actor.retained_inventory_item(packet[6],packet[7]);
            if(!held || !held->item || held->item->item.count!=packet[8])
                throw std::logic_error("Pickup count update lost its actor item");
            expected_uid=held->item->item.uid;
            const auto rows=owner->character()->replica().inventory().inventory();
            const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.item.bag==packet[6] && value.item.slot==packet[7];});
            if(row==rows.end())throw VerticalInventoryRemovalError();
            if(row->item.uid!=expected_uid)throw VerticalInventorySourceMismatch();
        }
        const auto result=save_->apply(packet);
        if(result!=InventoryReplicaResult::applied)throw VerticalInventoryDeliveryError(result);
        // DB-agent 0701 can silently fail allocation while reporting applied.
        if(insertion) {
            std::uint64_t uid=0;for(unsigned i=0;i<8;++i)uid|=std::uint64_t(packet[6+i])<<(8*i);
            const auto rows=owner->character()->replica().inventory().inventory();
            if(std::none_of(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==packet[18] && row.item.slot==packet[19] && row.item.uid==uid;}))
                throw VerticalInventoryInsertionError();
        }
        if(count_update) {
            const auto rows=owner->character()->replica().inventory().inventory();
            if(std::none_of(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==packet[6] && row.item.slot==packet[7] && row.item.uid==expected_uid && row.item.count==packet[8];}))
                throw VerticalInventorySourceMismatch();
        }
        check_live();
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;std::weak_ptr<VerticalWorldSession> session_;RuntimeZone& zone_;
    std::shared_ptr<Binding> save_;std::shared_ptr<ItemPickupWorld> services_;std::shared_ptr<VerticalInventoryActions> actions_;
    std::shared_ptr<const ItemQuestLinks> links_;std::shared_ptr<GroundItemPool> pool_;Other next_;GroundItem* selected_=nullptr;std::optional<GroundItem> selected_snapshot_;std::uint32_t selected_id_=0;bool pinned_=false,active_=false,stopped_=false;
};
}
