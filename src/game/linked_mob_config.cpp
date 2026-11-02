#include "eternity/game/linked_mob_config.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t integer(std::string_view text) {
    while(!text.empty() && std::string_view(" \t\r\n\v\f").find(text.front())!=std::string_view::npos) text.remove_prefix(1);
    bool negative=false;if(!text.empty() && (text.front()=='+' || text.front()=='-')) {negative=text.front()=='-';text.remove_prefix(1);}
    std::uint32_t n=0;for(auto c:text) {if(c<'0' || c>'9') break;n=n*10+static_cast<std::uint32_t>(c-'0');}return std::bit_cast<std::int32_t>(negative?0U-n:n);
}
float real(std::string_view text) {
    // MSVCRT atof accepts decimal D/d exponents, but not hex, inf or nan.
    const auto first=text.find_first_not_of(" \t\r\n\v\f");if(first==std::string_view::npos) return 0;text.remove_prefix(first);
    std::size_t at=0,digits=0;if(at<text.size() && (text[at]=='+' || text[at]=='-')) ++at;
    const auto decimal=[&](std::size_t i){return i<text.size() && text[i]>='0' && text[i]<='9';};
    while(decimal(at)) {++at;++digits;}if(at<text.size() && text[at]=='.') {++at;while(decimal(at)) {++at;++digits;}}
    if(digits==0) return 0;
    std::string value(text.substr(0,at));
    if(at<text.size() && std::string_view("eEdD").find(text[at])!=std::string_view::npos) {auto end=at+1;if(end<text.size() && (text[end]=='+' || text[end]=='-')) ++end;const auto start=end;while(decimal(end)) ++end;if(end!=start) {value+='e';value+=text.substr(at+1,end-at-1);}}
    return static_cast<float>(std::strtod(value.c_str(),nullptr));
}
std::int32_t twice(float radius) {const auto value=std::trunc(static_cast<double>(radius)+radius);if(!std::isfinite(value) || value<INT32_MIN || value>INT32_MAX) return INT32_MIN;return static_cast<std::int32_t>(value);}
bool prefix(std::string_view line,std::string_view label) {if(line.size()<label.size()) return false;for(std::size_t i=0;i<label.size();++i) {auto c=line[i];if(c>='A' && c<='Z') c=static_cast<char>(c+32);if(c!=label[i]) return false;}return true;}
struct Tokens {
    std::string_view text;
    std::optional<std::string_view> next() {
        constexpr std::string_view delimiters=" ,=()[]{}\t\r\n";const auto first=text.find_first_not_of(delimiters);if(first==std::string_view::npos) {text={};return {};}
        text.remove_prefix(first);const auto end=text.find_first_of(delimiters);const auto value=text.substr(0,end);text=end==std::string_view::npos?std::string_view{}:text.substr(end+1);return value;
    }
    bool number(std::int32_t& value) {const auto token=next();if(!token) return false;value=integer(*token);return true;}
    bool number(std::uint32_t& value) {std::int32_t n=0;if(!number(n)) return false;value=static_cast<std::uint32_t>(n);return true;}
    bool number(float& value) {const auto token=next();if(!token) return false;value=real(*token);return true;}
    bool position(UserPosition& value) {return number(value.x) && number(value.y) && number(value.z);}
};
struct Bucket {std::int32_t map=0;std::size_t count=0;std::array<LinkedMobGroup,64> groups{};};
}
LinkedMobConfiguration LinkedMobConfiguration::decode(std::string_view input,std::span<const ConfiguredMap> maps,const MobRuntimeTemplates& definitions) {
    // Temporary fixed buckets preserve writes to an uncommitted group/spawn
    // when an incomplete record is followed by another attempt at that slot.
    std::vector<std::unique_ptr<Bucket>> buckets;std::unique_ptr<Bucket> pending=std::make_unique<Bucket>();LinkedMobGroup* active=nullptr;int mode=0;
    const auto line=[&](std::string_view text) {
        text=text.substr(0,text.find('\0'));if(text.size()<=1) return;const auto first=text.find_first_not_of(" \t");if(first==std::string_view::npos) return;text.remove_prefix(first);
        if(text.starts_with("//") || text.starts_with("--")) return;
        if(prefix(text,"[portal_")) {mode=3;return;}
        if(prefix(text,"[bothmob_")) {
            mode=4;active=nullptr;Tokens tokens{text.substr(9)};std::uint32_t id=0;std::int32_t map=0;if(!tokens.number(id) || !tokens.number(map)) return;
            Bucket* bucket=pending.get();for(auto& candidate:buckets) if(candidate->map==map) {bucket=candidate.get();break;}
            if(bucket->count>=64) throw std::runtime_error("Linked mob group capacity exceeded");
            auto& group=bucket->groups[bucket->count];group.id=id;
            if(!tokens.number(group.change_count)) return;
            group.change_count=std::min(group.change_count,15);
            if(!tokens.number(group.respawn_time) || !tokens.number(group.portal_map) || !tokens.number(group.portal_id) || !tokens.position(group.leader.center)) return;
            group.leader.randomized=0;std::int32_t copied=0;for(;copied<group.change_count;++copied) if(!tokens.number(group.leader.type_ids[static_cast<std::size_t>(copied)])) break;
            if(copied!=group.change_count) return;
            active=&group;++bucket->count;
            if(bucket==pending.get()) {if(buckets.size()>=64) throw std::runtime_error("Linked map capacity exceeded");bucket->map=map;buckets.push_back(std::move(pending));pending=std::make_unique<Bucket>();}
            return;
        }
        if(mode==4 && prefix(text,"[mob]")) {
            if(!active || active->spawn_count>=32) return;
            auto& spawn=active->spawns[static_cast<std::size_t>(active->spawn_count)];Tokens tokens{text.substr(5)};
            if(!tokens.position(spawn.center) || !tokens.number(spawn.radius)) return;
            for(std::int32_t i=0;i<active->change_count+1;++i) {const auto token=tokens.next();if(token) spawn.type_ids[static_cast<std::size_t>(i)]=integer(*token);}
            if(!tokens.number(spawn.count)) return;
            spawn.count=std::min(spawn.count,16);++active->spawn_count;return;
        }
        if(mode==4 && prefix(text,"[randomrespawn]")) {
            if(!active) return;
            active->leader.randomized=1;Tokens tokens{text.substr(15)};
            if(!tokens.number(active->leader.minimum_minutes) || !tokens.number(active->leader.random_minutes)) return;
            for(auto& center:active->leader.random_centers) if(!tokens.position(center)) break;
            return;
        }
        if(prefix(text,"[zone_")) {mode=1;return;}
        if(mode==1 && prefix(text,"[obelisk_")) mode=2;
    };
    // Windows text-mode fgets: translate CRLF, stop at Ctrl-Z, split at 8191
    // bytes. The EOF probe can revisit the mutated last line; incomplete header
    // retries cannot commit twice because strtok has already inserted a NUL.
    std::string text;for(std::size_t i=0;i<input.size() && input[i]!='\x1a';++i) {if(input[i]=='\r' && i+1<input.size() && input[i+1]=='\n') continue;text+=input[i];}
    for(std::size_t at=0;at<text.size();) {const auto newline=text.find('\n',at);const auto end=std::min(at+8191,newline==std::string::npos?text.size():newline+1);line(std::string_view(text).substr(at,end-at));at=end;}
    LinkedMobConfiguration result;result.maps_.resize(maps.size());
    for(const auto& bucket:buckets) {
        if(bucket->map<0 || static_cast<std::size_t>(bucket->map)>=maps.size()) continue;
        auto& target=result.maps_[static_cast<std::size_t>(bucket->map)];
        target.reserve(bucket->count);
        for(std::size_t index=0;index<bucket->count;++index) {
            const auto& raw=bucket->groups[index];LinkedMobGroup group;group.id=raw.id;group.change_count=raw.change_count;group.respawn_time=raw.respawn_time*1000U;group.portal_map=raw.portal_map;group.portal_id=raw.portal_id;group.spawn_count=raw.spawn_count;
            if(group.portal_id>1000 && group.portal_map>=0 && static_cast<std::size_t>(group.portal_map)<maps.size()) for(const auto& portal:maps[static_cast<std::size_t>(group.portal_map)].definition.portals) if(portal.id==group.portal_id) {group.portal=&portal;break;}
            group.leader.center=raw.leader.center;group.leader.radius=raw.leader.radius;group.leader.radius_twice=twice(raw.leader.radius);group.leader.count=raw.leader.count;group.leader.randomized=raw.leader.randomized;group.leader.minimum_minutes=raw.leader.minimum_minutes;group.leader.random_minutes=raw.leader.random_minutes;group.leader.random_centers=raw.leader.random_centers;
            group.leader.respawn.type=static_cast<std::int32_t>(index);group.leader.respawn.count=1;group.leader.respawn.both=true;
            for(std::int32_t i=0;i<group.change_count;++i) {const auto id=raw.leader.type_ids[static_cast<std::size_t>(i)];auto& d=group.leader.definitions[static_cast<std::size_t>(i)];d=definitions.find(static_cast<std::uint32_t>(id));if(d) group.leader.type_ids[static_cast<std::size_t>(i)]=id;}
            for(std::int32_t i=0;i<group.spawn_count;++i) {const auto& from=raw.spawns[static_cast<std::size_t>(i)];auto& spawn=group.spawns[static_cast<std::size_t>(i)];spawn.center=from.center;spawn.radius=from.radius;spawn.radius_twice=twice(from.radius);spawn.count=from.count;spawn.respawn.count=spawn.count;
                for(std::int32_t j=0;j<group.change_count+1;++j) {const auto id=from.type_ids[static_cast<std::size_t>(j)];auto& d=spawn.definitions[static_cast<std::size_t>(j)];d=definitions.find(static_cast<std::uint32_t>(id));if(d) spawn.type_ids[static_cast<std::size_t>(j)]=id;}
            }
            target.push_back(std::move(group));
        }
        for(auto& group:target) for(std::int32_t i=0;i<group.spawn_count;++i) {auto& spawn=group.spawns[static_cast<std::size_t>(i)];spawn.respawn.both_source=&spawn;}
    }
    return result;
}
LinkedMobConfiguration LinkedMobConfiguration::load(const std::filesystem::path& path,std::span<const ConfiguredMap> maps,const MobRuntimeTemplates& definitions) {
    std::ifstream input(path,std::ios::binary);if(!input) throw std::runtime_error("Cannot open original linked mob configuration");const std::string text((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>());return decode(text,maps,definitions);
}
std::span<const LinkedMobGroup> LinkedMobConfiguration::for_map(std::uint16_t id) const noexcept {return id<maps_.size()?std::span<const LinkedMobGroup>(maps_[id]):std::span<const LinkedMobGroup>{};}
}
