#pragma once
#include "eternity/session/registry.hpp"
#include <stdexcept>

inline std::shared_ptr<eternity::session::Registry> world_claim_registry() {
    static auto registry=std::make_shared<eternity::session::Registry>();
    return registry;
}

inline eternity::session::LoginSession world_claim_fixture(std::uint32_t user_id=11) {
    using namespace eternity::session;
    auto registry=world_claim_registry();
    LoginSession account{}; account.user_id=user_id; account.session_id=1;
    registry->add(account);
    if(!registry->select(user_id,1,0,1)) throw std::logic_error("fixture handoff failed");
    auto claim=registry->claim_world(user_id,account.token,0,1,2);
    if(!claim.session) throw std::logic_error("fixture world claim failed");
    return std::move(*claim.session);
}
