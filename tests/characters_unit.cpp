#include "eternity/game/characters.hpp"
#include <iostream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {
    using namespace eternity::game;
    try {
        const auto leap=std::chrono::sys_days{std::chrono::year{2024}/2/29}+std::chrono::hours{23}+
            std::chrono::minutes{59}+std::chrono::seconds{58};
        require(pack_world_time(leap)==0x60bb7efa && unpack_world_time(0x60bb7efa)==leap,"Packed original world calendar changed");
        bool invalid=false;try{(void)unpack_world_time(0);}catch(const std::runtime_error&){invalid=true;}
        require(invalid,"Invalid packed calendar silently normalized");
        CharacterRoster roster;
        const auto empty=character_list(roster);
        require(empty.size()==6 && empty[0]==eternity::net::Packet({9,1,2,2}),"New account country/grow changed");
        for(std::size_t i=0;i<5;++i)
            require(empty[i+1]==eternity::net::Packet({1,1,static_cast<std::uint8_t>(i),0,0,0,0}),"Empty slot wire shape changed");
        CharacterSummary c;c.id=0x12345678;c.created=0x87654321;c.slot=3;c.level=0x102;
        c.family=1;c.grow=2;c.hair=3;c.face=4;c.size=5;c.job=6;c.sex=7;c.map=0x809;
        c.strength=0x1001;c.dexterity=0x2002;c.recovery=0x3003;c.intelligence=0x4004;c.wisdom=0x5005;c.luck=0x6006;
        c.hp=0x7007;c.mp=0x8008;c.sp=0x9009;c.equipment_type={1,2,3,4,5,6,7,8};c.equipment_id={9,10,11,12,13,14,15,16};
        c.name="NativeCharacter";c.cloak={17,18,19,20,21,22};c.rename=true;c.remaining_time=true;roster.slots[3]=c;
        const auto packets=character_list(roster,false);
        require(packets.size()==5 && packets[0][2]==0 && packets[1][2]==1 && packets[2][2]==2 && packets[3][2]==4,
                "Original empty-before-populated ordering changed");
        const eternity::net::Packet expected={
            1,1,3,0x78,0x56,0x34,0x12,0x21,0x43,0x65,0x87,2,1,1,2,3,4,5,6,7,9,8,
            1,0x10,2,0x20,3,0x30,4,0x40,5,0x50,6,0x60,7,0x70,8,0x80,9,0x90,
            1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,
            'N','a','t','i','v','e','C','h','a','r','a','c','t','e','r',0,0,0,0,1,1,17,18,19,20,21,22};
        require(packets.back()==expected,"Original packed character record changed");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
