#include "eternity/game/character_replica.hpp"
#include "eternity/game/interface_save.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet unhex(const std::string& value) {require(value.size()%2==0,"Invalid interface fixture hex");net::Packet result;for(std::size_t i=0;i<value.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));return result;}
std::string hex(std::string_view bytes) {constexpr char digits[]="0123456789abcdef";std::string result;for(unsigned char c:bytes) {result+=digits[c>>4];result+=digits[c&15];}return result;}
std::string encode(const InterfaceSaveOperation& op) {auto result=op.procedure;for(auto& c:result) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');for(const auto& p:op.parameters) {result+=' ';if(const auto* i=std::get_if<std::int64_t>(&p)) result+='i'+std::to_string(*i);else result+='s'+hex(std::get<std::string>(p));}return result;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected interface replica oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot read interface oracle");InventoryReplicaPool pool;std::unique_ptr<CharacterReplica> character;std::vector<InterfaceSaveOperation> plan;std::vector<std::string> attempted;std::size_t next=0;int failure=-1;unsigned int cases=0,episodes=0,acks=0,queries=0,attempts=0,soft_count=0,fatal_count=0;
        for(std::string line;std::getline(input,line);) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="INIT") {PlayerCore core;row>>core.user_id;core.character.id=500001;character=std::make_unique<CharacterReplica>(core,pool);for(std::uint8_t slot=0;slot<4;++slot) {unsigned int map=0;std::uint32_t x=0,y=0,z=0;row>>map>>x>>y>>z;require(character->interface().load_point(slot,static_cast<std::uint16_t>(map),std::bit_cast<float>(x),std::bit_cast<float>(y),std::bit_cast<float>(z)),"Cannot load interface fixture");}++episodes;}
            else if(tag=="ACK") {character->interface().save_success();++acks;}
            else if(tag=="OP") {std::string p;row>>p;require(character->apply(unhex(p))==InventoryReplicaResult::applied,"Interface packet acceptance differs");}
            else if(tag=="STATE") {std::size_t count=0;row>>count;require(character->interface().quick_slots().size()==count,"Quick count differs");for(const auto& slot:character->interface().quick_slots()) {unsigned int bar=0,pos=0,bag=0,number=0;row>>bar>>pos>>bag>>number;require(slot.bar==bar && slot.slot==pos && slot.bag==bag && slot.number==number,"Quick data differs");}for(const auto& point:character->interface().points()) {std::int32_t saved=0;unsigned int map=0;std::uint32_t x=0,y=0,z=0;row>>saved>>map>>x>>y>>z;require(point.save_type==saved && point.map==map && std::bit_cast<std::uint32_t>(point.x)==x && std::bit_cast<std::uint32_t>(point.y)==y && std::bit_cast<std::uint32_t>(point.z)==z,"Point payload/save flags differ at "+std::to_string(cases));}require(static_cast<bool>(row),"Truncated interface state");std::string extra;require(!(row>>extra),"Trailing interface state");++cases;}
            else if(tag=="QUICK" || tag=="POINTS") {std::string p;row>>p;require((tag=="QUICK"?character->interface().quick_slots_packet():character->interface().save_points_packet())==unhex(p),"DB-agent interface packet differs");}
            else if(tag=="SAVE") {bool quick=false;row>>quick>>failure;plan=interface_save_plan(character->interface(),500001,quick);next=0;attempted.clear();}
            else if(tag=="PLAN") {require(next<plan.size(),"Original emitted extra interface SQL");const auto actual=encode(plan[next++]);require(actual==line.substr(5),"Interface SQL differs at "+std::to_string(cases)+"\nactual: "+actual+"\noriginal: "+line.substr(5));++queries;}
            else if(tag=="ATTEMPT") {attempted.push_back(line.substr(8));++attempts;}
            else if(tag=="RESULT") {bool soft=false,fatal=false;row>>soft>>fatal;std::vector<std::string> actual;bool threw=false;InterfaceSaveResult result;try{result=execute_interface_save_plan(plan,[&](const auto& op){const auto index=actual.size();actual.push_back(encode(op));if(static_cast<int>(index)==failure) throw std::runtime_error("fixture");});}catch(const std::runtime_error&){threw=true;}require(threw==fatal && result.quick_failure.has_value()==soft && actual==attempted,"Interface SQL failure/continuation differs");soft_count+=soft;fatal_count+=fatal;}
            else if(tag=="END") require(next==plan.size(),"Native emitted extra interface SQL");
            else throw std::runtime_error("Unknown interface fixture row");
        }
        require(cases==1024 && episodes==16 && acks==64 && queries==5111 && attempts==4333 && soft_count>0 && fatal_count>0,"Interface oracle coverage changed");
        InterfaceReplica validation(12345);for(const auto& p:std::vector<net::Packet>{{9,6,0x39,0x30,0,0},{9,6,0x39,0x30,0,0,129},{9,6,0x38,0x30,0,0,0},{2,13,0x39,0x30,0,0,4,1,0,0,0,0,0,0,0,0,0,0,0,0,0}}) require(validation.apply(p)==InventoryReplicaResult::rejected,"Malformed/cross-owner interface packet accepted");require(validation.quick_slots().empty() && validation.save_points_packet().size()==7,"Rejected interface command mutated cache");require(validation.apply(net::Packet{3,13,0x39,0x30,0,0})==InventoryReplicaResult::unsupported,"Unknown interface command swallowed");std::vector<QuickSlot> overflow(129);require(!validation.load_quick_slots(overflow) && !validation.load_point(4,1,0,0,0),"Interface load exceeded original storage");
        PlayerSnapshot loaded;loaded.core.user_id=12345;loaded.quick_slots={{1,2,3,65535}};loaded.save_points={{2,1,1.25F,2.5F,3.75F},{2,3,4.5F,5.5F,6.5F},{8,9,1,2,3}};require(validation.load(loaded) && validation.quick_slots()[0].number==65535 && validation.points()[2].map==3 && validation.points()[2].save_type==0 && validation.points()[2].x==4.5F,"Typed interface load lost duplicates/slot filtering/WORD data");loaded.core.user_id=12346;require(!validation.load(loaded),"Cross-owner interface snapshot was loaded");
        std::cout<<cases<<" original interface states, "<<queries<<" SQL plans and "<<attempts<<" attempted calls passed; "<<soft_count<<" quick and "<<fatal_count<<" point errors\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
