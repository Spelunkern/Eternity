#include "eternity/game/creation.hpp"
#include "eternity/game/map_config.hpp"
#include <algorithm>
#include <charconv>
#include <map>

namespace eternity::game {
namespace {
bool allowed_job(std::uint8_t family,std::uint8_t job) {
    constexpr std::array<unsigned int,4> masks={0x23,0x1c,0x34,0x0b};
    return family<4 && job<6 && (masks[family]&(1U<<job))!=0;
}
unsigned int number(const storage::Row& row,std::size_t index,unsigned int maximum) {
    if(index>=row.size() || !row[index]) throw std::runtime_error("Missing creation definition field");
    const auto& text=*row[index];unsigned int value=0;
    const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
    if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value>maximum)
        throw std::runtime_error("Invalid creation definition field");
    return value;
}
std::size_t column(const storage::ResultSet& table,std::string_view name) {
    const auto found=std::find(table.columns.begin(),table.columns.end(),name);
    if(found==table.columns.end()) throw std::runtime_error("Missing original creation definition column");
    return static_cast<std::size_t>(found-table.columns.begin());
}
}

CreateCheck check_create(const net::Packet& packet,const CharacterRoster& roster) {
    // CUser::PacketCharacter 0046bf20; TrimString 0046c250; ChkCharName 0046bf00, ps_game.exe.
    if(packet.size()<31 || packet[0]!=2 || packet[1]!=1) return {{},true,{}};
    if(roster.country==2) return {{},false,1};
    if(packet[2]>=5 || packet[4]>roster.max_grow || packet[3]>=4 || packet[8]>=6) return {{},true,{}};
    std::string name;
    for(std::size_t index=10;index<30 && packet[index]!=0;++index)
        if(packet[index]!=' ') name.push_back(static_cast<char>(packet[index]));
    if(name.size()<2) return {{},false,1};
    if(roster.slots[packet[2]] || !allowed_job(packet[3],packet[8])) return {{},true,{}};
    return {CreateRequest{packet[2],packet[3],packet[4],packet[5],packet[6],packet[7],packet[8],packet[9],std::move(name)},false,{}};
}

StartPositions original_start_positions() {
    // CMap::LoadGameMapConfig, ps_game.exe 004270a0: fallback values when keys are absent.
    return {{{{1,813.0F,39.0F,1710.0F}},{{1,1489.0F,30.0F,1720.0F}},
             {{2,1726.0F,262.0F,334.0F}},{{2,419.0F,230.0F,723.0F}}}};
}
StartPositions read_start_positions(const std::filesystem::path& path) {
    const auto configuration=MapConfiguration::load(path);StartPositions positions;
    for(std::size_t family=0;family<4;++family) for(const auto& start:configuration.starts[family])
        positions[family].push_back({static_cast<std::uint16_t>(start.map),start.position.x,start.position.y,start.position.z});
    return positions;
}

