#include "eternity/net/world_protocol.hpp"
#include <iostream>
#include <stdexcept>
using namespace eternity;
static void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try {
    auto registry=std::make_shared<session::Registry>();auto xor_key=std::make_shared<net::WorldXor>(std::array<std::uint8_t,16>{});
    session::LoginSession account{42,7,11,"native",{}, {}};account.token.fill(0x7a);registry->add(account);check(registry->select(42,7,0,1000),"reserve failed");
    net::WorldProtocol protocol(registry,0,99,xor_key);bool invalid=false;try{protocol.set_admission_dispatch(true);}catch(const std::logic_error&){invalid=true;}check(invalid,"pre-auth routing accepted");
    net::Packet connect{1,0xa3,42,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());auto greeting=protocol.receive(connect,1001);check(greeting.connected && greeting.replies.size()==1,"greeting failed");
    net::LegacyCipher inbound(net::SessionSecrets{}),outbound(net::SessionSecrets{});auto wire=greeting.replies[0];outbound.crypt(wire);check(wire.size()==20,"greeting decrypt failed");
    auto receive=[&](net::Packet p){inbound.crypt(p);return protocol.receive(std::move(p),1002);};
    check(!receive({1,2}).application,"selection did not ignore 0201");protocol.set_admission_dispatch(true);check(protocol.state()==net::WorldProtocol::State::character_selection,"zone routing switched cipher state");
    check(receive({1,2}).application==net::Packet({1,2}),"zone-wait 0201 filtered");net::Packet selection{0x77,0x77,1};wire=protocol.outgoing(selection);outbound.crypt(wire);check(wire==selection,"zone-wait changed outbound AES");
    protocol.set_admission_dispatch(false);check(!receive({1,2}).application,"selection filter not restored");protocol.enter_world();check(!receive({1,2}).application,"cipher switch changed inbound selection filter");protocol.set_admission_dispatch(true);net::Packet world{0x78,0x77,1};wire=protocol.outgoing(world);xor_key->crypt(wire);check(wire==world,"world XOR changed");
    check(receive({1,2}).application==net::Packet({1,2}),"incoming AES lost continuity");protocol.enter_character_selection();check(!receive({1,2}).application,"return to selection retained routing");wire=protocol.outgoing(selection);outbound.crypt(wire);check(wire==selection,"selection AES did not resume");
    protocol.close();invalid=false;try{protocol.set_admission_dispatch(true);}catch(const std::logic_error&){invalid=true;}check(invalid,"closed session accepted routing");std::cout<<"Independent zone routing, selection ignore and AES/XOR continuity passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
