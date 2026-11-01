#pragma once
#include "eternity/net/login_crypto.hpp"
#include <algorithm>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>

// Test-only protocol peer: derives state from the public wire advertisement.
struct LoginPeer {
    eternity::net::Packet exchange;
    eternity::net::SessionSecrets secrets{};
    std::unique_ptr<eternity::net::LegacyCipher> send;
    std::unique_ptr<eternity::net::LegacyCipher> receive;

    explicit LoginPeer(const eternity::net::Packet& advertisement) {
        using namespace eternity::net;
        if (advertisement.size() != 197 || advertisement[0] != 1 || advertisement[1] != 0xa1 ||
            advertisement[3] == 0 || advertisement[3] > 64 || advertisement[4] != 128)
            throw std::runtime_error("Invalid login key advertisement");
        std::unique_ptr<BIGNUM, decltype(&BN_free)> modulus(BN_lebin2bn(advertisement.data() + 69, 128, nullptr), BN_free);
        std::unique_ptr<BIGNUM, decltype(&BN_free)> exponent(BN_lebin2bn(advertisement.data() + 5, advertisement[3], nullptr), BN_free);
        std::array<unsigned char, 128> plaintext{};
        for (std::size_t index = 0; index < 64; ++index) plaintext[index] = static_cast<unsigned char>(index + 1);
        std::unique_ptr<BIGNUM, decltype(&BN_free)> message(BN_lebin2bn(plaintext.data(), 128, nullptr), BN_free);
        std::unique_ptr<BIGNUM, decltype(&BN_free)> ciphertext(BN_new(), BN_free);
        std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)> context(BN_CTX_new(), BN_CTX_free);
        if (!modulus || !exponent || !message || !ciphertext || !context ||
            BN_mod_exp(ciphertext.get(), message.get(), exponent.get(), modulus.get(), context.get()) != 1)
            throw std::runtime_error("Test peer RSA operation failed");
        exchange = {0x01, 0xa1, 128};
        exchange.resize(131);
        if (BN_bn2lebinpad(ciphertext.get(), exchange.data() + 3, 128) != 128)
            throw std::runtime_error("Test peer RSA serialization failed");
        std::array<unsigned char, 32> digest{};
        unsigned int length = 0;
        if (!HMAC(EVP_sha256(), plaintext.data(), 128, advertisement.data() + 69, 128, digest.data(), &length) || length != 32)
            throw std::runtime_error("Test peer HMAC failed");
        std::copy_n(digest.begin(), 16, secrets.key.begin());
        std::copy_n(digest.begin() + 16, 16, secrets.iv.begin());
        send = std::make_unique<LegacyCipher>(secrets);
        receive = std::make_unique<LegacyCipher>(secrets);
    }

    eternity::net::Packet credentials(std::string username, std::string password) {
        if (username.size() > 32 || password.size() > 13) throw std::runtime_error("Oversize test credentials");
        eternity::net::Packet packet(47, 0);
        packet[0] = 0x02;
        packet[1] = 0xa1;
        std::copy(username.begin(), username.end(), packet.begin() + 2);
        std::copy(password.begin(), password.end(), packet.begin() + 34);
        send->crypt(packet);
        return packet;
    }
};
