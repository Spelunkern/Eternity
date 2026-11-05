#include "eternity/game/vertical_inventory.hpp"
#include "eternity/game/actor.hpp"
#include "eternity/game/item_ids.hpp"
#include "eternity/game/item_pool.hpp"
#include <algorithm>
#include <stdexcept>

namespace eternity::game {
VerticalInventoryReplicaWorld::VerticalInventoryReplicaWorld(InventoryWorld& services,std::uint32_t first_account,
    CharacterReplica& first,std::recursive_mutex& first_mutex,std::uint32_t second_account,CharacterReplica& second,std::recursive_mutex& second_mutex)
    :services_(services),first_account_(first_account),second_account_(second_account),first_replica_(first),second_replica_(second),
     first_mutex_(first_mutex),second_mutex_(second_mutex),first_(first,first_mutex),second_(second,second_mutex) {
    if(first_account==second_account) throw std::invalid_argument("Exchange replica accounts must be distinct");
    {std::lock_guard lock(first_mutex_);if(first_replica_.core().user_id!=first_account_) throw std::invalid_argument("First replica account mismatch");}
    {std::lock_guard lock(second_mutex_);if(second_replica_.core().user_id!=second_account_) throw std::invalid_argument("Second replica account mismatch");}
}
void VerticalInventoryReplicaWorld::deliver(const net::Packet& packet,const ExpectedRemoval* expected,bool exchange_insertion) {
    if(packet.size()<6) throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
    std::uint32_t account=0;for(unsigned i=0;i<4;++i) account|=std::uint32_t(packet[2+i])<<(8*i);
    CharacterReplicaDelivery* target=account==first_account_?&first_:account==second_account_?&second_:nullptr;
    if(!target) throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
    auto& mutex=account==first_account_?first_mutex_:second_mutex_;
    auto& replica=account==first_account_?first_replica_:second_replica_;
    std::lock_guard lock(mutex); // Same recursive mutex used by the real delivery and saver.
    if(exchange_insertion && packet[18]<6 && packet[19]<24) {
        const auto rows=replica.inventory().inventory();
        if(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==packet[18] && row.item.slot==packet[19];}))
            throw VerticalInventoryDestinationOccupied();
    }
    // ps_dbAgent.exe CUser::PacketUserItem 00408480 accepts a missing 0702
    // source as a no-op. Do not treat that as a completed native transfer.
    if(packet.size()==9 && packet[0]==2 && packet[1]==7 && packet[6]<6 && packet[7]<24) {
        const auto rows=replica.inventory().inventory();
        const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.item.bag==packet[6] && value.item.slot==packet[7];});
        if(row==rows.end()) throw VerticalInventoryRemovalError();
        if(expected && (row->item.uid!=expected->uid || row->item.count!=expected->count)) throw VerticalInventorySourceMismatch();
    }
    const auto result=target->apply(packet);
    if(result!=InventoryReplicaResult::applied) throw VerticalInventoryDeliveryError(result);
    if(packet.size()==57 && packet[0]==1 && packet[1]==7) {
        const auto rows=replica.inventory().inventory();
        std::uint64_t uid=0;for(unsigned i=0;i<8;++i) uid|=std::uint64_t(packet[6+i])<<(8*i);
        if(std::none_of(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==packet[18] && row.item.slot==packet[19] && (!exchange_insertion || row.item.uid==uid);}))
            throw VerticalInventoryInsertionError();
    }
}
void VerticalInventoryReplicaWorld::send_inventory_database(const net::Packet& p) {deliver(p);}
void VerticalInventoryReplicaWorld::send_exchange_insertion(const net::Packet& p) {
    if(p.size()!=57 || p[0]!=1 || p[1]!=7) throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
    deliver(p,nullptr,true);
}
void VerticalInventoryReplicaWorld::validate_exchange_destination(std::uint32_t account,std::uint8_t bag,std::uint8_t slot) {
    if(bag>=6 || slot>=24 || (account!=first_account_ && account!=second_account_))
        throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
    auto& mutex=account==first_account_?first_mutex_:second_mutex_;
    auto& replica=account==first_account_?first_replica_:second_replica_;
    std::lock_guard lock(mutex);
    const auto rows=replica.inventory().inventory();
    if(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.item.bag==bag && row.item.slot==slot;}))
        throw VerticalInventoryDestinationOccupied();
}
void VerticalInventoryReplicaWorld::validate_exchange_source(std::uint32_t account,std::uint8_t bag,std::uint8_t slot,
    std::uint64_t uid,std::uint8_t count) {
    if(bag>=6 || slot>=24 || (account!=first_account_ && account!=second_account_))
        throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
    auto& mutex=account==first_account_?first_mutex_:second_mutex_;
    auto& replica=account==first_account_?first_replica_:second_replica_;
    std::lock_guard lock(mutex);
    const auto rows=replica.inventory().inventory();
    const auto row=std::find_if(rows.begin(),rows.end(),[&](const auto& value){return value.item.bag==bag && value.item.slot==slot;});
    if(row==rows.end()) throw VerticalInventoryRemovalError();
    if(row->item.uid!=uid || row->item.count!=count) throw VerticalInventorySourceMismatch();
}
void VerticalInventoryReplicaWorld::send_exchange_removal(const net::Packet& p,std::uint64_t uid,std::uint8_t count) {
    if(p.size()!=9 || p[0]!=2 || p[1]!=7) throw VerticalInventoryDeliveryError(InventoryReplicaResult::rejected);
    const ExpectedRemoval expected{uid,count};deliver(p,&expected);
}
void VerticalInventoryReplicaWorld::send_money_database(const net::Packet& p) {deliver(p);}
std::uint32_t VerticalInventoryReplicaWorld::lock_inventory_party(Actor& a) {return services_.lock_inventory_party(a);}
void VerticalInventoryReplicaWorld::unlock_inventory_party(std::uint32_t token) noexcept {services_.unlock_inventory_party(token);}
std::optional<bool> VerticalInventoryReplicaWorld::inventory_party_kind(const Actor& a) {return services_.inventory_party_kind(a);}
void VerticalInventoryReplicaWorld::send_inventory_party(Actor& a,const net::Packet& p) {services_.send_inventory_party(a,p);}
std::string VerticalInventoryReplicaWorld::inventory_item_name(const ItemDefinition& item) {return services_.inventory_item_name(item);}
std::string VerticalInventoryReplicaWorld::inventory_drop_user_name(Actor& a,std::uint32_t id,bool lookup) {return services_.inventory_drop_user_name(a,id,lookup);}
bool VerticalInventoryReplicaWorld::return_inventory_item(Actor& a,GroundItem& item) {return services_.return_inventory_item(a,item);}
GameLogSubject VerticalInventoryReplicaWorld::money_log_subject(const Actor& a) {return services_.money_log_subject(a);}
std::chrono::sys_seconds VerticalInventoryReplicaWorld::money_local_calendar() {return services_.money_local_calendar();}
void VerticalInventoryReplicaWorld::send_money_log(const net::Packet& p) {services_.send_money_log(p);}

