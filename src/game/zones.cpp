#include "eternity/game/zones.hpp"
#include "eternity/game/world_admission.hpp"
#include "eternity/game/zone_users.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
std::int32_t RuntimeZone::instance_portal_country(std::int32_t portal_id) const noexcept {
    // GetInsZonePortalCountry 004179e0 scans all ten slots, including ID zero.
    for(const auto& portal:instance_portals) if(portal.id==portal_id) return portal.country;
    return 3;
}
void RuntimeZone::update_instance_portal_country(std::int32_t portal_id,std::int32_t country) {
    // UpdateInsZonePortalCountry 00417a10 stops at the first matching OR empty slot.
    if(portal_id<=1000) return;
    if(!map) throw std::logic_error("Zone map is not initialized");
    if(map->settings.create_type==0) return;
    for(auto& portal:instance_portals) if(portal.id==0 || portal.id==portal_id) {portal={portal_id,country};return;}
}

void RuntimeZone::update_mob_population(MobPool& pool,MobInitPorts& ports,std::uint32_t now,const std::function<void(std::uint32_t)>& obelisks,const std::function<void(std::uint32_t)>& linked_groups) {
    if(!map || !mobs || !mob_population || !obelisks || !linked_groups) throw std::logic_error("Zone respawn services are not initialized");
    mob_population->update(*mobs,pool,ports,now);
    if(map->settings.create_type==0) obelisks(now);
    linked_groups(now);
}
void RuntimeZone::release_mob_population(MobPool& pool) {
    if(!mobs) throw std::logic_error("Zone mob registry is not initialized");
    mobs->release_to_pool(pool);instance_portals.fill({});
}

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
    if(collecting_ || retiring_) throw std::logic_error("Zone registry is retiring an instance");
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
std::vector<RuntimeZone*> ZoneRegistry::logout_snapshot() const {
    if(!ready_) throw std::logic_error("Zone registry has not initialized");
    std::vector<RuntimeZone*> result;
    for(const auto& node:nodes_) {
        if(node.permanent) result.push_back(node.permanent.get());
        for(const auto& zone:node.active) result.push_back(zone.get());
    }
    return result;
}
InstanceExpiryResult ZoneRegistry::expire_instance(RuntimeZone& zone,WorldAdmission& admission,
    std::uint32_t now,ZoneActiveListRemoval& active_list) {
    // CWorldThread::Update 00407a4c..00407bfa, after send processing.
    if(!ready_) throw std::logic_error("Zone registry has not initialized");
    if(collecting_ || (retiring_ && retiring_!=&zone)) throw std::logic_error("Another instance retirement is active");
    const auto* map=zone.map;
    std::size_t index=0;
    while(index<nodes_.size() && index<=UINT16_MAX && maps_.find(static_cast<std::uint16_t>(index))!=map) ++index;
    if(!map || index>=nodes_.size() || index>UINT16_MAX) throw std::logic_error("Instance has a foreign map owner");
    auto& node=nodes_[index];
    auto it=std::find_if(node.active.begin(),node.active.end(),[&](const auto& item){return item.get()==&zone;});
    if(it==node.active.end() || map->settings.create_type==0) throw std::logic_error("Instance is not owned by this registry");
    struct Guard {bool& value;explicit Guard(bool& v):value(v){value=true;}~Guard(){value=false;}} guard(collecting_);
    if(!retiring_) {
        if(!zone.users) throw std::logic_error("Instance lacks its retained user owner");
        if(zone.users->size()!=0 || admission.zone_waiting_count(zone)!=0) return InstanceExpiryResult::occupied;
        if(map->settings.expire_ms!=0) {
            if(zone.expiry_tick==0) {zone.expiry_tick=now+map->settings.expire_ms;return InstanceExpiryResult::waiting;}
            if(zone.expiry_tick>now) return InstanceExpiryResult::waiting;
        }
        // Reserve before detaching the real world node; allocation failure then
        // leaves the live instance untouched.
        node.pool.reserve(node.pool.size()+1);
        active_list.remove_active(zone);
        retiring_=&zone;
        switch(map->settings.create_type) {
            case 3:case 4:--statistics_.party_current;--statistics_.total_created;break;
            case 5:--statistics_.rank_current;--statistics_.total_created;break;
            case 6:--statistics_.house_current;--statistics_.total_created;break;
        }
        if(map->settings.create_type>=3 && map->settings.create_type<=6) {
            const auto owned=node.owned.find(zone.owner);
            if(owned!=node.owned.end() && owned->second==&zone) node.owned.erase(owned);
        }
    }
    // Keep the retiring marker after exceptions so a retry never repeats list
    // removal or counter updates. No new selections are admitted until then.
    services_.release_objects(zone);
    node.pool.push_back(std::move(*it));node.active.erase(it);retiring_=nullptr;
    return InstanceExpiryResult::retired;
}

}
