#pragma once
#include <array>
#include <cstdint>
#include <exception>
#include <stdexcept>

namespace eternity::game {
struct WorldDbAgentSaveResult {
    bool due=false;
    std::array<bool,3> saved{};
    std::array<std::exception_ptr,3> errors{};
    bool success()const noexcept{return due && saved[0] && saved[1] && saved[2];}
};
// ps_dbAgent.exe CUserSaveThread::Run 0041d5b0 and CUserMgr::SaveAll
// 0041d2f0: kill counts, obelisks, then guilds, even after a failure.
// The caller serializes the real owners and runs this on a DB worker.
template<class KillOwner,class ObeliskOwner,class GuildOwner> class WorldDbAgentSave final {
public:
    WorldDbAgentSave(KillOwner& kills,ObeliskOwner& obelisks,GuildOwner& guilds)
        :kills_(kills),obelisks_(obelisks),guilds_(guilds) {}
    WorldDbAgentSaveResult periodic(std::uint32_t now) {
        if(!kills_.periodic_due(now)) {WorldDbAgentSaveResult result;return result;}
        return run([&]{return kills_.save_periodic(now);});
    }
    // Called after the real connection-specific user SaveAll traversal posts
    // its DB jobs; completion of those jobs is a separate lifecycle concern.
    WorldDbAgentSaveResult save_all(){return run([&]{return kills_.save_all();});}
private:
    template<class Save> WorldDbAgentSaveResult run(Save save_kills) {
        WorldDbAgentSaveResult result;result.due=true;
        invoke(result,0,save_kills);
        invoke(result,1,[&]{return obelisks_.save();});
        invoke(result,2,[&]{return guilds_.save();});
        return result;
    }
    template<class Save> static void invoke(WorldDbAgentSaveResult& result,std::size_t i,Save save) {
        try {result.saved[i]=save();}catch(...) {result.errors[i]=std::current_exception();}
    }
    KillOwner& kills_;ObeliskOwner& obelisks_;GuildOwner& guilds_;
};
}
