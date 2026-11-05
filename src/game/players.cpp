#include "eternity/game/players.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <limits>

namespace eternity::game {
namespace {
std::int64_t integer(const storage::Row& row,std::size_t index,bool nullable=false) {
    if(index>=row.size()) throw std::runtime_error("Missing original player field");
    if(!row[index]) {if(nullable) return 0;throw std::runtime_error("Unexpected SQL NULL in player field");}
    const auto& text=*row[index];std::int64_t value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size()) throw std::runtime_error("Invalid original player integer");
    return value;
}
std::uint32_t dword(const storage::Row& row,std::size_t index,bool nullable=false) {
    const auto value=integer(row,index,nullable);
    if(value<std::numeric_limits<std::int32_t>::min() || value>std::numeric_limits<std::uint32_t>::max())
        throw std::runtime_error("Player DWORD outside original range");
    return static_cast<std::uint32_t>(value);
}
std::uint16_t word(const storage::Row& row,std::size_t index) {
    const auto value=integer(row,index);
    if(value<std::numeric_limits<std::int16_t>::min() || value>65535) throw std::runtime_error("Player word outside original range");
    return static_cast<std::uint16_t>(value);
}
std::uint8_t byte(const storage::Row& row,std::size_t index,bool nullable=false) {
    const auto value=integer(row,index,nullable);
    if(value<0 || value>255) throw std::runtime_error("Player byte outside original range");
    return static_cast<std::uint8_t>(value);
}
float real(const storage::Row& row,std::size_t index) {
    if(index>=row.size() || !row[index]) throw std::runtime_error("Missing player coordinate");
    const auto& text=*row[index];float value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || !std::isfinite(value))
        throw std::runtime_error("Invalid player coordinate");
    return value;
}
std::string text(const storage::Row& row,std::size_t index,std::size_t maximum) {
    if(index>=row.size() || !row[index]) return {};
    auto value=row[index]->substr(0,maximum);value.erase(std::remove(value.begin(),value.end(),' '),value.end());return value;
}
std::chrono::sys_seconds calendar(const storage::Row& row,std::size_t offset) {
    const auto year=integer(row,offset),month=integer(row,offset+1),day=integer(row,offset+2);
    const auto hour=integer(row,offset+3),minute=integer(row,offset+4),second=integer(row,offset+5);
    if(year<1753 || year>9999 || month<1 || month>12 || day<1 || day>31 || hour<0 || hour>23 || minute<0 || minute>59 || second<0 || second>59)
        throw std::runtime_error("Invalid SQL player calendar");
    const std::chrono::year_month_day date{std::chrono::year{static_cast<int>(year)},
        std::chrono::month{static_cast<unsigned int>(month)},std::chrono::day{static_cast<unsigned int>(day)}};
    if(!date.ok()) throw std::runtime_error("Invalid SQL player date");
    return std::chrono::sys_days{date}+std::chrono::hours{hour}+std::chrono::minutes{minute}+std::chrono::seconds{second};
}
storage::ResultSet result(std::vector<storage::ResultSet> sets,std::size_t columns) {
    if(sets.size()!=1 || sets[0].columns.size()!=columns) throw std::runtime_error("Original player procedure shape changed");
    return std::move(sets[0]);
}
PlayerItem item(const storage::Row& row,bool bank) {
    if(row.size()!=(bank?21U:22U)) throw std::runtime_error("Original item detail row changed");
    PlayerItem value;value.id=dword(row,0);value.type=byte(row,1);value.type_id=byte(row,2);
    value.uid=std::bit_cast<std::uint64_t>(integer(row,3));
    if(!bank) value.bag=byte(row,4);
    const std::size_t start=bank?4:5;
    const auto slot=integer(row,start);
    if(slot<0 || slot>65535) throw std::runtime_error("Invalid persisted item slot");
    value.slot=static_cast<std::uint16_t>(slot);value.quality=word(row,start+1);
    for(std::size_t index=0;index<6;++index) value.gems[index]=byte(row,start+2+index);
    value.craft=text(row,start+8,20);value.count=byte(row,start+9);
    if(row[start+10]) value.make_type=row[start+10]->substr(0,1);
    value.made_at=pack_world_time(calendar(row,start+11));return value;
}
}

