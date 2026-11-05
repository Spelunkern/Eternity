#include "eternity/game/vertical_entry.hpp"
#include <iostream>
#include <optional>
#include <stdexcept>
using namespace eternity;
static void check(bool x,const char* message) {if(!x) throw std::runtime_error(message);}
static net::Packet select(std::uint32_t id) {net::Packet p{4,1};for(unsigned i=0;i<4;++i)p.push_back(static_cast<std::uint8_t>(id>>(8*i)));return p;}
int main(int argc,char** argv) {try {
    check(argc==2 && std::string_view(argv[1]).starts_with("Eternity_Test_Creation_"),"Requires existing disposable creation fixture");
    storage::SqlServer db(storage::SqlServerConfig::from_environment());
    const std::string prefix="["+std::string(argv[1])+"].dbo.";
    auto rows=db.query("SELECT UserUID,CharID FROM "+prefix+"Chars WHERE UserUID IN (7001,7002) AND Del=0 ORDER BY UserUID;");
    check(rows.size()==1 && rows[0].rows.size()==2,"Missing original player fixtures");
    const auto own=static_cast<std::uint32_t>(std::stoul(*rows[0].rows[0][1]));
    const auto foreign=static_cast<std::uint32_t>(std::stoul(*rows[0].rows[1][1]));
    game::Players players(db,1,argv[1]);game::Characters characters(db,1,std::chrono::sys_days{std::chrono::year{2024}/1/1},argv[1]);db.query("UPDATE "+prefix+"Chars SET RenameCnt=255,RemainTime=-1 WHERE CharID="+std::to_string(own)+";");
    auto raw=characters.load(7001);bool found=false;
    for(const auto& slot:raw.slots) if(slot && slot->id==own) {found=true;check(slot->rename==255 && slot->remaining_time==-1,"Roster loader lost raw original status values");check(!game::vertical_entry_roster_allows(raw,own),"Negative remaining time or rename 255 accepted");}
    check(found,"Fixture character missing from roster");
    db.query("UPDATE "+prefix+"Chars SET RenameCnt=0,RemainTime=0 WHERE CharID="+std::to_string(own)+";");
    auto roster=characters.load(7001);game::CharacterSelectionState state{7001,0,true};
    std::optional<game::PlayerSnapshot> loaded;unsigned unavailable=0,closed=0;
    game::VerticalEntryPorts ports{[&](game::PlayerSnapshot p){loaded=std::move(p);},[&]{++unavailable;},[&](std::uint32_t r){check(r==9,"Wrong close reason");++closed;}};
    auto packet=select(own);packet.push_back(0xaa);
    check(game::vertical_entry_select(state,packet,roster,players,"load_user",ports)==game::CharacterSelectionResult::requested && !state.enabled && loaded && loaded->core.user_id==7001 && loaded->core.character.id==own && !unavailable && !closed,"Owned 0104 did not reach real SQL loader");
    loaded.reset();
    check(game::vertical_entry_select(state,select(foreign),roster,players,"load_user",ports)==game::CharacterSelectionResult::ignored && !loaded && !unavailable && !closed,"Consumed latch performed a second load");
    state.enabled=true;
    check(game::vertical_entry_select(state,select(foreign),roster,players,"load_user",ports)==game::CharacterSelectionResult::requested && !state.enabled && !loaded && unavailable==1 && !closed,"Foreign selection did not reach unavailable boundary");
    // SQL revalidation is intentionally later than the retained roster gate.
    // Exercise stale retained state against the disposable catalog, then restore it.
    for (const auto* change : {"Del=1", "Slot=5", "RenameCnt=1", "RemainTime=1", "RemainTime=-1", "UserUID=7002", "ServerID=2"}) {
        db.query("UPDATE "+prefix+"Chars SET "+change+" WHERE CharID="+std::to_string(own)+";");
        state.enabled=true;const auto before=unavailable;
        check(game::vertical_entry_select(state,select(own),roster,players,"load_user",ports)==game::CharacterSelectionResult::requested && !state.enabled && unavailable==before+1 && !loaded,"SQL revalidation accepted a stale retained roster");
        db.query("UPDATE "+prefix+"Chars SET Del=0,Slot=0,RenameCnt=0,RemainTime=0,UserUID=7001,ServerID=1 WHERE CharID="+std::to_string(own)+";");
    }
    for (auto& slot:roster.slots) if(slot && slot->id==own) {
        state.enabled=true;slot->rename=true;
        check(game::vertical_entry_select(state,select(own),roster,players,"load_user",ports)==game::CharacterSelectionResult::requested && !state.enabled && unavailable==9 && !loaded,"Retained rename gate did not reject");
        slot->rename=false;slot->remaining_time=true;state.enabled=true;
        check(game::vertical_entry_select(state,select(own),roster,players,"load_user",ports)==game::CharacterSelectionResult::requested && !state.enabled && unavailable==10 && !loaded,"Retained pending-deletion gate did not reject");
        slot->remaining_time=false;
    }
    state.enabled=true;state.country=2;
    check(game::vertical_entry_select(state,select(own),roster,players,"load_user",ports)==game::CharacterSelectionResult::closed && state.enabled && closed==1 && unavailable==10 && !loaded,"Country gate changed latch or reached SQL");
    state.country=0;
    check(game::vertical_entry_select(state,net::Packet{4,1,0},roster,players,"load_user",ports)==game::CharacterSelectionResult::closed && state.enabled && closed==2 && unavailable==10 && !loaded,"Truncated request reached SQL or consumed latch");
    bool invalid=false;
    try {game::vertical_entry_select(state,select(0),roster,players,"load_user",ports);} catch(const std::logic_error&) {invalid=true;}
    check(invalid && !state.enabled && unavailable==10 && closed==2 && !loaded,"Unrepresentable empty slot was hidden or latch restored");
    std::cout<<"Real SQL selection, ownership failure and 0104 latch passed\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
