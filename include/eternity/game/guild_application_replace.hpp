#pragma once
#include "eternity/game/guild_application_cancel.hpp"
#include "eternity/game/guild_application_create.hpp"

namespace eternity::game {
// PacketGuild 00476ead..00476ee9. The world executor owns the catalog and
// retains the target and old guild across the synchronous transport callbacks.
// Caller has completed deadline, eligibility, creation and country checks.
inline EntryCatalogUpdate guild_application_replace(GuildUser& user,Actor& actor,LoadedGuild& target,
    GuildCatalog& catalog,const GuildApplicationCreatePorts& ports) {
    if(!ports.send_user || !ports.send_db || !ports.game_log)throw std::invalid_argument("Missing guild application services");
    if(user.character_id!=actor.id() || catalog.find_membership(&target.membership)!=&target)throw std::logic_error("Guild application owner mismatch");
    if(user.request_id)if(auto* old=catalog.find(user.request_id))guild_application_cancel(user,*old,catalog,ports.send_db);
    return guild_application_create(user,actor,target,catalog,ports);
}
}
