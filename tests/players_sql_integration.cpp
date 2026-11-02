#include "eternity/game/creation.hpp"
#include "eternity/game/player_packets.hpp"
#include <bit>
#include <iostream>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main(int argc,char** argv) {
    using namespace eternity;
    try {
        if(argc!=2 || !std::string_view(argv[1]).starts_with("Eternity_Test_Creation_")) throw std::runtime_error("Player load test requires disposable catalog");
        const std::string catalog=argv[1],prefix="["+catalog+"].dbo.";
        storage::SqlServer database(storage::SqlServerConfig::from_environment());
        const auto definitions=game::Definitions::load(database);const auto rules=game::CreationRules::load(definitions);
        const auto experience=game::Experience::load(definitions.catalogs.at("ExpDefs"));
        require(experience.bounds(0,1).previous==0 && experience.bounds(0,1).next==70,"Original level-one experience contract changed");
        const auto base=std::chrono::sys_days{std::chrono::year{2024}/1/1};game::Characters characters(database,1,base,catalog);
        require(characters.save_country(7001,0,true) && characters.save_country(7002,0,true),"Cannot initialize player fixture factions");
        auto own=rules.prepare({0,0,0,0,0,0,0,0,"LoadHero"},0),foreign=own;foreign.request.name="ForeignHero";
        for(std::size_t i=0;i<own.items.size();++i) {own.items[i].uid=0x2222000000000000ULL+i;foreign.items[i].uid=0x3333000000000000ULL+i;}
        const auto created=characters.create(7001,"load_user",own,base),other=characters.create(7002,"other_user",foreign,base);
        require(created.result==0 && other.result==0,"Cannot create player load fixtures");
        const auto cid=std::to_string(created.character_id),other_id=std::to_string(other.character_id);
        game::Players players(database,1,catalog),other_server(database,2,catalog);
        require(!players.load(7001,other.character_id,"load_user") && !other_server.load(7001,created.character_id,"load_user"),
                "Foreign account/server character was selected");
        for(const auto condition:{"RenameCnt=1","RemainTime=1","Del=1","Slot=5"}) {
            database.query("UPDATE "+prefix+"Chars SET "+condition+" WHERE CharID="+cid+";");
            require(!players.load(7001,created.character_id,"load_user"),"Unselectable character reached detail loader");
            database.query("UPDATE "+prefix+"Chars SET RenameCnt=0,RemainTime=0,Del=0,Slot=0 WHERE CharID="+cid+";");
        }
        const auto untouched=database.query("SELECT COUNT(*) FROM "+prefix+"Chars WHERE CharID IN ("+cid+","+other_id+") AND JoinDate IS NOT NULL;");
        require(untouched[0].rows[0][0]==storage::Cell{"0"},"Rejected selections updated character JoinDate");
        database.query("SET NOCOUNT ON; UPDATE "+prefix+"Chars SET K2=-1,LeaveDate='20240701 12:34:56',"
            "PosX=CAST(1.00000011920928955078125 AS real),PosY=577.98,PosZ=-39.5 WHERE CharID="+cid+"; "
            "INSERT "+prefix+"Users_Master (UserUID,UserID,Pw,Admin,AdminLevel,UseQueue,Status,[Leave],UserType,Enpassword,Point) "
            "VALUES(7001,'load_user','testpass',0,0,0,0,0,'N','',321); "
            "INSERT "+prefix+"UserStoredMoney(ServerID,UserUID,Money) VALUES(1,7001,5000000000); "
            "INSERT "+prefix+"CharEvents(CharID,Event1,Event2) VALUES("+cid+",1,0); "
            "INSERT "+prefix+"GuildChars(GuildID,CharID,GuildLevel,JoinDate) VALUES(77,"+cid+",2,GETDATE()); "
            "INSERT "+prefix+"CharItems(CharID,ItemID,ItemUID,Type,TypeID,Bag,Slot,Quality,Gem1,Gem2,Gem3,Gem4,Gem5,Gem6,Craftname,[Count],MakeTime,MakeType) "
            "VALUES("+cid+",1001,-1,1,1,0,5,900,1,2,3,4,5,6,'12 34',1,'20240102','M'); "
            "INSERT "+prefix+"UserStoredItems(ServerID,UserUID,ItemID,Type,TypeID,ItemUID,Slot,Quality,Gem1,Gem2,Gem3,Gem4,Gem5,Gem6,Craftname,[Count],MakeTime,MakeType) "
            "VALUES(1,7001,1001,1,1,1200,3,1000,0,0,0,0,0,0,'',1,'20240101','N'),"
            "(1,7001,1001,1,1,1300,3,1100,0,0,0,0,0,0,'',1,'20240102','N'); "
            "INSERT "+prefix+"CharSkills(CharID,SkillID,SkillLevel,Number,Delay,Del) VALUES("+cid+",10,1,7,9,0),("+cid+",11,2,20,13,0),("+cid+",12,3,21,17,1); "
            "INSERT "+prefix+"CharApplySkills(CharID,SkillID,SkillLevel,LeftResetTime) VALUES("+cid+",10,1,31); "
            "INSERT "+prefix+"CharQuests(CharID,QuestID,Delay,Count1,Count2,Count3,Success,Finish,Del) "
            "VALUES("+cid+",23,42,3,4,5,1,0,0),("+cid+",24,0,0,0,0,1,1,0),("+cid+",25,0,0,0,0,0,0,1); "
            "INSERT "+prefix+"CharQuickSlots(CharID,QuickBar,QuickSlot,Bag,Number) VALUES("+cid+",1,2,3,7); "
            "INSERT "+prefix+"CharSavePoint(CharID,Slot,MapID,PosX,PosY,PosZ) VALUES("+cid+",2,1,10.5,20.5,30.5); "
            "INSERT "+prefix+"PointLog(UserUID,CharID,UsePoint,ProductCode,UseType,UseDate) VALUES(7001,"+cid+",33,'fixture_product',1,'20240101'); "
            "INSERT "+prefix+"UserStoredPointItems(UserUID,Slot,ItemID,ItemCount) VALUES(7001,4,25001,2); "
            "INSERT "+prefix+"MarketCharConcern(CharID,MarketID) VALUES("+cid+",777); "
            "INSERT "+prefix+"FriendChars(CharID,FriendID,FriendName,Family,Grow,Job,Memo) VALUES("+cid+","+other_id+",'ForeignHero',0,0,0,NULL); "
            "INSERT "+prefix+"BanChars(CharID,BanID,BanName,Memo) VALUES("+cid+",88,'BlockedOne','memo'); "
            "INSERT "+prefix+"PointGiftNotify(UserUID,ProductCode,SendCharName,RecvDate) VALUES(7001,'gift_product','GiftSender','20240101');");
        auto snapshot=players.load(7001,created.character_id,"load_user");
        require(snapshot && snapshot->catalogs.size()==17 && snapshot->warnings.empty(),"Original complete player load did not produce all catalogs");
        const auto& core=snapshot->core;
        require(core.user_id==7001 && core.character.id==created.character_id && core.character.name=="LoadHero" &&
                core.character.hp==250 && core.skill_points==5 && core.vigor==200 && core.kills[1]==65535 && core.reset_stats && !core.reset_skills &&
                core.stored_money==5000000000ULL && core.guild_id==77 && core.guild_rank==2 && core.last_left.has_value(),"Player core fields did not survive SQL load");
        require(std::bit_cast<std::uint32_t>(core.x)==0x3f800001 && core.y==577.98F && core.z==-39.5F,"Player REAL coordinates lost their original binary precision");
        require(snapshot->items.size()==3 && snapshot->items[2].relocated && snapshot->items[2].uid==UINT64_MAX && snapshot->items[2].craft=="1234" &&
                snapshot->items[2].bag==1 && snapshot->items[2].slot==1,"Duplicate SQL inventory item did not relocate to the first free inventory slot");
        require(snapshot->bank.size()==2 && snapshot->bank[0].uid==1200 && snapshot->bank[0].relocated && snapshot->bank[1].uid==1300 &&
                snapshot->bank[1].slot==3,"Original bank item order/relocation changed");
        require(snapshot->skills.size()==2 && snapshot->skills[0].id==10 && snapshot->skills[0].number==0 && snapshot->skills[0].persisted_number==7 &&
                snapshot->skills[1].id==11 && snapshot->skills[1].number==1 && snapshot->skills[1].delay==13,"Original player skill ordering changed");
        require(snapshot->cash_points==321 && snapshot->guild_rejoin_hours>=71 && snapshot->guild_rejoin_hours<=72,
                "Original cash/guild output parameters were not captured");
        for(const auto name:{"AppliedSkills","Quests","FinishedQuests","QuickSlots","SavePoints","PointLog","PointItems","ConcernMarket","Friends","Blocked","GiftNotifications"})
            require(snapshot->catalogs.at(name).rows.size()==1,"Original ancillary player procedure did not load its fixture row");
        require(snapshot->catalogs.at("Quests").rows[0][0]==storage::Cell{"23"} && snapshot->catalogs.at("FinishedQuests").rows[0][0]==storage::Cell{"24"},
                "Original quest finish/delete filtering changed");
        const auto consumed=database.query("SELECT COUNT(*) FROM "+prefix+"PointGiftNotify WHERE UserUID=7001; SELECT COUNT(*) FROM "+prefix+
            "Chars WHERE CharID="+cid+" AND JoinDate IS NOT NULL; SELECT COUNT(*) FROM "+prefix+"Chars WHERE CharID="+other_id+" AND JoinDate IS NOT NULL;");
        require(consumed[0].rows[0][0]==storage::Cell{"0"} && consumed[1].rows[0][0]==storage::Cell{"1"} && consumed[2].rows[0][0]==storage::Cell{"0"},
                "Original gift consumption/JoinDate side effects changed or reached a foreign character");
        database.query("UPDATE "+prefix+"Users_Master SET Point=-9 WHERE UserUID=7001;");
        snapshot=players.load(7001,created.character_id,"load_user");
        require(snapshot && snapshot->cash_points==0 && snapshot->catalogs.at("GiftNotifications").rows.empty(),"Original negative-cash correction or one-shot gifts changed");
        const auto corrected=database.query("SELECT Point FROM "+prefix+"Users_Master WHERE UserUID=7001;");
        require(corrected[0].rows[0][0]==storage::Cell{"0"},"Original cash correction was not persisted in the fixture catalog");
        std::cout<<"Original player load SQL passed: owned core, inventory/bank repair, skills, quests, social, cash, gifts and JoinDate.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
