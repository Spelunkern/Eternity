#include "eternity/game/user_skill_runtime_snapshot.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace eternity::game {
void user_skill_runtime_snapshot(UserSkillRuntime& owner,UserSkillSnapshotClock& clock,std::span<std::uint8_t,2051> scratch){
    // ps_game.exe SendCharacterSkill 00483540: header precedes clock callback.
    scratch[0]=8;scratch[1]=1;
    const auto now=clock.tick_ms();const auto count=owner.learned_count();
    if(count>256)throw std::out_of_range("Skill snapshot exceeds original 256-slot scratch");
    const auto put=[&](std::size_t at,std::uint32_t value,unsigned size){for(unsigned i=0;i<size;++i)scratch[at+i]=static_cast<std::uint8_t>(value>>(8*i));};
    for(std::int32_t i=0;i<count;++i){
        const auto skill=owner.slot(static_cast<std::uint32_t>(i));if(!skill)continue;
        const auto seconds=std::max(std::bit_cast<std::int32_t>(skill->reset_tick-now)/1000,0);
        const auto at=3+static_cast<std::size_t>(i)*8;
        put(at,skill->skill_id,2);scratch[at+2]=static_cast<std::uint8_t>(skill->level);scratch[at+3]=static_cast<std::uint8_t>(skill->number);put(at+4,static_cast<std::uint32_t>(seconds),4);
    }
    const auto wire_count=static_cast<std::uint8_t>(std::max(count,0));scratch[2]=wire_count;
    const auto size=3+std::size_t(wire_count)*8;
    owner.actor().send_self(net::Packet(scratch.begin(),scratch.begin()+size));
    // No skill/Actor dereference follows the synchronous send callback.
}
}
