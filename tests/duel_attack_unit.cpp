#include "equipment_fixture.hpp"
#include "eternity/game/actor.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const net::Packet& packet) {static constexpr char digits[]="0123456789abcdef";std::string value;for(auto byte:packet) {value+=digits[byte>>4];value+=digits[byte&15];}return value;}
std::vector<std::uint32_t> item_state(const DuelDropItem& drop) {
    return {drop.item.type,drop.item.type_id,drop.item.count,drop.item.quality,static_cast<std::uint32_t>(static_cast<unsigned char>(drop.item.make_type.at(0))),
        static_cast<std::uint32_t>(drop.definition!=nullptr),drop.user_id,drop.party_id,drop.drop_type,drop.drop_id,drop.money,
        std::bit_cast<std::uint32_t>(drop.position.x),std::bit_cast<std::uint32_t>(drop.position.y),std::bit_cast<std::uint32_t>(drop.position.z)};
}
struct Scene final:ActorScene {
    std::uint32_t id;std::vector<std::string>& trace;
    Scene(std::uint32_t identity,std::vector<std::string>& events):id(identity),trace(events) {}
    void send_self(const net::Packet& packet) noexcept override {trace.push_back("S"+std::to_string(id)+":"+hex(packet));}
    void reset_damage() override {trace.push_back("H"+std::to_string(id));}
    std::optional<bool> party_kind() const noexcept override {return {};}
    void send_party(const net::Packet&) noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}
    void unexpected() {throw std::runtime_error("Unexpected duel scene operation");}
    void send_nearby(const net::Packet&) override {unexpected();}
    void send_cell_view(const net::Packet&) override {unexpected();}
    void stop_target_mob(std::uint32_t) override {unexpected();}
    void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}
    UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