VerticalInventoryResult vertical_inventory_request(Actor& actor,std::span<const std::uint8_t> packet,
    ZoneItems& zone,const ItemQuestLinks& links,ItemPickupWorld& world,GroundItemPool& pool,
    VerticalInventoryActions& actions) {
    if(packet.size()<2) return {VerticalInventoryRequest::malformed,{}};
    if(packet[0]!=0x05 || packet[1]!=0x02) return {VerticalInventoryRequest::unhandled,{}};
    if(packet.size()<6) return {VerticalInventoryRequest::malformed,{}};
    if(actor.attack_state().mode==1) return {VerticalInventoryRequest::ignored,{}};
    actions.cancel_action_exchange(actor);
    actions.end_shop(actor);
    // Original loads the ID after both callbacks; nested code can alter the buffer.
    std::uint32_t id=0;
    for(unsigned i=0;i<4;++i) id|=std::uint32_t(packet[2+i])<<(8*i);
    return {VerticalInventoryRequest::pickup,actor.pickup_item(zone,id,actions.pickup_checks(),links,world,pool)};
}
VerticalInventoryResult vertical_inventory_guarded_pickup_request(Actor& actor,std::span<const std::uint8_t> packet,
    ZoneItems& zone,const ItemQuestLinks& links,ItemPickupWorld& world,GroundItemPool& pool,VerticalInventoryActions& actions) {
    // ps_dbAgent.exe CUser::PacketUserItem 00408480: bag >=6 aliases bank.
    // Money items do not enter inventory, and missing items retain their error.
    if(packet.size()<6 || packet[0]!=5 || packet[1]!=2 || actor.attack_state().mode==1)
        return vertical_inventory_request(actor,packet,zone,links,world,pool,actions);
    const auto read_id=[&] {
        std::uint32_t id=0;for(unsigned i=0;i<4;++i) id|=std::uint32_t(packet[2+i])<<(8*i);
        return id;
    };
    const auto check=[&](std::uint32_t id) {
        const auto* item=zone.find(id);
        if(item && item->item.type!=26 && actor.status().skills.max_bags>5)
            throw std::out_of_range("Pickup bag exceeds DB-agent inventory storage");
    };
    check(read_id());
    struct GuardedActions final:VerticalInventoryActions {
        VerticalInventoryActions& owner;decltype(check)& validate;decltype(read_id)& read;std::uint32_t selected=0;
        GuardedActions(VerticalInventoryActions& a,decltype(check)& f,decltype(read_id)& r):owner(a),validate(f),read(r) {}
        void cancel_action_exchange(Actor& value) override {owner.cancel_action_exchange(value);}
        void end_shop(Actor& value) override {owner.end_shop(value);selected=read();}
        bool pickup_checks() override {const auto value=owner.pickup_checks();validate(selected);return value;}
    } guarded(actions,check,read_id);
    return vertical_inventory_request(actor,packet,zone,links,world,pool,guarded);
}
VerticalInventoryResult vertical_inventory_drop_request(Actor& actor,std::span<const std::uint8_t> packet,
    ItemDropWorld& world,GroundItemPool& pool,VerticalInventoryActions& actions) {
    if(packet.size()<2) return {VerticalInventoryRequest::malformed,{}};
    if(packet[0]!=0x06 || packet[1]!=0x02) return {VerticalInventoryRequest::unhandled,{}};
    if(packet.size()<5) return {VerticalInventoryRequest::malformed,{}};
    // CUser::IsBag 0045b3d0 compares bag against the signed maximum and slot
    // against 24. Request fields are zero-extended bytes at the call site.
    if(actor.attack_state().mode==1 || packet[2]==0 || packet[4]==0 ||
       static_cast<std::int32_t>(packet[2])>actor.status().skills.max_bags || packet[3]>=24)
        return {VerticalInventoryRequest::ignored,{}};
    actions.cancel_action_exchange(actor);
    actions.end_shop(actor);
    return {VerticalInventoryRequest::drop,{},actor.drop_item(packet[2],packet[3],packet[4],world,pool)};
}

VerticalInventoryResult vertical_inventory_guarded_drop_request(Actor& actor,std::span<const std::uint8_t> packet,
    ItemDropWorld& world,GroundItemPool& pool,VerticalInventoryActions& actions) {
    // ps_dbAgent.exe CUser::PacketUserItem 00408480: bag >=6 aliases bank.
    // Keep the original 004686f7 gate, including ignored requests, intact.
    if(packet.size()<5 || packet[0]!=6 || packet[1]!=2 || actor.attack_state().mode==1 ||
       packet[2]==0 || packet[4]==0 || static_cast<std::int32_t>(packet[2])>actor.status().skills.max_bags || packet[3]>=24)
        return vertical_inventory_drop_request(actor,packet,world,pool,actions);
    const auto check=[&] {if(packet[2]>=6) throw std::out_of_range("Drop bag exceeds DB-agent inventory storage");};
    check();
    struct GuardedActions final:VerticalInventoryActions {
        VerticalInventoryActions& owner;decltype(check)& validate;
        GuardedActions(VerticalInventoryActions& a,decltype(check)& f):owner(a),validate(f) {}
        void cancel_action_exchange(Actor& value) override {owner.cancel_action_exchange(value);}
        void end_shop(Actor& value) override {owner.end_shop(value);validate();}
        bool pickup_checks() override {return owner.pickup_checks();}
    } guarded(actions,check);
    return vertical_inventory_drop_request(actor,packet,world,pool,guarded);
}

bool vertical_inventory_exchange_money_valid(const Actor& payer,const Actor& recipient,std::uint32_t amount) noexcept {
    // ps_game.exe CUser::ExchangeMoneyAddEnable 0046f830; the addition is
    // checked against UINT32_MAX before either actor's balance changes.
    return amount<=payer.money() && money_addable(recipient.money(),amount);
}
bool vertical_inventory_exchange_items_fit(const Actor& recipient,const ItemExchangeState& offer) {
    // ps_game.exe CUser::ExchangeItemAddEnable 0046f860 counts offer entries,
    // including stale references, and requires one empty bag slot per entry.
    auto needed=std::count_if(offer.items.begin(),offer.items.end(),[](const auto& item){return item.bag!=0;});
    if(needed==0) return true;
    for(std::int32_t bag=1;bag<=recipient.status().skills.max_bags;++bag) {
        if(bag>9) throw std::out_of_range("Original exchange bag count exceeds retained storage");
        for(std::uint16_t slot=0;slot<24;++slot)
            if(recipient.inventory_slot_empty(static_cast<std::uint8_t>(bag),slot) && --needed==0) return true;
    }
    return false;
}

