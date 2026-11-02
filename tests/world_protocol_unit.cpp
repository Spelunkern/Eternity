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
        net::WorldProtocol replay(registry,0,101,xor_key);
        require(replay.receive(connect(),1002).close && registry->size()==1,"Token replay accepted or removed active owner");
        net::Packet unrelated{1,0x11};outgoing.crypt(unrelated);
        require(!world.receive(unrelated,1003).application,"Chat dispatched before character selection");
        net::Packet choose{4,1,0,0,0,0};const auto choose_plain=choose;outgoing.crypt(choose);
        require(world.receive(choose,1004).application==choose_plain,"Selection payload not decrypted after ignored packet");
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
        require(undefined.receive({0,0},1001).close,"Eleventh undefined packet did not close");
        net::WorldProtocol truncated(registry,0,105,xor_key);
        require(truncated.receive({1,0xa3,42},1001).close,"Truncated game connect accepted");
        reserve();auto wrong=connect();wrong.back()^=1;
        net::WorldProtocol bad_token(registry,0,106,xor_key);
        require(bad_token.receive(wrong,1001).close && registry->find(42)->location==session::LoginSession::Location::handoff,
                "Wrong token consumed legitimate reservation");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
