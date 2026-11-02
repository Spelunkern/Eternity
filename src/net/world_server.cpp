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
    LoadCharacters load;
    DispatchCharacter dispatch;
    asio::thread_pool database;
    std::unordered_map<std::uint64_t,std::shared_ptr<Connection>> connections;
    std::uint64_t next_id = 1;
    std::uint16_t bound_port = 0;
    bool running = false,started = false,stopped = false;
    Impl(asio::io_context& io,WorldServerOptions config,std::shared_ptr<session::Registry> registry,
        LoadCharacters loader,DispatchCharacter dispatcher)
        : strand(asio::make_strand(io)),acceptor(strand),options(std::move(config)),sessions(std::move(registry)),
          load(std::move(loader)),dispatch(std::move(dispatcher)),database(options.database_workers) {
        if(!sessions || !load || options.realm>=254 || options.maximum_connections==0 || options.database_workers==0)
            throw std::invalid_argument("World server requires session registry, loader and bounded worker limits");
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
    std::shared_ptr<game::CharacterRoster> roster = std::make_shared<game::CharacterRoster>();
    std::size_t queued_bytes = 0;
    std::atomic_bool closed{false};
    bool working = false,loaded = false;
    Connection(tcp::socket peer,const std::shared_ptr<Impl>& server,std::uint64_t identity)
        : socket(std::move(peer)),owner(server),id(identity) {}
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
        output.clear();pending.clear();queued_bytes=0;
    }
    void send(Packet payload) {
        if(closed) return;
        auto packet=std::make_shared<Packet>(frame(payload));
        if(queued_bytes+packet->size()>16384) return close();
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
    void schedule() {
        if(closed || working || !protocol->owner() || (loaded && pending.empty())) return;
        const auto server=owner.lock();if(!server) return close();
        working=true;
        const auto account=*protocol->owner();const bool initial=!loaded;
        Packet request;if(!initial) {request=std::move(pending.front());pending.pop_front();}
        const auto executor=socket.get_executor();const auto weak=weak_from_this();
        asio::post(server->database,[weak,executor,account,initial,roster=roster,request=std::move(request),load=server->load,dispatch=server->dispatch] {
            const auto current=weak.lock();if(!current || current->closed) return;
            bool failed=false;game::CharacterReply reply;
            try {
                if(initial) {*roster=load(account);reply.packets=game::character_list(*roster);}
                else if(dispatch) reply=dispatch(account,*roster,request);
                else failed=true; // Unreconstructed commands must not report fabricated success.
            } catch(const std::exception&) {failed=true;}
            asio::post(executor,[weak,initial,failed,reply=std::move(reply)]() mutable {
                const auto connection=weak.lock();if(!connection || connection->closed) return;
                if(failed || reply.close) return connection->close();
                connection->working=false;if(initial) connection->loaded=true;
                try {
                    for(auto& packet:reply.packets) {
                        connection->send(connection->protocol->outgoing(std::move(packet)));
                        if(connection->closed) return;
                    }
                    connection->schedule();
                } catch(const std::exception&) {connection->close();}
            });
        });
    }
    void read() {
        if(closed) return;
        socket.async_read_some(asio::buffer(input),[self=shared_from_this()](const asio::error_code& error,std::size_t count) {
            if(self->closed) return;
            if(error) return self->close();
            try {
                for(auto& packet:self->decoder.feed(std::span(self->input).first(count))) {
                    auto event=self->protocol->receive(std::move(packet),world_now_ms());
                    if(event.close) return self->close();
                    for(auto& reply:event.replies) self->send(std::move(reply));
                    if(self->closed) return;
                    if(event.application) {
                        if(self->pending.size()>=8) return self->close();
                        self->pending.push_back(std::move(*event.application));
                    }
                    if(event.connected || event.application) self->schedule();
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
WorldServer::WorldServer(asio::io_context& io,WorldServerOptions options,std::shared_ptr<session::Registry> sessions,
    LoadCharacters load,DispatchCharacter dispatch)
    : impl_(std::make_shared<Impl>(io,std::move(options),std::move(sessions),std::move(load),std::move(dispatch))) {}
WorldServer::~WorldServer() {stop();impl_->database.join();}
void WorldServer::start() {asio::dispatch(impl_->strand,[server=impl_] {
    if(server->started || server->stopped) return;
    server->started=true;server->running=true;server->accept();
});}
void WorldServer::stop() {asio::dispatch(impl_->strand,[server=impl_] {server->stop();});}
std::uint16_t WorldServer::port() const {return impl_->bound_port;}
}
