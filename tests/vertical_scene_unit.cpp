#include "eternity/game/vertical_scene.hpp"
#include "eternity/game/maps.hpp"
#include "login_socket.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool value,const char* why) {if(!value) throw std::runtime_error(why);}
struct PartyWorld final:PartyMembershipSendWorld {
    PartyMembershipSendBuffer buffer;std::vector<std::uint32_t> sent;bool active=false;unsigned locks=0,frees=0;
    PartyMembershipSendBuffer* allocate_send_buffer(std::uint16_t) override {check(!active,"Duplicate buffer allocation");active=true;return &buffer;}
    void free_send_buffer(PartyMembershipSendBuffer& b) override {check(&b==&buffer && b.references==0,"Wrong buffer release");active=false;++frees;}
    void enqueue_send(PartyMembershipUser& u,PartyMembershipSendBuffer& b) override {check(locks && &b==&buffer,"Missing party lock/buffer");sent.push_back(u.id);release_party_membership_buffer(b,*this);}
    void lock(PartyNormalMembership&) override {++locks;}
    void unlock(PartyNormalMembership&) noexcept override {--locks;}
    std::uint32_t tick() override {throw std::logic_error("Unexpected tick");}
    void lock_effects(PartyMembershipUser&) override {throw std::logic_error("Unexpected effect lock");}
    void unlock_effects(PartyMembershipUser&) noexcept override {std::terminate();}
    void send_user(PartyMembershipUser&,const net::Packet&) override {throw std::logic_error("Unexpected user send");}
    void send_scene(PartyMembershipUser&,const net::Packet&) override {throw std::logic_error("Unexpected scene send");}
    void unregister_party(PartyNormalMembership&) override {throw std::logic_error("Unexpected unregister");}
    void cleanup_guild_creation(PartyMembershipUser&) override {throw std::logic_error("Unexpected cleanup");}
    void fill_log(PartyMembershipUser&,std::span<std::uint8_t>) override {throw std::logic_error("Unexpected log");}
    void send_log(const net::Packet&) override {throw std::logic_error("Unexpected log send");}
};
struct Services final:VerticalSceneServices {
    PartyRegistryParty* group=nullptr;std::vector<std::string> calls;std::uint32_t freed=0;
    void nearby(ZoneUsers& z,Actor& a,const net::Packet&) override {check(z.find_actor(a.id())==&a,"Wrong nearby owner");calls.push_back("near");}
    void cell_view(ZoneUsers& z,Actor& a,const net::Packet&) override {check(z.find_actor(a.id())==&a,"Wrong cell owner");calls.push_back("cell");}
    PartyRegistryParty* party(PartyMembershipUser&) noexcept override {return group;}
    std::optional<std::uint32_t> allocate_effect_id() noexcept override {return 19;}
    void release_effect_id(std::uint32_t id) noexcept override {freed=id;}
    std::uint32_t tick_ms() const noexcept override {return 101;}
    void reset_damage(Actor&) override {calls.push_back("reset");}
    void stop_target_mob(Actor&,std::uint32_t id) override {check(id==44,"Wrong target");calls.push_back("stop");}
    void taunt(Actor&,std::uint16_t amount,float range) override {check(amount==5 && range==60,"Wrong taunt");calls.push_back("taunt");}
    UserShapeSocial user_shape_social(Actor&) override {return {2,"guild"};}
    void finish_periodic_damage(Actor&,EffectStatus&,const SkillDefinition&,const EffectSource&,std::uint32_t elapsed) override {check(elapsed==7,"Wrong damage time");calls.push_back("damage");}
};

