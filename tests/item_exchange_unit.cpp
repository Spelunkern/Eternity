#include "eternity/game/actor.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const auto& data) {static constexpr char digits[]="0123456789abcdef";std::string result;for(auto byte:data) {result+=digits[byte>>4];result+=digits[byte&15];}return result;}
std::array<ExchangeItemReference,8> offers(const std::string& value) {
    require(value.size()==48,"Invalid offer bytes");std::array<ExchangeItemReference,8> result;
    for(std::size_t i=0;i<8;++i) {result[i].bag=static_cast<std::uint8_t>(std::stoul(value.substr(i*6,2),nullptr,16));result[i].slot=static_cast<std::uint8_t>(std::stoul(value.substr(i*6+2,2),nullptr,16));result[i].count=static_cast<std::uint8_t>(std::stoul(value.substr(i*6+4,2),nullptr,16));}return result;
}
std::string offer_bytes(const ItemExchangeState& value) {net::Packet bytes;for(const auto& item:value.items) {bytes.push_back(item.bag);bytes.push_back(item.slot);bytes.push_back(item.count);}return hex(bytes);}
struct State {
    std::int32_t duel=0,mode=0,duel_mode=0;std::uint32_t opponent=0,money=0,duel_money=0;int peer=-1;unsigned int ready=0,duel_ready=0;std::string items,duel_items;
};
State read(std::istream& input) {State value;input>>value.duel>>value.opponent>>value.mode>>value.peer>>value.money>>value.ready>>value.items>>value.duel_mode>>value.duel_money>>value.duel_ready>>value.duel_items;return value;}
struct Context final:ItemExchangeWorld {
    std::array<Actor*,3> users{};std::vector<std::string> events;unsigned int mutation=0,available=0,match=0;int peer=-1;std::int32_t bag=0,slot=0;bool mutated=false;
    std::string snapshot() const {net::Packet result;for(const auto* actor:users) {result.push_back(actor->exchange().ready);result.push_back(actor->duel_exchange().ready);}for(const auto& item:users[0]->exchange().items) result.push_back(item.bag);for(const auto& item:users[0]->duel_exchange().items) result.push_back(item.bag);return hex(result);}
    void mutate() {
        if(mutated) return;mutated=true;auto& user=*users[0];
        switch(mutation) {
            case 1:user.relations().duel_state=0;break;
            case 2:user.relations().duel_state=5;user.relations().duel_opponent=9003;user.duel_exchange().mode=3;break;
            case 3:user.exchange().peer=users[2];break;
            case 4:user.exchange().items[match].bag=77;break;
            case 5:if(peer>=0) users[static_cast<std::size_t>(peer)]->exchange().ready=255;break;
            case 7:user.duel_exchange().mode=0;break;
        }
    }
    void send(std::uint32_t id,const net::Packet& packet) {events.push_back("S"+std::to_string(id)+":"+hex(packet)+":"+snapshot());mutate();}
    Actor* find_exchange_user(Actor& requester,std::uint32_t id) override {
        require(&requester==users[0],"Exchange lookup used another actor's zone");events.push_back("Q"+std::to_string(id));
        if(mutation==6 && bag>=0 && bag<=255 && slot>=0 && slot<=255) {auto& item=users[0]->duel_exchange().items[7];item.bag=static_cast<std::uint8_t>(bag);item.slot=static_cast<std::uint8_t>(slot);}
        if(id==9001) return users[0];if(id>=9002 && id<=9003 && (available&(1U<<(id-9001)))) return users[id-9001];return nullptr;
    }
};
struct Scene final:ActorScene {
    Context& context;std::uint32_t id;Scene(Context& value,std::uint32_t identity):context(value),id(identity) {}
    void send_self(const net::Packet& packet) noexcept override {context.send(id,packet);}
    void unexpected() {throw std::runtime_error("Unexpected item-exchange scene operation");}
    void send_nearby(const net::Packet&) override {unexpected();}void send_cell_view(const net::Packet&) override {unexpected();}
    std::optional<bool> party_kind() const noexcept override {std::terminate();}void send_party(const net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {unexpected();}std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}void release_effect_id(std::uint32_t) noexcept override {std::terminate();}
    std::uint32_t tick_ms() const noexcept override {std::terminate();}void stop_target_mob(std::uint32_t) override {unexpected();}void taunt(std::uint16_t,float) override {unexpected();}
    ZoneUsers* user_zone() override {unexpected();return nullptr;}UserShapeSocial user_shape_social() override {unexpected();return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {unexpected();}
};
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected item-exchange oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open exchange oracle");const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates mobs;std::string line;unsigned int checked=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);Context world;unsigned int op=0,direct=0;row>>op>>world.bag>>world.slot>>world.mutation>>world.available>>direct;
            std::array<State,3> original;for(auto& value:original) value=read(row);world.peer=original[0].peer;const auto initial=offers(original[0].items);
            for(unsigned int i=0;i<8;++i) if(initial[i].bag==static_cast<std::uint8_t>(world.bag) && initial[i].slot==static_cast<std::uint8_t>(world.slot)) {world.match=i;break;}
            std::array<Scene,3> scenes{Scene{world,9001},Scene{world,9002},Scene{world,9003}};std::array<std::unique_ptr<Actor>,3> actors;
            for(std::size_t i=0;i<3;++i) {actors[i]=std::make_unique<Actor>(static_cast<std::uint32_t>(9001+i),EffectStatus{},PlayerResources{},std::vector<PersistedEffect>{},scenes[i],ActorEquipmentRules{items,enchant,mobs});world.users[i]=actors[i].get();}
            for(std::size_t i=0;i<3;++i) {
                auto& actor=*actors[i];const auto& value=original[i];actor.relations().duel_state=value.duel;actor.relations().duel_opponent=value.opponent;
                auto& trade=actor.exchange();trade.mode=value.mode;trade.peer=value.peer<0?nullptr:world.users[static_cast<std::size_t>(value.peer)];trade.money=value.money;trade.ready=static_cast<std::uint8_t>(value.ready);trade.items=offers(value.items);
                auto& duel=actor.duel_exchange();duel.mode=value.duel_mode;duel.money=value.duel_money;duel.ready=static_cast<std::uint8_t>(value.duel_ready);duel.items=offers(value.duel_items);
            }
            if(op==0) actors[0]->remove_exchange_item(world.bag,world.slot,world);else actors[0]->cancel_exchange_ready(*actors[direct],op==2);
            const auto context=" at item-exchange vector "+std::to_string(checked);
            for(std::size_t i=0;i<3;++i) {
                const auto expected=read(row);const auto& actor=*actors[i];const auto& trade=actor.exchange();const auto& duel=actor.duel_exchange();
                require(actor.relations().duel_state==expected.duel && actor.relations().duel_opponent==expected.opponent,"Duel relation changed"+context);
                require(trade.mode==expected.mode && trade.peer==(expected.peer<0?nullptr:world.users[static_cast<std::size_t>(expected.peer)]) && trade.money==expected.money && trade.ready==expected.ready && offer_bytes(trade)==expected.items,"Trade offer state differs"+context);
                require(duel.mode==expected.duel_mode && duel.money==expected.duel_money && duel.ready==expected.duel_ready && offer_bytes(duel)==expected.duel_items,"Duel offer state differs"+context);
            }
            std::size_t events=0;row>>events;require(events==world.events.size(),"Exchange event count differs"+context+": "+std::to_string(world.events.size())+" != "+std::to_string(events));
            for(const auto& event:world.events) {std::string expected;row>>expected;require(event==expected,"Exchange event order differs"+context+": "+event+" != "+expected);}
            require(static_cast<bool>(row),"Truncated exchange vector");std::string extra;require(!(row>>extra),"Trailing exchange vector data");++checked;
        }
        require(checked==2048,"Exchange oracle coverage changed");std::cout<<checked<<" original exchange reference/readiness cases passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
