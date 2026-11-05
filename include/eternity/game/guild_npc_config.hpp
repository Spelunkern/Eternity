#pragma once
#include "eternity/game/guild_client_npc.hpp"
#include <cstdlib>
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
// g_GuildNpcIdShopOnLevel 00541cb0: [country][gate, weapon, defense, general][level][column].
using GuildNpcShopIds=std::array<std::array<std::array<std::array<std::uint16_t,3>,4>,4>,2>;
struct GuildHouseShopConfig {
    std::array<double,4> buy_rate{0.99,0.98,0.95,0.9}; // g_GuildNpcShopDecreRate 00541c18.
    std::array<double,4> potion_buy_rate{0.9,0.8,0.7,0.6}; // g_GuildNpcPotionShopDecreRate 00541c38.
    GuildNpcShopIds npc_ids=[] {
        constexpr std::array<std::array<std::array<std::uint16_t,3>,4>,2> first{{
            {{{37,38,39},{196,197,198},{200,201,202},{200,201,202}}},{{{41,42,43},{207,208,209},{211,212,213},{200,201,202}}}}};
        GuildNpcShopIds ids{};
        for(std::size_t country=0;country<2;++country) for(std::size_t type=0;type<4;++type) for(std::size_t level=1;level<=3;++level)
            ids[country][type][level][0]=first[country][type][level-1];
        return ids;
    }();
};
// ParseString2<double[4]> 00434fc0: missing tokens keep their retained value.
inline void guild_house_shop_rates(std::array<double,4>& row,std::string_view value) {
    constexpr std::string_view delimiters=" ,=()[]{}\t\r\n";
    value=value.substr(0,254);
    for(auto& cell:row) {
        auto first=value.find_first_not_of(delimiters);if(first==value.npos)break;
        value.remove_prefix(first);const auto end=value.find_first_of(delimiters);
        cell=std::strtod(std::string(value.substr(0,end)).c_str(),nullptr);
        if(end==value.npos)break;value.remove_prefix(end);
    }
}
// InitGuildHouseIniFile 00431b1a, 00431b5a and the NpcLevel loop 00431eac..00431fd2.
inline void guild_house_apply_shop_profile(GuildHouseShopConfig& config,std::string_view text) {
    using namespace guild_npc_config_detail;
    const auto values=profile(text);
    if(auto it=values.find({"shopdefault","buydecrerate"});it!=values.end() && !it->second.empty())guild_house_shop_rates(config.buy_rate,it->second);
    if(auto it=values.find({"shopdefault","buypotiondecrerate"});it!=values.end() && !it->second.empty())guild_house_shop_rates(config.potion_buy_rate,it->second);
    constexpr std::array<const char*,4> keys{"gatekeeper","shopweapon","shopdefen","shopgeneral"};
    constexpr std::array<const char*,2> sections{"npclevel_light","npclevel_dark"};
    for(std::size_t country=0;country<2;++country) for(std::size_t type=0;type<4;++type) for(std::size_t level=1;level<=3;++level) {
        const auto found=values.find({sections[country],keys[type]+std::to_string(level)});
        if(found!=values.end())config.npc_ids[country][type][level][0]=static_cast<std::uint16_t>(decimal(found->second));
    }
}
struct GuildHouseConfiguration {
    std::uint32_t purchase_price=50000000;
    GuildNpcUpgradeConfig npc=guild_npc_executable_defaults(2000);
    GuildHouseShopConfig shop;
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
    guild_npc_apply_profile(config.npc,text);guild_house_apply_shop_profile(config.shop,text);
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
