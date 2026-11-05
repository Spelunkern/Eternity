#pragma once
#include "eternity/game/guild_npc_log.hpp"
#include "eternity/game/ground_items.hpp"
#include "eternity/game/equipment.hpp"
#include "eternity/game/game_log.hpp"

namespace eternity::game {
// Wire effects in ps_game.exe PacketGuild 004773d4..0047766d. These packet
// builders do not detach items, send packets, update guild state, or free pools.
inline net::Packet guild_donation_item_database(std::uint32_t account,std::uint8_t bag,std::uint8_t slot,std::uint8_t count) {
    net::Packet result{2,7};for(unsigned i=0;i<4;++i)result.push_back(static_cast<std::uint8_t>(account>>(8*i)));
    result.insert(result.end(),{bag,slot,count});return result;
}
inline net::Packet guild_donation_item_log(const GameLogSubject& subject,std::chrono::sys_seconds time,const GroundItem& item,
    std::string_view item_name,std::uint8_t bag,std::uint8_t slot) {
    if(!item.definition || item_name.size()>31 || item_name.find('\0')!=item_name.npos)throw std::invalid_argument("Invalid guild donation item log");
    net::Packet p(152,0);write_game_log_main(p,subject,time);p[0]=0x15;p[1]=4;
    const auto put=[&](std::size_t at,std::uint64_t value,unsigned width){for(unsigned i=0;i<width;++i)p[at+i]=static_cast<std::uint8_t>(value>>(i*8));};
    put(74,item.item.uid,8);put(82,item.definition->id,4);std::copy(item_name.begin(),item_name.end(),p.begin()+86);
    std::copy(item.item.gems.begin(),item.item.gems.end(),p.begin()+118);put(124,item.item.made_at,4);p[149]=bag;p[150]=slot;
    return p; // Original final byte and unused string tails are uninitialized.
}
class GuildDonationBatch {
public:
    // Call only after the real Actor has detached the expected eligible item.
    void add(std::uint8_t bag,std::uint8_t slot,const GroundItem& item) {
        if(bag<1 || bag>9 || slot>=24 || entries_.size()>=216 || !item.definition || item.item.make_type.empty() || item.item.make_type.front()!='+')
            throw std::invalid_argument("Invalid detached guild donation item");
        const auto id=item.definition->type_id;std::uint32_t multiplier=0;
        if(id==52)multiplier=1;else if(id==53)multiplier=10;else if(id==54)multiplier=100;else if(id==55)multiplier=1000;
        amount_+=multiplier*item.item.count;entries_.emplace_back(bag,slot);
    }
    std::uint32_t amount() const noexcept{return amount_;}
    net::Packet response() const {
        net::Packet p{0x35,0x0d,static_cast<std::uint8_t>(entries_.size())};
        for(auto [bag,slot]:entries_)p.insert(p.end(),{bag,slot});return p;
    }
private:
    std::uint32_t amount_=0;std::vector<std::pair<std::uint8_t,std::uint8_t>> entries_;
};
inline net::Packet guild_donation_database(std::uint32_t guild,std::uint32_t total,std::uint32_t amount,std::uint32_t account,std::uint32_t returned) {
    net::Packet p{0x12,0x0b};for(auto value:{guild,total,amount,account,returned})for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(value>>(8*i)));return p;
}
inline net::Packet guild_donation_log(const GameLogSubject& subject,std::chrono::sys_seconds time,const LoadedGuild& guild,std::uint32_t amount) {
    const GuildNpcLogUser blank{};auto p=guild_house_log_packet(blank,guild);p.resize(118);p[0]=0x0a;write_game_log_main(p,subject,time);
    for(unsigned i=0;i<4;++i){p[110+i]=static_cast<std::uint8_t>(guild.metadata.etin>>(8*i));p[114+i]=static_cast<std::uint8_t>(amount>>(8*i));}return p;
}
}
