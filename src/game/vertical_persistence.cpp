#include "eternity/game/vertical_persistence.hpp"
#include <stdexcept>
#include <algorithm>

namespace eternity::game {
std::unique_ptr<CharacterReplica> vertical_persistence_restore(PlayerSnapshot& snapshot, std::uint8_t max_grow, InventoryReplicaPool& pool, SkillReplicaPool* skills, QuestReplicaPool* quests, PointItemPool* points) {
    auto cache=std::make_unique<CharacterReplica>(snapshot.core,pool,skills,quests,points);
    // Allocation happens in SQL row order before duplicate placement. Retain that
    // order even though the snapshot is laid out by resolved slot.
    const auto restore=[&](const std::vector<PlayerItem>& items,const std::vector<PlayerItem>& overflow,bool bank) {
        struct Row {const PlayerItem* item;bool overflow;};
        std::vector<Row> ordered;ordered.reserve(items.size()+overflow.size());
        for(const auto& item:items) ordered.push_back({&item,false});
        for(const auto& item:overflow) ordered.push_back({&item,true});
        std::stable_sort(ordered.begin(),ordered.end(),[](const auto& a,const auto& b){return a.item->source_order<b.item->source_order;});
        std::vector<std::unique_ptr<CachedInventoryItem>> pending;pending.reserve(overflow.size());
        for(const auto& row:ordered) {
            if(row.overflow) {
                auto allocated=pool.allocate();if(!allocated) throw std::runtime_error("Inventory allocation failed");
                if(allocated->save_type==0) allocated->save_type=2;
                allocated->item=*row.item;
                if(row.item->craft.size()>20 || row.item->craft.find('\0')!=std::string::npos) throw std::invalid_argument("Invalid overflow item craft");
                std::copy(row.item->craft.begin(),row.item->craft.end(),allocated->craft_storage.begin());
                allocated->craft_storage[row.item->craft.size()]=0;allocated->craft_storage[20]=0;
                pending.push_back(std::move(allocated));
            } else {
                const bool loaded=row.item->relocated ? cache->inventory().load_relocated(*row.item,bank) : cache->inventory().load(*row.item,bank);
                if(!loaded) throw std::runtime_error("Inventory allocation failed");
            }
        }
        // Duplicate-list cleanup follows the fetch loop, in FIFO order.
        for(auto& item:pending) pool.release(std::move(item));
    };
    const auto raw_restore=[&](bool bank) {
        const auto found=snapshot.catalogs.find(bank?"Bank":"Items");
        if(found==snapshot.catalogs.end()) {restore(bank?snapshot.bank:snapshot.items,bank?snapshot.overflow_bank:snapshot.overflow_items,bank);return;}
        std::array<bool,240> occupied{};std::vector<std::unique_ptr<CachedInventoryItem>> duplicates;
        std::size_t index=0;
        for(const auto& row:found->second.rows) {
            auto value=decode_player_item(row,bank);value.source_order=index++;
            if((bank && value.slot>=240) || (!bank && (value.bag>=6 || value.slot>=24)))continue;
            const auto slot=bank?value.slot:value.bag*24+value.slot;
            auto item=pool.allocate();if(!item)continue;
            const bool duplicate=occupied[slot];
            if(duplicate) {if(item->save_type==0)item->save_type=2;} else item->save_type=0;
            item->item=std::move(value);std::copy(item->item.craft.begin(),item->item.craft.end(),item->craft_storage.begin());
            item->craft_storage[item->item.craft.size()]=0;item->craft_storage[20]=0;
            if(duplicate)duplicates.push_back(std::move(item));
            else {occupied[slot]=true;cache->inventory().adopt_loaded(std::move(item),bank);}
        }
        for(auto& item:duplicates) {
            const auto first=bank?0U:24U,last=bank?6U:144U;auto slot=first;
            while(slot<last && occupied[slot])++slot;
            if(slot==last) {pool.release(std::move(item));continue;}
            occupied[slot]=true;item->item.relocated=true;item->item.bag=bank?0:static_cast<std::uint8_t>(slot/24);item->item.slot=static_cast<std::uint16_t>(bank?slot:slot%24);
            cache->inventory().adopt_loaded(std::move(item),bank);
        }
        auto& visible=bank?snapshot.bank:snapshot.items;visible.clear();
        for(const auto& cached:bank?cache->inventory().bank():cache->inventory().inventory())visible.push_back(cached.item);
        (bank?snapshot.overflow_bank:snapshot.overflow_items).clear();
    };
    raw_restore(false);raw_restore(true);
    if(!cache->skills().load(snapshot.skills,snapshot.effects)) throw std::runtime_error("Skill loading failed");
    // DBCharacter::LoadCharacter 00420610 skips quest rows on allocation failure.
    for(const auto& quest:snapshot.quests) (void)cache->quests().load(quest);
    for(const auto& quest:snapshot.finished_quests) (void)cache->quests().load_finished(quest);
    if(!cache->interface().load(snapshot) || !cache->market().load(snapshot) || !cache->points().load(snapshot) || !cache->purchases().load(snapshot)) throw std::runtime_error("Character auxiliary loading failed");
    cache->growth().load(max_grow);
    return cache;
}
std::unique_ptr<CharacterReplica> vertical_persistence_restore_with_products(PlayerSnapshot& snapshot, PointItemStore& products, std::uint8_t max_grow, InventoryReplicaPool& pool, SkillReplicaPool* skills, QuestReplicaPool* quests, PointItemPool* points) {
    auto replica=vertical_persistence_restore(snapshot,max_grow,pool,skills,quests,points);
    products.load_products(replica->points());
    return replica;
}
std::optional<VerticalPersistenceSelection> vertical_persistence_select(Players& players, PointItemStore& products, std::uint32_t user, std::uint32_t character, const std::string& username, std::uint8_t max_grow, InventoryReplicaPool& pool, SkillReplicaPool* skills, QuestReplicaPool* quests, PointItemPool* points) {
    auto snapshot=players.load(user,character,username);
    if(!snapshot) return std::nullopt;
    auto replica=vertical_persistence_restore_with_products(*snapshot,products,max_grow,pool,skills,quests,points);
    return VerticalPersistenceSelection{std::move(*snapshot),std::move(replica)};
}
}
