#include "eternity/net/world_protocol.hpp"
#include <iostream>

void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }

int main() {
    using namespace eternity;
    try {
        auto registry=std::make_shared<session::Registry>();
        auto xor_key=std::make_shared<net::WorldXor>(std::array<std::uint8_t,16>{});
        session::LoginSession account{42,7,11,"example",{}, {}};
        account.token.fill(0x7a);
        net::SessionSecrets secrets{};
        for(std::size_t i=0;i<16;++i) { secrets.key[i]=static_cast<std::uint8_t>(i);secrets.iv[i]=static_cast<std::uint8_t>(i+16); }
        std::copy(secrets.key.begin(),secrets.key.end(),account.game_key.begin());
        std::copy(secrets.iv.begin(),secrets.iv.end(),account.game_key.begin()+16);
        const auto reserve=[&] {registry->add(account);require(registry->select(42,7,0,1000),"Reserve failed");};
        const auto connect=[&] {net::Packet p{1,0xa3,42,0,0,0};p.insert(p.end(),account.token.begin(),account.token.end());return p;};
        reserve();
        net::WorldProtocol world(registry,0,100,xor_key);
        const auto greeting=world.receive(connect(),1001);
        require(greeting.connected && !greeting.close && greeting.replies.size()==1,"World connection rejected reserved session");
        net::LegacyCipher incoming(secrets),outgoing(secrets);
        auto reply=greeting.replies.front();incoming.crypt(reply);
        require(reply==net::Packet({1,0xa3,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}),"Encrypted original game greeting changed");
        require(world.owner() && world.owner()->session_id==7,"World lost login owner metadata");
        net::WorldProtocol other_realm(registry,1,115,xor_key);
        require(other_realm.receive(connect(),1002).close && registry->find(42)->location==session::LoginSession::Location::world,"A different realm treated a live owner as local");
        net::WorldProtocol replay(registry,0,101,xor_key);
        const auto short_duplicate=replay.receive({1,0xa3,42,0,0,0},1002);
        require(!short_duplicate.close && !short_duplicate.connected,"short duplicate rejected before live-account check");
        for(unsigned i=0;i<9;++i) {const auto denied=replay.receive(connect(),1002);require(!denied.close && !denied.connected && !denied.application,"Replay reached an owner or closed too early");}
        const auto replay_close=replay.receive(connect(),1002);
        require(replay_close.close && replay_close.close_reason==0x65 && registry->size()==1,"Replay did not follow duplicate-account limit");
        net::Packet unrelated{1,0x11};outgoing.crypt(unrelated);
        require(!world.receive(unrelated,1003).application,"Chat dispatched before character selection");
        net::Packet choose{4,1,0,0,0,0};const auto choose_plain=choose;outgoing.crypt(choose);
        require(world.receive(choose,1004).application==choose_plain,"Selection payload not decrypted after ignored packet");
        world.set_admission_dispatch(true);
        world.enter_world();
        net::Packet response{1,2,3,4};auto expected=response;xor_key->crypt(expected);
        require(world.outgoing(response)==expected,"World response did not switch to XOR");
        net::Packet move{1,2,3,4};const auto plain_move=move;outgoing.crypt(move);
        require(world.receive(move,1005).application==plain_move,"World input did not retain incoming AES stream");
        world.enter_character_selection();
        auto resumed=world.outgoing(response);incoming.crypt(resumed);
        require(resumed==response,"Returning to selection reset or advanced AES during gameplay");
        auto replacement=account;replacement.session_id=8;
        registry->add(replacement);world.close();
        require(registry->find(42)->session_id==8,"Stale world close removed replacement login");
        reserve();
        {net::WorldProtocol owner(registry,0,102,xor_key);require(owner.receive(connect(),1001).connected,"Second connection failed");}
        require(registry->size()==0,"World destruction leaked an account reservation");
        reserve();
        net::WorldProtocol expired(registry,0,103,xor_key);
        require(expired.receive(connect(),31000).close,"Deadline boundary token accepted");
        net::WorldProtocol undefined(registry,0,104,xor_key);
        for(unsigned int i=0;i<10;++i) require(!undefined.receive({0,0},1001).close,"Undefined packet closed too early");
        const auto undefined_close=undefined.receive({0,0},1001);
        require(undefined_close.close && undefined_close.close_reason==0x65,"Eleventh undefined packet lost original close reason");
        net::WorldProtocol zero(registry,0,107,xor_key);net::Packet zero_connect{1,0xa3,0,0,0,0};zero_connect.resize(22);
        for(unsigned int i=0;i<10;++i) require(!zero.receive(zero_connect,1001).close,"Zero account closed before undefined limit");
        const auto zero_close=zero.receive(zero_connect,1001);require(zero_close.close && zero_close.close_reason==0x65,"Zero account did not share undefined close behavior");
        reserve();net::WorldProtocol recovered(registry,0,108,xor_key);
        require(!recovered.receive(zero_connect,1001).close && recovered.receive(connect(),1001).connected,"Zero account prevented a valid retry");
        recovered.close();
        net::WorldProtocol truncated(registry,0,105,xor_key);
        require(truncated.receive({1,0xa3,42},1001).close,"Truncated game connect accepted");
        net::WorldProtocol short_unknown(registry,0,116,xor_key);
        require(short_unknown.receive({1,0xa3,43,0,0,0},1001).close,"Short nonduplicate game connect accepted");
        // Original SReply::CheckRecv cases, with accepted-agent intervals.
        struct ReplyCase {std::uint32_t last,now,reason;};
        for(const auto test:std::array<ReplyCase,10>{{{0,1,0},{0,0,0},{1,1,0},{1,900001,0},{1,900002,8},
             {1000,999,10},{0xfffffff0U,20,0},{0x100,0x80000100U,10},{0x100,0x800000ffU,8},{0xffff0000U,0x000cbb20U,0}}}) {
            net::WorldProtocol heartbeat(registry,0,109,xor_key);
            require(!heartbeat.receive({3,0},test.last).close,"first reply unexpectedly closed");
            const auto result=heartbeat.receive({3,0},test.now);
            require(result.close==(test.reason!=0) && result.close_reason==(test.reason?std::optional<std::uint32_t>{test.reason}:std::nullopt),"original reply timing or close reason changed");
        }
        // Original SReply::CheckTime after a reply.
        struct TimeoutCase {std::uint32_t last,now;bool timed_out;};
        for(const auto test:std::array<TimeoutCase,6>{{{1,900001,false},{1,900002,true},{1000,999,false},
             {0xfffffff0U,20,false},{0x100,0x80000100U,false},{0x100,0x800000ffU,true}}}) {
            net::WorldProtocol timeout(registry,0,111,xor_key);require(!timeout.receive({3,0},test.last).close,"timeout setup failed");
            require(timeout.reply_timed_out(test.now)==test.timed_out,"original periodic reply timeout changed");
        }
        net::WorldProtocol no_reply(registry,0,112,xor_key);
        require(!no_reply.reply_timed_out(900000) && no_reply.reply_timed_out(900001) && no_reply.reply_timed_out(0xffffffff) && !no_reply.reply_timed_out(0x100000001ULL),"original zero-baseline first-reply timeout changed");
        reserve();net::WorldProtocol protected_reply(registry,0,110,xor_key);require(protected_reply.receive(connect(),100).connected,"reply test connect failed");
        net::LegacyCipher reply_sender(secrets);net::Packet first_reply{3,0};reply_sender.crypt(first_reply);
        require(!protected_reply.receive(first_reply,100).close,"first encrypted reply closed");
        net::Packet late_reply{3,0};reply_sender.crypt(late_reply);const auto late=protected_reply.receive(late_reply,900101);
        require(late.close && late.close_reason==8 && registry->size()==0,"late encrypted reply did not close retained owner");
        // A pre-auth duplicate can retry after the previous world owner exits
        // and a new handoff is actually reserved; it never acquires the old owner.
        reserve();net::WorldProtocol previous(registry,0,113,xor_key);require(previous.receive(connect(),1001).connected,"duplicate setup failed");
        net::WorldProtocol waiting(registry,0,114,xor_key);const auto denied=waiting.receive(connect(),1002);
        require(!denied.close && !denied.connected && !waiting.owner(),"duplicate acquired a live account");
        previous.close();reserve();require(waiting.receive(connect(),1003).connected,"duplicate could not use a later valid handoff");waiting.close();
        // A live world owner remains the duplicate authority even while a
        // replacement login holds its real disconnect request in flight.
        auto shared=std::make_shared<session::Registry>();auto shared_account=account;shared_account.session_id=51;
        shared->add(shared_account);require(shared->select(42,51,0,1000),"fenced duplicate setup failed");
        net::WorldProtocol* old_owner=nullptr;
        net::WorldProtocol old_world(shared,0,151,xor_key,[&]{old_owner->close();});old_owner=&old_world;
        require(old_world.receive(connect(),1001).connected,"fenced duplicate world owner failed");
        auto new_account=shared_account;new_account.session_id=52;
        auto pending=shared->add_with_disconnect_fence(new_account);
        require(!pending.blocked && pending.disconnect_fence,"fenced duplicate replacement failed");
        net::WorldProtocol probe(shared,0,152,xor_key);
        for(unsigned i=0;i<10;++i) {const auto denied=probe.receive(connect(),1002);require(!denied.close && !denied.connected,"fenced live duplicate closed early");}
        const auto limit=probe.receive(connect(),1002);require(limit.close && limit.close_reason==0x65,"fenced live duplicate lost original limit");
        require(pending.disconnect_fence->request_disconnect()==session::LoginDisconnectFence::Request::issued &&
            shared->release_login_disconnect_fence(pending.disconnect_fence),"fenced actual world disconnect did not complete");

        reserve();auto wrong=connect();wrong.back()^=1;
        net::WorldProtocol bad_token(registry,0,106,xor_key);
        require(bad_token.receive(wrong,1001).close && registry->find(42)->location==session::LoginSession::Location::handoff,
                "Wrong token consumed legitimate reservation");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
