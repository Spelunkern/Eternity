#include "eternity/game/guild_bank.hpp"
#include "equipment_fixture.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
using namespace eternity;using namespace eternity::game;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
std::string hex(std::span<const std::uint8_t> p) {std::ostringstream out;out<<std::hex<<std::setfill('0');for(auto b:p) out<<std::setw(2)<<unsigned(b);return out.str();}
net::Packet decode(const std::string& raw) {net::Packet p;for(std::size_t i=0;i<raw.size();i+=2) p.push_back(static_cast<std::uint8_t>(std::stoul(raw.substr(i,2),nullptr,16)));return p;}
std::uint64_t get(std::span<const std::uint8_t> p,std::size_t at,unsigned int n=4) {std::uint64_t out=0;for(unsigned int i=0;i<n;++i) out|=std::uint64_t{p[at+i]}<<(8*i);return out;}
void append(net::Packet& p,std::uint64_t value,unsigned int n) {for(unsigned int i=0;i<n;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
PlayerItem value(std::uint32_t id,std::span<const std::uint8_t> p) {PlayerItem v;v.id=id;v.uid=get(p,0,8);v.type=p[9];v.type_id=p[10];v.quality=static_cast<std::uint16_t>(get(p,11,2));std::copy_n(p.begin()+13,6,v.gems.begin());v.count=p[19];v.made_at=static_cast<std::uint32_t>(get(p,20));if(p[24]) v.make_type.assign(1,static_cast<char>(p[24]));v.craft.assign(p.begin()+25,std::find(p.begin()+25,p.end(),0));return v;}
PlayerItem seed(unsigned int i) {PlayerItem v;v.id=10000+i;v.uid=0x10000000000ULL+i;v.type=5;v.type_id=1;v.quality=static_cast<std::uint16_t>(100+i);v.count=static_cast<std::uint8_t>(i*7);for(unsigned int j=0;j<6;++j) v.gems[j]=static_cast<std::uint8_t>(i+j);v.craft=i%3==0?"01020304050607080903":i%3==1?" 01 02 ":"";v.made_at=0x6154c34e;v.make_type="M";return v;}
std::string item_line(const CachedInventoryItem& v) {
    const auto& p=v.item;net::Packet uid,types{p.type,p.type_id,p.count},gems;append(uid,p.uid,8);append(gems,p.quality,2);gems.insert(gems.end(),p.gems.begin(),p.gems.end());const net::Packet craft(p.craft.begin(),p.craft.end());
    return "ITEM "+std::to_string(p.slot)+' '+std::to_string(v.save_type)+' '+std::to_string(p.id)+' '+hex(uid)+' '+hex(types)+' '+hex(gems)+' '+(craft.empty()?"-":hex(craft))+' '+std::to_string(p.made_at)+' '+std::to_string(p.make_type.empty()?0:static_cast<unsigned char>(p.make_type[0]));
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original guild bank oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open guild bank oracle");
        std::unique_ptr<InventoryReplicaPool> db_pool;std::unique_ptr<GuildBankReplica> db;std::unique_ptr<GroundItemPool> game_pool;std::unique_ptr<GuildBankServices> services;std::unique_ptr<GuildBank> game;ItemDefinitions definitions;EnchantValues enchant;
        std::vector<std::string> items,changes;std::size_t next_change=0,next_delete=0,commands=0;std::vector<net::Packet> db_packets,client_packets,corrections;std::size_t next_item=0,next_db=0,next_client=0,next_correction=0,cases=0;unsigned int scenario=0,seed_count=0,count=0,pool_count=0,kind=0,limit=0,db_fresh=0,game_fresh=0,row_index=0;int fault=-1;bool active=false;std::uint32_t guild=0;std::string line;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {
                game.reset();services.reset();game_pool.reset();db.reset();db_pool.reset();row>>scenario>>seed_count>>count>>fault>>pool_count>>kind>>limit>>guild;active=false;db_fresh=game_fresh=row_index=0;next_delete=0;corrections.clear();items.clear();next_item=next_db=next_client=next_correction=0;
                db_pool=std::make_unique<InventoryReplicaPool>([&]()->std::unique_ptr<CachedInventoryItem>{if(active && ++db_fresh%5==0 && scenario%7==0) return {};return std::make_unique<CachedInventoryItem>();});db=std::make_unique<GuildBankReplica>(*db_pool);db->begin_load();for(unsigned int i=0;i<seed_count;++i) db->load(seed(9000+i),static_cast<std::int32_t>(i));db->finish_load(true);
                for(unsigned int i=0;i<pool_count;++i) {auto stored=std::make_unique<CachedInventoryItem>();stored->save_type=std::array<int,3>{0,1,17}[i%3];db_pool->release(std::move(stored));}active=true;db->begin_load();
                auto table=item_definitions();table.rows[0][0]="501";table.rows[0][10]="0";table.rows[0].back()=std::to_string(limit);definitions=ItemDefinitions::load(table);
                // The original oracle supplies this definition directly at its lookup port.
                const_cast<ItemDefinition*>(definitions.find(5,1))->real_type=static_cast<std::uint8_t>(kind);
                game_pool=std::make_unique<GroundItemPool>(1,[&]()->std::unique_ptr<GroundItem>{if(++game_fresh%7==0 && scenario%6==0) return {};return std::make_unique<GroundItem>();});services=std::make_unique<GuildBankServices>(GuildBankServices{definitions,enchant,*game_pool,[&](net::Packet p){corrections.push_back(std::move(p));},[](std::uint8_t,std::uint8_t){throw std::runtime_error("Unexpected missing definition");},[](std::uint8_t,std::uint8_t,std::uint8_t){}});game=std::make_unique<GuildBank>(*services);++cases;
            } else if(tag=="ROW") {std::int32_t slot=0;std::uint32_t id=0;std::string raw;row>>slot>>id>>raw;if(fault!=-2 && (fault<0 || row_index<static_cast<unsigned int>(fault))) db->load(value(id,decode(raw)),slot);++row_index;}
            else if(tag=="RETURN") {
                unsigned int returned=0;row>>returned;const bool success=fault==-1;require(returned==static_cast<unsigned int>(success),"Original guild bank SQL fault scenario differs");db->finish_load(success);for(const auto& item:db->snapshot()) items.push_back(item_line(item));db_packets=db->packets(guild);
                for(unsigned int repeat=0;repeat<1+scenario%2;++repeat) for(const auto& p:db_packets) require(game->apply(p)==GuildBankUpdate::applied,"Original game bank packet rejected");client_packets=game->client_packets();
            } else if(tag=="POOL") {std::size_t expected=0;row>>expected;require(db_pool->statistics().available==expected,"Original guild bank pool reuse/release differs at case "+std::to_string(cases));}
            else if(tag=="ITEM") {require(next_item<items.size() && items[next_item]==line,"Original guild bank cache differs at case "+std::to_string(cases)+": "+(next_item<items.size()?items[next_item]:"<none>")+" vs "+line);++next_item;}
            else if(tag=="DB") {require(next_db<db_packets.size() && hex(db_packets[next_db])==line.substr(3),"Original guild bank DB bytes differ at case "+std::to_string(cases));++next_db;}
            else if(tag=="CORRECTION") {require(next_correction<corrections.size() && hex(corrections[next_correction])==line.substr(11),"Original game bank count correction differs at case "+std::to_string(cases));++next_correction;}
            else if(tag=="CLIENT") {require(next_client<client_packets.size() && hex(client_packets[next_client])==line.substr(7),"Original guild bank client bytes differ at case "+std::to_string(cases));++next_client;}
            else if(tag=="CMD") {
                std::string raw;row>>raw;std::map<unsigned int,std::string> before,after;for(const auto& item:db->snapshot()) before.emplace(item.item.slot,item_line(item).substr(5));
                require(db->apply(decode(raw))==InventoryReplicaResult::applied,"Original guild bank mutation rejected");for(const auto& item:db->snapshot()) after.emplace(item.item.slot,item_line(item).substr(5));
                changes.clear();next_change=0;for(unsigned int slot=0;slot<240;++slot) {const auto old=before.find(slot),current=after.find(slot);if(old==before.end() && current==after.end()) continue;if(old!=before.end() && current!=after.end() && old->second==current->second) continue;changes.push_back(current==after.end()?"EMPTY "+std::to_string(slot):"CHANGE "+current->second);}++commands;
            }
            else if(tag=="CHANGE" || tag=="EMPTY") {require(next_change<changes.size() && changes[next_change]==line,"Original guild bank mutation differs at command "+std::to_string(commands)+": "+(next_change<changes.size()?changes[next_change]:"<none>")+" vs "+line);++next_change;}
            else if(tag=="DELETE") {std::uint64_t uid=0;row>>uid;require(next_delete<db->deleted().size() && db->deleted()[next_delete]==uid,"Original guild bank deletion journal differs");++next_delete;}
            else if(tag=="POOLCMD") {std::size_t expected=0;row>>expected;require(db_pool->statistics().available==expected,"Original guild bank mutation recycling differs");}
            else if(tag=="ENDCMD") require(next_change==changes.size() && next_delete==db->deleted().size(),"Extra native guild bank mutation/journal");
            else if(tag=="END") require(next_item==items.size() && next_db==db_packets.size() && next_client==client_packets.size() && next_correction==corrections.size() && row_index==count,"Extra native guild bank state/packets");else throw std::runtime_error("Unknown guild bank fixture row");
        }
        require(cases==96 && commands==1536,"Guild bank oracle coverage changed");
        std::cout<<cases<<" original guild bank SQL/list/pool/DB packet/game receive/correction/client sequences and "<<commands<<" bank mutations passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
