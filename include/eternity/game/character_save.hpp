#pragma once
#include "eternity/game/character_core_save.hpp"
#include "eternity/game/character_recovery.hpp"
#include "eternity/game/inventory_save.hpp"
#include "eternity/game/interface_save.hpp"
#include "eternity/game/quest_save.hpp"
#include "eternity/game/skill_save.hpp"

namespace eternity::game {
// Complete DBCharacter::SaveCharacter orchestration. begin/commit/rollback and
// recovery results are deliberately ignored by the original. Callbacks other
// than phases must not throw; phases may throw runtime_error for a fatal save.
struct CharacterSavePort {
    std::function<std::int32_t()> growth;
    std::function<bool()> acquire,begin,commit,rollback,timeout;
    std::function<void()> lock,unlock,recovery,acknowledge,release;
    std::function<void(bool save_quick_slots)> phases;
    std::function<void(std::string_view)> diagnostic;
};
bool execute_character_save(bool user_present,bool save_quick_slots,bool recover_on_failure,bool& timeout_seen,const CharacterSavePort&);
struct UserCharacterSavePort {
    std::function<bool()> purchases_pending,save_purchases;
    std::function<bool(bool quick,bool recovery,bool& timeout_seen)> attempt;
    std::function<void()> save_friends,save_blocks,queue_exit,exit;
};
struct UserCharacterSaveResult {bool legacy_success=false,timeout_seen=false;std::uint8_t attempts=0;};
// CUser::SaveCharacter / SaveCharacterTick. Social/action callbacks must not throw.
UserCharacterSaveResult execute_user_character_save(bool periodic,bool quick,bool in_game,const UserCharacterSavePort&);
// Exit variants ignore the purchase result. Normal exit defers on a timeout;
// shutdown starts with the sticky timeout flag set and always leaves the user.
struct UserCharacterExitPort : UserCharacterSavePort {std::function<void()> save_leave;};
UserCharacterSaveResult execute_user_character_exit(bool shutdown,const UserCharacterExitPort&);
// CUser::UpdateDBSave uses unsigned absolute ticks, including DWORD wrap.
bool update_character_save_tick(std::uint32_t status,std::uint32_t now,std::uint32_t& next,std::uint32_t& last,const std::function<void(std::uint32_t)>& post);
struct CharacterSaveCallbacks {std::function<void()> save_friends,save_blocks,queue_exit,exit;};
class CharacterSaveStore {
public:
    // Both connections are dedicated and start outside a transaction. All cache
    // access uses the same external recursive mutex. Recovery writer is required;
    // an unavailable file still has the original marker-clearing behavior.
    CharacterSaveStore(storage::SqlServer& main,storage::SqlServer& auxiliary,std::uint8_t server,std::recursive_mutex& cache_mutex,CharacterRecoveryFile&,std::string catalog="PS_GameData");
    bool uses_cache_mutex(const std::recursive_mutex& mutex) const noexcept {return &cache_mutex_==&mutex;}
    bool attempt(CharacterReplica&,bool quick,bool recovery,bool& timeout_seen,std::string_view username,const std::function<void(std::string_view)>& diagnostic={});
    UserCharacterSaveResult save(CharacterReplica&,bool periodic,bool quick,bool in_game,std::string_view username,const CharacterSaveCallbacks&,const std::function<void(std::string_view)>& diagnostic={});
    UserCharacterSaveResult save_exit(CharacterReplica&,bool shutdown,std::string_view username,const CharacterSaveCallbacks&,const std::function<void(std::string_view)>& diagnostic={});
    // Standalone autocommit operation after exit saving; its result never blocks exit.
    bool save_leave(std::uint32_t character_id,const std::function<void(std::string_view)>& diagnostic={});
private:
    void phases(CharacterReplica&,bool quick,const std::function<void(std::string_view)>&);
    void observe(std::string_view);
    storage::SqlServer& main_;storage::SqlServer& auxiliary_;std::uint8_t server_;
    std::recursive_mutex& cache_mutex_;CharacterRecoveryFile& recovery_;
    std::string prefix_;
    CharacterCoreStore core_;PointItemStore receipts_;InventoryStore inventory_;SkillStore skills_;QuestStore quests_;InterfaceStore interface_;MarketStore market_;GrowthStore growth_;PurchaseStore purchases_;
};
}
