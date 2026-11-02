#include "eternity/game/mob_damage.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace eternity::game {
namespace {
const MobDamageRecord& value(const MobDamageRecord& record) {return record;}
const MobDamageRecord& value(MobDamageRecord* record) {return *record;}
template<class Range> bool select_damage(Range records,MobDamageRecord& output) {
    if(records.empty()) return false;
    const auto* best=&value(records[0]);
    for(const auto& record:records) if(best->damage<value(record).damage) best=&value(record);
    if(best->type==5) {
        const auto party=best->id;std::size_t first=0;
        while(first<records.size() && value(records[first]).type!=1) ++first;
        if(first==records.size()) throw std::invalid_argument("Party damage winner lacks a user record");
        best=&value(records[first]);
        // The original starts from the first user even if its party differs.
        for(auto i=first+1;i<records.size();++i) {const auto& record=value(records[i]);if(record.type!=5 && record.party==party && best->damage<record.damage) best=&record;}
    }
    output.type=best->type;output.id=best->id;output.damage=best->damage;output.party=best->party;return true;
}
template<class Range> bool select_aggro(Range records,MobDamageRecord& output) {
    if(records.empty()) return false;
    const auto* best=&value(records[0]);
    for(const auto& record:records) if(best->aggro<value(record).aggro) best=&value(record);
    output.type=best->type;output.id=best->id;output.damage=best->damage;output.aggro=best->aggro;return true;
}
}
bool maximum_mob_damage(std::span<const MobDamageRecord> records,MobDamageRecord& output) {return select_damage(records,output);}
bool maximum_mob_aggro(std::span<const MobDamageRecord> records,MobDamageRecord& output) {return select_aggro(records,output);}
bool MobDamageLedger::maximum_damage(MobDamageRecord& output) const {return select_damage(records(),output);}
bool MobDamageLedger::maximum_aggro(MobDamageRecord& output) const {return select_aggro(records(),output);}
MobDamageRecord* MobDamagePool::allocate() {
    if(!free_.empty()) {auto* record=free_.back();free_.pop_back();return record;}
    auto record=services_.construct();++attempts_;if(!record) return nullptr;
    auto* result=record.get();objects_.push_back(std::move(record));return result;
}
void MobDamagePool::release(MobDamageRecord* record) {free_.push_back(record);}
MobDamageLedger::~MobDamageLedger() {reset();}
void MobDamageLedger::reset() {for(auto* record:records_) pool_.release(record);records_.clear();fair_combat=true;}
void MobDamageLedger::add(std::int32_t type,std::uint32_t id,std::int32_t damage,std::int32_t aggro,std::uint32_t party) {
    const auto add_wrapped=[](std::int32_t a,std::int32_t b){return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(a)+static_cast<std::uint32_t>(b));};
    for(auto* record:records_) if(record->type==type && record->id==id) {
        record->damage=add_wrapped(record->damage,damage);if(type!=5) record->aggro=add_wrapped(record->aggro,aggro);record->party=party;return;
    }
    auto* record=pool_.allocate();if(!record) return;
    record->type=static_cast<std::uint8_t>(type);record->id=id;record->damage=damage;record->aggro=type==5?0:aggro;record->party=party;records_.push_back(record);
}

}
