#pragma once
#include "eternity/game/vertical_inventory.hpp"
#include "eternity/game/vertical_world_session_gameplay.hpp"
#include <algorithm>

namespace eternity::game {
// Retained CUser::PacketExchange invitation, response and non-settlement control. The world strand owns
// both actors, zone and packet; action and send callbacks must not mutate the
// packet or destroy participants. Other opcodes remain with the next owner.
class VerticalWorldInventoryExchange final:public std::enable_shared_from_this<VerticalWorldInventoryExchange> {
public:
    using Other=VerticalWorldSessionGameplay::Other;
    static std::shared_ptr<VerticalWorldInventoryExchange> create(VerticalWorldSession::Strand strand,
        VerticalWorldSession::Registry& registry,const std::shared_ptr<VerticalWorldSession>& session,
        RuntimeZone& zone,std::shared_ptr<VerticalInventoryActions> actions,Other next,
        std::shared_ptr<VerticalExchangeSettlement> settlement={}) {
        if(!strand.running_in_this_thread() || !session || !actions || !next)throw std::invalid_argument("Incomplete retained exchange owners");
        auto result=std::shared_ptr<VerticalWorldInventoryExchange>(new VerticalWorldInventoryExchange(std::move(strand),registry,session,zone,std::move(actions),std::move(next),std::move(settlement)));
        result->checked(*session);return result;
    }
    void dispatch(VerticalWorldSession& supplied,const net::Packet& packet) {
        auto self=shared_from_this();auto owner=checked(supplied);
        if(packet.size()<2)throw std::invalid_argument("Malformed exchange dispatch");
        if(packet[1]!=0x0a || (packet[0]!=1 && packet[0]!=2 && packet[0]!=5 && packet[0]!=6 && packet[0]!=7 && packet[0]!=8)) {next_(supplied,packet);return;}
        if(packet.size()<((packet[0]==1 || packet[0]==6 || packet[0]==8)?6U:3U))throw std::invalid_argument("Malformed exchange request");
        const bool ready=packet[0]==5 && packet[2]==0;
        if(ready && !settlement_) {next_(supplied,packet);return;}
        if(active_)throw std::logic_error("Reentrant exchange invitation");
        struct Guard {bool& active;explicit Guard(bool& value):active(value){active=true;}~Guard(){active=false;}} guard(active_);
        struct Actions final:VerticalInventoryActions {
            VerticalWorldInventoryExchange& parent;VerticalWorldSession& session;VerticalInventoryActions& actions;const net::Packet& packet;
            std::shared_ptr<VerticalWorldSession> peer;Actor* peer_actor=nullptr;bool peer_pass=false;GroundItem* item=nullptr;GroundItemPool* pool=nullptr;net::Packet item_snapshot;
            const ItemDefinition* definition=nullptr;std::array<std::uint8_t,21> craft_snapshot{};
            Actions(VerticalWorldInventoryExchange& p,VerticalWorldSession& s,VerticalInventoryActions& a,const net::Packet& bytes):parent(p),session(s),actions(a),packet(bytes) {}
            ~Actions() override {if(pool)pool->unpin(*item);}
            void cancel_action_exchange(Actor& actor) override {actions.cancel_action_exchange(actor);parent.checked(session);}
            void pin_peer(Actor* target) {
                if(peer_actor && peer_actor!=target)throw std::logic_error("Exchange peer changed during action");
                if(!target || peer)return;
                peer=parent.registry_.retain_actor_pointer(target);
                if(!peer || peer->closing() || !peer->world_user() || &peer->world_user()->actor!=target || !peer->character() ||
                   peer->character()->pipeline().result().actor.get()!=target || peer->admission().location!=4 ||
                   peer->admission().zone!=&parent.zone_ || parent.zone_.users->find_actor(target->id())!=target)
                    throw std::logic_error("Exchange peer has no admitted session");
                peer_actor=target;
                if(peer.get()!=&session) {peer->begin_admission_pass();peer_pass=true;}
            }
            void end_shop(Actor& actor) override {
                actions.end_shop(actor);parent.checked(session);
                Actor* target=nullptr;
                if(packet[0]==1 && actor.exchange().mode==0) {
                    std::uint32_t id=0;for(unsigned i=0;i<4;++i)id|=std::uint32_t(packet[2+i])<<(8*i);
                    target=parent.zone_.users->find_actor(id);
                } else if((packet[0]==2 && actor.exchange().mode==2) ||
                          ((packet[0]==5 || packet[0]==6 || packet[0]==7 || packet[0]==8) && actor.exchange().mode==3)) target=actor.exchange().peer;
                pin_peer(target);
                const auto& offer=actor.exchange();
                if(packet[0]==6 && offer.mode==3 && packet[2]>0 && packet[2]<=9 &&
                   static_cast<std::int32_t>(packet[2])<=actor.status().skills.max_bags && packet[3]<24 && packet[4] &&
                   packet[5]<offer.items.size() && !offer.items[packet[5]].bag && offer.peer &&
                   std::none_of(offer.items.begin(),offer.items.end(),[&](const auto& value){return value.bag==packet[2] && value.slot==packet[3];})) {
                    if(const auto held=actor.retained_inventory_item(packet[2],packet[3])) {
                        if(!held->item)throw std::logic_error("Exchange item has no allocation");
                        if(!held->pool || !held->pool->pin(*held->item))throw std::logic_error("Exchange item has no allocated pool owner");
                        item=held->item;pool=held->pool;
                        definition=item->definition;craft_snapshot=item->craft_storage;
                        if(definition)item_snapshot=inventory_add_packet(actor.account_id(),packet[2],packet[3],*item);
                    }
                }
            }
            void finish_item() noexcept {if(pool) {pool->unpin(*item);pool=nullptr;item=nullptr;}}
            void finish_peer() {if(peer_pass) {peer_pass=false;peer->end_admission_pass();}}
            bool pickup_checks() override {const auto value=actions.pickup_checks();parent.checked(session);return value;}
        } actions(*this,supplied,*actions_,packet);
        owner->begin_admission_pass();std::exception_ptr failure;
        try {
            auto& actor=owner->world_user()->actor;
            if(ready && actor.exchange().mode==3)actions.pin_peer(actor.exchange().peer);
            const auto result=ready?vertical_inventory_exchange_guarded_ready(actor,packet,actions,*settlement_):
                packet[0]==1?vertical_inventory_exchange_invite(actor,packet,*zone_.users,owner->exchange_timeout(),actions):
                vertical_inventory_exchange_request(actor,packet,actions);
            if(result==VerticalExchangeRequest::malformed || result==VerticalExchangeRequest::unhandled)throw std::logic_error("Exchange invitation lost packet identity");
            checked(supplied);
            if(actions.pool) {
                const auto held=owner->world_user()->actor.retained_inventory_item(packet[2],packet[3]);
                if(!actions.pool->is_allocated(*actions.item) || !held || held->item!=actions.item || held->pool!=actions.pool ||
                   actions.item->definition!=actions.definition || actions.item->craft_storage!=actions.craft_snapshot ||
                   (actions.definition && inventory_add_packet(owner->account_id(),packet[2],packet[3],*actions.item)!=actions.item_snapshot))
                    throw std::logic_error("Exchange offered item lost its retained owner");
            }
            if(actions.peer && (!actions.peer->world_user() || registry_.retain_actor(actions.peer->world_user()->actor)!=actions.peer))
                throw std::logic_error("Exchange peer lost its session");
        } catch(...) {failure=std::current_exception();}
        actions.finish_item();
        try {actions.finish_peer();} catch(...) {if(!failure)failure=std::current_exception();}
        try {owner->end_admission_pass();} catch(...) {if(!failure)failure=std::current_exception();}
        if(failure)std::rethrow_exception(failure);
    }
    static Other bind(std::shared_ptr<VerticalWorldInventoryExchange> owner) {
        if(!owner)throw std::invalid_argument("Missing retained exchange owner");
        return [owner=std::move(owner)](VerticalWorldSession& session,const net::Packet& packet){owner->dispatch(session,packet);};
    }
    // Called by the world's retained session timer on the same strand. The
    // caller supplies the same unsigned tick domain used by Actor::tick_ms().
    bool expire(VerticalWorldSession& supplied,std::uint32_t now) {
        auto self=shared_from_this();auto owner=checked(supplied);
        if(active_)throw std::logic_error("Reentrant exchange expiry");
        auto& timeout=owner->exchange_timeout();auto& actor=owner->world_user()->actor;
        if(!timeout || timeout>now || (actor.exchange().mode!=1 && actor.exchange().mode!=2))return false;
        auto* target=actor.exchange().peer;std::shared_ptr<VerticalWorldSession> peer;
        if(target) {
            peer=registry_.retain_actor_pointer(target);
            if(!peer || peer->closing() || !peer->world_user() || &peer->world_user()->actor!=target ||
               !peer->character() || peer->character()->pipeline().result().actor.get()!=target ||
               peer->admission().location!=4 || peer->admission().zone!=&zone_ || zone_.users->find_actor(target->id())!=target)
                throw std::logic_error("Exchange expiry peer has no admitted session");
        }
        return vertical_inventory_exchange_expire(actor,now,timeout,peer?&peer->exchange_timeout():nullptr);
    }
private:
    VerticalWorldInventoryExchange(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        const std::shared_ptr<VerticalWorldSession>& session,RuntimeZone& zone,std::shared_ptr<VerticalInventoryActions> actions,Other next,
        std::shared_ptr<VerticalExchangeSettlement> settlement)
        :strand_(std::move(strand)),registry_(registry),session_(session),zone_(zone),actions_(std::move(actions)),next_(std::move(next)),settlement_(std::move(settlement)) {}
    std::shared_ptr<VerticalWorldSession> checked(VerticalWorldSession& supplied) {
        if(!strand_.running_in_this_thread())throw std::logic_error("Exchange outside world strand");
        auto owner=session_.lock();
        if(!owner || owner.get()!=&supplied || owner->closing() || !owner->character() || !owner->world_user() ||
           owner->character()->pipeline().result().actor.get()!=&owner->world_user()->actor || owner->admission().location!=4 ||
           owner->admission().zone!=&zone_ || !zone_.users || zone_.users->find_actor(owner->world_user()->actor.id())!=&owner->world_user()->actor ||
           registry_.retain_actor(owner->world_user()->actor)!=owner)throw std::logic_error("Exchange lost admitted actor");
        return owner;
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;std::weak_ptr<VerticalWorldSession> session_;
    RuntimeZone& zone_;std::shared_ptr<VerticalInventoryActions> actions_;Other next_;std::shared_ptr<VerticalExchangeSettlement> settlement_;bool active_=false;
};
}
