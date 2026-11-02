#include "eternity/game/player_packets.hpp"
#include <iostream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {
    using namespace eternity;
    try {
        storage::ResultSet table;table.columns={"Level","Grow","Exp"};
        for(unsigned int grow=1;grow<=4;++grow) for(unsigned int level=1;level<=60;++level)
            table.rows.push_back({std::to_string(level),std::to_string(grow),std::to_string(grow)});
        const auto experience=game::Experience::load(table);
        require(experience.bounds(0,1).previous==0 && experience.bounds(0,1).next==1 && experience.bounds(2,3).previous==6 &&
                experience.bounds(2,3).next==9 && experience.bounds(2,61).previous==180 && experience.bounds(2,61).next==60 &&
                experience.bounds(7,1).previous==60 && experience.bounds(7,1).next==60,"Original cumulative experience/bounds changed");
        for(std::size_t i=0;i<60;++i) table.rows[i][2]=i<3?"2000000000":"0";
        const auto wrapped=game::Experience::load(table);
        require(wrapped.bounds(0,3).previous==4000000000U && wrapped.bounds(0,3).next==1705032704U,"Original DWORD experience accumulation changed");
        game::PlayerCore player;player.stat_points=0x102;player.skill_points=0x304;player.direction=0xdef0;
        player.experience=0x99aabbcc;player.money=0xddeeff00;player.x=1.0F;player.y=-2.0F;player.z=0.5F;
        player.kills={0x01020304,5,6,0xdeadbeef};
        const auto packet=game::character_data(player,{0x12345,0x6789,0xabcd},{0x11223344,0x55667788},"AB");
        const net::Packet expected={5,1,2,1,4,3,0x45,0x23,0x89,0x67,0xcd,0xab,0xf0,0xde,
            0x44,0x33,0x22,0x11,0x88,0x77,0x66,0x55,0xcc,0xbb,0xaa,0x99,0,0xff,0xee,0xdd,
            0,0,0x80,0x3f,0,0,0,0xc0,0,0,0,0x3f,4,3,2,1,5,0,0,0,6,0,0,0,0xef,0xbe,0xad,0xde,'A','B',0};
        require(packet==expected,"Original variable-length TP_CHAR_DATA wire bytes changed");
        require(game::character_data(player,{},{},"").size()==59 && game::character_data(player,{},{},std::string(24,'A')).size()==83,
                "Character data guild-name extent changed");
        bool invalid=false;try{(void)game::character_data(player,{},{},std::string(25,'A'));}catch(const std::invalid_argument&){invalid=true;}
        require(invalid,"Oversize original guild name accepted");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
