#include "eternity/game/quest_start_log.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <vector>
using namespace eternity::game;
namespace {
void check(bool b,const std::string& text) {if(!b) throw std::runtime_error(text);}
std::vector<std::uint8_t> decode(const std::string& s) {
    std::vector<std::uint8_t> r;if(s=="-") return r;
    for(std::size_t i=0;i<s.size();i+=2) r.push_back(static_cast<std::uint8_t>(std::stoul(s.substr(i,2),nullptr,16)));return r;
}
std::uint32_t number(const std::vector<std::uint8_t>& p,std::size_t at,unsigned n=4) {std::uint32_t r=0;for(unsigned i=0;i<n;++i) r|=std::uint32_t(p.at(at+i))<<(8*i);return r;}
std::string_view text(const std::vector<std::uint8_t>& p,std::size_t at,std::size_t n) {if(n==0) return {};return std::string_view(reinterpret_cast<const char*>(p.data()+at),n);}
struct World final:QuestStartWorld {
    QuestStartLogContext context;std::optional<QuestStartLogNpc> npc;
    std::array<std::uint8_t,144> prepared{},delivered{};std::array<std::uint8_t,8> client{};unsigned phase=0;
    std::optional<QuestStartNpc> find_npc(std::uint32_t) override {throw std::runtime_error("Unexpected lookup");}
    bool active(std::uint16_t) override {throw std::runtime_error("Unexpected lookup");}
    bool finished(std::uint16_t) override {throw std::runtime_error("Unexpected lookup");}
    bool find_item(std::uint8_t,std::uint8_t,bool) override {throw std::runtime_error("Unexpected requirement");}
    std::uint32_t tick_ms() override {throw std::runtime_error("Unexpected monotonic clock");}
    bool add(const QuestStartRecord&) override {throw std::runtime_error("Unexpected allocation");}
    void prepare_start_log(const QuestStartRecord& q,const std::optional<QuestStartNpc>&) override {check(phase++==0,"Snapshot order");write_quest_start_log(prepared,context,q,npc);}
    void send_self(const std::array<std::uint8_t,8>& p) override {check(phase++==1,"Client order");client=p;context.user_id=0xfefefefe;context.level=0xeded;context.character_id=0xedededed;}
    void log_start(const QuestStartRecord&,const std::optional<QuestStartNpc>&) override {check(phase++==2,"Log order");delivered=prepared;}
    bool give_item(std::uint8_t,std::uint8_t,std::uint8_t) override {throw std::runtime_error("Unexpected grant");}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original log fixture");std::ifstream input(argv[1]);check(input.good(),"Missing fixture");unsigned cases=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        unsigned id,fill,present;std::uint16_t year,month,day,hour,minute,second;std::uint32_t type,handle;std::string qhex,chex,nhex,clienthex,loghex;
        std::istringstream row(line);row>>id>>fill>>qhex>>chex>>present>>type>>nhex>>handle>>year>>month>>day>>hour>>minute>>second>>clienthex>>loghex;check(!row.fail(),"Malformed fixture");
        const auto qbytes=decode(qhex),c=decode(chex),name=decode(nhex),expected_client=decode(clienthex),expected_log=decode(loghex);
        check(qbytes.size()==464 && c.size()==74 && expected_log.size()==144 && expected_client.size()==8,"Invalid fixture sizes");
        check(quest_start_log_time(year,month,day,hour,minute,second)==number(c,70),"Original calendar packing mismatch");
        QuestStartRecord q;std::copy(qbytes.begin(),qbytes.end(),q.begin());World world;
        world.context={number(c,2),number(c,25),number(c,52),number(c,70),static_cast<std::uint16_t>(number(c,50,2)),static_cast<std::uint16_t>(number(c,56,2)),{number(c,58),number(c,62),number(c,66)},text(c,6,19),text(c,29,21)};
        if(present) world.npc=QuestStartLogNpc{type,text(name,0,name.size())};
        world.prepared.fill(static_cast<std::uint8_t>(fill));
        const auto fresh=quest_start_log_packet(world.context,q,world.npc);
        std::array<std::uint8_t,144> zero{};write_quest_start_log(zero,world.context,q,world.npc);check(fresh==zero,"Fresh packet mismatch");
        quest_start(q,present?std::optional(QuestStartNpc{handle,type,9}):std::nullopt,world);
        check(std::equal(world.delivered.begin(),world.delivered.end(),expected_log.begin()),"Original log mismatch "+std::to_string(id));
        check(std::equal(world.client.begin(),world.client.end(),expected_client.begin()),"Original client mismatch "+std::to_string(id));check(world.phase==3,"Missing delivery");++cases;
    }
    check(cases==256,"Insufficient log cases");
    QuestStartRecord q{};std::array<std::uint8_t,144> packet;packet.fill(0xa5);const auto unchanged=packet;QuestStartLogContext bad;bad.account_name="1234567890123456789";
    bool rejected=false;try {write_quest_start_log(packet,bad,q,{});} catch(const std::invalid_argument&) {rejected=true;}
    check(rejected && packet==unchanged,"Oversized string must be rejected before writes");
    std::cout<<cases<<" original complete logs/client packets matched; snapshot and retained tails verified\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
