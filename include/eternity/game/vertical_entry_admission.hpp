#pragma once
#include "eternity/game/vertical_protocol.hpp"
#include "eternity/game/vertical_session_character.hpp"

namespace eternity::game {
// Binds the original 0407 wait-user step to the retained world admission user.
// The caller serializes world, character and services on the same executor and
// keeps all owners alive. Other lifecycle steps remain mandatory services.
// A false result means WaitUser rejected the current state and closed it; the
// original 0407 handler still finishes the remaining delivery steps.
inline bool deliver_vertical_entry_with_admission(CharacterEntryState& state,VerticalSessionCharacter& character,
    WorldAdmission& world,WorldAdmissionUser& user,const SocialCatalog& social,const EntryCatalogs& catalogs,
    ExperienceBounds experience,std::string guild_name,VerticalProtocolServices services) {
    const auto& snapshot=character.snapshot();
    if(user.user_id!=snapshot.core.user_id)
        throw std::invalid_argument("Entry admission user does not match selected character");
    if(!services.lifecycle) throw std::invalid_argument("Incomplete entry admission lifecycle services");
    const auto lifecycle=std::move(services.lifecycle);bool accepted=false,called=false;
    services.lifecycle=[&](CharacterEntryStep step,Actor& actor) {
        if(step==CharacterEntryStep::wait_user) {called=true;accepted=world.wait(user);}
        else lifecycle(step,actor);
    };
    deliver_vertical_entry(state,character.pipeline(),snapshot,social,catalogs,experience,std::move(guild_name),services);
    if(!called) throw std::logic_error("Entry did not invoke world admission");
    return accepted;
}
}
