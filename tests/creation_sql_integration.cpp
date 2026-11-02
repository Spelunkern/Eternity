#include "eternity/game/creation.hpp"
#include <atomic>
#include <iostream>
#include <thread>

void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main(int argc,char** argv) {
    using namespace eternity;
    try {
        if(argc!=2 || !std::string_view(argv[1]).starts_with("Eternity_Test_Creation_"))
            throw std::runtime_error("Creation integration requires its disposable fixture catalog");
        const std::string catalog=argv[1];
        const auto config=storage::SqlServerConfig::from_environment();storage::SqlServer database(config);
        auto definitions=game::Definitions::load(database);
        const auto rules=game::CreationRules::load(definitions);
        const auto base=std::chrono::sys_days{std::chrono::year{2024}/1/1};
        const auto made=base+std::chrono::hours{12}+std::chrono::minutes{34}+std::chrono::seconds{56};
        game::Characters characters(database,1,base,catalog);
        auto empty=characters.load(42);
        require(empty.new_account && empty.country==2,"Disposable fixture did not begin empty");
        require(characters.save_country(42,0,true),"Original country insert failed");
        require(characters.load(42).country==0 && characters.load(42).max_grow==3,"Country insert did not preserve available SQL max-grow behavior");
        require(characters.save_country(42,1,false) && characters.load(42).country==1,"Original country update failed");
        game::CreateRequest request{0,0,0,1,2,3,0,1,"Native'Hero"};
        auto draft=rules.prepare(request,0);
        require(draft.items.size()==2,"Original human/fighter starter item definition count changed");
        for(std::size_t i=0;i<draft.items.size();++i) draft.items[i].uid=0x1234567800010000ULL+i;
        const auto created=characters.create(42,"test_user",draft,made);
        if(created.result!=0) throw std::runtime_error("Original SQL character create failed: "+created.error);
        auto roster=characters.load(42);
        require(roster.slots[0] && roster.slots[0]->id==created.character_id && roster.slots[0]->name=="Native'Hero",
                "Created character identity/name did not round-trip");
        const auto& character=*roster.slots[0];
        require(character.hp==250 && character.mp==85 && character.sp==195 && character.luck==15 && character.level==1,
                "Original creation stats did not round-trip");
        require(character.equipment_type[5]==5 && character.equipment_id[5]==1,"Starter weapon was not equipped in original slot five");
        const auto details=database.query("SELECT SkillPoint,PosX,PosY,PosZ,Vg FROM ["+catalog+"].dbo.Chars WHERE UserUID=42; "
            "SELECT Bag,Slot,Quality,CONVERT(varchar(40),CONVERT(varbinary(20),Craftname),2),[Count],MakeType,"
            "CONVERT(varchar(19),MakeTime,120) FROM ["+catalog+"].dbo.CharItems ORDER BY Bag,Slot;");
        require(details.size()==2 && details[0].rows[0][0]==storage::Cell{"5"},"Recovered procedure's SkillPoint override was lost");
        require(details[0].rows[0][4]==storage::Cell{"200"} && details[1].rows.size()==2,"Original initial vigor or item count changed");
        require(details[1].rows[0][0]==storage::Cell{"0"} && details[1].rows[0][1]==storage::Cell{"5"} &&
                details[1].rows[0][2]==storage::Cell{"1200"},"Original starter item placement/quality changed");
        // The restored database's Chinese procedure-parameter collation loses the five Korean characters.
        if(details[1].rows[0][3]!=storage::Cell{"3F3F3F3F3F"})
            throw std::runtime_error("Restored SQL procedure craft-label conversion changed: "+details[1].rows[0][3].value_or("NULL"));
        require(std::stof(*details[0].rows[0][1])==813.0F && std::stof(*details[0].rows[0][2])==39.0F &&
                std::stof(*details[0].rows[0][3])==1710.0F,"Original fallback start coordinates were not persisted");
        require(details[1].rows[0][5]==storage::Cell{"S"} && details[1].rows[0][6]==storage::Cell{"2024-01-01 12:34:56"},
                "Original starter item time/make type changed");
        const auto occupied=characters.create(42,"test_user",draft,made);
        require(occupied.result==1,"Occupied slot accepted a second character");
        auto duplicate=draft;duplicate.request.slot=1;
        require(characters.create(42,"test_user",duplicate,made).result==1,"Duplicate original character name accepted");
        auto failed=draft;failed.request.slot=1;failed.request.name="FailedHero";failed.items.back().item_id=2147483000;
        require(characters.create(42,"test_user",failed,made).result==1,"Fault injection did not fail second item creation");
        const auto rolled_back=database.query("SELECT COUNT(*) FROM ["+catalog+"].dbo.Chars; SELECT COUNT(*) FROM ["+catalog+"].dbo.CharItems;");
        require(rolled_back.size()==2 && rolled_back[0].rows[0][0]==storage::Cell{"1"} && rolled_back[1].rows[0][0]==storage::Cell{"2"},
                "Starter item failure left a character or partial inventory behind");
        failed.items.back().item_id=draft.items.back().item_id;
        const auto recovered=characters.create(42,"test_user",failed,made);
        if(recovered.result!=0) throw std::runtime_error("Connection did not recover after failed starter item save: "+recovered.error);
        std::atomic<unsigned int> successes{0};std::atomic_bool error{false};std::vector<std::jthread> racers;
        for(unsigned int i=0;i<2;++i) racers.emplace_back([&,i] {
            try {
                storage::SqlServer connection(config);game::Characters repository(connection,1,base,catalog);
                auto shared=draft;shared.request.name="SharedName";
                if(repository.create(100+i,"race_user",shared,made).result==0) ++successes;
            } catch(...) {error=true;}
        });
        racers.clear();require(!error && successes==1,"Concurrent name creation bypassed original uniqueness semantics");
        std::cout<<"Original creation SQL passed: stats, country, starter items, bound names, transaction rollback and concurrent names.\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
