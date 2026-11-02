#include "eternity/game/experience_award.hpp"
#include <bit>
#include <cmath>
namespace eternity::game {
namespace {
void append(net::Packet& packet,std::uint32_t value) {for(unsigned i=0;i<4;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void save(const PlayerCore& player,ExperienceAwardPorts& ports) {net::Packet packet{2,6};append(packet,player.user_id);append(packet,player.experience);ports.send_database(packet);}
}
void award_experience(PlayerCore& player,BaseStatus& status,const StatusRules& rules,const Experience& curve,std::uint32_t source,std::int32_t amount,ExperienceAwardOptions options,ExperienceAwardPorts& ports) {
    auto& c=player.character;if(amount==0 || c.level>=(c.grow==0?30:60)) return;
    if(!options.skip_modifiers) {
        double scale=options.double_bonus && options.half_bonus?2.0:options.double_bonus || options.half_bonus?1.5:1.0;
        if(options.bonus_mode>=1 && options.bonus_mode<=3) scale+=0.2;
        const auto converted=static_cast<std::int64_t>(std::trunc(static_cast<double>(amount)*scale));
        amount=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(converted));
    }
    if(amount<0) amount=0;
    const auto gain=static_cast<std::uint32_t>(amount),extra=options.normal_party_seven?gain:0U,total=player.experience+gain;
    if(total<curve.bounds(c.grow,c.level).next) {
        player.experience=total+extra;save(player,ports);net::Packet packet{7,2};append(packet,gain);append(packet,extra);ports.send_self(packet);return;
    }
    do {
        rules.level_up(player,status);
        if((c.grow==0 && c.level>=30) || c.level>=60) {
            const auto capped=curve.bounds(c.grow,c.level).previous;const auto applied=capped-player.experience;player.experience=capped;save(player,ports);ports.level_up(player,status,applied,source);return;
        }
    } while(total>=curve.bounds(c.grow,c.level).next);
    player.experience=total;save(player,ports);ports.level_up(player,status,gain,source);
}
}
