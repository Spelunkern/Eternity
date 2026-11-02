#include "eternity/game/guild_bank_save.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(const std::string& value) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(unsigned char b:value) out<<std::setw(2)<<unsigned(b);return out.str();}
std::string operation(const GuildBankSaveOperation& op) {std::string result=op.procedure;for(const auto& p:op.parameters) {result+=' ';if(const auto* n=std::get_if<std::int64_t>(&p)) result+='i'+std::to_string(*n);else result+='s'+hex(std::get<std::string>(p));}return result;}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original guild bank save oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open guild bank save oracle");
        std::array<std::unique_ptr<CachedInventoryItem>,240> owned;std::array<CachedInventoryItem*,240> slots{};std::vector<std::uint64_t> deleted;std::vector<std::string> trace;std::size_t next=0,cases=0,passes=0,attempt=0;std::uint32_t guild=0,returned=0;unsigned int mode=0,fail_at=0,turn=0;std::string line;
        GuildBankSavePorts ports{[&]{const bool ok=turn!=0 || mode!=1;trace.push_back("ACQUIRE "+std::to_string(ok));return ok;},[&]{trace.push_back("RELEASE");},[&]{trace.push_back("PREPARE");},[&](const GuildBankSaveOperation& op){const auto status=turn==0 && mode>=6?mode-5:turn==0 && mode>=2 && attempt==fail_at?mode-1:0;++attempt;trace.push_back("EXEC "+operation(op)+" | "+std::to_string(status));return static_cast<GuildBankSqlResult>(status);},[&](GuildBankSqlResult){trace.push_back("ERROR");}};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {row>>guild>>mode>>fail_at;for(auto& p:owned) p.reset();slots.fill(nullptr);deleted.clear();++cases;}
            else if(tag=="DEL") {std::uint64_t uid=0;row>>uid;deleted.push_back(uid);}
            else if(tag=="ITEM") {
                unsigned int slot=0,type=0,id=0,count=0,quality=0,make=0;std::string gems,craft;auto item=std::make_unique<CachedInventoryItem>();auto& v=item->item;
                row>>slot>>v.uid>>v.id>>type>>id>>count>>quality>>gems>>craft>>v.made_at>>make>>item->save_type;v.type=static_cast<std::uint8_t>(type);v.type_id=static_cast<std::uint8_t>(id);v.count=static_cast<std::uint8_t>(count);v.quality=static_cast<std::uint16_t>(quality);v.make_type.assign(1,static_cast<char>(make));v.craft=craft=="-"?"":craft;
                for(std::size_t i=0;i<6;++i) v.gems[i]=static_cast<std::uint8_t>(std::stoul(gems.substr(i*2,2),nullptr,16));slots.at(slot)=item.get();owned.at(slot)=std::move(item);
            } else if(tag=="PASS") {row>>turn;attempt=next=0;trace.clear();returned=save_guild_bank(guild,slots,deleted,ports);++passes;}
            else if(tag=="TRACE") {require(next<trace.size() && trace[next]==line.substr(6),"Original guild bank save trace differs at case "+std::to_string(cases)+" pass "+std::to_string(turn)+": "+(next<trace.size()?trace[next]:"<none>")+" vs "+line.substr(6));++next;}
            else if(tag=="RETURN") {std::uint32_t expected=0;row>>expected;require(returned==expected,"Original bank save error count differs");}
            else if(tag=="FLAG") {unsigned int slot=0;std::uint32_t expected=0;row>>slot>>expected;require(slots.at(slot) && static_cast<std::uint32_t>(slots[slot]->save_type)==expected,"Original bank save marker differs");}
            else if(tag=="JOURNAL") {std::vector<std::uint64_t> expected;std::uint64_t uid=0;while(row>>uid) expected.push_back(uid);require(deleted==expected,"Original bank save journal retention/insert recovery differs");}
            else if(tag=="ENDPASS") require(next==trace.size(),"Extra native bank persistence operation");
            else if(tag!="END") throw std::runtime_error("Unknown bank save fixture row");
        }
        require(cases==144 && passes==288,"Original bank save coverage changed");std::cout<<cases<<" original guild bank save scenarios and "<<passes<<" passes matched SQL arguments, errors, markers and journals\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
