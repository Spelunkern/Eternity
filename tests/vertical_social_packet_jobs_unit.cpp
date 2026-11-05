#include "eternity/game/vertical_social_save_fence.hpp"
#include "eternity/net/world_server.hpp"
#include <asio/io_context.hpp>
#include <asio/executor_work_guard.hpp>
#include <asio/post.hpp>
#include <future>
#include <iostream>
#include <thread>
using namespace eternity;using namespace eternity::game;
static void check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
struct Scene final:ActorScene {
 void send_self(const net::Packet&) noexcept override{}void send_nearby(const net::Packet&) override{}void send_cell_view(const net::Packet&) override{}
 std::optional<bool> party_kind() const noexcept override{return {};}void send_party(const net::Packet&) noexcept override{}void reset_damage() override{}
 std::optional<std::uint32_t> allocate_effect_id() noexcept override{return {};}void release_effect_id(std::uint32_t) noexcept override{}
 std::uint32_t tick_ms() const noexcept override{return 1;}void stop_target_mob(std::uint32_t) override{}void taunt(std::uint16_t,float) override{}
 ZoneUsers* user_zone() override{return nullptr;}UserShapeSocial user_shape_social() override{return {};}
 void finish_periodic_damage(EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t) override{}
};
struct Db {bool active=false;bool transaction_active() const{return active;}void begin(){active=true;}void commit(){active=false;}void rollback(){active=false;}void query(std::string,std::span<const storage::Parameter>) {}};
struct Session;using Social=VerticalSocialSession<Session,Db>;using Jobs=VerticalSocialPacketJobs<Session,Db>;using Router=VerticalSocialPacketRouter<Session,Db>;
struct SaveOwner;
struct Session {net::WorldChannel channel_value;std::shared_ptr<Social> social;std::shared_ptr<Jobs> jobs;std::shared_ptr<SaveOwner> save_owner;std::vector<net::Packet> sent;net::WorldChannel& channel(){return channel_value;}};
struct SaveOwner {
 std::shared_ptr<VerticalTrackedDbJobs<Session>> tracker;std::function<void()> before_submit,on_stop;
 bool submit(std::uint32_t job){before_submit();return tracker->submit(job);}
 void stop_and_drain(std::function<void()> done){on_stop();tracker->stop_producers();tracker->when_drained(std::move(done));}
};
using SaveFence=VerticalSocialSaveFence<Session,Db,SaveOwner>;
int main(){try {
 ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;Scene scene;InventoryReplicaPool pool;Db db;std::recursive_mutex mutex;
 Actor a(101,{}, {},{},scene,{items,enchant,mobs}),b(102,{}, {},{},scene,{items,enchant,mobs}),c(103,{}, {},{},scene,{items,enchant,mobs});a.account_id()=71;b.account_id()=72;c.account_id()=73;
 a.appearance().identity.name="Alpha";b.appearance().identity.name="Bravo";a.appearance().identity.country=b.appearance().identity.country=1;
 RuntimeZone zone;zone.users=std::make_unique<ZoneUsers>(2,2,0);zone.users->attach(a,0,0,{});zone.users->attach(b,0,0,{});
 WorldAdmissionUser aa,ab,ac;aa.user_id=71;ab.user_id=72;ac.user_id=73;aa.location=ab.location=ac.location=4;aa.zone=ab.zone=ac.zone=&zone;WorldUser ua{a,aa},ub{b,ab},uc{c,ac};
 WorldUsers world({[](WorldUser&){},[](WorldUser&){}});VerticalSessionRegistry<Session> registry;auto sa=std::make_shared<Session>(),sb=std::make_shared<Session>(),sc=std::make_shared<Session>();
 sa->channel_value.send=[&](net::Packet p){sa->sent.push_back(std::move(p));};sb->channel_value.send=[&](net::Packet p){sb->sent.push_back(std::move(p));};sc->channel_value.send=[&](net::Packet p){sc->sent.push_back(std::move(p));};
 PlayerCore ca{},cb{},cc{};ca.user_id=71;ca.character.id=101;cb.user_id=72;cb.character.id=102;cc.user_id=73;cc.character.id=103;CharacterReplica char_a(ca,pool),char_b(cb,pool),char_c(cc,pool);VerticalSocialReplica db_a(71),db_b(72),db_c(73);ZoneLeaveRequestState req_a,req_b,req_c;
 asio::io_context io;auto guard=asio::make_work_guard(io);auto strand=asio::make_strand(io);DbJobDispatcher dispatcher(false,4);
 std::promise<void> submitted,drained,save_stopping,save_permit;auto submitted_future=submitted.get_future(),drained_future=drained.get_future(),save_stopping_future=save_stopping.get_future();auto save_permit_future=save_permit.get_future().share();unsigned drains=0,save_runs=0,save_barriers=0;std::exception_ptr worker_error;
 std::shared_ptr<VerticalTrackedDbJobs<Session>> save_tracker;std::shared_ptr<SaveOwner> saves;std::unique_ptr<SaveFence> save_fence;
 std::unique_ptr<Router> router;Social::OwnerLookup owners=[](Session& s){return s.social;};
 std::thread thread([&]{io.run();});std::unique_lock hold(mutex);
 asio::post(strand,[&]{try {
  registry.retain(sa,a);registry.retain(sb,b);registry.retain(sc,c);world.enter(ua);world.enter(ub);
  router=std::make_unique<Router>(strand,registry,[](Session& s){return s.jobs.get();});
  Social::Services services{owners,[]{return 100U;},[&](net::Packet p){router->send(std::move(p));}};
  sa->social=std::make_shared<Social>(*sa,ua,SocialCatalog(71,101),req_a,db_a,char_a,db,mutex,registry,world,services,"test.");
  sb->social=std::make_shared<Social>(*sb,ub,SocialCatalog(72,102),req_b,db_b,char_b,db,mutex,registry,world,services,"test.");
  sc->social=std::make_shared<Social>(*sc,uc,SocialCatalog(73,103),req_c,db_c,char_c,db,mutex,registry,world,services,"test.");
  auto failed=[&](std::exception_ptr e){worker_error=e;try{save_stopping.set_exception(e);}catch(const std::future_error&) {}try{drained.set_exception(e);}catch(const std::future_error&) {}};
  sa->jobs=Jobs::create(strand,registry,sa,*sa->social,dispatcher,failed);sb->jobs=Jobs::create(strand,registry,sb,*sb->social,dispatcher,failed);
  bool duplicate_jobs=false;try{(void)Jobs::create(strand,registry,sa,*sa->social,dispatcher,failed);}catch(const std::logic_error&){duplicate_jobs=true;}check(duplicate_jobs,"duplicate account job tracker accepted");
  net::Packet request{2,0x22,'B','r','a','v','o',0};request.resize(23,0);
  auto on_drain=[&]{if(++drains==2)drained.set_value();};
  save_tracker=VerticalTrackedDbJobs<Session>::create(strand,registry,sa,71,dispatcher,
    [&](Session&,std::uint32_t job,const VerticalTrackedDbJobs<Session>::Retry&)->VerticalTrackedDbJobs<Session>::Packets {save_permit_future.wait();std::lock_guard lock(mutex);check(job==9,"wrong save job");++save_runs;return {};},
    [](Session&,VerticalTrackedDbJobs<Session>::Packets p){check(p.empty(),"unexpected save reply");},failed);
  saves=std::make_shared<SaveOwner>();saves->tracker=save_tracker;saves->before_submit=[&]{check(db_a.friends()[0].id==102,"save barrier overtook friend add");++save_barriers;};saves->on_stop=[&]{check(registry.pending_database_work(71)>0 && !registry.release(71,*sa),"save was not retained after social drain");bool duplicate=false;try{(void)Jobs::create(strand,registry,sa,*sa->social,dispatcher,[](std::exception_ptr e){std::rethrow_exception(e);});}catch(const std::logic_error&){duplicate=true;}check(duplicate,"new social worker claimed during save drain");save_stopping.set_value();};
  sa->save_owner=saves;auto save_lookup=[](Session& session){return session.save_owner;};
  bool foreign_save=false;try{(void)SaveFence(sb,sa->jobs,registry,save_lookup,failed);}catch(const std::invalid_argument&){foreign_save=true;}check(foreign_save,"foreign session save fence accepted");
  save_fence=std::make_unique<SaveFence>(sa,sa->jobs,registry,save_lookup,failed);
  check(sa->social->receive(request)==EntryCatalogUpdate::applied && sb->social->receive(net::Packet{3,0x22,1})==EntryCatalogUpdate::applied,"invitation failed");
  check(save_fence->submit(9),"ordered save rejected");
  check(sa->social->receive(net::Packet{6,0x22,102,0,0,0,3,'n','e','w',0})==EntryCatalogUpdate::applied && sa->social->receive(net::Packet{5,0x22,102,0,0,0})==EntryCatalogUpdate::applied,"social edits failed");
  check(registry.pending_database_work(71)>0 && registry.pending_database_work(72)>0 && !registry.release(71,*sa),"social jobs did not fence sessions");
  Router wrong(strand,registry,[&](Session&){return sa->jobs.get();});bool mismatched=false;try{wrong.send(net::Packet{3,12,72,0,0,0,101,0,0,0});}catch(const std::runtime_error&){mismatched=true;}check(mismatched,"foreign jobs owner accepted");
  bool missing=false;try{router->send(net::Packet{2,12,99,0,0,0});}catch(const std::runtime_error&){missing=true;}check(missing,"missing recipient accepted");
  save_fence->stop_and_drain(on_drain);sb->jobs->stop_and_drain(on_drain);
  check(!save_fence->submit(9),"stopped ordered save accepted");
  check(!sa->jobs->submit(net::Packet{3,12,71,0,0,0,102,0,0,0}),"stopped producer accepted work");
  submitted.set_value();
 }catch(...){submitted.set_exception(std::current_exception());}});
 bool permit_released=false;
 try{submitted_future.get();bool off_strand=false;try{router->send(net::Packet{3,12,71,0,0,0,102,0,0,0});}catch(const std::logic_error&){off_strand=true;}check(off_strand,"off-strand packet routing accepted");bool save_off_strand=false;try{save_fence->submit(9);}catch(const std::logic_error&){save_off_strand=true;}check(save_off_strand,"off-strand ordered save accepted");hold.unlock();save_stopping_future.get();check(drained_future.wait_for(std::chrono::seconds(0))==std::future_status::timeout,"combined drain completed before save");save_permit.set_value();permit_released=true;drained_future.get();dispatcher.wait_idle();}catch(...){if(hold.owns_lock())hold.unlock();if(!permit_released)save_permit.set_value();guard.reset();io.stop();thread.join();throw;}
 std::promise<void> reentrant_setup,reentrant_done;auto reentrant_setup_future=reentrant_setup.get_future(),reentrant_done_future=reentrant_done.get_future();unsigned reentrant_drains=0,reentrant_calls=0;
 std::shared_ptr<VerticalTrackedDbJobs<Session>> reentrant_save;
 asio::post(strand,[&]{try {
  auto failed_reentrant=[&](std::exception_ptr e){try{reentrant_done.set_exception(e);}catch(const std::future_error&) {}};
  auto on_reentrant_drain=[&]{if(++reentrant_drains==2)reentrant_done.set_value();};
  sc->jobs=Jobs::create(strand,registry,sc,*sc->social,dispatcher,failed_reentrant);
  reentrant_save=VerticalTrackedDbJobs<Session>::create(strand,registry,sc,73,dispatcher,
   [](Session&,std::uint32_t job,const VerticalTrackedDbJobs<Session>::Retry&)->VerticalTrackedDbJobs<Session>::Packets{check(job==8,"wrong reentrant save job");return {};},
   [](Session&,VerticalTrackedDbJobs<Session>::Packets replies){check(replies.empty(),"unexpected reentrant save reply");},failed_reentrant);
  check(sc->jobs->before_save([&,on_reentrant_drain]{
   ++reentrant_calls;const auto before=registry.pending_database_work(73);
   check(sc->jobs->submit(net::Packet{2,12,73,0,0,0,104,0,0,0,1,2,3,'D',0}),"reentrant social packet rejected");
   check(registry.pending_database_work(73)==before,"reentrant packet started another social worker");
   if(!reentrant_save->submit(8))return false;
   reentrant_save->stop_producers();reentrant_save->when_drained(on_reentrant_drain);sc->jobs->stop_and_drain(on_reentrant_drain);return true;
  }),"reentrant marker rejected");reentrant_setup.set_value();
 }catch(...){reentrant_setup.set_exception(std::current_exception());}});
 try{reentrant_setup_future.get();reentrant_done_future.get();dispatcher.wait_idle();}catch(...){guard.reset();io.stop();thread.join();throw;}
 std::promise<void> bad_setup,bad_failure;auto bad_setup_future=bad_setup.get_future(),bad_failure_future=bad_failure.get_future();unsigned bad_failures=0;
 auto failed_account=[&](std::uint32_t account,std::exception_ptr error){try {
  auto& session=account==71?sa:sb;check(static_cast<bool>(error) && registry.pending_database_work(account)==1 && !registry.release(account,*session),"failed work did not retain its fence");
  check(!session->jobs->submit(net::Packet{3,12,static_cast<std::uint8_t>(account),0,0,0,102,0,0,0}),"failed producer accepted more work");if(++bad_failures==2)bad_failure.set_value();
 }catch(...){try{bad_failure.set_exception(std::current_exception());}catch(const std::future_error&) {}}};
 asio::post(strand,[&]{try {
  check(reentrant_calls==1 && reentrant_drains==2 && db_c.friends()[0].id==104 && !registry.pending_database_work(73),"reentrant packet/save did not drain in order");
  sc->jobs.reset();reentrant_save.reset();sa->jobs.reset();sb->jobs.reset();
  {auto idle=Jobs::create(strand,registry,sc,*sc->social,dispatcher,[](std::exception_ptr e){std::rethrow_exception(e);});
   auto broken=std::make_shared<SaveOwner>();broken->tracker=save_tracker;broken->on_stop=[] {throw std::runtime_error("save drain failure");};sc->save_owner=broken;
   bool failure_seen=false,done_seen=false;SaveFence failing(sc,idle,registry,[](Session& session){return session.save_owner;},[&](std::exception_ptr e){failure_seen=static_cast<bool>(e);});
   failing.stop_and_drain([&]{done_seen=true;});check(failure_seen && !done_seen && registry.pending_database_work(73)==1 && !registry.release(73,*sc),"failed save drain lost session fence");
   bool duplicate=false;try{(void)Jobs::create(strand,registry,sc,*sc->social,dispatcher,[](std::exception_ptr e){std::rethrow_exception(e);});}catch(const std::logic_error&){duplicate=true;}check(duplicate,"failed save drain released social claim");sc->save_owner.reset();}
  sa->jobs=Jobs::create(strand,registry,sa,*sa->social,dispatcher,[&](std::exception_ptr error){failed_account(71,error);});
  sb->jobs=Jobs::create(strand,registry,sb,*sb->social,dispatcher,[&](std::exception_ptr error){failed_account(72,error);});
  check(sa->jobs->submit(net::Packet{2,12,71,0,0,0}),"malformed worker fixture was not queued");
  check(sb->jobs->before_save([]{return false;}),"rejected-save barrier was not queued");bad_setup.set_value();
 }catch(...){bad_setup.set_exception(std::current_exception());}});
 try{bad_setup_future.get();bad_failure_future.get();dispatcher.wait_idle();}catch(...){guard.reset();io.stop();thread.join();throw;}
 std::promise<void> cleanup;auto cleaned=cleanup.get_future();asio::post(strand,[&]{try {
  if(worker_error)std::rethrow_exception(worker_error);
  check(db_a.friends()[0].id==0 && db_a.deleted_friends().empty() && db_b.friends()[0].id==101 && save_barriers==1 && save_runs==1,"DB FIFO, save barrier or two-account routing differed");
  check(registry.pending_database_work(71)==1 && registry.pending_database_work(72)==1 && bad_failures==2,"failed work fences differ");
  world.leave(ua);world.leave(ub);zone.users->detach(a.id());zone.users->detach(b.id());save_fence.reset();sa->save_owner.reset();saves.reset();sa->jobs.reset();sb->jobs.reset();save_tracker.reset();sa->social.reset();sb->social.reset();sc->social.reset();
  check(!registry.release(71,*sa) && !registry.release(72,*sb) && !registry.release(73,*sc),"failed registry release differed");cleanup.set_value();
 }catch(...){cleanup.set_exception(std::current_exception());}});
 try{cleaned.get();}catch(...){guard.reset();io.stop();thread.join();throw;}guard.reset();io.stop();thread.join();
 std::cout<<"tracked social packet FIFO and drain passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
