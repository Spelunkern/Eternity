#include "eternity/game/guild_sql.hpp"
#include <iostream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main(int argc,char** argv) {
    try {
        require(argc==2 && std::string_view(argv[1]).starts_with("Eternity_Test_Guild_"),"Expected disposable guild catalog");const std::string catalog=argv[1];const auto config=storage::SqlServerConfig::from_environment();storage::SqlServer main(config),auxiliary(config);
        std::vector<net::Packet> packets;std::vector<std::string> errors;std::vector<std::uint32_t> banks;
        GuildSqlPorts port{[&](net::Packet p){packets.push_back(std::move(p));},[]{return true;},[&](std::uint32_t id){banks.push_back(id);},[&](const std::string& error){errors.push_back(error);}};
        GuildSql original(main,auxiliary,port);require(original.load_maximum(),"Original maximum guild ID query failed");
        require(!original.load() && !errors.empty() && errors.back().find("Rank")!=std::string::npos,"Restored Guild2 schema incompatibility changed; review the compatibility fixture");
        GuildSql load(main,auxiliary,port,catalog);packets.clear();errors.clear();require(load.load_maximum() && load.load(),"Disposable original guild procedures failed");
        require(errors.empty() && banks==std::vector<std::uint32_t>{10,20},"New metadata replicas did not invoke required bank/NPC owners once");
        require(load.replicas().at(10).npcs.slots[0].type==21 && load.replicas().at(10).npcs.slots[0].level==3 && load.replicas().at(10).npcs.slots[7].type==65535 && load.replicas().at(10).npcs.slots[7].level==255,"Actual NPC SQL loading lost signed type bits/byte levels or ignored slot bounds");
        require(packets.size()==5 && packets[0]==net::Packet({1,0x0b,31,0,0,0}) && packets[1].size()==138 && packets[2].size()==1747 && packets[2][6]==60 && packets[3][6]==3 && packets[4].size()==138,"Combined SQL guild/member framing or 60-row batches changed");
        load.send_npcs();require(packets.size()==6 && packets.back()==net::Packet({0x0e,0x0b,10,0,0,0,2,21,0,3,0,255,255,255,7}),"NPC replica did not emit original slot-ordered DB packet");
        GuildCatalog game({{[](const net::Packet&){return true;},[]{},[](GuildUser&,net::Packet){}},[]{return true;},[]{return true;}});
        for(const auto& p:packets) require(game.apply(p)==EntryCatalogUpdate::applied,"SQL guild packet rejected by actual game cache");
        auto* guild=game.find(10);require(guild && game.find("NATIVEGUILD")==guild && game.maximum_id()==31 && guild->metadata.listing.points==100 && guild->metadata.listing.rank==2 && guild->metadata.etin==300 && guild->metadata.return_count==7 && guild->metadata.keep_etin==0,"SQL-to-game metadata projection changed");
        require(guild->npcs.client_packet()==std::optional{net::Packet({0x24,0x0d,2,21,0,3,0,255,255,255,7})},"SQL NPC state did not reach the actual client packet");
        require(load.replicas().at(10).metadata.keep_etin==9999 && game.find(20)->metadata.listing.rank==31,"DB replica KeepEtin or SQL missing-details defaults changed");
        require(guild->membership.offline.size()==61 && guild->membership.applications.size()==2 && guild->metadata.listing.master=="NewMaster" && guild->membership.offline.at(1000)->rank==1,"SQL guild master correction/member classification changed");
        std::vector<net::Packet> delivered;GuildCatalog live({{[](const net::Packet&){return true;},[]{},[&](GuildUser&,net::Packet p){delivered.push_back(std::move(p));}},[]{return true;},[]{return true;}});
        for(const auto& p:packets) live.apply(p);GuildUser user;user.character_id=1001;live.directory().initialize(user);require(user.guild_id==10 && user.rank==1 && user.guild && !delivered.empty(),"SQL guild member did not reach original client roster delivery");live.directory().release(user);
        const auto prefix="["+catalog+"].dbo.";main.query("UPDATE "+prefix+"GuildDetails SET Etin=777,KeepEtin=888,Remark='Changed' WHERE GuildID=10; UPDATE "+prefix+"Guilds SET GuildName='Reloaded' WHERE GuildID=10;");
        packets.clear();require(load.load() && banks.size()==2 && load.replicas().at(10).metadata.etin==300,"Metadata reload replaced existing replica or repeated nested loading");
        GuildCatalog reloaded({{[](const net::Packet&){return true;},[]{},[](GuildUser&,net::Packet){}},[]{return true;},[]{return true;}});for(const auto& p:packets) reloaded.apply(p);require(reloaded.find(10)->metadata.listing.name=="Reloaded" && reloaded.find(10)->metadata.etin==300 && reloaded.find(10)->metadata.listing.notice=="Hello","Reload lost SQL identity/cached guild state split");
        main.query("UPDATE "+prefix+"GuildNpcLv SET NpcLevel=5 WHERE GuildID=10 AND Number=0;");packets.clear();load.send_npcs();require(packets.size()==1 && packets.front()[9]==3,"Repeated metadata loading reloaded an existing NPC replica");
        main.query("USE ["+catalog+"]; EXEC(N'ALTER PROCEDURE dbo.usp_Read_GuildNpcLv_R @GuildID int AS SELECT CAST(V.T AS smallint) AS GNpcType,V.L AS NpcLevel,CAST(V.N AS tinyint) AS Number FROM (VALUES(37,2,3),(38,256,4)) V(T,L,N) WHERE @GuildID=10 ORDER BY V.N;');");
        auto partial_ports=port;partial_ports.load_bank=[](std::uint32_t){};GuildSql partial(main,auxiliary,partial_ports,catalog);packets.clear();errors.clear();require(partial.load() && !errors.empty() && partial.replicas().at(10).npcs.slots[3].type==37 && partial.replicas().at(10).npcs.slots[3].level==2 && partial.replicas().at(10).npcs.slots[4].type==0,"NPC conversion error did not retain prior rows or incorrectly aborted outer guild loading");packets.clear();partial.send_npcs();require(packets==std::vector<net::Packet>{{0x0e,0x0b,10,0,0,0,1,37,0,2,3}},"Partial NPC SQL replica did not reach original DB output");
        main.query("UPDATE "+prefix+"Chars SET CharName=REPLICATE('X',21) WHERE CharID=1062;");packets.clear();errors.clear();require(load.load() && !errors.empty(),"Member conversion failure incorrectly aborted the outer guild loader or reported success silently");
        require(packets.size()==3 && packets[1][0]==3 && packets[1][6]==60 && packets[2][0]==2,"Member conversion failure lost completed batches or leaked its partial batch");
        main.query("UPDATE "+prefix+"GuildDetails SET Remark=REPLICATE('R',64) WHERE GuildID=10;");errors.clear();packets.clear();require(!load.load() && !errors.empty() && packets.empty(),"Guild GetData string truncation did not stop metadata loading");
        std::cout<<"Guild SQL procedures, restored schema diagnostic, interleaved DB/game/client loading, cached reload, nested owner calls and conversion failures passed in a disposable catalog.\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
