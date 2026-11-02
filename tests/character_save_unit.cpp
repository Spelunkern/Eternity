#include "eternity/game/character_save.hpp"
#include <fstream>
#include <iostream>
#include <sstream>

using namespace eternity::game;
namespace {void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}}
int main(int argc,char** argv) {
    try {
        require(argc==2,"Expected original character save owner oracle");std::ifstream input(argv[1]);require(input.good(),"Cannot open save owner oracle");std::vector<std::string> events;std::string line;std::size_t cases=0,db_cases=0,user_cases=0,exit_cases=0,tick_cases=0,next=0;bool returned=false,sticky=false;unsigned int attempts=0;
        while(std::getline(input,line)) {
            if(line.empty() || line[0]=='#') continue;std::istringstream row(line);std::string tag;row>>tag;
            if(tag=="CASE") {
                std::string kind;bool periodic=false,quick=false,active_or_recovery=false,old_timeout=false,present=false,pending=false,purchase_ok=false;row>>kind>>periodic>>quick>>active_or_recovery>>sticky>>old_timeout>>present>>pending>>purchase_ok;
                struct Config {int grow=0;bool allocate=false,begin=false;unsigned int phase=0;bool commit=false,rollback=false;};std::array<Config,2> configs;for(auto& c:configs) row>>c.grow>>c.allocate>>c.begin>>c.phase>>c.commit>>c.rollback;
                events.clear();next=attempts=0;bool timeout=old_timeout;
                const auto attempt=[&](bool slots,bool fallback,bool& flag){require(attempts<2,"Native emitted extra character attempt");const auto cfg=configs[attempts++];events.push_back("ATTEMPT "+std::to_string(slots)+' '+std::to_string(fallback));CharacterSavePort port;port.growth=[&](){events.push_back("GROW");return cfg.grow;};port.acquire=[&](){events.push_back("ALLOC");return cfg.allocate;};port.lock=[&](){events.push_back("LOCK");};port.unlock=[&](){events.push_back("UNLOCK");};port.begin=[&](){events.push_back("BEGIN");return cfg.begin;};port.commit=[&](){events.push_back("COMMIT");return cfg.commit;};port.rollback=[&](){events.push_back("ROLLBACK");return cfg.rollback;};port.timeout=[&](){return timeout;};port.recovery=[&](){events.push_back("RECOVERY");};port.acknowledge=[&](){events.push_back("ACK");};port.release=[&](){events.push_back("FREE");};port.diagnostic=[&](std::string_view){events.push_back("LOG");};port.phases=[&](bool explicit_slots){events.push_back("PHASES "+std::to_string(explicit_slots));if(cfg.phase) {if(cfg.phase!=3) timeout=cfg.phase==2;throw std::runtime_error("Original phase fault");}};return execute_character_save(present,slots,fallback,flag,port);};
                if(kind=="DB") {returned=attempt(quick,active_or_recovery,sticky);++db_cases;}
                else {UserCharacterExitPort port;port.purchases_pending=[&](){events.push_back("PENDING");return pending;};port.save_purchases=[&](){events.push_back("PURCHASES");return purchase_ok;};port.attempt=attempt;port.save_friends=[&](){events.push_back("FRIENDS");};port.save_blocks=[&](){events.push_back("BLOCKS");};port.queue_exit=[&](){events.push_back("QUEUE_EXIT");};port.exit=[&](){events.push_back("EXIT");};port.save_leave=[&](){events.push_back("LEAVE");};const bool exiting=kind=="EXIT" || kind=="SHUTDOWN";const auto result=exiting?execute_user_character_exit(kind=="SHUTDOWN",port):execute_user_character_save(periodic,quick,active_or_recovery,port);returned=result.legacy_success;sticky=result.timeout_seen;require(result.attempts==attempts,"Native user attempt accounting differs");if(exiting) ++exit_cases;else ++user_cases;}
                ++cases;
            }
            else if(tag=="TICK") {std::uint32_t status=0,now=0,nxt=0,last=0,expected_next=0,expected_last=0;unsigned int queued=0;row>>status>>now>>nxt>>last>>expected_next>>expected_last>>queued;unsigned int posts=0;const bool posted=update_character_save_tick(status,now,nxt,last,[&](std::uint32_t job){require(job==8,"Unexpected periodic save job");++posts;});require(nxt==expected_next && last==expected_last && posts==queued && posted==bool(queued),"Original absolute save tick differs");++tick_cases;}
            else if(tag=="RETURN") {bool expected=false,timeout=false;unsigned int count=0;row>>expected>>timeout>>count;require(returned==expected && sticky==timeout && attempts==count,"Original character save outcome differs at case "+std::to_string(cases));}
            else if(tag=="EVENT") {require(next<events.size() && events[next]==line.substr(6),"Save owner ordering differs at case "+std::to_string(cases)+" event "+std::to_string(next)+"\nactual: "+(next<events.size()?events[next]:"<none>")+"\nsource: "+line.substr(6));++next;}
            else if(tag=="END") require(next==events.size(),"Native emitted extra save owner effects");
            else throw std::runtime_error("Unknown save owner oracle row");
        }
        require(db_cases==3072 && user_cases==2048 && exit_cases==2048 && tick_cases==5024 && cases==7168,"Save owner oracle coverage changed");bool untouched=true;CharacterSavePort empty;require(!execute_character_save(false,false,true,untouched,empty) && untouched,"Null original user invoked save ports or changed sticky timeout");
        std::cout<<db_cases<<" original DB-save cases and "<<user_cases<<" user/periodic retries, "<<exit_cases<<" exits and "<<tick_cases<<" absolute save ticks passed\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
