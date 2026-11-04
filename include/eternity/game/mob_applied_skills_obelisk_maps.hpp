#pragma once
#include "eternity/game/mob_applied_skills_obelisk.hpp"
#include "eternity/game/obelisk_runtime_maps.hpp"
#include "eternity/game/obelisk_runtime_death.hpp"
#include "eternity/game/mob_targeting_bindings.hpp"
namespace eternity::game {
struct MobAppliedSkillsObeliskMapsServices {
    std::function<void(std::int32_t,std::int32_t)> add_kill_count,subtract_kill_count;
    std::function<bool(std::int32_t)> kill_count_status14;
    std::function<void(const net::Packet&)> game_log;
    MobRemovalOwners& linked;
};
// Per-primary-zone host bindings over the ONE retained Maps/configuration owner.
// ServerMaps, coherent template/full catalog owners, Maps and its SAME services,
// shared bound pool/Init/RNG/KC/backends remain externally owned and serialized.
// Maps/configuration must outlive the pool, including recycled mobs. Drain actors
// and detach while the primary zone is alive before replacing/destroying it.
class MobAppliedSkillsObeliskMaps {
public:
    MobAppliedSkillsObeliskMaps(ServerMaps&,const MobRuntimeTemplates&,const MobTargetingBindingCatalog&,
        ObeliskRuntimeMaps&,ObeliskRuntimeMapsServices&,MobPool&,MobInitPorts&,
        const std::array<std::int32_t,2>& kill_count_add,MobAppliedSkillsObeliskMapsServices&);
    ~MobAppliedSkillsObeliskMaps();
    MobAppliedSkillsObeliskMaps(const MobAppliedSkillsObeliskMaps&)=delete;
    MobAppliedSkillsObeliskMaps& operator=(const MobAppliedSkillsObeliskMaps&)=delete;
    // False means no current primary zone. True means original coordination
    // was invoked; individual allocation failures retain original partial state.
    bool startup(std::size_t map);
    bool tick(std::size_t map,std::uint32_t now);
    bool reset(std::size_t map,const std::function<std::uint32_t()>&);
    bool remove(std::size_t map,std::uint32_t actor);
    std::optional<std::int32_t> death_country(std::size_t,MobRuntime&,std::int32_t type);
    void destroyed(std::size_t,MobRuntime&,std::int32_t country,std::int32_t type);
    void remove_member(std::size_t,MobRuntime&,void* source);
    void detach_primary(std::size_t);
private:
    struct Binding;
    const ConfiguredMap& configured(std::size_t)const;
    Binding* binding(std::size_t);
    ServerMaps& maps_;const MobRuntimeTemplates& templates_;const MobTargetingBindingCatalog& catalog_;
    ObeliskRuntimeMaps& owner_;ObeliskRuntimeMapsServices& routes_;MobPool& pool_;MobInitPorts& init_;
    const std::array<std::int32_t,2>& kill_count_add_;MobAppliedSkillsObeliskMapsServices& services_;
    std::vector<std::unique_ptr<Binding>> bindings_;
};
}
