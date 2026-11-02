#include "eternity/game/actor.hpp"
#include "eternity/game/actor_load.hpp"
#include "equipment_fixture.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
struct Scene final:ActorScene {
    std::vector<std::string> events;std::optional<bool> party;std::uint32_t next=100;
    void record(char route,const net::Packet& packet) {static constexpr char hex[]="0123456789abcdef";std::string result(1,route);for(auto byte:packet){result+=hex[byte>>4];result+=hex[byte&15];}events.push_back(std::move(result));}
    void send_self(const net::Packet& packet) noexcept override {record('S',packet);}
    void send_nearby(const net::Packet& packet) override {record('N',packet);}
    void send_cell_view(const net::Packet& packet) override {record('C',packet);}
    std::optional<bool> party_kind() const noexcept override {return party;}
    void send_party(const net::Packet& packet) noexcept override {record('P',packet);}
    void reset_damage() override {events.emplace_back("D");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return next++;}
    void release_effect_id(std::uint32_t id) noexcept override {events.push_back("F"+std::to_string(id));}
    std::uint32_t tick_ms() const noexcept override {return 13001;}
    void unexpected() {throw std::runtime_error("Unexpected scene operation");}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected actor operation oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open actor oracle");std::string line;unsigned int count=0;
        const ItemDefinitions default_items;const EnchantValues default_enchant;const MobAttackTemplates default_mobs;
        const ActorEquipmentRules equipment_rules{default_items,default_enchant,default_mobs};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned int op=0,s=0;row>>op>>s;
            require(static_cast<bool>(row) && op<7 && s<128,"Malformed actor vector");Scene scene;if(s&8) scene.party=(s&16)!=0;
            EffectStatus status;status.in_zone=(s&1)!=0;status.shape=std::array<std::uint8_t,6>{0,13,100,101,102,255}[s%6];status.form_mob=0x1234;
            status.movement_block=s&1;status.action_block_a=(s>>1)&1;status.action_block_b=(s>>2)&1;status.casting=(s>>3)&1;status.fixed_form=(s>>4)&1;status.movement_restricted=(s>>5)&1;
            status.skills.equipment.attack_speed=static_cast<std::int32_t>(s%13)-2;status.skills.equipment.move_speed=static_cast<std::int32_t>(s%8)-2;
            status.skills.equipment.base.maximum={1000,2000,3000};status.current_target_type=2;
            const auto current=std::array<PlayerResources,4>{{{100,200,300},{999,1999,2999},{1000,2000,3000},{70000,80000,90000}}}[s%4];
            Actor actor(9001,status,current,{},scene,equipment_rules);actor.initializing((s&4)!=0);
            actor.appearance()={(s&2)!=0,static_cast<std::int32_t>(s%4),25,77,0x78563412};
            actor.attack_state()={std::array<std::int32_t,4>{0,1,2,-1}[s%4],std::array<std::int32_t,4>{-2,0,1,7}[s%4],123,456};
            if(op==0) actor.send_shape_self();else if(op==1) actor.shape_changed(actor.status());else if(op==2) actor.send_vehicle_partner();
            else if(op==3) actor.speed_changed(actor.status());else if(op==4) actor.run_mode_changed(actor.status(),(s&1)!=0);else if(op==5) actor.stop_attack(actor.status());
            else {
                SkillDefinition skill;skill.id=321;skill.level=2;skill.effect=4;skill.detail=81;skill.time_heal={3,7,65535};
                const auto elapsed=std::array<std::uint32_t,4>{0,1001,3000,UINT32_MAX}[s%4];
                EffectSource source;source.previous_tick=13001-elapsed;remove_active_effect(actor.status(),skill,source,13001,{},ItemDefinitions{},actor);
            }
            std::int32_t mode=0,recovery=0,target=0;std::uint32_t previous=0,following=0;PlayerResources resources{};std::size_t events=0;
            require(static_cast<bool>(row>>mode>>recovery>>target>>previous>>following>>resources.hp>>resources.mp>>resources.sp>>events),"Missing actor results");
            const auto context=" at actor vector "+std::to_string(count);
            require(actor.attack_state().mode==mode && actor.attack_state().recovery==recovery && actor.status().current_target_type==target && actor.attack_state().previous_tick==previous && actor.attack_state().next_tick==following,"Actor attack state differs"+context);
            require(actor.resources().hp==resources.hp && actor.resources().mp==resources.mp && actor.resources().sp==resources.sp,"Actor resources differ"+context);
            require(scene.events.size()==events,"Actor notification count differs"+context);
            for(const auto& event:scene.events) {std::string original;row>>original;require(event==original,"Actor notification differs"+context+": "+event+" versus "+original);}
            std::string extra;require(!(row>>extra),"Unexpected actor vector data");++count;
        }
        require(count==896,"Actor oracle coverage changed");
        // An expiry traverses the concrete actor adapter, mutates its DB-agent replica and releases its runtime ID.
        Scene scene;EffectStatus status;status.skills.equipment.base.maximum={100,100,100};Actor actor(9001,status,{75,50,25},{{321,1,1},{321,1,5}},scene,equipment_rules);
        SkillDefinition effect;effect.id=321;effect.level=1;effect.effect=4;effect.detail=82;effect.abilities[0]={1,10};
        const auto id=actor.allocate_id();require(id.has_value(),"Actor allocation failed");actor.effects().append(*id,{{321,1,1},1000,2000,false},effect);
        actor.initializing(true);actor.effects().initialize(actor.status(),actor);actor.initializing(false);scene.events.clear();
        actor.effects().expire(2000,actor.status(),{},ItemDefinitions{},actor,actor);
        require(actor.status().skills.equipment.base.maximum.hp==100 && actor.replica().snapshot().size()==1 && actor.replica().snapshot()[0].remaining==5 && !actor.effects().contains(*id),"Concrete actor expiry did not reverse and update replica");
        require(scene.events.size()==2 && scene.events[0]=="S0e0564000000" && scene.events[1]=="F100","Concrete actor expiry ordering changed");
        storage::ResultSet skill_table;skill_table.columns={"SkillID","SkillLevel","TypeDetail","TypeEffect","Weapon1","Weapon2","Weaponvalue","Bag",
            "AbilityType1","AbilityValue1","AbilityType2","AbilityValue2","AbilityType3","AbilityValue3",
            "AttrType","DefenceType","DefenceValue","ApplyRange","Arrow","ChangeType","ChangeLevel","FixRange","KeepTime","TimeHealHP","TimeHealSP","TimeHealMP"};
        for(unsigned int id:{100,101,102}) {
            storage::Row row(skill_table.columns.size(),storage::Cell{"0"});row[0]=std::to_string(id);row[1]="1";row[2]=id==100?"4":"82";row[3]=id==100?"0":"4";
            row[8]="1";row[9]=id==100?"40":"100";row[10]="2";row[11]=id==100?"60":"100";row[12]="4";row[13]=id==100?"7":"5";row[22]="100";
            if(id==102) row[2]="93";skill_table.rows.push_back(row);
        }
        const auto skills=SkillDefinitions::load(skill_table);storage::ResultSet status_table{{"Level","Job","HP","MP","SP"},{{"1","0","1000","2000","3000"}}};
        const auto statuses=StatusRules::load(status_table);const ItemDefinitions items;const EnchantValues enchant;
        const auto quests=QuestDefinitions::decode(net::Packet{0,0,0,0});const ActorLoadRules rules{statuses,items,enchant,skills,quests,default_mobs};
        PlayerSnapshot player;auto& character=player.core.character;character.id=9001;character.level=1;character.map=1;character.strength=10;character.dexterity=20;
        character.recovery=30;character.intelligence=40;character.wisdom=50;character.luck=60;character.hp=2000;character.mp=3000;character.sp=4000;
        player.skills={{100,5,1,0,0}};player.effects={{101,1,30}};
        BlessingDefinition blessing;blessing.modifiers[1]=10;blessing.modifiers[5]=5;blessing.modifiers[8]=3;Scene loading;
        {
            auto loaded=load_actor(player,rules,{10000,0,2,&blessing},loading);
            require(loaded.actor && !loaded.learned.invalid && loaded.learned.active.size()==1 && loaded.learned.active[0].end_tick==15000,"Actor load lost learned cooldown");
            const auto& loaded_status=loaded.actor->status();const auto maximum=loaded_status.skills.equipment.base.maximum;
            require(maximum.hp==1429 && maximum.mp==2425 && maximum.sp==3110 && loaded_status.skills.equipment.base.attributes.strength==22,"Actor equipment/blessing/passive/effect order changed");
            require(loaded.actor->resources().hp==1429 && loaded.actor->resources().mp==2325 && loaded.actor->resources().sp==3110,"Actor load postponed intermediate effect clamping until final maxima");
            require(loading.events==std::vector<std::string>{"S0b0529230000027909"},"Actor load initialization suppression or active MP notification changed");
            require(loaded_status.skills.benefits.bank_double==1 && loaded_status.skills.benefits.no_durability_loss==1 && loaded_status.skills.benefits.experience_half_bonus==1,"Actor billing phase changed");
            require(loaded.actor->effects().size()==1 && loaded.actor->replica().snapshot().size()==1,"Actor load lost its runtime effect or replica");
        }
        require(loading.events==std::vector<std::string>{"S0b0529230000027909","F100"},"Actor destruction reversed effects or failed to release its instance");
        loading.events.clear();player.skills[0].id=999;const auto invalid=load_actor(player,rules,{10000,0,0,nullptr},loading);
        require(!invalid.actor && invalid.learned.invalid && loading.events.empty(),"Invalid learned definition proceeded into actor initialization");
        player.skills[0].id=100;player.effects={{102,1,30}};bool scene_failure=false;
        try {auto failed=load_actor(player,rules,{10000,0,0,nullptr},loading);}catch(const std::runtime_error&){scene_failure=true;}
        require(scene_failure && loading.events==std::vector<std::string>{"F101"},"Failed actor initialization leaked an allocated effect or emitted reversal packets");
        const auto equipped_definitions=ItemDefinitions::load(item_definitions());EnchantValues equipped_enchant;
        for(std::size_t i=0;i<21;++i) equipped_enchant.additions[i]={static_cast<std::uint16_t>(i*7),static_cast<std::uint16_t>(i*11)};
        const auto mob_templates=MobAttackTemplates::load({{"MobID","Level","HP","Attack1","Attackplus1","Defense","Magic"},{{"1","38","2765","167","83","38","31"}}});
        PlayerItem equipped_weapon;equipped_weapon.type=5;equipped_weapon.type_id=1;equipped_weapon.slot=5;equipped_weapon.craft="01020304050607080903";equipped_weapon.gems[0]=1;
        PlayerItem equipped_armor;equipped_armor.type=16;equipped_armor.type_id=1;equipped_armor.slot=1;
        EffectStatus transformed;transformed.level=30;transformed.in_zone=true;
        transformed.skills.equipment=initialize_equipment({{100,200,300,400,500,600},{10000,20000,30000}},{equipped_weapon,equipped_armor},equipped_definitions,equipped_enchant);
        transformed.skills.equipment.quality[5]=1;transformed.skills.equipment.weapon_damage.fill(7);
        const auto previous_maximum=transformed.skills.equipment.base.maximum;const auto previous_range=transformed.skills.equipment.range;
        Scene transformed_scene;Actor copied(9001,transformed,{50,60,70},{},transformed_scene,{equipped_definitions,equipped_enchant,mob_templates},{equipped_weapon,equipped_armor});
        SkillDefinition transformation;transformation.id=200;transformation.level=1;transformation.effect=4;transformation.detail=90;transformation.change_type=7;transformation.change_level=1;
        apply_active_effect(copied.status(),transformation,0,copied);
        require(copied.status().shape==7 && copied.status().skills.equipment.attack.item_attack==167 && copied.status().skills.equipment.variance==83,"Actor transformation did not use its mob attack template");
        remove_active_effect(copied.status(),transformation,{},10000,{},equipped_definitions,copied);
        const auto& rebuilt=copied.status().skills.equipment;
        require(copied.status().shape==0 && rebuilt.attack.item_attack==559 && rebuilt.variance==533 && rebuilt.attack.physical_defense==33 && rebuilt.attack.magic_defense==34,"Actor transformation reversal did not rebuild cached-quality equipment");
        require(rebuilt.base.maximum.hp==previous_maximum.hp && rebuilt.base.maximum.mp==previous_maximum.mp && rebuilt.base.maximum.sp==previous_maximum.sp && rebuilt.range==previous_range,"Actor attack rebuild changed resource maxima or attack range");
        require(transformed_scene.events==std::vector<std::string>{"C1d052923000007","C1d052923000000"},"Actor transformation view sequence changed");
        Scene elemental_scene;EffectStatus elemental_status;elemental_status.skills.equipment.base.maximum={1000,1000,1000};
        std::array<SkillDefinition,3> elemental_skills{};
        Actor elemental(9001,elemental_status,{500,500,500},{{200,1,30},{201,1,30},{202,1,30}},elemental_scene,equipment_rules);
        for(std::size_t i=0;i<3;++i) {auto& skill=elemental_skills[i];skill.id=static_cast<std::uint16_t>(200+i);skill.level=1;skill.effect=4;skill.detail=i==0?82:94;skill.attribute=static_cast<std::uint8_t>(i);skill.abilities[0]={1,1};elemental.effects().append(static_cast<std::uint32_t>(1000+i),{{skill.id,1,30},10000,12000,false},skill);}
        elemental.effects().initialize(elemental.status(),elemental);
        require(elemental.status().skills.equipment.base.maximum.hp==1002 && elemental.effects().size()==2 && elemental.replica().snapshot().size()==2 && elemental.replica().snapshot()[1].skill_id==202,"Concrete actor lost original shared-cursor element reinitialization behavior");
        require(elemental_scene.events==std::vector<std::string>{"S0e05e9030000","F1001"},"Concrete actor element replacement notifications changed");
        Scene wear_scene;EffectStatus wear_status;wear_status.level=15;
        auto worn=equipped_armor;worn.slot=1;worn.quality=501;worn.bag=0;
        wear_status.skills.equipment=initialize_equipment({}, {worn},equipped_definitions,equipped_enchant);
        recalculate_attack(wear_status);
        Actor wearing(9001,wear_status,{100,100,100},{},wear_scene,{equipped_definitions,equipped_enchant,mob_templates},{worn});
        wearing.wear_defense();
        const auto& wear_equipment=wearing.status().skills.equipment;
        require(wearing.inventory()[0].quality==500 && wear_equipment.quality[1]==1 && wearing.defense_wear_cursor()==3,"Actor wear did not select armor or cross durability threshold");
        require(wear_equipment.attack.physical_defense==wear_status.skills.equipment.attack.physical_defense-502 && wear_equipment.attack.magic_defense==wear_status.skills.equipment.attack.magic_defense-502,"Actor wear did not update quality-dependent defenses");
        const auto calculated=calculate_attack(wearing.status().level,wear_equipment.base.attributes,wear_equipment.attack);
        for(std::size_t category=0;category<3;++category) require(wearing.status().calculated_attack[category].defense==calculated[category].defense,"Actor wear left final defense stale");
        wearing.status().shape=13;wearing.wear_defense();require(wearing.defense_wear_cursor()==3,"Transformed actor advanced wear cursor");
        wearing.status().shape=0;wearing.status().skills.benefits.no_durability_loss=1;wearing.wear_defense();
        require(wearing.defense_wear_cursor()==3 && wearing.inventory()[0].quality==500 && wear_scene.events.empty(),"Durability protection mutated gear or emitted packets");
        std::cout<<count<<" original actor operations, concrete effect expiry and durability integration passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
