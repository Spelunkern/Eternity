#include "eternity/game/guild_sql.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string decode(const std::string& value) {if(value=="-") return {};std::string out;for(std::size_t i=0;i<value.size();i+=2) out.push_back(static_cast<char>(std::stoul(value.substr(i,2),nullptr,16)));return out;}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original guild remark oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open guild remark oracle");GuildDbMetadata guild;std::string query,line;std::size_t cases=0;
        GuildSavePorts ports{[]{return true;},[]{},[]{},[&](const GuildSaveOperation& operation){query=guild_save_sql(operation);return GuildSaveResult{};},[](GuildDbMetadata&){return 0;},[](GuildDbMetadata&){return 0;},[](const GuildSaveResult&){throw std::runtime_error("Unexpected source remark query failure port");}};
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {guild=GuildDbMetadata{};auto& m=guild.metadata;unsigned int buy=0,rank=0,use=0;row>>m.listing.id>>m.listing.points>>m.etin>>m.return_count>>m.keep_etin>>buy>>rank>>use;m.buy_house=static_cast<std::uint8_t>(buy);m.listing.rank=static_cast<std::uint8_t>(rank);m.use_house=static_cast<std::uint8_t>(use);m.listing.notice="Previous";++cases;}
            else if(tag=="PACKET") {std::string raw;row>>raw;const auto packet=decode(raw);require(packet.size()==71,"Original remark packet width changed");guild.set_remark(std::string_view(packet).substr(6));}
            else if(tag=="NOTICE") {std::string raw;row>>raw;require(guild.metadata.listing.notice==decode(raw),"Original remark copying/forced terminator differs");}
            else if(tag=="DIRTY" || tag=="FINAL") {std::int32_t expected=0;row>>expected;require(guild.save_type==expected,"Original remark dirty marker differs");}
            else if(tag=="SQL") {std::array<GuildDbMetadata*,1> ordered{&guild};require(save_guilds(ordered,ports),"Original remark coordinator rejected");std::string raw;row>>raw;require(query==decode(raw),"Original remark formatted SQL differs at case "+std::to_string(cases));}
            else if(tag!="END") throw std::runtime_error("Unknown guild remark fixture row");
        }
        require(cases==128,"Original guild remark coverage changed");std::cout<<cases<<" original guild remark copies and formatted SQL queries matched quotes, terminators and integer widths\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
