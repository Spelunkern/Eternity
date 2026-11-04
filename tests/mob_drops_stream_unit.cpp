#include "eternity/game/mob_drops_stream.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace eternity;
namespace {
void check(bool ok,const std::string& why) {if(!ok) throw std::runtime_error(why);}
struct Driver {
    std::vector<game::MobDropRow> rows;int index=-1,fail=-1,fetch_fail=-1;unsigned column=1,null_column=0;
    bool query_fail=false,more_fail=false,warning=false;unsigned allocations=0,frees=0,disconnects=0,reads=0,rollbacks=0;
    std::vector<SQLSMALLINT> free_types;
    SQLRETURN fetch_status=SQL_SUCCESS,data_status=SQL_SUCCESS;
    std::vector<std::pair<SQLSMALLINT,SQLSMALLINT>> bindings;
} driver;
std::uintptr_t next_handle=256;
constexpr std::array types{storage::SqlColumnType::short_integer,storage::SqlColumnType::tiny_integer,storage::SqlColumnType::short_integer,storage::SqlColumnType::long_integer};
struct Diagnostics:game::MobDropReloadDiagnostics {std::string events;void rejected(const game::MobDropRow&) override {events+='X';}void failed(game::MobDropReloadFailure) override {events+='E';}};
std::string unhex(const std::string& value) {std::string bytes;for(std::size_t i=0;i<value.size();i+=2) bytes+=static_cast<char>(std::stoul(value.substr(i,2),nullptr,16));return bytes;}
void restore(game::MobDropDefinitions& defs,const std::string& hex) {
    const auto data=unhex(hex);check(data.size()==240,"Bad original table");defs.mobs.resize(4);std::size_t at=0;
    for(auto& mob:defs.mobs) for(auto& slot:mob) {std::uint16_t grade;std::uint32_t rate;std::memcpy(&grade,data.data()+at,2);std::memcpy(&rate,data.data()+at+2,4);slot={grade,rate};at+=6;}
}
std::string snapshot(const game::MobDropDefinitions& defs) {std::string data;for(const auto& mob:defs.mobs) for(const auto& slot:mob) {for(unsigned i=0;i<2;++i) data+=static_cast<char>(slot.grade>>(i*8));for(unsigned i=0;i<4;++i) data+=static_cast<char>(slot.rate>>(i*8));}return data;}
}
// Controlled ODBC boundary. Actual SqlServer binding, statement/connection lifetime,
// typed cursor and existing reload engine execute, not replacement SQL methods.
extern "C" {
SQLRETURN SQL_API SQLAllocHandle(SQLSMALLINT,SQLHANDLE,SQLHANDLE* output) {*output=reinterpret_cast<SQLHANDLE>(++next_handle);++driver.allocations;return SQL_SUCCESS;}
SQLRETURN SQL_API SQLFreeHandle(SQLSMALLINT type,SQLHANDLE) {driver.free_types.push_back(type);++driver.frees;return SQL_SUCCESS;}
SQLRETURN SQL_API SQLSetEnvAttr(SQLHENV,SQLINTEGER,SQLPOINTER,SQLINTEGER) {return SQL_SUCCESS;}
SQLRETURN SQL_API SQLSetConnectAttr(SQLHDBC,SQLINTEGER,SQLPOINTER,SQLINTEGER) {return SQL_SUCCESS;}
SQLRETURN SQL_API SQLSetStmtAttr(SQLHSTMT,SQLINTEGER,SQLPOINTER,SQLINTEGER) {return SQL_SUCCESS;}
SQLRETURN SQL_API SQLDriverConnect(SQLHDBC,SQLHWND,SQLCHAR*,SQLSMALLINT,SQLCHAR*,SQLSMALLINT,SQLSMALLINT*,SQLUSMALLINT) {return SQL_SUCCESS;}
SQLRETURN SQL_API SQLDisconnect(SQLHDBC) {++driver.disconnects;return SQL_SUCCESS;}
SQLRETURN SQL_API SQLEndTran(SQLSMALLINT,SQLHANDLE,SQLSMALLINT action) {if(action==SQL_ROLLBACK) ++driver.rollbacks;return SQL_SUCCESS;}
SQLRETURN SQL_API SQLPrepare(SQLHSTMT,SQLCHAR*,SQLINTEGER) {return SQL_SUCCESS;}
SQLRETURN SQL_API SQLBindParameter(SQLHSTMT,SQLUSMALLINT,SQLSMALLINT,SQLSMALLINT c,SQLSMALLINT sql,SQLULEN,SQLSMALLINT,SQLPOINTER,SQLLEN,SQLLEN*) {driver.bindings.emplace_back(c,sql);return SQL_SUCCESS;}
SQLRETURN SQL_API SQLExecute(SQLHSTMT) {driver.index=-1;return driver.query_fail?SQL_ERROR:SQL_SUCCESS;}
SQLRETURN SQL_API SQLExecDirect(SQLHSTMT,SQLCHAR*,SQLINTEGER) {driver.index=-1;return driver.query_fail?SQL_ERROR:SQL_SUCCESS;}
SQLRETURN SQL_API SQLNumResultCols(SQLHSTMT,SQLSMALLINT* count) {*count=4;return SQL_SUCCESS;}
SQLRETURN SQL_API SQLDescribeCol(SQLHSTMT,SQLUSMALLINT,SQLCHAR* name,SQLSMALLINT,SQLSMALLINT* size,SQLSMALLINT* type,SQLULEN* precision,SQLSMALLINT* decimals,SQLSMALLINT* nullable) {std::memcpy(name,"cell",5);*size=4;*type=SQL_INTEGER;*precision=10;*decimals=0;*nullable=1;return SQL_SUCCESS;}
SQLRETURN SQL_API SQLFetch(SQLHSTMT) {
    ++driver.index;if(driver.fetch_status!=SQL_SUCCESS) return driver.fetch_status;if(driver.index==driver.fetch_fail) return SQL_ERROR;
    return static_cast<std::size_t>(driver.index)>=driver.rows.size()?SQL_NO_DATA:driver.warning?SQL_SUCCESS_WITH_INFO:SQL_SUCCESS;
}
SQLRETURN SQL_API SQLMoreResults(SQLHSTMT) {return driver.more_fail?SQL_ERROR:SQL_NO_DATA;}
SQLRETURN SQL_API SQLGetDiagRec(SQLSMALLINT,SQLHANDLE,SQLSMALLINT index,SQLCHAR* state,SQLINTEGER* native,SQLCHAR* message,SQLSMALLINT,SQLSMALLINT* size) {
    if(index!=1) return SQL_NO_DATA;std::memcpy(state,"HY000",6);std::memcpy(message,"controlled ODBC failure",24);*size=23;*native=60000;return SQL_SUCCESS;
}
SQLRETURN SQL_API SQLGetData(SQLHSTMT,SQLUSMALLINT column,SQLSMALLINT type,SQLPOINTER value,SQLLEN width,SQLLEN* indicator) {
    ++driver.reads;if(driver.data_status!=SQL_SUCCESS && driver.data_status!=SQL_SUCCESS_WITH_INFO) return driver.data_status;if(driver.index==driver.fail && column==driver.column) return SQL_ERROR;
    if(column==driver.null_column) {*indicator=SQL_NULL_DATA;return SQL_SUCCESS;}
    const auto& row=driver.rows.at(static_cast<std::size_t>(driver.index));const std::array<std::uint32_t,4> values{row.mob,row.order,row.grade,row.rate};
    if(type==SQL_C_CHAR) {const auto text=std::to_string(values[column-1]);std::memcpy(value,text.c_str(),text.size()+1);*indicator=static_cast<SQLLEN>(text.size());return SQL_SUCCESS;}
    const std::array<SQLLEN,4> widths{2,1,2,4};const std::array<SQLSMALLINT,4> expected{SQL_C_SHORT,SQL_C_TINYINT,SQL_C_SHORT,SQL_C_LONG};
    check(width==widths[column-1] && type==expected[column-1],"Original GetData width/type differs");
    const auto raw=values[column-1];std::memcpy(value,&raw,static_cast<std::size_t>(width));*indicator=width;return driver.warning?SQL_SUCCESS_WITH_INFO:driver.data_status;
}
}
int main(int argc,char** argv) {try {
    check(argc==3,"Expected original reload and ODBC normalization fixtures");std::ifstream input(argv[1]);check(input.good(),"Missing original reload fixture");unsigned cases=0;
    for(std::string line;std::getline(input,line);) {
        if(line.empty() || line[0]=='#' || line[0]=='L') continue;std::istringstream row(line);char tag;unsigned scenario,request,column,query_fail,count,success;int fail;std::uint32_t initial_counter,final_counter;std::string initial,expected,events;
        row>>tag>>scenario>>request>>initial_counter>>fail>>column>>query_fail>>count>>success>>final_counter>>initial>>expected>>events;check(tag=='S' && !row.fail(),"Bad original scenario");driver={};driver.fail=fail;driver.column=column;driver.query_fail=query_fail;
        for(unsigned i=0;i<count;++i) {std::getline(input,line);std::istringstream values(line);game::MobDropRow value;unsigned order;values>>tag>>value.mob>>order>>value.grade>>value.rate;value.order=static_cast<std::uint8_t>(order);driver.rows.push_back(value);}
        {
            storage::SqlServer db({});game::MobDropDefinitions defs;restore(defs,initial);const auto* original=defs.mobs.data();auto changes=std::bit_cast<std::int32_t>(initial_counter);Diagnostics diagnostics;
            const bool result=game::reload_mob_drop_sql(defs,static_cast<std::uint16_t>(request),changes,db,diagnostics);
            check(result==(success!=0) && std::bit_cast<std::uint32_t>(changes)==final_counter && snapshot(defs)==unhex(expected),"Original complete table/counter differs through actual ODBC cursor: "+std::to_string(scenario));
            check((diagnostics.events.empty()?"-":diagnostics.events)==events && defs.mobs.data()==original,"Original diagnostics/stable binding differ");
            if(!success) check(db.last_query_sqlstate()=="HY000" && db.last_query_native_error()==60000,"Structured connection diagnostics lost");
        }
        check(driver.allocations==driver.frees && driver.disconnects==1,"ODBC handle leak");++cases;
    }
    for(unsigned fail_column=0;fail_column<=4;++fail_column) {
        driver={};driver.rows={{1,1,10,20},{1,2,30,40}};driver.fail=fail_column?1:-1;driver.column=fail_column;driver.fetch_fail=fail_column?-1:1;
        storage::SqlServer db({});auto cursor=db.cursor("SELECT projection",{},types);storage::TypedSqlRow output{99};
        check(cursor.next(output)==storage::SqlCursorRead::row && output[2]==10,"First typed row missing");const auto prefix=output;
        check(cursor.next(output)==storage::SqlCursorRead::error && output==prefix && cursor.next(output)==storage::SqlCursorRead::error && cursor.error()->native_error==60000,"Partial row/fetch failure was published or not sticky");
    }
    driver={};driver.rows={{1,1,10,20},{1,2,30,40}};
    {
        auto db=std::make_unique<storage::SqlServer>(storage::SqlServerConfig{});db->begin();auto cursor=db->cursor("SELECT lifetime",{},types);
        bool blocked=false;try {db->query("SELECT blocked");}catch(const std::logic_error&) {blocked=true;}check(blocked,"Connection allowed overlapping statement");
        db.reset();check(driver.disconnects==0 && driver.rollbacks==0,"Cursor lost retained connection or transaction");storage::TypedSqlRow row;
        check(cursor.next(row)==storage::SqlCursorRead::row,"Retained cursor could not read after facade destruction");
        auto moved=std::move(cursor);check(moved.next(row)==storage::SqlCursorRead::row,"Moved retained cursor failed");
        check(moved.next(row)==storage::SqlCursorRead::end && moved.next(row)==storage::SqlCursorRead::end,"End not sticky");
    }
    check(driver.disconnects==1 && driver.allocations==driver.frees && driver.rollbacks==1 && driver.free_types==std::vector<SQLSMALLINT>{SQL_HANDLE_STMT,SQL_HANDLE_DBC,SQL_HANDLE_ENV},"Retained owner cleanup order/transaction differs");
    driver={};driver.rows={{1,1,10,20}};driver.warning=true;
    {storage::SqlServer db({});auto cursor=db.cursor("SELECT warning",{},types);storage::TypedSqlRow row;check(cursor.next(row)==storage::SqlCursorRead::row && row[2]==10,"Original success-with-info normalization changed");}
    driver={};driver.rows={{1,1,10,20}};driver.null_column=3;
    {storage::SqlServer db({});auto cursor=db.cursor("SELECT NULL",{},types);storage::TypedSqlRow row;check(cursor.next(row)==storage::SqlCursorRead::row && !row[2],"Typed NULL was fabricated");}
    driver={};driver.rows={{1,1,10,20}};driver.more_fail=true;
    {storage::SqlServer db({});auto cursor=db.cursor("SELECT trailing error",{},types);storage::TypedSqlRow row;check(cursor.next(row)==storage::SqlCursorRead::row && cursor.next(row)==storage::SqlCursorRead::error,"Trailing result error lost");}
    driver={};driver.rows={{1,1,10,20}};
    {storage::SqlServer db({});const std::array<storage::Parameter,4> parameters{std::string("한글"),std::int64_t{-9},double{1.25},std::monostate{}};const auto sets=db.query("SELECT buffered parameters",parameters);check(sets.size()==1 && sets[0].rows.size()==1 && sets[0].rows[0][2]==storage::Cell{"10"},"Buffered API changed");check(driver.bindings==std::vector<std::pair<SQLSMALLINT,SQLSMALLINT>>{{SQL_C_WCHAR,SQL_WVARCHAR},{SQL_C_SBIGINT,SQL_BIGINT},{SQL_C_DOUBLE,SQL_DOUBLE},{SQL_C_CHAR,SQL_VARCHAR}},"Parameter contracts changed");}
    std::ifstream normalization(argv[2]);check(normalization.good(),"Missing original ODBC wrapper fixture");unsigned wrappers=0;
    for(std::string line;std::getline(normalization,line);) {
        if(line.empty() || line[0]=='#') continue;std::istringstream input(line);char stage;int raw,normalized,stored,diagnostics;input>>stage>>raw>>normalized>>stored>>diagnostics;check(!input.fail(),"Bad wrapper fixture");
        driver={};driver.rows={{1,1,10,20}};
        if(stage=='F') driver.fetch_status=static_cast<SQLRETURN>(raw);else driver.data_status=static_cast<SQLRETURN>(raw);
        storage::SqlServer db({});auto cursor=db.cursor("SELECT original normalization",{},types);storage::TypedSqlRow output{99};
        const auto expected=normalized==0?storage::SqlCursorRead::row:stage=='F' && normalized==100?storage::SqlCursorRead::end:storage::SqlCursorRead::error;
        check(cursor.next(output)==expected && stored==raw,"Original SDatabase wrapper status differs");++wrappers;
    }
    check(wrappers==12,"Missing original wrapper normalization cases");
    check(cases==512,"Incomplete original comparison");std::cout<<cases<<" original reload traces / 12 original wrapper outcomes through actual SqlServer typed cursor; all column/fetch failures, retained/moved lifetime, NULL, result error and buffered bindings passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
