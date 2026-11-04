#include "eternity/game/mob_targeting_lifecycle_ordinary.hpp"
#include "eternity/game/zone_mobs.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace eternity::game;
void check(bool ok,const std::string& why) {if(!ok) throw std::runtime_error(why);}
void put(eternity::net::Packet& p,std::uint32_t n,unsigned size=4) {for(unsigned i=0;i<size;++i) p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
std::string hex(const eternity::net::Packet& p) {constexpr char h[]="0123456789abcdef";std::string s;for(auto c:p) {s+=h[c>>4];s+=h[c&15];}return s;}
std::array<std::uint64_t,3> identities{};
PartyRegistryParty* party(unsigned n) {return n?reinterpret_cast<PartyRegistryParty*>(&identities.at(n-1)):nullptr;}
unsigned token(PartyRegistryParty* p) {if(!p) return 0;if(p==party(1)) return 1;if(p==party(2)) return 2;throw std::runtime_error("Unknown party");}
Actor* peer() {return reinterpret_cast<Actor*>(&identities[2]);} // Opaque, never dereferenced by the core.
std::string shot(const MobTargetingOrdinaryDeathState& s) {
    eternity::net::Packet p;
    for(auto v:{static_cast<std::uint32_t>(s.life),static_cast<std::uint32_t>(s.penalty),static_cast<std::uint32_t>(s.amulet),static_cast<std::uint32_t>(s.no_exp_loss),s.soul,s.id,s.pending_move,s.rebirth_deadline,s.rebirth_code,s.current.hp,s.current.mp,s.current.sp}) put(p,v);
    for(auto v:s.kills) put(p,v);put(p,token(s.party),1);put(p,s.in_zone,1);return hex(p);
}
struct Case final:MobTargetingOrdinaryDeathServices {
    unsigned index=0,mutation=0,lock_change=0;bool maximum=false,found=false,mutated=false,reenter=false,entered=false,force=false,player_maximum=false,fail=false;
    std::uint8_t winner=0;std::int32_t kind=0;std::uint32_t source=0,clock=0;MobTargetingOrdinaryDeathState s;MobTargetingOrdinaryDeathScratch scratch;std::vector<std::string> events;
    explicit Case(unsigned n):index(n) {
        s.life=std::array<int,8>{-1,0,1,2,3,0,0,0}[n%8];s.amulet=std::array<int,7>{-1,0,1,2,3,4,0x7fffffff}[(n/8)%7];s.no_exp_loss=static_cast<int>((n/56)%3)-1;s.party=party((n/168)%3);s.in_zone=(n/504)%2;
        if(n>=1536) s.life=0;
        s.penalty=17;s.soul=55;s.id=9001;s.pending_move=77;s.rebirth_deadline=88;s.rebirth_code=99;s.current={111,222,333};s.kills={1,2,3,4};
        maximum=(n/3)%2;winner=std::array<std::uint8_t,3>{0,2,255}[(n/13)%3];found=(n/19)%2;kind=std::array<int,3>{1,2,-1}[(n/5)%3];source=(n/17)%2?9001:999;clock=0xfffffff0U+n*7;mutation=(n/23)%19;lock_change=(n/29)%3;
        const auto seed=static_cast<std::uint8_t>(n%251+1);scratch.log.fill(seed);scratch.failed_max_source=static_cast<std::uint32_t>(seed)*0x01010101U;scratch.notification_opcode=static_cast<std::uint16_t>(seed*257);
        reenter=n>=1536;force=n%2;if(reenter) mutation=0;
    }
    void event(char stage,const std::string& extra="") {events.push_back(std::string(1,stage)+extra+"/"+shot(s));}
    void mutate(char stage) {
        const std::string stages="-XHAMGOFILPERDTVKBU";if(mutated || stages[mutation]!=stage) return;mutated=true;
        if(stage=='K') {s.party=party(lock_change);return;}
        if(stage=='U' && !s.party) {s.party=party(2);return;}
        s.life=7;s.penalty=-1;s.pending_move=0xabcdef01;s.rebirth_deadline=99;s.rebirth_code=88;s.current={101,202,303};s.kills={7,8,9,10};s.in_zone=!s.in_zone;
    }
    void run() {mob_targeting_death_ordinary(s,kind,source,*this,scratch);}
    void cancel_exchange() override {event('X');if(reenter && !entered) {entered=true;if(force) s.life=0;run();return;}mutate('X');}
    void end_shop() override {event('H');mutate('H');}
    void cancel_vehicle() override {event('A');mutate('A');}
    std::optional<DamageRecord> maximum_damage() override {event('M');mutate('M');if(player_maximum) return DamageRecord{1,123,999};if(maximum) return DamageRecord{winner,123,999};return std::nullopt;}
    void prepare_game_log(eternity::net::Packet& p) override {event('G');for(unsigned i=0;i<74;++i) p[i]=static_cast<std::uint8_t>(i*3);mutate('G');}
    std::optional<MobTargetingDeathMobLog> find_mob(std::uint32_t id) override {event('O',std::to_string(id));mutate('O');return found?std::optional<MobTargetingDeathMobLog>{{555,"Skeleton"}}:std::nullopt;}
    Actor* find_user(std::uint32_t id) override {event('F',std::to_string(id));mutate('F');return found?peer():nullptr;}
    std::string user_name(Actor* a) override {check(a==peer(),"Wrong retained user query");return "OtherPlayer";}
    void item_drop_by_user(Actor* a) override {event('I',a?"1":"0");mutate('I');}
    std::uint8_t party_kind(PartyRegistryParty* p) const noexcept override {return static_cast<std::uint8_t>(p==party(1)?(index/7)%3:(index/11)%3);}
    void send_game_log(const eternity::net::Packet& p) override {event('L',hex(p));mutate('L');}
    void pvp_leave_by_death() override {event('P');mutate('P');}
    void clear_effects() override {event('E');mutate('E');}
    void reset_damage() override {event('R');mutate('R');}
    void enter_dead() override {event('D');mutate('D');}
    std::uint32_t tick() override {event('T');mutate('T');return clock;}
    void send_death_view(const eternity::net::Packet& p,std::uint32_t exclude) override {event('V',hex(p)+":"+std::to_string(exclude));mutate('V');}
    void lock_party(PartyRegistryParty* p) override {event('K',std::to_string(token(p)));mutate('K');}
    void unlock_party(PartyRegistryParty* p) noexcept override {event('U',std::to_string(token(p)));mutate('U');}
    void send_party(PartyRegistryParty* p,const eternity::net::Packet& b) override {event('B',std::to_string(token(p))+":"+hex(b)+":9001");if(fail) {mutate('B');throw std::runtime_error("Injected transport failure");}mutate('B');}
    std::optional<std::array<std::uint8_t,32>> npc_name(std::int32_t,std::uint32_t) override {throw std::runtime_error("Unexpected NPC lookup");}
};
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
struct Cells final:ZoneMobServices {
    std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected mob move");}
    void send_cell_view(CellCoordinates,const eternity::net::Packet& p) override {check(p.size()==17,"Unexpected mob entry");}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>) override {throw std::runtime_error("Unexpected mob broadcast");}
    void lua_init(MobRuntime&) override {throw std::runtime_error("Unexpected Lua init");}
};
struct World final:MobTargetingOrdinaryDeathWorld {
    ZoneMobs& zone;PlayerCore core;std::vector<char> steps;std::size_t expected_ledger=1;
    std::int32_t penalty=17;std::uint32_t soul=55,deadline=88,code=99;
    explicit World(ZoneMobs& z):zone(z) {core.character.id=9001;core.character.name="Victim";core.kills={1,2,3,4};}
    std::int32_t& death_penalty(Actor&) noexcept override {return penalty;}
    std::uint32_t& soul_state(Actor&) noexcept override {return soul;}
    std::uint32_t& rebirth_deadline(Actor&) noexcept override {return deadline;}
    std::uint32_t& rebirth_code(Actor&) noexcept override {return code;}
    PartyRegistryParty* party(const Actor&) const noexcept override {return nullptr;}
    const PlayerCore& player(const Actor&) const noexcept override {return core;}
    GameLogSubject subject(const Actor& a) const override {GameLogSubject s;s.character_id=a.id();s.character_name=core.character.name;return s;}
    std::chrono::sys_seconds local_calendar() override {return std::chrono::sys_seconds{};}
    void cancel_exchange(Actor& a) override {check(a.attack_state().mode==1 && a.resources().hp==0 && a.resources().mp==0 && a.resources().sp==0 && soul==0,"General death prefix not published");steps.push_back('X');}
    void end_shop(Actor&) override {steps.push_back('H');}
    void cancel_vehicle(Actor&) override {steps.push_back('A');}
    MobRuntime* find_mob(Actor&,std::uint32_t id) override {check(id==123,"Maximum damage source not used");steps.push_back('O');return zone.find(id);}
    Actor* find_user(Actor&,std::uint32_t) override {throw std::runtime_error("Unexpected user lookup");}
    void item_drop_by_user(Actor&,Actor*) override {throw std::runtime_error("Unexpected player drop");}
    std::uint8_t party_kind(PartyRegistryParty*) const noexcept override {std::terminate();}
    void send_game_log(const eternity::net::Packet& p) override {check(p.size()==110 && p[0]==1 && p[1]==3 && p[74]==43 && p[75]==2 && p[78]=='S',"Real runtime/catalog mob log projection differs");steps.push_back('L');}
    void pvp_leave_by_death(Actor&) override {steps.push_back('P');}
    void reset_damage(Actor& a) override {check(a.damage_ledger().entries().size()==expected_ledger && a.status().casting==0,"Actual ledger/effect owners not composed");steps.push_back('R');MobTargetingNpcDeathWorld::reset_damage(a);}
    void enter_dead(Actor&) override {steps.push_back('D');throw std::runtime_error("Missing actual corpse owner");}
    void send_death_view(Actor&,const eternity::net::Packet&,std::uint32_t) override {throw std::runtime_error("Unexpected death view");}
    void lock_party(PartyRegistryParty*) override {throw std::runtime_error("Unexpected party lock");}
    void unlock_party(PartyRegistryParty*) noexcept override {std::terminate();}
    void send_party(PartyRegistryParty*,Actor&,const eternity::net::Packet&) override {throw std::runtime_error("Unexpected party delivery");}
};
template<class T> constexpr bool reset_available=requires(T& a) {a.reset_damage_ledger();};
template<class T> constexpr bool raw_available=requires(T& a,PlayerResources p) {a.replace_resources_raw(p);};
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected ordinary Death oracle");std::ifstream in(argv[1]);check(in.good(),"Cannot read fixture");std::string line;unsigned count=0;
        while(std::getline(in,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned index=0;char sep=0;std::string final;row>>index>>sep>>final>>sep;check(sep=='|',"Malformed fixture");
            Case c(index);c.run();check(shot(c.s)==final,"Original ordinary Death final state differs at "+std::to_string(index));
            std::vector<std::string> expected;std::string e;while(row>>e) expected.push_back(e);
            if(c.events!=expected) {std::ostringstream out;out<<"Original ordinary trace differs at "<<index<<"\n";for(const auto& v:c.events) out<<v<<' ';throw std::runtime_error(out.str());}
            check(!c.reenter || c.entered,"Reentry was not exercised");++count;
        }
        check(count==1552,"Ordinary coverage changed");
        Case unsupported(0);unsupported.player_maximum=true;bool blocked=false;try {unsupported.run();}catch(const MobTargetingPlayerDeathRequired&) {blocked=true;}
        check(blocked && unsupported.events.size()==4 && unsupported.events.back().starts_with("M/") && unsupported.s.life==1 && unsupported.s.current.hp==0,"Missing player rewards hidden as ordinary success");
        Case fault(168);fault.mutation=17;fault.fail=true;bool threw=false;try {fault.run();}catch(const std::runtime_error&) {threw=true;}
        check(threw && fault.events.back().starts_with("U1/") && fault.s.life==7,"Party failure lost lock/partial state");
        auto catalog=MobTargetingBindingCatalog::load(7,0,{},{});auto raw=catalog.mutable_mob_record(7);raw[0]=std::byte{7};const std::string name="Skeleton";for(unsigned i=0;i<name.size();++i) raw[2+i]=static_cast<std::byte>(name[i]);catalog.publish_mob_record(7);
        MobRuntimeTemplate definition;definition.id=7;MobRuntime mob;mob.id=123;mob.type=555;mob.definition=&definition;Cells cells;ZoneMobs zone(MapGeometry(1),cells);check(zone.enter(mob),"Cannot publish actual mob");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;Scene scene;
        {
            EffectStatus status;status.skills.equipment.base.maximum={100,200,300};Actor actor(9001,status,{100,0,0},{},scene,{items,enchant,attacks});
            check(actor.damage(2,123,100,SkillDefinitions{}),"Actual lethal damage not composed");actor.status().casting=7;actor.status().current_target=77;actor.actions().pending_move=77;
            World w(zone);threw=false;try {mob_targeting_death_ordinary(actor,3,999,catalog,w);}catch(const std::logic_error& e) {threw=std::string(e.what()).starts_with("Missing canonical Actor");}catch(const std::runtime_error& e) {threw=std::string(e.what())=="Missing actual corpse owner";}
            const auto expected=reset_available<Actor>?std::vector<char>({'X','H','A','O','L','P','R','D'}):std::vector<char>({'X','H','A','O','L','P','R'});
            check(threw && w.steps==expected && actor.attack_state().mode==1 && actor.resources().hp==0 && actor.status().current_target==77 && actor.actions().pending_move==77,"Actual Actor/max/catalog/effect prefix or failure publication differs");
            if constexpr(reset_available<Actor>) check(actor.damage_ledger().entries().empty(),"Canonical actual ledger reset not delegated");
            else check(actor.damage_ledger().maximum()->source==123,"Old branch lost actual damage maximum");
        }
        {
            Actor actor(9001,{}, {5,6,7},{},scene,{items,enchant,attacks});actor.status().casting=7;World w(zone);w.expected_ledger=0;threw=false;
            try {mob_targeting_death_ordinary(actor,2,123,catalog,w);}catch(const std::logic_error& e) {threw=std::string(e.what()).starts_with("Missing canonical Actor");}catch(const std::runtime_error& e) {threw=std::string(e.what())=="Missing actual corpse owner";}
            check(threw && actor.attack_state().mode==1 && w.soul==0,"Raw resource owner failure did not retain prefix");
            if constexpr(raw_available<Actor>) check(actor.resources().hp==0 && actor.resources().mp==0 && actor.resources().sp==0 && !w.steps.empty(),"Canonical raw resource API not delegated");
            else check(actor.resources().hp==5 && actor.resources().mp==6 && actor.resources().sp==7 && w.steps.empty(),"Old branch fabricated a resource owner");
        }
        {
            Actor actor(9001,{}, {100,0,0},{},scene,{items,enchant,attacks});actor.damage(1,321,100,SkillDefinitions{});World w(zone);blocked=false;
            try {mob_targeting_death_ordinary(actor,2,123,catalog,w);}catch(const MobTargetingPlayerDeathRequired&) {blocked=true;}
            check(blocked && w.steps==std::vector<char>({'X','H','A'}) && actor.attack_state().mode==1,"Real player maximum was silently treated as mob damage");
        }
        std::cout<<count<<" complete original ordinary/fallback Death traces including 16 reentries; actual Actor/ledger/zone/catalog prefix and explicit unsupported/resource failure checks passed (raw API "<<raw_available<Actor><<", reset API "<<reset_available<Actor><<")\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
