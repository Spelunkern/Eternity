#pragma once
#include "eternity/net/framing.hpp"
#include <functional>
#include <array>
#include <map>
#include <memory>
#include <string>

namespace eternity::game {
class GuildMembership;
struct GuildUser {
    std::uint32_t character_id=0,guild_id=0,request_id=0,ranking_entry=0;
    std::int32_t rank=0;
    GuildMembership* guild=nullptr;
};
struct GuildMember {
    std::uint32_t id=0,billing_id=0,ranking_entry=0;
    std::string name;
    std::int32_t job=0,level=0,rank=0;
    GuildUser* user=nullptr;
};
struct GuildMemberPorts {
    // One original shared packet allocation per broadcast, even with no peers.
    std::function<bool(const net::Packet&)> allocate;
    std::function<void()> release;
    std::function<void(GuildUser&,net::Packet)> send;
};
// Single world executor; callbacks must not mutate these maps or throw. Records
// remain stable through online/offline transitions. Maps permit null source rows.
class GuildMembership {
public:
    using Record=std::shared_ptr<GuildMember>;
    using Members=std::map<std::uint32_t,Record>;
    GuildMembership(std::uint32_t id,GuildMemberPorts);
    GuildMembership(const GuildMembership&)=delete;
    GuildMembership& operator=(const GuildMembership&)=delete;
    Members online,offline,applications;
    std::array<Record,7> leaders{};
    std::uint32_t id() const noexcept {return id_;}
    bool connect(GuildUser&); // CGuild::ConnectUser, ps_game.exe 0042d1c0.
    bool disconnect(GuildUser&); // 0042d300, does not clear the user's guild fields.
    void broadcast(net::Packet) const; // CGuild::Send 0042c050.
    void send_users(GuildUser&) const; // 0042c980: online, offline, billed offline.
    void send_applications(GuildUser&) const; // 0042ce10.
private:
    std::uint32_t id_;GuildMemberPorts ports_;
};
class GuildDirectory {
public:
    void add(std::uint32_t character,GuildMembership*); // AddGuildUser 00432ab0, first wins.
    bool connect(GuildUser&); // CGuildMgr::ConnectUser 00431730.
    void initialize(GuildUser&); // InitGuild 004762c0, then SendCharacterGuild.
    void release(GuildUser&); // ReleaseGuild 00476300, serialized captured guild.
    void send(GuildUser&); // SendCharacterGuild 00476390.
private:
    std::map<std::uint32_t,GuildMembership*> by_character_;
};
}
