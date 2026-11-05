#define ETERNITY_HANDOFF_FIXTURE_ONLY
#include "vertical_protocol_handoff_tcp_unit.cpp"
#include "eternity/game/serialized_world_application.hpp"
#include "eternity/game/vertical_admission_handoff.hpp"
#include "eternity/game/roster_world_handoff.hpp"
#include "eternity/game/vertical_entry.hpp"
#include "eternity/game/vertical_async_selection.hpp"
#include "eternity/game/vertical_session_registry.hpp"
#include "eternity/game/vertical_owned_selection.hpp"
#include <asio/thread_pool.hpp>

struct EntryOwner {
    std::shared_ptr<HandoffState> state;net::WorldChannel channel;std::shared_ptr<InventoryReplicaPool> pool=std::make_shared<InventoryReplicaPool>();
    std::unique_ptr<HandoffApp> fixture;std::shared_ptr<VerticalOwnedSelection<EntryOwner>> selection;std::shared_ptr<VerticalAdmissionHandoff> handoff;
    EntryOwner(std::shared_ptr<HandoffState> s,net::WorldChannel c):state(std::move(s)),channel(std::move(c)) {}
};
struct AsyncEntry final:net::WorldApplication {
    using Strand=game::SerializedWorldApplication::Strand;
    Strand strand;asio::any_io_executor db;std::shared_ptr<EntryOwner> owner;std::shared_ptr<VerticalSessionRegistry<EntryOwner>> registry;CharacterRoster roster;
    AsyncEntry(Strand executor,asio::any_io_executor database,std::shared_ptr<HandoffState> state,net::WorldChannel channel,CharacterRoster retained,std::shared_ptr<VerticalSessionRegistry<EntryOwner>> owners)
        :strand(std::move(executor)),db(std::move(database)),owner(std::make_shared<EntryOwner>(std::move(state),std::move(channel))),registry(std::move(owners)),roster(std::move(retained)) {registry->reserve(11,owner);}
    void open() override {}
    void receive(const net::Packet& packet) override {
        if(packet==net::Packet{4,1,42,0,0,0}) {
            owner->channel.pause_input([](bool){});
            auto weak=std::weak_ptr<EntryOwner>(owner);auto owners=registry;auto executor=strand;
            owner->selection=std::make_shared<VerticalOwnedSelection<EntryOwner>>(strand,db,*owners,owner,CharacterSelectionState{11,roster.country,true},roster,
                [pool=owner->pool](std::uint32_t user,std::uint32_t character)->std::optional<VerticalPersistenceSelection>{
                    check(user==11 && character==42,"worker received wrong identity");auto loaded=HandoffApp::selection(*pool);loaded.replica->retain_resources(pool);return loaded;
                },
                [weak,owners,executor](EntryOwner&,VerticalPersistenceSelection loaded){
                    auto self=weak.lock();check(bool(self),"selection lost owner");self->fixture=std::make_unique<HandoffApp>(self->state,self->channel,std::move(loaded));auto& fixture=*self->fixture;
                    owners->bind_actor(11,*self,fixture.actor);
                    self->handoff=std::make_shared<VerticalAdmissionHandoff>(executor,fixture.channel,fixture.user,[weak]{
                        auto self=weak.lock();check(bool(self),"admission lost owner");auto& fixture=*self->fixture;
                        CharacterEntryState entry;entry.user_id=11;entry.character_id=42;entry.level=fixture.character.snapshot().core.character.level;
                        SocialCatalog social(11,42);auto catalogs=vertical_restore_entry_catalogs(fixture.character.replica());VerticalProtocolServices protocol;
                        protocol.delivery.send_self=[self](net::Packet p){self->channel.send(std::move(p));};protocol.delivery.send_peer=[](std::uint32_t,net::Packet){};protocol.delivery.social=[](std::uint32_t)->const SocialCatalog*{return nullptr;};
                        protocol.delivery.tick=[] {return 1U;};protocol.delivery.kill_count=[](std::uint8_t){return EntryKillCount{};};
                        protocol.lifecycle=[](CharacterEntryStep,Actor&){};protocol.send_manager=[](net::Packet){};protocol.world_day=[] {return 0U;};protocol.system_time=[] {return 0U;};protocol.lock_guild=[] {return false;};protocol.unlock_guild=[]{};
                        check(deliver_vertical_entry_with_admission(entry,fixture.character,fixture.world,fixture.user,social,catalogs,{0,100},"",protocol) && entry.ready,"character delivery failed");
                        check(fixture.user.location==2,"character did not enter world queue");update_vertical_admission(fixture.world);check(fixture.user.location==3,"zone publication failed");fixture.channel.send({0x76,0x77});self->handoff->zone_ready();
                    },[](std::exception_ptr){throw std::runtime_error("asynchronous handoff failed");});self->handoff->start();
                },[](EntryOwner&) {throw std::runtime_error("selection unavailable");},[](EntryOwner&,std::uint32_t){throw std::runtime_error("selection closed");},[](EntryOwner&,std::exception_ptr){throw std::runtime_error("selection worker failed");});
            check(owner->selection->receive(packet)==CharacterSelectionResult::requested,"selection request failed");return;
        }
        check(bool(owner->fixture),"packet arrived before selection");auto& fixture=*owner->fixture;
        if(packet==net::Packet{1,2}) {fixture.pending_entry=packet;fixture.outcome.reset();fixture.world.update_zone_wait_packets(fixture.zone);check(fixture.outcome==VerticalEntryRequest::entered && fixture.zone.users->find_actor(42)==&fixture.actor,"actual actor failed to enter zone");fixture.channel.enter_world();fixture.channel.send({0x79,0x77});return;}
        if(packet.size()==17 && packet[0]==1 && packet[1]==5) {const auto moved=fixture.move_session.receive(packet);check(moved==VerticalMoveResult::moved && fixture.move_services.finished && fixture.zone.users->position(42)->x==45,"retained actor movement failed");fixture.channel.send({0x7b,0x77});return;}
        throw std::runtime_error("unexpected client packet");
    }
    void close() override {
        if(owner->handoff)owner->handoff->cancel();if(owner->fixture)owner->fixture->close();
        if(owner->selection){auto self=owner;auto owners=registry;owner->selection->cancel_and_drain([self,owners]{check(owners->release(11,*self),"session release before selection drain");});}
        else check(registry->release(11,*owner),"empty selection owner not released");
    }
};
int main(){try {
    asio::io_context io;game::SerializedWorldApplication::Strand strand(asio::make_strand(io));auto registry=std::make_shared<session::Registry>();auto state=std::make_shared<HandoffState>();auto owners=std::make_shared<VerticalSessionRegistry<EntryOwner>>();asio::thread_pool database(1);net::WorldServerOptions options;options.port=0;
    auto factory=game::roster_world_serialized_factory(strand,
        [strand](const session::LoginSession& account){check(!strand.running_in_this_thread() && account.user_id==11,"roster load ran on world strand or wrong account");CharacterRoster roster;roster.country=0;CharacterSummary character;character.id=42;character.slot=0;roster.slots[0]=character;return roster;},
        [](const session::LoginSession&,CharacterRoster&,const net::Packet&){return CharacterReply{{},true};},
        [state,strand,owners,&database](const session::LoginSession& account,net::WorldChannel channel,CharacterRoster roster){check(strand.running_in_this_thread() && account.user_id==11 && roster.slots[0] && roster.slots[0]->id==42,"handoff lost world strand or roster identity");return std::make_unique<AsyncEntry>(strand,database.get_executor(),state,std::move(channel),std::move(roster),owners);});
    net::WorldServer server(io,options,registry,std::move(factory));server.start();std::jthread thread([&]{io.run();});
    struct Guard{net::WorldServer& server;std::jthread& thread;~Guard(){server.stop();if(thread.joinable())thread.join();}} guard{server,thread};
    session::LoginSession account{11,101,1,"native",{}, {}};account.token.fill(0x32);registry->add(account);
    auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());check(registry->select(11,101,0,now),"reservation failed");
    PeerSocket peer(server.port());net::Packet connect{1,0xa3,11,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());peer.write(net::frame(connect));
    net::LegacyCipher incoming(net::SessionSecrets{}),outgoing(net::SessionSecrets{});auto greeting=peer.packet();incoming.crypt(greeting);check(greeting.size()==20,"greeting failed");std::array<std::uint8_t,16> seed{};std::copy_n(greeting.begin()+4,16,seed.begin());net::WorldXor xor_key(seed);
    auto country=peer.packet();incoming.crypt(country);check(country==net::Packet({9,1,0,2}),"roster country missing");
    for(unsigned i=0;i<4;++i){auto empty=peer.packet();incoming.crypt(empty);check(empty.size()==7 && empty[0]==1 && empty[1]==1,"empty roster slot missing");}
    auto listed=peer.packet();incoming.crypt(listed);check(listed.size()==83 && listed[0]==1 && listed[1]==1 && listed[3]==42,"selected character roster missing");
    net::Packet selection{4,1,42,0,0,0},enter{1,2};outgoing.crypt(selection);outgoing.crypt(enter);peer.write(net::frame(selection));peer.write(net::frame(enter));
    auto ack=peer.packet();incoming.crypt(ack);check(ack==net::Packet({4,1,0,42,0,0,0}),"selection acknowledgement differs");
    bool data=false,clock=false;for(unsigned count=0;;++count) {check(count<32,"initial packet stream did not finish");auto packet=peer.packet();incoming.crypt(packet);if(packet==net::Packet{0x76,0x77})break;check(packet.size()>=2,"short entry packet");data|=packet[0]==5 && packet[1]==1;clock|=packet[0]==0x14 && packet[1]==1;}
    check(data && clock,"entry payload missing");
    for(auto opcode:std::array<std::uint16_t,5>{0x405,0x403,0x201,0x51c,0xe01}) {auto packet=peer.packet();incoming.crypt(packet);check(packet.size()>=2 && (packet[0]|(packet[1]<<8))==opcode,"zone snapshot differs");}
    auto entered=peer.packet();xor_key.crypt(entered);check(entered==net::Packet({0x79,0x77}),"zone entry or XOR transition did not finish");
    net::Packet movement{1,5,0x81,0x34,0x12};for(float value:{45.F,0.F,40.F}){const auto bits=std::bit_cast<std::uint32_t>(value);for(unsigned i=0;i<4;++i)movement.push_back(static_cast<std::uint8_t>(bits>>(8*i)));}
    auto encrypted=movement;outgoing.crypt(encrypted);peer.write(net::frame(encrypted));auto moved=peer.packet();xor_key.crypt(moved);
    net::Packet expected{1,5,42,0,0,0,0x81,0x34,0x12};expected.insert(expected.end(),movement.begin()+5,movement.end());check(moved==expected,"asynchronous world movement packet differs");
    auto confirmed=peer.packet();xor_key.crypt(confirmed);check(confirmed==net::Packet({0x7b,0x77}),"movement did not finish");
    server.stop();thread.join();database.join();check(owners->size()==0 && server.application_failures()==0 && state->closed,"async world owner failed to close");
    std::cout<<"nonblocking TCP character delivery and actual admission passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
