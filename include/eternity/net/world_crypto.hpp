#pragma once
#include "eternity/net/login_crypto.hpp"

namespace eternity::net {
class WorldXor {
public:
    // CryptoInitKey/CryptoInitKey2, ps_game.exe 00457eb0/00457f00.
    explicit WorldXor(std::array<std::uint8_t,16> seed);
    const std::array<std::uint8_t,16>& seed() const noexcept { return seed_; }
    void crypt(std::span<std::uint8_t> payload) const;
private:
    std::array<std::uint8_t,16> seed_;
    std::array<std::uint8_t,4096> table_{};
};

class WorldCipher {
public:
    WorldCipher(const std::array<std::uint8_t,32>& game_key, std::shared_ptr<const WorldXor> world_xor);
    void decrypt(std::span<std::uint8_t> payload);
    void encrypt(std::span<std::uint8_t> payload);
    void enter_world() noexcept { world_ = true; }
    void enter_character_selection() noexcept { world_ = false; }
private:
    LegacyCipher receiver_;
    LegacyCipher sender_;
    std::shared_ptr<const WorldXor> world_xor_;
    bool world_ = false;
};
}
