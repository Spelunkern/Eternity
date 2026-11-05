#pragma once
#include "eternity/game/vertical_world_inventory_actions.hpp"
#include "eternity/game/zone_leave_exchange.hpp"

namespace eternity::game {
// CUser::CancleActionExc 0044b3d0: the retained session/zone executor owns
// both actors. Each retained session owns its canonical +5830 exchange deadline.
class VerticalWorldInventoryCancellation final {
public:
    VerticalWorldInventoryCancellation(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        const std::shared_ptr<VerticalWorldSession>& session,RuntimeZone& zone)
        :strand_(std::move(strand)),registry_(registry),session_(session),zone_(zone) {
        if(!strand_.running_in_this_thread() || !session)throw std::invalid_argument("Missing inventory cancellation owners");
    }
    void cancel(VerticalWorldSession& session,Actor& actor) {
        auto owner=checked(session,actor);if(active_)throw std::logic_error("Reentrant inventory cancellation");
        struct Guard {bool& active;explicit Guard(bool& value):active(value){active=true;}~Guard(){active=false;}} guard(active_);
        with_pass(*owner,[&]{cancel_live(session,actor,owner);});
    }
private:
    template<class F> static void with_pass(VerticalWorldSession& session,F&& work) {
        session.begin_admission_pass();std::exception_ptr failure;
        try {work();} catch(...) {failure=std::current_exception();}
        try {session.end_admission_pass();} catch(...) {if(!failure)failure=std::current_exception();}
        if(failure)std::rethrow_exception(failure);
    }
    void cancel_live(VerticalWorldSession& session,Actor& actor,const std::shared_ptr<VerticalWorldSession>& owner) {
        std::shared_ptr<VerticalWorldSession> exchange_peer;
        if(actor.exchange().mode!=0 && actor.exchange().peer) {
            auto* peer=actor.exchange().peer;exchange_peer=registry_.retain_actor_pointer(peer);
            if(!exchange_peer || exchange_peer->closing() || !exchange_peer->world_user() ||
               &exchange_peer->world_user()->actor!=peer || !exchange_peer->character() ||
               exchange_peer->character()->pipeline().result().actor.get()!=peer || exchange_peer->admission().location!=4 ||
               exchange_peer->admission().zone!=&zone_ || zone_.users->find_actor(peer->id())!=peer)
                throw std::logic_error("Cancellation exchange peer has no admitted session");
        }
        const auto exchange=[&]{zone_leave_exchange(actor,[&](Actor& peer)->std::shared_ptr<void>{return retain(peer);},[&](Actor& subject)->std::uint32_t& {
            auto target=retain(subject);auto& value=target->exchange_timeout();checked(session,actor);
            if(retain(subject)!=target)throw std::logic_error("Exchange timer lost its session");return value;
        });};
        if(exchange_peer && exchange_peer!=owner)with_pass(*exchange_peer,exchange);else exchange();
        checked(session,actor);
        if(actor.relations().duel_state!=5)return;
        auto* peer=zone_.users->find_actor(actor.relations().duel_opponent);if(!peer)return;
        auto peer_owner=retain(*peer);
        const auto clear=[&](Actor& subject,const std::shared_ptr<VerticalWorldSession>& expected) {
            subject.clear_duel_debuffs();checked(session,actor);
            if(retain(subject)!=expected)throw std::logic_error("Duel cancellation lost its actor");
            subject.reset_damage_ledger();subject.relations().duel_state=0;subject.relations().duel_type=-1;subject.status().current_target_type=0;
        };
        const auto duel=[&]{clear(*peer,peer_owner);clear(actor,owner);
            actor.send_self({8,0x24,2});checked(session,actor);
            if(retain(*peer)!=peer_owner)throw std::logic_error("Duel cancellation lost its peer");
            peer->send_self({8,0x24,3});checked(session,actor);};
        if(peer_owner!=owner)with_pass(*peer_owner,duel);else duel();
    }
public:
    static VerticalWorldInventoryActions::Cancel bind(std::shared_ptr<VerticalWorldInventoryCancellation> owner) {
        if(!owner)throw std::invalid_argument("Missing inventory cancellation owner");
        return [owner=std::move(owner)](VerticalWorldSession& session,Actor& actor){owner->cancel(session,actor);};
    }
private:
    std::shared_ptr<VerticalWorldSession> retain(const Actor& actor) {
        auto owner=registry_.retain_actor(actor);
        if(!owner || owner->closing() || !owner->world_user() || &owner->world_user()->actor!=&actor ||
           !owner->character() || owner->character()->pipeline().result().actor.get()!=&actor ||
           owner->admission().location!=4 || owner->admission().zone!=&zone_ || !zone_.users ||
           zone_.users->find_actor(actor.id())!=&actor)
            throw std::logic_error("Cancellation actor has no admitted session");
        return owner;
    }
    std::shared_ptr<VerticalWorldSession> checked(VerticalWorldSession& supplied,const Actor& actor) {
        if(!strand_.running_in_this_thread())throw std::logic_error("Inventory cancellation outside world strand");
        auto owner=session_.lock();
        if(!owner || owner.get()!=&supplied || owner->closing() || !owner->world_user() || &owner->world_user()->actor!=&actor ||
           owner->admission().location!=4 || owner->admission().zone!=&zone_ || !zone_.users ||
           zone_.users->find_actor(actor.id())!=&actor || registry_.retain_actor(actor)!=owner)
            throw std::logic_error("Inventory cancellation lost admitted actor");
        return owner;
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;std::weak_ptr<VerticalWorldSession> session_;
    RuntimeZone& zone_;bool active_=false;
};
}
