#pragma once
#include "eternity/game/obelisk_runtime.hpp"
#include "eternity/game/mob_pool.hpp"
#include "eternity/game/zone_mobs.hpp"
namespace eternity::game {
// Only the remaining existing world boundaries. Every callable is mandatory;
// bind actual retained publication/KC/log/portal/removal owners at construction.
struct MobAppliedSkillsObeliskServices {
    std::function<std::string_view(const MobRuntime&)> mob_name;
    std::function<void(std::uint32_t)> remove_by_id;
    std::function<void(std::size_t)> send_change;
    std::function<void(std::int32_t,std::int32_t)> add_kill_count,subtract_kill_count;
    std::function<bool(std::int32_t)> kill_count_status14;
    std::function<void(const net::Packet&)> broadcast,game_log,database;
    std::function<void()> update_portal_links;
};
// Concrete allocation/Init/EnterMob bridge: no second pool/zone/catalog/list.
// All references and callback backing owners outlive this adapter and all mobs.
// Same serialized executor/RNG as Init, no reload across callbacks. Subject,
// source/configuration and zone storage must survive every synchronous callback;
// defer actual subject destruction/recycling until enclosing owner calls return.
class MobAppliedSkillsObelisk final:public ObeliskRuntimeWorld {
public:
    MobAppliedSkillsObelisk(MobPool&,ZoneMobs&,MobInitPorts&,MobAppliedSkillsObeliskServices&);
    MobRuntime* allocate(bool manager_create)override;
    void prepare_companion(MobRuntime&)override;
    void initialize(MobRuntime&,const MobRuntimeTemplate&,MobRespawn&)override;
    // Throws if canonical entry rejects coordinates. No automatic rollback:
    // source records may already reference the actor and IDs are consumed.
    void enter(MobRuntime&)override;
    std::int32_t random()override;
    std::string_view mob_name(const MobRuntime&)override;
    void remove_by_id(std::uint32_t)override;
    void send_change(std::size_t)override;
    void add_kill_count(std::int32_t,std::int32_t)override;
    bool kill_count_status14(std::int32_t)override;
    void subtract_kill_count(std::int32_t,std::int32_t)override;
    void broadcast(const net::Packet&)override;
    void game_log(const net::Packet&)override;
    void database(const net::Packet&)override;
    void update_portal_links()override;
private:
    MobPool& pool_;ZoneMobs& zone_;MobInitPorts& init_;MobAppliedSkillsObeliskServices& services_;
};
}
