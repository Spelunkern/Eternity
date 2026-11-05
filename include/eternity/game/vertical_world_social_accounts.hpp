#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/vertical_world_session_social.hpp"
#include <map>

namespace eternity::game {
// The live db-agent account owner keeps these slots across character selection
// cycles. All operations run on the world strand. Retirement is explicit: the
// caller must first fence account-only DB traffic and drain all database jobs.
class VerticalWorldSocialAccounts final {
public:
    VerticalWorldSocialAccounts(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry)
        :strand_(std::move(strand)),registry_(registry) {}
    VerticalWorldSocialAccounts(const VerticalWorldSocialAccounts&)=delete;
    VerticalWorldSocialAccounts& operator=(const VerticalWorldSocialAccounts&)=delete;
    std::shared_ptr<VerticalWorldAccountSocial> acquire(VerticalWorldSession& session) {
        require_strand();auto owner=registry_.find_account(session.account_id());
        if(owner.get()!=&session || session.closing())throw std::logic_error("Social account requires a live registered session");
        auto found=accounts_.find(session.account_id());if(found!=accounts_.end())return found->second;
        auto account=std::make_shared<VerticalWorldAccountSocial>(session.account_id());accounts_.emplace(session.account_id(),account);return account;
    }
    // False means an active session or external account/mutex lease still
    // exists. An absent account is also not a successful retirement.
    bool retire(std::uint32_t account) {
        require_strand();auto it=accounts_.find(account);
        if(it==accounts_.end() || registry_.find_account(account) || it->second.use_count()!=1)return false;
        accounts_.erase(it);return true;
    }
    // Caller must first stop/fence account-only traffic and drain its jobs.
    // Validate every account before erasing any, so a retained lease cannot
    // cause partial retirement. Empty is already fully retired.
    bool retire_all() {
        require_strand();
        for(const auto& [id,account]:accounts_)if(registry_.find_account(id) || account.use_count()!=1)return false;
        accounts_.clear();return true;
    }
    std::size_t size() const {require_strand();return accounts_.size();}
private:
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Social accounts outside world strand");}
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;
    std::map<std::uint32_t,std::shared_ptr<VerticalWorldAccountSocial>> accounts_;
};
}
