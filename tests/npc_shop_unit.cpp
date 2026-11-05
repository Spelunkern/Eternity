#include "eternity/game/npc_shop.hpp"
#include "eternity/game/item_pool.hpp"
#include "equipment_fixture.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void check(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(std::span<const std::uint8_t> bytes) {constexpr char digits[]="0123456789abcdef";std::string out;for(auto b:bytes){out+=digits[b>>4];out+=digits[b&15];}return out;}
std::vector<std::uint8_t> unhex(std::string_view text) {std::vector<std::uint8_t> out;for(std::size_t i=0;i+1<text.size();i+=2) out.push_back(static_cast<std::uint8_t>(std::stoul(std::string(text.substr(i,2)),nullptr,16)));return out;}
std::vector<std::string> split(const std::string& text,char separator) {std::vector<std::string> out;std::istringstream stream(text);std::string part;while(std::getline(stream,part,separator)) out.push_back(part);return out;}
struct Scene final:ActorScene {
    std::vector<std::string>* events=nullptr;
    void send_self(const net::Packet& p) noexcept override{events->push_back("S"+hex(p));}
    void send_nearby(const net::Packet&) override{}
    void send_cell_view(const net::Packet&) override{}
    std::optional<bool> party_kind() const noexcept override{return {};}
    void send_party(const net::Packet&) noexcept override{}
    void reset_damage() override{}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override{return 1;}
    void release_effect_id(std::uint32_t) noexcept override{}
    std::uint32_t tick_ms() const noexcept override{return 123;}
    void stop_target_mob(std::uint32_t) override{}
    void taunt(std::uint16_t,float) override{}
    ZoneUsers* user_zone() override{return nullptr;}
    UserShapeSocial user_shape_social() override{return {};}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override{}
};
struct World final:NpcShopWorld {
    std::vector<std::string>& events;GroundItemPool& pool;const NpcDefinition& shop;const ItemDefinition& definition;
    std::optional<NpcShopEntity> npc;bool shop_found=false,definition_found=false,guild_zone=false,guild=false;
    NpcShopBuyRate rate;std::uint32_t interaction=0;
    World(std::vector<std::string>& log,GroundItemPool& items,const NpcDefinition& source,const ItemDefinition& item):events(log),pool(items),shop(source),definition(item) {}
    std::optional<NpcShopEntity> find_zone_npc(Actor&,std::uint32_t) override {return npc;}
    const NpcDefinition* find_shop_npc(std::uint32_t) override {return shop_found?&shop:nullptr;}
    const ItemDefinition* find_shop_item(std::uint8_t,std::uint8_t) override {return definition_found?&definition:nullptr;}
    void cancel_action_exchange(Actor&) override {events.push_back("C");}
    void end_my_shop(Actor&) override {events.push_back("E");}
    bool guild_house_zone(Actor&) override {return guild_zone;}
    std::optional<NpcShopBuyRate> guild_house_buy_rate(Actor&,std::uint16_t id,std::uint8_t type) override {
        if(!guild)return std::nullopt;
        events.push_back("R"+std::to_string(id)+":"+std::to_string(type));return rate;
    }
    GroundItemPool& shop_item_pool() override {return pool;}
    std::uint32_t shop_system_time() override {return 0x1234abcd;}
    std::uint64_t next_shop_item_uid() override {return 0x1122334455667788ULL;}
    void initialize_shop_item_craft(GroundItem&) override {events.push_back("K");}
    void send_shop_database(const net::Packet& p) override {events.push_back("D"+hex(p));}
    std::string shop_item_name(const ItemDefinition& item) override {return item.id==40000?"Other item":"Shop item";}
    void close_shop_connection(Actor&,std::uint32_t reason) override {events.push_back("F"+std::to_string(reason));}
    std::uint32_t shop_interaction_npc_type_id(Actor&) override {return interaction;}
    void send_money_database(const net::Packet& p) override {events.push_back("M"+std::to_string(p[6]|p[7]<<8|p[8]<<16|static_cast<std::uint32_t>(p[9])<<24));}
    GameLogSubject money_log_subject(const Actor&) override {return {};}
    std::chrono::sys_seconds money_local_calendar() override {return {};}
    // The oracle leaves SetGameLogMain's header untouched: compare only the fields after it.
    void send_money_log(const net::Packet& p) override {auto masked=p;std::fill(masked.begin()+2,masked.begin()+74,std::uint8_t{0});events.push_back("L"+hex(masked));}
};
template<class T> T pop(std::vector<std::string>& fields,std::size_t& at) {return static_cast<T>(std::stoull(fields.at(at++)));}
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected the shop oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot open the shop oracle");
        const auto base=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates mobs;
        std::string line;std::size_t checked=0,bought=0,sold=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#')continue;
            auto f=split(line,'\t');std::size_t at=0;
            const auto opcode=pop<std::uint16_t>(f,at);const auto life=pop<std::int32_t>(f,at);const auto bags=pop<std::int32_t>(f,at);const auto money=pop<std::uint32_t>(f,at);
            ItemDefinition definition;definition.stack_count=pop<std::uint8_t>(f,at);definition.buy=pop<std::uint32_t>(f,at);definition.sell=pop<std::uint32_t>(f,at);
            definition.real_type=pop<std::uint8_t>(f,at);definition.type=pop<std::uint8_t>(f,at);definition.type_id=pop<std::uint8_t>(f,at);
            definition.id=50000+static_cast<std::uint32_t>(checked);
            const auto has_npc=pop<int>(f,at);const auto kind=pop<std::uint32_t>(f,at);const auto has_shop=pop<int>(f,at);const auto shop_type=pop<std::uint8_t>(f,at);
            const auto shop_type_id=pop<std::uint16_t>(f,at);const auto shop_items=pop<std::size_t>(f,at);const auto has_definition=pop<int>(f,at);
            const auto guild_zone=pop<int>(f,at);const auto guild=pop<int>(f,at);const auto rate_result=pop<std::uint32_t>(f,at);const auto rate=std::stod(f.at(at++));
            const auto detail=pop<std::uint8_t>(f,at);const auto create=pop<int>(f,at);const auto interaction=pop<std::uint32_t>(f,at);const auto quality=pop<std::uint16_t>(f,at);
            definition.quality={quality,0,0};auto body=unhex(f.at(at++));
            ItemDefinition other=definition;other.id=40000;other.type=static_cast<std::uint8_t>(definition.type%100+1);
            NpcDefinition shop;shop.record[2]=static_cast<std::uint8_t>(shop_type_id);shop.record[3]=static_cast<std::uint8_t>(shop_type_id>>8);shop.shop_type=shop_type;
            shop.shop_items.assign(shop_items,{definition.type,definition.type_id});
            std::vector<std::string> events;Scene scene;scene.events=&events;bool allocate=false;
            GroundItemPool pool(1,[&]()->std::unique_ptr<GroundItem>{return allocate?std::make_unique<GroundItem>():nullptr;});
            Actor actor(9001,{}, {},{},scene,{base,enchant,mobs});actor.account_id()=12345;actor.money()=money;actor.status().skills.max_bags=bags;actor.attack_state().mode=life;
            const auto items=pop<std::size_t>(f,at);allocate=true;
            for(std::size_t i=0;i<items;++i) {
                auto v=split(f.at(at++),':');auto* item=pool.allocate();check(item,"Fixture allocation failed");
                const bool same=v.at(2)=="1";item->definition=same?&definition:&other;item->item.type=same?definition.type:other.type;item->item.type_id=definition.type_id;
                item->item.count=static_cast<std::uint8_t>(std::stoul(v.at(3)));item->item.uid=std::stoull(v.at(4));const auto gems=unhex(v.at(5));std::copy(gems.begin(),gems.end(),item->item.gems.begin());
                item->item.made_at=static_cast<std::uint32_t>(std::stoul(v.at(6)));if(v.at(7)!="-")set_ground_item_craft(*item,v.at(7));item->item.quality=static_cast<std::uint16_t>(std::stoul(v.at(8)));
                check(actor.claim_inventory_item(static_cast<std::uint8_t>(std::stoul(v.at(0))),static_cast<std::uint16_t>(std::stoul(v.at(1))),*item,pool),"Fixture claim failed");
            }
            allocate=create!=0;
            World world(events,pool,shop,definition);if(has_npc)world.npc=NpcShopEntity{kind,shop_type_id};world.shop_found=has_shop;world.definition_found=has_definition;
            world.guild_zone=guild_zone;world.guild=guild;world.rate={rate_result,rate,detail};world.interaction=interaction;
            net::Packet packet{static_cast<std::uint8_t>(opcode),static_cast<std::uint8_t>(opcode>>8)};packet.insert(packet.end(),body.begin(),body.end());
            const auto available=pool.statistics().available;
            const auto result=npc_shop_packet(actor,packet,world);
            const auto context=" at shop vector "+std::to_string(checked);
            // The original frees through its observed port; the native pool records the same release.
            for(std::size_t i=pool.statistics().available;i>available;--i) events.push_back("X");
            std::vector<std::string> expected;if(f.at(at)!="-")expected=split(f.at(at),',');++at;
            for(auto& event:expected) if(event[0]=='X')event="X";
            check(events==expected,"Events differ"+context+":\n native   "+[&]{std::string s;for(auto& e:events)s+=e+",";return s;}()+"\n original "+f.at(at-1));
            check(actor.money()==pop<std::uint32_t>(f,at),"Money differs"+context);
            const auto remaining=pop<std::size_t>(f,at);std::size_t found=0;
            for(int bag=1;bag<=5;++bag) for(std::uint16_t slot=0;slot<24;++slot) {
                const auto held=actor.retained_inventory_item(static_cast<std::uint8_t>(bag),slot);if(!held)continue;
                auto v=split(f.at(at+found),':');++found;check(found<=remaining,"Extra native item"+context);
                check(std::stoi(v.at(0))==bag && std::stoi(v.at(1))==slot,"Item position differs"+context);
                const auto& item=held->item->item;
                check(item.count==std::stoul(v.at(2)) && item.uid==std::stoull(v.at(3)) && item.quality==std::stoul(v.at(4)) && item.made_at==std::stoul(v.at(5)),"Item state differs"+context);
                const auto mirror=std::find_if(actor.inventory().begin(),actor.inventory().end(),[&](const auto& row){return row.bag==bag && row.slot==slot;});
                check(mirror!=actor.inventory().end() && mirror->count==item.count && mirror->uid==item.uid,"Item mirror differs"+context);
            }
            check(found==remaining,"Item count differs"+context);
            bought+=result==NpcShopResult::bought;sold+=result==NpcShopResult::sold;++checked;
            for(int bag=1;bag<=5;++bag) for(std::uint16_t slot=0;slot<24;++slot) (void)actor.release_inventory_item(static_cast<std::uint8_t>(bag),slot);
        }
        check(checked==4096,"Shop coverage changed");check(bought>200 && sold>200,"Shop oracle lost its successful paths");
        std::cout<<"NPC shop matched "<<checked<<" original executions ("<<bought<<" purchases, "<<sold<<" sales)\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
