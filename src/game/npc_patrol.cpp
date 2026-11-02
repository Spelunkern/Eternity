#include "eternity/game/npc_patrol.hpp"
#include <cmath>
#include <stdexcept>

namespace eternity::game {
namespace {
float truncated_position(float value) {
    const auto integral=std::trunc(static_cast<double>(value));
    if(!std::isfinite(integral) || integral<INT32_MIN || integral>INT32_MAX) return static_cast<float>(INT32_MIN);
    return static_cast<float>(static_cast<std::int32_t>(integral));
}
}
void update_npc_patrol(Npc& npc,std::uint32_t tick,RandomSource& random,NpcMovementWorld& world) {
    if(npc.users_using>0 || npc.next_move>tick || !npc.source || npc.source->positions.size()<=1) return;
    if(!npc.definition || npc.next_position<0 || static_cast<std::size_t>(npc.next_position)>=npc.source->positions.size())
        throw std::logic_error("NPC patrol has invalid original definition/path state");
    const auto& target=npc.source->positions[static_cast<std::size_t>(npc.next_position)].position;
    const auto dx=static_cast<float>(static_cast<double>(target.x)-truncated_position(npc.position.x));
    const auto dz=static_cast<float>(static_cast<double>(target.z)-truncated_position(npc.position.z));
    // SVector::Length/Length2 spill the sum to float BEFORE calling CRT sqrt.
    const auto squared=static_cast<float>(static_cast<double>(dx)*dx+static_cast<double>(dz)*dz);
    const auto distance=static_cast<float>(std::sqrt(static_cast<double>(squared)));
    const auto range=static_cast<float>(npc.definition->move_distance());
    if(distance<=range) {
        if(world.move(npc,target,0)) {
            if(npc.forward) {
                ++npc.next_position;
                if(static_cast<std::size_t>(npc.next_position)>=npc.source->positions.size()) {npc.next_position-=2;npc.forward=false;}
            } else {
                --npc.next_position;if(npc.next_position<0) {npc.next_position+=2;npc.forward=true;}
            }
        }
        const auto pause=random.next()%10000;
        npc.next_move=tick+static_cast<std::uint32_t>(npc.definition->move_speed())+10000U+static_cast<std::uint32_t>(pause);
        return;
    }
    UserPosition direction;
    if(dx!=0 || dz!=0) {direction.x=static_cast<float>(static_cast<double>(dx)/distance);direction.y=static_cast<float>(0.0/static_cast<double>(distance));direction.z=static_cast<float>(static_cast<double>(dz)/distance);}
    const UserPosition step{static_cast<float>(static_cast<double>(range)*direction.x),static_cast<float>(static_cast<double>(range)*direction.y),static_cast<float>(static_cast<double>(range)*direction.z)};
    const UserPosition destination{static_cast<float>(static_cast<double>(step.x)+npc.position.x),static_cast<float>(static_cast<double>(step.y)+npc.position.y),static_cast<float>(static_cast<double>(step.z)+npc.position.z)};
    world.move(npc,destination,0);npc.next_move=tick+static_cast<std::uint32_t>(npc.definition->move_speed());
}
}
