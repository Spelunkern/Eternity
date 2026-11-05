#pragma once
#include "eternity/game/vertical_async_selection.hpp"
#include "eternity/game/vertical_session_registry.hpp"

namespace eternity::game {
// Connects a retained real session to asynchronous selection. All operations
// and callbacks run on the world strand. The registry must outlive this object
// and its DB task. The caller reserves the owner before constructing this
// object, retains it through close, and supplies actual admission services.
// ps_game.exe PacketCharacter 0046bf20 and PacketUserDBChar 0046cf60.
template<class Session> class VerticalOwnedSelection final {
public:
    using Registry=VerticalSessionRegistry<Session>;
    using Strand=VerticalAsyncSelection::Strand;
    using Loaded=std::function<void(Session&,VerticalPersistenceSelection)>;
    using Unavailable=std::function<void(Session&)>;
    using Close=std::function<void(Session&,std::uint32_t)>;
    using Failure=std::function<void(Session&,std::exception_ptr)>;
    VerticalOwnedSelection(Strand strand,asio::any_io_executor database,Registry& registry,
        std::shared_ptr<Session> owner,CharacterSelectionState state,CharacterRoster roster,
        VerticalAsyncSelection::Load load,Loaded loaded,Unavailable unavailable,Close close,Failure failure)
        :registry_(registry),account_(state.user_id) {
        if(!strand.running_in_this_thread() || !owner || registry_.find_account(account_)!=owner ||
           !loaded || !unavailable || !close || !failure)
            throw std::invalid_argument("Incomplete retained selection owner or services");
        auto weak=std::weak_ptr<Session>(owner);
        selection_=std::make_shared<VerticalAsyncSelection>(strand,std::move(database),state,std::move(roster),std::move(load),
            [weak,loaded=std::move(loaded)](VerticalPersistenceSelection result){if(auto owner=weak.lock())loaded(*owner,std::move(result));},
            [weak,unavailable=std::move(unavailable)]{if(auto owner=weak.lock())unavailable(*owner);},
            [weak,close=std::move(close)](std::uint32_t reason){if(auto owner=weak.lock())close(*owner,reason);},
            [weak,failure=std::move(failure)](std::exception_ptr error){if(auto owner=weak.lock())failure(*owner,error);});
        auto* retained=&registry_;const auto account=account_;
        selection_->set_fence([retained,weak,account]{auto owner=weak.lock();return owner && retained->begin_database_work(account,*owner);},
            [retained,weak,account]{auto owner=weak.lock();return owner && retained->finish_database_work(account,*owner);});
    }
    // Callbacks can synchronously destroy the session and this adapter.
    CharacterSelectionResult receive(std::span<const std::uint8_t> packet) {auto selection=selection_;return selection->receive(packet);}
    // Stop selection before starting the owner's wider disconnect/drain. The
    // callback does not release the account: other producers, acknowledgments,
    // and admission teardown must also be fenced by the owner.
    void cancel_and_drain(std::function<void()> done) {auto selection=selection_;selection->cancel();selection->when_drained(std::move(done));}
    bool pending() const {return selection_->pending();}
private:
    Registry& registry_;std::uint32_t account_;
    std::shared_ptr<VerticalAsyncSelection> selection_;
};
}
