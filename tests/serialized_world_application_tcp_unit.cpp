#include "eternity/game/serialized_world_application.hpp"
#include "eternity/game/vertical_world_session.hpp"
#include "eternity/game/vertical_shutdown_drain.hpp"
#include "eternity/net/world_owner_stop.hpp"
#include "eternity/game/vertical_admission_handoff.hpp"
#include "eternity/game/vertical_async_selection.hpp"
#include "eternity/net/world_retained_lookup.hpp"
#include "eternity/net/world_priority_batch.hpp"
#include "eternity/net/world_priority_flush.hpp"
#include "eternity/game/user_priority_queue.hpp"
#include <future>
#include <barrier>
#include <condition_variable>
#include <mutex>
#include <atomic>
#include "login_socket.hpp"
#include <asio.hpp>
#include <iostream>
#include <memory>
#include <thread>
using namespace eternity;
using Strand=game::SerializedWorldApplication::Strand;
static void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct State {Strand strand;unsigned opened=0,received=0,closed=0,destroyed=0;bool invalid=false;explicit State(Strand s):strand(s){}};
struct App final:net::WorldApplication {
 State& state;net::WorldChannel channel;std::uint32_t id;game::WorldAdmissionUser user;game::RuntimeZone zone;std::shared_ptr<game::VerticalAdmissionHandoff> handoff;
 App(State& s,net::WorldChannel c,std::uint32_t uid):state(s),channel(std::move(c)),id(uid){check(state.strand.running_in_this_thread(),"factory outside shared strand");check(channel.peer_ipv4==std::array<std::uint8_t,4>{127,0,0,1},"accepted socket peer IPv4 was not retained");}
 ~App() override{if(!state.strand.running_in_this_thread())state.invalid=true;++state.destroyed;}
 void open() override{check(state.strand.running_in_this_thread(),"open outside shared strand");++state.opened;channel.send({0x7a,0x77,static_cast<std::uint8_t>(id)});}
 void receive(const net::Packet& packet) override{
  check(state.strand.running_in_this_thread(),"receive outside shared strand");++state.received;
  if(packet==net::Packet({9,1,55})){channel.send({0x7b,0x77,static_cast<std::uint8_t>(id)});return;}
  if(packet==net::Packet({4,1,42,0,0,0})){
   handoff=std::make_shared<game::VerticalAdmissionHandoff>(state.strand,channel,user,[this]{
    channel.send({4,1,0,42,0,0,0});user.location=3;user.zone=&zone;handoff->zone_ready();
   },[](std::exception_ptr){throw std::runtime_error("handoff failed");});handoff->start();return;
  }
  if(packet==net::Packet({1,2})){check(user.location==3 && user.zone==&zone,"queued entry arrived before publication");channel.send({0x7c,0x77});return;}
  throw std::runtime_error("bad application packet");
 }
 void close() override{check(state.strand.running_in_this_thread(),"close outside shared strand");if(handoff)handoff->cancel();++state.closed;}
};

struct DrainState {
 std::promise<void> started,release,closed,drained,exited;std::shared_future<void> gate=release.get_future().share();
 std::shared_ptr<void> lease;net::WorldChannel channel;
};
struct DrainApp final:net::WorldApplication {
 Strand strand;asio::any_io_executor db;std::shared_ptr<DrainState> state;std::shared_ptr<game::VerticalAsyncSelection> selection;
 DrainApp(Strand s,asio::any_io_executor d,std::shared_ptr<DrainState> p,net::WorldChannel c):strand(std::move(s)),db(std::move(d)),state(std::move(p)){state->channel=std::move(c);}
 void open() override {state->lease=state->channel.retain_io();check(bool(state->lease),"IO lease unavailable while connected");state->channel.send({0x7a,0x77});}
 void receive(const net::Packet& packet) override {
  game::CharacterRoster roster;roster.country=0;game::CharacterSummary row;row.id=42;roster.slots[0]=row;auto s=state;
  selection=std::make_shared<game::VerticalAsyncSelection>(strand,db,game::CharacterSelectionState{11,0,true},std::move(roster),
   [s](std::uint32_t,std::uint32_t)->std::optional<game::VerticalPersistenceSelection>{s->started.set_value();s->gate.wait();return {};},
   [](game::VerticalPersistenceSelection){throw std::logic_error("cancelled selection delivered");},[]{throw std::logic_error("cancelled selection unavailable");},
   [](std::uint32_t){throw std::logic_error("selection closed");},[](std::exception_ptr){throw std::logic_error("selection failed");});
  check(selection->receive(packet)==game::CharacterSelectionResult::requested,"selection was not requested");
 }
 void close() override {selection->cancel();auto s=state;selection->when_drained([s]{s->lease.reset();s->drained.set_value();});state->closed.set_value();}
};
static void verify_async_drain(){
 asio::io_context io;Strand strand(asio::make_strand(io));asio::thread_pool db(1);auto state=std::make_shared<DrainState>();auto sessions=std::make_shared<session::Registry>();
 auto started=state->started.get_future();auto closed=state->closed.get_future();auto drained=state->drained.get_future();auto exited=state->exited.get_future();
 net::WorldServerOptions options;options.port=0;
 auto factory=game::serialized_world_factory(strand,[strand,&db,state](const session::LoginSession&,net::WorldChannel c){return std::make_unique<DrainApp>(strand,db.get_executor(),state,std::move(c));});
 net::WorldServer server(io,options,sessions,std::move(factory));server.start();std::jthread thread([&]{io.run();state->exited.set_value();});
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());check(sessions->select(11,101,0,now),"drain reservation failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));
 net::LegacyCipher incoming(net::SessionSecrets{}),outgoing(net::SessionSecrets{});auto greeting=peer.packet();incoming.crypt(greeting);auto opened=peer.packet();incoming.crypt(opened);check(opened==net::Packet({0x7a,0x77}),"drain owner did not open");
 net::Packet request{4,1,42,0,0,0};outgoing.crypt(request);peer.write(net::frame(request));started.wait();
 std::vector<std::shared_ptr<void>> leases(17);leases[0]=state->channel.retain_io();check(bool(leases[0]),"concurrent IO lease unavailable");
 std::barrier start(17);std::vector<std::jthread> racers;for(std::size_t i=1;i<leases.size();++i)racers.emplace_back([&,i]{start.arrive_and_wait();leases[i]=state->channel.retain_io();});
 start.arrive_and_wait();server.stop();racers.clear();closed.wait();
 const bool held=exited.wait_for(std::chrono::milliseconds(100))==std::future_status::timeout;state->release.set_value();
 const bool completed=drained.wait_for(std::chrono::seconds(3))==std::future_status::ready;
 const bool lease_held=exited.wait_for(100ms)==std::future_status::timeout;leases.clear();if(!completed)io.stop();thread.join();db.join();
 check(held && completed && lease_held,"IO stopped before cancelled selection or retained leases drained");check(!state->channel.retain_io(),"closed connection issued IO lease");check(server.application_failures()==0,"draining application failed");
 bool pause_called=false,pause_active=true;state->channel.pause_input([&](bool active){pause_called=true;pause_active=active;});
 check(pause_called && !pause_active,"late admission pause did not fail after IO stopped");
 for(unsigned i=0;i<100;++i){state->channel.set_admission_dispatch(true);state->channel.enter_world();state->channel.enter_character_selection();state->channel.resume_input();state->channel.disconnect();state->channel.disconnect_with_reason(1);state->channel.send({1,2});}
 io.restart();check(io.poll()==0,"closed channel queued work after IO stopped");
}

