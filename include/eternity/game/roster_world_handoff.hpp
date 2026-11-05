#pragma once
#include "eternity/net/world_server.hpp"
#include "eternity/game/serialized_world_application.hpp"

namespace eternity::game {
// Per-connection roster application with a single transfer to a supplied
// world owner. All calls execute on the WorldServer application strand.
// The selected application owns the actual selection/load/admission process;
// no acknowledgement is synthesized here. Its close method is called even if
// open or the first receive fails after the factory returns it.
class RosterWorldHandoff final:public net::WorldApplication {
public:
    using Select=std::function<std::unique_ptr<net::WorldApplication>(
        const session::LoginSession&,net::WorldChannel,CharacterRoster)>;
    RosterWorldHandoff(session::LoginSession account,net::WorldChannel channel,net::LoadCharacters load,
        net::DispatchCharacter commands,Select select)
        :account_(std::move(account)),channel_(std::move(channel)),load_(std::move(load)),commands_(std::move(commands)),select_(std::move(select)) {
        if(!load_ || !commands_ || !select_ || !channel_.send || !channel_.disconnect)
            throw std::invalid_argument("Incomplete roster-to-world services");
    }
    void open() override {
        if(opened_)throw std::logic_error("Roster already opened");opened_=true;
        roster_=load_(account_);for(auto& packet:character_list(roster_))channel_.send(std::move(packet));
    }
    void receive(const net::Packet& packet) override {
        if(selected_)return selected_->receive(packet);
        if(!opened_)throw std::logic_error("Roster is not open");
        if(packet.size()>=2 && packet[0]==4 && packet[1]==1) {
            selected_=select_(account_,channel_,std::move(roster_));
            if(!selected_)throw std::runtime_error("World selection owner unavailable");
            selected_->open();selected_->receive(packet);return;
        }
        auto reply=commands_(account_,roster_,packet);
        if(reply.close)return channel_.disconnect();
        for(auto& response:reply.packets)channel_.send(std::move(response));
    }
    void receive_nprotect(const net::Packet& packet) override {
        if(!selected_)throw std::logic_error("NProtect answer without world owner");
        selected_->receive_nprotect(packet);
    }
    void close() override {close_with_reason({});}
    void close_with_reason(std::optional<std::uint32_t> reason) override {
        if(selected_) {auto owner=std::move(selected_);owner->close_with_reason(reason);}
        roster_={};opened_=false;
    }
private:
    session::LoginSession account_;net::WorldChannel channel_;net::LoadCharacters load_;net::DispatchCharacter commands_;Select select_;
    CharacterRoster roster_;std::unique_ptr<net::WorldApplication> selected_;bool opened_=false;
};
inline net::WorldApplicationFactory roster_world_handoff_factory(net::LoadCharacters load,net::DispatchCharacter commands,RosterWorldHandoff::Select select) {
    if(!load || !commands || !select)throw std::invalid_argument("Incomplete roster-to-world factory");
    return [load=std::move(load),commands=std::move(commands),select=std::move(select)](const session::LoginSession& account,net::WorldChannel channel) {
        return std::make_unique<RosterWorldHandoff>(account,std::move(channel),load,commands,select);
    };
}
// Roster loading and character commands run on WorldServer's per-connection
// worker, not on the shared world strand. Only the selected world application
// is constructed and invoked on that strand. Its DB work must still be posted
// to a separate worker rather than performed in its world-strand callbacks.
inline net::WorldApplicationFactory roster_world_serialized_factory(SerializedWorldApplication::Strand strand,
    net::LoadCharacters load,net::DispatchCharacter commands,RosterWorldHandoff::Select select) {
    if(!select)throw std::invalid_argument("Missing selected world application factory");
    return roster_world_handoff_factory(std::move(load),std::move(commands),
        [strand=std::move(strand),select=std::move(select)](const session::LoginSession& account,net::WorldChannel channel,CharacterRoster roster) {
            return std::make_unique<SerializedWorldApplication>(strand,
                [&]{return select(account,std::move(channel),std::move(roster));});
        });
}
}
