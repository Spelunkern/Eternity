#include "eternity/game/character_save.hpp"

namespace eternity::game {
bool execute_character_save(bool present,bool quick,bool recover,bool& timeout_seen,const CharacterSavePort& port) {
    if(!present) return false;
    if(!port.growth || !port.acquire || !port.begin || !port.commit || !port.rollback || !port.timeout || !port.lock || !port.unlock || !port.recovery || !port.acknowledge || !port.release || !port.phases) throw std::invalid_argument("Incomplete character save ports");
    const auto log=[&](std::string_view message){if(port.diagnostic) port.diagnostic(message);};
    if(port.growth()!=0) log("Maximum grow save failed before character saving");
    if(!port.acquire()) {log("Character DB allocation failed");if(recover) {port.lock();port.recovery();port.unlock();}return false;}
    port.lock();(void)port.begin();
    try {port.phases(quick);}
    catch(const std::runtime_error& error) {
        log(error.what());const bool first=!timeout_seen;if(first && port.timeout()) timeout_seen=true;
        (void)port.rollback();if(recover && (!first || !timeout_seen)) port.recovery();port.unlock();port.release();return false;
    }
    catch(...) {(void)port.rollback();port.unlock();port.release();throw;}
    (void)port.commit();port.acknowledge();port.unlock();port.release();return true;
}
UserCharacterSaveResult execute_user_character_save(bool periodic,bool quick,bool in_game,const UserCharacterSavePort& port) {
    if(!port.purchases_pending || !port.save_purchases || !port.attempt || !port.save_friends || !port.save_blocks || !port.queue_exit || !port.exit) throw std::invalid_argument("Incomplete user character save ports");
    if(port.purchases_pending() && !port.save_purchases()) {if(in_game) port.queue_exit();else port.exit();}
    UserCharacterSaveResult result;quick=quick && !periodic;
    for(unsigned int i=0;i<2;++i) {++result.attempts;if(port.attempt(quick,i==1,result.timeout_seen)) {port.save_friends();port.save_blocks();result.legacy_success=true;return result;}}
    if(periodic && result.timeout_seen) result.legacy_success=true;
    else if(!periodic && result.timeout_seen && in_game) port.queue_exit();
    else port.exit();
    return result;
}
CharacterSaveStore::CharacterSaveStore(storage::SqlServer& main,storage::SqlServer& auxiliary,std::uint8_t server,std::recursive_mutex& mutex,CharacterRecoveryFile& recovery,std::string catalog):main_(main),auxiliary_(auxiliary),server_(server),cache_mutex_(mutex),recovery_(recovery),core_(main,catalog),receipts_(main,server,catalog),inventory_(main,server,catalog),skills_(main,catalog),quests_(main,catalog),interface_(main,catalog),market_(main,catalog),growth_(auxiliary,server,catalog),purchases_(main,auxiliary,std::move(catalog)) {}
void CharacterSaveStore::observe(std::string_view message) {main_.observe_query_error(message);}
void CharacterSaveStore::phases(CharacterReplica& cache,bool quick,const std::function<void(std::string_view)>& diagnostic) {
    const auto log=[&](std::string_view message){observe(message);if(diagnostic) diagnostic(message);};
    main_.legacy_character_save_=true;
    try {
        const auto id=cache.core().character.id;core_.save_in_transaction(cache);receipts_.save_in_transaction(cache.points(),id);inventory_.save_in_transaction(cache.inventory(),id);skills_.save_in_transaction(cache.skills(),id);quests_.save_in_transaction(cache.quests(),id);
        try {(void)interface_.save_in_transaction(cache.interface(),id,quick,log);}catch(const std::runtime_error& error){log(error.what());throw;}
        (void)market_.save_in_transaction(cache.market(),id,log);
    }catch(const std::runtime_error& error) {observe(error.what());main_.legacy_character_save_=false;throw;}
    catch(...) {main_.legacy_character_save_=false;throw;}
    main_.legacy_character_save_=false;
}
bool CharacterSaveStore::attempt(CharacterReplica& cache,bool quick,bool recover,bool& timeout_seen,std::string_view username,const std::function<void(std::string_view)>& diagnostic) {
    if(main_.character_write_context_active() || auxiliary_.character_write_context_active()) throw std::logic_error("Complete character save must own its transaction");
    const auto transaction=[&](auto operation){if(operation!=&storage::SqlServer::begin && !main_.transaction_active()) return false;try{(main_.*operation)();return true;}catch(const std::runtime_error&){return false;}};
    CharacterSavePort port;port.growth=[&](){std::lock_guard lock(cache_mutex_);const auto core=cache.core();return growth_.save(cache.growth(),core.user_id,core.character.grow,core.character.level,diagnostic).legacy_code;};port.acquire=[](){return true;};port.begin=[&](){return transaction(&storage::SqlServer::begin);};port.commit=[&](){return transaction(&storage::SqlServer::commit);};port.rollback=[&](){return transaction(&storage::SqlServer::rollback);};port.timeout=[&](){return main_.last_query_sqlstate()=="HYT00";};
    port.lock=[&](){cache_mutex_.lock();};port.unlock=[&](){cache_mutex_.unlock();};port.phases=[&](bool save_quick){phases(cache,save_quick,diagnostic);};port.recovery=[&](){(void)recovery_.append(cache,server_,username);};
    port.acknowledge=[&](){cache.core_save_success();cache.inventory().inventory_save_success();cache.skills().save_success();cache.quests().save_success();cache.interface().save_success();cache.points().point_save_success();};port.release=[&](){if(main_.transaction_active()) (void)transaction(&storage::SqlServer::rollback);};port.diagnostic=diagnostic;
    return execute_character_save(true,quick,recover,timeout_seen,port);
}
UserCharacterSaveResult CharacterSaveStore::save(CharacterReplica& cache,bool periodic,bool quick,bool in_game,std::string_view username,const CharacterSaveCallbacks& callbacks,const std::function<void(std::string_view)>& diagnostic) {
    UserCharacterSavePort port;port.purchases_pending=[&](){std::lock_guard lock(cache_mutex_);return cache.purchases().pending_count()!=0;};port.save_purchases=[&](){std::lock_guard lock(cache_mutex_);return purchases_.save(cache.purchases(),cache.core().character.id,diagnostic).legacy_success;};port.attempt=[&](bool slots,bool fallback,bool& timeout){return attempt(cache,slots,fallback,timeout,username,diagnostic);};port.save_friends=callbacks.save_friends;port.save_blocks=callbacks.save_blocks;port.queue_exit=callbacks.queue_exit;port.exit=callbacks.exit;return execute_user_character_save(periodic,quick,in_game,port);
}
}
