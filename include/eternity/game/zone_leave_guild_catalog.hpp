#pragma once
#include "eternity/game/guild_duel_catalog_movement.hpp"
#include "eternity/game/zone_leave_guild_duel.hpp"
#include "eternity/game/zone_users.hpp"

namespace eternity::game {
// Uses the same catalog-owned guilds and recursive locks as movement. The
// owner must serialize the zone, membership, actor state and session lifetimes;
// retain pins a peer and its scene for the duration of each call.
class ZoneLeaveGuildCatalog final:public ZoneLeaveGuildDuelServices {
public:
    ZoneLeaveGuildCatalog(GuildDuelCatalogMovement& movement,ZoneUsers& zone,ZoneUsers::RetainActor retain)
        :movement_(movement),zone_(zone),retain_(std::move(retain)) {
        if(!retain_)throw std::invalid_argument("Guild leave requires actor retention");
    }
    std::shared_ptr<void> capture_guild_lock(Actor& a) override{return movement_.capture_guild_lock(a);}
    void* current_guild(Actor& a) override{return movement_.current_guild(a);}
    std::int32_t guild_duel_status(void* guild) override{return movement_.guild_duel_status(guild);}
    void leave_guild_duel(void* guild,Actor& actor,std::uint8_t mode) override{movement_.leave_guild_duel(guild,actor,mode);}
    GuildDuelState& duel_state(void* guild) override{return movement_.duel_state(guild);}
    VerticalGuildDuelMovement& movement_state(Actor& actor) override{return movement_.movement_state(actor);}
    Actor* find_in_zone(Actor&,std::uint32_t id) override{return zone_.find_actor(id);}
    std::shared_ptr<void> retain(Actor& actor) override{return retain_(actor);}
private:
    GuildDuelCatalogMovement& movement_;ZoneUsers& zone_;ZoneUsers::RetainActor retain_;
};
}
