#pragma once
#include "eternity/game/guild_client_accept.hpp"
#include "eternity/game/actor.hpp"

namespace eternity::game {
// CUser::JoinGuild, ps_game.exe 0042e7a0..0042e9df. Caller retains the
// session, actor, catalog, guild and all actual transports across callbacks.
inline void guild_invitation_join(GuildUser& user,Actor& actor,LoadedGuild& guild,
    GuildCatalog& catalog,const GuildApplicationAcceptPorts& ports) {
    if(!ports.send_user || !ports.send_db || !ports.send_bank || !ports.game_log || !ports.tick || !ports.set_join_deadline)
        throw std::invalid_argument("Missing guild invitation join services");
    if(actor.id()!=user.character_id || catalog.find_membership(&guild.membership)!=&guild)
        throw std::logic_error("Guild invitation join owner mismatch");
    const auto& name=actor.appearance().identity.name;const auto& guild_name=guild.metadata.listing.name;
    if(name.size()>20 || name.find('\0')!=name.npos || guild_name.size()>24 || guild_name.find('\0')!=guild_name.npos)
        throw std::invalid_argument("Invalid guild invitation join name");
    std::lock_guard guard(guild.mutex);
    auto record=catalog.allocate_application();if(!record)throw std::runtime_error("Guild member allocation failed");
    record->id=actor.id();record->name=name;record->job=actor.appearance().identity.job;record->level=actor.status().level;
    record->rank=9;record->user=&user;record->billing_id=0;
    user.rank=9;user.guild_id=guild.metadata.listing.id;user.guild=&guild.membership;
    guild.membership.online.emplace(record->id,record);catalog.directory().add(record->id,&guild.membership);
    auto append=[](net::Packet& p,std::uint32_t value,unsigned width=4){for(unsigned i=0;i<width;++i)p.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    net::Packet db{7,11};append(db,guild.metadata.listing.id);append(db,record->id);db.push_back(9);ports.send_db(std::move(db));
    net::Packet response{8,13,1};append(response,guild.metadata.listing.id);response.push_back(9);response.insert(response.end(),guild_name.begin(),guild_name.end());response.resize(33,0);ports.send_user(user,std::move(response));
    net::Packet notice{0x14,13,1};append(notice,record->id);notice.push_back(9);append(notice,static_cast<std::uint32_t>(record->level),2);notice.push_back(static_cast<std::uint8_t>(record->job));notice.insert(notice.end(),record->name.begin(),record->name.end());notice.resize(32,0);guild.membership.broadcast(std::move(notice));
    guild.membership.send_users(user);ports.game_log(*record,&user,guild);ports.send_bank(user,guild);
    if(auto npc=guild.npcs.client_packet())ports.send_user(user,std::move(*npc));
    ports.set_join_deadline(user,ports.tick()+0x0f731400u);
}
}
