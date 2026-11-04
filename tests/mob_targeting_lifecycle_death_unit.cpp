#include "eternity/game/mob_targeting_lifecycle_death.hpp"
#if __has_include("eternity/game/mob_drops_effect_transport.hpp")
#include "eternity/game/mob_drops_effect_transport.hpp"
#define ETERNITY_LIFECYCLE_EFFECT_DB 1
#endif
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool ok,const std::string& why) {if(!ok) throw std::runtime_error(why);}
void put(eternity::net::Packet& p,std::uint32_t n,unsigned size=4) {for(unsigned i=0;i<size;++i) p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
std::string hex(const eternity::net::Packet& p) {constexpr char h[]="0123456789abcdef";std::string s;for(auto c:p) {s+=h[c>>4];s+=h[c&15];}return s;}
// Opaque stable identities only: core never dereferences a party. Production
// ports receive actual retained PartyRegistryParty objects, not these tokens.
std::array<std::uint64_t,2> identities{};
PartyRegistryParty* party(unsigned n) {return n?reinterpret_cast<PartyRegistryParty*>(&identities.at(n-1)):nullptr;}
unsigned token(PartyRegistryParty* p) {if(!p) return 0;if(p==party(1)) return 1;if(p==party(2)) return 2;throw std::runtime_error("Unknown party identity");}
std::string shot(const MobTargetingDeathState& s) {
    eternity::net::Packet p;
    for(auto v:{static_cast<std::uint32_t>(s.life),static_cast<std::uint32_t>(s.penalty),static_cast<std::uint32_t>(s.amulet),static_cast<std::uint32_t>(s.no_exp_loss),s.soul,s.id,s.pending_move,s.rebirth_deadline,s.rebirth_code,s.current.hp,s.current.mp,s.current.sp}) put(p,v);
    put(p,token(s.party),1);put(p,s.in_zone,1);return hex(p);
}
struct Case final:MobTargetingNpcDeathServices {
    unsigned index=0,mutation=0,lock_change=0;bool npc=false,mutated=false,reenter=false,entered=false,force=false,fail=false;
    std::uint32_t clock=0;std::int32_t type=0;std::uint32_t id=0;
    MobTargetingDeathState s;std::array<std::uint8_t,110> scratch;std::vector<std::string> events;
    explicit Case(unsigned n):index(n) {
        s.life=std::array<int,8>{-1,0,1,2,3,0,0,0}[n%8];s.amulet=std::array<int,7>{-1,0,1,2,3,4,0x7fffffff}[(n/8)%7];s.no_exp_loss=static_cast<int>((n/56)%3)-1;s.party=party((n/168)%3);s.in_zone=(n/504)%2;
        if(n>=2048) {s.life=0;s.party=nullptr;s.in_zone=true;}
        s.penalty=17;s.soul=55;s.id=9001;s.pending_move=77;s.rebirth_deadline=88;s.rebirth_code=99;s.current={111,222,333};
        npc=(n/3)%2;clock=0xfffffff0U+n*7;mutation=(n/5)%13;lock_change=(n/11)%3;type=n%13+1;id=0x10000+n;
        scratch.fill(static_cast<std::uint8_t>(n%251+1));reenter=n>=2048;force=n%2;if(reenter) mutation=0;
    }
    void event(char stage,const std::string& extra="") {events.push_back(std::string(1,stage)+extra+"/"+shot(s));}
    void mutate(char stage) {
        const auto selected=std::array<char,13>{'-','G','N','L','P','E','R','D','T','V','K','B','U'}[mutation];
        if(mutated || selected!=stage) return;mutated=true;
        if(stage=='K') {s.party=party(lock_change);return;}
        if(stage=='U' && !s.party) {s.party=party(2);return;}
        s.life=7;s.penalty=-1;s.pending_move=0xabcdef01;s.rebirth_deadline=99;s.rebirth_code=88;s.current={101,202,303};s.in_zone=!s.in_zone;
    }
    void run() {mob_targeting_death_by_npc(s,type,id,*this,scratch);}
    void prepare_game_log(eternity::net::Packet& p) override {
        event('G');for(unsigned i=0;i<74;++i) p[i]=static_cast<std::uint8_t>(i*3);
        if(reenter && !entered) {entered=true;if(force) s.life=0;mob_targeting_death_by_npc(s,type,id,*this,scratch);return;}
        mutate('G');
    }
    std::optional<std::array<std::uint8_t,32>> npc_name(std::int32_t t,std::uint32_t n) override {
        event('N',std::to_string(t)+":"+std::to_string(n));mutate('N');if(!npc) return std::nullopt;
        std::array<std::uint8_t,32> name{};
        if(index%2) {name.fill('X');const std::string prefix="Guardian";std::copy(prefix.begin(),prefix.end(),name.begin());name[8]=0;}
        else for(unsigned i=0;i<32;++i) name[i]=static_cast<std::uint8_t>('A'+i%26);
        return name;
    }
    void send_game_log(const eternity::net::Packet& p) override {event('L',hex(p));mutate('L');}
    void pvp_leave_by_death() override {event('P');mutate('P');}
    void clear_effects() override {event('E');mutate('E');}
    void reset_damage() override {event('R');mutate('R');}
    void enter_dead() override {event('D');mutate('D');}
    std::uint32_t tick() override {event('T');mutate('T');return clock;}
    void send_death_view(const eternity::net::Packet& p,std::uint32_t exclude) override {event('V',hex(p)+":"+std::to_string(exclude));mutate('V');}
    void lock_party(PartyRegistryParty* p) override {event('K',std::to_string(token(p)));mutate('K');}
    void unlock_party(PartyRegistryParty* p) noexcept override {event('U',std::to_string(token(p)));mutate('U');}
    void send_party(PartyRegistryParty* p,const eternity::net::Packet& b) override {event('B',std::to_string(token(p))+":"+hex(b)+":9001");if(fail) throw std::runtime_error("Injected party transport failure");mutate('B');}
};
struct Scene final:ActorScene
#ifdef ETERNITY_LIFECYCLE_EFFECT_DB
    ,ActorEffectDatabase
#endif
{
    std::vector<unsigned> released;
    Actor* observed=nullptr;bool database_order=true;
    std::vector<eternity::net::Packet> database;
#ifdef ETERNITY_LIFECYCLE_EFFECT_DB
    void send_effect_database(const eternity::net::Packet& p) noexcept override {
        database.push_back(p);
        database_order=database_order && observed && observed->replica().snapshot().size()==1 && !observed->effects().contains(33) && released.empty();
    }
#endif
    void send_self(const eternity::net::Packet&) noexcept override {std::terminate();}
    void send_nearby(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected proximity delivery");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected cell delivery");}
    std::optional<bool> party_kind() const noexcept override {return std::nullopt;}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::runtime_error("Unexpected scene reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t id) noexcept override {released.push_back(id);}
    std::uint32_t tick_ms() const noexcept override {return 123;}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected target stop");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected taunt");}
    ZoneUsers* user_zone() override {throw std::runtime_error("Unexpected grid");}
    UserShapeSocial user_shape_social() override {throw std::runtime_error("Unexpected shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected damage");}
};
struct World:MobTargetingNpcDeathWorld {
    // Explicit projections for currently unexposed lifecycle fields, NOT a
    // production shadow Actor owner. Tests stop at the missing ledger accessor.
    std::int32_t penalty=17;std::uint32_t soul=55,deadline=88,code=99;std::vector<char> steps;
    std::int32_t& death_penalty(Actor&) noexcept override {return penalty;}
    std::uint32_t& soul_state(Actor&) noexcept override {return soul;}
    std::uint32_t& rebirth_deadline(Actor&) noexcept override {return deadline;}
    std::uint32_t& rebirth_code(Actor&) noexcept override {return code;}
    PartyRegistryParty* party(const Actor&) const noexcept override {return nullptr;}
    GameLogSubject subject(const Actor& a) const override {GameLogSubject s;s.character_id=a.id();return s;}
    std::chrono::sys_seconds local_calendar() override {return std::chrono::sys_seconds{};}
    void send_game_log(const eternity::net::Packet& p) override {check(p.size()==110 && p[0]==1 && p[1]==3 && p[74]==1 && p[75]==0 && p[78]=='G' && p[86]==0 && p[107]==0,"Actual NPC catalog/log mapping differs");steps.push_back('L');}
    void pvp_leave_by_death(Actor& a) override {check(a.attack_state().mode==1 && soul==0 && penalty==5 && a.resources().hp==120,"NPC entry publication/resource preservation differs");steps.push_back('P');}
    void reset_damage(Actor& a) override {
        check(!a.effects().contains(33) && a.effects().contains(34) && a.status().casting==0 && a.resources().hp==100,"Actual effect/resource cleanup not composed");
        check(a.damage_ledger().entries().size()==1 && a.damage_ledger().entries()[0].amount==10,"Actual ledger not retained until reset boundary");
        steps.push_back('R');throw std::runtime_error("Missing authoritative Actor ledger reset accessor");
    }
    void enter_dead(Actor&) override {throw std::runtime_error("Corpse publication must not run after reset failure");}
    void send_death_view(Actor&,const eternity::net::Packet&,std::uint32_t) override {throw std::runtime_error("Death view must not run after reset failure");}
    void lock_party(PartyRegistryParty*) override {throw std::runtime_error("Unexpected party lock");}
    void unlock_party(PartyRegistryParty*) noexcept override {std::terminate();}
    void send_party(PartyRegistryParty*,Actor&,const eternity::net::Packet&) override {throw std::runtime_error("Unexpected party delivery");}
};
// Explicit remainder fixture for a *zero-entry* ledger and corpse boundary.
// It is not a production reset/corpse implementation; it isolates the actual
// Actor bridge's exception-unlock semantics without adding another ledger.
struct FailureWorld final:World {
    unsigned locked=0,unlocked=0;
    PartyRegistryParty* party(const Actor&) const noexcept override {return ::party(1);}
    void reset_damage(Actor& a) override {check(a.damage_ledger().entries().empty(),"Failure fixture needs an empty actual ledger");steps.push_back('R');}
    void enter_dead(Actor&) override {steps.push_back('D');}
    void lock_party(PartyRegistryParty* p) override {check(p==::party(1),"Wrong captured lock");++locked;}
    void unlock_party(PartyRegistryParty* p) noexcept override {if(p!=::party(1)) std::terminate();++unlocked;}
    void send_party(PartyRegistryParty* p,Actor& a,const eternity::net::Packet& b) override {
        check(p==::party(1) && a.id()==9001 && b.size()==6 && b[0]==6 && b[1]==12,"Wrong excluded party route");
        a.attack_state().mode=7;a.actions().pending_move=99;penalty=8;soul=9;deadline=111;code=222;
        throw std::runtime_error("Injected actual Actor transport mutation/failure");
    }
};
NpcDefinitions definitions() {
    eternity::net::Packet b;
    for(unsigned group=0;group<13;++group) {
        put(b,group==2?1:0);if(group!=2) continue;
        put(b,3,1);put(b,1,2);for(unsigned i=0;i<4;++i) put(b,0);
        const std::string name="Guardian";put(b,9);b.insert(b.end(),name.begin(),name.end());b.push_back(0);
        put(b,0);put(b,0);put(b,0);
    }
    return NpcDefinitions::decode(b);
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected complete NPC death oracle");std::ifstream in(argv[1]);check(in.good(),"Cannot read oracle");std::string line;unsigned count=0;
        while(std::getline(in,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);unsigned index=0;char sep=0;std::string final;
            row>>index>>sep>>final>>sep;check(sep=='|',"Malformed original fixture");Case c(index);c.run();
            check(shot(c.s)==final,"Original final NPC death state differs at "+std::to_string(index));
            std::vector<std::string> expected;std::string e;while(row>>e) expected.push_back(e);
            if(c.events!=expected) {std::ostringstream out;out<<"Original NPC death trace differs at "<<index<<"\n";for(const auto& v:c.events) out<<v<<' ';throw std::runtime_error(out.str());}
            check(!c.reenter || c.entered,"Original recursion coverage lost");++count;
        }
        check(count==2064,"NPC death coverage changed");
        Case fail(168);fail.mutation=0;fail.fail=true;bool threw=false;try {fail.run();}catch(const std::runtime_error&) {threw=true;}
        check(threw && fail.events.back().starts_with("U1/"),"Captured party lock leaked on native transport exception");
        const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;const auto npcs=definitions();Scene scene;
        SkillDefinition normal;normal.id=55;normal.level=1;normal.effect=4;normal.detail=82;normal.abilities[0]={1,50};
        SkillDefinition keep;keep.id=66;keep.level=1;keep.preserve_on_death=true;
        {
            EffectStatus state;state.skills.equipment.base.maximum={100,200,300};
            Actor actor(9001,state,{130,20,30},{{55,1,6},{66,1,6}},scene,{items,enchant,attacks});
            actor.account_id()=12345;scene.observed=&actor;
#ifdef ETERNITY_LIFECYCLE_EFFECT_DB
            check(actor.effect_database_bound(),"Actual required Actor database transport not bound");
#endif
            actor.effects().append(33,{{55,1,6},0,6000,false},normal);actor.effects().append(34,{{66,1,6},0,6000,false},keep);
            actor.initializing(true);actor.effects().initialize(actor.status(),actor);actor.initializing(false);
            check(actor.status().skills.equipment.base.maximum.hp==150,"Actual effect initialization failed");
            actor.damage(2,99,10,SkillDefinitions{});check(actor.resources().hp==120,"Actual damage path not composed");
            actor.actions().pending_move=77;actor.status().casting=7;actor.status().current_target_type=2;actor.status().current_target=77;
            World w;threw=false;try {mob_targeting_death_by_npc(actor,3,1,npcs,w);}catch(const std::runtime_error& e) {threw=std::string(e.what()).starts_with("Missing authoritative");}
            check(threw && w.steps==std::vector<char>({'L','P','R'}),"Missing real ledger provider was hidden or order changed");
            check(actor.attack_state().mode==1 && actor.resources().hp==100 && actor.resources().mp==20 && actor.resources().sp==30 && actor.actions().pending_move==77 && w.deadline==88 && w.code==99,"Native failure ran later death stages or zeroed resources");
            check(actor.status().current_target==77 && actor.replica().snapshot().size()==1 && scene.released==std::vector<unsigned>{33},"Actual effect owner/replica or target semantics differ");
#ifdef ETERNITY_LIFECYCLE_EFFECT_DB
            check(scene.database_order && scene.database==std::vector<eternity::net::Packet>{{2,9,0x39,0x30,0,0,55,0,1}},"Actual 0902 replica/database/release order differs");
#endif
            mob_targeting_death_by_npc(actor,3,1,npcs,w);check(w.steps.size()==3,"Dead reentry queried missing providers");
        }
        scene.observed=nullptr;
        check(scene.released==std::vector<unsigned>({33,34}),"Actual retained effect release lifetime differs");
#ifdef ETERNITY_LIFECYCLE_EFFECT_DB
        check(scene.database.size()==1,"Shutdown sent an extra database removal");
#endif
        {
            Actor actor(9001,{}, {120,20,30},{},scene,{items,enchant,attacks});FailureWorld w;threw=false;
            try {mob_targeting_death_by_npc(actor,3,1,npcs,w);}catch(const std::runtime_error& e) {threw=std::string(e.what()).starts_with("Injected actual Actor");}
            check(threw && w.locked==1 && w.unlocked==1,"Exception did not release captured lock");
            check(actor.attack_state().mode==7 && actor.actions().pending_move==99 && w.penalty==8 && w.soul==9 && w.deadline==111 && w.code==222,"Exception unlock overwrote live Actor mutations");
        }
        std::cout<<count<<" complete original DeathByNPC/SendDeath traces including 16 reentries; actual Actor/effect/resource/ledger-prefix and native failure checks passed\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
