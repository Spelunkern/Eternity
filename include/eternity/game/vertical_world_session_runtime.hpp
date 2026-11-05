#pragma once
#include "eternity/game/vertical_world_session_packet_router.hpp"
#include "eternity/game/vertical_world_session_save_teardown.hpp"
#include "eternity/game/vertical_world_session_social.hpp"
#include "eternity/game/vertical_quests_world_session.hpp"
#include "eternity/game/vertical_world_social_accounts.hpp"
#include "eternity/game/vertical_social_world_owner.hpp"
#include <typeindex>
#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/guild_client_etin.hpp"
#include "eternity/game/guild_client_ignored.hpp"
#include "eternity/game/guild_client_leave_guard.hpp"
#include "eternity/game/guild_client_expel_guard.hpp"
#include "eternity/game/guild_client_expel_target.hpp"
#include "eternity/game/guild_client_expel_offline.hpp"
#include "eternity/game/vertical_guild_expel_services.hpp"
#include "eternity/game/guild_expel_log_service.hpp"
#include "eternity/game/guild_client_leave_leader.hpp"
#include "eternity/game/guild_client_leave_missing.hpp"
#include "eternity/game/guild_client_leave_member.hpp"
#include "eternity/game/guild_leave_log_service.hpp"
#include "eternity/game/guild_delete_empty.hpp"
#include "eternity/game/guild_release_log_service.hpp"
#include "eternity/game/guild_client_npc.hpp"
#include "eternity/game/guild_client_rank.hpp"
#include "eternity/game/guild_client_remark.hpp"
#include "eternity/game/guild_client_house.hpp"
#include "eternity/game/vertical_guild_accept_services.hpp"
#include "eternity/game/guild_client_donation_empty.hpp"
#include "eternity/game/guild_client_decline.hpp"
#include "eternity/game/guild_client_invitation_reply.hpp"
#include "eternity/game/guild_client_application_deadline.hpp"
#include "eternity/game/world_lifecycle.hpp"
#include "eternity/game/guild_client_application_gate.hpp"
#include "eternity/game/guild_client_application_creation.hpp"
#include "eternity/game/guild_client_application_target.hpp"
#include "eternity/game/guild_application_replace.hpp"
#include "eternity/game/guild_application_request_log.hpp"
#include "eternity/game/guild_client_rank.hpp"
#include "eternity/game/guild_client_remark.hpp"

