#pragma once
#include "eternity/game/vertical_move_retained_world_transfer.hpp"
#include <map>

namespace eternity::game {
// The world/session owner installs this on its strand and wraps the real
// disconnect service. Operations remain retained until that service actually
// completes; a reentrant close cannot run cleanup inside begin/complete.
class VerticalMoveTransferDirectory final:public std::enable_shared_from_this<VerticalMoveTransferDirectory> {
public:
    using Session=VerticalWorldSession;
    using Disconnect=std::function<void(Session&,std::optional<std::uint32_t>,std::function<void()>)>;
    static std::shared_ptr<VerticalMoveTransferDirectory> create(Session::Strand strand,Session::Registry& registry) {
        if(!strand.running_in_this_thread())throw std::logic_error("Transfer directory requires world strand");
        return std::shared_ptr<VerticalMoveTransferDirectory>(new VerticalMoveTransferDirectory(std::move(strand),registry));
    }
    void begin(std::shared_ptr<Session> owner,std::unique_ptr<VerticalMoveRetainedWorldTransfer> operation) {
        require();if(!owner || !operation || !operation->matches(*owner) || owner->closing() || owner->failed() || registry_.find_account(owner->account_id())!=owner)
            throw std::invalid_argument("Transfer directory requires a live registered session and operation");
        const auto account=owner->account_id();if(entries_.contains(account))throw std::logic_error("Session already has a transfer");
        auto entry=std::make_shared<Entry>();entry->owner=std::move(owner);entry->operation=std::move(operation);entries_.emplace(account,entry);
        run(entry,[&]{entry->operation->begin();});
    }
    void complete(Session& owner) {
        auto entry=find(owner);if(entry->closing)throw std::logic_error("Session transfer is disconnecting");
        run(entry,[&]{entry->operation->complete();});
        if(!entry->closing)erase(entry);
    }
    // Call from the actual session disconnect service. The supplied service
    // must perform real cleanup and invoke its completion exactly once.
    void disconnect(Session& owner,std::optional<std::uint32_t> reason,std::function<void()> done,Disconnect service) {
        require();if(!owner.closing() || !done || !service)throw std::invalid_argument("Transfer disconnect requires closing session and real service");
        auto found=entries_.find(owner.account_id());if(found==entries_.end()){service(owner,reason,std::move(done));return;}
        auto entry=find(owner);if(entry->closing)throw std::logic_error("Transfer disconnect already started");
        entry->closing=true;entry->reason=reason;entry->done=std::move(done);entry->disconnect=std::move(service);
        if(!entry->busy)start_disconnect(entry);
    }
    bool contains(Session& owner) {require();auto it=entries_.find(owner.account_id());return it!=entries_.end() && it->second->owner.get()==&owner;}
    std::size_t size() const {require();return entries_.size();}
private:
    struct Entry {std::shared_ptr<Session> owner;std::unique_ptr<VerticalMoveRetainedWorldTransfer> operation;
        bool busy=false,closing=false,started=false,finished=false;std::optional<std::uint32_t> reason;std::function<void()> done;Disconnect disconnect;};
    VerticalMoveTransferDirectory(Session::Strand strand,Session::Registry& registry):strand_(std::move(strand)),registry_(registry){}
    void require() const {if(!strand_.running_in_this_thread())throw std::logic_error("Transfer directory outside world strand");}
    std::shared_ptr<Entry> find(Session& owner) {
        require();auto it=entries_.find(owner.account_id());if(it==entries_.end() || it->second->owner.get()!=&owner || registry_.find_account(owner.account_id())!=it->second->owner)
            throw std::logic_error("Transfer directory lost registered session");return it->second;
    }
    void erase(const std::shared_ptr<Entry>& entry) {
        auto it=entries_.find(entry->owner->account_id());if(it==entries_.end() || it->second!=entry)throw std::logic_error("Transfer directory entry changed");entries_.erase(it);
    }
    template<class F> void run(const std::shared_ptr<Entry>& entry,F action) {
        require();if(entry->busy || entry->closing)throw std::logic_error("Transfer operation is busy or closing");entry->busy=true;
        try {action();}catch(...) {entry->busy=false;if(entry->closing)start_disconnect(entry);throw;}
        entry->busy=false;if(entry->closing)start_disconnect(entry);
    }
    void start_disconnect(const std::shared_ptr<Entry>& entry) {
        require();if(!entry->closing || entry->busy || entry->started)throw std::logic_error("Invalid transfer disconnect ordering");
        entry->started=true;auto self=shared_from_this();auto service=std::move(entry->disconnect);
        service(*entry->owner,entry->reason,[self,entry]{
            self->require();if(!entry->started || entry->busy || entry->finished)throw std::logic_error("Invalid transfer disconnect completion");
            entry->finished=true;self->erase(entry);entry->operation.reset();auto done=std::move(entry->done);done();
        });
    }
    Session::Strand strand_;Session::Registry& registry_;std::map<std::uint32_t,std::shared_ptr<Entry>> entries_;
};
}
