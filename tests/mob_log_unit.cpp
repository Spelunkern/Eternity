#include "eternity/game/mob_log.hpp"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;using namespace eternity::game;
namespace {
void check(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
net::Packet decode(const std::string& text) {
    check(text.size()%2==0,"Malformed mob log hex");net::Packet bytes;
    for(std::size_t i=0;i<text.size();i+=2) bytes.push_back(static_cast<std::uint8_t>(std::stoul(text.substr(i,2),nullptr,16)));
    return bytes;
}
std::optional<std::string> text(const std::string& encoded) {
    if(encoded=="N") return std::nullopt;
    if(encoded=="E") return std::string{};
    const auto bytes=decode(encoded);return std::string(bytes.begin(),bytes.end());
}
std::optional<std::string_view> view(const std::optional<std::string>& value) {return value?std::optional<std::string_view>{*value}:std::nullopt;}
std::uint32_t number(const net::Packet& bytes,std::size_t offset) {std::uint32_t n=0;for(unsigned i=0;i<4;++i) n|=std::uint32_t(bytes.at(offset+i))<<(8*i);return n;}
struct Transport final:MobLogTransport {
    std::vector<net::Packet> packets;
    void send_game_log(const net::Packet& p) override {packets.push_back(p);}
};
}
int main(int argc,char** argv) {try {
    check(argc==2,"Expected original mob log oracle");std::ifstream input(argv[1]);check(input.good(),"Cannot read mob log oracle");unsigned cases=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#') continue;
        std::istringstream row(line);
        unsigned scenario,scene,map,id,event;std::uint32_t value,hp;std::string positions,name,user,detail,seed,expected,zero;
        row>>scenario>>scene>>map>>id>>event>>value>>hp>>positions>>name>>user>>detail>>seed>>expected>>zero;check(static_cast<bool>(row),"Malformed mob log fixture");std::string extra;check(!(row>>extra),"Trailing mob log data");
        const auto name_bytes=text(name),user_bytes=text(user),detail_bytes=text(detail);check(name_bytes.has_value(),"Mob definition name cannot be absent");const auto bits=decode(positions);check(bits.size()==12,"Wrong mob position shape");
        MobRuntimeTemplate definition;definition.id=static_cast<std::uint16_t>(id);MobRuntime mob;mob.definition=&definition;mob.hp=hp;
        mob.position={std::bit_cast<float>(number(bits,0)),std::bit_cast<float>(number(bits,4)),std::bit_cast<float>(number(bits,8))};
        // The original routine only tests zone presence; map identity is an
        // explicit projection and no ZoneMobs operation executes in this fixture.
        std::uint64_t zone_tag=0;mob.zone=scene?reinterpret_cast<ZoneMobs*>(&zone_tag):nullptr;
        const MobLogContext context{static_cast<std::uint16_t>(map),*name_bytes};auto packet=decode(seed);const auto initial=packet;
        const auto written=write_mob_log(packet,mob,context,static_cast<std::uint8_t>(event),view(user_bytes),view(detail_bytes),value);
        check(written==static_cast<bool>(scene),"Original mob log zone guard differs");
        check(packet==(scene?decode(expected):initial),"Original mob log fields/string tails differ in scenario "+std::to_string(scenario));
        Transport transport;const auto sent=send_mob_log(mob,context,static_cast<std::uint8_t>(event),view(user_bytes),view(detail_bytes),value,transport);
        check(sent==static_cast<bool>(scene) && transport.packets.size()==scene && (!scene || transport.packets[0]==decode(zero)),"Original zero-initialized mob log delivery differs");++cases;
        if(event==4 && value<=1) {
            const auto skill=detail_bytes.value_or(std::string{});check(skill.size()<=31,"Invalid death fixture skill");std::copy(skill.begin(),skill.end(),mob.previous_attack_skill.begin());
            Transport death_transport;const auto death_sent=send_mob_death_log(mob,context,view(user_bytes),value!=0,death_transport);
            check(death_sent==static_cast<bool>(scene) && death_transport.packets.size()==scene && (!scene || death_transport.packets[0]==decode(zero)),"Mob death log adapter lost previous skill or pre-reset fair flag");
        }
    }
    check(cases==512,"Mob log coverage changed");
    MobRuntime absent;check(!write_mob_log({},absent,{0,std::string(100,'x')},4,std::nullopt,std::nullopt,1),"Absent-zone log inspected invalid input");
    std::uint64_t zone_tag=0;MobRuntimeTemplate definition;absent.zone=reinterpret_cast<ZoneMobs*>(&zone_tag);absent.definition=&definition;net::Packet storage(123,0xa5);const auto before=storage;
    bool rejected=false;try {write_mob_log(storage,absent,{0,std::string(32,'x')},4,std::nullopt,std::nullopt,1);} catch(const std::invalid_argument&) {rejected=true;}
    check(rejected && storage==before,"Unsafe source string did not fail before packet mutation");
    std::cout<<cases<<" complete original mob log cases, initialized string tails and transport guards passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
