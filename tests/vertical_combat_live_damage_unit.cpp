#include "eternity/game/vertical_combat_live_damage.hpp"
#include "eternity/game/item_pool.hpp"
#include "equipment_fixture.hpp"
#include <functional>
#include <bit>
#include <fstream>
#include <sstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
struct Scene:ActorScene {
 void send_self(const net::Packet&) noexcept override{} void send_cell_view(const net::Packet&) override{} void release_effect_id(std::uint32_t) noexcept override{}
 std::optional<bool> party_kind() const noexcept override{return {};} void send_nearby(const net::Packet&) override{} void send_party(const net::Packet&) noexcept override{} void reset_damage() override{}
 std::optional<std::uint32_t> allocate_effect_id() noexcept override{return {};} std::uint32_t tick_ms() const noexcept override{return 0;}
 void stop_target_mob(std::uint32_t) override{} void taunt(std::uint16_t,float) override{} ZoneUsers* user_zone() override{return nullptr;} UserShapeSocial user_shape_social() override{return {};}
 void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override{}
};
}
struct Pool:MobDamagePoolServices {std::string* events=nullptr;std::function<void()> callback;std::unique_ptr<MobDamageRecord> construct()override{if(events)*events+='A';if(callback)callback();return std::make_unique<MobDamageRecord>();}};
struct Ports:VerticalCombatLiveDamageServices {
 std::string events;std::uint32_t logged=0;bool delay=false,logging=false;std::function<void(MobRuntime&)> event_callback;std::function<void(MobRuntime&,std::uint32_t)> log_callback;
 std::optional<std::uint32_t> party_id(const Actor&)override{return {};}std::int32_t aggro_bonus(const Actor&)override{return 0;}
 void death(MobRuntime&,std::int32_t,std::uint32_t)override{throw std::logic_error("unused");}
 bool long_corpse_delay(const MobRuntime&)override{return delay;}bool damage_logging_enabled(const MobRuntime&)override{return logging;}
 void dispatch_death_event(MobRuntime& m)override{events+='E';if(event_callback)event_callback(m);}std::uint32_t tick_ms()override{events+='T';return 0xfffffff0;}
 void log_damage(MobRuntime& m,std::uint32_t d)override{events+='L';logged=d;if(log_callback)log_callback(m,d);}void retarget(MobRuntime&,bool stacked)override{events+=stacked?'X':'R';}
};
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("fixture required");std::ifstream in(argv[1]);if(!in)throw std::runtime_error("fixture missing");auto items=ItemDefinitions::load(item_definitions());EnchantValues enchant;MobAttackTemplates templates;ActorEquipmentRules rules{items,enchant,templates};Scene scene;Actor actor(42,{}, {},{},scene,rules);
 std::string line;unsigned count=0;while(std::getline(in,line)){
  if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned mode,status,flags,result;std::uint32_t hp,mp,sp,d0,d1,d2,bonus,base,ohp,omp,osp,hits,state,secondary,tick;std::string events;
  row>>mode>>status>>hp>>mp>>sp>>flags>>d0>>d1>>d2>>bonus>>base>>result>>ohp>>omp>>osp>>hits>>state>>secondary>>tick>>events;if(!row)throw std::runtime_error("fixture parse");if(mode!=1)continue;
  Pool allocator;MobDamagePool pool(allocator);MobRuntime mob;mob.damage=std::make_unique<MobDamageLedger>(pool);mob.hp=hp;mob.mp=mp;mob.sp=sp;mob.status=(flags&2)?7:status;if(flags&2)mob.state_stack=std::vector<std::int32_t>{static_cast<std::int32_t>(status)};mob.check_attack_count=flags&1;mob.attacked_count=-1;mob.target_kind=8;mob.target_id=77;
  Ports ports;ports.delay=flags&4;ports.logging=(flags&8)||((flags&16)&&(flags&32));allocator.events=&ports.events;std::array value{std::bit_cast<std::int32_t>(d0),std::bit_cast<std::int32_t>(d1),std::bit_cast<std::int32_t>(d2)};MobCombatCredit credit{1,42,77,std::bit_cast<std::int32_t>(bonus),std::bit_cast<std::int32_t>(base)};
  bool actual=ports.damage(actor,mob,value,credit);if(events=="-")events.clear();bool ok=actual==bool(result)&&mob.hp==ohp&&mob.mp==omp&&mob.sp==osp&&static_cast<std::uint32_t>(mob.attacked_count)==hits&&static_cast<std::uint32_t>(mob.target_kind)==secondary&&mob.target_id==77&&mob.death_time==tick&&ports.events==events;
  if((flags&2)||state==2)ok=ok&&static_cast<std::uint32_t>(mob.status)==state;if(events.find('L')!=std::string::npos)ok=ok&&ports.logged==d0;
  std::uint32_t kind,id,damage,aggro,party;if(row>>kind>>id>>damage>>aggro>>party){ok=ok&&mob.damage->records().size()==1;if(!mob.damage->records().empty()){auto& r=*mob.damage->records()[0];ok=ok&&r.type==kind&&r.id==id&&static_cast<std::uint32_t>(r.damage)==damage&&static_cast<std::uint32_t>(r.aggro)==aggro&&r.party==party;}}else ok=ok&&mob.damage->records().empty();
  if(!ok)throw std::runtime_error("original live damage mismatch case "+std::to_string(count)+" events="+ports.events+" expected="+events);++count;
 }
 if(count!=192)throw std::runtime_error("incomplete fixture");
 // Event, log and allocator see and may mutate the canonical mob immediately.
 Pool alloc;MobDamagePool pool(alloc);MobRuntime mob;mob.damage=std::make_unique<MobDamageLedger>(pool);mob.hp=5;mob.mp=50;mob.sp=50;mob.status=1;mob.target_kind=8;mob.state_stack=std::vector<std::int32_t>{1};Ports ports;ports.logging=true;
 std::array<std::int32_t,3> value{5,1,2};ports.event_callback=[&](MobRuntime& live){if(live.hp!=0||live.status!=2||live.target_kind!=8)throw std::runtime_error("event state hidden");live.hp=20;};ports.log_callback=[&](MobRuntime& live,std::uint32_t){if(live.hp!=20||live.target_kind!=0)throw std::runtime_error("log state hidden");value[0]=7;};alloc.callback=[&]{if(mob.hp!=20||mob.mp!=43)throw std::runtime_error("allocator state hidden");mob.hp=0;};
 if(!ports.damage(actor,mob,value,MobCombatCredit{1,42,0,0,0})||mob.hp!=0||mob.damage->records()[0]->damage!=7)throw std::runtime_error("live callback changes lost");
 Pool composed_allocator;MobDamagePool composed_pool(composed_allocator);MobRuntime composed;composed.damage=std::make_unique<MobDamageLedger>(composed_pool);composed.hp=100;Ports composed_ports;std::array<std::int32_t,3> hit{10,0,0};
 if(vertical_combat_hit(actor,composed,nullptr,hit,hit,composed_ports)||composed.hp!=90||composed.damage->records().size()!=1||composed.damage->records()[0]->damage!=10)throw std::runtime_error("hit to live damage composition");
 std::cout<<count<<" original triple damage cases and live callbacks passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