struct SceneApplication final:net::WorldApplication {
    MapGeometry geometry{160};ZoneUsers zone{geometry,0};PartyMembershipUser user;PartyWorld party_world;Services services;
    ItemDefinitions items;EnchantValues enchant;MobAttackTemplates mobs;VerticalScene scene;Actor actor;
    explicit SceneApplication(net::WorldChannel channel):scene(std::move(channel),zone,user,party_world,services),actor(42,{}, {},{},scene,{items,enchant,mobs}) {user.id=42;}
    void open() override {scene.bind(actor);check(zone.attach_at_position(actor,{20,0,20}),"Network scene attachment failed");scene.send_self({1,1,42});}
    void receive(const net::Packet&) override {throw std::logic_error("Unexpected network request");}
    void close() override {check(zone.detach(actor.id()),"Network scene detach failed");scene.unbind();}
};
void network_channel() {
    asio::io_context io;auto registry=std::make_shared<session::Registry>();session::LoginSession account{};account.user_id=42;account.session_id=7;account.username="scene";account.token.fill(0x23);
    for(unsigned i=0;i<account.game_key.size();++i) account.game_key[i]=static_cast<std::uint8_t>(i+1);
    registry->add(account);const auto now=static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());check(registry->select(42,7,0,now),"Cannot reserve test world");
    net::WorldServerOptions options;options.port=0;net::WorldServer server(io,options,registry,[](const session::LoginSession&,net::WorldChannel channel) {return std::make_unique<SceneApplication>(std::move(channel));});server.start();
    struct Running {asio::io_context& io;net::WorldServer& server;std::jthread thread;Running(asio::io_context& i,net::WorldServer& s):io(i),server(s),thread([&]{io.run();}){}~Running(){server.stop();if(thread.joinable()) thread.join();}} running(io,server);
    net::SessionSecrets secrets{};std::copy_n(account.game_key.begin(),16,secrets.key.begin());std::copy_n(account.game_key.begin()+16,16,secrets.iv.begin());net::LegacyCipher receive(secrets);
    PeerSocket peer(server.port());Packet connect{1,0xa3,42,0,0,0};connect.insert(connect.end(),account.token.begin(),account.token.end());peer.write(net::frame(connect));
    auto greeting=peer.packet();receive.crypt(greeting);check(greeting.size()==20 && greeting[0]==1 && greeting[1]==0xa3,"World greeting failed");
    auto message=peer.packet();receive.crypt(message);check(message==Packet({1,1,42}),"Actual WorldChannel scene delivery failed");
}
}
int main() {try {
    std::vector<net::Packet> outgoing;net::WorldChannel channel;channel.send=[&](net::Packet p){outgoing.push_back(std::move(p));};
    const MapGeometry geometry(160);ZoneUsers zone(geometry,0);PartyMembershipUser user;user.id=42;PartyWorld party_world;Services services;
    VerticalScene scene(channel,zone,user,party_world,services);const ItemDefinitions definitions;const EnchantValues enchant;const MobAttackTemplates mobs;
    Actor actor(42,{}, {},{},scene,{definitions,enchant,mobs});scene.bind(actor);
    scene.send_self({5,2,1});check(outgoing==std::vector<net::Packet>{{5,2,1}},"Wrong channel send");
    check(!scene.user_zone(),"Unattached actor has zone");scene.send_nearby({1,2});check(services.calls.empty(),"Detached view was sent");
    check(zone.attach_at_position(actor,{20,0,20}),"Cannot attach actor");scene.send_nearby({1,2});scene.send_cell_view({3,4});check(scene.user_zone()==&zone && services.calls==std::vector<std::string>{"near","cell"},"Zone views not routed");
    bool threw=false;try {scene.unbind();} catch(const std::logic_error&) {threw=true;}check(threw,"Attached unbind accepted");
    PartyRegistryParty party;PartyMembershipUser a,b,c;a.id=43;b.id=44;c.id=45;a.send_socket=1;b.send_socket=-1;c.send_socket=2;user.send_socket=3;user.party=&party;party.slots[0]=&user;party.slots[1]=&a;party.slots[2]=&b;party.extra_slots[0]=&c;party.count=3;services.group=&party;
    check(scene.party_kind()==false,"Normal kind lost");scene.send_party({1,2,3});check(party_world.sent==std::vector<std::uint32_t>{43} && party_world.frees==1 && !party_world.active,"Normal party source exclusion or buffer ownership failed");
    party.experience.kind=1;party.count=8;check(scene.party_kind()==true,"Raid kind lost");scene.send_party({9,8});check(party_world.sent==std::vector<std::uint32_t>({43,43,45}) && party_world.frees==2,"Raid extended slots or sender exclusion failed");
    scene.reset_damage();scene.stop_target_mob(44);scene.taunt(5,60);check(scene.allocate_effect_id()==19 && scene.tick_ms()==101,"Effect ID or clock delegation failed");scene.release_effect_id(19);check(services.freed==19 && scene.user_shape_social().guild_name=="guild","Effect release or social owner failed");
    EffectStatus status;SkillDefinition skill;EffectSource source;scene.finish_periodic_damage(status,skill,source,7);check(services.calls==std::vector<std::string>({"near","cell","reset","stop","taunt","damage"}),"Scene required service dispatch failed");
    check(zone.detach(actor.id()),"Cannot detach actor");scene.unbind();network_channel();std::cout<<"Scene channel, actual zone membership, normal/raid party buffer ownership and required services passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
