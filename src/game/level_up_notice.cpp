#include "eternity/game/level_up_notice.hpp"
#include <stdexcept>
namespace eternity::game {
namespace {
void append(net::Packet& packet,std::uint32_t value,unsigned n=4) {for(unsigned i=0;i<n;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
}
void notify_level_up(const PlayerCore& player,std::uint32_t actor_id,const PlayerResources& maximum,const PlayerResources& base,const Experience& curve,std::uint32_t amount,std::uint32_t source,LevelUpNoticeWorld& world) {
    world.rebuild_attack();const auto& c=player.character;
    net::Packet database{1,6};append(database,player.user_id);append(database,c.level,2);append(database,player.experience);append(database,player.stat_points,2);append(database,player.skill_points,2);
    for(auto value:{c.strength,c.dexterity,c.intelligence,c.wisdom,c.recovery,c.luck}) append(database,value,2);
    for(auto value:{maximum.hp,maximum.mp,maximum.sp}) append(database,value,2);
    world.send_database(database);
    if(amount!=0) {net::Packet gain{7,2};append(gain,amount);append(gain,0);world.send_self(gain);}
    const auto bounds=curve.bounds(c.grow,c.level);net::Packet scene;append(scene,source==actor_id?0x508:0x51e,2);append(scene,actor_id);append(scene,c.level,2);append(scene,player.stat_points,2);append(scene,player.skill_points,2);append(scene,bounds.previous);append(scene,bounds.next);
    if(world.in_zone()) world.send_scene(scene,60.0F,5);
    if(world.acquire_party()) {
        try {world.update_party_experience();const auto party=world.party_type();if(party>1) throw std::invalid_argument("Invalid level-up party type");net::Packet notice;append(notice,party==0?0xc09:0xc13,2);append(notice,actor_id);append(notice,c.level,2);world.send_party(notice);} catch(...) {world.release_party();throw;}
        world.release_party();
    }
    world.energy_all_max();world.recover_add(base);net::Packet log(74);world.fill_game_log_main(log);log[0]=8;log[1]=3;world.send_game_log(log);world.update_guild_level(actor_id,c.level);
}
}
