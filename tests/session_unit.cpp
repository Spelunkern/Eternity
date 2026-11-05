#include "eternity/session/registry.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <atomic>
#include <type_traits>
#include <future>
#include <chrono>
static_assert(!std::is_copy_constructible_v<eternity::session::WorldReservation>);
struct ExpiryCopyFailure {
    std::shared_ptr<std::atomic_int> copies_before_throw;
    std::shared_ptr<std::atomic_uint> calls;
    ExpiryCopyFailure(std::shared_ptr<std::atomic_int> n,std::shared_ptr<std::atomic_uint> c):copies_before_throw(std::move(n)),calls(std::move(c)){}
    ExpiryCopyFailure(const ExpiryCopyFailure& other):copies_before_throw(other.copies_before_throw),calls(other.calls) {
        if(copies_before_throw->load()>=0 && copies_before_throw->fetch_sub(1)==0)throw std::runtime_error("expiry copy failure");
    }
    void operator()() const {++*calls;}
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    using namespace eternity::session;
    try {
        Registry registry;
        LoginSession first{42,1,11,"example",{}, {}};
        require(!registry.add(first), "New account unexpectedly replaced a session");
        auto replacement = first; replacement.session_id = 2; replacement.token.fill(0x5a);
        const auto previous = registry.add(replacement);
        require(previous && previous->session_id == 1, "Replacement did not identify the old owner");
        require(!registry.remove(42,1), "Stale disconnect removed the replacement session");
        require(registry.find(42)->token == replacement.token && registry.size() == 1, "Replacement state was not atomic");
        require(registry.remove(42,2) && !registry.find(42), "Current owner could not remove its session");
        std::vector<std::jthread> threads;
        for (unsigned int index=0; index<16; ++index)
            threads.emplace_back([&registry,index] { for(unsigned int count=0; count<100; ++count)
                registry.add(LoginSession{42,static_cast<std::uint64_t>(index)*100+count+1,11,"example",{}, {}}); });
        threads.clear();
        require(registry.size() == 1 && registry.find(42), "Concurrent replacements duplicated an account");
        const auto key = game_key(eternity::net::SessionSecrets{});
        const std::array<std::uint8_t,16> expected_hash = {0x37,0x47,0x08,0xff,0xf7,0x71,0x9d,0xd5,0x97,0x9e,0xc8,0x75,0xd5,0x6c,0xd2,0x28};
        require(std::equal(expected_hash.begin(), expected_hash.end(), key.begin()+16), "World session key derivation changed");

        Registry expiry_atomic;auto copy_budget=std::make_shared<std::atomic_int>(-1);auto expiry_calls=std::make_shared<std::atomic_uint>(0);
        for(std::uint32_t id:{71U,72U}) {
            LoginSession entry{id,id+100,1,"expiry",{}, {}};entry.disconnect=ExpiryCopyFailure(copy_budget,expiry_calls);
            expiry_atomic.add(std::move(entry));require(expiry_atomic.select(id,id+100,0,0),"expiry atomic setup failed");
        }
        *copy_budget=1;bool copy_failed=false;try{(void)expiry_atomic.expire_with_disconnect_fences(30000);}catch(const std::runtime_error&){copy_failed=true;}
        require(copy_failed && expiry_atomic.size()==2 && *expiry_calls==0,"failed expiry lost an owner or invoked a callback");
        *copy_budget=-1;auto expired=expiry_atomic.expire_with_disconnect_fences(30000);require(expired.size()==2 && expiry_atomic.size()==0,"expiry retry lost owners");
        for(const auto& entry:expired)require(entry.disconnect_fence && entry.disconnect_fence->account()==entry.owner.user_id &&
            entry.disconnect_fence->session_id()==entry.owner.session_id && entry.disconnect_fence->location()==LoginSession::Location::handoff &&
            entry.disconnect_fence->realm()==0 && entry.disconnect_fence->world_connection()==0,"expired recovery identity changed");
        std::atomic_uint expiry_blocked{0};std::vector<std::jthread> expiry_contenders;
        for(unsigned i=0;i<16;++i)expiry_contenders.emplace_back([&,i]{LoginSession candidate{71,400+i,1,"candidate",{}, {}};if(expiry_atomic.add_with_disconnect_fence(std::move(candidate)).blocked)++expiry_blocked;});
        expiry_contenders.clear();require(expiry_blocked==16,"expired callback fence allowed concurrent login");
        for(auto& entry:expired){entry.owner.disconnect();require(expiry_atomic.release_login_disconnect_fence(entry.disconnect_fence),"expired disconnect fence release failed");entry.disconnect_fence.reset();}require(*expiry_calls==2,"expiry retry lost disconnect callbacks");
        LoginSession released{71,500,1,"released",{}, {}};require(!expiry_atomic.add_with_disconnect_fence(std::move(released)).blocked,"expired callback fence did not release");

        Registry shared_login;LoginSession prior{61,101,1,"old",{}, {}};prior.disconnect=[]{};
        shared_login.add(prior);LoginSession next=prior;next.session_id=102;auto replacing=shared_login.add_with_disconnect_fence(next);
        require(!replacing.blocked && replacing.previous && replacing.disconnect_fence && replacing.disconnect_fence->account()==61 && replacing.disconnect_fence->session_id()==101 &&
            replacing.disconnect_fence->location()==LoginSession::Location::login && replacing.disconnect_fence->realm()==0xff &&
            replacing.disconnect_fence->world_connection()==0,"replacement did not reserve original owner identity");
        std::atomic_uint fenced_adds{0};std::vector<std::jthread> contenders;
        for(unsigned i=0;i<16;++i)contenders.emplace_back([&,i]{auto candidate=prior;candidate.session_id=200+i;if(shared_login.add_with_disconnect_fence(std::move(candidate)).blocked)++fenced_adds;});
        contenders.clear();require(fenced_adds==16 && shared_login.find(61)->session_id==102,"concurrent login bypassed in-flight disconnect");
        std::atomic_uint fenced_selections{0};
        for(unsigned i=0;i<16;++i)contenders.emplace_back([&]{if(!shared_login.select(61,102,0,1))++fenced_selections;});
        contenders.clear();require(fenced_selections==16 && shared_login.find(61)->location==LoginSession::Location::login,
            "selection bypassed an in-flight disconnect");
        bool legacy_rejected=false;try{shared_login.add(prior);}catch(const std::logic_error&){legacy_rejected=true;}
        require(legacy_rejected,"legacy add bypassed pending disconnect");
        replacing.disconnect_fence.reset();auto retained=shared_login.login_disconnect_fences();
        require(retained.size()==1 && retained[0]->account()==61 && shared_login.add_with_disconnect_fence(prior).blocked,"abandoned callback lost registry fence");
        Registry foreign_registry;require(!foreign_registry.release_login_disconnect_fence(retained[0]),"foreign registry released disconnect fence");
        replacing.previous->disconnect();require(shared_login.release_login_disconnect_fence(retained[0]) && !shared_login.release_login_disconnect_fence(retained[0]),"disconnect fence identity release failed");
        auto admitted=shared_login.add_with_disconnect_fence(prior);
        require(!admitted.blocked && admitted.previous && admitted.previous->session_id==102 && admitted.disconnect_fence,"login remained fenced after disconnect request returned");
        require(!shared_login.release_login_disconnect_fence(retained[0]) && shared_login.add_with_disconnect_fence(next).blocked,
            "stale fence released a later disconnect request");
        require(!shared_login.select(61,101,0,0),"new login selected before its previous owner disconnected");
        admitted.previous->disconnect();require(shared_login.release_login_disconnect_fence(admitted.disconnect_fence),"in-flight disconnect release failed");
        admitted.disconnect_fence.reset();require(shared_login.select(61,101,0,0),"selection remained blocked after disconnect release");
        auto deferred=shared_login.expire_with_disconnect_fences(30000);
        require(deferred.size()==1 && deferred[0].disconnect_fence && shared_login.size()==0,"deferred handoff did not transfer after fence release");

        Registry world_identity;LoginSession world_original{62,301,1,"world",{}, {}};world_original.disconnect=[]{};
        world_identity.add(world_original);require(world_identity.select(62,301,1,0),"world identity selection failed");
        auto world_claim=world_identity.claim_world(62,world_original.token,1,777,1,[]{});require(bool(world_claim.session),"world identity claim failed");
        LoginSession world_replacement=world_original;world_replacement.session_id=302;
        auto world_transfer=world_identity.add_with_disconnect_fence(world_replacement);
        require(world_transfer.disconnect_fence && world_transfer.disconnect_fence->account()==62 && world_transfer.disconnect_fence->session_id()==301 &&
            world_transfer.disconnect_fence->location()==LoginSession::Location::world && world_transfer.disconnect_fence->realm()==1 &&
            world_transfer.disconnect_fence->world_connection()==777,"world recovery identity changed");
        require(!world_identity.select(62,302,1,1),"replacement selected while old world disconnect was unresolved");
        std::atomic_uint concurrent_duplicates{0};std::vector<std::jthread> duplicate_claimers;
        for(unsigned i=0;i<16;++i)duplicate_claimers.emplace_back([&,i]{auto result=world_identity.claim_world(62,world_replacement.token,1,800+i,2);if(result.duplicate && !result.session)++concurrent_duplicates;});
        duplicate_claimers.clear();require(concurrent_duplicates==16,"disconnect fence masked retained world duplicate");
        world_transfer.previous->disconnect();require(world_identity.release_login_disconnect_fence(world_transfer.disconnect_fence),"world disconnect release failed");
        require(world_identity.select(62,302,1,1),"replacement failed to select after disconnect request returned");
        require(world_identity.claim_world(62,world_replacement.token,1,778,2).duplicate,"retained old world owner lost its independent reservation");
        world_claim.session.reset();require(bool(world_identity.claim_world(62,world_replacement.token,1,778,2).session),"world remained fenced after actual reservation release");

        // Abandon the initiating owner's references while the registry retains
        // the exact callback, then race real recovery and identity release.
        Registry recoverable;std::promise<void> callback_entered,callback_release;
        auto entered=callback_entered.get_future();auto gate=callback_release.get_future().share();std::atomic_uint request_calls{0};
        LoginSession abandoned{63,401,1,"abandoned",{}, {}};
        abandoned.disconnect=[&]{++request_calls;callback_entered.set_value();gate.wait();};
        recoverable.add(abandoned);auto new_owner=abandoned;new_owner.session_id=402;
        auto transfer=recoverable.add_with_disconnect_fence(std::move(new_owner));
        transfer.previous.reset();transfer.disconnect_fence.reset();auto snapshots=recoverable.login_disconnect_fences();
        require(snapshots.size()==1 && snapshots[0]->session_id()==401,"recovery snapshot lost actual owner");
        std::promise<LoginDisconnectFence::Request> requested;auto request_result=requested.get_future();
        std::jthread requester([&]{try{requested.set_value(snapshots[0]->request_disconnect());}catch(...){requested.set_exception(std::current_exception());}});
        const bool began=entered.wait_for(std::chrono::seconds(3))==std::future_status::ready;
        const auto busy=began?snapshots[0]->request_disconnect():LoginDisconnectFence::Request::retired;
        const bool early_release=began && recoverable.release_login_disconnect_fence(snapshots[0]);
        const bool still_blocked=began && recoverable.add_with_disconnect_fence(abandoned).blocked;
        callback_release.set_value();requester.join();
        require(began && busy==LoginDisconnectFence::Request::in_flight && !early_release && still_blocked,"in-flight real recovery lost exclusion");
        require(request_result.get()==LoginDisconnectFence::Request::issued && request_calls==1 &&
            snapshots[0]->request_disconnect()==LoginDisconnectFence::Request::already_issued,"actual recovery was duplicated or lost");
        require(recoverable.release_login_disconnect_fence(snapshots[0]) &&
            snapshots[0]->request_disconnect()==LoginDisconnectFence::Request::retired,"retired recovery callback remained callable");

        Registry handoffs;
        first.token.fill(0x5a);handoffs.add(first);
        require(!handoffs.select(42,2,0,1000),"A stale login selected a world for the current session");
        require(handoffs.select(42,1,0,1000),"Original session handoff was not started");
        require(!handoffs.release_login(42,1) && handoffs.size()==1,"Closing login erased a pending world handoff");
        auto wrong=first.token;wrong[0]^=1;
        require(!handoffs.claim(42,wrong,0,9,1001),"Wrong world token accepted");
        require(!handoffs.claim(42,first.token,1,9,1001),"Token accepted by the wrong realm");
        require(!handoffs.claim(42,first.token,0,9,31000),"Expired token accepted at its deadline");
        require(handoffs.expire(30999).empty() && handoffs.expire(31000).size()==1 && handoffs.size()==0,
                "Original thirty-second handoff lifetime changed");
        handoffs.add(first);require(handoffs.select(42,1,0,1000),"Second handoff failed");
        std::atomic<unsigned int> accepted{0};
        for(unsigned int index=0;index<8;++index)
            threads.emplace_back([&handoffs,&accepted,&first,index] {
                if(handoffs.claim(42,first.token,0,index+1,1001)) ++accepted;
            });
        threads.clear();require(accepted==1,"Concurrent world connections consumed one token more than once");
        const auto world=handoffs.find(42);
        require(world && world->location==LoginSession::Location::world && !handoffs.release_login(42,1),
                "Transferred account did not belong to the world service");
        require(!handoffs.release_world(42,1,99),"Wrong world connection removed a session");
        replacement=first;replacement.session_id=2;
        const auto old_world=handoffs.add(replacement);
        require(old_world && old_world->location==LoginSession::Location::world,
                "Relogin did not return the previous world owner for disconnection");
        require(!handoffs.release_world(42,1,world->world_connection) && handoffs.find(42)->session_id==2,
                "Stale world close removed a replacement login");
        // A retained old-world lease must defeat concurrent replacement claims
        // even after the login index has been replaced and the socket released.
        Registry fenced;first.session_id=501;fenced.add(first);require(fenced.select(42,501,0,1),"fence setup failed");
        auto old=fenced.claim_world(42,first.token,0,601,2);require(bool(old.session),"old claim failed");
        require(old.session->world_reservation->account()==42 && old.session->world_reservation->realm()==0 &&
            old.session->world_reservation->session_id()==501 && old.session->world_reservation->connection()==601,"claim lost owner identity");
        replacement=first;replacement.session_id=502;fenced.add(replacement);require(fenced.select(42,502,0,2),"replacement setup failed");
        std::atomic_uint duplicates{0},unexpected{0};
        for(unsigned i=0;i<16;++i) threads.emplace_back([&,i]{
            auto result=fenced.claim_world(42,first.token,0,700+i,3);
            if(result.duplicate) ++duplicates;if(result.session) ++unexpected;
        });
        threads.clear();require(duplicates==16 && unexpected==0,"concurrent replacement bypassed old reservation");
        old.session.reset();auto current=fenced.claim_world(42,first.token,0,800,3);
        require(current.session && !current.duplicate,"replacement failed after old reservation drained");
        return 0;
    } catch(const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
