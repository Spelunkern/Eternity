#include "eternity/game/map_config.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <fstream>
#include <map>
#include <stdexcept>

namespace eternity::game {
namespace {
std::string lower(std::string_view text) {std::string result(text);for(auto& c:result) if(c>='A' && c<='Z') c=static_cast<char>(c+32);return result;}
std::string_view trim(std::string_view text) {
    const auto first=text.find_first_not_of(" \t\r\n");if(first==std::string_view::npos) return {};
    return text.substr(first,text.find_last_not_of(" \t\r\n")-first+1);
}
class Profile {
public:
    explicit Profile(std::string_view text) {
        std::string section;bool have_section=false;
        while(!text.empty()) {
            const auto newline=text.find('\n');auto line=trim(text.substr(0,newline));
            text=newline==std::string_view::npos?std::string_view{}:text.substr(newline+1);
            if(line.empty() || line.front()==';') continue;
            if(line.front()=='[') {
                const auto end=line.find(']');if(end==std::string_view::npos) continue;
                section=lower(trim(line.substr(1,end-1)));have_section=true;continue;
            }
            const auto equal=line.find('=');if(!have_section || equal==std::string_view::npos) continue;
            auto value=trim(line.substr(equal+1));
            if(value.size()>=2 && value.front()==value.back() && (value.front()=='\'' || value.front()=='"')) value=value.substr(1,value.size()-2);
            values_.try_emplace({section,lower(trim(line.substr(0,equal)))},value);
        }
    }
    std::string get(const std::string& section,std::string_view key) const {
        const auto found=values_.find({section,std::string(key)});
        // GetPrivateProfileStringA receives nSize=255 in both original callers.
        return found==values_.end()?std::string{}:found->second.substr(0,254);
    }
private:
    std::map<std::pair<std::string,std::string>,std::string> values_;
};
std::int32_t integer(std::string_view text) {
    // Original MSVCRT atol/atoi 0050451e/005045a6 accumulates a wrapping DWORD.
    while(!text.empty() && (text.front()==' ' || text.front()=='\t' || text.front()=='\r' || text.front()=='\n' || text.front()=='\v' || text.front()=='\f')) text.remove_prefix(1);
    bool negative=false;if(!text.empty() && (text.front()=='-' || text.front()=='+')) {negative=text.front()=='-';text.remove_prefix(1);}
    std::uint32_t value=0;for(const auto c:text) {if(c<'0' || c>'9') break;value=value*10+static_cast<std::uint32_t>(c-'0');}
    return std::bit_cast<std::int32_t>(negative?0U-value:value);
}
std::vector<std::string> tokens(std::string_view text) {
    constexpr std::string_view delimiters=" ,=()[]{}\t\r\n";std::vector<std::string> result;
    while(!text.empty()) {
        const auto first=text.find_first_not_of(delimiters);if(first==std::string_view::npos) break;text.remove_prefix(first);
        const auto end=text.find_first_of(delimiters);result.emplace_back(text.substr(0,end));if(end==std::string_view::npos) break;text.remove_prefix(end);
    }
    return result;
}
float real(const std::string& token) {return static_cast<float>(std::strtod(token.c_str(),nullptr));}
MapLocation location(const std::vector<std::string>& parts,std::size_t first=0) {
    MapLocation result;
    if(parts.size()>first) result.map=integer(parts[first]);
    if(parts.size()>first+1) result.position.x=real(parts[first+1]);
    if(parts.size()>first+2) result.position.y=real(parts[first+2]);
    if(parts.size()>first+3) result.position.z=real(parts[first+3]);
    return result;
}
std::string value_or(std::string value,std::string_view fallback) {return value.empty()?std::string(fallback):value;}
std::string inherited(const Profile& profile,const std::string& section,std::string_view key,std::string_view fallback) {
    auto value=profile.get(section,key);if(value.empty()) value=profile.get("default_set_zone",key);return value_or(std::move(value),fallback);
}
MapSettings settings(const Profile& profile,std::int32_t id) {
    // CMap::LoadConfig 00426940. Only MapType, weather and rebirth inherit defaults.
    MapSettings result;result.id=id;const auto section="set_zone_"+std::to_string(id);
    result.dungeon=lower(inherited(profile,section,"maptype","F"))=="d";
    result.filename=value_or(profile.get(section,"mapname"),std::to_string(id)+".svmap");
    result.war_type=lower(profile.get(section,"wartype"))=="p"?1:0;
    const auto type=lower(profile.get(section,"createtype"));
    const std::array<std::string_view,6> types={"s","f","p","g","r","h"};const auto found=std::find(types.begin(),types.end(),type);
    result.create_type=found==types.end()?0:static_cast<std::int32_t>(found-types.begin()+1);
    auto rebirth=profile.get(section,"rebirthmappos");
    if(rebirth.size()>=6) result.rebirth.fill(location(tokens(rebirth)));
    else for(std::size_t i=0;i<2;++i) {
        auto value=profile.get(section,i==0?"rebirthmappos1":"rebirthmappos2");
        if(value.empty()) value=profile.get("default_set_zone","rebirthmappos");
        result.rebirth[i]=location(tokens(value_or(std::move(value),"-1, 0, 0, 0")));
    }
    result.weather.state=integer(inherited(profile,section,"weatherstate","0"));
    if(result.weather.state<0 || result.weather.state>2) result.weather.state=0;
    result.weather.power=integer(inherited(profile,section,"weatherpower","2"));result.weather.rate=integer(inherited(profile,section,"weatherrate","30"));
    result.weather.delay_ms=static_cast<std::uint32_t>(integer(inherited(profile,section,"weatherdelay","3")))*60000U;
    result.weather.none_delay_ms=static_cast<std::uint32_t>(integer(inherited(profile,section,"weathernonedelay","30")))*60000U;
    result.expire_ms=static_cast<std::uint32_t>(integer(profile.get(section,"expiretime")))*60000U+30000U;
    const auto times=tokens(profile.get(section,"createtime"));
    for(std::size_t i=0;i<std::min<std::size_t>(times.size(),21);++i) {
        const auto value=static_cast<std::uint16_t>(integer(times[i]));auto& time=result.open_times[i/3];
        if(i%3==0) time.day=value;else if(i%3==1) time.hour=value;else time.keep_hours=value;
    }
    result.create_minimum_users=integer(profile.get(section,"createminuser"));result.enter_maximum_users=integer(value_or(profile.get(section,"entermaxuser"),"30"));result.market=id==42;
    return result;
}
}
MapConfiguration MapConfiguration::decode(std::string_view text) {
    // LoadGameMapConfig 004270a0, ParseMapPosArray 00426660. Only complete tuples count.
    const Profile profile(text);MapConfiguration result;const auto count=integer(profile.get("base","mapcount"));
    if(count<0 || count>65536) throw std::runtime_error("Configured map count outside original WORD map-ID space");
    const std::array<std::string_view,4> keys={"hugamestartmappos","elgamestartmappos","bagamestartmappos","degamestartmappos"};
    const std::array<std::string_view,4> defaults={"1,813,39,1710","1,1489,30,1720","2,1726,262,334","2,419,230,723"};
    for(std::size_t family=0;family<4;++family) {
        const auto parts=tokens(value_or(profile.get("base",keys[family]),defaults[family]));
        for(std::size_t at=0;at+3<parts.size() && result.starts[family].size()<64;at+=4) result.starts[family].push_back(location(parts,at));
    }
    result.maps.reserve(static_cast<std::size_t>(count));for(std::int32_t id=0;id<count;++id) result.maps.push_back(settings(profile,id));return result;
}
MapConfiguration MapConfiguration::load(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);if(!input) throw std::runtime_error("Cannot open original map configuration");
    const std::string text((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());return decode(text);
}
ServerMaps ServerMaps::load(const MapConfiguration& configuration,const std::filesystem::path& directory) {
    ServerMaps result;result.maps_.reserve(configuration.maps.size());
    for(const auto& settings:configuration.maps) result.maps_.push_back({settings,MapDefinition::load(directory/settings.filename)});
    return result;
}
const ConfiguredMap* ServerMaps::find(std::uint16_t id) const noexcept {return id<maps_.size()?&maps_[id]:nullptr;}
std::int32_t* ServerMaps::mutable_portal_country(std::uint16_t id,std::size_t slot) noexcept {
    if(id>=maps_.size()||slot>=maps_[id].definition.portals.size()) return nullptr;
    return &maps_[id].definition.portals[slot].country;
}

}
