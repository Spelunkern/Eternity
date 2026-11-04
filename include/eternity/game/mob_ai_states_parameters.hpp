#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>
namespace eternity::game {
// AI_Param: type, integer, float, DWORD, VariableID, char[128]. Existing payload
// fields remain independent: clearing one type must retain every other field.
struct MobAiParameter {
    explicit MobAiParameter(std::int32_t kind):type(kind){}
    std::int32_t type,integer=0;
    float real=0;
    std::uint32_t dword=0,variable_id=0;
    std::array<char,128> text{};
};
struct MobAiVariableDefinition {
    std::int32_t count=0;
    std::vector<MobAiParameter> locals;
};
using MobAiVariableCatalog=std::vector<std::optional<MobAiVariableDefinition>>;
using MobAiVariables=std::map<std::uint32_t,MobAiParameter>;
// AI_Param::AI_Param 0048fa30: retained type/text are not initialized.
void initialize_mob_ai_parameter(MobAiParameter&) noexcept;
// CAIFactory::ClearVariable 004a9840. Catalog contributes only VariableID.
// Original missing insertion leaves type/text indeterminate; native requires
// factory-populated typed slots and rejects missing entries instead of guessing.
// A negative index is an unsupported image out-of-bounds path and fails explicitly.
void clear_mob_ai_variables(MobAiVariables&,const MobAiVariableCatalog&,std::int32_t ai);
}
