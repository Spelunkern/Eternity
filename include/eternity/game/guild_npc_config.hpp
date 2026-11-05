#pragma once
#include "eternity/game/guild_client_npc.hpp"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>

namespace eternity::game {
namespace guild_npc_config_detail {
inline std::string_view trim(std::string_view text) {
    constexpr std::string_view space=" \t\r\n";
    const auto first=text.find_first_not_of(space);
    return first==text.npos ? std::string_view{} : text.substr(first,text.find_last_not_of(space)-first+1);
}
inline std::string lower(std::string_view text) {
    std::string out(text);
    for(auto& c:out) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');
    return out;
}
inline std::uint32_t decimal(std::string_view value) {
    value=trim(value);bool negative=false;
    if(!value.empty() && (value.front()=='-' || value.front()=='+')) {negative=value.front()=='-';value.remove_prefix(1);}
    std::uint32_t result=0;
    for(char c:value) {if(c<'0' || c>'9')break;result=result*10u+static_cast<unsigned>(c-'0');}
    return negative ? 0u-result : result;
}
using Profile=std::map<std::pair<std::string,std::string>,std::string>;
inline Profile profile(std::string_view text) {
    Profile values;std::string section;
    while(!text.empty()) {
        auto end=text.find('\n');auto line=trim(text.substr(0,end));
        if(end==text.npos)text={};else text.remove_prefix(end+1);
        if(line.empty() || line.front()==';')continue;
        if(line.front()=='[') {auto close=line.find(']');if(close!=line.npos)section=lower(trim(line.substr(1,close-1)));continue;}
        auto eq=line.find('=');if(section.empty() || eq==line.npos)continue;
        auto key=lower(trim(line.substr(0,eq)));auto value=trim(line.substr(eq+1));
        if(value.size()>=2 && (value.front()=='\'' || value.front()=='"') && value.back()==value.front()) value=value.substr(1,value.size()-2);
        values.try_emplace({section,key},value);
    }
    return values;
}
template<std::size_t N> inline void table(std::array<std::uint16_t,N>& row,std::string_view value,std::size_t count) {
    constexpr std::string_view delimiters=" ,=()[]{}\t\r\n";
    value=value.substr(0,254);
    for(std::size_t i=1;i<=count;++i) {
        auto first=value.find_first_not_of(delimiters);if(first==value.npos)break;
        value.remove_prefix(first);auto end=value.find_first_of(delimiters);
        row[i]=static_cast<std::uint16_t>(decimal(value.substr(0,end)));
        if(end==value.npos)break;value.remove_prefix(end);
    }
}
}
// ps_game.exe CGuildMgr::InitGuildHouseIniFile 004319a0 and ParseString1 00434e80.
// Apply to retained values: missing keys and missing tokens preserve existing cells.
inline void guild_npc_apply_profile(GuildNpcUpgradeConfig& config,std::string_view text) {
    using namespace guild_npc_config_detail;
    const auto values=profile(text);
    if(auto it=values.find({"defaultset","housekeepetin"});it!=values.end() && !it->second.empty())config.default_keep=decimal(it->second);
    const auto apply=[&](const char* section,const char* key,auto& row,std::size_t count) {
        if(auto it=values.find({section,key});it!=values.end() && !it->second.empty())table(row,it->second,count);
    };
    apply("gatekeeper","keepetin",config.upkeep[0],3);apply("gatekeeper","upgradeetin",config.costs[0],3);
    apply("shopweapon","upgradeetin",config.costs[1],3);apply("shopdefen","upgradeetin",config.costs[2],3);
    apply("shopposion","upgradeetin",config.costs[5],3);apply("shopposion","keepetin",config.upkeep[5],3);
    apply("bank","keepetin",config.upkeep[6],5);apply("bank","upgradeetin",config.costs[6],5);
    apply("general","upgradeetin",config.costs[3],3);apply("smith","keepetin",config.upkeep[4],5);
    apply("smith","upgradeetin",config.costs[4],5);
}
struct GuildHouseConfiguration {
    std::uint32_t purchase_price=50000000;
    GuildNpcUpgradeConfig npc=guild_npc_executable_defaults(2000);
};
// Returns whether the original loader emits its invalid/missing price diagnostic.
// Callers must forward that diagnostic through their real logging owner.
inline bool guild_house_apply_profile(GuildHouseConfiguration& config,std::string_view text) {
    const auto values=guild_npc_config_detail::profile(text);
    const auto found=values.find({"defaultset","housebuymoney"});
    const bool supplied=found!=values.end() && !found->second.empty();
    const auto value=supplied?guild_npc_config_detail::decimal(found->second):0u;
    const bool diagnostic=value==0 || (value&0x80000000u)!=0;
    if(supplied)config.purchase_price=value;
    guild_npc_apply_profile(config.npc,text);
    return diagnostic;
}
struct GuildHouseConfigurationLoad {GuildHouseConfiguration config;bool price_diagnostic;};
inline GuildHouseConfigurationLoad guild_house_load_profile(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot open guild house configuration: "+path.string());
    const std::string text{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    if(input.bad())throw std::runtime_error("Cannot read guild house configuration: "+path.string());
    GuildHouseConfigurationLoad result{};result.price_diagnostic=guild_house_apply_profile(result.config,text);return result;
}
inline GuildNpcUpgradeConfig guild_npc_load_profile(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    if(!input)throw std::runtime_error("Cannot open guild house configuration: "+path.string());
    const std::string text{std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
    if(input.bad())throw std::runtime_error("Cannot read guild house configuration: "+path.string());
    auto config=guild_npc_executable_defaults(2000);guild_npc_apply_profile(config,text);return config;
}
}
