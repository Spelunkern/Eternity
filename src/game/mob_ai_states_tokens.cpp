#include "eternity/game/mob_ai_states_tokens.hpp"
namespace eternity::game {
void initialize_mob_ai_token(MobAiToken& token) noexcept {
    token.integer=0;
    token.real=0.0F;
    token.dword=0;
    token.number_type=1;
}
}
