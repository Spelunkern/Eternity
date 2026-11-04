#include "eternity/game/vertical_protocol.hpp"
#include "eternity/net/world_server.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try {
    check(argc==2,"missing NProtect oracle");std::ifstream in(argv[1]);check(in.good(),"NProtect oracle missing");std::string line;unsigned cases=0;
    const net::Packet answer{2,0xa3,0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0xef,0xcd,0xab,0x89,0xcc,0xdd,0xee,0xff};const std::array<std::uint32_t,4> expected{0x11223344,0x55667788,0x89abcdef,0xffeeddcc};
    while(std::getline(in,line)) {if(line.empty()||line[0]=='#')continue;std::istringstream row(line);unsigned present,location,received,error,result,final_received;std::string original;row>>present>>location>>received>>error>>result>>final_received>>original;check(bool(row),"bad oracle row");
        NProtectState state{present!=0,received!=0,123};std::vector<std::string> events;VerticalNProtectPorts port;
        port.check_answer=[&](const auto& words){check(words==expected,"answer bytes changed");events.emplace_back("CHECK");return error;};
        port.diagnostic=[&](std::uint32_t e){check(e==error,"wrong diagnostic code");events.emplace_back("LOG");};port.close=[&](std::uint32_t reason){events.push_back("CLOSE"+std::to_string(reason));};
        const auto actual=receive_vertical_nprotect(answer,state,location,port);check((actual==VerticalNProtectResult::accepted)==bool(result) && state.received==bool(final_received) && state.receive_time==123,"NProtect state/result differs");
        std::string observed;for(auto& e:events){if(!observed.empty())observed+=',';observed+=e;}if(observed.empty())observed="-";check(observed==original,"NProtect callback order differs");++cases;
    }
    check(cases==32,"NProtect oracle coverage changed");NProtectState absent{};check(receive_vertical_nprotect(answer,absent,1,{})==VerticalNProtectResult::absent,"absent backend changed");NProtectState present{true,false,0};bool missing=false;try{receive_vertical_nprotect(answer,present,1,{});}catch(const std::invalid_argument&){missing=true;}check(missing && !present.received,"missing backend fabricated success");
    NProtectState retained{true,false,77};std::vector<net::Packet> queries;VerticalNProtectPorts answer_ports;
    answer_ports.check_answer=[&](const auto& words){check(words==expected,"wrong retained answer");return 0U;};answer_ports.diagnostic=[](auto){};answer_ports.close=[](auto){throw std::runtime_error("unexpected close");};
    check(receive_vertical_nprotect(answer,retained,3,answer_ports)==VerticalNProtectResult::accepted && retained.received,"retained answer was not recorded");
    NProtectPorts query_ports;query_ports.query=[](auto& words){words={1,2,3,4};return 0U;};query_ports.tick=[](){return 123U;};query_ports.send=[&](net::Packet p){queries.push_back(std::move(p));};query_ports.diagnostic=[](auto){};query_ports.close=[](auto){throw std::runtime_error("unexpected query close");};
    check(send_nprotect_query(true,retained,query_ports) && !retained.received && retained.receive_time==900123 && queries.size()==1 && queries[0]==net::Packet({2,0xa3,1,0,0,0,2,0,0,0,3,0,0,0,4,0,0,0}),"query and answer did not share NProtect state");
    struct Unbound final:net::WorldApplication {void open() override {} void receive(const net::Packet&) override {} void close() override {}} unbound;
    bool unbound_failed=false;try{unbound.receive_nprotect(answer);}catch(const std::logic_error&){unbound_failed=true;}check(unbound_failed,"unbound application fabricated NProtect success");
    auto registry=std::make_shared<session::Registry>();auto key=std::make_shared<net::WorldXor>(std::array<std::uint8_t,16>{});session::LoginSession account{42,7,11,"native",{}, {}};account.token.fill(0x7a);registry->add(account);check(registry->select(42,7,0,1000),"reserve failed");net::WorldProtocol protocol(registry,0,99,key);net::Packet connect{1,0xa3,42,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());check(protocol.receive(connect,1001).connected,"greeting failed");net::LegacyCipher in_cipher(net::SessionSecrets{});
    auto receive=[&](net::Packet p){in_cipher.crypt(p);return protocol.receive(std::move(p),1002);};
    auto reply=receive({3,0});check(!reply.close&&!reply.application&&!reply.nprotect,"system reply leaked to application");reply=receive(answer);check(reply.nprotect==answer&&!reply.application&&!reply.close,"decrypted NProtect was filtered or misrouted");reply=receive({1,2});check(!reply.close&&!reply.application&&!reply.nprotect,"selection gate changed");reply=receive({4,1});check(reply.application==net::Packet({4,1}),"cipher advanced incorrectly after system packets");reply=receive({2,0});check(reply.close && reply.close_reason==1 && protocol.state()==net::WorldProtocol::State::closed,"system close reason changed");
    std::cout<<cases<<" original NProtect cases and system routing/AES passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
