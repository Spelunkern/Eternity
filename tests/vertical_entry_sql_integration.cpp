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
    game::Players players(db,1,argv[1]);game::CharacterSelectionState state{7001,0,true};
    std::optional<game::PlayerSnapshot> loaded;unsigned unavailable=0,closed=0;
    game::VerticalEntryPorts ports{[&](game::PlayerSnapshot p){loaded=std::move(p);},[&]{++unavailable;},[&](std::uint32_t r){check(r==9,"Wrong close reason");++closed;}};
    auto packet=select(own);packet.push_back(0xaa);
    check(game::vertical_entry_select(state,packet,players,"load_user",ports)==game::CharacterSelectionResult::requested && !state.enabled && loaded && loaded->core.user_id==7001 && loaded->core.character.id==own && !unavailable && !closed,"Owned 0104 did not reach real SQL loader");
    loaded.reset();
    check(game::vertical_entry_select(state,select(foreign),players,"load_user",ports)==game::CharacterSelectionResult::ignored && !loaded && !unavailable && !closed,"Consumed latch performed a second load");
    state.enabled=true;
    check(game::vertical_entry_select(state,select(foreign),players,"load_user",ports)==game::CharacterSelectionResult::requested && !state.enabled && !loaded && unavailable==1 && !closed,"Foreign selection did not reach unavailable boundary");
    state.enabled=true;state.country=2;
    check(game::vertical_entry_select(state,select(own),players,"load_user",ports)==game::CharacterSelectionResult::closed && state.enabled && closed==1 && unavailable==1 && !loaded,"Country gate changed latch or reached SQL");
    state.country=0;
    check(game::vertical_entry_select(state,net::Packet{4,1,0},players,"load_user",ports)==game::CharacterSelectionResult::closed && state.enabled && closed==2 && unavailable==1 && !loaded,"Truncated request reached SQL or consumed latch");
    bool invalid=false;
    try {game::vertical_entry_select(state,select(0),players,"load_user",ports);} catch(const std::invalid_argument&) {invalid=true;}
    check(invalid && !state.enabled && unavailable==1 && closed==2 && !loaded,"Loader exception was hidden or latch restored");
    std::cout<<"Real SQL selection, ownership failure and 0104 latch passed\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