VerticalExchangePreflight vertical_inventory_exchange_preflight(Actor& actor,Actor& peer) {
    // ps_game.exe CUser::Exchange 0046f4a0..0046f53e and failure exits
    // 0046f76a..0046f826. The later transfer is not implemented here.
    const auto fail=[&](VerticalExchangePreflight result,std::uint8_t local,std::uint8_t remote) {
        actor.send_self({4,0x0a,local});peer.send_self({4,0x0a,remote});return result;
    };
    if(!vertical_inventory_exchange_money_valid(peer,actor,peer.exchange().money)) return fail(VerticalExchangePreflight::peer_money,6,7);
    if(!vertical_inventory_exchange_money_valid(actor,peer,actor.exchange().money)) return fail(VerticalExchangePreflight::actor_money,7,6);
    if(!vertical_inventory_exchange_items_fit(actor,peer.exchange())) return fail(VerticalExchangePreflight::actor_space,4,5);
    if(!vertical_inventory_exchange_items_fit(peer,actor.exchange())) return fail(VerticalExchangePreflight::peer_space,5,4);
    return VerticalExchangePreflight::transfer_required;
}

bool vertical_inventory_exchange_db_compatible(const Actor& actor,const Actor& peer) noexcept {
    // ps_dbAgent.exe CUser::PacketUserItem 00408480: CUser+e0 [6][24],
    // followed immediately by bank at +320. Bag >=6 aliases bank storage.
    const auto safe=[](const Actor& value) {
        if(value.status().skills.max_bags>5) return false;
        return std::all_of(value.exchange().items.begin(),value.exchange().items.end(),[](const auto& offer){return offer.bag<6;});
    };
    return safe(actor) && safe(peer);
}

std::uint32_t vertical_inventory_exchange_ipv4_field(std::span<const std::uint8_t,4> octets) noexcept {
    // ps_game.exe SInternetAddress::GetIP 00404e10 reads sockaddr_in+4.
    return std::uint32_t(octets[0]) | (std::uint32_t(octets[1])<<8) |
        (std::uint32_t(octets[2])<<16) | (std::uint32_t(octets[3])<<24);
}

