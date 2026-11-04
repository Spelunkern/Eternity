#pragma once
#include "eternity/game/mob_damage.hpp"
struct MobDamageFixture final:eternity::game::MobDamagePoolServices {
    eternity::game::MobDamagePool pool{*this};
    std::unique_ptr<eternity::game::MobDamageRecord> construct() override {return std::make_unique<eternity::game::MobDamageRecord>();}
};
