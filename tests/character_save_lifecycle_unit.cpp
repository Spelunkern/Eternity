#include "eternity/game/character_save_lifecycle.hpp"
#include "eternity/game/vertical_save_job_bridge.hpp"
#include <deque>
#include <iostream>
#include <stdexcept>
#include <vector>
template class eternity::game::CharacterSaveLifecycle<eternity::game::CharacterSaveStore>;
template class eternity::game::VerticalSaveJobBridge<eternity::game::CharacterSaveStore>;
using namespace eternity;using namespace eternity::game;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Store {
 bool defer=true,fail=false;unsigned exits=0,saves=0;
 UserCharacterSaveResult save(CharacterReplica&,bool,bool,bool,std::string_view,const CharacterSaveCallbacks&,const std::function<void(std::string_view)>&){++saves;return {.legacy_success=true};}
 UserCharacterSaveResult save_exit(CharacterReplica&,bool,std::string_view,const CharacterSaveCallbacks& cb,const std::function<void(std::string_view)>&){++exits;if(fail){fail=false;throw std::runtime_error("fixture save failure");}if(defer){defer=false;cb.queue_exit();}else cb.exit();return {};}
};
int main(){try {
 PlayerCore core{};core.user_id=42;InventoryReplicaPool pool;CharacterReplica replica(core,pool);std::recursive_mutex mutex;
 UserLifecycleState state{42,2,0,true};Store store;std::deque<std::uint32_t> jobs;std::vector<net::Packet> sent;unsigned leaves=0;
 CharacterSaveLifecyclePorts ports;ports.post=[&](auto job){jobs.push_back(job);};ports.send=[&](auto p){sent.push_back(std::move(p));};ports.leave=[&]{++leaves;};ports.reset=[]{};ports.roster=[](bool){};ports.save_friends=[]{};ports.save_blocks=[]{};
 bool rejected=false;auto incomplete=ports;incomplete.save_blocks={};try{CharacterSaveLifecycle<Store> invalid(state,replica,store,mutex,"account",incomplete);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"missing social callback accepted");
 UserLifecycleState foreign{43,2,0,true};rejected=false;try{CharacterSaveLifecycle<Store> invalid(foreign,replica,store,mutex,"account",ports);}catch(const std::invalid_argument&){rejected=true;}check(rejected,"mismatched owner accepted");
 CharacterSaveLifecycle<Store> bridge(state,replica,store,mutex,"account",ports);
 check(!bridge.disconnect(vertical_database_disconnect_packet(43)),"foreign account accepted");check(jobs.empty(),"foreign account scheduled job");
 check(bridge.disconnect(vertical_database_disconnect_packet(42)),"disconnect rejected");check(jobs.size()==1 && jobs.front()==9,"exit not queued");jobs.pop_front();
 check(bridge.dispatch(9),"exit job rejected");check(store.exits==1 && leaves==0 && sent.empty() && jobs.size()==1,"deferred exit ordering");jobs.pop_front();
 check(bridge.dispatch(9),"retry rejected");check(store.exits==2 && leaves==1 && sent.size()==2,"exit completion");
 check(sent[0]==net::Packet({2,3,42,0,0,0}) && sent[1]==net::Packet({3,3,42,0,0,0}),"exit packets");

 Store collected_store;collected_store.fail=true;UserLifecycleState collected_state{42,2,0,true};VerticalSaveJobBridge<Store> collected(collected_state,replica,collected_store,mutex,"account",ports);
 std::vector<std::uint32_t> retries;const auto retry=[&](std::uint32_t job){retries.push_back(job);};bool failed=false;
 try{(void)collected.dispatch_collect(9,retry);}catch(const std::runtime_error&){failed=true;}check(failed && retries.empty(),"save exception was lost");
 auto deferred=collected.dispatch_collect(9,retry);check(deferred.empty() && retries==std::vector<std::uint32_t>{9} && sent.size()==2,"retry escaped collection after failure");
 auto finished=collected.dispatch_collect(9,retry);check(finished==std::vector<net::Packet>({{2,3,42,0,0,0},{3,3,42,0,0,0}}) && sent.size()==2 && leaves==2,"save reply collection or context reset failed");
 check(!bridge.dispatch(999),"unknown job accepted");std::cout << "character save lifecycle ok\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