struct PriorityPoolFixture final:game::PartyMembershipSendWorld {
 Strand strand;std::atomic_uint frees{0};std::array<game::PartyMembershipSendBuffer,2> buffers;std::function<void()> on_free;
 explicit PriorityPoolFixture(Strand value):strand(std::move(value)){}
 game::PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t) override {throw std::logic_error("unexpected allocate");}
 void free_send_buffer(game::PartyMembershipSendBuffer& value) override {check(strand.running_in_this_thread() && value.references==0,"pool release outside world strand");++frees;if(on_free){auto action=std::move(on_free);action();}}
 void enqueue_send(game::PartyMembershipUser&,game::PartyMembershipSendBuffer&) override {throw std::logic_error("unexpected enqueue");}
 std::uint32_t tick() override {throw std::logic_error("unexpected tick");}
 void lock(game::PartyNormalMembership&) override {throw std::logic_error("unexpected lock");}
 void unlock(game::PartyNormalMembership&) noexcept override {std::terminate();}
 void lock_effects(game::PartyMembershipUser&) override {throw std::logic_error("unexpected lock");}
 void unlock_effects(game::PartyMembershipUser&) noexcept override {std::terminate();}
 void send_user(game::PartyMembershipUser&,const net::Packet&) override {throw std::logic_error("unexpected user send");}
 void send_scene(game::PartyMembershipUser&,const net::Packet&) override {throw std::logic_error("unexpected scene send");}
 void unregister_party(game::PartyNormalMembership&) override {throw std::logic_error("unexpected unregister");}
 void cleanup_guild_creation(game::PartyMembershipUser&) override {throw std::logic_error("unexpected cleanup");}
 void fill_log(game::PartyMembershipUser&,std::span<std::uint8_t>) override {throw std::logic_error("unexpected log");}
 void send_log(const net::Packet&) override {throw std::logic_error("unexpected log send");}
};
struct IdentityApp final:net::WorldApplication {
 net::WorldChannel channel;
 explicit IdentityApp(net::WorldChannel c):channel(std::move(c)){}
 void open() override {channel.send({0x7a,0x77});}
 void receive(const net::Packet&) override {}
 void close() override {}
};
static void verify_connection_identity(){
 asio::io_context io;auto sessions=std::make_shared<session::Registry>();net::WorldServerOptions options;options.port=0;options.maximum_pending_batches=1;
 std::promise<net::WorldChannel> first_channel,second_channel;auto first=first_channel.get_future(),second=second_channel.get_future();
 net::WorldServer server(io,options,sessions,[&](const session::LoginSession& account,net::WorldChannel channel){
  if(account.user_id==11)first_channel.set_value(channel);else second_channel.set_value(channel);
  return std::make_unique<IdentityApp>(std::move(channel));
 });
 server.start();std::jthread thread([&]{io.run();});struct Cleanup{net::WorldServer& server;asio::io_context& io;~Cleanup(){server.stop();io.stop();}} cleanup{server,io};
 const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 for(std::uint32_t id:{11,12}){session::LoginSession account{id,100+id,1,"native",{}, {}};account.token.fill(id);sessions->add(account);check(sessions->select(id,100+id,0,now),"identity account setup failed");}
 PeerSocket a(server.port()),b(server.port());net::Packet one{1,0xa3,11,0,0,0};one.insert(one.end(),16,11);a.write(net::frame(one));
 net::Packet two{1,0xa3,12,0,0,0};two.insert(two.end(),16,12);b.write(net::frame(two));
 check(first.wait_for(3s)==std::future_status::ready && second.wait_for(3s)==std::future_status::ready,"identity applications did not open");
 auto ca=first.get(),cb=second.get();auto ia=ca.connection_identity(),ib=cb.connection_identity();
 // Drain the initial real protocol greeting and application frame first.
 a.packet();a.packet();b.packet();b.packet();
 const auto initial_deadline=std::chrono::steady_clock::now()+3s;auto initial=ca.pending_send_state();
 while(initial && (initial->frames || initial->bytes) && std::chrono::steady_clock::now()<initial_deadline){std::this_thread::sleep_for(1ms);initial=ca.pending_send_state();}
 check(initial && initial->frames==0 && initial->bytes==0,"initial sends did not drain");
 std::promise<void> paused,release;auto paused_future=paused.get_future();auto gate=release.get_future().share();
 struct Release{std::promise<void>& promise;bool done=false;~Release(){if(!done)promise.set_value();}} release_guard{release};
 ca.pause_input([&](bool ok){if(ok)paused.set_value();gate.wait();});
 check(paused_future.wait_for(3s)==std::future_status::ready,"pending-send test did not pause socket strand");
 ca.send({0x7b,0x77});auto pending=ca.pending_send_state();
 check(pending && pending->frames==1 && pending->bytes==4 && pending->byte_limit==options.maximum_queued_bytes,"posted frame was not included in bounded pending state");
 release.set_value();release_guard.done=true;a.packet();
 const auto drained_by=std::chrono::steady_clock::now()+3s;
 do {pending=ca.pending_send_state();if(pending && pending->frames==0 && pending->bytes==0)break;std::this_thread::sleep_for(1ms);} while(std::chrono::steady_clock::now()<drained_by);
 check(pending && pending->frames==0 && pending->bytes==0,"actual write did not release pending state");
 std::promise<bool> sent1,sent2;auto sent_first=sent1.get_future(),sent_second=sent2.get_future();
 check(ca.send_tracked({0x7c,0x77},[&](bool ok){sent1.set_value(ok);check(ca.send_tracked({0x7d,0x77},[&](bool next){sent2.set_value(next);}),"reentrant send was rejected");}),"tracked send was rejected");
 a.packet();a.packet();check(sent_first.wait_for(3s)==std::future_status::ready && sent_first.get() && sent_second.wait_for(3s)==std::future_status::ready && sent_second.get(),"real tracked writes did not complete");
 const auto tracked_deadline=std::chrono::steady_clock::now()+3s;
 do {pending=ca.pending_send_state();if(pending && pending->frames==0 && pending->bytes==0)break;std::this_thread::sleep_for(1ms);}while(std::chrono::steady_clock::now()<tracked_deadline);
 check(pending && pending->frames==0 && pending->bytes==0,"tracked writes did not release their reservations");
 // A batch consumes one slot and one byte reservation until its actual write
 // and completion have both finished; nested admission observes that slot.
 std::promise<void> batch_paused,batch_release;auto batch_ready=batch_paused.get_future();auto batch_gate=batch_release.get_future().share();
 struct BatchRelease{std::promise<void>& p;bool done=false;~BatchRelease(){if(!done)p.set_value();}} batch_guard{batch_release};
 ca.pause_input([&](bool ok){if(ok)batch_paused.set_value();batch_gate.wait();});check(batch_ready.wait_for(3s)==std::future_status::ready,"batch socket pause failed");
 std::vector<net::Packet> batch{{0x81,0x77},{0x82,0x77}};std::promise<bool> batch_done;auto batch_result=batch_done.get_future();std::atomic_bool nested_rejected=false;
 check(ca.send_batch_tracked(batch,[&](bool ok){nested_rejected=!ca.send_batch_tracked(batch,[](bool){});batch_done.set_value(ok);}),"batch admission failed");
 auto batch_pending=ca.pending_send_state();check(batch_pending && batch_pending->batches==1 && batch_pending->batch_limit==1 && batch_pending->frames==1 && batch_pending->bytes==8,"batch reservation accounting differs");
 check(!ca.send_batch_tracked(batch,[](bool){}),"pending batch limit was bypassed");
 batch_release.set_value();batch_guard.done=true;a.packet();a.packet();check(batch_result.wait_for(3s)==std::future_status::ready && batch_result.get() && nested_rejected,"batch write or reentrant bound failed");
 const auto batch_deadline=std::chrono::steady_clock::now()+3s;do {batch_pending=ca.pending_send_state();if(batch_pending && batch_pending->batches==0)break;std::this_thread::sleep_for(1ms);}while(std::chrono::steady_clock::now()<batch_deadline);
 check(batch_pending && batch_pending->batches==0,"batch count did not drain after callback");
 // Transfer real extracted pool references, force the socket-to-world post
 // to fail, and retry only after the real socket completion has arrived.
 asio::io_context world_io;auto world_work=asio::make_work_guard(world_io);Strand world_strand(asio::make_strand(world_io));std::jthread world_thread([&]{world_io.run();});
 struct WorldStop{asio::io_context& io;~WorldStop(){io.stop();}} world_stop{world_io};auto pool=std::make_shared<PriorityPoolFixture>(world_strand);
 // Saturate the actual connection batch slot, then extract and attempt the
 // real pool transfer on its world strand. Rejection must preserve ownership.
 std::promise<void> blocked,unblock;auto blocked_ready=blocked.get_future();auto unblock_gate=unblock.get_future().share();
 struct Unblock{std::promise<void>& p;bool done=false;~Unblock(){if(!done)p.set_value();}} unblock_guard{unblock};
 ca.pause_input([&](bool ok){if(ok)blocked.set_value();unblock_gate.wait();});check(blocked_ready.wait_for(3s)==std::future_status::ready,"priority rejection setup stalled");
 std::promise<bool> blocker_done;auto blocker_result=blocker_done.get_future();check(ca.send_batch_tracked({{0x84,0x77}},[&](bool ok){blocker_done.set_value(ok);}),"priority blocker rejected");
 auto entries=std::make_shared<std::vector<game::PartyMembershipSendBuffer*>>();std::promise<void> rejected_promise;auto rejected_result=rejected_promise.get_future();
 asio::post(world_strand,[&]{game::UserPriorityQueue queue(*pool);for(auto& buffer:pool->buffers){buffer.references=1;buffer.frame={4,0,0x83,0x77};check(queue.enqueue(1,buffer,1,0),"priority enqueue failed");game::release_party_membership_buffer(buffer,*pool);}
  auto extracted=queue.take_batch();check(extracted && queue.size()==0,"priority extraction failed");*entries=std::move(*extracted);
  auto rejected=net::WorldPriorityBatch::submit(world_strand,pool,*entries,ca,[](bool){throw std::logic_error("rejected completion ran");});
  check(!rejected && entries->size()==2 && pool->buffers[0].references==1 && pool->buffers[1].references==1 && pool->frees==0,"rejected priority batch lost references");rejected_promise.set_value();});
 check(rejected_result.wait_for(3s)==std::future_status::ready,"priority rejection did not complete");unblock.set_value();unblock_guard.done=true;a.packet();
 check(blocker_result.wait_for(3s)==std::future_status::ready && blocker_result.get(),"priority blocker failed");
 const auto retry_deadline=std::chrono::steady_clock::now()+3s;do {batch_pending=ca.pending_send_state();if(batch_pending && batch_pending->batches==0)break;std::this_thread::sleep_for(1ms);}while(std::chrono::steady_clock::now()<retry_deadline);
 check(batch_pending && batch_pending->batches==0,"priority blocker did not drain");
 std::promise<std::shared_ptr<net::WorldPriorityBatch>> transfer_promise;auto transfer_future=transfer_promise.get_future();std::promise<bool> transfer_done;auto transfer_result=transfer_done.get_future();
 asio::post(world_strand,[&]{auto transfer=net::WorldPriorityBatch::submit(world_strand,pool,*entries,ca,[&](bool ok){transfer_done.set_value(ok);},[]{throw std::runtime_error("injected world notification failure");});
  check(transfer && entries->empty(),"pool batch ownership did not transfer");transfer_promise.set_value(std::move(transfer));});
 check(transfer_future.wait_for(3s)==std::future_status::ready,"pool batch submission stalled");auto transfer=transfer_future.get();a.packet();a.packet();
 const auto post_deadline=std::chrono::steady_clock::now()+3s;while(!transfer->post_error() && std::chrono::steady_clock::now()<post_deadline)std::this_thread::sleep_for(1ms);
 check(transfer->post_error() && pool->frees==0 && transfer_result.wait_for(0s)==std::future_status::timeout,"failed world post lost retained pool references");
 asio::post(world_strand,[transfer]{transfer->retry();transfer->retry();});
 check(transfer_result.wait_for(3s)==std::future_status::ready && transfer_result.get() && pool->frees==2,"real completion retry did not release pool references once");
 // Real priority flush resets an unsent queue while capacity is occupied,
 // and rejects reentry during that synchronous reset.
 std::promise<void> flush_paused,flush_release;auto flush_ready=flush_paused.get_future();auto flush_gate=flush_release.get_future().share();
 struct FlushRelease{std::promise<void>& p;bool done=false;~FlushRelease(){if(!done)p.set_value();}} flush_guard{flush_release};
 ca.pause_input([&](bool ok){if(ok)flush_paused.set_value();flush_gate.wait();});check(flush_ready.wait_for(3s)==std::future_status::ready,"flush pause failed");
 std::promise<bool> flush_blocked;auto flush_blocker=flush_blocked.get_future();check(ca.send_batch_tracked({{0x88,0x77}},[&](bool ok){flush_blocked.set_value(ok);}),"flush blocker rejected");
 auto handles=std::make_shared<std::vector<std::shared_ptr<net::WorldPriorityBatch>>>();auto active=std::make_shared<bool>(false);std::promise<bool> reset_promise;auto reset_result=reset_promise.get_future();
 asio::post(world_strand,[&]{game::UserPriorityQueue queue(*pool);for(auto& buffer:pool->buffers){buffer.references=1;buffer.frame={4,0,0x89,0x77};check(queue.enqueue(1,buffer,1,0),"flush enqueue failed");game::release_party_membership_buffer(buffer,*pool);}
  bool rejected=false;pool->on_free=[&]{try{net::flush_world_priority(world_strand,queue,pool,ca,*handles,*active,[](bool){});}catch(const std::logic_error&){rejected=true;}};
  net::flush_world_priority(world_strand,queue,pool,ca,*handles,*active,[](bool){throw std::logic_error("unexpected flush completion");});
  reset_promise.set_value(rejected && queue.size()==0 && pool->frees==4 && handles->empty() && !*active);});
 check(reset_result.wait_for(3s)==std::future_status::ready && reset_result.get(),"priority flush reset or reentry failed");flush_release.set_value();flush_guard.done=true;a.packet();
 check(flush_blocker.wait_for(3s)==std::future_status::ready && flush_blocker.get(),"flush blocker write failed");
 const auto flush_deadline=std::chrono::steady_clock::now()+3s;do {batch_pending=ca.pending_send_state();if(batch_pending && batch_pending->batches==0)break;std::this_thread::sleep_for(1ms);}while(std::chrono::steady_clock::now()<flush_deadline);
 check(batch_pending && batch_pending->batches==0,"flush blocker did not drain");
 std::promise<void> flush_started;auto flush_start=flush_started.get_future();std::promise<bool> flush_finished;auto flush_done=flush_finished.get_future();
 asio::post(world_strand,[&]{game::UserPriorityQueue queue(*pool);for(auto& buffer:pool->buffers){buffer.references=1;buffer.frame={4,0,0x8a,0x77};check(queue.enqueue(1,buffer,1,0),"accepted flush enqueue failed");game::release_party_membership_buffer(buffer,*pool);}
  net::flush_world_priority(world_strand,queue,pool,ca,*handles,*active,[&](bool ok){flush_finished.set_value(ok);});check(queue.size()==0 && handles->size()==1 && !*active,"accepted flush lost its handle");flush_started.set_value();});
 check(flush_start.wait_for(3s)==std::future_status::ready,"accepted flush stalled");a.packet();a.packet();check(flush_done.wait_for(3s)==std::future_status::ready && flush_done.get() && pool->frees==6,"real priority flush did not finish");
 std::promise<bool> flush_drained;auto drained=flush_drained.get_future();asio::post(world_strand,[&]{flush_drained.set_value(handles->size()==1 && handles->front()->complete());handles->clear();});
 check(drained.wait_for(3s)==std::future_status::ready && drained.get(),"priority flush world fence did not drain");
 world_work.reset();world_thread.join();
 check(ia && ib && ia->active() && ib->active() && ia->value!=ib->value &&
  (ia->generation.owner_before(ib->generation)||ib->generation.owner_before(ia->generation)),"accepted connection identities were not distinct and live");
 asio::error_code ignored;a.socket.close(ignored);const auto deadline=std::chrono::steady_clock::now()+3s;
 while(ia->active() && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
 check(!ia->active() && !ca.connection_identity() && !ca.pending_send_state() && ib->active(),"peer close did not invalidate only its own identity");
 // Queue a close ahead of an accepted send while the socket strand is held.
 std::promise<void> paused2,release2,callback_entered,callback_release;auto pause_ready=paused2.get_future();auto gate2=release2.get_future().share();auto callback_ready=callback_entered.get_future();auto callback_gate=callback_release.get_future().share();
 struct ReleaseBoth{std::promise<void>& a;std::promise<void>& b;bool ar=false,br=false;~ReleaseBoth(){if(!ar)a.set_value();if(!br)b.set_value();}} gates{release2,callback_release};
 cb.pause_input([&](bool ok){if(ok)paused2.set_value();gate2.wait();});check(pause_ready.wait_for(3s)==std::future_status::ready,"close race did not pause socket strand");
 cb.disconnect();std::promise<bool> finished;auto done=finished.get_future();std::atomic_bool cancelled_ok=true,rejected=false;
 check(cb.send_batch_tracked({{0x7e,0x77},{0x7f,0x77}},[&](bool ok){cancelled_ok=ok;rejected=!cb.send_batch_tracked({{0x7f,0x77}},[](bool){});server.stop([&](bool result){finished.set_value(result);});callback_entered.set_value();callback_gate.wait();}),"queued close-race send rejected before close");
 release2.set_value();gates.ar=true;check(callback_ready.wait_for(3s)==std::future_status::ready,"cancelled send callback did not run");
 check(done.wait_for(100ms)==std::future_status::timeout && !cancelled_ok && rejected,"stop or reentrant rejection bypassed tracked completion");
 callback_release.set_value();gates.br=true;
 check(done.wait_for(3s)==std::future_status::ready && done.get(),"identity stop stalled");
 check(!ib->active() && !cb.connection_identity(),"stop retained a live connection identity");thread.join();
}

static void verify_signed_batch_limit(){
 for(std::int32_t limit:{0,-1,std::numeric_limits<std::int32_t>::min()}) {
  asio::io_context io;auto sessions=std::make_shared<session::Registry>();net::WorldServerOptions options;options.port=0;options.maximum_pending_batches=limit;
  std::promise<net::WorldChannel> published;auto channel_future=published.get_future();std::atomic_uint callbacks{0};
  net::WorldServer server(io,options,sessions,[&](const session::LoginSession&,net::WorldChannel channel){published.set_value(channel);return std::make_unique<IdentityApp>(std::move(channel));});
  server.start();std::jthread thread([&]{io.run();});struct Cleanup{net::WorldServer& server;asio::io_context& io;~Cleanup(){server.stop();io.stop();}} cleanup{server,io};
  const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"signed limit setup failed");
  PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));
  check(channel_future.wait_for(3s)==std::future_status::ready,"signed limit channel stalled");auto channel=channel_future.get();peer.packet();peer.packet();
  std::vector<net::Packet> batch{{0x86,0x77}};auto state=channel.pending_send_state();
  check(state && state->batch_limit==limit && state->batches==0 && !channel.send_batch_tracked(batch,[&](bool){++callbacks;}) && batch.size()==1,"signed limit admitted a batch");
  std::promise<bool> sent;auto sent_future=sent.get_future();check(channel.send_tracked({0x87,0x77},[&](bool ok){sent.set_value(ok);}),"signed limit blocked ordinary tracked send");
  peer.packet();check(sent_future.wait_for(3s)==std::future_status::ready && sent_future.get() && callbacks==0,"signed limit altered actual single-frame delivery");
  std::promise<bool> stopped;auto done=stopped.get_future();server.stop([&](bool ok){stopped.set_value(ok);});check(done.wait_for(3s)==std::future_status::ready && done.get(),"signed limit stop stalled");thread.join();
 }
}

