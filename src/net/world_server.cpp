#include "eternity/net/world_server.hpp"
#include "outbound_budget.hpp"
#include "world_reply_schedule.hpp"
#include <asio.hpp>
#include <atomic>
#include <chrono>
#include <deque>
#include <limits>
#include <mutex>
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
    asio::steady_timer reply_timer;
    WorldServerOptions options;
    detail::WorldReplySchedule reply_schedule;
    std::shared_ptr<session::Registry> sessions;
    std::shared_ptr<const WorldXor> world_xor = random_world_xor();
    WorldApplicationFactory factory;
    std::atomic_uint64_t application_failures{0};
    asio::thread_pool database;
    std::unordered_map<std::uint64_t,std::shared_ptr<Connection>> connections;
    // Own each actual close until its socket-strand notification is delivered.
    std::unordered_map<std::uint64_t,std::shared_ptr<Connection>> close_receipts;
    std::atomic_uint64_t close_submission_failures{0};
    std::atomic_size_t pending_close_submissions{0};
    std::atomic_uint64_t close_notification_failures{0};
    std::atomic_size_t pending_close_notifications{0};
    std::atomic_uint64_t retained_lookup_delivery_failures{0};
    std::atomic_size_t pending_retained_lookup_delivery_failures{0};
    std::uint64_t next_id = 1;
    std::uint16_t bound_port = 0;
    std::size_t outstanding_closes = 0;
    struct StopCompletion {
        explicit StopCompletion(std::function<void(bool)> value):callback(std::move(value)) {}
        std::function<void(bool)> callback;
        std::unique_ptr<StopCompletion> next;
    };
    std::unique_ptr<StopCompletion> stop_completions;
    StopCompletion* stop_completions_tail=nullptr;
    std::atomic_bool close_failed{false};
    bool running = false,started = false,stopped = false;
    Impl(asio::io_context& io,WorldServerOptions config,std::shared_ptr<session::Registry> registry,
        WorldApplicationFactory application_factory)
        : strand(asio::make_strand(io)),acceptor(strand),reply_timer(strand),options(std::move(config)),
          reply_schedule(static_cast<std::uint32_t>(options.reply_scan_interval.count())),sessions(std::move(registry)),
          factory(std::move(application_factory)),database(options.database_workers) {
        if(!sessions || !factory || options.realm>=254 || options.maximum_connections==0 || options.database_workers==0 || options.maximum_queued_bytes<4 || options.reply_scan_interval.count()<=0 || static_cast<std::uint64_t>(options.reply_scan_interval.count())>std::numeric_limits<std::uint32_t>::max())
            throw std::invalid_argument("World server requires session registry, application factory and bounded worker limits");
        const auto address=asio::ip::make_address(options.address);
        if(!address.is_v4()) throw std::invalid_argument("Original game endpoints use IPv4");
        const tcp::endpoint endpoint(address,options.port);
        acceptor.open(endpoint.protocol());acceptor.set_option(tcp::acceptor::reuse_address(true));
        acceptor.bind(endpoint);acceptor.listen();bound_port=acceptor.local_endpoint().port();
    }
    std::uint64_t now_ms() const {return options.monotonic_ms?options.monotonic_ms():world_now_ms();}
    void accept();
    void scan_replies();
    void stop();
    void complete_close(std::uint64_t id);
    void notify_stopped();
};
struct WorldServer::Impl::Connection : std::enable_shared_from_this<Connection> {
    tcp::socket socket;
    std::weak_ptr<Impl> owner;
    std::uint64_t id;
    std::unique_ptr<WorldProtocol> protocol;
    FrameDecoder decoder;
    std::array<std::uint8_t,8192> input{};
    struct Outgoing {Packet bytes;std::shared_ptr<detail::OutboundBudget::Reservation> reservation;std::shared_ptr<std::function<void(bool)>> completion;};
    std::deque<std::shared_ptr<Outgoing>> output;
    struct PendingPacket {Packet packet;bool nprotect=false;};
    struct PendingLookup {Packet packet;std::mutex mutex;bool completed=false,cancelled=false,delivery_failed=false;};
    std::shared_ptr<PendingLookup> retained_lookup;
    std::deque<PendingPacket> pending;
    std::deque<Packet> received;
    asio::strand<asio::thread_pool::executor_type> worker;
    asio::executor_work_guard<asio::any_io_executor> lifetime;
    std::unique_ptr<WorldApplication> application; // Worker strand only.
    bool factory_failed=false; // Worker strand only.
    std::shared_ptr<const session::WorldReservation> reservation; // Socket strand until close.
    detail::OutboundBudget output_budget;
    std::atomic_bool output_rejected{false};
    // Bound decoded input independently of packet count while application work runs.
    static constexpr std::size_t maximum_received_bytes = 64 * 1024;
    std::size_t received_bytes = 0;
    std::atomic_bool closed{false};
    std::atomic_size_t tracked_pending{0};
    std::atomic_size_t pending_batches{0};
    bool close_worker_done=false; // Socket strand only.
    bool writing=false; // Socket strand only.
    // Serialize submissions and IO lease acquisition with connection close.
    std::mutex lifecycle_mutex;
    std::mutex close_notification_mutex;
    bool close_notification_failed=false;
    bool close_submission_failed=false; // Socket strand only.
    std::optional<std::uint32_t> close_reason; // Transferred to worker after submission.
    bool working = false,loaded = false,input_paused = false;
    Connection(tcp::socket peer,const std::shared_ptr<Impl>& server,std::uint64_t identity)
        : socket(std::move(peer)),owner(server),id(identity),worker(asio::make_strand(server->database)),
          lifetime(socket.get_executor()),output_budget(server->options.maximum_queued_bytes) {}
    void start() {
        const auto server=owner.lock();if(!server) return close();
        const auto weak=weak_from_this();const auto executor=socket.get_executor();
        protocol=std::make_unique<WorldProtocol>(server->sessions,server->options.realm,id,server->world_xor,[weak,executor] {
            const auto self=weak.lock();if(!self) return;
            std::lock_guard lock(self->lifecycle_mutex);if(self->closed) return;
            // Always post: inline dispatch could re-enter close while this
            // lifecycle mutex is held on the socket strand.
            asio::post(executor,[weak] {if(auto connection=weak.lock()) connection->close();});
        });
        read();
    }
    void close(std::optional<std::uint32_t> reason={}) {
        {std::lock_guard lock(lifecycle_mutex);if(closed.exchange(true)) return;}
        if(protocol) protocol->close();
        asio::error_code ignored;socket.cancel(ignored);socket.shutdown(tcp::socket::shutdown_both,ignored);socket.close(ignored);
        auto server=owner.lock();if(server) server->connections.erase(id);
        // Keep an in-flight write until its actual cancellation/completion
        // handler; queued frames have never been submitted and fail now.
        while(output.size()>(writing?1U:0U)) {
            auto item=std::move(output.back());output.pop_back();
            finish_tracked(std::move(item->completion),false);
        }
        pending.clear();received.clear();
        if(retained_lookup) {
            std::lock_guard lock(retained_lookup->mutex);retained_lookup->cancelled=true;
            if(retained_lookup->delivery_failed && server) {
                retained_lookup->delivery_failed=false;--server->pending_retained_lookup_delivery_failures;
            }
        }
        retained_lookup.reset();received_bytes=0;
        close_reason=reason;
        submit_close(server);
    }
    // Socket strand only. Failed submission retains this receipt, the reason,
    // reservation and IO lease for an explicit retry of the real worker task.
    void submit_close(const std::shared_ptr<Impl>& server) {
        const auto executor=socket.get_executor();
        try {
            if(server && server->options.before_close_submission_post)server->options.before_close_submission_post();
            asio::post(worker,[self=shared_from_this(),server,executor]() mutable {
                auto reservation=std::move(self->reservation);const auto reason=self->close_reason;
                bool uncertain=self->factory_failed;
                if(self->application) {
                    try {self->application->close_with_reason(reason);} catch(...) {
                        uncertain=true;
                        self->application_failed();
                    }
                    self->application.reset();
                }
                if(uncertain && server) {
                    server->close_failed=true;
                    // The entry was allocated by claim; retaining it cannot allocate.
                    // A missing claim is an invariant violation, never success.
                    if(reservation && !server->sessions->retain_failed_world(reservation))
                        ++server->application_failures;
                }
                reservation.reset();
                if(server) {
                    try {
                        if(server->options.before_close_notification_post)server->options.before_close_notification_post();
                        asio::post(executor,[server,id=self->id] {server->complete_close(id);});
                    } catch(...) {
                        // The close really returned; retain its receipt and IO lease
                        // until a caller retries delivery on the socket strand.
                        std::lock_guard lock(self->close_notification_mutex);
                        self->close_notification_failed=true;
                        ++server->close_notification_failures;++server->pending_close_notifications;
                        return;
                    }
                }
                self->lifetime.reset();
            });
            if(close_submission_failed) {close_submission_failed=false;if(server)--server->pending_close_submissions;}
        } catch(...) {
            if(server) {
                ++server->close_submission_failures;
                if(!close_submission_failed){close_submission_failed=true;++server->pending_close_submissions;}
            }
        }
    }
    void finish_tracked(std::shared_ptr<std::function<void(bool)>> completion,bool ok) {
        if(!completion || !*completion)return;
        auto callback=std::move(*completion);
        try {callback(ok);} catch(...) {application_failed();}
        if(--tracked_pending==0 && close_worker_done) {
            if(auto server=owner.lock())server->complete_close(id);
        }
    }
    void send_encoded(Packet bytes,std::shared_ptr<detail::OutboundBudget::Reservation> reservation,std::shared_ptr<std::function<void(bool)>> completion) {
        if(closed) {finish_tracked(std::move(completion),false);return;}
        auto packet=std::make_shared<Outgoing>(Outgoing{std::move(bytes),std::move(reservation),std::move(completion)});
        const bool idle=output.empty();output.push_back(std::move(packet));if(idle)write();
    }
    void send(Packet payload,std::shared_ptr<detail::OutboundBudget::Reservation> reservation={},std::shared_ptr<std::function<void(bool)>> completion={}) {
        if(closed) {finish_tracked(std::move(completion),false);return;}
        if(!reservation) reservation=output_budget.reserve(payload.size()+2);
        if(!reservation) {finish_tracked(std::move(completion),false);return close();}
        auto packet=std::make_shared<Outgoing>(Outgoing{frame(payload),std::move(reservation),std::move(completion)});
        const bool idle=output.empty();output.push_back(std::move(packet));if(idle) write();
    }
    void write() {
        if(closed || output.empty()) return;
        auto packet=output.front();writing=true;
        try {asio::async_write(socket,asio::buffer(packet->bytes),[self=shared_from_this(),packet](const asio::error_code& error,std::size_t) {
            self->writing=false;
            if(!self->output.empty() && self->output.front()==packet)self->output.pop_front();
            // Hold the close receipt until the actual completion callback has
            // returned, even if that callback reenters stop or disconnect.
            self->finish_tracked(std::move(packet->completion),!error);
            if(self->closed)return;
            if(error)return self->close();
            self->write();
        });} catch(...) {
            writing=false;output.pop_front();
            finish_tracked(std::move(packet->completion),false);close();
        }
    }
    void application_failed() {
        if(auto server=owner.lock()) ++server->application_failures;
    }
    WorldChannel channel() {
        const auto weak=weak_from_this();const auto executor=socket.get_executor();
        const auto post_open=[weak,executor](auto action) {
            const auto self=weak.lock();if(!self) return;
            std::lock_guard lock(self->lifecycle_mutex);if(self->closed) return;
            asio::post(executor,[weak,action=std::move(action)]() mutable {
                if(auto connection=weak.lock();connection && !connection->closed) action(*connection);
            });
        };
        WorldChannel result;
        result.connection_identity=[weak]() -> std::optional<WorldConnectionIdentity> {
            const auto self=weak.lock();if(!self) return {};
            std::lock_guard lock(self->lifecycle_mutex);if(self->closed) return {};
            return WorldConnectionIdentity{self->id,std::weak_ptr<const void>(self),[weak] {
                const auto current=weak.lock();if(!current)return false;
                std::lock_guard guard(current->lifecycle_mutex);return !current->closed;
            }};
        };
        result.pending_send_state=[weak]() -> std::optional<WorldPendingSendState> {
            const auto self=weak.lock();if(!self)return {};
            std::lock_guard lock(self->lifecycle_mutex);if(self->closed)return {};
            const auto server=self->owner.lock();if(!server)return {};
            return WorldPendingSendState{self->output_budget.frames(),self->output_budget.used(),self->output_budget.limit(),self->pending_batches.load(),server->options.maximum_pending_batches};
        };
        result.send_tracked=[weak,executor](Packet packet,std::function<void(bool)> completion) -> bool {
            if(!completion)return false;
            const auto self=weak.lock();if(!self)return false;
            std::lock_guard lock(self->lifecycle_mutex);if(self->closed || self->output_rejected)return false;
            if(packet.size()<2 || packet.size()>maximum_frame_size-2)return false;
            auto reservation=self->output_budget.reserve(packet.size()+2);if(!reservation)return false;
            auto callback=std::make_shared<std::function<void(bool)>>(std::move(completion));
            ++self->tracked_pending;
            try {
                asio::post(executor,[self,packet=std::move(packet),reservation=std::move(reservation),callback]() mutable {
                    if(self->closed){self->finish_tracked(callback,false);return;}
                    try {self->send(self->protocol->outgoing(std::move(packet)),std::move(reservation),callback);}
                    catch(...) {self->finish_tracked(callback,false);self->close();}
                });
            } catch(...) {--self->tracked_pending;return false;}
            return true;
        };
        result.send_batch_tracked=[weak,executor](const std::vector<Packet>& frames,std::function<void(bool)> completion) -> bool {
            if(!completion || frames.empty() || frames.size()>128)return false;
            const auto self=weak.lock();if(!self)return false;
            std::lock_guard lock(self->lifecycle_mutex);if(self->closed || self->output_rejected)return false;
            const auto server=self->owner.lock();if(!server || server->options.maximum_pending_batches<=0 ||
                self->pending_batches.load()>=static_cast<std::size_t>(server->options.maximum_pending_batches))return false;
            std::size_t bytes=0;
            for(const auto& packet:frames) {
                if(packet.size()<2 || packet.size()>maximum_frame_size-2 || packet.size()+2>server->options.maximum_queued_bytes-bytes)return false;
                bytes+=packet.size()+2;
            }
            auto reservation=self->output_budget.reserve(bytes);if(!reservation)return false;
            auto copied=frames;
            auto callback=std::make_shared<std::function<void(bool)>>([self,completion=std::move(completion)](bool ok) mutable {
                struct Release {std::shared_ptr<Connection> self;~Release(){--self->pending_batches;}} release{self};
                completion(ok);
            });
            ++self->pending_batches;++self->tracked_pending;
            try {
                asio::post(executor,[self,copied=std::move(copied),reservation=std::move(reservation),callback,bytes]() mutable {
                    if(self->closed){self->finish_tracked(callback,false);return;}
                    try {
                        Packet encoded;encoded.reserve(bytes);
                        for(auto& packet:copied) {
                            auto framed=frame(self->protocol->outgoing(std::move(packet)));
                            encoded.insert(encoded.end(),framed.begin(),framed.end());
                        }
                        self->send_encoded(std::move(encoded),std::move(reservation),callback);
                    } catch(...) {self->finish_tracked(callback,false);self->close();}
                });
            } catch(...) {--self->tracked_pending;--self->pending_batches;return false;}
            return true;
        };
        result.send=[weak,executor](Packet packet) {
            const auto self=weak.lock();if(!self) return;
            std::lock_guard lock(self->lifecycle_mutex);if(self->closed || self->output_rejected) return;
            auto reservation=packet.size()>=2 && packet.size()<=maximum_frame_size-2
                ? self->output_budget.reserve(packet.size()+2) : nullptr;
            if(!reservation) {
                if(!self->output_rejected.exchange(true)) asio::post(executor,[weak] {if(auto connection=weak.lock()) connection->close();});
                return;
            }
            asio::post(executor,[weak,packet=std::move(packet),reservation=std::move(reservation)]() mutable {
                const auto connection=weak.lock();if(!connection || connection->closed) return;
                try {connection->send(connection->protocol->outgoing(std::move(packet)),std::move(reservation));}
                catch(...) {connection->close();}
            });
        };
        result.retain_io=[weak,executor]() -> std::shared_ptr<void> {
            const auto self=weak.lock();if(!self) return {};
            std::lock_guard lock(self->lifecycle_mutex);if(self->closed) return {};
            return std::make_shared<asio::executor_work_guard<asio::any_io_executor>>(executor);
        };
        result.set_admission_dispatch=[post_open](bool enabled) {
            post_open([enabled](Connection& self) {
                try {self.protocol->set_admission_dispatch(enabled);} catch(...) {self.close();}
            });
        };
        result.enter_world=[post_open] {
            post_open([](Connection& self) {
                try {self.protocol->enter_world();} catch(...) {self.close();}
            });
        };
        result.enter_character_selection=[post_open] {
            post_open([](Connection& self) {
                try {self.protocol->enter_character_selection();} catch(...) {self.close();}
            });
        };
        result.pause_input=[weak,executor](std::function<void(bool)> done) {
            if(!done) throw std::invalid_argument("Input pause requires completion");
            if(const auto self=weak.lock()) {
                std::lock_guard lock(self->lifecycle_mutex);
                if(!self->closed) {
                    asio::post(executor,[weak,done=std::move(done)] {
                        const auto connection=weak.lock();
                        const bool active=connection && !connection->closed;
                        if(active) connection->input_paused=true;
                        try {done(active);} catch(...) {if(active) connection->close();}
                    });
                    return;
                }
            }
            // The socket executor may already have stopped; report failure on
            // the caller's thread instead of leaving the handoff unresolved.
            try {done(false);} catch(...) {}
        };
        result.resume_input=[post_open] {
            post_open([](Connection& self) {
                self.input_paused=false;
                self.process_received();
            });
        };
        result.disconnect=[post_open] {
            post_open([](Connection& self) {self.close();});
        };
        result.disconnect_with_reason=[post_open](std::uint32_t reason) {
            post_open([reason](Connection& self) {self.close(reason);});
        };
        return result;
    }
    void schedule() {
        if(closed || working || !protocol->owner() || (loaded && pending.empty())) return;
        const auto server=owner.lock();if(!server) return close();
        working=true;
        const auto account=*protocol->owner();const bool initial=!loaded;
        PendingPacket request;if(!initial) {request=std::move(pending.front());pending.pop_front();}
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
                } else if(request.nprotect) self->application->receive_nprotect(request.packet);
                else self->application->receive(request.packet);
            } catch(...) {if(initial && !self->application) self->factory_failed=true;self->application_failed();failed=true;}
            asio::post(executor,[weak,initial,failed] {
                const auto connection=weak.lock();if(!connection || connection->closed) return;
                if(failed) return connection->close();
                connection->working=false;if(initial) connection->loaded=true;
                connection->process_received();
            });
        });
    }
    void dispatch_packet(Packet packet,bool retained_world_live=false) {
        const auto server=owner.lock();if(!server) return close();
        auto event=protocol->receive(std::move(packet),server->now_ms(),retained_world_live);
        if(event.close) return close(event.close_reason);
        for(auto& reply:event.replies) send(std::move(reply));
        if(closed) return;
        if(event.application) pending.push_back({std::move(*event.application),false});
        if(event.nprotect) {
            if(!protocol->owner()) throw std::logic_error("Pre-auth NProtect answer has no retained owner");
            pending.push_back({std::move(*event.nprotect),true});
        }
        if(event.connected) reservation=protocol->owner()->world_reservation;
        if(event.connected || event.application || event.nprotect) schedule();
    }
    bool begin_retained_lookup(Packet& packet,bool& known_live) {
        const auto server=owner.lock();if(!server) {close();return true;}
        if(!server->options.retained_world_lookup || protocol->state()!=WorldProtocol::State::connecting ||
           packet.size()<6 || packet[0]!=1 || packet[1]!=0xa3) return false;
        std::uint32_t account=0;for(unsigned i=0;i<4;++i)account|=std::uint32_t(packet[2+i])<<(8*i);
        if(!account) return false;
        const auto current=server->sessions->find(account);
        if(server->sessions->world_reserved(account,server->options.realm) ||
           (current && current->realm==server->options.realm && current->location==session::LoginSession::Location::world)) {
            // Preserve this positive snapshot through dispatch even if a
            // concurrent login replaces the registry record immediately after.
            known_live=true;return false;
        }
        auto request=std::make_shared<PendingLookup>();request->packet=std::move(packet);retained_lookup=request;working=true;
        const auto weak=weak_from_this();const auto executor=socket.get_executor();
        const std::weak_ptr<PendingLookup> pending_request=request;const std::weak_ptr<Impl> weak_server=server;
        const auto before_delivery=server->options.before_retained_lookup_delivery_post;
        const auto before_close=server->options.before_retained_lookup_close_post;
        server->options.retained_world_lookup(account,[weak,executor,pending_request,weak_server,before_delivery,before_close](std::optional<bool> live) {
            const auto request=pending_request.lock();if(!request) return;
            // Serialize posting against close: an accepted completion is queued
            // before the connection drops its executor work guard.
            std::lock_guard lock(request->mutex);
            if(request->cancelled || request->completed) return;
            request->completed=true;
            try {
                if(before_delivery)before_delivery();
                asio::post(executor,[weak,request,live] {
                    const auto connection=weak.lock();if(!connection || connection->closed || connection->retained_lookup!=request) return;
                    connection->retained_lookup.reset();
                    if(!live.has_value()) return connection->close();
                    try {
                        connection->working=false;
                        connection->dispatch_packet(std::move(request->packet),*live);
                        connection->process_received();
                    } catch(...) {connection->close();}
                });
            } catch(...) {
                // Do not replay a stale ownership snapshot; retain the actual
                // connection until a retry or stop can close it on its strand.
                if(const auto server=weak_server.lock()) {
                    request->delivery_failed=true;++server->retained_lookup_delivery_failures;
                    ++server->pending_retained_lookup_delivery_failures;
                    try {
                        if(before_close)before_close();
                        asio::post(executor,[weak]{if(auto connection=weak.lock())connection->close();});
                    } catch(...) { /* Retain for retry or normal close. */ }
                }
            }
        });
        return true;
    }
    // Decrypt subsequent frames only after prior application and lookup calls
    // and their channel posts have run on the socket executor.
    void process_received() {
        try {
            while(!closed && !working && !input_paused && !received.empty()) {
                auto packet=std::move(received.front());received.pop_front();
                received_bytes-=packet.size()+2;
                bool known_live=false;if(begin_retained_lookup(packet,known_live)) return;
                dispatch_packet(std::move(packet),known_live);
            }
        } catch(...) {close();}
    }
    void read() {
        if(closed) return;
        socket.async_read_some(asio::buffer(input),[self=shared_from_this()](const asio::error_code& error,std::size_t count) {
            if(self->closed) return;
            if(error) return self->close();
            try {
                self->decoder.feed_each(std::span(self->input).first(count),[&](Packet packet) {
                    const auto size=packet.size()+2;
                    if(size>maximum_received_bytes-self->received_bytes) {self->close();return false;}
                    self->received_bytes+=size;self->received.push_back(std::move(packet));
                    self->process_received();
                    return !self->closed;
                });
            } catch(const std::exception&) {
                // ps_game.exe SWorker::IORecv 004dc690 closes an invalid
                // frame length with reason 13 (004dc896..004dc8a0).
                return self->close(self->decoder.failed()?std::optional<std::uint32_t>{13}:std::nullopt);
            }
            if(!self->closed) self->read();
        });
    }
};
void WorldServer::Impl::accept() {
    if(!running) return;
    acceptor.async_accept([server=shared_from_this()](const asio::error_code& error,tcp::socket socket) {
        if(!server->running) return;
        if(!error) {
            // A disconnected application still occupies a worker and may own
            // world state until close returns. Bound that backlog as well as
            // the sockets that remain in the connection map.
            if(server->outstanding_closes<server->options.maximum_connections && server->next_id<std::numeric_limits<std::uint64_t>::max()) {
                const auto id=server->next_id++;auto connection=std::make_shared<Connection>(std::move(socket),server,id);
                server->connections.emplace(id,connection);
                try {server->close_receipts.emplace(id,connection);}
                catch(...) {server->connections.erase(id);++server->application_failures;server->accept();return;}
                ++server->outstanding_closes;connection->start();
            } else {asio::error_code ignored;socket.close(ignored);}
        }
        server->accept();
    });
}
void WorldServer::Impl::scan_replies() {
    if(!running) return;
    try {
        const auto now=now_ms();
        if(reply_schedule.due(now)) {
            for(auto it=connections.begin();it!=connections.end();) {
                auto connection=(it++)->second;
                if(connection->protocol && connection->protocol->reply_timed_out(now)) connection->close(8);
            }
        }
        reply_timer.expires_after(options.reply_scan_interval);
        reply_timer.async_wait([server=shared_from_this()](const asio::error_code& error) {
            if(!error) server->scan_replies();
        });
    } catch(...) {stop();}
}
void WorldServer::Impl::stop() {
    stopped=true;running=false;asio::error_code ignored;acceptor.cancel(ignored);acceptor.close(ignored);reply_timer.cancel(ignored);
    while(!connections.empty()) {auto connection=connections.begin()->second;connection->close();}
    notify_stopped();
}
void WorldServer::Impl::complete_close(std::uint64_t id) {
    const auto found=close_receipts.find(id);
    if(found==close_receipts.end())throw std::logic_error("World close receipt is missing");
    found->second->close_worker_done=true;
    if(found->second->tracked_pending.load()!=0)return;
    close_receipts.erase(found);--outstanding_closes;notify_stopped();
}
void WorldServer::Impl::notify_stopped() {
    if(!stopped || outstanding_closes) return;
    auto completions=std::move(stop_completions);stop_completions_tail=nullptr;
    while(completions) {
        auto completion=std::move(completions);completions=std::move(completion->next);
        try {completion->callback(!close_failed.load());} catch(...) {}
    }
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
    server->started=true;server->running=true;server->accept();server->scan_replies();
});}
void WorldServer::stop() {asio::dispatch(impl_->strand,[server=impl_] {server->stop();});}
void WorldServer::stop(std::function<void(bool)> completion) {
    if(!completion) throw std::invalid_argument("World stop requires completion");
    // Allocate before submitting to the socket strand: failure is visible to
    // the caller and cannot silently lose an already accepted stop callback.
    auto node=std::make_unique<Impl::StopCompletion>(std::move(completion));
    asio::dispatch(impl_->strand,[server=impl_,node=std::move(node)]() mutable {
        auto* tail=node.get();
        if(server->stop_completions_tail)server->stop_completions_tail->next=std::move(node);
        else server->stop_completions=std::move(node);
        server->stop_completions_tail=tail;server->stop();
    });
}
std::uint16_t WorldServer::port() const {return impl_->bound_port;}
std::shared_ptr<const void> WorldServer::owner_identity() const {return impl_->options.owner_identity;}
std::shared_ptr<session::Registry> WorldServer::network_registry() const {return impl_->sessions;}
std::uint8_t WorldServer::realm() const {return impl_->options.realm;}
std::uint64_t WorldServer::application_failures() const noexcept {return impl_->application_failures.load();}
std::uint64_t WorldServer::retained_lookup_delivery_failures() const noexcept {return impl_->retained_lookup_delivery_failures.load();}
std::size_t WorldServer::pending_retained_lookup_delivery_failures() const noexcept {return impl_->pending_retained_lookup_delivery_failures.load();}
void WorldServer::retry_retained_lookup_delivery_failures() {
    asio::dispatch(impl_->strand,[server=impl_] {
        for(auto it=server->connections.begin();it!=server->connections.end();) {
            auto connection=(it++)->second;auto request=connection->retained_lookup;bool failed=false;
            if(request) {std::lock_guard lock(request->mutex);failed=request->delivery_failed;}
            if(failed)connection->close();
        }
    });
}
std::uint64_t WorldServer::close_submission_failures() const noexcept {return impl_->close_submission_failures.load();}
std::size_t WorldServer::pending_close_submissions() const noexcept {return impl_->pending_close_submissions.load();}
void WorldServer::retry_close_submissions() {
    asio::dispatch(impl_->strand,[server=impl_] {
        for(auto it=server->close_receipts.begin();it!=server->close_receipts.end();) {
            auto connection=(it++)->second;
            if(connection->close_submission_failed)connection->submit_close(server);
        }
    });
}
std::uint64_t WorldServer::close_notification_failures() const noexcept {return impl_->close_notification_failures.load();}
std::size_t WorldServer::pending_close_notifications() const noexcept {return impl_->pending_close_notifications.load();}
void WorldServer::retry_close_notifications() {
    asio::dispatch(impl_->strand,[server=impl_] {
        for(auto it=server->close_receipts.begin();it!=server->close_receipts.end();) {
            auto connection=(it++)->second;bool failed=false;
            {
                std::lock_guard lock(connection->close_notification_mutex);
                if(connection->close_notification_failed) {
                    connection->close_notification_failed=false;--server->pending_close_notifications;failed=true;
                }
            }
            if(failed) {connection->lifetime.reset();server->complete_close(connection->id);}
        }
    });
}
}
