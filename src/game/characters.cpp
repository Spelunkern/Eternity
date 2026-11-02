#include "eternity/game/characters.hpp"
#include <algorithm>
#include <charconv>
#include <limits>

namespace eternity::game {
namespace {
std::int64_t integer(const storage::Row& row, std::size_t index) {
    if(index>=row.size() || !row[index]) throw std::runtime_error("Missing character SQL field");
    const auto& text=*row[index];std::int64_t result=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),result);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size()) throw std::runtime_error("Invalid character SQL integer");
    return result;
}
std::uint8_t byte(const storage::Row& row, std::size_t index) {
    const auto value=integer(row,index);
    if(value<0 || value>255) throw std::runtime_error("Character byte outside original range");
    return static_cast<std::uint8_t>(value);
}
std::uint16_t word(const storage::Row& row, std::size_t index) {
    const auto value=integer(row,index);
    // SQL smallint is read as a two-byte value by the original, including its sign bit.
    if(value<std::numeric_limits<std::int16_t>::min() || value>65535) throw std::runtime_error("Character word outside original range");
    return static_cast<std::uint16_t>(value);
}
storage::ResultSet result(std::vector<storage::ResultSet> sets,std::size_t columns) {
    if(sets.size()!=1 || sets[0].columns.size()!=columns) throw std::runtime_error("Original character procedure shape changed");
    return std::move(sets[0]);
}
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,unsigned int bytes) {
    for(unsigned int index=0;index<bytes;++index) packet.at(offset+index)=static_cast<std::uint8_t>(value>>(index*8));
}
std::chrono::sys_seconds calendar(int year,unsigned int month,unsigned int day,int hour,int minute,int second) {
    const std::chrono::year_month_day date{std::chrono::year{year},std::chrono::month{month},std::chrono::day{day}};
    if(!date.ok() || hour<0 || hour>23 || minute<0 || minute>59 || second<0 || second>59)
        throw std::runtime_error("Invalid original calendar value");
    return std::chrono::sys_days{date}+std::chrono::hours{hour}+std::chrono::minutes{minute}+std::chrono::seconds{second};
}
}

std::chrono::sys_seconds unpack_world_time(std::uint32_t value) {
    // CWorld::SetWorldDay, ps_dbAgent.exe 0040f1c0.
    return calendar(static_cast<int>(2000+(value>>26)),(value>>22)&15,(value>>17)&31,
        static_cast<int>((value>>12)&31),static_cast<int>((value>>6)&63),static_cast<int>(value&63));
}
std::uint32_t pack_world_time(std::chrono::sys_seconds time) {
    const auto day=std::chrono::floor<std::chrono::days>(time);
    const std::chrono::year_month_day date{day};
    const auto year=static_cast<int>(date.year());
    if(year<2000 || year>2063) throw std::invalid_argument("World calendar year does not fit original format");
    const std::chrono::hh_mm_ss clock{time-day};
    return (static_cast<std::uint32_t>(year-2000)<<26) | (static_cast<unsigned int>(date.month())<<22) |
        (static_cast<unsigned int>(date.day())<<17) | (static_cast<std::uint32_t>(clock.hours().count())<<12) |
        (static_cast<std::uint32_t>(clock.minutes().count())<<6) | static_cast<std::uint32_t>(clock.seconds().count());
}

