#include "eternity/net/login_crypto.hpp"
#include <algorithm>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rsa.h>

namespace eternity::net {
namespace {
template<typename T, void (*Destroy)(T*)>
using Owned = std::unique_ptr<T, decltype(Destroy)>;
using Number = Owned<BIGNUM, BN_clear_free>;
using Context = Owned<BN_CTX, BN_CTX_free>;

void require(bool success, const char* operation) {
    if (!success) throw std::runtime_error(operation);
}

Number parameter(EVP_PKEY* key, const char* name) {
    BIGNUM* raw = nullptr;
    require(EVP_PKEY_get_bn_param(key, name, &raw) == 1, "Read RSA parameter failed");
    return Number(raw, BN_clear_free);
}
}

struct LoginKeyPair::Impl {
    Owned<EVP_PKEY, EVP_PKEY_free> key{nullptr, EVP_PKEY_free};
    std::array<std::uint8_t, 128> modulus{};
    std::array<std::uint8_t, 64> exponent{};
    std::uint8_t exponent_length = 0;
};

LoginKeyPair::LoginKeyPair() : impl_(std::make_unique<Impl>()) {
    Owned<EVP_PKEY_CTX, EVP_PKEY_CTX_free> context(EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr), EVP_PKEY_CTX_free);
    require(bool(context), "Allocate RSA key generator failed");
    require(EVP_PKEY_keygen_init(context.get()) == 1, "Initialize RSA key generator failed");
    require(EVP_PKEY_CTX_set_rsa_keygen_bits(context.get(), 1024) == 1, "Set original RSA key size failed");
    EVP_PKEY* raw = nullptr;
    require(EVP_PKEY_keygen(context.get(), &raw) == 1, "Generate RSA key failed");
    impl_->key.reset(raw);
    const auto modulus = parameter(raw, OSSL_PKEY_PARAM_RSA_N);
    const auto exponent = parameter(raw, OSSL_PKEY_PARAM_RSA_E);
    impl_->exponent_length = static_cast<std::uint8_t>(((BN_num_bits(exponent.get()) + 31) / 32) * 4);
    require(impl_->exponent_length <= impl_->exponent.size(), "Original RSA exponent field overflow");
    require(BN_bn2lebinpad(modulus.get(), impl_->modulus.data(), 128) == 128, "Serialize RSA modulus failed");
    require(BN_bn2lebinpad(exponent.get(), impl_->exponent.data(), 64) == 64, "Serialize RSA exponent failed");
}

LoginKeyPair::~LoginKeyPair() = default;

Packet LoginKeyPair::public_packet() const {
    // CUserCrypto::KeyPublicSend, ps_login.exe 00404de0, payload size 0xc5.
    Packet packet(197, 0);
    packet[0] = 0x01;
    packet[1] = 0xa1;
    packet[2] = 0;
    packet[3] = impl_->exponent_length;
    packet[4] = 128;
    std::copy(impl_->exponent.begin(), impl_->exponent.end(), packet.begin() + 5);
    std::copy(impl_->modulus.begin(), impl_->modulus.end(), packet.begin() + 69);
    return packet;
}

SessionSecrets LoginKeyPair::accept(std::span<const std::uint8_t> ciphertext) const {
    // CUserCrypto::KeyInit, ps_login.exe 00404ea0. No PKCS#1 padding is present.
    if (ciphertext.empty() || ciphertext.size() > 128)
        throw std::invalid_argument("Original RSA exchange length must be in 1..128");
    Number cipher(BN_lebin2bn(ciphertext.data(), static_cast<int>(ciphertext.size()), nullptr), BN_clear_free);
    Number message(BN_new(), BN_clear_free);
    const auto modulus = parameter(impl_->key.get(), OSSL_PKEY_PARAM_RSA_N);
    auto private_exponent = parameter(impl_->key.get(), OSSL_PKEY_PARAM_RSA_D);
    BN_set_flags(private_exponent.get(), BN_FLG_CONSTTIME);
    Context context(BN_CTX_new(), BN_CTX_free);
    require(bool(cipher) && bool(message) && bool(context), "Allocate RSA exchange state failed");
    require(BN_mod_exp_mont_consttime(message.get(), cipher.get(), private_exponent.get(),
            modulus.get(), context.get(), nullptr) == 1, "RSA exchange failed");
    std::array<std::uint8_t, 128> plaintext{};
    require(BN_bn2lebinpad(message.get(), plaintext.data(), 128) == 128, "Serialize RSA secret failed");
    std::array<std::uint8_t, 32> hash{};
    unsigned int length = 0;
    const auto* result = HMAC(EVP_sha256(), plaintext.data(), static_cast<int>(plaintext.size()),
                             impl_->modulus.data(), impl_->modulus.size(), hash.data(), &length);
    OPENSSL_cleanse(plaintext.data(), plaintext.size());
    require(result != nullptr && length == hash.size(), "Derive login cipher state failed");
    SessionSecrets secrets{};
    std::copy_n(hash.begin(), 16, secrets.key.begin());
    std::copy_n(hash.begin() + 16, 16, secrets.iv.begin());
    OPENSSL_cleanse(hash.data(), hash.size());
    return secrets;
}

struct LegacyCipher::Impl {
    Owned<EVP_CIPHER_CTX, EVP_CIPHER_CTX_free> context{EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free};
    std::array<std::uint8_t, 16> counter{};
    std::array<std::uint8_t, 16> stream{};
    std::size_t position = 0;
    ~Impl() { OPENSSL_cleanse(stream.data(), stream.size()); }
};

LegacyCipher::LegacyCipher(const SessionSecrets& secrets) : impl_(std::make_unique<Impl>()) {
    require(bool(impl_->context), "Allocate login cipher failed");
    require(EVP_EncryptInit_ex(impl_->context.get(), EVP_aes_128_ecb(), nullptr, secrets.key.data(), nullptr) == 1,
            "Initialize login cipher failed");
    require(EVP_CIPHER_CTX_set_padding(impl_->context.get(), 0) == 1, "Configure login cipher failed");
    impl_->counter = secrets.iv;
}

LegacyCipher::~LegacyCipher() = default;

void LegacyCipher::crypt(std::span<std::uint8_t> bytes) {
    for (auto& byte : bytes) {
        if (impl_->position == 0) {
            int written = 0;
            require(EVP_EncryptUpdate(impl_->context.get(), impl_->stream.data(), &written,
                    impl_->counter.data(), 16) == 1 && written == 16, "Generate login cipher block failed");
            // SAes::CtrInc 0040db10: least-significant limb is first, little-endian.
            for (auto& counter_byte : impl_->counter) {
                counter_byte = static_cast<std::uint8_t>(counter_byte + 1);
                if (counter_byte != 0) break;
            }
        }
        byte ^= impl_->stream[impl_->position];
        impl_->position = (impl_->position + 1) % 16;
    }
}
}
