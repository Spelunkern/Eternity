#include "eternity/game/guild_catalog.hpp"
#include "eternity/game/guild_client_etin.hpp"
#include "eternity/game/guild_client_ignored.hpp"
#include "eternity/game/guild_client_npc.hpp"
#include "eternity/game/guild_client_decline.hpp"
#include "eternity/game/guild_client_accept.hpp"
#include "eternity/game/guild_npc_config.hpp"
#include "eternity/game/guild_npc_log.hpp"
#include "eternity/game/guild_client_rank.hpp"
#include "eternity/game/guild_client_remark.hpp"
#include "eternity/game/guild_duel_catalog_world.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <thread>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(std::string_view s) {if(s.empty()) return "-";std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned char c:s) out<<std::setw(2)<<unsigned(c);return out.str();}
net::Packet packet(const std::string& raw) {net::Packet out;for(std::size_t i=0;i<raw.size();i+=2) out.push_back(static_cast<std::uint8_t>(std::stoul(raw.substr(i,2),nullptr,16)));return out;}
struct BroadcastFixture final:GuildDuelBroadcastServices {
    Actor& actor(GuildUser&) override{throw std::logic_error("Unexpected actor lookup");}
    std::shared_ptr<void> retain(Actor&) override{throw std::logic_error("Unexpected actor pin");}
    VerticalGuildDuelMovement& movement_state(Actor&) override{throw std::logic_error("Unexpected movement lookup");}
    std::unique_ptr<UserViewBuffer> allocate(const net::Packet&) override{throw std::logic_error("Unexpected buffer allocation");}
    void enqueue(GuildUser&,Actor&,UserViewBuffer&) override{throw std::logic_error("Unexpected send");}
};
struct CatalogDuelFixture final:GuildDuelCatalogWorld {
    CatalogDuelFixture(GuildCatalog& c,GuildDuelBroadcastServices& s):GuildDuelCatalogWorld(c,s){}
};
std::vector<std::string> snapshot(const GuildCatalog& catalog) {
    std::vector<std::string> out{"MAX "+std::to_string(catalog.maximum_id())};
    for(const auto& [id,unused]:catalog.listings()) {
        static_cast<void>(unused);const auto* guild=catalog.find(id);require(guild && guild->duel.guild==guild,"Guild duel state lost catalog owner");const auto& m=guild->metadata;const auto& v=m.listing;std::ostringstream line;
        line<<"GUILD "<<id<<' '<<hex(v.name)<<' '<<hex(v.master)<<' '<<v.country<<' '<<v.points<<' '<<unsigned(v.rank)<<' '<<m.etin<<' '<<unsigned(m.use_house)<<' '<<hex(v.notice)<<' '<<unsigned(m.buy_house)<<' '<<m.keep_etin<<' '<<m.return_count<<' '<<m.officers;out.push_back(line.str());
        for(unsigned int phase=0;phase<2;++phase) for(const auto& [key,record]:(phase?guild->membership.applications:guild->membership.offline)) {
            std::ostringstream row;row<<"MEMBER "<<id<<' '<<(phase?"APPLICATION":"OFFLINE")<<' '<<key<<' '<<record->id<<' '<<record->level<<' '<<record->job<<' '<<record->rank<<' '<<hex(record->name)<<' '<<record->billing_id<<' '<<record->ranking_entry;out.push_back(row.str());
        }
    }
    return out;
}
void client_etin_cases() {
    GuildCatalogPorts ports;ports.allocate_guild=[] {return true;};ports.allocate_member=[] {return true;};
    ports.transport.allocate=[](const net::Packet&)->bool{throw std::logic_error("unexpected guild allocation");};ports.transport.release=[]{throw std::logic_error("unexpected guild release");};ports.transport.send=[](GuildUser&,net::Packet){throw std::logic_error("unexpected guild broadcast");};
    GuildCatalog catalog(ports);net::Packet load(3+135,0);load[0]=2;load[1]=11;load[2]=1;load[3]=77;load[3+56]=0x78;load[3+57]=0x56;load[3+58]=0x34;load[3+59]=0x12;
    require(catalog.apply(load)==EntryCatalogUpdate::applied && catalog.find(77),"guild etin catalog load failed");auto* guild=catalog.find(77);GuildUser user;std::vector<net::Packet> sent;
    auto send=[&](GuildUser& target,net::Packet packet){require(&target==&user,"guild etin sent to wrong user");sent.push_back(std::move(packet));};
    require(guild_client_etin(user,catalog,net::Packet{0x23,0x0d},send)==EntryCatalogUpdate::applied && sent.empty(),"guildless query sent a response");
    user.guild=&guild->membership;require(guild_client_etin(user,catalog,net::Packet{0x23,0x0d},send)==EntryCatalogUpdate::applied && sent==std::vector<net::Packet>{{0x23,0x0d,0x78,0x56,0x34,0x12}},"guild etin response differs");
    sent.clear();unsigned nested=0;std::function<void(GuildUser&,net::Packet)> reentrant;
    reentrant=[&](GuildUser& target,net::Packet packet){sent.push_back(std::move(packet));if(nested++==0){bool other_acquired=false;std::thread observer([&]{other_acquired=guild->mutex.try_lock();if(other_acquired)guild->mutex.unlock();});observer.join();require(!other_acquired,"guild etin send ran outside captured lock");require(guild->mutex.try_lock(),"guild etin callback lost recursive lock");guild->mutex.unlock();guild->metadata.etin=0xffffffffU;require(guild_client_etin(target,catalog,net::Packet{0x23,0x0d},reentrant)==EntryCatalogUpdate::applied,"nested guild etin failed");}};
    require(guild_client_etin(user,catalog,net::Packet{0x23,0x0d},reentrant)==EntryCatalogUpdate::applied && sent==std::vector<net::Packet>{{0x23,0x0d,0x78,0x56,0x34,0x12},{0x23,0x0d,0xff,0xff,0xff,0xff}},"reentrant guild etin differs");
    bool missing=false;try{(void)guild_client_etin(user,catalog,net::Packet{0x23,0x0d},{});}catch(const std::invalid_argument&){missing=true;}require(missing,"missing guild etin transport accepted");
    GuildMembership foreign(99,ports.transport);user.guild=&foreign;bool mismatch=false;try{(void)guild_client_etin(user,catalog,net::Packet{0x23,0x0d},send);}catch(const std::logic_error&){mismatch=true;}require(mismatch,"foreign guild owner accepted");
    require(guild_client_etin(user,catalog,net::Packet{0x23},send)==EntryCatalogUpdate::rejected && guild_client_etin(user,catalog,net::Packet{0x24,0x0d},send)==EntryCatalogUpdate::unsupported,"guild etin envelope differs");
}
void client_ignored_cases() {
    for(unsigned op=0;op<256;++op) {
        const bool handled=(op>=3 && op<=10) || op==0x0c || (op>=0x20 && op<=0x23) || op==0x25 || op==0x34 || op==0x35;
        require(guild_client_ignored(net::Packet{static_cast<std::uint8_t>(op),0x0d})!=handled,"guild ignored opcode classification differs");
    }
    require(!guild_client_ignored(net::Packet{}) && !guild_client_ignored(net::Packet{0}) && !guild_client_ignored(net::Packet{0,0x22}),"non-guild packet classified as ignored guild packet");
}
void client_rank_cases() {
    std::vector<std::string> events;std::vector<net::Packet> db,broadcast;
    GuildCatalogPorts ports;ports.allocate_guild=[] {return true;};ports.allocate_member=[] {return true;};
    ports.transport.allocate=[&](const net::Packet&){events.push_back("allocate");return true;};ports.transport.release=[&]{events.push_back("release");};
    ports.transport.send=[&](GuildUser&,net::Packet p){events.push_back("broadcast");broadcast.push_back(std::move(p));};
    GuildCatalog catalog(ports);net::Packet load(138,0);load[0]=2;load[1]=11;load[2]=1;load[3]=77;require(catalog.apply(load)==EntryCatalogUpdate::applied,"rank guild load failed");auto& guild=*catalog.find(77);
    GuildUser requester;requester.character_id=40;requester.rank=1;requester.guild=&guild.membership;GuildUser peer;peer.character_id=43;peer.rank=3;peer.guild=&guild.membership;
    auto member=std::make_shared<GuildMember>();member->id=43;member->rank=3;member->user=&peer;guild.membership.online.emplace(43,member);guild.metadata.officers=1;
    auto send=[&](net::Packet p){events.push_back("db");db.push_back(std::move(p));};auto request=[](std::uint8_t action,std::uint32_t id){return net::Packet{0x0c,0x0d,action,static_cast<std::uint8_t>(id),0,0,0};};
    bool missing=false;try{(void)guild_client_rank(requester,catalog,request(0,43),{});}catch(const std::invalid_argument&){missing=true;}require(missing && member->rank==3 && guild.metadata.officers==1,"missing rank DB transport mutated state");
    require(guild_client_rank(requester,catalog,request(0,43),send)==EntryCatalogUpdate::applied && member->rank==2 && peer.rank==2 && guild.metadata.officers==2 && guild.membership.leaders[0]==member && events==std::vector<std::string>({"db","allocate","broadcast","release"}) && db.back()==net::Packet({9,11,77,0,0,0,43,0,0,0,2}) && broadcast.back()==net::Packet({12,13,0xca,43,0,0,0}),"online promotion differs");
    events.clear();require(guild_client_rank(requester,catalog,request(1,43),send)==EntryCatalogUpdate::applied && member->rank==3 && peer.rank==3 && guild.metadata.officers==1 && !guild.membership.leaders[0],"online demotion differs");
    guild.membership.online.clear();member->user=nullptr;guild.membership.offline.emplace(43,member);events.clear();require(guild_client_rank(requester,catalog,request(0,43),send)==EntryCatalogUpdate::applied && member->rank==2 && guild.metadata.officers==2 && !guild.membership.leaders[0] && events==std::vector<std::string>({"db","allocate","release"}),"offline promotion differs");
    events.clear();requester.rank=2;require(guild_client_rank(requester,catalog,request(1,43),send)==EntryCatalogUpdate::applied && events.empty() && member->rank==2,"peer rank change accepted");requester.rank=1;guild.metadata.officers=7;member->rank=3;
    require(guild_client_rank(requester,catalog,request(0,43),send)==EntryCatalogUpdate::applied && events.empty() && member->rank==3,"officer limit ignored");
    require(guild_client_rank(requester,catalog,request(2,43),send)==EntryCatalogUpdate::applied && guild_client_rank(requester,catalog,request(0,40),send)==EntryCatalogUpdate::applied && events.empty(),"invalid rank request had side effects");
    guild.membership.offline.clear();guild.membership.online.emplace(43,member);member->user=&peer;member->rank=3;peer.rank=3;guild.metadata.officers=1;events.clear();db.clear();broadcast.clear();bool nested=false;
    std::function<void(net::Packet)> reentrant;reentrant=[&](net::Packet p){events.push_back("db");db.push_back(std::move(p));if(!nested){nested=true;require(guild_client_rank(requester,catalog,request(1,43),reentrant)==EntryCatalogUpdate::applied,"nested rank update failed");}};
    require(guild_client_rank(requester,catalog,request(0,43),reentrant)==EntryCatalogUpdate::applied && member->rank==3 && peer.rank==3 && guild.metadata.officers==1 && events==std::vector<std::string>({"db","db","allocate","broadcast","release","allocate","broadcast","release"}) && db[0][10]==2 && db[1][10]==3 && broadcast[0][2]==0xcb && broadcast[1][2]==0xca,"reentrant rank update ordering differs");
    events.clear();bool failed=false;try{(void)guild_client_rank(requester,catalog,request(0,43),[&](net::Packet){events.push_back("db");throw std::runtime_error("DB transport failed");});}catch(const std::runtime_error&){failed=true;}
    require(failed && member->rank==2 && peer.rank==2 && events==std::vector<std::string>{"db"},"rank DB failure ordering differs");
    require(guild_client_rank(requester,catalog,net::Packet{12,13},send)==EntryCatalogUpdate::rejected && guild_client_rank(requester,catalog,net::Packet{11,13},send)==EntryCatalogUpdate::unsupported,"rank envelope differs");
}
void client_accept_cases() {
    std::vector<std::string> events;std::vector<net::Packet> db,admin,notice,peer_packets;GuildUser requester,officer;requester.character_id=40;requester.rank=1;officer.character_id=41;
    auto applicant=std::make_shared<GuildUser>();applicant->character_id=43;applicant->request_id=77;bool online=false,nest=false;std::uint32_t deadline=0;
    GuildCatalogPorts port;port.allocate_guild=[] {return true;};port.allocate_member=[] {return true;};port.transport.allocate=[&](const net::Packet& packet){events.push_back(packet[0]==0x18?"allocate_admin":"allocate_guild");return true;};port.transport.release=[&]{events.push_back("release");};
    port.transport.send=[&](GuildUser& user,net::Packet p){if(p[0]==0x18){require(&user==&officer,"accept admin owner differs");events.push_back("admin");admin.push_back(std::move(p));}else if(p[0]==0x14){events.push_back("notice");notice.push_back(std::move(p));}else{require(&user==applicant.get(),"accept roster owner differs");events.push_back("roster");}};
    GuildCatalog catalog(port);net::Packet load(138,0);load[0]=2;load[1]=11;load[2]=1;load[3]=77;std::copy_n("GuildName",9,load.begin()+7);require(catalog.apply(load)==EntryCatalogUpdate::applied,"accept guild load failed");auto& guild=*catalog.find(77);requester.guild=&guild.membership;
    auto leader=std::make_shared<GuildMember>();leader->id=41;leader->user=&officer;leader->rank=1;guild.membership.leaders[0]=leader;guild.membership.online.emplace(41,leader);
    auto add=[&]{auto record=std::make_shared<GuildMember>();record->id=43;record->name="Applicant";record->level=25;record->job=3;guild.membership.applications.emplace(43,record);catalog.directory().add(43,&guild.membership);return record;};
    GuildApplicationAcceptPorts service{[&](std::uint32_t id)->std::shared_ptr<GuildUser>{events.push_back("lookup");require(id==43,"accept lookup differs");return online?applicant:nullptr;},[&](GuildUser& target){require(&target==applicant.get(),"accept level owner differs");events.push_back("level");return std::uint16_t{41};},[&](GuildUser& target,net::Packet packet){require(&target==applicant.get(),"accept peer differs");events.push_back("client");peer_packets.push_back(std::move(packet));},[&](net::Packet packet){events.push_back("db");db.push_back(std::move(packet));},[&](GuildUser& target,const LoadedGuild& actual){require(&target==applicant.get() && &actual==&guild,"accept bank owner differs");events.push_back("bank");},[&]{events.push_back("tick");return 123u;},[&](GuildUser& target,std::uint32_t value){require(&target==applicant.get(),"accept deadline owner differs");events.push_back("deadline");deadline=value;},[&](const GuildMember& record,GuildUser* target,const LoadedGuild& actual){require(record.id==43 && target==(online?applicant.get():nullptr) && &actual==&guild,"accept log owner differs");events.push_back("log");}};
    net::Packet request{8,13,1,43,0,0,0};require(guild_client_accept(requester,catalog,request,service)==EntryCatalogUpdate::applied && events.empty(),"missing accept produced effects");
    auto record=add();requester.rank=3;require(guild_client_accept(requester,catalog,request,service)==EntryCatalogUpdate::applied && guild.membership.applications.contains(43),"nonofficer accepted application");requester.rank=1;request[2]=0;require(guild_client_accept(requester,catalog,request,service)==EntryCatalogUpdate::unsupported,"decline falsely accepted");request[2]=1;
    require(guild_client_accept(requester,catalog,request,service)==EntryCatalogUpdate::applied && guild.membership.offline.at(43)==record && record->rank==9 && !record->user && events==std::vector<std::string>({"lookup","db","allocate_admin","admin","release","allocate_guild","notice","release","log"}) && db.back()==net::Packet({9,11,77,0,0,0,43,0,0,0,9}) && admin.back()==net::Packet({0x18,13,43,0,0,0}) && notice.back().size()==32 && notice.back()[2]==0 && notice.back()[7]==9 && notice.back()[8]==25 && notice.back()[10]==3,"offline acceptance differs");
    guild.membership.offline.erase(43);record=add();events.clear();online=true;service.send_db=[&](net::Packet packet){events.push_back("db");if(!nest){nest=true;require(guild_client_accept(requester,catalog,request,service)==EntryCatalogUpdate::applied,"nested accept failed");}db.push_back(std::move(packet));};
    require(guild_client_accept(requester,catalog,request,service)==EntryCatalogUpdate::applied && nest && guild.membership.online.at(43)==record && record->rank==9 && record->level==41 && record->user==applicant.get() && applicant->guild==&guild.membership && applicant->rank==9 && applicant->guild_id==77 && applicant->request_id==0 && deadline==0x0f73147b && events==std::vector<std::string>({"lookup","level","db","allocate_admin","admin","release","allocate_guild","notice","notice","release","client","roster","bank","tick","deadline","log"}) && peer_packets.back().size()==33 && peer_packets.back()[2]==1 && notice.back()[2]==1 && notice.back()[8]==41,"online reentrant acceptance differs");
    guild.membership.online.erase(43);record=add();events.clear();auto broken=service;broken.send_db=[&](net::Packet){events.push_back("db");throw std::runtime_error("DB failure");};bool failed=false;try{guild_client_accept(requester,catalog,request,broken);}catch(const std::runtime_error&){failed=true;}require(failed && !guild.membership.applications.contains(43) && guild.membership.online.contains(43) && events==std::vector<std::string>({"lookup","level","db"}),"accept DB failure ordering differs");
    guild.membership.online.erase(43);record=add();auto missing=service;missing.game_log={};failed=false;try{guild_client_accept(requester,catalog,request,missing);}catch(const std::invalid_argument&){failed=true;}require(failed && guild.membership.applications.contains(43),"missing acceptance service mutated state");
    events.clear();auto failed_bank=service;failed_bank.send_bank=[&](GuildUser&,const LoadedGuild&){events.push_back("bank");throw std::runtime_error("bank unavailable");};failed=false;try{guild_client_accept(requester,catalog,request,failed_bank);}catch(const std::runtime_error&){failed=true;}require(failed && guild.membership.online.contains(43) && events.back()=="bank" && std::find(events.begin(),events.end(),"tick")==events.end() && std::find(events.begin(),events.end(),"log")==events.end(),"accept bank failure ordering differs");
    guild.membership.online.erase(43);record=add();events.clear();auto failed_log=service;failed_log.game_log=[&](const GuildMember&,GuildUser*,const LoadedGuild&){events.push_back("log");throw std::runtime_error("log unavailable");};failed=false;try{guild_client_accept(requester,catalog,request,failed_log);}catch(const std::runtime_error&){failed=true;}require(failed && guild.membership.online.contains(43) && events.back()=="log" && std::find(events.begin(),events.end(),"deadline")!=events.end(),"accept log failure ordering differs");
}
void client_decline_cases() {
    std::vector<std::string> events;std::vector<net::Packet> db,admin,peer_packets;bool allocate=true;GuildUser requester,officer,nonofficer;requester.character_id=40;requester.rank=1;officer.character_id=41;nonofficer.character_id=42;
    GuildCatalogPorts ports;ports.allocate_guild=[] {return true;};ports.allocate_member=[] {return true;};
    ports.transport.allocate=[&](const net::Packet&){events.push_back("allocate");return allocate;};ports.transport.release=[&]{events.push_back("release");};ports.transport.send=[&](GuildUser& target,net::Packet packet){require(&target==&officer,"decline sent to nonofficer");events.push_back("admin");admin.push_back(std::move(packet));};
    GuildCatalog catalog(ports);net::Packet load(138,0);load[0]=2;load[1]=11;load[2]=1;load[3]=77;std::copy_n("GuildName",9,load.begin()+7);require(catalog.apply(load)==EntryCatalogUpdate::applied,"decline guild load failed");auto& guild=*catalog.find(77);requester.guild=&guild.membership;
    auto leader=std::make_shared<GuildMember>();leader->id=41;leader->user=&officer;guild.membership.leaders[0]=leader;auto member=std::make_shared<GuildMember>();member->id=42;member->user=&nonofficer;guild.membership.online.emplace(42,member);
    auto applicant=std::make_shared<GuildUser>();applicant->character_id=43;applicant->request_id=77;bool online=false;
    GuildApplicationDeclinePorts services{[&](std::uint32_t id)->std::shared_ptr<GuildUser>{events.push_back("lookup");require(id==43,"decline wrong lookup");return online?applicant:nullptr;},[&](GuildUser& user,net::Packet packet){require(&user==applicant.get(),"decline wrong applicant");events.push_back("client");peer_packets.push_back(std::move(packet));},[&](net::Packet packet){events.push_back("db");db.push_back(std::move(packet));}};
    net::Packet request{8,13,0,43,0,0,0};require(guild_client_decline(requester,catalog,request,services)==EntryCatalogUpdate::applied && events.empty(),"missing application produced effects");
    guild.membership.applications.emplace(43,nullptr);require(guild_client_decline(requester,catalog,request,services)==EntryCatalogUpdate::applied && !guild.membership.applications.contains(43) && events.empty(),"null decline application not removed");
    auto add=[&]{auto record=std::make_shared<GuildMember>();record->id=43;guild.membership.applications.emplace(43,record);catalog.directory().add(43,&guild.membership);};
    add();requester.rank=3;require(guild_client_decline(requester,catalog,request,services)==EntryCatalogUpdate::applied && guild.membership.applications.contains(43),"nonofficer decline accepted");requester.rank=1;request[2]=1;require(guild_client_decline(requester,catalog,request,services)==EntryCatalogUpdate::unsupported && guild.membership.applications.contains(43),"guild acceptance falsely handled");request[2]=0;
    require(guild_client_decline(requester,catalog,request,services)==EntryCatalogUpdate::applied && !guild.membership.applications.contains(43) && events==std::vector<std::string>({"lookup","db","allocate","admin","release"}) && db.back()==net::Packet({8,11,77,0,0,0,43,0,0,0}) && admin.back()==net::Packet({0x18,13,43,0,0,0}),"offline decline differs");
    GuildUser orphan;orphan.character_id=43;require(!catalog.directory().connect(orphan),"declined application remained indexed");
    add();online=true;events.clear();require(guild_client_decline(requester,catalog,request,services)==EntryCatalogUpdate::applied && applicant->request_id==0 && events==std::vector<std::string>({"lookup","db","client","allocate","admin","release"}) && peer_packets.back().size()==33 && peer_packets.back()[0]==8 && peer_packets.back()[2]==0 && peer_packets.back()[3]==77 && peer_packets.back()[7]==9 && peer_packets.back()[8]=='G',"online decline differs");
    add();events.clear();auto broken=services;broken.send_db=[&](net::Packet){events.push_back("db");throw std::runtime_error("DB failure");};bool failed=false;try{guild_client_decline(requester,catalog,request,broken);}catch(const std::runtime_error&){failed=true;}require(failed && !guild.membership.applications.contains(43) && events==std::vector<std::string>({"lookup","db"}),"decline DB failure ordering differs");
    add();events.clear();bool nested=false;auto reentrant=services;reentrant.send_db=[&](net::Packet packet){events.push_back("db");if(!nested){nested=true;require(guild_client_decline(requester,catalog,request,services)==EntryCatalogUpdate::applied,"nested decline failed");}db.push_back(std::move(packet));};require(guild_client_decline(requester,catalog,request,reentrant)==EntryCatalogUpdate::applied && nested && events==std::vector<std::string>({"lookup","db","client","allocate","admin","release"}),"reentrant decline duplicated deletion");
    add();events.clear();allocate=false;require(guild_client_decline(requester,catalog,request,services)==EntryCatalogUpdate::applied && events==std::vector<std::string>({"lookup","db","client","allocate"}),"decline allocation failure ordering differs");
}
void client_npc_cases() {
    GuildUser user;user.character_id=43;user.rank=1;std::vector<std::string> events;std::vector<net::Packet> self,db,broadcast;bool nest=false,nested=false;GuildCatalog* catalog_ptr=nullptr;GuildNpcUpgradePorts* service_ptr=nullptr;
    GuildCatalogPorts ports;ports.allocate_guild=[] {return true;};ports.allocate_member=[] {return true;};
    ports.transport.allocate=[&](const net::Packet&){events.push_back("allocate");return true;};ports.transport.release=[&]{events.push_back("release");};
    ports.transport.send=[&](GuildUser& target,net::Packet p){require(&target==&user,"guild NPC broadcast owner differs");events.push_back("broadcast");broadcast.push_back(std::move(p));if(nest && !nested){nested=true;require(guild_client_npc(user,*catalog_ptr,net::Packet{0x25,0x0d,3,0,1},service_ptr)==EntryCatalogUpdate::applied,"nested guild NPC upgrade failed");}};
    GuildCatalog catalog(ports);catalog_ptr=&catalog;net::Packet load(138,0);load[0]=2;load[1]=11;load[2]=1;load[3]=77;load[3+55]=1;load[3+56]=0x10;load[3+57]=0x27;load[3+60]=1;load[3+126]=1;load[3+127]=0xd0;load[3+128]=7;
    require(catalog.apply(load)==EntryCatalogUpdate::applied,"guild NPC catalog load failed");auto& guild=*catalog.find(77);user.guild=&guild.membership;auto member=std::make_shared<GuildMember>();member->id=43;member->user=&user;guild.membership.online.emplace(43,member);
    auto config=guild_npc_executable_defaults(2000);GuildNpcUpgradePorts service{config,[&](GuildUser& target,net::Packet p){require(&target==&user,"guild NPC client owner differs");events.push_back("self");self.push_back(std::move(p));},[&](net::Packet p){events.push_back("db");db.push_back(std::move(p));},[&](GuildUser& target,const LoadedGuild& actual,std::uint16_t,std::uint8_t){require(&target==&user && &actual==&guild,"guild NPC log lost owners");events.push_back("log");}};service_ptr=&service;
    const auto request=[](std::uint16_t kind,std::uint8_t level){return net::Packet{0x25,0x0d,static_cast<std::uint8_t>(kind),static_cast<std::uint8_t>(kind>>8),level};};
    auto invalid=[&](std::uint8_t status,std::uint16_t kind,std::uint8_t level){events.clear();self.clear();require(guild_client_npc(user,catalog,request(kind,level),&service)==EntryCatalogUpdate::applied && events==std::vector<std::string>{"self"} && self.back()[2]==status,"guild NPC failure differs");};
    invalid(3,2,4);guild.metadata.listing.rank=15;invalid(4,3,1);guild.metadata.listing.rank=1;guild.metadata.etin=2000;invalid(1,2,1);guild.metadata.etin=10000;guild.npcs.slots[0]={2,2};invalid(2,2,1);guild.npcs.slots[0]={};
    auto missing=service;missing.send_db={};events.clear();bool failed=false;try{(void)guild_client_npc(user,catalog,request(2,1),&missing);}catch(const std::invalid_argument&){failed=true;}require(failed && events.empty() && guild.npcs.slots[0].type==0,"missing NPC DB transport mutated guild");
    events.clear();db.clear();self.clear();broadcast.clear();require(guild_client_npc(user,catalog,request(2,1),&service)==EntryCatalogUpdate::applied && events==std::vector<std::string>({"allocate","broadcast","release","self","db","log"}) && guild.npcs.slots[0].type==2 && guild.npcs.slots[0].level==1 && guild.metadata.etin==9000 && guild.metadata.keep_etin==2200 && broadcast.back()==net::Packet({0x25,0x0d,0,2,0,1,0}) && self.back()==net::Packet({0x23,0x0d,0x28,0x23,0,0}) && db.back()==net::Packet({0x0f,0x0b,77,0,0,0,2,0,1,0,0x28,0x23,0,0,0x98,8,0,0}),"guild NPC success differs");
    guild.npcs.slots={};guild.metadata.etin=10000;guild.metadata.keep_etin=2000;events.clear();db.clear();nest=true;require(guild_client_npc(user,catalog,request(2,1),&service)==EntryCatalogUpdate::applied && nested && guild.metadata.etin==8500 && guild.metadata.keep_etin==2300 && db.size()==2 && db[0][6]==3 && db[1][6]==2 && events==std::vector<std::string>({"allocate","broadcast","allocate","broadcast","release","self","db","log","release","self","db","log"}),"reentrant NPC upgrade ordering differs");
    nest=false;for(auto& slot:guild.npcs.slots)slot={6,1};guild.metadata.etin=10000;guild.metadata.keep_etin=2000;events.clear();db.clear();broadcast.clear();require(guild_client_npc(user,catalog,request(2,1),&service)==EntryCatalogUpdate::applied && guild.metadata.etin==9000 && guild.metadata.keep_etin==2800 && broadcast.back()[6]==8 && db.back()[9]==8 && guild.npcs.slots[0].type==6,"full guild NPC slot behavior differs");
    guild.npcs.slots={};guild.metadata.etin=10000;guild.metadata.keep_etin=2000;config.default_keep=3000;config.costs[0][1]=1250;config.upkeep[0][1]=321;events.clear();require(guild_client_npc(user,catalog,request(2,1),&service)==EntryCatalogUpdate::applied && guild.metadata.etin==8750 && guild.metadata.keep_etin==3321,"guild NPC loaded configuration overrides were ignored");
    guild.npcs.slots={};guild.metadata.etin=10000;guild.metadata.keep_etin=2000;events.clear();auto broken=service;broken.send_db=[&](net::Packet){events.push_back("db");throw std::runtime_error("NPC DB transport failed");};bool db_failed=false;
    try{(void)guild_client_npc(user,catalog,request(2,1),&broken);}catch(const std::runtime_error&){db_failed=true;}
    require(db_failed && guild.npcs.slots[0].type==2 && guild.metadata.etin==8750 && guild.metadata.keep_etin==3321 && events==std::vector<std::string>({"allocate","broadcast","release","self","db"}),"guild NPC DB failure ordering differs");
    user.rank=3;events.clear();require(guild_client_npc(user,catalog,request(2,1),nullptr)==EntryCatalogUpdate::applied && events.empty(),"non-officer NPC request produced effects");
    require(guild_client_npc(user,catalog,net::Packet{0x25,0x0d},&service)==EntryCatalogUpdate::rejected && guild_client_npc(user,catalog,net::Packet{0x24,0x0d},&service)==EntryCatalogUpdate::unsupported,"guild NPC envelope differs");
}
void client_remark_cases() {
    GuildUser user;user.character_id=43;GuildCatalog* catalog_ptr=nullptr;std::vector<std::string> events;std::vector<net::Packet> replies,db_packets,broadcasts;bool allocate=true,nest=false,in_nested=false;
    std::function<void(GuildUser&,net::Packet)> send_self=[&](GuildUser& target,net::Packet packet){require(&target==&user,"guild remark self owner differs");events.push_back("self");replies.push_back(std::move(packet));};
    std::function<void(net::Packet)> send_db=[&](net::Packet packet){events.push_back("db");db_packets.push_back(std::move(packet));};
    auto request=[](std::string_view text){net::Packet packet(67,0);packet[0]=0x20;packet[1]=0x0d;std::copy_n(text.begin(),std::min<std::size_t>(text.size(),64),packet.begin()+2);return packet;};
    GuildCatalogPorts ports;ports.allocate_guild=[] {return true;};ports.allocate_member=[] {return true;};
    ports.transport.allocate=[&](const net::Packet&){events.push_back("allocate");return allocate;};ports.transport.release=[&]{events.push_back("release");};
    ports.transport.send=[&](GuildUser& target,net::Packet packet){require(&target==&user,"guild remark broadcast owner differs");events.push_back("broadcast");broadcasts.push_back(std::move(packet));
        if(nest && !in_nested){in_nested=true;require(guild_client_remark(user,*catalog_ptr,request("Nested"),send_self,send_db)==EntryCatalogUpdate::applied,"nested guild remark failed");in_nested=false;}};
    GuildCatalog catalog(ports);catalog_ptr=&catalog;net::Packet load(3+135,0);load[0]=2;load[1]=11;load[2]=1;load[3]=77;
    require(catalog.apply(load)==EntryCatalogUpdate::applied,"guild remark catalog load failed");net::Packet member(7+29,0);member[0]=3;member[1]=11;member[2]=77;member[6]=1;member[7]=43;member[14]=1;std::copy_n("Member",6,member.begin()+15);
    require(catalog.apply(member)==EntryCatalogUpdate::applied,"guild remark member load failed");catalog.directory().initialize(user);auto* guild=catalog.find(77);require(guild && user.guild==&guild->membership && user.rank==1,"guild remark membership missing");events.clear();replies.clear();broadcasts.clear();
    user.rank=2;require(guild_client_remark(user,catalog,request("No"),send_self,send_db)==EntryCatalogUpdate::applied && events==std::vector<std::string>{"self"} && replies.back().size()==68 && replies.back()[2]==0 && guild->metadata.listing.notice.empty(),"nonleader remark differs");
    user.rank=1;events.clear();require(guild_client_remark(user,catalog,request("O'Brien"),send_self,send_db)==EntryCatalogUpdate::applied && events==std::vector<std::string>{"self"} && db_packets.empty(),"quoted remark was not rejected");
    auto* membership=user.guild;user.guild=nullptr;events.clear();require(guild_client_remark(user,catalog,request("NoGuild"),send_self,send_db)==EntryCatalogUpdate::applied && events.empty(),"guildless leader emitted response");user.guild=membership;
    bool missing=false;try{(void)guild_client_remark(user,catalog,request("Missing"),send_self,{});}catch(const std::invalid_argument&){missing=true;}require(missing && guild->metadata.listing.notice.empty(),"missing guild DB transport mutated remark");
    events.clear();broadcasts.clear();require(guild_client_remark(user,catalog,request("Hello"),send_self,send_db)==EntryCatalogUpdate::applied && events==std::vector<std::string>({"allocate","broadcast","release","db"}) && guild->metadata.listing.notice=="Hello" && broadcasts.back().size()==68 && broadcasts.back()[2]==1 && db_packets.back().size()==71 && db_packets.back()[0]==10 && db_packets.back()[2]==77,"guild remark success ordering differs");
    events.clear();db_packets.clear();allocate=false;require(guild_client_remark(user,catalog,request("NoBuffer"),send_self,send_db)==EntryCatalogUpdate::applied && events==std::vector<std::string>({"allocate","db"}) && guild->metadata.listing.notice=="NoBuffer","allocation failure lost DB mutation");allocate=true;
    events.clear();db_packets.clear();nest=true;require(guild_client_remark(user,catalog,request("Outer"),send_self,send_db)==EntryCatalogUpdate::applied && events==std::vector<std::string>({"allocate","broadcast","allocate","broadcast","release","db","release","db"}) && guild->metadata.listing.notice=="Nested" && db_packets.size()==2 && db_packets[0][6]=='N' && db_packets[1][6]=='O',"reentrant guild remark ordering differs");nest=false;
    auto max=request(std::string(64,'Z'));max[66]='\'';events.clear();require(guild_client_remark(user,catalog,max,send_self,send_db)==EntryCatalogUpdate::applied && guild->metadata.listing.notice==std::string(64,'Z'),"guild remark terminator boundary differs");
    require(guild_client_remark(user,catalog,net::Packet{0x20,0x0d},send_self,send_db)==EntryCatalogUpdate::rejected && guild_client_remark(user,catalog,net::Packet{0x21,0x0d},send_self,send_db)==EntryCatalogUpdate::unsupported,"guild remark envelope differs");
}
void client_npc_log_cases() {
    GuildMetadata metadata;metadata.listing.id=77;metadata.listing.name="GuildName";metadata.etin=9000;metadata.keep_etin=2200;
    GuildMemberPorts transport{[](const net::Packet&)->bool{throw std::runtime_error("unexpected broadcast allocation");},[]{throw std::runtime_error("unexpected broadcast release");},[](GuildUser&,net::Packet){throw std::runtime_error("unexpected broadcast send");}};
    LoadedGuild guild(metadata,transport);GuildUser user;user.character_id=0x55667788;
    GuildNpcLogUser value{0x11223344,0x55667788,0xabcdef01,0x01020304,0x05060708,0x090a0b0c,0x12345678,0x1234,0x4321,"Account","Character"};
    int captures=0,sends=0;net::Packet packet;
    auto log=guild_npc_log_transport([&](GuildUser& actual){require(&actual==&user,"log resolved wrong user");++captures;return value;},[&](net::Packet result){++sends;packet=std::move(result);});
    log(user,guild,2,1);
    require(packet==::packet("090b443322114163636f756e7400000000000000000000000088776655436861726163746572000000000000000000000000341201efcdab214304030201080706050c0b0a09785634124d0000004775696c644e616d6500000000000000000000000000000000000000000000000200012823000098080000"),"original full NPC log differs");
    require(captures==1 && sends==1 && packet.size()==121 && packet[0]==9 && packet[1]==11 && packet[2]==0x44 && packet[0x19]==0x88 && packet[0x32]==0x34 && packet[0x46]==0x78 && packet[0x4a]==77 && packet[0x6e]==2 && packet[0x70]==1 && packet[0x71]==0x28 && packet[0x72]==0x23 && packet[0x75]==0x98 && packet[0x76]==8,"guild NPC log serialization differs");
    require(std::string(packet.begin()+6,packet.begin()+13)=="Account" && std::string(packet.begin()+0x1d,packet.begin()+0x26)=="Character" && std::string(packet.begin()+0x4e,packet.begin()+0x57)=="GuildName","guild NPC log names differ");
    auto house_log=guild_house_log_packet(value,guild);auto expected=packet;expected.resize(110);expected[0]=8;require(house_log==expected,"guild house game-log common fields differ");
    guild.metadata.etin=42;value.time=99;log(user,guild,6,5);require(packet[0x71]==42 && packet[0x46]==99 && packet[0x6e]==6 && packet[0x70]==5,"guild NPC log did not capture live values");
    bool missing=false;try{(void)guild_npc_log_transport({},[](net::Packet){});}catch(const std::invalid_argument&){missing=true;}require(missing,"missing guild log owner accepted");
    value.account_name=std::string(19,'x');bool invalid=false;try{log(user,guild,2,1);}catch(const std::invalid_argument&){invalid=true;}require(invalid && sends==2,"invalid guild log name sent");
    auto failure=guild_npc_log_transport([&](GuildUser&){return GuildNpcLogUser{0,0,0,0,0,0,0,0,0,"",""};},[](net::Packet){throw std::runtime_error("transport failure");});bool propagated=false;try{failure(user,guild,2,1);}catch(const std::runtime_error&){propagated=true;}require(propagated,"guild log transport failure hidden");
}
void client_npc_config_cases() {
    GuildHouseConfiguration house;
    require(guild_house_apply_profile(house,"") && house.purchase_price==50000000 && house.npc.default_keep==2000,"guild house initial price differs");
    require(!guild_house_apply_profile(house,"[DefaultSet]\nHouseBuyMoney=12345\nHouseKeepEtin=4321\n") && house.purchase_price==12345 && house.npc.default_keep==4321,"guild house configured price differs");
    require(guild_house_apply_profile(house,"[DefaultSet]\nHouseBuyMoney=0\n") && house.purchase_price==0 && house.npc.default_keep==4321,"guild house zero price diagnostic differs");
    require(guild_house_apply_profile(house,"[DefaultSet]\nHouseBuyMoney=-1\n") && house.purchase_price==0xffffffffu,"guild house signed price diagnostic differs");
    require(guild_house_apply_profile(house,"") && house.purchase_price==0xffffffffu,"guild house reload lost retained price");
    require(!guild_house_apply_profile(house,"[DefaultSet]\nHouseBuyMoney=2147483647\n") && house.purchase_price==2147483647u,"guild house positive boundary differs");
    auto config=guild_npc_executable_defaults(2000);
    guild_npc_apply_profile(config,"[DefaultSet]\nHouseKeepEtin=3000\n[GateKeeper]\nUpgradeEtin=1250,77\nKeepEtin=321\n[ShopWeapon]\nUpgradeEtin=1,2,3\n[ShopDefen]\nUpgradeEtin=4,5,6\n[ShopPosion]\nUpgradeEtin=7,8,9\nKeepEtin=10,11,12\n[Bank]\nUpgradeEtin=13,14,15,16,17\nKeepEtin=18,19,20,21,22\n[General]\nUpgradeEtin=150,200,400\n[Smith]\nUpgradeEtin=23,24,25,26,27\nKeepEtin=100,200,500,800,1500\n");
    require(config.default_keep==3000 && config.costs[0][1]==1250 && config.costs[0][2]==77 && config.costs[0][3]==2000 && config.upkeep[0][1]==321 && config.upkeep[0][2]==500,"guild house partial override differs");
    require(config.costs[1][3]==3 && config.costs[2][3]==6 && config.costs[3][1]==150 && config.costs[4][5]==27 && config.costs[5][3]==9 && config.costs[6][5]==17 && config.upkeep[4][5]==1500 && config.upkeep[5][3]==12 && config.upkeep[6][5]==22,"guild house table mappings differ");
    guild_npc_apply_profile(config,"[GATEKEEPER]\nUpgradeEtin=-1,65537,bad,99\nKeepEtin={7=8} 9\n");
    require(config.default_keep==3000 && config.costs[0][1]==65535 && config.costs[0][2]==1 && config.costs[0][3]==0 && config.upkeep[0][1]==7 && config.upkeep[0][2]==8 && config.upkeep[0][3]==9,"guild house CRT token parsing differs");
    guild_npc_apply_profile(config,"[gatekeeper]\nUpgradeEtin=12\nUpgradeEtin=99,99,99\n");
    require(config.costs[0][1]==12 && config.costs[0][2]==1 && config.costs[0][3]==0,"guild house reload or first key differs");
    bool missing=false;try{(void)guild_npc_load_profile("/nonexistent-eternity-guild-house.ini");}catch(const std::runtime_error&){missing=true;}require(missing,"missing guild house configuration accepted");
}
int main(int argc,char** argv) {
    try {
        client_accept_cases();client_decline_cases();client_npc_log_cases();client_npc_config_cases();client_etin_cases();client_remark_cases();client_ignored_cases();client_rank_cases();client_npc_cases();
        require(argc==2,"Expected original guild catalog oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open guild catalog oracle");std::unique_ptr<GuildCatalog> catalog;std::vector<std::string> events,state;std::size_t next=0,field=0,cases=0,packets=0;unsigned int scenario=0,ga=0,ma=0;std::string line;
        GuildCatalogPorts port;port.transport={[](const net::Packet&)->bool{throw std::runtime_error("Load broadcast unexpectedly");},[](){throw std::runtime_error("Load release unexpectedly");},[](GuildUser&,net::Packet){throw std::runtime_error("Load sent client packet unexpectedly");}};
        port.allocate_guild=[&]{const auto ok=!(scenario%3==0 && ++ga%3==0);if(scenario%3!=0) ++ga;events.push_back("GALLOC "+std::to_string(ok));return ok;};port.allocate_member=[&]{const auto ok=!(scenario%4==0 && ++ma%4==0);if(scenario%4!=0) ++ma;events.push_back("MALLOC "+std::to_string(ok));return ok;};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {row>>scenario;catalog=std::make_unique<GuildCatalog>(port);ga=ma=0;++cases;}
            else if(tag=="PACKET") {std::string raw;row>>raw;events.clear();next=field=0;require(catalog->apply(packet(raw))==EntryCatalogUpdate::applied,"Original valid guild load rejected");state=snapshot(*catalog);BroadcastFixture fixture;CatalogDuelFixture duel(*catalog,fixture);
                for(const auto& [id,unused]:catalog->listings()){(void)unused;auto* guild=catalog->find(id);auto* state=duel.find_guild(id);require(state==&guild->duel && &duel.membership(*state)==&guild->membership && catalog->find_membership(&guild->membership)==guild,"Duel catalog identity differs");
                    auto lock=duel.lock(*state);auto nested=duel.lock(*state);require(lock && nested,"Recursive guild lock failed");}
                std::uint32_t missing=0;while(catalog->find(missing))++missing;require(duel.find_guild(missing)==nullptr,"Unknown guild duel found");++packets;}
            else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Guild catalog allocation order differs at packet "+std::to_string(packets));++next;}
            else if(tag=="MAX" || tag=="GUILD" || tag=="MEMBER") {require(field<state.size() && state[field]==line,"Original guild catalog state differs at packet "+std::to_string(packets)+": "+(field<state.size()?state[field]:"<none>")+" vs "+line);++field;}
            else if(tag=="ENDSTATE") require(next==events.size() && field==state.size(),"Extra native guild catalog effects/state");else if(tag!="END") throw std::runtime_error("Unknown guild catalog oracle row");
        }
        require(cases==96 && packets==768,"Guild catalog oracle coverage changed");
        std::cout<<cases<<" original guild catalog sequences, "<<packets<<" DB packets, first-key indexes, master correction and membership classification passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