struct World final:DuelWorld {
    std::array<Actor*,2> actors{};std::array<DuelDropItem,9> items;
    std::vector<std::string> trace;std::vector<std::int32_t> randoms;
    bool available=false,allocate=false;int year=2026;std::size_t random_index=0;
    unsigned int index(const Actor& actor) {require(actors[0]==&actor || actors[1]==&actor,"Unknown duel actor");return actors[0]==&actor?0:1;}
    Actor* find_duel_user(Actor& requester,std::uint32_t id) override {
        require(&requester==actors[0],"Unexpected duel requester");trace.push_back("Q"+std::to_string(id));
        return id==9001?actors[0]:id==9002 && available?actors[1]:nullptr;
    }
    UserPosition duel_position(const Actor& actor) override {const auto i=static_cast<float>(index(actor));return {10000.25F+i,-30.5F+i,-9000.75F+i};}
    std::int32_t drop_random() override {const auto value=randoms.at(random_index++);trace.push_back("R"+std::to_string(value));return value;}
    DuelDropItem* allocate_drop_item() override {trace.push_back("A"+std::to_string(allocate));return allocate?&items[0]:nullptr;}
    bool enter_duel_item(Actor& owner,DuelDropItem& drop,bool notify) override {
        std::string event="E"+std::to_string(&drop-items.data())+","+std::to_string(17+index(owner))+","+std::to_string(notify);
        for(auto value:item_state(drop)) event+=","+std::to_string(value);trace.push_back(event);return false;
    }
    GameLogSubject duel_log_subject(const Actor& actor) override {
        const auto i=index(actor);GameLogSubject result;result.account_id=12345+i;result.character_id=23456+i;result.experience=0xffff0000U+i;
        result.account_name=i==0?"long-account-name":"ab";result.character_name=i==0?"LongCharacterName":"Cd";result.level=static_cast<std::uint16_t>(123+i);result.map=static_cast<std::uint16_t>(7+i);result.position=duel_position(actor);return result;
    }
    std::chrono::sys_seconds local_calendar() override {
        trace.push_back("T");return std::chrono::sys_days{std::chrono::year{year}/11/2}+std::chrono::hours{13}+std::chrono::minutes{47}+std::chrono::seconds{59};
    }
    void send_duel_log(const net::Packet& packet) override {trace.push_back("G"+hex(packet));}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected duel attack oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open duel attack oracle");
        const auto definitions=ItemDefinitions::load(item_definitions());const EnchantValues enchant;const MobAttackTemplates mobs;
        const auto* definition=definitions.find(16,1);require(definition!=nullptr,"Missing item fixture");
        std::string line;unsigned int count=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;
            std::istringstream row(line);unsigned int op=0,peer=0,fill=0;std::int32_t mode=0;World world;
            row>>op>>mode>>peer>>world.available>>world.allocate>>world.year>>fill;
            Scene first_scene(9001,world.trace),second_scene(9002,world.trace);
            Actor first(9001,{}, {100,0,0},{},first_scene,{definitions,enchant,mobs}),second(9002,{}, {100,0,0},{},second_scene,{definitions,enchant,mobs});world.actors={&first,&second};
            for(unsigned int i=0;i<2;++i) {
                auto& actor=*world.actors[i];actor.relations().duel_state=i==0?mode:4;actor.relations().duel_type=static_cast<std::int32_t>(2+i);actor.relations().duel_opponent=i==0?peer:9001;
                actor.status().current_target_type=static_cast<std::int32_t>(1+i);actor.status().current_target=999+i;
                row>>actor.duel_wager().money;
                for(auto& item:actor.duel_wager().items) {unsigned int slot=0;row>>slot;require(slot<9,"Invalid wager slot");item=slot?&world.items[slot]:nullptr;}
            }
            world.randoms.resize(72);for(auto& value:world.randoms) row>>value;
            for(unsigned int i=0;i<9;++i) {
                auto& drop=world.items[i];drop.item.type=static_cast<std::uint8_t>(10+i);drop.item.type_id=static_cast<std::uint8_t>(20+i);drop.item.count=static_cast<std::uint8_t>(30+i);
                drop.item.quality=static_cast<std::uint16_t>(40+i);drop.item.make_type=std::string(1,static_cast<char>(65+i));drop.definition=definition;
                drop.user_id=88;drop.party_id=77;drop.drop_type=9;drop.drop_id=66;drop.money=55;drop.position={1,2,3};
            }
            net::Packet header;
            if(op==0) {header.assign(100,static_cast<std::uint8_t>(fill));write_game_log_main(header,world.duel_log_subject(first),world.local_calendar());}
            else if(op==1) drop_duel_wager(first,second,world);
            else {require(op==2,"Unknown duel operation");first.leave_duel_on_attack(world);}
            const auto context=" at duel attack vector "+std::to_string(count);std::vector<std::uint32_t> final;
            for(const auto* actor:world.actors) {
                const auto& relation=actor->relations();const auto& status=actor->status();
                for(auto value:{static_cast<std::uint32_t>(relation.duel_state),static_cast<std::uint32_t>(relation.duel_type),relation.duel_opponent,static_cast<std::uint32_t>(status.current_target_type),status.current_target,actor->duel_wager().money}) final.push_back(value);
                for(const auto* drop:actor->duel_wager().items) final.push_back(drop?static_cast<std::uint32_t>(drop-world.items.data()):0);
            }
            for(const auto& drop:world.items) {const auto values=item_state(drop);final.insert(final.end(),values.begin(),values.end());}
            for(std::size_t i=0;i<final.size();++i) {std::uint32_t original=0;row>>original;require(final[i]==original,"Duel attack state differs"+context+" field "+std::to_string(i)+": "+std::to_string(final[i])+" versus "+std::to_string(original));}
            std::size_t events=0;row>>events;require(world.trace.size()==events,"Duel attack event count differs"+context);
            for(const auto& event:world.trace) {std::string original;row>>original;require(event==original,"Duel attack event differs"+context+": "+event+" versus "+original);}
            if(op==0) {std::string original;row>>original;require(hex(header)==original,"Original log header differs"+context);}
            require(static_cast<bool>(row),"Truncated duel attack vector");std::string extra;require(!(row>>extra),"Trailing duel attack data");++count;
        }
        require(count==768,"Duel attack coverage changed");
        std::cout<<count<<" original log/wager/attack-exit cases passed\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