static void verify_priority_world_close_barrier(){
 asio::io_context socket_io,world_io;auto work=asio::make_work_guard(world_io);Strand world_strand(asio::make_strand(world_io));
 auto sessions=std::make_shared<session::Registry>();auto pool=std::make_shared<PriorityPoolFixture>(world_strand);net::WorldServerOptions options;options.port=0;
 std::promise<net::WorldChannel> channel_promise;auto channel_future=channel_promise.get_future();
 net::WorldServer server(socket_io,options,sessions,[&](const session::LoginSession&,net::WorldChannel channel){channel_promise.set_value(channel);return std::make_unique<IdentityApp>(std::move(channel));});
 server.start();std::jthread socket_thread([&]{socket_io.run();}),world_thread([&]{world_io.run();});
 std::promise<void> release_socket,release_world;auto socket_gate=release_socket.get_future().share(),world_gate=release_world.get_future().share();
 struct Cleanup{net::WorldServer& server;asio::io_context& socket_io;asio::io_context& world_io;std::promise<void>& a;std::promise<void>& b;bool ar=false,br=false;~Cleanup(){if(!ar)a.set_value();if(!br)b.set_value();server.stop();socket_io.stop();world_io.stop();}} cleanup{server,socket_io,world_io,release_socket,release_world};
 const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"priority close setup failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));
 check(channel_future.wait_for(3s)==std::future_status::ready,"priority close channel unavailable");auto channel=channel_future.get();peer.packet();peer.packet();
 std::promise<void> paused,entered,finished;auto paused_future=paused.get_future(),entered_future=entered.get_future(),finished_future=finished.get_future();std::atomic_bool result=true,early_complete=true;
 std::shared_ptr<net::WorldPriorityBatch> transfer;
 channel.pause_input([&](bool ok){if(ok)paused.set_value();socket_gate.wait();});check(paused_future.wait_for(3s)==std::future_status::ready,"priority close socket did not pause");channel.disconnect();
 std::promise<std::shared_ptr<net::WorldPriorityBatch>> transfer_promise;auto transfer_future=transfer_promise.get_future();
 asio::post(world_strand,[&]{game::UserPriorityQueue queue(*pool);for(auto& buffer:pool->buffers){buffer.references=1;buffer.frame={4,0,0x85,0x77};check(queue.enqueue(1,buffer,1,0),"priority close enqueue failed");game::release_party_membership_buffer(buffer,*pool);}
  auto batch=queue.take_batch();check(bool(batch),"priority close extraction failed");auto submitted=net::WorldPriorityBatch::submit(world_strand,pool,*batch,channel,[&](bool ok){result=ok;early_complete=transfer->complete();transfer->retry();entered.set_value();world_gate.wait();finished.set_value();});
  check(submitted && batch->empty(),"priority close batch rejected");transfer_promise.set_value(std::move(submitted));});
 check(transfer_future.wait_for(3s)==std::future_status::ready,"priority close transfer stalled");transfer=transfer_future.get();release_socket.set_value();cleanup.ar=true;
 check(entered_future.wait_for(3s)==std::future_status::ready && !result && !early_complete && pool->frees==2,"cancelled transfer did not release its references on world strand");
 std::promise<bool> stopped;auto stop_future=stopped.get_future();server.stop([&](bool ok){stopped.set_value(ok);});
 check(stop_future.wait_for(3s)==std::future_status::ready && stop_future.get() && finished_future.wait_for(100ms)==std::future_status::timeout,"transport stop did not remain separate from world completion");
 release_world.set_value();cleanup.br=true;check(finished_future.wait_for(3s)==std::future_status::ready,"world completion did not drain");
 std::promise<bool> complete_promise;auto complete_future=complete_promise.get_future();asio::post(world_strand,[&]{complete_promise.set_value(transfer->complete());});
 check(complete_future.wait_for(3s)==std::future_status::ready && complete_future.get(),"priority transfer did not complete");
 work.reset();socket_thread.join();world_thread.join();
}

