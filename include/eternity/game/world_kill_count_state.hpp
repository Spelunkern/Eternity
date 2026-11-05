#pragma once
#include "eternity/game/blessing.hpp"
#include "eternity/game/world_kill_count_timer.hpp"
#include <array>
#include <bit>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace eternity::game {
struct WorldKillCountCountry {
    std::uint32_t count=0,index=0,next_threshold=0;
    WorldKillCountDeadlines deadlines;
    std::array<std::int32_t,4> applied_modifiers{};
};
// CWorld::SetKillCount 004138e0 and ApplyKillCount 004139c0. Bootstrap is
// supplied by the actual persisted world state; this object owns it afterward.
template<class Root> class WorldKillCountState final:public WorldKillCountTimerServices {
public:
    using Clock=std::function<std::uint32_t()>;
    using Database=std::function<void(const net::Packet&)>;
    using Disabled=std::function<bool()>;
    using GameLog=std::function<void(const net::Packet&)>;
    static std::shared_ptr<WorldKillCountState> create_bound(std::shared_ptr<Root> root,
        std::shared_ptr<const BlessingDefinitions> definitions,std::array<WorldKillCountCountry,2> initial,Clock tick,Database database,Disabled disabled,GameLog game_log) {
        auto state=std::make_shared<WorldKillCountState>(root,std::move(definitions),std::move(initial),std::move(tick),std::move(database),std::move(disabled),std::move(game_log));
        root->bind_world_kill_count_timer(state);return state;
    }
    WorldKillCountState(std::shared_ptr<Root> root,std::shared_ptr<const BlessingDefinitions> definitions,
        std::array<WorldKillCountCountry,2> initial,Clock tick,Database database,Disabled disabled,GameLog game_log)
        :root_(root),definitions_(std::move(definitions)),countries_(std::move(initial)),tick_(std::move(tick)),database_(std::move(database)),disabled_(std::move(disabled)),game_log_(std::move(game_log)) {
        if(!root || !definitions_ || !tick_ || !database_ || !disabled_ || !game_log_)throw std::invalid_argument("World kill-count state requires actual root, definitions, clock, DB, enable state and log owners");
        for(std::uint8_t country=0;country<2;++country)
            if(!definitions_->find(country,static_cast<std::int32_t>(countries_[country].index)))throw std::invalid_argument("World kill-count bootstrap has no status definition");
    }
    const BlessingDefinitions& definitions()const noexcept{return *definitions_;}
    const WorldKillCountCountry& country(std::uint8_t country)const{return countries_.at(country);}
    WorldKillCountDeadlines& deadlines(std::uint8_t country) override{return countries_.at(country).deadlines;}
    std::uint32_t count(std::uint8_t country) override{return countries_.at(country).count;}
    void send_database(const net::Packet& packet) override{database_(packet);}
    void set_zero(std::uint8_t country) override{(void)set_count(country,0);}
    // CClientToDBAgent::PacketSystemConnect 00408c00 case 0205 and
    // CWorld::InitKillCount 00413150. The DB-agent payload has two DWORDs.
    // Malformed packets or missing definitions leave the retained state intact.
    bool initialize_from_database(std::span<const std::uint8_t> packet) {
        auto root=root_.lock();if(!root || !root->world_accepts_initialization())throw std::logic_error("DB-agent kill-count initialization requires an accepting root");
        if(packet.size()!=10 || packet[0]!=0x05 || packet[1]!=0x02)
            throw std::invalid_argument("Invalid DB-agent kill-count initialization packet");
        std::array<std::uint32_t,2> counts{};std::array<const BlessingDefinition*,2> definitions{};
        for(std::size_t country=0;country<2;++country) {
            for(std::size_t byte=0;byte<4;++byte)counts[country]|=static_cast<std::uint32_t>(packet[2+country*4+byte])<<(byte*8);
            definitions[country]=definitions_->by_count(static_cast<std::uint8_t>(country),counts[country]);
            if(!definitions[country])throw std::logic_error("DB-agent kill-count has no status definition");
        }
        auto updated=countries_;
        for(std::uint8_t country=0;country<2;++country) {
            auto& state=updated[country];const auto& definition=*definitions[country];
            state.count=counts[country];state.index=definition.index;
            if(static_cast<std::size_t>(state.index)+1<definitions_->size(country)) {
                const auto* next=definitions_->find(country,static_cast<std::int32_t>(state.index+1));
                if(!next)throw std::logic_error("DB-agent kill-count next status is unavailable");
                state.next_threshold=next->kill_count;
            } else {
                const auto now=tick_();if(!root->world_accepts_initialization())return false;
                state.deadlines.reset=now+600000U;
            }
            for(std::size_t i=0;i<state.applied_modifiers.size();++i)state.applied_modifiers[i]=definition.modifiers[24+i];
        }
        if(!root->world_accepts_initialization())return false;countries_=updated;return true;
    }
    bool status14(std::uint8_t country)const{return countries_.at(country).deadlines.reset!=0;}
    // CWorld::AddKillCount 00413600. Both country reset deadlines inhibit changes.
    void add_count(std::uint8_t country,std::int32_t amount) {
        auto root=live_root();auto& state=countries_.at(country);
        if(disabled_() || !root->world_send_running() || blocked())return;
        const auto now=tick_();if(!root->world_send_running())return;
        state.count+=static_cast<std::uint32_t>(amount);
        update_after_increase(*root,country,now);
    }
    // CWorld::AddKillCountGrade 00413410 returns false for disabled, locked
    // or negative target grades; the signed upper target is clamped to max.
    bool adjust_grade(std::uint8_t country,std::int32_t delta) {
        auto root=live_root();if(disabled_() || !root->world_send_running() || blocked() || country>1)return false;
        auto& state=countries_[country];
        auto target=std::bit_cast<std::int32_t>(state.index+static_cast<std::uint32_t>(delta));
        const auto maximum=static_cast<std::int32_t>(definitions_->size(country)-1);
        if(target>maximum)target=maximum;
        const auto* definition=definitions_->find(country,target);if(!definition)return false;
        const auto now=tick_();if(!root->world_send_running())return false;
        state.count=definition->kill_count;update_after_increase(*root,country,now);
        return root->world_send_running();
    }
    // CWorld::SubKillCount 00413750 uses signed 32-bit subtraction/clamping,
    // then compares the unsigned result to the previous grade's threshold.
    void subtract_count(std::uint8_t country,std::int32_t amount) {
        auto root=live_root();auto& state=countries_.at(country);
        if(disabled_() || !root->world_send_running() || blocked())return;
        const auto now=tick_();if(!root->world_send_running())return;
        const auto difference=state.count-static_cast<std::uint32_t>(amount);
        state.count=std::bit_cast<std::int32_t>(difference)<0?0:difference;
        if(state.index==0)return;
        const auto* previous=definitions_->find(country,static_cast<std::int32_t>(state.index-1));
        if(!previous)throw std::logic_error("World kill-count previous status is unavailable");
        if(state.count>=previous->kill_count)return;
        const auto* definition=definitions_->by_count(country,state.count);if(!definition)return;
        state.index=definition->index;
        if(const auto* next=definitions_->find(country,static_cast<std::int32_t>(state.index+1)))state.next_threshold=next->kill_count;
        apply(*root,country,*definition,false);state.deadlines.send=now+600000U;
    }
    // False means the stored count already matched, or shutdown interrupted
    // before the mutation. Later callback failures leave earlier effects intact.
    bool set_count(std::uint8_t country,std::uint32_t value) {
        auto root=root_.lock();if(!root || !root->world_send_running())throw std::logic_error("World kill-count state requires a running root");
        auto& state=countries_.at(country);if(state.count==value)return false;
        const auto now=tick_();if(!root->world_send_running())return false;
        state.count=value;const auto* definition=definitions_->by_count(country,value);if(!definition)return true;
        state.index=definition->index;
        if(static_cast<std::size_t>(state.index)+1<definitions_->size(country)) {
            const auto* next=definitions_->find(country,static_cast<std::int32_t>(state.index+1));
            if(!next)throw std::logic_error("World kill-count next status is unavailable");
            state.next_threshold=next->kill_count;
        } else state.deadlines.reset=now+600000U;
        for(std::size_t i=0;i<state.applied_modifiers.size();++i)state.applied_modifiers[i]=definition->modifiers[24+i];
        root->request_kill_count_apply(country);state.deadlines.send=now+600000U;
        return true;
    }
private:
    std::shared_ptr<Root> live_root()const {auto root=root_.lock();if(!root || !root->world_send_running())throw std::logic_error("World kill-count state requires a running root");return root;}
    bool blocked()const noexcept{return countries_[0].deadlines.reset!=0 || countries_[1].deadlines.reset!=0;}
    void update_after_increase(Root& root,std::uint8_t country,std::uint32_t now) {
        auto& state=countries_[country];if(state.count<state.next_threshold)return;
        const auto* definition=definitions_->by_count(country,state.count);if(!definition)return;
        state.index=definition->index;const bool maximum=static_cast<std::size_t>(state.index)+1>=definitions_->size(country);
        if(maximum)state.deadlines.reset=now+600000U;
        else {const auto* next=definitions_->find(country,static_cast<std::int32_t>(state.index+1));if(!next)throw std::logic_error("World kill-count next status is unavailable");state.next_threshold=next->kill_count;}
        apply(root,country,*definition,maximum);state.deadlines.send=now+600000U;
        if(!maximum || !root.world_send_running())return;
        reset_country(root,static_cast<std::uint8_t>(1-country));if(!root.world_send_running())return;
        game_log_(net::Packet{0x01,0x0a,country});
    }
    void apply(Root& root,std::uint8_t country,const BlessingDefinition& definition,bool reset_obelisks) {
        auto& state=countries_[country];for(std::size_t i=0;i<state.applied_modifiers.size();++i)state.applied_modifiers[i]=definition.modifiers[24+i];
        root.request_kill_count_apply(country);if(reset_obelisks)root.request_obelisk_reset();
    }
    void reset_country(Root& root,std::uint8_t country) {
        (void)set_count(country,0);if(!root.world_send_running())return;
        root.request_kill_count_send(country);const auto now=tick_();if(!root.world_send_running())return;
        countries_[country].deadlines.send=now+600000U;countries_[country].deadlines.reset=0;
        net::Packet packet{0x05,0x02};for(const auto& state:countries_)for(unsigned byte=0;byte<4;++byte)packet.push_back(static_cast<std::uint8_t>(state.count>>(byte*8)));
        database_(packet);
    }
    std::weak_ptr<Root> root_;std::shared_ptr<const BlessingDefinitions> definitions_;
    std::array<WorldKillCountCountry,2> countries_;Clock tick_;Database database_;Disabled disabled_;GameLog game_log_;
};
}
