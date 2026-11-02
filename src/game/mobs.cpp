#include "eternity/game/mobs.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>

namespace eternity::game {
namespace {
std::int64_t value(const storage::ResultSet& table,const storage::Row& row,std::string_view key,std::int64_t low,std::int64_t high) {
    const auto column=std::find(table.columns.begin(),table.columns.end(),key);if(column==table.columns.end()) throw std::runtime_error("Missing mob runtime column");
    const auto at=static_cast<std::size_t>(column-table.columns.begin());
    if(at>=row.size()) throw std::runtime_error("Missing mob runtime value");
    if(!row[at]) return 0;
    const auto& text=*row[at];std::int64_t parsed=0;const auto result=std::from_chars(text.data(),text.data()+text.size(),parsed);
    if(result.ec!=std::errc{} || result.ptr!=text.data()+text.size() || parsed<low || parsed>high) throw std::runtime_error("Invalid mob runtime value");
    return parsed;
}
std::int32_t integer(long double v) {v=std::trunc(v);return !std::isfinite(v) || v<INT32_MIN || v>INT32_MAX?INT32_MIN:static_cast<std::int32_t>(v);}
std::int32_t add(std::int32_t a,std::int32_t b,std::uint32_t c) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b)+c);}
long double f32(std::int32_t v) {return static_cast<float>(v);}
}
MobRuntimeTemplates MobRuntimeTemplates::load(const storage::ResultSet& table) {
    // CGameData::LoadConstMob 004c1720 normalizes HP/attribute and copies
    // Attacktime3 (column 63) to dwDefenseAddSH, independently of attack lookup.
    MobRuntimeTemplates result;
    for(const auto& row:table.rows) {
        const auto word=[&](std::string_view key){return static_cast<std::uint16_t>(value(table,row,key,INT16_MIN,UINT16_MAX));};
        const auto dword=[&](std::string_view key){return static_cast<std::uint32_t>(value(table,row,key,INT32_MIN,UINT32_MAX));};
        MobRuntimeTemplate m;m.id=word("MobID");m.level=word("Level");m.sp=word("SP");m.mp=word("MP");m.dexterity=word("Dex");m.wisdom=word("Wis");m.luck=word("Luc");m.physical_defense=word("Defense");m.magic_defense=word("Magic");m.hp=dword("HP");if(m.hp==0) m.hp=1;m.shield_defense=dword("Attacktime3");
        const auto selector=value(table,row,"Attackspecial3",0,255);
        constexpr std::array<std::uint32_t,16> times={5000,30000,60000,180000,300000,600000,900000,1800000,3600000,14400000,43200000,86400000,259200000,7200000,0,604800000};
        m.respawn_selector=selector>15?0:static_cast<std::uint8_t>(selector);
        m.respawn_time=times[m.respawn_selector];
        m.ai=static_cast<std::uint8_t>(value(table,row,"AI",0,255));m.attribute=static_cast<std::uint8_t>(value(table,row,"Attrib",0,255));if(m.attribute>4) m.attribute=0;result.records_[m.id]=m;
    }
    return result;
}
const MobRuntimeTemplate* MobRuntimeTemplates::find(std::uint32_t id) const noexcept {
    if(id>65535) return nullptr;
    const auto at=records_.find(static_cast<std::uint16_t>(id));return at==records_.end() || at->second.id==0 || at->second.level==0 || at->second.hp==0?nullptr:&at->second;
}
void account_regular_mob_corpse(MobRespawn& respawn,const MobRuntimeTemplate& definition,std::uint32_t now,RandomSource& random) {
    if(respawn.obelisk || respawn.both) throw std::invalid_argument("Regular corpse accounting requires a regular/member respawn");
    if(respawn.current==respawn.count) {
        auto deadline=now+definition.respawn_time;
        if(definition.respawn_time!=0 && definition.respawn_selector>=9) {
            const auto minutes=random.next()%120-60;
            deadline+=static_cast<std::uint32_t>(minutes)*60000U;
        }
        respawn.next=deadline;
    }
    respawn.current=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(respawn.current)-1U);
}
void rebuild_mob_attack(MobRuntime& m) {
    if(!m.definition) throw std::invalid_argument("Mob attack requires a definition");
    const auto& d=*m.definition;const auto dex=f32(d.dexterity),level=f32(d.level),luck=f32(d.luck);
    for(std::size_t i=0;i<2;++i) {auto& a=m.attack[i];const auto& b=m.additions[i];a.accuracy=integer(dex*0.5F+f32(b.accuracy)+level);a.attack=b.attack;}
    m.attack[0].evasion=integer(dex*0.5F+level/2.0F+luck*0.125F+f32(m.additions[0].evasion));
    m.attack[0].defense=integer(f32(d.physical_defense)*static_cast<long double>(m.multipliers[2])+f32(m.additions[0].defense));
    m.attack[1].evasion=integer(dex*0.5F+luck*0.125F+f32(m.additions[1].evasion)+level);
    m.attack[1].defense=add(std::bit_cast<std::int32_t>(d.shield_defense),m.additions[1].defense,d.physical_defense);
    m.attack[2].accuracy=m.additions[2].accuracy;m.attack[2].evasion=m.additions[2].evasion;m.attack[2].attack=add(m.wisdom,m.additions[2].attack,d.level);m.attack[2].defense=add(m.additions[2].defense,0,d.magic_defense);
    const auto critical=integer(f32(m.luck)*static_cast<long double>(0.19F));for(auto& a:m.attack) {a.critical_attack=critical;a.critical_defense=0;}
}
void reset_mob_attack(MobRuntime& m) {
    m.attack_disabled.fill(0);m.attack.fill({});m.additions.fill({});m.move_speed=2;m.attack_speed=3;m.current_attack=0;m.attack_times.fill(0);rebuild_mob_attack(m);
}
void initialize_mob(MobRuntime& m,std::uint32_t type,MobRespawn* respawn,MobInitPorts& ports) {
    if(!m.definition || !ports.tick || !ports.compiled_ai || !ports.lua_ai) throw std::invalid_argument("Incomplete mob initialization services");
    const auto& d=*m.definition;
    m.type=static_cast<std::uint16_t>(type);m.recover_time=5000;m.multipliers.fill(1);m.status=0;m.use_pais=false;m.attacked=false;m.in_battle=false;m.attack_at_moment=false;m.battle_type=0;m.check_attack_count=false;m.attacked_count=0;m.recover_enabled=false;m.target_kind=0;m.target_id=0;m.attack_enabled.fill(true);
    m.hp=d.hp;m.sp=d.sp;m.mp=d.mp;m.dexterity=d.dexterity;m.wisdom=d.wisdom;m.luck=d.luck;m.attribute=d.attribute;m.next_recover=0;m.next_help=0;m.next_attack=0;m.next_chase=0;
    const auto delay=ports.random.next()%60000;m.next_move=static_cast<std::uint32_t>(delay)+ports.tick();m.next_target=0;const auto now=ports.tick();m.death_remove=false;m.death_time=0;m.next_new_target=now+5000U;
    m.previous_position={};m.move_direction={};m.previous_move_time=0;m.move_range=0;m.respawn=respawn;m.stun=0;m.stop=0;m.sleep=0;m.path.reset();m.trail.clear();m.previous_attack_user[0]=0;m.previous_attack_skill[0]=0;reset_mob_attack(m);
    const auto ai=m.definition->ai;if(ai>=10 && ai<100 && ports.compiled_ai(m,ai)) {m.status=100;m.use_pais=true;}
    m.lock_on_tick=0;m.unbeatable=0;if(m.definition->ai>=100) ports.lua_ai(m);m.vision_fixed_user=0;
}
}
