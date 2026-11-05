#include "eternity/net/login_server.hpp"
#include "eternity/net/login_reply.hpp"
#include "eternity/session/registry.hpp"
#include <asio.hpp>
#include <deque>
#include <atomic>
#include <chrono>
#include <limits>
#include <mutex>
#include <openssl/rand.h>
#include <unordered_map>

namespace eternity::net {
using asio::ip::tcp;
namespace {
struct AuthenticationSlot {
    std::shared_ptr<std::atomic_size_t> count;
    explicit AuthenticationSlot(std::shared_ptr<std::atomic_size_t> c):count(std::move(c)) {++*count;}
    ~AuthenticationSlot() {--*count;}
};
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
    asio::steady_timer reply_timer;
    LoginReplyScanGate reply_gate;
    asio::steady_timer drain_timer;
    LoginServerOptions options;
    Authenticate authenticate;
    LoginKeyPair key_pair;
    std::shared_ptr<session::Registry> sessions;
    std::shared_ptr<auth::RealmDirectory> realms;
    asio::thread_pool database;
    std::shared_ptr<std::atomic_size_t> outstanding_auth=std::make_shared<std::atomic_size_t>(0);
    std::atomic_uint64_t authentication_delivery_failures{0};
    std::atomic_size_t pending_authentication_delivery_failures{0};
    std::atomic_uint64_t handoff_disconnect_failures{0};
    std::atomic_uint64_t handoff_expiration_failures{0};
    struct FailedHandoffs {
        std::vector<session::Registry::ExpiredHandoff> owners;
        std::unique_ptr<FailedHandoffs> next;
    };
    std::unique_ptr<FailedHandoffs> failed_handoffs;
    std::atomic_size_t failed_handoff_count{0};
    std::size_t handoff_dispatches=0; // Socket strand only.
    static void request_disconnect(const std::shared_ptr<const session::LoginDisconnectFence>& fence) {
        if(!fence)throw std::logic_error("Disconnect fence missing");
        const auto result=fence->request_disconnect();
        if(result==session::LoginDisconnectFence::Request::in_flight || result==session::LoginDisconnectFence::Request::retired)
            throw std::logic_error("Disconnect request is unavailable");
    }
    void retry_failed_handoffs() {
        auto pending=std::move(failed_handoffs);
        while(pending) {
            auto batch=std::move(pending);pending=std::move(batch->next);bool remaining=false;
            for(auto& entry:batch->owners) if(entry.owner.disconnect) {
                try {
                    request_disconnect(entry.disconnect_fence);
                    if(!sessions->release_login_disconnect_fence(entry.disconnect_fence))throw std::logic_error("Handoff disconnect fence lost");
                    entry.owner.disconnect={};entry.disconnect_fence.reset();--failed_handoff_count;
                }
                catch(...) {remaining=true;++handoff_disconnect_failures;}
            }
            if(remaining) {batch->next=std::move(failed_handoffs);failed_handoffs=std::move(batch);}
        }
        check_authentication_drain();
    }
    struct FailedReplacement {
        std::optional<session::LoginSession> owner;
        std::shared_ptr<const session::LoginDisconnectFence> fence;
        std::unique_ptr<FailedReplacement> next;
    };
    std::unique_ptr<FailedReplacement> failed_replacements;
    std::atomic_size_t failed_replacement_count{0};
    std::size_t replacement_dispatches=0; // Socket strand only.
    std::atomic_uint64_t replacement_disconnect_failures{0};
    // Socket strand only. Node was allocated before replacing the registry owner.
    void retain_failed_replacement(std::unique_ptr<FailedReplacement> node) noexcept {
        node->next=std::move(failed_replacements);failed_replacements=std::move(node);
        ++failed_replacement_count;++replacement_disconnect_failures;
    }
    void retry_failed_replacements() {
        auto pending=std::move(failed_replacements);
        while(pending) {
            auto node=std::move(pending);pending=std::move(node->next);
            try {
                request_disconnect(node->fence);
                if(!sessions->release_login_disconnect_fence(node->fence))throw std::logic_error("Replacement disconnect fence lost");
                node->fence.reset();--failed_replacement_count;
            }
            catch(...) {
                node->next=std::move(failed_replacements);failed_replacements=std::move(node);
                ++replacement_disconnect_failures;
            }
        }
        check_authentication_drain();
    }
    // Socket strand only. A retained failure cannot be bypassed by another
    // login handled by this owner while its real disconnect request is pending.
    bool failed_disconnect_for(std::uint32_t account) const {
        for(auto* node=failed_replacements.get();node;node=node->next.get())
            if(node->owner && node->owner->user_id==account)return true;
        for(auto* batch=failed_handoffs.get();batch;batch=batch->next.get())
            for(const auto& entry:batch->owners)if(entry.owner.disconnect && entry.owner.user_id==account)return true;
        return false;
    }
    struct DrainCompletion {
        DrainCompletion(std::function<void()> value,asio::io_context::executor_type executor)
            :callback(std::move(value)),guard(asio::make_work_guard(executor)) {}
        std::function<void()> callback;
        asio::executor_work_guard<asio::io_context::executor_type> guard;
        std::unique_ptr<DrainCompletion> next;
    };
    std::unique_ptr<DrainCompletion> drain_completions;
    DrainCompletion* drain_completions_tail=nullptr;
    bool drain_waiting=false;
    mutable std::mutex drain_error_mutex;
    std::exception_ptr drain_error;
    void set_drain_error(std::exception_ptr error) {std::lock_guard lock(drain_error_mutex);drain_error=std::move(error);}
    std::exception_ptr get_drain_error() const {std::lock_guard lock(drain_error_mutex);return drain_error;}
    std::unordered_map<std::uint64_t, std::shared_ptr<Connection>> connections;
    std::uint64_t next_id = 1;
    std::uint16_t bound_port = 0;
    bool running = false;
    bool started = false;
    bool stopped = false;