struct RetainedAccount {};
struct CloseBarrierState {
 std::promise<void> entered,release;std::shared_future<void> gate=release.get_future().share();
};
struct CloseBarrierApp final:net::WorldApplication {
 std::shared_ptr<CloseBarrierState> state;net::WorldChannel channel;
 CloseBarrierApp(std::shared_ptr<CloseBarrierState> s,net::WorldChannel c):state(std::move(s)),channel(std::move(c)){}
 void open() override {channel.send({0x7a,0x77});}
 void receive(const net::Packet&) override {}
 void close() override {state->entered.set_value();state->gate.wait();}
};
static void verify_stop_close_barrier(){
 asio::io_context io;auto sessions=std::make_shared<session::Registry>();auto state=std::make_shared<CloseBarrierState>();net::WorldServerOptions options;options.port=0;options.maximum_connections=1;
 auto entered=state->entered.get_future();std::promise<void> done1,done2,done3;auto first=done1.get_future(),second=done2.get_future(),third=done3.get_future();
 net::WorldServer server(io,options,sessions,[state](const session::LoginSession&,net::WorldChannel channel){return std::make_unique<CloseBarrierApp>(state,std::move(channel));});
 server.start();std::jthread thread([&]{io.run();});
 struct Cleanup {net::WorldServer& server;asio::io_context& io;std::shared_ptr<CloseBarrierState> state;bool released=false;~Cleanup(){if(!released)state->release.set_value();server.stop();io.stop();}} cleanup{server,io,state};
 auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"barrier reservation failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));peer.packet();peer.packet();
 auto old_session=sessions->find(11);check(old_session && old_session->disconnect,"world disconnect callback unavailable");auto stale_disconnect=old_session->disconnect;
 asio::error_code ignored;peer.socket.close(ignored);entered.wait();
 auto replacement=account;replacement.session_id=102;sessions->add(replacement);
 check(sessions->select(11,102,0,now),"close-race replacement selection failed");
 auto blocked=sessions->claim_world(11,replacement.token,0,777,now);
 check(blocked.duplicate && !blocked.session,"application close lost its account reservation");
 // A blocked close keeps the connection object alive. Temporarily stop the IO
 // runner and drain cancellation handlers to expose any late callback posts.
 io.stop();thread.join();io.restart();io.poll();io.restart();
 for(unsigned i=0;i<100;++i)stale_disconnect();
 check(io.poll()==0,"closed world owner queued stale disconnects");io.restart();thread=std::jthread([&]{io.run();});
 PeerSocket rejected(server.port());rejected.closed();
 std::atomic_bool successful{true};server.stop([&](bool ok){if(!ok)successful=false;server.stop([&](bool nested){if(!nested)successful=false;done3.set_value();});done1.set_value();});server.stop([&](bool ok){if(!ok)successful=false;done2.set_value();});
 const bool held=first.wait_for(100ms)==std::future_status::timeout && second.wait_for(100ms)==std::future_status::timeout;
 state->release.set_value();cleanup.released=true;const bool completed=first.wait_for(3s)==std::future_status::ready && second.wait_for(3s)==std::future_status::ready && third.wait_for(3s)==std::future_status::ready;
 if(!completed)io.stop();thread.join();check(held && completed && successful,"stop did not wait for previously closing application");
 check(!sessions->world_reserved(11,0),"completed close retained account reservation");
}
struct FailingCloseApp final:net::WorldApplication {
 net::WorldChannel channel;explicit FailingCloseApp(net::WorldChannel c):channel(std::move(c)){}
 void open() override {channel.send({0x7a,0x77});}
 void receive(const net::Packet&) override {}
 void close() override {throw std::runtime_error("close failed");}
};
static void verify_stop_close_failure(){
 asio::io_context io;auto sessions=std::make_shared<session::Registry>();net::WorldServerOptions options;options.port=0;
 net::WorldServer server(io,options,sessions,[](const session::LoginSession&,net::WorldChannel channel){return std::make_unique<FailingCloseApp>(std::move(channel));});
 server.start();std::jthread thread([&]{io.run();});
 auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"failure barrier reservation failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));peer.packet();peer.packet();
 std::promise<bool> done,done2,done3;auto result=done.get_future(),result2=done2.get_future(),result3=done3.get_future();
 server.stop([&](bool ok){server.stop([&](bool nested){done3.set_value(nested);});done.set_value(ok);});
 server.stop([&](bool ok){done2.set_value(ok);});
 const bool completed=result.wait_for(3s)==std::future_status::ready && result2.wait_for(3s)==std::future_status::ready && result3.wait_for(3s)==std::future_status::ready;
 if(!completed)io.stop();thread.join();
 check(completed,"close-failure stop completion timed out");
 check(!result.get() && !result2.get() && !result3.get(),"close exception was reported as successful shutdown");
 check(server.application_failures()==1,"close exception failure count was not exactly one");
 auto failures=sessions->failed_world_reservations();check(failures.size()==1 && failures[0]->account()==11 && failures[0]->realm()==0,"failed close lost reservation");
 check(!sessions->adopt_world(failures[0]),"failed close token admitted another owner");
 auto replacement=account;replacement.session_id=102;sessions->add(replacement);check(sessions->select(11,102,0,now),"failure replacement select failed");
 auto duplicate=sessions->claim_world(11,replacement.token,0,777,now);check(duplicate.duplicate && !duplicate.session,"failed close admitted replacement");
 check(sessions->release_failed_world(failures[0]) && !sessions->release_failed_world(failures[0]),"failed reservation release identity changed");
 check(sessions->world_reserved(11,0),"recovery snapshot failed to retain reservation");failures.clear();
 auto admitted=sessions->claim_world(11,replacement.token,0,777,now);check(admitted.session && !admitted.duplicate,"recovered close failed to release reservation");

}
static void verify_factory_failure_fence(){
 asio::io_context io;auto sessions=std::make_shared<session::Registry>();net::WorldServerOptions options;options.port=0;
 std::promise<void> release;auto gate=release.get_future().share();
 net::WorldServer server(io,options,sessions,[gate](const session::LoginSession&,net::WorldChannel)->std::unique_ptr<net::WorldApplication>{gate.wait();throw std::runtime_error("factory failed");});
 server.start();std::jthread thread([&]{io.run();});
 struct Guard{net::WorldServer& server;asio::io_context& io;std::jthread& thread;std::promise<void>& release;bool released=false;~Guard(){if(!released)release.set_value();server.stop();io.stop();if(thread.joinable())thread.join();}} guard{server,io,thread,release};
 auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"factory-failure selection failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));peer.packet();release.set_value();guard.released=true;peer.closed();
 std::promise<bool> done;auto result=done.get_future();server.stop([&](bool ok){done.set_value(ok);});
 check(result.wait_for(3s)==std::future_status::ready && !result.get(),"factory failure was reported as successful shutdown");
 auto failures=sessions->failed_world_reservations();check(failures.size()==1 && failures[0]->account()==11,"factory failure lost reservation");
 auto replacement=account;replacement.session_id=102;sessions->add(replacement);check(sessions->select(11,102,0,now),"factory replacement selection failed");
 auto duplicate=sessions->claim_world(11,replacement.token,0,777,now);check(duplicate.duplicate && !duplicate.session,"factory failure admitted replacement");
 check(sessions->release_failed_world(failures[0]),"factory recovery release failed");failures.clear();
 auto admitted=sessions->claim_world(11,replacement.token,0,777,now);check(bool(admitted.session),"factory recovery left reservation fenced");
 thread.join();
}
struct RetainedWorldOwner {
 Strand strand;game::VerticalSessionRegistry<RetainedAccount> registry;
 explicit RetainedWorldOwner(Strand s):strand(s){}
 auto& sessions(){check(strand.running_in_this_thread(),"world owner accessed outside strand");return registry;}
};
struct LookupEvents {
 std::mutex mutex;std::condition_variable cv;unsigned calls=0;
 void received(){std::lock_guard lock(mutex);++calls;cv.notify_all();}
 void wait(unsigned count){std::unique_lock lock(mutex);check(cv.wait_for(lock,3s,[&]{return calls>=count;}),"retained lookup timed out");}
};
static void verify_retained_lookup(){
 asio::io_context io;Strand strand(asio::make_strand(io));auto state=std::make_shared<State>(strand);auto sessions=std::make_shared<session::Registry>();
 auto world=std::make_shared<RetainedWorldOwner>(strand);auto owners=std::shared_ptr<game::VerticalSessionRegistry<RetainedAccount>>(world,&world->registry);auto old=std::make_shared<RetainedAccount>();auto events=std::make_shared<LookupEvents>();
 net::WorldServerOptions options;options.port=0;
 auto lookup=net::retained_world_owner_lookup(strand,world);
 options.retained_world_lookup=[lookup,events](std::uint32_t account,std::function<void(std::optional<bool>)> done){
  lookup(account,[events,done=std::move(done)](std::optional<bool> live){done(live);if(live && !*live)done(std::nullopt);events->received();});
 };
 auto factory=game::serialized_world_factory(strand,[state](const session::LoginSession& account,net::WorldChannel channel){return std::make_unique<App>(*state,std::move(channel),account.user_id);});
 net::WorldServer server(io,options,sessions,std::move(factory));server.start();std::jthread thread([&]{io.run();});
 struct Guard{net::WorldServer& server;std::jthread& thread;~Guard(){server.stop();if(thread.joinable())thread.join();}} guard{server,thread};
 std::promise<void> reserved;auto ready=reserved.get_future();asio::post(strand,[&]{owners->reserve(11,old);check(owners->begin_database_work(11,*old),"old owner fence missing");reserved.set_value();});ready.get();
 auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession old_login{11,101,1,"native",{}, {}};old_login.token.fill(11);sessions->add(old_login);check(sessions->select(11,101,0,now),"old reservation failed");
 check(bool(sessions->claim(11,old_login.token,0,999,now)),"old login did not reach world");
 session::LoginSession replacement{11,102,1,"native",{}, {}};replacement.token.fill(12);sessions->add(replacement);check(sessions->select(11,102,0,now),"replacement reservation failed");
 PeerSocket peer(server.port());net::Packet short_connect{1,0xa3,11,0,0,0};peer.write(net::frame(short_connect));events->wait(1);
 net::Packet connect=short_connect;connect.insert(connect.end(),16,12);peer.write(net::frame(connect));events->wait(2);
 std::promise<void> released;auto release_result=released.get_future();asio::post(strand,[&]{check(!owners->release(11,*old),"live DB work lost its account fence");check(owners->finish_database_work(11,*old) && owners->release(11,*old),"old owner did not drain");released.set_value();});release_result.get();
 net::LegacyCipher incoming(net::SessionSecrets{}),outgoing(net::SessionSecrets{});net::Packet command{9,1,55};outgoing.crypt(command);
 auto stream=net::frame(connect);auto early=net::frame(command);stream.insert(stream.end(),early.begin(),early.end());peer.write(stream);
 auto greeting=peer.packet();incoming.crypt(greeting);check(greeting.size()==20 && greeting[0]==1 && greeting[1]==0xa3,"replacement was not admitted after real owner drain");
 auto opened=peer.packet();incoming.crypt(opened);check(opened==net::Packet({0x7a,0x77,11}),"replacement application did not open");
 auto response=peer.packet();incoming.crypt(response);check(response==net::Packet({0x7b,0x77,11}),"early command was lost during lookup");events->wait(3);
 server.stop();thread.join();check(state->opened==1 && state->received==1 && state->closed==1 && owners->size()==0 && server.application_failures()==0,"lookup/drain lifecycle failed");
}
static void verify_retained_lookup_delivery_failure(bool stop_instead){
 asio::io_context io;Strand strand(asio::make_strand(io));auto state=std::make_shared<State>(strand);
 auto sessions=std::make_shared<session::Registry>();auto world=std::make_shared<RetainedWorldOwner>(strand);auto events=std::make_shared<LookupEvents>();
 auto lookup=net::retained_world_owner_lookup(strand,world);std::atomic_bool inject{true};std::atomic_uint created{0};
 net::WorldServerOptions options;options.port=0;
 options.before_retained_lookup_delivery_post=[&]{if(inject.exchange(false))throw std::runtime_error("injected lookup delivery failure");};
 options.before_retained_lookup_close_post=[]{throw std::runtime_error("injected fallback close failure");};
 options.retained_world_lookup=[lookup,events](std::uint32_t account,std::function<void(std::optional<bool>)> done){
  lookup(account,[events,done=std::move(done)](std::optional<bool> live){done(live);events->received();});
 };
 auto factory=game::serialized_world_factory(strand,[state,&created](const session::LoginSession& account,net::WorldChannel channel){++created;return std::make_unique<App>(*state,std::move(channel),account.user_id);});
 net::WorldServer server(io,options,sessions,std::move(factory));server.start();std::jthread thread([&]{io.run();});
 struct Cleanup{net::WorldServer& server;asio::io_context& io;std::jthread& thread;~Cleanup(){server.stop();io.stop();if(thread.joinable())thread.join();}} cleanup{server,io,thread};
 const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"lookup delivery setup failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));events->wait(1);
 const auto deadline=std::chrono::steady_clock::now()+3s;while(server.pending_retained_lookup_delivery_failures()!=1 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
 check(server.pending_retained_lookup_delivery_failures()==1 && server.retained_lookup_delivery_failures()==1 && created==0 &&
  sessions->find(11)->location==session::LoginSession::Location::handoff,"failed lookup delivery consumed the actual handoff");
 std::promise<bool> finished;auto done=finished.get_future();
 if(stop_instead)server.stop([&](bool ok){finished.set_value(ok);});else server.retry_retained_lookup_delivery_failures();
 peer.closed();check(server.pending_retained_lookup_delivery_failures()==0 && created==0,"real socket close did not clear failed lookup delivery");
 if(stop_instead)check(done.wait_for(3s)==std::future_status::ready && done.get(),"stop did not drain failed lookup delivery");
 else {
  PeerSocket recovered(server.port());recovered.write(net::frame(connect));net::LegacyCipher incoming(net::SessionSecrets{});
  auto greeting=recovered.packet();incoming.crypt(greeting);auto opened=recovered.packet();incoming.crypt(opened);
  check(greeting.size()==20 && opened==net::Packet({0x7a,0x77,11}) && created==1,"fresh actual lookup did not admit the handoff");events->wait(2);
  server.stop([&](bool ok){finished.set_value(ok);});check(done.wait_for(3s)==std::future_status::ready && done.get(),"lookup recovery stop stalled");
 }
 thread.join();
}

static void verify_retained_lookup_delivery_auto_close(){
 asio::io_context io;Strand strand(asio::make_strand(io));auto sessions=std::make_shared<session::Registry>();auto world=std::make_shared<RetainedWorldOwner>(strand);
 auto lookup=net::retained_world_owner_lookup(strand,world);std::atomic_uint created{0};net::WorldServerOptions options;options.port=0;
 options.retained_world_lookup=lookup;options.before_retained_lookup_delivery_post=[]{throw std::runtime_error("injected lookup delivery failure");};
 net::WorldServer server(io,options,sessions,[&](const session::LoginSession&,net::WorldChannel)->std::unique_ptr<net::WorldApplication>{++created;return {};});
 server.start();std::jthread thread([&]{io.run();});struct Cleanup{net::WorldServer& server;asio::io_context& io;~Cleanup(){server.stop();io.stop();}} cleanup{server,io};
 const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"lookup auto-close setup failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));peer.closed();
 check(server.retained_lookup_delivery_failures()==1 && server.pending_retained_lookup_delivery_failures()==0 && created==0 &&
  sessions->find(11)->location==session::LoginSession::Location::handoff,"lookup fallback did not close the actual socket safely");
 std::promise<bool> finished;auto done=finished.get_future();server.stop([&](bool ok){finished.set_value(ok);});
 check(done.wait_for(3s)==std::future_status::ready && done.get(),"lookup fallback stop stalled");thread.join();
}

static void verify_retained_lookup_delivery_stop_race(){
 asio::io_context socket_io,owner_io;Strand strand(asio::make_strand(owner_io));auto owner_guard=asio::make_work_guard(owner_io);
 auto sessions=std::make_shared<session::Registry>();auto world=std::make_shared<RetainedWorldOwner>(strand);auto lookup=net::retained_world_owner_lookup(strand,world);
 std::promise<void> entered,release,returned;auto ready=entered.get_future();auto gate=release.get_future().share();auto returned_future=returned.get_future();std::atomic_uint created{0};
 net::WorldServerOptions options;options.port=0;
 options.before_retained_lookup_delivery_post=[&]{entered.set_value();gate.wait();throw std::runtime_error("injected in-flight delivery failure");};
 options.retained_world_lookup=[lookup,&returned](std::uint32_t account,std::function<void(std::optional<bool>)> done){
  lookup(account,[done=std::move(done),&returned](std::optional<bool> live){done(live);returned.set_value();});
 };
 net::WorldServer server(socket_io,options,sessions,[&](const session::LoginSession&,net::WorldChannel)->std::unique_ptr<net::WorldApplication>{++created;return {};});
 server.start();std::jthread socket_thread([&]{socket_io.run();});std::jthread owner_thread([&]{owner_io.run();});
 struct Cleanup{net::WorldServer& server;asio::io_context& socket_io;asio::io_context& owner_io;std::promise<void>& release;bool released=false;~Cleanup(){if(!released)release.set_value();server.stop();socket_io.stop();owner_io.stop();}} cleanup{server,socket_io,owner_io,release};
 const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"lookup race setup failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));
 check(ready.wait_for(3s)==std::future_status::ready,"owner lookup did not reach delivery boundary");
 std::promise<bool> finished;auto done=finished.get_future();server.stop([&](bool ok){finished.set_value(ok);});
 check(done.wait_for(100ms)==std::future_status::timeout,"stop completed while lookup delivery held the request");
 release.set_value();cleanup.released=true;
 check(returned_future.wait_for(3s)==std::future_status::ready,"lookup callback did not return");
 check(done.wait_for(3s)==std::future_status::ready && done.get(),"stop did not drain lookup race");peer.closed();
 check(server.retained_lookup_delivery_failures()==1 && server.pending_retained_lookup_delivery_failures()==0 && created==0 &&
  sessions->find(11)->location==session::LoginSession::Location::handoff,"lookup race lost close bookkeeping or consumed handoff");
 owner_guard.reset();socket_thread.join();owner_thread.join();
}

static void verify_retained_lookup_failure(){
 for(bool throws:{false,true}) {
  asio::io_context io;auto sessions=std::make_shared<session::Registry>();net::WorldServerOptions options;options.port=0;std::atomic_uint calls{0};
  options.retained_world_lookup=[throws](std::uint32_t,std::function<void(std::optional<bool>)> done){if(throws)throw 42;done(std::nullopt);};
  net::WorldServer server(io,options,sessions,[&](const session::LoginSession&,net::WorldChannel)->std::unique_ptr<net::WorldApplication>{++calls;return {};});
  server.start();std::jthread thread([&]{io.run();});struct Guard{net::WorldServer& server;std::jthread& thread;~Guard(){server.stop();if(thread.joinable())thread.join();}} guard{server,thread};
  auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
  session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"failure reservation failed");
  PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));peer.closed();
  check(calls==0 && sessions->find(11)->location==session::LoginSession::Location::handoff,"lookup failure admitted or consumed account");
  server.stop();thread.join();
 }
}
static void verify_known_live_duplicate(){
 asio::io_context io;auto sessions=std::make_shared<session::Registry>();net::WorldServerOptions options;options.port=0;
 std::atomic_uint lookups{0},ticks{0},applications{0};options.monotonic_ms=[&]{++ticks;return std::uint64_t{2};};
 options.retained_world_lookup=[&](std::uint32_t,std::function<void(std::optional<bool>)>){++lookups;throw 42;};
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,1),"live duplicate reservation failed");
 check(bool(sessions->claim(11,account.token,0,999,1)),"live duplicate claim failed");
 net::WorldServer server(io,options,sessions,[&](const session::LoginSession&,net::WorldChannel)->std::unique_ptr<net::WorldApplication>{++applications;return {};});
 server.start();std::jthread thread([&]{io.run();});struct Guard{net::WorldServer& server;std::jthread& thread;~Guard(){server.stop();if(thread.joinable())thread.join();}} guard{server,thread};
 PeerSocket peer(server.port());net::Packet request{1,0xa3,11,0,0,0};auto framed=net::frame(request);net::Packet burst;for(int i=0;i<10;++i)burst.insert(burst.end(),framed.begin(),framed.end());peer.write(burst);
 const auto deadline=std::chrono::steady_clock::now()+3s;while(ticks<11 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
 std::uint8_t byte;asio::error_code error;peer.socket.read_some(asio::buffer(&byte,1),error);
 check(ticks>=11 && (error==asio::error::would_block || error==asio::error::try_again),"known duplicate closed before limit");
 peer.write(framed);peer.closed();check(lookups==0 && applications==0 && sessions->find(11)->location==session::LoginSession::Location::world,"known duplicate used unavailable lookup or changed owner");
 server.stop();thread.join();
}
static void verify_late_retained_lookup(){
 asio::io_context io;auto sessions=std::make_shared<session::Registry>();net::WorldServerOptions options;options.port=0;std::atomic_uint calls{0};
 std::promise<std::function<void(std::optional<bool>)>> pending;auto lookup=pending.get_future();
 options.retained_world_lookup=[&](std::uint32_t,std::function<void(std::optional<bool>)> done){pending.set_value(std::move(done));};
 net::WorldServer server(io,options,sessions,[&](const session::LoginSession&,net::WorldChannel)->std::unique_ptr<net::WorldApplication>{++calls;return {};});
 server.start();std::jthread thread([&]{io.run();});
 auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"late-lookup reservation failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));auto complete=lookup.get();
 asio::error_code ignored;peer.socket.close(ignored);server.stop();thread.join();complete(false);io.restart();io.poll();
 check(calls==0 && sessions->find(11)->location==session::LoginSession::Location::handoff,"late lookup revived a closed connection");
}
// The actual selected owner must keep the network reservation while its
// selection and disconnect services drain, even after transport stop.
struct ReservationAdmission final:game::WorldAdmissionServices {
 void packets(game::WorldAdmissionUser&) override {throw std::logic_error("unexpected admission packets");}
 game::RuntimeZone* select_zone(game::WorldAdmissionUser&) override {throw std::logic_error("unexpected zone selection");}
 void register_world(game::WorldAdmissionUser&) override {throw std::logic_error("unexpected registration");}
 bool enter_zone(game::WorldAdmissionUser&,game::RuntimeZone&) override {throw std::logic_error("unexpected entry");}
 void close(game::WorldAdmissionUser&,std::uint32_t) override {throw std::logic_error("unexpected admission close");}
 void diagnostic(game::WorldAdmissionUser&,game::WorldAdmissionError) override {throw std::logic_error("unexpected diagnostic");}
 std::optional<std::uint32_t> rank_window(game::RuntimeZone&) override {throw std::logic_error("unexpected rank");}
 bool lock_guild(game::WorldAdmissionUser&) override {throw std::logic_error("unexpected guild lock");}
 void unlock_guild(game::WorldAdmissionUser&) override {throw std::logic_error("unexpected guild unlock");}
};
struct ReservationServices final:game::VerticalWorldSessionServices {
 std::promise<std::function<void()>> disconnect_started;
 const game::ActorLoadRules& actor_rules() override {throw std::logic_error("unexpected actor rules");}
 game::ActorLoadOptions actor_options(const game::PlayerSnapshot&) override {throw std::logic_error("unexpected actor options");}
 game::VerticalWorldActorScene actor_scene(game::VerticalWorldSession&,const game::PlayerSnapshot&) override {throw std::logic_error("unexpected scene");}
 void configure_admission(game::VerticalWorldSession&,game::WorldAdmissionUser&,const game::PlayerSnapshot&) override {throw std::logic_error("unexpected admission");}
 void prepare(game::VerticalWorldSession&,std::function<void(std::exception_ptr)>) override {throw std::logic_error("unexpected prepare");}
 void cancel_preparation(game::VerticalWorldSession&,std::function<void()>) override {throw std::logic_error("unexpected cancel");}
 game::CharacterEntryState entry_state(game::VerticalWorldSession&) override {throw std::logic_error("unexpected entry state");}
 const game::SocialCatalog& social(game::VerticalWorldSession&) override {throw std::logic_error("unexpected social");}
 game::ExperienceBounds experience(game::VerticalWorldSession&) override {throw std::logic_error("unexpected experience");}
 std::string guild_name(game::VerticalWorldSession&) override {throw std::logic_error("unexpected guild");}
 game::VerticalProtocolServices protocol(game::VerticalWorldSession&) override {throw std::logic_error("unexpected protocol");}
 void waiting(game::VerticalWorldSession&) override {throw std::logic_error("unexpected waiting");}
 void receive(game::VerticalWorldSession&,const net::Packet&) override {throw std::logic_error("unexpected receive");}
 void receive_nprotect(game::VerticalWorldSession&,const net::Packet&) override {throw std::logic_error("unexpected nprotect");}
 void unavailable(game::VerticalWorldSession&) override {throw std::logic_error("cancelled selection delivered");}
 void failure(game::VerticalWorldSession&,std::exception_ptr) override {throw std::logic_error("unexpected session failure");}
 void disconnect(game::VerticalWorldSession&,std::optional<std::uint32_t>,std::function<void()> done) override {disconnect_started.set_value(std::move(done));}
};
struct BlockingIoRelease {
 std::shared_ptr<void> actual;std::promise<void>& entered;std::shared_future<void> gate;
 BlockingIoRelease(std::shared_ptr<void> value,std::promise<void>& event,std::shared_future<void> release)
  :actual(std::move(value)),entered(event),gate(std::move(release)) {}
 ~BlockingIoRelease(){entered.set_value();gate.wait();}
};
struct SessionRegistryOwner {
 game::VerticalWorldSession::Registry value;game::WorldAdmission* admission=nullptr;bool stopped=false;
 game::VerticalWorldSession::Registry& registry(){if(stopped)throw std::logic_error("owner stopped");return value;}
 void shutdown(){if(stopped || !admission || value.size()!=0 || admission->selection_count()!=0)throw std::logic_error("owner not drained");stopped=true;}
};
static std::shared_ptr<const void> bind_owner(asio::io_context& io,Strand strand,SessionRegistryOwner& owner,const std::shared_ptr<session::Registry>& sessions){
 std::promise<std::shared_ptr<const void>> bound;auto ready=bound.get_future();
 asio::post(strand,[&]{try{owner.registry().bind_network_registry(sessions,0);bound.set_value(owner.registry().owner_identity());}catch(...){bound.set_exception(std::current_exception());}});
 io.poll();return ready.get();
}
static void verify_real_session_reservation(bool use_owner_stop){
 asio::io_context io;auto work=asio::make_work_guard(io);Strand strand(asio::make_strand(io));asio::thread_pool database(1);game::DbJobDispatcher dispatcher(false,1);
 auto sessions=std::make_shared<session::Registry>();auto world_owner=std::make_shared<SessionRegistryOwner>();
 std::weak_ptr<SessionRegistryOwner> weak_owner=world_owner;auto& owners=world_owner->registry();
 ReservationAdmission admission_services;game::WorldAdmission admission(admission_services);world_owner->admission=&admission;ReservationServices services;
 std::promise<void> load_started,release_load,db_started,release_db,db_delivered,db_second_delivered,db_registered,lease_releasing,release_lease;
 auto started=load_started.get_future();auto gate=release_load.get_future().share();
 auto releasing=lease_releasing.get_future();auto lease_gate=release_lease.get_future().share();
 auto tracked_started=db_started.get_future(),tracked_delivered=db_delivered.get_future(),tracked_second=db_second_delivered.get_future(),tracked_registered=db_registered.get_future();auto db_gate=release_db.get_future().share();
 using Tracker=game::VerticalTrackedDbJobs<game::VerticalWorldSession>;using Joined=game::VerticalShutdownDrain<game::VerticalWorldSession>;
 std::shared_ptr<Tracker> tracker;std::shared_ptr<Joined> joined;std::shared_ptr<net::WorldOwnerStop<SessionRegistryOwner>> owner_stop;unsigned deliveries=0;
 auto disconnect=services.disconnect_started.get_future();
 auto select=game::vertical_world_session_select(strand,database.get_executor(),owners,admission,services,
  [&](const session::LoginSession&,const game::CharacterRoster&){return [&,gate](std::uint32_t,std::uint32_t)->std::optional<game::VerticalPersistenceSelection>{load_started.set_value();gate.wait();return {};};});
 auto factory=game::roster_world_serialized_factory(strand,
  [](const session::LoginSession&){game::CharacterRoster roster;roster.country=0;game::CharacterSummary character;character.id=42;character.slot=0;roster.slots[0]=character;return roster;},
  [](const session::LoginSession&,game::CharacterRoster&,const net::Packet&)->game::CharacterReply {throw std::logic_error("unexpected roster command");},
  [&,select=std::move(select)](const session::LoginSession& account,net::WorldChannel channel,game::CharacterRoster roster) mutable {
   for(unsigned field=0;field<6;++field) {
    auto wrong=account;
    switch(field) {case 0:++wrong.user_id;break;case 1:++wrong.realm;break;case 2:++wrong.session_id;break;
     case 3:++wrong.world_connection;break;case 4:wrong.location=session::LoginSession::Location::login;break;
     default:wrong.world_reservation.reset();}
    bool rejected=false;try {(void)game::VerticalWorldSession::create(strand,database.get_executor(),owners,admission,services,wrong,channel,roster,
      [](std::uint32_t,std::uint32_t)->std::optional<game::VerticalPersistenceSelection>{throw std::logic_error("unexpected selection");});}
    catch(const std::invalid_argument&){rejected=true;}
    check(rejected && owners.size()==0,"selected owner accepted mismatched claim identity");
   }
   // A different registry can issue the same numeric owner identity. Its
   // live token must still be rejected before the real owner is registered.
   session::Registry foreign;auto foreign_login=account;foreign.add(foreign_login);
   check(foreign.select(account.user_id,account.session_id,account.realm,1),"foreign login selection failed");
   auto foreign_claim=foreign.claim_world(account.user_id,account.token,account.realm,account.world_connection,2);
   check(bool(foreign_claim.session),"foreign claim failed");bool rejected=false;
   try {(void)game::VerticalWorldSession::create(strand,database.get_executor(),owners,admission,services,*foreign_claim.session,channel,roster,
      [](std::uint32_t,std::uint32_t)->std::optional<game::VerticalPersistenceSelection>{throw std::logic_error("unexpected selection");});}
   catch(const std::invalid_argument&){rejected=true;}
   check(rejected && owners.size()==0,"selected owner accepted another registry's matching claim");
   session::LoginSession other{12,211,1,"other",{}, {}};other.token.fill(12);sessions->add(other);
   check(sessions->select(12,211,1,1),"other realm handoff failed");
   auto other_claim=sessions->claim_world(12,other.token,1,311,2);check(bool(other_claim.session),"other realm claim failed");
   bool wrong_realm=false;try {(void)game::VerticalWorldSession::create(strand,database.get_executor(),owners,admission,services,*other_claim.session,channel,roster,
      [](std::uint32_t,std::uint32_t)->std::optional<game::VerticalPersistenceSelection>{throw std::logic_error("unexpected selection");});}
   catch(const std::invalid_argument&){wrong_realm=true;}
   check(wrong_realm && owners.size()==0 && sessions->world_adoptions().empty(),"selected owner accepted or consumed another realm's claim");
   other_claim.session.reset();check(sessions->release_world(12,211,311),"other realm release failed");
   // Wrap the real transport lease so the test can pause its destruction
   // after registry removal and race the next network claim against it.
   auto retain=channel.retain_io;
   channel.retain_io=[retain,&lease_releasing,lease_gate]() -> std::shared_ptr<void> {
    auto actual=retain();if(!actual)return {};
    return std::make_shared<BlockingIoRelease>(std::move(actual),lease_releasing,lease_gate);
   };
   return select(account,std::move(channel),std::move(roster));
  });
 net::WorldServerOptions options;options.port=0;options.owner_identity=bind_owner(io,strand,*world_owner,sessions);auto server=std::make_shared<net::WorldServer>(io,options,sessions,std::move(factory));server->start();std::jthread thread([&]{io.run();});
 struct Guard {net::WorldServer& server;asio::io_context& io;asio::thread_pool& db;game::DbJobDispatcher& dispatcher;std::jthread& thread;std::promise<void>& release;std::promise<void>& release_job;std::promise<void>& release_io;bool released=false,job_released=false,io_released=false;
  ~Guard(){if(!released)release.set_value();if(!job_released)release_job.set_value();if(!io_released)release_io.set_value();server.stop();io.stop();if(thread.joinable())thread.join();db.join();dispatcher.shutdown();}} guard{*server,io,database,dispatcher,thread,release_load,release_db,release_lease};
 const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"real owner selection setup failed");
 PeerSocket peer(server->port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));
 net::LegacyCipher incoming(net::SessionSecrets{}),outgoing(net::SessionSecrets{});auto greeting=peer.packet();incoming.crypt(greeting);
 for(unsigned i=0;i<6;++i){auto packet=peer.packet();incoming.crypt(packet);}
 net::Packet selection{4,1,42,0,0,0};outgoing.crypt(selection);peer.write(net::frame(selection));
 check(started.wait_for(3s)==std::future_status::ready,"actual owner selection did not start");
 // Submit a real tracked job before stopping its producer. The held selection
 // and this separate worker both belong to the same actual selected session.
 asio::post(strand,[&]{try {
  auto existing=owners.find_account(11);check(bool(existing),"actual owner missing for tracked work");
  tracker=Tracker::create(strand,owners,existing,11,dispatcher,
   [&,db_gate](game::VerticalWorldSession& session,std::uint32_t job,const Tracker::Retry&){check(session.account_id()==11 && (job==1 || job==2),"wrong tracked owner");if(job==1){db_started.set_value();db_gate.wait();return std::vector<net::Packet>{{3,4}};}return std::vector<net::Packet>{{5,6}};},
   [&](game::VerticalWorldSession& session,std::vector<net::Packet> packets){try {check(session.account_id()==11,"tracked reply lost owner");++deliveries;if(packets==std::vector<net::Packet>{{3,4}})db_delivered.set_value();else {check(packets==std::vector<net::Packet>{{5,6}},"tracked reply changed");db_second_delivered.set_value();}}catch(...){if(deliveries<2)db_delivered.set_exception(std::current_exception());else db_second_delivered.set_exception(std::current_exception());}},
   [&](std::exception_ptr error){db_delivered.set_exception(error);});
  check(tracker->submit(1),"tracked job was not accepted");db_registered.set_value();
 }catch(...){db_registered.set_exception(std::current_exception());}});
 check(tracked_registered.wait_for(3s)==std::future_status::ready,"tracked job registration stalled");tracked_registered.get();
 check(tracked_started.wait_for(3s)==std::future_status::ready,"tracked job did not start");
 std::promise<bool> stopped;auto stop=stopped.get_future();
 std::promise<void> drain_registered,owners_empty;auto registered=drain_registered.get_future();auto empty=owners_empty.get_future();bool disconnect_returned=false;
 auto completed=[&]{try {check(!weak_owner.expired() && disconnect_returned && deliveries==(use_owner_stop?2U:1U) && owners.size()==0 && admission.selection_count()==0 && !sessions->world_reserved(11,0),"drain lost owner or signaled before actual work returned");owners_empty.set_value();}catch(...){owners_empty.set_exception(std::current_exception());}};
 if(use_owner_stop) {
  asio::post(strand,[&]{try {
   auto foreign=std::make_shared<SessionRegistryOwner>();foreign->admission=&admission;foreign->registry().bind_network_registry(sessions,0);
   bool rejected=false;try{(void)net::WorldOwnerStop<SessionRegistryOwner>::create(strand,server,foreign,completed,[](std::exception_ptr){});}catch(const std::invalid_argument&){rejected=true;}
   check(rejected && !foreign->stopped && owners.size()==1 && !owners.admission_closed(),"foreign owner stop changed a live registry");
   // Even trusted marker metadata cannot make a foreign issuer or realm match.
   for(unsigned mismatch=0;mismatch<2;++mismatch) {
    net::WorldServerOptions wrong_options;wrong_options.port=0;wrong_options.owner_identity=owners.owner_identity();wrong_options.realm=mismatch==1?1:0;
    auto issuer=mismatch==0?std::make_shared<session::Registry>():sessions;
    auto wrong_transport=std::make_shared<net::WorldServer>(io,wrong_options,issuer,[](const session::LoginSession&,net::WorldChannel)->std::unique_ptr<net::WorldApplication>{throw std::logic_error("unexpected connection");});
    bool wrong_rejected=false;try{(void)net::WorldOwnerStop<SessionRegistryOwner>::create(strand,wrong_transport,world_owner,completed,[](std::exception_ptr){});}catch(const std::invalid_argument&){wrong_rejected=true;}
    check(wrong_rejected && owners.size()==1 && !owners.admission_closed(),"foreign transport binding changed a live registry");
   }
   owner_stop=net::WorldOwnerStop<SessionRegistryOwner>::create(strand,server,world_owner,completed,[&](std::exception_ptr e){owners_empty.set_exception(e);});
   check(owner_stop->start(),"owner stop failed to start");drain_registered.set_value();
  }catch(...){drain_registered.set_exception(std::current_exception());}});
  check(registered.wait_for(3s)==std::future_status::ready,"owner stop registration stalled");registered.get();
  server->stop([&](bool ok){stopped.set_value(ok);});check(stop.wait_for(3s)==std::future_status::ready && stop.get(),"transport did not close");
  std::promise<void> installed;auto installed_future=installed.get_future();asio::post(strand,[&]{try {check(owners.admission_closed() && !owner_stop->error() && tracker->submit(2),"owner stop blocked required post-close tracked work");installed.set_value();}catch(...){installed.set_exception(std::current_exception());}});
  check(installed_future.wait_for(3s)==std::future_status::ready,"owner stop marker stalled");installed_future.get();
 } else {
  server->stop([&](bool ok){stopped.set_value(ok);});check(stop.wait_for(3s)==std::future_status::ready && stop.get(),"transport did not close");
  asio::post(strand,[&]{try {
   auto existing=owners.find_account(11);check(bool(existing),"actual owner missing at shutdown");
   joined=Joined::create_for_owner(strand,world_owner,{tracker},completed);
   check(joined->start() && !joined->error() && !tracker->submit(2),"joined drain did not stop its tracked producer");
   bool rejected=false;try {owners.reserve(12,existing);} catch(const std::logic_error&){rejected=true;}
   check(rejected && !owners.drain_notification_error(),"shutdown admitted a new owner or lost notification");drain_registered.set_value();
  }catch(...){drain_registered.set_exception(std::current_exception());}});
  check(registered.wait_for(3s)==std::future_status::ready,"session drain registration stalled");registered.get();
 }
 world_owner.reset();check(!weak_owner.expired(),"drain lost its registry owner");
 check(empty.wait_for(0s)==std::future_status::timeout,"session drain ignored active selection");
 auto replacement=account;replacement.session_id=102;sessions->add(replacement);check(sessions->select(11,102,0,now),"replacement handoff failed");
 check(sessions->claim_world(11,replacement.token,0,777,now).duplicate,"selection drain lost reservation");
 release_load.set_value();guard.released=true;
 check(disconnect.wait_for(3s)==std::future_status::ready,"actual owner did not start disconnect");auto finish=disconnect.get();
 check(sessions->claim_world(11,replacement.token,0,777,now).duplicate && empty.wait_for(0s)==std::future_status::timeout,"disconnect drain lost reservation or signaled early");
 release_db.set_value();guard.job_released=true;
 check(tracked_delivered.wait_for(3s)==std::future_status::ready,"tracked reply did not arrive");tracked_delivered.get();
 if(use_owner_stop){check(tracked_second.wait_for(3s)==std::future_status::ready,"post-close tracked reply did not arrive");tracked_second.get();}
 std::promise<void> tracker_drained;auto tracked_done=tracker_drained.get_future();asio::post(strand,[&]{try {check(owners.pending_database_work(11)==0 && empty.wait_for(0s)==std::future_status::timeout,"joined drain ignored retained disconnect");if(use_owner_stop)tracker->stop_producers();tracker_drained.set_value();}catch(...){tracker_drained.set_exception(std::current_exception());}});
 check(tracked_done.wait_for(3s)==std::future_status::ready,"tracked drain stalled");tracked_done.get();
 std::promise<void> drained;auto complete=drained.get_future();asio::post(strand,[&]{try {finish();check(owners.size()==0 && admission.selection_count()==0,"actual owner failed to untrack");disconnect_returned=true;drained.set_value();}catch(...){drained.set_exception(std::current_exception());}});
 check(releasing.wait_for(3s)==std::future_status::ready,"IO lease release did not start");
 check(sessions->claim_world(11,replacement.token,0,777,now).duplicate && empty.wait_for(0s)==std::future_status::timeout,"replacement overtook final IO lease release");
 release_lease.set_value();guard.io_released=true;
 check(complete.wait_for(3s)==std::future_status::ready,"actual owner did not drain");complete.get();
 check(empty.wait_for(3s)==std::future_status::ready,"session drain notification did not arrive");empty.get();
 auto next=sessions->claim_world(11,replacement.token,0,777,now);check(bool(next.session),"replacement remained fenced after owner drain");
 dispatcher.wait_idle();std::promise<void> cleanup;auto cleaned=cleanup.get_future();asio::post(strand,[&]{try {if(use_owner_stop)check(!owner_stop->error(),"owner stop reported an error");finish={};tracker.reset();joined.reset();owner_stop.reset();check(weak_owner.expired(),"drained registry owner leaked");cleanup.set_value();}catch(...){cleanup.set_exception(std::current_exception());}});
 check(cleaned.wait_for(3s)==std::future_status::ready,"owner cleanup stalled");cleaned.get();work.reset();thread.join();database.join();
}
struct CloseSubmissionRaceState {
 std::promise<void> entered,release,closed;std::shared_future<void> gate=release.get_future().share();
};
struct CloseSubmissionRaceApp final:net::WorldApplication {
 std::shared_ptr<CloseSubmissionRaceState> state;net::WorldChannel channel;
 CloseSubmissionRaceApp(std::shared_ptr<CloseSubmissionRaceState> s,net::WorldChannel c):state(std::move(s)),channel(std::move(c)){}
 void open() override {channel.send({0x7a,0x77});}
 void receive(const net::Packet&) override {state->entered.set_value();state->gate.wait();}
 void close() override {state->closed.set_value();}
};
static void verify_close_submission_behind_worker(){
 asio::io_context io;auto sessions=std::make_shared<session::Registry>();auto state=std::make_shared<CloseSubmissionRaceState>();
 auto entered=state->entered.get_future(),closed=state->closed.get_future();std::atomic_bool inject{true};
 net::WorldServerOptions options;options.port=0;options.database_workers=1;
 options.before_close_submission_post=[&]{if(inject.exchange(false))throw std::runtime_error("injected worker submission failure");};
 net::WorldServer server(io,options,sessions,[state](const session::LoginSession&,net::WorldChannel channel){return std::make_unique<CloseSubmissionRaceApp>(state,std::move(channel));});
 server.start();std::jthread thread([&]{io.run();});
 struct Cleanup{net::WorldServer& server;asio::io_context& io;std::jthread& thread;std::shared_ptr<CloseSubmissionRaceState> state;bool released=false;
  ~Cleanup(){if(!released)state->release.set_value();server.stop();io.stop();if(thread.joinable())thread.join();}} cleanup{server,io,thread,state};
 const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"worker close race handoff failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));peer.packet();peer.packet();
 net::LegacyCipher outgoing(net::SessionSecrets{});net::Packet selection{4,1,42,0,0,0};outgoing.crypt(selection);peer.write(net::frame(selection));
 check(entered.wait_for(3s)==std::future_status::ready,"worker close race did not enter actual receive");
 std::promise<bool> finished;auto done=finished.get_future();server.stop([&](bool ok){finished.set_value(ok);});peer.closed();
 const auto fail_deadline=std::chrono::steady_clock::now()+3s;
 while(server.pending_close_submissions()!=1 && std::chrono::steady_clock::now()<fail_deadline)std::this_thread::sleep_for(1ms);
 check(server.pending_close_submissions()==1 && sessions->world_reserved(11,0),"failed submission lost in-flight owner");
 server.retry_close_submissions();const auto retry_deadline=std::chrono::steady_clock::now()+3s;
 while(server.pending_close_submissions()!=0 && std::chrono::steady_clock::now()<retry_deadline)std::this_thread::sleep_for(1ms);
 check(server.pending_close_submissions()==0 && closed.wait_for(100ms)==std::future_status::timeout &&
  done.wait_for(100ms)==std::future_status::timeout && sessions->world_reserved(11,0),"retry overtook the active worker or released its reservation");
 state->release.set_value();cleanup.released=true;
 check(closed.wait_for(3s)==std::future_status::ready && done.wait_for(3s)==std::future_status::ready && done.get() &&
  !sessions->world_reserved(11,0),"real worker close did not drain after receive returned");
 thread.join();
}

