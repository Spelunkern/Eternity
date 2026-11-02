#include "eternity/game/character_core_save.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet unhex(const std::string& value) {require(value.size()%2==0,"Invalid character oracle hex");net::Packet result;for(std::size_t i=0;i<value.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));return result;}
std::string hex(std::string_view bytes) {constexpr char digits[]="0123456789abcdef";std::string result;for(unsigned char c:bytes) {result+=digits[c>>4];result+=digits[c&15];}return result;}
std::uint32_t get(const net::Packet& bytes,std::size_t at,unsigned int count) {std::uint32_t value=0;for(unsigned int i=0;i<count;++i) value|=static_cast<std::uint32_t>(bytes[at+i])<<(8*i);return value;}
PlayerCore load(std::uint32_t account,const net::Packet& bytes) {
    require(bytes.size()==99 && bytes.back()==0,"Invalid initial core payload");PlayerCore p;p.user_id=account;auto& c=p.character;
    c.id=get(bytes,0,4);c.slot=bytes[4];c.family=bytes[7];c.grow=bytes[8];c.hair=bytes[9];c.face=bytes[10];c.size=bytes[11];c.job=bytes[12];c.sex=bytes[13];c.level=static_cast<std::uint16_t>(get(bytes,14,2));p.stat_points=static_cast<std::uint16_t>(get(bytes,16,2));p.skill_points=static_cast<std::uint16_t>(get(bytes,18,2));p.experience=get(bytes,20,4);p.money=get(bytes,24,4);p.stored_money=get(bytes,28,4);
    for(std::size_t i=0;i<4;++i) p.kills[i]=get(bytes,32+4*i,4);p.kill_level=get(bytes,48,4);p.dead_level=get(bytes,52,4);c.map=static_cast<std::uint16_t>(get(bytes,56,2));p.direction=static_cast<std::uint16_t>(get(bytes,58,2));p.x=std::bit_cast<float>(get(bytes,60,4));p.y=std::bit_cast<float>(get(bytes,64,4));p.z=std::bit_cast<float>(get(bytes,68,4));p.hunger=static_cast<std::uint16_t>(get(bytes,72,2));p.vigor=std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(get(bytes,74,2)));p.cg=bytes[76];p.og=bytes[77];p.ig=bytes[78];
    c.strength=static_cast<std::uint16_t>(get(bytes,80,2));c.dexterity=static_cast<std::uint16_t>(get(bytes,82,2));c.intelligence=static_cast<std::uint16_t>(get(bytes,84,2));c.wisdom=static_cast<std::uint16_t>(get(bytes,86,2));c.recovery=static_cast<std::uint16_t>(get(bytes,88,2));c.luck=static_cast<std::uint16_t>(get(bytes,90,2));c.hp=static_cast<std::uint16_t>(get(bytes,92,2));c.mp=static_cast<std::uint16_t>(get(bytes,94,2));c.sp=static_cast<std::uint16_t>(get(bytes,96,2));return p;
}
net::Packet snapshot(const CharacterReplica& replica) {
    const auto p=replica.core();const auto& c=p.character;net::Packet bytes(99);
    const auto put=[&](std::size_t at,std::uint32_t value,unsigned int count){for(unsigned int i=0;i<count;++i) bytes[at+i]=static_cast<std::uint8_t>(value>>(8*i));};
    put(0,c.id,4);bytes[4]=c.slot;bytes[7]=c.family;bytes[8]=c.grow;bytes[9]=c.hair;bytes[10]=c.face;bytes[11]=c.size;bytes[12]=c.job;bytes[13]=c.sex;put(14,c.level,2);put(16,p.stat_points,2);put(18,p.skill_points,2);put(20,p.experience,4);put(24,p.money,4);put(28,static_cast<std::uint32_t>(p.stored_money),4);for(std::size_t i=0;i<4;++i) put(32+4*i,p.kills[i],4);put(48,p.kill_level,4);put(52,p.dead_level,4);put(56,c.map,2);put(58,p.direction,2);put(60,std::bit_cast<std::uint32_t>(p.x),4);put(64,std::bit_cast<std::uint32_t>(p.y),4);put(68,std::bit_cast<std::uint32_t>(p.z),4);put(72,p.hunger,2);put(74,std::bit_cast<std::uint16_t>(p.vigor),2);bytes[76]=p.cg;bytes[77]=p.og;bytes[78]=p.ig;put(80,c.strength,2);put(82,c.dexterity,2);put(84,c.intelligence,2);put(86,c.wisdom,2);put(88,c.recovery,2);put(90,c.luck,2);put(92,c.hp,2);put(94,c.mp,2);put(96,c.sp,2);bytes[98]=replica.body_changed();return bytes;
}
std::string encode(const CharacterCoreSaveOperation& op) {
    std::string result=op.procedure;for(auto& c:result) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');
    for(const auto& value:op.parameters) {result+=' ';if(const auto* i=std::get_if<std::int64_t>(&value)) result+='i'+std::to_string(*i);else if(const auto* s=std::get_if<std::string>(&value)) result+='s'+hex(*s);else throw std::runtime_error("Unexpected core save parameter");}return result;
}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original character core oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot read character oracle");InventoryReplicaPool pool;std::unique_ptr<CharacterReplica> replica;unsigned int steps=0,episodes=0,queries=0,acks=0;std::size_t next=0;std::vector<CharacterCoreSaveOperation> plan;
        for(std::string line;std::getline(input,line);) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="INIT") {std::uint32_t account=0;std::string bytes;row>>account>>bytes;replica=std::make_unique<CharacterReplica>(load(account,unhex(bytes)),pool);++episodes;}
            else if(tag=="STEP") {std::string packet,expected;unsigned int returned=0;row>>packet>>returned>>expected;require(returned==1 && replica->apply(unhex(packet))==InventoryReplicaResult::applied,"Original status acceptance differs at step "+std::to_string(steps));require(snapshot(*replica)==unhex(expected),"Original status cache differs at step "+std::to_string(steps));plan=character_core_save_plan(*replica);next=0;++steps;}
            else if(tag=="QUERY") {require(next<plan.size(),"Original emitted extra core SQL");const auto actual=encode(plan[next++]);require(actual==line.substr(6),"Core SQL differs at step "+std::to_string(steps)+"\nactual: "+actual+"\noriginal: "+line.substr(6));++queries;}
            else if(tag=="ENDSTEP") require(next==plan.size(),"Native emitted extra core SQL");
            else if(tag=="ACK") {std::string expected;row>>expected;replica->core_save_success();require(snapshot(*replica)==unhex(expected),"Core acknowledgment changed other state");++acks;}
            else throw std::runtime_error("Unknown core fixture row");
        }
        require(episodes==32 && steps==2048 && queries==2798 && acks==160,"Core oracle coverage changed");
        PlayerCore core;core.user_id=12345;core.character.id=17;core.money=99;CharacterReplica cache(core,pool);PlayerItem item;item.bag=1;item.uid=1;item.count=10;require(cache.inventory().load(item),"Cannot seed shared-money row");
        const net::Packet gem{8,7,0x39,0x30,0,0,1,0,0,7,77,0,0,0};require(cache.apply(gem)==InventoryReplicaResult::applied && cache.core().money==77 && cache.inventory().money()==77,"Core money diverged from gem/repair/craft authority");
        const auto before=snapshot(cache);
        for(const auto& p:std::vector<net::Packet>{{1,6},{3,6,0x38,0x30,0,0,1,0,0,0},{0x0c,6,0x39,0x30,0,0,6,1,0,0,0},{0x15,6,0x39,0x30,0,0,1,2,3}}) require(cache.apply(p)==InventoryReplicaResult::rejected,"Malformed/cross-owner status was accepted");
        require(snapshot(cache)==before,"Rejected core packet mutated state");for(const auto op:{0x611,0x612,0x613,0x614,0x616,0x617,0x618,0x619}) require(cache.apply(net::Packet{static_cast<std::uint8_t>(op),6,0x39,0x30,0,0})==InventoryReplicaResult::unsupported,"Other status owner was swallowed");
        std::cout<<steps<<" original core mutations and "<<queries<<" SQL calls passed, with shared item money and envelope checks\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
