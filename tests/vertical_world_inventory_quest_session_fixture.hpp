#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/character_replica_delivery.hpp"

namespace eternity::game {
// Cohosted DB-agent route. The replica mutex is the SAME mutex used by the
// save owner; all remaining services are supplied by the actual world owner.
// Actor, replica and upstream must outlive this adapter and its calls. The
// mutex lease must own the same mutex used by the save owner.
class VerticalWorldQuestReplicaPorts final:public VerticalQuestPorts {
public:
    VerticalWorldQuestReplicaPorts(Actor& actor,CharacterReplica& replica,std::shared_ptr<std::recursive_mutex> mutex,VerticalQuestPorts& upstream)
        :actor_(actor),replica_(replica),mutex_(std::move(mutex)),delivery_(replica,require_mutex(mutex_)),upstream_(upstream) {
        std::lock_guard lock(*mutex_);validate_replica();
    }
    std::uint32_t local_make_time(Actor& a) override {validate_actor(a);return upstream_.local_make_time(a);}
    std::uint32_t tick_ms(Actor& a) override {validate_actor(a);return upstream_.tick_ms(a);}
    void send_database(Actor& a,const net::Packet& packet) override {
        validate_actor(a);std::lock_guard lock(*mutex_);validate_replica();
        if(delivery_.apply(packet)!=InventoryReplicaResult::applied) throw std::runtime_error("Quest database replica rejected packet");
    }
    void send_client(Actor& a,const net::Packet& packet) override {validate_actor(a);upstream_.send_client(a,packet);}
    void lock_quests(Actor& a) override {validate_actor(a);upstream_.lock_quests(a);}
    void unlock_quests(Actor& a) noexcept override {if(&a!=&actor_) std::terminate();upstream_.unlock_quests(a);}
    std::uint32_t quest_tick_ms() override {return upstream_.quest_tick_ms();}
    void fill_game_log_main(Actor& a,std::span<std::uint8_t> packet) override {validate_actor(a);upstream_.fill_game_log_main(a,packet);}
    void send_game_log(Actor& a,const net::Packet& packet) override {validate_actor(a);upstream_.send_game_log(a,packet);}
    QuestRewardWorld& reward_world(Actor& a) override {validate_actor(a);return upstream_.reward_world(a);}
    QuestStartLogContext start_log_context(Actor& a) override {validate_actor(a);return upstream_.start_log_context(a);}
    void send_start_log(Actor& a,const std::array<std::uint8_t,144>& packet) override {validate_actor(a);upstream_.send_start_log(a,packet);}
private:
    static std::recursive_mutex& require_mutex(const std::shared_ptr<std::recursive_mutex>& mutex) {if(!mutex) throw std::invalid_argument("Quest replica requires retained save mutex");return *mutex;}
    void validate_actor(const Actor& actor) const {if(&actor!=&actor_) throw std::logic_error("Quest replica Actor changed");}
    void validate_replica() const {const auto core=replica_.core();if(core.user_id!=actor_.account_id() || core.character.id!=actor_.id()) throw std::logic_error("Quest replica does not match Actor");}
    Actor& actor_;CharacterReplica& replica_;std::shared_ptr<std::recursive_mutex> mutex_;CharacterReplicaDelivery delivery_;VerticalQuestPorts& upstream_;
};
struct VerticalWorldQuestInstall {
    QuestStartItemsActorWorldOwners owners;
    QuestStartItemsActorUseState use;
    ZoneNpcs& zone;NpcPool& npcs;std::shared_ptr<std::recursive_mutex> replica_mutex;VerticalQuestPorts& upstream;
    PlayerCore& player;const StatusRules& rules;const Experience& experience;ExperienceAwardPorts& experience_ports;
    VerticalQuestRewardExperience::Options options;VerticalQuestRewardItems::ItemName item_name;
    VerticalNpcInteraction& interaction;VerticalQuestPacketActions& quest_actions;VerticalNpcPacketActions& npc_actions;
};
// Installation occurs only on the world strand during real preparation.
// The world retains all supplied owners and services until quest teardown.
inline void vertical_world_install_replica_quests(VerticalWorldSession& session,VerticalWorldQuestInstall binding) {
    auto* character=session.character();auto* user=session.world_user();
    if(!character || !user || !binding.owners.actor || binding.owners.actor.get()!=&user->actor)
        throw std::invalid_argument("World quest installation requires the selected Actor");
    if(!binding.replica_mutex) throw std::invalid_argument("World quest installation requires retained save mutex");
    const auto self=session.shared_from_this();
    if(!binding.replica_mutex.owner_before(self) && !self.owner_before(binding.replica_mutex))
        throw std::invalid_argument("Quest save mutex lease would retain its owning session");
    auto ports=std::make_unique<VerticalWorldQuestReplicaPorts>(*binding.owners.actor,character->replica(),std::move(binding.replica_mutex),binding.upstream);
    auto quests=std::make_unique<VerticalQuestSession>(std::move(binding.owners),binding.use,binding.zone,binding.npcs,*ports,
        binding.player,binding.rules,binding.experience,binding.experience_ports,std::move(binding.options),std::move(binding.item_name),
        binding.interaction,binding.quest_actions,binding.npc_actions);
    session.install_quests(std::move(quests),std::move(ports));
}
// Call on the session strand during preparation with the real mutex protecting
// this character's replica. The returned Actor lease pins its character and
// scene without retaining the enclosing world session.
inline VerticalQuestSessionRestoreResult vertical_world_quests_restore(VerticalWorldSession& session,std::recursive_mutex& replica_mutex,
    std::uint32_t tick,std::shared_ptr<QuestRuntimePool> pool,std::shared_ptr<const QuestDefinitions> definitions) {
    if(!pool || !definitions) throw std::invalid_argument("World quest restore requires pool and definitions");
    auto actor=session.quest_actor_lease();auto* character=session.character();
    if(!character) throw std::logic_error("World quest restore lost selected character");
    std::lock_guard lock(replica_mutex);const auto core=character->replica().core();
    if(actor->account_id()!=session.account_id() || actor->account_id()!=core.user_id || actor->id()!=core.character.id)
        throw std::logic_error("World quest actor and replica differ");
    auto quests=vertical_quests_restore_replica(character->replica(),tick,std::move(pool),std::move(definitions));
    if(actor->account_id()!=core.user_id || actor->id()!=core.character.id || session.character()!=character)
        throw std::logic_error("World quest actor changed during restore");
    return {std::move(actor),std::move(quests)};
}
// All calls run on the world strand; registry and services outlive visitors.
class VerticalWorldQuestSessions final:public VerticalQuestSessionResolver {
public:
    explicit VerticalWorldQuestSessions(VerticalWorldSession::Registry& registry):registry_(registry) {}
    std::shared_ptr<VerticalQuestSession> retain(Actor& actor) override {
        auto owner=registry_.retain_actor(actor);
        // Disconnect cleanup must still reach the retained quest/NPC owner.
        if(!owner || !owner->world_user() || &owner->world_user()->actor!=&actor) return {};
        auto quests=owner->quests_lease();
        if(!quests || !quests->quests().owns_actor(actor)) return {};
        return quests;
    }
private:
    VerticalWorldSession::Registry& registry_;
};
// Bind as VerticalWorldSessionPacketRouter::OwnedDispatch. The caller and all
// owners use the world strand; false leaves unrelated families to the router.
class VerticalWorldQuestPacketDispatch final {
public:
    explicit VerticalWorldQuestPacketDispatch(VerticalWorldSession::Registry& registry):registry_(registry),sessions_(registry) {}
    bool operator()(VerticalWorldSession& owner,const net::Packet& packet) {
        if(packet.size()<2) throw std::invalid_argument("Malformed world quest packet");
        if(packet[1]!=9 && packet[1]!=14) return false;
        auto* user=owner.world_user();auto* zone=owner.admission().zone;
        if(owner.closing() || !user || owner.admission().location!=4 || !zone || !zone->users || !zone->npcs ||
           zone->users->find_actor(user->actor.id())!=&user->actor || registry_.retain_actor(user->actor).get()!=&owner)
            throw std::logic_error("Quest packet requires the registered admitted Actor");
        auto quests=sessions_.retain(user->actor);
        if(!quests || !quests->quests().owns_npc_zone(*zone->npcs)) throw std::logic_error("Quest packet uses a different NPC zone");
        switch(quests->packets().receive(packet)) {
        case VerticalQuestNpcPacketResult::handled:case VerticalQuestNpcPacketResult::closed:return true;
        case VerticalQuestNpcPacketResult::malformed:throw std::invalid_argument("Malformed quest/NPC request");
        case VerticalQuestNpcPacketResult::unhandled:throw std::logic_error("Quest/NPC family was not handled");
        }
        throw std::logic_error("Invalid quest/NPC result");
    }
private:
    VerticalWorldSession::Registry& registry_;VerticalWorldQuestSessions sessions_;
};
}