namespace eternity::game {
// Concrete session service composition. Every provider is mandatory and must
// operate on the actual world state; none is replaced by synthetic success.
// The runtime and its dependencies outlive all sessions and callbacks.
template<class Resource,class Store=CharacterSaveStore> class VerticalWorldSessionRuntime final:public VerticalWorldSessionServices {
public:
    using Binding=VerticalWorldSessionSaveBinding<Store>;
    using Preparation=VerticalWorldSessionPreparation<Resource,Binding>;
    using Teardown=VerticalWorldSessionSaveTeardown<Resource,Store>;
    struct Ports {
        std::function<const ActorLoadRules&()> actor_rules;
        std::function<ActorLoadOptions(const PlayerSnapshot&)> actor_options;
        std::function<VerticalWorldActorScene(VerticalWorldSession&,const PlayerSnapshot&)> actor_scene;
        std::function<void(VerticalWorldSession&,WorldAdmissionUser&,const PlayerSnapshot&)> configure_admission;
        // Return a fresh, unstarted preparation bound to this exact session.
        std::function<std::shared_ptr<Preparation>(std::shared_ptr<VerticalWorldSession>)> preparation;
        std::function<CharacterEntryState(VerticalWorldSession&)> entry_state;
        // Loaded original experience definitions; outlives the runtime.
        const Experience* experience=nullptr;
        // The loaded live guild catalog and its transport outlive the runtime.
        GuildCatalog* guilds=nullptr;
        // The same world indexes used by admission and disconnect.
        std::function<ZoneGuildCreationServices&(VerticalWorldSession&)> guild_creation;
        // Actual zone/catalog duel owner, retained through guild leave.
        std::function<ZoneLeaveGuildDuelServices&(VerticalWorldSession&)> guild_leave_duel;
        WorldUsers* world_users=nullptr;
        std::function<MoneyWorld&(VerticalWorldSession&)> guild_money;
        std::function<void(net::Packet)> guild_db;
        // Real world country broadcast for guild manager removal.
        std::function<void(std::uint8_t,net::Packet)> guild_country;
        // Loaded GuildHouse configuration and real client/DB/log transports.
        const GuildNpcUpgradePorts* guild_npc=nullptr;
        std::function<GuildHousePurchasePorts(VerticalWorldSession&)> guild_house;
        std::function<VerticalProtocolServices(VerticalWorldSession&)> protocol;
        std::function<void(VerticalWorldSession&)> waiting;
        VerticalWorldSessionPacketRouter::GameplayFactory gameplay;
        std::function<void(VerticalWorldSession&,const net::Packet&)> nprotect;
        std::function<void(VerticalWorldSession&)> unavailable;
        std::function<void(VerticalWorldSession&,std::exception_ptr)> failure;
        // Must stop other producers, complete real world/zone removal, and
        // fence unrelated DB and account-only transport before calling done.
        std::function<void(VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()>)> disconnect;
    };
    VerticalWorldSessionRuntime(VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,
        std::size_t max_packets,std::size_t max_bytes,Ports ports)
        :strand_(std::move(strand)),registry_(registry),social_accounts_(strand_,registry),ports_(std::move(ports)),packets_(strand_,registry,max_packets,max_bytes,
            [this](VerticalWorldSession& session,RuntimeZone& zone){return ports_.gameplay(session,zone);},
            [this](VerticalWorldSession& session,const net::Packet& packet){return dispatch_owned(session,packet);}) {
        if(!ports_.actor_rules || !ports_.actor_options || !ports_.actor_scene || !ports_.configure_admission || !ports_.preparation ||
           !ports_.entry_state || !ports_.experience || !ports_.guilds || !ports_.world_users || !ports_.protocol || !ports_.waiting ||
           !ports_.gameplay || !ports_.nprotect || !ports_.unavailable || !ports_.failure || !ports_.disconnect)
            throw std::invalid_argument("Incomplete live session runtime services");
    }
    VerticalWorldSessionRuntime(const VerticalWorldSessionRuntime&)=delete;
    VerticalWorldSessionRuntime& operator=(const VerticalWorldSessionRuntime&)=delete;
    const ActorLoadRules& actor_rules() override {require_strand();return ports_.actor_rules();}
    ActorLoadOptions actor_options(const PlayerSnapshot& value) override {require_strand();return ports_.actor_options(value);}
    VerticalWorldActorScene actor_scene(VerticalWorldSession& session,const PlayerSnapshot& snapshot) override {require_strand();return ports_.actor_scene(session,snapshot);}
    void configure_admission(VerticalWorldSession& session,WorldAdmissionUser& user,const PlayerSnapshot& snapshot) override {require_strand();ports_.configure_admission(session,user,snapshot);}
    void prepare(VerticalWorldSession& session,std::function<void(std::exception_ptr)> done) override {
        require_strand();if(!done || contexts_.contains(session.account_id()))throw std::logic_error("Session preparation already exists");
        auto owner=session.shared_from_this();auto context=std::make_shared<Context>();context->owner=owner;
        contexts_.emplace(session.account_id(),context);
        if(!session.character())throw std::logic_error("Social selection requires a character");
        context->social=std::make_unique<VerticalWorldSessionSocial>(social_accounts_.acquire(session),session.character()->snapshot());
        packets_.attach(owner);context->attached=true;
        // A provider may synchronously reenter close before it returns. Keep
        // cancellation pending until its fresh, unstarted result is retired.
        context->constructing=true;
        try {context->preparation=ports_.preparation(owner);}
        catch(...) {
            context->constructing=false;auto cancelled=std::move(context->construction_cancel);
            if(cancelled)cancelled();throw;
        }
        context->constructing=false;
        if(context->preparation && (!context->preparation->uses_strand(strand_) || !context->preparation->fresh())) {
            // A foreign strand or previously started provider has an unknown
            // completion path. Retain it and never claim cancellation drained.
            context->invalid_preparation=true;
            if(context->construction_cancel) {
                try {ports_.failure(session,std::make_exception_ptr(std::logic_error("Session preparation freshness cannot be validated")));}catch(...) {}
                return;
            }
            throw std::logic_error("Session preparation freshness cannot be validated");
        }
        if(auto cancelled=std::move(context->construction_cancel)) {
            context->preparation.reset();cancelled();return;
        }
        // A close may reenter before selection releases its DB fence.
        if(session.closing()) {context->preparation.reset();return;}
        if(!context->preparation)throw std::runtime_error("Session preparation unavailable");
        if(!context->preparation->matches(strand_,registry_,session)) {
            context->preparation.reset();throw std::logic_error("Session preparation has foreign owner");
        }
        context->preparing=true;
        context->preparation->start([context,done=std::move(done)](std::exception_ptr error) mutable {
            context->preparing=false;
            if(!error && context->social_world) {
                auto binding=context->preparation->binding();
                if(!binding || !context->social_bound(*binding))error=std::make_exception_ptr(std::logic_error("Session social save binding is missing or mismatched"));
                else context->social_ready=true;
            }
            done(error);
        });
    }
    void cancel_preparation(VerticalWorldSession& session,std::function<void()> done) override {
        require_strand();if(!done)throw std::invalid_argument("Missing preparation cancellation completion");auto context=find_optional(session);
        if(context && context->constructing) {
            if(context->construction_cancel)throw std::logic_error("Preparation construction cancellation already pending");
            context->construction_cancel=std::move(done);return;
        }
        if(context && context->invalid_preparation) {
            ports_.failure(session,std::make_exception_ptr(std::logic_error("Rejected preparation has not been drained")));return;
        }
        // A synchronous setup failure before a preparation was started has no
        // accepted preparation work. Any attached packet owner is stopped later.
        if(!context || !context->preparing){done();return;}
        std::weak_ptr<VerticalWorldSession> weak=session.shared_from_this();
        context->preparation->cancel([context,done=std::move(done)]() mutable {context->preparing=false;done();},
            [this,weak](std::exception_ptr error){if(auto owner=weak.lock())ports_.failure(*owner,error);});
    }
    CharacterEntryState entry_state(VerticalWorldSession& session) override {require_strand();return ports_.entry_state(session);}
    const SocialCatalog& social(VerticalWorldSession& session) override {require_strand();auto context=find(session);if(!context->social)throw std::logic_error("Session has no social selection");return context->social->catalog();}
    VerticalWorldSessionSocial& social_session(VerticalWorldSession& session) {require_strand();auto context=find(session);if(!context->social)throw std::logic_error("Session has no social selection");return *context->social;}
    VerticalWorldSocialAccounts& social_accounts() {require_strand();return social_accounts_;}
    void assert_shutdown_ready() const {
        require_strand();if(!contexts_.empty())throw std::logic_error("Session runtime still has contexts");
        if(!disconnects_.empty())throw std::logic_error("Session runtime still has disconnects");
        if(social_accounts_.size())throw std::logic_error("Session runtime still has retained social accounts");
    }
    // Install the actual social bundle while the retained save preparation is
    // running. Its catalog, account mutex, packet jobs and save owner must be
    // the same objects used by the selected session.
    template<class Database> void install_social(VerticalWorldSession& session,std::shared_ptr<VerticalSocialWorldOwner<Database>> owner) {
        require_strand();auto context=find(session);
        if(!owner || !context->preparing || context->social_world || !context->social || session.closing())throw std::logic_error("Invalid runtime social installation");
        auto prepared=owner->prepared();
        if(!prepared.social || !prepared.packets || !prepared.social->owns_session(session) ||
           &prepared.social->catalog()!=&context->social->catalog() || !prepared.packets->uses_save_mutex(context->social->account()->mutex()))
            throw std::invalid_argument("Runtime social owners differ");
        context->social_type=typeid(Database);context->social_world=owner;
        context->social_receive=[owner](const net::Packet& packet){return owner->social().receive(packet);};
        context->social_entry=[owner](EntryDeliveryPorts ports){return owner->social().bind_entry(std::move(ports));};
        context->social_bound=[owner](const Binding& binding){return binding.binds_social(owner->social());};
    }
    template<class Database> std::shared_ptr<VerticalSocialSession<VerticalWorldSession,Database>> social_lease(VerticalWorldSession& session) {
        require_strand();auto context=find(session);
        if(!context->social_ready || !context->social_world || context->social_type!=typeid(Database))throw std::logic_error("Runtime social owner is not ready");
        auto owner=std::static_pointer_cast<VerticalSocialWorldOwner<Database>>(context->social_world);
        return {owner,&owner->social()};
    }
    // Other producers and zone membership still need their own barriers.
    void assert_world_detached(VerticalWorldSession& session) {
        require_strand();(void)find_optional(session);auto* user=session.world_user();if(!user)return;
        if(ports_.world_users->find(user->actor.id())==user || ports_.world_users->find(user->actor.appearance().identity.name)==user)
            throw std::logic_error("Session still has world user indexes");
    }
    // The actor's zone flag and retained entry state both survive until the
    // real leave-zone lifecycle finishes, including its broadcasts.
    void assert_zone_detached(VerticalWorldSession& session) {
        require_strand();(void)find_optional(session);auto* user=session.world_user();if(!user)return;
        if(user->actor.status().in_zone || session.zone_entry().bound)throw std::logic_error("Session still has zone membership");
    }
    void assert_party_detached(VerticalWorldSession& session) {
        require_strand();(void)find_optional(session);
        if(session.party_user().party)throw std::logic_error("Session still has party membership");
    }
    // Bind to the actual LeaveWorld step rather than generic socket teardown.
    // The returned ports are synchronous and must not outlive the runtime.
    WorldLifecyclePorts bind_world_release(VerticalWorldSession& session,WorldLifecyclePorts ports) {
        require_strand();auto context=find(session);if(!ports.step)throw std::invalid_argument("Missing world lifecycle service");
        auto next=std::move(ports.step);std::weak_ptr<VerticalWorldSession> weak=session.shared_from_this();
        ports.step=[this,weak,context,next=std::move(next)](WorldLifecycleStep step) {
            require_strand();auto owner=weak.lock();if(!owner || find_optional(*owner)!=context)throw std::logic_error("World release lost its session");
            if(step!=WorldLifecycleStep::release_guild){next(step);return;}
            if(context->guild_released || context->guild_releasing)throw std::logic_error("Guild release already started");
            struct Guard {bool& flag;explicit Guard(bool& value):flag(value){flag=true;}~Guard(){flag=false;}} guard(context->guild_releasing);
            auto& user=owner->guild_user();
            if(user.guild) {
                auto* guild=ports_.guilds->find_membership(user.guild);if(!guild)throw std::logic_error("World release has foreign guild membership");
                std::lock_guard lock(guild->mutex);auto it=guild->membership.online.find(user.character_id);
                if(it==guild->membership.online.end() || !it->second || it->second->user!=&user)throw std::logic_error("World release lost online guild user");
                ports_.guilds->directory().release(user);
            }
            context->guild_released=true;
        };
        return ports;
    }
    // Compose the coordinator's retained social owner with the guild release.
    // Its bind_world_release handles release_friends at the original step; the
    // callback pins it for the entire synchronous world lifecycle.
    template<class Social> WorldLifecyclePorts bind_social_world_release(VerticalWorldSession& session,
        std::shared_ptr<Social> social,WorldLifecyclePorts ports) {
        require_strand();auto context=find(session);
        if(!social || !social->owns_session(session) || &social->catalog()!=&social_session(session).catalog())
            throw std::invalid_argument("Mismatched retained social world owner");
        if(context->social_release_required)throw std::logic_error("Social world release already bound");
        auto bound=social->bind_world_release(std::move(ports));if(!bound.step)throw std::logic_error("Social world release has no lifecycle step");
        auto next=std::move(bound.step);bound.step=[social=std::move(social),context,next=std::move(next)](WorldLifecycleStep step){
            if(step!=WorldLifecycleStep::release_friends){next(step);return;}
            if(context->social_released || context->social_releasing)throw std::logic_error("Social release already started");
            struct Guard {bool& flag;explicit Guard(bool& value):flag(value){flag=true;}~Guard(){flag=false;}} guard(context->social_releasing);
            next(step);context->social_released=true;
        };
        auto result=bind_world_release(session,std::move(bound));context->social_release_required=true;return result;
    }
    ExperienceBounds experience(VerticalWorldSession& session) override {
        require_strand();auto context=find(session);(void)context;
        if(!session.character())throw std::logic_error("Experience requires a selected character");
        const auto& character=session.character()->snapshot().core.character;
        return ports_.experience->bounds(character.grow,character.level);
    }
    std::string guild_name(VerticalWorldSession& session) override {
        require_strand();(void)find(session);auto* membership=session.guild_user().guild;if(!membership)return {};
        auto* guild=ports_.guilds->find_membership(membership);if(!guild)throw std::logic_error("Session has foreign guild membership");
        std::lock_guard lock(guild->mutex);return guild->metadata.listing.name;
    }
    VerticalProtocolServices protocol(VerticalWorldSession& session) override {
        require_strand();auto context=find(session);auto protocol=ports_.protocol(session);
        if(context->social_world) {if(!context->social_ready)throw std::logic_error("Runtime social owner is not ready");protocol.delivery=context->social_entry(std::move(protocol.delivery));}
        if(!protocol.lifecycle)throw std::invalid_argument("Missing entry lifecycle service");
        auto next=std::move(protocol.lifecycle);std::weak_ptr<VerticalWorldSession> weak=session.shared_from_this();
        protocol.lifecycle=[this,weak,next=std::move(next)](CharacterEntryStep step,Actor& actor) {
            auto owner=weak.lock();if(!owner || owner->closing() || !owner->world_user() || &owner->world_user()->actor!=&actor || registry_.retain_actor(actor)!=owner)
                throw std::logic_error("Entry lifecycle lost its retained actor");
            if(step!=CharacterEntryStep::init_guild){next(step,actor);return;}
            auto* state=owner->entry_state();if(!state)throw std::logic_error("Guild initialization has no entry state");
            ports_.guilds->directory().initialize(owner->guild_user());state->guild_present=owner->guild_user().guild!=nullptr;
        };
        return protocol;
    }
    void waiting(VerticalWorldSession& session) override {require_strand();ports_.waiting(session);}
    void receive(VerticalWorldSession& session,const net::Packet& packet) override {require_strand();packets_.receive(session,packet);}
    void receive_nprotect(VerticalWorldSession& session,const net::Packet& packet) override {require_strand();ports_.nprotect(session,packet);}
    void unavailable(VerticalWorldSession& session) override {require_strand();ports_.unavailable(session);}
    void failure(VerticalWorldSession& session,std::exception_ptr error) override {require_strand();ports_.failure(session,error);}
    // Admission scheduler calls this from its packet provider. Before resource
    // preparation there is no post-selection queue to dispatch.
    void update_packets(VerticalWorldSession& session) {require_strand();auto context=find_optional(session);if(context && context->attached)packets_.update(session);}
    void invalidate_gameplay(VerticalWorldSession& session) {require_strand();auto context=find(session);if(context->attached)packets_.invalidate_gameplay(session);}
    std::shared_ptr<Binding> save_binding(VerticalWorldSession& session) {require_strand();auto context=find(session);return context->preparation?context->preparation->binding():std::shared_ptr<Binding>{};}
    void disconnect(VerticalWorldSession& session,std::optional<std::uint32_t> reason,std::function<void()> done) override {
        require_strand();if(!done)throw std::invalid_argument("Missing session disconnect completion");auto owner=session.shared_from_this();auto context=find_optional(session);
        if(context && context->invalid_preparation)throw std::logic_error("Cannot disconnect with an unowned started preparation");
        auto ticket=std::make_shared<Disconnect>();ticket->owner=owner;
        if(!disconnects_.emplace(owner->account_id(),ticket).second)throw std::logic_error("Session disconnect already started");
        if(context && context->attached){packets_.stop(session);context->attached=false;}
        ports_.disconnect(session,reason,[this,owner,context,ticket,done=std::move(done)]() mutable {
            require_strand();auto it=disconnects_.find(owner->account_id());
            if(it==disconnects_.end() || it->second!=ticket || ticket->completing)throw std::logic_error("Stale session disconnect completion");
            ticket->completing=true;
            if(!context || !context->preparation || !context->preparation->binding()){finish(owner,context,ticket,std::move(done));return;}
            std::weak_ptr<VerticalWorldSession> weak=owner;
            context->teardown=Teardown::create(strand_,context->preparation,[this,weak](std::exception_ptr error){if(auto session=weak.lock())ports_.failure(*session,error);});
            context->teardown->start([this,owner,context,ticket,done=std::move(done)]() mutable {finish(owner,context,ticket,std::move(done));});
        });
    }
private:
    struct Disconnect {std::shared_ptr<VerticalWorldSession> owner;bool completing=false;};
    struct Context {std::weak_ptr<VerticalWorldSession> owner;std::shared_ptr<void> social_world;std::type_index social_type{typeid(void)};
        std::function<EntryCatalogUpdate(const net::Packet&)> social_receive;std::function<EntryDeliveryPorts(EntryDeliveryPorts)> social_entry;
        std::function<bool(const Binding&)> social_bound;bool social_ready=false;std::unique_ptr<VerticalWorldSessionSocial> social;std::shared_ptr<Preparation> preparation;std::shared_ptr<Teardown> teardown;std::function<void()> construction_cancel;bool attached=false,constructing=false,preparing=false,invalid_preparation=false,guild_released=false,guild_releasing=false,social_release_required=false,social_released=false,social_releasing=false;};
    bool dispatch_owned(VerticalWorldSession& session,const net::Packet& packet) {
        if(guild_client_ignored(packet))return true;
        if(packet.size()>=2 && packet[0]==6 && packet[1]==0x0d) {
            if(!session.world_user())throw std::logic_error("Missing guild invitation actor");
            auto& actor=session.world_user()->actor;
            auto& state=session.zone_requests();
            struct ZoneServices final:GuildInvitationReplyServices {
                Actor& subject;ZoneUsers& users;VerticalWorldSession::Registry& registry;
                std::shared_ptr<VerticalWorldSession> retained;
                ZoneServices(Actor& a,ZoneUsers& u,VerticalWorldSession::Registry& r):subject(a),users(u),registry(r){}
                Actor* find_in_zone(Actor& a,std::uint32_t id) override {
                    if(&a!=&subject)throw std::logic_error("Guild invitation lost subject");
                    return users.find_actor(id);
                }
                std::shared_ptr<void> retain(Actor& peer) override {
                    auto owner=registry.retain_actor(peer);
                    if(!owner || !owner->world_user() || &owner->world_user()->actor!=&peer)
                        throw std::logic_error("Guild invitation lost peer owner");
                    retained=owner;return owner;
                }
                bool can_invite(Actor& peer) override {
                    if(!retained || !retained->world_user() || &retained->world_user()->actor!=&peer)
                        throw std::logic_error("Guild invitation lost retained peer");
                    const auto& user=retained->guild_user();return user.guild && user.rank<=2;
                }
            };
            EntryCatalogUpdate result;
            if(!state.request_1754 || packet.size()<3)result=guild_client_invitation_reply(actor,state,packet,nullptr);
            else {
                auto* zone=session.admission().zone;
                if(!zone || !zone->users)throw std::logic_error("Missing guild invitation zone");
                ZoneServices services(actor,*zone->users,registry_);
                result=guild_client_invitation_reply(actor,state,packet,&services);
            }
            if(result==EntryCatalogUpdate::unsupported)return false;
            if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild invitation packet rejected");
            return true;
        }
        if(packet.size()>=2 && packet[0]==0x07 && packet[1]==0x0d) {
            if(!session.world_user())throw std::logic_error("Missing guild application actor");
            const auto result=guild_client_application_deadline(session.guild_user(),session.world_user()->actor,packet,
                [&](GuildUser& user,net::Packet response) {
                    if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild application send lost owner");
                    session.channel().send(std::move(response));
                });
            if(result==EntryCatalogUpdate::unsupported) {
                const auto gate=guild_client_application_gate(session.guild_user(),session.world_user()->actor,session.zone_requests(),ports_.guilds,packet,
                    [&](GuildUser& user,net::Packet response) {
                        if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild application send lost owner");
                        session.channel().send(std::move(response));
                    });
                if(gate==EntryCatalogUpdate::unsupported) {
                    if(ports_.guild_creation) {
                        auto result=guild_client_application_creation(session.guild_user(),session.world_user()->actor,ports_.guild_creation(session),packet,
                            [&](GuildUser& user,net::Packet response) {
                                if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild application send lost owner");
                                session.channel().send(std::move(response));
                            });
                        if(result!=EntryCatalogUpdate::unsupported) {
                            if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild application packet rejected");return true;
                        }
                    }
                    if(!ports_.guilds)throw std::logic_error("Missing live guild catalog");
                    const auto target=guild_client_application_target(session.guild_user(),session.world_user()->actor,*ports_.guilds,packet,
                        [&](GuildUser& user,net::Packet response) {
                            if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild application send lost owner");
                            session.channel().send(std::move(response));
                        });
                    if(target==EntryCatalogUpdate::unsupported) {
                        if(!ports_.guild_creation || !ports_.guild_money || !ports_.guild_db)return false;
                        std::uint32_t id=0;for(unsigned i=0;i<4;++i)id|=std::uint32_t(packet[2+i])<<(i*8);
                        auto* guild=ports_.guilds->find(id);if(!guild)throw std::logic_error("Guild application target disappeared");
                        GuildApplicationCreatePorts service;
                        service.send_user=[&](GuildUser& user,net::Packet response) {
                            if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild application send lost owner");
                            session.channel().send(std::move(response));
                        };
                        if(!session.channel().send)throw std::logic_error("Missing guild application client transport");
                        service.send_db=ports_.guild_db;service.game_log=guild_application_request_log(ports_.guild_money(session));
                        const auto applied=guild_application_replace(session.guild_user(),session.world_user()->actor,*guild,*ports_.guilds,service);
                        if(applied!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild application packet rejected");return true;
                    }
                    if(target!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild application packet rejected");return true;
                }
                if(gate!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild application packet rejected");return true;
            }
            if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild application packet rejected");return true;
        }
        if(packet.size()>=2 && packet[0]==0x09 && packet[1]==0x0d) {
            if(!session.world_user())throw std::logic_error("Missing guild leave actor");
            ZoneGuildCreationServices* creation=nullptr;if(session.guild_user().guild && ports_.guild_creation)creation=&ports_.guild_creation(session);
            const auto result=guild_client_leave_guard(session.guild_user(),session.world_user()->actor,ports_.guilds,creation,packet);
            if(result==EntryCatalogUpdate::unsupported) {
                if(!creation)return false;
                const auto leader=guild_client_leave_leader(session.guild_user(),session.world_user()->actor,*ports_.guilds,*creation,packet,
                    [&](GuildUser& user,net::Packet response) {
                        if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild leave send lost owner");
                        session.channel().send(std::move(response));
                    });
                if(leader==EntryCatalogUpdate::unsupported) {
                    const auto missing=guild_client_leave_missing(session.guild_user(),session.world_user()->actor,*ports_.guilds,*creation,packet,
                        [&](GuildUser& user,net::Packet response) {
                            if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild leave send lost owner");
                            session.channel().send(std::move(response));
                        });
                    if(missing==EntryCatalogUpdate::unsupported) {
                        if(!ports_.guild_leave_duel || !ports_.guild_money || !ports_.guild_db)return false;
                        if(!session.channel().send)throw std::logic_error("Missing guild leave client transport");
                        GuildLeaveMemberPorts service;
                        service.send_user=[&](GuildUser& user,net::Packet response) {
                            if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild leave send lost owner");
                            session.channel().send(std::move(response));
                        };
                        auto& money=ports_.guild_money(session);service.send_db=ports_.guild_db;service.game_log=guild_leave_log_service(money);
                        if(ports_.guild_country) {
                            GuildDeleteEmptyPorts manager{guild_release_log_service(money),ports_.guild_db,ports_.guild_country};
                            service.delete_empty=[&,manager=std::move(manager)](Actor& actor,LoadedGuild& guild){guild_delete_empty(guild,*ports_.guilds,&actor,manager);};
                        }
                        const auto removed=guild_client_leave_member(session.guild_user(),session.world_user()->actor,*ports_.guilds,*creation,ports_.guild_leave_duel(session),packet,service);
                        if(removed==EntryCatalogUpdate::unsupported)return false;
                        if(removed!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild leave packet rejected");return true;
                    }
                    if(missing!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild leave packet rejected");return true;
                }
                if(leader!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild leave packet rejected");return true;
            }
            if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild leave packet rejected");return true;
        }
        if(packet.size()>=2 && packet[0]==0x0a && packet[1]==0x0d) {
            if(!session.world_user())throw std::logic_error("Missing guild expulsion actor");
            ZoneGuildCreationServices* creation=nullptr;if(session.guild_user().guild && session.guild_user().rank<=2 && ports_.guild_creation)creation=&ports_.guild_creation(session);
            const auto result=guild_client_expel_guard(session.guild_user(),session.world_user()->actor,ports_.guilds,creation,packet);
            if(result==EntryCatalogUpdate::unsupported) {
                if(!creation)return false;
                const auto target=guild_client_expel_target(session.guild_user(),session.world_user()->actor,*ports_.guilds,*creation,packet,
                    [&](GuildUser& user,net::Packet response) {
                        if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild expulsion send lost owner");
                        session.channel().send(std::move(response));
                    });
                if(target==EntryCatalogUpdate::unsupported) {
                    if(!ports_.guild_db || !ports_.guild_money || !ports_.world_users)return false;
                    if(!session.channel().send)throw std::logic_error("Missing guild expulsion client transport");
                    auto resolver=[this](GuildUser& peer)->std::shared_ptr<Actor> {
                        auto* found=ports_.world_users->find(peer.character_id);if(!found)throw std::logic_error("Expelled actor unavailable");
                        auto owner=registry_.retain_actor(found->actor);
                        if(!owner || owner->world_user()!=found || &owner->guild_user()!=&peer)throw std::logic_error("Expelled actor lost session owner");
                        return std::shared_ptr<Actor>(owner,&found->actor);
                    };
                    GuildExpelOfflinePorts service;
                    service.send_user=[&](GuildUser& user,net::Packet response) {
                        if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild expulsion send lost owner");
                        session.channel().send(std::move(response));
                    };
                    service.send_db=ports_.guild_db;service.game_log=guild_expel_log_service(ports_.guild_money(session),std::move(resolver));
                    const auto expelled=guild_client_expel_offline(session.guild_user(),session.world_user()->actor,*ports_.guilds,*creation,packet,service);
                    if(expelled==EntryCatalogUpdate::unsupported) {
                        if(!ports_.guild_leave_duel)return false;
                        auto online=vertical_guild_expel_services(session,registry_,*ports_.world_users,ports_.guild_money(session),ports_.guild_db,ports_.guild_leave_duel);
                        const auto result=guild_client_expel_online(session.guild_user(),session.world_user()->actor,*ports_.guilds,*creation,packet,online);
                        if(result==EntryCatalogUpdate::unsupported)return false;
                        if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild expulsion packet rejected");return true;
                    }
                    if(expelled!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild expulsion packet rejected");return true;
                }
                if(target!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild expulsion packet rejected");return true;
            }
            if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild expulsion packet rejected");return true;
        }
        if(packet.size()>=3 && packet[0]==0x08 && packet[1]==0x0d && packet[2]==1) {
            if(!ports_.guilds)throw std::logic_error("Missing live guild catalog");
            GuildApplicationAcceptPorts service;
            if(ports_.world_users && ports_.guild_money && ports_.guild_db)
                service=vertical_guild_accept_services(session,registry_,*ports_.world_users,ports_.guild_money(session),ports_.guild_db);
            const auto result=guild_client_accept(session.guild_user(),*ports_.guilds,packet,service);
            if(result==EntryCatalogUpdate::unsupported)return false;
            if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild acceptance packet rejected");return true;
        }
        if(packet.size()>=2 && packet[0]==0x08 && packet[1]==0x0d) {
            if(!ports_.guilds)throw std::logic_error("Missing live guild catalog");
            std::shared_ptr<VerticalWorldSession> peer;
            GuildApplicationDeclinePorts service;
            if(ports_.world_users)service.find_online=[&](std::uint32_t character)->std::shared_ptr<GuildUser> {
                auto* world=ports_.world_users->find(character);if(!world)return {};
                peer=registry_.retain_actor(world->actor);
                if(!peer || peer->world_user()!=world || peer->guild_user().character_id!=character)throw std::logic_error("Guild applicant lost session owner");
                return std::shared_ptr<GuildUser>(peer,&peer->guild_user());
            };
            service.send_user=[&](GuildUser& user,net::Packet response) {
                if(!peer || &peer->guild_user()!=&user || !peer->channel().send)throw std::logic_error("Guild applicant send lost owner");
                peer->channel().send(std::move(response));
            };
            service.send_db=ports_.guild_db;
            const auto result=guild_client_decline(session.guild_user(),*ports_.guilds,packet,service);
            if(result==EntryCatalogUpdate::unsupported)return false;
            if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild application packet rejected");return true;
        }
        if(packet.size()>=2 && packet[0]==0x35 && packet[1]==0x0d) {
            if(!ports_.guilds || !session.world_user())throw std::logic_error("Missing guild donation session services");
            const auto result=guild_client_donation_empty(session.guild_user(),session.world_user()->actor,*ports_.guilds,packet,
                [&](GuildUser& user,net::Packet response) {
                    if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild donation send lost owner");
                    session.channel().send(std::move(response));
                });
            if(result==EntryCatalogUpdate::unsupported)return false;
            if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild donation packet rejected");return true;
        }
        if(packet.size()>=2 && packet[0]==0x34 && packet[1]==0x0d) {
            if(!ports_.guilds || !ports_.guild_house || !session.world_user())throw std::logic_error("Missing guild house session services");
            auto service=ports_.guild_house(session);
            if(guild_client_house(session.guild_user(),session.world_user()->actor,*ports_.guilds,packet,service)!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild house packet rejected");return true;
        }
        if(packet.size()>=2 && packet[0]==0x25 && packet[1]==0x0d) {
            if(!ports_.guilds)throw std::logic_error("Missing live guild catalog");
            if(guild_client_npc(session.guild_user(),*ports_.guilds,packet,ports_.guild_npc)!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild NPC packet rejected");return true;
        }
        if(packet.size()>=2 && packet[0]==0x0c && packet[1]==0x0d) {
            if(!ports_.guilds)throw std::logic_error("Missing live guild catalog");
            if(guild_client_rank(session.guild_user(),*ports_.guilds,packet,ports_.guild_db)!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild rank packet rejected");return true;
        }
        if(packet.size()>=2 && packet[0]==0x20 && packet[1]==0x0d) {
            if(!ports_.guilds)throw std::logic_error("Missing live guild catalog");
            auto result=guild_client_remark(session.guild_user(),*ports_.guilds,packet,[&](GuildUser& user,net::Packet response) {
                if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild remark send lost owner");
                session.channel().send(std::move(response));
            },ports_.guild_db);
            if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild remark packet rejected");return true;
        }
        if(packet.size()>=2 && packet[0]==0x23 && packet[1]==0x0d) {
            if(!ports_.guilds)throw std::logic_error("Missing live guild catalog");
            auto result=guild_client_etin(session.guild_user(),*ports_.guilds,packet,[&](GuildUser& user,net::Packet response) {
                if(&user!=&session.guild_user() || !session.channel().send)throw std::logic_error("Guild etin send lost owner");
                session.channel().send(std::move(response));
            });
            if(result!=EntryCatalogUpdate::applied)throw std::runtime_error("Guild etin packet rejected");return true;
        }
        if(packet.size()>=2 && (packet[1]==9 || packet[1]==14)) {
            VerticalWorldQuestPacketDispatch quests(registry_);return quests(session,packet);
        }
        return dispatch_social(session,packet);
    }
    bool dispatch_social(VerticalWorldSession& session,const net::Packet& packet) {
        if(packet.size()<2 || packet[1]!=0x22)return false;
        auto context=find(session);if(!context->social_ready || !context->social_receive)throw std::logic_error("Runtime social owner is not ready");
        auto receive=context->social_receive;
        if(receive(packet)!=EntryCatalogUpdate::applied)throw std::runtime_error("Social client packet rejected");
        return true;
    }
    void require_strand() const {if(!strand_.running_in_this_thread())throw std::logic_error("Session runtime outside world strand");}
    std::shared_ptr<Context> find_optional(VerticalWorldSession& session) {
        auto registered=registry_.find_account(session.account_id());if(registered.get()!=&session)throw std::logic_error("Session runtime lost registry owner");
        auto it=contexts_.find(session.account_id());if(it==contexts_.end())return {};
        if(it->second->owner.lock()!=registered)throw std::logic_error("Session runtime found a stale context");return it->second;
    }
    std::shared_ptr<Context> find(VerticalWorldSession& session) {auto context=find_optional(session);if(!context)throw std::logic_error("Session runtime has no context");return context;}
    void finish(const std::shared_ptr<VerticalWorldSession>& owner,const std::shared_ptr<Context>& context,const std::shared_ptr<Disconnect>& ticket,std::function<void()> done) {
        require_strand();auto disconnect=disconnects_.find(owner->account_id());if(disconnect==disconnects_.end() || disconnect->second!=ticket || !ticket->completing)throw std::logic_error("Session runtime lost disconnect fence");
        auto it=contexts_.end();if(context){it=contexts_.find(owner->account_id());if(it==contexts_.end() || it->second!=context)throw std::logic_error("Session runtime lost disconnect context");}
        try {
        assert_world_detached(*owner);
        assert_zone_detached(*owner);
        assert_party_detached(*owner);
        if(owner->guild_user().guild && (!context || !context->guild_released))throw std::logic_error("Session disconnect omitted guild release");
        if(context && context->social_release_required && !context->social_released)throw std::logic_error("Session disconnect omitted social release");
        } catch(...) {disconnects_.erase(disconnect);throw;}
        done();if(context)contexts_.erase(it);disconnects_.erase(disconnect);
    }
    VerticalWorldSession::Strand strand_;VerticalWorldSession::Registry& registry_;VerticalWorldSocialAccounts social_accounts_;Ports ports_;VerticalWorldSessionPacketRouter packets_;
    std::map<std::uint32_t,std::shared_ptr<Context>> contexts_;
    std::map<std::uint32_t,std::shared_ptr<Disconnect>> disconnects_;
};
using VerticalWorldSessionSqlRuntime=VerticalWorldSessionRuntime<VerticalWorldSessionSqlSave>;
}