struct ReasonCloseState {std::promise<net::WorldChannel> opened;std::promise<std::optional<std::uint32_t>> reason;};
struct ReasonCloseApp final:net::WorldApplication {
 std::shared_ptr<ReasonCloseState> state;net::WorldChannel channel;
 ReasonCloseApp(std::shared_ptr<ReasonCloseState> s,net::WorldChannel c):state(std::move(s)),channel(std::move(c)){}
 void open() override {state->opened.set_value(channel);channel.send({0x7a,0x77});}
 void receive(const net::Packet&) override {}
 void close() override {state->reason.set_value({});}
 void close_with_reason(std::optional<std::uint32_t> reason) override {state->reason.set_value(reason);}
};
static void verify_close_submission_reason(){
 asio::io_context io;auto sessions=std::make_shared<session::Registry>();auto state=std::make_shared<ReasonCloseState>();
 auto opened=state->opened.get_future();auto reason=state->reason.get_future();std::atomic_bool inject{true};
 net::WorldServerOptions options;options.port=0;options.before_close_submission_post=[&]{if(inject.exchange(false))throw std::runtime_error("injected close submission failure");};
 net::WorldServer server(io,options,sessions,[state](const session::LoginSession&,net::WorldChannel channel){return std::make_unique<ReasonCloseApp>(state,std::move(channel));});
 server.start();std::jthread thread([&]{io.run();});
 struct Cleanup{net::WorldServer& server;asio::io_context& io;std::jthread& thread;~Cleanup(){server.stop();io.stop();if(thread.joinable())thread.join();}} cleanup{server,io,thread};
 const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"reason test handoff failed");
 PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));peer.packet();peer.packet();
 check(opened.wait_for(3s)==std::future_status::ready,"reason test application did not open");auto channel=opened.get();channel.disconnect_with_reason(111);peer.closed();
 const auto deadline=std::chrono::steady_clock::now()+3s;while(server.pending_close_submissions()!=1 && std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(1ms);
 check(server.pending_close_submissions()==1 && sessions->world_reserved(11,0) && reason.wait_for(0s)==std::future_status::timeout,"failed reason submission lost reservation or invoked close");
 std::promise<bool> finished;auto done=finished.get_future();server.stop([&](bool ok){finished.set_value(ok);});
 check(done.wait_for(100ms)==std::future_status::timeout,"reason test stop completed without worker close");server.retry_close_submissions();
 check(reason.wait_for(3s)==std::future_status::ready && reason.get()==std::optional<std::uint32_t>{111},"worker retry changed close reason");
 check(done.wait_for(3s)==std::future_status::ready && done.get() && server.pending_close_submissions()==0 && !sessions->world_reserved(11,0),"reason test did not drain actual close");
 thread.join();
}

