#pragma once
#include "eternity/game/mob_targeting_bindings.hpp"
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
namespace eternity::game {
enum class MobTargetingReloadKind { mob, skill };
struct MobTargetingReloadLink {
  std::uint32_t token = 0;
  std::uint8_t effect = 0;
  std::uint16_t detail = 0;
};
struct MobTargetingReloadWrite {
  std::uint16_t offset = 0;
  std::uint8_t width = 0;
  bool operator==(const MobTargetingReloadWrite &) const = default;
};
struct MobTargetingReloadResult {
  bool success = false;
  std::vector<MobTargetingReloadWrite> writes;
};
// Required SQL/pool owner boundary. Methods preserve the original lifecycle and
// must adapt SQL errors to statuses, not throw across the reload operation.
// get_data writes its typed value (or leaves destination unchanged for NULL).
struct MobTargetingReloadServices {
  virtual ~MobTargetingReloadServices() = default;
  virtual bool profile() = 0;
  virtual void parse_connection() = 0;
  virtual bool connect() = 0;
  virtual bool allocate() = 0;
  virtual bool query(MobTargetingReloadKind, std::uint16_t requested_id) = 0;
  virtual std::int16_t
  fetch() = 0; // SQL_NO_DATA=100; other statuses enter GetData.
  virtual bool get_data(unsigned column, unsigned length, int c_type,
                        std::span<std::byte> destination) = 0;
  virtual void free_database() = 0;
  virtual void disconnect() = 0;
  virtual void release_pool() = 0;
  virtual void log_error() = 0;
  virtual std::optional<MobTargetingReloadLink>
  skill(std::uint16_t id) const = 0;
  // Lookup uses the RETURNED ROW ID, after cleanup. Mutable stable owner view,
  // not a new allocation. Original 32-bit pointer slots use opaque link tokens
  // in this wire-layout projection; tokens must preserve pointer identity.
  virtual std::span<std::byte> target(MobTargetingReloadKind,
                                      std::uint16_t row_id) = 0;
};
MobTargetingReloadResult mob_targeting_reload(MobTargetingReloadKind,
                                              std::uint16_t requested_id,
                                              std::uint32_t &changed_fields,
                                              MobTargetingReloadServices &);
// Quiescent adapters into existing binding structs; never replace their owner.
// Coordinator must expose mutable stable catalog slots, unavailable through the
// current read-only startup catalog API. Raw records are 160 / 268 bytes.
void mob_targeting_reload_skill_projection(MobTargetingBindingSkill &,
                                           std::span<const std::byte>);
void mob_targeting_reload_definition_projection(
    MobTargetingBindingDefinition &, std::span<const std::byte>,
    const std::function<const MobTargetingBindingSkill *(std::uint32_t token)>
        &);
} // namespace eternity::game
