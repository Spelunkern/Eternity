#include "eternity/game/serialized_world_application.hpp"
#include "eternity/game/roster_world_handoff.hpp"
#include "eternity/game/vertical_async_selection.hpp"
#include <asio/thread_pool.hpp>
#include <asio/io_context.hpp>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
using namespace eternity;
using Strand=game::SerializedWorldApplication::Strand;
static void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct State {Strand strand;unsigned constructed=0,opened=0,received=0,closed=0,destroyed=0;bool overlap=false;explicit State(Strand s):strand(s){}};
struct App final:net::WorldApplication {
 State& state;explicit App(State& s):state(s){check(state.strand.running_in_this_thread(),"factory outside world strand");++state.constructed;}
 ~App() override {if(!state.strand.running_in_this_thread())state.overlap=true;++state.destroyed;}
 void open() override {check(state.strand.running_in_this_thread(),"open outside world strand");++state.opened;}
 void receive(const net::Packet& p) override {check(state.strand.running_in_this_thread(),"receive outside world strand");++state.received;if(p==net::Packet{9})throw std::runtime_error("fixture failure");}
 void close() override {check(state.strand.running_in_this_thread(),"close outside world strand");++state.closed;}
};
struct Selected final:net::WorldApplication {
 unsigned& opens;unsigned& receives;unsigned& closes;std::optional<std::uint32_t>& reason;bool fail;
 Selected(unsigned& o,unsigned& r,unsigned& c,std::optional<std::uint32_t>& why,bool f):opens(o),receives(r),closes(c),reason(why),fail(f){}
 void open() override{++opens;if(fail)throw std::runtime_error("fixture open failure");}
 void receive(const net::Packet&) override{++receives;}
 void close() override{++closes;}void close_with_reason(std::optional<std::uint32_t> r) override{reason=r;++closes;}
};
int main(){try {
 asio::io_context io;auto guard=asio::make_work_guard(io);Strand strand(asio::make_strand(io));State state(strand);
 std::jthread world([&]{io.run();});auto factory=game::serialized_world_factory(strand,[&](const session::LoginSession&,net::WorldChannel){return std::make_unique<App>(state);});
 std::vector<std::jthread> workers;std::atomic_uint failures=0;
 for(unsigned i=0;i<4;++i)workers.emplace_back([&]{try {auto app=factory({},{});app->open();for(int n=0;n<40;++n)app->receive({1});try{app->receive({9});++failures;}catch(const std::runtime_error&){}app->close();}catch(...){++failures;}});
 for(auto& worker:workers)worker.join();guard.reset();world.join();
 check(!failures && !state.overlap && state.constructed==4 && state.opened==4 && state.received==164 && state.closed==4 && state.destroyed==4,"serialized application lifecycle failed");
 unsigned opens=0,receives=0,closes=0;bool fail=false;std::optional<std::uint32_t> reason;std::vector<net::Packet> sent;
 net::WorldChannel channel;channel.send=[&](net::Packet p){sent.push_back(std::move(p));};channel.disconnect=[]{};
 auto roster_factory=game::roster_world_handoff_factory([](const session::LoginSession&){game::CharacterRoster roster;roster.country=0;return roster;},
  [](const session::LoginSession&,game::CharacterRoster& roster,const net::Packet&){roster.country=1;return game::CharacterReply{};},
  [&](const session::LoginSession& account,net::WorldChannel,game::CharacterRoster roster){check(account.user_id==11 && roster.country==1,"handoff lost account or updated roster");return std::make_unique<Selected>(opens,receives,closes,reason,fail);});
 session::LoginSession account{};account.user_id=11;auto handoff=roster_factory(account,channel);handoff->open();check(sent.size()==6,"roster was not delivered");handoff->receive({9,1,1});handoff->receive({4,1,42,0,0,0});handoff->receive({1,2});handoff->close_with_reason(111);
 check(opens==1 && receives==2 && closes==1 && reason==111,"selection ownership or close reason lost");
 fail=true;auto failure=roster_factory(account,channel);failure->open();failure->receive({9,1,1});bool caught=false;try{failure->receive({4,1,42,0,0,0});}catch(const std::runtime_error&){caught=true;}failure->close();
 check(caught && opens==2 && closes==2,"failed selected owner was not closed");
 asio::io_context io2;auto guard2=asio::make_work_guard(io2);Strand strand2(asio::make_strand(io2));std::jthread world2([&]{io2.run();});
 std::promise<void> entered,release,world_work;auto entered_future=entered.get_future();auto release_future=release.get_future();auto work_future=world_work.get_future();
 auto nonblocking=game::roster_world_serialized_factory(strand2,[&](const session::LoginSession&){entered.set_value();release_future.wait();return game::CharacterRoster{};},
  [](const session::LoginSession&,game::CharacterRoster&,const net::Packet&){return game::CharacterReply{};},
  [&](const session::LoginSession&,net::WorldChannel,game::CharacterRoster){return std::make_unique<Selected>(opens,receives,closes,reason,false);});
 auto pending=nonblocking(account,channel);std::jthread worker([&]{pending->open();});entered_future.wait();asio::post(strand2,[&]{world_work.set_value();});
 const bool progressed=work_future.wait_for(std::chrono::seconds(2))==std::future_status::ready;release.set_value();worker.join();pending->close();guard2.reset();world2.join();
 check(progressed,"blocked roster loader stalled the shared world strand");
 asio::io_context io3;auto guard3=asio::make_work_guard(io3);Strand strand3(asio::make_strand(io3));asio::thread_pool database(1);std::jthread world3([&]{io3.run();});
 auto pool=std::make_shared<game::InventoryReplicaPool>();game::CharacterRoster selected_roster;selected_roster.country=0;game::CharacterSummary row;row.id=42;selected_roster.slots[0]=row;
 std::promise<void> started,release_load,selection_done,world_progress;auto started_result=started.get_future();auto load_gate=release_load.get_future();auto selection_result=selection_done.get_future();auto world_result=world_progress.get_future();
 auto async=std::make_shared<game::VerticalAsyncSelection>(strand3,database.get_executor(),game::CharacterSelectionState{11,0,true},selected_roster,
  [pool,&started,&load_gate](std::uint32_t user,std::uint32_t character)->std::optional<game::VerticalPersistenceSelection>{started.set_value();load_gate.wait();game::PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;auto replica=std::make_unique<game::CharacterReplica>(snapshot.core,*pool);return game::VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};},
  [&](game::VerticalPersistenceSelection result){check(strand3.running_in_this_thread() && result.snapshot.core.user_id==11 && result.replica->core().character.id==42,"asynchronous selection changed identity");selection_done.set_value();},
  []{throw std::runtime_error("unexpected unavailable selection");},[](std::uint32_t){throw std::runtime_error("unexpected selection close");},[&](std::exception_ptr e){selection_done.set_exception(e);});
 asio::post(strand3,[&]{check(async->receive(net::Packet{4,1,42,0,0,0})==game::CharacterSelectionResult::requested,"async selection not accepted");});started_result.wait();asio::post(strand3,[&]{world_progress.set_value();});
 const bool world_available=world_result.wait_for(std::chrono::seconds(2))==std::future_status::ready;release_load.set_value();selection_result.get();
 std::promise<void> cancel_started,cancel_release,cancelled,selection_drained,world_barrier;auto cancel_started_result=cancel_started.get_future();auto cancel_gate=cancel_release.get_future();auto cancelled_result=cancelled.get_future();auto drained_result=selection_drained.get_future();auto barrier_result=world_barrier.get_future();unsigned unexpected=0;int selection_fence=0;
 auto cancel_selection=std::make_shared<game::VerticalAsyncSelection>(strand3,database.get_executor(),game::CharacterSelectionState{11,0,true},selected_roster,
  [pool,&cancel_started,&cancel_gate](std::uint32_t user,std::uint32_t character)->std::optional<game::VerticalPersistenceSelection>{cancel_started.set_value();cancel_gate.wait();game::PlayerSnapshot snapshot;snapshot.core.user_id=user;snapshot.core.character.id=character;auto replica=std::make_unique<game::CharacterReplica>(snapshot.core,*pool);return game::VerticalPersistenceSelection{std::move(snapshot),std::move(replica)};},
  [&](game::VerticalPersistenceSelection){++unexpected;},[&]{++unexpected;},[&](std::uint32_t){++unexpected;},[&](std::exception_ptr){++unexpected;});
 asio::post(strand3,[&]{cancel_selection->set_fence([&]{++selection_fence;return true;},[&]{--selection_fence;return true;});(void)cancel_selection->receive(net::Packet{4,1,42,0,0,0});});cancel_started_result.wait();asio::post(strand3,[&]{cancel_selection->cancel();check(cancel_selection->pending() && selection_fence==1,"pending selection fence was lost on cancellation");cancel_selection->when_drained([&]{check(!cancel_selection->pending() && selection_fence==0,"drain signaled before selection fence completed");selection_drained.set_value();});cancelled.set_value();});cancelled_result.wait();
 check(drained_result.wait_for(std::chrono::milliseconds(0))==std::future_status::timeout,"cancelled load drained before worker returned");cancel_release.set_value();drained_result.get();database.join();asio::post(strand3,[&]{world_barrier.set_value();});barrier_result.wait();
 check(unexpected==0,"cancelled selection delivered a late result");cancel_selection.reset();async.reset();guard3.reset();world3.join();check(world_available,"selection loader blocked world executor");
 auto retained_pool=std::make_shared<game::InventoryReplicaPool>();std::weak_ptr<game::InventoryReplicaPool> weak_pool=retained_pool;
 game::PlayerCore owned_core{};owned_core.user_id=11;owned_core.character.id=42;game::PlayerSnapshot owned_snapshot;owned_snapshot.core=owned_core;
 auto owned_replica=std::make_unique<game::CharacterReplica>(owned_core,*retained_pool);owned_replica->retain_resources(retained_pool);
 game::VerticalPersistenceSelection owned{std::move(owned_snapshot),std::move(owned_replica)};retained_pool.reset();auto moved=std::move(owned.replica);
 check(!weak_pool.expired(),"moved replica lost its pool owner");moved.reset();check(weak_pool.expired(),"replica leaked its pool owner");
 std::cout<<"world application callbacks serialized across four connections\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
