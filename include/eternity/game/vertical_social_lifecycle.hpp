#pragma once
#include "eternity/game/character_save_lifecycle.hpp"
#include "eternity/game/vertical_social_save.hpp"

namespace eternity::game {
// Own this bridge, the replica, save connection and mutex until all lifecycle
// jobs finish. The same mutex must guard social packet mutation and save jobs.
template<class Database> class VerticalSocialLifecycle {
public:
    VerticalSocialLifecycle(VerticalSocialReplica& social,CharacterReplica& character,
        Database& database,std::recursive_mutex& mutex,std::string prefix="PS_GameData.dbo.")
        :social_(social),character_(character),mutex_(mutex),save_(database,std::move(prefix)) {
        if(social.account_id()!=character.core().user_id)throw std::invalid_argument("Social and character account mismatch");
    }
    bool uses_save_mutex(const std::recursive_mutex& mutex) const noexcept {return &mutex_==&mutex;}
    EntryCatalogUpdate apply(std::span<const std::uint8_t> packet) {
        std::lock_guard lock(mutex_);return social_.apply_db(packet);
    }
    CharacterSaveLifecyclePorts bind(CharacterSaveLifecyclePorts ports) {
        auto diagnostic=ports.diagnostic;
        ports.save_friends=[this,diagnostic] {
            std::lock_guard lock(mutex_);
            if(!save_.friends(social_,character_.core().character.id) && diagnostic)diagnostic("Friend save failed");
        };
        ports.save_blocks=[this,diagnostic] {
            std::lock_guard lock(mutex_);
            if(!save_.blocks(social_,character_.core().character.id) && diagnostic)diagnostic("Block save failed");
        };
        return ports;
    }
private:
    VerticalSocialReplica& social_;CharacterReplica& character_;std::recursive_mutex& mutex_;
    VerticalSocialSave<Database> save_;
};
}