CreationRules CreationRules::load(const Definitions& definitions,StartPositions positions) {
    CreationRules rules;rules.starts_=std::move(positions);
    for(const auto& starts:rules.starts_) if(starts.empty() || starts.size()>64) throw std::invalid_argument("Missing original racial start positions");
    const auto& statuses=definitions.catalogs.at("StatusDefs");
    std::array<bool,6> found{};
    for(const auto& row:statuses.rows) {
        if(number(row,column(statuses,"Level"),65535)!=1) continue;
        const auto job=number(row,column(statuses,"Job"),5);
        rules.resources_[job]={static_cast<std::uint16_t>(number(row,column(statuses,"HP"),65535)),
            static_cast<std::uint16_t>(number(row,column(statuses,"SP"),65535)),
            static_cast<std::uint16_t>(number(row,column(statuses,"MP"),65535))};found[job]=true;
    }
    if(std::find(found.begin(),found.end(),false)!=found.end()) throw std::runtime_error("Missing level-one original status definitions");
    const auto& items=definitions.catalogs.at("Items");std::map<std::pair<unsigned int,unsigned int>,std::uint16_t> quality;
    for(const auto& row:items.rows)
        quality[{number(row,column(items,"Type"),255),number(row,column(items,"TypeID"),255)}]=
            static_cast<std::uint16_t>(number(row,column(items,"Quality"),65535));
    const auto& base=definitions.catalogs.at("BaseItemDefs");
    for(const auto& row:base.rows) {
        const auto family=number(row,column(base,"Family"),65535),job=number(row,column(base,"Job"),65535);
        if(family>=4 || job>=6) continue;
        const auto type=number(row,column(base,"Type"),255),id=number(row,column(base,"TypeID"),255);
        const auto item=quality.find({type,id});if(item==quality.end()) continue;
        auto& list=rules.items_[family][job];if(list.size()>=10) throw std::runtime_error("Original base item list exceeds ten entries");
        list.push_back({number(row,column(base,"ItemID"),2147483647),static_cast<std::uint8_t>(type),static_cast<std::uint8_t>(id),
            static_cast<std::uint8_t>(number(row,column(base,"Count"),255)),0,0,item->second});
    }
    // CGameData::LoadBaseItem 004bf800 ignores configured Slot/Quality. First type 1..19 equips in slot 5.
    for(auto& family:rules.items_) for(auto& list:family) {
        bool weapon=false;std::uint8_t inventory=0;
        for(auto& item:list) {
            if(item.type==0) continue;
            if(!weapon && item.type<=19) {item.bag=0;item.slot=5;weapon=true;}
            else {item.bag=1;item.slot=inventory++;}
        }
    }
    return rules;
}

std::size_t CreationRules::start_count(std::uint8_t family) const {
    if(family>=4) throw std::invalid_argument("Invalid character family");
    return starts_[family].size();
}
CharacterDraft CreationRules::prepare(const CreateRequest& request,std::size_t start_index) const {
    if(!allowed_job(request.family,request.job) || request.grow>3 || start_index>=start_count(request.family))
        throw std::invalid_argument("Invalid original character creation combination");
    // CUser::ProcessCharacterCreate, ps_game.exe 00480e90; verified field writes in assembly.
    struct Stats {std::uint16_t str,dex,rec,intelligence,wis,luck;};
    constexpr std::array<Stats,6> base={Stats{14,12,9,8,7,11},{10,9,12,10,10,10},
        {10,15,9,7,8,12},{11,10,10,7,10,13},{7,9,9,15,12,9},{8,9,10,12,14,8}};
    const auto stats=base[request.job];CharacterSummary c;
    c.slot=request.slot;c.level=1;c.name=request.name;c.family=request.family;c.grow=request.grow;
    c.hair=request.hair;c.face=request.face;c.size=request.size;c.job=request.job;c.sex=request.sex;
    c.strength=stats.str;c.dexterity=stats.dex;c.recovery=stats.rec;c.intelligence=stats.intelligence;c.wisdom=stats.wis;c.luck=stats.luck;
    const float multiplier=request.grow==2 ? 1.1F : request.grow==3 ? 1.5F : 1.0F;
    auto hp=static_cast<unsigned int>(static_cast<float>(resources_[request.job][0])*multiplier);
    auto sp=static_cast<unsigned int>(static_cast<float>(resources_[request.job][1])*multiplier);
    auto mp=static_cast<unsigned int>(static_cast<float>(resources_[request.job][2])*multiplier);
    if(request.family==0) {sp+=10;c.luck+=4;}
    if(request.family==1) {mp+=10;c.dexterity+=4;}
    if(request.family==2) {sp+=5;mp+=5;c.intelligence+=2;c.wisdom+=2;}
    if(request.family==3) {hp+=10;c.recovery+=2;c.strength+=2;}
    hp+=c.recovery*5U;sp+=c.dexterity*5U;mp+=c.wisdom*5U;
    c.hp=static_cast<std::uint16_t>(hp);c.sp=static_cast<std::uint16_t>(sp);c.mp=static_cast<std::uint16_t>(mp);
    c.map=starts_[request.family][start_index].map;
    return {request,std::move(c),starts_[request.family][start_index],items_[request.family][request.job]};
}
}
