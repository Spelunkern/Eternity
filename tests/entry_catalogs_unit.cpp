#include "eternity/game/entry_catalogs.hpp"
#include <bit>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet unhex(const std::string& value) {net::Packet p;if(value=="-") return p;for(std::size_t i=0;i<value.size();i+=2) p.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));return p;}
std::string text(const std::string& value) {const auto p=unhex(value);return {p.begin(),p.end()};}
std::string hex(const net::Packet& p) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto value:p) out<<std::setw(2)<<unsigned(value);return out.str();}
void append(net::Packet& p,std::uint32_t value,unsigned int bytes=4) {for(unsigned int i=0;i<bytes;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
void fixed(net::Packet& p,const std::string& value,std::size_t width) {p.insert(p.end(),value.begin(),value.end());p.resize(p.size()+width-value.size(),0);}
net::Packet db(std::uint16_t op) {net::Packet p;append(p,op,2);append(p,530001);p.push_back(1);return p;}
void reentrant_notices() {
    for(bool online:{false,true})for(unsigned mode=0;mode<3;++mode) {
        SocialCatalog social(71,0x12345678),peer(72,5);social.add_friend({5,0,0,0,"Five",""});social.add_friend({8,0,0,0,"Eight",""});if(mode!=2)social.add_friend({9,0,0,0,"Nine",""});
        std::vector<std::string> effects;bool changed=false;
        const auto remove=[&]{const auto ignore=[](net::Packet){};const auto absent=[](std::uint32_t)->const SocialCatalog*{return nullptr;};
            require(social.apply_client(net::Packet{5,0x22,5,0,0,0},ignore,ignore,absent,[](std::uint32_t,net::Packet){})==EntryCatalogUpdate::applied,"reentrant friend removal rejected");};
        const auto lookup=[&](std::uint32_t id)->const SocialCatalog* {effects.push_back("lookup "+std::to_string(id));if(mode==0 && !changed){changed=true;remove();}return id==5 || id==9?&peer:nullptr;};
        const auto send=[&](std::uint32_t id,net::Packet p){effects.push_back("send "+std::to_string(id));require(p==net::Packet({7,0x22,0x78,0x56,0x34,0x12,static_cast<std::uint8_t>(online)}),"reentrant notice packet differs");
            if(!changed && mode==1){changed=true;remove();}else if(!changed && mode==2){changed=true;social.add_friend({9,0,0,0,"Nine",""});}};
        if(online)social.notify_online(lookup,send);else social.notify_offline(lookup,send);
        const auto expected=mode==2?std::vector<std::string>{"lookup 5","send 5","lookup 8","lookup 9","send 9"}:std::vector<std::string>{"lookup 5","send 5","lookup 8"};
        require(changed && effects==expected,"reentrant social notice iteration differs");
    }
}
void reentrant_lists() {
    for(unsigned mode=0;mode<7;++mode) {
        SocialCatalog social(71,101),peer(72,2);peer.add_friend({101,0,0,0,"Owner",""});const unsigned friends=mode==0 || mode==5 || mode==6?1:mode==1?2:mode==2?25:mode==3?26:0;const unsigned blocks=mode==4?25:0;
        for(unsigned i=1;i<=friends;++i)social.add_friend({i,0,0,static_cast<std::uint8_t>(i),"F"+std::to_string(i),""});
        for(unsigned i=1;i<=blocks;++i)social.add_block({i,"B"+std::to_string(i),""});
        std::vector<net::Packet> packets;bool changed=false;
        const auto remove=[&]{const auto ignore=[](net::Packet){};const auto absent=[](std::uint32_t)->const SocialCatalog*{return nullptr;};
            require(social.apply_client(net::Packet{5,0x22,1,0,0,0},ignore,ignore,absent,[](std::uint32_t,net::Packet){})==EntryCatalogUpdate::applied,"list friend removal rejected");};
        const auto lookup=[&](std::uint32_t)->const SocialCatalog* {if((mode<=1 || mode==6) && !changed){changed=true;remove();}return mode==6?&peer:nullptr;};
        const auto send=[&](net::Packet packet){packets.push_back(std::move(packet));if(changed)return;
            if(mode==2 || mode==5){changed=true;social.add_friend({26,0,0,26,"F26",""});}
            else if(mode==3){changed=true;remove();}
            else if(mode==4 && packets.back()[0]==8){changed=true;social.add_block({26,"B26",""});}};
        social.send_packets(lookup,send);require(changed && !packets.empty(),"list reentrant mutation missing");
        if(mode==0)require(packets.size()==1 && packets[0].size()==31 && packets[0][2]==1 && packets[0][3]==0 && packets[0][7]==0 && packets[0][9]==0,"removed final fixed slot differs");
        if(mode==6)require(packets.size()==1 && packets[0][3]==0 && packets[0][8]==1,"mutual status did not use lookup before slot mutation");
        if(mode==1)require(packets.size()==1 && packets[0][2]==1 && packets[0][3]==2 && packets[0][7]==2,"lookup-time swapped row differs");
        if(mode==2 || mode==4)require(packets.size()==2 && packets[0][2]==25 && packets[1][2]==1 && packets[1][3]==26,"batch-boundary append differs");
        if(mode==3)require(packets.size()==1 && packets[0][2]==25 && social.friends().size()==25,"batch-boundary deletion differs");
        if(mode==5)require(packets.size()==1 && packets[0][2]==1 && social.friends().size()==2,"final flush incorrectly revisited friends");
    }
}
}
int main(int argc,char** argv) {
    try {
        reentrant_notices();reentrant_lists();
        require(argc==2,"Expected original entry catalog oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open entry catalog oracle");std::string line,kind;std::vector<std::string> events;std::size_t social=0,catalogs=0,kills=0,honors=0,guilds=0,sends=0,next=0;std::unique_ptr<SocialCatalog> cache,mutual,one_way;std::unique_ptr<EntryCatalogs> items;std::map<std::uint32_t,GuildListing> listings;unsigned int presence=0;std::uint8_t country=0;
        const auto packet=[&](net::Packet p){events.push_back("PACKET "+hex(p));};
        const auto lookup=[&](std::uint32_t id)->const SocialCatalog* {events.push_back("LOOKUP "+std::to_string(id));const bool present=presence==1 || presence==3 || (presence==2 && id%3!=0);const bool related=(presence==1 && id%2==0) || (presence==2 && id%5==1);return present?(related?mutual.get():one_way.get()):nullptr;};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="SOCIAL") {std::uint32_t id=0;std::size_t friends=0,blocked=0;row>>id>>friends>>blocked>>presence;cache=std::make_unique<SocialCatalog>(530001,id);mutual=std::make_unique<SocialCatalog>(1,0);one_way=std::make_unique<SocialCatalog>(1,0);mutual->add_friend({id,0,0,0,"",""});events.clear();next=0;kind=tag;++social;}
            else if(tag=="FRIEND") {FriendEntry f;unsigned int family=0,grow=0,job=0;std::string name,memo;row>>f.id>>family>>grow>>job>>name>>memo;auto p=db(0xc01);append(p,f.id);p.insert(p.end(),{static_cast<std::uint8_t>(family),static_cast<std::uint8_t>(grow),static_cast<std::uint8_t>(job)});fixed(p,text(name),21);fixed(p,text(memo),51);require(cache->apply_db(p)==EntryCatalogUpdate::applied,"Valid friend DB packet rejected");}
            else if(tag=="BLOCK") {std::uint32_t id=0;std::string name,memo;row>>id>>name>>memo;auto p=db(0xc05);append(p,id);fixed(p,text(name),21);fixed(p,text(memo),51);require(cache->apply_db(p)==EntryCatalogUpdate::applied,"Valid block DB packet rejected");}
            else if(tag=="COUNTS") {std::size_t f=0,b=0;row>>f>>b;require(cache->friends().size()==f && cache->blocked().size()==b,"Original social append/cap behavior differs");cache->notify_online(lookup,[&](std::uint32_t id,net::Packet p){events.push_back("PEER "+std::to_string(id)+' '+hex(p));});cache->send_packets(lookup,packet);}
            else if(tag=="CATALOG") {std::uint32_t id=0;row>>id;items=std::make_unique<EntryCatalogs>(id);events.clear();next=0;kind=tag;++catalogs;}
            else if(tag=="DB") {std::string bytes;row>>bytes;require(items->apply_db(unhex(bytes))==EntryCatalogUpdate::applied,"Original valid catalog DB update rejected");}
            else if(tag=="KC") {unsigned int side=0;std::uint32_t points=0,expires=0,tick=0;row>>side>>points>>expires>>tick;events.clear();next=0;packet(character_kill_count(static_cast<std::uint8_t>(side),points,expires,tick));++kills;}
            else if(tag=="HONOR") {std::uint16_t value=0;row>>value;events.clear();next=0;packet(character_honor(value));++honors;}
            else if(tag=="GUILD") {unsigned int side=0;row>>side;country=static_cast<std::uint8_t>(side);listings.clear();events.clear();next=0;kind=tag;++guilds;}
            else if(tag=="GUILD_ROW") {std::uint32_t key=0;GuildListing value;unsigned int rank=0;std::string name,master,notice;row>>key>>value.id>>value.country>>value.points>>rank>>name>>master>>notice;value.rank=static_cast<std::uint8_t>(rank);value.name=text(name);value.master=text(master);value.notice=text(notice);listings.emplace(key,std::move(value));}
            else if(tag=="SEND") {events.clear();next=0;if(kind=="GUILD") for(auto p:guild_listings(listings,country)) packet(std::move(p));else {if(auto p=items->save_point_packet()) packet(std::move(*p));packet(items->history_packet());if(auto p=items->stored_packet()) packet(std::move(*p));}++sends;}
            else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Original entry catalog effect differs at social "+std::to_string(social)+" catalogs "+std::to_string(catalogs)+" guild "+std::to_string(guilds)+" effect "+std::to_string(next)+": "+(next<events.size()?events[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="END") require(next==events.size(),"Native catalog emitted extra effects");
            else throw std::runtime_error("Unknown entry catalog oracle row");
        }
        require(social==240 && catalogs==96 && kills==180 && honors==5 && guilds==36 && sends==420,"Entry catalog oracle coverage changed");
        // Reject wrong owners/truncated frames without clearing loaded data.
        EntryCatalogs owned(530001);auto malformed=db(0xd01);malformed[2]^=1;require(owned.apply_db(malformed)==EntryCatalogUpdate::rejected,"Foreign catalog update accepted");malformed=db(0xe01);require(owned.apply_db(malformed)==EntryCatalogUpdate::rejected,"Short history header accepted");SocialCatalog social_owner(530001,9);malformed=db(0xc01);require(social_owner.apply_db(malformed)==EntryCatalogUpdate::rejected && social_owner.friends().empty(),"Truncated social row mutated cache");
        std::array<EntrySavePoint,4> raw;raw[0].map=65536;raw[0].x=std::bit_cast<float>(0x7fa01234U);const auto point=character_save_points(raw);require(point && (*point)[2]==1 && (*point)[3]==1 && (*point)[4]==0 && (*point)[5]==0,"Save point DWORD sentinel was narrowed before its original check");
        std::cout<<social<<" original social caches/notifications, "<<catalogs<<" stateful catalog sequences, "<<kills<<" kill-count clocks, "<<honors<<" honor and "<<guilds<<" guild-list scenarios passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
