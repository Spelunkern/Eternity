#pragma once
#include "eternity/game/vertical_world_session_save_binding.hpp"
#include "eternity/game/vertical_save_core_skills.hpp"
#include "eternity/game/vertical_database_disconnect.hpp"
#include "eternity/game/zone_actor_leave.hpp"

namespace eternity::game {
// Real location-4 disconnect operations for one retained session. All ports
// are live owners and outlive this adapter and its synchronous calls.
// ps_game.exe CWorld::LeaveUser 0040ca20, SendDBAgentCharGetInfo 0046c980,
// SendDBAgentUserLeave 0046c660, CZone::LeaveUser 004161b0.
template<class Store=CharacterSaveStore> class VerticalWorldSessionZoneServices final:public VerticalDisconnectServices {
public:
    using Binding=VerticalWorldSessionSaveBinding<Store>;
    struct Ports {
        std::function<std::recursive_mutex&(WorldUser&)> user_mutex;
        std::function<std::recursive_mutex&(RuntimeZone&)> zone_mutex;
        std::function<void(WorldUser&)> dead_disconnect;
        std::function<std::uint32_t()> tick_ms;
        std::function<UserSkillRuntime&(VerticalWorldSession&)> skills;
        std::function<QuestRuntime&(VerticalWorldSession&)> quests;
        // Alternative for a retained quest owner which exposes ordered save
        // rather than its runtime. It must use the same account, replica,
        // mutex and supplied tick; do not report a synthetic success.
        std::function<void(VerticalWorldSession&,std::uint32_t,VerticalSaveCoreSkillServices&)> save_quests;
        std::function<VerticalSaveCoreSkillServices&(VerticalWorldSession&)> save;
        std::function<std::shared_ptr<Binding>(VerticalWorldSession&)> binding;
        std::function<ZoneActorLeaveServices(VerticalWorldSession&,RuntimeZone&)> leave;
        std::function<void(VerticalWorldSession&,WorldUser&)> finish_final;
    };
    VerticalWorldSessionZoneServices(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        std::shared_ptr<VerticalWorldSession> owner,Ports ports)
        :strand_(std::move(strand)),registry_(registry),owner_(std::move(owner)),ports_(std::move(ports)) {
        if(!strand_.running_in_this_thread() || !owner_ || registry_.find_account(owner_->account_id())!=owner_ ||
           !ports_.user_mutex || !ports_.zone_mutex || !ports_.dead_disconnect || !ports_.tick_ms || !ports_.skills ||
           (bool(ports_.quests)==bool(ports_.save_quests)) || !ports_.save || !ports_.binding || !ports_.leave || !ports_.finish_final)
            throw std::invalid_argument("Incomplete session zone disconnect services");
    }
    bool matches(const VerticalWorldSession& session) const noexcept {return owner_.get()==&session;}
    std::recursive_mutex& user_mutex(WorldUser& user) override {require(user);return ports_.user_mutex(user);}
    std::recursive_mutex& zone_mutex(RuntimeZone& zone) override {require_owner();return ports_.zone_mutex(zone);}
    void dead_disconnect(WorldUser& user) override {require(user);ports_.dead_disconnect(user);}
    std::uint32_t tick_ms() override {require_owner();return ports_.tick_ms();}
    void prepare_save(WorldUser& user) override {
        require(user);auto* character=owner_->character();if(!character)throw std::logic_error("Zone save lost session character");
        auto binding=ports_.binding(*owner_);if(!binding || !binding->matches(*owner_))throw std::logic_error("Zone save lost session binding");
        auto& skills=ports_.skills(*owner_);if(!binding->matches_zone_skills(skills))throw std::logic_error("Zone save skill owner differs from replica");
        BoundSave save(ports_.save(*owner_),std::move(binding),skills);std::array<std::uint8_t,775> scratch{};
        const auto tick=vertical_save_core_skills_effects(user.actor,owner_->zone_entry(),skills,save,scratch);
        if(ports_.quests)quest_runtime_save(ports_.quests(*owner_),owner_->account_id(),tick,
            {[&]{save.lock_quests();},[&]{save.unlock_quests();},[&](const net::Packet& packet){save.send_database(packet);}});
        else ports_.save_quests(*owner_,tick,save);
        character->quick_slots().save(owner_->account_id(),[&](const net::Packet& packet){save.send_database(packet);});
    }
    void request_save(WorldUser& user) override {
        require(user);auto binding=ports_.binding(*owner_);if(!binding || !binding->matches(*owner_))throw std::logic_error("Zone save lost session binding");
        if(!binding->disconnect(vertical_database_disconnect_packet(owner_->account_id())))throw std::runtime_error("Zone save request rejected");
    }
    void leave_zone(WorldUser& user,RuntimeZone& zone) override {
        require(user);auto services=ports_.leave(*owner_,zone);
        if(!leave_zone_actor(zone,user,owner_->zone_entry(),services))throw std::logic_error("Zone leave did not complete");
    }
    void finish_final(WorldUser& user) override {require(user);ports_.finish_final(*owner_,user);}
private:
    class BoundSave final:public VerticalSaveCoreSkillServices {
    public:
        BoundSave(VerticalSaveCoreSkillServices& world,std::shared_ptr<Binding> binding,const UserSkillRuntime& skills):world_(world),binding_(std::move(binding)),skills_(skills){}
        UserPosition position(Actor& actor) override{return world_.position(actor);}
        std::uint32_t tick_ms() override{return world_.tick_ms();}
        void lock_effects() override{world_.lock_effects();}
        void unlock_effects() override{world_.unlock_effects();}
        void lock_quests() override{world_.lock_quests();}
        void unlock_quests() override{world_.unlock_quests();}
        void send_database(const net::Packet& packet) override {
            if(binding_->apply_zone_save(packet,&skills_)!=InventoryReplicaResult::applied)throw std::runtime_error("Zone save replica update rejected");
        }
    private:
        VerticalSaveCoreSkillServices& world_;std::shared_ptr<Binding> binding_;const UserSkillRuntime& skills_;
    };
    void require_owner() const {if(!strand_.running_in_this_thread() || registry_.find_account(owner_->account_id())!=owner_)throw std::logic_error("Zone disconnect lost retained session");}
    void require(WorldUser& user) const {require_owner();if(owner_->world_user()!=&user)throw std::logic_error("Zone disconnect received foreign user");}
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;std::shared_ptr<VerticalWorldSession> owner_;Ports ports_;
};
}
