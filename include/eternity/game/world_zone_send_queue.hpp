#pragma once
#include "eternity/net/framing.hpp"
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <stdexcept>

namespace eternity::game {
// CZone::WorldSendAllPush / WorldSendAllProcessing 00420320. The payload
// owner survives every queued and in-flight delivery; callers serialize the
// queue on the same executor as its zone and real transport.
enum class WorldZoneSendKind : std::uint8_t {all=1,country=2,priority=3,priority_country=4};
struct WorldZoneSendMessage {
    WorldZoneSendKind kind;std::shared_ptr<const net::Packet> payload;
    std::uint8_t country=0;std::uint32_t source=0,destination=0,priority=0;
};
inline void validate_world_zone_send(const WorldZoneSendMessage& message) {
    if(!message.payload || message.payload->empty() || static_cast<unsigned>(message.kind)<1 || static_cast<unsigned>(message.kind)>4)
        throw std::invalid_argument("World zone send requires an actual payload and known route");
}
class WorldZoneSendQueue final {
public:
    void push(WorldZoneSendMessage message) {
        validate_world_zone_send(message);
        queue_.push_back(std::move(message));
    }
    // Pop before delivery as the original does. New enqueues are included;
    // a throwing delivery stops the pass and leaves later entries queued.
    void drain(const std::function<void(const WorldZoneSendMessage&)>& deliver) {
        if(!deliver)throw std::invalid_argument("World zone send requires actual delivery");
        if(draining_)throw std::logic_error("Reentrant world zone send drain");
        struct Guard {bool& value;explicit Guard(bool& v):value(v){value=true;}~Guard(){value=false;}} guard(draining_);
        while(!queue_.empty()) {auto message=std::move(queue_.front());queue_.pop_front();deliver(message);}
    }
    std::size_t size() const noexcept{return queue_.size();}
private:
    std::deque<WorldZoneSendMessage> queue_;bool draining_=false;
};
}
