#include "eternity/game/duel.hpp"
#include "eternity/game/actor.hpp"
#include <algorithm>

namespace eternity::game {
UserPosition random_drop_position(const Actor& owner,DuelWorld& world) {
    // GetDropPosRand 0048a270: float offsets round before addition to live coordinates.
    const auto dx=static_cast<float>(static_cast<double>(world.drop_random()%1000)/1000.0-0.5);
    const auto dz=static_cast<float>(static_cast<double>(world.drop_random()%1000)/1000.0-0.5);
    const auto position=world.duel_position(owner);
    return {position.x+dx,position.y,position.z+dz};
}
void drop_duel_wager(Actor& owner,Actor& recipient,DuelWorld& world) {
    // ps_game.exe CUser::ItemDropPvP 0047c990. EnterItem failures do not retain stakes.
    auto& wager=owner.duel_wager();
    if(wager.money!=0) if(auto* drop=world.allocate_drop_item()) {
        drop->definition=nullptr;drop->item.type=26;drop->item.type_id=0;drop->item.count=1;drop->item.quality=0;drop->item.make_type="U";
        drop->user_id=recipient.id();drop->position=random_drop_position(owner,world);drop->drop_type=3;drop->drop_id=owner.id();drop->party_id=0;drop->money=wager.money;
        // InitCraftExpansion 004ba280 returns immediately for the null money definition.
        (void)world.enter_duel_item(owner,*drop,true);
    }
    for(auto* drop:wager.items) if(drop) {
        drop->position=random_drop_position(owner,world);drop->user_id=recipient.id();drop->party_id=0;drop->drop_type=3;drop->drop_id=owner.id();
        drop->position=random_drop_position(owner,world);(void)world.enter_duel_item(owner,*drop,true);
    }
    wager.money=0;wager.items.fill(nullptr);
}
void Actor::leave_duel_on_attack(DuelWorld& world) {
    // CUser::PvPChkLeaveByAttack 0047c750. A missing peer leaves all local state intact.
    if(relations_.duel_state==0) return;
    auto* peer=world.find_duel_user(*this,relations_.duel_opponent);if(!peer) return;
    const auto state=relations_.duel_state;
    if(state<1 || state>5) return;
    if(state==3 || state==4) {drop_duel_wager(*peer,*peer,world);drop_duel_wager(*this,*this,world);}
    peer->end_duel();end_duel();
    if(state==5) {send_self({0x08,0x24,2});peer->send_self({0x08,0x24,3});return;}
    const auto identity=state==1?peer->id():id_;
    net::Packet packet{static_cast<std::uint8_t>(state==3 || state==4?0x06:0x02),0x24,3};
    for(unsigned int i=0;i<4;++i) packet.push_back(static_cast<std::uint8_t>(identity>>(8*i)));
    peer->send_self(packet);send_self(packet);
    if(state==3 || state==4) {
        // Two 0e03 logs reuse one stack buffer; shorter second names retain earlier tails.
        net::Packet log(100,0);log[0]=3;log[1]=0x0e;log[99]=3;
        const auto emit=[&](Actor& subject,Actor& other) {
            const auto context=world.duel_log_subject(subject);write_game_log_main(log,context,world.local_calendar());
            const auto target=world.duel_log_subject(other);
            for(unsigned int i=0;i<4;++i) log[74+i]=static_cast<std::uint8_t>(target.character_id>>(8*i));
            if(target.character_name.size()>20 || target.character_name.find('\0')!=std::string::npos) throw std::invalid_argument("Invalid duel log target name");
            std::copy(target.character_name.begin(),target.character_name.end(),log.begin()+78);log[78+target.character_name.size()]=0;world.send_duel_log(log);
        };
        emit(*peer,*this);emit(*this,*peer);
    }
}
}
