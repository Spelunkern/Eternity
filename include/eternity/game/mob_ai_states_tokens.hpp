#pragma once
#include <array>
#include <cstdint>
namespace eternity::game {
// ps_game.exe CKToken: all 288 value bytes are owned, including string tails.
struct MobAiToken {
    std::uint32_t function_id=0,dword=0;
    std::int32_t param_count=0,opcode=0,integer=0;
    float real=0;
    std::int32_t type=0,number_type=1;
    std::array<char,128> value{},text{};
};
// CKToken constructor 004a89d0 initializes only these four fields. All other
// bytes are retained; fresh native zero defaults for other fields are not original stack bytes.
void initialize_mob_ai_token(MobAiToken&) noexcept;
}
