#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
namespace eternity::game {
// ps_game.exe stQuestInfo; immutable pointer-free projection of the recovered record.
using QuestStartRecord=std::array<std::uint8_t,464>;
struct QuestStartFinished {std::uint32_t id;std::uint8_t success;};
struct QuestStartUser {
    std::uint16_t level=0;
    std::uint8_t country=0,grow=0,job=0,last_item=0; // Job projection must be 0..5.
    std::uint32_t last_item_deadline=0;
    std::span<const QuestStartFinished> finished;
};
struct QuestStartNpc {std::uint32_t handle=0,type=0,id=0;};
// Every side effect has a required owner; no production no-op defaults.
class QuestStartWorld {
public:
    virtual ~QuestStartWorld()=default;
    virtual std::optional<QuestStartNpc> find_npc(std::uint32_t handle)=0;
    virtual bool active(std::uint16_t id)=0;
    virtual bool finished(std::uint16_t id)=0;
    virtual bool find_item(std::uint8_t type,std::uint8_t id,bool consume)=0;
    virtual std::uint32_t tick_ms()=0;
    virtual bool add(const QuestStartRecord&)=0; // QuestAdd owns allocation, runtime and DB packet.
    virtual void prepare_start_log(const QuestStartRecord&,const std::optional<QuestStartNpc>&)=0;
    virtual void send_self(const std::array<std::uint8_t,8>&)=0;
    virtual void log_start(const QuestStartRecord&,const std::optional<QuestStartNpc>&)=0;
    virtual bool give_item(std::uint8_t type,std::uint8_t id,std::uint8_t count)=0;
};
bool quest_enable_start(const QuestStartUser&,const QuestStartRecord&);
// Definitions are positional: request N selects record N-1, irrespective of record.wID.
bool quest_start_request(const QuestStartUser&,std::span<const QuestStartRecord>,std::uint16_t request,std::uint32_t target,QuestStartWorld&);
void quest_start(const QuestStartRecord&,const std::optional<QuestStartNpc>&,QuestStartWorld&);
}
