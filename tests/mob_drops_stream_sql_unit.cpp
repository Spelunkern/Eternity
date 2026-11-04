#include "eternity/game/mob_drops_stream.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
using namespace eternity;
void check(bool ok,const char* why) {if(!ok) throw std::runtime_error(why);}
constexpr std::array types{storage::SqlColumnType::short_integer,storage::SqlColumnType::tiny_integer,storage::SqlColumnType::short_integer,storage::SqlColumnType::long_integer};
struct Diagnostics:game::MobDropReloadDiagnostics {unsigned errors=0,rejections=0;void rejected(const game::MobDropRow&) override {++rejections;}void failed(game::MobDropReloadFailure) override {++errors;}};
int main() {try {
    const auto config=storage::SqlServerConfig::from_environment();storage::SqlServer db(config);unsigned controlled=0;
    const std::array<storage::Parameter,4> parameters{std::string("한글"),std::int64_t{INT64_MIN},double{1.25},std::monostate{}};
    const auto buffered=db.query("SELECT CAST(? AS nvarchar(20)),CAST(? AS bigint),CAST(? AS float),CAST(? AS int);",parameters);
    check(buffered.size()==1 && buffered[0].rows.size()==1,"Buffered parameter row missing");const auto& b=buffered[0].rows[0];
    check(b[0]==storage::Cell{"한글"} && b[1]==storage::Cell{std::to_string(INT64_MIN)} && b[2] && std::stod(*b[2])==1.25 && !b[3],"Unicode/bigint/double/NULL buffered contract changed");
    {
        auto cursor=db.cursor("SELECT CAST(-1 AS smallint),CAST(255 AS tinyint),CAST(-32768 AS smallint),CAST(-2147483648 AS int);",{},types);
        storage::TypedSqlRow row;check(cursor.next(row)==storage::SqlCursorRead::row && row==storage::TypedSqlRow({-1,255,-32768,INT32_MIN}),"Typed original signed/byte bits differ");
        check(cursor.next(row)==storage::SqlCursorRead::end,"Typed result did not end");
    }
    for(unsigned bad=0;bad<4;++bad) {
        std::string sql="SELECT m,o,g,r FROM (VALUES (0,'1','1','10','100'),(1,";
        for(unsigned col=0;col<4;++col) {if(col) sql+=',';sql+=col==bad?"'invalid'":col==2?"'30'":col==3?"'200'":"'1'";}
        sql+="),(2,'1','3','40','300')) t(n,m,o,g,r) ORDER BY n;";
        auto cursor=db.cursor(sql,{},types);game::MobDropDefinitions defs;defs.mobs.resize(4);std::int32_t changes=0;Diagnostics diagnostics;
        check(!game::reload_mob_drop_stream(defs,changes,cursor,diagnostics),"Real GetData failure was accepted");
        check(changes==2 && defs.mobs[1][1].grade==10 && defs.mobs[1][1].rate==100 && defs.mobs[1][2].grade==0 && defs.mobs[1][3].grade==0 && diagnostics.errors==1,"Real mid-row error lost prefix or published partial row");
        check(cursor.error()!=nullptr && !cursor.error()->sqlstate.empty() && db.last_query_sqlstate()==cursor.error()->sqlstate,"Real GetData diagnostics not retained");++controlled;
    }
    {
        auto cursor=db.cursor("SELECT CAST(1 AS smallint),CAST(1 AS tinyint),CAST(NULL AS smallint),CAST(10 AS int);",{},types);
        game::MobDropDefinitions defs;defs.mobs.resize(4);std::int32_t changes=0;Diagnostics diagnostics;
        check(!game::reload_mob_drop_stream(defs,changes,cursor,diagnostics) && changes==0 && diagnostics.errors==1,"NULL projection fabricated original buffer bytes");
    }
    {
        auto cursor=db.cursor("SELECT CAST(1 AS smallint),CAST(1 AS tinyint),CAST(10 AS smallint),CAST(100 AS int); SELECT 1;",{},types);
        storage::TypedSqlRow row;check(cursor.next(row)==storage::SqlCursorRead::row && cursor.next(row)==storage::SqlCursorRead::error && cursor.error()->sqlstate=="07005","Extra result set was silently ignored");
    }
    check(db.query("SELECT 1; SELECT 2;").size()==2,"Buffered multi-result compatibility changed");
    db.begin();
    {auto cursor=db.cursor("SELECT CAST(1 AS smallint),CAST(1 AS tinyint),CAST(10 AS smallint),CAST(100 AS int);",{},types);storage::TypedSqlRow row;check(cursor.next(row)==storage::SqlCursorRead::row && db.transaction_active(),"Cursor changed transaction ownership");}
    db.commit();check(!db.transaction_active(),"Read-only transaction commit contract changed");
    {
        auto facade=std::make_unique<storage::SqlServer>(config);
        const std::array<storage::Parameter,4> values{std::int64_t{2},std::int64_t{255},std::int64_t{-1},std::int64_t{INT32_MIN}};
        auto cursor=facade->cursor("SELECT CAST(? AS smallint),CAST(? AS tinyint),CAST(? AS smallint),CAST(? AS int);",values,types);
        facade.reset();storage::TypedSqlRow row;check(cursor.next(row)==storage::SqlCursorRead::row && row==storage::TypedSqlRow({2,255,-1,INT32_MIN}) && cursor.next(row)==storage::SqlCursorRead::end,"Real cursor did not retain statement/bindings/connection");
    }
    std::cout<<"Read-only real SQL: four controlled GetData failures preserve completed reload prefixes; signed/byte types, NULL, extra results, buffered Unicode/bigint/double parameters, read-only transaction and retained connection passed\n";
    check(controlled==4,"Missing real conversion failures");
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
