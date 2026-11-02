#include "eternity/game/creation.hpp"
#include <fstream>
#include <iostream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
eternity::game::Definitions definitions() {
    using namespace eternity;game::Definitions defs;
    defs.catalogs["StatusDefs"].columns={"Level","Job","HP","SP","MP"};
    const std::array<std::array<int,3>,6> status={{{205,125,50},{250,65,65},{170,150,60},{175,135,70},{150,60,170},{160,70,150}}};
    for(unsigned int job=0;job<6;++job) defs.catalogs["StatusDefs"].rows.push_back(
        {"1",std::to_string(job),std::to_string(status[job][0]),std::to_string(status[job][1]),std::to_string(status[job][2])});
    defs.catalogs["Items"]={{"Type","TypeID","Quality"},{{"25","1","0"},{"5","1","1200"},{"1","1","1300"}}};
    defs.catalogs["BaseItemDefs"]={{"Family","Job","ItemID","Type","TypeID","Count","Slot","Quality"},
        {{"0","0","25001","25","1","10","2","999"},{"0","0","5001","5","1","1","1","999"},
         {"0","0","1001","1","1","1","1","999"},{"0","0","999","99","1","1","0","999"}}};
    return defs;
}
int main() {
    using namespace eternity;using namespace eternity::game;
    try {
        auto defs=definitions();auto rules=CreationRules::load(defs);
        CharacterRoster roster;roster.country=0;roster.max_grow=3;
        net::Packet packet(31,0);packet[0]=2;packet[1]=1;
        const std::string spaced=" Native Hero ";std::copy(spaced.begin(),spaced.end(),packet.begin()+10);
        auto checked=check_create(packet,roster);
        require(checked.request && checked.request->name=="NativeHero" && !checked.close,"Original name trimming failed");
        const auto normal=rules.prepare(*checked.request,0);
        require(normal.character.strength==14 && normal.character.dexterity==12 && normal.character.recovery==9 &&
                normal.character.intelligence==8 && normal.character.wisdom==7 && normal.character.luck==15,"Human fighter base stats changed");
        require(normal.character.hp==250 && normal.character.sp==195 && normal.character.mp==85,"Original normal-mode resources changed");
        require(normal.items.size()==3 && normal.items[0].bag==1 && normal.items[0].slot==0 && normal.items[0].count==10 &&
                normal.items[1].bag==0 && normal.items[1].slot==5 && normal.items[1].quality==1200 &&
                normal.items[2].bag==1 && normal.items[2].slot==1,"Original initial equipment placement/quality changed");
        auto request=*checked.request;request.grow=2;const auto hard=rules.prepare(request,0);
        require(hard.character.hp==270 && hard.character.sp==207 && hard.character.mp==90,"Hard-mode multiplier did not truncate before racial/stat bonuses");
        request.grow=3;const auto ultimate=rules.prepare(request,0);
        require(ultimate.character.hp==352 && ultimate.character.sp==257 && ultimate.character.mp==110,"Ultimate-mode resources changed");
        request.grow=0;request.family=3;request.job=1;const auto dark=rules.prepare(request,0);
        require(dark.character.strength==12 && dark.character.recovery==14 && dark.character.dexterity==9 && dark.character.hp==330,
                "Dark-family racial stats changed");
        request.family=2;request.job=2;const auto vail=rules.prepare(request,0);
        require(vail.character.intelligence==9 && vail.character.wisdom==10 && vail.character.sp==230 && vail.character.mp==115,
                "Vail racial resources changed");
        roster.country=2;require(check_create(packet,roster).result==1,"No-country create did not return original failure result");roster.country=0;
        packet[2]=5;require(check_create(packet,roster).close,"Invalid slot accepted");packet[2]=0;
        packet[4]=4;require(check_create(packet,roster).close,"Mode above account maximum accepted");packet[4]=0;
        packet[3]=0;packet[8]=2;require(check_create(packet,roster).close,"Invalid race/class pair accepted");packet[8]=0;
        std::fill(packet.begin()+10,packet.end(),' ');packet[10]='A';require(check_create(packet,roster).result==1,"Single-byte name accepted");
        packet[11]='B';require(check_create(packet,roster).request.has_value(),"Original two-byte minimum changed");
        roster.slots[0]=CharacterSummary{};require(check_create(packet,roster).close,"Occupied slot accepted");

        const auto file=std::filesystem::temp_directory_path()/("eternity-creation-map-"+
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ini");
        {std::ofstream output(file);output<<"/* HUGameStartMapPos=-1,0,0,0 */\n[BASE]\n"
            "HUGameStartMapPos=(1,577.98,78.56,1762.12) (3,1,2,3) // comment\n[1]\nHUGameStartMapPos=-1,0,0,0\n";}
        StartPositions positions;
        try{positions=read_start_positions(file);}catch(...){std::filesystem::remove(file);throw;}
        std::filesystem::remove(file);
        require(positions[0].size()==2 && positions[0][0].x==577.98F && positions[0][1].map==3 && positions[1][0].x==1489.0F,
                "Map BASE configuration, multiple starts or original fallback changed");
        const auto configured=CreationRules::load(defs,positions);
        request=*checked.request;const auto alternate=configured.prepare(request,1);
        require(alternate.position.map==3 && alternate.position.y==2.0F && alternate.character.map==3,"Configured start index was ignored");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
