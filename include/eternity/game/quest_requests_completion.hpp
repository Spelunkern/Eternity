#pragma once
#include "eternity/game/quests.hpp"
#include <utility>

namespace eternity::game {
class QuestCompletionWorld {
public:
    virtual ~QuestCompletionWorld()=default;
    virtual std::uint32_t tick_ms()=0;
    virtual RestoredQuest* find_active(std::uint16_t)=0;
    virtual bool find_required_items(RestoredQuest&,bool consume)=0;
    virtual void send_self(const net::Packet&)=0;
    virtual void remove_quest(RestoredQuest&,bool success)=0;
    virtual void fill_game_log_main(std::span<std::uint8_t>)=0;
    virtual void send_game_log(const net::Packet&)=0;
    virtual void award(RestoredQuest&,std::uint8_t variant,std::uint32_t context)=0;
};
// Original QuestEnd 0047ec40 and QuestEndSelect 0047ebe0. Definitions captured
// on entry must survive callbacks; removal may recycle but must retain payload.
void quest_requests_complete(RestoredQuest&,std::uint32_t context,bool success,QuestCompletionWorld&);
void quest_requests_complete_selection(std::uint16_t id,std::uint8_t selection,QuestCompletionWorld&);
// Live owner adapter: Runtime = canonical QuestRuntime, RuntimeWorld = its DB
// sink. Award must call canonical award_quest_reward with the real live user/
// money/QuestRewardWorld. Ports supply clock, inventory, transports and log main;
// find_active must run on the serialized owner executor (or its recursive lock).
template<class Runtime,class RuntimeWorld,class Ports,class Award>
class QuestCompletionOwners final:public QuestCompletionWorld {
public:
    QuestCompletionOwners(Runtime& runtime,RuntimeWorld& database,Ports& ports,Award award):runtime_(runtime),database_(database),ports_(ports),award_(std::move(award)) {}
    std::uint32_t tick_ms() override {return ports_.tick_ms();}
    RestoredQuest* find_active(std::uint16_t id) override {return runtime_.find_active(id);}
    bool find_required_items(RestoredQuest& q,bool consume) override {return ports_.find_required_items(q,consume);}
    void send_self(const net::Packet& p) override {ports_.send_self(p);}
    void remove_quest(RestoredQuest& q,bool success) override {runtime_.remove(q,success,database_);}
    void fill_game_log_main(std::span<std::uint8_t> p) override {ports_.fill_game_log_main(p);}
    void send_game_log(const net::Packet& p) override {ports_.send_game_log(p);}
    void award(RestoredQuest& q,std::uint8_t v,std::uint32_t context) override {award_(q,v,context);}
private:
    Runtime& runtime_;RuntimeWorld& database_;Ports& ports_;Award award_;
};
}
