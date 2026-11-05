#include "eternity/game/vertical_quests.hpp"
#include "eternity/game/zone_leave_interactions.hpp"
#include "eternity/game/quest_runtime_save.hpp"
#include "eternity/game/character_replica.hpp"
#include "eternity/game/vertical_session_character.hpp"
#include <bit>
#include <limits>
#include <stdexcept>
namespace eternity::game {
namespace {
struct QuestCallDepth {
    std::size_t& value;
    explicit QuestCallDepth(std::size_t& depth):value(depth) {if(value==std::numeric_limits<std::size_t>::max()) throw std::overflow_error("Quest call depth exceeded");++value;}
    ~QuestCallDepth() {--value;}
};
QuestStartItemsActorWorldOwners checked(QuestStartItemsActorWorldOwners o) {
    if(!o.items || !o.item_pool || !o.ids || !o.quests || !o.quest_pool || !o.actor || !o.runtime) throw std::invalid_argument("Quest owner missing");
    if(!o.runtime->bound_to(o.actor->account_id(),*o.quest_pool)) throw std::invalid_argument("Quest runtime account or pool mismatch");
    return o;
}
}
VerticalQuestRestoreResult vertical_quests_restore_runtime(const RestoredQuests& restored,std::uint32_t account,
    std::shared_ptr<QuestRuntimePool> pool,std::shared_ptr<const QuestDefinitions> definitions) {
    if(!pool || !definitions) throw std::invalid_argument("Quest restore requires pool and definitions");
    for(const auto& quest:restored.active)
        if(!quest.definition || definitions->find(quest.packet.id)!=quest.definition)
            throw std::invalid_argument("Restored active quest uses different definitions");
    struct Owner {
        std::shared_ptr<QuestRuntimePool> pool;
        std::shared_ptr<const QuestDefinitions> definitions;
        QuestRuntime runtime;
        Owner(std::uint32_t id,std::shared_ptr<QuestRuntimePool> p,std::shared_ptr<const QuestDefinitions> d)
            :pool(std::move(p)),definitions(std::move(d)),runtime(id,*pool) {}
    };
    auto owner=std::make_shared<Owner>(account,std::move(pool),std::move(definitions));
    VerticalQuestRestoreResult result{std::shared_ptr<QuestRuntime>(owner,&owner->runtime)};
    for(const auto& quest:restored.active) if(!owner->runtime.load_active(quest)) ++result.skipped_active;
    for(const auto& finished:restored.finished) {
        const auto* definition=owner->definitions->find(finished.id);
        if(!definition) continue; // 0507 silently skips an invalid definition.
        auto* quest=owner->pool->allocate();
        if(!quest) {++result.skipped_finished;continue;}
        // 0507 writes only definition, ID and success into the recycled CQuest.
        quest->definition=definition;quest->packet.id=finished.id;quest->success=finished.success;
        try {owner->runtime.add_finished(*quest);} catch(...) {owner->pool->release(*quest);throw;}
    }
    return result;
}
VerticalQuestReplicaRestoreResult vertical_quests_restore_replica(const CharacterReplica& replica,std::uint32_t tick,
    std::shared_ptr<QuestRuntimePool> pool,std::shared_ptr<const QuestDefinitions> definitions) {
    if(!pool || !definitions) throw std::invalid_argument("Quest replica restore requires pool and definitions");
    PlayerSnapshot rows;rows.core=replica.core();
    for(const auto& quest:replica.quests().active()) rows.quests.push_back({quest.id,quest.delay,quest.counters});
    for(const auto& quest:replica.quests().finished()) rows.finished_quests.push_back({quest.id,quest.success});
    auto projected=restore_quests(rows,*definitions,tick);
    auto restored=vertical_quests_restore_runtime(projected,rows.core.user_id,std::move(pool),std::move(definitions));
    return {std::move(restored),std::move(projected.invalid_active)};
}
VerticalQuestSessionRestoreResult vertical_quests_restore_session(std::shared_ptr<void> lease,VerticalSessionCharacter& character,
    std::recursive_mutex& replica_mutex,std::uint32_t tick,std::shared_ptr<QuestRuntimePool> pool,std::shared_ptr<const QuestDefinitions> definitions) {
    if(!lease || !pool || !definitions) throw std::invalid_argument("Quest session restore requires retained owners");
    auto* actor=character.pipeline().result().actor.get();
    if(!actor) throw std::invalid_argument("Quest session restore requires the loaded actor");
    std::lock_guard lock(replica_mutex);const auto core=character.replica().core();
    if(actor->account_id()!=core.user_id || actor->id()!=core.character.id) throw std::invalid_argument("Quest session actor and DB replica differ");
    auto restored=vertical_quests_restore_replica(character.replica(),tick,std::move(pool),std::move(definitions));
    if(actor->account_id()!=core.user_id || actor->id()!=core.character.id) throw std::logic_error("Quest session actor changed during restoration");
    return {std::shared_ptr<Actor>(std::move(lease),actor),std::move(restored)};
}
VerticalQuestEntryRows vertical_quests_entry_rows(const QuestRuntime& runtime) {
    VerticalQuestEntryRows rows;
    rows.active.reserve(runtime.active().size());
    for(const auto* quest:runtime.active()) rows.active.push_back(quest->packet);
    {std::lock_guard lock(runtime.finished_mutex());rows.finished.reserve(runtime.finished().size());for(const auto* quest:runtime.finished()) rows.finished.push_back({quest->packet.id,quest->success});}
    return rows;
}
bool vertical_quests_send_entry_step(QuestRuntime& runtime,CharacterEntryStep step,VerticalQuestEntryPorts& ports) {
    if(step==CharacterEntryStep::quests) {
        const auto tick=ports.tick_ms();std::vector<QuestPacketEntry> batch;batch.reserve(255);
        ports.lock_active();
        try {
            for(auto* quest=runtime.first_active();quest;) {
                if(!quest->definition) throw std::logic_error("Entry quest lacks definition");
                auto row=quest->packet;row.timed=quest->definition->maximum_minutes()!=0;batch.push_back(row);
                quest=runtime.next_active();
                if(batch.size()==255) {ports.send_self(character_quests(batch,tick).front());batch.clear();}
            }
        } catch(...) {ports.unlock_active();throw;}
        ports.unlock_active();
        if(!batch.empty()) ports.send_self(character_quests(batch,tick).front());
        return true;
    }
    if(step==CharacterEntryStep::finished_quests) {
        std::vector<FinishedQuest> batch;batch.reserve(255);ports.lock_finished();
        try {
            for(const auto* quest:runtime.finished()) {
                batch.push_back({quest->packet.id,quest->success});
                if(batch.size()==255) {ports.send_self(character_finished_quests(batch).front());batch.clear();}
            }
        } catch(...) {ports.unlock_finished();throw;}
        ports.unlock_finished();
        if(!batch.empty()) ports.send_self(character_finished_quests(batch).front());
        return true;
    }
    return false;
}
VerticalQuestRewardItems::VerticalQuestRewardItems(Actor& actor,QuestStartItemsActor& items,ItemName name):items_(items),name_(std::move(name)) {
    if(!items_.owns_actor(actor) || !name_) throw std::invalid_argument("Quest reward item requires the matching actor and real name provider");
}
VerticalQuestRewardExperience::VerticalQuestRewardExperience(Actor& actor,PlayerCore& player,const StatusRules& rules,const Experience& curve,ExperienceAwardPorts& ports,Options options)
    :actor_(actor),player_(player),rules_(rules),curve_(curve),ports_(ports),options_(std::move(options)) {
    if(!options_ || player_.character.id!=actor_.id() || player_.user_id!=actor_.account_id()) throw std::invalid_argument("Quest experience requires the bound actor and live options");
}
void VerticalQuestRewardExperience::award(std::uint32_t source,std::int32_t amount,bool skip_modifiers) {
    if(player_.character.id!=actor_.id() || player_.user_id!=actor_.account_id()) throw std::invalid_argument("Quest experience actor binding changed");
    auto options=options_();options.skip_modifiers=skip_modifiers;
    if(player_.character.id!=actor_.id() || player_.user_id!=actor_.account_id()) throw std::invalid_argument("Quest experience actor binding changed during options lookup");
    actor_.gain_experience(player_,rules_,curve_,source,amount,options,ports_);
}
std::optional<QuestRewardItemReceipt> VerticalQuestRewardItems::receive(std::uint8_t type,std::uint8_t id,std::uint8_t count) {
    QuestStartItemsResult result;
    if(!items_.add(type,id,count,result)) return {};
    if(!result.definition || result.bag<1 || result.bag>9 || result.slot<0 || result.slot>=24) throw std::logic_error("Quest reward item returned invalid ownership");
    return QuestRewardItemReceipt{static_cast<std::uint8_t>(result.bag),static_cast<std::uint8_t>(result.slot),result.definition,name_(*result.definition)};
}
VerticalQuestRewardComposition::VerticalQuestRewardComposition(Actor& actor,VerticalQuestRewardExperience& experience,VerticalQuestRewardItems& items,VerticalQuestPorts& ports)
    :actor_(actor),experience_(experience),items_(items),ports_(ports) {
    if(!experience_.owns_actor(actor_) || !items_.owns_actor(actor_)) throw std::invalid_argument("Quest reward components require the same actor");
}
void VerticalQuestRewardComposition::add_experience(std::uint32_t source,std::int32_t amount,bool bypass) {experience_.award(source,amount,bypass);}
void VerticalQuestRewardComposition::send_database(const net::Packet& packet) {ports_.send_database(actor_,packet);}
void VerticalQuestRewardComposition::fill_game_log_main(std::span<std::uint8_t> packet) {ports_.fill_game_log_main(actor_,packet);}
std::optional<QuestRewardItemReceipt> VerticalQuestRewardComposition::receive_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) {return items_.receive(type,id,count);}
void VerticalQuestRewardComposition::send_self(const net::Packet& packet) {actor_.send_self(packet);}
void VerticalQuestRewardComposition::send_game_log(const net::Packet& packet) {ports_.send_game_log(actor_,packet);}
VerticalQuestRewardBinding::VerticalQuestRewardBinding(Actor& actor,QuestRuntime& runtime,QuestRuntimePool& pool,VerticalQuestRewardServices& services)
    :actor_(actor),runtime_(runtime),pool_(pool),services_(services) {
    if(!runtime_.bound_to(actor_.account_id(),pool_)) throw std::invalid_argument("Quest reward runtime account or pool mismatch");
    if(const auto* composed=dynamic_cast<const VerticalQuestRewardComposition*>(&services_);composed && !composed->owns_actor(actor_)) throw std::invalid_argument("Quest reward composition actor mismatch");
}
void VerticalQuestRewardBinding::validate() const {
    if(!runtime_.bound_to(actor_.account_id(),pool_)) throw std::logic_error("Quest reward runtime account or pool changed");
}
void VerticalQuestRewardBinding::add_experience(std::uint32_t source,std::int32_t amount,bool bypass) {validate();services_.add_experience(source,amount,bypass);}
void VerticalQuestRewardBinding::send_database(const net::Packet& packet) {validate();services_.send_database(packet);}
void VerticalQuestRewardBinding::fill_game_log_main(std::span<std::uint8_t> packet) {validate();services_.fill_game_log_main(packet);}
std::optional<QuestRewardItemReceipt> VerticalQuestRewardBinding::receive_item(std::uint8_t type,std::uint8_t id,std::uint8_t count) {validate();return services_.receive_item(type,id,count);}
void VerticalQuestRewardBinding::send_self(const net::Packet& packet) {validate();services_.send_self(packet);}
void VerticalQuestRewardBinding::remove_quest(RestoredQuest& quest,bool success) {validate();runtime_.remove(quest,success,*this);}
void VerticalQuestRewardBinding::send_game_log(const net::Packet& packet) {validate();services_.send_game_log(packet);}
VerticalQuests::VerticalQuests(QuestStartItemsActorWorldOwners o,QuestStartItemsActorUseState use,ZoneNpcs& zone,NpcPool& pool,VerticalQuestPorts& p)
    :owners_(checked(std::move(o))),npcs_(&zone),npc_pool_(pool),ports_(p),requests_(*owners_.actor,*owners_.runtime,*owners_.quests,*this),starts_(owners_,use,*this) {}
