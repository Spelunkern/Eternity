#include "eternity/game/mob_death.hpp"
#include "eternity/game/mob_experience.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t scaled_experience(std::int32_t amount,double factor) {
    // Original FISTP qword keeps only its signed low DWORD, including zero on
    // invalid conversion. x87 runs at double precision before truncation.
    const double rounded=static_cast<double>(amount)*factor;
    const auto truncated=std::trunc(static_cast<long double>(rounded));
    if(!std::isfinite(truncated) || truncated<-9223372036854775808.0L || truncated>=9223372036854775808.0L) return 0;
    return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::int64_t>(truncated)));
}
double level_share(std::int32_t maximum,std::uint16_t level) {
    const auto difference=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(maximum)-level);
    return difference<8?1.0:difference<16?0.8:difference<32?0.6:difference<48?0.4:0.2;
}
void append(net::Packet& packet,std::uint32_t value,unsigned bytes=4) {
    for(unsigned i=0;i<bytes;++i) packet.push_back(static_cast<std::uint8_t>(value>>(8*i)));
}
void account(MobRuntime& mob) {
    if(!mob.respawn) throw std::logic_error("Mob death transition lost its respawn owner");
    mob.respawn->next=0xffffffff;
    mob.respawn->current=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(mob.respawn->current)-1U);
}
}
MobDeathExperience::MobDeathExperience() {
    static constexpr std::array<double,39> original={1,1,1,0.73,0.645,0.56,0.475,0.39,1,1,0.73,0.645,0.56,0.475,0.39,0.305,0.27,0.251,0.235,0.219,0.21,0.17,0.14,0.1,0.06,0.03,0.01,0.01,0.01,0.01,0.01,0.01,0.01,0.01,0.01,0.01,0.01,0.01,0.01};
    std::copy_n(original.begin(),31,party.begin());std::copy_n(original.begin()+8,31,raid.begin());
}
void mob_death(MobRuntime& mob,std::int32_t attacker_type,std::uint32_t attacker_id,const MobDeathExperience& rules,MobDeathWorld& world) {
    if(!mob.damage) throw std::invalid_argument("Mob death requires its damage owner");
    bool obelisk_handled=false,linked_handled=false;MobDeathUser* credited=nullptr;
    const bool eligible=mob.damage->fair_combat || (mob.respawn && (mob.respawn->obelisk || mob.respawn->both));
    MobDamageRecord winner;const bool selected=eligible && mob.damage->maximum_damage(winner);
    net::Packet packet{6,6};append(packet,mob.id);append(packet,selected?(winner.type==5?1:winner.type):static_cast<std::uint32_t>(attacker_type),1);append(packet,selected?winner.id:attacker_id);
    if(mob.zone) world.send_death(mob,packet,6);
    if(eligible) {
        if(!selected) world.drop_items(mob,nullptr,nullptr);
        else if(winner.type==1) {
            credited=world.find_user(mob,winner.id);
            if(!credited) world.drop_items(mob,nullptr,nullptr);
            else {
                const auto experience=[&](const MobDeathUser& user) {
                    if(!mob.definition) throw std::invalid_argument("Mob reward requires its definition");
                    return mob_experience(mob.definition->level,mob.definition->experience,user.level,rules.admin_mode,rules.event_active,rules.multiplier);
                };
                if(auto* party=world.party(*credited)) {
                    world.lock_party(*party);
                    try {
                        if(party->type<=1) {
                            const bool raid=party->type==1;
                            for(std::size_t i=0;i<party->members.size();++i) {
                                auto* member=party->members[i];if(!member) throw std::invalid_argument("Mob reward party has a null member");
                                if(!world.in_reward_range(*credited,*member,100)) continue;
                                const auto count=party->members.size();const double factor=count>30?1.0:(raid?rules.raid[count]:rules.party[count]);
                                const auto candidate=scaled_experience(experience(*member),factor);
                                const auto amount=scaled_experience(candidate,level_share(party->maximum_level,member->level));
                                world.add_experience(*member,attacker_id,amount);world.quest_kill(*member,mob);
                            }
                            world.drop_items(mob,credited,party);
                        }
                    } catch(...) {world.unlock_party(*party);throw;}
                    world.unlock_party(*party);
                } else {
                    world.add_experience(*credited,attacker_id,scaled_experience(experience(*credited),1.2));
                    world.drop_items(mob,credited,nullptr);world.quest_kill(*credited,mob);
                }
                if(mob.respawn) {
                    if(mob.respawn->obelisk) {
                        world.destroy_obelisk(mob,credited->country,mob.respawn->type);account(mob);obelisk_handled=true;
                    } else if(mob.respawn->both) {
                        world.destroy_linked_group(mob,mob.respawn->type);account(mob);linked_handled=true;
                    }
                }
            }
        }
    }
    if(mob.lua_ai || (mob.respawn && mob.respawn->both)) world.log_death(mob,credited,mob.damage->fair_combat);
    if(mob.respawn) {
        if(mob.respawn->obelisk) {
            if(!obelisk_handled) if(const auto country=world.obelisk_country(mob,mob.respawn->type)) {
                world.destroy_obelisk(mob,*country==0?1:*country==1?0:2,mob.respawn->type);account(mob);
            }
        } else if(mob.respawn->obelisk_source) world.remove_obelisk_member(mob,mob.respawn->obelisk_source);
        else if(mob.respawn->both) {
            if(!linked_handled) {world.destroy_linked_group(mob,mob.respawn->type);account(mob);}
        } else if(mob.respawn->both_source) world.remove_linked_member(mob,mob.respawn->both_source);
    }
    mob.damage->reset();world.clear_applied_skills(mob);
}
}
