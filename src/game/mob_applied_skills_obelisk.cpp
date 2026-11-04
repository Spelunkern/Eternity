#include "eternity/game/mob_applied_skills_obelisk.hpp"
#include <stdexcept>
namespace eternity::game {
MobAppliedSkillsObelisk::MobAppliedSkillsObelisk(MobPool& pool,ZoneMobs& zone,MobInitPorts& init,MobAppliedSkillsObeliskServices& s)
    :pool_(pool),zone_(zone),init_(init),services_(s){
    if(!pool_.has_applied_skills_runtime() || !init.tick || !init.compiled_ai || !init.lua_ai || !s.mob_name || !s.remove_by_id || !s.send_change || !s.add_kill_count || !s.subtract_kill_count || !s.kill_count_status14 || !s.broadcast || !s.game_log || !s.database || !s.update_portal_links)
        throw std::invalid_argument("Obelisk adapter requires bound pool, Init and complete world services");
}
MobRuntime* MobAppliedSkillsObelisk::allocate(bool manager){return manager?pool_.allocate():pool_.allocate_raw();}
void MobAppliedSkillsObelisk::prepare_companion(MobRuntime& m){pool_.prepare_allocated(m);}
void MobAppliedSkillsObelisk::initialize(MobRuntime& m,const MobRuntimeTemplate& definition,MobRespawn& respawn){
    if(!pool_.is_prepared(m))throw std::invalid_argument("Obelisk Init requires a prepared actor from its pool");
    m.definition=&definition;initialize_mob(m,definition.id,&respawn,init_);
}
void MobAppliedSkillsObelisk::enter(MobRuntime& m){
    if(!pool_.is_prepared(m))throw std::invalid_argument("Obelisk entry requires a prepared actor from its pool");
    if(!zone_.enter(m))throw std::runtime_error("Obelisk canonical zone entry rejected position");
}
std::int32_t MobAppliedSkillsObelisk::random(){return init_.random.next();}
std::string_view MobAppliedSkillsObelisk::mob_name(const MobRuntime& m){return services_.mob_name(m);}
void MobAppliedSkillsObelisk::remove_by_id(std::uint32_t id){services_.remove_by_id(id);}
void MobAppliedSkillsObelisk::send_change(std::size_t i){services_.send_change(i);}
void MobAppliedSkillsObelisk::add_kill_count(std::int32_t c,std::int32_t n){services_.add_kill_count(c,n);}
bool MobAppliedSkillsObelisk::kill_count_status14(std::int32_t c){return services_.kill_count_status14(c);}
void MobAppliedSkillsObelisk::subtract_kill_count(std::int32_t c,std::int32_t n){services_.subtract_kill_count(c,n);}
void MobAppliedSkillsObelisk::broadcast(const net::Packet& p){services_.broadcast(p);}
void MobAppliedSkillsObelisk::game_log(const net::Packet& p){services_.game_log(p);}
void MobAppliedSkillsObelisk::database(const net::Packet& p){services_.database(p);}
void MobAppliedSkillsObelisk::update_portal_links(){services_.update_portal_links();}
}
