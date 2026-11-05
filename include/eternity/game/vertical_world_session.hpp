#pragma once
#include "eternity/game/guild_rejoin_deadline.hpp"
#include "eternity/game/vertical_world_session_auto_character.hpp"
#include "eternity/game/vertical_owned_selection.hpp"
#include "eternity/game/roster_world_handoff.hpp"
#include "eternity/game/vertical_admission_handoff.hpp"
#include "eternity/game/vertical_entry_admission.hpp"
#include "eternity/game/vertical_entry_catalog_restore.hpp"
#include "eternity/game/world_users.hpp"
#include "eternity/game/vertical_disconnect.hpp"
#include "eternity/game/vertical_database_ack.hpp"
#include "eternity/game/zone_entry.hpp"
#include "eternity/game/zone_leave_requests.hpp"
#include "eternity/game/vertical_move_continuation.hpp"
#include "eternity/game/guild_membership.hpp"
#include "eternity/game/party_runtime_requests.hpp"
#include "eternity/game/vertical_quests.hpp"
#include <limits>

namespace eternity::game {
class VerticalWorldSession;
struct VerticalWorldActorScene {
    std::shared_ptr<ActorScene> owner;
    // Binding must be atomic on failure; unbind runs after world/zone drain.
    std::function<void(Actor&)> bind;
    std::function<void()> unbind;
};
// Retain the uniquely owned Actor through its loaded character and actual scene,
// without retaining the world session and creating an ownership cycle.
inline std::shared_ptr<Actor> vertical_world_quest_actor_lease(std::shared_ptr<VerticalSessionCharacter> character,std::shared_ptr<ActorScene> scene) {
    if(!character || !scene)throw std::invalid_argument("Quest actor lease requires character and scene");
    auto* actor=character->pipeline().result().actor.get();if(!actor)throw std::logic_error("Quest actor is not loaded");
    struct Owner {std::shared_ptr<ActorScene> scene;std::shared_ptr<VerticalSessionCharacter> character;};
    auto owner=std::make_shared<Owner>(Owner{std::move(scene),std::move(character)});return {std::move(owner),actor};
}
// These services are supplied by the live world. No default implementation
// reports success. All callbacks, including disconnect completion, run on the
// world strand. Dependencies returned by reference outlive the session.
class VerticalWorldSessionServices {
public:
    virtual ~VerticalWorldSessionServices()=default;
    virtual const ActorLoadRules& actor_rules()=0;
    virtual ActorLoadOptions actor_options(const PlayerSnapshot&)=0;
    // The session retains its own scene until after actor destruction.
    virtual VerticalWorldActorScene actor_scene(VerticalWorldSession&,const PlayerSnapshot&)=0;
    virtual void configure_admission(VerticalWorldSession&,WorldAdmissionUser&,const PlayerSnapshot&)=0;
    // Completion runs on the world strand after accepted work and fences drain.
    virtual void prepare(VerticalWorldSession&,std::function<void(std::exception_ptr)> done)=0;
    // Stop and drain an in-flight preparation; no completion may follow done.
    virtual void cancel_preparation(VerticalWorldSession&,std::function<void()> done)=0;
    virtual CharacterEntryState entry_state(VerticalWorldSession&)=0;
    virtual const SocialCatalog& social(VerticalWorldSession&)=0;
    virtual ExperienceBounds experience(VerticalWorldSession&)=0;
    virtual std::string guild_name(VerticalWorldSession&)=0;
    virtual VerticalProtocolServices protocol(VerticalWorldSession&)=0;
    virtual void waiting(VerticalWorldSession&)=0;
    virtual void receive(VerticalWorldSession&,const net::Packet&)=0;
    virtual void receive_nprotect(VerticalWorldSession&,const net::Packet&)=0;
    virtual void unavailable(VerticalWorldSession&)=0;
    virtual void failure(VerticalWorldSession&,std::exception_ptr)=0;
    // Stop every producer, disconnect the actual world user and drain all
    // outstanding jobs and internal acknowledgments before invoking done.
    // World/zone membership must be removed before completion; this owner
    // removes the admission-list membership afterward. Failure must not call done.
    virtual void disconnect(VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()> done)=0;
};

// ps_game.exe PacketCharacter 0046bf20; PacketUserDBChar 0046cf60;
// CWorld::WaitUser 0040ce70; CWorld::LeaveUser 0040ca20.
// Runtime owns registry, world, services and executors until every session has
// drained. This owner is constructed by roster_world_serialized_factory.
class VerticalWorldSession final:public std::enable_shared_from_this<VerticalWorldSession> {
public:
    using Strand=VerticalAsyncSelection::Strand;
    using Registry=VerticalSessionRegistry<VerticalWorldSession>;
    static std::shared_ptr<VerticalWorldSession> create(Strand strand,asio::any_io_executor database,Registry& registry,
        WorldAdmission& world,VerticalWorldSessionServices& services,const session::LoginSession& account,
        net::WorldChannel channel,CharacterRoster roster,VerticalAsyncSelection::Load load) {
        if(!strand.running_in_this_thread() || !channel.disconnect || !channel.disconnect_with_reason || !channel.retain_io || !load)
            throw std::invalid_argument("Incomplete world session services");
        const auto& claim=account.world_reservation;
        if(!claim || account.location!=session::LoginSession::Location::world ||
           claim->account()!=account.user_id || claim->realm()!=account.realm ||
           claim->session_id()!=account.session_id || claim->connection()!=account.world_connection)
            throw std::invalid_argument("World session requires its exact network claim");
        auto io_lease=channel.retain_io();
        if(!io_lease) throw std::runtime_error("World connection closed before session creation");
        auto session=std::shared_ptr<VerticalWorldSession>(new VerticalWorldSession(strand,registry,world,services,account.user_id,account.permission,account.username,std::move(channel)));
        session->io_lease_=std::move(io_lease);
        registry.reserve(account.user_id,session,account.world_reservation);
        // Keep the network claim until the final disconnect cleanup finishes,
        // including release of the socket IO lease after registry removal.
        session->reservation_=account.world_reservation;
        try {
            session->selection_=std::make_shared<VerticalOwnedSelection<VerticalWorldSession>>(strand,std::move(database),registry,session,
                CharacterSelectionState{account.user_id,roster.country,true},std::move(roster),std::move(load),
                [](VerticalWorldSession& self,VerticalPersistenceSelection value){self.loaded(std::move(value));},
                [](VerticalWorldSession& self){self.services_.unavailable(self);self.channel_.disconnect();},
                [](VerticalWorldSession& self,std::uint32_t reason){self.channel_.disconnect_with_reason(reason);},
                [](VerticalWorldSession& self,std::exception_ptr error){self.fail(error);});
        } catch(...) {registry.release(account.user_id,*session);throw;}
        return session;
    }
    // Install only from this connection's successful manager authorization.
    // The caller supplies the actual socket UID held by the network owner.
    void install_manager_connect(std::span<const std::uint8_t> packet,std::uint32_t connection) {
        require_strand();if(opened_ || closing_ || failed_ || auto_character_)throw std::logic_error("Manager connection already installed or session opened");
        auto_character_=VerticalWorldSessionAutoCharacter::from_manager(packet,account_,connection);
    }
    std::uint32_t retained_auto_character_id() const {
        require_strand();if(!auto_character_)throw std::logic_error("Manager auto-character state unavailable");return auto_character_->value();
    }
    void open() {require_strand();if(opened_)throw std::logic_error("World session already open");world_.track_selection(user_);opened_=true;}
    void receive(const net::Packet& packet) {
        require_strand();if(closing_ || failed_)return;
        if(!opened_)throw std::logic_error("World session is not open");
        if(!handoff_) {
            auto weak=weak_from_this();auto request=packet;
            handoff_=std::make_shared<VerticalAdmissionHandoff>(strand_,channel_,user_,
                [weak,request=std::move(request)]{if(auto self=weak.lock())(void)self->selection_->receive(request);},
                [weak](std::exception_ptr error){if(auto self=weak.lock())self->fail(error);});
            handoff_->start();return;
        }
        if(!delivered_)throw std::logic_error("Packet arrived during character selection");
        services_.receive(*this,packet);
    }
    void receive_nprotect(const net::Packet& packet) {require_strand();if(!closing_ && !failed_)services_.receive_nprotect(*this,packet);}
    void report_dispatch_failure(std::exception_ptr error) {require_strand();if(!error)throw std::invalid_argument("Missing dispatch failure");if(!closing_ && !dispatch_failed_) {dispatch_failed_=true;fail(error);}}
    bool dispatch_failed() const noexcept {return dispatch_failed_ || failed_;}
    // Called by the real world scheduler after location 3 is published.
    void zone_ready() {require_strand();if(closing_ || failed_ || !delivered_ || !handoff_)throw std::logic_error("Session is not waiting for a zone");zone_handoff_pending_=false;handoff_->zone_ready();}
    // A world update may reenter while the entry pipeline is still delivering.
    // Defer its already-published zone until the pipeline and waiting callback finish.
    void publish_zone_handoff() {
        require_strand();if(closing_ || failed_ || !handoff_ || user_.location!=3 || !user_.zone)throw std::logic_error("Session has no published zone");
        if(!delivered_){zone_handoff_pending_=true;return;}zone_ready();
    }
    // Only the zone scheduler may dispatch queued zone-wait packets here.
    // The caller retains control of the outbound cipher transition.
    VerticalEntryRequest receive_zone_wait(const net::Packet& packet) {
        require_strand();if(closing_ || failed_ || !delivered_)return VerticalEntryRequest::unhandled;
        auto pin=shared_from_this();return VerticalEntrySession(world_,user_).receive(packet);
    }
    // Called by the disconnect service with the real world indexes, lifecycle
    // services and final-save state. Deferred and pending-save outcomes must
    // keep this session retained until the corresponding continuation.
    VerticalDisconnectResult disconnect_in_zone(WorldUsers& users,bool final_save,VerticalDisconnectServices& services) {
        require_strand();if(!closing_ || !world_user_)throw std::logic_error("Session is not disconnecting in a zone");
        return vertical_disconnect_in_zone(users,*world_user_,final_save,services);
    }
    void close(std::optional<std::uint32_t> reason={}) {
        require_strand();if(closing_)return;closing_=true;party_user_.send_socket=-1;if(database_ack_)database_ack_->socket_open=false;if(handoff_)handoff_->cancel();
        auto self=shared_from_this();selection_->cancel_and_drain([self,reason]{
            if(self->preparing_) {
                self->services_.cancel_preparation(*self,[self,reason]{self->require_strand();self->preparing_=false;self->begin_disconnect(reason);});
            } else self->begin_disconnect(reason);
        });
    }
    // Admission scheduler pins a session through the entire live-list pass.
    // A reentrant disconnect completion is finalized after the last pass ends.
    void begin_admission_pass() {
        require_strand();if(admission_passes_==std::numeric_limits<std::size_t>::max())throw std::overflow_error("Too many admission passes");++admission_passes_;
    }
    void end_admission_pass() {
        require_strand();if(!admission_passes_)throw std::logic_error("Admission pass underflow");
        --admission_passes_;if(!admission_passes_ && finish_requested_ && !finished_)finalize_disconnect();
    }
    WorldAdmissionUser& admission() noexcept {return user_;}
    WorldUser* world_user() noexcept {return world_user_.get();}
    VerticalDatabaseAckUser* database_ack_user() noexcept {return database_ack_.get();}
    net::WorldChannel& channel() noexcept {return channel_;}
    VerticalSessionCharacter* character() noexcept {return character_.get();}
    std::shared_ptr<Actor> quest_actor_lease() {
        require_strand();if(closing_ || !preparing_ || !character_ || !scene_.owner || !world_user_)throw std::logic_error("Quest actor is not preparing");
        auto* actor=character_->pipeline().result().actor.get();if(!actor || actor!=&world_user_->actor)throw std::logic_error("Quest actor changed during preparation");
        return vertical_world_quest_actor_lease(character_,scene_.owner);
    }
    void install_quests(std::unique_ptr<VerticalQuestSession> quests) {
        require_strand();if(closing_ || delivered_ || !preparing_ || !world_user_ || !quests || quest_session_ || !quests->quests().owns_actor(world_user_->actor))throw std::logic_error("Invalid world quest ownership");
        const auto self=shared_from_this();const auto& actor=quests->actor_owner();
        if(!actor.owner_before(self) && !self.owner_before(actor))throw std::logic_error("Quest actor lease would retain its owning session");
        quest_session_=std::move(quests);
    }
    // The port owner must survive the quest session that references it.
    void install_quests(std::unique_ptr<VerticalQuestSession> quests,std::unique_ptr<VerticalQuestPorts> ports) {
        require_strand();
        if(!quests || !ports || !quests->uses_ports(*ports)) {quests.reset();throw std::invalid_argument("Quest session and owned ports differ");}
        install_quests(std::move(quests));quest_ports_=std::move(ports);
    }
    std::shared_ptr<VerticalQuestSession> quests_lease() {
        require_strand();if(!quest_session_ || !world_user_ || !quest_session_->quests().owns_actor(world_user_->actor))return {};
        return {shared_from_this(),quest_session_.get()};
    }
    // Bind only after the actual quest session has been installed. Entry is
    // synchronous; the weak session and runtime lock avoid a session cycle.
    void bind_installed_quest_entry(VerticalProtocolServices& protocol) {
        require_strand();auto* user=world_user_.get();auto quests=quests_lease();
        if(closing_ || dispatch_failed_ || !user || !quests || protocol.quest_entry || registry_.find_account(account_).get()!=this || registry_.retain_actor(user->actor).get()!=this)
            throw std::logic_error("Cannot bind installed quest entry");
        auto runtime=quests->quests().runtime_owner_for(user->actor);auto self=shared_from_this();
        if(!runtime.owner_before(self) && !self.owner_before(runtime)) throw std::logic_error("Quest entry runtime would retain its session");
        auto finished=std::make_shared<VerticalQuestRuntimeFinishedLock>(user->actor,std::move(runtime));std::weak_ptr<VerticalWorldSession> weak=self;
        protocol.quest_entry=[weak,finished=std::move(finished)](CharacterEntryStep step,Actor& actor) {
            auto owner=weak.lock();if(!owner) throw std::logic_error("Quest entry session expired");owner->require_strand();
            if(owner->closing_ || owner->dispatch_failed_ || !owner->world_user_ || &owner->world_user_->actor!=&actor || actor.account_id()!=owner->account_ ||
               owner->registry_.find_account(owner->account_).get()!=owner.get() || owner->registry_.retain_actor(actor)!=owner)
                throw std::logic_error("Quest entry lost selected Actor");
            auto quests=owner->quests_lease();if(!quests || !quests->quests().send_entry_step(step,*finished)) throw std::logic_error("Quest entry step was not handled");
            if(owner->closing_ || owner->dispatch_failed_ || !owner->world_user_ || &owner->world_user_->actor!=&actor || actor.account_id()!=owner->account_ ||
               owner->registry_.find_account(owner->account_).get()!=owner.get() || owner->registry_.retain_actor(actor)!=owner)
                throw std::logic_error("Quest entry owner changed during delivery");
        };
    }
    CharacterEntryState* entry_state() noexcept {return entry_state_ ? &*entry_state_ : nullptr;}
    ZoneEntryState& zone_entry() noexcept {return zone_entry_;}
    VerticalMotionState& motion() noexcept {return motion_;}
    // Canonical CUser+5830 exchange deadline, shared by invite, cancellation,
    // timeout processing and zone leave on the world strand.
    std::uint32_t& exchange_timeout() {require_strand();return exchange_timeout_;}
    ZoneLeaveRequestState& zone_requests() noexcept {return zone_requests_;}
    // Keep the session and its canonical request state alive together through
    // social packet jobs and world-release callbacks.
    std::shared_ptr<ZoneLeaveRequestState> zone_requests_lease() {require_strand();return {shared_from_this(),&zone_requests_};}
    GuildUser& guild_user() noexcept {return guild_user_;}
    PartyMembershipUser& party_user() noexcept {return party_user_;}
    PartyRequestState& party_requests() noexcept {return party_requests_;}
    std::uint32_t account_id() const noexcept {return account_;}
    const std::string& username() const noexcept {return username_;}
    // Report a publication failure through the real session failure and socket services.
    void report_admission_failure(std::exception_ptr error) {require_strand();if(!error)throw std::invalid_argument("Missing admission failure");if(!closing_)fail(error);}
    bool closing() const noexcept {return closing_;}
    bool failed() const noexcept {return failed_;}
private:
    VerticalWorldSession(Strand strand,Registry& registry,WorldAdmission& world,VerticalWorldSessionServices& services,std::uint32_t account,std::uint8_t permission,std::string username,net::WorldChannel channel)
        :strand_(std::move(strand)),registry_(registry),world_(world),services_(services),account_(account),username_(std::move(username)),channel_(std::move(channel)) {user_.user_id=account;zone_entry_.permission=permission;}
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("World session outside world strand");}
    void fail(std::exception_ptr error) {if(closing_ || failed_)return;failed_=true;try{services_.failure(*this,error);}catch(...){}if(!closing_)channel_.disconnect();}
    void loaded(VerticalPersistenceSelection value) {
        if(closing_ || failed_)return;
        auto options=services_.actor_options(value.snapshot);if(closing_ || failed_)return;
        const auto& core=value.snapshot.core;const auto& summary=core.character;
        party_user_.id=summary.id;party_user_.name=summary.name;party_user_.level=summary.level;party_user_.job=summary.job;
        party_user_.hp=summary.hp;party_user_.mp=summary.mp;party_user_.sp=summary.sp;party_user_.position={core.x,core.y,core.z};party_requests_.country=options.country;
        scene_=services_.actor_scene(*this,value.snapshot);if(closing_ || failed_)return;
        if(!scene_.owner || !scene_.bind || !scene_.unbind)throw std::runtime_error("Selected character scene unavailable");
        character_=std::make_shared<VerticalSessionCharacter>(std::move(value),services_.actor_rules(),options,*scene_.owner);if(closing_ || failed_)return;
        auto* actor=character_->pipeline().result().actor.get();if(!actor)throw std::runtime_error("Selected character failed actor restoration");
        scene_.bind(*actor);scene_bound_=true;if(closing_ || failed_)return;
        registry_.bind_actor(account_,*this,*actor);
        world_user_=std::make_unique<WorldUser>(WorldUser{*actor,user_});
        database_ack_=std::make_unique<VerticalDatabaseAckUser>(*world_user_,ack_mutex_);database_ack_->socket_open=true;
        const auto& snapshot=character_->snapshot();const auto& loaded=snapshot.core;user_.map=loaded.character.map;
        zone_entry_.character_id=loaded.character.id;zone_entry_.map=loaded.character.map;zone_entry_.direction=loaded.direction;zone_entry_.position={loaded.x,loaded.y,loaded.z};guild_user_.character_id=loaded.character.id;guild_restore_rejoin_deadline(guild_user_,*actor,snapshot.guild_rejoin_hours);
        services_.configure_admission(*this,user_,snapshot);if(closing_ || failed_)return;
        preparing_=true;auto weak=weak_from_this();
        services_.prepare(*this,[weak](std::exception_ptr error){if(auto self=weak.lock())self->prepared(error);});
    }
    struct EntryCancelled {};
    void prepared(std::exception_ptr error) {
        require_strand();if(!preparing_)throw std::logic_error("Unexpected session preparation completion");
        if(closing_)return;preparing_=false;
        if(dispatch_failed_ || failed_)return;
        if(error){fail(error);return;}
        try {
            entry_state_=services_.entry_state(*this);if(closing_ || failed_)return;
            auto catalogs=vertical_restore_entry_catalogs(character_->replica());
            auto protocol=services_.protocol(*this);if(closing_ || failed_)return;
            if(quest_session_) bind_installed_quest_entry(protocol);
            protocol.delivery.guild_name=[this]{return services_.guild_name(*this);};
            const auto& social=services_.social(*this);if(closing_ || failed_)return;
            const auto experience=services_.experience(*this);if(closing_ || failed_)return;
            auto prior_checkpoint=std::move(protocol.checkpoint);
            protocol.checkpoint=[this,prior=std::move(prior_checkpoint)]{if(closing_ || failed_)throw EntryCancelled{};if(prior)prior();if(closing_ || failed_)throw EntryCancelled{};};
            bool admitted=false;std::exception_ptr delivery_error;begin_admission_pass();
            try {admitted=deliver_vertical_entry_with_admission(*entry_state_,*character_,world_,user_,social,catalogs,experience,{},std::move(protocol));}
            catch(...) {delivery_error=std::current_exception();}
            try {end_admission_pass();}catch(...) {if(!delivery_error)delivery_error=std::current_exception();}
            if(delivery_error)std::rethrow_exception(delivery_error);
            if(!admitted) {if(!closing_ && !failed_)channel_.disconnect();return;}
            if(closing_ || failed_)return;delivered_=true;services_.waiting(*this);
            if(!closing_ && !failed_ && zone_handoff_pending_)zone_ready();
        } catch(const EntryCancelled&) {if(!closing_ && !failed_)fail(std::current_exception());}
          catch(...) {fail(std::current_exception());}
    }
    void begin_disconnect(std::optional<std::uint32_t> reason) {
        auto self=shared_from_this();services_.disconnect(*this,reason,[self]{self->finish_disconnect();});
    }
    void finish_disconnect() {
        require_strand();if(!closing_ || finished_ || finish_requested_)throw std::logic_error("Invalid world disconnect completion");
        if(registry_.pending_database_work(account_))throw std::logic_error("World disconnect completed with database work pending");
        finish_requested_=true;if(!admission_passes_)finalize_disconnect();
    }
    void finalize_disconnect() {
        if(registry_.pending_database_work(account_))throw std::logic_error("World disconnect acquired new database work");
        if(scene_bound_){scene_.unbind();scene_bound_=false;}
        world_.untrack(user_);if(!registry_.release(account_,*this))throw std::logic_error("World session release failed");
        finished_=true;io_lease_.reset();reservation_.reset();
    }
    Strand strand_;Registry& registry_;WorldAdmission& world_;VerticalWorldSessionServices& services_;std::uint32_t account_;std::string username_;net::WorldChannel channel_;
    WorldAdmissionUser user_;ZoneEntryState zone_entry_;VerticalMotionState motion_;std::uint32_t exchange_timeout_=0;ZoneLeaveRequestState zone_requests_;GuildUser guild_user_;PartyMembershipUser party_user_;PartyRequestState party_requests_;
    std::optional<VerticalWorldSessionAutoCharacter> auto_character_;std::optional<CharacterEntryState> entry_state_;VerticalWorldActorScene scene_;std::shared_ptr<VerticalSessionCharacter> character_;std::unique_ptr<WorldUser> world_user_;
    std::recursive_mutex ack_mutex_;std::unique_ptr<VerticalDatabaseAckUser> database_ack_;
    std::unique_ptr<VerticalQuestPorts> quest_ports_;std::unique_ptr<VerticalQuestSession> quest_session_;
    std::shared_ptr<VerticalOwnedSelection<VerticalWorldSession>> selection_;
    std::shared_ptr<VerticalAdmissionHandoff> handoff_;std::shared_ptr<void> io_lease_;
    std::shared_ptr<const session::WorldReservation> reservation_;std::size_t admission_passes_=0;bool opened_=false,closing_=false,failed_=false,dispatch_failed_=false,preparing_=false,delivered_=false,zone_handoff_pending_=false,finish_requested_=false,finished_=false,scene_bound_=false;
};