void VerticalQuests::validate_binding() const {
    if(!owners_.runtime->bound_to(owners_.actor->account_id(),*owners_.quest_pool)) throw std::logic_error("Quest runtime account or pool changed");
}
VerticalQuestNpcPackets::VerticalQuestNpcPackets(Actor& actor,VerticalQuests& quests,ZoneNpcs& zone,VerticalNpcInteraction& interaction,VerticalQuestPacketActions& quest_actions,VerticalNpcPacketActions& npc_actions)
    :actor_(actor),quests_(quests),zone_(&zone),interaction_(interaction),quest_actions_(quest_actions),npc_actions_(npc_actions) {
    if(!quests_.owns_actor(actor_) || !quests_.owns_npc_zone(*zone_)) throw std::invalid_argument("Quest/NPC packets require the same actor and zone");
    synchronize_interaction();
}
void VerticalQuestNpcPackets::synchronize_interaction() {
    if(npc_lease_.get()==interaction_.npc) return;
    NpcPool::Lease next;
    if(interaction_.npc) next=quests_.npc_pool_.pin(*interaction_.npc);
    npc_lease_=std::move(next);
}
void VerticalQuestNpcPackets::move_interactions(bool wide,VerticalMoveInteractionServices& services) {
    QuestCallDepth guard(dispatch_depth_);
    synchronize_interaction();
    try {vertical_move_interactions(actor_,interaction_,wide,services);}
    catch(...) {if(!interaction_.npc) npc_lease_=NpcPool::Lease{};throw;}
    synchronize_interaction();
}
void VerticalQuestNpcPackets::leave_interactions(ZoneLeaveShopState& shop,ZoneLeaveInteractionServices& services) {
    QuestCallDepth guard(dispatch_depth_);
    synchronize_interaction();
    try {zone_leave_interactions(actor_,interaction_,shop,services);}
    catch(...) {if(!interaction_.npc) npc_lease_=NpcPool::Lease{};throw;}
    synchronize_interaction();
}
void VerticalQuestNpcPackets::rebind_npc_zone(ZoneNpcs& expected,ZoneNpcs& destination) {
    if(zone_!=&expected || !quests_.owns_npc_zone(expected)) throw std::logic_error("Quest NPC source zone changed");
    if(dispatch_depth_ || quests_.operation_depth_ || quests_.frame_ || interaction_.npc) throw std::logic_error("Quest NPC zone is still in use");
    synchronize_interaction();
    if(npc_lease_.get()) throw std::logic_error("Quest NPC interaction lease remains");
    zone_=&destination;quests_.npcs_=&destination;
}
VerticalQuestNpcPacketResult VerticalQuestNpcPackets::receive(std::span<const std::uint8_t> packet) {
    QuestCallDepth guard(dispatch_depth_);
    synchronize_interaction();
    if(packet.size()<2) return VerticalQuestNpcPacketResult::malformed;
    if(packet[1]==9) {
        switch(quests_.receive(packet,quest_actions_)) {
        case VerticalQuestPacketResult::unhandled:return VerticalQuestNpcPacketResult::unhandled;
        case VerticalQuestPacketResult::malformed:return VerticalQuestNpcPacketResult::malformed;
        case VerticalQuestPacketResult::handled:return VerticalQuestNpcPacketResult::handled;
        case VerticalQuestPacketResult::closed:return VerticalQuestNpcPacketResult::closed;
        }
    }
    if(packet[1]==14) {
        // The original successful 0e04 branch has no intervening callbacks.
        // Acquire the native lifetime lease before changing the shared pointer
        // and use counts, so an invalid pool or allocation failure is harmless.
        NpcPool::Lease next;
        if(packet[0]==4 && packet.size()>=6) {
            std::uint32_t id=0;for(unsigned i=0;i<4;++i) id|=std::uint32_t(packet[2+i])<<(8*i);
            if(auto* target=zone_->find(id);target && target!=interaction_.npc) next=quests_.npc_pool_.pin(*target);
        }
        const auto result=vertical_npc_request(actor_,packet,*zone_,interaction_,npc_actions_);
        if(next.get()) {
            if(interaction_.npc!=next.get()) throw std::logic_error("NPC interaction changed during selection");
            npc_lease_=std::move(next);
        } else synchronize_interaction();
        switch(result) {
        case VerticalNpcPacketResult::unhandled:return VerticalQuestNpcPacketResult::unhandled;
        case VerticalNpcPacketResult::malformed:return VerticalQuestNpcPacketResult::malformed;
        case VerticalNpcPacketResult::handled:return VerticalQuestNpcPacketResult::handled;
        case VerticalNpcPacketResult::closed:return VerticalQuestNpcPacketResult::closed;
        }
    }
    return VerticalQuestNpcPacketResult::unhandled;
}
bool VerticalQuests::active(std::uint16_t quest) {QuestCallDepth guard(operation_depth_);validate_binding();return requests_.find_quest(quest)!=nullptr;}
VerticalQuestPickupWorld::VerticalQuestPickupWorld(Actor& actor,VerticalQuests& quests,ItemPickupWorld& world):actor_(actor),quests_(quests),world_(world) {
    if(!quests_.owns_actor(actor_)) throw std::invalid_argument("Quest pickup requires the bound actor");
}
bool VerticalQuestPickupWorld::pick_quest_active(Actor& actor,std::uint16_t id) {
    if(&actor!=&actor_) throw std::invalid_argument("Quest pickup actor changed");
    return quests_.active(id);
}
void vertical_quests_kill_pc_to(Actor& killer,Actor& victim,VerticalQuests& quests,VerticalQuestKillPcContinuation& continuation) {
    if(!quests.owns_actor(killer)) throw std::invalid_argument("Player kill requires the killer's quest owner");
    quests.kill_player();continuation.after_quest(killer,victim);
}
VerticalNpcPacketResult vertical_npc_request(Actor& actor,std::span<const std::uint8_t> packet,
    ZoneNpcs& zone,VerticalNpcInteraction& interaction,VerticalNpcPacketActions& actions) {
    if(packet.size()<2) return VerticalNpcPacketResult::malformed;
    if(packet[1]!=0x0e) return VerticalNpcPacketResult::unhandled;
    if(packet[0]!=0x04) {actions.close_connection(actor,9);return VerticalNpcPacketResult::closed;}
    if(packet.size()<6) return VerticalNpcPacketResult::malformed;
    std::uint32_t id=0;for(unsigned i=0;i<4;++i) id|=std::uint32_t(packet[2+i])<<(8*i);
    auto* npc=zone.find(id);if(!npc || npc==interaction.npc) return VerticalNpcPacketResult::handled;
    if(auto* previous=interaction.npc) previous->users_using=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(previous->users_using)-1U);
    npc->users_using=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(npc->users_using)+1U);
    interaction.npc=npc;return VerticalNpcPacketResult::handled;
}
bool VerticalQuests::start(std::uint16_t row,std::uint32_t npc) {
    QuestCallDepth guard(operation_depth_);validate_binding();StartFrame frame;auto* previous=frame_;frame_=&frame;
    try {const bool result=starts_.request(row,npc);frame_=previous;return result;} catch(...) {frame_=previous;throw;}
}
void VerticalQuests::end(std::uint16_t row,std::uint32_t npc) {
    QuestCallDepth guard(operation_depth_);validate_binding();if(owners_.quests->size()>static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max())) throw std::overflow_error("Quest catalog too large");
    quest_end_request(row,npc,static_cast<std::int32_t>(owners_.quests->size()),requests_);
}
void VerticalQuests::select_reward(std::uint16_t id,std::uint8_t selection) {QuestCallDepth guard(operation_depth_);validate_binding();quest_requests_complete_selection(id,selection,requests_);}
void VerticalQuests::abandon(std::uint16_t id) {
    QuestCallDepth guard(operation_depth_);validate_binding();if(auto* quest=requests_.find_quest(id)) requests_.complete_quest(*quest,0,false);
}
VerticalQuestPacketResult VerticalQuests::receive(std::span<const std::uint8_t> packet,VerticalQuestPacketActions& actions) {
    if(packet.size()<2) return VerticalQuestPacketResult::malformed;
    if(packet[1]!=9) return VerticalQuestPacketResult::unhandled;
    QuestCallDepth guard(operation_depth_);validate_binding();actions.cancel_action_exchange(*owners_.actor);
    actions.end_shop(*owners_.actor);
    const auto word=[&](std::size_t at) {return static_cast<std::uint16_t>(packet[at]|(std::uint16_t(packet[at+1])<<8));};
    const auto dword=[&](std::size_t at) {std::uint32_t value=0;for(unsigned i=0;i<4;++i) value|=std::uint32_t(packet[at+i])<<(8*i);return value;};
    switch(word(0)) {
    case 0x902:case 0x904:
        if(packet.size()<8) return VerticalQuestPacketResult::malformed;
        (void)start(word(6),dword(2));break;
    case 0x903:
        if(packet.size()<8) return VerticalQuestPacketResult::malformed;
        end(word(6),dword(2));break;
    case 0x907:
        if(packet.size()<5) return VerticalQuestPacketResult::malformed;
        select_reward(word(2),packet[4]);break;
    case 0x908:
        if(packet.size()<4) return VerticalQuestPacketResult::malformed;
        abandon(word(2));break;
    default:
        actions.close_connection(*owners_.actor,9);return VerticalQuestPacketResult::closed;
    }
    return VerticalQuestPacketResult::handled;
}
void VerticalQuests::kill_mob(std::uint16_t mob,const MobQuestLinks& links) {QuestCallDepth guard(operation_depth_);validate_binding();update_mob_kill_quests(owners_.actor->account_id(),owners_.runtime->mob_kill_quests(),mob,links,*this);}
void VerticalQuests::kill_player() {QuestCallDepth guard(operation_depth_);validate_binding();quest_requests_progress_kill_pc(owners_.actor->account_id(),owners_.runtime->player_kill_quests(),requests_);}
void VerticalQuests::pick_item(std::int32_t type,std::int32_t id,const ItemQuestLinks& links) {QuestCallDepth guard(operation_depth_);validate_binding();quest_requests_progress_pick_item(owners_.actor->account_id(),type,id,links,requests_);}
void VerticalQuests::update_timers(std::uint32_t tick) {QuestCallDepth guard(operation_depth_);validate_binding();quest_requests_update_timers(tick,requests_);}
void VerticalQuests::save_runtime(std::uint32_t tick) {
    QuestCallDepth guard(operation_depth_);validate_binding();const auto account=owners_.actor->account_id();
    quest_runtime_save(*owners_.runtime,account,tick,{
        [&]{ports_.lock_quests(*owners_.actor);try {validate_binding();} catch(...) {ports_.unlock_quests(*owners_.actor);throw;}},
        [&]{ports_.unlock_quests(*owners_.actor);},
        [&](const net::Packet& packet){send_database(*owners_.actor,packet);}});
}
bool VerticalQuests::send_entry_step(CharacterEntryStep step,VerticalQuestEntryFinishedLock& finished) {
    if(step!=CharacterEntryStep::quests && step!=CharacterEntryStep::finished_quests) return false;
    QuestCallDepth guard(operation_depth_);validate_binding();
    struct Bridge final:VerticalQuestEntryPorts {
        Actor& actor;QuestRuntime& runtime;QuestRuntimePool& pool;VerticalQuestPorts& ports;VerticalQuestEntryFinishedLock& finished;
        Bridge(Actor& a,QuestRuntime& r,QuestRuntimePool& p,VerticalQuestPorts& s,VerticalQuestEntryFinishedLock& f):actor(a),runtime(r),pool(p),ports(s),finished(f) {}
        void validate() const {if(!runtime.bound_to(actor.account_id(),pool)) throw std::logic_error("Quest entry runtime account or pool changed");}
        std::uint32_t tick_ms() override {validate();const auto tick=ports.quest_tick_ms();validate();return tick;}
        void lock_active() override {ports.lock_quests(actor);try {validate();} catch(...) {ports.unlock_quests(actor);throw;}}
        void unlock_active() noexcept override {ports.unlock_quests(actor);}
        void lock_finished() override {finished.lock_finished(actor);try {validate();} catch(...) {finished.unlock_finished(actor);throw;}}
        void unlock_finished() noexcept override {finished.unlock_finished(actor);}
        void send_self(const net::Packet& packet) override {validate();ports.send_client(actor,packet);}
    } bridge(*owners_.actor,*owners_.runtime,*owners_.quest_pool,ports_,finished);
    return vertical_quests_send_entry_step(*owners_.runtime,step,bridge);
}
void VerticalQuests::update_timers_for(Actor& actor,std::uint32_t tick) {
    if(!owns_actor(actor)) throw std::logic_error("Zone quest owner does not match actor");
    update_timers(tick);
}
QuestRuntime& VerticalQuests::runtime_for(Actor& actor) {
    if(!owns_actor(actor)) throw std::logic_error("Quest runtime actor mismatch");
    validate_binding();return *owners_.runtime;
}
void VerticalQuests::lock_save_for(Actor& actor) {
    (void)runtime_for(actor);ports_.lock_quests(actor);
    try {(void)runtime_for(actor);} catch(...) {ports_.unlock_quests(actor);throw;}
}
void VerticalQuests::unlock_save_for(Actor& actor) noexcept {
    if(!owns_actor(actor)) std::terminate();ports_.unlock_quests(actor);
}
void VerticalQuests::deliver_save_for(Actor& actor,const net::Packet& packet) {
    (void)runtime_for(actor);
    if(packet.size()<2 || packet[0]!=4 || packet[1]!=10) throw std::invalid_argument("Quest save expected 0a04 packet");
    ports_.send_database(actor,packet);
}
std::optional<QuestStartNpc> VerticalQuests::find_start_npc(Actor&,std::uint32_t id) {auto* n=npcs_->find(id);if(!n) return {};frame_->npc=npc_pool_.pin(*n);return QuestStartNpc{n->id,static_cast<std::uint32_t>(n->type),static_cast<std::uint32_t>(n->type_id)};}
bool VerticalQuests::require_start_item(Actor&,std::uint8_t type,std::uint8_t id,bool consume) {return requests_.find_item(type,id,consume);}
QuestStartLogContext VerticalQuests::start_log_context(Actor& a) {return ports_.start_log_context(a);}
QuestStartLogNpc VerticalQuests::start_log_npc(Actor&,const QuestStartNpc& npc) {auto* n=frame_->npc.get();if(!n || n->id!=npc.handle || !n->definition) throw std::logic_error("Quest start NPC lost its definition");frame_->name=n->definition->name();return {static_cast<std::uint32_t>(n->type),frame_->name};}
void VerticalQuests::send_start_log(Actor& a,const std::array<std::uint8_t,144>& p) {ports_.send_start_log(a,p);}
std::uint32_t VerticalQuests::local_make_time(Actor& a) {return ports_.local_make_time(a);}
std::uint32_t VerticalQuests::tick_ms(Actor& a) {return ports_.tick_ms(a);}
void VerticalQuests::send_database(Actor& a,const net::Packet& p) {validate_binding();ports_.send_database(a,p);}
void VerticalQuests::send_client(Actor& a,const net::Packet& p) {ports_.send_client(a,p);}
void VerticalQuests::lock_quests(Actor& a) {ports_.lock_quests(a);}
void VerticalQuests::unlock_quests(Actor& a) noexcept {ports_.unlock_quests(a);}
std::optional<QuestRequestNpc> VerticalQuests::find_npc(Actor&,std::uint32_t id) {auto* n=npcs_->find(id);if(!n) return {};return QuestRequestNpc{n->type,n->type_id};}
void VerticalQuests::fill_game_log_main(Actor& a,std::span<std::uint8_t> p) {ports_.fill_game_log_main(a,p);}
void VerticalQuests::send_game_log(Actor& a,const net::Packet& p) {ports_.send_game_log(a,p);}
QuestRewardWorld& VerticalQuests::reward_world(Actor& a) {return ports_.reward_world(a);}
std::uint32_t VerticalQuests::tick_ms() {return ports_.quest_tick_ms();}
RestoredQuest* VerticalQuests::find_quest(std::uint16_t id) {return requests_.find_quest(id);}
void VerticalQuests::send_database(const net::Packet& p) {requests_.send_database(p);}
void VerticalQuests::send_self(const net::Packet& p) {requests_.send_self(p);}
void VerticalQuests::complete_quest(RestoredQuest& q,std::int32_t npc,bool success) {requests_.complete_quest(q,static_cast<std::uint32_t>(npc),success);}
std::shared_ptr<VerticalQuests> VerticalQuestSessionZoneUsers::retain_quests(Actor& actor) {
    auto session=resolver_.retain(actor);
    if(!session || !session->quests().owns_actor(actor)) throw std::logic_error("Zone quest session does not match actor");
    auto* quests=&session->quests();return std::shared_ptr<VerticalQuests>(std::move(session),quests);
}
VerticalQuestNpcPacketResult vertical_quests_session_receive(Actor& actor,std::span<const std::uint8_t> packet,VerticalQuestSessionResolver& resolver) {
    if(packet.size()<2) return VerticalQuestNpcPacketResult::malformed;
    if(packet[1]!=9 && packet[1]!=14) return VerticalQuestNpcPacketResult::unhandled;
    auto session=resolver.retain(actor);
    if(!session || !session->quests().owns_actor(actor)) throw std::logic_error("Quest/NPC session does not match actor");
    return session->packets().receive(packet);
}
void vertical_quests_session_move_interactions(Actor& actor,bool wide,VerticalMoveInteractionServices& services,VerticalQuestSessionResolver& resolver) {
    auto session=resolver.retain(actor);
    if(!session || !session->quests().owns_actor(actor)) throw std::logic_error("Movement quest session does not match actor");
    session->packets().move_interactions(wide,services);
}
void vertical_quests_session_leave_interactions(Actor& actor,ZoneLeaveShopState& shop,ZoneLeaveInteractionServices& services,VerticalQuestSessionResolver& resolver) {
    auto session=resolver.retain(actor);
    if(!session || !session->quests().owns_actor(actor)) throw std::logic_error("Leaving quest session does not match actor");
    session->packets().leave_interactions(shop,services);
}
bool vertical_quests_session_send_entry_step(Actor& actor,CharacterEntryStep step,VerticalQuestEntryFinishedLock& finished,VerticalQuestSessionResolver& resolver) {
    if(step!=CharacterEntryStep::quests && step!=CharacterEntryStep::finished_quests) return false;
    auto session=resolver.retain(actor);
    if(!session || !session->quests().owns_actor(actor)) throw std::logic_error("Entry quest session does not match actor");
    return session->quests().send_entry_step(step,finished);
}
void vertical_quests_session_save_runtime(Actor& actor,std::uint32_t tick,VerticalQuestSessionResolver& resolver) {
    auto session=resolver.retain(actor);
    if(!session || !session->quests().owns_actor(actor)) throw std::logic_error("Saving quest session does not match actor");
    session->quests().save_runtime(tick);
}
VerticalInventoryResult vertical_quests_session_pickup_request(Actor& actor,std::span<const std::uint8_t> packet,
    ZoneItems& zone,const ItemQuestLinks& links,ItemPickupWorld& world,GroundItemPool& pool,
    VerticalInventoryActions& actions,VerticalQuestSessionResolver& resolver) {
    if(packet.size()<2) return {VerticalInventoryRequest::malformed,{}};
    if(packet[0]!=0x05 || packet[1]!=0x02) return {VerticalInventoryRequest::unhandled,{}};
    if(packet.size()<6) return {VerticalInventoryRequest::malformed,{}};
    if(actor.attack_state().mode==1) return {VerticalInventoryRequest::ignored,{}};
    auto session=resolver.retain(actor);
    if(!session || !session->quests().owns_actor(actor)) throw std::logic_error("Pickup quest session does not match actor");
    VerticalQuestPickupWorld pickup(actor,session->quests(),world);
    return vertical_inventory_request(actor,packet,zone,links,pickup,pool,actions);
}
void vertical_quests_session_kill_pc_to(Actor& killer,Actor& victim,VerticalQuestSessionResolver& resolver,VerticalQuestKillPcContinuation& continuation) {
    auto session=resolver.retain(killer);
    if(!session || !session->quests().owns_actor(killer)) throw std::logic_error("Player kill quest session does not match killer");
    vertical_quests_kill_pc_to(killer,victim,session->quests(),continuation);
}
void VerticalQuestSessionMobCredit::apply(MobDeathUser& user,MobRuntime& mob) {
    auto actor=actors_.retain(user);
    if(!actor || actor->id()!=user.id) throw std::logic_error("Mob quest actor does not match credited user");
    auto session=sessions_.retain(*actor);
    if(!session || actor->id()!=user.id || !session->quests().owns_actor(*actor)) throw std::logic_error("Mob quest session does not match credited actor");
    session->quests().kill_mob(mob.type,links_);
}
std::span<const std::uint16_t> VerticalQuestSessionLoot::end_quests(const MobLootItem& item) const {
    const auto* linked=links_.find(item.type,item.type_id);
    return linked?std::span<const std::uint16_t>(linked->end):std::span<const std::uint16_t>{};
}
bool VerticalQuestSessionLoot::has_quest(MobLootUser& user,std::uint16_t id) {
    auto actor=actors_.retain(user);
    if(!actor || actor->id()!=user.id) throw std::logic_error("Quest loot actor does not match user");
    auto session=sessions_.retain(*actor);
    if(!session || actor->id()!=user.id || !session->quests().owns_actor(*actor)) throw std::logic_error("Quest loot session does not match actor");
    return session->quests().active(id);
}
void vertical_quests_update_zone(ZoneUsers& zone,std::uint32_t tick,VerticalQuestZoneUsers& users) {
    std::shared_ptr<VerticalQuests> current;
    zone.visit_all([&](Actor& actor) -> std::shared_ptr<void> {
        current=users.retain_quests(actor);
        return current;
    },[&](Actor& actor) {
        auto owner=std::move(current);
        if(!owner) throw std::logic_error("Zone quest owner is missing");
        owner->update_timers_for(actor,tick);
    });
}
void VerticalQuestMobDeath::quest_kill(MobDeathUser& user,MobRuntime& mob) {
    if(credit_) {credit_->apply(user,mob);return;}
    auto& quests=users_->quests_for(user);
    if(quests.actor_id()!=user.id) throw std::logic_error("Mob reward quest owner does not match credited user");
    quests.kill_mob(mob.type,*links_);
}
class VerticalQuestSession::Ports final:public VerticalQuestPorts {
public:
    Ports(Actor& actor,VerticalQuestPorts& upstream):actor_(actor),upstream_(upstream) {}
    void bind(QuestRewardWorld& reward) {if(reward_) throw std::logic_error("Quest session reward already bound");reward_=&reward;}
    bool uses(const VerticalQuestPorts& ports) const noexcept {return &upstream_==&ports;}
    const VerticalQuestPorts& upstream() const noexcept {return upstream_;}
    std::uint32_t local_make_time(Actor& a) override {check(a);return upstream_.local_make_time(a);}
    std::uint32_t tick_ms(Actor& a) override {check(a);return upstream_.tick_ms(a);}
    void send_database(Actor& a,const net::Packet& p) override {check(a);upstream_.send_database(a,p);}
    void send_client(Actor& a,const net::Packet& p) override {check(a);upstream_.send_client(a,p);}
    void lock_quests(Actor& a) override {check(a);upstream_.lock_quests(a);}
    void unlock_quests(Actor& a) noexcept override {if(&a!=&actor_) std::terminate();upstream_.unlock_quests(a);}
    std::uint32_t quest_tick_ms() override {return upstream_.quest_tick_ms();}
    void fill_game_log_main(Actor& a,std::span<std::uint8_t> p) override {check(a);upstream_.fill_game_log_main(a,p);}
    void send_game_log(Actor& a,const net::Packet& p) override {check(a);upstream_.send_game_log(a,p);}
    QuestRewardWorld& reward_world(Actor& a) override {check(a);if(!reward_) throw std::logic_error("Quest session reward is not bound");return *reward_;}
    QuestStartLogContext start_log_context(Actor& a) override {check(a);return upstream_.start_log_context(a);}
    void send_start_log(Actor& a,const std::array<std::uint8_t,144>& p) override {check(a);upstream_.send_start_log(a,p);}
private:
    void check(const Actor& a) const {if(&a!=&actor_) throw std::logic_error("Quest session service actor mismatch");}
    Actor& actor_;VerticalQuestPorts& upstream_;QuestRewardWorld* reward_=nullptr;
};
VerticalQuestSession::VerticalQuestSession(QuestStartItemsActorWorldOwners owners,QuestStartItemsActorUseState use,
    ZoneNpcs& zone,NpcPool& npc_pool,VerticalQuestPorts& ports,PlayerCore& player,const StatusRules& rules,
    const Experience& curve,ExperienceAwardPorts& experience_ports,VerticalQuestRewardExperience::Options options,
    VerticalQuestRewardItems::ItemName item_name,VerticalNpcInteraction& interaction,VerticalQuestPacketActions& quest_actions,VerticalNpcPacketActions& npc_actions)
    :owners_(checked(std::move(owners))),ports_(std::make_unique<Ports>(*owners_.actor,ports)),
     grants_({owners_.items,owners_.item_pool,owners_.ids,owners_.actor},*ports_),
     experience_(*owners_.actor,player,rules,curve,experience_ports,std::move(options)),
     items_(*owners_.actor,grants_,std::move(item_name)),reward_services_(*owners_.actor,experience_,items_,*ports_),
     reward_(*owners_.actor,*owners_.runtime,*owners_.quest_pool,reward_services_),
     quests_(owners_,use,zone,npc_pool,*ports_),packets_(*owners_.actor,quests_,zone,interaction,quest_actions,npc_actions) {ports_->bind(reward_);}
VerticalQuestSession::~VerticalQuestSession()=default;
bool VerticalQuestSession::uses_ports(const VerticalQuestPorts& ports) const noexcept {return ports_->uses(ports);}
const VerticalQuestPorts& VerticalQuestSession::upstream_ports() const noexcept {return ports_->upstream();}
}
