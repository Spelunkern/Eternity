#include "eternity/game/player_packets.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <limits>

namespace eternity::game {
namespace {
std::int64_t integer(const storage::Row& row,std::size_t index) {
    if(index>=row.size() || !row[index]) throw std::runtime_error("Missing original experience definition");
    const auto& text=*row[index];std::int64_t value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size()) throw std::runtime_error("Invalid experience definition");
    return value;
}
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,unsigned int bytes) {
    for(unsigned int index=0;index<bytes;++index) packet.at(offset+index)=static_cast<std::uint8_t>(value>>(index*8));
}
}
Experience Experience::load(const storage::ResultSet& table) {
    // CGameData::LoadExp, ps_game.exe 004bfe00: SQL Grow is 1..4, then cumulative sum per level.
    if(table.columns!=std::vector<std::string>{"Level","Grow","Exp"}) throw std::runtime_error("Original experience procedure shape changed");
    Experience result;std::array<std::array<bool,61>,4> found{};
    for(const auto& row:table.rows) {
        const auto level=integer(row,0),grow=integer(row,1),experience=integer(row,2);
        if(level<1 || level>60 || grow<1 || grow>4 || experience<std::numeric_limits<std::int32_t>::min() ||
            experience>std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("Experience definition outside original range");
        result.levels_[static_cast<std::size_t>(grow-1)][static_cast<std::size_t>(level)]=static_cast<std::uint32_t>(experience);
        found[static_cast<std::size_t>(grow-1)][static_cast<std::size_t>(level)]=true;
    }
    for(std::size_t grow=0;grow<4;++grow) for(std::size_t level=1;level<=60;++level) {
        if(!found[grow][level]) throw std::runtime_error("Missing original experience level");
        result.levels_[grow][level]+=result.levels_[grow][level-1];
    }
    return result;
}
ExperienceBounds Experience::bounds(std::uint8_t grow,std::uint16_t level) const {
    // CUser::SendCharacterData 00483140 uses level-1/current level and mode 0 at level 60 as fallback.
    const auto fallback=levels_[0][60];
    return {grow<4 && level>=1 && level<=61 ? levels_[grow][level-1] : fallback,
            grow<4 && level<=60 ? levels_[grow][level] : fallback};
}
net::Packet character_data(const PlayerCore& player,PlayerResources maximum,ExperienceBounds experience,std::string_view guild) {
    // TP_CHAR_DATA is sent as 59+strlen(guild), including its final NUL; not a fixed 83-byte record.
    if(guild.size()>24 || guild.find('\0')!=std::string_view::npos) throw std::invalid_argument("Invalid original guild wire name");
    net::Packet packet(59+guild.size(),0);put(packet,0,0x105,2);
    put(packet,2,player.stat_points,2);put(packet,4,player.skill_points,2);put(packet,6,maximum.hp,2);
    put(packet,8,maximum.mp,2);put(packet,10,maximum.sp,2);put(packet,12,player.direction,2);
    put(packet,14,experience.previous,4);put(packet,18,experience.next,4);put(packet,22,player.experience,4);put(packet,26,player.money,4);
    put(packet,30,std::bit_cast<std::uint32_t>(player.x),4);put(packet,34,std::bit_cast<std::uint32_t>(player.y),4);put(packet,38,std::bit_cast<std::uint32_t>(player.z),4);
    for(std::size_t index=0;index<4;++index) put(packet,42+index*4,player.kills[index],4);
    std::copy(guild.begin(),guild.end(),packet.begin()+58);return packet;
}
}
