#include "eternity/game/actor.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool condition,const std::string& message) {if(!condition) throw std::runtime_error(message);}
struct Scene final:ActorScene {
    UserShapeSocial social;unsigned int lookups=0;std::vector<net::Packet> sent;
    void unexpected() const {throw std::runtime_error("Unexpected user-shape scene operation");}
    void send_self(const net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const net::Packet&) override {unexpected();}
    void send_cell_view(const net::Packet& packet) override {sent.push_back(packet);}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return {};}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {return 0;}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    void detect_hidden(const SkillDefinition&) override {unexpected();}
    UserShapeSocial user_shape_social() override {++lookups;return social;}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
std::string hex(const net::Packet& packet) {
    static constexpr char digits[]="0123456789abcdef";std::string result;
    for(auto byte:packet) {result+=digits[byte>>4];result+=digits[byte&15];}return result;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected user-shape oracle path");std::ifstream input(argv[1]);require(input.good(),"Cannot read user-shape oracle");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;
        const std::array<std::string,7> crafts={"","1234567890123456789","01234567890123456789","01234567890123456799",
            "012345678901234567x1","0123456789012345671/","012345678901234567890"};
        unsigned int count=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned int s=0,lookups=0;std::string expected;
            require(static_cast<bool>(row>>s>>lookups>>expected),"Invalid user-shape oracle row");
            Scene scene;scene.social.party_role=static_cast<std::uint8_t>(s%3);
            scene.social.guild_name=std::array<std::string,4>{"","G","ABCDEFGHIJKLMNOPQRSTUVWX","Eternity"}[(s>>4)%4];
            std::vector<PlayerItem> inventory;
            for(unsigned int slot=0;slot<13;++slot) {
                if(!(slot==7?bool(s&8):(s+slot)%3!=0)) continue;
                PlayerItem item;item.slot=static_cast<std::uint16_t>(slot);item.type=static_cast<std::uint8_t>(slot*7+s);item.type_id=static_cast<std::uint8_t>(slot*3+s);
                for(std::size_t j=0;j<6;++j) item.gems[j]=static_cast<std::uint8_t>(s+j*17);
                item.craft=crafts[(slot+s)%crafts.size()];inventory.push_back(item);
            }
            PlayerItem hidden;hidden.bag=1;hidden.slot=0;hidden.type=255;inventory.push_back(hidden);
            EffectStatus status;status.in_zone=(s&1)!=0;status.shape=s&2?100:0;
            Actor actor(9001,status,{0,0,0},{},scene,{items,enchant,mobs},inventory);
            auto& appearance=actor.appearance();appearance.identity={static_cast<std::uint8_t>(s%3),static_cast<std::uint8_t>((s>>2)%4),
                static_cast<std::uint8_t>(s*3),static_cast<std::uint8_t>(s*5),static_cast<std::uint8_t>(s%5),static_cast<std::uint8_t>(s%6),
                static_cast<std::uint8_t>(s%2),static_cast<std::uint8_t>(s%4),0xfedcba98+s,
                std::array<std::string,3>{"Actor","ABCDEFGHIJKLMNOPQRST","\xb0\xa1\xb3\xaa"}[(s>>3)%3]};
            actor.attack_state().mode=static_cast<std::int32_t>(s%4);actor.attack_state().motion=static_cast<std::uint8_t>(s*7);
            if(s&4) {
                CopiedUserShape copy;copy.kill_count=0x12345678+s;copy.length=std::array<std::uint8_t,4>{67,73,91,97}[s%4];
                for(std::size_t j=0;j<copy.appearance.size();++j) copy.appearance[j]=static_cast<std::uint8_t>(s+j*11);
                for(std::size_t j=0;j<copy.equipment.size();++j) copy.equipment[j]=static_cast<std::uint8_t>(s+j*13);
                for(std::size_t j=0;j<copy.name.size();++j) copy.name[j]=static_cast<std::uint8_t>(s+j*7);
                for(std::size_t j=0;j<copy.extra.size();++j) copy.extra[j]=static_cast<std::uint8_t>(s+j*19);
                appearance.copied_user=copy;
            }
            actor.user_shape_changed(actor.status());const auto context=" at user-shape vector "+std::to_string(s);
            require(scene.lookups==lookups,"User-shape social lookup differs"+context);
            require(scene.sent.size()==(expected=="-"?0:1),"User-shape packet count differs"+context);
            if(!scene.sent.empty()) require(hex(scene.sent[0])==expected,"User-shape packet bytes differ"+context);
            std::string extra;require(!(row>>extra),"Unexpected user-shape row data");++count;
        }
        require(count==256,"User-shape oracle coverage changed");
        PlayerCore player;player.character.family=3;player.character.grow=2;player.character.hair=1;player.character.face=4;
        player.character.size=5;player.character.job=2;player.character.sex=1;player.character.name="Actor";player.kills[0]=77;
        const auto identity=user_shape_identity(player,1);
        require(identity.country==1 && identity.family==3 && identity.grow==2 && identity.hair==1 && identity.face==4 && identity.size==5 && identity.job==2 && identity.sex==1 && identity.kill_count==77 && identity.name=="Actor","Persisted user-shape identity changed");
        std::cout<<count<<" original user-shape packets and scene routes passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
