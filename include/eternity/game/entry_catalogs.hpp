#pragma once
#include "eternity/game/players.hpp"
#include <functional>

namespace eternity::game {
enum class EntryCatalogUpdate {applied,unsupported,rejected};
struct FriendEntry {
    std::uint32_t id=0;std::uint8_t family=0,grow=0,job=0;
    std::string name,memo; // Original wire bytes, name <=20, memo <=50.
};
struct BlockEntry {std::uint32_t id=0;std::string name,memo;};
class SocialCatalog {
public:
    using Lookup=std::function<const SocialCatalog*(std::uint32_t character_id)>;
    SocialCatalog(std::uint32_t account,std::uint32_t character):account_(account),character_(character) {friends_.reserve(100);blocked_.reserve(100);}
    void clear();
    void add_friend(FriendEntry); // Original Add returns false even when it appends.
    void add_block(BlockEntry); // Both append duplicates, ignoring entries after 100.
    const FriendEntry* find(std::uint32_t id) const;
    std::span<const FriendEntry> friends() const noexcept {return friends_;}
    std::span<const BlockEntry> blocked() const noexcept {return blocked_;}
    std::uint32_t account_id() const noexcept {return account_;}
    std::uint32_t character_id() const noexcept {return character_;}
    EntryCatalogUpdate apply_db(std::span<const std::uint8_t>); // 0c01 / 0c05.
    // Lookup/sends must not mutate these lists. InitFriend notifies every present
    // nonzero friend, whereas 2201 marks online only with a mutual relationship.
    void notify_online(const Lookup&,const std::function<void(std::uint32_t,net::Packet)>&) const;
    void send_packets(const Lookup&,const std::function<void(net::Packet)>&) const;
    std::vector<net::Packet> packets(const Lookup&) const;
private:
    std::uint32_t account_,character_;std::vector<FriendEntry> friends_;std::vector<BlockEntry> blocked_;
};
struct EntrySavePoint {std::uint32_t map=65535;float x=0,y=0,z=0;};
struct EntryPurchaseLog {std::array<std::uint8_t,21> code{};std::uint32_t made_at=0,points=0;};
struct EntryPointItem {std::uint8_t type=0,type_id=0,count=0;};
class EntryCatalogs {
public:
    explicit EntryCatalogs(std::uint32_t account):account_(account) {}
    EntryCatalogUpdate apply_db(std::span<const std::uint8_t>); // 0d01 / 0e01 / 0e04.
    const std::array<EntrySavePoint,4>& save_points() const noexcept {return points_;}
    const std::array<EntryPurchaseLog,10>& history() const noexcept {return history_;}
    const std::array<EntryPointItem,240>& stored() const noexcept {return stored_;}
    std::uint32_t cash_points() const noexcept {return cash_;}
    std::uint32_t account_id() const noexcept {return account_;}
    std::optional<net::Packet> save_point_packet() const;
    net::Packet history_packet() const; // Always sends 2601, including empty history.
    std::optional<net::Packet> stored_packet() const; // 2606 omits zero counts.
private:
    std::uint32_t account_,cash_=0;std::array<EntrySavePoint,4> points_{};
    std::array<EntryPurchaseLog,10> history_{};std::array<EntryPointItem,240> stored_{};
};
// ps_game.exe SendSavePoint 00483d60, Honor 00483ea0, KCStatus 00481920.
std::optional<net::Packet> character_save_points(std::span<const EntrySavePoint,4>);
net::Packet character_honor(std::uint16_t value);
net::Packet character_kill_count(std::uint8_t country,std::uint32_t points,std::uint32_t expires,std::uint32_t tick);
struct GuildListing {
    std::uint32_t id=0,country=0,points=0;std::uint8_t rank=0;
    std::string name,master,notice; // Wire limits 24 / 20 / 64 bytes.
};
// CGuildMgr::SendGuildList 00432000: key order, country filter, 14 rows per batch.
std::vector<net::Packet> guild_listings(const std::map<std::uint32_t,GuildListing>&,std::uint8_t country);
}