PlayerItem decode_player_item(const storage::Row& row,bool bank) {return item(row,bank);}

PlayerCore decode_player_core(const storage::Row& row) {
    // DBCharacter::LoadCharacter, ps_dbAgent.exe 00420610, 53 ordered result columns.
    if(row.size()!=53) throw std::runtime_error("Original character detail row changed");
    PlayerCore player;auto& c=player.character;
    player.user_id=dword(row,0);c.id=dword(row,1);c.name=text(row,2,20);c.slot=byte(row,3);
    c.family=byte(row,4);c.grow=byte(row,5);c.hair=byte(row,6);c.face=byte(row,7);c.size=byte(row,8);
    c.job=byte(row,9);c.sex=byte(row,10);c.level=word(row,11);player.stat_points=word(row,12);player.skill_points=word(row,13);
    c.strength=word(row,14);c.dexterity=word(row,15);c.recovery=word(row,16);c.intelligence=word(row,17);c.luck=word(row,18);c.wisdom=word(row,19);
    c.hp=word(row,20);c.mp=word(row,21);c.sp=word(row,22);c.map=word(row,23);player.direction=word(row,24);
    player.experience=dword(row,25);player.money=dword(row,26);player.x=real(row,27);player.y=real(row,28);player.z=real(row,29);
    player.hunger=word(row,30);player.vigor=std::bit_cast<std::int16_t>(word(row,31));player.cg=byte(row,32);player.og=byte(row,33);player.ig=byte(row,34);
    if(byte(row,35)!=0) throw std::runtime_error("Deleted character returned by detail procedure");
    for(std::size_t index=0;index<4;++index) {
        const auto count=dword(row,36+index);
        player.kills[index]=(count&0x80000000U)!=0 ? count&0xffffU : count;
    }
    player.stored_money=std::bit_cast<std::uint64_t>(integer(row,40,true));
    player.guild_id=dword(row,41,true);player.guild_rank=byte(row,42,true);player.kill_level=byte(row,43);player.dead_level=byte(row,44);
    player.reset_stats=byte(row,45,true)!=0;player.reset_skills=byte(row,46,true)!=0;
    bool date_present=false;for(std::size_t index=47;index<53;++index) date_present=date_present || row[index].has_value();
    if(date_present) player.last_left=calendar(row,47);
    return player;
}

void decode_player_inventory(PlayerSnapshot& player) {
    player.items.clear();player.bank.clear();player.skills.clear();player.overflow_items.clear();player.overflow_bank.clear();
    const auto decode=[&](std::string_view catalog,bool bank) {
        std::array<std::optional<PlayerItem>,240> slots;std::vector<PlayerItem> duplicates;
        std::size_t source_order=0;
        for(const auto& row:player.catalogs.at(std::string(catalog)).rows) {
            auto value=item(row,bank);value.source_order=source_order++;
            if((bank && value.slot>=240) || (!bank && (value.bag>=6 || value.slot>=24))) continue;
            const auto index=bank ? value.slot : value.bag*24+value.slot;
            if(slots[static_cast<std::size_t>(index)]) duplicates.push_back(std::move(value));
            else slots[static_cast<std::size_t>(index)]=std::move(value);
        }
        for(auto& value:duplicates) {
            auto free=std::find_if(slots.begin()+(bank?0:24),slots.begin()+(bank?6:144),[](const auto& slot){return !slot;});
            if(free==slots.begin()+(bank?6:144)) {(bank?player.overflow_bank:player.overflow_items).push_back(std::move(value));continue;}
            const auto index=static_cast<std::size_t>(free-slots.begin());value.relocated=true;
            value.bag=bank?0:static_cast<std::uint8_t>(index/24);value.slot=static_cast<std::uint16_t>(bank?index:index%24);*free=std::move(value);
        }
        auto& destination=bank?player.bank:player.items;
        for(auto& slot:slots) if(slot) destination.push_back(std::move(*slot));
    };
    decode("Items",false);decode("Bank",true);
    const auto& rows=player.catalogs.at("Skills").rows;
    if(rows.size()>255) throw std::runtime_error("Player exceeds original 255-skill load limit");
    for(const auto& row:rows) player.skills.push_back({word(row,0),word(row,3),byte(row,1),
        static_cast<std::uint8_t>(player.skills.size()),byte(row,2)});
}

