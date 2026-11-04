#include "eternity/net/world_server.hpp"
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <deque>
#include <limits>
#include <openssl/rand.h>
#include <unordered_map>

namespace eternity::net {
using asio::ip::tcp;
namespace {
std::uint64_t world_now_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
std::shared_ptr<const WorldXor> random_world_xor() {
    std::array<std::uint8_t,16> seed{};
    if(RAND_bytes(seed.data(),static_cast<int>(seed.size()))!=1) throw std::runtime_error("Generate world XOR seed failed");
    return std::make_shared<WorldXor>(seed);
}
}
struct WorldServer::Impl : std::enable_shared_from_this<WorldServer::Impl> {
    struct Connection;
    asio::strand<asio::io_context::executor_type> strand;
    tcp::acceptor acceptor;
    WorldServerOptions options;
    std::shared_ptr<session::Registry> sessions;
    std::shared_ptr<const WorldXor> world_xor = random_world_xor();
    WorldApplicationFactory factory;
    std::atomic_uint64_t application_failures{0};
    asio::thread_pool database;
    std::unordered_map<std::uint64_t,std::shared_ptr<Connection>> connections;
    std::uint64_t next_id = 1;
    std::uint16_t bound_port = 0;
    bool running = false,started = false,stopped = false;
    Impl(asio::io_context& io,WorldServerOptions config,std::shared_ptr<session::Registry> registry,
        WorldApplicationFactory application_factory)
        : strand(asio::make_strand(io)),acceptor(strand),options(std::move(config)),sessions(std::move(registry)),
          factory(std::move(application_factory)),database(options.database_workers) {
        if(!sessions || !factory || options.realm>=254 || options.maximum_connections==0 || options.database_workers==0 || options.maximum_queued_bytes<4)
            throw std::invalid_argument("World server requires session registry, application factory and bounded worker limits");
        const auto address=asio::ip::make_address(options.address);
        if(!address.is_v4()) throw std::invalid_argument("Original game endpoints use IPv4");
        const tcp::endpoint endpoint(address,options.port);
        acceptor.open(endpoint.protocol());acceptor.set_option(tcp::acceptor::reuse_address(true));
        acceptor.bind(endpoint);acceptor.listen();bound_port=acceptor.local_endpoint().port();
    }
    void accept();
    void stop();
};
struct WorldServer::Impl::Connection : std::enable_shared_from_this<Connection> {
    tcp::socket socket;
    std::weak_ptr<Impl> owner;
    std::uint64_t id;
    std::unique_ptr<WorldProtocol> protocol;
    FrameDecoder decoder;
    std::array<std::uint8_t,8192> input{};
    std::deque<std::shared_ptr<Packet>> output;
    std::deque<Packet> pending;
    std::deque<Packet> received;
    asio::strand<asio::thread_pool::executor_type> worker;
    asio::executor_work_guard<asio::any_io_executor> lifetime;
    std::unique_ptr<WorldApplication> application; // Worker strand only.
    std::size_t maximum_queued_bytes;
    std::size_t queued_bytes = 0;
    std::atomic_bool closed{false};
    bool working = false,loaded = false,input_paused = false;
    Connection(tcp::socket peer,const std::shared_ptr<Impl>& server,std::uint64_t identity)
        : socket(std::move(peer)),owner(server),id(identity),worker(asio::make_strand(server->database)),
          lifetime(socket.get_executor()),maximum_queued_bytes(server->options.maximum_queued_bytes) {}
    void start() {
        const auto server=owner.lock();if(!server) return close();
        const auto weak=weak_from_this();const auto executor=socket.get_executor();
        protocol=std::make_unique<WorldProtocol>(server->sessions,server->options.realm,id,server->world_xor,[weak,executor] {
            if(weak.expired()) return;
            asio::dispatch(executor,[weak] {if(auto connection=weak.lock()) connection->close();});
        });
        read();
    }
    void close() {
        if(closed.exchange(true)) return;
        if(protocol) protocol->close();
        asio::error_code ignored;socket.cancel(ignored);socket.shutdown(tcp::socket::shutdown_both,ignored);socket.close(ignored);
        if(auto server=owner.lock()) server->connections.erase(id);
        output.clear();pending.clear();received.clear();queued_bytes=0;
        asio::post(worker,[self=shared_from_this()] {
            if(self->application) {
                try {self->application->close();} catch(...) {self->application_failed();}
                self->application.reset();
            }
            self->lifetime.reset();
        });
    }
    void send(Packet payload) {
        if(closed) return;
        auto packet=std::make_shared<Packet>(frame(payload));
        if(packet->size()>maximum_queued_bytes-queued_bytes) return close();
        const bool idle=output.empty();queued_bytes+=packet->size();output.push_back(std::move(packet));if(idle) write();
    }
    void write() {
        if(closed || output.empty()) return;
        auto bytes=output.front();
        asio::async_write(socket,asio::buffer(*bytes),[self=shared_from_this(),bytes](const asio::error_code& error,std::size_t) {
            if(self->closed) return;
            if(error) return self->close();
            self->queued_bytes-=bytes->size();self->output.pop_front();self->write();
        });
    }
    void application_failed() {
        if(auto server=owner.lock()) ++server->application_failures;
    }
    WorldChannel channel() {
        const auto weak=weak_from_this();const auto executor=socket.get_executor();
        WorldChannel result;
        result.send=[weak,executor](Packet packet) {
            asio::post(executor,[weak,packet=std::move(packet)]() mutable {
                const auto self=weak.lock();if(!self || self->closed) return;
                try {self->send(self->protocol->outgoing(std::move(packet)));}
                catch(...) {self->close();}
            });
        };
        result.enter_world=[weak,executor] {
            asio::post(executor,[weak] {
                const auto self=weak.lock();if(!self || self->closed) return;
                try {self->protocol->enter_world();} catch(...) {self->close();}
            });
        };
        result.enter_character_selection=[weak,executor] {
            asio::post(executor,[weak] {
                const auto self=weak.lock();if(!self || self->closed) return;
                try {self->protocol->enter_character_selection();} catch(...) {self->close();}
            });
        };
        result.pause_input=[weak,executor] {
            asio::post(executor,[weak] {if(auto self=weak.lock();self && !self->closed) self->input_paused=true;});
        };
        result.resume_input=[weak,executor] {
            asio::post(executor,[weak] {
                if(auto self=weak.lock();self && !self->closed) {
                    self->input_paused=false;
                    self->process_received();
                }
            });
        };
        result.disconnect=[weak,executor] {
            asio::post(executor,[weak] {if(auto self=weak.lock()) self->close();});
        };
        return result;
    }
    void schedule() {
        if(closed || working || !protocol->owner() || (loaded && pending.empty())) return;
        const auto server=owner.lock();if(!server) return close();
        working=true;
        const auto account=*protocol->owner();const bool initial=!loaded;
        Packet request;if(!initial) {request=std::move(pending.front());pending.pop_front();}
        const auto executor=socket.get_executor();const auto weak=weak_from_this();
        asio::post(worker,[self=shared_from_this(),executor,weak,account,initial,
                          request=std::move(request),factory=server->factory] {
            if(self->closed) return;
            bool failed=false;
            try {
                if(initial) {
                    self->application=factory(account,self->channel());
                    if(!self->application) throw std::runtime_error("World application factory returned null");
                    self->application->open();
                } else self->application->receive(request);
            } catch(...) {self->application_failed();failed=true;}
            asio::post(executor,[weak,initial,failed] {
                const auto connection=weak.lock();if(!connection || connection->closed) return;
                if(failed) return connection->close();
                connection->working=false;if(initial) connection->loaded=true;
                connection->process_received();
            });
        });
    }
    // Protocol state can change during an application call. Decrypt and
    // dispatch subsequent frames only after that call and its channel posts
    // have run on the socket executor, preserving the incoming cipher order.
    void process_received() {
        try {
            while(!closed && !working && !input_paused && !received.empty()) {
                auto packet=std::move(received.front());received.pop_front();
                auto event=protocol->receive(std::move(packet),world_now_ms());
                if(event.close) return close();
                for(auto& reply:event.replies) send(std::move(reply));
                if(closed) return;
                if(event.application) pending.push_back(std::move(*event.application));
                if(event.connected || event.application) schedule();
            }
        } catch(const std::exception&) {close();}
    }
    void read() {
        if(closed) return;
        socket.async_read_some(asio::buffer(input),[self=shared_from_this()](const asio::error_code& error,std::size_t count) {
            if(self->closed) return;
            if(error) return self->close();
            try {
                for(auto& packet:self->decoder.feed(std::span(self->input).first(count))) {
                    if(self->received.size()>=8) return self->close();
                    self->received.push_back(std::move(packet));
                    self->process_received();
                    if(self->closed) return;
                }
            } catch(const std::exception&) {return self->close();}
            self->read();
        });
    }
};
void WorldServer::Impl::accept() {
    if(!running) return;
    acceptor.async_accept([server=shared_from_this()](const asio::error_code& error,tcp::socket socket) {
        if(!server->running) return;
        if(!error) {
            if(server->connections.size()<server->options.maximum_connections && server->next_id<std::numeric_limits<std::uint64_t>::max()) {
                const auto id=server->next_id++;auto connection=std::make_shared<Connection>(std::move(socket),server,id);
                server->connections.emplace(id,connection);connection->start();
            } else {asio::error_code ignored;socket.close(ignored);}
        }
        server->accept();
    });
}
void WorldServer::Impl::stop() {
    stopped=true;running=false;asio::error_code ignored;acceptor.cancel(ignored);acceptor.close(ignored);
    while(!connections.empty()) {auto connection=connections.begin()->second;connection->close();}
}
namespace {
class RosterApplication final : public WorldApplication {
public:
    RosterApplication(session::LoginSession account,WorldChannel channel,LoadCharacters load,DispatchCharacter dispatch)
        : account_(std::move(account)),channel_(std::move(channel)),load_(std::move(load)),dispatch_(std::move(dispatch)) {}
    void open() override {roster_=load_(account_);for(auto& packet:game::character_list(roster_)) channel_.send(std::move(packet));}
    void receive(const Packet& packet) override {
        if(!dispatch_) return channel_.disconnect();
        auto reply=dispatch_(account_,roster_,packet);
        if(reply.close) return channel_.disconnect();
        for(auto& response:reply.packets) channel_.send(std::move(response));
    }
    void close() override {roster_={};}
private:
    session::LoginSession account_;
    WorldChannel channel_;
    LoadCharacters load_;
    DispatchCharacter dispatch_;
    game::CharacterRoster roster_;
};
WorldApplicationFactory roster_factory(LoadCharacters load,DispatchCharacter dispatch) {
    if(!load) throw std::invalid_argument("World roster requires a loader");
    return [load=std::move(load),dispatch=std::move(dispatch)](const session::LoginSession& account,WorldChannel channel) {
        return std::make_unique<RosterApplication>(account,std::move(channel),load,dispatch);
    };
}
}
WorldServer::WorldServer(asio::io_context& io,WorldServerOptions options,std::shared_ptr<session::Registry> sessions,
    WorldApplicationFactory factory)
    : impl_(std::make_shared<Impl>(io,std::move(options),std::move(sessions),std::move(factory))) {}
WorldServer::WorldServer(asio::io_context& io,WorldServerOptions options,std::shared_ptr<session::Registry> sessions,
    LoadCharacters load,DispatchCharacter dispatch)
    : WorldServer(io,std::move(options),std::move(sessions),roster_factory(std::move(load),std::move(dispatch))) {}
WorldServer::~WorldServer() {stop();impl_->database.join();}
void WorldServer::start() {asio::dispatch(impl_->strand,[server=impl_] {
    if(server->started || server->stopped) return;
    server->started=true;server->running=true;server->accept();
});}
void WorldServer::stop() {asio::dispatch(impl_->strand,[server=impl_] {server->stop();});}
std::uint16_t WorldServer::port() const {return impl_->bound_port;}
std::uint64_t WorldServer::application_failures() const noexcept {return impl_->application_failures.load();}
}
