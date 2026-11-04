#include "eternity/net/login_server.hpp"
#include "eternity/session/registry.hpp"
#include <asio.hpp>
#include <deque>
#include <atomic>
#include <chrono>
#include <limits>
#include <openssl/rand.h>
#include <unordered_map>

namespace eternity::net {
using asio::ip::tcp;
namespace {
std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
}

struct LoginServer::Impl : std::enable_shared_from_this<LoginServer::Impl> {
    struct Connection;
    asio::strand<asio::io_context::executor_type> strand;
    tcp::acceptor acceptor;
    asio::steady_timer handoff_timer;
    LoginServerOptions options;
    Authenticate authenticate;
    LoginKeyPair key_pair;
    std::shared_ptr<session::Registry> sessions;
    std::shared_ptr<auth::RealmDirectory> realms;
    asio::thread_pool database;
    std::unordered_map<std::uint64_t, std::shared_ptr<Connection>> connections;
    std::uint64_t next_id = 1;
    std::uint16_t bound_port = 0;
    bool running = false;
    bool started = false;
    bool stopped = false;

    Impl(asio::io_context& io, LoginServerOptions config, Authenticate function,
         std::shared_ptr<session::Registry> registry,std::shared_ptr<auth::RealmDirectory> directory)
        : strand(asio::make_strand(io)), acceptor(strand),handoff_timer(strand), options(std::move(config)),
          authenticate(std::move(function)),
          sessions(registry?std::move(registry):std::make_shared<session::Registry>()),
          realms(directory?std::move(directory):std::make_shared<auth::RealmDirectory>(options.realms)),
          database(options.database_workers) {
        if (!authenticate || options.maximum_connections == 0 || options.database_workers == 0)
            throw std::invalid_argument("Login server requires authentication and bounded nonzero worker limits");
        // Validate realm metadata before opening the listener.
        (void)auth::LoginProtocol(key_pair, realms);
        std::array<unsigned char, 8> seed{};
        if (RAND_bytes(seed.data(), static_cast<int>(seed.size())) != 1)
            throw std::runtime_error("Generate session identifier seed failed");
        next_id = 0;
        for (unsigned int index=0; index<8; ++index)
            next_id |= static_cast<std::uint64_t>(seed[index]) << (index*8);
        next_id &= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if (next_id == 0) next_id = 1;
        const auto address=asio::ip::make_address(options.address);
        if(!address.is_v4()) throw std::invalid_argument("Original login endpoints use IPv4");
        const tcp::endpoint endpoint(address, options.port);
        acceptor.open(endpoint.protocol());
        acceptor.set_option(tcp::acceptor::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen();
        bound_port = acceptor.local_endpoint().port();
    }

    void accept();
    void expire_handoffs();
    void stop();
};

struct LoginServer::Impl::Connection : std::enable_shared_from_this<Connection> {
    tcp::socket socket;
    std::weak_ptr<Impl> owner;
    std::uint64_t id;
    std::optional<std::uint32_t> user_id;
    auth::LoginProtocol protocol;
    FrameDecoder decoder;
    std::array<std::uint8_t, 8192> input{};
    std::deque<std::shared_ptr<Packet>> output;
    std::size_t queued_bytes = 0;
    std::atomic_bool closed{false};

    Connection(tcp::socket peer, const std::shared_ptr<Impl>& server, std::uint64_t identity)
        : socket(std::move(peer)), owner(server), id(identity), protocol(server->key_pair, server->realms) {}

    void start() { send(protocol.greeting()); read(); }

    void close() {
        if (closed) return;
        closed = true;
        protocol.close();
        const auto server = owner.lock();
        // Release the login owner before the peer can observe EOF. Otherwise a
        // reconnect or observer can see a disconnected socket with a live login.
        if (server && user_id) server->sessions->release_login(*user_id, id);
        asio::error_code ignored;
        socket.cancel(ignored);
        socket.shutdown(tcp::socket::shutdown_both, ignored);
        socket.close(ignored);
        if (server) server->connections.erase(id);
        // In-flight writes own their buffers separately until cancellation completes.
        output.clear();
        queued_bytes = 0;
    }

    void send(Packet payload) {
        if (closed) return;
        auto packet = std::make_shared<Packet>(frame(payload));
        if (queued_bytes + packet->size() > 16384) { close(); return; }
        const bool idle = output.empty();
        queued_bytes += packet->size();
        output.push_back(std::move(packet));
        if (idle) write();
    }

    void write() {
        if (closed || output.empty()) return;
        auto bytes = output.front();
        asio::async_write(socket, asio::buffer(*bytes),
            [self=shared_from_this(), bytes](const asio::error_code& error, std::size_t) {
                if (self->closed) return;
                if (error) { self->close(); return; }
                self->queued_bytes -= bytes->size();
                self->output.pop_front();
                self->write();
            });
    }

    void event(auth::LoginEvent result) {
        if (closed) return;
        if (result.close) { close(); return; }
        for (auto& reply : result.replies) send(std::move(reply));
        if (result.credentials) authenticate(std::move(*result.credentials));
        if (result.selection) {
            const auto server=owner.lock();
            const bool accepted=server && user_id && server->sessions->select(*user_id,id,result.selection->realm_id,now_ms());
            event(protocol.selected(accepted));
        }
    }

