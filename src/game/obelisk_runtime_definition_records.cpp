#include "eternity/game/obelisk_runtime_definition_records.hpp"
#include <stdexcept>
#include <utility>
namespace eternity::game {
ObeliskRuntimeDefinitionRecords::ObeliskRuntimeDefinitionRecords(Access access):access_(std::move(access)) {
    if(!access_) throw std::invalid_argument("Obelisk definition marking requires its live record owner");
}
void ObeliskRuntimeDefinitionRecords::mark_obelisk(const MobRuntimeTemplate& definition) {
    auto record=access_(definition.id);
    if(record.size()!=268) throw std::invalid_argument("Obelisk definition owner must expose complete stMOBInfo");
    const auto read=[&](std::size_t offset,unsigned bytes) {
        std::uint32_t value=0;for(unsigned i=0;i<bytes;++i) value|=std::to_integer<std::uint32_t>(record[offset+i])<<(8*i);return value;
    };
    // The binder resolved an admitted canonical runtime template. Reject stale
    // or unrelated raw ownership instead of setting a flag on a second catalog.
    if(definition.id==0||read(0,2)!=definition.id||read(0x22,2)==0||read(0x30,4)==0)
        throw std::invalid_argument("Obelisk runtime template and shared record disagree");
    // ps_game.exe LoadObelisk 00427300: 004281b1/004281f0/0042822e.
    // This is a raw shared flag; no other targeting/skill view changes here.
    record[0x7c]=std::byte{1};
}
}
