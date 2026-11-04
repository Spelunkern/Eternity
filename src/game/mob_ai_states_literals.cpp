#include "eternity/game/mob_ai_states_compare.hpp"
#include "eternity/game/mob_ai_states_bindings.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <stdexcept>
namespace eternity::game {
namespace {
std::int32_t increment(std::int32_t value){return std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)+1U);}
void assign(MobAiParameter& target,const MobAiParameter& source){
    target.type=source.type;target.integer=source.integer;target.real=source.real;target.dword=source.dword;target.variable_id=source.variable_id;
    if(source.type==4){const auto end=std::find(source.text.begin(),source.text.end(),'\0');if(end==source.text.end())throw std::logic_error("Mob AI string literal has no retained terminator");std::copy(source.text.begin(),end+1,target.text.begin());}
}
void push(MobAiValueStack& stack,const MobAiParameter& value){
    stack.top=increment(stack.top);
    if(stack.top<0)throw std::logic_error("Mob AI literal stack index wrapped outside retained storage");
    if(stack.top<stack.capacity)assign(stack.slots[static_cast<std::size_t>(stack.top)],value);
}
MobAiParameter pop(MobAiValueStack& stack){
    if(stack.top<0||stack.top>=stack.capacity||static_cast<std::size_t>(stack.top)>=stack.slots.size())throw std::logic_error("Mob AI comparison pop has undefined original operand storage");
    auto value=stack.slots[static_cast<std::size_t>(stack.top)];--stack.top;return value;
}
template<class T> bool relation(T first,T second,std::int32_t opcode){
    switch(opcode){case 0:return first<second;case 1:return second<first;case 2:return first<=second;case 3:return second<=first;case 4:return first==second;case 5:return first!=second;default:return false;}
}
bool integer_comparison(const MobAiParameter& first,const MobAiParameter& second,std::int32_t opcode){
    if((first.type!=1&&first.type!=3)||(second.type!=1&&second.type!=3))throw std::logic_error("Mob AI comparison requires the unrecovered noninteger type matrix");
    // Original right-DWORD category (2) contributes no result for these types.
    if(first.type==3)return false;
    if(second.type==3)return relation(static_cast<std::uint32_t>(first.integer),second.dword,opcode);
    return relation(first.integer,second.integer,opcode);
}
}
static std::int32_t interpret_values(MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition,bool local_reads,bool comparisons){
    auto& stack=owner.value_stack;
    if(stack.capacity<0||static_cast<std::size_t>(stack.capacity)>stack.slots.size())throw std::logic_error("Mob AI literal stack requires its retained capacity/storage");
    stack.top=-1;condition.words[2]=0;std::uint32_t passed=0;
    MobAiParameter numeric(0);bool numeric_type_known=false; // Original scratch type is undefined until a typed literal.
    for(;;){
        const auto index=std::bit_cast<std::int32_t>(condition.words[2]);
        const auto next=increment(index);condition.words[2]=static_cast<std::uint32_t>(next);
        if(next>std::bit_cast<std::int32_t>(condition.words[1]))return std::bit_cast<std::int32_t>(passed);
        if(condition.tokens.data()==nullptr&&index==0)return std::bit_cast<std::int32_t>(passed);
        if(index<0||static_cast<std::size_t>(index)>=condition.tokens.size())throw std::logic_error("Mob AI declared token count exceeds retained token storage");
        const auto& token=condition.tokens[static_cast<std::size_t>(index)];
        if(token.type==2){
            if(token.number_type==1){numeric_type_known=true;numeric.type=1;numeric.integer=token.integer;}
            else if(token.number_type==2){numeric_type_known=true;numeric.type=2;numeric.real=token.real;}
            else if(token.number_type==3){numeric_type_known=true;numeric.type=3;numeric.dword=token.dword;}
            if(!numeric_type_known)throw std::logic_error("Mob AI untyped first literal has undefined original scratch type");
            push(stack,numeric);
        }else if(token.type==7){
            if(std::find(token.value.begin(),token.value.end(),'\0')==token.value.end())throw std::logic_error("Mob AI string literal has no retained terminator");
            MobAiParameter string(4);string.text=token.value;push(stack,string);
        }else if(token.type==3&&local_reads){
            const auto end=std::find(token.text.begin(),token.text.end(),'\0');
            if(end==token.text.end())throw std::logic_error("Mob AI local name has no retained terminator");
            const auto key=mob_ai_name_code(std::string_view(token.text.data(),static_cast<std::size_t>(end-token.text.begin())));
            const auto value=owner.variables.find(key);
            if(value==owner.variables.end())throw std::logic_error("Mob AI local read requires a factory-populated typed variable");
            if(value->second.type==4&&std::find(value->second.text.begin(),value->second.text.end(),'\0')==value->second.text.end())throw std::logic_error("Mob AI local string has no retained terminator");
            push(stack,value->second);
        }else if(token.type==9&&comparisons){
            const auto first=pop(stack);const auto second=pop(stack);
            if(integer_comparison(first,second,token.opcode))++passed;
        }else if(token.type==3||token.type==4||token.type==8||token.type==9){
            throw std::logic_error("Mob AI token requires the unrecovered full TravelCondition provider");
        }
        // Kinds outside the dispatch range and original cases 5/6 are ignored.
    }
}
std::int32_t interpret_mob_ai_literals(MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition){return interpret_values(owner,condition,false,false);}
std::int32_t interpret_mob_ai_local_values(MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition){return interpret_values(owner,condition,true,false);}

std::int32_t interpret_mob_ai_integer_conditions(MobAiConfiguredFsm& owner,MobAiConfiguredCondition& condition){return interpret_values(owner,condition,true,true);}

}
