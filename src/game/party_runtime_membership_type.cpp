#include "eternity/game/party_runtime_membership_type.hpp"

namespace eternity::game {
std::uint8_t PartyMembershipWorld::party_type(PartyMembershipUser& user) {
    return party_membership_type(user,*this);
}
std::uint8_t party_membership_type(PartyMembershipUser& user,PartyMembershipWorld& world) {
    auto* retained=user.party;
    if(retained) {
        world.lock(*retained);
        if(!user.party) {
            world.unlock(*retained);
            retained=nullptr;
        }
    }
    auto* current=user.party;
    if(current) {
        world.lock(*current);
        const bool leader=current->count>0 && current->slots[static_cast<std::size_t>(current->leader)]==&user;
        world.unlock(*current);
        if(retained) world.unlock(*retained);
        return leader?2:1;
    }
    if(retained) world.unlock(*retained);
    return 0;
}
}
