#include "eternity/game/actor_load.hpp"

namespace eternity::game {
ActorLoadPipeline::ActorLoadPipeline(const PlayerSnapshot& player,const ActorLoadRules& rules,ActorLoadOptions options,ActorScene& scene):player_(player),rules_(rules),options_(options) {
    // PacketUserDBSetInfo 0046d780 loads 0504/0505/0506 before PacketUserDBChar 0046cf60 case 0407.
    auto& result=result_;result.learned=restore_skills(player.skills,rules.skills,options.tick_ms);
    if(result.learned.invalid) return;
    EffectStatus initial;initial.level=player.core.character.level;
    result.actor=std::make_unique<Actor>(player.core.character.id,std::move(initial),
        PlayerResources{player.core.character.hp,player.core.character.mp,player.core.character.sp},player.effects,scene,
        ActorEquipmentRules{rules.items,rules.enchant,rules.mobs},options.inventory_pool?std::vector<PlayerItem>{}:player.items);
    auto& actor=*result.actor;
    actor.account_id()=player.core.user_id;actor.money()=player.core.money;
    if(options.inventory_pool) for(const auto& item:player.items) actor.load_inventory_item(item,*options.inventory_pool);
    actor.appearance().identity=user_shape_identity(player.core,options.country);
    actor.relations().guild_id=player.core.guild_id;
    actor.effects().restore(player.effects,rules.skills,player.core.character.map,options.disconnected_seconds,options.tick_ms,actor);
    result.quests=restore_quests(player,rules.quests,options.tick_ms);
}
void ActorLoadPipeline::advance(unsigned int expected) {
    if(!result_.actor || result_.learned.invalid || stage_!=expected) throw std::logic_error("Actor load phase is missing, repeated or out of order");
    ++stage_;
}
void ActorLoadPipeline::equipment() {
    advance(0);const auto& player=player_;const auto& rules=rules_;const auto options=options_;auto& result=result_;auto& actor=*result.actor;auto& status=actor.status();
    // InitEquipment 00454560 applies equipped slots, then blessing and SetAttack.
    actor.initializing(true);
    status.skills.equipment=initialize_equipment(rules.statuses.base(player.core.character),actor.inventory(),rules.items,rules.enchant);
    actor.initializing(false);
    if(options.blessing) result.blessing=initialize_blessing(status.skills,result.attack_recovery,*options.blessing,
        {status.calculated_attack[0].defense,status.calculated_attack[1].defense});
    recalculate_attack(status);
}
void ActorLoadPipeline::passive() {
    advance(1);const auto& player=player_;const auto& rules=rules_;auto& actor=*result_.actor;auto& status=actor.status();
    // InitPassive 004899a0 suppresses notifications but preserves per-ability attack recalculations.
    actor.initializing(true);
    std::optional<std::uint8_t> weapon,shield;
    for(const auto& item:actor.inventory()) if(item.bag==0 && (item.slot==5 || item.slot==6)) {
        const auto* definition=rules.items.find(item.type,item.type_id);
        if(!definition) throw std::runtime_error("Missing original equipped item definition during actor load");
        (item.slot==5?weapon:shield)=definition->real_type;
    }
    for(const auto& learned:player.skills) {
        const auto& skill=*rules.skills.find(learned.id,learned.level);if(skill.effect!=0) continue;
        if(skill.detail==4) {
            for(const auto& ability:skill.abilities) apply_effect_ability(status,skill.effect,ability.type,ability.value,actor);
        } else {
            add_passive(status.skills,skill,weapon,shield);
            const bool equipped_weapon=weapon && (*weapon==skill.weapon1 || *weapon==skill.weapon2);
            if((skill.detail==1 && equipped_weapon) || (skill.detail==10 && shield && *shield==skill.weapon1)) recalculate_attack(status);
            if(skill.detail==8 && equipped_weapon) actor.speed_changed(status);
        }
    }
    actor.initializing(false);
}
void ActorLoadPipeline::effects() {
    advance(2);auto& actor=*result_.actor;auto& status=actor.status();
    // InitApplySkill 00487e50 does not set the initialization flag: these callbacks may clamp current resources.
    actor.effects().initialize(status,actor);
}
void ActorLoadPipeline::billing() {
    advance(3);auto& actor=*result_.actor;apply_billing(actor.status().skills,options_.billing_type);
    actor.finish_loading_resources();
}
LoadedActor ActorLoadPipeline::release() {
    if(result_.actor && stage_!=4) throw std::logic_error("Actor released before complete initialization");
    return std::move(result_);
}
LoadedActor load_actor(const PlayerSnapshot& player,const ActorLoadRules& rules,ActorLoadOptions options,ActorScene& scene) {
    ActorLoadPipeline pipeline(player,rules,options,scene);
    if(pipeline.result().actor) {pipeline.equipment();pipeline.passive();pipeline.effects();pipeline.billing();auto& actor=*pipeline.result().actor;actor.stop_attack(actor.status());}
    return pipeline.release();
}
}
