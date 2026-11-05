#pragma once
#include "eternity/game/vertical_world_session_gameplay.hpp"
#include <map>

namespace eternity::game {
// Owns each selected session's input FIFO and its lazily constructed gameplay
// owner. All calls, callbacks, registry and zone access use the world strand.
// The gameplay factory must bind the actual session/zone and all real movement,
// view and other-opcode services. No missing handler is treated as success.
// This router must outlive its queues and every in-flight callback.
// ps_game.exe CUser::UpdatePackets 00466cc0, PacketProcessing 00466da0.
class VerticalWorldSessionPacketRouter final {
public:
    using GameplayFactory=std::function<std::shared_ptr<VerticalWorldSessionGameplay>(VerticalWorldSession&,RuntimeZone&)>;
    using OwnedDispatch=std::function<bool(VerticalWorldSession&,const net::Packet&)>;
    VerticalWorldSessionPacketRouter(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        std::size_t max_packets,std::size_t max_bytes,GameplayFactory gameplay,OwnedDispatch owned={})
        :strand_(std::move(strand)),registry_(registry),max_packets_(max_packets),max_bytes_(max_bytes),gameplay_(std::move(gameplay)),owned_(std::move(owned)) {
        if(!max_packets_ || !max_bytes_ || !gameplay_)throw std::invalid_argument("Incomplete session packet router");
    }
    VerticalWorldSessionPacketRouter(const VerticalWorldSessionPacketRouter&)=delete;
    VerticalWorldSessionPacketRouter& operator=(const VerticalWorldSessionPacketRouter&)=delete;
    VerticalWorldSessionPacketRouter(VerticalWorldSessionPacketRouter&&)=delete;
    VerticalWorldSessionPacketRouter& operator=(VerticalWorldSessionPacketRouter&&)=delete;
    // Attach once after account reservation. The service must stop this owner
    // before registry release, including failed preparation and disconnect.
    void attach(std::shared_ptr<VerticalWorldSession> owner) {
        require_strand();if(!owner || registry_.find_account(owner->account_id())!=owner)throw std::invalid_argument("Packet router requires registered session");
        if(entries_.contains(owner->account_id()))throw std::logic_error("Packet router account already attached");
        auto entry=std::make_shared<Entry>();entry->owner=owner;std::weak_ptr<Entry> weak=entry;
        entry->packets=VerticalWorldSessionPackets::create(strand_,owner,max_packets_,max_bytes_,
            [this,weak](VerticalWorldSession& session,const net::Packet& packet){auto entry=weak.lock();if(!entry || !entry->active)throw std::logic_error("Packet owner stopped during dispatch");dispatch(*entry,session,packet);});
        entries_.emplace(owner->account_id(),std::move(entry));
    }
    // False means stopped, closed or bounded queue full. receive() is the
    // socket-facing policy and requests an actual disconnect on failure.
    bool try_enqueue(VerticalWorldSession& owner,const net::Packet& packet) {auto entry=find(owner);return !owner.dispatch_failed() && entry->packets->enqueue(packet);}
    void receive(VerticalWorldSession& owner,const net::Packet& packet) {if(!try_enqueue(owner,packet))owner.channel().disconnect();}
    // WorldAdmission may visit a selecting user before entry preparation has
    // completed. Leave its post-selection FIFO untouched until location 3/4.
    void update(VerticalWorldSession& owner) {
        auto entry=find(owner);const auto location=owner.admission().location;
        if(owner.dispatch_failed()) {entry->packets->stop();return;}
        if(location==3 || location==4) {
            try {entry->packets->update();}
            catch(...) {auto failure=std::current_exception();entry->packets->stop();owner.report_dispatch_failure(failure);}
        }
        else if(location!=1 && location!=2)throw std::logic_error("Packet router cannot update this location");
    }
    // Call on zone transfer even if a zone allocation reuses the same address.
    void invalidate_gameplay(VerticalWorldSession& owner) {auto entry=find(owner);entry->gameplay.reset();entry->zone=nullptr;}
    void suspend_gameplay(VerticalWorldSession& owner) {auto entry=find(owner);entry->packets->suspend_gameplay();}
    void resume_gameplay(VerticalWorldSession& owner) {auto entry=find(owner);entry->packets->resume_gameplay();}
    bool gameplay_suspended(VerticalWorldSession& owner) {auto entry=find(owner);return entry->packets->gameplay_suspended();}
    void stop(VerticalWorldSession& owner) {
        auto entry=find(owner);entry->active=false;entry->packets->stop();entry->gameplay.reset();entries_.erase(owner.account_id());
    }
    std::size_t pending(VerticalWorldSession& owner) {return find(owner)->packets->pending();}
private:
    struct Entry {std::weak_ptr<VerticalWorldSession> owner;std::shared_ptr<VerticalWorldSessionPackets> packets;
        std::shared_ptr<VerticalWorldSessionGameplay> gameplay;RuntimeZone* zone=nullptr;bool active=true;};
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Session packet router outside world strand");}
    std::shared_ptr<Entry> find(VerticalWorldSession& owner) {
        require_strand();auto found=entries_.find(owner.account_id());auto current=registry_.find_account(owner.account_id());
        if(found==entries_.end() || current.get()!=&owner || found->second->owner.lock()!=current || !found->second->active)
            throw std::logic_error("Packet router lost session owner");
        return found->second;
    }
    void dispatch(Entry& entry,VerticalWorldSession& owner,const net::Packet& packet) {
        auto* zone=owner.admission().zone;if(!zone || owner.admission().location!=4)throw std::logic_error("Gameplay packet has no admitted zone");
        if(owned_ && owned_(owner,packet))return;
        if(!entry.gameplay || entry.zone!=zone) {
            auto gameplay=gameplay_(owner,*zone);if(!gameplay)throw std::runtime_error("Gameplay owner unavailable");
            if(!entry.active || owner.closing() || owner.failed())throw std::logic_error("Session closed while creating gameplay owner");
            entry.gameplay=std::move(gameplay);entry.zone=zone;
        }
        auto gameplay=entry.gameplay;(void)gameplay->dispatch(owner,packet);
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;std::size_t max_packets_,max_bytes_;GameplayFactory gameplay_;OwnedDispatch owned_;
    std::map<std::uint32_t,std::shared_ptr<Entry>> entries_;
};
}
