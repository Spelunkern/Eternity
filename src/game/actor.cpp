#include "eternity/game/actor.hpp"
#include "eternity/game/entry_packets.hpp"
#include "eternity/game/item_pool.hpp"
#include <stdexcept>

namespace eternity::game {
namespace {
void append(net::Packet& packet,std::uint32_t value,unsigned int bytes) {for(unsigned int i=0;i<bytes;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
}
Actor::Actor(std::uint32_t id,EffectStatus status,PlayerResources current,std::vector<PersistedEffect> replica,ActorScene& scene,
    ActorEquipmentRules equipment,std::vector<PlayerItem> inventory)
    :id_(id),status_(std::move(status)),scene_(scene),resources_(id,current,*this),effects_(id),replica_(std::move(replica)),equipment_(equipment),inventory_(std::move(inventory)) {}
Actor::~Actor() {effects_.release_all(*this);for(auto& held:retained_inventory_) held.pool->release(*held.item);}
std::array<std::int32_t,3> Actor::heal(std::array<std::int32_t,3> amounts) {return resources_.heal(status_.skills.equipment.base.maximum,amounts);}
void Actor::recover(std::array<std::int32_t,3> amounts,bool send_set,bool recovering) {resources_.recover(status_.skills.equipment.base.maximum,amounts,send_set,attack_.mode,recovering);}
void Actor::wear_defense() {
    const auto result=game::wear_defense(status_.skills.equipment,inventory_,equipment_.items,defense_wear_cursor_,status_.skills.benefits.no_durability_loss,status_.shape);
    if(result.attack_changed) recalculate_attack(status_);
}
void Actor::remove_equipment_modifiers(std::uint8_t slot,const PlayerItem& item,EquipmentRemovalDiagnostics& diagnostics) {
    struct Operations final:EquipmentRemovalWorld {
        Actor& actor;EquipmentRemovalDiagnostics& diagnostics;
        Operations(Actor& owner,EquipmentRemovalDiagnostics& sink):actor(owner),diagnostics(sink) {}
        void remove_weapon_elements() override {
            EffectEquipment equipped;for(const auto& value:actor.inventory_) if(value.bag==0) {if(value.slot==5) equipped.weapon=&value;else if(value.slot==1) equipped.armor=&value;}
            actor.effects_.remove_weapon_elements(actor.status_,equipped,actor.equipment_.items,actor,actor);
        }
        void negative_craft_reduction(const PlayerItem& value,std::int32_t amount) override {diagnostics.negative_craft_reduction(value,amount);}
        void equipment_speed_changed() override {actor.speed_changed(actor.status_);}
        void equipment_maximum_changed(std::uint8_t resource) override {actor.maximum_changed(actor.status_,resource);}
        void equipment_recover() override {actor.recover({0,0,0});}
    } operations(*this,diagnostics);
    game::remove_equipment(status_.skills.equipment,slot,item,equipment_.items,equipment_.enchant,initializing_,operations);
}
bool Actor::check_hp_ability(const SkillDefinitions& definitions) {return effects_.activate_hp(status_,resources_.current(),definitions,*this,*this);}
void Actor::clear_effects_on_death(const std::int32_t& death_penalty) {
    EffectEquipment equipped;
    for(const auto& item:inventory_) if(item.bag==0) {if(item.slot==5) equipped.weapon=&item;else if(item.slot==1) equipped.armor=&item;}
    effects_.clear_on_death(death_penalty,status_,equipped,equipment_.items,*this,*this);
}
void Actor::clear_duel_debuffs() {
    EffectEquipment equipped;
    for(const auto& item:inventory_) if(item.bag==0) {if(item.slot==5) equipped.weapon=&item;else if(item.slot==1) equipped.armor=&item;}
    effects_.clear_duel_debuffs(status_,equipped,equipment_.items,*this,*this);
}
void Actor::end_duel() {
    // PvPModeEnd 0047b200 runs even if the stored duel state is already zero.
    clear_duel_debuffs();reset_damage();relations_.duel_state=0;relations_.duel_type=-1;status_.current_target_type=0;
}
bool Actor::damage(std::int32_t kind,std::uint32_t source,std::int32_t amount,const SkillDefinitions& definitions) {
    struct Operations final:DamageObserver {
        Actor& actor;const SkillDefinitions& definitions;
        Operations(Actor& owner,const SkillDefinitions& skills):actor(owner),definitions(skills) {}
        void defense_wear() override {actor.wear_defense();}
        void record_damage(std::int32_t kind,std::uint32_t source,std::int32_t amount) override {actor.damage_.add(kind,source,amount);}
        void check_hp_ability() override {actor.check_hp_ability(definitions);}
    } operations(*this,definitions);
    const auto& equipment=status_.skills.equipment;
    return resources_.damage({attack_.mode,static_cast<std::uint32_t>(equipment.damage_reduction),equipment.base.maximum.hp},kind,source,amount,operations);
}
void Actor::maximum_changed(EffectStatus& status,std::uint8_t resource) {resources_.maximum_changed(status.skills.equipment.base.maximum,resource,initializing_);}
void Actor::speed_changed(EffectStatus& status) {
    // CUser::SendSpeed 004827b0: suppressed during initialization, proximity view only.
    if(!initializing_ && status.in_zone) scene_.send_nearby(character_speed(id_,status));
}
std::uint8_t Actor::effective_shape() const {
    if(!appearance_.visible) return 13;
    if(appearance_.vehicle_mode==2) return static_cast<std::uint8_t>(static_cast<std::uint32_t>(appearance_.vehicle_shape)+static_cast<std::uint32_t>(appearance_.vehicle_addition));
    return status_.shape;
}
void Actor::send_shape_self() {
    // SendShapeMe 00482510 always uses 051d, including copied-mob shape 102.
    net::Packet packet{0x1d,5};append(packet,id_,4);packet.push_back(effective_shape());scene_.send_self(packet);
}
void Actor::shape_changed(EffectStatus&) {
    // SendShape 00482590 uses the zone's grid view, unlike resource/speed proximity messages.
    if(!status_.in_zone) return;
    const auto shape=effective_shape();net::Packet packet{static_cast<std::uint8_t>(shape==102?0x24:0x1d),5};
    append(packet,id_,4);packet.push_back(shape);if(shape==102) append(packet,status_.form_mob,2);scene_.send_cell_view(packet);
}
void Actor::send_vehicle_partner() {
    // SendShapeVehicle2 00482660.
    if(!status_.in_zone) return;
    net::Packet packet{0x1c,2};append(packet,id_,4);append(packet,appearance_.vehicle_mode==3?appearance_.vehicle_partner:0,4);scene_.send_cell_view(packet);
}
void Actor::run_mode_changed(EffectStatus&,bool running) {
    // SendRunMode 004826c0 sends a command only; it does not mutate the actor's movement state.
    scene_.send_self(net::Packet{0x10,2,static_cast<std::uint8_t>(running)});
}
void Actor::stop_attack(EffectStatus& status) {
    // StatusAttackStop 00489ed0 preserves modes 0 and 1, including their auxiliary fields.
    if(attack_.mode==0 || attack_.mode==1) return;
    attack_.mode=0;status.current_target_type=0;attack_.previous_tick=0;attack_.next_tick=0;
    if(attack_.recovery<1) attack_.recovery=1;
}
void Actor::stop_target_mob(EffectStatus&,std::uint32_t id) {scene_.stop_target_mob(id);}
void Actor::taunt(EffectStatus&,std::uint16_t aggro,float range) {scene_.taunt(aggro,range);}
void Actor::detect_hidden(EffectStatus&,const SkillDefinition& skill) {if(auto* zone=scene_.user_zone()) zone->detect_hidden(id_,skill.apply_range);}
bool Actor::reveal() {return effects_.remove_hide(status_,*this,*this);}
bool Actor::cancel_shape_on_attack() {return effects_.cancel_shape_on_attack(status_,*this,*this);}
void Actor::remove_sleep() {effects_.remove_sleep(status_,*this,*this);}
void Actor::rebuild_attack(EffectStatus& status,bool equipment) {
    if(equipment) rebuild_equipment_attack(status.skills.equipment,inventory_,equipment_.items,equipment_.enchant);
    else rebuild_transformed_attack(status.skills.equipment,equipment_.mobs.find(status.shape_template));
    recalculate_attack(status);
}
void Actor::user_shape_changed(EffectStatus&) {
    // SendUserShape 00482ad0: copied profiles bypass current party/guild lookups.
    if(status_.shape==100 && appearance_.copied_user) {
        if(status_.in_zone) scene_.send_cell_view(character_copied_user_shape(id_,*appearance_.copied_user));
    } else {
        auto packet=character_user_shape(id_,appearance_.identity,attack_.mode==1,attack_.motion,scene_.user_shape_social(),inventory_);
        if(status_.in_zone) scene_.send_cell_view(packet);
    }
}
void Actor::replace_attack_elements(EffectStatus& status,std::uint8_t attribute) {
    EffectEquipment equipped;
    for(const auto& item:inventory_) if(item.bag==0) {
        if(item.slot==5) equipped.weapon=&item;
        else if(item.slot==1) equipped.armor=&item;
    }
    effects_.replace_attack_elements(attribute,status,equipped,equipment_.items,*this,*this);
}
void Actor::finish_periodic_heal(EffectStatus&,const SkillDefinition& skill,const EffectSource&,const std::array<std::int32_t,3>& amounts) {
    // RemApplySkillBuff 004852bd..00485317 broadcasts actual, clamped amounts even when zero.
    const auto actual=heal(amounts);net::Packet packet{0x0f,5};append(packet,id_,4);append(packet,skill.id,2);packet.push_back(skill.level);
    for(const auto value:actual) append(packet,static_cast<std::uint32_t>(value),2);
    if(status_.in_zone) scene_.send_nearby(packet); // PSendViewCombat 004bd8d0 delegates this route.
}
void Actor::finish_periodic_damage(EffectStatus& status,const SkillDefinition& skill,const EffectSource& source,std::uint32_t elapsed) {scene_.finish_periodic_damage(status,skill,source,elapsed);}
std::optional<std::uint32_t> Actor::allocate_id() noexcept {return scene_.allocate_effect_id();}
void Actor::release_id(std::uint32_t id) noexcept {scene_.release_effect_id(id);}
std::uint32_t Actor::tick_ms() const noexcept {return scene_.tick_ms();}
void Actor::erase_persisted(const PersistedEffect& effect) noexcept {replica_.erase(effect.skill_id,effect.level);}
void Actor::persist_applied(const PersistedEffect& effect) noexcept {replica_.apply(effect);}
void Actor::send_self(const net::Packet& packet) noexcept {scene_.send_self(packet);}
std::optional<bool> Actor::party_kind() const noexcept {return scene_.party_kind();}
void Actor::send_party(const net::Packet& packet) noexcept {scene_.send_party(packet);}
void Actor::send_view(const net::Packet& packet) {scene_.send_nearby(packet);}
void Actor::reset_damage() {damage_.reset();scene_.reset_damage();}
}
