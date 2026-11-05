#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/zone_entry.hpp"
#include <exception>
#include <vector>

namespace eternity::game {
// The live world supplies scheduler, map, guild and zone-entry services. Every
// callback runs on the common world strand; returned entry state is the
// canonical per-session state and survives subsequent movement and zone leave.
// Callbacks must not throw or destroy users while WorldAdmission is iterating;
// use the adapter update methods to retain session owners through each pass.
struct VerticalWorldSessionAdmissionPorts {
    std::function<void(VerticalWorldSession&)> packets;
    std::function<RuntimeZone*(VerticalWorldSession&)> select_zone;
    std::function<ZoneActorEntryServices(VerticalWorldSession&,RuntimeZone&)> entry_services;
    std::function<void(VerticalWorldSession&,WorldAdmissionError)> diagnostic;
    std::function<std::optional<std::uint32_t>(RuntimeZone&)> rank_window;
    std::function<bool(VerticalWorldSession&)> lock_guild;
    std::function<void(VerticalWorldSession&)> unlock_guild;
};
// Binds WorldAdmission to retained sessions, WorldUsers and actual actor zone
// entry. Registry, indexes, callbacks, actor and zone outlive each invocation.
// ps_game.exe CWorld::WaitUser 0040ce70 / UpdatePackets 0040bf90,
// EnterUserWorld 0040d450 and CZone::EnterUser 00415f80.
class VerticalWorldSessionAdmissionServices final:public WorldAdmissionServices {
public:
    VerticalWorldSessionAdmissionServices(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        WorldUsers& users,VerticalWorldSessionAdmissionPorts ports)
        :strand_(std::move(strand)),registry_(registry),users_(users),ports_(std::move(ports)) {
        if(!ports_.packets || !ports_.select_zone || !ports_.entry_services || !ports_.diagnostic ||
           !ports_.rank_window || !ports_.lock_guild || !ports_.unlock_guild)
            throw std::invalid_argument("Incomplete session admission services");
    }
    VerticalWorldSessionAdmissionServices(const VerticalWorldSessionAdmissionServices&)=delete;
    VerticalWorldSessionAdmissionServices& operator=(const VerticalWorldSessionAdmissionServices&)=delete;
    // These entry points retain each callback owner until the complete live-list
    // pass ends. The underlying world list and callback order are unchanged.
    void update_world(WorldAdmission& world) {with_pass([&]{world.update_packets();});}
    // The composed owner also releases the paused socket after the actual
    // location-3 publication, while this pass still retains every candidate.
    void update_world_and_release(WorldAdmission& world) {
        with_pass([&]{world.update_packets();auto published=std::move(pass_->published);std::exception_ptr failure;
            for(auto& [owner,zone]:published) {
                if(owner->closing() || owner->failed() || owner->admission().location!=3 || owner->admission().zone!=zone)continue;
                try {owner->publish_zone_handoff();}catch(...) {
                    auto error=std::current_exception();if(!failure)failure=error;
                    try {owner->report_admission_failure(error);}catch(...) {}
                }
            }
            if(failure)std::rethrow_exception(failure);
        },true);
    }
    void update_zone_packets(WorldAdmission& world,RuntimeZone& zone) {with_pass([&]{world.update_zone_wait_packets(zone);});}
    void update_zone_logout(WorldAdmission& world,RuntimeZone& zone,std::uint32_t tick,
        const std::function<void(VerticalWorldSession&)>& logout) {
        if(!logout)throw std::invalid_argument("Missing zone logout service");
        with_pass([&]{world.update_zone_wait_logout_deadlines(zone,tick,[&](WorldAdmissionUser& user){auto owner=retain(user);logout(*owner);});});
    }
    void packets(WorldAdmissionUser& user) override {auto owner=retain(user);ports_.packets(*owner);}
    RuntimeZone* select_zone(WorldAdmissionUser& user) override {
        auto owner=retain(user);const bool waiting=user.location==2;auto* zone=ports_.select_zone(*owner);
        if(waiting && zone && pass_ && pass_->release_handoffs)pass_->published.emplace_back(owner,zone);
        return zone;
    }
    void register_world(WorldAdmissionUser& user) override {
        auto owner=retain(user);auto* current=owner->world_user();if(!current)throw std::logic_error("Admission has no world user");
        if(users_.find(current->actor.id()) || users_.find(current->actor.appearance().identity.name))throw std::logic_error("Duplicate world user index");
        users_.enter(*current);
    }
    bool enter_zone(WorldAdmissionUser& user,RuntimeZone& zone) override {
        auto owner=retain(user);auto* current=owner->world_user();if(!current || users_.find(current->actor.id())!=current)
            throw std::logic_error("Zone entry lost world user");
        auto& state=owner->zone_entry();auto services=ports_.entry_services(*owner,zone);
        return enter_zone_actor(zone,current->actor,state,services);
    }
    void close(WorldAdmissionUser& user,std::uint32_t reason) override {auto owner=retain(user);owner->channel().disconnect_with_reason(reason);}
    void diagnostic(WorldAdmissionUser& user,WorldAdmissionError error) override {auto owner=retain(user);ports_.diagnostic(*owner,error);}
    std::optional<std::uint32_t> rank_window(RuntimeZone& zone) override {require_strand();return ports_.rank_window(zone);}
    bool lock_guild(WorldAdmissionUser& user) override {auto owner=retain(user);return ports_.lock_guild(*owner);}
    void unlock_guild(WorldAdmissionUser& user) override {auto owner=retain(user);ports_.unlock_guild(*owner);}
private:
    struct Pass {Pass* previous;std::vector<std::shared_ptr<VerticalWorldSession>> pins;bool release_handoffs;std::vector<std::pair<std::shared_ptr<VerticalWorldSession>,RuntimeZone*>> published;};
    template<class F> void with_pass(F&& action,bool release_handoffs=false) {
        require_strand();Pass pass{pass_,{},release_handoffs,{}};pass_=&pass;std::exception_ptr error;
        try {action();} catch(...) {error=std::current_exception();}
        pass_=pass.previous;
        for(auto& owner:pass.pins)try{owner->end_admission_pass();}catch(...){if(!error)error=std::current_exception();}
        if(error)std::rethrow_exception(error);
    }
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Session admission outside world strand");}
    std::shared_ptr<VerticalWorldSession> retain(WorldAdmissionUser& user) const {
        require_strand();auto owner=registry_.find_account(user.user_id);
        if(!owner || &owner->admission()!=&user)throw std::logic_error("Admission user lost its retained session");
        for(auto* pass=pass_;pass;pass=pass->previous) {pass->pins.push_back(owner);try{owner->begin_admission_pass();}catch(...){pass->pins.pop_back();throw;}}
        return owner;
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;WorldUsers& users_;VerticalWorldSessionAdmissionPorts ports_;mutable Pass* pass_=nullptr;
};
}
