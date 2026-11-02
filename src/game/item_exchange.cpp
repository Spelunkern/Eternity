#include "eternity/game/item_exchange.hpp"
#include "eternity/game/actor.hpp"

namespace eternity::game {
void Actor::cancel_exchange_ready(Actor& peer,bool duel) {
    // ps_game.exe ExchangeCancleReady 0046f450 / ExchangeCancleReadyPvP 0047d0a0.
    auto& local=duel?duel_exchange_:exchange_;auto& remote=duel?peer.duel_exchange_:peer.exchange_;
    if(local.ready==0 && remote.ready==0) return;
    local.ready=0;remote.ready=0;
    const net::Packet packet{static_cast<std::uint8_t>(duel?9:5),static_cast<std::uint8_t>(duel?0x24:0x0a),3,1};
    send_self(packet);peer.send_self(packet);
}
void Actor::remove_exchange_item(std::int32_t bag,std::int32_t slot,ItemExchangeWorld& world) {
    // Complete ExchangeItemRemove 0046f310. The original marks an offer empty
    // by clearing only its bag byte; stale slot/count and later duplicates remain.
    if(exchange_.mode==3) if(auto* peer=exchange_.peer) {
        for(std::size_t i=0;i<exchange_.items.size();++i) {
            auto& item=exchange_.items[i];if(item.bag!=bag || item.slot!=slot) continue;
            cancel_exchange_ready(*peer,false);item.bag=0;
            net::Packet packet{7,0x0a,1,static_cast<std::uint8_t>(i)};send_self(packet);packet[2]=2;peer->send_self(packet);break;
        }
    }
    // Read live state after trade callbacks. Finding the duel peer precedes
    // searching its local offer, even when no reference will match.
    if(relations_.duel_state!=5 || duel_exchange_.mode!=3) return;
    auto* peer=world.find_exchange_user(*this,relations_.duel_opponent);if(!peer) return;
    for(std::size_t i=0;i<duel_exchange_.items.size();++i) {
        auto& item=duel_exchange_.items[i];if(item.bag!=bag || item.slot!=slot) continue;
        cancel_exchange_ready(*peer,true);item.bag=0;
        net::Packet packet{0x0b,0x24,1,static_cast<std::uint8_t>(i)};send_self(packet);packet[2]=2;peer->send_self(packet);return;
    }
}
}
