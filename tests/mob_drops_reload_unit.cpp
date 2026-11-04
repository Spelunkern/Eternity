#include "eternity/game/mob_drops_reload.hpp"
#include <bit>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity::game;
namespace {
void check(bool value,const std::string& why) {if(!value) throw std::runtime_error(why);}
std::string unhex(const std::string& text) {std::string value;check(text.size()%2==0,"Invalid reload hex");for(std::size_t i=0;i<text.size();i+=2) value+=static_cast<char>(std::stoul(text.substr(i,2),nullptr,16));return value;}
void restore(MobDropDefinitions& definitions,const std::string& hex) {
    const auto bytes=unhex(hex);check(bytes.size()==240,"Invalid reload table length");definitions.mobs.resize(4);std::size_t at=0;
    const auto number=[&](unsigned count) {std::uint32_t value=0;for(unsigned i=0;i<count;++i) value|=static_cast<std::uint32_t>(static_cast<unsigned char>(bytes[at++]))<<(i*8);return value;};
    for(auto& mob:definitions.mobs) for(auto& slot:mob) {slot.grade=static_cast<std::uint16_t>(number(2));slot.rate=number(4);}
}
std::string snapshot(const MobDropDefinitions& definitions) {
    std::string bytes;const auto add=[&](std::uint32_t value,unsigned count) {for(unsigned i=0;i<count;++i) bytes+=static_cast<char>(value>>(i*8));};
    for(const auto& mob:definitions.mobs) for(const auto& slot:mob) {add(slot.grade,2);add(slot.rate,4);}return bytes;
}
struct Diagnostics:MobDropReloadDiagnostics {
    std::string events;void rejected(const MobDropRow&) override {events+='X';}void failed(MobDropReloadFailure) override {events+='E';}
};
struct Cursor:MobDropReloadCursor {
    std::vector<MobDropRow> rows;int fail=-1;bool query_fail=false;std::size_t index=0;
    MobDropReloadRead next(MobDropRow& row) override {if(query_fail) return MobDropReloadRead::failed;if(index==rows.size()) return MobDropReloadRead::end;if(static_cast<int>(index)==fail) return MobDropReloadRead::failed;row=rows[index++];return MobDropReloadRead::row;}
};
struct Zero:RandomSource {unsigned calls=0;std::int32_t next() override {++calls;return 0;}};
struct World:MobLootWorld {
    MobDropDefinitions& definitions;Diagnostics diagnostics;std::int32_t changes=0;bool reloaded=false;std::string events;std::array<MobLootItem,10> items{};
    explicit World(MobDropDefinitions& value):definitions(value) {for(unsigned i=0;i<10;++i) items[i].id=1000+i;}
    [[noreturn]] static void unexpected() {throw std::runtime_error("Unexpected live reload loot service");}
    void event(const std::string& value) {if(!events.empty()) events+=',';events+=value;}
    const MobLootItem* item(std::uint8_t,std::uint8_t) override {unexpected();}
    std::span<const std::uint16_t> end_quests(const MobLootItem&) override {unexpected();}
    bool has_quest(MobLootUser&,std::uint16_t) override {unexpected();}
    const MobLootItem* grade_item(std::uint16_t grade) override {check(grade<items.size(),"Unexpected grade after live reload");event("S"+std::to_string(grade));return &items[grade];}
    const MobLootItem* sequence_item(std::uint16_t,std::uint32_t) override {unexpected();}
    void deliver(const MobLootItem& item,MobLootUser*,MobLootParty*,bool sequence) override {
        check(!sequence,"Unexpected live sequence");event("D"+std::to_string(item.id));if(reloaded) return;reloaded=true;Cursor source;source.rows={{1,2,9,100},{1,3,3,0}};check(reload_mob_drop_rows(definitions,changes,source,diagnostics),"Live reload failed");
    }
    void guild_ranking(MobLootUser&,std::uint16_t) override {unexpected();}void log_money(std::int32_t) override {unexpected();}void ground_money(std::uint32_t,std::int32_t) override {unexpected();}void give_money(MobLootUser&,std::int32_t) override {unexpected();}bool zone_item(MobLootUser&,MobLootUser*) override {unexpected();}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original reload fixture");std::ifstream input(argv[1]);check(input.good(),"Cannot read reload fixture");unsigned cases=0,failures=0,live=0;std::array<unsigned,4> columns{};
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream row(line);char tag;row>>tag;
        if(tag=='L') {
            std::string expected;unsigned calls;row>>expected>>calls;check(!row.fail(),"Malformed live reload fixture");MobDropDefinitions definitions;definitions.mobs.resize(4);for(unsigned i=1;i<4;++i) definitions.mobs[1][i]={static_cast<std::uint16_t>(i),100};
            const auto* binding=&definitions.mobs[1];MobLootContext context;context.slots=*binding;context.live_slots=binding;Zero random;ServerRandom server;World world(definitions);mob_loot(context,nullptr,nullptr,random,server,world);
            check(world.events==expected && random.calls==calls && world.changes==2 && world.diagnostics.events.empty() && context.live_slots==binding && context.slots[2].grade==2,"Original live slot reads differ");++live;continue;
        }
        unsigned scenario,request,column,query_fail,count,success;int fail;std::uint32_t initial_counter,final_counter;std::string initial,expected,events;
        row>>scenario>>request>>initial_counter>>fail>>column>>query_fail>>count>>success>>final_counter>>initial>>expected>>events;check(tag=='S' && !row.fail(),"Malformed reload scenario");
        MobDropDefinitions definitions;restore(definitions,initial);definitions.rejected_rows={99};const auto* base=definitions.mobs.data();Cursor source;source.fail=fail;source.query_fail=query_fail;
        for(unsigned i=0;i<count;++i) {check(static_cast<bool>(std::getline(input,line)),"Truncated reload rows");std::istringstream values(line);MobDropRow value;unsigned order;values>>tag>>value.mob>>order>>value.grade>>value.rate;value.order=static_cast<std::uint8_t>(order);check(tag=='R' && !values.fail(),"Malformed reload row");source.rows.push_back(value);}
        auto changes=std::bit_cast<std::int32_t>(initial_counter);Diagnostics diagnostics;const bool result=reload_mob_drop_rows(definitions,changes,source,diagnostics);
        check(result==(success!=0) && std::bit_cast<std::uint32_t>(changes)==final_counter && snapshot(definitions)==unhex(expected),"Original selective reload differs: "+std::to_string(scenario));
        check((diagnostics.events.empty()?"-":diagnostics.events)==events && definitions.mobs.data()==base && definitions.rejected_rows==std::vector<std::size_t>{99},"Reload diagnostics or retained storage changed");
        if(!result) {++failures;if(!query_fail) ++columns[column-1];}++cases;
    }
    // Lazy SQL conversion must retain an earlier committed row on a later NULL.
    eternity::storage::ResultSet table;table.columns={"MobID","ItemOrder","Grade","DropRate"};table.rows={{{"1"},{"1"},{"-1"},{"-1"}},{{"1"},{"2"},std::nullopt,{"10"}}};MobDropDefinitions definitions;definitions.mobs.resize(4);std::int32_t changes=INT32_MAX;Diagnostics diagnostics;
    check(!reload_mob_drop_result(definitions,table,changes,diagnostics) && changes==INT32_MIN+1 && definitions.mobs[1][1].grade==65535 && definitions.mobs[1][1].rate==UINT32_MAX && definitions.mobs[1][2].grade==0 && diagnostics.events=="E","Buffered SQL prefix/NULL failure semantics differ");
    check(cases==512 && failures>0 && live==1 && std::all_of(columns.begin(),columns.end(),[](auto value){return value>0;}),"Incomplete reload evidence");std::cout<<cases<<" complete original selective reloads; "<<failures<<" partial/query failures; stable storage and live loot read matched\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
