#pragma once
#include "eternity/net/framing.hpp"
#include <array>
#include <memory>

namespace eternity::net {
struct SessionSecrets {
    std::array<std::uint8_t, 16> key;
    std::array<std::uint8_t, 16> iv;
    bool operator==(const SessionSecrets&) const = default;
};

// Original login transport uses raw little-endian RSA, HMAC-SHA256 and AES-128.
class LoginKeyPair {
public:
    LoginKeyPair();
    ~LoginKeyPair();
    LoginKeyPair(const LoginKeyPair&) = delete;
    LoginKeyPair& operator=(const LoginKeyPair&) = delete;
    Packet public_packet() const;
    SessionSecrets accept(std::span<const std::uint8_t> ciphertext) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// AES-CTR with a little-endian 128-bit counter, kept across packet boundaries.
class LegacyCipher {
public:
    explicit LegacyCipher(const SessionSecrets& secrets);
    ~LegacyCipher();
    LegacyCipher(const LegacyCipher&) = delete;
    LegacyCipher& operator=(const LegacyCipher&) = delete;
    void crypt(std::span<std::uint8_t> bytes);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
