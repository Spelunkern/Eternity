#include "eternity/game/zones.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
std::optional<std::uint32_t> guild_rank_window(const MapSettings& map,ZoneLocalTime time) {
    // CMap::IsGuildRankMapOpenTime 00428d20 checks only the first TWO of seven schedule slots.
    for(std::size_t i=0;i<2;++i) {
        const auto& window=map.open_times[i];if(window.day==65535) return {};
        auto end=static_cast<std::uint16_t>(window.hour+window.keep_hours);auto next_day=window.day;
        if(end>24) next_day=window.day==6?0:static_cast<std::uint16_t>(window.day+1);
        bool open=false;
        if(time.weekday==window.day) open=window.hour<=time.hour && time.hour<end;
        else if(time.weekday==next_day) {end=static_cast<std::uint16_t>(end-24);open=time.hour<end;}
        if(open) return (static_cast<std::uint32_t>(window.hour&255U)<<24)|(static_cast<std::uint32_t>(window.day&255U)<<16)|
            (static_cast<std::uint32_t>(time.day&255U)<<8)|static_cast<std::uint32_t>(time.month&255U);
    }
    return {};
}
bool guild_house_enabled(const ZoneGuildAccess& guild) noexcept {
    // CGuild::EnableGuildHouse 0042f0c0 uses rank byte <31 and exact acquired byte 1.
    return guild.ranking_index<31 && guild.house_acquired==1 && guild.house_enabled!=0;
}
ZoneRegistry::ZoneRegistry(const ServerMaps& maps,ZoneServices& services,std::uint32_t serial,ZoneStatistics statistics)
    :maps_(maps),services_(services),nodes_(maps.size()),serial_(serial),statistics_(statistics) {}
bool ZoneRegistry::initialize() {
    // CWorld::InitZones 00414870 prepares one zone per map, then caches every nonpermanent mode.
    if(started_) throw std::logic_error("Zone registry initialization already attempted");
    started_=true;
    for(std::size_t i=0;i<nodes_.size();++i) {
        const auto& map=*maps_.find(static_cast<std::uint16_t>(i));auto zone=services_.allocate();if(!zone) return false;
        if(zone->id==0) zone->id=++serial_;
        zone->map=&map;
        if(!services_.initialize_map(*zone,map) || !services_.initialize_objects(*zone)) return false;
        if(map.settings.create_type==0) nodes_[i].permanent=std::move(zone);
        else {services_.release_objects(*zone);nodes_[i].pool.push_back(std::move(zone));}
    }
    ready_=true;return true;
}
RuntimeZone* ZoneRegistry::select(std::uint16_t id,const ZoneUserAccess& user) {
    // GetZoneByMapID 00413df0. Existing permanent zones bypass all user/creation checks.
    if(!ready_) throw std::logic_error("Zone registry has not initialized");
    const auto* map=maps_.find(id);if(!map) return nullptr;auto& node=nodes_[id];
    if(node.permanent) return node.permanent.get();
    std::optional<std::uint32_t> owner;const bool override=user.permission<=10 && user.instance_owner!=0;
    switch(map->settings.create_type) {
        case 1:break;
        case 2:if(!node.active.empty()) return node.active.front().get();break;
        case 3:
            if(override) owner=user.instance_owner;else owner=user.party_id;
            if(!owner) return nullptr;
            break;
        case 4:if(!user.guild) return nullptr;owner=user.guild->id;break;
        case 5:
            if(override) owner=user.instance_owner;
            else {
                if(!user.guild) return nullptr;
                const auto window=guild_rank_window(map->settings,services_.local_time());
                if(!window || *window==user.last_rank_entry) return nullptr;
                owner=user.guild->id;
            }
            break;
        case 6:
            if(override) owner=user.instance_owner;
            else {if(!user.guild || !guild_house_enabled(*user.guild)) return nullptr;owner=user.guild->id;}
            break;
        default:return nullptr;
    }
    if(owner) {const auto found=node.owned.find(*owner);if(found!=node.owned.end()) return found->second;}
    std::unique_ptr<RuntimeZone> zone;
    if(node.pool.empty()) zone=services_.allocate();else {zone=std::move(node.pool.back());node.pool.pop_back();}
    if(!zone) return nullptr;
    if(zone->id==0) {
        zone->id=++serial_;zone->map=map;
        static_cast<void>(services_.initialize_map(*zone,*map)); // Dynamic selection ignores both initialization return values.
    }
    static_cast<void>(services_.initialize_objects(*zone));if(owner) zone->owner=*owner;zone->expiry_tick=0;
    const auto increment=[](std::uint32_t& current,std::uint32_t& peak){++current;peak=std::max(peak,current);};
    switch(map->settings.create_type) {
        case 3:increment(statistics_.party_current,statistics_.party_peak);++statistics_.total_created;break;
        case 5:increment(statistics_.rank_current,statistics_.rank_peak);++statistics_.total_created;break;
        case 6:increment(statistics_.house_current,statistics_.house_peak);++statistics_.total_created;break;
    }
    auto* result=zone.get();node.active.push_back(std::move(zone));if(owner) node.owned.emplace(*owner,result);
    services_.enqueue_wait(*result);return result;
}
}
