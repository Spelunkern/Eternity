#include "eternity/game/characters.hpp"
#include <iostream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {
    using namespace eternity;
    try {
        storage::SqlServer database(storage::SqlServerConfig::from_environment());
        constexpr auto snapshot="SELECT COUNT(*) FROM PS_GameData.dbo.Chars; SELECT COUNT(*) FROM PS_GameData.dbo.CharItems; "
            "SELECT COUNT(*) FROM PS_GameData.dbo.UserMaxGrow; SELECT IDENT_CURRENT('PS_GameData.dbo.Chars'); "
            "SELECT IDENT_CURRENT('PS_GameData.dbo.CharItems'); SELECT IDENT_CURRENT('PS_GameData.dbo.UserMaxGrow');";
        const auto before=database.query(snapshot);
        const auto base=std::chrono::sys_days{std::chrono::year{2024}/2/28};
        game::Characters characters(database,255,base);
        auto roster=characters.load(2147483000);
        require(roster.new_account && roster.country==2 && roster.max_grow==2,"Missing country did not use original defaults");
        for(const auto& slot:roster.slots) require(!slot,"Synthetic account ID already has characters");
        database.begin();
        try {
            // IDs stay below existing identity seeds: rollback preserves both rows and counters.
            database.query(
                "SET NOCOUNT ON; "
                "IF EXISTS (SELECT 1 FROM PS_GameData.dbo.Chars WHERE CharID IN (1,2)) OR "
                "EXISTS (SELECT 1 FROM PS_GameData.dbo.CharItems WHERE RowID IN (1,2,3,4)) OR "
                "EXISTS (SELECT 1 FROM PS_GameData.dbo.UserMaxGrow WHERE RowID=1 OR (ServerID=255 AND UserUID=2147483000)) "
                "THROW 50001, 'Character fixture identifiers are occupied', 1; "
                "IF IDENT_CURRENT('PS_GameData.dbo.Chars')<2 OR IDENT_CURRENT('PS_GameData.dbo.CharItems')<4 OR "
                "IDENT_CURRENT('PS_GameData.dbo.UserMaxGrow')<1 THROW 50002, 'Fixture would advance identity counters', 1; "
                "SET IDENTITY_INSERT PS_GameData.dbo.Chars ON; "
                "INSERT PS_GameData.dbo.Chars (CharID,ServerID,UserID,UserUID,CharName,Slot,Family,Grow,Hair,Face,Size,Job,Sex,Level,Str,Dex,Rec,Int,Luc,Wis,HP,MP,SP,Hg,Vg,Cg,Og,Ig,RenameCnt,RemainTime,RegDate) "
                "VALUES (1,255,'et_fixture',2147483000,' Native Hero ',3,1,2,3,4,5,0,1,42,101,102,103,104,105,106,501,502,503,0,0,0,0,0,2,60,'20240229 00:00:00'), "
                "(2,254,'et_fixture',2147483000,'Other realm',1,1,2,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,0,0,0,0,'20240229 00:00:00'); "
                "SET IDENTITY_INSERT PS_GameData.dbo.Chars OFF; "
                "SET IDENTITY_INSERT PS_GameData.dbo.CharItems ON; "
                "INSERT PS_GameData.dbo.CharItems (RowID,CharID,ItemID,ItemUID,Type,TypeID,Bag,Slot,Quality,Gem1,Gem2,Gem3,Gem4,Gem5,Gem6,Count,MakeTime,MakeType,Del) "
                "VALUES (1,1,1001,1,1,7,0,0,0,0,0,0,0,0,0,1,GETDATE(),'N',0), "
                "(2,1,1002,2,30,8,0,7,0,11,12,13,14,15,16,1,GETDATE(),'N',0), "
                "(3,1,1003,3,99,99,1,1,0,0,0,0,0,0,0,1,GETDATE(),'N',0), "
                "(4,1,1004,4,88,88,0,2,0,0,0,0,0,0,0,1,GETDATE(),'N',1); "
                "SET IDENTITY_INSERT PS_GameData.dbo.CharItems OFF; "
                "SET IDENTITY_INSERT PS_GameData.dbo.UserMaxGrow ON; "
                "INSERT PS_GameData.dbo.UserMaxGrow (RowID,ServerID,UserUID,Country,MaxGrow) VALUES(1,255,2147483000,1,1); "
                "SET IDENTITY_INSERT PS_GameData.dbo.UserMaxGrow OFF;");
            roster=characters.load(2147483000);
            require(!roster.new_account && roster.country==1 && roster.max_grow==3,"Available original max-grow procedure was bypassed");
            require(roster.slots[3].has_value() && !roster.slots[1],"Character realm isolation changed");
            const auto& c=*roster.slots[3];
            require(c.id==1 && c.name=="NativeHero" && c.created==86400 && c.level==42,"Character identity/date/name conversion failed");
            require(c.wisdom==106 && c.luck==105 && c.hp==501 && c.rename && c.remaining_time,"Original stat/flag mapping changed");
            require(c.equipment_type[0]==1 && c.equipment_id[0]==7 && c.equipment_type[7]==30 && c.equipment_id[7]==8,
                    "Equipped item procedure parameters changed");
            require(c.equipment_type[1]==0 && c.equipment_type[2]==0 && c.cloak==std::array<std::uint8_t,6>{11,12,13,14,15,16},
                    "Cloak, bag or deleted-item filtering changed");
            const auto packets=game::character_list(roster);
            require(packets.size()==6 && packets.back().size()==83 && packets.back()[75]==1 && packets.back()[76]==1,
                    "SQL-loaded character failed wire serialization");
            database.query("UPDATE PS_GameData.dbo.Chars SET Del=1 WHERE CharID=1;");
            const auto deleted=characters.load(2147483000);
            require(!deleted.slots[3],"Deleted character remained selectable");
            database.query("UPDATE PS_GameData.dbo.Chars SET Del=0,Slot=5 WHERE CharID=1;");
            bool invalid_slot=false;
            try{(void)characters.load(2147483000);}catch(const std::runtime_error&){invalid_slot=true;}
            require(invalid_slot,"Out-of-range database character slot was admitted");
            database.rollback();
        } catch(...) {
            try{database.rollback();}catch(...){}
            throw;
        }
        const auto after=database.query(snapshot);
        require(before.size()==6 && after.size()==6,"Fixture rollback verification shape changed");
        for(std::size_t i=0;i<before.size();++i) require(before[i].rows==after[i].rows,"Character fixture changed rows or identity counters");
        std::cout<<"Original character roster, equipment and country SQL contracts passed with rollback.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