Players::Players(storage::SqlServer& database,std::uint8_t server_id,std::string catalog)
    : database_(database),server_id_(server_id) {
    if(catalog.empty() || catalog.size()>128 || std::any_of(catalog.begin(),catalog.end(),[](unsigned char c) {
        return !((c>='A'&&c<='Z') || (c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='_');
    })) throw std::invalid_argument("Invalid player SQL catalog identifier");
    prefix_="["+catalog+"].dbo.";
}
std::vector<PlayerPointItem> decode_point_items(const storage::ResultSet& data) {
    std::vector<PlayerPointItem> result;
    for(const auto& row:data.rows) {
        if(row.size()!=3) throw std::runtime_error("Original point item row changed");
        result.push_back({byte(row,0),dword(row,1),byte(row,2)});
    }
    return result;
}
void decode_player_auxiliary(PlayerSnapshot& player) {
    // ps_dbAgent DBCharacter::LoadCharacter 00420610: WORD IDs/delays, BYTE counters, ordered quick slots.
    player.effects.clear();player.quests.clear();player.finished_quests.clear();player.quick_slots.clear();player.save_points.clear();
    for(const auto& row:player.catalogs.at("AppliedSkills").rows) {
        if(row.size()!=3) throw std::runtime_error("Original applied skill row changed");
        player.effects.push_back({word(row,0),byte(row,1),dword(row,2)});
    }
    for(const auto& row:player.catalogs.at("Quests").rows) {
        if(row.size()!=5) throw std::runtime_error("Original quest row changed");
        player.quests.push_back({word(row,0),word(row,1),{byte(row,2),byte(row,3),byte(row,4)}});
    }
    for(const auto& row:player.catalogs.at("FinishedQuests").rows) {
        if(row.size()!=2) throw std::runtime_error("Original finished quest row changed");
        player.finished_quests.push_back({word(row,0),byte(row,1)});
    }
    const auto& quick=player.catalogs.at("QuickSlots").rows;
    if(quick.size()>255) throw std::runtime_error("Player exceeds original quick-slot wire count");
    for(const auto& row:quick) {
        if(row.size()!=4) throw std::runtime_error("Original quick slot row changed");
        player.quick_slots.push_back({byte(row,0),byte(row,1),byte(row,2),word(row,3)});
    }
    for(const auto& row:player.catalogs.at("SavePoints").rows) {
        if(row.size()!=5) throw std::runtime_error("Original save point row changed");
        const auto slot=byte(row,0);if(slot>=4) continue;
        player.save_points.push_back({slot,word(row,1),real(row,2),real(row,3),real(row,4)});
    }
    player.point_items=decode_point_items(player.catalogs.at("PointItems"));
}
std::optional<PlayerSnapshot> Players::load(std::uint32_t uid,std::uint32_t cid,const std::string& username) {
    if(uid==0 || uid>2147483647 || cid==0 || cid>2147483647 || username.empty() || username.size()>12)
        throw std::invalid_argument("Invalid player load identity");
    const std::array<storage::Parameter,3> identity={std::int64_t{server_id_},std::int64_t{uid},std::int64_t{cid}};
    const auto owned=result(database_.query("SET NOCOUNT ON; SELECT CharID FROM "+prefix_+
        "Chars WHERE ServerID=? AND UserUID=? AND CharID=? AND Slot<5 AND Del=0 AND RenameCnt=0 AND RemainTime=0;",identity),1);
    if(owned.rows.empty()) return std::nullopt;
    const std::array<storage::Parameter,1> character={std::int64_t{cid}},user={std::int64_t{uid}};
    PlayerSnapshot snapshot;
    snapshot.catalogs["Core"]=result(database_.query("EXEC "+prefix_+"usp_Read_Chars_Detail2_R ?;",character),53);
    const auto& core=snapshot.catalogs.at("Core");
    if(core.rows.size()!=1) throw std::runtime_error("Original player detail procedure did not return one character");
    snapshot.core=decode_player_core(core.rows[0]);
    if(snapshot.core.user_id!=uid || snapshot.core.character.id!=cid) throw std::runtime_error("Original player detail ownership changed");
    struct Loader {const char* name;const char* procedure;std::size_t columns;bool user;};
    snapshot.catalogs["Items"]=result(database_.query("EXEC "+prefix_+"usp_Read_Char_Items_Detail_R ?;",character),22);
    const std::array<storage::Parameter,2> bank={std::int64_t{server_id_},std::int64_t{uid}};
    snapshot.catalogs["Bank"]=result(database_.query("EXEC "+prefix_+"usp_Read_User_StoredItems_Detail_R ?,?;",bank),21);
    snapshot.catalogs["Skills"]=result(database_.query("EXEC "+prefix_+"usp_Read_Char_Skills_R ?;",character),4);
    decode_player_inventory(snapshot);
    constexpr std::array<Loader,7> loaders={Loader{"AppliedSkills","usp_Read_Char_ApplySkills_R",3,false},
        {"Quests","usp_Read_Char_Quest_R",5,false},{"FinishedQuests","usp_Read_Char_FinishedQuest_R",2,false},
        {"QuickSlots","usp_Read_Char_QuickSlots_R",4,false},{"SavePoints","usp_Read_Char_SavePoint",5,false},
        {"PointLog","usp_Read_Chars_PointLog_R",3,false},{"PointItems","usp_Read_User_StoredPointItems",3,true}};
    for(const auto& loader:loaders) snapshot.catalogs[loader.name]=result(database_.query(
        "EXEC "+prefix_+loader.procedure+" ?;",loader.user?user:character),loader.columns);
    decode_player_auxiliary(snapshot);
    const std::array<storage::Parameter,2> cash={std::int64_t{uid},username};
    snapshot.catalogs["Cash"]=result(database_.query("DECLARE @Code int,@Cash int=0; EXEC @Code="+prefix_+
        "usp_Read_User_CashPoint_NCash @Cash OUTPUT,?,?; SELECT @Code,@Cash;",cash),2);
    const auto& cash_rows=snapshot.catalogs.at("Cash").rows;
    if(cash_rows.size()!=1) throw std::runtime_error("Missing original cash balance output");
    snapshot.cash_points=dword(cash_rows[0],1);
    snapshot.catalogs["GuildRejoin"]=result(database_.query("DECLARE @Hours int; EXEC @Hours="+prefix_+
        "usp_Read_Char_GuildJoinDate_R ?; SELECT @Hours;",character),1);
    const auto& hours=snapshot.catalogs.at("GuildRejoin").rows;
    if(hours.size()!=1) throw std::runtime_error("Missing original guild rejoin output");
    snapshot.guild_rejoin_hours=static_cast<std::int32_t>(integer(hours[0],0));
    snapshot.catalogs["ConcernMarket"]=result(database_.query("EXEC "+prefix_+"usp_Read_Char_ConcernMarket_R ?;",character),1);
    for(const auto& row:snapshot.catalogs.at("ConcernMarket").rows) {
        if(row.size()!=1) throw std::runtime_error("Original market concern row changed");
        snapshot.concern_market.push_back(dword(row,0));
    }
    // CUser::LoadCharacter 00404b00 ignores separate friend/block/gift loader failures.
    for(const auto social:{Loader{"Friends","usp_Read_FriendChar_R",7,false},Loader{"Blocked","usp_Read_BanChar_R",3,false},
                          Loader{"GiftNotifications","usp_Read_User_GiftPointItemNotify",3,true}}) {
        try {snapshot.catalogs[social.name]=result(database_.query("EXEC "+prefix_+social.procedure+" ?;",social.user?user:character),social.columns);}
        catch(const std::exception& error) {snapshot.warnings.push_back(std::string(social.name)+": "+error.what());snapshot.catalogs[social.name]={};}
    }
    return snapshot;
}
}
