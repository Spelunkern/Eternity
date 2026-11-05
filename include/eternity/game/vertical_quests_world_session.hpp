#pragma once
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/world_primary_quest_phase.hpp"
#include "eternity/game/character_replica_delivery.hpp"
#include "eternity/game/vertical_save_core_skills.hpp"
#include <algorithm>

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
    bool matches_save_binding(const CharacterReplica& replica,const std::recursive_mutex& mutex) const noexcept {return &replica_==&replica && mutex_.get()==&mutex;}
    std::uint32_t tick_ms(Actor& a) override {validate_actor(a);return upstream_.tick_ms(a);}
    void send_database(Actor& a,const net::Packet& packet) override {
        validate_actor(a);std::lock_guard lock(*mutex_);validate_replica();
        const bool add_quest=packet.size()==10 && packet[0]==1 && packet[1]==10;
        const bool remove_quest=packet.size()==9 && packet[0]==2 && packet[1]==10;
        const bool update_quest=packet.size()==13 && packet[0]==3 && packet[1]==10;
        const bool progress_quest=packet.size()==10 && packet[0]==5 && packet[1]==10 && packet[8]<3;
        const bool bulk_quest=packet.size()>=7 && packet[0]==4 && packet[1]==10 && packet.size()==7+7U*packet[6];
        const bool add_item=packet.size()==57 && packet[0]==1 && packet[1]==7;
        const bool remove_item=packet.size()==9 && packet[0]==2 && packet[1]==7 && packet[6]<6 && packet[7]<24;
        const bool count_item=packet.size()==9 && packet[0]==7 && packet[1]==7 && packet[6]<6 && packet[7]<24;
        const bool set_money=packet.size()==10 && packet[0]==3 && packet[1]==6;
        std::uint16_t quest_id=0;std::size_t before=0;std::uint64_t uid=0;
        std::uint32_t money=0;
        std::vector<std::size_t> bulk_indices;std::uint8_t item_count=0;
        if(add_quest || remove_quest || update_quest || progress_quest) {
            quest_id=static_cast<std::uint16_t>(packet[6]|(std::uint16_t(packet[7])<<8));
            const auto rows=replica_.quests().active();before=std::count_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==quest_id;});
            if(!add_quest && before==0) throw std::runtime_error("Quest database replica lacks active quest");
        }
        if(bulk_quest) {
            const auto rows=replica_.quests().active();std::size_t current=0;
            for(std::size_t at=7;at<packet.size();at+=7) {
                const auto id=static_cast<std::uint16_t>(packet[at]|(std::uint16_t(packet[at+1])<<8));
                while(current<rows.size() && rows[current].id!=id) ++current;
                if(current==rows.size()) throw std::runtime_error("Quest database replica lacks ordered bulk quest");
                bulk_indices.push_back(current++);
            }
        }
        if(add_item) {
            for(unsigned i=0;i<8;++i) uid|=std::uint64_t(packet[6+i])<<(8*i);
            const auto rows=replica_.inventory().inventory();
            if(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==packet[18] && row.item.slot==packet[19];}))
                throw std::runtime_error("Quest item replica destination occupied");
            if(packet[18]<6 && packet[19]<24) {
                const auto held=actor_.retained_inventory_item(packet[18],packet[19]);
                const auto id=std::uint32_t(packet[14])|(std::uint32_t(packet[15])<<8)|(std::uint32_t(packet[16])<<16)|(std::uint32_t(packet[17])<<24);
                if(!held || held->item->item.uid!=uid || held->item->item.id!=id || held->item->item.type!=packet[20] ||
                   held->item->item.type_id!=packet[21] || held->item->item.count!=packet[22])
                    throw std::runtime_error("Quest item insertion differs from actor inventory");
            }
        }
        if(remove_item || count_item) {
            const auto rows=replica_.inventory().inventory();
            const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.item.bag==packet[6] && value.item.slot==packet[7];});
            if(row==rows.end()) throw std::runtime_error("Quest item replica lacks inventory item");
            const auto held=actor_.retained_inventory_item(packet[6],packet[7]);
            if(!held || held->item->item.uid!=row->item.uid || held->item->item.type!=row->item.type || held->item->item.type_id!=row->item.type_id)
                throw std::runtime_error("Quest item replica differs from actor inventory");
            if((count_item && held->item->item.count!=packet[8]) ||
               (remove_item && packet[8]<row->item.count && held->item->item.count!=row->item.count-packet[8]))
                throw std::runtime_error("Quest item replica count differs from actor inventory");
            uid=row->item.uid;item_count=row->item.count;
        }
        if(set_money) {
            money=std::uint32_t(packet[6])|(std::uint32_t(packet[7])<<8)|(std::uint32_t(packet[8])<<16)|(std::uint32_t(packet[9])<<24);
            if(money!=actor_.money()) throw std::runtime_error("Quest money packet differs from actor balance");
        }
        if(delivery_.apply(packet)!=InventoryReplicaResult::applied) throw std::runtime_error("Quest database replica rejected packet");
        if(set_money && replica_.core().money!=money) throw std::runtime_error("Quest money replica failed to update balance");
        if(add_quest) {
            const auto rows=replica_.quests().active();
            if(static_cast<std::size_t>(std::count_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==quest_id;}))!=before+1)
                throw std::runtime_error("Quest database replica failed to allocate quest");
        }
        if(remove_quest || update_quest || progress_quest) {
            const auto rows=replica_.quests().active();const auto count=static_cast<std::size_t>(std::count_if(rows.begin(),rows.end(),[&](const auto& row){return row.id==quest_id;}));
            if(remove_quest) {if(count+1!=before) throw std::runtime_error("Quest database replica failed to remove quest");}
            else {
                const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.id==quest_id;});
                if(row==rows.end() || (progress_quest && row->counters[packet[8]]!=packet[9]) ||
                   (update_quest && (row->delay!=static_cast<std::uint16_t>(packet[8]|(std::uint16_t(packet[9])<<8)) ||
                    !std::equal(row->counters.begin(),row->counters.end(),packet.begin()+10))))
                    throw std::runtime_error("Quest database replica failed to update quest");
            }
        }
        if(bulk_quest) {
            const auto rows=replica_.quests().active();
            for(std::size_t i=0;i<bulk_indices.size();++i) {
                const auto at=7+7*i;const auto index=bulk_indices[i];
                if(index>=rows.size() || rows[index].id!=static_cast<std::uint16_t>(packet[at]|(std::uint16_t(packet[at+1])<<8)) ||
                   rows[index].delay!=static_cast<std::uint16_t>(packet[at+2]|(std::uint16_t(packet[at+3])<<8)) ||
                   !std::equal(rows[index].counters.begin(),rows[index].counters.end(),packet.begin()+static_cast<std::ptrdiff_t>(at+4)))
                    throw std::runtime_error("Quest database replica failed bulk update");
            }
        }
        if(add_item) {
            const auto rows=replica_.inventory().inventory();
            if(std::none_of(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==packet[18] && row.item.slot==packet[19] && row.item.uid==uid;}))
                throw std::runtime_error("Quest item replica failed to allocate item");
        }
        if(remove_item || count_item) {
            const auto rows=replica_.inventory().inventory();
            const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.item.bag==packet[6] && value.item.slot==packet[7];});
            const bool removed=remove_item && packet[8]>=item_count;
            const auto expected=count_item?packet[8]:static_cast<std::uint8_t>(item_count-packet[8]);
            if((removed && row!=rows.end()) || (!removed && (row==rows.end() || row->item.uid!=uid || row->item.count!=expected)))
                throw std::runtime_error("Quest item replica failed inventory update");
        }
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
struct VerticalWorldQuestPrepareResult {
    std::size_t skipped_active=0,skipped_finished=0;std::vector<std::uint16_t> invalid_active;
};
// The factory binds the restored owners to real world services. The selected
// session and those services remain live across this synchronous operation.
// Callbacks run without the save mutex; recheck the restored projection under
// that mutex after each callback and hold it through installation.
template<class Factory> inline VerticalWorldQuestPrepareResult vertical_world_prepare_replica_quests(VerticalWorldSession& session,
    std::shared_ptr<std::recursive_mutex> mutex,std::uint32_t tick,std::shared_ptr<QuestRuntimePool> pool,
    std::shared_ptr<const QuestDefinitions> definitions,Factory&& factory,
    std::function<void(const VerticalWorldQuestPrepareResult&)> before_install={}) {
    if(!mutex || !pool || !definitions) throw std::invalid_argument("Quest preparation requires replica mutex, pool and definitions");
    std::unique_lock lock(*mutex);
    auto restored=vertical_world_quests_restore(session,*mutex,tick,pool,definitions);
    auto* character=session.character();
    const auto active=character->replica().quests().active();
    const auto finished=character->replica().quests().finished();
    const auto validate_snapshot=[&] {
        if(session.character()!=character || session.quest_actor_lease().get()!=restored.actor.get())
            throw std::logic_error("Quest preparation lost selected actor");
        const auto current_active=character->replica().quests().active();
        const auto current_finished=character->replica().quests().finished();
        const auto active_equal=[](const CachedQuest& a,const CachedQuest& b){return a.id==b.id && a.delay==b.delay && a.counters==b.counters;};
        const auto finished_equal=[](const CachedQuest& a,const CachedQuest& b){return a.id==b.id && a.success==b.success;};
        if(current_active.size()!=active.size() || current_finished.size()!=finished.size() ||
           !std::equal(active.begin(),active.end(),current_active.begin(),active_equal) ||
           !std::equal(finished.begin(),finished.end(),current_finished.begin(),finished_equal))
            throw std::logic_error("Quest replica changed during preparation");
    };
    VerticalWorldQuestPrepareResult result{restored.quests.restored.skipped_active,restored.quests.restored.skipped_finished,restored.quests.invalid_active};
    lock.unlock();
    if(before_install)before_install(result);
    lock.lock();validate_snapshot();lock.unlock();
    auto binding=std::forward<Factory>(factory)(restored);
    const auto same_lease=[](const auto& a,const auto& b) {return a.get()==b.get() && !a.owner_before(b) && !b.owner_before(a);};
    if(!same_lease(binding.owners.actor,restored.actor) || !same_lease(binding.owners.runtime,restored.quests.restored.runtime) ||
       !same_lease(binding.owners.quest_pool,pool) || !same_lease(binding.owners.quests,definitions) || !same_lease(binding.replica_mutex,mutex))
        throw std::logic_error("Quest preparation factory changed restored owners");
    lock.lock();validate_snapshot();
    vertical_world_install_replica_quests(session,std::move(binding));return result;
}
// Invoke after the preparation owner has retained its save binding, before it
// publishes success. The independently owned mutex must also be the mutex used
// by that binding; it must not be an alias retaining the SQL resource.
template<class Binding,class Factory> inline VerticalWorldQuestPrepareResult vertical_world_prepare_bound_replica_quests(
    VerticalWorldSession& session,const VerticalWorldSession::Strand& strand,VerticalWorldSession::Registry& registry,
    Binding& save,std::shared_ptr<std::recursive_mutex> mutex,std::uint32_t tick,std::shared_ptr<QuestRuntimePool> pool,
    std::shared_ptr<const QuestDefinitions> definitions,Factory&& factory,
    std::function<void(const VerticalWorldQuestPrepareResult&)> before_install={}) {
    if(!strand.running_in_this_thread() || !mutex || !save.matches(strand,registry,session) || &save.save_mutex()!=mutex.get())
        throw std::invalid_argument("Quest preparation differs from retained save binding");
    return vertical_world_prepare_replica_quests(session,std::move(mutex),tick,std::move(pool),std::move(definitions),
        std::forward<Factory>(factory),std::move(before_install));
}
// Build the retained preparation's post-bind callback from actual per-session
// owners. The restore policy must handle skipped and invalid records explicitly.
template<class Binding> inline std::function<void(VerticalWorldSession&,Binding&)> vertical_world_quest_after_bind(
    VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,std::shared_ptr<std::recursive_mutex> mutex,
    std::shared_ptr<QuestRuntimePool> pool,std::shared_ptr<const QuestDefinitions> definitions,
    std::function<std::uint32_t(VerticalWorldSession&)> tick,
    std::function<VerticalWorldQuestInstall(VerticalWorldSession&,const VerticalQuestSessionRestoreResult&)> factory,
    std::function<void(VerticalWorldSession&,const VerticalWorldQuestPrepareResult&)> restore_policy) {
    if(!mutex || !pool || !definitions || !tick || !factory || !restore_policy)
        throw std::invalid_argument("Incomplete retained quest preparation services");
    return [strand=std::move(strand),&registry,mutex=std::move(mutex),pool=std::move(pool),definitions=std::move(definitions),
        tick=std::move(tick),factory=std::move(factory),restore_policy=std::move(restore_policy)](VerticalWorldSession& session,Binding& save) {
        if(!strand.running_in_this_thread() || !save.matches(strand,registry,session) || &save.save_mutex()!=mutex.get())
            throw std::invalid_argument("Quest preparation differs from retained save binding");
        (void)vertical_world_prepare_bound_replica_quests(session,strand,registry,save,mutex,tick(session),pool,definitions,
            [&](const VerticalQuestSessionRestoreResult& restored){return factory(session,restored);},
            [&](const VerticalWorldQuestPrepareResult& result){restore_policy(session,result);});
    };
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
// Entry runs on the session strand after quest installation and before world
// admission. The finished-list lock and registry outlive this synchronous use.
class VerticalWorldQuestEntry final {
public:
    VerticalWorldQuestEntry(VerticalWorldSession& owner,VerticalWorldSession::Registry& registry,VerticalQuestEntryFinishedLock& finished)
        :owner_(owner),registry_(registry),finished_(finished) {}
    void operator()(CharacterEntryStep step,Actor& actor) {
        if(step!=CharacterEntryStep::quests && step!=CharacterEntryStep::finished_quests) throw std::invalid_argument("Unexpected quest entry step");
        if(owner_.closing() || owner_.dispatch_failed() || owner_.world_user()==nullptr || &owner_.world_user()->actor!=&actor ||
           registry_.find_account(owner_.account_id()).get()!=&owner_ || registry_.retain_actor(actor).get()!=&owner_)
            throw std::logic_error("Quest entry lost selected Actor");
        auto quests=owner_.quests_lease();if(!quests || !quests->quests().owns_actor(actor)) throw std::logic_error("Quest entry lost session owner");
        if(!quests->quests().send_entry_step(step,finished_)) throw std::logic_error("Quest entry step was not handled");
        if(owner_.closing() || owner_.dispatch_failed() || owner_.world_user()==nullptr || &owner_.world_user()->actor!=&actor ||
           actor.account_id()!=owner_.account_id() || registry_.retain_actor(actor).get()!=&owner_)
            throw std::logic_error("Quest entry owner changed during delivery");
    }
private:
    VerticalWorldSession& owner_;VerticalWorldSession::Registry& registry_;VerticalQuestEntryFinishedLock& finished_;
};
inline std::shared_ptr<VerticalQuestEntryFinishedLock> vertical_world_quests_finished_lock(VerticalWorldSession& owner,VerticalWorldSession::Registry& registry) {
    auto* user=owner.world_user();
    if(!user || registry.find_account(owner.account_id()).get()!=&owner || registry.retain_actor(user->actor).get()!=&owner)
        throw std::logic_error("Finished quest lock lost selected Actor");
    auto quests=owner.quests_lease();if(!quests) throw std::logic_error("Finished quest lock lost session owner");
    auto runtime=quests->quests().runtime_owner_for(user->actor);const auto self=owner.shared_from_this();
    if(!runtime.owner_before(self) && !self.owner_before(runtime)) throw std::logic_error("Finished quest runtime would retain its session");
    return std::make_shared<VerticalQuestRuntimeFinishedLock>(user->actor,std::move(runtime));
}
// Protocol services are copied for synchronous entry delivery. Keep the real
// finished-list lock alive without retaining the enclosing session in a cycle.
inline void vertical_world_bind_quest_entry(VerticalProtocolServices& protocol,VerticalWorldSession& owner,
    VerticalWorldSession::Registry& registry,std::shared_ptr<VerticalQuestEntryFinishedLock> finished) {
    if(protocol.quest_entry || !finished || owner.closing() || owner.dispatch_failed() || !owner.world_user() || registry.find_account(owner.account_id()).get()!=&owner ||
       registry.retain_actor(owner.world_user()->actor).get()!=&owner || !owner.quests_lease())
        throw std::invalid_argument("Quest entry requires the selected session and finished lock");
    const auto self=owner.shared_from_this();
    if(!finished.owner_before(self) && !self.owner_before(finished)) throw std::invalid_argument("Quest entry lock would retain its owning session");
    std::weak_ptr<VerticalWorldSession> weak=self;
    protocol.quest_entry=[weak,&registry,finished=std::move(finished)](CharacterEntryStep step,Actor& actor) {
        auto owner=weak.lock();if(!owner) throw std::logic_error("Quest entry session expired");
        VerticalWorldQuestEntry entry(*owner,registry,*finished);entry(step,actor);
    };
}
// A save must pin the quest session and its pool through the entire save
// callback. Closing is allowed because the final save precedes teardown.
inline std::shared_ptr<QuestRuntime> vertical_world_quests_runtime_for_save(VerticalWorldSession& owner,VerticalWorldSession::Registry& registry) {
    auto* user=owner.world_user();
    if(!user || registry.find_account(owner.account_id()).get()!=&owner || registry.retain_actor(user->actor).get()!=&owner)
        throw std::logic_error("Quest save lost its registered Actor");
    auto quests=owner.quests_lease();if(!quests) throw std::logic_error("Quest save lost its session");
    auto* runtime=&quests->quests().runtime_for(user->actor);
    return {std::move(quests),runtime};
}
// Quest phase for a save coordinator that already persisted core, skills and
// effects with this same captured tick. It never sends the DB exit request.
inline void vertical_world_quests_save_phase(VerticalWorldSession& owner,VerticalWorldSession::Registry& registry,std::recursive_mutex& save_mutex,std::uint32_t tick) {
    auto* user=owner.world_user();auto* character=owner.character();
    if(!user || !character) throw std::logic_error("Quest save lost its character");
    auto runtime=vertical_world_quests_runtime_for_save(owner,registry);
    auto quests=owner.quests_lease();if(!quests || &quests->quests().runtime_for(user->actor)!=runtime.get())
        throw std::logic_error("Quest save runtime changed");
    const auto* ports=dynamic_cast<const VerticalWorldQuestReplicaPorts*>(&quests->upstream_ports());
    if(!ports || !ports->matches_save_binding(character->replica(),save_mutex))
        throw std::logic_error("Quest save replica or mutex differs from session binding");
    quests->quests().save_runtime(tick);
    if(owner.world_user()!=user || owner.character()!=character || user->actor.account_id()!=owner.account_id() ||
       registry.find_account(owner.account_id()).get()!=&owner || registry.retain_actor(user->actor).get()!=&owner)
        throw std::logic_error("Quest save owner changed during delivery");
}
// Implemented by the zone save wrapper that retains the actual save binding.
class VerticalQuestZoneSaveBindingIdentity {
public:
    virtual ~VerticalQuestZoneSaveBindingIdentity()=default;
    virtual const void* quest_save_binding_identity() const noexcept=0;
};
// Bind the coordinator's split save_quests callback to its real save binding.
// Lookup and binding must outlive this callback and run on the world strand.
template<class Binding> inline auto vertical_world_quests_save_phase_callback(VerticalWorldSession::Registry& registry,
    std::function<std::shared_ptr<Binding>(VerticalWorldSession&)> lookup) {
    if(!lookup) throw std::invalid_argument("Quest save requires the real session binding lookup");
    return [&registry,lookup=std::move(lookup)](VerticalWorldSession& owner,std::uint32_t tick,VerticalSaveCoreSkillServices& save) {
        auto binding=lookup(owner);
        if(!binding || !binding->matches(owner)) throw std::logic_error("Quest save binding belongs to another session");
        const auto* identity=dynamic_cast<const VerticalQuestZoneSaveBindingIdentity*>(&save);
        if(!identity || identity->quest_save_binding_identity()!=binding.get()) throw std::logic_error("Quest save binding differs from active zone save");
        auto* mutex=&binding->save_mutex();
        vertical_world_quests_save_phase(owner,registry,*mutex,tick);
        if(!binding->matches(owner) || identity->quest_save_binding_identity()!=binding.get() || &binding->save_mutex()!=mutex)
            throw std::logic_error("Quest save binding changed during delivery");
    };
}
// Configure the existing zone-service ports before constructing that service.
// Its own binding lookup remains authoritative for every save phase.
template<class Binding,class ZonePorts> inline void vertical_world_bind_quest_save_phase(ZonePorts& ports,VerticalWorldSession::Registry& registry) {
    if(!ports.binding || ports.quests || ports.save_quests) throw std::invalid_argument("Quest zone save requires one real binding and no existing quest provider");
    ports.save_quests=vertical_world_quests_save_phase_callback<Binding>(registry,ports.binding);
}
// Invoke from the real save owner before its DB exit request. The supplied
// services are the actual ordered save transport and locks, not a success shim.
inline std::uint32_t vertical_world_quests_save_character(VerticalWorldSession& owner,VerticalWorldSession::Registry& registry,
    UserSkillRuntime& skills,VerticalSaveCoreSkillServices& services,std::span<std::uint8_t,775> scratch) {
    auto* user=owner.world_user();auto* character=owner.character();
    if(!user || !character || &skills.actor()!=&user->actor) throw std::logic_error("Session save actor or character mismatch");
    auto runtime=vertical_world_quests_runtime_for_save(owner,registry);
    auto quest_session=owner.quests_lease();if(!quest_session) throw std::logic_error("Session save lost quest owner");
    struct QuestSaveServices final:VerticalSaveCoreSkillServices {
        VerticalWorldSession& owner;VerticalWorldSession::Registry& registry;WorldUser* user;VerticalSessionCharacter* character;
        Actor& actor;VerticalQuests& quests;VerticalSaveCoreSkillServices& upstream;
        QuestSaveServices(VerticalWorldSession& o,VerticalWorldSession::Registry& r,WorldUser* u,VerticalSessionCharacter* c,VerticalQuests& q,VerticalSaveCoreSkillServices& s)
            :owner(o),registry(r),user(u),character(c),actor(u->actor),quests(q),upstream(s) {}
        void validate() const {
            if(owner.world_user()!=user || owner.character()!=character || actor.account_id()!=owner.account_id() ||
               registry.find_account(owner.account_id()).get()!=&owner || registry.retain_actor(actor).get()!=&owner)
                throw std::logic_error("Session save owner changed during delivery");
        }
        UserPosition position(Actor& a) override {validate();auto value=upstream.position(a);validate();return value;}
        std::uint32_t tick_ms() override {validate();auto value=upstream.tick_ms();validate();return value;}
        void lock_effects() override {validate();upstream.lock_effects();try {validate();} catch(...) {upstream.unlock_effects();throw;}}
        void unlock_effects() override {upstream.unlock_effects();}
        void lock_quests() override {validate();quests.lock_save_for(actor);try {validate();} catch(...) {quests.unlock_save_for(actor);throw;}}
        void unlock_quests() override {quests.unlock_save_for(actor);}
        void send_database(const net::Packet& packet) override {
            validate();
            if(packet.size()>=2 && packet[0]==4 && packet[1]==10) quests.deliver_save_for(actor,packet);
            else upstream.send_database(packet);
            validate();
        }
    } routed(owner,registry,user,character,quest_session->quests(),services);
    const auto tick=vertical_save_character(user->actor,owner.zone_entry(),skills,*runtime,character->quick_slots(),routed,scratch);
    routed.validate();
    return tick;
}
// The death owner must compare the actual live death-user object, not merely
// its character ID. All lookups and the match callback run on the world strand.
class VerticalWorldQuestMobActors final:public VerticalQuestMobActorResolver {
public:
    using Match=std::function<bool(VerticalWorldSession&,MobDeathUser&)>;
    VerticalWorldQuestMobActors(VerticalWorldSession::Registry& registry,Match match):registry_(registry),match_(std::move(match)) {
        if(!match_) throw std::invalid_argument("Mob quest actor identity service is required");
    }
    std::shared_ptr<Actor> retain(MobDeathUser& user) override {
        const auto id=user.id;auto owner=registry_.find_character(id);
        if(!valid(owner,id) || !match_(*owner,user)) return {};
        if(user.id!=id || !valid(owner,id)) throw std::logic_error("Mob quest actor changed during identity lookup");
        return {owner,&owner->world_user()->actor};
    }
private:
    bool valid(const std::shared_ptr<VerticalWorldSession>& owner,std::uint32_t id) const {
        if(!owner || owner->closing() || owner->dispatch_failed() || owner->admission().location!=4 || !owner->world_user()) return false;
        auto& actor=owner->world_user()->actor;auto* zone=owner->admission().zone;
        return actor.id()==id && actor.account_id()==owner->account_id() && zone && zone->users &&
            zone->users->find_actor(id)==&actor && registry_.retain_actor(actor)==owner;
    }
    VerticalWorldSession::Registry& registry_;Match match_;
};
// Loot users are separate projections from death users. The actual loot
// owner must identify its live record; an equal character ID is insufficient.
class VerticalWorldQuestLootActors final:public VerticalQuestLootActorResolver {
public:
    using Match=std::function<bool(VerticalWorldSession&,MobLootUser&)>;
    VerticalWorldQuestLootActors(VerticalWorldSession::Registry& registry,Match match):registry_(registry),match_(std::move(match)) {
        if(!match_) throw std::invalid_argument("Loot quest actor identity service is required");
    }
    std::shared_ptr<Actor> retain(MobLootUser& user) override {
        const auto id=user.id;auto owner=registry_.find_character(id);
        if(!valid(owner,id) || !match_(*owner,user)) return {};
        if(user.id!=id || !valid(owner,id)) throw std::logic_error("Loot quest actor changed during identity lookup");
        return {owner,&owner->world_user()->actor};
    }
private:
    bool valid(const std::shared_ptr<VerticalWorldSession>& owner,std::uint32_t id) const {
        if(!owner || owner->closing() || owner->dispatch_failed() || owner->admission().location!=4 || !owner->world_user()) return false;
        auto& actor=owner->world_user()->actor;auto* zone=owner->admission().zone;
        return actor.id()==id && actor.account_id()==owner->account_id() && zone && zone->users &&
            zone->users->find_actor(id)==&actor && registry_.retain_actor(actor)==owner;
    }
    VerticalWorldSession::Registry& registry_;Match match_;
};
// Invoke after real old-zone interaction cleanup and destination admission,
// before enabling destination gameplay packets. Both zones remain retained.
inline void vertical_world_quests_rebind_zone(VerticalWorldSession& owner,RuntimeZone& source,RuntimeZone& destination,VerticalWorldSession::Registry& registry) {
    auto* user=owner.world_user();
    if(owner.closing() || owner.dispatch_failed() || !user || owner.admission().location!=4 || owner.admission().zone!=&destination ||
       !source.npcs || !destination.npcs || !destination.users || destination.users->find_actor(user->actor.id())!=&user->actor ||
       registry.retain_actor(user->actor).get()!=&owner)
        throw std::logic_error("Quest zone transfer requires the registered destination Actor");
    VerticalWorldQuestSessions sessions(registry);auto quests=sessions.retain(user->actor);
    if(!quests) throw std::logic_error("Quest zone transfer lost its session");
    quests->packets().rebind_npc_zone(*source.npcs,*destination.npcs);
}
// Bind as VerticalWorldSessionPacketRouter::OwnedDispatch. The caller and all
// owners use the world strand; false leaves unrelated families to the router.
class VerticalWorldQuestPacketDispatch final {
public:
    explicit VerticalWorldQuestPacketDispatch(VerticalWorldSession::Registry& registry):registry_(registry),sessions_(registry) {}
    bool operator()(VerticalWorldSession& owner,const net::Packet& packet) {
        if(packet.size()<2) throw std::invalid_argument("Malformed world quest packet");
        if(packet[1]!=9 && packet[1]!=14) return false;
        auto* user=owner.world_user();auto* zone=owner.admission().zone;
        if(owner.closing() || owner.dispatch_failed() || !user || owner.admission().location!=4 || !zone || !zone->users || !zone->npcs ||
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
struct VerticalWorldQuestPickupOwners {
    std::shared_ptr<void> lifetime;
    const ItemQuestLinks* links=nullptr;ItemPickupWorld* world=nullptr;GroundItemPool* pool=nullptr;VerticalInventoryActions* actions=nullptr;
};
// The lookup retains the actual zone/inventory owners through dispatch and
// must not mutate the Actor, packet, zone membership or attack state.
class VerticalWorldQuestPickupDispatch final {
public:
    using Lookup=std::function<VerticalWorldQuestPickupOwners(VerticalWorldSession&,RuntimeZone&)>;
    VerticalWorldQuestPickupDispatch(VerticalWorldSession::Registry& registry,Lookup lookup):registry_(registry),sessions_(registry),lookup_(std::move(lookup)) {
        if(!lookup_) throw std::invalid_argument("Quest pickup requires live owners");
    }
    bool operator()(VerticalWorldSession& owner,const net::Packet& packet) {
        if(packet.size()<2) throw std::invalid_argument("Malformed world pickup packet");
        if(packet[0]!=5 || packet[1]!=2) return false;
        if(packet.size()<6) throw std::invalid_argument("Malformed pickup request");
        auto* user=owner.world_user();auto* zone=owner.admission().zone;
        if(owner.closing() || owner.dispatch_failed() || !user || owner.admission().location!=4 || !zone || !zone->users || !zone->items ||
           zone->users->find_actor(user->actor.id())!=&user->actor || registry_.retain_actor(user->actor).get()!=&owner)
            throw std::logic_error("Pickup requires the registered admitted Actor");
        if(user->actor.attack_state().mode==1) return true;
        auto* items=zone->items.get();auto* users=zone->users.get();const auto attack_mode=user->actor.attack_state().mode;
        auto owners=lookup_(owner,*zone);
        if(!owners.lifetime || !owners.links || !owners.world || !owners.pool || !owners.actions)
            throw std::logic_error("Pickup owner lookup is incomplete");
        if(owner.closing() || owner.dispatch_failed() || owner.admission().location!=4 || owner.admission().zone!=zone || owner.world_user()!=user ||
           zone->users.get()!=users || zone->items.get()!=items || user->actor.attack_state().mode!=attack_mode ||
           users->find_actor(user->actor.id())!=&user->actor || registry_.retain_actor(user->actor).get()!=&owner)
            throw std::logic_error("Pickup owner changed during lookup");
        const auto result=vertical_quests_session_pickup_request(user->actor,packet,*items,*owners.links,*owners.world,*owners.pool,*owners.actions,sessions_);
        if(result.request==VerticalInventoryRequest::ignored || result.request==VerticalInventoryRequest::pickup) return true;
        if(result.request==VerticalInventoryRequest::malformed) throw std::invalid_argument("Malformed pickup request");
        throw std::logic_error("Pickup request was not handled");
    }
private:
    VerticalWorldSession::Registry& registry_;VerticalWorldQuestSessions sessions_;Lookup lookup_;
};
class VerticalWorldPrimaryQuestServices final:public PrimaryQuestPhaseServices {
public:
    explicit VerticalWorldPrimaryQuestServices(VerticalWorldSession::Registry& registry):registry_(registry),sessions_(registry) {}
    std::shared_ptr<void> retain(Actor& actor) override {return sessions_.retain(actor);}
    bool eligible(Actor& actor) override {
        auto owner=registry_.retain_actor(actor);
        return owner && !owner->closing() && !owner->dispatch_failed() && owner->world_user() && &owner->world_user()->actor==&actor;
    }
    void failure(Actor& actor,std::exception_ptr error) override {
        auto owner=registry_.retain_actor(actor);
        if(!owner || !owner->world_user() || &owner->world_user()->actor!=&actor)
            throw std::logic_error("Primary quest failure lost its actor");
        owner->report_dispatch_failure(error);
    }
    VerticalQuests& quests(Actor& actor) override {
        auto owner=registry_.retain_actor(actor);
        if(!owner || !owner->world_user() || &owner->world_user()->actor!=&actor)
            throw std::logic_error("Primary quest actor is not retained");
        if(owner->closing() || owner->dispatch_failed()) throw std::logic_error("Primary quest session is closing or has failed");
        auto quests=owner->quests_lease();
        if(!quests || !quests->quests().owns_actor(actor)) throw std::logic_error("Primary quest session does not match actor");
        return quests->quests();
    }
private:
    VerticalWorldSession::Registry& registry_;
    VerticalWorldQuestSessions sessions_;
};
}
