#include "eternity/game/character_commands.hpp"
#include <algorithm>
#include <random>

namespace eternity::game {
CharacterCommands::CharacterCommands(storage::SqlServerConfig config,CharacterCommandOptions options,
    std::shared_ptr<const CreationRules> rules,std::shared_ptr<ItemIds> identifiers,
    std::function<CreationTime()> clock,std::function<std::size_t(std::size_t)> choose_start)
    : config_(std::move(config)),options_(std::move(options)),rules_(std::move(rules)),identifiers_(std::move(identifiers)),
      clock_(std::move(clock)),choose_start_(std::move(choose_start)) {
    if(!rules_ || !identifiers_ || !clock_) throw std::invalid_argument("Character commands require original creation services");
    if(!choose_start_) choose_start_=[](std::size_t count) {return static_cast<std::size_t>(std::random_device{}())%count;};
}
CharacterReply CharacterCommands::execute(std::uint32_t uid,const std::string& username,CharacterRoster& roster,const net::Packet& packet) const {
    if(packet.size()<2) return {{},true};
    const auto opcode=static_cast<unsigned int>(packet[0]) | (static_cast<unsigned int>(packet[1])<<8);
    if(opcode==0x107) return {{},true};
    if(opcode==0x102) {
        auto checked=check_create(packet,roster);
        if(checked.close) return {{},true};
        if(checked.result) return {{{2,1,*checked.result}},false};
        if(!checked.request) return {{},true};
        auto draft=rules_->prepare(*checked.request,choose_start_(rules_->start_count(checked.request->family)));
        const auto now=clock_();
        try {for(auto& item:draft.items) {
            item.uid=identifiers_->next(clock_());item.made_at=clock_().local_calendar;
        }}
        catch(const std::overflow_error&) {return {{{2,1,1}},false};}
        storage::SqlServer database(config_);Characters repository(database,options_.database_id,options_.world_base,options_.catalog);
        const auto created=repository.create(uid,username,draft,now.local_calendar);
        CharacterReply reply{{{2,1,created.result}},false};
        if(created.result==0) {
            auto refreshed=repository.load(uid);
            // Original CreateCharacter reloads the character list without reloading country/max-grow.
            refreshed.country=roster.country;refreshed.max_grow=roster.max_grow;refreshed.new_account=roster.new_account;
            roster=std::move(refreshed);auto packets=character_list(roster,false);
            for(auto& listed:packets) reply.packets.push_back(std::move(listed));
        }
        return reply;
    }
    if(opcode==0x109) {
        if(packet.size()<3 || packet[2]>1) return {{},true};
        const auto country=packet[2];roster.country=2;
        storage::SqlServer database(config_);Characters repository(database,options_.database_id,options_.world_base,options_.catalog);
        // CUser::OnDispatchDB case 2 processes pending deletions before deciding if country can change.
        for(auto& slot:roster.slots) if(slot && slot->remaining_time) {
            if(!repository.erase(uid,slot->id)) return {{},true};
            slot.reset();
        }
        if(std::any_of(roster.slots.begin(),roster.slots.end(),[](const auto& slot){return slot.has_value();})) return {};
        // Refresh the count to retain the original database-side empty-account check.
        const auto current=repository.load(uid);
        if(std::any_of(current.slots.begin(),current.slots.end(),[](const auto& slot){return slot.has_value();})) return {};
        if(!repository.save_country(uid,country,roster.new_account)) return {{},true};
        roster.country=country;roster.new_account=false;
        return {}; // Internal 402 acknowledgement updates game state; no client packet is emitted.
    }
    return {{},true}; // Remaining character commands are not reconstructed yet.
}
}
