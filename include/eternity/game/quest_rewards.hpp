#pragma once
#include "eternity/game/quests.hpp"
#include "eternity/game/equipment.hpp"

namespace eternity::game {
struct QuestRewardItemReceipt {
    std::uint8_t bag=0,slot=0;
    const ItemDefinition* definition=nullptr;
    std::string name; // Original bytes, at most 31; definition storage survives delivery.
};
class QuestRewardWorld {
public:
    virtual ~QuestRewardWorld()=default;
    virtual void add_experience(std::uint32_t source,std::int32_t amount,bool bypass)=0;
    virtual void send_database(const net::Packet&)=0;
    virtual void fill_game_log_main(std::span<std::uint8_t>)=0;
    virtual std::optional<QuestRewardItemReceipt> receive_item(std::uint8_t type,std::uint8_t type_id,std::uint8_t count)=0;
    virtual void send_self(const net::Packet&)=0;
    // CUser::QuestRemove, success=true; retain the quest through log delivery.
    virtual void remove_quest(RestoredQuest&,bool success)=0;
    virtual void send_game_log(const net::Packet&)=0;
};
// ps_game.exe CUser::QuestSuccessResult 0047ee40. The owner binds the user's live
// money and original UID, not the character/runtime ID. This does not substitute
// the required actor experience, QuestAddItem or QuestRemove providers.
void award_quest_reward(RestoredQuest&,std::uint8_t variant,std::uint32_t context,std::uint32_t user_id,std::uint32_t& money,QuestRewardWorld&);
}
