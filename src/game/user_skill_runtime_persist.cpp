#include "eternity/game/user_skill_runtime_persist.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
std::uint32_t user_skill_runtime_persist(UserSkillRuntime& owner,UserSkillPersistWorld& world,std::span<std::uint8_t,775> scratch){
    // ps_game.exe SendDBAgentCharGetInfo 0046c980, learned phase 0046cab9.
    const auto now=world.tick_ms();const auto account=owner.actor().account_id();const auto count=owner.learned_count();
    if(count>256)throw std::out_of_range("Learned persistence exceeds original 256 slots");
    const auto put=[&](std::size_t at,std::uint32_t value,unsigned size){for(unsigned i=0;i<size;++i)scratch[at+i]=static_cast<std::uint8_t>(value>>(8*i));};
    put(0,0x804,2);put(2,account,4);
    for(std::int32_t i=0;i<count;++i){
        const auto skill=owner.slot(static_cast<std::uint32_t>(i));if(!skill)continue;
        std::uint32_t seconds=0;
        if(skill->reset_tick>now){
            if(!skill->definition)throw std::logic_error("Future learned cooldown without retained definition");
            seconds=std::min((skill->reset_tick-now)/1000U,std::uint32_t(skill->definition->reset_seconds));
        }
        const auto at=7+static_cast<std::size_t>(i)*3;scratch[at]=static_cast<std::uint8_t>(skill->number);put(at+1,seconds,2);
    }
    // Original reads the live count again, even on the signed <=0 loop path.
    const auto wire_count=static_cast<std::uint8_t>(owner.learned_count());scratch[6]=wire_count;
    if(wire_count)world.send_database(net::Packet(scratch.begin(),scratch.begin()+7+std::size_t(wire_count)*3));
    return now; // No owner/Actor/runtime access follows synchronous DB delivery.
}
}
