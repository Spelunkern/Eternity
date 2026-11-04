#include "eternity/game/quests.hpp"
#include "eternity/game/npcs.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
namespace {
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data):data_(data) {}
    std::span<const std::uint8_t> take(std::size_t size) {
        if(size>data_.size()-position_) throw std::runtime_error("Truncated original NpcQuest.SData");
        const auto result=data_.subspan(position_,size);position_+=size;return result;
    }
    std::uint32_t number() {const auto value=take(4);std::uint32_t result=0;for(unsigned int i=0;i<4;++i) result|=static_cast<std::uint32_t>(value[i])<<(8*i);return result;}
    void string() {const auto size=number();take(size);}
    void words() {const auto count=number();take(static_cast<std::size_t>(count)*2);}
    std::size_t position() const {return position_;}
private:
    std::span<const std::uint8_t> data_;
    std::size_t position_=0;
};
std::uint32_t field(const std::array<std::uint8_t,464>& data,std::size_t offset,unsigned int bytes) {
    std::uint32_t result=0;for(unsigned int i=0;i<bytes;++i) result|=static_cast<std::uint32_t>(data[offset+i])<<(8*i);return result;
}
}
std::uint16_t QuestDefinition::id() const {return static_cast<std::uint16_t>(field(record_,0,2));}
std::string QuestDefinition::name() const {const auto end=std::find(record_.begin()+2,record_.begin()+258,0);return std::string(record_.begin()+2,end);}
std::uint16_t QuestDefinition::minimum_level() const {return static_cast<std::uint16_t>(field(record_,0x102,2));}
std::uint32_t QuestDefinition::minimum_minutes() const {return field(record_,0x124,4);}
std::uint32_t QuestDefinition::maximum_minutes() const {return field(record_,0x128,4);}
std::array<std::uint16_t,2> QuestDefinition::mob_targets() const {return {static_cast<std::uint16_t>(field(record_,0x156,2)),static_cast<std::uint16_t>(field(record_,0x158,2))};}
QuestDefinitions QuestDefinitions::decode(std::span<const std::uint8_t> data) {
    // ps_game.exe CQuestData::Load 004ca5d0. Preserve every read field without native alignment assumptions.
    Reader input(data);const auto count=input.number();if(count>65535) throw std::runtime_error("Original quest definition count exceeds WORD identifiers");
    QuestDefinitions result;result.definitions_.resize(count);
    for(auto& definition:result.definitions_) {
        auto copy=[&](std::size_t offset,std::size_t size) {const auto bytes=input.take(size);std::copy(bytes.begin(),bytes.end(),definition.record_.begin()+static_cast<std::ptrdiff_t>(offset));};
        definition.record_[0x1cc]=1;copy(0,2);const auto length=input.number();if(length>256) throw std::runtime_error("Original quest name exceeds storage");copy(2,length);
        definition.record_[0x1f]=' ';definition.record_[0x20]=0;input.string();
        // Fixed requirement fields, excluding the original structure's padding bytes.
        copy(0x102,2);copy(0x104,2);copy(0x106,10);copy(0x110,4);copy(0x114,3);copy(0x118,2);copy(0x11a,7);
        copy(0x124,20);copy(0x138,17);copy(0x14a,12);copy(0x156,2);copy(0x15a,1);copy(0x158,2);copy(0x15b,3);
        for(std::size_t offset=0x160;offset<0x1c0;offset+=32) {
            copy(offset,5);copy(offset+8,9);copy(offset+20,12);
        }
        for(unsigned int i=0;i<7;++i) input.string();
    }
    result.consumed_=input.position();return result;
}
QuestDefinitions QuestDefinitions::npc_quest(std::span<const std::uint8_t> data) {
    // DBInitializeConstNpcQuest 004caf80: NPC lists, 256x256 item links, then quests.
    const auto npcs=NpcDefinitions::decode(data);Reader input(data);input.take(npcs.consumed());
    for(unsigned int link=0;link<65536;++link) {input.words();input.words();}
    auto result=decode(data.subspan(input.position()));result.consumed_+=input.position();return result;
}
const QuestDefinition* QuestDefinitions::find(std::uint16_t id) const {
    // GetQuest 0046c2c0 indexes by row position, independently of a row's embedded ID.
    return id>0 && id<=definitions_.size()?&definitions_[id-1]:nullptr;
}
RestoredQuests restore_quests(const PlayerSnapshot& player,const QuestDefinitions& definitions,std::uint32_t tick) {
    // PacketUserDBSetInfo 0046d780 cases 0506/0507: restored active success starts at zero.
    RestoredQuests result;
    for(const auto& saved:player.quests) {
        const auto* definition=definitions.find(saved.id);if(!definition) {result.invalid_active.push_back(saved.id);continue;}
        RestoredQuest quest;quest.definition=definition;quest.packet={saved.id,0,definition->maximum_minutes()!=0,saved.counters};
        if(quest.packet.timed) {quest.packet.end_tick=tick+static_cast<std::uint32_t>(saved.delay)*60000U;quest.start_tick=quest.packet.end_tick-definition->maximum_minutes()*60000U;}
        else quest.start_tick=tick-static_cast<std::uint32_t>(saved.delay)*60000U;
        if(definition->minimum_minutes()!=0) quest.success_tick=quest.start_tick+definition->minimum_minutes()*60000U;
        if(definition->end_type()==3) ++result.player_kill_quests;else if(definition->end_type()==2) ++result.mob_kill_quests;
        result.active.push_back(quest);
    }
    for(const auto& saved:player.finished_quests) if(definitions.find(saved.id)) result.finished.push_back(saved);
    return result;
}
}
