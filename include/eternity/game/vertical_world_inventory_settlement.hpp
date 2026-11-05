#pragma once
#include "eternity/game/vertical_inventory.hpp"
#include "eternity/game/vertical_world_session_runtime.hpp"
#include "eternity/game/item_ids.hpp"
#include "eternity/game/item_pool.hpp"

namespace eternity::game {
// SInternetAddress::GetIP 00404e10 / CUser::Exchange 0046f4a0.
// Uses the accepted game socket's captured address, never the login endpoint.
inline std::uint32_t vertical_world_inventory_connection_ipv4(VerticalWorldSession& session) {
    const auto& address=session.channel().peer_ipv4;
    if(!address)throw std::logic_error("Settlement connection IPv4 unavailable");
    return vertical_inventory_exchange_ipv4_field(*address);
}
// Caller-owned original packet scratch, retained across synchronous exchanges.
struct VerticalWorldInventorySettlementScratch {
    net::Packet db,outgoing_item,receiver,incoming_item,outgoing_money,incoming_money;
};

// Composes only existing real owners. The runtime, zone, UID generator, clock,
// service callbacks and address provider must survive this owner and execute
// on the world strand. The caller holds both sessions' admission passes through
// each call. Callbacks must not alter participants, offers or scratch buffers.
template<class Resource,class Store=CharacterSaveStore>
class VerticalWorldInventorySettlement final:public VerticalExchangeSettlement {
public:
    using Runtime=VerticalWorldSessionRuntime<Resource,Store>;
    using Address=std::function<std::uint32_t(VerticalWorldSession&)>;
    VerticalWorldInventorySettlement(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        Runtime& runtime,const std::shared_ptr<VerticalWorldSession>& session,RuntimeZone& zone,
        std::shared_ptr<GroundItemPool> pool,std::shared_ptr<ItemIds> ids,std::shared_ptr<ItemIdClock> clock,
        std::shared_ptr<InventoryWorld> services,std::shared_ptr<VerticalWorldInventorySettlementScratch> scratch,Address address)
        :strand_(std::move(strand)),registry_(registry),runtime_(runtime),session_(session),zone_(zone),pool_(std::move(pool)),
         ids_(std::move(ids)),clock_(std::move(clock)),services_(std::move(services)),scratch_(std::move(scratch)),address_(std::move(address)) {
        if(!strand_.running_in_this_thread() || !session || !pool_ || !ids_ || !clock_ || !services_ || !scratch_ || !address_ ||
           scratch_->db.size()!=57 || scratch_->outgoing_item.size()!=181 || scratch_->receiver.size()!=36 ||
           scratch_->incoming_item.size()!=179 || scratch_->outgoing_money.size()!=126 || scratch_->incoming_money.size()!=111)
            throw std::invalid_argument("Incomplete retained settlement owners");
    }
    void validate_before_ready(Actor& actor,Actor& peer) override {with_executor(actor,peer,[&](auto& executor){executor.validate_before_ready(actor,peer);});}
    void settle_exchange(Actor& actor,Actor& peer) override {with_executor(actor,peer,[&](auto& executor){executor.settle_exchange(actor,peer);});}
private:
    template<class F> void with_executor(Actor& actor,Actor& peer,F&& work) {
        if(!strand_.running_in_this_thread() || active_)throw std::logic_error("Invalid retained settlement execution");
        struct Guard {bool& active;explicit Guard(bool& value):active(value){active=true;}~Guard(){active=false;}} guard(active_);
        auto owner=session_.lock();auto other=registry_.retain_actor_pointer(&peer);
        const auto valid=[&](const std::shared_ptr<VerticalWorldSession>& current,Actor& expected) {
            return current && !current->closing() && current->world_user() && &current->world_user()->actor==&expected &&
                current->character() && current->character()->pipeline().result().actor.get()==&expected &&
                current->admission().location==4 && current->admission().zone==&zone_ && zone_.users &&
                zone_.users->find_actor(expected.id())==&expected && registry_.retain_actor(expected)==current;
        };
        if(&actor==&peer || !valid(owner,actor) || !valid(other,peer))throw std::logic_error("Settlement requires two admitted sessions");
        auto first=runtime_.save_binding(*owner);auto second=runtime_.save_binding(*other);
        if(!first || !second || !first->owns_session(*owner) || !second->owns_session(*other))
            throw std::logic_error("Settlement requires both retained save bindings");
        const auto field=address_(*owner);
        if(!valid(owner,actor) || !valid(other,peer))throw std::logic_error("Settlement lost admitted participants");
        VerticalInventoryReplicaWorld world(*services_,owner->account_id(),owner->character()->replica(),first->save_mutex(),
            other->account_id(),other->character()->replica(),second->save_mutex());
        VerticalExchangeWholeItemTransferExecutor whole(world,scratch_->db,scratch_->outgoing_item,scratch_->receiver,scratch_->incoming_item);
        VerticalExchangeItemTransferExecutor items(*pool_,*ids_,*clock_,world,whole,scratch_->db,scratch_->outgoing_item,scratch_->receiver,scratch_->incoming_item);
        VerticalExchangeSettlementExecutor executor(world,items,scratch_->outgoing_money,scratch_->incoming_money,field);
        work(executor);
        if(!valid(owner,actor) || !valid(other,peer))throw std::logic_error("Settlement lost participants after delivery");
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;Runtime& runtime_;std::weak_ptr<VerticalWorldSession> session_;
    RuntimeZone& zone_;std::shared_ptr<GroundItemPool> pool_;std::shared_ptr<ItemIds> ids_;std::shared_ptr<ItemIdClock> clock_;
    std::shared_ptr<InventoryWorld> services_;std::shared_ptr<VerticalWorldInventorySettlementScratch> scratch_;Address address_;bool active_=false;
};
}
