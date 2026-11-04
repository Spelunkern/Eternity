#pragma once
#include "eternity/net/framing.hpp"
#include <cstdint>
#include <mutex>
namespace eternity::game {
// Required actual DB-agent connection/queue. The binding must outlive Actor and
// callbacks, and copy/retain packet bytes before returning. Send failure must be
// handled by the transport; original removal does not wait for acknowledgment.
class ActorEffectDatabase {
public:
    virtual ~ActorEffectDatabase()=default;
    virtual void send_effect_database(const net::Packet&) noexcept=0;
};
class SkillReplica;
enum class InventoryReplicaResult;
class ActorEffectDatabaseDiagnostics {
public:
    virtual ~ActorEffectDatabaseDiagnostics()=default;
    virtual void rejected_effect_database(const net::Packet&,InventoryReplicaResult) noexcept=0;
    virtual void failed_effect_database(const net::Packet&) noexcept=0;
};
// Explicit cohosted DB-agent route, not a second cache or an outbox. Bind the
// existing CharacterReplica::skills() and SAME recursive cache mutex as saves.
// Remote deployments instead supply their actual connection/queue implementation.
// Referenced receiver/mutex/diagnostics must outlive the Actor and this adapter.
class ActorEffectReplicaDelivery final:public ActorEffectDatabase {
public:
    ActorEffectReplicaDelivery(SkillReplica& replica,std::recursive_mutex& mutex,ActorEffectDatabaseDiagnostics& diagnostics)
        :replica_(replica),mutex_(mutex),diagnostics_(diagnostics) {}
    void send_effect_database(const net::Packet&) noexcept override;
private:
    SkillReplica& replica_;std::recursive_mutex& mutex_;ActorEffectDatabaseDiagnostics& diagnostics_;
};
// ps_game.exe SendSkillRemove 00482a70: 0902, billing/account DWORD, skill WORD,
// level BYTE. Runtime instance ID belongs only to the separate client 050e.
net::Packet effect_database_removed_packet(std::uint32_t account,std::uint16_t skill,std::uint8_t level);
}
