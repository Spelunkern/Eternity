#include "eternity/game/effect_cycle.hpp"
#include "eternity/game/entry_packets.hpp"
#include <algorithm>
#include <bit>
#include <limits>

namespace eternity::game {
namespace {
void put(net::Packet& packet,std::size_t offset,std::uint32_t value,std::size_t length) {
    for(std::size_t i=0;i<length;++i) packet[offset+i]=static_cast<std::uint8_t>(value>>(8*i));
}
}
net::Packet effect_removed_packet(std::uint32_t id) {
    // CUser::ChkEndTimeSkill 004865b0: opcode 050e and the skill instance's DWORD ID.
    net::Packet packet(6,0);packet[0]=0x0e;packet[1]=5;put(packet,2,id,4);return packet;
}
net::Packet party_effect_removed_packet(std::uint32_t cid,std::uint16_t id,std::uint8_t level,bool raid) {
    // CUser::SendSkillRemoveParty 004829a0, ps_game.exe.
    net::Packet packet(9,0);packet[0]=raid?0x10:5;packet[1]=0x0c;put(packet,2,cid,4);put(packet,6,id,2);packet[8]=level;return packet;
}
bool EffectCycle::contains(std::uint32_t id) const noexcept {
    return std::any_of(effects_.begin(),effects_.end(),[&](const auto& entry){return entry.second.id==id;});
}
void EffectCycle::release_all(EffectObserver& observer) noexcept {
    // ResetCharacter 0044a050 frees the active list without reversing state or changing persistence.
    while(!effects_.empty()) {auto node=effects_.extract(effects_.begin());observer.release_id(node.mapped().id);}
    shared_cursor_=0;
}
void EffectCycle::append(std::uint32_t id,EffectTimer timer,const SkillDefinition& definition) {
    if(contains(id)) throw std::invalid_argument("Duplicate active effect instance");
    if(timer.effect.skill_id!=definition.id || timer.effect.level!=definition.level) throw std::invalid_argument("Effect timer/definition identity mismatch");
    if(sequence_==std::numeric_limits<std::uint64_t>::max()) throw std::overflow_error("Effect insertion sequence exhausted");
    effects_.emplace(++sequence_,Active{id,std::move(timer),&definition});
}
void EffectCycle::restore(const std::vector<PersistedEffect>& effects,const SkillDefinitions& definitions,std::uint16_t map,
    std::uint32_t disconnected,std::uint32_t tick,EffectObserver& observer) {
    // AddApplySkill 00487ce0 processes each database row in arrival order.
    for(const auto& effect:effects) {
        // PacketUserDBSetInfo 0046d780 case 0505 filters transient effects before AddApplySkill.
        const auto* definition=definitions.find(effect.skill_id,effect.level);
        if(definition && (definition->detail==74 || definition->detail==75 || definition->detail==76 ||
            (definition->detail==90 && (definition->change_type==100 || definition->change_type==101 || definition->change_type==102)))) {
            observer.erase_persisted(effect);continue;
        }
        const auto restored=restore_effects({effect},definitions,map,disconnected,tick);
        for(const auto& expired:restored.erase) observer.erase_persisted(expired);
        for(const auto& timer:restored.active) if(const auto id=observer.allocate_id()) {
            try {append(*id,timer,*definitions.find(effect.skill_id,effect.level));}
            catch(...) {observer.release_id(*id);throw;}
        }
    }
}
void EffectCycle::initialize(EffectStatus& status,EffectWorld& world) {
    // InitApplySkill 00487e50 reads the shared cursor after each application. Nested replacement
    // leaves it at the sentinel, so initialization can revisit earlier surviving effects.
    shared_cursor_=effects_.empty()?0:effects_.begin()->first;
    while(shared_cursor_!=0) {
        const auto active=effects_.at(shared_cursor_);
        apply_active_effect(status,*active.definition,active.timer.attacker_id,world);
        const auto next=shared_cursor_==0?effects_.begin():effects_.upper_bound(shared_cursor_);
        shared_cursor_=next==effects_.end()?0:next->first;
    }
}
bool EffectCycle::activate_hp(EffectStatus& status,const PlayerResources& current,const SkillDefinitions& definitions,
    EffectWorld& world,EffectActivationObserver& observer) {
    // ps_game.exe CUser::ChkHPAbilityApply 00487020.
    auto& passive=status.skills;
    if(passive.hp_ability_id==0 || passive.hp_ability_applied) return false;
    const auto tick=observer.tick_ms();
    if(passive.hp_ability_previous+180000U>tick) return false;
    const auto* skill=definitions.find(passive.hp_ability_id,passive.hp_ability_level);
    if(!skill) return false;
    const auto product=std::bit_cast<std::int32_t>(status.skills.equipment.base.maximum.hp*static_cast<std::uint32_t>(skill->limit_hp));
    const float rounded=static_cast<float>(product);
    const auto threshold=static_cast<std::int32_t>(static_cast<double>(rounded)/100.0);
    if(std::bit_cast<std::int32_t>(current.hp)>threshold) return false;
    const auto id=observer.allocate_id();if(!id) return false;
    const PersistedEffect effect{passive.hp_ability_id,passive.hp_ability_level,skill->keep_time_seconds};
    // SendSkillApply 00482860: DB-agent replica, self, then optional group.
    observer.persist_applied(effect);
    net::Packet packet(9,0);put(packet,0,0x50d,2);put(packet,2,*id,4);put(packet,6,effect.skill_id,2);packet[8]=effect.level;
    observer.send_self(packet);
    if(const auto raid=observer.party_kind()) {
        put(packet,0,*raid?0xc0e:0xc04,2);put(packet,2,character_id_,4);observer.send_party(packet);
    }
    passive.hp_ability_applied=true;passive.hp_ability_previous=tick;
    for(const auto& ability:skill->abilities) apply_effect_ability(status,skill->effect,ability.type,ability.value,world);
    // The runtime lifetime is fixed, independently of the persisted KeepTime.
    append(*id,{effect,tick,tick+30000U,skill->offline_countdown},*skill);
    return true;
}
bool EffectCycle::remove_hide(EffectStatus& status,EffectWorld& world,EffectObserver& observer) {
    // RemApplySkillHide 00489290 removes only the first detail-88 node, without generic unapply.
    if(status.shape==0) return false;
    shared_cursor_=0;
    for(auto entry=effects_.begin();entry!=effects_.end();++entry) {
        shared_cursor_=entry->first;
        if(entry->second.definition->detail!=88) continue;
        shared_cursor_=entry==effects_.begin()?0:std::prev(entry)->first;
        auto removed=effects_.extract(entry);const auto& active=removed.mapped();
        observer.erase_persisted(active.timer.effect);observer.send_self(effect_removed_packet(active.id));
        const bool notify_party=observer.party_kind().has_value();
        status.shape=0;status.skills.equipment.transformed=false;world.shape_changed(status);
        world.run_mode_changed(status,true);observer.release_id(active.id);
        if(notify_party) if(const auto raid=observer.party_kind()) observer.send_party(party_effect_removed_packet(character_id_,active.timer.effect.skill_id,active.timer.effect.level,*raid));
        return true;
    }
    shared_cursor_=0;return false;
}
bool EffectCycle::cancel_shape_on_attack(EffectStatus& status,EffectWorld& world,EffectObserver& observer) {
    // ps_game.exe RemApplySkillToMOB 00489560 removes the first hide/transform
    // candidate only. A protected transform stops the search without removal.
    if(status.shape==0) return false;
    shared_cursor_=effects_.empty()?0:effects_.begin()->first;
    while(shared_cursor_!=0) {
        auto entry=effects_.find(shared_cursor_);const auto active=entry->second;const auto& skill=*active.definition;
        if(skill.detail==90 || skill.detail==88) {
            if(skill.detail==90 && skill.id==127 && skill.level!=100 && skill.change_type!=101) return false;
            shared_cursor_=entry==effects_.begin()?0:std::prev(entry)->first;effects_.erase(entry);
            observer.erase_persisted(active.timer.effect);observer.send_self(effect_removed_packet(active.id));
            const bool notify_party=observer.party_kind().has_value();
            if(skill.detail==90) {
                world.rebuild_attack(status,true);status.shape=0;status.skills.equipment.transformed=false;status.shape_template=0;world.shape_changed(status);
                if(skill.change_type==100) world.user_shape_changed(status);
                else if(skill.change_type==101) {status.fixed_form=0;world.speed_changed(status);}
            } else {
                status.shape=0;status.skills.equipment.transformed=false;world.shape_changed(status);world.run_mode_changed(status,true);
            }
            observer.release_id(active.id);
            if(notify_party) if(const auto raid=observer.party_kind()) observer.send_party(party_effect_removed_packet(character_id_,active.timer.effect.skill_id,active.timer.effect.level,*raid));
            return true;
        }
        const auto next=effects_.upper_bound(shared_cursor_);shared_cursor_=next==effects_.end()?0:next->first;
    }
    return false;
}
void EffectCycle::remove_sleep(EffectStatus& status,EffectWorld& world,EffectObserver& observer) {
    // RemApplySkillSleep 004893a0 removes every detail-66 node, then zeros
    // the sleep block and sends speed before at most 48 deferred group notices.
    if(status.action_block_b==0) return;
    struct Notice {std::uint16_t id;std::uint8_t level;};std::vector<Notice> party;
    shared_cursor_=effects_.empty()?0:effects_.begin()->first;
    while(shared_cursor_!=0) {
        auto entry=effects_.find(shared_cursor_);const auto active=entry->second;
        if(active.definition->detail==66) {
            shared_cursor_=entry==effects_.begin()?0:std::prev(entry)->first;effects_.erase(entry);
            observer.erase_persisted(active.timer.effect);observer.send_self(effect_removed_packet(active.id));
            if(observer.party_kind().has_value() && party.size()<48) party.push_back({active.timer.effect.skill_id,active.timer.effect.level});
            observer.release_id(active.id);
        }
        const auto next=shared_cursor_==0?effects_.begin():effects_.upper_bound(shared_cursor_);shared_cursor_=next==effects_.end()?0:next->first;
    }
    status.action_block_b=0;world.speed_changed(status);
    for(const auto& notice:party) if(const auto raid=observer.party_kind()) observer.send_party(party_effect_removed_packet(character_id_,notice.id,notice.level,*raid));
}
void EffectCycle::replace_attack_elements(std::uint8_t attribute,EffectStatus& status,EffectEquipment equipment,
    const ItemDefinitions& definitions,EffectRemovalWorld& world,EffectObserver& observer) {
    // AddApplySkillBuff 00484f86..004850a2: all conflicting detail-94 nodes, regardless of category/countdown.
    struct Notice {std::uint16_t id;std::uint8_t level;};std::vector<Notice> party;
    shared_cursor_=0;
    for(auto entry=effects_.begin();entry!=effects_.end();entry=shared_cursor_==0?effects_.begin():effects_.upper_bound(shared_cursor_)) {
        shared_cursor_=entry->first;const auto& definition=*entry->second.definition;
        if(definition.detail!=94 || definition.attribute==attribute) continue;
        shared_cursor_=entry==effects_.begin()?0:std::prev(entry)->first;
        auto removed=effects_.extract(entry);const auto& active=removed.mapped();
        observer.erase_persisted(active.timer.effect);observer.send_self(effect_removed_packet(active.id));
        if(observer.party_kind().has_value() && party.size()<48) party.push_back({active.timer.effect.skill_id,active.timer.effect.level});
        const EffectSource source{active.id,active.timer.attacker_id,active.timer.previous_tick,active.timer.attacker_kind};
        remove_active_effect(status,*active.definition,source,0,equipment,definitions,world);
        observer.release_id(active.id);
    }
    shared_cursor_=0;
    for(const auto& notice:party) if(const auto raid=observer.party_kind()) observer.send_party(party_effect_removed_packet(character_id_,notice.id,notice.level,*raid));
}
std::vector<net::Packet> EffectCycle::initial_packets(std::uint32_t tick) const {
    std::vector<ActiveEffectPacketEntry> entries;entries.reserve(effects_.size());
    for(const auto& [sequence,active]:effects_) {
        (void)sequence;
        entries.push_back({active.id,active.timer.effect.skill_id,active.timer.effect.level,active.timer.end,active.definition->offline_countdown});
    }
    return character_effects(entries,tick);
}
void EffectCycle::expire(std::uint32_t tick,EffectStatus& status,EffectEquipment equipment,const ItemDefinitions& definitions,
    EffectRemovalWorld& world,EffectObserver& observer) {
    // ChkEndTimeSkill 004865b0: unlink, DB erase, self notification, unapply/free; party notifications follow.
    struct Notice {std::uint16_t id;std::uint8_t level;};std::vector<Notice> party;
    std::uint64_t cursor=0;
    for(auto entry=effects_.begin();entry!=effects_.end();entry=effects_.upper_bound(cursor)) {
        cursor=entry->first;auto timer=entry->second.timer;
        // The original checks the live definition, including after a definition reload.
        timer.offline_countdown=entry->second.definition->offline_countdown;
        if(!timer.expired(tick)) continue;
        auto removed=effects_.extract(entry);const auto& active=removed.mapped();
        observer.erase_persisted(timer.effect);observer.send_self(effect_removed_packet(active.id));
        if(!timer.offline_countdown && observer.party_kind().has_value() && party.size()<48) party.push_back({timer.effect.skill_id,timer.effect.level});
        const EffectSource source{active.id,timer.attacker_id,timer.previous_tick,timer.attacker_kind};
        const auto& skill=*active.definition;
        const bool samples_clock=(skill.effect==4 && skill.detail==81) || (skill.effect==3 && skill.detail==61);
        remove_active_effect(status,skill,source,samples_clock?observer.tick_ms():tick,equipment,definitions,world);
        observer.release_id(active.id);
    }
    for(const auto& notice:party) if(const auto raid=observer.party_kind()) observer.send_party(party_effect_removed_packet(character_id_,notice.id,notice.level,*raid));
}
EffectStore::EffectStore(storage::SqlServer& database,std::uint8_t server_id,std::string catalog):database_(database),server_id_(server_id) {
    if(catalog.empty() || catalog.size()>128 || std::any_of(catalog.begin(),catalog.end(),[](unsigned char c){return !(c>='a'&&c<='z') && !(c>='A'&&c<='Z') && !(c>='0'&&c<='9') && c!='_';}))
        throw std::invalid_argument("Invalid effect database catalog");
    prefix_="["+catalog+"].dbo.";
}
void EffectCycle::clear_on_death(const std::int32_t& death_penalty,EffectStatus& status,EffectEquipment equipment,
    const ItemDefinitions& definitions,EffectWorld& world,EffectObserver& observer) {
    // ps_game.exe CUser::ClearApplySkillByDeath 00488000. This intentionally
    // differs from ordinary expiry: only selected details reverse abilities.
    shared_cursor_=effects_.empty()?0:effects_.begin()->first;
    while(shared_cursor_!=0) {
        auto entry=effects_.find(shared_cursor_);const auto active=entry->second;const auto& skill=*active.definition;
        if(!skill.preserve_on_death && !(death_penalty==-1 && active.timer.effect.skill_id>200 && active.timer.effect.level!=100)) {
            shared_cursor_=entry==effects_.begin()?0:std::prev(entry)->first;
            effects_.erase(entry);
            if(skill.detail==6) status.skills.hp_ability_applied=false;
            if(skill.detail==6 || skill.detail==62 || skill.detail==67 || skill.detail==82 || skill.detail==91) {
                for(std::size_t i=0;i<skill.abilities.size();++i) {
                    const auto ability=skill.abilities[i];const auto positive=skill.detail==62 || skill.detail==67 || (skill.detail==91 && i==0);
                    apply_effect_ability(status,skill.effect,ability.type,positive?ability.value:-static_cast<std::int32_t>(ability.value),world);
                }
            } else if(skill.detail==90) {
                world.rebuild_attack(status,true);status.shape=0;status.skills.equipment.transformed=false;status.shape_template=0;
                if(skill.change_type==100) world.user_shape_changed(status);
            }
            observer.erase_persisted(active.timer.effect);observer.release_id(active.id);
        }
        const auto next=shared_cursor_==0?effects_.begin():effects_.upper_bound(shared_cursor_);
        shared_cursor_=next==effects_.end()?0:next->first;
    }
    status.action_block_a=0;status.movement_block=0;status.action_block_b=0;status.fixed_form=0;status.form_mob=0;
    status.zero_attack_attribute_buff=false;status.skills.equipment.attack_attribute_buffs.fill(0);
    status.no_mob_targeting=false;status.block_death=false;status.block_dying=false;status.shape=0;status.skills.equipment.transformed=false;
    for(auto& defense:status.defense) {defense.mirror_skill=0;defense.block_mode=0;}
    status.block_all=0;status.giveup_sp_mp=0;status.chase_id=0;status.shape_template=0;status.casting=0;
    if(equipment.armor) status.skills.equipment.defense_attribute=item_attribute(*equipment.armor,definitions);
    if(equipment.weapon) status.skills.equipment.attack_attribute=item_attribute(*equipment.weapon,definitions);
    status.skills.equipment.defense_attribute_skill=std::any_of(effects_.begin(),effects_.end(),[](const auto& entry){return entry.second.definition->detail==95;});
    world.speed_changed(status);
}
bool EffectReplica::erase(std::uint16_t id,std::uint8_t level) {
    // ps_dbAgent.exe CUser::PacketUserSkillApply 00406250, case 0902: remove the first matching replica node.
    const auto found=std::find_if(effects_.begin(),effects_.end(),[&](const auto& effect){return effect.skill_id==id && effect.level==level;});
    if(found==effects_.end()) return false;
    effects_.erase(found);return true;
}
void EffectReplica::apply(PersistedEffect effect) {
    // ps_dbAgent.exe PacketUserSkillApply 00406250 case 0901 updates the first
    // matching ID/level in place; an absent pair is appended at the tail.
    const auto found=std::find_if(effects_.begin(),effects_.end(),[&](const auto& existing){return existing.skill_id==effect.skill_id && existing.level==effect.level;});
    if(found!=effects_.end()) *found=effect;
    else effects_.push_back(effect);
}
bool EffectStore::replace(std::uint32_t uid,std::uint32_t cid,const std::vector<PersistedEffect>& effects) {
    if(uid==0 || uid>INT32_MAX || cid==0 || cid>INT32_MAX) throw std::invalid_argument("Invalid effect owner identity");
    const auto saved=[&](std::string query,std::span<const storage::Parameter> parameters){
        const auto result=database_.query(query,parameters);
        if(result.size()!=1 || result[0].rows.size()!=1 || result[0].rows[0].size()!=1 || !result[0].rows[0][0]) throw std::runtime_error("Original effect save result changed");
        if(*result[0].rows[0][0]!="1") throw std::runtime_error("Original effect save procedure failed");
    };
    database_.begin();
    try {
        const std::array<storage::Parameter,3> owner={std::int64_t{server_id_},std::int64_t{uid},std::int64_t{cid}};
        const auto owned=database_.query("SELECT CharID FROM "+prefix_+"Chars WITH(UPDLOCK,HOLDLOCK) WHERE ServerID=? AND UserUID=? AND CharID=? AND Del=0;",owner);
        if(owned.size()!=1 || owned[0].columns.size()!=1) throw std::runtime_error("Effect ownership result changed");
        if(owned[0].rows.empty()) {database_.rollback();return false;}
        // DBCharacter::SaveCharacter 00423190 deletes all saved effects, then inserts its ordered replica.
        const std::array<storage::Parameter,1> character={std::int64_t{cid}};
        saved("DECLARE @Saved int; EXEC @Saved="+prefix_+"usp_Save_Char_ApplySkill_Del_E ?,0,0,1; SELECT @Saved;",character);
        for(const auto& effect:effects) {
            const std::array<storage::Parameter,4> parameters={std::int64_t{cid},std::int64_t{std::bit_cast<std::int16_t>(effect.skill_id)},
                std::int64_t{effect.level},std::int64_t{std::bit_cast<std::int32_t>(effect.remaining)}};
            saved("DECLARE @Saved int; EXEC @Saved="+prefix_+"usp_Save_Char_ApplySkill_Add_E2 ?,?,?,?; SELECT @Saved;",parameters);
        }
        database_.commit();return true;
    } catch(...) {try{database_.rollback();}catch(...){}throw;}
}
}