    Impl(asio::io_context& io, LoginServerOptions config, Authenticate function,
         std::shared_ptr<session::Registry> registry,std::shared_ptr<auth::RealmDirectory> directory)
        : strand(asio::make_strand(io)), acceptor(strand),handoff_timer(strand),reply_timer(strand),drain_timer(strand), options(std::move(config)),
          authenticate(std::move(function)),
          sessions(registry?std::move(registry):std::make_shared<session::Registry>()),
          realms(directory?std::move(directory):std::make_shared<auth::RealmDirectory>(options.realms)),
          database(options.database_workers) {
        if (!authenticate || options.maximum_connections == 0 || options.database_workers == 0 || options.reply_scan_period.count() <= 0)
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
    void scan_replies();
    std::uint64_t tick() const { return options.clock ? options.clock() : now_ms(); }
    void stop();
    void check_authentication_drain();
};

struct LoginServer::Impl::Connection : std::enable_shared_from_this<Connection> {
    tcp::socket socket;
    std::weak_ptr<Impl> owner;
    std::uint64_t id;
    std::optional<std::uint32_t> user_id;
    auth::LoginProtocol protocol;
    LoginReplyClock reply_clock;
    FrameDecoder decoder;
    std::array<std::uint8_t, 8192> input{};
    std::deque<std::shared_ptr<Packet>> output;
    std::size_t queued_bytes = 0;
    std::atomic_bool closed{false};
    std::atomic_bool authentication_delivery_failed{false};
    std::mutex lifecycle_mutex;

    Connection(tcp::socket peer, const std::shared_ptr<Impl>& server, std::uint64_t identity)
        : socket(std::move(peer)), owner(server), id(identity), protocol(server->key_pair, server->realms) {}

    void start() { send(protocol.greeting()); read(); }

    void close() {
        {std::lock_guard lock(lifecycle_mutex);if (closed.exchange(true)) return;}
        protocol.close();
        const auto server = owner.lock();
        if(authentication_delivery_failed.exchange(false) && server)--server->pending_authentication_delivery_failures;
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
        if (result.heartbeat) {
            const auto server=owner.lock();
            if (!server || reply_clock.observe_reply(server->tick())) { close(); return; }
        }
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
                if(server->failed_disconnect_for(*user_id))throw std::runtime_error("Previous disconnect request is unresolved");
                auto record=session::LoginSession{
                    *user_id,id,decision.permission,username,token,session::game_key(protocol.secrets())};
                const auto executor=socket.get_executor();
                const auto weak=weak_from_this();
                record.disconnect=[weak,executor] {
                    const auto connection=weak.lock();if(!connection) return;
                    std::lock_guard lock(connection->lifecycle_mutex);if(connection->closed) return;
                    asio::post(executor,[weak] {if(auto current=weak.lock()) current->close();});
                };
                // Reserve recovery storage before replacing the actual owner.
                auto recovery=std::make_unique<Impl::FailedReplacement>();
                auto added=server->sessions->add_with_disconnect_fence(std::move(record));
                if(added.blocked)throw std::runtime_error("Previous disconnect request is unresolved");
                if (added.previous && added.previous->session_id != id && added.previous->disconnect) {
                    recovery->owner=std::move(added.previous);recovery->fence=std::move(added.disconnect_fence);
                    ++server->replacement_dispatches;
                    try {
                        server->request_disconnect(recovery->fence);
                        if(!server->sessions->release_login_disconnect_fence(recovery->fence))throw std::logic_error("Replacement disconnect fence lost");
                        recovery->fence.reset();
                    }
                    catch(...) {
                        server->retain_failed_replacement(std::move(recovery));
                        --server->replacement_dispatches;server->check_authentication_drain();throw;
                    }
                    --server->replacement_dispatches;server->check_authentication_drain();
                }
            }
            event(protocol.complete(result, token));
        } catch (...) { close(); }
    }

