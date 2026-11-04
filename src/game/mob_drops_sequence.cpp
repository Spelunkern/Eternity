#include "eternity/game/mob_drops_sequence.hpp"
#include <algorithm>
#include <bit>
#include <charconv>
#include <fstream>
#include <stdexcept>

namespace eternity::game {
namespace {
std::int32_t group_count(MobDropSequenceSource& source) {return std::bit_cast<std::int32_t>(source.integer("SEQUENCE_GROUP_COUNT","COUNT",0));}
std::string section(std::size_t index) {return "SEQUENCE_GROUP_"+std::to_string(index+1);}
void grades(MobDropSequence& group,MobDropSequenceSource& source,const std::string& name) {
    for(std::size_t i=0;i<group.grades.size();++i) group.grades[i]=source.integer(name,"GRADE_"+std::to_string(i),0);
}
std::string_view trim(std::string_view value) {
    const auto first=value.find_first_not_of(" \t\r");if(first==std::string_view::npos) return {};
    return value.substr(first,value.find_last_not_of(" \t\r")-first+1);
}
std::string lower(std::string_view value) {std::string result(value);for(auto& c:result) if(c>='A' && c<='Z') c=static_cast<char>(c-'A'+'a');return result;}
std::uint32_t number(std::string_view value) {
    value=trim(value);bool negative=false;
    if(!value.empty() && (value.front()=='-' || value.front()=='+')) {negative=value.front()=='-';value.remove_prefix(1);}
    if(value.empty()) throw std::invalid_argument("Empty sequence INI integer");
    std::uint32_t result=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
    if(parsed.ec!=std::errc{} || parsed.ptr!=value.data()+value.size()) throw std::invalid_argument("Unsupported sequence INI integer");
    return negative?0U-result:result;
}
}
void MobDropSequenceConfiguration::initialize(MobDropSequenceSource& source,MobDropSequenceDiagnostics& diagnostics) {
    count=group_count(source);
    if(count<=0) {diagnostics.problem(false,MobDropSequenceProblem::nonpositive_count);return;}
    if(count>8) {diagnostics.problem(false,MobDropSequenceProblem::clamped_count);count=8;}
    for(std::size_t i=0;i<static_cast<std::size_t>(count);++i) {
        const auto name=section(i);auto& group=groups[i];group.divisor=source.integer(name,"SEQUENCE",0);
        if(group.divisor==0) {diagnostics.problem(false,MobDropSequenceProblem::zero_divisor);count=0;return;}
        grades(group,source,name);
    }
}
bool MobDropSequenceConfiguration::reload(MobDropSequenceSource& source,MobDropSequenceDiagnostics& diagnostics) {
    auto requested=group_count(source);
    if(requested<=0) {diagnostics.problem(true,MobDropSequenceProblem::nonpositive_count);return false;}
    if(requested>8) {diagnostics.problem(true,MobDropSequenceProblem::clamped_count);requested=8;}
    count=requested;
    for(std::size_t i=0;i<static_cast<std::size_t>(count);++i) {
        const auto name=section(i);const auto divisor=source.integer(name,"SEQUENCE",0);
        if(divisor==0) {diagnostics.problem(true,MobDropSequenceProblem::zero_divisor);count=0;return false;}
        groups[i].divisor=divisor;grades(groups[i],source,name);
    }
    return true;
}
void MobDropSequenceConfiguration::publish(MobDropCatalog& catalog) const {
    if(count>8) throw std::invalid_argument("Sequence group count exceeds original storage");
    if(count<=0) catalog.sequences.clear();
    else catalog.sequences.assign(groups.begin(),groups.begin()+count);
}
MobDropSequenceFile MobDropSequenceFile::decode(std::string_view text) {
    if(text.find('\0')!=std::string_view::npos) throw std::invalid_argument("Unsupported NUL in sequence INI");
    MobDropSequenceFile result;std::string group;
    while(!text.empty()) {
        const auto end=text.find('\n');const auto line=trim(text.substr(0,end));
        if(end==std::string_view::npos) text={};else text.remove_prefix(end+1);
        if(line.empty() || line.front()==';') continue;
        if(line.front()=='[') {
            if(line.size()<3 || line.back()!=']') throw std::invalid_argument("Unsupported sequence INI section");
            group=lower(trim(line.substr(1,line.size()-2)));continue;
        }
        const auto equal=line.find('=');if(group.empty() || equal==std::string_view::npos) throw std::invalid_argument("Unsupported sequence INI entry");
        const auto key=lower(trim(line.substr(0,equal)));if(key.empty()) throw std::invalid_argument("Empty sequence INI key");
        if(!result.values_.emplace(std::make_pair(group,key),number(line.substr(equal+1))).second) throw std::invalid_argument("Ambiguous duplicate sequence INI key");
    }
    return result;
}
MobDropSequenceFile MobDropSequenceFile::read(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);if(!input) throw std::runtime_error("Cannot read sequence INI");
    return decode(std::string((std::istreambuf_iterator<char>(input)),std::istreambuf_iterator<char>()));
}
std::uint32_t MobDropSequenceFile::integer(std::string_view group,std::string_view key,std::uint32_t fallback) {
    const auto at=values_.find({lower(group),lower(key)});return at==values_.end()?fallback:at->second;
}
}
