#include "eternity/storage/sql_server.hpp"
#include "eternity/auth/accounts.hpp"
#include "eternity/game/definitions.hpp"
#include "eternity/game/characters.hpp"
#include "eternity/game/stats.hpp"
#include "eternity/game/equipment.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <array>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    using namespace eternity::storage;
    try {
        SqlServer database(SqlServerConfig::from_environment());
        const auto result = database.query(
            "SET NOCOUNT ON; SELECT CAST(NULL AS varchar(8)) AS NullValue, '' AS EmptyValue, "
            "CAST(9223372036854775807 AS bigint) AS MaxId, "
            "REPLICATE(CAST('abcdef' AS varchar(max)),2000) AS LongValue, N'á漢字' AS UnicodeValue; "
            "SELECT 7 AS SecondResult;");
        require(result.size() == 2, "Multiple result sets not preserved");
        require(result[0].columns.size() == 5 && result[0].rows.size() == 1, "Result shape changed");
        const auto& row = result[0].rows[0];
        require(!row[0], "NULL must remain distinct from empty strings");
        require(row[1] && row[1]->empty(), "Empty string lost");
        require(row[2] == Cell{"9223372036854775807"}, "64-bit identifier lost precision");
        require(row[3] && row[3]->size() == 12000 && row[3]->substr(11994) == "abcdef", "Long cell truncated");
        require(row[4] == Cell{"á漢字"}, "Unicode text was not preserved as UTF-8");
        require(result[1].rows[0][0] == Cell{"7"}, "Second result lost");
        const std::array<Parameter, 4> parameters = {
            std::string{"O'Brien; SELECT 99; --"}, std::int64_t{9223372036854775807LL},
            std::monostate{}, 12.5};
        const auto bound = database.query(
            "SELECT CAST(? AS varchar(128)), CAST(? AS bigint), CAST(? AS varchar(8)), CAST(? AS float);", parameters);
        require(bound.size() == 1 && bound[0].rows.size() == 1, "Parameterized result shape changed");
        require(bound[0].rows[0][0] == Cell{"O'Brien; SELECT 99; --"}, "Bound string was interpreted as SQL");
        require(bound[0].rows[0][1] == Cell{"9223372036854775807"}, "Bound bigint lost precision");
        require(!bound[0].rows[0][2], "Bound NULL lost");
        require(bound[0].rows[0][3] == Cell{"12.5"}, "Bound floating-point value changed");
        const std::array<Parameter,1> unicode_parameter={std::string{"á漢字기본아이템😀"}};
        const auto unicode_bound=database.query("SELECT CAST(? AS nvarchar(128));",unicode_parameter);
        require(unicode_bound.size()==1 && unicode_bound[0].rows[0][0]==Cell{"á漢字기본아이템😀"},
                "Bound Unicode text did not preserve UTF-8/surrogate pairs");
        const std::array<Parameter,1> empty_parameter={std::string{}};
        const auto empty_bound=database.query("SELECT CAST(? AS nvarchar(128));",empty_parameter);
        require(empty_bound.size()==1 && empty_bound[0].rows[0][0]==Cell{""},"Empty Unicode parameter became SQL NULL");
        bool invalid_utf8=false;
        try {
            const std::array<Parameter,1> truncated_utf8={std::string{"\xe2\x82"}};
            (void)database.query("SELECT CAST(? AS nvarchar(128));",truncated_utf8);
        } catch(const std::exception&) {invalid_utf8=true;}
        require(invalid_utf8,"Truncated UTF-8 parameter was silently accepted");
        database.query("CREATE TABLE #EternityTest(Value int NOT NULL);");
        database.begin();
        bool nested_rejected = false;
        try { database.begin(); } catch (const std::logic_error&) { nested_rejected = true; }
        require(nested_rejected, "Nested transaction was silently admitted");
        database.query("INSERT INTO #EternityTest VALUES(1);");
        database.commit();
        database.begin();
        database.query("INSERT INTO #EternityTest VALUES(2);");
        database.rollback();
        const auto transaction = database.query("SELECT COUNT(*) FROM #EternityTest; DROP TABLE #EternityTest;");
        require(transaction.size() == 1 && transaction[0].rows[0][0] == Cell{"1"}, "Commit/rollback did not preserve state");
        bool failed = false;
        try { (void)database.query("SELECT * FROM dbo.EternityNonexistentTable;"); }
        catch (const std::runtime_error&) { failed = true; }
        require(failed, "SQL errors must propagate to the caller");
        const auto recovered = database.query("SELECT 1;");
        require(recovered.size() == 1 && recovered[0].rows[0][0] == Cell{"1"}, "Connection unusable after query error");
        const auto definitions = eternity::game::Definitions::load(database);
        require(definitions.catalogs.size() == 8 && !definitions.catalogs.at("Mobs").rows.empty(),
                "Original definition procedures did not load");
        require(definitions.catalogs.at("Mobs").columns.size() == 69, "Original mob definition contract changed");
        const auto statuses=eternity::game::StatusRules::load(definitions.catalogs.at("StatusDefs"));
        eternity::game::CharacterSummary fighter;fighter.level=1;fighter.family=0;fighter.grow=1;
        fighter.strength=14;fighter.dexterity=12;fighter.recovery=9;fighter.intelligence=8;fighter.wisdom=7;fighter.luck=15;
        const auto fighter_status=statuses.base(fighter);
        require(fighter_status.maximum.hp==250 && fighter_status.maximum.mp==85 && fighter_status.maximum.sp==195,
                "Restored status definitions disagree with original fighter resources");
        const auto experience=eternity::game::Experience::load(definitions.catalogs.at("ExpDefs"));
        require(experience.bounds(1,1).previous==0 && experience.bounds(1,1).next>0,"Restored experience curve did not load");
        const auto items=eternity::game::ItemDefinitions::load(definitions.catalogs.at("Items"));
        require(items.find(5,1) && items.find(30,1),"Restored weapon/gem definitions did not load");
        eternity::game::Characters characters(database,255,std::chrono::sys_days{std::chrono::year{2024}/1/1});
        const auto empty_roster=characters.load(2147483000);
        require(empty_roster.new_account && empty_roster.country==2 && empty_roster.max_grow==2,
                "Runtime login cannot read the original character/country contracts");
        // Synthetic account and all login log writes remain inside a rolled-back transaction.
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count() % 1000000000;
        const std::string username = "et_it_" + std::to_string(stamp);
        database.begin();
        try {
            const std::array<Parameter, 1> account = {username};
            const auto created = database.query(
                "DECLARE @Name varchar(18)=?; "
                "IF EXISTS (SELECT 1 FROM PS_UserData.dbo.Users_Master WHERE UserID=@Name) "
                "THROW 50001, 'Synthetic test account already exists', 1; "
                "DECLARE @Id int=(SELECT ISNULL(MAX(UserUID),0)+1 FROM PS_UserData.dbo.Users_Master); "
                "INSERT INTO PS_UserData.dbo.Users_Master "
                "(UserUID,UserID,Pw,Admin,AdminLevel,UseQueue,Status,[Leave],UserType,Enpassword) "
                "VALUES(@Id,@Name,'testpass',0,0,0,0,0,'N',master.dbo.fn_md5('testpass')); "
                "SELECT @Id;", account);
            require(created.size() == 1, "Synthetic test account was not created");
            eternity::auth::Accounts accounts(database);
            const auto accepted = accounts.login(username, "testpass", 9000000000001LL, "127.0.0.1");
            require(accepted.status == 0 && std::to_string(accepted.user_id) == *created[0].rows[0][0],
                    "Valid account rejected by original login procedure");
            const auto rejected = accounts.login(username, "wrong", 9000000000002LL, "127.0.0.1");
            require(rejected.status == -1, "Invalid password did not preserve original status");
            const auto unknown = accounts.login("et_missing_000001", "wrong", 9000000000003LL, "127.0.0.1");
            require(unknown.status == -3, "Missing account did not preserve original status");
            database.rollback();
            const auto remaining = database.query("SELECT COUNT(*) FROM PS_UserData.dbo.Users_Master WHERE UserID=?;", account);
            require(remaining[0].rows[0][0] == Cell{"0"}, "Synthetic account was not rolled back");
        } catch (...) {
            try { database.rollback(); } catch (...) {}
            throw;
        }
        std::cout << "SQL Server integration passed: NULL, UTF-8, bigint, long cells, result sets, rollback and error recovery.\n";
        std::cout << "Original definition loaders and transactional account authentication passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