void vertical_inventory_exchange_money_log(std::span<std::uint8_t> scratch,VerticalExchangeMoneyLog kind,
    const GameLogSubject& subject,const GameLogSubject& counterparty,std::uint32_t amount,
    std::uint32_t balance,std::uint32_t initiator_field,std::chrono::sys_seconds local_calendar) {
    // ps_game.exe CUser::Exchange 0046f559..0046f717; each header is filled
    // separately, and bytes after the written name remain existing stack bytes.
    const bool incoming=kind==VerticalExchangeMoneyLog::incoming;
    if(scratch.size()!=(incoming?111U:126U) || counterparty.character_name.size()>20 || counterparty.character_name.find('\0')!=std::string::npos)
        throw std::invalid_argument("Invalid exchange money log buffer or name");
    write_game_log_main(scratch,subject,local_calendar);
    const auto put=[&](std::size_t offset,std::uint32_t value) {for(unsigned i=0;i<4;++i) scratch[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};
    scratch[0]=incoming?3:4;scratch[1]=8;put(74,amount);put(78,balance);put(82,counterparty.character_id);
    std::copy(counterparty.character_name.begin(),counterparty.character_name.end(),scratch.begin()+86);scratch[86+counterparty.character_name.size()]=0;
    if(incoming) put(107,initiator_field);
}

void vertical_inventory_exchange_money_phase(Actor& actor,Actor& peer,MoneyWorld& world,
    net::Packet& outgoing,net::Packet& incoming,const std::uint32_t& initiator_field) {
    // CUser::Exchange 0046f53e..0046f729. The caller must have completed
    // the ordered preflight and continue the actual item phase afterward.
    if(outgoing.size()!=126 || incoming.size()!=111) throw std::invalid_argument("Invalid exchange money scratch buffers");
    const auto log=[&](Actor& subject,Actor& other,Actor& offerer,VerticalExchangeMoneyLog kind,net::Packet& scratch) {
        const auto header=world.money_log_subject(subject);const auto time=world.money_local_calendar();
        const auto amount=offerer.exchange().money;const auto balance=subject.money();
        const auto counterparty=world.money_log_subject(other);const auto field=initiator_field;
        vertical_inventory_exchange_money_log(scratch,kind,header,counterparty,amount,balance,field,time);
        world.send_money_log(scratch);
    };
    if(const auto amount=actor.exchange().money) {
        actor.money()-=amount;peer.money()+=amount;
        log(actor,peer,actor,VerticalExchangeMoneyLog::outgoing,outgoing);
        log(peer,actor,actor,VerticalExchangeMoneyLog::incoming,incoming);
    }
    if(const auto amount=peer.exchange().money) {
        actor.money()+=amount;peer.money()-=amount;
        log(peer,actor,peer,VerticalExchangeMoneyLog::outgoing,outgoing);
        log(actor,peer,peer,VerticalExchangeMoneyLog::incoming,incoming);
    }
    actor.send_money_database(world);peer.send_money_database(world);
}

void vertical_inventory_exchange_item_phase(Actor& actor,Actor& peer,VerticalExchangeItemTransfer& transfer) {
    // ps_game.exe CUser::Exchange 0046f72a..0046f767.
    transfer.transfer_exchange_items(actor,peer);
    transfer.transfer_exchange_items(peer,actor);
    actor.send_self({4,0x0a,0});
    peer.send_self({4,0x0a,0});
}

std::optional<VerticalExchangeItemSelection> vertical_inventory_exchange_next_item(
    Actor& recipient,Actor& source,std::size_t first_offer) {
    // ps_game.exe CUser::ExchangeItemAdd 0046f950..0046f9e7.
    const auto& offers=source.exchange().items;
    for(auto index=first_offer;index<offers.size();++index) {
        const auto& offer=offers[index];
        if(!offer.bag) continue;
        if(offer.bag>9 || offer.slot>=24) throw std::out_of_range("Invalid exchange offer position");
        const auto held=source.retained_inventory_item(offer.bag,offer.slot);
        if(!held) {
            if(!source.inventory_slot_empty(offer.bag,offer.slot)) throw std::logic_error("Exchange source has no pooled owner");
            continue;
        }
        if(!held->item->definition) throw std::logic_error("Exchange source has no item definition");
        for(std::int32_t bag=1;bag<=recipient.status().skills.max_bags;++bag) {
            if(bag>9) throw std::out_of_range("Original exchange bag count exceeds retained storage");
            for(std::uint16_t slot=0;slot<24;++slot) if(recipient.inventory_slot_empty(static_cast<std::uint8_t>(bag),slot)) {
                const auto count=held->item->item.count;
                const bool whole=(held->item->definition->stack_count==1 && count==1) || offer.count==count;
                return VerticalExchangeItemSelection{index,static_cast<std::uint8_t>(bag),static_cast<std::uint8_t>(slot),held->item,held->pool,whole};
            }
        }
    }
    return std::nullopt;
}

GroundItem* vertical_inventory_exchange_split_item(Actor& recipient,Actor& source,
    const VerticalExchangeItemSelection& selection,GroundItemPool& pool,ItemIds& ids,ItemIdClock& clock) {
    // ps_game.exe CUser::ExchangeItemAdd 0046fb21..0046fceb.
    if(selection.whole || selection.offer_index>=source.exchange().items.size()) throw std::invalid_argument("Expected a split exchange selection");
    const auto& offer=source.exchange().items[selection.offer_index];
    if(!offer.bag || offer.bag>9 || offer.slot>=24 || !selection.destination_bag || selection.destination_bag>9 || selection.destination_slot>=24 ||
       selection.destination_bag>recipient.status().skills.max_bags) throw std::invalid_argument("Invalid split exchange position");
    const auto held=source.retained_inventory_item(offer.bag,offer.slot);
    if(!held || held->item!=selection.source_item || held->pool!=selection.source_pool || !held->item->definition ||
       !recipient.inventory_slot_empty(selection.destination_bag,selection.destination_slot)) throw std::logic_error("Split exchange selection changed");
    if((held->item->definition->stack_count==1 && held->item->item.count==1) || offer.count==held->item->item.count)
        throw std::logic_error("Split exchange branch changed");
    auto* item=pool.allocate();if(!item) return nullptr;
    auto& old=*held->item;
    const auto original_count=old.item.count;bool source_changed=false;
    try {
        const auto remaining=static_cast<std::uint8_t>(original_count-offer.count);
        if(!source.set_inventory_item_count(offer.bag,offer.slot,remaining,&old)) throw std::logic_error("Split source changed during allocation");
        source_changed=true;
        item->definition=old.definition;item->item.uid=ids.next();item->item.type=old.item.type;item->item.type_id=old.item.type_id;item->item.count=offer.count;
        item->item.quality=0;item->item.gems.fill(0);
        if(item->definition->server && item->definition->required_wisdom && item->definition->required_wisdom<100) set_ground_item_craft(*item,std::string(20,'0'));
        item->item.made_at=clock.packed_local_calendar();item->item.make_type="T";
        if(!recipient.claim_inventory_item(selection.destination_bag,selection.destination_slot,*item,pool)) throw std::logic_error("Split destination changed during allocation");
    } catch(...) {
        // Native allocation and clock callbacks can throw; the original Win32
        // slice has no such C++ exceptions. Keep the retained owners coherent.
        if(source_changed && !source.set_inventory_item_count(offer.bag,offer.slot,original_count,&old)) std::terminate();
        pool.release(*item);
        throw;
    }
    return item;
}

void vertical_inventory_exchange_item_log(std::span<std::uint8_t> scratch,VerticalExchangeItemLog kind,
    const GameLogSubject& subject,const GameLogSubject& counterparty,const GroundItem& source,
    std::string_view item_name,std::uint8_t offered_count,std::uint8_t destination_bag,
    std::uint8_t destination_slot,std::chrono::sys_seconds local_calendar) {
    // ps_game.exe CUser::ExchangeItemAdd 0046fe48..004700de.
    const bool incoming=kind==VerticalExchangeItemLog::incoming;
    const auto craft_end=std::find(source.craft_storage.begin(),source.craft_storage.end(),std::uint8_t{0});
    if((kind!=VerticalExchangeItemLog::incoming && kind!=VerticalExchangeItemLog::outgoing) ||
       scratch.size()!=(incoming?179U:181U) || !source.definition || craft_end==source.craft_storage.end() ||
       item_name.size()>31 || item_name.find('\0')!=std::string_view::npos ||
       counterparty.character_name.size()>20 || counterparty.character_name.find('\0')!=std::string::npos)
        throw std::invalid_argument("Invalid exchange item log input");
    write_game_log_main(scratch,subject,local_calendar);
    const auto put=[&](std::size_t offset,std::uint64_t value,unsigned size) {for(unsigned i=0;i<size;++i) scratch[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};
    scratch[0]=incoming?5:6;scratch[1]=4;put(74,source.item.uid,8);put(82,source.definition->id,4);
    std::copy(item_name.begin(),item_name.end(),scratch.begin()+86);scratch[86+item_name.size()]=0;
    std::copy(source.item.gems.begin(),source.item.gems.end(),scratch.begin()+118);put(124,source.item.made_at,4);
    std::copy(source.craft_storage.begin(),craft_end+1,scratch.begin()+128);put(149,counterparty.character_id,4);
    std::copy(counterparty.character_name.begin(),counterparty.character_name.end(),scratch.begin()+153);scratch[153+counterparty.character_name.size()]=0;
    scratch[174]=offered_count;std::fill(scratch.begin()+175,scratch.begin()+179,0);
    if(!incoming) {scratch[179]=destination_bag;scratch[180]=destination_slot;}
}

net::Packet vertical_inventory_exchange_sender_packet(const ExchangeItemReference& offer,
    const GroundItem& source,bool source_remains) {
    // CUser::ExchangeItemAdd 0046fdf7..0046fe48.
    return {6,2,offer.bag,offer.slot,source_remains?source.item.type:std::uint8_t{0},
        source_remains?source.item.type_id:std::uint8_t{0},source_remains?source.item.count:std::uint8_t{0}};
}
void vertical_inventory_exchange_receiver_packet(std::span<std::uint8_t> scratch,
    std::uint8_t destination_bag,std::uint8_t destination_slot,std::uint8_t offered_count,
    const GroundItem& source) {
    // CUser::ExchangeItemAdd 0046ff56..0046ffec copies its source craft as
    // a C string; unlike 0a09, bytes after the first NUL are not overwritten.
    const auto end=std::find(source.craft_storage.begin(),source.craft_storage.end(),std::uint8_t{0});
    if(scratch.size()!=36 || end==source.craft_storage.end()) throw std::invalid_argument("Invalid exchange receipt scratch or craft string");
    scratch[0]=5;scratch[1]=2;scratch[2]=destination_bag;scratch[3]=destination_slot;scratch[4]=source.item.type;scratch[5]=source.item.type_id;scratch[6]=offered_count;
    scratch[7]=static_cast<std::uint8_t>(source.item.quality);scratch[8]=static_cast<std::uint8_t>(source.item.quality>>8);
    std::copy(source.item.gems.begin(),source.item.gems.end(),scratch.begin()+9);
    std::copy(source.craft_storage.begin(),end+1,scratch.begin()+15);
}

void vertical_inventory_exchange_db_add_packet(std::span<std::uint8_t> scratch,
    std::uint32_t account,std::uint8_t bag,std::uint8_t slot,const GroundItem& transferred) {
    // CUser::ExchangeItemAdd whole 0046fa10..0046fae1 and split
    // 0046fceb..0046fdb3. Both copy craft only through its first NUL.
    const auto end=std::find(transferred.craft_storage.begin(),transferred.craft_storage.end(),std::uint8_t{0});
    if(scratch.size()!=57 || !transferred.definition || end==transferred.craft_storage.end()) throw std::invalid_argument("Invalid exchange DB add scratch or item");
    const auto put=[&](std::size_t offset,std::uint64_t value,unsigned size) {for(unsigned i=0;i<size;++i) scratch[offset+i]=static_cast<std::uint8_t>(value>>(8*i));};
    scratch[0]=1;scratch[1]=7;put(2,account,4);put(6,transferred.item.uid,8);put(14,transferred.definition->id,4);scratch[18]=bag;scratch[19]=slot;
    scratch[20]=transferred.item.type;scratch[21]=transferred.item.type_id;scratch[22]=transferred.item.count;put(23,transferred.item.quality,2);
    std::copy(transferred.item.gems.begin(),transferred.item.gems.end(),scratch.begin()+25);std::copy(transferred.craft_storage.begin(),end+1,scratch.begin()+31);
    put(52,transferred.item.made_at,4);scratch[56]=transferred.item.make_type.empty()?0:static_cast<std::uint8_t>(transferred.item.make_type[0]);
}
net::Packet vertical_inventory_exchange_db_remove_packet(std::uint32_t account,
    const ExchangeItemReference& offer,std::uint8_t count) {
    // Whole 0046fae1..0046fb1c, split 0046fdb3..0046fdee.
    net::Packet packet{2,7,0,0,0,0,offer.bag,offer.slot,count};for(unsigned i=0;i<4;++i) packet[2+i]=static_cast<std::uint8_t>(account>>(8*i));return packet;
}

VerticalExchangeWholeItemTransferExecutor::VerticalExchangeWholeItemTransferExecutor(VerticalInventoryExchangeWorld& world,
    net::Packet& db,net::Packet& outgoing,net::Packet& receiver,net::Packet& incoming)
    :world_(world),db_(db),outgoing_(outgoing),receiver_(receiver),incoming_(incoming) {}
void VerticalExchangeWholeItemTransferExecutor::validate_whole_exchange_source(Actor& recipient,Actor& source,
    const ExchangeItemReference& offer,GroundItem& item,GroundItemPool& pool) {
    if(&recipient==&source || !offer.bag || offer.bag>=6 || offer.slot>=24 || !item.definition || !pool.is_allocated(item))
        throw std::logic_error("Invalid whole exchange source");
    const auto held=source.retained_inventory_item(offer.bag,offer.slot);
    if(!held || held->item!=&item || held->pool!=&pool ||
       !((item.definition->stack_count==1 && item.item.count==1) || offer.count==item.item.count))
        throw std::logic_error("Whole exchange source changed");
    world_.validate_exchange_source(source.account_id(),offer.bag,offer.slot,item.item.uid,item.item.count);
}
void VerticalExchangeWholeItemTransferExecutor::transfer_whole_exchange_item(Actor& recipient,Actor& source,
    const VerticalExchangeItemSelection& selection) {
    if(db_.size()!=57 || outgoing_.size()!=181 || receiver_.size()!=36 || incoming_.size()!=179 || !selection.whole ||
       selection.offer_index>=source.exchange().items.size() || !selection.source_item || !selection.source_pool ||
       !selection.destination_bag || selection.destination_bag>=6 || selection.destination_slot>=24 ||
       selection.destination_bag>recipient.status().skills.max_bags)
        throw std::invalid_argument("Invalid whole exchange selection");
    const auto offer=source.exchange().items[selection.offer_index];auto& item=*selection.source_item;
    validate_whole_exchange_source(recipient,source,offer,item,*selection.source_pool);
    if(!recipient.inventory_slot_empty(selection.destination_bag,selection.destination_slot))throw std::logic_error("Whole exchange destination changed");
    world_.validate_exchange_destination(recipient.account_id(),selection.destination_bag,selection.destination_slot);
    const auto uid=item.item.uid;const auto count=item.item.count;
    if(!source.transfer_inventory_item(recipient,offer.bag,offer.slot,selection.destination_bag,selection.destination_slot,&item))
        throw std::logic_error("Whole exchange ownership transfer failed");
    vertical_inventory_exchange_db_add_packet(db_,recipient.account_id(),selection.destination_bag,selection.destination_slot,item);
    world_.send_exchange_insertion(db_);
    world_.send_exchange_removal(vertical_inventory_exchange_db_remove_packet(source.account_id(),offer,count),uid,count);
    source.send_self(vertical_inventory_exchange_sender_packet(offer,item,false));
    const auto log=[&](Actor& subject,Actor& other,VerticalExchangeItemLog kind,net::Packet& scratch) {
        const auto header=world_.money_log_subject(subject);const auto time=world_.money_local_calendar();
        const auto name=world_.inventory_item_name(*item.definition);const auto counterparty=world_.money_log_subject(other);
        vertical_inventory_exchange_item_log(scratch,kind,header,counterparty,item,name,offer.count,selection.destination_bag,selection.destination_slot,time);
        world_.send_money_log(scratch);
    };
    log(source,recipient,VerticalExchangeItemLog::outgoing,outgoing_);
    vertical_inventory_exchange_receiver_packet(receiver_,selection.destination_bag,selection.destination_slot,offer.count,item);
    recipient.send_self(receiver_);
    log(recipient,source,VerticalExchangeItemLog::incoming,incoming_);
}

bool vertical_inventory_exchange_split_entry(Actor& recipient,Actor& source,
    const VerticalExchangeItemSelection& selection,GroundItemPool& pool,ItemIds& ids,ItemIdClock& clock,VerticalInventoryExchangeWorld& world,
    net::Packet& db_scratch,net::Packet& outgoing_log,net::Packet& receiver_scratch,net::Packet& incoming_log) {
    // CUser::ExchangeItemAdd 0046fb21..004700de, split branch.
    if(db_scratch.size()!=57 || outgoing_log.size()!=181 || receiver_scratch.size()!=36 || incoming_log.size()!=179)
        throw std::invalid_argument("Invalid split exchange scratch buffers");
    if(selection.offer_index>=source.exchange().items.size()) throw std::invalid_argument("Invalid split exchange selection");
    if(selection.destination_bag>=6 || source.exchange().items[selection.offer_index].bag>=6)
        throw std::out_of_range("Split exchange bag exceeds DB-agent inventory storage");
    world.validate_exchange_destination(recipient.account_id(),selection.destination_bag,selection.destination_slot);
    auto* transferred=vertical_inventory_exchange_split_item(recipient,source,selection,pool,ids,clock);
    const auto& offer=source.exchange().items[selection.offer_index];auto& original=*selection.source_item;
    if(transferred) {
        vertical_inventory_exchange_db_add_packet(db_scratch,recipient.account_id(),selection.destination_bag,selection.destination_slot,*transferred);
        world.send_exchange_insertion(db_scratch);
        const auto original_count=static_cast<std::uint8_t>(original.item.count+offer.count);
        world.send_exchange_removal(vertical_inventory_exchange_db_remove_packet(source.account_id(),offer,offer.count),original.item.uid,original_count);
    }
    source.send_self(vertical_inventory_exchange_sender_packet(offer,original,transferred!=nullptr));
    const auto log=[&](Actor& subject,Actor& other,VerticalExchangeItemLog kind,net::Packet& scratch) {
        const auto header=world.money_log_subject(subject);const auto time=world.money_local_calendar();
        const auto name=world.inventory_item_name(*original.definition);const auto counterparty=world.money_log_subject(other);
        vertical_inventory_exchange_item_log(scratch,kind,header,counterparty,original,name,offer.count,selection.destination_bag,selection.destination_slot,time);
        world.send_money_log(scratch);
    };
    log(source,recipient,VerticalExchangeItemLog::outgoing,outgoing_log);
    vertical_inventory_exchange_receiver_packet(receiver_scratch,selection.destination_bag,selection.destination_slot,offer.count,original);
    recipient.send_self(receiver_scratch);
    log(recipient,source,VerticalExchangeItemLog::incoming,incoming_log);
    return transferred!=nullptr;
}

void vertical_inventory_exchange_transfer_items(Actor& recipient,Actor& source,
    GroundItemPool& pool,ItemIds& ids,ItemIdClock& clock,VerticalInventoryExchangeWorld& world,VerticalExchangeWholeItemTransfer& whole,
    net::Packet& db_scratch,net::Packet& outgoing_log,net::Packet& receiver_scratch,net::Packet& incoming_log) {
    // ps_game.exe CUser::ExchangeItemAdd 0046f950..004700f8.
    if(!vertical_inventory_exchange_db_compatible(recipient,source))
        throw std::out_of_range("Exchange bag exceeds DB-agent inventory storage");
    std::size_t cursor=0;
    while(const auto selection=vertical_inventory_exchange_next_item(recipient,source,cursor)) {
        cursor=selection->offer_index+1;
        if(selection->whole) whole.transfer_whole_exchange_item(recipient,source,*selection);
        else (void)vertical_inventory_exchange_split_entry(recipient,source,*selection,pool,ids,clock,world,db_scratch,outgoing_log,receiver_scratch,incoming_log);
    }
}

VerticalExchangeItemTransferExecutor::VerticalExchangeItemTransferExecutor(GroundItemPool& pool,ItemIds& ids,ItemIdClock& clock,
    VerticalInventoryExchangeWorld& world,VerticalExchangeWholeItemTransfer& whole,net::Packet& db,net::Packet& outgoing,net::Packet& receiver,net::Packet& incoming)
    :pool_(pool),ids_(ids),clock_(clock),world_(world),whole_(whole),db_(db),outgoing_(outgoing),receiver_(receiver),incoming_(incoming) {}
void VerticalExchangeItemTransferExecutor::validate_exchange_sources(Actor& actor,Actor& peer) {
    // Native consistency guard before money; the original checks capacity but
    // does not compare the separate ps_dbAgent replica's UID/count here.
    const auto validate=[&](Actor& recipient,Actor& source) {
        for(const auto& offer:source.exchange().items) {
            if(!offer.bag) continue;
            if(offer.bag>=6 || offer.slot>=24) throw std::out_of_range("Invalid exchange source position");
            const auto held=source.retained_inventory_item(offer.bag,offer.slot);
            if(!held) {
                if(!source.inventory_slot_empty(offer.bag,offer.slot)) throw std::logic_error("Exchange source has no pooled owner");
                continue; // Original ExchangeItemAdd skips stale offers.
            }
            if(!held->item || !held->pool || !held->item->definition) throw std::logic_error("Exchange source owner is incomplete");
            world_.validate_exchange_source(source.account_id(),offer.bag,offer.slot,held->item->item.uid,held->item->item.count);
            const auto count=held->item->item.count;
            if((held->item->definition->stack_count==1 && count==1) || offer.count==count)
                whole_.validate_whole_exchange_source(recipient,source,offer,*held->item,*held->pool);
        }
    };
    validate(actor,peer);validate(peer,actor); // Same direction order as ExchangeItemAdd.
    // Only predict the first actual transfer: subsequent destinations may
    // change as prior whole items leave or arrive. Repeat this under the real
    // replica lock immediately before mutation and again at insertion.
    if(const auto first=vertical_inventory_exchange_next_item(actor,peer,0))
        world_.validate_exchange_destination(actor.account_id(),first->destination_bag,first->destination_slot);
    else if(const auto first=vertical_inventory_exchange_next_item(peer,actor,0))
        world_.validate_exchange_destination(peer.account_id(),first->destination_bag,first->destination_slot);
}
void VerticalExchangeItemTransferExecutor::transfer_exchange_items(Actor& recipient,Actor& source) {
    vertical_inventory_exchange_transfer_items(recipient,source,pool_,ids_,clock_,world_,whole_,db_,outgoing_,receiver_,incoming_);
}

VerticalExchangeSettlementExecutor::VerticalExchangeSettlementExecutor(MoneyWorld& money,VerticalExchangeItemTransfer& items,
    net::Packet& outgoing,net::Packet& incoming,const std::uint32_t& initiator_field)
    :money_(money),items_(items),outgoing_(outgoing),incoming_(incoming),initiator_field_(initiator_field) {}

void VerticalExchangeSettlementExecutor::validate_before_ready(Actor& actor,Actor& peer) {
    if(!vertical_inventory_exchange_db_compatible(actor,peer)) throw std::out_of_range("Exchange bag exceeds DB-agent inventory storage");
    if(outgoing_.size()!=126 || incoming_.size()!=111) throw std::invalid_argument("Invalid exchange money scratch buffers");
    // Mirror the original ordered preflight without its failure sends. An
    // original rejection is left to CUser::Exchange after readiness sends.
    if(!vertical_inventory_exchange_money_valid(peer,actor,peer.exchange().money) ||
       !vertical_inventory_exchange_money_valid(actor,peer,actor.exchange().money) ||
       !vertical_inventory_exchange_items_fit(actor,peer.exchange()) ||
       !vertical_inventory_exchange_items_fit(peer,actor.exchange())) return;
    items_.validate_exchange_sources(actor,peer);
}

void VerticalExchangeSettlementExecutor::settle_exchange(Actor& actor,Actor& peer) {
    // ps_game.exe CUser::Exchange 0046f4a0..0046f767, with the native DB
    // domain check preceding every original observable operation.
    if(!vertical_inventory_exchange_db_compatible(actor,peer)) throw std::out_of_range("Exchange bag exceeds DB-agent inventory storage");
    if(outgoing_.size()!=126 || incoming_.size()!=111) throw std::invalid_argument("Invalid exchange money scratch buffers");
    if(vertical_inventory_exchange_preflight(actor,peer)!=VerticalExchangePreflight::transfer_required) return;
    items_.validate_exchange_sources(actor,peer);
    vertical_inventory_exchange_money_phase(actor,peer,money_,outgoing_,incoming_,initiator_field_);
    vertical_inventory_exchange_item_phase(actor,peer,items_);
}

VerticalExchangeRequest vertical_inventory_exchange_ready(Actor& actor,std::span<const std::uint8_t> packet,
    VerticalInventoryActions& actions,VerticalExchangeSettlement& settlement) {
    if(packet.size()<2) return VerticalExchangeRequest::malformed;
    if(packet[0]!=5 || packet[1]!=0x0a) return VerticalExchangeRequest::unhandled;
    if(packet.size()<3) return VerticalExchangeRequest::malformed;
    if(packet[2]!=0) return VerticalExchangeRequest::unhandled;
    actions.end_shop(actor);
    if(packet[0]!=5 || packet[1]!=0x0a || packet[2]!=0) throw std::logic_error("Exchange callback changed ready request");
    auto& state=actor.exchange();if(state.mode!=3 || !state.peer) return VerticalExchangeRequest::ignored;
    auto* peer=state.peer;state.ready=1;
    actor.send_self({5,0x0a,1,0});peer->send_self({5,0x0a,2,0});
    if(peer->exchange().ready) {
        state.mode=4;peer->exchange().mode=4;
        settlement.settle_exchange(actor,*peer);
        state={};peer->exchange()={};
    }
    return VerticalExchangeRequest::updated;
}

VerticalExchangeRequest vertical_inventory_exchange_guarded_ready(Actor& actor,std::span<const std::uint8_t> packet,
    VerticalInventoryActions& actions,VerticalExchangeSettlement& settlement) {
    // Native safety boundary for ps_dbAgent.exe CUser::PacketUserItem 00408480;
    // the original PacketExchange 0046ec90 has no corresponding bag check.
    if(packet.size()<3 || packet[0]!=5 || packet[1]!=0x0a || packet[2]!=0)
        return vertical_inventory_exchange_ready(actor,packet,actions,settlement);
    const auto check=[&](Actor& value) {
        const auto& state=value.exchange();
        if(state.mode==3 && state.peer && !vertical_inventory_exchange_db_compatible(value,*state.peer))
            throw std::out_of_range("Exchange bag exceeds DB-agent inventory storage");
        if(state.mode==3 && state.peer && state.peer->exchange().ready)
            settlement.validate_before_ready(value,*state.peer);
    };
    check(actor);
    struct GuardedActions final:VerticalInventoryActions {
        VerticalInventoryActions& owner;decltype(check)& validate;
        GuardedActions(VerticalInventoryActions& a,decltype(check)& f):owner(a),validate(f) {}
        void cancel_action_exchange(Actor& value) override {owner.cancel_action_exchange(value);}
        void end_shop(Actor& value) override {owner.end_shop(value);validate(value);}
        bool pickup_checks() override {return owner.pickup_checks();}
    } guarded(actions,check);
    return vertical_inventory_exchange_ready(actor,packet,guarded,settlement);
}

VerticalExchangeRequest vertical_inventory_exchange_invite(Actor& actor,std::span<const std::uint8_t> packet,
    ZoneUsers& zone,std::uint32_t& timeout,VerticalInventoryActions& actions) {
    if(packet.size()<2) return VerticalExchangeRequest::malformed;
    if(packet[0]!=1 || packet[1]!=0x0a) return VerticalExchangeRequest::unhandled;
    if(packet.size()<6) return VerticalExchangeRequest::malformed;
    actions.end_shop(actor);
    if(packet[0]!=1 || packet[1]!=0x0a) throw std::logic_error("Exchange callback changed opcode");
    const auto target_id=[&] {std::uint32_t value=0;for(unsigned i=0;i<4;++i) value|=std::uint32_t(packet[2+i])<<(8*i);return value;};
    const auto reply=[&] {net::Packet notice{3,0x0a,0,0,0,0,2};const auto value=target_id();for(unsigned i=0;i<4;++i) notice[2+i]=static_cast<std::uint8_t>(value>>(8*i));actor.send_self(notice);};
    auto& state=actor.exchange();
    if(state.mode!=0) {reply();return VerticalExchangeRequest::ignored;}
    auto* peer=zone.find_actor(target_id());if(!peer) return VerticalExchangeRequest::ignored;
    if(peer->exchange().mode!=0 || peer->appearance().identity.country!=actor.appearance().identity.country) {reply();return VerticalExchangeRequest::ignored;}
    state.peer=peer;peer->exchange().peer=&actor;state.mode=1;peer->exchange().mode=2;
    net::Packet notice{1,0x0a,0,0,0,0};const auto id=actor.id();for(unsigned i=0;i<4;++i) notice[2+i]=static_cast<std::uint8_t>(id>>(8*i));peer->send_self(notice);
    timeout=actor.tick_ms()+30000U;return VerticalExchangeRequest::updated;
}

bool vertical_inventory_exchange_expire(Actor& actor,std::uint32_t now,std::uint32_t& timeout,
    std::uint32_t* peer_timeout) {
    // ps_game.exe CUser::UpdateTimeover 0048e310..0048e39c uses unsigned
    // deadline comparison and resets the two original invitation modes only.
    if(!timeout || timeout>now) return false;
    auto& state=actor.exchange();const auto mode=state.mode;if(mode!=1 && mode!=2) return false;
    auto* peer=state.peer;
    if(peer && peer->exchange().mode==1) {
        if(mode==1) {
            if(!peer_timeout || (peer==&actor && peer_timeout!=&timeout)) throw std::invalid_argument("Exchange expiry requires the retained peer timer");
            peer->exchange()={};*peer_timeout=0;
        } else peer->exchange()={};
    }
    state={};if(mode==2) timeout=0;return true;
}

VerticalExchangeRequest vertical_inventory_exchange_request(Actor& actor,std::span<const std::uint8_t> packet,
    VerticalInventoryActions& actions) {
    if(packet.size()<2) return VerticalExchangeRequest::malformed;
    if(packet[1]!=0x0a || (packet[0]!=0x02 && packet[0]!=0x05 && packet[0]!=0x06 && packet[0]!=0x07 && packet[0]!=0x08)) return VerticalExchangeRequest::unhandled;
    const auto opcode=packet[0];
    if(packet.size()<((opcode==2 || opcode==5 || opcode==7)?3U:6U)) return VerticalExchangeRequest::malformed;
    // Command 0 needs the real CUser::Exchange settlement owner. Leave it
    // unhandled before the shop callback so the eventual owner can route it.
    if(opcode==5 && packet[2]==0) return VerticalExchangeRequest::unhandled;
    actions.end_shop(actor);
    if(packet[0]!=opcode || packet[1]!=0x0a) throw std::logic_error("Exchange callback changed opcode");
    if(opcode==5 && packet[2]==0) throw std::logic_error("Exchange callback changed control to settlement");
    auto& state=actor.exchange();
    if(opcode==2) {
        // PacketExchange 0046edcf..0046eea0; CExchange::Reset 00449660.
        if(state.mode!=2) return VerticalExchangeRequest::ignored;
        auto* peer=state.peer;
        if(!peer || peer->exchange().mode!=1) {state={};return VerticalExchangeRequest::updated;}
        const auto reply=[&](std::uint32_t id,std::uint8_t result) {
            net::Packet notice{3,0x0a,0,0,0,0,result};for(unsigned i=0;i<4;++i) notice[2+i]=static_cast<std::uint8_t>(id>>(8*i));return notice;
        };
        if(packet[2]) {state={};peer->exchange()={};peer->send_self(reply(peer->id(),1));}
        else {state.mode=3;peer->exchange().mode=3;actor.send_self(reply(peer->id(),0));peer->send_self(reply(actor.id(),0));}
        return VerticalExchangeRequest::updated;
    }
    if(state.mode!=3) return VerticalExchangeRequest::ignored;
    if(opcode==5) {
        auto* peer=state.peer;if(!peer) return VerticalExchangeRequest::ignored;
        if(packet[2]==1) {actor.cancel_exchange_ready(*peer,false);return VerticalExchangeRequest::updated;}
        if(packet[2]!=2) return VerticalExchangeRequest::ignored;
        state={};peer->exchange()={};actor.send_self({4,0x0a,2});peer->send_self({4,0x0a,3});
        return VerticalExchangeRequest::updated;
    }
    if(opcode==6) {
        const auto bag=packet[2],slot=packet[3],count=packet[4],index=packet[5];
        if(!count || index>=state.items.size() || state.items[index].bag || !bag ||
           static_cast<std::int32_t>(bag)>actor.status().skills.max_bags || slot>=24) return VerticalExchangeRequest::ignored;
        const auto held=actor.retained_inventory_item(bag,slot);
        if(!held || !held->item->definition || held->item->definition->real_type==27 || count>held->item->item.count) return VerticalExchangeRequest::ignored;
        if(std::any_of(state.items.begin(),state.items.end(),[&](const auto& offered){return offered.bag==bag && offered.slot==slot;})) return VerticalExchangeRequest::ignored;
        if(!state.peer) return VerticalExchangeRequest::ignored;
        auto* peer=state.peer;actor.cancel_exchange_ready(*peer,false);
        if(packet[5]>=state.items.size()) throw std::out_of_range("Reloaded exchange offer index exceeds storage");
        state.items[packet[5]]={packet[2],packet[3],packet[4]};
        actor.send_self({6,0x0a,packet[2],packet[3],packet[4],packet[5]});
        const auto current=actor.retained_inventory_item(packet[2],packet[3]);
        if(!current || !current->item->definition) throw std::logic_error("Exchange callback removed reloaded inventory item");
        const auto& item=*current->item;
        net::Packet notice(35,0);notice[0]=9;notice[1]=0x0a;notice[2]=packet[5];notice[3]=item.item.type;notice[4]=item.item.type_id;notice[5]=packet[4];
        notice[6]=static_cast<std::uint8_t>(item.item.quality);notice[7]=static_cast<std::uint8_t>(item.item.quality>>8);
        std::copy(item.item.gems.begin(),item.item.gems.end(),notice.begin()+8);std::copy(item.craft_storage.begin(),item.craft_storage.end(),notice.begin()+14);peer->send_self(notice);
    } else if(opcode==7) {
        const auto index=packet[2];
        if(index>=state.items.size() || !state.items[index].bag || !state.peer) return VerticalExchangeRequest::ignored;
        auto* peer=state.peer;
        actor.cancel_exchange_ready(*peer,false);
        const auto current=packet[2];
        if(current>=state.items.size()) throw std::out_of_range("Reloaded exchange offer index exceeds storage");
        state.items[current].bag=0;
        net::Packet notice{7,0x0a,1,current};actor.send_self(notice);notice[2]=2;peer->send_self(notice);
    } else {
        const auto read=[&] {std::uint32_t value=0;for(unsigned i=0;i<4;++i) value|=std::uint32_t(packet[2+i])<<(8*i);return value;};
        if(read()>actor.money() || !state.peer) return VerticalExchangeRequest::ignored;
        auto* peer=state.peer;
        actor.cancel_exchange_ready(*peer,false);
        state.money=read();const auto value=read();
        net::Packet notice{8,0x0a,1,0,0,0,0};for(unsigned i=0;i<4;++i) notice[3+i]=static_cast<std::uint8_t>(value>>(8*i));
        actor.send_self(notice);notice[2]=2;peer->send_self(notice);
    }
    return VerticalExchangeRequest::updated;
}

}
