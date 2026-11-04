#pragma once
#include "eternity/game/mob_drops.hpp"
#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace eternity::game {
enum class MobDropSequenceProblem {nonpositive_count,clamped_count,zero_divisor};
class MobDropSequenceSource {
public:
    virtual ~MobDropSequenceSource()=default;
    // Original GetPrivateProfileIntA boundary; preserve all 32 returned bits.
    virtual std::uint32_t integer(std::string_view section,std::string_view key,std::uint32_t fallback)=0;
};
class MobDropSequenceDiagnostics {
public:
    virtual ~MobDropSequenceDiagnostics()=default;
    virtual void problem(bool reload,MobDropSequenceProblem)=0;
};
struct MobDropSequenceConfiguration {
    std::int32_t count=0;
    std::array<MobDropSequence,8> groups{};
    // ps_game.exe 004be520 / 004be6a0. Inactive slots retain their old bytes;
    // neither operation is transactional when a later divisor is zero.
    void initialize(MobDropSequenceSource&,MobDropSequenceDiagnostics&);
    bool reload(MobDropSequenceSource&,MobDropSequenceDiagnostics&);
    void publish(MobDropCatalog&) const;
};
// Native reader for the decimal, unquoted, unique-key INI syntax used by the
// recovered SequenceItemCreate.ini. Ambiguous/unsupported syntax is rejected;
// this does not claim to reproduce every Windows profile parsing edge case.
class MobDropSequenceFile final:public MobDropSequenceSource {
public:
    static MobDropSequenceFile decode(std::string_view);
    static MobDropSequenceFile read(const std::filesystem::path&);
    std::uint32_t integer(std::string_view,std::string_view,std::uint32_t fallback) override;
private:
    std::map<std::pair<std::string,std::string>,std::uint32_t> values_;
};
}
