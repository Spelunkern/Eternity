#include "eternity/game/mob_targeting_lifecycle_experience.hpp"
#include "eternity/game/actor.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <stdexcept>
namespace eternity::game {
namespace {
void nearest() {
    if(std::fegetround()!=FE_TONEAREST)throw std::logic_error("Death experience requires original nearest floating environment");
}
std::int32_t low_word(double value) {
    // All products in these two recovered finite input domains fit signed QWORD.
    const auto integer=static_cast<std::int64_t>(std::trunc(value));
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(integer));
}
}
std::int32_t mob_targeting_user_death_experience(std::uint16_t level,std::int32_t recipient) {
    nearest();
    const auto difference=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(level)-static_cast<std::uint32_t>(recipient));
    if(difference>9)return 0;
    const auto adjusted=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(difference)+1U);
    volatile double scaled=static_cast<double>(adjusted)*0x1.999999999999ap-3;
    volatile double factor=scaled+1.0;
    volatile double product=static_cast<double>(static_cast<unsigned>(level)*50U)*factor;
    const auto result=low_word(product);return result<1?1:result;
}
void mob_targeting_solo_death_reward(std::uint32_t source,MobTargetingSoloDeathServices& s) {
    const auto base=mob_targeting_user_death_experience(s.victim_level(),s.recipient_level());
    volatile double product=static_cast<double>(base)*0x1.3333333333333p+0;
    s.award(source,low_word(product));s.kill_from();s.drop_by_user();
}
void mob_targeting_solo_death_reward(Actor& victim,Actor& recipient,std::uint32_t source,
    MobTargetingSoloDeathWorld& world) {
    if(world.has_party(recipient))throw std::logic_error("Party Death rewards require their separate live party coordinator");
    struct Bridge final:MobTargetingSoloDeathServices {
        Actor& victim;Actor& recipient;MobTargetingSoloDeathWorld& w;
        Bridge(Actor& a,Actor& b,MobTargetingSoloDeathWorld& c):victim(a),recipient(b),w(c) {}
        std::uint16_t victim_level() const noexcept override {return victim.status().level;}
        std::uint16_t recipient_level() const noexcept override {return recipient.status().level;}
        void award(std::uint32_t source,std::int32_t amount) override {
            const auto& benefits=recipient.status().skills.benefits;
            // Selected solo branch and pure owner queries: no intervening callback
            // can have admitted a party before AddExpFromUser samples it.
            const ExperienceAwardOptions options{false,benefits.experience_double!=0,
                benefits.experience_half_bonus!=0,false,benefits.amulet};
            recipient.gain_experience(w.player(recipient),w.status_rules(),w.experience_curve(),
                source,amount,options,w.award_ports(recipient));
        }
        void kill_from() override {w.kill_from(recipient,victim);}
        void drop_by_user() override {w.drop_by_user(victim,recipient);}
    } bridge(victim,recipient,world);
    mob_targeting_solo_death_reward(source,bridge);
}
}
