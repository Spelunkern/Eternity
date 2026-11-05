#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include <vector>

namespace eternity::game {
// Route ps_game.exe DB-agent 0302 (00408d30..00408dfd) through the
// account's retained session. All calls and the supplied leave continuation
// run on the world strand. The continuation must run actual disconnect/save
// cleanup; it must not treat receipt alone as a successful database save.
// Sessions without a selected WorldUser have no ack state; their transport must
// separately fence any outstanding preselection acknowledgments before release.
class VerticalWorldSessionAckRouter final {
public:
    using Leave=std::function<void(std::shared_ptr<VerticalWorldSession>)>;
    VerticalWorldSessionAckRouter(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,Leave leave)
        :strand_(std::move(strand)),registry_(registry),leave_(std::move(leave)) {
        if(!leave_)throw std::invalid_argument("Missing world acknowledgment leave service");
    }
    bool matches(const VerticalWorldSession::Strand& strand,const VerticalWorldSession::Registry& registry) const noexcept {
        return strand_==strand && &registry_==&registry;
    }
    bool receive(std::span<const std::uint8_t> packet) {
        if(!strand_.running_in_this_thread())throw std::logic_error("Database acknowledgment outside world strand");
        struct Adapter final:VerticalDatabaseAckServices {
            VerticalWorldSession::Registry& registry;Leave& on_leave;std::vector<std::shared_ptr<VerticalWorldSession>> pins;
            Adapter(VerticalWorldSession::Registry& r,Leave& l):registry(r),on_leave(l){}
            VerticalDatabaseAckUser* find(std::uint32_t account) override {
                auto session=registry.find_account(account);if(!session || !session->database_ack_user())return nullptr;
                auto* user=session->database_ack_user();pins.push_back(std::move(session));return user;
            }
            std::shared_ptr<VerticalWorldSession> owner(VerticalDatabaseAckUser& user) {
                for(const auto& pin:pins)if(pin->database_ack_user()==&user)return pin;
                throw std::logic_error("Database acknowledgment lost session owner");
            }
            void close(VerticalDatabaseAckUser& user,std::uint32_t reason) override {auto session=owner(user);session->channel().disconnect_with_reason(reason);}
            void leave(VerticalDatabaseAckUser& user) override {on_leave(owner(user));}
        } adapter(registry_,leave_);
        return vertical_database_disconnect_ack(packet,adapter);
    }
private:
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;Leave leave_;
};
}
