#include "eternity/game/character_replica.hpp"
#include "eternity/game/quest_save.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet unhex(const std::string& value) {require(value.size()%2==0,"Invalid quest fixture hex");net::Packet result;for(std::size_t i=0;i<value.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));return result;}
void compare(std::istream& row,const std::vector<CachedQuest>& actual,unsigned int index) {
    std::size_t count=0;row>>count;require(actual.size()==count,"Quest count differs at "+std::to_string(index));
    for(const auto& item:actual) {std::int32_t saved=0;unsigned int id=0,success=0,delay=0;std::array<unsigned int,3> counters{};row>>saved>>id>>success>>delay>>counters[0]>>counters[1]>>counters[2];require(item.save_type==saved && item.id==id && item.success==success && item.delay==delay && item.counters[0]==counters[0] && item.counters[1]==counters[1] && item.counters[2]==counters[2],"Quest payload/save flag differs at "+std::to_string(index));}
}
std::string encode(const QuestSaveOperation& op) {auto result=op.procedure;for(auto& c:result) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');for(const auto& p:op.parameters) result+=' '+std::to_string(std::get<std::int64_t>(p));return result;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected quest replica oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot read quest oracle");InventoryReplicaPool items;std::unique_ptr<QuestReplicaPool> pool;std::unique_ptr<CharacterReplica> character;bool allocate=true;unsigned int episodes=0,cases=0,acks=0,queries=0;std::size_t next=0;std::vector<QuestSaveOperation> plan;
        for(std::string line;std::getline(input,line);) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="INIT") {character.reset();pool=std::make_unique<QuestReplicaPool>([&]()->std::unique_ptr<CachedQuest>{return allocate?std::make_unique<CachedQuest>():nullptr;});allocate=true;PlayerCore core;row>>core.user_id;core.character.id=500001;character=std::make_unique<CharacterReplica>(core,items,nullptr,pool.get());std::size_t count=0;row>>count;
                for(std::size_t i=0;i<count;++i) {unsigned int id=0,delay=0;std::array<unsigned int,3> counters{};row>>id>>delay>>counters[0]>>counters[1]>>counters[2];require(character->quests().load({static_cast<std::uint16_t>(id),static_cast<std::uint16_t>(delay),{static_cast<std::uint8_t>(counters[0]),static_cast<std::uint8_t>(counters[1]),static_cast<std::uint8_t>(counters[2])}}),"Cannot load quest fixture");}
                row>>count;for(std::size_t i=0;i<count;++i) {unsigned int id=0,success=0;row>>id>>success;require(character->quests().load_finished({static_cast<std::uint16_t>(id),static_cast<std::uint8_t>(success)}),"Cannot load finished quest fixture");}++episodes;}
            else if(tag=="OP") {std::string packet;unsigned int accepted=0;row>>allocate>>packet>>accepted;require(accepted==1 && character->apply(unhex(packet))==InventoryReplicaResult::applied,"Quest opcode acceptance differs");}
            else if(tag=="ACK") {character->quests().save_success();++acks;}
            else if(tag=="STATE") {compare(row,character->quests().active(),cases);compare(row,character->quests().finished(),cases);std::size_t count=0;row>>count;require(count==character->quests().deleted().size(),"Quest deletion journal size differs");for(auto id:character->quests().deleted()) {unsigned int expected=0;row>>expected;require(id==expected,"Quest deletion journal order differs");}std::uint32_t attempts=0;std::size_t available=0;row>>attempts>>available;const auto statistics=pool->statistics();require(statistics.attempts==attempts && statistics.available==available,"Quest allocation/recycle order differs");require(static_cast<bool>(row),"Truncated quest state");std::string extra;require(!(row>>extra),"Trailing quest state");plan=quest_save_plan(character->quests(),500001);next=0;++cases;}
            else if(tag=="QUERY") {require(next<plan.size(),"Original emitted extra quest SQL");const auto actual=encode(plan[next++]);require(actual==line.substr(6),"Quest SQL differs at "+std::to_string(cases)+"\nactual: "+actual+"\noriginal: "+line.substr(6));++queries;}
            else if(tag=="END") require(next==plan.size(),"Native emitted extra quest SQL");
            else throw std::runtime_error("Unknown quest fixture row");
        }
        require(episodes==32 && cases==2048 && acks==160 && queries==26748,"Quest coverage changed");character.reset();pool.reset();
        bool available=false;QuestReplicaPool failure_pool([&]()->std::unique_ptr<CachedQuest>{return available?std::make_unique<CachedQuest>():nullptr;});QuestReplica cache(12345,failure_pool);require(!cache.load({5,10,{1,2,3}}) && !cache.load_finished({6,1}) && cache.active().empty() && cache.finished().empty(),"Quest row allocation failure was not skipped");available=true;require(cache.load({5,10,{1,2,3}}),"Cannot seed quest validation row");const auto before=cache.active();
        for(const auto& p:std::vector<net::Packet>{{1,10},{2,10,0x38,0x30,0,0,5,0,2},{5,10,0x39,0x30,0,0,5,0,3,99},{4,10,0x39,0x30,0,0,1,5,0,9,0,1,2}}) require(cache.apply(p)==InventoryReplicaResult::rejected,"Malformed/cross-owner quest packet accepted");require(cache.active()[0].delay==before[0].delay && cache.active()[0].counters==before[0].counters && cache.deleted().empty(),"Rejected quest command mutated state");
        require(cache.apply(net::Packet{6,10,0x39,0x30,0,0})==InventoryReplicaResult::unsupported,"Unknown quest command was swallowed");
        std::cout<<cases<<" original quest operations, "<<acks<<" complete save acknowledgments and "<<queries<<" SQL calls passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
