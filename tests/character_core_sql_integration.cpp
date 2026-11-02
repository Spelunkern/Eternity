#include "eternity/game/character_core_save.hpp"
#include "eternity/game/inventory_save.hpp"
#include "eternity/game/creation.hpp"
#include <bit>
#include <iostream>
#include <limits>

using namespace eternity;using namespace eternity::game;
namespace {
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void put(net::Packet& p,std::uint32_t value,unsigned int bytes) {for(unsigned int i=0;i<bytes;++i) p.push_back(static_cast<std::uint8_t>(value>>(8*i)));}
net::Packet packet(std::uint16_t op) {net::Packet p;put(p,op,2);put(p,9101,4);return p;}
net::Packet fields(std::uint16_t op,std::initializer_list<std::uint16_t> words) {auto p=packet(op);for(auto value:words) put(p,value,2);return p;}
net::Packet scalar(std::uint16_t op,std::uint32_t value) {auto p=packet(op);put(p,value,4);return p;}
net::Packet indexed(std::uint16_t op,std::uint8_t index,std::uint32_t value,unsigned int bytes) {auto p=packet(op);p.push_back(index);put(p,value,bytes);return p;}
net::Packet body(std::uint8_t hair,std::uint8_t face,std::uint8_t size,std::uint8_t sex) {auto p=packet(0x615);p.insert(p.end(),{hair,face,size,sex});return p;}
}
int main(int argc,char** argv) {
    try {
        require(argc==2 && std::string_view(argv[1]).starts_with("Eternity_Test_Creation_"),"Core SQL test requires disposable catalog");
        const std::string catalog=argv[1],prefix="["+catalog+"].dbo.";storage::SqlServer db(storage::SqlServerConfig::from_environment());
        const auto definitions=Definitions::load(db);const auto rules=CreationRules::load(definitions);const auto time=std::chrono::sys_days{std::chrono::year{2024}/1/1};Characters characters(db,1,time,catalog);
        require(characters.save_country(9101,0,true) && characters.save_country(9102,0,true),"Cannot initialize core fixture factions");
        auto draft=rules.prepare({0,0,0,0,0,0,0,0,"CoreHero"},0);draft.items.clear();const auto created=characters.create(9101,"core_user",draft,time);draft.request.name="CoreOther";const auto other=characters.create(9102,"core_other",draft,time);require(created.result==0 && other.result==0,"Cannot create core fixture characters");
        PlayerCore initial;initial.user_id=9101;initial.character=draft.character;initial.character.id=created.character_id;initial.money=123;
        InventoryReplicaPool pool;CharacterReplica cache(initial,pool);CharacterCoreStore core_store(db,catalog);InventoryStore items(db,1,catalog);const auto cid=std::to_string(created.character_id),other_id=std::to_string(other.character_id);
        const auto apply=[&](const net::Packet& p){require(cache.apply(p)==InventoryReplicaResult::applied,"Cannot apply core fixture command");};
        bool rejected=false;try{core_store.save_in_transaction(cache);}catch(const std::logic_error&){rejected=true;}require(rejected,"Core phase ran outside character transaction");
        auto level=packet(0x601);put(level,12,2);put(level,11,4);for(auto value:{65535U,32768U,65535U,32768U,5U,6U,7U,8U,100U,101U,102U}) put(level,value,2);apply(level);
        apply(scalar(0x602,UINT32_MAX));apply(scalar(0x603,0x80000001U));apply(fields(0x606,{50000,65000,65535}));
        auto position=fields(0x607,{65535,32768});for(float value:{std::bit_cast<float>(0x3f800001U),-std::numeric_limits<float>::denorm_min(),0.0078125F}) put(position,std::bit_cast<std::uint32_t>(value),4);apply(position);
        apply(indexed(0x605,1,32767,2));apply(indexed(0x605,2,0xffff,2));apply(indexed(0x605,3,255,2));apply(indexed(0x605,4,254,2));apply(indexed(0x605,5,253,2));
        for(std::uint8_t i=0;i<4;++i) apply(indexed(0x60c,i,UINT32_MAX-i,4));apply(indexed(0x60c,4,0xffff0001U,4));apply(indexed(0x60c,5,0xabcd00ffU,4));apply(body(1,2,3,1));
        PlayerItem item;item.uid=0x7777000000000300ULL;item.id=5001;item.type=5;item.type_id=1;item.bag=1;item.count=9;item.quality=100;item.made_at=pack_world_time(time);item.make_type="U";require(cache.inventory().load(item,false,1),"Cannot seed composite core inventory");
        auto gem=packet(0x708);gem.insert(gem.end(),{1,0,0,9});put(gem,UINT32_MAX,4);apply(gem);
        db.begin();core_store.save_in_transaction(cache);items.save_in_transaction(cache.inventory(),created.character_id);require(cache.body_changed() && cache.inventory().inventory()[0].save_type==1,"Composite phase acknowledged before character commit");db.commit();cache.core_save_success();cache.inventory().inventory_save_success();
        auto rows=db.query("SELECT [Level],StatPoint,SkillPoint,[Str],Dex,Rec,[Int],Wis,Luc,HP,MP,SP,Map,Dir,[Exp],Money,Hg,Vg,Cg,Og,Ig,K1,K2,K3,K4,KillLevel,DeadLevel,Hair,Face,Size,Sex,CASE WHEN LeaveDate IS NOT NULL THEN 1 ELSE 0 END,CONVERT(int,CONVERT(binary(4),PosX)),CONVERT(int,CONVERT(binary(4),PosY)),CONVERT(int,CONVERT(binary(4),PosZ)) FROM "+prefix+"Chars WHERE CharID="+cid+"; SELECT [Count],Gem1 FROM "+prefix+"CharItems WHERE CharID="+cid+"; SELECT [Level],Money,Hair,CASE WHEN LeaveDate IS NULL THEN 1 ELSE 0 END FROM "+prefix+"Chars WHERE CharID="+other_id+";");
        const std::vector<std::string> expected={"12","-1","-32768","-1","-32768","7","5","6","8","-15536","-536","-1","-1","-32768","-1","-1","32767","-1","255","254","253","-1","-2","-3","-4","1","255","1","2","3","1","1","1065353216","0",std::to_string(std::bit_cast<std::uint32_t>(0.007813F))};
        require(rows.size()==3 && rows[0].rows.size()==1 && rows[0].rows[0].size()==expected.size(),"Original core save result shape changed");for(std::size_t i=0;i<expected.size();++i) if(rows[0].rows[0][i]!=storage::Cell{expected[i]}) throw std::runtime_error("Core SQL field "+std::to_string(i)+" differs: "+rows[0].rows[0][i].value_or("NULL")+" vs "+expected[i]);
        require(rows[1].rows[0]==storage::Row{storage::Cell{"9"},storage::Cell{"9"}} && rows[2].rows[0]==storage::Row{storage::Cell{"1"},storage::Cell{"0"},storage::Cell{"0"},storage::Cell{"1"}},"Composite save did not preserve inventory or touched a different character");
        // A later item insert error must also roll back core/body and retain dirty state.
        apply(scalar(0x602,42));apply(body(9,9,9,1));auto count=packet(0x707);count.insert(count.end(),{1,0,4});apply(count);auto bad=item;bad.uid+=1;bad.slot=1;bad.id=2147483000;require(cache.inventory().load(bad,false,1),"Cannot seed failed composite save");
        db.begin();bool failed=false;try{core_store.save_in_transaction(cache);items.save_in_transaction(cache.inventory(),created.character_id);}catch(const std::runtime_error&){failed=true;}require(failed && db.transaction_active(),"Failed composite phase lost its caller transaction");db.rollback();
        rows=db.query("SELECT [Exp],Hair,Face,Size FROM "+prefix+"Chars WHERE CharID="+cid+"; SELECT [Count] FROM "+prefix+"CharItems WHERE CharID="+cid+";");require(rows[0].rows[0]==storage::Row{storage::Cell{"-1"},storage::Cell{"1"},storage::Cell{"2"},storage::Cell{"3"}} && rows[1].rows[0][0]==storage::Cell{"9"},"Inventory failure retained partial core/body updates");require(cache.body_changed() && cache.inventory().inventory()[0].save_type==2 && cache.inventory().inventory()[1].save_type==1,"Rollback acknowledged pending character changes");
        auto remove=packet(0x702);remove.insert(remove.end(),{1,1,255});apply(remove);db.begin();core_store.save_in_transaction(cache);items.save_in_transaction(cache.inventory(),created.character_id);db.commit();cache.core_save_success();cache.inventory().inventory_save_success();
        rows=db.query("SELECT [Exp],Hair,Face,Size FROM "+prefix+"Chars WHERE CharID="+cid+"; SELECT [Count] FROM "+prefix+"CharItems WHERE CharID="+cid+";");require(rows[0].rows[0]==storage::Row{storage::Cell{"42"},storage::Cell{"9"},storage::Cell{"9"},storage::Cell{"9"}} && rows[1].rows[0][0]==storage::Cell{"4"},"Pending composite changes did not survive retry");
        const auto reject_save=[&](){db.begin();bool failed=false;try{core_store.save_in_transaction(cache);}catch(const std::runtime_error&){failed=true;}db.rollback();require(failed,"Original SQL conversion failure was accepted");};
        apply(indexed(0x605,1,65535,2));reject_save();apply(indexed(0x605,1,32767,2));apply(indexed(0x60c,4,0x8000,4));reject_save();apply(indexed(0x60c,4,1,4));
        for(const auto invalid:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::max()}) {auto p=packet(0x608);for(float value:{invalid,0.0F,0.0F}) put(p,std::bit_cast<std::uint32_t>(value),4);apply(p);reject_save();}
        auto missing_core=initial;missing_core.character.id=INT32_MAX;CharacterReplica missing(missing_core,pool);require(missing.apply(body(1,2,3,0))==InventoryReplicaResult::applied,"Cannot mark missing character body");db.begin();failed=false;try{core_store.save_in_transaction(missing);}catch(const std::runtime_error&){failed=true;}db.rollback();require(failed && missing.body_changed(),"Missing character save failed to preserve negative procedure result/dirty body");
        std::cout<<"Original core/body SQL passed: signed widths, coordinate decimals/zero, shared money, composite commit/rollback/retry and conversion failures\n";
    }catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