    void authenticate(auth::Credentials credentials) {
        auto server = owner.lock();
        if (!server || closed) { close(); return; }
        // Socket close does not cancel an executing backend call. Keep that
        // job counted until it actually returns, including jobs queued behind
        // it, so repeated disconnects cannot grow the worker queue unbounded.
        if(server->outstanding_auth->load()>=server->options.maximum_connections) {close();return;}
        auto slot=std::make_shared<AuthenticationSlot>(server->outstanding_auth);
        const auto endpoint = socket.remote_endpoint();
        const auto ip = endpoint.address().to_string();
        const auto function = server->authenticate;
        const auto before_delivery=server->options.before_authentication_delivery_post;
        const auto executor = socket.get_executor();
        const auto weak = weak_from_this();
        const auto session_id = id;
        asio::post(server->database, [weak,executor,function,before_delivery,session_id,ip,slot=std::move(slot),credentials=std::move(credentials)]() mutable {
            const auto current = weak.lock();
            if (!current || current->closed) return;
            auth::LoginResult result{-3,0};
            try { result = function(credentials, session_id, ip); }
            catch (...) { /* GetUser maps SQL/pool failure to wire result 1. */ }
            {
                std::lock_guard lock(current->lifecycle_mutex);
                if(current->closed) return;
                try {
                    if(before_delivery)before_delivery(false);
                    asio::post(executor, [weak,result,username=credentials.username] {
                        if (auto connection=weak.lock()) {
                            {std::lock_guard guard(connection->lifecycle_mutex);if(connection->closed) return;}
                            connection->authenticated(result, username);
                        }
                    });
                    // The posted completion cannot run past its lifecycle check
                    // until the slot is released, so retry may use it immediately.
                    slot.reset();
                } catch(...) {
                    const auto server=current->owner.lock();
                    if(server && !current->authentication_delivery_failed.exchange(true)) {
                        ++server->pending_authentication_delivery_failures;
                        ++server->authentication_delivery_failures;
                    }
                    try {
                        if(before_delivery)before_delivery(true);
                        asio::post(executor,[weak]{if(auto connection=weak.lock())connection->close();});
                    } catch(...) { /* retained for explicit real socket-close retry */ }
                }
            }
        });
    }

