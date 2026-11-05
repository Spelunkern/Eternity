#pragma once
#include "eternity/game/vertical_social_replica.hpp"

namespace eternity::game {
// DBFriend::SaveFriend 004261e0 and SaveBlock 004266e0 in ps_dbAgent.exe.
// Caller serializes mutations and saves for the lifetime of each transaction.
// The SQL connection must be dedicated to this operation while it is active.
template<class Database> class VerticalSocialSave {
public:
    explicit VerticalSocialSave(Database& db,std::string prefix="PS_GameData.dbo.")
        :db_(db),prefix_(std::move(prefix)) {}

    bool friends(VerticalSocialReplica& replica,std::uint32_t character) {
        return save(replica,character,true);
    }
    bool blocks(VerticalSocialReplica& replica,std::uint32_t character) {
        return save(replica,character,false);
    }
private:
    static std::int64_t signed_id(std::uint32_t id) {
        return id<=0x7fffffffU ? id : static_cast<std::int64_t>(id)-0x100000000LL;
    }
    bool query(std::string_view procedure,std::initializer_list<storage::Parameter> args) {
        try {
            std::string sql="EXEC "+prefix_+std::string(procedure)+" ";
            for(std::size_t i=0;i<args.size();++i)sql+=(i?",?":"?");
            sql+=';';db_.query(sql,std::span<const storage::Parameter>(args.begin(),args.size()));return true;
        } catch(const storage::SqlError&) {return false;}
    }
    bool save(VerticalSocialReplica& replica,std::uint32_t character,bool friends) {
        if(db_.transaction_active())return false;
        try {db_.begin();} catch(const storage::SqlError&) {return false;}
        try {
            const auto owner=signed_id(character);
            const auto& deleted=friends?replica.deleted_friends():replica.deleted_blocks();
            for(auto id:deleted)if(!query(friends?"usp_Delete_FriendChar_E":"usp_Delete_BanChar_E",{owner,signed_id(id)})) {
                db_.rollback();return false;
            }
            if(friends) {for(const auto& row:replica.friends())if(row.id) {
                if(row.save_type==1 && !query("usp_Create_FriendChar_E",{owner,signed_id(row.id),row.name,std::int64_t(row.family),std::int64_t(row.grow),std::int64_t(row.job)})) {db_.rollback();return false;}
                if(row.save_type==2)(void)query("usp_Save_FriendMemo_E",{owner,signed_id(row.id),row.memo});
            }} else {for(const auto& row:replica.blocked())if(row.id) {
                if(row.save_type==1 && !query("usp_Create_BanChar_E",{owner,signed_id(row.id),row.name})) {db_.rollback();return false;}
                if(row.save_type==2)(void)query("usp_Save_BandMemo_E",{owner,signed_id(row.id),row.memo});
            }}
            db_.commit();
        } catch(...) {
            if(db_.transaction_active())try {db_.rollback();}catch(...) {}
            return false;
        }
        if(friends)replica.acknowledge_friends();else replica.acknowledge_blocks();
        return true;
    }
    Database& db_;std::string prefix_;
};
using VerticalSocialSqlSave=VerticalSocialSave<storage::SqlServer>;
}
