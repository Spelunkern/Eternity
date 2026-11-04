#include "eternity/game/mob_drops.hpp"
#include "eternity/game/mob_drops_range.hpp"
#include <bit>
#include <charconv>
#include <stdexcept>

namespace eternity::game {
MobDropDefinitions MobDropDefinitions::load(std::size_t count,std::span<const MobDropRow> rows) {
    MobDropDefinitions result;result.mobs.resize(count);
    for(std::size_t i=0;i<rows.size();++i) {
        const auto& row=rows[i];
        if(row.mob>=count || row.order>=10) {result.rejected_rows.push_back(i);continue;}
        result.mobs[row.mob][row.order]={row.grade,row.rate};
    }
    return result;
}
MobDropDefinitions MobDropDefinitions::load_sql(std::size_t count,const storage::ResultSet& table) {
    if(table.columns.size()!=4) throw std::invalid_argument("MobItems procedure must return four columns");
    std::vector<MobDropRow> rows;rows.reserve(table.rows.size());
    for(const auto& row:table.rows) {
        if(row.size()!=4) throw std::invalid_argument("MobItems row must contain four columns");
        const auto number=[&](std::size_t at,std::int64_t low,std::int64_t high) {
            if(!row[at]) throw std::invalid_argument("MobItems NULL has no deterministic original value");
            const auto& text=*row[at];std::int64_t value=0;const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
            if(parsed.ec!=std::errc{} || parsed.ptr!=text.data()+text.size() || value<low || value>high) throw std::invalid_argument("MobItems value exceeds original ODBC type");
            return value;
        };
        rows.push_back({static_cast<std::uint16_t>(number(0,INT16_MIN,UINT16_MAX)),static_cast<std::uint8_t>(number(1,0,UINT8_MAX)),static_cast<std::uint16_t>(number(2,INT16_MIN,UINT16_MAX)),static_cast<std::uint32_t>(number(3,INT32_MIN,UINT32_MAX))});
    }
    return load(count,rows);
}
void mob_drop_slots(const MobDropDefinition& definition,RandomSource& random,ServerRandom& server,MobDropProvider& world) {
    for(std::size_t slot=1;slot<definition.size();++slot) {
        const auto& entry=definition[slot];if(entry.grade==0) continue;
        const auto roll=slot<7?static_cast<std::uint32_t>(random.next()%100):server.next(1000000);
        if(roll>=entry.rate) continue;
        const auto item=world.select_item(entry.grade);
        if(item) world.deliver_item(item,1,false);
    }
}
bool mob_drop_limit(MobDropLimit& limit,std::uint32_t tick,RandomSource& random) {
    if(limit.maximum==0) return true;
    if(tick<limit.next_tick || static_cast<std::int32_t>(limit.maximum)<=limit.count) return false;
    if(limit.random_delay==0) throw std::invalid_argument("Original drop random delay must be nonzero");
    limit.count=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(limit.count)+1U);
    limit.next_tick=static_cast<std::uint32_t>(random.next())%limit.random_delay+limit.delay+tick;
    return true;
}
}

