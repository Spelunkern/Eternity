#include "eternity/game/vertical_entry.hpp"

namespace eternity::game {
bool vertical_entry_roster_allows(const CharacterRoster& roster, std::uint32_t character_id) {
    // ps_dbAgent.exe PacketUserChar 004059e2..00405a32: first matching slot,
    // rename count > 0 and remaining time != 0 reject before DB job 7.
    for (const auto& slot : roster.slots) {
        if (!slot) {
            if (character_id==0) throw std::logic_error("Empty roster slot status residue is unavailable");
            continue;
        }
        if (slot->id==character_id) return !slot->rename && !slot->remaining_time;
    }
    return false;
}
CharacterSelectionResult vertical_entry_select(CharacterSelectionState& state, std::span<const std::uint8_t> packet,
    const CharacterRoster& roster, Players& players, const std::string& username, const VerticalEntryPorts& ports) {
    if (!ports.loaded || !ports.unavailable || !ports.close)
        throw std::invalid_argument("Incomplete character entry handoff");
    CharacterSelectionPorts gate;
    gate.close=ports.close;
    gate.send_db=[&](net::Packet request) {
        // PacketCharacter 0046c0f0 emits 0407, UID and the requested DWORD.
        const auto read=[&](std::size_t offset) {
            std::uint32_t value=0;
            for (unsigned int i=0;i<4;++i) value|=std::uint32_t(request[offset+i])<<(8*i);
            return value;
        };
        const auto id=read(6);
        if (!vertical_entry_roster_allows(roster,id)) {ports.unavailable();return;}
        auto snapshot=players.load(read(2),id,username);
        if (snapshot) ports.loaded(std::move(*snapshot));
        else ports.unavailable();
    };
    return request_character_selection(state,packet,gate);
}
CharacterSelectionResult vertical_entry_select_persisted(CharacterSelectionState& state,std::span<const std::uint8_t> packet,
    const CharacterRoster& roster,Players& players,PointItemStore& products,const std::string& username,
    std::uint8_t max_grow,InventoryReplicaPool& pool,const VerticalEntryPersistencePorts& ports,
    SkillReplicaPool* skills,QuestReplicaPool* quests,PointItemPool* points) {
    if(!ports.loaded || !ports.unavailable || !ports.close)
        throw std::invalid_argument("Incomplete persisted character selection handoff");
    VerticalEntryPorts entry{
        [&](PlayerSnapshot snapshot) {
            auto replica=vertical_persistence_restore_with_products(snapshot,products,max_grow,pool,skills,quests,points);
            ports.loaded(VerticalPersistenceSelection{std::move(snapshot),std::move(replica)});
        },ports.unavailable,ports.close};
    return vertical_entry_select(state,packet,roster,players,username,entry);
}
}
