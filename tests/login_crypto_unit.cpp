#include "eternity/net/login_crypto.hpp"
#include <algorithm>
#include <iostream>
#include <openssl/bn.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

int main() {
    using namespace eternity::net;
    try {
        // AES-128 ECB(0) and ECB(01 00 ... 00): independent fixed wire vector.
        const Packet expected = {0x66,0xe9,0x4b,0xd4,0xef,0x8a,0x2c,0x3b,0x88,0x4c,0xfa,0x59,0xca,0x34,0x2b,0x2e,
                                 0x47,0x71,0x18,0x16,0xe9,0x1d,0x6f,0xf0,0x59,0xbb,0xbf,0x2b,0xf5,0x8e,0x0f,0xd3};
        LegacyCipher cipher(SessionSecrets{});
        Packet encrypted(32, 0);
        cipher.crypt(encrypted);
        require(encrypted == expected, "Original little-endian AES-CTR vector changed");
        LegacyCipher fragmented(SessionSecrets{});
        Packet chunks(32, 0);
        fragmented.crypt(std::span(chunks).first(1));
        fragmented.crypt(std::span(chunks).subspan(1, 16));
        fragmented.crypt(std::span(chunks).subspan(17));
        require(chunks == expected, "Cipher counter did not survive packet boundaries");
        LegacyCipher receiver(SessionSecrets{});
        receiver.crypt(encrypted);
        require(encrypted == Packet(32, 0), "Independent receive cipher did not decrypt");

        LoginKeyPair server;
        const auto packet = server.public_packet();
        require(packet.size() == 197 && packet[0] == 1 && packet[1] == 0xa1 && packet[2] == 0,
                "Original key advertisement layout changed");
        require(packet[3] == 4 && packet[4] == 128, "Original GMP limb lengths changed");
        // Model a client's raw RSA operation using only the advertised public fields.
        std::unique_ptr<BIGNUM, decltype(&BN_free)> n(BN_lebin2bn(packet.data()+69, packet[4], nullptr), BN_free);
        std::unique_ptr<BIGNUM, decltype(&BN_free)> e(BN_lebin2bn(packet.data()+5, packet[3], nullptr), BN_free);
        std::array<unsigned char,128> plaintext{};
        for (std::size_t index=0; index<64; ++index) plaintext[index] = static_cast<unsigned char>(index+1);
        std::unique_ptr<BIGNUM, decltype(&BN_free)> m(BN_lebin2bn(plaintext.data(), 128, nullptr), BN_free);
        std::unique_ptr<BIGNUM, decltype(&BN_free)> c(BN_new(), BN_free);
        std::unique_ptr<BN_CTX, decltype(&BN_CTX_free)> context(BN_CTX_new(), BN_CTX_free);
        require(n && e && m && c && context, "Test public RSA state allocation failed");
        require(BN_mod_exp(c.get(), m.get(), e.get(), n.get(), context.get()) == 1, "Test public RSA failed");
        Packet ciphertext(128);
        require(BN_bn2lebinpad(c.get(), ciphertext.data(), 128) == 128, "Test RSA serialization failed");
        const auto derived = server.accept(ciphertext);
        std::array<unsigned char,32> expected_hash{};
        unsigned int length=0;
        require(HMAC(EVP_sha256(), plaintext.data(), 128, packet.data()+69, 128,
                     expected_hash.data(), &length) != nullptr && length == 32, "Test HMAC failed");
        require(std::equal(derived.key.begin(), derived.key.end(), expected_hash.begin()) &&
                std::equal(derived.iv.begin(), derived.iv.end(), expected_hash.begin()+16),
                "RSA exchange did not recover the client's expected key and IV");
        bool rejected = false;
        try { (void)server.accept(Packet(129)); } catch (const std::invalid_argument&) { rejected = true; }
        require(rejected, "Oversize RSA exchange accepted");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
