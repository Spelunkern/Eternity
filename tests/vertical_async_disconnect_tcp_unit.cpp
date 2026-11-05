#define ETERNITY_DISCONNECT_FIXTURE_ONLY
#include "vertical_disconnect_tcp_unit.cpp"
#include "eternity/game/serialized_world_application.hpp"
#include "eternity/game/vertical_admission_handoff.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/vertical_tracked_db_jobs.hpp"
#include "eternity/game/vertical_db_drain_group.hpp"
#include "eternity/game/vertical_social_replica.hpp"
#include <future>

struct AsyncForwarder final:net::WorldApplication {
 using Strand=game::SerializedWorldApplication::Strand;
 Strand strand;std::shared_ptr<RetainedSession> session;std::shared_ptr<Retention> retention;std::shared_ptr<VerticalSessionRegistry<RetainedSession>> owners;std::shared_ptr<VerticalAdmissionHandoff> handoff;
 AsyncForwarder(Strand s,std::shared_ptr<RetainedSession> owner,std::shared_ptr<Retention> retain,std::shared_ptr<VerticalSessionRegistry<RetainedSession>> registry):strand(std::move(s)),session(std::move(owner)),retention(std::move(retain)),owners(std::move(registry)){}
 ~AsyncForwarder() override{std::lock_guard lock(session->app.state->mutex);retention->forwarder_destroyed=true;session->app.state->cv.notify_all();}
 void open() override{}
 void receive(const net::Packet& packet) override {
  auto& app=session->app;
  if(packet==net::Packet{4,1,42,0,0,0}) {
   handoff=std::make_shared<VerticalAdmissionHandoff>(strand,app.channel,app.user,[this]{
    auto& app=session->app;CharacterEntryState entry;entry.user_id=11;entry.character_id=42;entry.level=app.character.snapshot().core.character.level;
    auto catalogs=vertical_restore_entry_catalogs(app.character.replica());VerticalProtocolServices ports;
    ports.delivery.send_self=[&](net::Packet p){app.channel.send(std::move(p));};ports.delivery.send_peer=[](std::uint32_t,net::Packet){};
    ports.delivery.social=[](std::uint32_t)->const SocialCatalog*{return nullptr;};ports.delivery.tick=[] {return 1U;};ports.delivery.kill_count=[](std::uint8_t){return EntryKillCount{};};
    ports.lifecycle=[](CharacterEntryStep,Actor&){};ports.send_manager=[](net::Packet){};ports.world_day=[] {return 0U;};ports.system_time=[] {return 0U;};ports.lock_guild=[] {return false;};ports.unlock_guild=[]{};
    check(deliver_vertical_entry_with_admission(entry,app.character,app.world,app.user,session->social,catalogs,{0,100},"",ports),"async entry failed");
    owners->retain(session,app.actor);session->users.enter(session->user);update_vertical_admission(app.world);check(app.user.location==3,"zone publication failed");app.channel.send({0x76,0x77});handoff->zone_ready();
   },[](std::exception_ptr){throw std::runtime_error("handoff failed");});handoff->start();return;
  }
  if(packet==net::Packet{1,2}){app.pending_entry=packet;app.outcome.reset();app.world.update_zone_wait_packets(app.zone);check(app.outcome==VerticalEntryRequest::entered,"actor did not enter");app.channel.enter_world();app.channel.send({0x79,0x77});return;}
  if(packet.size()==17 && packet[0]==1 && packet[1]==5){check(app.move_session.receive(packet)==VerticalMoveResult::moved,"movement failed");app.channel.send({0x7b,0x77});return;}
  if(packet.size()>=2 && packet[0]==0x0b && packet[1]==1){check(app.character.quick_slots().receive(packet,app.actor.account_id(),[&](const net::Packet& p){check(session->lifecycle.apply(p)==InventoryReplicaResult::applied,"quick rejected");},[&](std::uint32_t){app.channel.disconnect();})==QuickSlotUpdate::accepted,"quick failed");app.channel.send({0x7c,0x77});return;}
  if(packet.size()>=2 && packet[0]==6 && packet[1]==0x22){check(session->social.apply_client(packet,[&](net::Packet p){check(session->social_lifecycle.apply(p)==EntryCatalogUpdate::applied,"social DB update rejected");},[&](net::Packet p){app.channel.send(std::move(p));},[](std::uint32_t)->const SocialCatalog*{return nullptr;},[](std::uint32_t,net::Packet){})==EntryCatalogUpdate::applied && session->social_db.friends()[0].memo=="x y" && session->social_db.friends()[0].save_type==2,"social TCP update failed");return;}
  throw std::runtime_error("unexpected packet");
 }
 void close() override {
  if(handoff)handoff->cancel();check(owners->find_account(11)==session,"world registry lost session");
  check(vertical_disconnect_in_zone(session->users,session->user,false,session->disconnect)==VerticalDisconnectResult::deferred,"disconnect did not defer");
  std::lock_guard lock(session->app.state->mutex);session->app.state->closed=true;session->app.state->cv.notify_all();
 }
};
int main(){try {
 asio::io_context io;game::SerializedWorldApplication::Strand strand(asio::make_strand(io));auto registry=std::make_shared<session::Registry>();auto state=std::make_shared<HandoffState>();auto retained=std::make_shared<Retention>();auto owners=std::make_shared<VerticalSessionRegistry<RetainedSession>>();auto shutdown_lease=std::make_shared<std::shared_ptr<void>>();auto channel_port=std::make_shared<net::WorldChannel>();net::WorldServerOptions options;options.port=0;
 auto factory=game::serialized_world_factory(strand,[=](const session::LoginSession&,net::WorldChannel channel){*channel_port=channel;*shutdown_lease=channel.retain_io();check(bool(*shutdown_lease),"shutdown lease unavailable");auto owner=std::make_shared<RetainedSession>(state,std::move(channel));retained->weak=owner;return std::make_unique<AsyncForwarder>(strand,std::move(owner),retained,owners);});
 DbJobDispatcher db_dispatch(false,1);std::shared_ptr<VerticalTrackedDbJobs<RetainedSession>> db_jobs;std::shared_ptr<VerticalDbDrainGroup<RetainedSession>> drain_group;
 std::promise<void> worker_started,worker_release,io_exited;auto worker_ready=worker_started.get_future();auto worker_gate=worker_release.get_future().share();auto io_done=io_exited.get_future();std::atomic_bool first_job{true};
 net::WorldServer server(io,options,registry,std::move(factory));server.start();std::jthread thread([&]{io.run();io_exited.set_value();});
 struct Guard{net::WorldServer& server;std::jthread& thread;~Guard(){server.stop();if(thread.joinable())thread.join();}} guard{server,thread};
 struct ReleaseWorker{std::promise<void>& promise;bool released=false;~ReleaseWorker(){if(!released)promise.set_value();}} release_worker{worker_release};
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(0x32);registry->add(account);auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());check(registry->select(11,101,0,now),"reservation failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());peer.write(net::frame(connect));net::LegacyCipher incoming(net::SessionSecrets{}),outgoing(net::SessionSecrets{});auto greeting=peer.packet();incoming.crypt(greeting);check(greeting.size()==20,"greeting failed");std::array<std::uint8_t,16> seed{};std::copy_n(greeting.begin()+4,16,seed.begin());net::WorldXor xor_key(seed);
 auto send=[&](net::Packet packet){outgoing.crypt(packet);peer.write(net::frame(packet));};send({4,1,42,0,0,0});send({1,2});auto ack=peer.packet();incoming.crypt(ack);check(ack==net::Packet({4,1,0,42,0,0,0}),"selection ack differs");
 bool saw_friend=false;for(unsigned i=0;;++i){check(i<32,"initial stream incomplete");auto packet=peer.packet();incoming.crypt(packet);if(packet==net::Packet{0x76,0x77})break;if(packet.size()>=3 && packet[0]==1 && packet[1]==0x22){check(packet[2]==1 && packet[3]==55 && packet[9]=='F',"loaded friend packet differs");saw_friend=true;}}check(saw_friend,"loaded friend was not delivered over TCP");
 for(int i=0;i<5;++i){auto packet=peer.packet();incoming.crypt(packet);}auto entered=peer.packet();xor_key.crypt(entered);check(entered==net::Packet({0x79,0x77}),"zone entry failed");
 net::Packet movement{1,5,0x81,0x34,0x12};for(float value:{45.F,0.F,40.F}){auto bits=std::bit_cast<std::uint32_t>(value);for(unsigned i=0;i<4;++i)movement.push_back(static_cast<std::uint8_t>(bits>>(8*i)));}send(movement);auto moved=peer.packet();xor_key.crypt(moved);check(moved.size()==21 && moved[0]==1 && moved[1]==5,"movement broadcast failed");auto done=peer.packet();xor_key.crypt(done);check(done==net::Packet({0x7b,0x77}),"movement confirmation failed");
 send({0x0b,1,1,2,3,4,0x34,0x12});auto quick=peer.packet();xor_key.crypt(quick);check(quick==net::Packet({0x7c,0x77}),"quick slots failed");
 send({6,0x22,55,0,0,0,3,'x','\'','y',0});auto social_reply=peer.packet();xor_key.crypt(social_reply);check(social_reply==net::Packet({6,0x22,55,0,0,0,3,'x','\'','y',0}),"social memo response failed");
 asio::error_code error;peer.socket.close(error);state->wait([&]{return state->closed && retained->forwarder_destroyed;});
 std::promise<void> submitted,completed,drained;auto submitted_result=submitted.get_future();auto result=completed.get_future();auto drain_result=drained.get_future();asio::post(strand,[&]{try {
  auto owner=owners->find_account(11);check(owner && owners->retain_actor(owner->app.actor)==owner && owner->app.user.logout_tick==11234 && owner->users.find(42)==&owner->user && owner->social_db.friends()[0].memo=="x y" && owner->social_db.friends()[0].save_type==2,"socket close lost world owner or social update");
  db_jobs=VerticalTrackedDbJobs<RetainedSession>::create(strand,*owners,owner,11,db_dispatch,
   [&](RetainedSession& session,std::uint32_t job,const VerticalTrackedDbJobs<RetainedSession>::Retry& retry){
    if(first_job.exchange(false)){worker_started.set_value();worker_gate.wait();}
    return session.lifecycle.dispatch_collect(job,retry);},
   [&](RetainedSession& session,std::vector<net::Packet> replies){try {
    check(owners->pending_database_work(11)>=1 && !owners->release(11,session),"DB response not fenced");
    if(replies.empty()){check(owners->pending_database_work(11)==2,"retry did not preserve fence");return;}
    check(session.social_database.commits==2 && session.social_database.calls.size()==1 && session.social_database.calls[0].first.find("usp_Save_FriendMemo_E")!=std::string::npos && std::get<std::int64_t>(session.social_database.calls[0].second[0])==42 && std::get<std::int64_t>(session.social_database.calls[0].second[1])==55 && std::get<std::string>(session.social_database.calls[0].second[2])=="x y" && session.social_db.friends()[0].save_type==0,"retained social memo did not reach committed DB adapter");
    check(replies.size()==2 && vertical_database_disconnect_ack(replies.front(),session.ack_services),"DB ack failed");
    check(!session.users.find(42) && session.app.zone.users->size()==0,"world cleanup failed");completed.set_value();
   }catch(...){completed.set_exception(std::current_exception());}},
   [&](std::exception_ptr e){completed.set_exception(e);});
  owner->on_db_post=[&](std::uint32_t job){check(db_jobs->submit(job),"tracked DB enqueue failed");};
  owner->simulated_timeout_attempts=2;owner->app.actor.replace_resources_raw({123,456,789});zone_update_logout_deadlines(*owner->app.zone.users,11234,[&](Actor&){return std::shared_ptr<void>(owner);},[&](Actor&)->WorldAdmissionUser&{return owner->app.user;},[&](Actor&,WorldAdmissionUser&){check(vertical_disconnect_in_zone(owner->users,owner->user,false,owner->disconnect)==VerticalDisconnectResult::save_requested,"save not requested");});
  auto core=owner->app.character.replica().core();check(core.character.hp==123 && core.x==45,"save lost live resources or moved position");check(owners->pending_database_work(11)==1,"exit job not tracked");
  drain_group=VerticalDbDrainGroup<RetainedSession>::start({db_jobs},[&]{try{check(owners->pending_database_work(11)==0,"drain completed early");drained.set_value();}catch(...){drained.set_exception(std::current_exception());}});submitted.set_value();
 }catch(...){submitted.set_exception(std::current_exception());}});submitted_result.get();worker_ready.get();
 std::promise<bool> transport_closed;auto transport_result=transport_closed.get_future();server.stop([&](bool ok){transport_closed.set_value(ok);});
 check(transport_result.wait_for(3s)==std::future_status::ready && transport_result.get(),"transport close barrier did not complete");
 check(drain_result.wait_for(0ms)==std::future_status::timeout && !retained->weak.expired(),"transport close falsely implied DB drain or owner release");
 std::promise<bool> stop_barrier;auto stopped=stop_barrier.get_future();channel_port->pause_input([&](bool active){stop_barrier.set_value(active);});
 check(!stopped.get(),"closed connection resumed during stop");
 const bool io_held=io_done.wait_for(100ms)==std::future_status::timeout;worker_release.set_value();release_worker.released=true;
 check(io_held,"world IO exited while save worker was blocked");result.get();drain_result.get();db_dispatch.wait_idle();
 std::promise<void> released;auto release_result=released.get_future();asio::post(strand,[&]{try {
  auto owner=owners->find_account(11);check(owner && owners->pending_database_work(11)==0 && owner->jobs==std::vector<std::uint32_t>({9}),"DB retry or fence not drained");db_jobs->stop_producers();owner->on_db_post={};
  check(owners->release(11,*owner) && owners->size()==0,"world owner release failed");drain_group.reset();db_jobs.reset();owner.reset();check(retained->weak.expired(),"world session still retained");shutdown_lease->reset();released.set_value();
 }catch(...){released.set_exception(std::current_exception());}});release_result.get();db_dispatch.shutdown();thread.join();check(io_done.wait_for(0ms)==std::future_status::ready,"world IO did not finish after save drain");check(server.application_failures()==0 && !db_dispatch.failure(),"application or DB worker failed");
 std::cout<<"nonblocking entry, movement, disconnect, retained save and cleanup passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