static void verify_owner_stop_transport_failure(){
 asio::io_context io;auto work=asio::make_work_guard(io);Strand strand(asio::make_strand(io));
 auto sessions=std::make_shared<session::Registry>();auto owner=std::make_shared<SessionRegistryOwner>();
 ReservationAdmission admission_services;game::WorldAdmission admission(admission_services);owner->admission=&admission;
 net::WorldServerOptions options;options.port=0;options.owner_identity=bind_owner(io,strand,*owner,sessions);
 std::atomic_uint submit_attempts{0};std::atomic_bool inject_notice{true};
 options.before_close_submission_post=[&]{if(++submit_attempts<=2)throw std::runtime_error("injected close submission failure");};
 options.before_close_notification_post=[&]{if(inject_notice.exchange(false))throw std::runtime_error("injected close notification failure");};
 auto server=std::make_shared<net::WorldServer>(io,options,sessions,[](const session::LoginSession&,net::WorldChannel channel){return std::make_unique<FailingCloseApp>(std::move(channel));});
 server->start();std::jthread thread([&]{io.run();});
 struct Cleanup{net::WorldServer& server;asio::io_context& io;std::jthread& thread;~Cleanup(){server.stop();io.stop();if(thread.joinable())thread.join();}} cleanup{*server,io,thread};
 const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(11);sessions->add(account);check(sessions->select(11,101,0,now),"owner stop failure setup failed");
 PeerSocket peer(server->port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),16,11);peer.write(net::frame(connect));peer.packet();peer.packet();
 using Stop=net::WorldOwnerStop<SessionRegistryOwner>;std::shared_ptr<Stop> stop;std::promise<std::exception_ptr> failure;auto failed=failure.get_future();std::atomic_bool completed=false;
 std::promise<void> started;auto start=started.get_future();asio::post(strand,[&]{try {
  stop=Stop::create(strand,server,owner,[&]{completed=true;},[&](std::exception_ptr error){failure.set_value(error);});
  check(stop->start(),"owner stop failure test did not start");started.set_value();
 }catch(...){started.set_exception(std::current_exception());}});start.get();
 const auto submission_deadline=std::chrono::steady_clock::now()+3s;
 while(server->pending_close_submissions()!=1 && std::chrono::steady_clock::now()<submission_deadline)std::this_thread::sleep_for(1ms);
 check(server->pending_close_submissions()==1 && server->close_submission_failures()==1 && server->application_failures()==0 &&
  sessions->world_reserved(11,0),"failed worker submission lost owner or ran a synthetic close");
 check(failed.wait_for(100ms)==std::future_status::timeout && !completed,"owner stop completed before real close submission");
 auto retry_owner=[&](std::size_t submissions,std::size_t notifications){
  std::promise<void> retried;auto ready=retried.get_future();asio::post(strand,[&]{try {
   const auto pending=stop->transport_pending();check(pending.submissions==submissions && pending.notifications==notifications,"owner stop lost transport pending state");
   stop->retry();retried.set_value();
  }catch(...){retried.set_exception(std::current_exception());}});
  check(ready.wait_for(3s)==std::future_status::ready,"owner transport retry stalled");ready.get();
 };
 retry_owner(1,0);
 const auto retry_deadline=std::chrono::steady_clock::now()+3s;
 while(server->close_submission_failures()!=2 && std::chrono::steady_clock::now()<retry_deadline)std::this_thread::sleep_for(1ms);
 check(server->pending_close_submissions()==1 && server->close_submission_failures()==2 && server->application_failures()==0 &&
  sessions->world_reserved(11,0),"failed worker retry lost retained ownership");
 retry_owner(1,0);
 const auto post_deadline=std::chrono::steady_clock::now()+3s;
 while(server->pending_close_notifications()!=1 && std::chrono::steady_clock::now()<post_deadline)std::this_thread::sleep_for(1ms);
 check(server->pending_close_submissions()==0 && server->close_submission_failures()==2 && server->pending_close_notifications()==1 &&
  server->close_notification_failures()==1 && server->application_failures()==1,"actual close notification failure was not retained");
 check(failed.wait_for(100ms)==std::future_status::timeout && !completed,"owner stop completed without the actual close notification");
 auto before_retry=sessions->failed_world_reservations();check(before_retry.size()==1 && before_retry[0]->account()==11,"close failure lost the real reservation before retry");before_retry.clear();
 retry_owner(0,1);
 check(failed.wait_for(3s)==std::future_status::ready,"owner stop did not report transport failure");auto error=failed.get();check(bool(error),"owner stop lost transport error");
 check(server->pending_close_notifications()==0 && server->close_notification_failures()==1,"close notification retry lost its failure history");
 peer.closed();std::promise<void> inspected;auto done=inspected.get_future();asio::post(strand,[&]{try {
  check(!completed && !owner->stopped && stop->error() && server->application_failures()==1,"failed transport reported owner success");
  bool rejected=false;try{stop->retry();}catch(const std::logic_error&){rejected=true;}check(rejected && !owner->stopped,"failed transport was converted into successful retry");
  auto fences=sessions->failed_world_reservations();check(fences.size()==1 && fences[0]->account()==11,"failed transport lost recovery fence");
  stop.reset();owner.reset();inspected.set_value();
 }catch(...){inspected.set_exception(std::current_exception());}});
 check(done.wait_for(3s)==std::future_status::ready,"owner stop failure inspection stalled");done.get();work.reset();thread.join();
}

