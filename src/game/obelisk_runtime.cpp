#include "eternity/game/obelisk_runtime_publication.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace eternity::game {
namespace {
std::size_t slots(const ObeliskRuntimeSpawn& s) {
    if(s.count<0) return 0;
    const auto n=static_cast<std::size_t>(s.count);
    if(n>s.members.size()) throw std::invalid_argument("Obelisk member projection too short");
    return n;
}
void country_check(std::int32_t country) {if(country<0 || country>2) throw std::invalid_argument("Invalid obelisk country");}
void put(net::Packet& p,std::uint32_t v,unsigned n) {for(unsigned i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(v>>(i*8)));}
net::Packet log_packet(std::uint16_t opcode,const ObeliskRuntimeMap& map,const MobRuntime& mob,ObeliskRuntimeWorld& w) {
    net::Packet p;put(p,opcode,2);put(p,map.id,2);
    for(float f:{mob.position.x,mob.position.y,mob.position.z}) put(p,std::bit_cast<std::uint32_t>(f),4);
    // Original strcpy into a 32-byte field. Padding is unspecified stack data;
    // native packets deliberately zero unused name bytes.
    if(!mob.definition) throw std::invalid_argument("Obelisk log requires mob definition");
    const auto name=w.mob_name(mob);
    if(name.size()>31) throw std::invalid_argument("Obelisk mob name exceeds original field");
    for(std::size_t i=0;i<32;++i) p.push_back(i<name.size()?static_cast<std::uint8_t>(name[i]):0);
    return p;
}
std::int32_t tile(float value) {const auto v=std::trunc(static_cast<double>(value));return !std::isfinite(v)||v<INT32_MIN||v>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(v);}
float coordinate(ObeliskRuntimeWorld& w,const ObeliskRuntimeSpawn& s,float center) {
    return static_cast<float>((static_cast<double>(static_cast<float>(w.random()%s.radius_twice))-s.radius)+center);
}
std::optional<UserPosition> random_position(const ObeliskRuntimeMap& map,const ObeliskRuntimeSpawn& s,ObeliskRuntimeWorld& w) {
    if(s.radius_twice<=0) throw std::invalid_argument("Invalid obelisk spawn radius");
    const auto x=coordinate(w,s,s.center.x),z=coordinate(w,s,s.center.z);const auto tx=tile(x),tz=tile(z);
    if(tx<0||tz<0||tx>=map.size||tz>=map.size) return std::nullopt;
    const auto at=static_cast<std::uint32_t>(map.size)*static_cast<std::uint32_t>(tz)+static_cast<std::uint32_t>(tx);
    if(at/8>=map.blocked_tiles.size()) throw std::invalid_argument("Obelisk terrain projection too short");
    if((map.blocked_tiles[at/8]>>(at%8))&1U) return std::nullopt;
    return UserPosition{x,s.center.y,z};
}
}
void ObeliskRuntimeSpawn::add(MobRuntime* mob) {if(members.empty()) return;const auto end=members.begin()+static_cast<std::ptrdiff_t>(slots(*this));const auto at=std::find(members.begin(),end,nullptr);if(at!=end) *at=mob;}
void ObeliskRuntimeSpawn::remove(MobRuntime* mob) {if(members.empty()) return;const auto end=members.begin()+static_cast<std::ptrdiff_t>(slots(*this));const auto at=std::find(members.begin(),end,mob);if(at!=end) *at=nullptr;}
void destroy_obelisk(ObeliskRuntimeMap& map,ObeliskRuntime& o,std::size_t index,const MobRuntime& mob,std::int32_t country,bool reset,const std::array<std::int32_t,2>& kc,ObeliskRuntimeWorld& w) {
    if(!map.zone_present) return;
    country_check(country);country_check(o.country);country_check(o.base_country);
    o.principal=nullptr;
    for(auto& s:o.spawns) for(std::size_t i=0;i<slots(s);++i) if(s.members[i]) {w.remove_by_id(s.members[i]->id);s.members[i]=nullptr;}
    const auto previous=o.country;
    o.country=country==2 && o.base_country!=2?o.base_country:country;
    if(!map.reset_send) w.send_change(index);
    if(!reset) {
        if(o.country!=2) {
            bool subtract=true;
            if(kc[static_cast<std::size_t>(o.country)]!=0) {w.add_kill_count(o.country,kc[static_cast<std::size_t>(o.country)]);subtract=!w.kill_count_status14(o.country);}
            if(subtract && previous!=2 && kc[static_cast<std::size_t>(previous)]!=0) w.subtract_kill_count(previous,kc[static_cast<std::size_t>(previous)]);
        }
        net::Packet p;put(p,0x2103,2);put(p,mob.type,2);
        put(p,previous==2?0:static_cast<std::uint32_t>(previous+1),1);put(p,o.country==2?0:static_cast<std::uint32_t>(o.country+1),1);w.broadcast(p);
    }
    auto p=log_packet(0xa03,map,mob,w);put(p,static_cast<std::uint32_t>(previous),1);put(p,static_cast<std::uint32_t>(o.country),1);w.game_log(p);
    if(previous!=o.country) {
        if(o.country!=2) o.last_destroy_country=o.country;
        w.database(obelisk_database_change_packet(o));w.update_portal_links();
    }
}
namespace {
void populate_obelisk(ObeliskRuntimeMap& map,ObeliskRuntime& o,ObeliskRuntimeWorld& w,bool manager_principal,bool spawn_log) {
    country_check(o.country);const auto country=static_cast<std::size_t>(o.country);const auto* definition=o.definitions[country];if(!definition) return;
    o.respawn.next=0;o.respawn.current=1;
    auto* mob=w.allocate(manager_principal);if(!mob) return;
    if(!manager_principal) w.prepare_companion(*mob);
    mob->definition=definition;w.initialize(*mob,*definition,o.respawn);mob->position=o.center;o.principal=mob;w.enter(*mob);
    if(spawn_log) {auto p=log_packet(0xa02,map,*mob,w);put(p,static_cast<std::uint32_t>(o.country),1);w.game_log(p);}
    for(auto& s:o.spawns) {
        definition=s.definitions[static_cast<std::size_t>(o.country)];if(!definition) continue;
        s.respawn.current=s.count;s.respawn.next=0;
        for(std::int32_t n=0;n<s.count;++n) {
            const auto position=random_position(map,s,w);if(!position) continue;
            mob=w.allocate(false);if(!mob) continue;
            w.prepare_companion(*mob);mob->definition=definition;w.initialize(*mob,*definition,s.respawn);mob->position=*position;s.add(mob);w.enter(*mob);
        }
    }
}
}
void respawn_obelisk(ObeliskRuntimeMap& map,ObeliskRuntime& o,ObeliskRuntimeWorld& w) {
    if(map.zone_present) populate_obelisk(map,o,w,true,true);
}
void initialize_obelisks(ObeliskRuntimeMap& map,std::span<ObeliskRuntime> obelisks,ObeliskRuntimeWorld& w) {
    if(!map.zone_present) return;
    for(auto& o:obelisks) populate_obelisk(map,o,w,false,false);
}
void update_obelisk_respawns(ObeliskRuntimeMap& map,std::span<ObeliskRuntime> obelisks,std::uint32_t now,ObeliskRuntimeWorld& w) {
    if(!map.zone_present) return;
    for(auto& o:obelisks) {
        country_check(o.country);
        if(!o.definitions[static_cast<std::size_t>(o.country)]) continue;
        if(o.respawn.current<1) {
            if(o.respawn.next<=now) respawn_obelisk(map,o,w);
            continue;
        }
        for(auto& s:o.spawns) {
            country_check(o.country);
            const auto* definition=s.definitions[static_cast<std::size_t>(o.country)];
            if(!definition || s.respawn.current>=s.count || s.respawn.next>now) continue;
            const auto position=random_position(map,s,w);if(!position) continue;
            auto* mob=w.allocate(true);if(!mob) continue;
            mob->definition=definition;w.initialize(*mob,*definition,s.respawn);mob->position=*position;
            // Original reads the live definition after Init, before AddMob/Enter.
            if(!mob->definition) throw std::invalid_argument("Initialized obelisk mob requires definition");
            s.respawn.current=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(s.respawn.current)+1U);
            s.respawn.next=now+mob->definition->respawn_time;
            s.add(mob);w.enter(*mob);
        }
    }
}
void reset_obelisks(ObeliskRuntimeMap& map,std::span<ObeliskRuntime> obelisks,const std::function<std::uint32_t()>& tick,ObeliskRuntimeWorld& w) {
    if(!map.zone_present) return;
    if(!tick) throw std::invalid_argument("Obelisk reset requires clock");
    const auto deadline=tick()+600000U;
    for(std::size_t i=0;i<obelisks.size();++i) {
        auto& o=obelisks[i];
        if(o.principal) {
            map.reset_send=true;
            try {w.remove_by_id(o.principal->id);} catch(...) {map.reset_send=false;throw;}
            map.reset_send=false;
        }
        // Removal can synchronously destroy the principal and update the record.
        // Reset deliberately does not clear the principal or alter current itself.
        o.respawn.next=deadline;country_check(o.base_country);o.country=o.base_country;
        if(o.country!=2) o.last_destroy_country=o.country;
        w.send_change(i);w.database(obelisk_database_change_packet(o));
    }
    w.update_portal_links();
}
}
