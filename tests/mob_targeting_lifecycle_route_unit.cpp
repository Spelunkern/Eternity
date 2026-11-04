#include "eternity/game/mob_targeting_lifecycle_route.hpp"
#include "eternity/game/actor.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
struct Scene final:ActorScene {
    void send_self(const eternity::net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected scene send");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected scene send");}
    std::optional<bool> party_kind() const noexcept override {return std::nullopt;}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::runtime_error("Unexpected scene reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 123;}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected target reset");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected taunt");}
    ZoneUsers* user_zone() override {throw std::runtime_error("Unexpected user grid");}
    UserShapeSocial user_shape_social() override {throw std::runtime_error("Unexpected shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected damage");}
};

void assign(Actor& a,const MobTargetingPvpRelation& p) {
    a.relations().duel_state=std::bit_cast<std::int32_t>(p.duel_state);
    a.relations().duel_opponent=p.duel_peer;
    a.relations().guild_id=p.guild;
    a.relations().opposing_guild_id=p.opposing_guild;
}
int main(int argc,char** argv) {
    if(argc!=2)throw std::runtime_error("Expected original decision fixture");
    std::ifstream file(argv[1]);if(!file)throw std::runtime_error("Missing fixture");
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;Scene scene;
    unsigned count=0,duels=0,guilds=0,skips=0,rewards=0;std::string line;
    while(std::getline(file,line)) {
        if(line.empty()||line[0]=='#')continue;
        std::istringstream row(line);unsigned same,flag,initial,route,final;
        MobTargetingPvpRelation a{},b{};
        if(!(row>>same>>flag>>initial>>a.id>>a.duel_state>>a.duel_peer>>a.guild>>a.opposing_guild
             >>b.id>>b.duel_state>>b.duel_peer>>b.guild>>b.opposing_guild>>route>>final))
            throw std::runtime_error("Invalid original decision record");
        auto penalty=std::bit_cast<std::int32_t>(initial);
        auto result=mob_targeting_player_death_route(a,b,same!=0,static_cast<std::uint8_t>(flag),penalty);
        if(static_cast<unsigned>(result)!=route||std::bit_cast<unsigned>(penalty)!=final)
            throw std::runtime_error("Original decision mismatch at "+std::to_string(count));
        Actor victim(a.id,{}, {11,22,33},{},scene,{items,enchant,attacks});
        Actor attacker(b.id,{}, {44,55,66},{},scene,{items,enchant,attacks});
        assign(victim,a);assign(attacker,b);victim.attack_state().mode=7;
        const Actor& peer=same?victim:attacker;
        penalty=std::bit_cast<std::int32_t>(initial);
        if(mob_targeting_player_death_route(victim,peer,static_cast<std::uint8_t>(flag),penalty)!=result ||
           std::bit_cast<unsigned>(penalty)!=final || victim.attack_state().mode!=7 || victim.resources().hp!=11)
            throw std::runtime_error("Actual Actor decision or mutation mismatch");
        if(mob_targeting_pvp_death_type(victim,peer)!=mob_targeting_pvp_death_type(a,b,same!=0))
            throw std::runtime_error("Actual Actor classifier mismatch");
        // Live relation changes must be observed on the next call, no cached view.
        victim.relations().duel_state=4;victim.relations().duel_opponent=peer.id();
        if(!same && mob_targeting_pvp_death_type(victim,peer)!=MobTargetingPvpDeathType::duel)
            throw std::runtime_error("Actor live relation update lost");
        ++count;duels+=route==4;guilds+=route==5;skips+=route==0;rewards+=route==3;
    }
    if(count!=16384||!duels||!guilds||!skips||!rewards)throw std::runtime_error("Missing decision coverage");
    std::cout<<count<<" original bounded Death decisions and actual Actor compositions; duel="<<duels<<" guild="<<guilds<<" skip="<<skips<<" rewards="<<rewards<<'\n';
}
