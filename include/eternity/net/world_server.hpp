#pragma once
#include <array>
#include "eternity/net/world_protocol.hpp"
#include "eternity/game/characters.hpp"
#include "eternity/game/character_commands.hpp"
#include <asio/io_context.hpp>
#include <chrono>

namespace eternity::net {
using RetainedWorldLookup = std::function<void(std::uint32_t,std::function<void(std::optional<bool>)>)>;

struct WorldServerOptions {
    std::string address = "127.0.0.1";
    std::uint16_t port = 30810;
    std::uint8_t realm = 0;
    // Includes connections whose sockets closed but whose application close
    // has not returned; released after close completion reaches the IO strand.
    std::size_t maximum_connections = 128;
    std::size_t database_workers = 2;
    // ps_game.exe CServer::Start 00405a30: accepted socket sender threshold.
    std::size_t maximum_queued_bytes = 0xc0000;
    // Signed worker limit; the default is a native bound pending live config.
    // Nonpositive limits reject every new batch, as in the recovered gate.
    std::int32_t maximum_pending_batches = 128;
    // Defaults follow SReply's 300000 ms update cadence and monotonic clock.
    // Overrides support deterministic tests; use the same clock as the login registry.
    std::chrono::milliseconds reply_scan_interval{300000};
    std::function<std::uint64_t()> monotonic_ms = {};
    // Optional failure injection before a real application-close submission
    // or its later socket-strand notification.
    std::function<void()> before_close_submission_post = {};
    std::function<void()> before_close_notification_post = {};
    // Optional retained-world index for A301. The provider must return promptly
    // and invoke completion once, from any thread: true means the account is
    // still retained, false means absent, nullopt means lookup failed. A failed
    // lookup closes the connection. Bind this to the same-realm serialized
    // world owner; it supplements the session-registry projection. The result
    // is a snapshot, not an account reservation. Without this port, retained
    // owners hidden by a replacement login cannot be detected.
    RetainedWorldLookup retained_world_lookup = {};
    // Optional failure injection before posting a real retained-world result.
    std::function<void()> before_retained_lookup_delivery_post = {};
    // Optional failure injection before the fallback real socket-close post.
    std::function<void()> before_retained_lookup_close_post = {};
    // Trusted composition metadata: obtain from the same bound owner used by
    // the application factory, on that owner's strand. Not factory verification.
    std::shared_ptr<const void> owner_identity = {};
};
using LoadCharacters = std::function<game::CharacterRoster(const session::LoginSession&)>;
using DispatchCharacter = std::function<game::CharacterReply(const session::LoginSession&,game::CharacterRoster&,const Packet&)>;

// Identifies one accepted connection within its server. The generation is a
// weak reference to that actual connection; compare ownership as well as value
// across listeners. active() is a thread-safe snapshot, not a send guarantee.
// This value is not a native socket descriptor and must not be passed as one.
struct WorldConnectionIdentity {
    std::uint64_t value;
    std::weak_ptr<const void> generation;
    std::function<bool()> active;
};

// Independently sampled native reservations and bytes, including posts,
// queued frames and writes. A batch uses one reservation for multiple frames.
// These counters do not represent legacy priority batches;
// the two counters may change between loads and are not a completion fence.
struct WorldPendingSendState {
    std::size_t frames;
    std::size_t bytes;
    std::size_t byte_limit;
    std::size_t batches;
    std::int32_t batch_limit;
};

// Copyable, thread-safe transport ports. Asynchronous calls are ordered on the
// socket executor; ports retained after disconnect become inert. Payloads are plaintext.
// A previously acquired IO lease remains active until its last owner releases it.
struct WorldChannel {
    // Empty after close; the returned weak generation never keeps IO alive.
    std::function<std::optional<WorldConnectionIdentity>()> connection_identity;
    // Empty after close. Each value is a transient snapshot, not admission.
    std::function<std::optional<WorldPendingSendState>()> pending_send_state;
    // Returns true only when ownership was posted. On true, completion runs
    // exactly once on the socket strand after the real write or cancellation;
    // false means no callback. Callback must not block and may reenter ports.
    // Success means the complete frame was written to the local socket, not
    // that the remote application received it. Stop waits for callback return.
    std::function<bool(Packet,std::function<void(bool)>)> send_tracked;
    // All frames are copied and admitted atomically against the batch and byte
    // bounds. False leaves the caller's frames untouched and calls no callback;
    // allocation exceptions may propagate before admission. At most 128 frames.
    // Accepted completion follows the same local-write/close contract above.
    std::function<bool(const std::vector<Packet>&,std::function<void(bool)>)> send_batch_tracked;
    std::function<void(Packet)> send;
    // Acquire inside an application callback, before starting asynchronous work whose completion
    // must run on the socket IO context after close. A separate world IO context
    // needs its own guard. Retain until the real drain
    // completes; an empty result means the connection is already closed. The
    // lease keeps IO alive, not the connection, application, or DB executor.
    std::function<std::shared_ptr<void>()> retain_io;
    // Posted after location 3 is published, before input resumes; retain through location 4.
    // Disable on return to selection. This does not change outgoing encryption;
    // the application remains responsible for location-specific packet dispatch.
    std::function<void(bool)> set_admission_dispatch;
    std::function<void()> enter_world;
    std::function<void()> enter_character_selection;
    // If the connection is still active, completion runs on the socket
    // executor after the pause is applied (or with false if close wins). If
    // already closed, completion runs immediately on the caller's thread with
    // false, even if the socket executor has stopped. The
    // world owner must await true before transferring the user to zone-wait;
    // false means that the connection was already closed. Completion must not
    // block and must hand work back to the serialized world executor.
    std::function<void(std::function<void(bool)>)> pause_input;
    std::function<void()> resume_input;
    std::function<void()> disconnect;
    // Preserve a known original service close reason (for example NProtect 111).
    std::function<void(std::uint32_t)> disconnect_with_reason;
    // Immutable IPv4 peer captured on the socket strand at accept. Missing
    // means lookup failed; consumers must not substitute an address.
    std::optional<std::array<std::uint8_t,4>> peer_ipv4;
};

// Factory, open, receive and close run on one per-connection worker strand.
// Every returned application is closed once, after its last in-flight call,
// including when open/receive throws. Close returns after starting cleanup;
// asynchronous cleanup must retain an IO lease through its own drain barrier.
class WorldApplication {
public:
    virtual ~WorldApplication() = default;
    virtual void open() = 0;
    virtual void receive(const Packet& packet) = 0;
    // Must bind the same retained NProtect state/backend used by zone lifecycle.
    // No default success: an unbound NProtect response fails the application.
    virtual void receive_nprotect(const Packet&) {throw std::logic_error("NProtect answer service is not bound");}
    virtual void close() = 0;
    // Known original close reason, or nullopt for an unmapped transport failure.
    virtual void close_with_reason(std::optional<std::uint32_t>) {close();}
};
using WorldApplicationFactory = std::function<std::unique_ptr<WorldApplication>(const session::LoginSession&,WorldChannel)>;

// Native game TCP endpoint. Stop and drain the io_context before destruction.
class WorldServer {
public:
    WorldServer(asio::io_context& io,WorldServerOptions options,std::shared_ptr<session::Registry> registry,
        WorldApplicationFactory factory);
    WorldServer(asio::io_context& io,WorldServerOptions options,std::shared_ptr<session::Registry> registry,
        LoadCharacters load,DispatchCharacter dispatch = {});
    ~WorldServer();
    WorldServer(const WorldServer&) = delete;
    WorldServer& operator=(const WorldServer&) = delete;
    void start();
    void stop();
    // Stops admission and closes every connection. Completion runs on the
    // socket strand after all application close calls have returned, including
    // connections already closing. It does not wait for asynchronous DB work
    // started by close; the world owner must drain that work separately. Its
    // argument is false if any application close threw, including a close
    // before stop was requested; the owner must handle that cleanup failure.
    // Keep the io_context and world executor running through completion.
    void stop(std::function<void(bool)> completion);
    std::uint16_t port() const;
    std::shared_ptr<const void> owner_identity() const;
    std::shared_ptr<session::Registry> network_registry() const;
    std::uint8_t realm() const;
    std::uint64_t application_failures() const noexcept;
    // A failed worker submission retains the real owner and reservation until
    // the actual close task is successfully submitted by retry.
    std::uint64_t close_submission_failures() const noexcept;
    std::size_t pending_close_submissions() const noexcept;
    void retry_close_submissions();
    // A failed worker-to-socket notification remains pending; retry only
    // acknowledges application-close work that has actually returned.
    std::uint64_t close_notification_failures() const noexcept;
    std::size_t pending_close_notifications() const noexcept;
    void retry_close_notifications();
    // A failed retained-world result post is closed on retry, forcing a fresh
    // lookup on reconnect instead of applying a delayed ownership snapshot.
    std::uint64_t retained_lookup_delivery_failures() const noexcept;
    std::size_t pending_retained_lookup_delivery_failures() const noexcept;
    void retry_retained_lookup_delivery_failures();
private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};
}
