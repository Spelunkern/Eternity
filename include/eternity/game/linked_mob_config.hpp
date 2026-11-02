#pragma once
#include "eternity/game/linked_mobs.hpp"
#include "eternity/game/map_config.hpp"

namespace eternity::game {
// Linked-group portion of CMap::LoadObelisk 00427300. Other sections retain
// parser context but their obelisk/portal-link owners remain separate work.
// Maps and mob definitions must outlive this catalog and its zone copies.
class LinkedMobConfiguration {
public:
    static LinkedMobConfiguration decode(std::string_view,std::span<const ConfiguredMap>,const MobRuntimeTemplates&);
    static LinkedMobConfiguration load(const std::filesystem::path&,std::span<const ConfiguredMap>,const MobRuntimeTemplates&);
    LinkedMobConfiguration(LinkedMobConfiguration&&)=default;
    LinkedMobConfiguration& operator=(LinkedMobConfiguration&&)=default;
    LinkedMobConfiguration(const LinkedMobConfiguration&)=delete;
    std::span<const LinkedMobGroup> for_map(std::uint16_t id) const noexcept;
private:
    LinkedMobConfiguration()=default;
    std::vector<std::vector<LinkedMobGroup>> maps_;
};
}
