#pragma once
#include "eternity/game/mob_combat_scheduler.hpp"
#include "eternity/game/zone_users.hpp"
#include "eternity/game/vertical_combat_spend.hpp"
#include "eternity/game/party_runtime_membership.hpp"
#include "eternity/game/game_log.hpp"
#include "eternity/game/party_runtime_requests.hpp"
#include "eternity/game/mob_targeting_lifecycle.hpp"
#include "eternity/game/corpses.hpp"
#include "eternity/game/world_admission.hpp"
#include "eternity/game/inventory.hpp"
#include "eternity/game/vertical_combat_view.hpp"
#include "eternity/game/character_replica_delivery.hpp"
#include "eternity/game/maps.hpp"
#include "eternity/game/map_config.hpp"
#include "eternity/game/zone_entry.hpp"
#include "eternity/game/user_view_actor_publication.hpp"
#include "eternity/game/party_runtime_membership_send.hpp"
#include "eternity/game/party_runtime_registry.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/vertical_combat_spend_party.hpp"
#include "eternity/game/random.hpp"
#include <utility>
namespace eternity::game {
class VerticalCombatSchedulerZoneServices {
public:
 virtual ~VerticalCombatSchedulerZoneServices()=default;
 // Lease must pin the actor and all its equipment, pool and world owners for
 // the complete poll; world() is a pure context read within that lease.
 virtual std::shared_ptr<void> retain(Actor&)=0;
 virtual MobCombatSchedulerWorld& world(Actor&)=0;
};
// The lease pins this exact loaded actor, scene, timer and all continuation
// owners. Lookups must be pure; exclude membership mutation for the current
// actor until all three continuations have returned.
class VerticalCombatRecoveryZoneServices {
public:
 virtual ~VerticalCombatRecoveryZoneServices()=default;
 virtual std::shared_ptr<void> retain(Actor&)=0;
 virtual LoadedActor& loaded(Actor&)=0;
 virtual std::uint32_t& next_recovery(Actor&)=0;
 virtual void expire_effects(Actor&,std::uint32_t)=0; // CUser::ChkEndTimeSkill 004865b0
 virtual void update_duel(Actor&,std::uint32_t)=0; // original 0047c210, individual and guild duel
 virtual void update_rebirth(Actor&,std::uint32_t)=0; // CUser::UpdateRebirth 0048d240
};
// Bind expiry to the retained Actor's EffectCycle, status, inventory, observer,
// removal world and DB-agent transport. Return the exact ItemDefinitions owner
// used by that Actor through a pure lookup. Retain definitions and prevent equipment-vector mutation
// through synchronous expiry callbacks, as the existing Actor effect methods do.
class VerticalCombatRecoveryEffectServices:public VerticalCombatRecoveryZoneServices {
public:
 void expire_effects(Actor&,std::uint32_t) final;
 virtual const ItemDefinitions& effect_item_definitions(Actor&)=0;
};
// CUser::UpdateRebirth 0048d240 requires the real vehicle prefix and the
// exact log/soul owners. The log callback sends the original 0305 log and
// may reenter; party presence is a pure read of the actor's live +173c owner.
class VerticalCombatRecoveryRebirthServices:public VerticalCombatRecoveryEffectServices {
public:
 void update_rebirth(Actor&,std::uint32_t) final;
 virtual void update_vehicle_prefix(Actor&,std::uint32_t)=0;
 virtual void log_rebirth_timeout(Actor&)=0;
 virtual bool rebirth_party_present(Actor&)=0;
 virtual void expire_party_soul(Actor&)=0;
 virtual void expire_solo_soul(Actor&)=0;
};
// Bind the +173c presence test to the actor's actual party/raid membership.
// The actor, membership and any referenced party survive all callbacks. One
// serialized owner publishes membership changes before the next live read.
class VerticalCombatRecoveryPartyServices:public VerticalCombatRecoveryRebirthServices {
public:
 VerticalCombatRecoveryPartyServices(Actor&,PartyMembershipUser&);
 bool rebirth_party_present(Actor&) final;
private:
 Actor& actor_;PartyMembershipUser& member_;
};
// Use the existing SetGameLogMain owner and required real log transport. The
// original leaves bytes of the 101-byte stack packet untouched; scratch is an
// explicit seed. Subject, calendar and scratch are pure, non-reentrant reads.
class VerticalCombatRecoveryLogServices:public VerticalCombatRecoveryPartyServices {
public:
 VerticalCombatRecoveryLogServices(Actor&,PartyMembershipUser&);
 void log_rebirth_timeout(Actor&) final;
 virtual std::array<std::uint8_t,101> rebirth_log_scratch(const Actor&)=0;
 virtual GameLogSubject rebirth_log_subject(const Actor&)=0;
 virtual std::chrono::sys_seconds rebirth_log_calendar()=0;
 virtual void send_rebirth_log(const net::Packet&)=0;
private:
 Actor& log_actor_;
};
// Exact retained stItemInfo record for the same GroundItem::definition; no
// ID-only substitute. A nonempty record must expose bytes through +43.
class VerticalCombatVehicleItemRecords {
public:
 virtual ~VerticalCombatVehicleItemRecords()=default;
 virtual std::span<const std::byte> vehicle_item_record(const GroundItem&)=0;
};
void vertical_combat_vehicle_prefix(Actor&,std::uint32_t,VerticalCombatVehicleItemRecords&);
// Actor, slot-13 item/pool, catalog record and scene must survive the prefix;
// record lookup is pure and reloads/equipment mutation are excluded until send.
class VerticalCombatRecoveryVehicleServices:public VerticalCombatRecoveryLogServices,public VerticalCombatVehicleItemRecords {
public:
 VerticalCombatRecoveryVehicleServices(Actor&,PartyMembershipUser&);
 void update_vehicle_prefix(Actor&,std::uint32_t) final;
private:
 Actor& vehicle_actor_;
};
// Pure leases bind the exact registry party, selected membership record,
// Actor, request state, scene and zone. Retain them through unlock, ExpDrop and
// Rebirth; party lookup and leader acquisition must not mutate membership.
struct VerticalCombatSoulLeader {
 std::shared_ptr<Actor> actor;PartyMembershipUser* member=nullptr;
 PartyRequestState* state=nullptr;std::shared_ptr<void> lifetime;
};
class VerticalCombatPartySoulWorld {
public:
 virtual ~VerticalCombatPartySoulWorld()=default;
 virtual std::shared_ptr<PartyRegistryParty> retain_party(PartyNormalMembership&)=0;
 virtual VerticalCombatSoulLeader retain_leader(PartyMembershipUser&)=0;
 virtual PartyMembershipWorld& membership_world()=0;
 virtual UserPosition leader_position(const Actor&)=0; // pure current +d0
 virtual void drop_experience(Actor&,std::int32_t penalty)=0; // CUser::ExpDrop 0045ad50
 virtual MobTargetingLifecycleWorld& rebirth_world(Actor&)=0;
};
// CUser::KillSoulParty 004596a0: GetPartyBoth under the captured party lock,
// EqualZone on live selected leader, ExpDrop, then the actual Rebirth owner.
// Solo KillSoul remains a required provider inherited from the base.
class VerticalCombatRecoveryPartySoulServices:public VerticalCombatRecoveryVehicleServices {
public:
 VerticalCombatRecoveryPartySoulServices(Actor&,PartyMembershipUser&,PartyRequestState&,VerticalCombatPartySoulWorld&);
 void expire_party_soul(Actor&) final;
private:
 Actor& soul_actor_;PartyMembershipUser& soul_member_;PartyRequestState& soul_state_;VerticalCombatPartySoulWorld& soul_world_;
};
// The real CMap::GetBindPos owners must write into the supplied seeded
// scratch exactly, including failure paths. All reads are from the retained
// actor/zone/map; no fabricated destination or successful bind is allowed.
class VerticalCombatSoloSoulWorld:public VerticalCombatPartySoulWorld {
public:
 virtual std::pair<UserPosition,std::int32_t> bind_scratch(const Actor&)=0;
 virtual void bind_from_zone(Actor&,UserPosition&,std::int32_t&)=0; // 004295a0
 virtual void bind_from_map(Actor&,std::uint16_t,UserPosition&)=0; // 00429360
 virtual UserPosition actor_position(const Actor&)=0; // pure current +d0
 virtual void ultimate_soul(Actor&)=0; // complete 00459a2b..00459c19 branch
};
// CUser::KillSoul 004599e0: actor-owned rebirth benefit +58d8 preserves the
// real half-resource and current-zone bind path. The no-benefit item/logout
// path is a required distinct provider, not a synthetic success.
class VerticalCombatUltimateSoulWorld:public VerticalCombatSoloSoulWorld {
public:
 void ultimate_soul(Actor&) final;
 virtual void ultimate_item_soul(Actor&)=0; // complete 00459aa3..00459c19 branch
};
// CUser::FindItemRebirth 0045f740. The caller supplies the live signed bag
// limit and exact stItemInfo records; pin inventory, pool and records during
// both scans. An occupied value-only inventory slot is an ownership error.
class VerticalCombatRebirthItemRecords {
public:
 virtual ~VerticalCombatRebirthItemRecords()=default;
 virtual std::span<const std::byte> rebirth_item_record(const GroundItem&)=0;
};
struct VerticalCombatRebirthItem {std::uint8_t bag,slot;GroundItem* item;};
std::optional<VerticalCombatRebirthItem> vertical_combat_find_rebirth_item(Actor&,std::int32_t,bool,VerticalCombatRebirthItemRecords&);
// KillSoul passes zero to FindItemRebirth. The selected item and both
// continuations remain owned by the actual inventory and session services.
class VerticalCombatUltimateItemSoulWorld:public VerticalCombatUltimateSoulWorld,public VerticalCombatRebirthItemRecords {
public:
 void ultimate_item_soul(Actor&) final;
 virtual std::int32_t rebirth_bag_limit(const Actor&)=0;
 virtual void ultimate_rebirth_item(Actor&,const VerticalCombatRebirthItem&)=0;
 virtual void ultimate_no_rebirth_item(Actor&)=0;
};
// ItemUseNSend may reenter and alter resources, zone and map. Its callback must
// execute the real 00465120 owner; retain all bindings through the continuation.
class VerticalCombatUltimateItemUse {
public:
 virtual ~VerticalCombatUltimateItemUse()=default;
 virtual void use_rebirth_item(Actor&,std::uint8_t bag,std::uint8_t slot)=0;
};
void vertical_combat_ultimate_item_rebirth(Actor&,const VerticalCombatRebirthItem&,PartyMembershipUser&,PartyRequestState&,VerticalCombatSoloSoulWorld&,VerticalCombatUltimateItemUse&);
class VerticalCombatUltimateItemRebirthWorld:public VerticalCombatUltimateItemSoulWorld,public VerticalCombatUltimateItemUse {
public:
 VerticalCombatUltimateItemRebirthWorld(Actor&,PartyMembershipUser&,PartyRequestState&);
 void ultimate_rebirth_item(Actor&,const VerticalCombatRebirthItem&) final;
private:
 Actor& item_actor_;PartyMembershipUser& item_member_;PartyRequestState& item_state_;
};
// Exact DB transport and full CWorld::LogoutUser, not a socket-close shortcut.
// The retained actor and all owners survive ExpDrop, DB send and logout.
class VerticalCombatUltimateNoItemServices {
public:
 virtual ~VerticalCombatUltimateNoItemServices()=default;
 virtual void send_ultimate_death_database(const net::Packet&)=0;
 virtual void logout_ultimate_user(Actor&)=0;
};
void vertical_combat_ultimate_no_item(Actor&,PlayerCore&,PartyMembershipUser&,WorldAdmissionUser&,WorldCorpses&,VerticalCombatSoloSoulWorld&,VerticalCombatUltimateNoItemServices&);
class VerticalCombatUltimateNoItemWorld:public VerticalCombatUltimateItemRebirthWorld,public VerticalCombatUltimateNoItemServices {
public:
 VerticalCombatUltimateNoItemWorld(Actor&,PlayerCore&,PartyMembershipUser&,PartyRequestState&,WorldAdmissionUser&,WorldCorpses&);
 void ultimate_no_rebirth_item(Actor&) final;
private:
 Actor& no_item_actor_;PlayerCore& no_item_player_;PartyMembershipUser& no_item_member_;WorldAdmissionUser& no_item_admission_;WorldCorpses& no_item_corpses_;
};
// ItemUseNSend 00465120, with notify=false as used by KillSoul. The singleton
// detach must remove the exact inventory slot without freeing the GroundItem;
// all callbacks preserve the selected item's allocation and payload.
class VerticalCombatUltimateItemUseServices {
public:
 virtual ~VerticalCombatUltimateItemUseServices()=default;
 virtual void log_ultimate_item_use(Actor&,const GroundItem&,std::uint8_t,std::uint8_t)=0;
 virtual void detach_ultimate_item(Actor&,GroundItem&,std::uint8_t,std::uint8_t)=0;
 virtual void send_ultimate_item_database(Actor&,const net::Packet&)=0;
 virtual bool ultimate_item_zone_present(const Actor&)=0;
 virtual void send_ultimate_item_view(Actor&,const net::Packet&)=0;
};
void vertical_combat_ultimate_use_item(Actor&,std::uint8_t,std::uint8_t,VerticalCombatUltimateItemUseServices&);
class VerticalCombatUltimateItemUseWorld:public VerticalCombatUltimateNoItemWorld,public VerticalCombatUltimateItemUseServices {
public:
 using VerticalCombatUltimateNoItemWorld::VerticalCombatUltimateNoItemWorld;
 void use_rebirth_item(Actor&,std::uint8_t,std::uint8_t) final;
};
// ItemUseNSend 00465120 conditional 040e log. Unwritten original stack bytes
// come from scratch; item name is the actual retained stItemInfo name.
net::Packet vertical_combat_ultimate_item_log(const GroundItem&,std::string_view,std::uint8_t,std::uint8_t,const GameLogSubject&,std::chrono::sys_seconds,const std::array<std::uint8_t,152>&);
class VerticalCombatUltimateItemLogWorld:public VerticalCombatUltimateItemUseWorld {
public:
 using VerticalCombatUltimateItemUseWorld::VerticalCombatUltimateItemUseWorld;
 void log_ultimate_item_use(Actor&,const GroundItem&,std::uint8_t,std::uint8_t) final;
 virtual std::string ultimate_item_name(const GroundItem&)=0;
 virtual GameLogSubject ultimate_item_log_subject(const Actor&)=0;
 virtual std::chrono::sys_seconds ultimate_item_log_calendar()=0;
 virtual std::array<std::uint8_t,152> ultimate_item_log_scratch(const Actor&)=0;
 virtual void send_ultimate_item_log(Actor&,const net::Packet&)=0;
};
// All three inputs are the same retained Actor's live owners. InventoryWorld
// supplies actual DB/log transports and stItemInfo names; the view supplies the
// real zone fanout. Their reads must be pure and callbacks serialize ownership.
class VerticalCombatUltimateInventoryBindings {
public:
 VerticalCombatUltimateInventoryBindings(Actor&,PartyRequestState&,InventoryWorld&,VerticalCombatView&);
 std::string item_name(const GroundItem&);
 GameLogSubject subject(const Actor&);
 std::chrono::sys_seconds calendar();
 void database(Actor&,const net::Packet&);
 void log(Actor&,const net::Packet&);
 bool zone_present(const Actor&) const;
 void view(Actor&,const net::Packet&);
private:
 void require_actor(const Actor&) const;
 Actor& actor_;PartyRequestState& state_;InventoryWorld& inventory_;VerticalCombatView& view_;
};
class VerticalCombatUltimateInventoryWorld:public VerticalCombatUltimateItemLogWorld {
public:
 VerticalCombatUltimateInventoryWorld(Actor&,PlayerCore&,PartyMembershipUser&,PartyRequestState&,WorldAdmissionUser&,WorldCorpses&,InventoryWorld&,VerticalCombatView&);
 std::string ultimate_item_name(const GroundItem&) final;
 GameLogSubject ultimate_item_log_subject(const Actor&) final;
 std::chrono::sys_seconds ultimate_item_log_calendar() final;
 void send_ultimate_item_log(Actor&,const net::Packet&) final;
 void send_ultimate_item_database(Actor&,const net::Packet&) final;
 bool ultimate_item_zone_present(const Actor&) final;
 void send_ultimate_item_view(Actor&,const net::Packet&) final;
private:
 VerticalCombatUltimateInventoryBindings bindings_;
};
// Rebirth and vehicle lookups borrow the same retained stItemInfo record.
// The provider must expose the full original record, not a truncated vehicle
// projection, and must retain it through both inventory scans.
class VerticalCombatUltimateActorInventoryBindings:public VerticalCombatRebirthItemRecords {
public:
 VerticalCombatUltimateActorInventoryBindings(Actor&,VerticalCombatVehicleItemRecords&);
 std::int32_t bag_limit(const Actor&) const;
 std::span<const std::byte> rebirth_item_record(const GroundItem&) override;
private:
 Actor& actor_;VerticalCombatVehicleItemRecords& records_;
};
class VerticalCombatUltimateActorInventoryWorld:public VerticalCombatUltimateInventoryWorld {
public:
 VerticalCombatUltimateActorInventoryWorld(Actor&,PlayerCore&,PartyMembershipUser&,PartyRequestState&,WorldAdmissionUser&,WorldCorpses&,InventoryWorld&,VerticalCombatView&,VerticalCombatVehicleItemRecords&);
 std::int32_t rebirth_bag_limit(const Actor&) final;
 std::span<const std::byte> rebirth_item_record(const GroundItem&) final;
private:
 VerticalCombatUltimateActorInventoryBindings actor_inventory_;
};
// CUser::ExpDrop 0045ad50: PlayerCore owns live experience; protection and
// level are Actor fields. The reduction is the live faction blessing value.
class VerticalCombatExperienceDropServices {
public:
 virtual ~VerticalCombatExperienceDropServices()=default;
 virtual std::int32_t experience_drop_reduction(const Actor&)=0;
 virtual void send_experience_drop_database(Actor&,const net::Packet&)=0;
};
void vertical_combat_experience_drop(Actor&,PlayerCore&,const Experience&,std::int32_t,VerticalCombatExperienceDropServices&);
class VerticalCombatExperienceDropWorld:public VerticalCombatUltimateActorInventoryWorld,public VerticalCombatExperienceDropServices {
public:
 VerticalCombatExperienceDropWorld(Actor&,PlayerCore&,PartyMembershipUser&,PartyRequestState&,WorldAdmissionUser&,WorldCorpses&,InventoryWorld&,VerticalCombatView&,VerticalCombatVehicleItemRecords&,const Experience&);
 void drop_experience(Actor&,std::int32_t) final;
private:
 Actor& exp_actor_;PlayerCore& exp_player_;const Experience& exp_curve_;
};
// Synchronous cohosted DB-agent path. Delivery uses the replica's actual save
// mutex; the session must retain the Actor, PlayerCore and delivery throughout.
class VerticalCombatExperienceReplicaDelivery {
public:
 VerticalCombatExperienceReplicaDelivery(Actor&,PlayerCore&,CharacterReplicaDelivery&);
 void send(Actor&,const net::Packet&);
private:
 Actor& actor_;PlayerCore& player_;CharacterReplicaDelivery& delivery_;
};
class VerticalCombatExperienceReplicaWorld:public VerticalCombatExperienceDropWorld {
public:
 VerticalCombatExperienceReplicaWorld(Actor&,PlayerCore&,PartyMembershipUser&,PartyRequestState&,WorldAdmissionUser&,WorldCorpses&,InventoryWorld&,VerticalCombatView&,VerticalCombatVehicleItemRecords&,const Experience&,CharacterReplicaDelivery&);
 void send_experience_drop_database(Actor&,const net::Packet&) final;
private:
 VerticalCombatExperienceReplicaDelivery experience_delivery_;
};
// CMap::GetBindPos 00429360. The map and shared MSVCRT random stream are the
// live world owners; invalid dimensions raise where the original would fault.
bool vertical_combat_map_bind(const MapDefinition&,std::uint8_t,const UserPosition&,UserPosition&,RandomSource&);
// CMap::GetBindPos 004295a0: the current configured map and catalog must be
// the retained world owners; no destination-map substitute is permitted.
bool vertical_combat_zone_bind(const ConfiguredMap&,std::span<const ConfiguredMap>,std::uint8_t,const UserPosition&,UserPosition&,std::int32_t&,RandomSource&);
class VerticalCombatMapBindWorld:public VerticalCombatExperienceReplicaWorld {
public:
 VerticalCombatMapBindWorld(Actor&,PlayerCore&,PartyMembershipUser&,PartyRequestState&,WorldAdmissionUser&,WorldCorpses&,InventoryWorld&,VerticalCombatView&,VerticalCombatVehicleItemRecords&,const Experience&,CharacterReplicaDelivery&,RandomSource&,const ServerMaps&);
 void bind_from_map(Actor&,std::uint16_t,UserPosition&) final;
 void bind_from_zone(Actor&,UserPosition&,std::int32_t&) final;
 virtual const ConfiguredMap& rebirth_zone_map(Actor&)=0; // exact current zone +24 map
private:
 Actor& map_actor_;RandomSource& map_random_;const ServerMaps& maps_;
};
// Bind +d0 and current zone +24 to the same retained movement/admission
// owners. Position stays available after zone removal; the root must publish
// every movement into this exact ZoneEntryState before entering combat. When
// the Actor remains in zone membership, its linked position must also match.
class VerticalCombatLiveMapBindings {
public:
 VerticalCombatLiveMapBindings(Actor&,ZoneEntryState&,WorldAdmissionUser&);
 UserPosition position(const Actor&) const;
 const ConfiguredMap& zone_map(const Actor&) const;
private:
 void require_actor(const Actor&) const;
 Actor& actor_;ZoneEntryState& entry_;WorldAdmissionUser& admission_;
};
class VerticalCombatLiveMapBindWorld:public VerticalCombatMapBindWorld {
public:
 VerticalCombatLiveMapBindWorld(Actor&,PlayerCore&,PartyMembershipUser&,PartyRequestState&,WorldAdmissionUser&,WorldCorpses&,InventoryWorld&,VerticalCombatView&,VerticalCombatVehicleItemRecords&,const Experience&,CharacterReplicaDelivery&,RandomSource&,const ServerMaps&,ZoneEntryState&);
 UserPosition actor_position(const Actor&) final;
 const ConfiguredMap& rebirth_zone_map(Actor&) final;
private:
 VerticalCombatLiveMapBindings live_map_;
};
// CZone::MoveUser 00416260 precedes the separate MoveUserLocal 00416370.
// Lookup is the real same-zone CZone::FindUser owner; the lease pins its Actor.
class VerticalCombatRebirthMoveServices {
public:
 virtual ~VerticalCombatRebirthMoveServices()=default;
 virtual std::shared_ptr<void> retain_move_peer(Actor&)=0;
 virtual bool move_rebirth_local(Actor&,std::uint16_t,const UserPosition&)=0;
};
// The caller serializes and retains the actor, membership, zone and all
// publication owners through the move and its synchronous continuations.
class VerticalCombatRebirthLocalServices {
public:
 virtual ~VerticalCombatRebirthLocalServices()=default;
 virtual void close_rebirth_move(Actor&,std::uint32_t)=0;
 virtual void move_by_game(Actor&)=0;
 virtual bool rebirth_party_present(const Actor&)=0;
 virtual void send_party_move(Actor&)=0;
};
// Binds MoveByGame to the Actor's actual movement-check state and the same
// retained position as the zone move. The clock is the live server tick.
class VerticalCombatRebirthMoveGameServices:public VerticalCombatRebirthLocalServices {
public:
 VerticalCombatRebirthMoveGameServices(Actor&,ZoneEntryState&);
 void move_by_game(Actor&) final;
 virtual std::uint32_t rebirth_move_tick()=0;
private:
 Actor& actor_;ZoneEntryState& admission_;
};
// CParty::SendMove 00445880 / SendSameMap 004457f0. The caller retains the
// exact registry party, user, actor, entry and send world through callbacks.
void vertical_combat_rebirth_party_move(Actor&,PartyMembershipUser&,PartyRegistryParty&,const ZoneEntryState&,PartyMembershipSendWorld&,std::array<std::uint8_t,2> opcode_scratch);
class VerticalCombatRebirthPartyMoveServices:public VerticalCombatRebirthMoveGameServices {
public:
 VerticalCombatRebirthPartyMoveServices(Actor&,ZoneEntryState&,PartyMembershipUser&,PartyMembershipSendWorld&);
 bool rebirth_party_present(const Actor&) final;
 void send_party_move(Actor&) final;
 virtual std::shared_ptr<PartyRegistryParty> retain_rebirth_move_party(PartyNormalMembership&)=0;
 virtual std::array<std::uint8_t,2> party_move_opcode_scratch()=0;
private:
 Actor& actor_;ZoneEntryState& entry_;PartyMembershipUser& member_;PartyMembershipSendWorld& party_world_;
};
// Reuses the skill-spend party lease; registry lookup proves the concrete
// 30-slot party identity before constructing the aliasing retained pointer.
class VerticalCombatRebirthRegistryPartyServices:public VerticalCombatRebirthPartyMoveServices {
public:
 VerticalCombatRebirthRegistryPartyServices(Actor&,ZoneEntryState&,PartyMembershipUser&,PartyMembershipSendWorld&,PartyRegistry&,PartyRegistryPorts&,VerticalCombatPartyLease&);
 std::shared_ptr<PartyRegistryParty> retain_rebirth_move_party(PartyNormalMembership&) final;
private:
 PartyRegistry& registry_;PartyRegistryPorts& ports_;VerticalCombatPartyLease& lease_;
};
bool vertical_combat_rebirth_move_local(Actor&,RuntimeZone&,ZoneEntryState&,std::uint32_t,UserPosition,UserViewActorTransport&,VerticalCombatRebirthLocalServices&);
// Joins CZone::MoveUser's retained vehicle prefix to the actual local move.
// World admission, scene state, actor and movement owners remain serialized.
class VerticalCombatRebirthMoveLocalServices:public VerticalCombatRebirthMoveServices {
public:
 VerticalCombatRebirthMoveLocalServices(Actor&,WorldAdmissionUser&,ZoneEntryState&,PartyRequestState&,UserViewActorTransport&,VerticalCombatRebirthLocalServices&);
 bool move_rebirth_local(Actor&,std::uint16_t,const UserPosition&) final;
private:
 Actor& actor_;WorldAdmissionUser& admission_;ZoneEntryState& entry_;PartyRequestState& state_;UserViewActorTransport& view_;VerticalCombatRebirthLocalServices& local_;
};
// The actual world session must retain the actor and its scene/zone owners as
// required by VerticalSessionRegistry. Registry access is world-serialized.
template<class Session>
class VerticalCombatRebirthMoveSessionServices:public VerticalCombatRebirthMoveLocalServices {
public:
 VerticalCombatRebirthMoveSessionServices(Actor& actor,WorldAdmissionUser& admission,ZoneEntryState& entry,PartyRequestState& state,UserViewActorTransport& view,VerticalCombatRebirthLocalServices& local,VerticalSessionRegistry<Session>& sessions)
  :VerticalCombatRebirthMoveLocalServices(actor,admission,entry,state,view,local),sessions_(sessions){}
 std::shared_ptr<void> retain_move_peer(Actor& peer) final {return sessions_.retain_actor(peer);}
private:
 VerticalSessionRegistry<Session>& sessions_;
};
bool vertical_combat_rebirth_move(Actor&,std::uint16_t,const UserPosition&,ActorActionWorld&,VerticalCombatRebirthMoveServices&);
// All non-movement lifecycle operations remain the actual retained delegate.
class VerticalCombatRebirthMoveWorld:public MobTargetingLifecycleWorld {
public:
 VerticalCombatRebirthMoveWorld(Actor&,MobTargetingLifecycleWorld&,ActorActionWorld&,VerticalCombatRebirthMoveServices&);
 GameLogSubject subject(const Actor&) const override;
 std::uint16_t instance(const Actor&) const override;
 std::uint8_t presentation_motion(const Actor&) const override;
 void presentation_motion(Actor&,std::uint8_t) override;
 void replace_resources(Actor&,PlayerResources) override;
 std::optional<std::uint32_t> find_dead(Actor&,std::uint32_t) override;
 void leave_dead(Actor&,std::uint32_t) override;
 bool move_user(Actor&,std::uint16_t,const UserPosition&) override;
 bool map_present(std::int32_t) override;
 void zone_leave_user_move(Actor&,std::int32_t,const UserPosition&) override;
 void send_cell_view(Actor&,const net::Packet&) override;
 void send_recover_change(Actor&,PlayerResources) override;
 std::chrono::sys_seconds local_calendar() override;
 void send_game_log(const net::Packet&) override;
private:
 Actor& actor_;MobTargetingLifecycleWorld& world_;ActorActionWorld& actions_;VerticalCombatRebirthMoveServices& services_;
};
// CUser::KillSoul 004599e0 ordinary path uses actual ExpDrop and Rebirth;
// the ultimate-mode item/logout branch remains an explicit required owner.
class VerticalCombatRecoverySoloSoulServices:public VerticalCombatRecoveryPartySoulServices {
public:
 VerticalCombatRecoverySoloSoulServices(Actor&,PartyMembershipUser&,PartyRequestState&,VerticalCombatSoloSoulWorld&);
 void expire_solo_soul(Actor&) final;
private:
 Actor& solo_actor_;PartyMembershipUser& solo_member_;PartyRequestState& solo_state_;VerticalCombatSoloSoulWorld& solo_world_;
};
// Per-actor slice of the 00421f80 zone loop, preserving the shared tick,
// global live cursor and recovery/effects/duel/rebirth ordering. All callbacks
// are mandatory real owners; do not run a second copy of this loop.
void vertical_combat_recovery_zone(ZoneUsers&,std::uint32_t,VerticalCombatRecoveryZoneServices&);
// CZone::UpdateUserAttack 00421930; same supplied tick for the live cursor.
// Caller serializes the zone and uses this in place of the other zone poller.
void vertical_combat_scheduler_zone(ZoneUsers&,std::uint32_t,VerticalCombatSchedulerZoneServices&);
}