static void verify_owner_stop_post_retry_race(){
 asio::io_context socket_io,world_io;auto work=asio::make_work_guard(world_io);Strand strand(asio::make_strand(world_io));
 auto sessions=std::make_shared<session::Registry>();auto owner=std::make_shared<SessionRegistryOwner>();
 ReservationAdmission admission_services;game::WorldAdmission admission(admission_services);owner->admission=&admission;
 net::WorldServerOptions options;options.port=0;options.owner_identity=bind_owner(world_io,strand,*owner,sessions);
 auto server=std::make_shared<net::WorldServer>(socket_io,options,sessions,[](const session::LoginSession&,net::WorldChannel)->std::unique_ptr<net::WorldApplication>{throw std::logic_error("unexpected connection");});
 std::promise<void> entered,release,finished;auto blocked=entered.get_future(),done=finished.get_future();auto gate=release.get_future().share();std::atomic_uint failures{0};
 server->start();std::jthread first([&]{socket_io.run();}),second([&]{world_io.run();});
 struct Cleanup{net::WorldServer& server;asio::io_context& socket_io;asio::io_context& world_io;std::jthread& first;std::jthread& second;std::promise<void>& release;bool released=false;
  ~Cleanup(){if(!released)release.set_value();server.stop();socket_io.stop();world_io.stop();if(first.joinable())first.join();if(second.joinable())second.join();}} cleanup{*server,socket_io,world_io,first,second,release};
 using Stop=net::WorldOwnerStop<SessionRegistryOwner>;std::shared_ptr<Stop> stop;std::promise<void> started;auto start=started.get_future();
 asio::post(strand,[&]{try {
  stop=Stop::create(strand,server,owner,[&]{finished.set_value();},[&](std::exception_ptr){++failures;},[&]{entered.set_value();gate.wait();throw std::runtime_error("injected transport post failure");});
  check(stop->start(),"post-race stop did not start");started.set_value();
 }catch(...){started.set_exception(std::current_exception());}});
 check(start.wait_for(3s)==std::future_status::ready,"post-race setup stalled");start.get();
 check(blocked.wait_for(3s)==std::future_status::ready,"real transport completion did not enter post seam");
 std::promise<void> retried;auto retry=retried.get_future();asio::post(strand,[&]{try{stop->retry();retried.set_value();}catch(...){retried.set_exception(std::current_exception());}});
 check(retry.wait_for(3s)==std::future_status::ready,"owner retry did not consume real pending result");retry.get();
 check(done.wait_for(3s)==std::future_status::ready && owner->stopped && failures==0,"owner did not finish from real pending transport result");
 release.set_value();cleanup.released=true;
 std::promise<bool> barrier;auto settled=barrier.get_future();server->stop([&](bool ok){barrier.set_value(ok);});
 check(settled.wait_for(3s)==std::future_status::ready && settled.get(),"socket strand did not finish blocked transport callback");
 check(!stop->transport_post_error() && failures==0,"late post failure left stale error after actual result consumption");
 std::promise<void> released;auto release_done=released.get_future();asio::post(strand,[&]{stop.reset();owner.reset();released.set_value();});
 check(release_done.wait_for(3s)==std::future_status::ready,"post-race release stalled");work.reset();first.join();second.join();
}

