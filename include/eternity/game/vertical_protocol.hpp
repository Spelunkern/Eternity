#pragma once
#include "eternity/game/entry_delivery.hpp"
#include "eternity/game/world_admission.hpp"
#include "eternity/game/zone_lifecycle.hpp"

namespace eternity::game {
// The connection owner retains the pipeline and all referenced catalogs until
// this synchronous operation completes. External steps are mandatory services.
struct VerticalProtocolServices {
    EntryDeliveryPorts delivery;
    std::function<void(CharacterEntryStep, Actor&)> lifecycle;
    std::function<void(net::Packet)> send_manager;
    std::function<std::uint32_t()> world_day,system_time;
    std::function<bool()> lock_guild;
    std::function<void()> unlock_guild;
};
// Successful ps_game.exe PacketUserDBChar 0046cf60, 0407 branch
// 0046d4c1..0046d717. The caller owns state, pipeline and their lifetimes.
// Discard the pipeline after failure; release it only after successful return.
void deliver_vertical_entry(CharacterEntryState&, ActorLoadPipeline&, const PlayerSnapshot&,
    const SocialCatalog&, const EntryCatalogs&, ExperienceBounds, std::string guild_name,
    const VerticalProtocolServices&);
}

namespace eternity::game {
enum class VerticalEntryRequest {unhandled, entered, rejected};
// One per-connection view of the existing admission user; never owns membership.
// All methods run on the same serialized world executor as WorldAdmission.
class VerticalEntrySession {
public:
    VerticalEntrySession(WorldAdmission& world,WorldAdmissionUser& user):world_(world),user_(user) {}
    bool wait() {return world_.wait(user_);}
    VerticalEntryRequest receive(std::span<const std::uint8_t> packet);
private:
    WorldAdmission& world_;WorldAdmissionUser& user_;
};
// Called once per world update, not once per connection.
void update_vertical_admission(WorldAdmission& world);
}

namespace eternity::game {
enum class VerticalNProtectResult {absent,accepted,rejected};
struct VerticalNProtectPorts {
    // Provider owns the retained CCSAuth2 object, stores these answer DWORDs and
    // returns the real CheckAuthAnswer result. Never use a synthetic success.
    std::function<std::uint32_t(const std::array<std::uint32_t,4>&)> check_answer;
    // Reads query and answer fields from the live retained CCSAuth2 object.
    std::function<void(std::uint32_t)> diagnostic;
    std::function<void(std::uint32_t)> close;
};
// CUser::NProtectCSARecv 0044b7e0; use the SAME NProtectState passed to
// send_nprotect_query. Caller supplies the current WorldAdmissionUser location.
VerticalNProtectResult receive_vertical_nprotect(std::span<const std::uint8_t>,NProtectState&,std::uint32_t location,const VerticalNProtectPorts&);
}
