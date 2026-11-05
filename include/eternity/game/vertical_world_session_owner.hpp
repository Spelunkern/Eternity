#pragma once
#include "eternity/game/vertical_world_session_admission.hpp"
#include "eternity/game/vertical_world_session_runtime.hpp"
#include "eternity/game/vertical_world_session_sql_selection.hpp"
#include "eternity/game/vertical_world_zone_disconnect.hpp"
#include "eternity/game/vertical_world_session_zone_services.hpp"
#include "eternity/game/vertical_quests_world_session.hpp"
#include "eternity/game/zone_logout_update.hpp"
#include <mutex>

namespace eternity::game {
// Owns the account registry, concrete session runtime and admission lists.
// The external world indexes, database executor, scheduler and all service
// dependencies outlive it. Drain every returned roster application and session
// before destruction; shutdown fences new selections but does not close them.
// ps_game.exe PacketCharacter 0046bf20 / CWorld::WaitUser 0040ce70.
template<class Resource,class Store=CharacterSaveStore> class VerticalWorldSessionOwner final {
    struct RootLease {std::shared_ptr<VerticalWorldSessionOwner> owner;std::shared_ptr<void> gate;};
public:
    using Runtime=VerticalWorldSessionRuntime<Resource,Store>;
    using LoadForAccount=std::function<VerticalAsyncSelection::Load(const session::LoginSession&,const CharacterRoster&)>;
    using PreparationProvider=std::function<std::shared_ptr<typename Runtime::Preparation>(VerticalWorldSession::Strand,asio::any_io_executor,
        VerticalWorldSession::Registry&,std::shared_ptr<VerticalWorldSession>)>;
    using RuntimePreparationProvider=std::function<std::shared_ptr<typename Runtime::Preparation>(VerticalWorldSession::Strand,asio::any_io_executor,
        VerticalWorldSession::Registry&,Runtime&,std::shared_ptr<VerticalWorldSession>)>;
    VerticalWorldSessionOwner(VerticalWorldSession::Strand strand,asio::any_io_executor database,WorldUsers& users,
        std::size_t max_packets,std::size_t max_bytes,typename Runtime::Ports ports,VerticalWorldSessionAdmissionPorts admission,std::shared_ptr<session::Registry> network,std::uint8_t realm,PreparationProvider preparation={},RuntimePreparationProvider runtime_preparation={})
        :strand_(std::move(strand)),database_(std::move(database)),runtime_(strand_,registry_,max_packets,max_bytes,world_ports(std::move(ports),users,strand_,database_,registry_,std::move(preparation),std::move(runtime_preparation),this)),
         admission_(strand_,registry_,users,admission_ports(std::move(admission))),world_(admission_) {registry_.bind_network_registry(std::move(network),realm);}
    VerticalWorldSessionOwner(const VerticalWorldSessionOwner&)=delete;
    VerticalWorldSessionOwner& operator=(const VerticalWorldSessionOwner&)=delete;
    ~VerticalWorldSessionOwner() {if(registry_.size()!=0 || !gate_->stop_if_idle())std::terminate();try{runtime_.assert_shutdown_ready();}catch(...){std::terminate();}}
    VerticalWorldSession::Registry& registry() {require_running();return registry_;}
    Runtime& runtime() {require_running();return runtime_;}
    // Aliases share the owner control block, so the root cannot outlive the
    // registry or the exact admission queue used by socket-facing sessions.
    struct RootBindings {
        std::shared_ptr<VerticalWorldSession::Registry> sessions;
        std::shared_ptr<WorldAdmission> admission;
        std::function<void()> update_admission;
        std::function<void(RuntimeZone&)> update_zone_wait_packets;
    };
    static RootBindings root_bindings(const std::shared_ptr<VerticalWorldSessionOwner>& owner) {
        if(!owner)throw std::invalid_argument("Missing world session owner");owner->require_running();
        auto retained=std::make_shared<RootLease>(RootLease{owner,owner->gate_->acquire_resource()});
        return {{retained,&owner->registry_},{retained,&owner->world_},[retained]{retained->owner->update_world();},
            [retained](RuntimeZone& zone){retained->owner->update_zone_packets(zone);}};
    }
    WorldAdmission& world() {require_running();return world_;}
    // The router and every alias retain an owner lease. Its real leave
    // continuation and all producers still need their own disconnect drain.
    std::shared_ptr<VerticalWorldSessionAckRouter> make_ack_router(VerticalWorldSessionAckRouter::Leave leave) {
        require_running();auto lease=gate_->acquire_resource();
        auto retained=std::make_shared<RetainedAckRouter>(std::move(lease),strand_,registry_,std::move(leave));
        return {retained,&retained->router};
    }
    // Install before accepting sessions. The supplied completion service must
    // perform the real remaining producer/transport cleanup and drain.
    using ZoneCompletion=std::function<void(VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()>)>;
    void install_zone_disconnect(VerticalWorldZoneDisconnect::Service service,ZoneCompletion completion) {
        require_active();if(zone_disconnect_ || registry_.size() || !service || !completion)throw std::invalid_argument("Invalid zone disconnect installation");
        zone_completion_=std::move(completion);zone_disconnect_=std::make_unique<VerticalWorldZoneDisconnect>(strand_,registry_,*runtime_world_users_,std::move(service));
    }
    // Own the concrete per-session adapter. Its binding comes from this
    // runtime; callers supply the remaining actual live services.
    using ZoneServices=VerticalWorldSessionZoneServices<Store>;
    void install_session_zone_services(typename ZoneServices::Ports ports,ZoneCompletion completion) {
        require_active();if(ports.binding || !completion || !ports.user_mutex || !ports.zone_mutex || !ports.dead_disconnect || !ports.tick_ms ||
           !ports.skills || (bool(ports.quests)==bool(ports.save_quests)) || !ports.save || !ports.leave || !ports.finish_final)
            throw std::invalid_argument("Invalid session zone service installation");
        ports.binding=[this](VerticalWorldSession& session){return runtime_.save_binding(session);};
        install_zone_disconnect([this,ports=std::move(ports)](VerticalWorldSession& session) mutable ->VerticalDisconnectServices& {
            auto it=zone_services_.find(session.account_id());
            if(it!=zone_services_.end()) {if(!it->second->matches(session))throw std::logic_error("Stale session zone services");return *it->second;}
            auto owner=registry_.find_account(session.account_id());if(owner.get()!=&session)throw std::logic_error("Session zone services lost owner");
            auto service=std::make_unique<ZoneServices>(strand_,registry_,std::move(owner),ports);auto* result=service.get();
            zone_services_.emplace(session.account_id(),std::move(service));return *result;
        },[this,completion=std::move(completion)](VerticalWorldSession& session,std::optional<std::uint32_t> reason,std::function<void()> done) mutable {
            const auto account=session.account_id();auto expected=session.shared_from_this();
            completion(session,reason,[this,account,expected=std::move(expected),done=std::move(done)]() mutable {
                require_running();auto it=zone_services_.find(account);
                if(it==zone_services_.end() || !it->second->matches(*expected))throw std::logic_error("Session zone services lost completion owner");
                zone_services_.erase(it);done();
            });
        });
    }
    // Bind zone save to the session's installed quest runtime and to the
    // same replica mutex as its retained save binding.
    void install_retained_quest_zone_services(typename ZoneServices::Ports ports,ZoneCompletion completion) {
        require_active();if(ports.quests || ports.save_quests)throw std::invalid_argument("Quest zone service must use retained session quests");
        ports.save_quests=[this](VerticalWorldSession& session,std::uint32_t tick,VerticalSaveCoreSkillServices&) {
            auto binding=runtime_.save_binding(session);
            if(!binding || !binding->owns_session(session))throw std::logic_error("Quest save binding belongs to another session");
            vertical_world_quests_save_phase(session,registry_,binding->save_mutex(),tick);
        };
        install_session_zone_services(std::move(ports),std::move(completion));
    }
    std::shared_ptr<VerticalWorldSessionAckRouter> make_zone_ack_router() {
        require_running();if(!zone_disconnect_)throw std::logic_error("Zone disconnect is not installed");
        return make_ack_router([this](std::shared_ptr<VerticalWorldSession> owner){zone_disconnect_->acknowledge(*owner);});
    }
    void update_zone_user_logout(RuntimeZone& zone,std::uint32_t tick,const std::function<void(VerticalWorldSession&)>& other_logout) {
        require_running();if(!zone_disconnect_ || !zone.users || !other_logout)throw std::invalid_argument("Missing zone logout services");
        auto retain=[this](Actor& actor)->std::shared_ptr<VerticalWorldSession> {
            auto owner=registry_.find_account(actor.account_id());
            if(!owner || !owner->world_user() || &owner->world_user()->actor!=&actor)throw std::logic_error("Zone logout lost session owner");return owner;
        };
        zone_update_logout_deadlines(*zone.users,tick,[&](Actor& actor)->std::shared_ptr<void>{return retain(actor);},
            [&](Actor& actor)->WorldAdmissionUser&{return retain(actor)->admission();},
            [&](Actor& actor,WorldAdmissionUser&){auto owner=retain(actor);if(zone_disconnect_->contains(*owner))zone_disconnect_->resume(*owner);else other_logout(*owner);});
    }
    void update_world() {require_running();admission_.update_world_and_release(world_);}
    void update_zone_packets(RuntimeZone& zone) {require_running();admission_.update_zone_packets(world_,zone);}
    void update_zone_logout(RuntimeZone& zone,std::uint32_t tick,const std::function<void(VerticalWorldSession&)>& logout) {
        require_running();admission_.update_zone_logout(world_,zone,tick,logout);
    }
    net::WorldApplicationFactory factory(net::LoadCharacters load,net::DispatchCharacter commands,LoadForAccount loader) {
        require_active();return wrap(std::move(load),std::move(commands),vertical_world_session_select(strand_,database_,registry_,world_,runtime_,std::move(loader)));
    }
    net::WorldApplicationFactory sql_factory(net::LoadCharacters load,net::DispatchCharacter commands,storage::SqlServerConfig config,
        std::uint8_t server,std::string catalog,std::shared_ptr<InventoryReplicaPool> inventory,
        std::shared_ptr<SkillReplicaPool> skills={},std::shared_ptr<QuestReplicaPool> quests={},std::shared_ptr<PointItemPool> points={}) {
        require_active();return wrap(std::move(load),std::move(commands),vertical_world_session_sql_select(strand_,database_,registry_,world_,runtime_,
            std::move(config),server,std::move(catalog),std::move(inventory),std::move(skills),std::move(quests),std::move(points)));
    }
    // Stop new roster applications and new selections while existing session
    // schedulers and disconnect/DB drains remain usable on the world strand.
    void stop_accepting() {require_running();gate_->stop();}
    void shutdown() {
        require_running();if(registry_.size() || world_.connected_count() || world_.selection_count() || world_.waiting_count())
            throw std::logic_error("World session owner still has live sessions");
        runtime_.assert_shutdown_ready();
        if((zone_disconnect_ && zone_disconnect_->pending()) || !zone_services_.empty())throw std::logic_error("World session owner still has zone disconnects");
        if(!gate_->stop_if_idle())throw std::logic_error("World session owner still has retained applications or routers");
        shutdown_=true;
    }
private:
    static typename Runtime::Ports world_ports(typename Runtime::Ports ports,WorldUsers& users,VerticalWorldSession::Strand strand,
        asio::any_io_executor database,VerticalWorldSession::Registry& registry,PreparationProvider preparation,RuntimePreparationProvider runtime_preparation,VerticalWorldSessionOwner* owner) {
        if(ports.world_users && ports.world_users!=&users)throw std::invalid_argument("Session runtime uses foreign world indexes");
        if(runtime_preparation) {
            if(ports.preparation || preparation)throw std::invalid_argument("Duplicate session preparation providers");
            // The runtime is fully constructed before this callback can run.
            ports.preparation=[strand,database,&registry,owner,runtime_preparation=std::move(runtime_preparation)](std::shared_ptr<VerticalWorldSession> session) {
                return runtime_preparation(strand,database,registry,owner->runtime_,std::move(session));
            };
        }
        if(preparation) {
            if(ports.preparation)throw std::invalid_argument("Duplicate session preparation providers");
            ports.preparation=[strand=std::move(strand),database=std::move(database),&registry,preparation=std::move(preparation)](std::shared_ptr<VerticalWorldSession> session) {
                return preparation(strand,database,registry,std::move(session));
            };
        }
        if(!ports.disconnect)throw std::invalid_argument("Missing session disconnect service");
        auto other_disconnect=std::move(ports.disconnect);
        ports.disconnect=[owner,other_disconnect=std::move(other_disconnect)](VerticalWorldSession& session,std::optional<std::uint32_t> reason,std::function<void()> done) mutable {
            if(!owner->zone_disconnect_ || session.admission().location!=4){other_disconnect(session,reason,std::move(done));return;}
            auto retained=session.shared_from_this();
            owner->zone_disconnect_->begin(session,[owner,retained,reason,done=std::move(done)]() mutable {owner->zone_completion_(*retained,reason,std::move(done));});
        };
        owner->runtime_world_users_=&users;ports.world_users=&users;return ports;
    }
    VerticalWorldSessionAdmissionPorts admission_ports(VerticalWorldSessionAdmissionPorts ports) {
        if(ports.packets)throw std::invalid_argument("Session owner supplies admission packet routing");
        ports.packets=[this](VerticalWorldSession& session){runtime_.update_packets(session);};return ports;
    }
    net::WorldApplicationFactory wrap(net::LoadCharacters load,net::DispatchCharacter commands,RosterWorldHandoff::Select select) {
        auto guarded=[this,select=std::move(select)](const session::LoginSession& account,net::WorldChannel channel,CharacterRoster roster) {
            require_active();return select(account,std::move(channel),std::move(roster));
        };
        auto factory=roster_world_serialized_factory(strand_,std::move(load),std::move(commands),std::move(guarded));
        return [gate=gate_,factory=std::move(factory)](const session::LoginSession& account,net::WorldChannel channel) {
            auto lease=gate->acquire();auto application=factory(account,std::move(channel));
            if(!application)throw std::runtime_error("Roster application unavailable");
            return std::make_unique<TrackedApplication>(std::move(lease),std::move(application));
        };
    }
    void require_active() const {
        require_running();
        if(!gate_->accepting())throw std::logic_error("World session owner is stopped");
    }
    void require_running() const {
        if(!strand_.running_in_this_thread())throw std::logic_error("World session owner outside world strand");
        if(shutdown_)throw std::logic_error("World session owner is shut down");
    }
    class Gate final:public std::enable_shared_from_this<Gate> {
        struct Lease {
            Lease(std::shared_ptr<Gate> value,bool resource):gate(std::move(value)),resource(resource){}
            ~Lease(){if(active)gate->release(resource);}
            std::shared_ptr<Gate> gate;bool resource,active=false;
        };
    public:
        std::shared_ptr<void> acquire() {
            auto lease=std::make_shared<Lease>(this->shared_from_this(),false);std::lock_guard lock(mutex_);
            if(!accepting_)throw std::logic_error("World session owner is stopped");
            if(applications_==std::numeric_limits<std::size_t>::max())throw std::overflow_error("Too many roster applications");
            ++applications_;lease->active=true;return lease;
        }
        std::shared_ptr<void> acquire_resource() {
            auto lease=std::make_shared<Lease>(this->shared_from_this(),true);std::lock_guard lock(mutex_);
            if(closed_)throw std::logic_error("World session owner is shut down");
            if(resources_==std::numeric_limits<std::size_t>::max())throw std::overflow_error("Too many retained owner resources");
            ++resources_;lease->active=true;return lease;
        }
        bool stop_if_idle() {std::lock_guard lock(mutex_);if(applications_ || resources_)return false;accepting_=false;closed_=true;return true;}
        void stop() {std::lock_guard lock(mutex_);accepting_=false;}
        bool accepting() const {std::lock_guard lock(mutex_);return accepting_;}
    private:
        void release(bool resource) noexcept {std::lock_guard lock(mutex_);auto& count=resource?resources_:applications_;if(!count)std::terminate();--count;}
        mutable std::mutex mutex_;std::size_t applications_=0,resources_=0;bool accepting_=true,closed_=false;
    };
    struct RetainedAckRouter {
        RetainedAckRouter(std::shared_ptr<void> value,VerticalWorldSession::Strand strand,VerticalWorldSession::Registry& registry,VerticalWorldSessionAckRouter::Leave leave)
            :lease(std::move(value)),router(std::move(strand),registry,std::move(leave)){}
        // Router is destroyed before its registry lease.
        std::shared_ptr<void> lease;VerticalWorldSessionAckRouter router;
    };
    class TrackedApplication final:public net::WorldApplication {
    public:
        TrackedApplication(std::shared_ptr<void> lease,std::unique_ptr<net::WorldApplication> application)
            :lease_(std::move(lease)),application_(std::move(application)){}
        void open() override {application_->open();}
        void receive(const net::Packet& packet) override {application_->receive(packet);}
        void receive_nprotect(const net::Packet& packet) override {application_->receive_nprotect(packet);}
        void close() override {application_->close();}
        void close_with_reason(std::optional<std::uint32_t> reason) override {application_->close_with_reason(reason);}
    private:
        // Destroy the wrapped application before releasing the owner fence.
        std::shared_ptr<void> lease_;std::unique_ptr<net::WorldApplication> application_;
    };
    VerticalWorldSession::Strand strand_;asio::any_io_executor database_;VerticalWorldSession::Registry registry_;
    WorldUsers* runtime_world_users_=nullptr;
    Runtime runtime_;VerticalWorldSessionAdmissionServices admission_;WorldAdmission world_;std::map<std::uint32_t,std::unique_ptr<ZoneServices>> zone_services_;ZoneCompletion zone_completion_;std::unique_ptr<VerticalWorldZoneDisconnect> zone_disconnect_;std::shared_ptr<Gate> gate_=std::make_shared<Gate>();bool shutdown_=false;
};
using VerticalWorldSessionSqlOwner=VerticalWorldSessionOwner<VerticalWorldSessionSqlSave>;
}