static void verify_owner_stop_retry(){
 asio::io_context io;auto work=asio::make_work_guard(io);Strand strand(asio::make_strand(io));
 auto sessions=std::make_shared<session::Registry>();auto owner=std::make_shared<SessionRegistryOwner>();
 ReservationAdmission admission_services;game::WorldAdmission admission(admission_services);owner->admission=&admission;
 game::WorldAdmissionUser held;held.user_id=77;net::WorldServerOptions options;options.port=0;options.owner_identity=bind_owner(io,strand,*owner,sessions);
 auto server=std::make_shared<net::WorldServer>(io,options,sessions,[](const session::LoginSession&,net::WorldChannel)->std::unique_ptr<net::WorldApplication>{throw std::logic_error("unexpected connection");});
 server->start();std::jthread thread([&]{io.run();});
 struct Cleanup{net::WorldServer& server;asio::io_context& io;std::jthread& thread;~Cleanup(){server.stop();io.stop();if(thread.joinable())thread.join();}} cleanup{*server,io,thread};
 using Stop=net::WorldOwnerStop<SessionRegistryOwner>;std::shared_ptr<Stop> stop;
 std::promise<void> started,done;std::promise<std::exception_ptr> failure;auto ready=started.get_future(),finished=done.get_future();auto failed=failure.get_future();
 asio::post(strand,[&]{try {
  admission.track_selection(held);
  stop=Stop::create(strand,server,owner,[&]{done.set_value();},[&](std::exception_ptr error){failure.set_value(error);});
  check(stop->start(),"retry stop did not start");started.set_value();
 }catch(...){started.set_exception(std::current_exception());}});
 check(ready.wait_for(3s)==std::future_status::ready,"retry stop setup stalled");ready.get();work.reset();
 check(failed.wait_for(3s)==std::future_status::ready && bool(failed.get()),"real owner shutdown failure was lost");
 check(finished.wait_for(0s)==std::future_status::timeout,"owner stop reported success with an admission member");
 auto cleanup_work=asio::make_work_guard(io);
 std::promise<void> retried;auto retry=retried.get_future();asio::post(strand,[&]{try {
  check(stop->error() && !owner->stopped && admission.selection_count()==1,"owner shutdown failure state changed");
  admission.untrack(held);stop->retry();check(owner->stopped && !stop->error(),"owner shutdown retry failed");retried.set_value();
 }catch(...){retried.set_exception(std::current_exception());}});
 check(retry.wait_for(3s)==std::future_status::ready,"owner stop retry stalled");retry.get();
 check(finished.wait_for(3s)==std::future_status::ready,"owner stop retry did not complete");finished.get();
 std::promise<void> released;auto release=released.get_future();asio::post(strand,[&]{stop.reset();owner.reset();released.set_value();});
 check(release.wait_for(3s)==std::future_status::ready,"retry handle release stalled");
 release.get();cleanup_work.reset();thread.join();
}

static void verify_owner_stop_separate_io(){
 asio::io_context socket_io,world_io;auto world_work=asio::make_work_guard(world_io);Strand strand(asio::make_strand(world_io));
 auto sessions=std::make_shared<session::Registry>();auto owner=std::make_shared<SessionRegistryOwner>();std::weak_ptr<SessionRegistryOwner> weak=owner;
 ReservationAdmission admission_services;game::WorldAdmission admission(admission_services);owner->admission=&admission;
 net::WorldServerOptions options;options.port=0;options.owner_identity=bind_owner(world_io,strand,*owner,sessions);
 auto server=std::make_shared<net::WorldServer>(socket_io,options,sessions,[](const session::LoginSession&,net::WorldChannel)->std::unique_ptr<net::WorldApplication>{throw std::logic_error("unexpected connection");});
 server->start();std::jthread socket_thread([&]{socket_io.run();}),world_thread([&]{world_io.run();});
 struct Cleanup{net::WorldServer& server;asio::io_context& socket_io;asio::io_context& world_io;std::jthread& socket_thread;std::jthread& world_thread;
  ~Cleanup(){server.stop();socket_io.stop();world_io.stop();if(socket_thread.joinable())socket_thread.join();if(world_thread.joinable())world_thread.join();}} cleanup{*server,socket_io,world_io,socket_thread,world_thread};
 using Stop=net::WorldOwnerStop<SessionRegistryOwner>;std::shared_ptr<Stop> stop;std::promise<void> started,finished;auto ready=started.get_future(),done=finished.get_future();
 asio::post(strand,[&]{try {
  stop=Stop::create(strand,server,owner,[&]{try{auto retained=weak.lock();check(retained && retained->stopped,"separate IO owner did not stop");retained.reset();stop.reset();finished.set_value();}catch(...){finished.set_exception(std::current_exception());}},
   [&](std::exception_ptr error){finished.set_exception(error);});
  check(stop->start(),"separate IO owner stop failed to start");owner.reset();started.set_value();
 }catch(...){started.set_exception(std::current_exception());}});
 check(ready.wait_for(3s)==std::future_status::ready,"separate IO setup stalled");ready.get();world_work.reset();
 check(done.wait_for(3s)==std::future_status::ready,"separate IO stop did not complete");done.get();socket_thread.join();world_thread.join();
 check(weak.expired(),"separate IO stop retained owner after completion");
}

int main(){try{
 asio::io_context io;Strand strand(asio::make_strand(io));auto state=std::make_shared<State>(strand);auto sessions=std::make_shared<session::Registry>();net::WorldServerOptions options;options.port=0;
 auto factory=game::serialized_world_factory(strand,[state](const session::LoginSession& account,net::WorldChannel channel){return std::make_unique<App>(*state,std::move(channel),account.user_id);});
 net::WorldServer server(io,options,sessions,std::move(factory));server.start();std::jthread thread([&]{io.run();});
 struct Guard{net::WorldServer& server;std::jthread& thread;~Guard(){server.stop();if(thread.joinable())thread.join();}} guard{server,thread};
 auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
 for(unsigned uid:{11U,12U}){session::LoginSession account{uid,uid+100,1,"native",{}, {}};account.token.fill(static_cast<std::uint8_t>(uid));sessions->add(account);check(sessions->select(uid,uid+100,0,now),"reservation failed");}
 PeerSocket a(server.port()),b(server.port());net::LegacyCipher in_a(net::SessionSecrets{}),in_b(net::SessionSecrets{}),out_a(net::SessionSecrets{}),out_b(net::SessionSecrets{});
 auto connect=[](PeerSocket& peer,unsigned id){net::Packet p{1,0xa3,static_cast<std::uint8_t>(id),0,0,0};p.insert(p.end(),16,static_cast<std::uint8_t>(id));peer.write(net::frame(p));};
 connect(a,11);connect(b,12);
 auto ready=[](PeerSocket& peer,net::LegacyCipher& cipher,unsigned id){auto greeting=peer.packet();cipher.crypt(greeting);check(greeting.size()==20 && greeting[0]==1 && greeting[1]==0xa3,"greeting differs");auto opened=peer.packet();cipher.crypt(opened);check(opened==net::Packet({0x7a,0x77,static_cast<std::uint8_t>(id)}),"open reply differs");};
 ready(a,in_a,11);ready(b,in_b,12);
 auto send=[](PeerSocket& peer,net::LegacyCipher& cipher){net::Packet p{9,1,55};cipher.crypt(p);peer.write(net::frame(p));};send(a,out_a);send(b,out_b);
 auto response=[](PeerSocket& peer,net::LegacyCipher& cipher,unsigned id){auto packet=peer.packet();cipher.crypt(packet);check(packet==net::Packet({0x7b,0x77,static_cast<std::uint8_t>(id)}),"response differs");};response(a,in_a,11);response(b,in_b,12);
 net::Packet select{4,1,42,0,0,0},enter{1,2};out_a.crypt(select);out_a.crypt(enter);a.write(net::frame(select));a.write(net::frame(enter));
 auto selected=a.packet();in_a.crypt(selected);check(selected==net::Packet({4,1,0,42,0,0,0}),"asynchronous selection reply differs");
 auto entered=a.packet();in_a.crypt(entered);check(entered==net::Packet({0x7c,0x77}),"queued entry was not delivered after publication");
 server.stop();thread.join();check(!state->invalid && state->opened==2 && state->received==4 && state->closed==2 && state->destroyed==2,"shutdown did not drain both world applications");
 check(server.application_failures()==0,"application error while draining");verify_connection_identity();verify_signed_batch_limit();verify_priority_world_close_barrier();verify_async_drain();verify_stop_close_barrier();verify_stop_close_failure();verify_factory_failure_fence();verify_real_session_reservation(false);verify_real_session_reservation(true);verify_close_submission_behind_worker();verify_close_submission_reason();verify_owner_stop_transport_failure();verify_owner_stop_post_retry_race();verify_owner_stop_retry();verify_owner_stop_separate_io();verify_retained_lookup();verify_retained_lookup_delivery_failure(false);verify_retained_lookup_delivery_failure(true);verify_retained_lookup_delivery_auto_close();verify_retained_lookup_delivery_stop_race();verify_retained_lookup_failure();verify_known_live_duplicate();verify_late_retained_lookup();std::cout<<"shared world strand handles and drains two TCP connections\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
