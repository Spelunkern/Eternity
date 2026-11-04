#include "eternity/game/mob_applied_skills_runtime.hpp"
#include "eternity/game/mob_pool.hpp"
#include "eternity/game/obelisk_runtime.hpp"
#include "eternity/game/zone_mobs.hpp"
#include "mob_damage_fixture.hpp"
#include <iostream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool b){if(!b)throw std::runtime_error("Obelisk real raw/manager pool mismatch");}
struct Skills:MobAppliedSkillPoolServices {std::unique_ptr<MobAppliedSkill> construct()override{return std::make_unique<MobAppliedSkill>();}};
struct World:ObeliskRuntimeWorld,MobPoolServices,MobAppliedSkillsRuntimeServices,MobAppliedSkillsCatalogPorts,ZoneMobServices {
    MobDamageFixture damage;MobPool* pool=nullptr;ZoneMobs* zone=nullptr;unsigned& counter;
    std::vector<MobRuntime*> objects;unsigned raw=0,manager=0,prepared=0,times=0,binds=0,logs=0,entries=0,teardowns=0,views=0;
    bool constructing=false,fail=false;MicrosoftRandom rng{17};
    explicit World(unsigned& c):counter(c){}
    std::unique_ptr<MobRuntime> construct()override{if(fail)return {};auto p=std::make_unique<MobRuntime>();constructing=true;p->drop_random=ServerRandom(unix_time());constructing=false;objects.push_back(p.get());return p;}
    std::uint32_t unix_time()override{++times;return 0x12345678;}
    MobDamagePool& damage_pool()override{return damage.pool;}
    void release_compiled_ai(MobRuntime& m)override{check(m.has_applied_skills() && m.damage);++teardowns;}
    void release_stack_ai(MobRuntime&)override{throw std::runtime_error("Unexpected stack provider");}
    void close_lua(void*)override{throw std::runtime_error("Unexpected VM provider");}
    MobAppliedSkillsCatalogPorts& ports(MobRuntime&)override{++binds;return *this;}
    std::string_view mob_name(const MobRuntime&)override{return "Obelisk";}
    MobRuntime* allocate(bool create)override{
        const auto old=counter,clock=times;if(create)++manager;else ++raw;
        auto* m=create?pool->allocate():pool->allocate_raw();
        if(m){check(m->has_applied_skills() && m->damage);if(create)check(counter==old+1U && m->drop_random.seed()==(0x12345678U^m->id));else check(counter==old);}
        else check(counter==old && times==clock);return m;
    }
    void prepare_companion(MobRuntime& m)override{const auto old=counter,clock=times;pool->prepare_allocated(m);++prepared;check(counter==old+1U && times==clock+1 && m.id==counter && m.drop_random.seed()==(0x12345678U^m.id));}
    void initialize(MobRuntime& m,const MobRuntimeTemplate& d,MobRespawn& r)override{
        check(m.id==counter && m.drop_random.seed()==(0x12345678U^m.id));m.definition=&d;
        MobInitPorts ports{rng,[]{return 77U;},[](MobRuntime&,std::uint8_t){throw std::runtime_error("Unexpected compiled backend");return false;},[](MobRuntime&){throw std::runtime_error("Unexpected Lua backend");}};
        initialize_mob(m,d.id,&r,ports);
    }
    void enter(MobRuntime& m)override{check(zone->enter(m));check(zone->find(m.id)==&m);++entries;}
    std::int32_t random()override{return rng.next();}
    std::uint32_t tick_ms()override{return 77;}
    void send_cell_view(CellCoordinates,const eternity::net::Packet& p)override{check(p.size()>2 && p[0]==1 && p[1]==6);++views;}
    std::unique_ptr<MobBroadcast> allocate_broadcast(const eternity::net::Packet&,std::optional<std::uint8_t>)override{throw std::runtime_error("Unexpected broadcast");}
    void lua_init(MobRuntime&)override{throw std::runtime_error("Unexpected Lua entry");}
    void game_log(const eternity::net::Packet& p)override{check(p[0]==2 && p[1]==10);++logs;}
    void remove_by_id(std::uint32_t)override{throw std::runtime_error("Unexpected removal");}
    void send_change(std::size_t)override{throw std::runtime_error("Unexpected publication");}
    void add_kill_count(std::int32_t,std::int32_t)override{throw std::runtime_error("Unexpected KC");}
    bool kill_count_status14(std::int32_t)override{throw std::runtime_error("Unexpected KC");}
    void subtract_kill_count(std::int32_t,std::int32_t)override{throw std::runtime_error("Unexpected KC");}
    void broadcast(const eternity::net::Packet&)override{throw std::runtime_error("Unexpected publication");}
    void database(const eternity::net::Packet&)override{throw std::runtime_error("Unexpected DB");}
    void update_portal_links()override{throw std::runtime_error("Unexpected portal");}
    std::uint32_t tick()noexcept override{return 77;}
    void log(const MobAppliedSkill&,std::string_view,std::uint8_t)noexcept override{std::terminate();}
    void rebuild_attack(MobRuntime&)noexcept override{std::terminate();}
    void send_speed(MobRuntime&)noexcept override{std::terminate();}
    bool damage_periodic(MobRuntime&,std::uint32_t,std::uint32_t,MobAppliedSkillDamage&)noexcept override{std::terminate();}
    bool damage_hp(MobRuntime&,std::uint32_t,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void death(MobRuntime&,std::uint32_t,std::uint32_t)noexcept override{std::terminate();}
    void send_result(MobRuntime&,const MobAppliedSkillDamagePacket&,float,std::uint8_t)noexcept override{std::terminate();}
};
}
int main(){try{for(unsigned n=0;n<64;++n){
    auto catalog=MobTargetingBindingCatalog::load(0,1,{},{});Skills factory;MobAppliedSkillPool skills(factory);std::atomic<std::uint32_t> skill_ids{0};unsigned ids=n&1?0xfffffffd:100;
    World world(ids);world.rng=MicrosoftRandom(n*17+1);MobAppliedSkillsRuntime runtime(catalog,skills,skill_ids,world);
    MapDefinition geometry;geometry.size=80;MobRuntimeTemplate definition;definition.id=1;definition.hp=1000;definition.level=1;
    const unsigned count=n%4,total=1+count;ObeliskRuntime o;o.country=static_cast<int>((n/4)%3);o.definitions.fill(&definition);o.center={20,0,20};o.spawns.resize(1);auto& spawn=o.spawns[0];spawn.count=static_cast<int>(count);spawn.members.resize(count);spawn.center={30,0,30};spawn.radius=1;spawn.radius_twice=2;spawn.definitions.fill(&definition);
    std::vector<std::uint8_t> blocked(800,0);ObeliskRuntimeMap map{true,false,1,80,blocked};
    {
        MobPool pool(ids,world,runtime);world.pool=&pool;ZoneMobs zone(geometry.geometry(),world);world.zone=&zone;
        const auto initial=ids;
        if(n&32){map.zone_present=false;initialize_obelisks(map,std::span<ObeliskRuntime>(&o,1),world);check(pool.size()==0 && world.raw==0 && ids==initial);map.zone_present=true;}
        if(n&16){world.fail=true;initialize_obelisks(map,std::span<ObeliskRuntime>(&o,1),world);check(pool.size()==0 && o.respawn.current==1 && spawn.respawn.current==0 && world.times==0 && ids==initial);world.fail=false;}
        initialize_obelisks(map,std::span<ObeliskRuntime>(&o,1),world);
        check(ids==initial+total && pool.size()==total && zone.size()==total && world.raw==total+unsigned((n&16)!=0) && world.manager==0 && world.prepared==total && world.logs==0 && world.times==2*total);
        check(pool.construction_attempts()==total+unsigned((n&16)!=0));auto* principal=o.principal;auto* bridge=&principal->applied_skills();check(o.respawn.current==1 && spawn.respawn.current==static_cast<int>(count));
        // Actual leave/release retain the actors and their bound owners. Complete
        // respawn must reuse them, preparing principal only in manager allocate.
        auto registered=zone.all();for(auto* m:registered){check(zone.leave(*m));pool.release(*m);}o.principal=nullptr;spawn.members.assign(count,nullptr);o.respawn.current=0;spawn.respawn.current=0;
        world.fail=true;const auto before=ids;respawn_obelisk(map,o,world);check(ids==before+total && pool.size()==total && zone.size()==total && world.manager==1 && world.raw==total+count+unsigned((n&16)!=0) && world.prepared==total+count && world.logs==1 && world.times==3*total && world.binds==total);
        for(auto* m:zone.all())check(m->drop_random.seed()==(0x12345678U^m->id) && m->has_applied_skills());check(&principal->applied_skills()==bridge);
        for(auto* m:zone.all()){check(zone.leave(*m));pool.release(*m);}o.principal=nullptr;spawn.members.assign(count,nullptr);
        // Guard host misuse without extra manager state writes or time samples.
        auto* raw=pool.allocate_raw();auto old=ids,clock=world.times;check(raw);check(zone.enter(*raw));bool attached_rejected=false;try{pool.prepare_allocated(*raw);}catch(const std::invalid_argument&){attached_rejected=true;}check(attached_rejected && ids==old && world.times==clock);check(zone.leave(*raw));pool.prepare_allocated(*raw);check(ids==old+1U && world.times==clock+1);old=ids;clock=world.times;
        bool rejected=false;try{pool.prepare_allocated(*raw);}catch(const std::invalid_argument&){rejected=true;}check(rejected && ids==old && world.times==clock);
        pool.release(*raw);rejected=false;try{pool.prepare_allocated(*raw);}catch(const std::invalid_argument&){rejected=true;}check(rejected && ids==old && world.times==clock);
        MobRuntime foreign;rejected=false;try{pool.prepare_allocated(foreign);}catch(const std::invalid_argument&){rejected=true;}check(rejected && ids==old && world.times==clock);
    }
    check(world.binds==total && world.entries==2*total && world.teardowns==3*total+1 && skill_ids==0);
}std::cout<<"64 actual obelisk startup/respawn/raw-manager pool/Init/zone/bound-destructor compositions matched\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
