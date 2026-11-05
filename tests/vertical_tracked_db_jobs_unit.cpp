#define ETERNITY_SESSION_REGISTRY_FIXTURE_ONLY
#include "vertical_session_registry_unit.cpp"
#include "eternity/game/vertical_tracked_db_jobs.hpp"
#include <asio/io_context.hpp>
#include <future>
#include <thread>
int main(){try {
 asio::io_context io;auto guard=asio::make_work_guard(io);VerticalTrackedDbJobs<Owner>::Strand strand(asio::make_strand(io));
 VerticalSessionRegistry<Owner> registry;auto owner=std::make_shared<Owner>(11,42);DbJobDispatcher dispatcher(false,1);
 std::jthread world([&]{io.run();});std::promise<void> submitted,delivered,drained;auto sent=submitted.get_future(),done=delivered.get_future(),drain_done=drained.get_future();std::shared_ptr<VerticalTrackedDbJobs<Owner>> jobs;unsigned db_runs=0,world_replies=0;
 asio::post(strand,[&]{try {
  registry.retain(owner,owner->actor);
  jobs=VerticalTrackedDbJobs<Owner>::create(strand,registry,owner,11,dispatcher,
   [&](Owner& current,std::uint32_t job,const VerticalTrackedDbJobs<Owner>::Retry& retry){check(current.actor.id()==42 && job==9,"wrong DB owner/job");if(++db_runs==1){retry(9);return std::vector<net::Packet>{};}return std::vector<net::Packet>{{2,3,11,0,0,0}};},
   [&](Owner& current,std::vector<net::Packet> replies){try {++world_replies;check(strand.running_in_this_thread() && !registry.release(11,current),"reply lost fence");if(world_replies==1){check(replies.empty() && registry.pending_database_work(11)==2,"retry not fenced before original completion");return;}check(registry.pending_database_work(11)==1 && replies==std::vector<net::Packet>{{2,3,11,0,0,0}},"retry reply differs");delivered.set_value();}catch(...){delivered.set_exception(std::current_exception());}},
   [&](std::exception_ptr e){delivered.set_exception(e);});
  check(jobs->submit(9) && registry.pending_database_work(11)==1 && !registry.release(11,*owner),"submission not fenced");jobs->stop_producers();check(!jobs->submit(9),"stopped producer accepted new work");jobs->when_drained([&]{try{check(registry.pending_database_work(11)==0 && world_replies==2,"drain fired too early");drained.set_value();}catch(...){drained.set_exception(std::current_exception());}});submitted.set_value();
 }catch(...){submitted.set_exception(std::current_exception());}});
 sent.get();done.get();drain_done.get();dispatcher.wait_idle();std::promise<void> cleanup;auto cleaned=cleanup.get_future();asio::post(strand,[&]{try{check(registry.pending_database_work(11)==0 && db_runs==2 && world_replies==2,"retry did not drain after producer stop");jobs->stop_producers();check(!jobs->submit(9) && registry.release(11,*owner),"stop or release failed");jobs.reset();cleanup.set_value();}catch(...){cleanup.set_exception(std::current_exception());}});cleaned.get();
 dispatcher.shutdown();guard.reset();world.join();check(!dispatcher.failure(),"DB worker failed");std::cout<<"tracked native DB work and reply fence passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