CharacterRoster Characters::load(std::uint32_t user_id) {
    if(user_id==0 || user_id>static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()))
        throw std::invalid_argument("Invalid character account identifier");
    const std::array<storage::Parameter,2> parameters={std::int64_t{server_id_},std::int64_t{user_id}};
    CharacterRoster roster;
    const auto country=result(database_.query("EXEC PS_GameData.dbo.usp_Read_User_MaxGrow_R ?,?;",parameters),2);
    if(!country.rows.empty()) {
        roster.country=byte(country.rows[0],0);
        roster.max_grow=std::max<std::uint8_t>(2,byte(country.rows[0],1));
        roster.new_account=false;
    }
    const auto characters=result(database_.query("EXEC PS_GameData.dbo.usp_Read_Chars_R ?,?;",parameters),29);
    for(const auto& row:characters.rows) {
        CharacterSummary value;
        const auto id=integer(row,0);
        if(id<=0 || id>std::numeric_limits<std::int32_t>::max() || !row[1]) throw std::runtime_error("Invalid character identity");
        value.id=static_cast<std::uint32_t>(id);value.slot=byte(row,2);
        if(value.slot>=roster.slots.size()) throw std::runtime_error("Character outside original five slots");
        value.name=row[1]->substr(0,20);
        value.name.erase(std::remove(value.name.begin(),value.name.end(),' '),value.name.end());
        value.family=byte(row,3);value.grow=byte(row,4);value.hair=byte(row,5);value.face=byte(row,6);
        value.size=byte(row,7);value.job=byte(row,8);value.sex=byte(row,9);value.level=word(row,10);
        value.strength=word(row,11);value.dexterity=word(row,12);value.recovery=word(row,13);
        value.intelligence=word(row,14);value.luck=word(row,15);value.wisdom=word(row,16);
        value.hp=word(row,17);value.mp=word(row,18);value.sp=word(row,19);value.map=word(row,20);
        value.rename=byte(row,21)!=0;value.remaining_time=integer(row,22)!=0;
        const auto created=calendar(static_cast<int>(integer(row,23)),static_cast<unsigned int>(integer(row,24)),
            static_cast<unsigned int>(integer(row,25)),static_cast<int>(integer(row,26)),
            static_cast<int>(integer(row,27)),static_cast<int>(integer(row,28)));
        value.created=static_cast<std::uint32_t>((created-world_base_).count());
        // Original load keeps the last row for a slot if the database contains duplicates.
        roster.slots[value.slot]=std::move(value);
    }
    for(auto& slot:roster.slots) {
        if(!slot) continue;
        const std::array<storage::Parameter,2> items={std::int64_t{slot->id},std::int64_t{8}};
        const auto equipment=result(database_.query("EXEC PS_GameData.dbo.usp_Read_Char_Items_Simple_R ?,?,0;",items),9);
        for(const auto& row:equipment.rows) {
            const auto index=byte(row,0);
            if(index>=8) continue;
            slot->equipment_type[index]=byte(row,1);slot->equipment_id[index]=byte(row,2);
            if(index==7) for(std::size_t gem=0;gem<6;++gem) slot->cloak[gem]=byte(row,gem+3);
        }
    }
    return roster;
}

std::vector<net::Packet> character_list(const CharacterRoster& roster,bool include_country) {
    // CUser::PacketUserDBChar, ps_game.exe 0046cf60, case 403.
    std::vector<net::Packet> packets;
    if(include_country) packets.push_back({9,1,roster.country,roster.max_grow});
    for(std::size_t index=0;index<roster.slots.size();++index)
        if(!roster.slots[index]) packets.push_back({1,1,static_cast<std::uint8_t>(index),0,0,0,0});
    for(const auto& slot:roster.slots) {
        if(!slot) continue;
        const auto& c=*slot;net::Packet packet(83,0);packet[0]=1;packet[1]=1;packet[2]=c.slot;
        put(packet,3,c.id,4);put(packet,7,c.created,4);put(packet,11,c.level,2);
        packet[13]=c.family;packet[14]=c.grow;packet[15]=c.hair;packet[16]=c.face;
        packet[17]=c.size;packet[18]=c.job;packet[19]=c.sex;put(packet,20,c.map,2);
        const std::array stats={c.strength,c.dexterity,c.recovery,c.intelligence,c.wisdom,c.luck,c.hp,c.mp,c.sp};
        for(std::size_t i=0;i<stats.size();++i) put(packet,22+i*2,stats[i],2);
        std::copy(c.equipment_type.begin(),c.equipment_type.end(),packet.begin()+40);
        std::copy(c.equipment_id.begin(),c.equipment_id.end(),packet.begin()+48);
        std::copy_n(c.name.begin(),std::min<std::size_t>(c.name.size(),19),packet.begin()+56);
        // Assembly writes remaining-time and rename flags over the final two bytes of szName[21].
        packet[75]=c.remaining_time;packet[76]=c.rename;
        std::copy(c.cloak.begin(),c.cloak.end(),packet.begin()+77);packets.push_back(std::move(packet));
    }
    return packets;
}
}
