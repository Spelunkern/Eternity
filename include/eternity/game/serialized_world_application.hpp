#pragma once
#include "eternity/net/world_server.hpp"
#include <asio/any_io_executor.hpp>
#include <asio/post.hpp>
#include <asio/strand.hpp>
#include <future>
#include <memory>
#include <utility>

namespace eternity::game {
// WorldServer owns each wrapper on a connection worker. The shared strand owns
// every access to the actual applications and world state. The supplied executor
// must keep running until all WorldServer close calls have returned; do not stop
// or join it from one of these callbacks. Callbacks must not block on work
// posted to the same strand or to its underlying io_context. This adapter starts
// no threads.
class SerializedWorldApplication final:public net::WorldApplication {
public:
    using Strand=asio::strand<asio::any_io_executor>;
    SerializedWorldApplication(Strand strand,std::function<std::unique_ptr<net::WorldApplication>()> factory)
        :strand_(std::move(strand)) {
        if(!factory) throw std::invalid_argument("Missing world application factory");
        invoke([&]{application_=factory();if(!application_)throw std::runtime_error("World application factory returned null");});
    }
    void open() override {invoke([&]{application_->open();});}
    void receive(const net::Packet& packet) override {invoke([&]{application_->receive(packet);});}
    void receive_nprotect(const net::Packet& packet) override {invoke([&]{application_->receive_nprotect(packet);});}
    void close() override {close_with_reason({});}
    void close_with_reason(std::optional<std::uint32_t> reason) override {
        invoke([&]{if(!application_)return;auto owner=std::move(application_);owner->close_with_reason(reason);});
    }
private:
    template<class F> void invoke(F&& action) {
        if(strand_.running_in_this_thread()) {action();return;}
        auto task=std::make_shared<std::packaged_task<void()>>(std::forward<F>(action));
        auto result=task->get_future();asio::post(strand_,[task]{(*task)();});result.get();
    }
    Strand strand_;std::unique_ptr<net::WorldApplication> application_;
};
// Copy this factory for every WorldServer connection; each wrapper shares the
// same strand and builds/destroys its application there.
inline net::WorldApplicationFactory serialized_world_factory(SerializedWorldApplication::Strand strand,net::WorldApplicationFactory factory) {
    if(!factory) throw std::invalid_argument("Missing world application factory");
    return [strand=std::move(strand),factory=std::move(factory)](const session::LoginSession& account,net::WorldChannel channel) {
        return std::make_unique<SerializedWorldApplication>(strand,[&]{return factory(account,std::move(channel));});
    };
}
}
