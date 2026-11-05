#define ETERNITY_SESSION_REGISTRY_FIXTURE_ONLY
#include "vertical_session_registry_unit.cpp"
#include "eternity/game/vertical_db_drain_group.hpp"
#include "eternity/game/vertical_shutdown_drain.hpp"
#include <asio/io_context.hpp>
#include <atomic>
#include <future>
#include <thread>
static void verify_shutdown_join(){
 asio::io_context io;auto guard=asio::make_work_guard(io);VerticalTrackedDbJobs<Owner>::Strand strand(asio::make_strand(io));
 VerticalSessionRegistry<Owner> registry,foreign;auto owner=std::make_shared<Owner>(21,52);DbJobDispatcher dispatcher(false,1);
 std::promise<void> worker_started,release_worker,registered,delivered,complete;auto started=worker_started.get_future(),ready=registered.get_future(),reply=delivered.get_future(),drained=complete.get_future();auto gate=release_worker.get_future().share();
 std::shared_ptr<VerticalTrackedDbJobs<Owner>> tracker;std::shared_ptr<VerticalShutdownDrain<Owner>> shutdown;bool release_returned=false;unsigned deliveries=0;
 std::jthread world([&]{io.run();});
 struct Cleanup {asio::io_context& io;std::jthread& world;DbJobDispatcher& dispatcher;std::promise<void>& release;bool released=false;
  ~Cleanup(){if(!released)release.set_value();io.stop();if(world.joinable())world.join();dispatcher.shutdown();}} cleanup{io,world,dispatcher,release_worker};
 asio::post(strand,[&]{try {
  registry.retain(owner,owner->actor);
  tracker=VerticalTrackedDbJobs<Owner>::create(strand,registry,owner,21,dispatcher,
   [&](Owner&,std::uint32_t,const VerticalTrackedDbJobs<Owner>::Retry&){worker_started.set_value();gate.wait();return std::vector<net::Packet>{{1,2}};},
   [&](Owner&,std::vector<net::Packet> packets){try {check(packets==std::vector<net::Packet>{{1,2}},"DB reply changed");++deliveries;delivered.set_value();}catch(...){delivered.set_exception(std::current_exception());}},
   [&](std::exception_ptr e){delivered.set_exception(e);});
  check(tracker->submit(1),"tracked shutdown job rejected");
  bool wrong=false,duplicate=false,wrong_strand=false;
  try {(void)VerticalShutdownDrain<Owner>::create(strand,foreign,{tracker},[]{});}catch(const std::invalid_argument&){wrong=true;}
  try {(void)VerticalShutdownDrain<Owner>::create(strand,registry,{tracker,tracker},[]{});}catch(const std::invalid_argument&){duplicate=true;}
  VerticalTrackedDbJobs<Owner>::Strand other(asio::make_strand(io));
  auto misplaced=VerticalTrackedDbJobs<Owner>::create(other,registry,owner,21,dispatcher,
   [](Owner&,std::uint32_t,const VerticalTrackedDbJobs<Owner>::Retry&)->std::vector<net::Packet>{throw std::logic_error("unexpected DB work");},
   [](Owner&,std::vector<net::Packet>){throw std::logic_error("unexpected DB delivery");},[](std::exception_ptr){throw std::logic_error("unexpected DB failure");});
  try {(void)VerticalShutdownDrain<Owner>::create(strand,registry,{misplaced},[]{});}catch(const std::invalid_argument&){wrong_strand=true;}
  check(wrong && duplicate && wrong_strand && !registry.admission_closed(),"invalid drain changed registry state");
  shutdown=VerticalShutdownDrain<Owner>::create(strand,registry,{tracker},[&]{try{check(release_returned && deliveries==1 && registry.size()==0,"joined drain signaled before session release");complete.set_value();}catch(...){complete.set_exception(std::current_exception());}});
  check(shutdown->start() && !shutdown->error() && !tracker->submit(2) && !registry.release(21,*owner),"shutdown failed to retain active work");
  registered.set_value();
 }catch(...){registered.set_exception(std::current_exception());}});
 ready.get();started.get();check(drained.wait_for(std::chrono::seconds(0))==std::future_status::timeout,"joined drain ignored active worker");
 release_worker.set_value();cleanup.released=true;reply.get();
 std::promise<void> released;auto released_future=released.get_future();asio::post(strand,[&]{try {
  check(drained.wait_for(std::chrono::seconds(0))==std::future_status::timeout && registry.release(21,*owner),"joined drain ignored retained session");
  release_returned=true;released.set_value();
 }catch(...){released.set_exception(std::current_exception());}});
 released_future.get();drained.get();dispatcher.wait_idle();shutdown.reset();tracker.reset();guard.reset();world.join();
}
static void verify_failed_shutdown_join(bool fail_work){
 using namespace eternity;
 asio::io_context io;auto guard=asio::make_work_guard(io);VerticalTrackedDbJobs<Owner>::Strand strand(asio::make_strand(io));
 auto issuer=std::make_shared<session::Registry>();session::LoginSession login{77,301,1,"native",{}, {}};login.token.fill(7);
 issuer->add(login);check(issuer->select(77,301,0,1),"failure handoff failed");auto claim=issuer->claim_world(77,login.token,0,401,2);check(bool(claim.session),"failure claim failed");
 auto owners=std::make_unique<VerticalSessionRegistry<Owner>>();auto owner=std::make_shared<Owner>(77,500);DbJobDispatcher dispatcher(false,1);
 std::shared_ptr<VerticalTrackedDbJobs<Owner>> tracker;std::shared_ptr<VerticalShutdownDrain<Owner>> shutdown;
 std::promise<void> setup,checked;std::promise<std::exception_ptr> failure;auto ready=setup.get_future();auto failed=failure.get_future();auto verified=checked.get_future();std::atomic_bool completed{false};
 std::jthread world([&]{io.run();});
 struct Cleanup {asio::io_context& io;std::jthread& world;DbJobDispatcher& dispatcher;~Cleanup(){io.stop();if(world.joinable())world.join();dispatcher.shutdown();}} cleanup{io,world,dispatcher};
 asio::post(strand,[&]{try {
  owners->bind_network_registry(issuer,0);owners->reserve(77,owner,claim.session->world_reservation);
  tracker=VerticalTrackedDbJobs<Owner>::create(strand,*owners,owner,77,dispatcher,
   [fail_work](Owner&,std::uint32_t,const VerticalTrackedDbJobs<Owner>::Retry&)->std::vector<net::Packet>{if(fail_work)throw std::runtime_error("worker failure");return {{3,4}};},
   [fail_work](Owner&,std::vector<net::Packet>){if(!fail_work)throw std::runtime_error("delivery failure");throw std::logic_error("unexpected delivery");},
   [&](std::exception_ptr error){failure.set_value(error);});
  check(tracker->submit(9),"failure job not accepted");
  shutdown=VerticalShutdownDrain<Owner>::create(strand,*owners,{tracker},[&]{completed=true;});
  check(shutdown->start() && !shutdown->error() && !tracker->submit(10),"failure drain did not start");setup.set_value();
 }catch(...){setup.set_exception(std::current_exception());}});
 check(ready.wait_for(std::chrono::seconds(3))==std::future_status::ready,"failure setup stalled");ready.get();
 auto replacement=login;replacement.session_id=302;issuer->add(replacement);check(issuer->select(77,302,0,2),"failure replacement handoff failed");claim.session.reset();
 check(failed.wait_for(std::chrono::seconds(3))==std::future_status::ready,"real tracker failure was not delivered");
 auto error=failed.get();check(bool(error),"tracker returned an empty failure");bool expected=false;try{std::rethrow_exception(error);}catch(const std::runtime_error& e){expected=std::string(e.what())==(fail_work?"worker failure":"delivery failure");}
 check(expected,"tracker failure changed");dispatcher.wait_idle();
 asio::post(strand,[&]{try {
  check(!completed && owners->pending_database_work(77)==1 && !owners->release(77,*owner),"failed DB work reported drained or released owner");
  check(issuer->claim_world(77,login.token,0,402,3).duplicate && issuer->world_adoptions().size()==1,"failed DB work lost account fence");
  // The test fixture is being abandoned after its worker has stopped. Do not
  // synthesize a successful drain or release the issuer's adoption.
  shutdown.reset();tracker.reset();owner.reset();owners.reset();
  check(!completed && issuer->world_adoptions().size()==1 && issuer->claim_world(77,login.token,0,402,3).duplicate,"abandoned failure lost issuer fence");checked.set_value();
 }catch(...){checked.set_exception(std::current_exception());}});
 check(verified.wait_for(std::chrono::seconds(3))==std::future_status::ready,"failure verification stalled");verified.get();
 dispatcher.shutdown();guard.reset();world.join();check(!dispatcher.failure(),"dispatcher failed outside tracked job");
}
static void verify_empty_shutdown_join(){
 asio::io_context io;VerticalTrackedDbJobs<Owner>::Strand strand(asio::make_strand(io));VerticalSessionRegistry<Owner> registry;
 std::shared_ptr<VerticalShutdownDrain<Owner>> shutdown;bool start_returned=false;std::promise<void> done;auto result=done.get_future();
 asio::post(strand,[&]{try {
  shutdown=VerticalShutdownDrain<Owner>::create(strand,registry,{},[&]{try {check(start_returned && registry.admission_closed(),"empty shutdown notified inline");done.set_value();}catch(...){done.set_exception(std::current_exception());}});
  check(shutdown->start() && !shutdown->error(),"empty shutdown failed");bool duplicate=false;try{(void)shutdown->start();}catch(const std::logic_error&){duplicate=true;}
  check(duplicate,"shutdown started twice");start_returned=true;
 }catch(...){done.set_exception(std::current_exception());}});
 io.run();result.get();shutdown.reset();
}
struct ShutdownRootFixture {
 VerticalSessionRegistry<Owner> registry;
 VerticalSessionRegistry<Owner>& sessions(){return registry;}
};
static void verify_root_shutdown_lifetime(){
 asio::io_context io;VerticalTrackedDbJobs<Owner>::Strand strand(asio::make_strand(io));
 auto root=std::make_shared<ShutdownRootFixture>();std::weak_ptr<ShutdownRootFixture> weak=root;
 std::shared_ptr<VerticalShutdownDrain<Owner>> shutdown;bool returned=false;std::promise<void> done;auto complete=done.get_future();
 asio::post(strand,[&]{try {
  shutdown=VerticalShutdownDrain<Owner>::create_for_root(strand,root,{},[&]{try {check(returned && !weak.expired(),"root was not retained through drain");done.set_value();}catch(...){done.set_exception(std::current_exception());}});
  root.reset();check(!weak.expired() && shutdown->start() && !shutdown->error(),"root drain failed to start");returned=true;
 }catch(...){done.set_exception(std::current_exception());}});
 io.run();complete.get();std::promise<void> destroyed;auto released=destroyed.get_future();io.restart();
 asio::post(strand,[&]{try {shutdown.reset();check(weak.expired(),"root remained after drain handle release");destroyed.set_value();}catch(...){destroyed.set_exception(std::current_exception());}});
 io.run();released.get();
}
int main(){try {
 verify_root_shutdown_lifetime();verify_empty_shutdown_join();verify_shutdown_join();verify_failed_shutdown_join(true);verify_failed_shutdown_join(false);
 asio::io_context io;auto guard=asio::make_work_guard(io);VerticalTrackedDbJobs<Owner>::Strand strand(asio::make_strand(io));
 VerticalSessionRegistry<Owner> registry;auto a=std::make_shared<Owner>(11,42),b=std::make_shared<Owner>(12,43);DbJobDispatcher dispatcher(false,2);
 std::jthread world([&]{io.run();});std::promise<void> submitted,first,second_started,allow_second,done;auto ready=submitted.get_future(),first_result=first.get_future(),started=second_started.get_future(),drained=done.get_future();auto allowed=allow_second.get_future().share();
 std::atomic_bool group_done=false;unsigned delivered=0;std::shared_ptr<VerticalTrackedDbJobs<Owner>> left,right;std::shared_ptr<VerticalDbDrainGroup<Owner>> group;
 asio::post(strand,[&]{try {
  registry.retain(a,a->actor);registry.retain(b,b->actor);
  left=VerticalTrackedDbJobs<Owner>::create(strand,registry,a,11,dispatcher,
   [](Owner&,std::uint32_t,const VerticalTrackedDbJobs<Owner>::Retry&){return std::vector<net::Packet>{{1,2}};},
   [&](Owner&,std::vector<net::Packet>){++delivered;first.set_value();},[&](std::exception_ptr e){done.set_exception(e);});
  right=VerticalTrackedDbJobs<Owner>::create(strand,registry,b,12,dispatcher,
   [&](Owner&,std::uint32_t,const VerticalTrackedDbJobs<Owner>::Retry&){second_started.set_value();allowed.wait();return std::vector<net::Packet>{{3,4}};},
   [&](Owner&,std::vector<net::Packet>){++delivered;},[&](std::exception_ptr e){done.set_exception(e);});
  check(left->submit(8) && right->submit(9),"cannot submit jobs");
  group=VerticalDbDrainGroup<Owner>::start({left,right},[&]{try{check(delivered==2 && registry.pending_database_work(11)==0 && registry.pending_database_work(12)==0,"drain fired early");group_done=true;done.set_value();}catch(...){done.set_exception(std::current_exception());}});
  check(!left->submit(8) && !right->submit(9),"drain accepted producer");submitted.set_value();
 }catch(...){submitted.set_exception(std::current_exception());}});
 ready.get();started.get();first_result.get();check(!group_done.load(),"group drained while second worker blocked");allow_second.set_value();drained.get();dispatcher.wait_idle();
 std::promise<void> cleanup;auto cleaned=cleanup.get_future();asio::post(strand,[&]{try {check(registry.release(11,*a) && registry.release(12,*b),"sessions did not release");group.reset();left.reset();right.reset();cleanup.set_value();}catch(...){cleanup.set_exception(std::current_exception());}});cleaned.get();
 auto c=std::make_shared<Owner>(13,44);std::shared_ptr<VerticalTrackedDbJobs<Owner>> failed_tracker;std::shared_ptr<VerticalDbDrainGroup<Owner>> failed_group;std::weak_ptr<VerticalTrackedDbJobs<Owner>> weak_tracker;std::promise<void> setup_failure,failure,abandoned;auto set_up=setup_failure.get_future(),failed=failure.get_future(),cancelled=abandoned.get_future();bool false_completion=false;
 asio::post(strand,[&]{try {
  registry.retain(c,c->actor);failed_tracker=VerticalTrackedDbJobs<Owner>::create(strand,registry,c,13,dispatcher,
   [](Owner&,std::uint32_t,const VerticalTrackedDbJobs<Owner>::Retry&)->std::vector<net::Packet>{throw std::runtime_error("DB failure fixture");},
   [](Owner&,std::vector<net::Packet>){throw std::runtime_error("unexpected delivery");},[&](std::exception_ptr){failure.set_value();});
  weak_tracker=failed_tracker;check(failed_tracker->submit(9),"failed job was not accepted");
  failed_group=VerticalDbDrainGroup<Owner>::start({failed_tracker},[&]{false_completion=true;});setup_failure.set_value();
 }catch(...){setup_failure.set_exception(std::current_exception());}});
 set_up.get();failed.get();dispatcher.wait_idle();asio::post(strand,[&]{try {
  check(registry.pending_database_work(13)==1 && !registry.release(13,*c),"failed job lost fence");
  failed_group->cancel();failed_group.reset();failed_tracker.reset();check(weak_tracker.expired() && !false_completion,"cancel left ownership cycle or reported success");abandoned.set_value();
 }catch(...){abandoned.set_exception(std::current_exception());}});cancelled.get();
 dispatcher.shutdown();guard.reset();world.join();check(!dispatcher.failure(),"DB dispatcher failed");std::cout<<"multiple session DB drains passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