#include <algorithm>
#include <cmath>
#include <limits>
namespace eternity::game {
namespace {
MobLootUser* leader(MobLootParty& p) {
    if(p.leader<0 || static_cast<std::size_t>(p.leader)>=p.members.size()) throw std::invalid_argument("Invalid original party leader index");
    return p.members[static_cast<std::size_t>(p.leader)];
}
std::int32_t original_int(float value) {
    if(!std::isfinite(value) || value>=2147483648.0F || value< -2147483648.0F) return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(value);
}
std::int32_t add_wrap(std::int32_t a,std::int32_t b) {return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));}

}
void mob_loot(const MobLootContext& mob,MobLootUser* user,MobLootParty* party,RandomSource& random,ServerRandom& server,MobLootWorld& world) {
    std::uint32_t owner=0;
    if(user) {
        owner=user->id;
        if(party && party->mode==4) {auto* selected=leader(*party);if(world.zone_item(*user,selected)) {if(!selected) throw std::logic_error("Zone provider accepted null leader");owner=selected->id;}}
        if(mob.quest_type==0) owner=user->id;
    }
    if(mob.quest_type) {
        if(const auto* item=world.item(mob.quest_type,mob.quest_type_id)) {
            bool eligible=item->quest_start;
            if(!eligible && user) {
                for(auto quest:world.end_quests(*item)) {
                    if(party) {for(auto* member:party->members) {if(member && world.has_quest(*member,quest)) {eligible=true;break;}}}
                    else eligible=world.has_quest(*user,quest);
                    if(eligible) break;
                }
            }
            if(eligible && random.next()%100<item->quality) world.deliver(*item,user,party,false);
        }
    }
    // Same loop as mob_drop_slots, retaining resolved item pointers explicitly.
    for(std::size_t i=1;i<mob.slots.size();++i) {
        const auto& slot=(mob.live_slots?*mob.live_slots:mob.slots)[i];if(!slot.grade) continue;
        const auto roll=i<7?static_cast<std::uint32_t>(random.next()%100):server.next(1000000);
        if(roll<slot.rate) if(const auto* item=world.grade_item(slot.grade)) world.deliver(*item,user,party,false);
    }
    if(mob.zone_type==5 && mob.money_base==0 && mob.money_random!=0) {
        if(user) world.guild_ranking(*user,mob.money_random);
        return;
    }
    if(mob.money_base) if(const auto* item=world.sequence_item(mob.level,mob.object_id)) world.deliver(*item,user,party,true);
    if(random.next()%100>=30) return;
    auto amount=static_cast<std::int32_t>(mob.money_base);
    if(mob.money_random) amount+=random.next()%mob.money_random;
    if(amount<=0) return;
    if(mob.logged) world.log_money(amount);
    if(!party) {world.ground_money(owner,amount);return;}
    const auto size=party->members.size();
    // Original float division converts indefinite to INT_MIN for an empty party,
    // clamps to one, then visits no members.
    auto share=size?original_int(static_cast<float>(amount)/static_cast<float>(size)):1;
    if(share<1) share=1;
    for(auto* member:party->members) {
        if(!member) throw std::invalid_argument("Original money loop requires non-null party members");
        if(!user) throw std::invalid_argument("Original party money loop requires credited user");
        if(!mob_drop_money_range_enabled(*user,*member)) continue;
        auto bonus=member->money_percent;
        if(member->money_mode==2) bonus=add_wrap(bonus,20);
        else if(member->money_mode==3) bonus=add_wrap(bonus,50);
        if(bonus>0) bonus=original_int((static_cast<float>(bonus)/100.0F)*static_cast<float>(share));
        world.give_money(*member,add_wrap(share,bonus));
    }
}
void mob_deliver_item(const MobLootItem* definition,MobLootUser* user,MobLootParty* party,bool sequence,bool logged,RandomSource& random,MobLootDeliveryWorld& world) {
    if(!definition) return;
    std::uint32_t owner=user?user->id:0;
    if(user && party) {
        if(party->mode==4) {auto* selected=leader(*party);if(world.zone_item(*user,selected)) {if(!selected) throw std::logic_error("Zone provider accepted null leader");owner=selected->id;}}
        if(definition->real_type==27 || definition->dex==0) owner=user->id;
    }
    auto* object=world.create(*definition,owner,party?party->id:0);if(!object) return;
    if(sequence) world.sequence_log(*object,user);
    if(definition->real_type!=27 && party) {
        if(party->type==1 && party->mode==4) {
            auto* selected=leader(*party);
            if(selected && world.range_item(*selected,user,100)) {
                if(logged) world.log_item(*object,selected);
                if(definition->dex) world.give_item(*selected,*object);
                else world.enter_item(*object,2);
                return;
            }
        }
        if(party->mode==2) {
            if(party->members.empty()) throw std::invalid_argument("Original random loot requires nonempty party");
            auto* selected=party->members[static_cast<std::size_t>(random.next())%party->members.size()];
            if(selected && world.range_item(*selected,user,100)) {
                world.give_item(*selected,*object);
                if(logged) world.log_item(*object,selected);
                return;
            }
        } else if(party->mode==1 || party->mode==3) {
            const bool ground=party->mode==3;const auto count=party->members.size();
            for(std::size_t i=0;i<(ground?count:party->members.size());++i) {
                if(party->cursor<0) throw std::invalid_argument("Negative original loot cursor");
                if(static_cast<std::size_t>(party->cursor)>=party->members.size()) party->cursor=0;
                auto* selected=party->members[static_cast<std::size_t>(party->cursor)];
                if(selected && world.range_item(*selected,user,100)) {
                    if(logged) world.log_item(*object,selected);
                    if(!ground) world.give_item(*selected,*object);
                    else {object->party=0;object->owner=selected->id;}
                    party->cursor=add_wrap(party->cursor,1);
                    if(ground) world.enter_item(*object,2);
                    return;
                }
                party->cursor=add_wrap(party->cursor,1);
            }
        }
    }
    if(logged) world.log_item(*object,nullptr);
    world.enter_item(*object,2);
}
}

namespace eternity::game {
const MobLootItem* MobDropCatalog::select(std::uint32_t grade,RandomSource& random,MobDropCatalogWorld& world) {
    if(grade>=grades.size() || grades[grade].empty()) return nullptr;
    auto* entry=grades[grade][static_cast<std::size_t>(random.next())%grades[grade].size()];
    if(!entry) throw std::invalid_argument("Original grade catalog requires non-null definitions");
    const auto tick=world.tick();world.lock();
    struct Unlock {MobDropCatalogWorld& world;~Unlock(){world.unlock();}} unlock{world};
    return mob_drop_limit(entry->limit,tick,random)?&entry->item:nullptr;
}
const MobLootItem* MobDropCatalog::sequence(std::uint16_t level,std::uint32_t object,RandomSource& random,MobDropCatalogWorld& world) {
    if(level==0 || level>=100) return nullptr;
    for(const auto& sequence:sequences) {
        if(sequence.divisor==0) throw std::invalid_argument("Original sequence divisor must be nonzero");
        if(object && object%sequence.divisor==0) {
            const auto grade=sequence.grades[level/10];
            return grade?select(grade,random,world):nullptr;
        }
    }
    return nullptr;
}
}

namespace eternity::game {
MobLootPosition mob_drop_position(float x,float z,RandomSource& random) {
    const auto dx=static_cast<float>(static_cast<double>(random.next()%1000)/1000.0-0.5);
    const auto dz=static_cast<float>(static_cast<double>(random.next()%1000)/1000.0-0.5);
    return {x+dx,0,z+dz};
}
}
