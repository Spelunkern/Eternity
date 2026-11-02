#include "eternity/game/player_packets.hpp"
#include <iostream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
eternity::storage::Row item(unsigned int bag,unsigned int slot,std::int64_t uid,bool bank=false) {
    eternity::storage::Row row={"5001","5","1",std::to_string(uid),std::to_string(bag),std::to_string(slot),"1200",
        "1","2","3","4","5","6","12 34 ","1","S","2024","1","1","0","0","0"};
    if(bank) row.erase(row.begin()+4);return row;
}
int main() {
    using namespace eternity;
    try {
        storage::Row row(53,storage::Cell{"0"});row[0]="42";row[1]="99";row[2]=" Player Hero ";row[3]="2";
        row[4]="1";row[5]="2";row[9]="4";row[11]="30";row[12]="7";row[13]="9";
        for(std::size_t i=0;i<6;++i) row[14+i]=std::to_string(501+i);
        row[20]="601";row[21]="602";row[22]="603";row[23]="2";row[24]="90";row[25]="123";row[26]="456";
        row[27]="1.5";row[28]="2.5";row[29]="-3.5";row[31]="-5";
        row[36]="-1";row[37]="70000";row[38]="-2147483648";row[39]="1234";row[40]="5000000000";
        row[41]="77";row[42]="2";row[43]="3";row[44]="4";row[45]=std::nullopt;row[46]="1";
        for(std::size_t i=47;i<53;++i) row[i]=std::nullopt;
        const auto core=game::decode_player_core(row);
        require(core.user_id==42 && core.character.id==99 && core.character.name=="PlayerHero" && core.character.slot==2,"Player identity mapping changed");
        require(core.character.strength==501 && core.character.dexterity==502 && core.character.recovery==503 && core.character.intelligence==504 &&
                core.character.luck==505 && core.character.wisdom==506 && core.character.hp==601 && core.character.mp==602 && core.character.sp==603,
                "Player stat order changed");
        require(core.stored_money==5000000000ULL && core.vigor==-5 && core.kills==std::array<std::uint32_t,4>{65535,70000,0,1234} &&
                !core.reset_stats && core.reset_skills && !core.last_left,"Player legacy counts, nullable fields or bigint lost fidelity");
        game::PlayerSnapshot snapshot;snapshot.catalogs["Items"].rows={item(0,5,1),item(0,5,2),item(1,0,3),item(6,0,4)};
        snapshot.catalogs["Bank"].rows={item(0,3,10,true),item(0,3,11,true)};
        snapshot.catalogs["Skills"].rows={{"7","1","12","4"},{"8","2","90","5"}};
        game::decode_player_inventory(snapshot);
        require(snapshot.items.size()==3 && snapshot.items[0].uid==1 && snapshot.items[0].bag==0 && snapshot.items[0].slot==5 &&
                snapshot.items[1].uid==3 && snapshot.items[2].uid==2 && snapshot.items[2].bag==1 && snapshot.items[2].slot==1 && snapshot.items[2].relocated,
                "Original duplicate inventory relocation changed");
        require(snapshot.items[0].craft=="1234" && snapshot.items[0].made_at==0x60420000 && snapshot.items[0].gems[5]==6,
                "Player item craft/date/gem mapping changed");
        require(snapshot.bank.size()==2 && snapshot.bank[0].uid==11 && snapshot.bank[0].slot==0 && snapshot.bank[0].relocated &&
                snapshot.bank[1].uid==10 && snapshot.bank[1].slot==3,"Original duplicate bank relocation changed");
        require(snapshot.skills.size()==2 && snapshot.skills[0].number==0 && snapshot.skills[0].persisted_number==12 &&
                snapshot.skills[1].number==1 && snapshot.skills[1].persisted_number==90,"Original skill load did not renumber sorted records");
        snapshot.catalogs["Items"].rows={item(0,5,1),item(0,5,999)};
        for(unsigned int bag=1;bag<6;++bag) for(unsigned int slot=0;slot<24;++slot) snapshot.catalogs["Items"].rows.push_back(item(bag,slot,1000+bag*24+slot));
        game::decode_player_inventory(snapshot);
        require(snapshot.items.size()==121 && snapshot.overflow_items.size()==1 && snapshot.overflow_items[0].uid==999,
                "Full inventory relocated a duplicate into equipment slots or lost its record");
        snapshot.catalogs["Skills"].rows.assign(256,storage::Row{"7","1","0","0"});
        bool excessive=false;try{game::decode_player_inventory(snapshot);}catch(const std::runtime_error&){excessive=true;}
        require(excessive,"Original 255-skill limit was not enforced");
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
