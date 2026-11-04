#include "eternity/game/obelisk_runtime_config.hpp"
#include "eternity/game/obelisk_runtime_portal_config.hpp"
#include <algorithm>
#include <bit>
#include <cstdlib>
#include <memory>
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

struct Record {ObeliskRuntimeInput value;std::array<ObeliskRuntimeSpawnInput,64> groups{};std::size_t count=0;};
struct Zone {std::int32_t map=0;std::array<Record,64> records;std::size_t count=0;};
}
std::vector<ObeliskRuntimeZoneInput> parse_obelisk_runtime_configuration(std::string_view input) {
    std::vector<std::unique_ptr<Zone>> zones;Zone* zone=nullptr;Record* active=nullptr;int mode=0;
    const auto line=[&](std::string_view text) {
        text=text.substr(0,text.find('\0'));if(text.size()<=1) return;
        const auto first=text.find_first_not_of(" \t");if(first==std::string_view::npos) return;text.remove_prefix(first);
        if(text.starts_with("//")||text.starts_with("--")) return;
        if(prefix(text,"[portal_")) {mode=3;return;}
        if(prefix(text,"[bothmob_")) {mode=4;return;}
        if(prefix(text,"[zone_")) {
            mode=1;Tokens tokens{text.substr(6)};std::int32_t map=0;if(!tokens.number(map)) return;
            if(zones.size()==64) throw std::runtime_error("Original obelisk zone capacity exceeded");
            zones.push_back(std::make_unique<Zone>());zone=zones.back().get();zone->map=map;active=nullptr;return;
        }
        if(mode!=1||!zone) return;
        if(prefix(text,"[obelisk_")) {
            if(zone->count==64) throw std::runtime_error("Original obelisk record capacity exceeded");
            active=&zone->records[zone->count];auto& record=active->value;Tokens tokens{text.substr(9)};
            std::uint32_t id=0;if(!tokens.number(id)) return;
            record.base_id=static_cast<std::uint32_t>(zone->map)*65536U+id;
            if(!tokens.position(record.center)) return;
            for(auto& mob:record.mobs) if(!tokens.number(mob)) return;
            if(!tokens.number(record.country)) return;
            ++zone->count;return;
        }
        if(prefix(text,"[mob]")&&active) {
            if(active->count==64) throw std::runtime_error("Original obelisk spawn capacity exceeded");
            auto& spawn=active->groups[active->count];Tokens tokens{text.substr(5)};
            if(!tokens.position(spawn.center)||!tokens.number(spawn.radius)) return;
            for(auto& mob:spawn.mobs) if(!tokens.number(mob)) return;
            if(tokens.number(spawn.count)) ++active->count;
        }
    };
    // Windows text-mode fgets: CRLF translation, Ctrl-Z EOF and 8191-byte reads.
    // Most EOF retries see an incomplete tokenized record. Zone headers need
    // only one token, so a newline-terminated final header commits twice.
    std::string text;for(std::size_t i=0;i<input.size()&&input[i]!='\x1a';++i) {if(input[i]=='\r'&&i+1<input.size()&&input[i+1]=='\n') continue;text+=input[i];}
    std::string_view last_chunk;
    for(std::size_t at=0;at<text.size();) {const auto newline=text.find('\n',at);const auto end=std::min(at+8191,newline==std::string::npos?text.size():newline+1);last_chunk=std::string_view(text).substr(at,end-at);line(last_chunk);at=end;}
    if(!text.empty()&&text.back()=='\n') {
        auto retry=last_chunk;retry=retry.substr(0,retry.find('\0'));
        const auto first=retry.find_first_not_of(" \t");if(first!=std::string_view::npos) {
            retry.remove_prefix(first);
            if(prefix(retry,"[zone_")) {
                constexpr std::string_view delimiters=" ,=()[]{}\t\r\n";
                const auto token=retry.find_first_not_of(delimiters,6);
                if(token!=std::string_view::npos) line(retry.substr(0,retry.find_first_of(delimiters,token)));
            }
        }
    }
    std::vector<ObeliskRuntimeZoneInput> result;
    for(auto& z:zones) {
        ObeliskRuntimeZoneInput value;value.map=z->map;
        for(std::size_t i=0;i<z->count;++i) {auto& r=z->records[i];r.value.spawns.assign(r.groups.begin(),r.groups.begin()+static_cast<std::ptrdiff_t>(r.count));value.records.push_back(std::move(r.value));}
        result.push_back(std::move(value));
    }
    return result;
}
std::vector<ObeliskRuntimePortalInput> parse_obelisk_runtime_portal_configuration(std::string_view input) {
    std::array<ObeliskRuntimePortalInput,64> slots{};std::size_t count=0;ObeliskRuntimePortalInput* active=nullptr;int mode=0;
    const auto line=[&](std::string_view text) {
        text=text.substr(0,text.find('\0'));if(text.size()<=1) return;
        const auto first=text.find_first_not_of(" \t");if(first==std::string_view::npos) return;text.remove_prefix(first);
        if(text.starts_with("//")||text.starts_with("--")) return;
        if(prefix(text,"[portal_")) {
            mode=3;if(count==slots.size()) throw std::runtime_error("Original portal configuration capacity exceeded");
            auto& slot=slots[count];Tokens tokens{text.substr(8)};std::uint32_t id=0;
            if(!tokens.number(id)) return;
            slot.base_id=id+0x10000000U;
            if(!tokens.number(slot.map)||!tokens.number(slot.portal)) return;
            active=&slot;++count;return;
        }
        if(mode==3&&(prefix(text,"[light]")||prefix(text,"[fury]"))) {
            if(!active) return;
            const auto side=prefix(text,"[light]")?0U:1U;
            // Original fury compares six characters but advances seven too.
            Tokens tokens{text.substr(std::min<std::size_t>(7,text.size()))};
            for(auto& ref:active->references[side]) {
                if(!tokens.number(ref.map)) return;
                const auto label=tokens.next();
                if(!label||label->size()<9||!prefix(*label,"obelisk_")) return;
                ref.ordinal=integer(label->substr(8));
            }
            return;
        }
        if(prefix(text,"[bothmob_")) mode=4;
        else if(prefix(text,"[zone_")) mode=1;
    };
    std::string text;for(std::size_t i=0;i<input.size()&&input[i]!='\x1a';++i) {if(input[i]=='\r'&&i+1<input.size()&&input[i+1]=='\n') continue;text+=input[i];}
    for(std::size_t at=0;at<text.size();) {const auto newline=text.find('\n',at);const auto end=std::min(at+8191,newline==std::string::npos?text.size():newline+1);line(std::string_view(text).substr(at,end-at));at=end;}
    // EOF retry changes only the same first field of pending headers/references;
    // no completed portal row or reference changes on that retry.
    return {slots.begin(),slots.begin()+static_cast<std::ptrdiff_t>(count)};
}

}
