#include "eternity/game/actor_load.hpp"

namespace eternity::game {
LoadedActor load_actor(const PlayerSnapshot& player,const ActorLoadRules& rules,ActorLoadOptions options,ActorScene& scene) {
    // PacketUserDBSetInfo 0046d780 loads 0504/0505/0506 before PacketUserDBChar 0046cf60 case 0407.
    LoadedActor result;result.learned=restore_skills(player.skills,rules.skills,options.tick_ms);
    if(result.learned.invalid) return result;
    EffectStatus initial;initial.level=player.core.character.level;
    result.actor=std::make_unique<Actor>(player.core.character.id,std::move(initial),
        PlayerResources{player.core.character.hp,player.core.character.mp,player.core.character.sp},player.effects,scene,
        ActorEquipmentRules{rules.items,rules.enchant,rules.mobs},options.inventory_pool?std::vector<PlayerItem>{}:player.items);
    auto& actor=*result.actor;auto& status=actor.status();
    actor.account_id()=player.core.user_id;actor.money()=player.core.money;
    if(options.inventory_pool) for(const auto& item:player.items) actor.load_inventory_item(item,*options.inventory_pool);
    actor.appearance().identity=user_shape_identity(player.core,options.country);
    actor.relations().guild_id=player.core.guild_id;
    actor.effects().restore(player.effects,rules.skills,player.core.character.map,options.disconnected_seconds,options.tick_ms,actor);
    result.quests=restore_quests(player,rules.quests,options.tick_ms);
    // InitEquipment 00454560 applies equipped slots, then blessing and SetAttack.
    actor.initializing(true);
    status.skills.equipment=initialize_equipment(rules.statuses.base(player.core.character),actor.inventory(),rules.items,rules.enchant);
    actor.initializing(false);
    if(options.blessing) result.blessing=initialize_blessing(status.skills,result.attack_recovery,*options.blessing,
        {status.calculated_attack[0].defense,status.calculated_attack[1].defense});
    recalculate_attack(status);
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
    // InitApplySkill 00487e50 does not set the initialization flag: these callbacks may clamp current resources.
    actor.effects().initialize(status,actor);
    apply_billing(status.skills,options.billing_type);
    actor.finish_loading_resources();actor.stop_attack(status);
    return result;
}
}
