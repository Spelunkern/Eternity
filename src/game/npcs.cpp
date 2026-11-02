#include "eternity/game/npcs.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace eternity::game {
namespace {
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data):data_(data) {}
    std::span<const std::uint8_t> take(std::size_t size) {
        if(size>data_.size()-position_) throw std::runtime_error("Truncated original NPC definitions");
        const auto result=data_.subspan(position_,size);position_+=size;return result;
    }
    std::uint32_t number(unsigned int size=4) {
        std::uint32_t value=0;const auto bytes=take(size);
        for(unsigned int i=0;i<size;++i) value|=static_cast<std::uint32_t>(bytes[i])<<(8*i);
        return value;
    }
    std::uint32_t count() {const auto value=number();if(value>65536) throw std::runtime_error("Invalid original NPC list count");return value;}
    std::span<const std::uint8_t> string(std::size_t limit) {
        const auto length=number();if(length>limit) throw std::runtime_error("Original NPC string exceeds storage");return take(length);
    }
    std::vector<std::uint16_t> words() {
        const auto length=count();std::vector<std::uint16_t> result;result.reserve(length);
        for(std::uint32_t i=0;i<length;++i) result.push_back(static_cast<std::uint16_t>(number(2)));
        return result;
    }
    std::size_t position() const noexcept {return position_;}
private:
    std::span<const std::uint8_t> data_;std::size_t position_=0;
};
std::uint32_t field(const NpcDefinition& npc,std::size_t offset,unsigned int size=4) {
    std::uint32_t result=0;for(unsigned int i=0;i<size;++i) result|=static_cast<std::uint32_t>(npc.record[offset+i])<<(8*i);return result;
}
}
std::uint16_t NpcDefinition::type_id() const noexcept {return static_cast<std::uint16_t>(field(*this,2,2));}
std::int32_t NpcDefinition::model() const noexcept {return std::bit_cast<std::int32_t>(field(*this,4));}
std::int32_t NpcDefinition::move_distance() const noexcept {return std::bit_cast<std::int32_t>(field(*this,8));}
std::int32_t NpcDefinition::move_speed() const noexcept {return std::bit_cast<std::int32_t>(field(*this,12));}
std::int32_t NpcDefinition::country() const noexcept {return static_cast<std::int32_t>(field(*this,16));}
std::string NpcDefinition::name() const {return std::string(record.begin()+20,std::find(record.begin()+20,record.end(),0));}
NpcDefinitions NpcDefinitions::decode(std::span<const std::uint8_t> data) {
    Reader input(data);NpcDefinitions result;
    for(std::size_t group=0;group<result.lists_.size();++group) {
        auto& list=result.lists_[group];list.resize(input.count());
        for(auto& npc:list) {
            auto copy=[&](std::size_t offset,std::size_t size) {const auto bytes=input.take(size);std::copy(bytes.begin(),bytes.end(),npc.record.begin()+static_cast<std::ptrdiff_t>(offset));};
            copy(0,1);copy(2,2);if(group==0) npc.shop_type=static_cast<std::uint8_t>(input.number(1));
            copy(4,12);const auto country=input.number();npc.record[16]=country==1?0:country==2?1:2;
            const auto name=input.string(256);std::copy(name.begin(),name.end(),npc.record.begin()+20);
            npc.record[49]=' ';npc.record[50]=0;input.string(4096);
            if(group==0) {
                npc.shop_items.resize(input.count());
                for(auto& item:npc.shop_items) {item[0]=static_cast<std::uint8_t>(input.number(1));item[1]=static_cast<std::uint8_t>(input.number(1));}
            } else if(group==1) {
                for(auto& gate:npc.gates) {
                    gate.map_id=static_cast<std::uint16_t>(input.number(2));
                    gate.position.x=std::bit_cast<float>(input.number());
                    gate.position.y=static_cast<float>(static_cast<double>(std::bit_cast<float>(input.number()))+static_cast<double>(0.9f));
                    gate.position.z=std::bit_cast<float>(input.number());
                    const auto gate_name=input.string(256);std::copy(gate_name.begin(),gate_name.end(),gate.name.begin());
                    gate.money=input.number();
                }
            }
            npc.start_quests=input.words();npc.end_quests=input.words();
        }
    }
    result.consumed_=input.position();return result;
}
const NpcDefinition* NpcDefinitions::find(std::int32_t type,std::int32_t type_id) const noexcept {
    // GetNpc 0041d640 indexes row position, independently of the embedded type/ID.
    if(type<1 || type>13 || type_id<1) return nullptr;
    const auto& list=lists_[static_cast<std::size_t>(type-1)];
    return static_cast<std::size_t>(type_id)<=list.size()?&list[static_cast<std::size_t>(type_id-1)]:nullptr;
}
std::size_t NpcDefinitions::count(std::int32_t type) const noexcept {return type>=1 && type<=13?lists_[static_cast<std::size_t>(type-1)].size():0;}
}
