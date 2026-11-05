#pragma once
#include "eternity/game/vertical_social_selection.hpp"
#include <memory>
#include <mutex>
#include <type_traits>

namespace eternity::game {
// Retain one account owner across character selections and outstanding social
// save jobs. The mutex also guards VerticalSocialLifecycle and DB mutations.
class VerticalWorldAccountSocial : public std::enable_shared_from_this<VerticalWorldAccountSocial> {
public:
    explicit VerticalWorldAccountSocial(std::uint32_t account):replica_(account) {}
    VerticalSocialReplica& replica() noexcept {return replica_;}
    std::recursive_mutex& mutex() noexcept {return mutex_;}
    // Aliasing ownership keeps the replica and mutex alive through DB drain.
    std::shared_ptr<std::recursive_mutex> shared_mutex() {return {shared_from_this(),&mutex_};}
private:
    VerticalSocialReplica replica_;
    std::recursive_mutex mutex_;
};

// The entry catalog belongs to the selected character; the DB replica belongs
// to the retained account. Construct on the world strand after selection.
class VerticalWorldSessionSocial {
public:
    VerticalWorldSessionSocial(std::shared_ptr<VerticalWorldAccountSocial> account,const PlayerSnapshot& snapshot)
        :account_(require_account(std::move(account))),selection_(load(*account_,snapshot)),
         character_(snapshot.core.character.id) {}
    const SocialCatalog& catalog() const {
        if(external_catalog_)return *external_catalog_;
        if(taken_)throw std::logic_error("Social catalog handoff incomplete");
        return selection_.catalog;
    }
    // Transfer once to the real social session before preparation completes.
    // Entry is unavailable until bind_catalog installs that exact owner.
    SocialCatalog take_catalog() {
        if(taken_ || external_catalog_)throw std::logic_error("Social catalog already transferred");
        taken_=true;return std::move(selection_.catalog);
    }
    template<class Owner> void bind_catalog(std::shared_ptr<Owner> owner) {
        static_assert(std::is_same_v<decltype(std::declval<Owner&>().catalog()),const SocialCatalog&>,"Social owner must expose a retained catalog reference");
        if(!taken_ || external_catalog_ || !owner)throw std::logic_error("Invalid social catalog binding");
        const auto& value=owner->catalog();
        if(value.account_id()!=account_->replica().account_id() || value.character_id()!=character_)
            throw std::invalid_argument("Social catalog identity mismatch");
        external_catalog_=&value;catalog_owner_=std::move(owner);
    }
    bool friends_loaded() const noexcept {return selection_.friends_loaded;}
    bool blocks_loaded() const noexcept {return selection_.blocks_loaded;}
    const std::shared_ptr<VerticalWorldAccountSocial>& account() const noexcept {return account_;}
private:
    static std::shared_ptr<VerticalWorldAccountSocial> require_account(std::shared_ptr<VerticalWorldAccountSocial> account) {
        if(!account)throw std::invalid_argument("Missing retained social account");return account;
    }
    static VerticalSocialSelection load(VerticalWorldAccountSocial& account,const PlayerSnapshot& snapshot) {
        std::lock_guard lock(account.mutex());return vertical_load_social_selection(snapshot,account.replica());
    }
    std::shared_ptr<VerticalWorldAccountSocial> account_;
    VerticalSocialSelection selection_;
    std::uint32_t character_;std::shared_ptr<void> catalog_owner_;const SocialCatalog* external_catalog_=nullptr;bool taken_=false;
};
}
