#include "eternity/game/mob_targeting_lifecycle_lose.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/guild_membership.hpp"
#if __has_include("eternity/game/mob_drops_effect_transport.hpp")
#include "eternity/game/mob_drops_effect_transport.hpp"
#define ETERNITY_LOSE_DB 1
#endif
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
void check(bool x,const std::string& s) {if(!x)throw std::runtime_error(s);}
void put(eternity::net::Packet& p,std::uint32_t n,unsigned size=4) {for(unsigned i=0;i<size;++i)p.push_back(static_cast<std::uint8_t>(n>>(8*i)));}
std::string hex(const eternity::net::Packet& p) {std::string s;constexpr char h[]="0123456789abcdef";for(auto n:p){s+=h[n>>4];s+=h[n&15];}return s;}
std::array<std::uint64_t,2> opaque{};
GuildMembership* guild(unsigned n) {return n?reinterpret_cast<GuildMembership*>(&opaque.at(n-1)):nullptr;}
unsigned token(GuildMembership* p) {if(!p)return 0;if(p==guild(1))return 1;if(p==guild(2))return 2;throw std::runtime_error("Unknown guild");}
struct Case final:MobTargetingLoseDeathServices {
    std::array<std::array<std::uint32_t,10>,2> users{};std::array<std::string,2> names{"LongLoserName","W"};
    unsigned index,ordinal=0,mutation,change,current;bool alias,mutated=false,reenter,entered=false,fail=false;
    std::int32_t kind;std::array<std::uint8_t,99> scratch;std::vector<std::string> events;
    explicit Case(unsigned n):index(n),mutation((n/7)%17),change((n/29)%3),current((n/5)%3),alias((n/11)%2),reenter(n>=2048) {
        kind=std::array<int,8>{4,5,4,5,0,3,-1,-2147483647-1}[n%8];
        if(reenter){kind=n%2?5:4;mutation=999;current=1;}
        for(unsigned i=0;i<2;++i) {
            const auto count=std::array<unsigned,6>{0,65534,65535,65536,0xffffffff,1}[(n/3+i)%6];
            users[i]={4+i,8+i,2+i,222+i,333+i,count,count,500+i,600+i,7+i};
        }
        scratch.fill(static_cast<std::uint8_t>(n%251+1));
    }
    unsigned identity(unsigned i) const {return alias?0:i;}
    auto& user(unsigned i){return users[identity(i)];}
    const auto& user(unsigned i) const{return users[identity(i)];}
    std::string shot() const {eternity::net::Packet p;for(unsigned i=0;i<2;++i)for(auto n:user(i))put(p,n);put(p,current,1);return hex(p);}
    void event(char stage,std::string extra) {events.push_back(std::string(1,stage)+extra+"/"+shot());++ordinal;}
    void mutate(char stage) {
        if(mutated||ordinal!=mutation)return;mutated=true;
        if(stage=='K'){current=change;return;}
        if(stage=='U'&&current==0){current=2;return;}
        for(unsigned i=0;i<2;++i){user(i)={7,99,3,303,404,65534+i,100+i,700+i,800+i,9};names[identity(i)]=i==0?"MutatedLoser":"M";}
    }
    bool nested(char stage) {if(reenter&&!entered&&(stage=='W'||stage=='K')){entered=true;run();return true;}return false;}
    void run(){mob_targeting_pvp_lose_death(kind,*this,scratch);}
    void drop_wager(unsigned a,unsigned b) override {event('W',std::to_string(identity(a))+":"+std::to_string(identity(b)));if(!nested('W'))mutate('W');}
    void clear_debuffs(unsigned i) override {event('C',std::to_string(identity(i)));mutate('C');}
    void reset_damage(unsigned i) override {event('R',std::to_string(identity(i)));mutate('R');}
    void reset_duel_fields(unsigned i) override {user(i)[0]=0;user(i)[1]=0xffffffff;user(i)[2]=0;}
    std::uint32_t counter(unsigned i,unsigned f) const noexcept override {return user(i)[f+3];}
    void counter(unsigned i,unsigned f,std::uint32_t n) noexcept override {user(i)[f+3]=n;}
    std::uint32_t billing_id(unsigned i) const noexcept override {return user(i)[7];}
    std::uint32_t character_id(unsigned i) const noexcept override {return user(i)[8];}
    std::string name(unsigned i) const override {return names[identity(i)];}
    void send_database(const eternity::net::Packet& p) override {event('D',hex(p));mutate('D');}
    void send_self(unsigned i,const eternity::net::Packet& p) override {event('S',std::to_string(identity(i))+":"+hex(p));mutate('S');}
    void prepare_log(unsigned i,eternity::net::Packet& p) override {i=identity(i);event('G',std::to_string(i));for(unsigned n=0;n<74;++n)p[n]=static_cast<std::uint8_t>(n*3+i);mutate('G');}
    void send_log(const eternity::net::Packet& p) override {event('L',hex(p));mutate('L');}
    GuildMembership* guild() const noexcept override {return ::guild(current);}
    void lock_guild(GuildMembership* g) override {event('K',std::to_string(token(g)));if(!nested('K'))mutate('K');}
    void unlock_guild(GuildMembership* g) noexcept override {event('U',std::to_string(token(g)));mutate('U');}
    void leave_guild(GuildMembership* g,std::int32_t reason) override {event('V',std::to_string(token(g))+":"+std::to_string(reason));mutate('V');if(fail){current=2;throw std::runtime_error("Injected guild leave failure");}}
};
struct Scene final:ActorScene
#ifdef ETERNITY_LOSE_DB
,ActorEffectDatabase
#endif
{
    std::vector<eternity::net::Packet> self,database;
    std::vector<unsigned> released;
    Actor* observed=nullptr;bool database_order=true;
#ifdef ETERNITY_LOSE_DB
    void send_effect_database(const eternity::net::Packet& p) noexcept override {
        database.push_back(p);
        database_order=database_order && observed && !observed->effects().contains(33) && observed->replica().snapshot().size()==1 && released.empty();
    }
#endif
    void send_self(const eternity::net::Packet& p) noexcept override {self.push_back(p);}
    void send_nearby(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected scene send");}
    void send_cell_view(const eternity::net::Packet&) override {throw std::runtime_error("Unexpected scene send");}
    std::optional<bool> party_kind() const noexcept override {return std::nullopt;}
    void send_party(const eternity::net::Packet&) noexcept override {std::terminate();}
    void reset_damage() override {throw std::runtime_error("Unexpected scene reset");}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {std::terminate();}
    void release_effect_id(std::uint32_t n) noexcept override {released.push_back(n);}
    std::uint32_t tick_ms() const noexcept override {return 123;}
    void stop_target_mob(std::uint32_t) override {throw std::runtime_error("Unexpected target reset");}
    void taunt(std::uint16_t,float) override {throw std::runtime_error("Unexpected taunt");}
    ZoneUsers* user_zone() override {throw std::runtime_error("Unexpected user grid");}
    UserShapeSocial user_shape_social() override {throw std::runtime_error("Unexpected shape");}
    void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override {throw std::runtime_error("Unexpected damage");}
};
struct World final:MobTargetingLoseDeathWorld {
    Actor& loser;Actor& winner;std::array<PlayerCore,2> players;
    GuildUser membership;std::vector<eternity::net::Packet> database,logs;
    std::array<GroundItem,2> items;unsigned allocations=0,entries=0,randoms=0,locks=0,unlocks=0,leaves=0;
    GuildMembership* captured=nullptr;GuildMembership* replacement=nullptr;
    World(Actor& a,Actor& b):loser(a),winner(b) {
        players[0].character.id=100;players[0].character.name="LongLoserName";players[0].kills={0,0,65534,65534};
        players[1].character.id=200;players[1].character.name="W";players[1].kills={0,0,65534,65534};
        membership.character_id=a.id();
    }
    PlayerCore& player(Actor& a) const noexcept override {return const_cast<PlayerCore&>(players[&a==&loser?0:1]);}
    std::uint32_t billing_id(const Actor& a) const noexcept override {return &a==&loser?12345:67890;}
    Actor* find_duel_user(Actor&,std::uint32_t) override {throw std::runtime_error("Unexpected peer lookup");}
    UserPosition duel_position(const Actor&) override {return {1,2,3};}
    std::int32_t drop_random() override {++randoms;return randoms%2?0:500;}
    DuelDropItem* allocate_drop_item() override {return &items.at(allocations++);}
    bool enter_duel_item(Actor&,DuelDropItem& d,bool notify) override {check(notify && d.user_id==winner.id(),"Actual wager recipient differs");++entries;return false;}
    GameLogSubject duel_log_subject(const Actor& a) override {
        const auto& p=players[&a==&loser?0:1];GameLogSubject s;s.account_id=77;s.character_id=p.character.id;s.character_name=p.character.name;s.account_name="Account";return s;
    }
    std::chrono::sys_seconds local_calendar() override {return std::chrono::sys_seconds{};}
    void send_duel_log(const eternity::net::Packet& p) override {logs.push_back(p);}
    void send_database(const eternity::net::Packet& p) override {database.push_back(p);}
    GuildMembership* guild(const Actor& a) const noexcept override {return &a==&loser?membership.guild:nullptr;}
    void lock_guild(GuildMembership* g) override {++locks;captured=g;membership.guild=replacement;}
    void unlock_guild(GuildMembership* g) noexcept override {if(g!=captured)std::terminate();++unlocks;}
    void leave_guild(GuildMembership* g,Actor& a,std::int32_t reason) override {check(g==replacement&&&a==&loser&&reason==3,"Wrong live actual guild");++leaves;membership.guild=nullptr;}
};
template<class T> constexpr bool has_reset=requires(T& a){a.reset_damage_ledger();};
void actual_owners() {
    const ItemDefinitions items;const EnchantValues enchant;const MobAttackTemplates attacks;Scene a,b;
    SkillDefinition debuff;debuff.id=55;debuff.level=1;debuff.effect=3;debuff.detail=70;
    SkillDefinition buff;buff.id=66;buff.level=1;buff.effect=4;
    {
        EffectStatus initial;initial.skills.equipment.base.maximum={100,100,100};
        Actor loser(9001,initial,{100,20,30},{{55,1,6},{66,1,6}},a,{items,enchant,attacks});
        Actor winner(9002,initial,{100,20,30},{},b,{items,enchant,attacks});
        a.observed=&loser;loser.account_id()=12345;
        loser.effects().append(33,{{55,1,6},0,6000,false},debuff);loser.effects().append(34,{{66,1,6},0,6000,false},buff);
        loser.status().chase_id=999;
        loser.damage(1,9002,10,SkillDefinitions{});winner.damage(1,9001,10,SkillDefinitions{});
        loser.attack_state().mode=1;winner.attack_state().mode=0;
        for(auto* user:{&loser,&winner}) {
            user->relations().duel_state=4;user->relations().duel_type=8;user->relations().duel_opponent=777;
            user->status().current_target_type=2;user->status().current_target=888;user->status().casting=7;
        }
        loser.duel_wager().money=99;winner.duel_wager().money=88;
        World world(loser,winner);bool missing=false;
        try {mob_targeting_pvp_lose_death(loser,winner,4,world);}catch(const std::logic_error& e){missing=std::string(e.what()).starts_with("Missing canonical Actor::reset_damage_ledger");if(!missing)throw;}
        check(missing==!has_reset<Actor>,"Canonical reset availability differs");
        check(world.allocations==2&&world.entries==2&&world.randoms==4&&loser.duel_wager().money==0&&winner.duel_wager().money==0,"Actual wager owner/order not composed");
        check(world.items[0].money==88&&world.items[0].drop_id==winner.id()&&world.items[1].money==99&&world.items[1].drop_id==loser.id(),"Actual wager drop identities differ");
        check(!loser.effects().contains(33)&&loser.effects().contains(34)&&loser.status().chase_id==0&&a.released==std::vector<unsigned>{33},"Actual debuff owner not composed");
        check(loser.resources().hp==90&&winner.resources().hp==90&&loser.attack_state().mode==1&&winner.attack_state().mode==0,"PvP loss changed resource/life owner");
        check(loser.status().current_target==888&&loser.relations().duel_opponent==777&&loser.status().casting==7,"PvP loss cleared unrelated state");
#ifdef ETERNITY_LOSE_DB
        check(a.database_order&&a.database==std::vector<eternity::net::Packet>{{2,9,0x39,0x30,0,0,55,0,1}},"Actual debuff 0902 order differs");
#endif
        if constexpr(has_reset<Actor>) {
            check(loser.damage_ledger().entries().empty()&&winner.damage_ledger().entries().empty(),"Actual ledger not reset");
            check(loser.relations().duel_state==0&&winner.relations().duel_state==0&&loser.relations().duel_type==-1&&loser.status().current_target_type==0,"Actual duel fields not reset");
            check(world.players[0].kills[3]==65535&&world.players[1].kills[2]==65535&&world.database.size()==2,"Actual counters/DB update differs");
            check(a.self==std::vector<eternity::net::Packet>{{5,0x24,2}}&&b.self==std::vector<eternity::net::Packet>{{5,0x24,1}}&&world.logs.size()==2,"Actual transport boundary differs");
        } else {
            check(loser.relations().duel_state==4&&loser.status().current_target_type==2&&world.database.empty()&&world.logs.empty()&&a.self.empty(),"Missing reset published later stages");
        }
        // Real existing guild records, without inventing a PvP implementation.
        GuildMemberPorts ports;
        ports.allocate=[](const eternity::net::Packet&)->bool{throw std::runtime_error("Unexpected guild allocation");};
        ports.release=[] {throw std::runtime_error("Unexpected guild release");};
        ports.send=[](GuildUser&,eternity::net::Packet){throw std::runtime_error("Unexpected guild broadcast");};
        GuildMembership first(1,ports),second(2,ports);world.membership.guild=&first;world.replacement=&second;
        mob_targeting_pvp_lose_death(loser,winner,5,world);
        check(world.locks==1&&world.unlocks==1&&world.leaves==1&&world.captured==&first&&!world.membership.guild,"Actual guild captured/live owner differs");
        a.observed=nullptr;
    }
    check(a.released==std::vector<unsigned>({33,34}),"Actual effect lifetime differs");
}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Expected original fixture");std::ifstream f(argv[1]);check(bool(f),"Missing original fixture");std::string line;unsigned count=0;
        while(std::getline(f,line)) {
            if(line.empty()||line[0]=='#')continue;
            std::istringstream row(line);unsigned n;char bar;std::string final;row>>n>>bar>>final>>bar;check(bool(row)&&bar=='|',"Bad fixture row");
            Case c(n);c.run();check(c.shot()==final,"Original final PvP loss state differs at "+std::to_string(n));
            std::vector<std::string> expected;std::string e;while(row>>e)expected.push_back(e);
            if(c.events!=expected){std::ostringstream out;out<<"Original PvP loss trace differs at "<<n<<'\n';for(const auto& v:c.events)out<<v<<' ';throw std::runtime_error(out.str());}
            check(!c.reenter||c.entered,"Nested original coverage missing");++count;
        }
        check(count==2064,"PvP loss fixture coverage differs");
        Case fail(0);fail.kind=5;fail.current=1;fail.mutation=999;fail.fail=true;bool threw=false;
        try{fail.run();}catch(const std::runtime_error&){threw=true;}
        check(threw&&fail.current==2&&fail.events.back().starts_with("U1/"),"Exception leaked captured guild or overwrote live owner");
        actual_owners();
        std::cout<<count<<" complete original PvPChkLoseDeath traces including 16 reentries; actual Actor/wager/effect/guild/counter compositions passed (reset API "<<has_reset<Actor><<")\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
