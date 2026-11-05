#pragma once
#include "eternity/game/vertical_move_channel_send_world.hpp"
#include "eternity/game/vertical_move_runtime_composition.hpp"

namespace eternity::game {
// World-strand composition over the real party send owner, world index and
// session registry. The lease retains those owners, zones and every service
// below; it must not retain a session, directory or this object. The party send
// owner supplies the same pool used by party operations. The world must stop
// producers, fence transport IO and drain movement before releasing the lease.
template<class Session> class VerticalMoveSharedWorld final:public std::enable_shared_from_this<VerticalMoveSharedWorld<Session>> {
public:
    using Registry=VerticalSessionRegistry<Session>;
    using Shop=typename VerticalMoveShopShared<Session>::Shop;
    using GuildRefresh=typename VerticalMoveRetainedInteractions<Session>::GuildRefresh;
    using LogBytes=typename VerticalMoveRetainedPvp<Session>::LogBytes;
    using StackBytes=typename VerticalMoveRetainedSession<Session>::StackBytes;
    static std::shared_ptr<VerticalMoveSharedWorld> create(std::shared_ptr<void> lease,Registry& registry,WorldUsers& users,
        PartyMembershipSendWorld& party,Shop shop,DuelWorld& duel,VerticalGuildDuelMovementServices& guild,
        GuildRefresh refresh,LogBytes log_bytes,const std::array<float,5>& speeds,VerticalMoveBoundOperations::Clock clock,
        StackBytes stack_bytes) {
        if(!lease || !shop || !refresh || !log_bytes || !clock || !stack_bytes)
            throw std::invalid_argument("Incomplete shared movement world owners");
        return std::shared_ptr<VerticalMoveSharedWorld>(new VerticalMoveSharedWorld(std::move(lease),registry,users,party,
            std::move(shop),duel,guild,std::move(refresh),std::move(log_bytes),speeds,std::move(clock),std::move(stack_bytes)));
    }
    template<class Gameplay,class Strand>
    std::shared_ptr<VerticalMoveRuntimeComposition<Gameplay,Strand,Session>> composition(Strand strand,typename Gameplay::Other other) {
        using Composition=VerticalMoveRuntimeComposition<Gameplay,Strand,Session>;
        if(composition_created_)throw std::logic_error("Shared movement world already has a composition");
        auto self=this->shared_from_this();
        auto retained=std::make_shared<std::weak_ptr<Composition>>();
        auto result=Composition::create(std::move(strand),registry_,
            [self](Session& owner)->typename Composition::Resources{return {self,self->channel_,owner.party_user(),self->shop_(owner)};},
            [self,retained](Session&,RuntimeZone&,typename Composition::Directory&)->VerticalMoveDirectoryServices<Session> {
                return {self,self->duel_,self->guild_,self->refresh_,self->log_bytes_,self->buffers_,
                    [](Session& owner)->PartyMembershipUser&{return owner.party_user();},
                    [retained](Session& owner)->UserPriorityQueue& {
                        auto composition=retained->lock();
                        if(!composition)throw std::logic_error("Movement recipient lost its composition");
                        return composition->resources(owner)->queue();
                    },self->shop_,self->speeds_,self->clock_,self->stack_bytes_};
            },std::move(other));
        *retained=result;composition_created_=true;return result;
    }
    PartyMembershipSendWorld& send_world() noexcept {return channel_;}
    UserViewSharedBuffers& buffers() noexcept {return buffers_;}
private:
    VerticalMoveSharedWorld(std::shared_ptr<void> lease,Registry& registry,WorldUsers& users,PartyMembershipSendWorld& party,
        Shop shop,DuelWorld& duel,VerticalGuildDuelMovementServices& guild,GuildRefresh refresh,LogBytes log_bytes,
        const std::array<float,5>& speeds,VerticalMoveBoundOperations::Clock clock,StackBytes stack_bytes)
        :lease_(std::move(lease)),registry_(registry),shop_(std::move(shop)),duel_(duel),guild_(guild),refresh_(std::move(refresh)),
         log_bytes_(std::move(log_bytes)),speeds_(speeds),clock_(std::move(clock)),stack_bytes_(std::move(stack_bytes)),
         channel_(party,users,registry,[](Session& owner)->PartyMembershipUser&{return owner.party_user();},
            [](Session& owner)->net::WorldChannel&{return owner.channel();}),buffers_(channel_) {}
    std::shared_ptr<void> lease_;Registry& registry_;Shop shop_;DuelWorld& duel_;VerticalGuildDuelMovementServices& guild_;
    GuildRefresh refresh_;LogBytes log_bytes_;const std::array<float,5>& speeds_;VerticalMoveBoundOperations::Clock clock_;StackBytes stack_bytes_;
    VerticalMoveChannelSendWorld<Session> channel_;UserViewSharedBuffers buffers_;bool composition_created_=false;
};
}
