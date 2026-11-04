#include "eternity/game/mob_ai_states_parameters.hpp"
#include <stdexcept>
namespace eternity::game {
void initialize_mob_ai_parameter(MobAiParameter& value) noexcept{value.integer=0;value.real=0.0F;value.dword=0;value.variable_id=0;}
void clear_mob_ai_variables(MobAiVariables& variables,const MobAiVariableCatalog& catalog,std::int32_t ai){
    if(ai<0)throw std::logic_error("Negative mob AI catalog index is outside the recovered contract");
    const auto index=static_cast<std::size_t>(ai);if(index>=catalog.size()||!catalog[index])return;
    const auto& definition=*catalog[index];
    for(std::int32_t i=0;i<definition.count;++i){
        if(static_cast<std::size_t>(i)>=definition.locals.size())throw std::logic_error("Mob AI local count exceeds retained definitions");
        const auto at=variables.find(definition.locals[static_cast<std::size_t>(i)].variable_id);
        if(at==variables.end())throw std::logic_error("Mob AI reset requires factory-populated typed local variables");
        auto& value=at->second;
        switch(value.type){case 1:value.integer=0;break;case 2:value.real=0.0F;break;case 3:value.dword=0;break;case 4:value.text[0]='\0';break;default:break;}
    }
}
}
