#include "eternity/game/character_recovery.hpp"
#include "eternity/game/inventory_save.hpp"
#include "eternity/game/interface_save.hpp"
#include "eternity/game/quest_save.hpp"
#include "eternity/game/skill_save.hpp"
#include "eternity/game/legacy_sql.hpp"
#include <bit>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace eternity::game {
std::vector<RecoveryOperation> capture_character_recovery(CharacterReplica& replica,std::uint8_t server,const std::function<void(const RecoveryOperation&)>& output) {
    const auto core=replica.core();const auto& c=core.character;
    if(!c.id || c.id>INT32_MAX || !core.user_id || core.user_id>INT32_MAX) throw std::invalid_argument("Invalid character recovery owner");
    std::vector<RecoveryOperation> result;
    const auto add=[&](RecoveryOperation operation){result.push_back(std::move(operation));if(output) output(result.back());};
    if(replica.growth().advance(c.grow,c.level)) add({"usp_Save_Char_Maxgrow_E",{std::int64_t{server},std::int64_t{core.user_id},std::int64_t{replica.growth().maximum()}}});
    // This older file format zero-extends WORD stats/resources, prints unsigned
    // kills, and omits the two kill-level arguments present in live SQL saving.
    add({"usp_Save_Char_Info_E",{std::int64_t{c.id},std::int64_t{c.level},std::int64_t{core.stat_points},std::int64_t{core.skill_points},std::int64_t{c.strength},std::int64_t{c.dexterity},std::int64_t{c.recovery},std::int64_t{c.intelligence},std::int64_t{c.wisdom},std::int64_t{c.luck},std::int64_t{c.hp},std::int64_t{c.mp},std::int64_t{c.sp},std::int64_t{c.map},std::int64_t{core.direction},std::int64_t{std::bit_cast<std::int32_t>(core.experience)},std::int64_t{std::bit_cast<std::int32_t>(core.money)},legacy_sql_coordinate(core.x),legacy_sql_coordinate(core.y),legacy_sql_coordinate(core.z),std::int64_t{core.hunger},std::int64_t{core.vigor},std::int64_t{core.cg},std::int64_t{core.og},std::int64_t{core.ig},std::int64_t{core.kills[0]},std::int64_t{core.kills[1]},std::int64_t{core.kills[2]},std::int64_t{core.kills[3]}}});
    add({"usp_Save_User_StoredMoney_E",{std::int64_t{server},std::int64_t{core.user_id},std::int64_t{static_cast<std::uint32_t>(core.stored_money)}},false});
    if(replica.body_changed()) add({"usp_Save_Char_BodyChg_E",{std::int64_t{c.id},std::int64_t{c.hair},std::int64_t{c.face},std::int64_t{c.size},std::int64_t{c.sex}}});
    replica.core_save_success();
    for(auto op:point_save_plan(replica.points(),server,c.id)) {
        // Original product recovery calls omit six gems and the craft argument.
        // Stored-point recovery uses the full procedure envelope.
        if(op.procedure=="usp_Save_Char_Product_Item_E") op.parameters.erase(op.parameters.begin()+11,op.parameters.begin()+18);
        add({std::move(op.procedure),std::move(op.parameters)});
    }
    replica.points().recovery_save_success();
    for(const auto& op:inventory_save_plan(replica.inventory(),server,c.id)) {
        if(op.procedure=="usp_Save_Char_Item_Add_E") add({op.retry_delete_procedure,op.retry_delete_parameters});
        add({op.procedure,op.parameters});
    }
    replica.inventory().inventory_save_success();
    for(const auto& op:skill_save_plan(replica.skills(),c.id)) {
        add({op.procedure,op.parameters});if(op.procedure=="usp_Save_Char_ApplySkill_Del_E") break;
    }
    // File recovery emits expired effects too; the live SQL phase skips them.
    for(const auto& effect:replica.skills().effects()) add({"usp_Save_Char_ApplySkill_Add_E2",{std::int64_t{c.id},std::int64_t{effect.id},std::int64_t{effect.level},std::int64_t{std::bit_cast<std::int32_t>(effect.delay)}}});
    replica.skills().save_success();
    for(const auto& op:quest_save_plan(replica.quests(),c.id)) add({op.procedure,op.parameters});
    replica.quests().recovery_save_success();
    for(const auto& op:interface_save_plan(replica.interface(),c.id,true)) add({op.procedure,op.parameters,false});
    // Save-point flags, stored-point receipts, market state and queued purchases
    // are not acknowledged by this function.
    return result;
}
std::string recovery_sql(const RecoveryOperation& operation) {
    std::string result=operation.odbc?"{?=call ":"EXEC ";result+=operation.procedure;result+=operation.odbc?"(":" ";
    for(std::size_t i=0;i<operation.parameters.size();++i) {
        if(i) result+=',';
        const auto& value=operation.parameters[i];
        if(const auto number=std::get_if<std::int64_t>(&value)) result+=std::to_string(*number);
        else if(const auto text=std::get_if<std::string>(&value)) {
            const bool coordinate=(operation.procedure=="usp_Save_Char_Info_E" && i>=17 && i<=19) || ((operation.procedure=="usp_Create_Char_SavePoint" || operation.procedure=="usp_Save_Char_SavePoint") && i>=3);
            result+=coordinate?*text:"'"+*text+"'"; // Original SQL literals are unescaped.
        }else throw std::invalid_argument("Unsupported original recovery SQL parameter");
    }
    if(operation.odbc) result+=")}";
    return result;
}
CharacterRecoveryFile::CharacterRecoveryFile(std::filesystem::path path,std::function<std::string()> clock):path_(std::move(path)),clock_(std::move(clock)) {
    if(!clock_) clock_=[](){const auto now=std::time(nullptr);std::tm local{};localtime_r(&now,&local);std::ostringstream value;value<<std::put_time(&local,"%Y-%m-%d %H:%M:%S");return value.str();};
}
RecoveryWriteResult CharacterRecoveryFile::append(CharacterReplica& replica,std::uint8_t server,std::string_view username) {
    std::lock_guard lock(mutex_);const auto began=clock_();const auto core=replica.core();
    std::ofstream file(path_,std::ios::binary|std::ios::app);
    file<<"--<begin>"<<began<<','<<username<<','<<core.user_id<<','<<core.character.name<<','<<core.character.id<<"\r\n";file.flush();
    const auto operations=capture_character_recovery(replica,server,[&](const RecoveryOperation& operation){file<<recovery_sql(operation)<<"\r\n";file.flush();});
    const auto ended=clock_();file<<"--<end>"<<ended<<','<<username<<','<<core.user_id<<','<<core.character.name<<','<<core.character.id<<"\r\n\r\n\r\n";file.flush();
    return {true,file.good(),operations.size()};
}
}
