#pragma once
#include "eternity/auth/login_protocol.hpp"
#include <asio/io_context.hpp>
#include <functional>
#include <exception>
#include <chrono>

namespace eternity::session { class Registry; }

namespace eternity::net {
struct LoginServerOptions {
    std::string address = "127.0.0.1";
    std::uint16_t port = 30800;
    // Also bounds authentication jobs queued or executing after socket close.
    std::size_t maximum_connections = 128;
    std::size_t database_workers = 2;
    // Injectable monotonic tick and worker cadence for deterministic transport tests.
    std::function<std::uint64_t()> clock;
    std::chrono::milliseconds reply_scan_period{300000};
    // Optional failure injection before the real drain timer is armed.
    // Intended for deterministic tests; an exception leaves drain retryable.
    std::function<void()> before_authentication_drain_wait = {};
    // Optional failure injection before result or failure-close delivery.
    // The argument is true for the failure-close attempt; exceptions fail closed.
    std::function<void(bool)> before_authentication_delivery_post = {};
    std::vector<auth::Realm> realms = {{0,2,0,1000,"Eternity"}};
};

using Authenticate = std::function<auth::LoginResult(const auth::Credentials&, std::uint64_t, const std::string&)>;

// Socket state and output queues are serialized on one Asio strand; SQL runs off it.
class LoginServer {
public:
    LoginServer(asio::io_context& io, LoginServerOptions options, Authenticate authenticate,
                std::shared_ptr<session::Registry> sessions = {}, std::shared_ptr<auth::RealmDirectory> realms = {});
    ~LoginServer();
    LoginServer(const LoginServer&) = delete;
    LoginServer& operator=(const LoginServer&) = delete;
    void start();
    void stop();
    // Stop accepting and close sockets, then notify on the socket strand after
    // all accepted authentication backend calls have returned or been skipped
    // and failed replacement and expired-handoff requests have been retried.
    // A pending completion keeps the io_context alive; explicit io_context::stop
    // still interrupts it. This does not cancel SQL,
    // imply backend success, or drain work owned by other services.
    void stop(std::function<void()> completion);
    // A timer setup/completion error leaves stop callbacks pending. Inspect
    // from any thread and retry while the IO context is still running.
    std::exception_ptr authentication_drain_error() const;
    void retry_authentication_drain();
    std::uint16_t port() const;
    std::size_t session_count() const;
    // Queued or executing authentication work, including disconnected peers.
    std::size_t outstanding_authentication() const noexcept;
    // Failed DB-result posts are never treated as authentication success.
    // A failed close post remains pending until the real socket closes or retry.
    std::uint64_t authentication_delivery_failures() const noexcept;
    std::size_t pending_authentication_delivery_failures() const noexcept;
    void retry_authentication_delivery_failures();
    // Failed expired-handoff disconnect callbacks are retained for retry;
    // an erased handoff is not restored or assumed disconnected.
    std::uint64_t handoff_disconnect_failures() const noexcept;
    std::size_t pending_handoff_disconnects() const noexcept;
    void retry_handoff_disconnects();
    // Failed expiration sweeps retain every handoff for a later scan.
    std::uint64_t handoff_expiration_failures() const noexcept;
    // Retained failed replacement requests require actual recovery. A
    // successful retry only means the old owner's callback returned.
    std::size_t pending_replacement_disconnects() const noexcept;
    std::uint64_t replacement_disconnect_failures() const noexcept;
    void retry_replacement_disconnects();
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
}
