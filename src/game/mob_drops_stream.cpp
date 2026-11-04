#include "eternity/game/mob_drops_stream.hpp"
namespace eternity::game {
namespace {
class OdbcRows final:public MobDropReloadCursor {
public:
    explicit OdbcRows(storage::TypedSqlCursor& cursor):cursor_(cursor) {}
    MobDropReloadRead next(MobDropRow& output) override {
        storage::TypedSqlRow row;
        const auto status=cursor_.next(row);
        if(status==storage::SqlCursorRead::end) return MobDropReloadRead::end;
        if(status==storage::SqlCursorRead::error) return MobDropReloadRead::failed;
        if(row.size()!=4 || !row[0] || !row[1] || !row[2] || !row[3]) return MobDropReloadRead::failed;
        output={static_cast<std::uint16_t>(*row[0]),static_cast<std::uint8_t>(*row[1]),static_cast<std::uint16_t>(*row[2]),static_cast<std::uint32_t>(*row[3])};
        return MobDropReloadRead::row;
    }
private:
    storage::TypedSqlCursor& cursor_;
};
}
bool reload_mob_drop_stream(MobDropDefinitions& definitions,std::int32_t& changes,storage::TypedSqlCursor& cursor,MobDropReloadDiagnostics& diagnostics) {
    OdbcRows rows(cursor);return reload_mob_drop_rows(definitions,changes,rows,diagnostics);
}
}
