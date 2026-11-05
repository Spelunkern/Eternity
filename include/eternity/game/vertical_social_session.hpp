#pragma once
#include "eternity/game/vertical_social_owner.hpp"
#include "eternity/game/entry_delivery.hpp"
#include "eternity/game/vertical_social_dispatch.hpp"
#include "eternity/game/vertical_social_db_route.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/world_users.hpp"
#include "eternity/game/world_lifecycle.hpp"
#include "eternity/game/zone_users.hpp"

namespace eternity::game {
template<class Session,class Database> class VerticalSocialPacketJobs;
// Retained with the real session on the serialized world executor. The session,
// WorldUser/Actor, replica, character, database, shared save mutex, registry,
// world indexes and zone-request state outlive this object and its callbacks.
// PacketComm 00475870, UpdateTimeover 0048e310 and ReleaseFriend 004757a0.
template<class Session,class Database> class VerticalSocialSession final {
public:
    using Registry=VerticalSessionRegistry<Session>;
    // The alias retains the complete bundle backing the social session.
    using OwnerLookup=std::function<std::shared_ptr<VerticalSocialSession>(Session&)>;
    struct Services {
        OwnerLookup social_for;
        std::function<std::uint32_t()> tick;
        // Real DB-agent transport/queue; it must preserve account routing and
        // fence outstanding work before the owning session is released.
        std::function<void(net::Packet)> send_db;
        std::function<EntryCatalogUpdate(std::span<const std::uint8_t>)> party_find;
    };
    VerticalSocialSession(Session& session,WorldUser& user,SocialCatalog catalog,ZoneLeaveRequestState& requests,
        VerticalSocialReplica& replica,CharacterReplica& character,Database& database,std::recursive_mutex& save_mutex,
        Registry& registry,WorldUsers& world,Services services,std::string prefix="PS_GameData.dbo.")
        :session_(session),user_(user),catalog_(std::move(catalog)),requests_(requests),
         lifecycle_(replica,character,database,save_mutex,std::move(prefix)),registry_(registry),world_(world),services_(std::move(services)),
         friend_(bind_social_friend_user(catalog_,requests_,character.core().character.id,user.actor.id(),0,0,0,0,"")) {
        auto pin=registry_.retain_actor(user_.actor);const auto core=character.core();
        if(!pin || pin.get()!=&session_ || user_.admission.user_id!=core.user_id || user_.actor.account_id()!=core.user_id ||
           user_.actor.id()!=core.character.id || catalog_.account_id()!=core.user_id || catalog_.character_id()!=core.character.id ||
           !session_.channel().send || !services_.social_for || !services_.tick || !services_.send_db)
            throw std::invalid_argument("Incomplete or mismatched social session owners");
        refresh();
    }
    VerticalSocialSession(const VerticalSocialSession&)=delete;
    VerticalSocialSession& operator=(const VerticalSocialSession&)=delete;
    const SocialCatalog& catalog() const noexcept {return catalog_;}
    std::uint32_t account_id() const noexcept {return catalog_.account_id();}
    bool owns_session(const Session& session) const noexcept {return &session_==&session;}
    bool uses_save_mutex(const std::recursive_mutex& mutex) const noexcept {return lifecycle_.uses_save_mutex(mutex);}
    // Worker-side mutation uses the same mutex as social saves; the session
    // and this owner must be retained until the tracked job has drained.
    EntryCatalogUpdate apply_db(std::span<const std::uint8_t> packet) {return lifecycle_.apply(packet);}
    CharacterSaveLifecyclePorts bind_save(CharacterSaveLifecyclePorts ports) {
        if(!ports.diagnostic)throw std::invalid_argument("Missing social save diagnostics");
        return lifecycle_.bind(std::move(ports));
    }
    EntryCatalogUpdate receive(std::span<const std::uint8_t> packet) {
        Context c(*this);refresh();
        VerticalSocialClientPorts p;
        p.friend_by_name_in_zone=[&](std::string_view name){return c.friend_name(name);};
        p.friend_world_id=[&](std::uint32_t id)->std::optional<std::uint32_t>{auto* u=world_.find_zone_move(id);return u?std::optional<std::uint32_t>{u->actor.id()}:std::nullopt;};
        p.friend_by_id_in_zone=[&](std::uint32_t id){return c.friend_id(id);};
        p.block_by_name=[&](std::string_view name){return c.block_name(name);};
        p.tick=services_.tick;p.send_db=services_.send_db;p.party_find=services_.party_find;
        p.send_user=[&](SocialFriendUser& target,net::Packet packet){c.send_user(target,std::move(packet));};
        p.catalog_by_id=[&](std::uint32_t id){return c.catalog(id);};
        p.send_peer=[&](std::uint32_t id,net::Packet packet){c.send_peer(id,std::move(packet));};
        return vertical_social_client(friend_,packet,p);
    }
    // Retain the calling session and every resolved peer through the existing
    // synchronous EntryDelivery callbacks. Do not retain these ports beyond
    // the session's entry/drain lifetime.
    EntryDeliveryPorts bind_entry(EntryDeliveryPorts ports) {
        auto context=std::make_shared<Context>(*this);
        ports.send_self=[context](net::Packet p){context->send_self(std::move(p));};
        ports.social=[context](std::uint32_t id){return context->catalog(id);};
        ports.send_peer=[context](std::uint32_t id,net::Packet p){context->send_peer(id,std::move(p));};
        return ports;
    }
    // Entry delivery calls these for InitFriend / SendCharacterFriend. The
    // caller must not also invoke EntryDelivery's corresponding social steps.
    void notify_online() {
        Context c(*this);catalog_.notify_online([&](std::uint32_t id){return c.catalog(id);},
            [&](std::uint32_t id,net::Packet p){c.send_peer(id,std::move(p));});
    }
    void send_lists() {
        Context c(*this);catalog_.send_packets([&](std::uint32_t id){return c.catalog(id);},
            [&](net::Packet p){session_.channel().send(std::move(p));});
    }
    bool update_timeout(std::uint32_t tick) {
        Context c(*this);
        const bool guild=zone_guild_invitation_timeout(requests_,tick);
        const bool friend_request=social_friend_timeout(friend_,tick);
        return guild || friend_request;
    }
    WorldLifecyclePorts bind_world_release(WorldLifecyclePorts ports) {
        if(!ports.step)throw std::invalid_argument("Missing world lifecycle step service");
        auto lease=services_.social_for(session_);if(lease.get()!=this)throw std::logic_error("World release lost its social owner");
        auto next=std::move(ports.step);
        ports.step=[this,lease=std::move(lease),next=std::move(next)](WorldLifecycleStep step) {
            if(step!=WorldLifecycleStep::release_friends){next(step);return;}
            Context c(*this,lease);
            catalog_.notify_offline([&](std::uint32_t id){return c.catalog(id);},
                [&](std::uint32_t id,net::Packet p){c.send_peer(id,std::move(p));});
        };
        return ports;
    }
    // Invoke on the world executor; registry lookup pins the actual session
    // through the replica mutation. The returned rejection must be handled.
    static EntryCatalogUpdate route_db(std::span<const std::uint8_t> packet,Registry& registry,const OwnerLookup& owner_for) {
        if(!owner_for)throw std::invalid_argument("Missing social session lookup");
        std::shared_ptr<Session> pin;std::shared_ptr<VerticalSocialSession> social;
        return vertical_social_db_route<Database>(packet,[&](std::uint32_t id)->VerticalSocialLifecycle<Database>* {
            pin=registry.find_account(id);if(!pin)return nullptr;
            social=owner_for(*pin);if(!social || &social->session_!=pin.get())throw std::logic_error("Missing retained social owner");
            return &social->lifecycle_;
        });
    }
private:
    friend class VerticalSocialPacketJobs<Session,Database>;
    bool claim_packet_jobs() {if(packet_jobs_claimed_)return false;packet_jobs_claimed_=true;return true;}
    void release_packet_jobs() {if(!packet_jobs_claimed_)throw std::logic_error("Social packet jobs claim missing");packet_jobs_claimed_=false;}
    void refresh() {
        const auto& shape=user_.actor.appearance().identity;
        friend_.country=shape.country;friend_.family=shape.family;friend_.grow=shape.grow;friend_.job=shape.job;friend_.name=shape.name;
    }
    class Context {
        struct Pin {std::shared_ptr<Session> session;std::shared_ptr<VerticalSocialSession> social;};
    public:
        explicit Context(VerticalSocialSession& self,std::shared_ptr<VerticalSocialSession> social={}):self_(self) {
            auto pin=self_.registry_.retain_actor(self_.user_.actor);
            if(!pin || pin.get()!=&self_.session_)throw std::logic_error("Social session is not retained");
            if(!social)social=self_.services_.social_for(*pin);if(social.get()!=&self_)throw std::logic_error("Social session lost its owner lease");
            pins_.push_back({std::move(pin),std::move(social)});
        }
        SocialFriendUser* friend_name(std::string_view name) {return same_zone(self_.world_.find_zone_move(name));}
        SocialFriendUser* friend_id(std::uint32_t id) {return same_zone(self_.world_.find(id));}
        std::optional<SocialBlockTarget> block_name(std::string_view name) {
            auto* u=self_.world_.find(name);if(!u)return {};
            auto& owner=pin(*u);owner.refresh();const auto& value=owner.friend_;
            return SocialBlockTarget{value.list_id,value.wire_id,value.country,value.name,&owner==&self_};
        }
        const SocialCatalog* catalog(std::uint32_t id) {
            auto* u=self_.world_.find(id);return u?&pin(*u).catalog_:nullptr;
        }
        void send_self(net::Packet packet) {pins_.front().session->channel().send(std::move(packet));}
        void send_user(SocialFriendUser& user,net::Packet packet) {
            for(auto& p:pins_)if(&p.social->friend_==&user){p.session->channel().send(std::move(packet));return;}
            throw std::logic_error("Social peer was not retained");
        }
        void send_peer(std::uint32_t id,net::Packet packet) {
            for(auto it=pins_.rbegin();it!=pins_.rend();++it)if(it->social->user_.actor.id()==id){it->session->channel().send(std::move(packet));return;}
            throw std::logic_error("Social peer was not resolved");
        }
    private:
        VerticalSocialSession& pin(WorldUser& u) {
            for(auto& p:pins_)if(&p.social->user_.actor==&u.actor)return *p.social;
            auto owner=self_.registry_.retain_actor(u.actor);if(!owner)throw std::logic_error("World social peer has no session lease");
            auto social=self_.services_.social_for(*owner);
            if(!social || &social->session_!=owner.get() || &social->user_!=&u)throw std::logic_error("World social peer has no matching owner");
            auto& retained=*social;pins_.push_back({std::move(owner),std::move(social)});return retained;
        }
        SocialFriendUser* same_zone(WorldUser* u) {
            auto* zone=self_.user_.admission.zone;
            if(!u || !zone || u->admission.zone!=zone || !zone->users || zone->users->find_actor(u->actor.id())!=&u->actor)return nullptr;
            auto& social=pin(*u);social.refresh();return &social.friend_;
        }
        VerticalSocialSession& self_;std::vector<Pin> pins_;
    };
    Session& session_;WorldUser& user_;SocialCatalog catalog_;ZoneLeaveRequestState& requests_;
    VerticalSocialLifecycle<Database> lifecycle_;Registry& registry_;WorldUsers& world_;Services services_;SocialFriendUser friend_;bool packet_jobs_claimed_=false;
};
}
