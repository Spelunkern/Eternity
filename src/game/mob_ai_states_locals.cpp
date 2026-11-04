#include "eternity/game/mob_ai_states_locals.hpp"
#include <algorithm>
#include <stdexcept>
namespace eternity::game {
bool add_mob_ai_local(MobAiVariables& values,std::uint32_t key,const MobAiParameter& source){
    // Original temporary copies leave inactive text and string tails undefined.
    // New native storage normalizes only those bytes, never an existing value.
    MobAiParameter value(source.type);
    value.integer=source.integer;
    value.real=source.real;
    value.dword=source.dword;
    value.variable_id=source.variable_id;
    if(source.type==4){
        const auto end=std::find(source.text.begin(),source.text.end(),'\0');
        if(end==source.text.end())throw std::logic_error("Mob AI local string lacks a retained terminator");
        std::copy(source.text.begin(),end+1,value.text.begin());
    }
    values.emplace(key,value);
    return true;
}
bool install_mob_ai_locals(MobAiVariables& values,const MobAiVariableCatalog& catalog,std::int32_t ai){
    if(ai<0)throw std::logic_error("Negative mob AI factory index is outside the recovered contract");
    const auto index=static_cast<std::size_t>(ai);
    if(index>=catalog.size()||!catalog[index])return false;
    const auto& definition=*catalog[index];
    for(std::int32_t i=0;i<definition.count;++i){
        if(static_cast<std::size_t>(i)>=definition.locals.size())throw std::logic_error("Mob AI factory local count exceeds retained definitions");
        const auto& value=definition.locals[static_cast<std::size_t>(i)];
        add_mob_ai_local(values,value.variable_id,value);
    }
    return true;
}
}
