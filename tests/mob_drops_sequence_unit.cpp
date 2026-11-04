#include "eternity/game/mob_drops_sequence.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool value,const std::string& why) {if(!value) throw std::runtime_error(why);}
std::string unhex(const std::string& text) {
    check(text.size()%2==0,"Invalid oracle hex");std::string result;
    for(std::size_t i=0;i<text.size();i+=2) result+=static_cast<char>(std::stoul(text.substr(i,2),nullptr,16));return result;
}
std::vector<std::uint32_t> words(const std::string& text) {
    const auto bytes=unhex(text);check(bytes.size()%4==0,"Invalid oracle words");std::vector<std::uint32_t> result;
    for(std::size_t i=0;i<bytes.size();i+=4) {std::uint32_t value=0;for(unsigned j=0;j<4;++j) value|=static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[i+j]))<<(j*8);result.push_back(value);}return result;
}
void restore(MobDropSequenceConfiguration& config,const std::vector<std::uint32_t>& values) {
    check(values.size()==89,"Invalid sequence state size");config.count=std::bit_cast<std::int32_t>(values[0]);
    for(std::size_t i=0;i<8;++i) {config.groups[i].divisor=values[1+i*11];for(std::size_t j=0;j<10;++j) config.groups[i].grades[j]=values[2+i*11+j];}
}
std::vector<std::uint32_t> snapshot(const MobDropSequenceConfiguration& config) {
    std::vector<std::uint32_t> result{std::bit_cast<std::uint32_t>(config.count)};
    for(const auto& group:config.groups) {result.push_back(group.divisor);result.insert(result.end(),group.grades.begin(),group.grades.end());}return result;
}
struct World:MobDropSequenceSource,MobDropSequenceDiagnostics {
    std::vector<std::uint32_t> values;std::string events;
    void event(std::string text) {if(!events.empty()) events+=',';events+=text;}
    std::uint32_t integer(std::string_view section,std::string_view key,std::uint32_t fallback) override {
        check(fallback==0,"Wrong profile fallback");std::uint32_t result;
        if(section=="SEQUENCE_GROUP_COUNT" && key=="COUNT") result=values[0];
        else {
            check(section.starts_with("SEQUENCE_GROUP_"),"Wrong profile section");const auto i=std::stoul(std::string(section.substr(15)))-1;
            const auto j=key=="SEQUENCE"?0:std::stoul(std::string(key.substr(6)))+1;check(i<8 && j<11,"Wrong profile key");result=values[1+i*11+j];
        }
        event("P"+std::string(section)+":"+std::string(key)+"="+std::to_string(result));return result;
    }
    void problem(bool reload,MobDropSequenceProblem value) override {event(std::string(reload?"R":"I")+std::to_string(static_cast<int>(value)));}
};
struct Clock:MobDropCatalogWorld {
    unsigned calls=0;std::uint32_t tick() override {++calls;return 0;}void lock() override {++calls;}void unlock() noexcept override {++calls;}
};
}
int main(int argc,char** argv) {try {
    check(argc==2 || argc==3,"Expected sequence fixture and optional original INI");std::ifstream input(argv[1]);check(input.good(),"Cannot open sequence fixture");unsigned cases=0,wide=0,assets=0;std::vector<std::uint32_t> asset_values,asset_state;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char tag;row>>tag;
        if(tag=='Q') {
            std::uint32_t grade,expected;row>>grade>>expected;check(!row.fail(),"Malformed wide grade case");MobDropCatalog catalog;MobDropCatalogEntry entry;catalog.grades[1].push_back(&entry);MobDropSequence sequence;sequence.divisor=2;sequence.grades[1]=grade;catalog.sequences.push_back(sequence);MicrosoftRandom random(1);Clock clock;
            check(expected==0 && catalog.sequence(10,2,random,clock)==nullptr && random.state()==1 && clock.calls==0,"Original wide sequence grade was narrowed");++wide;continue;
        }
        unsigned scenario,reload,success;std::string initial,values,expected,events;row>>scenario>>reload>>initial>>values>>expected>>success>>events;check(!row.fail(),"Malformed sequence scenario");
        MobDropSequenceConfiguration config;restore(config,words(initial));World world;world.values=words(values);
        if(reload) check(config.reload(world,world)==(success!=0),"Original sequence reload result differs");else config.initialize(world,world);
        check(snapshot(config)==words(expected),"Original sequence partial state differs: "+std::to_string(scenario));
        check(world.events==unhex(events),"Original sequence profile/log order differs: "+std::to_string(scenario));
        MobDropCatalog catalog;config.publish(catalog);check(catalog.sequences.size()==static_cast<std::size_t>(std::max(config.count,0)),"Sequence publication count differs");
        if(tag=='A') {asset_values=world.values;asset_state=words(expected);++assets;}else {check(tag=='S',"Unknown sequence fixture tag");++cases;}
    }
    const auto text="; comment\r\n[sequence_group_count]\r\nCOUNT=1\r\n[sequence_group_1]\r\nSEQUENCE=+17\r\nGRADE_0=65537\r\nGRADE_1=-1\r\n";
    auto file=MobDropSequenceFile::decode(text);check(file.integer("SEQUENCE_GROUP_COUNT","COUNT",0)==1 && file.integer("SEQUENCE_GROUP_1","GRADE_0",0)==65537 && file.integer("sequence_group_1","grade_1",0)==UINT32_MAX && file.integer("missing","absent",9)==9,"Sequence decimal profile differs");
    bool rejected=false;try {(void)MobDropSequenceFile::decode("[a]\nx=1\nx=2\n");} catch(const std::invalid_argument&) {rejected=true;}check(rejected,"Ambiguous sequence INI was guessed");
    if(argc==3) {
        auto original=MobDropSequenceFile::read(argv[2]);check(original.integer("SEQUENCE_GROUP_COUNT","COUNT",0)==asset_values[0],"Original INI group count differs");
        for(std::size_t i=0;i<8;++i) {const auto name="SEQUENCE_GROUP_"+std::to_string(i+1);check(original.integer(name,"SEQUENCE",0)==asset_values[1+i*11],"Original INI divisor differs");for(std::size_t j=0;j<10;++j) check(original.integer(name,"GRADE_"+std::to_string(j),0)==asset_values[2+i*11+j],"Original INI grade differs");}
        MobDropSequenceConfiguration config;World log;config.initialize(original,log);check(snapshot(config)==asset_state,"Original INI initialization differs");std::cout<<"Original seven-group sequence INI matched complete original execution\n";
    }
    check(cases==256 && assets==1 && wide==6,"Incomplete sequence fixture coverage");std::cout<<cases<<" original init/reload traces and "<<wide<<" original wide grade lookups matched\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
