#pragma once
#include "eternity/game/vertical_move_shop_services.hpp"
#include "eternity/game/user_view_shared_cells.hpp"

namespace eternity::game {
// Concrete shop-shutdown broadcaster using the same buffer pool, connection
// projections, zone membership and session registry as retained movement.
// Pool, all sessions and actors survive every send and pending completion;
// the serialized world owner must bind accessors to each session's real state.
template<class Session> class VerticalMoveShopShared final:public ZoneShopShutdownServices {
public:
    using Shop=typename VerticalMoveShopServices<Session>::Shop;
    using Connection=std::function<PartyMembershipUser&(Session&)>;
private:
    class Recipients final:public UserViewSharedCellServices {
    public:
        Recipients(UserViewSharedBuffers& buffers,VerticalSessionRegistry<Session>& registry,Connection& connection)
            :UserViewSharedCellServices(buffers),registry_(registry),connection_(connection) {}
        std::shared_ptr<void> retain(Actor& actor) override {return pin(actor);}
        PartyMembershipUser& connection(Actor& actor) override {auto owner=pin(actor);return connection_(*owner);}
        UserViewLiveAppearance appearance(const Actor&) override {throw std::logic_error("Unexpected shop appearance request");}
        void leave_snapshot(CellCoordinates) override {throw std::logic_error("Unexpected shop leave snapshot");}
        void enter_snapshot(CellCoordinates) override {throw std::logic_error("Unexpected shop enter snapshot");}
    private:
        std::shared_ptr<Session> pin(Actor& actor) {
            auto owner=registry_.retain_actor(actor);if(!owner)throw std::logic_error("Shop recipient has no retained session");return owner;
        }
        VerticalSessionRegistry<Session>& registry_;Connection& connection_;
    };
    static ZoneUsers& users(RuntimeZone& zone) {
        if(!zone.users)throw std::invalid_argument("Shop broadcast requires zone users");return *zone.users;
    }
public:
    VerticalMoveShopShared(Actor& actor,RuntimeZone& zone,VerticalSessionRegistry<Session>& registry,
        UserViewSharedBuffers& buffers,Connection connection,Shop shop)
        :connection_(std::move(connection)),recipients_(buffers,registry,connection_),transport_(users(zone),recipients_),
         services_(actor,zone,registry,transport_,std::move(shop)) {
        if(!connection_)throw std::invalid_argument("Shop broadcast requires connection accessor");
    }
    Actor* find_customer(Actor& actor,std::uint32_t id) override{return services_.find_customer(actor,id);}
    std::shared_ptr<void> retain(Actor& actor) override{return services_.retain(actor);}
    ZoneLeaveShopState& shop_state(Actor& actor) override{return services_.shop_state(actor);}
    std::optional<CellCoordinates> cell(Actor& actor) override{return services_.cell(actor);}
    void send_view(CellCoordinates cell,const net::Packet& packet) override{services_.send_view(cell,packet);}
    VerticalMoveShopShared(const VerticalMoveShopShared&)=delete;
    VerticalMoveShopShared& operator=(const VerticalMoveShopShared&)=delete;
private:
    Connection connection_;Recipients recipients_;UserViewCellTransport transport_;VerticalMoveShopServices<Session> services_;
};
}