// The unique socket-facing wrapper leaves session lifetime with the registry.
class VerticalWorldSessionApplication final:public net::WorldApplication {
public:
    explicit VerticalWorldSessionApplication(std::shared_ptr<VerticalWorldSession> session):session_(std::move(session)) {if(!session_)throw std::invalid_argument("Missing world session");}
    void open() override {session_->open();}
    void receive(const net::Packet& packet) override {session_->receive(packet);}
    void receive_nprotect(const net::Packet& packet) override {session_->receive_nprotect(packet);}
    void close() override {close_with_reason({});}
    void close_with_reason(std::optional<std::uint32_t> reason) override {if(session_){auto session=std::move(session_);session->close(reason);}}
private:std::shared_ptr<VerticalWorldSession> session_;
};
// Supply this to roster_world_serialized_factory; load_for_account must retain
// each account's SQL loader and pools through asynchronous completion.
inline RosterWorldHandoff::Select vertical_world_session_select(VerticalWorldSession::Strand strand,asio::any_io_executor database,
    VerticalWorldSession::Registry& registry,WorldAdmission& world,VerticalWorldSessionServices& services,
    std::function<VerticalAsyncSelection::Load(const session::LoginSession&,const CharacterRoster&)> load_for_account) {
    if(!load_for_account)throw std::invalid_argument("Missing per-account selection loader");
    return [strand=std::move(strand),database=std::move(database),&registry,&world,&services,load_for_account=std::move(load_for_account)]
        (const session::LoginSession& account,net::WorldChannel channel,CharacterRoster roster) {
        auto load=load_for_account(account,roster);
        return std::make_unique<VerticalWorldSessionApplication>(VerticalWorldSession::create(strand,database,registry,world,services,account,std::move(channel),std::move(roster),std::move(load)));
    };
}
}
