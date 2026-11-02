#include "eternity/game/character_replica.hpp"
#include "eternity/game/skill_save.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet unhex(const std::string& value) {require(value.size()%2==0,"Invalid skill fixture hex");net::Packet result;for(std::size_t i=0;i<value.size();i+=2) result.push_back(static_cast<std::uint8_t>(std::stoul(value.substr(i,2),nullptr,16)));return result;}
void compare(std::istream& row,const std::vector<CachedSkill>& actual,const std::string& where) {
    std::size_t count=0;row>>count;require(actual.size()==count,"Skill cache count differs "+where);
    for(const auto& item:actual) {unsigned int id=0,level=0,number=0;std::uint32_t delay=0;row>>id>>level>>number>>delay;require(item.id==id && item.level==level && item.number==number && item.delay==delay,"Skill cache payload differs "+where);}
}
std::string encode(const SkillSaveOperation& op) {auto result=op.procedure;for(auto& c:result) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');for(const auto& p:op.parameters) result+=' '+std::to_string(std::get<std::int64_t>(p));return result;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original skill replica fixture");std::ifstream input(argv[1]);require(input.good(),"Cannot read skill replica fixture");
        InventoryReplicaPool inventory_pool;std::unique_ptr<SkillReplicaPool> pool;std::unique_ptr<CharacterReplica> character;bool allocate=true;std::uint32_t account=0;unsigned int cases=0,episodes=0,queries=0,resets=0,acks=0;std::size_t next=0;std::vector<SkillSaveOperation> plan;net::Packet sent;
        for(std::string line;std::getline(input,line);) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="INIT") {character.reset();pool=std::make_unique<SkillReplicaPool>([&]()->std::unique_ptr<CachedSkill>{return allocate?std::make_unique<CachedSkill>():nullptr;});allocate=true;unsigned int points=0;std::size_t count=0;row>>account>>points>>count;std::vector<PlayerSkill> learned;std::vector<PersistedEffect> effects;
                for(std::size_t i=0;i<count;++i) {unsigned int id=0,level=0,delay=0;row>>id>>level>>delay;learned.push_back({static_cast<std::uint16_t>(id),static_cast<std::uint16_t>(delay),static_cast<std::uint8_t>(level),255,255});}
                row>>count;for(std::size_t i=0;i<count;++i) {unsigned int id=0,level=0;std::uint32_t delay=0;row>>id>>level>>delay;effects.push_back({static_cast<std::uint16_t>(id),static_cast<std::uint8_t>(level),delay});}
                PlayerCore core;core.user_id=account;core.character.id=500001;core.skill_points=static_cast<std::uint16_t>(points);character=std::make_unique<CharacterReplica>(core,inventory_pool,pool.get());require(character->skills().load(learned,effects),"Cannot load skill fixture");++episodes;}
            else if(tag=="OP") {std::string packet;unsigned int accepted=0;row>>allocate>>packet>>accepted;require(accepted==1 && character->apply(unhex(packet))==InventoryReplicaResult::applied,"Original skill acceptance differs");sent.clear();}
            else if(tag=="RESET") {unsigned int points=0;row>>points;sent=character->skills().finish_reset(static_cast<std::uint16_t>(points));++resets;}
            else if(tag=="ACK") {character->skills().save_success();sent.clear();++acks;}
            else if(tag=="SEND") {std::string expected;row>>expected;require(sent==unhex(expected),"Skill reset completion packet differs");sent.clear();}
            else if(tag=="STATE") {unsigned int points=0,saved=0,reset=0;row>>points>>saved>>reset;const auto where="at skill operation "+std::to_string(cases);require(character->core().skill_points==points && character->skills().saved_count()==saved && character->skills().reset_pending()==static_cast<bool>(reset),"Skill points/save markers differ "+where);compare(row,character->skills().learned(),where);compare(row,character->skills().effects(),where);
                std::uint32_t attempts=0;std::size_t available=0,stranded=0;row>>attempts>>available>>stranded;const auto stats=pool->statistics();require(stats.attempts==attempts && stats.available==available && stats.stranded==stranded,"Skill heap reuse/allocation/stranding differs "+where);require(static_cast<bool>(row),"Truncated skill state");std::string extra;require(!(row>>extra),"Trailing skill state");require(sent.empty(),"Missing original skill send");plan=skill_save_plan(character->skills(),500001);next=0;++cases;}
            else if(tag=="QUERY") {require(next<plan.size(),"Original emitted extra skill SQL");const auto actual=encode(plan[next++]);require(actual==line.substr(6),"Skill SQL differs at operation "+std::to_string(cases)+"\nactual: "+actual+"\noriginal: "+line.substr(6));++queries;}
            else if(tag=="END") require(next==plan.size(),"Native emitted extra skill SQL");
            else throw std::runtime_error("Unknown skill fixture row");
        }
        require(episodes==32 && cases==2048 && queries==7392 && resets==96 && acks==128,"Skill oracle coverage changed");character.reset();pool.reset();
        std::uint16_t points=10;SkillReplicaPool validation_pool;
        {SkillReplica failed(12345,points,validation_pool);std::vector<PlayerSkill> too_many(256);require(!failed.load(too_many,{}) && failed.learned().size()==256 && failed.saved_count()==0,"Original 256th load-row failure was ignored");}
        SkillReplica cache(12345,points,validation_pool);std::vector<PlayerSkill> full(255);require(cache.load(full,{}),"Cannot load maximum original learned rows");
        const net::Packet add{1,8,0x39,0x30,0,0,9,0,7,0,1};require(cache.apply(add)==InventoryReplicaResult::applied && cache.learned().size()==256 && cache.learned().back().number==255 && points==9,"Last learned pointer/number changed");require(cache.apply(add)==InventoryReplicaResult::rejected && points==9 && cache.learned().size()==256,"Learned overflow corrupted the cache");
        for(const auto& p:std::vector<net::Packet>{{1,8},{1,8,0x38,0x30,0,0,9,0,7,0,1},{4,8,0x39,0x30,0,0,2,0,1,0},{4,9,0x39,0x30,0,0,1,7,0,1,0,0,0}}) require(cache.apply(p)==InventoryReplicaResult::rejected,"Malformed/cross-owner skill packet was accepted");
        require(cache.apply(net::Packet{6,9,0x39,0x30,0,0})==InventoryReplicaResult::unsupported,"Unknown skill command was swallowed");
        std::cout<<cases<<" original skill cache/reset operations and "<<queries<<" SQL calls passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