    void read() {
        if (closed) return;
        socket.async_read_some(asio::buffer(input),
            [self=shared_from_this()](const asio::error_code& error, std::size_t count) {
                if (self->closed) return;
                if (error) { self->close(); return; }
                try {
                    self->decoder.feed_each(std::span(self->input).first(count),[&](Packet packet) {
                        self->event(self->protocol.receive(std::move(packet)));
                        return !self->closed;
                    });
                } catch (const std::exception&) { self->close(); return; }
                if (!self->closed) self->read();
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
        try {
            // Allocate recovery ownership before expiration removes any owner.
            auto batch=std::make_unique<FailedHandoffs>();
            batch->owners=server->sessions->expire_with_disconnect_fences(now_ms());
            ++server->handoff_dispatches;bool failed=false;
            for(auto& expired:batch->owners) if(expired.owner.disconnect) {
                try {
                    server->request_disconnect(expired.disconnect_fence);
                    if(!server->sessions->release_login_disconnect_fence(expired.disconnect_fence))throw std::logic_error("Handoff disconnect fence lost");
                    expired.owner.disconnect={};expired.disconnect_fence.reset();
                }
                catch(...) {failed=true;++server->failed_handoff_count;++server->handoff_disconnect_failures;}
            }
            if(failed) {batch->next=std::move(server->failed_handoffs);server->failed_handoffs=std::move(batch);}
            --server->handoff_dispatches;server->check_authentication_drain();
        } catch(...) {++server->handoff_expiration_failures;}
        server->expire_handoffs();
    });
}

void LoginServer::Impl::scan_replies() {
    if (!running) return;
    reply_timer.expires_after(options.reply_scan_period);
    reply_timer.async_wait([server=shared_from_this()](const asio::error_code& error) {
        if (error || !server->running) return;
        const auto now=server->tick();
        if (server->reply_gate.due(now)) {
            for (auto it=server->connections.begin(); it!=server->connections.end();) {
                auto connection=(it++)->second;
                if (connection->reply_clock.timed_out(now)) connection->close();
            }
        }
        server->scan_replies();
    });
}

void LoginServer::Impl::stop() {
    stopped = true;
    running = false;
    asio::error_code ignored;
    acceptor.cancel(ignored);
    acceptor.close(ignored);
    handoff_timer.cancel(ignored);
    reply_timer.cancel(ignored);
    while (!connections.empty()) {
        auto connection = connections.begin()->second;
        connection->close();
    }
    check_authentication_drain();
}
void LoginServer::Impl::check_authentication_drain() {
    if(!stopped || !drain_completions) return;
    if(outstanding_auth->load()==0 && failed_replacement_count.load()==0 && replacement_dispatches==0 &&
       failed_handoff_count.load()==0 && handoff_dispatches==0) {
        set_drain_error({});
        auto completions=std::move(drain_completions);drain_completions_tail=nullptr;
        while(completions) {
            auto completion=std::move(completions);completions=std::move(completion->next);
            try {completion->callback();} catch(...) {}
        }
        return;
    }
    // Failed replacement requests are retried explicitly, not by a timer.
    if(outstanding_auth->load()==0 || drain_waiting) return;
    try {
        if(options.before_authentication_drain_wait)options.before_authentication_drain_wait();
        drain_timer.expires_after(std::chrono::milliseconds(25));
        drain_timer.async_wait([server=shared_from_this()](const asio::error_code& error) {
            server->drain_waiting=false;
            if(!error) {server->check_authentication_drain();return;}
            if(!server->drain_completions)return;
            try {throw asio::system_error(error);} catch(...) {server->set_drain_error(std::current_exception());}
        });
        drain_waiting=true;set_drain_error({});
    } catch(...) {drain_waiting=false;set_drain_error(std::current_exception());}
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
        server->scan_replies();
    });
}
void LoginServer::stop() { asio::dispatch(impl_->strand, [server=impl_] { server->stop(); }); }
void LoginServer::stop(std::function<void()> completion) {
    if(!completion) throw std::invalid_argument("Login stop requires completion");
    // Allocate before dispatch so a registration failure reaches the caller.
    auto node=std::make_unique<Impl::DrainCompletion>(std::move(completion),impl_->strand.get_inner_executor());
    asio::dispatch(impl_->strand,[server=impl_,node=std::move(node)]() mutable {
        auto* tail=node.get();
        if(server->drain_completions_tail)server->drain_completions_tail->next=std::move(node);
        else server->drain_completions=std::move(node);
        server->drain_completions_tail=tail;server->stop();
    });
}
std::exception_ptr LoginServer::authentication_drain_error() const {return impl_->get_drain_error();}
void LoginServer::retry_authentication_drain() {
    asio::dispatch(impl_->strand,[server=impl_] {server->check_authentication_drain();});
}
std::uint16_t LoginServer::port() const { return impl_->bound_port; }
std::size_t LoginServer::outstanding_authentication() const noexcept { return impl_->outstanding_auth->load(); }
std::uint64_t LoginServer::authentication_delivery_failures() const noexcept {return impl_->authentication_delivery_failures.load();}
std::size_t LoginServer::pending_authentication_delivery_failures() const noexcept {return impl_->pending_authentication_delivery_failures.load();}
void LoginServer::retry_authentication_delivery_failures() {
    asio::dispatch(impl_->strand,[server=impl_] {
        for(auto it=server->connections.begin();it!=server->connections.end();) {
            auto connection=(it++)->second;
            if(connection->authentication_delivery_failed.load())connection->close();
        }
    });
}
std::uint64_t LoginServer::handoff_disconnect_failures() const noexcept { return impl_->handoff_disconnect_failures.load(); }
std::size_t LoginServer::pending_handoff_disconnects() const noexcept {return impl_->failed_handoff_count.load();}
void LoginServer::retry_handoff_disconnects() {
    asio::dispatch(impl_->strand,[server=impl_]{server->retry_failed_handoffs();});
}
std::uint64_t LoginServer::handoff_expiration_failures() const noexcept { return impl_->handoff_expiration_failures.load(); }
std::size_t LoginServer::pending_replacement_disconnects() const noexcept {return impl_->failed_replacement_count.load();}
std::uint64_t LoginServer::replacement_disconnect_failures() const noexcept {return impl_->replacement_disconnect_failures.load();}
void LoginServer::retry_replacement_disconnects() {
    asio::dispatch(impl_->strand,[server=impl_]{server->retry_failed_replacements();});
}
std::size_t LoginServer::session_count() const { return impl_->sessions->size(); }
}
