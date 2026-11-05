#pragma once
#include "eternity/game/inventory.hpp"
#include "eternity/game/character_replica_delivery.hpp"
#include <span>
#include <stdexcept>

namespace eternity::game {
class GroundItemPool;
class ItemIds;
class ItemIdClock;
// A real cohosted DB-agent delivery failure; no result is converted to success.
class VerticalInventoryDeliveryError final:public std::runtime_error {
public:
    explicit VerticalInventoryDeliveryError(InventoryReplicaResult result)
        :std::runtime_error("Inventory replica delivery failed"),result_(result) {}
    InventoryReplicaResult result() const noexcept {return result_;}
private:
    InventoryReplicaResult result_;
};
// The original DB-agent reports success when item-cache allocation fails.
// The guarded native adapter signals that distinct observed outcome explicitly.
class VerticalInventoryInsertionError final:public std::runtime_error {
public:
    VerticalInventoryInsertionError():std::runtime_error("Inventory replica did not retain inserted item") {}
};
// The original DB-agent silently accepts removal of an absent source slot.
// The guarded native adapter reports that divergence before applying it.
class VerticalInventoryRemovalError final:public std::runtime_error {
public:
    VerticalInventoryRemovalError():std::runtime_error("Inventory replica removal source is absent") {}
};
class VerticalInventorySourceMismatch final:public std::runtime_error {
public:
    VerticalInventorySourceMismatch():std::runtime_error("Inventory replica removal source differs from actor") {}
};
class VerticalInventoryDestinationOccupied final:public std::runtime_error {
public:
    VerticalInventoryDestinationOccupied():std::runtime_error("Inventory replica exchange destination is occupied") {}
};
// Required real exchange delivery: validate the actor's expected source UID
// and pre-transfer count against the retained replica under its save lock,
// then apply the original 0702 packet while holding that same lock.
class VerticalInventoryExchangeWorld:public InventoryWorld {
public:
    // Apply a new exchange item only to an empty retained replica slot, under
    // its save lock, and verify the requested UID was actually inserted.
    virtual void send_exchange_insertion(const net::Packet&) = 0;
    // Pure snapshot of the actual retained destination under its save lock.
    virtual void validate_exchange_destination(std::uint32_t account,std::uint8_t bag,std::uint8_t slot) = 0;
    // Pure live check of the actual retained replica under its save lock.
    virtual void validate_exchange_source(std::uint32_t account,std::uint8_t bag,std::uint8_t slot,
        std::uint64_t expected_uid,std::uint8_t expected_count) = 0;
    virtual void send_exchange_removal(const net::Packet&,std::uint64_t expected_uid,std::uint8_t expected_count) = 0;
};
// Two actual retained character-replica owners for a serialized exchange.
// All non-DB operations are forwarded to the real services owner. The caller
// retains both replicas and their actual save mutexes, services and actors;
// account IDs are distinct and verified against those exact retained replicas.
// A failure propagates immediately, without rolling back earlier sends or actor
// changes. Applied 0701 is checked for a retained item, and 0702 requires a
// retained source, under the same lock.
class VerticalInventoryReplicaWorld final:public VerticalInventoryExchangeWorld {
public:
    VerticalInventoryReplicaWorld(InventoryWorld& services,std::uint32_t first_account,CharacterReplica& first,std::recursive_mutex& first_mutex,
        std::uint32_t second_account,CharacterReplica& second,std::recursive_mutex& second_mutex);
    void send_inventory_database(const net::Packet&) override;
    void send_exchange_insertion(const net::Packet&) override;
    void validate_exchange_destination(std::uint32_t,std::uint8_t,std::uint8_t) override;
    void validate_exchange_source(std::uint32_t,std::uint8_t,std::uint8_t,std::uint64_t,std::uint8_t) override;
    void send_exchange_removal(const net::Packet&,std::uint64_t,std::uint8_t) override;
    void send_money_database(const net::Packet&) override;
    std::uint32_t lock_inventory_party(Actor&) override;
    void unlock_inventory_party(std::uint32_t) noexcept override;
    std::optional<bool> inventory_party_kind(const Actor&) override;
    void send_inventory_party(Actor&,const net::Packet&) override;
    std::string inventory_item_name(const ItemDefinition&) override;
    std::string inventory_drop_user_name(Actor&,std::uint32_t,bool) override;
    bool return_inventory_item(Actor&,GroundItem&) override;
    GameLogSubject money_log_subject(const Actor&) override;
    std::chrono::sys_seconds money_local_calendar() override;
    void send_money_log(const net::Packet&) override;
private:
    struct ExpectedRemoval {std::uint64_t uid;std::uint8_t count;};
    void deliver(const net::Packet&,const ExpectedRemoval* expected=nullptr,bool exchange_insertion=false);
    InventoryWorld& services_;std::uint32_t first_account_,second_account_;
    CharacterReplica& first_replica_;CharacterReplica& second_replica_;
    std::recursive_mutex& first_mutex_;std::recursive_mutex& second_mutex_;
    CharacterReplicaDelivery first_;CharacterReplicaDelivery second_;
};
// Required live owners for the original handler prefix. Callbacks are synchronous;
// the packet, Actor, zone, pool and services must survive both callbacks.
class VerticalInventoryActions {
public:
    virtual ~VerticalInventoryActions() = default;
    virtual void cancel_action_exchange(Actor&) = 0; // CUser::CancleActionExc
    virtual void end_shop(Actor&) = 0;              // stMyShop::Ended
    virtual bool pickup_checks() = 0; // Pure read of retained ps_game.exe 00541af8 setting.
};
enum class VerticalInventoryRequest {unhandled, malformed, ignored, pickup, drop};
struct VerticalInventoryResult {
    VerticalInventoryRequest request;
    std::optional<InventoryReceipt> receipt;
    std::optional<ItemDropResult> drop_result{};
};
// ps_game.exe CUser::PacketMaininterface 00468420 / 004686c9..004686f2.
// Only opcode 0205 is owned here. Framing policy belongs to the caller.
// The pickup-check setting must remain stable for the synchronous pickup.
VerticalInventoryResult vertical_inventory_request(Actor&,std::span<const std::uint8_t>,
    ZoneItems&,const ItemQuestLinks&,ItemPickupWorld&,GroundItemPool&,
    VerticalInventoryActions&);
// Native DB-domain guard for a live pickup: a non-money ground item is not
// removed when the recipient can scan unsupported DB-agent bags. Rechecks the
// live ID and maximum after request callbacks. Money and absent items follow
// the original path. The serialized owner pins actor, packet, zone and items;
// pickup services must not mutate the selected item or bag maximum reentrantly.
VerticalInventoryResult vertical_inventory_guarded_pickup_request(Actor&,std::span<const std::uint8_t>,
    ZoneItems&,const ItemQuestLinks&,ItemPickupWorld&,GroundItemPool&,VerticalInventoryActions&);
// ps_game.exe CUser::PacketMaininterface 00468420, opcode 0206 at
// 004686f7..00468758. The synchronous callbacks must leave any reloaded bag
// and slot within Actor::drop_item's supported original storage bounds.
VerticalInventoryResult vertical_inventory_drop_request(Actor&,std::span<const std::uint8_t>,
    ItemDropWorld&,GroundItemPool&,VerticalInventoryActions&);
// Native DB-domain guard for live drop dispatch: rejects bag >=6 before
// callbacks and rechecks the reloaded bag after the shop callback. Original
// ignored/malformed/unhandled requests retain their original behavior. The
// callback must keep actor, packet, services and pooled items pinned.
VerticalInventoryResult vertical_inventory_guarded_drop_request(Actor&,std::span<const std::uint8_t>,
    ItemDropWorld&,GroundItemPool&,VerticalInventoryActions&);
// PacketExchange 0046ec90: 0a02, nonzero 0a05, 0a06, 0a07 and 0a08 only. Shop and send callbacks
// must keep the actor, captured peer and packet alive, and must not change
// the opcode or change 0a05 into settlement command 0. Reloaded offer and inventory
// positions must remain valid and
// refer to a retained pooled item; the session serializes both actors.
enum class VerticalExchangeRequest {unhandled,malformed,ignored,updated};
// Original settlement preflight checks; they do not transfer items or money.
// 0046f830 checks the payer and recipient; 0046f860 counts every nonzero
// offered bag against currently empty unlocked recipient slots, without merging.
bool vertical_inventory_exchange_money_valid(const Actor& payer,const Actor& recipient,std::uint32_t amount) noexcept;
bool vertical_inventory_exchange_items_fit(const Actor& recipient,const ItemExchangeState& offer);
// CUser::Exchange 0046f4a0 failure prefix. A transfer_required result is
// not settlement: no balances, items or database state have been changed.
// Both actors and their scenes must remain alive through the failure sends.
enum class VerticalExchangePreflight {peer_money,actor_money,actor_space,peer_space,transfer_required};
VerticalExchangePreflight vertical_inventory_exchange_preflight(Actor& actor,Actor& peer);
// Native integration boundary: ps_dbAgent.exe CUser has six inventory bags;
// larger bag indices alias its bank. Check before any money/item mutation.
// Conservative: both actors' unlocked maxima and all offered bags must fit.
// This is not an original client response or original settlement preflight.
bool vertical_inventory_exchange_db_compatible(const Actor& actor,const Actor& peer) noexcept;
// CUser::Exchange 0046f53e..0046f71c. Writes only original log fields,
// preserving caller-provided scratch tails. The initiator field is the live
// original CUser+70 connection IPv4 DWORD and is written only to incoming 0803 logs.
// The four input bytes are the real peer IPv4 octets in network byte order.
// This reproduces the original little-endian DWORD load from sockaddr_in.
std::uint32_t vertical_inventory_exchange_ipv4_field(std::span<const std::uint8_t,4>) noexcept;
enum class VerticalExchangeMoneyLog {outgoing,incoming};
void vertical_inventory_exchange_money_log(std::span<std::uint8_t> scratch,VerticalExchangeMoneyLog,
    const GameLogSubject& subject,const GameLogSubject& counterparty,std::uint32_t amount,
    std::uint32_t balance,std::uint32_t initiator_field,std::chrono::sys_seconds local_calendar);
// Money phase of CUser::Exchange 0046f53e..0046f729 after successful
// preflight. Uses actual balances, live exchange offers and mandatory MoneyWorld
// DB/log delivery. Scratch buffers and initiator_field refer to their real
// retained owners; both actors and buffers survive synchronous callbacks.
// Subject lookup is a pure live read. Callbacks complete normally and may
// change live offers/balances; the executor serializes both actors.
void vertical_inventory_exchange_money_phase(Actor& actor,Actor& peer,MoneyWorld&,
    net::Packet& outgoing_scratch,net::Packet& incoming_scratch,const std::uint32_t& initiator_field);
// ExchangeItemAdd must perform the actual item transfer using the source's live
// exchange offer, including its DB, client and log sends. No default exists.
class VerticalExchangeItemTransfer {
public:
    virtual ~VerticalExchangeItemTransfer() = default;
    // Pure validation against actual source owners for both directions; no
    // success default. Throws on divergence before settlement money changes.
    virtual void validate_exchange_sources(Actor& actor,Actor& peer) = 0;
    virtual void transfer_exchange_items(Actor& recipient,Actor& source) = 0;
};
// CUser::Exchange 0046f72a..0046f767, only after preflight and the real money
// phase. The real transfer owner must complete both calls synchronously; actors
// and scenes remain alive and serialized throughout all callbacks.
void vertical_inventory_exchange_item_phase(Actor& actor,Actor& peer,VerticalExchangeItemTransfer&);
struct VerticalExchangeItemSelection {
    std::size_t offer_index;
    std::uint8_t destination_bag,destination_slot;
    GroundItem* source_item;
    GroundItemPool* source_pool;
    bool whole;
};
// ExchangeItemAdd's next live offer and first empty destination slot. Resume
// at the prior index + 1 after each completed operation, re-reading live state.
// Both actors and the returned pooled item remain pinned until consumed.
std::optional<VerticalExchangeItemSelection> vertical_inventory_exchange_next_item(
    Actor& recipient,Actor& source,std::size_t first_offer=0);
// Split allocation and retained ownership only, 0046fb21..0046fceb. A null
// result leaves both inventories unchanged and requires the original failure
// packet/log path. On success the caller must immediately perform the original
// DB sends and client/log sequence; this is not a complete settlement. The
// pool, UID generator and clock are the real shared owners. Both actors and
// selected owners remain stable, and the clock completes normally without
// reentry or mutation, throughout this serialized operation.
// Native exceptions before adoption restore the source and release the new
// item; an already-issued UID remains consumed, and the exception propagates.
GroundItem* vertical_inventory_exchange_split_item(Actor& recipient,Actor& source,
    const VerticalExchangeItemSelection&,GroundItemPool&,ItemIds&,ItemIdClock&);
// Complete one selected split entry, including the original allocator-failure
// notifications. All scratch buffers are retained, real caller-owned bytes.
// World sends complete normally, keep actors/items/buffers pinned and do not
// mutate the offer, item, definition or destination during this operation.
// Subject/name lookups are pure; the serialized caller must finish remaining
// offers and settlement. The return value reports allocation, not settlement.
// Unsupported DB-agent bag indices throw before allocation or mutation.
bool vertical_inventory_exchange_split_entry(Actor& recipient,Actor& source,
    const VerticalExchangeItemSelection&,GroundItemPool&,ItemIds&,ItemIdClock&,VerticalInventoryExchangeWorld&,
    net::Packet& db_scratch,net::Packet& outgoing_log,net::Packet& receiver_scratch,net::Packet& incoming_log);
// Mandatory real whole-item transfer implementation, including ownership,
// database, client and log delivery; no default or success substitute exists.
class VerticalExchangeWholeItemTransfer {
public:
    virtual ~VerticalExchangeWholeItemTransfer() = default;
    // Pure real-owner validation of a currently selected whole source. Must
    // throw if actual non-freeing transfer/delivery cannot be performed.
    virtual void validate_whole_exchange_source(Actor& recipient,Actor& source,
        const ExchangeItemReference&,GroundItem&,GroundItemPool&) = 0;
    virtual void transfer_whole_exchange_item(Actor& recipient,Actor& source,const VerticalExchangeItemSelection&) = 0;
};
// Whole-item transfer using the existing actor, replica and transport owners.
// All services and scratch buffers must remain live and serialized throughout.
class VerticalExchangeWholeItemTransferExecutor final:public VerticalExchangeWholeItemTransfer {
public:
    VerticalExchangeWholeItemTransferExecutor(VerticalInventoryExchangeWorld&,net::Packet& db_scratch,
        net::Packet& outgoing_log,net::Packet& receiver_scratch,net::Packet& incoming_log);
    void validate_whole_exchange_source(Actor&,Actor&,const ExchangeItemReference&,GroundItem&,GroundItemPool&) override;
    void transfer_whole_exchange_item(Actor&,Actor&,const VerticalExchangeItemSelection&) override;
private:
    VerticalInventoryExchangeWorld& world_;net::Packet& db_;net::Packet& outgoing_;net::Packet& receiver_;net::Packet& incoming_;
};
// ExchangeItemAdd's ordered eight-entry loop. The whole owner and the real
// split services complete synchronously, preserve actor/item lifetimes and
// do not mutate any remaining offer while processing a selected entry.
// Does not perform money, preflight, the reverse direction or settlement.
void vertical_inventory_exchange_transfer_items(Actor& recipient,Actor& source,
    GroundItemPool&,ItemIds&,ItemIdClock&,VerticalInventoryExchangeWorld&,VerticalExchangeWholeItemTransfer&,
    net::Packet& db_scratch,net::Packet& outgoing_log,net::Packet& receiver_scratch,net::Packet& incoming_log);
// Binds the actual retained split loop to the two-direction settlement phase.
// The mandatory whole-item owner must implement real ownership and delivery.
// All referenced services and scratch buffers outlive synchronous calls.
class VerticalExchangeItemTransferExecutor final:public VerticalExchangeItemTransfer {
public:
    VerticalExchangeItemTransferExecutor(GroundItemPool&,ItemIds&,ItemIdClock&,VerticalInventoryExchangeWorld&,
        VerticalExchangeWholeItemTransfer&,net::Packet& db_scratch,net::Packet& outgoing_log,
        net::Packet& receiver_scratch,net::Packet& incoming_log);
    void validate_exchange_sources(Actor& actor,Actor& peer) override;
    void transfer_exchange_items(Actor& recipient,Actor& source) override;
private:
    GroundItemPool& pool_;ItemIds& ids_;ItemIdClock& clock_;VerticalInventoryExchangeWorld& world_;
    VerticalExchangeWholeItemTransfer& whole_;
    net::Packet& db_;net::Packet& outgoing_;net::Packet& receiver_;net::Packet& incoming_;
};
// CUser::ExchangeItemAdd 0046f910 sender 0206 and receiver 0205 packets.
// The source is the live original pooled item, even for a split allocation.
// The 36-byte receiver scratch preserves bytes following the craft terminator.
net::Packet vertical_inventory_exchange_sender_packet(const ExchangeItemReference& offer,
    const GroundItem& source,bool source_remains);
// ExchangeItemAdd 0406/0405 logs use the original source item even for a split.
// The item name comes from the live item-definition owner. Scratch tails survive.
enum class VerticalExchangeItemLog {outgoing,incoming};
void vertical_inventory_exchange_item_log(std::span<std::uint8_t> scratch,VerticalExchangeItemLog,
    const GameLogSubject& subject,const GameLogSubject& counterparty,const GroundItem& source,
    std::string_view item_name,std::uint8_t offered_count,std::uint8_t destination_bag,
    std::uint8_t destination_slot,std::chrono::sys_seconds local_calendar);
void vertical_inventory_exchange_receiver_packet(std::span<std::uint8_t> scratch,
    std::uint8_t destination_bag,std::uint8_t destination_slot,std::uint8_t offered_count,
    const GroundItem& source);
// ExchangeItemAdd DB-agent 0701/0702 sends. The 57-byte add scratch
// preserves bytes after the transferred pooled item's craft terminator.
// The caller supplies the live source count for a whole move or live offered
// count for a split, sampled after the add send as in the original.
void vertical_inventory_exchange_db_add_packet(std::span<std::uint8_t> scratch,
    std::uint32_t account,std::uint8_t bag,std::uint8_t slot,const GroundItem& transferred);
net::Packet vertical_inventory_exchange_db_remove_packet(std::uint32_t account,
    const ExchangeItemReference& offer,std::uint8_t count);
// Required real implementation of CUser::Exchange 0046f4a0, including all
// failure/success sends, transfers, DB delivery and logs; no default behavior.
// The call must finish synchronously and normally while both actors are pinned.
class VerticalExchangeSettlement {
public:
    virtual ~VerticalExchangeSettlement() = default;
    // Pure native readiness validation; it must use the actual owners and may
    // return normally for an original preflight rejection. No default exists.
    virtual void validate_before_ready(Actor& actor,Actor& peer) = 0;
    virtual void settle_exchange(Actor& actor,Actor& peer) = 0;
};
// CUser::Exchange 0046f4a0 composition. The item owner is mandatory and must
// actually transfer every whole and split item and deliver its real messages;
// this executor cannot make an incomplete item owner into a settlement.
// Both actors, services, scratch and the initiator's actual connection IPv4
// field (CUser+70) stay pinned;
// callbacks complete synchronously and normally. Compatibility is checked
// before original preflight, money mutation or sends; no client response is
// invented for an unsupported DB-agent bag domain.
class VerticalExchangeSettlementExecutor final:public VerticalExchangeSettlement {
public:
    VerticalExchangeSettlementExecutor(MoneyWorld&,VerticalExchangeItemTransfer&,
        net::Packet& outgoing_money_log,net::Packet& incoming_money_log,const std::uint32_t& initiator_field);
    void validate_before_ready(Actor& actor,Actor& peer) override;
    void settle_exchange(Actor& actor,Actor& peer) override;
private:
    MoneyWorld& money_;
    VerticalExchangeItemTransfer& items_;
    net::Packet& outgoing_;
    net::Packet& incoming_;
    const std::uint32_t& initiator_field_;
};
// 0a05 control 0 only. The caller must supply the actual settlement owner even
// when the peer is not yet ready. Both actors, scenes and packet remain alive.
VerticalExchangeRequest vertical_inventory_exchange_ready(Actor&,std::span<const std::uint8_t>,
    VerticalInventoryActions&,VerticalExchangeSettlement&);
// Native guarded entry for live ready dispatch. Rejects incompatible DB bags
// before shop/ready effects. If settlement is imminent, validates the actual
// owners before shop and again afterward, before original readiness sends.
// Incompatible domains throw without an invented client response. Both actors,
// scenes, packet and services remain pinned; readiness sends must not mutate
// either actor's bag maxima, offers, peer or ready state reentrantly.
VerticalExchangeRequest vertical_inventory_exchange_guarded_ready(Actor&,std::span<const std::uint8_t>,
    VerticalInventoryActions&,VerticalExchangeSettlement&);
// 0a01 at 0046ecc9..0046edce. The supplied zone must be the actor's
// current zone, and timeout must be its retained CUser+5830 owner. Both
// actors and the zone must stay alive across synchronous callbacks.
VerticalExchangeRequest vertical_inventory_exchange_invite(Actor&,std::span<const std::uint8_t>,
    ZoneUsers&,std::uint32_t& timeout,VerticalInventoryActions&);
// Exchange-only prefix of CUser::UpdateTimeover 0048e310..0048e39c.
// timeout and peer_timeout refer to retained CUser+5830 storage; peer_timeout
// is required when mode 1 has a peer in mode 1. If the peer is the actor it
// must alias timeout. The executor serializes both actors and their timers.
bool vertical_inventory_exchange_expire(Actor&,std::uint32_t now,std::uint32_t& timeout,
    std::uint32_t* peer_timeout);
VerticalExchangeRequest vertical_inventory_exchange_request(Actor&,std::span<const std::uint8_t>,
    VerticalInventoryActions&);

}
