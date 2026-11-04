#include "eternity/game/party_runtime_registry_break.hpp"
namespace eternity::game {
namespace {
PartyMembershipUser*& slot(PartyRegistryParty& party,std::size_t index) {
    return index<party.slots.size()?party.slots[index]:party.extra_slots[index-party.slots.size()];
}
}
bool break_registry_party(PartyRegistry& registry,PartyRegistryParty& party,PartyRegistryPorts& ports,PartyMembershipWorld& world) {
    world.lock(party);
    for(std::int32_t i=0;i<party.count;++i) {
        const auto index=static_cast<std::size_t>(i);
        if(slot(party,index)) {
            slot(party,index)->party=nullptr;
            auto* user=slot(party,index);
            notify_party_membership_scene(*user,world);
            // Original reloads this slot after scene delivery. It can now name
            // a different user; do not keep the earlier scene subject for log.
            log_party_membership(party,*slot(party,index),0xf02,world);
        }
    }
    reset_registry_party(party);
    remove_registry_party(registry,&party,ports);
    world.unlock(party);
    return true;
}
}
