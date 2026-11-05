#pragma once
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/actor.hpp"

namespace eternity::game {
struct GuildApplicationCreatePorts {
    std::function<void(GuildUser&,net::Packet)> send_user;
    std::function<void(net::Packet)> send_db;
    std::function<void(GuildUser&,Actor&,const LoadedGuild&)> game_log;
};
// CGuild::JoinRequest 0042e120. The caller must perform PacketGuild's outer
// deadline, eligibility, target lookup and previous-application cancellation.
// Caller retains actor, user, catalog, target and transports through callbacks.
inline EntryCatalogUpdate guild_application_create(GuildUser& user,Actor& actor,LoadedGuild& target,
    GuildCatalog& catalog,const GuildApplicationCreatePorts& ports) {
    if(!ports.send_user)throw std::invalid_argument("Missing guild application client transport");
    if(user.character_id!=actor.id() || catalog.find_membership(&target.membership)!=&target)throw std::logic_error("Guild application owner mismatch");
    std::lock_guard guard(target.mutex);
    auto reject=[&]{ports.send_user(user,net::Packet{7,13,0});return EntryCatalogUpdate::applied;};
    const auto found=target.membership.applications.find(actor.id());
    if(found!=target.membership.applications.end() && found->second)return reject();
    auto record=catalog.allocate_application();if(!record)return reject();
    if(!ports.send_db || !ports.game_log)throw std::invalid_argument("Missing guild application DB or log transport");
    const auto& identity=actor.appearance().identity;
    if(identity.name.size()>20 || identity.name.find('\0')!=identity.name.npos)throw std::invalid_argument("Invalid guild applicant name");
    record->id=actor.id();record->name=identity.name;record->level=actor.status().level;record->job=identity.job;record->rank=0;record->billing_id=0;record->user=nullptr;
    user.request_id=target.metadata.listing.id;target.membership.applications.emplace(record->id,record);catalog.directory().add(record->id,&target.membership);
    auto append=[](net::Packet& p,std::uint32_t value,unsigned width=4){for(unsigned i=0;i<width;++i)p.push_back(static_cast<std::uint8_t>(value>>(8*i)));};
    net::Packet db{7,11};append(db,target.metadata.listing.id);append(db,record->id);db.push_back(0);ports.send_db(std::move(db));
    net::Packet admin{0x17,13};append(admin,record->id);append(admin,static_cast<std::uint32_t>(record->level),2);admin.push_back(static_cast<std::uint8_t>(record->job));admin.insert(admin.end(),record->name.begin(),record->name.end());admin.resize(30,0);target.membership.broadcast_admin(std::move(admin));
    ports.send_user(user,net::Packet{7,13,1});ports.game_log(user,actor,target);return EntryCatalogUpdate::applied;
}
}