    void authenticated(auth::LoginResult result, const std::string& username) {
        if (closed || protocol.state() != auth::LoginProtocol::State::authenticating) return;
        const auto server = owner.lock();
        if (!server) { close(); return; }
        std::array<std::uint8_t,16> token{};
        try {
            const auto decision = auth::login_decision(result);
            if (decision.result == 0) {
                if (RAND_bytes(token.data(), static_cast<int>(token.size())) != 1)
                    throw std::runtime_error("Generate login session token failed");
                user_id = static_cast<std::uint32_t>(result.user_id);
                auto record=session::LoginSession{
                    *user_id,id,decision.permission,username,token,session::game_key(protocol.secrets())};
                const auto executor=socket.get_executor();
                const auto weak=weak_from_this();
                record.disconnect=[weak,executor] { if(weak.expired()) return; asio::dispatch(executor,[weak] {
                    if(auto connection=weak.lock()) connection->close();
                }); };
                auto previous = server->sessions->add(std::move(record));
                if (previous && previous->session_id != id) {
                    if(previous->disconnect) previous->disconnect();
                }
            }
            event(protocol.complete(result, token));
        } catch (const std::exception&) { close(); }
    }

    void authenticate(auth::Credentials credentials) {
        auto server = owner.lock();
        if (!server || closed) { close(); return; }
        const auto endpoint = socket.remote_endpoint();
        const auto ip = endpoint.address().to_string();
        const auto function = server->authenticate;
        const auto executor = socket.get_executor();
        const auto weak = weak_from_this();
        const auto session_id = id;
        asio::post(server->database, [weak,executor,function,session_id,ip,credentials=std::move(credentials)] {
            const auto current = weak.lock();
            if (!current || current->closed) return;
            auth::LoginResult result{-3,0};
            try { result = function(credentials, session_id, ip); }
            catch (const std::exception&) { /* GetUser maps SQL/pool failure to wire result 1. */ }
            asio::post(executor, [weak,result,username=credentials.username] {
                if (auto connection=weak.lock()) connection->authenticated(result, username);
            });
        });
    }

    void read() {
        if (closed) return;
        socket.async_read_some(asio::buffer(input),
            [self=shared_from_this()](const asio::error_code& error, std::size_t count) {
                if (self->closed) return;
                if (error) { self->close(); return; }
                try {
                    auto packets = self->decoder.feed(std::span(self->input).first(count));
                    for (auto& packet : packets) {
                        self->event(self->protocol.receive(std::move(packet)));
                        if (self->closed) return;
                    }
                } catch (const std::exception&) { self->close(); return; }
                self->read();
            });
    }
};

void LoginServer::Impl::accept() {
    if (!running) return;
    acceptor.async_accept([self=shared_from_this()](const asio::error_code& error, tcp::socket socket) {
        if (!self->running) return;
        if (!error) {
            if (self->connections.size() < self->options.maximum_connections &&
                self->next_id < static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                const auto identity = self->next_id++;
                auto connection = std::make_shared<Connection>(std::move(socket), self, identity);
                self->connections.emplace(identity, connection);
                connection->start();
            } else {
                asio::error_code ignored;
                socket.close(ignored);
            }
        }
        self->accept();
    });
}

void LoginServer::Impl::expire_handoffs() {
    if(!running) return;
    handoff_timer.expires_after(std::chrono::seconds(1));
    handoff_timer.async_wait([server=shared_from_this()](const asio::error_code& error) {
        if(error || !server->running) return;
        for(const auto& expired:server->sessions->expire(now_ms())) if(expired.disconnect) expired.disconnect();
        server->expire_handoffs();
    });
}

void LoginServer::Impl::stop() {
    stopped = true;
    running = false;
    asio::error_code ignored;
    acceptor.cancel(ignored);
    acceptor.close(ignored);
    handoff_timer.cancel(ignored);
    while (!connections.empty()) {
        auto connection = connections.begin()->second;
        connection->close();
    }
}

LoginServer::LoginServer(asio::io_context& io, LoginServerOptions options, Authenticate function,
                         std::shared_ptr<session::Registry> sessions,std::shared_ptr<auth::RealmDirectory> realms)
    : impl_(std::make_shared<Impl>(io,std::move(options),std::move(function),std::move(sessions),std::move(realms))) {}
LoginServer::~LoginServer() { stop(); impl_->database.join(); }
void LoginServer::start() {
    asio::dispatch(impl_->strand, [server=impl_] {
        if (server->started || server->stopped) return;
        server->started = true;
        server->running = true;
        server->accept();
        server->expire_handoffs();
    });
}
void LoginServer::stop() { asio::dispatch(impl_->strand, [server=impl_] { server->stop(); }); }
std::uint16_t LoginServer::port() const { return impl_->bound_port; }
std::size_t LoginServer::session_count() const { return impl_->sessions->size(); }
}
